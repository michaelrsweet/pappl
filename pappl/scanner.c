//
// scanner object for the Scanner Application Framework
//
// Copyright © 2019-2024 by Michael R Sweet.
// Copyright © 2010-2019 by Apple Inc.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#include "pappl-private.h"
#include "scanner-private.h"
#include "scanner.h"
#include "system-private.h"

//
// 'papplScannerCreate()' - Create a new scanner.
//
// This function creates a new scanner (service) on the specified system. The
// "scanner_id" argument specifies a positive integer identifier that is
// unique to the system. If you specify a value of `0`, a new identifier will
// be assigned.
//
// The "scanner_name" argument specifies a human-readable name for the scanner.
//
// The "driver_name" argument specifies a named driver for the scanner.
//
// The "device_id" and "device_uri" arguments specify the device ID
// and device URI strings for the scanner.
//
// On error, this function sets the `errno` variable to one of the following
// values:
//
// - `EEXIST`: A scanner with the specified name already exists.
// - `EINVAL`: Bad values for the arguments were specified.
// - `EIO`: The driver callback failed.
// - `ENOENT`: No driver callback has been set.
// - `ENOMEM`: Ran out of memory.
//


pappl_scanner_t *			// O - Scanner or `NULL` on error
papplScannerCreate(
    pappl_system_t       *system,	    // I - System
    int                  scanner_id,	    // I - scanner-id value or `0` for new
    const char           *scanner_name,	// I - Human-readable scanner name
    const char           *driver_name,	// I - Driver name
    const char           *device_id,	// I - IEEE-1284 device ID
    const char           *device_uri)	// I - Device URI
{
  pappl_scanner_t	*scanner;	// Scanner
  pappl_sc_driver_data_t driver_data;	// Driver data
  char			        resource[1024],	// Resource path
			            *resptr,	// Pointer into resource path
			            uuid[128];	// scanner-uuid
  char			path[256];	// Path to resource

  // Range check input...
  if (!system || !scanner_name || !driver_name || !device_uri)
  {
    errno = EINVAL;
    return (NULL);
  }

  if (!system->driver_cb)
  {
    papplLog(system, PAPPL_LOGLEVEL_ERROR, "No driver callback set, unable to add scanner.");
    errno = ENOENT;
    return (NULL);
  }

  // Prepare URI values for the scanner attributes...
  if (system->options & PAPPL_SOPTIONS_MULTI_QUEUE)
  {
    // Make sure scanner names that start with a digit have a resource path
    // containing an underscore...
    if (isdigit(*scanner_name & 255))
      snprintf(resource, sizeof(resource), "/escl/scan/_%s", scanner_name);
    else
      snprintf(resource, sizeof(resource), "/escl/scan/%s", scanner_name);

    // Convert URL reserved characters to underscore...
    for (resptr = resource + 11; *resptr; resptr ++)
    {
      if ((*resptr & 255) <= ' ' || strchr("\177/\\\'\"?#", *resptr))
	*resptr = '_';
    }

    // Eliminate duplicate and trailing underscores...
    resptr = resource + 11;
    while (*resptr)
    {
      if (resptr[0] == '_' && resptr[1] == '_')
        memmove(resptr, resptr + 1, strlen(resptr));	// Duplicate underscores
      else if (resptr[0] == '_' && !resptr[1])
        *resptr = '\0';		// Trailing underscore
      else
        resptr ++;
    }
  }
  else
  {
    papplCopyString(resource, "/escl/scan", sizeof(resource));
  }

  // Make sure the scanner doesn't already exist...
  if ((scanner = papplSystemFindScanner(system, resource, 0, NULL)) != NULL)
  {
    int		n;		// Current instance number
    char	temp[1024];	// Temporary resource path

    if (!strcmp(scanner_name, scanner->name))
    {
      papplLog(system, PAPPL_LOGLEVEL_ERROR, "Scanner '%s' already exists.", scanner_name);
      errno = EEXIST;
      return (NULL);
    }

    for (n = 2; n < 10; n ++)
    {
      snprintf(temp, sizeof(temp), "%s_%d", resource, n);
      if (!papplSystemFindScanner(system, temp, 0, NULL))
        break;
    }

    if (n >= 10)
    {
      papplLog(system, PAPPL_LOGLEVEL_ERROR, "Scanner '%s' name conflicts with existing scanner.", scanner_name);
      errno = EEXIST;
      return (NULL);
    }

    papplCopyString(resource, temp, sizeof(resource));
  }

  // Allocate memory for the scanner...
  if ((scanner = calloc(1, sizeof(pappl_scanner_t))) == NULL)
  {
    papplLog(system, PAPPL_LOGLEVEL_ERROR, "Unable to allocate memory for scanner: %s", strerror(errno));
    return (NULL);
  }

  papplLog(system, PAPPL_LOGLEVEL_INFO, "Scanner '%s' at resource path '%s'.", scanner_name, resource);

  _papplSystemMakeUUID(system, scanner_name, 0, uuid, sizeof(uuid));

  // Initialize scanner structure and attributes...
  pthread_rwlock_init(&scanner->rwlock, NULL);

  scanner->system             = system;
  scanner->name               = strdup(scanner_name);
  scanner->dns_sd_name        = strdup(scanner_name);
  scanner->resource           = strdup(resource);
  scanner->resourcelen        = strlen(resource);
  scanner->uriname            = scanner->resource + 10; // Skip "/escl/scan" in resource
  scanner->device_id          = device_id ? strdup(device_id) : NULL;
  scanner->device_uri         = strdup(device_uri);
  scanner->driver_name        = strdup(driver_name);
  scanner->uuid               = strdup(uuid);
  scanner->start_time         = time(NULL);
  scanner->config_time        = scanner->start_time;
  scanner->state              = ESCL_SSTATE_IDLE;
  scanner->state_reasons      = PAPPL_SREASON_NONE;
  scanner->state_time         = scanner->start_time;
  scanner->is_accepting       = true;
  scanner->next_job_id        = 1;
  scanner->processing_job     = NULL; // Initialize to NULL
  scanner->device             = NULL; // Initialize to NULL
  scanner->device_in_use      = false; // Initialize to false

  // Check for memory allocation failures
  if (!scanner->name || !scanner->dns_sd_name || !scanner->resource || (device_id && !scanner->device_id) || !scanner->device_uri || !scanner->driver_name || !scanner->uuid)
  {
    // Failed to allocate one of the required members...
    _papplScannerDelete(scanner);
    return (NULL);
  }

  // If the driver is "auto", figure out the proper driver name...
  if (!strcmp(driver_name, "auto") && system->autoadd_sc_cb)
  {
    // If device_id is NULL, try to look it up...
    if (!scanner->device_id && strncmp(device_uri, "file://", 7))
    {
      pappl_device_t	*device;	// Connection to scanner

      if ((device = papplDeviceOpen(device_uri, "auto", papplLogDevice, system)) != NULL)
      {
        char	new_id[1024];		// New device ID

        if (papplDeviceGetID(device, new_id, sizeof(new_id)))
          scanner->device_id = strdup(new_id);

        papplDeviceClose(device);
      }
    }

    if ((driver_name = (system->autoadd_sc_cb)(scanner_name, device_uri, scanner->device_id, system->sc_driver_cbdata)) == NULL)
    {
      errno = EIO;
      _papplScannerDelete(scanner);
      return (NULL);
    }
  }


  _papplScannerInitDriverData(scanner,&driver_data);

  if (!(system->driver_sc_cb)(system, driver_name, device_uri, device_id, &driver_data, system->sc_driver_cbdata))
  {
    errno = EIO;
    _papplScannerDelete(scanner);
    return (NULL);
  }

  papplScannerSetDriverData(scanner, &driver_data);

  // Add the scanner to the system...
  _papplSystemAddScanner(system, scanner, scanner_id);

  // Do any post-creation work...
  if (system->create_sc_cb)
    (system->create_sc_cb)(scanner, system->sc_driver_cbdata);

  // Add icons...
  _papplSystemAddScannerIcons(system, scanner);

  // Add web pages, if any...
  if (system->options & PAPPL_SOPTIONS_WEB_INTERFACE)
  {
    snprintf(path, sizeof(path), "%s/", scanner->uriname);
    papplSystemAddResourceCallback(system, path, "text/html", (pappl_resource_cb_t)_papplScannerWebHome, scanner);

    snprintf(path, sizeof(path), "%s/delete", scanner->uriname);
    papplSystemAddResourceCallback(system, path, "text/html", (pappl_resource_cb_t)_papplScannerWebDelete, scanner);

    snprintf(path, sizeof(path), "%s/config", scanner->uriname);
    papplSystemAddResourceCallback(system, path, "text/html", (pappl_resource_cb_t)_papplScannerWebConfig, scanner);

    snprintf(path, sizeof(path), "%s/printing", scanner->uriname);
    papplSystemAddResourceCallback(system, path, "text/html", (pappl_resource_cb_t)_papplScannerWebDefaults, scanner);

  }

  _papplSystemConfigChanged(system);

  // Return it!
  return (scanner);
}

//
// '_papplScannerDelete()' - Free memory associated with a scanner.
//

void
_papplScannerDelete(
    pappl_scanner_t *scanner)		// I - Scanner
{
  _pappl_resource_t	*r;		// Current resource
  char			prefix[1024];	// Prefix for scanner resources
  size_t		prefixlen;	// Length of prefix


  _papplRWLockWrite(scanner);
  scanner->is_deleted = true;
  _papplRWUnlock(scanner);

  // Remove DNS-SD registrations...
  _papplScannerUnregisterDNSSDNoLock(scanner);

  // Remove scanner-specific resources...
  snprintf(prefix, sizeof(prefix), "%s/", scanner->uriname);
  prefixlen = strlen(prefix);

  // Note: System writer lock is already held when calling cupsArrayRemove
  // for the system's scanner object, so we don't need a separate lock here
  // and can safely use cupsArrayGetFirst/Next...
  _papplRWLockWrite(scanner->system);
  for (r = (_pappl_resource_t *)cupsArrayGetFirst(scanner->system->resources); r; r = (_pappl_resource_t *)cupsArrayGetNext(scanner->system->resources))
  {
    if (r->cbdata == scanner || !strncmp(r->path, prefix, prefixlen))
      cupsArrayRemove(scanner->system->resources, r);
  }
  _papplRWUnlock(scanner->system);

  // If applicable, call the delete function...
  if (scanner->driver_data.sc_delete_cb)
    (scanner->driver_data.sc_delete_cb)(scanner, &scanner->driver_data);

  // Free memory...
  free(scanner->name);
  free(scanner->dns_sd_name);
  free(scanner->location);
  free(scanner->geo_location);
  free(scanner->organization);
  free(scanner->resource);
  free(scanner->device_id);
  free(scanner->device_uri);
  free(scanner->driver_name);
  free(scanner->uuid);

  cupsArrayDelete(scanner->links);

  pthread_rwlock_destroy(&scanner->rwlock);

  free(scanner);
}


//
// 'papplScannerDelete()' - Delete a scanner.
//
// This function deletes a scanner from a system, freeing all memory and
// canceling all jobs as needed.
//

void
papplScannerDelete(
    pappl_scanner_t *scanner)		// I - Scanner
{
  pappl_system_t *system = scanner->system;
					// System

  // Deliver delete event...
  papplSystemAddScannerEvent(scanner->system, scanner, NULL, PAPPL_EVENT_SCANNER_STATE_CHANGED | PAPPL_EVENT_SYSTEM_CONFIG_CHANGED, NULL);

  // Remove the scanner from the system object...
  _papplRWLockWrite(system);
  cupsArrayRemove(system->scanners, scanner);
  _papplRWUnlock(system);

  _papplScannerDelete(scanner);

  _papplSystemConfigChanged(system);
}
