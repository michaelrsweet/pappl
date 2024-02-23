//
// Job IPP processing for the Printer Application Framework
//
// Copyright © 2019-2024 by Michael R Sweet.
// Copyright © 2010-2019 by Apple Inc.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#include "pappl-private.h"


//
// Local functions...
//

static void		copy_doc_attributes_no_lock(pappl_job_t *job, int doc_number, pappl_client_t *client, cups_array_t *ra);
static _pappl_doc_t	*find_document_no_lock(pappl_client_t *client);
static void		ipp_acknowledge_document(pappl_client_t *client);
static void		ipp_acknowledge_job(pappl_client_t *client);
static void		ipp_cancel_document(pappl_client_t *client);
static void		ipp_cancel_job(pappl_client_t *client);
static void		ipp_close_job(pappl_client_t *client);
static void		ipp_fetch_document(pappl_client_t *client);
static void		ipp_fetch_job(pappl_client_t *client);
static void		ipp_get_document_attributes(pappl_client_t *client);
static void		ipp_get_documents(pappl_client_t *client);
static void		ipp_get_job_attributes(pappl_client_t *client);
static void		ipp_hold_job(pappl_client_t *client);
static void		ipp_release_job(pappl_client_t *client);
static void		ipp_send_document(pappl_client_t *client);
static void		ipp_update_document_status(pappl_client_t *client);
static void		ipp_update_job_status(pappl_client_t *client);



//
// '_papplJobCopyAttributesNoLock()' - Copy job attributes to the response.
//

void
_papplJobCopyAttributesNoLock(
    pappl_job_t    *job,		// I - Job
    pappl_client_t *client,		// I - Client
    cups_array_t   *ra,			// I - requested-attributes
    bool           include_status)	// I - Include Job Status attributes?
{
  _papplCopyAttributes(client->response, job->attrs, ra, IPP_TAG_JOB, false);

  if (include_status)
  {
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
  }

  if (!ra || cupsArrayFind(ra, "job-impressions"))
    ippAddInteger(client->response, IPP_TAG_JOB, IPP_TAG_INTEGER, "job-impressions", job->impressions);

  if (!ra || cupsArrayFind(ra, "job-impressions-col"))
  {
    ipp_t	*col;			// Collection value

    col = ippNew();
    ippAddInteger(col, IPP_TAG_JOB, IPP_TAG_INTEGER, "monochrome", job->impressions - job->impcolor);
    ippAddInteger(col, IPP_TAG_JOB, IPP_TAG_INTEGER, "full-color", job->impcolor);

    ippAddCollection(client->response, IPP_TAG_JOB, "job-impressions-col", col);
    ippDelete(col);
  }

  if (include_status && (!ra || cupsArrayFind(ra, "job-impressions-completed")))
    ippAddInteger(client->response, IPP_TAG_JOB, IPP_TAG_INTEGER, "job-impressions-completed", job->impcompleted);

  if (!ra || cupsArrayFind(ra, "job-k-octets"))
  {
    off_t k_octets = (job->k_octets + 1023) / 1024;
					// Scale the value down

    ippAddInteger(client->response, IPP_TAG_JOB, IPP_TAG_INTEGER, "job-k-octets", k_octets > INT_MAX ? INT_MAX : (int)k_octets);
  }

  if (include_status)
  {
    if (!ra || cupsArrayFind(ra, "job-printer-up-time"))
      ippAddInteger(client->response, IPP_TAG_JOB, IPP_TAG_INTEGER, "job-printer-up-time", (int)(time(NULL) - client->printer->start_time));

    _papplJobCopyStateNoLock(job, IPP_TAG_JOB, client->response, ra);

    if ((!ra || cupsArrayFind(ra, "output-device-uuid-assigned")) && job->output_device)
      ippAddString(client->response, IPP_TAG_JOB, IPP_TAG_URI, "output-device-uuid-assigned", /*language*/NULL, job->output_device->device_uuid);

    if (!ra || cupsArrayFind(ra, "time-at-creation"))
      ippAddInteger(client->response, IPP_TAG_JOB, IPP_TAG_INTEGER, "time-at-creation", (int)(job->created - client->printer->start_time));

    if (!ra || cupsArrayFind(ra, "time-at-completed"))
      ippAddInteger(client->response, IPP_TAG_JOB, job->completed ? IPP_TAG_INTEGER : IPP_TAG_NOVALUE, "time-at-completed", (int)(job->completed - client->printer->start_time));

    if (!ra || cupsArrayFind(ra, "time-at-processing"))
      ippAddInteger(client->response, IPP_TAG_JOB, job->processing ? IPP_TAG_INTEGER : IPP_TAG_NOVALUE, "time-at-processing", (int)(job->processing - client->printer->start_time));
  }
}


//
// '_papplJobCopyDocumentData()' - Finish receiving a document file in an IPP
//                                 request and start processing.
//

void
_papplJobCopyDocumentData(
    pappl_client_t *client,		// I - Client
    pappl_job_t    *job,		// I - Job
    const char     *format,		// I - Document format
    bool           last_document)	// I - Last document?
{
  char			filename[1024],	// Filename buffer
			buffer[4096];	// Copy buffer
  ssize_t		bytes,		// Bytes read
			total = 0;	// Total bytes copied
  cups_array_t		*ra;		// Attributes to send in response


  // If we have a PWG or Apple raster file and this is not an Infrastructure
  // Printer, process it directly or return server-error-busy...
  if (!job->printer->output_devices && (!strcmp(format, "image/pwg-raster") || !strcmp(format, "image/urf")))
  {
    _papplRWLockRead(job->printer);

    // TODO: Spool raster when multiple document jobs are enabled
    if (job->printer->processing_job)
    {
      papplClientRespondIPP(client, IPP_STATUS_ERROR_BUSY, "Currently printing another job.");
      _papplRWUnlock(job->printer);
      goto abort_job;
    }
    else if (job->printer->hold_new_jobs)
    {
      papplClientRespondIPP(client, IPP_STATUS_ERROR_NOT_ACCEPTING_JOBS, "Currently holding new jobs.");
      _papplRWUnlock(job->printer);
      goto abort_job;
    }

    _papplRWLockWrite(job);

    job->state = IPP_JSTATE_PENDING;

    _papplRWUnlock(job);
    _papplRWUnlock(job->printer);

    _papplJobProcessRaster(job, client);

    goto complete_job;
  }

  // Create a file for the request data...
  if ((job->fd = papplJobOpenFile(job, job->num_documents + 1, filename, sizeof(filename), client->system->directory, NULL, format, "w")) < 0)
  {
    papplClientRespondIPP(client, IPP_STATUS_ERROR_INTERNAL, "Unable to create print file: %s", strerror(errno));
    papplLogClient(client, PAPPL_LOGLEVEL_DEBUG, "Print filename is '%s'.", filename);
    goto abort_job;
  }

  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "Created job file \"%s\", format \"%s\".", filename, format);

  while ((bytes = httpRead(client->http, buffer, sizeof(buffer))) > 0)
  {
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
  _papplJobSubmitFile(job, filename, format, client->request, last_document);

  complete_job:

  // Return the job info...
  papplClientRespondIPP(client, IPP_STATUS_OK, NULL);

  ra = cupsArrayNew((cups_array_cb_t)strcmp, NULL, NULL, 0, NULL, NULL);
  cupsArrayAdd(ra, "job-id");
  cupsArrayAdd(ra, "job-state");
  cupsArrayAdd(ra, "job-state-message");
  cupsArrayAdd(ra, "job-state-reasons");
  cupsArrayAdd(ra, "job-uri");

  _papplRWLockRead(job);
  _papplJobCopyAttributesNoLock(job, client, ra, /*include_status*/true);
  _papplRWUnlock(job);

  cupsArrayDelete(ra);
  return;

  // If we get here we had to abort the job...
  abort_job:

  papplLogJob(job, PAPPL_LOGLEVEL_INFO, "Aborting job.");

  _papplClientFlushDocumentData(client);

  _papplRWLockWrite(client->printer);
  _papplRWLockWrite(job);

  job->state     = IPP_JSTATE_ABORTED;
  job->completed = time(NULL);

  cupsArrayRemove(client->printer->active_jobs, job);
  cupsArrayAdd(client->printer->completed_jobs, job);

  _papplRWLockWrite(client->system);
  if (!client->system->clean_time)
    client->system->clean_time = time(NULL) + 60;
  _papplRWUnlock(client->system);

  _papplRWUnlock(job);
  _papplRWUnlock(client->printer);

  ra = cupsArrayNew((cups_array_cb_t)strcmp, NULL, NULL, 0, NULL, NULL);
  cupsArrayAdd(ra, "job-id");
  cupsArrayAdd(ra, "job-state");
  cupsArrayAdd(ra, "job-state-reasons");
  cupsArrayAdd(ra, "job-uri");

  _papplJobCopyAttributesNoLock(job, client, ra, /*include_status*/true);
  cupsArrayDelete(ra);
}


//
// '_papplJobCopyStateNoLock()' - Copy the job-state-xxx attributes.
//

void
_papplJobCopyStateNoLock(
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

      for (bit = PAPPL_JREASON_ABORTED_BY_SYSTEM; bit <= PAPPL_JREASON_JOB_RELEASE_WAIT; bit *= 2)
      {
        if (bit & job->state_reasons)
          svalues[num_values ++] = _papplJobReasonString(bit);
      }

      ippAddStrings(ipp, group_tag, IPP_CONST_TAG(IPP_TAG_KEYWORD), "job-state-reasons", num_values, NULL, svalues);
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

    case IPP_OP_CANCEL_DOCUMENT :
	ipp_cancel_document(client);
	break;

    case IPP_OP_GET_DOCUMENT_ATTRIBUTES :
        ipp_get_document_attributes(client);
        break;

    case IPP_OP_GET_DOCUMENTS :
        ipp_get_documents(client);
        break;

    case IPP_OP_ACKNOWLEDGE_DOCUMENT :
        if (client->printer->output_devices)
	  ipp_acknowledge_document(client);
	else
	  papplClientRespondIPP(client, IPP_STATUS_ERROR_OPERATION_NOT_SUPPORTED, "Operation not supported.");
        break;

    case IPP_OP_ACKNOWLEDGE_JOB :
        if (client->printer->output_devices)
	  ipp_acknowledge_job(client);
	else
	  papplClientRespondIPP(client, IPP_STATUS_ERROR_OPERATION_NOT_SUPPORTED, "Operation not supported.");
        break;

    case IPP_OP_FETCH_DOCUMENT :
        if (client->printer->output_devices)
	  ipp_fetch_document(client);
	else
	  papplClientRespondIPP(client, IPP_STATUS_ERROR_OPERATION_NOT_SUPPORTED, "Operation not supported.");
	  ipp_fetch_document(client);
        break;

    case IPP_OP_FETCH_JOB :
        if (client->printer->output_devices)
	  ipp_fetch_job(client);
	else
	  papplClientRespondIPP(client, IPP_STATUS_ERROR_OPERATION_NOT_SUPPORTED, "Operation not supported.");
        break;

    case IPP_OP_UPDATE_DOCUMENT_STATUS :
        if (client->printer->output_devices)
	  ipp_update_document_status(client);
	else
	  papplClientRespondIPP(client, IPP_STATUS_ERROR_OPERATION_NOT_SUPPORTED, "Operation not supported.");
        break;

    case IPP_OP_UPDATE_JOB_STATUS :
        if (client->printer->output_devices)
	  ipp_update_job_status(client);
	else
	  papplClientRespondIPP(client, IPP_STATUS_ERROR_OPERATION_NOT_SUPPORTED, "Operation not supported.");
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
    pappl_client_t *client,		// I - Client
    const char     **format)		// O - Document format
{
  bool			valid = true;	// Valid attributes?
  ipp_op_t		op = ippGetOperation(client->request);
					// IPP operation
  const char		*op_name = ippOpString(op);
					// IPP operation name
  ipp_attribute_t	*attr,		// Current attribute
			*supported;	// xxx-supported attribute
  const char		*compression = NULL;
					// compression value


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
    else if (format)
    {
      *format = ippGetString(attr, 0, NULL);

      papplLogClient(client, PAPPL_LOGLEVEL_DEBUG, "%s \"document-format\"='%s'", op_name, *format);
    }
  }
  else if (format)
  {
    *format = ippGetString(ippFindAttribute(client->printer->attrs, "document-format-default", IPP_TAG_MIMETYPE), 0, NULL);
    if (!*format)
      *format = "application/octet-stream"; /* Should never happen */
  }

  if (format && *format && !strcmp(*format, "application/octet-stream") && (ippGetOperation(client->request) == IPP_OP_PRINT_JOB || ippGetOperation(client->request) == IPP_OP_SEND_DOCUMENT))
  {
    // Auto-type the file using the first N bytes of the file...
    unsigned char	header[8192];	// First 8k bytes of file
    ssize_t		headersize;	// Number of bytes read

    memset(header, 0, sizeof(header));
    headersize = httpPeek(client->http, (char *)header, sizeof(header));

    _papplRWLockRead(client->system);

    if (!memcmp(header, "%PDF", 4))
      *format = "application/pdf";
    else if (!memcmp(header, "%!", 2))
      *format = "application/postscript";
    else if (!memcmp(header, "\377\330\377", 3) && header[3] >= 0xe0 && header[3] <= 0xef)
      *format = "image/jpeg";
    else if (!memcmp(header, "\211PNG", 4))
      *format = "image/png";
    else if (!memcmp(header, "RaS2PwgR", 8))
      *format = "image/pwg-raster";
    else if (!memcmp(header, "UNIRAST", 8))
      *format = "image/urf";
    else if (client->system->mime_cb)
      *format = (client->system->mime_cb)(header, (size_t)headersize, client->system->mime_cbdata);
    else
      *format = NULL;

    _papplRWUnlock(client->system);

    papplLogClient(client, PAPPL_LOGLEVEL_DEBUG, "Auto-type header: %02X%02X%02X%02X%02X%02X%02X%02X... format: %s\n", header[0], header[1], header[2], header[3], header[4], header[5], header[6], header[7], *format ? *format : "unknown");

    if (*format)
    {
      papplLogClient(client, PAPPL_LOGLEVEL_DEBUG, "%s Auto-typed \"document-format\"='%s'.", op_name, *format);
    }
  }

  _papplRWLockRead(client->printer);

  if (op != IPP_OP_CREATE_JOB && format && (supported = ippFindAttribute(client->printer->attrs, "document-format-supported", IPP_TAG_MIMETYPE)) != NULL && !ippContainsString(supported, *format))
  {
    papplClientRespondIPPUnsupported(client, attr);
    valid = false;
  }

  _papplRWUnlock(client->printer);

  return (valid);
}


//
// 'copy_doc_attributes_no_lock()' - Copy the document attributes to the response.
//

static void
copy_doc_attributes_no_lock(
    pappl_job_t    *job,		// I - Job
    int            doc_number,		// I - Document number (`1` based)
    pappl_client_t *client,		// I - Client
    cups_array_t   *ra)			// I - "requested-attributes"
{
  _pappl_doc_t   *doc = job->documents + doc_number - 1;
					// Document


  _papplCopyAttributes(client->response, doc->attrs, ra, IPP_TAG_DOCUMENT, false);

  if (!ra || cupsArrayFind(ra, "date-time-at-creation"))
    ippAddDate(client->response, IPP_TAG_DOCUMENT, "date-time-at-creation", ippTimeToDate(doc->created));

  if (!ra || cupsArrayFind(ra, "date-time-at-completed"))
  {
    if (doc->completed)
      ippAddDate(client->response, IPP_TAG_DOCUMENT, "date-time-at-completed", ippTimeToDate(doc->completed));
    else
      ippAddOutOfBand(client->response, IPP_TAG_DOCUMENT, IPP_TAG_NOVALUE, "date-time-at-completed");
  }

  if (!ra || cupsArrayFind(ra, "date-time-at-processing"))
  {
    if (doc->processing)
      ippAddDate(client->response, IPP_TAG_DOCUMENT, "date-time-at-processing", ippTimeToDate(doc->processing));
    else
      ippAddOutOfBand(client->response, IPP_TAG_DOCUMENT, IPP_TAG_NOVALUE, "date-time-at-processing");
  }

  if (!ra || cupsArrayFind(ra, "document-job-id"))
    ippAddInteger(client->response, IPP_TAG_DOCUMENT, IPP_TAG_INTEGER, "document-job-id", job->job_id);

  if (!ra || cupsArrayFind(ra, "document-job-uri"))
    ippAddString(client->response, IPP_TAG_DOCUMENT, IPP_TAG_URI, "document-job-uri", NULL, job->uri);

  if (!ra || cupsArrayFind(ra, "document-printer-uri"))
    ippAddString(client->response, IPP_TAG_DOCUMENT, IPP_TAG_URI, "document-printer-uri", NULL, job->printer_uri);

  if (!ra || cupsArrayFind(ra, "document-number"))
    ippAddInteger(client->response, IPP_TAG_DOCUMENT, IPP_TAG_INTEGER, "document-number", doc_number);

  if (!ra || cupsArrayFind(ra, "document-state"))
    ippAddInteger(client->response, IPP_TAG_DOCUMENT, IPP_TAG_ENUM, "document-state", (int)doc->state);

  if (!ra || cupsArrayFind(ra, "document-state-reasons"))
  {
    if (doc->state_reasons)
    {
      size_t		num_values = 0;	// Number of string values
      const char	*svalues[32];	// String values
      pappl_jreason_t	bit;		// Current reason bit

      for (bit = PAPPL_JREASON_ABORTED_BY_SYSTEM; bit <= PAPPL_JREASON_JOB_RELEASE_WAIT; bit *= 2)
      {
        if (bit & doc->state_reasons)
        {
          if (bit == PAPPL_JREASON_JOB_FETCHABLE)
	    svalues[num_values ++] = "document-fetchable";
	  else
	    svalues[num_values ++] = _papplJobReasonString(bit);
        }
      }

      ippAddStrings(client->response, IPP_TAG_DOCUMENT, IPP_CONST_TAG(IPP_TAG_KEYWORD), "document-state-reasons", num_values, NULL, svalues);
    }
    else
    {
      ippAddString(client->response, IPP_TAG_DOCUMENT, IPP_CONST_TAG(IPP_TAG_KEYWORD), "document-state-reasons", NULL, "none");
    }
  }

  if (!ra || cupsArrayFind(ra, "impressions"))
    ippAddInteger(client->response, IPP_TAG_DOCUMENT, IPP_TAG_INTEGER, "impressions", doc->impressions);

  if (!ra || cupsArrayFind(ra, "impressions-col"))
  {
    ipp_t	*col;			// Collection value

    col = ippNew();
    ippAddInteger(col, IPP_TAG_DOCUMENT, IPP_TAG_INTEGER, "monochrome", doc->impressions - doc->impcolor);
    ippAddInteger(col, IPP_TAG_DOCUMENT, IPP_TAG_INTEGER, "full-color", doc->impcolor);

    ippAddCollection(client->response, IPP_TAG_DOCUMENT, "impressions-col", col);
    ippDelete(col);
  }

  if (!ra || cupsArrayFind(ra, "impressions-completed"))
    ippAddInteger(client->response, IPP_TAG_DOCUMENT, IPP_TAG_INTEGER, "impressions-completed", doc->impcompleted);

  if (!ra || cupsArrayFind(ra, "k-octets"))
  {
    off_t k_octets = (doc->k_octets + 1023) / 1024;
					// Scale the value down

    ippAddInteger(client->response, IPP_TAG_DOCUMENT, IPP_TAG_INTEGER, "k-octets", k_octets > INT_MAX ? INT_MAX : (int)k_octets);
  }

  if (!ra || cupsArrayFind(ra, "last-document"))
    ippAddBoolean(client->response, IPP_TAG_DOCUMENT, "last-document", doc_number == job->num_documents);

  if (!ra || cupsArrayFind(ra, "printer-up-time"))
    ippAddInteger(client->response, IPP_TAG_DOCUMENT, IPP_TAG_INTEGER, "printer-up-time", (int)(time(NULL) - client->printer->start_time));

  if (!ra || cupsArrayFind(ra, "time-at-creation"))
    ippAddInteger(client->response, IPP_TAG_DOCUMENT, IPP_TAG_INTEGER, "time-at-creation", (int)(doc->created - client->printer->start_time));

  if (!ra || cupsArrayFind(ra, "time-at-completed"))
    ippAddInteger(client->response, IPP_TAG_DOCUMENT, doc->completed ? IPP_TAG_INTEGER : IPP_TAG_NOVALUE, "time-at-completed", (int)(doc->completed - client->printer->start_time));

  if (!ra || cupsArrayFind(ra, "time-at-processing"))
    ippAddInteger(client->response, IPP_TAG_DOCUMENT, doc->processing ? IPP_TAG_INTEGER : IPP_TAG_NOVALUE, "time-at-processing", (int)(doc->processing - client->printer->start_time));
}


//
// 'find_document_no_lock()' - Get the document for a request.
//

static _pappl_doc_t *			// O - Document or `NULL` on error
find_document_no_lock(
    pappl_client_t *client)		// I - Client
{
  ipp_attribute_t *attr;		// "document-number" attribute
  int		doc_number;		// Document number


  if ((attr = ippFindAttribute(client->request, "document-number", IPP_TAG_ZERO)) == NULL)
  {
    papplClientRespondIPP(client, IPP_STATUS_ERROR_BAD_REQUEST, "Missing required \"document-number\" attribute.");
    return (NULL);
  }
  else if (ippGetGroupTag(attr) != IPP_TAG_OPERATION || ippGetValueTag(attr) != IPP_TAG_INTEGER || ippGetCount(attr) != 1)
  {
    papplClientRespondIPPUnsupported(client, attr);
    return (NULL);
  }
  else if ((doc_number = ippGetInteger(attr, 0)) < 1 || doc_number > client->job->num_documents)
  {
    papplClientRespondIPPUnsupported(client, attr);
    return (NULL);
  }

  return (client->job->documents + doc_number - 1);
}


//
// 'ipp_acknowledge_document()' - Acknowledge a document.
//

static void
ipp_acknowledge_document(
    pappl_client_t *client)		// I - Client
{
  pappl_printer_t	*printer = client->printer;
					// Printer
  _pappl_odevice_t	*od;		// Output device


  // Authorize access...
  if (!_papplPrinterIsAuthorized(client))
    return;

  // Find the output device
  _papplRWLockRead(printer);
  cupsRWLockWrite(&printer->output_rwlock);

  if ((od = _papplClientFindDeviceNoLock(client)) != NULL)
  {
    pappl_job_t	*job = client->job;	// Job
    _pappl_doc_t *doc;			// Document

    _papplRWLockRead(job);

    // TODO: Handle fetch-status-code
    if (!job->output_device)
    {
      papplClientRespondIPP(client, IPP_STATUS_ERROR_NOT_POSSIBLE, "Job not accepted.");
    }
    else if (job->output_device != od)
    {
      papplClientRespondIPP(client, IPP_STATUS_ERROR_NOT_POSSIBLE, "Job accepted by another device.");
    }
    else if ((doc = find_document_no_lock(client)) != NULL)
    {
      if (doc->state >= IPP_DSTATE_CANCELED)
      {
	// Document is in a terminating state...
	papplClientRespondIPP(client, IPP_STATUS_ERROR_NOT_POSSIBLE, "Document already terminated.");
      }
      else if (doc->state != IPP_DSTATE_PENDING)
      {
	// Document is in a terminating state...
	papplClientRespondIPP(client, IPP_STATUS_ERROR_NOT_POSSIBLE, "Document already accepted.");
      }
      else
      {
        // Move document to processing state...
        doc->state = IPP_DSTATE_PROCESSING;

        _papplSystemAddEventNoLock(client->system, printer, job, PAPPL_EVENT_DOCUMENT_STATE_CHANGED, "Document accepted for printing.");
        papplClientRespondIPP(client, IPP_STATUS_OK, /*message*/NULL);
      }
    }

    _papplRWUnlock(job);
  }

  cupsRWUnlock(&printer->output_rwlock);
  _papplRWUnlock(printer);
}


//
// 'ipp_acknowledge_job()' - Acknowledge a job.
//

static void
ipp_acknowledge_job(
    pappl_client_t *client)		// I - Client
{
  pappl_printer_t	*printer = client->printer;
					// Printer
  _pappl_odevice_t	*od;		// Output device


  // Authorize access...
  if (!_papplPrinterIsAuthorized(client))
    return;

  // Find the output device
  _papplRWLockRead(printer);
  cupsRWLockWrite(&printer->output_rwlock);

  if ((od = _papplClientFindDeviceNoLock(client)) != NULL)
  {
    pappl_job_t	*job = client->job;	// Job

    _papplRWLockWrite(job);

    // TODO: Handle fetch-status-code
    if (job->output_device && job->output_device != od)
    {
      // Already assigned to another device...
      papplClientRespondIPP(client, IPP_STATUS_ERROR_NOT_POSSIBLE, "Job already assigned to another device.");
    }
    else if (job->state >= IPP_JSTATE_CANCELED)
    {
      // Job is in a terminating state...
      papplClientRespondIPP(client, IPP_STATUS_ERROR_NOT_POSSIBLE, "Job already terminated.");
    }
    else
    {
      // Assign job...
      job->output_device = od;

      job->state_reasons &= (pappl_jreason_t)~PAPPL_JREASON_JOB_FETCHABLE;

      if (job->state == IPP_JSTATE_HELD)
        _papplJobReleaseNoLock(job, papplClientGetIPPUsername(client));
      else
        _papplSystemAddEventNoLock(job->system, printer, job, PAPPL_EVENT_JOB_STATE_CHANGED, "Job assigned to output device.");

      papplClientRespondIPP(client, IPP_STATUS_OK, /*message*/NULL);
    }

    _papplRWUnlock(job);
  }

  cupsRWUnlock(&printer->output_rwlock);
  _papplRWUnlock(printer);
}


//
// 'ipp_cancel_document()' - Cancel a document.
//

static void
ipp_cancel_document(
    pappl_client_t *client)		// I - Client
{
  pappl_job_t	*job;			// Job information
  ipp_attribute_t *attr;		// "document-number" attribute
  int		doc_number;		// "document-number" value
  _pappl_doc_t	*doc;			// Document


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

  if ((attr = ippFindAttribute(client->request, "document-number", IPP_TAG_ZERO)) == NULL)
  {
    papplClientRespondIPP(client, IPP_STATUS_ERROR_BAD_REQUEST, "Missing required \"document-number\" operation attribute.");
    return;
  }
  else if (ippGetGroupTag(attr) != IPP_TAG_OPERATION || ippGetValueTag(attr) != IPP_TAG_INTEGER || ippGetCount(attr) != 1)
  {
    papplClientRespondIPP(client, IPP_STATUS_ERROR_BAD_REQUEST, "Bad \"document-number\" attribute in request.");
    return;
  }

  _papplRWLockWrite(job);

  if ((doc_number = ippGetInteger(attr, 0)) < 1 || doc_number > job->num_documents)
  {
    _papplRWUnlock(job);
    papplClientRespondIPP(client, IPP_STATUS_ERROR_NOT_FOUND, "Document number %d not found.", doc_number);
    return;
  }

  doc = job->documents + doc_number - 1;

  // See if the document is already completed, canceled, or aborted; if so,
  // we can't cancel...
  switch (doc->state)
  {
    case IPP_DSTATE_CANCELED :
	papplClientRespondIPP(client, IPP_STATUS_ERROR_NOT_POSSIBLE, "Document #%d is already canceled - can\'t cancel.", doc_number);
        break;

    case IPP_JSTATE_ABORTED :
	papplClientRespondIPP(client, IPP_STATUS_ERROR_NOT_POSSIBLE, "Document #%d is already aborted - can\'t cancel.", doc_number);
        break;

    case IPP_JSTATE_COMPLETED :
	papplClientRespondIPP(client, IPP_STATUS_ERROR_NOT_POSSIBLE, "Document #%d is already completed - can\'t cancel.", doc_number);
        break;

    default :
        // Cancel the job...
        doc->state = IPP_DSTATE_CANCELED;

	papplClientRespondIPP(client, IPP_STATUS_OK, NULL);
        break;
  }

  _papplRWUnlock(job);
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
// 'ipp_fetch_document()' - Fetch a document.
//

static void
ipp_fetch_document(
    pappl_client_t *client)		// I - Client
{
  pappl_printer_t	*printer = client->printer;
					// Printer
  _pappl_odevice_t	*od;		// Output device


  // Authorize access...
  if (!_papplPrinterIsAuthorized(client))
    return;

  // Find the output device
  _papplRWLockRead(printer);
  cupsRWLockWrite(&printer->output_rwlock);

  if ((od = _papplClientFindDeviceNoLock(client)) != NULL)
  {
    pappl_job_t		*job = client->job;
					// Job
    _pappl_doc_t	*doc;		// Document
    ipp_attribute_t	*compressions,	// "compression-accepted" attribute
			*formats;	// "document-format-accepted" or "document-format-supported" attribute
    const char		*compression = "none";
					// "compression" value to use
    //const char		*format;	// "document-format" value to use

    if ((compressions = ippFindAttribute(client->request, "compression-accepted", IPP_TAG_ZERO)) != NULL)
    {
      if (ippGetGroupTag(compressions) != IPP_TAG_OPERATION || ippGetValueTag(compressions) != IPP_TAG_KEYWORD || (!ippContainsString(compressions, "none") && !ippContainsString(compressions, "gzip")))
      {
        papplClientRespondIPPUnsupported(client, compressions);
        goto done;
      }
      else if (ippContainsString(compressions, "gzip"))
      {
        compression = "gzip";
      }
    }

    if ((formats = ippFindAttribute(client->request, "document-format-accepted", IPP_TAG_ZERO)) != NULL)
    {
      if (ippGetGroupTag(formats) != IPP_TAG_OPERATION || ippGetValueTag(formats) != IPP_TAG_MIMETYPE)
      {
        papplClientRespondIPPUnsupported(client, formats);
        goto done;
      }
    }
    else
    {
      // Use document-format-supported from the output device...
      if ((formats = ippFindAttribute(od->device_attrs, "document-format-supported", IPP_TAG_MIMETYPE)) == NULL)
      {
        papplClientRespondIPP(client, IPP_STATUS_ERROR_NOT_POSSIBLE, "No \"document-format-supported\" attribute for device.");
        goto done;
      }
    }

    _papplRWLockRead(job);

    if (!job->output_device)
    {
      papplClientRespondIPP(client, IPP_STATUS_ERROR_NOT_POSSIBLE, "Job not accepted.");
    }
    else if (job->output_device != od)
    {
      papplClientRespondIPP(client, IPP_STATUS_ERROR_NOT_POSSIBLE, "Job accepted by another device.");
    }
    else if ((doc = find_document_no_lock(client)) != NULL)
    {
      if (doc->state >= IPP_DSTATE_CANCELED)
      {
	// Document is in a terminating state...
	papplClientRespondIPP(client, IPP_STATUS_ERROR_NOT_POSSIBLE, "Document already terminated.");
      }
      else if (!ippContainsString(formats, doc->format))
      {
        // TODO: Support filtering for Fetch-Document
        papplClientRespondIPPUnsupported(client, formats);
      }
      else
      {
        // Send document to client...
        papplClientRespondIPP(client, IPP_STATUS_OK, /*message*/NULL);

	// Flush trailing (junk) data
	if (httpGetState(client->http) == HTTP_STATE_POST_RECV)
	  _papplClientFlushDocumentData(client);

	// Send the HTTP header and document data...
	if (papplClientRespond(client, HTTP_STATUS_OK, compression, "application/ipp", /*last_modified*/0, /*length*/0))
	{
	  // Open the document file and copy it to the client...
	  int		fd;		// File descriptor
	  char		buffer[16384];	// Buffer
	  ssize_t	bytes;		// Bytes read

	  if ((fd = open(doc->filename, O_RDONLY | O_BINARY)) >= 0)
	  {
	    while ((bytes = read(fd, buffer, sizeof(buffer))) > 0)
	      httpWrite(client->http, buffer, (size_t)bytes);
	  }

	  // Send a 0-length chunk...
	  httpWrite(client->http, "", 0);
	}
      }
    }

    _papplRWUnlock(job);
  }

  done:

  cupsRWUnlock(&printer->output_rwlock);
  _papplRWUnlock(printer);
}


//
// 'ipp_fetch_job()' - Fetch a job.
//

static void
ipp_fetch_job(
    pappl_client_t *client)		// I - Client
{
  pappl_printer_t	*printer = client->printer;
					// Printer
  _pappl_odevice_t	*od;		// Output device


  // Authorize access...
  if (!_papplPrinterIsAuthorized(client))
    return;

  // Find the output device
  _papplRWLockRead(printer);
  cupsRWLockWrite(&printer->output_rwlock);

  if ((od = _papplClientFindDeviceNoLock(client)) != NULL)
  {
    pappl_job_t	*job = client->job;	// Job

    _papplRWLockWrite(job);

    if (job->output_device && job->output_device != od)
    {
      // Already assigned to another device...
      papplClientRespondIPP(client, IPP_STATUS_ERROR_NOT_POSSIBLE, "Job already assigned to another device.");
    }
    else if (!(job->state_reasons & PAPPL_JREASON_JOB_FETCHABLE) || job->is_canceled || job->state >= IPP_JSTATE_CANCELED)
    {
      // Job is not fetchable...
      papplClientRespondIPP(client, IPP_STATUS_ERROR_NOT_POSSIBLE, "Job is not fetchable.");
    }
    else
    {
      // Copy Job attributes...
      papplClientRespondIPP(client, IPP_STATUS_OK, /*message*/NULL);
      _papplJobCopyAttributesNoLock(job, client, /*ra*/NULL, /*include_status*/false);
    }

    _papplRWUnlock(job);
  }

  cupsRWUnlock(&printer->output_rwlock);
  _papplRWUnlock(printer);
}


//
// 'ipp_get_document_attributes()' - Get attributes for a document object.
//

static void
ipp_get_document_attributes(
    pappl_client_t *client)		// I - Client
{
  pappl_job_t	*job = client->job;	// Job information
  ipp_attribute_t *attr;		// "document-number" attribute
  int		doc_number;		// "document-number" value
  cups_array_t	*ra;			// "requested-attributes" values


  // Authorize access...
  if (!_papplPrinterIsAuthorized(client))
    return;

  if (!job)
  {
    papplClientRespondIPP(client, IPP_STATUS_ERROR_NOT_FOUND, "Job not found.");
    return;
  }

  if ((attr = ippFindAttribute(client->request, "document-number", IPP_TAG_ZERO)) == NULL)
  {
    papplClientRespondIPP(client, IPP_STATUS_ERROR_BAD_REQUEST, "Missing required \"document-number\" operation attribute.");
    return;
  }
  else if (ippGetGroupTag(attr) != IPP_TAG_OPERATION || ippGetValueTag(attr) != IPP_TAG_INTEGER || ippGetCount(attr) != 1)
  {
    papplClientRespondIPP(client, IPP_STATUS_ERROR_BAD_REQUEST, "Bad \"document-number\" attribute in request.");
    return;
  }

  _papplRWLockRead(job);

  if ((doc_number = ippGetInteger(attr, 0)) < 1 || doc_number > job->num_documents)
  {
    _papplRWUnlock(job);
    papplClientRespondIPP(client, IPP_STATUS_ERROR_NOT_FOUND, "Document number %d not found.", doc_number);
    return;
  }

  papplClientRespondIPP(client, IPP_STATUS_OK, NULL);

  ra = ippCreateRequestedArray(client->request);
  copy_doc_attributes_no_lock(job, doc_number, client, ra);
  cupsArrayDelete(ra);

  _papplRWUnlock(job);
}


//
// 'ipp_get_documents()' - Get a list of documents in a job object.
//

static void
ipp_get_documents(
    pappl_client_t *client)		// I - Client
{
  pappl_job_t	*job = client->job;	// Job information
  int		doc_number,		// Current document number
		limit;			// "limit" value
  cups_array_t	*ra;			// "requested-attributes" values


  // Authorize access...
  if (!_papplPrinterIsAuthorized(client))
    return;

  if (!job)
  {
    papplClientRespondIPP(client, IPP_STATUS_ERROR_NOT_FOUND, "Job not found.");
    return;
  }

  _papplRWLockRead(job);

  if ((limit = ippGetInteger(ippFindAttribute(client->request, "limit", IPP_TAG_INTEGER), 0)) <= 0)
    limit = job->num_documents;

  papplClientRespondIPP(client, IPP_STATUS_OK, NULL);

  ra = ippCreateRequestedArray(client->request);

  for (doc_number = 1; doc_number <= job->num_documents && doc_number <= limit; doc_number ++)
  {
    if (doc_number > 1)
      ippAddSeparator(client->response);

    copy_doc_attributes_no_lock(job, doc_number, client, ra);
  }

  cupsArrayDelete(ra);

  _papplRWUnlock(job);
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
  _papplJobCopyAttributesNoLock(job, client, ra, /*include_status*/true);
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
  bool		last_document,		// "last-document" value
		have_data;		// Do we have document data?
  const char	*format = NULL;		// Format of document data


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
    if ((job->num_documents > 0 || job->fd >= 0 || job->streaming) && !(job->system->options & PAPPL_SOPTIONS_MULTI_DOCUMENT_JOBS))
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

  last_document = ippGetBoolean(attr, 0);

  // Validate document attributes...
  if (have_data && !_papplJobValidateDocumentAttributes(client, &format))
  {
    _papplClientFlushDocumentData(client);
    return;
  }

  if (!have_data && job->num_documents == 0)
    job->state = IPP_JSTATE_ABORTED;

  // Then finish getting the document data and process things...
  _papplRWLockWrite(client->printer);

//  _papplCopyAttributes(job->attrs, client->request, NULL, IPP_TAG_JOB, false);

  _papplRWUnlock(client->printer);

  if (have_data)
    _papplJobCopyDocumentData(client, job, format, last_document);
}


//
// 'ipp_update_document_status()' - Update document status.
//

static void
ipp_update_document_status(
    pappl_client_t *client)		// I - Client
{
  pappl_printer_t	*printer = client->printer;
					// Printer
  _pappl_odevice_t	*od;		// Output device


  // Authorize access...
  if (!_papplPrinterIsAuthorized(client))
    return;

  // Find the output device
  _papplRWLockRead(printer);
  cupsRWLockWrite(&printer->output_rwlock);

  if ((od = _papplClientFindDeviceNoLock(client)) != NULL)
  {
    pappl_job_t		*job = client->job;
					// Job
    _pappl_doc_t	*doc;		// Document
    ipp_attribute_t	*attr;		// Request attribute...
    pappl_event_t	events = PAPPL_EVENT_NONE;
					// Notification event(s)

    _papplRWLockWrite(job);

    if (!job->output_device)
    {
      papplClientRespondIPP(client, IPP_STATUS_ERROR_NOT_POSSIBLE, "Job not accepted.");
    }
    else if (job->output_device != od)
    {
      papplClientRespondIPP(client, IPP_STATUS_ERROR_NOT_POSSIBLE, "Job accepted by another device.");
    }
    else if ((doc = find_document_no_lock(client)) != NULL)
    {
      if (doc->state >= IPP_DSTATE_CANCELED)
      {
	// Document is in a terminating state...
	papplClientRespondIPP(client, IPP_STATUS_ERROR_NOT_POSSIBLE, "Document already terminated.");
      }
      else
      {
        // Update document state
        if ((attr = ippFindAttribute(client->request, "impressions-completed", IPP_TAG_ZERO)) != NULL)
        {
          if (ippGetGroupTag(attr) != IPP_TAG_DOCUMENT || ippGetValueTag(attr) != IPP_TAG_INTEGER || ippGetCount(attr) != 1)
          {
            papplClientRespondIPPUnsupported(client, attr);
	  }
	  else
	  {
	    doc->impcompleted = ippGetInteger(attr, 0);
	    events |= PAPPL_EVENT_DOCUMENT_STATE_CHANGED;
	  }
        }

        if ((attr = ippFindAttribute(client->request, "output-device-document-state", IPP_TAG_ZERO)) != NULL)
        {
          ipp_dstate_t	dstate;		// document-state value

          if (ippGetGroupTag(attr) != IPP_TAG_DOCUMENT || ippGetValueTag(attr) != IPP_TAG_ENUM || ippGetCount(attr) != 1)
          {
            papplClientRespondIPPUnsupported(client, attr);
	  }
	  else if ((dstate = (ipp_dstate_t)ippGetInteger(attr, 0)) >= IPP_DSTATE_CANCELED)
	  {
	    doc->state = dstate;
	    events |= PAPPL_EVENT_DOCUMENT_COMPLETED;
	  }
        }

        if ((attr = ippFindAttribute(client->request, "output-device-document-state-reasons", IPP_TAG_ZERO)) != NULL)
        {
          if (ippGetGroupTag(attr) != IPP_TAG_DOCUMENT || ippGetValueTag(attr) != IPP_TAG_KEYWORD)
          {
            papplClientRespondIPPUnsupported(client, attr);
	  }
	  else
	  {
	    size_t	i,		// Looping var
			count;		// Number of reasons
	    pappl_jreason_t reasons = PAPPL_JREASON_NONE;
					// document-state-reasons values

            for (i = 0, count = ippGetCount(attr); i < count; i ++)
              reasons |= _papplJobReasonValue(ippGetString(attr, i, NULL));

	    doc->state_reasons = reasons;
	    events |= PAPPL_EVENT_DOCUMENT_STATE_CHANGED;
	  }
        }

        if (ippGetStatusCode(client->response) == IPP_STATUS_OK)
          papplClientRespondIPP(client, IPP_STATUS_OK, /*message*/NULL);
      }
    }

    if (events)
      _papplSystemAddEventNoLock(job->system, printer, job, events, /*message*/NULL);

    _papplRWUnlock(job);
  }

  cupsRWUnlock(&printer->output_rwlock);
  _papplRWUnlock(printer);
}


//
// 'ipp_update_job_status()' - Update job status.
//

static void
ipp_update_job_status(
    pappl_client_t *client)		// I - Client
{
  pappl_printer_t	*printer = client->printer;
					// Printer
  _pappl_odevice_t	*od;		// Output device


  // Authorize access...
  if (!_papplPrinterIsAuthorized(client))
    return;

  // Find the output device
  _papplRWLockWrite(printer);

  if ((od = _papplClientFindDeviceNoLock(client)) != NULL)
  {
    pappl_job_t		*job = client->job;
					// Job
    ipp_attribute_t	*attr;		// Request attribute...
    pappl_event_t	events = PAPPL_EVENT_NONE;
					// Notification event(s)

    _papplRWLockWrite(job);

    if (!job->output_device)
    {
      papplClientRespondIPP(client, IPP_STATUS_ERROR_NOT_POSSIBLE, "Job not accepted.");
    }
    else if (job->output_device != od)
    {
      papplClientRespondIPP(client, IPP_STATUS_ERROR_NOT_POSSIBLE, "Job accepted by another device.");
    }
    else if (job->state >= IPP_JSTATE_CANCELED)
    {
      // Job is in a terminating state...
      papplClientRespondIPP(client, IPP_STATUS_ERROR_NOT_POSSIBLE, "Job already terminated.");
    }
    else
    {
      // Update job state
      if ((attr = ippFindAttribute(client->request, "job-impressions-completed", IPP_TAG_ZERO)) != NULL)
      {
	if (ippGetGroupTag(attr) != IPP_TAG_JOB || ippGetValueTag(attr) != IPP_TAG_INTEGER || ippGetCount(attr) != 1)
	{
	  papplClientRespondIPPUnsupported(client, attr);
	}
	else
	{
	  job->impcompleted = ippGetInteger(attr, 0);
	  events |= PAPPL_EVENT_JOB_STATE_CHANGED;
	}
      }

      if ((attr = ippFindAttribute(client->request, "output-device-job-state", IPP_TAG_ZERO)) != NULL)
      {
        ipp_jstate_t	jstate;		// New job state

	if (ippGetGroupTag(attr) != IPP_TAG_DOCUMENT || ippGetValueTag(attr) != IPP_TAG_ENUM || ippGetCount(attr) != 1)
	{
	  papplClientRespondIPPUnsupported(client, attr);
	}
	else if ((jstate = (ipp_jstate_t)ippGetInteger(attr, 0)) >= IPP_JSTATE_ABORTED)
	{
	  // Job is completed, aborted, or canceled on the Proxy side...
	  _papplJobSetStateNoLock(job, jstate);
	  _papplJobSetRetainNoLock(job);

	  printer->processing_job = NULL;

	  if (!printer->max_preserved_jobs && !job->retain_until)
	    _papplJobRemoveFiles(job);

	  if (printer->is_stopped)
	  {
	    // New printer-state is 'stopped'...
	    printer->state      = IPP_PSTATE_STOPPED;
	    printer->is_stopped = false;
	  }
	  else
	  {
	    // New printer-state is 'idle'...
	    printer->state = IPP_PSTATE_IDLE;
	  }

	  printer->state_time = time(NULL);

	  cupsArrayRemove(printer->active_jobs, job);
	  cupsArrayAdd(printer->completed_jobs, job);

	  printer->impcompleted += job->impcompleted;

	  if (!job->system->clean_time)
	    job->system->clean_time = time(NULL) + 60;

	  events |= PAPPL_EVENT_JOB_COMPLETED;
	}
      }

      if ((attr = ippFindAttribute(client->request, "output-device-job-state-reasons", IPP_TAG_ZERO)) != NULL)
      {
	if (ippGetGroupTag(attr) != IPP_TAG_DOCUMENT || ippGetValueTag(attr) != IPP_TAG_KEYWORD)
	{
	  papplClientRespondIPPUnsupported(client, attr);
	}
	else
	{
	  size_t	i,		// Looping var
		      count;		// Number of reasons
	  pappl_jreason_t reasons = PAPPL_JREASON_NONE;
				      // document-state-reasons values

	  for (i = 0, count = ippGetCount(attr); i < count; i ++)
	    reasons |= _papplJobReasonValue(ippGetString(attr, i, NULL));

	  job->state_reasons = reasons;
	  events |= PAPPL_EVENT_JOB_STATE_CHANGED;
	}
      }

      if (ippGetStatusCode(client->response) == IPP_STATUS_OK)
	papplClientRespondIPP(client, IPP_STATUS_OK, /*message*/NULL);
    }

    if (events)
      _papplSystemAddEventNoLock(job->system, printer, job, events, /*message*/NULL);

    _papplRWUnlock(job);
  }

  cupsRWUnlock(&printer->output_rwlock);
  _papplRWUnlock(printer);
}
