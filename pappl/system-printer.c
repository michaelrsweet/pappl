//
// Printer object for the Printer Application Framework
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
  _papplRWLockWrite(system);

  if (printer_id)
    printer->printer_id = printer_id;
  else
    printer->printer_id = system->next_printer_id ++;

  if (!system->printers)
    system->printers = cupsArrayNew((cups_array_cb_t)compare_printers, NULL, NULL, 0, NULL, (cups_afree_cb_t)_papplPrinterDelete);

  cupsArrayAdd(system->printers, printer);

  if (!system->default_printer_id)
    system->default_printer_id = printer->printer_id;

  _papplRWUnlock(system);

  _papplSystemConfigChanged(system);
  papplSystemAddEvent(system, printer, NULL, PAPPL_EVENT_PRINTER_CREATED | PAPPL_EVENT_SYSTEM_CONFIG_CHANGED, NULL);
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
  cups_len_t		i,		// Current printer index
			count;		// Printer count
  pappl_printer_t	*printer = NULL;// Matching printer


  // Range check input...
  if (!system)
    return (NULL);

  _papplRWLockRead(system);

  if (resource && (!strcmp(resource, "/") || !strcmp(resource, "/ipp/print") || (!strncmp(resource, "/ipp/print/", 11) && isdigit(resource[11] & 255))))
  {
    printer_id = system->default_printer_id;
    resource   = NULL;
  }

  // Loop through the printers to find the one we want...
  //
  // Note: Cannot use cupsArrayGetFirst/Last since other threads might be
  // enumerating the printers array.

  for (i = 0, count = cupsArrayGetCount(system->printers); i < count; i ++)
  {
    printer = (pappl_printer_t *)cupsArrayGetElement(system->printers, i);

    if (resource && !strncasecmp(printer->resource, resource, printer->resourcelen) && (!resource[printer->resourcelen] || resource[printer->resourcelen] == '/'))
      break;
    else if (printer->printer_id == printer_id)
      break;
    else if (device_uri && !strcmp(printer->device_uri, device_uri))
      break;
  }

  if (i >= count)
    printer = NULL;

  _papplRWUnlock(system);

  if (!printer)
  {
    if (resource)
      papplLog(system, PAPPL_LOGLEVEL_DEBUG, "Unable to find printer at '%s'.", resource);
    else
      papplLog(system, PAPPL_LOGLEVEL_DEBUG, "Unable to find printer with printer-id='%d'.", printer_id);
  }

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
