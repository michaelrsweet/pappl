//
// papplMainloop support functions for the Printer Application Framework
//
// Copyright © 2020-2024 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#  include "pappl-private.h"
#  if _WIN32
#  else
#    include <spawn.h>
#    include <libgen.h>
#  endif // _WIN32


//
// Globals...
//

char	*_papplMainloopPath = NULL;	// Path to self


//
// Local functions
//

static int	get_length(const char *value);


//
// '_papplMainloopAddOptions()' - Add default/job template attributes from options.
//

void
_papplMainloopAddOptions(
    ipp_t         *request,		// I - IPP request
    cups_len_t    num_options,		// I - Number of options
    cups_option_t *options,		// I - Options
    ipp_t         *supported)		// I - Supported attributes
{
  ipp_attribute_t *job_attrs;		// job-creation-attributes-supported
  int		is_default;		// Adding xxx-default attributes?
  ipp_tag_t	group_tag;		// Group to add to
  char		*end;			// End of value
  const char	*value;			// String value
  int		intvalue;		// Integer value
  const char	*media_left_offset = cupsGetOption("media-left-offset", num_options, options),
		*media_source = cupsGetOption("media-source", num_options, options),
                *media_top_offset = cupsGetOption("media-top-offset", num_options, options),
		*media_tracking = cupsGetOption("media-tracking", num_options, options),
		*media_type = cupsGetOption("media-type", num_options, options);
					// media-xxx member values


  // Determine what kind of options we are adding...
  group_tag  = ippGetOperation(request) == IPP_OP_PRINT_JOB ? IPP_TAG_JOB : IPP_TAG_PRINTER;
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
      cups_len_t num_values;		// Number of values
      char	*values[PAPPL_MAX_SOURCE],
					// Pointers to size strings
		*cvalue,		// Copied value
		*cptr;			// Pointer into copy

      num_values = 0;
      cvalue     = strdup(value);
      cptr       = cvalue;

#if _WIN32
      if ((values[num_values] = strtok_s(cvalue, ",", &cptr)) != NULL)
      {
        num_values ++;

        while (num_values < PAPPL_MAX_SOURCE && (values[num_values] = strtok_s(NULL, ",", &cptr)) != NULL)
          num_values ++;
      }
#else
      while (num_values < PAPPL_MAX_SOURCE && (values[num_values] = strsep(&cptr, ",")) != NULL)
        num_values ++;
#endif // _WIN32

      if (num_values > 0)
        ippAddStrings(request, IPP_TAG_PRINTER, IPP_TAG_KEYWORD, "media-ready", num_values, NULL, (const char * const *)values);

      free(cvalue);
    }

    if ((value = cupsGetOption("printer-darkness-configured", num_options, options)) != NULL)
    {
      if ((intvalue = (int)strtol(value, &end, 10)) >= 0 && intvalue <= 100 && errno != ERANGE && !*end)
        ippAddInteger(request, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "printer-darkness-configured", intvalue);
    }

    if ((value = cupsGetOption("printer-geo-location", num_options, options)) != NULL)
      ippAddString(request, IPP_TAG_PRINTER, IPP_TAG_URI, "printer-geo-location", NULL, value);

    if ((value = cupsGetOption("printer-location", num_options, options)) != NULL)
      ippAddString(request, IPP_TAG_PRINTER, IPP_TAG_TEXT, "printer-location", NULL, value);

    if ((value = cupsGetOption("printer-organization", num_options, options)) != NULL)
      ippAddString(request, IPP_TAG_PRINTER, IPP_TAG_TEXT, "printer-organization", NULL, value);

    if ((value = cupsGetOption("printer-organizational-unit", num_options, options)) != NULL)
      ippAddString(request, IPP_TAG_PRINTER, IPP_TAG_TEXT, "printer-organizational-unit", NULL, value);
  }
  else
  {
    if ((value = cupsGetOption("compression", num_options, options)) != NULL)
      ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_KEYWORD, "compression", NULL, value);

    if ((value = cupsGetOption("page-ranges", num_options, options)) != NULL)
    {
      int	first_page = 1,		// First page
		last_page = INT_MAX;	// Last page
      char	*valptr = (char *)value;// Pointer into value

      if (isdigit(*valptr & 255))
	first_page = (int)strtol(valptr, &valptr, 10);
      if (*valptr == '-')
	valptr ++;
      if (isdigit(*valptr & 255))
	last_page = (int)strtol(valptr, &valptr, 10);

      ippAddRange(request, IPP_TAG_JOB, "page-ranges", first_page, last_page);
    }
  }

  if ((value = cupsGetOption("copies", num_options, options)) == NULL)
    value = cupsGetOption("copies-default", num_options, options);
  if (value && (intvalue = (int)strtol(value, &end, 10)) >= 1 && intvalue <= 9999 && errno != ERANGE && !*end)
    ippAddInteger(request, group_tag, IPP_TAG_INTEGER, is_default ? "copies-default" : "copies", intvalue);

  if ((value = cupsGetOption("finishings", num_options, options)) == NULL)
    value = cupsGetOption("finishings-default", num_options, options);
  if (value)
  {
    // Get finishings enum values...
    cups_len_t	num_enumvalues = 0;	// Number of enum values
    int		enumvalues[32];		// Enum values
    char	keyword[128],		// Current keyword/value
		*kptr;			// Pointer into keyword/value

    while (*value && num_enumvalues < (cups_len_t)(sizeof(enumvalues) / sizeof(enumvalues[0])))
    {
      for (kptr = keyword; *value && *value != ','; value ++)
      {
        if (kptr < (keyword + sizeof(keyword) - 1))
          *kptr = *value;
      }

      *kptr = '\0';
      if (isdigit(keyword[0] & 255))
        enumvalues[num_enumvalues ++] = atoi(keyword);
      else if (keyword[0])
        enumvalues[num_enumvalues ++] = ippEnumValue("finishings", keyword);
    }

    if (num_enumvalues > 0)
      ippAddIntegers(request, group_tag, IPP_TAG_ENUM, is_default ? "finishings-default" : "finishings", num_enumvalues, enumvalues);
  }

  value = cupsGetOption("media", num_options, options);
  if (media_left_offset || media_source || media_top_offset || media_tracking || media_type)
  {
    // Add media-col
    ipp_t 	*media_col = ippNew();	// media-col value
    pwg_media_t *pwg = pwgMediaForPWG(value);
					// Size

    if (pwg)
    {
      ipp_t *media_size = ippNew();	// media-size value

      ippAddInteger(media_size, IPP_TAG_ZERO, IPP_TAG_INTEGER, "x-dimension", pwg->width);
      ippAddInteger(media_size, IPP_TAG_ZERO, IPP_TAG_INTEGER, "y-dimension", pwg->length);
      ippAddCollection(media_col, IPP_TAG_ZERO, "media-size", media_size);
      ippDelete(media_size);
    }

    if (media_left_offset)
      ippAddInteger(media_col, IPP_TAG_ZERO, IPP_TAG_INTEGER, "media-left-offset", get_length(media_left_offset));

    if (media_source)
      ippAddString(media_col, IPP_TAG_ZERO, IPP_TAG_KEYWORD, "media-source", NULL, media_source);

    if (media_top_offset)
      ippAddInteger(media_col, IPP_TAG_ZERO, IPP_TAG_INTEGER, "media-top-offset", get_length(media_top_offset));

    if (media_tracking)
      ippAddString(media_col, IPP_TAG_ZERO, IPP_TAG_KEYWORD, "media-tracking", NULL, media_tracking);

    if (media_type)
      ippAddString(media_col, IPP_TAG_ZERO, IPP_TAG_KEYWORD, "media-type", NULL, media_type);

    ippAddCollection(request, group_tag, is_default ? "media-col-default" : "media-col", media_col);
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
    else if ((intvalue = (int)strtol(value, &end, 10)) >= IPP_ORIENT_PORTRAIT && intvalue <= IPP_ORIENT_NONE && errno != ERANGE && !*end)
      ippAddInteger(request, group_tag, IPP_TAG_ENUM, is_default ? "orientation-requested-default" : "orientation-requested", intvalue);
  }

  if ((value = cupsGetOption("output-bin", num_options, options)) == NULL)
    value = cupsGetOption("output-bin-default", num_options, options);
  if (value)
    ippAddString(request, group_tag, IPP_TAG_KEYWORD, is_default ? "output-bin-default" : "output-bin", NULL, value);

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
  if (value && (intvalue = (int)strtol(value, &end, 10)) >= -100 && intvalue <= 100 && errno != ERANGE && !*end)
    ippAddInteger(request, group_tag, IPP_TAG_INTEGER, is_default ? "print-darkness-default" : "print-darkness", intvalue);

  if ((value = cupsGetOption("print-quality", num_options, options)) == NULL)
    value = cupsGetOption("print-quality-default", num_options, options);
  if (value)
  {
    if ((intvalue = ippEnumValue("print-quality", value)) != 0)
      ippAddInteger(request, group_tag, IPP_TAG_ENUM, is_default ? "print-quality-default" : "print-quality", intvalue);
    else if ((intvalue = (int)strtol(value, &end, 10)) >= IPP_QUALITY_DRAFT && intvalue <= IPP_QUALITY_HIGH && errno != ERANGE && !*end)
      ippAddInteger(request, group_tag, IPP_TAG_ENUM, is_default ? "print-quality-default" : "print-quality", intvalue);
  }

  if ((value = cupsGetOption("print-scaling", num_options, options)) == NULL)
    value = cupsGetOption("print-scaling-default", num_options, options);
  if (value)
    ippAddString(request, group_tag, IPP_TAG_KEYWORD, is_default ? "print-scaling-default" : "print-scaling", NULL, value);

  if ((value = cupsGetOption("print-speed", num_options, options)) == NULL)
    value = cupsGetOption("print-speed-default", num_options, options);
  if (value)
    ippAddInteger(request, group_tag, IPP_TAG_INTEGER, is_default ? "print-speed-default" : "print-speed", get_length(value));

  if ((value = cupsGetOption("printer-resolution", num_options, options)) == NULL)
    value = cupsGetOption("printer-resolution-default", num_options, options);

  if (value)
  {
    int		xres, yres;		// Resolution values
    char	units[32];		// Resolution units

    if (sscanf(value, "%dx%d%31s", &xres, &yres, units) != 3)
    {
      if (sscanf(value, "%d%31s", &xres, units) != 2)
      {
        xres = 300;

        papplCopyString(units, "dpi", sizeof(units));
      }

      yres = xres;
    }

    ippAddResolution(request, group_tag, is_default ? "printer-resolution-default" : "printer-resolution", !strcmp(units, "dpi") ? IPP_RES_PER_INCH : IPP_RES_PER_CM, xres, yres);
  }

  if ((value = cupsGetOption("sides", num_options, options)) == NULL)
    value = cupsGetOption("sides-default", num_options, options);
  if (value)
    ippAddString(request, group_tag, IPP_TAG_KEYWORD, is_default ? "sides-default" : "sides", NULL, value);

  // Vendor attributes/options
  if ((job_attrs = ippFindAttribute(supported, "job-creation-attributes-supported", IPP_TAG_KEYWORD)) != NULL)
  {
    cups_len_t	i,			// Looping var
		count;			// Count
    const char	*name;			// Attribute name
    char	defname[128],		// xxx-default name
		supname[128];		// xxx-supported name
    ipp_attribute_t *attr;		// Attribute

    for (i = 0, count = ippGetCount(job_attrs); i < count; i ++)
    {
      name = ippGetString(job_attrs, i, NULL);

      snprintf(defname, sizeof(defname), "%s-default", name);
      snprintf(supname, sizeof(supname), "%s-supported", name);

      if ((value = cupsGetOption(name, num_options, options)) == NULL)
        value = cupsGetOption(defname, num_options, options);

      if (!value)
        continue;

      if (!strcmp(name, "copies") || !strcmp(name, "finishings") || !strcmp(name, "media") || !strcmp(name, "multiple-document-handling") || !strcmp(name, "orientation-requested") || !strcmp(name, "output-bin") || !strcmp(name, "print-color-mode") || !strcmp(name, "print-content-optimize") || !strcmp(name, "print-darkness") || !strcmp(name, "print-quality") || !strcmp(name, "print-scaling") || !strcmp(name, "print-speed") || !strcmp(name, "printer-resolution") || !strcmp(name, "print-speed") || !strcmp(name, "sides"))
        continue;

      if ((attr = ippFindAttribute(supported, supname, IPP_TAG_ZERO)) != NULL)
      {
	switch (ippGetValueTag(attr))
	{
	  case IPP_TAG_BOOLEAN :
	      ippAddBoolean(request, group_tag, is_default ? defname : name, !strcmp(value, "true"));
	      break;

	  case IPP_TAG_INTEGER :
	  case IPP_TAG_RANGE :
	      intvalue = (int)strtol(value, &end, 10);
	      if (errno != ERANGE && !*end)
	        ippAddInteger(request, group_tag, IPP_TAG_INTEGER, is_default ? defname : name, intvalue);
	      break;

	  case IPP_TAG_KEYWORD :
	      ippAddString(request, group_tag, IPP_TAG_KEYWORD, is_default ? defname : name, NULL, value);
	      break;

	  default :
	      break;
	}
      }
      else
      {
	ippAddString(request, group_tag, IPP_TAG_TEXT, is_default ? defname : name, NULL, value);
      }
    }
  }
}


//
// '_papplMainloopAddPrinterURI()' - Add the printer-uri attribute and return a
//                                   resource path.
//

void
_papplMainloopAddPrinterURI(
    ipp_t      *request,		// I - IPP request
    const char *printer_name,		// I - Printer name
    char       *resource,		// I - Resource path buffer
    size_t     rsize)			// I - Size of buffer
{
  char	uri[1024],			// printer-uri value
	*resptr;			// Pointer into resource path


  snprintf(resource, rsize, "/ipp/print/%s", printer_name);
  for (resptr = resource + 11; *resptr; resptr ++)
  {
    if ((*resptr & 255) <= ' ' || strchr("\177/\\\'\"?#", *resptr))
      *resptr = '_';
  }

  // Eliminate duplicate and trailing underscores...
  resptr = resource + 11;
  while (*resptr)
  {
    if (resptr[0] == '_' && resptr[1] == '_')
      memmove(resptr, resptr + 1, strlen(resptr));
					// Duplicate underscores
    else if (resptr[0] == '_' && !resptr[1])
      *resptr = '\0';			// Trailing underscore
    else
      resptr ++;
  }

  httpAssembleURI(HTTP_URI_CODING_ALL, uri, sizeof(uri), "ipp", NULL, "localhost", 0, resource);

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri", NULL, uri);
}


// '_papplMainloopAddScannerURI()' - Add the scanner-uri attribute and return a
//                                   resource path.
//

int
_papplMainloopAddScannerURI(
    http_t     *request,        // I - HTTP request
    const char *scanner_name,    // I - Scanner name
    char       *resource,        // I - Resource path buffer
    size_t     rsize)            // I - Size of buffer
{
    char    uri[1024];           // scanner-uri value
    char    *resptr;             // Pointer into resource path
    int     ret;

    // Construct the initial resource path for the scanner
    ret = snprintf(resource, rsize, "/escl/scan/%s", scanner_name);
    if (ret < 0 || (size_t)ret >= rsize) {
        fprintf(stderr, "Error: Resource path buffer too small.\n");
        return -1;
    }

    // Sanitize the resource path by replacing invalid characters with '_'
    for (resptr = resource + strlen("/escl/scan/"); *resptr; resptr++) {
        if ((*resptr & 255) <= ' ' || strchr("\177/\\\'\"?#", *resptr)) {
            *resptr = '_';
        }
    }

    // Eliminate duplicate and trailing underscores
    resptr = resource + strlen("/escl/scan/");
    while (*resptr) {
        if (resptr[0] == '_' && resptr[1] == '_') {
            memmove(resptr, resptr + 1, strlen(resptr));
            // Duplicate underscores removed, do not advance resptr
        }
        else if (resptr[0] == '_' && !resptr[1]) {
            *resptr = '\0';  // Trailing underscore removed
            break;
        }
        else {
            resptr++;
        }
    }

    // Assemble the full URI using HTTP
    ret = httpAssembleURI(HTTP_URI_CODING_ALL, uri, sizeof(uri), "http", NULL, "localhost", 0, resource);

    if (ret < 0) {
        fprintf(stderr, "Error: Failed to assemble URI.\n");
        return -1;
    }

    httpSetField(request, HTTP_FIELD_CONTENT_TYPE, uri);
    return 0;
}

//
// '_papplMainloopConnect()' - Connect to the local server.
//

http_t *				// O - HTTP connection
_papplMainloopConnect(
    const char *base_name,		// I - Printer application name
    bool       auto_start)		// I - `true` to start server if not running
{
  http_t	*http;			// HTTP connection
  char		sockname[1024];		// Socket filename


  // See if the server is running...
  http = httpConnect(_papplMainloopGetServerPath(base_name, getuid(), sockname, sizeof(sockname)), _papplMainloopGetServerPort(base_name), NULL, AF_UNSPEC, HTTP_ENCRYPTION_IF_REQUESTED, 1, 30000, NULL);

#if !_WIN32
  if (!http && getuid())
  {
    // Try root server...
    http = httpConnect(_papplMainloopGetServerPath(base_name, 0, sockname, sizeof(sockname)), _papplMainloopGetServerPort(base_name), NULL, AF_UNSPEC, HTTP_ENCRYPTION_IF_REQUESTED, 1, 30000, NULL);
  }
#endif // !_WIN32

  if (!http && auto_start)
  {
    // Nope, start it now...
    int		tries;			// Number of retries
#if !_WIN32
    pid_t	server_pid;		// Server process ID
    posix_spawnattr_t server_attrs;	// Server process attributes
#endif // !_WIN32
    char * const server_argv[] =	// Server command-line
    {
      _papplMainloopPath,
      "server",
      "-o",
      "private-server=true",
      NULL
    };

#if _WIN32
  int status = (int)_spawnvpe(_P_NOWAIT, _papplMainloopPath, server_argv, environ);

  if (status != 0)
  {
    char status_str[32];		// Status string

    snprintf(status_str, sizeof(status_str), "%d", status);
    _papplLocPrintf(stderr, _PAPPL_LOC("%s: Unable to start server: %s"), base_name, status_str);
    return (NULL);
  }

#else
    posix_spawnattr_init(&server_attrs);
    posix_spawnattr_setpgroup(&server_attrs, 0);

    if (posix_spawn(&server_pid, _papplMainloopPath, NULL, &server_attrs, server_argv, environ))
    {
      _papplLocPrintf(stderr, _PAPPL_LOC("%s: Unable to start server: %s"), base_name, strerror(errno));
      posix_spawnattr_destroy(&server_attrs);
      return (NULL);
    }

    posix_spawnattr_destroy(&server_attrs);
#endif // _WIN32

    // Wait for it to start...
    _papplMainloopGetServerPath(base_name, getuid(), sockname, sizeof(sockname));

    for (tries = 0; tries < 40; tries ++)
    {
      usleep(250000);

      if ((http = httpConnect(_papplMainloopGetServerPath(base_name, getuid(), sockname, sizeof(sockname)), _papplMainloopGetServerPort(base_name), NULL, AF_UNSPEC, HTTP_ENCRYPTION_IF_REQUESTED, 1, 30000, NULL)) != NULL)
        break;
    }

    if (!http)
      _papplLocPrintf(stderr, _PAPPL_LOC("%s: Unable to connect to server: %s"), base_name, cupsGetErrorString());
  }

  return (http);
}


//
// '_papplMainloopConnectURI()' - Connect to an IPP printer directly.
//

http_t *				// O - HTTP connection or `NULL` on error
_papplMainloopConnectURI(
    const char *base_name,		// I - Base Name
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
  if (httpSeparateURI(HTTP_URI_CODING_ALL, printer_uri, scheme, sizeof(scheme), userpass, sizeof(userpass), hostname, sizeof(hostname), &port, resource, (cups_len_t)rsize) < HTTP_URI_STATUS_OK)
  {
    _papplLocPrintf(stderr, _PAPPL_LOC("%s: Bad printer URI '%s'."), base_name, printer_uri);
    return (NULL);
  }

  if (strcmp(scheme, "ipp") && strcmp(scheme, "ipps"))
  {
    _papplLocPrintf(stderr, _PAPPL_LOC("%s: Unsupported URI scheme '%s'."), base_name, scheme);
    return (NULL);
  }

  if (userpass[0])
    _papplLocPrintf(stderr, _PAPPL_LOC("%s: Warning - user credentials are not supported in URIs."), base_name);

  if (!strcmp(scheme, "ipps") || port == 443)
    encryption = HTTP_ENCRYPTION_ALWAYS;
  else
    encryption = HTTP_ENCRYPTION_IF_REQUESTED;

  if ((http = httpConnect(hostname, port, NULL, AF_UNSPEC, encryption, 1, 30000, NULL)) == NULL)
    _papplLocPrintf(stderr, _PAPPL_LOC("%s: Unable to connect to printer at '%s:%d': %s"), base_name, hostname, port, cupsGetErrorString());

  return (http);
}


//
// '_papplMainloopGetDefaultPrinter' - Get the default printer.
//

char *					// O - Default printer or `NULL` for none
_papplMainloopGetDefaultPrinter(
    http_t *http,			// I - HTTP connection
    char   *buffer,			// I - Buffer for printer name
    size_t bufsize)			// I - Size of buffer
{
  ipp_t		*request,		// IPP request
		*response;		// IPP response
  const char	*printer_name;		// Printer name


  // Ask the server for its default printer
  request = ippNewRequest(IPP_OP_CUPS_GET_DEFAULT);
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name", NULL, cupsGetUser());
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_KEYWORD, "requested-attributes", NULL, "printer-name");

  response = cupsDoRequest(http, request, "/ipp/system");

  if ((printer_name = ippGetString(ippFindAttribute(response, "printer-name", IPP_TAG_NAME), 0, NULL)) != NULL)
    papplCopyString(buffer, printer_name, bufsize);
  else
    *buffer = '\0';

  ippDelete(response);

  return (*buffer ? buffer : NULL);
}


//
// '_papplMainloopGetServerPath()' - Get the UNIX domain socket for the server.
//

char *					// O - Socket filename
_papplMainloopGetServerPath(
    const char *base_name,		// I - Base name
    uid_t      uid,			// I - UID for server
    char       *buffer,			// I - Buffer for filename
    size_t     bufsize)			// I - Size of buffer
{
#if _WIN32
  // Server running as local service...
  (void)base_name;
  (void)uid;

  papplCopyString(buffer, "localhost", bufsize);

#else
  const char	*snap_common;		// SNAP_COMMON environment variable


  if (uid)
  {
    // Per-user server...
    snprintf(buffer, bufsize, "%s/%s%d.sock", papplGetTempDir(), base_name, (int)uid);
  }
  else if ((snap_common = getenv("SNAP_COMMON")) != NULL)
  {
    // System server running as root inside a snap (https://snapcraft.io)...
    snprintf(buffer, bufsize, "%s/%s.sock", snap_common, base_name);
  }
  else
  {
    // System server running as root
    snprintf(buffer, bufsize, PAPPL_SOCKDIR "/%s.sock", base_name);
  }

  _PAPPL_DEBUG("Using domain socket '%s'.\n", buffer);
#endif // _WIN32

  return (buffer);
}


//
// '_papplMainloopGetServerPort()' - Get the socket port number for the server.
//

int					// O - Port number
_papplMainloopGetServerPort(
    const char *base_name)		// I - Base name of printer application
{
#if _WIN32
  char		path[1024];		// Registry path
  HKEY		key;			// Registry key
  DWORD		dport = 0;		// Port number value for registry


  // The server's port number is saved in SOFTWARE\appname\port
  snprintf(path, sizeof(path), "SOFTWARE\\%s", base_name);

  if (!RegOpenKeyExA(HKEY_LOCAL_MACHINE, path, 0, KEY_READ, &key))
  {
    // Was able to open the registry, get the port number...
    DWORD dsize = sizeof(dport);	// Size of port number value

    RegGetValueA(key, NULL, "port", RRF_RT_REG_DWORD, NULL, &dport, &dsize);
    RegCloseKey(key);
  }

  return ((int)dport);

#else
  // POSIX platforms use domain instead of TCP/IP sockets...
  (void)base_name;

  return (0);
#endif // _WIN32
}



//
// 'get_length()' - Get a length in hundredths of millimeters.
//

static int				// O - Length value
get_length(const char *value)		// I - Length string
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
