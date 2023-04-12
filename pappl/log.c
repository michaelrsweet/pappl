//
// Logging functions for the Printer Application Framework
//
// Copyright © 2019-2021 by Michael R Sweet.
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
#if !_WIN32
#  include <syslog.h>
#endif // !_WIN32


//
// Local functions...
//

static void	rotate_log(pappl_system_t *system);
static void	write_log(pappl_system_t *system, pappl_loglevel_t level, const char *message, va_list ap);


//
// Local globals...
//

static pthread_mutex_t	log_mutex = PTHREAD_MUTEX_INITIALIZER;
					// Log rotation mutex
#if !_WIN32
static const int	syslevels[] =	// Mapping of log levels to syslog
{
  LOG_DEBUG | LOG_PID | LOG_LPR,
  LOG_INFO | LOG_PID | LOG_LPR,
  LOG_WARNING | LOG_PID | LOG_LPR,
  LOG_ERR | LOG_PID | LOG_LPR,
  LOG_CRIT | LOG_PID | LOG_LPR
};
#endif // !_WIN32


//
// 'papplLog()' - Log a message for the system.
//
// This function sends a message to the system's log file.  The "level" argument
// specifies the urgency of the message:
//
// - `PAPPL_LOGLEVEL_DEBUG`: A debugging message.
// - `PAPPL_LOGLEVEL_ERROR`: An error message.
// - `PAPPL_LOGLEVEL_FATAL`: A fatal error message.
// - `PAPPL_LOGLEVEL_INFO`: An informational message.
// - `PAPPL_LOGLEVEL_WARN`: A warning message.
//
// The "message" argument specifies a `printf`-style format string.  Values
// logged using the "%c" and "%s" format specifiers are sanitized to not
// contain control characters.
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
#if !_WIN32
  else
    vsyslog(syslevels[level], message, ap);
#endif // !_WIN32

  va_end(ap);
}


//
// '_papplLogAttributes()' - Log IPP attributes for a client connection.
//
// This function logs the IPP attributes sent or recieved on a client
// connection at the `PAPPL_LOGLEVEL_DEBUG` (debug) log level.
//

void
_papplLogAttributes(
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

  for (attr = ippGetFirstAttribute(ipp); attr; attr = ippGetNextAttribute(ipp))
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
// This function sends a client message to the system's log file.  The "level"
// argument specifies the urgency of the message:
//
// - `PAPPL_LOGLEVEL_DEBUG`: A debugging message.
// - `PAPPL_LOGLEVEL_ERROR`: An error message.
// - `PAPPL_LOGLEVEL_FATAL`: A fatal error message.
// - `PAPPL_LOGLEVEL_INFO`: An informational message.
// - `PAPPL_LOGLEVEL_WARN`: A warning message.
//
// The "message" argument specifies a `printf`-style format string.  Values
// logged using the "%c" and "%s" format specifiers are sanitized to not
// contain control characters.
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
#if !_WIN32
  else
    vsyslog(syslevels[level], cmessage, ap);
#endif // !_WIN32

  va_end(ap);
}


//
// 'papplLogDevice()' - Log a device error for the system...
//
// This function sends a device error message to the system's log file.
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
// This function sends a job message to the system's log file.  The "level"
// argument specifies the urgency of the message:
//
// - `PAPPL_LOGLEVEL_DEBUG`: A debugging message.
// - `PAPPL_LOGLEVEL_ERROR`: An error message.
// - `PAPPL_LOGLEVEL_FATAL`: A fatal error message.
// - `PAPPL_LOGLEVEL_INFO`: An informational message.
// - `PAPPL_LOGLEVEL_WARN`: A warning message.
//
// The "message" argument specifies a `printf`-style format string.  Values
// logged using the "%c" and "%s" format specifiers are sanitized to not
// contain control characters.
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
#if !_WIN32
  else
    vsyslog(syslevels[level], jmessage, ap);
#endif // !_WIN32

  va_end(ap);
}


//
// '_papplLogOpen()' - Open the log file
//

void
_papplLogOpen(
    pappl_system_t *system)		// I - System
{
  // Open the log file...
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
    int	oldfd = system->logfd;		// Old log file descriptor

    // Log to a file...
    if ((system->logfd = open(system->logfile, O_CREAT | O_WRONLY | O_APPEND | O_NOFOLLOW | O_CLOEXEC, 0600)) < 0)
    {
      // Fallback to logging to stderr if we can't open the log file...
      perror(system->logfile);

      system->logfd = 2;
    }

    // Close any old file...
    if (oldfd != -1)
      close(oldfd);
  }

  // Log the system status information
  papplLog(system, PAPPL_LOGLEVEL_INFO, "Starting log, system up %ld second(s), %d printer(s), listening for connections on '%s:%d' from up to %d clients.", (long)(time(NULL) - system->start_time), (int)cupsArrayGetCount(system->printers), system->hostname, system->port, system->max_clients);
}


//
// 'papplLogPrinter()' - Log a message for a printer.
//
// This function sends a printer message to the system's log file.  The "level"
// argument specifies the urgency of the message:
//
// - `PAPPL_LOGLEVEL_DEBUG`: A debugging message.
// - `PAPPL_LOGLEVEL_ERROR`: An error message.
// - `PAPPL_LOGLEVEL_FATAL`: A fatal error message.
// - `PAPPL_LOGLEVEL_INFO`: An informational message.
// - `PAPPL_LOGLEVEL_WARN`: A warning message.
//
// The "message" argument specifies a `printf`-style format string.  Values
// logged using the "%c" and "%s" format specifiers are sanitized to not
// contain control characters.
//

void
papplLogPrinter(
    pappl_printer_t  *printer,		// I - Printer
    pappl_loglevel_t level,		// I - Log level
    const char       *message,		// I - Printf-style message string
    ...)				// I - Additional arguments as needed
{
  char		pmessage[1024],		// Message with printer prefix
		*pptr,			// Pointer into prefix
		*nameptr;		// Pointer into printer name
  va_list	ap;			// Pointer to arguments


  if (!printer || !message)
    return;

  if (level < printer->system->loglevel)
    return;

  // Prefix the message with "[Printer foo]", making sure to not insert any
  // printf format specifiers.
  papplCopyString(pmessage, "[Printer ", sizeof(pmessage));
  for (pptr = pmessage + 9, nameptr = printer->name; *nameptr && pptr < (pmessage + 200); pptr ++)
  {
    if (*nameptr == '%')
      *pptr++ = '%';
    *pptr = *nameptr++;
  }
  *pptr++ = ']';
  *pptr++ = ' ';
  papplCopyString(pptr, message, sizeof(pmessage) - (size_t)(pptr - pmessage));

  // Write the log message...
  va_start(ap, message);

  if (printer->system->logfd >= 0)
    write_log(printer->system, level, pmessage, ap);
#if !_WIN32
  else
    vsyslog(syslevels[level], pmessage, ap);
#endif // !_WIN32

  va_end(ap);
}


//
// 'rotate_log()' - Rotate the log file...
//

static void
rotate_log(pappl_system_t *system)	// I - System
{
  struct stat	loginfo;		// Lof file information


  // Re-check whether we need to rotate the log file...
  if (!fstat(system->logfd, &loginfo) && loginfo.st_size >= (off_t)system->logmaxsize)
  {
    // Rename existing log file to "xxx.O"
    char	backname[1024];		// Backup log filename

#if _WIN32
    // Windows doesn't allow an open file to be renamed...
    close(system->logfd);
    system->logfd = -1;
#endif // _WIN32

    snprintf(backname, sizeof(backname), "%s.O", system->logfile);
    unlink(backname);
    rename(system->logfile, backname);

    _papplLogOpen(system);
  }
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
  struct stat	loginfo;		// Log file information
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


  // Rotate log as needed...
  if (system->logmaxsize > 0 && !fstat(system->logfd, &loginfo) && loginfo.st_size >= (off_t)system->logmaxsize)
  {
    pthread_mutex_lock(&log_mutex);
    rotate_log(system);
    pthread_mutex_unlock(&log_mutex);
  }

  // Each log line starts with a standard prefix of log level and date/time...
  gettimeofday(&curtime, NULL);
#if _WIN32
  time_t curtemp = (time_t)curtime.tv_sec;
  gmtime_s(&curdate, &curtemp);
#else
  gmtime_r(&curtime.tv_sec, &curdate);
#endif // _WIN32

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
	    snprintf(bufptr, (size_t)(bufend - bufptr), tformat, va_arg(ap, double));
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
	      snprintf(bufptr, (size_t)(bufend - bufptr), tformat, va_arg(ap, long long));
	    else
#  endif // HAVE_LONG_LONG
            if (size == 'l')
	      snprintf(bufptr, (size_t)(bufend - bufptr), tformat, va_arg(ap, long));
	    else
	      snprintf(bufptr, (size_t)(bufend - bufptr), tformat, va_arg(ap, int));
            bufptr += strlen(bufptr);
            break;

        case 'p' : // Log a pointer
            snprintf(bufptr, (size_t)(bufend - bufptr), "%p", va_arg(ap, void *));
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
                  *bufptr++ = (char)('0' + (val / 64));
                  *bufptr++ = (char)('0' + ((val / 8) & 7));
                  *bufptr++ = (char)('0' + (val & 7));
                }
              }
              else
                *bufptr++ = (char)val;
            }
            break;

        default : // Something else we don't support
            papplCopyString(bufptr, tformat, (size_t)(bufend - bufptr));
            bufptr += strlen(bufptr);
            break;
      }
    }
    else
      *bufptr++ = *message++;
  }

  // Add a newline and write it out...
  *bufptr++ = '\n';

  write(system->logfd, buffer, (size_t)(bufptr - buffer));
}
