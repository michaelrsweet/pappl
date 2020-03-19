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
// Callback function types...
//

typedef int (*pappl_driver_cb_t)(const char *driver_name, const char *device_uri, pappl_driver_data_t *driver_data, void *data);
typedef int (*pappl_ipp_op_cb_t)(pappl_client_t *client, void *data);
typedef void (*pappl_printer_cb_t)(pappl_printer_t *printer, void *data);
typedef int (*pappl_resource_cb_t)(pappl_client_t *client, void *data);
typedef int (*pappl_save_cb_t)(pappl_system_t *system, void *data);


//
// Functions...
//

extern bool		papplSystemAddListeners(pappl_system_t *system, const char *name) _PAPPL_PUBLIC;
extern void		papplSystemAddResourceCallback(pappl_system_t *system, const char *path, const char *format, pappl_resource_cb_t cb, void *data) _PAPPL_PUBLIC;
extern void		papplSystemAddResourceData(pappl_system_t *system, const char *path, const char *format, const void *data, size_t datalen) _PAPPL_PUBLIC;
extern void		papplSystemAddResourceDirectory(pappl_system_t *system, const char *basepath, const char *directory) _PAPPL_PUBLIC;
extern void		papplSystemAddResourceFile(pappl_system_t *system, const char *path, const char *format, const char *filename) _PAPPL_PUBLIC;
extern void		papplSystemAddStringsData(pappl_system_t *system, const char *path, const char *language, const char *data) _PAPPL_PUBLIC;
extern void		papplSystemAddStringsFile(pappl_system_t *system, const char *path, const char *language, const char *filename) _PAPPL_PUBLIC;
extern void		papplSystemCleanJobs(pappl_system_t *system) _PAPPL_PUBLIC;
extern pappl_system_t	*papplSystemCreate(const char *uuid, const char *name, const char *hostname, int port, const char *subtypes, const char *spooldir, const char *logfile, pappl_loglevel_t loglevel, const char *auth_service, bool tls_only) _PAPPL_PUBLIC;
extern void		papplSystemDelete(pappl_system_t *system) _PAPPL_PUBLIC;
extern pappl_printer_t	*papplSystemFindPrinter(pappl_system_t *system, const char *resource, int printer_id) _PAPPL_PUBLIC;
extern char		*papplSystemGetAdminGroup(pappl_system_t *system, char *buffer, size_t bufsize) _PAPPL_PUBLIC;
extern const char	*papplSystemGetAuthService(pappl_system_t *system) _PAPPL_PUBLIC;
extern int		papplSystemGetDefaultPrinterID(pappl_system_t *system) _PAPPL_PUBLIC;
extern char		*papplSystemGetDefaultPrintGroup(pappl_system_t *system, char *buffer, size_t bufsize) _PAPPL_PUBLIC;
extern char		*papplSystemGetDNSSDName(pappl_system_t *system, char *buffer, size_t bufsize) _PAPPL_PUBLIC;
extern int		papplSystemGetNextPrinterID(pappl_system_t *system) _PAPPL_PUBLIC;
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
extern void		papplSystemSetDriverCallback(pappl_system_t *system, pappl_driver_cb_t cb, void *data) _PAPPL_PUBLIC;
extern void		papplSystemSetNextPrinterID(pappl_system_t *system, int next_printer_id) _PAPPL_PUBLIC;
extern void		papplSystemSetOperationCallback(pappl_system_t *system, pappl_ipp_op_cb_t cb, void *data) _PAPPL_PUBLIC;
extern void		papplSystemSetSaveCallback(pappl_system_t *system, pappl_save_cb_t cb, void *data) _PAPPL_PUBLIC;
extern void		papplSystemShutdown(pappl_system_t *system) _PAPPL_PUBLIC;


#endif // !_PAPPL_SYSTEM_H_
