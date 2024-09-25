//
// Infrastructure proxy functions for the Printer Application Framework
//
// Copyright © 2024 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#include "printer-private.h"
#include "system-private.h"
#include "job-private.h"


//
// Local types...
//

typedef struct _pappl_proxy_job_s	// Proxy job data
{
  pappl_job_t	*job;			// Local job
  int		parent_job_id;		// parent-job-id value
  char		parent_job_uuid[46];	// parent-job-uuid value
} _pappl_proxy_job_t;


//
// Local functions...
//

static int		compare_proxy_jobs(_pappl_proxy_job_t *pja, _pappl_proxy_job_t *pjb, void *data);
static _pappl_proxy_job_t *copy_proxy_job(_pappl_proxy_job_t *pj, void *data);
static void		free_proxy_job(_pappl_proxy_job_t *pj, void *data);
static bool		update_proxy_jobs(pappl_printer_t *printer);


//
// '_papplPrinterRunProxy()' - Run the proxy thread until the printer is deleted or system is shutdown.
//

void *					// O - Thread exit status
_papplPrinterRunProxy(
    pappl_printer_t *printer)		// I - Printer
{
  http_t	*http = NULL;		// Connection to server
  char		resource[1024];		// Resource path
  ipp_t		*request,		// IPP request
		*response;		// IPP response
//  ipp_attribute_t *attr;		// Current IPP attribute
  int		sub_id = 0;		// Event subscription ID
//  int		seq_number = 0;		// Event sequence number
  bool		update_jobs = true;	// Do an Update-Active-Jobs request?
  time_t	update_time = 0;	// Next update time


  papplLogPrinter(printer, PAPPL_LOGLEVEL_DEBUG, "Running proxy thread.");

  // Update the list of current proxy jobs...
  _papplRWLockWrite(printer);
  printer->proxy_active = true;
  update_proxy_jobs(printer);
  _papplRWUnlock(printer);

  while (!printer->proxy_terminate && !papplPrinterIsDeleted(printer) && papplSystemIsRunning(printer->system))
  {
    // See if we have anything to do...
    if (sub_id > 0 && !update_jobs && (time(NULL) - update_time) < 5)
    {
      // Nothing to do, sleep for 1 second and then continue...
      sleep(1);
      continue;
    }

    // Connect to the infrastructure printer...
    if (!http && (http = httpConnectURI(printer->proxy_uri, /*host*/NULL, /*hsize*/0, /*port*/NULL, resource, sizeof(resource), /*blocking*/true, /*msec*/30000, /*cancel*/NULL, /*require_ca*/false)) == NULL)
    {
      papplLogPrinter(printer, PAPPL_LOGLEVEL_ERROR, "Unable to connect to infrastructure printer '%s': %s", printer->proxy_uri, cupsGetErrorString());
      sleep(1);
      continue;
    }

    // If we need to update the list of proxied jobs, do so now...
    if (update_jobs)
    {
      size_t		i,		// Looping var
			count;		// Number of values
      _pappl_proxy_job_t *job,		// Current proxy job
			key;		// Search key for proxy job
      ipp_attribute_t	*job_ids,	// "job-ids" attribute
			*job_states;	// "output-device-job-states" attribute
      bool		check_jobs = false;
					// Check for new jobs to print?

      // Create an Update-Active-Jobs request...
      _papplRWLockRead(printer);

      request = ippNewRequest(IPP_OP_UPDATE_ACTIVE_JOBS);
      ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri", /*language*/NULL, printer->proxy_uri);
      ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "output-device-uuid", /*language*/NULL, printer->proxy_uuid);

      if ((count = cupsArrayGetCount(printer->proxy_jobs)) > 0)
      {
        job_ids    = ippAddIntegers(request, IPP_TAG_OPERATION, IPP_TAG_INTEGER, "job-ids", count, /*values*/NULL);
        job_states = ippAddIntegers(request, IPP_TAG_OPERATION, IPP_TAG_ENUM, "output-device-job-states", count, /*values*/NULL);

        for (i = 0; i < count; i ++)
        {
          job = (_pappl_proxy_job_t *)cupsArrayGetElement(printer->proxy_jobs, i);
          ippSetInteger(request, &job_ids, i, job->parent_job_id);
          ippSetInteger(request, &job_states, i, (int)papplJobGetState(job->job));
        }
      }

      _papplRWUnlock(printer);

      // Send the request...
      response = cupsDoRequest(http, request, resource);

      if (ippGetStatusCode(response) >= IPP_STATUS_ERROR_BAD_REQUEST)
      {
        papplLogPrinter(printer, PAPPL_LOGLEVEL_ERROR, "Update-Active-Jobs request failed with status %s: %s", ippErrorString(ippGetStatusCode(response)), cupsGetErrorString());
        sleep(1);
        continue;
      }

      // Parse the successful response...
      update_jobs = false;

      job_ids    = ippFindAttribute(response, "job-ids", IPP_TAG_INTEGER);
      job_states = ippFindAttribute(response, "output-device-job-states", IPP_TAG_ENUM);

      // Get the jobs that have different states...
      if (ippGetGroupTag(job_ids) == IPP_TAG_OPERATION && ippGetGroupTag(job_states) == IPP_TAG_OPERATION && ippGetCount(job_ids) == ippGetCount(job_states))
      {
        // Got a list of jobs with different states...
        ipp_jstate_t	local_state,	// Local job state
			remote_state;	// Remote job state

	_papplRWLockWrite(printer);

        for (i = 0, count = ippGetCount(job_ids); i < count; i ++)
        {
          key.parent_job_id = ippGetInteger(job_ids, i);
          remote_state      = (ipp_jstate_t)ippGetInteger(job_states, i);

          if ((job = (_pappl_proxy_job_t *)cupsArrayFind(printer->proxy_jobs, &key)) != NULL)
          {
            local_state = papplJobGetState(job->job);

            if (remote_state >= IPP_JSTATE_CANCELED && local_state < IPP_JSTATE_CANCELED)
            {
              // Cancel local job
              _papplRWLockWrite(job->job);
              _papplJobCancelNoLock(job->job);
              _papplRWUnlock(job->job);
            }
            else if (remote_state == IPP_JSTATE_PENDING && local_state == IPP_JSTATE_HELD)
            {
              // Release held job
              _papplRWLockWrite(job->job);
              _papplJobReleaseNoLock(job->job, /*username*/NULL);
              check_jobs = true;
              _papplRWUnlock(job->job);
            }
          }
        }

        // Get the jobs that no longer exist on the Infrastructure Printer...
        if ((job_ids = ippFindNextAttribute(response, "job-ids", IPP_TAG_INTEGER)) != NULL && ippGetGroupTag(job_ids) == IPP_TAG_UNSUPPORTED_GROUP)
        {
	  for (i = 0, count = ippGetCount(job_ids); i < count; i ++)
	  {
	    key.parent_job_id = ippGetInteger(job_ids, i);

	    if ((job = (_pappl_proxy_job_t *)cupsArrayFind(printer->proxy_jobs, &key)) != NULL)
	    {
	      // Make sure the local job is canceled so it doesn't show up again...
              _papplRWLockWrite(job->job);
              _papplJobCancelNoLock(job->job);
              _papplRWUnlock(job->job);

	      // Remove the proxy job that no longer exists...
	      cupsArrayRemove(printer->proxy_jobs, job);
	    }
	  }
	}

        // If any jobs were released, see if they can be started now...
        if (check_jobs)
          _papplPrinterCheckJobsNoLock(printer);

	_papplRWUnlock(printer);
      }

      // Free response from Update-Active-Jobs...
      ippDelete(response);
    }

    // TODO: Poll for new jobs
    sleep(1);
  }

  _papplRWLockWrite(printer);
  printer->proxy_active = false;
  _papplRWUnlock(printer);

  return (NULL);
}


//
// 'compare_proxy_jobs()' - Compare two proxy jobs.
//

static int				// O - Result of comparison
compare_proxy_jobs(
    _pappl_proxy_job_t *pja,		// I - First proxy job
    _pappl_proxy_job_t *pjb,		// I - Second proxy job
    void               *data)		// I - Callback data (unused)
{
  (void)data;

  return (pjb->parent_job_id - pja->parent_job_id);
}


//
// 'copy_proxy_job()' - Copy a proxy job.
//

static _pappl_proxy_job_t *		// O - New proxy job
copy_proxy_job(_pappl_proxy_job_t *pj,	// I - Proxy job
	       void               *data)// I - Callback data (unused)
{
  _pappl_proxy_job_t *npj;		// New proxy job


  (void)data;

  if ((npj = (_pappl_proxy_job_t *)calloc(1, sizeof(_pappl_proxy_job_t))) != NULL)
    memcpy(npj, pj, sizeof(_pappl_proxy_job_t));

  return (npj);
}


//
// 'free_proxy_job()' - Free a proxy job.
//

static void
free_proxy_job(_pappl_proxy_job_t *pj,	// I - Proxy job
               void               *data)// I - Callback data (unused)
{
  (void)data;

  free(pj);
}


//
// 'update_proxy_jobs()' - Update the available proxy jobs.
//

static bool				// O - `true` on success, `false` on error
update_proxy_jobs(
    pappl_printer_t *printer)		// I - Printer
{
  // Build a local list of proxied jobs, if any...
  if (!printer->proxy_jobs)
  {
    pappl_job_t		*job;		// Current job
    ipp_attribute_t	*attr;		// Job attribute
    _pappl_proxy_job_t	pj;		// Proxy job

    // Create the proxy jobs array...
    printer->proxy_jobs = cupsArrayNew((cups_array_cb_t)compare_proxy_jobs, /*cbdata*/NULL, /*hash_cb*/NULL, /*hash_size*/0, (cups_acopy_cb_t)copy_proxy_job, (cups_afree_cb_t)free_proxy_job);

    // Scan existing jobs for parent-job-xxx attributes...
    for (job = (pappl_job_t *)cupsArrayGetFirst(printer->all_jobs); job; job = (pappl_job_t *)cupsArrayGetNext(printer->all_jobs))
    {
      if (papplJobGetState(job) >= IPP_JSTATE_CANCELED)
        continue;

      if ((attr = ippFindAttribute(job->attrs, "parent-job-id", IPP_TAG_INTEGER)) != NULL)
      {
        pj.job           = job;
        pj.parent_job_id = ippGetInteger(attr, 0);

        if ((attr = ippFindAttribute(job->attrs, "parent-job-uuid", IPP_TAG_URI)) != NULL)
        {
          // Saw parent-job-id and parent-job-uuid, add it...
          cupsCopyString(pj.parent_job_uuid, ippGetString(attr, 0, NULL), sizeof(pj.parent_job_uuid));
          cupsArrayAdd(printer->proxy_jobs, &pj);
        }
      }
    }
  }


  return (printer->proxy_jobs != NULL);
}

