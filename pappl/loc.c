//
// Localization functions for the Printer Application Framework
//
// Copyright © 2022 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#include "loc-private.h"


//
// Local functions...
//

#if 0
static bool		loc_load_resource(pappl_loc_t *loc, _pappl_resource_t *res);
static int		locpair_compare(_pappl_locpair_t *a, _pappl_locpair_t *b);
static _pappl_locpair_t	*locpair_copy(_pappl_locpair_t *pair);
static void		locpair_free(_pappl_locpair_t *pair);
#endif // 0


//
// '_papplLocCompare()' - Compare two localizations.
//

int					// O - Result of comparison
_papplLocCompare(pappl_loc_t *a,	// I - First localization
                 pappl_loc_t *b)	// I - Second localization
{
  int	ret;				// Return value


  // Compare languages first...
  if ((ret = strcasecmp(a->language, b->language)) == 0)
  {
    // then printers...
    if (a->printer)
    {
      if (b->printer)
        ret = strcmp(a->printer->name, b->printer->name);
      else
        ret = 1;
    }
    else if (!a->printer && b->printer)
      ret = -1;
  }

  return (ret);
}


//
// '_papplLocCreate()' - Create a localization for the given system, printer, and language.
//

pappl_loc_t *				// O - New localization or `NULL` on error
_papplLocCreate(
    pappl_system_t  *system,		// I - System
    pappl_printer_t *printer,		// I - Printer
    const char      *language)		// I - Language
{
  (void)system;
  (void)printer;
  (void)language;

  return (NULL);
}


//
// '_papplLocDelete()' - Free memory used by a localization.
//

void
_papplLocDelete(pappl_loc_t *loc)	// I - Localization
{
  free(loc->language);
  cupsArrayDelete(loc->pairs);
  free(loc);
}


//
// 'papplLocFormatString()' - Format a localized string into a buffer.
//
// This function formats a localized string into a buffer using the specified
// localization data.  Numbers are formatted according to the locale and
// language used.
//

const char *				// O - Localized formatted string
papplLocFormatString(
    pappl_loc_t *loc,			// I - Localization data
    char        *buffer,		// I - Output buffer
    size_t      bufsize,		// I - Size of output buffer
    const char  *key,			// I - Printf-style key string to localize
    ...)				// I - Additional arguments as needed
{
  (void)loc;
  (void)bufsize;
  (void)key;

  if (buffer)
  {
    *buffer = '\0';
    return (buffer);
  }
  else
  {
    return (NULL);
  }
}


//
// 'papplLocGetString()' - Get a localized version of a key string.
//
// This function looks up the specified key string in the localization data and
// returns either the localized value or the original key string if no
// localization is available.
//

const char *				// O - Localized text string
papplLocGetString(pappl_loc_t *loc,	// I - Localization data
                  const char  *key)	// I - Key text
{
  _pappl_locpair_t	search,		// Search key
			*match;		// Matching pair, if any


  // Range check input...
  if (!loc)
    return (key);

  // Look up the key...
  search.key = (char *)key;
  if ((match = cupsArrayFind(loc->pairs, &search)) != NULL)
    return (match->text);
  else
    return (key);
}


#if 0
//
// 'loc_load_resource()' - Load a strings resource into a localization.
//

static bool				// O - `true` on success, `false` otherwise
loc_load_resource(
    pappl_loc_t       *loc,		// I - Localization data
    _pappl_resource_t *res)		// I - Resource
{
  (void)loc;
  (void)res;
  return (false);
}


//
// 'locpair_compare()' - Compare the keys of two key/text pairs.
//

static int				// O - Result of comparison
locpair_compare(_pappl_locpair_t *a,	// I - First key/text pair
                _pappl_locpair_t *b)	// I - Second key/text pair
{
  return (strcmp(a->key, b->key));
}


//
// 'locpair_copy()' - Copy a key/text pair.
//

static _pappl_locpair_t	*		// O - Copy of pair
locpair_copy(_pappl_locpair_t *pair)	// I - Pair to copy
{
  _pappl_locpair_t	*npair;		// New pair


  // Allocate a new pair...
  if ((npair = calloc(1, sizeof(_pappl_locpair_t))) != NULL)
  {
    // Duplicate the strings...
    npair->key  = strdup(pair->key);
    npair->text = strdup(pair->text);

    if (!npair->key || !npair->text)
    {
      // Wasn't able to allocate everything...
      locpair_free(npair);
      npair = NULL;
    }
  }

  return (npair);
}


//
// 'locpair_free()' - Free memory used by a key/text pair.
//

static void
locpair_free(_pappl_locpair_t *pair)	// I - Pair
{
  free(pair->key);
  free(pair->text);
  free(pair);
}
#endif // 0
