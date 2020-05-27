//
// Printer object for the Printer Application Framework
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
// Local functions...
//

static int	compare_active_jobs(pappl_job_t *a, pappl_job_t *b);
static int	compare_all_jobs(pappl_job_t *a, pappl_job_t *b);
static int	compare_completed_jobs(pappl_job_t *a, pappl_job_t *b);
static int	compare_printers(pappl_printer_t *a, pappl_printer_t *b);
static void	free_printer(pappl_printer_t *printer);


//
// 'papplPrinterCancelAllJobs()' - Cancel all jobs on the printer.
//

void
papplPrinterCancelAllJobs(
    pappl_printer_t *printer)		// I - Printer
{
  pappl_job_t	*job;			// Job information


  // Loop through all jobs and cancel them...
  pthread_rwlock_wrlock(&printer->rwlock);

  for (job = (pappl_job_t *)cupsArrayFirst(printer->active_jobs); job; job = (pappl_job_t *)cupsArrayNext(printer->active_jobs))
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

      if (job->filename)
      {
	unlink(job->filename);
	free(job->filename);
	job->filename = NULL;
      }

      cupsArrayRemove(printer->active_jobs, job);
      cupsArrayAdd(printer->completed_jobs, job);
    }
  }

  pthread_rwlock_unlock(&printer->rwlock);

  if (!printer->system->clean_time)
    printer->system->clean_time = time(NULL) + 60;
}


//
// 'papplPrinterCreate()' - Create a new printer.
//

pappl_printer_t *			// O - Printer
papplPrinterCreate(
    pappl_system_t       *system,	// I - System
    pappl_service_type_t type,		// I - Service type
    int                  printer_id,	// I - printer-id value or 0 for new
    const char           *printer_name,	// I - Printer name
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
  struct statfs		spoolinfo;	// FS info for spool directory
  double		spoolsize;	// FS size
  char			path[256];	// Path to resource
  pappl_pdriver_data_t	driver_data;	// Driver data
  ipp_t			*driver_attrs;	// Driver attributes
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
    IPP_OP_SET_PRINTER_ATTRIBUTES,
    IPP_OP_CANCEL_MY_JOBS,
    IPP_OP_CLOSE_JOB,
    IPP_OP_IDENTIFY_PRINTER
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
  static const char * const print_content_optimize[] =
  {					// print-content-optimize-supported
    "auto",
    "graphic",
    "photo",
    "text-and-graphic",
    "text"
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
  if (!system || !printer_name || !driver_name || !device_uri || !strcmp(printer_name, "ipp") || type != PAPPL_SERVICE_TYPE_PRINT)
    return (NULL);
 
  if (!system->pdriver_cb)
  {
    papplLog(system, PAPPL_LOGLEVEL_ERROR, "No driver callback set, unable to add printer.");
    return (NULL);
  }
 
  // Allocate memory for the printer...
  if ((printer = calloc(1, sizeof(pappl_printer_t))) == NULL)
  {
    papplLog(system, PAPPL_LOGLEVEL_ERROR, "Unable to allocate memory for printer: %s", strerror(errno));
    return (NULL);
  }

  // Prepare URI values for the printer attributes...
  if (system->options & PAPPL_SOPTIONS_MULTI_QUEUE)
  {
    snprintf(resource, sizeof(resource), "/ipp/print/%s", printer_name);
    for (resptr = resource + 11; *resptr; resptr ++)
      if ((*resptr & 255) <= ' ' || *resptr == 0x7f)
	*resptr = '_';
  }
  else
    strlcpy(resource, "/ipp/print", sizeof(resource));

  papplLog(system, PAPPL_LOGLEVEL_INFO, "Printer '%s' at resource path '%s'.", printer_name, resource);

  _papplSystemMakeUUID(system, printer_name, 0, uuid, sizeof(uuid));

  // Get the maximum spool size based on the size of the filesystem used for
  // the spool directory.  If the host OS doesn't support the statfs call
  // or the filesystem is larger than 2TiB, always report INT_MAX.
  if (statfs(system->directory, &spoolinfo))
    k_supported = INT_MAX;
  else if ((spoolsize = (double)spoolinfo.f_bsize * spoolinfo.f_blocks / 1024) > INT_MAX)
    k_supported = INT_MAX;
  else
    k_supported = (int)spoolsize;

  // Initialize printer structure and attributes...
  pthread_rwlock_init(&printer->rwlock, NULL);

  printer->system             = system;
  printer->type               = type;
  printer->name               = strdup(printer_name);
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
  printer->all_jobs           = cupsArrayNew3((cups_array_func_t)compare_all_jobs, NULL, NULL, 0, NULL, (cups_afree_func_t)_papplJobDelete);
  printer->active_jobs        = cupsArrayNew((cups_array_func_t)compare_active_jobs, NULL);
  printer->completed_jobs     = cupsArrayNew((cups_array_func_t)compare_completed_jobs, NULL);
  printer->next_job_id        = 1;
  printer->max_active_jobs    = (system->options & PAPPL_SOPTIONS_MULTI_QUEUE) ? 0 : 1;
  printer->max_completed_jobs = 100;

  if (papplSystemGetDefaultPrintGroup(system, print_group, sizeof(print_group)))
    papplPrinterSetPrintGroup(printer, print_group);

  // Initialize driver...
  driver_attrs = NULL;
  _papplPrinterInitPrintDriverData(&driver_data);

  if (!(system->pdriver_cb)(system, driver_name, device_uri, &driver_data, &driver_attrs, system->pdriver_cbdata))
  {
    free_printer(printer);
    return (NULL);
  }

  papplPrinterSetPrintDriverData(printer, &driver_data, driver_attrs);
  ippDelete(driver_attrs);

  // charset-configured
  ippAddString(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_CHARSET), "charset-configured", NULL, "utf-8");

  // charset-supported
  ippAddStrings(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_CHARSET), "charset-supported", sizeof(charset) / sizeof(charset[0]), NULL, charset);

  // compression-supported
  ippAddStrings(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "compression-supported", (int)(sizeof(compression) / sizeof(compression[0])), NULL, compression);

  // copies-default
  ippAddInteger(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "copies-default", 1);

  // copies-supported
  // TODO: filter based on document format
  ippAddRange(printer->attrs, IPP_TAG_PRINTER, "copies-supported", 1, 999);

  // document-format-default
  ippAddString(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_MIMETYPE), "document-format-default", NULL, "application/octet-stream");

  // generated-natural-language-supported
  ippAddString(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_LANGUAGE), "generated-natural-language-supported", NULL, "en");

  // ipp-versions-supported
  ippAddStrings(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "ipp-versions-supported", (int)(sizeof(ipp_versions) / sizeof(ipp_versions[0])), NULL, ipp_versions);

  // job-ids-supported
  ippAddBoolean(printer->attrs, IPP_TAG_PRINTER, "job-ids-supported", 1);

  // job-k-octets-supported
  ippAddRange(printer->attrs, IPP_TAG_PRINTER, "job-k-octets-supported", 0, k_supported);

  // job-priority-default
  ippAddInteger(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "job-priority-default", 50);

  // job-priority-supported
  ippAddInteger(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "job-priority-supported", 1);

  // job-sheets-default
  ippAddString(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_NAME), "job-sheets-default", NULL, "none");

  // job-sheets-supported
  ippAddString(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_NAME), "job-sheets-supported", NULL, "none");

  // job-spooling-supported
  ippAddString(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "job-spooling-supported", NULL, printer->max_active_jobs > 1 ? "spool" : "stream");

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
    ippAddRange(printer->attrs, IPP_TAG_PRINTER, "jpeg-x-dimension-supported", 0, 16384);

    // jpeg-y-dimension-supported
    ippAddRange(printer->attrs, IPP_TAG_PRINTER, "jpeg-y-dimension-supported", 1, 16384);
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

  // operations-supported
  ippAddIntegers(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_ENUM, "operations-supported", (int)(sizeof(operations) / sizeof(operations[0])), operations);

  // orientation-requested-supported
  ippAddIntegers(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_ENUM, "orientation-requested-supported", (int)(sizeof(orientation_requested) / sizeof(orientation_requested[0])), orientation_requested);

  if (_papplSystemFindMIMEFilter(system, "application/pdf", "image/pwg-raster"))
  {
    static const char * const pdf_versions_supported[] =
    {					// "pdf-versions-supported" values
      "adobe-1.3",
      "adobe-1.4",
      "adobe-1.5",
      "adobe-1.6",
      "iso-32000-1_2008"		// PDF 1.7
    };

    // max-page-ranges-supported
    ippAddInteger(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "max-page-ranges-supported", 1);

    // page-ranges-supported
    ippAddBoolean(printer->attrs, IPP_TAG_PRINTER, "page-ranges-supported", 1);

    // pdf-k-octets-supported
    ippAddRange(printer->attrs, IPP_TAG_PRINTER, "pdf-k-octets-supported", 0, k_supported);

    // pdf-versions-supported
    ippAddStrings(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "pdf-versions-supported", (int)(sizeof(pdf_versions_supported) / sizeof(pdf_versions_supported[0])), NULL, pdf_versions_supported);
  }

  // pdl-override-supported
  ippAddString(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "pdl-override-supported", NULL, "attempted");

  // print-content-optimize-supported
  ippAddStrings(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "print-content-optimize-supported", (int)(sizeof(print_content_optimize) / sizeof(print_content_optimize[0])), NULL, print_content_optimize);

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

#if 0 // TODO: put in copy_printer_attributes
  // printer-strings-languages-supported
  ippAddStrings(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_LANGUAGE, "printer-strings-languages-supported", (int)(sizeof(printer_strings_languages) / sizeof(printer_strings_languages[0])), NULL, printer_strings_languages);
#endif // 0

  // printer-uuid
  ippAddString(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_URI, "printer-uuid", NULL, uuid);

  // uri-security-supported
  if (papplSystemGetTLSOnly(system))
    ippAddString(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "uri-security-supported", NULL, "tls");
  else
    ippAddStrings(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "uri-security-supported", 2, NULL, uri_security);

  // which-jobs-supported
  ippAddStrings(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "which-jobs-supported", sizeof(which_jobs) / sizeof(which_jobs[0]), NULL, which_jobs);

  // Add the printer to the system...
  pthread_rwlock_wrlock(&system->rwlock);

  if (printer_id)
    printer->printer_id = printer_id;
  else
    printer->printer_id = system->next_printer_id ++;

  if (!system->printers)
    system->printers = cupsArrayNew3((cups_array_func_t)compare_printers, NULL, NULL, 0, NULL, (cups_afree_func_t)free_printer);

  cupsArrayAdd(system->printers, printer);

  if (!system->default_printer_id)
    system->default_printer_id = printer->printer_id;

  pthread_rwlock_unlock(&system->rwlock);

  _papplSystemConfigChanged(system);

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
      }
    }
  }

  // Add icons...
  _papplSystemAddPrinterIcons(system, printer);

  // Add web pages, if any...
  if (system->options & PAPPL_SOPTIONS_STANDARD)
  {
    snprintf(path, sizeof(path), "%s/", printer->uriname);
    papplSystemAddResourceCallback(system, path, "text/html", (pappl_resource_cb_t)_papplPrinterWebHome, printer);

    snprintf(path, sizeof(path), "%s/cancel", printer->uriname);
    papplSystemAddResourceCallback(system, path, "text/html", (pappl_resource_cb_t)_papplPrinterWebCancelJob, printer);

    snprintf(path, sizeof(path), "%s/cancelall", printer->uriname);
    papplSystemAddResourceCallback(system, path, "text/html", (pappl_resource_cb_t)_papplPrinterWebCancelAllJobs, printer);

    snprintf(path, sizeof(path), "%s/config", printer->uriname);
    papplSystemAddResourceCallback(system, path, "text/html", (pappl_resource_cb_t)_papplPrinterWebConfig, printer);

    snprintf(path, sizeof(path), "%s/jobs", printer->uriname);
    papplSystemAddResourceCallback(system, path, "text/html", (pappl_resource_cb_t)_papplPrinterWebJobs, printer);

    snprintf(path, sizeof(path), "%s/media", printer->uriname);
    papplSystemAddResourceCallback(system, path, "text/html", (pappl_resource_cb_t)_papplPrinterWebMedia, printer);
    papplPrinterAddLink(printer, "Media", path, true);

    snprintf(path, sizeof(path), "%s/printing", printer->uriname);
    papplSystemAddResourceCallback(system, path, "text/html", (pappl_resource_cb_t)_papplPrinterWebDefaults, printer);
    papplPrinterAddLink(printer, "Printing Defaults", path, true);

    if (printer->driver_data.has_supplies)
    {
      snprintf(path, sizeof(path), "%s/supplies", printer->uriname);
      papplSystemAddResourceCallback(system, path, "text/html", (pappl_resource_cb_t)_papplPrinterWebSupplies, printer);
      papplPrinterAddLink(printer, "Supplies", path, false);
    }
  }

  _papplSystemConfigChanged(system);

  // Return it!
  return (printer);
}


//
// 'papplPrinterDelete()' - Delete a printer.
//

void
papplPrinterDelete(
    pappl_printer_t *printer)		// I - Printer
{
  pappl_system_t *system = printer->system;
					// System

  // Remove the printer from the system object...
  pthread_rwlock_wrlock(&system->rwlock);
  cupsArrayRemove(system->printers, printer);
  pthread_rwlock_unlock(&system->rwlock);
}


//
// 'papplSystemFindPrinter()' - Find a printer by resource...
//

pappl_printer_t *			// O - Printer or `NULL` if none
papplSystemFindPrinter(
    pappl_system_t *system,		// I - System
    const char     *resource,		// I - Resource path or `NULL`
    int            printer_id)		// I - Printer ID or `0`
{
  pappl_printer_t	*printer;	// Matching printer


  papplLog(system, PAPPL_LOGLEVEL_DEBUG, "papplSystemFindPrinter(system, resource=\"%s\", printer_id=%d)", resource, printer_id);

  pthread_rwlock_rdlock(&system->rwlock);

  if (resource && (!strcmp(resource, "/") || !strcmp(resource, "/ipp/print") || (!strncmp(resource, "/ipp/print/", 11) && isdigit(resource[11] & 255))))
  {
    printer_id = system->default_printer_id;
    resource   = NULL;

    papplLog(system, PAPPL_LOGLEVEL_DEBUG, "papplSystemFindPrinter: Looking for default printer_id=%d", printer_id);
  }

  for (printer = (pappl_printer_t *)cupsArrayFirst(system->printers); printer; printer = (pappl_printer_t *)cupsArrayNext(system->printers))
  {
    papplLog(system, PAPPL_LOGLEVEL_DEBUG, "papplSystemFindPrinter: printer '%s' - resource=\"%s\", printer_id=%d", printer->name, printer->resource, printer->printer_id);

    if (resource && !strncmp(printer->resource, resource, printer->resourcelen) && (!resource[printer->resourcelen] || resource[printer->resourcelen] == '/'))
      break;
    else if (printer->printer_id == printer_id)
      break;
  }
  pthread_rwlock_unlock(&system->rwlock);

  papplLog(system, PAPPL_LOGLEVEL_DEBUG, "papplSystemFindPrinter: Returning %p(%s)", printer, printer ? printer->name : "none");

  return (printer);
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


//
// 'compare_printers()' - Compare two printers.
//

static int				// O - Result of comparison
compare_printers(pappl_printer_t *a,	// I - First printer
                 pappl_printer_t *b)	// I - Second printer
{
  return (strcmp(a->name, b->name));
}


//
// 'free_printer()' - Free the memory used by a printer.
//

static void
free_printer(pappl_printer_t *printer)	// I - Printer
{
  // Remove DNS-SD registrations...
  _papplPrinterUnregisterDNSSDNoLock(printer);

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

  ippDelete(printer->driver_attrs);
  ippDelete(printer->attrs);

  cupsArrayDelete(printer->active_jobs);
  cupsArrayDelete(printer->completed_jobs);
  cupsArrayDelete(printer->all_jobs);

  cupsArrayDelete(printer->links);

  free(printer);
}

