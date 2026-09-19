//
// Scan job processing for the Printer Application Framework
//
// Copyright © 2019-2026 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#include "pappl-private.h"


//
// Local functions...
//

static void	finish_scan_job(pappl_job_t *job);
static bool	start_scan_job(pappl_job_t *job);
static const char *scan_color_mode_string(pappl_scan_color_mode_t value);
static const char *scan_content_string(pappl_scan_content_t value);
static const char *scan_input_source_string(pappl_scan_input_source_t value);
static const char *scan_intent_string(pappl_scan_intent_t value);


static const char *	scan_color_mode_string(
    pappl_scan_color_mode_t value)	// I - Color mode bit value
{
  switch (value)
  {
    case PAPPL_SCAN_COLOR_MODE_BLACK_AND_WHITE_1 :
        return ("black-and-white");
    case PAPPL_SCAN_COLOR_MODE_GRAYSCALE_8 :
        return ("grayscale");
    case PAPPL_SCAN_COLOR_MODE_GRAYSCALE_16 :
        return ("grayscale16");
    case PAPPL_SCAN_COLOR_MODE_RGB_24 :
        return ("color");
    case PAPPL_SCAN_COLOR_MODE_RGB_48 :
        return ("color48");
    default :
        return (NULL);
  }
}


static const char *	scan_content_string(
    pappl_scan_content_t value)		// I - Content type bit value
{
  switch (value)
  {
    case PAPPL_SCAN_CONTENT_AUTO :
        return ("auto");
    case PAPPL_SCAN_CONTENT_HALFTONE :
        return ("halftone");
    case PAPPL_SCAN_CONTENT_LINE_ART :
        return ("line-art");
    case PAPPL_SCAN_CONTENT_MAGAZINE :
        return ("magazine");
    case PAPPL_SCAN_CONTENT_PHOTO :
        return ("photo");
    case PAPPL_SCAN_CONTENT_TEXT :
        return ("text");
    case PAPPL_SCAN_CONTENT_TEXT_AND_PHOTO :
        return ("text-and-photo");
    default :
        return (NULL);
  }
}


static const char *	scan_input_source_string(
    pappl_scan_input_source_t value)	// I - Input source bit value
{
  switch (value)
  {
    case PAPPL_SCAN_INPUT_SOURCE_PLATEN :
        return ("platen");
    case PAPPL_SCAN_INPUT_SOURCE_ADF :
        return ("adf");
    case PAPPL_SCAN_INPUT_SOURCE_CAMERA :
        return ("camera");
    default :
        return (NULL);
  }
}


static const char *	scan_intent_string(
    pappl_scan_intent_t value)		// I - Intent bit value
{
  switch (value)
  {
    case PAPPL_SCAN_INTENT_DOCUMENT :
        return ("document");
    case PAPPL_SCAN_INTENT_PHOTO :
        return ("photo");
    case PAPPL_SCAN_INTENT_PREVIEW :
        return ("preview");
    case PAPPL_SCAN_INTENT_TEXT_AND_GRAPHIC :
        return ("text-and-graphic");
    case PAPPL_SCAN_INTENT_BUSINESS_CARD :
        return ("business-card");
    default :
        return (NULL);
  }
}


//
// 'papplJobCreateScanOptions()' - Create the scan options for a job.
//
// This function allocates a scan options structure and returns a copy of the
// eSCL scan options stored on the job.  The caller is responsible for freeing
// the options using the @link papplJobDeleteScanOptions@ function.
//

pappl_sc_options_t *			// O - Scan options or `NULL` on error
papplJobCreateScanOptions(
    pappl_job_t *job)			// I - Job
{
  pappl_sc_options_t	*options;	// New options data
  pappl_scanner_t	*scanner;	// Scanner


  // Range check input...
  if (!job || !job->scanner)
    return (NULL);

  scanner = job->scanner;

  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "Getting scan options.");

  // Allocate options...
  if ((options = calloc(1, sizeof(pappl_sc_options_t))) == NULL)
    return (NULL);

  _papplRWLockRead(scanner);

  // Copy the canonical eSCL scan options stored on the job...
  if (job->scan_options)
  {
    *options = *job->scan_options;
  }
  else
  {
    // Defensive fallback to driver defaults...
    options->intent       = scanner->driver_data.intent_default;
    options->input_source = scanner->driver_data.input_source_default;
    options->color_mode   = scanner->driver_data.color_default;
    options->content_type = scanner->driver_data.content_default;
    options->x_resolution = scanner->driver_data.x_default;
    options->y_resolution = scanner->driver_data.y_default;
  }

  // Apply defaults for scan region if needed...
  if (options->scan_width <= 0)
  {
    if (options->input_source == PAPPL_SCAN_INPUT_SOURCE_ADF &&
        scanner->driver_data.adf_max_width > 0)
      options->scan_width = scanner->driver_data.adf_max_width;
    else
      options->scan_width = scanner->driver_data.platen_max_width;
  }

  if (options->scan_height <= 0)
  {
    if (options->input_source == PAPPL_SCAN_INPUT_SOURCE_ADF &&
        scanner->driver_data.adf_max_height > 0)
      options->scan_height = scanner->driver_data.adf_max_height;
    else
      options->scan_height = scanner->driver_data.platen_max_height;
  }

  // Apply default format if empty...
  if (!options->format[0])
  {
    if (options->intent == PAPPL_SCAN_INTENT_PHOTO ||
        options->intent == PAPPL_SCAN_INTENT_PREVIEW)
      cupsCopyString(options->format, "image/jpeg", sizeof(options->format));
    else
      cupsCopyString(options->format, "application/pdf", sizeof(options->format));
  }

  // Apply default compression if not set...
  if (options->compression <= 0)
    options->compression = 80;

  _papplRWUnlock(scanner);

  // Log options...
  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "scan-intent=%d", options->intent);
  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "input-source=%d", options->input_source);
  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "scan-color-mode=%d", options->color_mode);
  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "scanner-resolution=%dx%ddpi", options->x_resolution, options->y_resolution);
  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "document-format='%s'", options->format);
  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "scan-region=%d,%d %dx%d", options->scan_x, options->scan_y, options->scan_width, options->scan_height);

  return (options);
}


//
// 'papplJobDeleteScanOptions()' - Free scan options.
//
// This function frees a scan options structure created by
// @link papplJobCreateScanOptions@.
//

void
papplJobDeleteScanOptions(
    pappl_sc_options_t *options)		// I - Scan options
{
  free(options);
}


//
// 'papplJobGetScanner()' - Get the scanner for a scan job.
//

pappl_scanner_t *			// O - Scanner or `NULL`
papplJobGetScanner(pappl_job_t *job)	// I - Job
{
  return (job ? job->scanner : NULL);
}


//
// '_papplJobCreateScan()' - Create a new scan job.
//
// This function creates a scan job on the specified scanner.  The job is
// created with the given options and is placed in the pending state.
//

pappl_job_t *				// O - Job or `NULL` on error
_papplJobCreateScan(
    pappl_scanner_t    *scanner,	// I - Scanner
    const char         *username,	// I - Username
    pappl_sc_options_t *options)		// I - Scan options
{
  pappl_job_t		*job;		// Job
  char			job_uri[1024],	// job-uri value
			job_uuid[64];	// job-uuid value


  // Range check input...
  if (!scanner || !options)
    return (NULL);

  _papplRWLockWrite(scanner);

  // Check limits...
  if (scanner->max_active_jobs > 0 &&
      cupsArrayGetCount(scanner->active_jobs) >= (size_t)scanner->max_active_jobs)
  {
    papplLog(scanner->system, PAPPL_LOGLEVEL_WARN,
             "Scanner '%s': max active jobs (%d) reached.",
             scanner->name, (int)scanner->max_active_jobs);
    _papplRWUnlock(scanner);
    return (NULL);
  }

  // Allocate and initialize the job object...
  if ((job = calloc(1, sizeof(pappl_job_t))) == NULL)
  {
    papplLog(scanner->system, PAPPL_LOGLEVEL_ERROR,
             "Unable to allocate memory for scan job: %s", strerror(errno));
    _papplRWUnlock(scanner);
    return (NULL);
  }

  cupsRWInit(&job->rwlock);
  cupsMutexInit(&job->proxy_mutex);

  job->attrs       = ippNew();
  job->fd          = -1;
  job->system      = scanner->system;
  job->scanner     = scanner;
  job->is_scan_job = true;
  job->created     = time(NULL);
  job->state       = IPP_JSTATE_PENDING;
  job->copies      = 1;

  if (!username)
    username = "anonymous";

  // Assign job ID and generate URIs...
  job->job_id     = scanner->next_job_id++;
  // Generate log prefix for scan jobs...
  {
    char prefix[256];

    if (scanner->system->options & PAPPL_SOPTIONS_MULTI_QUEUE)
      snprintf(prefix, sizeof(prefix), "[Job %s-%d]", scanner->name, job->job_id);
    else
      snprintf(prefix, sizeof(prefix), "[Job %d]", job->job_id);

    job->log_prefix = strdup(prefix);
  }

  httpAssembleURIf(HTTP_URI_CODING_ALL, job_uri, sizeof(job_uri), "ipps",
                   NULL, scanner->system->hostname, scanner->system->port,
                   "%s/%d", scanner->resource, job->job_id);

  _papplSystemMakeUUID(scanner->system, scanner->name, job->job_id,
                       job_uuid, sizeof(job_uuid));

  // Add job attributes...
  ippAddInteger(job->attrs, IPP_TAG_JOB, IPP_TAG_INTEGER, "job-id",
                job->job_id);
  job->name = ippGetString(ippAddString(job->attrs, IPP_TAG_JOB, IPP_TAG_NAME,
                           "job-name", NULL, "eSCL Scan Job"), 0, NULL);
  job->username = ippGetString(ippAddString(job->attrs, IPP_TAG_JOB,
                               IPP_TAG_NAME, "job-originating-user-name",
                               NULL, username), 0, NULL);
  job->uri = ippGetString(ippAddString(job->attrs, IPP_TAG_JOB, IPP_TAG_URI,
                          "job-uri", NULL, job_uri), 0, NULL);
  ippAddString(job->attrs, IPP_TAG_JOB, IPP_TAG_URI, "job-uuid", NULL,
               job_uuid);

  // Store the canonical eSCL scan options on the job.  This is the source of
  // truth used by papplJobCreateScanOptions() and _papplJobProcessScan().
  if ((job->scan_options = calloc(1, sizeof(pappl_sc_options_t))) != NULL)
    *job->scan_options = *options;

  // Store scan options as IPP attributes for introspection via
  // Get-Job-Attributes...
  ippAddString(job->attrs, IPP_TAG_JOB, IPP_TAG_KEYWORD, "scan-intent", NULL,
               scan_intent_string(options->intent));
  ippAddString(job->attrs, IPP_TAG_JOB, IPP_TAG_KEYWORD, "input-source", NULL,
               scan_input_source_string(options->input_source));
  ippAddString(job->attrs, IPP_TAG_JOB, IPP_TAG_KEYWORD, "scan-color-mode", NULL,
               scan_color_mode_string(options->color_mode));
  ippAddString(job->attrs, IPP_TAG_JOB, IPP_TAG_KEYWORD, "scan-content-type", NULL,
               scan_content_string(options->content_type));
  ippAddResolution(job->attrs, IPP_TAG_JOB, "scanner-resolution",
                   options->x_resolution, options->y_resolution,
                   IPP_RES_PER_INCH);
  ippAddString(job->attrs, IPP_TAG_JOB, IPP_TAG_MIMETYPE, "document-format",
               NULL, options->format);
  ippAddInteger(job->attrs, IPP_TAG_JOB, IPP_TAG_INTEGER,
                "scan-region-x-offset", options->scan_x);
  ippAddInteger(job->attrs, IPP_TAG_JOB, IPP_TAG_INTEGER,
                "scan-region-y-offset", options->scan_y);
  ippAddInteger(job->attrs, IPP_TAG_JOB, IPP_TAG_INTEGER,
                "scan-region-width", options->scan_width);
  ippAddInteger(job->attrs, IPP_TAG_JOB, IPP_TAG_INTEGER,
                "scan-region-height", options->scan_height);
  ippAddBoolean(job->attrs, IPP_TAG_JOB, "duplex", options->duplex);
  ippAddInteger(job->attrs, IPP_TAG_JOB, IPP_TAG_INTEGER,
                "brightness", options->brightness);
  ippAddInteger(job->attrs, IPP_TAG_JOB, IPP_TAG_INTEGER,
                "contrast", options->contrast);
  ippAddInteger(job->attrs, IPP_TAG_JOB, IPP_TAG_INTEGER,
                "compression", options->compression);
  ippAddInteger(job->attrs, IPP_TAG_JOB, IPP_TAG_INTEGER,
                "threshold", options->threshold);
  ippAddInteger(job->attrs, IPP_TAG_JOB, IPP_TAG_INTEGER,
                "sharpen", options->sharpen);

  // Add to job arrays...
  cupsArrayAdd(scanner->all_jobs, job);
  cupsArrayAdd(scanner->active_jobs, job);

  _papplRWUnlock(scanner);

  papplLog(scanner->system, PAPPL_LOGLEVEL_INFO,
           "Scanner '%s': Created scan job %d.", scanner->name, job->job_id);

  _papplSystemConfigChanged(scanner->system);

  return (job);
}


//
// '_papplScannerCheckJobsNoLock()' - Check for new scan jobs to process.
//
// This function must be called with the scanner write lock held.
//

void
_papplScannerCheckJobsNoLock(
    pappl_scanner_t *scanner)		// I - Scanner
{
  pappl_job_t	*job;			// Current job


  papplLog(scanner->system, PAPPL_LOGLEVEL_DEBUG,
           "Scanner '%s': Checking for new scan jobs.", scanner->name);

  if (scanner->device_in_use)
  {
    papplLog(scanner->system, PAPPL_LOGLEVEL_DEBUG,
             "Scanner '%s': Device is in use.", scanner->name);
    return;
  }
  else if (scanner->processing_job)
  {
    papplLog(scanner->system, PAPPL_LOGLEVEL_DEBUG,
             "Scanner '%s': Already processing job %d.", scanner->name,
             scanner->processing_job->job_id);
    return;
  }
  else if (scanner->is_deleted)
  {
    papplLog(scanner->system, PAPPL_LOGLEVEL_DEBUG,
             "Scanner '%s': Being deleted.", scanner->name);
    return;
  }
  else if (scanner->state == IPP_PSTATE_STOPPED)
  {
    papplLog(scanner->system, PAPPL_LOGLEVEL_DEBUG,
             "Scanner '%s': Stopped.", scanner->name);
    return;
  }

  // Enumerate jobs — we hold an exclusive lock so cupsArrayGetFirst/Next
  // is safe here...
  for (job = (pappl_job_t *)cupsArrayGetFirst(scanner->active_jobs);
       job;
       job = (pappl_job_t *)cupsArrayGetNext(scanner->active_jobs))
  {
    if (job->state == IPP_JSTATE_PENDING)
    {
      cups_thread_t t;			// Thread

      papplLog(scanner->system, PAPPL_LOGLEVEL_DEBUG,
               "Scanner '%s': Starting scan job %d.", scanner->name,
               job->job_id);

      if ((t = cupsThreadCreate((void *(*)(void *))_papplJobProcessScan, job)) == CUPS_THREAD_INVALID)
      {
        job->state     = IPP_JSTATE_ABORTED;
        job->completed = time(NULL);

        cupsArrayRemove(scanner->active_jobs, job);
        cupsArrayAdd(scanner->completed_jobs, job);

        papplLog(scanner->system, PAPPL_LOGLEVEL_ERROR,
                 "Scanner '%s': Unable to start scan thread for job %d.",
                 scanner->name, job->job_id);
      }
      else
      {
        cupsThreadDetach(t);
      }
      break;
    }
  }
}


//
// '_papplJobProcessScan()' - Process a scan job.
//
// This is the scan job processing thread.  It invokes driver callbacks to
// acquire scan data from the scanner hardware, writing each page to a
// temporary file in the system spool directory.
//

void *					// O - Thread exit status
_papplJobProcessScan(pappl_job_t *job)	// I - Job
{
  pappl_scanner_t	*scanner = job->scanner;
					// Scanner
  pappl_sc_options_t	*options = NULL;// Scan options
  unsigned		page = 0;	// Current page number
  unsigned		y;		// Current line
  unsigned char		*line = NULL;	// Line buffer
  size_t		linesize;	// Size of a single scan line
  int			width_pixels,	// Width in pixels
			height_pixels;	// Height in pixels
  int			fd = -1;	// File descriptor for page data
  char			filename[1024];	// Page data filename
  bool			more_pages;	// Are there more pages?


  // Start the scan job...
  if (!start_scan_job(job))
  {
    finish_scan_job(job);
    return (NULL);
  }

  // Create scan options...
  options = papplJobCreateScanOptions(job);
  if (!options)
  {
    papplLogJob(job, PAPPL_LOGLEVEL_ERROR, "Unable to create scan options.");
    job->state = IPP_JSTATE_ABORTED;
    finish_scan_job(job);
    return (NULL);
  }

  // Calculate line buffer size from scan region and color mode...
  width_pixels = (int)((double)options->scan_width * options->x_resolution / 300.0);
  if (width_pixels < 1)
    width_pixels = 1;

  height_pixels = (int)((double)options->scan_height * options->y_resolution / 300.0);
  if (height_pixels < 1)
    height_pixels = 1;

  // Determine bytes per pixel from color mode...
  switch (options->color_mode)
  {
    case PAPPL_SCAN_COLOR_MODE_BLACK_AND_WHITE_1 :
        linesize = (size_t)((width_pixels + 7) / 8);
        break;
    case PAPPL_SCAN_COLOR_MODE_GRAYSCALE_8 :
        linesize = (size_t)width_pixels;
        break;
    case PAPPL_SCAN_COLOR_MODE_GRAYSCALE_16 :
        linesize = (size_t)width_pixels * 2;
        break;
    case PAPPL_SCAN_COLOR_MODE_RGB_24 :
    default :
        linesize = (size_t)width_pixels * 3;
        break;
    case PAPPL_SCAN_COLOR_MODE_RGB_48 :
        linesize = (size_t)width_pixels * 6;
        break;
  }

  // Allocate line buffer...
  if ((line = malloc(linesize)) == NULL)
  {
    papplLogJob(job, PAPPL_LOGLEVEL_ERROR,
                "Unable to allocate line buffer (%u bytes).",
                (unsigned)linesize);
    job->state = IPP_JSTATE_ABORTED;
    papplJobDeleteScanOptions(options);
    finish_scan_job(job);
    return (NULL);
  }

  // Call the driver's start-job callback...
  if (!(scanner->driver_data.rstartjob_cb)(job, options, scanner->device))
  {
    papplLogJob(job, PAPPL_LOGLEVEL_ERROR, "Unable to start scan job.");
    job->state = IPP_JSTATE_ABORTED;
    free(line);
    papplJobDeleteScanOptions(options);
    finish_scan_job(job);
    return (NULL);
  }

  papplLogJob(job, PAPPL_LOGLEVEL_INFO,
              "Scanning: %dx%d pixels, %dx%d dpi, linesize=%u",
              width_pixels, height_pixels,
              options->x_resolution, options->y_resolution,
              (unsigned)linesize);

  // Loop through pages...
  more_pages = true;
  while (more_pages && !job->is_canceled)
  {
    page ++;

    // Call the driver's start-page callback — returns false when no more
    // pages are available (e.g. ADF exhausted)...
    if (!(scanner->driver_data.rstartpage_cb)(job, options, scanner->device, page))
    {
      page --;
      break;				// No more pages
    }

    papplLogJob(job, PAPPL_LOGLEVEL_INFO, "Scanning page %u...", page);

    // Create a temp file for this page's raw data in the spool directory...
    snprintf(filename, sizeof(filename), "%s/s%05dj%09dp%04d.raw",
             scanner->system->directory, scanner->scanner_id,
             job->job_id, page);

    if ((fd = open(filename, O_CREAT | O_WRONLY | O_TRUNC, 0600)) < 0)
    {
      papplLogJob(job, PAPPL_LOGLEVEL_ERROR,
                  "Unable to create scan data file '%s': %s",
                  filename, strerror(errno));
      job->state = IPP_JSTATE_ABORTED;
      break;
    }

    // Read scan lines from the driver...
    for (y = 0; y < (unsigned)height_pixels && !job->is_canceled; y ++)
    {
      memset(line, 0, linesize);

      if (!(scanner->driver_data.rreadline_cb)(job, options, scanner->device,
                                               y, line, linesize))
      {
        papplLogJob(job, PAPPL_LOGLEVEL_ERROR,
                    "Error reading scan line %u on page %u.", y, page);
        job->state = IPP_JSTATE_ABORTED;
        break;
      }

      // Write raw pixel data to the temp file...
      if (write(fd, line, linesize) < (ssize_t)linesize)
      {
        papplLogJob(job, PAPPL_LOGLEVEL_ERROR,
                    "Error writing scan data: %s", strerror(errno));
        job->state = IPP_JSTATE_ABORTED;
        break;
      }
    }

    close(fd);
    fd = -1;

    // Call the driver's end-page callback...
    (scanner->driver_data.rendpage_cb)(job, options, scanner->device, page);

    if (job->state == IPP_JSTATE_ABORTED || job->is_canceled)
      break;

    // Record that this page is ready for retrieval...
    _papplRWLockWrite(job);
    job->scan_pages_ready ++;
    job->impcompleted ++;
    _papplRWUnlock(job);

    papplLogJob(job, PAPPL_LOGLEVEL_INFO, "Page %u scanned (%u ready).",
                page, (unsigned)job->scan_pages_ready);

    // For platen, only one page...
    if (options->input_source == PAPPL_SCAN_INPUT_SOURCE_PLATEN)
      more_pages = false;
  }

  // Call the driver's end-job callback...
  (scanner->driver_data.rendjob_cb)(job, options, scanner->device);

  // Mark scanning as complete...
  _papplRWLockWrite(job);
  job->scan_complete = true;
  _papplRWUnlock(job);

  papplLogJob(job, PAPPL_LOGLEVEL_INFO,
              "Scan complete: %u page(s) scanned.", page);

  // Clean up...
  free(line);
  papplJobDeleteScanOptions(options);
  finish_scan_job(job);

  return (NULL);
}


//
// '_papplJobGetScanPageFile()' - Get the filename for a scanned page.
//
// This builds the spool file path for the specified page.  Returns `true` if
// the file exists and `false` otherwise.
//

bool					// O - `true` if file exists
_papplJobGetScanPageFile(
    pappl_job_t *job,			// I - Job
    int         page,			// I - Page number (1-based)
    char        *fname,			// I - Filename buffer
    size_t      fnamesize)		// I - Size of filename buffer
{
  if (!job || !job->scanner || page < 1 || !fname || fnamesize < 256)
  {
    if (fname)
      *fname = '\0';
    return (false);
  }

  snprintf(fname, fnamesize, "%s/s%05dj%09dp%04d.raw",
           job->system->directory, job->scanner->scanner_id,
           job->job_id, page);

  return (!access(fname, R_OK));
}


//
// '_papplJobRemoveScanFiles()' - Remove scan data files for a job.
//

void
_papplJobRemoveScanFiles(
    pappl_job_t *job)			// I - Job
{
  int	page;				// Page number
  char	filename[1024];			// Filename buffer


  if (!job || !job->scanner)
    return;

  for (page = 1; page <= job->scan_pages_ready; page ++)
  {
    snprintf(filename, sizeof(filename), "%s/s%05dj%09dp%04d.raw",
             job->system->directory, job->scanner->scanner_id,
             job->job_id, page);
    unlink(filename);
  }
}


//
// '_papplJobCancelScan()' - Cancel a scan job.
//
// Sets the cancel flag and waits for the processing thread to notice.
// If the job is pending (not yet processing), moves it to canceled directly.
//

void
_papplJobCancelScan(
    pappl_job_t *job)			// I - Job
{
  pappl_scanner_t *scanner;		// Scanner


  if (!job || !job->scanner)
    return;

  scanner = job->scanner;

  _papplRWLockWrite(scanner);
  _papplRWLockWrite(job);

  if (job->state == IPP_JSTATE_PROCESSING)
  {
    // Signal the processing thread to stop...
    job->is_canceled = true;
  }
  else if (job->state <= IPP_JSTATE_PENDING)
  {
    job->state     = IPP_JSTATE_CANCELED;
    job->completed = time(NULL);

    cupsArrayRemove(scanner->active_jobs, job);
    cupsArrayAdd(scanner->completed_jobs, job);

    _papplJobRemoveScanFiles(job);
  }

  _papplRWUnlock(job);
  _papplRWUnlock(scanner);

  if (!scanner->system->clean_time)
    scanner->system->clean_time = time(NULL) + 60;
}


//
// 'start_scan_job()' - Start processing a scan job.
//

static bool				// O - `true` on success, `false` otherwise
start_scan_job(pappl_job_t *job)	// I - Job
{
  bool			ret = false;	// Return value
  pappl_scanner_t	*scanner = job->scanner;
					// Scanner
  bool			first_open = true;
					// First time opening device?


  // Move the job to the 'processing' state...
  _papplRWLockWrite(scanner);
  _papplRWLockWrite(job);

  papplLogJob(job, PAPPL_LOGLEVEL_INFO, "Starting scan job.");

  job->state              = IPP_JSTATE_PROCESSING;
  job->processing         = time(NULL);
  scanner->processing_job = job;
  scanner->state          = IPP_PSTATE_PROCESSING;
  scanner->state_time     = time(NULL);

  _papplRWUnlock(job);

  // Open the output device...
  if (scanner->device_in_use)
  {
    papplLog(scanner->system, PAPPL_LOGLEVEL_DEBUG,
             "Scanner '%s': Waiting for device.", scanner->name);

    while (scanner->device_in_use && !scanner->is_deleted &&
           !job->is_canceled && papplSystemIsRunning(scanner->system))
    {
      _papplRWUnlock(scanner);
      sleep(1);
      _papplRWLockWrite(scanner);
    }
  }

  while (!scanner->device && !scanner->is_deleted && !job->is_canceled &&
         papplSystemIsRunning(scanner->system))
  {
    scanner->device = papplDeviceOpen(scanner->device_uri, job,
                                      papplLogDevice, job->system);

    if (!scanner->device && !scanner->is_deleted && !job->is_canceled)
    {
      // Log that the scanner is unavailable then sleep for 5 seconds to retry.
      if (first_open)
      {
        papplLog(scanner->system, PAPPL_LOGLEVEL_ERROR,
                 "Scanner '%s': Unable to open device '%s', retrying.",
                 scanner->name, scanner->device_uri);
        first_open = false;

        scanner->state      = IPP_PSTATE_STOPPED;
        scanner->state_time = time(NULL);
      }

      _papplRWUnlock(scanner);
      sleep(5);
      _papplRWLockWrite(scanner);
    }
  }

  if (scanner->device)
  {
    scanner->state      = IPP_PSTATE_PROCESSING;
    scanner->state_time = time(NULL);
    ret                 = true;
  }

  _papplRWUnlock(scanner);

  if (!ret)
    papplLogJob(job, PAPPL_LOGLEVEL_ERROR, "Unable to open scanner device.");

  return (ret);
}


//
// 'finish_scan_job()' - Finish scan job processing.
//
// Note: This does NOT move the job to completed immediately.  Scan jobs remain
// in the active list until all pages have been retrieved by the client (via
// NextDocument) OR the job is explicitly canceled/deleted.
//

static void
finish_scan_job(pappl_job_t *job)	// I - Job
{
  pappl_scanner_t *scanner = job->scanner;
					// Scanner
  static const char * const job_states[] =
  {					// Job state strings
    "Pending",
    "Held",
    "Processing",
    "Stopped",
    "Canceled",
    "Aborted",
    "Completed"
  };


  _papplRWLockWrite(scanner);
  _papplRWLockWrite(job);

  if (job->is_canceled)
  {
    job->state = IPP_JSTATE_CANCELED;
  }
  else if (job->state == IPP_JSTATE_ABORTED)
  {
    // Keep aborted state
  }
  else if (job->state == IPP_JSTATE_PROCESSING)
  {
    // Scan data has been produced; job stays in "processing" state until all
    // pages have been retrieved or a timeout occurs.  The eSCL layer
    // (scanner-escl.c) calls _papplJobCompleteScan() when the last page has
    // been sent to the client.
    // If scan_pages_ready == 0 (nothing scanned), move to completed.
    if (job->scan_pages_ready == 0)
      job->state = IPP_JSTATE_COMPLETED;
  }

  papplLogJob(job, PAPPL_LOGLEVEL_INFO, "%s, pages-scanned=%d.",
              job_states[job->state - IPP_JSTATE_PENDING],
              job->impcompleted);

  if (job->state >= IPP_JSTATE_CANCELED)
  {
    job->completed = time(NULL);

    cupsArrayRemove(scanner->active_jobs, job);
    cupsArrayAdd(scanner->completed_jobs, job);

    _papplJobRemoveScanFiles(job);
  }

  scanner->processing_job = NULL;
  scanner->state          = IPP_PSTATE_IDLE;
  scanner->state_time     = time(NULL);

  _papplRWUnlock(job);

  // Close device...
  if (scanner->device)
  {
    papplDeviceClose(scanner->device);
    scanner->device = NULL;
  }

  _papplRWUnlock(scanner);

  // Check for more jobs...
  _papplRWLockWrite(scanner);
  _papplScannerCheckJobsNoLock(scanner);
  _papplRWUnlock(scanner);

  _papplSystemConfigChanged(scanner->system);
}


//
// '_papplJobCompleteScan()' - Mark a scan job as completed.
//
// Called when the last page has been retrieved by the client or the job
// should transition to completed state.
//

void
_papplJobCompleteScan(
    pappl_job_t *job)			// I - Job
{
  pappl_scanner_t *scanner;		// Scanner


  if (!job || !job->scanner)
    return;

  scanner = job->scanner;

  _papplRWLockWrite(scanner);
  _papplRWLockWrite(job);

  if (job->state < IPP_JSTATE_CANCELED)
  {
    job->state     = IPP_JSTATE_COMPLETED;
    job->completed = time(NULL);

    cupsArrayRemove(scanner->active_jobs, job);
    cupsArrayAdd(scanner->completed_jobs, job);

    scanner->impcompleted += job->impcompleted;

    papplLogJob(job, PAPPL_LOGLEVEL_INFO, "Completed, pages=%d.",
                job->impcompleted);
  }

  _papplJobRemoveScanFiles(job);

  _papplRWUnlock(job);
  _papplRWUnlock(scanner);

  if (!scanner->system->clean_time)
    scanner->system->clean_time = time(NULL) + 60;

  _papplSystemConfigChanged(scanner->system);
}
