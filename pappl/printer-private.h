//
// Private printer header file for the Printer Application Framework
//
// Copyright © 2019-2020 by Michael R Sweet.
// Copyright © 2010-2019 by Apple Inc.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#ifndef _PAPPL_PRINTER_PRIVATE_H_
#  define _PAPPL_PRINTER_PRIVATE_H_

//
// Include necessary headers...
//

#  include "base-private.h"
#  include "printer.h"
#  include "log.h"
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
#    include <gnutls/crypto.h>
#  endif // HAVE_GNUTLS_RND


//
// Include necessary headers...
//

#  include "base-private.h"
#  include "device.h"


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
  cups_array_t		*ra;		// Requested attributes
  ipp_tag_t		group_tag;	// Group to copy
} _pappl_filter_t;

struct _pappl_printer_s			// Printer data
{
  pthread_rwlock_t	rwlock;			// Reader/writer lock
  pappl_system_t	*system;		// Containing system
  int			printer_id;		// "printer-id" value
  char			*name,			// "printer-name" value
			*dns_sd_name,		// "printer-dns-sd-name" value
			*location,		// "printer-location" value
			*geo_location,		// "printer-geo-location" value (geo: URI)
			*organization,		// "printer-organization" value
			*org_unit,		// "printer-organizational-unit" value
			*resource;		// Resource path of printer
  size_t		resourcelen;		// Length of resource path
  ipp_pstate_t		state;			// "printer-state" value
  pappl_preason_t	state_reasons;		// "printer-state-reasons" values
  time_t		state_time;		// "printer-state-change-time" value
  bool			is_deleted;		// Has this printer been deleted?
  char			*device_uri;		// Device URI
  pappl_device_t	*device;		// Current connection to device (if any)
  char			*driver_name;		// Driver name
  pappl_driver_data_t	driver_data;		// Driver data
  ipp_t			*driver_attrs;		// Driver attributes
  ipp_t			*attrs;			// Other (static) printer attributes
  ipp_attribute_t	*xri_supported;		// "printer-xri-supported" attribute
  time_t		start_time;		// Startup time
  time_t		config_time;		// "printer-config-change-time" value
  time_t		status_time;		// Last time status was updated
  int			num_supply;		// Number of "printer-supply" values
  pappl_supply_t	supply[PAPPL_MAX_SUPPLY];
						// "printer-supply" values
  pappl_job_t		*processing_job;	// Currently printing job, if any
  int			max_active_jobs;	// Maximum number of active jobs to accept
  cups_array_t		*active_jobs,		// Array of active jobs
			*all_jobs,		// Array of all jobs
			*completed_jobs;	// Array of completed jobs
  int			next_job_id,		// Next "job-id" value
			impcompleted;		// "printer-impressions-completed" value
#  ifdef HAVE_DNSSD
  _pappl_srv_t		ipp_ref,		// DNS-SD IPP service
			ipps_ref,		// DNS-SD IPPS service
			http_ref,		// DNS-SD HTTP service
			printer_ref,		// DNS-SD LPD service
			loc_ref;		// DNS-SD LOC record
#  elif defined(HAVE_AVAHI)
  _pappl_srv_t		dns_sd_ref;		// DNS-SD services
#  endif // HAVE_DNSSD
  bool			dns_sd_collision;	// Was there a name collision?
};


//
// Functions...
//

extern void		_papplPrinterCheckJobs(pappl_printer_t *printer) _PAPPL_PRIVATE;
extern void		_papplPrinterCleanJobs(pappl_printer_t *printer) _PAPPL_PRIVATE;
extern int		_papplPrinterCompare(pappl_printer_t *a, pappl_printer_t *b) _PAPPL_PRIVATE;
extern int		_papplPrinterRegisterDNSSD(pappl_printer_t *printer) _PAPPL_PRIVATE;
extern void		_papplPrinterUnregisterDNSSD(pappl_printer_t *printer) _PAPPL_PRIVATE;

extern const char	*_papplColorModeString(pappl_color_mode_t value) _PAPPL_PRIVATE;
extern pappl_color_mode_t _papplColorModeValue(const char *value) _PAPPL_PRIVATE;

extern ipp_t		*_papplCreateMediaSize(const char *size_name) _PAPPL_PRIVATE;

extern const char	*_papplLabelModeString(pappl_label_mode_t v) _PAPPL_PRIVATE;
extern pappl_label_mode_t _papplLabelModeValue(const char *s) _PAPPL_PRIVATE;

extern const char	*_papplMarkerColorString(pappl_supply_color_t v) _PAPPL_PRIVATE;
extern const char	*_papplMarkerTypeString(pappl_supply_type_t v) _PAPPL_PRIVATE;
extern ipp_t		*_papplMediaColExport(pappl_media_col_t *media, bool db) _PAPPL_PRIVATE;
extern void		_papplMediaColImport(ipp_t *col, pappl_media_col_t *media) _PAPPL_PRIVATE;

extern const char	*_papplMediaTrackingString(pappl_media_tracking_t v);
extern pappl_media_tracking_t _papplMediaTrackingValue(const char *s);

extern const char	*_papplPrinterReasonString(pappl_preason_t value) _PAPPL_PRIVATE;
extern pappl_preason_t	_papplPrinterReasonValue(const char *value) _PAPPL_PRIVATE;

extern const char	*_papplRasterTypeString(pappl_raster_type_t value) _PAPPL_PRIVATE;
extern const char	*_papplSupplyColorString(pappl_supply_color_t value) _PAPPL_PRIVATE;
extern const char	*_papplSupplyTypeString(pappl_supply_type_t value) _PAPPL_PRIVATE;


#endif // !_PAPPL_PRINTER_PRIVATE_H_
