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
#    define _CUPS_INTERNAL	__attribute__ ((visibility("hidden")))
#    define _CUPS_PRIVATE	__attribute__ ((visibility("default")))
#    define _CUPS_PUBLIC	__attribute__ ((visibility("default")))
#    define _CUPS_FORMAT(a,b)	__attribute__ ((__format__(__printf__, a,b)))
#    define _CUPS_NONNULL(...)	__attribute__ ((nonnull(__VA_ARGS__)))
#    define _CUPS_NORETURN	__attribute__ ((noreturn))
#  else
#    define _CUPS_INTERNAL
#    define _CUPS_PRIVATE
#    define _CUPS_PUBLIC
#    define _CUPS_FORMAT(a,b)
#    define _CUPS_NONNULL(...)
#    define _CUPS_NORETURN
#  endif // __has_extension || __GNUC__


#endif // !_PAPPL_BASE_H_
