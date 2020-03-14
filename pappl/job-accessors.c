//
// Job accessor functions for the Printer Application Framework
//
// Copyright © 2020 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

//
// Include necessary headers...
//

#include "pappl-private.h"


//
// Local globals...
//

static const char * const pappl_jreasons[] =
{
  "aborted-by-system",
  "compression-error",
  "document-format-error",
  "document-permission-error",
  "document-unprintable-error",
  "errors-detected",
  "job-canceled-at-device",
  "job-canceled-by-user",
  "job-completed-successfully",
  "job-completed-with-errors",
  "job-completed-with-warnings",
  "job-data-insufficient",
  "job-incoming",
  "job-printing",
  "job-queued",
  "job-spooling",
  "printer-stopped",
  "printer-stopped-partly",
  "processing-to-stop-point",
  "queued-in-device",
  "warnings-detected"
};


//
// 'papplJobGetAttribute()' - Get an attribute from a job.
//

ipp_attribute_t *			// O - Attribute or `NULL` if not found
papplJobGetAttribute(pappl_job_t *job,	// I - Job
                     const char  *name)	// I - Attribute name
{
  ipp_attribute_t	*attr = NULL;	// Attribute

  if (job)
  {
    pthread_rwlock_rdlock(&job->rwlock);
    attr = ippFindAttribute(job->attrs, name, IPP_TAG_ZERO);
    pthread_rwlock_unlock(&job->rwlock);
  }

  return (attr);
}


//
// 'papplJobGetData()' - Get per-job driver data.
//

void *					// O - Per-job driver data or `NULL` if none
papplJobGetData(pappl_job_t *job)	// I - Job
{
  return (job ? job->data : NULL);
}


//
// 'papplJobGetJobFilename()' - Get the job's filename.
//

const char *				// O - Filename or `NULL` if none
papplJobGetFilename(pappl_job_t *job)	// I - Job
{
  return (job ? job->filename : NULL);
}


//
// 'papplJobGetFormat()' - Get the MIME media type for the job's file.
//

const char *				// O - MIME media type or `NULl` for none
papplJobGetFormat(pappl_job_t *job)	// I - Job
{
  return (job ? job->format : NULL);
}


//
// 'papplJobGetId()' - Get the job ID value.
//

int					// O - Job ID or `0` for none
papplJobGetId(pappl_job_t *job)		// I - Job
{
  return (job ? job->job_id : 0);
}


//
// 'papplJobGetImpressions()' - Get the number of impressions (sides) in the job.
//

int					// O - Number of impressions in job
papplJobGetImpressions(pappl_job_t *job)// I - Job
{
  return (job ? job->impressions : 0);
}


//
// 'papplJobGetImpressionsCompleted()' - Get the number of completed impressions (sides) in the job.
//

int					// O - Number of completed impressions in job
papplJobGetImpressionsCompleted(
    pappl_job_t *job)			// I - Job
{
  return (job ? job->impcompleted : 0);
}


//
// 'papplJobGetMessage()' - .
//

const char *				// O - Current job-state-message value or `NULL` for none
papplJobGetMessage(pappl_job_t *job)	// I - Job
{
  return (job ? job->message : NULL);
}


//
// 'papplJobGetName()' - Get the job name/title.
//

const char *				// O - Job name/title or `NULL` for none
papplJobGetName(pappl_job_t *job)	// I - Job
{
  return (job ? job->name : NULL);
}


//
// 'papplJobGetReasons()' - Get the curret job state reasons.
//

pappl_jreason_t				// O - IPP "job-state-reasons" bits
papplJobGetReasons(pappl_job_t *job)	// I - Job
{
  return (job ? job->state_reasons : PAPPL_JREASON_NONE);
}


//
// 'papplJobGetState()' - Get the current job state.
//

ipp_jstate_t				// O - IPP "job-state" value
papplJobGetState(pappl_job_t *job)	// I - Job
{
  return (job ? job->state : IPP_JSTATE_ABORTED);
}


//
// 'papplJobGetTimeCompleted()' - Get the date and time when the job reached the completed, canceled, or aborted states.
//

time_t					// O - Date/time when the job completed or `0` if not completed
papplJobGetTimeCompleted(
    pappl_job_t *job)			// I - Job
{
  return (job ? job->completed : 0);
}


//
// 'papplJobGetTimeCreated()' - Get the date and time when the job was created.
//

time_t					// O - Date/time when the job was created
papplJobGetTimeCreated(pappl_job_t *job)// I - Job
{
  return (job ? job->created : 0);
}


//
// 'papplJobGetTimeProcessed()' - Get the date and time hen the job started processing (printing).
//

time_t					// O - Date/time when the job started processing (printing) or `0` if not yet processed
papplJobGetTimeProcessed(
    pappl_job_t *job)			// I - Job
{
  return (job ? job->processing : 0);
}


//
// 'papplJobGetUsername()' - Get the name of the user that submitted the job.
//

const char *				// O - Username or `NULL` for unknown
papplJobGetUsername(pappl_job_t *job)	// I - Job
{
  return (job ? job->username : NULL);
}


//
// '_papplJobReasonString()' - Return the keyword value associated with the IPP "job-state-reasons" bit value.
//

const char *				// O - IPP "job-state-reasons" keyword value
_papplJobReasonString(
    pappl_jreason_t reason)		// I - IPP "job-state-reasons" bit value
{
  if (reason == PAPPL_JREASON_NONE)
    return ("none");
  else
    return (_PAPPL_LOOKUP_STRING(reason, pappl_jreasons));
}


//
// 'papplJobSetData()' - Set the per-job driver data pointer.
//

void
papplJobSetData(pappl_job_t *job,	// I - Job
                void        *data)	// I - Data pointer
{
  if (job)
    job->data = data;
}


//
// 'papplJobSetImpressions()' - Set the number of impressions (sides) in a job.
//

void
papplJobSetImpressions(
    pappl_job_t *job,			// I - Job
    int         impressions)		// I - Number of impressions/sides
{
  if (job)
    job->impressions = impressions;
}


//
// 'papplJobSetImpressionsCompleted()' - Add completed impressions (sides) to the job.
//

void
papplJobSetImpressionsCompleted(
    pappl_job_t *job,			// I - Job
    int         add)			// I - Number of impressions/sides to add
{
  if (job)
  {
    pthread_rwlock_wrlock(&job->rwlock);
    job->impcompleted += add;
    pthread_rwlock_unlock(&job->rwlock);
  }
}


//
// 'papplJobSetMessage()' - Set the job message string..
//

void
papplJobSetMessage(pappl_job_t *job,	// I - Job
                   const char *message,	// I - Printf-style message string
                   ...)			// I - Additional arguments as needed
{
  if (job)
  {
    char	buffer[1024];		// Message buffer
    va_list	ap;			// Pointer to arguments

    va_start(ap, message);
    vsnprintf(buffer, sizeof(buffer), message, ap);
    va_end(ap);

    pthread_rwlock_wrlock(&job->rwlock);
    free(job->message);
    job->message = strdup(buffer);
    pthread_rwlock_unlock(&job->rwlock);
  }
}


//
// 'papplJobSetReasons()' - Set the IPP "job-state-reasons" bit values.
//

void
papplJobSetReasons(
    pappl_job_t     *job,		// I - Job
    pappl_jreason_t add,		// I - IPP "job-state-reasons" bit value(s) to add
    pappl_jreason_t remove)		// I - IPP "job-state-reasons" bit value(s) to remove
{
  if (job)
  {
    pthread_rwlock_wrlock(&job->rwlock);
    job->state_reasons &= ~remove;
    job->state_reasons |= add;
    pthread_rwlock_unlock(&job->rwlock);
  }
}


//
// '_papplJobSetState()' - Set the IPP "job-state" value.
//

void
_papplJobSetState(pappl_job_t  *job,	// I - Job
                  ipp_jstate_t state)	// I - New IPP "job-state" value
{
  if (job && job->state != state)
  {
    pthread_rwlock_wrlock(&job->rwlock);

    job->state = state;

    if (state == IPP_JSTATE_PROCESSING)
    {
      job->processing = time(NULL);
      job->state_reasons |= PAPPL_JREASON_JOB_PRINTING;
    }
    else if (state >= IPP_JSTATE_CANCELED)
    {
      job->completed = time(NULL);
      job->state_reasons &= ~PAPPL_JREASON_JOB_PRINTING;

      if (state == IPP_JSTATE_ABORTED)
	job->state_reasons |= PAPPL_JREASON_ABORTED_BY_SYSTEM;
      else if (state == IPP_JSTATE_CANCELED)
	job->state_reasons |= PAPPL_JREASON_JOB_CANCELED_BY_USER;

      if (job->state_reasons & PAPPL_JREASON_ERRORS_DETECTED)
        job->state_reasons |= PAPPL_JREASON_JOB_COMPLETED_WITH_ERRORS;
      if (job->state_reasons & PAPPL_JREASON_WARNINGS_DETECTED)
        job->state_reasons |= PAPPL_JREASON_JOB_COMPLETED_WITH_WARNINGS;
    }
    pthread_rwlock_unlock(&job->rwlock);
  }
}
