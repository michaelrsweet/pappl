//
// System web interface functions for the Printer Application Framework
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
#include <net/if.h>
#include <ifaddrs.h>
#ifdef HAVE_GNUTLS
#  include <gnutls/gnutls.h>
#  include <gnutls/x509.h>
#endif // HAVE_GNUTLS


//
// Local functions...
//

static bool	system_device_cb(const char *device_info, const char *device_uri, const char *device_id, void *data);
static void	system_footer(pappl_client_t *client);
static void	system_header(pappl_client_t *client, const char *title);

#ifdef HAVE_GNUTLS
static bool	tls_install_certificate(pappl_client_t *client, const char *crtfile, const char *keyfile);
static bool	tls_install_file(pappl_client_t *client, const char *dst, const char *src);
static bool	tls_make_certificate(pappl_client_t *client, int num_form, cups_option_t *form);
static bool	tls_make_certsignreq(pappl_client_t *client, int num_form, cups_option_t *form, char *crqpath, size_t crqsize);
#endif // HAVE_GNUTLS


//
// Local globals...
//

#ifdef HAVE_GNUTLS
static const char * const countries[][2] =
{					// List of countries and their ISO 3166 2-letter codes
  { "AF", "Afghanistan" },
  { "AX", "Åland Islands" },
  { "AL", "Albania" },
  { "DZ", "Algeria" },
  { "AS", "American Samoa" },
  { "AD", "Andorra" },
  { "AO", "Angola" },
  { "AI", "Anguilla" },
  { "AQ", "Antarctica" },
  { "AG", "Antigua and Barbuda" },
  { "AR", "Argentina" },
  { "AM", "Armenia" },
  { "AW", "Aruba" },
  { "AU", "Australia" },
  { "AT", "Austria" },
  { "AZ", "Azerbaijan" },
  { "BS", "Bahamas" },
  { "BH", "Bahrain" },
  { "BD", "Bangladesh" },
  { "BB", "Barbados" },
  { "BY", "Belarus" },
  { "BE", "Belgium" },
  { "BZ", "Belize" },
  { "BJ", "Benin" },
  { "BM", "Bermuda" },
  { "BT", "Bhutan" },
  { "BO", "Bolivia (Plurinational State of)" },
  { "BQ", "Bonaire, Sint Eustatius and Saba" },
  { "BA", "Bosnia and Herzegovina" },
  { "BW", "Botswana" },
  { "BV", "Bouvet Island" },
  { "BR", "Brazil" },
  { "IO", "British Indian Ocean Territory" },
  { "BN", "Brunei Darussalam" },
  { "BG", "Bulgaria" },
  { "BF", "Burkina Faso" },
  { "BI", "Burundi" },
  { "CV", "Cabo Verde" },
  { "KH", "Cambodia" },
  { "CM", "Cameroon" },
  { "CA", "Canada" },
  { "KY", "Cayman Islands" },
  { "CF", "Central African Republic" },
  { "TD", "Chad" },
  { "CL", "Chile" },
  { "CN", "China" },
  { "CX", "Christmas Island" },
  { "CC", "Cocos (Keeling) Islands" },
  { "CO", "Colombia" },
  { "KM", "Comoros" },
  { "CD", "Congo, Democratic Republic of the" },
  { "CG", "Congo" },
  { "CK", "Cook Islands" },
  { "CR", "Costa Rica" },
  { "CI", "Côte d'Ivoire" },
  { "HR", "Croatia" },
  { "CU", "Cuba" },
  { "CW", "Curaçao" },
  { "CY", "Cyprus" },
  { "CZ", "Czechia" },
  { "DK", "Denmark" },
  { "DJ", "Djibouti" },
  { "DM", "Dominica" },
  { "DO", "Dominican Republic" },
  { "EC", "Ecuador" },
  { "EG", "Egypt" },
  { "SV", "El Salvador" },
  { "GQ", "Equatorial Guinea" },
  { "ER", "Eritrea" },
  { "EE", "Estonia" },
  { "SZ", "Eswatini" },
  { "ET", "Ethiopia" },
  { "FK", "Falkland Islands (Malvinas)" },
  { "FO", "Faroe Islands" },
  { "FJ", "Fiji" },
  { "FI", "Finland" },
  { "FR", "France" },
  { "GF", "French Guiana" },
  { "PF", "French Polynesia" },
  { "TF", "French Southern Territories" },
  { "GA", "Gabon" },
  { "GM", "Gambia" },
  { "GE", "Georgia" },
  { "DE", "Germany" },
  { "GH", "Ghana" },
  { "GI", "Gibraltar" },
  { "GR", "Greece" },
  { "GL", "Greenland" },
  { "GD", "Grenada" },
  { "GP", "Guadeloupe" },
  { "GU", "Guam" },
  { "GT", "Guatemala" },
  { "GG", "Guernsey" },
  { "GW", "Guinea-Bissau" },
  { "GN", "Guinea" },
  { "GY", "Guyana" },
  { "HT", "Haiti" },
  { "HM", "Heard Island and McDonald Islands" },
  { "VA", "Holy See" },
  { "HN", "Honduras" },
  { "HK", "Hong Kong" },
  { "HU", "Hungary" },
  { "IS", "Iceland" },
  { "IN", "India" },
  { "ID", "Indonesia" },
  { "IR", "Iran (Islamic Republic of)" },
  { "IQ", "Iraq" },
  { "IE", "Ireland" },
  { "IM", "Isle of Man" },
  { "IL", "Israel" },
  { "IT", "Italy" },
  { "JM", "Jamaica" },
  { "JP", "Japan" },
  { "JE", "Jersey" },
  { "JO", "Jordan" },
  { "KZ", "Kazakhstan" },
  { "KE", "Kenya" },
  { "KI", "Kiribati" },
  { "KP", "Korea (Democratic People's Republic of)" },
  { "KR", "Korea, Republic of" },
  { "KW", "Kuwait" },
  { "KG", "Kyrgyzstan" },
  { "LA", "Lao People's Democratic Republic" },
  { "LV", "Latvia" },
  { "LB", "Lebanon" },
  { "LS", "Lesotho" },
  { "LR", "Liberia" },
  { "LY", "Libya" },
  { "LI", "Liechtenstein" },
  { "LT", "Lithuania" },
  { "LU", "Luxembourg" },
  { "MO", "Macao" },
  { "MG", "Madagascar" },
  { "MW", "Malawi" },
  { "MY", "Malaysia" },
  { "MV", "Maldives" },
  { "ML", "Mali" },
  { "MT", "Malta" },
  { "MH", "Marshall Islands" },
  { "MQ", "Martinique" },
  { "MR", "Mauritania" },
  { "MU", "Mauritius" },
  { "YT", "Mayotte" },
  { "MX", "Mexico" },
  { "FM", "Micronesia (Federated States of)" },
  { "MD", "Moldova, Republic of" },
  { "MC", "Monaco" },
  { "MN", "Mongolia" },
  { "ME", "Montenegro" },
  { "MS", "Montserrat" },
  { "MA", "Morocco" },
  { "MZ", "Mozambique" },
  { "MM", "Myanmar" },
  { "NA", "Namibia" },
  { "NR", "Nauru" },
  { "NP", "Nepal" },
  { "NL", "Netherlands" },
  { "NC", "New Caledonia" },
  { "NZ", "New Zealand" },
  { "NI", "Nicaragua" },
  { "NE", "Niger" },
  { "NG", "Nigeria" },
  { "NU", "Niue" },
  { "NF", "Norfolk Island" },
  { "MK", "North Macedonia" },
  { "MP", "Northern Mariana Islands" },
  { "NO", "Norway" },
  { "OM", "Oman" },
  { "PK", "Pakistan" },
  { "PW", "Palau" },
  { "PS", "Palestine, State of" },
  { "PA", "Panama" },
  { "PG", "Papua New Guinea" },
  { "PY", "Paraguay" },
  { "PE", "Peru" },
  { "PH", "Philippines" },
  { "PN", "Pitcairn" },
  { "PL", "Poland" },
  { "PT", "Portugal" },
  { "PR", "Puerto Rico" },
  { "QA", "Qatar" },
  { "RE", "Réunion" },
  { "RO", "Romania" },
  { "RU", "Russian Federation" },
  { "RW", "Rwanda" },
  { "BL", "Saint Barthélemy" },
  { "SH", "Saint Helena, Ascension and Tristan da Cunha" },
  { "KN", "Saint Kitts and Nevis" },
  { "LC", "Saint Lucia" },
  { "MF", "Saint Martin (French part)" },
  { "PM", "Saint Pierre and Miquelon" },
  { "VC", "Saint Vincent and the Grenadines" },
  { "WS", "Samoa" },
  { "SM", "San Marino" },
  { "ST", "Sao Tome and Principe" },
  { "SA", "Saudi Arabia" },
  { "SN", "Senegal" },
  { "RS", "Serbia" },
  { "SC", "Seychelles" },
  { "SL", "Sierra Leone" },
  { "SG", "Singapore" },
  { "SX", "Sint Maarten (Dutch part)" },
  { "SK", "Slovakia" },
  { "SI", "Slovenia" },
  { "SB", "Solomon Islands" },
  { "SO", "Somalia" },
  { "ZA", "South Africa" },
  { "GS", "South Georgia and the South Sandwich Islands" },
  { "SS", "South Sudan" },
  { "ES", "Spain" },
  { "LK", "Sri Lanka" },
  { "SD", "Sudan" },
  { "SR", "Suriname" },
  { "SJ", "Svalbard and Jan Mayen" },
  { "SE", "Sweden" },
  { "CH", "Switzerland" },
  { "SY", "Syrian Arab Republic" },
  { "TW", "Taiwan, Province of China" },
  { "TJ", "Tajikistan" },
  { "TZ", "Tanzania, United Republic of" },
  { "TH", "Thailand" },
  { "TL", "Timor-Leste" },
  { "TG", "Togo" },
  { "TK", "Tokelau" },
  { "TO", "Tonga" },
  { "TT", "Trinidad and Tobago" },
  { "TN", "Tunisia" },
  { "TR", "Turkey" },
  { "TM", "Turkmenistan" },
  { "TC", "Turks and Caicos Islands" },
  { "TV", "Tuvalu" },
  { "UG", "Uganda" },
  { "UA", "Ukraine" },
  { "AE", "United Arab Emirates" },
  { "GB", "United Kingdom of Great Britain and Northern Ireland" },
  { "UK", "United Kingdom" },
  { "UM", "United States Minor Outlying Islands" },
  { "US", "United States of America" },
  { "UY", "Uruguay" },
  { "UZ", "Uzbekistan" },
  { "VU", "Vanuatu" },
  { "VE", "Venezuela (Bolivarian Republic of)" },
  { "VN", "Viet Nam" },
  { "VG", "Virgin Islands (British)" },
  { "VI", "Virgin Islands (U.S.)" },
  { "WF", "Wallis and Futuna" },
  { "EH", "Western Sahara" },
  { "YE", "Yemen" },
  { "ZM", "Zambia" },
  { "ZW", "Zimbabwe" }
};
#endif // HAVE_GNUTLS

//
// '_papplSystemAddPrinter()' - Add a printer
//

void
_papplSystemWebAddPrinter(
    pappl_client_t *client,
    pappl_system_t *system)
{
  int		i;			// Looping var
  const char	*status = NULL;		// Status message, if any
  char		printer_name[128] = "",	// Printer Name
		driver_name[128] = "",	// Driver Name
		device_uri[128] = "",	// Device URI
		*device_id = NULL,	// Device ID
		hostname[256] = "",	// Hostname
		*ptr;			// Pointer into string
  int		port = 0;		// Default port for Socket printing
  static const char *hostname_pattern =	// IP address or hostname pattern
		// Hostname per RFC 1123
		"(^\\s*((?=.{1,255}$)[0-9A-Za-z](?:(?:[0-9A-Za-z]|\\b-){0,61}[0-9A-Za-z])?(?:\\.[0-9A-Za-z](?:(?:[0-9A-Za-z]|\\b-){0,61}[0-9A-Za-z])?)*\\.?)\\s*$)"
		"|"
		// IPv4 address
		"(^\\s*((25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\\.(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\\.(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\\.(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?))\\s*$)"
		"|"
		// IPv6 address
		"(^\\s*((([0-9A-Fa-f]{1,4}:){7}([0-9A-Fa-f]{1,4}|:))|(([0-9A-Fa-f]{1,4}:){6}(:[0-9A-Fa-f]{1,4}|((25[0-5]|2[0-4]\\d|1\\d\\d|[1-9]?\\d)(\\.(25[0-5]|2[0-4]\\d|1\\d\\d|[1-9]?\\d)){3})|:))|(([0-9A-Fa-f]{1,4}:){5}(((:[0-9A-Fa-f]{1,4}){1,2})|:((25[0-5]|2[0-4]\\d|1\\d\\d|[1-9]?\\d)(\\.(25[0-5]|2[0-4]\\d|1\\d\\d|[1-9]?\\d)){3})|:))|(([0-9A-Fa-f]{1,4}:){4}(((:[0-9A-Fa-f]{1,4}){1,3})|((:[0-9A-Fa-f]{1,4})?:((25[0-5]|2[0-4]\\d|1\\d\\d|[1-9]?\\d)(\\.(25[0-5]|2[0-4]\\d|1\\d\\d|[1-9]?\\d)){3}))|:))|(([0-9A-Fa-f]{1,4}:){3}(((:[0-9A-Fa-f]{1,4}){1,4})|((:[0-9A-Fa-f]{1,4}){0,2}:((25[0-5]|2[0-4]\\d|1\\d\\d|[1-9]?\\d)(\\.(25[0-5]|2[0-4]\\d|1\\d\\d|[1-9]?\\d)){3}))|:))|(([0-9A-Fa-f]{1,4}:){2}(((:[0-9A-Fa-f]{1,4}){1,5})|((:[0-9A-Fa-f]{1,4}){0,3}:((25[0-5]|2[0-4]\\d|1\\d\\d|[1-9]?\\d)(\\.(25[0-5]|2[0-4]\\d|1\\d\\d|[1-9]?\\d)){3}))|:))|(([0-9A-Fa-f]{1,4}:){1}(((:[0-9A-Fa-f]{1,4}){1,6})|((:[0-9A-Fa-f]{1,4}){0,4}:((25[0-5]|2[0-4]\\d|1\\d\\d|[1-9]?\\d)(\\.(25[0-5]|2[0-4]\\d|1\\d\\d|[1-9]?\\d)){3}))|:))|(:(((:[0-9A-Fa-f]{1,4}){1,7})|((:[0-9A-Fa-f]{1,4}){0,5}:((25[0-5]|2[0-4]\\d|1\\d\\d|[1-9]?\\d)(\\.(25[0-5]|2[0-4]\\d|1\\d\\d|[1-9]?\\d)){3}))|:)))(%.+)?\\s*$)";


  if (!papplClientHTMLAuthorize(client))
    return;

  if (client->operation == HTTP_STATE_POST)
  {
    const char		*value;		// Form value
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
      http_addrlist_t	*list;		// Address list

      if ((value = cupsGetOption("printer_name", num_form, form)) != NULL)
        strlcpy(printer_name, value, sizeof(printer_name));
      if ((value = cupsGetOption("driver_name", num_form, form)) != NULL)
        strlcpy(driver_name, value, sizeof(driver_name));
      if ((value = cupsGetOption("device_uri", num_form, form)) != NULL)
      {
        strlcpy(device_uri, value, sizeof(device_uri));
        if ((device_id = strchr(device_uri, '|')) != NULL)
          *device_id++ = '\0';
      }

      if (!strcmp(device_uri, "socket"))
      {
        // Make URI using hostname
        if ((value = cupsGetOption("hostname", num_form, form)) == NULL)
        {
          status        = "Please enter a hostname or IP address for the printer.";
          device_uri[0] = '\0';
	}
	else
	{
	  // Break out the port number, if present...
	  strlcpy(hostname, value, sizeof(hostname));
	  if ((ptr = strrchr(hostname, ':')) != NULL && !strchr(ptr, ']'))
	  {
	    *ptr++ = '\0';
	    port   = atoi(ptr);
	  }

          // Then see if we can lookup the hostname or IP address (port number
          // isn't used here...)
          if ((list = httpAddrGetList(hostname, AF_UNSPEC, "9100")) == NULL)
          {
            status = "Unable to lookup address.";
	  }
	  else
	  {
	    httpAddrFreeList(list);
	    httpAssembleURI(HTTP_URI_CODING_ALL, device_uri, sizeof(device_uri), "socket", NULL, hostname, port, "/");
	  }
	}
      }
      else if (!printer_name[0])
      {
        status = "Please enter a printer name.";
      }
      else if (!device_uri[0])
      {
        status = "Please select a device.";
      }
      else if (!driver_name[0])
      {
        status = "Please select a driver.";
      }
      else
      {
        pappl_printer_t *printer = papplPrinterCreate(system, 0, printer_name, driver_name, device_id, device_uri);
					// New printer

        if (printer)
        {
          papplClientRespondRedirect(client, HTTP_STATUS_FOUND, printer->uriname);
          cupsFreeOptions(num_form, form);
          return;
	}
      }
    }

    cupsFreeOptions(num_form, form);
  }

  system_header(client, "Add Printer");

  if (status)
    papplClientHTMLPrintf(client, "<div class=\"banner\">%s</div>\n", status);

  papplClientHTMLStartForm(client, client->uri, false);

  papplClientHTMLPuts(client,
		      "          <table class=\"form\">\n"
		      "            <tbody>\n"
		      "              <tr><th><label for=\"printer_name\">Name:</label></th><td><input type=\"text\" name=\"printer_name\" placeholder=\"Name of printer\" required></td></tr>\n"
		      "              <tr><th><label for=\"device_uri\">Device:</label></th><td><select name=\"device_uri\" id=\"device_uri\"><option value=\"\">Select Device</option>");

  papplDeviceList(PAPPL_DEVTYPE_ALL, system_device_cb, client, papplLogDevice, system);

  papplClientHTMLPrintf(client,
			"<option value=\"socket\">Network Printer</option></tr>\n"
			"              <tr><th><label for=\"hostname\">Hostname/IP Address:</label></th><td><input type=\"text\" name=\"hostname\" id=\"hostname\" placeholder=\"IP address or hostname\" pattern=\"%s\" value=\"%s\" disabled=\"disabled\"></td></tr>\n"
			"              <tr><th><label for=\"driver_name\">Driver Name:</label></th><td><select name=\"driver_name\"><option value=\"\">Select Driver</option>\n", hostname_pattern, hostname);

  for (i = 0; i < system->num_drivers; i ++)
    papplClientHTMLPrintf(client, "<option value=\"%s\">%s</option>", system->drivers[i].name, system->drivers[i].description);

  papplClientHTMLPuts(client,
		      "</select></td></tr>\n"
		      "             <tr><th></th><td><input type=\"submit\" value=\"Add Printer\"></td></tr>\n"
		      "            </tbody></table>\n"
		      "           </form>\n"
		      "          <script>document.forms['form']['device_uri'].onchange = function () {\n"
		      "  if (this.value == 'socket') {\n"
		      "    document.forms['form']['hostname'].disabled = false;\n"
		      "    document.forms['form']['hostname'].required = true;\n"
		      "  } else {\n"
		      "    document.forms['form']['hostname'].disabled = true;\n"
		      "    document.forms['form']['hostname'].required = false;\n"
		      "  }\n"
		      "}</script>\n"
		      "         </div>\n"
		      "       </div>\n");

  system_footer(client);
}


//
// '_papplSystemWebConfig()' - Show the system configuration page.
//

void
_papplSystemWebConfig(
    pappl_client_t *client,		// I - Client
    pappl_system_t *system)		// I - System
{
  char		dns_sd_name[64],	// DNS-SD name
		location[128],		// Location
		geo_location[128],	// Geo-location latitude
		organization[128],	// Organization
		org_unit[128];		// Organizational unit
  pappl_contact_t contact;		// Contact info
  const char	*status = NULL;		// Status message, if any


  if (!papplClientHTMLAuthorize(client))
    return;

  if (client->operation == HTTP_STATE_POST)
  {
    int			num_form = 0;	// Number of form variable
    cups_option_t	*form = NULL;	// Form variables

    if ((num_form = papplClientGetForm(client, &form)) == 0)
      status = "Invalid form data.";
    else if (!papplClientIsValidForm(client, num_form, form))
      status = "Invalid form submission.";
    else
    {
      _papplSystemWebConfigFinalize(system, num_form, form);

      status = "Changes saved.";
    }

    cupsFreeOptions(num_form, form);
  }

  system_header(client, "Configuration");
  if (status)
    papplClientHTMLPrintf(client, "<div class=\"banner\">%s</div>\n", status);

  _papplClientHTMLInfo(client, true, papplSystemGetDNSSDName(system, dns_sd_name, sizeof(dns_sd_name)), papplSystemGetLocation(system, location, sizeof(location)), papplSystemGetGeoLocation(system, geo_location, sizeof(geo_location)), papplSystemGetOrganization(system, organization, sizeof(organization)), papplSystemGetOrganizationalUnit(system, org_unit, sizeof(org_unit)), papplSystemGetContact(system, &contact));

  papplClientHTMLPuts(client,
                      "        </div>\n"
                      "      </div>\n");

  system_footer(client);
}


//
// '_papplSystemWebConfigFinalize()' - Save the changes to the system configuration.
//

void
_papplSystemWebConfigFinalize(
    pappl_system_t *system,		// I - System
    int            num_form,		// I - Number of form variables
    cups_option_t  *form)		// I - Form variables
{
  const char	*value,			// Form value
		*geo_lat,		// Geo-location latitude
		*geo_lon,		// Geo-location longitude
		*contact_name,		// Contact name
		*contact_email,		// Contact email
		*contact_tel;		// Contact telephone number


  if ((value = cupsGetOption("dns_sd_name", num_form, form)) != NULL)
    papplSystemSetDNSSDName(system, *value ? value : NULL);

  if ((value = cupsGetOption("location", num_form, form)) != NULL)
    papplSystemSetLocation(system, *value ? value : NULL);

  geo_lat = cupsGetOption("geo_location_lat", num_form, form);
  geo_lon = cupsGetOption("geo_location_lon", num_form, form);
  if (geo_lat && geo_lon)
  {
    char	uri[1024];		// "geo:" URI

    if (*geo_lat && *geo_lon)
    {
      snprintf(uri, sizeof(uri), "geo:%g,%g", atof(geo_lat), atof(geo_lon));
      papplSystemSetGeoLocation(system, uri);
    }
    else
      papplSystemSetGeoLocation(system, NULL);
  }

  if ((value = cupsGetOption("organization", num_form, form)) != NULL)
    papplSystemSetOrganization(system, *value ? value : NULL);

  if ((value = cupsGetOption("organizational_unit", num_form, form)) != NULL)
    papplSystemSetOrganizationalUnit(system, *value ? value : NULL);

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

    papplSystemSetContact(system, &contact);
  }
}


//
// '_papplSystemWebHome()' - Show the system home page.
//

void
_papplSystemWebHome(
    pappl_client_t *client,		// I - Client
    pappl_system_t *system)		// I - System
{
  system_header(client, NULL);

  papplClientHTMLPrintf(client,
			"      <div class=\"row\">\n"
			"        <div class=\"col-6\">\n"
			"          <h1 class=\"title\">Configuration <a class=\"btn\" href=\"https://%s:%d/config\">Change</a></h1>\n", client->host_field, client->host_port);

  _papplClientHTMLInfo(client, false, system->dns_sd_name, system->location, system->geo_location, system->organization, system->org_unit, &system->contact);

  _papplSystemWebSettings(client);

  papplClientHTMLPrintf(client,
		      "        </div>\n"
                      "        <div class=\"col-6\">\n"
                      "          <h1 class=\"title\">Printers</h1>\n"
                      "          <a class=\"btn\" href=\"https://%s:%d/addprinter\">Add Printer</a>", client->host_field, client->host_port);

  papplSystemIteratePrinters(system, (pappl_printer_cb_t)_papplPrinterIteratorWebCallback, client);

  papplClientHTMLPuts(client,
                      "        </div>\n"
                      "      </div>\n");

  system_footer(client);
}


//
// '_papplSystemWebLogFile()' - Return the log file as requested
//

void
_papplSystemWebLogFile(
    pappl_client_t *client,		// I - Client
    pappl_system_t *system)		// I - System
{
  if (!papplClientHTMLAuthorize(client))
    return;

  if (client->operation == HTTP_STATE_GET)
  {
    http_status_t	code;		// HTTP status of response
    struct stat		loginfo;	// Log information
    ssize_t		bytes;		// Bytes read/written
    size_t		length = 0;	// Log length
    const char		*value;		// Range Field value
    char		*rangeptr;	// Pointer into range...
    char		buffer[8192];	// Copy buffer
    int			fd;		// Resource file descriptor
    long		low = 0,	// Log lower range
			high = -1;	// Log upper range

    value = httpGetField(client->http, HTTP_FIELD_RANGE);

    // If range exists, send log content from low to high
    if (value && *value && (rangeptr = strstr(value, "bytes=")) != NULL)
    {
      if ((low = strtol(rangeptr + 6, &rangeptr, 10)) < 0)
        low = 0;
      else if (rangeptr && *rangeptr == '-' && isdigit(rangeptr[1] & 255))
        high = strtol(rangeptr + 1, NULL, 10);
    }

    if ((fd = open(system->logfile, O_RDONLY)) < 0)
    {
      papplLogClient(client, PAPPL_LOGLEVEL_ERROR, "Unable to open log file '%s': %s", system->logfile, strerror(errno));
      papplClientRespond(client, HTTP_STATUS_SERVER_ERROR, NULL, NULL, 0, 0);
      return;
    }

    // Get log file info
    if (fstat(fd, &loginfo))
    {
      papplLogClient(client, PAPPL_LOGLEVEL_ERROR, "Unable to access log file '%s': %s", system->logfile, strerror(errno));
      papplClientRespond(client, HTTP_STATUS_SERVER_ERROR, NULL, NULL, 0, 0);
      return;
    }

    // Write the log in the range
    if (low > loginfo.st_size)
    {
      length = (size_t)loginfo.st_size;
      code   = HTTP_STATUS_OK;
      low    = 0;
    }
    else if (high < 0)
    {
      length = (size_t)loginfo.st_size - (size_t)low;
      code   = HTTP_STATUS_PARTIAL_CONTENT;
    }
    else
    {
      length = (size_t)(high - low);
      code   = HTTP_STATUS_PARTIAL_CONTENT;
    }

    httpSetLength(client->http, length);
    httpSetField(client->http, HTTP_FIELD_SERVER, papplSystemGetServerHeader(system));
    httpSetField(client->http, HTTP_FIELD_CONTENT_TYPE, "text/plain");

    // Seek to position low in log
    if (lseek(fd, (off_t)low, SEEK_CUR) < 0)
    {
      papplLogClient(client, PAPPL_LOGLEVEL_ERROR, "Unable to seek to offset %ld in log file '%s': %s", low, system->logfile, strerror(errno));
      papplClientRespond(client, HTTP_STATUS_SERVER_ERROR, NULL, NULL, 0, 0);
      return;
    }

    if (httpWriteResponse(client->http, code) < 0)
      return;

    papplLogClient(client, PAPPL_LOGLEVEL_INFO, "%s %s %d", code == HTTP_STATUS_OK ? "OK" : "Partial Content", "text/plain", (int)length);

    // Read buffer and write to client
    while (length > 0 && (bytes = read(fd, buffer, sizeof(buffer))) > 0)
    {
      if ((size_t)bytes > length)
        bytes = (ssize_t)length;

      length -= (size_t)bytes;
      httpWrite2(client->http, buffer, (size_t)bytes);
    }

    httpWrite2(client->http, "", 0);

    close(fd);
  }
  else
    papplClientRespond(client, HTTP_STATUS_BAD_REQUEST, NULL, NULL, 0, 0);
}


//
// '_papplSystemWebLogs()' - Show the system logs
//

void
_papplSystemWebLogs(
    pappl_client_t *client,		// I - Client
    pappl_system_t *system)		// I - System
{
  pappl_loglevel_t	i,		// Looping var
			loglevel;	// Current log level
  const char		*status = NULL;	// Status message, if any
  static const char * const levels[] =	// Log level strings
  {
    "Debugging",
    "Informational",
    "Warning",
    "Errors",
    "Fatal Errors/Conditions",
  };


  if (!papplClientHTMLAuthorize(client))
    return;

  if (client->operation == HTTP_STATE_POST)
  {
    const char		*value;		// Form value
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
      if ((value = cupsGetOption("log_level", num_form, form)) != NULL)
      {
        // Get log level and save it...
	for (loglevel = PAPPL_LOGLEVEL_DEBUG; loglevel <= PAPPL_LOGLEVEL_FATAL; loglevel ++)
        {
          if (!strcmp(value, levels[loglevel]))
            break;
        }

        if (loglevel <= PAPPL_LOGLEVEL_FATAL)
        {
          papplSystemSetLogLevel(system, loglevel);
          status = "Changes Saved.";
	}
	else
	  status = "Please select a valid log level.";
      }
      else
      {
        status = "Please select a valid log level.";
      }
    }

    cupsFreeOptions(num_form, form);
  }

  system_header(client, "Logs");

  if (status)
    papplClientHTMLPrintf(client, "<div class=\"banner\">%s</div>\n", status);

  papplClientHTMLStartForm(client, client->uri, false);

  papplClientHTMLPuts(client,
		      "          <table>\n"
		      "            <tbody>\n"
		      "              <tr><th><label for=\"log_level\">Log Level:</label></th><td><select name=\"log_level\" id=\"log_level\"><option value=\"\">Select Log Level</option>\n");

  for (i = PAPPL_LOGLEVEL_DEBUG, loglevel = papplSystemGetLogLevel(system); i <= PAPPL_LOGLEVEL_FATAL; i ++)
  {
    papplClientHTMLPrintf(client, "               <option value=\"%s\"%s>%s</option>\n", levels[i - PAPPL_LOGLEVEL_DEBUG], i == loglevel ? " selected" : "", levels[i - PAPPL_LOGLEVEL_DEBUG]);
  }

  papplClientHTMLPuts(client,
		      "             </select> <input type=\"submit\" value=\"Change Log Level\"></td></tr>\n"
		      "            </tbody>\n"
		      "          </table>\n"
		      "        </form>\n"
		      "        <div class=\"log\" id=\"logdiv\"><pre id=\"log\"></pre></div>\n"
		      "        <script>\n"
		      "var content_length = 0;\n"
		      "function update_log() {\n"
		      "  let xhr = new XMLHttpRequest();\n"
		      "  xhr.open('GET', '/logfile.txt');\n"
		      "  xhr.setRequestHeader('Range', 'bytes=' + content_length + '-');\n"
		      "  xhr.send();\n"
		      "  xhr.onreadystatechange = function() {\n"
		      "    var log = document.getElementById('log');\n"
		      "    var logdiv = document.getElementById('logdiv');\n"
		      "    if (xhr.readyState != 4) return;\n"
		      "    if (xhr.status == 200) {\n"
		      "      log.innerText = xhr.response;\n"
		      "      content_length = xhr.getResponseHeader('Content-Length');\n"
		      "    }\n"
		      "    else if (xhr.status == 206) {\n"
		      "       log.innerText += xhr.response;\n"
		      "       content_length += xhr.getResponseHeader('Content-Length');\n"
		      "    }\n"
		      "    window.setTimeout('update_log()', 5000);\n"
		      "    logdiv.scrollTop = logdiv.scrollHeight - logdiv.clientHeight;\n"
		      "  }\n"
		      "}\n"
		      "update_log();</script>\n");

  system_footer(client);
}


//
// '_papplSystemWebNetwork()' - Show the system network configuration page.
//

void
_papplSystemWebNetwork(
    pappl_client_t *client,		// I - Client
    pappl_system_t *system)		// I - System
{
  const char	*status = NULL;		// Status message, if any
  struct ifaddrs *addrs,		// List of network addresses
		*addr;			// Current network address


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
      const char *value;		// Form variable value

      if ((value = cupsGetOption("hostname", num_form, form)) != NULL)
      {
        // Set hostname and save it...
	papplSystemSetHostname(client->system, value);
        status = "Changes saved.";
      }
    }

    cupsFreeOptions(num_form, form);
  }

  system_header(client, "Networking");

  if (status)
    papplClientHTMLPrintf(client, "<div class=\"banner\">%s</div>\n", status);

  papplClientHTMLStartForm(client, client->uri, false);
  papplClientHTMLPrintf(client,
			"          <table class=\"form\">\n"
			"            <tbody>\n"
			"              <tr><th><label for=\"hostname\">Hostname:</label></th><td><input type=\"text\" name=\"hostname\" value=\"%s\" placeholder=\"name.domain\" pattern=\"^(|[-_a-zA-Z0-9][.-_a-zA-Z0-9]*)$\"> <input type=\"submit\" value=\"Save Changes\"></td></tr>\n", system->hostname);

  if (!getifaddrs(&addrs))
  {
    char	temp[256],		// Address string
		*tempptr;		// Pointer into address

    papplClientHTMLPuts(client, "              <tr><th>IPv4 Addresses:</th><td>");

    for (addr = addrs; addr; addr = addr->ifa_next)
    {
      if (addr->ifa_name == NULL || addr->ifa_addr == NULL || addr->ifa_addr->sa_family != AF_INET || !(addr->ifa_flags & IFF_UP) || (addr->ifa_flags & (IFF_LOOPBACK | IFF_POINTOPOINT)) || !strncmp(addr->ifa_name, "awdl", 4))
        continue;

      httpAddrString((http_addr_t *)addr->ifa_addr, temp, sizeof(temp));
      tempptr = temp;

      if (!strcmp(addr->ifa_name, "wlan0") || !strcmp(addr->ifa_name, "wlp2s0"))
        papplClientHTMLPrintf(client, "Wi-Fi: %s<br>", tempptr);
      else if (!strncmp(addr->ifa_name, "wlan", 4) && isdigit(addr->ifa_name[4]))
        papplClientHTMLPrintf(client, "Wi-Fi %d: %s<br>", atoi(addr->ifa_name + 4) + 1, tempptr);
      else if (!strcmp(addr->ifa_name, "en0") || !strcmp(addr->ifa_name, "eth0") || !strncmp(addr->ifa_name, "enx", 3))
        papplClientHTMLPrintf(client, "Ethernet: %s<br>", tempptr);
      else if (!strncmp(addr->ifa_name, "en", 2) && isdigit(addr->ifa_name[2]))
        papplClientHTMLPrintf(client, "Ethernet %d: %s<br>", atoi(addr->ifa_name + 2) + 1, tempptr);
      else if (!strncmp(addr->ifa_name, "eth", 3) && isdigit(addr->ifa_name[3]))
        papplClientHTMLPrintf(client, "Ethernet %d: %s<br>", atoi(addr->ifa_name + 3) + 1, tempptr);
    }

    papplClientHTMLPuts(client,
                        "</td></tr>\n"
                        "              <tr><th>IPv6 Addresses:</th><td>");

    for (addr = addrs; addr; addr = addr->ifa_next)
    {
      if (addr->ifa_name == NULL || addr->ifa_addr == NULL || addr->ifa_addr->sa_family != AF_INET6 || !(addr->ifa_flags & IFF_UP) || (addr->ifa_flags & (IFF_LOOPBACK | IFF_POINTOPOINT)) || !strncmp(addr->ifa_name, "awdl", 4))
        continue;

      httpAddrString((http_addr_t *)addr->ifa_addr, temp, sizeof(temp));

      if ((tempptr = strchr(temp, '+')) != NULL)
        *tempptr = '\0';
      else if ((tempptr = strchr(temp, ']')) != NULL)
        *tempptr = '\0';

      if (!strncmp(temp, "[v1.", 4))
        tempptr = temp + 4;
      else if (*temp == '[')
        tempptr = temp + 1;
      else
        tempptr = temp;

      if (!strcmp(addr->ifa_name, "wlan0") || !strcmp(addr->ifa_name, "wlp2s0"))
        papplClientHTMLPrintf(client, "Wi-Fi: %s<br>", tempptr);
      else if (!strncmp(addr->ifa_name, "wlan", 4) && isdigit(addr->ifa_name[4]))
        papplClientHTMLPrintf(client, "Wi-Fi %d: %s<br>", atoi(addr->ifa_name + 4) + 1, tempptr);
      else if (!strcmp(addr->ifa_name, "en0") || !strcmp(addr->ifa_name, "eth0") || !strncmp(addr->ifa_name, "enx", 3))
        papplClientHTMLPrintf(client, "Ethernet: %s<br>", tempptr);
      else if (!strncmp(addr->ifa_name, "en", 2) && isdigit(addr->ifa_name[2]))
        papplClientHTMLPrintf(client, "Ethernet %d: %s<br>", atoi(addr->ifa_name + 2) + 1, tempptr);
      else if (!strncmp(addr->ifa_name, "eth", 3) && isdigit(addr->ifa_name[3]))
        papplClientHTMLPrintf(client, "Ethernet %d: %s<br>", atoi(addr->ifa_name + 3) + 1, tempptr);
    }

    papplClientHTMLPuts(client, "</td></tr>\n");

    freeifaddrs(addrs);
  }

  papplClientHTMLPuts(client,
		      "            </tbody>\n"
		      "          </table>\n"
		      "      </form>\n");

  system_footer(client);
}


//
// '_papplSystemWebSecurity()' - Show the system security (users/password) page.
//

void
_papplSystemWebSecurity(
    pappl_client_t *client,		// I - Client
    pappl_system_t *system)		// I - System
{
  const char	*status = NULL;		// Status message, if any
  struct group	*grp;			// Current group


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
    else if (!client->system->auth_service)
    {
      const char	*old_password,	// Old password (if any)
			*new_password,	// New password
			*new_password2;	// New password again
      char		hash[1024];	// Hash of password

      old_password  = cupsGetOption("old_password", num_form, form);
      new_password  = cupsGetOption("new_password", num_form, form);
      new_password2 = cupsGetOption("new_password2", num_form, form);

      if (system->password_hash[0] && (!old_password || strcmp(system->password_hash, papplSystemHashPassword(system, system->password_hash, old_password, hash, sizeof(hash)))))
      {
        status = "Wrong old password.";
      }
      else if (!new_password || !new_password2 || strcmp(new_password, new_password2))
      {
        status = "Passwords do not match.";
      }
      else
      {
        const char	*passptr;	// Pointer into password
        bool		have_lower,	// Do we have a lowercase letter?
			have_upper,	// Do we have an uppercase letter?
			have_digit;	// Do we have a number?

        for (passptr = new_password, have_lower = false, have_upper = false, have_digit = false; *passptr; passptr ++)
        {
          if (isdigit(*passptr & 255))
            have_digit = true;
	  else if (islower(*passptr & 255))
	    have_lower = true;
	  else if (isupper(*passptr & 255))
	    have_upper = true;
	}

        if (!have_digit || !have_lower || !have_upper || strlen(new_password) < 8)
        {
          status = "Password must be at least eight characters long and contain at least one uppercase letter, one lowercase letter, and one digit.";
        }
        else
        {
          papplSystemHashPassword(system, NULL, new_password, hash, sizeof(hash));
          papplSystemSetPassword(system, hash);
          status = "Password changed.";
	}
      }
    }
    else
    {
      const char	 *group;	// Current group
      char		buffer[8192];	// Buffer for strings
      struct group	grpbuf;		// Group buffer

      grp = NULL;

      if ((group = cupsGetOption("admin_group", num_form, form)) != NULL)
      {
        if (getgrnam_r(group, &grpbuf, buffer, sizeof(buffer), &grp) || !grp)
          status = "Bad administration group.";
	else
	  papplSystemSetAdminGroup(system, group);
      }

      if ((group = cupsGetOption("print_group", num_form, form)) != NULL)
      {
        if (getgrnam_r(group, &grpbuf, buffer, sizeof(buffer), &grp) || !grp)
        {
          status = "Bad print group.";
	}
	else
	{
	  papplSystemSetDefaultPrintGroup(system, group);
	  papplSystemIteratePrinters(system, (pappl_printer_cb_t)papplPrinterSetPrintGroup, (void *)group);
	}
      }

      if (!status)
        status = "Group changes saved.";
    }

    cupsFreeOptions(num_form, form);
  }

  system_header(client, "Security");

  if (status)
    papplClientHTMLPrintf(client, "<div class=\"banner\">%s</div>\n", status);

  papplClientHTMLPuts(client,
                      "        </div>\n"
                      "      </div>\n"
                      "      <div class=\"row\">\n");

  if (system->auth_service)
  {
    // Show Users pane for group controls
    papplClientHTMLPuts(client,
			"        <div class=\"col-12\">\n"
			"          <h2 class=\"title\">Users</h2>\n");

    papplClientHTMLStartForm(client, client->uri, false);

    papplClientHTMLPuts(client,
			"          <table class=\"form\">\n"
			"            <tbody>\n"
			"              <tr><th><label for=\"admin_group\">Admin Group:</label></th><td><select name\"admin_group\"><option value=\"\">None</option>");

    setgrent();
    while ((grp = getgrent()) != NULL)
    {
      papplClientHTMLPrintf(client, "<option%s>%s</option>", (system->admin_group && !strcmp(grp->gr_name, system->admin_group)) ? " selected" : "", grp->gr_name);
    }

    papplClientHTMLPuts(client,
			"</select></td></tr>\n"
			"              <tr><th><label for=\"print_group\">Print Group:</label></th><td><select name\"print_group\"><option value=\"\">None</option>");

    setgrent();
    while ((grp = getgrent()) != NULL)
    {
      papplClientHTMLPrintf(client, "<option%s>%s</option>", (system->default_print_group && !strcmp(grp->gr_name, system->default_print_group)) ? " selected" : "", grp->gr_name);
    }

    papplClientHTMLPuts(client,
			"</select></td></tr>\n"
			"              <tr><th></th><td><input type=\"submit\" value=\"Save Changes\"></td></tr>\n"
			"            </tbody>\n"
			"          </table>\n"
			"        </div>\n"
			"        </form>\n");
  }
  else if (system->password_hash[0])
  {
    // Show simple access password update form...
    papplClientHTMLPuts(client,
			"        <div class=\"col-12\">\n"
			"          <h2 class=\"title\">Change Access Password</h2>\n");

    papplClientHTMLStartForm(client, client->uri, false);

    papplClientHTMLPuts(client,
			"          <table class=\"form\">\n"
			"            <tbody>\n"
			"              <tr><th><label for=\"old_password\">Current Password:</label></th><td><input type=\"password\" name=\"old_password\"></td></tr>\n"
			"              <tr><th><label for=\"new_password\">New Password:</label></th><td><input type=\"password\" name=\"new_password\" placeholder=\"8+, upper+lower+digit\"></td></tr>\n"
			"              <tr><th><label for=\"new_password2\">New Password (again):</label></th><td><input type=\"password\" name=\"new_password2\" placeholder=\"8+, upper+lower+digit\"></td></tr>\n"
			"              <tr><th></th><td><input type=\"submit\" value=\"Change Access Password\"></td></tr>\n"
			"            </tbody>\n"
			"          </table>\n"
			"        </div>\n"
			"        </form>\n");

  }
  else
  {
    // Show simple access password initial setting form...
    papplClientHTMLPuts(client,
			"        <div class=\"col-12\">\n"
			"          <h2 class=\"title\">Set Access Password</h2>\n");

    papplClientHTMLStartForm(client, client->uri, false);

    papplClientHTMLPuts(client,
			"          <table class=\"form\">\n"
			"            <tbody>\n"
			"              <tr><th><label for=\"new_password\">Password:</label></th><td><input type=\"password\" name=\"new_password\" placeholder=\"8+, upper+lower+digit\"></td></tr>\n"
			"              <tr><th><label for=\"new_password2\">Password (again):</label></th><td><input type=\"password\" name=\"new_password2\" placeholder=\"8+, upper+lower+digit\"></td></tr>\n"
			"              <tr><th></th><td><input type=\"submit\" value=\"Set Access Password\"></td></tr>\n"
			"            </tbody>\n"
			"          </table>\n"
			"        </div>\n"
			"        </form>\n");
  }

  // Finish up...
  papplClientHTMLPuts(client,
                      "      </div>\n");

  system_footer(client);
}


//
// '_papplSystemWebSettings()' - Show the system settings panel, as needed.
//

void
_papplSystemWebSettings(
    pappl_client_t *client)		// I - Client
{
  if (client->system->options & (PAPPL_SOPTIONS_WEB_NETWORK | PAPPL_SOPTIONS_WEB_SECURITY | PAPPL_SOPTIONS_WEB_TLS))
  {
    papplClientHTMLPuts(client,
                        "          <h2 class=\"title\">Other Settings</h2>\n"
                        "          <div class=\"btn\">");
    if (client->system->options & PAPPL_SOPTIONS_WEB_NETWORK)
      papplClientHTMLPrintf(client, "<a class=\"btn\" href=\"https://%s:%d/network\">Network</a> ", client->host_field, client->host_port);
    if (client->system->options & PAPPL_SOPTIONS_WEB_SECURITY)
      papplClientHTMLPrintf(client, "<a class=\"btn\" href=\"https://%s:%d/security\">Security</a> ", client->host_field, client->host_port);
#ifdef HAVE_GNUTLS
    if (client->system->options & PAPPL_SOPTIONS_WEB_TLS)
      papplClientHTMLPrintf(client,
                            "<a class=\"btn\" href=\"https://%s:%d/tls-install-crt\">Install TLS Certificate</a> "
                            "<a class=\"btn\" href=\"https://%s:%d/tls-new-crt\">Create New TLS Certificate</a> "
                            "<a class=\"btn\" href=\"https://%s:%d/tls-new-csr\">Create TLS Certificate Request</a> ", client->host_field, client->host_port, client->host_field, client->host_port, client->host_field, client->host_port);
#endif // HAVE_GNUTLS
    papplClientHTMLPuts(client, "</div>\n");
  }

  if ((client->system->options & PAPPL_SOPTIONS_WEB_LOG) && client->system->logfile && strcmp(client->system->logfile, "-") && strcmp(client->system->logfile, "syslog"))
    papplClientHTMLPrintf(client,
                        "          <h2 class=\"title\">Logging</h2>\n"
                        "          <div class=\"btn\"><a class=\"btn\" href=\"https://%s:%d/logs\">View Logs</a></div>\n", client->host_field, client->host_port);
}


#ifdef HAVE_GNUTLS
//
// '_papplSystemWebTLSInstall()' - Show the system TLS certificate installation page.
//

void
_papplSystemWebTLSInstall(
    pappl_client_t *client,		// I - Client
    pappl_system_t *system)		// I - System
{
  const char	*status = NULL;		// Status message, if any


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
      const char	*crtfile,	// Certificate file
			*keyfile;	// Private key file
      char		filename[1024];	// Filename

      crtfile = cupsGetOption("certificate", num_form, form);
      keyfile = cupsGetOption("privatekey", num_form, form);

      if (!keyfile)
      {
        char	hostname[256],		// Hostname
	      	*hostptr;		// Pointer into hostname

        strlcpy(hostname, client->system->hostname, sizeof(hostname));
        if ((hostptr = strchr(hostname, '.')) != NULL)
          *hostptr = '\0';

        snprintf(filename, sizeof(filename), "%s/%s.key", client->system->directory, hostname);
        if (!access(filename, R_OK))
          keyfile = filename;
	else
	  status = "Missing private key.";
      }

      if (!status)
      {
        if (tls_install_certificate(client, crtfile, keyfile))
          status = "Certificate installed.";
        else
          status = "Invalid certificate or private key.";
      }
    }

    cupsFreeOptions(num_form, form);
  }

  system_header(client, "Install TLS Certificate");

  if (status)
    papplClientHTMLPrintf(client, "<div class=\"banner\">%s</div>\n", status);

  papplClientHTMLPuts(client,
                      "        </div>\n"
                      "      </div>\n"
                      "      <div class=\"row\">\n");

  papplClientHTMLStartForm(client, client->uri, true);
  papplClientHTMLPuts(client,
		      "        <div class=\"col-12\">\n"
		      "          <p>This form will install a trusted TLS certificate you have obtained from a Certificate Authority ('CA'). Once installed, it will be used immediately.</p>\n"
		      "          <table class=\"form\">\n"
		      "            <tbody>\n"
		      "              <tr><th><label for=\"certificate\">Certificate:</label></th><td><input type=\"file\" name=\"certificate\" accept=\".crt,.pem,application/pem-certificate-chain,application/x-x509-ca-cert,application/octet-stream\" required> (PEM-encoded)</td></tr>\n"
		      "              <tr><th><label for=\"privatekey\">Private Key:</label></th><td><input type=\"file\" name=\"privatekey\" accept=\".key,.pem,application/octet-stream\"> (PEM-encoded, leave unselected to use the key from the last signing request)</td></tr>\n"
		      "              <tr><th></th><td><input type=\"submit\" value=\"Install Certificate\"></td></tr>\n"
		      "            </tbody>\n"
		      "          </table>\n"
		      "        </div>\n"
		      "        </form>\n"
                      "      </div>\n");

  system_footer(client);
}


//
// '_papplSystemWebTLSNew()' - Show the system TLS certificate/request creation page.
//

void
_papplSystemWebTLSNew(
    pappl_client_t *client,		// I - Client
    pappl_system_t *system)		// I - System
{
  int		i;			// Looping var
  const char	*status = NULL;		// Status message, if any
  char		crqpath[256] = "";	// Certificate request file, if any
  bool		success = false;	// Were we successful?


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
    else if (!strcmp(client->uri, "/tls-new-crt"))
    {
      if (tls_make_certificate(client, num_form, form))
      {
        status  = "Certificate created.";
        success = true;
      }
      else
        status = "Unable to create certificate.";
    }
    else
    {
      if (tls_make_certsignreq(client, num_form, form, crqpath, sizeof(crqpath)))
      {
        status  = "Certificate request created.";
        success = true;
      }
      else
        status = "Unable to create certificate request.";
    }

    cupsFreeOptions(num_form, form);
  }

  if (!strcmp(client->uri, "/tls-new-crt"))
    system_header(client, "Create New TLS Certificate");
  else
    system_header(client, "Create TLS Certificate Request");

  if (status)
  {
    papplClientHTMLPrintf(client, "          <div class=\"banner\">%s</div>\n", status);

    if (crqpath[0])
      papplClientHTMLPrintf(client, "          <p><a class=\"btn\" href=\"%s\">Download Certificate Request File</a></p>\n", crqpath);

    if (success)
    {
      papplClientHTMLPuts(client,
                          "        </div>\n"
                          "      </div>\n");
      system_footer(client);
      return;
    }
  }

  papplClientHTMLPuts(client,
                      "        </div>\n"
                      "      </div>\n"
                      "      <div class=\"row\">\n");

  papplClientHTMLStartForm(client, client->uri, false);

  if (!strcmp(client->uri, "/tls-new-crt"))
    papplClientHTMLPuts(client,
			"        <div class=\"col-12\">\n"
			"          <p>This form creates a new 'self-signed' TLS certificate for secure printing. Self-signed certificates are not automatically trusted by web browsers.</p>\n"
			"          <table class=\"form\">\n"
			"            <tbody>\n"
			"              <tr><th><label for=\"duration\">Duration:</label></th><td><input type=\"number\" name=\"duration\" min=\"1\" max=\"10\" step=\"1\" value=\"5\" size=\"2\" maxsize=\"2\">&nbsp;years</td></tr>\n");
  else
    papplClientHTMLPuts(client,
			"        <div class=\"col-12\">\n"
			"          <p>This form creates a certificate signing request ('CSR') that you can send to a Certificate Authority ('CA') to obtain a trusted TLS certificate. The private key is saved separately for use with the certificate you get from the CA.</p>\n"
			"          <table class=\"form\">\n"
			"            <tbody>\n");

  papplClientHTMLPrintf(client,
			"              <tr><th><label for=\"level\">Level:</label></th><td><select name=\"level\"><option value=\"rsa-2048\">Good (2048-bit RSA)</option><option value=\"rsa-4096\">Better (4096-bit RSA)</option><option value=\"ecdsa-p384\">Best (384-bit ECC)</option></select></td></tr>\n"
			"              <tr><th><label for=\"email\">EMail (contact):</label></th><td><input type=\"email\" name=\"email\" value=\"%s\" placeholder=\"name@example.com\"></td></tr>\n"
			"              <tr><th><label for=\"organization\">Organization:</label></th><td><input type=\"text\" name=\"organization\" value=\"%s\" placeholder=\"Organization/business name\"></td></tr>\n"
			"              <tr><th><label for=\"organizational_unit\">Organization Unit:</label></th><td><input type=\"text\" name=\"organizational_unit\" value=\"%s\" placeholder=\"Unit, department, etc.\"></td></tr>\n"
			"              <tr><th><label for=\"city\">City/Locality:</label></th><td><input type=\"text\" name=\"city\" placeholder=\"City/town name\">  <button id=\"address_lookup\" onClick=\"event.preventDefault(); navigator.geolocation.getCurrentPosition(setAddress);\">Use My Position</button></td></tr>\n"
			"              <tr><th><label for=\"state\">State/Province:</label></th><td><input type=\"text\" name=\"state\" placeholder=\"State/province name\"></td></tr>\n"
			"              <tr><th><label for=\"country\">Country or Region:</label></th><td><select name=\"country\"><option value="">Choose</option>", system->contact.email, system->organization ? system->organization : "", system->org_unit ? system->org_unit : "");

  for (i = 0; i < (int)(sizeof(countries) / sizeof(countries[0])); i ++)
    papplClientHTMLPrintf(client, "<option value=\"%s\">%s</option>", countries[i][0], countries[i][1]);

  if (!strcmp(client->uri, "/tls-new-crt"))
    papplClientHTMLPuts(client,
			"</select></td></tr>\n"
			"              <tr><th></th><td><input type=\"submit\" value=\"Create New Certificate\"></td></tr>\n");
  else
    papplClientHTMLPuts(client,
			"</select></td></tr>\n"
			"              <tr><th></th><td><input type=\"submit\" value=\"Create Certificate Signing Request\"></td></tr>\n");

  papplClientHTMLPuts(client,
		      "            </tbody>\n"
		      "          </table>\n"
		      "        </div>\n"
		      "        </form>\n"
		      "        <script>\n"
		      "function setAddress(p) {\n"
		      "  let lat = p.coords.latitude.toFixed(4);\n"
		      "  let lon = p.coords.longitude.toFixed(4);\n"
		      "  let xhr = new XMLHttpRequest();\n"
		      "  xhr.open('GET', 'https://nominatim.openstreetmap.org/reverse?format=jsonv2&lat=' + lat + '&lon=' + lon);\n"
		      "  xhr.responseType = 'json';\n"
		      "  xhr.send();\n"
		      "  xhr.onload = function() {\n"
		      "    if (xhr.status == 200) {\n"
		      "      let response = xhr.response;\n"
		      "      document.forms['form']['city'].value = response['address']['city'];\n"
		      "      document.forms['form']['state'].value = response['address']['state'];\n"
		      "      let country = document.forms['form']['country'];\n"
		      "      let cc = response['address']['country_code'].toUpperCase();\n"
		      "      for (i = 0; i < country.length; i ++) {\n"
		      "	if (country[i].value == cc) {\n"
		      "	  country.selectedIndex = i;\n"
		      "	  break;\n"
		      "	}\n"
		      "      }\n"
		      "    } else {\n"
		      "      let button = document.getElementById('address_lookup');\n"
		      "      button.innerHTML = 'Lookup Failed.';\n"
		      "    }\n"
		      "  }\n"
		      "}\n"
		      "        </script>\n"
                      "      </div>\n");

  system_footer(client);
}
#endif // HAVE_GNUTLS


//
// 'system_device_cb()' - Device callback for the "add printer" chooser.
//

static bool				// O - `true` to stop, `false` to continue
system_device_cb(
    const char *device_info,		// I - Device description
    const char *device_uri,		// I - Device URI
    const char *device_id,		// I - IEEE-1284 device ID
    void       *data)			// I - Callback data (client)
{
  pappl_client_t *client = (pappl_client_t *)data;
					// Client


  papplClientHTMLPrintf(client, "<option value=\"%s|%s\">%s</option>", device_uri, device_id, device_info);

  return (false);
}


//
// 'system_footer()' - Show the system footer.
//

static void
system_footer(pappl_client_t *client)	// I - Client
{
  papplClientHTMLPuts(client, "    </div>\n");

  papplClientHTMLFooter(client);
}


//
// 'system_header()' - Show the system header.
//

static void
system_header(pappl_client_t *client,	// I - Client
              const char     *title)	// I - Title
{
  if (!papplClientRespond(client, HTTP_STATUS_OK, NULL, "text/html", 0, 0))
    return;

  papplClientHTMLHeader(client, title, 0);

  if (client->system->versions[0].sversion[0])
    papplClientHTMLPrintf(client,
			  "    <div class=\"header2\">\n"
			  "      <div class=\"row\">\n"
			  "        <div class=\"col-12 nav\">\n"
			  "          Version %s\n"
			  "        </div>\n"
			  "      </div>\n"
			  "    </div>\n", client->system->versions[0].sversion);

  papplClientHTMLPuts(client, "    <div class=\"content\">\n");

  if (title)
    papplClientHTMLPrintf(client,
			  "      <div class=\"row\">\n"
			  "        <div class=\"col-12\">\n"
			  "          <h1 class=\"title\">%s</h1>\n", title);
}


#ifdef HAVE_GNUTLS
//
// 'tls_install_certificate()' - Install a certificate and private key.
//

static bool				// O - `true` on success, `false` otherwise
tls_install_certificate(
    pappl_client_t *client,		// I - Client
    const char     *crtfile,		// I - PEM-encoded certificate filename
    const char     *keyfile)		// I - PEM-encoded private key filename
{
  pappl_system_t *system = papplClientGetSystem(client);
					// System
  const char	*home;			// Home directory
  char		hostname[256],		// Hostname
		basedir[256],		// CUPS directory
		ssldir[256],		// CUPS "ssl" directory
		dstcrt[1024],		// Destination certificate
		dstkey[1024];		// Destination private key
  gnutls_certificate_credentials_t *credentials;
					// TLS credentials
  int		status;			// Status for loading of credentials


  // Try loading the credentials...
  if ((credentials = (gnutls_certificate_credentials_t *)malloc(sizeof(gnutls_certificate_credentials_t))) == NULL)
    return (false);

  gnutls_certificate_allocate_credentials(credentials);

  status = gnutls_certificate_set_x509_key_file(*credentials, crtfile, keyfile, GNUTLS_X509_FMT_PEM);
  gnutls_certificate_free_credentials(*credentials);
  free(credentials);

  if (status != 0)
  {
    papplLogClient(client, PAPPL_LOGLEVEL_ERROR, "Unable to load TLS credentials: %s", gnutls_strerror(status));
    return (false);
  }

  // If everything checks out, copy the certificate and private key to the
  // CUPS "ssl" directory...
  home = getuid() ? getenv("HOME") : NULL;
  if (home)
    snprintf(basedir, sizeof(basedir), "%s/.cups", home);
  else
    strlcpy(basedir, "/etc/cups", sizeof(basedir));

  if (access(basedir, X_OK))
  {
    // Make "~/.cups" or "/etc/cups" directory...
    if (mkdir(basedir, 0755))
    {
      papplLogClient(client, PAPPL_LOGLEVEL_ERROR, "Unable to create directory '%s': %s", basedir, strerror(errno));
      return (false);
    }
  }

  snprintf(ssldir, sizeof(ssldir), "%s/ssl", basedir);
  if (access(ssldir, X_OK))
  {
    // Make "~/.cups/ssl" or "/etc/cups/ssl" directory...
    if (mkdir(ssldir, 0755))
    {
      papplLogClient(client, PAPPL_LOGLEVEL_ERROR, "Unable to create directory '%s': %s", ssldir, strerror(errno));
      return (false);
    }
  }

  snprintf(dstkey, sizeof(dstkey), "%s/%s.key", ssldir, papplSystemGetHostname(system, hostname, sizeof(hostname)));
  snprintf(dstcrt, sizeof(dstcrt), "%s/%s.crt", ssldir, hostname);
  if (!tls_install_file(client, dstkey, keyfile))
  {
    unlink(dstcrt);
    return (false);
  }

  if (!tls_install_file(client, dstcrt, crtfile))
  {
    unlink(dstkey);
    return (false);
  }

  // If we get this far we are done!
  return (true);
}


//
// 'tls_install_file()' - Copy a TLS file.
//

static bool				// O - `true` on success, `false` otherwise
tls_install_file(
    pappl_client_t *client,		// I - Client
    const char     *dst,		// I - Destination filename
    const char     *src)		// I - Source filename
{
  cups_file_t	*dstfile,		// Destination file
		*srcfile;		// Source file
  char		buffer[32768];		// Copy buffer
  ssize_t	bytes;			// Bytes to copy


  if ((dstfile = cupsFileOpen(dst, "wb")) == NULL)
  {
    papplLogClient(client, PAPPL_LOGLEVEL_ERROR, "Unable to create file '%s': %s", dst, strerror(errno));
    return (false);
  }

  if ((srcfile = cupsFileOpen(src, "rb")) == NULL)
  {
    papplLogClient(client, PAPPL_LOGLEVEL_ERROR, "Unable to open file '%s': %s", src, strerror(errno));
    cupsFileClose(dstfile);
    unlink(dst);
    return (false);
  }

  while ((bytes = cupsFileRead(srcfile, buffer, sizeof(buffer))) > 0)
  {
    if (cupsFileWrite(dstfile, buffer, (size_t)bytes) < 0)
    {
      papplLogClient(client, PAPPL_LOGLEVEL_ERROR, "Unable to write file '%s': %s", dst, strerror(errno));
      cupsFileClose(dstfile);
      unlink(dst);
      cupsFileClose(srcfile);
      return (false);
    }
  }

  cupsFileClose(dstfile);
  cupsFileClose(srcfile);

  return (true);
}


//
// 'tls_make_certificate()' - Make a self-signed certificate and private key.
//

static bool				// O - `true` on success, `false` otherwise
tls_make_certificate(
    pappl_client_t *client,		// I - Client
    int            num_form,		// I - Number of form variables
    cups_option_t  *form)		// I - Form variables
{
  int		i;			// Looping var
  pappl_system_t *system = papplClientGetSystem(client);
					// System
  const char	*home,			// Home directory
		*value,			// Value from form variables
		*level,			// Level/algorithm+bits
		*email,			// Email address
		*organization,		// Organization name
		*org_unit,		// Organizational unit, if any
		*city,			// City/locality
		*state,			// State/province
		*country;		// Country
  int		duration;		// Duration in years
  int		num_alt_names = 1;	// Alternate names
  char		alt_names[4][256];	// Subject alternate names
  char		hostname[256],		// Hostname
		*domain,		// Domain name
		basedir[256],		// CUPS directory
		ssldir[256],		// CUPS "ssl" directory
		crtfile[1024],		// Certificate file
		keyfile[1024];		// Private key file
  gnutls_x509_crt_t crt;		// Self-signed certificate
  gnutls_x509_privkey_t key;		// Private/public key pair
  cups_file_t	*fp;			// Key/cert file
  unsigned char	buffer[8192];		// Buffer for key/cert data
  size_t	bytes;			// Number of bytes of data
  unsigned char	serial[4];		// Serial number buffer
  int		status;			// GNU TLS status


  // Verify that we have all of the required form variables...
  if ((value = cupsGetOption("duration", num_form, form)) == NULL)
  {
    papplLogClient(client, PAPPL_LOGLEVEL_ERROR, "Missing 'duration' form field.");
    return (false);
  }
  else if ((duration = atoi(value)) < 1 || duration > 10)
  {
    papplLogClient(client, PAPPL_LOGLEVEL_ERROR, "Bad 'duration'='%s' form field.", value);
    return (false);
  }

  if ((level = cupsGetOption("level", num_form, form)) == NULL)
  {
    papplLogClient(client, PAPPL_LOGLEVEL_ERROR, "Missing 'level' form field.");
    return (false);
  }

  if ((email = cupsGetOption("email", num_form, form)) == NULL)
  {
    papplLogClient(client, PAPPL_LOGLEVEL_ERROR, "Missing 'email' form field.");
    return (false);
  }

  if ((organization = cupsGetOption("organization", num_form, form)) == NULL)
  {
    papplLogClient(client, PAPPL_LOGLEVEL_ERROR, "Missing 'organization' form field.");
    return (false);
  }

  if ((org_unit = cupsGetOption("organizational_unit", num_form, form)) == NULL)
  {
    papplLogClient(client, PAPPL_LOGLEVEL_ERROR, "Missing 'organizational_unit' form field.");
    return (false);
  }

  if ((city = cupsGetOption("city", num_form, form)) == NULL)
  {
    papplLogClient(client, PAPPL_LOGLEVEL_ERROR, "Missing 'city' form field.");
    return (false);
  }

  if ((state = cupsGetOption("state", num_form, form)) == NULL)
  {
    papplLogClient(client, PAPPL_LOGLEVEL_ERROR, "Missing 'state' form field.");
    return (false);
  }

  if ((country = cupsGetOption("country", num_form, form)) == NULL)
  {
    papplLogClient(client, PAPPL_LOGLEVEL_ERROR, "Missing 'country' form field.");
    return (false);
  }

  // Get all of the names this system is known by...
  papplSystemGetHostname(system, hostname, sizeof(hostname));
  if ((domain = strchr(hostname, '.')) != NULL)
  {
    // If the domain name is not hostname.local or hostname.lan, make that the
    // second Subject Alternate Name...
    if (strcmp(domain, ".local") && strcmp(domain, ".lan"))
      strlcpy(alt_names[num_alt_names ++], hostname, sizeof(alt_names[0]));

    *domain = '\0';
  }

  // then add hostname as the first alternate name...
  strlcpy(alt_names[0], hostname, sizeof(alt_names[0]));

  // and finish up with hostname.lan and hostname.local as the final alternates...
  snprintf(alt_names[num_alt_names ++], sizeof(alt_names[0]), "%s.lan", hostname);
  snprintf(alt_names[num_alt_names ++], sizeof(alt_names[0]), "%s.local", hostname);

  // Store the certificate and private key in the CUPS "ssl" directory...
  home = getuid() ? getenv("HOME") : NULL;
  if (home)
    snprintf(basedir, sizeof(basedir), "%s/.cups", home);
  else
    strlcpy(basedir, "/etc/cups", sizeof(basedir));

  if (access(basedir, X_OK))
  {
    // Make "~/.cups" or "/etc/cups" directory...
    if (mkdir(basedir, 0755))
    {
      papplLogClient(client, PAPPL_LOGLEVEL_ERROR, "Unable to create directory '%s': %s", basedir, strerror(errno));
      return (false);
    }
  }

  snprintf(ssldir, sizeof(ssldir), "%s/ssl", basedir);
  if (access(ssldir, X_OK))
  {
    // Make "~/.cups/ssl" or "/etc/cups/ssl" directory...
    if (mkdir(ssldir, 0755))
    {
      papplLogClient(client, PAPPL_LOGLEVEL_ERROR, "Unable to create directory '%s': %s", ssldir, strerror(errno));
      return (false);
    }
  }

  snprintf(keyfile, sizeof(keyfile), "%s/%s.key", ssldir, hostname);
  snprintf(crtfile, sizeof(crtfile), "%s/%s.crt", ssldir, hostname);

  papplLogClient(client, PAPPL_LOGLEVEL_DEBUG, "Creating crtfile='%s', keyfile='%s'.", crtfile, keyfile);

  // Create the paired encryption keys...
  gnutls_x509_privkey_init(&key);

  if (!strcmp(level, "rsa-2048"))
    gnutls_x509_privkey_generate(key, GNUTLS_PK_RSA, 2048, 0);
  else if (!strcmp(level, "rsa-4096"))
    gnutls_x509_privkey_generate(key, GNUTLS_PK_RSA, 4096, 0);
  else
    gnutls_x509_privkey_generate(key, GNUTLS_PK_ECDSA, 384, 0);

  // Save the private key...
  bytes = sizeof(buffer);

  if ((status = gnutls_x509_privkey_export(key, GNUTLS_X509_FMT_PEM, buffer, &bytes)) < 0)
  {
    papplLogClient(client, PAPPL_LOGLEVEL_ERROR, "Unable to export private key: %s", gnutls_strerror(status));
    gnutls_x509_privkey_deinit(key);
    return (false);
  }
  else if ((fp = cupsFileOpen(keyfile, "w")) != NULL)
  {
    cupsFileWrite(fp, (char *)buffer, bytes);
    cupsFileClose(fp);
  }
  else
  {
    papplLogClient(client, PAPPL_LOGLEVEL_ERROR, "Unable to create private key file '%s': %s", keyfile, strerror(errno));
    gnutls_x509_privkey_deinit(key);
    return (false);
  }

  // Create the self-signed certificate...
  i         = (int)(time(NULL) / 60);
  serial[0] = i >> 24;
  serial[1] = i >> 16;
  serial[2] = i >> 8;
  serial[3] = i;

  gnutls_x509_crt_init(&crt);
  gnutls_x509_crt_set_dn_by_oid(crt, GNUTLS_OID_X520_COUNTRY_NAME, 0, country, (unsigned)strlen(country));
  gnutls_x509_crt_set_dn_by_oid(crt, GNUTLS_OID_X520_COMMON_NAME, 0, hostname, (unsigned)strlen(hostname));
  gnutls_x509_crt_set_dn_by_oid(crt, GNUTLS_OID_X520_ORGANIZATION_NAME, 0, organization, (unsigned)strlen(organization));
  gnutls_x509_crt_set_dn_by_oid(crt, GNUTLS_OID_X520_ORGANIZATIONAL_UNIT_NAME, 0, org_unit, (unsigned)strlen(org_unit));
  gnutls_x509_crt_set_dn_by_oid(crt, GNUTLS_OID_X520_STATE_OR_PROVINCE_NAME, 0, state, (unsigned)strlen(state));
  gnutls_x509_crt_set_dn_by_oid(crt, GNUTLS_OID_X520_LOCALITY_NAME, 0, city, (unsigned)strlen(city));
  gnutls_x509_crt_set_dn_by_oid(crt, GNUTLS_OID_PKCS9_EMAIL, 0, email, (unsigned)strlen(email));
  gnutls_x509_crt_set_key(crt, key);
  gnutls_x509_crt_set_serial(crt, serial, sizeof(serial));
  gnutls_x509_crt_set_activation_time(crt, time(NULL));
  gnutls_x509_crt_set_expiration_time(crt, time(NULL) + duration * 365 * 86400);
  gnutls_x509_crt_set_ca_status(crt, 0);
  gnutls_x509_crt_set_subject_alt_name(crt, GNUTLS_SAN_DNSNAME, alt_names[0], (unsigned)strlen(alt_names[0]), GNUTLS_FSAN_SET);
  for (i = 1; i < num_alt_names; i ++)
    gnutls_x509_crt_set_subject_alt_name(crt, GNUTLS_SAN_DNSNAME, alt_names[i], (unsigned)strlen(alt_names[i]), GNUTLS_FSAN_APPEND);
  gnutls_x509_crt_set_key_purpose_oid(crt, GNUTLS_KP_TLS_WWW_SERVER, 0);
  gnutls_x509_crt_set_key_usage(crt, GNUTLS_KEY_DIGITAL_SIGNATURE | GNUTLS_KEY_KEY_ENCIPHERMENT);
  gnutls_x509_crt_set_version(crt, 3);

  bytes = sizeof(buffer);
  if (gnutls_x509_crt_get_key_id(crt, 0, buffer, &bytes) >= 0)
    gnutls_x509_crt_set_subject_key_id(crt, buffer, bytes);

  gnutls_x509_crt_sign(crt, crt, key);

  // Save the certificate and public key...
  bytes = sizeof(buffer);
  if ((status = gnutls_x509_crt_export(crt, GNUTLS_X509_FMT_PEM, buffer, &bytes)) < 0)
  {
    papplLogClient(client, PAPPL_LOGLEVEL_ERROR, "Unable to export public key and X.509 certificate: %s", gnutls_strerror(status));
    gnutls_x509_crt_deinit(crt);
    gnutls_x509_privkey_deinit(key);
    return (false);
  }
  else if ((fp = cupsFileOpen(crtfile, "w")) != NULL)
  {
    cupsFileWrite(fp, (char *)buffer, bytes);
    cupsFileClose(fp);
  }
  else
  {
    papplLogClient(client, PAPPL_LOGLEVEL_ERROR, "Unable to create public key and X.509 certificate file '%s': %s", crtfile, strerror(errno));
    gnutls_x509_crt_deinit(crt);
    gnutls_x509_privkey_deinit(key);
    return (false);
  }

  // Now create symlinks for each of the alternate names...
  for (i = 1; i < num_alt_names; i ++)
  {
    char altfile[1024];			// Alternate cert/key filename

    snprintf(altfile, sizeof(altfile), "%s/%s.key", ssldir, alt_names[i]);
    unlink(altfile);
    symlink(keyfile, altfile);

    snprintf(altfile, sizeof(altfile), "%s/%s.crt", ssldir, alt_names[i]);
    unlink(altfile);
    symlink(crtfile, altfile);
  }

  // If we get this far we are done!
  gnutls_x509_crt_deinit(crt);
  gnutls_x509_privkey_deinit(key);

  return (true);
}


//
// 'tls_make_certsignreq()' - Make a certificate signing request and private key.
//

static bool				// O - `true` on success, `false` otherwise
tls_make_certsignreq(
    pappl_client_t *client,		// I - Client
    int            num_form,		// I - Number of form variables
    cups_option_t  *form,		// I - Form variables
    char           *crqpath,		// I - Certificate request filename buffer
    size_t         crqsize)		// I - Size of certificate request buffer
{
  pappl_system_t *system = papplClientGetSystem(client);
					// System
  const char	*level,			// Level/algorithm+bits
		*email,			// Email address
		*organization,		// Organization name
		*org_unit,		// Organizational unit, if any
		*city,			// City/locality
		*state,			// State/province
		*country;		// Country
  char		hostname[256],		// Hostname
		crqfile[1024],		// Certificate request file
		keyfile[1024];		// Private key file
  gnutls_x509_crq_t crq;		// Certificate request
  gnutls_x509_privkey_t key;		// Private/public key pair
  cups_file_t	*fp;			// Key/cert file
  unsigned char	buffer[8192];		// Buffer for key/cert data
  size_t	bytes;			// Number of bytes of data
  int		status;			// GNU TLS status


  *crqpath = '\0';

  // Verify that we have all of the required form variables...
  if ((level = cupsGetOption("level", num_form, form)) == NULL)
  {
    papplLogClient(client, PAPPL_LOGLEVEL_ERROR, "Missing 'level' form field.");
    return (false);
  }

  if ((email = cupsGetOption("email", num_form, form)) == NULL)
  {
    papplLogClient(client, PAPPL_LOGLEVEL_ERROR, "Missing 'email' form field.");
    return (false);
  }

  if ((organization = cupsGetOption("organization", num_form, form)) == NULL)
  {
    papplLogClient(client, PAPPL_LOGLEVEL_ERROR, "Missing 'organization' form field.");
    return (false);
  }

  if ((org_unit = cupsGetOption("organizational_unit", num_form, form)) == NULL)
  {
    papplLogClient(client, PAPPL_LOGLEVEL_ERROR, "Missing 'organizational_unit' form field.");
    return (false);
  }

  if ((city = cupsGetOption("city", num_form, form)) == NULL)
  {
    papplLogClient(client, PAPPL_LOGLEVEL_ERROR, "Missing 'city' form field.");
    return (false);
  }

  if ((state = cupsGetOption("state", num_form, form)) == NULL)
  {
    papplLogClient(client, PAPPL_LOGLEVEL_ERROR, "Missing 'state' form field.");
    return (false);
  }

  if ((country = cupsGetOption("country", num_form, form)) == NULL)
  {
    papplLogClient(client, PAPPL_LOGLEVEL_ERROR, "Missing 'country' form field.");
    return (false);
  }

  // Store the certificate request and private key in the spool directory...
  snprintf(keyfile, sizeof(keyfile), "%s/%s.key", system->directory, papplSystemGetHostname(system, hostname, sizeof(hostname)));
  snprintf(crqfile, sizeof(crqfile), "%s/%s.csr", system->directory, hostname);
  snprintf(crqpath, crqsize, "/%s.csr", hostname);

  // Create the paired encryption keys...
  gnutls_x509_privkey_init(&key);

  if (!strcmp(level, "rsa-2048"))
    gnutls_x509_privkey_generate(key, GNUTLS_PK_RSA, 2048, 0);
  else if (!strcmp(level, "rsa-4096"))
    gnutls_x509_privkey_generate(key, GNUTLS_PK_RSA, 4096, 0);
  else
    gnutls_x509_privkey_generate(key, GNUTLS_PK_ECDSA, 384, 0);

  // Save the private key...
  bytes = sizeof(buffer);

  if ((status = gnutls_x509_privkey_export(key, GNUTLS_X509_FMT_PEM, buffer, &bytes)) < 0)
  {
    papplLogClient(client, PAPPL_LOGLEVEL_ERROR, "Unable to export private key: %s", gnutls_strerror(status));
    gnutls_x509_privkey_deinit(key);
    return (false);
  }
  else if ((fp = cupsFileOpen(keyfile, "w")) != NULL)
  {
    cupsFileWrite(fp, (char *)buffer, bytes);
    cupsFileClose(fp);
  }
  else
  {
    papplLogClient(client, PAPPL_LOGLEVEL_ERROR, "Unable to create private key file '%s': %s", keyfile, strerror(errno));
    gnutls_x509_privkey_deinit(key);
    return (false);
  }

  // Create the certificate request...
  gnutls_x509_crq_init(&crq);
  gnutls_x509_crq_set_dn_by_oid(crq, GNUTLS_OID_X520_COUNTRY_NAME, 0, country, (unsigned)strlen(country));
  gnutls_x509_crq_set_dn_by_oid(crq, GNUTLS_OID_X520_COMMON_NAME, 0, hostname, (unsigned)strlen(hostname));
  gnutls_x509_crq_set_dn_by_oid(crq, GNUTLS_OID_X520_ORGANIZATION_NAME, 0, organization, (unsigned)strlen(organization));
  gnutls_x509_crq_set_dn_by_oid(crq, GNUTLS_OID_X520_ORGANIZATIONAL_UNIT_NAME, 0, org_unit, (unsigned)strlen(org_unit));
  gnutls_x509_crq_set_dn_by_oid(crq, GNUTLS_OID_X520_STATE_OR_PROVINCE_NAME, 0, state, (unsigned)strlen(state));
  gnutls_x509_crq_set_dn_by_oid(crq, GNUTLS_OID_X520_LOCALITY_NAME, 0, city, (unsigned)strlen(city));
  gnutls_x509_crq_set_dn_by_oid(crq, GNUTLS_OID_PKCS9_EMAIL, 0, email, (unsigned)strlen(email));
  gnutls_x509_crq_set_key(crq, key);
  gnutls_x509_crq_set_subject_alt_name(crq, GNUTLS_SAN_DNSNAME, hostname, (unsigned)strlen(hostname), GNUTLS_FSAN_SET);
  gnutls_x509_crq_set_key_purpose_oid(crq, GNUTLS_KP_TLS_WWW_SERVER, 0);
  gnutls_x509_crq_set_key_usage(crq, GNUTLS_KEY_DIGITAL_SIGNATURE | GNUTLS_KEY_KEY_ENCIPHERMENT);
  gnutls_x509_crq_set_version(crq, 3);

  gnutls_x509_crq_sign2(crq, key, GNUTLS_DIG_SHA256, 0);

  // Save the certificate request and public key...
  bytes = sizeof(buffer);
  if ((status = gnutls_x509_crq_export(crq, GNUTLS_X509_FMT_PEM, buffer, &bytes)) < 0)
  {
    papplLogClient(client, PAPPL_LOGLEVEL_ERROR, "Unable to export public key and X.509 certificate request: %s", gnutls_strerror(status));
    gnutls_x509_crq_deinit(crq);
    gnutls_x509_privkey_deinit(key);
    return (false);
  }
  else if ((fp = cupsFileOpen(crqfile, "w")) != NULL)
  {
    cupsFileWrite(fp, (char *)buffer, bytes);
    cupsFileClose(fp);
  }
  else
  {
    papplLogClient(client, PAPPL_LOGLEVEL_ERROR, "Unable to create public key and X.509 certificate request file '%s': %s", crqfile, strerror(errno));
    gnutls_x509_crq_deinit(crq);
    gnutls_x509_privkey_deinit(key);
    return (false);
  }

  // If we get this far we are done!
  papplSystemAddResourceFile(system, crqpath, "application/pkcs10", crqfile);

  gnutls_x509_crq_deinit(crq);
  gnutls_x509_privkey_deinit(key);

  return (true);
}
#endif // HAVE_GNUTLS
