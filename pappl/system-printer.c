//
// Printer object for the Printer Application Framework
//
// Copyright © 2019-2023 by Michael R Sweet.
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
static int  compare_scanners(pappl_scanner_t *a, pappl_scanner_t *b);


//
// '_papplSystemAddScanner()' - Add a scanner to the system object, creating the scanners array as needed.
//

void
_papplSystemAddScanner(
    pappl_system_t  *system,		// I - System
    pappl_scanner_t *scanner,		// I - Scanner
    int             scanner_id)		// I - Scanner ID or `0` for new
{
  // Add the scanner to the system...
  _papplRWLockWrite(system);

  if (scanner_id)
    scanner->scanner_id = scanner_id;
  else
    scanner->scanner_id = system->next_scanner_id ++;

  if (!system->scanners)
    system->scanners = cupsArrayNew((cups_array_cb_t)compare_scanners, /*cb_data*/NULL, /*hash_cb*/NULL, /*hash_size*/0, /*copy_cb*/NULL, /*free_cb*/NULL);

  cupsArrayAdd(system->scanners, scanner);

  if (!system->default_scanner_id)
    system->default_scanner_id = scanner->scanner_id;

  _papplRWUnlock(system);

  _papplSystemConfigChanged(system);

  papplSystemAddScannerEvent(scanner->system, scanner, NULL, PAPPL_EVENT_SCANNER_STATE_CHANGED, NULL);
}

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
    system->printers = cupsArrayNew((cups_array_cb_t)compare_printers, /*cb_data*/NULL, /*hash_cb*/NULL, /*hash_size*/0, /*copy_cb*/NULL, /*free_cb*/NULL);

  cupsArrayAdd(system->printers, printer);

  if (!system->default_printer_id)
    system->default_printer_id = printer->printer_id;

  _papplRWUnlock(system);

  _papplSystemConfigChanged(system);
  papplSystemAddEvent(system, printer, NULL, PAPPL_EVENT_PRINTER_CREATED | PAPPL_EVENT_SYSTEM_CONFIG_CHANGED, NULL);
}


//
// 'papplSystemCreatePrinters()' - Create newly discovered printers.
//
// This function lists all devices specified by "types" and attempts to add any
// new printers that are found.  The callback function "cb" is invoked for each
// printer that is added.
//

bool					// O - `true` if printers were added, `false` otherwise
papplSystemCreatePrinters(
    pappl_system_t       *system,	// I - System
    pappl_devtype_t      types,		// I - Device types
    pappl_pr_create_cb_t cb,		// I - Callback function
    void                 *cb_data)	// I - Callback data
{
  bool			ret = false;	// Return value
  cups_array_t		*devices;	// Device array
  _pappl_dinfo_t	*d;		// Current device information


  // List the devices...
  devices = _papplDeviceInfoCreateArray();

  papplDeviceList(types, (pappl_device_cb_t)_papplDeviceInfoCallback, devices, papplLogDevice, system);

  // Loop through the devices to find new stuff...
  for (d = (_pappl_dinfo_t *)cupsArrayGetFirst(devices); d; d = (_pappl_dinfo_t *)cupsArrayGetNext(devices))
  {
    pappl_printer_t	*printer = NULL;// New printer

    // See if there is already a printer for this device URI...
    if (papplSystemFindPrinter(system, NULL, 0, d->device_uri))
      continue;			// Printer with this device URI exists

    // Then try creating the printer...
    if ((printer = papplPrinterCreate(system, 0, d->device_info, "auto", d->device_id, d->device_uri)) == NULL)
      continue;			// Printer with this name exists

    // Register the DNS-SD service...
    _papplRWLockRead(printer->system);
      _papplRWLockRead(printer);
	_papplPrinterRegisterDNSSDNoLock(printer);
      _papplRWUnlock(printer);
    _papplRWUnlock(printer->system);

    // Created, return true and invoke the callback if provided...
    ret = true;

    if (cb)
      (cb)(printer, cb_data);
  }

  cupsArrayDelete(devices);

  return (ret);
}


//
// 'papplSystemCreateScanners()' - Create newly discovered scanners.
//
// This function lists all devices specified by "types" and attempts to add any
// new scanners that are found.  The callback function "cb" is invoked for each
// scanner that is added.
//

bool					// O - `true` if printers were added, `false` otherwise
papplSystemCreateScanners(
    pappl_system_t       *system,	// I - System
    pappl_devtype_t      types,		// I - Device types
    pappl_sc_create_cb_t cb,		// I - Callback function
    void                 *cb_data)	// I - Callback data
{
  bool			ret = false;	// Return value
  cups_array_t		*devices;	// Device array
  _pappl_dinfo_t	*d;		// Current device information

  // List the devices...
  devices = _papplDeviceInfoCreateArray();

  papplDeviceList(types, (pappl_device_cb_t)_papplDeviceInfoCallback, devices, papplLogDevice, system);

  // Loop through the devices to find new stuff...
  for (d = (_pappl_dinfo_t *)cupsArrayGetFirst(devices); d; d = (_pappl_dinfo_t *)cupsArrayGetNext(devices))
  {
    pappl_scanner_t	*scanner = NULL;// New scanner

    // See if there is already a scanner for this device URI...
    if (papplSystemFindScanner(system, NULL, 0, d->device_uri))
      continue;			// Scanner with this device URI exists

    // Then try creating the scanner...
    if ((scanner = papplScannerCreate(system, 0, d->device_info, "auto", d->device_id, d->device_uri)) == NULL)
      continue;			// Scanner with this name exists

    // Register the DNS-SD service...
    _papplRWLockRead(scanner->system);
      _papplRWLockRead(scanner);
	_papplScannerRegisterDNSSDNoLock(scanner);
      _papplRWUnlock(scanner);
    _papplRWUnlock(scanner->system);

    // Created, return true and invoke the callback if provided...
    ret = true;

    if (cb)
      (cb)(scanner, cb_data);
  }

  cupsArrayDelete(devices);

  return (ret);
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

  return (printer);
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
    int            scanner_id,		// I - Printer ID or `0`
    const char     *device_uri)		// I - Device URI or `NULL`
{
  cups_len_t		i,		// Current scanner index
			count;		// scanner count
  pappl_scanner_t	*scanner = NULL;// Matching scanner


  // Range check input...
  if (!system)
    return (NULL);

  _papplRWLockRead(system);

  if (resource && (!strcmp(resource, "/") || !strcmp(resource, "/escl/scan") || (!strncmp(resource, "/escl/scan/", 11) && isdigit(resource[11] & 255))))
  {
    scanner_id = system->default_scanner_id;
    resource   = NULL;
  }

  // Loop through the scanners to find the one we want...
  //
  // Note: Cannot use cupsArrayGetFirst/Last since other threads might be
  // enumerating the scanners array.

  for (i = 0, count = cupsArrayGetCount(system->scanners); i < count; i ++)
  {
    scanner = (pappl_scanner_t *)cupsArrayGetElement(system->scanners, i);

    if (resource && !strncasecmp(scanner->resource, resource, scanner->resourcelen) && (!resource[scanner->resourcelen] || resource[scanner->resourcelen] == '/'))
      break;
    else if (scanner->scanner_id == scanner_id)
      break;
    else if (device_uri && !strcmp(scanner->device_uri, device_uri))
      break;
  }

  if (i >= count)
    scanner = NULL;

  _papplRWUnlock(system);

  return (scanner);
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


//
// 'compare_scanners())' - Compare two scanners.
//
static int				// O - Result of comparison
compare_scanners(pappl_scanner_t *a,	// I - First scanner
                 pappl_scanner_t *b)	// I - Second scanner
{
  return (strcmp(a->name, b->name));
}
