//
// Private scanner header file for the Scanner Application Framework
//
// Copyright © 2019-2024 by Michael R Sweet.
// Copyright © 2010-2019 by Apple Inc.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#ifndef _PAPPL_SCANNER_PRIVATE_H_
#  define _PAPPL_SCANNER_PRIVATE_H_
#  include "dnssd-private.h"
#  include "scanner.h" // Changed to Scanner.h
#  include "log.h"
#  ifdef __APPLE__
#    include <sys/param.h>
#    include <sys/mount.h>
#  elif !_WIN32
#    include <sys/statfs.h>
#  endif // __APPLE__
#  ifdef HAVE_SYS_RANDOM_H
#    include <sys/random.h>
#  endif // HAVE_SYS_RANDOM_H
#  ifdef HAVE_GNUTLS_RND
#    include <gnutls/gnutls.h>
#    include <gnutls/crypto.h>
#  endif // HAVE_GNUTLS_RND
#  include "base-private.h"
#  include "device.h"

//
// Types and structures...
//

struct _pappl_scanner_s			// Scanner data that get configured when a scanning job is configured
{
  pthread_rwlock_t	rwlock;			// Reader/writer lock
  pappl_system_t	*system;		// Containing system
  int			scanner_id;		// "scanner-id" value
  char			*name,			// "scanner-name" value
  *dns_sd_name,		// "scanner-dns-sd-name" value
  *location,		// "scanner-location" value
  *geo_location,		// "scanner-geo-location" value (geo: URI)
  *organization,		// "scanner-organization" value
  *org_unit;		// "scanner-organizational-unit" value
  pappl_contact_t	contact;		// "scanner-contact" value
  char			*resource;		// Resource path of scanner
  size_t		resourcelen;		// Length of resource path
  char			*uriname;		// Name for URLs
  char      *uuid;  // UUID for the scanner
  escl_sstate_t        state;			// "scanner-state" value    --> Replacement for ipp_pstate_t
  pappl_sreason_t	state_reasons;		// "scanner-state-reasons" values --> Replacement for pappl_preason_t
  time_t        state_time;		// "scanner-state-change-time" value
  bool			is_accepting,		// Are we accepting scan jobs?
  is_stopped,		// Are we stopping this scanner?
  is_deleted;		// Has this scanner been deleted?
  char			*device_id,		// "scanner-device-id" value
  *device_uri;		// Device URI
  pappl_device_t	*device;		// Current connection to device (if any)
  bool			device_in_use;		// Is the device in use?
  char			*driver_name;		// Driver name
  pappl_sc_driver_data_t driver_data;		// Driver data
  time_t		start_time;		// Startup time
  time_t		config_time;		// "scanner-config-change-time" value
  time_t		status_time;		// Last time status was updated
  pappl_job_t		*processing_job;	// Current scanning job, if any
  int			next_job_id;		// Next "job-id" value
  cups_array_t        *links;			// Web navigation links
  #  ifdef HAVE_MDNSRESPONDER
  _pappl_srv_t		dns_sd_http_ref,	// DNS-SD HTTP service
  _pappl_srv_t		dns_sd_escl_ref,	// DNS-SD eSCL service
  DNSRecordRef		dns_sd_escl_loc_ref,	// DNS-SD LOC record for ESCL service
  #  elif defined(HAVE_AVAHI)
  _pappl_srv_t		dns_sd_ref;		// DNS-SD services
  #  endif // HAVE_MDNSRESPONDER
  unsigned char		dns_sd_loc[16];		// DNS-SD LOC record data
  bool			dns_sd_collision;	// Was there a name collision?
  int			dns_sd_serial;		// DNS-SD serial number (for collisions)
};

//
// Functions...
//
extern void _papplScannerCopyStateNoLock(pappl_scanner_t *scanner, ipp_tag_t group_tag, ipp_t *ipp, pappl_client_t *client, cups_array_t *ra) _PAPPL_PRIVATE;
extern const char *_papplScannerReasonString(pappl_sreason_t reason) _PAPPL_PRIVATE;

extern void		_papplScannerDelete(pappl_scanner_t *scanner) _PAPPL_PRIVATE;
extern void   _papplScannerInitDriverData(pappl_scanner_t *scanner, pappl_sc_driver_data_t *d) _PAPPL_PRIVATE;
extern bool		_papplScannerIsAuthorized(pappl_client_t *client) _PAPPL_PRIVATE; // Implement in scan-escl.c
extern void		_papplScannerProcessESCL(pappl_client_t *client) _PAPPL_PRIVATE;// Implement in scan-escl.c
extern bool		_papplScannerRegisterDNSSDNoLock(pappl_scanner_t *scanner) _PAPPL_PRIVATE; // Implement with reference to _papplPrinterRegisterDNSSDNoLock
extern void		_papplScannerUnregisterDNSSDNoLock(pappl_scanner_t *scanner) _PAPPL_PRIVATE; // Implement with reference to _papplPrinterUnregisterDNSSDNoLock

extern const char	*_papplScannerColorModeString(pappl_sc_color_mode_t value) _PAPPL_PRIVATE;
extern pappl_sc_color_mode_t _papplScannerColorModeValue(const char *value) _PAPPL_PRIVATE;

extern const char	*_papplScannerReasonString(pappl_sreason_t value) _PAPPL_PRIVATE;
extern pappl_sreason_t	_papplScannerReasonValue(const char *value) _PAPPL_PRIVATE;

extern void		_papplScannerWebConfig(pappl_client_t *client, pappl_scanner_t *scanner) _PAPPL_PRIVATE;
extern void   _papplScannerWebConfigFinalize(pappl_scanner_t *scanner, cups_len_t num_form, cups_option_t *form) _PAPPL_PRIVATE;
extern void		_papplScannerWebDefaults(pappl_client_t *client, pappl_scanner_t *scanner) _PAPPL_PRIVATE;
extern void		_papplScannerWebDelete(pappl_client_t *client, pappl_scanner_t *scanner) _PAPPL_PRIVATE;
extern void		_papplScannerWebHome(pappl_client_t *client, pappl_scanner_t *scanner) _PAPPL_PRIVATE;
extern void		_papplSannerWebIteratorCallback(pappl_scanner_t *scanner, pappl_client_t *client) _PAPPL_PRIVATE; // Check
extern void		_papplScannerWebJobs(pappl_client_t *client, pappl_scanner_t *scanner) _PAPPL_PRIVATE; // Check
extern void		_papplScannerWebMedia(pappl_client_t *client, pappl_scanner_t *printer) _PAPPL_PRIVATE; //Check


#endif // !_PAPPL_SCANNER_PRIVATE_H_
