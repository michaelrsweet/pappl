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


//
// Local functions...
//

static bool	install_certificate(const char *crtfile, const char *keyfile);
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
  system_header(client, NULL);

  papplClientHTMLPrintf(client,
			"      <div class=\"row\">\n"
			"        <div class=\"col-6\">\n"
			"          <h1 class=\"title\">Configuration <a class=\"btn\" href=\"https://%s:%d/config\">Change</a></h1>\n", client->host_field, client->host_port);

  _papplClientHTMLInfo(client, false, system->dns_sd_name, system->location, system->geo_location, system->organization, system->org_unit, &system->contact);

  _papplSystemWebSettings(client);

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
    else if (!papplClientValidateForm(client, num_form, form))
    {
      status = "Invalid form submission.";
    }
    else
    {
      const char *value;		// Form variable value

      if ((value = cupsGetOption("hostname", num_form, form)) != NULL)
      {
        // Set hostname and save it...
	// TODO: Add papplSystemSetHostname function
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

      if (!strcmp(addr->ifa_name, "wlan0"))
        papplClientHTMLPrintf(client, "Wi-Fi: %s<br>", tempptr);
      else if (!strncmp(addr->ifa_name, "wlan", 4) && isdigit(addr->ifa_name[4]))
        papplClientHTMLPrintf(client, "Wi-Fi %d: %s<br>", atoi(addr->ifa_name + 4) + 1, tempptr);
      else if (!strcmp(addr->ifa_name, "en0") || !strcmp(addr->ifa_name, "eth0"))
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

      if (!strcmp(addr->ifa_name, "wlan0"))
        papplClientHTMLPrintf(client, "Wi-Fi: %s<br>", tempptr);
      else if (!strncmp(addr->ifa_name, "wlan", 4) && isdigit(addr->ifa_name[4]))
        papplClientHTMLPrintf(client, "Wi-Fi %d: %s<br>", atoi(addr->ifa_name + 4) + 1, tempptr);
      else if (!strcmp(addr->ifa_name, "en0") || !strcmp(addr->ifa_name, "eth0"))
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
// '_papplSystemWebSettings()' - Show the system settings panel, as needed.
//

void
_papplSystemWebSettings(
    pappl_client_t *client)		// I - Client
{
  if (client->system->options & (PAPPL_SOPTIONS_NETWORK | PAPPL_SOPTIONS_SECURITY | PAPPL_SOPTIONS_TLS))
  {
    papplClientHTMLPuts(client,
                        "          <h2 class=\"title\">Other Settings</h2>\n"
                        "          <div class=\"btn\">");
    if (client->system->options & PAPPL_SOPTIONS_NETWORK)
      papplClientHTMLPrintf(client, "<a class=\"btn\" href=\"https://%s:%d/network\">Network</a> ", client->host_field, client->host_port);
    if (client->system->options & PAPPL_SOPTIONS_SECURITY)
      papplClientHTMLPrintf(client, "<a class=\"btn\" href=\"https://%s:%d/security\">Security</a> ", client->host_field, client->host_port);
    if (client->system->options & PAPPL_SOPTIONS_TLS)
      papplClientHTMLPrintf(client,
                            "<a class=\"btn\" href=\"https://%s:%d/tls-install-crt\">Install TLS Certificate</a> "
                            "<a class=\"btn\" href=\"https://%s:%d/tls-new-crt\">Create New TLS Certificate</a> "
                            "<a class=\"btn\" href=\"https://%s:%d/tls-new-csr\">Create TLS Certificate Request</a> ", client->host_field, client->host_port, client->host_field, client->host_port, client->host_field, client->host_port);
    papplClientHTMLPuts(client, "</div>\n");
  }

  if ((client->system->options & PAPPL_SOPTIONS_LOG) && client->system->logfile && strcmp(client->system->logfile, "-") && strcmp(client->system->logfile, "syslog"))
    papplClientHTMLPuts(client,
                        "          <h2 class=\"title\">Logging</h2>\n"
                        "          <div class=\"btn\"><a class=\"btn\" href=\"/system.log\">View Log File</a></div>\n");
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
      const char	*crtfile,	// Certificate file
			*keyfile;	// Private key file
      char		filename[1024];	// Filename

      crtfile = cupsGetOption("certificate", num_form, form);
      keyfile = cupsGetOption("privatekey", num_form, form);

      if (!keyfile)
      {
        snprintf(filename, sizeof(filename), "%s/request.key", client->system->directory);
        if (!access(filename, R_OK))
          keyfile = filename;
	else
	  status = "Missing private key.";
      }

      if (!status)
      {
        if (install_certificate(crtfile, keyfile))
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

  if (!strcmp(client->uri, "/tls-new-crt"))
    system_header(client, "Create New TLS Certificate");
  else
    system_header(client, "Create TLS Certificate Request");

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
// 'install_certificate()' - Install a certificate and private key.
//

static bool				// O - `true` on success, `false` otherwise
install_certificate(
    const char *crtfile,		// I - PEM-encoded certificate filename
    const char *keyfile)		// I - PEM-encoded private key filename
{
  // TODO: Try loading cert and key, then copy to /etc/cups/ssl.
  (void)crtfile;
  (void)keyfile;

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
