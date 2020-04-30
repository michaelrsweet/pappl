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


//
// Local types...
//

typedef struct _pappl_netconf_s		// Network configuration data
{
  char	hostname[256],			// Host name
      	dns_address1[256],		// DNS address #1
      	dns_address2[256];		// DNS address #2
} _pappl_netconf_t;

typedef struct _pappl_netif_s		// Network interface configuration data
{
  char	iface[128],			// Interface name
	desc[128];			// Interface description
  bool	use_dhcp,			// Use DHCP?
	is_enabled,			// Is this interface enabled?
	is_wifi;			// Is this interface Wi-Fi?
  char	wifi_ssid[128],			// Wi-Fi SSID, if any
	wifi_password[128],		// Wi-Fi password, if any
      	ipv4_address[16],		// IPv4 address
      	ipv4_netmask[16],		// IPv4 netmask
      	ipv4_gateway[16],		// IPv4 gateway/router
      	ipv6_address[40],		// IPv4 address
      	ipv6_netmask[40],		// IPv4 netmask
      	ipv6_router[40];		// IPv4 router
} _pappl_netif_t;

typedef struct _pappl_wifi_s		// Wi-Fi network information
{
  char	ssid[128];			// Wi-Fi ID
  bool	is_secure;			// Needs a password?
} _pappl_wifi_t;


//
// Local functions...
//

static int	get_network(_pappl_netconf_t *netconf, int max_netifs, _pappl_netif_t *netifs);
static bool	set_network(_pappl_netconf_t *netconf, int num_netifs, _pappl_netif_t *netifs);
static int	get_wifi_networks(int max_wifi, _pappl_wifi_t *wifis);

static void	system_footer(pappl_client_t *client);
static void	system_header(pappl_client_t *client, const char *title);


//
// Local globals...
//

static const char * const countries[][2] =
{					// List of countries and their ISO 3166 2-letter codes
  { "af", "Afghanistan" },
  { "ax", "Åland Islands" },
  { "al", "Albania" },
  { "dz", "Algeria" },
  { "as", "American Samoa" },
  { "ad", "Andorra" },
  { "ao", "Angola" },
  { "ai", "Anguilla" },
  { "aq", "Antarctica" },
  { "ag", "Antigua and Barbuda" },
  { "ar", "Argentina" },
  { "am", "Armenia" },
  { "aw", "Aruba" },
  { "au", "Australia" },
  { "at", "Austria" },
  { "az", "Azerbaijan" },
  { "bs", "Bahamas" },
  { "bh", "Bahrain" },
  { "bd", "Bangladesh" },
  { "bb", "Barbados" },
  { "by", "Belarus" },
  { "be", "Belgium" },
  { "bz", "Belize" },
  { "bj", "Benin" },
  { "bm", "Bermuda" },
  { "bt", "Bhutan" },
  { "bo", "Bolivia (Plurinational State of)" },
  { "bq", "Bonaire, Sint Eustatius and Saba" },
  { "ba", "Bosnia and Herzegovina" },
  { "bw", "Botswana" },
  { "bv", "Bouvet Island" },
  { "br", "Brazil" },
  { "io", "British Indian Ocean Territory" },
  { "bn", "Brunei Darussalam" },
  { "bg", "Bulgaria" },
  { "bf", "Burkina Faso" },
  { "bi", "Burundi" },
  { "cv", "Cabo Verde" },
  { "kh", "Cambodia" },
  { "cm", "Cameroon" },
  { "ca", "Canada" },
  { "ky", "Cayman Islands" },
  { "cf", "Central African Republic" },
  { "td", "Chad" },
  { "cl", "Chile" },
  { "cn", "China" },
  { "cx", "Christmas Island" },
  { "cc", "Cocos (Keeling) Islands" },
  { "co", "Colombia" },
  { "km", "Comoros" },
  { "cd", "Congo, Democratic Republic of the" },
  { "cg", "Congo" },
  { "ck", "Cook Islands" },
  { "cr", "Costa Rica" },
  { "ci", "Côte d'Ivoire" },
  { "hr", "Croatia" },
  { "cu", "Cuba" },
  { "cw", "Curaçao" },
  { "cy", "Cyprus" },
  { "cz", "Czechia" },
  { "dk", "Denmark" },
  { "dj", "Djibouti" },
  { "dm", "Dominica" },
  { "do", "Dominican Republic" },
  { "ec", "Ecuador" },
  { "eg", "Egypt" },
  { "sv", "El Salvador" },
  { "gq", "Equatorial Guinea" },
  { "er", "Eritrea" },
  { "ee", "Estonia" },
  { "sz", "Eswatini" },
  { "et", "Ethiopia" },
  { "fk", "Falkland Islands (Malvinas)" },
  { "fo", "Faroe Islands" },
  { "fj", "Fiji" },
  { "fi", "Finland" },
  { "fr", "France" },
  { "gf", "French Guiana" },
  { "pf", "French Polynesia" },
  { "tf", "French Southern Territories" },
  { "ga", "Gabon" },
  { "gm", "Gambia" },
  { "ge", "Georgia" },
  { "de", "Germany" },
  { "gh", "Ghana" },
  { "gi", "Gibraltar" },
  { "gr", "Greece" },
  { "gl", "Greenland" },
  { "gd", "Grenada" },
  { "gp", "Guadeloupe" },
  { "gu", "Guam" },
  { "gt", "Guatemala" },
  { "gg", "Guernsey" },
  { "gw", "Guinea-Bissau" },
  { "gn", "Guinea" },
  { "gy", "Guyana" },
  { "ht", "Haiti" },
  { "hm", "Heard Island and McDonald Islands" },
  { "va", "Holy See" },
  { "hn", "Honduras" },
  { "hk", "Hong Kong" },
  { "hu", "Hungary" },
  { "is", "Iceland" },
  { "in", "India" },
  { "id", "Indonesia" },
  { "ir", "Iran (Islamic Republic of)" },
  { "iq", "Iraq" },
  { "ie", "Ireland" },
  { "im", "Isle of Man" },
  { "il", "Israel" },
  { "it", "Italy" },
  { "jm", "Jamaica" },
  { "jp", "Japan" },
  { "je", "Jersey" },
  { "jo", "Jordan" },
  { "kz", "Kazakhstan" },
  { "ke", "Kenya" },
  { "ki", "Kiribati" },
  { "kp", "Korea (Democratic People's Republic of)" },
  { "kr", "Korea, Republic of" },
  { "kw", "Kuwait" },
  { "kg", "Kyrgyzstan" },
  { "la", "Lao People's Democratic Republic" },
  { "lv", "Latvia" },
  { "lb", "Lebanon" },
  { "ls", "Lesotho" },
  { "lr", "Liberia" },
  { "ly", "Libya" },
  { "li", "Liechtenstein" },
  { "lt", "Lithuania" },
  { "lu", "Luxembourg" },
  { "mo", "Macao" },
  { "mg", "Madagascar" },
  { "mw", "Malawi" },
  { "my", "Malaysia" },
  { "mv", "Maldives" },
  { "ml", "Mali" },
  { "mt", "Malta" },
  { "mh", "Marshall Islands" },
  { "mq", "Martinique" },
  { "mr", "Mauritania" },
  { "mu", "Mauritius" },
  { "yt", "Mayotte" },
  { "mx", "Mexico" },
  { "fm", "Micronesia (Federated States of)" },
  { "md", "Moldova, Republic of" },
  { "mc", "Monaco" },
  { "mn", "Mongolia" },
  { "me", "Montenegro" },
  { "ms", "Montserrat" },
  { "ma", "Morocco" },
  { "mz", "Mozambique" },
  { "mm", "Myanmar" },
  { "na", "Namibia" },
  { "nr", "Nauru" },
  { "np", "Nepal" },
  { "nl", "Netherlands" },
  { "nc", "New Caledonia" },
  { "nz", "New Zealand" },
  { "ni", "Nicaragua" },
  { "ne", "Niger" },
  { "ng", "Nigeria" },
  { "nu", "Niue" },
  { "nf", "Norfolk Island" },
  { "mk", "North Macedonia" },
  { "mp", "Northern Mariana Islands" },
  { "no", "Norway" },
  { "om", "Oman" },
  { "pk", "Pakistan" },
  { "pw", "Palau" },
  { "ps", "Palestine, State of" },
  { "pa", "Panama" },
  { "pg", "Papua New Guinea" },
  { "py", "Paraguay" },
  { "pe", "Peru" },
  { "ph", "Philippines" },
  { "pn", "Pitcairn" },
  { "pl", "Poland" },
  { "pt", "Portugal" },
  { "pr", "Puerto Rico" },
  { "qa", "Qatar" },
  { "re", "Réunion" },
  { "ro", "Romania" },
  { "ru", "Russian Federation" },
  { "rw", "Rwanda" },
  { "bl", "Saint Barthélemy" },
  { "sh", "Saint Helena, Ascension and Tristan da Cunha" },
  { "kn", "Saint Kitts and Nevis" },
  { "lc", "Saint Lucia" },
  { "mf", "Saint Martin (French part)" },
  { "pm", "Saint Pierre and Miquelon" },
  { "vc", "Saint Vincent and the Grenadines" },
  { "ws", "Samoa" },
  { "sm", "San Marino" },
  { "st", "Sao Tome and Principe" },
  { "sa", "Saudi Arabia" },
  { "sn", "Senegal" },
  { "rs", "Serbia" },
  { "sc", "Seychelles" },
  { "sl", "Sierra Leone" },
  { "sg", "Singapore" },
  { "sx", "Sint Maarten (Dutch part)" },
  { "sk", "Slovakia" },
  { "si", "Slovenia" },
  { "sb", "Solomon Islands" },
  { "so", "Somalia" },
  { "za", "South Africa" },
  { "gs", "South Georgia and the South Sandwich Islands" },
  { "ss", "South Sudan" },
  { "es", "Spain" },
  { "lk", "Sri Lanka" },
  { "sd", "Sudan" },
  { "sr", "Suriname" },
  { "sj", "Svalbard and Jan Mayen" },
  { "se", "Sweden" },
  { "ch", "Switzerland" },
  { "sy", "Syrian Arab Republic" },
  { "tw", "Taiwan, Province of China" },
  { "tj", "Tajikistan" },
  { "tz", "Tanzania, United Republic of" },
  { "th", "Thailand" },
  { "tl", "Timor-Leste" },
  { "tg", "Togo" },
  { "tk", "Tokelau" },
  { "to", "Tonga" },
  { "tt", "Trinidad and Tobago" },
  { "tn", "Tunisia" },
  { "tr", "Turkey" },
  { "tm", "Turkmenistan" },
  { "tc", "Turks and Caicos Islands" },
  { "tv", "Tuvalu" },
  { "ug", "Uganda" },
  { "ua", "Ukraine" },
  { "ae", "United Arab Emirates" },
  { "gb", "United Kingdom of Great Britain and Northern Ireland" },
  { "uk", "United Kingdom" },
  { "um", "United States Minor Outlying Islands" },
  { "us", "United States of America" },
  { "uy", "Uruguay" },
  { "uz", "Uzbekistan" },
  { "vu", "Vanuatu" },
  { "ve", "Venezuela (Bolivarian Republic of)" },
  { "vn", "Viet Nam" },
  { "vg", "Virgin Islands (British)" },
  { "vi", "Virgin Islands (U.S.)" },
  { "wf", "Wallis and Futuna" },
  { "eh", "Western Sahara" },
  { "ye", "Yemen" },
  { "zm", "Zambia" },
  { "zw", "Zimbabwe" }
};


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
    else if (!papplClientValidateForm(client, num_form, form))
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
  _pappl_netconf_t	netconf;	// Network configuration
  _pappl_netif_t	netifs[100];	// Network interfaces


  system_header(client, NULL);

  papplClientHTMLPrintf(client,
			"      <div class=\"row\">\n"
			"        <div class=\"col-6\">\n"
			"          <h1 class=\"title\">Configuration <a class=\"btn\" href=\"https://%s:%d/config\">Change</a></h1>\n", client->host_field, client->host_port);

  _papplClientHTMLInfo(client, false, system->dns_sd_name, system->location, system->geo_location, system->organization, system->org_unit, &system->contact);

  if (system->options & (PAPPL_SOPTIONS_NETWORK | PAPPL_SOPTIONS_SECURITY | PAPPL_SOPTIONS_TLS))
  {
    papplClientHTMLPuts(client,
                        "          <h2 class=\"title\">Settings</h2>\n"
                        "          <div class=\"btn\">");
    if ((system->options & PAPPL_SOPTIONS_NETWORK) && get_network(&netconf, (int)(sizeof(netifs) / sizeof(netifs[0])), netifs) > 0)
      papplClientHTMLPrintf(client, "<a class=\"btn\" href=\"https://%s:%d/network\">Network</a> ", client->host_field, client->host_port);
    if (system->options & PAPPL_SOPTIONS_SECURITY)
      papplClientHTMLPrintf(client, "<a class=\"btn\" href=\"https://%s:%d/security\">Security</a> ", client->host_field, client->host_port);
    if (system->options & PAPPL_SOPTIONS_TLS)
      papplClientHTMLPrintf(client,
                            "<a class=\"btn\" href=\"https://%s:%d/tls-install-crt\">Install TLS Certificate</a> "
                            "<a class=\"btn\" href=\"https://%s:%d/tls-new-crt\">Create New TLS Certificate</a> "
                            "<a class=\"btn\" href=\"https://%s:%d/tls-new-csr\">Create TLS Certificate Request</a> ", client->host_field, client->host_port, client->host_field, client->host_port, client->host_field, client->host_port);
    papplClientHTMLPuts(client, "</div>\n");
  }

  papplClientHTMLPuts(client,
		      "        </div>\n"
                      "        <div class=\"col-6\">\n"
                      "          <h1 class=\"title\">Printers</h1>\n");

  papplSystemIteratePrinters(system, (pappl_printer_cb_t)_papplPrinterIteratorWebCallback, client);

  papplClientHTMLPuts(client,
                      "        </div>\n"
                      "      </div>\n");

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
  int		i;			// Looping var
  _pappl_netconf_t netconf;		// Network configuration
  int		num_netifs;		// Number of network interfaces
  _pappl_netif_t netifs[100],		// Network interfaces
		*netif;			// Current network interface
  const char	*status = NULL;		// Status message, if any
  static const char	*ipv4_address_pattern = "^(25[0-9]|2[0-4][0-9]|1[0-9]{2}|[1-9][0-9]|[0-9])\\.(25[0-9]|2[0-4][0-9]|1[0-9]{2}|[1-9][0-9]|[0-9])\\.(25[0-9]|2[0-4][0-9]|1[0-9]{2}|[1-9][0-9]|[0-9])\\.(25[0-9]|2[0-4][0-9]|1[0-9]{2}|[1-9][0-9]|[1-9])$";
  static const char	*ipv4_netmask_pattern = "^((128|192|224|240|248|252|254|255)\\.0\\.0\\.0|255\\.(128|192|224|240|248|252|254|255)\\.0\\.0|255\\.255\\.(128|192|224|240|248|252|254|255)\\.0|255\\.255\\.255\\.(128|192|224|240|248|252|254))$";


  if (!papplClientHTMLAuthorize(client))
    return;

  num_netifs = get_network(&netconf, (int)(sizeof(netifs) / sizeof(netifs[0])), netifs);

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
      char	name[128];		// Form variable name
      const char *value;		// Form variable value

      if ((value = cupsGetOption("hostname", num_form, form)) != NULL)
        strlcpy(netconf.hostname, value, sizeof(netconf.hostname));
      if ((value = cupsGetOption("dns_address1", num_form, form)) != NULL)
	strlcpy(netconf.dns_address1, value, sizeof(netconf.dns_address1));
      if ((value = cupsGetOption("dns_address2", num_form, form)) != NULL)
	strlcpy(netconf.dns_address2, value, sizeof(netconf.dns_address2));

      for (i = 0, netif = netifs; i < num_netifs; i ++, netif ++)
      {
        snprintf(name, sizeof(name), "mode%d", i);
        if ((value = cupsGetOption(name, num_form, form)) != NULL)
          netif->use_dhcp = !strcmp(value, "dhcp");

        if (netif->use_dhcp)
        {
          netif->ipv4_address[0] = '\0';
          netif->ipv4_netmask[0] = '\0';
          netif->ipv4_gateway[0] = '\0';
        }
        else
        {
          snprintf(name, sizeof(name), "ipv4_address%d", i);
          if ((value = cupsGetOption(name, num_form, form)) != NULL)
            strlcpy(netif->ipv4_address, value, sizeof(netif->ipv4_address));

          snprintf(name, sizeof(name), "ipv4_netmask%d", i);
          if ((value = cupsGetOption(name, num_form, form)) != NULL)
            strlcpy(netif->ipv4_netmask, value, sizeof(netif->ipv4_netmask));

          snprintf(name, sizeof(name), "ipv4_gateway%d", i);
          if ((value = cupsGetOption(name, num_form, form)) != NULL)
            strlcpy(netif->ipv4_gateway, value, sizeof(netif->ipv4_gateway));
        }

        if (netif->is_wifi)
        {
          snprintf(name, sizeof(name), "wifi_ssid%d", i);
	  if ((value = cupsGetOption(name, num_form, form)) != NULL)
	    strlcpy(netif->wifi_ssid, value, sizeof(netif->wifi_ssid));

          snprintf(name, sizeof(name), "wifi_password%d", i);
	  if ((value = cupsGetOption(name, num_form, form)) != NULL)
	    strlcpy(netif->wifi_password, value, sizeof(netif->wifi_password));
	}
      }

      if (!set_network(&netconf, num_netifs, netifs))
        status = "Unable to save network changes.";
      else
        status = "Changes saved.";
    }

    cupsFreeOptions(num_form, form);
  }

  system_header(client, "Networking");

  if (status)
    papplClientHTMLPrintf(client, "<div class=\"banner\">%s</div>\n", status);

  papplClientHTMLPuts(client,
		      "        </div>\n"
		      "      </div>\n");
  papplClientHTMLStartForm(client, client->uri, false);
  papplClientHTMLPrintf(client,
			"      <div class=\"row\">\n"
			"        <div class=\"col-4\">\n"
			"          <h2 class=\"title\">DNS</h2>\n"
			"          <table class=\"form\">\n"
			"            <tbody>\n"
			"              <tr><th><label for=\"hostname\">Hostname:</label></th><td><input type=\"text\" name=\"hostname\" value=\"%s\" placeholder=\"name.domain\" pattern=\"^(|[-_a-zA-Z0-9][.-_a-zA-Z0-9]*)$\"></td></tr>\n"
			"              <tr><th><label for=\"dns_address1\">Primary DNS:</label></th><td><input type=\"text\" name=\"dns_address1\" value=\"%s\" placeholder=\"N.N.N.N\" pattern=\"%s\"></td></tr>\n"
			"              <tr><th><label for=\"dns_address2\">Secondary DNS:</label></th><td><input type=\"text\" name=\"dns_address2\" value=\"%s\" placeholder=\"N.N.N.N\" pattern=\"%s\"></td></tr>\n"
			"              <tr><th></th><td><input type=\"submit\" value=\"Save Changes\"></td></tr>\n"
			"            </tbody>\n"
			"          </table>\n"
			"        </div>\n", netconf.hostname, netconf.dns_address1, ipv4_address_pattern, netconf.dns_address2, ipv4_address_pattern);

  for (i = 0, netif = netifs; i < num_netifs; i ++, netif ++)
  {
    papplClientHTMLPrintf(client,
			  "        <div class=\"col-4\">\n"
			  "          <h2 class=\"title\">%s</h2>\n"
			  "          <table class=\"form\">\n"
			  "            <tbody>\n", netif->desc);

    if (netif->is_wifi)
    {
      int		j,		// Looping var
		      num_wifis;	// Number of Wi-Fi networks
      _pappl_wifi_t	wifis[100],	// Wi-Fi networks we can see
		      *wifi;		// Current Wi-Fi network

      num_wifis = get_wifi_networks((int)(sizeof(wifis) / sizeof(wifis[0])), wifis);
      papplClientHTMLPrintf(client, "              <tr><th><label for=\"wifi_ssid%d\">Network:</label></th><td><select name=\"wifi_ssid%d\"><option value=\"\">None</option>", i, i);
      for (j = 0, wifi = wifis; j < num_wifis; j ++, wifi ++)
	papplClientHTMLPrintf(client, "<option value=\"%s\"%s>%s%s</option>", wifi->ssid, !strcmp(wifi->ssid, netif->wifi_ssid) ? " selected" : "", wifi->ssid, wifi->is_secure ? "*" : "");
      papplClientHTMLPuts(client, "</select></td></tr>\n");
      papplClientHTMLPrintf(client, "              <tr><th><label for=\"wifi_password%d\">Password:</label></th><td><input type=\"password\" name=\"wifi_password%d\" value=\"%s\"></td></tr>\n", i, i, netif->wifi_password);
    }

    papplClientHTMLPrintf(client,
			  "              <tr><th><label for\"mode%d\">Mode:</label></th><td><input type=\"radio\" name=\"mode%d\" value=\"dhcp\"%s>&nbsp;Automatic&nbsp;(DHCP) <input type=\"radio\" name=\"mode%d\" value=\"manual\"%s>&nbsp;Manual</td></tr>\n"
			  "              <tr><th><label for=\"ipv4_address%d\">IPv4 Address:</label></th><td><input type=\"text\" name=\"ipv4_address%d\" value=\"%s\" pattern=\"%s\" size=\"15\" maxlength=\"15\"></td></tr>\n"
			  "              <tr><th><label for=\"ipv4_netmask%d\">IPv4 Netmask:</label></th><td><input type=\"text\" name=\"ipv4_netmask%d\" value=\"%s\" pattern=\"%s\" size=\"15\" maxlength=\"15\"></td></tr>\n"
			  "              <tr><th><label for=\"ipv4_gateway%d\">IPv4 Gateway:</label></th><td><input type=\"text\" name=\"ipv4_gateway%d\" value=\"%s\" pattern=\"%s\" size=\"15\" maxlength=\"15\"></td></tr>\n", i, i, netif->use_dhcp ? " checked" : "", i, netif->use_dhcp ? "" : " checked", i, i, netif->ipv4_address, ipv4_address_pattern, i, i, netif->ipv4_netmask, ipv4_netmask_pattern, i, i, netif->ipv4_gateway, ipv4_address_pattern);
    papplClientHTMLPuts(client,
			"              <tr><th></th><td><input type=\"submit\" value=\"Save Changes\"></td></tr>\n"
			"            </tbody>\n"
			"          </table>\n"
			"        </div>\n");
  }

  papplClientHTMLPuts(client,
		      "      </div>\n"
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
    else if (!papplClientValidateForm(client, num_form, form))
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
      struct group	grpbuf,		// Group buffer
			*grp = NULL;	// Admin group


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
    papplClientHTMLStartForm(client, client->uri, false);

    papplClientHTMLPuts(client,
			"        <div class=\"col-12\">\n"
			"          <h2 class=\"title\">Users</h2>\n"
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
    papplClientHTMLStartForm(client, client->uri, false);

    papplClientHTMLPuts(client,
			"        <div class=\"col-12\">\n"
			"          <h2 class=\"title\">Change Access Password</h2>\n"
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
    papplClientHTMLStartForm(client, client->uri, false);

    papplClientHTMLPuts(client,
			"        <div class=\"col-12\">\n"
			"          <h2 class=\"title\">Set Access Password</h2>\n"
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
    else if (!papplClientValidateForm(client, num_form, form))
    {
      status = "Invalid form submission.";
    }
    else
    {
      status = "Certificate installed.";
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
		      "          <h2 class=\"title\">Install Certificate</h2>\n"
		      "          <p>This form will install a trusted TLS certificate you have obtained from a Certificate Authority ('CA'). Once installed, it will be used immediately.</p>\n"
		      "          <table class=\"form\">\n"
		      "            <tbody>\n"
		      "              <tr><th><label for=\"certificate\">Certificate:</label></th><td><input type=\"file\" name=\"certificate\" required> (PEM-encoded)</td></tr>\n"
		      "              <tr><th><label for=\"privatekey\">Private Key:</label></th><td><input type=\"file\" name=\"privatekey\"> (PEM-encoded, leave unselected to use the key from the last signing request)</td></tr>\n"
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
    else if (!strcmp(client->uri, "/tls-new-crt"))
    {
      status = "Certificate created.";
    }
    else
    {
      status = "Certificate request created.";
    }

    cupsFreeOptions(num_form, form);
  }

  system_header(client, "TLS Certificates");

  if (status)
    papplClientHTMLPrintf(client, "<div class=\"banner\">%s</div>\n", status);

  papplClientHTMLPuts(client,
                      "        </div>\n"
                      "      </div>\n"
                      "      <div class=\"row\">\n");

  papplClientHTMLStartForm(client, client->uri, false);

  if (!strcmp(client->uri, "/tls-new-crt"))
    papplClientHTMLPuts(client,
			"        <div class=\"col-12\">\n"
			"          <h2 class=\"title\">Create New Certificate</h2>\n"
			"          <p>This form creates a new 'self-signed' TLS certificate for secure printing. Self-signed certificates are not automatically trusted by web browsers.</p>\n"
			"          <table class=\"form\">\n"
			"            <tbody>\n"
			"              <tr><th><label for=\"duration\">Duration:</label></th><td><input type=\"number\" name=\"duration\" min=\"1\" max=\"10\" step=\"1\" value=\"5\" size=\"2\" maxsize=\"2\">&nbsp;years</td></tr>\n");
  else
    papplClientHTMLPuts(client,
			"        <div class=\"col-12\">\n"
			"          <h2 class=\"title\">Create Certificate Signing Request</h2>\n"
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
		      "      for (i = 0; i < country.length; i ++) {\n"
		      "	if (country[i].value == response['address']['country_code']) {\n"
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


//
// 'get_network()' - Load the network configuration.
//
// Note: Currently only supports Linux /etc/network/interfaces file.
//

static int				// O - Number of network interfaces
get_network(_pappl_netconf_t *netconf,	// I - Network configuration
            int              max_netifs,// I - Maximum number of interfaces
	    _pappl_netif_t   *netifs)	// O - Network interfaces
{
  cups_file_t	*fp;			// "/etc/network/interfaces" file
  char		line[1024],		// Line from file
		*value;			// Value on line
  int		linenum = 0;		// Line number in file
  int		num_netifs = 0;		// Number of network interfaces
  _pappl_netif_t *netif = NULL;		// Current network interface


  memset(netconf, 0, sizeof(_pappl_netconf_t));
  memset(netifs, 0, (size_t)max_netifs * sizeof(_pappl_netif_t));

  if (getenv("PAPPL_NETCONF"))
  {
    char	*conf = strdup(getenv("PAPPL_NETCONF")),
					// Copy of environment variable
		*value,			// Current value
		*next;			// Next value

    next  = conf;

    if ((value = strsep(&next, ",")) != NULL)
      strlcpy(netconf->hostname, value, sizeof(netconf->hostname));
    if ((value = strsep(&next, ",")) != NULL)
      strlcpy(netconf->dns_address1, value, sizeof(netconf->dns_address1));
    if ((value = strsep(&next, ",")) != NULL)
      strlcpy(netconf->dns_address2, value, sizeof(netconf->dns_address2));

    free(conf);
  }
  else
  {
    httpGetHostname(NULL, netconf->hostname, (int)sizeof(netconf->hostname));
  }

  if (getenv("PAPPL_NETIFS"))
  {
    // Copy list of network interfaces from the PAPPL_NETWORK environment
    // variable, of the form:
    //
    //   PAPPL_NETWORK="name[,ipv4-address] name[,ipv4-address][,SSID] ..."
    char	*names = strdup(getenv("PAPPL_NETIFS")),
					// Copy of environment variable
		*name,			// Current interface name
		*ipv4,			// IPv4 address, if any
		*ssid,			// Wi-Fi SSID, if any
		*next;			// Next name

    for (name = names, netif = netifs; name && *name && num_netifs < max_netifs; name = next, num_netifs ++, netif ++)
    {
      if ((next = strchr(name, ' ')) != NULL)
      {
        while (*next && isspace(*next & 255))
          *next++ = '\0';
      }

      if ((ipv4 = strchr(name, ',')) != NULL)
        *ipv4++ = '\0';

      if ((ssid = strchr(ipv4, ',')) != NULL)
        *ssid++ = '\0';

      strlcpy(netif->iface, name, sizeof(netif->iface));
      if (!strcmp(name, "eth0"))
      {
	strlcpy(netif->desc, "Ethernet", sizeof(netif->desc));
      }
      else if (!strncmp(name, "eth", 3))
      {
	snprintf(netif->desc, sizeof(netif->desc), "Ethernet (%s)", name);
      }
      else if (!strcmp(name, "wlan0"))
      {
	strlcpy(netif->desc, "Wi-Fi", sizeof(netif->desc));
	netif->is_wifi = true;
      }
      else if (!strncmp(name, "wlan", 4))
      {
	snprintf(netif->desc, sizeof(netif->desc), "Wi-Fi (%s)", name);
	netif->is_wifi = true;
      }
      else
      {
	snprintf(netif->desc, sizeof(netif->desc), "Other (%s)", name);
      }

      netif->use_dhcp = (ipv4 == NULL || !*ipv4);

      if (ipv4 && *ipv4)
      {
        char	*ptr;			// Pointer to last tuple

        strlcpy(netif->ipv4_address, ipv4, sizeof(netif->ipv4_address));
        strlcpy(netif->ipv4_netmask, "255.255.255.0", sizeof(netif->ipv4_netmask));
        strlcpy(netif->ipv4_gateway, ipv4, sizeof(netif->ipv4_gateway));
        if ((ptr = strrchr(netif->ipv4_gateway, '.')) != NULL)
          memcpy(ptr, ".1", 3);
      }

      if (ssid)
        strlcpy(netif->wifi_ssid, ssid, sizeof(netif->wifi_ssid));
    }

    free(names);
  }
  else if (!access("/etc/network/interfaces", W_OK) && (fp = cupsFileOpen("/etc/network/interfaces", "r")) != NULL)
  {
    _pappl_netif_t	*wifi = NULL;	// Wi-Fi network interface

    // Copy network interface configuration from /etc/network/interfaces
    while (cupsFileGetConf(fp, line, sizeof(line), &value, &linenum))
    {
      if (!strcmp(line, "iface") && value && strncmp(value, "lo ", 3))
      {
        char	*mode = strchr(value, ' ');
					// Pointer to next field...

        if (num_netifs >= max_netifs)
          break;

        netif = netifs + num_netifs;
        num_netifs ++;

       if (mode)
         *mode++ = '\0';

        strlcpy(netif->iface, value, sizeof(netif->iface));
        if (!strcmp(value, "eth0"))
        {
          strlcpy(netif->desc, "Ethernet", sizeof(netif->desc));
	}
	else if (!strncmp(value, "eth", 3))
	{
	  snprintf(netif->desc, sizeof(netif->desc), "Ethernet (%s)", value);
	}
	else if (!strcmp(value, "wlan0"))
	{
	  strlcpy(netif->desc, "Wi-Fi", sizeof(netif->desc));
	  netif->is_wifi = true;
	  wifi = netif;
	}
	else if (!strncmp(value, "wlan", 4))
	{
	  snprintf(netif->desc, sizeof(netif->desc), "Wi-Fi (%s)", value);
	  netif->is_wifi = true;
	}
	else
	{
          snprintf(netif->desc, sizeof(netif->desc), "Other (%s)", value);
	}

        if (mode && !strcmp(mode, "dhcp"))
          netif->use_dhcp = true;
      }
      else if (!strcmp(line, "address") && value && netif)
	strlcpy(netif->ipv4_address, value, sizeof(netif->ipv4_address));
      else if (!strcmp(line, "netmask") && value && netif)
	strlcpy(netif->ipv4_netmask, value, sizeof(netif->ipv4_netmask));
      else if (!strcmp(line, "gateway") && value && netif)
	strlcpy(netif->ipv4_gateway, value, sizeof(netif->ipv4_gateway));
    }

    cupsFileClose(fp);

    if (wifi && (fp = cupsFileOpen("/etc/wpa_supplicant/wpa_supplicant.conf", "r")) != NULL)
    {
      linenum = 0;

      while (cupsFileGetConf(fp, line, sizeof(line), &value, &linenum))
      {
        char	*end;			// End of value

        if (!value)
        {
          if ((value = strchr(line, '=')) != NULL)
            *value++ = '\0';
	}

        if (!value || *value != '\"')
          continue;

        value ++;
        if ((end = strchr(value, '\"')) != NULL)
          *end++ = '\0';

        if (!strcmp(line, "ssid"))
          strlcpy(wifi->wifi_ssid, value, sizeof(wifi->wifi_ssid));
        else if (!strcmp(line, "psk"))
          strlcpy(wifi->wifi_password, value, sizeof(wifi->wifi_password));

        if (wifi->wifi_ssid[0] && wifi->wifi_password[0])
          break;
      }

      cupsFileClose(fp);
    }
  }

  return (num_netifs);
}


//
// 'set_network()' - Save the network configuration.
//

static bool				// O - `true` on success, `false` on failure
set_network(_pappl_netconf_t *netconf,	// I - Network configuration
            int              num_netifs,// I - Number of network interfaces
            _pappl_netif_t   *netifs)	// I - Network interfaces
{
  if (getenv("PAPPL_NETCONF") || getenv("PAPPL_NETIFS"))
  {
  }
  else
  {
  }

  return (true);
}


//
// 'get_wifi_networks()' - Get the list of Wi-Fi networks.
//

static int				// O - Number of Wi-Fi networks found
get_wifi_networks(
    int           max_wifi,		// I - Maximum  number of Wi-Fi networks
    _pappl_wifi_t *wifis)		// O - Wi-Fi networks
{
  int	num_wifi = 0;			// Number of Wi-Fi networks


  memset(wifis, 0, (size_t)max_wifi * sizeof(_pappl_wifi_t));

  if (getenv("PAPPL_WIFI"))
  {
    // Load Wi-Fi network list from the PAPPL_WIFI environment variable.  The
    // format is:
    //
    //   [*]ssid[,...,ssid]
    //
    // where "ssid" is the SSID of the Wi-Fi network.  If preceded by an
    // asterisk, the network is "secure" (requires a password)
    char	*networks = strdup(getenv("PAPPL_WIFI")),
					// Wi-Fi networks
		*ssid,			// Current SSID
		*next;			// Next SSID

    next = networks;
    while ((ssid = strsep(&next, ",")) != NULL && num_wifi < max_wifi)
    {
      if (*ssid == '*')
      {
        wifis[num_wifi].is_secure = true;
        ssid ++;
      }

      strlcpy(wifis[num_wifi ++].ssid, ssid, sizeof(wifis[0].ssid));
    }

    free(networks);
  }
  else if (!access("/sbin/iwlist", X_OK))
  {
    // Scan for Wi-Fi networks...
    FILE	*iwlist = popen("/sbin/iwlist scanning", "w");
					// iwlist command output
    int		i;			// Looping var
    char	line[1024],		// Line from iwlist command
		*essid,			// SSID
		*end;			// End of value

    while (fgets(line, sizeof(line), iwlist))
    {
      if ((essid = strstr(line, "ESSID:\"")) != NULL)
      {
	if (num_wifi >= max_wifi)
	  break;

        essid += 7;
        if ((end = strchr(essid, '\"')) != NULL)
	{
	  *end = '\0';
	  for (i = 0; i < num_wifi; i ++)
	  {
	    if (!strcmp(wifis[i].ssid, essid))
	      break;
	  }

	  if (i >= num_wifi && *essid)
	    strlcpy(wifis[num_wifi ++].ssid, essid, sizeof(wifis[0].ssid));
	}
      }
      else if (strstr(line, " : PSK") && num_wifi > 0)
        wifis[num_wifi - 1].is_secure = true;
    }

    pclose(iwlist);
  }

  return (num_wifi);
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
  if (!papplClientRespondHTTP(client, HTTP_STATUS_OK, NULL, "text/html", 0, 0))
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


#if 0
//
// 'device_cb()' - Device callback.
//

static int				// O - 1 to continue
device_cb(const char      *device_uri,	// I - Device URI
          pappl_client_t *client)	// I - Client
{
  char	scheme[32],			// URI scheme
	userpass[32],			// Username/password (unused)
	make[64],			// Make from URI
	model[256],			// Model from URI
	*serial;			// Pointer to serial number
  int	port;				// Port number (unused)


  if (httpSeparateURI(HTTP_URI_CODING_ALL, device_uri, scheme, sizeof(scheme), userpass, sizeof(userpass), make, sizeof(make), &port, model, sizeof(model)) >= HTTP_URI_STATUS_OK)
  {
    if ((serial = strstr(model, "?serial=")) != NULL)
    {
      *serial = '\0';
      serial += 8;
    }

    if (serial)
      papplClientHTMLPrintf(client, "<option value=\"%s\">%s %s (%s)</option>", device_uri, make, model + 1, serial);
    else
      papplClientHTMLPrintf(client, "<option value=\"%s\">%s %s</option>", device_uri, make, model + 1);
  }

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
