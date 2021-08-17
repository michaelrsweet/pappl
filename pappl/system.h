//
// Public system header file for the Printer Application Framework
//
// Copyright © 2019-2021 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#ifndef _PAPPL_SYSTEM_H_
#  define _PAPPL_SYSTEM_H_

//
// Include necessary headers...
//

#  include "base.h"
#  include "log.h"


//
// C++ magic...
//

#  ifdef __cplusplus
extern "C" {
#  endif // __cplusplus


//
// Types...
//

typedef struct pappl_pr_driver_s	// Printer driver information
{
  const char	*name;				// Driver name
  const char	*description;			// Driver description (usually the make and model)
  const char	*device_id;			// IEEE-1284 device ID
  void		*extension;			// Extension data pointer
} pappl_pr_driver_t;

enum pappl_soptions_e			// System option bits
{
  PAPPL_SOPTIONS_NONE = 0x0000,			// No options
  PAPPL_SOPTIONS_DNSSD_HOST = 0x0001,		// Use hostname in DNS-SD service names instead of serial number/UUID
  PAPPL_SOPTIONS_MULTI_QUEUE = 0x0002,		// Support multiple printers
  PAPPL_SOPTIONS_RAW_SOCKET = 0x0004,		// Accept jobs via raw sockets
  PAPPL_SOPTIONS_USB_PRINTER = 0x0008,		// Accept jobs via USB for default printer (embedded Linux only)
  PAPPL_SOPTIONS_WEB_INTERFACE = 0x0010,	// Enable the standard web pages
  PAPPL_SOPTIONS_WEB_LOG = 0x0020,		// Enable the log file page
  PAPPL_SOPTIONS_WEB_NETWORK = 0x0040,		// Enable the network settings page
  PAPPL_SOPTIONS_WEB_REMOTE = 0x0080,		// Allow remote queue management (vs. localhost only)
  PAPPL_SOPTIONS_WEB_SECURITY = 0x0100,		// Enable the user/password settings page
  PAPPL_SOPTIONS_WEB_TLS = 0x0200,		// Enable the TLS settings page
  PAPPL_SOPTIONS_NO_TLS = 0x0400		// Disable TLS support @since PAPPL 1.1@
};
typedef unsigned pappl_soptions_t;	// Bitfield for system options

typedef struct pappl_version_s		// Firmware version information
{
  char			name[64];		// "xxx-firmware-name" value
  char			patches[64];		// "xxx-firmware-patches" value
  char			sversion[64];		// "xxx-firmware-string-version" value
  unsigned short	version[4];		// "xxx-firmware-version" value
} pappl_version_t;

typedef enum pappl_wifi_state_e		// "printer-wifi-state" values
{
  PAPPL_WIFI_STATE_OFF = 3,			// 'off'
  PAPPL_WIFI_STATE_NOT_CONFIGURED,		// 'not-configured'
  PAPPL_WIFI_STATE_NOT_VISIBLE,			// 'not-visible'
  PAPPL_WIFI_STATE_CANNOT_JOIN,			// 'cannot-join'
  PAPPL_WIFI_STATE_JOINING,			// 'joining'
  PAPPL_WIFI_STATE_ON				// 'on'
} pappl_wifi_state_t;

typedef struct pappl_wifi_s		// Wi-Fi status/configuration information
{
  pappl_wifi_state_t	state;			// Current "printer-wifi-state" value
  char			ssid[128];		// Current "printer-wifi-ssid" value
} pappl_wifi_t;


//
// Callback function types...
//

typedef const char *(*pappl_pr_autoadd_cb_t)(const char *device_info, const char *device_uri, const char *device_id, void *data);
					// Auto-add callback
typedef void (*pappl_pr_create_cb_t)(pappl_printer_t *printer, void *data);
					// Printer creation callback
typedef bool (*pappl_pr_driver_cb_t)(pappl_system_t *system, const char *driver_name, const char *device_uri, const char *device_id, pappl_pr_driver_data_t *driver_data, ipp_t **driver_attrs, void *data);
					// Driver callback function

typedef const char *(*pappl_sc_autoadd_cb_t)(const char *device_info, const char *device_uri, const char *device_id, void *data);
					// Auto-add callback
typedef void (*pappl_sc_create_cb_t)(pappl_scanner_t *scanner, void *data);
					// Scanner creation callback
typedef bool (*pappl_sc_driver_cb_t)(pappl_system_t *system, const char *driver_name, const char *device_uri, const char *device_id, pappl_sc_driver_data_t *driver_data, ipp_t **driver_attrs, void *data);
					// Driver callback function

typedef bool (*pappl_mime_filter_cb_t)(pappl_job_t *job, pappl_device_t *device, void *data);
					// Filter callback function
typedef bool (*pappl_ipp_op_cb_t)(pappl_client_t *client, void *data);
					// IPP operation callback function
typedef const char *(*pappl_mime_cb_t)(const unsigned char *header, size_t headersize, void *data);
					// MIME typing callback function
typedef void (*pappl_printer_cb_t)(pappl_printer_t *printer, void *data);
					// Printer iterator callback function

typedef void (*pappl_scanner_cb_t)(pappl_scanner_t *scanner, void *data);
					// Scanner iterator callback function


typedef bool (*pappl_resource_cb_t)(pappl_client_t *client, void *data);
					// Dynamic resource callback function
typedef bool (*pappl_save_cb_t)(pappl_system_t *system, void *data);
					// Save callback function

typedef bool (*pappl_wifi_join_cb_t)(pappl_system_t *system, void *data, const char *ssid, const char *psk);
					// Wi-Fi join callback
typedef int (*pappl_wifi_list_cb_t)(pappl_system_t *system, void *data, cups_dest_t **ssids);
					// Wi-Fi list callback
typedef pappl_wifi_t *(*pappl_wifi_status_cb_t)(pappl_system_t *system, void *data, pappl_wifi_t *wifi_data);
					// Wi-Fi status callback


//
// Functions...
//

extern void		papplSystemAddLink(pappl_system_t *system, const char *label, const char *path_or_url, pappl_loptions_t options) _PAPPL_PUBLIC;
extern bool		papplSystemAddListeners(pappl_system_t *system, const char *name) _PAPPL_PUBLIC;
extern void		papplSystemAddMIMEFilter(pappl_system_t *system, const char *srctype, const char *dsttype, pappl_mime_filter_cb_t cb, void *data) _PAPPL_PUBLIC;
extern void		papplSystemAddResourceCallback(pappl_system_t *system, const char *path, const char *format, pappl_resource_cb_t cb, void *data) _PAPPL_PUBLIC;
extern void		papplSystemAddResourceData(pappl_system_t *system, const char *path, const char *format, const void *data, size_t datalen) _PAPPL_PUBLIC;
extern void		papplSystemAddResourceDirectory(pappl_system_t *system, const char *basepath, const char *directory) _PAPPL_PUBLIC;
extern void		papplSystemAddResourceFile(pappl_system_t *system, const char *path, const char *format, const char *filename) _PAPPL_PUBLIC;
extern void		papplSystemAddResourceString(pappl_system_t *system, const char *path, const char *format, const char *data) _PAPPL_PUBLIC;
extern void		papplSystemAddStringsData(pappl_system_t *system, const char *path, const char *language, const char *data) _PAPPL_PUBLIC;
extern void		papplSystemAddStringsFile(pappl_system_t *system, const char *path, const char *language, const char *filename) _PAPPL_PUBLIC;
extern void		papplSystemCleanJobs(pappl_system_t *system) _PAPPL_PUBLIC;
extern pappl_system_t	*papplSystemCreate(pappl_soptions_t options, const char *name, int port, const char *subtypes, const char *spooldir, const char *logfile, pappl_loglevel_t loglevel, const char *auth_service, bool tls_only) _PAPPL_PUBLIC;
extern void		papplSystemDelete(pappl_system_t *system) _PAPPL_PUBLIC;
extern pappl_printer_t	*papplSystemFindPrinter(pappl_system_t *system, const char *resource, int printer_id, const char *device_uri) _PAPPL_PUBLIC;
extern char		*papplSystemGetAdminGroup(pappl_system_t *system, char *buffer, size_t bufsize) _PAPPL_PUBLIC;
extern const char	*papplSystemGetAuthService(pappl_system_t *system) _PAPPL_PUBLIC;
extern pappl_contact_t	*papplSystemGetContact(pappl_system_t *system, pappl_contact_t *contact) _PAPPL_PUBLIC;
extern int		papplSystemGetDefaultPrinterID(pappl_system_t *system) _PAPPL_PUBLIC;
extern char		*papplSystemGetDefaultPrintGroup(pappl_system_t *system, char *buffer, size_t bufsize) _PAPPL_PUBLIC;
extern char		*papplSystemGetDNSSDName(pappl_system_t *system, char *buffer, size_t bufsize) _PAPPL_PUBLIC;
extern const char	*papplSystemGetFooterHTML(pappl_system_t *system) _PAPPL_PUBLIC;
extern char		*papplSystemGetGeoLocation(pappl_system_t *system, char *buffer, size_t bufsize) _PAPPL_PUBLIC;
extern char		*papplSystemGetHostname(pappl_system_t *system, char *buffer, size_t bufsize) _PAPPL_PUBLIC;
extern char		*papplSystemGetLocation(pappl_system_t *system, char *buffer, size_t bufsize) _PAPPL_PUBLIC;
extern pappl_loglevel_t	papplSystemGetLogLevel(pappl_system_t *system) _PAPPL_PUBLIC;
extern size_t		papplSystemGetMaxLogSize(pappl_system_t *system) _PAPPL_PUBLIC;
extern char		*papplSystemGetName(pappl_system_t *system, char *buffer, size_t bufsize) _PAPPL_PUBLIC;
extern int		papplSystemGetNextPrinterID(pappl_system_t *system) _PAPPL_PUBLIC;
extern pappl_soptions_t	papplSystemGetOptions(pappl_system_t *system) _PAPPL_PUBLIC;
extern char		*papplSystemGetOrganization(pappl_system_t *system, char *buffer, size_t bufsize) _PAPPL_PUBLIC;
extern char		*papplSystemGetOrganizationalUnit(pappl_system_t *system, char *buffer, size_t bufsize) _PAPPL_PUBLIC;
extern char		*papplSystemGetPassword(pappl_system_t *system, char *buffer, size_t bufsize) _PAPPL_PUBLIC;
extern int		papplSystemGetPort(pappl_system_t *system) _PAPPL_PUBLIC;
extern const char	*papplSystemGetServerHeader(pappl_system_t *system) _PAPPL_PUBLIC;
extern char		*papplSystemGetSessionKey(pappl_system_t *system, char *buffer, size_t bufsize) _PAPPL_PUBLIC;
extern bool		papplSystemGetTLSOnly(pappl_system_t *system) _PAPPL_PUBLIC;
extern const char	*papplSystemGetUUID(pappl_system_t *system) _PAPPL_PUBLIC;
extern int		papplSystemGetVersions(pappl_system_t *system, int max_versions, pappl_version_t *versions) _PAPPL_PUBLIC;
extern char		*papplSystemHashPassword(pappl_system_t *system, const char *salt, const char *password, char *buffer, size_t bufsize) _PAPPL_PUBLIC;
extern bool		papplSystemIsRunning(pappl_system_t *system) _PAPPL_PUBLIC;
extern bool		papplSystemIsShutdown(pappl_system_t *system) _PAPPL_PUBLIC;
extern void		papplSystemIteratePrinters(pappl_system_t *system, pappl_printer_cb_t cb, void *data) _PAPPL_PUBLIC;
extern bool		papplSystemLoadState(pappl_system_t *system, const char *filename) _PAPPL_PUBLIC;
extern const char	*papplSystemMatchDriver(pappl_system_t *system, const char *device_id) _PAPPL_PUBLIC;
extern void		papplSystemRemoveLink(pappl_system_t *system, const char *label) _PAPPL_PUBLIC;
extern void		papplSystemRemoveResource(pappl_system_t *system, const char *path) _PAPPL_PUBLIC;
extern void		papplSystemRun(pappl_system_t *system) _PAPPL_PUBLIC;
extern bool		papplSystemSaveState(pappl_system_t *system, const char *filename) _PAPPL_PUBLIC;

extern void		papplSystemSetAdminGroup(pappl_system_t *system, const char *value) _PAPPL_PUBLIC;
extern void		papplSystemSetContact(pappl_system_t *system, pappl_contact_t *contact) _PAPPL_PUBLIC;
extern void		papplSystemSetDefaultPrinterID(pappl_system_t *system, int default_printer_id) _PAPPL_PUBLIC;
extern void		papplSystemSetDefaultPrintGroup(pappl_system_t *system, const char *value) _PAPPL_PUBLIC;
extern void		papplSystemSetDNSSDName(pappl_system_t *system, const char *value) _PAPPL_PUBLIC;
extern void		papplSystemSetFooterHTML(pappl_system_t *system, const char *html) _PAPPL_PUBLIC;
extern void		papplSystemSetGeoLocation(pappl_system_t *system, const char *value) _PAPPL_PUBLIC;
extern void		papplSystemSetHostname(pappl_system_t *system, const char *value) _PAPPL_PUBLIC;
extern void		papplSystemSetLocation(pappl_system_t *system, const char *value) _PAPPL_PUBLIC;
extern void		papplSystemSetLogLevel(pappl_system_t *system, pappl_loglevel_t loglevel) _PAPPL_PUBLIC;
extern void		papplSystemSetMaxLogSize(pappl_system_t *system, size_t maxSize) _PAPPL_PUBLIC;
extern void		papplSystemSetMIMECallback(pappl_system_t *system, pappl_mime_cb_t cb, void *data) _PAPPL_PUBLIC;
extern void		papplSystemSetNextPrinterID(pappl_system_t *system, int next_printer_id) _PAPPL_PUBLIC;
extern void		papplSystemSetOperationCallback(pappl_system_t *system, pappl_ipp_op_cb_t cb, void *data) _PAPPL_PUBLIC;
extern void		papplSystemSetOrganization(pappl_system_t *system, const char *value) _PAPPL_PUBLIC;
extern void		papplSystemSetOrganizationalUnit(pappl_system_t *system, const char *value) _PAPPL_PUBLIC;
extern void		papplSystemSetPassword(pappl_system_t *system, const char *hash) _PAPPL_PUBLIC;
extern void		papplSystemSetPrinterDrivers(pappl_system_t *system, int num_drivers, pappl_pr_driver_t *drivers, pappl_pr_autoadd_cb_t autoadd_cb, pappl_pr_create_cb_t create_cb, pappl_pr_driver_cb_t driver_cb, void *data) _PAPPL_PUBLIC;

//extern void		papplSystemSetScannerDrivers(pappl_system_t *system, int num_drivers, pappl_sc_driver_t *drivers, pappl_sc_autoadd_cb_t autoadd_cb, pappl_sc_create_cb_t create_cb, pappl_sc_driver_cb_t driver_cb, void *data) _PAPPL_PUBLIC;


extern void		papplSystemSetSaveCallback(pappl_system_t *system, pappl_save_cb_t cb, void *data) _PAPPL_PUBLIC;
extern void		papplSystemSetUUID(pappl_system_t *system, const char *value) _PAPPL_PUBLIC;
extern void		papplSystemSetVersions(pappl_system_t *system, int num_versions, pappl_version_t *versions) _PAPPL_PUBLIC;
extern void		papplSystemSetWiFiCallbacks(pappl_system_t *system, pappl_wifi_join_cb_t join_cb, pappl_wifi_list_cb_t list_cb, pappl_wifi_status_cb_t status_cb, void *data) _PAPPL_PUBLIC;
extern void		papplSystemShutdown(pappl_system_t *system) _PAPPL_PUBLIC;


//
// C++ magic...
//

#  ifdef __cplusplus
}
#  endif // __cplusplus


#endif // !_PAPPL_SYSTEM_H_
