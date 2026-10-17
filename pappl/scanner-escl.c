//
// eSCL protocol endpoint handler for the Printer Application Framework
//
// Copyright © 2019-2026 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#include "pappl-private.h"
#include <mxml.h>


//
// Local types...
//

typedef enum escl_state_e		// eSCL scanner state
{
  ESCL_STATE_IDLE = 0,			// Idle
  ESCL_STATE_PROCESSING,		// Processing
  ESCL_STATE_TESTING,			// Testing/calibrating
  ESCL_STATE_STOPPED,			// Stopped
  ESCL_STATE_DOWN			// Down
} escl_state_t;

typedef enum escl_job_state_e		// eSCL job state
{
  ESCL_JSTATE_PENDING = 0,		// Pending
  ESCL_JSTATE_PROCESSING,		// Processing
  ESCL_JSTATE_COMPLETED,		// Completed
  ESCL_JSTATE_CANCELED,			// Canceled
  ESCL_JSTATE_ABORTED			// Aborted
} escl_job_state_t;


//
// Local functions...
//

static void		escl_get_capabilities(pappl_client_t *client, pappl_scanner_t *scanner);
static void		escl_get_status(pappl_client_t *client, pappl_scanner_t *scanner);
static void		escl_post_scan_jobs(pappl_client_t *client, pappl_scanner_t *scanner);
static void		escl_get_next_document(pappl_client_t *client, pappl_scanner_t *scanner, int job_id);
static void		escl_get_scan_image_info(pappl_client_t *client, pappl_scanner_t *scanner, int job_id);
static void		escl_delete_scan_job(pappl_client_t *client, pappl_scanner_t *scanner, int job_id);

static mxml_node_t	*escl_create_capabilities_xml(pappl_scanner_t *scanner);
static mxml_node_t	*escl_create_status_xml(pappl_scanner_t *scanner);
static bool		escl_parse_scan_settings(const char *xml_data, size_t xml_len, pappl_sc_options_t *options);

static const char	*escl_color_mode_string(pappl_scan_color_mode_t mode) __attribute__((unused));
static pappl_scan_color_mode_t escl_color_mode_value(const char *s);
static const char	*escl_input_source_string(pappl_scan_input_source_t src) __attribute__((unused));
static pappl_scan_input_source_t escl_input_source_value(const char *s);
static const char	*escl_intent_string(pappl_scan_intent_t intent) __attribute__((unused));
static pappl_scan_intent_t escl_intent_value(const char *s);
static const char	*escl_content_type_string(pappl_scan_content_t ct) __attribute__((unused));
static pappl_scan_content_t escl_content_type_value(const char *s);
static const char	*escl_job_state_string(ipp_jstate_t state);
static const char	*escl_job_state_reason_string(ipp_jstate_t state);
static const char	*escl_scanner_state_string(ipp_pstate_t state);

static void		escl_add_input_source_caps(mxml_node_t *parent, const char *element, pappl_sc_driver_data_t *data, int min_w, int min_h, int max_w, int max_h);
static mxml_node_t	*escl_new_element_text(mxml_node_t *parent, const char *name, const char *value);
static mxml_node_t	*escl_new_element_int(mxml_node_t *parent, const char *name, int value);
static char		*escl_xml_to_string(mxml_node_t *xml, size_t *length);
static mxml_type_t		escl_type_cb(void *cbdata, mxml_node_t *node) __attribute__((unused));


//
// '_papplScannerProcessESCL()' - Process an eSCL HTTP request.
//

void
_papplScannerProcessESCL(
    pappl_client_t *client)		// I - Client connection
{
  pappl_system_t	*system = client->system;
					// System
  pappl_scanner_t	*scanner = NULL;// Scanner
  const char		*path;		// Path after /eSCL/
  int			job_id = 0;	// Job ID from path
  char			*end;		// End pointer


  // Skip "/eSCL/" prefix...
  path = client->uri + 6;

  // Find the scanner - for single-scanner systems use the first scanner
  _papplRWLockRead(system);
  if (system->scanners && cupsArrayGetCount(system->scanners) > 0)
    scanner = (pappl_scanner_t *)cupsArrayGetElement(system->scanners, 0);
  _papplRWUnlock(system);

  if (!scanner)
  {
    papplClientRespond(client, HTTP_STATUS_NOT_FOUND, NULL, NULL, 0, 0);
    return;
  }

  // Route the request based on method and path...
  if (client->operation == HTTP_STATE_GET)
  {
    if (!strcmp(path, "ScannerCapabilities"))
    {
      escl_get_capabilities(client, scanner);
    }
    else if (!strcmp(path, "ScannerStatus"))
    {
      escl_get_status(client, scanner);
    }
    else if (!strncmp(path, "ScanJobs/", 9))
    {
      // Parse job ID from path: ScanJobs/{id}/NextDocument or
      // ScanJobs/{id}/ScanImageInfo
      job_id = (int)strtol(path + 9, &end, 10);
      if (job_id <= 0)
      {
        papplClientRespond(client, HTTP_STATUS_NOT_FOUND, NULL, NULL, 0, 0);
        return;
      }

      if (!strcmp(end, "/NextDocument"))
      {
        escl_get_next_document(client, scanner, job_id);
      }
      else if (!strcmp(end, "/ScanImageInfo"))
      {
        escl_get_scan_image_info(client, scanner, job_id);
      }
      else
      {
        papplClientRespond(client, HTTP_STATUS_NOT_FOUND, NULL, NULL, 0, 0);
      }
    }
    else
    {
      papplClientRespond(client, HTTP_STATUS_NOT_FOUND, NULL, NULL, 0, 0);
    }
  }
  else if (client->operation == HTTP_STATE_POST)
  {
    if (!strcmp(path, "ScanJobs"))
    {
      escl_post_scan_jobs(client, scanner);
    }
    else
    {
      papplClientRespond(client, HTTP_STATUS_NOT_FOUND, NULL, NULL, 0, 0);
    }
  }
  else if (client->operation == HTTP_STATE_DELETE)
  {
    // Parse: ScanJobs/{id}
    if (!strncmp(path, "ScanJobs/", 9))
    {
      job_id = (int)strtol(path + 9, &end, 10);
      if (job_id > 0 && (*end == '\0' || *end == '/'))
      {
        escl_delete_scan_job(client, scanner, job_id);
      }
      else
      {
        papplClientRespond(client, HTTP_STATUS_NOT_FOUND, NULL, NULL, 0, 0);
      }
    }
    else
    {
      papplClientRespond(client, HTTP_STATUS_NOT_FOUND, NULL, NULL, 0, 0);
    }
  }
  else
  {
    papplClientRespond(client, HTTP_STATUS_METHOD_NOT_ALLOWED, NULL, NULL, 0, 0);
  }
}


//
// 'escl_get_capabilities()' - Handle GET /eSCL/ScannerCapabilities.
//

static void
escl_get_capabilities(
    pappl_client_t  *client,		// I - Client connection
    pappl_scanner_t *scanner)		// I - Scanner
{
  mxml_node_t	*xml;			// XML document
  char		*xml_str;		// XML string
  size_t	xml_len;		// XML string length


  // Build capabilities XML...
  xml = escl_create_capabilities_xml(scanner);
  if (!xml)
  {
    papplClientRespond(client, HTTP_STATUS_SERVER_ERROR, NULL, NULL, 0, 0);
    return;
  }

  // Convert to string...
  xml_str = escl_xml_to_string(xml, &xml_len);
  mxmlDelete(xml);

  if (!xml_str)
  {
    papplClientRespond(client, HTTP_STATUS_SERVER_ERROR, NULL, NULL, 0, 0);
    return;
  }

  // Send response...
  papplClientRespond(client, HTTP_STATUS_OK, NULL, "text/xml", 0, xml_len);
  httpWrite(client->http, xml_str, xml_len);
  httpFlushWrite(client->http);

  free(xml_str);
}


//
// 'escl_get_status()' - Handle GET /eSCL/ScannerStatus.
//

static void
escl_get_status(
    pappl_client_t  *client,		// I - Client connection
    pappl_scanner_t *scanner)		// I - Scanner
{
  mxml_node_t	*xml;			// XML document
  char		*xml_str;		// XML string
  size_t	xml_len;		// XML string length


  // Build status XML...
  xml = escl_create_status_xml(scanner);
  if (!xml)
  {
    papplClientRespond(client, HTTP_STATUS_SERVER_ERROR, NULL, NULL, 0, 0);
    return;
  }

  // Convert to string...
  xml_str = escl_xml_to_string(xml, &xml_len);
  mxmlDelete(xml);

  if (!xml_str)
  {
    papplClientRespond(client, HTTP_STATUS_SERVER_ERROR, NULL, NULL, 0, 0);
    return;
  }

  // Send response...
  papplClientRespond(client, HTTP_STATUS_OK, NULL, "text/xml", 0, xml_len);
  httpWrite(client->http, xml_str, xml_len);
  httpFlushWrite(client->http);

  free(xml_str);
}


//
// 'escl_post_scan_jobs()' - Handle POST /eSCL/ScanJobs.
//

static void
escl_post_scan_jobs(
    pappl_client_t  *client,		// I - Client connection
    pappl_scanner_t *scanner)		// I - Scanner
{
  pappl_sc_options_t	options;	// Scan options
  pappl_job_t		*job;		// New scan job
  char			body[65536];	// Request body buffer
  ssize_t		body_len;	// Actual body length
  size_t		total = 0;	// Total bytes read
  char			location[1024];	// Location header value


  // Check scanner state...
  if (papplScannerGetState(scanner) == IPP_PSTATE_STOPPED)
  {
    papplClientRespond(client, HTTP_STATUS_SERVICE_UNAVAILABLE, NULL, NULL, 0, 0);
    return;
  }

  // Read the XML request body...
  while (total < (sizeof(body) - 1))
  {
    body_len = httpRead(client->http, body + total, sizeof(body) - 1 - total);
    if (body_len <= 0)
      break;
    total += (size_t)body_len;
  }
  body[total] = '\0';

  if (total == 0)
  {
    papplClientRespond(client, HTTP_STATUS_BAD_REQUEST, NULL, NULL, 0, 0);
    return;
  }

  // Parse the scan settings XML...
  memset(&options, 0, sizeof(options));
  if (!escl_parse_scan_settings(body, total, &options))
  {
    papplClientRespond(client, HTTP_STATUS_CONFLICT, NULL, NULL, 0, 0);
    return;
  }

  // Create the scan job...
  job = _papplJobCreateScan(scanner, /*username*/NULL, &options);
  if (!job)
  {
    papplClientRespond(client, HTTP_STATUS_SERVICE_UNAVAILABLE, NULL, NULL, 0, 0);
    return;
  }

  // Start checking for pending jobs...
  _papplRWLockWrite(scanner);
  _papplScannerCheckJobsNoLock(scanner);
  _papplRWUnlock(scanner);

  // Return 201 Created with Location header...
  // Use papplClientRespondCreated() because papplClientRespond() clears
  // HTTP fields, which would wipe out the Location header.
  snprintf(location, sizeof(location), "/eSCL/ScanJobs/%d", job->job_id);

  papplClientRespondCreated(client, location);
}


//
// 'escl_get_next_document()' - Handle GET /eSCL/ScanJobs/{id}/NextDocument.
//

static void
escl_get_next_document(
    pappl_client_t  *client,		// I - Client connection
    pappl_scanner_t *scanner,		// I - Scanner
    int             job_id)		// I - Job ID
{
  pappl_job_t	*job;			// Scan job
  int		page;			// Page number to retrieve
  char		fname[1024];		// Page file path
  int		fd;			// File descriptor
  struct stat	st;			// File info
  char		buffer[65536];		// Read buffer
  ssize_t	bytes;			// Bytes read
  pappl_sc_options_t *options = NULL;	// Scan options
  const char	*content_type;		// Content-Type for response


  // Find the job...
  job = papplScannerFindJob(scanner, job_id);
  if (!job || !job->is_scan_job)
  {
    papplClientRespond(client, HTTP_STATUS_NOT_FOUND, NULL, NULL, 0, 0);
    return;
  }

  // Check if job is canceled or aborted...
  if (job->state == IPP_JSTATE_CANCELED || job->state == IPP_JSTATE_ABORTED)
  {
    papplClientRespond(client, HTTP_STATUS_GONE, NULL, NULL, 0, 0);
    return;
  }

  // Determine which page to send next...
  page = job->scan_pages_sent + 1;

  // Check if page data is ready...
  if (!_papplJobGetScanPageFile(job, page, fname, sizeof(fname)))
  {
    // No page file exists yet...
    if (job->scan_complete && page > job->scan_pages_ready)
    {
      // Scanning is done and all pages sent - no more pages...
      if (job->scan_pages_sent > 0)
        _papplJobCompleteScan(job);

      papplClientRespond(client, HTTP_STATUS_NOT_FOUND, NULL, NULL, 0, 0);
      return;
    }

    // Scanning still in progress - tell client to retry...
    httpSetField(client->http, HTTP_FIELD_WWW_AUTHENTICATE, "3");
    papplClientRespond(client, HTTP_STATUS_SERVICE_UNAVAILABLE, NULL, NULL, 0, 0);
    return;
  }

  // Open the page file...
  if ((fd = open(fname, O_RDONLY)) < 0)
  {
    papplClientRespond(client, HTTP_STATUS_SERVER_ERROR, NULL, NULL, 0, 0);
    return;
  }

  if (fstat(fd, &st) < 0)
  {
    close(fd);
    papplClientRespond(client, HTTP_STATUS_SERVER_ERROR, NULL, NULL, 0, 0);
    return;
  }

  // If the page file is empty and scanning is still in progress, the page
  // isn't ready yet.  Return 503 so the client retries instead of getting a
  // 0-byte response that looks like a truncated stream.
  if (st.st_size == 0 && !job->scan_complete)
  {
    close(fd);
    httpSetField(client->http, HTTP_FIELD_WWW_AUTHENTICATE, "3");
    papplClientRespond(client, HTTP_STATUS_SERVICE_UNAVAILABLE, NULL, NULL, 0, 0);
    return;
  }

  // Determine Content-Type from scan options...
  options = papplJobCreateScanOptions(job);
  if (options && !strncmp(options->format, "application/pdf", 15))
    content_type = "application/pdf";
  else if (options && !strncmp(options->format, "image/png", 9))
    content_type = "image/png";
  else
    content_type = "image/jpeg";
  papplJobDeleteScanOptions(options);

  // Send the response...
  papplClientRespond(client, HTTP_STATUS_OK, NULL, content_type, 0, (size_t)st.st_size);

  // Stream the file data...
  while ((bytes = read(fd, buffer, sizeof(buffer))) > 0)
    httpWrite(client->http, buffer, (size_t)bytes);

  httpFlushWrite(client->http);
  close(fd);

  // Update the sent counter...
  _papplRWLockWrite(job);
  job->scan_pages_sent = page;
  _papplRWUnlock(job);

  // If scanning is complete and all pages sent, complete the job...
  if (job->scan_complete && job->scan_pages_sent >= job->scan_pages_ready)
    _papplJobCompleteScan(job);
}


//
// 'escl_get_scan_image_info()' - Handle GET /eSCL/ScanJobs/{id}/ScanImageInfo.
//

static void
escl_get_scan_image_info(
    pappl_client_t  *client,		// I - Client connection
    pappl_scanner_t *scanner,		// I - Scanner
    int             job_id)		// I - Job ID
{
  pappl_job_t		*job;		// Scan job
  mxml_node_t		*xml,		// XML document
			*info_node;	// ScanImageInfo node
  pappl_sc_options_t	*options;	// Scan options
  int			width_pixels,	// Image width in pixels
			height_pixels,	// Image height in pixels
			bytes_per_line;	// Bytes per raster line
  char			*xml_str;	// XML string
  size_t		xml_len;	// XML length


  // Find the job...
  job = papplScannerFindJob(scanner, job_id);
  if (!job || !job->is_scan_job)
  {
    papplClientRespond(client, HTTP_STATUS_NOT_FOUND, NULL, NULL, 0, 0);
    return;
  }

  // Get scan options to compute dimensions...
  options = papplJobCreateScanOptions(job);
  if (!options)
  {
    papplClientRespond(client, HTTP_STATUS_SERVER_ERROR, NULL, NULL, 0, 0);
    return;
  }

  // Calculate image dimensions in pixels...
  width_pixels  = options->scan_width * options->x_resolution / 300;
  height_pixels = options->scan_height * options->y_resolution / 300;

  // Calculate bytes per line based on color mode...
  switch (options->color_mode)
  {
    case PAPPL_SCAN_COLOR_MODE_BLACK_AND_WHITE_1 :
        bytes_per_line = (width_pixels + 7) / 8;
        break;
    case PAPPL_SCAN_COLOR_MODE_GRAYSCALE_8 :
        bytes_per_line = width_pixels;
        break;
    case PAPPL_SCAN_COLOR_MODE_GRAYSCALE_16 :
        bytes_per_line = width_pixels * 2;
        break;
    case PAPPL_SCAN_COLOR_MODE_RGB_48 :
        bytes_per_line = width_pixels * 6;
        break;
    default : // RGB_24
        bytes_per_line = width_pixels * 3;
        break;
  }

  papplJobDeleteScanOptions(options);

  // Build XML response...
  xml = mxmlNewXML("1.0");

  info_node = mxmlNewElement(xml, "scan:ScanImageInfo");
  mxmlElementSetAttr(info_node, "xmlns:scan", "http://schemas.hp.com/imaging/escl/2011/05/03");
  mxmlElementSetAttr(info_node, "xmlns:pwg", "http://www.pwg.org/schemas/2010/12/sm");

  escl_new_element_text(info_node, "pwg:JobState", escl_job_state_string(job->state));
  escl_new_element_int(info_node, "scan:ActualWidth", width_pixels);
  escl_new_element_int(info_node, "scan:ActualHeight", height_pixels);
  escl_new_element_int(info_node, "scan:ActualBytesPerLine", bytes_per_line);

  // Convert to string and send...
  xml_str = escl_xml_to_string(xml, &xml_len);
  mxmlDelete(xml);

  if (!xml_str)
  {
    papplClientRespond(client, HTTP_STATUS_SERVER_ERROR, NULL, NULL, 0, 0);
    return;
  }

  papplClientRespond(client, HTTP_STATUS_OK, NULL, "text/xml", 0, xml_len);
  httpWrite(client->http, xml_str, xml_len);
  httpFlushWrite(client->http);

  free(xml_str);
}


//
// 'escl_delete_scan_job()' - Handle DELETE /eSCL/ScanJobs/{id}.
//

static void
escl_delete_scan_job(
    pappl_client_t  *client,		// I - Client connection
    pappl_scanner_t *scanner,		// I - Scanner
    int             job_id)		// I - Job ID
{
  pappl_job_t	*job;			// Scan job


  // Find the job...
  job = papplScannerFindJob(scanner, job_id);
  if (!job || !job->is_scan_job)
  {
    papplClientRespond(client, HTTP_STATUS_NOT_FOUND, NULL, NULL, 0, 0);
    return;
  }

  // If completed, return 410 Gone...
  if (job->state == IPP_JSTATE_COMPLETED || job->state == IPP_JSTATE_CANCELED ||
      job->state == IPP_JSTATE_ABORTED)
  {
    papplClientRespond(client, HTTP_STATUS_GONE, NULL, NULL, 0, 0);
    return;
  }

  // Cancel the job...
  _papplJobCancelScan(job);

  papplClientRespond(client, HTTP_STATUS_OK, NULL, NULL, 0, 0);
}


//
// 'escl_create_capabilities_xml()' - Create ScannerCapabilities XML.
//

static mxml_node_t *			// O - XML document tree
escl_create_capabilities_xml(
    pappl_scanner_t *scanner)		// I - Scanner
{
  mxml_node_t		*xml,		// XML document
			*caps,		// ScannerCapabilities node
			*node;		// Current node
  pappl_sc_driver_data_t data;		// Driver data
  size_t		i;		// Looping var
  char			uri_buf[1024];	// URI buffer


  // Get driver data...
  _papplRWLockRead(scanner);
  memcpy(&data, &scanner->driver_data, sizeof(data));
  _papplRWUnlock(scanner);

  // Create XML document with declaration...
  xml = mxmlNewXML("1.0");

  // Create root element with namespaces...
  caps = mxmlNewElement(xml, "scan:ScannerCapabilities");
  mxmlElementSetAttr(caps, "xmlns:scan", "http://schemas.hp.com/imaging/escl/2011/05/03");
  mxmlElementSetAttr(caps, "xmlns:pwg", "http://www.pwg.org/schemas/2010/12/sm");

  // Version...
  escl_new_element_text(caps, "pwg:Version", "2.0");

  // Make and model...
  escl_new_element_text(caps, "pwg:MakeAndModel", data.make_and_model);

  // UUID...
  _papplRWLockRead(scanner);
  escl_new_element_text(caps, "scan:UUID", scanner->uuid);
  _papplRWUnlock(scanner);

  // AdminURI...
  _papplRWLockRead(scanner);
  snprintf(uri_buf, sizeof(uri_buf), "http://%s:%d%s",
           scanner->system->hostname, scanner->system->port,
           scanner->resource);
  _papplRWUnlock(scanner);
  escl_new_element_text(caps, "scan:AdminURI", uri_buf);

  // IconURI...
  _papplRWLockRead(scanner);
  snprintf(uri_buf, sizeof(uri_buf), "http://%s:%d/icon-lg.png",
           scanner->system->hostname, scanner->system->port);
  _papplRWUnlock(scanner);
  escl_new_element_text(caps, "scan:IconURI", uri_buf);

  // Platen capabilities...
  if (data.input_sources_supported & PAPPL_SCAN_INPUT_SOURCE_PLATEN)
  {
    node = mxmlNewElement(caps, "scan:Platen");
    escl_add_input_source_caps(node, "scan:PlatenInputCaps", &data,
        data.platen_min_width, data.platen_min_height,
        data.platen_max_width, data.platen_max_height);
  }

  // ADF capabilities...
  if (data.input_sources_supported & PAPPL_SCAN_INPUT_SOURCE_ADF)
  {
    node = mxmlNewElement(caps, "scan:Adf");
    escl_add_input_source_caps(node, "scan:AdfSimplexInputCaps", &data,
        data.adf_min_width, data.adf_min_height,
        data.adf_max_width, data.adf_max_height);

    if (data.duplex_supported)
    {
      escl_add_input_source_caps(node, "scan:AdfDuplexInputCaps", &data,
          data.adf_min_width, data.adf_min_height,
          data.adf_max_width, data.adf_max_height);
    }
  }

  // Camera capabilities (if supported)...
  if (data.input_sources_supported & PAPPL_SCAN_INPUT_SOURCE_CAMERA)
  {
    node = mxmlNewElement(caps, "scan:Camera");
    escl_add_input_source_caps(node, "scan:CameraInputCaps", &data,
        data.platen_min_width, data.platen_min_height,
        data.platen_max_width, data.platen_max_height);
  }

  // Supported intents...
  node = mxmlNewElement(caps, "scan:SupportedIntents");
  if (data.intents_supported & PAPPL_SCAN_INTENT_DOCUMENT)
    escl_new_element_text(node, "scan:Intent", "Document");
  if (data.intents_supported & PAPPL_SCAN_INTENT_TEXT_AND_GRAPHIC)
    escl_new_element_text(node, "scan:Intent", "TextAndGraphic");
  if (data.intents_supported & PAPPL_SCAN_INTENT_PHOTO)
    escl_new_element_text(node, "scan:Intent", "Photo");
  if (data.intents_supported & PAPPL_SCAN_INTENT_PREVIEW)
    escl_new_element_text(node, "scan:Intent", "Preview");
  if (data.intents_supported & PAPPL_SCAN_INTENT_BUSINESS_CARD)
    escl_new_element_text(node, "scan:Intent", "BusinessCard");

  // Image adjustments...
  if (data.brightness_supported)
  {
    node = mxmlNewElement(caps, "scan:BrightnessSupport");
    escl_new_element_int(node, "scan:Min", -100);
    escl_new_element_int(node, "scan:Max", 100);
    escl_new_element_int(node, "scan:Normal", 0);
    escl_new_element_int(node, "scan:Step", 1);
  }

  if (data.contrast_supported)
  {
    node = mxmlNewElement(caps, "scan:ContrastSupport");
    escl_new_element_int(node, "scan:Min", -100);
    escl_new_element_int(node, "scan:Max", 100);
    escl_new_element_int(node, "scan:Normal", 0);
    escl_new_element_int(node, "scan:Step", 1);
  }

  if (data.sharpen_supported)
  {
    node = mxmlNewElement(caps, "scan:SharpenSupport");
    escl_new_element_int(node, "scan:Min", 0);
    escl_new_element_int(node, "scan:Max", 100);
    escl_new_element_int(node, "scan:Normal", 0);
    escl_new_element_int(node, "scan:Step", 1);
  }

  if (data.threshold_supported)
  {
    node = mxmlNewElement(caps, "scan:ThresholdSupport");
    escl_new_element_int(node, "scan:Min", 0);
    escl_new_element_int(node, "scan:Max", 255);
    escl_new_element_int(node, "scan:Normal", 128);
    escl_new_element_int(node, "scan:Step", 1);
  }

  (void)i;

  return (xml);
}


//
// 'escl_create_status_xml()' - Create ScannerStatus XML.
//

static mxml_node_t *			// O - XML document tree
escl_create_status_xml(
    pappl_scanner_t *scanner)		// I - Scanner
{
  mxml_node_t	*xml,			// XML document
		*status,		// ScannerStatus node
		*jobs_node,		// Jobs node
		*job_info;		// JobInfo node
  size_t	i,			// Looping var
		count;			// Number of jobs
  pappl_job_t	*job;			// Current job
  time_t	now;			// Current time


  // Create XML document...
  xml = mxmlNewXML("1.0");

  status = mxmlNewElement(xml, "scan:ScannerStatus");
  mxmlElementSetAttr(status, "xmlns:scan", "http://schemas.hp.com/imaging/escl/2011/05/03");
  mxmlElementSetAttr(status, "xmlns:pwg", "http://www.pwg.org/schemas/2010/12/sm");

  // Version...
  escl_new_element_text(status, "pwg:Version", "2.0");

  // Scanner state...
  _papplRWLockRead(scanner);
  escl_new_element_text(status, "pwg:State", escl_scanner_state_string(scanner->state));
  _papplRWUnlock(scanner);

  // Jobs section...
  now = time(NULL);
  jobs_node = mxmlNewElement(status, "scan:Jobs");

  // Add active jobs...
  _papplRWLockRead(scanner);
  count = cupsArrayGetCount(scanner->active_jobs);
  for (i = 0; i < count; i ++)
  {
    job = (pappl_job_t *)cupsArrayGetElement(scanner->active_jobs, i);
    if (!job)
      continue;

    job_info = mxmlNewElement(jobs_node, "scan:JobInfo");

    char job_uri[256];
    snprintf(job_uri, sizeof(job_uri), "/eSCL/ScanJobs/%d", job->job_id);
    escl_new_element_text(job_info, "pwg:JobUri", job_uri);
    escl_new_element_text(job_info, "pwg:JobUuid", job->uri ? job->uri : "");
    escl_new_element_int(job_info, "scan:Age", (int)(now - job->created));
    escl_new_element_int(job_info, "pwg:ImagesCompleted", job->scan_pages_ready);
    escl_new_element_int(job_info, "pwg:ImagesToTransfer",
        job->scan_pages_ready - job->scan_pages_sent);
    escl_new_element_text(job_info, "pwg:JobState", escl_job_state_string(job->state));

    mxml_node_t *reasons = mxmlNewElement(job_info, "pwg:JobStateReasons");
    escl_new_element_text(reasons, "pwg:JobStateReason",
        escl_job_state_reason_string(job->state));
  }

  // Add most recent completed jobs (up to 2 per spec)...
  count = cupsArrayGetCount(scanner->completed_jobs);
  for (i = (count > 2) ? count - 2 : 0; i < count; i ++)
  {
    job = (pappl_job_t *)cupsArrayGetElement(scanner->completed_jobs, i);
    if (!job)
      continue;

    job_info = mxmlNewElement(jobs_node, "scan:JobInfo");

    char job_uri[256];
    snprintf(job_uri, sizeof(job_uri), "/eSCL/ScanJobs/%d", job->job_id);
    escl_new_element_text(job_info, "pwg:JobUri", job_uri);
    escl_new_element_text(job_info, "pwg:JobUuid", job->uri ? job->uri : "");
    escl_new_element_int(job_info, "scan:Age", (int)(now - job->completed));
    escl_new_element_int(job_info, "pwg:ImagesCompleted", job->scan_pages_ready);
    escl_new_element_int(job_info, "pwg:ImagesToTransfer", 0);
    escl_new_element_text(job_info, "pwg:JobState", escl_job_state_string(job->state));

    mxml_node_t *reasons = mxmlNewElement(job_info, "pwg:JobStateReasons");
    escl_new_element_text(reasons, "pwg:JobStateReason",
        escl_job_state_reason_string(job->state));
  }
  _papplRWUnlock(scanner);

  return (xml);
}


//
// 'escl_parse_scan_settings()' - Parse eSCL ScanSettings XML into options.
//

static bool				// O - true on success, false on error
escl_parse_scan_settings(
    const char         *xml_data,	// I - XML data
    size_t             xml_len,		// I - XML data length
    pappl_sc_options_t *options)		// O - Scan options
{
  mxml_node_t	*xml,			// Parsed XML
		*settings,		// ScanSettings node
		*node;			// Current node
  const char	*text;			// Text value
  mxml_options_t *mopts;		// mxml options


  (void)xml_len;

  // Parse XML...
  mopts = mxmlOptionsNew();
  mxmlOptionsSetTypeValue(mopts, MXML_TYPE_OPAQUE);
  xml = mxmlLoadString(NULL, mopts, xml_data);
  mxmlOptionsDelete(mopts);

  if (!xml)
    return (false);

  // Find ScanSettings element (handle with or without namespace prefix)...
  settings = mxmlFindElement(xml, xml, "scan:ScanSettings", NULL, NULL, MXML_DESCEND_ALL);
  if (!settings)
    settings = mxmlFindElement(xml, xml, "ScanSettings", NULL, NULL, MXML_DESCEND_ALL);
  if (!settings)
  {
    mxmlDelete(xml);
    return (false);
  }

  // Parse Intent...
  node = mxmlFindElement(settings, settings, "scan:Intent", NULL, NULL, MXML_DESCEND_FIRST);
  if (!node)
    node = mxmlFindElement(settings, settings, "Intent", NULL, NULL, MXML_DESCEND_FIRST);
  if (node && (text = mxmlGetOpaque(mxmlGetFirstChild(node))) != NULL)
    options->intent = escl_intent_value(text);

  // Parse InputSource...
  node = mxmlFindElement(settings, settings, "pwg:InputSource", NULL, NULL, MXML_DESCEND_FIRST);
  if (!node)
    node = mxmlFindElement(settings, settings, "InputSource", NULL, NULL, MXML_DESCEND_FIRST);
  if (node && (text = mxmlGetOpaque(mxmlGetFirstChild(node))) != NULL)
    options->input_source = escl_input_source_value(text);

  // Parse ColorMode...
  node = mxmlFindElement(settings, settings, "scan:ColorMode", NULL, NULL, MXML_DESCEND_FIRST);
  if (!node)
    node = mxmlFindElement(settings, settings, "ColorMode", NULL, NULL, MXML_DESCEND_FIRST);
  if (node && (text = mxmlGetOpaque(mxmlGetFirstChild(node))) != NULL)
    options->color_mode = escl_color_mode_value(text);

  // Parse XResolution...
  node = mxmlFindElement(settings, settings, "scan:XResolution", NULL, NULL, MXML_DESCEND_FIRST);
  if (!node)
    node = mxmlFindElement(settings, settings, "XResolution", NULL, NULL, MXML_DESCEND_FIRST);
  if (node && (text = mxmlGetOpaque(mxmlGetFirstChild(node))) != NULL)
    options->x_resolution = atoi(text);

  // Parse YResolution...
  node = mxmlFindElement(settings, settings, "scan:YResolution", NULL, NULL, MXML_DESCEND_FIRST);
  if (!node)
    node = mxmlFindElement(settings, settings, "YResolution", NULL, NULL, MXML_DESCEND_FIRST);
  if (node && (text = mxmlGetOpaque(mxmlGetFirstChild(node))) != NULL)
    options->y_resolution = atoi(text);

  // Parse DocumentFormat / DocumentFormatExt...
  node = mxmlFindElement(settings, settings, "scan:DocumentFormatExt", NULL, NULL, MXML_DESCEND_FIRST);
  if (!node)
    node = mxmlFindElement(settings, settings, "pwg:DocumentFormat", NULL, NULL, MXML_DESCEND_FIRST);
  if (!node)
    node = mxmlFindElement(settings, settings, "DocumentFormatExt", NULL, NULL, MXML_DESCEND_FIRST);
  if (!node)
    node = mxmlFindElement(settings, settings, "DocumentFormat", NULL, NULL, MXML_DESCEND_FIRST);
  if (node && (text = mxmlGetOpaque(mxmlGetFirstChild(node))) != NULL)
    cupsCopyString(options->format, text, sizeof(options->format));

  // Parse ScanRegions...
  node = mxmlFindElement(settings, settings, "pwg:ScanRegion", NULL, NULL, MXML_DESCEND_ALL);
  if (!node)
    node = mxmlFindElement(settings, settings, "ScanRegion", NULL, NULL, MXML_DESCEND_ALL);
  if (node)
  {
    mxml_node_t *child;

    child = mxmlFindElement(node, node, "pwg:Width", NULL, NULL, MXML_DESCEND_FIRST);
    if (!child)
      child = mxmlFindElement(node, node, "Width", NULL, NULL, MXML_DESCEND_FIRST);
    if (child && (text = mxmlGetOpaque(mxmlGetFirstChild(child))) != NULL)
      options->scan_width = atoi(text);

    child = mxmlFindElement(node, node, "pwg:Height", NULL, NULL, MXML_DESCEND_FIRST);
    if (!child)
      child = mxmlFindElement(node, node, "Height", NULL, NULL, MXML_DESCEND_FIRST);
    if (child && (text = mxmlGetOpaque(mxmlGetFirstChild(child))) != NULL)
      options->scan_height = atoi(text);

    child = mxmlFindElement(node, node, "pwg:XOffset", NULL, NULL, MXML_DESCEND_FIRST);
    if (!child)
      child = mxmlFindElement(node, node, "XOffset", NULL, NULL, MXML_DESCEND_FIRST);
    if (child && (text = mxmlGetOpaque(mxmlGetFirstChild(child))) != NULL)
      options->scan_x = atoi(text);

    child = mxmlFindElement(node, node, "pwg:YOffset", NULL, NULL, MXML_DESCEND_FIRST);
    if (!child)
      child = mxmlFindElement(node, node, "YOffset", NULL, NULL, MXML_DESCEND_FIRST);
    if (child && (text = mxmlGetOpaque(mxmlGetFirstChild(child))) != NULL)
      options->scan_y = atoi(text);
  }

  // Parse ContentType...
  node = mxmlFindElement(settings, settings, "pwg:ContentType", NULL, NULL, MXML_DESCEND_FIRST);
  if (!node)
    node = mxmlFindElement(settings, settings, "ContentType", NULL, NULL, MXML_DESCEND_FIRST);
  if (node && (text = mxmlGetOpaque(mxmlGetFirstChild(node))) != NULL)
    options->content_type = escl_content_type_value(text);

  // Parse Duplex...
  node = mxmlFindElement(settings, settings, "scan:Duplex", NULL, NULL, MXML_DESCEND_FIRST);
  if (!node)
    node = mxmlFindElement(settings, settings, "Duplex", NULL, NULL, MXML_DESCEND_FIRST);
  if (node && (text = mxmlGetOpaque(mxmlGetFirstChild(node))) != NULL)
    options->duplex = !strcasecmp(text, "true");

  // Parse Brightness...
  node = mxmlFindElement(settings, settings, "scan:Brightness", NULL, NULL, MXML_DESCEND_FIRST);
  if (node && (text = mxmlGetOpaque(mxmlGetFirstChild(node))) != NULL)
    options->brightness = atoi(text);

  // Parse Contrast...
  node = mxmlFindElement(settings, settings, "scan:Contrast", NULL, NULL, MXML_DESCEND_FIRST);
  if (node && (text = mxmlGetOpaque(mxmlGetFirstChild(node))) != NULL)
    options->contrast = atoi(text);

  // Parse CompressionFactor...
  node = mxmlFindElement(settings, settings, "scan:CompressionFactor", NULL, NULL, MXML_DESCEND_FIRST);
  if (node && (text = mxmlGetOpaque(mxmlGetFirstChild(node))) != NULL)
    options->compression = atoi(text);

  // Parse Sharpen...
  node = mxmlFindElement(settings, settings, "scan:Sharpen", NULL, NULL, MXML_DESCEND_FIRST);
  if (node && (text = mxmlGetOpaque(mxmlGetFirstChild(node))) != NULL)
    options->sharpen = atoi(text);

  // Parse Threshold...
  node = mxmlFindElement(settings, settings, "scan:Threshold", NULL, NULL, MXML_DESCEND_FIRST);
  if (node && (text = mxmlGetOpaque(mxmlGetFirstChild(node))) != NULL)
    options->threshold = atoi(text);

  mxmlDelete(xml);
  return (true);
}


//
// 'escl_add_input_source_caps()' - Add InputSourceCaps XML element.
//

static void
escl_add_input_source_caps(
    mxml_node_t            *parent,	// I - Parent node
    const char             *element,	// I - Element name
    pappl_sc_driver_data_t *data,	// I - Driver data
    int                    min_w,	// I - Minimum width (1/300")
    int                    min_h,	// I - Minimum height (1/300")
    int                    max_w,	// I - Maximum width (1/300")
    int                    max_h)	// I - Maximum height (1/300")
{
  mxml_node_t	*caps,			// InputSourceCaps node
		*profiles,		// SettingProfiles node
		*profile,		// SettingProfile node
		*modes,			// ColorModes node
		*formats,		// DocumentFormats node
		*resolutions,		// SupportedResolutions node
		*discrete,		// DiscreteResolutions node
		*res_node;		// DiscreteResolution node
  size_t	i;			// Looping var


  caps = mxmlNewElement(parent, element);

  // Min/Max dimensions...
  escl_new_element_int(caps, "scan:MinWidth", min_w > 0 ? min_w : 1);
  escl_new_element_int(caps, "scan:MaxWidth", max_w);
  escl_new_element_int(caps, "scan:MinHeight", min_h > 0 ? min_h : 1);
  escl_new_element_int(caps, "scan:MaxHeight", max_h);
  escl_new_element_int(caps, "scan:MaxScanRegions", 1);

  // Setting profiles...
  profiles = mxmlNewElement(caps, "scan:SettingProfiles");
  profile  = mxmlNewElement(profiles, "scan:SettingProfile");

  // Color modes...
  modes = mxmlNewElement(profile, "scan:ColorModes");
  if (data->color_supported & PAPPL_SCAN_COLOR_MODE_BLACK_AND_WHITE_1)
    escl_new_element_text(modes, "scan:ColorMode", "BlackAndWhite1");
  if (data->color_supported & PAPPL_SCAN_COLOR_MODE_GRAYSCALE_8)
    escl_new_element_text(modes, "scan:ColorMode", "Grayscale8");
  if (data->color_supported & PAPPL_SCAN_COLOR_MODE_GRAYSCALE_16)
    escl_new_element_text(modes, "scan:ColorMode", "Grayscale16");
  if (data->color_supported & PAPPL_SCAN_COLOR_MODE_RGB_24)
    escl_new_element_text(modes, "scan:ColorMode", "RGB24");
  if (data->color_supported & PAPPL_SCAN_COLOR_MODE_RGB_48)
    escl_new_element_text(modes, "scan:ColorMode", "RGB48");

  // Document formats (both old and new style per eSCL 2.1+)...
  formats = mxmlNewElement(profile, "scan:DocumentFormats");
  for (i = 0; i < data->num_format; i ++)
  {
    escl_new_element_text(formats, "pwg:DocumentFormat", data->format[i]);
    escl_new_element_text(formats, "scan:DocumentFormatExt", data->format[i]);
  }

  // Resolutions...
  resolutions = mxmlNewElement(profile, "scan:SupportedResolutions");
  discrete    = mxmlNewElement(resolutions, "scan:DiscreteResolutions");

  for (i = 0; i < data->num_resolution; i ++)
  {
    res_node = mxmlNewElement(discrete, "scan:DiscreteResolution");
    escl_new_element_int(res_node, "scan:XResolution", data->x_resolution[i]);
    escl_new_element_int(res_node, "scan:YResolution", data->y_resolution[i]);
  }

  // Color spaces...
  mxml_node_t *cs_node = mxmlNewElement(profile, "scan:ColorSpaces");
  mxml_node_t *cs_el   = mxmlNewElement(cs_node, "scan:ColorSpace");
  mxmlNewOpaque(cs_el, "sRGB");

  // Supported intents for this input source...
  mxml_node_t *intents_node = mxmlNewElement(caps, "scan:SupportedIntents");
  if (data->intents_supported & PAPPL_SCAN_INTENT_DOCUMENT)
    escl_new_element_text(intents_node, "scan:Intent", "Document");
  if (data->intents_supported & PAPPL_SCAN_INTENT_TEXT_AND_GRAPHIC)
    escl_new_element_text(intents_node, "scan:Intent", "TextAndGraphic");
  if (data->intents_supported & PAPPL_SCAN_INTENT_PHOTO)
    escl_new_element_text(intents_node, "scan:Intent", "Photo");
  if (data->intents_supported & PAPPL_SCAN_INTENT_PREVIEW)
    escl_new_element_text(intents_node, "scan:Intent", "Preview");
}


//
// String conversion helpers...
//

static const char *
escl_color_mode_string(
    pappl_scan_color_mode_t mode)
{
  switch (mode)
  {
    case PAPPL_SCAN_COLOR_MODE_BLACK_AND_WHITE_1 :
        return ("BlackAndWhite1");
    case PAPPL_SCAN_COLOR_MODE_GRAYSCALE_8 :
        return ("Grayscale8");
    case PAPPL_SCAN_COLOR_MODE_GRAYSCALE_16 :
        return ("Grayscale16");
    case PAPPL_SCAN_COLOR_MODE_RGB_24 :
        return ("RGB24");
    case PAPPL_SCAN_COLOR_MODE_RGB_48 :
        return ("RGB48");
    default :
        return ("RGB24");
  }
}

static pappl_scan_color_mode_t
escl_color_mode_value(
    const char *s)
{
  if (!strcasecmp(s, "BlackAndWhite1"))
    return (PAPPL_SCAN_COLOR_MODE_BLACK_AND_WHITE_1);
  else if (!strcasecmp(s, "Grayscale8"))
    return (PAPPL_SCAN_COLOR_MODE_GRAYSCALE_8);
  else if (!strcasecmp(s, "Grayscale16"))
    return (PAPPL_SCAN_COLOR_MODE_GRAYSCALE_16);
  else if (!strcasecmp(s, "RGB24"))
    return (PAPPL_SCAN_COLOR_MODE_RGB_24);
  else if (!strcasecmp(s, "RGB48"))
    return (PAPPL_SCAN_COLOR_MODE_RGB_48);
  else
    return (PAPPL_SCAN_COLOR_MODE_RGB_24);
}

static const char *
escl_input_source_string(
    pappl_scan_input_source_t src)
{
  switch (src)
  {
    case PAPPL_SCAN_INPUT_SOURCE_PLATEN :
        return ("Platen");
    case PAPPL_SCAN_INPUT_SOURCE_ADF :
        return ("Feeder");
    case PAPPL_SCAN_INPUT_SOURCE_CAMERA :
        return ("Camera");
    default :
        return ("Platen");
  }
}

static pappl_scan_input_source_t
escl_input_source_value(
    const char *s)
{
  if (!strcasecmp(s, "Platen"))
    return (PAPPL_SCAN_INPUT_SOURCE_PLATEN);
  else if (!strcasecmp(s, "Feeder") || !strcasecmp(s, "Adf") ||
           !strcasecmp(s, "ADF"))
    return (PAPPL_SCAN_INPUT_SOURCE_ADF);
  else if (!strcasecmp(s, "Camera"))
    return (PAPPL_SCAN_INPUT_SOURCE_CAMERA);
  else
    return (PAPPL_SCAN_INPUT_SOURCE_PLATEN);
}

static const char *
escl_intent_string(
    pappl_scan_intent_t intent)
{
  switch (intent)
  {
    case PAPPL_SCAN_INTENT_DOCUMENT :
        return ("Document");
    case PAPPL_SCAN_INTENT_TEXT_AND_GRAPHIC :
        return ("TextAndGraphic");
    case PAPPL_SCAN_INTENT_PHOTO :
        return ("Photo");
    case PAPPL_SCAN_INTENT_PREVIEW :
        return ("Preview");
    case PAPPL_SCAN_INTENT_BUSINESS_CARD :
        return ("BusinessCard");
    default :
        return ("Document");
  }
}

static pappl_scan_intent_t
escl_intent_value(
    const char *s)
{
  if (!strcasecmp(s, "Document"))
    return (PAPPL_SCAN_INTENT_DOCUMENT);
  else if (!strcasecmp(s, "TextAndGraphic"))
    return (PAPPL_SCAN_INTENT_TEXT_AND_GRAPHIC);
  else if (!strcasecmp(s, "Photo"))
    return (PAPPL_SCAN_INTENT_PHOTO);
  else if (!strcasecmp(s, "Preview"))
    return (PAPPL_SCAN_INTENT_PREVIEW);
  else if (!strcasecmp(s, "BusinessCard"))
    return (PAPPL_SCAN_INTENT_BUSINESS_CARD);
  else
    return (PAPPL_SCAN_INTENT_DOCUMENT);
}

static const char *
escl_content_type_string(
    pappl_scan_content_t ct)
{
  switch (ct)
  {
    case PAPPL_SCAN_CONTENT_AUTO :
        return ("Auto");
    case PAPPL_SCAN_CONTENT_HALFTONE :
        return ("Halftone");
    case PAPPL_SCAN_CONTENT_LINE_ART :
        return ("LineArt");
    case PAPPL_SCAN_CONTENT_MAGAZINE :
        return ("Magazine");
    case PAPPL_SCAN_CONTENT_PHOTO :
        return ("Photo");
    case PAPPL_SCAN_CONTENT_TEXT :
        return ("Text");
    case PAPPL_SCAN_CONTENT_TEXT_AND_PHOTO :
        return ("TextAndPhoto");
    default :
        return ("Auto");
  }
}

static pappl_scan_content_t
escl_content_type_value(
    const char *s)
{
  if (!strcasecmp(s, "Auto"))
    return (PAPPL_SCAN_CONTENT_AUTO);
  else if (!strcasecmp(s, "Halftone"))
    return (PAPPL_SCAN_CONTENT_HALFTONE);
  else if (!strcasecmp(s, "LineArt"))
    return (PAPPL_SCAN_CONTENT_LINE_ART);
  else if (!strcasecmp(s, "Magazine"))
    return (PAPPL_SCAN_CONTENT_MAGAZINE);
  else if (!strcasecmp(s, "Photo"))
    return (PAPPL_SCAN_CONTENT_PHOTO);
  else if (!strcasecmp(s, "Text"))
    return (PAPPL_SCAN_CONTENT_TEXT);
  else if (!strcasecmp(s, "TextAndPhoto"))
    return (PAPPL_SCAN_CONTENT_TEXT_AND_PHOTO);
  else
    return (PAPPL_SCAN_CONTENT_AUTO);
}

static const char *
escl_job_state_string(
    ipp_jstate_t state)
{
  switch (state)
  {
    case IPP_JSTATE_PENDING :
    case IPP_JSTATE_HELD :
        return ("Pending");
    case IPP_JSTATE_PROCESSING :
    case IPP_JSTATE_STOPPED :
        return ("Processing");
    case IPP_JSTATE_COMPLETED :
        return ("Completed");
    case IPP_JSTATE_CANCELED :
        return ("Canceled");
    case IPP_JSTATE_ABORTED :
        return ("Aborted");
    default :
        return ("Pending");
  }
}

static const char *
escl_job_state_reason_string(
    ipp_jstate_t state)
{
  switch (state)
  {
    case IPP_JSTATE_PENDING :
    case IPP_JSTATE_HELD :
        return ("JobQueued");
    case IPP_JSTATE_PROCESSING :
    case IPP_JSTATE_STOPPED :
        return ("JobScanning");
    case IPP_JSTATE_COMPLETED :
        return ("JobCompletedSuccessfully");
    case IPP_JSTATE_CANCELED :
        return ("JobCanceledByUser");
    case IPP_JSTATE_ABORTED :
        return ("JobAbortedBySystem");
    default :
        return ("JobQueued");
  }
}

static const char *
escl_scanner_state_string(
    ipp_pstate_t state)
{
  switch (state)
  {
    case IPP_PSTATE_IDLE :
        return ("Idle");
    case IPP_PSTATE_PROCESSING :
        return ("Processing");
    case IPP_PSTATE_STOPPED :
        return ("Stopped");
    default :
        return ("Idle");
  }
}


//
// XML helper functions...
//

static mxml_node_t *			// O - New element node
escl_new_element_text(
    mxml_node_t *parent,		// I - Parent node
    const char  *name,			// I - Element name
    const char  *value)			// I - Text value
{
  mxml_node_t *node = mxmlNewElement(parent, name);

  if (value)
    mxmlNewOpaque(node, value);

  return (node);
}

static mxml_node_t *			// O - New element node
escl_new_element_int(
    mxml_node_t *parent,		// I - Parent node
    const char  *name,			// I - Element name
    int         value)			// I - Integer value
{
  mxml_node_t	*node = mxmlNewElement(parent, name);
  char		buf[32];		// String buffer

  snprintf(buf, sizeof(buf), "%d", value);
  mxmlNewOpaque(node, buf);

  return (node);
}

static char *				// O - Allocated string (caller frees)
escl_xml_to_string(
    mxml_node_t *xml,			// I - XML document
    size_t      *length)		// O - String length
{
  mxml_options_t *opts;			// Save options
  char		*str;			// Result string


  opts = mxmlOptionsNew();
  mxmlOptionsSetWrapMargin(opts, 0);
  str = mxmlSaveAllocString(xml, opts);
  mxmlOptionsDelete(opts);

  if (str && length)
    *length = strlen(str);

  return (str);
}

static mxml_type_t			// O - Type
escl_type_cb(
    void        *cbdata,		// I - Callback data (unused)
    mxml_node_t *node)			// I - Current node (unused)
{
  (void)cbdata;
  (void)node;

  return (MXML_TYPE_OPAQUE);
}
