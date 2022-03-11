//
// System localization support for the Printer Application Framework
//
// Copyright © 2022 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#include "system-private.h"
#include "loc-private.h"


//
// '_papplSystemAddLoc()' - Add localization data to a system.
//

void
_papplSystemAddLoc(
    pappl_system_t *system,		// I - System
    pappl_loc_t    *loc)		// I - Localization data
{
  // Create an array to hold the localizations as needed, then add...
  pthread_rwlock_wrlock(&system->rwlock);

  if (!system->localizations)
    system->localizations = cupsArrayNew((cups_array_cb_t)_papplLocCompare, NULL, NULL, 0, NULL, (cups_afree_cb_t)_papplLocDelete);

  cupsArrayAdd(system->localizations, loc);

  pthread_rwlock_unlock(&system->rwlock);
}


//
// 'papplSystemFindLoc()' - Find a localization for the given printer and language.
//

pappl_loc_t *				// O - Localization or `NULL` if none
papplSystemFindLoc(
    pappl_system_t  *system,		// I - System
    const char      *language)		// I - Language
{
  pappl_loc_t	key,			// Search key
		*match;			// Matching localization


  // Range check input...
  if (!system || !language)
    return (NULL);

  // Find any existing localization...
  pthread_rwlock_rdlock(&system->rwlock);

  key.system   = system;
  key.language = (char *)language;

  match = cupsArrayFind(system->localizations, &key);

  pthread_rwlock_unlock(&system->rwlock);

  return (match);
}
