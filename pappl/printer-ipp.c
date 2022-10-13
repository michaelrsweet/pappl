//
// Printer IPP processing for the Printer Application Framework
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
  cups_len_t	max_count;		// Max number of values
} _pappl_attr_t;


//
// Local functions...
//

static pappl_job_t	*create_job(pappl_client_t *client);

static void		ipp_cancel_current_job(pappl_client_t *client);
static void		ipp_cancel_jobs(pappl_client_t *client);
static void		ipp_create_job(pappl_client_t *client);
static void		ipp_disable_printer(pappl_client_t *client);
static void		ipp_enable_printer(pappl_client_t *client);
static void		ipp_get_jobs(pappl_client_t *client);
static void		ipp_get_printer_attributes(pappl_client_t *client);
static void		ipp_identify_printer(pappl_client_t *client);
static void		ipp_pause_printer(pappl_client_t *client);
static void		ipp_print_job(pappl_client_t *client);
static void		ipp_resume_printer(pappl_client_t *client);
static void		ipp_set_printer_attributes(pappl_client_t *client);
static void		ipp_validate_job(pappl_client_t *client);

static bool		valid_job_attributes(pappl_client_t *client);


//
// '_papplPrinterCopyAttributes()' - Copy printer attributes to a response...
//

void
_papplPrinterCopyAttributes(
    pappl_printer_t *printer,		// I - Printer
    pappl_client_t  *client,		// I - Client
    cups_array_t    *ra,		// I - Requested attributes
    const char      *format)		// I - "document-format" value, if any
{
  cups_len_t	i,			// Looping var
		num_values;		// Number of values
  unsigned	bit;			// Current bit value
  const char	*svalues[100];		// String values
  int		ivalues[100];		// Integer values
  pappl_pr_driver_data_t *data = &printer->driver_data;
					// Driver data
  const char	*webscheme = (httpAddrIsLocalhost(httpGetAddress(client->http)) || !papplSystemGetTLSOnly(client->system)) ? "http" : "https";
					// URL scheme for resources


  _papplCopyAttributes(client->response, printer->attrs, ra, IPP_TAG_ZERO, IPP_TAG_CUPS_CONST);
  _papplCopyAttributes(client->response, printer->driver_attrs, ra, IPP_TAG_ZERO, IPP_TAG_CUPS_CONST);
  _papplPrinterCopyState(printer, IPP_TAG_PRINTER, client->response, client, ra);

  if (!ra || cupsArrayFind(ra, "copies-supported"))
  {
    // Filter copies-supported value based on the document format...
    // (no copy support for streaming raster formats)
    if (format && (!strcmp(format, "image/pwg-raster") || !strcmp(format, "image/urf")))
      ippAddRange(client->response, IPP_TAG_PRINTER, "copies-supported", 1, 1);
    else
      ippAddRange(client->response, IPP_TAG_PRINTER, "copies-supported", 1, 999);
  }

  if (!ra || cupsArrayFind(ra, "identify-actions-default"))
  {
    for (num_values = 0, bit = PAPPL_IDENTIFY_ACTIONS_DISPLAY; bit <= PAPPL_IDENTIFY_ACTIONS_SPEAK; bit *= 2)
    {
      if (data->identify_default & bit)
	svalues[num_values ++] = _papplIdentifyActionsString(bit);
    }

    if (num_values > 0)
      ippAddStrings(client->response, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "identify-actions-default", IPP_NUM_CAST num_values, NULL, svalues);
    else
      ippAddString(client->response, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "identify-actions-default", NULL, "none");
  }

  if (!ra || cupsArrayFind(ra, "job-spooling-supported"))
    ippAddString(client->response, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "job-spooling-supported", NULL, printer->max_active_jobs == 1 ? "stream" : "spool");

  if ((!ra || cupsArrayFind(ra, "label-mode-configured")) && data->mode_configured)
    ippAddString(client->response, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "label-mode-configured", NULL, _papplLabelModeString(data->mode_configured));

  if ((!ra || cupsArrayFind(ra, "label-tear-offset-configured")) && data->tear_offset_supported[1] > 0)
    ippAddInteger(client->response, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "label-tear-offset-configured", data->tear_offset_configured);

  if (printer->num_supply > 0)
  {
    pappl_supply_t *supply = printer->supply;
					// Supply values...

    if (!ra || cupsArrayFind(ra, "marker-colors"))
    {
      for (i = 0; i < (cups_len_t)printer->num_supply; i ++)
        svalues[i] = _papplMarkerColorString(supply[i].color);

      ippAddStrings(client->response, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_NAME), "marker-colors", IPP_NUM_CAST printer->num_supply, NULL, svalues);
    }

    if (!ra || cupsArrayFind(ra, "marker-high-levels"))
    {
      for (i = 0; i < (cups_len_t)printer->num_supply; i ++)
        ivalues[i] = supply[i].is_consumed ? 100 : 90;

      ippAddIntegers(client->response, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "marker-high-levels", IPP_NUM_CAST printer->num_supply, ivalues);
    }

    if (!ra || cupsArrayFind(ra, "marker-levels"))
    {
      for (i = 0; i < (cups_len_t)printer->num_supply; i ++)
        ivalues[i] = supply[i].level;

      ippAddIntegers(client->response, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "marker-levels", IPP_NUM_CAST printer->num_supply, ivalues);
    }

    if (!ra || cupsArrayFind(ra, "marker-low-levels"))
    {
      for (i = 0; i < (cups_len_t)printer->num_supply; i ++)
        ivalues[i] = supply[i].is_consumed ? 10 : 0;

      ippAddIntegers(client->response, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "marker-low-levels", IPP_NUM_CAST printer->num_supply, ivalues);
    }

    if (!ra || cupsArrayFind(ra, "marker-names"))
    {
      for (i = 0; i < (cups_len_t)printer->num_supply; i ++)
        svalues[i] = supply[i].description;

      ippAddStrings(client->response, IPP_TAG_PRINTER, IPP_TAG_NAME, "marker-names", IPP_NUM_CAST printer->num_supply, NULL, svalues);
    }

    if (!ra || cupsArrayFind(ra, "marker-types"))
    {
      for (i = 0; i < (cups_len_t)printer->num_supply; i ++)
        svalues[i] = _papplMarkerTypeString(supply[i].type);

      ippAddStrings(client->response, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "marker-types", IPP_NUM_CAST printer->num_supply, NULL, svalues);
    }
  }

  if ((!ra || cupsArrayFind(ra, "media-col-default")) && data->media_default.size_name[0])
  {
    ipp_t *col = _papplMediaColExport(&printer->driver_data, &data->media_default, 0);
					// Collection value

    ippAddCollection(client->response, IPP_TAG_PRINTER, "media-col-default", col);
    ippDelete(col);
  }

  if (!ra || cupsArrayFind(ra, "media-col-ready"))
  {
    cups_len_t		j,		// Looping var
			count;		// Number of values
    ipp_t		*col;		// Collection value
    ipp_attribute_t	*attr;		// media-col-ready attribute
    pappl_media_col_t	media;		// Current media...

    for (i = 0, count = 0; i < (cups_len_t)printer->num_ready; i ++)
    {
      if (data->media_ready[i].size_name[0])
        count ++;
    }

    if (data->borderless && (data->bottom_top != 0 || data->left_right != 0))
      count *= 2;			// Need to report ready media for borderless, too...

    if (count > 0)
    {
      attr = ippAddCollections(client->response, IPP_TAG_PRINTER, "media-col-ready", IPP_NUM_CAST count, NULL);

      for (i = 0, j = 0; i < (cups_len_t)printer->num_ready && j < count; i ++)
      {
	if (data->media_ready[i].size_name[0])
	{
          if (data->borderless && (data->bottom_top != 0 || data->left_right != 0))
	  {
	    // Report both bordered and borderless media-col values...
	    media = data->media_ready[i];

	    media.bottom_margin = media.top_margin   = data->bottom_top;
	    media.left_margin   = media.right_margin = data->left_right;
	    col = _papplMediaColExport(&printer->driver_data, &media, 0);
	    ippSetCollection(client->response, &attr, IPP_NUM_CAST j ++, col);
	    ippDelete(col);

	    media.bottom_margin = media.top_margin   = 0;
	    media.left_margin   = media.right_margin = 0;
	    col = _papplMediaColExport(&printer->driver_data, &media, 0);
	    ippSetCollection(client->response, &attr, IPP_NUM_CAST j ++, col);
	    ippDelete(col);
	  }
	  else
	  {
	    // Just report the single media-col value...
	    col = _papplMediaColExport(&printer->driver_data, data->media_ready + i, 0);
	    ippSetCollection(client->response, &attr, IPP_NUM_CAST j ++, col);
	    ippDelete(col);
	  }
	}
      }
    }
  }

  if ((!ra || cupsArrayFind(ra, "media-default")) && data->media_default.size_name[0])
    ippAddString(client->response, IPP_TAG_PRINTER, IPP_TAG_KEYWORD, "media-default", NULL, data->media_default.size_name);

  if (!ra || cupsArrayFind(ra, "media-ready"))
  {
    cups_len_t		j,		// Looping vars
			count;		// Number of values
    ipp_attribute_t	*attr;		// media-col-ready attribute

    for (i = 0, count = 0; i < (cups_len_t)printer->num_ready; i ++)
    {
      if (data->media_ready[i].size_name[0])
        count ++;
    }

    if (count > 0)
    {
      attr = ippAddStrings(client->response, IPP_TAG_PRINTER, IPP_TAG_KEYWORD, "media-ready", IPP_NUM_CAST count, NULL, NULL);

      for (i = 0, j = 0; i < (cups_len_t)printer->num_ready && j < count; i ++)
      {
	if (data->media_ready[i].size_name[0])
	  ippSetString(client->response, &attr, IPP_NUM_CAST j ++, data->media_ready[i].size_name);
      }
    }
  }

  if (!ra || cupsArrayFind(ra, "multiple-document-handling-default"))
    ippAddString(client->response, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "multiple-document-handling-default", NULL, "separate-documents-collated-copies");

  if (!ra || cupsArrayFind(ra, "orientation-requested-default"))
    ippAddInteger(client->response, IPP_TAG_PRINTER, IPP_TAG_ENUM, "orientation-requested-default", (int)data->orient_default);

  if (!ra || cupsArrayFind(ra, "output-bin-default"))
  {
    if (data->num_bin > 0)
      ippAddString(client->response, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "output-bin-default", NULL, data->bin[data->bin_default]);
    else if (data->output_face_up)
      ippAddString(client->response, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "output-bin-default", NULL, "face-up");
    else
      ippAddString(client->response, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "output-bin-default", NULL, "face-down");
  }

  if ((!ra || cupsArrayFind(ra, "print-color-mode-default")) && data->color_default)
    ippAddString(client->response, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "print-color-mode-default", NULL, _papplColorModeString(data->color_default));

  if (!ra || cupsArrayFind(ra, "print-content-optimize-default"))
  {
    if (data->content_default)
      ippAddString(client->response, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "print-content-optimize-default", NULL, _papplContentString(data->content_default));
    else
      ippAddString(client->response, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "print-content-optimize-default", NULL, "auto");
  }

  if (!ra || cupsArrayFind(ra, "print-quality-default"))
  {
    if (data->quality_default)
      ippAddInteger(client->response, IPP_TAG_PRINTER, IPP_TAG_ENUM, "print-quality-default", (int)data->quality_default);
    else
      ippAddInteger(client->response, IPP_TAG_PRINTER, IPP_TAG_ENUM, "print-quality-default", IPP_QUALITY_NORMAL);
  }

  if (!ra || cupsArrayFind(ra, "print-scaling-default"))
  {
    if (data->scaling_default)
      ippAddString(client->response, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "print-scaling-default", NULL, _papplScalingString(data->scaling_default));
    else
      ippAddString(client->response, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "print-scaling-default", NULL, "auto");
  }

  if (!ra || cupsArrayFind(ra, "printer-config-change-date-time"))
    ippAddDate(client->response, IPP_TAG_PRINTER, "printer-config-change-date-time", ippTimeToDate(printer->config_time));

  if (!ra || cupsArrayFind(ra, "printer-config-change-time"))
  {
    if (printer->config_time > printer->start_time)
      ippAddInteger(client->response, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "printer-config-change-time", (int)(printer->config_time - printer->start_time));
    else
      ippAddInteger(client->response, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "printer-config-change-time", 1);
  }

  if (!ra || cupsArrayFind(ra, "printer-contact-col"))
  {
    ipp_t *col = _papplContactExport(&printer->contact);
    ippAddCollection(client->response, IPP_TAG_PRINTER, "printer-contact-col", col);
    ippDelete(col);
  }

  if (!ra || cupsArrayFind(ra, "printer-current-time"))
    ippAddDate(client->response, IPP_TAG_PRINTER, "printer-current-time", ippTimeToDate(time(NULL)));

  if ((!ra || cupsArrayFind(ra, "printer-darkness-configured")) && data->darkness_supported > 0)
    ippAddInteger(client->response, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "printer-darkness-configured", data->darkness_configured);

  if (!ra || cupsArrayFind(ra, "printer-dns-sd-name"))
    ippAddString(client->response, IPP_TAG_PRINTER, IPP_TAG_NAME, "printer-dns-sd-name", NULL, printer->dns_sd_name ? printer->dns_sd_name : "");

  pthread_rwlock_rdlock(&client->system->rwlock);
  _papplSystemExportVersions(client->system, client->response, IPP_TAG_PRINTER, ra);
  pthread_rwlock_unlock(&client->system->rwlock);

  if (!ra || cupsArrayFind(ra, "printer-geo-location"))
  {
    if (printer->geo_location)
      ippAddString(client->response, IPP_TAG_PRINTER, IPP_TAG_URI, "printer-geo-location", NULL, printer->geo_location);
    else
      ippAddOutOfBand(client->response, IPP_TAG_PRINTER, IPP_TAG_UNKNOWN, "printer-geo-location");
  }

  if (!ra || cupsArrayFind(ra, "printer-icons"))
  {
    char	uris[3][1024];		// Buffers for URIs
    const char	*values[3];		// Values for attribute

    httpAssembleURIf(HTTP_URI_CODING_ALL, uris[0], sizeof(uris[0]), webscheme, NULL, client->host_field, client->host_port, "%s/icon-sm.png", printer->uriname);
    httpAssembleURIf(HTTP_URI_CODING_ALL, uris[1], sizeof(uris[1]), webscheme, NULL, client->host_field, client->host_port, "%s/icon-md.png", printer->uriname);
    httpAssembleURIf(HTTP_URI_CODING_ALL, uris[2], sizeof(uris[2]), webscheme, NULL, client->host_field, client->host_port, "%s/icon-lg.png", printer->uriname);

    values[0] = uris[0];
    values[1] = uris[1];
    values[2] = uris[2];

    ippAddStrings(client->response, IPP_TAG_PRINTER, IPP_TAG_URI, "printer-icons", 3, NULL, values);
  }

  if (!ra || cupsArrayFind(ra, "printer-impressions-completed"))
    ippAddInteger(client->response, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "printer-impressions-completed", printer->impcompleted);

  if (!ra || cupsArrayFind(ra, "printer-input-tray"))
  {
    ipp_attribute_t	*attr = NULL;	// "printer-input-tray" attribute
    char		value[256];	// Value for current tray
    pappl_media_col_t	*media;		// Media in the tray

    for (i = 0, media = data->media_ready; i < (cups_len_t)data->num_source; i ++, media ++)
    {
      const char	*type;		// Tray type

      if (!strcmp(data->source[i], "manual"))
        type = "sheetFeedManual";
      else if (!strcmp(data->source[i], "by-pass-tray"))
        type = "sheetFeedAutoNonRemovableTray";
      else
        type = "sheetFeedAutoRemovableTray";

      snprintf(value, sizeof(value), "type=%s;mediafeed=%d;mediaxfeed=%d;maxcapacity=%d;level=-2;status=0;name=%s;", type, media->size_length, media->size_width, !strcmp(media->source, "manual") ? 1 : -2, media->source);

      if (attr)
        ippSetOctetString(client->response, &attr, ippGetCount(attr), value, IPP_NUM_CAST strlen(value));
      else
        attr = ippAddOctetString(client->response, IPP_TAG_PRINTER, "printer-input-tray", value, IPP_NUM_CAST strlen(value));
    }

    // The "auto" tray is a dummy entry...
    papplCopyString(value, "type=other;mediafeed=0;mediaxfeed=0;maxcapacity=-2;level=-2;status=0;name=auto;", sizeof(value));
    ippSetOctetString(client->response, &attr, ippGetCount(attr), value, IPP_NUM_CAST strlen(value));
  }

  if (!ra || cupsArrayFind(ra, "printer-location"))
    ippAddString(client->response, IPP_TAG_PRINTER, IPP_TAG_TEXT, "printer-location", NULL, printer->location ? printer->location : "");

  if (!ra || cupsArrayFind(ra, "printer-more-info"))
  {
    char	uri[1024];		// URI value

    httpAssembleURIf(HTTP_URI_CODING_ALL, uri, sizeof(uri), webscheme, NULL, client->host_field, client->host_port, "%s/", printer->uriname);
    ippAddString(client->response, IPP_TAG_PRINTER, IPP_TAG_URI, "printer-more-info", NULL, uri);
  }

  if (!ra || cupsArrayFind(ra, "printer-organization"))
    ippAddString(client->response, IPP_TAG_PRINTER, IPP_TAG_TEXT, "printer-organization", NULL, printer->organization ? printer->organization : "");

  if (!ra || cupsArrayFind(ra, "printer-organizational-unit"))
    ippAddString(client->response, IPP_TAG_PRINTER, IPP_TAG_TEXT, "printer-organizational-unit", NULL, printer->org_unit ? printer->org_unit : "");

  if (!ra || cupsArrayFind(ra, "printer-resolution-default"))
    ippAddResolution(client->response, IPP_TAG_PRINTER, "printer-resolution-default", IPP_RES_PER_INCH, data->x_default, data->y_default);

  if (!ra || cupsArrayFind(ra, "printer-speed-default"))
    ippAddInteger(client->response, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "printer-speed-default", data->speed_default);

  if (!ra || cupsArrayFind(ra, "printer-state-change-date-time"))
    ippAddDate(client->response, IPP_TAG_PRINTER, "printer-state-change-date-time", ippTimeToDate(printer->state_time));

  if (!ra || cupsArrayFind(ra, "printer-state-change-time"))
    ippAddInteger(client->response, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "printer-state-change-time", (int)(printer->state_time - printer->start_time));

  if (!ra || cupsArrayFind(ra, "printer-strings-languages-supported"))
  {
    _pappl_resource_t	*r;		// Current resource
    cups_len_t		rcount;		// Number of resources

    pthread_rwlock_rdlock(&printer->system->rwlock);

    // Cannot use cupsArrayGetFirst/Last since other threads might be iterating
    // this array...
    for (i = 0, num_values = 0, rcount = cupsArrayGetCount(printer->system->resources); i < rcount && num_values < (cups_len_t)(sizeof(svalues) / sizeof(svalues[0])); i ++)
    {
      r = (_pappl_resource_t *)cupsArrayGetElement(printer->system->resources, (size_t)i);

      if (r->language)
        svalues[num_values ++] = r->language;
    }
    pthread_rwlock_unlock(&printer->system->rwlock);

    if (num_values > 0)
      ippAddStrings(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_LANGUAGE, "printer-strings-languages-supported", IPP_NUM_CAST num_values, NULL, svalues);
  }

  if (!ra || cupsArrayFind(ra, "printer-strings-uri"))
  {
    const char	*lang = ippGetString(ippFindAttribute(client->request, "attributes-natural-language", IPP_TAG_LANGUAGE), 0, NULL);
					// Language
    char	baselang[3],		// Base language
		uri[1024];		// Strings file URI
    _pappl_resource_t	*r;		// Current resource
    cups_len_t	rcount;			// Number of resources

    papplCopyString(baselang, lang, sizeof(baselang));

    pthread_rwlock_rdlock(&printer->system->rwlock);

    // Cannot use cupsArrayGetFirst/Last since other threads might be iterating
    // this array...
    for (i = 0, rcount = cupsArrayGetCount(printer->system->resources); i < rcount; i ++)
    {
      r = (_pappl_resource_t *)cupsArrayGetElement(printer->system->resources, i);

      if (r->language && (!strcmp(r->language, lang) || !strcmp(r->language, baselang)))
      {
        httpAssembleURI(HTTP_URI_CODING_ALL, uri, sizeof(uri), webscheme, NULL, client->host_field, client->host_port, r->path);
        ippAddString(client->response, IPP_TAG_PRINTER, IPP_TAG_URI, "printer-strings-uri", NULL, uri);
        break;
      }
    }

    pthread_rwlock_unlock(&printer->system->rwlock);
  }

  if (printer->num_supply > 0)
  {
    pappl_supply_t	 *supply = printer->supply;
					// Supply values...

    if (!ra || cupsArrayFind(ra, "printer-supply"))
    {
      char		value[256];	// "printer-supply" value
      ipp_attribute_t	*attr = NULL;	// "printer-supply" attribute

      for (i = 0; i < (cups_len_t)printer->num_supply; i ++)
      {
	snprintf(value, sizeof(value), "index=%u;type=%s;maxcapacity=100;level=%d;colorantname=%s;", (unsigned)i, _papplSupplyTypeString(supply[i].type), supply[i].level, _papplSupplyColorString(supply[i].color));

	if (attr)
	  ippSetOctetString(client->response, &attr, ippGetCount(attr), value, IPP_NUM_CAST strlen(value));
	else
	  attr = ippAddOctetString(client->response, IPP_TAG_PRINTER, "printer-supply", value, IPP_NUM_CAST strlen(value));
      }
    }

    if (!ra || cupsArrayFind(ra, "printer-supply-description"))
    {
      for (i = 0; i < (cups_len_t)printer->num_supply; i ++)
        svalues[i] = supply[i].description;

      ippAddStrings(client->response, IPP_TAG_PRINTER, IPP_TAG_TEXT, "printer-supply-description", IPP_NUM_CAST printer->num_supply, NULL, svalues);
    }
  }

  if (!ra || cupsArrayFind(ra, "printer-supply-info-uri"))
  {
    char	uri[1024];		// URI value

    httpAssembleURIf(HTTP_URI_CODING_ALL, uri, sizeof(uri), webscheme, NULL, client->host_field, client->host_port, "%s/supplies", printer->uriname);
    ippAddString(client->response, IPP_TAG_PRINTER, IPP_TAG_URI, "printer-supply-info-uri", NULL, uri);
  }

  if (!ra || cupsArrayFind(ra, "printer-up-time"))
    ippAddInteger(client->response, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "printer-up-time", (int)(time(NULL) - printer->start_time));

  if (!ra || cupsArrayFind(ra, "printer-uri-supported"))
  {
    char	uris[2][1024];		// Buffers for URIs
    const char	*values[2];		// Values for attribute

    num_values = 0;

    if (httpAddrIsLocalhost(httpGetAddress(client->http)) || !papplSystemGetTLSOnly(client->system))
    {
      httpAssembleURI(HTTP_URI_CODING_ALL, uris[num_values], sizeof(uris[0]), "ipp", NULL, client->host_field, client->host_port, printer->resource);
      values[num_values] = uris[num_values];
      num_values ++;
    }

    if (!httpAddrIsLocalhost(httpGetAddress(client->http)) && !(client->system->options & PAPPL_SOPTIONS_NO_TLS))
    {
      httpAssembleURI(HTTP_URI_CODING_ALL, uris[num_values], sizeof(uris[0]), "ipps", NULL, client->host_field, client->host_port, printer->resource);
      values[num_values] = uris[num_values];
      num_values ++;
    }

    if (num_values > 0)
      ippAddStrings(client->response, IPP_TAG_PRINTER, IPP_TAG_URI, "printer-uri-supported", IPP_NUM_CAST num_values, NULL, values);
  }

  if (client->system->wifi_status_cb && httpAddrIsLocalhost(httpGetAddress(client->http)) && (!ra || cupsArrayFind(ra, "printer-wifi-ssid") || cupsArrayFind(ra, "printer-wifi-state")))
  {
    // Get Wi-Fi status...
    pappl_wifi_t	wifi;		// Wi-Fi status

    if ((client->system->wifi_status_cb)(client->system, client->system->wifi_cbdata, &wifi))
    {
      if (!ra || cupsArrayFind(ra, "printer-wifi-ssid"))
        ippAddString(client->response, IPP_TAG_PRINTER, IPP_TAG_NAME, "printer-wifi-ssid", NULL, wifi.ssid);

      if (!ra || cupsArrayFind(ra, "printer-wifi-state"))
        ippAddInteger(client->response, IPP_TAG_PRINTER, IPP_TAG_ENUM, "printer-wifi-state", (int)wifi.state);
    }
  }

  if (!ra || cupsArrayFind(ra, "printer-xri-supported"))
    _papplPrinterCopyXRI(printer, client->response, client);

  if (!ra || cupsArrayFind(ra, "queued-job-count"))
    ippAddInteger(client->response, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "queued-job-count", (int)cupsArrayGetCount(printer->active_jobs));

  if (!ra || cupsArrayFind(ra, "sides-default"))
  {
    if (data->sides_default)
      ippAddString(client->response, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "sides-default", NULL, _papplSidesString(data->sides_default));
    else
      ippAddString(client->response, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "sides-default", NULL, "one-sided");
  }

  if (!ra || cupsArrayFind(ra, "uri-authentication-supported"))
  {
    // For each supported printer-uri value, report whether authentication is
    // supported.  Since we only support authentication over a secure (TLS)
    // channel, the value is always 'none' for the "ipp" URI and either 'none'
    // or 'basic' for the "ipps" URI...
    if (httpAddrIsLocalhost(httpGetAddress(client->http)) || (client->system->options & PAPPL_SOPTIONS_NO_TLS))
    {
      ippAddString(client->response, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "uri-authentication-supported", NULL, "none");
    }
    else if (papplSystemGetTLSOnly(client->system))
    {
      if (papplSystemGetAuthService(client->system))
        ippAddString(client->response, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "uri-authentication-supported", NULL, "basic");
      else
        ippAddString(client->response, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "uri-authentication-supported", NULL, "none");
    }
    else if (papplSystemGetAuthService(client->system))
    {
      static const char * const uri_authentication_basic[] =
      {					// uri-authentication-supported values
	"none",
	"basic"
      };

      ippAddStrings(client->response, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "uri-authentication-supported", 2, NULL, uri_authentication_basic);
    }
    else
    {
      static const char * const uri_authentication_none[] =
      {					// uri-authentication-supported values
	"none",
	"none"
      };

      ippAddStrings(client->response, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "uri-authentication-supported", 2, NULL, uri_authentication_none);
    }
  }
}


//
// '_papplPrinterCopyState()' - Copy the printer-state-xxx attributes.
//

void
_papplPrinterCopyState(
    pappl_printer_t *printer,		// I - Printer
    ipp_tag_t       group_tag,		// I - Group tag
    ipp_t           *ipp,		// I - IPP message
    pappl_client_t  *client,		// I - Client connection
    cups_array_t    *ra)		// I - Requested attributes
{
  if (!ra || cupsArrayFind(ra, "printer-is-accepting-jobs"))
    ippAddBoolean(ipp, group_tag, "printer-is-accepting-jobs", printer->is_accepting);

  if (!ra || cupsArrayFind(ra, "printer-state"))
    ippAddInteger(ipp, group_tag, IPP_TAG_ENUM, "printer-state", (int)printer->state);

  if (!ra || cupsArrayFind(ra, "printer-state-message"))
  {
    static const char * const messages[] = { "Idle.", "Printing.", "Stopped." };

    ippAddString(ipp, group_tag, IPP_CONST_TAG(IPP_TAG_TEXT), "printer-state-message", NULL, messages[printer->state - IPP_PSTATE_IDLE]);
  }

  if (!ra || cupsArrayFind(ra, "printer-state-reasons"))
  {
    ipp_attribute_t	*attr = NULL;	// printer-state-reasons
    bool		wifi_not_configured = false;
					// Need the 'wifi-not-configured' reason?

    if (client && client->system->wifi_status_cb && httpAddrIsLocalhost(httpGetAddress(client->http)))
    {
      pappl_wifi_t	wifi;		// Wi-Fi status

      if ((client->system->wifi_status_cb)(client->system, client->system->wifi_cbdata, &wifi))
      {
        if (wifi.state == PAPPL_WIFI_STATE_NOT_CONFIGURED)
          wifi_not_configured = true;
      }
    }

    if (printer->state_reasons == PAPPL_PREASON_NONE)
    {
      if (printer->is_stopped)
	attr = ippAddString(ipp, group_tag, IPP_CONST_TAG(IPP_TAG_KEYWORD), "printer-state-reasons", NULL, "moving-to-paused");
      else if (printer->state == IPP_PSTATE_STOPPED)
	attr = ippAddString(ipp, group_tag, IPP_CONST_TAG(IPP_TAG_KEYWORD), "printer-state-reasons", NULL, "paused");

      if (wifi_not_configured)
      {
        if (attr)
          ippSetString(ipp, &attr, ippGetCount(attr), "wifi-not-configured-report");
	else
          attr = ippAddString(ipp, group_tag, IPP_CONST_TAG(IPP_TAG_KEYWORD), "printer-state-reasons", NULL, "wifi-not-configured-report");
      }
      else if (!attr)
	ippAddString(ipp, group_tag, IPP_CONST_TAG(IPP_TAG_KEYWORD), "printer-state-reasons", NULL, "none");
    }
    else
    {
      pappl_preason_t	bit;			// Reason bit

      for (bit = PAPPL_PREASON_OTHER; bit <= PAPPL_PREASON_TONER_LOW; bit *= 2)
      {
        if (printer->state_reasons & bit)
	{
	  if (attr)
	    ippSetString(ipp, &attr, ippGetCount(attr), _papplPrinterReasonString(bit));
	  else
	    attr = ippAddString(ipp, group_tag, IPP_CONST_TAG(IPP_TAG_KEYWORD), "printer-state-reasons", NULL, _papplPrinterReasonString(bit));
	}
      }

      if (printer->is_stopped)
	ippSetString(ipp, &attr, ippGetCount(attr), "moving-to-paused");
      else if (printer->state == IPP_PSTATE_STOPPED)
	ippSetString(ipp, &attr, ippGetCount(attr), "paused");
      else if (wifi_not_configured)
	ippSetString(ipp, &attr, ippGetCount(attr), "wifi-not-configured-report");
    }
  }
}


//
// '_papplPrinterCopyXRI()' - Copy the "printer-xri-supported" attribute.
//

void
_papplPrinterCopyXRI(
    pappl_printer_t *printer,		// I - Printer
    ipp_t           *ipp,		// I - IPP message
    pappl_client_t  *client)		// I - Client
{
  char	uri[1024];			// URI value
  int	i,				// Looping var
	num_values = 0;			// Number of values
  ipp_t	*col,				// Current collection value
	*values[2];			// Values for attribute


  if (httpAddrIsLocalhost(httpGetAddress(client->http)) || !papplSystemGetTLSOnly(client->system))
  {
    // Add ipp: URI...
    httpAssembleURI(HTTP_URI_CODING_ALL, uri, sizeof(uri), "ipp", NULL, client->host_field, client->host_port, printer->resource);
    col = ippNew();

    ippAddString(col, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "xri-authentication", NULL, "none");
    ippAddString(col, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "xri-security", NULL, "none");
    ippAddString(col, IPP_TAG_PRINTER, IPP_TAG_URI, "xri-uri", NULL, uri);

    values[num_values ++] = col;
  }

  if (!httpAddrIsLocalhost(httpGetAddress(client->http)) && !(client->system->options & PAPPL_SOPTIONS_NO_TLS))
  {
    // Add ipps: URI...
    httpAssembleURI(HTTP_URI_CODING_ALL, uri, sizeof(uri), "ipps", NULL, client->host_field, client->host_port, printer->resource);
    col = ippNew();

    ippAddString(col, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "xri-authentication", NULL, papplSystemGetAuthService(client->system) ? "basic" : "none");
    ippAddString(col, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "xri-security", NULL, "tls");
    ippAddString(col, IPP_TAG_PRINTER, IPP_TAG_URI, "xri-uri", NULL, uri);

    values[num_values ++] = col;
  }

  if (num_values > 0)
    ippAddCollections(ipp, IPP_TAG_PRINTER, "printer-xri-supported", IPP_NUM_CAST num_values, (const ipp_t **)values);

  for (i = 0; i < num_values; i ++)
    ippDelete(values[i]);
}


//
// '_papplPrinterIsAuthorized()' - Authorize access to a printer.
//

bool					// O - `true` on success, `false` on failure
_papplPrinterIsAuthorized(
    pappl_client_t  *client)		// I - Client
{
  http_status_t code = _papplClientIsAuthorizedForGroup(client, true, client->printer->print_group, client->printer->print_gid);

  if (code == HTTP_STATUS_CONTINUE && client->job && client->job->username && strcmp(client->username, client->job->username))
  {
    // Not the owner, try authorizing with admin group...
    code = _papplClientIsAuthorizedForGroup(client, true, client->system->admin_group, client->system->admin_gid);
  }

  if (code == HTTP_STATUS_CONTINUE)
    return (true);

  papplClientRespond(client, code, NULL, NULL, 0, 0);
  return (false);
}


//
// '_papplPrinterProcessIPP()' - Process an IPP Printer request.
//

void
_papplPrinterProcessIPP(
    pappl_client_t *client)		// I - Client
{
  if (!client->printer)
  {
    papplClientRespondIPP(client, IPP_STATUS_ERROR_NOT_FOUND, "No printer found.");
    return;
  }

  switch (ippGetOperation(client->request))
  {
    case IPP_OP_PRINT_JOB :
	ipp_print_job(client);
	break;

    case IPP_OP_VALIDATE_JOB :
	ipp_validate_job(client);
	break;

    case IPP_OP_CREATE_JOB :
	ipp_create_job(client);
	break;

    case IPP_OP_CANCEL_CURRENT_JOB :
	ipp_cancel_current_job(client);
	break;

    case IPP_OP_CANCEL_JOBS :
    case IPP_OP_CANCEL_MY_JOBS :
	ipp_cancel_jobs(client);
	break;

    case IPP_OP_GET_JOBS :
	ipp_get_jobs(client);
	break;

    case IPP_OP_GET_PRINTER_ATTRIBUTES :
    case IPP_OP_GET_PRINTER_SUPPORTED_VALUES :
    case IPP_OP_CUPS_GET_DEFAULT :
	ipp_get_printer_attributes(client);
	break;

    case IPP_OP_SET_PRINTER_ATTRIBUTES :
	ipp_set_printer_attributes(client);
	break;

    case IPP_OP_IDENTIFY_PRINTER :
	ipp_identify_printer(client);
	break;

    case IPP_OP_PAUSE_PRINTER :
    case IPP_OP_PAUSE_PRINTER_AFTER_CURRENT_JOB :
	ipp_pause_printer(client);
	break;

    case IPP_OP_RESUME_PRINTER :
	ipp_resume_printer(client);
	break;

    case IPP_OP_ENABLE_PRINTER :
        ipp_enable_printer(client);
        break;

    case IPP_OP_DISABLE_PRINTER :
        ipp_disable_printer(client);
        break;

    case IPP_OP_CREATE_PRINTER_SUBSCRIPTIONS :
    case IPP_OP_CREATE_JOB_SUBSCRIPTIONS :
        _papplSubscriptionIPPCreate(client);
        break;

    case IPP_OP_GET_SUBSCRIPTION_ATTRIBUTES :
        _papplSubscriptionIPPGetAttributes(client);
        break;

    case IPP_OP_GET_SUBSCRIPTIONS :
        _papplSubscriptionIPPList(client);
        break;

    case IPP_OP_RENEW_SUBSCRIPTION :
        _papplSubscriptionIPPRenew(client);
        break;

    case IPP_OP_CANCEL_SUBSCRIPTION :
        _papplSubscriptionIPPCancel(client);
        break;

    case IPP_OP_GET_NOTIFICATIONS :
        _papplSubscriptionIPPGetNotifications(client);
        break;

    default :
        if (client->system->op_cb && (client->system->op_cb)(client, client->system->op_cbdata))
          break;

	papplClientRespondIPP(client, IPP_STATUS_ERROR_OPERATION_NOT_SUPPORTED, "Operation not supported.");
	break;
  }
}


//
// '_papplPrinterSetAttributes()' - Set printer attributes.
//

bool					// O - `true` if OK, `false` otherwise
_papplPrinterSetAttributes(
    pappl_client_t  *client,		// I - Client
    pappl_printer_t *printer)		// I - Printer
{
  int			create_printer;	// Create-Printer request?
  ipp_attribute_t	*rattr;		// Current request attribute
  ipp_tag_t		value_tag;	// Value tag
  cups_len_t		count;		// Number of values
  const char		*name;		// Attribute name
  char			defname[128],	// xxx-default name
			value[1024];	// xxx-default value
  cups_len_t		i, j;		// Looping vars
  pwg_media_t		*pwg;		// PWG media size data
  pappl_pr_driver_data_t driver_data;	// Printer driver data
  bool			do_defaults = false,
					// Update defaults?
			do_ready = false;
					// Update ready media?
  cups_len_t		num_vendor = 0;	// Number of vendor defaults
  cups_option_t		*vendor = NULL;	// Vendor defaults
  pappl_contact_t	contact;	// printer-contact value
  bool			do_contact = false;
					// Update contact?
  const char		*geo_location = NULL,
					// printer-geo-location value
			*location = NULL,
					// printer-location value
			*organization = NULL,
					// printer-organization value
			*org_unit = NULL;
					// printer-organizational-unit value
  char			wifi_ssid[256] = "",
					// printer-wifi-ssid value
			wifi_password[256] = "";
					// printer-wifi-password value
  bool			do_wifi = false;// Join a Wi-Fi network?
  static _pappl_attr_t	pattrs[] =	// Settable printer attributes
  {
    { "label-mode-configured",		IPP_TAG_KEYWORD,	1 },
    { "label-tear-off-configured",	IPP_TAG_INTEGER,	1 },
    { "media-col-default",		IPP_TAG_BEGIN_COLLECTION, 1 },
    { "media-col-ready",		IPP_TAG_BEGIN_COLLECTION, PAPPL_MAX_SOURCE },
    { "media-default",			IPP_TAG_KEYWORD,	1 },
    { "media-ready",			IPP_TAG_KEYWORD,	PAPPL_MAX_SOURCE },
    { "orientation-requested-default",	IPP_TAG_ENUM,		1 },
    { "print-color-mode-default",	IPP_TAG_KEYWORD,	1 },
    { "print-content-optimize-default",	IPP_TAG_KEYWORD,	1 },
    { "print-darkness-default",		IPP_TAG_INTEGER,	1 },
    { "print-quality-default",		IPP_TAG_ENUM,		1 },
    { "print-speed-default",		IPP_TAG_INTEGER,	1 },
    { "printer-contact-col",		IPP_TAG_BEGIN_COLLECTION, 1 },
    { "printer-darkness-configured",	IPP_TAG_INTEGER,	1 },
    { "printer-geo-location",		IPP_TAG_URI,		1 },
    { "printer-location",		IPP_TAG_TEXT,		1 },
    { "printer-organization",		IPP_TAG_TEXT,		1 },
    { "printer-organizational-unit",	IPP_TAG_TEXT,		1 },
    { "printer-resolution-default",	IPP_TAG_RESOLUTION,	1 },
    { "printer-wifi-password",		IPP_TAG_STRING,		1 },
    { "printer-wifi-ssid",		IPP_TAG_NAME,		1 }
  };


  // Preflight request attributes...
  create_printer = ippGetOperation(client->request) == IPP_OP_CREATE_PRINTER;

  papplPrinterGetDriverData(printer, &driver_data);

  for (rattr = ippGetFirstAttribute(client->request); rattr; rattr = ippGetNextAttribute(client->request))
  {
    papplLogClient(client, PAPPL_LOGLEVEL_DEBUG, "%s %s %s%s ...", ippTagString(ippGetGroupTag(rattr)), ippGetName(rattr), ippGetCount(rattr) > 1 ? "1setOf " : "", ippTagString(ippGetValueTag(rattr)));

    if (ippGetGroupTag(rattr) == IPP_TAG_OPERATION || (name = ippGetName(rattr)) == NULL)
    {
      continue;
    }
    else if (ippGetGroupTag(rattr) != IPP_TAG_PRINTER)
    {
      papplClientRespondIPPUnsupported(client, rattr);
      continue;
    }

    if (create_printer && (!strcmp(name, "printer-device-id") || !strcmp(name, "printer-name") || !strcmp(name, "smi2699-device-uri") || !strcmp(name, "smi2699-device-command")))
      continue;

    if ((create_printer || !httpAddrIsLocalhost(httpGetAddress(client->http)) || !client->system->wifi_join_cb) && (!strcmp(name, "printer-wifi-password") || !strcmp(name, "printer-wifi-ssid")))
    {
      // Wi-Fi configuration can only be done over localhost...
      papplClientRespondIPPUnsupported(client, rattr);
      continue;
    }

    // Validate syntax of provided attributes...
    value_tag = ippGetValueTag(rattr);
    count     = ippGetCount(rattr);

    for (i = 0; i < (cups_len_t)(sizeof(pattrs) / sizeof(pattrs[0])); i ++)
    {
      if (!strcmp(name, pattrs[i].name) && value_tag == pattrs[i].value_tag && count <= pattrs[i].max_count)
        break;
    }

    if (i >= (int)(sizeof(pattrs) / sizeof(pattrs[0])))
    {
      for (j = 0; j < (cups_len_t)printer->driver_data.num_vendor; j ++)
      {
        snprintf(defname, sizeof(defname), "%s-default", printer->driver_data.vendor[j]);
        if (!strcmp(name, defname))
        {
          ippAttributeString(rattr, value, sizeof(value));
          num_vendor = cupsAddOption(printer->driver_data.vendor[j], value, num_vendor, &vendor);
          do_defaults = true;
          break;
	}
      }

      if (j >= (cups_len_t)printer->driver_data.num_vendor)
        papplClientRespondIPPUnsupported(client, rattr);
    }

    // Then copy the xxx-default values to the
    if (!strcmp(name, "identify-actions-default"))
    {
      driver_data.identify_default = PAPPL_IDENTIFY_ACTIONS_NONE;

      for (i = 0, count = ippGetCount(rattr); i < count; i ++)
        driver_data.identify_default |= _papplIdentifyActionsValue(ippGetString(rattr, i, NULL));
      do_defaults = true;
    }
    else if (!strcmp(name, "label-mode-configured"))
    {
      driver_data.mode_configured = _papplLabelModeValue(ippGetString(rattr, 0, NULL));
      do_defaults = true;
    }
    else if (!strcmp(name, "label-tear-offset-configured"))
    {
      driver_data.tear_offset_configured = ippGetInteger(rattr, 0);
      do_defaults = true;
    }
    else if (!strcmp(name, "media-col-default"))
    {
      _papplMediaColImport(ippGetCollection(rattr, 0), &driver_data.media_default);
      do_defaults = true;
    }
    else if (!strcmp(name, "media-col-ready"))
    {
      count = ippGetCount(rattr);

      for (i = 0; i < count; i ++)
        _papplMediaColImport(ippGetCollection(rattr, i), driver_data.media_ready + i);

      for (; i < PAPPL_MAX_SOURCE; i ++)
        memset(driver_data.media_ready + i, 0, sizeof(pappl_media_col_t));

      do_ready = true;
    }
    else if (!strcmp(name, "media-default"))
    {
      if ((pwg = pwgMediaForPWG(ippGetString(rattr, 0, NULL))) != NULL)
      {
        papplCopyString(driver_data.media_default.size_name, pwg->pwg, sizeof(driver_data.media_default.size_name));
        driver_data.media_default.size_width  = pwg->width;
        driver_data.media_default.size_length = pwg->length;
      }

      do_defaults = true;
    }
    else if (!strcmp(name, "media-ready"))
    {
      count = ippGetCount(rattr);

      for (i = 0; i < count; i ++)
      {
        if ((pwg = pwgMediaForPWG(ippGetString(rattr, i, NULL))) != NULL)
        {
          papplCopyString(driver_data.media_ready[i].size_name, pwg->pwg, sizeof(driver_data.media_ready[i].size_name));
	  driver_data.media_ready[i].size_width  = pwg->width;
	  driver_data.media_ready[i].size_length = pwg->length;
	}
      }

      for (; i < PAPPL_MAX_SOURCE; i ++)
      {
        driver_data.media_ready[i].size_name[0] = '\0';
        driver_data.media_ready[i].size_width   = 0;
        driver_data.media_ready[i].size_length  = 0;
      }

      do_ready = true;
    }
    else if (!strcmp(name, "orientation-requested-default"))
    {
      driver_data.orient_default = (ipp_orient_t)ippGetInteger(rattr, 0);
      do_defaults = true;
    }
    else if (!strcmp(name, "print-color-mode-default"))
    {
      driver_data.color_default = _papplColorModeValue(ippGetString(rattr, 0, NULL));
      do_defaults = true;
    }
    else if (!strcmp(name, "print-content-optimize-default"))
    {
      driver_data.content_default = _papplContentValue(ippGetString(rattr, 0, NULL));
      do_defaults = true;
    }
    else if (!strcmp(name, "print-darkness-default"))
    {
      driver_data.darkness_default = ippGetInteger(rattr, 0);
      do_defaults = true;
    }
    else if (!strcmp(name, "print-quality-default"))
    {
      driver_data.quality_default = (ipp_quality_t)ippGetInteger(rattr, 0);
      do_defaults = true;
    }
    else if (!strcmp(name, "print-scaling-default"))
    {
      driver_data.scaling_default = _papplScalingValue(ippGetString(rattr, 0, NULL));
      do_defaults = true;
    }
    else if (!strcmp(name, "print-speed-default"))
    {
      driver_data.speed_default = ippGetInteger(rattr, 0);
      do_defaults = true;
    }
    else if (!strcmp(name, "printer-contact-col"))
    {
      _papplContactImport(ippGetCollection(rattr, 0), &contact);
      do_contact = true;
    }
    else if (!strcmp(name, "printer-darkness-configured"))
    {
      driver_data.darkness_configured = ippGetInteger(rattr, 0);
      do_defaults = true;
    }
    else if (!strcmp(name, "printer-geo-location"))
    {
      float geo_lat, geo_lon;		// Latitude and longitude

      geo_location = ippGetString(rattr, 0, NULL);
      if (sscanf(geo_location, "geo:%f,%f", &geo_lat, &geo_lon) != 2 || geo_lat < -90.0 || geo_lat > 90.0 || geo_lon < -180.0 || geo_lon > 180.0)
        papplClientRespondIPPUnsupported(client, rattr);
    }
    else if (!strcmp(name, "printer-location"))
    {
      location = ippGetString(rattr, 0, NULL);
    }
    else if (!strcmp(name, "printer-organization"))
    {
      organization = ippGetString(rattr, 0, NULL);
    }
    else if (!strcmp(name, "printer-organization-unit"))
    {
      org_unit = ippGetString(rattr, 0, NULL);
    }
    else if (!strcmp(name, "printer-resolution-default"))
    {
      ipp_res_t units;			// Resolution units

      driver_data.x_default = ippGetResolution(rattr, 0, &driver_data.y_default, &units);
      do_defaults = true;
    }
    else if (!strcmp(name, "printer-wifi-password"))
    {
      void		*data;		// Password
      cups_len_t	datalen;	// Length of password

      data = ippGetOctetString(rattr, 0, &datalen);
      if (datalen > ((int)sizeof(wifi_password) - 1))
      {
	papplClientRespondIPPUnsupported(client, rattr);
	continue;
      }

      memcpy(wifi_password, data, datalen);
      wifi_password[datalen] = '\0';

      do_wifi = true;
    }
    else if (!strcmp(name, "printer-wifi-ssid"))
    {
      papplCopyString(wifi_ssid, ippGetString(rattr, 0, NULL), sizeof(wifi_ssid));
      do_wifi = true;
    }
  }

  if (ippGetStatusCode(client->response) != IPP_STATUS_OK)
  {
    cupsFreeOptions(num_vendor, vendor);
    return (false);
  }

  // Now apply changes...
  if (do_defaults && !papplPrinterSetDriverDefaults(printer, &driver_data, (int)num_vendor, vendor))
  {
    papplClientRespondIPP(client, IPP_STATUS_ERROR_ATTRIBUTES_OR_VALUES, "One or more attribute values were not supported.");
    cupsFreeOptions(num_vendor, vendor);
    return (false);
  }

  cupsFreeOptions(num_vendor, vendor);

  if (do_ready && !papplPrinterSetReadyMedia(printer, driver_data.num_source, driver_data.media_ready))
  {
    papplClientRespondIPP(client, IPP_STATUS_ERROR_ATTRIBUTES_OR_VALUES, "One or more attribute values were not supported.");
    return (false);
  }

  if (do_wifi)
  {
    if (!(printer->system->wifi_join_cb)(printer->system, printer->system->wifi_cbdata, wifi_ssid, wifi_password))
    {
      papplClientRespondIPP(client, IPP_STATUS_ERROR_ATTRIBUTES_OR_VALUES, "Unable to join Wi-Fi network '%s'.", wifi_ssid);
      return (false);
    }
  }

  if (do_contact)
    papplPrinterSetContact(printer, &contact);

  if (geo_location)
    papplPrinterSetGeoLocation(printer, geo_location);

  if (location)
    papplPrinterSetGeoLocation(printer, location);

  if (organization)
    papplPrinterSetGeoLocation(printer, organization);

  if (org_unit)
    papplPrinterSetGeoLocation(printer, org_unit);

  papplSystemAddEvent(printer->system, printer, NULL, PAPPL_EVENT_PRINTER_CONFIG_CHANGED, NULL);

  return (true);
}


//
// 'create_job()' - Create a new job object from a Print-Job or Create-Job
//                  request.
//

static pappl_job_t *			// O - Job
create_job(
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

  return (_papplJobCreate(client->printer, 0, username, NULL, job_name, client->request));
}


//
// 'ipp_cancel_current_job()' - Cancel the current job.
//

static void
ipp_cancel_current_job(
    pappl_client_t *client)		// I - Client
{
  pappl_job_t	*job;			// Job information


  // Get the job...
  if ((job = client->printer->processing_job) == NULL)
  {
    papplClientRespondIPP(client, IPP_STATUS_ERROR_NOT_FOUND, "No currently printing job.");
    return;
  }

  // Authorize access...
  if (!_papplPrinterIsAuthorized(client))
    return;

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
// 'ipp_cancel_jobs()' - Cancel all jobs.
//

static void
ipp_cancel_jobs(pappl_client_t *client)	// I - Client
{
  http_status_t	auth_status;		// Authorization status


  // Verify the connection is authorized...
  if ((auth_status = papplClientIsAuthorized(client)) != HTTP_STATUS_CONTINUE)
  {
    papplClientRespond(client, auth_status, NULL, NULL, 0, 0);
    return;
  }

  // Cancel all jobs...
  papplPrinterCancelAllJobs(client->printer);

  papplClientRespondIPP(client, IPP_STATUS_OK, NULL);
}


//
// 'ipp_create_job()' - Create a job object.
//

static void
ipp_create_job(pappl_client_t *client)	// I - Client
{
  pappl_job_t		*job;		// New job
  cups_array_t		*ra;		// Attributes to send in response


  // Authorize access...
  if (!_papplPrinterIsAuthorized(client))
    return;

  // Do we have a file to print?
  if (_papplClientHaveDocumentData(client))
  {
    _papplClientFlushDocumentData(client);
    papplClientRespondIPP(client, IPP_STATUS_ERROR_BAD_REQUEST, "Unexpected document data following request.");
    return;
  }

  // Are we accepting jobs?
  if (!client->printer->is_accepting)
  {
    papplClientRespondIPP(client, IPP_STATUS_ERROR_NOT_ACCEPTING_JOBS, "Not accepting new jobs.");
    return;
  }

  // Validate print job attributes...
  if (!valid_job_attributes(client))
    return;

  // Create the job...
  if ((job = create_job(client)) == NULL)
  {
    papplClientRespondIPP(client, IPP_STATUS_ERROR_BUSY, "Currently printing another job.");
    return;
  }

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
}


//
// 'ipp_disable_printer()' - Stop accepting new jobs for a printer.
//

static void
ipp_disable_printer(
    pappl_client_t *client)		// I - Client
{
  // Authorize access...
  if (!_papplPrinterIsAuthorized(client))
    return;

  // Disable the printer...
  papplPrinterDisable(client->printer);
}


//
// 'ipp_enable_printer()' - Start/resume accepting new jobs for a printer.
//

static void
ipp_enable_printer(
    pappl_client_t *client)		// I - Client
{
  // Authorize access...
  if (!_papplPrinterIsAuthorized(client))
    return;

  // Enable the printer...
  papplPrinterEnable(client->printer);
}


//
// 'ipp_get_jobs()' - Get a list of job objects.
//

static void
ipp_get_jobs(pappl_client_t *client)	// I - Client
{
  ipp_attribute_t	*attr;		// Current attribute
  const char		*which_jobs = NULL;
					// which-jobs values
  int			job_comparison;	// Job comparison
  ipp_jstate_t		job_state;	// job-state value
  cups_len_t		i,		// Looping var
			limit,		// Maximum number of jobs to return
			count;		// Number of jobs that match
  const char		*username;	// Username
  cups_array_t		*list;		// Jobs list
  pappl_job_t		*job;		// Current job pointer
  cups_array_t		*ra;		// Requested attributes array


  // Authorize access...
  if (!_papplPrinterIsAuthorized(client))
    return;

  // See if the "which-jobs" attribute have been specified...
  if ((attr = ippFindAttribute(client->request, "which-jobs", IPP_TAG_KEYWORD)) != NULL)
  {
    which_jobs = ippGetString(attr, 0, NULL);
    papplLogClient(client, PAPPL_LOGLEVEL_DEBUG, "Get-Jobs \"which-jobs\"='%s'", which_jobs);
  }

  if (!which_jobs || !strcmp(which_jobs, "not-completed"))
  {
    job_comparison = -1;
    job_state      = IPP_JSTATE_STOPPED;
    list           = client->printer->active_jobs;
  }
  else if (!strcmp(which_jobs, "completed"))
  {
    job_comparison = 1;
    job_state      = IPP_JSTATE_CANCELED;
    list           = client->printer->completed_jobs;
  }
  else if (!strcmp(which_jobs, "all"))
  {
    job_comparison = 1;
    job_state      = IPP_JSTATE_PENDING;
    list           = client->printer->all_jobs;
  }
  else
  {
    papplClientRespondIPP(client, IPP_STATUS_ERROR_ATTRIBUTES_OR_VALUES, "The \"which-jobs\" value '%s' is not supported.", which_jobs);
    ippAddString(client->response, IPP_TAG_UNSUPPORTED_GROUP, IPP_TAG_KEYWORD, "which-jobs", NULL, which_jobs);
    return;
  }

  // See if they want to limit the number of jobs reported...
  if ((attr = ippFindAttribute(client->request, "limit", IPP_TAG_INTEGER)) != NULL)
  {
    int temp = ippGetInteger(attr, 0);

    papplLogClient(client, PAPPL_LOGLEVEL_DEBUG, "Get-Jobs \"limit\"='%d'", temp);
    if (temp < 0)
      limit = 0;
    else
      limit = (cups_len_t)temp;
  }
  else
    limit = 0;

  // See if we only want to see jobs for a specific user...
  username = NULL;

  if ((attr = ippFindAttribute(client->request, "my-jobs", IPP_TAG_BOOLEAN)) != NULL)
  {
    int my_jobs = ippGetBoolean(attr, 0);

    papplLogClient(client, PAPPL_LOGLEVEL_DEBUG, "Get-Jobs \"my-jobs\"='%s'", my_jobs ? "true" : "false");

    if (my_jobs)
    {
      if ((attr = ippFindAttribute(client->request, "requesting-user-name", IPP_TAG_NAME)) == NULL)
      {
	papplClientRespondIPP(client, IPP_STATUS_ERROR_BAD_REQUEST, "Need \"requesting-user-name\" with \"my-jobs\".");
	return;
      }

      username = ippGetString(attr, 0, NULL);

      papplLogClient(client, PAPPL_LOGLEVEL_DEBUG, "Get-Jobs \"requesting-user-name\"='%s'", username);
    }
  }

  // OK, build a list of jobs for this printer...
  ra = ippCreateRequestedArray(client->request);

  papplClientRespondIPP(client, IPP_STATUS_OK, NULL);

  pthread_rwlock_rdlock(&(client->printer->rwlock));

  count = cupsArrayGetCount(list);
  if (limit == 0 || limit > count)
    limit = count;

  for (count = 0, i = 0; i < limit; i ++)
  {
    job = (pappl_job_t *)cupsArrayGetElement(list, i);

    // Filter out jobs that don't match...
    if ((job_comparison < 0 && job->state > job_state) || /* (job_comparison == 0 && job->state != job_state) || */ (job_comparison > 0 && job->state < job_state) || (username && job->username && strcasecmp(username, job->username)))
      continue;

    if (count > 0)
      ippAddSeparator(client->response);

    count ++;
    _papplJobCopyAttributes(job, client, ra);
  }

  cupsArrayDelete(ra);

  pthread_rwlock_unlock(&(client->printer->rwlock));
}


//
// 'ipp_get_printer_attributes()' - Get the attributes for a printer object.
//

static void
ipp_get_printer_attributes(
    pappl_client_t *client)		// I - Client
{
  cups_array_t		*ra;		// Requested attributes array
  pappl_printer_t	*printer = client->printer;
					// Printer


  if (!printer->device_in_use && !printer->processing_job && (time(NULL) - printer->status_time) > 1 && printer->driver_data.status_cb)
  {
    // Update printer status...
    (printer->driver_data.status_cb)(printer);
    printer->status_time = time(NULL);
  }

  // Send the attributes...
  ra = ippCreateRequestedArray(client->request);

  papplClientRespondIPP(client, IPP_STATUS_OK, NULL);

  pthread_rwlock_rdlock(&(printer->rwlock));

  _papplPrinterCopyAttributes(printer, client, ra, ippGetString(ippFindAttribute(client->request, "document-format", IPP_TAG_MIMETYPE), 0, NULL));

  pthread_rwlock_unlock(&(printer->rwlock));

  cupsArrayDelete(ra);
}


//
// 'ipp_identify_printer()' - Beep or display a message.
//

static void
ipp_identify_printer(
    pappl_client_t *client)		// I - Client
{
  cups_len_t		i;		// Looping var
  ipp_attribute_t	*attr;		// IPP attribute
  pappl_identify_actions_t actions;	// "identify-actions" value
  const char		*message;	// "message" value


  if (client->printer->driver_data.identify_cb)
  {
    if ((attr = ippFindAttribute(client->request, "identify-actions", IPP_TAG_KEYWORD)) != NULL)
    {
      actions = PAPPL_IDENTIFY_ACTIONS_NONE;

      for (i = 0; i < ippGetCount(attr); i ++)
	actions |= _papplIdentifyActionsValue(ippGetString(attr, i, NULL));
    }
    else
      actions = client->printer->driver_data.identify_default;

    if ((attr = ippFindAttribute(client->request, "message", IPP_TAG_TEXT)) != NULL)
      message = ippGetString(attr, 0, NULL);
    else
      message = NULL;

    (client->printer->driver_data.identify_cb)(client->printer, actions, message);
  }

  papplClientRespondIPP(client, IPP_STATUS_OK, NULL);
}


//
// 'ipp_pause_printer()' - Stop a printer.
//

static void
ipp_pause_printer(
    pappl_client_t *client)		// I - Client
{
  http_status_t	auth_status;		// Authorization status


  // Verify the connection is authorized...
  if ((auth_status = papplClientIsAuthorized(client)) != HTTP_STATUS_CONTINUE)
  {
    papplClientRespond(client, auth_status, NULL, NULL, 0, 0);
    return;
  }

  papplPrinterPause(client->printer);
  papplClientRespondIPP(client, IPP_STATUS_OK, "Printer paused.");
}


//
// 'ipp_print_job()' - Create a job object with an attached document.
//

static void
ipp_print_job(pappl_client_t *client)	// I - Client
{
  pappl_job_t		*job;		// New job


  // Authorize access...
  if (!_papplPrinterIsAuthorized(client))
    return;

  // Do we have a file to print?
  if (!_papplClientHaveDocumentData(client))
  {
    papplClientRespondIPP(client, IPP_STATUS_ERROR_BAD_REQUEST, "No file in request.");
    return;
  }

  // Are we accepting jobs?
  if (!client->printer->is_accepting)
  {
    papplClientRespondIPP(client, IPP_STATUS_ERROR_NOT_ACCEPTING_JOBS, "Not accepting new jobs.");
    return;
  }

  // Validate print job attributes...
  if (!valid_job_attributes(client))
  {
    _papplClientFlushDocumentData(client);
    return;
  }

  // Create the job...
  if ((job = create_job(client)) == NULL)
  {
    papplClientRespondIPP(client, IPP_STATUS_ERROR_BUSY, "Currently printing another job.");
    return;
  }

  // Then finish getting the document data and process things...
  _papplJobCopyDocumentData(client, job);
}


//
// 'ipp_resume_printer()' - Start a printer.
//

static void
ipp_resume_printer(
    pappl_client_t *client)		// I - Client
{
  http_status_t	auth_status;		// Authorization status


  // Verify the connection is authorized...
  if ((auth_status = papplClientIsAuthorized(client)) != HTTP_STATUS_CONTINUE)
  {
    papplClientRespond(client, auth_status, NULL, NULL, 0, 0);
    return;
  }

  papplPrinterResume(client->printer);
  papplClientRespondIPP(client, IPP_STATUS_OK, "Printer resumed.");
}


//
// 'ipp_set_printer_attributes()' - Set printer attributes.
//

static void
ipp_set_printer_attributes(
    pappl_client_t *client)		// I - Client
{
  http_status_t	auth_status;		// Authorization status


  // Verify the connection is authorized...
  if ((auth_status = papplClientIsAuthorized(client)) != HTTP_STATUS_CONTINUE)
  {
    papplClientRespond(client, auth_status, NULL, NULL, 0, 0);
    return;
  }

  if (!_papplPrinterSetAttributes(client, client->printer))
    return;

  papplClientRespondIPP(client, IPP_STATUS_OK, "Printer attributes set.");
}


//
// 'ipp_validate_job()' - Validate job creation attributes.
//

static void
ipp_validate_job(
    pappl_client_t *client)		// I - Client
{
  // Authorize access...
  if (!_papplPrinterIsAuthorized(client))
    return;

  if (valid_job_attributes(client))
    papplClientRespondIPP(client, IPP_STATUS_OK, NULL);
}


//
// 'valid_job_attributes()' - Determine whether the job attributes are valid.
//
// When one or more job attributes are invalid, this function adds a suitable
// response and attributes to the unsupported group.
//

static bool				// O - `true` if valid, `false` if not
valid_job_attributes(
    pappl_client_t *client)		// I - Client
{
  cups_len_t		i,		// Looping var
			count;		// Number of values
  bool			valid = true;	// Valid attributes?
  ipp_attribute_t	*attr,		// Current attribute
			*supported;	// xxx-supported attribute


  // If a shutdown is pending, do not accept more jobs...
  if (client->system->shutdown_time)
  {
    papplClientRespondIPP(client, IPP_STATUS_ERROR_NOT_ACCEPTING_JOBS, "Not accepting new jobs.");
    return (false);
  }

  // Check operation attributes...
  valid = _papplJobValidateDocumentAttributes(client);

  pthread_rwlock_rdlock(&client->printer->rwlock);

  // Check the various job template attributes...
  if ((attr = ippFindAttribute(client->request, "copies", IPP_TAG_ZERO)) != NULL)
  {
    if (ippGetCount(attr) != 1 || ippGetValueTag(attr) != IPP_TAG_INTEGER || ippGetInteger(attr, 0) < 1 || ippGetInteger(attr, 0) > 999)
    {
      papplClientRespondIPPUnsupported(client, attr);
      valid = false;
    }
  }

  if ((attr = ippFindAttribute(client->request, "ipp-attribute-fidelity", IPP_TAG_ZERO)) != NULL)
  {
    if (ippGetCount(attr) != 1 || ippGetValueTag(attr) != IPP_TAG_BOOLEAN)
    {
      papplClientRespondIPPUnsupported(client, attr);
      valid = false;
    }
  }

  if ((attr = ippFindAttribute(client->request, "job-hold-until", IPP_TAG_ZERO)) != NULL)
  {
    if (ippGetCount(attr) != 1 || (ippGetValueTag(attr) != IPP_TAG_NAME && ippGetValueTag(attr) != IPP_TAG_NAMELANG && ippGetValueTag(attr) != IPP_TAG_KEYWORD) || strcmp(ippGetString(attr, 0, NULL), "no-hold"))
    {
      papplClientRespondIPPUnsupported(client, attr);
      valid = false;
    }
  }

  if ((attr = ippFindAttribute(client->request, "job-impressions", IPP_TAG_ZERO)) != NULL)
  {
    if (ippGetCount(attr) != 1 || ippGetValueTag(attr) != IPP_TAG_INTEGER || ippGetInteger(attr, 0) < 0)
    {
      papplClientRespondIPPUnsupported(client, attr);
      valid = false;
    }
  }

  if ((attr = ippFindAttribute(client->request, "job-name", IPP_TAG_ZERO)) != NULL)
  {
    if (ippGetCount(attr) != 1 || (ippGetValueTag(attr) != IPP_TAG_NAME && ippGetValueTag(attr) != IPP_TAG_NAMELANG))
    {
      papplClientRespondIPPUnsupported(client, attr);
      valid = false;
    }

    ippSetGroupTag(client->request, &attr, IPP_TAG_JOB);
  }
  else
    ippAddString(client->request, IPP_TAG_JOB, IPP_TAG_NAME, "job-name", NULL, "Untitled");

  if ((attr = ippFindAttribute(client->request, "job-priority", IPP_TAG_ZERO)) != NULL)
  {
    if (ippGetCount(attr) != 1 || ippGetValueTag(attr) != IPP_TAG_INTEGER || ippGetInteger(attr, 0) < 1 || ippGetInteger(attr, 0) > 100)
    {
      papplClientRespondIPPUnsupported(client, attr);
      valid = false;
    }
  }

  if ((attr = ippFindAttribute(client->request, "job-sheets", IPP_TAG_ZERO)) != NULL)
  {
    if (ippGetCount(attr) != 1 || (ippGetValueTag(attr) != IPP_TAG_NAME && ippGetValueTag(attr) != IPP_TAG_NAMELANG && ippGetValueTag(attr) != IPP_TAG_KEYWORD) || strcmp(ippGetString(attr, 0, NULL), "none"))
    {
      papplClientRespondIPPUnsupported(client, attr);
      valid = false;
    }
  }

  if ((attr = ippFindAttribute(client->request, "media", IPP_TAG_ZERO)) != NULL)
  {
    if (ippGetCount(attr) != 1 || (ippGetValueTag(attr) != IPP_TAG_NAME && ippGetValueTag(attr) != IPP_TAG_NAMELANG && ippGetValueTag(attr) != IPP_TAG_KEYWORD))
    {
      papplClientRespondIPPUnsupported(client, attr);
      valid = false;
    }
    else
    {
      supported = ippFindAttribute(client->printer->driver_attrs, "media-supported", IPP_TAG_KEYWORD);

      if (!ippContainsString(supported, ippGetString(attr, 0, NULL)))
      {
	papplClientRespondIPPUnsupported(client, attr);
	valid = false;
      }
    }
  }

  if ((attr = ippFindAttribute(client->request, "media-col", IPP_TAG_ZERO)) != NULL)
  {
    ipp_t		*col,		// media-col collection
			*size;		// media-size collection
    ipp_attribute_t	*member,	// Member attribute
			*x_dim,		// x-dimension
			*y_dim;		// y-dimension
    int			x_value,	// y-dimension value
			y_value;	// x-dimension value

    if (ippGetCount(attr) != 1 || ippGetValueTag(attr) != IPP_TAG_BEGIN_COLLECTION)
    {
      papplClientRespondIPPUnsupported(client, attr);
      valid = false;
    }

    col = ippGetCollection(attr, 0);

    if ((member = ippFindAttribute(col, "media-size-name", IPP_TAG_ZERO)) != NULL)
    {
      if (ippGetCount(member) != 1 || (ippGetValueTag(member) != IPP_TAG_NAME && ippGetValueTag(member) != IPP_TAG_NAMELANG && ippGetValueTag(member) != IPP_TAG_KEYWORD))
      {
	papplClientRespondIPPUnsupported(client, attr);
	valid = false;
      }
      else
      {
	supported = ippFindAttribute(client->printer->driver_attrs, "media-supported", IPP_TAG_KEYWORD);

	if (!ippContainsString(supported, ippGetString(member, 0, NULL)))
	{
	  papplClientRespondIPPUnsupported(client, attr);
	  valid = false;
	}
      }
    }
    else if ((member = ippFindAttribute(col, "media-size", IPP_TAG_BEGIN_COLLECTION)) != NULL)
    {
      if (ippGetCount(member) != 1)
      {
	papplClientRespondIPPUnsupported(client, attr);
	valid = false;
      }
      else
      {
	size = ippGetCollection(member, 0);

	if ((x_dim = ippFindAttribute(size, "x-dimension", IPP_TAG_INTEGER)) == NULL || ippGetCount(x_dim) != 1 || (y_dim = ippFindAttribute(size, "y-dimension", IPP_TAG_INTEGER)) == NULL || ippGetCount(y_dim) != 1)
	{
	  papplClientRespondIPPUnsupported(client, attr);
	  valid = false;
	}
	else
	{
	  x_value   = ippGetInteger(x_dim, 0);
	  y_value   = ippGetInteger(y_dim, 0);
	  supported = ippFindAttribute(client->printer->driver_attrs, "media-size-supported", IPP_TAG_BEGIN_COLLECTION);
	  count     = ippGetCount(supported);

	  for (i = 0; i < count ; i ++)
	  {
	    size  = ippGetCollection(supported, i);
	    x_dim = ippFindAttribute(size, "x-dimension", IPP_TAG_ZERO);
	    y_dim = ippFindAttribute(size, "y-dimension", IPP_TAG_ZERO);

	    if (ippContainsInteger(x_dim, x_value) && ippContainsInteger(y_dim, y_value))
	      break;
	  }

	  if (i >= count)
	  {
	    papplClientRespondIPPUnsupported(client, attr);
	    valid = false;
	  }
	}
      }
    }
  }

  if ((attr = ippFindAttribute(client->request, "multiple-document-handling", IPP_TAG_ZERO)) != NULL)
  {
    if (ippGetCount(attr) != 1 || ippGetValueTag(attr) != IPP_TAG_KEYWORD || (strcmp(ippGetString(attr, 0, NULL), "separate-documents-uncollated-copies") && strcmp(ippGetString(attr, 0, NULL), "separate-documents-collated-copies")))
    {
      papplClientRespondIPPUnsupported(client, attr);
      valid = false;
    }
  }

  if ((attr = ippFindAttribute(client->request, "orientation-requested", IPP_TAG_ZERO)) != NULL)
  {
    if (ippGetCount(attr) != 1 || ippGetValueTag(attr) != IPP_TAG_ENUM || ippGetInteger(attr, 0) < IPP_ORIENT_PORTRAIT || ippGetInteger(attr, 0) > IPP_ORIENT_NONE)
    {
      papplClientRespondIPPUnsupported(client, attr);
      valid = false;
    }
  }

  if ((attr = ippFindAttribute(client->request, "page-ranges", IPP_TAG_ZERO)) != NULL)
  {
    int upper = 0, lower = ippGetRange(attr, 0, &upper);
					// "page-ranges" value

    if (!ippGetBoolean(ippFindAttribute(client->printer->driver_attrs, "page-ranges-supported", IPP_TAG_BOOLEAN), 0) || ippGetValueTag(attr) != IPP_TAG_RANGE || ippGetCount(attr) != 1 || lower < 1 || upper < lower)
    {
      papplClientRespondIPPUnsupported(client, attr);
      valid = false;
    }
  }

  if ((attr = ippFindAttribute(client->request, "print-color-mode", IPP_TAG_ZERO)) != NULL)
  {
    pappl_color_mode_t value = _papplColorModeValue(ippGetString(attr, 0, NULL));
					// "print-color-mode" value

    if (ippGetCount(attr) != 1 || ippGetValueTag(attr) != IPP_TAG_KEYWORD || !(value & client->printer->driver_data.color_supported))
    {
      papplClientRespondIPPUnsupported(client, attr);
      valid = false;
    }
  }

  if ((attr = ippFindAttribute(client->request, "print-content-optimize", IPP_TAG_ZERO)) != NULL)
  {
    if (ippGetCount(attr) != 1 || ippGetValueTag(attr) != IPP_TAG_KEYWORD || !_papplContentValue(ippGetString(attr, 0, NULL)))
    {
      papplClientRespondIPPUnsupported(client, attr);
      valid = false;
    }
  }

  if ((attr = ippFindAttribute(client->request, "print-darkness", IPP_TAG_ZERO)) != NULL)
  {
    int value = ippGetInteger(attr, 0);	// "print-darkness" value

    if (ippGetCount(attr) != 1 || ippGetValueTag(attr) != IPP_TAG_INTEGER || value < -100 || value > 100 || client->printer->driver_data.darkness_supported == 0)
    {
      papplClientRespondIPPUnsupported(client, attr);
      valid = false;
    }
  }

  if ((attr = ippFindAttribute(client->request, "print-quality", IPP_TAG_ZERO)) != NULL)
  {
    if (ippGetCount(attr) != 1 || ippGetValueTag(attr) != IPP_TAG_ENUM || ippGetInteger(attr, 0) < IPP_QUALITY_DRAFT || ippGetInteger(attr, 0) > IPP_QUALITY_HIGH)
    {
      papplClientRespondIPPUnsupported(client, attr);
      valid = false;
    }
  }

  if ((attr = ippFindAttribute(client->request, "print-scaling", IPP_TAG_ZERO)) != NULL)
  {
    if (ippGetCount(attr) != 1 || ippGetValueTag(attr) != IPP_TAG_KEYWORD || !_papplScalingValue(ippGetString(attr, 0, NULL)))
    {
      papplClientRespondIPPUnsupported(client, attr);
      valid = false;
    }
  }

  if ((attr = ippFindAttribute(client->request, "print-speed", IPP_TAG_ZERO)) != NULL)
  {
    int value = ippGetInteger(attr, 0);	// "print-speed" value

    if (ippGetCount(attr) != 1 || ippGetValueTag(attr) != IPP_TAG_INTEGER || value < client->printer->driver_data.speed_supported[0] || value > client->printer->driver_data.speed_supported[1] || client->printer->driver_data.speed_supported[1] == 0)
    {
      papplClientRespondIPPUnsupported(client, attr);
      valid = false;
    }
  }

  if ((attr = ippFindAttribute(client->request, "printer-resolution", IPP_TAG_ZERO)) != NULL)
  {
    int		xdpi,			// Horizontal resolution
		ydpi;			// Vertical resolution
    ipp_res_t	units;			// Resolution units

    xdpi  = ippGetResolution(attr, 0, &ydpi, &units);

    if (ippGetCount(attr) != 1 || ippGetValueTag(attr) != IPP_TAG_RESOLUTION || units != IPP_RES_PER_INCH)
    {
      papplClientRespondIPPUnsupported(client, attr);
      valid = false;
    }
    else
    {
      for (i = 0; i < (cups_len_t)client->printer->driver_data.num_resolution; i ++)
      {
        if (xdpi == client->printer->driver_data.x_resolution[i] && ydpi == client->printer->driver_data.y_resolution[i])
          break;
      }

      if (i >= (cups_len_t)client->printer->driver_data.num_resolution)
      {
	papplClientRespondIPPUnsupported(client, attr);
	valid = false;
      }
    }
  }

  if ((attr = ippFindAttribute(client->request, "sides", IPP_TAG_ZERO)) != NULL)
  {
    pappl_sides_t value = _papplSidesValue(ippGetString(attr, 0, NULL));
					// "sides" value

    if (ippGetCount(attr) != 1 || ippGetValueTag(attr) != IPP_TAG_KEYWORD || !(value & client->printer->driver_data.sides_supported))
    {
      papplClientRespondIPPUnsupported(client, attr);
      valid = false;
    }
  }

  pthread_rwlock_unlock(&client->printer->rwlock);

  return (valid);
}
