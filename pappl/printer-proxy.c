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

static void		check_fetchable_jobs(pappl_printer_t *printer, http_t *http);
static int		compare_proxy_jobs(_pappl_proxy_job_t *pja, _pappl_proxy_job_t *pjb, void *data);
static _pappl_proxy_job_t *copy_proxy_job(_pappl_proxy_job_t *pj, void *data);
static ipp_t		*do_request(pappl_printer_t *printer, http_t *http, ipp_t *request);
static void		free_proxy_job(_pappl_proxy_job_t *pj, void *data);
static int		subscribe_events(pappl_printer_t *printer, http_t *http);
static void		unsubscribe_events(pappl_printer_t *printer, http_t *http, int sub_id);
static bool		update_active_jobs(pappl_printer_t *printer, http_t *http);
static bool		update_proxy_job_no_lock(pappl_printer_t *printer, int job_id, ipp_jstate_t job_state);
static bool		update_proxy_jobs(pappl_printer_t *printer);
static bool		wait_events(pappl_printer_t *printer, http_t *http, int sub_id, int *seq_number, time_t *next_wait_events);


//
// '_papplPrinterConnectProxy()' - Connect to the Infrastructure Printer and save the resource path as needed.
//

http_t *				// O - Connection to Infrastructure Printer or `NULL` on error
_papplPrinterConnectProxy(
    pappl_printer_t *printer)		// I - Printer
{
  http_t	*http = NULL;		// Connection to server
  char		resource[1024];		// Resource path


  // TODO: Add client credentials support using proxy_uuid as common name
  _papplRWLockWrite(printer);

  if ((http = httpConnectURI(printer->proxy_uri, /*host*/NULL, /*hsize*/0, /*port*/NULL, resource, sizeof(resource), /*blocking*/true, /*msec*/30000, /*cancel*/NULL, /*require_ca*/false)) == NULL)
    papplLogPrinter(printer, PAPPL_LOGLEVEL_ERROR, "Unable to connect to infrastructure printer '%s': %s", printer->proxy_uri, cupsGetErrorString());
  else if (!printer->proxy_resource)
    printer->proxy_resource = strdup(resource);

  _papplRWUnlock(printer);

  return (http);
}


//
// '_papplPrinterRunProxy()' - Run the proxy thread until the printer is deleted or system is shutdown.
//

void *					// O - Thread exit status
_papplPrinterRunProxy(
    pappl_printer_t *printer)		// I - Printer
{
  http_t	*http = NULL;		// Connection to server
  int		sub_id = 0;		// Event subscription ID
  int		seq_number = 0;		// Event sequence number
  bool		fetch_jobs = true,	// Check for new jobs to accept?
		update_jobs = true;	// Do an Update-Active-Jobs request?
  time_t	next_wait_events = 0;	// Next update time


  papplLogPrinter(printer, PAPPL_LOGLEVEL_DEBUG, "Running proxy thread.");

  // Update the list of current proxy jobs...
  _papplRWLockWrite(printer);
  printer->proxy_active = true;
  update_proxy_jobs(printer);
  _papplRWUnlock(printer);

  while (!printer->proxy_terminate && !papplPrinterIsDeleted(printer) && papplSystemIsRunning(printer->system))
  {
    // See if we have anything to do...
    if (sub_id > 0 && !update_jobs && !fetch_jobs && time(NULL) < next_wait_events)
    {
      // Nothing to do, sleep for 1 second and then continue...
      sleep(1);
      continue;
    }

    // Connect to the infrastructure printer...
    // TODO: Add config option to control "require_ca" value for proxies?
    if (!http && (http = _papplPrinterConnectProxy(printer)) == NULL)
    {
      sleep(1);
      continue;
    }

    // If we need to update the list of proxied jobs, do so now...
    if (update_jobs)
      update_jobs = !update_active_jobs(printer, http);

    // Subscribe for events as needed...
    if (sub_id <= 0)
      sub_id = subscribe_events(printer, http);

    // Check for new jobs as needed...
    if (fetch_jobs)
      check_fetchable_jobs(printer, http);

    // Wait for new jobs/state changes...
    fetch_jobs = wait_events(printer, http, sub_id, &seq_number, &next_wait_events);
  }

  // Unsubscribe from events and close the connection to the Infrastructure Printer...
  unsubscribe_events(printer, http, sub_id);
  httpClose(http);

  _papplRWLockWrite(printer);
  printer->proxy_active = false;
  _papplRWUnlock(printer);

  return (NULL);
}


//
// '_papplPrinterUpdateProxyDocument()' - Update the proxy document status.
//

void
_papplPrinterUpdateProxyDocument(
    pappl_printer_t *printer,		// I - Printer
    pappl_job_t     *job,		// I - Local job
    int             doc_number)		// I - Local document number (1-N)
{
  ipp_t			*request;	// IPP request
  _pappl_proxy_job_t	*pjob;		// Proxy job
  _pappl_doc_t		*doc;		// Document


  // Find the proxy job, if any...
  cupsMutexLock(&printer->proxy_jobs_mutex);
  for (pjob = (_pappl_proxy_job_t *)cupsArrayGetFirst(printer->proxy_jobs); pjob; pjob = (_pappl_proxy_job_t *)cupsArrayGetNext(printer->proxy_jobs))
  {
    if (pjob->job == job)
      break;
  }
  cupsMutexUnlock(&printer->proxy_jobs_mutex);

  if (!pjob)
    return;

  // Send a Update-Document-Status request
  _papplRWLockRead(job);

  doc = job->documents + doc_number - 1;

  request = ippNewRequest(IPP_OP_UPDATE_DOCUMENT_STATUS);
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri", /*language*/NULL, printer->proxy_uri);
  ippAddInteger(request, IPP_TAG_OPERATION, IPP_TAG_INTEGER, "job-id", pjob->parent_job_id);
  ippAddInteger(request, IPP_TAG_OPERATION, IPP_TAG_INTEGER, "document-number", doc_number);
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "output-device-uuid", /*language*/NULL, printer->proxy_uuid);

  ippAddInteger(request, IPP_TAG_DOCUMENT, IPP_TAG_INTEGER, "impressions-completed", doc->impcompleted);
  ippAddInteger(request, IPP_TAG_JOB, IPP_TAG_ENUM, "output-device-document-state", (int)doc->state);
  _papplJobCopyStateReasonsNoLock(job, request, IPP_TAG_DOCUMENT, "output-device-document-state-reasons", (ipp_jstate_t)doc->state, doc->state_reasons);

  _papplRWUnlock(job);

  ippDelete(do_request(printer, job->proxy_http, request));

  if (cupsGetError() != IPP_STATUS_OK)
    papplLogPrinter(printer, PAPPL_LOGLEVEL_ERROR, "Unable to job %d document %d status on '%s': %s", job->job_id, doc_number, printer->proxy_uri, cupsGetErrorString());
}


//
// '_papplPrinterUpdateProxyJobNoLock()' - Update the proxy job status.
//

void
_papplPrinterUpdateProxyJobNoLock(
    pappl_printer_t *printer,		// I - Printer
    pappl_job_t     *job)		// I - Local job
{
  ipp_t			*request;	// IPP request
  _pappl_proxy_job_t	*pjob;		// Proxy job


  // Find the proxy job, if any...
  cupsMutexLock(&printer->proxy_jobs_mutex);
  for (pjob = (_pappl_proxy_job_t *)cupsArrayGetFirst(printer->proxy_jobs); pjob; pjob = (_pappl_proxy_job_t *)cupsArrayGetNext(printer->proxy_jobs))
  {
    if (pjob->job == job)
      break;
  }
  cupsMutexUnlock(&printer->proxy_jobs_mutex);

  if (!pjob)
    return;

  // Send a Update-Job-Status request
  request = ippNewRequest(IPP_OP_UPDATE_JOB_STATUS);
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri", /*language*/NULL, printer->proxy_uri);
  ippAddInteger(request, IPP_TAG_OPERATION, IPP_TAG_INTEGER, "job-id", pjob->parent_job_id);
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "output-device-uuid", /*language*/NULL, printer->proxy_uuid);

  ippAddInteger(request, IPP_TAG_JOB, IPP_TAG_INTEGER, "job-impressions-completed", job->impcompleted);
  ippAddInteger(request, IPP_TAG_JOB, IPP_TAG_ENUM, "output-device-job-state", (int)job->state);
  if (job->message)
    ippAddString(request, IPP_TAG_JOB, IPP_TAG_TEXT, "output-device-job-state-message", /*language*/NULL, job->message);
  _papplJobCopyStateReasonsNoLock(job, request, IPP_TAG_JOB, "output-device-job-state-reasons", job->state, job->state_reasons);

  ippDelete(do_request(printer, job->proxy_http, request));

  if (cupsGetError() != IPP_STATUS_OK)
    papplLogPrinter(printer, PAPPL_LOGLEVEL_ERROR, "Unable to job %d status on '%s': %s", job->job_id, printer->proxy_uri, cupsGetErrorString());
}


//
// 'check_fetchable_jobs()' - Check for fetchable jobs.
//

static void
check_fetchable_jobs(
    pappl_printer_t *printer,		// I - Printer
    http_t          *http)		// I - Connection to Infrastructure Printer
{
  // TODO: Send Get-Jobs request
  (void)printer;
  (void)http;
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
// 'do_request()' - Send an IPP request to the Infrastructure Printer with any required authorization.
//

static ipp_t *
do_request(pappl_printer_t *printer,	// I - Printer
           http_t          *http,	// I - Connection to Infrastructure Printer
           ipp_t           *request)	// I - IPP request
{
  // TODO: Add support for OAuth/Basic authorization header
  return (cupsDoRequest(http, request, printer->proxy_resource));
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
// 'subscribe_events()' - Subscribe to event notifications.
//

static int
subscribe_events(
    pappl_printer_t *printer,		// I - Printer
    http_t          *http)		// I - Connection to Infrastructure Printer
{
  int		sub_id = -1;		// Subscription ID
  ipp_t		*request,		// IPP request
		*response;		// IPP response
  ipp_attribute_t *attr;		// Attribute from printer
  static const char * const notify_events[] =
  {					// Notification events
    "job-state-changed",
    "job-fetchable"
  };


  // Send a Create-Printer-Subscriptions request...
  request = ippNewRequest(IPP_OP_CREATE_PRINTER_SUBSCRIPTIONS);
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri", /*language*/NULL, printer->proxy_uri);

  ippAddString(request, IPP_TAG_SUBSCRIPTION, IPP_TAG_KEYWORD, "notify-pull-method", /*language*/NULL, "ippget");
  ippAddStrings(request, IPP_TAG_SUBSCRIPTION, IPP_TAG_KEYWORD, "notify-events", sizeof(notify_events) / sizeof(notify_events[0]), /*language*/NULL, notify_events);
  ippAddInteger(request, IPP_TAG_SUBSCRIPTION, IPP_TAG_INTEGER, "notify-lease-duration", 0);

  response = do_request(printer, http, request);

  // Parse the response and free it...
  if (cupsGetError() != IPP_STATUS_OK)
    papplLogPrinter(printer, PAPPL_LOGLEVEL_ERROR, "Unable to create event notification subscription on '%s': %s", printer->proxy_uri, cupsGetErrorString());
  else if ((attr = ippFindAttribute(response, "notify-subscription-id", IPP_TAG_INTEGER)) == NULL)
    papplLogPrinter(printer, PAPPL_LOGLEVEL_ERROR, "Missing subscription ID from '%s'.", printer->proxy_uri);
  else
    sub_id = ippGetInteger(attr, 0);

  ippDelete(response);

  // Return the subscription ID, if any...
  return (sub_id);
}


//
// 'unsubscribe_events()' - Unsubscribe from event notifications.
//

static void
unsubscribe_events(
    pappl_printer_t *printer,		// I - Printer
    http_t          *http,		// I - Connection to Infrastructure Printer
    int             sub_id)		// I - Subscription ID
{
  ipp_t		*request;		// IPP request


  // Send a Cancel-Subscription request
  request = ippNewRequest(IPP_OP_CANCEL_SUBSCRIPTION);
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri", /*language*/NULL, printer->proxy_uri);
  ippAddInteger(request, IPP_TAG_OPERATION, IPP_TAG_INTEGER, "notify-subscription-id", sub_id);

  ippDelete(do_request(printer, http, request));

  if (cupsGetError() != IPP_STATUS_OK)
    papplLogPrinter(printer, PAPPL_LOGLEVEL_ERROR, "Unable to cancel event notification subscription on '%s': %s", printer->proxy_uri, cupsGetErrorString());
}


//
// 'update_active_jobs()' - Update the list of active proxy jobs with the Infrastructure Printer.
//

static bool				// O - `true` on success, `false` on failure
update_active_jobs(
    pappl_printer_t *printer,		// I - Printer
    http_t          *http)		// I - Connection to Infrastructure Printer
{
  ipp_t		*request,		// IPP request
		*response;		// IPP response
  size_t	i,			// Looping var
		count;			// Number of values
  _pappl_proxy_job_t *job,		// Current proxy job
		key;			// Search key for proxy job
  ipp_attribute_t *job_ids,		// "job-ids" attribute
		*job_states;		// "output-device-job-states" attribute
  bool		check_jobs = false;	// Check for new jobs to print?


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
  response = do_request(printer, http, request);

  if (ippGetStatusCode(response) >= IPP_STATUS_ERROR_BAD_REQUEST)
  {
    papplLogPrinter(printer, PAPPL_LOGLEVEL_ERROR, "Update-Active-Jobs request failed with status %s: %s", ippErrorString(ippGetStatusCode(response)), cupsGetErrorString());
    return (false);
  }

  // Parse the successful response...
  job_ids    = ippFindAttribute(response, "job-ids", IPP_TAG_INTEGER);
  job_states = ippFindAttribute(response, "output-device-job-states", IPP_TAG_ENUM);

  // Get the jobs that have different states...
  if (ippGetGroupTag(job_ids) == IPP_TAG_OPERATION && ippGetGroupTag(job_states) == IPP_TAG_OPERATION && ippGetCount(job_ids) == ippGetCount(job_states))
  {
    // Got a list of jobs with different states...
    for (i = 0, count = ippGetCount(job_ids); i < count; i ++)
      check_jobs |= update_proxy_job_no_lock(printer, ippGetInteger(job_ids, i), (ipp_jstate_t)ippGetInteger(job_states, i));

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

  // Free response from Update-Active-Jobs and return successfully...
  ippDelete(response);

  return (true);
}


//
// 'update_proxy_job_no_lock()' - Update a proxy job's state without changing the printer's rwlock.
//

static bool				// O - Check jobs?
update_proxy_job_no_lock(
    pappl_printer_t *printer,		// I - Printer
    int             job_id,		// I - Job ID
    ipp_jstate_t    remote_state)	// I - Remote job state value
{
  bool			check_jobs = false;
					// Check for jobs to process?
  _pappl_proxy_job_t	key,		// Search key
			*job;		// Proxy job
  ipp_jstate_t		local_state;	// Local jost state value


  // Find the proxy job...
  cupsMutexLock(&printer->proxy_jobs_mutex);

  key.parent_job_id = job_id;
  job = (_pappl_proxy_job_t *)cupsArrayFind(printer->proxy_jobs, &key);

  cupsMutexUnlock(&printer->proxy_jobs_mutex);

  if (job)
  {
    // Update the local job as needed...
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

  return (check_jobs);
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
    cupsMutexLock(&printer->proxy_jobs_mutex);

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

    cupsMutexUnlock(&printer->proxy_jobs_mutex);
  }


  return (printer->proxy_jobs != NULL);
}


//
// 'wait_events()' - Wait for job events.
//

static bool				// O  - `true` if there are jobs to fetch, `false` otherwise
wait_events(
    pappl_printer_t *printer,		// I  - Printer
    http_t          *http,		// I  - Connection to Infrastructure Printer
    int             sub_id,		// I  - Subscription ID
    int             *seq_number,	// IO - Event sequence number
    time_t          *next_wait_events)	// O  - notify-get-interval time
{
  bool		check_jobs = false,	// Return value
		fetch_jobs = false;	// Fetch jobs?
  ipp_t		*request,		// IPP request
		*response;		// IPP response
  ipp_attribute_t *attr;		// IPP attribute
  int		get_interval;		// "notify-get-interval" value
  int		job_id = 0;		// Remote job ID
  ipp_jstate_t	job_state = IPP_JSTATE_ABORTED;
					// Remote job state


  // Send a Get-Notifications request
  request = ippNewRequest(IPP_OP_GET_NOTIFICATIONS);
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri", /*language*/NULL, printer->proxy_uri);
  ippAddInteger(request, IPP_TAG_OPERATION, IPP_TAG_INTEGER, "notify-subscription-ids", sub_id);
  ippAddInteger(request, IPP_TAG_OPERATION, IPP_TAG_INTEGER, "notify-sequence-numbers", *seq_number + 1);

  response = do_request(printer, http, request);

  // Parse the response...
  if (cupsGetError() != IPP_STATUS_ERROR_NOT_FOUND)
    papplLogPrinter(printer, PAPPL_LOGLEVEL_ERROR, "Unable to get event notifications on '%s': %s", printer->proxy_uri, cupsGetErrorString());

  // Honor notify-get-interval between 5 and 60 seconds, otherwise check back in 5 seconds...
  if ((get_interval = ippGetInteger(ippFindAttribute(response, "notify-get-interval", IPP_TAG_INTEGER), 0)) >= 5 && get_interval <= 60)
    *next_wait_events = time(NULL) + get_interval;
  else
    *next_wait_events = time(NULL) + 5;

  // Process events...
  _papplRWLockWrite(printer);

  for (attr = ippGetFirstAttribute(response); attr; attr = ippGetNextAttribute(response))
  {
    // Look at the current attribute/group...
    const char	*name;			// Attribute name
    ipp_tag_t	value_tag;		// Value type
    const char	*keyword;		// "notify-event" value
    int		number;			// "notify-sequence-number" value

    if (ippGetGroupTag(attr) != IPP_TAG_EVENT_NOTIFICATION)
    {
      // Update proxy (remote) print job...
      if (job_id > 0)
        check_jobs |= update_proxy_job_no_lock(printer, job_id, job_state);

      job_id    = 0;
      job_state = IPP_JSTATE_ABORTED;
      continue;
    }

    // In the middle of an event...
    name      = ippGetName(attr);
    value_tag = ippGetValueTag(attr);

    if (!strcmp(name, "notify-job-id") && value_tag == IPP_TAG_INTEGER)
    {
      job_id = ippGetInteger(attr, 0);
    }
    else if (!strcmp(name, "notify-sequence-number") && value_tag == IPP_TAG_INTEGER && (number = ippGetInteger(attr, 0)) > *seq_number)
    {
      *seq_number = number;
    }
    else if (!strcmp(name, "notify-subscribed-event") && value_tag == IPP_TAG_KEYWORD)
    {
      // See what kind of a job event this is...
      keyword = ippGetString(attr, 0, NULL);

      if (!strcmp(keyword, "job-fetchable"))
        fetch_jobs = true;
    }
    else if (!strcmp(name, "job-state") && value_tag == IPP_TAG_ENUM)
    {
      job_state = (ipp_jstate_t)ippGetInteger(attr, 0);
    }
  }

  if (job_id > 0)
    check_jobs |= update_proxy_job_no_lock(printer, job_id, job_state);

  // If any jobs were released, see if they can be started now...
  if (check_jobs)
    _papplPrinterCheckJobsNoLock(printer);

  _papplRWUnlock(printer);

  // Free memory and return...
  ippDelete(response);

  return (fetch_jobs);
}

