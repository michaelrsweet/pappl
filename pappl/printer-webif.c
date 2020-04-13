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
static void	media_chooser(pappl_client_t *client, pappl_driver_data_t *driver_data, const char *title, const char *name, pappl_media_col_t *media);
static void	printer_footer(pappl_client_t *client);
static void	printer_header(pappl_client_t *client, pappl_printer_t *printer, const char *title, int refresh);
#if 0
static void	media_parse(const char *name, pappl_media_col_t *media, int num_form, cups_option_t *form);
static int	show_modify(pappl_client_t *client, int printer_id);
#endif // 0
static char	*time_string(time_t tv, char *buffer, size_t bufsize);


//
// '_papplPrinterIteratorWebCallback()' - Show the printer status.
//

void
_papplPrinterIteratorWebCallback(
    pappl_printer_t *printer,		// I - Printer
    pappl_client_t  *client)		// I - Client
{
  ipp_pstate_t		printer_state;	// Printer state
  pappl_driver_data_t	driver_data;	// Printer driver data
  char			value[256];	// String buffer
  int			i;		// Looping var
  pappl_preason_t	reason,		// Current reason
			printer_reasons;// Printer state reasons
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


  printer_state   = papplPrinterGetState(printer);
  printer_reasons = papplPrinterGetReasons(printer);

  papplPrinterGetDriverData(printer, &driver_data);

  if (!strcmp(client->uri, "/") && (client->system->options & PAPPL_SOPTIONS_MULTI_QUEUE))
    papplClientHTMLPrintf(client,
			  "          <h2 class=\"title\"><a href=\"%s/\">%s</a></h2>\n", printer->uriname, printer->name);

  papplClientHTMLPrintf(client,
			"          <p><img class=\"%s\" src=\"%s/icon-md.png\" width=\"64\" height=\"64\">%s", ippEnumString("printer-state", printer_state), printer->uriname, driver_data.make_and_model);
  if (papplPrinterGetLocation(printer, value, sizeof(value)))
    papplClientHTMLPrintf(client, ", %s", value);
  if (papplPrinterGetOrganization(printer, value, sizeof(value)))
  {
    char	orgunit[256];		// Organizational unit

    papplPrinterGetOrganizationalUnit(printer, orgunit, sizeof(orgunit));

    papplClientHTMLPrintf(client, "<br>\n%s%s%s", value, orgunit[0] ? ", " : "", orgunit[0] ? orgunit : "");
  }
  papplClientHTMLPrintf(client,
                        "<br>\n"
			"%s, %d job(s)", printer_state == IPP_PSTATE_IDLE ? "Idle" : printer_state == IPP_PSTATE_PROCESSING ? "Printing" : "Stopped", papplPrinterGetActiveJobs(printer));
  for (i = 0, reason = PAPPL_PREASON_OTHER; reason <= PAPPL_PREASON_TONER_LOW; i ++, reason *= 2)
  {
    if (printer_reasons & reason)
      papplClientHTMLPrintf(client, ", %s", reasons[i]);
  }
  papplClientHTMLPuts(client,
                      ".<br clear=\"all\"></p>\n");

#if 0
  if (client->system->auth_service)
  {
    papplClientHTMLPrintf(client, "<p><button onclick=\"window.location.href='/modify/%d';\">Modify</button> <button onclick=\"window.location.href='/delete/%d';\">Delete</button>", printer->printer_id, printer->printer_id);
    if (printer->printer_id != client->system->default_printer)
      papplClientHTMLPrintf(client, " <button onclick=\"window.location.href='/default/%d';\">Set As Default</button>", printer->printer_id);
    papplClientHTMLPrintf(client, "</p>\n");
  }
#endif // 0
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
  char		path[256],		// Path for config page...
		dns_sd_name[64],	// DNS-SD name
		location[128],		// Location
		geo_location[128],	// Geo-location latitude
		organization[128],	// Organization
		org_unit[128];		// Organizational unit
  pappl_contact_t contact;		// Contact info


  if (client->operation == HTTP_STATE_POST)
  {
    int			num_form = 0;	// Number of form variable
    cups_option_t	*form = NULL;	// Form variables

    if ((num_form = papplClientGetForm(client, &form)) == 0)
      status = "Invalid form data.";
    else if (!papplClientValidateForm(client, num_form, form))
      status = "Invalid form submission.";
    else
    {
      _papplPrinterWebConfigFinalize(printer, num_form, form);

      if (!(printer->system->options & PAPPL_SOPTIONS_MULTI_QUEUE))
        _papplSystemWebConfigFinalize(printer->system, num_form, form);

      cupsFreeOptions(num_form, form);

      status = "Changes saved.";
    }
  }

  printer_header(client, printer, "Configuration", 0);
  if (status)
    papplClientHTMLPrintf(client, "<div class=\"banner\">%s</div>\n", status);

  snprintf(path, sizeof(path), "%s/config", printer->uriname);
  _papplClientHTMLInfo(client, path, papplPrinterGetDNSSDName(printer, dns_sd_name, sizeof(dns_sd_name)), papplPrinterGetLocation(printer, location, sizeof(location)), papplPrinterGetGeoLocation(printer, geo_location, sizeof(geo_location)), papplPrinterGetOrganization(printer, organization, sizeof(organization)), papplPrinterGetOrganizationalUnit(printer, org_unit, sizeof(org_unit)), papplPrinterGetContact(printer, &contact));

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
  printer_header(client, printer, "Printing Defaults", 0);

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

  printer_header(client, printer, "Home", printer_state == IPP_PSTATE_PROCESSING ? 10 : 0);

  _papplPrinterIteratorWebCallback(printer, client);

  papplClientHTMLPuts(client,
		      "          <h2 class=\"title\">Jobs</h2>\n");

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

  snprintf(edit_path, sizeof(edit_path), "%s/config", printer->uriname);

  _papplClientHTMLInfo(client, edit_path, printer->dns_sd_name, printer->location, printer->geo_location, printer->organization, printer->org_unit, &printer->contact);

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
  pappl_driver_data_t	data;		// Driver data
  char			name[32],	// Prefix (readyN)
			text[256];	// Localized media-souce name
  const char		*status = NULL;	// Status message, if any
  bool			is_form = client->operation != HTTP_STATE_POST;
					// Is this a form?


  papplPrinterGetDriverData(printer, &data);

  if (client->operation == HTTP_STATE_POST)
  {
    int			num_form = 0;	// Number of form variable
    cups_option_t	*form = NULL;	// Form variables

    if ((num_form = papplClientGetForm(client, &form)) == 0)
      status = "Invalid form data.";
    else if (!papplClientValidateForm(client, num_form, form))
      status = "Invalid form submission.";
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
        if ((value = cupsGetOption(name, num_form, form)) != NULL)
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
          ready->top_offset = (int)(2540.0 * atof(value));

        // tracking
        snprintf(name, sizeof(name), "ready%d-tracking", i);
        if ((value = cupsGetOption(name, num_form, form)) != NULL)
          ready->tracking = _papplMediaTrackingValue(value);

        // type
        snprintf(name, sizeof(name), "ready%d-type", i);
        if ((value = cupsGetOption(name, num_form, form)) != NULL)
          strlcpy(ready->type, value, sizeof(ready->type));
      }
      cupsFreeOptions(num_form, form);

      papplPrinterSetReadyMedia(printer, data.num_source, data.media_ready);

      status = "Changes saved.";
    }
  }

  printer_header(client, printer, "Media", 0);
  if (status)
    papplClientHTMLPrintf(client, "<div class=\"banner\">%s</div>\n", status);

  if (is_form)
    papplClientHTMLStartForm(client, client->uri);

  papplClientHTMLPuts(client,
		      "          <table class=\"form\">\n"
		      "            <tbody>\n");

  for (i = 0; i < data.num_source; i ++)
  {
    if (!strcmp(data.source[i], "manual"))
      continue;

    if (is_form)
    {
      snprintf(name, sizeof(name), "ready%d", i);
      media_chooser(client, &data, localize_keyword("media-source", data.source[i], text, sizeof(text)), name, data.media_ready + i);
    }
    else
    {
      char	desc[256];		// Description of media

      papplClientHTMLPrintf(client, "          <tr><th>%s</th><td>%s</td></tr>\n", localize_keyword("media-source", data.source[i], text, sizeof(text)), localize_media(data.media_ready + i, false, desc, sizeof(desc)));
    }
  }

  if (is_form)
    papplClientHTMLPuts(client, "              <tr><th></th><td><input type=\"submit\" value=\"Save Changes\"></td></tr>\n");

  papplClientHTMLPuts(client,
                      "            </tbody>\n"
                      "          </table>");
  if (is_form)
    papplClientHTMLPuts(client,
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

  if (!strcmp(keyword, "labels"))
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

  if (media->tracking)
    snprintf(tracking, sizeof(tracking), ", %s tracking", _papplMediaTrackingString(media->tracking));
  else
    tracking[0] = '\0';

  if (include_source)
    snprintf(buffer, bufsize, "%s (%s%s%s) from %s", size, type, borderless, tracking, localize_keyword("media-source", media->source, source, sizeof(source)));
  else
    snprintf(buffer, bufsize, "%s (%s%s%s)", size, type, borderless, tracking);

  return (buffer);
}


//
// 'media_chooser()' - Show the media chooser.
//

static void
media_chooser(
    pappl_client_t      *client,	// I - Client
    pappl_driver_data_t *driver_data,	// I - Driver data
    const char          *title,		// I - Label/title
    const char          *name,		// I - Base name
    pappl_media_col_t   *media)		// I - Current media values
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
    papplClientHTMLPrintf(client, "                Offset&nbsp;<input type=\"number\" name=\"%s-top-offset\" min=\"%.2f\" max=\"%.2f\" value=\"%.2f\">&nbsp;inches\n", name, driver_data->top_offset_supported[0] / 2540.0, driver_data->top_offset_supported[1] / 2540.0, media->top_offset / 2540.0);
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

  papplClientHTMLHeader(client, title, refresh);

  if (printer->system->options & PAPPL_SOPTIONS_MULTI_QUEUE)
  {
    int		i;			// Looping var
    char	path[1024];		// Printer path
    static const char * const pages[][2] =
    {					// Printer pages
      { "/",         "Home" },
      { "/config",   "Configuration" },
      { "/media",    "Media" },
      { "/printing", "Printing Defaults" },
      { "/supplies", "Supplies" }
    };

    papplClientHTMLPrintf(client,
			  "    <div class=\"header2\">\n"
			  "      <div class=\"row\">\n"
			  "        <div class=\"col-12 nav\">\n"
			  "          <a class=\"btn\" href=\"%s/\"><img src=\"%s/icon-sm.png\"></a>\n", printer->uriname, printer->uriname);

    for (i = 0; i < (int)(sizeof(pages) / sizeof(pages[0])); i ++)
    {
      if (!strcmp(pages[i][0], "/supplies") && papplPrinterGetSupplies(printer, 0, NULL) == 0)
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

  papplClientHTMLPrintf(client,
			"    <div class=\"content\">\n"
			"      <div class=\"row\">\n"
			"        <div class=\"col-12\">\n"
			"          <h1 class=\"title\">%s %s</h1>\n", papplPrinterGetName(printer), title);
}


#if 0
//
// 'media_parse()' - Parse media values.
//

static void
media_parse(
    const char         *name,		// I - Base name
    pappl_media_col_t *media,		// I - Media values
    int                num_form,	// I - Number of form values
    cups_option_t      *form)		// I - Form values
{
  char		varname[64];		// Variable name
  const char	*value;			// Variable value


  snprintf(varname, sizeof(varname), "%s-size", name);
  if ((value = cupsGetOption(varname, num_form, form)) != NULL)
  {
    pwg_media_t	*pwg;			// PWG media size

    strlcpy(media->size_name, value, sizeof(media->size_name));

    if ((pwg = pwgMediaForPWG(value)) != NULL)
    {
      media->size_width  = pwg->width;
      media->size_length = pwg->length;
    }
    else
    {
      media->size_width  = 0;
      media->size_length = 0;
    }
  }

  snprintf(varname, sizeof(varname), "%s-tracking", name);
  if ((value = cupsGetOption(varname, num_form, form)) != NULL)
    media->tracking = lprintMediaTrackingValue(value);

  snprintf(varname, sizeof(varname), "%s-type", name);
  if ((value = cupsGetOption(varname, num_form, form)) != NULL)
    strlcpy(media->type, value, sizeof(media->type));
}


//
// 'show_add()' - Show the add printer page.
//

static int				// O - 1 on success, 0 on failure
show_add(pappl_client_t *client)	// I - Client connection
{
  http_status_t	status;			// Authorization status
  int		num_form = 0;		// Number of form variables
  cups_option_t	*form = NULL;		// Form variables
  const char	*session = NULL,	// Session key
		*printer_name = NULL,	// Printer name
		*pappl_driver = NULL,	// Driver name
		*device_uri = NULL,	// Device URI
		*socket_address = NULL,	// Socket device address
		*ptr,			// Pointer into value
		*error = NULL;		// Error message, if any
  int		i,			// Looping var
		num_drivers;		// Number of drivers in list
  const char * const *drivers;		// Driver list


  if ((status = lprintIsAuthorized(client)) != HTTP_STATUS_CONTINUE)
  {
    // Need authentication...
    return (papplClientRespondHTTP(client, status, NULL, NULL, 0));
  }

  if (client->operation == HTTP_STATE_POST)
  {
    // Get form data...
    int	valid = 1;

    num_form       = get_form_data(client, &form);
    session        = cupsGetOption("session-key", num_form, form);
    printer_name   = cupsGetOption("printer-name", num_form, form);
    pappl_driver  = cupsGetOption("lprint-driver", num_form, form);
    device_uri     = cupsGetOption("device-uri", num_form, form);
    socket_address = cupsGetOption("socket-address", num_form, form);

    if (!session || strcmp(session, client->system->session_key))
    {
      valid = 0;
      error = "Bad or missing session key.";
    }

    if (valid && printer_name)
    {
      if (!*printer_name)
      {
	valid = 0;
	error = "Empty printer name.";
      }
      else if (strlen(printer_name) > 127)
      {
        valid = 0;
        error = "Printer name too long.";
      }

      for (ptr = printer_name; valid && *ptr; ptr ++)
      {
        if (!isalnum(*ptr & 255) && *ptr != '.' && *ptr != '-')
        {
          valid = 0;
          error = "Bad printer name - use only letters, numbers, '.', and '-'.";
          break;
        }
      }

      if (valid)
      {
        char	resource[1024];		// Resource path for printer

        snprintf(resource, sizeof(resource), "/ipp/print/%s", printer_name);
        if (lprintFindPrinter(client->system, resource, 0))
        {
          valid = 0;
          error = "A printer with that name already exists.";
	}
      }
    }

    if (valid && pappl_driver && !lprintGetMakeAndModel(pappl_driver))
    {
      valid = 0;
      error = "Bad driver.";
    }

    if (device_uri && strncmp(device_uri, "usb://", 6))
    {
      if (strcmp(device_uri, "socket"))
      {
        valid = 0;
        error = "Bad device.";
      }
      else if (!socket_address || !*socket_address)
      {
        valid = 0;
        error = "Bad network address.";
      }
    }

    if (valid)
    {
      // Add the printer...
      pappl_printer_t	*printer;	// Printer
      char		uri[1024];	// Socket URI

      if (!strcmp(device_uri, "socket"))
      {
        httpAssembleURI(HTTP_URI_CODING_ALL, uri, sizeof(uri), "socket", NULL, socket_address, 9100, "/");
        device_uri = uri;
      }

      printer = lprintCreatePrinter(client->system, 0, printer_name, pappl_driver, device_uri, NULL, NULL, NULL, NULL);

      if (printer)
      {
	if (!client->system->save_time)
	  client->system->save_time = time(NULL) + 1;

	papplClientRespondHTTP(client, HTTP_STATUS_OK, NULL, "text/html", 0);

	papplClientHTMLHeader(client, "Printer Added", 0);
	papplClientHTMLPrintf(client, "<p><button onclick=\"window.location.href='/';\">&lArr; Return to Printers</button> <button onclick=\"window.location.href='/modify/%d';\">Modify Printer</button></p>\n", printer->printer_id);
	papplClientHTMLFooter(client);
	return (1);
      }
      else
        error = "Printer creation failed.";
    }
  }

  papplClientRespondHTTP(client, HTTP_STATUS_OK, NULL, "text/html", 0);

  papplClientHTMLHeader(client, "Add Printer", 0);
  papplClientHTMLPrintf(client, "<p><button onclick=\"window.location.href='/';\">&lArr; Return to Printers</button></p>\n");

  if (error)
    papplClientHTMLPrintf(client, "<blockquote><em>Error:</em> %s</blockquote>\n", error);

  papplClientHTMLPrintf(client, "<form method=\"POST\" action=\"/add\">"
                      "<input name=\"session-key\" type=\"hidden\" value=\"%s\">"
		      "<table class=\"form\">\n"
                      "<tr><th>Name:</th><td><input name=\"printer-name\" value=\"%s\" size=\"32\" placeholder=\"Letters, numbers, '.', and '-'.\"></td></tr>\n"
                      "<tr><th>Device:</th><td><select name=\"device-uri\"><option value=\"socket\">Network Printer</option>", client->system->session_key, printer_name ? printer_name : "");
  lprintListDevices((pappl_device_cb_t)device_cb, client, NULL, NULL);
  papplClientHTMLPrintf(client, "</select><br>\n"
                      "<input name=\"socket-address\" value=\"%s\" size=\"32\" placeholder=\"IP address or hostname\"></td></tr>\n", socket_address ? socket_address : "");
  papplClientHTMLPrintf(client, "<tr><th>Driver:</th><td><select name=\"lprint-driver\">");
  drivers = lprintGetDrivers(&num_drivers);
  for (i = 0; i < num_drivers; i ++)
  {
    papplClientHTMLPrintf(client, "<option value=\"%s\"%s>%s</option>", drivers[i], (pappl_driver && !strcmp(drivers[i], pappl_driver)) ? " selected" : "", lprintGetMakeAndModel(drivers[i]));
  }
  papplClientHTMLPrintf(client, "</select></td></tr>\n");
  papplClientHTMLPrintf(client, "<tr><th></th><td><input type=\"submit\" value=\"Add Printer\"></td></tr>\n"
                      "</table></form>\n");
  papplClientHTMLFooter(client);

  cupsFreeOptions(num_form, form);

  return (1);
}


//
// 'show_default()' - Show the set default printer page.
//

static int				// O - 1 on success, 0 on failure
show_default(pappl_client_t *client,	// I - Client connection
             int             printer_id)// I - Printer ID
{
  pappl_printer_t *printer;		// Printer
  http_status_t	status;			// Authorization status
  int		num_form = 0;		// Number of form variables
  cups_option_t	*form = NULL;		// Form variables
  const char	*session = NULL,	// Session key
		*error = NULL;		// Error message, if any
  char		title[1024];		// Title for page


  if ((printer = lprintFindPrinter(client->system, NULL, printer_id)) == NULL)
  {
    // Printer not found...
    return (papplClientRespondHTTP(client, HTTP_STATUS_NOT_FOUND, NULL, NULL, 0));
  }

  if ((status = lprintIsAuthorized(client)) != HTTP_STATUS_CONTINUE)
  {
    // Need authentication...
    return (papplClientRespondHTTP(client, status, NULL, NULL, 0));
  }

  if (client->operation == HTTP_STATE_POST)
  {
    // Get form data...
    int	valid = 1;

    num_form       = get_form_data(client, &form);
    session        = cupsGetOption("session-key", num_form, form);

    if (!session || strcmp(session, client->system->session_key))
    {
      valid = 0;
      error = "Bad or missing session key.";
    }

    if (valid)
    {
      // Set as default...
      client->system->default_printer = printer_id;
      if (!client->system->save_time)
	client->system->save_time = time(NULL) + 1;

      papplClientRespondHTTP(client, HTTP_STATUS_OK, NULL, "text/html", 0);

      snprintf(title, sizeof(title), "Default Printer Set to '%s'", printer->printer_name);
      papplClientHTMLHeader(client, title, 0);
      papplClientHTMLPrintf(client, "<p><button onclick=\"window.location.href='/';\">&lArr; Return to Printers</button></p>\n");
      papplClientHTMLFooter(client);
      return (1);
    }
  }

  papplClientRespondHTTP(client, HTTP_STATUS_OK, NULL, "text/html", 0);

  snprintf(title, sizeof(title), "Set '%s' As Default Printer", printer->printer_name);
  papplClientHTMLHeader(client, title, 0);
  papplClientHTMLPrintf(client, "<p><button onclick=\"window.location.href='/';\">&lArr; Return to Printers</button></p>\n");

  if (error)
    papplClientHTMLPrintf(client, "<blockquote><em>Error:</em> %s</blockquote>\n", error);

  papplClientHTMLPrintf(client, "<form method=\"POST\" action=\"/default/%d\">"
                      "<input name=\"session-key\" type=\"hidden\" value=\"%s\">"
		      "<table class=\"form\">\n"
		      "<tr><th>Confirm:</th><td><input type=\"submit\" value=\"Set '%s' As Default Printer\"></td></tr>\n"
		      "</table></form>\n", printer_id, client->system->session_key, printer->printer_name);
  papplClientHTMLFooter(client);

  cupsFreeOptions(num_form, form);

  return (1);
}


//
// 'show_delete()' - Show the delete printer page.
//

static int				// O - 1 on success, 0 on failure
show_delete(pappl_client_t *client,	// I - Client connection
            int             printer_id)	// I - Printer ID
{
  pappl_printer_t *printer;		// Printer
  http_status_t	status;			// Authorization status
  int		num_form = 0;		// Number of form variables
  cups_option_t	*form = NULL;		// Form variables
  const char	*session = NULL,	// Session key
		*error = NULL;		// Error message, if any
  char		title[1024];		// Title for page


  if ((printer = lprintFindPrinter(client->system, NULL, printer_id)) == NULL)
  {
    // Printer not found...
    return (papplClientRespondHTTP(client, HTTP_STATUS_NOT_FOUND, NULL, NULL, 0));
  }

  if ((status = lprintIsAuthorized(client)) != HTTP_STATUS_CONTINUE)
  {
    // Need authentication...
    return (papplClientRespondHTTP(client, status, NULL, NULL, 0));
  }

  if (client->operation == HTTP_STATE_POST)
  {
    // Get form data...
    int	valid = 1;

    num_form       = get_form_data(client, &form);
    session        = cupsGetOption("session-key", num_form, form);

    if (!session || strcmp(session, client->system->session_key))
    {
      valid = 0;
      error = "Bad or missing session key.";
    }

    if (valid)
    {
      // Delete printer...
      if (!printer->processing_job)
      {
	lprintDeletePrinter(printer);
	printer = NULL;
      }
      else
	printer->is_deleted = 1;

      if (!client->system->save_time)
	client->system->save_time = time(NULL) + 1;

      papplClientRespondHTTP(client, HTTP_STATUS_OK, NULL, "text/html", 0);

      papplClientHTMLHeader(client, printer ? "Deleting Printer" : "Printer Deleted", 0);
      papplClientHTMLPrintf(client, "<p><button onclick=\"window.location.href='/';\">&lArr; Return to Printers</button></p>\n");
      papplClientHTMLFooter(client);
      return (1);
    }
  }

  papplClientRespondHTTP(client, HTTP_STATUS_OK, NULL, "text/html", 0);

  snprintf(title, sizeof(title), "Delete Printer '%s'", printer->printer_name);
  papplClientHTMLHeader(client, title, 0);
  papplClientHTMLPrintf(client, "<p><button onclick=\"window.location.href='/';\">&lArr; Return to Printers</button></p>\n");

  if (error)
    papplClientHTMLPrintf(client, "<blockquote><em>Error:</em> %s</blockquote>\n", error);

  papplClientHTMLPrintf(client, "<form method=\"POST\" action=\"/delete/%d\">"
                      "<input name=\"session-key\" type=\"hidden\" value=\"%s\">"
		      "<table class=\"form\">\n"
		      "<tr><th>Confirm:</th><td><input type=\"submit\" value=\"Delete Printer '%s'\"></td></tr>\n"
		      "</table></form>\n", printer_id, client->system->session_key, printer->printer_name);
  papplClientHTMLFooter(client);

  cupsFreeOptions(num_form, form);

  return (1);
}


//
// 'show_modify()' - Show the modify printer page.
//

static int				// O - 1 on success, 0 on failure
show_modify(pappl_client_t *client,	// I - Client connection
	    int             printer_id)	// I - Printer ID
{
  pappl_printer_t *printer;		// Printer
  http_status_t	status;			// Authorization status
  int		num_form = 0;		// Number of form variables
  cups_option_t	*form = NULL;		// Form variables
  const char	*session = NULL,	// Session key
		*location = NULL,	// Human-readable location
		*latitude = NULL,	// Latitude
		*longitude = NULL,	// Longitude
		*organization = NULL,	// Organization
		*org_unit = NULL,	// Organizational unit
		*error = NULL;		// Error message, if any
  char		title[1024];		// Title for page
  float		latval = 0.0f,		// Latitude in degrees
		lonval = 0.0f;		// Longitude in degrees


  if ((printer = lprintFindPrinter(client->system, NULL, printer_id)) == NULL)
  {
    // Printer not found...
    return (papplClientRespondHTTP(client, HTTP_STATUS_NOT_FOUND, NULL, NULL, 0));
  }

  if ((status = lprintIsAuthorized(client)) != HTTP_STATUS_CONTINUE)
  {
    // Need authentication...
    return (papplClientRespondHTTP(client, status, NULL, NULL, 0));
  }

  if (client->operation == HTTP_STATE_POST)
  {
    // Get form data...
    int		valid = 1;		// Is form data valid?

    num_form     = get_form_data(client, &form);
    session      = cupsGetOption("session-key", num_form, form);
    location     = cupsGetOption("printer-location", num_form, form);
    latitude     = cupsGetOption("latitude", num_form, form);
    longitude    = cupsGetOption("longitude", num_form, form);
    organization = cupsGetOption("printer-organization", num_form, form);
    org_unit     = cupsGetOption("printer-organizational-unit", num_form, form);

    if (!session || strcmp(session, client->system->session_key))
    {
      valid = 0;
      error = "Bad or missing session key.";
    }

    if (valid && latitude)
    {
      latval = atof(latitude);

      if (*latitude && (!strchr("0123456789.-+", *latitude) || latval < -90.0f || latval > 90.0f))
      {
        valid = 0;
        error = "Bad latitude value.";
      }
    }

    if (valid && longitude)
    {
      lonval = atof(longitude);

      if (*longitude && (!strchr("0123456789.-+", *longitude) || lonval < -180.0f || lonval > 180.0f))
      {
        valid = 0;
        error = "Bad longitude value.";
      }
    }

    if (valid && latitude && longitude && !*latitude != !*longitude)
    {
      valid = 0;
      error = "Both latitude and longitude must be specified.";
    }

    if (valid)
    {
      pthread_rwlock_wrlock(&printer->rwlock);

      if (location)
      {
        free(printer->location);
        printer->location = strdup(location);
      }

      if (latitude && *latitude && longitude && *longitude)
      {
        char geo[1024];			// geo: URI

        snprintf(geo, sizeof(geo), "geo:%g,%g", atof(latitude), atof(longitude));
        free(printer->geo_location);
        printer->geo_location = strdup(geo);
      }
      else if (latitude && longitude)
      {
        free(printer->geo_location);
        printer->geo_location = NULL;
      }

      if (organization)
      {
        free(printer->organization);
        printer->organization = strdup(organization);
      }

      if (org_unit)
      {
        free(printer->org_unit);
        printer->org_unit = strdup(org_unit);
      }

      media_parse("media-ready0", printer->driver->media_ready + 0, num_form, form);
      if (printer->driver->num_source > 1)
        media_parse("media-ready1", printer->driver->media_ready + 1, num_form, form);

      printer->config_time = time(NULL);

      pthread_rwlock_unlock(&printer->rwlock);

      if (!client->system->save_time)
	client->system->save_time = time(NULL) + 1;

      papplClientRespondHTTP(client, HTTP_STATUS_OK, NULL, "text/html", 0);

      papplClientHTMLHeader(client, "Printer Modified", 0);
      papplClientHTMLPrintf(client, "<p><button onclick=\"window.location.href='/';\">&lArr; Return to Printers</button></p>\n");
      papplClientHTMLFooter(client);
      return (1);
    }
  }

  if (!location)
    location = printer->location;

  if (latitude && longitude)
  {
    latval = atof(latitude);
    lonval = atof(longitude);
  }
  else if (printer->geo_location)
    sscanf(printer->geo_location, "geo:%f,%f", &latval, &lonval);

  if (!organization)
    organization = printer->organization;

  if (!org_unit)
    org_unit = printer->org_unit;

  papplClientRespondHTTP(client, HTTP_STATUS_OK, NULL, "text/html", 0);

  snprintf(title, sizeof(title), "Modify Printer '%s'", printer->printer_name);
  papplClientHTMLHeader(client, title, 0);
  papplClientHTMLPrintf(client, "<p><button onclick=\"window.location.href='/';\">&lArr; Return to Printers</button></p>\n");

  if (error)
    papplClientHTMLPrintf(client, "<blockquote><em>Error:</em> %s</blockquote>\n", error);

  papplClientHTMLPrintf(client, "<form method=\"POST\" action=\"/modify/%d\">"
                      "<input name=\"session-key\" type=\"hidden\" value=\"%s\">"
		      "<table class=\"form\">\n", printer_id, client->system->session_key);
  papplClientHTMLPrintf(client, "<tr><th>Location:</th><td><input name=\"printer-location\" value=\"%s\" size=\"32\" placeholder=\"Human-readable location\"></td></tr>\n", location ? location : "");
  papplClientHTMLPrintf(client, "<tr><th>Latitude:</th><td><input name=\"latitude\" type=\"number\" value=\"%g\" min=\"-90\" max=\"90\" step=\"0.000001\" size=\"10\" placeholder=\"Latitude Degrees\"></td></tr>\n", latval);
  papplClientHTMLPrintf(client, "<tr><th>Longitude:</th><td><input name=\"longitude\" type=\"number\" value=\"%g\" min=\"-180\" max=\"180\" step=\"0.000001\" size=\"11\" placeholder=\"Longitude Degrees\"></td></tr>\n", lonval);
  papplClientHTMLPrintf(client, "<tr><th>Organization:</th><td><input name=\"printer-organization\" value=\"%s\" size=\"32\" placeholder=\"Organization name\"></td></tr>\n", organization ? organization : "");
  papplClientHTMLPrintf(client, "<tr><th>Organizational Unit:</th><td><input name=\"printer-organizational-unit\" value=\"%s\" size=\"32\" placeholder=\"Unit/division/group\"></td></tr>\n", org_unit ? org_unit : "");
  media_chooser(client, printer, "Main Roll:", "media-ready0", printer->driver->media_ready + 0);
  if (printer->driver->num_source > 1)
    media_chooser(client, printer, "Second Roll:", "media-ready1", printer->driver->media_ready + 1);
  papplClientHTMLPrintf(client, "<tr><th></th><td><input type=\"submit\" value=\"Modify Printer\"></td></tr>\n"
                      "</table></form>\n");
  papplClientHTMLFooter(client);

  cupsFreeOptions(num_form, form);

  return (1);
}
#endif // 0


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
