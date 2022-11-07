//
// Client processing code for the Printer Application Framework
//
// Copyright © 2019-2022 by Michael R Sweet.
// Copyright © 2010-2019 by Apple Inc.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

//
// Include necessary headers...
//

#include "pappl-private.h"


//
// Local functions...
//

static bool	eval_if_modified(pappl_client_t *client, _pappl_resource_t *r);


//
// '_papplClientCleanTempFiles()' - Clean temporary files...
//

void
_papplClientCleanTempFiles(
    pappl_client_t *client)		// I - Client
{
  int	i;				// Looping var


  for (i = 0; i < client->num_files; i ++)
  {
    unlink(client->files[i]);
    free(client->files[i]);
  }

  client->num_files = 0;
}


//
// '_papplClientCreate()' - Accept a new network connection and create a client
//                          object.
//
// The new network connection is accepted from the specified listen socket.
// The client object is managed by the system and is automatically freed when
// the connection is closed.
//
// > Note: This function is normally only called from @link papplSystemRun@.
//

pappl_client_t *			// O - Client
_papplClientCreate(
    pappl_system_t *system,		// I - Printer
    int            sock)		// I - Listen socket
{
  pappl_client_t	*client;	// Client


  if ((client = calloc(1, sizeof(pappl_client_t))) == NULL)
  {
    papplLog(system, PAPPL_LOGLEVEL_ERROR, "Unable to allocate memory for client connection: %s", strerror(errno));
    return (NULL);
  }

  client->system = system;

  _papplRWLockWrite(system);
  client->number = system->next_client ++;
  _papplRWUnlock(system);

  // Accept the client and get the remote address...
  if ((client->http = httpAcceptConnection(sock, 1)) == NULL)
  {
    papplLog(system, PAPPL_LOGLEVEL_ERROR, "Unable to accept client connection: %s", strerror(errno));
    free(client);
    return (NULL);
  }

  httpGetHostname(client->http, client->hostname, sizeof(client->hostname));

  papplLogClient(client, PAPPL_LOGLEVEL_INFO, "Accepted connection from '%s'.", client->hostname);

  return (client);
}


//
// '_papplClientCreateTempFile()' - Create a temporary file.
//

char *					// O - Temporary filename or `NULL` on error
_papplClientCreateTempFile(
    pappl_client_t *client,		// I - Client
    const void     *data,		// I - Data
    size_t         datasize)		// I - Size of data
{
  int	fd;				// File descriptor
  char	tempfile[1024];			// Temporary filename


  // See if we have room for another temp file...
  if (client->num_files >= (int)(sizeof(client->files) / sizeof(client->files[0])))
  {
    papplLogClient(client, PAPPL_LOGLEVEL_ERROR, "Too many temporary files.");
    return (NULL);
  }

  // Write the data to a temporary file...
  if ((fd = cupsTempFd(NULL, NULL, tempfile, sizeof(tempfile))) < 0)
  {
    papplLogClient(client, PAPPL_LOGLEVEL_ERROR, "Unable to create temporary file: %s", strerror(errno));
    return (NULL);
  }

  if (write(fd, data, datasize) < 0)
  {
    papplLogClient(client, PAPPL_LOGLEVEL_ERROR, "Unable to write to temporary file: %s", strerror(errno));
    close(fd);
    unlink(tempfile);
    return (NULL);
  }

  close(fd);

  if ((client->files[client->num_files] = strdup(tempfile)) != NULL)
    client->num_files ++;
  else
    unlink(tempfile);

  if (client->num_files > 0)
    return (client->files[client->num_files - 1]);
  else
    return (NULL);
}


//
// '_papplClientDelete()' - Close the client connection and free all memory used
//                          by a client object.
//
// > Note: This function is normally only called by
//

void
_papplClientDelete(
    pappl_client_t *client)		// I - Client
{
  pappl_system_t *system = client->system;
					// System


  papplLogClient(client, PAPPL_LOGLEVEL_INFO, "Closing connection from '%s'.", client->hostname);

  // Flush pending writes before closing...
  httpFlushWrite(client->http);

  _papplClientCleanTempFiles(client);

  // Free memory...
  httpClose(client->http);

  ippDelete(client->request);
  ippDelete(client->response);

  free(client);

  // Update the number of active clients...
  _papplRWLockWrite(system);
  system->num_clients --;
  _papplRWUnlock(system);
}


//
// '_papplClientProcessHTTP()' - Process a HTTP request.
//

bool					// O - `true` on success, `false` on failure
_papplClientProcessHTTP(
    pappl_client_t *client)		// I - Client connection
{
  char			uri[1024];	// URI
  http_state_t		http_state;	// HTTP state
  http_status_t		http_status;	// HTTP status
  http_version_t	http_version;	// HTTP version
  ipp_state_t		ipp_state;	// State of IPP transfer
  char			scheme[32],	// Method/scheme
			userpass[128],	// Username:password
			hostname[HTTP_MAX_HOST];
					// Hostname
  int			port;		// Port number
  char			*ptr;		// Pointer into string
  _pappl_resource_t	*resource;	// Current resource
  char			system_host[HTTP_MAX_HOST];
					// System hostname


  // Clear state variables...
  ippDelete(client->request);
  ippDelete(client->response);

  client->loc       = NULL;
  client->request   = NULL;
  client->response  = NULL;
  client->operation = HTTP_STATE_WAITING;

  // Read a request from the connection...
  while ((http_state = httpReadRequest(client->http, uri, sizeof(uri))) == HTTP_STATE_WAITING)
    usleep(1);

  // Parse the request line...
  if (http_state == HTTP_STATE_ERROR)
  {
    if (httpError(client->http) != EPIPE && httpError(client->http))
      papplLogClient(client, PAPPL_LOGLEVEL_DEBUG, "Bad request line (%s).", strerror(httpError(client->http)));

    return (false);
  }
  else if (http_state == HTTP_STATE_UNKNOWN_METHOD)
  {
    papplLogClient(client, PAPPL_LOGLEVEL_ERROR, "Bad/unknown operation.");
    papplClientRespond(client, HTTP_STATUS_BAD_REQUEST, NULL, NULL, 0, 0);
    return (false);
  }
  else if (http_state == HTTP_STATE_UNKNOWN_VERSION)
  {
    papplLogClient(client, PAPPL_LOGLEVEL_ERROR, "Bad HTTP version.");
    papplClientRespond(client, HTTP_STATUS_BAD_REQUEST, NULL, NULL, 0, 0);
    return (false);
  }

  // Separate the URI into its components...
  if (httpSeparateURI(HTTP_URI_CODING_MOST, uri, scheme, sizeof(scheme), userpass, sizeof(userpass), hostname, sizeof(hostname), &port, client->uri, sizeof(client->uri)) < HTTP_URI_STATUS_OK && (http_state != HTTP_STATE_OPTIONS || strcmp(uri, "*")))
  {
    papplLogClient(client, PAPPL_LOGLEVEL_ERROR, "Bad URI '%s'.", uri);
    papplClientRespond(client, HTTP_STATUS_BAD_REQUEST, NULL, NULL, 0, 0);
    return (false);
  }

  if ((client->options = strchr(client->uri, '?')) != NULL)
    *(client->options)++ = '\0';

  // Process the request...
  client->start     = time(NULL);
  client->operation = httpGetState(client->http);

  // Parse incoming parameters until the status changes...
  while ((http_status = httpUpdate(client->http)) == HTTP_STATUS_CONTINUE)
    ;					// Read all HTTP headers...

  if (http_status != HTTP_STATUS_OK)
  {
    papplLogClient(client, PAPPL_LOGLEVEL_DEBUG, "http_status=%d", http_status);
    papplClientRespond(client, HTTP_STATUS_BAD_REQUEST, NULL, NULL, 0, 0);
    return (false);
  }

  http_version = httpGetVersion(client->http);

  papplCopyString(client->language, httpGetField(client->http, HTTP_FIELD_ACCEPT_LANGUAGE), sizeof(client->language));

  papplLogClient(client, PAPPL_LOGLEVEL_INFO, "%s %s://%s%s HTTP/%d.%d (%s)", httpStateString(http_state), httpIsEncrypted(client->http) ? "https" : "http", httpGetField(client->http, HTTP_FIELD_HOST), uri, http_version / 100, http_version % 100, client->language);

  // Validate the host header...
  if (!httpGetField(client->http, HTTP_FIELD_HOST)[0] &&
      httpGetVersion(client->http) >= HTTP_VERSION_1_1)
  {
    // HTTP/1.1 and higher require the "Host:" field...
    papplClientRespond(client, HTTP_STATUS_BAD_REQUEST, NULL, NULL, 0, 0);
    return (false);
  }

  papplCopyString(client->host_field, httpGetField(client->http, HTTP_FIELD_HOST), sizeof(client->host_field));
  if ((ptr = strrchr(client->host_field, ':')) != NULL)
  {
    char *end;				// End of port number

    // Grab port number from Host: header...
    *ptr++ = '\0';
    client->host_port = (int)strtol(ptr, &end, 10);

    if (errno == ERANGE || *end)
      client->host_port = papplSystemGetHostPort(client->system);
  }
  else
  {
    // Use the default port number...
    client->host_port = papplSystemGetHostPort(client->system);
  }

  ptr = strstr(client->host_field, ".local");

  if (!isdigit(client->host_field[0] & 255) && client->host_field[0] != '[' && strcmp(client->host_field, papplSystemGetHostName(client->system, system_host, sizeof(system_host))) && strcmp(client->host_field, "localhost") && (!ptr || (strcmp(ptr, ".local") && strcmp(ptr, ".local."))))
  {
    papplLogClient(client, PAPPL_LOGLEVEL_ERROR, "Bad Host: header '%s'.", client->host_field);
    papplClientRespond(client, HTTP_STATUS_BAD_REQUEST, NULL, NULL, 0, 0);
    return (false);
  }

  // Handle HTTP Upgrade...
  if (!strcasecmp(httpGetField(client->http, HTTP_FIELD_CONNECTION), "Upgrade"))
  {
    if (strstr(httpGetField(client->http, HTTP_FIELD_UPGRADE), "TLS/") != NULL && !httpIsEncrypted(client->http) && !(client->system->options & PAPPL_SOPTIONS_NO_TLS))
    {
      if (!papplClientRespond(client, HTTP_STATUS_SWITCHING_PROTOCOLS, NULL, NULL, 0, 0))
        return (false);

      papplLogClient(client, PAPPL_LOGLEVEL_INFO, "Upgrading to encrypted connection.");

      if (!httpSetEncryption(client->http, HTTP_ENCRYPTION_REQUIRED))
      {
	papplLogClient(client, PAPPL_LOGLEVEL_ERROR, "Unable to encrypt connection: %s", cupsLastErrorString());
	return (false);
      }

      papplLogClient(client, PAPPL_LOGLEVEL_INFO, "Connection now encrypted.");
    }
    else if (!papplClientRespond(client, HTTP_STATUS_NOT_IMPLEMENTED, NULL, NULL, 0, 0))
      return (false);
  }

  // Handle HTTP Expect...
  if (httpGetExpect(client->http) && (client->operation == HTTP_STATE_POST || client->operation == HTTP_STATE_PUT))
  {
    if (httpGetExpect(client->http) == HTTP_STATUS_CONTINUE)
    {
      // Send 100-continue header...
      if (!papplClientRespond(client, HTTP_STATUS_CONTINUE, NULL, NULL, 0, 0))
	return (false);
    }
    else
    {
      // Send 417-expectation-failed header...
      if (!papplClientRespond(client, HTTP_STATUS_EXPECTATION_FAILED, NULL, NULL, 0, 0))
	return (false);
    }
  }

  // Handle new transfers...
  switch (client->operation)
  {
    case HTTP_STATE_OPTIONS :
        // Do OPTIONS command...
	return (papplClientRespond(client, HTTP_STATUS_OK, NULL, NULL, 0, 0));

    case HTTP_STATE_HEAD :
        // See if we have a matching resource to serve...
        if ((resource = _papplSystemFindResourceForPath(client->system, client->uri)) != NULL)
        {
          if (eval_if_modified(client, resource))
	    return (papplClientRespond(client, HTTP_STATUS_OK, NULL, resource->format, resource->last_modified, 0));
          else
            return (papplClientRespond(client, HTTP_STATUS_NOT_MODIFIED, NULL, NULL, resource->last_modified, 0));
	}

        // If we get here the resource wasn't found...
	return (papplClientRespond(client, HTTP_STATUS_NOT_FOUND, NULL, NULL, 0, 0));

    case HTTP_STATE_GET :
        // See if we have a matching resource to serve...
        if ((resource = _papplSystemFindResourceForPath(client->system, client->uri)) != NULL)
        {
          if (!eval_if_modified(client, resource))
          {
            return (papplClientRespond(client, HTTP_STATUS_NOT_MODIFIED, NULL, NULL, resource->last_modified, 0));
          }
          else if (resource->cb)
          {
            // Send output of a callback...
            return ((resource->cb)(client, resource->cbdata));
	  }
	  else if (resource->filename)
	  {
	    // Send an external file...
	    int		fd;		// Resource file descriptor
	    char	buffer[8192];	// Copy buffer
	    ssize_t	bytes;		// Bytes read/written

            if ((fd = open(resource->filename, O_RDONLY)) >= 0)
	    {
	      if (!papplClientRespond(client, HTTP_STATUS_OK, NULL, resource->format, resource->last_modified, 0))
	      {
	        close(fd);
		return (false);
	      }

              while ((bytes = read(fd, buffer, sizeof(buffer))) > 0)
                httpWrite(client->http, buffer, (size_t)bytes);

	      httpWrite(client->http, "", 0);

	      close(fd);

	      return (true);
	    }
	  }
	  else
	  {
	    // Send a static resource file...
	    if (!papplClientRespond(client, HTTP_STATUS_OK, NULL, resource->format, resource->last_modified, resource->length))
	      return (false);

	    httpWrite(client->http, (const char *)resource->data, resource->length);
	    httpFlushWrite(client->http);
	    return (true);
	  }
	}

        // If we get here then the resource wasn't found...
	return (papplClientRespond(client, HTTP_STATUS_NOT_FOUND, NULL, NULL, 0, 0));

    case HTTP_STATE_POST :
        if (!strcmp(httpGetField(client->http, HTTP_FIELD_CONTENT_TYPE), "application/ipp"))
        {
	  // Read the IPP request...
	  client->request = ippNew();

	  while ((ipp_state = ippRead(client->http, client->request)) != IPP_STATE_DATA)
	  {
	    if (ipp_state == IPP_STATE_ERROR)
	    {
	      papplLogClient(client, PAPPL_LOGLEVEL_ERROR, "IPP read error (%s).", cupsLastErrorString());
	      papplClientRespond(client, HTTP_STATUS_BAD_REQUEST, NULL, NULL, 0, 0);
	      return (false);
	    }
	  }

	  // Now that we have the IPP request, process the request...
	  return (_papplClientProcessIPP(client));
	}
	else if ((resource = _papplSystemFindResourceForPath(client->system, client->uri)) != NULL)
        {
	  // Serve a matching resource...
          if (resource->cb)
          {
            // Handle a post request through the callback...
            return ((resource->cb)(client, resource->cbdata));
          }
          else
          {
            // Otherwise you can't POST to a resource...
	    return (papplClientRespond(client, HTTP_STATUS_BAD_REQUEST, NULL, NULL, 0, 0));
          }
        }
        else
        {
	  // Not an IPP request or form, return an error...
	  return (papplClientRespond(client, HTTP_STATUS_BAD_REQUEST, NULL, NULL, 0, 0));
	}

    default :
        break; // Anti-compiler-warning-code
  }

  return (true);
}


//
// 'papplClientRespond()' - Send a regular HTTP response.
//
// This function sends all of the required HTTP fields and includes standard
// messages for errors.  The following values for "code" are explicitly
// supported:
//
// - `HTTP_STATUS_OK`: The request is successful.
// - `HTTP_STATUS_BAD_REQUEST`: The client submitted a bad request.
// - `HTTP_STATUS_CONTINUE`: An authentication challenge is not needed.
// - `HTTP_STATUS_FORBIDDEN`: Authenticated but not allowed.
// - `HTTP_STATUS_METHOD_NOT_ALLOWED`: The HTTP method is not supported for the
//   given URI.
// - `HTTP_STATUS_UNAUTHORIZED`: Not authenticated.
// - `HTTP_STATUS_UPGRADE_REQUIRED`: Redirects the client to a secure page.
//
// Use the @link papplClientRespondRedirect@ when you need to redirect the
// client to another page.
//

bool					// O - `true` on success, `false` on failure
papplClientRespond(
    pappl_client_t *client,		// I - Client
    http_status_t  code,		// I - HTTP status of response
    const char     *content_encoding,	// I - Content-Encoding of response
    const char     *type,		// I - MIME media type of response
    time_t         last_modified,	// I - Last-Modified date/time or `0` for none
    size_t         length)		// I - Length of response or `0` for variable-length
{
  char	message[1024],			// Text message
	last_str[256];			// Date string


  if (type)
    papplLogClient(client, PAPPL_LOGLEVEL_INFO, "%s %s %d", httpStatusString(code), type, (int)length);
  else
    papplLogClient(client, PAPPL_LOGLEVEL_INFO, "%s", httpStatusString(code));

  if (code == HTTP_STATUS_CONTINUE)
  {
    // 100-continue doesn't send any headers...
    return (httpWriteResponse(client->http, HTTP_STATUS_CONTINUE));
  }

  // Format an error message...
  if (!type && !length && code != HTTP_STATUS_OK && code != HTTP_STATUS_SWITCHING_PROTOCOLS)
  {
    snprintf(message, sizeof(message), "%d - %s\n", code, httpStatusString(code));

    type   = "text/plain";
    length = strlen(message);
  }
  else
    message[0] = '\0';

  // Send the HTTP response header...
  httpClearFields(client->http);
  httpSetField(client->http, HTTP_FIELD_SERVER, papplSystemGetServerHeader(client->system));
  if (last_modified)
    httpSetField(client->http, HTTP_FIELD_LAST_MODIFIED, httpGetDateString(last_modified, last_str, sizeof(last_str)));

  if (code == HTTP_STATUS_METHOD_NOT_ALLOWED || client->operation == HTTP_STATE_OPTIONS)
    httpSetField(client->http, HTTP_FIELD_ALLOW, "GET, HEAD, OPTIONS, POST");

  if (code == HTTP_STATUS_UNAUTHORIZED)
  {
    char	value[HTTP_MAX_VALUE];	// WWW-Authenticate value

    snprintf(value, sizeof(value), "%s realm=\"%s\"", client->system->auth_scheme ? client->system->auth_scheme : "Basic", client->system->name);
    httpSetField(client->http, HTTP_FIELD_WWW_AUTHENTICATE, value);
  }

  if (type)
  {
    if (!strcmp(type, "text/html"))
      httpSetField(client->http, HTTP_FIELD_CONTENT_TYPE, "text/html; charset=utf-8");
    else
      httpSetField(client->http, HTTP_FIELD_CONTENT_TYPE, type);

    if (content_encoding)
      httpSetField(client->http, HTTP_FIELD_CONTENT_ENCODING, content_encoding);
  }

  httpSetLength(client->http, length);

  if (code == HTTP_STATUS_UPGRADE_REQUIRED && client->operation == HTTP_STATE_GET)
  {
    char	redirect[1024];		// Redirect URI

    code = HTTP_STATUS_MOVED_PERMANENTLY;

    httpAssembleURI(HTTP_URI_CODING_ALL, redirect, sizeof(redirect), "https", NULL, client->host_field, client->host_port, client->uri);
    httpSetField(client->http, HTTP_FIELD_LOCATION, redirect);
  }

  if (!httpWriteResponse(client->http, code))
    return (false);

  // Send the response data...
  if (message[0])
  {
    // Send a plain text message.
    if (httpPrintf(client->http, "%s", message) < 0)
      return (false);

    if (httpWrite(client->http, "", 0) < 0)
      return (false);
  }
  else if (client->response)
  {
    // Send an IPP response...
    _papplLogAttributes(client, ippOpString(client->operation_id), client->response, true);

    ippSetState(client->response, IPP_STATE_IDLE);

    if (ippWrite(client->http, client->response) != IPP_STATE_DATA)
      return (false);
  }

  return (true);
}


//
// 'papplClientRespondRedirect()' - Respond with a redirect to another page.
//
// This function sends a HTTP response that redirects the client to another
// page or URL.  The most common "code" value to return is `HTTP_STATUS_FOUND`.
//

bool					// O - `true` on success, `false` otherwise
papplClientRespondRedirect(
    pappl_client_t *client,		// I - Client
    http_status_t  code,		// I - `HTTP_STATUS_MOVED_PERMANENTLY` or `HTTP_STATUS_FOUND`
    const char     *path)		// I - Redirection path/URL
{
  papplLogClient(client, PAPPL_LOGLEVEL_INFO, "%s %s", httpStatusString(code), path);

  // Send the HTTP response header...
  httpClearFields(client->http);
  httpSetField(client->http, HTTP_FIELD_SERVER, papplSystemGetServerHeader(client->system));
  httpSetLength(client->http, 0);

  if (*path == '/' || !strchr(path, ':'))
  {
    // Generate an absolute URL...
    char	url[1024];		// Absolute URL

    if (*path == '/')
      httpAssembleURI(HTTP_URI_CODING_ALL, url, sizeof(url), httpIsEncrypted(client->http) ? "https" : "http", NULL, client->host_field, client->host_port, path);
    else
      httpAssembleURIf(HTTP_URI_CODING_ALL, url, sizeof(url), httpIsEncrypted(client->http) ? "https" : "http", NULL, client->host_field, client->host_port, "/%s", path);

    httpSetField(client->http, HTTP_FIELD_LOCATION, url);
  }
  else
  {
    // The path is already an absolute URL...
    httpSetField(client->http, HTTP_FIELD_LOCATION, path);
  }

  if (!httpWriteResponse(client->http, code))
    return (false);

  return (httpWrite(client->http, "", 0) >= 0);
}


//
// '_papplClientRun()' - Process client requests on a thread.
//

void *					// O - Exit status
_papplClientRun(
    pappl_client_t *client)		// I - Client
{
  int first_time = 1;			// First time request?


  // Loop until we are out of requests or timeout (30 seconds)...
  while (httpWait(client->http, 30000))
  {
    if (first_time && !(client->system->options & PAPPL_SOPTIONS_NO_TLS))
    {
      // See if we need to negotiate a TLS connection...
      char buf[1];			// First byte from client

      if (recv(httpGetFd(client->http), buf, 1, MSG_PEEK) == 1 && (!buf[0] || !strchr("DGHOPT", buf[0])))
      {
        papplLogClient(client, PAPPL_LOGLEVEL_INFO, "Starting HTTPS session.");

	if (!httpSetEncryption(client->http, HTTP_ENCRYPTION_ALWAYS))
	{
          papplLogClient(client, PAPPL_LOGLEVEL_ERROR, "Unable to encrypt connection: %s", cupsLastErrorString());
	  break;
        }

        papplLogClient(client, PAPPL_LOGLEVEL_INFO, "Connection now encrypted.");
      }

      first_time = 0;
    }

    if (!_papplClientProcessHTTP(client))
      break;

    _papplClientCleanTempFiles(client);
  }

  // Close the conection to the client and return...
  _papplClientDelete(client);

  return (NULL);
}


//
// 'eval_if_modified()' - Evaluate an "If-Modified-Since" header.
//

static bool				// O - `true` if modified, `false` otherwise
eval_if_modified(
    pappl_client_t    *client,		// I - Client
    _pappl_resource_t *r)		// I - Resource
{
  const char	*ptr;			// Pointer into field
  time_t	date = 0;		// Time/date value
  off_t		size = 0;		// Size/length value


  // Dynamic content always needs to be updated...
  if (r->cb)
    return (true);

  // Get "If-Modified-Since:" header
  ptr = httpGetField(client->http, HTTP_FIELD_IF_MODIFIED_SINCE);

  if (*ptr == '\0')
    return (true);

  // Decode the If-Modified-Since: header...
  while (*ptr != '\0')
  {
    while (isspace(*ptr) || *ptr == ';')
      ptr ++;

    if (!strncasecmp(ptr, "length=", 7))
    {
      char	*next;			// Next character

      size = (off_t)strtoll(ptr + 7, &next, 10);
      if (!next)
      {
        papplLogClient(client, PAPPL_LOGLEVEL_DEBUG, "If-Modified-Since: Bad length.");
        return (true);
      }

      ptr = next;
    }
    else if (isalpha(*ptr & 255))
    {
      date = httpGetDateTime(ptr);
      while (*ptr != '\0' && *ptr != ';')
        ptr ++;
    }
    else
      ptr ++;
  }

  // Return the evaluation based on the last modified date, time, and size...
  return ((size != 0 && size != (off_t)r->length) || (date != 0 && date < r->last_modified) || (size == 0 && date == 0));
}
