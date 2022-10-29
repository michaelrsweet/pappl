//
// Job IPP processing for the Printer Application Framework
//
// Copyright © 2019-2022 by Michael R Sweet.
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
// Local type...
//

typedef struct _pappl_attr_s		// Input attribute structure
{
  const char	*name;			// Attribute name
  ipp_tag_t	value_tag;		// Value tag
  int		max_count;		// Max number of values
} _pappl_attr_t;


//
// Local functions...
//

static void		ipp_cancel_job(pappl_client_t *client);
static void		ipp_close_job(pappl_client_t *client);
static void		ipp_get_job_attributes(pappl_client_t *client);
static void		ipp_hold_job(pappl_client_t *client);
static void		ipp_release_job(pappl_client_t *client);
static void		ipp_send_document(pappl_client_t *client);


//
// '_papplJobCopyAttributes()' - Copy job attributes to the response.
//

void
_papplJobCopyAttributes(
    pappl_job_t    *job,		// I - Job
    pappl_client_t *client,		// I - Client
    cups_array_t   *ra)			// I - requested-attributes
{
  _papplCopyAttributes(client->response, job->attrs, ra, IPP_TAG_JOB, 0);

  if (!ra || cupsArrayFind(ra, "date-time-at-creation"))
    ippAddDate(client->response, IPP_TAG_JOB, "date-time-at-creation", ippTimeToDate(job->created));

  if (!ra || cupsArrayFind(ra, "date-time-at-completed"))
  {
    if (job->completed)
      ippAddDate(client->response, IPP_TAG_JOB, "date-time-at-completed", ippTimeToDate(job->completed));
    else
      ippAddOutOfBand(client->response, IPP_TAG_JOB, IPP_TAG_NOVALUE, "date-time-at-completed");
  }

  if (!ra || cupsArrayFind(ra, "date-time-at-processing"))
  {
    if (job->processing)
      ippAddDate(client->response, IPP_TAG_JOB, "date-time-at-processing", ippTimeToDate(job->processing));
    else
      ippAddOutOfBand(client->response, IPP_TAG_JOB, IPP_TAG_NOVALUE, "date-time-at-processing");
  }

  if (!ra || cupsArrayFind(ra, "job-impressions"))
    ippAddInteger(client->response, IPP_TAG_JOB, IPP_TAG_INTEGER, "job-impressions", job->impressions);

  if (!ra || cupsArrayFind(ra, "job-impressions-completed"))
    ippAddInteger(client->response, IPP_TAG_JOB, IPP_TAG_INTEGER, "job-impressions-completed", job->impcompleted);

  if (!ra || cupsArrayFind(ra, "job-printer-up-time"))
    ippAddInteger(client->response, IPP_TAG_JOB, IPP_TAG_INTEGER, "job-printer-up-time", (int)(time(NULL) - client->printer->start_time));

  _papplJobCopyState(job, IPP_TAG_JOB, client->response, ra);

  if (!ra || cupsArrayFind(ra, "time-at-creation"))
    ippAddInteger(client->response, IPP_TAG_JOB, IPP_TAG_INTEGER, "time-at-creation", (int)(job->created - client->printer->start_time));

  if (!ra || cupsArrayFind(ra, "time-at-completed"))
    ippAddInteger(client->response, IPP_TAG_JOB, job->completed ? IPP_TAG_INTEGER : IPP_TAG_NOVALUE, "time-at-completed", (int)(job->completed - client->printer->start_time));

  if (!ra || cupsArrayFind(ra, "time-at-processing"))
    ippAddInteger(client->response, IPP_TAG_JOB, job->processing ? IPP_TAG_INTEGER : IPP_TAG_NOVALUE, "time-at-processing", (int)(job->processing - client->printer->start_time));
}


//
// '_papplJobCopyDocumentData()' - Finish receiving a document file in an IPP
//                                 request and start processing.
//

void
_papplJobCopyDocumentData(
    pappl_client_t *client,		// I - Client
    pappl_job_t    *job)		// I - Job
{
  char			filename[1024],	// Filename buffer
			buffer[4096];	// Copy buffer
  ssize_t		bytes,		// Bytes read
			total = 0;	// Total bytes copied
  cups_array_t		*ra;		// Attributes to send in response


  // If we have a PWG or Apple raster file, process it directly or return
  // server-error-busy...
  if (!strcmp(job->format, "image/pwg-raster") || !strcmp(job->format, "image/urf"))
  {
    if (job->printer->processing_job)
    {
      papplClientRespondIPP(client, IPP_STATUS_ERROR_BUSY, "Currently printing another job.");
      goto abort_job;
    }
    else if (job->printer->hold_new_jobs)
    {
      papplClientRespondIPP(client, IPP_STATUS_ERROR_NOT_ACCEPTING_JOBS, "Currently holding new jobs.");
      goto abort_job;
    }

    job->state = IPP_JSTATE_PENDING;

    _papplJobProcessRaster(job, client);

    goto complete_job;
  }

  // Create a file for the request data...
  if ((job->fd = papplJobOpenFile(job, filename, sizeof(filename), client->system->directory, NULL, "w")) < 0)
  {
    papplClientRespondIPP(client, IPP_STATUS_ERROR_INTERNAL, "Unable to create print file: %s", strerror(errno));

    goto abort_job;
  }

  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "Created job file \"%s\", format \"%s\".", filename, job->format);

  while ((bytes = httpRead(client->http, buffer, sizeof(buffer))) > 0)
  {
    papplLogClient(client, PAPPL_LOGLEVEL_DEBUG, "Read %d bytes...", (int)bytes);

    if (write(job->fd, buffer, (size_t)bytes) < bytes)
    {
      int error = errno;		// Write error

      close(job->fd);
      job->fd = -1;

      unlink(filename);

      papplClientRespondIPP(client, IPP_STATUS_ERROR_INTERNAL, "Unable to write print file: %s", strerror(error));

      goto abort_job;
    }

    total += (size_t)bytes;
  }

  if (bytes < 0)
  {
    // Got an error while reading the print data, so abort this job.
    close(job->fd);
    job->fd = -1;

    unlink(filename);

    papplClientRespondIPP(client, IPP_STATUS_ERROR_INTERNAL, "Unable to read print file.");

    goto abort_job;
  }

  if (close(job->fd))
  {
    int error = errno;			// Write error

    job->fd = -1;

    unlink(filename);

    papplClientRespondIPP(client, IPP_STATUS_ERROR_INTERNAL, "Unable to write print file: %s", strerror(error));

    goto abort_job;
  }

  papplLogJob(job, PAPPL_LOGLEVEL_INFO, "Received %lu bytes of document data.", (unsigned long)total);

  job->fd = -1;

  // Submit the job for processing...
  _papplJobSubmitFile(job, filename);

  complete_job:

  // Return the job info...
  papplClientRespondIPP(client, IPP_STATUS_OK, NULL);

  ra = cupsArrayNew((cups_array_cb_t)strcmp, NULL, NULL, 0, NULL, NULL);
  cupsArrayAdd(ra, "job-id");
  cupsArrayAdd(ra, "job-state");
  cupsArrayAdd(ra, "job-state-message");
  cupsArrayAdd(ra, "job-state-reasons");
  cupsArrayAdd(ra, "job-uri");

  _papplJobCopyAttributes(job, client, ra);
  cupsArrayDelete(ra);
  return;

  // If we get here we had to abort the job...
  abort_job:

  _papplClientFlushDocumentData(client);

  job->state     = IPP_JSTATE_ABORTED;
  job->completed = time(NULL);

  pthread_rwlock_wrlock(&client->printer->rwlock);

  cupsArrayRemove(client->printer->active_jobs, job);
  cupsArrayAdd(client->printer->completed_jobs, job);

  if (!client->system->clean_time)
    client->system->clean_time = time(NULL) + 60;

  pthread_rwlock_unlock(&client->printer->rwlock);

  ra = cupsArrayNew((cups_array_cb_t)strcmp, NULL, NULL, 0, NULL, NULL);
  cupsArrayAdd(ra, "job-id");
  cupsArrayAdd(ra, "job-state");
  cupsArrayAdd(ra, "job-state-reasons");
  cupsArrayAdd(ra, "job-uri");

  _papplJobCopyAttributes(job, client, ra);
  cupsArrayDelete(ra);
}


//
// '_papplJobCopyState()' - Copy the job-state-xxx sttributes.
//

void
_papplJobCopyState(
    pappl_job_t    *job,	// I - Job
    ipp_tag_t      group_tag,	// I - Group tag
    ipp_t          *ipp,	// I - IPP message
    cups_array_t   *ra)		// I - Requested attributes
{
  if (!ra || cupsArrayFind(ra, "job-state"))
    ippAddInteger(ipp, group_tag, IPP_TAG_ENUM, "job-state", (int)job->state);

  if (!ra || cupsArrayFind(ra, "job-state-message"))
  {
    if (job->message)
    {
      ippAddString(ipp, group_tag, IPP_TAG_TEXT, "job-state-message", NULL, job->message);
    }
    else
    {
      switch (job->state)
      {
	case IPP_JSTATE_PENDING :
	    ippAddString(ipp, group_tag, IPP_CONST_TAG(IPP_TAG_TEXT), "job-state-message", NULL, "Job pending.");
	    break;

	case IPP_JSTATE_HELD :
	    if (job->fd >= 0)
	      ippAddString(ipp, group_tag, IPP_CONST_TAG(IPP_TAG_TEXT), "job-state-message", NULL, "Job incoming.");
	    else if (ippFindAttribute(job->attrs, "job-hold-until", IPP_TAG_ZERO))
	      ippAddString(ipp, group_tag, IPP_CONST_TAG(IPP_TAG_TEXT), "job-state-message", NULL, "Job held.");
	    else
	      ippAddString(ipp, group_tag, IPP_CONST_TAG(IPP_TAG_TEXT), "job-state-message", NULL, "Job created.");
	    break;

	case IPP_JSTATE_PROCESSING :
	    if (job->is_canceled)
	      ippAddString(ipp, group_tag, IPP_CONST_TAG(IPP_TAG_TEXT), "job-state-message", NULL, "Job canceling.");
	    else
	      ippAddString(ipp, group_tag, IPP_CONST_TAG(IPP_TAG_TEXT), "job-state-message", NULL, "Job printing.");
	    break;

	case IPP_JSTATE_STOPPED :
	    ippAddString(ipp, group_tag, IPP_CONST_TAG(IPP_TAG_TEXT), "job-state-message", NULL, "Job stopped.");
	    break;

	case IPP_JSTATE_CANCELED :
	    ippAddString(ipp, group_tag, IPP_CONST_TAG(IPP_TAG_TEXT), "job-state-message", NULL, "Job canceled.");
	    break;

	case IPP_JSTATE_ABORTED :
	    ippAddString(ipp, group_tag, IPP_CONST_TAG(IPP_TAG_TEXT), "job-state-message", NULL, "Job aborted.");
	    break;

	case IPP_JSTATE_COMPLETED :
	    ippAddString(ipp, group_tag, IPP_CONST_TAG(IPP_TAG_TEXT), "job-state-message", NULL, "Job completed.");
	    break;
      }
    }
  }

  if (!ra || cupsArrayFind(ra, "job-state-reasons"))
  {
    if (job->state_reasons)
    {
      size_t		num_values = 0;	// Number of string values
      const char	*svalues[32];	// String values
      pappl_jreason_t	bit;		// Current reason bit

      for (bit = PAPPL_JREASON_ABORTED_BY_SYSTEM; bit <= PAPPL_JREASON_WARNINGS_DETECTED; bit *= 2)
      {
        if (bit & job->state_reasons)
          svalues[num_values ++] = _papplJobReasonString(bit);
      }

      ippAddStrings(ipp, group_tag, IPP_CONST_TAG(IPP_TAG_KEYWORD), "job-state-reasons", IPP_NUM_CAST num_values, NULL, svalues);
    }
    else
    {
      switch (job->state)
      {
	case IPP_JSTATE_PENDING :
	    ippAddString(ipp, group_tag, IPP_CONST_TAG(IPP_TAG_KEYWORD), "job-state-reasons", NULL, "none");
	    break;

	case IPP_JSTATE_HELD :
	    if (job->fd >= 0)
	      ippAddString(ipp, group_tag, IPP_CONST_TAG(IPP_TAG_KEYWORD), "job-state-reasons", NULL, "job-incoming");
	    else
	      ippAddString(ipp, group_tag, IPP_CONST_TAG(IPP_TAG_KEYWORD), "job-state-reasons", NULL, "job-data-insufficient");
	    break;

	case IPP_JSTATE_PROCESSING :
	    if (job->is_canceled)
	      ippAddString(ipp, group_tag, IPP_CONST_TAG(IPP_TAG_KEYWORD), "job-state-reasons", NULL, "processing-to-stop-point");
	    else
	      ippAddString(ipp, group_tag, IPP_CONST_TAG(IPP_TAG_KEYWORD), "job-state-reasons", NULL, "job-printing");
	    break;

	case IPP_JSTATE_STOPPED :
	    ippAddString(ipp, group_tag, IPP_CONST_TAG(IPP_TAG_KEYWORD), "job-state-reasons", NULL, "job-stopped");
	    break;

	case IPP_JSTATE_CANCELED :
	    ippAddString(ipp, group_tag, IPP_CONST_TAG(IPP_TAG_KEYWORD), "job-state-reasons", NULL, "job-canceled-by-user");
	    break;

	case IPP_JSTATE_ABORTED :
	    ippAddString(ipp, group_tag, IPP_CONST_TAG(IPP_TAG_KEYWORD), "job-state-reasons", NULL, "aborted-by-system");
	    break;

	case IPP_JSTATE_COMPLETED :
	    ippAddString(ipp, group_tag, IPP_CONST_TAG(IPP_TAG_KEYWORD), "job-state-reasons", NULL, "job-completed-successfully");
	    break;
      }
    }
  }
}


//
// '_papplJobProcessIPP()' - Process an IPP Job request.
//

void
_papplJobProcessIPP(
    pappl_client_t *client)		// I - Client
{
  switch (ippGetOperation(client->request))
  {
    case IPP_OP_CANCEL_JOB :
	ipp_cancel_job(client);
	break;

    case IPP_OP_CLOSE_JOB :
	ipp_close_job(client);
	break;

    case IPP_OP_GET_JOB_ATTRIBUTES :
	ipp_get_job_attributes(client);
	break;

    case IPP_OP_HOLD_JOB :
	ipp_hold_job(client);
	break;

    case IPP_OP_RELEASE_JOB :
	ipp_release_job(client);
	break;

    case IPP_OP_SEND_DOCUMENT :
	ipp_send_document(client);
	break;

    default :
        if (client->system->op_cb && (client->system->op_cb)(client, client->system->op_cbdata))
          break;

	papplClientRespondIPP(client, IPP_STATUS_ERROR_OPERATION_NOT_SUPPORTED, "Operation not supported.");
	break;
  }
}


//
// '_papplJobValidateDocumentAttributes()' - Determine whether the document
//                                           attributes are valid.
//
// When one or more document attributes are invalid, this function adds a
// suitable response and attributes to the unsupported group.
//

bool					// O - `true` if valid, `false` if not
_papplJobValidateDocumentAttributes(
    pappl_client_t *client)		// I - Client
{
  bool			valid = true;	// Valid attributes?
  ipp_op_t		op = ippGetOperation(client->request);
					// IPP operation
  const char		*op_name = ippOpString(op);
					// IPP operation name
  ipp_attribute_t	*attr,		// Current attribute
			*supported;	// xxx-supported attribute
  const char		*compression = NULL,
					// compression value
			*format = NULL;	// document-format value


  // Check operation attributes...
  if ((attr = ippFindAttribute(client->request, "compression", IPP_TAG_ZERO)) != NULL)
  {
    // If compression is specified, only accept a supported value in a Print-Job
    // or Send-Document request...
    compression = ippGetString(attr, 0, NULL);
    supported   = ippFindAttribute(client->printer->attrs, "compression-supported", IPP_TAG_KEYWORD);

    if (ippGetCount(attr) != 1 || ippGetValueTag(attr) != IPP_TAG_KEYWORD || ippGetGroupTag(attr) != IPP_TAG_OPERATION || (op != IPP_OP_PRINT_JOB && op != IPP_OP_SEND_DOCUMENT && op != IPP_OP_VALIDATE_JOB) || !ippContainsString(supported, compression))
    {
      papplClientRespondIPPUnsupported(client, attr);
      valid = false;
    }
    else
    {
      papplLogClient(client, PAPPL_LOGLEVEL_DEBUG, "%s \"compression\"='%s'", op_name, compression);

      ippAddString(client->request, IPP_TAG_JOB, IPP_TAG_KEYWORD, "compression-supplied", NULL, compression);

      if (strcmp(compression, "none"))
      {
        papplLogClient(client, PAPPL_LOGLEVEL_INFO, "Receiving job file with '%s' compression.", compression);
        httpSetField(client->http, HTTP_FIELD_CONTENT_ENCODING, compression);
      }
    }
  }

  // Is it a format we support?
  if ((attr = ippFindAttribute(client->request, "document-format", IPP_TAG_ZERO)) != NULL)
  {
    if (ippGetCount(attr) != 1 || ippGetValueTag(attr) != IPP_TAG_MIMETYPE || ippGetGroupTag(attr) != IPP_TAG_OPERATION)
    {
      papplClientRespondIPPUnsupported(client, attr);
      valid = false;
    }
    else
    {
      format = ippGetString(attr, 0, NULL);

      papplLogClient(client, PAPPL_LOGLEVEL_DEBUG, "%s \"document-format\"='%s'", op_name, format);

      ippAddString(client->request, IPP_TAG_JOB, IPP_TAG_MIMETYPE, "document-format-supplied", NULL, format);
    }
  }
  else
  {
    format = ippGetString(ippFindAttribute(client->printer->attrs, "document-format-default", IPP_TAG_MIMETYPE), 0, NULL);
    if (!format)
      format = "application/octet-stream"; /* Should never happen */

    attr = ippAddString(client->request, IPP_TAG_OPERATION, IPP_TAG_MIMETYPE, "document-format", NULL, format);
  }

  if (format && !strcmp(format, "application/octet-stream") && (ippGetOperation(client->request) == IPP_OP_PRINT_JOB || ippGetOperation(client->request) == IPP_OP_SEND_DOCUMENT))
  {
    // Auto-type the file using the first N bytes of the file...
    unsigned char	header[8192];	// First 8k bytes of file
    ssize_t		headersize;	// Number of bytes read


    memset(header, 0, sizeof(header));
    headersize = httpPeek(client->http, (char *)header, sizeof(header));

    if (!memcmp(header, "%PDF", 4))
      format = "application/pdf";
    else if (!memcmp(header, "%!", 2))
      format = "application/postscript";
    else if (!memcmp(header, "\377\330\377", 3) && header[3] >= 0xe0 && header[3] <= 0xef)
      format = "image/jpeg";
    else if (!memcmp(header, "\211PNG", 4))
      format = "image/png";
    else if (!memcmp(header, "RaS2PwgR", 8))
      format = "image/pwg-raster";
    else if (!memcmp(header, "UNIRAST", 8))
      format = "image/urf";
    else if (client->system->mime_cb)
      format = (client->system->mime_cb)(header, (size_t)headersize, client->system->mime_cbdata);
    else
      format = NULL;

    papplLogClient(client, PAPPL_LOGLEVEL_DEBUG, "Auto-type header: %02X%02X%02X%02X%02X%02X%02X%02X... format: %s\n", header[0], header[1], header[2], header[3], header[4], header[5], header[6], header[7], format ? format : "unknown");

    if (format)
    {
      papplLogClient(client, PAPPL_LOGLEVEL_DEBUG, "%s Auto-typed \"document-format\"='%s'.", op_name, format);

      ippAddString(client->request, IPP_TAG_JOB, IPP_TAG_MIMETYPE, "document-format-detected", NULL, format);
    }
  }

  pthread_rwlock_rdlock(&client->printer->rwlock);

  if (op != IPP_OP_CREATE_JOB && (supported = ippFindAttribute(client->printer->attrs, "document-format-supported", IPP_TAG_MIMETYPE)) != NULL && !ippContainsString(supported, format))
  {
    papplClientRespondIPPUnsupported(client, attr);
    valid = false;
  }

  pthread_rwlock_unlock(&client->printer->rwlock);

  return (valid);
}


//
// 'ipp_cancel_job()' - Cancel a job.
//

static void
ipp_cancel_job(pappl_client_t *client)	// I - Client
{
  pappl_job_t	*job;			// Job information


  // Authorize access...
  if (!_papplPrinterIsAuthorized(client))
    return;

  // Get the job...
  if (ippGetOperation(client->request) == IPP_OP_CANCEL_CURRENT_JOB)
    job = client->printer->processing_job;
  else
    job = client->job;

  if (!job)
  {
    papplClientRespondIPP(client, IPP_STATUS_ERROR_NOT_FOUND, "Job does not exist.");
    return;
  }

  // See if the job is already completed, canceled, or aborted; if so,
  // we can't cancel...
  switch (job->state)
  {
    case IPP_JSTATE_CANCELED :
	papplClientRespondIPP(client, IPP_STATUS_ERROR_NOT_POSSIBLE, "Job #%d is already canceled - can\'t cancel.", job->job_id);
        break;

    case IPP_JSTATE_ABORTED :
	papplClientRespondIPP(client, IPP_STATUS_ERROR_NOT_POSSIBLE, "Job #%d is already aborted - can\'t cancel.", job->job_id);
        break;

    case IPP_JSTATE_COMPLETED :
	papplClientRespondIPP(client, IPP_STATUS_ERROR_NOT_POSSIBLE, "Job #%d is already completed - can\'t cancel.", job->job_id);
        break;

    default :
        // Cancel the job...
        papplJobCancel(job);

	papplClientRespondIPP(client, IPP_STATUS_OK, NULL);
        break;
  }
}


//
// 'ipp_close_job()' - Close an open job.
//

static void
ipp_close_job(pappl_client_t *client)	// I - Client
{
  pappl_job_t	*job = client->job;	// Job information


  // Authorize access...
  if (!_papplPrinterIsAuthorized(client))
    return;

  // Get the job...
  if (!job)
  {
    papplClientRespondIPP(client, IPP_STATUS_ERROR_NOT_FOUND, "Job does not exist.");
    return;
  }

  // See if the job is already completed, canceled, or aborted; if so,
  // we can't cancel...
  switch (job->state)
  {
    case IPP_JSTATE_CANCELED :
	papplClientRespondIPP(client, IPP_STATUS_ERROR_NOT_POSSIBLE, "Job #%d is canceled - can\'t close.", job->job_id);
        break;

    case IPP_JSTATE_ABORTED :
	papplClientRespondIPP(client, IPP_STATUS_ERROR_NOT_POSSIBLE, "Job #%d is aborted - can\'t close.", job->job_id);
        break;

    case IPP_JSTATE_COMPLETED :
	papplClientRespondIPP(client, IPP_STATUS_ERROR_NOT_POSSIBLE, "Job #%d is completed - can\'t close.", job->job_id);
        break;

    case IPP_JSTATE_PROCESSING :
    case IPP_JSTATE_STOPPED :
	papplClientRespondIPP(client, IPP_STATUS_ERROR_NOT_POSSIBLE, "Job #%d is already closed.", job->job_id);
        break;

    default :
	papplClientRespondIPP(client, IPP_STATUS_OK, NULL);
        break;
  }
}


//
// 'ipp_get_job_attributes()' - Get the attributes for a job object.
//

static void
ipp_get_job_attributes(
    pappl_client_t *client)		// I - Client
{
  pappl_job_t	*job = client->job;	// Job information
  cups_array_t	*ra;			// requested-attributes


  // Authorize access...
  if (!_papplPrinterIsAuthorized(client))
    return;

  if (!job)
  {
    papplClientRespondIPP(client, IPP_STATUS_ERROR_NOT_FOUND, "Job not found.");
    return;
  }

  papplClientRespondIPP(client, IPP_STATUS_OK, NULL);

  ra = ippCreateRequestedArray(client->request);
  _papplJobCopyAttributes(job, client, ra);
  cupsArrayDelete(ra);
}


//
// 'ipp_hold_job()' - Hold a job.
//

static void
ipp_hold_job(pappl_client_t *client)	// I - Client
{
  pappl_job_t	*job = client->job;	// Job information


  // Authorize access...
  if (!_papplPrinterIsAuthorized(client))
    return;

  // Get the job...
  if (job)
  {
    const char	*hold_until;		// "job-hold-until" keyword
    time_t	hold_until_time;	// "job-hold-until-time" value

    hold_until      = ippGetString(ippFindAttribute(client->request, "job-hold-until", IPP_TAG_KEYWORD), 0, NULL);
    hold_until_time = ippDateToTime(ippGetDate(ippFindAttribute(client->request, "job-hold-until-time", IPP_TAG_DATE), 0));

    if (!hold_until && !hold_until_time)
      hold_until = "indefinite";

    // "job-hold-until" = 'no-hold' means release the job...
    if (hold_until && !strcmp(hold_until, "no-hold"))
    {
      if (papplJobRelease(job, client->username))
	papplClientRespondIPP(client, IPP_STATUS_OK, "Job released.");
      else
	papplClientRespondIPP(client, IPP_STATUS_ERROR_NOT_POSSIBLE, "Job already released.");
    }
    else
    {
      // Otherwise hold with the specified values...
      if (papplJobHold(job, client->username, hold_until, hold_until_time))
	papplClientRespondIPP(client, IPP_STATUS_OK, "Job held.");
      else
	papplClientRespondIPP(client, IPP_STATUS_ERROR_NOT_POSSIBLE, "Job not pending/held.");
    }
  }
  else
  {
    // Not found...
    papplClientRespondIPP(client, IPP_STATUS_ERROR_NOT_FOUND, "Job does not exist.");
  }
}


//
// 'ipp_release_job()' - Release a job.
//

static void
ipp_release_job(pappl_client_t *client)	// I - Client
{
  pappl_job_t	*job = client->job;	// Job information


  // Authorize access...
  if (!_papplPrinterIsAuthorized(client))
    return;

  // Get the job...
  if (job)
  {
    if (papplJobRelease(job, client->username))
      papplClientRespondIPP(client, IPP_STATUS_OK, "Job released.");
    else
      papplClientRespondIPP(client, IPP_STATUS_ERROR_NOT_POSSIBLE, "Job not held.");
  }
  else
  {
    // Not found...
    papplClientRespondIPP(client, IPP_STATUS_ERROR_NOT_FOUND, "Job does not exist.");
  }
}


//
// 'ipp_send_document()' - Add an attached document to a job object created with
//                         Create-Job.
//

static void
ipp_send_document(
    pappl_client_t *client)		// I - Client
{
  pappl_job_t	*job = client->job;	// Job information
  ipp_attribute_t *attr;		// Current attribute
  bool		have_data;		// Do we have document data?


  // Authorize access...
  if (!_papplPrinterIsAuthorized(client))
    return;

  // Get the job...
  if (!job)
  {
    papplClientRespondIPP(client, IPP_STATUS_ERROR_NOT_FOUND, "Job does not exist.");
    _papplClientFlushDocumentData(client);
    return;
  }

  // See if we already have a document for this job or the job has already
  // in a non-pending state...
  have_data = _papplClientHaveDocumentData(client);

  if (have_data)
  {
    if (job->filename || job->fd >= 0 || job->streaming)
    {
      papplClientRespondIPP(client, IPP_STATUS_ERROR_MULTIPLE_JOBS_NOT_SUPPORTED, "Multiple document jobs are not supported.");
      _papplClientFlushDocumentData(client);
      return;
    }
    else if (job->state > IPP_JSTATE_HELD)
    {
      papplClientRespondIPP(client, IPP_STATUS_ERROR_NOT_POSSIBLE, "Job is not in a pending state.");
      _papplClientFlushDocumentData(client);
      return;
    }
  }

  // Make sure we have the "last-document" operation attribute...
  if ((attr = ippFindAttribute(client->request, "last-document", IPP_TAG_ZERO)) == NULL)
  {
    papplClientRespondIPP(client, IPP_STATUS_ERROR_BAD_REQUEST, "Missing required \"last-document\" attribute.");
    _papplClientFlushDocumentData(client);
    return;
  }
  else if (ippGetGroupTag(attr) != IPP_TAG_OPERATION)
  {
    papplClientRespondIPP(client, IPP_STATUS_ERROR_BAD_REQUEST, "The \"last-document\" attribute is not in the operation group.");
    _papplClientFlushDocumentData(client);
    return;
  }
  else if (ippGetValueTag(attr) != IPP_TAG_BOOLEAN || ippGetCount(attr) != 1)
  {
    papplClientRespondIPPUnsupported(client, attr);
    _papplClientFlushDocumentData(client);
    return;
  }

  // Validate document attributes...
  if (have_data && !_papplJobValidateDocumentAttributes(client))
  {
    _papplClientFlushDocumentData(client);
    return;
  }

  if (!have_data && !job->filename)
    job->state = IPP_JSTATE_ABORTED;

  // Then finish getting the document data and process things...
  pthread_rwlock_wrlock(&(client->printer->rwlock));

  _papplCopyAttributes(job->attrs, client->request, NULL, IPP_TAG_JOB, 0);

  if ((attr = ippFindAttribute(job->attrs, "document-format-detected", IPP_TAG_MIMETYPE)) != NULL)
    job->format = ippGetString(attr, 0, NULL);
  else if ((attr = ippFindAttribute(job->attrs, "document-format-supplied", IPP_TAG_MIMETYPE)) != NULL)
    job->format = ippGetString(attr, 0, NULL);
  else
    job->format = client->printer->driver_data.format;

  pthread_rwlock_unlock(&(client->printer->rwlock));

  if (have_data)
    _papplJobCopyDocumentData(client, job);
}
