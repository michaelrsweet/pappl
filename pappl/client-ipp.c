//
// Common client IPP processing for the Printer Application Framework
//
// Copyright © 2019-2023 by Michael R Sweet.
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
// '_papplClientFlushDocumentData()' - Safely flush remaining document data.
//

void
_papplClientFlushDocumentData(
    pappl_client_t *client)		// I - Client
{
  char	buffer[8192];			// Read buffer


  if (httpGetState(client->http) == HTTP_STATE_POST_RECV)
  {
    while (httpRead(client->http, buffer, sizeof(buffer)) > 0)
      ;				// Read all data
  }
}


//
// '_papplClientHaveDocumentData()' - Determine whether we have more document data.
//

bool					// O - `true` if data is present, `false` otherwise
_papplClientHaveDocumentData(
    pappl_client_t *client)		// I - Client
{
  char temp;				// Data


  if (httpGetState(client->http) != HTTP_STATE_POST_RECV)
    return (false);
  else
    return (httpPeek(client->http, &temp, 1) > 0);
}


//
// '_papplClientProcessIPP()' - Process an IPP request.
//

bool					// O - `true` on success, `false` on error
_papplClientProcessIPP(
    pappl_client_t *client)		// I - Client
{
  ipp_tag_t		group;		// Current group tag
  ipp_attribute_t	*attr;		// Current attribute
  ipp_attribute_t	*charset;	// Character set attribute
  ipp_attribute_t	*language;	// Language attribute
  ipp_attribute_t	*uri;		// Printer URI attribute
  int			major, minor;	// Version number
  ipp_op_t		op;		// Operation code
  const char		*name;		// Name of attribute
  bool			printer_op = true;
					// Printer operation?


  // First build an empty response message for this request...
  client->operation_id = ippGetOperation(client->request);
  client->response     = ippNewResponse(client->request);

  // Then validate the request header and required attributes...
  major = ippGetVersion(client->request, &minor);
  op    = ippGetOperation(client->request);

  _papplLogAttributes(client, ippOpString(op), client->request, false);

  if (major < 1 || major > 2)
  {
    // Return an error, since we only support IPP 1.x and 2.x.
    papplClientRespondIPP(client, IPP_STATUS_ERROR_VERSION_NOT_SUPPORTED, "Bad request version number %d.%d.", major, minor);
  }
  else if (ippGetRequestId(client->request) <= 0)
  {
    papplClientRespondIPP(client, IPP_STATUS_ERROR_BAD_REQUEST, "Bad request-id %d.", ippGetRequestId(client->request));
  }
  else if (!ippGetFirstAttribute(client->request))
  {
    papplClientRespondIPP(client, IPP_STATUS_ERROR_BAD_REQUEST, "No attributes in request.");
  }
  else
  {
    // Make sure that the attributes are provided in the correct order and
    // don't repeat groups...
    for (attr = ippGetFirstAttribute(client->request), group = ippGetGroupTag(attr); attr; attr = ippGetNextAttribute(client->request))
    {
      if (ippGetGroupTag(attr) < group && ippGetGroupTag(attr) != IPP_TAG_ZERO)
      {
        // Out of order; return an error...
	papplClientRespondIPP(client, IPP_STATUS_ERROR_BAD_REQUEST, "Attribute groups are out of order (%x < %x).", ippGetGroupTag(attr), group);
	break;
      }
      else
	group = ippGetGroupTag(attr);
    }

    if (!attr)
    {
      // Then make sure that the first three attributes are:
      //
      //   attributes-charset
      //   attributes-natural-language
      //   system-uri/printer-uri/job-uri
      attr = ippGetFirstAttribute(client->request);
      name = ippGetName(attr);
      if (attr && name && !strcmp(name, "attributes-charset") && ippGetValueTag(attr) == IPP_TAG_CHARSET)
	charset = attr;
      else
	charset = NULL;

      attr = ippGetNextAttribute(client->request);
      name = ippGetName(attr);

      if (attr && name && !strcmp(name, "attributes-natural-language") && ippGetValueTag(attr) == IPP_TAG_LANGUAGE)
	language = attr;
      else
	language = NULL;

      if ((attr = ippFindAttribute(client->request, "system-uri", IPP_TAG_URI)) != NULL)
	uri = attr;
      else if ((attr = ippFindAttribute(client->request, "printer-uri", IPP_TAG_URI)) != NULL)
	uri = attr;
      else if ((attr = ippFindAttribute(client->request, "job-uri", IPP_TAG_URI)) != NULL)
	uri = attr;
      else
	uri = NULL;

      client->printer = NULL;
      client->job     = NULL;

      if (charset && strcasecmp(ippGetString(charset, 0, NULL), "us-ascii") && strcasecmp(ippGetString(charset, 0, NULL), "utf-8"))
      {
        // Bad character set...
	papplClientRespondIPP(client, IPP_STATUS_ERROR_BAD_REQUEST, "Unsupported character set \"%s\".", ippGetString(charset, 0, NULL));
      }
      else if (!charset || !language || (!uri && op != IPP_OP_CUPS_GET_DEFAULT && op != IPP_OP_CUPS_GET_PRINTERS))
      {
        // Return an error, since attributes-charset,
	// attributes-natural-language, and system/printer/job-uri are required
	// for all operations.
	papplClientRespondIPP(client, IPP_STATUS_ERROR_BAD_REQUEST, "Missing required attributes.");
      }
      else
      {
        if (uri)
        {
	  char	scheme[32],		// URI scheme
		userpass[32],		// Username/password in URI
		host[256],		// Host name in URI
		resource[256],		// Resource path in URI
		*resptr;		// Pointer into resource
	  int	port,			// Port number in URI
		job_id;			// Job ID

	  name = ippGetName(uri);

	  if (httpSeparateURI(HTTP_URI_CODING_ALL, ippGetString(uri, 0, NULL), scheme, sizeof(scheme), userpass, sizeof(userpass), host, sizeof(host), &port, resource, sizeof(resource)) < HTTP_URI_STATUS_OK)
	  {
	    papplClientRespondIPP(client, IPP_STATUS_ERROR_ATTRIBUTES_OR_VALUES, "Bad %s value '%s'.", name, ippGetString(uri, 0, NULL));
	  }
	  else if (!strcmp(name, "system-uri"))
	  {
	    printer_op = false;

	    if (strcmp(resource, "/ipp/system"))
	    {
	      papplClientRespondIPP(client, IPP_STATUS_ERROR_ATTRIBUTES_OR_VALUES, "Bad %s value '%s'.", name, ippGetString(uri, 0, NULL));
	    }
	    else
	      client->printer = papplSystemFindPrinter(client->system, NULL, ippGetInteger(ippFindAttribute(client->request, "printer-id", IPP_TAG_INTEGER), 0), NULL);
	  }
	  else if ((client->printer = papplSystemFindPrinter(client->system, resource, 0, NULL)) != NULL)
	  {
	    if (!strcmp(name, "job-uri") && (resptr = strrchr(resource, '/')) != NULL)
	    {
	      char *endptr;		// Pointer after job ID

	      job_id = (int)strtol(resptr + 1, &endptr, 10);

	      if (errno == ERANGE || *endptr)
	        job_id = 0;
	    }
	    else
	      job_id = ippGetInteger(ippFindAttribute(client->request, "job-id", IPP_TAG_INTEGER), 0);

	    if (job_id)
	    {
	      if ((client->job = papplPrinterFindJob(client->printer, job_id)) == NULL)
	      {
		papplClientRespondIPP(client, IPP_STATUS_ERROR_NOT_FOUND, "job-id %d not found.", job_id);
	      }
	    }
	  }
	  else
	  {
	    papplClientRespondIPP(client, IPP_STATUS_ERROR_NOT_FOUND, "%s %s not found.", name, ippGetString(uri, 0, NULL));
	  }
        }
        else
          printer_op = false;

	if (ippGetStatusCode(client->response) == IPP_STATUS_OK)
	{
	  papplLogClient(client, PAPPL_LOGLEVEL_DEBUG, "IPP/%d.%d %s (%s)", major, minor, ippOpString(op), httpGetField(client->http, HTTP_FIELD_USER_AGENT));

	  if (printer_op)
	  {
	    // Process job or printer operation...
	    if (client->job)
	      _papplJobProcessIPP(client);
	    else
	      _papplPrinterProcessIPP(client);
	  }
	  else
	  {
	    // Process system operation...
	    _papplSystemProcessIPP(client);
	  }
	}
      }
    }
  }

  // Send the HTTP header and return...
  if (httpGetState(client->http) != HTTP_STATE_POST_SEND)
  {
    // Flush trailing (junk) data
    _papplClientFlushDocumentData(client);
  }

  if (httpGetState(client->http) != HTTP_STATE_WAITING)
    return (papplClientRespond(client, HTTP_STATUS_OK, NULL, "application/ipp", 0, ippGetLength(client->response)));
  else
    return (true);
}


//
// 'papplClientRespondIPP()' - Send an IPP response.
//
// This function sets the return status for an IPP request and returns the
// current IPP response message.  The "status" and "message" arguments replace
// any existing status-code and "status-message" attribute value that may be
// already present in the response.
//
// > Note: You should call this function prior to adding any response
// > attributes.
//

ipp_t *					// O - IPP response message
papplClientRespondIPP(
    pappl_client_t *client,		// I - Client
    ipp_status_t   status,		// I - status-code
    const char     *message,		// I - printf-style status-message
    ...)				// I - Additional args as needed
{
  const char	*formatted = NULL;	// Formatted message


  if (status > ippGetStatusCode(client->response))
    ippSetStatusCode(client->response, status);

  if (message)
  {
    va_list		ap;		// Pointer to additional args
    ipp_attribute_t	*attr;		// New status-message attribute

    va_start(ap, message);
    if ((attr = ippFindAttribute(client->response, "status-message", IPP_TAG_TEXT)) != NULL)
      ippSetStringfv(client->response, &attr, 0, message, ap);
    else
      attr = ippAddStringfv(client->response, IPP_TAG_OPERATION, IPP_TAG_TEXT, "status-message", NULL, message, ap);
    va_end(ap);

    formatted = ippGetString(attr, 0, NULL);
  }

  if (formatted)
    papplLogClient(client, PAPPL_LOGLEVEL_INFO, "%s %s (%s)", ippOpString(client->operation_id), ippErrorString(status), formatted);
  else
    papplLogClient(client, PAPPL_LOGLEVEL_INFO, "%s %s", ippOpString(client->operation_id), ippErrorString(status));

  return (client->response);
}


//
// '_papplClientRespondIPPIgnored()' - Respond with an ignored IPP attribute.
//
// This function returns a 'successful-ok-ignored-or-substituted-attributes'
// status code and adds the specified attribute to the unsupported attributes
// group in the response.
//

void
_papplClientRespondIPPIgnored(
    pappl_client_t  *client,		// I - Client
    ipp_attribute_t *attr)		// I - Atribute
{
  ipp_attribute_t	*temp;		// Copy of attribute


  papplClientRespondIPP(client, IPP_STATUS_OK_IGNORED_OR_SUBSTITUTED, "Ignoring unsupported %s %s%s value.", ippGetName(attr), ippGetCount(attr) > 1 ? "1setOf " : "", ippTagString(ippGetValueTag(attr)));

  temp = ippCopyAttribute(client->response, attr, 0);
  ippSetGroupTag(client->response, &temp, IPP_TAG_UNSUPPORTED_GROUP);
}


//
// 'papplClientRespondIPPUnsupported()' - Respond with an unsupported IPP attribute.
//
// This function returns a 'client-error-attributes-or-values-not-supported'
// status code and adds the specified attribute to the unsupported attributes
// group in the response.
//

void
papplClientRespondIPPUnsupported(
    pappl_client_t  *client,		// I - Client
    ipp_attribute_t *attr)		// I - Atribute
{
  ipp_attribute_t	*temp;		// Copy of attribute


  papplClientRespondIPP(client, IPP_STATUS_ERROR_ATTRIBUTES_OR_VALUES, "Unsupported %s %s%s value.", ippGetName(attr), ippGetCount(attr) > 1 ? "1setOf " : "", ippTagString(ippGetValueTag(attr)));

  temp = ippCopyAttribute(client->response, attr, 0);
  ippSetGroupTag(client->response, &temp, IPP_TAG_UNSUPPORTED_GROUP);
}
