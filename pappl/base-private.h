//
// Private base definitions for the Printer Application Framework
//
// Copyright © 2019-2022 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#ifndef _PAPPL_BASE_PRIVATE_H_
#  define _PAPPL_BASE_PRIVATE_H_
#  include <config.h>
#  include "base.h"
#  include <limits.h>
#  include <sys/stat.h>
#  if _WIN32
#    include <winreg.h>
#    include "win32-gettimeofday.h"
#    include "win32-pthread.h"
#    include "win32-socket.h"
#    define getuid()	0
#  else // !_WIN32
#    include <time.h>
#    include <sys/time.h>
#    include <grp.h>
#    include <poll.h>
#    include <pthread.h>
#    include <sys/fcntl.h>
#    include <sys/wait.h>
extern char **environ;
#    define O_BINARY	0		// I hate Windows...
#  endif // _WIN32


//
// The CUPS array API is changed in CUPS v3...
//

#  if CUPS_VERSION_MAJOR < 3
#    define cupsArrayNew cupsArrayNew3
#    define cupsArrayGetCount (size_t)cupsArrayCount
#    define cupsArrayGetElement(a,n) cupsArrayIndex(a,(int)n)
#    define cupsArrayGetFirst cupsArrayFirst
#    define cupsArrayGetLast cupsArrayLast
#    define cupsArrayGetNext cupsArrayNext
#    define cupsArrayGetPrev cupsArrayPrev
#    define cupsGetUser cupsUser
#    define httpConnect httpConnect2
#    define httpReconnect httpReconnect2
#    define IPP_NUM_CAST (int)
typedef cups_array_func_t cups_array_cb_t;
typedef cups_acopy_func_t cups_acopy_cb_t;
typedef cups_afree_func_t cups_afree_cb_t;
#  else
#    define IPP_NUM_CAST
#  endif // CUPS_VERSION_MAJOR < 3


//
// Macros...
//

#  ifdef DEBUG
#    define _PAPPL_DEBUG(...) fprintf(stderr, __VA_ARGS__)
#  else
#    define _PAPPL_DEBUG(...)
#  endif // DEBUG

#  define _PAPPL_LOC(s) s
#  define _PAPPL_LOOKUP_STRING(bit,strings) _papplLookupString(bit, sizeof(strings) / sizeof(strings[0]), strings)
#  define _PAPPL_LOOKUP_VALUE(keyword,strings) _papplLookupValue(keyword, sizeof(strings) / sizeof(strings[0]), strings)


//
// Types and structures...
//

typedef struct _pappl_ipp_filter_s	// Attribute filter
{
  cups_array_t		*ra;			// Requested attributes
  ipp_tag_t		group_tag;		// Group to copy
} _pappl_ipp_filter_t;

typedef struct _pappl_link_s		// Web interface navigation link
{
  char			*label,			// Label
			*path_or_url;		// Path or URL
  pappl_loptions_t	options;		// Link options
} _pappl_link_t;


//
// Utility functions...
//

extern ipp_t		*_papplContactExport(pappl_contact_t *contact) _PAPPL_PRIVATE;
extern void		_papplContactImport(ipp_t *col, pappl_contact_t *contact) _PAPPL_PRIVATE;
extern void		_papplCopyAttributes(ipp_t *to, ipp_t *from, cups_array_t *ra, ipp_tag_t group_tag, int quickcopy) _PAPPL_PRIVATE;
extern const char	*_papplLookupString(unsigned bit, size_t num_strings, const char * const *strings) _PAPPL_PRIVATE;
extern unsigned		_papplLookupValue(const char *keyword, size_t num_strings, const char * const *strings) _PAPPL_PRIVATE;


#endif // !_PAPPL_BASE_PRIVATE_H_
