//
// Client accessor functions for the Printer Application Framework
//
// Copyright © 2020-2021 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

//
// Include necessary headers...
//

#include "pappl-private.h"


//
// 'papplClientGetCSRFToken()' - Get a unique Cross-Site Request Forgery token
//                               string.
//
// This function generates and returns a unique Cross-Site Request Forgery
// token string to be used as the value of a hidden variable in all HTML forms
// sent in the response and then compared when validating the form data in the
// subsequent request.
//
// The value is based on the current system session key and client address in
// order to make replay attacks infeasible.
//
// > Note: The @link papplClientHTMLStartForm@ function automatically adds the
// > hidden CSRF variable, and the @link papplClientIsValidForm@ function
// > validates the value.
//

char *					// O - Token string
papplClientGetCSRFToken(
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
// 'papplClientGetHostName()' - Get the hostname from the client-supplied Host:
//                              field.
//
// This function returns the hostname that was used in the request and should
// be used in any URLs or URIs that you generate.
//

const char *				// O - Hostname or `NULL` for none
papplClientGetHostName(
    pappl_client_t *client)		// I - Client
{
  return (client ? client->host_field : NULL);
}


//
// 'papplClientGetHostPort()' - Get the port from the client-supplied Host:
//                              field.
//
// This function returns the port number that was used in the request and should
// be used in any URLs or URIs that you generate.
//

int					// O - Port number or `0` for none
papplClientGetHostPort(
    pappl_client_t *client)		// I - Client
{
  return (client ? client->host_port : 0);
}


//
// 'papplClientGetHTTP()' - Get the HTTP connection associated with a client
//                          object.
//
// This function returns the HTTP connection associated with the client and is
// used when sending response data directly to the client using the CUPS
// `httpXxx` functions.
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
// This function returns the job associated with the current IPP request.
// `NULL` is returned if the request does not target a job.
//

pappl_job_t *				// O - Target job or `NULL` if none
papplClientGetJob(
    pappl_client_t *client)		// I - Client
{
  return (client ? client->job : NULL);
}


//
// 'papplClientGetMethod()' - Get the HTTP request method.
//
// This function returns the HTTP request method that was used, for example
// `HTTP_STATE_GET` for a GET request or `HTTP_STATE_POST` for a POST request.
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
// This function returns the IPP operation code associated with the current IPP
// request.
//

ipp_op_t				// O - IPP operation code
papplClientGetOperation(
    pappl_client_t *client)		// I - Client
{
  return (client ? client->operation_id : IPP_OP_CUPS_NONE);
}


//
// 'papplClientGetOptions()' - Get the options from the request URI.
//
// This function returns any options that were passed in the HTTP request URI.
// The options are the characters after the "?" character, for example a
// request URI of "/mypage?name=value" will have an options string of
// "name=value".
//
// `NULL` is returned if the request URI did not contain any options.
//
// > Note: HTTP GET form variables are normally accessed using the
// > @link papplClientGetForm@ function.  This function should only be used when
// > getting non-form data.
//

const char *				// O - Options or `NULL` if none
papplClientGetOptions(
    pappl_client_t *client)		// I - Client
{
  return (client ? client->options : NULL);
}


//
// 'papplClientGetPrinter()' - Get the target printer for an IPP request.
//
// This function returns the printer associated with the current IPP request.
// `NULL` is returned if the request does not target a printer.
//

pappl_printer_t	*			// O - Target printer or `NULL` if none
papplClientGetPrinter(
    pappl_client_t *client)		// I - Client
{
  return (client ? client->printer : NULL);
}


//
// 'papplClientGetRequest()' - Get the IPP request message.
//
// This function returns the attributes in the current IPP request, for use
// with the CUPS `ippFindAttribute`, `ippFindNextAttribute`,
// `ippFirstAttribute`, and `ippNextAttribute` functions.
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
// This function returns the attributes in the current IPP response, for use
// with the CUPS `ippAddXxx` and `ippSetXxx` functions.  Use the
// @link papplClientRespondIPP@ function to set the status code and message,
// if any.
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
// This function returns the system object that contains the client.
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
// This function returns the URI that was sent in the current HTTP request.
//
// > Note: Any options in the URI are removed and can be accessed separately
// > using the @link papplClientGetOptions@ function.
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
// This function returns the current authenticated username, if any.
//

const char *				// O - Authenticated username or `NULL` if none
papplClientGetUsername(
    pappl_client_t *client)		// I - Client
{
  return (client && client->username[0] ? client->username : NULL);
}


//
// '_papplClientGetAuthWebScheme()' - Get the URI scheme to use for potentially authenticated web page links.
//

const char *				// O - "http" or "https"
_papplClientGetAuthWebScheme(
    pappl_client_t *client)		// I - Client
{
  if (!client || (papplSystemGetOptions(client->system) & PAPPL_SOPTIONS_NO_TLS))
  {
    // Use HTTP if TLS is disabled...
    return ("http");
  }
  else if (papplSystemGetTLSOnly(client->system))
  {
    // Use HTTPS if non-TLS is disabled...
    return ("https");
  }
  else if (httpAddrIsLocalhost(httpGetAddress(client->http)))
  {
    // Use HTTP over loopback interface...
    return ("http");
  }
  else if (client->system->auth_service || client->system->auth_cb || client->system->password_hash[0])
  {
    // Use HTTPS since some form of authentication is used...
    return ("https");
  }
  else
  {
    // Use HTTP since no authentication is used...
    return ("http");
  }
}


//
// 'papplClientIsEncrypted()' - Return whether a Client connection is encrypted.
//
// This function returns a boolean value indicating whether a Client connection
// is encrypted with TLS.
//

bool					// O - `true` if encrypted, `false` otherwise
papplClientIsEncrypted(
    pappl_client_t *client)		// I - Client
{
  return (client ? httpIsEncrypted(client->http) : false);
}


//
// 'papplClientSetUsername()' - Set the authenticated username, if any.
//
// This function sets the current authenticated username, if any.
//

void
papplClientSetUsername(
    pappl_client_t *client,		// I - Client
    const char     *username)		// I - Username or `NULL` for none
{
  if (client)
  {
    if (username)
      papplCopyString(client->username, username, sizeof(client->username));
    else
      client->username[0] = '\0';
  }
}
