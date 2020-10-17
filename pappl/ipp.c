//
// IPP processing for the Printer Application Framework
//
// Copyright © 2019-2020 by Michael R Sweet.
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
// Local type...
//

typedef struct _pappl_attr_s		// Input attribute structure
{
  const char	*name;			// Attribute name
  ipp_tag_t	value_tag;		// Value tag
  int		max_count;		// Max number of values
} _pappl_attr_t;


//
// Local functions...
//

static void		copy_job_attributes(pappl_client_t *client, pappl_job_t *job, cups_array_t *ra);
static void		copy_printer_attributes(pappl_client_t *client, pappl_printer_t *printer, cups_array_t *ra);
static void		copy_printer_state(ipp_t *ipp, pappl_printer_t *printer, cups_array_t *ra);
static void		copy_printer_xri(pappl_client_t *client, ipp_t *ipp, pappl_printer_t *printer);
static pappl_job_t	*create_job(pappl_client_t *client);
static void		finish_document_data(pappl_client_t *client, pappl_job_t *job);
static void		flush_document_data(pappl_client_t *client);
static bool		have_document_data(pappl_client_t *client);

static void		ipp_cancel_job(pappl_client_t *client);
static void		ipp_cancel_jobs(pappl_client_t *client);
static void		ipp_close_job(pappl_client_t *client);
static void		ipp_create_job(pappl_client_t *client);
static void		ipp_create_printer(pappl_client_t *client);
static void		ipp_delete_printer(pappl_client_t *client);
static void		ipp_get_job_attributes(pappl_client_t *client);
static void		ipp_get_jobs(pappl_client_t *client);
static void		ipp_get_printer_attributes(pappl_client_t *client);
static void		ipp_get_printers(pappl_client_t *client);
static void		ipp_get_system_attributes(pappl_client_t *client);
static void		ipp_identify_printer(pappl_client_t *client);
static void		ipp_pause_printer(pappl_client_t *client);
static void		ipp_print_job(pappl_client_t *client);
static void		ipp_resume_printer(pappl_client_t *client);
static void		ipp_send_document(pappl_client_t *client);
static void		ipp_set_printer_attributes(pappl_client_t *client);
static void		ipp_set_system_attributes(pappl_client_t *client);
static void		ipp_shutdown_all_printers(pappl_client_t *client);
static void		ipp_validate_job(pappl_client_t *client);

static void		respond_unsupported(pappl_client_t *client, ipp_attribute_t *attr);

static int		set_printer_attributes(pappl_client_t *client, pappl_printer_t *printer);

static int		valid_doc_attributes(pappl_client_t *client);
static int		valid_job_attributes(pappl_client_t *client);


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
  else if (!ippFirstAttribute(client->request))
  {
    papplClientRespondIPP(client, IPP_STATUS_ERROR_BAD_REQUEST, "No attributes in request.");
  }
  else
  {
    // Make sure that the attributes are provided in the correct order and
    // don't repeat groups...
    for (attr = ippFirstAttribute(client->request), group = ippGetGroupTag(attr); attr; attr = ippNextAttribute(client->request))
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
      attr = ippFirstAttribute(client->request);
      name = ippGetName(attr);
      if (attr && name && !strcmp(name, "attributes-charset") && ippGetValueTag(attr) == IPP_TAG_CHARSET)
	charset = attr;
      else
	charset = NULL;

      attr = ippNextAttribute(client->request);
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
	      job_id = atoi(resptr + 1);
	    else
	      job_id = ippGetInteger(ippFindAttribute(client->request, "job-id", IPP_TAG_INTEGER), 0);

	    if (job_id)
	      client->job = papplPrinterFindJob(client->printer, job_id);
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
	  if (printer_op)
	  {
	    // Try processing the printer operation...
	    switch (ippGetOperation(client->request))
	    {
	      case IPP_OP_PRINT_JOB :
		  ipp_print_job(client);
		  break;

	      case IPP_OP_VALIDATE_JOB :
		  ipp_validate_job(client);
		  break;

	      case IPP_OP_CREATE_JOB :
		  ipp_create_job(client);
		  break;

	      case IPP_OP_SEND_DOCUMENT :
		  ipp_send_document(client);
		  break;

	      case IPP_OP_CANCEL_JOB :
	      case IPP_OP_CANCEL_CURRENT_JOB :
		  ipp_cancel_job(client);
		  break;

	      case IPP_OP_CANCEL_JOBS :
	      case IPP_OP_CANCEL_MY_JOBS :
		  ipp_cancel_jobs(client);
		  break;

	      case IPP_OP_GET_JOB_ATTRIBUTES :
		  ipp_get_job_attributes(client);
		  break;

	      case IPP_OP_GET_JOBS :
		  ipp_get_jobs(client);
		  break;

	      case IPP_OP_GET_PRINTER_ATTRIBUTES :
		  ipp_get_printer_attributes(client);
		  break;

	      case IPP_OP_SET_PRINTER_ATTRIBUTES :
		  ipp_set_printer_attributes(client);
		  break;

	      case IPP_OP_CLOSE_JOB :
		  ipp_close_job(client);
		  break;

	      case IPP_OP_IDENTIFY_PRINTER :
		  ipp_identify_printer(client);
		  break;

              case IPP_OP_PAUSE_PRINTER :
                  ipp_pause_printer(client);
                  break;

              case IPP_OP_RESUME_PRINTER :
                  ipp_resume_printer(client);
                  break;

	      default :
		  papplClientRespondIPP(client, IPP_STATUS_ERROR_OPERATION_NOT_SUPPORTED, "Operation not supported.");
		  break;
	    }
	  }
	  else
	  {
	    // Try processing the system operation...
	    switch (ippGetOperation(client->request))
	    {
	      case IPP_OP_CREATE_PRINTER :
	          ipp_create_printer(client);
	          break;

	      case IPP_OP_DELETE_PRINTER :
	          ipp_delete_printer(client);
	          break;

              case IPP_OP_GET_PRINTERS :
              case IPP_OP_CUPS_GET_PRINTERS :
		  ipp_get_printers(client);
		  break;

	      case IPP_OP_GET_PRINTER_ATTRIBUTES :
	      case IPP_OP_CUPS_GET_DEFAULT :
                  client->printer = papplSystemFindPrinter(client->system, NULL, client->system->default_printer_id, NULL);
		  ipp_get_printer_attributes(client);
		  break;

	      case IPP_OP_GET_SYSTEM_ATTRIBUTES :
		  ipp_get_system_attributes(client);
		  break;

	      case IPP_OP_SET_SYSTEM_ATTRIBUTES :
		  ipp_set_system_attributes(client);
		  break;

	      case IPP_OP_SHUTDOWN_ALL_PRINTERS :
	          ipp_shutdown_all_printers(client);
	          break;

	      default :
		  papplClientRespondIPP(client, IPP_STATUS_ERROR_OPERATION_NOT_SUPPORTED, "Operation not supported.");
		  break;
	    }
	  }
	}
      }
    }
  }

  // Send the HTTP header and return...
  if (httpGetState(client->http) != HTTP_STATE_POST_SEND)
    flush_document_data(client);	// Flush trailing (junk) data

  return (papplClientRespondHTTP(client, HTTP_STATUS_OK, NULL, "application/ipp", 0, ippLength(client->response)));
}


//
// 'papplClientRespondIPP()' - Send an IPP response.
//

void
papplClientRespondIPP(
    pappl_client_t *client,		// I - Client
    ipp_status_t    status,		// I - status-code
    const char      *message,		// I - printf-style status-message
    ...)				// I - Additional args as needed
{
  const char	*formatted = NULL;	// Formatted message


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
}


//
// 'copy_job_attrs()' - Copy job attributes to the response.
//

static void
copy_job_attributes(
    pappl_client_t *client,		// I - Client
    pappl_job_t    *job,		// I - Job
    cups_array_t  *ra)			// I - requested-attributes
{
  _papplCopyAttributes(client->response, job->attrs, ra, IPP_TAG_JOB, 0);

  if (!ra || cupsArrayFind(ra, "date-time-at-creation"))
    ippAddDate(client->response, IPP_TAG_JOB, "date-time-at-creation", ippTimeToDate(job->created));

  if (!ra || cupsArrayFind(ra, "date-time-at-completed"))
  {
    if (job->completed)
      ippAddDate(client->response, IPP_TAG_JOB, "date-time-at-completed", ippTimeToDate(job->completed));
    else
      ippAddOutOfBand(client->response, IPP_TAG_JOB, IPP_TAG_NOVALUE, "date-time-at-completed");
  }

  if (!ra || cupsArrayFind(ra, "date-time-at-processing"))
  {
    if (job->processing)
      ippAddDate(client->response, IPP_TAG_JOB, "date-time-at-processing", ippTimeToDate(job->processing));
    else
      ippAddOutOfBand(client->response, IPP_TAG_JOB, IPP_TAG_NOVALUE, "date-time-at-processing");
  }

  if (!ra || cupsArrayFind(ra, "job-impressions"))
    ippAddInteger(client->response, IPP_TAG_JOB, IPP_TAG_INTEGER, "job-impressions", job->impressions);

  if (!ra || cupsArrayFind(ra, "job-impressions-completed"))
    ippAddInteger(client->response, IPP_TAG_JOB, IPP_TAG_INTEGER, "job-impressions-completed", job->impcompleted);

  if (!ra || cupsArrayFind(ra, "job-printer-up-time"))
    ippAddInteger(client->response, IPP_TAG_JOB, IPP_TAG_INTEGER, "job-printer-up-time", (int)(time(NULL) - client->printer->start_time));

  if (!ra || cupsArrayFind(ra, "job-state"))
    ippAddInteger(client->response, IPP_TAG_JOB, IPP_TAG_ENUM, "job-state", (int)job->state);

  if (!ra || cupsArrayFind(ra, "job-state-message"))
  {
    if (job->message)
    {
      ippAddString(client->response, IPP_TAG_JOB, IPP_TAG_TEXT, "job-state-message", NULL, job->message);
    }
    else
    {
      switch (job->state)
      {
	case IPP_JSTATE_PENDING :
	    ippAddString(client->response, IPP_TAG_JOB, IPP_CONST_TAG(IPP_TAG_TEXT), "job-state-message", NULL, "Job pending.");
	    break;

	case IPP_JSTATE_HELD :
	    if (job->fd >= 0)
	      ippAddString(client->response, IPP_TAG_JOB, IPP_CONST_TAG(IPP_TAG_TEXT), "job-state-message", NULL, "Job incoming.");
	    else if (ippFindAttribute(job->attrs, "job-hold-until", IPP_TAG_ZERO))
	      ippAddString(client->response, IPP_TAG_JOB, IPP_CONST_TAG(IPP_TAG_TEXT), "job-state-message", NULL, "Job held.");
	    else
	      ippAddString(client->response, IPP_TAG_JOB, IPP_CONST_TAG(IPP_TAG_TEXT), "job-state-message", NULL, "Job created.");
	    break;

	case IPP_JSTATE_PROCESSING :
	    if (job->is_canceled)
	      ippAddString(client->response, IPP_TAG_JOB, IPP_CONST_TAG(IPP_TAG_TEXT), "job-state-message", NULL, "Job canceling.");
	    else
	      ippAddString(client->response, IPP_TAG_JOB, IPP_CONST_TAG(IPP_TAG_TEXT), "job-state-message", NULL, "Job printing.");
	    break;

	case IPP_JSTATE_STOPPED :
	    ippAddString(client->response, IPP_TAG_JOB, IPP_CONST_TAG(IPP_TAG_TEXT), "job-state-message", NULL, "Job stopped.");
	    break;

	case IPP_JSTATE_CANCELED :
	    ippAddString(client->response, IPP_TAG_JOB, IPP_CONST_TAG(IPP_TAG_TEXT), "job-state-message", NULL, "Job canceled.");
	    break;

	case IPP_JSTATE_ABORTED :
	    ippAddString(client->response, IPP_TAG_JOB, IPP_CONST_TAG(IPP_TAG_TEXT), "job-state-message", NULL, "Job aborted.");
	    break;

	case IPP_JSTATE_COMPLETED :
	    ippAddString(client->response, IPP_TAG_JOB, IPP_CONST_TAG(IPP_TAG_TEXT), "job-state-message", NULL, "Job completed.");
	    break;
      }
    }
  }

  if (!ra || cupsArrayFind(ra, "job-state-reasons"))
  {
    if (job->state_reasons)
    {
      int		num_values = 0;	// Number of string values
      const char	*svalues[32];	// String values
      pappl_jreason_t	bit;		// Current reason bit

      for (bit = PAPPL_JREASON_ABORTED_BY_SYSTEM; bit <= PAPPL_JREASON_WARNINGS_DETECTED; bit *= 2)
      {
        if (bit & job->state_reasons)
          svalues[num_values ++] = _papplJobReasonString(bit);
      }

      ippAddStrings(client->response, IPP_TAG_JOB, IPP_CONST_TAG(IPP_TAG_KEYWORD), "job-state-reasons", num_values, NULL, svalues);
    }
    else
    {
      switch (job->state)
      {
	case IPP_JSTATE_PENDING :
	    ippAddString(client->response, IPP_TAG_JOB, IPP_CONST_TAG(IPP_TAG_KEYWORD), "job-state-reasons", NULL, "none");
	    break;

	case IPP_JSTATE_HELD :
	    if (job->fd >= 0)
	      ippAddString(client->response, IPP_TAG_JOB, IPP_CONST_TAG(IPP_TAG_KEYWORD), "job-state-reasons", NULL, "job-incoming");
	    else
	      ippAddString(client->response, IPP_TAG_JOB, IPP_CONST_TAG(IPP_TAG_KEYWORD), "job-state-reasons", NULL, "job-data-insufficient");
	    break;

	case IPP_JSTATE_PROCESSING :
	    if (job->is_canceled)
	      ippAddString(client->response, IPP_TAG_JOB, IPP_CONST_TAG(IPP_TAG_KEYWORD), "job-state-reasons", NULL, "processing-to-stop-point");
	    else
	      ippAddString(client->response, IPP_TAG_JOB, IPP_CONST_TAG(IPP_TAG_KEYWORD), "job-state-reasons", NULL, "job-printing");
	    break;

	case IPP_JSTATE_STOPPED :
	    ippAddString(client->response, IPP_TAG_JOB, IPP_CONST_TAG(IPP_TAG_KEYWORD), "job-state-reasons", NULL, "job-stopped");
	    break;

	case IPP_JSTATE_CANCELED :
	    ippAddString(client->response, IPP_TAG_JOB, IPP_CONST_TAG(IPP_TAG_KEYWORD), "job-state-reasons", NULL, "job-canceled-by-user");
	    break;

	case IPP_JSTATE_ABORTED :
	    ippAddString(client->response, IPP_TAG_JOB, IPP_CONST_TAG(IPP_TAG_KEYWORD), "job-state-reasons", NULL, "aborted-by-system");
	    break;

	case IPP_JSTATE_COMPLETED :
	    ippAddString(client->response, IPP_TAG_JOB, IPP_CONST_TAG(IPP_TAG_KEYWORD), "job-state-reasons", NULL, "job-completed-successfully");
	    break;
      }
    }
  }

  if (!ra || cupsArrayFind(ra, "time-at-creation"))
    ippAddInteger(client->response, IPP_TAG_JOB, IPP_TAG_INTEGER, "time-at-creation", (int)(job->created - client->printer->start_time));

  if (!ra || cupsArrayFind(ra, "time-at-completed"))
    ippAddInteger(client->response, IPP_TAG_JOB, job->completed ? IPP_TAG_INTEGER : IPP_TAG_NOVALUE, "time-at-completed", (int)(job->completed - client->printer->start_time));

  if (!ra || cupsArrayFind(ra, "time-at-processing"))
    ippAddInteger(client->response, IPP_TAG_JOB, job->processing ? IPP_TAG_INTEGER : IPP_TAG_NOVALUE, "time-at-processing", (int)(job->processing - client->printer->start_time));
}


//
// 'copy_printer_attributes()' - Copy printer attributes to a response...
//

static void
copy_printer_attributes(
    pappl_client_t  *client,		// I - Client
    pappl_printer_t *printer,		// I - Printer
    cups_array_t     *ra)		// I - Requested attributes
{
  int		i,			// Looping var
		num_values;		// Number of values
  unsigned	bit;			// Current bit value
  const char	*svalues[100];		// String values
  int		ivalues[100];		// Integer values
  pappl_driver_data_t	*data = &printer->driver_data;
					// Driver data


  _papplCopyAttributes(client->response, printer->attrs, ra, IPP_TAG_ZERO, IPP_TAG_CUPS_CONST);
  _papplCopyAttributes(client->response, printer->driver_attrs, ra, IPP_TAG_ZERO, IPP_TAG_CUPS_CONST);
  copy_printer_state(client->response, printer, ra);

  if (!ra || cupsArrayFind(ra, "identify-actions-default"))
  {
    for (num_values = 0, bit = PAPPL_IDENTIFY_ACTIONS_DISPLAY; bit <= PAPPL_IDENTIFY_ACTIONS_SPEAK; bit *= 2)
    {
      if (data->identify_default & bit)
	svalues[num_values ++] = _papplIdentifyActionsString(bit);
    }

    if (num_values > 0)
      ippAddStrings(client->response, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "identify-actions-default", num_values, NULL, svalues);
    else
      ippAddString(client->response, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "identify-actions-default", NULL, "none");
  }

  if ((!ra || cupsArrayFind(ra, "label-mode-configured")) && data->mode_configured)
    ippAddString(client->response, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "label-mode-configured", NULL, _papplLabelModeString(data->mode_configured));

  if ((!ra || cupsArrayFind(ra, "label-tear-offset-configured")) && data->tear_offset_supported[1] > 0)
    ippAddInteger(client->response, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "label-tear-offset-configured", data->tear_offset_configured);

  if (printer->num_supply > 0)
  {
    pappl_supply_t *supply = printer->supply;
					// Supply values...

    if (!ra || cupsArrayFind(ra, "marker-colors"))
    {
      for (i = 0; i < printer->num_supply; i ++)
        svalues[i] = _papplMarkerColorString(supply[i].color);

      ippAddStrings(client->response, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_NAME), "marker-colors", printer->num_supply, NULL, svalues);
    }

    if (!ra || cupsArrayFind(ra, "marker-high-levels"))
    {
      for (i = 0; i < printer->num_supply; i ++)
        ivalues[i] = supply[i].is_consumed ? 100 : 90;

      ippAddIntegers(client->response, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "marker-high-levels", printer->num_supply, ivalues);
    }

    if (!ra || cupsArrayFind(ra, "marker-levels"))
    {
      for (i = 0; i < printer->num_supply; i ++)
        ivalues[i] = supply[i].level;

      ippAddIntegers(client->response, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "marker-levels", printer->num_supply, ivalues);
    }

    if (!ra || cupsArrayFind(ra, "marker-low-levels"))
    {
      for (i = 0; i < printer->num_supply; i ++)
        ivalues[i] = supply[i].is_consumed ? 10 : 0;

      ippAddIntegers(client->response, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "marker-low-levels", printer->num_supply, ivalues);
    }

    if (!ra || cupsArrayFind(ra, "marker-names"))
    {
      for (i = 0; i < printer->num_supply; i ++)
        svalues[i] = supply[i].description;

      ippAddStrings(client->response, IPP_TAG_PRINTER, IPP_TAG_NAME, "marker-names", printer->num_supply, NULL, svalues);
    }

    if (!ra || cupsArrayFind(ra, "marker-types"))
    {
      for (i = 0; i < printer->num_supply; i ++)
        svalues[i] = _papplMarkerTypeString(supply[i].type);

      ippAddStrings(client->response, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "marker-types", printer->num_supply, NULL, svalues);
    }
  }

  if ((!ra || cupsArrayFind(ra, "media-col-default")) && data->media_default.size_name[0])
  {
    ipp_t *col = _papplMediaColExport(&printer->driver_data, &data->media_default, 0);
					// Collection value

    ippAddCollection(client->response, IPP_TAG_PRINTER, "media-col-default", col);
    ippDelete(col);
  }

  if (!ra || cupsArrayFind(ra, "media-col-ready"))
  {
    int			i, j,		// Looping vars
			count;		// Number of values
    ipp_t		*col;		// Collection value
    ipp_attribute_t	*attr;		// media-col-ready attribute
    pappl_media_col_t	media;		// Current media...

    for (i = 0, count = 0; i < data->num_source; i ++)
    {
      if (data->media_ready[i].size_name[0])
        count ++;
    }

    if (data->borderless && (data->bottom_top != 0 || data->left_right != 0))
      count *= 2;			// Need to report ready media for borderless, too...

    if (count > 0)
    {
      attr = ippAddCollections(client->response, IPP_TAG_PRINTER, "media-col-ready", count, NULL);

      for (i = 0, j = 0; i < data->num_source && j < count; i ++)
      {
	if (data->media_ready[i].size_name[0])
	{
          if (data->borderless && (data->bottom_top != 0 || data->left_right != 0))
	  {
	    // Report both bordered and borderless media-col values...
	    media = data->media_ready[i];

	    media.bottom_margin = media.top_margin   = data->bottom_top;
	    media.left_margin   = media.right_margin = data->left_right;
	    col = _papplMediaColExport(&printer->driver_data, &media, 0);
	    ippSetCollection(client->response, &attr, j ++, col);
	    ippDelete(col);

	    media.bottom_margin = media.top_margin   = 0;
	    media.left_margin   = media.right_margin = 0;
	    col = _papplMediaColExport(&printer->driver_data, &media, 0);
	    ippSetCollection(client->response, &attr, j ++, col);
	    ippDelete(col);
	  }
	  else
	  {
	    // Just report the single media-col value...
	    col = _papplMediaColExport(&printer->driver_data, data->media_ready + i, 0);
	    ippSetCollection(client->response, &attr, j ++, col);
	    ippDelete(col);
	  }
	}
      }
    }
  }

  if ((!ra || cupsArrayFind(ra, "media-default")) && data->media_default.size_name[0])
    ippAddString(client->response, IPP_TAG_PRINTER, IPP_TAG_KEYWORD, "media-default", NULL, data->media_default.size_name);

  if (!ra || cupsArrayFind(ra, "media-ready"))
  {
    int			i, j,		// Looping vars
			count;		// Number of values
    ipp_attribute_t	*attr;		// media-col-ready attribute

    for (i = 0, count = 0; i < data->num_source; i ++)
    {
      if (data->media_ready[i].size_name[0])
        count ++;
    }

    if (count > 0)
    {
      attr = ippAddStrings(client->response, IPP_TAG_PRINTER, IPP_TAG_KEYWORD, "media-ready", count, NULL, NULL);

      for (i = 0, j = 0; i < data->num_source && j < count; i ++)
      {
	if (data->media_ready[i].size_name[0])
	  ippSetString(client->response, &attr, j ++, data->media_ready[i].size_name);
      }
    }
  }

  if (!ra || cupsArrayFind(ra, "multiple-document-handling-default"))
    ippAddString(client->response, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "multiple-document-handling-default", NULL, "separate-documents-collated-copies");

  if (!ra || cupsArrayFind(ra, "orientation-requested-default"))
    ippAddInteger(client->response, IPP_TAG_PRINTER, IPP_TAG_ENUM, "orientation-requested-default", data->orient_default);

  if (!ra || cupsArrayFind(ra, "output-bin-default"))
  {
    if (data->num_bin > 0)
      ippAddString(client->response, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "output-bin-default", NULL, data->bin[data->bin_default]);
    else if (data->output_face_up)
      ippAddString(client->response, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "output-bin-default", NULL, "face-up");
    else
      ippAddString(client->response, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "output-bin-default", NULL, "face-down");
  }

  if ((!ra || cupsArrayFind(ra, "print-color-mode-default")) && data->color_default)
    ippAddString(client->response, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "print-color-mode-default", NULL, _papplColorModeString(data->color_default));

  if (!ra || cupsArrayFind(ra, "print-content-optimize-default"))
  {
    if (data->content_default)
      ippAddString(client->response, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "print-content-optimize-default", NULL, _papplContentString(data->content_default));
    else
      ippAddString(client->response, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "print-content-optimize-default", NULL, "auto");
  }

  if (!ra || cupsArrayFind(ra, "print-quality-default"))
  {
    if (data->quality_default)
      ippAddInteger(client->response, IPP_TAG_PRINTER, IPP_TAG_ENUM, "print-quality-default", data->quality_default);
    else
      ippAddInteger(client->response, IPP_TAG_PRINTER, IPP_TAG_ENUM, "print-quality-default", IPP_QUALITY_NORMAL);
  }

  if (!ra || cupsArrayFind(ra, "print-scaling-default"))
  {
    if (data->scaling_default)
      ippAddString(client->response, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "print-scaling-default", NULL, _papplScalingString(data->scaling_default));
    else
      ippAddString(client->response, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "print-scaling-default", NULL, "auto");
  }

  if (!ra || cupsArrayFind(ra, "printer-config-change-date-time"))
    ippAddDate(client->response, IPP_TAG_PRINTER, "printer-config-change-date-time", ippTimeToDate(printer->config_time));

  if (!ra || cupsArrayFind(ra, "printer-config-change-time"))
    ippAddInteger(client->response, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "printer-config-change-time", (int)(printer->config_time - printer->start_time));

  if (!ra || cupsArrayFind(ra, "printer-contact-col"))
  {
    ipp_t *col = _papplContactExport(&printer->contact);
    ippAddCollection(client->response, IPP_TAG_PRINTER, "printer-contact-col", col);
    ippDelete(col);
  }

  if (!ra || cupsArrayFind(ra, "printer-current-time"))
    ippAddDate(client->response, IPP_TAG_PRINTER, "printer-current-time", ippTimeToDate(time(NULL)));

  if ((!ra || cupsArrayFind(ra, "printer-darkness-configured")) && data->darkness_supported > 0)
    ippAddInteger(client->response, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "printer-darkness-configured", data->darkness_configured);

  _papplSystemExportVersions(client->system, client->response, IPP_TAG_PRINTER, ra);

  if (!ra || cupsArrayFind(ra, "printer-dns-sd-name"))
    ippAddString(client->response, IPP_TAG_PRINTER, IPP_TAG_NAME, "printer-dns-sd-name", NULL, printer->dns_sd_name ? printer->dns_sd_name : "");

  if (!ra || cupsArrayFind(ra, "printer-geo-location"))
  {
    if (printer->geo_location)
      ippAddString(client->response, IPP_TAG_PRINTER, IPP_TAG_URI, "printer-geo-location", NULL, printer->geo_location);
    else
      ippAddOutOfBand(client->response, IPP_TAG_PRINTER, IPP_TAG_UNKNOWN, "printer-geo-location");
  }

  if (!ra || cupsArrayFind(ra, "printer-icons"))
  {
    char	uris[3][1024];		// Buffers for URIs
    const char	*values[3];		// Values for attribute

    httpAssembleURIf(HTTP_URI_CODING_ALL, uris[0], sizeof(uris[0]), "https", NULL, client->host_field, client->host_port, "%s/icon-sm.png", printer->uriname);
    httpAssembleURIf(HTTP_URI_CODING_ALL, uris[1], sizeof(uris[1]), "https", NULL, client->host_field, client->host_port, "%s/icon-md.png", printer->uriname);
    httpAssembleURIf(HTTP_URI_CODING_ALL, uris[2], sizeof(uris[2]), "https", NULL, client->host_field, client->host_port, "%s/icon-lg.png", printer->uriname);

    values[0] = uris[0];
    values[1] = uris[1];
    values[2] = uris[2];

    ippAddStrings(client->response, IPP_TAG_PRINTER, IPP_TAG_URI, "printer-icons", 3, NULL, values);
  }

  if (!ra || cupsArrayFind(ra, "printer-impressions-completed"))
    ippAddInteger(client->response, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "printer-impressions-completed", printer->impcompleted);

  if (!ra || cupsArrayFind(ra, "printer-input-tray"))
  {
    int			i;		// Looping var
    ipp_attribute_t	*attr = NULL;	// "printer-input-tray" attribute
    char		value[256];	// Value for current tray
    pappl_media_col_t	*media;		// Media in the tray

    for (i = 0, media = data->media_ready; i < data->num_source; i ++, media ++)
    {
      const char	*type;		// Tray type

      if (!strcmp(data->source[i], "manual"))
        type = "sheetFeedManual";
      else if (!strcmp(data->source[i], "by-pass-tray"))
        type = "sheetFeedAutoNonRemovableTray";
      else
        type = "sheetFeedAutoRemovableTray";

      snprintf(value, sizeof(value), "type=%s;mediafeed=%d;mediaxfeed=%d;maxcapacity=%d;level=-2;status=0;name=%s;", type, media->size_length, media->size_width, !strcmp(media->source, "manual") ? 1 : -2, media->source);

      if (attr)
        ippSetOctetString(client->response, &attr, ippGetCount(attr), value, (int)strlen(value));
      else
        attr = ippAddOctetString(client->response, IPP_TAG_PRINTER, "printer-input-tray", value, (int)strlen(value));
    }

    // The "auto" tray is a dummy entry...
    strlcpy(value, "type=other;mediafeed=0;mediaxfeed=0;maxcapacity=-2;level=-2;status=0;name=auto;", sizeof(value));
    ippSetOctetString(client->response, &attr, ippGetCount(attr), value, (int)strlen(value));
  }

  if (!ra || cupsArrayFind(ra, "printer-is-accepting-jobs"))
    ippAddBoolean(client->response, IPP_TAG_PRINTER, "printer-is-accepting-jobs", !printer->system->shutdown_time);

  if (!ra || cupsArrayFind(ra, "printer-location"))
    ippAddString(client->response, IPP_TAG_PRINTER, IPP_TAG_TEXT, "printer-location", NULL, printer->location ? printer->location : "");

  if (!ra || cupsArrayFind(ra, "printer-more-info"))
  {
    char	uri[1024];		// URI value

    httpAssembleURIf(HTTP_URI_CODING_ALL, uri, sizeof(uri), "https", NULL, client->host_field, client->host_port, "%s/", printer->uriname);
    ippAddString(client->response, IPP_TAG_PRINTER, IPP_TAG_URI, "printer-more-info", NULL, uri);
  }

  if (!ra || cupsArrayFind(ra, "printer-organization"))
    ippAddString(client->response, IPP_TAG_PRINTER, IPP_TAG_TEXT, "printer-organization", NULL, printer->organization ? printer->organization : "");

  if (!ra || cupsArrayFind(ra, "printer-organizational-unit"))
    ippAddString(client->response, IPP_TAG_PRINTER, IPP_TAG_TEXT, "printer-organizational-unit", NULL, printer->org_unit ? printer->org_unit : "");

  if (!ra || cupsArrayFind(ra, "printer-resolution-default"))
    ippAddResolution(client->response, IPP_TAG_PRINTER, "printer-resolution-default", IPP_RES_PER_INCH, data->x_default, data->y_default);

  if (!ra || cupsArrayFind(ra, "printer-speed-default"))
    ippAddInteger(client->response, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "printer-speed-default", data->speed_default);

  if (!ra || cupsArrayFind(ra, "printer-state-change-date-time"))
    ippAddDate(client->response, IPP_TAG_PRINTER, "printer-state-change-date-time", ippTimeToDate(printer->state_time));

  if (!ra || cupsArrayFind(ra, "printer-state-change-time"))
    ippAddInteger(client->response, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "printer-state-change-time", (int)(printer->state_time - printer->start_time));

#if 0 // TODO: Lookup localization resource files...
  if (!ra || cupsArrayFind(ra, "printer-strings-languages-supported"))
  {
    ippAddStrings(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_LANGUAGE, "printer-strings-languages-supported", (int)(sizeof(printer_strings_languages) / sizeof(printer_strings_languages[0])), NULL, printer_strings_languages);
  }
#endif // 0

  if (!ra || cupsArrayFind(ra, "printer-strings-uri"))
  {
    const char	*lang = ippGetString(ippFindAttribute(client->request, "attributes-natural-language", IPP_TAG_LANGUAGE), 0, NULL);
					// Language
    char	baselang[3],		// Base language
		uri[1024];		// Strings file URI

    strlcpy(baselang, lang, sizeof(baselang));
    if (!strcmp(baselang, "de") || !strcmp(baselang, "en") || !strcmp(baselang, "es") || !strcmp(baselang, "fr") || !strcmp(baselang, "it"))
    {
      // TODO: Lookup localization resource file...
      httpAssembleURIf(HTTP_URI_CODING_ALL, uri, sizeof(uri), "https", NULL, client->host_field, client->host_port, "/%s.strings", baselang);
      ippAddString(client->response, IPP_TAG_PRINTER, IPP_TAG_URI, "printer-strings-uri", NULL, uri);
    }
  }

  if (printer->num_supply > 0)
  {
    pappl_supply_t	 *supply = printer->supply;
					// Supply values...

    if (!ra || cupsArrayFind(ra, "printer-supply"))
    {
      char		value[256];	// "printer-supply" value
      ipp_attribute_t	*attr = NULL;	// "printer-supply" attribute

      for (i = 0; i < printer->num_supply; i ++)
      {
	snprintf(value, sizeof(value), "index=%d;type=%s;maxcapacity=100;level=%d;colorantname=%s;", i, _papplSupplyTypeString(supply[i].type), supply[i].level, _papplSupplyColorString(supply[i].color));

	if (attr)
	  ippSetOctetString(client->response, &attr, ippGetCount(attr), value, (int)strlen(value));
	else
	  attr = ippAddOctetString(client->response, IPP_TAG_PRINTER, "printer-supply", value, (int)strlen(value));
      }
    }

    if (!ra || cupsArrayFind(ra, "printer-supply-description"))
    {
      for (i = 0; i < printer->num_supply; i ++)
        svalues[i] = supply[i].description;

      ippAddStrings(client->response, IPP_TAG_PRINTER, IPP_TAG_TEXT, "printer-supply-description", printer->num_supply, NULL, svalues);
    }
  }

  if (!ra || cupsArrayFind(ra, "printer-supply-info-uri"))
  {
    char	uri[1024];		// URI value

    httpAssembleURIf(HTTP_URI_CODING_ALL, uri, sizeof(uri), "https", NULL, client->host_field, client->host_port, "%s/supplies", printer->uriname);
    ippAddString(client->response, IPP_TAG_PRINTER, IPP_TAG_URI, "printer-supply-info-uri", NULL, uri);
  }

  if (!ra || cupsArrayFind(ra, "printer-up-time"))
    ippAddInteger(client->response, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "printer-up-time", (int)(time(NULL) - printer->start_time));

  if (!ra || cupsArrayFind(ra, "printer-uri-supported"))
  {
    char	uris[2][1024];		// Buffers for URIs
    int		num_values = 0;		// Number of values
    const char	*values[2];		// Values for attribute

    if (!papplSystemGetTLSOnly(client->system))
    {
      httpAssembleURI(HTTP_URI_CODING_ALL, uris[num_values], sizeof(uris[0]), "ipp", NULL, client->host_field, client->host_port, printer->resource);
      values[num_values] = uris[num_values];
      num_values ++;
    }

    httpAssembleURI(HTTP_URI_CODING_ALL, uris[num_values], sizeof(uris[0]), "ipps", NULL, client->host_field, client->host_port, printer->resource);
    values[num_values] = uris[num_values];
    num_values ++;

    ippAddStrings(client->response, IPP_TAG_PRINTER, IPP_TAG_URI, "printer-uri-supported", num_values, NULL, values);
  }

  if (!ra || cupsArrayFind(ra, "printer-xri-supported"))
    copy_printer_xri(client, client->response, printer);

  if (!ra || cupsArrayFind(ra, "queued-job-count"))
    ippAddInteger(client->response, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "queued-job-count", cupsArrayCount(printer->active_jobs));

  if (!ra || cupsArrayFind(ra, "sides-default"))
  {
    if (data->sides_default)
      ippAddString(client->response, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "sides-default", NULL, _papplSidesString(data->sides_default));
    else
      ippAddString(client->response, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "sides-default", NULL, "one-sided");
  }

  if (!ra || cupsArrayFind(ra, "uri-authentication-supported"))
  {
    // For each supported printer-uri value, report whether authentication is
    // supported.  Since we only support authentication over a secure (TLS)
    // channel, the value is always 'none' for the "ipp" URI and either 'none'
    // or 'basic' for the "ipps" URI...
    if (papplSystemGetTLSOnly(client->system))
    {
      if (papplSystemGetAuthService(client->system))
        ippAddString(client->response, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "uri-authentication-supported", NULL, "basic");
      else
        ippAddString(client->response, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "uri-authentication-supported", NULL, "none");
    }
    else if (papplSystemGetAuthService(client->system))
    {
      static const char * const uri_authentication_basic[] =
      {					// uri-authentication-supported values
	"none",
	"basic"
      };

      ippAddStrings(client->response, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "uri-authentication-supported", 2, NULL, uri_authentication_basic);
    }
    else
    {
      static const char * const uri_authentication_none[] =
      {					// uri-authentication-supported values
	"none",
	"none"
      };

      ippAddStrings(client->response, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "uri-authentication-supported", 2, NULL, uri_authentication_none);
    }
  }
}


//
// 'copy_printer_state()' - Copy the printer-state-xxx attributes.
//

static void
copy_printer_state(
    ipp_t            *ipp,		// I - IPP message
    pappl_printer_t *printer,		// I - Printer
    cups_array_t     *ra)		// I - Requested attributes
{
  if (!ra || cupsArrayFind(ra, "printer-state"))
    ippAddInteger(ipp, IPP_TAG_PRINTER, IPP_TAG_ENUM, "printer-state", (int)printer->state);

  if (!ra || cupsArrayFind(ra, "printer-state-message"))
  {
    static const char * const messages[] = { "Idle.", "Printing.", "Stopped." };

    ippAddString(ipp, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_TEXT), "printer-state-message", NULL, messages[printer->state - IPP_PSTATE_IDLE]);
  }

  if (!ra || cupsArrayFind(ra, "printer-state-reasons"))
  {
    if (printer->state_reasons == PAPPL_PREASON_NONE)
    {
      if (printer->is_stopped)
	ippAddString(ipp, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "printer-state-reasons", NULL, "moving-to-paused");
      else if (printer->state == IPP_PSTATE_STOPPED)
	ippAddString(ipp, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "printer-state-reasons", NULL, "paused");
      else
	ippAddString(ipp, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "printer-state-reasons", NULL, "none");
    }
    else
    {
      ipp_attribute_t	*attr = NULL;		// printer-state-reasons
      pappl_preason_t	bit;			// Reason bit

      for (bit = PAPPL_PREASON_OTHER; bit <= PAPPL_PREASON_TONER_LOW; bit *= 2)
      {
        if (printer->state_reasons & bit)
	{
	  if (attr)
	    ippSetString(ipp, &attr, ippGetCount(attr), _papplPrinterReasonString(bit));
	  else
	    attr = ippAddString(ipp, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "printer-state-reasons", NULL, _papplPrinterReasonString(bit));
	}
      }

      if (printer->is_stopped)
	ippSetString(ipp, &attr, ippGetCount(attr), "moving-to-paused");
      else if (printer->state == IPP_PSTATE_STOPPED)
	ippSetString(ipp, &attr, ippGetCount(attr), "paused");
    }
  }
}


//
// 'copy_printer_xri()' - Copy the "printer-xri-supported" attribute.
//

static void
copy_printer_xri(
    pappl_client_t  *client,		// I - Client
    ipp_t           *ipp,		// I - IPP message
    pappl_printer_t *printer)		// I - Printer
{
  char	uri[1024];			// URI value
  int	i,				// Looping var
	num_values = 0;			// Number of values
  ipp_t	*col,				// Current collection value
	*values[2];			// Values for attribute


  if (!papplSystemGetTLSOnly(client->system))
  {
    // Add ipp: URI...
    httpAssembleURI(HTTP_URI_CODING_ALL, uri, sizeof(uri), "ipp", NULL, client->host_field, client->host_port, printer->resource);
    col = ippNew();

    ippAddString(col, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "xri-authentication", NULL, "none");
    ippAddString(col, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "xri-security", NULL, "none");
    ippAddString(col, IPP_TAG_PRINTER, IPP_TAG_URI, "xri-uri", NULL, uri);

    values[num_values ++] = col;
  }

  // Add ipps: URI...
  httpAssembleURI(HTTP_URI_CODING_ALL, uri, sizeof(uri), "ipps", NULL, client->host_field, client->host_port, printer->resource);
  col = ippNew();

  ippAddString(col, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "xri-authentication", NULL, papplSystemGetAuthService(client->system) ? "basic" : "none");
  ippAddString(col, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "xri-security", NULL, "tls");
  ippAddString(col, IPP_TAG_PRINTER, IPP_TAG_URI, "xri-uri", NULL, uri);

  values[num_values ++] = col;

  ippAddCollections(ipp, IPP_TAG_PRINTER, "printer-xri-supported", num_values, (const ipp_t **)values);

  for (i = 0; i < num_values; i ++)
    ippDelete(values[i]);
}


//
// 'create_job()' - Create a new job object from a Print-Job or Create-Job
//                  request.
//

static pappl_job_t *			// O - Job
create_job(
    pappl_client_t *client)		// I - Client
{
  ipp_attribute_t	*attr;		// Job attribute
  const char		*job_name,	// Job name
			*username;	// Owner


  // Get the requesting-user-name, document format, and name...
  if (client->username[0])
    username = client->username;
  else  if ((attr = ippFindAttribute(client->request, "requesting-user-name", IPP_TAG_NAME)) != NULL)
    username = ippGetString(attr, 0, NULL);
  else
    username = "guest";

  if ((attr = ippFindAttribute(client->request, "job-name", IPP_TAG_NAME)) != NULL)
    job_name = ippGetString(attr, 0, NULL);
  else
    job_name = "Untitled";

  return (_papplJobCreate(client->printer, 0, username, NULL, job_name, client->request));
}


//
// 'finish_document()' - Finish receiving a document file and start processing.
//

static void
finish_document_data(
    pappl_client_t *client,		// I - Client
    pappl_job_t    *job)		// I - Job
{
  char			filename[1024],	// Filename buffer
			buffer[4096];	// Copy buffer
  ssize_t		bytes;		// Bytes read
  cups_array_t		*ra;		// Attributes to send in response


  // If we have a PWG or Apple raster file, process it directly or return
  // server-error-busy...
  if (!strcmp(job->format, "image/pwg-raster") || !strcmp(job->format, "image/urf"))
  {
    if (job->printer->processing_job)
    {
      papplClientRespondIPP(client, IPP_STATUS_ERROR_BUSY, "Currently printing another job.");
      flush_document_data(client);
      return;
    }

    job->state = IPP_JSTATE_PENDING;

    _papplJobProcessRaster(job, client);

    goto complete_job;
  }

  // Create a file for the request data...
  if ((job->fd = papplJobOpenFile(job, filename, sizeof(filename), client->system->directory, NULL, "w")) < 0)
  {
    papplClientRespondIPP(client, IPP_STATUS_ERROR_INTERNAL, "Unable to create print file: %s", strerror(errno));

    goto abort_job;
  }

  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "Created job file \"%s\", format \"%s\".", filename, job->format);

  while ((bytes = httpRead2(client->http, buffer, sizeof(buffer))) > 0)
  {
    if (write(job->fd, buffer, (size_t)bytes) < bytes)
    {
      int error = errno;		// Write error

      close(job->fd);
      job->fd = -1;

      unlink(filename);

      papplClientRespondIPP(client, IPP_STATUS_ERROR_INTERNAL, "Unable to write print file: %s", strerror(error));

      goto abort_job;
    }
  }

  if (bytes < 0)
  {
    // Got an error while reading the print data, so abort this job.
    close(job->fd);
    job->fd = -1;

    unlink(filename);

    papplClientRespondIPP(client, IPP_STATUS_ERROR_INTERNAL, "Unable to read print file.");

    goto abort_job;
  }

  if (close(job->fd))
  {
    int error = errno;			// Write error

    job->fd = -1;

    unlink(filename);

    papplClientRespondIPP(client, IPP_STATUS_ERROR_INTERNAL, "Unable to write print file: %s", strerror(error));

    goto abort_job;
  }

  job->fd = -1;

  // Submit the job for processing...
  _papplJobSubmitFile(job, filename);

  complete_job:

  // Return the job info...
  papplClientRespondIPP(client, IPP_STATUS_OK, NULL);

  ra = cupsArrayNew((cups_array_func_t)strcmp, NULL);
  cupsArrayAdd(ra, "job-id");
  cupsArrayAdd(ra, "job-state");
  cupsArrayAdd(ra, "job-state-message");
  cupsArrayAdd(ra, "job-state-reasons");
  cupsArrayAdd(ra, "job-uri");

  copy_job_attributes(client, job, ra);
  cupsArrayDelete(ra);
  return;

  // If we get here we had to abort the job...
  abort_job:

  flush_document_data(client);

  job->state     = IPP_JSTATE_ABORTED;
  job->completed = time(NULL);

  pthread_rwlock_wrlock(&client->printer->rwlock);

  cupsArrayRemove(client->printer->active_jobs, job);
  cupsArrayAdd(client->printer->completed_jobs, job);

  if (!client->system->clean_time)
    client->system->clean_time = time(NULL) + 60;

  pthread_rwlock_unlock(&client->printer->rwlock);

  ra = cupsArrayNew((cups_array_func_t)strcmp, NULL);
  cupsArrayAdd(ra, "job-id");
  cupsArrayAdd(ra, "job-state");
  cupsArrayAdd(ra, "job-state-reasons");
  cupsArrayAdd(ra, "job-uri");

  copy_job_attributes(client, job, ra);
  cupsArrayDelete(ra);
}


//
// 'flush_document_data()' - Safely flush remaining document data.
//

static void
flush_document_data(
    pappl_client_t *client)		// I - Client
{
  char	buffer[8192];			// Read buffer


  if (httpGetState(client->http) == HTTP_STATE_POST_RECV)
  {
    while (httpRead2(client->http, buffer, sizeof(buffer)) > 0);
  }
}


//
// 'have_document_data()' - Determine whether we have more document data.
//

static bool				// O - `true` if data is present, `false` otherwise
have_document_data(
    pappl_client_t *client)		// I - Client
{
  char temp;				// Data


  if (httpGetState(client->http) != HTTP_STATE_POST_RECV)
    return (false);
  else
    return (httpPeek(client->http, &temp, 1) > 0);
}


//
// 'ipp_cancel_job()' - Cancel a job.
//

static void
ipp_cancel_job(pappl_client_t *client)	// I - Client
{
  pappl_job_t	*job;			// Job information


  // Get the job...
  if (ippGetOperation(client->request) == IPP_OP_CANCEL_CURRENT_JOB)
    job = client->printer->processing_job;
  else
    job = client->job;

  if (!job)
  {
    papplClientRespondIPP(client, IPP_STATUS_ERROR_NOT_FOUND, "Job does not exist.");
    return;
  }

  // See if the job is already completed, canceled, or aborted; if so,
  // we can't cancel...
  switch (job->state)
  {
    case IPP_JSTATE_CANCELED :
	papplClientRespondIPP(client, IPP_STATUS_ERROR_NOT_POSSIBLE, "Job #%d is already canceled - can\'t cancel.", job->job_id);
        break;

    case IPP_JSTATE_ABORTED :
	papplClientRespondIPP(client, IPP_STATUS_ERROR_NOT_POSSIBLE, "Job #%d is already aborted - can\'t cancel.", job->job_id);
        break;

    case IPP_JSTATE_COMPLETED :
	papplClientRespondIPP(client, IPP_STATUS_ERROR_NOT_POSSIBLE, "Job #%d is already completed - can\'t cancel.", job->job_id);
        break;

    default :
        // Cancel the job...
        papplJobCancel(job);

	papplClientRespondIPP(client, IPP_STATUS_OK, NULL);
        break;
  }
}


//
// 'ipp_cancel_jobs()' - Cancel all jobs.
//

static void
ipp_cancel_jobs(pappl_client_t *client)// I - Client
{
  http_status_t	auth_status;		// Authorization status


  // Verify the connection is authorized...
  if ((auth_status = papplClientIsAuthorized(client)) != HTTP_STATUS_CONTINUE)
  {
    papplClientRespondHTTP(client, auth_status, NULL, NULL, 0, 0);
    return;
  }

  // Cancel all jobs...
  papplPrinterCancelAllJobs(client->printer);

  papplClientRespondIPP(client, IPP_STATUS_OK, NULL);
}


//
// 'ipp_close_job()' - Close an open job.
//

static void
ipp_close_job(pappl_client_t *client)	// I - Client
{
  pappl_job_t	*job = client->job;	// Job information


  // Get the job...
  if (!job)
  {
    papplClientRespondIPP(client, IPP_STATUS_ERROR_NOT_FOUND, "Job does not exist.");
    return;
  }

  // See if the job is already completed, canceled, or aborted; if so,
  // we can't cancel...
  switch (job->state)
  {
    case IPP_JSTATE_CANCELED :
	papplClientRespondIPP(client, IPP_STATUS_ERROR_NOT_POSSIBLE, "Job #%d is canceled - can\'t close.", job->job_id);
        break;

    case IPP_JSTATE_ABORTED :
	papplClientRespondIPP(client, IPP_STATUS_ERROR_NOT_POSSIBLE, "Job #%d is aborted - can\'t close.", job->job_id);
        break;

    case IPP_JSTATE_COMPLETED :
	papplClientRespondIPP(client, IPP_STATUS_ERROR_NOT_POSSIBLE, "Job #%d is completed - can\'t close.", job->job_id);
        break;

    case IPP_JSTATE_PROCESSING :
    case IPP_JSTATE_STOPPED :
	papplClientRespondIPP(client, IPP_STATUS_ERROR_NOT_POSSIBLE, "Job #%d is already closed.", job->job_id);
        break;

    default :
	papplClientRespondIPP(client, IPP_STATUS_OK, NULL);
        break;
  }
}


//
// 'ipp_create_job()' - Create a job object.
//

static void
ipp_create_job(pappl_client_t *client)	// I - Client
{
  pappl_job_t		*job;		// New job
  cups_array_t		*ra;		// Attributes to send in response


  // Do we have a file to print?
  if (have_document_data(client))
  {
    flush_document_data(client);
    papplClientRespondIPP(client, IPP_STATUS_ERROR_BAD_REQUEST, "Unexpected document data following request.");
    return;
  }

  // Validate print job attributes...
  if (!valid_job_attributes(client))
    return;

  // Create the job...
  if ((job = create_job(client)) == NULL)
  {
    papplClientRespondIPP(client, IPP_STATUS_ERROR_BUSY, "Currently printing another job.");
    return;
  }

  // Return the job info...
  papplClientRespondIPP(client, IPP_STATUS_OK, NULL);

  ra = cupsArrayNew((cups_array_func_t)strcmp, NULL);
  cupsArrayAdd(ra, "job-id");
  cupsArrayAdd(ra, "job-state");
  cupsArrayAdd(ra, "job-state-message");
  cupsArrayAdd(ra, "job-state-reasons");
  cupsArrayAdd(ra, "job-uri");

  copy_job_attributes(client, job, ra);
  cupsArrayDelete(ra);
}


//
// 'ipp_create_printer()' - Create a printer.
//

static void
ipp_create_printer(
    pappl_client_t *client)		// I - Client
{
  const char	*printer_name,		// Printer name
		*device_id,		// Device URI
		*device_uri,		// Device URI
		*driver_name;		// Name of driver
  ipp_attribute_t *attr;		// Current attribute
  char		resource[256];		// Resource path
  pappl_printer_t *printer;		// Printer
  cups_array_t	*ra;			// Requested attributes
  http_status_t	auth_status;		// Authorization status


  // Verify the connection is authorized...
  if ((auth_status = papplClientIsAuthorized(client)) != HTTP_STATUS_CONTINUE)
  {
    papplClientRespondHTTP(client, auth_status, NULL, NULL, 0, 0);
    return;
  }

  // Get required attributes...
  if ((attr = ippFindAttribute(client->request, "printer-service-type", IPP_TAG_ZERO)) == NULL)
  {
    papplClientRespondIPP(client, IPP_STATUS_ERROR_BAD_REQUEST, "Missing 'printer-service-type' attribute in request.");
    return;
  }
  else if (ippGetGroupTag(attr) != IPP_TAG_OPERATION || ippGetValueTag(attr) != IPP_TAG_KEYWORD || ippGetCount(attr) != 1 || strcmp(ippGetString(attr, 0, NULL), "print"))
  {
    respond_unsupported(client, attr);
    return;
  }

  if ((attr = ippFindAttribute(client->request, "printer-name", IPP_TAG_ZERO)) == NULL)
  {
    papplClientRespondIPP(client, IPP_STATUS_ERROR_BAD_REQUEST, "Missing 'printer-name' attribute in request.");
    return;
  }
  else if (ippGetGroupTag(attr) != IPP_TAG_PRINTER || (ippGetValueTag(attr) != IPP_TAG_NAME && ippGetValueTag(attr) != IPP_TAG_NAMELANG) || ippGetCount(attr) != 1 || strlen(ippGetString(attr, 0, NULL)) > 127)
  {
    respond_unsupported(client, attr);
    return;
  }
  else
    printer_name = ippGetString(attr, 0, NULL);

  if ((attr = ippFindAttribute(client->request, "printer-device-id", IPP_TAG_ZERO)) != NULL && (ippGetGroupTag(attr) != IPP_TAG_PRINTER || ippGetValueTag(attr) != IPP_TAG_TEXT || ippGetCount(attr) != 1))
  {
    respond_unsupported(client, attr);
    return;
  }
  else
    device_id = ippGetString(attr, 0, NULL);

  if ((attr = ippFindAttribute(client->request, "smi2699-device-uri", IPP_TAG_ZERO)) == NULL)
  {
    papplClientRespondIPP(client, IPP_STATUS_ERROR_BAD_REQUEST, "Missing 'smi2699-device-uri' attribute in request.");
    return;
  }
  else if (ippGetGroupTag(attr) != IPP_TAG_PRINTER || ippGetValueTag(attr) != IPP_TAG_URI || ippGetCount(attr) != 1)
  {
    respond_unsupported(client, attr);
    return;
  }
  else
  {
    device_uri = ippGetString(attr, 0, NULL);

    if (strncmp(device_uri, "file:///", 8) && strncmp(device_uri, "socket://", 9) && strncmp(device_uri, "usb://", 6))
    {
      respond_unsupported(client, attr);
      return;
    }
  }

  if ((attr = ippFindAttribute(client->request, "smi2699-device-command", IPP_TAG_ZERO)) == NULL)
  {
    papplClientRespondIPP(client, IPP_STATUS_ERROR_BAD_REQUEST, "Missing 'smi2699-device-command' attribute in request.");
    return;
  }
  else if (ippGetGroupTag(attr) != IPP_TAG_PRINTER || ippGetValueTag(attr) != IPP_TAG_KEYWORD || ippGetCount(attr) != 1)
  {
    respond_unsupported(client, attr);
    return;
  }
  else if (client->system->pdriver_cb)
  {
    driver_name = ippGetString(attr, 0, NULL);
  }
  else
  {
    papplLog(client->system, PAPPL_LOGLEVEL_ERROR, "No driver callback set, unable to add printer.");

    respond_unsupported(client, attr);
    return;
  }

  // See if the printer already exists...
  snprintf(resource, sizeof(resource), "/ipp/print/%s", printer_name);

  if (papplSystemFindPrinter(client->system, resource, 0, NULL))
  {
    papplClientRespondIPP(client, IPP_STATUS_ERROR_NOT_POSSIBLE, "Printer name '%s' already exists.", printer_name);
    return;
  }

  // Create the printer...
  if ((printer = papplPrinterCreate(client->system, PAPPL_SERVICE_TYPE_PRINT, 0, printer_name, driver_name, device_id, device_uri)) == NULL)
  {
    papplClientRespondIPP(client, IPP_STATUS_ERROR_INTERNAL, "Printer name '%s' already exists.", printer_name);
    return;
  }

  if (!set_printer_attributes(client, printer))
    return;

  // Return the printer
  papplClientRespondIPP(client, IPP_STATUS_OK, NULL);

  ra = cupsArrayNew((cups_array_func_t)strcmp, NULL);
  cupsArrayAdd(ra, "printer-id");
  cupsArrayAdd(ra, "printer-is-accepting-jobs");
  cupsArrayAdd(ra, "printer-state");
  cupsArrayAdd(ra, "printer-state-reasons");
  cupsArrayAdd(ra, "printer-uuid");
  cupsArrayAdd(ra, "printer-xri-supported");

  copy_printer_attributes(client, printer, ra);
  cupsArrayDelete(ra);
}


//
// 'ipp_delete_printer()' - Delete a printer.
//

static void
ipp_delete_printer(
    pappl_client_t *client)		// I - Client
{
  http_status_t	auth_status;		// Authorization status


  // Verify the connection is authorized...
  if ((auth_status = papplClientIsAuthorized(client)) != HTTP_STATUS_CONTINUE)
  {
    papplClientRespondHTTP(client, auth_status, NULL, NULL, 0, 0);
    return;
  }

  if (!client->printer)
  {
    papplClientRespondIPP(client, IPP_STATUS_ERROR_NOT_FOUND, "Printer not found.");
    return;
  }

  if (!client->printer->processing_job)
    papplPrinterDelete(client->printer);
  else
    client->printer->is_deleted = true;

  papplClientRespondIPP(client, IPP_STATUS_OK, NULL);
}


//
// 'ipp_get_job_attributes()' - Get the attributes for a job object.
//

static void
ipp_get_job_attributes(
    pappl_client_t *client)		// I - Client
{
  pappl_job_t	*job = client->job;	// Job information
  cups_array_t	*ra;			// requested-attributes


  if (!job)
  {
    papplClientRespondIPP(client, IPP_STATUS_ERROR_NOT_FOUND, "Job not found.");
    return;
  }

  papplClientRespondIPP(client, IPP_STATUS_OK, NULL);

  ra = ippCreateRequestedArray(client->request);
  copy_job_attributes(client, job, ra);
  cupsArrayDelete(ra);
}


//
// 'ipp_get_jobs()' - Get a list of job objects.
//

static void
ipp_get_jobs(pappl_client_t *client)	// I - Client
{
  ipp_attribute_t	*attr;		// Current attribute
  const char		*which_jobs = NULL;
					// which-jobs values
  int			job_comparison;	// Job comparison
  ipp_jstate_t		job_state;	// job-state value
  int			first_job_id,	// First job ID
			limit,		// Maximum number of jobs to return
			count;		// Number of jobs that match
  const char		*username;	// Username
  cups_array_t		*list;		// Jobs list
  pappl_job_t		*job;		// Current job pointer
  cups_array_t		*ra;		// Requested attributes array


  // See if the "which-jobs" attribute have been specified...
  if ((attr = ippFindAttribute(client->request, "which-jobs", IPP_TAG_KEYWORD)) != NULL)
  {
    which_jobs = ippGetString(attr, 0, NULL);
    papplLogClient(client, PAPPL_LOGLEVEL_DEBUG, "Get-Jobs \"which-jobs\"='%s'", which_jobs);
  }

  if (!which_jobs || !strcmp(which_jobs, "not-completed"))
  {
    job_comparison = -1;
    job_state      = IPP_JSTATE_STOPPED;
    list           = client->printer->active_jobs;
  }
  else if (!strcmp(which_jobs, "completed"))
  {
    job_comparison = 1;
    job_state      = IPP_JSTATE_CANCELED;
    list           = client->printer->completed_jobs;
  }
  else if (!strcmp(which_jobs, "all"))
  {
    job_comparison = 1;
    job_state      = IPP_JSTATE_PENDING;
    list           = client->printer->all_jobs;
  }
  else
  {
    papplClientRespondIPP(client, IPP_STATUS_ERROR_ATTRIBUTES_OR_VALUES, "The \"which-jobs\" value '%s' is not supported.", which_jobs);
    ippAddString(client->response, IPP_TAG_UNSUPPORTED_GROUP, IPP_TAG_KEYWORD, "which-jobs", NULL, which_jobs);
    return;
  }

  // See if they want to limit the number of jobs reported...
  if ((attr = ippFindAttribute(client->request, "limit", IPP_TAG_INTEGER)) != NULL)
  {
    limit = ippGetInteger(attr, 0);

    papplLogClient(client, PAPPL_LOGLEVEL_DEBUG, "Get-Jobs \"limit\"='%d'", limit);
  }
  else
    limit = 0;

  if ((attr = ippFindAttribute(client->request, "first-job-id", IPP_TAG_INTEGER)) != NULL)
  {
    first_job_id = ippGetInteger(attr, 0);

    papplLogClient(client, PAPPL_LOGLEVEL_DEBUG, "Get-Jobs \"first-job-id\"='%d'", first_job_id);
  }
  else
    first_job_id = 1;

  // See if we only want to see jobs for a specific user...
  username = NULL;

  if ((attr = ippFindAttribute(client->request, "my-jobs", IPP_TAG_BOOLEAN)) != NULL)
  {
    int my_jobs = ippGetBoolean(attr, 0);

    papplLogClient(client, PAPPL_LOGLEVEL_DEBUG, "Get-Jobs \"my-jobs\"='%s'", my_jobs ? "true" : "false");

    if (my_jobs)
    {
      if ((attr = ippFindAttribute(client->request, "requesting-user-name", IPP_TAG_NAME)) == NULL)
      {
	papplClientRespondIPP(client, IPP_STATUS_ERROR_BAD_REQUEST, "Need \"requesting-user-name\" with \"my-jobs\".");
	return;
      }

      username = ippGetString(attr, 0, NULL);

      papplLogClient(client, PAPPL_LOGLEVEL_DEBUG, "Get-Jobs \"requesting-user-name\"='%s'", username);
    }
  }

  // OK, build a list of jobs for this printer...
  ra = ippCreateRequestedArray(client->request);

  papplClientRespondIPP(client, IPP_STATUS_OK, NULL);

  pthread_rwlock_rdlock(&(client->printer->rwlock));

  for (count = 0, job = (pappl_job_t *)cupsArrayFirst(list); (limit <= 0 || count < limit) && job; job = (pappl_job_t *)cupsArrayNext(list))
  {
    // Filter out jobs that don't match...
    if (job->printer != client->printer)
      continue;

    if ((job_comparison < 0 && job->state > job_state) || (job_comparison == 0 && job->state != job_state) || (job_comparison > 0 && job->state < job_state) || job->job_id < first_job_id || (username && job->username && strcasecmp(username, job->username)))
      continue;

    if (count > 0)
      ippAddSeparator(client->response);

    count ++;
    copy_job_attributes(client, job, ra);
  }

  cupsArrayDelete(ra);

  pthread_rwlock_unlock(&(client->printer->rwlock));
}


//
// 'ipp_get_printer_attributes()' - Get the attributes for a printer object.
//

static void
ipp_get_printer_attributes(
    pappl_client_t *client)		// I - Client
{
  cups_array_t		*ra;		// Requested attributes array
  pappl_printer_t	*printer = client->printer;
					// Printer


  if (!printer->device_in_use && !printer->processing_job && (time(NULL) - printer->status_time) > 1 && printer->driver_data.status)
  {
    // Update printer status...
    (printer->driver_data.status)(printer);
    printer->status_time = time(NULL);
  }

  // Send the attributes...
  ra = ippCreateRequestedArray(client->request);

  papplClientRespondIPP(client, IPP_STATUS_OK, NULL);

  pthread_rwlock_rdlock(&(printer->rwlock));

  copy_printer_attributes(client, printer, ra);

  pthread_rwlock_unlock(&(printer->rwlock));

  cupsArrayDelete(ra);
}


//
// 'ipp_get_printers()' - Get printers.
//

static void
ipp_get_printers(
    pappl_client_t *client)		// I - Client
{
  pappl_system_t	*system = client->system;
					// System
  cups_array_t		*ra;		// Requested attributes array
  int			i,		// Looping var
			limit;		// Maximum number to return
  pappl_printer_t	*printer;	// Current printer


  // Get request attributes...
  limit = ippGetInteger(ippFindAttribute(client->request, "limit", IPP_TAG_INTEGER), 0);
  ra    = ippCreateRequestedArray(client->request);

  papplClientRespondIPP(client, IPP_STATUS_OK, NULL);

  pthread_rwlock_rdlock(&system->rwlock);

  for (i = 0, printer = (pappl_printer_t *)cupsArrayFirst(system->printers); printer; i ++, printer = (pappl_printer_t *)cupsArrayNext(system->printers))
  {
    if (limit && i >= limit)
      break;

    if (i)
      ippAddSeparator(client->response);

    pthread_rwlock_rdlock(&printer->rwlock);
    copy_printer_attributes(client, printer, ra);
    pthread_rwlock_unlock(&printer->rwlock);
  }

  pthread_rwlock_unlock(&system->rwlock);

  cupsArrayDelete(ra);
}


//
// 'ipp_get_system_attributes()' - Get system attributes.
//

static void
ipp_get_system_attributes(
    pappl_client_t *client)		// I - Client
{
  pappl_system_t	*system = client->system;
					// System
  cups_array_t		*ra;		// Requested attributes array
  int			i;		// Looping var
  pappl_printer_t	*printer;	// Current printer
  ipp_attribute_t	*attr;		// Current attribute
  ipp_t			*col;		// configured-printers value
  time_t		config_time = system->config_time;
					// system-config-change-[date-]time value
  time_t		state_time = 0;	// system-state-change-[date-]time value


  ra = ippCreateRequestedArray(client->request);

  papplClientRespondIPP(client, IPP_STATUS_OK, NULL);

  pthread_rwlock_rdlock(&system->rwlock);

  if (!ra || cupsArrayFind(ra, "printer-creation-attributes-supported"))
  {
    static const char * const values[] =
    {					// Values
      "copies-default",
      "finishings-col-default",
      "finishings-default",
      "media-col-default",
      "media-default",
      "orientation-requested-default",
      "print-color-mode-default",
      "print-content-optimize-default",
      "print-quality-default",
      "printer-contact-col",
      "printer-device-id",
      "printer-dns-sd-name",
      "printer-geo-location",
      "printer-location",
      "printer-name",
      "printer-resolution-default",
      "smi2699-device-command",
      "smi2699-device-uri"
    };

    ippAddStrings(client->response, IPP_TAG_SYSTEM, IPP_CONST_TAG(IPP_TAG_KEYWORD), "printer-creation-attributes-supported", (int)(sizeof(values) / sizeof(values[0])), NULL, values);
  }

  if (system->num_pdrivers > 0 && (!ra || cupsArrayFind(ra, "smi2699-device-command-supported")))
    ippAddStrings(client->response, IPP_TAG_SYSTEM, IPP_CONST_TAG(IPP_TAG_NAME), "smi2699-device-command-supported", system->num_pdrivers, NULL, system->pdrivers);

  if (!ra || cupsArrayFind(ra, "smi2699-device-uri-schemes-supported"))
  {
    static const char * const values[] =
    {					// Values
      "file",
      "socket",
      "usb"
    };

    ippAddStrings(client->response, IPP_TAG_SYSTEM, IPP_CONST_TAG(IPP_TAG_URISCHEME), "smi2699-device-uri-schemes-supported", (int)(sizeof(values) / sizeof(values[0])), NULL, values);
  }

  if (!ra || cupsArrayFind(ra, "system-config-change-date-time") || cupsArrayFind(ra, "system-config-change-time"))
  {
    for (printer = (pappl_printer_t *)cupsArrayFirst(system->printers); printer; printer = (pappl_printer_t *)cupsArrayNext(system->printers))
    {
      if (config_time < printer->config_time)
        config_time = printer->config_time;
    }

    if (!ra || cupsArrayFind(ra, "system-config-change-date-time"))
      ippAddDate(client->response, IPP_TAG_SYSTEM, "system-config-change-date-time", ippTimeToDate(config_time));

    if (!ra || cupsArrayFind(ra, "system-config-change-time"))
      ippAddInteger(client->response, IPP_TAG_SYSTEM, IPP_TAG_INTEGER, "system-config-change-time", (int)(config_time - system->start_time));
  }

  if (!ra || cupsArrayFind(ra, "system-configured-printers"))
  {
    attr = ippAddCollections(client->response, IPP_TAG_SYSTEM, "system-configured-printers", cupsArrayCount(system->printers), NULL);

    for (i = 0, printer = (pappl_printer_t *)cupsArrayFirst(system->printers); printer; i ++, printer = (pappl_printer_t *)cupsArrayNext(system->printers))
    {
      col = ippNew();

      pthread_rwlock_rdlock(&printer->rwlock);

      ippAddInteger(col, IPP_TAG_SYSTEM, IPP_TAG_INTEGER, "printer-id", printer->printer_id);
      ippAddString(col, IPP_TAG_SYSTEM, IPP_TAG_TEXT, "printer-info", NULL, printer->name);
      ippAddBoolean(col, IPP_TAG_SYSTEM, "printer-is-accepting-jobs", 1);
      ippAddString(col, IPP_TAG_SYSTEM, IPP_TAG_TEXT, "printer-name", NULL, printer->name);
      ippAddString(col, IPP_TAG_SYSTEM, IPP_TAG_KEYWORD, "printer-service-type", NULL, "print");
      copy_printer_state(col, printer, NULL);
      copy_printer_xri(client, col, printer);

      pthread_rwlock_unlock(&printer->rwlock);

      ippSetCollection(client->response, &attr, i, col);
      ippDelete(col);
    }
  }

  if (!ra || cupsArrayFind(ra, "system-contact-col"))
  {
    ipp_t *col = _papplContactExport(&system->contact);
    ippAddCollection(client->response, IPP_TAG_SYSTEM, "system-contact-col", col);
    ippDelete(col);
  }

  if (!ra || cupsArrayFind(ra, "system-current-time"))
    ippAddDate(client->response, IPP_TAG_SYSTEM, "system-current-time", ippTimeToDate(time(NULL)));

  if (!ra || cupsArrayFind(ra, "system-default-printer-id"))
    ippAddInteger(client->response, IPP_TAG_SYSTEM, IPP_TAG_INTEGER, "system-default-printer-id", system->default_printer_id);

  _papplSystemExportVersions(system, client->response, IPP_TAG_SYSTEM, ra);

  if (!ra || cupsArrayFind(ra, "system-geo-location"))
  {
    if (system->geo_location)
      ippAddString(client->response, IPP_TAG_SYSTEM, IPP_TAG_URI, "system-geo-location", NULL, system->geo_location);
    else
      ippAddOutOfBand(client->response, IPP_TAG_SYSTEM, IPP_TAG_UNKNOWN, "system-geo-location");
  }

  if (!ra || cupsArrayFind(ra, "system-location"))
    ippAddString(client->response, IPP_TAG_SYSTEM, IPP_TAG_TEXT, "system-location", NULL, system->location ? system->location : "");

  if (!ra || cupsArrayFind(ra, "system-mandatory-printer-attributes"))
  {
    static const char * const values[] =
    {					// Values
      "printer-name",
      "smi2699-device-command",
      "smi2699-device-uri"
    };

    ippAddStrings(client->response, IPP_TAG_SYSTEM, IPP_CONST_TAG(IPP_TAG_KEYWORD), "system-mandatory-printer-attributes", (int)(sizeof(values) / sizeof(values[0])), NULL, values);
  }

  if (!ra || cupsArrayFind(ra, "system-organization"))
    ippAddString(client->response, IPP_TAG_SYSTEM, IPP_TAG_TEXT, "system-organization", NULL, system->organization ? system->organization : "");

  if (!ra || cupsArrayFind(ra, "system-organizational-unit"))
    ippAddString(client->response, IPP_TAG_SYSTEM, IPP_TAG_TEXT, "system-organizational-unit", NULL, system->org_unit ? system->org_unit : "");

  if (!ra || cupsArrayFind(ra, "system-settable-attributes-supported"))
  {
    static const char * const values[] =
    {					// Values
      "system-contact-col",
      "system-default-printer-id",
      "system-dns-sd-name",
      "system-geo-location",
      "system-location",
      "system-name",
      "system-organization",
      "system-organizational-unit"
    };

    ippAddStrings(client->response, IPP_TAG_SYSTEM, IPP_CONST_TAG(IPP_TAG_KEYWORD), "system-settable-attributes-supported", (int)(sizeof(values) / sizeof(values[0])), NULL, values);
  }

  if (!ra || cupsArrayFind(ra, "system-state"))
  {
    int	state = IPP_PSTATE_IDLE;	// System state

    for (printer = (pappl_printer_t *)cupsArrayFirst(system->printers); printer; printer = (pappl_printer_t *)cupsArrayNext(system->printers))
    {
      if (printer->state == IPP_PSTATE_PROCESSING)
      {
        state = IPP_PSTATE_PROCESSING;
        break;
      }
    }

    ippAddInteger(client->response, IPP_TAG_SYSTEM, IPP_TAG_ENUM, "system-state", state);
  }

  if (!ra || cupsArrayFind(ra, "system-state-change-date-time") || cupsArrayFind(ra, "system-state-change-time"))
  {
    for (printer = (pappl_printer_t *)cupsArrayFirst(system->printers); printer; printer = (pappl_printer_t *)cupsArrayNext(system->printers))
    {
      if (state_time < printer->state_time)
        state_time = printer->state_time;
    }

    if (!ra || cupsArrayFind(ra, "system-state-change-date-time"))
      ippAddDate(client->response, IPP_TAG_SYSTEM, "system-state-change-date-time", ippTimeToDate(state_time));

    if (!ra || cupsArrayFind(ra, "system-state-change-time"))
      ippAddInteger(client->response, IPP_TAG_SYSTEM, IPP_TAG_INTEGER, "system-state-change-time", (int)(state_time - system->start_time));
  }

  if (!ra || cupsArrayFind(ra, "system-state-reasons"))
  {
    pappl_preason_t	state_reasons = PAPPL_PREASON_NONE;

    for (printer = (pappl_printer_t *)cupsArrayFirst(system->printers); printer; printer = (pappl_printer_t *)cupsArrayNext(system->printers))
    {
      state_reasons |= printer->state_reasons;
    }

    if (state_reasons == PAPPL_PREASON_NONE)
    {
      ippAddString(client->response, IPP_TAG_SYSTEM, IPP_CONST_TAG(IPP_TAG_KEYWORD), "system-state-reasons", NULL, "none");
    }
    else
    {
      ipp_attribute_t	*attr = NULL;		// printer-state-reasons
      pappl_preason_t	bit;			// Reason bit

      for (bit = PAPPL_PREASON_OTHER; bit <= PAPPL_PREASON_TONER_LOW; bit *= 2)
      {
        if (state_reasons & bit)
	{
	  if (attr)
	    ippSetString(client->response, &attr, ippGetCount(attr), _papplPrinterReasonString(bit));
	  else
	    attr = ippAddString(client->response, IPP_TAG_SYSTEM, IPP_TAG_KEYWORD, "system-state-reasons", NULL, _papplPrinterReasonString(bit));
	}
      }
    }
  }

  if (!ra || cupsArrayFind(ra, "system-up-time"))
    ippAddInteger(client->response, IPP_TAG_SYSTEM, IPP_TAG_INTEGER, "system-up-time", (int)(time(NULL) - system->start_time));

  pthread_rwlock_unlock(&system->rwlock);

  cupsArrayDelete(ra);
}


//
// 'ipp_identify_printer()' - Beep or display a message.
//

static void
ipp_identify_printer(
    pappl_client_t *client)		// I - Client
{
  int			i;		// Looping var
  ipp_attribute_t	*attr;		// IPP attribute
  pappl_identify_actions_t actions;	// "identify-actions" value
  const char		*message;	// "message" value


  if (client->printer->driver_data.identify)
  {
    if ((attr = ippFindAttribute(client->request, "identify-actions", IPP_TAG_KEYWORD)) != NULL)
    {
      actions = PAPPL_IDENTIFY_ACTIONS_NONE;

      for (i = 0; i < ippGetCount(attr); i ++)
	actions |= _papplIdentifyActionsValue(ippGetString(attr, i, NULL));
    }
    else
      actions = client->printer->driver_data.identify_default;

    if ((attr = ippFindAttribute(client->request, "message", IPP_TAG_TEXT)) != NULL)
      message = ippGetString(attr, 0, NULL);
    else
      message = NULL;

    (client->printer->driver_data.identify)(client->printer, actions, message);
  }

  papplClientRespondIPP(client, IPP_STATUS_OK, NULL);
}


//
// 'ipp_pause_printer()' - Stop a printer.
//

static void
ipp_pause_printer(
    pappl_client_t *client)		// I - Client
{
  http_status_t	auth_status;		// Authorization status


  // Verify the connection is authorized...
  if ((auth_status = papplClientIsAuthorized(client)) != HTTP_STATUS_CONTINUE)
  {
    papplClientRespondHTTP(client, auth_status, NULL, NULL, 0, 0);
    return;
  }

  papplPrinterPause(client->printer);
  papplClientRespondIPP(client, IPP_STATUS_OK, "Printer paused.");
}


//
// 'ipp_print_job()' - Create a job object with an attached document.
//

static void
ipp_print_job(pappl_client_t *client)	// I - Client
{
  pappl_job_t		*job;		// New job


  // Do we have a file to print?
  if (!have_document_data(client))
  {
    papplClientRespondIPP(client, IPP_STATUS_ERROR_BAD_REQUEST, "No file in request.");
    return;
  }

  // Validate print job attributes...
  if (!valid_job_attributes(client))
  {
    flush_document_data(client);
    return;
  }

  // Create the job...
  if ((job = create_job(client)) == NULL)
  {
    papplClientRespondIPP(client, IPP_STATUS_ERROR_BUSY, "Currently printing another job.");
    return;
  }

  // Then finish getting the document data and process things...
  finish_document_data(client, job);
}


//
// 'ipp_resume_printer()' - Start a printer.
//

static void
ipp_resume_printer(
    pappl_client_t *client)		// I - Client
{
  http_status_t	auth_status;		// Authorization status


  // Verify the connection is authorized...
  if ((auth_status = papplClientIsAuthorized(client)) != HTTP_STATUS_CONTINUE)
  {
    papplClientRespondHTTP(client, auth_status, NULL, NULL, 0, 0);
    return;
  }

  papplPrinterResume(client->printer);
  papplClientRespondIPP(client, IPP_STATUS_OK, "Printer resumed.");
}


//
// 'ipp_send_document()' - Add an attached document to a job object created with
//                         Create-Job.
//

static void
ipp_send_document(
    pappl_client_t *client)		// I - Client
{
  pappl_job_t	*job = client->job;	// Job information
  ipp_attribute_t *attr;		// Current attribute
  bool		have_data;		// Do we have document data?


  // Get the job...
  if (!job)
  {
    papplClientRespondIPP(client, IPP_STATUS_ERROR_NOT_FOUND, "Job does not exist.");
    flush_document_data(client);
    return;
  }

  // See if we already have a document for this job or the job has already
  // in a non-pending state...
  have_data = have_document_data(client);

  if (have_data)
  {
    if (job->filename || job->fd >= 0 || job->streaming)
    {
      papplClientRespondIPP(client, IPP_STATUS_ERROR_MULTIPLE_JOBS_NOT_SUPPORTED, "Multiple document jobs are not supported.");
      flush_document_data(client);
      return;
    }
    else if (job->state > IPP_JSTATE_HELD)
    {
      papplClientRespondIPP(client, IPP_STATUS_ERROR_NOT_POSSIBLE, "Job is not in a pending state.");
      flush_document_data(client);
      return;
    }
  }

  // Make sure we have the "last-document" operation attribute...
  if ((attr = ippFindAttribute(client->request, "last-document", IPP_TAG_ZERO)) == NULL)
  {
    papplClientRespondIPP(client, IPP_STATUS_ERROR_BAD_REQUEST, "Missing required \"last-document\" attribute.");
    flush_document_data(client);
    return;
  }
  else if (ippGetGroupTag(attr) != IPP_TAG_OPERATION)
  {
    papplClientRespondIPP(client, IPP_STATUS_ERROR_BAD_REQUEST, "The \"last-document\" attribute is not in the operation group.");
    flush_document_data(client);
    return;
  }
  else if (ippGetValueTag(attr) != IPP_TAG_BOOLEAN || ippGetCount(attr) != 1)
  {
    respond_unsupported(client, attr);
    flush_document_data(client);
    return;
  }

  // Validate document attributes...
  if (have_data && !valid_doc_attributes(client))
  {
    flush_document_data(client);
    return;
  }

  if (!have_data && !job->filename)
    job->state = IPP_JSTATE_ABORTED;

  // Then finish getting the document data and process things...
  pthread_rwlock_wrlock(&(client->printer->rwlock));

  _papplCopyAttributes(job->attrs, client->request, NULL, IPP_TAG_JOB, 0);

  if ((attr = ippFindAttribute(job->attrs, "document-format-detected", IPP_TAG_MIMETYPE)) != NULL)
    job->format = ippGetString(attr, 0, NULL);
  else if ((attr = ippFindAttribute(job->attrs, "document-format-supplied", IPP_TAG_MIMETYPE)) != NULL)
    job->format = ippGetString(attr, 0, NULL);
  else
    job->format = client->printer->driver_data.format;

  pthread_rwlock_unlock(&(client->printer->rwlock));

  if (have_data)
    finish_document_data(client, job);
}


//
// 'ipp_set_printer_attributes()' - Set printer attributes.
//

static void
ipp_set_printer_attributes(
    pappl_client_t *client)		// I - Client
{
  http_status_t	auth_status;		// Authorization status


  // Verify the connection is authorized...
  if ((auth_status = papplClientIsAuthorized(client)) != HTTP_STATUS_CONTINUE)
  {
    papplClientRespondHTTP(client, auth_status, NULL, NULL, 0, 0);
    return;
  }

  if (!set_printer_attributes(client, client->printer))
    return;

  papplClientRespondIPP(client, IPP_STATUS_OK, "Printer attributes set.");
}


//
// 'ipp_set_system_attributes()' - Set system attributes.
//

static void
ipp_set_system_attributes(
    pappl_client_t *client)		// I - Client
{
  pappl_system_t	*system = client->system;
					// System
  ipp_attribute_t	*rattr;		// Current request attribute
  ipp_tag_t		value_tag;	// Value tag
  int			count;		// Number of values
  const char		*name;		// Attribute name
  int			i;		// Looping var
  http_status_t		auth_status;	// Authorization status
  static _pappl_attr_t	sattrs[] =	// Settable system attributes
  {
    { "system-contact-col",		IPP_TAG_BEGIN_COLLECTION, 1 },
    { "system-default-printer-id",	IPP_TAG_INTEGER,	1 },
    { "system-geo-location",		IPP_TAG_URI,		1 },
    { "system-location",		IPP_TAG_TEXT,		1 },
    { "system-organization",		IPP_TAG_TEXT,		1 },
    { "system-organizational-unit",	IPP_TAG_TEXT,		1 }
  };


  // Verify the connection is authorized...
  if ((auth_status = papplClientIsAuthorized(client)) != HTTP_STATUS_CONTINUE)
  {
    papplClientRespondHTTP(client, auth_status, NULL, NULL, 0, 0);
    return;
  }

  // Preflight request attributes...
  for (rattr = ippFirstAttribute(client->request); rattr; rattr = ippNextAttribute(client->request))
  {
    papplLogClient(client, PAPPL_LOGLEVEL_DEBUG, "%s %s %s%s ...", ippTagString(ippGetGroupTag(rattr)), ippGetName(rattr), ippGetCount(rattr) > 1 ? "1setOf " : "", ippTagString(ippGetValueTag(rattr)));

    if (ippGetGroupTag(rattr) == IPP_TAG_OPERATION)
    {
      continue;
    }
    else if (ippGetGroupTag(rattr) != IPP_TAG_SYSTEM)
    {
      respond_unsupported(client, rattr);
      continue;
    }

    name      = ippGetName(rattr);
    value_tag = ippGetValueTag(rattr);
    count     = ippGetCount(rattr);

    for (i = 0; i < (int)(sizeof(sattrs) / sizeof(sattrs[0])); i ++)
    {
      if (!strcmp(name, sattrs[i].name) && value_tag == sattrs[i].value_tag && count <= sattrs[i].max_count)
        break;
    }

    if (i >= (int)(sizeof(sattrs) / sizeof(sattrs[0])))
      respond_unsupported(client, rattr);

    if (!strcmp(name, "system-default-printer-id"))
    {
      if (!papplSystemFindPrinter(system, NULL, ippGetInteger(rattr, 0), NULL))
      {
        respond_unsupported(client, rattr);
        break;
      }
    }
  }

  if (ippGetStatusCode(client->response) != IPP_STATUS_OK)
    return;

  // Now apply changes...
  pthread_rwlock_wrlock(&system->rwlock);

  for (rattr = ippFirstAttribute(client->request); rattr; rattr = ippNextAttribute(client->request))
  {
    if (ippGetGroupTag(rattr) == IPP_TAG_OPERATION)
      continue;

    name = ippGetName(rattr);

    if (!strcmp(name, "system-contact-col"))
    {
      _papplContactImport(ippGetCollection(rattr, 0), &system->contact);
    }
    else if (!strcmp(name, "system-default-printer-id"))
    {
      // Value was checked previously...
      system->default_printer_id = ippGetInteger(rattr, 0);
    }
    else if (!strcmp(name, "system-geo-location"))
    {
      free(system->geo_location);
      system->geo_location = strdup(ippGetString(rattr, 0, NULL));
    }
    else if (!strcmp(name, "system-location"))
    {
      free(system->location);
      system->location = strdup(ippGetString(rattr, 0, NULL));
    }
    else if (!strcmp(name, "system-organization"))
    {
      free(system->organization);
      system->organization = strdup(ippGetString(rattr, 0, NULL));
    }
    else if (!strcmp(name, "system-organization-unit"))
    {
      free(system->org_unit);
      system->org_unit = strdup(ippGetString(rattr, 0, NULL));
    }
  }

  system->config_changes ++;

  pthread_rwlock_unlock(&system->rwlock);

  papplClientRespondIPP(client, IPP_STATUS_OK, NULL);
}


//
// 'ipp_shutdown_all_printers()' - Shutdown the system.
//

static void
ipp_shutdown_all_printers(
    pappl_client_t *client)		// I - Client
{
  http_status_t	auth_status;		// Authorization status


  // Verify the connection is authorized...
  if ((auth_status = papplClientIsAuthorized(client)) != HTTP_STATUS_CONTINUE)
  {
    papplClientRespondHTTP(client, auth_status, NULL, NULL, 0, 0);
    return;
  }

  client->system->shutdown_time = time(NULL);

  papplClientRespondIPP(client, IPP_STATUS_OK, NULL);
}


//
// 'ipp_validate_job()' - Validate job creation attributes.
//

static void
ipp_validate_job(
    pappl_client_t *client)		// I - Client
{
  if (valid_job_attributes(client))
    papplClientRespondIPP(client, IPP_STATUS_OK, NULL);
}


//
// 'respond_unsupported()' - Respond with an unsupported attribute.
//

static void
respond_unsupported(
    pappl_client_t   *client,		// I - Client
    ipp_attribute_t *attr)		// I - Atribute
{
  ipp_attribute_t	*temp;		// Copy of attribute


  papplClientRespondIPP(client, IPP_STATUS_ERROR_ATTRIBUTES_OR_VALUES, "Unsupported %s %s%s value.", ippGetName(attr), ippGetCount(attr) > 1 ? "1setOf " : "", ippTagString(ippGetValueTag(attr)));

  temp = ippCopyAttribute(client->response, attr, 0);
  ippSetGroupTag(client->response, &temp, IPP_TAG_UNSUPPORTED_GROUP);
}


//
// 'set_printer_attributes()' - Set printer attributes.
//

static int				// O - 1 if OK, 0 on failure
set_printer_attributes(
    pappl_client_t  *client,		// I - Client
    pappl_printer_t *printer)		// I - Printer
{
  int			create_printer;	// Create-Printer request?
  ipp_attribute_t	*rattr;		// Current request attribute
  ipp_tag_t		value_tag;	// Value tag
  int			count;		// Number of values
  const char		*name;		// Attribute name
  int			i;		// Looping var
  pwg_media_t		*pwg;		// PWG media size data
  static _pappl_attr_t	pattrs[] =	// Settable printer attributes
  {
    { "label-mode-configured",		IPP_TAG_KEYWORD,	1 },
    { "label-tear-off-configured",	IPP_TAG_INTEGER,	1 },
    { "media-col-default",		IPP_TAG_BEGIN_COLLECTION, 1 },
    { "media-col-ready",		IPP_TAG_BEGIN_COLLECTION, PAPPL_MAX_SOURCE },
    { "media-default",			IPP_TAG_KEYWORD,	1 },
    { "media-ready",			IPP_TAG_KEYWORD,	PAPPL_MAX_SOURCE },
    { "orientation-requested-default",	IPP_TAG_ENUM,		1 },
    { "print-color-mode-default",	IPP_TAG_KEYWORD,	1 },
    { "print-content-optimize-default",	IPP_TAG_KEYWORD,	1 },
    { "print-darkness-default",		IPP_TAG_INTEGER,	1 },
    { "print-quality-default",		IPP_TAG_ENUM,		1 },
    { "print-speed-default",		IPP_TAG_INTEGER,	1 },
    { "printer-contact-col",		IPP_TAG_BEGIN_COLLECTION, 1 },
    { "printer-darkness-configured",	IPP_TAG_INTEGER,	1 },
    { "printer-geo-location",		IPP_TAG_URI,		1 },
    { "printer-location",		IPP_TAG_TEXT,		1 },
    { "printer-organization",		IPP_TAG_TEXT,		1 },
    { "printer-organizational-unit",	IPP_TAG_TEXT,		1 },
    { "printer-resolution-default",	IPP_TAG_RESOLUTION,	1 }
  };


  // Preflight request attributes...
  create_printer = ippGetOperation(client->request) == IPP_OP_CREATE_PRINTER;

  for (rattr = ippFirstAttribute(client->request); rattr; rattr = ippNextAttribute(client->request))
  {
    papplLogClient(client, PAPPL_LOGLEVEL_DEBUG, "%s %s %s%s ...", ippTagString(ippGetGroupTag(rattr)), ippGetName(rattr), ippGetCount(rattr) > 1 ? "1setOf " : "", ippTagString(ippGetValueTag(rattr)));

    if (ippGetGroupTag(rattr) == IPP_TAG_OPERATION || (name = ippGetName(rattr)) == NULL)
    {
      continue;
    }
    else if (ippGetGroupTag(rattr) != IPP_TAG_PRINTER)
    {
      respond_unsupported(client, rattr);
      continue;
    }

    if (create_printer && (!strcmp(name, "printer-device-id") || !strcmp(name, "printer-name") || !strcmp(name, "smi2699-device-uri") || !strcmp(name, "smi2699-device-command")))
      continue;

    value_tag = ippGetValueTag(rattr);
    count     = ippGetCount(rattr);

    // TODO: Validate values as well as names and syntax.
    for (i = 0; i < (int)(sizeof(pattrs) / sizeof(pattrs[0])); i ++)
    {
      if (!strcmp(name, pattrs[i].name) && value_tag == pattrs[i].value_tag && count <= pattrs[i].max_count)
        break;
    }

    if (i >= (int)(sizeof(pattrs) / sizeof(pattrs[0])))
      respond_unsupported(client, rattr);
  }

  if (ippGetStatusCode(client->response) != IPP_STATUS_OK)
    return (0);

  // Now apply changes...
  pthread_rwlock_wrlock(&printer->rwlock);

  for (rattr = ippFirstAttribute(client->request); rattr; rattr = ippNextAttribute(client->request))
  {
    if (ippGetGroupTag(rattr) == IPP_TAG_OPERATION || (name = ippGetName(rattr)) == NULL)
      continue;

    if (!strcmp(name, "identify-actions-default"))
    {
      printer->driver_data.identify_default = PAPPL_IDENTIFY_ACTIONS_NONE;

      for (i = 0, count = ippGetCount(rattr); i < count; i ++)
        printer->driver_data.identify_default |= _papplIdentifyActionsValue(ippGetString(rattr, i, NULL));
    }
    else if (!strcmp(name, "label-mode-configured"))
    {
      printer->driver_data.mode_configured = _papplLabelModeValue(ippGetString(rattr, 0, NULL));
    }
    else if (!strcmp(name, "label-tear-offset-configured"))
    {
      printer->driver_data.tear_offset_configured = ippGetInteger(rattr, 0);
    }
    else if (!strcmp(name, "media-col-default"))
    {
      _papplMediaColImport(ippGetCollection(rattr, 0), &printer->driver_data.media_default);
    }
    else if (!strcmp(name, "media-col-ready"))
    {
      count = ippGetCount(rattr);

      for (i = 0; i < count; i ++)
        _papplMediaColImport(ippGetCollection(rattr, i), printer->driver_data.media_ready + i);

      for (; i < PAPPL_MAX_SOURCE; i ++)
        memset(printer->driver_data.media_ready + i, 0, sizeof(pappl_media_col_t));
    }
    else if (!strcmp(name, "media-default"))
    {
      if ((pwg = pwgMediaForPWG(ippGetString(rattr, 0, NULL))) != NULL)
      {
        strlcpy(printer->driver_data.media_default.size_name, pwg->pwg, sizeof(printer->driver_data.media_default.size_name));
        printer->driver_data.media_default.size_width  = pwg->width;
        printer->driver_data.media_default.size_length = pwg->length;
      }
    }
    else if (!strcmp(name, "media-ready"))
    {
      count = ippGetCount(rattr);

      for (i = 0; i < count; i ++)
      {
        if ((pwg = pwgMediaForPWG(ippGetString(rattr, i, NULL))) != NULL)
        {
          strlcpy(printer->driver_data.media_ready[i].size_name, pwg->pwg, sizeof(printer->driver_data.media_ready[i].size_name));
	  printer->driver_data.media_ready[i].size_width  = pwg->width;
	  printer->driver_data.media_ready[i].size_length = pwg->length;
	}
      }

      for (; i < PAPPL_MAX_SOURCE; i ++)
      {
        printer->driver_data.media_ready[i].size_name[0] = '\0';
        printer->driver_data.media_ready[i].size_width   = 0;
        printer->driver_data.media_ready[i].size_length  = 0;
      }
    }
    else if (!strcmp(name, "orientation-requested-default"))
    {
      printer->driver_data.orient_default = (ipp_orient_t)ippGetInteger(rattr, 0);
    }
    else if (!strcmp(name, "print-color-mode-default"))
    {
      printer->driver_data.color_default = _papplColorModeValue(ippGetString(rattr, 0, NULL));
    }
    else if (!strcmp(name, "print-content-optimize-default"))
    {
      printer->driver_data.content_default = _papplContentValue(ippGetString(rattr, 0, NULL));
    }
    else if (!strcmp(name, "print-darkness-default"))
    {
      printer->driver_data.darkness_default = ippGetInteger(rattr, 0);
    }
    else if (!strcmp(name, "print-quality-default"))
    {
      printer->driver_data.quality_default = (ipp_quality_t)ippGetInteger(rattr, 0);
    }
    else if (!strcmp(name, "print-scaling-default"))
    {
      printer->driver_data.scaling_default = _papplScalingValue(ippGetString(rattr, 0, NULL));
    }
    else if (!strcmp(name, "print-speed-default"))
    {
      printer->driver_data.speed_default = ippGetInteger(rattr, 0);
    }
    else if (!strcmp(name, "printer-contact-col"))
    {
      _papplContactImport(ippGetCollection(rattr, 0), &printer->contact);
    }
    else if (!strcmp(name, "printer-darkness-configured"))
    {
      printer->driver_data.darkness_configured = ippGetInteger(rattr, 0);
    }
    else if (!strcmp(name, "printer-geo-location"))
    {
      free(printer->geo_location);
      printer->geo_location = strdup(ippGetString(rattr, 0, NULL));
    }
    else if (!strcmp(name, "printer-location"))
    {
      free(printer->location);
      printer->location = strdup(ippGetString(rattr, 0, NULL));
    }
    else if (!strcmp(name, "printer-organization"))
    {
      free(printer->organization);
      printer->organization = strdup(ippGetString(rattr, 0, NULL));
    }
    else if (!strcmp(name, "printer-organization-unit"))
    {
      free(printer->org_unit);
      printer->org_unit = strdup(ippGetString(rattr, 0, NULL));
    }
    else if (!strcmp(name, "printer-resolution-default"))
    {
      ipp_res_t units;			// Resolution units

      printer->driver_data.x_default = ippGetResolution(rattr, 0, &printer->driver_data.y_default, &units);
    }
  }

  printer->config_time = time(NULL);

  pthread_rwlock_unlock(&printer->rwlock);

  _papplSystemConfigChanged(client->system);

  return (1);
}


//
// 'valid_doc_attributes()' - Determine whether the document attributes are
//                            valid.
//
// When one or more document attributes are invalid, this function adds a
// suitable response and attributes to the unsupported group.
//

static int				// O - 1 if valid, 0 if not
valid_doc_attributes(
    pappl_client_t *client)		// I - Client
{
  int			valid = 1;	// Valid attributes?
  ipp_op_t		op = ippGetOperation(client->request);
					// IPP operation
  const char		*op_name = ippOpString(op);
					// IPP operation name
  ipp_attribute_t	*attr,		// Current attribute
			*supported;	// xxx-supported attribute
  const char		*compression = NULL,
					// compression value
			*format = NULL;	// document-format value


  // Check operation attributes...
  if ((attr = ippFindAttribute(client->request, "compression", IPP_TAG_ZERO)) != NULL)
  {
    // If compression is specified, only accept a supported value in a Print-Job
    // or Send-Document request...
    compression = ippGetString(attr, 0, NULL);
    supported   = ippFindAttribute(client->printer->attrs, "compression-supported", IPP_TAG_KEYWORD);

    if (ippGetCount(attr) != 1 || ippGetValueTag(attr) != IPP_TAG_KEYWORD || ippGetGroupTag(attr) != IPP_TAG_OPERATION || (op != IPP_OP_PRINT_JOB && op != IPP_OP_SEND_DOCUMENT && op != IPP_OP_VALIDATE_JOB) || !ippContainsString(supported, compression))
    {
      respond_unsupported(client, attr);
      valid = 0;
    }
    else
    {
      papplLogClient(client, PAPPL_LOGLEVEL_DEBUG, "%s \"compression\"='%s'", op_name, compression);

      ippAddString(client->request, IPP_TAG_JOB, IPP_TAG_KEYWORD, "compression-supplied", NULL, compression);

      if (strcmp(compression, "none"))
      {
        papplLogClient(client, PAPPL_LOGLEVEL_INFO, "Receiving job file with '%s' compression.", compression);
        httpSetField(client->http, HTTP_FIELD_CONTENT_ENCODING, compression);
      }
    }
  }

  // Is it a format we support?
  if ((attr = ippFindAttribute(client->request, "document-format", IPP_TAG_ZERO)) != NULL)
  {
    if (ippGetCount(attr) != 1 || ippGetValueTag(attr) != IPP_TAG_MIMETYPE || ippGetGroupTag(attr) != IPP_TAG_OPERATION)
    {
      respond_unsupported(client, attr);
      valid = 0;
    }
    else
    {
      format = ippGetString(attr, 0, NULL);

      papplLogClient(client, PAPPL_LOGLEVEL_DEBUG, "%s \"document-format\"='%s'", op_name, format);

      ippAddString(client->request, IPP_TAG_JOB, IPP_TAG_MIMETYPE, "document-format-supplied", NULL, format);
    }
  }
  else
  {
    format = ippGetString(ippFindAttribute(client->printer->attrs, "document-format-default", IPP_TAG_MIMETYPE), 0, NULL);
    if (!format)
      format = "application/octet-stream"; /* Should never happen */

    attr = ippAddString(client->request, IPP_TAG_OPERATION, IPP_TAG_MIMETYPE, "document-format", NULL, format);
  }

  if (format && !strcmp(format, "application/octet-stream") && (ippGetOperation(client->request) == IPP_OP_PRINT_JOB || ippGetOperation(client->request) == IPP_OP_SEND_DOCUMENT))
  {
    // Auto-type the file using the first N bytes of the file...
    unsigned char	header[8192];	// First 8k bytes of file
    ssize_t		headersize;	// Number of bytes read


    memset(header, 0, sizeof(header));
    headersize = httpPeek(client->http, (char *)header, sizeof(header));

    if (!memcmp(header, "%PDF", 4))
      format = "application/pdf";
    else if (!memcmp(header, "%!", 2))
      format = "application/postscript";
    else if (!memcmp(header, "\377\330\377", 3) && header[3] >= 0xe0 && header[3] <= 0xef)
      format = "image/jpeg";
    else if (!memcmp(header, "\211PNG", 4))
      format = "image/png";
    else if (!memcmp(header, "RaS2PwgR", 8))
      format = "image/pwg-raster";
    else if (!memcmp(header, "UNIRAST", 8))
      format = "image/urf";
    else if (client->system->mime_cb)
      format = (client->system->mime_cb)(header, (size_t)headersize, client->system->mime_cbdata);
    else
      format = NULL;

    papplLogClient(client, PAPPL_LOGLEVEL_DEBUG, "Auto-type header: %02X%02X%02X%02X%02X%02X%02X%02X... format: %s\n", header[0], header[1], header[2], header[3], header[4], header[5], header[6], header[7], format ? format : "unknown");

    if (format)
    {
      papplLogClient(client, PAPPL_LOGLEVEL_DEBUG, "%s Auto-typed \"document-format\"='%s'.", op_name, format);

      ippAddString(client->request, IPP_TAG_JOB, IPP_TAG_MIMETYPE, "document-format-detected", NULL, format);
    }
  }

  pthread_rwlock_rdlock(&client->printer->rwlock);

  if (op != IPP_OP_CREATE_JOB && (supported = ippFindAttribute(client->printer->attrs, "document-format-supported", IPP_TAG_MIMETYPE)) != NULL && !ippContainsString(supported, format))
  {
    respond_unsupported(client, attr);
    valid = 0;
  }

  pthread_rwlock_unlock(&client->printer->rwlock);

  return (valid);
}


//
// 'valid_job_attributes()' - Determine whether the job attributes are valid.
//
// When one or more job attributes are invalid, this function adds a suitable
// response and attributes to the unsupported group.
//

static int				// O - 1 if valid, 0 if not
valid_job_attributes(
    pappl_client_t *client)		// I - Client
{
  int			i,		// Looping var
			count,		// Number of values
			valid = 1;	// Valid attributes?
  ipp_attribute_t	*attr,		// Current attribute
			*supported;	// xxx-supported attribute


  // If a shutdown is pending, do not accept more jobs...
  if (client->system->shutdown_time)
  {
    papplClientRespondIPP(client, IPP_STATUS_ERROR_NOT_ACCEPTING_JOBS, "Not accepting new jobs.");
    return (0);
  }

  // Check operation attributes...
  valid = valid_doc_attributes(client);

  pthread_rwlock_rdlock(&client->printer->rwlock);

  // Check the various job template attributes...
  if ((attr = ippFindAttribute(client->request, "copies", IPP_TAG_ZERO)) != NULL)
  {
    if (ippGetCount(attr) != 1 || ippGetValueTag(attr) != IPP_TAG_INTEGER || ippGetInteger(attr, 0) < 1 || ippGetInteger(attr, 0) > 999)
    {
      respond_unsupported(client, attr);
      valid = 0;
    }
  }

  if ((attr = ippFindAttribute(client->request, "ipp-attribute-fidelity", IPP_TAG_ZERO)) != NULL)
  {
    if (ippGetCount(attr) != 1 || ippGetValueTag(attr) != IPP_TAG_BOOLEAN)
    {
      respond_unsupported(client, attr);
      valid = 0;
    }
  }

  if ((attr = ippFindAttribute(client->request, "job-hold-until", IPP_TAG_ZERO)) != NULL)
  {
    if (ippGetCount(attr) != 1 || (ippGetValueTag(attr) != IPP_TAG_NAME && ippGetValueTag(attr) != IPP_TAG_NAMELANG && ippGetValueTag(attr) != IPP_TAG_KEYWORD) || strcmp(ippGetString(attr, 0, NULL), "no-hold"))
    {
      respond_unsupported(client, attr);
      valid = 0;
    }
  }

  if ((attr = ippFindAttribute(client->request, "job-impressions", IPP_TAG_ZERO)) != NULL)
  {
    if (ippGetCount(attr) != 1 || ippGetValueTag(attr) != IPP_TAG_INTEGER || ippGetInteger(attr, 0) < 0)
    {
      respond_unsupported(client, attr);
      valid = 0;
    }
  }

  if ((attr = ippFindAttribute(client->request, "job-name", IPP_TAG_ZERO)) != NULL)
  {
    if (ippGetCount(attr) != 1 || (ippGetValueTag(attr) != IPP_TAG_NAME && ippGetValueTag(attr) != IPP_TAG_NAMELANG))
    {
      respond_unsupported(client, attr);
      valid = 0;
    }

    ippSetGroupTag(client->request, &attr, IPP_TAG_JOB);
  }
  else
    ippAddString(client->request, IPP_TAG_JOB, IPP_TAG_NAME, "job-name", NULL, "Untitled");

  if ((attr = ippFindAttribute(client->request, "job-priority", IPP_TAG_ZERO)) != NULL)
  {
    if (ippGetCount(attr) != 1 || ippGetValueTag(attr) != IPP_TAG_INTEGER || ippGetInteger(attr, 0) < 1 || ippGetInteger(attr, 0) > 100)
    {
      respond_unsupported(client, attr);
      valid = 0;
    }
  }

  if ((attr = ippFindAttribute(client->request, "job-sheets", IPP_TAG_ZERO)) != NULL)
  {
    if (ippGetCount(attr) != 1 || (ippGetValueTag(attr) != IPP_TAG_NAME && ippGetValueTag(attr) != IPP_TAG_NAMELANG && ippGetValueTag(attr) != IPP_TAG_KEYWORD) || strcmp(ippGetString(attr, 0, NULL), "none"))
    {
      respond_unsupported(client, attr);
      valid = 0;
    }
  }

  if ((attr = ippFindAttribute(client->request, "media", IPP_TAG_ZERO)) != NULL)
  {
    if (ippGetCount(attr) != 1 || (ippGetValueTag(attr) != IPP_TAG_NAME && ippGetValueTag(attr) != IPP_TAG_NAMELANG && ippGetValueTag(attr) != IPP_TAG_KEYWORD))
    {
      respond_unsupported(client, attr);
      valid = 0;
    }
    else
    {
      supported = ippFindAttribute(client->printer->driver_attrs, "media-supported", IPP_TAG_KEYWORD);

      if (!ippContainsString(supported, ippGetString(attr, 0, NULL)))
      {
	respond_unsupported(client, attr);
	valid = 0;
      }
    }
  }

  if ((attr = ippFindAttribute(client->request, "media-col", IPP_TAG_ZERO)) != NULL)
  {
    ipp_t		*col,		// media-col collection
			*size;		// media-size collection
    ipp_attribute_t	*member,	// Member attribute
			*x_dim,		// x-dimension
			*y_dim;		// y-dimension
    int			x_value,	// y-dimension value
			y_value;	// x-dimension value

    if (ippGetCount(attr) != 1 || ippGetValueTag(attr) != IPP_TAG_BEGIN_COLLECTION)
    {
      respond_unsupported(client, attr);
      valid = 0;
    }

    col = ippGetCollection(attr, 0);

    if ((member = ippFindAttribute(col, "media-size-name", IPP_TAG_ZERO)) != NULL)
    {
      if (ippGetCount(member) != 1 || (ippGetValueTag(member) != IPP_TAG_NAME && ippGetValueTag(member) != IPP_TAG_NAMELANG && ippGetValueTag(member) != IPP_TAG_KEYWORD))
      {
	respond_unsupported(client, attr);
	valid = 0;
      }
      else
      {
	supported = ippFindAttribute(client->printer->driver_attrs, "media-supported", IPP_TAG_KEYWORD);

	if (!ippContainsString(supported, ippGetString(member, 0, NULL)))
	{
	  respond_unsupported(client, attr);
	  valid = 0;
	}
      }
    }
    else if ((member = ippFindAttribute(col, "media-size", IPP_TAG_BEGIN_COLLECTION)) != NULL)
    {
      if (ippGetCount(member) != 1)
      {
	respond_unsupported(client, attr);
	valid = 0;
      }
      else
      {
	size = ippGetCollection(member, 0);

	if ((x_dim = ippFindAttribute(size, "x-dimension", IPP_TAG_INTEGER)) == NULL || ippGetCount(x_dim) != 1 || (y_dim = ippFindAttribute(size, "y-dimension", IPP_TAG_INTEGER)) == NULL || ippGetCount(y_dim) != 1)
	{
	  respond_unsupported(client, attr);
	  valid = 0;
	}
	else
	{
	  x_value   = ippGetInteger(x_dim, 0);
	  y_value   = ippGetInteger(y_dim, 0);
	  supported = ippFindAttribute(client->printer->driver_attrs, "media-size-supported", IPP_TAG_BEGIN_COLLECTION);
	  count     = ippGetCount(supported);

	  for (i = 0; i < count ; i ++)
	  {
	    size  = ippGetCollection(supported, i);
	    x_dim = ippFindAttribute(size, "x-dimension", IPP_TAG_ZERO);
	    y_dim = ippFindAttribute(size, "y-dimension", IPP_TAG_ZERO);

	    if (ippContainsInteger(x_dim, x_value) && ippContainsInteger(y_dim, y_value))
	      break;
	  }

	  if (i >= count)
	  {
	    respond_unsupported(client, attr);
	    valid = 0;
	  }
	}
      }
    }
  }

  if ((attr = ippFindAttribute(client->request, "multiple-document-handling", IPP_TAG_ZERO)) != NULL)
  {
    if (ippGetCount(attr) != 1 || ippGetValueTag(attr) != IPP_TAG_KEYWORD || (strcmp(ippGetString(attr, 0, NULL), "separate-documents-uncollated-copies") && strcmp(ippGetString(attr, 0, NULL), "separate-documents-collated-copies")))
    {
      respond_unsupported(client, attr);
      valid = 0;
    }
  }

  if ((attr = ippFindAttribute(client->request, "orientation-requested", IPP_TAG_ZERO)) != NULL)
  {
    if (ippGetCount(attr) != 1 || ippGetValueTag(attr) != IPP_TAG_ENUM || ippGetInteger(attr, 0) < IPP_ORIENT_PORTRAIT || ippGetInteger(attr, 0) > IPP_ORIENT_NONE)
    {
      respond_unsupported(client, attr);
      valid = 0;
    }
  }

  if ((attr = ippFindAttribute(client->request, "page-ranges", IPP_TAG_ZERO)) != NULL)
  {
    int upper = 0, lower = ippGetRange(attr, 0, &upper);
					// "page-ranges" value

    if (!ippGetBoolean(ippFindAttribute(client->printer->attrs, "page-ranges-supported", IPP_TAG_BOOLEAN), 0) || ippGetValueTag(attr) != IPP_TAG_RANGE || ippGetCount(attr) != 1 || lower < 1 || upper < lower)
    {
      respond_unsupported(client, attr);
      valid = 0;
    }
  }

  if ((attr = ippFindAttribute(client->request, "print-color-mode", IPP_TAG_ZERO)) != NULL)
  {
    pappl_color_mode_t value = _papplColorModeValue(ippGetString(attr, 0, NULL));
					// "print-color-mode" value

    if (ippGetCount(attr) != 1 || ippGetValueTag(attr) != IPP_TAG_KEYWORD || !(value & client->printer->driver_data.color_supported))
    {
      respond_unsupported(client, attr);
      valid = 0;
    }
  }

  if ((attr = ippFindAttribute(client->request, "print-content-optimize", IPP_TAG_ZERO)) != NULL)
  {
    if (ippGetCount(attr) != 1 || ippGetValueTag(attr) != IPP_TAG_KEYWORD || !_papplContentValue(ippGetString(attr, 0, NULL)))
    {
      respond_unsupported(client, attr);
      valid = 0;
    }
  }

  if ((attr = ippFindAttribute(client->request, "print-darkness", IPP_TAG_ZERO)) != NULL)
  {
    int value = ippGetInteger(attr, 0);	// "print-darkness" value

    if (ippGetCount(attr) != 1 || ippGetValueTag(attr) != IPP_TAG_INTEGER || value < -100 || value > 100 || client->printer->driver_data.darkness_supported == 0)
    {
      respond_unsupported(client, attr);
      valid = 0;
    }
  }

  if ((attr = ippFindAttribute(client->request, "print-quality", IPP_TAG_ZERO)) != NULL)
  {
    if (ippGetCount(attr) != 1 || ippGetValueTag(attr) != IPP_TAG_ENUM || ippGetInteger(attr, 0) < IPP_QUALITY_DRAFT || ippGetInteger(attr, 0) > IPP_QUALITY_HIGH)
    {
      respond_unsupported(client, attr);
      valid = 0;
    }
  }

  if ((attr = ippFindAttribute(client->request, "print-scaling", IPP_TAG_ZERO)) != NULL)
  {
    if (ippGetCount(attr) != 1 || ippGetValueTag(attr) != IPP_TAG_KEYWORD || !_papplScalingValue(ippGetString(attr, 0, NULL)))
    {
      respond_unsupported(client, attr);
      valid = 0;
    }
  }

  if ((attr = ippFindAttribute(client->request, "print-speed", IPP_TAG_ZERO)) != NULL)
  {
    int value = ippGetInteger(attr, 0);	// "print-speed" value

    if (ippGetCount(attr) != 1 || ippGetValueTag(attr) != IPP_TAG_INTEGER || value < client->printer->driver_data.speed_supported[0] || value > client->printer->driver_data.speed_supported[1] || client->printer->driver_data.speed_supported[1] == 0)
    {
      respond_unsupported(client, attr);
      valid = 0;
    }
  }

  if ((attr = ippFindAttribute(client->request, "printer-resolution", IPP_TAG_ZERO)) != NULL)
  {
    int		xdpi,			// Horizontal resolution
		ydpi;			// Vertical resolution
    ipp_res_t	units;			// Resolution units

    xdpi  = ippGetResolution(attr, 0, &ydpi, &units);

    if (ippGetCount(attr) != 1 || ippGetValueTag(attr) != IPP_TAG_RESOLUTION || units != IPP_RES_PER_INCH)
    {
      respond_unsupported(client, attr);
      valid = 0;
    }
    else
    {
      int	i;			// Looping var

      for (i = 0; i < client->printer->driver_data.num_resolution; i ++)
      {
        if (xdpi == client->printer->driver_data.x_resolution[i] && ydpi == client->printer->driver_data.y_resolution[i])
          break;
      }

      if (i >= client->printer->driver_data.num_resolution)
      {
	respond_unsupported(client, attr);
	valid = 0;
      }
    }
  }

  if ((attr = ippFindAttribute(client->request, "sides", IPP_TAG_ZERO)) != NULL)
  {
    pappl_sides_t value = _papplSidesValue(ippGetString(attr, 0, NULL));
					// "sides" value

    if (ippGetCount(attr) != 1 || ippGetValueTag(attr) != IPP_TAG_KEYWORD || !(value & client->printer->driver_data.sides_supported))
    {
      respond_unsupported(client, attr);
      valid = 0;
    }
  }

  pthread_rwlock_unlock(&client->printer->rwlock);

  return (valid);
}
