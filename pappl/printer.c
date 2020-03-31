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
// 'papplPrinterCreate()' - Create a new printer.
//

pappl_printer_t *			// O - Printer
papplPrinterCreate(
    pappl_system_t *system,		// I - System
    int            printer_id,		// I - printer-id value or 0 for new
    const char     *printer_name,	// I - Printer name
    const char     *driver_name,	// I - Driver name
    const char     *device_uri)		// I - Device URI
{
  pappl_printer_t	*printer;	// Printer
  char			resource[1024],	// Resource path
			uuid[128],	// printer-uuid
			print_group[65],// print-group value
			fw_name[256],	// printer-firmware-name value
			fw_sversion[256];
					// pritner-firmware-string-version value
  unsigned short	fw_version[4];	// printer-firmware-version value
  int			k_supported;	// Maximum file size supported
  struct statfs		spoolinfo;	// FS info for spool directory
  double		spoolsize;	// FS size
  char			path[256];	// Path to resource
  pappl_driver_data_t	driver_data;	// Driver data
  ipp_t			*driver_attrs;	// Driver attributes
  static const char * const ipp_versions[] =
  {					// ipp-versions-supported values
    "1.1",
    "2.0"
  };
  static const char * const ipp_features[] =
  {					// ipp-features-supported values
    "ipp-everywhere"
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
#if 0
  static const char * const printer_strings_languages[] =
  {					// printer-strings-languages-supported values
    "de",
    "en",
    "es",
    "fr",
    "it"
  };
#endif // 0
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
    return (NULL);
 
  if (!system->driver_cb)
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
  snprintf(resource, sizeof(resource), "/ipp/print/%s", printer_name);

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

  printer->system         = system;
  printer->name           = strdup(printer_name);
  printer->resource       = strdup(resource);
  printer->resourcelen    = strlen(resource);
  printer->device_uri     = strdup(device_uri);
  printer->driver_name    = strdup(driver_name);
  printer->attrs          = ippNew();
  printer->start_time     = time(NULL);
  printer->config_time    = printer->start_time;
  printer->state          = IPP_PSTATE_IDLE;
  printer->state_reasons  = PAPPL_PREASON_NONE;
  printer->state_time     = printer->start_time;
  printer->all_jobs       = cupsArrayNew3((cups_array_func_t)compare_all_jobs, NULL, NULL, 0, NULL, (cups_afree_func_t)_papplJobDelete);
  printer->active_jobs    = cupsArrayNew((cups_array_func_t)compare_active_jobs, NULL);
  printer->completed_jobs = cupsArrayNew((cups_array_func_t)compare_completed_jobs, NULL);
  printer->next_job_id    = 1;

  if (papplSystemGetDefaultPrintGroup(system, print_group, sizeof(print_group)))
    papplPrinterSetPrintGroup(printer, print_group);

  // Initialize driver...
  driver_attrs = NULL;
  memset(&driver_data, 0, sizeof(driver_data));

  if (!(system->driver_cb)(system, driver_name, device_uri, &driver_data, &driver_attrs, system->driver_cbdata))
  {
    free_printer(printer);
    return (NULL);
  }

  papplPrinterSetDriverData(printer, &driver_data, driver_attrs);
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

  // ipp-features-supported
  ippAddStrings(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "ipp-features-supported", sizeof(ipp_features) / sizeof(ipp_features[0]), NULL, ipp_features);

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

  // orientation-requested-default
  ippAddInteger(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_ENUM, "orientation-requested-default", IPP_ORIENT_NONE);

  // orientation-requested-supported
  ippAddIntegers(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_ENUM, "orientation-requested-supported", (int)(sizeof(orientation_requested) / sizeof(orientation_requested[0])), orientation_requested);

  // pdl-override-supported
  ippAddString(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "pdl-override-supported", NULL, "attempted");

  // print-content-optimize-default
  ippAddString(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "print-content-optimize-default", NULL, "auto");

  // print-content-optimize-supported
  ippAddStrings(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "print-content-optimize-supported", (int)(sizeof(print_content_optimize) / sizeof(print_content_optimize[0])), NULL, print_content_optimize);

  // print-quality-default
  ippAddInteger(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_ENUM, "print-quality-default", IPP_QUALITY_NORMAL);

  // print-quality-supported
  ippAddIntegers(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_ENUM, "print-quality-supported", (int)(sizeof(print_quality) / sizeof(print_quality[0])), print_quality);

  // printer-firmware-name
  if (!papplSystemGetFirmware(system, fw_name, sizeof(fw_name), fw_sversion, sizeof(fw_sversion), fw_version))
    strlcpy(fw_name, "Unknown", sizeof(fw_name));

  ippAddString(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_NAME, "printer-firmware-name", NULL, fw_name);

  // printer-firmware-patches
  ippAddString(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_NAME), "printer-firmware-patches", NULL, "");

  // printer-firmware-string-version
  ippAddString(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_NAME, "printer-firmware-string-version", NULL, fw_sversion);

  // printer-firmware-version
  ippAddOctetString(printer->attrs, IPP_TAG_PRINTER, "printer-firmware-version", fw_version, (int)sizeof(fw_version));

  // printer-get-attributes-supported
  ippAddString(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "printer-get-attributes-supported", NULL, "document-format");

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

  // Add icons...
  _papplSystemAddPrinterIcons(system, printer);

  // Add web pages, if any...
  if (system->options & PAPPL_SOPTIONS_STANDARD)
  {
    snprintf(path, sizeof(path), "/config/%d", printer->printer_id);
    papplSystemAddResourceCallback(system, /* label */NULL, path, "text/html", (pappl_resource_cb_t)_papplPrinterWebConfig, printer);

    snprintf(path, sizeof(path), "/contact/%d", printer->printer_id);
    papplSystemAddResourceCallback(system, /* label */NULL, path, "text/html", (pappl_resource_cb_t)_papplPrinterWebContact, printer);

    snprintf(path, sizeof(path), "/defaults/%d", printer->printer_id);
    papplSystemAddResourceCallback(system, /* label */NULL, path, "text/html", (pappl_resource_cb_t)_papplPrinterWebDefaults, printer);

    snprintf(path, sizeof(path), "/media/%d", printer->printer_id);
    papplSystemAddResourceCallback(system, /* label */NULL, path, "text/html", (pappl_resource_cb_t)_papplPrinterWebMedia, printer);

    snprintf(path, sizeof(path), "/status/%d", printer->printer_id);
    papplSystemAddResourceCallback(system, /* label */NULL, path, "text/html", (pappl_resource_cb_t)_papplPrinterWebStatus, printer);

    snprintf(path, sizeof(path), "/supplies/%d", printer->printer_id);
    papplSystemAddResourceCallback(system, /* label */NULL, path, "text/html", (pappl_resource_cb_t)_papplPrinterWebSupplies, printer);
  }

  // Return it!
  return (printer);
}


//
// 'papplPrinterDelete()' - Delete a printer.
//

void
papplPrinterDelete(pappl_printer_t *printer)	/* I - Printer */
{
  // Remove the printer from the system object...
  pthread_rwlock_wrlock(&printer->system->rwlock);
  cupsArrayRemove(printer->system->printers, printer);
  pthread_rwlock_unlock(&printer->system->rwlock);
}


//
// 'papplSystemFindPrinter()' - Find a printer by resource...
//

pappl_printer_t *			// O - Printer or `NULL` if none
papplSystemFindPrinter(
    pappl_system_t *system,		// I - System
    const char      *resource,		// I - Resource path or `NULL`
    int             printer_id)		// I - Printer ID or `0`
{
  pappl_printer_t	*printer;	// Matching printer


  papplLog(system, PAPPL_LOGLEVEL_DEBUG, "papplSystemFindPrinter(system, resource=\"%s\", printer_id=%d)", resource, printer_id);

  pthread_rwlock_rdlock(&system->rwlock);

  if (resource && (!strcmp(resource, "/ipp/print") || (!strncmp(resource, "/ipp/print/", 11) && isdigit(resource[11] & 255))))
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
  free(printer->device_uri);
  free(printer->driver_name);

  ippDelete(printer->driver_attrs);
  ippDelete(printer->attrs);

  cupsArrayDelete(printer->active_jobs);
  cupsArrayDelete(printer->completed_jobs);
  cupsArrayDelete(printer->all_jobs);

  free(printer);
}

