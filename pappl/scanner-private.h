//
// Private scanner header file for the Scanner Application Framework
//
// Copyright © 2019-2021 by Michael R Sweet.
// Copyright © 2010-2019 by Apple Inc.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#ifndef _PAPPL_SCANNER_PRIVATE_H_
#  define _PAPPL_SCANNER_PRIVATE_H_

//
// Include necessary headers...
//

#  include "dnssd-private.h"
#  include "scanner.h"
#  include "printer.h"
#  include "log.h"
#  include <grp.h>
#  ifdef __APPLE__
#    include <sys/param.h>
#    include <sys/mount.h>
#  else
#    include <sys/statfs.h>
#  endif // __APPLE__
#  ifdef HAVE_SYS_RANDOM_H
#    include <sys/random.h>
#  endif // HAVE_SYS_RANDOM_H
#  ifdef HAVE_GNUTLS_RND
#    include <gnutls/gnutls.h>
#    include <gnutls/crypto.h>
#  endif // HAVE_GNUTLS_RND


//
// Include necessary headers...
//

#  include "base-private.h"
#  include "base.h"
#  include "device.h"


//
// Types and structures...
//

struct _pappl_scanner_s			// Scanner data
{
  pthread_rwlock_t	rwlock;			// Reader/writer lock
  pappl_system_t	*system;		// Containing system
  int			printer_id;		// "printer-id" value
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
  ipp_pstate_t		state;			// "scanner-state" value
  pappl_preason_t	state_reasons;		// "scanner-state-reasons" values
  time_t		state_time;		// "scanner-state-change-time" value
  bool			is_stopped,		// Are we stopping this scanner?
			is_deleted;		// Has this scanner been deleted?
  char			*device_id,		// "scanner-device-id" value
			*device_uri;		// Device URI
  pappl_device_t	*device;		// Current connection to device (if any)
  bool			device_in_use;		// Is the device in use?
  char			*driver_name;		// Driver name
  pappl_sc_driver_data_t driver_data;	// Driver data
  ipp_t			*driver_attrs;		// Driver attributes
  ipp_t			*attrs;			// Other (static) scanner attributes
  time_t		start_time;		// Startup time
  time_t		config_time;		// "scanner-config-change-time" value
  time_t		status_time;		// Last time status was updated
  char			*scan_group;		// PAM scanning group, if any
  gid_t			scan_gid;		// PAM scanning group ID
  pappl_job_t		*processing_job;	// Currently scanning job, if any
  int			max_active_jobs,	// Maximum number of active jobs to accept
			max_completed_jobs;	// Maximum number of completed jobs to retain in history
  cups_array_t		*active_jobs,		// Array of active jobs
			*all_jobs,		// Array of all jobs
			*completed_jobs;	// Array of completed jobs
  int			next_job_id,		// Next "job-id" value
			impcompleted;		// "scanner-impressions-completed" value
  cups_array_t		*links;			// Web navigation links
#  ifdef HAVE_MDNSRESPONDER
  _pappl_srv_t		dns_sd_ipp_ref,		// DNS-SD IPP service
			dns_sd_ipps_ref,	// DNS-SD IPPS service
			dns_sd_http_ref,	// DNS-SD HTTP service
  DNSRecordRef		dns_sd_ipp_loc_ref,	// DNS-SD LOC record for IPP service
			dns_sd_ipps_loc_ref;	// DNS-SD LOC record for IPPS service
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

extern void		_papplScannerCheckJobs(pappl_scanner_t *scanner) _PAPPL_PRIVATE;
extern void		_papplScannerCleanJobs(pappl_scanner_t *scanner) _PAPPL_PRIVATE;
extern void		_papplScannerCopyAttributes(pappl_client_t *client, pappl_scanner_t *scanner, cups_array_t *ra, const char *format) _PAPPL_PRIVATE;
extern void		_papplScannerCopyState(pappl_client_t *client, ipp_t *ipp, pappl_scanner_t *scanner, cups_array_t *ra) _PAPPL_PRIVATE;
extern void		_papplScannerCopyXRI(pappl_client_t *client, ipp_t *ipp, pappl_scanner_t *scanner) _PAPPL_PRIVATE;
extern void		_papplScannerDelete(pappl_scanner_t *scanner) _PAPPL_PRIVATE;
extern void		_papplScannerInitDriverData(pappl_sc_driver_data_t *d) _PAPPL_PRIVATE;
extern void		_papplScannerProcessIPP(pappl_client_t *client) _PAPPL_PRIVATE;
extern bool		_papplScannerRegisterDNSSDNoLock(pappl_scanner_t *scanner) _PAPPL_PRIVATE;
extern bool		_papplScannerSetAttributes(pappl_client_t *client, pappl_scanner_t *scanner) _PAPPL_PRIVATE;
extern void		_papplScannerUnregisterDNSSDNoLock(pappl_scanner_t *scanner) _PAPPL_PRIVATE;

extern void		_papplScannerWebCancelAllJobs(pappl_client_t *client, pappl_scanner_t *scanner) _PAPPL_PRIVATE;
extern void		_papplScannerWebCancelJob(pappl_client_t *client, pappl_scanner_t *scanner) _PAPPL_PRIVATE;
extern void		_papplScannerWebConfig(pappl_client_t *client, pappl_scanner_t *scanner) _PAPPL_PRIVATE;
extern void		_papplScannerWebConfigFinalize(pappl_scanner_t *scanner, int num_form, cups_option_t *form) _PAPPL_PRIVATE;
extern void		_papplScannerWebDefaults(pappl_client_t *client, pappl_scanner_t *scanner) _PAPPL_PRIVATE;
extern void		_papplScannerWebDelete(pappl_client_t *client, pappl_scanner_t *scanner) _PAPPL_PRIVATE;
extern void		_papplScannerWebHome(pappl_client_t *client, pappl_scanner_t *scanner) _PAPPL_PRIVATE;
extern void		_papplScannerWebIteratorCallback(pappl_scanner_t *scanner, pappl_client_t *client) _PAPPL_PRIVATE;
extern void		_papplScannerWebJobs(pappl_client_t *client, pappl_scanner_t *scanner) _PAPPL_PRIVATE;
extern void		_papplScannerWebMedia(pappl_client_t *client, pappl_scanner_t *scanner) _PAPPL_PRIVATE;
extern ipp_t		*_papplCreateMediaSize(const char *size_name) _PAPPL_PRIVATE;

extern const char	*_papplLabelModeString(pappl_label_mode_t v) _PAPPL_PRIVATE;
extern pappl_label_mode_t _papplLabelModeValue(const char *s) _PAPPL_PRIVATE;

extern void		_papplMediaColImport(ipp_t *col, pappl_media_col_t *media) _PAPPL_PRIVATE;

extern const char	*_papplMediaTrackingString(pappl_media_tracking_t v);
extern pappl_media_tracking_t _papplMediaTrackingValue(const char *s);

extern const char	*_papplScannerReasonString(pappl_preason_t value) _PAPPL_PRIVATE;
extern pappl_preason_t	_papplScannerReasonValue(const char *value) _PAPPL_PRIVATE;

extern const char	*_papplRasterTypeString(pappl_raster_type_t value) _PAPPL_PRIVATE;

extern const char	*_papplScalingString(pappl_scaling_t value) _PAPPL_PRIVATE;
extern pappl_scaling_t	_papplScalingValue(const char *value) _PAPPL_PRIVATE;

extern const char	*_papplSidesString(pappl_sides_t value) _PAPPL_PRIVATE;
extern pappl_sides_t	_papplSidesValue(const char *value) _PAPPL_PRIVATE;


#endif // !_PAPPL_SCANNER_PRIVATE_H_
