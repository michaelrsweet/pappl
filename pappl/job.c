//
// Job object for the Printer Application Framework
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

  pthread_rwlock_wrlock(&job->rwlock);
  pthread_rwlock_wrlock(&job->printer->rwlock);

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

  pthread_rwlock_unlock(&job->printer->rwlock);

  if (!job->system->clean_time)
    job->system->clean_time = time(NULL) + 60;

  pthread_rwlock_unlock(&job->rwlock);
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



  pthread_rwlock_wrlock(&printer->rwlock);

  if (printer->max_active_jobs > 0 && cupsArrayCount(printer->active_jobs) >= printer->max_active_jobs)
  {
    pthread_rwlock_unlock(&printer->rwlock);
    return (NULL);
  }

  // Allocate and initialize the job object...
  if ((job = calloc(1, sizeof(pappl_job_t))) == NULL)
  {
    papplLog(printer->system, PAPPL_LOGLEVEL_ERROR, "Unable to allocate memory for job: %s", strerror(errno));
    pthread_rwlock_unlock(&printer->rwlock);
    return (NULL);
  }

  job->attrs   = ippNew();
  job->fd      = -1;
  job->format  = format;
  job->name    = job_name;
  job->printer = printer;
  job->state   = IPP_JSTATE_HELD;
  job->system  = printer->system;
  job->created = time(NULL);

  if (attrs)
  {
    // Copy all of the job attributes...
    _papplCopyAttributes(job->attrs, attrs, NULL, IPP_TAG_JOB, 0);

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
    ippAddString(job->attrs, IPP_TAG_JOB, IPP_TAG_NAME, "job-name", NULL, job_name);

  if ((attr = ippAddString(job->attrs, IPP_TAG_JOB, IPP_TAG_NAME, "job-originating-user-name", NULL, username)) != NULL)
    job->username = ippGetString(attr, 0, NULL);

  if ((attr = ippFindAttribute(attrs, "job-impressions", IPP_TAG_INTEGER)) != NULL)
    job->impressions = ippGetInteger(attr, 0);

  // Add job description attributes and add to the jobs array...
  job->job_id = job_id > 0 ? job_id : printer->next_job_id ++;

  if ((attr = ippFindAttribute(attrs, "printer-uri", IPP_TAG_URI)) != NULL)
  {
    strlcpy(job_printer_uri, ippGetString(attr, 0, NULL), sizeof(job_printer_uri));

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

  pthread_rwlock_unlock(&printer->rwlock);

  _papplSystemConfigChanged(printer->system);

  return (job);
}


//
// '_papplJobDelete()' - Remove a job from the system and free its memory.
//

void
_papplJobDelete(pappl_job_t *job)	// I - Job
{
  papplLogJob(job, PAPPL_LOGLEVEL_INFO, "Removing job from history.");

  ippDelete(job->attrs);

  free(job->message);

  // Only remove the job file (document) if the job is in a terminating state...
  if (job->state >= IPP_JSTATE_CANCELED)
    _papplJobRemoveFile(job);

  free(job);
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


  // Make sure the spool directory exists...
  if (!directory)
    directory = job->system->directory;

  if (access(directory, X_OK))
  {
    if (errno == ENOENT)
    {
      // Spool directory does not exist, might have been deleted...
      if (mkdir(directory, 0777))
      {
        papplLogJob(job, PAPPL_LOGLEVEL_FATAL, "Unable to create spool directory '%s': %s", directory, strerror(errno));
        return (-1);
      }
    }
    else
    {
      papplLogJob(job, PAPPL_LOGLEVEL_FATAL, "Unable to access spool directory '%s': %s", directory, strerror(errno));
      return (-1);
    }
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
    return (open(fname, O_RDONLY | O_NOFOLLOW | O_CLOEXEC));
  else if (!strcmp(mode, "w"))
    return (open(fname, O_WRONLY | O_CREAT | O_TRUNC | O_NOFOLLOW | O_CLOEXEC, 0600));
  else if (!strcmp(mode, "x"))
    return (unlink(fname));
  else
    return (-1);
}


//
// '_papplJobRemoveFile()' - Remove a file in spool directory
//

void
_papplJobRemoveFile(pappl_job_t *job)	// I - Job
{
  size_t dirlen = strlen(job->system->directory);
					// Length of spool directory


  // Only remove the file if it is in spool directory...
  if (job->filename && !strncmp(job->filename, job->system->directory, dirlen) && job->filename[dirlen] == '/')
    unlink(job->filename);

  free(job->filename);
  job->filename = NULL;
}


//
// '_papplJobSubmitFile()' - Submit a file for printing.
//

void
_papplJobSubmitFile(
    pappl_job_t *job,			// I - Job
    const char  *filename)		// I - Filename
{
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
    // Process the job...
    job->state = IPP_JSTATE_PENDING;

    _papplPrinterCheckJobs(job->printer);
  }
  else
  {
    // Abort the job...
    job->state     = IPP_JSTATE_ABORTED;
    job->completed = time(NULL);

    papplLogJob(job, PAPPL_LOGLEVEL_ERROR, "Unable to allocate filename.");
    unlink(filename);

    pthread_rwlock_wrlock(&job->printer->rwlock);
    cupsArrayRemove(job->printer->active_jobs, job);
    cupsArrayAdd(job->printer->completed_jobs, job);
    pthread_rwlock_unlock(&job->printer->rwlock);

    if (!job->system->clean_time)
      job->system->clean_time = time(NULL) + 60;
  }
}


//
// '_papplPrinterCheckJobs()' - Check for new jobs to process.
//

void
_papplPrinterCheckJobs(
    pappl_printer_t *printer)		// I - Printer
{
  pappl_job_t	*job;			// Current job


  papplLogPrinter(printer, PAPPL_LOGLEVEL_DEBUG, "Checking for new jobs to process.");

  if (printer->processing_job)
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

  pthread_rwlock_wrlock(&printer->rwlock);

  // Enumerate the jobs.  Since we have a writer (exclusive) lock, we are the
  // only thread enumerating and can use cupsArrayFirst/Last...

  for (job = (pappl_job_t *)cupsArrayFirst(printer->active_jobs); job; job = (pappl_job_t *)cupsArrayNext(printer->active_jobs))
  {
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
	pthread_detach(t);
      break;
    }
  }

  if (!job)
    papplLogPrinter(printer, PAPPL_LOGLEVEL_DEBUG, "No jobs to process at this time.");

  pthread_rwlock_unlock(&printer->rwlock);
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

  pthread_rwlock_rdlock(&(printer->rwlock));
  job = (pappl_job_t *)cupsArrayFind(printer->all_jobs, &key);
  pthread_rwlock_unlock(&(printer->rwlock));

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
  int			i,		// Looping var
			count;		// Number of printers
  pappl_printer_t	*printer;	// Current printer
  pappl_job_t		*job;		// Current job
  time_t		cleantime;	// Clean time


  cleantime = time(NULL) - 60;

  pthread_rwlock_rdlock(&system->rwlock);

  // Loop through the printers.
  //
  // Note: Cannot use cupsArrayFirst/Last since other threads might be
  // enumerating the printers array.

  for (i = 0, count = cupsArrayCount(system->printers); i < count; i ++)
  {
    printer = (pappl_printer_t *)cupsArrayIndex(system->printers, i);

    if (cupsArrayCount(printer->completed_jobs) == 0 || printer->max_completed_jobs <= 0)
      continue;

    pthread_rwlock_wrlock(&printer->rwlock);

    // Enumerate the jobs.  Since we have a writer (exclusive) lock, we are the
    // only thread enumerating and can use cupsArrayFirst/Last...

    for (job = (pappl_job_t *)cupsArrayFirst(printer->completed_jobs); job; job = (pappl_job_t *)cupsArrayNext(printer->completed_jobs))
    {
      if (job->completed && job->completed < cleantime && cupsArrayCount(printer->completed_jobs) > printer->max_completed_jobs)
      {
	cupsArrayRemove(printer->completed_jobs, job);
	cupsArrayRemove(printer->all_jobs, job);
      }
      else
	break;
    }

    pthread_rwlock_unlock(&printer->rwlock);
  }

  pthread_rwlock_unlock(&system->rwlock);
}
