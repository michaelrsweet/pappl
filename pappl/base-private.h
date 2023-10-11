//
// Private base definitions for the Printer Application Framework
//
// Copyright © 2019-2023 by Michael R Sweet.
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
#  include <cups/dnssd.h>
#  include <cups/thread.h>
#  if _WIN32
#    include <winreg.h>
#    include "win32-gettimeofday.h"
#    include "win32-socket.h"
#    define getuid()	0
#  else // !_WIN32
#    include <time.h>
#    include <sys/time.h>
#    include <grp.h>
#    include <poll.h>
#    include <sys/fcntl.h>
#    include <sys/wait.h>
extern char **environ;
#    define O_BINARY	0		// I hate Windows...
#  endif // _WIN32


//
// Macros...
//

#  ifdef DEBUG
#    define _PAPPL_DEBUG(...) fprintf(stderr, __VA_ARGS__)
#    define _papplRWLockRead(obj) fprintf(stderr, "%p/%s: rdlock %p(%s)\n", (void *)pthread_self(), __func__, (void *)obj, obj->name), cupsRWLockRead(&obj->rwlock)
#    define _papplRWLockWrite(obj) fprintf(stderr, "%p/%s: wrlock %p(%s)\n", (void *)pthread_self(), __func__, (void *)obj, obj->name), cupsRWLockWrite(&obj->rwlock)
#    define _papplRWUnlock(obj) fprintf(stderr, "%p/%s: unlock %p(%s)\n", (void *)pthread_self(), __func__, (void *)obj, obj->name), cupsRWUnlock(&obj->rwlock)
#  else
#    define _PAPPL_DEBUG(...)
#    define _papplRWLockRead(obj) cupsRWLockRead(&obj->rwlock)
#    define _papplRWLockWrite(obj) cupsRWLockWrite(&obj->rwlock)
#    define _papplRWUnlock(obj) cupsRWUnlock(&obj->rwlock)
#  endif // DEBUG

#  define _PAPPL_LOC(s) s
#  define _PAPPL_LOOKUP_STRING(bit,strings) _papplLookupString(bit, sizeof(strings) / sizeof(strings[0]), strings)
#  define _PAPPL_LOOKUP_VALUE(keyword,strings) _papplLookupValue(keyword, sizeof(strings) / sizeof(strings[0]), strings)


//
// Macros to implement a simple Fibonacci sequence for variable back-off...
//

#  define _PAPPL_FIB_NEXT(v) (((((v >> 8) + (v & 255) - 1) % 60) + 1) | ((v & 255) << 8))
#  define _PAPPL_FIB_VALUE(v) (v & 255)



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
extern bool		_papplIsEqual(const char *a, const char *b) _PAPPL_PRIVATE;
extern const char	*_papplLookupString(unsigned bit, size_t num_strings, const char * const *strings) _PAPPL_PRIVATE;
extern size_t		_papplLookupStrings(unsigned value, size_t max_keywords, char *keywords[], size_t num_strings, const char * const *strings) _PAPPL_PRIVATE;
extern unsigned		_papplLookupValue(const char *keyword, size_t num_strings, const char * const *strings) _PAPPL_PRIVATE;


#endif // !_PAPPL_BASE_PRIVATE_H_
