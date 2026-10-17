//
// Private scanner header file for the Printer Application Framework
//
// Copyright © 2019-2026 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#ifndef _PAPPL_SCANNER_PRIVATE_H_
#  define _PAPPL_SCANNER_PRIVATE_H_
#  include "base-private.h"
#  include "scanner.h"
#  include "log.h"


//
// Types and structures...
//

struct _pappl_scanner_s			// Scanner data
{
  cups_rwlock_t		rwlock;		// Reader/writer lock
  pappl_system_t	*system;	// Containing system
  int			scanner_id;	// scanner-id value (own ID space)
  char			*name,		// "scanner-name" value
			*dns_sd_name,	// "scanner-dns-sd-name" value
			*location,	// "scanner-location" value
			*geo_location,	// "scanner-geo-location" value (geo: URI)
			*organization,	// "scanner-organization" value
			*org_unit;	// "scanner-organizational-unit" value
  pappl_contact_t	contact;	// "scanner-contact" value
  char			*resource;	// Resource path of scanner
  size_t		resourcelen;	// Length of resource path
  char			*uriname;	// Name for URLs
  ipp_pstate_t		state;		// "scanner-state" value
  pappl_preason_t	state_reasons;	// "scanner-state-reasons" values
  time_t		state_time;	// "scanner-state-change-time" value
  bool			is_deleted;	// Has this scanner been deleted?
  char			*device_id,	// "scanner-device-id" value
			*device_uri;	// Device URI
  pappl_device_t	*device;	// Current connection to device (if any)
  bool			device_in_use;	// Is the device in use?
  char			*driver_name;	// Driver name
  pappl_sc_driver_data_t driver_data;	// Driver data
  ipp_t			*driver_attrs;	// Driver attributes
  ipp_t			*attrs;		// Other (static) scanner attributes
  time_t		start_time;	// Startup time
  time_t		config_time;	// "scanner-config-change-time" value
  time_t		status_time;	// Last time status was updated
  char			*scan_group;	// PAM scanning group, if any
  gid_t			scan_gid;	// PAM scanning group ID
  pappl_job_t		*processing_job;// Currently scanning job, if any
  size_t		max_active_jobs,// Maximum number of active jobs
			max_completed_jobs;
					// Maximum number of completed jobs
  cups_array_t		*active_jobs,	// Array of active jobs
			*all_jobs,	// Array of all jobs
			*completed_jobs;// Array of completed jobs
  int			next_job_id;	// Next "job-id" value
  int			impcompleted;	// "scanner-impressions-completed" value
  cups_array_t		*links;		// Web navigation links
  cups_dnssd_service_t	*dns_sd_services;
					// DNS-SD services
  bool			dns_sd_collision;
					// Was there a name collision?
  int			dns_sd_serial;	// DNS-SD serial number (for collisions)
  char			*uuid;		// "scanner-uuid" value
};


//
// Functions...
//

extern void		_papplScannerCheckJobsNoLock(pappl_scanner_t *scanner) _PAPPL_PRIVATE;
extern void		_papplScannerCopyAttributesNoLock(pappl_scanner_t *scanner, ipp_t *ipp, cups_array_t *ra) _PAPPL_PRIVATE;
extern void		_papplScannerCopyStateNoLock(pappl_scanner_t *scanner, ipp_tag_t group_tag, ipp_t *ipp, cups_array_t *ra) _PAPPL_PRIVATE;
extern void		_papplScannerDelete(pappl_scanner_t *scanner) _PAPPL_PRIVATE;
extern void		_papplScannerInitDriverData(pappl_sc_driver_data_t *d) _PAPPL_PRIVATE;
extern void		_papplScannerProcessESCL(pappl_client_t *client) _PAPPL_PRIVATE;
extern void		_papplScannerProcessIPP(pappl_client_t *client) _PAPPL_PRIVATE;
extern bool		_papplScannerRegisterDNSSDNoLock(pappl_scanner_t *scanner) _PAPPL_PRIVATE;
extern void		_papplScannerUnregisterDNSSDNoLock(pappl_scanner_t *scanner) _PAPPL_PRIVATE;
extern void		_papplScannerWebCancelAllJobs(pappl_client_t *client, pappl_scanner_t *scanner) _PAPPL_PRIVATE;
extern void		_papplScannerWebConfig(pappl_client_t *client, pappl_scanner_t *scanner) _PAPPL_PRIVATE;
extern void		_papplScannerWebConfigFinalize(pappl_scanner_t *scanner, size_t num_form, cups_option_t *form) _PAPPL_PRIVATE;
extern void		_papplScannerWebDefaults(pappl_client_t *client, pappl_scanner_t *scanner) _PAPPL_PRIVATE;
extern void		_papplScannerWebDelete(pappl_client_t *client, pappl_scanner_t *scanner) _PAPPL_PRIVATE;
extern void		_papplScannerWebFooter(pappl_client_t *client) _PAPPL_PRIVATE;
extern void		_papplScannerWebHeader(pappl_client_t *client, pappl_scanner_t *scanner, const char *title, int refresh, const char *label, const char *path_or_url) _PAPPL_PRIVATE;
extern void		_papplScannerWebHome(pappl_client_t *client, pappl_scanner_t *scanner) _PAPPL_PRIVATE;
extern void		_papplScannerWebIteratorCallback(pappl_scanner_t *scanner, pappl_client_t *client) _PAPPL_PRIVATE;
extern void		_papplScannerWebJobs(pappl_client_t *client, pappl_scanner_t *scanner) _PAPPL_PRIVATE;


#endif // !_PAPPL_SCANNER_PRIVATE_H_
