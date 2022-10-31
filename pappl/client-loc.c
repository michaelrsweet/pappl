//
// Client localization functions for the Printer Application Framework
//
// Copyright © 2022 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#include "client-private.h"
#include "loc-private.h"


//
// 'papplClientGetLoc()' - Get the localization data for a client connection.
//

pappl_loc_t *				// O - Localization data to use
papplClientGetLoc(
    pappl_client_t *client)		// I - Client
{
  // Range check input...
  if (!client)
    return (NULL);

  if (!client->loc)
  {
    const char	*language;		// Language name
    char	temp[7],		// Temporary language string
		*ptr;			// Pointer into temp string

    // Look up language
    if ((language = ippGetString(ippFindAttribute(client->request, "attributes-natural-language", IPP_TAG_LANGUAGE), 0, NULL)) != NULL)
    {
      // Use IPP language specification...
      papplLogClient(client, PAPPL_LOGLEVEL_DEBUG, "Using IPP language code '%s' for localization.", language);

      if ((client->loc = papplSystemFindLoc(client->system, language)) == NULL && language[2])
      {
        // Try the generic localization...
        papplCopyString(temp, language, sizeof(temp));
        temp[2]     = '\0';
	client->loc = papplSystemFindLoc(client->system, temp);
      }

      if (client->loc)
        papplLogClient(client, PAPPL_LOGLEVEL_DEBUG, "Using language '%s'.", client->loc->name);
    }
    else if (client->language[0])
    {
      // Parse language string...
      language = client->language;
      papplLogClient(client, PAPPL_LOGLEVEL_DEBUG, "Using HTTP Accept-Language value '%s' for localization.", language);

      while (*language)
      {
        // Grab the next language code.  The format (from RFC 7231) is:
        //
        // lang-code[;q=#][,...,land-code[;q=#]]
        for (ptr = temp; *language && *language != ';' && *language != ','; language ++)
        {
          if (ptr < (temp + sizeof(temp) - 1))
            *ptr++ = *language;
        }
        *ptr = '\0';

	if (*language == ';')
	{
	  // Skip "quality" parameter...
	  while (*language && *language != ',')
	    language ++;
	}

	if (*language == ',')
	{
	  // Skip comma and whitespace...
	  language ++;
	  while (*language && isspace(*language & 255))
	    language ++;
	}

	// Look up the language...
	if ((client->loc = papplSystemFindLoc(client->system, temp)) != NULL)
	  break;

	// If the lookup failed and this is a regional language request, try
	// the generic localization for that language...
	if (temp[2])
	{
	  // Truncate after the 2-digit language code...
	  temp[2] = '\0';

	  if ((client->loc = papplSystemFindLoc(client->system, temp)) != NULL)
	    break;
	}
      }

      if (client->loc)
        papplLogClient(client, PAPPL_LOGLEVEL_DEBUG, "Using language '%s'.", client->loc->name);
    }
    else
    {
      // Use default language...
      papplLogClient(client, PAPPL_LOGLEVEL_DEBUG, "Using default language 'en'.");
      client->loc = papplSystemFindLoc(client->system, "en");
    }
  }

  return (client->loc);
}


//
// 'papplClientGetLocString()' - Get a localized string for the client.
//

const char *				// O - Localized string
papplClientGetLocString(
    pappl_client_t *client,		// I - Client
    const char     *s)			// I - String to localize
{
  // Range check input...
  if (!client)
    return (NULL);

  if (client->loc)
    return (papplLocGetString(client->loc, s));
  else
    return (papplLocGetString(papplClientGetLoc(client), s));
}
