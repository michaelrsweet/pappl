//
// Scan Job functions for the Scanner Application Framework
//
// Copyright © 2020-2024 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#include "pappl-private.h"

// Internal Helper Functions
//
// Helper functions for scanner option handling
//

//
// '_papplValidateDocumentFormat()' - Validate document format against supported formats.
//
static bool // O - TRUE if valid, FALSE if not
_papplValidateDocumentFormat(
  const char *format,     // I - Format to validate
  const char **supported, // I - Array of supported formats
  size_t num_supported)   // I - Number of supported formats
{
  size_t i;

  if (!format || !supported || num_supported == 0)
  return (false);

  for (i = 0; i < num_supported && supported[i]; i++)
  {
  if (!strcmp(format, supported[i]))
    return (true);
  }

  return (false);
}

//
// '_papplValidateScanResolution()' - Validate scan resolution against supported values.
//
static bool // O - TRUE if valid, FALSE if not
_papplValidateScanResolution(
  int resolution,       // I - Resolution to validate
  const int *supported, // I - Array of supported resolutions
  size_t num_supported) // I - Number of supported resolutions
{
  size_t i; // Looping var

  if (resolution <= 0 || !supported || num_supported == 0)
  return (false);

  for (i = 0; i < num_supported && supported[i] > 0; i++)
  {
  if (resolution == supported[i])
    return (true);
  }

  return (false);
}

//
// '_papplValidateScanRegion()' - Validate scan region against supported dimensions.
//
static bool // O - TRUE if valid, FALSE if not
_papplValidateScanRegion(
  int x,                  // I - X offset
  int y,                  // I - Y offset
  int w,                  // I - Width
  int h,                  // I - Height
  const int supported[4]) // I - Supported region values [x,y,w,h]
{
  if (!supported)
  return (false);

  return (x >= supported[0] && y >= supported[1] &&
    w <= supported[2] && h <= supported[3] &&
    w > 0 && h > 0);
}

// End of helper functions




//
// 'papplJobGetScanner()' - Get the scanner for the job.
//
// This function returns the scanner containing the job.
//

pappl_scanner_t *                    // O - Scanner
papplJobGetScanner(pappl_job_t *job) // I - Job
{
  pappl_scanner_t *ret = NULL; // Return value

  if (job)
  {
  _papplRWLockRead(job);
  ret = job->scanner;
  _papplRWUnlock(job);
  }

  return (ret);
}

//
// 'papplJobDeleteScanOptions()' - Delete a job options structure.
//
// This function frees the memory used for a job options structure.
//

void papplJobDeleteScanOptions(
  pappl_sc_options_t *options) // I - Scan options
{
  if (options)
  {
  free(options);
  }
}


//
// 'papplJobCreateScanOptions()' - Create the scanner options for a job.
//
// This function allocates a scanner options structure and computes the scan
// options for a job based upon the job configuration and default values
// set in the scanner driver data.
//

pappl_sc_options_t * // O - Job options data or `NULL` on error
papplJobCreateScanOptions(
  pappl_job_t *job) // I - Job
{
  pappl_sc_options_t *options; // New options data
  pappl_scanner_t *scanner;    // Scanner
  int i;                       // Looping var
  bool mode_supported,         // Is color mode supported?
  source_supported,        // Is source supported?
  intent_supported;        // Is intent supported?

  if (!job)
  return (NULL);

  scanner = (pappl_scanner_t *)job->printer;
  if (!scanner)
  return (NULL);

  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "Getting scan options for job");

  // Clear all options...
  if ((options = calloc(1, sizeof(pappl_sc_options_t))) == NULL)
  return (NULL);

  _papplRWLockRead(scanner);

  // Document format - Validate against supported formats
  if (_papplValidateDocumentFormat(scanner->driver_data.default_document_format,
           scanner->driver_data.document_formats_supported,
           PAPPL_MAX_FORMATS))
  {
  papplCopyString(options->document_format,
      scanner->driver_data.default_document_format,
      sizeof(options->document_format));
  }
  else if (scanner->driver_data.document_formats_supported[0])
  {
  // Default to first supported format if default is invalid
  papplCopyString(options->document_format,
      scanner->driver_data.document_formats_supported[0],
      sizeof(options->document_format));
  }
  else
  {
  // Fallback to a safe default
  papplCopyString(options->document_format, "application/pdf", sizeof(options->document_format));
  }

  // Color mode - Validate and set
  mode_supported = false;
  for (i = 0; i < PAPPL_MAX_COLOR_MODES; i++)
  {
  if (scanner->driver_data.color_modes_supported[i] == scanner->driver_data.default_color_mode)
  {
    mode_supported = true;
    break;
  }
  }
  options->color_mode = mode_supported ? scanner->driver_data.default_color_mode : PAPPL_BLACKANDWHITE1;

  // Resolution - Validate against supported resolutions
  if (_papplValidateScanResolution(scanner->driver_data.default_resolution,
           scanner->driver_data.resolutions,
           MAX_RESOLUTIONS))
  {
  options->resolution = scanner->driver_data.default_resolution;
  }
  else if (scanner->driver_data.resolutions[0] > 0)
  {
  options->resolution = scanner->driver_data.resolutions[0];
  }
  else
  {
  options->resolution = 300; // Safe default
  }

  // Input source - Validate against supported sources
  source_supported = false;
  for (i = 0; i < PAPPL_MAX_SOURCES; i++)
  {
  if (scanner->driver_data.input_sources_supported[i] == scanner->driver_data.default_input_source)
  {
    source_supported = true;
    break;
  }
  }
  options->input_source = source_supported ? scanner->driver_data.default_input_source : PAPPL_FLATBED;

  // Duplex - Only enable if supported
  options->duplex = scanner->driver_data.duplex_supported ? false : false;

  // Scan intent - Validate against supported intents
  intent_supported = false;
  for (i = 0; scanner->driver_data.mandatory_intents[i] && i < 5; i++)
  {
  if (!strcmp(scanner->driver_data.default_intent, scanner->driver_data.mandatory_intents[i]))
  {
    intent_supported = true;
    break;
  }
  }
  if (intent_supported)
  {
  papplCopyString(options->intent,
      scanner->driver_data.default_intent,
      sizeof(options->intent));
  }
  else if (scanner->driver_data.mandatory_intents[0])
  {
  papplCopyString(options->intent,
      scanner->driver_data.mandatory_intents[0],
      sizeof(options->intent));
  }
  else
  {
  papplCopyString(options->intent, "document", sizeof(options->intent));
  }

  // Scan area - Validate against supported dimensions
  if (_papplValidateScanRegion(0, 0,
         scanner->driver_data.default_scan_area[0],
         scanner->driver_data.default_scan_area[1],
         scanner->driver_data.scan_region_supported))
  {
  options->scan_area.width = scanner->driver_data.default_scan_area[0];
  options->scan_area.height = scanner->driver_data.default_scan_area[1];
  options->scan_area.x_offset = 0;
  options->scan_area.y_offset = 0;
  }
  else
  {
  // Use maximum supported dimensions if defaults are invalid
  options->scan_area.width = scanner->driver_data.scan_region_supported[2];
  options->scan_area.height = scanner->driver_data.scan_region_supported[3];
  options->scan_area.x_offset = scanner->driver_data.scan_region_supported[0];
  options->scan_area.y_offset = scanner->driver_data.scan_region_supported[1];
  }

  // Image adjustments
  options->adjustments.brightness = scanner->driver_data.adjustments.brightness;
  options->adjustments.contrast = scanner->driver_data.adjustments.contrast;
  options->adjustments.gamma = scanner->driver_data.adjustments.gamma;
  options->adjustments.threshold = scanner->driver_data.adjustments.threshold;
  options->adjustments.saturation = scanner->driver_data.adjustments.saturation;
  options->adjustments.sharpness = scanner->driver_data.adjustments.sharpness;

  // Processing options - Set based on capability
  options->blank_page_removal = false;
  options->compression_factor = 0;
  options->noise_removal = false;
  options->sharpening = false;

  // Number of pages - Based on input source
  options->num_pages = (options->input_source == PAPPL_ADF) ? 0 : 1;

  // Log all options
  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "document-format='%s'", options->document_format);
  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "color-mode=%d", options->color_mode);
  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "resolution=%ddpi", options->resolution);
  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "input-source=%d", options->input_source);
  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "duplex=%s", options->duplex ? "true" : "false");
  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "intent='%s'", options->intent);
  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "scan-area=[%d,%d,%d,%d]",
    options->scan_area.x_offset, options->scan_area.y_offset,
    options->scan_area.width, options->scan_area.height);
  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "adjustments.brightness=%d", options->adjustments.brightness);
  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "adjustments.contrast=%d", options->adjustments.contrast);
  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "adjustments.gamma=%d", options->adjustments.gamma);
  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "adjustments.threshold=%d", options->adjustments.threshold);
  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "adjustments.saturation=%d", options->adjustments.saturation);
  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "adjustments.sharpness=%d", options->adjustments.sharpness);
  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "blank-page-removal=%s", options->blank_page_removal ? "true" : "false");
  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "compression-factor=%d", options->compression_factor);
  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "noise-removal=%s", options->noise_removal ? "true" : "false");
  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "sharpening=%s", options->sharpening ? "true" : "false");
  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "num-pages=%u", options->num_pages);

  _papplRWUnlock(scanner);

  return (options);
}

//
// 'papplScanJobCreate()' - Create a new/existing scan job object.
//

pappl_job_t * // O - Job
papplScanJobCreate(
  pappl_scanner_t *scanner, // I - Scanner
  int job_id,               // I - Job ID or `0` for new job
  const char *username,     // I - Username
  const char *format,       // I - Document format
  const char *job_name)     // I - Job name
{
  pappl_job_t *job;   // Job
  char job_uri[1024]; // job-uri value
  char job_uuid[64];  // job-uuid value

  if (!scanner || !username || !job_name)
  return (NULL);

  // Check if scanner is accepting jobs
  _papplRWLockWrite(scanner);

  if (!scanner->is_accepting)
  {
  _papplRWUnlock(scanner);
  return (NULL);
  }

  // Allocate and initialize the job object
  if ((job = calloc(1, sizeof(pappl_job_t))) == NULL)
  {
  papplLog(scanner->system, PAPPL_LOGLEVEL_ERROR, "Unable to allocate memory for job: %s", strerror(errno));
  _papplRWUnlock(scanner);
  return (NULL);
  }

  pthread_rwlock_init(&job->rwlock, NULL);

  // Initialize basic job properties
  job->system = scanner->system;
  job->format = format ? format : scanner->driver_data.default_document_format;
  job->name = job_name;
  job->username = username;
  job->state = ESCL_SSTATE_IDLE;
  job->created = time(NULL);
  job->fd = -1;

  // Set job ID
  job->job_id = job_id > 0 ? job_id : scanner->next_job_id++;

  // Create HTTPS URI for eSCL
  httpAssembleURIf(HTTP_URI_CODING_ALL, job_uri, sizeof(job_uri),
       "https", NULL, scanner->system->hostname, scanner->system->port,
       "%s/ScanJobs/%d", scanner->resource, job->job_id);

  // Create job UUID
  _papplSystemMakeUUID(scanner->system, scanner->name, job->job_id, job_uuid,
       sizeof(job_uuid));

  // Set job times
  job->processing = 0;
  job->completed = 0;


  // Add event and update system configuration
  papplSystemAddEvent(scanner->system, NULL, job, PAPPL_EVENT_JOB_CREATED, NULL);
  _papplSystemConfigChanged(scanner->system);

  _papplRWUnlock(scanner);

  papplLogJob(job, PAPPL_LOGLEVEL_INFO, "Created scan job %d.", job->job_id);

  return (job);
}

static bool                 // O - `true` on success, `false` otherwise
start_job(pappl_job_t *job) // I - Job
{
  bool ret = false; // Return value
  pappl_scanner_t *scanner = job->scanner;
  // Scanner
  bool first_open = true; // Is this the first time we try to open the device?

  // Move the job to the 'processing' state...
  _papplRWLockWrite(scanner);
  _papplRWLockWrite(job);

  papplLogJob(job, PAPPL_LOGLEVEL_INFO, "Starting scan job.");

  job->state = IPP_JSTATE_PROCESSING;
  job->processing = time(NULL);
  scanner->processing_job = job;

  // First event call is correct
  _papplSystemAddEventNoLock(scanner->system, NULL, scanner, job, PAPPL_EVENT_JOB_STATE_CHANGED, NULL);

  _papplRWUnlock(job);

  // Open the output device...
  if (scanner->device_in_use)
  {
  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "Waiting for device to become available.");

  while (scanner->device_in_use && !scanner->is_deleted && !job->is_canceled && papplSystemIsRunning(scanner->system))
  {
    _papplRWUnlock(scanner);
    sleep(1);
    _papplRWLockWrite(scanner);
  }
  }

  while (!scanner->device && !scanner->is_deleted && !job->is_canceled && papplSystemIsRunning(scanner->system))
  {
  papplLogScanner(scanner, PAPPL_LOGLEVEL_DEBUG, "Opening device for job %d.", job->job_id);

  scanner->device = papplDeviceOpen(scanner->device_uri, job->name, papplLogDevice, job->system);

  if (!scanner->device && !scanner->is_deleted && !job->is_canceled)
  {
    // Log that the printer is unavailable then sleep for 5 seconds to retry.
    if (first_open)
    {
    papplLogScanner(scanner, PAPPL_LOGLEVEL_ERROR, "Unable to open device '%s', pausing until scanner becomes available.", scanner->device_uri);
    first_open = false;

    scanner->state = ESCL_SSTATE_STOPPED;
    scanner->state_time = time(NULL);
    }
    else
    {
    papplLogScanner(scanner, PAPPL_LOGLEVEL_DEBUG, "Still unable to open device.");
    }

    _papplRWUnlock(scanner);
    sleep(5);
    _papplRWLockWrite(scanner);
  }
  }

  if (!papplSystemIsRunning(scanner->system))
  {
  job->state = IPP_JSTATE_PENDING;

  _papplRWLockRead(job);
  // Corrected event call: pass `job` as the fourth argument and event as the fifth
  _papplSystemAddEventNoLock(job->system, NULL, job->scanner, job, PAPPL_EVENT_JOB_STATE_CHANGED, NULL);
  _papplRWUnlock(job);

  if (scanner->device)
  {
    papplDeviceClose(scanner->device);
    scanner->device = NULL;
  }
  }

  if (scanner->device)
  {
  // Move the scanner to the 'processing' state...
  scanner->state = ESCL_SSTATE_PROCESSING;
  scanner->state_time = time(NULL);
  ret = true;
  }

  _papplSystemAddEventNoLock(scanner->system, NULL, scanner, NULL, PAPPL_EVENT_SCANNER_STATE_CHANGED, NULL);

  _papplRWUnlock(scanner);

  return (ret);
}

static void
finish_job(pappl_job_t *job) // I - Job
{
  pappl_scanner_t *scanner = job->scanner;
  // Scanner
  static const char *const job_states[] =
  {
    "Pending",
    "Held",
    "Processing",
    "Canceled",
    "Aborted",
    "Completed"};

  _papplRWLockWrite(scanner);
  _papplRWLockWrite(job);

  if (job->is_canceled)
  job->state = IPP_JSTATE_CANCELED;
  else if (job->state == IPP_JSTATE_PROCESSING)
  job->state = IPP_JSTATE_COMPLETED;

  // Ensure job->state is within the bounds of job_states array
  if ((job->state - IPP_JSTATE_PENDING) >= 0 &&
  (job->state - IPP_JSTATE_PENDING) < (sizeof(job_states) / sizeof(job_states[0])))
  {
  papplLogJob(job, PAPPL_LOGLEVEL_INFO, "%s, job-impressions-completed=%d.",
      job_states[job->state - IPP_JSTATE_PENDING],
      job->impcompleted);
  }
  else
  {
  papplLogJob(job, PAPPL_LOGLEVEL_WARN, "Unknown job state: %d, job-impressions-completed=%d.",
      job->state, job->impcompleted);
  }

  if (job->state >= IPP_JSTATE_CANCELED)
  job->completed = time(NULL);

  _papplJobSetRetain(job);

  scanner->processing_job = NULL;


  // Corrected Call: Ensure fifth argument is the event, and sixth is the message
  _papplSystemAddEventNoLock(scanner->system, NULL, scanner, job, PAPPL_EVENT_JOB_COMPLETED, NULL);

  if (scanner->is_stopped)
  {
  // New scanner-state is 'stopped'...
  scanner->state = ESCL_SSTATE_STOPPED;
  scanner->is_stopped = false;
  }
  else
  {
  // New scanner-state is 'idle'...
  scanner->state = ESCL_SSTATE_IDLE;
  }

  scanner->state_time = time(NULL);

  if (!job->system->clean_time)
  job->system->clean_time = time(NULL) + 60;

  _papplRWUnlock(job);

  // Corrected Call: Ensure fifth argument is the event, and sixth is the message
  _papplSystemAddEventNoLock(scanner->system, NULL, scanner, NULL, PAPPL_EVENT_SCANNER_STATE_CHANGED, NULL);
  // ^ Changed event type to PAPPL_EVENT_SCANNER_STATE_CHANGED

  _papplRWUnlock(scanner);

  _papplSystemConfigChanged(scanner->system);

  if (papplScannerIsDeleted(scanner))
  {
  papplScannerDelete(scanner);
  return;
  }
  else if (!strncmp(scanner->device_uri, "file:", 5))
  {
  pappl_devmetrics_t metrics; // Metrics for device IO

  _papplRWLockWrite(scanner);

  papplDeviceGetMetrics(scanner->device, &metrics);
  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "Device read metrics: %lu requests, %lu bytes, %lu msecs",
      (unsigned long)metrics.read_requests,
      (unsigned long)metrics.read_bytes,
      (unsigned long)metrics.read_msecs);
  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "Device write metrics: %lu requests, %lu bytes, %lu msecs",
      (unsigned long)metrics.write_requests,
      (unsigned long)metrics.write_bytes,
      (unsigned long)metrics.write_msecs);

  papplLogScanner(scanner, PAPPL_LOGLEVEL_DEBUG, "Closing device for job %d.", job->job_id);

  papplDeviceClose(scanner->device);
  scanner->device = NULL;

  _papplRWUnlock(scanner);
  }
}

//
// '_papplScanJobProcess()' - Process a scan job.
//

void *                             // O - Thread exit status
_papplScanJobProcess(pappl_job_t *job) // I - Job
{

  // Start processing the job...
  start_job(job);

  // Move the job to a completed state...
  finish_job(job);

  return (NULL);
}