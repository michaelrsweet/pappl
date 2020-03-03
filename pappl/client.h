//
// Public client header file for the Printer Application Framework
//
// Copyright © 2019-2020 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#ifndef _PAPPL_CLIENT_H_
#  define _PAPPL_CLIENT_H_


//
// Include necessary headers...
//

#  include "base.h"


extern pappl_client_t	*papplClientCreate(pappl_system_t *system, int sock) _PAPPL_PUBLIC;
extern void		papplClientDelete(pappl_client_t *client) _PAPPL_PUBLIC;
extern int		papplClientGetForm(pappl_client_t *client, cups_option_t **form) _PAPPL_PUBLIC;
extern http_t		*papplClientGetHTTP(pappl_client_t *client) _PAPPL_PUBLIC;
extern pappl_job_t	*papplClientGetJob(pappl_client_t *client) _PAPPL_PUBLIC;
extern http_state_t	papplClientGetMethod(pappl_client_t *client) _PAPPL_PUBLIC;
extern ipp_op_t		*papplClientGetOperation(pappl_client_t *client) _PAPPL_PUBLIC;
extern pappl_printer_t	*papplClientGetPrinter(pappl_client_t *client) _PAPPL_PUBLIC;
extern ipp_t		*papplClientGetRequest(pappl_client_t *client) _PAPPL_PUBLIC;
extern ipp_t		*papplClientGetResponse(pappl_client_t *client) _PAPPL_PUBLIC;
extern pappl_system_t	*papplClientGetSystem(pappl_client_t *client) _PAPPL_PUBLIC;
extern const char	*papplClientGetUsername(pappl_client_t *client) _PAPPL_PUBLIC;
extern http_status_t	papplClientIsAuthorized(pappl_client_t *client) _PAPPL_PUBLIC;
extern int		papplClientPrintf(pappl_client_t *client, const char *message, ...) _PAPPL_PUBLIC _PAPPL_FORMAT(2,3);
extern int		papplClientRespondHTTP(pappl_client_t *client, http_status_t code, const char *content_coding, const char *type, size_t length) _PAPPL_PUBLIC;
extern void		papplClientRespondIPP(pappl_client_t *client, ipp_status_t status, const char *message, ...) _PAPPL_PUBLIC _PAPPL_FORMAT(3, 4);


#endif // !_PAPPL_CLIENT_H_
