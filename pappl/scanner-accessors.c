//
// Scanner accessor functions for the Printer Application Framework
//
// Copyright © 2024-2026 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#include "pappl-private.h"


//
// 'papplScannerGetContact()' - Get the "scanner-contact" value.
//
// This function copies the current scanner contact information to the buffer
// pointed to by the "contact" argument.
//

pappl_contact_t *			// O - Contact
papplScannerGetContact(
    pappl_scanner_t *scanner,		// I - Scanner
    pappl_contact_t *contact)		// O - Contact
{
  if (!scanner || !contact)
  {
    if (contact)
      memset(contact, 0, sizeof(pappl_contact_t));

    return (contact);
  }

  _papplRWLockRead(scanner);

  *contact = scanner->contact;

  _papplRWUnlock(scanner);

  return (contact);
}


//
// 'papplScannerGetDeviceID()' - Get the IEEE-1284 device ID of the scanner.
//
// This function returns the IEEE-1284 device ID of the scanner.
//

const char *				// O - IEEE-1284 device ID string
papplScannerGetDeviceID(
    pappl_scanner_t *scanner)		// I - Scanner
{
  return (scanner ? scanner->device_id : NULL);
}


//
// 'papplScannerGetDeviceURI()' - Get the URI of the device associated with the
//                                scanner.
//
// This function returns the device URI for the scanner.
//

const char *				// O - Device URI string
papplScannerGetDeviceURI(
    pappl_scanner_t *scanner)		// I - Scanner
{
  return (scanner ? scanner->device_uri : "file:///dev/null");
}


//
// 'papplScannerGetDNSSDName()' - Get the current DNS-SD service name.
//
// This function copies the current DNS-SD service name to the buffer pointed
// to by the "buffer" argument.
//

char *					// O - DNS-SD service name or `NULL` for none
papplScannerGetDNSSDName(
    pappl_scanner_t *scanner,		// I - Scanner
    char            *buffer,		// I - String buffer
    size_t          bufsize)		// I - Size of string buffer
{
  if (!scanner || !scanner->dns_sd_name || !buffer || bufsize == 0)
  {
    if (buffer)
      *buffer = '\0';

    return (NULL);
  }

  _papplRWLockRead(scanner);
  cupsCopyString(buffer, scanner->dns_sd_name, bufsize);
  _papplRWUnlock(scanner);

  return (buffer);
}


//
// 'papplScannerGetDriverAttributes()' - Get the driver IPP attributes for a
//                                       scanner.
//
// This function returns the driver-specific IPP capability attributes for a
// scanner.  The returned IPP attributes object must not be modified or freed.
//

ipp_t *					// O - IPP attributes or `NULL` for none
papplScannerGetDriverAttributes(
    pappl_scanner_t *scanner)		// I - Scanner
{
  return (scanner ? scanner->driver_attrs : NULL);
}


//
// 'papplScannerGetDriverData()' - Get the driver data for a scanner.
//
// This function copies the current scanner driver data to the buffer pointed
// to by the "data" argument.
//

pappl_sc_driver_data_t *		// O - Driver data or `NULL` on error
papplScannerGetDriverData(
    pappl_scanner_t        *scanner,	// I - Scanner
    pappl_sc_driver_data_t *data)	// O - Driver data
{
  if (!scanner || !data)
  {
    if (data)
      memset(data, 0, sizeof(pappl_sc_driver_data_t));

    return (data);
  }

  _papplRWLockRead(scanner);

  *data = scanner->driver_data;

  _papplRWUnlock(scanner);

  return (data);
}


//
// 'papplScannerGetDriverName()' - Get the driver name for a scanner.
//
// This function returns the scanner's driver name.
//

const char *				// O - Driver name
papplScannerGetDriverName(
    pappl_scanner_t *scanner)		// I - Scanner
{
  return (scanner ? scanner->driver_name : NULL);
}


//
// 'papplScannerGetGeoLocation()' - Get the current geo-location as a "geo:"
//                                  URI.
//
// This function copies the currently configured geographic location as a "geo:"
// URI to the buffer pointed to by the "buffer" argument.
//

char *					// O - "geo:" URI or `NULL` for unknown
papplScannerGetGeoLocation(
    pappl_scanner_t *scanner,		// I - Scanner
    char            *buffer,		// I - String buffer
    size_t          bufsize)		// I - Size of string buffer
{
  if (!scanner || !scanner->geo_location || !buffer || bufsize == 0)
  {
    if (buffer)
      *buffer = '\0';

    return (NULL);
  }

  _papplRWLockRead(scanner);
  cupsCopyString(buffer, scanner->geo_location, bufsize);
  _papplRWUnlock(scanner);

  return (buffer);
}


//
// 'papplScannerGetID()' - Get the scanner ID.
//
// This function returns the scanner's unique positive integer identifier.
//

int					// O - "scanner-id" value or `0` for none
papplScannerGetID(
    pappl_scanner_t *scanner)		// I - Scanner
{
  return (scanner ? scanner->scanner_id : 0);
}


//
// 'papplScannerGetImpressionsCompleted()' - Get the number of impressions
//                                           (pages) that have been scanned.
//
// This function returns the number of impressions that have been scanned.  An
// impression is one side of a scanned page.
//

int					// O - Number of scanned impressions/sides
papplScannerGetImpressionsCompleted(
    pappl_scanner_t *scanner)		// I - Scanner
{
  return (scanner ? scanner->impcompleted : 0);
}


//
// 'papplScannerGetLocation()' - Get the location string.
//
// This function copies the scanner's human-readable location to the buffer
// pointed to by the "buffer" argument.
//

char *					// O - Location or `NULL` for none
papplScannerGetLocation(
    pappl_scanner_t *scanner,		// I - Scanner
    char            *buffer,		// I - String buffer
    size_t          bufsize)		// I - Size of string buffer
{
  if (!scanner || !scanner->location || !buffer || bufsize == 0)
  {
    if (buffer)
      *buffer = '\0';

    return (NULL);
  }

  _papplRWLockRead(scanner);
  cupsCopyString(buffer, scanner->location, bufsize);
  _papplRWUnlock(scanner);

  return (buffer);
}


//
// 'papplScannerGetMaxActiveJobs()' - Get the maximum number of active (queued)
//                                    jobs allowed by the scanner.
//
// This function returns the maximum number of active jobs that the scanner
// supports, as configured by the @link papplScannerSetMaxActiveJobs@ function.
//

size_t					// O - Maximum number of active jobs, `0` for unlimited
papplScannerGetMaxActiveJobs(
    pappl_scanner_t *scanner)		// I - Scanner
{
  return (scanner ? scanner->max_active_jobs : 0);
}


//
// 'papplScannerGetMaxCompletedJobs()' - Get the maximum number of jobs retained
//                                       for history by the scanner.
//
// This function returns the maximum number of jobs that are retained in the
// job history as configured by the @link papplScannerSetMaxCompletedJobs@
// function.
//

size_t					// O - Maximum number of completed jobs, `0` for unlimited
papplScannerGetMaxCompletedJobs(
    pappl_scanner_t *scanner)		// I - Scanner
{
  return (scanner ? scanner->max_completed_jobs : 0);
}


//
// 'papplScannerGetName()' - Get the scanner name.
//
// This function returns the scanner's human-readable name.
//

const char *				// O - Scanner name
papplScannerGetName(
    pappl_scanner_t *scanner)		// I - Scanner
{
  return (scanner ? scanner->name : NULL);
}


//
// 'papplScannerGetNextJobID()' - Get the next job ID.
//
// This function returns the positive integer identifier that will be used for
// the next job that is created.
//

int					// O - Next job ID or `0` for none
papplScannerGetNextJobID(
    pappl_scanner_t *scanner)		// I - Scanner
{
  return (scanner ? scanner->next_job_id : 0);
}


//
// 'papplScannerGetNumberOfActiveJobs()' - Get the number of active scan jobs.
//
// This function returns the number of scan jobs that are either scanning or
// waiting to be scanned.
//

size_t					// O - Number of active scan jobs
papplScannerGetNumberOfActiveJobs(
    pappl_scanner_t *scanner)		// I - Scanner
{
  return (scanner ? cupsArrayGetCount(scanner->active_jobs) : 0);
}


//
// 'papplScannerGetNumberOfCompletedJobs()' - Get the number of completed scan
//                                            jobs.
//
// This function returns the number of scan jobs that have been aborted,
// canceled, or completed.
//

size_t					// O - Number of completed scan jobs
papplScannerGetNumberOfCompletedJobs(
    pappl_scanner_t *scanner)		// I - Scanner
{
  return (scanner ? cupsArrayGetCount(scanner->completed_jobs) : 0);
}


//
// 'papplScannerGetNumberOfJobs()' - Get the total number of scan jobs.
//
// This function returns the number of scan jobs that are scanning, waiting
// to be scanned, have been aborted, have been canceled, or have completed.
//

size_t					// O - Total number of scan jobs
papplScannerGetNumberOfJobs(
    pappl_scanner_t *scanner)		// I - Scanner
{
  return (scanner ? cupsArrayGetCount(scanner->all_jobs) : 0);
}


//
// 'papplScannerGetOrganization()' - Get the organization name.
//
// This function copies the scanner's organization name to the buffer pointed
// to by the "buffer" argument.
//

char *					// O - Organization name or `NULL` for none
papplScannerGetOrganization(
    pappl_scanner_t *scanner,		// I - Scanner
    char            *buffer,		// I - String buffer
    size_t          bufsize)		// I - Size of string buffer
{
  if (!scanner || !scanner->organization || !buffer || bufsize == 0)
  {
    if (buffer)
      *buffer = '\0';

    return (NULL);
  }

  _papplRWLockRead(scanner);
  cupsCopyString(buffer, scanner->organization, bufsize);
  _papplRWUnlock(scanner);

  return (buffer);
}


//
// 'papplScannerGetOrganizationalUnit()' - Get the organizational unit name.
//
// This function copies the scanner's organizational unit name to the buffer
// pointed to by the "buffer" argument.
//

char *					// O - Organizational unit name or `NULL` for none
papplScannerGetOrganizationalUnit(
    pappl_scanner_t *scanner,		// I - Scanner
    char            *buffer,		// I - String buffer
    size_t          bufsize)		// I - Size of string buffer
{
  if (!scanner || !scanner->org_unit || !buffer || bufsize == 0)
  {
    if (buffer)
      *buffer = '\0';

    return (NULL);
  }

  _papplRWLockRead(scanner);
  cupsCopyString(buffer, scanner->org_unit, bufsize);
  _papplRWUnlock(scanner);

  return (buffer);
}


//
// 'papplScannerGetPath()' - Get the URL path for a scanner web page.
//
// This function generates and returns the URL path for the scanner's web page.
// The "subpath" argument specifies an optional sub-path for a specific scanner
// web page.
//

char *					// O - URI path or `NULL` on error
papplScannerGetPath(
    pappl_scanner_t *scanner,		// I - Scanner
    const char      *subpath,		// I - Sub-path or `NULL` for none
    char            *buffer,		// I - String buffer
    size_t          bufsize)		// I - Size of string buffer
{
  if (!scanner || !buffer || bufsize < 32)
  {
    if (buffer)
      *buffer = '\0';

    return (NULL);
  }

  if (subpath)
    snprintf(buffer, bufsize, "%s/%s", scanner->uriname, subpath);
  else
    cupsCopyString(buffer, scanner->uriname, bufsize);

  return (buffer);
}


//
// 'papplScannerGetReasons()' - Get the current "scanner-state-reasons" bit
//                              values.
//
// This function returns the current scanner state reasons bitfield, which can
// be updated by the scanner driver and/or by the @link papplScannerSetReasons@
// function.
//

pappl_preason_t				// O - "scanner-state-reasons" bit values
papplScannerGetReasons(
    pappl_scanner_t *scanner)		// I - Scanner
{
  pappl_preason_t ret = PAPPL_PREASON_NONE;
					// Return value


  if (scanner)
  {
    _papplRWLockRead(scanner);

    if (!scanner->device_in_use && !scanner->processing_job && (time(NULL) - scanner->status_time) > 1 && scanner->driver_data.status_cb)
    {
      // Update scanner status...
      _papplRWUnlock(scanner);
      (scanner->driver_data.status_cb)(scanner);

      _papplRWLockRead(scanner);
      scanner->status_time = time(NULL);
    }

    ret = scanner->state_reasons;

    _papplRWUnlock(scanner);
  }

  return (ret);
}


//
// 'papplScannerGetScanGroup()' - Get the scan authorization group, if any.
//
// This function copies the scanner's authorization group name to the buffer
// pointed to by the "buffer" argument.
//

char *					// O - Scan authorization group name or `NULL` for none
papplScannerGetScanGroup(
    pappl_scanner_t *scanner,		// I - Scanner
    char            *buffer,		// I - String buffer
    size_t          bufsize)		// I - Size of string buffer
{
  if (!scanner || !scanner->scan_group || !buffer || bufsize == 0)
  {
    if (buffer)
      *buffer = '\0';

    return (NULL);
  }

  _papplRWLockRead(scanner);
  cupsCopyString(buffer, scanner->scan_group, bufsize);
  _papplRWUnlock(scanner);

  return (buffer);
}


//
// 'papplScannerGetState()' - Get the current "scanner-state" value.
//
// This function returns the current scanner state as an enumeration:
//
// - `IPP_PSTATE_IDLE`: The scanner is idle and has no jobs to process.
// - `IPP_PSTATE_PROCESSING`: The scanner is processing a job and/or producing
//   output.
// - `IPP_PSTATE_STOPPED`: The scanner is stopped for maintenance.
//

ipp_pstate_t				// O - "scanner-state" value
papplScannerGetState(
    pappl_scanner_t *scanner)		// I - Scanner
{
  return (scanner ? scanner->state : IPP_PSTATE_STOPPED);
}


//
// 'papplScannerGetSystem()' - Get the system associated with the scanner.
//
// This function returns a pointer to the system object that contains the
// scanner.
//

pappl_system_t *			// O - System
papplScannerGetSystem(
    pappl_scanner_t *scanner)		// I - Scanner
{
  return (scanner ? scanner->system : NULL);
}


//
// 'papplScannerIsDeleted()' - Return whether a scanner is in the process of
//                             being deleted.
//
// This function returns a boolean value indicating whether a scanner is being
// deleted.
//

bool					// O - `true` if scanner is being deleted, `false` otherwise
papplScannerIsDeleted(
    pappl_scanner_t *scanner)		// I - Scanner
{
  bool	is_deleted;			// Return value


  // Range check input...
  if (!scanner)
    return (false);

  // Lock and grab value...
  _papplRWLockRead(scanner);
  is_deleted = scanner->is_deleted;
  _papplRWUnlock(scanner);

  return (is_deleted);
}


//
// 'papplScannerIterateActiveJobs()' - Iterate over the active jobs.
//
// This function iterates over jobs that are either scanning or waiting to be
// scanned.  The specified callback "cb" will be called once per job with the
// data pointer "data".
//
// The "job_index" argument specifies the first job in the list to iterate,
// where `1` is the first job, etc.  The "limit" argument specifies the maximum
// number of jobs to iterate - use `0` to iterate an unlimited number of jobs.
//

void
papplScannerIterateActiveJobs(
    pappl_scanner_t *scanner,		// I - Scanner
    pappl_job_cb_t  cb,			// I - Callback function
    void            *data,		// I - Callback data
    size_t          job_index,		// I - First job to iterate (1-based)
    size_t          limit)		// I - Maximum jobs to iterate or `0` for no limit
{
  pappl_job_t	*job;			// Current job
  size_t	j,			// Looping var
		jcount,			// Number of jobs
		count;			// Number of jobs iterated


  if (!scanner || !cb)
    return;

  _papplRWLockRead(scanner);

  // Note: Cannot use cupsArrayGetFirst/Last since other threads might be
  // enumerating the active_jobs array.

  if (limit == 0)
    limit = INT_MAX;

  for (count = 0, j = (size_t)job_index - 1, jcount = cupsArrayGetCount(scanner->active_jobs); j < jcount && count < limit; j ++, count ++)
  {
    job = (pappl_job_t *)cupsArrayGetElement(scanner->active_jobs, j);

    (cb)(job, data);
  }

  _papplRWUnlock(scanner);
}


//
// 'papplScannerIterateAllJobs()' - Iterate over all the jobs.
//
// This function iterates over all jobs.  The specified callback "cb" will be
// called once per job with the data pointer "data".
//
// The "job_index" argument specifies the first job in the list to iterate,
// where `1` is the first job, etc.  The "limit" argument specifies the maximum
// number of jobs to iterate - use `0` to iterate an unlimited number of jobs.
//

void
papplScannerIterateAllJobs(
    pappl_scanner_t *scanner,		// I - Scanner
    pappl_job_cb_t  cb,			// I - Callback function
    void            *data,		// I - Callback data
    size_t          job_index,		// I - First job to iterate (1-based)
    size_t          limit)		// I - Maximum jobs to iterate, `0` for no limit
{
  pappl_job_t	*job;			// Current job
  size_t	j,			// Looping var
		jcount,			// Number of jobs
		count;			// Number of jobs iterated


  if (!scanner || !cb)
    return;

  _papplRWLockRead(scanner);

  // Note: Cannot use cupsArrayGetFirst/Last since other threads might be
  // enumerating the all_jobs array.

  if (limit == 0)
    limit = INT_MAX;

  for (count = 0, j = (size_t)job_index - 1, jcount = cupsArrayGetCount(scanner->all_jobs); j < jcount && count < limit; j ++, count ++)
  {
    job = (pappl_job_t *)cupsArrayGetElement(scanner->all_jobs, j);

    (cb)(job, data);
  }

  _papplRWUnlock(scanner);
}


//
// 'papplScannerIterateCompletedJobs()' - Iterate over the completed jobs.
//
// This function iterates over jobs that are aborted, canceled, or completed.
// The specified callback "cb" will be called once per job with the data pointer
// "data".
//
// The "job_index" argument specifies the first job in the list to iterate,
// where `1` is the first job, etc.  The "limit" argument specifies the maximum
// number of jobs to iterate - use `0` to iterate an unlimited number of jobs.
//

void
papplScannerIterateCompletedJobs(
    pappl_scanner_t *scanner,		// I - Scanner
    pappl_job_cb_t  cb,			// I - Callback function
    void            *data,		// I - Callback data
    size_t          job_index,		// I - First job to iterate (1-based)
    size_t          limit)		// I - Maximum jobs to iterate, `0` for no limit
{
  pappl_job_t	*job;			// Current job
  size_t	j,			// Looping var
		jcount,			// Number of jobs
		count;			// Number of jobs iterated


  if (!scanner || !cb)
    return;

  _papplRWLockRead(scanner);

  // Note: Cannot use cupsArrayGetFirst/Last since other threads might be
  // enumerating the completed_jobs array.

  if (limit == 0)
    limit = INT_MAX;

  for (count = 0, j = (size_t)job_index - 1, jcount = cupsArrayGetCount(scanner->completed_jobs); j < jcount && count < limit; j ++, count ++)
  {
    job = (pappl_job_t *)cupsArrayGetElement(scanner->completed_jobs, j);

    (cb)(job, data);
  }

  _papplRWUnlock(scanner);
}


//
// 'papplScannerOpenDevice()' - Open the device associated with a scanner.
//
// This function opens the scanner's device.  `NULL` is returned if the device
// is already in use, for example while a job is being scanned.
//
// The returned device must be closed using the @link papplScannerCloseDevice@
// function.
//

pappl_device_t *			// O - Device or `NULL` if not possible
papplScannerOpenDevice(
    pappl_scanner_t *scanner)		// I - Scanner
{
  pappl_device_t	*device = NULL;	// Open device


  if (!scanner)
    return (NULL);

  _papplRWLockWrite(scanner);

  if (!scanner->device_in_use && !scanner->processing_job && scanner->device_uri)
  {
    scanner->device        = device = papplDeviceOpen(scanner->device_uri, NULL, papplLogDevice, scanner->system);
    scanner->device_in_use = device != NULL;
  }

  _papplRWUnlock(scanner);

  return (device);
}


//
// 'papplScannerCloseDevice()' - Close the device associated with the scanner.
//
// This function closes the device for a scanner.  The device must have been
// previously opened using the @link papplScannerOpenDevice@ function.
//

void
papplScannerCloseDevice(
    pappl_scanner_t *scanner)		// I - Scanner
{
  if (!scanner)
    return;

  _papplRWLockWrite(scanner);
  if (scanner->device && scanner->device_in_use)
  {
    scanner->device_in_use = false;

    if (scanner->state != IPP_PSTATE_PROCESSING)
    {
      papplDeviceClose(scanner->device);
      scanner->device = NULL;
    }
  }

  _papplRWUnlock(scanner);
}


//
// 'papplScannerSetContact()' - Set the "scanner-contact" value.
//
// This function sets the scanner's contact information.
//

void
papplScannerSetContact(
    pappl_scanner_t *scanner,		// I - Scanner
    pappl_contact_t *contact)		// I - Contact
{
  if (!scanner || !contact)
    return;

  _papplRWLockWrite(scanner);

  scanner->contact     = *contact;
  scanner->config_time = time(NULL);

  _papplRWUnlock(scanner);

  _papplSystemConfigChanged(scanner->system);
}


//
// 'papplScannerSetDNSSDName()' - Set the DNS-SD service name.
//
// This function sets the scanner's DNS-SD service name.  If `NULL`, the scanner
// will stop advertising the scanner via DNS-SD.
//

void
papplScannerSetDNSSDName(
    pappl_scanner_t *scanner,		// I - Scanner
    const char      *value)		// I - DNS-SD service name or `NULL` for none
{
  if (!scanner)
    return;

  _papplRWLockWrite(scanner);

  free(scanner->dns_sd_name);
  scanner->dns_sd_name      = value ? strdup(value) : NULL;
  scanner->dns_sd_collision = false;
  scanner->dns_sd_serial    = 0;
  scanner->config_time      = time(NULL);

  if (!value)
    _papplScannerUnregisterDNSSDNoLock(scanner);
  else
    _papplScannerRegisterDNSSDNoLock(scanner);

  _papplRWUnlock(scanner);

  _papplSystemConfigChanged(scanner->system);
}


//
// 'papplScannerSetGeoLocation()' - Set the geo-location value as a "geo:" URI.
//
// This function sets the scanner's geographic location as a "geo:" URI.  If
// `NULL`, the location is cleared to the 'unknown' value.
//

void
papplScannerSetGeoLocation(
    pappl_scanner_t *scanner,		// I - Scanner
    const char      *value)		// I - "geo:" URI or `NULL` for unknown
{
  float	lat, lon;			// Latitude and longitude from geo: URI


  if (!scanner)
    return;

  // Validate geo-location - must be NULL or a "geo:" URI...
  if (value && *value && sscanf(value, "geo:%f,%f", &lat, &lon) != 2)
    return;

  _papplRWLockWrite(scanner);

  free(scanner->geo_location);
  scanner->geo_location = value && *value ? strdup(value) : NULL;
  scanner->config_time  = time(NULL);

  _papplScannerRegisterDNSSDNoLock(scanner);

  _papplRWUnlock(scanner);

  _papplSystemConfigChanged(scanner->system);
}


//
// 'papplScannerSetImpressionsCompleted()' - Add impressions (pages) to the
//                                           total count of scanned impressions.
//
// This function adds to the scanner's impressions counter.  An impression is
// one side of a scanned page.
//

void
papplScannerSetImpressionsCompleted(
    pappl_scanner_t *scanner,		// I - Scanner
    int             add)		// I - Number of impressions/sides to add
{
  if (!scanner || add <= 0)
    return;

  _papplRWLockWrite(scanner);

  scanner->impcompleted += add;
  scanner->state_time   = time(NULL);

  _papplRWUnlock(scanner);
}


//
// 'papplScannerSetLocation()' - Set the location string.
//
// This function sets the scanner's human-readable location string.  If `NULL`,
// the location is cleared.
//

void
papplScannerSetLocation(
    pappl_scanner_t *scanner,		// I - Scanner
    const char      *value)		// I - Location ("Bob's Office", etc.) or `NULL` for none
{
  if (!scanner)
    return;

  _papplRWLockWrite(scanner);

  free(scanner->location);
  scanner->location    = value ? strdup(value) : NULL;
  scanner->config_time = time(NULL);

  _papplScannerRegisterDNSSDNoLock(scanner);

  _papplRWUnlock(scanner);

  _papplSystemConfigChanged(scanner->system);
}


//
// 'papplScannerSetMaxActiveJobs()' - Set the maximum number of active jobs for
//                                    the scanner.
//
// This function sets the maximum number of jobs that can be queued on the
// scanner at one time.
//

void
papplScannerSetMaxActiveJobs(
    pappl_scanner_t *scanner,		// I - Scanner
    size_t          max_active_jobs)	// I - Maximum number of active jobs, `0` for unlimited
{
  if (!scanner)
    return;

  _papplRWLockWrite(scanner);

  scanner->max_active_jobs = max_active_jobs;
  scanner->config_time     = time(NULL);

  _papplRWUnlock(scanner);

  _papplSystemConfigChanged(scanner->system);
}


//
// 'papplScannerSetMaxCompletedJobs()' - Set the maximum number of completed
//                                       jobs for the scanner.
//
// This function sets the maximum number of aborted, canceled, or completed jobs
// that are retained in the job history.
//

void
papplScannerSetMaxCompletedJobs(
    pappl_scanner_t *scanner,		// I - Scanner
    size_t          max_completed_jobs)	// I - Maximum number of completed jobs, `0` for unlimited
{
  if (!scanner)
    return;

  _papplRWLockWrite(scanner);

  scanner->max_completed_jobs = max_completed_jobs;
  scanner->config_time        = time(NULL);

  _papplRWUnlock(scanner);

  _papplSystemConfigChanged(scanner->system);
}


//
// 'papplScannerSetNextJobID()' - Set the next "job-id" value.
//
// This function sets the next unique positive integer identifier that will be
// used for a job.
//
// > Note: This function is normally only called once to restore the previous
// > state of the scanner.
//

void
papplScannerSetNextJobID(
    pappl_scanner_t *scanner,		// I - Scanner
    int             next_job_id)	// I - Next "job-id" value
{
  if (!scanner || next_job_id < 1)
    return;

  _papplRWLockWrite(scanner);

  scanner->next_job_id = next_job_id;
  scanner->config_time = time(NULL);

  _papplRWUnlock(scanner);

  _papplSystemConfigChanged(scanner->system);
}


//
// 'papplScannerSetOrganization()' - Set the organization name.
//
// This function sets the scanner's organization name.  If `NULL` the value is
// cleared.
//

void
papplScannerSetOrganization(
    pappl_scanner_t *scanner,		// I - Scanner
    const char      *value)		// I - Organization name or `NULL` for none
{
  if (!scanner)
    return;

  _papplRWLockWrite(scanner);

  free(scanner->organization);
  scanner->organization = value ? strdup(value) : NULL;
  scanner->config_time  = time(NULL);

  _papplRWUnlock(scanner);

  _papplSystemConfigChanged(scanner->system);
}


//
// 'papplScannerSetOrganizationalUnit()' - Set the organizational unit name.
//
// This function sets the scanner's organizational unit name.  If `NULL` the
// value is cleared.
//

void
papplScannerSetOrganizationalUnit(
    pappl_scanner_t *scanner,		// I - Scanner
    const char      *value)		// I - Organizational unit name or `NULL` for none
{
  if (!scanner)
    return;

  _papplRWLockWrite(scanner);

  free(scanner->org_unit);
  scanner->org_unit    = value ? strdup(value) : NULL;
  scanner->config_time = time(NULL);

  _papplRWUnlock(scanner);

  _papplSystemConfigChanged(scanner->system);
}


//
// 'papplScannerSetReasons()' - Add or remove values from
//                              "scanner-state-reasons".
//
// This function updates the scanner state reasons bitfield by clearing any bit
// values in the "remove" argument and setting any bit values in the "add"
// argument.
//

void
papplScannerSetReasons(
    pappl_scanner_t *scanner,		// I - Scanner
    pappl_preason_t add,		// I - "scanner-state-reasons" bit values to add or `PAPPL_PREASON_NONE` for none
    pappl_preason_t remove)		// I - "scanner-state-reasons" bit values to remove or `PAPPL_PREASON_NONE` for none
{
  if (!scanner)
    return;

  _papplRWLockWrite(scanner);

  scanner->state_reasons &= ~remove;
  scanner->state_reasons |= add;
  scanner->state_time    = scanner->status_time = time(NULL);

  _papplRWUnlock(scanner);
}


//
// 'papplScannerSetScanGroup()' - Set the scan authorization group, if any.
//
// This function sets the scanner's authorization group.  If `NULL`, the group
// is cleared.
//
// > Note: The authorization group is only used if the system is created with a
// > named authorization service.
//

void
papplScannerSetScanGroup(
    pappl_scanner_t *scanner,		// I - Scanner
    const char      *value)		// I - Scan authorization group or `NULL` for none
{
  if (!scanner)
    return;

  _papplRWLockWrite(scanner);

  free(scanner->scan_group);
  scanner->scan_group = value ? strdup(value) : NULL;
  scanner->config_time = time(NULL);

#if !_WIN32
  if (scanner->scan_group && strcmp(scanner->scan_group, "none"))
  {
    char		buffer[8192];	// Buffer for strings
    struct group	grpbuf,		// Group buffer
			*grp = NULL;	// Scan group

    if (getgrnam_r(scanner->scan_group, &grpbuf, buffer, sizeof(buffer), &grp) || !grp)
      papplLog(scanner->system, PAPPL_LOGLEVEL_ERROR, "Scanner '%s': Unable to find scan group '%s'.", scanner->name, scanner->scan_group);
    else
      scanner->scan_gid = grp->gr_gid;
  }
  else
#endif // !_WIN32
    scanner->scan_gid = (gid_t)-1;

  _papplRWUnlock(scanner);

  _papplSystemConfigChanged(scanner->system);
}


//
// 'papplPrinterGetScanner()' - Get the associated scanner for an MFP printer.
//
// This function returns a pointer to the scanner associated with the printer,
// if any.  If the printer is not part of a multi-function device or has no
// associated scanner, `NULL` is returned.
//

pappl_scanner_t *			// O - Scanner or `NULL`
papplPrinterGetScanner(
    pappl_printer_t *printer)		// I - Printer
{
  pappl_scanner_t	*scanner = NULL;// Return value


  if (!printer)
    return (NULL);

  _papplRWLockRead(printer);
  scanner = printer->scanner;
  _papplRWUnlock(printer);

  return (scanner);
}


//
// 'papplPrinterSetScanner()' - Associate a scanner with a printer for MFP
//                              support.
//
// This function associates a scanner with a printer, indicating that both are
// part of the same multi-function physical device.  For eSCL MFP devices, the
// scanner and printer will share the same UUID.
//
// Pass `NULL` for the "scanner" argument to clear any existing association.
//

void
papplPrinterSetScanner(
    pappl_printer_t *printer,		// I - Printer
    pappl_scanner_t *scanner)		// I - Scanner or `NULL`
{
  if (!printer)
    return;

  _papplRWLockWrite(printer);
  printer->scanner = scanner;
  _papplRWUnlock(printer);

  _papplSystemConfigChanged(printer->system);
}
