//
// Core client web interface functions for the Printer Application Framework
//
// Copyright © 2019-2024 by Michael R Sweet.
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
// 'papplClientGetCookie()' - Get a cookie from the client.
//
// This function gets a HTTP "cookie" value from the client request.  `NULL`
// is returned if no cookie has been set by a prior request, or if the user has
// disabled or removed the cookie.
//
// Use the @link papplClientSetCookie@ function to set a cookie in a response
// to a request.
//
// > Note: Cookies set with @link papplClientSetCookie@ will not be available to
// > this function until the following request.
//

char *					// O - Cookie value or `NULL` if not set
papplClientGetCookie(
    pappl_client_t *client,		// I - Client
    const char     *name,		// I - Name of cookie
    char           *buffer,		// I - Value buffer
    size_t         bufsize)		// I - Size of value buffer
{
  const char	*cookie = httpGetCookie(client->http);
					// Cookies from client
  char		temp[256],		// Temporary string
		*ptr,			// Pointer into temporary string
	        *end;			// End of temporary string
  bool		found;			// Did we find it?


  // Make sure the buffer is initialize, and return if we don't have any
  // cookies...
  *buffer = '\0';

  if (!cookie)
    return (NULL);

  // Scan the cookie string for 'name=value' or 'name="value"', separated by
  // semicolons...
  while (*cookie)
  {
    while (*cookie && isspace(*cookie & 255))
      cookie ++;

    if (!*cookie)
      break;

    for (ptr = temp, end = temp + sizeof(temp) - 1; *cookie && *cookie != '='; cookie ++)
    {
      if (ptr < end)
        *ptr++ = *cookie;
    }

    if (*cookie == '=')
    {
      cookie ++;
      *ptr = '\0';
      found = !strcmp(temp, name);

      if (found)
      {
        ptr = buffer;
        end = buffer + bufsize - 1;
      }
      else
      {
        ptr = temp;
        end = temp + sizeof(temp) - 1;
      }

      if (*cookie == '\"')
      {
        for (cookie ++; *cookie && *cookie != '\"'; cookie ++)
        {
          if (ptr < end)
            *ptr++ = *cookie;
	}

	if (*cookie == '\"')
	  cookie ++;
      }
      else
      {
        for (; *cookie && *cookie != ';'; cookie ++)
        {
          if (ptr < end)
            *ptr++ = *cookie;
        }
      }

      *ptr = '\0';

      if (found)
        return (buffer);
      else if (*cookie == ';')
        cookie ++;
    }
  }

  return (NULL);
}


//
// 'papplClientGetForm()' - Get form data from the web client.
//
// For HTTP GET requests, the form data is collected from the request URI.  For
// HTTP POST requests, the form data is read from the client.
//
// The returned form values must be freed using the @code cupsFreeOptions@
// function.
//
// > Note: Because the form data is read from the client connection, this
// > function can only be called once per request.
//

int					// O - Number of form variables read
papplClientGetForm(
    pappl_client_t *client,		// I - Client
    cups_option_t  **form)		// O - Form variables
{
  const char	*content_type;		// Content-Type header
  const char	*boundary;		// boundary value for multi-part
  char		*body,			// Message body
		*bodyptr,		// Pointer into message body
		*bodyend;		// End of message body
  size_t	body_alloc,		// Allocated message body size
		body_size = 0;		// Size of message body
  ssize_t	bytes;			// Bytes read
  cups_len_t	num_form = 0;		// Number of form variables
  http_state_t	initial_state;		// Initial HTTP state


  if (!client || !form)
  {
    if (form)
      *form = NULL;

    return (0);
  }

  content_type = httpGetField(client->http, HTTP_FIELD_CONTENT_TYPE);

  if (client->operation == HTTP_STATE_GET)
  {
    // Copy form data from the request URI...
    if (!client->options)
    {
      *form = NULL;
      return (0);
    }

    if ((body = strdup(client->options)) == NULL)
    {
      papplLogClient(client, PAPPL_LOGLEVEL_ERROR, "Unable to allocate memory for form data.");
      *form = NULL;
      return (0);
    }

    body_size    = strlen(body);
    content_type = "application/x-www-form-urlencoded";
  }
  else
  {
    // Read up to 2MB of data from the client...
    *form         = NULL;
    initial_state = httpGetState(client->http);
    body_alloc    = 65536;

    if ((body = malloc(body_alloc)) == NULL)
    {
      papplLogClient(client, PAPPL_LOGLEVEL_ERROR, "Unable to allocate memory for form data.");
      *form = NULL;
      return (0);
    }

    for (bodyptr = body, bodyend = body + body_alloc; (bytes = httpRead(client->http, bodyptr, (size_t)(bodyend - bodyptr))) > 0; bodyptr += bytes)
    {
      body_size += (size_t)bytes;

      if (body_size >= body_alloc)
      {
        char	*temp;			// Temporary pointer
        size_t	temp_offset;		// Temporary offset

        if (body_alloc >= (2 * 1024 * 1024))
          break;

        body_alloc += 65536;
        temp_offset = (size_t)(bodyptr - body);

        if ((temp = realloc(body, body_alloc)) == NULL)
        {
	  papplLogClient(client, PAPPL_LOGLEVEL_ERROR, "Unable to allocate memory for form data.");
          free(body);
	  *form = NULL;
	  return (0);
        }

        bodyptr = temp + temp_offset;
        bodyend = temp + body_alloc;
        body    = temp;
      }
    }

    papplLogClient(client, PAPPL_LOGLEVEL_DEBUG, "Read %ld bytes of form data (%s).", (long)body_size, content_type);

    // Flush remaining data...
    if (httpGetState(client->http) == initial_state)
      httpFlush(client->http);
  }

  // Parse the data in memory...
  bodyend = body + body_size;

  if (!strcmp(content_type, "application/x-www-form-urlencoded"))
  {
    // Read URL-encoded form data...
    char	name[256],		// Variable name
		*nameptr,		// Pointer into name
		value[2048],		// Variable value
		*valptr;		// Pointer into value

    for (bodyptr = body; bodyptr < bodyend;)
    {
      // Get the name...
      nameptr = name;
      while (bodyptr < bodyend && *bodyptr != '=')
      {
	int ch = *bodyptr++;		// Name character

	if (ch == '%' && isxdigit(bodyptr[0] & 255) && isxdigit(bodyptr[1] & 255))
	{
	  // Hex-encoded character
	  if (isdigit(*bodyptr))
	    ch = (*bodyptr++ - '0') << 4;
	  else
	    ch = (tolower(*bodyptr++) - 'a' + 10) << 4;

	  if (isdigit(*bodyptr))
	    ch |= *bodyptr++ - '0';
	  else
	    ch |= tolower(*bodyptr++) - 'a' + 10;
	}
	else if (ch == '+')
	  ch = ' ';

	if (nameptr < (name + sizeof(name) - 1))
	  *nameptr++ = (char)ch;
      }
      *nameptr = '\0';

      if (bodyptr >= bodyend)
	break;

      // Get the value...
      bodyptr ++;
      valptr = value;
      while (bodyptr < bodyend && *bodyptr != '&')
      {
	int ch = *bodyptr++;			// Name character

	if (ch == '%' && isxdigit(bodyptr[0] & 255) && isxdigit(bodyptr[1] & 255))
	{
	  // Hex-encoded character
	  if (isdigit(*bodyptr))
	    ch = (*bodyptr++ - '0') << 4;
	  else
	    ch = (tolower(*bodyptr++) - 'a' + 10) << 4;

	  if (isdigit(*bodyptr))
	    ch |= *bodyptr++ - '0';
	  else
	    ch |= tolower(*bodyptr++) - 'a' + 10;
	}
	else if (ch == '+')
	  ch = ' ';

	if (valptr < (value + sizeof(value) - 1))
	  *valptr++ = (char)ch;
      }
      *valptr = '\0';

      if (bodyptr < bodyend)
	bodyptr ++;

      // Add the name + value to the option array...
      num_form = cupsAddOption(name, value, num_form, form);
    }
  }
  else if (!strncmp(content_type, "multipart/form-data; ", 21) && (boundary = strstr(content_type, "boundary=")) != NULL)
  {
    // Read multi-part form data...
    char	name[1024],		// Form variable name
		filename[1024],		// Form filename
		bstring[256],		// Boundary string to look for
		*bend,			// End of value (boundary)
		*line,			// Start of line
		*ptr;			// Pointer into name/filename
    size_t	blen;			// Length of boundary string

    // Format the boundary string we are looking for...
    snprintf(bstring, sizeof(bstring), "\r\n--%s", boundary + 9);
    blen = strlen(bstring);

    // Parse lines in the message body...
    name[0] = '\0';
    filename[0] = '\0';

    for (bodyptr = body; bodyptr < bodyend;)
    {
      // Split out a line...
      for (line = bodyptr; bodyptr < bodyend; bodyptr ++)
      {
        if (!memcmp(bodyptr, "\r\n", 2))
        {
          *bodyptr = '\0';
          bodyptr += 2;
          break;
        }
      }

      if (bodyptr >= bodyend)
        break;

      papplLogClient(client, PAPPL_LOGLEVEL_DEBUG, "Line '%s'.", line);

      if (!*line)
      {
        // End of headers, grab value...
        if (!name[0])
        {
          // No name value...
	  papplLogClient(client, PAPPL_LOGLEVEL_ERROR, "Invalid multipart form data.");
	  break;
	}

	for (bend = bodyend - blen, ptr = memchr(bodyptr, '\r', (size_t)(bend - bodyptr)); ptr; ptr = memchr(ptr + 1, '\r', (size_t)(bend - ptr - 1)))
	{
	  // Check for boundary string...
	  if (!memcmp(ptr, bstring, blen))
	    break;
	}

	if (!ptr)
	{
	  // No boundary string, invalid data...
	  papplLogClient(client, PAPPL_LOGLEVEL_ERROR, "Invalid multipart form data.");
	  break;
	}

        // Point to the start of the boundary string...
	bend    = ptr;
        ptr     = bodyptr;
        bodyptr = bend + blen;

	if (filename[0])
	{
	  // Save an embedded file...
          const char *tempfile;		// Temporary file

	  if ((tempfile = _papplClientCreateTempFile(client, ptr, (size_t)(bend - ptr))) == NULL)
	    break;

          num_form = cupsAddOption(name, tempfile, num_form, form);
	}
	else
	{
	  // Save the form variable...
	  *bend = '\0';

          num_form = cupsAddOption(name, ptr, num_form, form);
	}

	name[0]     = '\0';
	filename[0] = '\0';

        if (bodyptr < (bodyend - 1) && bodyptr[0] == '\r' && bodyptr[1] == '\n')
          bodyptr += 2;
      }
      else if (!strncasecmp(line, "Content-Disposition:", 20))
      {
	if ((ptr = strstr(line + 20, " name=\"")) != NULL)
	{
	  papplCopyString(name, ptr + 7, sizeof(name));

	  if ((ptr = strchr(name, '\"')) != NULL)
	    *ptr = '\0';
	}

	if ((ptr = strstr(line + 20, " filename=\"")) != NULL)
	{
	  papplCopyString(filename, ptr + 11, sizeof(filename));

	  if ((ptr = strchr(filename, '\"')) != NULL)
	    *ptr = '\0';
	}
      }
    }
  }

  free(body);

  // Return whatever we got...
  return ((int)num_form);
}


//
// 'papplClientHTMLAuthorize()' - Handle authorization for the web interface.
//
// The web interface supports both authentication against user accounts and
// authentication using a single administrative access password.  This function
// handles the details of authentication for the web interface based on the
// system authentication service configuration = the "auth_service" argument to
// @link papplSystemCreate@ and any callback set using
// @link papplSystemSetAuthCallback@.
//
// > Note: IPP operation callbacks needing to perform authorization should use
// > the @link papplClientIsAuthorized@ function instead.
//

bool					// O - `true` if authorized, `false` otherwise
papplClientHTMLAuthorize(
    pappl_client_t *client)		// I - Client
{
  char		auth_cookie[65],	// Authorization cookie
		session_key[65],	// Current session key
		password_hash[100],	// Password hash
		auth_text[256];		// Authorization string
  unsigned char	auth_hash[32];		// Authorization hash
  const char	*status = NULL;		// Status message, if any


  // Don't authorize if we have no auth service or we don't have a password set.
  if (!client || (!client->system->auth_service && !client->system->auth_cb && !client->system->password_hash[0]))
  {
    _PAPPL_DEBUG("papplClientHTMLAuthorize: auth_service='%s', auth_cb=%s, password_hash=%s\n", client->system->auth_service, client->system->auth_cb != NULL ? "set" : "unset", client->system->password_hash[0] ? "set" : "unset");
    _PAPPL_DEBUG("papplClientHTMLAuthorize: Returning true.");
    return (true);
  }

  // When using an auth service, use HTTP Basic authentication...
  if (client->system->auth_service || client->system->auth_cb)
  {
    http_status_t code = papplClientIsAuthorized(client);
					// Authorization status code

    _PAPPL_DEBUG("papplClientHTMLAuthorize: code=%d.\n", code);

    if (code != HTTP_STATUS_CONTINUE)
    {
      _PAPPL_DEBUG("papplClientHTMLAuthorize: Returning false.\n");
      papplClientRespond(client, code, NULL, NULL, 0, 0);
      return (false);
    }
    else
    {
      _PAPPL_DEBUG("papplClientHTMLAuthorize: Returning true.\n");
      return (true);
    }
  }

  // Otherwise look for the authorization cookie...
  if (papplClientGetCookie(client, "auth", auth_cookie, sizeof(auth_cookie)))
  {
    _PAPPL_DEBUG("papplClientHTMLAuthorize: Got auth cookie '%s'.\n", auth_cookie);
    snprintf(auth_text, sizeof(auth_text), "%s:%s", papplSystemGetSessionKey(client->system, session_key, sizeof(session_key)), papplSystemGetPassword(client->system, password_hash, sizeof(password_hash)));
    cupsHashData("sha2-256", (unsigned char *)auth_text, strlen(auth_text), auth_hash, sizeof(auth_hash));
    cupsHashString(auth_hash, sizeof(auth_hash), auth_text, sizeof(auth_text));

    _PAPPL_DEBUG("papplClientHTMLAuthorize: Expect auth cookie '%s'.\n", auth_text);

    if (_papplIsEqual(auth_cookie, auth_text))
    {
      // Hashes match so we are authorized.  Use "web-admin" as the username.
      papplCopyString(client->username, "web-admin", sizeof(client->username));

      _PAPPL_DEBUG("papplClientHTMLAuthorize: Returning true.\n");
      return (true);
    }
  }

  // No cookie, so see if this is a form submission...
  if (client->operation == HTTP_STATE_POST)
  {
    // Yes, grab the login information and try to authorize...
    cups_len_t		num_form = 0;	// Number of form variable
    cups_option_t	*form = NULL;	// Form variables
    const char		*password;	// Password from user

    _PAPPL_DEBUG("papplClientHTMLAuthorize: POST.\n");

    if ((num_form = (cups_len_t)papplClientGetForm(client, &form)) == 0)
    {
      status = "Invalid form data.";
    }
    else if (!papplClientIsValidForm(client, (int)num_form, form))
    {
      status = "Invalid form submission.";
    }
    else if ((password = cupsGetOption("password", num_form, form)) == NULL)
    {
      status = "Login password required.";
    }
    else
    {
      // Hash the user-supplied password with the salt from the stored password
      papplSystemGetPassword(client->system, password_hash, sizeof(password_hash));
      papplSystemHashPassword(client->system, password_hash, password, auth_text, sizeof(auth_text));

      _PAPPL_DEBUG("papplClientHTMLAuthorize: Saved password_hash is '%s'.\n", password_hash);
      _PAPPL_DEBUG("papplClientHTMLAuthorize: Hashed form password is '%s'.\n", auth_text);

      if (_papplIsEqual(password_hash, auth_text))
      {
        // Password hashes match, generate the cookie from the session key and
        // password hash...

	snprintf(auth_text, sizeof(auth_text), "%s:%s", papplSystemGetSessionKey(client->system, session_key, sizeof(session_key)), password_hash);
	cupsHashData("sha2-256", (unsigned char *)auth_text, strlen(auth_text), auth_hash, sizeof(auth_hash));
	cupsHashString(auth_hash, sizeof(auth_hash), auth_text, sizeof(auth_text));

	papplClientSetCookie(client, "auth", auth_text, 3600);
        _PAPPL_DEBUG("papplClientHTMLAuthorize: Setting 'auth' cookie to '%s'.\n", auth_text);
      }
      else
      {
        status = "Password incorrect.";
      }
    }

    cupsFreeOptions(num_form, form);

    // Make the caller think this is a GET request...
    client->operation = HTTP_STATE_GET;

    _PAPPL_DEBUG("papplClientHTMLAuthorize: Status message is '%s'.\n", status);

    if (!status)
    {
      // Hashes match so we are authorized.  Use "web-admin" as the username.
      papplCopyString(client->username, "web-admin", sizeof(client->username));

      _PAPPL_DEBUG("papplClientHTMLAuthorize: Returning true.\n");
      return (true);
    }
  }

  // If we get this far, show the standard login form...
  _PAPPL_DEBUG("papplClientHTMLAuthorize: Showing login form.\n");

  papplClientRespond(client, HTTP_STATUS_OK, NULL, "text/html", 0, 0);
  papplClientHTMLHeader(client, "Login", 0);
  papplClientHTMLPuts(client,
                      "    <div class=\"content\">\n"
                      "      <div class=\"row\">\n"
		      "        <div class=\"col-12\">\n"
		      "          <h1 class=\"title\">Login</h1>\n");

  if (status)
    papplClientHTMLPrintf(client, "          <div class=\"banner\">%s</div>\n", status);

  papplClientHTMLStartForm(client, client->uri, false);
  papplClientHTMLPuts(client,
                      "          <p><label>Password: <input type=\"password\" name=\"password\"></label> <input type=\"submit\" value=\"Login\"></p>\n"
                      "          </form>\n"
                      "        </div>\n"
                      "      </div>\n");
  papplClientHTMLFooter(client);

  _PAPPL_DEBUG("papplClientHTMLAuthorize: Returning false.\n");
  return (false);
}


//
// 'papplClientHTMLEscape()' - Send a string to a web browser client.
//
// This function sends the specified string to the web browser client and
// escapes special characters as HTML entities as needed, for example "&" is
// sent as `&amp;`.
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
        httpWrite(client->http, start, (size_t)(s - start));

      if (*s == '&')
        httpWrite(client->http, "&amp;", 5);
      else if (*s == '<')
        httpWrite(client->http, "&lt;", 4);
      else
        httpWrite(client->http, "&quot;", 6);

      start = s + 1;
    }

    s ++;
  }

  if (s > start)
    httpWrite(client->http, start, (size_t)(s - start));
}


//
// 'papplClientHTMLFooter()' - Show the web interface footer.
//
// This function sends the standard web interface footer followed by a
// trailing 0-length chunk to finish the current HTTP response.  Use the
// @link papplSystemSetFooterHTML@ function to add any custom HTML needed in
// the footer.
//

void
papplClientHTMLFooter(
    pappl_client_t *client)		// I - Client
{
  const char *footer = papplClientGetLocString(client, papplSystemGetFooterHTML(papplClientGetSystem(client)));
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
  httpWrite(client->http, "", 0);
}


//
// 'papplClientHTMLHeader()' - Show the web interface header and title.
//
// This function sends the standard web interface header and title.  If the
// "refresh" argument is greater than zero, the page will automatically reload
// after that many seconds.
//
// Use the @link papplSystemAddLink@ function to add system-wide navigation
// links to the header.  Similarly, use @link papplPrinterAddLink@ to add
// printer-specific links, which will appear in the web interface printer if
// the system is not configured to support multiple printers
// (the `PAPPL_SOPTIONS_MULTI_QUEUE` option to @link papplSystemCreate@).
//

void
papplClientHTMLHeader(
    pappl_client_t *client,		// I - Client
    const char     *title,		// I - Title
    int            refresh)		// I - Refresh time in seconds (`0` for no refresh)
{
  pappl_system_t	*system = client->system;
					// System
  pappl_printer_t	*printer;	// Printer
  const char		*name;		// Name for title/header


  _papplRWLockRead(system);
  printer = (pappl_printer_t *)cupsArrayGetFirst(system->printers);
  _papplRWUnlock(system);

  if ((system->options & PAPPL_SOPTIONS_MULTI_QUEUE) || !printer)
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
			"    <meta http-equiv=\"X-UA-Compatible\" content=\"IE=9\">\n", title ? papplClientGetLocString(client, title) : "", title ? " - " : "", name);
  if (refresh > 0)
    papplClientHTMLPrintf(client, "<meta http-equiv=\"refresh\" content=\"%d\">\n", refresh);
  papplClientHTMLPuts(client,
		      "    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n"
		      "  </head>\n"
		      "  <body>\n"
		      "    <div class=\"header\">\n"
		      "      <div class=\"row\">\n"
		      "        <div class=\"col-12 nav\">\n"
		      "          <a class=\"btn\" href=\"/\"><img src=\"/navicon.png\"></a>\n");

  _papplRWLockRead(system);

  _papplClientHTMLPutLinks(client, system->links, PAPPL_LOPTIONS_NAVIGATION);

  _papplRWUnlock(system);

  if (!(system->options & PAPPL_SOPTIONS_MULTI_QUEUE) && printer)
  {
    if (cupsArrayGetCount(system->links) > 0)
      papplClientHTMLPuts(client, "          <span class=\"spacer\"></span>\n");

    _papplRWLockRead(printer);

    _papplClientHTMLPutLinks(client, printer->links, PAPPL_LOPTIONS_NAVIGATION);

    _papplRWUnlock(printer);
  }

  papplClientHTMLPuts(client,
		      "        </div>\n"
		      "      </div>\n"
		      "    </div>\n");
}


//
// '_papplClientHTMLInfo()' - Show system/printer information.
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
  papplClientHTMLPrintf(client,
		        "          <table class=\"form\">\n"
		        "            <tbody>\n"
		        "              <tr><th>%s:</th><td>", papplClientGetLocString(client, _PAPPL_LOC("Name")));
  if (is_form)
    papplClientHTMLPrintf(client, "<input type=\"text\" name=\"dns_sd_name\" value=\"%s\" placeholder=\"%s\">", dns_sd_name ? dns_sd_name : "", papplClientGetLocString(client, _PAPPL_LOC("DNS-SD Service Name")));
  else
    papplClientHTMLEscape(client, dns_sd_name ? dns_sd_name : papplClientGetLocString(client, _PAPPL_LOC("Not set")), 0);

  // Location and geo-location...
  papplClientHTMLPrintf(client,
                        "</td></tr>\n"
		        "              <tr><th>%s:</th><td>", papplClientGetLocString(client, _PAPPL_LOC("Location")));
  if (is_form)
  {
    papplClientHTMLPrintf(client,
                          "<input type=\"text\" name=\"location\" placeholder=\"%s\" value=\"%s\"><br>\n"
                          "<input type=\"number\" name=\"geo_location_lat\" min=\"-90\" max=\"90\" step=\"0.0001\" value=\"%.4f\" onChange=\"updateMap();\">&nbsp;&deg;&nbsp;latitude x <input type=\"number\" name=\"geo_location_lon\" min=\"-180\" max=\"180\" step=\"0.0001\" value=\"%.4f\" onChange=\"updateMap();\">&nbsp;&deg;&nbsp;longitude", papplClientGetLocString(client, _PAPPL_LOC("Human-Readable Location")), location ? location : "", lat, lon);

    if (httpIsEncrypted(client->http))
    {
      // If the connection is encrypted, show a button to lookup the position...
      papplClientHTMLPrintf(client, " <button id=\"geo_location_lookup\" onClick=\"event.preventDefault(); navigator.geolocation.getCurrentPosition(setGeoLocation);\">%s</button>", papplClientGetLocString(client, _PAPPL_LOC("Use My Position")));
    }
    else if (!(client->system->options & PAPPL_SOPTIONS_NO_TLS))
    {
      // If the connection is not encrypted, redirect to a secure page...
      papplClientHTMLPrintf(client, " <a class=\"btn\" href=\"https://%s:%d%s?get-location\">%s</a>", client->host_field, client->host_port, client->uri, papplClientGetLocString(client, _PAPPL_LOC("Use My Position")));
    }
  }
  else
  {
    papplClientHTMLPrintf(client, "%s", location ? location : papplClientGetLocString(client, _PAPPL_LOC("Not set")));
    if (geo_location)
      papplClientHTMLPrintf(client, "<br>\n%g&deg;&nbsp;%c %g&deg;&nbsp;%c", fabs(lat), lat < 0.0 ? 'S' : 'N', fabs(lon), lon < 0.0 ? 'W' : 'E');
  }

  // Show an embedded map of the location...
  if (geo_location || is_form)
    papplClientHTMLPrintf(client,
			  "<br>\n"
			  "<iframe id=\"map\" frameborder=\"0\" scrolling=\"no\" marginheight=\"0\" marginwidth=\"0\" src=\"https://www.openstreetmap.org/export/embed.html?bbox=%g,%g,%g,%g&amp;layer=mapnik&amp;marker=%g,%g\"></iframe>", lon - 0.00025, lat - 0.00025, lon + 0.00025, lat + 0.00025, lat, lon);

  // Organization
  papplClientHTMLPrintf(client,
                        "</td></tr>\n"
		        "              <tr><th>%s:</th><td>", papplClientGetLocString(client, _PAPPL_LOC("Organization")));

  if (is_form)
    papplClientHTMLPrintf(client,
                          "<input type=\"text\" name=\"organization\" placeholder=\"%s\" value=\"%s\"><br>\n"
                          "<input type=\"text\" name=\"organizational_unit\" placeholder=\"%s\" value=\"%s\">", papplClientGetLocString(client, _PAPPL_LOC("Organization Name")), organization ? organization : "", papplClientGetLocString(client, _PAPPL_LOC("Organizational Unit")), org_unit ? org_unit : "");
  else
    papplClientHTMLPrintf(client, "%s%s%s", organization ? organization : papplClientGetLocString(client, _PAPPL_LOC("Not set")), org_unit ? ", " : "", org_unit ? org_unit : "");

  // Contact
  papplClientHTMLPrintf(client,
                        "</td></tr>\n"
                        "              <tr><th>%s:</th><td>", papplClientGetLocString(client, _PAPPL_LOC("Contact")));

  if (is_form)
  {
    papplClientHTMLPrintf(client,
                          "<input type=\"text\" name=\"contact_name\" placeholder=\"%s\" value=\"%s\"><br>\n"
                          "<input type=\"email\" name=\"contact_email\" placeholder=\"name@domain\" value=\"%s\"><br>\n"
                          "<input type=\"tel\" name=\"contact_telephone\" placeholder=\"867-5309\" value=\"%s\"></td></tr>\n"
		      "              <tr><th></th><td><input type=\"submit\" value=\"Save Changes\">", papplClientGetLocString(client, _PAPPL_LOC("Name")), contact->name, contact->email, contact->telephone);
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
    papplClientHTMLPuts(client, papplClientGetLocString(client, _PAPPL_LOC("Not set")));

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
                        "}\n");
    if (client->options && !strcmp(client->options, "get-location"))
      papplClientHTMLPuts(client, "navigator.geolocation.getCurrentPosition(setGeoLocation);\n");
    papplClientHTMLPuts(client, "</script>\n");
  }
}


//
// 'papplClientHTMLPrinterFooter()' - Show the web interface footer for printers.
//
// This function sends the standard web interface footer for a printer followed
// by a trailing 0-length chunk to finish the current HTTP response.  Use the
// @link papplSystemSetFooterHTML@ function to add any custom HTML needed in
// the footer.
//

void
papplClientHTMLPrinterFooter(pappl_client_t *client)	// I - Client
{
  papplClientHTMLPuts(client,
                      "          </div>\n"
                      "        </div>\n"
                      "      </div>\n");
  papplClientHTMLFooter(client);
}


//
// 'papplClientHTMLScannerFooter()' - Show the web interface footer for scanners.
//
// This function sends the standard web interface footer for a scanner followed
// by a trailing 0-length chunk to finish the current HTTP response.  Use the
// @link papplSystemSetFooterHTML@ function to add any custom HTML needed in
// the footer.
//

void
papplClientHTMLScannerFooter(pappl_client_t *client)	// I - Client
{
  papplClientHTMLPuts(client,
                      "          </div>\n"
                      "        </div>\n"
                      "      </div>\n");
  papplClientHTMLFooter(client);
}


//
// 'papplClientHTMLPrinterHeader()' - Show the web interface header and title
//                                    for printers.
//
// This function sends the standard web interface header and title for a
// printer.  If the "refresh" argument is greater than zero, the page will
// automatically reload after that many seconds.
//
// If "label" and "path_or_url" are non-`NULL` strings, an additional navigation
// link is included with the title header - this is typically used for an
// action button ("Change").
//
// Use the @link papplSystemAddLink@ function to add system-wide navigation
// links to the header.  Similarly, use @link papplPrinterAddLink@ to add
// printer-specific links, which will appear in the web interface printer if
// the system is not configured to support multiple printers
// (the `PAPPL_SOPTIONS_MULTI_QUEUE` option to @link papplSystemCreate@).
//

void
papplClientHTMLPrinterHeader(
    pappl_client_t  *client,		// I - Client
    pappl_printer_t *printer,		// I - Printer
    const char      *title,		// I - Title
    int             refresh,		// I - Refresh time in seconds or 0 for none
    const char      *label,		// I - Button label or `NULL` for none
    const char      *path_or_url)	// I - Button path or `NULL` for none
{
  const char	*header;		// Header text


  if (!papplClientRespond(client, HTTP_STATUS_OK, NULL, "text/html", 0, 0))
    return;

  if (printer->system->options & PAPPL_SOPTIONS_MULTI_QUEUE)
  {
    // Multi-queue mode, need to add the printer name to the title...
    if (title)
    {
      char	full_title[1024];	// Full title

      // Need to grab the localized title here since the title includes the printer name...
      snprintf(full_title, sizeof(full_title), "%s - %s", papplClientGetLocString(client, title), printer->name);
      papplClientHTMLHeader(client, full_title, refresh);
    }
    else
    {
      papplClientHTMLHeader(client, printer->name, refresh);
    }
  }
  else
  {
    // Single queue mode - the function will automatically add the printer name and localize the title...
    papplClientHTMLHeader(client, title, refresh);
  }

  if (printer->system->options & PAPPL_SOPTIONS_MULTI_QUEUE)
  {
    _papplRWLockRead(printer);
    papplClientHTMLPrintf(client,
			  "    <div class=\"header2\">\n"
			  "      <div class=\"row\">\n"
			  "        <div class=\"col-12 nav\"><a class=\"btn\" href=\"%s\">%s:</a>\n", printer->uriname, printer->name);
    _papplClientHTMLPutLinks(client, printer->links, PAPPL_LOPTIONS_NAVIGATION);
    papplClientHTMLPuts(client,
			"        </div>\n"
			"      </div>\n"
			"    </div>\n");
    _papplRWUnlock(printer);
  }
  else if (client->system->versions[0].sversion[0])
    papplClientHTMLPrintf(client,
			  "    <div class=\"header2\">\n"
			  "      <div class=\"row\">\n"
			  "        <div class=\"col-12 nav\">\n"
			  "          Version %s\n"
			  "        </div>\n"
			  "      </div>\n"
			  "    </div>\n", client->system->versions[0].sversion);

  papplClientHTMLPuts(client, "    <div class=\"content\">\n");

  if ((header = papplClientGetLocString(client, client->uri)) == client->uri)
  {
    size_t urilen = strlen(printer->uriname);
					// Length of printer URI name
    const char *uriptr = client->uri + urilen;
					// Pointer into client URI

    if (strlen(client->uri) <= urilen || !strcmp(client->uri, "/") || (header = papplClientGetLocString(client, uriptr)) == uriptr)
      header = NULL;
  }

  if (header)
  {
    // Show header text
    papplClientHTMLPuts(client,
			"      <div class=\"row\">\n"
			"        <div class=\"col-12\">\n");
    papplClientHTMLPuts(client, header);
    papplClientHTMLPuts(client,
                        "\n"
                        "        </div>\n"
                        "      </div>\n");
  }

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
// 'papplClientHTMLScannerHeader()' - Show the web interface header and title
//                                    for scanners.
//
// This function sends the standard web interface header and title for a
// scanner.  If the "refresh" argument is greater than zero, the page will
// automatically reload after that many seconds.
//
// If "label" and "path_or_url" are non-`NULL` strings, an additional navigation
// link is included with the title header - this is typically used for an
// action button ("Change").
//
// Use the @link papplSystemAddLink@ function to add system-wide navigation
// links to the header.  Similarly, use @link papplScannerAddLink@ to add
// scanner-specific links, which will appear in the web interface scanner if
// the system is not configured to support multiple scanners
//

// TODO : papplScannerAddLink
void
papplClientHTMLScannerHeader(
    pappl_client_t  *client,		// I - Client
    pappl_scanner_t *scanner,		// I - Scanner
    const char      *title,		// I - Title
    int             refresh,		// I - Refresh time in seconds or 0 for none
    const char      *label,		// I - Button label or `NULL` for none
    const char      *path_or_url)	// I - Button path or `NULL` for none
{
  const char	*header;		// Header text

  if (!papplClientRespond(client, HTTP_STATUS_OK, NULL, "text/html", 0, 0))
    return;


  // Multi-queue mode not required for scanners

  // Single queue mode - the function will automatically add the scanner name and localize the title...
  papplClientHTMLHeader(client, title, refresh);

  // Generate HTML for scanners
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

  // Display system version if available
  if (client->system->versions[0].sversion[0])
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

  if ((header = papplClientGetLocString(client, client->uri)) == client->uri)
  {
    size_t urilen = strlen(scanner->uriname);
					// Length of scanner URI name
    const char *uriptr = client->uri + urilen;
					// Pointer into client URI

    if (strlen(client->uri) <= urilen || !strcmp(client->uri, "/") || (header = papplClientGetLocString(client, uriptr)) == uriptr)
      header = NULL;
  }

  if (header)
  {
    // Show header text
    papplClientHTMLPuts(client,
			"      <div class=\"row\">\n"
			"        <div class=\"col-12\">\n");
    papplClientHTMLPuts(client, header);
    papplClientHTMLPuts(client,
                        "\n"
                        "        </div>\n"
                        "      </div>\n");
  }

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
// 'papplClientHTMLPrintf()' - Send formatted text to the web browser client,
//                             escaping as needed.
//
// This function sends formatted text to the web browser client using
// `printf`-style formatting codes.  The format string itself is not escaped
// to allow for embedded HTML, however strings inserted using the '%c' or `%s`
// codes are escaped properly for HTML - "&" is sent as `&amp;`, etc.
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
  char		tformat[100],		// Temporary format string for snprintf()
		*tptr,			// Pointer into temporary format
		temp[1024];		// Buffer for formatted numbers
  const char	*s;			// Pointer to string


  // Loop through the format string, formatting as needed...
  // TODO: Support positional parameters, e.g. "%2$s" to access the second string argument
  va_start(ap, format);
  start = format;

  while (*format)
  {
    if (*format == '%')
    {
      if (format > start)
        httpWrite(client->http, start, (size_t)(format - start));

      tptr    = tformat;
      *tptr++ = *format++;

      if (*format == '%')
      {
        httpWrite(client->http, "%", 1);
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

	    snprintf(temp, sizeof(temp), tformat, va_arg(ap, double));

            httpWrite(client->http, temp, strlen(temp));
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
	      snprintf(temp, sizeof(temp), tformat, va_arg(ap, long long));
	    else
#  endif // HAVE_LONG_LONG
            if (size == 'l')
	      snprintf(temp, sizeof(temp), tformat, va_arg(ap, long));
	    else
	      snprintf(temp, sizeof(temp), tformat, va_arg(ap, int));

            httpWrite(client->http, temp, strlen(temp));
	    break;

	case 'p' : // Pointer value
	    if ((size_t)(width + 2) > sizeof(temp))
	      break;

	    snprintf(temp, sizeof(temp), tformat, va_arg(ap, void *));

            httpWrite(client->http, temp, strlen(temp));
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
	    if ((s = va_arg(ap, const char *)) == NULL)
	      s = "(null)";

            papplClientHTMLEscape(client, s, strlen(s));
	    break;
      }
    }
    else
      format ++;
  }

  if (format > start)
    httpWrite(client->http, start, (size_t)(format - start));

  va_end(ap);
}


//
// '_papplClientHTMLPutLinks()' - Print an array of links.
//

void
_papplClientHTMLPutLinks(
    pappl_client_t   *client,		// I - Client
    cups_array_t     *links,		// I - Array of links
    pappl_loptions_t which)		// I - Which links to show
{
  cups_len_t	i,			// Looping var
		count;			// Number of links
  _pappl_link_t	*l;			// Current link
  const char	*webscheme = _papplClientGetAuthWebScheme(client);
					// URL scheme for authenticated links


  // Loop through the links.
  //
  // Note: We use a loop and not cupsArrayGetFirst/Last because other threads may
  // be enumerating the same array of links.
  for (i = 0, count = cupsArrayGetCount(links); i < count; i ++)
  {
    l = (_pappl_link_t *)cupsArrayGetElement(links, i);

    if (!l || !(l->options & which))
      continue;

    if (strcmp(client->uri, l->path_or_url))
    {
      if (l->path_or_url[0] != '/' || !(l->options & PAPPL_LOPTIONS_HTTPS_REQUIRED))
	papplClientHTMLPrintf(client, "          <a class=\"btn\" href=\"%s\">%s</a>\n", l->path_or_url, papplClientGetLocString(client, l->label));
      else
	papplClientHTMLPrintf(client, "          <a class=\"btn\" href=\"%s://%s:%d%s\">%s</a>\n", webscheme, client->host_field, client->host_port, l->path_or_url, papplClientGetLocString(client, l->label));
    }
    else
      papplClientHTMLPrintf(client, "          <span class=\"active\">%s</span>\n", papplClientGetLocString(client, l->label));
  }
}


//
// 'papplClientHTMLPuts()' - Send a HTML string to the web browser client.
//
// This function sends a HTML string to the client without performing any
// escaping of special characters.
//

void
papplClientHTMLPuts(
    pappl_client_t *client,		// I - Client
    const char     *s)			// I - String
{
  if (client && s && *s)
    httpWrite(client->http, s, strlen(s));
}


//
// 'papplClientHTMLStartForm()' - Start a HTML form.
//
// This function starts a HTML form with the specified "action" path and
// includes the CSRF token as a hidden variable.  If the "multipart" argument
// is `true`, the form is annotated to support file attachments up to 2MiB in
// size.
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
    // When allowing file attachments, the maximum size is 2MB...
    papplClientHTMLPrintf(client,
			  "          <form action=\"%s\" id=\"form\" method=\"POST\" enctype=\"multipart/form-data\">\n"
			  "          <input type=\"hidden\" name=\"session\" value=\"%s\">\n"
			  "          <input type=\"hidden\" name=\"MAX_FILE_SIZE\" value=\"2097152\">\n", action, papplClientGetCSRFToken(client, token, sizeof(token)));
  }
  else
  {
    papplClientHTMLPrintf(client,
			  "          <form action=\"%s\" id=\"form\" method=\"POST\">\n"
			  "          <input type=\"hidden\" name=\"session\" value=\"%s\">\n", action, papplClientGetCSRFToken(client, token, sizeof(token)));
  }
}


//
// 'papplClientIsValidForm()' - Validate HTML form variables.
//
// This function validates the contents of a HTML form using the CSRF token
// included as a hidden variable.  When sending a HTML form you should use the
// @link papplClientStartForm@ function to start the HTML form and insert the
// CSRF token for later validation.
//
// > Note: Callers are expected to validate all other form variables.
//

bool					// O - `true` if the CSRF token is valid, `false` otherwise
papplClientIsValidForm(
    pappl_client_t *client,		// I - Client
    int            num_form,		// I - Number of form variables
    cups_option_t  *form)		// I - Form variables
{
  char		token[256];		// Expected CSRF token
  const char	*session;		// Form variable


  if ((session = cupsGetOption("session", (cups_len_t)num_form, form)) == NULL)
    return (false);

  return (!strcmp(session, papplClientGetCSRFToken(client, token, sizeof(token))));
}


//
// 'papplClientSetCookie()' - Set a cookie for the web browser client.
//
// This function sets the value of a cookie for the client by updating the
// `Set-Cookie` header in the HTTP response that will be sent.  The "name" and
// "value" strings must contain only valid characters for a cookie and its
// value as documented in RFC 6265, which basically means letters, numbers, "@",
// "-", ".", and "_".
//
// The "expires" argument specifies how long the cookie will remain active in
// seconds, for example `3600` seconds is one hour and `86400` seconds is one
// day.  If the value is zero or less, a "session" cookie is created instead
// which will expire as soon as the web browser is closed.
//

void
papplClientSetCookie(
    pappl_client_t *client,		// I - Client
    const char     *name,		// I - Cookie name
    const char     *value,		// I - Cookie value
    int            expires)		// I - Expiration in seconds from now, `0` for a session cookie
{
  const char	*client_cookie = httpGetCookie(client->http);
					// Current cookie
  char		buffer[1024],		// New cookie buffer
		cookie[256],		// New authorization cookie
		expireTime[64];		// Expiration date/time

  if (!name)
    return;

  if (expires > 0)
    snprintf(cookie, sizeof(cookie), "%s=%s; path=/; expires=%s; httponly; secure;", name, value, httpGetDateString(time(NULL) + expires, expireTime, sizeof(expireTime)));
  else
    snprintf(cookie, sizeof(cookie), "%s=%s; path=/; httponly; secure;", name, value);

  if (!client_cookie || !*client_cookie)
  {
    // No other cookies set...
    httpSetCookie(client->http, cookie);
  }
  else
  {
    // Append the new cookie with a Set-Cookie: header since libcups only
    // directly supports setting a single Set-Cookie header...
    snprintf(buffer, sizeof(buffer), "%s\r\nSet-Cookie: %s", client_cookie, cookie);
    httpSetCookie(client->http, buffer);
  }
}
