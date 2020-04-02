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
#include "system.h"


//
// 'papplClientGetCSRFToken()' - Get a unique Cross-Site Request Forgery token string.
//
// This value is based on the current system session key and client address.
// It should be used as the value of a hidden variable in all HTML forms on
// the GET request and then compared when validating the form data in the
// corresponding POST request.
//

char *					// O - Token string
papplClientGetCSRFString(
    pappl_client_t *client,		// I - Client
    char           *buffer,		// I - String buffer
    size_t         bufsize)		// I - Size of string buffer
{
  char		session_key[65],	// Current session key
		csrf_data[1024];	// CSRF data to hash
  unsigned char	csrf_sum[32];		// SHA2-256 sum of data


  if (!client || !buffer || bufsize < 65)
  {
    if (buffer)
      *buffer = '\0';

    return (NULL);
  }

  snprintf(csrf_data, sizeof(csrf_data), "%s:%s", papplSystemGetSessionKey(client->system, session_key, sizeof(session_key)), client->hostname);
  cupsHashData("sha2-256", csrf_data, strlen(csrf_data), csrf_sum, sizeof(csrf_sum));
  cupsHashString(csrf_sum, sizeof(csrf_sum), buffer, bufsize);

  return (buffer);
}


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
