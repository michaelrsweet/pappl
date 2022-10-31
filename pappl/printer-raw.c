//
// Raw printing support for the Printer Application Framework
//
// Copyright © 2019-2021 by Michael R Sweet.
// Copyright © 2010-2019 by Apple Inc.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

//
// Include necessary headers...
//

#include "pappl-private.h"


//
// '_papplPrinterAddRawListeners()' - Create listener sockets for raw print queues.
//

bool					// O - `true` on success, `false` otherwise
_papplPrinterAddRawListeners(
    pappl_printer_t *printer)		// I - Printer
{
  int			sock,		// Listener socket
			port;		// Listener port
  http_addrlist_t	*addrlist;	// Listen addresses
  char			service[255];	// Service port


  // Listen on port 9100, 9101, etc.
  port = 9099 + printer->printer_id;
  snprintf(service, sizeof(service), "%d", port);

  if ((addrlist = httpAddrGetList(NULL, AF_INET, service)) != NULL)
  {
    if ((sock = httpAddrListen(&(addrlist->addr), port)) < 0)
    {
      papplLogPrinter(printer, PAPPL_LOGLEVEL_ERROR, "Unable to create socket print listener for '*:%d': %s", port, cupsLastErrorString());
    }
    else
    {
      printer->raw_listeners[printer->num_raw_listeners].fd     = sock;
      printer->raw_listeners[printer->num_raw_listeners].events = POLLIN | POLLERR;
      printer->num_raw_listeners ++;
    }

    httpAddrFreeList(addrlist);
  }

  if ((addrlist = httpAddrGetList(NULL, AF_INET6, service)) != NULL)
  {
    if ((sock = httpAddrListen(&(addrlist->addr), port)) < 0)
    {
      papplLogPrinter(printer, PAPPL_LOGLEVEL_ERROR, "Unable to create socket print listener for '*:%d': %s", port, cupsLastErrorString());
    }
    else
    {
      printer->raw_listeners[printer->num_raw_listeners].fd     = sock;
      printer->raw_listeners[printer->num_raw_listeners].events = POLLIN | POLLERR;
      printer->num_raw_listeners ++;
    }

    httpAddrFreeList(addrlist);
  }

  if (printer->num_raw_listeners > 0)
    papplLogPrinter(printer, PAPPL_LOGLEVEL_INFO, "Listening for socket print jobs on '*:%d'.", port);

  return (printer->num_raw_listeners > 0);
}


//
// '_papplPrinterRunRaw()' - Accept raw print requests over sockets.
//

void *					// O - Thread exit value
_papplPrinterRunRaw(
    pappl_printer_t *printer)		// I - Printer
{
  int	i;				// Looping var


  papplLogPrinter(printer, PAPPL_LOGLEVEL_DEBUG, "Running socket print thread with %d listeners.", printer->num_raw_listeners);

  _papplRWLockWrite(printer);
  printer->raw_active = true;
  _papplRWUnlock(printer);

  while (!papplPrinterIsDeleted(printer) && papplSystemIsRunning(printer->system))
  {
    // Don't accept connections if we can't accept a new job...
    _papplRWLockRead(printer);
    while ((int)cupsArrayGetCount(printer->active_jobs) >= printer->max_active_jobs && !printer->is_deleted && papplSystemIsRunning(printer->system))
    {
      _papplRWUnlock(printer);
      usleep(100000);
      _papplRWLockRead(printer);
    }
    _papplRWUnlock(printer);

    if (papplPrinterIsDeleted(printer) || !papplSystemIsRunning(printer->system))
      break;

    // Wait 1 second for new connections...
    if ((i = poll(printer->raw_listeners, (nfds_t)printer->num_raw_listeners, 1000)) > 0)
    {
      if (papplPrinterIsDeleted(printer) || !papplSystemIsRunning(printer->system))
	break;

      // Got a new connection request, accept from the corresponding listener...
      for (i = 0; i < printer->num_raw_listeners; i ++)
      {
        if (printer->raw_listeners[i].revents & POLLIN)
        {
          time_t	activity;	// Network activity watchdog
          int		sock;		// Client socket
          http_addr_t	sockaddr;	// Client address
          socklen_t	sockaddrlen;	// Length of client address
          struct pollfd	sockp;		// poll() data for client socket
          pappl_job_t	*job;		// New print job
          ssize_t	bytes;		// Bytes read from socket
          char		buffer[8192];	// Copy buffer
          char		filename[1024];	// Job filename

          // Accept the connection...
          sockaddrlen = sizeof(sockaddr);
          if ((sock = (int)accept(printer->raw_listeners[i].fd, (struct sockaddr *)&sockaddr, &sockaddrlen)) < 0)
          {
            papplLogPrinter(printer, PAPPL_LOGLEVEL_ERROR, "Unable to accept socket print connection: %s", strerror(errno));
            continue;
          }

	  // Create a new job with default attributes...
	  papplLogPrinter(printer, PAPPL_LOGLEVEL_INFO, "Accepted socket print connection from '%s'.", httpAddrGetString(&sockaddr, buffer, sizeof(buffer)));
          if ((job = _papplJobCreate(printer, 0, "guest", printer->driver_data.format ? printer->driver_data.format : "application/octet-stream", "Untitled", NULL)) == NULL)
          {
            close(sock);
            continue;
          }

          // Read the print data from the socket...
	  if ((job->fd = papplJobOpenFile(job, filename, sizeof(filename), printer->system->directory, NULL, "w")) < 0)
	  {
	    papplLogJob(job, PAPPL_LOGLEVEL_ERROR, "Unable to create print file: %s", strerror(errno));

	    goto abort_job;
	  }

	  if ((job->filename = strdup(filename)) == NULL)
	  {
            unlink(filename);
	    goto abort_job;
	  }

	  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "Created job file \"%s\", format \"%s\".", filename, job->format);

          activity     = time(NULL);
          sockp.fd     = sock;
          sockp.events = POLLIN | POLLERR;

          for (;;)
          {
	    if (papplPrinterIsDeleted(printer) || !papplSystemIsRunning(printer->system))
	    {
	      bytes = -1;
	      break;
	    }

            if ((bytes = poll(&sockp, 1, 1000)) < 0)
	    {
	      if ((time(NULL) - activity) >= 60)
	        break;
	      else
	        continue;
	    }

            activity = time(NULL);

            if (sockp.revents & POLLIN)
            {
              if ((bytes = recv(sock, buffer, sizeof(buffer), 0)) > 0)
                write(job->fd, buffer, (size_t)bytes);
              else
                break;
            }
            else if (sockp.revents & POLLERR)
            {
              bytes = -1;
              break;
            }
          }

          close(sock);
	  close(job->fd);
	  job->fd = -1;

          if (bytes < 0)
          {
            // Error while reading
	    goto abort_job;
	  }

	  // Finish the job...
	  job->state = IPP_JSTATE_PENDING;

	  _papplPrinterCheckJobs(printer);
	  continue;

	  // Abort the job...
	  abort_job:

	  job->state     = IPP_JSTATE_ABORTED;
	  job->completed = time(NULL);

	  _papplRWLockWrite(printer);

	  cupsArrayRemove(printer->active_jobs, job);
	  cupsArrayAdd(printer->completed_jobs, job);

	  _papplSystemNeedClean(printer->system);

	  _papplRWUnlock(printer);
        }
      }
    }
    else if (i < 0 && errno != EAGAIN)
      break;
  }

  _papplRWLockWrite(printer);
  printer->raw_active = false;
  _papplRWUnlock(printer);

  return (NULL);
}
