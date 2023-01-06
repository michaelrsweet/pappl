//
// Printer object for the Printer Application Framework
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

static int	compare_active_jobs(pappl_job_t *a, pappl_job_t *b);
static int	compare_all_jobs(pappl_job_t *a, pappl_job_t *b);
static int	compare_completed_jobs(pappl_job_t *a, pappl_job_t *b);


//
// 'papplPrinterCancelAllJobs()' - Cancel all jobs on the printer.
//
// This function cancels all jobs on the printer that are in the 'pending-held', 'pending',
// 'processing' or 'processing-stopped' states. If any job is currently being printed (in the
// 'processing' state), it will be stopped at a convenient time (usually the end of a page)
// so that the printer will be left in a known state.
//

void
papplPrinterCancelAllJobs(
    pappl_printer_t *printer)		// I - Printer
{
  pappl_job_t	*job;			// Job information


  // Loop through all jobs and cancel them.
  //
  // Since we have a writer lock, it is safe to use cupsArrayGetFirst/Last...
  _papplRWLockWrite(printer);

  for (job = (pappl_job_t *)cupsArrayGetFirst(printer->active_jobs); job; job = (pappl_job_t *)cupsArrayGetNext(printer->active_jobs))
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

      cupsArrayRemove(printer->active_jobs, job);
      cupsArrayAdd(printer->completed_jobs, job);
    }
  }

  _papplRWUnlock(printer);

  if (!printer->system->clean_time)
    printer->system->clean_time = time(NULL) + 60;
}


//
// 'papplPrinterCreate()' - Create a new printer.
//
// This function creates a new printer (service) on the specified system.  The
// "type" argument specifies the type of service to create and must currently
// be the value `PAPPL_SERVICE_TYPE_PRINT`.
//
// The "printer_id" argument specifies a positive integer identifier that is
// unique to the system.  If you specify a value of `0` a new identifier will
// be assigned.
//
// The "driver_name" argument specifies a named driver for the printer, from
// the list of drivers registered with the @link papplSystemSetPrinterDrivers@
// function.
//
// The "device_id" and "device_uri" arguments specify the IEEE-1284 device ID
// and device URI strings for the printer.
//
// On error, this function sets the `errno` variable to one of the following
// values:
//
// - `EEXIST`: A printer with the specified name already exists.
// - `EINVAL`: Bad values for the arguments were specified.
// - `EIO`: The driver callback failed.
// - `ENOENT`: No driver callback has been set.
// - `ENOMEM`: Ran out of memory.
//


pappl_printer_t *			// O - Printer or `NULL` on error
papplPrinterCreate(
    pappl_system_t       *system,	// I - System
    int                  printer_id,	// I - printer-id value or `0` for new
    const char           *printer_name,	// I - Human-readable printer name
    const char           *driver_name,	// I - Driver name
    const char           *device_id,	// I - IEEE-1284 device ID
    const char           *device_uri)	// I - Device URI
{
  pappl_printer_t	*printer;	// Printer
  char			resource[1024],	// Resource path
			*resptr,	// Pointer into resource path
			uuid[128],	// printer-uuid
			print_group[65];// print-group value
  int			k_supported;	// Maximum file size supported
#if !_WIN32
  struct statfs		spoolinfo;	// FS info for spool directory
  double		spoolsize;	// FS size
#endif // !_WIN32
  char			path[256];	// Path to resource
  pappl_pr_driver_data_t driver_data;	// Driver data
  ipp_t			*driver_attrs;	// Driver attributes
  size_t num_keywords = 0;  // Counter for how many keywords in various places
  static const char * const ipp_versions[] =
  {					// ipp-versions-supported values
    "1.1",
    "2.0"
  };
  static const int	operations[] =	// operations-supported values
  {
    IPP_OP_PRINT_JOB,
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
    IPP_OP_GET_PRINTER_SUPPORTED_VALUES,
    IPP_OP_CREATE_PRINTER_SUBSCRIPTIONS,
    IPP_OP_CREATE_JOB_SUBSCRIPTIONS,
    IPP_OP_GET_SUBSCRIPTION_ATTRIBUTES,
    IPP_OP_GET_SUBSCRIPTIONS,
    IPP_OP_RENEW_SUBSCRIPTION,
    IPP_OP_CANCEL_SUBSCRIPTION,
    IPP_OP_GET_NOTIFICATIONS,
    IPP_OP_ENABLE_PRINTER,
    IPP_OP_DISABLE_PRINTER,
    IPP_OP_PAUSE_PRINTER_AFTER_CURRENT_JOB,
    IPP_OP_CANCEL_CURRENT_JOB,
    IPP_OP_CANCEL_JOBS,
    IPP_OP_CANCEL_MY_JOBS,
    IPP_OP_CLOSE_JOB,
    IPP_OP_IDENTIFY_PRINTER,
    IPP_OP_GET_USER_PRINTER_ATTRIBUTES,
    IPP_OP_HOLD_JOB,
    IPP_OP_RELEASE_JOB,
    IPP_OP_HOLD_NEW_JOBS,
    IPP_OP_RELEASE_HELD_NEW_JOBS
  };
  static const char * const charset[] =	// charset-supported values
  {
    "us-ascii",
    "utf-8"
  };
  static const char * const client_info[] =
  {					// client-info-supported values
    "client-name",
    "client-patches",
    "client-string-version",
    "client-version"
  };
  static const char * const compression[] =
  {					// compression-supported values
    "deflate",
    "gzip",
    "none"
  };
  static const char * const job_password_encryption[] =
  {                    // job-password-encryption-supported values
    "none",
    "sha2-256",
    "sha2-384",
    "sha2-512",
    // Other values from EPX not yet supported by libcups
  };
  static const char * const job_password_repertoire[] =
  {                    // job-password-repertoire-supported values
    "iana_us-ascii_digits",
    "iana_us-ascii_letters",
    "iana_us-ascii_complex",
    "iana_us-ascii_any",
    "iana_utf-8_digits",
    "iana_utf-8_letters",
    "iana_utf-8_any"
  };
  static const char * const job_release_action[] =
  {                    // job-release-action-supported values
    "none",
    "button-press",
    "job-password",
    "owner-authorized"
  };
  
  static const char * const job_hold_until[] =
  {					// job-hold-until-supported values
    "day-time",
    "evening",
    "indefinite",
    "night",
    "no-hold",
    "second-shift",
    "third-shift",
    "weekend"
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
  static const char * const print_content_optimize[] =
  {					// print-content-optimize-supported
    "auto",
    "graphic",
    "photo",
    "text-and-graphic",
    "text"
  };
  static const char *print_processing[] =
  {					// print-processing-attributes-supported
    "print-color-mode",
    "printer-resolution"
  };
  static const int print_quality[] =	// print-quality-supported
  {
    IPP_QUALITY_DRAFT,
    IPP_QUALITY_NORMAL,
    IPP_QUALITY_HIGH
  };
  static const char * const print_scaling[] =
  {					// print-scaling-supported
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
  if (!system || !printer_name || !driver_name || !device_uri)
  {
    errno = EINVAL;
    return (NULL);
  }

  if (!system->driver_cb)
  {
    papplLog(system, PAPPL_LOGLEVEL_ERROR, "No driver callback set, unable to add printer.");
    errno = ENOENT;
    return (NULL);
  }

  // Prepare URI values for the printer attributes...
  if (system->options & PAPPL_SOPTIONS_MULTI_QUEUE)
  {
    // Make sure printer names that start with a digit have a resource path
    // containing an underscore...
    if (isdigit(*printer_name & 255))
      snprintf(resource, sizeof(resource), "/ipp/print/_%s", printer_name);
    else
      snprintf(resource, sizeof(resource), "/ipp/print/%s", printer_name);

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
    papplCopyString(resource, "/ipp/print", sizeof(resource));

  // Make sure the printer doesn't already exist...
  if ((printer = papplSystemFindPrinter(system, resource, 0, NULL)) != NULL)
  {
    int		n;		// Current instance number
    char	temp[1024];	// Temporary resource path

    if (!strcmp(printer_name, printer->name))
    {
      papplLog(system, PAPPL_LOGLEVEL_ERROR, "Printer '%s' already exists.", printer_name);
      errno = EEXIST;
      return (NULL);
    }

    for (n = 2; n < 10; n ++)
    {
      snprintf(temp, sizeof(temp), "%s_%d", resource, n);
      if (!papplSystemFindPrinter(system, temp, 0, NULL))
        break;
    }

    if (n >= 10)
    {
      papplLog(system, PAPPL_LOGLEVEL_ERROR, "Printer '%s' name conflicts with existing printer.", printer_name);
      errno = EEXIST;
      return (NULL);
    }

    papplCopyString(resource, temp, sizeof(resource));
  }

  // Allocate memory for the printer...
  if ((printer = calloc(1, sizeof(pappl_printer_t))) == NULL)
  {
    papplLog(system, PAPPL_LOGLEVEL_ERROR, "Unable to allocate memory for printer: %s", strerror(errno));
    return (NULL);
  }

  papplLog(system, PAPPL_LOGLEVEL_INFO, "Printer '%s' at resource path '%s'.", printer_name, resource);

  _papplSystemMakeUUID(system, printer_name, 0, uuid, sizeof(uuid));

  // Get the maximum spool size based on the size of the filesystem used for
  // the spool directory.  If the host OS doesn't support the statfs call
  // or the filesystem is larger than 2TiB, always report INT_MAX.
#if _WIN32
  k_supported = INT_MAX;
#else // !_WIN32
  if (statfs(system->directory, &spoolinfo))
    k_supported = INT_MAX;
  else if ((spoolsize = (double)spoolinfo.f_bsize * spoolinfo.f_blocks / 1024) > INT_MAX)
    k_supported = INT_MAX;
  else
    k_supported = (int)spoolsize;
#endif // _WIN32

  // Initialize printer structure and attributes...
  pthread_rwlock_init(&printer->rwlock, NULL);

  printer->system             = system;
  printer->name               = strdup(printer_name);
  printer->dns_sd_name        = strdup(printer_name);
  printer->resource           = strdup(resource);
  printer->resourcelen        = strlen(resource);
  printer->uriname            = printer->resource + 10; // Skip "/ipp/print" in resource
  printer->device_id          = device_id ? strdup(device_id) : NULL;
  printer->device_uri         = strdup(device_uri);
  printer->driver_name        = strdup(driver_name);
  printer->attrs              = ippNew();
  printer->start_time         = time(NULL);
  printer->config_time        = printer->start_time;
  printer->state              = IPP_PSTATE_IDLE;
  printer->state_reasons      = PAPPL_PREASON_NONE;
  printer->state_time         = printer->start_time;
  printer->is_accepting       = true;
  printer->all_jobs           = cupsArrayNew((cups_array_cb_t)compare_all_jobs, NULL, NULL, 0, NULL, (cups_afree_cb_t)_papplJobDelete);
  printer->active_jobs        = cupsArrayNew((cups_array_cb_t)compare_active_jobs, NULL, NULL, 0, NULL, NULL);
  printer->completed_jobs     = cupsArrayNew((cups_array_cb_t)compare_completed_jobs, NULL, NULL, 0, NULL, NULL);
  printer->next_job_id        = 1;
  printer->max_active_jobs    = (system->options & PAPPL_SOPTIONS_MULTI_QUEUE) ? 0 : 1;
  printer->max_completed_jobs = 100;
  printer->usb_vendor_id      = 0x1209;	// See <https://pid.codes>
  printer->usb_product_id     = 0x8011;
  printer->cancel_after_time              = INT_MAX; // initial "job-cancel-after-default" value
  printer->pw_repertoire_configured       = PAPPL_PW_REPERTOIRE_IANA_UTF_8_ANY; // initial "job-password-repertoire-configured" value
  printer->release_action_default         = PAPPL_RELEASE_ACTION_NONE;
  printer->st_access_supported            = PAPPL_ST_ACCESS_OWNER | PAPPL_ST_ACCESS_PUBLIC;
  printer->st_disposition_supported       = PAPPL_ST_DISPOSITION_STORE_ONLY | PAPPL_ST_DISPOSITION_PRINT_AND_STORE;
  printer->st_group_supported             = false;
  printer->which_jobs_supported           = PAPPL_WHICH_JOBS_ALL | PAPPL_WHICH_JOBS_COMPLETED | PAPPL_WHICH_JOBS_NOT_COMPLETED; // Always supported - not conditional on features

  if (printer->st_access_supported & PAPPL_ST_ACCESS_OWNER)
    printer->which_jobs_supported |= PAPPL_WHICH_JOBS_STORED_OWNER;
    
  if (printer->st_access_supported & PAPPL_ST_ACCESS_PUBLIC)
    printer->which_jobs_supported |= PAPPL_WHICH_JOBS_STORED_PUBLIC;
    
  if (printer->st_group_supported)
  {
    printer->st_access_supported |= PAPPL_ST_ACCESS_GROUP;
    printer->which_jobs_supported |= PAPPL_WHICH_JOBS_STORED_GROUP;
  }

  if (!printer->name || !printer->dns_sd_name || !printer->resource || (device_id && !printer->device_id) || !printer->device_uri || !printer->driver_name || !printer->attrs)
  {
    // Failed to allocate one of the required members...
    _papplPrinterDelete(printer);
    return (NULL);
  }

  if (papplSystemGetDefaultPrintGroup(system, print_group, sizeof(print_group)))
    papplPrinterSetPrintGroup(printer, print_group);

  // If the driver is "auto", figure out the proper driver name...
  if (!strcmp(driver_name, "auto") && system->autoadd_cb)
  {
    // If device_id is NULL, try to look it up...
    if (!printer->device_id && strncmp(device_uri, "file://", 7))
    {
      pappl_device_t	*device;	// Connection to printer

      papplLogPrinter(printer, PAPPL_LOGLEVEL_DEBUG, "Opening device for auto-setup.");
      if ((device = papplDeviceOpen(device_uri, "auto", papplLogDevice, system)) != NULL)
      {
        char	new_id[1024];		// New 1284 device ID

        if (papplDeviceGetID(device, new_id, sizeof(new_id)))
          printer->device_id = strdup(new_id);

        papplLogPrinter(printer, PAPPL_LOGLEVEL_DEBUG, "Closing device for auto-setup.");

        papplDeviceClose(device);
      }
    }

    if ((driver_name = (system->autoadd_cb)(printer_name, device_uri, printer->device_id, system->driver_cbdata)) == NULL)
    {
      errno = EIO;
      _papplPrinterDelete(printer);
      return (NULL);
    }
  }

  // Initialize driver...
  driver_attrs = NULL;
  _papplPrinterInitDriverData(&driver_data);

  if (!(system->driver_cb)(system, driver_name, device_uri, device_id, &driver_data, &driver_attrs, system->driver_cbdata))
  {
    errno = EIO;
    _papplPrinterDelete(printer);
    return (NULL);
  }

  papplPrinterSetDriverData(printer, &driver_data, driver_attrs);
  ippDelete(driver_attrs);

  // Generate printer-device-id value as needed...
  if (!printer->device_id)
  {
    char	temp_id[400],		// Temporary "printer-device-id" string
		mfg[128],		// Manufacturer name
		*mdl,			// Model name
		cmd[128],		// Command (format) list
		*ptr;			// Pointer into string
    ipp_attribute_t *formats;		// "document-format-supported" attribute
    cups_len_t	i,			// Looping var
		count;			// Number of values

    // Assume make and model are separated by a space...
    papplCopyString(mfg, driver_data.make_and_model, sizeof(mfg));
    if ((mdl = strchr(mfg, ' ')) != NULL)
      *mdl++ = '\0';			// Nul-terminate the make
    else
      mdl = mfg;			// No separator, so assume the make and model are the same

    formats = ippFindAttribute(printer->driver_attrs, "document-format-supported", IPP_TAG_MIMETYPE);
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
      else if (!strcmp(format, "application/vnd.zebra-epl"))
        format = "EPL";
      else if (!strcmp(format, "application/vnd.zebra-zpl"))
        format = "ZPL";
      else if (!strcmp(format, "image/jpeg"))
        format = "JPEG";
      else if (!strcmp(format, "image/png"))
        format = "PNG";
      else if (!strcmp(format, "image/pwg-raster"))
        format = "PWGRaster";
      else if (!strcmp(format, "image/urf"))
        format = "URF";
      else if (!strcmp(format, "text/plain"))
        format = "TXT";
      else if (!strcmp(format, "application/octet-stream"))
        continue;

      if (ptr > cmd)
        snprintf(ptr, sizeof(cmd) - (size_t)(ptr - cmd), ",%s", format);
      else
        papplCopyString(cmd, format, sizeof(cmd));

      ptr += strlen(ptr);
    }

    *ptr = '\0';

    snprintf(temp_id, sizeof(temp_id), "MFG:%s;MDL:%s;CMD:%s;", mfg, mdl, cmd);
    if ((printer->device_id = strdup(temp_id)) == NULL)
    {
      _papplPrinterDelete(printer);
      return (NULL);
    }
  }

  // charset-configured
  ippAddString(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_CHARSET), "charset-configured", NULL, "utf-8");

  // charset-supported
  ippAddStrings(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_CHARSET), "charset-supported", sizeof(charset) / sizeof(charset[0]), NULL, charset);

  // client-info-supported
  ippAddStrings(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "client-info-supported", (int)(sizeof(client_info) / sizeof(client_info[0])), NULL, client_info);

  // compression-supported
  ippAddStrings(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "compression-supported", (int)(sizeof(compression) / sizeof(compression[0])), NULL, compression);

  // copies-default
  ippAddInteger(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "copies-default", 1);

  // device-uuid
  ippAddString(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_URI, "device-uuid", NULL, uuid);

  // document-format-default
  ippAddString(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_MIMETYPE), "document-format-default", NULL, "application/octet-stream");

  // generated-natural-language-supported
  ippAddString(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_LANGUAGE), "generated-natural-language-supported", NULL, "en");

  // ipp-versions-supported
  ippAddStrings(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "ipp-versions-supported", (cups_len_t)(sizeof(ipp_versions) / sizeof(ipp_versions[0])), NULL, ipp_versions);

  // job-hold-until-default
  ippAddString(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "job-hold-until-default", NULL, "no-hold");

  // job-hold-until-supported
  ippAddStrings(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "job-hold-until-supported", (cups_len_t)(sizeof(job_hold_until) / sizeof(job_hold_until[0])), NULL, job_hold_until);

  // job-hold-until-time-supported
  ippAddBoolean(printer->attrs, IPP_TAG_PRINTER, "job-hold-until-time-supported", 1);

  // job-cancel-after-supported
  ippAddRange(printer->attrs, IPP_TAG_PRINTER, "job-cancel-after-supported", 30, INT_MAX); // 30 seconds minimum

  // job-ids-supported
  ippAddBoolean(printer->attrs, IPP_TAG_PRINTER, "job-ids-supported", 1);

  // job-k-octets-supported
  ippAddRange(printer->attrs, IPP_TAG_PRINTER, "job-k-octets-supported", 0, k_supported);

  // job-password-encryption-supported
  ippAddStrings(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "job-password-encryption-supported", (cups_len_t)(sizeof(job_password_encryption) / sizeof(job_password_encryption[0])), NULL, job_password_encryption);

  // job-password-length-supported
  ippAddRange(printer->attrs, IPP_TAG_PRINTER, "job-password-length-supported", 4, 1020); // 4 digit PIN up to 255 characters UTF-8 with 4 octets / character

  // job-password-repertoire-supported
  ippAddStrings(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "job-password-repertoire-supported", (cups_len_t)(sizeof(job_password_repertoire) / sizeof(job_password_repertoire[0])), NULL, job_password_repertoire);

  // job-password-supported
  ippAddInteger(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "job-password-supported", 255);

  // job-priority-default
  ippAddInteger(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "job-priority-default", 50);

  // job-priority-supported
  ippAddInteger(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "job-priority-supported", 1);

  // job-release-action-supported
  ippAddStrings(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "job-release-action-supported", (cups_len_t)(sizeof(job_release_action) / sizeof(job_release_action[0])), NULL, job_release_action);

  // job-sheets-default
  ippAddString(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_NAME), "job-sheets-default", NULL, "none");

  // job-sheets-supported
  ippAddString(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_NAME), "job-sheets-supported", NULL, "none");

  // job-storage-supported
  const char * job_st_supported_keywords[3];
  job_st_supported_keywords[0] = "job-storage-access";
  job_st_supported_keywords[1] = "job-storage-disposition";
  num_keywords = 2;
  if (printer->st_group_supported)
    job_st_supported_keywords[++num_keywords] = "job-storage-group";
  ippAddStrings(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "job-storage-supported", (cups_len_t)num_keywords, NULL, job_st_supported_keywords);

  // job-storage-access-supported
  char * job_st_access_supported_keywords[3];
  num_keywords = _papplLookupStrings(printer->st_access_supported, 3, job_st_access_supported_keywords, sizeof(_pappl_st_access) / sizeof(_pappl_st_access[0]), _pappl_st_access);
  ippAddStrings(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "job-storage-access-supported", (cups_len_t)num_keywords, NULL, job_st_access_supported_keywords);

  // job-storage-disposition-supported
  char * job_st_disposition_supported_keywords[3];
  num_keywords = _papplLookupStrings(printer->st_disposition_supported, 3, job_st_disposition_supported_keywords, sizeof(_pappl_st_disposition) / sizeof(_pappl_st_disposition[0]), _pappl_st_disposition);
  ippAddStrings(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "job-storage-disposition-supported", (cups_len_t)num_keywords, NULL, job_st_disposition_supported_keywords);

  // job-storage-group-supported
  if (printer->st_group_supported)
  {
    const char * job_st_group_supported[] = { "fake_group1", "fake_group2", "fake_group3"};
    ippAddStrings(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "job-storage-group-supported", (cups_len_t)(sizeof(job_st_group_supported) / sizeof(job_st_group_supported[0])), NULL, job_st_group_supported);
  }

  if (_papplSystemFindMIMEFilter(system, "image/jpeg", "image/pwg-raster"))
  {
    static const char * const jpeg_features_supported[] =
    {					// "jpeg-features-supported" values
      "arithmetic",
      "cmyk",
      "progressive"
    };

    // jpeg-features-supported
    ippAddStrings(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "jpeg-features-supported", (int)(sizeof(jpeg_features_supported) / sizeof(jpeg_features_supported[0])), NULL, jpeg_features_supported);

    // jpeg-k-octets-supported
    ippAddRange(printer->attrs, IPP_TAG_PRINTER, "jpeg-k-octets-supported", 0, k_supported);

    // jpeg-x-dimension-supported
    ippAddRange(printer->attrs, IPP_TAG_PRINTER, "jpeg-x-dimension-supported", 0, system->max_image_width);

    // jpeg-y-dimension-supported
    ippAddRange(printer->attrs, IPP_TAG_PRINTER, "jpeg-y-dimension-supported", 1, system->max_image_height);
  }

  // multiple-document-handling-supported
  ippAddStrings(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "multiple-document-handling-supported", sizeof(multiple_document_handling) / sizeof(multiple_document_handling[0]), NULL, multiple_document_handling);

  // multiple-document-jobs-supported
  ippAddBoolean(printer->attrs, IPP_TAG_PRINTER, "multiple-document-jobs-supported", 0);

  // multiple-operation-time-out
  ippAddInteger(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "multiple-operation-time-out", 60);

  // multiple-operation-time-out-action
  ippAddString(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "multiple-operation-time-out-action", NULL, "abort-job");

  // natural-language-configured
  ippAddString(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_LANGUAGE), "natural-language-configured", NULL, "en");

  // notify-events-default
  ippAddString(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "notify-events-default", NULL, "job-completed");

  // notify-events-supported
  ippAddStrings(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "notify-events-supported", (int)(sizeof(_papplEvents) / sizeof(_papplEvents[0])), NULL, _papplEvents);

  // notify-lease-duration-default
  ippAddInteger(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "notify-lease-duration-default", PAPPL_LEASE_DEFAULT);

  // notify-lease-duration-supported
  ippAddRange(printer->attrs, IPP_TAG_PRINTER, "notify-lease-duration-supported", 0, PAPPL_LEASE_MAX);

  // notify-max-events-supported
  ippAddInteger(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "notify-max-events-supported", PAPPL_MAX_EVENTS);

  // notify-pull-method-supported
  ippAddString(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "notify-pull-method-supported", NULL, "ippget");

  // operations-supported
  ippAddIntegers(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_ENUM, "operations-supported", (int)(sizeof(operations) / sizeof(operations[0])), operations);

  // orientation-requested-supported
  ippAddIntegers(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_ENUM, "orientation-requested-supported", (int)(sizeof(orientation_requested) / sizeof(orientation_requested[0])), orientation_requested);

  // pdl-override-supported
  ippAddString(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "pdl-override-supported", NULL, "attempted");

  // print-content-optimize-supported
  ippAddStrings(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "print-content-optimize-supported", (int)(sizeof(print_content_optimize) / sizeof(print_content_optimize[0])), NULL, print_content_optimize);

  // print-processing-attributes-supported
  ippAddStrings(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "print-processing-attributes-supported", (int)(sizeof(print_processing) / sizeof(print_processing[0])), NULL, print_processing);

  // print-quality-supported
  ippAddIntegers(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_ENUM, "print-quality-supported", (int)(sizeof(print_quality) / sizeof(print_quality[0])), print_quality);

  // print-scaling-supported
  ippAddStrings(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "print-scaling-supported", (int)(sizeof(print_scaling) / sizeof(print_scaling[0])), NULL, print_scaling);

  // printer-device-id
  if (printer->device_id)
    ippAddString(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_TEXT, "printer-device-id", NULL, printer->device_id);

  // printer-get-attributes-supported
  ippAddString(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "printer-get-attributes-supported", NULL, "document-format");

  // printer-id
  ippAddInteger(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "printer-id", printer_id);

  // printer-info
  ippAddString(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_TEXT, "printer-info", NULL, printer_name);

  // printer-name
  ippAddString(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_NAME, "printer-name", NULL, printer_name);

  // printer-uuid
  ippAddString(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_URI, "printer-uuid", NULL, uuid);

  // proof-copies-supported
  ippAddRange(printer->attrs, IPP_TAG_PRINTER, "proof-copies-supported", 1, INT_MAX);

  // requesting-user-uri-supported
  ippAddBoolean(printer->attrs, IPP_TAG_PRINTER, "requesting-user-uri-supported", 1);

  // uri-security-supported
  if (system->options & PAPPL_SOPTIONS_NO_TLS)
    ippAddString(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "uri-security-supported", NULL, "none");
  else if (papplSystemGetTLSOnly(system))
    ippAddString(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "uri-security-supported", NULL, "tls");
  else
    ippAddStrings(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "uri-security-supported", 2, NULL, uri_security);

  // which-jobs-supported
  const char * which_jobs_supported_keywords[16];
  num_keywords = _papplLookupStrings(printer->which_jobs_supported, 16, which_jobs_supported_keywords, sizeof(_pappl_which_jobs) / sizeof(_pappl_which_jobs[0]), _pappl_which_jobs);
  ippAddStrings(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "which-jobs-supported", num_keywords, NULL, which_jobs_supported_keywords);
    

  // Add the printer to the system...
  _papplSystemAddPrinter(system, printer, printer_id);

  // Do any post-creation work...
  if (system->create_cb)
    (system->create_cb)(printer, system->driver_cbdata);

  // Add socket listeners...
  if (system->options & PAPPL_SOPTIONS_RAW_SOCKET)
  {
    if (_papplPrinterAddRawListeners(printer) && system->is_running)
    {
      pthread_t	tid;			// Thread ID

      if (pthread_create(&tid, NULL, (void *(*)(void *))_papplPrinterRunRaw, printer))
      {
	// Unable to create client thread...
	papplLogPrinter(printer, PAPPL_LOGLEVEL_ERROR, "Unable to create raw listener thread: %s", strerror(errno));
      }
      else
      {
	// Detach the main thread from the raw thread to prevent hangs...
	pthread_detach(tid);

        _papplRWLockRead(printer);
	while (!printer->raw_active)
	{
	  _papplRWUnlock(printer);
	  usleep(1000);			// Wait for raw thread to start
	  _papplRWLockRead(printer);
	}
	_papplRWUnlock(printer);
      }
    }
  }

  // Add icons...
  _papplSystemAddPrinterIcons(system, printer);

  // Add web pages, if any...
  if (system->options & PAPPL_SOPTIONS_WEB_INTERFACE)
  {
    snprintf(path, sizeof(path), "%s/", printer->uriname);
    papplSystemAddResourceCallback(system, path, "text/html", (pappl_resource_cb_t)_papplPrinterWebHome, printer);

    snprintf(path, sizeof(path), "%s/cancelall", printer->uriname);
    papplSystemAddResourceCallback(system, path, "text/html", (pappl_resource_cb_t)_papplPrinterWebCancelAllJobs, printer);

    if (system->options & PAPPL_SOPTIONS_MULTI_QUEUE)
    {
      snprintf(path, sizeof(path), "%s/delete", printer->uriname);
      papplSystemAddResourceCallback(system, path, "text/html", (pappl_resource_cb_t)_papplPrinterWebDelete, printer);
    }

    snprintf(path, sizeof(path), "%s/config", printer->uriname);
    papplSystemAddResourceCallback(system, path, "text/html", (pappl_resource_cb_t)_papplPrinterWebConfig, printer);

    snprintf(path, sizeof(path), "%s/jobs", printer->uriname);
    papplSystemAddResourceCallback(system, path, "text/html", (pappl_resource_cb_t)_papplPrinterWebJobs, printer);

    snprintf(path, sizeof(path), "%s/media", printer->uriname);
    papplSystemAddResourceCallback(system, path, "text/html", (pappl_resource_cb_t)_papplPrinterWebMedia, printer);
    papplPrinterAddLink(printer, _PAPPL_LOC("Media"), path, PAPPL_LOPTIONS_NAVIGATION | PAPPL_LOPTIONS_STATUS);

    snprintf(path, sizeof(path), "%s/printing", printer->uriname);
    papplSystemAddResourceCallback(system, path, "text/html", (pappl_resource_cb_t)_papplPrinterWebDefaults, printer);
    papplPrinterAddLink(printer, _PAPPL_LOC("Printing Defaults"), path, PAPPL_LOPTIONS_NAVIGATION | PAPPL_LOPTIONS_STATUS);

    if (printer->driver_data.has_supplies)
    {
      snprintf(path, sizeof(path), "%s/supplies", printer->uriname);
      papplSystemAddResourceCallback(system, path, "text/html", (pappl_resource_cb_t)_papplPrinterWebSupplies, printer);
      papplPrinterAddLink(printer, _PAPPL_LOC("Supplies"), path, PAPPL_LOPTIONS_STATUS);
    }
  }

  _papplSystemConfigChanged(system);

  // Return it!
  return (printer);
}


//
// '_papplPrinterDelete()' - Free memory associated with a printer.
//

void
_papplPrinterDelete(
    pappl_printer_t *printer)		// I - Printer
{
  int			i;		// Looping var
  _pappl_resource_t	*r;		// Current resource
  char			prefix[1024];	// Prefix for printer resources
  size_t		prefixlen;	// Length of prefix


  // Let USB/raw printing threads know to exit
  _papplRWLockWrite(printer);
  printer->is_deleted = true;

  while (printer->raw_active || printer->usb_active)
  {
    // Wait for threads to finish
    _papplRWUnlock(printer);
    usleep(1000);
    _papplRWLockRead(printer);
  }
  _papplRWUnlock(printer);

  // Close raw listener sockets...
  for (i = 0; i < printer->num_raw_listeners; i ++)
  {
#if _WIN32
    closesocket(printer->raw_listeners[i].fd);
#else
    close(printer->raw_listeners[i].fd);
#endif // _WIN32
    printer->raw_listeners[i].fd = -1;
  }

  printer->num_raw_listeners = 0;

  // Remove DNS-SD registrations...
  _papplPrinterUnregisterDNSSDNoLock(printer);

  // Remove printer-specific resources...
  snprintf(prefix, sizeof(prefix), "%s/", printer->uriname);
  prefixlen = strlen(prefix);

  // Note: System writer lock is already held when calling cupsArrayRemove
  // for the system's printer object, so we don't need a separate lock here
  // and can safely use cupsArrayGetFirst/Next...
  for (r = (_pappl_resource_t *)cupsArrayGetFirst(printer->system->resources); r; r = (_pappl_resource_t *)cupsArrayGetNext(printer->system->resources))
  {
    if (r->cbdata == printer || !strncmp(r->path, prefix, prefixlen))
      cupsArrayRemove(printer->system->resources, r);
  }

  // If applicable, call the delete function...
  if (printer->driver_data.delete_cb)
    (printer->driver_data.delete_cb)(printer, &printer->driver_data);

  // Delete jobs...
  cupsArrayDelete(printer->active_jobs);
  cupsArrayDelete(printer->completed_jobs);
  cupsArrayDelete(printer->all_jobs);

  // Free memory...
  free(printer->name);
  free(printer->dns_sd_name);
  free(printer->location);
  free(printer->geo_location);
  free(printer->organization);
  free(printer->org_unit);
  free(printer->resource);
  free(printer->device_id);
  free(printer->device_uri);
  free(printer->driver_name);
  free(printer->usb_storage);

  ippDelete(printer->driver_attrs);
  ippDelete(printer->attrs);

  cupsArrayDelete(printer->links);

  pthread_rwlock_destroy(&printer->rwlock);

  free(printer);
}


//
// 'papplPrinterDelete()' - Delete a printer.
//
// This function deletes a printer from a system, freeing all memory and
// canceling all jobs as needed.
//

void
papplPrinterDelete(
    pappl_printer_t *printer)		// I - Printer
{
  pappl_system_t *system = printer->system;
					// System


  // Deliver delete event...
  papplSystemAddEvent(system, printer, NULL, PAPPL_EVENT_PRINTER_DELETED | PAPPL_EVENT_SYSTEM_CONFIG_CHANGED, NULL);

  // Remove the printer from the system object...
  _papplRWLockWrite(system);
  cupsArrayRemove(system->printers, printer);
  _papplRWUnlock(system);

  _papplSystemConfigChanged(system);
}


//
// 'papplPrinterOpenFile()' - Create or open a file for a printer.
//
// This function creates, opens, or removes a file for a printer.  The "fname"
// and "fnamesize" arguments specify the location and size of a buffer to store
// the printer filename, which incorporates the "directory", printer ID,
// resource name, and "ext" values.  The resource name is "sanitized" to only
// contain alphanumeric characters.
//
// The "mode" argument is "r" to read an existing printer file, "w" to write a
// new printer file, or "x" to remove an exitsing printer file.  New files are
// created with restricted permissions for security purposes.
//
// For the "r" and "w" modes, the return value is the file descriptor number on
// success or `-1` on error.  For the "x" mode, the return value is `0` on
// success and `-1` on error.  The `errno` variable is set appropriately on
// error.
//

int					// O - File descriptor or -1 on error
papplPrinterOpenFile(
    pappl_printer_t *printer,		// I - Printer
    char            *fname,		// I - Filename buffer
    size_t          fnamesize,		// I - Size of filename buffer
    const char      *directory,		// I - Directory to store in (`NULL` for default)
    const char      *resname,		// I - Resource name
    const char      *ext,		// I - Extension (`NULL` for none)
    const char      *mode)		// I - Open mode - "r" for reading or "w" for writing
{
  char	name[64],			// "Safe" filename
	*nameptr;			// Pointer into filename


  // Range check input...
  if (!printer || !fname || fnamesize < 256 || !resname || !mode)
  {
    if (fname)
      *fname = '\0';

    return (-1);
  }

  // Make sure the spool directory exists...
  if (!directory)
    directory = printer->system->directory;

  if (access(directory, X_OK))
  {
    if (errno == ENOENT)
    {
      // Spool directory does not exist, might have been deleted...
      if (mkdir(directory, 0777))
      {
        papplLogPrinter(printer, PAPPL_LOGLEVEL_FATAL, "Unable to create spool directory '%s': %s", directory, strerror(errno));
        return (-1);
      }
    }
    else
    {
      papplLogPrinter(printer, PAPPL_LOGLEVEL_FATAL, "Unable to access spool directory '%s': %s", directory, strerror(errno));
      return (-1);
    }
  }

  // Make a name from the resource name argument...
  for (nameptr = name; *resname && nameptr < (name + sizeof(name) - 1); resname ++)
  {
    if (isalnum(*resname & 255) || *resname == '-' || *resname == '.')
    {
      *nameptr++ = (char)tolower(*resname & 255);
    }
    else
    {
      *nameptr++ = '_';

      while (resname[1] && !isalnum(resname[1] & 255) && resname[1] != '-' && resname[1] != '.')
        resname ++;
    }
  }

  *nameptr = '\0';

  // Create a filename...
  if (ext)
    snprintf(fname, fnamesize, "%s/p%05d-%s.%s", directory, printer->printer_id, name, ext);
  else
    snprintf(fname, fnamesize, "%s/p%05d-%s", directory, printer->printer_id, name);

  if (!strcmp(mode, "r"))
    return (open(fname, O_RDONLY | O_NOFOLLOW | O_CLOEXEC | O_BINARY));
  else if (!strcmp(mode, "w"))
    return (open(fname, O_WRONLY | O_CREAT | O_TRUNC | O_NOFOLLOW | O_CLOEXEC | O_BINARY, 0600));
  else if (!strcmp(mode, "x"))
    return (unlink(fname));
  else
    return (-1);
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
