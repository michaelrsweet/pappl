//
// Scanner object for the Scanner Application Framework
//
// Copyright © 2019-2021 by Michael R Sweet.
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

static int	compare_active_jobs(pappl_job_t *a, pappl_job_t *b);
static int	compare_all_jobs(pappl_job_t *a, pappl_job_t *b);
static int	compare_completed_jobs(pappl_job_t *a, pappl_job_t *b);


//
// 'papplScannerCancelAllJobs()' - Cancel all jobs on the scanner.
//
// This function cancels all jobs on the scanner.  If any job is currently being
// scaned, it will be stopped at a convenient time (usually the end of a page)
// so that the scanner will be left in a known state.
//

void
papplScannerCancelAllJobs(
    pappl_scanner_t *scanner)		// I - Scanner
{
  pappl_job_t	*job;			// Job information


  // Loop through all jobs and cancel them.
  //
  // Since we have a writer lock, it is safe to use cupsArrayFirst/Last...
  pthread_rwlock_wrlock(&scanner->rwlock);

  for (job = (pappl_job_t *)cupsArrayFirst(scanner->active_jobs); job; job = (pappl_job_t *)cupsArrayNext(scanner->active_jobs))
  {
    // Cancel this job...
    if (job->state == IPP_JSTATE_PROCESSING || (job->state == IPP_JSTATE_HELD && job->fd >= 0))
    {
      job->is_canceled = true;
    }
    else
    {
      job->state     = IPP_JSTATE_CANCELED;
      job->completed = time(NULL);

      _papplJobRemoveFile(job);

      cupsArrayRemove(scanner->active_jobs, job);
      cupsArrayAdd(scanner->completed_jobs, job);
    }
  }

  pthread_rwlock_unlock(&scanner->rwlock);

  if (!scanner->system->clean_time)
    scanner->system->clean_time = time(NULL) + 60;
}


//
// 'papplScannerCreate()' - Create a new scanner.
//
// This function creates a new scanner (service) on the specified system.  The
// "type" argument specifies the type of service to create and must currently
// be the value `PAPPL_SERVICE_TYPE_SCAN`.
//
// The "printer_id" argument specifies a positive integer identifier that is
// unique to the system.  If you specify a value of `0` a new identifier will
// be assigned.
//
// The "driver_name" argument specifies a named driver for the scanner, from
// the list of drivers registered with the @link papplSystemSetScannerDrivers@
// function.
//
// The "device_id" and "device_uri" arguments specify the IEEE-1284 device ID
// and device URI strings for the scanner.
//
// On error, this function sets the `errno` variable to one of the following
// values:
//
// - `EEXIST`: A scanner with the specified name already exists.
// - `EINVAL`: Bad values for the arguments were specified.
// - `EIO`: The driver callback failed.
// - `ENOENT`: No driver callback has been set.
// - `ENOMEM`: Ran out of memory.
//


pappl_scanner_t *			// O - Scanner or `NULL` on error
papplScannerCreate(
    pappl_system_t       *system,	// I - System
    int                  printer_id,	// I - printer-id value or `0` for new
    const char           *scanner_name,	// I - Human-readable scanner name
    const char           *driver_name,	// I - Driver name
    const char           *device_id,	// I - IEEE-1284 device ID
    const char           *device_uri)	// I - Device URI
{
  pappl_scanner_t	*scanner;	// Scanner
  char			resource[1024],	// Resource path
			*resptr,	// Pointer into resource path
			uuid[128],	// scanner-uuid
			scan_group[65];// scan-group value
  int			k_supported;	// Maximum file size supported
  struct statfs		spoolinfo;	// FS info for spool directory
  double		spoolsize;	// FS size
  char			path[256];	// Path to resource
  pappl_sc_driver_data_t driver_data;	// Driver data
  ipp_t			*driver_attrs;	// Driver attributes
  static const char * const ipp_versions[] =
  {					// ipp-versions-supported values
    "1.1",
    "2.0"
  };
  static const int	operations[] =	// operations-supported values
  {
    IPP_OP_SCAN_JOB,
    IPP_OP_VALIDATE_JOB,
    IPP_OP_CREATE_JOB,
    IPP_OP_SEND_DOCUMENT,
    IPP_OP_CANCEL_JOB,
    IPP_OP_GET_JOB_ATTRIBUTES,
    IPP_OP_GET_JOBS,
    IPP_OP_GET_PRINTER_ATTRIBUTES,
    IPP_OP_PAUSE_PRINTER,
    IPP_OP_RESUME_PRINTER,
    IPP_OP_SET_PRINTER_ATTRIBUTES,
    IPP_OP_CANCEL_MY_JOBS,
    IPP_OP_CLOSE_JOB,
  };
  static const char * const charset[] =	// charset-supported values
  {
    "us-ascii",
    "utf-8"
  };
  static const char * const compression[] =
  {					// compression-supported values
    "deflate",
    "gzip",
    "none"
  };
  static const char * const multiple_document_handling[] =
  {					// multiple-document-handling-supported values
    "separate-documents-uncollated-copies",
    "separate-documents-collated-copies"
  };
  static const int orientation_requested[] =
  {
    IPP_ORIENT_PORTRAIT,
    IPP_ORIENT_LANDSCAPE,
    IPP_ORIENT_REVERSE_LANDSCAPE,
    IPP_ORIENT_REVERSE_PORTRAIT,
    IPP_ORIENT_NONE
  };
  static const int scan_quality[] =	// scan-quality-supported
  {
    IPP_QUALITY_DRAFT,
    IPP_QUALITY_NORMAL,
    IPP_QUALITY_HIGH
  };
  static const char * const scan_scaling[] =
  {					// scan-scaling-supported
    "auto",
    "auto-fit",
    "fill",
    "fit",
    "none"
  };
  static const char * const uri_security[] =
  {					// uri-security-supported values
    "none",
    "tls"
  };
  static const char * const which_jobs[] =
  {					// which-jobs-supported values
    "completed",
    "not-completed",
    "all"
  };


  // Range check input...
  if (!system || !scanner_name || !driver_name || !device_uri)
  {
    errno = EINVAL;
    return (NULL);
  }

  if (!system->driver_cb)
  {
    papplLog(system, PAPPL_LOGLEVEL_ERROR, "No driver callback set, unable to add scanner.");
    errno = ENOENT;
    return (NULL);
  }

  // Prepare URI values for the scanner attributes...
  if (system->options & PAPPL_SOPTIONS_MULTI_QUEUE)
  {
    // Make sure scanner names that start with a digit have a resource path
    // containing an underscore...
    if (isdigit(*scanner_name & 255))
      snprintf(resource, sizeof(resource), "/ipp/scan/_%s", scanner_name);
    else
      snprintf(resource, sizeof(resource), "/ipp/scan/%s", scanner_name);

    // Convert URL reserved characters to underscore...
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
  }
  else
    strlcpy(resource, "/ipp/scan", sizeof(resource));

  // Make sure the scanner doesn't already exist...
  if ((scanner = papplSystemFindScanner(system, resource, 0, NULL)) != NULL)
  {
    int		n;		// Current instance number
    char	temp[1024];	// Temporary resource path

    if (!strcmp(scanner_name, scanner->name))
    {
      papplLog(system, PAPPL_LOGLEVEL_ERROR, "Scanner '%s' already exists.", scanner_name);
      errno = EEXIST;
      return (NULL);
    }

    for (n = 2; n < 10; n ++)
    {
      snprintf(temp, sizeof(temp), "%s_%d", resource, n);
      if (!papplSystemFindScanner(system, temp, 0, NULL))
        break;
    }

    if (n >= 10)
    {
      papplLog(system, PAPPL_LOGLEVEL_ERROR, "Scanner '%s' name conflicts with existing scanner.", scanner_name);
      errno = EEXIST;
      return (NULL);
    }

    strlcpy(resource, temp, sizeof(resource));
  }

  // Allocate memory for the scanner...
  if ((scanner = calloc(1, sizeof(pappl_scanner_t))) == NULL)
  {
    papplLog(system, PAPPL_LOGLEVEL_ERROR, "Unable to allocate memory for scanner: %s", strerror(errno));
    return (NULL);
  }

  papplLog(system, PAPPL_LOGLEVEL_INFO, "Scanner '%s' at resource path '%s'.", scanner_name, resource);

  _papplSystemMakeUUID(system, scanner_name, 0, uuid, sizeof(uuid));

  // Get the maximum spool size based on the size of the filesystem used for
  // the spool directory.  If the host OS doesn't support the statfs call
  // or the filesystem is larger than 2TiB, always report INT_MAX.
  if (statfs(system->directory, &spoolinfo))
    k_supported = INT_MAX;
  else if ((spoolsize = (double)spoolinfo.f_bsize * spoolinfo.f_blocks / 1024) > INT_MAX)
    k_supported = INT_MAX;
  else
    k_supported = (int)spoolsize;

  // Initialize scanner structure and attributes...
  pthread_rwlock_init(&scanner->rwlock, NULL);

  scanner->system             = system;
  scanner->name               = strdup(scanner_name);
  scanner->dns_sd_name        = strdup(scanner_name);
  scanner->resource           = strdup(resource);
  scanner->resourcelen        = strlen(resource);
  scanner->uriname            = scanner->resource + 10; // Skip "/ipp/scan" in resource
  scanner->device_id          = device_id ? strdup(device_id) : NULL;
  scanner->device_uri         = strdup(device_uri);
  scanner->driver_name        = strdup(driver_name);
  scanner->attrs              = ippNew();
  scanner->start_time         = time(NULL);
  scanner->config_time        = scanner->start_time;
  scanner->state              = IPP_PSTATE_IDLE;
  scanner->state_reasons      = PAPPL_PREASON_NONE;
  scanner->state_time         = scanner->start_time;
  scanner->all_jobs           = cupsArrayNew3((cups_array_func_t)compare_all_jobs, NULL, NULL, 0, NULL, (cups_afree_func_t)_papplJobDelete);
  scanner->active_jobs        = cupsArrayNew((cups_array_func_t)compare_active_jobs, NULL);
  scanner->completed_jobs     = cupsArrayNew((cups_array_func_t)compare_completed_jobs, NULL);
  scanner->next_job_id        = 1;
  scanner->max_active_jobs    = (system->options & PAPPL_SOPTIONS_MULTI_QUEUE) ? 0 : 1;
  scanner->max_completed_jobs = 100;
  
  if (!scanner->name || !scanner->dns_sd_name || !scanner->resource || (device_id && !scanner->device_id) || !scanner->device_uri || !scanner->driver_name || !scanner->attrs)
  {
    // Failed to allocate one of the required members...
    _papplScannerDelete(scanner);
    return (NULL);
  }

  if (papplSystemGetDefaultScanGroup(system, scan_group, sizeof(scan_group)))
    papplScannerSetScanGroup(scanner, scan_group);

  // If the driver is "auto", figure out the proper driver name...
  if (!strcmp(driver_name, "auto") && system->autoadd_cb)
  {
    // If device_id is NULL, try to look it up...
    if (!scanner->device_id && strncmp(device_uri, "file://", 7))
    {
      pappl_device_t	*device;	// Connection to scanner

      if ((device = papplDeviceOpen(device_uri, "auto", papplLogDevice, system)) != NULL)
      {
        char	new_id[1024];		// New 1284 device ID

        if (papplDeviceGetID(device, new_id, sizeof(new_id)))
          scanner->device_id = strdup(new_id);

        papplDeviceClose(device);
      }
    }

    if ((driver_name = (system->autoadd_cb)(scanner_name, device_uri, scanner->device_id, system->driver_cbdata)) == NULL)
    {
      errno = EIO;
      _papplScannerDelete(scanner);
      return (NULL);
    }
  }

  // Initialize driver...
  driver_attrs = NULL;
  _papplScannerInitDriverData(&driver_data);

  if (!(system->driver_cb)(system, driver_name, device_uri, device_id, &driver_data, &driver_attrs, system->driver_cbdata))
  {
    errno = EIO;
    _papplScannerDelete(scanner);
    return (NULL);
  }

  papplScannerSetDriverData(scanner, &driver_data, driver_attrs);
  ippDelete(driver_attrs);

  // Generate scanner-device-id value as needed...
  if (!scanner->device_id)
  {
    char	temp_id[400],		// Temporary "scanner-device-id" string
		mfg[128],		// Manufacturer name
		*mdl,			// Model name
		cmd[128],		// Command (format) list
		*ptr;			// Pointer into string
    ipp_attribute_t *formats;		// "document-format-supported" attribute
    int		i,			// Looping var
		count;			// Number of values

    // Assume make and model are separated by a space...
    strlcpy(mfg, driver_data.make_and_model, sizeof(mfg));
    if ((mdl = strchr(mfg, ' ')) != NULL)
      *mdl++ = '\0';			// Nul-terminate the make
    else
      mdl = mfg;			// No separator, so assume the make and model are the same

    formats = ippFindAttribute(scanner->driver_attrs, "document-format-supported", IPP_TAG_MIMETYPE);
    count   = ippGetCount(formats);
    for (i = 0, ptr = cmd; i < count; i ++)
    {
      const char *format = ippGetString(formats, i, NULL);
					// Current MIME media type

      if (!strcmp(format, "application/pdf"))
        format = "PDF";
      else if (!strcmp(format, "application/postscript"))
        format = "PS";
      else if (!strcmp(format, "application/vnd.hp-postscript"))
        format = "PCL";
      else if (!strcmp(format, "application/vnd.zebra-zpl"))
        format = "ZPL";
      else if (!strcmp(format, "image/jpeg"))
        format = "JPEG";
      else if (!strcmp(format, "image/png"))
        format = "PNG";
      else if (!strcmp(format, "image/pwg-raster"))
        format = "PWG";
      else if (!strcmp(format, "image/urf"))
        format = "URF";
      else if (!strcmp(format, "text/plain"))
        format = "TXT";
      else if (!strcmp(format, "application/octet-stream"))
        continue;

      if (ptr > cmd)
        snprintf(ptr, sizeof(cmd) - (size_t)(ptr - cmd), ",%s", format);
      else
        strlcpy(cmd, format, sizeof(cmd));

      ptr += strlen(ptr);
    }

    *ptr = '\0';

    snprintf(temp_id, sizeof(temp_id), "MFG:%s;MDL:%s;CMD:%s;", mfg, mdl, cmd);
    if ((scanner->device_id = strdup(temp_id)) == NULL)
    {
      _papplScannerDelete(scanner);
      return (NULL);
    }
  }

  // charset-configured
  ippAddString(scanner->attrs, IPP_TAG_SCANNER, IPP_CONST_TAG(IPP_TAG_CHARSET), "charset-configured", NULL, "utf-8");

  // charset-supported
  ippAddStrings(scanner->attrs, IPP_TAG_SCANNER, IPP_CONST_TAG(IPP_TAG_CHARSET), "charset-supported", sizeof(charset) / sizeof(charset[0]), NULL, charset);

  // compression-supported
  ippAddStrings(scanner->attrs, IPP_TAG_SCANNER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "compression-supported", (int)(sizeof(compression) / sizeof(compression[0])), NULL, compression);

  // copies-default
  ippAddInteger(scanner->attrs, IPP_TAG_SCANNER, IPP_TAG_INTEGER, "copies-default", 1);

  // copies-supported
  ippAddInteger(scanner->attrs, IPP_TAG_SCANNER, IPP_TAG_INTEGER, "copies-supported", 1);

  // document-format-default
  ippAddString(scanner->attrs, IPP_TAG_SCANNER, IPP_CONST_TAG(IPP_TAG_MIMETYPE), "document-format-default", NULL, NULL);

  // generated-natural-language-supported
  ippAddString(scanner->attrs, IPP_TAG_SCANNER, IPP_CONST_TAG(IPP_TAG_LANGUAGE), "generated-natural-language-supported", NULL, "en");

  // ipp-versions-supported
  ippAddStrings(scanner->attrs, IPP_TAG_SCANNER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "ipp-versions-supported", (int)(sizeof(ipp_versions) / sizeof(ipp_versions[0])), NULL, ipp_versions);

  // job-ids-supported
  ippAddBoolean(scanner->attrs, IPP_TAG_SCANNER, "job-ids-supported", 1);

  // multiple-document-handling-supported
  ippAddStrings(scanner->attrs, IPP_TAG_SCANNER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "multiple-document-handling-supported", sizeof(multiple_document_handling) / sizeof(multiple_document_handling[0]), NULL, multiple_document_handling);

  // multiple-document-jobs-supported
  ippAddBoolean(scanner->attrs, IPP_TAG_SCANNER, "multiple-document-jobs-supported", 0);

  // multiple-operation-time-out
  ippAddInteger(scanner->attrs, IPP_TAG_SCANNER, IPP_TAG_INTEGER, "multiple-operation-time-out", 60);

  // multiple-operation-time-out-action
  ippAddString(scanner->attrs, IPP_TAG_SCANNER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "multiple-operation-time-out-action", NULL, "abort-job");

  // natural-language-configured
  ippAddString(scanner->attrs, IPP_TAG_SCANNER, IPP_CONST_TAG(IPP_TAG_LANGUAGE), "natural-language-configured", NULL, "en");

  // operations-supported
  ippAddIntegers(scanner->attrs, IPP_TAG_SCANNER, IPP_TAG_ENUM, "operations-supported", (int)(sizeof(operations) / sizeof(operations[0])), operations);

  // input-orientation-requested-supported
  ippAddIntegers(scanner->attrs, IPP_TAG_SCANNER, IPP_TAG_ENUM, "input-orientation-requested-supported", (int)(sizeof(orientation_requested) / sizeof(orientation_requested[0])), orientation_requested);

  // input-quality-supported
  ippAddIntegers(scanner->attrs, IPP_TAG_SCANNER, IPP_TAG_ENUM, "input-quality-supported", (int)(sizeof(scan_quality) / sizeof(scan_quality[0])), scan_quality);

  // printer-device-id
  if (scanner->device_id)
    ippAddString(scanner->attrs, IPP_TAG_SCANNER, IPP_TAG_TEXT, "printer-device-id", NULL, scanner->device_id);

  // printer-get-attributes-supported
  ippAddString(scanner->attrs, IPP_TAG_SCANNER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "printer-get-attributes-supported", NULL, "document-format");

  // printer-info
  ippAddString(scanner->attrs, IPP_TAG_SCANNER, IPP_TAG_TEXT, "printer-info", NULL, scanner_name);

  // printer-name
  ippAddString(scanner->attrs, IPP_TAG_SCANNER, IPP_TAG_NAME, "printer-name", NULL, scanner_name);

  // printer-uuid
  ippAddString(scanner->attrs, IPP_TAG_SCANNER, IPP_TAG_URI, "printer-uuid", NULL, uuid);

  // xri-security-supported
  if (system->options & PAPPL_SOPTIONS_NO_TLS)
    ippAddString(scanner->attrs, IPP_TAG_SCANNER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "xri-security-supported", NULL, "none");
  else if (papplSystemGetTLSOnly(system))
    ippAddString(scanner->attrs, IPP_TAG_SCANNER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "xri-security-supported", NULL, "tls");
  else
    ippAddStrings(scanner->attrs, IPP_TAG_SCANNER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "xri-security-supported", 2, NULL, xri_security);

  // which-jobs-supported
  ippAddStrings(scanner->attrs, IPP_TAG_SCANNER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "which-jobs-supported", sizeof(which_jobs) / sizeof(which_jobs[0]), NULL, which_jobs);

  // Add the scanner to the system...
  _papplSystemAddScanner(system, scanner, printer_id);

  // Do any post-creation work...
  if (system->create_cb)
    (system->create_cb)(scanner, system->driver_cbdata);

  // Add icons...
  _papplSystemAddScannerIcons(system, scanner);

  // Add web pages, if any...
  if (system->options & PAPPL_SOPTIONS_WEB_INTERFACE)
  {
    snprintf(path, sizeof(path), "%s/", scanner->uriname);
    papplSystemAddResourceCallback(system, path, "text/html", (pappl_resource_cb_t)_papplScannerWebHome, scanner);

    snprintf(path, sizeof(path), "%s/cancel", scanner->uriname);
    papplSystemAddResourceCallback(system, path, "text/html", (pappl_resource_cb_t)_papplScannerWebCancelJob, scanner);

    snprintf(path, sizeof(path), "%s/cancelall", scanner->uriname);
    papplSystemAddResourceCallback(system, path, "text/html", (pappl_resource_cb_t)_papplScannerWebCancelAllJobs, scanner);

    if (system->options & PAPPL_SOPTIONS_MULTI_QUEUE)
    {
      snprintf(path, sizeof(path), "%s/delete", scanner->uriname);
      papplSystemAddResourceCallback(system, path, "text/html", (pappl_resource_cb_t)_papplScannerWebDelete, scanner);
    }

    snprintf(path, sizeof(path), "%s/config", scanner->uriname);
    papplSystemAddResourceCallback(system, path, "text/html", (pappl_resource_cb_t)_papplScannerWebConfig, scanner);

    snprintf(path, sizeof(path), "%s/jobs", scanner->uriname);
    papplSystemAddResourceCallback(system, path, "text/html", (pappl_resource_cb_t)_papplScannerWebJobs, scanner);

    snprintf(path, sizeof(path), "%s/media", scanner->uriname);
    papplSystemAddResourceCallback(system, path, "text/html", (pappl_resource_cb_t)_papplScannerWebMedia, scanner);
    papplScannerAddLink(scanner, "Media", path, PAPPL_LOPTIONS_NAVIGATION | PAPPL_LOPTIONS_STATUS);

    snprintf(path, sizeof(path), "%s/scaning", scanner->uriname);
    papplSystemAddResourceCallback(system, path, "text/html", (pappl_resource_cb_t)_papplScannerWebDefaults, scanner);
    papplScannerAddLink(scanner, "Scaning Defaults", path, PAPPL_LOPTIONS_NAVIGATION | PAPPL_LOPTIONS_STATUS);

  _papplSystemConfigChanged(system);

  // Return it!
  return (scanner);
}


//
// '_papplScannerDelete()' - Free memory associated with a scanner.
//

void
_papplScannerDelete(
    pappl_scanner_t *scanner)		// I - Scanner
{
  int			i;		// Looping var
  _pappl_resource_t	*r;		// Current resource
  char			prefix[1024];	// Prefix for scanner resources
  size_t		prefixlen;	// Length of prefix

  // Remove DNS-SD registrations...
  _papplScannerUnregisterDNSSDNoLock(scanner);

  // Remove scanner-specific resources...
  snprintf(prefix, sizeof(prefix), "%s/", scanner->uriname);
  prefixlen = strlen(prefix);

  // Note: System writer lock is already held when calling cupsArrayRemove
  // for the system's scanner object, so we don't need a separate lock here
  // and can safely use cupsArrayFirst/Next...
  for (r = (_pappl_resource_t *)cupsArrayFirst(scanner->system->resources); r; r = (_pappl_resource_t *)cupsArrayNext(scanner->system->resources))
  {
    if (r->cbdata == scanner || !strncmp(r->path, prefix, prefixlen))
      cupsArrayRemove(scanner->system->resources, r);
  }

  // If applicable, call the delete function...
  if (scanner->driver_data.delete_cb)
    (scanner->driver_data.delete_cb)(scanner, &scanner->driver_data);

  // Delete jobs...
  cupsArrayDelete(scanner->active_jobs);
  cupsArrayDelete(scanner->completed_jobs);
  cupsArrayDelete(scanner->all_jobs);

  // Free memory...
  free(scanner->name);
  free(scanner->dns_sd_name);
  free(scanner->location);
  free(scanner->geo_location);
  free(scanner->organization);
  free(scanner->org_unit);
  free(scanner->resource);
  free(scanner->device_id);
  free(scanner->device_uri);
  free(scanner->driver_name);

  ippDelete(scanner->driver_attrs);
  ippDelete(scanner->attrs);

  cupsArrayDelete(scanner->links);

  free(scanner);
}


//
// 'papplScannerDelete()' - Delete a scanner.
//
// This function deletes a scanner from a system, freeing all memory and
// canceling all jobs as needed.
//

void
papplScannerDelete(
    pappl_scanner_t *scanner)		// I - Scanner
{
  pappl_system_t *system = scanner->system;
					// System


  // Remove the scanner from the system object...
  pthread_rwlock_wrlock(&system->rwlock);
  cupsArrayRemove(system->scanners, scanner);
  pthread_rwlock_unlock(&system->rwlock);

  _papplSystemConfigChanged(system);
}


//
// 'compare_active_jobs()' - Compare two active jobs.
//

static int				// O - Result of comparison
compare_active_jobs(pappl_job_t *a,	// I - First job
                    pappl_job_t *b)	// I - Second job
{
  return (b->job_id - a->job_id);
}


//
// 'compare_jobs()' - Compare two jobs.
//

static int				// O - Result of comparison
compare_all_jobs(pappl_job_t *a,	// I - First job
                 pappl_job_t *b)	// I - Second job
{
  return (b->job_id - a->job_id);
}


//
// 'compare_completed_jobs()' - Compare two completed jobs.
//

static int				// O - Result of comparison
compare_completed_jobs(pappl_job_t *a,	// I - First job
                       pappl_job_t *b)	// I - Second job
{
  return (b->job_id - a->job_id);
}
