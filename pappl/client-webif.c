//
// Core client web interface functions for the Printer Application Framework
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
#include <math.h>


//
// 'papplClientGetForm()' - Get POST form data from the web client.
//

int					// O - Number of form variables read
papplClientGetForm(
    pappl_client_t *client,		// I - Client
    cups_option_t  **form)		// O - Form variables
{
  int		num_form = 0;		// Number of form variables
  char		body[8192],		// Message body data string
		*ptr,			// Pointer into string
		name[64],		// Variable name
		*nameptr,		// Pointer into name
		value[1024],		// Variable value
		*valptr;		// Pointer into value
  ssize_t	bytes;			// Bytes read
  http_state_t	initial_state;		// Initial HTTP state


  // Read form data...
  initial_state = httpGetState(client->http);

  for (ptr = body; ptr < (body + sizeof(body) - 1); ptr += bytes)
  {
    if ((bytes = httpRead2(client->http, ptr, sizeof(body) - (size_t)(ptr - body + 1))) <= 0)
      break;
  }

  *ptr = '\0';

  if (httpGetState(client->http) == initial_state)
    httpFlush(client->http);		// Flush remainder...

  // Parse the form data...
  *form = NULL;

  for (ptr = body; *ptr;)
  {
    // Get the name...
    nameptr = name;
    while (*ptr && *ptr != '=')
    {
      int ch = *ptr++;			// Name character

      if (ch == '%' && isxdigit(ptr[0] & 255) && isxdigit(ptr[1] & 255))
      {
        // Hex-encoded character
        if (isdigit(*ptr))
          ch = (*ptr++ - '0') << 4;
	else
	  ch = (tolower(*ptr++) - 'a' + 10) << 4;

        if (isdigit(*ptr))
          ch |= *ptr++ - '0';
	else
	  ch |= tolower(*ptr++) - 'a' + 10;
      }
      else if (ch == '+')
        ch = ' ';

      if (nameptr < (name + sizeof(name) - 1))
        *nameptr++ = ch;
    }
    *nameptr = '\0';

    if (!*ptr)
      break;

    // Get the value...
    ptr ++;
    valptr = value;
    while (*ptr && *ptr != '&')
    {
      int ch = *ptr++;			// Name character

      if (ch == '%' && isxdigit(ptr[0] & 255) && isxdigit(ptr[1] & 255))
      {
        // Hex-encoded character
        if (isdigit(*ptr))
          ch = (*ptr++ - '0') << 4;
	else
	  ch = (tolower(*ptr++) - 'a' + 10) << 4;

        if (isdigit(*ptr))
          ch |= *ptr++ - '0';
	else
	  ch |= tolower(*ptr++) - 'a' + 10;
      }
      else if (ch == '+')
        ch = ' ';

      if (valptr < (value + sizeof(value) - 1))
        *valptr++ = ch;
    }
    *valptr = '\0';

    if (*ptr)
      ptr ++;

    // Add the name + value to the option array...
    num_form = cupsAddOption(name, value, num_form, form);
  }

  return (num_form);
}


//
// 'papplClientHTMLEscape()' - Write a HTML-safe string.
//

void
papplClientHTMLEscape(
    pappl_client_t *client,		// I - Client
    const char     *s,			// I - String to write
    size_t         slen)		// I - Number of characters to write (`0` for nul-terminated)
{
  const char	*start,			// Start of segment
		*end;			// End of string


  start = s;
  end   = s + (slen > 0 ? slen : strlen(s));

  while (*s && s < end)
  {
    if (*s == '&' || *s == '<' || *s == '\"')
    {
      if (s > start)
        httpWrite2(client->http, start, (size_t)(s - start));

      if (*s == '&')
        httpWrite2(client->http, "&amp;", 5);
      else if (*s == '<')
        httpWrite2(client->http, "&lt;", 5);
      else
        httpWrite2(client->http, "&quot;", 4);

      start = s + 1;
    }

    s ++;
  }

  if (s > start)
    httpWrite2(client->http, start, (size_t)(s - start));
}


//
// 'papplClientHTMLFooter()' - Show the web interface footer.
//
// This function also writes the trailing 0-length chunk.
//

void
papplClientHTMLFooter(
    pappl_client_t *client)		// I - Client
{
  const char *footer = papplSystemGetFooterHTML(papplClientGetSystem(client));
					// Footer HTML

  if (footer)
  {
    papplClientHTMLPuts(client,
                        "    <div class=\"footer\">\n"
                        "      <div class=\"row\">\n"
                        "        <div class=\"col-12\">");
    papplClientHTMLPuts(client, footer);
    papplClientHTMLPuts(client,
                        "</div>\n"
                        "      </div>\n"
                        "    </div>\n");
  }

  papplClientHTMLPuts(client,
		      "  </body>\n"
		      "</html>\n");
  httpWrite2(client->http, "", 0);
}


//
// 'papplClientHTMLHeader()' - Show the web interface header and title.
//

void
papplClientHTMLHeader(
    pappl_client_t *client,		// I - Client
    const char     *title,		// I - Title
    int            refresh)		// I - Refresh timer, if any
{
  pappl_system_t	*system = client->system;
					// System
  pappl_printer_t	*printer;	// Printer
  _pappl_resource_t	*r;		// Current resource
  const char		*name;		// Name for title/header


  if ((system->options & PAPPL_SOPTIONS_MULTI_QUEUE) || (printer = (pappl_printer_t *)cupsArrayFirst(system->printers)) == NULL)
    name = system->name;
  else
    name = printer->name;

  papplClientHTMLPrintf(client,
			"<!DOCTYPE html>\n"
			"<html>\n"
			"  <head>\n"
			"    <title>%s%s%s</title>\n"
			"    <link rel=\"shortcut icon\" href=\"/favicon.png\" type=\"image/png\">\n"
			"    <link rel=\"stylesheet\" href=\"/style.css\">\n"
			"    <meta http-equiv=\"X-UA-Compatible\" content=\"IE=9\">\n", title ? title : "", title ? " - " : "", name);
  if (refresh > 0)
    papplClientHTMLPrintf(client, "<meta http-equiv=\"refresh\" content=\"%d\">\n", refresh);
  papplClientHTMLPrintf(client,
			"    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n"
			"  </head>\n"
			"  <body>\n"
			"    <div class=\"header\">\n"
			"      <div class=\"row\">\n"
			"        <div class=\"col-12 nav\">\n"
			"          <a class=\"btn\" href=\"/\"><img src=\"/navicon.png\"> %s</a>\n", name);

  pthread_rwlock_rdlock(&system->rwlock);

  for (r = (_pappl_resource_t *)cupsArrayFirst(system->resources); r; r = (_pappl_resource_t *)cupsArrayNext(system->resources))
  {
    if (r->label)
    {
      if (strcmp(client->uri, r->path))
      {
        if (r->secure)
          papplClientHTMLPrintf(client, "          <a class=\"btn\" href=\"https://%s:%d%s\">%s</a>\n", client->host_field, client->host_port, r->path, r->label);
	else
          papplClientHTMLPrintf(client, "          <a class=\"btn\" href=\"%s\">%s</a>\n", r->path, r->label);
      }
      else
        papplClientHTMLPrintf(client, "          <span class=\"active\">%s</span>\n", r->label);
    }
  }

  pthread_rwlock_unlock(&system->rwlock);

  papplClientHTMLPuts(client,
		      "        </div>\n"
		      "      </div>\n"
		      "    </div>\n");
}


//
// '_papplCLientHTMLInfo()' - Show system/printer information.
//

void
_papplClientHTMLInfo(
    pappl_client_t  *client,		// I - Client
    bool            is_form,		// I - `true` to show a form, `false` otherwise
    const char      *dns_sd_name,	// I - DNS-SD name, if any
    const char      *location,		// I - Location, if any
    const char      *geo_location,	// I - Geo-location, if any
    const char      *organization,	// I - Organization, if any
    const char      *org_unit,		// I - Organizational unit, if any
    pappl_contact_t *contact)		// I - Contact, if any
{
  double	lat = 0.0, lon = 0.0;	// Latitude and longitude in degrees


  if (geo_location)
    sscanf(geo_location, "geo:%lf,%lf", &lat, &lon);

  if (is_form)
    papplClientHTMLStartForm(client, client->uri, false);

  // DNS-SD name...
  papplClientHTMLPuts(client,
		      "          <table class=\"form\">\n"
		      "            <tbody>\n"
		      "              <tr><th>Name:</th><td>");
  if (is_form)
    papplClientHTMLPrintf(client, "<input type=\"text\" name=\"dns_sd_name\" value=\"%s\" placeholder=\"DNS-SD Service Name\">", dns_sd_name ? dns_sd_name : "");
  else
    papplClientHTMLEscape(client, dns_sd_name ? dns_sd_name : "Not set", 0);

  // Location and geo-location...
  papplClientHTMLPuts(client,
                      "</td></tr>\n"
		      "              <tr><th>Location:</th><td>");
  if (is_form)
  {
    papplClientHTMLPrintf(client,
                          "<input type=\"text\" name=\"location\" value=\"%s\" placeholder=\"Human-Readable Location\"><br>\n"
                          "<input type=\"number\" name=\"geo_location_lat\" min=\"-90\" max=\"90\" step=\"0.0001\" value=\"%.4f\" onChange=\"updateMap();\">&nbsp;&deg;&nbsp;latitude x <input type=\"number\" name=\"geo_location_lon\" min=\"-180\" max=\"180\" step=\"0.0001\" value=\"%.4f\" onChange=\"updateMap();\">&nbsp;&deg;&nbsp;longitude <button id=\"geo_location_lookup\" onClick=\"event.preventDefault(); navigator.geolocation.getCurrentPosition(setGeoLocation);\">Use My Position</button>", location ? location : "", lat, lon);
  }
  else
  {
    papplClientHTMLPrintf(client, "%s", location ? location : "Not set");
    if (geo_location)
      papplClientHTMLPrintf(client, "<br>\n%g&deg;&nbsp;%c&nbsp;latitude x %g&deg;&nbsp;%c&nbsp;longitude", fabs(lat), lat < 0.0 ? 'S' : 'N', fabs(lon), lon < 0.0 ? 'W' : 'E');
  }

  // Show an embedded map of the location...
  if (geo_location || is_form)
    papplClientHTMLPrintf(client,
			  "<br>\n"
			  "<iframe id=\"map\" frameborder=\"0\" scrolling=\"no\" marginheight=\"0\" marginwidth=\"0\" src=\"https://www.openstreetmap.org/export/embed.html?bbox=%g,%g,%g,%g&amp;layer=mapnik&amp;marker=%g,%g\"></iframe>", lon - 0.00025, lat - 0.00025, lon + 0.00025, lat + 0.00025, lat, lon);

  // Organization
  papplClientHTMLPuts(client,
                      "</td></tr>\n"
		      "              <tr><th>Organization:</th><td>");

  if (is_form)
    papplClientHTMLPrintf(client,
                          "<input type=\"text\" name=\"organization\" placeholder=\"Organization Name\" value=\"%s\"><br>\n"
                          "<input type=\"text\" name=\"organizational_unit\" placeholder=\"Organizational Unit\" value=\"%s\">", organization ? organization : "", org_unit ? org_unit : "");
  else
    papplClientHTMLPrintf(client, "%s%s%s", organization ? organization : "Unknown", org_unit ? ", " : "", org_unit ? org_unit : "");

  // Contact
  papplClientHTMLPuts(client,
                      "</td></tr>\n"
                      "              <tr><th>Contact:</th><td>");

  if (is_form)
  {
    papplClientHTMLPrintf(client,
                          "<input type=\"text\" name=\"contact_name\" placeholder=\"Name\" value=\"%s\"><br>\n"
                          "<input type=\"email\" name=\"contact_email\" placeholder=\"name@domain\" value=\"%s\"><br>\n"
                          "<input type=\"tel\" name=\"contact_telephone\" placeholder=\"867-5309\" value=\"%s\"></td></tr>\n"
		      "              <tr><th></th><td><input type=\"submit\" value=\"Save Changes\">", contact->name, contact->email, contact->telephone);
  }
  else if (contact->email[0])
  {
    papplClientHTMLPrintf(client, "<a href=\"mailto:%s\">%s</a>", contact->email, contact->name[0] ? contact->name : contact->email);

    if (contact->telephone[0])
      papplClientHTMLPrintf(client, "<br><a href=\"tel:%s\">%s</a>", contact->telephone, contact->telephone);
  }
  else if (contact->name[0])
  {
    papplClientHTMLEscape(client, contact->name, 0);

    if (contact->telephone[0])
      papplClientHTMLPrintf(client, "<br><a href=\"tel:%s\">%s</a>", contact->telephone, contact->telephone);
  }
  else if (contact->telephone[0])
  {
    papplClientHTMLPrintf(client, "<a href=\"tel:%s\">%s</a>", contact->telephone, contact->telephone);
  }
  else
    papplClientHTMLPuts(client, "Not set");

  papplClientHTMLPuts(client,
		      "</td></tr>\n"
		      "            </tbody>\n"
		      "          </table>\n");

  if (is_form)
  {
    // The following Javascript updates the map and lat/lon fields.
    //
    // Note: I should probably use the Openstreetmap Javascript API so that
    // the marker position gets updated.  Right now I'm setting the marker
    // value in the URL but the OSM simple embedding URL doesn't update the
    // marker position after the page is loaded...
    papplClientHTMLPuts(client,
                        "          </form>\n"
                        "          <script>\n"
                        "function updateMap() {\n"
                        "  let map = document.getElementById('map');\n"
                        "  let lat = parseFloat(document.forms['form']['geo_location_lat'].value);\n"
                        "  let lon = parseFloat(document.forms['form']['geo_location_lon'].value);\n"
                        "  let bboxl = (lon - 0.00025).toFixed(4);\n"
                        "  let bboxb = (lat - 0.00025).toFixed(4);\n"
                        "  let bboxr = (lon + 0.00025).toFixed(4);\n"
                        "  let bboxt = (lat + 0.00025).toFixed(4);\n"
                        "  map.src = 'https://www.openstreetmap.org/export/embed.html?bbox=' + bboxl + ',' + bboxb + ',' + bboxr + ',' + bboxt + '&amp;layer=mapnik&amp;marker=' + lat + ',' + lon;\n"
                        "}\n"
                        "function setGeoLocation(p) {\n"
                        "  let lat = p.coords.latitude.toFixed(4);\n"
                        "  let lon = p.coords.longitude.toFixed(4);\n"
                        "  document.forms['form']['geo_location_lat'].value = lat;\n"
                        "  document.forms['form']['geo_location_lon'].value = lon;\n"
                        "  updateMap();\n"
                        "}\n"
                        "</script>\n");
  }
}


//
// 'papplClientHTMLPrintf()' - Send formatted text to the client, quoting as needed.
//

void
papplClientHTMLPrintf(
    pappl_client_t *client,		// I - Client
    const char     *format,		// I - Printf-style format string
    ...)				// I - Additional arguments as needed
{
  va_list	ap;			// Pointer to arguments
  const char	*start;			// Start of string
  char		size,			// Size character (h, l, L)
		type;			// Format type character
  int		width,			// Width of field
		prec;			// Number of characters of precision
  char		tformat[100],		// Temporary format string for sprintf()
		*tptr,			// Pointer into temporary format
		temp[1024];		// Buffer for formatted numbers
  char		*s;			// Pointer to string


  // Loop through the format string, formatting as needed...
  va_start(ap, format);
  start = format;

  while (*format)
  {
    if (*format == '%')
    {
      if (format > start)
        httpWrite2(client->http, start, (size_t)(format - start));

      tptr    = tformat;
      *tptr++ = *format++;

      if (*format == '%')
      {
        httpWrite2(client->http, "%", 1);
        format ++;
	start = format;
	continue;
      }
      else if (strchr(" -+#\'", *format))
        *tptr++ = *format++;

      if (*format == '*')
      {
        // Get width from argument...
	format ++;
	width = va_arg(ap, int);

	snprintf(tptr, sizeof(tformat) - (size_t)(tptr - tformat), "%d", width);
	tptr += strlen(tptr);
      }
      else
      {
	width = 0;

	while (isdigit(*format & 255))
	{
	  if (tptr < (tformat + sizeof(tformat) - 1))
	    *tptr++ = *format;

	  width = width * 10 + *format++ - '0';
	}
      }

      if (*format == '.')
      {
	if (tptr < (tformat + sizeof(tformat) - 1))
	  *tptr++ = *format;

        format ++;

        if (*format == '*')
	{
          // Get precision from argument...
	  format ++;
	  prec = va_arg(ap, int);

	  snprintf(tptr, sizeof(tformat) - (size_t)(tptr - tformat), "%d", prec);
	  tptr += strlen(tptr);
	}
	else
	{
	  prec = 0;

	  while (isdigit(*format & 255))
	  {
	    if (tptr < (tformat + sizeof(tformat) - 1))
	      *tptr++ = *format;

	    prec = prec * 10 + *format++ - '0';
	  }
	}
      }

      if (*format == 'l' && format[1] == 'l')
      {
        size = 'L';

	if (tptr < (tformat + sizeof(tformat) - 2))
	{
	  *tptr++ = 'l';
	  *tptr++ = 'l';
	}

	format += 2;
      }
      else if (*format == 'h' || *format == 'l' || *format == 'L')
      {
	if (tptr < (tformat + sizeof(tformat) - 1))
	  *tptr++ = *format;

        size = *format++;
      }
      else
        size = 0;

      if (!*format)
      {
        start = format;
        break;
      }

      if (tptr < (tformat + sizeof(tformat) - 1))
        *tptr++ = *format;

      type  = *format++;
      *tptr = '\0';
      start = format;

      switch (type)
      {
	case 'E' : // Floating point formats
	case 'G' :
	case 'e' :
	case 'f' :
	case 'g' :
	    if ((size_t)(width + 2) > sizeof(temp))
	      break;

	    sprintf(temp, tformat, va_arg(ap, double));

            httpWrite2(client->http, temp, strlen(temp));
	    break;

        case 'B' : // Integer formats
	case 'X' :
	case 'b' :
        case 'd' :
	case 'i' :
	case 'o' :
	case 'u' :
	case 'x' :
	    if ((size_t)(width + 2) > sizeof(temp))
	      break;

#  ifdef HAVE_LONG_LONG
            if (size == 'L')
	      sprintf(temp, tformat, va_arg(ap, long long));
	    else
#  endif // HAVE_LONG_LONG
            if (size == 'l')
	      sprintf(temp, tformat, va_arg(ap, long));
	    else
	      sprintf(temp, tformat, va_arg(ap, int));

            httpWrite2(client->http, temp, strlen(temp));
	    break;

	case 'p' : // Pointer value
	    if ((size_t)(width + 2) > sizeof(temp))
	      break;

	    sprintf(temp, tformat, va_arg(ap, void *));

            httpWrite2(client->http, temp, strlen(temp));
	    break;

        case 'c' : // Character or character array
            if (width <= 1)
            {
              temp[0] = (char)va_arg(ap, int);
              temp[1] = '\0';
              papplClientHTMLEscape(client, temp, 1);
            }
            else
              papplClientHTMLEscape(client, va_arg(ap, char *), (size_t)width);
	    break;

	case 's' : // String
	    if ((s = va_arg(ap, char *)) == NULL)
	      s = "(null)";

            papplClientHTMLEscape(client, s, strlen(s));
	    break;
      }
    }
    else
      format ++;
  }

  if (format > start)
    httpWrite2(client->http, start, (size_t)(format - start));

  va_end(ap);
}


//
// 'papplClientHTMLPuts()' - Write a HTML string.
//

void
papplClientHTMLPuts(
    pappl_client_t *client,		// I - Client
    const char     *s)			// I - String
{
  if (client && s && *s)
    httpWrite2(client->http, s, strlen(s));
}


//
// 'papplClientHTMLStartForm()' - Start a HTML form.
//
// This function starts a HTML form with the specified "action" path and
// includes the CSRF token as a hidden variable.
//

void
papplClientHTMLStartForm(
    pappl_client_t *client,		// I - Client
    const char     *action,		// I - Form action URL
    bool           multipart)		// I - `true` if the form allows file uploads, `false` otherwise
{
  char	token[256];			// CSRF token


  if (multipart)
  {
    // When allowing file attachments, the maximum size is 1MB...
    papplClientHTMLPrintf(client,
			  "          <form action=\"%s\" id=\"form\" method=\"POST\" enctype=\"multipart/form-data\">\n"
			  "          <input type=\"hidden\" name=\"session\" value=\"%s\">\n"
			  "          <input type=\"hidden\" name=\"MAX_FILE_SIZE\" value=\"1048576\">\n", action, papplClientGetCSRFToken(client, token, sizeof(token)));
  }
  else
  {
    papplClientHTMLPrintf(client,
			  "          <form action=\"%s\" id=\"form\" method=\"POST\">\n"
			  "          <input type=\"hidden\" name=\"session\" value=\"%s\">\n", action, papplClientGetCSRFToken(client, token, sizeof(token)));
  }
}


//
// 'papplClientValidateForm()' - Validate HTML form variables.
//
// This function validates the contents of a POST form using the CSRF token
// included as a hidden variable.
//
// Note: Callers are expected to validate all other form variables.
//

bool					// O - `true` if the CSRF token is valid, `false` otherwise
papplClientValidateForm(
    pappl_client_t *client,		// I - Client
    int            num_form,		// I - Number of form variables
    cups_option_t  *form)		// I - Form variables
{
  char		token[256];		// Expected CSRF token
  const char	*session;		// Form variable


  if ((session = cupsGetOption("session", num_form, form)) == NULL)
    return (false);

  return (!strcmp(session, papplClientGetCSRFToken(client, token, sizeof(token))));
}
