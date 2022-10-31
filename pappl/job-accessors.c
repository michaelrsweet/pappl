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
  "document-password-error",
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
  "warnings-detected",
  "job-hold-until-specified"
};


//
// 'papplJobGetAttribute()' - Get an attribute from a job.
//
// This function gets the named IPP attribute from a job.  The returned
// attribute can be examined using the `ippGetXxx` functions.
//

ipp_attribute_t *			// O - Attribute or `NULL` if not found
papplJobGetAttribute(pappl_job_t *job,	// I - Job
                     const char  *name)	// I - Attribute name
{
  ipp_attribute_t	*attr = NULL;	// Attribute

  if (job)
  {
    _papplRWLockRead(job);
    attr = ippFindAttribute(job->attrs, name, IPP_TAG_ZERO);
    _papplRWUnlock(job);
  }

  return (attr);
}


//
// 'papplJobGetData()' - Get per-job driver data.
//
// This function returns the driver data associated with the job.  It is
// normally only called from drivers to maintain state for the processing of
// the job, for example to store bitmap compression information.
//

void *					// O - Per-job driver data or `NULL` if none
papplJobGetData(pappl_job_t *job)	// I - Job
{
  return (job ? job->data : NULL);
}


//
// 'papplJobGetJobFilename()' - Get the job's filename.
//
// This function returns the filename for the job's document data.
//

const char *				// O - Filename or `NULL` if none
papplJobGetFilename(pappl_job_t *job)	// I - Job
{
  return (job ? job->filename : NULL);
}


//
// 'papplJobGetFormat()' - Get the MIME media type for the job's file.
//
// This function returns the MIME media type for the job's document data.
//

const char *				// O - MIME media type or `NULL` for none
papplJobGetFormat(pappl_job_t *job)	// I - Job
{
  return (job ? job->format : NULL);
}


//
// 'papplJobGetId()' - Get the job ID value.
//
// This function returns the job's unique integer identifier.
//

int					// O - Job ID or `0` for none
papplJobGetID(pappl_job_t *job)		// I - Job
{
  return (job ? job->job_id : 0);
}


//
// 'papplJobGetImpressions()' - Get the number of impressions (sides) in the job.
//
// This function returns the number of impressions in the job's document data.
// An impression is one side of an output page.
//

int					// O - Number of impressions in job
papplJobGetImpressions(pappl_job_t *job)// I - Job
{
  return (job ? job->impressions : 0);
}


//
// 'papplJobGetImpressionsCompleted()' - Get the number of completed impressions
//                                       (sides) in the job.
//
// This function returns the number of impressions that have been printed.  An
// impression is one side of an output page.
//

int					// O - Number of completed impressions in job
papplJobGetImpressionsCompleted(
    pappl_job_t *job)			// I - Job
{
  return (job ? job->impcompleted : 0);
}


//
// 'papplJobGetMessage()' - Get the current job message string, if any.
//
// This function returns the current job message string, if any.
//

const char *				// O - Current "job-state-message" value or `NULL` for none
papplJobGetMessage(pappl_job_t *job)	// I - Job
{
  return (job ? job->message : NULL);
}


//
// 'papplJobGetName()' - Get the job name/title.
//
// This function returns the name or title of the job.
//

const char *				// O - Job name/title or `NULL` for none
papplJobGetName(pappl_job_t *job)	// I - Job
{
  return (job ? job->name : NULL);
}


//
// 'papplJobGetPrinter()' - Get the printer for the job.
//
// This function returns the printer containing the job.
//

pappl_printer_t *			// O - Printer
papplJobGetPrinter(pappl_job_t *job) 	// I - Job
{
  return (job ? job->printer : NULL);
}


//
// 'papplJobGetReasons()' - Get the current job state reasons.
//
// This function returns the current job state reasons bitfield.
//

pappl_jreason_t				// O - IPP "job-state-reasons" bits
papplJobGetReasons(pappl_job_t *job)	// I - Job
{
  return (job ? job->state_reasons : PAPPL_JREASON_NONE);
}


//
// 'papplJobGetState()' - Get the current job state.
//
// This function returns the current job processing state, which is represented
// as an enumeration:
//
// - `IPP_JSTATE_ABORTED`: Job has been aborted by the system due to an error.
// - `IPP_JSTATE_CANCELED`: Job has been canceled by a user.
// - `IPP_JSTATE_COMPLETED`: Job has finished printing.
// - `IPP_JSTATE_HELD`: Job is being held for some reason, typically because
//   the document data is being received.
// - `IPP_JSTATE_PENDING`: Job is queued and waiting to be printed.
// - `IPP_JSTATE_PROCESSING`: Job is being printed.
// - `IPP_JSTATE_STOPPED`: Job is paused, typically when the printer is not
//   ready.
//

ipp_jstate_t				// O - IPP "job-state" value
papplJobGetState(pappl_job_t *job)	// I - Job
{
  return (job ? job->state : IPP_JSTATE_ABORTED);
}


//
// 'papplJobGetTimeCompleted()' - Get the job completion time, if any.
//
// This function returns the date and time when the job reached the completed,
// canceled, or aborted states.  `0` is returned if the job is not yet in one of
// those states.
//

time_t					// O - Date/time when the job completed or `0` if not completed
papplJobGetTimeCompleted(
    pappl_job_t *job)			// I - Job
{
  return (job ? job->completed : 0);
}


//
// 'papplJobGetTimeCreated()' - Get the job creation time.
//
// This function returns the date and time when the job was created.
//

time_t					// O - Date/time when the job was created
papplJobGetTimeCreated(pappl_job_t *job)// I - Job
{
  return (job ? job->created : 0);
}


//
// 'papplJobGetTimeProcessed()' - Get the job processing time.
//
// This function returns the date and time when the job started processing
// (printing).
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
// This function returns the name of the user that submitted the job.
//

const char *				// O - Username or `NULL` for unknown
papplJobGetUsername(pappl_job_t *job)	// I - Job
{
  return (job ? job->username : NULL);
}


//
// 'papplJobIsCanceled()' - Return whether the job is canceled.
//
// This function returns `true` if the job has been canceled or aborted.
//

bool					// O - `true` if the job is canceled or aborted, `false` otherwise
papplJobIsCanceled(pappl_job_t *job)	// I - Job
{
  if (job)
    return (job->is_canceled || job->state == IPP_JSTATE_CANCELED || job->state == IPP_JSTATE_ABORTED);
  else
    return (false);
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
// This function sets the driver data for the specified job.  It is
// normally only called from drivers to maintain state for the processing of
// the job, for example to store bitmap compression information.

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
// This function sets the number of impressions in a job.  An impression is one
// side of an output page.
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
// 'papplJobSetImpressionsCompleted()' - Add completed impressions (sides) to
//                                       the job.
//
// This function updates the number of completed impressions in a job.  An
// impression is one side of an output page.
//


void
papplJobSetImpressionsCompleted(
    pappl_job_t *job,			// I - Job
    int         add)			// I - Number of impressions/sides to add
{
  if (job)
  {
    _papplRWLockWrite(job);
    job->impcompleted += add;
    _papplRWUnlock(job);
  }
}


//
// 'papplJobSetMessage()' - Set the job message string.
//
// This function sets the job message string using a `printf`-style format
// string.
//
// > Note: The maximum length of the job message string is 1023 bytes.
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

    _papplRWLockWrite(job);
    free(job->message);
    job->message = strdup(buffer);
    _papplRWUnlock(job);
  }
}


//
// 'papplJobSetReasons()' - Set the job state reasons bit values.
//
// This function updates the job state reasons bitfield.  The "remove" bits
// are cleared first, then the "add" bits are set.
//

void
papplJobSetReasons(
    pappl_job_t     *job,		// I - Job
    pappl_jreason_t add,		// I - IPP "job-state-reasons" bit value(s) to add
    pappl_jreason_t remove)		// I - IPP "job-state-reasons" bit value(s) to remove
{
  if (job)
  {
    _papplRWLockWrite(job);
    job->state_reasons &= ~remove;
    job->state_reasons |= add;
    _papplRWUnlock(job);
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
    _papplRWLockWrite(job);

    job->state = state;

    if (state == IPP_JSTATE_PROCESSING)
    {
      job->processing = time(NULL);
      job->state_reasons |= PAPPL_JREASON_JOB_PRINTING;
    }
    else if (state >= IPP_JSTATE_CANCELED)
    {
      job->completed = time(NULL);
      job->state_reasons &= (unsigned)~PAPPL_JREASON_JOB_PRINTING;

      if (state == IPP_JSTATE_ABORTED)
	job->state_reasons |= PAPPL_JREASON_ABORTED_BY_SYSTEM;
      else if (state == IPP_JSTATE_CANCELED)
	job->state_reasons |= PAPPL_JREASON_JOB_CANCELED_BY_USER;

      if (job->state_reasons & PAPPL_JREASON_ERRORS_DETECTED)
        job->state_reasons |= PAPPL_JREASON_JOB_COMPLETED_WITH_ERRORS;
      if (job->state_reasons & PAPPL_JREASON_WARNINGS_DETECTED)
        job->state_reasons |= PAPPL_JREASON_JOB_COMPLETED_WITH_WARNINGS;
    }
    _papplRWUnlock(job);
  }
}
