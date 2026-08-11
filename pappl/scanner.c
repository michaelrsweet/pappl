//
// Scanner object for the Printer Application Framework
//
// Copyright © 2019-2026 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#include "pappl-private.h"


//
// Local functions...
//

static int	compare_active_jobs(pappl_job_t *a, pappl_job_t *b);
static int	compare_all_jobs(pappl_job_t *a, pappl_job_t *b);
static int	compare_completed_jobs(pappl_job_t *a, pappl_job_t *b);


//
// 'papplScannerCancelAllJobs()' - Cancel all jobs on the scanner.
//
// This function cancels all jobs on the scanner.  If any job is currently being
// processed, it will be stopped at a convenient time so that the scanner will
// be left in a known state.
//

void
papplScannerCancelAllJobs(
    pappl_scanner_t *scanner)		// I - Scanner
{
  pappl_job_t	*job;			// Job information


  // Loop through all jobs and cancel them...
  _papplRWLockWrite(scanner);

  for (job = (pappl_job_t *)cupsArrayGetFirst(scanner->active_jobs); job; job = (pappl_job_t *)cupsArrayGetNext(scanner->active_jobs))
  {
    if (job->state == IPP_JSTATE_PROCESSING)
    {
      job->is_canceled = true;
    }
    else
    {
      job->state     = IPP_JSTATE_CANCELED;
      job->completed = time(NULL);

      cupsArrayRemove(scanner->active_jobs, job);
      cupsArrayAdd(scanner->completed_jobs, job);
    }
  }

  _papplRWUnlock(scanner);

  if (!scanner->system->clean_time)
    scanner->system->clean_time = time(NULL) + 60;
}


//
// 'papplScannerCreate()' - Create a new scanner.
//
// This function creates a new scanner (service) on the specified system.  The
// "scanner_id" argument specifies a positive integer identifier that is unique
// to the system.  If you specify a value of `0` a new identifier will be
// assigned.
//
// The "driver_name" argument specifies a named driver for the scanner, from
// the list of drivers registered with the @link papplSystemSetScannerDrivers@
// function.
//
// The "device_id" and "device_uri" arguments specify the IEEE-1284 device ID
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
    pappl_system_t *system,		// I - System
    int            scanner_id,		// I - scanner-id value or `0` for new
    const char     *scanner_name,	// I - Human-readable scanner name
    const char     *driver_name,	// I - Driver name
    const char     *device_id,		// I - IEEE-1284 device ID
    const char     *device_uri)		// I - Device URI
{
  pappl_scanner_t	*scanner;	// Scanner
  char			resource[1024],	// Resource path
			*resptr,	// Pointer into resource path
			uuid[128];	// scanner-uuid
  pappl_sc_driver_data_t driver_data;	// Driver data
  ipp_t			*driver_attrs;	// Driver attributes


  // Range check input...
  if (!system || !scanner_name || !driver_name)
  {
    errno = EINVAL;
    return (NULL);
  }

  if (!device_uri)
  {
    errno = EINVAL;
    return (NULL);
  }

  if (!system->sc_driver_cb)
  {
    papplLog(system, PAPPL_LOGLEVEL_ERROR, "No scanner driver callback set, unable to add scanner.");
    errno = ENOENT;
    return (NULL);
  }

  // Prepare URI values for the scanner attributes...
  if (system->options & PAPPL_SOPTIONS_MULTI_QUEUE)
  {
    // Make sure scanner names that start with a digit have a resource path
    // containing an underscore...
    if (isdigit(*scanner_name & 255))
      snprintf(resource, sizeof(resource), "/ipp/scan/_%s", scanner_name);
    else
      snprintf(resource, sizeof(resource), "/ipp/scan/%s", scanner_name);

    // Convert URL reserved characters to underscore...
    for (resptr = resource + 10; *resptr; resptr ++)
    {
      if ((*resptr & 255) <= ' ' || strchr("\177/\\\'\"?#", *resptr))
	*resptr = '_';
    }

    // Eliminate duplicate and trailing underscores...
    resptr = resource + 10;
    while (*resptr)
    {
      if (resptr[0] == '_' && resptr[1] == '_')
        memmove(resptr, resptr + 1, strlen(resptr));
      else if (resptr[0] == '_' && !resptr[1])
        *resptr = '\0';
      else
        resptr ++;
    }
  }
  else
  {
    cupsCopyString(resource, "/ipp/scan", sizeof(resource));
  }

  // Make sure the scanner doesn't already exist...
  if (_papplSystemFindScanner(system, resource, 0, NULL))
  {
    papplLog(system, PAPPL_LOGLEVEL_ERROR, "Scanner '%s' already exists.", scanner_name);
    errno = EEXIST;
    return (NULL);
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
  cupsRWInit(&scanner->rwlock);

  scanner->system           = system;
  scanner->name             = strdup(scanner_name);
  scanner->dns_sd_name      = strdup(scanner_name);
  scanner->resource         = strdup(resource);
  scanner->resourcelen      = strlen(resource);
  scanner->uriname          = scanner->resource + 9;
					// Skip "/ipp/scan" in resource
  scanner->device_id        = device_id ? strdup(device_id) : NULL;
  scanner->device_uri       = device_uri ? strdup(device_uri) : NULL;
  scanner->driver_name      = strdup(driver_name);
  scanner->attrs            = ippNew();
  scanner->start_time       = time(NULL);
  scanner->config_time      = scanner->start_time;
  scanner->state            = IPP_PSTATE_IDLE;
  scanner->state_reasons    = PAPPL_PREASON_NONE;
  scanner->state_time       = scanner->start_time;
  scanner->uuid             = strdup(uuid);
  scanner->all_jobs         = cupsArrayNew((cups_array_cb_t)compare_all_jobs, NULL, NULL, 0, NULL, (cups_afree_cb_t)_papplJobDelete);
  scanner->active_jobs      = cupsArrayNew((cups_array_cb_t)compare_active_jobs, NULL, NULL, 0, NULL, NULL);
  scanner->completed_jobs   = cupsArrayNew((cups_array_cb_t)compare_completed_jobs, NULL, NULL, 0, NULL, NULL);
  scanner->next_job_id      = 1;
  scanner->max_active_jobs  = 1;
  scanner->max_completed_jobs = 100;

  // Verify all allocations succeeded...
  if (!scanner->name || !scanner->dns_sd_name || !scanner->resource || (device_id && !scanner->device_id) || (device_uri && !scanner->device_uri) || !scanner->driver_name || !scanner->attrs || !scanner->uuid)
  {
    _papplScannerDelete(scanner);
    return (NULL);
  }

  // Initialize driver and driver-specific attributes...
  driver_attrs = NULL;
  _papplScannerInitDriverData(&driver_data);

  if (!(system->sc_driver_cb)(system, driver_name, device_uri, device_id, &driver_data, &driver_attrs, system->sc_driver_cbdata))
  {
    errno = EIO;
    _papplScannerDelete(scanner);
    return (NULL);
  }

  if (!papplScannerSetDriverData(scanner, &driver_data, driver_attrs))
  {
    errno = EINVAL;
    _papplScannerDelete(scanner);
    return (NULL);
  }

  ippDelete(driver_attrs);

  // Add the scanner to the system...
  _papplSystemAddScanner(system, scanner, scanner_id);

  // scanner-id
  _papplRWLockWrite(scanner);
  ippAddInteger(scanner->attrs, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "scanner-id", scanner->scanner_id);
  _papplRWUnlock(scanner);

  // Do any post-creation work...
  if (system->sc_create_cb)
    (system->sc_create_cb)(scanner, system->sc_driver_cbdata);

  // Register web interface...
  {
    char path[1024];			// Resource path

    papplSystemAddResourceCallback(system, scanner->uriname, "text/html", (pappl_resource_cb_t)_papplScannerWebHome, scanner);

    snprintf(path, sizeof(path), "%s/cancelall", scanner->uriname);
    papplSystemAddResourceCallback(system, path, "text/html", (pappl_resource_cb_t)_papplScannerWebCancelAllJobs, scanner);

    if (system->options & PAPPL_SOPTIONS_MULTI_QUEUE)
    {
      snprintf(path, sizeof(path), "%s/delete", scanner->uriname);
      papplSystemAddResourceCallback(system, path, "text/html", (pappl_resource_cb_t)_papplScannerWebDelete, scanner);
    }

    snprintf(path, sizeof(path), "%s/config", scanner->uriname);
    papplSystemAddResourceCallback(system, path, "text/html", (pappl_resource_cb_t)_papplScannerWebConfig, scanner);

    snprintf(path, sizeof(path), "%s/jobs", scanner->uriname);
    papplSystemAddResourceCallback(system, path, "text/html", (pappl_resource_cb_t)_papplScannerWebJobs, scanner);

    snprintf(path, sizeof(path), "%s/defaults", scanner->uriname);
    papplSystemAddResourceCallback(system, path, "text/html", (pappl_resource_cb_t)_papplScannerWebDefaults, scanner);
    papplScannerAddLink(scanner, _PAPPL_LOC("Scanning Defaults"), path, PAPPL_LOPTIONS_NAVIGATION | PAPPL_LOPTIONS_STATUS);
  }

  _papplSystemConfigChanged(system);

  return (scanner);
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


  // Remove the scanner from the system object...
  _papplRWLockWrite(system);
  cupsArrayRemove(system->scanners, scanner);
  _papplRWUnlock(system);

  _papplScannerDelete(scanner);

  _papplSystemConfigChanged(system);
}


//
// 'papplScannerFindJob()' - Find a scan job by ID.
//
// This function searches a scanner's job arrays for a job with the specified
// ID.  Returns `NULL` if the job is not found.
//

pappl_job_t *				// O - Job or `NULL` if not found
papplScannerFindJob(
    pappl_scanner_t *scanner,		// I - Scanner
    int             job_id)		// I - Job ID
{
  size_t	i,			// Looping var
		count;			// Number of jobs
  pappl_job_t	*job;			// Current job


  if (!scanner || job_id < 1)
    return (NULL);

  _papplRWLockRead(scanner);

  for (i = 0, count = cupsArrayGetCount(scanner->all_jobs); i < count; i ++)
  {
    job = (pappl_job_t *)cupsArrayGetElement(scanner->all_jobs, i);
    if (job->job_id == job_id)
    {
      _papplRWUnlock(scanner);
      return (job);
    }
  }

  _papplRWUnlock(scanner);

  return (NULL);
}


//
// '_papplScannerDelete()' - Free memory associated with a scanner.
//

void
_papplScannerDelete(
    pappl_scanner_t *scanner)		// I - Scanner
{
  // Mark as deleted...
  _papplRWLockWrite(scanner);
  scanner->is_deleted = true;
  _papplRWUnlock(scanner);

  // Unregister DNS-SD...
  _papplScannerUnregisterDNSSDNoLock(scanner);

  // If applicable, call the delete function...
  if (scanner->driver_data.delete_cb)
    (scanner->driver_data.delete_cb)(scanner, &scanner->driver_data);

  // Delete jobs...
  cupsArrayDelete(scanner->active_jobs);
  cupsArrayDelete(scanner->completed_jobs);
  cupsArrayDelete(scanner->all_jobs);

  // Free memory...
  free(scanner->name);
  free(scanner->dns_sd_name);
  free(scanner->location);
  free(scanner->geo_location);
  free(scanner->organization);
  free(scanner->org_unit);
  free(scanner->resource);
  free(scanner->device_id);
  free(scanner->device_uri);
  free(scanner->driver_name);
  free(scanner->scan_group);
  free(scanner->uuid);

  ippDelete(scanner->driver_attrs);
  ippDelete(scanner->attrs);

  cupsArrayDelete(scanner->links);

  cupsRWDestroy(&scanner->rwlock);

  free(scanner);
}





//
// '_papplScannerUnregisterDNSSDNoLock()' - Unregister scanner DNS-SD services.
//

void
_papplScannerUnregisterDNSSDNoLock(
    pappl_scanner_t *scanner)		// I - Scanner
{
  if (scanner->dns_sd_services)
  {
    cupsDNSSDServiceDelete(scanner->dns_sd_services);
    scanner->dns_sd_services = NULL;
  }
}


//
// 'compare_active_jobs()' - Compare two active jobs.
//

static int				// O - Result of comparison
compare_active_jobs(pappl_job_t *a,	// I - First job
                    pappl_job_t *b)	// I - Second job
{
  return (b->job_id - a->job_id);
}


//
// 'compare_all_jobs()' - Compare two jobs.
//

static int				// O - Result of comparison
compare_all_jobs(pappl_job_t *a,	// I - First job
                 pappl_job_t *b)	// I - Second job
{
  return (b->job_id - a->job_id);
}


//
// 'compare_completed_jobs()' - Compare two completed jobs.
//

static int				// O - Result of comparison
compare_completed_jobs(pappl_job_t *a,	// I - First job
                       pappl_job_t *b)	// I - Second job
{
  return (b->job_id - a->job_id);
}
