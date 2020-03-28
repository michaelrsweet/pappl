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


//
// 'papplClientGetForm()' - Get POST form data from the web client.
//

int					// O - Number of form variables read
papplClientGetForm(
    pappl_client_t *client,		// I - Client
    cups_option_t   **form)		// O - Form variables
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
    if (*s == '&' || *s == '<')
    {
      if (s > start)
        httpWrite2(client->http, start, (size_t)(s - start));

      if (*s == '&')
        httpWrite2(client->http, "&amp;", 5);
      else
        httpWrite2(client->http, "&lt;", 4);

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
  _pappl_resource_t	*r;		// Current resource
  const char		*sw_name = system->firmware_name ? system->firmware_name : "Unknown";


  papplClientHTMLPrintf(client,
			"<!DOCTYPE html>\n"
			"<html>\n"
			"  <head>\n"
			"    <title>%s - %s</title>\n"
			"    <link rel=\"shortcut icon\" href=\"/apple-touch-icon.png\" type=\"image/png\">\n"
			"    <link rel=\"apple-touch-icon\" href=\"/apple-touch-icon.png\" type=\"image/png\">\n"
			"    <link rel=\"stylesheet\" href=\"/style.css\">\n"
			"    <meta http-equiv=\"X-UA-Compatible\" content=\"IE=9\">\n", title, sw_name);
  if (refresh > 0)
    papplClientHTMLPrintf(client, "<meta http-equiv=\"refresh\" content=\"%d\">\n", refresh);
  papplClientHTMLPrintf(client,
			"    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n"
			"  </head>\n"
			"  <body>\n"
			"    <div class=\"header\">\n"
			"      <div class=\"row\">\n"
			"        <div class=\"col-3 nav\">\n"
			"          <a class=\"nav\" href=\"/\"><img class=\"nav\" src=\"/nav-icon.png\"> %s</a>\n"
			"        </div>\n"
			"        <div class=\"col-9 nav\">\n", sw_name);

  pthread_rwlock_rdlock(&system->rwlock);

  for (r = (_pappl_resource_t *)cupsArrayFirst(system->resources); r; r = (_pappl_resource_t *)cupsArrayNext(system->resources))
  {
    if (r->label)
      papplClientHTMLPrintf(client, "          <a class=\"nav\" href=\"%s\">%s</a>\n", r->path, r->label);
  }

  pthread_rwlock_unlock(&system->rwlock);

  papplClientHTMLPuts(client,
		      "        </div>\n"
		      "      </div>\n"
		      "    </div>\n");
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
