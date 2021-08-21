//
// Scanner web interface functions for the Scanner Application Framework
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
// Local functions...
//

static void	job_cb(pappl_job_t *job, pappl_client_t *client);
static char	*localize_keyword(const char *attrname, const char *keyword, char *buffer, size_t bufsize);
static char	*localize_media(pappl_media_col_t *media, bool include_source, char *buffer, size_t bufsize);
static char	*time_string(time_t tv, char *buffer, size_t bufsize);
static void	job_pager(pappl_client_t *client, pappl_scanner_t *scanner, int job_index, int limit);


//
// '_papplScannerWebCancelAllJobs()' - Cancel all scanner jobs.
//

void
_papplScannerWebCancelAllJobs(
    pappl_client_t  *client,		// I - Client
    pappl_scanner_t *scanner)		// I - Scanner
{
  const char	*status = NULL;		// Status message, if any


  if (!papplClientHTMLAuthorize(client))
    return;

  if (client->operation == HTTP_STATE_POST)
  {
    int			num_form = 0;	// Number of form variables
    cups_option_t	*form = NULL;	// Form variables

    if ((num_form = papplClientGetForm(client, &form)) == 0)
    {
      status = "Invalid form data.";
    }
    else if (!papplClientIsValidForm(client, num_form, form))
    {
      status = "Invalid form submission.";
    }
    else
    {
      char	path[1024];		// Resource path

      papplScannerCancelAllJobs(scanner);
      snprintf(path, sizeof(path), "%s/jobs", scanner->uriname);
      papplClientRespondRedirect(client, HTTP_STATUS_FOUND, path);
      cupsFreeOptions(num_form, form);
      return;
    }

    cupsFreeOptions(num_form, form);
  }

  papplClientHTMLScannerHeader(client, scanner, "Cancel All Jobs", 0, NULL, NULL);

  if (status)
    papplClientHTMLPrintf(client, "<div class=\"banner\">%s</div>\n", status);

  papplClientHTMLStartForm(client, client->uri, false);
  papplClientHTMLPuts(client, "           <input type=\"submit\" value=\"Confirm Cancel All\"></form>");

  if (papplScannerGetNumberOfActiveJobs(scanner) > 0)
  {
    papplClientHTMLPuts(client,
			"          <table class=\"list\" summary=\"Jobs\">\n"
			"            <thead>\n"
			"              <tr><th>Job #</th><th>Name</th><th>Owner</th><th>Pages Completed</th><th>Status</th><th></th></tr>\n"
			"            </thead>\n"
			"            <tbody>\n");

    papplScannerIterateActiveJobs(scanner, (pappl_job_cb_t)job_cb, client, 1, 0);

    papplClientHTMLPuts(client,
			"            </tbody>\n"
			"          </table>\n");
  }
  else
    papplClientHTMLPuts(client, "        <p>No jobs in history.</p>\n");

  papplClientHTMLFooter(client);
}


//
// '_papplScannerWebConfig()' - Show the scanner configuration web page.
//

void
_papplScannerWebConfig(
    pappl_client_t  *client,		// I - Client
    pappl_scanner_t *scanner)		// I - Scanner
{
  const char	*status = NULL;		// Status message, if any
  char		dns_sd_name[64],	// DNS-SD name
		location[128],		// Location
		geo_location[128],	// Geo-location latitude
		organization[128],	// Organization
		org_unit[128];		// Organizational unit
  pappl_contact_t contact;		// Contact info


  if (!papplClientHTMLAuthorize(client))
    return;

  if (client->operation == HTTP_STATE_POST)
  {
    int			num_form = 0;	// Number of form variable
    cups_option_t	*form = NULL;	// Form variables

    if ((num_form = papplClientGetForm(client, &form)) == 0)
    {
      status = "Invalid form data.";
    }
    else if (!papplClientIsValidForm(client, num_form, form))
    {
      status = "Invalid form submission.";
    }
    else
    {
      _papplScannerWebConfigFinalize(scanner, num_form, form);

      if (scanner->system->options & PAPPL_SOPTIONS_MULTI_QUEUE)
        _papplScannerWebDelete(client, scanner);

      status = "Changes saved.";
    }

    cupsFreeOptions(num_form, form);
  }

  papplClientHTMLScannerHeader(client, scanner, "Configuration", 0, NULL, NULL);
  if (status)
    papplClientHTMLPrintf(client, "<div class=\"banner\">%s</div>\n", status);

  _papplClientHTMLInfo(client, true, papplScannerGetDNSSDName(scanner, dns_sd_name, sizeof(dns_sd_name)), papplScannerGetLocation(scanner, location, sizeof(location)), papplScannerGetGeoLocation(scanner, geo_location, sizeof(geo_location)), papplScannerGetOrganization(scanner, organization, sizeof(organization)), papplScannerGetOrganizationalUnit(scanner, org_unit, sizeof(org_unit)), papplScannerGetContact(scanner, &contact));

  papplClientHTMLPrinterFooter(client);
}


//
// '_papplScannerWebConfigFinalize()' - Save the changes to the scanner configuration.
//

void
_papplScannerWebConfigFinalize(
    pappl_scanner_t *scanner,		// I - Scanner
    int             num_form,		// I - Number of form variables
    cups_option_t   *form)		// I - Form variables
{
  const char	*value,			// Form value
		*geo_lat,		// Geo-location latitude
		*geo_lon,		// Geo-location longitude
		*contact_name,		// Contact name
		*contact_email,		// Contact email
		*contact_tel;		// Contact telephone number


  if ((value = cupsGetOption("dns_sd_name", num_form, form)) != NULL)
    papplScannerSetDNSSDName(scanner, *value ? value : NULL);

  if ((value = cupsGetOption("location", num_form, form)) != NULL)
    papplScannerSetLocation(scanner, *value ? value : NULL);

  geo_lat = cupsGetOption("geo_location_lat", num_form, form);
  geo_lon = cupsGetOption("geo_location_lon", num_form, form);
  if (geo_lat && geo_lon)
  {
    char	uri[1024];		// "geo:" URI

    if (*geo_lat && *geo_lon)
    {
      snprintf(uri, sizeof(uri), "geo:%g,%g", strtod(geo_lat, NULL), strtod(geo_lon, NULL));
      papplScannerSetGeoLocation(scanner, uri);
    }
    else
      papplScannerSetGeoLocation(scanner, NULL);
  }

  if ((value = cupsGetOption("organization", num_form, form)) != NULL)
    papplScannerSetOrganization(scanner, *value ? value : NULL);

  if ((value = cupsGetOption("organizational_unit", num_form, form)) != NULL)
    papplScannerSetOrganizationalUnit(scanner, *value ? value : NULL);

  contact_name  = cupsGetOption("contact_name", num_form, form);
  contact_email = cupsGetOption("contact_email", num_form, form);
  contact_tel   = cupsGetOption("contact_telephone", num_form, form);
  if (contact_name || contact_email || contact_tel)
  {
    pappl_contact_t	contact;	// Contact info

    memset(&contact, 0, sizeof(contact));

    if (contact_name)
      strlcpy(contact.name, contact_name, sizeof(contact.name));
    if (contact_email)
      strlcpy(contact.email, contact_email, sizeof(contact.email));
    if (contact_tel)
      strlcpy(contact.telephone, contact_tel, sizeof(contact.telephone));

    papplScannerSetContact(scanner, &contact);
  }
}


//
// '_papplScannerWebDefaults()' - Show the scanner defaults web page.
//

void
_papplScannerWebDefaults(
    pappl_client_t  *client,		// I - Client
    pappl_scanner_t *scanner)		// I - Scanner
{
  int			i, j;		// Looping vars
  pappl_sc_driver_data_t data;		// Driver data
  const char		*keyword;	// Current keyword
  char			text[256];	// Localized text for keyword
  const char		*status = NULL;	// Status message, if any
  bool			show_source = false;
					// Show the media source?
  static const char * const orients[] =	// orientation-requested strings
  {
    "Portrait",
    "Landscape",
    "Reverse Landscape",
    "Reverse Portrait",
    "Auto"
  };
  static const char * const orient_svgs[] =
  {					// orientation-requested images
    "%3csvg xmlns='http://www.w3.org/2000/svg' width='18' height='24' viewBox='0 0 18 24'%3e%3crect fill='rgba(255,255,255,.5)' stroke='currentColor' stroke-width='1' x='0' y='0' width='18' height='24' rx='5' ry='5'/%3e%3ctext x='3' y='18' font-size='18' fill='currentColor' rotate='0'%3eA%3c/text%3e%3c/svg%3e",
    "%3csvg xmlns='http://www.w3.org/2000/svg' width='18' height='24' viewBox='0 0 18 24'%3e%3crect fill='rgba(255,255,255,.5)' stroke='currentColor' stroke-width='1' x='0' y='0' width='18' height='24' rx='5' ry='5'/%3e%3ctext x='15' y='19' font-size='18' fill='currentColor' rotate='-90'%3eA%3c/text%3e%3c/svg%3e",
    "%3csvg xmlns='http://www.w3.org/2000/svg' width='18' height='24' viewBox='0 0 18 24'%3e%3crect fill='rgba(255,255,255,.5)' stroke='currentColor' stroke-width='1' x='0' y='0' width='18' height='24' rx='5' ry='5'/%3e%3ctext x='3' y='6' font-size='18' fill='currentColor' rotate='90'%3eA%3c/text%3e%3c/svg%3e",
    "%3csvg xmlns='http://www.w3.org/2000/svg' width='18' height='24' viewBox='0 0 18 24'%3e%3crect fill='rgba(255,255,255,.5)' stroke='currentColor' stroke-width='1' x='0' y='0' width='18' height='24' rx='5' ry='5'/%3e%3ctext x='15' y='7' font-size='18' fill='currentColor' rotate='180'%3eA%3c/text%3e%3c/svg%3e",
    "%3csvg xmlns='http://www.w3.org/2000/svg' width='18' height='24' viewBox='0 0 18 24'%3e%3crect fill='rgba(255,255,255,.5)' stroke='currentColor' stroke-width='1' x='0' y='0' width='18' height='24' rx='5' ry='5'/%3e%3ctext x='5' y='18' font-size='18' fill='currentColor' rotate='0'%3e?%3c/text%3e%3c/svg%3e"
  };


  if (!papplClientHTMLAuthorize(client))
    return;

  papplScannerGetDriverData(scanner, &data);

  if (client->operation == HTTP_STATE_POST)
  {
    int			num_form = 0;	// Number of form variable
    cups_option_t	*form = NULL;	// Form variables

    if ((num_form = papplClientGetForm(client, &form)) == 0)
    {
      status = "Invalid form data.";
    }
    else if (!papplClientIsValidForm(client, num_form, form))
    {
      status = "Invalid form submission.";
    }
    else
    {
      const char	*value;		// Value of form variable
      char		*end;			// End of value

      if ((value = cupsGetOption("orientation-requested", num_form, form)) != NULL)
      {
        data.orient_default = (ipp_orient_t)strtol(value, &end, 10);

        if (errno == ERANGE || *end || data.orient_default < IPP_ORIENT_PORTRAIT || data.orient_default > IPP_ORIENT_NONE)
          data.orient_default = IPP_ORIENT_PORTRAIT;
      }

      if ((value = cupsGetOption("output-bin", num_form, form)) != NULL)
      {
        for (i = 0; i < data.num_bin; i ++)
        {
          if (!strcmp(data.bin[i], value))
          {
            data.bin_default = i;
            break;
          }
	}
      }

      if ((value = cupsGetOption("scan-color-mode", num_form, form)) != NULL)
        data.color_default = _papplColorModeValue(value);

      if ((value = cupsGetOption("scan-darkness", num_form, form)) != NULL)
      {
        data.darkness_configured = (int)strtol(value, &end, 10);

        if (errno == ERANGE || *end || data.darkness_configured < 0 || data.darkness_configured > 100)
          data.darkness_configured = 50;
      }

      if ((value = cupsGetOption("scan-quality", num_form, form)) != NULL)
        data.quality_default = (ipp_quality_t)ippEnumValue("scan-quality", value);

      if ((value = cupsGetOption("scan-scaling", num_form, form)) != NULL)
        data.scaling_default = _papplScalingValue(value);

      if ((value = cupsGetOption("scan-speed", num_form, form)) != NULL)
      {
        data.speed_default = (int)strtol(value, &end, 10) * 2540;

        if (errno == ERANGE || *end || data.speed_default < 0 || data.speed_default > data.speed_supported[1])
          data.speed_default = 0;
      }

      if ((value = cupsGetOption("sides", num_form, form)) != NULL)
        data.sides_default = _papplSidesValue(value);

      if ((value = cupsGetOption("scanner-resolution", num_form, form)) != NULL)
      {
        if (sscanf(value, "%dx%ddpi", &data.x_default, &data.y_default) == 1)
          data.y_default = data.x_default;
      }

      if ((value = cupsGetOption("media-source", num_form, form)) != NULL)
      {
        for (i = 0; i < data.num_source; i ++)
	{
	  if (!strcmp(value, data.source[i]))
	  {
	    data.media_default = data.media_ready[i];
	    break;
	  }
	}
      }

    }

    cupsFreeOptions(num_form, form);
  }

  papplClientHTMLScannerHeader(client, scanner, "Scaning Defaults", 0, NULL, NULL);
  if (status)
    papplClientHTMLPrintf(client, "<div class=\"banner\">%s</div>\n", status);

  papplClientHTMLStartForm(client, client->uri, false);

  papplClientHTMLPuts(client,
		      "          <table class=\"form\">\n"
		      "            <tbody>\n");

  // media-col-default
  papplClientHTMLPuts(client, "              <tr><th>Media:</th><td>");

  if (data.num_source > 1)
  {
    papplClientHTMLPuts(client, "<select name=\"media-source\">");

    for (i = 0; i < data.num_source; i ++)
    {
      // See if any two sources have the same size...
      for (j = i + 1; j < data.num_source; j ++)
      {
	if (data.media_ready[i].size_width > 0 && data.media_ready[i].size_width == data.media_ready[j].size_width && data.media_ready[i].size_length == data.media_ready[j].size_length)
	{
	  show_source = true;
	  break;
	}
      }
    }

    for (i = 0; i < data.num_source; i ++)
    {
      keyword = data.source[i];

      if (strcmp(keyword, "manual"))
      {
	papplClientHTMLPrintf(client, "<option value=\"%s\"%s>%s</option>", keyword, !strcmp(keyword, data.media_default.source) ? " selected" : "", localize_media(data.media_ready + i, show_source, text, sizeof(text)));
      }
    }
    papplClientHTMLPuts(client, "</select>");
  }
  else
    papplClientHTMLEscape(client, localize_media(data.media_ready, false, text, sizeof(text)), 0);

  papplClientHTMLPrintf(client, " <a class=\"btn\" href=\"%s/media\">Configure Media</a></td></tr>\n", scanner->uriname);

  // orientation-requested-default
  papplClientHTMLPuts(client, "              <tr><th>Orientation:</th><td>");
  for (i = IPP_ORIENT_PORTRAIT; i <= IPP_ORIENT_NONE; i ++)
  {
    papplClientHTMLPrintf(client, "<label class=\"image\"><input type=\"radio\" name=\"orientation-requested\" value=\"%d\"%s> <img src=\"data:image/svg+xml,%s\" alt=\"%s\"></label> ", i, data.orient_default == (ipp_orient_t)i ? " checked" : "", orient_svgs[i - IPP_ORIENT_PORTRAIT], orients[i - IPP_ORIENT_PORTRAIT]);
  }
  papplClientHTMLPuts(client, "</td></tr>\n");

  // scan-color-mode-default
  papplClientHTMLPuts(client, "              <tr><th>Scan Mode:</th><td>");
  if (data.color_supported == (PAPPL_COLOR_MODE_AUTO | PAPPL_COLOR_MODE_MONOCHROME) || data.color_supported == (PAPPL_COLOR_MODE_AUTO | PAPPL_COLOR_MODE_MONOCHROME | PAPPL_COLOR_MODE_AUTO_MONOCHROME))
  {
    papplClientHTMLPuts(client, "B&amp;W");
  }
  else
  {
    for (i = PAPPL_COLOR_MODE_AUTO; i <= PAPPL_COLOR_MODE_PROCESS_MONOCHROME; i *= 2)
    {
      if ((data.color_supported & (pappl_color_mode_t)i) && i != PAPPL_COLOR_MODE_AUTO_MONOCHROME)
      {
	keyword = _papplColorModeString((pappl_color_mode_t)i);
	papplClientHTMLPrintf(client, "<label><input type=\"radio\" name=\"scan-color-mode\" value=\"%s\"%s> %s</label> ", keyword, (pappl_color_mode_t)i == data.color_default ? " checked" : "", localize_keyword("scan-color-mode", keyword, text, sizeof(text)));
      }
    }
  }
  papplClientHTMLPuts(client, "</td></tr>\n");

  if (data.sides_supported && data.sides_supported != PAPPL_SIDES_ONE_SIDED)
  {
    // sides-default
    papplClientHTMLPuts(client, "              <tr><th>2-Sided Scaning:</th><td>");
    for (i = PAPPL_SIDES_ONE_SIDED; i <= PAPPL_SIDES_TWO_SIDED_SHORT_EDGE; i *= 2)
    {
      if (data.sides_supported & (pappl_sides_t)i)
      {
	keyword = _papplSidesString((pappl_sides_t)i);
	papplClientHTMLPrintf(client, "<label><input type=\"radio\" name=\"sides\" value=\"%s\"%s> %s</label> ", keyword, (pappl_sides_t)i == data.sides_default ? " checked" : "", localize_keyword("sides", keyword, text, sizeof(text)));
      }
    }
    papplClientHTMLPuts(client, "</td></tr>\n");
  }

  // output-bin-default
  if (data.num_bin > 0)
  {
    papplClientHTMLPuts(client, "              <tr><th>Output Tray:</th><td>");
    if (data.num_bin > 1)
    {
      papplClientHTMLPuts(client, "<select name=\"output-bin\">");
      for (i = 0; i < data.num_bin; i ++)
	papplClientHTMLPrintf(client, "<option value=\"%s\"%s>%s</option>", data.bin[i], i == data.bin_default ? " selected" : "", localize_keyword("output-bin", data.bin[i], text, sizeof(text)));
      papplClientHTMLPuts(client, "</select>");
    }
    else
    {
      papplClientHTMLPrintf(client, "%s", localize_keyword("output-bin", data.bin[data.bin_default], text, sizeof(text)));
    }
    papplClientHTMLPuts(client, "</td></tr>\n");
  }

  // scan-quality-default
  papplClientHTMLPuts(client, "              <tr><th>Scan Quality:</th><td>");
  for (i = IPP_QUALITY_DRAFT; i <= IPP_QUALITY_HIGH; i ++)
  {
    keyword = ippEnumString("scan-quality", i);
    papplClientHTMLPrintf(client, "<label><input type=\"radio\" name=\"scan-quality\" value=\"%s\"%s> %s</label> ", keyword, (ipp_quality_t)i == data.quality_default ? " checked" : "", localize_keyword("scan-quality", keyword, text, sizeof(text)));
  }
  papplClientHTMLPuts(client, "</select></td></tr>\n");

  // scan-darkness-configured
  if (data.darkness_supported)
  {
    papplClientHTMLPuts(client, "              <tr><th>Scan Darkness:</th><td><select name=\"scan-darkness\">");
    for (i = 0; i < data.darkness_supported; i ++)
    {
      int percent = 100 * i / (data.darkness_supported - 1);
					// Percent darkness

      papplClientHTMLPrintf(client, "<option value=\"%d\"%s>%d%%</option>", percent, percent == data.darkness_configured ? " selected" : "", percent);
    }
    papplClientHTMLPuts(client, "</select></td></tr>\n");
  }

  // scan-speed-default
  if (data.speed_supported[1])
  {
    papplClientHTMLPuts(client, "              <tr><th>Scan Speed:</th><td><select name=\"scan-speed\"><option value=\"0\">Auto</option>");
    for (i = data.speed_supported[0]; i <= data.speed_supported[1]; i += 2540)
    {
      if (i > 0)
	papplClientHTMLPrintf(client, "<option value=\"%d\"%s>%d %s/sec</option>", i / 2540, i == data.speed_default ? " selected" : "", i / 2540, i >= (2 * 2540) ? "inches" : "inch");
    }
    papplClientHTMLPuts(client, "</select></td></tr>\n");
  }

  // scan-scaling-default
  papplClientHTMLPuts(client, "              <tr><th>Scaling:</th><td><select name=\"scan-scaling\">");
  for (i = PAPPL_SCALING_AUTO; i <= PAPPL_SCALING_NONE; i *= 2)
  {
    keyword = _papplScalingString((pappl_scaling_t)i);
    papplClientHTMLPrintf(client, "<option value=\"%s\"%s>%s</option>", keyword, (pappl_scaling_t)i == data.scaling_default ? " selected" : "", localize_keyword("scan-scaling", keyword, text, sizeof(text)));
  }
  papplClientHTMLPuts(client, "</select></td></tr>\n");

  // scanner-resolution-default
  papplClientHTMLPuts(client, "              <tr><th>Resolution:</th><td>");

  if (data.num_resolution == 1)
  {
    if (data.x_resolution[0] != data.y_resolution[0])
      papplClientHTMLPrintf(client, "%dx%ddpi", data.x_resolution[0], data.y_resolution[0]);
    else
      papplClientHTMLPrintf(client, "%ddpi", data.x_resolution[0]);
  }
  else
  {
    papplClientHTMLPuts(client, "<select name=\"scanner-resolution\">");
    for (i = 0; i < data.num_resolution; i ++)
    {
      if (data.x_resolution[i] != data.y_resolution[i])
	snprintf(text, sizeof(text), "%dx%ddpi", data.x_resolution[i], data.y_resolution[i]);
      else
	snprintf(text, sizeof(text), "%ddpi", data.x_resolution[i]);

      papplClientHTMLPrintf(client, "<option value=\"%s\"%s>%s</option>", text, (data.x_default == data.x_resolution[i] && data.y_default == data.y_resolution[i]) ? " selected" : "", text);
    }
    papplClientHTMLPuts(client, "</select>");
  }
  papplClientHTMLPuts(client, "</td></tr>\n");

  pthread_rwlock_unlock(&scanner->rwlock);

  papplClientHTMLPuts(client,
                      "              <tr><th></th><td><input type=\"submit\" value=\"Save Changes\"></td></tr>\n"
                      "            </tbody>\n"
                      "          </table>"
                      "        </form>\n");

  papplClientHTMLPrinterFooter(client);
}


//
// '_papplScannerWebDelete()' - Show the scanner delete confirmation web page.
//

void
_papplScannerWebDelete(
    pappl_client_t  *client,		// I - Client
    pappl_scanner_t *scanner)		// I - Scanner
{
  const char	*status = NULL;		// Status message, if any


  if (!papplClientHTMLAuthorize(client))
    return;

  if (client->operation == HTTP_STATE_POST)
  {
    int			num_form = 0;	// Number of form variables
    cups_option_t	*form = NULL;	// Form variables

    if ((num_form = papplClientGetForm(client, &form)) == 0)
    {
      status = "Invalid form data.";
    }
    else if (!papplClientIsValidForm(client, num_form, form))
    {
      status = "Invalid form submission.";
    }
    else if (scanner->processing_job)
    {
      // Scanner is processing a job...
      status = "Scanner is currently active.";
    }
    else
    {
      if (!scanner->is_deleted)
      {
        papplScannerDelete(scanner);
        scanner = NULL;
      }

      papplClientRespondRedirect(client, HTTP_STATUS_FOUND, "/");
      cupsFreeOptions(num_form, form);
      return;
    }

    cupsFreeOptions(num_form, form);
  }

  papplClientHTMLScannerHeader(client, scanner, "Delete Scanner", 0, NULL, NULL);

  if (status)
    papplClientHTMLPrintf(client, "          <div class=\"banner\">%s</div>\n", status);

  papplClientHTMLStartForm(client, client->uri, false);
  papplClientHTMLPuts(client,"          <input type=\"submit\" value=\"Confirm Delete Scanner\"></form>");

  papplClientHTMLFooter(client);
}


//
// '_papplScannerWebHome()' - Show the scanner home page.
//

void
_papplScannerWebHome(
    pappl_client_t  *client,		// I - Client
    pappl_scanner_t *scanner)		// I - Scanner
{
  const char	*status = NULL;		// Status text, if any
  ipp_pstate_t	scanner_state;		// Scanner state
  char		edit_path[1024];	// Edit configuration URL
  const int	limit = 20;		// Jobs per page
  int		job_index = 1;		// Job index


  // Save current scanner state...
  scanner_state = papplScannerGetState(scanner);

  // Show status...
  papplClientHTMLScannerHeader(client, scanner, NULL, scanner_state == IPP_PSTATE_PROCESSING ? 10 : 0, NULL, NULL);

  papplClientHTMLPuts(client,
                      "      <div class=\"row\">\n"
                      "        <div class=\"col-6\">\n");

  _papplScannerWebIteratorCallback(scanner, client);

  if (status)
    papplClientHTMLPrintf(client, "<div class=\"banner\">%s</div>\n", status);

  snprintf(edit_path, sizeof(edit_path), "%s/config", scanner->uriname);
  papplClientHTMLPrintf(client, "          <h1 class=\"title\">Configuration <a class=\"btn\" href=\"https://%s:%d%s\">Change</a></h1>\n", client->host_field, client->host_port, edit_path);

  _papplClientHTMLPutLinks(client, scanner->links, PAPPL_LOPTIONS_CONFIGURATION);

  _papplClientHTMLInfo(client, false, scanner->dns_sd_name, scanner->location, scanner->geo_location, scanner->organization, scanner->org_unit, &scanner->contact);

  if (!(scanner->system->options & PAPPL_SOPTIONS_MULTI_QUEUE))
    _papplSystemWebSettings(client);

  papplClientHTMLPrintf(client,
			"        </div>\n"
			"        <div class=\"col-6\">\n"
			"          <h1 class=\"title\"><a href=\"%s/jobs\">Jobs</a>", scanner->uriname);

  if (papplScannerGetNumberOfJobs(scanner) > 0)
  {
    if (cupsArrayCount(scanner->active_jobs) > 0)
      papplClientHTMLPrintf(client, " <a class=\"btn\" href=\"https://%s:%d%s/cancelall\">Cancel All Jobs</a></h1>\n", client->host_field, client->host_port, scanner->uriname);
    else
      papplClientHTMLPuts(client, "</h1>\n");

    _papplClientHTMLPutLinks(client, scanner->links, PAPPL_LOPTIONS_JOB);

    job_pager(client, scanner, job_index, limit);

    papplClientHTMLPuts(client,
			"          <table class=\"list\" summary=\"Jobs\">\n"
			"            <thead>\n"
			"              <tr><th>Job #</th><th>Name</th><th>Owner</th><th>Pages</th><th>Status</th><th></th></tr>\n"
			"            </thead>\n"
			"            <tbody>\n");

    papplScannerIterateAllJobs(scanner, (pappl_job_cb_t)job_cb, client, job_index, limit);

    papplClientHTMLPuts(client,
			"            </tbody>\n"
			"          </table>\n");

    job_pager(client, scanner, job_index, limit);
  }
  else
  {
    papplClientHTMLPuts(client, "</h1>\n");
    _papplClientHTMLPutLinks(client, scanner->links, PAPPL_LOPTIONS_JOB);
    papplClientHTMLPuts(client, "        <p>No jobs in history.</p>\n");
  }

  papplClientHTMLPrinterFooter(client);
}


//
// '_papplScannerWebIteratorCallback()' - Show the scanner status.
//

void
_papplScannerWebIteratorCallback(
    pappl_scanner_t *scanner,		// I - Scanner
    pappl_client_t  *client)		// I - Client
{
  int			i;		// Looping var
  pappl_preason_t	reason,		// Current reason
			scanner_reasons;// Scanner state reasons
  ipp_pstate_t		scanner_state;	// Scanner state
  int			scanner_jobs;	// Number of queued jobs
  char			uri[256];	// Form URI
  static const char * const states[] =	// State strings
  {
    "Idle",
    "Scaning",
    "Stopped"
  };
  static const char * const reasons[] =	// Reason strings
  {
    "Other",
    "Cover Open",
    "Tray Missing",
    "Out of Ink",
    "Low Ink",
    "Waste Tank Almost Full",
    "Waste Tank Full",
    "Media Empty",
    "Media Jam",
    "Media Low",
    "Media Needed",
    "Too Many Jobs",
    "Out of Toner",
    "Low Toner"
  };


  scanner_jobs    = papplScannerGetNumberOfActiveJobs(scanner);
  scanner_state   = papplScannerGetState(scanner);
  scanner_reasons = papplScannerGetReasons(scanner);

  snprintf(uri, sizeof(uri), "%s/", scanner->uriname);

  if (!strcmp(client->uri, "/") && (client->system->options & PAPPL_SOPTIONS_MULTI_QUEUE))
    papplClientHTMLPrintf(client,
			  "          <h2 class=\"title\"><a href=\"%s/\">%s</a> <a class=\"btn\" href=\"https://%s:%d%s/delete\">Delete</a></h2>\n", scanner->uriname, scanner->name, client->host_field, client->host_port, scanner->uriname);
  else
    papplClientHTMLPuts(client, "          <h1 class=\"title\">Status</h1>\n");

  papplClientHTMLPrintf(client,
			"          <p><img class=\"%s\" src=\"%s/icon-md.png\">%s, %d %s", ippEnumString("scanner-state", (int)scanner_state), scanner->uriname, states[scanner_state - IPP_PSTATE_IDLE], scanner_jobs, scanner_jobs == 1 ? "job" : "jobs");
  for (i = 0, reason = PAPPL_PREASON_OTHER; reason <= PAPPL_PREASON_TONER_LOW; i ++, reason *= 2)
  {
    if (scanner_reasons & reason)
      papplClientHTMLPrintf(client, ", %s", reasons[i]);
  }

  if (strcmp(scanner->name, scanner->driver_data.make_and_model))
    papplClientHTMLPrintf(client, ".<br>%s</p>\n", scanner->driver_data.make_and_model);
  else
    papplClientHTMLPuts(client, ".</p>\n");

  papplClientHTMLPuts(client, "          <div class=\"btn\">");
  _papplClientHTMLPutLinks(client, scanner->links, PAPPL_LOPTIONS_STATUS);

  if (strcmp(client->uri, "/") && (client->system->options & PAPPL_SOPTIONS_MULTI_QUEUE))
    papplClientHTMLPrintf(client, " <a class=\"btn\" href=\"https://%s:%d%s/delete\">Delete Scanner</a>", client->host_field, client->host_port, scanner->uriname);

  papplClientHTMLPuts(client, "<br clear=\"all\"></div>\n");
}


//
// '_papplScannerWebJobs()' - Show the scanner jobs web page.
//

void
_papplScannerWebJobs(
    pappl_client_t  *client,		// I - Client
    pappl_scanner_t *scanner)		// I - Scanner
{
  ipp_pstate_t	scanner_state;		// Scanner state
  int		job_index = 1,		// Job index
		limit = 20;		// Jobs per page


  if (!papplClientHTMLAuthorize(client))
    return;

  scanner_state = papplScannerGetState(scanner);

  if (client->operation == HTTP_STATE_GET)
  {
    cups_option_t	*form = NULL;	// Form variables
    int			num_form = papplClientGetForm(client, &form);
					// Number of form variables
    const char		*value = NULL;	// Value of form variable

    if ((value = cupsGetOption("job-index", num_form, form)) != NULL)
      job_index = (int)strtol(value, NULL, 10);

    cupsFreeOptions(num_form, form);
  }

  if (cupsArrayCount(scanner->active_jobs) > 0)
  {
    char	url[1024];		// URL for Cancel All Jobs

    httpAssembleURIf(HTTP_URI_CODING_ALL, url, sizeof(url), "https", NULL, client->host_field, client->host_port, "%s/cancelall", scanner->uriname);

    papplClientHTMLScannerHeader(client, scanner, "Jobs", scanner_state == IPP_PSTATE_PROCESSING ? 10 : 0, "Cancel All Jobs", url);
  }
  else
  {
    papplClientHTMLScannerHeader(client, scanner, "Jobs", scanner_state == IPP_PSTATE_PROCESSING ? 10 : 0, NULL, NULL);
  }

  if (papplScannerGetNumberOfJobs(scanner) > 0)
  {
    job_pager(client, scanner, job_index, limit);

    papplClientHTMLPuts(client,
			"          <table class=\"list\" summary=\"Jobs\">\n"
			"            <thead>\n"
			"              <tr><th>Job #</th><th>Name</th><th>Owner</th><th>Pages Completed</th><th>Status</th><th></th></tr>\n"
			"            </thead>\n"
			"            <tbody>\n");

    papplScannerIterateAllJobs(scanner, (pappl_job_cb_t)job_cb, client, job_index, limit);

    papplClientHTMLPuts(client,
			"            </tbody>\n"
			"          </table>\n");

    job_pager(client, scanner, job_index, limit);
  }
  else
    papplClientHTMLPuts(client, "        <p>No jobs in history.</p>\n");

  papplClientHTMLPrinterFooter(client);
}


//
// 'job_cb()' - Job iterator callback.
//

static void
job_cb(pappl_job_t    *job,		// I - Job
       pappl_client_t *client)		// I - Client
{
  bool	show_cancel = false;		// Show the "cancel" button?
  char	when[256],			// When job queued/started/finished
	hhmmss[64];			// Time HH:MM:SS


  switch (papplJobGetState(job))
  {
    case IPP_JSTATE_PENDING :
    case IPP_JSTATE_HELD :
	show_cancel = true;
	snprintf(when, sizeof(when), "Queued at %s", time_string(papplJobGetTimeCreated(job), hhmmss, sizeof(hhmmss)));
	break;

    case IPP_JSTATE_PROCESSING :
    case IPP_JSTATE_STOPPED :
	if (papplJobIsCanceled(job))
	{
	  strlcpy(when, "Canceling", sizeof(when));
	}
	else
	{
	  show_cancel = true;
	  snprintf(when, sizeof(when), "Started at %s", time_string(papplJobGetTimeProcessed(job), hhmmss, sizeof(hhmmss)));
	}
	break;

    case IPP_JSTATE_ABORTED :
	snprintf(when, sizeof(when), "Aborted at %s", time_string(papplJobGetTimeCompleted(job), hhmmss, sizeof(hhmmss)));
	break;

    case IPP_JSTATE_CANCELED :
	snprintf(when, sizeof(when), "Canceled at %s", time_string(papplJobGetTimeCompleted(job), hhmmss, sizeof(hhmmss)));
	break;

    case IPP_JSTATE_COMPLETED :
	snprintf(when, sizeof(when), "Completed at %s", time_string(papplJobGetTimeCompleted(job), hhmmss, sizeof(hhmmss)));
	break;
  }

  papplClientHTMLPrintf(client, "              <tr><td>%d</td><td>%s</td><td>%s</td><td>%d</td><td>%s</td>", papplJobGetID(job), papplJobGetName(job), papplJobGetUsername(job), papplJobGetImpressionsCompleted(job), when);

  if (show_cancel)
    papplClientHTMLPrintf(client, "          <td><a class=\"btn\" href=\"%s/cancel?job-id=%d\">Cancel Job</a></td></tr>\n", job->scanner->uriname, papplJobGetID(job));
  else
    papplClientHTMLPuts(client, "<td></td></tr>\n");
}


//
// 'job_pager()' - Show the job paging links.
//

static void
job_pager(pappl_client_t  *client,	// I - Client
	  pappl_scanner_t *scanner,	// I - Scanner
	  int             job_index,	// I - First job shown (1-based)
	  int             limit)	// I - Maximum jobs shown
{
  int	num_jobs = 0,			// Number of jobs
	num_pages = 0,			// Number of pages
	i,				// Looping var
	page = 0;			// Current page
  char	path[1024];			// resource path


  if ((num_jobs = papplScannerGetNumberOfJobs(scanner)) <= limit)
    return;

  num_pages = (num_jobs + limit - 1) / limit;
  page      = (job_index - 1) / limit;

  snprintf(path, sizeof(path), "%s/jobs", scanner->uriname);

  papplClientHTMLPuts(client, "          <div class=\"pager\">");

  if (page > 0)
    papplClientHTMLPrintf(client, "<a class=\"btn\" href=\"%s?job-index=%d\">&laquo;</a>", path, (page - 1) * limit + 1);

  for (i = 0; i < num_pages; i ++)
  {
    if (i == page)
      papplClientHTMLPrintf(client, " %d", i + 1);
    else
      papplClientHTMLPrintf(client, " <a class=\"btn\" href=\"%s?job-index=%d\">%d</a>", path, i * limit + 1, i + 1);
  }

  if (page < (num_pages - 1))
    papplClientHTMLPrintf(client, " <a class=\"btn\" href=\"%s?job-index=%d\">&raquo;</a>", path, (page + 1) * limit + 1);

  papplClientHTMLPuts(client, "</div>\n");
}


//
// 'localize_keyword()' - Localize a media keyword...
//

static char *				// O - Localized string
localize_keyword(
    const char *attrname,		// I - Attribute name
    const char *keyword,		// I - Keyword string
    char       *buffer,			// I - String buffer
    size_t     bufsize)			// I - String buffer size
{
  char	*ptr;				// Pointer into string


  // TODO: Do real localization of keywords (Issue #58)
  if (!strcmp(keyword, "bi-level"))
  {
    strlcpy(buffer, "B&W (no shading)", bufsize);
  }
  else if (!strcmp(keyword, "monochrome"))
  {
    strlcpy(buffer, "B&W", bufsize);
  }
  else if (!strcmp(keyword, "main-roll"))
  {
    strlcpy(buffer, "Main", bufsize);
  }
  else if (!strcmp(keyword, "alternate-roll"))
  {
    strlcpy(buffer, "Alternate", bufsize);
  }
  else if (!strcmp(keyword, "labels"))
  {
    strlcpy(buffer, "Cut Labels", bufsize);
  }
  else if (!strcmp(keyword, "labels-continuous"))
  {
    strlcpy(buffer, "Continuous Labels", bufsize);
  }
  else if (!strcmp(attrname, "media-type") && !strcmp(keyword, "continuous"))
  {
    strlcpy(buffer, "Continuous Paper", bufsize);
  }
  else if (!strncmp(keyword, "photographic", 12))
  {
    if (keyword[12] == '-')
      snprintf(buffer, bufsize, "%c%s Photo Paper", toupper(keyword[13]), keyword + 14);
    else
      strlcpy(buffer, "Photo Paper", bufsize);
  }
  else if (!strcmp(keyword, "stationery"))
  {
    strlcpy(buffer, "Plain Paper", bufsize);
  }
  else if (!strcmp(keyword, "stationery-letterhead"))
  {
    strlcpy(buffer, "Letterhead", bufsize);
  }
  else if (!strcmp(keyword, "one-sided"))
  {
    strlcpy(buffer, "Off", bufsize);
  }
  else if (!strcmp(keyword, "two-sided-long-edge"))
  {
    strlcpy(buffer, "On (Portrait)", bufsize);
  }
  else if (!strcmp(keyword, "two-sided-short-edge"))
  {
    strlcpy(buffer, "On (Landscape)", bufsize);
  }
  else if (!strcmp(attrname, "media"))
  {
    pwg_media_t *pwg = pwgMediaForPWG(keyword);
					// PWG media size info

    if (!strcmp(pwg->ppd, "Letter"))
      strlcpy(buffer, "US Letter", bufsize);
    else if (!strcmp(pwg->ppd, "Legal"))
      strlcpy(buffer, "US Legal", bufsize);
    else if (!strcmp(pwg->ppd, "Env10"))
      strlcpy(buffer, "#10 Envelope", bufsize);
    else if (!strcmp(pwg->ppd, "A4") || !strcmp(pwg->ppd, "A5") || !strcmp(pwg->ppd, "A6"))
      strlcpy(buffer, pwg->ppd, bufsize);
    else if (!strcmp(pwg->ppd, "EnvDL"))
      strlcpy(buffer, "DL Envelope", bufsize);
    else if ((pwg->width % 100) == 0 && (pwg->width % 2540) != 0)
      snprintf(buffer, bufsize, "%d x %dmm", pwg->width / 100, pwg->length / 100);
    else
      snprintf(buffer, bufsize, "%g x %g\"", pwg->width / 2540.0, pwg->length / 2540.0);
  }
  else
  {
    strlcpy(buffer, keyword, bufsize);
    *buffer = (char)toupper(*buffer);
    for (ptr = buffer + 1; *ptr; ptr ++)
    {
      if (*ptr == '-' && ptr[1])
      {
	*ptr++ = ' ';
	*ptr   = (char)toupper(*ptr);
      }
    }
  }

  return (buffer);
}


//
// 'localize_media()' - Localize media-col information.
//

static char *				// O - Localized description of the media
localize_media(
    pappl_media_col_t *media,		// I - Media info
    bool              include_source,	// I - Include the media source?
    char              *buffer,		// I - String buffer
    size_t            bufsize)		// I - Size of string buffer
{
  char		size[128],		// Size name string
		source[128],		// Source string
		type[128];		// Type string
  const char	*borderless;		// Borderless qualifier


  if (!media->size_name[0])
    strlcpy(size, "Unknown", sizeof(size));
  else
    localize_keyword("media", media->size_name, size, sizeof(size));

  if (!media->type[0])
    strlcpy(type, "Unknown", sizeof(type));
  else
    localize_keyword("media-type", media->type, type, sizeof(type));

  if (!media->left_margin && !media->right_margin && !media->top_margin && !media->bottom_margin)
    borderless = ", Borderless";
  else
    borderless = "";

  if (include_source)
    snprintf(buffer, bufsize, "%s (%s%s) from %s", size, type, borderless, localize_keyword("media-source", media->source, source, sizeof(source)));
  else
    snprintf(buffer, bufsize, "%s (%s%s)", size, type, borderless);

  return (buffer);
}


//
// 'time_string()' - Return the local time in hours, minutes, and seconds.
//

static char *
time_string(time_t tv,			// I - Time value
            char   *buffer,		// I - Buffer
	    size_t bufsize)		// I - Size of buffer
{
  struct tm	date;			// Local time and date

  localtime_r(&tv, &date);

  strftime(buffer, bufsize, "%X", &date);

  return (buffer);
}
