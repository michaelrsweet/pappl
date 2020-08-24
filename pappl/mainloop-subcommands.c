//
// Standard papplMainloop sub-commands for the Printer Application Framework
//
// Copyright © 2020 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

//
// Include necessary headers
//

#  include "pappl-private.h"


//
// Local functions
//

static char	*copy_stdin(const char *base_name, char *name, size_t namesize);
static void	device_error_cb(const char *message, void *err_data);
static bool	device_list_cb(const char *device_uri, const char *device_id, void *data);
static char	*get_value(ipp_attribute_t *attr, const char *name, int element, char *buffer, size_t bufsize);
static void	print_option(ipp_t *response, const char *name);


//
// '_papplMainloopAddPrinter()' - Add a printer.
//

int					// O - Exit status
_papplMainloopAddPrinter(
    const char    *base_name,		// I - Base name
    int           num_options,		// I - Number of options
    cups_option_t *options)		// I - Options
{
  http_t	*http;			// Connection to server
  ipp_t		*request;		// Create-Printer request
  const char	*device_uri,		// Device URI
		*driver_name,		// Name of driver
		*printer_name,		// Name of printer
		*printer_uri;		// Printer URI


  // Get required values...
  device_uri   = cupsGetOption("smi2699-device-uri", num_options, options);
  driver_name  = cupsGetOption("smi2699-device-command", num_options, options);
  printer_name = cupsGetOption("printer-name", num_options, options);

  if (!device_uri || !driver_name || !printer_name)
  {
    if (!printer_name)
      fprintf(stderr, "%s: Missing '-d PRINTER'.\n", base_name);
    if (!driver_name)
      fprintf(stderr, "%s: Missing '-m DRIVER-NAME'.\n", base_name);
    if (!device_uri)
      fprintf(stderr, "%s: Missing '-v DEVICE-URI'.\n", base_name);

    return (1);
  }

  if ((printer_uri = cupsGetOption("printer-uri", num_options, options)) != NULL)
  {
    char	resource[1024];		// Resource path

    // Connect to the remote printer...
    if ((http = _papplMainloopConnectURI(base_name, printer_uri, resource, sizeof(resource))) == NULL)
      return (1);
  }
  else if ((http = _papplMainloopConnect(base_name, true)) == NULL)
  {
    return (1);
  }

  // Send a Create-Printer request to the server...
  request = ippNewRequest(IPP_OP_CREATE_PRINTER);
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "system-uri", NULL, "ipp://localhost/ipp/system");
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_KEYWORD, "printer-service-type", NULL, "print");
  ippAddString(request, IPP_TAG_PRINTER, IPP_TAG_NAME, "printer-name", NULL, printer_name);
  ippAddString(request, IPP_TAG_PRINTER, IPP_TAG_KEYWORD, "smi2699-device-command", NULL, driver_name);
  ippAddString(request, IPP_TAG_PRINTER, IPP_TAG_URI, "smi2699-device-uri", NULL, device_uri);

  _papplMainloopAddOptions(request, num_options, options);

  ippDelete(cupsDoRequest(http, request, "/ipp/system"));

  httpClose(http);

  if (cupsLastError() != IPP_STATUS_OK)
  {
    fprintf(stderr, "%s: Unable to add printer - %s\n", base_name, cupsLastErrorString());
    return (1);
  }

  return (0);
}


//
// '_papplMainloopCancelJob()' - Cancel job(s).
//

int					// O - Exit status
_papplMainloopCancelJob(
    const char    *base_name,		// I - Base name
    int           num_options,		// I - Number of options
    cups_option_t *options)		// I - Options
{
  const char	*printer_uri,		// Printer URI
		*printer_name;		// Printer name
  char		default_printer[256],	// Default printer
		resource[1024];		// Resource path
  http_t	*http;			// Server connection
  ipp_t		*request;		// IPP request
  const char	*value;			// Option value
  int		job_id = 0;		// job-id


  if ((printer_uri = cupsGetOption("printer-uri", num_options, options)) != NULL)
  {
    // Connect to the remote printer...
    if ((http = _papplMainloopConnectURI(base_name, printer_uri, resource, sizeof(resource))) == NULL)
      return (1);
  }
  else
  {
    // Connect to the server and get the destination printer...
    if ((http = _papplMainloopConnect(base_name, true)) == NULL)
      return (1);

    if ((printer_name = cupsGetOption("printer-name", num_options, options)) == NULL)
    {
      if ((printer_name = _papplMainloopGetDefaultPrinter(http, default_printer, sizeof(default_printer))) == NULL)
      {
        fprintf(stderr, "%s: No default printer available.\n", base_name);
        httpClose(http);
        return (1);
      }
    }
  }

  // Figure out which job(s) to cancel...
  if (cupsGetOption("cancel-all", num_options, options))
  {
    request = ippNewRequest(IPP_OP_CANCEL_MY_JOBS);
  }
  else if ((value = cupsGetOption("job-id", num_options, options)) != NULL)
  {
    request = ippNewRequest(IPP_OP_CANCEL_JOB);
    job_id  = atoi(value);
  }
  else
    request = ippNewRequest(IPP_OP_CANCEL_CURRENT_JOB);

  if (printer_uri)
    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri", NULL, printer_uri);
  else
    _papplMainloopAddPrinterURI(request, printer_name, resource, sizeof(resource));

  if (job_id)
    ippAddInteger(request, IPP_TAG_OPERATION, IPP_TAG_INTEGER, "job-id", job_id);
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name", NULL, cupsUser());

  ippDelete(cupsDoRequest(http, request, resource));
  httpClose(http);

  if (cupsLastError() != IPP_STATUS_OK)
  {
    fprintf(stderr, "%s: Unable to cancel - %s\n", base_name, cupsLastErrorString());
    return (1);
  }
    
  return (0);
}


//
// '_papplMainloopDeletePrinter()' - Delete a printer.
//

int					// O - Exit status
_papplMainloopDeletePrinter(
    const char    *base_name,		// I - Base name
    int           num_options,		// I - Number of options
    cups_option_t *options)		// I - Options
{
  const char	*printer_uri,		// Printer URI
		*printer_name;		// Printer name
  http_t	*http;			// Server connection
  ipp_t		*request,		// IPP request
		*response;		// IPP response
  char		resource[1024];		// Resource path
  int		printer_id;		// printer-id value


  // Connect to/start up the server and get the destination printer...
  if ((printer_uri = cupsGetOption("printer-uri", num_options, options)) != NULL)
  {
    // Connect to the remote printer...
    if ((http = _papplMainloopConnectURI(base_name, printer_uri, resource, sizeof(resource))) == NULL)
      return (1);
  }
  else if ((http = _papplMainloopConnect(base_name, true)) == NULL)
  {
    return (1);
  }
  else if ((printer_name = cupsGetOption("printer-name", num_options, options)) == NULL)
  {
    fprintf(stderr, "%s: Missing '-d PRINTER'.\n", base_name);
    httpClose(http);
    return (1);
  }

  // Get the printer-id for the printer we are deleting...
  request = ippNewRequest(IPP_OP_GET_PRINTER_ATTRIBUTES);
  if (printer_uri)
    ippAddString(request, IPP_TAG_PRINTER, IPP_TAG_URI, "printer-uri", NULL, printer_uri);
  else
    _papplMainloopAddPrinterURI(request, printer_name, resource, sizeof(resource));
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name", NULL, cupsUser());
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_KEYWORD, "requested-attributes", NULL, "printer-id");

  response   = cupsDoRequest(http, request, resource);
  printer_id = ippGetInteger(ippFindAttribute(response, "printer-id", IPP_TAG_INTEGER), 0);
  ippDelete(response);

  if (printer_id == 0)
  {
    fprintf(stderr, "%s: Unable to get information for '%s': %s\n", base_name, printer_name, cupsLastErrorString());
    httpClose(http);
    return (1);
  }

  // Now that we have the printer-id, delete it from the system service...
  request = ippNewRequest(IPP_OP_DELETE_PRINTER);
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "system-uri", NULL, "ipp://localhost/ipp/system");
  ippAddInteger(request, IPP_TAG_OPERATION, IPP_TAG_INTEGER, "printer-id", printer_id);
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name", NULL, cupsUser());

  ippDelete(cupsDoRequest(http, request, "/ipp/system"));
  httpClose(http);

  if (cupsLastError() != IPP_STATUS_OK)
  {
    fprintf(stderr, "%s: Unable to delete printer: %s\n", base_name, cupsLastErrorString());
    return (1);
  }

  return (0);
}


//
// '_papplMainloopGetSetDefaultPrinter()' - Get/set the default printer.
//

int					// O - Exit status
_papplMainloopGetSetDefaultPrinter(
    const char    *base_name,		// I - Base name
    int           num_options,		// I - Number of options
    cups_option_t *options)		// I - Options
{
   const char	*printer_uri,		// Printer URI
		*printer_name;		// Printer name
   http_t	*http;			// Server connection
   ipp_t	*request,		// IPP request
		*response;		// IPP response
   char		resource[1024];		// Resource path
   int		printer_id;		// printer-id value


  // Connect to/start up the server and get the destination printer...
  if ((printer_uri = cupsGetOption("printer-uri", num_options, options)) != NULL)
  {
    // Connect to the remote printer...
    if ((http = _papplMainloopConnectURI(base_name, printer_uri, resource, sizeof(resource))) == NULL)
      return (1);
  }
  else if ((http = _papplMainloopConnect(base_name, true)) == NULL)
  {
    return (1);
  }

  if ((printer_name = cupsGetOption("printer-name", num_options, options)) == NULL)
  {
    char  default_printer[256];	// Default printer

    if (_papplMainloopGetDefaultPrinter(http, default_printer, sizeof(default_printer)))
      puts(default_printer);
    else
      puts("No default printer set");

    httpClose(http);

    return (0);
  }

  // OK, setting the default printer so get the printer-id for it...
  request = ippNewRequest(IPP_OP_GET_PRINTER_ATTRIBUTES);
  if (printer_uri)
    ippAddString(request, IPP_TAG_PRINTER, IPP_TAG_URI, "printer-uri", NULL, printer_uri);
  else
    _papplMainloopAddPrinterURI(request, printer_name, resource, sizeof(resource));

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name", NULL, cupsUser());
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_KEYWORD, "requested-attributes", NULL, "printer-id");

  response   = cupsDoRequest(http, request, resource);
  printer_id = ippGetInteger(ippFindAttribute(response, "printer-id", IPP_TAG_INTEGER), 0);
  ippDelete(response);

  if (printer_id == 0)
  {
    fprintf(stderr, "%s: Unable to get information for '%s' - %s\n", base_name, printer_name, cupsLastErrorString());
    httpClose(http);
    return (1);
  }

  // Now that we have the printer-id, set the system-default-printer-id
  // attribute for the system service...
  request = ippNewRequest(IPP_OP_SET_SYSTEM_ATTRIBUTES);
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "system-uri", NULL, "ipp://localhost/ipp/system");
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name", NULL, cupsUser());
  ippAddInteger(request, IPP_TAG_SYSTEM, IPP_TAG_INTEGER, "system-default-printer-id", printer_id);

  ippDelete(cupsDoRequest(http, request, "/ipp/system"));
  httpClose(http);

  if (cupsLastError() != IPP_STATUS_OK)
  {
    fprintf(stderr, "%s: Unable to set default printer - %s\n", base_name, cupsLastErrorString());
    return (1);
  }

  return (0);
}


//
// '_papplMainloopModifyPrinter()' - Modify printer.
//

int					// O - Exit status
_papplMainloopModifyPrinter(
    const char    *base_name,		// I - Base name
    int           num_options,		// I - Number of options
    cups_option_t *options)		// I - Options
{
  http_t	*http;			// Connection to server
  ipp_t		*request;		// Create-Printer request
  const char	*printer_uri,		// Printer URI
		*printer_name;		// Name of printer
  char		resource[1024];		// Resource path


  // Open a connection to the server...
  if ((printer_uri = cupsGetOption("printer-uri", num_options, options)) != NULL)
  {
    // Connect to the remote printer...
    if ((http = _papplMainloopConnectURI(base_name, printer_uri, resource, sizeof(resource))) == NULL)
      return (1);
  }
  else if ((http = _papplMainloopConnect(base_name, true)) == NULL)
  {
    return (1);
  }
  else if ((printer_name  = cupsGetOption("printer-name", num_options, options)) == NULL)
  {
    fprintf(stderr, "%s: Missing '-d PRINTER'.\n", base_name);
    return (1);
  }

  // Send a Set-Printer-Attributes request to the server...
  request = ippNewRequest(IPP_OP_SET_PRINTER_ATTRIBUTES);
  if (printer_uri)
    ippAddString(request, IPP_TAG_PRINTER, IPP_TAG_URI, "printer-uri", NULL, printer_uri);
  else
    _papplMainloopAddPrinterURI(request, printer_name, resource, sizeof(resource));
  _papplMainloopAddOptions(request, num_options, options);

  ippDelete(cupsDoRequest(http, request, resource));

  httpClose(http);

  if (cupsLastError() != IPP_STATUS_OK)
  {
    fprintf(stderr, "%s: Unable to modify printer: %s\n", base_name, cupsLastErrorString());
    return (1);
  }

  return (0);
}


//
// '_papplMainloopRunServer()' - Run server.
//

int					// O - Exit status
_papplMainloopRunServer(
    const char           *base_name,	// I - Base name
    int                  num_options,	// I - Number of options
    cups_option_t        *options,	// I - Options
    pappl_ml_system_cb_t system_cb,	// I - System callback
    void                 *data)		// I - Callback data
{
  pappl_system_t	*system;	// System object
  char			sockname[1024];	// Socket filename


  if (!system_cb)
  {
    fprintf(stderr, "%s: No system callback specified.\n", base_name);
    return (1);
  }

  if ((system = (system_cb)(num_options, options, data)) == NULL)
  {
    fprintf(stderr, "%s: Failed to create a system.\n", base_name);
    return (1);
  }

  papplSystemAddListeners(system, _papplMainloopGetServerPath(base_name, sockname, sizeof(sockname)));

  papplSystemRun(system);
  papplSystemDelete(system);

  return (0);
}


//
// '_papplMainlooploopShowDevices()' - Show available devices.
//

int					// O - Exit status
_papplMainloopShowDevices(
    const char    *base_name,		// I - Basename of application
    int           num_options,		// I - Number of options
    cups_option_t *options)		// I - Options
{
  papplDeviceList(PAPPL_DTYPE_ALL, (pappl_device_cb_t)device_list_cb, (void *)cupsGetOption("verbose", num_options, options), (pappl_deverror_cb_t)device_error_cb, (void *)base_name);

  return (0);
}


//
// '_papplMainlooploopShowDrivers()' - Show available drivers.
//

int					// O - Exit status
_papplMainloopShowDrivers(
    const char           *base_name,	// I - Basename of application
    int                  num_options,	// I - Number of options
    cups_option_t        *options,	// I - Options
    pappl_ml_system_cb_t system_cb,	// I - System callback
    void                 *data)		// I - Callback data
{
  int			i;		// Looping variable
  pappl_system_t	*system;	// System object


  if (!system_cb)
  {
    fprintf(stderr, "%s: No system callback specified.\n", base_name);
    return (1);
  }

  if ((system = (system_cb)(num_options, options, data)) == NULL)
  {
    fprintf(stderr, "%s: Failed to create a system.\n", base_name);
    return (1);
  }

  for (i = 0; i < system->num_pdrivers; i ++)
    printf("%-39s %s\n", system->pdrivers[i], system->pdrivers_desc[i]);

  papplSystemDelete(system);

  return (0);
}


//
// '_papplMainloopShowJobs()' - Show pending printer jobs.
//

int					// O - Exit status
_papplMainloopShowJobs(
    const char    *base_name,		// I - Base name
    int           num_options,		// I - Number of options
    cups_option_t *options)		// I - Options
{
  const char	*printer_uri,		// Printer URI
		*printer_name;		// Printer name
  char		default_printer[256],	// Default printer
		resource[1024];		// Resource path
  http_t	*http;			// Server connection
  ipp_t		*request,		// IPP request
		*response;		// IPP response
  ipp_attribute_t *attr;		// Current attribute
  const char	*attrname;		// Attribute name
  int		job_id,			// Current job-id
		job_state;		// Current job-state
  const char	*job_name,		// Current job-name
		*job_user;		// Current job-originating-user-name


  if ((printer_uri = cupsGetOption("printer-uri", num_options, options)) != NULL)
  {
    // Connect to the remote printer...
    if ((http = _papplMainloopConnectURI(base_name, printer_uri, resource, sizeof(resource))) == NULL)
      return (1);
  }
  else
  {
    // Connect to/start up the server and get the destination printer...
    if ((http = _papplMainloopConnect(base_name, true)) == NULL)
      return (1);

    if ((printer_name = cupsGetOption("printer-name", num_options, options)) == NULL)
    {
      if ((printer_name = _papplMainloopGetDefaultPrinter(http, default_printer, sizeof(default_printer))) == NULL)
      {
        fprintf(stderr, "%s: No default printer available.\n", base_name);
        httpClose(http);
        return (1);
      }
    }
  }

  // Send a Get-Jobs request...
  request = ippNewRequest(IPP_OP_GET_JOBS);
  if (printer_uri)
    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri", NULL, printer_uri);
  else
    _papplMainloopAddPrinterURI(request, printer_name, resource, sizeof(resource));
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name", NULL, cupsUser());
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_KEYWORD, "which-jobs", NULL, "all");

  response = cupsDoRequest(http, request, resource);

  for (attr = ippFirstAttribute(response); attr; attr = ippNextAttribute(response))
  {
    if (ippGetGroupTag(attr) == IPP_TAG_OPERATION)
      continue;

    job_id    = 0;
    job_state = IPP_JSTATE_PENDING;
    job_name  = "(none)";
    job_user  = "(unknown)";

    while (ippGetGroupTag(attr) == IPP_TAG_JOB)
    {
      attrname = ippGetName(attr);
      if (!strcmp(attrname, "job-id"))
        job_id = ippGetInteger(attr, 0);
      else if (!strcmp(attrname, "job-name"))
        job_name = ippGetString(attr, 0, NULL);
      else if (!strcmp(attrname, "job-originating-user-name"))
        job_user = ippGetString(attr, 0, NULL);
      else if (!strcmp(attrname, "job-state"))
        job_state = ippGetInteger(attr, 0);

      attr = ippNextAttribute(response);
    }

    printf("%d %-12s %-16s %s\n", job_id, ippEnumString("job-state", job_state), job_user, job_name);
  }

  ippDelete(response);
  httpClose(http);

  return (0);
}


//
// '_papplMainloopShowOptions()' - Show supported option.
//

int					// O - Exit status
_papplMainloopShowOptions(
    const char    *base_name,		// I - Base name
    int           num_options,		// I - Number of options
    cups_option_t *options)		// I - Options
{
  const char	*printer_uri,		// Printer URI
		*printer_name;		// Printer name
  char		default_printer[256];	// Default printer name
  http_t	*http;			// Server connection
  ipp_t		*request,		// IPP request
		*response;		// IPP response
  char		resource[1024];		// Resource path


  if ((printer_uri = cupsGetOption("printer-uri", num_options, options)) != NULL)
  {
    // Connect to the remote printer...
    if ((http = _papplMainloopConnectURI(base_name, printer_uri, resource, sizeof(resource))) == NULL)
      return (1);
  }
  else
  {
    // Connect to/start up the server and get the destination printer...
    if ((http = _papplMainloopConnect(base_name, true)) == NULL)
      return (1);

    if ((printer_name = cupsGetOption("printer-name", num_options, options)) == NULL)
    {
      if ((printer_name = _papplMainloopGetDefaultPrinter(http, default_printer, sizeof(default_printer))) == NULL)
      {
        fprintf(stderr, "%s: No default printer available.\n", base_name);
        httpClose(http);
        return (1);
      }
    }
  }

  // Get the xxx-supported and xxx-default attributes
  request = ippNewRequest(IPP_OP_GET_PRINTER_ATTRIBUTES);
  if (printer_uri)
    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri", NULL, printer_uri);
  else
    _papplMainloopAddPrinterURI(request, printer_name, resource, sizeof(resource));
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name", NULL, cupsUser());

  response = cupsDoRequest(http, request, resource);

  if (cupsLastError() != IPP_STATUS_OK)
  {
    fprintf(stderr, "%s: Unable to get printer options: %s\n", base_name, cupsLastErrorString());
    ippDelete(response);
    httpClose(http);
    return (1);
  }

  printf("Print job options:\n");
  printf("  -c copies\n");
  print_option(response, "media");
  print_option(response, "media-source");
  print_option(response, "media-top-offset");
  print_option(response, "media-tracking");
  print_option(response, "media-type");
  print_option(response, "orientation-requested");
  print_option(response, "print-color-mode");
  print_option(response, "print-content-optimize");
  if (ippFindAttribute(response, "print-darkness-supported", IPP_TAG_ZERO))
    printf("  -o print-darkness=-100 to 100\n");
  print_option(response, "print-quality");
  print_option(response, "print-speed");
  print_option(response, "printer-resolution");
  printf("\n");

  printf("Printer options:\n");
  print_option(response, "label-mode");
  print_option(response, "label-tear-offset");
  if (ippFindAttribute(response, "printer-darkness-supported", IPP_TAG_ZERO))
    printf("  -o printer-darkness=0 to 100\n");
  printf("  -o printer-geo-location='geo:LATITUDE,LONGITUDE'\n");
  printf("  -o printer-location='LOCATION'\n");
  printf("  -o printer-organization='ORGANIZATION'\n");
  printf("  -o printer-organizational-unit='UNIT/SECTION'\n");

  ippDelete(response);
  httpClose(http);

  return (0);
}


//
// 'papplMainShowPrinters()' - Show printer queues.
//

int					// O - Exit status
_papplMainloopShowPrinters(
    const char    *base_name,		// I - Base name
    int           num_options,		// I - Number of options
    cups_option_t *options)		// I - Options
{
  http_t	*http;			// Server connection
  ipp_t		*request,		// IPP request
		*response;		// IPP response
  ipp_attribute_t *attr;		// Current attribute


  // Connect to/start up the server and get the list of printers...
  if ((http = _papplMainloopConnect(base_name, true)) == NULL)
    return (1);

  request = ippNewRequest(IPP_OP_GET_PRINTERS);
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "system-uri", NULL, "ipp://localhost/ipp/system");
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name", NULL, cupsUser());

  response = cupsDoRequest(http, request, "/ipp/system");

  for (attr = ippFindAttribute(response, "printer-name", IPP_TAG_NAME); attr; attr = ippFindNextAttribute(response, "printer-name", IPP_TAG_NAME))
    puts(ippGetString(attr, 0, NULL));

  ippDelete(response);
  httpClose(http);

  return (0);
}


//
// '_papplMainloopShowStatus()' - Show system/printer status.
//

int					// O - Exit status
_papplMainloopShowStatus(
    const char    *base_name,		// I - Base name
    int           num_options,		// I - Number of options
    cups_option_t *options)		// I - Options
{
  http_t		*http;		// HTTP connection
  const char		*printer_uri,	// Printer URI
			*printer_name;	// Printer name
  char			resource[1024];	// Resource path
  ipp_t			*request,	// IPP request
			*response;	// IPP response
  int			i,		// Looping var
			count,		// Number of reasons
			state;		// *-state value
  ipp_attribute_t	*state_reasons;	// *-state-reasons attribute
  time_t		state_time;	// *-state-change-time value
  const char		*reason;	// *-state-reasons value
  static const char * const states[] =	// *-state strings
  {
      "idle",
      "processing jobs",
      "stopped"
  };
  static const char * const pattrs[] =
  {					// Requested printer attributes
      "printer-state",
      "printer-state-change-date-time",
      "printer-state-reasons"
  };
  static const char * const sysattrs[] =
  {					// Requested system attributes
      "system-state",
      "system-state-change-date-time",
      "system-state-reasons"
  };


  if ((printer_uri = cupsGetOption("printer-uri", num_options, options)) != NULL)
  {
    // Connect to the remote printer...
    if ((http = _papplMainloopConnectURI(base_name, printer_uri, resource, sizeof(resource))) == NULL)
      return (1);
  }
  else
  {
    // Connect to the server...
    if ((http = _papplMainloopConnect(base_name, false)) == NULL)
    {
      puts("Server is not running.");
      return (0);
    }
  }

  if (printer_uri || (printer_name = cupsGetOption("printer-name", num_options, options)) != NULL)
  {
    // Get the printer's status
    request = ippNewRequest(IPP_OP_GET_PRINTER_ATTRIBUTES);
    if (printer_uri)
      ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri", NULL, printer_uri);
    else
      _papplMainloopAddPrinterURI(request, printer_name, resource, sizeof(resource));
    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name", NULL, cupsUser());
    ippAddStrings(request, IPP_TAG_OPERATION, IPP_TAG_KEYWORD, "requested-attributes", (int)(sizeof(pattrs) / sizeof(pattrs[0])), NULL, pattrs);

    response      = cupsDoRequest(http, request, resource);
    state         = ippGetInteger(ippFindAttribute(response, "printer-state", IPP_TAG_ENUM), 0);
    state_time    = ippDateToTime(ippGetDate(ippFindAttribute(response, "printer-state-change-date-time", IPP_TAG_DATE), 0));
    state_reasons = ippFindAttribute(response, "printer-state-reasons", IPP_TAG_KEYWORD);
  }
  else
  {
    // Get the system status
    request = ippNewRequest(IPP_OP_GET_SYSTEM_ATTRIBUTES);
    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "system-uri", NULL, "ipp://localhost/ipp/system");
    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name", NULL, cupsUser());
    ippAddStrings(request, IPP_TAG_OPERATION, IPP_TAG_KEYWORD, "requested-attributes", (int)(sizeof(sysattrs) / sizeof(sysattrs[0])), NULL, sysattrs);

    response      = cupsDoRequest(http, request, "/ipp/system");
    state         = ippGetInteger(ippFindAttribute(response, "system-state", IPP_TAG_ENUM), 0);
    state_time    = ippDateToTime(ippGetDate(ippFindAttribute(response, "system-state-change-date-time", IPP_TAG_DATE), 0));
    state_reasons = ippFindAttribute(response, "system-state-reasons", IPP_TAG_KEYWORD);
  }

  if (state < IPP_PSTATE_IDLE)
    state = IPP_PSTATE_IDLE;
  else if (state > IPP_PSTATE_STOPPED)
    state = IPP_PSTATE_STOPPED;

  printf("Running, %s since %s\n", states[state - IPP_PSTATE_IDLE], httpGetDateString(state_time));

  if (state_reasons)
  {
    for (i = 0, count = ippGetCount(state_reasons); i < count; i ++)
    {
      reason = ippGetString(state_reasons, i, NULL);
      if (strcmp(reason, "none"))
        puts(reason);
    }
  }

  ippDelete(response);

  return (0);
}


//
// '_papplMainloopShutdownServer()' - Shutdown the server.
//

int					// O - Exit status
_papplMainloopShutdownServer(
    const char    *base_name,		// I - Base name
    int           num_options,		// I - Number of options
    cups_option_t *options)		// I - Options
{
  http_t	*http;			// HTTP connection
  ipp_t		*request;		// IPP request


  // Try connecting to the server...
  if ((http = _papplMainloopConnect(base_name, false)) == NULL)
  {
    fprintf(stderr, "%s: Server is not running.\n", base_name);
    return (1);
  }

  request = ippNewRequest(IPP_OP_SHUTDOWN_ALL_PRINTERS);
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "system-uri", NULL, "ipp://localhost/ipp/system");
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name", NULL, cupsUser());

  ippDelete(cupsDoRequest(http, request, "/ipp/system"));

  if (cupsLastError() != IPP_STATUS_OK)
  {
    fprintf(stderr, "%s: Unable to shutdown server: %s\n", base_name, cupsLastErrorString());
    return (1);
  }

  return (0);
}


//
// '_papplMainloopSubmitJob()' - Submit job(s).
//

int					// O - Exit status
_papplMainloopSubmitJob(
    const char    *base_name,		// I - Base name
    int           num_options,		// I - Number of options
    cups_option_t *options,		// I - Options
    int           num_files,		// I - Number of files
    char          **files)		// I - Files
{
  const char	*document_format,	// Document format
		*document_name,		// Document name
		*filename,		// Current print filename
		*job_name,		// Job name
		*printer_name,		// Printer name
		*printer_uri;		// Printer URI
  http_t	*http;			// Server connection
  ipp_t		*request,		// IPP request
		*response;		// IPP response
  char		default_printer[256],	// Default printer name
		resource[1024],		// Resource path
		tempfile[1024];		// Temporary file
  int		i;			// Looping var
  char		*stdin_file;		// Dummy filename for passive stdin jobs
  ipp_attribute_t *job_id;		// job-id for created job


  // If there are no input files and stdin is not a TTY, treat that as an
  // implicit request to print from stdin...
  if (num_files == 0 && !isatty(0))
  {
    stdin_file = (char *)"-";
    files      = &stdin_file;
    num_files  = 1;
  }

  if (num_files == 0)
  {
    fprintf(stderr, "%s: No files to print.\n", base_name);
    return (1);
  }

  if ((printer_uri = cupsGetOption("printer-uri", num_options, options)) != NULL)
  {
    // Connect to the remote printer...
    if ((http = _papplMainloopConnectURI(base_name, printer_uri, resource, sizeof(resource))) == NULL)
      return (1);
  }
  else
  {
    // Connect to/start up the server and get the destination printer...
    if ((http = _papplMainloopConnect(base_name, true)) == NULL)
      return (1);

    if ((printer_name = cupsGetOption("printer-name", num_options, options)) == NULL)
    {
      if ((printer_name = _papplMainloopGetDefaultPrinter(http, default_printer, sizeof(default_printer))) == NULL)
      {
        fprintf(stderr, "%s: No default printer available.\n", base_name);
        httpClose(http);
        return (1);
      }
    }
  }

  // Loop through the print files
  job_name        = cupsGetOption("job-name", num_options, options);
  document_format = cupsGetOption("document-format", num_options, options);

  for (i = 0; i < num_files; i ++)
  {
    // Get the current print file...
    if (!strcmp(files[i], "-"))
    {
      if (!copy_stdin(base_name, tempfile, sizeof(tempfile)))
      {
        httpClose(http);
        return (1);
      }

      filename      = tempfile;
      document_name = "(stdin)";
    }
    else
    {
      filename = files[i];
      if ((document_name = strrchr(filename, '/')) != NULL)
        document_name ++;
      else
        document_name = filename;
    }

    // Send a Print-Job request...
    request = ippNewRequest(IPP_OP_PRINT_JOB);
    if (printer_uri)
      ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri", NULL, printer_uri);
    else
      _papplMainloopAddPrinterURI(request, printer_name, resource, sizeof(resource));

    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name", NULL, cupsUser());
    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "job-name", NULL, job_name ? job_name : document_name);
    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "document-name", NULL, document_name);

    if (document_format)
      ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_MIMETYPE, "document-format", NULL, document_format);

    _papplMainloopAddOptions(request, num_options, options);

    response = cupsDoFileRequest(http, request, resource, filename);

    if ((job_id = ippFindAttribute(response, "job-id", IPP_TAG_INTEGER)) == NULL)
    {
      fprintf(stderr, "%s: Unable to print '%s': %s\n", base_name, filename, cupsLastErrorString());
      ippDelete(response);
      httpClose(http);
      return (1);
    }

    if (printer_uri)
      printf("%d\n", ippGetInteger(job_id, 0));
    else
      printf("%s-%d\n", printer_name, ippGetInteger(job_id, 0));

    ippDelete(response);

    if (filename == tempfile)
      unlink(tempfile);
  }

  httpClose(http);

  return (0);
}


//
// 'copy_stdin()' - Copy print data from the standard input.
//

char *					// O - Temporary filename or `NULL` on error
copy_stdin(
    const char *base_name,		// I - Printer application name
    char       *name,			// I - Filename buffer
    size_t     namesize)		// I - Size of filename buffer
{
  int		tempfd;			// Temporary file descriptor
  size_t	total = 0;		// Total bytes read/written
  ssize_t	bytes;			// Number of bytes read/written
  char		buffer[65536];		// Copy buffer


  // Create a temporary file for printing...
  if ((tempfd = cupsTempFd(name, (int)namesize)) < 0)
  {
    fprintf(stderr, "%s: Unable to create temporary file: %s\n", base_name, strerror(errno));
    return (NULL);
  }

  // Read from stdin until we see EOF...
  while ((bytes = read(0, buffer, sizeof(buffer))) > 0)
  {
    if (write(tempfd, buffer, (size_t)bytes) < 0)
    {
      fprintf(stderr, "%s: Unable to write to temporary file: %s\n", base_name, strerror(errno));
      goto fail;
    }

    total += (size_t)bytes;
  }

  // Only allow non-empty files...
  if (total == 0)
  {
    fprintf(stderr, "%s: Empty print file received on the standard input.\n", base_name);
    goto fail;
  }

  // Close the temporary file and return it...
  close(tempfd);

  return (name);

  // If we get here, something went wrong...
  fail:

  // Close and remove the temporary file...
  close(tempfd);
  unlink(name);

  // Return NULL and an empty filename...
  *name = '\0';

  return (NULL);
}


//
// 'device_error_cb()' - Show a device error message.
//

static void
device_error_cb(const char *message,	// I - Error message
		void       *data)	// I - Callback data (application name)
{
  printf("%s: %s\n", (char *)data, message);
}


//
// 'device_list_cb()' - List a device.
//

static bool				// O - `true` to stop, `false` to continue
device_list_cb(const char *device_uri,	// I - Device URI
	       const char *device_id,	// I - IEEE-1284 device ID
	       void       *data)	// I - Callback data (NULL for plain, "verbose" for verbose output)
{
  puts(device_uri);

  if (device_id && data)
    printf("    %s\n", device_id);

  return (false);
}


//
// 'get_value()' - Get the string representation of an attribute value.
//

static char *				// O - String value
get_value(ipp_attribute_t *attr,	// I - Attribute
          const char      *name,	// I - Base name of attribute
          int             element,	// I - Value index
          char            *buffer,	// I - String buffer
          size_t          bufsize)	// I - Size of string buffer
{
  const char	*value;			// String value
  int		intvalue;		// Integer value
  int		lower,			// Lower range value
		upper;			// Upper range value
  int		xres,			// X resolution
		yres;			// Y resolution
  ipp_res_t	units;			// Resolution units


  *buffer = '\0';

  switch (ippGetValueTag(attr))
  {
    default :
    case IPP_TAG_KEYWORD :
        if ((value = ippGetString(attr, element, NULL)) != NULL)
        {
          if (!strcmp(name, "media"))
          {
            pwg_media_t	*pwg = pwgMediaForPWG(value);
					// Media size

            if ((pwg->width % 100) == 0)
              snprintf(buffer, bufsize, "%s (%dx%dmm or %.2gx%.2gin)", value, pwg->width / 100, pwg->length / 100, pwg->width / 2540.0, pwg->length / 2540.0);
	    else
              snprintf(buffer, bufsize, "%s (%.2gx%.2gin or %dx%dmm)", value, pwg->width / 2540.0, pwg->length / 2540.0, pwg->width / 100, pwg->length / 100);
          }
          else
          {
            strlcpy(buffer, value, bufsize);
	  }
	}
	break;

    case IPP_TAG_ENUM :
        strlcpy(buffer, ippEnumString(name, ippGetInteger(attr, element)), bufsize);
        break;

    case IPP_TAG_INTEGER :
        intvalue = ippGetInteger(attr, element);

        if (!strcmp(name, "label-tear-offset") || !strcmp(name, "media-top-offset") || !strcmp(name, "print-speed"))
        {
          if ((intvalue % 635) == 0)
            snprintf(buffer, bufsize, "%.2gin", intvalue / 2540.0);
	  else
	    snprintf(buffer, bufsize, "%.2gmm", intvalue * 0.01);
        }
        else
          snprintf(buffer, bufsize, "%d", intvalue);
	break;

    case IPP_TAG_RANGE :
        lower = ippGetRange(attr, element, &upper);

        if (!strcmp(name, "label-tear-offset") || !strcmp(name, "media-top-offset") || !strcmp(name, "print-speed"))
        {
          if ((upper % 635) == 0)
            snprintf(buffer, bufsize, "%.2gin to %.2gin", lower / 2540.0, upper / 2540.0);
	  else
	    snprintf(buffer, bufsize, "%.2gmm to %.2gmm", lower * 0.01, upper * 0.01);
        }
        else
          snprintf(buffer, bufsize, "%d to %d", lower, upper);
	break;

    case IPP_TAG_RESOLUTION :
        xres = ippGetResolution(attr, element, &yres, &units);
        if (xres == yres)
	  snprintf(buffer, bufsize, "%d%s", xres, units == IPP_RES_PER_INCH ? "dpi" : "dpcm");
	else
	  snprintf(buffer, bufsize, "%dx%d%s", xres, yres, units == IPP_RES_PER_INCH ? "dpi" : "dpcm");
	break;
  }

  return (buffer);
}


//
// 'print_option()' - Print the supported and default value for an option.
//

void
print_option(ipp_t      *response,	// I - Get-Printer-Attributes response
	     const char *name)		// I - Attribute name
{
  char		defname[256],		// xxx-default/xxx-configured name
		supname[256];		// xxx-supported name
  ipp_attribute_t *defattr,		// xxx-default/xxx-configured attribute
		*supattr;		// xxx-supported attribute
  int		i,			// Looping var
		count;			// Number of values
  char		defvalue[256],		// xxx-default/xxx-configured value
		supvalue[256];		// xxx-supported value


  // Get the default and supported attributes...
  snprintf(supname, sizeof(supname), "%s-supported", name);
  if ((supattr = ippFindAttribute(response, supname, IPP_TAG_ZERO)) == NULL)
    return;

  if (!strncmp(name, "media-", 6))
    snprintf(defname, sizeof(defname), "media-col-default/%s", name);
  else
    snprintf(defname, sizeof(defname), "%s-default", name);
  if ((defattr = ippFindAttribute(response, defname, IPP_TAG_ZERO)) == NULL)
  {
    snprintf(defname, sizeof(defname), "%s-configured", name);
    defattr = ippFindAttribute(response, defname, IPP_TAG_ZERO);
  }
  get_value(defattr, name, 0, defvalue, sizeof(defvalue));

  // Show the option with its values...
  if (defvalue[0])
    printf("  -o %s=%s (default)\n", name, defvalue);

  for (i = 0, count = ippGetCount(supattr); i < count; i ++)
  {
    if (strcmp(defvalue, get_value(supattr, name, i, supvalue, sizeof(supvalue))))
      printf("  -o %s=%s\n", name, supvalue);
  }
}
