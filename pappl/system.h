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


//
// Callback function types...
//

typedef void (*pappl_printer_cb_t)(pappl_printer_t *printer, void *data);
typedef int (*pappl_resource_cb_t)(pappl_client_t *client, void *data);


//
// Functions...
//

extern void		papplSystemCleanJobs(pappl_system_t *system) _PAPPL_PUBLIC;
extern pappl_system_t	*papplSystemCreate(const char *hostname, int port, const char *subtypes, const char *spooldir, const char *logfile, pappl_loglevel_t loglevel, const char *auth_service, const char *admin_group);
extern void		papplSystemDelete(pappl_system_t *system);
extern pappl_printer_t	*papplSystemFindPrinter(pappl_system_t *system, const char *resource, int printer_id);
extern void		papplSystemInitDNSSD(pappl_system_t *system); // Why not part of creating the system?
extern void		papplSystemIteratePrinters(pappl_system_t *system, pappl_printer_cb_t cb, void *data);
extern void		papplSystemRun(pappl_system_t *system);

#endif // !_PAPPL_SYSTEM_H_
