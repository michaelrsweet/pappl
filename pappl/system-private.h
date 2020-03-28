//
// Private system header file for the Printer Application Framework
//
// Copyright © 2019-2020 by Michael R Sweet.
// Copyright © 2010-2019 by Apple Inc.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#ifndef _PAPPL_SYSTEM_PRIVATE_H_
#  define _PAPPL_SYSTEM_PRIVATE_H_

//
// Include necessary headers...
//

#  include "base-private.h"
#  include "system.h"
#  include <grp.h>


//
// Constants...
//

#  define _PAPPL_MAX_LISTENERS	32	// Maximum number of listener sockets


//
// Types and structures...
//

typedef struct _pappl_resource_s	// Resource
{
  char			*label,			// Label string
			*path,			// Path
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
			save_time,		// Do we need to save the config?
			shutdown_time;		// Shutdown requested?
  char			*uuid,			// "system-uuid" value
			*name,			// "system-name" value
			*location,		// "system-location" value
			*geo_location,		// "system-geo-location" value
			*dns_sd_name,		// "system-dns-sd-name" value
			*hostname;		// Published hostname
  int			port;			// Port number, if any
  char			*firmware_name,		// "xxx-firmware-name" value
			*firmware_sversion;	// "xxx-firmware-string-version" value
  int			firmware_version[4];	// "xxx-firmware-version" values
  char			*footer_html;		// Footer HTML for web interface
  char			*server_header;		// Server: header value
  char			*directory;		// Spool directory
  char			*logfile;		// Log filename, if any
  int			logfd;			// Log file descriptor, if any
  pappl_loglevel_t	loglevel;		// Log level
  char			*subtypes;		// DNS-SD sub-types, if any
  bool			tls_only;		// Only support TLS?
  char			*auth_service;		// PAM authorization service, if any
  char			*admin_group;		// PAM administrative group, if any
  gid_t			admin_gid;		// PAM administrative group ID
  char			*default_print_group;	// Default PAM printing group, if any
  char			session_key[65];	// Session key
  time_t		session_time;		// Session key time
  int			num_listeners;		// Number of listener sockets
  struct pollfd		listeners[_PAPPL_MAX_LISTENERS];
						// Listener sockets
  cups_array_t		*resources;		// Array of resources
  int			next_client;		// Next client number
  cups_array_t		*printers;		// Array of printers
  int			default_printer_id,	// Default printer-id
			next_printer_id;	// Next printer-id
  pappl_driver_cb_t	driver_cb;		// Driver callback
  void			*driver_cbdata;		// Driver callback data
  pappl_ipp_op_cb_t	op_cb;			// IPP operation callback
  void			*op_cbdata;		// IPP operation callback data
  pappl_save_cb_t	save_cb;		// Save callback
  void			*save_cbdata;		// Save callback data
#  ifdef HAVE_DNSSD
  DNSServiceRef		dns_sd_master;		// DNS-SD services container
  _pappl_srv_t		ipps_ref,		// DNS-SD IPPS service
			loc_ref;		// DNS-SD LOC record
#  elif defined(HAVE_AVAHI)
  AvahiThreadedPoll	*dns_sd_master;		// DNS-SD services container
  AvahiClient		*dns_sd_client;		// Avahi client
  _pappl_srv_t		dns_sd_ref;		// DNS-SD services
#endif // HAVE_DNSSD
  bool			dns_sd_any_collision;	// Was there a name collision for any printer?
  bool			dns_sd_collision;	// Was there a name collision for this system?
};


//
// Functions...
//

extern void		_papplSystemAddPrinterIcons(pappl_system_t *system, pappl_printer_t *printer) _PAPPL_PRIVATE;
extern void		_papplSystemCleanJobs(pappl_system_t *system) _PAPPL_PRIVATE;
extern _pappl_resource_t *_papplSystemFindResource(pappl_system_t *system, const char *path) _PAPPL_PRIVATE;
extern void		_papplSystemInitDNSSD(pappl_system_t *system) _PAPPL_PRIVATE;
extern char		*_papplSystemMakeUUID(pappl_system_t *system, const char *printer_name, int job_id, char *buffer, size_t bufsize) _PAPPL_PRIVATE;
extern bool		_papplSystemRegisterDNSSDNoLock(pappl_system_t *system) _PAPPL_PRIVATE;
extern void		_papplSystemUnregisterDNSSDNoLock(pappl_system_t *system) _PAPPL_PRIVATE;

extern void		_papplSystemWebConfig(pappl_client_t *client, pappl_system_t *system) _PAPPL_PRIVATE;
extern void		_papplSystemWebContact(pappl_client_t *client, pappl_system_t *system) _PAPPL_PRIVATE;
extern void		_papplSystemWebLogin(pappl_client_t *client, pappl_system_t *system) _PAPPL_PRIVATE;
extern void		_papplSystemWebLogout(pappl_client_t *client, pappl_system_t *system) _PAPPL_PRIVATE;
extern void		_papplSystemWebNetwork(pappl_client_t *client, pappl_system_t *system) _PAPPL_PRIVATE;
extern void		_papplSystemWebStatus(pappl_client_t *client, pappl_system_t *system) _PAPPL_PRIVATE;
extern void		_papplSystemWebTLS(pappl_client_t *client, pappl_system_t *system) _PAPPL_PRIVATE;
extern void		_papplSystemWebUsers(pappl_client_t *client, pappl_system_t *system) _PAPPL_PRIVATE;

#endif // !_PAPPL_SYSTEM_PRIVATE_H_
