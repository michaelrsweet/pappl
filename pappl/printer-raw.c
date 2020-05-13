//
// Raw printing support for the Printer Application Framework
//
// Copyright © 2019-2020 by Michael R Sweet.
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
      printer->listeners[printer->num_listeners].fd     = sock;
      printer->listeners[printer->num_listeners].events = POLLIN | POLLERR;
      printer->num_listeners ++;
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
      printer->listeners[printer->num_listeners].fd     = sock;
      printer->listeners[printer->num_listeners].events = POLLIN | POLLERR;
      printer->num_listeners ++;
    }

    httpAddrFreeList(addrlist);
  }

  if (printer->num_listeners > 0)
    papplLogPrinter(printer, PAPPL_LOGLEVEL_INFO, "Listening for socket print jobs on '*:%d'.", port);

  return (printer->num_listeners > 0);
}


//
// '_papplPrinterRunRaw()' - Accept raw print requests over sockets.
//

void *					// O - Thread exit value
_papplPrinterRunRaw(
    pappl_printer_t *printer)		// I - Printer
{
  int	i;				// Looping var


  while (printer->listeners[0].fd >= 0)
  {
    // Don't accept connections if we can't accept a new job...
    while (cupsArrayCount(printer->active_jobs) >= printer->max_active_jobs && printer->listeners[0].fd >= 0)
      sleep(1);

    if (printer->listeners[0].fd < 0)
      break;

    // Wait 1 second for new connections...
    if ((i = poll(printer->listeners, printer->num_listeners, 1000)) > 0)
    {
      // Got a new connection request, accept from the corresponding listener...
      for (i = 0; i < printer->num_listeners; i ++)
      {
        if (printer->listeners[i].revents & POLLIN)
        {
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
          if ((sock = accept(printer->listeners[i].fd, (struct sockaddr *)&sockaddr, &sockaddrlen)) < 0)
          {
            papplLogPrinter(printer, PAPPL_LOGLEVEL_ERROR, "Unable to accept socket print connection: %s", strerror(errno));
            continue;
          }

	  // Create a new job with default attributes...
	  papplLogPrinter(printer, PAPPL_LOGLEVEL_INFO, "Accepted socket print connection from '%s'.", httpAddrString(&sockaddr, buffer, sizeof(buffer)));
          if ((job = _papplJobCreate(printer, "guest", printer->driver_data.format ? printer->driver_data.format : "application/octet-stream", "Untitled", NULL)) == NULL)
          {
            close(sock);
            continue;
          }

          // Read the print data from the socket...
	  if ((job->fd = papplJobCreateFile(job, filename, sizeof(filename), printer->system->directory, NULL)) < 0)
	  {
	    papplLogJob(job, PAPPL_LOGLEVEL_ERROR, "Unable to create print file: %s", strerror(errno));

	    goto abort_job;
	  }

	  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "Created job file \"%s\", format \"%s\".", filename, job->format);

          sockp.fd     = sock;
          sockp.events = POLLIN | POLLERR;

          while ((bytes = poll(&sockp, 1, 60000)) >= 0)
          {
            if (sockp.revents & POLLIN)
            {
              if ((bytes = read(sock, buffer, sizeof(buffer))) > 0)
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
	    unlink(filename);
	    goto abort_job;
	  }

	  // Finish the job...
	  job->filename = strdup(filename);
	  job->state    = IPP_JSTATE_PENDING;

	  _papplPrinterCheckJobs(printer);
	  continue;

	  // Abort the job...
	  abort_job:

	  job->state     = IPP_JSTATE_ABORTED;
	  job->completed = time(NULL);

	  pthread_rwlock_wrlock(&printer->rwlock);

	  cupsArrayRemove(printer->active_jobs, job);
	  cupsArrayAdd(printer->completed_jobs, job);

	  if (!printer->system->clean_time)
	    printer->system->clean_time = time(NULL) + 60;

	  pthread_rwlock_unlock(&printer->rwlock);
        }
      }
    }
    else if (i < 0 && errno != EAGAIN)
      break;
  }

  return (NULL);
}
