//
// IPP processing for the Printer Application Framework
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
// Local type...
//

typedef struct _pappl_attr_s		// Input attribute structure
{
  const char	*name;			// Attribute name
  ipp_tag_t	value_tag;		// Value tag
  cups_len_t	max_count;		// Max number of values
} _pappl_attr_t;

typedef struct _pappl_create_s		// Printer creation callback data
{
  pappl_client_t *client;		// Client connection
  cups_array_t	*ra;			// "requested-attributes" array
} _pappl_create_t;


//
// Local functions...
//

static void	ipp_create_printer(pappl_client_t *client);
static void	ipp_create_printers(pappl_client_t *client);
static void	ipp_delete_printer(pappl_client_t *client);
static void	ipp_disable_all_printers(pappl_client_t *client);
static void	ipp_enable_all_printers(pappl_client_t *client);
static void	ipp_find_devices(pappl_client_t *client);
static void	ipp_find_drivers(pappl_client_t *client);
static void	ipp_get_printers(pappl_client_t *client);
static void	ipp_get_system_attributes(pappl_client_t *client);
static void	ipp_pause_all_printers(pappl_client_t *client);
static void	ipp_resume_all_printers(pappl_client_t *client);
static void	ipp_set_system_attributes(pappl_client_t *client);
static void	ipp_shutdown_all_printers(pappl_client_t *client);
static void	printer_create_cb(pappl_printer_t *printer, _pappl_create_t *data);


//
// '_papplSystemProcessIPP()' - Process an IPP System request.
//

void
_papplSystemProcessIPP(
    pappl_client_t *client)		// I - Client
{
  switch ((int)ippGetOperation(client->request))
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

    case IPP_OP_PAPPL_CREATE_PRINTERS :
        ipp_create_printers(client);
        break;

    case IPP_OP_PAPPL_FIND_DEVICES :
        ipp_find_devices(client);
        break;

    case IPP_OP_PAPPL_FIND_DRIVERS :
        ipp_find_drivers(client);
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
		*device_id,		// Device ID
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

  if ((attr = ippFindAttribute(client->request, "smi55357-device-uri", IPP_TAG_ZERO)) == NULL)
  {
    papplClientRespondIPP(client, IPP_STATUS_ERROR_BAD_REQUEST, "Missing 'smi55357-device-uri' attribute in request.");
    return;
  }
  else if (ippGetGroupTag(attr) != IPP_TAG_OPERATION || ippGetValueTag(attr) != IPP_TAG_URI || ippGetCount(attr) != 1)
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

  if ((attr = ippFindAttribute(client->request, "smi55357-driver", IPP_TAG_ZERO)) == NULL)
  {
    papplClientRespondIPP(client, IPP_STATUS_ERROR_BAD_REQUEST, "Missing 'smi55357-driver' attribute in request.");
    return;
  }
  else if (ippGetGroupTag(attr) != IPP_TAG_OPERATION || ippGetValueTag(attr) != IPP_TAG_KEYWORD || ippGetCount(attr) != 1)
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
      ippAddString(client->response, IPP_TAG_UNSUPPORTED_GROUP, IPP_TAG_KEYWORD, "smi55357-driver", NULL, driver_name);
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

  _papplRWLockRead(printer->system);
    _papplRWLockRead(printer);
      _papplPrinterCopyAttributesNoLock(printer, client, ra, NULL);
      _papplPrinterRegisterDNSSDNoLock(printer);
    _papplRWUnlock(printer);
  _papplRWUnlock(printer->system);

  cupsArrayDelete(ra);
}


//
// 'ipp_create_printers()' - Auto-add newly discovered printers.
//

static void
ipp_create_printers(
    pappl_client_t *client)		// I - Client
{
  http_status_t		auth_status;	// Authorization status
  ipp_attribute_t	*type_attr;	// "smi55357-device-type" attribute
  cups_len_t		i,		// Looping var
			count;		// Number of values
  pappl_devtype_t	types;		// Device types
  _pappl_create_t	data;		// Callback data


  // Verify the connection is authorized...
  if ((auth_status = papplClientIsAuthorized(client)) != HTTP_STATUS_CONTINUE)
  {
    papplClientRespond(client, auth_status, NULL, NULL, 0, 0);
    return;
  }

  // Get the device type bits...
  if ((type_attr = ippFindAttribute(client->request, "smi55357-device-type", IPP_TAG_KEYWORD)) != NULL)
  {
    for (types = (pappl_devtype_t)0, i = 0, count = ippGetCount(type_attr); i < count; i ++)
    {
      const char *type = ippGetString(type_attr, i, NULL);
					// Current type keyword

      if (!strcmp(type, "all"))
      {
        types = PAPPL_DEVTYPE_ALL;
        break;
      }
      else if (!strcmp(type, "dns-sd"))
      {
        types |= PAPPL_DEVTYPE_DNS_SD;
      }
      else if (!strcmp(type, "local"))
      {
        types |= PAPPL_DEVTYPE_LOCAL;
      }
      else if (!strcmp(type, "network"))
      {
        types |= PAPPL_DEVTYPE_NETWORK;
      }
      else if (!strcmp(type, "other-local"))
      {
        types |= PAPPL_DEVTYPE_CUSTOM_LOCAL;
      }
      else if (!strcmp(type, "other-network"))
      {
        types |= PAPPL_DEVTYPE_CUSTOM_NETWORK;
      }
      else if (!strcmp(type, "snmp"))
      {
        types |= PAPPL_DEVTYPE_SNMP;
      }
      else if (!strcmp(type, "usb"))
      {
        types |= PAPPL_DEVTYPE_USB;
      }
      else
      {
        papplClientRespondIPPUnsupported(client, type_attr);
        return;
      }
    }
  }
  else
  {
    // Report all devices...
    types = PAPPL_DEVTYPE_ALL;
  }

  // List all devices
  data.client = client;
  data.ra     = NULL;

  if (!papplSystemCreatePrinters(client->system, types, (pappl_pr_create_cb_t)printer_create_cb, &data))
    papplClientRespondIPP(client, IPP_STATUS_ERROR_NOT_FOUND, "No devices found.");

  cupsArrayDelete(data.ra);
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

  _papplRWLockWrite(client->printer);
  if (!client->printer->processing_job)
  {
    // Not busy, delete immediately...
    _papplRWUnlock(client->printer);
    papplPrinterDelete(client->printer);
  }
  else
  {
    // Busy, delete when current job is completed...
    client->printer->is_deleted = true;
    _papplRWUnlock(client->printer);
  }

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
  _papplRWLockRead(system);
  for (printer = (pappl_printer_t *)cupsArrayGetFirst(system->printers); printer; printer = (pappl_printer_t *)cupsArrayGetNext(system->printers))
  {
    papplPrinterDisable(printer);
  }
  _papplRWUnlock(system);
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
  _papplRWLockRead(system);
  for (printer = (pappl_printer_t *)cupsArrayGetFirst(system->printers); printer; printer = (pappl_printer_t *)cupsArrayGetNext(system->printers))
  {
    papplPrinterEnable(printer);
  }
  _papplRWUnlock(system);
}


//
// 'ipp_find_devices()' - Find devices.
//

static void
ipp_find_devices(
    pappl_client_t *client)		// I - Client
{
  http_status_t		auth_status;	// Authorization status
  ipp_attribute_t	*type_attr;	// "smi55357-device-type" attribute
  cups_len_t		i,		// Looping var
			count;		// Number of values
  pappl_devtype_t	types;		// Device types
  cups_array_t		*devices;	// Device array
  _pappl_dinfo_t	*d;		// Current device information


  // Verify the connection is authorized...
  if ((auth_status = papplClientIsAuthorized(client)) != HTTP_STATUS_CONTINUE)
  {
    papplClientRespond(client, auth_status, NULL, NULL, 0, 0);
    return;
  }

  // Get the device type bits...
  if ((type_attr = ippFindAttribute(client->request, "smi55357-device-type", IPP_TAG_KEYWORD)) != NULL)
  {
    for (types = (pappl_devtype_t)0, i = 0, count = ippGetCount(type_attr); i < count; i ++)
    {
      const char *type = ippGetString(type_attr, i, NULL);
					// Current type keyword

      if (!strcmp(type, "all"))
      {
        types = PAPPL_DEVTYPE_ALL;
        break;
      }
      else if (!strcmp(type, "dns-sd"))
      {
        types |= PAPPL_DEVTYPE_DNS_SD;
      }
      else if (!strcmp(type, "local"))
      {
        types |= PAPPL_DEVTYPE_LOCAL;
      }
      else if (!strcmp(type, "network"))
      {
        types |= PAPPL_DEVTYPE_NETWORK;
      }
      else if (!strcmp(type, "other-local"))
      {
        types |= PAPPL_DEVTYPE_CUSTOM_LOCAL;
      }
      else if (!strcmp(type, "other-network"))
      {
        types |= PAPPL_DEVTYPE_CUSTOM_NETWORK;
      }
      else if (!strcmp(type, "snmp"))
      {
        types |= PAPPL_DEVTYPE_SNMP;
      }
      else if (!strcmp(type, "usb"))
      {
        types |= PAPPL_DEVTYPE_USB;
      }
      else
      {
        papplClientRespondIPPUnsupported(client, type_attr);
        return;
      }
    }
  }
  else
  {
    // Report all devices...
    types = PAPPL_DEVTYPE_ALL;
  }

  // List all devices
  papplClientRespondIPP(client, IPP_STATUS_OK, NULL);

  devices = _papplDeviceInfoCreateArray();

  papplDeviceList(types, (pappl_device_cb_t)_papplDeviceInfoCallback, devices, papplLogDevice, client->system);

  if (cupsArrayGetCount(devices) == 0)
  {
    // No devices...
    papplClientRespondIPP(client, IPP_STATUS_ERROR_NOT_FOUND, "No devices found.");
  }
  else
  {
    // Respond with collection attribute...
    ipp_attribute_t	*device_col = NULL;
					// smi55357-device-col attribute

    papplClientRespondIPP(client, IPP_STATUS_OK, NULL);

    for (d = (_pappl_dinfo_t *)cupsArrayGetFirst(devices); d; d = (_pappl_dinfo_t *)cupsArrayGetNext(devices))
    {
      ipp_t	*col;			// Collection value

      // Create a new collection value for "smi55357-device-col"...
      col = ippNew();
      ippAddString(col, IPP_TAG_ZERO, IPP_TAG_TEXT, "smi55357-device-id", NULL, d->device_id);
      ippAddString(col, IPP_TAG_ZERO, IPP_TAG_TEXT, "smi55357-device-info", NULL, d->device_info);
      ippAddString(col, IPP_TAG_ZERO, IPP_TAG_URI, "smi55357-device-uri", NULL, d->device_uri);

      // Add or update the attribute...
      if (device_col)
	ippSetCollection(client->response, &device_col, ippGetCount(device_col), col);
      else
	device_col = ippAddCollection(client->response, IPP_TAG_SYSTEM, "smi55357-device-col", col);

      ippDelete(col);
    }
  }

  cupsArrayDelete(devices);
}


//
// 'ipp_find_drivers()' - Find drivers.
//

static void
ipp_find_drivers(
    pappl_client_t *client)		// I - Client
{
  http_status_t	auth_status;		// Authorization status
  const char	*device_id;		// Device ID
  cups_len_t	num_dids = 0;		// Number of key/value pairs
  cups_option_t	*dids = NULL;		// Device ID key/value pairs
  const char	*driver_name = NULL,	// Matching driver name, if any
		*cmd = NULL,		// Command set from device ID
		*make = NULL,		// Make from device ID
		*model = NULL;		// Model from device ID
  cups_len_t	i;			// Looping var
  pappl_pr_driver_t *driver;		// Current driver
  ipp_attribute_t *driver_col = NULL;	// Collection for drivers
  ipp_t		*col;			// Collection value


  // Verify the connection is authorized...
  if ((auth_status = papplClientIsAuthorized(client)) != HTTP_STATUS_CONTINUE)
  {
    papplClientRespond(client, auth_status, NULL, NULL, 0, 0);
    return;
  }

  // Assemble a list of drivers...
  papplClientRespondIPP(client, IPP_STATUS_OK, NULL);

  // See if the Client provided a device ID to match...
  if ((device_id = ippGetString(ippFindAttribute(client->request, "smi55357-device-id", IPP_TAG_TEXT), 0, NULL)) != NULL)
  {
    // Yes, filter based on device ID...
    if (client->system->autoadd_cb)
    {
      // Filter using the auto-add callback...
      if ((driver_name = (client->system->autoadd_cb)(NULL, NULL, device_id, client->system->driver_cbdata)) == NULL)
      {
        // No matching driver...
	papplClientRespondIPP(client, IPP_STATUS_ERROR_NOT_FOUND, "No matching driver found.");
        return;
      }
    }
    else
    {
      // Filter using device ID matching...
      num_dids = papplDeviceParseID(device_id, &dids);

      if ((cmd = cupsGetOption("COMMAND SET", num_dids, dids)) == NULL)
        cmd = cupsGetOption("CMD", num_dids, dids);

      if ((make = cupsGetOption("MANUFACTURER", num_dids, dids)) == NULL)
        make = cupsGetOption("MFG", num_dids, dids);

      if ((model = cupsGetOption("MODEL", num_dids, dids)) == NULL)
        model = cupsGetOption("MDL", num_dids, dids);
    }
  }

  for (i = client->system->num_drivers, driver = client->system->drivers; i > 0; i --, driver ++)
  {
    if (driver_name)
    {
      // Compare driver name...
      if (strcmp(driver_name, driver->name))
        continue;
    }
    else if (num_dids > 0)
    {
      // Compare device ID values...
      cups_len_t	num_dids2;	// Number of device ID key/value pairs
      cups_option_t	*dids2;		// Device ID key/value pairs
      const char	*cmd2,		// Command set from device ID
			*make2,		// Make from device ID
			*model2;	// Model from device ID
      bool		match = true;	// Do we have a match?

      num_dids2 = papplDeviceParseID(driver->device_id, &dids2);

      if ((cmd2 = cupsGetOption("COMMAND SET", num_dids2, dids2)) == NULL)
        cmd2 = cupsGetOption("CMD", num_dids2, dids2);

      if (cmd && cmd2 && !strstr(cmd, cmd2))
        match = false;

      if ((make2 = cupsGetOption("MANUFACTURER", num_dids2, dids2)) == NULL)
        make2 = cupsGetOption("MFG", num_dids2, dids2);

      if (make && make2 && strcmp(make, make2))
        match = false;

      if ((model2 = cupsGetOption("MODEL", num_dids2, dids2)) == NULL)
        model2 = cupsGetOption("MDL", num_dids2, dids2);

      if (model && model2 && strcmp(model, model2))
        match = false;

      cupsFreeOptions(num_dids2, dids2);

      if (!match)
        continue;
    }

    // Create a collection for the driver values...
    col = ippNew();
    if (driver->device_id)
      ippAddString(col, IPP_TAG_ZERO, IPP_TAG_TEXT, "smi55357-device-id", NULL, driver->device_id);
    ippAddString(col, IPP_TAG_ZERO, IPP_TAG_KEYWORD, "smi55357-driver", NULL, driver->name);
    ippAddString(col, IPP_TAG_ZERO, IPP_TAG_TEXT, "smi55357-driver-info", NULL, driver->description);

    if (driver_col)
      ippSetCollection(client->response, &driver_col, ippGetCount(driver_col), col);
    else
      driver_col = ippAddCollection(client->response, IPP_TAG_SYSTEM, "smi55357-driver-col", col);

    ippDelete(col);
  }

  cupsFreeOptions(num_dids, dids);

  if (!driver_col)
    papplClientRespondIPP(client, IPP_STATUS_ERROR_NOT_FOUND, "No matching drivers found.");
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
  cups_len_t		i,		// Looping var
			count,		// Number of printers
			limit;		// Maximum number to return
  pappl_printer_t	*printer;	// Current printer
  const char		*format;	// "document-format" value, if any


  // Get request attributes...
  limit  = (cups_len_t)ippGetInteger(ippFindAttribute(client->request, "limit", IPP_TAG_INTEGER), 0);
  ra     = ippCreateRequestedArray(client->request);
  format = ippGetString(ippFindAttribute(client->request, "document-format", IPP_TAG_MIMETYPE), 0, NULL);

  papplClientRespondIPP(client, IPP_STATUS_OK, NULL);

  _papplRWLockRead(system);

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

    _papplRWLockRead(printer);
    _papplPrinterCopyAttributesNoLock(printer, client, ra, format);
    _papplRWUnlock(printer);
  }

  _papplRWUnlock(system);

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
  cups_len_t		i,		// Looping var
			count;		// Count of values
  pappl_printer_t	*printer;	// Current printer
  ipp_attribute_t	*attr;		// Current attribute
  ipp_t			*col;		// configured-printers value
  time_t		config_time = system->config_time;
					// system-config-change-[date-]time value
  time_t		state_time = 0;	// system-state-change-[date-]time value


  ra = ippCreateRequestedArray(client->request);

  papplClientRespondIPP(client, IPP_STATUS_OK, NULL);

  _papplRWLockRead(system);

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

      _papplRWLockRead(printer);

      ippAddInteger(col, IPP_TAG_SYSTEM, IPP_TAG_INTEGER, "printer-id", printer->printer_id);
      ippAddString(col, IPP_TAG_SYSTEM, IPP_TAG_TEXT, "printer-info", NULL, printer->name);
      ippAddString(col, IPP_TAG_SYSTEM, IPP_TAG_TEXT, "printer-name", NULL, printer->name);
      ippAddString(col, IPP_TAG_SYSTEM, IPP_TAG_KEYWORD, "printer-service-type", NULL, "print");
      _papplPrinterCopyStateNoLock(printer, IPP_TAG_PRINTER, col, client, NULL);
      _papplPrinterCopyXRINoLock(printer, col, client);

      _papplRWUnlock(printer);

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

  _papplRWUnlock(system);

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
  _papplRWLockRead(system);
  for (printer = (pappl_printer_t *)cupsArrayGetFirst(system->printers); printer; printer = (pappl_printer_t *)cupsArrayGetNext(system->printers))
  {
    papplPrinterPause(client->printer);
  }
  _papplRWUnlock(system);
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
  _papplRWLockRead(system);
  for (printer = (pappl_printer_t *)cupsArrayGetFirst(system->printers); printer; printer = (pappl_printer_t *)cupsArrayGetNext(system->printers))
  {
    papplPrinterResume(client->printer);
  }
  _papplRWUnlock(system);
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
  cups_len_t		count;		// Number of values
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
  for (rattr = ippGetFirstAttribute(client->request); rattr; rattr = ippGetNextAttribute(client->request))
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
  _papplRWLockWrite(system);

  for (rattr = ippGetFirstAttribute(client->request); rattr; rattr = ippGetNextAttribute(client->request))
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

  _papplRWUnlock(system);

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


//
// 'printer_create_cb()' - Add a printer to the PAPPL-Create-Printers response.
//

static void
printer_create_cb(
    pappl_printer_t *printer,		// I - Printer
    _pappl_create_t *data)		// I - Callback data
{
  // Return printer information...
  if (data->ra)
  {
    // Nth printer (N > 1), need a separator...
    ippAddSeparator(data->client->response);
  }
  else
  {
    // First printer, set the response status and create the "requested" array...
    papplClientRespondIPP(data->client, IPP_STATUS_OK, NULL);

    data->ra = cupsArrayNew((cups_array_cb_t)strcmp, /*cb_data*/NULL, /*hash_cb*/NULL, /*hash_size*/0, /*copy_cb*/NULL, /*free_cb*/NULL);
    cupsArrayAdd(data->ra, "printer-id");
    cupsArrayAdd(data->ra, "printer-is-accepting-jobs");
    cupsArrayAdd(data->ra, "printer-state");
    cupsArrayAdd(data->ra, "printer-state-reasons");
    cupsArrayAdd(data->ra, "printer-uuid");
    cupsArrayAdd(data->ra, "printer-xri-supported");
  }

  // Add the printer attributes to the response...
  _papplRWLockRead(printer->system);
    _papplRWLockRead(printer);
      _papplPrinterCopyAttributesNoLock(printer, data->client, data->ra, NULL);
    _papplRWUnlock(printer);
  _papplRWUnlock(printer->system);
}
