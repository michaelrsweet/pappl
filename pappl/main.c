//
// Command line utilities for the Printer Application Framework
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
#  include <spawn.h>
#  include <libgen.h>


static char *_papplPath;  // Path to self


//
// Local functions
//

static void   device_error_cb(const char *message, void *err_data);
static bool   device_list_cb(const char *device_uri, const char *device_id, void *data);
static int    get_length(const char *value);
static void   pappl_main_usage_cb();
static void   pappl_main_error_cb();
static void   help();


//
// 'papplMain()' - Main entry for pappl.
//

int           // O - Exit status
papplMain(
    int               argc,          // I - Number of command line arguments
    char              *argv[],       // I - Command line arguments
    pappl_driver_cb_t driver_cb,     // I - Callback for driver
    const char        *cb_state,     // I - System Callback
    const char        *footer,       // I - Footer
    pappl_soptions_t  soptions,      // I - System options
    int               num_versions,  // I - Number of system versions
    pappl_version_t   *sversion,     // I - System version info
    pappl_contact_t   *scontact,     // I - System contact
    const char        *geolocation,  // I - System geolocation
    const char        *organization, // I - System organization
    pappl_usage_cb_t  usage_cb,      // I - Usage callback
    pappl_error_cb_t  error_cb)      // I - Error callback
{
  char             *files[1000];            // Files array
  int              num_files = 0;           // File count
  int              num_options = 0;         // Number of options
  cups_option_t    *options = NULL;         // Options
  const char       *subcommand = NULL;      // Sub command
  bool             ret = 0;                 // Return value
  char             *base_name;              // Base Name


  _papplPath = argv[0];
  base_name = basename(argv[0]);

  // If no callback is provided, use the default callback
  if (!usage_cb)
  {
    _PAPPL_DEBUG("Using the default usage callback.\n");
    usage_cb = pappl_main_usage_cb;
  }
  if (!error_cb)
  {
    _PAPPL_DEBUG("Using the default error callback.\n");
    error_cb = pappl_main_error_cb;
  }

  for (int i = 1; i < argc; i ++)
  {
    if (!strcmp(argv[i], "--help"))
    {
      (*usage_cb)();
    }
    else if (!strcmp(argv[i], "--version"))
    {
      puts(PAPPL_VERSION);
      return (0);
    }
    else if (!strcmp(argv[i], "--list-devices"))
    {
      papplDeviceList(PAPPL_DTYPE_ALL, (pappl_device_cb_t)device_list_cb, NULL, (pappl_deverr_cb_t)device_error_cb, NULL);
      return (0);
    }
    else if (!strcmp(argv[i], "--list-devices-dns-sd"))
    {
      papplDeviceList(PAPPL_DTYPE_DNS_SD, device_list_cb, NULL, device_error_cb, NULL);
      return (0);
    }
    else if (!strcmp(argv[i], "--list-devices-local"))
    {
      papplDeviceList(PAPPL_DTYPE_ALL_LOCAL, device_list_cb, NULL, device_error_cb, NULL);
      return (0);
    }
    else if (!strcmp(argv[i], "--list-devices-remote"))
    {
      papplDeviceList(PAPPL_DTYPE_ALL_REMOTE, device_list_cb, NULL, device_error_cb, NULL);
      return (0);
    }
    else if (!strcmp(argv[i], "--list-devices-usb"))
    {
      papplDeviceList(PAPPL_DTYPE_USB, device_list_cb, NULL, device_error_cb, NULL);
      return (0);
    }
    else if (!strcmp(argv[i], "--list-printers"))
    {
      ret = _papplMainListPrinters(base_name);
      return (ret == true ? 0 : 1);
    }
    else if (!strncmp(argv[i], "--", 2))
    {
      const char *opt = argv[i] + 2;

      if (!strcmp(opt, "add"))
        subcommand = "add";
      else if (!strcmp(opt, "cancel"))
        subcommand = "cancel";
      else if (!strcmp(opt, "default"))
        subcommand = "default";
      else if (!strcmp(opt, "delete"))
        subcommand = "delete";
      else if (!strcmp(opt, "jobs"))
        subcommand = "jobs";
      else if (!strcmp(opt, "modify"))
        subcommand = "modify";
      else if (!strcmp(opt, "options"))
        subcommand = "options";
      else if (!strcmp(opt, "server"))
        subcommand = "server";
      else if (!strcmp(opt, "shutdown"))
        subcommand = "shutdown";
      else if (!strcmp(opt, "status"))
        subcommand = "status";
      else if (!strcmp(opt, "submit"))
        subcommand = "submit";
      else
      {
        printf("%s: Unknown option '%s'.\n", base_name, argv[i]);
        (*error_cb)();
      }
    }
    else if (argv[i][0]=='-')
    {
      switch (argv[i][1])
      {
          // cancel-all
          case 'a':
              num_options = cupsAddOption("cancel-all", "true", num_options, &options);
          break;

          // authentication service
          case 'A':
              i ++;
              if (i >= argc)
              {
                printf("%s: Missing PAM service name after '-A'.\n", base_name);
                (*error_cb)();
              }
              num_options = cupsAddOption("auth", argv[i], num_options, &options);
          break;

          // copies
          case 'c':
              i ++;
              if (i >= argc)
              {
                printf("%s: Missing copy count after '-c'.\n", base_name);
                (*error_cb)();
              }
              num_options = cupsAddOption("num-copies", argv[i], num_options, &options);
          break;

          // printer
          case 'd':
              i ++;
              if (i >= argc)
              {
                printf("%s: Missing printer name after '-d'.\n", base_name);
                (*error_cb)();
              }
              num_options = cupsAddOption("printer-name", argv[i], num_options, &options);
          break;

          // host
          case 'h':
              i ++;
              if (i >= argc)
              {
                printf("%s: Missing hostname after '-h'.\n", base_name);
                (*error_cb)();
              }
              num_options = cupsAddOption("server-hostname", argv[i], num_options, &options);
          break;

          // job id
          case 'j':
              i ++;
              if (i >= argc)
              {
                printf("%s: Missing job-id after '-j'.\n", base_name);
                (*error_cb)();
              }
              num_options = cupsAddOption("job-id", argv[i], num_options, &options);
          break;

          // log file
          case 'l':
              i ++;
              if (i >= argc)
              {
                  printf("%s: Missing file name after '-l'.\n", base_name);
                  (*error_cb)();
              }
              num_options = cupsAddOption("log-file", argv[i], num_options, &options);
          break;

          // log level
          case 'L':
              i ++;
              if (i >= argc)
              {
                printf("%s: Missing log level after '-L'.\n", base_name);
                (*error_cb)();
              }

              if (!strcmp(argv[i], "debug"))
                num_options = cupsAddOption("log-level", "debug", num_options, &options);
              else if (!strcmp(argv[i], "error"))
                num_options = cupsAddOption("log-level", "error", num_options, &options);
              else if (!strcmp(argv[i], "fatal"))
                num_options = cupsAddOption("log-level", "fatal", num_options, &options);
              else if (!strcmp(argv[i], "info"))
                num_options = cupsAddOption("log-level", "info", num_options, &options);
              else if (!strcmp(argv[i], "warn"))
                num_options = cupsAddOption("log-level", "warn", num_options, &options);
              else
              {
                printf("%s: Unknown log level '%s'.\n", base_name, argv[i]);
                (*error_cb)();
              }
          break;

          // driver name
          case 'm':
              i ++;
              if (i >= argc)
              {
                printf("%s: Missing driver after '-m'.\n", base_name);
                (*error_cb)();
              }
              num_options = cupsAddOption("driver", argv[i], num_options, &options);
          break;

          // server name
          case 'n':
              i ++;
              if (i >= argc)
              {
                printf("%s: Missing server name after '-n'.\n", base_name);
                (*error_cb)();
              }
              num_options = cupsAddOption("system-name", argv[i], num_options, &options);
          break;

          // option
          case 'o':
              i ++;
              if (i >= argc)
              {
                printf("%s: Missing option attribute after '-o'.\n", base_name);
                (*error_cb)();
              }
              num_options = cupsParseOptions(argv[i], num_options, &options);
          break;

          // port
          case 'p':
              i ++;
              if (i >= argc)
              {
                printf("%s: Missing port after '-p'.\n", base_name);
                (*error_cb)();
              }
              num_options = cupsAddOption("server-port", argv[i], num_options, &options);
          break;

          // spool directory
          case 's':
              i ++;
              if (i >= argc)
              {
                printf("%s: Missing spool directory after '-s'.\n", base_name);
                (*error_cb)();
              }
              num_options = cupsAddOption("spool", argv[i], num_options, &options);
          break;

          // printer uri
          case 'u':
              i ++;
              if (i >= argc)
              {
                printf("%s: Missing printer-uri after '-u'.\n", base_name);
                (*error_cb)();
              }
              num_options = cupsAddOption("printer-uri", argv[i], num_options, &options);
          break;

          // device uri
          case 'v':
              i ++;
              if (i >= argc)
              {
                printf("%s: Missing device-uri after '-v'.\n", base_name);
                (*error_cb)();
              }
              num_options = cupsAddOption("device-uri", argv[i], num_options, &options);
          break;

          default:
              printf("%s: Unknown option '%s'.\n", argv[i], base_name);
              (*error_cb)();
        }
    }
    else
    {
      if (num_files >= (int)(sizeof(files) / sizeof(files[0])))
      {
        printf("%s: Cannot print more files.\n", base_name);
        return (1);
      }
      files[num_files++] = argv[i];
    }
  }

  if (!subcommand && num_files > 0)
    subcommand = "submit";

  if (num_files && strcmp(subcommand, "submit"))
  {
    printf("%s: '%s' subcommand does not accept files.\n", base_name, subcommand);
    (*error_cb)();
  }

  // handle subcommands
  if (subcommand)
  {
    if (!strcmp(subcommand, "add"))
      ret = _papplMainAdd(base_name, num_options, options);
    else if (!strcmp(subcommand, "cancel"))
      ret = _papplMainCancel(base_name, num_options, options);
    else if (!strcmp(subcommand, "default"))
      ret = _papplMainDefault(base_name, num_options, options);
    else if (!strcmp(subcommand, "delete"))
      ret = _papplMainDelete(base_name, num_options, options);
    else if (!strcmp(subcommand, "jobs"))
      ret = _papplMainJobs(base_name, num_options, options);
    else if (!strcmp(subcommand, "modify"))
      ret = _papplMainModify(base_name, num_options, options);
    else if (!strcmp(subcommand, "options"))
      ret = _papplMainOptions(base_name, num_options, options);
    else if (!strcmp(subcommand, "server"))
      ret = _papplMainServer(base_name, num_options, options, driver_cb, cb_state, footer, soptions, num_versions, sversion, scontact, geolocation, organization);
    else if (!strcmp(subcommand, "shutdown"))
      ret = _papplMainShutdown(base_name, num_options, options);
    else if (!strcmp(subcommand, "status"))
      ret = _papplMainStatus(base_name, num_options, options);
    else
      ret = _papplMainSubmit(base_name, num_options, options, num_files, files);
    return (ret == true ? 0 : 1);
  }

  return (0);
}


//
// '_papplMainAddOptions()' - Add default/job template attributes from options.
//

void
_papplMainAddOptions(
    ipp_t         *request,		// I - IPP request
    int           num_options,		// I - Number of options
    cups_option_t *options)		// I - Options
{
  int		is_default;		// Adding xxx-default attributes?
  ipp_tag_t	group_tag;		// Group to add to
  const char	*value;			// String value
  int		intvalue;		// Integer value
  const char	*media_source = cupsGetOption("media-source", num_options, options),
    *media_top_offset = cupsGetOption("media-top-offset", num_options, options),
    *media_tracking = cupsGetOption("media-tracking", num_options, options),
    *media_type = cupsGetOption("media-type", num_options, options);
                      // media-xxx member values


  group_tag  = ippGetOperation(request) == IPP_PRINT_JOB ? IPP_TAG_JOB : IPP_TAG_PRINTER;
  is_default = (group_tag == IPP_TAG_PRINTER);

  if (is_default)
  {
    // Add Printer Description attributes...
    if ((value = cupsGetOption("label-mode-configured", num_options, options)) != NULL)
      ippAddString(request, IPP_TAG_PRINTER, IPP_TAG_KEYWORD, "label-mode-configured", NULL, value);

    if ((value = cupsGetOption("label-tear-offset-configured", num_options, options)) != NULL)
      ippAddInteger(request, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "label-tear-offset-configured", get_length(value));

    if ((value = cupsGetOption("media-ready", num_options, options)) != NULL)
    {
      int	num_values;		// Number of values
      char	*values[4],		// Pointers to size strings
        sizes[4][256];		// Size strings


      if ((num_values = sscanf(value, "%255s,%255s,%255s,%255s", sizes[0], sizes[1], sizes[2], sizes[3])) > 0)
      {
        values[0] = sizes[0];
        values[1] = sizes[1];
        values[2] = sizes[2];
        values[3] = sizes[3];

        ippAddStrings(request, IPP_TAG_PRINTER, IPP_TAG_KEYWORD, "media-ready", num_values, NULL, (const char * const *)values);
      }
    }

    if ((value = cupsGetOption("printer-darkness-configured", num_options, options)) != NULL)
      ippAddInteger(request, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "printer-darkness-configured", atoi(value));

    if ((value = cupsGetOption("printer-geo-location", num_options, options)) != NULL)
      ippAddString(request, IPP_TAG_PRINTER, IPP_TAG_URI, "printer-geo-location", NULL, value);

    if ((value = cupsGetOption("printer-location", num_options, options)) != NULL)
      ippAddString(request, IPP_TAG_PRINTER, IPP_TAG_TEXT, "printer-location", NULL, value);

    if ((value = cupsGetOption("printer-organization", num_options, options)) != NULL)
      ippAddString(request, IPP_TAG_PRINTER, IPP_TAG_TEXT, "printer-organization", NULL, value);

    if ((value = cupsGetOption("printer-organizational-unit", num_options, options)) != NULL)
      ippAddString(request, IPP_TAG_PRINTER, IPP_TAG_TEXT, "printer-organizational-unit", NULL, value);
  }

  if ((value = cupsGetOption("copies", num_options, options)) == NULL)
    value = cupsGetOption("copies-default", num_options, options);
  if (value)
    ippAddInteger(request, group_tag, IPP_TAG_INTEGER, is_default ? "copies-default" : "copies", atoi(value));

  value = cupsGetOption("media", num_options, options);
  if (media_source || media_top_offset || media_tracking || media_type)
  {
    // Add media-col
    ipp_t 	*media_col = ippNew();	// media-col value
    pwg_media_t *pwg = pwgMediaForPWG(value);
                    // Size

    if (pwg)
    {
      ipp_t		*media_size = ippNew();
                  // media-size value

      ippAddInteger(media_size, IPP_TAG_JOB, IPP_TAG_INTEGER, "x-dimension", pwg->width);
      ippAddInteger(media_size, IPP_TAG_JOB, IPP_TAG_INTEGER, "y-dimension", pwg->length);
      ippAddCollection(media_col, IPP_TAG_JOB, "media-size", media_size);
      ippDelete(media_size);
    }

    if (media_source)
      ippAddString(media_col, IPP_TAG_JOB, IPP_TAG_KEYWORD, "media-source", NULL, media_source);

    if (media_top_offset)
      ippAddInteger(media_col, IPP_TAG_JOB, IPP_TAG_INTEGER, "media-top-offset", get_length(media_top_offset));

    if (media_tracking)
      ippAddString(media_col, IPP_TAG_JOB, IPP_TAG_KEYWORD, "media-tracking", NULL, media_tracking);

    if (media_type)
      ippAddString(media_col, IPP_TAG_JOB, IPP_TAG_KEYWORD, "media-type", NULL, media_type);

    ippAddCollection(request, IPP_TAG_JOB, is_default ? "media-col-default" : "media-col", media_col);
    ippDelete(media_col);
  }
  else if (value)
  {
    // Add media
    ippAddString(request, IPP_TAG_JOB, IPP_TAG_KEYWORD, is_default ? "media-default" : "media", NULL, value);
  }

  if ((value = cupsGetOption("orientation-requested", num_options, options)) == NULL)
    value = cupsGetOption("orientation-requested-default", num_options, options);
  if (value)
  {
    if ((intvalue = ippEnumValue("orientation-requested", value)) != 0)
      ippAddInteger(request, group_tag, IPP_TAG_ENUM, is_default ? "orientation-requested-default" : "orientation-requested", intvalue);
    else
      ippAddInteger(request, group_tag, IPP_TAG_ENUM, is_default ? "orientation-requested-default" : "orientation-requested", atoi(value));
  }

  if ((value = cupsGetOption("print-color-mode", num_options, options)) == NULL)
    value = cupsGetOption("print-color-mode-default", num_options, options);
  if (value)
    ippAddString(request, group_tag, IPP_TAG_KEYWORD, is_default ? "print-color-mode-default" : "print-color-mode", NULL, value);

  if ((value = cupsGetOption("print-content-optimize", num_options, options)) == NULL)
    value = cupsGetOption("print-content-optimize-default", num_options, options);
  if (value)
    ippAddString(request, group_tag, IPP_TAG_KEYWORD, is_default ? "print-content-optimize-mode-default" : "print-content-optimize", NULL, value);

  if ((value = cupsGetOption("print-darkness", num_options, options)) == NULL)
    value = cupsGetOption("print-darkness-default", num_options, options);
  if (value)
    ippAddInteger(request, group_tag, IPP_TAG_INTEGER, is_default ? "print-darkness-default" : "print-darkness", atoi(value));

  if ((value = cupsGetOption("print-quality", num_options, options)) == NULL)
    value = cupsGetOption("print-quality-default", num_options, options);
  if (value)
  {
    if ((intvalue = ippEnumValue("print-quality", value)) != 0)
      ippAddInteger(request, group_tag, IPP_TAG_ENUM, is_default ? "print-quality-default" : "print-quality", intvalue);
    else
      ippAddInteger(request, group_tag, IPP_TAG_ENUM, is_default ? "print-quality-default" : "print-quality", atoi(value));
  }

  if ((value = cupsGetOption("print-speed", num_options, options)) == NULL)
    value = cupsGetOption("print-speed-default", num_options, options);
  if (value)
    ippAddInteger(request, group_tag, IPP_TAG_INTEGER, is_default ? "print-speed-default" : "print-speed", get_length(value));

  if ((value = cupsGetOption("printer-resolution", num_options, options)) == NULL)
    value = cupsGetOption("printer-resolution-default", num_options, options);

  if (value)
  {
    int		xres,
      yres;		// Resolution values
    char	units[32];		// Resolution units

    if (sscanf(value, "%dx%d%31s", &xres, &yres, units) != 3)
    {
      if (sscanf(value, "%d%31s", &xres, units) != 2)
      {
        xres = 300;

        strlcpy(units, "dpi", sizeof(units));
      }

      yres = xres;
    }

    ippAddResolution(request, group_tag, is_default ? "printer-resolution-default" : "printer-resolution", !strcmp(units, "dpi") ? IPP_RES_PER_INCH : IPP_RES_PER_CM, xres, yres);
  }
}


//
// '_papplMainAddPrinterURI()' - Add the printer-uri attribute and return a resource path.
//

void
_papplMainAddPrinterURI(
    ipp_t      *request,		// I - IPP request
    const char *printer_name,		// I - Printer name
    char       *resource,		// I - Resource path buffer
    size_t     rsize)			// I - Size of buffer
{
  char	uri[1024];			// printer-uri value


  snprintf(resource, rsize, "/ipp/print/%s", printer_name);
  httpAssembleURI(HTTP_URI_CODING_ALL, uri, sizeof(uri), "ipp", NULL, "localhost", 0, resource);

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri", NULL, uri);
}


//
// '_papplMainConnect()' - Connect to the local server.
//

http_t *				// O - HTTP connection
_papplMainConnect(
      char *base_name,      // I - Base Name
      int  auto_start)		// I - 1 to start server if not running
{
  http_t	*http;			// HTTP connection
  char		sockname[1024];		// Socket filename


  // See if the server is running...
  http = httpConnect2(_papplMainGetServerPath(base_name, sockname, sizeof(sockname)), 0, NULL, AF_UNSPEC, HTTP_ENCRYPTION_IF_REQUESTED, 1, 30000, NULL);

  if (!http && auto_start)
  {
    // Nope, start it now...
    pid_t	server_pid;		// Server process ID
    posix_spawnattr_t server_attrs;	// Server process attributes
    char * const server_argv[] =	// Server command-line
    {
        _papplPath,
        "--server",
        NULL
    };

    posix_spawnattr_init(&server_attrs);
    posix_spawnattr_setpgroup(&server_attrs, 0);

    if (posix_spawn(&server_pid, _papplPath, NULL, &server_attrs, server_argv, environ))
    {
      perror("Unable to start pappl server");
      posix_spawnattr_destroy(&server_attrs);
      return (NULL);
    }

    posix_spawnattr_destroy(&server_attrs);

    // Wait for it to start...
    do
    {
      usleep(250000);
    }
    while (access(_papplMainGetServerPath(base_name, sockname, sizeof(sockname)), 0));

    http = httpConnect2(sockname, 0, NULL, AF_UNSPEC, HTTP_ENCRYPTION_IF_REQUESTED, 1, 30000, NULL);

    if (!http)
      fprintf(stderr, "%s: Unable to connect to server - %s\n", base_name, cupsLastErrorString());
  }

  return (http);
}


//
// '_papplMainConnectURI()' - Connect to an IPP printer directly.
//

http_t *				// O - HTTP connection or `NULL` on error
_papplMainConnectURI(
    char       *base_name,		// I - Base Name
    const char *printer_uri,		// I - Printer URI
    char       *resource,		// I - Resource path buffer
    size_t     rsize)			// I - Size of buffer
{
  char			scheme[32],	// Scheme (ipp or ipps)
    userpass[256],	// Username/password (unused)
    hostname[256];	// Hostname
  int			port;		// Port number
  http_encryption_t	encryption;	// Type of encryption to use
  http_t		*http;		// HTTP connection


  // First extract the components of the URI...
  if (httpSeparateURI(HTTP_URI_CODING_ALL, printer_uri, scheme, sizeof(scheme), userpass, sizeof(userpass), hostname, sizeof(hostname), &port, resource, (int)rsize) < HTTP_URI_STATUS_OK)
  {
    fprintf(stderr, "%s: Bad printer URI '%s'.\n", base_name, printer_uri);
    return (NULL);
  }

  if (strcmp(scheme, "ipp") && strcmp(scheme, "ipps"))
  {
    fprintf(stderr, "%s: Unsupported URI scheme '%s'.\n", base_name, scheme);
    return (NULL);
  }

  if (userpass[0])
    fprintf(stderr, "%s: User credentials are not supported in URIs.\n", base_name);

  if (!strcmp(scheme, "ipps") || port == 443)
    encryption = HTTP_ENCRYPTION_ALWAYS;
  else
    encryption = HTTP_ENCRYPTION_IF_REQUESTED;

  if ((http = httpConnect2(hostname, port, NULL, AF_UNSPEC, encryption, 1, 30000, NULL)) == NULL)
    fprintf(stderr, "%s: Unable to connect to printer at '%s' - %s\n", base_name, printer_uri, cupsLastErrorString());

  return (http);
}

//
// '_papplMainGetDefaultPrinter' - Get the default printer.
//

char *					// O - Default printer or `NULL` for none
_papplMainGetDefaultPrinter(
    http_t *http,			// I - HTTP connection
    char   *buffer,			// I - Buffer for printer name
    size_t bufsize)			// I - Size of buffer
{
  ipp_t		*request,		// IPP request
    *response;		// IPP response
  const char	*printer_name;		// Printer name


  // Ask the server for its default printer
  request = ippNewRequest(IPP_OP_CUPS_GET_DEFAULT);
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name", NULL, cupsUser());
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_KEYWORD, "requested-attributes", NULL, "printer-name");

  response = cupsDoRequest(http, request, "/ipp/system");

  if ((printer_name = ippGetString(ippFindAttribute(response, "printer-name", IPP_TAG_NAME), 0, NULL)) != NULL)
    strlcpy(buffer, printer_name, bufsize);
  else
    *buffer = '\0';

  ippDelete(response);

  return (*buffer ? buffer : NULL);
}


//
// '_papplMainGetServerPath()' - Get the UNIX domain socket for the server.
//

char *					// O - Socket filename
_papplMainGetServerPath(
    char   *base_name,  // I - Base name
    char   *buffer,	    // I - Buffer for filename
    size_t bufsize)	    // I - Size of buffer
{
  const char	*tmpdir = getenv("TMPDIR");
					// Temporary directory

#ifdef __APPLE__
  if (!tmpdir)
    tmpdir = "/private/tmp";
#else
  if (!tmpdir)
    tmpdir = "/tmp";
#endif // __APPLE__

  snprintf(buffer, bufsize, "%s/%s%d.sock", tmpdir, base_name, (int)getuid());
  _PAPPL_DEBUG("Creating domain socket as '%s'\n", buffer);

  return (buffer);
}


//
// 'device_error_cb()' - Show a device error message.
//

static void
device_error_cb(
    const char *message,	// I - Error message
    void       *err_data)	// I - Callback data (unused)
{
  (void)err_data;

  printf("device_error_cb: Error - %s\n", message);
}


//
// 'device_list_cb()' - List a device.
//

static bool				// O - `true` to stop, `false` to continue
device_list_cb(
    const char *device_uri,	// I - Device URI
    const char *device_id,	// I - IEEE-1284 device ID
    void       *data)	// I - Callback data (unused)
{
  (void)data;

  printf("%s\n    %s\n", device_uri, device_id);

  return (false);
}


//
// 'get_length()' - Get a length in hundreths of millimeters.
//

static int				// O - Length value
get_length(
    const char *value)		// I - Length string
{
  double	n;			// Number
  char		*units;			// Pointer to units


  n = strtod(value, &units);

  if (units && !strcmp(units, "cm"))
    return ((int)(n * 1000.0));
  if (units && !strcmp(units, "in"))
    return ((int)(n * 2540.0));
  else if (units && !strcmp(units, "mm"))
    return ((int)(n * 100.0));
  else if (units && !strcmp(units, "m"))
    return ((int)(n * 100000.0));
  else
    return ((int)n);
}


//
// 'pappl_main_usage_cb()' - Default usage callback.
//

void           // O - Exit status
pappl_main_usage_cb()
{
  help();

  exit(0);
}


//
// 'pappl_main_error_cb()' - Default error callback.
//

void           // O - Exit status
pappl_main_error_cb()
{
  help();

  exit(1);
}


//
// 'help()' - Show usage.
//

void help()
{
  printf("Usage: %s [subcommand] [options]\n", basename(_papplPath));
  printf("Options:\n");
  printf("    --help                   Show this menu.\n");
  printf("    --version                Show version.\n");
  printf("    --list-devices           List ALL devices.\n");
  printf("    --list-devices-dns-sd    List DNS-SD devices.\n");
  printf("    --list-devices-local     List LOCAL devices.\n");
  printf("    --list-devices-remote    List REMOTE devices.\n");
  printf("    --list-devices-usb       List USB devices.\n");
  printf("    --list-printers          List printer queues.\n");
  printf("    -A pam-service           Enable authentication using PAM service.\n");
  printf("    -a                       Cancel all jobs.\n");
  printf("    -c copies                Specify job copies.\n");
  printf("    -d printer               Specify printer name.\n");
  printf("    -h hostname              Set hostname.\n");
  printf("    -j job-id                Specify job id.\n");
  printf("    -L level                 Set the log level(fatal, error, warn, info, debug).\n");
  printf("    -l logfile               Set the log file.\n");
  printf("    -m driver                Specify driver.\n");
  printf("    -n system-name           Specify the system name.\n");
  printf("    -o name=value            Specify options.\n");
  printf("    -p port                  Set the listen port.\n");
  printf("    -s spool-directory       Set the spool directory.\n");
  printf("    -u printer-uri           Specify printer uri.\n");
  printf("    -v device-uri            Specify device uri.\n");
  printf("\n");
  printf("Sub commands:\n");
  printf("    --add                    Add printer.\n");
  printf("    --cancel                 Cancel job(s).\n");
  printf("    --default                Get/set the default printer.\n");
  printf("    --delete                 Delete printer.\n");
  printf("    --jobs                   List pending jobs.\n");
  printf("    --modify                 Modify printer.\n");
  printf("    --options                Show supported options.\n");
  printf("    --server                 Start a server.\n");
  printf("    --shutdown               Shutdown a server.\n");
  printf("    --status                 Show printer/server status.\n");
  printf("    --submit                 Submit job(s) for printing.\n");
}
