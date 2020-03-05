//
// Client accessor functions for the Printer Application Framework
//
// Copyright © 2020 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

//
// Include necessary headers...
//

#include "client-private.h"


//
// 'papplClientGetHTTP()' - Get the HTTP connection to the client.
//

http_t *				// O - HTTP connection
papplClientGetHTTP(
    pappl_client_t *client)		// I - Client
{
  return (client ? client->http : NULL);
}



//
// 'papplClientGetJob()' - Get the target job for an IPP request.
//

pappl_job_t *				// O - Target job
papplClientGetJob(
    pappl_client_t *client)		// I - Client
{
  return (client ? client->job : NULL);
}



//
// 'papplClientGetMethod()' - Get the HTTP request method.
//

http_state_t				// O - HTTP method
papplClientGetMethod(
    pappl_client_t *client)		// I - Client
{
  return (client ? client->operation : HTTP_STATE_ERROR);
}



//
// 'papplClientGetOperation()' - Get the IPP operation code.
//

ipp_op_t				// O - IPP operation code
papplClientGetOperation(
    pappl_client_t *client)		// I - Client
{
  return (client ? client->operation_id : IPP_OP_CUPS_NONE);
}



//
// 'papplClientGetPrinter()' - Get the target printer for an IPP request.
//

pappl_printer_t	*			// O - Target printer
papplClientGetPrinter(
    pappl_client_t *client)		// I - Client
{
  return (client ? client->printer : NULL);
}



//
// 'papplClientGetRequest()' - Get the IPP request message.
//

ipp_t *					// O - IPP request message
papplClientGetRequest(
    pappl_client_t *client)		// I - Client
{
  return (client ? client->request : NULL);
}



//
// 'papplClientGetResponse()' - Get the IPP response message.
//

ipp_t *					// O - IPP response message
papplClientGetResponse(
    pappl_client_t *client)		// I - Client
{
  return (client ? client->response : NULL);
}



//
// 'papplClientGetSystem()' - Get the containing system for the client.
//

pappl_system_t *			// O - System
papplClientGetSystem(
    pappl_client_t *client)		// I - Client
{
  return (client ? client->system : NULL);
}



//
// 'papplClientGetURI()' - Get the HTTP request URI.
//

const char *				// O - Request URI
papplClientGetURI(
    pappl_client_t *client)		// I - Client
{
  return (client ? client->uri : NULL);
}



//
// 'papplClientGetUsername()' - Get the authenticated username, if any.
//

const char *				// O - Authenticated username
papplClientGetUsername(
    pappl_client_t *client)		// I - Client
{
  return (client && client->username[0] ? client->username : NULL);
}
