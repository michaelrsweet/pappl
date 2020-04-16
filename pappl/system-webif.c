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
      	ipv6_router[40],		// IPv4 router
      	dns_address1[16],		// DNS address #1
      	dns_address2[16];		// DNS address #2
} _pappl_netif_t;

typedef struct _pappl_wifi_s		// Wi-Fi network information
{
  char	ssid[128];			// Wi-Fi ID
  bool	is_secure;			// Needs a password?
} _pappl_wifi_t;


//
// Local functions...
//

static int	get_network(char *hostname, size_t hostsize, int max_netifs, _pappl_netif_t *netifs);
static bool	set_network(const char *hostname, int num_netifs, _pappl_netif_t *netifs);
static int	get_wifi_networks(int max_wifi, _pappl_wifi_t *wifis);

static void	system_footer(pappl_client_t *client);
static void	system_header(pappl_client_t *client, const char *title);


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
  system_header(client, NULL);

  papplClientHTMLPrintf(client,
			"      <div class=\"row\">\n"
			"        <div class=\"col-6\">\n"
			"          <h1 class=\"title\">Configuration <a class=\"btn\" href=\"https://%s:%d/config\">Change</a></h1>\n", client->host_field, client->host_port);

  _papplClientHTMLInfo(client, false, system->dns_sd_name, system->location, system->geo_location, system->organization, system->org_unit, &system->contact);

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
  int		i,			// Looping var
		num_netifs;		// Number of network interfaces
  _pappl_netif_t netifs[100],		// Network interfaces
		*netif;			// Current network interface
  char		hostname[256];		// Hostname, if any
  const char	*status = NULL;		// Status message, if any
  static const char	*ipv4_address_pattern = "^(25[0-9]|2[0-4][0-9]|1[0-9]{2}|[1-9][0-9]|[0-9])\\.(25[0-9]|2[0-4][0-9]|1[0-9]{2}|[1-9][0-9]|[0-9])\\.(25[0-9]|2[0-4][0-9]|1[0-9]{2}|[1-9][0-9]|[0-9])\\.(25[0-9]|2[0-4][0-9]|1[0-9]{2}|[1-9][0-9]|[1-9])$";
  static const char	*ipv4_netmask_pattern = "^((128|192|224|240|248|252|254|255)\\.0\\.0\\.0|255\\.(128|192|224|240|248|252|254|255)\\.0\\.0|255\\.255\\.(128|192|224|240|248|252|254|255)\\.0|255\\.255\\.255\\.(128|192|224|240|248|252|254))$";


  num_netifs = get_network(hostname, sizeof(hostname), (int)(sizeof(netifs) / sizeof(netifs[0])), netifs);

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
        strlcpy(hostname, value, sizeof(hostname));

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

        if ((value = cupsGetOption("dns_address1", num_form, form)) != NULL)
          strlcpy(netif->dns_address1, value, sizeof(netif->dns_address1));
        if ((value = cupsGetOption("dns_address2", num_form, form)) != NULL)
          strlcpy(netif->dns_address2, value, sizeof(netif->dns_address2));

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

      if (!set_network(hostname, num_netifs, netifs))
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
  papplClientHTMLStartForm(client, client->uri);
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
			"        </div>\n", hostname, netifs[0].dns_address1, ipv4_address_pattern, netifs[0].dns_address2, ipv4_address_pattern);

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
// '_papplSystemWebSecurity()' - Show the system security (users/TLS) management page.
//

void
_papplSystemWebSecurity(
    pappl_client_t *client,		// I - Client
    pappl_system_t *system)		// I - System
{
  const char	*status = NULL;		// Status message, if any
  struct group	*grp;			// Current group


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
      status = "???";
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

  if (client->operation != HTTP_STATE_POST && client->options && !strcmp(client->options, "installcrt"))
  {
    papplClientHTMLStartForm(client, "/security?installcrt");
    papplClientHTMLPuts(client,
                        "        <div class=\"col-12\">\n"
                        "          <h2 class=\"title\">Install Certificate</h2>\n"
                        "          <p>This form will install a trusted TLS certificate you have obtained from a Certificate Authority ('CA'). Once installed it will be used immediately.</p>\n"
			"          <table class=\"form\">\n"
			"            <tbody>\n"
			"              <tr><th><label for=\"certificate\">Certificate:</label></th><td><textarea name=\"certificate\" rows=\"16\"></textarea><br>(PEM-encoded)</td></tr>\n"
			"              <tr><th><label for=\"privatekey\">Private Key:</label></th><td><textarea name=\"privatekey\" rows=\"8\"></textarea><br> (PEM-encoded, leave blank to use the key from the last signing request)</td></tr>\n"
			"              <tr><th></th><td><input type=\"submit\" value=\"Install Certificate\"></td></tr>\n"
			"            </tbody>\n"
			"          </table>\n"
			"        </div>\n"
			"        </form>\n");
  }
  else if (client->operation != HTTP_STATE_POST && client->options && (!strcmp(client->options, "newcrt") || !strcmp(client->options, "newcsr")))
  {
    int		i;			// Looping var
    char	path[1024];		// Form submission path
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

    snprintf(path, sizeof(path), "/security?%s", client->options);
    papplClientHTMLStartForm(client, path);

    if (!strcmp(client->options, "newcrt"))
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

    if (!strcmp(client->options, "newcrt"))
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
			"        </script>\n");
  }
  else
  {
    if ((system->options & PAPPL_SOPTIONS_USERS) && system->auth_service)
    {
      // Show Users pane for group controls
      papplClientHTMLStartForm(client, "/security?users");

      papplClientHTMLPuts(client,
			  "        <div class=\"col-6\">\n"
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
      papplClientHTMLStartForm(client, "/security?password");

      papplClientHTMLPuts(client,
			  "        <div class=\"col-6\">\n"
			  "          <h2 class=\"title\">Administration Password</h2>\n"
			  "          <table class=\"form\">\n"
			  "            <tbody>\n"
			  "              <tr><th><label for=\"old_password\">Current Password:</label></th><td><input type=\"password\" name=\"old_password\"></td></tr>\n"
			  "              <tr><th><label for=\"new_password\">New Password:</label></th><td><input type=\"password\" name=\"new_password\"></td></tr>\n"
			  "              <tr><th><label for=\"new_password2\">New Password (again):</label></th><td><input type=\"password\" name=\"new_password2\"></td></tr>\n"
			  "              <tr><th></th><td><input type=\"submit\" value=\"Change Password\"></td></tr>\n"
			  "            </tbody>\n"
			  "          </table>\n"
			  "        </div>\n"
			  "        </form>\n");

    }
    else
    {
      // Show simple access password initial setting form...
      papplClientHTMLStartForm(client, "/security?password");

      papplClientHTMLPuts(client,
			  "        <div class=\"col-6\">\n"
			  "          <h2 class=\"title\">Administration Password</h2>\n"
			  "          <table class=\"form\">\n"
			  "            <tbody>\n"
			  "              <tr><th><label for=\"new_password\">Password:</label></th><td><input type=\"password\" name=\"new_password\"></td></tr>\n"
			  "              <tr><th><label for=\"new_password2\">Password (again):</label></th><td><input type=\"password\" name=\"new_password2\"></td></tr>\n"
			  "              <tr><th></th><td><input type=\"submit\" value=\"Set Password\"></td></tr>\n"
			  "            </tbody>\n"
			  "          </table>\n"
			  "        </div>\n"
			  "        </form>\n");
    }

    // TLS certificates
    papplClientHTMLPuts(client,
			"        <div class=\"col-6\">\n"
			"          <h2 class=\"title\">TLS Certificates</h2>\n"
			"          <p><a class=\"btn\" href=\"/security?newcrt\">Create New Certificate</a>"
			" <a class=\"btn\" href=\"/security?newcsr\">Create Certificate Signing Request</a>"
			" <a class=\"btn\" href=\"/security?installcrt\">Install CA Certificate</a></p>\n");

    char certinfo[1024] = "TODO LOAD CERTIFICATE INFORMATION\n"
			  "Issuer: C = GB, ST = Greater Manchester, L = Salford, O = COMODO CA Limited, CN = COMODO RSA Domain Validation Secure Server CA\n"
			  "Validity\n"
			  "    Not Before: Mar 12 00:00:00 2018 GMT\n"
			  "    Not After : Mar 11 23:59:59 2020 GMT\n";

    papplClientHTMLPrintf(client,
			  "          <p>Current certificate:</p>\n"
			  "          <pre style=\"white-space: pre-wrap;\">%s</pre>\n", certinfo);

    papplClientHTMLPuts(client,
			"            </tbody>\n"
			"          </table>\n"
			"        </div>\n");
  }

  // Finish up...
  papplClientHTMLPuts(client,
                      "      </div>\n");

  system_footer(client);
}


//
// 'get_network()' - Load the network configuration.
//
// Note: Currently only supports Linux /etc/network/interfaces file.
//

static int				// O - Number of network interfaces
get_network(char           *hostname,	// O - Hostname
            size_t         hostsize,	// I - Size of hostname buffer
            int            max_netifs,	// I - Maximum number of interfaces
	    _pappl_netif_t *netifs)	// O - Network interfaces
{
  cups_file_t	*fp;			// "/etc/network/interfaces" file
  char		line[1024],		// Line from file
		*value;			// Value on line
  int		linenum = 0;		// Line number in file
  int		num_netifs = 0;		// Number of network interfaces
  _pappl_netif_t *netif = NULL;		// Current network interface


  memset(netifs, 0, (size_t)max_netifs * sizeof(_pappl_netif_t));

  httpGetHostname(NULL, hostname, (int)hostsize);

  if (getenv("PAPPL_NETWORK"))
  {
    // Copy list of network interfaces from the PAPPL_NETWORK environment
    // variable, of the form:
    //
    //   PAPPL_NETWORK="name[,ipv4-address] name[,ipv4-address][,SSID] ..."
    char	*names = strdup(getenv("PAPPL_NETWORK")),
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
  else if ((fp = cupsFileOpen("/etc/network/interfaces", "r")) != NULL)
  {
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
      // TODO: Implement wpa_supplicant stuff
    }

    cupsFileClose(fp);
  }

  return (num_netifs);
}


//
// 'set_network()' - Save the network configuration.
//

static bool				// O - `true` on success, `false` on failure
set_network(const char     *hostname,	// I - Hostname
            int            num_netifs,	// I - Number of network interfaces
            _pappl_netif_t *netifs)	// I - Network interfaces
{
  // TODO: Implement me
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
  memset(wifis, 0, (size_t)max_wifi * sizeof(_pappl_wifi_t));

  strlcpy(wifis[0].ssid, "TestPublic", sizeof(wifis[0].ssid));

  strlcpy(wifis[1].ssid, "TestPrivate", sizeof(wifis[1].ssid));
  wifis[1].is_secure = true;

  return (2);
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


//
// 'show_status()' - Show printer/system state.
//

static int				// O - 1 on success, 0 on failure
show_status(pappl_client_t  *client)	// I - Client connection
{
  pappl_system_t	*system = client->system;
					// System
  pappl_printer_t	*printer;	// Printer
  pappl_job_t		*job;		// Current job
  int			i;		// Looping var
  pappl_preason_t	reason;		// Current reason
  static const char * const reasons[] =	// Reason strings
  {
    "Other",
    "Cover Open",
    "Media Empty",
    "Media Jam",
    "Media Low",
    "Media Needed"
  };
  static const char * const state_colors[] =
  {					// State colors
    "rgba(0,192,0,0.5)",		// Idle
    "rgba(224,224,0,0.5)",		// Processing
    "rgba(192,0,0,0.5)"			// Stopped
  };


  if (!papplClientRespondHTTP(client, HTTP_STATUS_OK, NULL, "text/html", 0))
    return (0);

  pthread_rwlock_rdlock(&system->rwlock);

  for (printer = (pappl_printer_t *)cupsArrayFirst(system->printers); printer; printer = (pappl_printer_t *)cupsArrayNext(system->printers))
  {
    if (printer->state == IPP_PSTATE_PROCESSING)
      break;
  }

  papplClientHTMLHeader(client, "Printers", printer ? 5 : 15);

  if (client->system->auth_service)
    papplClientHTMLPrintf(client, "<p><button onclick=\"window.location.href='/add';\">Add Printer</button></p>\n");

  for (printer = (pappl_printer_t *)cupsArrayFirst(system->printers); printer; printer = (pappl_printer_t *)cupsArrayNext(system->printers))
  {
    papplClientHTMLPrintf(client, "<h2 class=\"title\">%s%s</h2>\n"
                        "<p><img style=\"background: %s; border-radius: 10px; float: left; margin-right: 10px; padding: 5px;\" src=\"/lprint-large.png\" width=\"64\" height=\"64\">%s", printer->printer_name, printer->printer_id == client->system->default_printer ? " (Default)" : "", state_colors[printer->state - IPP_PSTATE_IDLE], lprintGetMakeAndModel(printer->driver_name));
    if (printer->location)
      papplClientHTMLPrintf(client, ", %s", printer->location);
    if (printer->organization)
      papplClientHTMLPrintf(client, "<br>\n%s%s%s", printer->organization, printer->org_unit ? ", " : "", printer->org_unit ? printer->org_unit : "");
    papplClientHTMLPrintf(client, "<br>\n"
                        "%s, %d job(s)", printer->state == IPP_PSTATE_IDLE ? "Idle" : printer->state == IPP_PSTATE_PROCESSING ? "Printing" : "Stopped", cupsArrayCount(printer->jobs));
    for (i = 0, reason = 1; i < (int)(sizeof(reasons) / sizeof(reasons[0])); i ++, reason <<= 1)
    {
      if (printer->state_reasons & reason)
	papplClientHTMLPrintf(client, ",%s", reasons[i]);
    }
    papplClientHTMLPrintf(client, ".</p>\n");

    if (client->system->auth_service)
    {
      papplClientHTMLPrintf(client, "<p><button onclick=\"window.location.href='/modify/%d';\">Modify</button> <button onclick=\"window.location.href='/delete/%d';\">Delete</button>", printer->printer_id, printer->printer_id);
      if (printer->printer_id != client->system->default_printer)
        papplClientHTMLPrintf(client, " <button onclick=\"window.location.href='/default/%d';\">Set As Default</button>", printer->printer_id);
      papplClientHTMLPrintf(client, "</p>\n");
    }

    if (cupsArrayCount(printer->jobs) > 0)
    {
      pthread_rwlock_rdlock(&printer->rwlock);

      papplClientHTMLPrintf(client, "<table class=\"striped\" summary=\"Jobs\"><thead><tr><th>Job #</th><th>Name</th><th>Owner</th><th>Status</th></tr></thead><tbody>\n");
      for (job = (pappl_job_t *)cupsArrayFirst(printer->jobs); job; job = (pappl_job_t *)cupsArrayNext(printer->jobs))
      {
	char	when[256],		// When job queued/started/finished
		hhmmss[64];		// Time HH:MM:SS

	switch (job->state)
	{
	  case IPP_JSTATE_PENDING :
	  case IPP_JSTATE_HELD :
	      snprintf(when, sizeof(when), "Queued at %s", time_string(job->created, hhmmss, sizeof(hhmmss)));
	      break;
	  case IPP_JSTATE_PROCESSING :
	  case IPP_JSTATE_STOPPED :
	      snprintf(when, sizeof(when), "Started at %s", time_string(job->processing, hhmmss, sizeof(hhmmss)));
	      break;
	  case IPP_JSTATE_ABORTED :
	      snprintf(when, sizeof(when), "Aborted at %s", time_string(job->completed, hhmmss, sizeof(hhmmss)));
	      break;
	  case IPP_JSTATE_CANCELED :
	      snprintf(when, sizeof(when), "Canceled at %s", time_string(job->completed, hhmmss, sizeof(hhmmss)));
	      break;
	  case IPP_JSTATE_COMPLETED :
	      snprintf(when, sizeof(when), "Completed at %s", time_string(job->completed, hhmmss, sizeof(hhmmss)));
	      break;
	}

	papplClientHTMLPrintf(client, "<tr><td>%d</td><td>%s</td><td>%s</td><td>%s</td></tr>\n", job->id, job->name, job->username, when);
      }
      papplClientHTMLPrintf(client, "</tbody></table>\n");

      pthread_rwlock_unlock(&printer->rwlock);
    }
  }

  papplClientHTMLFooter(client);

  pthread_rwlock_unlock(&system->rwlock);

  return (1);
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
#endif // 0
