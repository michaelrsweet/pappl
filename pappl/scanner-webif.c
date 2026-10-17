//
// Scanner web interface functions for the Printer Application Framework
//
// Copyright © 2019-2026 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#include "pappl-private.h"


//
// Local functions...
//

static void	scan_job_cb(pappl_job_t *job, pappl_client_t *client);
static void	scan_job_pager(pappl_client_t *client, pappl_scanner_t *scanner, size_t job_index, size_t limit);
static char	*scan_localize_keyword(pappl_client_t *client, const char *attrname, const char *keyword, char *buffer, size_t bufsize);
static char	*scan_time_string(pappl_client_t *client, time_t tv, char *buffer, size_t bufsize);


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
    size_t		num_form = 0;	// Number of form variables
    cups_option_t	*form = NULL;	// Form variables

    if ((num_form = (size_t)papplClientGetForm(client, &form)) == 0)
    {
      status = _PAPPL_LOC("Invalid form data.");
    }
    else if (!papplClientIsValidForm(client, num_form, form))
    {
      status = _PAPPL_LOC("Invalid form submission.");
    }
    else
    {
      char	path[1024];		// Resource path

      papplScannerCancelAllJobs(scanner);
      papplScannerGetPath(scanner, "jobs", path, sizeof(path));
      papplClientRespondRedirect(client, HTTP_STATUS_FOUND, path);
      cupsFreeOptions((cups_len_t)num_form, form);
      return;
    }

    cupsFreeOptions((cups_len_t)num_form, form);
  }

  _papplScannerWebHeader(client, scanner, _PAPPL_LOC("Cancel All Jobs"), 0, NULL, NULL);

  if (status)
    papplClientHTMLPrintf(client, "<div class=\"banner\">%s</div>\n", papplClientGetLocString(client, status));

  papplClientHTMLStartForm(client, client->uri, false);
  papplClientHTMLPrintf(client, "           <input type=\"submit\" value=\"%s\"></form>", papplClientGetLocString(client, _PAPPL_LOC("Confirm Cancel All")));

  if (papplScannerGetNumberOfActiveJobs(scanner) > 0)
  {
    papplClientHTMLPrintf(client,
			  "          <table class=\"list\" summary=\"Jobs\">\n"
			  "            <thead>\n"
			  "              <tr><th>%s</th><th>%s</th><th>%s</th><th>%s</th><th>%s</th><th></th></tr>\n"
			  "            </thead>\n"
			  "            <tbody>\n", papplClientGetLocString(client, _PAPPL_LOC("Job #")), papplClientGetLocString(client, _PAPPL_LOC("Name")), papplClientGetLocString(client, _PAPPL_LOC("Owner")), papplClientGetLocString(client, _PAPPL_LOC("Pages Scanned")), papplClientGetLocString(client, _PAPPL_LOC("Status")));

    papplScannerIterateActiveJobs(scanner, (pappl_job_cb_t)scan_job_cb, client, 1, 0);

    papplClientHTMLPuts(client,
			"            </tbody>\n"
			"          </table>\n");
  }
  else
  {
    papplClientHTMLPrintf(client, "        <p>%s</p>\n", papplClientGetLocString(client, _PAPPL_LOC("No jobs in history.")));
  }

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
		geo_location[128],	// Geo-location
		organization[128],	// Organization
		org_unit[128];		// Organizational unit
  pappl_contact_t contact;		// Contact info


  if (!papplClientHTMLAuthorize(client))
    return;

  if (client->operation == HTTP_STATE_POST)
  {
    size_t		num_form = 0;	// Number of form variables
    cups_option_t	*form = NULL;	// Form variables

    if ((num_form = (size_t)papplClientGetForm(client, &form)) == 0)
    {
      status = _PAPPL_LOC("Invalid form data.");
    }
    else if (!papplClientIsValidForm(client, num_form, form))
    {
      status = _PAPPL_LOC("Invalid form submission.");
    }
    else
    {
      _papplScannerWebConfigFinalize(scanner, num_form, form);

      status = _PAPPL_LOC("Changes saved.");
    }

    cupsFreeOptions((cups_len_t)num_form, form);
  }

  _papplScannerWebHeader(client, scanner, _PAPPL_LOC("Configuration"), 0, NULL, NULL);
  if (status)
    papplClientHTMLPrintf(client, "<div class=\"banner\">%s</div>\n", papplClientGetLocString(client, status));

  _papplClientHTMLInfo(client, true, papplScannerGetDNSSDName(scanner, dns_sd_name, sizeof(dns_sd_name)), papplScannerGetLocation(scanner, location, sizeof(location)), papplScannerGetGeoLocation(scanner, geo_location, sizeof(geo_location)), papplScannerGetOrganization(scanner, organization, sizeof(organization)), papplScannerGetOrganizationalUnit(scanner, org_unit, sizeof(org_unit)), papplScannerGetContact(scanner, &contact));

  _papplScannerWebFooter(client);
}


//
// '_papplScannerWebConfigFinalize()' - Save changes to the scanner
//                                      configuration.
//

void
_papplScannerWebConfigFinalize(
    pappl_scanner_t *scanner,		// I - Scanner
    size_t          num_form,		// I - Number of form variables
    cups_option_t   *form)		// I - Form variables
{
  const char	*value,			// Form value
		*geo_lat,		// Geo-location latitude
		*geo_lon,		// Geo-location longitude
		*contact_name,		// Contact name
		*contact_email,		// Contact email
		*contact_tel;		// Contact telephone number


  if ((value = cupsGetOption("dns_sd_name", (cups_len_t)num_form, form)) != NULL)
    papplScannerSetDNSSDName(scanner, *value ? value : NULL);

  if ((value = cupsGetOption("location", (cups_len_t)num_form, form)) != NULL)
    papplScannerSetLocation(scanner, *value ? value : NULL);

  geo_lat = cupsGetOption("geo_location_lat", (cups_len_t)num_form, form);
  geo_lon = cupsGetOption("geo_location_lon", (cups_len_t)num_form, form);
  if (geo_lat && geo_lon)
  {
    char	uri[1024];		// "geo:" URI

    if (*geo_lat && *geo_lon)
    {
      snprintf(uri, sizeof(uri), "geo:%g,%g", strtod(geo_lat, NULL), strtod(geo_lon, NULL));
      papplScannerSetGeoLocation(scanner, uri);
    }
    else
    {
      papplScannerSetGeoLocation(scanner, NULL);
    }
  }

  if ((value = cupsGetOption("organization", (cups_len_t)num_form, form)) != NULL)
    papplScannerSetOrganization(scanner, *value ? value : NULL);

  if ((value = cupsGetOption("organizational_unit", (cups_len_t)num_form, form)) != NULL)
    papplScannerSetOrganizationalUnit(scanner, *value ? value : NULL);

  contact_name  = cupsGetOption("contact_name", (cups_len_t)num_form, form);
  contact_email = cupsGetOption("contact_email", (cups_len_t)num_form, form);
  contact_tel   = cupsGetOption("contact_telephone", (cups_len_t)num_form, form);
  if (contact_name || contact_email || contact_tel)
  {
    pappl_contact_t	contact;	// Contact info

    memset(&contact, 0, sizeof(contact));

    if (contact_name)
      cupsCopyString(contact.name, contact_name, sizeof(contact.name));
    if (contact_email)
      cupsCopyString(contact.email, contact_email, sizeof(contact.email));
    if (contact_tel)
      cupsCopyString(contact.telephone, contact_tel, sizeof(contact.telephone));

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
  size_t		i;		// Looping var
  pappl_sc_driver_data_t data;		// Driver data
  char			text[256];	// Localized text for keyword
  const char		*status = NULL;	// Status message, if any
  static const char * const color_mode_strings[] =
  {					// Color mode strings
    "black-and-white",
    "grayscale",
    "grayscale16",
    "color",
    "color48"
  };
  static const pappl_scan_color_mode_t color_mode_values[] =
  {					// Color mode values
    PAPPL_SCAN_COLOR_MODE_BLACK_AND_WHITE_1,
    PAPPL_SCAN_COLOR_MODE_GRAYSCALE_8,
    PAPPL_SCAN_COLOR_MODE_GRAYSCALE_16,
    PAPPL_SCAN_COLOR_MODE_RGB_24,
    PAPPL_SCAN_COLOR_MODE_RGB_48
  };
  static const char * const intent_strings[] =
  {					// Intent strings
    "document",
    "photo",
    "preview",
    "text-and-graphic",
    "business-card"
  };
  static const pappl_scan_intent_t intent_values[] =
  {					// Intent values
    PAPPL_SCAN_INTENT_DOCUMENT,
    PAPPL_SCAN_INTENT_PHOTO,
    PAPPL_SCAN_INTENT_PREVIEW,
    PAPPL_SCAN_INTENT_TEXT_AND_GRAPHIC,
    PAPPL_SCAN_INTENT_BUSINESS_CARD
  };
  static const char * const source_strings[] =
  {					// Input source strings
    "platen",
    "adf",
    "camera"
  };
  static const pappl_scan_input_source_t source_values[] =
  {					// Input source values
    PAPPL_SCAN_INPUT_SOURCE_PLATEN,
    PAPPL_SCAN_INPUT_SOURCE_ADF,
    PAPPL_SCAN_INPUT_SOURCE_CAMERA
  };
  static const char * const content_strings[] =
  {					// Content type strings
    "auto",
    "halftone",
    "line-art",
    "magazine",
    "photo",
    "text",
    "text-and-photo"
  };
  static const pappl_scan_content_t content_values[] =
  {					// Content type values
    PAPPL_SCAN_CONTENT_AUTO,
    PAPPL_SCAN_CONTENT_HALFTONE,
    PAPPL_SCAN_CONTENT_LINE_ART,
    PAPPL_SCAN_CONTENT_MAGAZINE,
    PAPPL_SCAN_CONTENT_PHOTO,
    PAPPL_SCAN_CONTENT_TEXT,
    PAPPL_SCAN_CONTENT_TEXT_AND_PHOTO
  };


  if (!papplClientHTMLAuthorize(client))
    return;

  papplScannerGetDriverData(scanner, &data);

  if (client->operation == HTTP_STATE_POST)
  {
    size_t		num_form = 0;	// Number of form variables
    cups_option_t	*form = NULL;	// Form variables

    if ((num_form = (size_t)papplClientGetForm(client, &form)) == 0)
    {
      status = _PAPPL_LOC("Invalid form data.");
    }
    else if (!papplClientIsValidForm(client, num_form, form))
    {
      status = _PAPPL_LOC("Invalid form submission.");
    }
    else
    {
      const char	*value;		// Value of form variable

      if ((value = cupsGetOption("scan-color-mode", (cups_len_t)num_form, form)) != NULL)
      {
        for (i = 0; i < (sizeof(color_mode_strings) / sizeof(color_mode_strings[0])); i ++)
        {
          if (!strcmp(value, color_mode_strings[i]))
          {
            data.color_default = color_mode_values[i];
            break;
          }
        }
      }

      if ((value = cupsGetOption("scan-intent", (cups_len_t)num_form, form)) != NULL)
      {
        for (i = 0; i < (sizeof(intent_strings) / sizeof(intent_strings[0])); i ++)
        {
          if (!strcmp(value, intent_strings[i]))
          {
            data.intent_default = intent_values[i];
            break;
          }
        }
      }

      if ((value = cupsGetOption("input-source", (cups_len_t)num_form, form)) != NULL)
      {
        for (i = 0; i < (sizeof(source_strings) / sizeof(source_strings[0])); i ++)
        {
          if (!strcmp(value, source_strings[i]))
          {
            data.input_source_default = source_values[i];
            break;
          }
        }
      }

      if ((value = cupsGetOption("scan-content-type", (cups_len_t)num_form, form)) != NULL)
      {
        for (i = 0; i < (sizeof(content_strings) / sizeof(content_strings[0])); i ++)
        {
          if (!strcmp(value, content_strings[i]))
          {
            data.content_default = content_values[i];
            break;
          }
        }
      }

      if ((value = cupsGetOption("scanner-resolution", (cups_len_t)num_form, form)) != NULL)
      {
        if (sscanf(value, "%dx%ddpi", &data.x_default, &data.y_default) == 1)
          data.y_default = data.x_default;
      }

      if (papplScannerSetDriverDefaults(scanner, &data))
        status = _PAPPL_LOC("Changes saved.");
      else
        status = _PAPPL_LOC("Bad scanner defaults.");
    }

    cupsFreeOptions((cups_len_t)num_form, form);
  }

  _papplScannerWebHeader(client, scanner, _PAPPL_LOC("Scanning Defaults"), 0, NULL, NULL);
  if (status)
    papplClientHTMLPrintf(client, "<div class=\"banner\">%s</div>\n", papplClientGetLocString(client, status));

  papplClientHTMLStartForm(client, client->uri, false);

  papplClientHTMLPuts(client,
		      "          <table class=\"form\">\n"
		      "            <tbody>\n");

  // scan-color-mode-default
  papplClientHTMLPrintf(client, "              <tr><th>%s:</th><td>", papplClientGetLocString(client, "scan-color-mode"));
  for (i = 0; i < (sizeof(color_mode_values) / sizeof(color_mode_values[0])); i ++)
  {
    if (data.color_supported & color_mode_values[i])
    {
      papplClientHTMLPrintf(client, "<label><input type=\"radio\" name=\"scan-color-mode\" value=\"%s\"%s> %s</label> ", color_mode_strings[i], color_mode_values[i] == data.color_default ? " checked" : "", scan_localize_keyword(client, "scan-color-mode", color_mode_strings[i], text, sizeof(text)));
    }
  }
  papplClientHTMLPuts(client, "</td></tr>\n");

  // scan-intent-default
  papplClientHTMLPrintf(client, "              <tr><th>%s:</th><td>", papplClientGetLocString(client, "scan-intent"));
  for (i = 0; i < (sizeof(intent_values) / sizeof(intent_values[0])); i ++)
  {
    if (data.intents_supported & intent_values[i])
    {
      papplClientHTMLPrintf(client, "<label><input type=\"radio\" name=\"scan-intent\" value=\"%s\"%s> %s</label> ", intent_strings[i], intent_values[i] == data.intent_default ? " checked" : "", scan_localize_keyword(client, "scan-intent", intent_strings[i], text, sizeof(text)));
    }
  }
  papplClientHTMLPuts(client, "</td></tr>\n");

  // input-source-default
  papplClientHTMLPrintf(client, "              <tr><th>%s:</th><td>", papplClientGetLocString(client, "input-source"));
  for (i = 0; i < (sizeof(source_values) / sizeof(source_values[0])); i ++)
  {
    if (data.input_sources_supported & source_values[i])
    {
      papplClientHTMLPrintf(client, "<label><input type=\"radio\" name=\"input-source\" value=\"%s\"%s> %s</label> ", source_strings[i], source_values[i] == data.input_source_default ? " checked" : "", scan_localize_keyword(client, "input-source", source_strings[i], text, sizeof(text)));
    }
  }
  papplClientHTMLPuts(client, "</td></tr>\n");

  // scan-content-type-default
  papplClientHTMLPrintf(client, "              <tr><th>%s:</th><td><select name=\"scan-content-type\">", papplClientGetLocString(client, "scan-content-type"));
  for (i = 0; i < (sizeof(content_values) / sizeof(content_values[0])); i ++)
  {
    if (data.content_supported & content_values[i])
    {
      papplClientHTMLPrintf(client, "<option value=\"%s\"%s>%s</option>", content_strings[i], content_values[i] == data.content_default ? " selected" : "", scan_localize_keyword(client, "scan-content-type", content_strings[i], text, sizeof(text)));
    }
  }
  papplClientHTMLPuts(client, "</select></td></tr>\n");

  // scanner-resolution-default
  papplClientHTMLPrintf(client, "              <tr><th>%s:</th><td>", papplClientGetLocString(client, "scanner-resolution"));
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

  papplClientHTMLPrintf(client,
                        "              <tr><th></th><td><input type=\"submit\" value=\"%s\"></td></tr>\n"
                        "            </tbody>\n"
                        "          </table>"
                        "        </form>\n", papplClientGetLocString(client, _PAPPL_LOC("Save Changes")));

  _papplScannerWebFooter(client);
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
    size_t		num_form = 0;	// Number of form variables
    cups_option_t	*form = NULL;	// Form variables

    if ((num_form = (size_t)papplClientGetForm(client, &form)) == 0)
    {
      status = _PAPPL_LOC("Invalid form data.");
    }
    else if (!papplClientIsValidForm(client, num_form, form))
    {
      status = _PAPPL_LOC("Invalid form submission.");
    }
    else if (scanner->processing_job)
    {
      // Scanner is processing a job...
      status = _PAPPL_LOC("Scanner is currently active.");
    }
    else
    {
      if (!papplScannerIsDeleted(scanner))
      {
        papplScannerDelete(scanner);
        scanner = NULL;
      }

      papplClientRespondRedirect(client, HTTP_STATUS_FOUND, "/");
      cupsFreeOptions((cups_len_t)num_form, form);
      return;
    }

    cupsFreeOptions((cups_len_t)num_form, form);
  }

  _papplScannerWebHeader(client, scanner, _PAPPL_LOC("Delete Scanner"), 0, NULL, NULL);

  if (status)
    papplClientHTMLPrintf(client, "          <div class=\"banner\">%s</div>\n", papplClientGetLocString(client, status));

  papplClientHTMLStartForm(client, client->uri, false);
  papplClientHTMLPrintf(client, "          <input type=\"submit\" value=\"%s\"></form>", papplClientGetLocString(client, _PAPPL_LOC("Confirm Delete Scanner")));

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
  const size_t	limit = 20;		// Jobs per page
  size_t	job_index = 1;		// Job index
  char		dns_sd_name[64],	// Scanner DNS-SD name
		location[128],		// Scanner location
		geo_location[128],	// Scanner geo-location
		organization[256],	// Scanner organization
		org_unit[256];		// Scanner organizational unit
  pappl_contact_t contact;		// Scanner contact


  // Save current scanner state...
  scanner_state = papplScannerGetState(scanner);

  // Handle POSTs for actions...
  if (client->operation == HTTP_STATE_POST)
  {
    size_t		num_form = 0;	// Number of form variables
    cups_option_t	*form = NULL;	// Form variables
    const char		*action;	// Form action

    if ((num_form = papplClientGetForm(client, &form)) == 0)
    {
      status = _PAPPL_LOC("Invalid form data.");
    }
    else if (!papplClientIsValidForm(client, num_form, form))
    {
      status = _PAPPL_LOC("Invalid form submission.");
    }
    else if ((action = cupsGetOption("action", (cups_len_t)num_form, form)) == NULL)
    {
      status = _PAPPL_LOC("Missing action.");
    }
    else if (!strcmp(action, "pause-scanner"))
    {
      _papplRWLockWrite(scanner);
      scanner->state      = IPP_PSTATE_STOPPED;
      scanner->state_time = time(NULL);
      _papplRWUnlock(scanner);

      status = _PAPPL_LOC("Scanner paused.");
    }
    else if (!strcmp(action, "resume-scanner"))
    {
      _papplRWLockWrite(scanner);
      scanner->state      = IPP_PSTATE_IDLE;
      scanner->state_time = time(NULL);
      _papplRWUnlock(scanner);
      _papplScannerCheckJobsNoLock(scanner);

      status = _PAPPL_LOC("Scanner resuming.");
    }
    else if (!strcmp(action, "set-as-default"))
    {
      papplSystemSetDefaultScannerID(scanner->system, scanner->scanner_id);
      status = _PAPPL_LOC("Default scanner set.");
    }
    else
    {
      status = _PAPPL_LOC("Unknown action.");
    }

    cupsFreeOptions((cups_len_t)num_form, form);
  }

  // Show status...
  _papplScannerWebHeader(client, scanner, NULL, scanner_state == IPP_PSTATE_PROCESSING ? 10 : 0, NULL, NULL);

  papplClientHTMLPuts(client,
                      "      <div class=\"row\">\n"
                      "        <div class=\"col-6\">\n");

  _papplScannerWebIteratorCallback(scanner, client);

  if (status)
    papplClientHTMLPrintf(client, "<div class=\"banner\">%s</div>\n", papplClientGetLocString(client, status));

  papplScannerGetPath(scanner, "config", edit_path, sizeof(edit_path));
  papplClientHTMLPrintf(client, "          <h1 class=\"title\">%s <a class=\"btn\" href=\"%s://%s:%d%s\">%s</a></h1>\n", papplClientGetLocString(client, _PAPPL_LOC("Configuration")), _papplClientGetAuthWebScheme(client), client->host_field, client->host_port, edit_path, papplClientGetLocString(client, _PAPPL_LOC("Change")));

  _papplClientHTMLPutLinks(client, scanner->links, PAPPL_LOPTIONS_CONFIGURATION);

  _papplClientHTMLInfo(client, false, papplScannerGetDNSSDName(scanner, dns_sd_name, sizeof(dns_sd_name)), papplScannerGetLocation(scanner, location, sizeof(location)), papplScannerGetGeoLocation(scanner, geo_location, sizeof(geo_location)), papplScannerGetOrganization(scanner, organization, sizeof(organization)), papplScannerGetOrganizationalUnit(scanner, org_unit, sizeof(org_unit)), papplScannerGetContact(scanner, &contact));

  if (!(scanner->system->options & PAPPL_SOPTIONS_MULTI_QUEUE))
    _papplSystemWebSettings(client);

  papplClientHTMLPuts(client, "        </div>\n");

  // Jobs column...
  papplClientHTMLPuts(client, "        <div class=\"col-6\">\n");

  {
    char	jobs_path[1024];	// Jobs path

    papplScannerGetPath(scanner, "jobs", jobs_path, sizeof(jobs_path));
    papplClientHTMLPrintf(client, "          <h1 class=\"title\"><a href=\"%s\">%s</a>", jobs_path, papplClientGetLocString(client, _PAPPL_LOC("Jobs")));
  }

  if (papplScannerGetNumberOfJobs(scanner) > 0)
  {
    if (cupsArrayGetCount(scanner->active_jobs) > 0)
    {
      char	cancelall_path[1024];	// Cancel all path

      papplScannerGetPath(scanner, "cancelall", cancelall_path, sizeof(cancelall_path));
      papplClientHTMLPrintf(client, " <a class=\"btn\" href=\"%s://%s:%d%s\">%s</a></h1>\n", _papplClientGetAuthWebScheme(client), client->host_field, client->host_port, cancelall_path, papplClientGetLocString(client, _PAPPL_LOC("Cancel All Jobs")));
    }
    else
    {
      papplClientHTMLPuts(client, "</h1>\n");
    }

    _papplClientHTMLPutLinks(client, scanner->links, PAPPL_LOPTIONS_JOB);

    scan_job_pager(client, scanner, job_index, limit);

    papplClientHTMLPrintf(client,
			  "          <table class=\"list\" summary=\"Jobs\">\n"
			  "            <thead>\n"
			  "              <tr><th>%s</th><th>%s</th><th>%s</th><th>%s</th><th>%s</th><th></th></tr>\n"
			  "            </thead>\n"
			  "            <tbody>\n", papplClientGetLocString(client, _PAPPL_LOC("Job #")), papplClientGetLocString(client, _PAPPL_LOC("Name")), papplClientGetLocString(client, _PAPPL_LOC("Owner")), papplClientGetLocString(client, _PAPPL_LOC("Pages")), papplClientGetLocString(client, _PAPPL_LOC("Status")));

    papplScannerIterateAllJobs(scanner, (pappl_job_cb_t)scan_job_cb, client, job_index, limit);

    papplClientHTMLPuts(client,
			"            </tbody>\n"
			"          </table>\n");

    scan_job_pager(client, scanner, job_index, limit);
  }
  else
  {
    papplClientHTMLPuts(client, "</h1>\n");
    _papplClientHTMLPutLinks(client, scanner->links, PAPPL_LOPTIONS_JOB);
    papplClientHTMLPrintf(client, "        <p>%s</p>\n", papplClientGetLocString(client, _PAPPL_LOC("No jobs in history.")));
  }

  _papplScannerWebFooter(client);
}


//
// '_papplScannerWebIteratorCallback()' - Show the scanner status.
//

void
_papplScannerWebIteratorCallback(
    pappl_scanner_t *scanner,		// I - Scanner
    pappl_client_t  *client)		// I - Client
{
  pappl_preason_t	reason,		// Current reason
			scanner_reasons;// Scanner state reasons
  ipp_pstate_t		scanner_state;	// Scanner state
  size_t		scanner_jobs;	// Number of queued jobs
  char			state_str[8],	// State string
			jobs_str[256],	// Number of jobs string
			uri[256],	// Form URI
			text[1024];	// Localized text


  scanner_jobs    = papplScannerGetNumberOfActiveJobs(scanner);
  scanner_state   = papplScannerGetState(scanner);
  scanner_reasons = papplScannerGetReasons(scanner);

  snprintf(uri, sizeof(uri), "%s/", scanner->uriname);

  if (!strcmp(client->uri, "/") && (client->system->options & PAPPL_SOPTIONS_MULTI_QUEUE))
  {
    char	delete_path[1024];	// Delete path

    papplScannerGetPath(scanner, "delete", delete_path, sizeof(delete_path));
    papplClientHTMLPrintf(client,
			  "          <h2 class=\"title\"><a href=\"%s/\">%s</a> <a class=\"btn\" href=\"%s://%s:%d%s\">%s</a></h2>\n", scanner->uriname, scanner->name, _papplClientGetAuthWebScheme(client), client->host_field, client->host_port, delete_path, papplClientGetLocString(client, _PAPPL_LOC("Delete")));
  }
  else
  {
    papplClientHTMLPrintf(client, "          <h1 class=\"title\">%s</h1>\n", papplClientGetLocString(client, _PAPPL_LOC("Status")));
  }

  snprintf(state_str, sizeof(state_str), "%d", (int)scanner_state);
  papplLocFormatString(papplClientGetLoc(client), jobs_str, sizeof(jobs_str), scanner_jobs == 1 ? _PAPPL_LOC("%d job") : _PAPPL_LOC("%d jobs"), (int)scanner_jobs);

  papplClientHTMLPrintf(client,
			"          <p><img class=\"%s\" src=\"%s/icon-md.png\">%s, %s", ippEnumString("printer-state", (int)scanner_state), scanner->uriname, scan_localize_keyword(client, "scanner-state", state_str, text, sizeof(text)), jobs_str);

  if ((scanner->system->options & PAPPL_SOPTIONS_MULTI_QUEUE) && scanner->scanner_id == scanner->system->default_scanner_id)
    papplClientHTMLPrintf(client, ", %s", papplClientGetLocString(client, _PAPPL_LOC("default scanner")));

  for (reason = PAPPL_PREASON_OTHER; reason <= PAPPL_PREASON_TONER_LOW; reason *= 2)
  {
    if (scanner_reasons & reason)
      papplClientHTMLPrintf(client, ", %s", scan_localize_keyword(client, "printer-state-reasons", _papplPrinterReasonString(reason), text, sizeof(text)));
  }

  if (strcmp(scanner->name, scanner->driver_data.make_and_model))
    papplClientHTMLPrintf(client, ".<br>%s</p>\n", scanner->driver_data.make_and_model);
  else
    papplClientHTMLPuts(client, ".</p>\n");

  papplClientHTMLPuts(client, "          <div class=\"btn\">");
  _papplClientHTMLPutLinks(client, scanner->links, PAPPL_LOPTIONS_STATUS);

  if (scanner->system->options & PAPPL_SOPTIONS_MULTI_QUEUE)
  {
    if (scanner->state == IPP_PSTATE_STOPPED)
    {
      papplClientHTMLStartForm(client, uri, false);
      papplClientHTMLPrintf(client, "<input type=\"hidden\" name=\"action\" value=\"resume-scanner\"><input type=\"submit\" value=\"%s\"></form>", papplClientGetLocString(client, _PAPPL_LOC("Resume Scanning")));
    }
    else
    {
      papplClientHTMLStartForm(client, uri, false);
      papplClientHTMLPrintf(client, "<input type=\"hidden\" name=\"action\" value=\"pause-scanner\"><input type=\"submit\" value=\"%s\"></form>", papplClientGetLocString(client, _PAPPL_LOC("Pause Scanning")));
    }

    if (scanner->scanner_id != scanner->system->default_scanner_id)
    {
      papplClientHTMLStartForm(client, uri, false);
      papplClientHTMLPrintf(client, "<input type=\"hidden\" name=\"action\" value=\"set-as-default\"><input type=\"submit\" value=\"%s\"></form>", papplClientGetLocString(client, _PAPPL_LOC("Set as Default")));
    }
  }

  if (strcmp(client->uri, "/") && (client->system->options & PAPPL_SOPTIONS_MULTI_QUEUE))
  {
    char	delete_path[1024];	// Delete path

    papplScannerGetPath(scanner, "delete", delete_path, sizeof(delete_path));
    papplClientHTMLPrintf(client, " <a class=\"btn\" href=\"%s://%s:%d%s\">%s</a>", _papplClientGetAuthWebScheme(client), client->host_field, client->host_port, delete_path, papplClientGetLocString(client, _PAPPL_LOC("Delete Scanner")));
  }

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
  size_t	job_index = 1,		// Job index
		limit = 20;		// Jobs per page
  const char	*status = NULL;		// Status message
  bool		refresh;		// Refresh the window?


  if (!papplClientHTMLAuthorize(client))
    return;

  scanner_state = papplScannerGetState(scanner);
  refresh       = scanner_state == IPP_PSTATE_PROCESSING;

  if (client->operation == HTTP_STATE_GET)
  {
    cups_option_t	*form = NULL;	// Form variables
    size_t		num_form = (size_t)papplClientGetForm(client, &form);
					// Number of form variables
    const char		*value = NULL;	// Value of form variable

    if ((value = cupsGetOption("job-index", (cups_len_t)num_form, form)) != NULL)
      job_index = (size_t)strtol(value, NULL, 10);

    cupsFreeOptions((cups_len_t)num_form, form);
  }
  else if (client->operation == HTTP_STATE_POST)
  {
    size_t		num_form = 0;	// Number of form variables
    cups_option_t	*form = NULL;	// Form variables
    const char		*value;		// Value of form variable
    int			job_id = 0;	// Job ID to cancel
    pappl_job_t		*job;		// Job to cancel
    const char		*action;	// Form action

    if ((num_form = papplClientGetForm(client, &form)) == 0)
    {
      status = _PAPPL_LOC("Invalid form data.");
    }
    else if (!papplClientIsValidForm(client, num_form, form))
    {
      status = _PAPPL_LOC("Invalid form submission.");
    }
    else if ((value = cupsGetOption("job-id", (cups_len_t)num_form, form)) != NULL)
    {
      char *end;			// End of value

      job_id = (int)strtol(value, &end, 10);

      if (errno == ERANGE || *end)
      {
        status = _PAPPL_LOC("Invalid job ID.");
      }
      else if ((job = papplScannerFindJob(scanner, job_id)) != NULL)
      {
	if ((action = cupsGetOption("action", (cups_len_t)num_form, form)) == NULL)
	{
	  status = _PAPPL_LOC("Missing action.");
	}
	else if (!strcmp(action, "cancel-job"))
	{
	  _papplJobCancelScan(job);
	  status = _PAPPL_LOC("Job canceled.");
	}
	else
	{
	  status = _PAPPL_LOC("Unknown action.");
	}
      }
      else
      {
        status = _PAPPL_LOC("Invalid Job ID.");
      }
    }
    else
    {
      status = _PAPPL_LOC("Missing job ID.");
    }

    cupsFreeOptions((cups_len_t)num_form, form);
  }

  if (cupsArrayGetCount(scanner->active_jobs) > 0)
  {
    char	url[1024];		// URL for Cancel All Jobs

    httpAssembleURIf(HTTP_URI_CODING_ALL, url, sizeof(url), "https", NULL, client->host_field, client->host_port, "%s/cancelall", scanner->uriname);

    _papplScannerWebHeader(client, scanner, _PAPPL_LOC("Jobs"), refresh ? 10 : 0, _PAPPL_LOC("Cancel All Jobs"), url);
  }
  else
  {
    _papplScannerWebHeader(client, scanner, _PAPPL_LOC("Jobs"), scanner_state == IPP_PSTATE_PROCESSING ? 10 : 0, NULL, NULL);
  }

  if (status)
    papplClientHTMLPrintf(client,
                          "      <div class=\"row\">\n"
			  "        <div class=\"col-6\">\n"
			  "          <div class=\"banner\">%s</div>\n"
			  "        </div>\n"
			  "      </div>\n", papplClientGetLocString(client, status));

  if (papplScannerGetNumberOfJobs(scanner) > 0)
  {
    scan_job_pager(client, scanner, job_index, limit);

    papplClientHTMLPrintf(client,
			  "          <table class=\"list\" summary=\"Jobs\">\n"
			  "            <thead>\n"
			  "              <tr><th>%s</th><th>%s</th><th>%s</th><th>%s</th><th>%s</th><th></th></tr>\n"
			  "            </thead>\n"
			  "            <tbody>\n", papplClientGetLocString(client, _PAPPL_LOC("Job #")), papplClientGetLocString(client, _PAPPL_LOC("Name")), papplClientGetLocString(client, _PAPPL_LOC("Owner")), papplClientGetLocString(client, _PAPPL_LOC("Pages Scanned")), papplClientGetLocString(client, _PAPPL_LOC("Status")));

    papplScannerIterateAllJobs(scanner, (pappl_job_cb_t)scan_job_cb, client, job_index, limit);

    papplClientHTMLPuts(client,
			"            </tbody>\n"
			"          </table>\n");

    scan_job_pager(client, scanner, job_index, limit);
  }
  else
  {
    papplClientHTMLPrintf(client, "        <p>%s</p>\n", papplClientGetLocString(client, _PAPPL_LOC("No jobs in history.")));
  }

  _papplScannerWebFooter(client);
}


//
// '_papplScannerWebHeader()' - Show the web interface header and title for
//                               scanners.
//

void
_papplScannerWebHeader(
    pappl_client_t  *client,		// I - Client
    pappl_scanner_t *scanner,		// I - Scanner
    const char      *title,		// I - Title
    int             refresh,		// I - Refresh time in seconds or 0
    const char      *label,		// I - Button label or `NULL` for none
    const char      *path_or_url)	// I - Button path or `NULL` for none
{
  if (!papplClientRespond(client, HTTP_STATUS_OK, NULL, "text/html", 0, 0))
    return;

  if (scanner->system->options & PAPPL_SOPTIONS_MULTI_QUEUE)
  {
    // Multi-queue mode, add the scanner name to the title...
    if (title)
    {
      char	full_title[1024];	// Full title

      snprintf(full_title, sizeof(full_title), "%s - %s", papplClientGetLocString(client, title), scanner->name);
      papplClientHTMLHeader(client, full_title, refresh);
    }
    else
    {
      papplClientHTMLHeader(client, scanner->name, refresh);
    }
  }
  else
  {
    // Single queue mode...
    papplClientHTMLHeader(client, title, refresh);
  }

  if (scanner->system->options & PAPPL_SOPTIONS_MULTI_QUEUE)
  {
    _papplRWLockRead(scanner);
    papplClientHTMLPrintf(client,
			  "    <div class=\"header2\">\n"
			  "      <div class=\"row\">\n"
			  "        <div class=\"col-12 nav\"><a class=\"btn\" href=\"%s\">%s:</a>\n", scanner->uriname, scanner->name);
    _papplClientHTMLPutLinks(client, scanner->links, PAPPL_LOPTIONS_NAVIGATION);
    papplClientHTMLPuts(client,
			"        </div>\n"
			"      </div>\n"
			"    </div>\n");
    _papplRWUnlock(scanner);
  }
  else if (client->system->versions[0].sversion[0])
  {
    papplClientHTMLPrintf(client,
			  "    <div class=\"header2\">\n"
			  "      <div class=\"row\">\n"
			  "        <div class=\"col-12 nav\">\n"
			  "          Version %s\n"
			  "        </div>\n"
			  "      </div>\n"
			  "    </div>\n", client->system->versions[0].sversion);
  }

  papplClientHTMLPuts(client, "    <div class=\"content\">\n");

  if (title)
  {
    papplClientHTMLPrintf(client,
			  "      <div class=\"row\">\n"
			  "        <div class=\"col-12\">\n"
			  "          <h1 class=\"title\">%s", papplClientGetLocString(client, title));
    if (label && path_or_url)
      papplClientHTMLPrintf(client, " <a class=\"btn\" href=\"%s\">%s</a>", path_or_url, papplClientGetLocString(client, label));
    papplClientHTMLPuts(client, "</h1>\n");
  }
}


//
// '_papplScannerWebFooter()' - Show the web interface footer for scanners.
//

void
_papplScannerWebFooter(
    pappl_client_t *client)		// I - Client
{
  papplClientHTMLPuts(client,
                      "          </div>\n"
                      "        </div>\n"
                      "      </div>\n");
  papplClientHTMLFooter(client);
}


//
// 'scan_job_cb()' - Job iterator callback for scan jobs.
//

static void
scan_job_cb(pappl_job_t    *job,	// I - Job
            pappl_client_t *client)	// I - Client
{
  bool	show_cancel = false;		// Show the "cancel" button?
  char	uri[256],			// Form URI
	when[256],			// When job queued/started/finished
	hhmmss[64];			// Time HH:MM:SS


  // Build the form URI using the scanner's uriname...
  if (job->scanner)
    snprintf(uri, sizeof(uri), "%s/jobs", job->scanner->uriname);
  else
    cupsCopyString(uri, "/", sizeof(uri));

  switch (papplJobGetState(job))
  {
    case IPP_JSTATE_PENDING :
	show_cancel = true;
	papplLocFormatString(papplClientGetLoc(client), when, sizeof(when), _PAPPL_LOC("Queued %s"), scan_time_string(client, papplJobGetTimeCreated(job), hhmmss, sizeof(hhmmss)));
	break;

    case IPP_JSTATE_HELD :
	show_cancel = true;
	papplLocFormatString(papplClientGetLoc(client), when, sizeof(when), _PAPPL_LOC("Queued %s"), scan_time_string(client, papplJobGetTimeCreated(job), hhmmss, sizeof(hhmmss)));
	break;

    case IPP_JSTATE_PROCESSING :
    case IPP_JSTATE_STOPPED :
	if (papplJobIsCanceled(job))
	{
	  cupsCopyString(when, papplClientGetLocString(client, _PAPPL_LOC("Canceling")), sizeof(when));
	}
	else
	{
	  show_cancel = true;
	  papplLocFormatString(papplClientGetLoc(client), when, sizeof(when), _PAPPL_LOC("Started %s"), scan_time_string(client, papplJobGetTimeProcessed(job), hhmmss, sizeof(hhmmss)));
	}
	break;

    case IPP_JSTATE_ABORTED :
	papplLocFormatString(papplClientGetLoc(client), when, sizeof(when), _PAPPL_LOC("Aborted %s"), scan_time_string(client, papplJobGetTimeCompleted(job), hhmmss, sizeof(hhmmss)));
	break;

    case IPP_JSTATE_CANCELED :
	papplLocFormatString(papplClientGetLoc(client), when, sizeof(when), _PAPPL_LOC("Canceled %s"), scan_time_string(client, papplJobGetTimeCompleted(job), hhmmss, sizeof(hhmmss)));
	break;

    case IPP_JSTATE_COMPLETED :
	papplLocFormatString(papplClientGetLoc(client), when, sizeof(when), _PAPPL_LOC("Completed %s"), scan_time_string(client, papplJobGetTimeCompleted(job), hhmmss, sizeof(hhmmss)));
	break;
  }

  papplClientHTMLPrintf(client, "              <tr><td>%d</td><td>%s</td><td>%s</td><td>%d</td><td>%s</td><td>", papplJobGetID(job), papplJobGetName(job), papplJobGetUsername(job), papplJobGetImpressionsCompleted(job), when);

  if (show_cancel)
  {
    papplClientHTMLStartForm(client, uri, false);
    papplClientHTMLPrintf(client, "<input type=\"hidden\" name=\"action\" value=\"cancel-job\"><input type=\"hidden\" name=\"job-id\" value=\"%d\"><input type=\"submit\" value=\"%s\"></form>", papplJobGetID(job), papplClientGetLocString(client, _PAPPL_LOC("Cancel Job")));
  }

  papplClientHTMLPuts(client, "</td></tr>\n");
}


//
// 'scan_job_pager()' - Show the scan job paging links.
//

static void
scan_job_pager(
    pappl_client_t  *client,		// I - Client
    pappl_scanner_t *scanner,		// I - Scanner
    size_t          job_index,		// I - First job shown (1-based)
    size_t          limit)		// I - Maximum jobs shown
{
  size_t	num_jobs = 0,		// Number of jobs
		num_pages = 0,		// Number of pages
		i,			// Looping var
		page = 0;		// Current page
  char		path[1024];		// Resource path


  if ((num_jobs = papplScannerGetNumberOfJobs(scanner)) <= limit)
    return;

  num_pages = (num_jobs + limit - 1) / limit;
  page      = (job_index - 1) / limit;

  papplScannerGetPath(scanner, "jobs", path, sizeof(path));

  papplClientHTMLPuts(client, "          <div class=\"pager\">");

  if (page > 0)
    papplClientHTMLPrintf(client, "<a class=\"btn\" href=\"%s?job-index=%u\">&laquo;</a>", path, (unsigned)((page - 1) * limit + 1));

  for (i = 0; i < num_pages; i ++)
  {
    if (i == page)
      papplClientHTMLPrintf(client, " %u", (unsigned)i + 1);
    else
      papplClientHTMLPrintf(client, " <a class=\"btn\" href=\"%s?job-index=%u\">%u</a>", path, (unsigned)(i * limit + 1), (unsigned)(i + 1));
  }

  if (page < (num_pages - 1))
    papplClientHTMLPrintf(client, " <a class=\"btn\" href=\"%s?job-index=%u\">&raquo;</a>", path, (unsigned)((page + 1) * limit + 1));

  papplClientHTMLPuts(client, "</div>\n");
}


//
// 'scan_localize_keyword()' - Localize a keyword string.
//

static char *				// O - Localized string
scan_localize_keyword(
    pappl_client_t *client,		// I - Client
    const char     *attrname,		// I - Attribute name
    const char     *keyword,		// I - Keyword string
    char           *buffer,		// I - String buffer
    size_t         bufsize)		// I - String buffer size
{
  char		pair[1024];		// attribute.keyword pair
  const char	*locpair;		// Localized pair
  char		*ptr;			// Pointer into string


  // Try looking up the attribute.keyword pair first...
  snprintf(pair, sizeof(pair), "%s.%s", attrname, keyword);
  locpair = papplClientGetLocString(client, pair);

  if (strcmp(pair, locpair))
  {
    // Have it, copy the localized string...
    cupsCopyString(buffer, locpair, bufsize);
  }
  else
  {
    // No localization, just capitalize the hyphenated words...
    cupsCopyString(buffer, keyword, bufsize);
    *buffer = (char)toupper(*buffer & 255);
    for (ptr = buffer + 1; *ptr; ptr ++)
    {
      if (*ptr == '-' && ptr[1])
      {
	*ptr++ = ' ';
	*ptr   = (char)toupper(*ptr & 255);
      }
    }
  }

  return (buffer);
}


//
// 'scan_time_string()' - Return the local time in hours, minutes, and seconds.
//

static char *				// O - Formatted time string
scan_time_string(
    pappl_client_t *client,		// I - Client
    time_t         tv,			// I - Time value
    char           *buffer,		// I - Buffer
    size_t         bufsize)		// I - Size of buffer
{
  struct tm	date;			// Local time and date
  time_t	age;			// How old is the time?


  // Get the local time in hours, minutes, and seconds...
  localtime_r(&tv, &date);

  // See how long ago this was...
  age = time(NULL) - tv;

  // Format based on the age...
  if (age < 86400)
    papplLocFormatString(papplClientGetLoc(client), buffer, bufsize, _PAPPL_LOC("at %02d:%02d:%02d"), date.tm_hour, date.tm_min, date.tm_sec);
  else if (age < (2 * 86400))
    papplLocFormatString(papplClientGetLoc(client), buffer, bufsize, _PAPPL_LOC("yesterday at %02d:%02d:%02d"), date.tm_hour, date.tm_min, date.tm_sec);
  else if (age < (31 * 86400))
    papplLocFormatString(papplClientGetLoc(client), buffer, bufsize, _PAPPL_LOC("%d days ago at %02d:%02d:%02d"), (int)(age / 86400), date.tm_hour, date.tm_min, date.tm_sec);
  else
    papplLocFormatString(papplClientGetLoc(client), buffer, bufsize, _PAPPL_LOC("%04d-%02d-%02d at %02d:%02d:%02d"), date.tm_year + 1900, date.tm_mon + 1, date.tm_mday, date.tm_hour, date.tm_min, date.tm_sec);

  // Return the formatted string...
  return (buffer);
}
