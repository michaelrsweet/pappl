//
// Private client header file for the Printer Application Framework
//
// Copyright © 2019-2022 by Michael R Sweet.
// Copyright © 2010-2019 by Apple Inc.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#ifndef _PAPPL_CLIENT_PRIVATE_H_
#  define _PAPPL_CLIENT_PRIVATE_H_
#  include "base-private.h"
#  include "client.h"
#  include "log.h"


//
// Client structure...
//

struct _pappl_client_s			// Client data
{
  pappl_system_t	*system;		// Containing system
  int			number;			// Connection number
  pthread_t		thread_id;		// Thread ID
  http_t		*http;			// HTTP connection
  ipp_t			*request,		// IPP request
			*response;		// IPP response
  time_t		start;			// Request start time
  http_state_t		operation;		// Request operation
  ipp_op_t		operation_id;		// IPP operation-id
  char			uri[1024],		// Request URI
			*options,		// URI options
			host_field[HTTP_MAX_VALUE];
						// Host: header
  int			host_port;		// Port number from Host: header
  http_addr_t		addr;			// Client address
  char			hostname[256];		// Client hostname
  char			username[256];		// Authenticated username, if any
  char			language[256];		// Accept-Language value, if any
  pappl_printer_t	*printer;		// Printer, if any
  pappl_scanner_t *scanner;		// Scanner, if any
  pappl_job_t		*job;			// Job, if any
  pappl_loc_t		*loc;			// Localization, if any
  int			num_files;		// Number of temporary files
  char			*files[10];		// Temporary files
};


//
// Functions...
//

extern void		_papplClientCleanTempFiles(pappl_client_t *client) _PAPPL_PRIVATE;
extern pappl_client_t	*_papplClientCreate(pappl_system_t *system, int sock) _PAPPL_PRIVATE;
extern char		*_papplClientCreateTempFile(pappl_client_t *client, const void *data, size_t datasize) _PAPPL_PRIVATE;
extern void		_papplClientDelete(pappl_client_t *client) _PAPPL_PRIVATE;
extern void		_papplClientFlushDocumentData(pappl_client_t *client) _PAPPL_PRIVATE;
extern const char	*_papplClientGetAuthWebScheme(pappl_client_t *client) _PAPPL_PRIVATE;
extern bool		_papplClientHaveDocumentData(pappl_client_t *client) _PAPPL_PRIVATE;
extern http_status_t	_papplClientIsAuthorizedForGroup(pappl_client_t *client, bool allow_remote, const char *group, gid_t groupid) _PAPPL_PUBLIC;
extern bool		_papplClientProcessHTTP(pappl_client_t *client) _PAPPL_PRIVATE;
extern bool		_papplClientProcessIPP(pappl_client_t *client) _PAPPL_PRIVATE;
extern void		_papplClientRespondIPPIgnored(pappl_client_t *client, ipp_attribute_t *attr) _PAPPL_PRIVATE;
extern void		*_papplClientRun(pappl_client_t *client) _PAPPL_PRIVATE;
extern void		_papplClientHTMLInfo(pappl_client_t *client, bool is_form, const char *dns_sd_name, const char *location, const char *geo_location, const char *organization, const char *org_unit, pappl_contact_t *contact);
extern void		_papplClientHTMLPutLinks(pappl_client_t *client, cups_array_t *links, pappl_loptions_t which);


#endif // !_PAPPL_CLIENT_PRIVATE_H_
