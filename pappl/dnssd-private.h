//
// Private DNS-SD header file for the Printer Application Framework
//
// Copyright © 2019-2023 by Michael R Sweet.
// Copyright © 2010-2019 by Apple Inc.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#ifndef _PAPPL_DNSSD_PRIVATE_H_
#  define _PAPPL_DNSSD_PRIVATE_H_
#  include "base-private.h"
#  ifdef HAVE_MDNSRESPONDER
#    include <dns_sd.h>
#  elif defined(HAVE_AVAHI)
#    include <avahi-client/client.h>
#    include <avahi-client/lookup.h>
#    include <avahi-client/publish.h>
#    include <avahi-common/domain.h>
#    include <avahi-common/error.h>
#    include <avahi-common/thread-watch.h>
#    include <net/if.h>
#  endif // HAVE_MDNSRESPONDER


//
// Types and structures...
//

#  ifdef HAVE_MDNSRESPONDER
typedef DNSServiceRef _pappl_srv_t;	// DNS-SD service reference
typedef TXTRecordRef _pappl_txt_t;	// DNS-SD TXT record
typedef DNSServiceRef _pappl_dns_sd_t;	// DNS-SD master reference

#elif defined(HAVE_AVAHI)
typedef AvahiEntryGroup *_pappl_srv_t;	// DNS-SD service reference
typedef AvahiStringList *_pappl_txt_t;	// DNS-SD TXT record
typedef AvahiClient *_pappl_dns_sd_t;	// DNS-SD master reference

#else
typedef void *_pappl_srv_t;		// DNS-SD service reference
typedef void *_pappl_txt_t;		// DNS-SD TXT record
typedef void *_pappl_dns_sd_t;		// DNS-SD master reference
#endif // HAVE_MDNSRESPONDER


//
// Functions...
//

extern const char	*_papplDNSSDCopyHostName(char *buffer, size_t bufsize) _PAPPL_PRIVATE;
extern int		_papplDNSSDGetHostChanges(void) _PAPPL_PRIVATE;
extern _pappl_dns_sd_t	_papplDNSSDInit(pappl_system_t *system) _PAPPL_PRIVATE;
extern void		_papplDNSSDLock(void) _PAPPL_PRIVATE;
extern const char	*_papplDNSSDStrError(int error) _PAPPL_PRIVATE;
extern void		_papplDNSSDUnlock(void) _PAPPL_PRIVATE;


#endif // !_PAPPL_DNSSD_PRIVATE_H_
