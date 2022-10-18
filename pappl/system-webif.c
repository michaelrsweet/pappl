//
// System web interface functions for the Printer Application Framework
//
// Copyright © 2019-2022 by Michael R Sweet.
// Copyright © 2010-2019 by Apple Inc.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

//
// Include necessary headers...
//

#include "pappl-private.h"
#if !_WIN32
#  include <net/if.h>
#  include <ifaddrs.h>
#endif // !_WIN32
#ifdef HAVE_OPENSSL
#  include <openssl/bn.h>
#  include <openssl/err.h>
#  include <openssl/ssl.h>
#  include <openssl/x509.h>
#  include <openssl/x509v3.h>
#elif defined(HAVE_GNUTLS)
#  include <gnutls/gnutls.h>
#  include <gnutls/x509.h>
#endif // HAVE_OPENSSL


//
// Local types...
//

typedef struct _pappl_system_dev_s	// System device callback data
{
  pappl_client_t	*client;	// Client connection
  const char		*device_uri;	// Current device URI
} _pappl_system_dev_t;


//
// Local functions...
//

static size_t	get_networks(size_t max_networks, pappl_network_t *networks);
static bool	system_device_cb(const char *device_info, const char *device_uri, const char *device_id, void *data);
static void	system_footer(pappl_client_t *client);
static void	system_header(pappl_client_t *client, const char *title);

#if defined(HAVE_OPENSSL) || defined(HAVE_GNUTLS)
static bool	tls_install_certificate(pappl_client_t *client, const char *crtfile, const char *keyfile);
static bool	tls_install_file(pappl_client_t *client, const char *dst, const char *src);
static bool	tls_make_certificate(pappl_client_t *client, cups_len_t num_form, cups_option_t *form);
static bool	tls_make_certsignreq(pappl_client_t *client, cups_len_t num_form, cups_option_t *form, char *crqpath, size_t crqsize);
#endif // HAVE_OPENSSL || HAVE_GNUTLS


//
// Local globals...
//

#if defined(HAVE_OPENSSL) || defined(HAVE_GNUTLS)
static const char * const countries[][2] =
{					// List of countries and their ISO 3166 2-letter codes
  { "AF", _PAPPL_LOC("Afghanistan") },
  { "AX", _PAPPL_LOC("Åland Islands") },
  { "AL", _PAPPL_LOC("Albania") },
  { "DZ", _PAPPL_LOC("Algeria") },
  { "AS", _PAPPL_LOC("American Samoa") },
  { "AD", _PAPPL_LOC("Andorra") },
  { "AO", _PAPPL_LOC("Angola") },
  { "AI", _PAPPL_LOC("Anguilla") },
  { "AQ", _PAPPL_LOC("Antarctica") },
  { "AG", _PAPPL_LOC("Antigua and Barbuda") },
  { "AR", _PAPPL_LOC("Argentina") },
  { "AM", _PAPPL_LOC("Armenia") },
  { "AW", _PAPPL_LOC("Aruba") },
  { "AU", _PAPPL_LOC("Australia") },
  { "AT", _PAPPL_LOC("Austria") },
  { "AZ", _PAPPL_LOC("Azerbaijan") },
  { "BS", _PAPPL_LOC("Bahamas") },
  { "BH", _PAPPL_LOC("Bahrain") },
  { "BD", _PAPPL_LOC("Bangladesh") },
  { "BB", _PAPPL_LOC("Barbados") },
  { "BY", _PAPPL_LOC("Belarus") },
  { "BE", _PAPPL_LOC("Belgium") },
  { "BZ", _PAPPL_LOC("Belize") },
  { "BJ", _PAPPL_LOC("Benin") },
  { "BM", _PAPPL_LOC("Bermuda") },
  { "BT", _PAPPL_LOC("Bhutan") },
  { "BO", _PAPPL_LOC("Bolivia (Plurinational State of)") },
  { "BQ", _PAPPL_LOC("Bonaire, Sint Eustatius and Saba") },
  { "BA", _PAPPL_LOC("Bosnia and Herzegovina") },
  { "BW", _PAPPL_LOC("Botswana") },
  { "BV", _PAPPL_LOC("Bouvet Island") },
  { "BR", _PAPPL_LOC("Brazil") },
  { "IO", _PAPPL_LOC("British Indian Ocean Territory") },
  { "BN", _PAPPL_LOC("Brunei Darussalam") },
  { "BG", _PAPPL_LOC("Bulgaria") },
  { "BF", _PAPPL_LOC("Burkina Faso") },
  { "BI", _PAPPL_LOC("Burundi") },
  { "CV", _PAPPL_LOC("Cabo Verde") },
  { "KH", _PAPPL_LOC("Cambodia") },
  { "CM", _PAPPL_LOC("Cameroon") },
  { "CA", _PAPPL_LOC("Canada") },
  { "KY", _PAPPL_LOC("Cayman Islands") },
  { "CF", _PAPPL_LOC("Central African Republic") },
  { "TD", _PAPPL_LOC("Chad") },
  { "CL", _PAPPL_LOC("Chile") },
  { "CN", _PAPPL_LOC("China") },
  { "CX", _PAPPL_LOC("Christmas Island") },
  { "CC", _PAPPL_LOC("Cocos (Keeling) Islands") },
  { "CO", _PAPPL_LOC("Colombia") },
  { "KM", _PAPPL_LOC("Comoros") },
  { "CD", _PAPPL_LOC("Congo, Democratic Republic of the") },
  { "CG", _PAPPL_LOC("Congo") },
  { "CK", _PAPPL_LOC("Cook Islands") },
  { "CR", _PAPPL_LOC("Costa Rica") },
  { "CI", _PAPPL_LOC("Côte d'Ivoire") },
  { "HR", _PAPPL_LOC("Croatia") },
  { "CU", _PAPPL_LOC("Cuba") },
  { "CW", _PAPPL_LOC("Curaçao") },
  { "CY", _PAPPL_LOC("Cyprus") },
  { "CZ", _PAPPL_LOC("Czechia") },
  { "DK", _PAPPL_LOC("Denmark") },
  { "DJ", _PAPPL_LOC("Djibouti") },
  { "DM", _PAPPL_LOC("Dominica") },
  { "DO", _PAPPL_LOC("Dominican Republic") },
  { "EC", _PAPPL_LOC("Ecuador") },
  { "EG", _PAPPL_LOC("Egypt") },
  { "SV", _PAPPL_LOC("El Salvador") },
  { "GQ", _PAPPL_LOC("Equatorial Guinea") },
  { "ER", _PAPPL_LOC("Eritrea") },
  { "EE", _PAPPL_LOC("Estonia") },
  { "SZ", _PAPPL_LOC("Eswatini") },
  { "ET", _PAPPL_LOC("Ethiopia") },
  { "FK", _PAPPL_LOC("Falkland Islands (Malvinas)") },
  { "FO", _PAPPL_LOC("Faroe Islands") },
  { "FJ", _PAPPL_LOC("Fiji") },
  { "FI", _PAPPL_LOC("Finland") },
  { "FR", _PAPPL_LOC("France") },
  { "GF", _PAPPL_LOC("French Guiana") },
  { "PF", _PAPPL_LOC("French Polynesia") },
  { "TF", _PAPPL_LOC("French Southern Territories") },
  { "GA", _PAPPL_LOC("Gabon") },
  { "GM", _PAPPL_LOC("Gambia") },
  { "GE", _PAPPL_LOC("Georgia") },
  { "DE", _PAPPL_LOC("Germany") },
  { "GH", _PAPPL_LOC("Ghana") },
  { "GI", _PAPPL_LOC("Gibraltar") },
  { "GR", _PAPPL_LOC("Greece") },
  { "GL", _PAPPL_LOC("Greenland") },
  { "GD", _PAPPL_LOC("Grenada") },
  { "GP", _PAPPL_LOC("Guadeloupe") },
  { "GU", _PAPPL_LOC("Guam") },
  { "GT", _PAPPL_LOC("Guatemala") },
  { "GG", _PAPPL_LOC("Guernsey") },
  { "GW", _PAPPL_LOC("Guinea-Bissau") },
  { "GN", _PAPPL_LOC("Guinea") },
  { "GY", _PAPPL_LOC("Guyana") },
  { "HT", _PAPPL_LOC("Haiti") },
  { "HM", _PAPPL_LOC("Heard Island and McDonald Islands") },
  { "VA", _PAPPL_LOC("Holy See") },
  { "HN", _PAPPL_LOC("Honduras") },
  { "HK", _PAPPL_LOC("Hong Kong") },
  { "HU", _PAPPL_LOC("Hungary") },
  { "IS", _PAPPL_LOC("Iceland") },
  { "IN", _PAPPL_LOC("India") },
  { "ID", _PAPPL_LOC("Indonesia") },
  { "IR", _PAPPL_LOC("Iran (Islamic Republic of)") },
  { "IQ", _PAPPL_LOC("Iraq") },
  { "IE", _PAPPL_LOC("Ireland") },
  { "IM", _PAPPL_LOC("Isle of Man") },
  { "IL", _PAPPL_LOC("Israel") },
  { "IT", _PAPPL_LOC("Italy") },
  { "JM", _PAPPL_LOC("Jamaica") },
  { "JP", _PAPPL_LOC("Japan") },
  { "JE", _PAPPL_LOC("Jersey") },
  { "JO", _PAPPL_LOC("Jordan") },
  { "KZ", _PAPPL_LOC("Kazakhstan") },
  { "KE", _PAPPL_LOC("Kenya") },
  { "KI", _PAPPL_LOC("Kiribati") },
  { "KP", _PAPPL_LOC("Korea (Democratic People's Republic of)") },
  { "KR", _PAPPL_LOC("Korea, Republic of") },
  { "KW", _PAPPL_LOC("Kuwait") },
  { "KG", _PAPPL_LOC("Kyrgyzstan") },
  { "LA", _PAPPL_LOC("Lao People's Democratic Republic") },
  { "LV", _PAPPL_LOC("Latvia") },
  { "LB", _PAPPL_LOC("Lebanon") },
  { "LS", _PAPPL_LOC("Lesotho") },
  { "LR", _PAPPL_LOC("Liberia") },
  { "LY", _PAPPL_LOC("Libya") },
  { "LI", _PAPPL_LOC("Liechtenstein") },
  { "LT", _PAPPL_LOC("Lithuania") },
  { "LU", _PAPPL_LOC("Luxembourg") },
  { "MO", _PAPPL_LOC("Macao") },
  { "MG", _PAPPL_LOC("Madagascar") },
  { "MW", _PAPPL_LOC("Malawi") },
  { "MY", _PAPPL_LOC("Malaysia") },
  { "MV", _PAPPL_LOC("Maldives") },
  { "ML", _PAPPL_LOC("Mali") },
  { "MT", _PAPPL_LOC("Malta") },
  { "MH", _PAPPL_LOC("Marshall Islands") },
  { "MQ", _PAPPL_LOC("Martinique") },
  { "MR", _PAPPL_LOC("Mauritania") },
  { "MU", _PAPPL_LOC("Mauritius") },
  { "YT", _PAPPL_LOC("Mayotte") },
  { "MX", _PAPPL_LOC("Mexico") },
  { "FM", _PAPPL_LOC("Micronesia (Federated States of)") },
  { "MD", _PAPPL_LOC("Moldova, Republic of") },
  { "MC", _PAPPL_LOC("Monaco") },
  { "MN", _PAPPL_LOC("Mongolia") },
  { "ME", _PAPPL_LOC("Montenegro") },
  { "MS", _PAPPL_LOC("Montserrat") },
  { "MA", _PAPPL_LOC("Morocco") },
  { "MZ", _PAPPL_LOC("Mozambique") },
  { "MM", _PAPPL_LOC("Myanmar") },
  { "NA", _PAPPL_LOC("Namibia") },
  { "NR", _PAPPL_LOC("Nauru") },
  { "NP", _PAPPL_LOC("Nepal") },
  { "NL", _PAPPL_LOC("Netherlands") },
  { "NC", _PAPPL_LOC("New Caledonia") },
  { "NZ", _PAPPL_LOC("New Zealand") },
  { "NI", _PAPPL_LOC("Nicaragua") },
  { "NE", _PAPPL_LOC("Niger") },
  { "NG", _PAPPL_LOC("Nigeria") },
  { "NU", _PAPPL_LOC("Niue") },
  { "NF", _PAPPL_LOC("Norfolk Island") },
  { "MK", _PAPPL_LOC("North Macedonia") },
  { "MP", _PAPPL_LOC("Northern Mariana Islands") },
  { "NO", _PAPPL_LOC("Norway") },
  { "OM", _PAPPL_LOC("Oman") },
  { "PK", _PAPPL_LOC("Pakistan") },
  { "PW", _PAPPL_LOC("Palau") },
  { "PS", _PAPPL_LOC("Palestine, State of") },
  { "PA", _PAPPL_LOC("Panama") },
  { "PG", _PAPPL_LOC("Papua New Guinea") },
  { "PY", _PAPPL_LOC("Paraguay") },
  { "PE", _PAPPL_LOC("Peru") },
  { "PH", _PAPPL_LOC("Philippines") },
  { "PN", _PAPPL_LOC("Pitcairn") },
  { "PL", _PAPPL_LOC("Poland") },
  { "PT", _PAPPL_LOC("Portugal") },
  { "PR", _PAPPL_LOC("Puerto Rico") },
  { "QA", _PAPPL_LOC("Qatar") },
  { "RE", _PAPPL_LOC("Réunion") },
  { "RO", _PAPPL_LOC("Romania") },
  { "RU", _PAPPL_LOC("Russian Federation") },
  { "RW", _PAPPL_LOC("Rwanda") },
  { "BL", _PAPPL_LOC("Saint Barthélemy") },
  { "SH", _PAPPL_LOC("Saint Helena, Ascension and Tristan da Cunha") },
  { "KN", _PAPPL_LOC("Saint Kitts and Nevis") },
  { "LC", _PAPPL_LOC("Saint Lucia") },
  { "MF", _PAPPL_LOC("Saint Martin (French part)") },
  { "PM", _PAPPL_LOC("Saint Pierre and Miquelon") },
  { "VC", _PAPPL_LOC("Saint Vincent and the Grenadines") },
  { "WS", _PAPPL_LOC("Samoa") },
  { "SM", _PAPPL_LOC("San Marino") },
  { "ST", _PAPPL_LOC("Sao Tome and Principe") },
  { "SA", _PAPPL_LOC("Saudi Arabia") },
  { "SN", _PAPPL_LOC("Senegal") },
  { "RS", _PAPPL_LOC("Serbia") },
  { "SC", _PAPPL_LOC("Seychelles") },
  { "SL", _PAPPL_LOC("Sierra Leone") },
  { "SG", _PAPPL_LOC("Singapore") },
  { "SX", _PAPPL_LOC("Sint Maarten (Dutch part)") },
  { "SK", _PAPPL_LOC("Slovakia") },
  { "SI", _PAPPL_LOC("Slovenia") },
  { "SB", _PAPPL_LOC("Solomon Islands") },
  { "SO", _PAPPL_LOC("Somalia") },
  { "ZA", _PAPPL_LOC("South Africa") },
  { "GS", _PAPPL_LOC("South Georgia and the South Sandwich Islands") },
  { "SS", _PAPPL_LOC("South Sudan") },
  { "ES", _PAPPL_LOC("Spain") },
  { "LK", _PAPPL_LOC("Sri Lanka") },
  { "SD", _PAPPL_LOC("Sudan") },
  { "SR", _PAPPL_LOC("Suriname") },
  { "SJ", _PAPPL_LOC("Svalbard and Jan Mayen") },
  { "SE", _PAPPL_LOC("Sweden") },
  { "CH", _PAPPL_LOC("Switzerland") },
  { "SY", _PAPPL_LOC("Syrian Arab Republic") },
  { "TW", _PAPPL_LOC("Taiwan, Province of China") },
  { "TJ", _PAPPL_LOC("Tajikistan") },
  { "TZ", _PAPPL_LOC("Tanzania, United Republic of") },
  { "TH", _PAPPL_LOC("Thailand") },
  { "TL", _PAPPL_LOC("Timor-Leste") },
  { "TG", _PAPPL_LOC("Togo") },
  { "TK", _PAPPL_LOC("Tokelau") },
  { "TO", _PAPPL_LOC("Tonga") },
  { "TT", _PAPPL_LOC("Trinidad and Tobago") },
  { "TN", _PAPPL_LOC("Tunisia") },
  { "TR", _PAPPL_LOC("Turkey") },
  { "TM", _PAPPL_LOC("Turkmenistan") },
  { "TC", _PAPPL_LOC("Turks and Caicos Islands") },
  { "TV", _PAPPL_LOC("Tuvalu") },
  { "UG", _PAPPL_LOC("Uganda") },
  { "UA", _PAPPL_LOC("Ukraine") },
  { "AE", _PAPPL_LOC("United Arab Emirates") },
  { "GB", _PAPPL_LOC("United Kingdom of Great Britain and Northern Ireland") },
  { "UK", _PAPPL_LOC("United Kingdom") },
  { "UM", _PAPPL_LOC("United States Minor Outlying Islands") },
  { "US", _PAPPL_LOC("United States of America") },
  { "UY", _PAPPL_LOC("Uruguay") },
  { "UZ", _PAPPL_LOC("Uzbekistan") },
  { "VU", _PAPPL_LOC("Vanuatu") },
  { "VE", _PAPPL_LOC("Venezuela (Bolivarian Republic of)") },
  { "VN", _PAPPL_LOC("Viet Nam") },
  { "VG", _PAPPL_LOC("Virgin Islands (British)") },
  { "VI", _PAPPL_LOC("Virgin Islands (U.S.)") },
  { "WF", _PAPPL_LOC("Wallis and Futuna") },
  { "EH", _PAPPL_LOC("Western Sahara") },
  { "YE", _PAPPL_LOC("Yemen") },
  { "ZM", _PAPPL_LOC("Zambia") },
  { "ZW", _PAPPL_LOC("Zimbabwe") }
};
#endif // HAVE_OPENSSL || HAVE_GNUTLS


//
// '_papplSystemAddPrinter()' - Add a printer
//

void
_papplSystemWebAddPrinter(
    pappl_client_t *client,
    pappl_system_t *system)
{
  cups_len_t	i;			// Looping var
  const char	*status = NULL;		// Status message, if any
  char		printer_name[128] = "",	// Printer Name
		driver_name[128] = "",	// Driver Name
		device_uri[1024] = "",	// Device URI
		*device_id = NULL,	// Device ID
		hostname[256] = "",	// Hostname
		*ptr;			// Pointer into string
  int		port = 0;		// Default port for Socket printing
  _pappl_system_dev_t devdata;		// Device callback data
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
      http_addrlist_t	*list;		// Address list

      if ((value = cupsGetOption("printer_name", num_form, form)) != NULL)
        papplCopyString(printer_name, value, sizeof(printer_name));
      if ((value = cupsGetOption("driver_name", num_form, form)) != NULL)
        papplCopyString(driver_name, value, sizeof(driver_name));
      if ((value = cupsGetOption("device_uri", num_form, form)) != NULL)
      {
        papplCopyString(device_uri, value, sizeof(device_uri));
        if ((device_id = strchr(device_uri, '|')) != NULL)
          *device_id++ = '\0';
      }

      if (!strcmp(device_uri, "socket"))
      {
        // Make URI using hostname
        if ((value = cupsGetOption("hostname", num_form, form)) == NULL)
        {
          status        = _PAPPL_LOC("Please enter a hostname or IP address for the printer.");
          device_uri[0] = '\0';
	}
	else
	{
	  // Break out the port number, if present...
	  papplCopyString(hostname, value, sizeof(hostname));
	  if ((ptr = strrchr(hostname, ':')) != NULL && !strchr(ptr, ']'))
	  {
	    char *end;			// End of value

	    *ptr++ = '\0';
	    port   = (int)strtol(ptr, &end, 10);

            if (errno == ERANGE || *end || port <= 0 || port > 65535)
            {
              status        = _PAPPL_LOC("Bad port number.");
              device_uri[0] = '\0';
            }
	  }

          if (!status)
          {
            // Then see if we can lookup the hostname or IP address (port number
            // isn't used here...)
            if ((list = httpAddrGetList(hostname, AF_UNSPEC, "9100")) == NULL)
            {
              status = _PAPPL_LOC("Unable to lookup address.");
	    }
	    else
	    {
	      httpAddrFreeList(list);
	      httpAssembleURI(HTTP_URI_CODING_ALL, device_uri, sizeof(device_uri), "socket", NULL, hostname, port, "/");
	    }
	  }
	}
      }

      if (!printer_name[0])
      {
        status = _PAPPL_LOC("Please enter a printer name.");
      }
      else if (!device_uri[0])
      {
        status = _PAPPL_LOC("Please select a device.");
      }
      else if (!driver_name[0])
      {
        status = _PAPPL_LOC("Please select a driver.");
      }
      else if (!status)
      {
        pappl_printer_t *printer = papplPrinterCreate(system, 0, printer_name, driver_name, device_id, device_uri);
					// New printer

        if (printer)
        {
          // Advertise the printer...
          pthread_rwlock_wrlock(&printer->rwlock);
          _papplPrinterRegisterDNSSDNoLock(printer);
          pthread_rwlock_unlock(&printer->rwlock);

	  // Redirect the client to the printer's status page...
          papplClientRespondRedirect(client, HTTP_STATUS_FOUND, printer->uriname);
          cupsFreeOptions(num_form, form);
          return;
	}

        switch (errno)
        {
          case EEXIST :
	      status = _PAPPL_LOC("A printer with that name already exists.");
              break;
          case EIO :
              status = _PAPPL_LOC("Unable to use that driver.");
              break;
	  case EINVAL :
	      status = _PAPPL_LOC("Printer names must start with a letter or underscore and cannot contain special characters.");
	      break;
	  default :
	      status = strerror(errno);
	      break;
	}
      }
    }

    cupsFreeOptions(num_form, form);
  }

  system_header(client, _PAPPL_LOC("Add Printer"));

  if (status)
    papplClientHTMLPrintf(client, "<div class=\"banner\">%s</div>\n", papplClientGetLocString(client, status));

  papplClientHTMLStartForm(client, client->uri, false);

  papplClientHTMLPrintf(client,
			"          <table class=\"form\">\n"
			"            <tbody>\n"
			"              <tr><th><label for=\"printer_name\">%s:</label></th><td><input type=\"text\" name=\"printer_name\" placeholder=\"%s\" value=\"%s\" required></td></tr>\n"
			"              <tr><th><label for=\"device_uri\">%s:</label></th><td><select name=\"device_uri\" id=\"device_uri\"><option value=\"\">%s</option>", papplClientGetLocString(client, _PAPPL_LOC("Name")), papplClientGetLocString(client, _PAPPL_LOC("Name of printer")), printer_name, papplClientGetLocString(client, _PAPPL_LOC("Device")), papplClientGetLocString(client, _PAPPL_LOC("Select Device")));

  devdata.client     = client;
  devdata.device_uri = device_uri;

  papplDeviceList(PAPPL_DEVTYPE_ALL, system_device_cb, &devdata, papplLogDevice, system);

  papplClientHTMLPrintf(client,
			"<option value=\"socket\">%s</option></tr>\n"
			"              <tr><th><label for=\"hostname\">%s:</label></th><td><input type=\"text\" name=\"hostname\" id=\"hostname\" placeholder=\"%s\" pattern=\"%s\" value=\"%s\" disabled=\"disabled\"></td></tr>\n"
			"              <tr><th><label for=\"driver_name\">%s:</label></th><td><select name=\"driver_name\">", papplClientGetLocString(client, _PAPPL_LOC("Network Printer")), papplClientGetLocString(client, _PAPPL_LOC("Hostname/IP Address")), papplClientGetLocString(client, _PAPPL_LOC("IP address or hostname")), hostname_pattern, hostname, papplClientGetLocString(client, _PAPPL_LOC("Driver Name")));

  if (system->autoadd_cb)
    papplClientHTMLPrintf(client, "<option value=\"auto\">%s</option>", papplClientGetLocString(client, _PAPPL_LOC("Auto-Detect Driver")));
  else
    papplClientHTMLPrintf(client, "<option value=\"\">%s</option>", papplClientGetLocString(client, _PAPPL_LOC("Select Driver")));

  for (i = 0; i < system->num_drivers; i ++)
    papplClientHTMLPrintf(client, "<option value=\"%s\"%s>%s</option>", system->drivers[i].name, !strcmp(system->drivers[i].name, driver_name) ? " selected" : "", papplClientGetLocString(client, system->drivers[i].description));

  papplClientHTMLPrintf(client,
		        "</select></td></tr>\n"
		        "             <tr><th></th><td><input type=\"submit\" value=\"%s\"></td></tr>\n"
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
		        "       </div>\n", papplClientGetLocString(client, _PAPPL_LOC("Add Printer")));

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
    cups_len_t		num_form = 0;	// Number of form variable
    cups_option_t	*form = NULL;	// Form variables

    if ((num_form = (cups_len_t)papplClientGetForm(client, &form)) == 0)
      status = _PAPPL_LOC("Invalid form data.");
    else if (!papplClientIsValidForm(client, (int)num_form, form))
      status = _PAPPL_LOC("Invalid form submission.");
    else
    {
      _papplSystemWebConfigFinalize(system, num_form, form);

      status = _PAPPL_LOC("Changes saved.");
    }

    cupsFreeOptions(num_form, form);
  }

  system_header(client, _PAPPL_LOC("Configuration"));
  if (status)
    papplClientHTMLPrintf(client, "<div class=\"banner\">%s</div>\n", papplClientGetLocString(client, status));

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
    cups_len_t     num_form,		// I - Number of form variables
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
      snprintf(uri, sizeof(uri), "geo:%g,%g", strtod(geo_lat, NULL), strtod(geo_lon, NULL));
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
      papplCopyString(contact.name, contact_name, sizeof(contact.name));
    if (contact_email)
      papplCopyString(contact.email, contact_email, sizeof(contact.email));
    if (contact_tel)
      papplCopyString(contact.telephone, contact_tel, sizeof(contact.telephone));

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
			"          <h1 class=\"title\">%s <a class=\"btn\" href=\"%s://%s:%d/config\">%s</a></h1>\n", papplClientGetLocString(client, _PAPPL_LOC("Configuration")), _papplClientGetAuthWebScheme(client), client->host_field, client->host_port, papplClientGetLocString(client, _PAPPL_LOC("Change")));

  _papplClientHTMLPutLinks(client, system->links, PAPPL_LOPTIONS_CONFIGURATION);

  _papplClientHTMLInfo(client, false, system->dns_sd_name, system->location, system->geo_location, system->organization, system->org_unit, &system->contact);

  _papplSystemWebSettings(client);

  papplClientHTMLPrintf(client,
		        "        </div>\n"
                        "        <div class=\"col-6\">\n"
                        "          <h1 class=\"title\">%s</h1>\n", papplClientGetLocString(client, _PAPPL_LOC("Printers")));

  _papplClientHTMLPutLinks(client, system->links, PAPPL_LOPTIONS_PRINTER);

  papplSystemIteratePrinters(system, (pappl_printer_cb_t)_papplPrinterWebIteratorCallback, client);

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
      close(fd);
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
      close(fd);
      return;
    }

    if (!httpWriteResponse(client->http, code))
    {
      close(fd);
      return;
    }

    papplLogClient(client, PAPPL_LOGLEVEL_INFO, "%s %s %d", code == HTTP_STATUS_OK ? "OK" : "Partial Content", "text/plain", (int)length);

    // Read buffer and write to client
    while (length > 0 && (bytes = read(fd, buffer, sizeof(buffer))) > 0)
    {
      if ((size_t)bytes > length)
        bytes = (ssize_t)length;

      length -= (size_t)bytes;
      httpWrite(client->http, buffer, (size_t)bytes);
    }

    httpWrite(client->http, "", 0);

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
    _PAPPL_LOC("Debugging"),
    _PAPPL_LOC("Informational"),
    _PAPPL_LOC("Warning"),
    _PAPPL_LOC("Errors"),
    _PAPPL_LOC("Fatal Errors/Conditions"),
  };


  if (!papplClientHTMLAuthorize(client))
    return;

  if (client->operation == HTTP_STATE_POST)
  {
    const char		*value;		// Form value
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
          status = _PAPPL_LOC("Changes Saved.");
	}
	else
	  status = _PAPPL_LOC("Please select a valid log level.");
      }
      else
      {
        status = _PAPPL_LOC("Please select a valid log level.");
      }
    }

    cupsFreeOptions(num_form, form);
  }

  system_header(client, _PAPPL_LOC("Logs"));

  if (status)
    papplClientHTMLPrintf(client, "<div class=\"banner\">%s</div>\n", papplClientGetLocString(client, status));

  papplClientHTMLStartForm(client, client->uri, false);

  papplClientHTMLPrintf(client,
		        "          <table>\n"
		        "            <tbody>\n"
		        "              <tr><th><label for=\"log_level\">%s:</label></th><td><select name=\"log_level\" id=\"log_level\"><option value=\"\">%s</option>\n", papplClientGetLocString(client, _PAPPL_LOC("Log Level")), papplClientGetLocString(client, _PAPPL_LOC("Select Log Level")));

  for (i = PAPPL_LOGLEVEL_DEBUG, loglevel = papplSystemGetLogLevel(system); i <= PAPPL_LOGLEVEL_FATAL; i ++)
  {
    papplClientHTMLPrintf(client, "               <option value=\"%s\"%s>%s</option>\n", levels[i - PAPPL_LOGLEVEL_DEBUG], i == loglevel ? " selected" : "", papplClientGetLocString(client, levels[i - PAPPL_LOGLEVEL_DEBUG]));
  }

  papplClientHTMLPrintf(client,
		        "             </select> <input type=\"submit\" value=\"%s\"></td></tr>\n"
		        "              <tr><th>%s:</label></th><td><a class=\"btn\" href=\"/logfile.txt\">%s</a></td></tr>\n"
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
		        "update_log();</script>\n", papplClientGetLocString(client, _PAPPL_LOC("Change Log Level")), papplClientGetLocString(client, _PAPPL_LOC("Log File")), papplClientGetLocString(client, _PAPPL_LOC("Download Log File")));

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
  pappl_loc_t	*loc;			// Localization
  size_t	i,			// Looping var
		num_networks;		// Number of network interfaces
  pappl_network_t networks[32],		// Network interfaces
		*network;		// Curent network


  if (!papplClientHTMLAuthorize(client))
    return;

  if (client->system->network_get_cb)
    num_networks = (client->system->network_get_cb)(client->system, client->system->network_cbdata, sizeof(networks) / sizeof(networks[0]), networks);
  else
    num_networks = get_networks(sizeof(networks) / sizeof(networks[0]), networks);

  if (client->operation == HTTP_STATE_POST)
  {
    int			j;		// Looping var
    cups_len_t		num_form = 0;	// Number of form variable
    cups_option_t	*form = NULL;	// Form variables
    char		name[128];	// Variable name
    const char		*value;		// Form variable value

    if ((num_form = (cups_len_t)papplClientGetForm(client, &form)) == 0)
    {
      status = _PAPPL_LOC("Invalid form data.");
    }
    else if (!papplClientIsValidForm(client, (int)num_form, form))
    {
      status = _PAPPL_LOC("Invalid form submission.");
    }
    else if ((value = cupsGetOption("hostname", num_form, form)) != NULL)
    {
      // Set hostname and save it...
      papplSystemSetHostName(client->system, value);

      // Then look for network changes
      if (client->system->network_set_cb && num_networks > 0)
      {
        for (i = num_networks, network = networks; i > 0; i --, network ++)
        {
          snprintf(name, sizeof(name), "%s.domain", network->ident);
          if ((value = cupsGetOption(name, num_form, form)) != NULL)
            papplCopyString(network->domain, value, sizeof(network->domain));

          snprintf(name, sizeof(name), "%s.config", network->ident);
          if ((value = cupsGetOption(name, num_form, form)) != NULL)
          {
            pappl_netconf_t config = (pappl_netconf_t)atoi(value);
					// Configuration value

            if (config < PAPPL_NETCONF_OFF || config > PAPPL_NETCONF_MANUAL)
	    {
	      status = _PAPPL_LOC("Invalid network configuration.");
	      goto post_done;
	    }

            network->config = config;
	  }

          snprintf(name, sizeof(name), "%s.addr4", network->ident);
          if ((value = cupsGetOption(name, num_form, form)) != NULL)
          {
            if (inet_pton(AF_INET, value, &network->addr4.ipv4.sin_addr) <= 0)
            {
	      status = _PAPPL_LOC("Invalid IPv4 address.");
	      goto post_done;
            }
          }

          snprintf(name, sizeof(name), "%s.mask4", network->ident);
          if ((value = cupsGetOption(name, num_form, form)) != NULL)
          {
            if (inet_pton(AF_INET, value, &network->mask4.ipv4.sin_addr) <= 0)
            {
	      status = _PAPPL_LOC("Invalid IPv4 netmask.");
	      goto post_done;
            }
          }

          snprintf(name, sizeof(name), "%s.gateway4", network->ident);
          if ((value = cupsGetOption(name, num_form, form)) != NULL)
          {
            if (*value)
              inet_pton(AF_INET, value, &network->gateway4.ipv4.sin_addr);
	    else
              network->gateway4.ipv4.sin_addr.s_addr = 0;
          }

          for (j = 0; j < 2; j ++)
          {
	    network->dns[j].ipv4.sin_family      = AF_INET;
	    network->dns[j].ipv4.sin_addr.s_addr = 0;

	    snprintf(name, sizeof(name), "%s.dns_%d", network->ident, j + 1);
	    if ((value = cupsGetOption(name, num_form, form)) != NULL)
	    {
	      if (*value)
	      {
		if (strchr(value, ':') != NULL)
		{
		  network->dns[j].ipv6.sin6_family = AF_INET6;
		  inet_pton(AF_INET6, value, &network->dns[j].ipv6.sin6_addr);
		}
		else
		{
		  network->dns[j].ipv4.sin_family = AF_INET;
		  inet_pton(AF_INET, value, &network->dns[j].ipv4.sin_addr);
		}
	      }
	    }
	  }

          snprintf(name, sizeof(name), "%s.addr6", network->ident);
          if ((value = cupsGetOption(name, num_form, form)) != NULL)
          {
            network->addr6.ipv6.sin6_family = AF_INET6;

            if (*value)
            {
              // Parse IPv6 address
              if (inet_pton(AF_INET6, value, &network->addr6.ipv6.sin6_addr) <= 0)
              {
		status = _PAPPL_LOC("Invalid IPv6 address.");
		goto post_done;
              }
	    }
	    else
	    {
	      // Clear IPv6 address...
	      memset(&network->addr6.ipv6.sin6_addr, 0, sizeof(network->addr6.ipv6.sin6_addr));
	    }
	  }

          snprintf(name, sizeof(name), "%s.prefix6", network->ident);
          if ((value = cupsGetOption(name, num_form, form)) != NULL)
          {
            int intvalue = atoi(value);

            if (intvalue > 0 && intvalue <= 128)
	    {
	      network->prefix6 = (unsigned)intvalue;
	    }
	    else
	    {
	      status = _PAPPL_LOC("Invalid IPv6 prefix length.");
	      goto post_done;
	    }
          }
        }

        if (!(client->system->network_set_cb)(client->system, client->system->network_cbdata, num_networks, networks))
        {
	  status = _PAPPL_LOC("Unable to save network configuration.");
	  goto post_done;
        }
      }

      if (!status)
        status = _PAPPL_LOC("Changes saved.");
    }
    else
    {
      status = _PAPPL_LOC("Unknown form request.");
    }

    post_done:

    cupsFreeOptions(num_form, form);
  }

  system_header(client, _PAPPL_LOC("Networking"));

  if (status)
    papplClientHTMLPrintf(client, "<div class=\"banner\">%s</div>\n", papplClientGetLocString(client, status));

  papplClientHTMLStartForm(client, client->uri, false);
  papplClientHTMLPuts(client,
		      "          <table class=\"form\">\n"
		      "            <tbody>\n");
  if (system->wifi_status_cb)
  {
    pappl_wifi_t	wifi_info;	// Wi-Fi info
    static const char * const wifi_statuses[] =
    {					// Wi-Fi state values
      _PAPPL_LOC("off"),
      _PAPPL_LOC("not configured"),
      _PAPPL_LOC("not visible"),
      _PAPPL_LOC("unable to join"),
      _PAPPL_LOC("joining"),
      _PAPPL_LOC("on")
    };

    if ((system->wifi_status_cb)(system, system->wifi_cbdata, &wifi_info))
    {
      papplClientHTMLPrintf(client, "              <tr><th>%s:</th><td>%s (%s)", papplClientGetLocString(client, _PAPPL_LOC("Wi-Fi Network")), wifi_info.ssid, papplClientGetLocString(client, wifi_statuses[wifi_info.state - PAPPL_WIFI_STATE_OFF]));
      if (system->wifi_list_cb)
        papplClientHTMLPrintf(client, " <a class=\"btn\" href=\"/network-wifi\">%s</a></td></tr>\n", papplClientGetLocString(client, _PAPPL_LOC("Change Wi-Fi Network")));
      else
        papplClientHTMLPuts(client, "</td></tr>\n");
    }
  }

  papplClientHTMLPrintf(client, "              <tr><th><label for=\"hostname\">%s:</label></th><td><input type=\"text\" name=\"hostname\" value=\"%s\" placeholder=\"name.domain\" pattern=\"^(|[-_a-zA-Z0-9][-._a-zA-Z0-9]*)$\"> <input type=\"submit\" value=\"%s\"></td></tr>\n", papplClientGetLocString(client, _PAPPL_LOC("Hostname")), system->hostname, papplClientGetLocString(client, _PAPPL_LOC("Change Hostname")));

  loc = papplClientGetLoc(client);

  if (num_networks > 0)
  {
    size_t		j;		// Looping var
    char		temp[256];	// Address string
    static const char * const configs[] =
    {
      _PAPPL_LOC("Off"),
      _PAPPL_LOC("DHCP"),
      _PAPPL_LOC("DHCP w/Manual Address"),
      _PAPPL_LOC("Manual Configuration")
    };

    papplClientHTMLPuts(client,
                        "              <script><!--\n"
                        "function update_ipv4(ifname) {\n"
                        "  let config = document.forms['form'][ifname + '.config'].selectedIndex;\n"
                        "  document.forms['form'][ifname + '.addr4'].disabled = config < 2;\n"
                        "  document.forms['form'][ifname + '.mask4'].disabled = config < 3;\n"
                        "  document.forms['form'][ifname + '.gateway4'].disabled = config < 3;\n"
                        "  document.forms['form'][ifname + '.dns_1'].disabled = config < 3;\n"
                        "  document.forms['form'][ifname + '.dns_2'].disabled = config < 3;\n"
                        "}\n"
                        "--></script>\n");

    for (i = num_networks, network = networks; i > 0; i --, network ++)
    {
      if (client->system->network_set_cb)
      {
	papplClientHTMLPrintf(client, "              <tr><th>%s:</th><td>%s: <select name=\"%s.config\" onchange=\"update_ipv4('%s');\">", papplLocGetString(loc, network->name), papplLocGetString(loc, _PAPPL_LOC("Configuration")), network->ident, network->ident);
	for (j = 0; j < (sizeof(configs) / sizeof(configs[0])); j ++)
	  papplClientHTMLPrintf(client, "<option value=\"%u\"%s>%s</option>", (unsigned)j, (pappl_netconf_t)j == network->config ? " selected" : "", papplLocGetString(loc, configs[j]));
	papplClientHTMLPuts(client, "</select><br>");
      }
      else
      {
	papplClientHTMLPrintf(client, "              <tr><th>%s:</th><td>%s: %s<br>", papplLocGetString(loc, network->name), papplLocGetString(loc, _PAPPL_LOC("Configuration")), papplLocGetString(loc, configs[network->config]));
      }

      papplClientHTMLPrintf(client, "%s: ", papplLocGetString(loc, _PAPPL_LOC("IPv4 Address")));
      inet_ntop(AF_INET, &network->addr4.ipv4.sin_addr, temp, sizeof(temp));
      if (client->system->network_set_cb)
        papplClientHTMLPrintf(client, "<input type=\"text\" name=\"%s.addr4\" value=\"%s\" size=\"15\" pattern=\"[0-9]{1,3}\\.[0-9]{1,3}\\.[0-9]{1,3}\\.[0-9]{1,3}\"%s><br>", network->ident, temp, network->config < PAPPL_NETCONF_DHCP_MANUAL ? " disabled" : "");
      else
        papplClientHTMLPrintf(client, "<tt>%s</tt><br>", temp);

      papplClientHTMLPrintf(client, "%s: ", papplLocGetString(loc, _PAPPL_LOC("IPv4 Netmask")));
      inet_ntop(AF_INET, &network->mask4.ipv4.sin_addr, temp, sizeof(temp));
      if (client->system->network_set_cb)
        papplClientHTMLPrintf(client, "<input type=\"text\" name=\"%s.mask4\" value=\"%s\" size=\"15\" pattern=\"[0-9]{1,3}\\.[0-9]{1,3}\\.[0-9]{1,3}\\.[0-9]{1,3}\"%s><br>", network->ident, temp, network->config < PAPPL_NETCONF_DHCP_MANUAL ? " disabled" : "");
      else
        papplClientHTMLPrintf(client, "<tt>%s</tt><br>", temp);

      papplClientHTMLPrintf(client, "%s: ", papplLocGetString(loc, _PAPPL_LOC("IPv4 Gateway")));
      inet_ntop(AF_INET, &network->gateway4.ipv4.sin_addr, temp, sizeof(temp));
      if (client->system->network_set_cb)
        papplClientHTMLPrintf(client, "<input type=\"text\" name=\"%s.gateway4\" value=\"%s\" size=\"15\" pattern=\"[0-9]{1,3}\\.[0-9]{1,3}\\.[0-9]{1,3}\\.[0-9]{1,3}\"%s><br>", network->ident, temp, network->config < PAPPL_NETCONF_DHCP_MANUAL ? " disabled" : "");
      else
        papplClientHTMLPrintf(client, "<tt>%s</tt><br>", temp);

      papplClientHTMLPrintf(client, "%s: ", papplLocGetString(loc, _PAPPL_LOC("IPv4 Primary DNS")));
      if (network->dns[0].addr.sa_family == AF_INET6)
        inet_ntop(AF_INET6, &network->dns[0].ipv6.sin6_addr, temp, sizeof(temp));
      else
        inet_ntop(AF_INET, &network->dns[0].ipv4.sin_addr, temp, sizeof(temp));
      if (!strcmp(temp, "0.0.0.0"))
        temp[0] = '\0';
      if (client->system->network_set_cb)
        papplClientHTMLPrintf(client, "<input type=\"text\" name=\"%s.dns_1\" value=\"%s\" size=\"15\" pattern=\"[0-9]{1,3}\\.[0-9]{1,3}\\.[0-9]{1,3}\\.[0-9]{1,3}\"%s><br>", network->ident, temp, network->config < PAPPL_NETCONF_DHCP_MANUAL ? " disabled" : "");
      else
        papplClientHTMLPrintf(client, "<tt>%s</tt><br>", temp);

      papplClientHTMLPrintf(client, "%s: ", papplLocGetString(loc, _PAPPL_LOC("IPv4 Secondary DNS")));
      if (network->dns[1].addr.sa_family == AF_INET6)
        inet_ntop(AF_INET6, &network->dns[1].ipv6.sin6_addr, temp, sizeof(temp));
      else
        inet_ntop(AF_INET, &network->dns[1].ipv4.sin_addr, temp, sizeof(temp));
      if (!strcmp(temp, "0.0.0.0"))
        temp[0] = '\0';
      if (client->system->network_set_cb)
        papplClientHTMLPrintf(client, "<input type=\"text\" name=\"%s.dns_2\" value=\"%s\" size=\"15\" pattern=\"[0-9]{1,3}\\.[0-9]{1,3}\\.[0-9]{1,3}\\.[0-9]{1,3}\"%s><br>", network->ident, temp, network->config < PAPPL_NETCONF_DHCP_MANUAL ? " disabled" : "");
      else
        papplClientHTMLPrintf(client, "<tt>%s</tt><br>", temp);

      papplClientHTMLPrintf(client, "%s: ", papplLocGetString(loc, _PAPPL_LOC("Domain Name")));
      if (client->system->network_set_cb)
        papplClientHTMLPrintf(client, "<input type=\"text\" name=\"%s.domain\" value=\"%s\" size=\"15\"><br>", network->ident, network->domain);
      else if (network->domain[0])
        papplClientHTMLPrintf(client, "<tt>%s</tt><br>", network->domain);

      inet_ntop(AF_INET6, &network->linkaddr6.ipv6.sin6_addr, temp, sizeof(temp));
      papplClientHTMLPrintf(client, "%s: <tt>%s</tt><br>", papplLocGetString(loc, _PAPPL_LOC("IPv6 Link-Local")), temp);

      if (client->system->network_set_cb)
      {
        // IPv6 routable address...
        inet_ntop(AF_INET6, &network->addr6.ipv6.sin6_addr, temp, sizeof(temp));

	papplClientHTMLPrintf(client, "%s: ", papplLocGetString(loc, _PAPPL_LOC("IPv6 Address")));
        papplClientHTMLPrintf(client, "<input type=\"text\" name=\"%s.addr6\" value=\"%s\"><br>", network->ident, temp);

	papplClientHTMLPrintf(client, "%s: ", papplLocGetString(loc, _PAPPL_LOC("IPv6 Prefix Length")));
        papplClientHTMLPrintf(client, "<input type=\"number\" name=\"%s.prefix6\" value=\"%u\"><br>", network->ident, network->prefix6 ? network->prefix6 : 64);
      }

      if (client->system->network_set_cb)
        papplClientHTMLPrintf(client, "<input type=\"submit\" value=\"%s\">", _PAPPL_LOC("Change Network Settings"));

      papplClientHTMLPuts(client, "</td></tr>\n");
    }
  }

  papplClientHTMLPuts(client,
		      "            </tbody>\n"
		      "          </table>\n"
		      "        </form>\n");

  _papplClientHTMLPutLinks(client, client->system->links, PAPPL_LOPTIONS_NETWORK);

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
#if !_WIN32
  struct group	*grp;			// Current group
#endif // !_WIN32


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
        status = _PAPPL_LOC("Wrong old password.");
      }
      else if (!new_password || !new_password2 || strcmp(new_password, new_password2))
      {
        status = _PAPPL_LOC("Passwords do not match.");
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
          status = _PAPPL_LOC("Password must be at least eight characters long and contain at least one uppercase letter, one lowercase letter, and one digit.");
        }
        else
        {
          papplSystemHashPassword(system, NULL, new_password, hash, sizeof(hash));
          papplSystemSetPassword(system, hash);
          status = _PAPPL_LOC("Password changed.");
	}
      }
    }
#if !_WIN32
    else
    {
      const char	 *group;	// Current group
      char		buffer[8192];	// Buffer for strings
      struct group	grpbuf;		// Group buffer

      grp = NULL;

      if ((group = cupsGetOption("admin_group", num_form, form)) != NULL)
      {
        if (getgrnam_r(group, &grpbuf, buffer, sizeof(buffer), &grp) || !grp)
          status = _PAPPL_LOC("Bad administration group.");
	else
	  papplSystemSetAdminGroup(system, group);
      }

      if ((group = cupsGetOption("print_group", num_form, form)) != NULL)
      {
        if (getgrnam_r(group, &grpbuf, buffer, sizeof(buffer), &grp) || !grp)
        {
          status = _PAPPL_LOC("Bad print group.");
	}
	else
	{
	  papplSystemSetDefaultPrintGroup(system, group);
	  papplSystemIteratePrinters(system, (pappl_printer_cb_t)papplPrinterSetPrintGroup, (void *)group);
	}
      }

      if (!status)
        status = _PAPPL_LOC("Group changes saved.");
    }
#endif // !_WIN32

    cupsFreeOptions(num_form, form);
  }

  system_header(client, _PAPPL_LOC("Security"));

  if (status)
    papplClientHTMLPrintf(client, "<div class=\"banner\">%s</div>\n", papplClientGetLocString(client, status));

  papplClientHTMLPuts(client,
                      "        </div>\n"
                      "      </div>\n"
                      "      <div class=\"row\">\n");

#if !_WIN32
  if (system->auth_service)
  {
    // Show Users pane for group controls
    papplClientHTMLPrintf(client,
			  "        <div class=\"col-12\">\n"
			  "          <h2 class=\"title\">%s</h2>\n", papplClientGetLocString(client, _PAPPL_LOC("Users")));

    papplClientHTMLStartForm(client, client->uri, false);

    papplClientHTMLPrintf(client,
			  "          <table class=\"form\">\n"
			  "            <tbody>\n"
			  "              <tr><th><label for=\"admin_group\">%s:</label></th><td><select name\"admin_group\"><option value=\"\">%s</option>", papplClientGetLocString(client, _PAPPL_LOC("Admin Group")), papplClientGetLocString(client, _PAPPL_LOC("None")));

    setgrent();
    while ((grp = getgrent()) != NULL)
    {
      papplClientHTMLPrintf(client, "<option%s>%s</option>", (system->admin_group && !strcmp(grp->gr_name, system->admin_group)) ? " selected" : "", grp->gr_name);
    }

    papplClientHTMLPrintf(client,
			  "</select></td></tr>\n"
			  "              <tr><th><label for=\"print_group\">%s:</label></th><td><select name\"print_group\"><option value=\"\">%s</option>", papplClientGetLocString(client, _PAPPL_LOC("Print Group")), papplClientGetLocString(client, _PAPPL_LOC("None")));

    setgrent();
    while ((grp = getgrent()) != NULL)
    {
      papplClientHTMLPrintf(client, "<option%s>%s</option>", (system->default_print_group && !strcmp(grp->gr_name, system->default_print_group)) ? " selected" : "", grp->gr_name);
    }

    papplClientHTMLPrintf(client,
			  "</select></td></tr>\n"
			  "              <tr><th></th><td><input type=\"submit\" value=\"%s\"></td></tr>\n"
			  "            </tbody>\n"
			  "          </table>\n"
			  "        </div>\n"
			  "        </form>\n", papplClientGetLocString(client, _PAPPL_LOC("Save Changes")));
  }
  else
#endif // !_WIN32
  if (system->password_hash[0])
  {
    // Show simple access password update form...
    papplClientHTMLPrintf(client,
			  "        <div class=\"col-12\">\n"
			  "          <h2 class=\"title\">%s</h2>\n", papplClientGetLocString(client, _PAPPL_LOC("Change Access Password")));

    papplClientHTMLStartForm(client, client->uri, false);

    papplClientHTMLPrintf(client,
			  "          <table class=\"form\">\n"
			  "            <tbody>\n"
			  "              <tr><th><label for=\"old_password\">%s:</label></th><td><input type=\"password\" name=\"old_password\"></td></tr>\n"
			  "              <tr><th><label for=\"new_password\">%s:</label></th><td><input type=\"password\" name=\"new_password\" placeholder=\"%s\"></td></tr>\n"
			  "              <tr><th><label for=\"new_password2\">%s:</label></th><td><input type=\"password\" name=\"new_password2\" placeholder=\"%s\"></td></tr>\n"
			  "              <tr><th></th><td><input type=\"submit\" value=\"%s\"></td></tr>\n"
			  "            </tbody>\n"
			  "          </table>\n"
			  "        </div>\n"
			  "        </form>\n", papplClientGetLocString(client, _PAPPL_LOC("Current Password")), papplClientGetLocString(client, _PAPPL_LOC("New Password")), papplClientGetLocString(client, _PAPPL_LOC(/*Password Requirements*/"8+, upper+lower+digit")), papplClientGetLocString(client, _PAPPL_LOC("New Password (again)")), papplClientGetLocString(client, _PAPPL_LOC(/*Password Requirements*/"8+, upper+lower+digit")), papplClientGetLocString(client, _PAPPL_LOC("Change Access Password")));

  }
  else
  {
    // Show simple access password initial setting form...
    papplClientHTMLPrintf(client,
			  "        <div class=\"col-12\">\n"
			  "          <h2 class=\"title\">%s</h2>\n", papplClientGetLocString(client, _PAPPL_LOC("Set Access Password")));

    papplClientHTMLStartForm(client, client->uri, false);

    papplClientHTMLPrintf(client,
			  "          <table class=\"form\">\n"
			  "            <tbody>\n"
			  "              <tr><th><label for=\"new_password\">%s:</label></th><td><input type=\"password\" name=\"new_password\" placeholder=\"%s\"></td></tr>\n"
			  "              <tr><th><label for=\"new_password2\">%s:</label></th><td><input type=\"password\" name=\"new_password2\" placeholder=\"%s\"></td></tr>\n"
			  "              <tr><th></th><td><input type=\"submit\" value=\"%s\"></td></tr>\n"
			  "            </tbody>\n"
			  "          </table>\n"
			  "        </div>\n"
			  "        </form>\n", papplClientGetLocString(client, _PAPPL_LOC("Password")), papplClientGetLocString(client, _PAPPL_LOC(/*Password Requirements*/"8+, upper+lower+digit")), papplClientGetLocString(client, _PAPPL_LOC("Password (again)")), papplClientGetLocString(client, _PAPPL_LOC(/*Password Requirements*/"8+, upper+lower+digit")), papplClientGetLocString(client, _PAPPL_LOC("Set Access Password")));
  }

  _papplClientHTMLPutLinks(client, client->system->links, PAPPL_LOPTIONS_SECURITY);

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
  cups_len_t	i,			// Looping var
		count;			// Number of links
  _pappl_link_t	*l;			// Current link


  for (i = 0, count = cupsArrayGetCount(client->system->links); i < count; i ++)
  {
    l = (_pappl_link_t *)cupsArrayGetElement(client->system->links, i);

    if (!l)
      continue;

    if (l->options & PAPPL_LOPTIONS_OTHER)
      break;
  }

  if (i < count)
  {
    papplClientHTMLPrintf(client,
                          "          <h2 class=\"title\">%s</h2>\n"
                          "          <div class=\"btn\">", papplClientGetLocString(client, _PAPPL_LOC("Other Settings")));
    _papplClientHTMLPutLinks(client, client->system->links, PAPPL_LOPTIONS_OTHER);
    papplClientHTMLPuts(client, "</div>\n");
  }

  if ((client->system->options & PAPPL_SOPTIONS_WEB_LOG) && client->system->logfile && strcmp(client->system->logfile, "-") && strcmp(client->system->logfile, "syslog"))
  {
    papplClientHTMLPrintf(client,
                          "          <h2 class=\"title\">%s</h2>\n"
                          "          <div class=\"btn\">", papplClientGetLocString(client, _PAPPL_LOC("Logging")));
    _papplClientHTMLPutLinks(client, client->system->links, PAPPL_LOPTIONS_LOGGING);
    papplClientHTMLPuts(client, "</div>\n");
  }
}


#if defined(HAVE_OPENSSL) || defined(HAVE_GNUTLS)
//
// '_papplSystemWebTLSInstall()' - Show the system TLS certificate installation page.
//

void
_papplSystemWebTLSInstall(
    pappl_client_t *client,		// I - Client
    pappl_system_t *system)		// I - System
{
  const char	*status = NULL;		// Status message, if any


  (void)system;

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
      const char	*crtfile,	// Certificate file
			*keyfile;	// Private key file
      char		filename[1024];	// Filename

      crtfile = cupsGetOption("certificate", num_form, form);
      keyfile = cupsGetOption("privatekey", num_form, form);

      if (!keyfile)
      {
        char	hostname[256],		// Hostname
	      	*hostptr;		// Pointer into hostname

        papplCopyString(hostname, client->system->hostname, sizeof(hostname));
        if ((hostptr = strchr(hostname, '.')) != NULL)
          *hostptr = '\0';

        snprintf(filename, sizeof(filename), "%s/%s.key", client->system->directory, hostname);
        if (!access(filename, R_OK))
          keyfile = filename;
	else
	  status = _PAPPL_LOC("Missing private key.");
      }

      if (!status)
      {
        if (tls_install_certificate(client, crtfile, keyfile))
          status = _PAPPL_LOC("Certificate installed.");
        else
          status = _PAPPL_LOC("Invalid certificate or private key.");
      }
    }

    cupsFreeOptions(num_form, form);
  }

  system_header(client, _PAPPL_LOC("Install TLS Certificate"));

  if (status)
    papplClientHTMLPrintf(client, "<div class=\"banner\">%s</div>\n", papplClientGetLocString(client, status));

  papplClientHTMLPuts(client,
                      "        </div>\n"
                      "      </div>\n"
                      "      <div class=\"row\">\n");

  papplClientHTMLStartForm(client, client->uri, true);
  papplClientHTMLPrintf(client,
		        "        <div class=\"col-12\">\n"
		        "          <p>%s</p>\n"
		        "          <table class=\"form\">\n"
		        "            <tbody>\n"
		        "              <tr><th><label for=\"certificate\">%s:</label></th><td><input type=\"file\" name=\"certificate\" accept=\".crt,.pem,application/pem-certificate-chain,application/x-x509-ca-cert,application/octet-stream\" required> (PEM-encoded)</td></tr>\n"
		        "              <tr><th><label for=\"privatekey\">%s:</label></th><td><input type=\"file\" name=\"privatekey\" accept=\".key,.pem,application/octet-stream\"> %s</td></tr>\n"
		        "              <tr><th></th><td><input type=\"submit\" value=\"%s\"></td></tr>\n"
		        "            </tbody>\n"
		        "          </table>\n"
		        "        </div>\n"
		        "        </form>\n", papplClientGetLocString(client, _PAPPL_LOC("This form will install a trusted TLS certificate you have obtained from a Certificate Authority ('CA'). Once installed, it will be used immediately.")), papplClientGetLocString(client, _PAPPL_LOC("Certificate")), papplClientGetLocString(client, _PAPPL_LOC("Private Key")), papplClientGetLocString(client, _PAPPL_LOC("(PEM-encoded, leave unselected to use the key from the last signing request)")), papplClientGetLocString(client, _PAPPL_LOC("Install Certificate")));

  _papplClientHTMLPutLinks(client, client->system->links, PAPPL_LOPTIONS_TLS);

  papplClientHTMLPuts(client, "      </div>\n");

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
    else if (!strcmp(client->uri, "/tls-new-crt"))
    {
      if (tls_make_certificate(client, num_form, form))
      {
        status  = _PAPPL_LOC("Certificate created.");
        success = true;
      }
      else
        status = _PAPPL_LOC("Unable to create certificate.");
    }
    else
    {
      if (tls_make_certsignreq(client, num_form, form, crqpath, sizeof(crqpath)))
      {
        status  = _PAPPL_LOC("Certificate request created.");
        success = true;
      }
      else
        status = _PAPPL_LOC("Unable to create certificate request.");
    }

    cupsFreeOptions(num_form, form);
  }

  if (!strcmp(client->uri, "/tls-new-crt"))
    system_header(client, _PAPPL_LOC("Create New TLS Certificate"));
  else
    system_header(client, _PAPPL_LOC("Create TLS Certificate Request"));

  if (status)
  {
    papplClientHTMLPrintf(client, "          <div class=\"banner\">%s</div>\n", papplClientGetLocString(client, status));

    if (crqpath[0])
      papplClientHTMLPrintf(client, "          <p><a class=\"btn\" href=\"%s\">%s</a></p>\n", crqpath, papplClientGetLocString(client, _PAPPL_LOC("Download Certificate Request File")));

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
    papplClientHTMLPrintf(client,
			  "        <div class=\"col-12\">\n"
			  "          <p>%s</p>\n"
			  "          <table class=\"form\">\n"
			  "            <tbody>\n"
			  "              <tr><th><label for=\"duration\">%s:</label></th><td><input type=\"number\" name=\"duration\" min=\"1\" max=\"10\" step=\"1\" value=\"5\" size=\"2\" maxsize=\"2\"></td></tr>\n", papplClientGetLocString(client, _PAPPL_LOC("This form creates a new 'self-signed' TLS certificate for secure printing. Self-signed certificates are not automatically trusted by web browsers.")), papplClientGetLocString(client, _PAPPL_LOC("Duration (years)")));
  else
    papplClientHTMLPrintf(client,
			  "        <div class=\"col-12\">\n"
			  "          <p>%s</p>\n"
			  "          <table class=\"form\">\n"
			  "            <tbody>\n", papplClientGetLocString(client, _PAPPL_LOC("This form creates a certificate signing request ('CSR') that you can send to a Certificate Authority ('CA') to obtain a trusted TLS certificate. The private key is saved separately for use with the certificate you get from the CA.")));

  papplClientHTMLPrintf(client,
			"              <tr><th><label for=\"level\">%s:</label></th><td><select name=\"level\"><option value=\"rsa-2048\">%s</option><option value=\"rsa-4096\">%s</option><option value=\"ecdsa-p384\">%s</option></select></td></tr>\n"
			"              <tr><th><label for=\"email\">%s:</label></th><td><input type=\"email\" name=\"email\" value=\"%s\" placeholder=\"name@example.com\"></td></tr>\n"
			"              <tr><th><label for=\"organization\">%s:</label></th><td><input type=\"text\" name=\"organization\" value=\"%s\" placeholder=\"%s\"></td></tr>\n"
			"              <tr><th><label for=\"organizational_unit\">%s:</label></th><td><input type=\"text\" name=\"organizational_unit\" value=\"%s\" placeholder=\"%s\"></td></tr>\n"
			"              <tr><th><label for=\"city\">%s:</label></th><td><input type=\"text\" name=\"city\" placeholder=\"%s\">  <button id=\"address_lookup\" onClick=\"event.preventDefault(); navigator.geolocation.getCurrentPosition(setAddress);\">%s</button></td></tr>\n"
			"              <tr><th><label for=\"state\">%s:</label></th><td><input type=\"text\" name=\"state\" placeholder=\"%s\"></td></tr>\n"
			"              <tr><th><label for=\"country\">%s:</label></th><td><select name=\"country\"><option value="">%s</option>", papplClientGetLocString(client, _PAPPL_LOC("Level")), papplClientGetLocString(client, _PAPPL_LOC("Good (2048-bit RSA)")), papplClientGetLocString(client, _PAPPL_LOC("Better (4096-bit RSA)")), papplClientGetLocString(client, _PAPPL_LOC("Best (384-bit ECC)")), papplClientGetLocString(client, _PAPPL_LOC("EMail (contact)")), system->contact.email, papplClientGetLocString(client, _PAPPL_LOC("Organization")), system->organization ? system->organization : "", papplClientGetLocString(client, _PAPPL_LOC("Organization/business name")), papplClientGetLocString(client, _PAPPL_LOC("Organization Unit")), system->org_unit ? system->org_unit : "", papplClientGetLocString(client, _PAPPL_LOC("Unit, department, etc.")), papplClientGetLocString(client, _PAPPL_LOC("City/Locality")), papplClientGetLocString(client, _PAPPL_LOC("City/town name")), papplClientGetLocString(client, _PAPPL_LOC("Use My Position")), papplClientGetLocString(client, _PAPPL_LOC("State/Province")), papplClientGetLocString(client, _PAPPL_LOC("State/province name")), papplClientGetLocString(client, _PAPPL_LOC("Country or Region")), papplClientGetLocString(client, _PAPPL_LOC("Choose")));

  for (i = 0; i < (int)(sizeof(countries) / sizeof(countries[0])); i ++)
    papplClientHTMLPrintf(client, "<option value=\"%s\">%s</option>", countries[i][0], papplClientGetLocString(client, countries[i][1]));

  if (!strcmp(client->uri, "/tls-new-crt"))
    papplClientHTMLPrintf(client,
			  "</select></td></tr>\n"
			  "              <tr><th></th><td><input type=\"submit\" value=\"%s\"></td></tr>\n", papplClientGetLocString(client, _PAPPL_LOC("Create New Certificate")));
  else
    papplClientHTMLPrintf(client,
			  "</select></td></tr>\n"
			  "              <tr><th></th><td><input type=\"submit\" value=\"%s\"></td></tr>\n", papplClientGetLocString(client, _PAPPL_LOC("Create Certificate Signing Request")));

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
		      "        </script>\n");

  _papplClientHTMLPutLinks(client, client->system->links, PAPPL_LOPTIONS_TLS);

  papplClientHTMLPuts(client, "      </div>\n");

  system_footer(client);
}
#endif // HAVE_OPENSSL || HAVE_GNUTLS


//
// '_papplSystemWebWiFi()' - Show the Wi-Fi network UI.
//

void
_papplSystemWebWiFi(
    pappl_client_t *client,		// I - Client
    pappl_system_t *system)		// I - System
{
  cups_len_t	i,			// Looping var
		num_ssids;		// Number of Wi-Fi networks
  cups_dest_t	*ssids;			// Wi-Fi networks
  const char	*status = NULL;		// Status message, if any


  if (!papplClientHTMLAuthorize(client))
    return;

  if (client->operation == HTTP_STATE_POST)
  {
    cups_len_t		num_form = 0;	// Number of form variable
    cups_option_t	*form = NULL;	// Form variables
    const char		*ssid,		// Wi-Fi network name
			*psk;		// Wi-Fi password

    if ((num_form = (cups_len_t)papplClientGetForm(client, &form)) == 0)
    {
      status = _PAPPL_LOC("Invalid form data.");
    }
    else if (!papplClientIsValidForm(client, (int)num_form, form))
    {
      status = _PAPPL_LOC("Invalid form submission.");
    }
    else if ((ssid = cupsGetOption("ssid", num_form, form)) != NULL && (psk = cupsGetOption("psk", num_form, form)) != NULL)
    {
      if ((system->wifi_join_cb)(system, system->wifi_cbdata, ssid, psk))
      {
        papplClientRespondRedirect(client, HTTP_STATUS_FOUND, "/network");
        return;
      }
      else
      {
        status = _PAPPL_LOC("Unable to join Wi-Fi network.");
      }
    }
    else
    {
      status = _PAPPL_LOC("Unknown form action.");
    }

    cupsFreeOptions(num_form, form);
  }

  // Show the Wi-Fi configuration
  system_header(client, _PAPPL_LOC("Wi-Fi Configuration"));

  if (status)
    papplClientHTMLPrintf(client, "<div class=\"banner\">%s</div>\n", papplClientGetLocString(client, status));

  papplClientHTMLStartForm(client, client->uri, false);
  papplClientHTMLPrintf(client,
			"          <table class=\"form\">\n"
			"            <tbody>\n"
			"              <tr><th><label for=\"ssid\">%s:</label></th><td><select name=\"ssid\"><option value=\"\">%s</option>", papplClientGetLocString(client, _PAPPL_LOC("Network")), papplClientGetLocString(client, _PAPPL_LOC("Choose")));

  num_ssids = (cups_len_t)(system->wifi_list_cb)(system, system->wifi_cbdata, &ssids);
  for (i = 0; i < num_ssids; i ++)
    papplClientHTMLPrintf(client, "<option%s>%s</option>", ssids[i].is_default ? " selected" : "", ssids[i].name);
  cupsFreeDests(num_ssids, ssids);

  papplClientHTMLPrintf(client,
			"</select> <a class=\"btn\" href=\"/network-wifi\">%s</a></td></tr>\n"
			"              <tr><th><label for=\"psk\">%s:</label></th><td><input type=\"password\" name=\"psk\"></td></tr>\n"
			"              <tr><th></th><td><input type=\"submit\" value=\"%s\"></td></tr>\n"
			"            </tbody>\n"
			"          </table>\n"
			"        </form>\n", papplClientGetLocString(client, _PAPPL_LOC("Rescan")), papplClientGetLocString(client, _PAPPL_LOC("Password")), papplClientGetLocString(client, _PAPPL_LOC("Join Wi-Fi Network")));

  system_footer(client);
}


//
// 'get_networks()' - Get the list of available networks.
//
// This is the default implementation of the @link pappl_network_get_cb_t@
// function that just gets the current network interface status without respect
// to whatever network management software is in use.
//

static size_t				// O - Actual networks
get_networks(
    size_t          max_networks,	// I - Maximum number of networks
    pappl_network_t *networks)		// I - Networks
{
#if _WIN32
  // TODO: Implement network interface lookups for Windows...
  return (0);

#else
  size_t	i,			// Looping var
		num_networks = 0;	// Number of networks
  struct ifaddrs *addrs,		// List of network addresses
		*addr;			// Current network address
  pappl_network_t *network;		// Current network


  memset(networks, 0, max_networks * sizeof(pappl_network_t));

  if (getifaddrs(&addrs))
    return (0);

  for (addr = addrs; addr; addr = addr->ifa_next)
  {
    // Skip loopback and point-to-point interfaces...
    if (addr->ifa_name == NULL || addr->ifa_addr == NULL || (addr->ifa_addr->sa_family != AF_INET && addr->ifa_addr->sa_family != AF_INET6) || (addr->ifa_flags & (IFF_LOOPBACK | IFF_POINTOPOINT)) || !strncmp(addr->ifa_name, "awdl", 4))
      continue;

    // Find the interface in the list...
    for (i = num_networks, network = networks; i > 0; i --, network ++)
    {
      if (!strcmp(network->name, addr->ifa_name))
        break;
    }

    if (i == 0)
    {
      // Not found, add it or skip it...
      if (num_networks < max_networks)
      {
        network = networks + num_networks;
        num_networks ++;
        papplCopyString(network->name, addr->ifa_name, sizeof(network->name));
        papplCopyString(network->ident, addr->ifa_name, sizeof(network->ident));
        network->up = (addr->ifa_flags & IFF_UP) != 0;
      }
      else
      {
        continue;
      }
    }

    // Now assign the address information...
    if (addr->ifa_addr->sa_family == AF_INET)
    {
      // IPv4
      unsigned ipv4 = ntohl(((struct sockaddr_in *)addr->ifa_addr)->sin_addr.s_addr);
					// IPv4 address

      network->addr4.ipv4 = *((struct sockaddr_in *)addr->ifa_addr);
      network->mask4.ipv4 = *((struct sockaddr_in *)addr->ifa_netmask);

      // Assume default router is first node in subnet...
      network->gateway4                 = network->addr4;
      network->gateway4.ipv4.sin_addr.s_addr = (network->gateway4.ipv4.sin_addr.s_addr & network->mask4.ipv4.sin_addr.s_addr) | htonl(1);

      if ((ipv4 & 0xff000000) == 0x0a000000 || (ipv4 & 0xfff00000) == 0xac100000 || (ipv4 & 0xffff0000) == 0xc0a80000)
      {
        // Private use 10.*, 172.16.*, or 192.168.* so this is likely DHCP-assigned
        if ((ipv4 & 255) < 200)
          network->config = PAPPL_NETCONF_DHCP;
	else
          network->config = PAPPL_NETCONF_DHCP_MANUAL;
      }
      else
      {
        // Otherwise assume manual configuration...
        network->config = PAPPL_NETCONF_MANUAL;
      }
    }
    else
    {
      // IPv6
      if (IN6_IS_ADDR_LINKLOCAL(&((struct sockaddr_in6 *)addr->ifa_addr)->sin6_addr))
      {
        // Save link-local address...
        network->linkaddr6.ipv6 = *((struct sockaddr_in6 *)addr->ifa_addr);
      }
      else
      {
        // Save routable address...
        struct sockaddr_in6 *netmask6 = (struct sockaddr_in6 *)addr->ifa_netmask;

        network->addr6.ipv6 = *((struct sockaddr_in6 *)addr->ifa_addr);
        for (network->prefix6 = 0, i = 0; i < 16; i ++)
        {
          switch (netmask6->sin6_addr.s6_addr[i])
          {
            case 0xff :
                network->prefix6 += 8;
                break;
            case 0xfe :
                network->prefix6 += 7;
                break;
            case 0xfc :
                network->prefix6 += 6;
                break;
            case 0xf8 :
                network->prefix6 += 5;
                break;
            case 0xf0 :
                network->prefix6 += 4;
                break;
            case 0xe0 :
                network->prefix6 += 3;
                break;
            case 0xc0 :
                network->prefix6 += 2;
                break;
            case 0x80 :
                network->prefix6 += 1;
                break;
            default :
                break;
          }

          if (netmask6->sin6_addr.s6_addr[i] < 0xff)
            break;
        }
      }
    }
  }

  freeifaddrs(addrs);

  // Return the number of networks we found...
  return (num_networks);
#endif // _WIN32
}


//
// 'system_device_cb()' - Device callback for the "add printer" chooser.
//

static bool				// O - `true` to stop, `false` to continue
system_device_cb(
    const char *device_info,		// I - Device description
    const char *device_uri,		// I - Device URI
    const char *device_id,		// I - IEEE-1284 device ID
    void       *data)			// I - Callback data (client + device URI)
{
  _pappl_system_dev_t *devdata = (_pappl_system_dev_t *)data;
					// Callback data


  papplClientHTMLPrintf(devdata->client, "<option value=\"%s|%s\"%s>%s</option>", device_uri, device_id, !strcmp(devdata->device_uri, device_uri) ? " selected" : "", device_info);

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
  char	text[1024];			// Localized version number


  if (!papplClientRespond(client, HTTP_STATUS_OK, NULL, "text/html", 0, 0))
    return;

  papplClientHTMLHeader(client, title, 0);

  if (client->system->versions[0].sversion[0])
  {
    papplLocFormatString(papplClientGetLoc(client), text, sizeof(text), _PAPPL_LOC("Version %s"), client->system->versions[0].sversion);
    papplClientHTMLPrintf(client,
			  "    <div class=\"header2\">\n"
			  "      <div class=\"row\">\n"
			  "        <div class=\"col-12 nav\">\n"
			  "          %s\n"
			  "        </div>\n"
			  "      </div>\n"
			  "    </div>\n", text);
  }

  papplClientHTMLPuts(client, "    <div class=\"content\">\n");

  if (title)
    papplClientHTMLPrintf(client,
			  "      <div class=\"row\">\n"
			  "        <div class=\"col-12\">\n"
			  "          <h1 class=\"title\">%s</h1>\n", papplClientGetLocString(client, title));
}


#if defined(HAVE_OPENSSL) || defined(HAVE_GNUTLS)
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
#  ifdef HAVE_OPENSSL
#  else
  gnutls_certificate_credentials_t *credentials;
					// TLS credentials
  int		status;			// Status for loading of credentials
#  endif // HAVE_OPENSSL


#  ifdef HAVE_OPENSSL
#  else
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
#  endif // HAVE_OPENSSL

  // If everything checks out, copy the certificate and private key to the
  // CUPS "ssl" directory...
  home = getuid() ? getenv("HOME") : NULL;
  if (home)
    snprintf(basedir, sizeof(basedir), "%s/.cups", home);
  else
    papplCopyString(basedir, CUPS_SERVERROOT, sizeof(basedir));

  if (access(basedir, X_OK))
  {
    // Make "~/.cups" or "CUPS_SERVERROOT" directory...
    if (mkdir(basedir, 0755))
    {
      papplLogClient(client, PAPPL_LOGLEVEL_ERROR, "Unable to create directory '%s': %s", basedir, strerror(errno));
      return (false);
    }
  }

  snprintf(ssldir, sizeof(ssldir), "%s/ssl", basedir);
  if (access(ssldir, X_OK))
  {
    // Make "~/.cups/ssl" or "CUPS_SERVERROOT/ssl" directory...
    if (mkdir(ssldir, 0755))
    {
      papplLogClient(client, PAPPL_LOGLEVEL_ERROR, "Unable to create directory '%s': %s", ssldir, strerror(errno));
      return (false);
    }
  }

  snprintf(dstkey, sizeof(dstkey), "%s/%s.key", ssldir, papplSystemGetHostName(system, hostname, sizeof(hostname)));
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
#if CUPS_VERSION_MAJOR < 3
    if (cupsFileWrite(dstfile, buffer, (size_t)bytes) < 0)
#else
    if (!cupsFileWrite(dstfile, buffer, (size_t)bytes))
#endif // CUPS_VERSION_MAJOR < 3
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
    cups_len_t     num_form,		// I - Number of form variables
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
#  ifdef HAVE_OPENSSL
  bool		result = false;		// Result of operations
  EVP_PKEY	*pkey;			// Private key
  BIGNUM	*rsaexp;		// Public exponent for RSA keys
  RSA		*rsa = NULL;		// RSA key pair
  EC_KEY	*ecdsa = NULL;		// ECDSA key pair
  X509		*cert;			// Certificate
  char		dns_name[1024];		// DNS: prefixed hostname
  X509_EXTENSION *san_ext;		// Extension for subjectAltName
  ASN1_OCTET_STRING *san_asn1;		// ASN1 string
  time_t	curtime;		// Current time
  X509_NAME	*name;			// Subject/issuer name
  BIO		*bio;			// Output file
#  else // HAVE_GNUTLS
  gnutls_x509_crt_t crt;		// Self-signed certificate
  gnutls_x509_privkey_t key;		// Private/public key pair
  cups_file_t	*fp;			// Key/cert file
  unsigned char	buffer[8192];		// Buffer for key/cert data
  size_t	bytes;			// Number of bytes of data
  unsigned char	serial[4];		// Serial number buffer
  int		status;			// GNU TLS status
#  endif // HAVE_OPENSSL


  // Verify that we have all of the required form variables...
  if ((value = cupsGetOption("duration", num_form, form)) == NULL)
  {
    papplLogClient(client, PAPPL_LOGLEVEL_ERROR, "Missing 'duration' form field.");
    return (false);
  }
  else if ((duration = (int)strtol(value, NULL, 10)) < 1 || duration > 10)
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
  papplSystemGetHostName(system, hostname, sizeof(hostname));
  if ((domain = strchr(hostname, '.')) != NULL)
  {
    // If the domain name is not hostname.local or hostname.lan, make that the
    // second Subject Alternate Name...
    if (strcmp(domain, ".local") && strcmp(domain, ".lan"))
      papplCopyString(alt_names[num_alt_names ++], hostname, sizeof(alt_names[0]));

    *domain = '\0';
  }

  // then add hostname as the first alternate name...
  papplCopyString(alt_names[0], hostname, sizeof(alt_names[0]));

  // and finish up with hostname.lan and hostname.local as the final alternates...
  snprintf(alt_names[num_alt_names ++], sizeof(alt_names[0]), "%s.lan", hostname);
  snprintf(alt_names[num_alt_names ++], sizeof(alt_names[0]), "%s.local", hostname);

  // Store the certificate and private key in the CUPS "ssl" directory...
  home = getuid() ? getenv("HOME") : NULL;
  if (home)
    snprintf(basedir, sizeof(basedir), "%s/.cups", home);
  else
    papplCopyString(basedir, CUPS_SERVERROOT, sizeof(basedir));

  if (access(basedir, X_OK))
  {
    // Make "~/.cups" or "CUPS_SERVERROOT" directory...
    if (mkdir(basedir, 0755))
    {
      papplLogClient(client, PAPPL_LOGLEVEL_ERROR, "Unable to create directory '%s': %s", basedir, strerror(errno));
      return (false);
    }
  }

  snprintf(ssldir, sizeof(ssldir), "%s/ssl", basedir);
  if (access(ssldir, X_OK))
  {
    // Make "~/.cups/ssl" or "CUPS_SERVERROOT/ssl" directory...
    if (mkdir(ssldir, 0755))
    {
      papplLogClient(client, PAPPL_LOGLEVEL_ERROR, "Unable to create directory '%s': %s", ssldir, strerror(errno));
      return (false);
    }
  }

  snprintf(keyfile, sizeof(keyfile), "%s/%s.key", ssldir, hostname);
  snprintf(crtfile, sizeof(crtfile), "%s/%s.crt", ssldir, hostname);

  papplLogClient(client, PAPPL_LOGLEVEL_DEBUG, "Creating crtfile='%s', keyfile='%s'.", crtfile, keyfile);

#  ifdef HAVE_OPENSSL
  // Create the paired encryption keys...
  if ((pkey = EVP_PKEY_new()) == NULL)
  {
    papplLogClient(client, PAPPL_LOGLEVEL_ERROR, "Unable to create private key.");
    return (false);
  }

  if (!strcmp(level, "rsa-2048"))
  {
    // 2048-bit RSA key...
    rsaexp = BN_new();
    BN_set_word(rsaexp, RSA_F4);
    rsa = RSA_new();
    RSA_generate_key_ex(rsa, 2048, rsaexp, NULL);
    BN_free(rsaexp);
  }
  else if (!strcmp(level, "rsa-4096"))
  {
    // 4096-bit RSA key...
    rsaexp = BN_new();
    BN_set_word(rsaexp, RSA_F4);
    rsa = RSA_new();
    RSA_generate_key_ex(rsa, 4096, rsaexp, NULL);
    BN_free(rsaexp);
  }
  else
  {
    // 384-bit ECDSA key...
    ecdsa = EC_KEY_new_by_curve_name(NID_secp384r1);
  }

  if (!rsa && !ecdsa)
  {
    papplLogClient(client, PAPPL_LOGLEVEL_ERROR, "Unable to create RSA/ECDSA key pair.");
    return (false);
  }

  if (rsa)
    EVP_PKEY_assign_RSA(pkey, rsa);
  else
    EVP_PKEY_assign_EC_KEY(pkey, ecdsa);

  // Create the self-signed certificate...
  if ((cert = X509_new()) == NULL)
  {
    EVP_PKEY_free(pkey);
    papplLogClient(client, PAPPL_LOGLEVEL_ERROR, "Unable to create X.509 certificate.");
    return (false);
  }

  curtime  = time(NULL);

  ASN1_TIME_set(X509_get_notBefore(cert), curtime);
  ASN1_TIME_set(X509_get_notAfter(cert), curtime * duration * 365 * 86400);
  ASN1_INTEGER_set(X509_get_serialNumber(cert), (int)curtime);
  X509_set_pubkey(cert, pkey);

  name = X509_get_subject_name(cert);
  X509_NAME_add_entry_by_txt(name, "C", MBSTRING_ASC, (unsigned char *)country, -1, -1, 0);
  X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC, (unsigned char *)hostname, -1, -1, 0);
  X509_NAME_add_entry_by_txt(name, "emailAddress", MBSTRING_ASC, (unsigned char *)email, -1, -1, 0);
  X509_NAME_add_entry_by_txt(name, "L", MBSTRING_ASC, (unsigned char *)city, -1, -1, 0);
  X509_NAME_add_entry_by_txt(name, "O", MBSTRING_ASC, (unsigned char *)organization, -1, -1, 0);
  X509_NAME_add_entry_by_txt(name, "OU", MBSTRING_ASC, (unsigned char *)org_unit, -1, -1, 0);
  X509_NAME_add_entry_by_txt(name, "ST", MBSTRING_ASC, (unsigned char *)state, -1, -1, 0);

  X509_set_issuer_name(cert, name);

  for (i = 0; i < num_alt_names; i ++)
  {
    // The subjectAltName value for DNS names starts with a DNS: prefix...
    snprintf(dns_name, sizeof(dns_name), "DNS: %s", alt_names[i]);

    if ((san_asn1 = ASN1_OCTET_STRING_new()) == NULL)
      break;

    ASN1_OCTET_STRING_set(san_asn1, (unsigned char *)dns_name, (int)strlen(dns_name));
    if (!X509_EXTENSION_create_by_NID(&san_ext, NID_subject_alt_name, 0, san_asn1))
    {
      ASN1_OCTET_STRING_free(san_asn1);
      break;
    }

    X509_add_ext(cert, san_ext, -1);
    X509_EXTENSION_free(san_ext);
    ASN1_OCTET_STRING_free(san_asn1);
  }

  X509_sign(cert, pkey, EVP_sha256());

  // Save them...
  if ((bio = BIO_new_file(keyfile, "wb")) == NULL)
  {
    papplLogClient(client, PAPPL_LOGLEVEL_ERROR, "Unable to create key file '%s': %s", keyfile, strerror(errno));
    goto done;
  }

  if (!PEM_write_bio_PrivateKey(bio, pkey, NULL, NULL, 0, NULL, NULL))
  {
    papplLogClient(client, PAPPL_LOGLEVEL_ERROR, "Unable to write key file '%s': %s", keyfile, strerror(errno));
    BIO_free(bio);
    goto done;
  }

  BIO_free(bio);

  if ((bio = BIO_new_file(crtfile, "wb")) == NULL)
  {
    papplLogClient(client, PAPPL_LOGLEVEL_ERROR, "Unable to create certificate file '%s': %s", crtfile, strerror(errno));
    goto done;
  }

  if (!PEM_write_bio_X509(bio, cert))
  {
    BIO_free(bio);
    papplLogClient(client, PAPPL_LOGLEVEL_ERROR, "Unable to write certificate file '%s': %s", crtfile, strerror(errno));
    goto done;
  }

  BIO_free(bio);

  result = true;

  // Cleanup...
  done:

  X509_free(cert);
  EVP_PKEY_free(pkey);

  if (!result)
    return (false);


#  else // HAVE_GNUTLS
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
  serial[0] = (unsigned char)(i >> 24);
  serial[1] = (unsigned char)(i >> 16);
  serial[2] = (unsigned char)(i >> 8);
  serial[3] = (unsigned char)i;

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

  // If we get this far we are done!

  gnutls_x509_crt_deinit(crt);
  gnutls_x509_privkey_deinit(key);
#  endif // HAVE_OPENSSL

  // Create symlinks for each of the alternate names...
#  if _WIN32
#    define symlink(src,dst) CreateSymbolicLinkA(dst,src,SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE)
#  endif // _WIN32

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

  return (true);
}


//
// 'tls_make_certsignreq()' - Make a certificate signing request and private key.
//

static bool				// O - `true` on success, `false` otherwise
tls_make_certsignreq(
    pappl_client_t *client,		// I - Client
    cups_len_t     num_form,		// I - Number of form variables
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
#  ifdef HAVE_OPENSSL
  bool		result = false;		// Result of operations
  EVP_PKEY	*pkey;			// Private key
  BIGNUM	*rsaexp;		// Public exponent for RSA keys
  RSA		*rsa = NULL;		// RSA key pair
  EC_KEY	*ecdsa = NULL;		// ECDSA key pair
  X509_REQ	*crq;			// Certificate request
  char		dns_name[1024];		// DNS: prefixed hostname
  STACK_OF(X509_EXTENSION) *san_exts;	// Extensions
  X509_EXTENSION *san_ext;		// Extension for subjectAltName
  ASN1_OCTET_STRING *san_asn1;		// ASN1 string
  X509_NAME	*name;			// Subject/issuer name
  BIO		*bio;			// Output file
#  else
  gnutls_x509_crq_t crq;		// Certificate request
  gnutls_x509_privkey_t key;		// Private/public key pair
  cups_file_t	*fp;			// Key/cert file
  unsigned char	buffer[8192];		// Buffer for key/cert data
  size_t	bytes;			// Number of bytes of data
  int		status;			// GNU TLS status
#  endif // HAVE_OPENSSL


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
  snprintf(keyfile, sizeof(keyfile), "%s/%s.key", system->directory, papplSystemGetHostName(system, hostname, sizeof(hostname)));
  snprintf(crqfile, sizeof(crqfile), "%s/%s.csr", system->directory, hostname);
  snprintf(crqpath, crqsize, "/%s.csr", hostname);

#  ifdef HAVE_OPENSSL
  // Create the paired encryption keys...
  if ((pkey = EVP_PKEY_new()) == NULL)
  {
    papplLogClient(client, PAPPL_LOGLEVEL_ERROR, "Unable to create private key.");
    return (false);
  }

  if (!strcmp(level, "rsa-2048"))
  {
    // 2048-bit RSA key...
    rsaexp = BN_new();
    BN_set_word(rsaexp, RSA_F4);
    rsa = RSA_new();
    RSA_generate_key_ex(rsa, 2048, rsaexp, NULL);
    BN_free(rsaexp);
  }
  else if (!strcmp(level, "rsa-4096"))
  {
    // 4096-bit RSA key...
    rsaexp = BN_new();
    BN_set_word(rsaexp, RSA_F4);
    rsa = RSA_new();
    RSA_generate_key_ex(rsa, 4096, rsaexp, NULL);
    BN_free(rsaexp);
  }
  else
  {
    // 384-bit ECDSA key...
    ecdsa = EC_KEY_new_by_curve_name(NID_secp384r1);
  }

  if (!rsa && !ecdsa)
  {
    papplLogClient(client, PAPPL_LOGLEVEL_ERROR, "Unable to create RSA/ECDSA key pair.");
    return (false);
  }

  if (rsa)
    EVP_PKEY_assign_RSA(pkey, rsa);
  else
    EVP_PKEY_assign_EC_KEY(pkey, ecdsa);

  // Create the certificate request...
  if ((crq = X509_REQ_new()) == NULL)
  {
    EVP_PKEY_free(pkey);
    papplLogClient(client, PAPPL_LOGLEVEL_ERROR, "Unable to create X.509 certificate request.");
    return (false);
  }

  X509_REQ_set_pubkey(crq, pkey);

  name = X509_REQ_get_subject_name(crq);
  X509_NAME_add_entry_by_txt(name, "C", MBSTRING_ASC, (unsigned char *)country, -1, -1, 0);
  X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC, (unsigned char *)hostname, -1, -1, 0);
  X509_NAME_add_entry_by_txt(name, "emailAddress", MBSTRING_ASC, (unsigned char *)email, -1, -1, 0);
  X509_NAME_add_entry_by_txt(name, "L", MBSTRING_ASC, (unsigned char *)city, -1, -1, 0);
  X509_NAME_add_entry_by_txt(name, "O", MBSTRING_ASC, (unsigned char *)organization, -1, -1, 0);
  X509_NAME_add_entry_by_txt(name, "OU", MBSTRING_ASC, (unsigned char *)org_unit, -1, -1, 0);
  X509_NAME_add_entry_by_txt(name, "ST", MBSTRING_ASC, (unsigned char *)state, -1, -1, 0);

  // The subjectAltName value for DNS names starts with a DNS: prefix...
  snprintf(dns_name, sizeof(dns_name), "DNS: %s", hostname);

  if ((san_asn1 = ASN1_OCTET_STRING_new()) == NULL)
    goto done;

  ASN1_OCTET_STRING_set(san_asn1, (unsigned char *)dns_name, (int)strlen(dns_name));
  if (!X509_EXTENSION_create_by_NID(&san_ext, NID_subject_alt_name, 0, san_asn1))
  {
    ASN1_OCTET_STRING_free(san_asn1);
    goto done;
  }

  if ((san_exts = sk_X509_EXTENSION_new_null()) != NULL)
  {
    sk_X509_EXTENSION_push(san_exts, san_ext);
    X509_REQ_add_extensions(crq, san_exts);
    sk_X509_EXTENSION_free(san_exts);
  }

  X509_EXTENSION_free(san_ext);
  ASN1_OCTET_STRING_free(san_asn1);

  X509_REQ_sign(crq, pkey, EVP_sha256());

  // Save them...
  if ((bio = BIO_new_file(keyfile, "wb")) == NULL)
  {
    papplLogClient(client, PAPPL_LOGLEVEL_ERROR, "Unable to create key file '%s': %s", keyfile, strerror(errno));
    goto done;
  }

  if (!PEM_write_bio_PrivateKey(bio, pkey, NULL, NULL, 0, NULL, NULL))
  {
    papplLogClient(client, PAPPL_LOGLEVEL_ERROR, "Unable to write key file '%s': %s", keyfile, strerror(errno));
    BIO_free(bio);
    goto done;
  }

  BIO_free(bio);

  if ((bio = BIO_new_file(crqfile, "wb")) == NULL)
  {
    papplLogClient(client, PAPPL_LOGLEVEL_ERROR, "Unable to create certificate request file '%s': %s", crqfile, strerror(errno));
    goto done;
  }

  if (!PEM_write_bio_X509_REQ(bio, crq))
  {
    BIO_free(bio);
    papplLogClient(client, PAPPL_LOGLEVEL_ERROR, "Unable to write certificate request file '%s': %s", crqfile, strerror(errno));
    goto done;
  }

  BIO_free(bio);

  result = true;

  // Cleanup...
  done:

  X509_REQ_free(crq);
  EVP_PKEY_free(pkey);

  if (!result)
    return (false);

#  else
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

  gnutls_x509_crq_deinit(crq);
  gnutls_x509_privkey_deinit(key);
#  endif // HAVE_OPENSSL

  // If we get this far we are done!
  papplSystemAddResourceFile(system, crqpath, "application/pkcs10", crqfile);

  return (true);
}
#endif // HAVE_OPENSSL || HAVE_GNUTLS
