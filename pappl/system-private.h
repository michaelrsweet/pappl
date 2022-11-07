//
// Private system header file for the Printer Application Framework
//
// Copyright © 2019-2022 by Michael R Sweet.
// Copyright © 2010-2019 by Apple Inc.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#ifndef _PAPPL_SYSTEM_PRIVATE_H_
#  define _PAPPL_SYSTEM_PRIVATE_H_
#  include "dnssd-private.h"
#  include "subscription-private.h"
#  include "system.h"


//
// Constants...
//

#  define _PAPPL_MAX_LISTENERS	32	// Maximum number of listener sockets


//
// Types and structures...
//

typedef struct _pappl_mime_filter_s	// MIME filter
{
  const char		*src,			// Source MIME media type
			*dst;			// Destination MIME media type
  pappl_mime_filter_cb_t cb;			// Filter callback function
  void			*cbdata;		// Filter callback data
} _pappl_mime_filter_t;

typedef struct _pappl_resource_s	// Resource
{
  char			*path,			// Path
			*format,		// Content type (MIME media type)
			*filename,		// Filename
			*language;		// Language (for strings)
  time_t		last_modified;		// Last-Modified date/time
  const void		*data;			// Static data
  size_t		length;			// Length of file/data
  pappl_resource_cb_t	cb;			// Dynamic callback
  void			*cbdata;		// Callback data
} _pappl_resource_t;

struct _pappl_system_s			// System data
{
  pthread_rwlock_t	rwlock;			// Reader/writer lock
  pappl_soptions_t	options;		// Server options
  bool			is_running;		// Is the system running?
  time_t		start_time,		// Startup time
			config_time,		// Time of last config change
			clean_time,		// Next clean time
			shutdown_time;		// Shutdown requested?
  pthread_mutex_t	config_mutex;		// Mutex for configuration changes
  size_t		config_changes,		// Number of configuration changes
			save_changes;		// Number of saved changes
  char			*uuid,			// "system-uuid" value
			*name,			// "system-name" value
			*dns_sd_name,		// "system-dns-sd-name" value
			*location,		// "system-location" value
			*geo_location,		// "system-geo-location" value
			*organization,		// "system-organization" value
			*org_unit;		// "system-organizational-unit" value
  pappl_contact_t	contact;		// "system-contact-col" value
  char			*hostname;		// Published hostname
  int			port;			// Port number, if any
  char			*domain_path;		// Domain socket path, if any
  cups_len_t		num_versions;		// Number of "xxx-firmware-yyy" values
  pappl_version_t	versions[10];		// "xxx-firmware-yyy" values
  char			*footer_html;		// Footer HTML for web interface
  char			*server_header;		// Server: header value
  char			*directory;		// Spool directory
  char			*logfile;		// Log filename, if any
  int			logfd;			// Log file descriptor, if any
  pappl_loglevel_t	loglevel;		// Log level
  size_t		logmaxsize;		// Maximum log file size or `0` for none
  char			*subtypes;		// DNS-SD sub-types, if any
  bool			tls_only;		// Only support TLS?
  char			*auth_service;		// PAM authorization service, if any
  char			*admin_group;		// PAM administrative group, if any
  gid_t			admin_gid;		// PAM administrative group ID
  char			*default_print_group;	// Default PAM printing group, if any
  char			session_key[65];	// Session key
  pthread_rwlock_t	session_rwlock;		// Reader/writer lock for the session key
  time_t		session_time;		// Session key time
  cups_len_t		num_listeners;		// Number of listener sockets
  struct pollfd		listeners[_PAPPL_MAX_LISTENERS];
						// Listener sockets
  int			num_clients,		// Current number of clients
			max_clients;		// Maximum number of clients
  cups_array_t		*links;			// Web navigation links
  cups_array_t		*resources;		// Array of resources
  cups_array_t		*localizations;		// Array of localizations
  cups_array_t		*filters;		// Array of filters
  int			next_client;		// Next client number
  cups_array_t		*printers;		// Array of printers
  int			default_printer_id,	// Default printer-id
			next_printer_id;	// Next printer-id
  char			password_hash[100];	// Access password hash
  cups_len_t		num_drivers;		// Number of printer drivers
  pappl_pr_driver_t	*drivers;		// Printer drivers
  pappl_pr_autoadd_cb_t	autoadd_cb;		// Printer driver auto-add callback
  pappl_pr_create_cb_t	create_cb;		// Printer driver creation callback
  pappl_pr_driver_cb_t	driver_cb;		// Printer driver initialization callback
  void			*driver_cbdata;		// Printer driver callback data
  ipp_t			*attrs;			// Static attributes for system
  char			*auth_scheme;		// Authentication scheme
  pappl_auth_cb_t	auth_cb;		// Authentication callback
  void			*auth_cbdata;		// Authentication callback data
  pappl_mime_cb_t	mime_cb;		// MIME typing callback
  void			*mime_cbdata;		// MIME typing callback data
  pappl_ipp_op_cb_t	op_cb;			// IPP operation callback
  void			*op_cbdata;		// IPP operation callback data
  pappl_save_cb_t	save_cb;		// Save callback
  void			*save_cbdata;		// Save callback data
#  ifdef HAVE_MDNSRESPONDER
  _pappl_srv_t		dns_sd_ipps_ref,	// DNS-SD IPPS service
			dns_sd_http_ref;	// DNS-SD HTTP service
  DNSRecordRef		dns_sd_loc_ref;		// DNS-SD LOC record
#  else
  _pappl_srv_t		dns_sd_ref;		// DNS-SD services
#  endif // HAVE_MDNSRESPONDER
  unsigned char		dns_sd_loc[16];		// DNS-SD LOC record data
  bool			dns_sd_any_collision;	// Was there a name collision for any printer?
  bool			dns_sd_collision;	// Was there a name collision for this system?
  int			dns_sd_serial;		// DNS-SD serial number (for collisions)
  int			dns_sd_host_changes;	// Last count of DNS-SD host name changes
  pappl_network_get_cb_t network_get_cb;	// Get networks callback
  pappl_network_set_cb_t network_set_cb;	// Set networks callback
  void			*network_cbdata;	// Network callback data
  pappl_wifi_join_cb_t	wifi_join_cb;		// Wi-Fi join callback
  pappl_wifi_list_cb_t	wifi_list_cb;		// Wi-Fi list callback
  pappl_wifi_status_cb_t wifi_status_cb;	// Wi-Fi status callback
  void			*wifi_cbdata;		// Wi-Fi callback data

  pappl_event_cb_t	event_cb;		// Event callback
  void			*event_data;		// Event callback data
  pappl_event_cb_t	systemui_cb;		// System UI event callback
  void			*systemui_data;		// System UI event callback data
  size_t		max_subscriptions;	// Maximum number of subscriptions
  cups_array_t		*subscriptions;		// Subscription array
  int			next_subscription_id;	// Next "notify-subscription-id" value
  pthread_cond_t	subscription_cond;	// Subscription condition variable
  pthread_mutex_t	subscription_mutex;	// Subscription mutex
  cups_array_t		*timers;		// Timers array
  size_t		max_image_size;		// Maximum image file size (uncompressed)
  int			max_image_width,	// Maximum image file width
			max_image_height;	// Maximum image file height
};

typedef struct _pappl_timer_s			// Timer callback data
{
  pappl_timer_cb_t	cb;			// Callback function
  void			*cb_data;		// Callback data
  time_t		next;			// Next time
  int			interval;		// Timer interval
} _pappl_timer_t;


//
// Functions...
//

extern void		_papplSystemAddEventNoLock(pappl_system_t *system, pappl_printer_t *printer, pappl_job_t *job, pappl_event_t event, const char *message, ...) _PAPPL_FORMAT(5, 6) _PAPPL_PRIVATE;
extern void		_papplSystemAddEventNoLockv(pappl_system_t *system, pappl_printer_t *printer, pappl_job_t *job, pappl_event_t event, const char *message, va_list ap) _PAPPL_PRIVATE;
extern void		_papplSystemAddLoc(pappl_system_t *system, pappl_loc_t *loc) _PAPPL_PRIVATE;
extern void		_papplSystemAddPrinter(pappl_system_t *system, pappl_printer_t *printer, int printer_id) _PAPPL_PRIVATE;
extern void		_papplSystemAddPrinterIcons(pappl_system_t *system, pappl_printer_t *printer) _PAPPL_PRIVATE;
extern bool		_papplSystemAddSubscription(pappl_system_t *system, pappl_subscription_t *sub, int sub_id) _PAPPL_PRIVATE;
extern void		_papplSystemCleanJobs(pappl_system_t *system) _PAPPL_PRIVATE;
extern void		_papplSystemCleanSubscriptions(pappl_system_t *system, bool clean_all) _PAPPL_PRIVATE;
extern void		_papplSystemConfigChanged(pappl_system_t *system) _PAPPL_PRIVATE;
extern void		_papplSystemExportVersions(pappl_system_t *system, ipp_t *ipp, ipp_tag_t group_tag, cups_array_t *ra);
extern _pappl_mime_filter_t *_papplSystemFindMIMEFilter(pappl_system_t *system, const char *srctype, const char *dsttype) _PAPPL_PRIVATE;
extern _pappl_resource_t *_papplSystemFindResourceForLanguage(pappl_system_t *system, const char *language) _PAPPL_PRIVATE;
extern _pappl_resource_t *_papplSystemFindResourceForPath(pappl_system_t *system, const char *path) _PAPPL_PRIVATE;
extern char		*_papplSystemMakeUUID(pappl_system_t *system, const char *printer_name, int job_id, char *buffer, size_t bufsize) _PAPPL_PRIVATE;
extern void		_papplSystemNeedClean(pappl_system_t *system) _PAPPL_PRIVATE;
extern void		_papplSystemProcessIPP(pappl_client_t *client) _PAPPL_PRIVATE;
extern bool		_papplSystemRegisterDNSSDNoLock(pappl_system_t *system) _PAPPL_PRIVATE;
extern void		_papplSystemSetHostNameNoLock(pappl_system_t *system, const char *value) _PAPPL_PRIVATE;
extern void		_papplSystemStatusUI(pappl_system_t *system) _PAPPL_PRIVATE;
extern void		_papplSystemUnregisterDNSSDNoLock(pappl_system_t *system) _PAPPL_PRIVATE;

extern void		_papplSystemWebAddPrinter(pappl_client_t *client, pappl_system_t *system) _PAPPL_PRIVATE;
extern void		_papplSystemWebConfig(pappl_client_t *client, pappl_system_t *system) _PAPPL_PRIVATE;
extern void		_papplSystemWebConfigFinalize(pappl_system_t *system, cups_len_t num_form, cups_option_t *form) _PAPPL_PRIVATE;
extern void		_papplSystemWebHome(pappl_client_t *client, pappl_system_t *system) _PAPPL_PRIVATE;
extern void		_papplSystemWebLogFile(pappl_client_t *client, pappl_system_t *system) _PAPPL_PRIVATE;
extern void		_papplSystemWebLogs(pappl_client_t *client, pappl_system_t *system) _PAPPL_PRIVATE;
extern void		_papplSystemWebNetwork(pappl_client_t *client, pappl_system_t *system) _PAPPL_PRIVATE;
extern void		_papplSystemWebSecurity(pappl_client_t *client, pappl_system_t *system) _PAPPL_PRIVATE;
extern void		_papplSystemWebSettings(pappl_client_t *client) _PAPPL_PRIVATE;
#  if defined(HAVE_OPENSSL) || defined(HAVE_GNUTLS)
extern void		_papplSystemWebTLSInstall(pappl_client_t *client, pappl_system_t *system) _PAPPL_PRIVATE;
extern void		_papplSystemWebTLSNew(pappl_client_t *client, pappl_system_t *system) _PAPPL_PRIVATE;
#  endif // HAVE_OPENSSL || HAVE_GNUTLS
extern void		_papplSystemWebWiFi(pappl_client_t *client, pappl_system_t *system) _PAPPL_PRIVATE;

#endif // !_PAPPL_SYSTEM_PRIVATE_H_
