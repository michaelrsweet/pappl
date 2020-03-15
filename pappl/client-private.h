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
			*options;		// URI options
  http_addr_t		addr;			// Client address
  char			hostname[256];		// Client hostname
  char			username[256];		// Authenticated username, if any
  pappl_printer_t	*printer;		// Printer, if any
  pappl_job_t		*job;			// Job, if any
};


//
// Functions...
//

extern int		_papplClientProcessHTTP(pappl_client_t *client) _PAPPL_PRIVATE;
extern int		_papplClientProcessIPP(pappl_client_t *client) _PAPPL_PRIVATE;
extern void		*_papplClientRun(pappl_client_t *client) _PAPPL_PRIVATE;


#endif // !_PAPPL_CLIENT_PRIVATE_H_
