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

  pthread_rwlock_wrlock(&printer->rwlock);

  for (job = (pappl_job_t *)cupsArrayFirst(printer->active_jobs);
       job;
       job = (pappl_job_t *)cupsArrayNext(printer->active_jobs))
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
// 'papplSystemCleanJobs()' - Clean out old (completed) jobs.
//

void
papplSystemCleanJobs(
    pappl_system_t *system)		// I - System
{
  pappl_printer_t	*printer;	// Current printer
  pappl_job_t		*job;		// Current job
  time_t		cleantime;	// Clean time


  cleantime = time(NULL) - 60;

  pthread_rwlock_rdlock(&system->rwlock);

  for (printer = (pappl_printer_t *)cupsArrayFirst(system->printers); printer; printer = (pappl_printer_t *)cupsArrayNext(system->printers))
  {
    if (cupsArrayCount(printer->completed_jobs) == 0 || printer->max_completed_jobs <= 0)
      continue;

    pthread_rwlock_wrlock(&printer->rwlock);

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


//
// 'papplJobCancel()' - Cancel a job.
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
// '_papplJobCreate()' - Create a new job object.
//

pappl_job_t *				// O - Job
_papplJobCreate(
    pappl_printer_t *printer,		// I - Printer
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

  job->attrs    = ippNew();
  job->fd       = -1;
  job->format   = format;
  job->name     = job_name;
  job->printer  = printer;
  job->state    = IPP_JSTATE_HELD;
  job->system   = printer->system;

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

  if ((attr = ippAddString(job->attrs, IPP_TAG_JOB, IPP_TAG_NAME, "job-originating-user-name", NULL, username)) != NULL)
    job->username = ippGetString(attr, 0, NULL);

  if ((attr = ippFindAttribute(attrs, "job-impressions", IPP_TAG_INTEGER)) != NULL)
    job->impressions = ippGetInteger(attr, 0);

  // Add job description attributes and add to the jobs array...
  job->job_id = printer->next_job_id ++;

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

  ippAddDate(job->attrs, IPP_TAG_JOB, "date-time-at-creation", ippTimeToDate(time(&job->created)));
  ippAddInteger(job->attrs, IPP_TAG_JOB, IPP_TAG_INTEGER, "job-id", job->job_id);
  ippAddString(job->attrs, IPP_TAG_JOB, IPP_TAG_URI, "job-uri", NULL, job_uri);
  ippAddString(job->attrs, IPP_TAG_JOB, IPP_TAG_URI, "job-uuid", NULL, job_uuid);
  ippAddString(job->attrs, IPP_TAG_JOB, IPP_TAG_URI, "job-printer-uri", NULL, job_printer_uri);
  ippAddInteger(job->attrs, IPP_TAG_JOB, IPP_TAG_INTEGER, "time-at-creation", (int)(job->created - printer->start_time));

  cupsArrayAdd(printer->all_jobs, job);
  cupsArrayAdd(printer->active_jobs, job);

  pthread_rwlock_unlock(&printer->rwlock);

  _papplSystemConfigChanged(printer->system);

  return (job);
}


//
// 'papplJobCreate()' - Create a new job object from a Print-Job or Create-Job request.
//

pappl_job_t *				// O - Job
papplJobCreate(
    pappl_client_t *client)		// I - Client
{
  ipp_attribute_t	*attr;		// Job attribute
  const char		*job_name,	// Job name
			*username;	// Owner


  // Get the requesting-user-name, document format, and name...
  if (client->username[0])
    username = client->username;
  else  if ((attr = ippFindAttribute(client->request, "requesting-user-name", IPP_TAG_NAME)) != NULL)
    username = ippGetString(attr, 0, NULL);
  else
    username = "guest";

  if ((attr = ippFindAttribute(client->request, "job-name", IPP_TAG_NAME)) != NULL)
    job_name = ippGetString(attr, 0, NULL);
  else
    job_name = "Untitled";

  return (_papplJobCreate(client->printer, username, NULL, job_name, client->request));
}


//
// 'papplJobCreateFile()' - Create a file for the document in a job.
//

int					// O - File descriptor or -1 on error
papplJobCreateFile(
    pappl_job_t *job,			// I - Job
    char        *fname,			// I - Filename buffer
    size_t      fnamesize,		// I - Size of filename buffer
    const char  *directory,		// I - Directory to store in
    const char  *ext)			// I - Extension (`NULL` for default)
{
  char			name[256],	// "Safe" filename
			*nameptr;	// Pointer into filename
  const char		*job_name;	// job-name value


  // Make a name from the job-name attribute...
  if ((job_name = ippGetString(ippFindAttribute(job->attrs, "job-name", IPP_TAG_NAME), 0, NULL)) == NULL)
    job_name = "untitled";

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

  return (open(fname, O_WRONLY | O_CREAT | O_TRUNC | O_NOFOLLOW | O_CLOEXEC, 0600));
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

  _papplJobRemoveFile(job);

  free(job);
}


//
// '_papplJobRemoveFile()' - Remove a file in spool directory
//

void
_papplJobRemoveFile(
    pappl_job_t *job)
{
  size_t dirlen = strlen(job->system->directory);

  // Only remove the file if it is in spool directory...
  if (job->filename && job->filename[dirlen] == '/' && !strncmp(job->filename, job->system->directory, dirlen))
  {
    unlink(job->filename);
    free(job->filename);

    job->filename = NULL;
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
  unsigned char	header[8192];		// First 8k bytes of file
  ssize_t	headersize;		// Number of bytes read
  int		filefd;			// File descriptor


  if (!job->format)
  {
    // Open the file
    filefd = open(filename, O_RDONLY);

    // Auto-type the file using the first N bytes of the file...
    memset(header, 0, sizeof(header));
    headersize = read(filefd, (char *)header, sizeof(header));

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
    else
    {
      char    *format = strrchr(filename, '.');

      // Peeking doesn't yield match, Look for extension...
      if (!strcasecmp(format, ".jpg") || !strcasecmp(format, ".jpeg"))
        job->format = "image/jpeg";
      else if (!strcasecmp(format, ".png"))
        job->format = "image/png";
      else if (!strcasecmp(format, ".pwg"))
        job->format = "image/pwg-raster";
      else if (!strcasecmp(format, ".urf"))
        job->format = "image/urf";
      else if (!strcasecmp(format, ".txt"))
        job->format = "text/plain";
      else if (!strcasecmp(format, ".pdf"))
        job->format = "application/pdf";
      else
        job->format = "application/postscript";
    }

    close(filefd);
  }

  // Save the print file information...
  job->filename = strdup(filename);

  // Process the job...
  job->state = IPP_JSTATE_PENDING;

  _papplPrinterCheckJobs(job->printer);
}


//
// 'papplPrinterFindJob()' - Find a job by its "job-id" value.
//

pappl_job_t *				// O - Job or `NULL`
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
