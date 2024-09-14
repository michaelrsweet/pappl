//
// Job object for the Printer Application Framework
//
// Copyright © 2019-2024 by Michael R Sweet.
// Copyright © 2010-2019 by Apple Inc.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#include "pappl-private.h"


//
// 'papplJobCancel()' - Cancel a job.
//
// This function cancels the specified job.  If the job is currently being
// printed, it will be stopped at a convenient time (usually the end of a page)
// so that the printer will be left in a known state.
//

void
papplJobCancel(pappl_job_t *job)	// I - Job
{
  if (!job)
    return;

  _papplRWLockWrite(job->printer);
  _papplRWLockWrite(job);

  if (job->state == IPP_JSTATE_PROCESSING || (job->state == IPP_JSTATE_HELD && job->fd >= 0))
  {
    job->is_canceled = true;
  }
  else
  {
    job->state     = IPP_JSTATE_CANCELED;
    job->completed = time(NULL);

    _papplJobRemoveFile(job);

    cupsArrayRemove(job->printer->active_jobs, job);
    cupsArrayAdd(job->printer->completed_jobs, job);
  }

  if (!job->system->clean_time)
    job->system->clean_time = time(NULL) + 60;

  _papplRWUnlock(job);
  _papplRWUnlock(job->printer);

  papplSystemAddEvent(job->system, job->printer, job, PAPPL_EVENT_JOB_COMPLETED, NULL);
}


//
// '_papplJobCreate()' - Create a new/existing job object.
//

pappl_job_t *				// O - Job
_papplJobCreate(
    pappl_printer_t *printer,		// I - Printer
    int             job_id,		// I - Existing Job ID or `0` for new job
    const char      *username,		// I - Username
    const char      *format,		// I - Document format or `NULL` for none
    const char      *job_name,		// I - Job name
    ipp_t           *attrs)		// I - Job creation attributes or `NULL` for none
{
  pappl_job_t		*job;		// Job
  ipp_attribute_t	*attr;		// Job attribute
  char			job_printer_uri[1024],
					// job-printer-uri value
			job_uri[1024],	// job-uri value
			job_uuid[64];	// job-uuid value



  _papplRWLockWrite(printer);

  if (printer->max_active_jobs > 0 && (int)cupsArrayGetCount(printer->active_jobs) >= printer->max_active_jobs)
  {
    _papplRWUnlock(printer);
    return (NULL);
  }

  // Allocate and initialize the job object...
  if ((job = calloc(1, sizeof(pappl_job_t))) == NULL)
  {
    papplLog(printer->system, PAPPL_LOGLEVEL_ERROR, "Unable to allocate memory for job: %s", strerror(errno));
    _papplRWUnlock(printer);
    return (NULL);
  }

  pthread_rwlock_init(&job->rwlock, NULL);

  job->attrs   = ippNew();
  job->fd      = -1;
  job->format  = format;
  job->name    = job_name;
  job->printer = printer;
  job->state   = IPP_JSTATE_HELD;
  job->system  = printer->system;
  job->created = time(NULL);
  job->copies  = 1;

  if (attrs)
  {
    // Copy all of the job attributes...
    const char	*hold_until;		// "job-hold-until" value
    time_t	hold_until_time;	// "job-hold-until-time" value

    if ((attr = ippFindAttribute(attrs, "client-info", IPP_TAG_BEGIN_COLLECTION)) != NULL)
    {
      if ((attr = ippCopyAttribute(job->attrs, attr, false)) != NULL)
        ippSetGroupTag(job->attrs, &attr, IPP_TAG_JOB);
    }

    _papplCopyAttributes(job->attrs, attrs, NULL, IPP_TAG_JOB, 0);

    if ((attr = ippFindAttribute(job->attrs, "copies", IPP_TAG_INTEGER)) != NULL)
      job->copies = ippGetInteger(attr, 0);

    hold_until      = ippGetString(ippFindAttribute(attrs, "job-hold-until", IPP_TAG_KEYWORD), 0, NULL);
    hold_until_time = ippDateToTime(ippGetDate(ippFindAttribute(attrs, "job-hold-until-time", IPP_TAG_DATE), 0));

    if ((hold_until && strcmp(hold_until, "no-hold")) || hold_until_time)
      _papplJobHoldNoLock(job, NULL, hold_until, hold_until_time);

    if (!format && ippGetOperation(attrs) != IPP_OP_CREATE_JOB)
    {
      if ((attr = ippFindAttribute(attrs, "document-format-detected", IPP_TAG_MIMETYPE)) != NULL)
	job->format = ippGetString(attr, 0, NULL);
      else if ((attr = ippFindAttribute(attrs, "document-format-supplied", IPP_TAG_MIMETYPE)) != NULL)
	job->format = ippGetString(attr, 0, NULL);
      else
	job->format = "application/octet-stream";
    }
  }
  else
  {
    ippAddString(job->attrs, IPP_TAG_JOB, IPP_TAG_NAME, "job-name", NULL, job_name);
  }

  if ((attr = ippAddString(job->attrs, IPP_TAG_JOB, IPP_TAG_NAME, "job-originating-user-name", NULL, username)) != NULL)
    job->username = ippGetString(attr, 0, NULL);

  if ((attr = ippFindAttribute(attrs, "job-impressions", IPP_TAG_INTEGER)) != NULL)
    job->impressions = ippGetInteger(attr, 0);

  // Add job description attributes and add to the jobs array...
  job->job_id = job_id > 0 ? job_id : printer->next_job_id ++;

  if ((attr = ippFindAttribute(attrs, "printer-uri", IPP_TAG_URI)) != NULL)
  {
    papplCopyString(job_printer_uri, ippGetString(attr, 0, NULL), sizeof(job_printer_uri));

    snprintf(job_uri, sizeof(job_uri), "%s/%d", ippGetString(attr, 0, NULL), job->job_id);
  }
  else
  {
    httpAssembleURI(HTTP_URI_CODING_ALL, job_printer_uri, sizeof(job_printer_uri), "ipps", NULL, printer->system->hostname, printer->system->port, printer->resource);
    httpAssembleURIf(HTTP_URI_CODING_ALL, job_uri, sizeof(job_uri), "ipps", NULL, printer->system->hostname, printer->system->port, "%s/%d", printer->resource, job->job_id);
  }

  _papplSystemMakeUUID(printer->system, printer->name, job->job_id, job_uuid, sizeof(job_uuid));

  ippAddInteger(job->attrs, IPP_TAG_JOB, IPP_TAG_INTEGER, "job-id", job->job_id);
  ippAddString(job->attrs, IPP_TAG_JOB, IPP_TAG_URI, "job-uri", NULL, job_uri);
  ippAddString(job->attrs, IPP_TAG_JOB, IPP_TAG_URI, "job-uuid", NULL, job_uuid);
  ippAddString(job->attrs, IPP_TAG_JOB, IPP_TAG_URI, "job-printer-uri", NULL, job_printer_uri);

  cupsArrayAdd(printer->all_jobs, job);

  if (!job_id)
    cupsArrayAdd(printer->active_jobs, job);

  _papplRWUnlock(printer);

  papplSystemAddEvent(printer->system, printer, job, PAPPL_EVENT_JOB_CREATED, NULL);

  _papplSystemConfigChanged(printer->system);

  return (job);
}


//
// 'papplJobCreateWithFile()' - Create a job with a local file.
//
// This function creates a new print job with a local file.  The "num_options"
// and "options" parameters specify additional print options, as needed.  The
// file specified by "filename" is removed automatically if it resides in the
// spool directory.
//

pappl_job_t *				// O - New job object or `NULL` on error
papplJobCreateWithFile(
    pappl_printer_t *printer,		// I - Printer
    const char      *username,		// I - Submitting user name
    const char      *format,		// I - MIME media type of file
    const char      *job_name,		// I - Job name
    int             num_options,	// I - Number of options
    cups_option_t   *options,		// I - Options or `NULL` if none
    const char      *filename)		// I - File to print
{
  pappl_job_t	*job;			// New job
  ipp_t		*attrs;			// Attributes for job


  // Range check input...
  if (!printer || !username || !format || !job_name || !filename)
    return (NULL);

  // Encode options as needed...
  if (num_options > 0 && options)
  {
    attrs = ippNew();

    _papplRWLockRead(printer);
    _papplMainloopAddOptions(attrs, (cups_len_t)num_options, options, printer->driver_attrs);
    _papplRWUnlock(printer);
  }
  else
  {
    attrs = NULL;
  }

  // Create the job...
  if ((job = _papplJobCreate(printer, 0, username, format, job_name, attrs)) != NULL)
    _papplJobSubmitFile(job, filename);

  ippDelete(attrs);

  return (job);
}


//
// '_papplJobDelete()' - Remove a job from the system and free its memory.
//

void
_papplJobDelete(pappl_job_t *job)	// I - Job
{
  papplLogJob(job, PAPPL_LOGLEVEL_INFO, "Removing job from history.");

  pthread_rwlock_destroy(&job->rwlock);

  ippDelete(job->attrs);

  free(job->message);

  // Only remove the job file (document) if the job is in a terminating state...
  if (job->state >= IPP_JSTATE_CANCELED)
    _papplJobRemoveFile(job);

  free(job);
}


//
// 'papplJobHold()' - Hold a job for printing.
//
// This function holds a pending job for printing at a later time.
//

bool					// O - `true` on success, `false` on failure
papplJobHold(pappl_job_t *job,		// I - Job
	     const char  *username,	// I - User that held the job or `NULL` for none/system
	     const char  *until,	// I - "job-hold-until" keyword or `NULL`
	     time_t      until_time)	// I - "job-hold-until-time" value or 0 for indefinite
{
  bool	ret = false;			// Return value


  // Range check input
  if (!job)
    return (false);

  // Lock the printer and job so we can change it...
  _papplRWLockRead(job->printer);
  _papplRWLockWrite(job);

  // Only hold jobs that haven't entered the processing state...
  if (job->state < IPP_JSTATE_PROCESSING)
  {
    // Hold until the specified time...
    ret = _papplJobHoldNoLock(job, username, until, until_time);
  }

  _papplRWUnlock(job);
  _papplRWUnlock(job->printer);

  return (ret);
}


//
// '_papplJobHoldNoLock()' - Hold a job for printing without locking.
//

bool					// O - `true` on success, `false` on failure
_papplJobHoldNoLock(
    pappl_job_t *job,			// I - Job
    const char  *username,		// I - User that held the job or `NULL` for none/system
    const char  *until,			// I - "job-hold-until" keyword or `NULL`
    time_t      until_time)		// I - "job-hold-until-time" value or 0 for indefinite
{
  ipp_attribute_t	*attr;		// "job-hold-until[-time]" attribute


  job->state = IPP_JSTATE_HELD;

  if (until)
  {
    // Hold until the specified time period...
    time_t	curtime;		// Current time
    struct tm	curdate;		// Current date

    job->state_reasons |= PAPPL_JREASON_JOB_HOLD_UNTIL_SPECIFIED;

    time(&curtime);
    localtime_r(&curtime, &curdate);

    if (!strcmp(until, "day-time"))
    {
      // Hold to 6am the next morning unless local time is < 6pm.
      if (curdate.tm_hour < 18)
	job->hold_until = curtime;
      else
	job->hold_until = curtime + ((29 - curdate.tm_hour) * 60 + 59 - curdate.tm_min) * 60 + 60 - curdate.tm_sec;
    }
    else if (!strcmp(until, "evening") || !strcmp(until, "night"))
    {
      // Hold to 6pm unless local time is > 6pm or < 6am.
      if (curdate.tm_hour < 6 || curdate.tm_hour >= 18)
	job->hold_until = curtime;
      else
	job->hold_until = curtime + ((17 - curdate.tm_hour) * 60 + 59 - curdate.tm_min) * 60 + 60 - curdate.tm_sec;
    }
    else if (!strcmp(until, "second-shift"))
    {
      // Hold to 4pm unless local time is > 4pm.
      if (curdate.tm_hour >= 16)
	job->hold_until = curtime;
      else
	job->hold_until = curtime + ((15 - curdate.tm_hour) * 60 + 59 - curdate.tm_min) * 60 + 60 - curdate.tm_sec;
    }
    else if (!strcmp(until, "third-shift"))
    {
      // Hold to 12am unless local time is < 8am.
      if (curdate.tm_hour < 8)
	job->hold_until = curtime;
      else
	job->hold_until = curtime + ((23 - curdate.tm_hour) * 60 + 59 - curdate.tm_min) * 60 + 60 - curdate.tm_sec;
    }
    else if (!strcmp(until, "weekend"))
    {
      // Hold to weekend unless we are in the weekend.
      if (curdate.tm_wday == 0 || curdate.tm_wday == 6)
	job->hold_until = curtime;
      else
	job->hold_until = curtime + (((5 - curdate.tm_wday) * 24 + (17 - curdate.tm_hour)) * 60 + 59 - curdate.tm_min) * 60 + 60 - curdate.tm_sec;
    }
    else
    {
      // Hold indefinitely...
      job->hold_until = 0;
    }

    // Update attributes...
    if ((attr = ippFindAttribute(job->attrs, "job-hold-until", IPP_TAG_KEYWORD)) != NULL)
      ippSetString(job->attrs, &attr, 0, until);
    else
      ippAddString(job->attrs, IPP_TAG_JOB, IPP_TAG_KEYWORD, "job-hold-until", NULL, until);

    if ((attr = ippFindAttribute(job->attrs, "job-hold-until-time", IPP_TAG_DATE)) != NULL)
      ippDeleteAttribute(job->attrs, attr);
  }
  else if (until_time > 0)
  {
    // Hold until the specified time...
    job->state_reasons |= PAPPL_JREASON_JOB_HOLD_UNTIL_SPECIFIED;
    job->hold_until    = until_time;

    // Update attributes...
    if ((attr = ippFindAttribute(job->attrs, "job-hold-until", IPP_TAG_KEYWORD)) != NULL)
      ippDeleteAttribute(job->attrs, attr);

    if ((attr = ippFindAttribute(job->attrs, "job-hold-until-time", IPP_TAG_DATE)) != NULL)
      ippSetDate(job->attrs, &attr, 0, ippTimeToDate(until_time));
    else
      ippAddDate(job->attrs, IPP_TAG_JOB, "job-hold-until-time", ippTimeToDate(until_time));
  }
  else
  {
    // Hold indefinitely...
    job->state_reasons &= (pappl_jreason_t)(~PAPPL_JREASON_JOB_HOLD_UNTIL_SPECIFIED);
    job->hold_until    = 0;

    // Update attributes...
    if ((attr = ippFindAttribute(job->attrs, "job-hold-until", IPP_TAG_KEYWORD)) != NULL)
      ippDeleteAttribute(job->attrs, attr);

    if ((attr = ippFindAttribute(job->attrs, "job-hold-until-time", IPP_TAG_DATE)) != NULL)
      ippDeleteAttribute(job->attrs, attr);
  }

  if (username)
  {
    _papplSystemAddEventNoLock(job->system, job->printer, NULL, job, PAPPL_EVENT_JOB_STATE_CHANGED, "Job held by '%s'.", username);
  }

  return (true);
}


//
// 'papplJobOpenFile()' - Create or open a file for the document in a job.
//
// This function creates or opens a file for a job.  The "fname" and "fnamesize"
// arguments specify the location and size of a buffer to store the job
// filename, which incorporates the "directory", printer ID, job ID, job name
// (title), and "ext" values.  The job name is "sanitized" to only contain
// alphanumeric characters.
//
// The "mode" argument is "r" to read an existing job file or "w" to write a
// new job file.  New files are created with restricted permissions for
// security purposes.
//

int					// O - File descriptor or -1 on error
papplJobOpenFile(
    pappl_job_t *job,			// I - Job
    char        *fname,			// I - Filename buffer
    size_t      fnamesize,		// I - Size of filename buffer
    const char  *directory,		// I - Directory to store in (`NULL` for default)
    const char  *ext,			// I - Extension (`NULL` for default)
    const char  *mode)			// I - Open mode - "r" for reading or "w" for writing
{
  char			name[64],	// "Safe" filename
			*nameptr;	// Pointer into filename
  const char		*job_name;	// job-name value


  // Range check input...
  if (!job || !fname || fnamesize < 256 || !mode)
  {
    if (fname)
      *fname = '\0';

    return (-1);
  }

  // Make sure the spool directory exists...
  if (!directory)
    directory = job->system->directory;

  if (mkdir(directory, 0777) && errno != EEXIST)
  {
    papplLogJob(job, PAPPL_LOGLEVEL_FATAL, "Unable to create spool directory '%s': %s", directory, strerror(errno));
    return (-1);
  }

  // Make a name from the job-name attribute...
  if ((job_name = ippGetString(ippFindAttribute(job->attrs, "job-name", IPP_TAG_NAME), 0, NULL)) == NULL)
    job_name = "untitled";

  if ((nameptr = strrchr(job_name, '/')) != NULL && nameptr[1])
    job_name = nameptr + 1;

  for (nameptr = name; *job_name && nameptr < (name + sizeof(name) - 1); job_name ++)
  {
    if (isalnum(*job_name & 255) || *job_name == '-')
    {
      *nameptr++ = (char)tolower(*job_name & 255);
    }
    else
    {
      *nameptr++ = '_';

      while (job_name[1] && !isalnum(job_name[1] & 255) && job_name[1] != '-')
        job_name ++;
    }
  }

  *nameptr = '\0';

  // Figure out the extension...
  if (!ext)
  {
    if (!strcasecmp(job->format, "image/jpeg"))
      ext = "jpg";
    else if (!strcasecmp(job->format, "image/png"))
      ext = "png";
    else if (!strcasecmp(job->format, "image/pwg-raster"))
      ext = "pwg";
    else if (!strcasecmp(job->format, "image/urf"))
      ext = "urf";
    else if (!strcasecmp(job->format, "application/pdf"))
      ext = "pdf";
    else if (!strcasecmp(job->format, "application/postscript"))
      ext = "ps";
    else
      ext = "prn";
  }

  // Create a filename with the job-id, job-name, and document-format (extension)...
  snprintf(fname, fnamesize, "%s/p%05dj%09d-%s.%s", directory, job->printer->printer_id, job->job_id, name, ext);

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
// 'papplJobRelease()' - Release a job for printing.
//
// This function releases a held job for printing.
//

bool					// O - `true` on success, `false` on failure
papplJobRelease(pappl_job_t *job,	// I - Job
                const char  *username)	// I - User that released the job or `NULL` for none/system
{
  bool ret = false;			// Return value


  // Range check input
  if (!job)
    return (false);

  // Lock the job and printer...
  _papplRWLockWrite(job->printer);
  _papplRWLockWrite(job);

  // Only release jobs in the held state...
  if (job->state == IPP_JSTATE_HELD)
  {
    // Do the release...
    _papplJobReleaseNoLock(job, username);
    ret = true;
  }

  // Unlock and return...
  _papplRWUnlock(job);

  _papplPrinterCheckJobsNoLock(job->printer);

  _papplRWUnlock(job->printer);

  return (ret);
}


//
// '_papplJobReleaseNoLock()' - Release a job for printing without locking.
//

void
_papplJobReleaseNoLock(
    pappl_job_t *job,			// I - Job
    const char  *username)		// I - User that released the job or `NULL` for none/system
{
  ipp_attribute_t	*attr;		// "job-hold-until[-time]" attribute


  // Move the job back to the pending state and clear any attributes or states
  // related to job-hold-until...
  job->state         = IPP_JSTATE_PENDING;
  job->state_reasons &= (pappl_jreason_t)(~PAPPL_JREASON_JOB_HOLD_UNTIL_SPECIFIED);

  if ((attr = ippFindAttribute(job->attrs, "job-hold-until", IPP_TAG_KEYWORD)) != NULL)
    ippDeleteAttribute(job->attrs, attr);

  if ((attr = ippFindAttribute(job->attrs, "job-hold-until-time", IPP_TAG_DATE)) != NULL)
    ippDeleteAttribute(job->attrs, attr);

  if (username)
    _papplSystemAddEventNoLock(job->system, job->printer, NULL, job, PAPPL_EVENT_JOB_STATE_CHANGED, "Job released by '%s'.", username);
}


//
// '_papplJobRemoveFile()' - Remove a file in spool directory
//

void
_papplJobRemoveFile(pappl_job_t *job)	// I - Job
{
  size_t dirlen = strlen(job->system->directory);
					// Length of spool directory
  const char *tempdir = papplGetTempDir();
					// Location of temporary files
  size_t templen = strlen(tempdir);	// Length of temporary directory


  // Only remove the file if it is in spool or temporary directory...
  if (job->filename)
  {
    if ((!strncmp(job->filename, job->system->directory, dirlen) && job->filename[dirlen] == '/') || (!strncmp(job->filename, tempdir, templen) && job->filename[templen] == '/'))
      unlink(job->filename);
  }

  free(job->filename);
  job->filename = NULL;
}


//
// 'papplJobRetain()' - Retain a completed job until the specified time.
//

bool					// O - `true` on success, `false` on failure
papplJobRetain(
    pappl_job_t *job,			// I - Job
    const char  *username,		// I - User that held the job or `NULL` for none/system
    const char  *until,			// I - "job-retain-until" value or `NULL` for none
    int         until_interval,		// I - "job-retain-until-interval" value or `0` for none
    time_t      until_time)		// I - "job-retain-until-time" value or `0` for none
{
  bool	ret = false;			// Return value


  // Range check input
  if (!job)
    return (false);

  // Lock the printer and job so we can change it...
  _papplRWLockRead(job->printer);
  _papplRWLockWrite(job);

  // Only hold jobs that haven't entered the processing state...
  if (job->state < IPP_JSTATE_CANCELED)
  {
    // Hold until the specified time...
    ret = _papplJobRetainNoLock(job, username, until, until_interval, until_time);
  }

  _papplRWUnlock(job);
  _papplRWUnlock(job->printer);

  return (ret);
}


//
// '_papplJobRetainNoLock()' - Retain a completed job until the specified time.
//

bool					// O - `true` on success, `false` on failure
_papplJobRetainNoLock(
    pappl_job_t *job,			// I - Job
    const char  *username,		// I - User that held the job or `NULL` for none/system
    const char  *until,			// I - "job-retain-until" value or `NULL` for none
    int         until_interval,		// I - "job-retain-until-interval" value or `0` for none
    time_t      until_time)		// I - "job-retain-until-time" value or `0` for none
{
  ipp_attribute_t	*attr;		// "job-retain-until[-interval,-time]" attribute


  // Update attributes...
  if ((attr = ippFindAttribute(job->attrs, "job-retain-until", IPP_TAG_KEYWORD)) != NULL)
  {
    if (until)
      ippSetString(job->attrs, &attr, 0, until);
    else
      ippDeleteAttribute(job->attrs, attr);
  }
  else if (until)
  {
    ippAddString(job->attrs, IPP_TAG_JOB, IPP_TAG_KEYWORD, "job-retain-until", NULL, until);
  }

  if ((attr = ippFindAttribute(job->attrs, "job-retain-until-interval", IPP_TAG_INTEGER)) != NULL)
  {
    if (until_interval > 0)
      ippSetInteger(job->attrs, &attr, 0, until_interval);
    else
      ippDeleteAttribute(job->attrs, attr);
  }
  else if (until_interval > 0)
  {
    ippAddInteger(job->attrs, IPP_TAG_JOB, IPP_TAG_INTEGER, "job-retain-until-interval", until_interval);
  }

  if ((attr = ippFindAttribute(job->attrs, "job-retain-until-time", IPP_TAG_DATE)) != NULL)
  {
    if (until_time > 0)
      ippSetDate(job->attrs, &attr, 0, ippTimeToDate(until_time));
    else
      ippDeleteAttribute(job->attrs, attr);
  }
  else if (until_time > 0)
  {
    ippAddDate(job->attrs, IPP_TAG_JOB, "job-retain-until-time", ippTimeToDate(until_time));
  }

  if (username)
  {
    _papplSystemAddEventNoLock(job->system, job->printer, NULL, job, PAPPL_EVENT_JOB_CONFIG_CHANGED, "Job retain set by '%s'.", username);
  }

  return (true);
}


//
// '_papplJobSetRetain()' - Set the "retain_until" value for a Job.
//

void
_papplJobSetRetain(pappl_job_t *job)	// I - Job
{
  ipp_attribute_t	*attr;		// "job-retain-until[-interval,-time]" attribute


  if ((attr = ippFindAttribute(job->attrs, "job-retain-until", IPP_TAG_KEYWORD)) != NULL)
  {
    // Hold until the specified time period...
    const char	*until = ippGetString(attr, 0, NULL);
					// Keyword value
    time_t	curtime;		// Current time
    struct tm	curdate;		// Current date

    time(&curtime);
    localtime_r(&curtime, &curdate);

    if (!strcmp(until, "day-time"))
    {
      // Hold to 6am the next morning unless local time is < 6pm.
      if (curdate.tm_hour < 18)
	job->retain_until = curtime;
      else
	job->retain_until = curtime + ((29 - curdate.tm_hour) * 60 + 59 - curdate.tm_min) * 60 + 60 - curdate.tm_sec;
    }
    else if (!strcmp(until, "evening") || !strcmp(until, "night"))
    {
      // Hold to 6pm unless local time is > 6pm or < 6am.
      if (curdate.tm_hour < 6 || curdate.tm_hour >= 18)
	job->retain_until = curtime;
      else
	job->retain_until = curtime + ((17 - curdate.tm_hour) * 60 + 59 - curdate.tm_min) * 60 + 60 - curdate.tm_sec;
    }
    else if (!strcmp(until, "second-shift"))
    {
      // Hold to 4pm unless local time is > 4pm.
      if (curdate.tm_hour >= 16)
	job->retain_until = curtime;
      else
	job->retain_until = curtime + ((15 - curdate.tm_hour) * 60 + 59 - curdate.tm_min) * 60 + 60 - curdate.tm_sec;
    }
    else if (!strcmp(until, "third-shift"))
    {
      // Hold to 12am unless local time is < 8am.
      if (curdate.tm_hour < 8)
	job->retain_until = curtime;
      else
	job->retain_until = curtime + ((23 - curdate.tm_hour) * 60 + 59 - curdate.tm_min) * 60 + 60 - curdate.tm_sec;
    }
    else if (!strcmp(until, "weekend"))
    {
      // Hold to weekend unless we are in the weekend.
      if (curdate.tm_wday == 0 || curdate.tm_wday == 6)
	job->retain_until = curtime;
      else
	job->retain_until = curtime + (((5 - curdate.tm_wday) * 24 + (17 - curdate.tm_hour)) * 60 + 59 - curdate.tm_min) * 60 + 60 - curdate.tm_sec;
    }
  }
  else if ((attr = ippFindAttribute(job->attrs, "job-retain-until-interval", IPP_TAG_INTEGER)) != NULL)
  {
    job->retain_until = time(NULL) + ippGetInteger(attr, 0);
  }
  else if ((attr = ippFindAttribute(job->attrs, "job-retain-until-time", IPP_TAG_DATE)) != NULL)
  {
    job->retain_until = ippDateToTime(ippGetDate(attr, 0));
  }
}


//
// '_papplJobSubmitFile()' - Submit a file for printing.
//

void
_papplJobSubmitFile(
    pappl_job_t *job,			// I - Job
    const char  *filename)		// I - Filename
{
  _papplRWLockWrite(job);

  if (!job->format)
  {
    // Open the file
    unsigned char	header[8192];	// First 8k bytes of file
    ssize_t		headersize;	// Number of bytes read
    int			fd;		// File descriptor

    if ((fd = open(filename, O_RDONLY)) >= 0)
    {
      // Auto-type the file using the first N bytes of the file...
      memset(header, 0, sizeof(header));
      headersize = read(fd, (char *)header, sizeof(header));
      close(fd);

      _papplRWLockRead(job->system);

      if (!memcmp(header, "%PDF", 4))
	job->format = "application/pdf";
      else if (!memcmp(header, "%!", 2))
	job->format = "application/postscript";
      else if (!memcmp(header, "\377\330\377", 3) && header[3] >= 0xe0 && header[3] <= 0xef)
	job->format = "image/jpeg";
      else if (!memcmp(header, "\211PNG", 4))
	job->format = "image/png";
      else if (!memcmp(header, "RaS2PwgR", 8))
	job->format = "image/pwg-raster";
      else if (!memcmp(header, "UNIRAST", 8))
	job->format = "image/urf";
      else if (job->system->mime_cb)
	job->format = (job->system->mime_cb)(header, (size_t)headersize, job->system->mime_cbdata);

      _papplRWUnlock(job->system);
    }
  }

  if (!job->format)
  {
    // Guess the format using the filename extension...
    const char *ext = strrchr(filename, '.');
				// Extension on filename

    if (!ext)
      job->format = job->printer->driver_data.format;
    else if (!strcmp(ext, ".jpg") || !strcmp(ext, ".jpeg"))
      job->format = "image/jpeg";
    else if (!strcmp(ext, ".png"))
      job->format = "image/png";
    else if (!strcmp(ext, ".pwg"))
      job->format = "image/pwg-raster";
    else if (!strcmp(ext, ".urf"))
      job->format = "image/urf";
    else if (!strcmp(ext, ".txt"))
      job->format = "text/plain";
    else if (!strcmp(ext, ".pdf"))
      job->format = "application/pdf";
    else if (!strcmp(ext, ".ps"))
      job->format = "application/postscript";
    else
      job->format = job->printer->driver_data.format;
  }

  // Save the print file information...
  if ((job->filename = strdup(filename)) != NULL)
  {
    if (!job->printer->hold_new_jobs && !(job->state_reasons & PAPPL_JREASON_JOB_HOLD_UNTIL_SPECIFIED))
    {
      // Process the job...
      job->state = IPP_JSTATE_PENDING;

      _papplRWUnlock(job);
      _papplRWLockWrite(job->printer);
      _papplPrinterCheckJobsNoLock(job->printer);
      _papplRWUnlock(job->printer);
    }
    else
    {
      // Queue the job...
      _papplRWUnlock(job);
    }
  }
  else
  {
    // Abort the job...
    size_t dirlen = strlen(job->system->directory);
					// Length of spool directory

    job->state     = IPP_JSTATE_ABORTED;
    job->completed = time(NULL);

    _papplRWUnlock(job);

    papplLogJob(job, PAPPL_LOGLEVEL_ERROR, "Unable to allocate filename.");

    if (!strncmp(filename, job->system->directory, dirlen) && filename[dirlen] == '/')
      unlink(filename);

    _papplRWLockWrite(job->printer);
    cupsArrayRemove(job->printer->active_jobs, job);
    cupsArrayAdd(job->printer->completed_jobs, job);
    _papplRWUnlock(job->printer);

    if (!job->system->clean_time)
      job->system->clean_time = time(NULL) + 60;
  }
}


//
// '_papplPrinterCheckJobsNoLock()' - Check for new jobs to process.
//

void
_papplPrinterCheckJobsNoLock(
    pappl_printer_t *printer)		// I - Printer
{
  pappl_job_t	*job;			// Current job


  papplLogPrinter(printer, PAPPL_LOGLEVEL_DEBUG, "Checking for new jobs to process.");

  if (printer->device_in_use)
  {
    papplLogPrinter(printer, PAPPL_LOGLEVEL_DEBUG, "Printer is in use.");
    return;
  }
  else if (printer->processing_job)
  {
    papplLogPrinter(printer, PAPPL_LOGLEVEL_DEBUG, "Printer is already processing job %d.", printer->processing_job->job_id);
    return;
  }
  else if (printer->is_deleted)
  {
    papplLogPrinter(printer, PAPPL_LOGLEVEL_DEBUG, "Printer is being deleted.");
    return;
  }
  else if (printer->state == IPP_PSTATE_STOPPED || printer->is_stopped)
  {
    papplLogPrinter(printer, PAPPL_LOGLEVEL_DEBUG, "Printer is stopped.");
    return;
  }

  // Enumerate the jobs.  Since we have a writer (exclusive) lock, we are the
  // only thread enumerating and can use cupsArrayGetFirst/Last...
  for (job = (pappl_job_t *)cupsArrayGetFirst(printer->active_jobs); job; job = (pappl_job_t *)cupsArrayGetNext(printer->active_jobs))
  {
    if (job->state == IPP_JSTATE_HELD && job->hold_until && job->hold_until >= time(NULL))
    {
      // Release job when the hold time arrives...
      _papplRWLockWrite(job);
      _papplJobReleaseNoLock(job, NULL);
      _papplRWUnlock(job);
    }

    if (job->state == IPP_JSTATE_PENDING)
    {
      pthread_t	t;			// Thread

      papplLogPrinter(printer, PAPPL_LOGLEVEL_DEBUG, "Starting job %d.", job->job_id);

      if (pthread_create(&t, NULL, (void *(*)(void *))_papplJobProcess, job))
      {
	job->state     = IPP_JSTATE_ABORTED;
	job->completed = time(NULL);

	cupsArrayRemove(printer->active_jobs, job);
	cupsArrayAdd(printer->completed_jobs, job);

	if (!printer->system->clean_time)
	  printer->system->clean_time = time(NULL) + 60;
      }
      else
      {
	pthread_detach(t);
      }
      break;
    }
  }

  if (!job)
    papplLogPrinter(printer, PAPPL_LOGLEVEL_DEBUG, "No jobs to process at this time.");
}


//
// '_papplPrinterCleanJobsNoLock()' - Clean completed jobs for a printer.
//

void
_papplPrinterCleanJobsNoLock(
    pappl_printer_t *printer)		// I - Printer
{
  time_t	cleantime;		// Clean time
  pappl_job_t	*job;			// Current job
  int		preserved;		// Number of preserved jobs


  if (cupsArrayGetCount(printer->completed_jobs) == 0 || (printer->max_preserved_jobs == 0 && printer->max_completed_jobs <= 0))
    return;

  // Enumerate the jobs.  Since we have a writer (exclusive) lock, we are the
  // only thread enumerating and can use cupsArrayGetFirst/Last...
  for (job = (pappl_job_t *)cupsArrayGetFirst(printer->completed_jobs), cleantime = time(NULL) - 60, preserved = 0; job; job = (pappl_job_t *)cupsArrayGetNext(printer->completed_jobs))
  {
    if (job->completed && job->completed < cleantime && printer->max_completed_jobs > 0 && (int)cupsArrayGetCount(printer->completed_jobs) > printer->max_completed_jobs)
    {
      cupsArrayRemove(printer->completed_jobs, job);
      cupsArrayRemove(printer->all_jobs, job);
    }
    else if (printer->max_preserved_jobs > 0)
    {
      if (job->filename)
      {
	if ((preserved + 1) > printer->max_preserved_jobs || (job->retain_until && time(NULL) > job->retain_until))
	  _papplJobRemoveFile(job);
	else
	  preserved ++;
      }
    }
    else
    {
      break;
    }
  }
}


//
// 'papplPrinterFindJob()' - Find a job.
//
// This function finds a job submitted to a printer using its integer ID value.
//

pappl_job_t *				// O - Job or `NULL` if not found
papplPrinterFindJob(
    pappl_printer_t *printer,		// I - Printer
    int             job_id)		// I - Job ID
{
  pappl_job_t		key,		// Job search key
			*job;		// Matching job, if any


  key.job_id = job_id;

  _papplRWLockRead(printer);
  job = (pappl_job_t *)cupsArrayFind(printer->all_jobs, &key);
  _papplRWUnlock(printer);

  return (job);
}


//
// 'papplSystemCleanJobs()' - Clean out old (completed) jobs.
//
// This function deletes all old (completed) jobs above the limit set by the
// @link papplPrinterSetMaxCompletedJobs@ function.  The level may temporarily
// exceed this limit if the jobs were completed within the last 60 seconds.
//
// > Note: This function is normally called automatically from the
// > @link papplSystemRun@ function.
//

void
papplSystemCleanJobs(
    pappl_system_t *system)		// I - System
{
  cups_len_t		i,		// Looping var
			count;		// Number of printers
  pappl_printer_t	*printer;	// Current printer


  _papplRWLockRead(system);

  // Loop through the printers.
  //
  // Note: Cannot use cupsArrayGetFirst/Last since other threads might be
  // enumerating the printers array.

  for (i = 0, count = cupsArrayGetCount(system->printers); i < count; i ++)
  {
    printer = (pappl_printer_t *)cupsArrayGetElement(system->printers, i);

    _papplRWLockWrite(printer);
    _papplPrinterCleanJobsNoLock(printer);
    _papplRWUnlock(printer);
  }

  system->clean_time = 0;

  _papplRWUnlock(system);
}
