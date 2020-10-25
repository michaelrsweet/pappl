//
// Printer object for the Printer Application Framework
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
// Local functions...
//

static int	compare_printers(pappl_printer_t *a, pappl_printer_t *b);


//
// '_papplSystemAddPrinter()' - Add a printer to the system object, creating the printers array as needed.
//

void
_papplSystemAddPrinter(
    pappl_system_t  *system,		// I - System
    pappl_printer_t *printer,		// I - Printer
    int             printer_id)		// I - Printer ID or `0` for new
{
  // Add the printer to the system...
  pthread_rwlock_wrlock(&system->rwlock);

  if (printer_id)
    printer->printer_id = printer_id;
  else
    printer->printer_id = system->next_printer_id ++;

  if (!system->printers)
    system->printers = cupsArrayNew3((cups_array_func_t)compare_printers, NULL, NULL, 0, NULL, (cups_afree_func_t)_papplPrinterDelete);

  cupsArrayAdd(system->printers, printer);

  if (!system->default_printer_id)
    system->default_printer_id = printer->printer_id;

  pthread_rwlock_unlock(&system->rwlock);

  _papplSystemConfigChanged(system);
}


//
// 'papplSystemFindPrinter()' - Find a printer by resource, ID, or device URI.
//
// This function finds a printer contained in the system using its resource
// path, unique integer identifier, or device URI.  If none of these is
// specified, the current default printer is returned.
//

pappl_printer_t *			// O - Printer or `NULL` if none
papplSystemFindPrinter(
    pappl_system_t *system,		// I - System
    const char     *resource,		// I - Resource path or `NULL`
    int            printer_id,		// I - Printer ID or `0`
    const char     *device_uri)		// I - Device URI or `NULL`
{
  pappl_printer_t	*printer;	// Matching printer


  papplLog(system, PAPPL_LOGLEVEL_DEBUG, "papplSystemFindPrinter(system, resource=\"%s\", printer_id=%d, device_uri=\"%s\")", resource, printer_id, device_uri);

  pthread_rwlock_rdlock(&system->rwlock);

  if (resource && (!strcmp(resource, "/") || !strcmp(resource, "/ipp/print") || (!strncmp(resource, "/ipp/print/", 11) && isdigit(resource[11] & 255))))
  {
    printer_id = system->default_printer_id;
    resource   = NULL;

    papplLog(system, PAPPL_LOGLEVEL_DEBUG, "papplSystemFindPrinter: Looking for default printer_id=%d", printer_id);
  }

  for (printer = (pappl_printer_t *)cupsArrayFirst(system->printers); printer; printer = (pappl_printer_t *)cupsArrayNext(system->printers))
  {
    papplLog(system, PAPPL_LOGLEVEL_DEBUG, "papplSystemFindPrinter: printer '%s' - resource=\"%s\", printer_id=%d, device_uri=\"%s\"", printer->name, printer->resource, printer->printer_id, printer->device_uri);

    if (resource && !strncmp(printer->resource, resource, printer->resourcelen) && (!resource[printer->resourcelen] || resource[printer->resourcelen] == '/'))
      break;
    else if (printer->printer_id == printer_id)
      break;
    else if (device_uri && !strcmp(printer->device_uri, device_uri))
      break;
  }
  pthread_rwlock_unlock(&system->rwlock);

  papplLog(system, PAPPL_LOGLEVEL_DEBUG, "papplSystemFindPrinter: Returning %p(%s)", printer, printer ? printer->name : "none");

  return (printer);
}


//
// 'compare_printers()' - Compare two printers.
//

static int				// O - Result of comparison
compare_printers(pappl_printer_t *a,	// I - First printer
                 pappl_printer_t *b)	// I - Second printer
{
  return (strcmp(a->name, b->name));
}
