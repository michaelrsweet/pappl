//
// System scanner management for the Printer Application Framework
//
// Copyright © 2019-2026 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#include "pappl-private.h"
#include "device-private.h"


//
// Local functions...
//

static int	compare_scanners(pappl_scanner_t *a, pappl_scanner_t *b);


//
// '_papplSystemAddScanner()' - Add a scanner to the system object, creating
//                              the scanners array as needed.
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

  cupsRWLockWrite(&system->scanners_rwlock);
  if (!system->scanners)
    system->scanners = cupsArrayNew((cups_array_cb_t)compare_scanners, /*cb_data*/NULL, /*hash_cb*/NULL, /*hash_size*/0, /*copy_cb*/NULL, /*free_cb*/NULL);

  cupsArrayAdd(system->scanners, scanner);
  cupsRWUnlock(&system->scanners_rwlock);

  if (!system->default_scanner_id)
    system->default_scanner_id = scanner->scanner_id;

  _papplRWUnlock(system);

  _papplSystemConfigChanged(system);
}


//
// '_papplSystemFindScanner()' - Find a scanner by resource, ID, or device URI.
//

pappl_scanner_t *			// O - Scanner or `NULL` if none
_papplSystemFindScanner(
    pappl_system_t *system,		// I - System
    const char     *resource,		// I - Resource path or `NULL`
    int            scanner_id,		// I - Scanner ID or `0`
    const char     *device_uri)		// I - Device URI or `NULL`
{
  size_t		i,		// Current scanner index
			count;		// Scanner count
  pappl_scanner_t	*scanner = NULL;// Matching scanner


  // Range check input...
  if (!system)
    return (NULL);

  _papplRWLockRead(system);

  if (resource && (!strcmp(resource, "/") || !strcmp(resource, "/ipp/scan") || (!strncmp(resource, "/ipp/scan/", 10) && isdigit(resource[10] & 255))))
  {
    scanner_id = system->default_scanner_id;
    resource   = NULL;
  }

  // Loop through the scanners to find the one we want...
  //
  // Note: Cannot use cupsArrayGetFirst/Last since other threads might be
  // enumerating the scanners array.

  cupsRWLockRead(&system->scanners_rwlock);

  for (i = 0, count = cupsArrayGetCount(system->scanners); i < count; i ++)
  {
    scanner = (pappl_scanner_t *)cupsArrayGetElement(system->scanners, i);

    if (resource && !strncasecmp(scanner->resource, resource, scanner->resourcelen) && (!resource[scanner->resourcelen] || resource[scanner->resourcelen] == '/'))
      break;
    else if (scanner->scanner_id == scanner_id)
      break;
    else if (device_uri && scanner->device_uri && !strcmp(scanner->device_uri, device_uri))
      break;
  }

  cupsRWUnlock(&system->scanners_rwlock);

  if (i >= count)
    scanner = NULL;

  _papplRWUnlock(system);

  return (scanner);
}


//
// 'papplSystemCreateScanners()' - Create newly discovered scanners.
//
// This function lists all devices specified by "types" and attempts to add any
// new scanners that are found.  The callback function "cb" is invoked for each
// scanner that is added.
//

bool					// O - `true` if scanners were added, `false` otherwise
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
    pappl_scanner_t	*scanner = NULL;
					// New scanner

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
    int            scanner_id,		// I - Scanner ID or `0`
    const char     *device_uri)		// I - Device URI or `NULL`
{
  return (_papplSystemFindScanner(system, resource, scanner_id, device_uri));
}


//
// 'papplSystemSetScannerDrivers()' - Set the list of scanner drivers and the
//                                    driver callbacks.
//
// This function sets the lists of scanner drivers and the driver callbacks for
// a system.  It is typically called from @link papplSystemCreate@.
//

void
papplSystemSetScannerDrivers(
    pappl_system_t       *system,	// I - System
    size_t               num_drivers,	// I - Number of drivers
    pappl_sc_driver_t    *drivers,	// I - Drivers
    pappl_sc_autoadd_cb_t autoadd_cb,	// I - Auto-add callback or `NULL`
    pappl_sc_create_cb_t create_cb,	// I - Creation callback or `NULL`
    pappl_sc_driver_cb_t driver_cb,	// I - Driver initialization callback
    void                 *data)		// I - Callback data
{
  if (!system || !drivers || !driver_cb)
    return;

  system->num_sc_drivers    = num_drivers;
  system->sc_drivers        = drivers;
  system->sc_autoadd_cb     = autoadd_cb;
  system->sc_create_cb      = create_cb;
  system->sc_driver_cb      = driver_cb;
  system->sc_driver_cbdata  = data;
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

