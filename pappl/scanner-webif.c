//
// Scanner web interface functions for the Scanner Application Framework
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

// Duplicate of the local functions in scanner-driver.c. TODO:Remove Later
extern const char *InputSourceString(pappl_sc_input_source_t value) _PAPPL_PRIVATE;
extern const char *ResolutionString(int resolution) _PAPPL_PRIVATE;

// Converts resolution to string
const char *ResolutionString(int resolution)
{
  static char res_str[32];
  snprintf(res_str, sizeof(res_str), "%d DPI", resolution);
  return res_str;
}

// Converts input source to string
const char *InputSourceString(pappl_sc_input_source_t value)
{
  switch (value)
  {
  case PAPPL_FLATBED:
  return "Flatbed";
  case PAPPL_ADF:
  return "ADF";
  default:
  return "Unknown";
  }
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
		organization[128];	// Organization
  pappl_contact_t contact;		// Contact info


  if (!papplClientHTMLAuthorize(client))
    return;

  if (client->operation == HTTP_STATE_POST)
  {
    cups_len_t		num_form = 0;	// Number of form variable
    cups_option_t	*form = NULL;	// Form variables

    if ((num_form = (cups_len_t)papplClientGetForm(client, &form)) == 0)
    {
      status = _PAPPL_LOC("Invalid form data.");
    }
    else if (!papplClientIsValidForm(client, (int)num_form, form))
    {
      status = _PAPPL_LOC("Invalid form submission.");
    }
    else
    {
      _papplScannerWebConfigFinalize(scanner, num_form, form);
      status = _PAPPL_LOC("Changes saved.");
    }

    cupsFreeOptions(num_form, form);
  }

  papplClientHTMLScannerHeader(client, scanner, _PAPPL_LOC("Configuration"), 0, NULL, NULL);
  if (status)
    papplClientHTMLPrintf(client, "<div class=\"banner\">%s</div>\n", papplClientGetLocString(client, status));

  _papplClientHTMLInfo(client, true, papplScannerGetDNSSDName(scanner, dns_sd_name, sizeof(dns_sd_name)), papplScannerGetLocation(scanner, location, sizeof(location)), papplScannerGetGeoLocation(scanner, geo_location, sizeof(geo_location)), papplScannerGetOrganization(scanner, organization, sizeof(organization)), NULL , papplScannerGetContact(scanner, &contact));

  papplClientHTMLScannerFooter(client);
}


//
// '_papplScannerWebConfigFinalize()' - Save the changes to the scanner configuration.
//

void
_papplScannerWebConfigFinalize(
    pappl_scanner_t *scanner,		// I - Scanner
    cups_len_t      num_form,		// I - Number of form variables
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

  contact_name  = cupsGetOption("contact_name", num_form, form);
  contact_email = cupsGetOption("contact_email", num_form, form);
  contact_tel   = cupsGetOption("contact_telephone", num_form, form);
  if (contact_name || contact_email || contact_tel)
  {
    pappl_contact_t	contact;	// Contact info

    memset(&contact, 0, sizeof(contact));

    if (contact_name)
      papplCopyString(contact.name, contact_name, sizeof(contact.name));
    if (contact_email)
      papplCopyString(contact.email, contact_email, sizeof(contact.email));
    if (contact_tel)
      papplCopyString(contact.telephone, contact_tel, sizeof(contact.telephone));

    papplScannerSetContact(scanner, &contact);
  }
}

//
// '_papplScannerWebDefaults()' - Show the scanner defaults web page.
//

void
_papplScannerWebDefaults(
    pappl_client_t  *client,    // I - Client
    pappl_scanner_t *scanner)   // I - Scanner
{
    pappl_sc_driver_data_t data;
    const char       *status = NULL;

    if (!papplClientHTMLAuthorize(client))
        return;

    papplScannerGetDriverData(scanner, &data);

    if (client->operation == HTTP_STATE_POST)
    {
        cups_len_t        num_form = 0;
        cups_option_t    *form = NULL;
        int              num_vendor = 0;
        cups_option_t    *vendor = NULL;

        if ((num_form = (cups_len_t)papplClientGetForm(client, &form)) == 0)
        {
            status = _PAPPL_LOC("Invalid form data.");
        }
        else if (!papplClientIsValidForm(client, (int)num_form, form))
        {
            status = _PAPPL_LOC("Invalid form submission.");
        }
        else
        {
            const char *value;
            char *end;

            if ((value = cupsGetOption("document-format", num_form, form)) != NULL)
            {
                strncpy((char *)data.default_document_format, value, sizeof(data.default_document_format) - 1);
            }

            if ((value = cupsGetOption("color-mode", num_form, form)) != NULL)
            {
                data.default_color_mode = (pappl_sc_color_mode_t)_papplColorModeValue(value);
            }

            if ((value = cupsGetOption("resolution", num_form, form)) != NULL)
            {
                data.default_resolution = (int)strtol(value, &end, 10);

                if (errno == ERANGE || *end || data.default_resolution < 0)
                    data.default_resolution = data.default_resolution;
            }

            if ((value = cupsGetOption("input-source", num_form, form)) != NULL)
            {
                for (int i = 0; i < PAPPL_MAX_SOURCES; i++)
                {
                    if (!strcmp(InputSourceString(data.input_sources_supported[i]), value))
                    {
                        data.default_input_source = data.input_sources_supported[i];
                        break;
                    }
                }
            }

            if ((value = cupsGetOption("duplex", num_form, form)) != NULL)
            {
                data.duplex_supported = !strcmp(value, "true");
            }

            if ((value = cupsGetOption("intent", num_form, form)) != NULL)
            {
                strncpy((char *)data.default_intent, value, sizeof(data.default_intent) - 1);
            }

            if ((value = cupsGetOption("scan-area-width", num_form, form)) != NULL)
            {
                data.default_scan_area[0] = (int)strtol(value, &end, 10);
            }

            if ((value = cupsGetOption("scan-area-height", num_form, form)) != NULL)
            {
                data.default_scan_area[1] = (int)strtol(value, &end, 10);
            }

            if ((value = cupsGetOption("brightness", num_form, form)) != NULL)
            {
                data.adjustments.brightness = (int)strtol(value, &end, 10);
            }

            if ((value = cupsGetOption("contrast", num_form, form)) != NULL)
            {
                data.adjustments.contrast = (int)strtol(value, &end, 10);
            }

            if ((value = cupsGetOption("gamma", num_form, form)) != NULL)
            {
                data.adjustments.gamma = (int)strtol(value, &end, 10);
            }

            if ((value = cupsGetOption("threshold", num_form, form)) != NULL)
            {
                data.adjustments.threshold = (int)strtol(value, &end, 10);
            }

            if ((value = cupsGetOption("saturation", num_form, form)) != NULL)
            {
                data.adjustments.saturation = (int)strtol(value, &end, 10);
            }

            if ((value = cupsGetOption("sharpness", num_form, form)) != NULL)
            {
                data.adjustments.sharpness = (int)strtol(value, &end, 10);
            }

            if ((value = cupsGetOption("compression-supported", num_form, form)) != NULL)
            {
                data.compression_supported = !strcmp(value, "true");
            }

            if ((value = cupsGetOption("noise-removal-supported", num_form, form)) != NULL)
            {
                data.noise_removal_supported = !strcmp(value, "true");
            }

            if ((value = cupsGetOption("sharpening-supported", num_form, form)) != NULL)
            {
                data.sharpening_supported = !strcmp(value, "true");
            }

            if ((value = cupsGetOption("binary-rendering-supported", num_form, form)) != NULL)
            {
                data.binary_rendering_supported = !strcmp(value, "true");
            }

            if ((value = cupsGetOption("blank-page-removal-supported", num_form, form)) != NULL)
            {
                data.blank_page_removal_supported = !strcmp(value, "true");
            }

            if (papplScannerSetDriverDefaults(scanner, &data))
                status = _PAPPL_LOC("Changes saved.");
            else
                status = _PAPPL_LOC("Bad scanner defaults.");

            cupsFreeOptions((cups_len_t)num_vendor, vendor);
        }

        cupsFreeOptions(num_form, form);
    }

    papplClientHTMLScannerHeader(client, scanner, _PAPPL_LOC("Scanning Defaults"), 0, NULL, NULL);
    if (status)
        papplClientHTMLPrintf(client, "<div class=\"banner\">%s</div>\n", papplClientGetLocString(client, status));

    papplClientHTMLStartForm(client, client->uri, false);

    papplClientHTMLPuts(client,
        "          <table class=\"form\">\n"
        "            <tbody>\n");

    // Document Format
    papplClientHTMLPrintf(client, "              <tr><th>%s:</th><td>", papplClientGetLocString(client, "document-format"));
    papplClientHTMLPuts(client, "<select name=\"document-format\">");

    for (int i = 0; i < PAPPL_MAX_FORMATS && data.document_formats_supported[i]; i++)
    {
        papplClientHTMLPrintf(client, "<option value=\"%s\"%s>%s</option>", data.document_formats_supported[i],
            !strcmp(data.document_formats_supported[i], data.default_document_format) ? " selected" : "",
            papplClientGetLocString(client, data.document_formats_supported[i]));
    }

    papplClientHTMLPuts(client, "</select></td></tr>\n");

    // Resolution
    papplClientHTMLPrintf(client, "              <tr><th>%s:</th><td>", papplClientGetLocString(client, "resolution"));
    papplClientHTMLPuts(client, "<select name=\"resolution\">");

    for (int i = 0; i < MAX_RESOLUTIONS && data.resolutions[i]; i++)
    {
        papplClientHTMLPrintf(client, "<option value=\"%d\"%s>%ddpi</option>", data.resolutions[i],
            data.resolutions[i] == data.default_resolution ? " selected" : "",
            data.resolutions[i]);
    }

    papplClientHTMLPuts(client, "</select></td></tr>\n");

    // Color Mode
    papplClientHTMLPrintf(client, "              <tr><th>%s:</th><td>", papplClientGetLocString(client, "color-mode"));
    papplClientHTMLPuts(client, "<select name=\"color-mode\">");

    for (int i = 0; i < PAPPL_MAX_COLOR_MODES; i++)
    {
        papplClientHTMLPrintf(client, "<option value=\"%s\"%s>%s</option>", _papplColorModeString(data.color_modes_supported[i]),
            data.color_modes_supported[i] == data.default_color_mode ? " selected" : "",
            papplClientGetLocString(client, _papplColorModeString(data.color_modes_supported[i])));
    }

    papplClientHTMLPuts(client, "</select></td></tr>\n");

    // Input Source
    papplClientHTMLPrintf(client, "              <tr><th>%s:</th><td>", papplClientGetLocString(client, "input-source"));
    papplClientHTMLPuts(client, "<select name=\"input-source\">");

    for (int i = 0; i < PAPPL_MAX_SOURCES; i++)
    {
        papplClientHTMLPrintf(client, "<option value=\"%s\"%s>%s</option>", InputSourceString(data.input_sources_supported[i]),
            data.input_sources_supported[i] == data.default_input_source ? " selected" : "",
            papplClientGetLocString(client, InputSourceString(data.input_sources_supported[i])));
    }

    papplClientHTMLPuts(client, "</select></td></tr>\n");

    // Duplex
    papplClientHTMLPrintf(client, "              <tr><th>%s:</th><td>", papplClientGetLocString(client, "duplex"));
    papplClientHTMLPrintf(client, "<input type=\"checkbox\" name=\"duplex\"%s></td></tr>\n",
                          data.duplex_supported ? " checked" : "");

    // Scan Intent
    papplClientHTMLPrintf(client, "              <tr><th>%s:</th><td>", papplClientGetLocString(client, "intent"));
    papplClientHTMLPuts(client, "<select name=\"intent\">");

    for (int i = 0; i < 5; i++)
    {
        if (data.mandatory_intents[i] || data.optional_intents[i])
        {
            const char *intent = data.mandatory_intents[i] ? data.mandatory_intents[i] : data.optional_intents[i];
            papplClientHTMLPrintf(client, "<option value=\"%s\"%s>%s</option>", intent,
                                  !strcmp(intent, data.default_intent) ? " selected" : "",
                                  papplClientGetLocString(client, intent));
        }
    }

    papplClientHTMLPuts(client, "</select></td></tr>\n");

    // Scan Area Width
    papplClientHTMLPrintf(client, "              <tr><th>%s:</th><td>", papplClientGetLocString(client, "scan-area-width"));
    papplClientHTMLPrintf(client, "<input type=\"number\" name=\"scan-area-width\" value=\"%d\"></td></tr>\n",
                          data.default_scan_area[0]);

    // Scan Area Height
    papplClientHTMLPrintf(client, "              <tr><th>%s:</th><td>", papplClientGetLocString(client, "scan-area-height"));
    papplClientHTMLPrintf(client, "<input type=\"number\" name=\"scan-area-height\" value=\"%d\"></td></tr>\n",
                          data.default_scan_area[1]);

    // Adjustments
    papplClientHTMLPrintf(client, "              <tr><th>%s:</th><td>", papplClientGetLocString(client, "brightness"));
    papplClientHTMLPrintf(client, "<input type=\"number\" name=\"brightness\" value=\"%d\"></td></tr>\n",
                          data.adjustments.brightness);

    papplClientHTMLPrintf(client, "              <tr><th>%s:</th><td>", papplClientGetLocString(client, "contrast"));
    papplClientHTMLPrintf(client, "<input type=\"number\" name=\"contrast\" value=\"%d\"></td></tr>\n",
                          data.adjustments.contrast);

    papplClientHTMLPrintf(client, "              <tr><th>%s:</th><td>", papplClientGetLocString(client, "gamma"));
    papplClientHTMLPrintf(client, "<input type=\"number\" name=\"gamma\" value=\"%d\"></td></tr>\n",
                          data.adjustments.gamma);

    papplClientHTMLPrintf(client, "              <tr><th>%s:</th><td>", papplClientGetLocString(client, "threshold"));
    papplClientHTMLPrintf(client, "<input type=\"number\" name=\"threshold\" value=\"%d\"></td></tr>\n",
                          data.adjustments.threshold);

    papplClientHTMLPrintf(client, "              <tr><th>%s:</th><td>", papplClientGetLocString(client, "saturation"));
    papplClientHTMLPrintf(client, "<input type=\"number\" name=\"saturation\" value=\"%d\"></td></tr>\n",
                          data.adjustments.saturation);

    papplClientHTMLPrintf(client, "              <tr><th>%s:</th><td>", papplClientGetLocString(client, "sharpness"));
    papplClientHTMLPrintf(client, "<input type=\"number\" name=\"sharpness\" value=\"%d\"></td></tr>\n",
                          data.adjustments.sharpness);

    // Blank Page Removal
    papplClientHTMLPrintf(client, "              <tr><th>%s:</th><td>", papplClientGetLocString(client, "blank-page-removal"));
    papplClientHTMLPrintf(client, "<input type=\"checkbox\" name=\"blank-page-removal\"%s></td></tr>\n",
                          data.blank_page_removal_supported ? " checked" : "");


    // Noise Removal
    papplClientHTMLPrintf(client, "              <tr><th>%s:</th><td>", papplClientGetLocString(client, "noise-removal"));
    papplClientHTMLPrintf(client, "<input type=\"checkbox\" name=\"noise-removal\"%s></td></tr>\n",
                          data.noise_removal_supported ? " checked" : "");
    // Sharpening
    papplClientHTMLPrintf(client, "              <tr><th>%s:</th><td>", papplClientGetLocString(client, "sharpening"));
    papplClientHTMLPrintf(client, "<input type=\"checkbox\" name=\"sharpening\"%s></td></tr>\n",
                          data.sharpening_supported ? " checked" : "");

    papplClientHTMLPuts(client,
        "              <tr><th></th><td><input type=\"submit\" value=\"Save Changes\"></td></tr>\n"
        "            </tbody>\n"
        "          </table>\n"
        "        </form>\n");

    papplClientHTMLScannerFooter(client);
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
    cups_len_t		num_form = 0;	// Number of form variables
    cups_option_t	*form = NULL;	// Form variables

    if ((num_form = (cups_len_t)papplClientGetForm(client, &form)) == 0)
    {
      status = _PAPPL_LOC("Invalid form data.");
    }
    else if (!papplClientIsValidForm(client, (int)num_form, form))
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
      cupsFreeOptions(num_form, form);
      return;
    }

    cupsFreeOptions(num_form, form);
  }

  papplClientHTMLScannerHeader(client, scanner, _PAPPL_LOC("Delete Scanner"), 0, NULL, NULL);

  if (status)
    papplClientHTMLPrintf(client, "          <div class=\"banner\">%s</div>\n", papplClientGetLocString(client, status));

  papplClientHTMLStartForm(client, client->uri, false);
  papplClientHTMLPrintf(client,"          <input type=\"submit\" value=\"%s\"></form>", papplClientGetLocString(client, _PAPPL_LOC("Confirm Delete Scanner")));

  papplClientHTMLFooter(client);
}

//
// '_papplScannerWebHome()' - Show the scanner home page.
//

void
_papplScannerWebHome(
    pappl_client_t  *client,        // I - Client
    pappl_scanner_t *scanner)       // I - Scanner
{
  const char    *status = NULL;    // Status text, if any
  escl_sstate_t scanner_state;     // Scanner state
  char          edit_path[1024];   // Edit configuration URL
  // Job index and limit not required as of now
  char          dns_sd_name[64],   // Scanner DNS-SD name
                location[128],     // Scanner location
                geo_location[128], // Scanner geo-location
                organization[256]; // Scanner organization
  pappl_contact_t contact;         // Scanner contact

  // Save current scanner state...
  scanner_state = scanner->state;

  // Handle POSTs to perform scanner actions...
  if (client->operation == HTTP_STATE_POST)
  {
    cups_len_t        num_form = 0;  // Number of form variables
    cups_option_t    *form = NULL;   // Form variables
    const char       *action;        // Form action

    if ((num_form = (cups_len_t)papplClientGetForm(client, &form)) == 0)
    {
      status = _PAPPL_LOC("Invalid form data.");
    }
    else if (!papplClientIsValidForm(client, (int)num_form, form))
    {
      status = _PAPPL_LOC("Invalid form submission.");
    }
    else if ((action = cupsGetOption("action", num_form, form)) == NULL)
    {
      status = _PAPPL_LOC("Missing action.");
    }
    else
    {
      // Handle different actions (e.g., pause, resume, identify, etc.)
      if (!strcmp(action, "identify-scanner"))
      {
        if (scanner->driver_data.identify_cb)
        {
          (scanner->driver_data.identify_cb)(scanner, scanner->driver_data.identify_supported, "Hello.");
          status = _PAPPL_LOC("Scanner identified.");
        }
        else
        {
          status = _PAPPL_LOC("Unable to identify scanner.");
        }
      }
      else if (!strcmp(action, "resume-scanner"))
      {
        papplScannerResume(scanner);
        scanner->state = ESCL_SSTATE_IDLE;
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
    }
    cupsFreeOptions(num_form, form);
    }


     // Show status...
  papplClientHTMLScannerHeader(client, scanner, NULL, scanner_state == ESCL_SSTATE_PROCESSING ? 10 : 0, NULL, NULL);

  papplClientHTMLPuts(client,
                      "      <div class=\"row\">\n"
                      "        <div class=\"col-6\">\n");


  if (status)
    papplClientHTMLPrintf(client, "<div class=\"banner\">%s</div>\n", papplClientGetLocString(client, status));

  snprintf(edit_path, sizeof(edit_path), "%s/config", scanner->uriname);
  papplClientHTMLPrintf(client, "<h1 class=\"title\">%s <a class=\"btn\" href=\"%s://%s:%d%s\">%s</a></h1>\n", papplClientGetLocString(client, _PAPPL_LOC("Configuration")), _papplClientGetAuthWebScheme(client), client->host_field, client->host_port, edit_path, papplClientGetLocString(client, _PAPPL_LOC("Change")));

   // Display scanner information and links
  _papplClientHTMLInfo(client, false,
                       papplScannerGetDNSSDName(scanner, dns_sd_name, sizeof(dns_sd_name)),
                       papplScannerGetLocation(scanner, location, sizeof(location)),
                       papplScannerGetGeoLocation(scanner, geo_location, sizeof(geo_location)),
                       papplScannerGetOrganization(scanner, organization, sizeof(organization)),
                       NULL,
                       papplScannerGetContact(scanner, &contact));

  _papplClientHTMLPutLinks(client, scanner->links, PAPPL_LOPTIONS_CONFIGURATION);

  // Display scanner state
  papplClientHTMLPrintf(client,
                        "        </div>\n"
                        "        <div class=\"col-6\">\n"
                        "          <h1 class=\"title\">%s</h1>\n", papplClientGetLocString(client, _PAPPL_LOC("Scanner Status")));

  if (scanner->state == ESCL_SSTATE_PROCESSING)
  {
    papplClientHTMLPrintf(client, "<p>%s</p>\n", papplClientGetLocString(client, _PAPPL_LOC("Processing")));
  }
  else
  {
    papplClientHTMLPrintf(client, "<p>%s</p>\n", papplClientGetLocString(client, _PAPPL_LOC("Idle")));
  }

  _papplClientHTMLPutLinks(client, scanner->links, PAPPL_LOPTIONS_JOB);

  papplClientHTMLScannerFooter(client);

  // Optional TODO: Add functions to show all completed jobs, right now we only show scanner status and configuration

}
