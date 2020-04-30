//
// Printer web interface functions for the Printer Application Framework
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
static void	media_chooser(pappl_client_t *client, pappl_pdriver_data_t *driver_data, const char *title, const char *name, pappl_media_col_t *media);
static void	printer_footer(pappl_client_t *client);
static void	printer_header(pappl_client_t *client, pappl_printer_t *printer, const char *title, int refresh);
static char	*time_string(time_t tv, char *buffer, size_t bufsize);


//
// '_papplPrinterIteratorWebCallback()' - Show the printer status.
//

void
_papplPrinterIteratorWebCallback(
    pappl_printer_t *printer,		// I - Printer
    pappl_client_t  *client)		// I - Client
{
  int			i;		// Looping var
  pappl_preason_t	reason,		// Current reason
			printer_reasons;// Printer state reasons
  ipp_pstate_t		printer_state;	// Printer state
  int			printer_jobs;	// Number of queued jobs
  static const char * const states[] =	// State strings
  {
    "Idle",
    "Printing",
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


  printer_jobs    = papplPrinterGetActiveJobs(printer);
  printer_state   = papplPrinterGetState(printer);
  printer_reasons = papplPrinterGetReasons(printer);

  if (!strcmp(client->uri, "/") && (client->system->options & PAPPL_SOPTIONS_MULTI_QUEUE))
    papplClientHTMLPrintf(client,
			  "          <h2 class=\"title\"><a href=\"%s/\">%s</a></h2>\n", printer->uriname, printer->name);
  else
    papplClientHTMLPuts(client, "          <h1 class=\"title\">Status</h1>\n");

  papplClientHTMLPrintf(client,
			"          <p><img class=\"%s\" src=\"%s/icon-md.png\">%s, %d %s", ippEnumString("printer-state", printer_state), printer->uriname, states[printer_state - IPP_PSTATE_IDLE], printer_jobs, printer_jobs == 1 ? "job" : "jobs");

  for (i = 0, reason = PAPPL_PREASON_OTHER; reason <= PAPPL_PREASON_TONER_LOW; i ++, reason *= 2)
  {
    if (printer_reasons & reason)
      papplClientHTMLPrintf(client, ", %s", reasons[i]);
  }
  papplClientHTMLPrintf(client,
                        ".</p>\n"
                        "          <p><a class=\"btn\" href=\"%s/media\">Media</a> <a class=\"btn\" href=\"%s/printing\">Printing Defaults</a>", printer->uriname, printer->uriname);
  if (printer->driver_data.has_supplies)
    papplClientHTMLPrintf(client, " <a class=\"btn\" href=\"%s/supplies\">Supplies</a>", printer->uriname);

  papplClientHTMLPuts(client, "<br clear=\"all\"></p>\n");
}


//
// '_papplPrinterWebConfig()' - Show the printer configuration web page.
//

void
_papplPrinterWebConfig(
    pappl_client_t  *client,		// I - Client
    pappl_printer_t *printer)		// I - Printer
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
    else if (!papplClientValidateForm(client, num_form, form))
    {
      status = "Invalid form submission.";
    }
    else
    {
      _papplPrinterWebConfigFinalize(printer, num_form, form);

      if (!(printer->system->options & PAPPL_SOPTIONS_MULTI_QUEUE))
        _papplSystemWebConfigFinalize(printer->system, num_form, form);

      status = "Changes saved.";
    }

    cupsFreeOptions(num_form, form);
  }

  printer_header(client, printer, "Configuration", 0);
  if (status)
    papplClientHTMLPrintf(client, "<div class=\"banner\">%s</div>\n", status);

  _papplClientHTMLInfo(client, true, papplPrinterGetDNSSDName(printer, dns_sd_name, sizeof(dns_sd_name)), papplPrinterGetLocation(printer, location, sizeof(location)), papplPrinterGetGeoLocation(printer, geo_location, sizeof(geo_location)), papplPrinterGetOrganization(printer, organization, sizeof(organization)), papplPrinterGetOrganizationalUnit(printer, org_unit, sizeof(org_unit)), papplPrinterGetContact(printer, &contact));

  printer_footer(client);
}


//
// '_papplPrinterWebConfigFinalize()' - Save the changes to the printer configuration.
//

void
_papplPrinterWebConfigFinalize(
    pappl_printer_t *printer,		// I - Printer
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
    papplPrinterSetDNSSDName(printer, *value ? value : NULL);

  if ((value = cupsGetOption("location", num_form, form)) != NULL)
    papplPrinterSetLocation(printer, *value ? value : NULL);

  geo_lat = cupsGetOption("geo_location_lat", num_form, form);
  geo_lon = cupsGetOption("geo_location_lon", num_form, form);
  if (geo_lat && geo_lon)
  {
    char	uri[1024];		// "geo:" URI

    if (*geo_lat && *geo_lon)
    {
      snprintf(uri, sizeof(uri), "geo:%g,%g", atof(geo_lat), atof(geo_lon));
      papplPrinterSetGeoLocation(printer, uri);
    }
    else
      papplPrinterSetGeoLocation(printer, NULL);
  }

  if ((value = cupsGetOption("organization", num_form, form)) != NULL)
    papplPrinterSetOrganization(printer, *value ? value : NULL);

  if ((value = cupsGetOption("organizational_unit", num_form, form)) != NULL)
    papplPrinterSetOrganizationalUnit(printer, *value ? value : NULL);

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

    papplPrinterSetContact(printer, &contact);
  }
}


//
// '_papplPrinterWebDefaults()' - Show the printer defaults web page.
//

void
_papplPrinterWebDefaults(
    pappl_client_t  *client,		// I - Client
    pappl_printer_t *printer)		// I - Printer
{
  int			i;		// Looping var
  pappl_pdriver_data_t	data;		// Driver data
  const char		*keyword;	// Current keyword
  char			text[256];	// Localized text for keyword
  const char		*status = NULL;	// Status message, if any


  if (!papplClientHTMLAuthorize(client))
    return;

  papplPrinterGetPrintDriverData(printer, &data);

  if (client->operation == HTTP_STATE_POST)
  {
    int			num_form = 0;	// Number of form variable
    cups_option_t	*form = NULL;	// Form variables

    if ((num_form = papplClientGetForm(client, &form)) == 0)
    {
      status = "Invalid form data.";
    }
    else if (!papplClientValidateForm(client, num_form, form))
    {
      status = "Invalid form submission.";
    }
    else
    {
      const char	*value;		// Value of form variable

      if ((value = cupsGetOption("print-color-mode", num_form, form)) != NULL)
        data.color_default = _papplColorModeValue(value);

      if ((value = cupsGetOption("print-content-optimize", num_form, form)) != NULL)
        data.content_default = _papplContentValue(value);

      if ((value = cupsGetOption("print-quality", num_form, form)) != NULL)
        data.quality_default = ippEnumValue("print-quality", value);

      if ((value = cupsGetOption("print-scaling", num_form, form)) != NULL)
        data.scaling_default = _papplScalingValue(value);

      if ((value = cupsGetOption("sides", num_form, form)) != NULL)
        data.sides_default = _papplSidesValue(value);

      if ((value = cupsGetOption("printer-resolution", num_form, form)) != NULL)
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

      papplPrinterSetPrintDefaults(printer, &data);

      status = "Changes saved.";
    }

    cupsFreeOptions(num_form, form);
  }

  printer_header(client, printer, "Printing Defaults", 0);
  if (status)
    papplClientHTMLPrintf(client, "<div class=\"banner\">%s</div>\n", status);

  papplClientHTMLStartForm(client, client->uri, false);

  papplClientHTMLPuts(client,
		      "          <table class=\"form\">\n"
		      "            <tbody>\n");

  // media-col-default
  papplClientHTMLPuts(client, "              <tr><th>Media:</th><td><select name=\"media-source\">");
  for (i = 0; i < data.num_source; i ++)
  {
    keyword = data.source[i];

    if (strcmp(keyword, "manual"))
    {
      papplClientHTMLPrintf(client, "<option value=\"%s\"%s>%s</option>", keyword, !strcmp(keyword, data.media_default.source) ? " selected" : "", localize_media(data.media_ready + i, true, text, sizeof(text)));
    }
  }
  papplClientHTMLPrintf(client, "</select> <a class=\"btn\" href=\"%s/media\">Configure Media</a></td></tr>\n", printer->uriname);

  // print-color-mode-default
  papplClientHTMLPuts(client, "              <tr><th>Color Mode:</th><td><select name=\"print-color-mode\">");
  for (i = PAPPL_COLOR_MODE_AUTO; i <= PAPPL_COLOR_MODE_PROCESS_MONOCHROME; i *= 2)
  {
    if (data.color_supported & i)
    {
      keyword = _papplColorModeString(i);
      papplClientHTMLPrintf(client, "<option value=\"%s\"%s>%s</option>", keyword, i == data.color_default ? " selected" : "", localize_keyword("print-color-mode", keyword, text, sizeof(text)));
    }
  }
  papplClientHTMLPuts(client, "</select></td></tr>\n");

  if (data.sides_supported)
  {
    // sides-default
    papplClientHTMLPuts(client, "              <tr><th>2-Sided Printing:</th><td><select name=\"sides\">");
    for (i = PAPPL_SIDES_ONE_SIDED; i <= PAPPL_SIDES_TWO_SIDED_SHORT_EDGE; i *= 2)
    {
      if (data.sides_supported & i)
      {
	keyword = _papplSidesString(i);
	papplClientHTMLPrintf(client, "<option value=\"%s\"%s>%s</option>", keyword, i == data.sides_default ? " selected" : "", localize_keyword("sides", keyword, text, sizeof(text)));
      }
    }
    papplClientHTMLPuts(client, "</select></td></tr>\n");
  }

  // print-quality-default
  papplClientHTMLPuts(client, "              <tr><th>Print Quality:</th><td><select name=\"print-quality\">");
  for (i = IPP_QUALITY_DRAFT; i <= IPP_QUALITY_HIGH; i ++)
  {
    keyword = ippEnumString("print-quality", i);
    papplClientHTMLPrintf(client, "<option value=\"%s\"%s>%s</option>", keyword, i == data.quality_default ? " selected" : "", localize_keyword("print-quality", keyword, text, sizeof(text)));
  }
  papplClientHTMLPuts(client, "</select></td></tr>\n");

  // print-content-optimize-default
  papplClientHTMLPuts(client, "              <tr><th>Optimize For:</th><td><select name=\"print-content-optimize\">");
  for (i = PAPPL_CONTENT_AUTO; i <= PAPPL_CONTENT_TEXT_AND_GRAPHIC; i *= 2)
  {
    keyword = _papplContentString(i);
    papplClientHTMLPrintf(client, "<option value=\"%s\"%s>%s</option>", keyword, i == data.content_default ? " selected" : "", localize_keyword("print-content-optimize", keyword, text, sizeof(text)));
  }
  papplClientHTMLPuts(client, "</select></td></tr>\n");

  // print-scaling-default
  papplClientHTMLPuts(client, "              <tr><th>Scaling:</th><td><select name=\"print-scaling\">");
  for (i = PAPPL_SCALING_AUTO; i <= PAPPL_SCALING_NONE; i *= 2)
  {
    keyword = _papplScalingString(i);
    papplClientHTMLPrintf(client, "<option value=\"%s\"%s>%s</option>", keyword, i == data.scaling_default ? " selected" : "", localize_keyword("print-scaling", keyword, text, sizeof(text)));
  }
  papplClientHTMLPuts(client, "</select></td></tr>\n");

  // printer-resolution-default
  papplClientHTMLPuts(client, "              <tr><th>Resolution:</th><td><select name=\"printer-resolution\">");
  for (i = 0; i < data.num_resolution; i ++)
  {
    if (data.x_resolution[i] != data.y_resolution[i])
      snprintf(text, sizeof(text), "%dx%ddpi", data.x_resolution[i], data.y_resolution[i]);
    else
      snprintf(text, sizeof(text), "%ddpi", data.x_resolution[i]);

    papplClientHTMLPrintf(client, "<option value=\"%s\"%s>%s</option>", text, (data.x_default == data.x_resolution[i] && data.y_default == data.y_resolution[i]) ? " selected" : "", text);
  }
  papplClientHTMLPuts(client, "</select></td></tr>\n");

#if 0
  for (i = 0; i < data.num_source; i ++)
  {
    if (!strcmp(data.source[i], "manual"))
      continue;

    snprintf(name, sizeof(name), "ready%d", i);
    media_chooser(client, &data, localize_keyword("media-source", data.source[i], text, sizeof(text)), name, data.media_ready + i);
  }
#endif // 0

  papplClientHTMLPuts(client,
                      "              <tr><th></th><td><input type=\"submit\" value=\"Save Changes\"></td></tr>\n"
                      "            </tbody>\n"
                      "          </table>"
                      "        </form>\n");

  printer_footer(client);
}


//
// '_papplPrinterWebHome()' - Show the printer home page.
//

void
_papplPrinterWebHome(
    pappl_client_t  *client,		// I - Client
    pappl_printer_t *printer)		// I - Printer
{
  ipp_pstate_t		printer_state;	// Printer state
  char			edit_path[1024];// Edit configuration URL


  printer_state = papplPrinterGetState(printer);

  printer_header(client, printer, NULL, printer_state == IPP_PSTATE_PROCESSING ? 10 : 0);

  papplClientHTMLPuts(client,
                      "      <div class=\"row\">\n"
                      "        <div class=\"col-6\">\n");

  _papplPrinterIteratorWebCallback(printer, client);

  snprintf(edit_path, sizeof(edit_path), "%s/config", printer->uriname);
  papplClientHTMLPrintf(client, "          <h1 class=\"title\">Configuration <a class=\"btn\" href=\"https://%s:%d%s\">Change</a></h1>\n", client->host_field, client->host_port, edit_path);

  _papplClientHTMLInfo(client, false, printer->dns_sd_name, printer->location, printer->geo_location, printer->organization, printer->org_unit, &printer->contact);

  papplClientHTMLPuts(client,
		      "        </div>\n"
                      "        <div class=\"col-6\">\n"
                      "          <h1 class=\"title\">Jobs</h1>\n");

  if (papplPrinterGetNumberOfJobs(printer) > 0)
  {
    papplClientHTMLPuts(client,
			"          <table class=\"list\" summary=\"Jobs\">\n"
			"            <thead>\n"
			"              <tr><th>Job #</th><th>Name</th><th>Owner</th><th>Status</th></tr>\n"
			"            </thead>\n"
			"            <tbody>\n");

    papplPrinterIterateAllJobs(printer, (pappl_job_cb_t)job_cb, client);

    papplClientHTMLPuts(client,
			"            </tbody>\n"
			"          </table>\n");
  }
  else
    papplClientHTMLPuts(client, "        <p>No jobs in history.</p>\n");

  printer_footer(client);
}


//
// '_papplPrinterWebMedia()' - Show the printer media web page.
//

void
_papplPrinterWebMedia(
    pappl_client_t  *client,		// I - Client
    pappl_printer_t *printer)		// I - Printer
{
  int			i;		// Looping var
  pappl_pdriver_data_t	data;		// Driver data
  char			name[32],	// Prefix (readyN)
			text[256];	// Localized media-souce name
  const char		*status = NULL;	// Status message, if any


  if (!papplClientHTMLAuthorize(client))
    return;

  papplPrinterGetPrintDriverData(printer, &data);

  if (client->operation == HTTP_STATE_POST)
  {
    int			num_form = 0;	// Number of form variable
    cups_option_t	*form = NULL;	// Form variables

    if ((num_form = papplClientGetForm(client, &form)) == 0)
    {
      status = "Invalid form data.";
    }
    else if (!papplClientValidateForm(client, num_form, form))
    {
      status = "Invalid form submission.";
    }
    else
    {
      pwg_media_t	*pwg;		// PWG media info
      pappl_media_col_t	*ready;		// Current ready media
      const char	*value,		// Value of form variable
			*custom_width,	// Custom media width
			*custom_length;	// Custom media length

      memset(data.media_ready, 0, sizeof(data.media_ready));
      for (i = 0, ready = data.media_ready; i < data.num_source; i ++, ready ++)
      {
        // size
        snprintf(name, sizeof(name), "ready%d-size", i);
        if ((value = cupsGetOption(name, num_form, form)) == NULL)
          continue;

        if (!strcmp(value, "custom"))
        {
          snprintf(name, sizeof(name), "ready%d-custom-width", i);
          custom_width = cupsGetOption(name, num_form, form);
          snprintf(name, sizeof(name), "ready%d-custom-length", i);
          custom_length = cupsGetOption(name, num_form, form);

          if (custom_width && custom_length)
          {
            snprintf(ready->size_name, sizeof(ready->size_name), "custom_%s_%.2fx%.2fin", data.source[i], atof(custom_width), atof(custom_length));
            ready->size_width  = (int)(2540.0 * atof(custom_width));
            ready->size_length = (int)(2540.0 * atof(custom_length));
          }
        }
        else if ((pwg = pwgMediaForPWG(value)) != NULL)
        {
          strlcpy(ready->size_name, value, sizeof(ready->size_name));
          ready->size_width  = pwg->width;
          ready->size_length = pwg->length;
        }

        // source
        strlcpy(ready->source, data.source[i], sizeof(ready->source));

        // margins
        snprintf(name, sizeof(name), "ready%d-borderless", i);
        if (cupsGetOption(name, num_form, form))
	{
	  ready->bottom_margin = ready->top_margin = 0;
	  ready->left_margin = ready->right_margin = 0;
	}
	else
	{
	  ready->bottom_margin = ready->top_margin = data.bottom_top;
	  ready->left_margin = ready->right_margin = data.left_right;
	}


        // top-offset
        snprintf(name, sizeof(name), "ready%d-top-offset", i);
        if ((value = cupsGetOption(name, num_form, form)) != NULL)
          ready->top_offset = (int)(100.0 * atof(value));

        // tracking
        snprintf(name, sizeof(name), "ready%d-tracking", i);
        if ((value = cupsGetOption(name, num_form, form)) != NULL)
          ready->tracking = _papplMediaTrackingValue(value);

        // type
        snprintf(name, sizeof(name), "ready%d-type", i);
        if ((value = cupsGetOption(name, num_form, form)) != NULL)
          strlcpy(ready->type, value, sizeof(ready->type));
      }

      papplPrinterSetReadyMedia(printer, data.num_source, data.media_ready);

      status = "Changes saved.";
    }

    cupsFreeOptions(num_form, form);
  }

  printer_header(client, printer, "Media", 0);
  if (status)
    papplClientHTMLPrintf(client, "<div class=\"banner\">%s</div>\n", status);

  papplClientHTMLStartForm(client, client->uri, false);

  papplClientHTMLPuts(client,
		      "          <table class=\"form\">\n"
		      "            <tbody>\n");

  for (i = 0; i < data.num_source; i ++)
  {
    if (!strcmp(data.source[i], "manual"))
      continue;

    snprintf(name, sizeof(name), "ready%d", i);
    media_chooser(client, &data, localize_keyword("media-source", data.source[i], text, sizeof(text)), name, data.media_ready + i);
  }

  papplClientHTMLPuts(client,
                      "              <tr><th></th><td><input type=\"submit\" value=\"Save Changes\"></td></tr>\n"
                      "            </tbody>\n"
                      "          </table>"
		      "        </form>\n"
		      "        <script>function show_hide_custom(name) {\n"
		      "  let selelem = document.forms['form'][name + '-size'];\n"
		      "  let divelem = document.getElementById(name + '-custom');\n"
		      "  if (selelem.selectedIndex == 0)\n"
		      "    divelem.style = 'display: inline-block;';\n"
		      "  else\n"
		      "    divelem.style = 'display: none;';\n"
		      "}</script>\n");

  printer_footer(client);
}


//
// '_papplPrinterWebSupplies()' - Show the printer supplies web page.
//

void
_papplPrinterWebSupplies(
    pappl_client_t  *client,		// I - Client
    pappl_printer_t *printer)		// I - Printer
{
  int		i,			// Looping var
		num_supply;		// Number of supplies
  pappl_supply_t supply[100];		// Supplies
  static const char * const backgrounds[] =
  {
    "url(data:image/png;base64,"
      "iVBORw0KGgoAAAANSUhEUgAAAAwAAAAMCAYAAABWdVznAAAAAXNSR0IArs4c"
      "6QAAAERlWElmTU0AKgAAAAgAAYdpAAQAAAABAAAAGgAAAAAAA6ABAAMAAAAB"
      "AAEAAKACAAQAAAABAAAADKADAAQAAAABAAAADAAAAAATDPpdAAAAaUlEQVQo"
      "FY2R0Q3AIAhEa7siCet0HeKQtGeiwWKR+wH0HWAsRKTHK2ZGWEpExvmJLAuD"
      "LbXWNgHFV7Zzv2sTemHjCsYmS8MfjIbOEMHOsIMnQwYehiwMw6WqNxKr6F/c"
      "oyMYm0yGHYwtHq4fKZD9DnawAAAAAElFTkSuQmCC)",
					// no-color
    "#222",				// black - not 100% black for dark mode UI
    "#0FF",				// cyan
    "#777",				// gray
    "#0C0",				// green
    "#7FF",				// light-cyan
    "#CCC",				// light-gray
    "#FCF",				// light-magenta
    "#F0F",				// magenta
    "#F70",				// orange
    "#707",				// violet
    "#FF0"				// yellow
  };


  num_supply = papplPrinterGetSupplies(printer, (int)(sizeof(supply) / sizeof(supply[0])), supply);

  printer_header(client, printer, "Supplies", 0);

  papplClientHTMLPuts(client,
		      "          <table class=\"meter\" summary=\"Supplies\">\n"
		      "            <thead>\n"
		      "              <tr><th></th><td></td><td></td><td></td><td></td></tr>\n"
		      "            </thead>\n"
		      "            <tbody>\n");

  for (i = 0; i < num_supply; i ++)
  {
    papplClientHTMLPrintf(client, "<tr><th>%s</th><td colspan=\"4\"><span class=\"bar\" style=\"background: %s; padding: 0px %.1f%%;\" title=\"%d%%\"></span><span class=\"bar\" style=\"background: transparent; padding: 0px %.1f%%;\" title=\"%d%%\"></span></td></tr>\n", supply[i].description, backgrounds[supply[i].color], supply[i].level * 0.5, supply[i].level, 50.0 - supply[i].level * 0.5, supply[i].level);
  }

  papplClientHTMLPuts(client,
                      "            </tbody>\n"
                      "            <tfoot>\n"
                      "              <tr><th></th><td></td><td></td><td></td><td></td></tr>\n"
                      "            </tfoot>\n"
                      "          </table>\n");

  printer_footer(client);
}


//
// 'job_cb()' - Job iterator callback.
//

static void
job_cb(pappl_job_t    *job,		// I - Job
       pappl_client_t *client)		// I - Client
{
  char	when[256],			// When job queued/started/finished
	hhmmss[64];			// Time HH:MM:SS


  switch (papplJobGetState(job))
  {
    case IPP_JSTATE_PENDING :
    case IPP_JSTATE_HELD :
	snprintf(when, sizeof(when), "Queued at %s", time_string(papplJobGetTimeCreated(job), hhmmss, sizeof(hhmmss)));
	break;
    case IPP_JSTATE_PROCESSING :
    case IPP_JSTATE_STOPPED :
	snprintf(when, sizeof(when), "Started at %s", time_string(papplJobGetTimeProcessed(job), hhmmss, sizeof(hhmmss)));
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

  papplClientHTMLPrintf(client, "              <tr><td>%d</td><td>%s</td><td>%s</td><td>%s</td></tr>\n", papplJobGetID(job), papplJobGetName(job), papplJobGetUsername(job), when);
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


  // TODO: Do real localization of keywords...
  (void)attrname;

  if (!strcmp(keyword, "bi-level"))
  {
    strlcpy(buffer, "Bi-Level", bufsize);
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
    *buffer = toupper(*buffer);
    for (ptr = buffer + 1; *ptr; ptr ++)
    {
      if (*ptr == '-' && ptr[1])
      {
	*ptr++ = ' ';
	*ptr   = toupper(*ptr);
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
  char		size[128],		// I - Size name string
		source[128],		// I - Source string
		top_offset[128],	// I - Top offset, if any
		tracking[128],		// I - Tracking string, if any
		type[128];		// I - Type string
  const char	*borderless;		// I - Borderless string, if any


  if (!media->size_name[0])
    strlcpy(size, "Unknown", sizeof(size));
  else
    localize_keyword("media", media->size_name, size, sizeof(size));

  if (media->bottom_margin == 0 && media->left_margin == 0 && media->right_margin == 0 && media->top_margin == 0)
    borderless = ", borderless";
  else
    borderless = "";

  if (!media->type[0])
    strlcpy(type, "Unknown", sizeof(type));
  else
    localize_keyword("media-type", media->type, type, sizeof(type));

  if (media->top_offset)
    snprintf(top_offset, sizeof(top_offset), ", %.1fmm offset", media->top_offset / 100.0);
  else
    top_offset[0] = '\0';

  if (media->tracking)
    snprintf(tracking, sizeof(tracking), ", %s tracking", _papplMediaTrackingString(media->tracking));
  else
    tracking[0] = '\0';

  if (include_source)
    snprintf(buffer, bufsize, "%s (%s%s%s%s) from %s", size, type, borderless, top_offset, tracking, localize_keyword("media-source", media->source, source, sizeof(source)));
  else
    snprintf(buffer, bufsize, "%s (%s%s%s%s)", size, type, borderless, top_offset, tracking);

  return (buffer);
}


//
// 'media_chooser()' - Show the media chooser.
//

static void
media_chooser(
    pappl_client_t       *client,	// I - Client
    pappl_pdriver_data_t *driver_data,	// I - Driver data
    const char           *title,	// I - Label/title
    const char           *name,		// I - Base name
    pappl_media_col_t    *media)	// I - Current media values
{
  int		i,			// Looping var
		cur_index = 0,		// Current size index
	        sel_index = 0;		// Selected size index...
  pwg_media_t	*pwg;			// PWG media size info
  char		text[256];		// Human-readable value/text
  const char	*min_size = NULL,	// Minimum size
		*max_size = NULL;	// Maximum size


  // media-size
  papplClientHTMLPrintf(client, "              <tr><th>%s</th><td>", title);
  for (i = 0; i < driver_data->num_media && (!min_size || !max_size); i ++)
  {
    if (!strncmp(driver_data->media[i], "custom_", 7) || !strncmp(driver_data->media[i], "roll_", 5))
    {
      if (strstr(driver_data->media[i], "_min_"))
        min_size = driver_data->media[i];
      else if (strstr(driver_data->media[i], "_max_"))
        max_size = driver_data->media[i];
    }
  }
  if (min_size && max_size)
  {
    papplClientHTMLPrintf(client, "<select name=\"%s-size\" onChange=\"show_hide_custom('%s');\"><option value=\"custom\">Custom Size</option>", name, name);
    cur_index ++;
  }
  else
    papplClientHTMLPrintf(client, "<select name=\"%s-size\">", name);

  for (i = 0; i < driver_data->num_media; i ++)
  {
    if (!strncmp(driver_data->media[i], "custom_", 7) || !strncmp(driver_data->media[i], "roll_", 5))
    {
      if (strstr(driver_data->media[i], "_min_"))
        min_size = driver_data->media[i];
      else if (strstr(driver_data->media[i], "_max_"))
        max_size = driver_data->media[i];

      continue;
    }

    if (!strcmp(driver_data->media[i], media->size_name))
      sel_index = cur_index;

    papplClientHTMLPrintf(client, "<option value=\"%s\"%s>%s</option>", driver_data->media[i], sel_index == cur_index ? " selected" : "", localize_keyword("media", driver_data->media[i], text, sizeof(text)));
    cur_index ++;
  }
  if (min_size && max_size)
  {
    int cur_width, min_width, max_width;// Current/min/max width
    int cur_length, min_length, max_length;
					// Current/min/max length

    if ((pwg = pwgMediaForPWG(min_size)) != NULL)
    {
      min_width  = pwg->width;
      min_length = pwg->length;
    }
    else
    {
      min_width  = 1 * 2540;
      min_length = 1 * 2540;
    }

    if ((pwg = pwgMediaForPWG(max_size)) != NULL)
    {
      max_width  = pwg->width;
      max_length = pwg->length;
    }
    else
    {
      max_width  = 9 * 2540;
      max_length = 22 * 2540;
    }

    if ((cur_width = media->size_width) < min_width)
      cur_width = min_width;
    else if (cur_width > max_width)
      cur_width = max_width;

    if ((cur_length = media->size_length) < min_length)
      cur_length = min_length;
    else if (cur_length > max_length)
      cur_length = max_length;

    papplClientHTMLPrintf(client, "</select><div style=\"display: %s;\" id=\"%s-custom\"><input type=\"number\" name=\"%s-custom-width\" min=\"%.2f\" max=\"%.2f\" value=\"%.2f\" step=\".01\" placeholder=\"Width inches\">x<input type=\"number\" name=\"%s-custom-length\" min=\"%.2f\" max=\"%.2f\" value=\"%.2f\" step=\".01\" placeholder=\"Height inches\"></div>\n", sel_index == 0 ? "inline-block" : "none", name, name, min_width / 2540.0, max_width / 2540.0, cur_width / 2540.0, name, min_length / 2540.0, max_length / 2540.0, cur_length / 2540.0);
  }
  else
    papplClientHTMLPuts(client, "</select>\n");

  if (driver_data->borderless)
  {
    papplClientHTMLPrintf(client, "                <input type=\"checkbox\" name=\"%s-borderless\" value=\"%s\">&nbsp;Borderless\n", name, (!media->bottom_margin && !media->left_margin && !media->right_margin && !media->top_margin) ? "checked" : "");
  }

  // media-top-offset (if needed)
  if (driver_data->top_offset_supported[1])
  {
    papplClientHTMLPrintf(client, "                Offset&nbsp;<input type=\"number\" name=\"%s-top-offset\" min=\"%.1f\" max=\"%.1f\" step=\"0.1\" value=\"%.1f\">&nbsp;mm\n", name, driver_data->top_offset_supported[0] / 100.0, driver_data->top_offset_supported[1] / 100.0, media->top_offset / 100.0);
  }

  // media-tracking (if needed)
  if (driver_data->tracking_supported)
  {
    papplClientHTMLPrintf(client, "                <select name=\"%s-tracking\">", name);
    for (i = PAPPL_MEDIA_TRACKING_CONTINUOUS; i <= PAPPL_MEDIA_TRACKING_WEB; i *= 2)
    {
      const char *val = _papplMediaTrackingString(i);

      if (!(driver_data->tracking_supported & i))
	continue;

      papplClientHTMLPrintf(client, "<option value=\"%s\"%s>%s</option>", val, i == media->tracking ? " selected" : "", localize_keyword("media-tracking", val, text, sizeof(text)));
    }
    papplClientHTMLPuts(client, "</select>\n");
  }

  // media-type
  papplClientHTMLPrintf(client, "                <select name=\"%s-type\">", name);
  for (i = 0; i < driver_data->num_type; i ++)
  {
    papplClientHTMLPrintf(client, "<option value=\"%s\"%s>%s</option>", driver_data->type[i], !strcmp(driver_data->type[i], media->type) ? " selected" : "", localize_keyword("media-type", driver_data->type[i], text, sizeof(text)));
  }
  papplClientHTMLPrintf(client, "</select></td></tr>\n");
}


//
// 'printer_footer()' - Show the footer for printers...
//

static void
printer_footer(pappl_client_t *client)	// I - Client
{
  papplClientHTMLPuts(client,
                      "          </div>\n"
                      "        </div>\n"
                      "      </div>\n");
  papplClientHTMLFooter(client);
}


//
// 'printer_header()' - Show the sub-header for printers, as needed...
//

static void
printer_header(pappl_client_t  *client,	// I - Client
               pappl_printer_t *printer,// I - Printer
               const char      *title,	// I - Title
               int             refresh)	// I - Refresh time in seconds or 0 for none
{
  if (!papplClientRespondHTTP(client, HTTP_STATUS_OK, NULL, "text/html", 0, 0))
    return;


  if (printer->system->options & PAPPL_SOPTIONS_MULTI_QUEUE)
  {
    // Multi-queue mode, need to add the printer name to the title...
    if (title)
    {
      char	full_title[1024];	// Full title

      snprintf(full_title, sizeof(full_title), "%s - %s", title, printer->name);
      papplClientHTMLHeader(client, full_title, refresh);
    }
    else
    {
      papplClientHTMLHeader(client, printer->name, refresh);
    }
  }
  else
  {
    // Single queue mode - the function will automatically add the printer name...
    papplClientHTMLHeader(client, title, refresh);
  }

  if (printer->system->options & PAPPL_SOPTIONS_MULTI_QUEUE)
  {
    int		i;			// Looping var
    char	path[1024];		// Printer path
    static const char * const pages[][2] =
    {					// Printer pages
      { "/config",   "Configuration" },
      { "/media",    "Media" },
      { "/printing", "Printing Defaults" },
      { "/supplies", "Supplies" }
    };

    papplClientHTMLPrintf(client,
			  "    <div class=\"header2\">\n"
			  "      <div class=\"row\">\n"
			  "        <div class=\"col-12 nav\">\n"
			  "          <a class=\"btn\" href=\"%s/\"><img src=\"%s/icon-sm.png\"> %s</a>\n", printer->uriname, printer->uriname, printer->name);

    for (i = 0; i < (int)(sizeof(pages) / sizeof(pages[0])); i ++)
    {
      if (!strcmp(pages[i][0], "/supplies") && !printer->driver_data.has_supplies)
        continue;

      snprintf(path, sizeof(path), "%s%s", printer->uriname, pages[i][0]);
      if (strcmp(path, client->uri))
      {
        if (i == 0 || i == (int)(sizeof(pages) / sizeof(pages[0]) - 1))
          papplClientHTMLPrintf(client, "          <a class=\"btn\" href=\"%s\">%s</a>\n", path, pages[i][1]);
        else
          papplClientHTMLPrintf(client, "          <a class=\"btn\" href=\"https://%s:%d%s\">%s</a>\n", client->host_field, client->host_port, path, pages[i][1]);
      }
      else
        papplClientHTMLPrintf(client, "          <span class=\"active\">%s</span>\n", pages[i][1]);
    }

    papplClientHTMLPuts(client,
			"        </div>\n"
			"      </div>\n"
			"    </div>\n");
  }

  papplClientHTMLPuts(client, "    <div class=\"content\">\n");

  if (title)
    papplClientHTMLPrintf(client,
			  "      <div class=\"row\">\n"
			  "        <div class=\"col-12\">\n"
			  "          <h1 class=\"title\">%s</h1>\n", title);
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
