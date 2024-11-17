//
// Logging header file for the Printer Application Framework
//
// Copyright © 2019-2022 by Michael R Sweet.
// Copyright © 2010-2019 by Apple Inc.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#ifndef _PAPPL_LOG_H_
#  define _PAPPL_LOG_H_
#  include "base.h"
#  ifdef __cplusplus
extern "C" {
#  endif // __cplusplus


//
// Constants...
//

typedef enum pappl_loglevel_e		// Log levels
{
  PAPPL_LOGLEVEL_UNSPEC = -1,			// Not specified
  PAPPL_LOGLEVEL_DEBUG,				// Debug message
  PAPPL_LOGLEVEL_INFO,				// Informational message
  PAPPL_LOGLEVEL_WARN,				// Warning message
  PAPPL_LOGLEVEL_ERROR,				// Error message
  PAPPL_LOGLEVEL_FATAL				// Fatal message
} pappl_loglevel_t;


//
// Functions...
//

extern void		papplLog(pappl_system_t *system, pappl_loglevel_t level, const char *message, ...) _PAPPL_PUBLIC _PAPPL_FORMAT(3,4);
extern void		papplLogClient(pappl_client_t *client, pappl_loglevel_t level, const char *message, ...) _PAPPL_PUBLIC _PAPPL_FORMAT(3, 4);
extern void		papplLogDevice(const char *message, void *data) _PAPPL_PUBLIC;
extern void		papplLogJob(pappl_job_t *job, pappl_loglevel_t level, const char *message, ...) _PAPPL_PUBLIC _PAPPL_FORMAT(3, 4);
extern void		papplLogPrinter(pappl_printer_t *printer, pappl_loglevel_t level, const char *message, ...) _PAPPL_PUBLIC _PAPPL_FORMAT(3, 4);
extern void		papplLogScanner(pappl_scanner_t *scanner, pappl_loglevel_t level, const char *message, ...) _PAPPL_PUBLIC _PAPPL_FORMAT(3, 4);


#  ifdef __cplusplus
}
#  endif // __cplusplus
#endif // !_PAPPL_LOG_H_
