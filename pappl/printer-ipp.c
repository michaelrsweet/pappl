//
// Printer IPP processing for the Printer Application Framework
//
// Copyright © 2019-2025 by Michael R Sweet.
// Copyright © 2010-2019 by Apple Inc.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#include "pappl-private.h"


//
// Local functions...
//

static pappl_job_t	*create_job(pappl_client_t *client);

static void		ipp_acknowledge_identify_printer(pappl_client_t *client);
static void		ipp_cancel_current_job(pappl_client_t *client);
static void		ipp_cancel_jobs(pappl_client_t *client);
static void		ipp_create_job(pappl_client_t *client);
static void		ipp_deregister_output_device(pappl_client_t *client);
static void		ipp_disable_printer(pappl_client_t *client);
static void		ipp_enable_printer(pappl_client_t *client);
static void		ipp_get_jobs(pappl_client_t *client);
static void		ipp_get_output_device_attributes(pappl_client_t *client);
static void		ipp_get_printer_attributes(pappl_client_t *client);
static void		ipp_hold_new_jobs(pappl_client_t *client);
static void		ipp_identify_printer(pappl_client_t *client);
static void		ipp_pause_printer(pappl_client_t *client);
static void		ipp_print_job(pappl_client_t *client);
static void		ipp_release_held_new_jobs(pappl_client_t *client);
static void		ipp_resume_printer(pappl_client_t *client);
static void		ipp_set_printer_attributes(pappl_client_t *client);
static void		ipp_update_active_jobs(pappl_client_t *client);
static void		ipp_update_output_device_attributes(pappl_client_t *client);
static void		ipp_validate_job(pappl_client_t *client);

static bool		valid_job_attributes(pappl_client_t *client, const char **format);


//
// '_papplPrinterCopyAttributesNoLock()' - Copy printer attributes to a response...
//

void
_papplPrinterCopyAttributesNoLock(
    pappl_printer_t *printer,		// I - Printer
    pappl_client_t  *client,		// I - Client
    cups_array_t    *ra,		// I - Requested attributes
    const char      *format)		// I - "document-format" value, if any
{
  size_t	i,			// Looping var
		num_values;		// Number of values
  unsigned	bit;			// Current bit value
  const char	*svalues[100];		// String values
  int		ivalues[100];		// Integer values
  pappl_pr_driver_data_t *data = &printer->driver_data;
					// Driver data
  const char	*webscheme = (httpAddrIsLocalhost(httpGetAddress(client->http)) || !papplSystemGetTLSOnly(client->system)) ? "http" : "https";
					// URL scheme for resources


  _papplCopyAttributes(client->response, printer->attrs, ra, IPP_TAG_ZERO, true);
  _papplCopyAttributes(client->response, printer->driver_attrs, ra, IPP_TAG_ZERO, false);
  _papplPrinterCopyStateNoLock(printer, IPP_TAG_PRINTER, client->response, client, ra);

  if (!ra || cupsArrayFind(ra, "copies-default"))
  {
    // copies-default
    ippAddInteger(client->response, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "copies-default", data->copies_default);
  }

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
      ippAddStrings(client->response, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "identify-actions-default", num_values, NULL, svalues);
    else
      ippAddString(client->response, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "identify-actions-default", NULL, "none");
  }

  if (printer->max_preserved_jobs > 0)
  {
    static const char * const job_retain_until[] =
    {					// job-retain-until-supported values
      "day-time",
      "evening",
      "indefinite",
      "night",
      "no-hold",
      "second-shift",
      "third-shift",
      "weekend"
    };

    if (!ra || cupsArrayFind(ra, "job-retain-until-default"))
      ippAddString(client->response, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "job-retain-until-default", NULL, "none");

    if (!ra || cupsArrayFind(ra, "job-retain-until-interval-default"))
      ippAddOutOfBand(client->response, IPP_TAG_PRINTER, IPP_TAG_NOVALUE, "job-retain-until-interval-default");

    if (!ra || cupsArrayFind(ra, "job-retain-until-interval-supported"))
      ippAddRange(client->response, IPP_TAG_PRINTER, "job-retain-until-interval-supported", 0, 86400);

    if (!ra || cupsArrayFind(ra, "job-retain-until-supported"))
      ippAddStrings(client->response, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "job-retain-until-supported", (size_t)(sizeof(job_retain_until) / sizeof(job_retain_until[0])), NULL, job_retain_until);

    if (!ra || cupsArrayFind(ra, "job-retain-until-time-supported"))
      ippAddRange(client->response, IPP_TAG_PRINTER, "job-retain-until-time-supported", 0, 86400);
  }

  if (!ra || cupsArrayFind(ra, "job-spooling-supported"))
    ippAddString(client->response, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "job-spooling-supported", NULL, (printer->max_active_jobs == 1 || (format && (!strcmp(format, "image/pwg-raster") || !strcmp(format, "image/urf")))) ? "stream" : "spool");

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
      for (i = 0; i < (size_t)printer->num_supply; i ++)
        svalues[i] = _papplMarkerColorString(supply[i].color);

      ippAddStrings(client->response, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_NAME), "marker-colors", printer->num_supply, NULL, svalues);
    }

    if (!ra || cupsArrayFind(ra, "marker-high-levels"))
    {
      for (i = 0; i < (size_t)printer->num_supply; i ++)
        ivalues[i] = supply[i].is_consumed ? 100 : 90;

      ippAddIntegers(client->response, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "marker-high-levels", printer->num_supply, ivalues);
    }

    if (!ra || cupsArrayFind(ra, "marker-levels"))
    {
      for (i = 0; i < (size_t)printer->num_supply; i ++)
        ivalues[i] = supply[i].level;

      ippAddIntegers(client->response, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "marker-levels", printer->num_supply, ivalues);
    }

    if (!ra || cupsArrayFind(ra, "marker-low-levels"))
    {
      for (i = 0; i < (size_t)printer->num_supply; i ++)
        ivalues[i] = supply[i].is_consumed ? 10 : 0;

      ippAddIntegers(client->response, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "marker-low-levels", printer->num_supply, ivalues);
    }

    if (!ra || cupsArrayFind(ra, "marker-names"))
    {
      for (i = 0; i < (size_t)printer->num_supply; i ++)
        svalues[i] = supply[i].description;

      ippAddStrings(client->response, IPP_TAG_PRINTER, IPP_TAG_NAME, "marker-names", printer->num_supply, NULL, svalues);
    }

    if (!ra || cupsArrayFind(ra, "marker-types"))
    {
      for (i = 0; i < (size_t)printer->num_supply; i ++)
        svalues[i] = _papplMarkerTypeString(supply[i].type);

      ippAddStrings(client->response, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "marker-types", printer->num_supply, NULL, svalues);
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
    size_t		j,		// Looping var
			count;		// Number of values
    ipp_t		*col;		// Collection value
    ipp_attribute_t	*attr;		// media-col-ready attribute
    pappl_media_col_t	media;		// Current media...

    for (i = 0, count = 0; i < (size_t)printer->num_ready; i ++)
    {
      if (data->media_ready[i].size_name[0])
        count ++;
    }

    if (data->borderless && (data->bottom_top != 0 || data->left_right != 0))
      count *= 2;			// Need to report ready media for borderless, too...

    if (count > 0)
    {
      attr = ippAddCollections(client->response, IPP_TAG_PRINTER, "media-col-ready", count, NULL);

      for (i = 0, j = 0; i < (size_t)printer->num_ready && j < count; i ++)
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
	    ippSetCollection(client->response, &attr, j ++, col);
	    ippDelete(col);

	    media.bottom_margin = media.top_margin   = 0;
	    media.left_margin   = media.right_margin = 0;
	    col = _papplMediaColExport(&printer->driver_data, &media, 0);
	    ippSetCollection(client->response, &attr, j ++, col);
	    ippDelete(col);
	  }
	  else
	  {
	    // Just report the single media-col value...
	    col = _papplMediaColExport(&printer->driver_data, data->media_ready + i, 0);
	    ippSetCollection(client->response, &attr, j ++, col);
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
    size_t		j,		// Looping vars
			count;		// Number of values
    ipp_attribute_t	*attr;		// media-col-ready attribute

    for (i = 0, count = 0; i < (size_t)printer->num_ready; i ++)
    {
      if (data->media_ready[i].size_name[0])
        count ++;
    }

    if (count > 0)
    {
      attr = ippAddStrings(client->response, IPP_TAG_PRINTER, IPP_TAG_KEYWORD, "media-ready", count, NULL, NULL);

      for (i = 0, j = 0; i < (size_t)printer->num_ready && j < count; i ++)
      {
	if (data->media_ready[i].size_name[0])
	  ippSetString(client->response, &attr, j ++, data->media_ready[i].size_name);
      }
    }
  }

  if (!ra || cupsArrayFind(ra, "multiple-document-handling-default"))
    ippAddString(client->response, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "multiple-document-handling-default", NULL, _papplHandlingString(data->handling_default));

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

  if ((!ra || cupsArrayFind(ra, "print-darkness-default")) && data->darkness_supported > 0)
    ippAddInteger(client->response, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "print-darkness-default", data->darkness_default);

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

  if ((!ra || cupsArrayFind(ra, "print-speed-default")) && data->speed_supported[1] > 0)
    ippAddInteger(client->response, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "print-speed-default", data->speed_default);

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

  _papplSystemExportVersions(client->system, client->response, IPP_TAG_PRINTER, ra);

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

    for (i = 0, media = data->media_ready; i < (size_t)data->num_source; i ++, media ++)
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
        ippSetOctetString(client->response, &attr, ippGetCount(attr), value, strlen(value));
      else
        attr = ippAddOctetString(client->response, IPP_TAG_PRINTER, "printer-input-tray", value, strlen(value));
    }

    // The "auto" tray is a dummy entry...
    cupsCopyString(value, "type=other;mediafeed=0;mediaxfeed=0;maxcapacity=-2;level=-2;status=0;name=auto;", sizeof(value));
    ippSetOctetString(client->response, &attr, ippGetCount(attr), value, strlen(value));
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
    size_t		rcount;		// Number of resources

    // Cannot use cupsArrayGetFirst/Last since other threads might be iterating
    // this array...
    for (i = 0, num_values = 0, rcount = cupsArrayGetCount(printer->system->resources); i < rcount && num_values < (size_t)(sizeof(svalues) / sizeof(svalues[0])); i ++)
    {
      r = (_pappl_resource_t *)cupsArrayGetElement(printer->system->resources, (size_t)i);

      if (r->language)
        svalues[num_values ++] = r->language;
    }

    if (num_values > 0)
      ippAddStrings(client->response, IPP_TAG_PRINTER, IPP_TAG_LANGUAGE, "printer-strings-languages-supported", num_values, NULL, svalues);
  }

  if (!ra || cupsArrayFind(ra, "printer-strings-uri"))
  {
    const char	*lang = ippGetString(ippFindAttribute(client->request, "attributes-natural-language", IPP_TAG_LANGUAGE), 0, NULL);
					// Language
    char	baselang[3],		// Base language
		uri[1024];		// Strings file URI
    _pappl_resource_t	*r;		// Current resource
    size_t	rcount;			// Number of resources

    cupsCopyString(baselang, lang, sizeof(baselang));

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
  }

  if (printer->num_supply > 0)
  {
    pappl_supply_t	 *supply = printer->supply;
					// Supply values...

    if (!ra || cupsArrayFind(ra, "printer-supply"))
    {
      char		value[256];	// "printer-supply" value
      ipp_attribute_t	*attr = NULL;	// "printer-supply" attribute

      for (i = 0; i < (size_t)printer->num_supply; i ++)
      {
	snprintf(value, sizeof(value), "index=%u;type=%s;maxcapacity=100;level=%d;colorantname=%s;", (unsigned)i, _papplSupplyTypeString(supply[i].type), supply[i].level, _papplSupplyColorString(supply[i].color));

	if (attr)
	  ippSetOctetString(client->response, &attr, ippGetCount(attr), value, strlen(value));
	else
	  attr = ippAddOctetString(client->response, IPP_TAG_PRINTER, "printer-supply", value, strlen(value));
      }
    }

    if (!ra || cupsArrayFind(ra, "printer-supply-description"))
    {
      for (i = 0; i < (size_t)printer->num_supply; i ++)
        svalues[i] = supply[i].description;

      ippAddStrings(client->response, IPP_TAG_PRINTER, IPP_TAG_TEXT, "printer-supply-description", printer->num_supply, NULL, svalues);
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

    if (!httpAddrIsLocalhost(httpGetAddress(client->http)) && !(client->system->options & PAPPL_SOPTIONS_NO_TLS))
    {
      httpAssembleURI(HTTP_URI_CODING_ALL, uris[num_values], sizeof(uris[0]), "ipps", NULL, client->host_field, client->host_port, printer->resource);
      values[num_values] = uris[num_values];
      num_values ++;
    }

    if (httpAddrIsLocalhost(httpGetAddress(client->http)) || !papplSystemGetTLSOnly(client->system))
    {
      httpAssembleURI(HTTP_URI_CODING_ALL, uris[num_values], sizeof(uris[0]), "ipp", NULL, client->host_field, client->host_port, printer->resource);
      values[num_values] = uris[num_values];
      num_values ++;
    }

    if (num_values > 0)
      ippAddStrings(client->response, IPP_TAG_PRINTER, IPP_TAG_URI, "printer-uri-supported", num_values, NULL, values);
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
    _papplPrinterCopyXRINoLock(printer, client->response, client);

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
	"basic",
	"none"
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
// '_papplPrinterCopyStateNoLock()' - Copy the printer-state-xxx attributes.
//

void
_papplPrinterCopyStateNoLock(
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

      if (printer->hold_new_jobs)
      {
        if (attr)
          ippSetString(ipp, &attr, ippGetCount(attr), "hold-new-jobs");
	else
          attr = ippAddString(ipp, group_tag, IPP_CONST_TAG(IPP_TAG_KEYWORD), "printer-state-reasons", NULL, "hold-new-jobs");
      }

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

      for (bit = PAPPL_PREASON_OTHER; bit <= PAPPL_PREASON_IDENTIFY_PRINTER_REQUESTED; bit *= 2)
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

      if (printer->hold_new_jobs)
	ippSetString(ipp, &attr, ippGetCount(attr), "hold-new-jobs");

      if (wifi_not_configured)
	ippSetString(ipp, &attr, ippGetCount(attr), "wifi-not-configured-report");
    }
  }
}


//
// '_papplPrinterCopyXRINoLock()' - Copy the "printer-xri-supported" attribute.
//

void
_papplPrinterCopyXRINoLock(
    pappl_printer_t *printer,		// I - Printer
    ipp_t           *ipp,		// I - IPP message
    pappl_client_t  *client)		// I - Client
{
  char		uri[1024];		// URI value
  size_t	i,			// Looping var
		num_values = 0;		// Number of values
  ipp_t		*col,			// Current collection value
		*values[2];		// Values for attribute


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
    ippAddCollections(ipp, IPP_TAG_PRINTER, "printer-xri-supported", num_values, (const ipp_t **)values);

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

    case IPP_OP_HOLD_NEW_JOBS :
        ipp_hold_new_jobs(client);
        break;

    case IPP_OP_RELEASE_HELD_NEW_JOBS :
        ipp_release_held_new_jobs(client);
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

    case IPP_OP_ACKNOWLEDGE_IDENTIFY_PRINTER :
        if (client->printer->output_devices)
	  ipp_acknowledge_identify_printer(client);
	else
	  papplClientRespondIPP(client, IPP_STATUS_ERROR_OPERATION_NOT_SUPPORTED, "Operation not supported.");
        break;

    case IPP_OP_GET_OUTPUT_DEVICE_ATTRIBUTES :
        if (client->printer->output_devices)
	  ipp_get_output_device_attributes(client);
	else
	  papplClientRespondIPP(client, IPP_STATUS_ERROR_OPERATION_NOT_SUPPORTED, "Operation not supported.");
        break;

    case IPP_OP_DEREGISTER_OUTPUT_DEVICE :
        if (client->printer->output_devices)
	  ipp_deregister_output_device(client);
	else
	  papplClientRespondIPP(client, IPP_STATUS_ERROR_OPERATION_NOT_SUPPORTED, "Operation not supported.");
        break;

    case IPP_OP_UPDATE_ACTIVE_JOBS :
        if (client->printer->output_devices)
	  ipp_update_active_jobs(client);
	else
	  papplClientRespondIPP(client, IPP_STATUS_ERROR_OPERATION_NOT_SUPPORTED, "Operation not supported.");
        break;

    case IPP_OP_UPDATE_OUTPUT_DEVICE_ATTRIBUTES :
        if (client->printer->output_devices)
	  ipp_update_output_device_attributes(client);
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
// '_papplPrinterSetAttributes()' - Set printer attributes.
//

bool					// O - `true` if OK, `false` otherwise
_papplPrinterSetAttributes(
    pappl_client_t  *client,		// I - Client
    pappl_printer_t *printer)		// I - Printer
{
  bool			create_printer;	// Create-Printer request?
  ipp_attribute_t	*rattr;		// Current request attribute
  ipp_tag_t		value_tag;	// Value tag
  size_t		count;		// Number of values
  const char		*name,		// Attribute name
			*keyword;	// Keyword value
  int			intvalue;	// Integer value
  char			defname[128],	// xxx-default name
			value[1024];	// xxx-default value
  size_t		i, j;		// Looping vars
  pwg_media_t		*pwg;		// PWG media size data
  pappl_pr_driver_data_t driver_data;	// Printer driver data
  bool			do_defaults = false,
					// Update defaults?
			do_ready = false;
					// Update ready media?
  size_t		num_vendor = 0;	// Number of vendor defaults
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
    { "copies-default",			IPP_TAG_INTEGER,	1 },
    { "label-mode-configured",		IPP_TAG_KEYWORD,	1 },
    { "label-tear-off-configured",	IPP_TAG_INTEGER,	1 },
    { "media-col-default",		IPP_TAG_BEGIN_COLLECTION, 1 },
    { "media-col-ready",		IPP_TAG_BEGIN_COLLECTION, PAPPL_MAX_SOURCE },
    { "media-default",			IPP_TAG_KEYWORD,	1 },
    { "media-ready",			IPP_TAG_KEYWORD,	PAPPL_MAX_SOURCE },
    { "multiple-document-handling-default", IPP_TAG_KEYWORD,	1 },
    { "orientation-requested-default",	IPP_TAG_ENUM,		1 },
    { "output-bin-default",		IPP_TAG_KEYWORD,	1 },
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
    { "printer-wifi-ssid",		IPP_TAG_NAME,		1 },
    { "sides-default",			IPP_TAG_KEYWORD,	1 }
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

    if (create_printer && (!strcmp(name, "printer-device-id") || !strcmp(name, "printer-name") || !strcmp(name, "smi55357-device-uri") || !strcmp(name, "smi55357-driver")))
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

    for (i = 0; i < (size_t)(sizeof(pattrs) / sizeof(pattrs[0])); i ++)
    {
      if (!strcmp(name, pattrs[i].name) && value_tag == pattrs[i].value_tag && count <= pattrs[i].max_count)
        break;
    }

    if (i >= (int)(sizeof(pattrs) / sizeof(pattrs[0])))
    {
      for (j = 0; j < (size_t)printer->driver_data.num_vendor; j ++)
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

      if (j >= (size_t)printer->driver_data.num_vendor)
        papplClientRespondIPPUnsupported(client, rattr);
    }

    // Then copy the xxx-default values to the driver data
    if (!strcmp(name, "copies-default"))
    {
      intvalue = ippGetInteger(rattr, 0);

      if (intvalue < 1 || intvalue > 999)
      {
        papplClientRespondIPP(client, IPP_STATUS_ERROR_ATTRIBUTES_OR_VALUES, "Unsupported \"copies-default\" value '%d'.", intvalue);
        papplClientRespondIPPUnsupported(client, rattr);
      }
      else
      {
	driver_data.copies_default = intvalue;
	do_defaults = true;
      }
    }
    else if (!strcmp(name, "identify-actions-default"))
    {
      pappl_identify_actions_t	identify_actions = PAPPL_IDENTIFY_ACTIONS_NONE;
					// "identify-actions" bit values

      for (i = 0, count = ippGetCount(rattr); i < count; i ++)
      {
        pappl_identify_actions_t action;// Current action

        keyword = ippGetString(rattr, i, NULL);
        action  = _papplIdentifyActionsValue(keyword);

        if (!action || !(action & driver_data.identify_supported))
        {
	  papplClientRespondIPP(client, IPP_STATUS_ERROR_ATTRIBUTES_OR_VALUES, "Unsupported \"identify-actions-default\" value '%s'.", keyword);
	  break;
        }

        identify_actions |= action;
      }

      if (i < count)
      {
        papplClientRespondIPPUnsupported(client, rattr);
      }
      else
      {
	driver_data.identify_default = identify_actions;
	do_defaults                  = true;
      }
    }
    else if (!strcmp(name, "label-mode-configured"))
    {
      pappl_label_mode_t label_mode;	// "label-mode-configured" value

      keyword    = ippGetString(rattr, 0, NULL);
      label_mode = _papplLabelModeValue(keyword);

      if (!(label_mode & driver_data.mode_supported))
      {
	papplClientRespondIPP(client, IPP_STATUS_ERROR_ATTRIBUTES_OR_VALUES, "Unsupported \"label-mode-configured\" value '%s'.", keyword);
        papplClientRespondIPPUnsupported(client, rattr);
      }
      else
      {
	driver_data.mode_configured = label_mode;
	do_defaults                 = true;
      }
    }
    else if (!strcmp(name, "label-tear-offset-configured"))
    {
      intvalue = ippGetInteger(rattr, 0);

      if (intvalue < driver_data.tear_offset_supported[0] || intvalue > driver_data.tear_offset_supported[1])
      {
	papplClientRespondIPP(client, IPP_STATUS_ERROR_ATTRIBUTES_OR_VALUES, "Unsupported \"label-tear-offset-configured\" value '%d'.", intvalue);
        papplClientRespondIPPUnsupported(client, rattr);
      }
      else
      {
	driver_data.tear_offset_configured = intvalue;
	do_defaults                        = true;
      }
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
        cupsCopyString(driver_data.media_default.size_name, pwg->pwg, sizeof(driver_data.media_default.size_name));
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
          cupsCopyString(driver_data.media_ready[i].size_name, pwg->pwg, sizeof(driver_data.media_ready[i].size_name));
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
    else if (!strcmp(name, "multiple-document-handling-default"))
    {
      pappl_handling_t	handling;	// "multiple-document-handling" bit value

      keyword  = ippGetString(rattr, 0, NULL);
      handling = _papplHandlingValue(keyword);

      if (!handling || (handling > PAPPL_HANDLING_UNCOLLATED_COPIES && !(client->system->options & PAPPL_SOPTIONS_MULTI_DOCUMENT_JOBS)))
      {
        papplClientRespondIPP(client, IPP_STATUS_ERROR_ATTRIBUTES_OR_VALUES, "Unsupported \"multiple-document-handling-default\" value '%s'.", keyword);
        papplClientRespondIPPUnsupported(client, rattr);
      }
      else
      {
        driver_data.handling_default = handling;
        do_defaults = true;
      }
    }
    else if (!strcmp(name, "orientation-requested-default"))
    {
      intvalue = ippGetInteger(rattr, 0);

      if (intvalue < IPP_ORIENT_PORTRAIT || intvalue > IPP_ORIENT_NONE)
      {
        papplClientRespondIPP(client, IPP_STATUS_ERROR_ATTRIBUTES_OR_VALUES, "Unsupported \"orientation-requested-default\" value '%d'.", intvalue);
        papplClientRespondIPPUnsupported(client, rattr);
      }
      else
      {
        driver_data.orient_default = (ipp_orient_t)intvalue;
        do_defaults                = true;
      }
    }
    else if (!strcmp(name, "output-bin-default"))
    {
      keyword = ippGetString(rattr, 0, NULL);

      for (i = 0; i < driver_data.num_bin; i ++)
      {
        if (!strcmp(keyword, driver_data.bin[i]))
        {
          driver_data.bin_default = i;
          break;
        }
      }

      if (i >= driver_data.num_bin)
      {
        papplClientRespondIPP(client, IPP_STATUS_ERROR_ATTRIBUTES_OR_VALUES, "Unsupported \"output-bin-default\" value '%s'.", keyword);
        papplClientRespondIPPUnsupported(client, rattr);
      }
      else
      {
        do_defaults = true;
      }
    }
    else if (!strcmp(name, "output-bin-default"))
    {
      keyword = ippGetString(rattr, 0, NULL);

      for (i = 0; i < driver_data.num_bin; i ++)
      {
        if (!strcmp(keyword, driver_data.bin[i]))
        {
          driver_data.bin_default = i;
          break;
        }
      }

      if (i >= driver_data.num_bin)
        papplClientRespondIPP(client, IPP_STATUS_ERROR_ATTRIBUTES_OR_VALUES, "Unsupported \"output-bin-default\" value '%s'.", keyword);
      else
        do_defaults = true;
    }
    else if (!strcmp(name, "print-color-mode-default"))
    {
      pappl_color_mode_t color_mode;	// "print-color-mode" bit value

      keyword    = ippGetString(rattr, 0, NULL);
      color_mode = _papplColorModeValue(keyword);

      if (!(color_mode & driver_data.color_supported))
      {
        papplClientRespondIPP(client, IPP_STATUS_ERROR_ATTRIBUTES_OR_VALUES, "Unsupported \"print-color-mode-default\" value '%s'.", keyword);
        papplClientRespondIPPUnsupported(client, rattr);
      }
      else
      {
	driver_data.color_default = color_mode;
	do_defaults               = true;
      }
    }
    else if (!strcmp(name, "print-content-optimize-default"))
    {
      pappl_content_t content;		// "print-content-optimize" bit value

      keyword = ippGetString(rattr, 0, NULL);
      content = _papplContentValue(keyword);

      if (!content)
      {
        papplClientRespondIPP(client, IPP_STATUS_ERROR_ATTRIBUTES_OR_VALUES, "Unsupported \"print-content-optimize-default\" value '%s'.", keyword);
        papplClientRespondIPPUnsupported(client, rattr);
      }
      else
      {
	driver_data.content_default = content;
	do_defaults                 = true;
      }
    }
    else if (!strcmp(name, "print-darkness-default"))
    {
      intvalue = ippGetInteger(rattr, 0);

      if (intvalue < 0 || intvalue > 100 || !driver_data.darkness_supported)
      {
        papplClientRespondIPP(client, IPP_STATUS_ERROR_ATTRIBUTES_OR_VALUES, "Unsupported \"print-darkness-default\" value '%d'.", intvalue);
        papplClientRespondIPPUnsupported(client, rattr);
      }
      else
      {
	driver_data.darkness_default = intvalue;
	do_defaults                  = true;
      }
    }
    else if (!strcmp(name, "print-quality-default"))
    {
      intvalue = ippGetInteger(rattr, 0);

      if (intvalue < IPP_QUALITY_DRAFT || intvalue > IPP_QUALITY_HIGH)
      {
        papplClientRespondIPP(client, IPP_STATUS_ERROR_ATTRIBUTES_OR_VALUES, "Unsupported \"print-quality-default\" value '%d'.", intvalue);
        papplClientRespondIPPUnsupported(client, rattr);
      }
      else
      {
	driver_data.quality_default = (ipp_quality_t)intvalue;
	do_defaults                 = true;
      }
    }
    else if (!strcmp(name, "print-scaling-default"))
    {
      pappl_scaling_t scaling;		// "print-scaling" bit value

      keyword = ippGetString(rattr, 0, NULL);
      scaling = _papplScalingValue(keyword);

      if (!scaling)
      {
        papplClientRespondIPP(client, IPP_STATUS_ERROR_ATTRIBUTES_OR_VALUES, "Unsupported \"print-scaling-default\" value '%s'.", keyword);
        papplClientRespondIPPUnsupported(client, rattr);
      }
      else
      {
        driver_data.scaling_default = scaling;
        do_defaults                 = true;
      }
    }
    else if (!strcmp(name, "print-speed-default"))
    {
      intvalue = ippGetInteger(rattr, 0);

      if (intvalue < driver_data.speed_supported[0] || intvalue > driver_data.speed_supported[1])
      {
        papplClientRespondIPP(client, IPP_STATUS_ERROR_ATTRIBUTES_OR_VALUES, "Unsupported \"print-speed-default\" value '%d'.", intvalue);
        papplClientRespondIPPUnsupported(client, rattr);
      }
      else
      {
	driver_data.speed_default = intvalue;
	do_defaults               = true;
      }
    }
    else if (!strcmp(name, "printer-contact-col"))
    {
      _papplContactImport(ippGetCollection(rattr, 0), &contact);
      do_contact = true;
    }
    else if (!strcmp(name, "printer-darkness-configured"))
    {
      intvalue = ippGetInteger(rattr, 0);

      if (intvalue < 0 || intvalue > 100 || !driver_data.darkness_supported)
      {
        papplClientRespondIPP(client, IPP_STATUS_ERROR_ATTRIBUTES_OR_VALUES, "Unsupported \"printer-darkness-configured\" value '%d'.", intvalue);
        papplClientRespondIPPUnsupported(client, rattr);
      }
      else
      {
	driver_data.darkness_configured = intvalue;
	do_defaults                     = true;
      }
    }
    else if (!strcmp(name, "printer-geo-location"))
    {
      float geo_lat, geo_lon;		// Latitude and longitude

      geo_location = ippGetString(rattr, 0, NULL);
      if (sscanf(geo_location, "geo:%f,%f", &geo_lat, &geo_lon) != 2 || geo_lat < -90.0 || geo_lat > 90.0 || geo_lon < -180.0 || geo_lon > 180.0)
      {
        papplClientRespondIPP(client, IPP_STATUS_ERROR_ATTRIBUTES_OR_VALUES, "Unsupported \"printer-geo-location\" value '%s'.", geo_location);
        papplClientRespondIPPUnsupported(client, rattr);
      }
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
      int	xres, yres;		// X and Y resolution
      ipp_res_t units;			// Resolution units

      xres = ippGetResolution(rattr, 0, &yres, &units);

      for (i = 0; i < driver_data.num_resolution; i ++)
      {
        if (xres == driver_data.x_resolution[i] && yres == driver_data.y_resolution[i])
          break;
      }

      if (units != IPP_RES_PER_INCH || i >= driver_data.num_resolution)
      {
        papplClientRespondIPP(client, IPP_STATUS_ERROR_ATTRIBUTES_OR_VALUES, "Unsupported \"printer-resolution-default\" value.");
        papplClientRespondIPPUnsupported(client, rattr);
      }
      else
      {
	driver_data.x_default = xres;
	driver_data.y_default = yres;
	do_defaults           = true;
      }
    }
    else if (!strcmp(name, "printer-wifi-password"))
    {
      void	*data;			// Password
      cups_len_t datalen;		// Length of password

      data = ippGetOctetString(rattr, 0, &datalen);
      if (datalen > (sizeof(wifi_password) - 1))
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
      cupsCopyString(wifi_ssid, ippGetString(rattr, 0, NULL), sizeof(wifi_ssid));
      do_wifi = true;
    }
    else if (!strcmp(name, "sides-default"))
    {
      pappl_sides_t sides;		// Sides value

      keyword = ippGetString(rattr, 0, NULL);
      sides   = _papplSidesValue(keyword);

      if (!sides || !(driver_data.sides_supported & sides))
      {
        papplClientRespondIPP(client, IPP_STATUS_ERROR_ATTRIBUTES_OR_VALUES, "Unsupported \"sides-default\" value '%s'.", keyword);
	papplClientRespondIPPUnsupported(client, rattr);
      }
      else
      {
        driver_data.sides_default = sides;
        do_defaults               = true;
      }
    }
  }

  if (ippGetStatusCode(client->response) != IPP_STATUS_OK)
  {
    cupsFreeOptions(num_vendor, vendor);
    return (false);
  }

  // Now apply changes...
  if (do_defaults && !papplPrinterSetDriverDefaults(printer, &driver_data, num_vendor, vendor))
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
  const char	*job_name,		// Job name
		*username;		// Owner


  // Get the job name/title and most authenticated user name...
  if ((job_name = ippGetString(ippFindAttribute(client->request, "job-name", IPP_TAG_NAME), 0, NULL)) == NULL)
    job_name = "Untitled";

  username = papplClientGetIPPUsername(client);

  return (_papplJobCreate(client->printer, /*job_id*/0, username, job_name, client->request));
}


//
// 'ipp_acknowledge_identify_printer()' - Acknowledge an Identify-Printer request.
//

static void
ipp_acknowledge_identify_printer(
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
  cupsRWLockRead(&printer->output_rwlock);
  od = _papplClientFindDeviceNoLock(client);
  cupsRWUnlock(&printer->output_rwlock);

  if (od)
  {
    if (od->pending_actions)
    {
      cups_len_t	i,		// Looping var
			count;		// Number of output devices
      pappl_identify_actions_t action;	// Current action
      size_t		num_actions = 0;// Number of actions
      const char	*actions[4];	// Actions

      papplClientRespondIPP(client, IPP_STATUS_OK, /*message*/NULL);

      for (action = PAPPL_IDENTIFY_ACTIONS_DISPLAY; action <= PAPPL_IDENTIFY_ACTIONS_SPEAK; action *= 2)
      {
        if (od->pending_actions & action)
          actions[num_actions ++] = _papplIdentifyActionsString(action);
      }

      if (num_actions > 0)
        ippAddStrings(client->response, IPP_TAG_OPERATION, IPP_TAG_KEYWORD, "identify-actions", num_actions, /*language*/NULL, actions);

      od->pending_actions = PAPPL_IDENTIFY_ACTIONS_NONE;

      if (od->pending_message)
      {
        ippAddString(client->response, IPP_TAG_OPERATION, IPP_TAG_TEXT, "message", /*language*/NULL, od->pending_message);
        free(od->pending_message);
        od->pending_message = NULL;
      }

      // Update the 'identify-printer-requested' keyword as needed...
      cupsRWLockRead(&printer->output_rwlock);
      for (i = 0, count = cupsArrayGetCount(printer->output_devices); i < count; i ++)
      {
        od = (_pappl_odevice_t *)cupsArrayGetElement(printer->output_devices, i);

        if (od->pending_actions)
          break;
      }
      cupsRWUnlock(&printer->output_rwlock);

      if (i >= count)
      {
        // No more pending Identify-Printer requests...
        printer->state_reasons &= (unsigned)~PAPPL_PREASON_IDENTIFY_PRINTER_REQUESTED;

        _papplSystemAddEventNoLock(printer->system, printer, /*job*/NULL, PAPPL_EVENT_PRINTER_STATE_CHANGED, /*message*/NULL);
      }
    }
    else
    {
      papplClientRespondIPP(client, IPP_STATUS_ERROR_NOT_POSSIBLE, "No pending Identify-Printer requests.");
    }
  }

  _papplRWUnlock(printer);
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
  if (!valid_job_attributes(client, NULL))
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

  _papplJobCopyAttributesNoLock(job, client, ra, /*include_status*/true);
  cupsArrayDelete(ra);
}


//
// 'ipp_deregister_output_device()' - Deregister an output device.
//

static void
ipp_deregister_output_device(
    pappl_client_t *client)		// I - Client
{
  pappl_printer_t	*printer = client->printer;
					// Printer
  pappl_system_t	*system = client->system;
					// System
  _pappl_odevice_t	*od;		// Output device
  bool			keep = true;	// Keep the printer?
  pappl_event_t		events = PAPPL_EVENT_NONE;
					// Notification event(s)


  // Authorize access...
  if (!_papplPrinterIsAuthorized(client))
    return;

  // Find the output device
  _papplRWLockRead(printer);
  cupsRWLockWrite(&printer->output_rwlock);

  if ((od = _papplClientFindDeviceNoLock(client)) != NULL)
  {
    pappl_job_t	*job;			// Current job
    size_t	i,			// Looping var
		count;			// Number of jobs

    // Determine whether the printer will be kept...
    if (system->deregister_cb)
      keep = (system->deregister_cb)(client, od->device_uuid, printer, system->register_cbdata);
    else
      keep = cupsArrayGetCount(printer->output_devices) == 1;

    // Unassign jobs as needed...
    for (i = 0, count = cupsArrayGetCount(printer->all_jobs); i < count; i ++)
    {
      job = (pappl_job_t *)cupsArrayGetElement(printer->all_jobs, i);

      _papplRWLockWrite(job);
      if (job->output_device == od)
        job->output_device = NULL;
      _papplRWUnlock(job);
    }

    // Remove the output device from the array...
    cupsArrayRemove(printer->output_devices, od);
    events |= PAPPL_EVENT_PRINTER_CONFIG_CHANGED;

    // Return "ok"...
    papplClientRespondIPP(client, IPP_STATUS_OK, /*message*/NULL);
  }

  cupsRWUnlock(&printer->output_rwlock);
  _papplRWUnlock(printer);

  if (keep)
  {
    // Keep printer...
    if (ippGetStatusCode(client->response) == IPP_STATUS_OK)
    {
      // Update attributes based on the new device attributes...
      _papplPrinterUpdateInfra(printer);
    }
  }
  else
  {
    // Delete printer...
    papplPrinterDelete(printer);
    printer = NULL;
    events |= PAPPL_EVENT_PRINTER_DELETED;
  }

  if (events)
    papplSystemAddEvent(system, printer, /*job*/NULL, events, "Output device deregistered.");
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
  pappl_jreason_t	job_reasons;	// job-state-reasons value
  size_t		i,		// Looping var
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
    job_reasons    = PAPPL_JREASON_NONE;
    list           = client->printer->active_jobs;
  }
  else if (!strcmp(which_jobs, "completed"))
  {
    job_comparison = 1;
    job_state      = IPP_JSTATE_CANCELED;
    job_reasons    = PAPPL_JREASON_NONE;
    list           = client->printer->completed_jobs;
  }
  else if (!strcmp(which_jobs, "all"))
  {
    job_comparison = 1;
    job_state      = IPP_JSTATE_PENDING;
    job_reasons    = PAPPL_JREASON_NONE;
    list           = client->printer->all_jobs;
  }
  else if (!strcmp(which_jobs, "fetchable"))
  {
    job_comparison = -1;
    job_state      = IPP_JSTATE_STOPPED;
    job_reasons    = PAPPL_JREASON_JOB_FETCHABLE;
    list           = client->printer->active_jobs;
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
      limit = (size_t)temp;
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
      username = papplClientGetIPPUsername(client);
      papplLogClient(client, PAPPL_LOGLEVEL_DEBUG, "Get-Jobs \"requesting-user-name\"='%s'", username);
    }
  }

  // OK, build a list of jobs for this printer...
  ra = ippCreateRequestedArray(client->request);

  papplClientRespondIPP(client, IPP_STATUS_OK, NULL);

  _papplRWLockRead(client->printer);

  count = cupsArrayGetCount(list);
  if (limit == 0 || limit > count)
    limit = count;

  for (count = 0, i = 0; i < limit; i ++)
  {
    job = (pappl_job_t *)cupsArrayGetElement(list, i);

    // Filter out jobs that don't match...
    if ((job_comparison < 0 && job->state > job_state) || /* (job_comparison == 0 && job->state != job_state) || */ (job_comparison > 0 && job->state < job_state) || (username && job->username && strcasecmp(username, job->username)))
      continue;

    if (job_reasons && !(job->state_reasons & job_reasons))
      continue;

    if (count > 0)
      ippAddSeparator(client->response);

    count ++;
    _papplJobCopyAttributesNoLock(job, client, ra, /*include_status*/true);
  }

  cupsArrayDelete(ra);

  _papplRWUnlock(client->printer);
}


//
// 'ipp_get_output_device_attributes()' - Get output device attributes.
//

static void
ipp_get_output_device_attributes(
    pappl_client_t *client)		// I - Client
{
  cups_array_t		*ra;		// Requested attributes array
  pappl_printer_t	*printer = client->printer;
					// Printer
  _pappl_odevice_t	*od;		// Output device


  // Authorize access...
  if (!_papplPrinterIsAuthorized(client))
    return;

  // Find the output device
  _papplRWLockRead(printer);
  cupsRWLockRead(&printer->output_rwlock);
  od = _papplClientFindDeviceNoLock(client);
  cupsRWUnlock(&printer->output_rwlock);

  if (od)
  {
    // Send the attributes...
    papplClientRespondIPP(client, IPP_STATUS_OK, NULL);

    ra = ippCreateRequestedArray(client->request);

    _papplCopyAttributes(client->response, od->device_attrs, ra, IPP_TAG_PRINTER, false);

    cupsArrayDelete(ra);
  }

  _papplRWUnlock(printer);
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


  _papplRWLockRead(printer->system);
  _papplRWLockRead(printer);

  if (!printer->device_in_use && !printer->processing_job && (time(NULL) - printer->status_time) > 1 && printer->driver_data.status_cb)
  {
    // Update printer status...
    _papplRWUnlock(printer);
    _papplRWUnlock(printer->system);

    (printer->driver_data.status_cb)(printer);

    _papplRWLockRead(printer->system);
    _papplRWLockWrite(printer);

    printer->status_time = time(NULL);
  }

  // Send the attributes...
  ra = ippCreateRequestedArray(client->request);

  papplClientRespondIPP(client, IPP_STATUS_OK, NULL);

  _papplPrinterCopyAttributesNoLock(printer, client, ra, ippGetString(ippFindAttribute(client->request, "document-format", IPP_TAG_MIMETYPE), 0, NULL));
  _papplRWUnlock(printer);
  _papplRWUnlock(printer->system);

  cupsArrayDelete(ra);
}


//
// 'ipp_hold_new_jobs()' - Hold new jobs.
//

static void
ipp_hold_new_jobs(
    pappl_client_t *client)		// I - Client
{
  http_status_t	auth_status;		// Authorization status


  // Verify the connection is authorized...
  if ((auth_status = papplClientIsAuthorized(client)) != HTTP_STATUS_CONTINUE)
  {
    papplClientRespond(client, auth_status, NULL, NULL, 0, 0);
    return;
  }

  // Hold new jobs...
  if (papplPrinterHoldNewJobs(client->printer))
    papplClientRespondIPP(client, IPP_STATUS_OK, "New jobs being held.");
  else
    papplClientRespondIPP(client, IPP_STATUS_ERROR_NOT_POSSIBLE, "Jobs already being held.");
}


//
// 'ipp_identify_printer()' - Beep or display a message.
//

static void
ipp_identify_printer(
    pappl_client_t *client)		// I - Client
{
  pappl_printer_t	*printer = client->printer;
					// Printer
  size_t		i;		// Looping var
  ipp_attribute_t	*attr;		// IPP attribute
  pappl_identify_actions_t actions;	// "identify-actions" value
  const char		*message;	// "message" value



  // Get request attributes...
  if ((attr = ippFindAttribute(client->request, "identify-actions", IPP_TAG_KEYWORD)) != NULL)
  {
    actions = PAPPL_IDENTIFY_ACTIONS_NONE;

    for (i = 0; i < ippGetCount(attr); i ++)
      actions |= _papplIdentifyActionsValue(ippGetString(attr, i, NULL));
  }
  else
  {
    actions = printer->driver_data.identify_default;
  }

  if ((attr = ippFindAttribute(client->request, "message", IPP_TAG_TEXT)) != NULL)
    message = ippGetString(attr, 0, NULL);
  else
    message = NULL;

  if (printer->output_devices)
  {
    // Save the identification request for the Proxy...
    _pappl_odevice_t	*od;		// Output device

    // Find the output device, if any
    _papplRWLockWrite(printer);
    cupsRWLockRead(&printer->output_rwlock);
    od = _papplClientFindDeviceNoLock(client);

    if (od)
    {
      // Save actions/message for this device...
      od->pending_actions |= actions;
      if (message)
      {
        free(od->pending_message);
        od->pending_message = strdup(message);
      }
    }
    else
    {
      // No device specified, make this pending for all devices...
      cups_len_t	j,		// Looping var
			count;		// Number of devices

      for (j = 0, count = cupsArrayGetCount(printer->output_devices); j < count; j ++)
      {
        od = (_pappl_odevice_t *)cupsArrayGetElement(printer->output_devices, j);

	od->pending_actions |= actions;
	if (message)
	{
	  free(od->pending_message);
	  od->pending_message = strdup(message);
	}
      }
    }

    cupsRWUnlock(&printer->output_rwlock);
    _papplRWUnlock(printer);

    // Add 'identify-printer-requested' to the "printer-state-reasons" values...
    printer->state_reasons |= PAPPL_PREASON_IDENTIFY_PRINTER_REQUESTED;

    _papplSystemAddEventNoLock(printer->system, printer, /*job*/NULL, PAPPL_EVENT_PRINTER_STATE_CHANGED, "Identify-Printer requested.");
  }
  else if (printer->driver_data.identify_cb)
  {
    // Have the driver handle identification...
    (printer->driver_data.identify_cb)(client->printer, actions, message);
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
  const char		*format;	// Document format


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
  if (!papplPrinterIsAcceptingJobs(client->printer))
  {
    _papplClientFlushDocumentData(client);
    papplClientRespondIPP(client, IPP_STATUS_ERROR_NOT_ACCEPTING_JOBS, "Not accepting new jobs.");
    return;
  }

  // Validate print job attributes...
  if (!valid_job_attributes(client, &format))
  {
    _papplClientFlushDocumentData(client);
    return;
  }

  // Create the job...
  if ((job = create_job(client)) == NULL)
  {
    _papplClientFlushDocumentData(client);
    papplClientRespondIPP(client, IPP_STATUS_ERROR_BUSY, "Currently printing another job.");
    return;
  }

  // Then finish getting the document data and process things...
  _papplJobCopyDocumentData(client, job, format, /*last_document*/true);
}


//
// 'ipp_release_held_new_jobs()' - Release held (new) jobs.
//

static void
ipp_release_held_new_jobs(
    pappl_client_t *client)		// I - Client
{
  http_status_t	auth_status;		// Authorization status


  // Verify the connection is authorized...
  if ((auth_status = papplClientIsAuthorized(client)) != HTTP_STATUS_CONTINUE)
  {
    papplClientRespond(client, auth_status, NULL, NULL, 0, 0);
    return;
  }

  // Hold new jobs...
  if (papplPrinterReleaseHeldNewJobs(client->printer, client->username))
    papplClientRespondIPP(client, IPP_STATUS_OK, "Released all held jobs.");
  else
    papplClientRespondIPP(client, IPP_STATUS_ERROR_NOT_POSSIBLE, "Jobs not being held.");
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
// 'ipp_update_active_jobs()' - Update output device attributes.
//

static void
ipp_update_active_jobs(
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
  cupsRWLockRead(&printer->output_rwlock);
  od = _papplClientFindDeviceNoLock(client);
  cupsRWUnlock(&printer->output_rwlock);

  if (od)
  {
    // Get required attributes...
    size_t		i,		// Looping var
			count;		// Number of values
    ipp_attribute_t	*job_ids,	// "job-ids" attribute
			*job_states;	// "output-device-job-states" attribute
    pappl_job_t		*job;		// Current job
    ipp_jstate_t	job_state;	// Current job state
    const char		*device_uuid = od->device_uuid;
					// "output-device-uuid" value

    if ((job_ids = ippFindAttribute(client->request, "job-ids", IPP_TAG_ZERO)) != NULL && (ippGetGroupTag(job_ids) != IPP_TAG_OPERATION || ippGetValueTag(job_ids) != IPP_TAG_INTEGER))
    {
      papplClientRespondIPPUnsupported(client, job_ids);
      job_ids = NULL;
    }

    count = ippGetCount(job_ids);

    if ((job_states = ippFindAttribute(client->request, "output-device-job-states", IPP_TAG_ZERO)) == NULL && job_ids != NULL)
    {
      papplClientRespondIPP(client, IPP_STATUS_ERROR_BAD_REQUEST, "Missing \"output-device-job-states\" operation attribute.");
    }
    else if (job_states && !job_ids)
    {
      papplClientRespondIPP(client, IPP_STATUS_ERROR_BAD_REQUEST, "Missing \"job-ids\" operation attribute.");
    }
    else if (job_states && (ippGetGroupTag(job_states) != IPP_TAG_OPERATION || ippGetValueTag(job_states) != IPP_TAG_ENUM || ippGetCount(job_states) != count))
    {
      papplClientRespondIPPUnsupported(client, job_ids);
      job_states = NULL;
    }

    if (job_ids && job_states)
    {
      // Valid attributes, update job states...
      size_t	num_unsup = 0;		// Number of bad/unsupported job IDs
      int	unsup_ids[1000];	// Unsupported job-id values
      size_t	num_updates = 0;	// Number of updates that are different
      int	update_ids[1000],	// Updated job-id values
		update_states[1000];	// Updated job-state values

      for (i = 0; i < count; i ++)
      {
        job       = _papplPrinterFindJobNoLock(printer, ippGetInteger(job_ids, i));
        job_state = (ipp_jstate_t)ippGetInteger(job_states, i);

        if (!job)
        {
          // Job not found...
          if (num_unsup < (sizeof(unsup_ids) / sizeof(unsup_ids[0])))
            unsup_ids[num_unsup ++] = ippGetInteger(job_ids, i);
	  continue;
        }
        else
        {
          // Job found...
          _papplRWLockWrite(job);
          if (!job->output_device || strcmp(device_uuid, job->output_device->device_uuid))
          {
            // Not assigned to this output device...
	    if (num_unsup < (sizeof(unsup_ids) / sizeof(unsup_ids[0])))
	      unsup_ids[num_unsup ++] = ippGetInteger(job_ids, i);
          }
          else
          {
            // Assigned, update state...
            if ((job->state >= IPP_JSTATE_CANCELED || job->is_canceled) && job_state < IPP_JSTATE_CANCELED)
            {
              // Local job is already terminated, report this back to the proxy...
              if (num_updates < (sizeof(update_ids) / sizeof(update_ids[0])))
              {
		update_ids[num_updates]       = job->job_id;
		update_states[num_updates ++] = (int)job->state;
              }
            }
            else if (job->state != job_state)
            {
              // Update state
              _papplJobSetStateNoLock(job, job_state);
            }
          }
          _papplRWUnlock(job);
        }
      }

      // Look for new jobs that the proxy didn't provide...
      for (i = 0, count = cupsArrayGetCount(printer->active_jobs); i < count && num_updates < (sizeof(update_ids) / sizeof(update_ids[0])); i ++)
      {
        job = (pappl_job_t *)cupsArrayGetElement(printer->active_jobs, i);

	_papplRWLockRead(job);
        if (!strcmp(device_uuid, job->output_device->device_uuid) && !ippContainsInteger(job_ids, job->job_id))
        {
          update_ids[num_updates]       = job->job_id;
          update_states[num_updates ++] = (int)job->state;
        }
	_papplRWUnlock(job);
      }

      // If we get this far without an error, return successful-ok...
      if (ippGetStatusCode(client->response) == IPP_STATUS_OK)
	papplClientRespondIPP(client, IPP_STATUS_OK, NULL);

      if (num_updates > 0)
      {
	ippAddIntegers(client->response, IPP_TAG_OPERATION, IPP_TAG_INTEGER, "job-ids", num_updates, update_ids);
	ippAddIntegers(client->response, IPP_TAG_OPERATION, IPP_TAG_ENUM, "output-device-job-states", num_updates, update_states);
      }

      if (num_unsup > 0)
	ippAddIntegers(client->response, IPP_TAG_UNSUPPORTED_GROUP, IPP_TAG_INTEGER, "job-ids", num_unsup, unsup_ids);
    }
  }

  _papplRWUnlock(printer);
}


//
// 'ipp_update_output_device_attributes()' - Update output device attributes.
//

static void
ipp_update_output_device_attributes(
    pappl_client_t *client)		// I - Client
{
  pappl_printer_t	*printer = client->printer;
					// Printer
  _pappl_odevice_t	*od;		// Output device
  pappl_event_t		events = PAPPL_EVENT_NONE;
					// Notification event(s)


  // Authorize access...
  if (!_papplPrinterIsAuthorized(client))
    return;

  // Find the output device
  _papplRWLockWrite(printer);
  cupsRWLockRead(&printer->output_rwlock);
  od = _papplClientFindDeviceNoLock(client);
  cupsRWUnlock(&printer->output_rwlock);

  if (!od)
  {
    const char		*device_uuid = ippGetString(ippFindAttribute(client->request, "output-device-uuid", IPP_TAG_URI), 0, NULL);
					// output-device-uuid value

    _papplRWUnlock(printer);

    if (device_uuid && (client->system->register_cb)(client, device_uuid, printer, client->system->register_cbdata) == printer)
      od = _papplClientFindDeviceNoLock(client);

    _papplRWLockWrite(printer);
  }

  if (od)
  {
    // Update the attributes...
    ipp_attribute_t	*attr,		// Current attribute
			*old_attr;	// Old attribute

    if (!od->device_attrs)
      od->device_attrs = ippNew();

    for (attr = ippGetFirstAttribute(client->request); attr; attr = ippGetNextAttribute(client->request))
    {
      const char	*name = ippGetName(attr),
					// Attribute name
      			*nameptr;	// Pointer into name
      ipp_tag_t		value_tag = ippGetValueTag(attr);
					// Syntax/tag for attribute

      // Only update printer attributes...
      if (!name || ippGetGroupTag(attr) != IPP_TAG_PRINTER)
        continue;

      // Update this attribute...
      if (!strncmp(name, "printer-alert", 13) || !strncmp(name, "printer-finisher", 16) || !strcmp(name, "printer-input-tray") || !strcmp(name, "printer-is-accepting-jobs") || !strcmp(name, "printer-output-tray") || !strncmp(name, "printer-state", 13) || !strncmp(name, "printer-supply", 14))
        events |= PAPPL_EVENT_PRINTER_STATE_CHANGED;
      else
        events |= PAPPL_EVENT_PRINTER_CONFIG_CHANGED;

      if ((nameptr = strchr(name, '.')) != NULL && isdigit(nameptr[1] & 255))
      {
        // Sparse update - name.NNN or name.SSS-EEE
        int	start, end;		// Start and end indices
        size_t	i,			// Looping var
		count,			// New number of values
		old_count,		// Original number of values
		range_count;		// Number of indices in range
      	char	tempname[256],		// Temporary attribute name
      		*tempptr;		// Pointer into temporary name

        // Get range...
        start = (int)strtol(nameptr + 1, (char **)&nameptr, 10);
        if (nameptr && *nameptr == '-')
          end = (int)strtol(nameptr + 1, NULL, 10);
        else
          end = start;

        if (start < 1 || start > end)
        {
          // Bad range...
          papplClientRespondIPPUnsupported(client, attr);
          continue;
        }

        start --;
        end --;
        range_count = (size_t)(end - start + 1);

        // Get base attribute...
        cupsCopyString(tempname, name, sizeof(tempname));
        if ((tempptr = strchr(tempname, '.')) != NULL)
          *tempptr = '\0';		// Truncate range from name

        if ((old_attr = ippFindAttribute(od->device_attrs, tempname, IPP_TAG_ZERO)) == NULL)
        {
          // Attribute not found...
          papplClientRespondIPPUnsupported(client, attr);
          continue;
	}

	if (value_tag != ippGetValueTag(old_attr) && value_tag != IPP_TAG_DELETEATTR)
	{
          // Attribute syntax doesn't match...
          papplClientRespondIPPUnsupported(client, attr);
          continue;
	}

	if (value_tag == IPP_TAG_DELETEATTR)
	{
	  // Delete values
	  ippDeleteValues(od->device_attrs, &old_attr, (size_t)start, range_count);
	  continue;
        }

        // Update values
        count     = ippGetCount(attr);
        old_count = ippGetCount(old_attr);

	if ((size_t)start < old_count && count < range_count)
	{
	  // Delete one or more values...
	  ippDeleteValues(od->device_attrs, &old_attr, (size_t)start, range_count - count);
	}
	else if ((size_t)end < old_count && count > range_count)
	{
	  // Insert one or more values...
	  size_t	offset = count - range_count;
				      // Offset for new values

	  switch (value_tag)
	  {
	    default :
		break;

	    case IPP_TAG_BOOLEAN :
		for (i = old_count - 1; i >= (size_t)end; i --)
		  ippSetBoolean(od->device_attrs, &old_attr, i + offset, ippGetBoolean(old_attr, i));
		break;

	    case IPP_TAG_INTEGER :
	    case IPP_TAG_ENUM :
		for (i = old_count - 1; i >= (size_t)end; i --)
		  ippSetInteger(od->device_attrs, &old_attr, i + offset, ippGetInteger(old_attr, i));
		break;

	    case IPP_TAG_STRING :
		for (i = old_count - 1; i >= (size_t)end; i --)
		{
		  cups_len_t	datalen;// Length of string
		  void		*data = ippGetOctetString(old_attr, i, &datalen);
					// String

		  ippSetOctetString(od->device_attrs, &old_attr, i + offset, data, datalen);
		}
		break;

	    case IPP_TAG_DATE :
		for (i = old_count - 1; i >= (size_t)end; i --)
		  ippSetDate(od->device_attrs, &old_attr, i + offset, ippGetDate(old_attr, i));
		break;

	    case IPP_TAG_RESOLUTION :
		for (i = old_count - 1; i >= (size_t)end; i --)
		{
		  int		xres,	// X resolution
				yres;	// Y resolution
		  ipp_res_t	units;	// Units

		  xres = ippGetResolution(old_attr, i, &yres, &units);
		  ippSetResolution(od->device_attrs, &old_attr, i + offset, units, xres, yres);
		}
		break;

	    case IPP_TAG_RANGE :
		for (i = old_count - 1; i >= (size_t)end; i --)
		{
		  int upper, lower = ippGetRange(old_attr, i, &upper);
				      // Range

		  ippSetRange(od->device_attrs, &old_attr, i + offset, lower, upper);
		}
		break;

	    case IPP_TAG_BEGIN_COLLECTION :
		for (i = old_count - 1; i >= (size_t)end; i --)
		  ippSetCollection(od->device_attrs, &old_attr, i + offset, ippGetCollection(old_attr, i));
		break;

	    case IPP_TAG_TEXTLANG :
	    case IPP_TAG_NAMELANG :
	    case IPP_TAG_TEXT :
	    case IPP_TAG_NAME :
	    case IPP_TAG_KEYWORD :
	    case IPP_TAG_URI :
	    case IPP_TAG_URISCHEME :
	    case IPP_TAG_CHARSET :
	    case IPP_TAG_LANGUAGE :
	    case IPP_TAG_MIMETYPE :
		for (i = old_count - 1; i >= (size_t)end; i --)
		  ippSetString(od->device_attrs, &old_attr, i + offset, ippGetString(old_attr, i, NULL));
		break;
	  }
	}

	switch (value_tag)
	{
	  default :
	      // Syntax not supported...
	      papplClientRespondIPPUnsupported(client, attr);
	      break;

	  case IPP_TAG_INTEGER :
	  case IPP_TAG_ENUM :
	      for (i = (size_t)end; i >= (size_t)start; i --)
		ippSetInteger(od->device_attrs, &old_attr, i, ippGetInteger(attr, i - (size_t)start));
	      break;

	  case IPP_TAG_BOOLEAN :
	      for (i = (size_t)end; i >= (size_t)start; i --)
		ippSetBoolean(od->device_attrs, &old_attr, i, ippGetBoolean(attr, i - (size_t)start));
	      break;

	  case IPP_TAG_STRING :
	      for (i = (size_t)end; i >= (size_t)start; i --)
	      {
		cups_len_t datalen;	// Length of string
		void *data = ippGetOctetString(attr, i - (size_t)start, &datalen);
					// String

		ippSetOctetString(od->device_attrs, &old_attr, i, data, datalen);
	      }
	      break;

	  case IPP_TAG_DATE :
	      for (i = (size_t)end; i >= (size_t)start; i --)
		ippSetDate(od->device_attrs, &old_attr, i, ippGetDate(attr, i - (size_t)start));
	      break;

	  case IPP_TAG_RESOLUTION :
	      for (i = (size_t)end; i >= (size_t)start; i --)
	      {
		int		xres,	// X resolution
				yres;	// Y resolution
		ipp_res_t	units;	// Units

		xres = ippGetResolution(attr, i - (size_t)start, &yres, &units);
		ippSetResolution(od->device_attrs, &old_attr, i, units, xres, yres);
	      }
	      break;

	  case IPP_TAG_RANGE :
	      for (i = (size_t)end; i >= (size_t)start; i --)
	      {
		int upper, lower = ippGetRange(attr, i - (size_t)start, &upper);
					// Range

		ippSetRange(od->device_attrs, &old_attr, i, lower, upper);
	      }
	      break;

	  case IPP_TAG_BEGIN_COLLECTION :
	      for (i = (size_t)end; i >= (size_t)start; i --)
		ippSetCollection(od->device_attrs, &old_attr, i, ippGetCollection(attr, i - (size_t)start));
	      break;

	  case IPP_TAG_TEXTLANG :
	  case IPP_TAG_NAMELANG :
	  case IPP_TAG_TEXT :
	  case IPP_TAG_NAME :
	  case IPP_TAG_KEYWORD :
	  case IPP_TAG_URI :
	  case IPP_TAG_URISCHEME :
	  case IPP_TAG_CHARSET :
	  case IPP_TAG_LANGUAGE :
	  case IPP_TAG_MIMETYPE :
	      for (i = (size_t)end; i >= (size_t)start; i --)
		ippSetString(od->device_attrs, &old_attr, i, ippGetString(attr, i - (size_t)start, NULL));
	      break;
	}
      }
      else
      {
        // Add/replace
        if ((old_attr = ippFindAttribute(od->device_attrs, name, IPP_TAG_ZERO)) != NULL)
          ippDeleteAttribute(od->device_attrs, old_attr);

	if (ippGetValueTag(attr) != IPP_TAG_DELETEATTR)
	  ippCopyAttribute(od->device_attrs, attr, /*quickcopy*/false);
      }
    }

    // If we get this far without an error, return successful-ok...
    if (ippGetStatusCode(client->response) == IPP_STATUS_OK)
      papplClientRespondIPP(client, IPP_STATUS_OK, NULL);
  }
  else
  {
    papplClientRespondIPP(client, IPP_STATUS_ERROR_NOT_AUTHORIZED, "Output device not authorized for printer.");
  }

  _papplRWUnlock(printer);

  if (ippGetStatusCode(client->response) == IPP_STATUS_OK)
  {
    // Update attributes based on the new device attributes...
    _papplPrinterUpdateInfra(printer);

    if (events)
      papplSystemAddEvent(printer->system, printer, /*job*/NULL, events, "Output device attributes updated.");
  }
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

  if (valid_job_attributes(client, NULL))
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
    pappl_client_t *client,		// I - Client
    const char     **format)		// O - Document format
{
  size_t		i,		// Looping var
			count;		// Number of values
  bool			valid = true,	// Valid attributes?
			exact;		// Need attribute fidelity?
  ipp_attribute_t	*attr,		// Current attribute
			*supported;	// xxx-supported attribute


  // If a shutdown is pending, do not accept more jobs...
  if (client->system->shutdown_time)
  {
    papplClientRespondIPP(client, IPP_STATUS_ERROR_NOT_ACCEPTING_JOBS, "Not accepting new jobs.");
    return (false);
  }

  // Check operation attributes...
  valid = _papplJobValidateDocumentAttributes(client, format);

  _papplRWLockRead(client->printer);

  // Check the various job template attributes...
  exact = ippGetOperation(client->request) == IPP_OP_VALIDATE_JOB;

  if ((attr = ippFindAttribute(client->request, "ipp-attribute-fidelity", IPP_TAG_ZERO)) != NULL)
  {
    if (ippGetCount(attr) != 1 || ippGetValueTag(attr) != IPP_TAG_BOOLEAN)
    {
      papplClientRespondIPPUnsupported(client, attr);
      valid = false;
    }

    if (ippGetBoolean(attr, 0))
      exact = true;
  }

  if ((attr = ippFindAttribute(client->request, "copies", IPP_TAG_ZERO)) != NULL)
  {
    if (ippGetCount(attr) != 1 || ippGetValueTag(attr) != IPP_TAG_INTEGER || ippGetInteger(attr, 0) < 1 || ippGetInteger(attr, 0) > 999)
    {
      papplClientRespondIPPUnsupported(client, attr);
      valid = false;
    }
  }

  if ((attr = ippFindAttribute(client->request, "job-hold-until", IPP_TAG_ZERO)) != NULL)
  {
    if (ippGetCount(attr) != 1 || (ippGetValueTag(attr) != IPP_TAG_NAME && ippGetValueTag(attr) != IPP_TAG_NAMELANG && ippGetValueTag(attr) != IPP_TAG_KEYWORD))
    {
      papplClientRespondIPPUnsupported(client, attr);
      valid = false;
    }

    if ((supported = ippFindAttribute(client->printer->attrs, "job-hold-until", IPP_TAG_KEYWORD)) != NULL && !ippContainsString(supported, ippGetString(attr, 0, NULL)))
    {
      if (exact)
      {
        papplClientRespondIPPUnsupported(client, attr);
        valid = false;
      }
      else
      {
        _papplClientRespondIPPIgnored(client, attr);
	ippDeleteAttribute(client->request, attr);
      }
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
  {
    ippAddString(client->request, IPP_TAG_JOB, IPP_TAG_NAME, "job-name", NULL, "Untitled");
  }

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
    if (ippGetCount(attr) != 1 || (ippGetValueTag(attr) != IPP_TAG_NAME && ippGetValueTag(attr) != IPP_TAG_NAMELANG && ippGetValueTag(attr) != IPP_TAG_KEYWORD) || (exact && strcmp(ippGetString(attr, 0, NULL), "none")))
    {
      papplClientRespondIPPUnsupported(client, attr);
      valid = false;
    }
    else if (strcmp(ippGetString(attr, 0, NULL), "none"))
    {
      _papplClientRespondIPPIgnored(client, attr);
      ippDeleteAttribute(client->request, attr);
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
        if (exact)
        {
	  papplClientRespondIPPUnsupported(client, attr);
	  valid = false;
	}
	else
	{
	  _papplClientRespondIPPIgnored(client, attr);
	  ippDeleteAttribute(client->request, attr);
	}
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
	  if (exact)
	  {
	    papplClientRespondIPPUnsupported(client, attr);
	    valid = false;
	  }
	  else
	  {
	    _papplClientRespondIPPIgnored(client, attr);
	    ippDeleteAttribute(client->request, attr);
	  }
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
	    if (exact)
	    {
	      papplClientRespondIPPUnsupported(client, attr);
	      valid = false;
	    }
	    else
	    {
	      _papplClientRespondIPPIgnored(client, attr);
	      ippDeleteAttribute(client->request, attr);
	    }
	  }
	}
      }
    }
  }

  if ((attr = ippFindAttribute(client->request, "multiple-document-handling", IPP_TAG_ZERO)) != NULL)
  {
    if (ippGetCount(attr) != 1 || ippGetValueTag(attr) != IPP_TAG_KEYWORD)
    {
      papplClientRespondIPPUnsupported(client, attr);
      valid = false;
    }
    else if (strcmp(ippGetString(attr, 0, NULL), "separate-documents-uncollated-copies") && strcmp(ippGetString(attr, 0, NULL), "separate-documents-collated-copies"))
    {
      if (exact)
      {
	papplClientRespondIPPUnsupported(client, attr);
	valid = false;
      }
      else
      {
	_papplClientRespondIPPIgnored(client, attr);
	ippDeleteAttribute(client->request, attr);
      }
    }
  }

  if ((attr = ippFindAttribute(client->request, "orientation-requested", IPP_TAG_ZERO)) != NULL)
  {
    if (ippGetCount(attr) != 1 || ippGetValueTag(attr) != IPP_TAG_ENUM)
    {
      papplClientRespondIPPUnsupported(client, attr);
      valid = false;
    }
    else if (ippGetInteger(attr, 0) < IPP_ORIENT_PORTRAIT || ippGetInteger(attr, 0) > IPP_ORIENT_NONE)
    {
      if (exact)
      {
	papplClientRespondIPPUnsupported(client, attr);
	valid = false;
      }
      else
      {
	_papplClientRespondIPPIgnored(client, attr);
	ippDeleteAttribute(client->request, attr);
      }
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

    if (ippGetCount(attr) != 1 || ippGetValueTag(attr) != IPP_TAG_KEYWORD)
    {
      papplClientRespondIPPUnsupported(client, attr);
      valid = false;
    }
    else if (!(value & client->printer->driver_data.color_supported))
    {
      if (exact)
      {
	papplClientRespondIPPUnsupported(client, attr);
	valid = false;
      }
      else
      {
	_papplClientRespondIPPIgnored(client, attr);
	ippDeleteAttribute(client->request, attr);
      }
    }
  }

  if ((attr = ippFindAttribute(client->request, "print-content-optimize", IPP_TAG_ZERO)) != NULL)
  {
    if (ippGetCount(attr) != 1 || ippGetValueTag(attr) != IPP_TAG_KEYWORD)
    {
      papplClientRespondIPPUnsupported(client, attr);
      valid = false;
    }
    else if (!_papplContentValue(ippGetString(attr, 0, NULL)))
    {
      if (exact)
      {
	papplClientRespondIPPUnsupported(client, attr);
	valid = false;
      }
      else
      {
	_papplClientRespondIPPIgnored(client, attr);
	ippDeleteAttribute(client->request, attr);
      }
    }
  }

  if ((attr = ippFindAttribute(client->request, "print-darkness", IPP_TAG_ZERO)) != NULL)
  {
    int value = ippGetInteger(attr, 0);	// "print-darkness" value

    if (ippGetCount(attr) != 1 || ippGetValueTag(attr) != IPP_TAG_INTEGER || value < -100 || value > 100)
    {
      papplClientRespondIPPUnsupported(client, attr);
      valid = false;
    }
    else if (client->printer->driver_data.darkness_supported == 0)
    {
      if (exact)
      {
	papplClientRespondIPPUnsupported(client, attr);
	valid = false;
      }
      else
      {
	_papplClientRespondIPPIgnored(client, attr);
	ippDeleteAttribute(client->request, attr);
      }
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

    if (ippGetCount(attr) != 1 || ippGetValueTag(attr) != IPP_TAG_INTEGER)
    {
      papplClientRespondIPPUnsupported(client, attr);
      valid = false;
    }
    else if (value < client->printer->driver_data.speed_supported[0] || value > client->printer->driver_data.speed_supported[1] || client->printer->driver_data.speed_supported[1] == 0)
    {
      if (exact)
      {
	papplClientRespondIPPUnsupported(client, attr);
	valid = false;
      }
      else
      {
	_papplClientRespondIPPIgnored(client, attr);
	ippDeleteAttribute(client->request, attr);
      }
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
      for (i = 0; i < (size_t)client->printer->driver_data.num_resolution; i ++)
      {
        if (xdpi == client->printer->driver_data.x_resolution[i] && ydpi == client->printer->driver_data.y_resolution[i])
          break;
      }

      if (i >= (size_t)client->printer->driver_data.num_resolution)
      {
        if (exact)
        {
	  papplClientRespondIPPUnsupported(client, attr);
	  valid = false;
	}
	else
	{
	  _papplClientRespondIPPIgnored(client, attr);
	  ippDeleteAttribute(client->request, attr);
	}
      }
    }
  }

  if ((attr = ippFindAttribute(client->request, "sides", IPP_TAG_ZERO)) != NULL)
  {
    pappl_sides_t value = _papplSidesValue(ippGetString(attr, 0, NULL));
					// "sides" value

    if (ippGetCount(attr) != 1 || ippGetValueTag(attr) != IPP_TAG_KEYWORD)
    {
      papplClientRespondIPPUnsupported(client, attr);
      valid = false;
    }
    else if (!(value & client->printer->driver_data.sides_supported))
    {
      if (exact)
      {
	papplClientRespondIPPUnsupported(client, attr);
	valid = false;
      }
      else
      {
	_papplClientRespondIPPIgnored(client, attr);
	ippDeleteAttribute(client->request, attr);
      }
    }
  }

  _papplRWUnlock(client->printer);

  return (valid);
}
