//
// Private base definitions for the Printer Application Framework
//
// Copyright © 2019-2020 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#ifndef _PAPPL_BASE_PRIVATE_H_
#  define _PAPPL_BASE_PRIVATE_H_


//
// Include necessary headers...
//

#  include "base.h"
#  include <config.h>
#  include <limits.h>
#  include <poll.h>
#  include <sys/fcntl.h>
#  include <sys/stat.h>
#  include <sys/wait.h>
#  ifdef HAVE_DNSSD
#    include <dns_sd.h>
#  elif defined(HAVE_AVAHI)
#    include <avahi-client/client.h>
#    include <avahi-client/publish.h>
#    include <avahi-common/error.h>
#    include <avahi-common/thread-watch.h>
#  endif // HAVE_DNSSD

extern char **environ;


//
// Macros...
//

#  ifdef DEBUG
#    define _PAPPL_DEBUG(...) fprintf(stderr, __VA_ARGS__)
#  else
#    define _PAPPL_DEBUG(...)
#  endif // DEBUG

#  define _PAPPL_LOOKUP_STRING(bit,strings) _papplLookupString(bit, sizeof(strings) / sizeof(strings[0]), strings)
#  define _PAPPL_LOOKUP_VALUE(keyword,strings) _papplLookupValue(keyword, sizeof(strings) / sizeof(strings[0]), strings)

#  ifndef HAVE_STRLCPY
#    define strlcpy(dst,src,dstsize) _pappl_strlcpy(dst,src,dstsize)
#  endif // !HAVE_STRLCPY


//
// Types and structures...
//

#  ifdef HAVE_DNSSD
typedef DNSServiceRef _pappl_srv_t;	// Service reference
typedef TXTRecordRef _pappl_txt_t;	// TXT record

#elif defined(HAVE_AVAHI)
typedef AvahiEntryGroup *_pappl_srv_t;	// Service reference
typedef AvahiStringList *_pappl_txt_t;	// TXT record

#else
typedef void *_pappl_srv_t;		// Service reference
typedef void *_pappl_txt_t;		// TXT record
#endif // HAVE_DNSSD

typedef struct _pappl_filter_s		// Attribute filter
{
  cups_array_t		*ra;			// Requested attributes
  ipp_tag_t		group_tag;		// Group to copy
} _pappl_filter_t;


//
// Utility functions...
//

#  ifndef HAVE_STRLCPY
extern size_t		_pappl_strlcpy(char *dst, const char *src, size_t dstsize) _PAPPL_PRIVATE;
#  endif // !HAVE_STRLCPY
extern ipp_t		*_papplContactExport(pappl_contact_t *contact) _PAPPL_PRIVATE;
extern void		_papplContactImport(ipp_t *col, pappl_contact_t *contact) _PAPPL_PRIVATE;
extern void		_papplCopyAttributes(ipp_t *to, ipp_t *from, cups_array_t *ra, ipp_tag_t group_tag, int quickcopy) _PAPPL_PRIVATE;
extern unsigned		_papplGetRand(void) _PAPPL_PRIVATE;
extern const char	*_papplLookupString(unsigned bit, size_t num_strings, const char * const *strings) _PAPPL_PRIVATE;
extern unsigned		_papplLookupValue(const char *keyword, size_t num_strings, const char * const *strings) _PAPPL_PRIVATE;


#endif // !_PAPPL_BASE_PRIVATE_H_
