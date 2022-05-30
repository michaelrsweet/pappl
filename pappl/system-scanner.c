//
// Scanner object for the Scanner Application Framework
//
// Copyright © 2019-2022 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

//
// Include necessary headers...
//

#include "pappl-private.h"


//
// Local functions...
//

static int	compare_scanners(pappl_scanner_t *a, pappl_scanner_t *b);


//
// '_papplSystemAddScanner()' - Add a scanner to the system object, creating the scanners array as needed.
//

void
_papplSystemAddScanner(
    pappl_system_t  *system,		// I - System
    pappl_scanner_t *scanner,		// I - Scanner
    int             printer_id)		// I - Scanner ID or `0` for new
{
  // Add the scanner to the system...
  pthread_rwlock_wrlock(&system->rwlock);

  if (printer_id)
    scanner->printer_id = printer_id;
  else
    scanner->printer_id = system->next_printer_id ++;

  if (!system->scanners)
    system->scanners = cupsArrayNew((cups_array_cb_t)compare_scanners, NULL, NULL, 0, NULL, (cups_afree_cb_t)_papplScannerDelete);

  cupsArrayAdd(system->scanners, scanner);

  if (!system->default_printer_id)
    system->default_printer_id = scanner->printer_id;

  pthread_rwlock_unlock(&system->rwlock);

  _papplSystemConfigChanged(system);
}


//
// 'papplSystemFindScanner()' - Find a scanner by resource, ID, or device URI.
//
// This function finds a scanner contained in the system using its resource
// path, unique integer identifier, or device URI.  If none of these is
// specified, the current default scanner is returned.
//

pappl_scanner_t *			// O - Scanner or `NULL` if none
papplSystemFindScanner(
    pappl_system_t *system,		// I - System
    const char     *resource,		// I - Resource path or `NULL`
    int            printer_id,		// I - Scanner ID or `0`
    const char     *device_uri)		// I - Device URI or `NULL`
{
  int			i,		// Current scanner index
			count;		// Scanner count
  pappl_scanner_t	*scanner = NULL;// Matching scanner


  // Range check input...
  if (!system)
    return (NULL);

  papplLog(system, PAPPL_LOGLEVEL_DEBUG, "papplSystemFindScanner(system=%p, resource=\"%s\", printer_id=%d, device_uri=\"%s\")", (void *)system, resource, printer_id, device_uri);

  pthread_rwlock_rdlock(&system->rwlock);

  if (resource && (!strcmp(resource, "/") || !strcmp(resource, "/ipp/scan") || (!strncmp(resource, "/ipp/scan/", 11) && isdigit(resource[11] & 255))))
  {
    printer_id = system->default_printer_id;
    resource   = NULL;

    papplLog(system, PAPPL_LOGLEVEL_DEBUG, "papplSystemFindScanner: Looking for default printer_id=%d", printer_id);
  }

  // Loop through the scanners to find the one we want...
  //
  // Note: Cannot use cupsArrayFirst/Last since other threads might be
  // enumerating the scanners array.

  for (i = 0, count = cupsArrayGetCount(system->scanners); i < count; i ++)
  {
    scanner = (pappl_scanner_t *)cupsArrayGetElement(system->scanners, i);

    papplLog(system, PAPPL_LOGLEVEL_DEBUG, "papplSystemFindScanner: scanner '%s' - resource=\"%s\", printer_id=%d, device_uri=\"%s\"", scanner->name, scanner->resource, scanner->printer_id, scanner->device_uri);

    if (resource && !strncasecmp(scanner->resource, resource, scanner->resourcelen) && (!resource[scanner->resourcelen] || resource[scanner->resourcelen] == '/'))
      break;
    else if (scanner->printer_id == printer_id)
      break;
    else if (device_uri && !strcmp(scanner->device_uri, device_uri))
      break;
  }

  if (i >= count)
    scanner = NULL;

  pthread_rwlock_unlock(&system->rwlock);

  papplLog(system, PAPPL_LOGLEVEL_DEBUG, "papplSystemFindScanner: Returning %p(%s)", scanner, scanner ? scanner->name : "none");

  return (scanner);
}


//
// 'compare_scanners()' - Compare two scanners.
//

static int				// O - Result of comparison
compare_scanners(pappl_scanner_t *a,	// I - First scanner
                 pappl_scanner_t *b)	// I - Second scanner
{
  return (strcmp(a->name, b->name));
}
