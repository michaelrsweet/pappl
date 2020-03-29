//
// Base definitions for the Printer Application Framework
//
// Copyright © 2019-2020 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#ifndef _PAPPL_BASE_H_
#  define _PAPPL_BASE_H_


//
// Include necessary headers...
//

#  include <cups/cups.h>
#  include <cups/raster.h>
#  include <stdio.h>
#  include <stdlib.h>
#  include <ctype.h>
#  include <errno.h>
#  include <fcntl.h>
#  include <pthread.h>
#  include <stdbool.h>
#  include <sys/stat.h>
#  include <unistd.h>


//
// New operations/tags defined in CUPS 2.3 and later...
//

#  if CUPS_VERSION_MAJOR == 2 && CUPS_VERSION_MINOR < 3
#    define IPP_OP_CREATE_PRINTER		(ipp_op_t)0x004C
#    define IPP_OP_DELETE_PRINTER		(ipp_op_t)0x004E
#    define IPP_OP_GET_PRINTERS			(ipp_op_t)0x004F
#    define IPP_OP_GET_SYSTEM_ATTRIBUTES	(ipp_op_t)0x005B
#    define IPP_OP_SET_SYSTEM_ATTRIBUTES	(ipp_op_t)0x0062
#    define IPP_OP_SHUTDOWN_ALL_PRINTERS	(ipp_op_t)0x0063

#    define IPP_TAG_SYSTEM			(ipp_tag_t)0x000A
#  endif // CUPS_VERSION_MAJOR == 2 && CUPS_VERSION_MINOR < 3


//
// Visibility and other annotations...
//

#  if defined(__has_extension) || defined(__GNUC__)
#    define _PAPPL_INTERNAL	__attribute__ ((visibility("hidden")))
#    define _PAPPL_PRIVATE	__attribute__ ((visibility("default")))
#    define _PAPPL_PUBLIC	__attribute__ ((visibility("default")))
#    define _PAPPL_FORMAT(a,b)	__attribute__ ((__format__(__printf__, a,b)))
#    define _PAPPL_NONNULL(...) __attribute__ ((nonnull(__VA_ARGS__)))
#    define _PAPPL_NORETURN	__attribute__ ((noreturn))
#  else
#    define _PAPPL_INTERNAL
#    define _PAPPL_PRIVATE
#    define _PAPPL_PUBLIC
#    define _PAPPL_FORMAT(a,b)
#    define _PAPPL_NONNULL(...)
#    define _PAPPL_NORETURN
#  endif // __has_extension || __GNUC__


//
// Common types...
//

typedef struct _pappl_client_s pappl_client_t;
					// Client connection object
typedef struct _pappl_device_s pappl_device_t;
					// Device connection object
typedef struct pappl_driver_data_s pappl_driver_data_t;
					// Driver data
typedef struct _pappl_job_s pappl_job_t;// Job object
typedef struct _pappl_printer_s pappl_printer_t;
					// Printer object
typedef struct _pappl_system_s pappl_system_t;
					// System object

typedef struct pappl_contact_s		// Contact information
{
  char	name[256],				// Contact name
	telephone[256],				// Contact phone number
	email[256];				// Contact email address
} pappl_contact_t;


#endif // !_PAPPL_BASE_H_
