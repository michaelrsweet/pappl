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
static void	printer_footer(pappl_client_t *client);
static void	printer_header(pappl_client_t *client, pappl_printer_t *printer, const char *title, int refresh);
#if 0
static void	media_chooser(pappl_client_t *client, pappl_printer_t *printer, const char *title, const char *name, pappl_media_col_t *media);
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
  printer_header(client, printer, "Configuration", 0);

  printer_footer(client);
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
  printer_header(client, printer, "Media", 0);

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
// 'printer_footer()' - Show the footer for printers...
//

static void
printer_footer(pappl_client_t *client)	// I - Client
{
  papplClientHTMLPuts(client,
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
// 'media_chooser()' - Show the media chooser.
//

static void
media_chooser(
    pappl_client_t    *client,		// I - Client
    pappl_printer_t   *printer,	// I - Printer
    const char         *title,		// I - Label/title
    const char         *name,		// I - Base name
    pappl_media_col_t *media)		// I - Current media values
{
  int		i;			// Looping var
  pwg_media_t	*pwg;			// PWG media size info
  char		text[256];		// Human-readable value/text
  pappl_driver_t *driver = printer->driver;
					// Driver info


  papplClientHTMLPrintf(client, "<tr><th>%s</th><td><select name=\"%s-size\">", title, name);
  for (i = 0; i < driver->num_media; i ++)
  {
    if (!strncmp(driver->media[i], "roll_", 5))
      continue;

    pwg = pwgMediaForPWG(driver->media[i]);

    if ((pwg->width % 100) == 0)
      snprintf(text, sizeof(text), "%dx%dmm", pwg->width / 100, pwg->length / 100);
    else
      snprintf(text, sizeof(text), "%gx%g\"", pwg->width / 2540.0, pwg->length / 2540.0);

    papplClientHTMLPrintf(client, "<option value=\"%s\"%s>%s</option>", driver->media[i], !strcmp(driver->media[i], media->size_name) ? " selected" : "", text);
  }
  papplClientHTMLPrintf(client, "</select><select name=\"%s-tracking\">", name);
  for (i = PAPPL_MEDIA_TRACKING_CONTINUOUS; i <= PAPPL_MEDIA_TRACKING_WEB; i *= 2)
  {
    const char *val = lprintMediaTrackingString(i);

    if (!(driver->tracking_supported & i))
      continue;

    strlcpy(text, val, sizeof(text));
    text[0] = toupper(text[0]);

    papplClientHTMLPrintf(client, "<option value=\"%s\"%s>%s</option>", val, i == media->tracking ? " selected" : "", text);
  }
  papplClientHTMLPrintf(client, "</select><select name=\"%s-type\">", name);
  for (i = 0; i < driver->num_type; i ++)
  {
    if (!strcmp(driver->type[i], "labels"))
      strlcpy(text, "Cut Labels", sizeof(text));
    else if (!strcmp(driver->type[i], "labels-continuous"))
      strlcpy(text, "Continuous Labels", sizeof(text));
    else if (!strcmp(driver->type[i], "continuous"))
      strlcpy(text, "Continuous Paper", sizeof(text));
    else
      strlcpy(text, driver->type[i], sizeof(text));

    papplClientHTMLPrintf(client, "<option value=\"%s\"%s>%s</option>", driver->type[i], !strcmp(driver->type[i], media->type) ? " selected" : "", text);
  }
  papplClientHTMLPrintf(client, "</select></td></tr>");
}


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
