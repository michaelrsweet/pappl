//
// Private printer header file for the Printer Application Framework
//
// Copyright © 2019-2022 by Michael R Sweet.
// Copyright © 2010-2019 by Apple Inc.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#ifndef _PAPPL_PRINTER_PRIVATE_H_
#  define _PAPPL_PRINTER_PRIVATE_H_
#  include "dnssd-private.h"
#  include "printer.h"
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
			*org_unit;		// "printer-organizational-unit" value
  pappl_contact_t	contact;		// "printer-contact" value
  pappl_contact_t	service_contact;		// "printer-service-contact-col" value
  char			*resource;		// Resource path of printer
  size_t		resourcelen;		// Length of resource path
  char			*uriname;		// Name for URLs
  ipp_pstate_t		state;			// "printer-state" value
  pappl_preason_t	state_reasons;		// "printer-state-reasons" values
  time_t		state_time;		// "printer-state-change-time" value
  bool			is_accepting,		// Are we accepting jobs?
			is_stopped,		// Are we stopping this printer?
			is_deleted;		// Has this printer been deleted?
  char			*device_id,		// "printer-device-id" value
			*device_uri;		// Device URI
  pappl_device_t	*device;		// Current connection to device (if any)
  bool			device_in_use;		// Is the device in use?
  char			*driver_name;		// Driver name
  pappl_pr_driver_data_t driver_data;		// Driver data
  ipp_t			*driver_attrs;		// Driver attributes
  int			num_ready;		// Number of ready media
  ipp_t			*attrs;			// Other (static) printer attributes
  time_t		start_time;		// Startup time
  time_t		config_time;		// "printer-config-change-time" value
  time_t		status_time;		// Last time status was updated
  char			*print_group;		// PAM printing group, if any
  gid_t			print_gid;		// PAM printing group ID
  int			num_supply;		// Number of "printer-supply" values
  pappl_supply_t	supply[PAPPL_MAX_SUPPLY];
						// "printer-supply" values
  pappl_job_t		*processing_job;	// Currently printing job, if any
  bool			hold_new_jobs;		// Hold new jobs
  int			max_active_jobs,	// Maximum number of active jobs to accept
			max_completed_jobs,	// Maximum number of completed jobs to retain in history
			max_preserved_jobs;	// Maximum number of completed jobs to preserve in history
  cups_array_t		*active_jobs,		// Array of active jobs
			*all_jobs,		// Array of all jobs
			*completed_jobs;	// Array of completed jobs
  int			next_job_id,		// Next "job-id" value
			impcompleted;		// "printer-impressions-completed" value
  cups_array_t		*links;			// Web navigation links
#  ifdef HAVE_MDNSRESPONDER
  _pappl_srv_t		dns_sd_ipp_ref,		// DNS-SD IPP service
			dns_sd_ipps_ref,	// DNS-SD IPPS service
			dns_sd_http_ref,	// DNS-SD HTTP service
			dns_sd_printer_ref,	// DNS-SD LPD service
			dns_sd_pdl_ref;		// DNS-SD AppSocket service
  DNSRecordRef		dns_sd_ipp_loc_ref,	// DNS-SD LOC record for IPP service
			dns_sd_ipps_loc_ref;	// DNS-SD LOC record for IPPS service
#  elif defined(HAVE_AVAHI)
  _pappl_srv_t		dns_sd_ref;		// DNS-SD services
#  endif // HAVE_MDNSRESPONDER
  unsigned char		dns_sd_loc[16];		// DNS-SD LOC record data
  bool			dns_sd_collision;	// Was there a name collision?
  int			dns_sd_serial;		// DNS-SD serial number (for collisions)
  bool			raw_active;		// Raw listener active?
  int			num_raw_listeners;	// Number of raw socket listeners
  struct pollfd		raw_listeners[2];	// Raw socket listeners
  bool			usb_active;		// USB gadget active?
  unsigned short	usb_vendor_id,		// USB vendor ID
			usb_product_id;		// USB product ID
  pappl_uoptions_t	usb_options;		// USB gadget options
  char			*usb_storage;		// USB storage gadget file, if any
  pappl_pr_usb_cb_t	usb_cb;			// USB processing callback, if any
  void			*usb_cbdata;		// USB processing callback data, if any
  time_t                          cancel_after_time;              // "job-cancel-after-default" as time_t
  pappl_pw_repertoire_t           pw_repertoire_configured;       // "job-password-repertoire-configured" value as enum
  pappl_release_action_t          release_action_default;         // "job-release-action-default" as enum
  pappl_st_access_t               st_access_supported;            // "job-storage-access-supported" as enum
  pappl_st_disposition_t          st_disposition_supported;       // "job-storage-disposition-supported" as enum
  bool                            st_group_supported;             // Indicates support for "job-storage-group"
  pappl_which_jobs_t              which_jobs_supported;           // "which-jobs-supported" as enum
};


extern const char * const _pappl_color_modes[6];
extern const char * const _pappl_contents[5];
extern const char * const _pappl_identify_actions[4];
extern const char * const _pappl_job_password_repertoires[8];
extern const char * const _pappl_kinds[9];
extern const char * const _pappl_label_modes[9];
extern const char * const _pappl_marker_colors[13];
extern const char * const _pappl_marker_types[34];
extern const char * const _pappl_media_trackings[4];
extern const char * const _pappl_preasons[17];
extern const char * const _pappl_raster_types[13];
extern const char * const _pappl_release_actions[4];
extern const char * const _pappl_scalings[5];
extern const char * const _pappl_sides[3];
extern const char * const _pappl_st_access[3];
extern const char * const _pappl_st_disposition[2];
extern const char * const _pappl_supply_colors[13];
extern const char * const _pappl_supply_types[34];
extern const char * const _pappl_which_jobs[16];

//
// Functions...
//

extern bool		_papplPrinterAddRawListeners(pappl_printer_t *printer) _PAPPL_PRIVATE;
extern void		*_papplPrinterRunRaw(pappl_printer_t *printer) _PAPPL_PRIVATE;

extern void		*_papplPrinterRunUSB(pappl_printer_t *printer) _PAPPL_PRIVATE;

extern void		_papplPrinterCheckJobs(pappl_printer_t *printer) _PAPPL_PRIVATE;
extern void		_papplPrinterCleanJobsNoLock(pappl_printer_t *printer) _PAPPL_PRIVATE;
extern void		_papplPrinterCopyAttributesNoLock(pappl_printer_t *printer, pappl_client_t *client, cups_array_t *ra, const char *format) _PAPPL_PRIVATE;
extern void		_papplPrinterCopyStateNoLock(pappl_printer_t *printer, ipp_tag_t group_tag, ipp_t *ipp, pappl_client_t *client, cups_array_t *ra) _PAPPL_PRIVATE;
extern void		_papplPrinterCopyXRINoLock(pappl_printer_t *printer, ipp_t *ipp, pappl_client_t *client) _PAPPL_PRIVATE;
extern void		_papplPrinterDelete(pappl_printer_t *printer) _PAPPL_PRIVATE;
extern void		_papplPrinterInitDriverData(pappl_pr_driver_data_t *d) _PAPPL_PRIVATE;
extern bool		_papplPrinterIsAuthorized(pappl_client_t *client) _PAPPL_PRIVATE;
extern void		_papplPrinterProcessIPP(pappl_client_t *client) _PAPPL_PRIVATE;
extern bool		_papplPrinterRegisterDNSSDNoLock(pappl_printer_t *printer) _PAPPL_PRIVATE;
extern bool		_papplPrinterSetAttributes(pappl_client_t *client, pappl_printer_t *printer) _PAPPL_PRIVATE;
extern void		_papplPrinterUnregisterDNSSDNoLock(pappl_printer_t *printer) _PAPPL_PRIVATE;

extern void		_papplPrinterWebCancelAllJobs(pappl_client_t *client, pappl_printer_t *printer) _PAPPL_PRIVATE;
extern void		_papplPrinterWebConfig(pappl_client_t *client, pappl_printer_t *printer) _PAPPL_PRIVATE;
extern void		_papplPrinterWebConfigFinalize(pappl_printer_t *printer, cups_len_t num_form, cups_option_t *form) _PAPPL_PRIVATE;
extern void		_papplPrinterWebDefaults(pappl_client_t *client, pappl_printer_t *printer) _PAPPL_PRIVATE;
extern void		_papplPrinterWebDelete(pappl_client_t *client, pappl_printer_t *printer) _PAPPL_PRIVATE;
extern void		_papplPrinterWebHome(pappl_client_t *client, pappl_printer_t *printer) _PAPPL_PRIVATE;
extern void		_papplPrinterWebIteratorCallback(pappl_printer_t *printer, pappl_client_t *client) _PAPPL_PRIVATE;
extern void		_papplPrinterWebJobs(pappl_client_t *client, pappl_printer_t *printer) _PAPPL_PRIVATE;
extern void		_papplPrinterWebMedia(pappl_client_t *client, pappl_printer_t *printer) _PAPPL_PRIVATE;
extern void		_papplPrinterWebSupplies(pappl_client_t *client, pappl_printer_t *printer) _PAPPL_PRIVATE;

extern const char	*_papplColorModeString(pappl_color_mode_t value) _PAPPL_PRIVATE;
extern pappl_color_mode_t _papplColorModeValue(const char *value) _PAPPL_PRIVATE;

extern const char	*_papplContentString(pappl_content_t value) _PAPPL_PRIVATE;
extern pappl_content_t	_papplContentValue(const char *value) _PAPPL_PRIVATE;

extern ipp_t		*_papplCreateMediaSize(const char *size_name) _PAPPL_PRIVATE;

extern const char	*_papplIdentifyActionsString(pappl_identify_actions_t v) _PAPPL_PRIVATE;
extern pappl_identify_actions_t _papplIdentifyActionsValue(const char *s) _PAPPL_PRIVATE;

extern const char	*_papplKindString(pappl_kind_t v) _PAPPL_PRIVATE;

extern const char	*_papplLabelModeString(pappl_label_mode_t v) _PAPPL_PRIVATE;
extern pappl_label_mode_t _papplLabelModeValue(const char *s) _PAPPL_PRIVATE;

extern const char	*_papplMarkerColorString(pappl_supply_color_t v) _PAPPL_PRIVATE;
extern const char	*_papplMarkerTypeString(pappl_supply_type_t v) _PAPPL_PRIVATE;
extern ipp_t		*_papplMediaColExport(pappl_pr_driver_data_t *driver_data, pappl_media_col_t *media, bool db) _PAPPL_PRIVATE;
extern void		_papplMediaColImport(ipp_t *col, pappl_media_col_t *media) _PAPPL_PRIVATE;

extern const char	*_papplMediaTrackingString(pappl_media_tracking_t v);
extern pappl_media_tracking_t _papplMediaTrackingValue(const char *s);

extern const char  *_papplPasswordRepertoireString(pappl_pw_repertoire_t value) _PAPPL_PRIVATE;
extern const char	*_papplPrinterReasonString(pappl_preason_t value) _PAPPL_PRIVATE;
extern pappl_preason_t	_papplPrinterReasonValue(const char *value) _PAPPL_PRIVATE;

extern const char	*_papplRasterTypeString(pappl_raster_type_t value) _PAPPL_PRIVATE;
extern const char  *_papplReleaseActionString(pappl_release_action_t value) _PAPPL_PRIVATE;

extern const char	*_papplScalingString(pappl_scaling_t value) _PAPPL_PRIVATE;
extern pappl_scaling_t	_papplScalingValue(const char *value) _PAPPL_PRIVATE;

extern const char	*_papplSidesString(pappl_sides_t value) _PAPPL_PRIVATE;
extern pappl_sides_t	_papplSidesValue(const char *value) _PAPPL_PRIVATE;

extern const char *_papplStorageAccessString(pappl_st_access_t value) _PAPPL_PRIVATE;
extern pappl_st_access_t  _papplStorageAccessValue(const char *value) _PAPPL_PRIVATE;

extern const char *_papplStorageDispositionString(pappl_st_disposition_t value) _PAPPL_PRIVATE;
extern pappl_st_disposition_t  _papplStorageDispositionValue(const char *value) _PAPPL_PRIVATE;

extern const char	*_papplSupplyColorString(pappl_supply_color_t value) _PAPPL_PRIVATE;
extern pappl_supply_color_t _papplSupplyColorValue(const char *value) _PAPPL_PRIVATE;
extern const char	*_papplSupplyTypeString(pappl_supply_type_t value) _PAPPL_PRIVATE;
extern pappl_supply_type_t _papplSupplyTypeValue(const char *value) _PAPPL_PRIVATE;


#endif // !_PAPPL_PRINTER_PRIVATE_H_
