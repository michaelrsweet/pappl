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
}


//
// 'papplSystemFindLoc()' - Find a localization for the given printer and language.
//

pappl_loc_t *				// O - Localization or `NULL` if none
papplSystemFindLoc(
    pappl_system_t  *system,		// I - System
    pappl_printer_t *printer,		// I - Printer
    const char      *language)		// I - Language
{
  (void)system;
  (void)printer;
  (void)language;

  return (NULL);
}
