//
// IPP processing for the Printer Application Framework
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

static void	ipp_create_printer(pappl_client_t *client);
static void	ipp_delete_printer(pappl_client_t *client);
static void	ipp_disable_all_printers(pappl_client_t *client);
static void	ipp_enable_all_printers(pappl_client_t *client);
static void	ipp_get_printers(pappl_client_t *client);
static void	ipp_get_system_attributes(pappl_client_t *client);
static void	ipp_pause_all_printers(pappl_client_t *client);
static void	ipp_resume_all_printers(pappl_client_t *client);
static void	ipp_set_system_attributes(pappl_client_t *client);
static void	ipp_shutdown_all_printers(pappl_client_t *client);

//
// '_papplSystemProcessIPP()' - Process an IPP System request.
//

void
_papplSystemProcessIPP(
    pappl_client_t *client)		// I - Client
{
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
	_papplPrinterProcessIPP(client);
	break;

    case IPP_OP_GET_SYSTEM_ATTRIBUTES :
	ipp_get_system_attributes(client);
	break;

    case IPP_OP_SET_SYSTEM_ATTRIBUTES :
	ipp_set_system_attributes(client);
	break;

    case IPP_OP_DISABLE_ALL_PRINTERS :
        ipp_disable_all_printers(client);
        break;

    case IPP_OP_ENABLE_ALL_PRINTERS :
        ipp_enable_all_printers(client);
        break;

    case IPP_OP_PAUSE_ALL_PRINTERS :
    case IPP_OP_PAUSE_ALL_PRINTERS_AFTER_CURRENT_JOB :
        ipp_pause_all_printers(client);
        break;

    case IPP_OP_RESUME_ALL_PRINTERS :
        ipp_resume_all_printers(client);
        break;

    case IPP_OP_SHUTDOWN_ALL_PRINTERS :
	ipp_shutdown_all_printers(client);
	break;

    case IPP_OP_CREATE_SYSTEM_SUBSCRIPTIONS :
        _papplSubscriptionIPPCreate(client);
        break;

    case IPP_OP_GET_SUBSCRIPTION_ATTRIBUTES :
        _papplSubscriptionIPPGetAttributes(client);
        break;

    case IPP_OP_GET_SUBSCRIPTIONS :
        _papplSubscriptionIPPList(client);
        break;

    case IPP_OP_RENEW_SUBSCRIPTION :
        _papplSubscriptionIPPRenew(client);
        break;

    case IPP_OP_CANCEL_SUBSCRIPTION :
        _papplSubscriptionIPPCancel(client);
        break;

    case IPP_OP_GET_NOTIFICATIONS :
        _papplSubscriptionIPPGetNotifications(client);
        break;

    default :
        if (client->system->op_cb && (client->system->op_cb)(client, client->system->op_cbdata))
          break;

	papplClientRespondIPP(client, IPP_STATUS_ERROR_OPERATION_NOT_SUPPORTED, "Operation not supported.");
	break;
  }
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
  pappl_printer_t *printer;		// Printer
  cups_array_t	*ra;			// Requested attributes
  http_status_t	auth_status;		// Authorization status


  // Verify the connection is authorized...
  if ((auth_status = papplClientIsAuthorized(client)) != HTTP_STATUS_CONTINUE)
  {
    papplClientRespond(client, auth_status, NULL, NULL, 0, 0);
    return;
  }

  // Is the system configured to support multiple printers?
  if (!(client->system->options & PAPPL_SOPTIONS_MULTI_QUEUE))
  {
    papplClientRespondIPP(client, IPP_STATUS_ERROR_OPERATION_NOT_SUPPORTED, "This operation is not supported.");
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
    papplClientRespondIPPUnsupported(client, attr);
    return;
  }

  if ((attr = ippFindAttribute(client->request, "printer-name", IPP_TAG_ZERO)) == NULL)
  {
    papplClientRespondIPP(client, IPP_STATUS_ERROR_BAD_REQUEST, "Missing 'printer-name' attribute in request.");
    return;
  }
  else if (ippGetGroupTag(attr) != IPP_TAG_PRINTER || (ippGetValueTag(attr) != IPP_TAG_NAME && ippGetValueTag(attr) != IPP_TAG_NAMELANG) || ippGetCount(attr) != 1 || strlen(ippGetString(attr, 0, NULL)) > 127)
  {
    papplClientRespondIPPUnsupported(client, attr);
    return;
  }
  else
    printer_name = ippGetString(attr, 0, NULL);

  if ((attr = ippFindAttribute(client->request, "printer-device-id", IPP_TAG_ZERO)) != NULL && (ippGetGroupTag(attr) != IPP_TAG_PRINTER || ippGetValueTag(attr) != IPP_TAG_TEXT || ippGetCount(attr) != 1))
  {
    papplClientRespondIPPUnsupported(client, attr);
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
    papplClientRespondIPPUnsupported(client, attr);
    return;
  }
  else
  {
    device_uri = ippGetString(attr, 0, NULL);

    if (!papplDeviceIsSupported(device_uri))
    {
      papplClientRespondIPPUnsupported(client, attr);
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
    papplClientRespondIPPUnsupported(client, attr);
    return;
  }
  else if (client->system->driver_cb)
  {
    driver_name = ippGetString(attr, 0, NULL);
  }
  else
  {
    papplLog(client->system, PAPPL_LOGLEVEL_ERROR, "No driver callback set, unable to add printer.");

    papplClientRespondIPPUnsupported(client, attr);
    return;
  }

  // Create the printer...
  if ((printer = papplPrinterCreate(client->system, 0, printer_name, driver_name, device_id, device_uri)) == NULL)
  {
    if (errno == EEXIST)
    {
      papplClientRespondIPP(client, IPP_STATUS_ERROR_NOT_POSSIBLE, "Printer name '%s' already exists.", printer_name);
      ippAddString(client->response, IPP_TAG_UNSUPPORTED_GROUP, IPP_TAG_NAME, "printer-name", NULL, printer_name);
    }
    else if (errno == EIO)
    {
      papplClientRespondIPP(client, IPP_STATUS_ERROR_ATTRIBUTES_OR_VALUES, "Driver '%s' cannot be used with this printer.", driver_name);
      ippAddString(client->response, IPP_TAG_UNSUPPORTED_GROUP, IPP_TAG_KEYWORD, "smi2699-device-command", NULL, driver_name);
    }
    else if (errno == EINVAL)
    {
      papplClientRespondIPP(client, IPP_STATUS_ERROR_ATTRIBUTES_OR_VALUES, "Printer names must start with a letter or underscore and cannot contain special characters.");
      ippAddString(client->response, IPP_TAG_UNSUPPORTED_GROUP, IPP_TAG_NAME, "printer-name", NULL, printer_name);
    }
    else
    {
      papplClientRespondIPP(client, IPP_STATUS_ERROR_INTERNAL, "An error occurred when adding the printer: %s.", strerror(errno));
    }

    return;
  }

  if (!_papplPrinterSetAttributes(client, printer))
    return;

  // Return the printer
  papplClientRespondIPP(client, IPP_STATUS_OK, NULL);

  ra = cupsArrayNew((cups_array_cb_t)strcmp, NULL, NULL, 0, NULL, NULL);
  cupsArrayAdd(ra, "printer-id");
  cupsArrayAdd(ra, "printer-is-accepting-jobs");
  cupsArrayAdd(ra, "printer-state");
  cupsArrayAdd(ra, "printer-state-reasons");
  cupsArrayAdd(ra, "printer-uuid");
  cupsArrayAdd(ra, "printer-xri-supported");

  _papplPrinterCopyAttributes(printer, client, ra, NULL);
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
    papplClientRespond(client, auth_status, NULL, NULL, 0, 0);
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
// 'ipp_disable_all_printers()' - Disable all printers.
//

static void
ipp_disable_all_printers(
    pappl_client_t *client)		// I - Client
{
  pappl_system_t	*system = client->system;
					// System
  pappl_printer_t	*printer;	// Current printer
  http_status_t		auth_status;	// Authorization status


  // Verify the connection is authorized...
  if ((auth_status = papplClientIsAuthorized(client)) != HTTP_STATUS_CONTINUE)
  {
    papplClientRespond(client, auth_status, NULL, NULL, 0, 0);
    return;
  }

  // Loop through the printers...
  pthread_rwlock_rdlock(&system->rwlock);
  for (printer = (pappl_printer_t *)cupsArrayGetFirst(system->printers); printer; printer = (pappl_printer_t *)cupsArrayGetNext(system->printers))
  {
    papplPrinterDisable(printer);
  }
  pthread_rwlock_unlock(&system->rwlock);
}


//
// 'ipp_enable_all_printers()' - Enable all printers.
//

static void
ipp_enable_all_printers(
    pappl_client_t *client)		// I - Client
{
  pappl_system_t	*system = client->system;
					// System
  pappl_printer_t	*printer;	// Current printer
  http_status_t		auth_status;	// Authorization status


  // Verify the connection is authorized...
  if ((auth_status = papplClientIsAuthorized(client)) != HTTP_STATUS_CONTINUE)
  {
    papplClientRespond(client, auth_status, NULL, NULL, 0, 0);
    return;
  }

  // Loop through the printers...
  pthread_rwlock_rdlock(&system->rwlock);
  for (printer = (pappl_printer_t *)cupsArrayGetFirst(system->printers); printer; printer = (pappl_printer_t *)cupsArrayGetNext(system->printers))
  {
    papplPrinterEnable(printer);
  }
  pthread_rwlock_unlock(&system->rwlock);
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
  size_t		i,		// Looping var
			count,		// Number of printers
			limit;		// Maximum number to return
  pappl_printer_t	*printer;	// Current printer
  const char		*format;	// "document-format" value, if any


  // Get request attributes...
  limit  = (size_t)ippGetInteger(ippFindAttribute(client->request, "limit", IPP_TAG_INTEGER), 0);
  ra     = ippCreateRequestedArray(client->request);
  format = ippGetString(ippFindAttribute(client->request, "document-format", IPP_TAG_MIMETYPE), 0, NULL);

  papplClientRespondIPP(client, IPP_STATUS_OK, NULL);

  pthread_rwlock_rdlock(&system->rwlock);

  // Enumerate the printers for the client...
  count = cupsArrayGetCount(system->printers);

  if (limit > 0 && limit < count)
    count = limit;

  for (i = 0; i < count; i ++)
  {
    printer = (pappl_printer_t *)cupsArrayGetElement(system->printers, i);

    if (limit && i >= limit)
      break;

    if (i)
      ippAddSeparator(client->response);

    pthread_rwlock_rdlock(&printer->rwlock);
    _papplPrinterCopyAttributes(printer, client, ra, format);
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
  size_t		i,		// Looping var
			count;		// Count of values
  pappl_printer_t	*printer;	// Current printer
  ipp_attribute_t	*attr;		// Current attribute
  ipp_t			*col;		// configured-printers value
  time_t		config_time = system->config_time;
					// system-config-change-[date-]time value
  time_t		state_time = 0;	// system-state-change-[date-]time value


  ra = ippCreateRequestedArray(client->request);

  papplClientRespondIPP(client, IPP_STATUS_OK, NULL);

  pthread_rwlock_rdlock(&system->rwlock);

  _papplCopyAttributes(client->response, system->attrs, ra, IPP_TAG_ZERO, IPP_TAG_CUPS_CONST);

  if (!ra || cupsArrayFind(ra, "system-config-change-date-time") || cupsArrayFind(ra, "system-config-change-time"))
  {
    for (i = 0, count = cupsArrayGetCount(system->printers); i < count; i ++)
    {
      printer = (pappl_printer_t *)cupsArrayGetElement(system->printers, i);

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
    attr = ippAddCollections(client->response, IPP_TAG_SYSTEM, "system-configured-printers", IPP_NUM_CAST cupsArrayGetCount(system->printers), NULL);

    for (i = 0, count = cupsArrayGetCount(system->printers); i < count; i ++)
    {
      printer = (pappl_printer_t *)cupsArrayGetElement(system->printers, i);

      col = ippNew();

      pthread_rwlock_rdlock(&printer->rwlock);

      ippAddInteger(col, IPP_TAG_SYSTEM, IPP_TAG_INTEGER, "printer-id", printer->printer_id);
      ippAddString(col, IPP_TAG_SYSTEM, IPP_TAG_TEXT, "printer-info", NULL, printer->name);
      ippAddString(col, IPP_TAG_SYSTEM, IPP_TAG_TEXT, "printer-name", NULL, printer->name);
      ippAddString(col, IPP_TAG_SYSTEM, IPP_TAG_KEYWORD, "printer-service-type", NULL, "print");
      _papplPrinterCopyState(printer, IPP_TAG_PRINTER, col, client, NULL);
      _papplPrinterCopyXRI(printer, col, client);

      pthread_rwlock_unlock(&printer->rwlock);

      ippSetCollection(client->response, &attr, IPP_NUM_CAST i, col);
      ippDelete(col);
    }
  }

  if (!ra || cupsArrayFind(ra, "system-contact-col"))
  {
    col = _papplContactExport(&system->contact);
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

  if (!ra || cupsArrayFind(ra, "system-name"))
    ippAddString(client->response, IPP_TAG_SYSTEM, IPP_TAG_NAME, "system-name", NULL, system->name);

  if (!ra || cupsArrayFind(ra, "system-organization"))
    ippAddString(client->response, IPP_TAG_SYSTEM, IPP_TAG_TEXT, "system-organization", NULL, system->organization ? system->organization : "");

  if (!ra || cupsArrayFind(ra, "system-organizational-unit"))
    ippAddString(client->response, IPP_TAG_SYSTEM, IPP_TAG_TEXT, "system-organizational-unit", NULL, system->org_unit ? system->org_unit : "");

  if (!ra || cupsArrayFind(ra, "system-state"))
  {
    int	state = IPP_PSTATE_IDLE;	// System state

    for (i = 0, count = cupsArrayGetCount(system->printers); i < count; i ++)
    {
      printer = (pappl_printer_t *)cupsArrayGetElement(system->printers, i);

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
    for (i = 0, count = cupsArrayGetCount(system->printers); i < count; i ++)
    {
      printer = (pappl_printer_t *)cupsArrayGetElement(system->printers, i);

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

    for (i = 0, count = cupsArrayGetCount(system->printers); i < count; i ++)
    {
      printer = (pappl_printer_t *)cupsArrayGetElement(system->printers, i);

      state_reasons |= printer->state_reasons;
    }

    if (state_reasons == PAPPL_PREASON_NONE)
    {
      ippAddString(client->response, IPP_TAG_SYSTEM, IPP_CONST_TAG(IPP_TAG_KEYWORD), "system-state-reasons", NULL, "none");
    }
    else
    {
      pappl_preason_t	bit;			// Reason bit

      for (attr = NULL, bit = PAPPL_PREASON_OTHER; bit <= PAPPL_PREASON_TONER_LOW; bit *= 2)
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

  if (system->uuid && (!ra || cupsArrayFind(ra, "system-uuid")))
    ippAddString(client->response, IPP_TAG_SYSTEM, IPP_TAG_URI, "system-uuid", NULL, system->uuid);

  if (!ra || cupsArrayFind(ra, "system-xri-supported"))
  {
    char	uri[1024];		// URI value

    httpAssembleURI(HTTP_URI_CODING_ALL, uri, sizeof(uri), (system->options & PAPPL_SOPTIONS_NO_TLS) ? "ipp" : "ipps", NULL, client->host_field, client->host_port, "/ipp/system");
    col = ippNew();
    ippAddString(col, IPP_TAG_SYSTEM, IPP_CONST_TAG(IPP_TAG_KEYWORD), "xri-authentication", NULL, client->system->auth_service ? "basic" : "none");
    ippAddString(col, IPP_TAG_SYSTEM, IPP_CONST_TAG(IPP_TAG_KEYWORD), "xri-security", NULL, (system->options & PAPPL_SOPTIONS_NO_TLS) ? "none" : "tls");
    ippAddString(col, IPP_TAG_SYSTEM, IPP_TAG_URI, "xri-uri", NULL, uri);

    ippAddCollection(client->response, IPP_TAG_SYSTEM, "system-xri-supported", col);
    ippDelete(col);
  }

  pthread_rwlock_unlock(&system->rwlock);

  cupsArrayDelete(ra);
}


//
// 'ipp_pause_all_printers()' -  all printers.
//

static void
ipp_pause_all_printers(
    pappl_client_t *client)		// I - Client
{
  pappl_system_t	*system = client->system;
					// System
  pappl_printer_t	*printer;	// Current printer
  http_status_t		auth_status;	// Authorization status


  // Verify the connection is authorized...
  if ((auth_status = papplClientIsAuthorized(client)) != HTTP_STATUS_CONTINUE)
  {
    papplClientRespond(client, auth_status, NULL, NULL, 0, 0);
    return;
  }

  // Loop through the printers...
  pthread_rwlock_rdlock(&system->rwlock);
  for (printer = (pappl_printer_t *)cupsArrayGetFirst(system->printers); printer; printer = (pappl_printer_t *)cupsArrayGetNext(system->printers))
  {
    papplPrinterPause(client->printer);
  }
  pthread_rwlock_unlock(&system->rwlock);
}


//
// 'ipp_resume_all_printers()' -  all printers.
//

static void
ipp_resume_all_printers(
    pappl_client_t *client)		// I - Client
{
  pappl_system_t	*system = client->system;
					// System
  pappl_printer_t	*printer;	// Current printer
  http_status_t		auth_status;	// Authorization status


  // Verify the connection is authorized...
  if ((auth_status = papplClientIsAuthorized(client)) != HTTP_STATUS_CONTINUE)
  {
    papplClientRespond(client, auth_status, NULL, NULL, 0, 0);
    return;
  }

  // Loop through the printers...
  pthread_rwlock_rdlock(&system->rwlock);
  for (printer = (pappl_printer_t *)cupsArrayGetFirst(system->printers); printer; printer = (pappl_printer_t *)cupsArrayGetNext(system->printers))
  {
    papplPrinterResume(client->printer);
  }
  pthread_rwlock_unlock(&system->rwlock);
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
  size_t		i;		// Looping var
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
    papplClientRespond(client, auth_status, NULL, NULL, 0, 0);
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
      papplClientRespondIPPUnsupported(client, rattr);
      continue;
    }

    name      = ippGetName(rattr);
    value_tag = ippGetValueTag(rattr);
    count     = ippGetCount(rattr);

    for (i = 0; i < (sizeof(sattrs) / sizeof(sattrs[0])); i ++)
    {
      if (!strcmp(name, sattrs[i].name) && value_tag == sattrs[i].value_tag && count <= sattrs[i].max_count)
        break;
    }

    if (i >= (sizeof(sattrs) / sizeof(sattrs[0])))
      papplClientRespondIPPUnsupported(client, rattr);

    if (!strcmp(name, "system-default-printer-id"))
    {
      if (!papplSystemFindPrinter(system, NULL, ippGetInteger(rattr, 0), NULL))
      {
        papplClientRespondIPPUnsupported(client, rattr);
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

  papplSystemAddEvent(system, NULL, NULL, PAPPL_EVENT_SYSTEM_CONFIG_CHANGED, NULL);

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
    papplClientRespond(client, auth_status, NULL, NULL, 0, 0);
    return;
  }

  client->system->shutdown_time = time(NULL);

  papplClientRespondIPP(client, IPP_STATUS_OK, NULL);
}
