//
// Private base definitions for the Printer Application Framework
//
// Copyright © 2019-2024 by Michael R Sweet.
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
// The CUPS API is changed in CUPS v3...
//

#  if CUPS_VERSION_MAJOR < 3
#    define CUPS_ENCODING_ISO8859_1	CUPS_ISO8859_1
#    define CUPS_ENCODING_JIS_X0213	CUPS_JIS_X0213
#    define cups_len_t int
#    define cups_utf8_src_t char
#    define cups_page_header_t cups_page_header2_t
#    define cupsArrayNew cupsArrayNew3
#    define cupsLangGetName(lang)	lang->language
#    define cupsRasterReadHeader cupsRasterReadHeader2
#    define cupsRasterWriteHeader cupsRasterWriteHeader2
#    define httpAddrConnect httpAddrConnect2
#    define httpConnect httpConnect2
#    define httpGetDateString httpGetDateString2
#    define httpRead httpRead2
#    define httpWrite httpWrite2
#    define httpWriteResponse(http,code) (httpWriteResponse(http,code) == 0)
#    define IPP_NUM_CAST (int)
#    define cupsParseOptions cupsParseOptions2
#    define httpDecode64 httpDecode64_3
#    define httpEncode64 httpEncode64_3
#  else
#    define cups_len_t size_t
#    define cups_utf8_t char
#    define cups_utf8_src_t char
#    define IPP_NUM_CAST (size_t)
#  endif // CUPS_VERSION_MAJOR < 3


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

typedef struct _pappl_attr_s		// Input attribute structure
{
  const char	*name;			// Attribute name
  ipp_tag_t	value_tag;		// Value tag
  size_t	max_count;		// Max number of values
} _pappl_attr_t;

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

typedef struct _pappl_odevice_s _pappl_odevice_t;
					// Output device


//
// Utility functions...
//

extern ipp_t		*_papplContactExport(pappl_contact_t *contact) _PAPPL_PRIVATE;
extern void		_papplContactImport(ipp_t *col, pappl_contact_t *contact) _PAPPL_PRIVATE;
extern void		_papplCopyAttributes(ipp_t *to, ipp_t *from, cups_array_t *ra, ipp_tag_t group_tag, bool quickcopy) _PAPPL_PRIVATE;
extern bool		_papplIsEqual(const char *a, const char *b) _PAPPL_PRIVATE;
extern const char	*_papplLookupString(unsigned bit, size_t num_strings, const char * const *strings) _PAPPL_PRIVATE;
extern size_t		_papplLookupStrings(unsigned value, size_t max_keywords, char *keywords[], size_t num_strings, const char * const *strings) _PAPPL_PRIVATE;
extern unsigned		_papplLookupValue(const char *keyword, size_t num_strings, const char * const *strings) _PAPPL_PRIVATE;


#endif // !_PAPPL_BASE_PRIVATE_H_
