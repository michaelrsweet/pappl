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

extern void		papplSystemAddDomainListener(pappl_system_t *system, const char *filename);
extern void		papplSystemAddResourceCallback(pappl_system_t *system, const char *path, const char *format, pappl_resource_cb_t cb, void *data) _PAPPL_PUBLIC;
extern void		papplSystemAddResourceData(pappl_system_t *system, const char *path, const char *format, const void *data, size_t datalen) _PAPPL_PUBLIC;
extern void		papplSystemAddResourceDirectory(pappl_system_t *system, const char *basepath, const char *directory) _PAPPL_PUBLIC;
extern void		papplSystemAddResourceFile(pappl_system_t *system, const char *path, const char *format, const char *filename) _PAPPL_PUBLIC;
extern void		papplSystemAddStringsData(pappl_system_t *system, const char *path, const char *language, const char *data) _PAPPL_PUBLIC;
extern void		papplSystemAddStringsFile(pappl_system_t *system, const char *path, const char *language, const char *filename) _PAPPL_PUBLIC;
extern void		papplSystemCleanJobs(pappl_system_t *system) _PAPPL_PUBLIC;
extern pappl_system_t	*papplSystemCreate(const char *hostname, int port, const char *subtypes, const char *spooldir, const char *logfile, pappl_loglevel_t loglevel, const char *auth_service, const char *admin_group) _PAPPL_PUBLIC;
extern void		papplSystemDelete(pappl_system_t *system) _PAPPL_PUBLIC;
extern pappl_printer_t	*papplSystemFindPrinter(pappl_system_t *system, const char *resource, int printer_id) _PAPPL_PUBLIC;
extern int		papplSystemGetDefaultPrinterId(pappl_system_t *system) _PAPPL_PUBLIC;
extern int		papplSystemGetNextPrinterId(pappl_system_t *system) _PAPPL_PUBLIC;
extern const char	papplSystemGetSessionKey(pappl_system_t *system) _PAPPL_PUBLIC;
extern void		papplSystemInitDNSSD(pappl_system_t *system) _PAPPL_PUBLIC; // Why not part of creating the system?
extern void		papplSystemIteratePrinters(pappl_system_t *system, pappl_printer_cb_t cb, void *data) _PAPPL_PUBLIC;
extern void		papplSystemRemoveResource(pappl_system_t *system, const char *path) _PAPPL_PUBLIC;
extern void		papplSystemRun(pappl_system_t *system) _PAPPL_PUBLIC;
extern void		papplSystemSetDefaultPrinterId(pappl_system_t *system, int default_printer_id) _PAPPL_PUBLIC;
extern void		papplSystemSetDriverCallback(pappl_system_t *system, pappl_driver_cb_t cb, void *data);
extern void		papplSystemSetNextPrinterId(pappl_system_t *system, int next_printer_id) _PAPPL_PUBLIC;
extern void		papplSystemSetOperationCallback(pappl_system_t *system, pappl_ipp_op_cb_t cb, void *data);
extern void		papplSystemSetSaveCallback(pappl_system_t *system, pappl_save_cb_t cb, void *data);
extern void		papplSystemShutdown(pappl_system_t *system);


#endif // !_PAPPL_SYSTEM_H_
