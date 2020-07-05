//
// Logging functions for the Printer Application Framework
//
// Copyright © 2019-2020 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

//
// Include necessary headers...
//

#include "client-private.h"
#include "job-private.h"
#include "log-private.h"
#include "printer-private.h"
#include "system-private.h"
#include <stdarg.h>
#include <syslog.h>


//
// Local functions...
//

static void	write_log(pappl_system_t *system, pappl_loglevel_t level, const char *message, va_list ap);


//
// Local globals...
//

static const int	syslevels[] =	// Mapping of log levels to syslog
{
  LOG_DEBUG | LOG_PID | LOG_LPR,
  LOG_INFO | LOG_PID | LOG_LPR,
  LOG_WARNING | LOG_PID | LOG_LPR,
  LOG_ERR | LOG_PID | LOG_LPR,
  LOG_CRIT | LOG_PID | LOG_LPR
};


//
// '_papplLogCheck()' - Open/Rotate Log File
//

void
_papplLogCheck(
    pappl_system_t *system) // I - System
{
  struct stat statbuf;          // Log file information
  char		    backname[1024],		// Backup log filename
              hostname[256];    // Hostname
  struct tm	  start_date;		    // System start date
  int         logfd;            // Log file descriptor

  // See if log file to check exists? OR handle logging to stderr
  if (!system->logfile || !system->logfile[0])
    return;

  pthread_rwlock_wrlock(&system->rwlock);

  if (!strcmp(system->logfile, "syslog"))
  {
    // Log to syslog...
    system->logfd = -1;
  }
  else if (!strcmp(system->logfile, "-"))
  {
    // Log to stderr...
    system->logfd = 2;
  }
  else
  {
    // Open the log file
    if ((logfd = open(system->logfile, O_RDONLY)) < 0)
    {
      // Check if file exists, if not create one
      if ((system->logfd = open(system->logfile, O_CREAT | O_WRONLY | O_APPEND | O_NOFOLLOW | O_CLOEXEC, 0600)) < 0)
      {
        // Fallback to stderr if we can't open the log file...
        perror(system->logfile);

        system->logfd = 2;
      }
    }

    // Get the log file information and check if log rotation is required?
    if (fstat(system->logfd, &statbuf) == 0 && statbuf.st_size > system->maxLogSize && system->maxLogSize > 0)
    {
      // change existing file to object file
      snprintf(backname, sizeof(backname), "%s.O", system->logfile);

      unlink(backname);

      // Rename old log file to new file name
      if (rename(system->logfile, backname) != 0)
      {
        fprintf(stderr, "error renaming already existing file %s: %s", system->logfile, strerror(errno));
        return;
      }

      logfd = system->logfd;

      // Create new log file
      if ((system->logfd = open(system->logfile, O_CREAT | O_WRONLY | O_APPEND | O_NOFOLLOW | O_CLOEXEC, 0600)) < 0)
      {
        // Fallback to stderr if we can't open the log file...
        perror(system->logfile);

        system->logfd = 2;
      }

      close(logfd);
    }
  }

  pthread_rwlock_unlock(&system->rwlock);

  // Log the system status information
  gmtime_r(&system->start_time, &start_date);

  papplSystemGetHostname(system, hostname, sizeof(hostname));

  papplLog(system, PAPPL_LOGLEVEL_INFO, "Restarting log.");
  papplLog(system, PAPPL_LOGLEVEL_INFO, "System started at %04d-%02d-%02dT%02d:%02d:%02dZ", start_date.tm_year + 1900, start_date.tm_mon + 1, start_date.tm_mday, start_date.tm_hour, start_date.tm_min, start_date.tm_sec);
  papplLog(system, PAPPL_LOGLEVEL_INFO, "Listening for connections on '%s:%d'.", (hostname[0] == '\0') ? "*" : hostname, system->port);

  // Log printer state/job
  papplSystemIteratePrinters(system, (pappl_printer_cb_t)_papplPrinterIteratorStatusCallback, system);
}


//
// '_papplPrinterIteratorStatusCallback()' - Log the printer info
//

void
_papplPrinterIteratorStatusCallback(
    pappl_printer_t *printer,  // I - Printer
    pappl_system_t *system)    // I -System
{
  ipp_pstate_t		printer_state;	// Printer state
  int			        printer_jobs;	  // Number of queued jobs
  static const char * const states[] =	// State strings
  {
    "Idle",
    "Printing",
    "Stopped"
  };

  printer_jobs    = papplPrinterGetActiveJobs(printer);
  printer_state   = papplPrinterGetState(printer);

  papplLog(system, PAPPL_LOGLEVEL_INFO, "Printer '%s' at resource path '%s'.", printer->name, printer->resource);
  papplLog(system, PAPPL_LOGLEVEL_INFO, "Printer '%s' %s, %d %s", printer->name, states[printer_state - IPP_STATE_IDLE], printer_jobs, printer_jobs == 1 ? "job" : "jobs");
}


//
// 'papplLog()' - Log a message for the system.
//

void
papplLog(pappl_system_t   *system,	// I - System
         pappl_loglevel_t level,	// I - Log level
         const char       *message,	// I - Printf-style message string
         ...)				// I - Additional arguments as needed
{
  va_list	ap;			// Pointer to arguments


  if (!message)
    return;

  if (!system)
  {
    if (level >= PAPPL_LOGLEVEL_WARN)
    {
      va_start(ap, message);
      vfprintf(stderr, message, ap);
      putc('\n', stderr);
      va_end(ap);
    }

    return;
  }

  if (level < system->loglevel)
    return;

  va_start(ap, message);

  if (system->logfd >= 0)
    write_log(system, level, message, ap);
  else
    vsyslog(syslevels[level], message, ap);

  va_end(ap);
}


//
// 'papplLogAttributes()' - Log attributes for a client connection.
//

void
papplLogAttributes(
    pappl_client_t *client,		// I - Client
    const char     *title,		// I - Title for attributes
    ipp_t          *ipp,		// I - IPP message
    bool           is_response)		// I - `true` if a response, `false` if a request
{
  int			major,		// Major version number
			minor;		// Minor version number
  ipp_attribute_t	*attr;		// Current attribute
  ipp_tag_t		group = IPP_TAG_ZERO;
					// Current group
  const char		*name;		// Name
  char			value[1024];	// Value


  if (!client || !title || !ipp)
    return;

  if (client->system->loglevel > PAPPL_LOGLEVEL_DEBUG)
    return;

  major = ippGetVersion(ipp, &minor);
  if (is_response)
    papplLogClient(client, PAPPL_LOGLEVEL_DEBUG, "%s response: IPP/%d.%d request-id=%d, status-code=%s", title, major, minor, ippGetRequestId(ipp), ippErrorString(ippGetStatusCode(ipp)));
  else
    papplLogClient(client, PAPPL_LOGLEVEL_DEBUG, "%s request: IPP/%d.%d request-id=%d", title, major, minor, ippGetRequestId(ipp));

  for (attr = ippFirstAttribute(ipp); attr; attr = ippNextAttribute(ipp))
  {
    if ((name = ippGetName(attr)) == NULL)
    {
      group = IPP_TAG_ZERO;
      continue;
    }

    if (ippGetGroupTag(attr) != group)
    {
      group = ippGetGroupTag(attr);
      papplLogClient(client, PAPPL_LOGLEVEL_DEBUG, "%s %s: %s", title, is_response ? "response" : "request", ippTagString(group));
    }

    ippAttributeString(attr, value, sizeof(value));
    papplLogClient(client, PAPPL_LOGLEVEL_DEBUG, "%s %s:   %s %s%s %s", title, is_response ? "response" : "request", name, ippGetCount(attr) > 1 ? "1setOf " : "", ippTagString(ippGetValueTag(attr)), value);
  }
}


//
// 'papplLogClient()' - Log a message for a client.
//

void
papplLogClient(
    pappl_client_t   *client,		// I - Client
    pappl_loglevel_t level,		// I - Log level
    const char       *message,		// I - Printf-style message string
    ...)				// I - Additional arguments as needed
{
  char		cmessage[1024];		// Message with client prefix
  va_list	ap;			// Pointer to arguments


  if (!client || !message)
    return;

  if (level < client->system->loglevel)
    return;

  snprintf(cmessage, sizeof(cmessage), "[Client %d] %s", client->number, message);
  va_start(ap, message);

  if (client->system->logfd >= 0)
    write_log(client->system, level, cmessage, ap);
  else
    vsyslog(syslevels[level], cmessage, ap);

  va_end(ap);
}


//
// 'papplLogDevice()' - Log a device error for the system...
//

void
papplLogDevice(
    const char *message,		// I - Message
    void       *data)			// I - System
{
  pappl_system_t	*system = (pappl_system_t *)data;
					// System


  papplLog(system, PAPPL_LOGLEVEL_ERROR, "[Device] %s", message);
}


//
// 'papplLogJob()' - Log a message for a job.
//

void
papplLogJob(
    pappl_job_t      *job,		// I - Job
    pappl_loglevel_t level,		// I - Log level
    const char       *message,		// I - Printf-style message string
    ...)				// I - Additional arguments as needed
{
  char		jmessage[1024];		// Message with job prefix
  va_list	ap;			// Pointer to arguments


  if (!job || !message)
    return;

  if (level < job->system->loglevel)
    return;

  snprintf(jmessage, sizeof(jmessage), "[Job %d] %s", job->job_id, message);
  va_start(ap, message);

  if (job->system->logfd >= 0)
    write_log(job->system, level, jmessage, ap);
  else
    vsyslog(syslevels[level], jmessage, ap);

  va_end(ap);
}


//
// 'papplLogPrinter()' - Log a message for a printer.
//

void
papplLogPrinter(
    pappl_printer_t  *printer,		// I - Printer
    pappl_loglevel_t level,		// I - Log level
    const char       *message,		// I - Printf-style message string
    ...)				// I - Additional arguments as needed
{
  char		pmessage[1024];		// Message with printer prefix
  va_list	ap;			// Pointer to arguments


  if (!printer || !message)
    return;

  if (level < printer->system->loglevel)
    return;

  snprintf(pmessage, sizeof(pmessage), "[Printer %s] %s", printer->name, message);
  va_start(ap, message);

  if (printer->system->logfd >= 0)
    write_log(printer->system, level, pmessage, ap);
  else
    vsyslog(syslevels[level], pmessage, ap);

  va_end(ap);
}


//
// 'write_log()' - Write a line to the log file...
//

static void
write_log(pappl_system_t   *system,	// I - System
          pappl_loglevel_t level,	// I - Log level
          const char       *message,	// I - Printf-style message string
          va_list          ap)		// I - Pointer to additional arguments
{
  char		buffer[2048],		// Output buffer
		*bufptr,		// Pointer into buffer
		*bufend;		// Pointer to end of buffer
  struct timeval curtime;		// Current time
  struct tm	curdate;		// Current date
  static const char *prefix = "DIWEF";	// Message prefix
  const char	*sval;			// String value
  char		size,			// Size character (h, l, L)
		type;			// Format type character
  int		width,			// Width of field
		prec;			// Number of characters of precision
  char		tformat[100],		// Temporary format string for sprintf()
		*tptr;			// Pointer into temporary format


  // Each log line starts with a standard prefix of log level and date/time...
  gettimeofday(&curtime, NULL);
  gmtime_r(&curtime.tv_sec, &curdate);

  snprintf(buffer, sizeof(buffer), "%c [%04d-%02d-%02dT%02d:%02d:%02d.%03dZ] ", prefix[level], curdate.tm_year + 1900, curdate.tm_mon + 1, curdate.tm_mday, curdate.tm_hour, curdate.tm_min, curdate.tm_sec, (int)(curtime.tv_usec / 1000));
  bufptr = buffer + 29;			// Skip level/date/time
  bufend = buffer + sizeof(buffer) - 1;	// Leave room for newline on end

  // Then format the message line using printf format sequences...
  while (*message && bufptr < bufend)
  {
    if (*message == '%')
    {
      tptr    = tformat;
      *tptr++ = *message++;

      if (*message == '%')
      {
        *bufptr++ = *message++;
	continue;
      }
      else if (strchr(" -+#\'", *message))
        *tptr++ = *message++;

      if (*message == '*')
      {
        // Get width from argument...
	message ++;
	width = va_arg(ap, int);

	snprintf(tptr, sizeof(tformat) - (size_t)(tptr - tformat), "%d", width);
	tptr += strlen(tptr);
      }
      else
      {
	width = 0;

	while (isdigit(*message & 255))
	{
	  if (tptr < (tformat + sizeof(tformat) - 1))
	    *tptr++ = *message;

	  width = width * 10 + *message++ - '0';
	}
      }

      if (*message == '.')
      {
	if (tptr < (tformat + sizeof(tformat) - 1))
	  *tptr++ = *message;

        message ++;

        if (*message == '*')
	{
          // Get precision from argument...
	  message ++;
	  prec = va_arg(ap, int);

	  snprintf(tptr, sizeof(tformat) - (size_t)(tptr - tformat), "%d", prec);
	  tptr += strlen(tptr);
	}
	else
	{
	  prec = 0;

	  while (isdigit(*message & 255))
	  {
	    if (tptr < (tformat + sizeof(tformat) - 1))
	      *tptr++ = *message;

	    prec = prec * 10 + *message++ - '0';
	  }
	}
      }

      if (*message == 'l' && message[1] == 'l')
      {
        size = 'L';

	if (tptr < (tformat + sizeof(tformat) - 2))
	{
	  *tptr++ = 'l';
	  *tptr++ = 'l';
	}

	message += 2;
      }
      else if (*message == 'h' || *message == 'l' || *message == 'L')
      {
	if (tptr < (tformat + sizeof(tformat) - 1))
	  *tptr++ = *message;

        size = *message++;
      }
      else
        size = 0;

      if (!*message)
        break;

      if (tptr < (tformat + sizeof(tformat) - 1))
        *tptr++ = *message;

      type  = *message++;
      *tptr = '\0';

      switch (type)
      {
	case 'E' : // Floating point formats
	case 'G' :
	case 'e' :
	case 'f' :
	case 'g' :
	    snprintf(bufptr, bufptr - bufend + 1, tformat, va_arg(ap, double));
	    bufptr += strlen(bufptr);
	    break;

        case 'B' : // Integer formats
	case 'X' :
	case 'b' :
        case 'd' :
	case 'i' :
	case 'o' :
	case 'u' :
	case 'x' :
#  ifdef HAVE_LONG_LONG
            if (size == 'L')
	      snprintf(bufptr, bufend - bufptr + 1, tformat, va_arg(ap, long long));
	    else
#  endif // HAVE_LONG_LONG
            if (size == 'l')
	      snprintf(bufptr, bufend - bufptr + 1, tformat, va_arg(ap, long));
	    else
	      snprintf(bufptr, bufend - bufptr + 1, tformat, va_arg(ap, int));
            bufptr += strlen(bufptr);
            break;

        case 'p' : // Log a pointer
            snprintf(bufptr, bufend - bufptr, "%p", va_arg(ap, void *));
            bufptr += strlen(bufptr);
            break;

        case 'c' : // Character or character array
            if (width <= 1)
            {
              *bufptr++ = (char)va_arg(ap, int);
            }
            else
            {
              if ((bufend - bufptr) < width)
                width = (int)(bufend - bufptr);

              memcpy(bufptr, va_arg(ap, char *), (size_t)width);
              bufptr += width;
	    }
	    break;

        case 's' : // Log a string
            if ((sval = va_arg(ap, char *)) == NULL)
              sval = "(null)";

            while (*sval && bufptr < bufend)
            {
              int val = (*sval++) & 255;

              if (val < ' ' || val == 0x7f || val == '\\' || val == '\'' || val == '\"')
              {
                // Escape control and special characters in the string...
                if (bufptr > (bufend - 4))
                  break;

                *bufptr++ = '\\';

                if (val == '\\')
                  *bufptr++ = '\\';
                else if (val == '\'')
                  *bufptr++ = '\'';
                else if (val == '\"')
                  *bufptr++ = '\"';
                else if (val == '\n')
                  *bufptr++ = 'n';
                else if (val == '\r')
                  *bufptr++ = 'r';
                else if (val == '\t')
                  *bufptr++ = 't';
                else
                {
                  // Use octal escape for other control characters...
                  *bufptr++ = '0' + (val / 64);
                  *bufptr++ = '0' + ((val / 8) & 7);
                  *bufptr++ = '0' + (val & 7);
                }
              }
              else
                *bufptr++ = val;
            }
            break;

        default : // Something else we don't support
            strlcpy(bufptr, tformat, bufend - bufptr + 1);
            bufptr += strlen(bufptr);
            break;
      }
    }
    else
      *bufptr++ = *message++;
  }

  // Add a newline and write it out...
  *bufptr++ = '\n';

  write(system->logfd, buffer, bufptr - buffer);
}
