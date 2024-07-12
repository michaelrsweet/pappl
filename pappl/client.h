//
// Public client header file for the Printer Application Framework
//
// Copyright © 2019-2022 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#ifndef _PAPPL_CLIENT_H_
#  define _PAPPL_CLIENT_H_
#  include "base.h"
#  ifdef __cplusplus
extern "C" {
#  endif // __cplusplus


//
// Functions...
//

extern char		*papplClientGetCSRFToken(pappl_client_t *client, char *buffer, size_t bufsize) _PAPPL_PUBLIC;
extern char		*papplClientGetCookie(pappl_client_t *client, const char *name, char *buffer, size_t bufsize) _PAPPL_PUBLIC;
extern int		papplClientGetForm(pappl_client_t *client, cups_option_t **form) _PAPPL_PUBLIC;
extern const char	*papplClientGetHostName(pappl_client_t *client) _PAPPL_PUBLIC;
extern int		papplClientGetHostPort(pappl_client_t *client) _PAPPL_PUBLIC;
extern http_t		*papplClientGetHTTP(pappl_client_t *client) _PAPPL_PUBLIC;
extern pappl_job_t	*papplClientGetJob(pappl_client_t *client) _PAPPL_PUBLIC;
extern pappl_loc_t	*papplClientGetLoc(pappl_client_t *client) _PAPPL_PUBLIC;
extern const char	*papplClientGetLocString(pappl_client_t *client, const char *s) _PAPPL_PUBLIC;
extern http_state_t	papplClientGetMethod(pappl_client_t *client) _PAPPL_PUBLIC;
extern ipp_op_t		papplClientGetOperation(pappl_client_t *client) _PAPPL_PUBLIC;
extern const char	*papplClientGetOptions(pappl_client_t *client) _PAPPL_PUBLIC;
extern pappl_printer_t	*papplClientGetPrinter(pappl_client_t *client) _PAPPL_PUBLIC;
extern ipp_t		*papplClientGetRequest(pappl_client_t *client) _PAPPL_PUBLIC;
extern ipp_t		*papplClientGetResponse(pappl_client_t *client) _PAPPL_PUBLIC;
extern pappl_system_t	*papplClientGetSystem(pappl_client_t *client) _PAPPL_PUBLIC;
extern const char	*papplClientGetURI(pappl_client_t *client) _PAPPL_PUBLIC;
extern const char	*papplClientGetUsername(pappl_client_t *client) _PAPPL_PUBLIC;

extern bool		papplClientHTMLAuthorize(pappl_client_t *client) _PAPPL_PUBLIC;
extern void		papplClientHTMLEscape(pappl_client_t *client, const char *s, size_t slen) _PAPPL_PUBLIC;
extern void		papplClientHTMLFooter(pappl_client_t *client) _PAPPL_PUBLIC;
extern void		papplClientHTMLHeader(pappl_client_t *client, const char *title, int refresh) _PAPPL_PUBLIC;
extern void     papplClientHTMLScannerHeader(pappl_client_t *client, pappl_scanner_t *scanner, const char *title, int refresh, const char *label, const char *path_or_url)_PAPPL_PUBLIC;
extern void     papplClientHTMLScannerFooter(pappl_client_t *client)_PAPPL_PUBLIC;
extern void		papplClientHTMLPrinterFooter(pappl_client_t *client) _PAPPL_PUBLIC;
extern void		papplClientHTMLPrinterHeader(pappl_client_t *client, pappl_printer_t *printer, const char *title, int refresh, const char *label, const char *path_or_url) _PAPPL_PUBLIC;
extern void		papplClientHTMLPrintf(pappl_client_t *client, const char *format, ...) _PAPPL_PUBLIC _PAPPL_FORMAT(2, 3);
extern void		papplClientHTMLPuts(pappl_client_t *client, const char *s) _PAPPL_PUBLIC;
extern void		papplClientHTMLStartForm(pappl_client_t *client, const char *action, bool multipart) _PAPPL_PUBLIC;

extern http_status_t	papplClientIsAuthorized(pappl_client_t *client) _PAPPL_PUBLIC;
extern bool		papplClientIsEncrypted(pappl_client_t *client) _PAPPL_PUBLIC;
extern bool		papplClientIsValidForm(pappl_client_t *client, int num_form, cups_option_t *form) _PAPPL_PUBLIC;

extern bool		papplClientRespond(pappl_client_t *client, http_status_t code, const char *content_coding, const char *type, time_t last_modified, size_t length) _PAPPL_PUBLIC;
extern ipp_t		*papplClientRespondIPP(pappl_client_t *client, ipp_status_t status, const char *message, ...) _PAPPL_PUBLIC _PAPPL_FORMAT(3, 4);
extern void		papplClientRespondIPPUnsupported(pappl_client_t *client, ipp_attribute_t *attr) _PAPPL_PUBLIC;
extern bool		papplClientRespondRedirect(pappl_client_t *client, http_status_t code, const char *path) _PAPPL_PUBLIC;

extern void		papplClientSetCookie(pappl_client_t *client, const char * name, const char *value, int expires) _PAPPL_PUBLIC;
extern void		papplClientSetUsername(pappl_client_t *client, const char *username) _PAPPL_PUBLIC;


#  ifdef __cplusplus
}
#  endif // __cplusplus
#endif // !_PAPPL_CLIENT_H_
