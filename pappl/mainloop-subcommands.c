//
// Standard papplMainloop sub-commands for the Printer Application Framework
//
// Copyright © 2020-2024 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#  include "pappl-private.h"
#  ifdef __APPLE__
#    include <bsm/audit.h>
#    include <bsm/audit_session.h>
#  endif // __APPLE__


//
// Local globals
//

static pthread_mutex_t	mainloop_mutex = PTHREAD_MUTEX_INITIALIZER;
					// Mutex for system object
static pappl_system_t	*mainloop_system = NULL;
					// Current system object


//
// Local functions
//

static char	*copy_stdin(const char *base_name, char *name, size_t namesize);
static pappl_system_t *default_system_cb(const char *base_name, int num_options, cups_option_t *options, void *data);
static ipp_t	*get_printer_attributes(http_t *http, const char *printer_uri, const char *printer_name, const char *resource, cups_len_t num_requested, const char * const *requested);
static char	*get_value(ipp_attribute_t *attr, const char *name, cups_len_t element, char *buffer, size_t bufsize);
static cups_len_t load_options(const char *filename, cups_len_t num_options, cups_option_t **options);
static void	print_option(ipp_t *response, const char *name);
#if _WIN32
static void	save_server_port(const char *base_name, int port);
#endif // _WIN32


//
// '_papplMainloopAddPrinter()' - Add a printer.
//

int					// O - Exit status
_papplMainloopAddPrinter(
    const char    *base_name,		// I - Base name
    cups_len_t    num_options,		// I - Number of options
    cups_option_t *options)		// I - Options
{
  http_t	*http;			// Connection to server
  ipp_t		*request;		// Create-Printer request
  const char	*device_uri,		// Device URI
		*driver_name,		// Name of driver
		*printer_name,		// Name of printer
		*printer_uri;		// Printer URI


  // Get required values...
  device_uri   = cupsGetOption("smi55357-device-uri", num_options, options);
  driver_name  = cupsGetOption("smi55357-driver", num_options, options);
  printer_name = cupsGetOption("printer-name", num_options, options);

  if (!device_uri || !driver_name || !printer_name)
  {
    if (!printer_name)
      _papplLocPrintf(stderr, _PAPPL_LOC("%s: Missing '-d PRINTER'."), base_name);
    if (!driver_name)
      _papplLocPrintf(stderr, _PAPPL_LOC("%s: Missing '-m DRIVER-NAME'."), base_name);
    if (!device_uri)
      _papplLocPrintf(stderr, _PAPPL_LOC("%s: Missing '-v DEVICE-URI'."), base_name);

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
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_KEYWORD, "smi55357-driver", NULL, driver_name);
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "smi55357-device-uri", NULL, device_uri);
  ippAddString(request, IPP_TAG_PRINTER, IPP_TAG_NAME, "printer-name", NULL, printer_name);

  _papplMainloopAddOptions(request, num_options, options, NULL);

  ippDelete(cupsDoRequest(http, request, "/ipp/system"));

  httpClose(http);

  if (cupsGetError() != IPP_STATUS_OK)
  {
    _papplLocPrintf(stderr, _PAPPL_LOC("%s: Unable to add printer: %s"), base_name, cupsGetErrorString());
    return (1);
  }

  return (0);
}



//
// '_papplMainloopAddScanner()' - Add a scanner using eSCL.
//
int
_papplMainloopAddScanner(
    const char    *base_name,      // I - Base name
    cups_len_t    num_options,     // I - Number of options
    cups_option_t *options)        // I - Options
{
    http_t     *http = NULL;       // Connection to server
    const char *device_uri,        // Device URI
               *scanner_name,      // Name of scanner
               *scanner_uri,       // Scanner URI
               *escl_path;         // eSCL resource path
    char       resource[1024];     // Resource path for connection
    bool       status = false;     // Status of scanner addition

    // Get required values...
    device_uri = cupsGetOption("device-uri", (int)num_options, options);
    scanner_name = cupsGetOption("scanner-name", (int)num_options, options);
    escl_path = cupsGetOption("escl", (int)num_options, options);

    // Rest of the implementation remains the same...
    if (!device_uri || !scanner_name)
    {
        if (!scanner_name)
            _papplLocPrintf(stderr, _PAPPL_LOC("%s: Missing '-d SCANNER'."), base_name);
        if (!device_uri)
            _papplLocPrintf(stderr, _PAPPL_LOC("%s: Missing '-v DEVICE-URI'."), base_name);
        return (1);
    }

    if ((scanner_uri = cupsGetOption("scanner-uri", (int)num_options, options)) != NULL)
    {
        if ((http = _papplMainloopConnectURI(base_name, scanner_uri, resource,
                                          sizeof(resource))) == NULL)
            return (1);
    }

    // Set up eSCL connection and registration
    if (!escl_path)
        escl_path = "/eSCL/";  // Default eSCL path if not specified

    // Create scanner registration request
    char *post_data = NULL;
    size_t post_size = 0;
    FILE *post_file = open_memstream(&post_data, &post_size);

    if (post_file)
    {
        // Format eSCL scanner registration XML
        fprintf(post_file, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
        fprintf(post_file, "<scan:ScannerRegistration xmlns:scan=\"http://schemas.hp.com/imaging/escl/2011/05/03\">\n");
        fprintf(post_file, "  <scan:ScannerName>%s</scan:ScannerName>\n", scanner_name);
        fprintf(post_file, "  <scan:DeviceURI>%s</scan:DeviceURI>\n", device_uri);
        fprintf(post_file, "</scan:ScannerRegistration>\n");
        fclose(post_file);

        // Set up HTTP POST request
        httpClearFields(http);
        httpSetField(http, HTTP_FIELD_CONTENT_TYPE, "application/xml");
        httpSetLength(http, post_size);

        // Send the registration request
        if (httpPost(http, escl_path) == HTTP_STATUS_OK)
        {
            http_status_t response = httpUpdate(http);
            if (response == HTTP_STATUS_OK || response == HTTP_STATUS_CREATED)
                status = true;
        }

        free(post_data);
    }

    httpClose(http);

    if (!status)
    {
        _papplLocPrintf(stderr, _PAPPL_LOC("%s: Unable to add scanner: %s"),
                      base_name, cupsGetErrorString());
        return (1);
    }

    return (0);
}



//
// '_papplMainloopAutoAddPrinters()' - Automatically add printers.
//

int					// O - Exit status
_papplMainloopAutoAddPrinters(
    const char    *base_name,		// I - Basename of application
    cups_len_t    num_options,		// I - Number of options
    cups_option_t *options)		// I - Options
{
  http_t	*http;			// Connection to server
  ipp_t		*request;		// IPP request


  (void)num_options;
  (void)options;

  // Try connecting to server...
  if ((http = _papplMainloopConnect(base_name, true)) == NULL)
    return (1);

  // Send a PAPPL-Create-Printers request...
  request = ippNewRequest(IPP_OP_PAPPL_CREATE_PRINTERS);

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "system-uri", NULL, "ipp://localhost/ipp/system");
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name", NULL, cupsGetUser());

  ippDelete(cupsDoRequest(http, request, "/ipp/system"));
  httpClose(http);

  return (0);
}


//
// '_papplMainloopCancelJob()' - Cancel job(s).
//

int					// O - Exit status
_papplMainloopCancelJob(
    const char    *base_name,		// I - Base name
    cups_len_t    num_options,		// I - Number of options
    cups_option_t *options)		// I - Options
{
  const char	*printer_uri,		// Printer URI
		*printer_name = NULL;	// Printer name
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
        _papplLocPrintf(stderr, _PAPPL_LOC("%s: No default printer available."), base_name);
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
    char *end;				// End of value

    request = ippNewRequest(IPP_OP_CANCEL_JOB);
    job_id  = (int)strtol(value, &end, 10);

    if (job_id < 1 || errno == ERANGE || *end)
    {
      _papplLocPrintf(stderr, _PAPPL_LOC("%s: Bad job ID."), base_name);
      httpClose(http);
      return (1);
    }
  }
  else
    request = ippNewRequest(IPP_OP_CANCEL_CURRENT_JOB);

  if (printer_uri)
    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri", NULL, printer_uri);
  else
    _papplMainloopAddPrinterURI(request, printer_name, resource, sizeof(resource));

  if (job_id)
    ippAddInteger(request, IPP_TAG_OPERATION, IPP_TAG_INTEGER, "job-id", job_id);
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name", NULL, cupsGetUser());

  ippDelete(cupsDoRequest(http, request, resource));
  httpClose(http);

  if (cupsGetError() != IPP_STATUS_OK)
  {
    _papplLocPrintf(stderr, _PAPPL_LOC("%s: Unable to cancel job: %s"), base_name, cupsGetErrorString());
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
    cups_len_t    num_options,		// I - Number of options
    cups_option_t *options)		// I - Options
{
  const char	*printer_uri,		// Printer URI
		*printer_name;		// Printer name
  http_t	*http;			// Server connection
  ipp_t		*request,		// IPP request
		*response;		// IPP response
  char		resource[1024];		// Resource path
  int		printer_id;		// printer-id value
  static const char *pattrs = "printer-id";
					// Requested attributes


  // Connect to/start up the server and get the destination printer...
  if ((printer_uri = cupsGetOption("printer-uri", num_options, options)) != NULL)
  {
    // Connect to the remote printer...
    if ((http = _papplMainloopConnectURI(base_name, printer_uri, resource, sizeof(resource))) == NULL)
      return (1);

    printer_name = NULL;
  }
  else if ((http = _papplMainloopConnect(base_name, true)) == NULL)
  {
    return (1);
  }
  else if ((printer_name = cupsGetOption("printer-name", num_options, options)) == NULL)
  {
    _papplLocPrintf(stderr, _PAPPL_LOC("%s: Missing '-d PRINTER'."), base_name);
    httpClose(http);
    return (1);
  }

  // Get the printer-id for the printer we are deleting...
  response   = get_printer_attributes(http, printer_uri, printer_name, resource, 1, &pattrs);
  printer_id = ippGetInteger(ippFindAttribute(response, "printer-id", IPP_TAG_INTEGER), 0);
  ippDelete(response);

  if (printer_id == 0)
  {
    _papplLocPrintf(stderr, _PAPPL_LOC("%s: Unable to get information for printer: %s"), base_name, cupsGetErrorString());
    httpClose(http);
    return (1);
  }

  // Now that we have the printer-id, delete it from the system service...
  request = ippNewRequest(IPP_OP_DELETE_PRINTER);
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "system-uri", NULL, "ipp://localhost/ipp/system");
  ippAddInteger(request, IPP_TAG_OPERATION, IPP_TAG_INTEGER, "printer-id", printer_id);
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name", NULL, cupsGetUser());

  ippDelete(cupsDoRequest(http, request, "/ipp/system"));
  httpClose(http);

  if (cupsGetError() != IPP_STATUS_OK)
  {
    _papplLocPrintf(stderr, _PAPPL_LOC("%s: Unable to delete printer: %s"), base_name, cupsGetErrorString());
    return (1);
  }

  return (0);
}

//
// '_papplMainloopDeleteScanner()' - Delete a scanner registration.
//

int                                     // O - Exit status
_papplMainloopDeleteScanner(
    const char    *base_name,           // I - Base name
    cups_len_t    num_options,          // I - Number of options
    cups_option_t *options)             // I - Options
{
    http_t       *http = NULL;          // Connection to server
    const char   *device_uri,           // Device URI
                 *scanner_name,         // Name of scanner
                 *scanner_uri,          // Scanner URI
                 *escl_path;            // eSCL resource path
    char         resource[1024];        // Resource path for connection
    bool         status = false;        // Status of scanner deletion
    http_status_t response;             // HTTP response status

    // Get required values...
    device_uri = cupsGetOption("device-uri", (int)num_options, options);
    scanner_name = cupsGetOption("scanner-name", (int)num_options, options);
    escl_path = cupsGetOption("escl", (int)num_options, options);

    // Check if we're deleting a remote scanner
    scanner_uri = cupsGetOption("scanner-uri", (int)num_options, options);
    if (scanner_uri)
    {
        // Connect to the remote scanner...
        http = _papplMainloopConnectURI(base_name, scanner_uri, resource, sizeof(resource));
        if (!http)
        {
            _papplLocPrintf(stderr, _PAPPL_LOC("%s: Unable to connect to remote scanner at '%s'"),
                           base_name, scanner_uri);
            return (1);
        }
    }
    else
    {
        // Validate required parameters for local scanner
        if (!scanner_name)
        {
            _papplLocPrintf(stderr, _PAPPL_LOC("%s: Missing '-d SCANNER'."), base_name);
            return (1);
        }

        if (!device_uri)
        {
            _papplLocPrintf(stderr, _PAPPL_LOC("%s: Missing '-v DEVICE-URI'."), base_name);
            return (1);
        }

        // Connect to local scanner
        http = httpConnect2(device_uri, 0, NULL, AF_UNSPEC, HTTP_ENCRYPTION_IF_REQUESTED, 1, 30000, NULL);
        if (!http)
        {
            _papplLocPrintf(stderr, _PAPPL_LOC("%s: Unable to connect to scanner at '%s'"),
                           base_name, device_uri);
            return (1);
        }
    }

    // Set up eSCL path
    if (!escl_path)
        escl_path = "/eSCL/";  // Default eSCL path if not specified

    // Construct the deletion path
    char delete_path[1024];
    snprintf(delete_path, sizeof(delete_path), "%sregistration/%s",
             escl_path, scanner_name);

    // Send DELETE request to remove scanner registration
    httpClearFields(http);

    if (httpDelete(http, delete_path) != HTTP_STATUS_OK)
    {
        _papplLocPrintf(stderr, _PAPPL_LOC("%s: Unable to send deletion request: %s"),
                       base_name, cupsGetErrorString());
        httpClose(http);
        return (1);
    }

    // Check the response
    response = httpUpdate(http);
    if (response == HTTP_STATUS_OK || response == HTTP_STATUS_NO_CONTENT)
    {
        status = true;
        _papplLocPrintf(stderr, _PAPPL_LOC("%s: Successfully deleted scanner '%s'"),
                       base_name, scanner_name);
    }
    else
    {
        _papplLocPrintf(stderr, _PAPPL_LOC("%s: Scanner deletion failed with status %d"),
                       base_name, response);
    }

    // Clean up
    httpClose(http);

    return (status ? 0 : 1);
}

//
// '_papplMainloopGetSetDefaultPrinter()' - Get/set the default printer.
//

int					// O - Exit status
_papplMainloopGetSetDefaultPrinter(
    const char    *base_name,		// I - Base name
    cups_len_t    num_options,		// I - Number of options
    cups_option_t *options)		// I - Options
{
  const char	*printer_uri,		// Printer URI
		*printer_name;		// Printer name
  http_t	*http;			// Server connection
  ipp_t		*request,		// IPP request
		*response;		// IPP response
  char		resource[1024];		// Resource path
  int		printer_id;		// printer-id value
  static const char *pattrs = "printer-id";
					// Requested attributes


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
      _papplLocPrintf(stdout, _PAPPL_LOC("No default printer set."));

    httpClose(http);

    return (0);
  }

  // OK, setting the default printer so get the printer-id for it...
  response   = get_printer_attributes(http, printer_uri, printer_name, resource, 1, &pattrs);
  printer_id = ippGetInteger(ippFindAttribute(response, "printer-id", IPP_TAG_INTEGER), 0);
  ippDelete(response);

  if (printer_id == 0)
  {
    _papplLocPrintf(stderr, _PAPPL_LOC("%s: Unable to get information for '%s': %s"), base_name, printer_name, cupsGetErrorString());
    httpClose(http);
    return (1);
  }

  // Now that we have the printer-id, set the system-default-printer-id
  // attribute for the system service...
  request = ippNewRequest(IPP_OP_SET_SYSTEM_ATTRIBUTES);
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "system-uri", NULL, "ipp://localhost/ipp/system");
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name", NULL, cupsGetUser());
  ippAddInteger(request, IPP_TAG_SYSTEM, IPP_TAG_INTEGER, "system-default-printer-id", printer_id);

  ippDelete(cupsDoRequest(http, request, "/ipp/system"));
  httpClose(http);

  if (cupsGetError() != IPP_STATUS_OK)
  {
    _papplLocPrintf(stderr, _PAPPL_LOC("%s: Unable to set default printer: %s"), base_name, cupsGetErrorString());
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
    cups_len_t    num_options,		// I - Number of options
    cups_option_t *options)		// I - Options
{
  http_t	*http;			// Connection to server
  ipp_t		*request,		// Set-Printer-Attributes request
		*supported;		// Supported attributes
  const char	*printer_uri,		// Printer URI
		*printer_name;		// Name of printer
  char		resource[1024];		// Resource path


  // Open a connection to the server...
  if ((printer_uri = cupsGetOption("printer-uri", num_options, options)) != NULL)
  {
    // Connect to the remote printer...
    if ((http = _papplMainloopConnectURI(base_name, printer_uri, resource, sizeof(resource))) == NULL)
      return (1);

    printer_name = NULL;
  }
  else if ((http = _papplMainloopConnect(base_name, true)) == NULL)
  {
    return (1);
  }
  else if ((printer_name  = cupsGetOption("printer-name", num_options, options)) == NULL)
  {
    _papplLocPrintf(stderr, _PAPPL_LOC("%s: Missing '-d PRINTER'."), base_name);
    return (1);
  }

  // Get the supported attributes...
  supported = get_printer_attributes(http, printer_uri, printer_name, resource, 0, NULL);

  // Send a Set-Printer-Attributes request to the server...
  request = ippNewRequest(IPP_OP_SET_PRINTER_ATTRIBUTES);
  if (printer_uri)
    ippAddString(request, IPP_TAG_PRINTER, IPP_TAG_URI, "printer-uri", NULL, printer_uri);
  else
    _papplMainloopAddPrinterURI(request, printer_name, resource, sizeof(resource));
  _papplMainloopAddOptions(request, num_options, options, supported);
  ippDelete(supported);

  ippDelete(cupsDoRequest(http, request, resource));

  httpClose(http);

  if (cupsGetError() != IPP_STATUS_OK)
  {
    _papplLocPrintf(stderr, _PAPPL_LOC("%s: Unable to modify printer: %s"), base_name, cupsGetErrorString());
    return (1);
  }

  return (0);
}


//
// '_papplMainloopPausePrinter()' - Pause printer.
//

int					// O - Exit status
_papplMainloopPausePrinter(
    const char    *base_name,		// I - Base name
    cups_len_t    num_options,		// I - Number of options
    cups_option_t *options)		// I - Options
{
  http_t	*http;			// Connection to server
  ipp_t		*request;		// Pause-Printer request
  const char	*printer_uri,		// Printer URI
		*printer_name;		// Name of printer
  char		resource[1024];		// Resource path


  // Open a connection to the server...
  if ((printer_uri = cupsGetOption("printer-uri", num_options, options)) != NULL)
  {
    // Connect to the remote printer...
    if ((http = _papplMainloopConnectURI(base_name, printer_uri, resource, sizeof(resource))) == NULL)
      return (1);

    printer_name = NULL;
  }
  else if ((http = _papplMainloopConnect(base_name, true)) == NULL)
  {
    return (1);
  }
  else if ((printer_name  = cupsGetOption("printer-name", num_options, options)) == NULL)
  {
    _papplLocPrintf(stderr, _PAPPL_LOC("%s: Missing '-d PRINTER'."), base_name);
    return (1);
  }

  // Send a Pause-Printer request to the server...
  request = ippNewRequest(IPP_OP_PAUSE_PRINTER);
  if (printer_uri)
    ippAddString(request, IPP_TAG_PRINTER, IPP_TAG_URI, "printer-uri", NULL, printer_uri);
  else
    _papplMainloopAddPrinterURI(request, printer_name, resource, sizeof(resource));

  ippDelete(cupsDoRequest(http, request, resource));

  httpClose(http);

  if (cupsGetError() != IPP_STATUS_OK)
  {
    _papplLocPrintf(stderr, _PAPPL_LOC("%s: Unable to pause printer: %s"), base_name, cupsGetErrorString());
    return (1);
  }

  return (0);
}


//
// '_papplMainloopResumePrinter()' - Resume printer.
//

int					// O - Exit status
_papplMainloopResumePrinter(
    const char    *base_name,		// I - Base name
    cups_len_t    num_options,		// I - Number of options
    cups_option_t *options)		// I - Options
{
  http_t	*http;			// Connection to server
  ipp_t		*request;		// Pause-Printer request
  const char	*printer_uri,		// Printer URI
		*printer_name;		// Name of printer
  char		resource[1024];		// Resource path


  // Open a connection to the server...
  if ((printer_uri = cupsGetOption("printer-uri", num_options, options)) != NULL)
  {
    // Connect to the remote printer...
    if ((http = _papplMainloopConnectURI(base_name, printer_uri, resource, sizeof(resource))) == NULL)
      return (1);

    printer_name = NULL;
  }
  else if ((http = _papplMainloopConnect(base_name, true)) == NULL)
  {
    return (1);
  }
  else if ((printer_name  = cupsGetOption("printer-name", num_options, options)) == NULL)
  {
    _papplLocPrintf(stderr, _PAPPL_LOC("%s: Missing '-d PRINTER'."), base_name);
    return (1);
  }

  // Send a Resume-Printer request to the server...
  request = ippNewRequest(IPP_OP_RESUME_PRINTER);
  if (printer_uri)
    ippAddString(request, IPP_TAG_PRINTER, IPP_TAG_URI, "printer-uri", NULL, printer_uri);
  else
    _papplMainloopAddPrinterURI(request, printer_name, resource, sizeof(resource));

  ippDelete(cupsDoRequest(http, request, resource));

  httpClose(http);

  if (cupsGetError() != IPP_STATUS_OK)
  {
    _papplLocPrintf(stderr, _PAPPL_LOC("%s: Unable to resume printer: %s"), base_name, cupsGetErrorString());
    return (1);
  }

  return (0);
}


//
// '_papplMainloopRunServer()' - Run server.
//

int					// O - Exit status
_papplMainloopRunServer(
    const char            *base_name,	// I - Base name
    const char            *version,	// I - Version number
    const char            *footer_html,	// I - Footer HTML or `NULL` for none
    cups_len_t            num_drivers,	// I - Number of drivers
    pappl_pr_driver_t     *drivers,	// I - Drivers
    pappl_pr_autoadd_cb_t autoadd_cb,	// I - Auto-add callback
    pappl_pr_driver_cb_t  driver_cb,	// I - Driver callback
    cups_len_t            num_options,	// I - Number of options
    cups_option_t         **options,	// I - Options
    pappl_ml_system_cb_t  system_cb,	// I - System callback
    void                  *data)	// I - Callback data
{
  pappl_system_t	*system;	// System object
  char			filename[1023];	// Config filename
#if !_WIN32
  char			sockname[1024];	// Socket filename
#endif // !_WIN32
  char			statename[1024];// State filename
#if _WIN32
  const char		*home = getenv("USERPROFILE");
					// Home directory
#else
  const char		*home = getenv("HOME");
					// Home directory
#endif // _WIN32
  const char		*snap_common = getenv("SNAP_COMMON");
					// Common data directory for snaps
  const char		*tmpdir = papplGetTempDir();
					// Temporary directory
  const char		*xdg_config_home = getenv("XDG_CONFIG_HOME");
					// Freedesktop per-user config directory
#ifdef __APPLE__
  pthread_t		tid;		// Thread ID
#endif // __APPLE__


  // Load additional options from config files...
  if (xdg_config_home)
  {
    snprintf(filename, sizeof(filename), "%s/%s.conf", xdg_config_home, base_name);
    num_options = load_options(filename, num_options, options);
  }
  else if (home)
  {
#ifdef __APPLE__
    snprintf(filename, sizeof(filename), "%s/Library/Application Support/%s.conf", home, base_name);
#elif _WIN32
    snprintf(filename, sizeof(filename), "%s/AppData/Local/%s.conf", home, base_name);
#else
    snprintf(filename, sizeof(filename), "%s/.config/%s.conf", home, base_name);
#endif // __APPLE__

    num_options = load_options(filename, num_options, options);
  }

  if (snap_common)
  {
    snprintf(filename, sizeof(filename), "%s/%s.conf", snap_common, base_name);
    num_options = load_options(filename, num_options, options);
  }
  else
  {
#ifdef __APPLE__
    snprintf(filename, sizeof(filename), "/Library/Application Support/%s.conf", base_name);

#else
    snprintf(filename, sizeof(filename), "/usr/local/etc/%s.conf", base_name);
    num_options = load_options(filename, num_options, options);

    snprintf(filename, sizeof(filename), "/etc/%s.conf", base_name);
#endif // __APPLE__

    num_options = load_options(filename, num_options, options);
  }

  // Create the system object...
  if (system_cb)
  {
    // Developer-supplied system object...
    system = (system_cb)((int)num_options, *options, data);
  }
  else
  {
    // Use the default system object...
    system = default_system_cb(base_name, (int)num_options, *options, data);
  }

  if (!system)
  {
    _papplLocPrintf(stderr, _PAPPL_LOC("%s: Failed to create a system."), base_name);
    return (1);
  }

  // Set the version number as needed...
  if (system->num_versions == 0 && version)
  {
    pappl_version_t	sysversion;	// System version

    memset(&sysversion, 0, sizeof(sysversion));
    papplCopyString(sysversion.name, base_name, sizeof(sysversion.name));
    papplCopyString(sysversion.sversion, version, sizeof(sysversion.sversion));
    sscanf(version, "%hu.%hu.%hu.%hu", sysversion.version + 0, sysversion.version + 1, sysversion.version + 2, sysversion.version + 3);
    papplSystemSetVersions(system, 1, &sysversion);
  }

  // Set the footer HTML as needed...
  if (!system->footer_html && footer_html)
    papplSystemSetFooterHTML(system, footer_html);

  // Set the driver info as needed...
  if (system->num_drivers == 0 && num_drivers > 0 && drivers && driver_cb)
    papplSystemSetPrinterDrivers(system, (int)num_drivers, drivers, autoadd_cb, /* create_cb */NULL, driver_cb, data);

#if _WIN32
  // Save the TCP/IP socket for the server in the registry so other processes
  // can find us...
  save_server_port(base_name, papplSystemGetHostPort(system));

#else
  // Listen for local (domain socket) connections so other processes can find
  // us...
  papplSystemAddListeners(system, _papplMainloopGetServerPath(base_name, getuid(), sockname, sizeof(sockname)));
#endif // _WIN32

  // Finish initialization...
  if (!system->save_cb)
  {
    // Register a callback for saving state information, then load any
    // previous state...
    statename[0] = '\0';

    if (snap_common)
    {
      // Running inside a snap (https://snapcraft.io), so use the snap's common
      // data directory...
      if (!access(snap_common, X_OK))
        snprintf(statename, sizeof(statename), "%s/%s.state", snap_common, base_name);
    }
#if !_WIN32
    else if (!getuid())
    {
      // Running as root, so put the state file in the local state directory
      snprintf(statename, sizeof(statename), PAPPL_STATEDIR "/lib/%s.state", base_name);

      // Make sure base directory exists
      if (mkdir(PAPPL_STATEDIR "/lib", 0777) && errno != EEXIST)
	statename[0] = '\0';
    }
#endif // !_WIN32
    else if (xdg_config_home)
    {
      // Use Freedesktop per-user config directory
      if (!access(xdg_config_home, X_OK))
	snprintf(statename, sizeof(statename), "%s/%s.state", xdg_config_home, base_name);
    }
    else if (home)
    {
#ifdef __APPLE__
      // Put the state in "~/Library/Application Support"
      snprintf(statename, sizeof(statename), "%s/Library/Application Support/%s.state", home, base_name);

#elif _WIN32
      // Put the state in "~/AppData/Local"
      snprintf(statename, sizeof(statename), "%s/AppData/Local/%s.state", home, base_name);

#else
      // Put the state under a ".config" directory in the home directory
      snprintf(statename, sizeof(statename), "%s/.config", home);

      // Make ~/.config as needed
      if (mkdir(statename, 0777) && errno != EEXIST)
	statename[0] = '\0';

      if (statename[0])
	snprintf(statename, sizeof(statename), "%s/.config/%s.state", home, base_name);
#endif // __APPL__
    }

    if (!statename[0])
    {
      // As a last resort, put the state in the temporary directory (where it
      // will be lost on the nest reboot/logout...
#if _WIN32
      snprintf(statename, sizeof(statename), "%s/%s.state", tmpdir, base_name);
#else
      snprintf(statename, sizeof(statename), "%s/%s%d.state", tmpdir, base_name, (int)getuid());
#endif // _WIN32
    }

    papplSystemSetSaveCallback(system, (pappl_save_cb_t)papplSystemSaveState, (void *)statename);

    if (!papplSystemLoadState(system, statename) && autoadd_cb)
    {
      // If there is no state file, auto-add locally-connected printers...
      papplSystemCreatePrinters(system, PAPPL_DEVTYPE_LOCAL, /*cb*/NULL, /*cb_data*/NULL);
    }
  }

  // Set the mainloop system object in case it is needed.
  pthread_mutex_lock(&mainloop_mutex);
  mainloop_system = system;
  pthread_mutex_unlock(&mainloop_mutex);

  // Run the system until shutdown...
#ifdef __APPLE__ // TODO: Implement private/public API for running with UI
  auditinfo_addr_t	ainfo;		// Information about this process

  if (!getaudit_addr(&ainfo, sizeof(ainfo)) && (ainfo.ai_flags & AU_SESSION_FLAG_HAS_GRAPHIC_ACCESS))
  {
    // Show menubar extra when running in an ordinary login session.  Since
    // macOS requires UI code to run on the main thread, run the system object
    // in a background thread...
    if (pthread_create(&tid, NULL, (void *(*)(void *))papplSystemRun, system))
    {
      papplLog(system, PAPPL_LOGLEVEL_ERROR, "Unable to create system thread: %s", strerror(errno));
    }
    else
    {
      // Then run the UI stuff on the main thread (macOS limitation...)
      while (!papplSystemIsRunning(system))
	sleep(1);

      _papplSystemStatusUI(system);

      while (papplSystemIsRunning(system))
	sleep(1);
    }
  }
  else
    // Don't do UI thread...
#endif // __APPLE__

  // Run the system on the main thread...
  papplSystemRun(system);

#if _WIN32
  save_server_port(base_name, 0);	// Clear the Windows registry
#endif // _WIN32

  // Clear the mainloop system object.
  pthread_mutex_lock(&mainloop_mutex);
  mainloop_system = NULL;
  pthread_mutex_unlock(&mainloop_mutex);

  // Delete the system and return...
  papplSystemDelete(system);

  return (0);
}


//
// '_papplMainloopShowDevices()' - Show available devices.
//

int					// O - Exit status
_papplMainloopShowDevices(
    const char    *base_name,		// I - Basename of application
    cups_len_t    num_options,		// I - Number of options
    cups_option_t *options)		// I - Options
{
  http_t	*http;			// Server connection
  ipp_t		*request,		// IPP request
		*response;		// IPP response
  ipp_attribute_t *attr;		// IPP attribute


  // Connect to/start up the server and get the destination printer...
  if ((http = _papplMainloopConnect(base_name, true)) == NULL)
    return (1);

  request = ippNewRequest(IPP_OP_PAPPL_FIND_DEVICES);
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "system-uri", NULL, "ipp://localhost/ipp/system");

  response = cupsDoRequest(http, request, "/ipp/system");
  httpClose(http);

  if (cupsGetError() != IPP_STATUS_OK && cupsGetError() != IPP_STATUS_ERROR_NOT_FOUND)
  {
    _papplLocPrintf(stderr, _PAPPL_LOC("%s: Unable to get available devices: %s"), base_name, cupsGetErrorString());
    ippDelete(response);
    return (1);
  }

  if ((attr = ippFindAttribute(response, "smi55357-device-col", IPP_TAG_BEGIN_COLLECTION)) != NULL)
  {
    cups_len_t	i,			// Looping var
		num_devices = ippGetCount(attr);
					// Number of device entries

    for (i = 0; i < num_devices; i ++)
    {
      ipp_t		*item = ippGetCollection(attr, i);
					// Device entry
      ipp_attribute_t	*item_attr;	// Device attr

      if ((item_attr = ippFindAttribute(item, "smi55357-device-uri", IPP_TAG_ZERO)) != NULL)
      {
	puts(ippGetString(item_attr, 0, NULL));

	if (cupsGetOption("verbose", num_options, options))
	{
	  if ((item_attr = ippFindAttribute(item, "smi55357-device-info", IPP_TAG_ZERO)) != NULL)
	    printf("    %s\n", ippGetString(item_attr, 0, NULL));
	  if ((item_attr = ippFindAttribute(item, "smi55357-device-id", IPP_TAG_ZERO)) != NULL)
	    printf("    %s\n", ippGetString(item_attr, 0, NULL));
	}
      }
    }
  }

  ippDelete(response);

  return (0);
}


//
// '_papplMainloopShowDrivers()' - Show available drivers.
//

int					// O - Exit status
_papplMainloopShowDrivers(
    const char            *base_name,	// I - Basename of application
    cups_len_t            num_drivers,	// I - Number of drivers
    pappl_pr_driver_t     *drivers,	// I - Drivers
    pappl_pr_autoadd_cb_t autoadd_cb,	// I - Auto-add callback
    pappl_pr_driver_cb_t  driver_cb,	// I - Driver callback
    cups_len_t            num_options,	// I - Number of options
    cups_option_t         *options,	// I - Options
    pappl_ml_system_cb_t  system_cb,	// I - System callback
    void                  *data)	// I - Callback data
{
  cups_len_t		i;		// Looping variable
  pappl_system_t	*system;	// System object
  const char           *driver_name;	// I - Driver name
  const char           *device_id;	// I - IEEE-1284 device ID

  if (system_cb)
    system = (system_cb)((int)num_options, options, data);
  else
    system = default_system_cb(base_name, (int)num_options, options, data);

  if (!system)
  {
    _papplLocPrintf(stderr, _PAPPL_LOC("%s: Failed to create a system."), base_name);
    return (1);
  }

  // Set the driver info as needed...
  if (system->num_drivers == 0 && num_drivers > 0 && drivers && driver_cb)
    papplSystemSetPrinterDrivers(system, (int)num_drivers, drivers, autoadd_cb, /* create_cb */NULL, driver_cb, data);

  if ((device_id = cupsGetOption("device-id", num_options, options)) != NULL)
  {
    if ((driver_name = (system->autoadd_cb)(NULL, NULL, device_id, data)) == NULL)
      goto cleanup;
  }
  else
    driver_name = NULL;

  for (i = 0; i < system->num_drivers; i ++)
  {
    if (!driver_name || !strcmp(driver_name, system->drivers[i].name))
      printf("%s \"%s\" \"%s\"\n", system->drivers[i].name, system->drivers[i].description, system->drivers[i].device_id ? system->drivers[i].device_id : "");
  }

  cleanup:

  papplSystemDelete(system);

  return (0);
}


//
// '_papplMainloopShowJobs()' - Show pending printer jobs.
//

int					// O - Exit status
_papplMainloopShowJobs(
    const char    *base_name,		// I - Base name
    cups_len_t    num_options,		// I - Number of options
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
  static const char * const jattrs[] =	// Requested attributes
  {
    "job-id",
    "job-name",
    "job-originating-user-name",
    "job-state"
  };


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
        _papplLocPrintf(stderr, _PAPPL_LOC("%s: No default printer available."), base_name);
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
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name", NULL, cupsGetUser());
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_KEYWORD, "which-jobs", NULL, "all");
  ippAddStrings(request, IPP_TAG_OPERATION, IPP_TAG_KEYWORD, "requested-attributes", (int)(sizeof(jattrs) / sizeof(jattrs[0])), NULL, jattrs);

  response = cupsDoRequest(http, request, resource);

  for (attr = ippGetFirstAttribute(response); attr; attr = ippGetNextAttribute(response))
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

      attr = ippGetNextAttribute(response);
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
    cups_len_t    num_options,		// I - Number of options
    cups_option_t *options)		// I - Options
{
  cups_len_t	i, j,			// Looping vars
		count;			// Number of values
  const char	*printer_uri,		// Printer URI
		*printer_name;		// Printer name
  char		default_printer[256];	// Default printer name
  http_t	*http;			// Server connection
  ipp_t		*response;		// IPP response
  ipp_attribute_t *job_attrs;		// "job-creation-attributes-supported"
  char		resource[1024];		// Resource path
  static const char * const standard_options[] =
  {					// Standard options
    "copies",
    "document-format",
    "document-name",
    "ipp-attribute-fidelity",
    "job-hold-until",
    "job-hold-until-time",
    "job-name",
    "job-priority",
    "job-retain-until",
    "job-retain-until-interval",
    "job-retain-until-time",
    "media",
    "media-col",
    "multiple-document-handling",
    "orientation-requested",
    "output-bin",
    "page-ranges",
    "print-color-mode",
    "print-content-optimize",
    "print-darkness",
    "print-quality",
    "print-speed",
    "printer-resolution",
    "sides"
  };


  if ((printer_uri = cupsGetOption("printer-uri", num_options, options)) != NULL)
  {
    // Connect to the remote printer...
    if ((http = _papplMainloopConnectURI(base_name, printer_uri, resource, sizeof(resource))) == NULL)
      return (1);

    printer_name = NULL;
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
        _papplLocPrintf(stderr, _PAPPL_LOC("%s: No default printer available."), base_name);
        httpClose(http);
        return (1);
      }
    }
  }

  // Get the xxx-supported and xxx-default attributes
  response = get_printer_attributes(http, printer_uri, printer_name, resource, 0, NULL);

  if (cupsGetError() != IPP_STATUS_OK)
  {
    _papplLocPrintf(stderr, _PAPPL_LOC("%s: Unable to get printer options: %s"), base_name, cupsGetErrorString());
    ippDelete(response);
    httpClose(http);
    return (1);
  }

  _papplLocPrintf(stdout, _PAPPL_LOC("Print job options:"));
  _papplLocPrintf(stdout, _PAPPL_LOC("  -c COPIES"));
  print_option(response, "media");
  print_option(response, "media-source");
  print_option(response, "media-top-offset");
  print_option(response, "media-tracking");
  print_option(response, "media-type");
  print_option(response, "orientation-requested");
  print_option(response, "print-color-mode");
  print_option(response, "print-content-optimize");
  if (ippFindAttribute(response, "print-darkness-supported", IPP_TAG_ZERO))
    _papplLocPrintf(stdout, _PAPPL_LOC("  -o print-darkness=-100 to 100"));
  print_option(response, "print-quality");
  print_option(response, "print-speed");
  print_option(response, "printer-resolution");

  // Show vendor extension options...
  if ((job_attrs = ippFindAttribute(response, "job-creation-attributes-supported", IPP_TAG_KEYWORD)) != NULL)
  {
    for (i = 0, count = ippGetCount(job_attrs); i < count; i ++)
    {
      const char *name = ippGetString(job_attrs, i, NULL);
					// Attribute name

      for (j = 0; j < (cups_len_t)(sizeof(standard_options) / sizeof(standard_options[0])); j ++)
      {
        if (!strcmp(name, standard_options[j]))
          break;
      }

      if (j >= (cups_len_t)(sizeof(standard_options) / sizeof(standard_options[0])))
      {
        // Vendor option...
        print_option(response, name);
      }
    }
  }

  // Show printer settings...
  puts("");
  _papplLocPrintf(stdout, _PAPPL_LOC("Printer options:"));
  print_option(response, "label-mode");
  print_option(response, "label-tear-offset");
  if (ippFindAttribute(response, "printer-darkness-supported", IPP_TAG_ZERO))
    _papplLocPrintf(stdout, _PAPPL_LOC("  -o printer-darkness=0 to 100"));
  _papplLocPrintf(stdout, _PAPPL_LOC("  -o printer-geo-location='geo:LATITUDE,LONGITUDE'"));
  _papplLocPrintf(stdout, _PAPPL_LOC("  -o printer-location='LOCATION'"));
  _papplLocPrintf(stdout, _PAPPL_LOC("  -o printer-organization='ORGANIZATION'"));
  _papplLocPrintf(stdout, _PAPPL_LOC("  -o printer-organizational-unit='UNIT/SECTION'"));

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
    cups_len_t    num_options,		// I - Number of options
    cups_option_t *options)		// I - Options
{
  http_t	*http;			// Server connection
  ipp_t		*request,		// IPP request
		*response;		// IPP response
  ipp_attribute_t *attr;		// Current attribute


  (void)num_options;
  (void)options;

  // Connect to/start up the server and get the list of printers...
  if ((http = _papplMainloopConnect(base_name, true)) == NULL)
    return (1);

  request = ippNewRequest(IPP_OP_GET_PRINTERS);
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "system-uri", NULL, "ipp://localhost/ipp/system");
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name", NULL, cupsGetUser());

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
    cups_len_t    num_options,		// I - Number of options
    cups_option_t *options)		// I - Options
{
  http_t		*http;		// HTTP connection
  const char		*printer_uri,	// Printer URI
			*printer_name;	// Printer name
  char			resource[1024];	// Resource path
  ipp_t			*request,	// IPP request
			*response;	// IPP response
  cups_len_t		i,		// Looping var
			count;		// Number of reasons
  int			state;		// *-state value
  ipp_attribute_t	*state_reasons;	// *-state-reasons attribute
  char			state_reasons_str[1024],
					// *-state-reasons string
			*state_reasons_ptr;
					// Pointer into string
  time_t		state_time;	// *-state-change-time value
  char			state_time_str[256];
					// *-state-change-time date string
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

    printer_name = NULL;
  }
  else
  {
    // Connect to the server...
    if ((http = _papplMainloopConnect(base_name, false)) == NULL)
    {
      _papplLocPrintf(stdout, _PAPPL_LOC("Server is not running."));
      return (0);
    }
  }

  if (printer_uri || (printer_name = cupsGetOption("printer-name", num_options, options)) != NULL)
  {
    // Get the printer's status
    response      = get_printer_attributes(http, printer_uri, printer_name, resource, (int)(sizeof(pattrs) / sizeof(pattrs[0])), pattrs);
    state         = ippGetInteger(ippFindAttribute(response, "printer-state", IPP_TAG_ENUM), 0);
    state_time    = ippDateToTime(ippGetDate(ippFindAttribute(response, "printer-state-change-date-time", IPP_TAG_DATE), 0));
    state_reasons = ippFindAttribute(response, "printer-state-reasons", IPP_TAG_KEYWORD);
  }
  else
  {
    // Get the system status
    request = ippNewRequest(IPP_OP_GET_SYSTEM_ATTRIBUTES);
    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "system-uri", NULL, "ipp://localhost/ipp/system");
    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name", NULL, cupsGetUser());
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

  state_reasons_str[0] = '\0';

  if (state_reasons)
  {
    // Build a string with all of the reasons...
    for (i = 0, count = ippGetCount(state_reasons), state_reasons_ptr = state_reasons_str; i < count; i ++)
    {
      reason = ippGetString(state_reasons, i, NULL);

      if (strcmp(reason, "none"))
      {
        if (state_reasons_ptr > state_reasons_str)
          snprintf(state_reasons_ptr, sizeof(state_reasons_str) - (size_t)(state_reasons_ptr - state_reasons_str), ", %s", reason);
	else
	  papplCopyString(state_reasons_str, reason, sizeof(state_reasons_str));

	state_reasons_ptr += strlen(state_reasons_ptr);
      }
    }
  }

  _papplLocPrintf(stdout, _PAPPL_LOC(/* Running, STATE since DATE REASONS */"Running, %s since %s%s."), states[state - IPP_PSTATE_IDLE], httpGetDateString(state_time, state_time_str, sizeof(state_time_str)), state_reasons_str);

  ippDelete(response);

  return (0);
}


//
// 'papplMainloopShutdown()' - Request a shutdown of a running system.
//
// This function requests that the system started by @link papplMainloop@ be
// shutdown.
//

void
papplMainloopShutdown(void)
{
  pthread_mutex_lock(&mainloop_mutex);
  papplSystemShutdown(mainloop_system);
  pthread_mutex_unlock(&mainloop_mutex);
}


//
// '_papplMainloopShutdownServer()' - Shutdown the server.
//

int					// O - Exit status
_papplMainloopShutdownServer(
    const char    *base_name,		// I - Base name
    cups_len_t    num_options,		// I - Number of options
    cups_option_t *options)		// I - Options
{
  http_t	*http;			// HTTP connection
  ipp_t		*request;		// IPP request


  (void)num_options;
  (void)options;

  // Try connecting to the server...
  if ((http = _papplMainloopConnect(base_name, false)) == NULL)
  {
    _papplLocPrintf(stderr, _PAPPL_LOC("%s: Server is not running."), base_name);
    return (1);
  }

  request = ippNewRequest(IPP_OP_SHUTDOWN_ALL_PRINTERS);
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "system-uri", NULL, "ipp://localhost/ipp/system");
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name", NULL, cupsGetUser());

  ippDelete(cupsDoRequest(http, request, "/ipp/system"));

  if (cupsGetError() != IPP_STATUS_OK)
  {
    _papplLocPrintf(stderr, _PAPPL_LOC("%s: Unable to shutdown server: %s"), base_name, cupsGetErrorString());
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
    cups_len_t    num_options,		// I - Number of options
    cups_option_t *options,		// I - Options
    cups_len_t    num_files,		// I - Number of files
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
		*response,		// IPP response
		*supported;		// Supported attributes
  char		default_printer[256],	// Default printer name
		resource[1024],		// Resource path
		tempfile[1024] = "";	// Temporary file
  cups_len_t	i;			// Looping var
  ipp_attribute_t *job_id;		// job-id for created job


#if !_WIN32
  // If there are no input files and stdin is not a TTY, treat that as an
  // implicit request to print from stdin...
  char		*stdin_file;		// Dummy filename for passive stdin jobs

  if (num_files == 0 && !isatty(0))
  {
    stdin_file = (char *)"-";
    files      = &stdin_file;
    num_files  = 1;
  }
#endif // !_WIN32

  if (num_files == 0)
  {
    _papplLocPrintf(stderr, _PAPPL_LOC("%s: No files to print."), base_name);
    return (1);
  }

  if ((printer_uri = cupsGetOption("printer-uri", num_options, options)) != NULL)
  {
    // Connect to the remote printer...
    if ((http = _papplMainloopConnectURI(base_name, printer_uri, resource, sizeof(resource))) == NULL)
      return (1);

    printer_name = NULL;
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
        _papplLocPrintf(stderr, _PAPPL_LOC("%s: No default printer available."), base_name);
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

    // Get supported attributes...
    supported = get_printer_attributes(http, printer_uri, printer_name, resource, 0, NULL);

    // Send a Print-Job request...
    request = ippNewRequest(IPP_OP_PRINT_JOB);
    if (printer_uri)
      ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri", NULL, printer_uri);
    else
      _papplMainloopAddPrinterURI(request, printer_name, resource, sizeof(resource));

    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name", NULL, cupsGetUser());
    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "job-name", NULL, job_name ? job_name : document_name);
    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "document-name", NULL, document_name);

    if (document_format)
      ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_MIMETYPE, "document-format", NULL, document_format);

    _papplMainloopAddOptions(request, num_options, options, supported);
    ippDelete(supported);

    response = cupsDoFileRequest(http, request, resource, filename);

    if ((job_id = ippFindAttribute(response, "job-id", IPP_TAG_INTEGER)) == NULL)
    {
      _papplLocPrintf(stderr, _PAPPL_LOC("%s: Unable to print '%s': %s"), base_name, filename, cupsGetErrorString());
      ippDelete(response);
      httpClose(http);
      return (1);
    }

    if (printer_uri)
      printf("%d\n", ippGetInteger(job_id, 0));
    else
      printf("%s-%d\n", printer_name, ippGetInteger(job_id, 0));

    ippDelete(response);

    if (tempfile[0])
      unlink(tempfile);
  }

  httpClose(http);

  return (0);
}


//
// 'copy_stdin()' - Copy print data from the standard input.
//

static char *				// O - Temporary filename or `NULL` on error
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
  if ((tempfd = cupsCreateTempFd(NULL, NULL, name, (cups_len_t)namesize)) < 0)
  {
    _papplLocPrintf(stderr, _PAPPL_LOC("%s: Unable to create temporary file: %s"), base_name, strerror(errno));
    return (NULL);
  }

  // Read from stdin until we see EOF...
  while ((bytes = read(0, buffer, sizeof(buffer))) > 0)
  {
    if (write(tempfd, buffer, (size_t)bytes) < 0)
    {
      _papplLocPrintf(stderr, _PAPPL_LOC("%s: Unable to write to temporary file: %s"), base_name, strerror(errno));
      goto fail;
    }

    total += (size_t)bytes;
  }

  // Only allow non-empty files...
  if (total == 0)
  {
    _papplLocPrintf(stderr, _PAPPL_LOC("%s: Empty print file received on the standard input."), base_name);
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
// 'default_system_cb()' - Create a system object.
//

static pappl_system_t *			// O - System object
default_system_cb(
    const char    *base_name,		// I - Base name of application
    int           num_options,		// I - Number of options
    cups_option_t *options,		// I - Options
    void          *data)		// I - Data (unused)
{
  pappl_system_t *system;		// System object
  pappl_soptions_t soptions = PAPPL_SOPTIONS_MULTI_QUEUE | PAPPL_SOPTIONS_WEB_INTERFACE | PAPPL_SOPTIONS_WEB_TLS;
					// Server options
  char		spoolname[1024];	// Default spool directory
  const char	*directory = cupsGetOption("spool-directory", (cups_len_t)num_options, options),
					// Spool directory
		*logfile = cupsGetOption("log-file", (cups_len_t)num_options, options),
					// Log file
		*server_hostname = cupsGetOption("server-hostname", (cups_len_t)num_options, options),
					// Server hostname
		*value,			// Other option
		*valptr;		// Pointer into option
  pappl_loglevel_t loglevel = PAPPL_LOGLEVEL_WARN;
					// Log level
  int		port = 0;		// Port
#if _WIN32
  const char	*home = getenv("USERPROFILE");
					// Home directory
#else
  const char	*home = getenv("HOME");	// Home directory
#endif // _WIN32
  const char	*snap_common = getenv("SNAP_COMMON");
					// Common data directory for snaps
  const char	*tmpdir = papplGetTempDir();
					// Temporary directory


  (void)data;

  // Collect standard options...
  if ((value = cupsGetOption("log-level", (cups_len_t)num_options, options)) != NULL)
  {
    if (!strcmp(value, "fatal"))
      loglevel = PAPPL_LOGLEVEL_FATAL;
    else if (!strcmp(value, "error"))
      loglevel = PAPPL_LOGLEVEL_ERROR;
    else if (!strcmp(value, "warn"))
      loglevel = PAPPL_LOGLEVEL_WARN;
    else if (!strcmp(value, "info"))
      loglevel = PAPPL_LOGLEVEL_INFO;
    else if (!strcmp(value, "debug"))
      loglevel = PAPPL_LOGLEVEL_DEBUG;
  }

  if ((value = cupsGetOption("server-options", (cups_len_t)num_options, options)) != NULL)
  {
    for (valptr = value; valptr && *valptr;)
    {
      if (!strcmp(valptr, "none") || !strncmp(valptr, "none,", 5))
        soptions = PAPPL_SOPTIONS_NONE;
      else if (!strcmp(valptr, "dnssd-host") || !strncmp(valptr, "dnssd-host,", 11))
        soptions |= PAPPL_SOPTIONS_DNSSD_HOST;
      else if (!strcmp(valptr, "no-multi-queue") || !strncmp(valptr, "no-multi-queue,", 15))
        soptions &= (pappl_soptions_t)~PAPPL_SOPTIONS_MULTI_QUEUE;
      else if (!strcmp(valptr, "raw-socket") || !strncmp(valptr, "raw-socket,", 11))
        soptions |= PAPPL_SOPTIONS_RAW_SOCKET;
      else if (!strcmp(valptr, "usb-printer") || !strncmp(valptr, "usb-printer,", 12))
        soptions |= PAPPL_SOPTIONS_USB_PRINTER;
      else if (!strcmp(valptr, "no-web-interface") || !strncmp(valptr, "no-web-interface,", 17))
        soptions &= (pappl_soptions_t)~PAPPL_SOPTIONS_WEB_INTERFACE;
      else if (!strcmp(valptr, "web-log") || !strncmp(valptr, "web-log,", 8))
        soptions |= PAPPL_SOPTIONS_WEB_LOG;
      else if (!strcmp(valptr, "web-network") || !strncmp(valptr, "web-network,", 12))
        soptions |= PAPPL_SOPTIONS_WEB_NETWORK;
      else if (!strcmp(valptr, "web-remote") || !strncmp(valptr, "web-remote,", 11))
        soptions |= PAPPL_SOPTIONS_WEB_REMOTE;
      else if (!strcmp(valptr, "web-security") || !strncmp(valptr, "web-security,", 13))
        soptions |= PAPPL_SOPTIONS_WEB_SECURITY;
      else if (!strcmp(valptr, "no-tls") || !strncmp(valptr, "no-tls,", 7))
        soptions = (pappl_soptions_t)((soptions | PAPPL_SOPTIONS_NO_TLS) & (pappl_soptions_t)~PAPPL_SOPTIONS_WEB_TLS);

      if ((valptr = strchr(valptr, ',')) != NULL)
        valptr ++;
    }
  }

  if ((value = cupsGetOption("server-port", (cups_len_t)num_options, options)) != NULL)
  {
    char *end;			// End of value

    port = (int)strtol(value, &end, 10);

    if (port < 0 || errno == ERANGE || *end)
    {
      _papplLocPrintf(stderr, _PAPPL_LOC("%s: Bad 'server-port' value."), base_name);
      return (NULL);
    }
  }

  // Make sure we have a spool directory...
  if (!directory)
  {
    spoolname[0] = '\0';

    if (snap_common)
    {
      // Running inside a snap (https://snapcraft.io), so use the snap's common
      // data directory...
      snprintf(spoolname, sizeof(spoolname), "%s/%s.d", snap_common, base_name);
    }
#if !_WIN32
    else if (!getuid())
    {
      // Running as root, so put the state file in the local state directory
      snprintf(spoolname, sizeof(spoolname), PAPPL_STATEDIR "/spool/%s", base_name);

      // Make sure base directory exists
      if (mkdir(PAPPL_STATEDIR "/spool", 0777) && errno != EEXIST)
      {
	// Can't use local state directory, so use the last resort...
	spoolname[0] = '\0';
      }
    }
#endif // !_WIN32
    else if (home)
    {
#ifdef __APPLE__
      // Put the spool directory in "~/Library/Application Support"
      snprintf(spoolname, sizeof(spoolname), "%s/Library/Application Support/%s.d", home, base_name);
#elif _WIN32
      // Put the spool directory in "~/AppData/Local"
      snprintf(spoolname, sizeof(spoolname), "%s/AppData/Local/%s.d", home, base_name);
#else
      // Put the spool directory under a ".config" directory in the home directory
      snprintf(spoolname, sizeof(spoolname), "%s/.config", home);

      // Make ~/.config as needed
      if (mkdir(spoolname, 0777) && errno != EEXIST)
	spoolname[0] = '\0';

      if (spoolname[0])
	snprintf(spoolname, sizeof(spoolname), "%s/.config/%s.d", home, base_name);
#endif // __APPLE__
    }

    if (!spoolname[0])
    {
      // As a last resort, put the spool directory in the temporary directory
      // (where it will be lost on the nest reboot/logout...
#if _WIN32
      snprintf(spoolname, sizeof(spoolname), "%s/%s.d", tmpdir, base_name);
#else
      snprintf(spoolname, sizeof(spoolname), "%s/%s%d.d", tmpdir, base_name, (int)getuid());
#endif // _WIN32
    }

    directory = spoolname;
  }

  // Create the system object...
  system = papplSystemCreate(soptions, base_name, port, "_print,_universal", directory, logfile, loglevel, cupsGetOption("auth-service", (cups_len_t)num_options, options), /* tls_only */false);

  // Set any admin group and listen for network connections...
  if ((value = cupsGetOption("admin-group", (cups_len_t)num_options, options)) != NULL)
    papplSystemSetAdminGroup(system, value);

  if (server_hostname)
    papplSystemSetHostName(system, server_hostname);

  if (!cupsGetOption("private-server", (cups_len_t)num_options, options))
  {
    // Listen for TCP/IP connections...
    papplSystemAddListeners(system, cupsGetOption("listen-hostname", (cups_len_t)num_options, options));
  }

  return (system);
}


//
// 'get_printer_attributes()' - Get printer attributes.
//

static ipp_t *				// O - IPP response
get_printer_attributes(
    http_t             *http,		// I - HTTP connection
    const char         *printer_uri,	// I - Printer URI, if any
    const char         *printer_name,	// I - Printer name, if any
    const char         *resource,	// I - Resource path
    cups_len_t         num_requested,	// I - Number of requested attributes
    const char * const *requested)	// I - Requested attributes or `NULL`
{
  ipp_t	*request;			// IPP request
  char	temp[1024];			// Temporary string


  request = ippNewRequest(IPP_OP_GET_PRINTER_ATTRIBUTES);
  if (printer_uri)
  {
    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri", NULL, printer_uri);
  }
  else
  {
    _papplMainloopAddPrinterURI(request, printer_name, temp, sizeof(temp));
    resource = temp;
  }

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name", NULL, cupsGetUser());

  if (num_requested > 0 && requested)
    ippAddStrings(request, IPP_TAG_OPERATION, IPP_TAG_KEYWORD, "requested-attributes", num_requested, NULL, requested);

  return (cupsDoRequest(http, request, resource));
}


//
// 'get_value()' - Get the string representation of an attribute value.
//

static char *				// O - String value
get_value(ipp_attribute_t *attr,	// I - Attribute
          const char      *name,	// I - Base name of attribute
          cups_len_t      element,	// I - Value index
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
            papplCopyString(buffer, value, bufsize);
	  }
	}
	break;

    case IPP_TAG_ENUM :
        papplCopyString(buffer, ippEnumString(name, ippGetInteger(attr, element)), bufsize);
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
// 'load_options()' - Load options from a file.
//

static cups_len_t			// O  - New number of options
load_options(const char    *filename,	// I  - Filename
             cups_len_t    num_options,	// I  - Number of options
             cups_option_t **options)	// IO - Options
{
  cups_file_t	*fp;			// File pointer
  char		line[8192];		// Line from file
  cups_len_t	i,			// Looping var
		num_loptions;		// Number of line options
  cups_option_t	*loptions,		// Line options
		*loption;		// Current line option


  // Open the file...
  if ((fp = cupsFileOpen(filename, "r")) == NULL)
    return (num_options);

  // Read lines until EOF...
  while (cupsFileGets(fp, line, sizeof(line)))
  {
    // Skip comment and blank lines...
    if (line[0] == '#' || !line[0])
      continue;

    // Parse any options on this line...
    num_loptions = cupsParseOptions(line, /*end*/NULL, 0, &loptions);

    // Copy any unset line options to the options array...
    for (i = num_loptions, loption = loptions; i > 0; i --, loption ++)
    {
      if (!cupsGetOption(loption->name, num_options, *options))
        num_options = cupsAddOption(loption->name, loption->value, num_options, options);
    }

    // Free the line options...
    cupsFreeOptions(num_loptions, loptions);
  }

  // Close the file and return...
  cupsFileClose(fp);

  return (num_options);
}


//
// 'print_option()' - Print the supported and default value for an option.
//

static void
print_option(ipp_t      *response,	// I - Get-Printer-Attributes response
	     const char *name)		// I - Attribute name
{
  char		defname[256],		// xxx-default/xxx-configured name
		supname[256];		// xxx-supported name
  ipp_attribute_t *defattr,		// xxx-default/xxx-configured attribute
		*supattr;		// xxx-supported attribute
  cups_len_t	i,			// Looping var
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
    _papplLocPrintf(stdout, _PAPPL_LOC("  -o %s=%s (default)"), name, defvalue);

  for (i = 0, count = ippGetCount(supattr); i < count; i ++)
  {
    if (strcmp(defvalue, get_value(supattr, name, i, supvalue, sizeof(supvalue))))
      printf("  -o %s=%s\n", name, supvalue);
  }
}


#if _WIN32
//
// 'save_server_port()' - Save the port number we are using for the server in
//                        the registry.
//

static void
save_server_port(const char *base_name,	// I - Base name of application
                 int        port)	// I - TCP/IP port number
{
  char		path[1024];		// Registry path
  HKEY		key;			// Registry key
  DWORD		dport;			// Port number value for registry


  // The server's port number is saved in SOFTWARE\appname\port
  snprintf(path, sizeof(path), "SOFTWARE\\%s", base_name);

  if (!RegOpenKeyExA(HKEY_LOCAL_MACHINE, path, 0, KEY_WRITE, &key))
  {
    // Was able to open the registry, save the port number...
    dport = (DWORD)port;
    RegSetKeyValueA(key, NULL, "port", REG_DWORD, &dport, sizeof(dport));
    RegCloseKey(key);
  }
}
#endif // _WIN32
