//
// Public system header file for the Printer Application Framework
//
// Copyright © 2019-2020 by Michael R Sweet.
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
// Types...
//

enum pappl_soptions_e			// System option bits
{
  PAPPL_SOPTIONS_NONE = 0x0000,			// No options
  PAPPL_SOPTIONS_MULTI_QUEUE = 0x0001,		// Support multiple printers
  PAPPL_SOPTIONS_STANDARD = 0x0002,		// Include the standard web interfaces
  PAPPL_SOPTIONS_REMOTE_ADMIN = 0x0004,		// Allow remote queue management (vs. localhost only)
  PAPPL_SOPTIONS_NETWORK = 0x0008,		// Include network configuration
  PAPPL_SOPTIONS_TLS = 0x0010,			// Include TLS configuration
  PAPPL_SOPTIONS_USERS = 0x0020,		// Include user configuration
  PAPPL_SOPTIONS_ALL = 0x7fffffff		// Include all options
};
typedef unsigned pappl_soptions_t;	// Bitfield for system options


//
// Callback function types...
//

typedef bool (*pappl_driver_cb_t)(pappl_system_t *system, const char *driver_name, const char *device_uri, pappl_driver_data_t *driver_data, ipp_t **driver_attrs, void *data);
typedef bool (*pappl_ipp_op_cb_t)(pappl_client_t *client, void *data);
typedef void (*pappl_printer_cb_t)(pappl_printer_t *printer, void *data);
typedef bool (*pappl_resource_cb_t)(pappl_client_t *client, void *data);
typedef bool (*pappl_save_cb_t)(pappl_system_t *system, void *data);


//
// Functions...
//

extern bool		papplSystemAddListeners(pappl_system_t *system, const char *name) _PAPPL_PUBLIC;
extern void		papplSystemAddResourceCallback(pappl_system_t *system, const char *label, const char *path, const char *format, pappl_resource_cb_t cb, void *data) _PAPPL_PUBLIC;
extern void		papplSystemAddResourceData(pappl_system_t *system, const char *path, const char *format, const void *data, size_t datalen) _PAPPL_PUBLIC;
extern void		papplSystemAddResourceDirectory(pappl_system_t *system, const char *basepath, const char *directory) _PAPPL_PUBLIC;
extern void		papplSystemAddResourceFile(pappl_system_t *system, const char *path, const char *format, const char *filename) _PAPPL_PUBLIC;
extern void		papplSystemAddResourceString(pappl_system_t *system, const char *path, const char *format, const char *data) _PAPPL_PUBLIC;
extern void		papplSystemAddStringsData(pappl_system_t *system, const char *path, const char *language, const char *data) _PAPPL_PUBLIC;
extern void		papplSystemAddStringsFile(pappl_system_t *system, const char *path, const char *language, const char *filename) _PAPPL_PUBLIC;
extern void		papplSystemCleanJobs(pappl_system_t *system) _PAPPL_PUBLIC;
extern pappl_system_t	*papplSystemCreate(pappl_soptions_t options, const char *uuid, const char *name, const char *hostname, int port, const char *subtypes, const char *spooldir, const char *logfile, pappl_loglevel_t loglevel, const char *auth_service, bool tls_only) _PAPPL_PUBLIC;
extern void		papplSystemDelete(pappl_system_t *system) _PAPPL_PUBLIC;
extern pappl_printer_t	*papplSystemFindPrinter(pappl_system_t *system, const char *resource, int printer_id) _PAPPL_PUBLIC;
extern char		*papplSystemGetAdminGroup(pappl_system_t *system, char *buffer, size_t bufsize) _PAPPL_PUBLIC;
extern const char	*papplSystemGetAuthService(pappl_system_t *system) _PAPPL_PUBLIC;
extern int		papplSystemGetDefaultPrinterID(pappl_system_t *system) _PAPPL_PUBLIC;
extern char		*papplSystemGetDefaultPrintGroup(pappl_system_t *system, char *buffer, size_t bufsize) _PAPPL_PUBLIC;
extern char		*papplSystemGetDNSSDName(pappl_system_t *system, char *buffer, size_t bufsize) _PAPPL_PUBLIC;
extern char		*papplSystemGetFirmware(pappl_system_t *system, char *name, size_t namesize, char *sversion, size_t sversionsize, unsigned short version[4]) _PAPPL_PUBLIC;
extern const char	*papplSystemGetFooterHTML(pappl_system_t *system) _PAPPL_PUBLIC;
extern char		*papplSystemGetGeoLocation(pappl_system_t *system, char *buffer, size_t bufsize) _PAPPL_PUBLIC;
extern char		*papplSystemGetLocation(pappl_system_t *system, char *buffer, size_t bufsize) _PAPPL_PUBLIC;
extern char		*papplSystemGetName(pappl_system_t *system, char *buffer, size_t bufsize) _PAPPL_PUBLIC;
extern int		papplSystemGetNextPrinterID(pappl_system_t *system) _PAPPL_PUBLIC;
extern pappl_soptions_t	papplSystemGetOptions(pappl_system_t *system) _PAPPL_PUBLIC;
extern const char	*papplSystemGetServerHeader(pappl_system_t *system) _PAPPL_PUBLIC;
extern char		*papplSystemGetSessionKey(pappl_system_t *system, char *buffer, size_t bufsize) _PAPPL_PUBLIC;
extern bool		papplSystemGetTLSOnly(pappl_system_t *system) _PAPPL_PUBLIC;
extern const char	*papplSystemGetUUID(pappl_system_t *system) _PAPPL_PUBLIC;
extern void		papplSystemIteratePrinters(pappl_system_t *system, pappl_printer_cb_t cb, void *data) _PAPPL_PUBLIC;
extern void		papplSystemRemoveResource(pappl_system_t *system, const char *path) _PAPPL_PUBLIC;
extern void		papplSystemRun(pappl_system_t *system) _PAPPL_PUBLIC;
extern void		papplSystemSetAdminGroup(pappl_system_t *system, const char *value) _PAPPL_PUBLIC;
extern void		papplSystemSetDefaultPrinterID(pappl_system_t *system, int default_printer_id) _PAPPL_PUBLIC;
extern void		papplSystemSetDefaultPrintGroup(pappl_system_t *system, const char *value) _PAPPL_PUBLIC;
extern void		papplSystemSetDNSSDName(pappl_system_t *system, const char *value) _PAPPL_PUBLIC;
extern void		papplSystemSetDrivers(pappl_system_t *system, int num_names, const char * const *names, pappl_driver_cb_t cb, void *data) _PAPPL_PUBLIC;
extern void		papplSystemSetFirmware(pappl_system_t *system, const char *name, const char *sversion, unsigned short version[4]) _PAPPL_PUBLIC;
extern void		papplSystemSetFooterHTML(pappl_system_t *system, const char *html) _PAPPL_PUBLIC;
extern void		papplSystemSetGeoLocation(pappl_system_t *system, const char *value) _PAPPL_PUBLIC;
extern void		papplSystemSetLocation(pappl_system_t *system, const char *value) _PAPPL_PUBLIC;
extern void		papplSystemSetName(pappl_system_t *system, const char *value) _PAPPL_PUBLIC;
extern void		papplSystemSetNextPrinterID(pappl_system_t *system, int next_printer_id) _PAPPL_PUBLIC;
extern void		papplSystemSetOperationCallback(pappl_system_t *system, pappl_ipp_op_cb_t cb, void *data) _PAPPL_PUBLIC;
extern void		papplSystemSetSaveCallback(pappl_system_t *system, pappl_save_cb_t cb, void *data) _PAPPL_PUBLIC;
extern void		papplSystemShutdown(pappl_system_t *system) _PAPPL_PUBLIC;


#endif // !_PAPPL_SYSTEM_H_
