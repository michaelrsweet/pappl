//
// Private client header file for the Printer Application Framework
//
// Copyright © 2019-2020 by Michael R Sweet.
// Copyright © 2010-2019 by Apple Inc.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#ifndef _PAPPL_CLIENT_PRIVATE_H_
#  define _PAPPL_CLIENT_PRIVATE_H_

//
// Include necessary headers...
//

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
  pappl_printer_t	*printer;		// Printer, if any
  pappl_job_t		*job;			// Job, if any
  int			num_files;		// Number of temporary files
  char			*files[10];		// Temporary files
};


//
// Functions...
//

extern void		_papplClientCleanTempFiles(pappl_client_t *client) _PAPPL_PRIVATE;
extern char		*_papplClientCreateTempFile(pappl_client_t *client, const void *data, size_t datasize) _PAPPL_PRIVATE;
extern bool		_papplClientProcessHTTP(pappl_client_t *client) _PAPPL_PRIVATE;
extern bool		_papplClientProcessIPP(pappl_client_t *client) _PAPPL_PRIVATE;
extern void		*_papplClientRun(pappl_client_t *client) _PAPPL_PRIVATE;
extern void		_papplClientHTMLInfo(pappl_client_t *client, bool is_form, const char *dns_sd_name, const char *location, const char *geo_location, const char *organization, const char *org_unit, pappl_contact_t *contact);

#endif // !_PAPPL_CLIENT_PRIVATE_H_
