//
// Printer accessor functions for the Printer Application Framework
//
// Copyright © 2020-2024 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#include "printer-private.h"
#include "job-private.h"
#include "system-private.h"


//
// 'papplPrinterCloseDevice()' - Close the device associated with the printer.
//
// This function closes the device for a printer.  The device must have been
// previously opened using the @link papplPrinterOpenDevice@ function.
//

void
papplPrinterCloseDevice(
    pappl_printer_t *printer)		// I - Printer
{
  if (!printer)
    return;

  _papplRWLockWrite(printer);
  if (printer->device && printer->device_in_use)
  {
    printer->device_in_use = false;

    if (cupsArrayGetCount(printer->active_jobs) > 0 && !printer->processing_job)
      _papplPrinterCheckJobsNoLock(printer);

    if (printer->state != IPP_PSTATE_PROCESSING)
    {
//      papplLogPrinter(printer, PAPPL_LOGLEVEL_DEBUG, "Closing device.");

      papplDeviceClose(printer->device);
      printer->device = NULL;
    }
  }

  _papplRWUnlock(printer);
}


//
// 'papplPrinterDisable()' - Stop accepting jobs on a printer.
//
// This function stops accepting jobs on a printer.
//

void
papplPrinterDisable(
    pappl_printer_t *printer)		// I - Printer
{
  if (printer)
  {
    printer->is_accepting = false;
    papplSystemAddEvent(printer->system, printer, NULL, PAPPL_EVENT_PRINTER_STATE_CHANGED, NULL);
  }
}


//
// 'papplPrinterEnable()' - Start accepting jobs on a printer.
//
// This function starts accepting jobs on a printer.
//

void
papplPrinterEnable(
    pappl_printer_t *printer)		// I - Printer
{
  if (printer)
  {
    printer->is_accepting = true;
    papplSystemAddEvent(printer->system, printer, NULL, PAPPL_EVENT_PRINTER_STATE_CHANGED, NULL);
  }
}


//
// 'papplPrinterGetContact()' - Get the "printer-contact" value.
//
// This function copies the current printer contact information to the buffer
// pointed to by the "contact" argument.
//

pappl_contact_t *			// O - Contact
papplPrinterGetContact(
    pappl_printer_t *printer,		// I - Printer
    pappl_contact_t *contact)		// O - Contact
{
  if (!printer || !contact)
  {
    if (contact)
      memset(contact, 0, sizeof(pappl_contact_t));

    return (contact);
  }

  _papplRWLockRead(printer);

  *contact = printer->contact;

  _papplRWUnlock(printer);

  return (contact);
}


//
// 'papplPrinterGetDeviceID()' - Get the IEEE-1284 device ID of the printer.
//
// This function returns the IEEE-1284 device ID of the printer.
//

const char *				// O - IEEE-1284 device ID string
papplPrinterGetDeviceID(
    pappl_printer_t *printer)		// I - Printer
{
  return (printer ? printer->device_id : NULL);
}


//
// 'papplPrinterGetDeviceURI()' - Get the URI of the device associated with the
//                                printer.
//
// This function returns the device URI for the printer.
//

const char *				// O - Device URI string
papplPrinterGetDeviceURI(
    pappl_printer_t *printer)		// I - Printer
{
  return (printer ? printer->device_uri : "file:///dev/null");
}


//
// 'papplPrinterGetDNSSDName()' - Get the current DNS-SD service name.
//
// This function copies the current DNS-SD service name to the buffer pointed
// to by the "buffer" argument.
//

char *					// O - DNS-SD service name or `NULL` for none
papplPrinterGetDNSSDName(
    pappl_printer_t *printer,		// I - Printer
    char            *buffer,		// I - String buffer
    size_t          bufsize)		// I - Size of string buffer
{
  if (!printer || !printer->dns_sd_name || !buffer || bufsize == 0)
  {
    if (buffer)
      *buffer = '\0';

    return (NULL);
  }

  _papplRWLockRead(printer);
  papplCopyString(buffer, printer->dns_sd_name, bufsize);
  _papplRWUnlock(printer);

  return (buffer);
}


//
// 'papplPrinterGetGeoLocation()' - Get the current geo-location as a "geo:"
//                                  URI.
//
// This function copies the currently configured geographic location as a "geo:"
// URI to the buffer pointed to by the "buffer" argument.
//

char *					// O - "geo:" URI or `NULL` for unknown
papplPrinterGetGeoLocation(
    pappl_printer_t *printer,		// I - Printer
    char            *buffer,		// I - String buffer
    size_t          bufsize)		// I - Size of string buffer
{
  if (!printer || !printer->geo_location || !buffer || bufsize == 0)
  {
    if (buffer)
      *buffer = '\0';

    return (NULL);
  }

  _papplRWLockRead(printer);
  papplCopyString(buffer, printer->geo_location, bufsize);
  _papplRWUnlock(printer);

  return (buffer);
}


//
// 'papplPrinterGetID()' - Get the printer ID.
//
// This function returns the printer's unique positive integer identifier.
//

int					// O - "printer-id" value or `0` for none
papplPrinterGetID(
    pappl_printer_t *printer)		// I - Printer
{
  return (printer ? printer->printer_id : 0);
}


//
// 'papplPrinterGetImpressionsCompleted()' - Get the number of impressions
//                                           (sides) that have been printed.
//
// This function returns the number of impressions that have been printed.  An
// impression is one side of an output page.
//

int					// O - Number of printed impressions/sides
papplPrinterGetImpressionsCompleted(
    pappl_printer_t *printer)		// I - Printer
{
  return (printer ? printer->impcompleted : 0);
}


//
// 'papplPrinterGetLocation()' - Get the location string.
//
// This function copies the printer's human-readable location to the buffer
// pointed to by the "buffer" argument.
//

char *					// O - Location or `NULL` for none
papplPrinterGetLocation(
    pappl_printer_t *printer,		// I - Printer
    char            *buffer,		// I - String buffer
    size_t          bufsize)		// I - Size of string buffer
{
  if (!printer || !printer->location || !buffer || bufsize == 0)
  {
    if (buffer)
      *buffer = '\0';

    return (NULL);
  }

  _papplRWLockRead(printer);
  papplCopyString(buffer, printer->location, bufsize);
  _papplRWUnlock(printer);

  return (buffer);
}


//
// 'papplPrinterGetMaxActiveJobs()' - Get the maximum number of active (queued)
//                                    jobs allowed by the printer.
//
// This function returns the maximum number of active jobs that the printer
// supports, as configured by the @link papplPrinterSetMaxActiveJobs@ function.
//

int					// O - Maximum number of active jobs, `0` for unlimited
papplPrinterGetMaxActiveJobs(
    pappl_printer_t *printer)		// I - Printer
{
  return (printer ? printer->max_active_jobs : 0);
}


//
// 'papplPrinterGetMaxCompletedJobs()' - Get the maximum number of jobs retained
//                                       for history by the printer.
//
// This function returns the maximum number of jobs that are retained in the
// job history as configured by the @link papplPrinterSetMaxCompletedJobs@
// function.
//

int					// O - Maximum number of completed jobs, `0` for unlimited
papplPrinterGetMaxCompletedJobs(
    pappl_printer_t *printer)		// I - Printer
{
  return (printer ? printer->max_completed_jobs : 0);
}


//
// 'papplPrinterGetMaxPreservedJobs()' - Get the maximum number of jobs
//                                       preserved by the printer.
//
// This function returns the maximum number of jobs that are retained (including
// document data) in the job history as configured by the
// @link papplPrinterSetMaxPreservedJobs@ function.
//

int					// O - Maximum number of preserved jobs, `0` for none
papplPrinterGetMaxPreservedJobs(
    pappl_printer_t *printer)		// I - Printer
{
  return (printer ? printer->max_preserved_jobs : 0);
}


//
// 'papplPrinterGetName()' - Get the printer name.
//
// This function returns the printer's human-readable name.
//


const char *				// O - Printer name
papplPrinterGetName(
    pappl_printer_t *printer)		// I - Printer
{
  return (printer ? printer->name : NULL);
}


//
// 'papplPrinterGetNextJobID()' - Get the next job ID.
//
// This function returns the positive integer identifier that will be used for
// the next job that is created.
//

int					// O - Next job ID or `0` for none
papplPrinterGetNextJobID(
    pappl_printer_t *printer)		// I - Printer
{
  return (printer ? printer->next_job_id : 0);
}


//
// 'papplPrinterGetNumberOfActiveJobs()' - Get the number of active print jobs.
//
// This function returns the number of print jobs that are either printing or
// waiting to be printed.
//

int					// O - Number of active print jobs
papplPrinterGetNumberOfActiveJobs(
    pappl_printer_t *printer)		// I - Printer
{
  return (printer ? (int)cupsArrayGetCount(printer->active_jobs) : 0);
}


//
// 'papplPrinterGetNumberOfCompletedJobs()' - Get the number of completed print
//                                            jobs.
//
// This function returns the number of print jobs that have been aborted,
// canceled, or completed.
//

int					// O - Number of completed print jobs
papplPrinterGetNumberOfCompletedJobs(
    pappl_printer_t *printer)		// I - Printer
{
  return (printer ? (int)cupsArrayGetCount(printer->completed_jobs) : 0);
}


//
// 'papplPrinterGetNumberOfJobs()' - Get the total number of print jobs.
//
// This function returns the number of print jobs that are printing, waiting
// to be printed, have been aborted, have been canceled, or have completed.
//

int					// O - Total number of print jobs
papplPrinterGetNumberOfJobs(
    pappl_printer_t *printer)		// I - Printer
{
  return (printer ? (int)cupsArrayGetCount(printer->all_jobs) : 0);
}


//
// 'papplPrinterGetOrganization()' - Get the organization name.
//
// This function copies the printer's organization name to the buffer pointed
// to by the "buffer" argument.
//

char *					// O - Organization name or `NULL` for none
papplPrinterGetOrganization(
    pappl_printer_t *printer,		// I - Printer
    char            *buffer,		// I - String buffer
    size_t          bufsize)		// I - Size of string buffer
{
  if (!printer || !printer->organization || !buffer || bufsize == 0)
  {
    if (buffer)
      *buffer = '\0';

    return (NULL);
  }

  _papplRWLockRead(printer);
  papplCopyString(buffer, printer->organization, bufsize);
  _papplRWUnlock(printer);

  return (buffer);
}


//
// 'papplPrinterGetOrganizationalUnit()' - Get the organizational unit name.
//
// This function copies the printer's organizational unit name to the buffer
// pointed to by the "buffer" argument.
//

char *					// O - Organizational unit name or `NULL` for none
papplPrinterGetOrganizationalUnit(
    pappl_printer_t *printer,		// I - Printer
    char            *buffer,		// I - String buffer
    size_t          bufsize)		// I - Size of string buffer
{
  if (!printer || !printer->org_unit || !buffer || bufsize == 0)
  {
    if (buffer)
      *buffer = '\0';

    return (NULL);
  }

  _papplRWLockRead(printer);
  papplCopyString(buffer, printer->org_unit, bufsize);
  _papplRWUnlock(printer);

  return (buffer);
}


//
// 'papplPrinterGetPath()' - Get the URL path for a printer web page.
//
// This function generates and returns the URL path for the printer's web page.
// The "subpath" argument specifies an optional sub-path for a specific printer
// web page.
//

char *					// O - URI path or `NULL` on error
papplPrinterGetPath(
    pappl_printer_t *printer,		// I - Printer
    const char      *subpath,		// I - Sub-path or `NULL` for none
    char            *buffer,		// I - String buffer
    size_t          bufsize)		// I - Size of string buffer
{
  if (!printer || !buffer || bufsize < 32)
  {
    if (buffer)
      *buffer = '\0';

    return (NULL);
  }

  if (subpath)
    snprintf(buffer, bufsize, "%s/%s", printer->uriname, subpath);
  else
    papplCopyString(buffer, printer->uriname, bufsize);

  return (buffer);
}


//
// 'papplPrinterGetPrintGroup()' - Get the print authorization group, if any.
//
// This function copies the printer's authorization group name to the buffer
// pointed to by the "buffer" argument.
//

char *					// O - Print authorization group name or `NULL` for none
papplPrinterGetPrintGroup(
    pappl_printer_t *printer,		// I - Printer
    char            *buffer,		// I - String buffer
    size_t          bufsize)		// I - Size of string buffer
{
  if (!printer || !printer->print_group || !buffer || bufsize == 0)
  {
    if (buffer)
      *buffer = '\0';

    return (NULL);
  }

  _papplRWLockRead(printer);
  papplCopyString(buffer, printer->print_group, bufsize);
  _papplRWUnlock(printer);

  return (buffer);
}


//
// 'papplPrinterGetReasons()' - Get the current "printer-state-reasons" bit values.
//
// This function returns the current printer state reasons bitfield, which can
// be updated by the printer driver and/or by the @link papplPrinterSetReasons@
// function.
//

pappl_preason_t				// O - "printer-state-reasons" bit values
papplPrinterGetReasons(
    pappl_printer_t *printer)		// I - Printer
{
  pappl_preason_t ret = PAPPL_PREASON_NONE;
					// Return value


  if (printer)
  {
    _papplRWLockRead(printer);

    if (!printer->device_in_use && !printer->processing_job && (time(NULL) - printer->status_time) > 1 && printer->driver_data.status_cb)
    {
      // Update printer status...
      _papplRWUnlock(printer);
      (printer->driver_data.status_cb)(printer);

      _papplRWLockRead(printer);
      printer->status_time = time(NULL);
    }

    ret = printer->state_reasons;

    _papplRWUnlock(printer);
  }

  return (ret);
}


//
// 'papplPrinterGetState()' - Get the current "printer-state" value.
//
// This function returns the current printer state as an enumeration:
//
// - `IPP_PSTATE_IDLE`: The printer is idle and has no jobs to process.
// - `IPP_PSTATE_PROCESSING`: The printer is processing a job and/or producing
//   output.
// - `IPP_PSTATE_STOPPED`: The printer is stopped for maintenance.
//

ipp_pstate_t				// O - "printer-state" value
papplPrinterGetState(
    pappl_printer_t *printer)		// I - Printer
{
  return (printer ? printer->state : IPP_PSTATE_STOPPED);
}


//
// 'papplPrinterGetSupplies()' - Get the current "printer-supplies" values.
//
// This function copies the current printer supply values to the specified
// array.  The "max_supplies" and "supplies" arguments can be `0` and `NULL`
// to query the number of supplies used.
//
// The return value is the actual number of supplies used by the printer,
// regardless of the size of the array.
//

int					// O - Number of values
papplPrinterGetSupplies(
    pappl_printer_t *printer,		// I - Printer
    int             max_supplies,	// I - Maximum number of supplies
    pappl_supply_t  *supplies)		// I - Array for supplies
{
  int	count;				// Number of supplies


  if (!printer || max_supplies < 0 || (max_supplies > 0 && !supplies))
    return (0);

  if (max_supplies == 0)
    return (printer->num_supply);

  memset(supplies, 0, (size_t)max_supplies * sizeof(pappl_supply_t));

  _papplRWLockRead(printer);

  if ((count = printer->num_supply) > max_supplies)
    count = max_supplies;

  memcpy(supplies, printer->supply, (size_t)count * sizeof(pappl_supply_t));

  _papplRWUnlock(printer);

  return (count);
}


//
// 'papplPrinterGetSystem()' - Get the system associated with the printer.
//
// This function returns a pointer to the system object that contains the
// printer.
//

pappl_system_t *			// O - System
papplPrinterGetSystem(
    pappl_printer_t *printer)		// I - Printer
{
  return (printer ? printer->system : NULL);
}


//
// 'papplPrinterHoldNewJobs()' - Hold new jobs for printing.
//
// This function holds any new jobs for printing and is typically used prior to
// performing printer maintenance.  Existing jobs will finish printing but new
// jobs will be held until you call @link papplPrinterReleaseHeldNewJobs@.
//

bool					// O - `true` on success, `false` on failure
papplPrinterHoldNewJobs(
    pappl_printer_t *printer)		// I - Printer
{
  bool	ret = false;			// Return value


  // Range check input...
  if (!printer)
    return (false);

  // See if we need to change things...
  _papplRWLockWrite(printer);

  if (!printer->hold_new_jobs)
  {
    // Set the 'hold-new-jobs' flag...
    printer->hold_new_jobs = true;
    printer->config_time   = time(NULL);
    ret                    = true;

    // Notify of the change in state...
    _papplSystemAddEventNoLock(printer->system, printer, NULL, NULL, PAPPL_EVENT_PRINTER_CONFIG_CHANGED, "Holding new jobs.");
  }

  _papplRWUnlock(printer);

  if (ret)
    _papplSystemConfigChanged(printer->system);

  return (ret);
}


//
// 'papplPrinterIsAcceptingJobs()' - Return whether the printer is accepting jobs.
//
// This function returns a boolean value indicating whether a printer is
// accepting jobs.
//

bool					// O - `true` if the printer is accepting jobs, `false` otherwise
papplPrinterIsAcceptingJobs(
    pappl_printer_t *printer)		// I - Printer
{
  bool	is_accepting;			// Return value


  // Range check input...
  if (!printer)
    return (false);

  // Lock and grab value...
  _papplRWLockRead(printer);
  is_accepting = printer->is_accepting;
  _papplRWUnlock(printer);

  return (is_accepting);
}


//
// 'papplPrinterIsDeleted()' - Return whether a printer is in the process of being deleted.
//
// This function returns a boolean value indicating whether a printer is being
// deleted.
//

bool					// O - `true` is printer is being deleted, `false` otherwise
papplPrinterIsDeleted(
    pappl_printer_t *printer)		// I - Printer
{
  bool	is_deleted;			// Return value


  // Range check input...
  if (!printer)
    return (false);

  // Lock and grab value...
  _papplRWLockRead(printer);
  is_deleted = printer->is_deleted;
  _papplRWUnlock(printer);

  return (is_deleted);
}


//
// 'papplPrinterIsHoldingNewJobs()' - Return whether the printer is holding new jobs.
//
// This function returns a boolean value indicating whether a printer is
// holding new jobs.
//

bool					// O - `true` if the printer is holding new jobs, `false` otherwise
papplPrinterIsHoldingNewJobs(
    pappl_printer_t *printer)		// I - Printer
{
  bool	hold_new_jobs;			// Return value


  // Range check input...
  if (!printer)
    return (false);

  // Lock and grab value...
  _papplRWLockRead(printer);
  hold_new_jobs = printer->hold_new_jobs;
  _papplRWUnlock(printer);

  return (hold_new_jobs);
}


//
// 'papplPrinterIterateActiveJobs()' - Iterate over the active jobs.
//
// This function iterates over jobs that are either printing or waiting to be
// printed.  The specified callback "cb" will be called once per job with the
// data pointer "data".
//
// The "job_index" argument specifies the first job in the list to iterate,
// where `1` is the first job, etc.  The "limit" argument specifies the maximum
// number of jobs to iterate - use `0` to iterate an unlimited number of jobs.
//

void
papplPrinterIterateActiveJobs(
    pappl_printer_t *printer,		// I - Printer
    pappl_job_cb_t  cb,			// I - Callback function
    void            *data,		// I - Callback data
    int             job_index,		// I - First job to iterate (1-based)
    int             limit)		// I - Maximum jobs to iterate or `0` for no limit
{
  pappl_job_t	*job;			// Current job
  cups_len_t	j,			// Looping var
		jcount;			// Number of jobs
  int		count;			// Number of jobs iterated


  if (!printer || !cb)
    return;

  _papplRWLockRead(printer);

  // Note: Cannot use cupsArrayGetFirst/Last since other threads might be
  // enumerating the active_jobs array.

  if (limit <= 0)
    limit = INT_MAX;

  for (count = 0, j = (cups_len_t)job_index - 1, jcount = cupsArrayGetCount(printer->active_jobs); j < jcount && count < limit; j ++, count ++)
  {
    job = (pappl_job_t *)cupsArrayGetElement(printer->active_jobs, j);

    (cb)(job, data);
  }

  _papplRWUnlock(printer);
}


//
// 'papplPrinterIterateAllJobs()' - Iterate over all the jobs.
//
// This function iterates over all jobs.  The specified callback "cb" will be
// called once per job with the data pointer "data".
//
// The "job_index" argument specifies the first job in the list to iterate,
// where `1` is the first job, etc.  The "limit" argument specifies the maximum
// number of jobs to iterate - use `0` to iterate an unlimited number of jobs.
//

void
papplPrinterIterateAllJobs(
    pappl_printer_t *printer,		// I - Printer
    pappl_job_cb_t  cb,			// I - Callback function
    void            *data,		// I - Callback data
    int             job_index,		// I - First job to iterate (1-based)
    int             limit)		// I - Maximum jobs to iterate, `0` for no limit
{
  pappl_job_t	*job;			// Current job
  cups_len_t	j,			// Looping var
		jcount;			// Number of jobs
  int		count;			// Number of jobs iterated


  if (!printer || !cb)
    return;

  _papplRWLockRead(printer);

  // Note: Cannot use cupsArrayGetFirst/Last since other threads might be
  // enumerating the all_jobs array.

  if (limit <= 0)
    limit = INT_MAX;

  for (count = 0, j = (cups_len_t)job_index - 1, jcount = cupsArrayGetCount(printer->all_jobs); j < jcount && count < limit; j ++, count ++)
  {
    job = (pappl_job_t *)cupsArrayGetElement(printer->all_jobs, j);

    (cb)(job, data);
  }

  _papplRWUnlock(printer);
}


//
// 'papplPrinterIterateCompletedJobs()' - Iterate over the completed jobs.
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
papplPrinterIterateCompletedJobs(
    pappl_printer_t *printer,		// I - Printer
    pappl_job_cb_t  cb,			// I - Callback function
    void            *data,		// I - Callback data
    int             job_index,		// I - First job to iterate (1-based)
    int             limit)		// I - Maximum jobs to iterate, `0` for no limit
{
  pappl_job_t	*job;			// Current job
  cups_len_t	j,			// Looping var
		jcount;			// Number of jobs
  int		count;			// Number of jobs iterated


  if (!printer || !cb)
    return;

  _papplRWLockRead(printer);

  // Note: Cannot use cupsArrayGetFirst/Last since other threads might be
  // enumerating the completed_jobs array.

  if (limit <= 0)
    limit = INT_MAX;

  for (count = 0, j = (cups_len_t)job_index - 1, jcount = cupsArrayGetCount(printer->completed_jobs); j < jcount && count < limit; j ++, count ++)
  {
    job = (pappl_job_t *)cupsArrayGetElement(printer->completed_jobs, j);

    (cb)(job, data);
  }

  _papplRWUnlock(printer);
}


//
// 'papplPrinterOpenDevice()' - Open the device associated with a printer.
//
// This function opens the printer's device.  `NULL` is returned if the device
// is already in use, for example while a job is being printed.
//
// The returned device must be closed using the @link papplPrinterCloseDevice@
// function.
//

pappl_device_t *			// O - Device or `NULL` if not possible
papplPrinterOpenDevice(
    pappl_printer_t *printer)		// I - Printer
{
  pappl_device_t	*device = NULL;	// Open device


  if (!printer)
    return (NULL);

  _papplRWLockWrite(printer);

  if (!printer->device_in_use && !printer->processing_job && printer->device_uri)
  {
//    papplLogPrinter(printer, PAPPL_LOGLEVEL_DEBUG, "Opening device for status/maintenance.");

    printer->device        = device = papplDeviceOpen(printer->device_uri, "printer", papplLogDevice, printer->system);
    printer->device_in_use = device != NULL;
  }

  _papplRWUnlock(printer);

  return (device);
}


//
// 'papplPrinterPause()' - Pause (stop) a printer.
//
// This function pauses a printer.  If the printer is currently processing
// (printing) a job, it will be completed before the printer is stopped.
//

void
papplPrinterPause(
    pappl_printer_t *printer)		// I - Printer
{
  if (!printer)
    return;

  _papplRWLockWrite(printer);

  if (printer->processing_job)
    printer->is_stopped = true;
  else
    printer->state = IPP_PSTATE_STOPPED;

  _papplSystemAddEventNoLock(printer->system, printer, NULL, NULL, PAPPL_EVENT_PRINTER_STATE_CHANGED | PAPPL_EVENT_PRINTER_STOPPED, NULL);

  _papplRWUnlock(printer);
}


//
// 'papplPrinterReleaseHeldNewJobs()' - Release any previously held jobs for printing.
//
// This function releases all jobs that were previously held due to a prior
// call to @link papplPrinterHoldNewJobs@.
//

bool					// O - `true` on success, `false` on failure
papplPrinterReleaseHeldNewJobs(
    pappl_printer_t *printer,		// I - Printer
    const char      *username)		// I - User that released the held jobs or `NULL` for none/system
{
  pappl_job_t	*job;			// Current job
  bool		ret = false,		// Return value
		released_jobs = false;	// Have we released any jobs?


  // Range check input...
  if (!printer)
    return (false);

  // Only release if the printer is holding new jobs...
  _papplRWLockWrite(printer);

  if (printer->hold_new_jobs)
  {
    // Release jobs and clear the 'hold-new-jobs' flag...
    printer->hold_new_jobs = false;
    printer->config_time   = time(NULL);
    ret                    = true;

    _papplSystemAddEventNoLock(printer->system, printer, NULL, NULL, PAPPL_EVENT_PRINTER_CONFIG_CHANGED, "Releasing held new jobs.");

    for (job = (pappl_job_t *)cupsArrayGetFirst(printer->active_jobs); job; job = (pappl_job_t *)cupsArrayGetNext(printer->active_jobs))
    {
      if (job->state == IPP_JSTATE_HELD && job->hold_until == 0 && !(job->state_reasons & PAPPL_JREASON_JOB_HOLD_UNTIL_SPECIFIED))
      {
	// Release this job
	_papplRWLockWrite(job);
	_papplJobReleaseNoLock(job, username);
	_papplRWUnlock(job);

	released_jobs = true;
      }
    }
  }

  if (released_jobs)
    _papplPrinterCheckJobsNoLock(printer);

  _papplRWUnlock(printer);

  return (ret);
}


//
// 'papplPrinterResume()' - Resume (start) a printer.
//
// This function resumes a printer and starts processing any pending jobs.
//

void
papplPrinterResume(
    pappl_printer_t *printer)		// I - Printer
{
  if (!printer)
    return;

  _papplRWLockWrite(printer);

  printer->is_stopped = false;
  printer->state      = IPP_PSTATE_IDLE;

  _papplSystemAddEventNoLock(printer->system, printer, NULL, NULL, PAPPL_EVENT_PRINTER_STATE_CHANGED, "Resumed printer.");

  _papplPrinterCheckJobsNoLock(printer);

  _papplRWUnlock(printer);
}


//
// 'papplPrinterSetContact()' - Set the "printer-contact" value.
//
// This function sets the printer's contact information.
//

void
papplPrinterSetContact(
    pappl_printer_t *printer,		// I - Printer
    pappl_contact_t *contact)		// I - Contact
{
  if (!printer || !contact)
    return;

  _papplRWLockWrite(printer);

  printer->contact     = *contact;
  printer->config_time = time(NULL);

  _papplRWUnlock(printer);

  _papplSystemConfigChanged(printer->system);
}


//
// 'papplPrinterSetDNSSDName()' - Set the DNS-SD service name.
//
// This function sets the printer's DNS-SD service name.  If `NULL`, the printer
// will stop advertising the printer.
//

void
papplPrinterSetDNSSDName(
    pappl_printer_t *printer,		// I - Printer
    const char      *value)		// I - DNS-SD service name or `NULL` for none
{
  if (!printer)
    return;

  _papplRWLockWrite(printer);

  free(printer->dns_sd_name);
  printer->dns_sd_name      = value ? strdup(value) : NULL;
  printer->dns_sd_collision = false;
  printer->dns_sd_serial    = 0;
  printer->config_time      = time(NULL);

  if (!value)
    _papplPrinterUnregisterDNSSDNoLock(printer);
  else
    _papplPrinterRegisterDNSSDNoLock(printer);

  _papplRWUnlock(printer);

  _papplSystemConfigChanged(printer->system);
}


//
// 'papplPrinterSetGeoLocation()' - Set the geo-location value as a "geo:" URI.
//
// This function sets the printer's geographic location as a "geo:" URI.  If
// `NULL`, the location is cleared to the 'unknown' value.
//

void
papplPrinterSetGeoLocation(
    pappl_printer_t *printer,		// I - Printer
    const char      *value)		// I - "geo:" URI or `NULL` for unknown
{
  float	lat, lon;			// Latitude and longitude from geo: URI


  if (!printer)
    return;

  // Validate geo-location - must be NULL or a "geo:" URI...
  if (value && *value && sscanf(value, "geo:%f,%f", &lat, &lon) != 2)
    return;

  _papplRWLockWrite(printer);

  free(printer->geo_location);
  printer->geo_location = value && *value ? strdup(value) : NULL;
  printer->config_time  = time(NULL);

  _papplPrinterRegisterDNSSDNoLock(printer);

  _papplRWUnlock(printer);

  _papplSystemConfigChanged(printer->system);
}


//
// 'papplPrinterSetImpressionsCompleted()' - Add impressions (sides) to the
//                                           total count of printed impressions.
//
// This function adds to the printer's impressions counter.  An impression is
// one side of an output page.
//

void
papplPrinterSetImpressionsCompleted(
    pappl_printer_t *printer,		// I - Printer
    int             add)		// I - Number of impressions/sides to add
{
  if (!printer || add <= 0)
    return;

  _papplRWLockWrite(printer);

  printer->impcompleted += add;
  printer->state_time   = time(NULL);

  _papplRWUnlock(printer);

  _papplSystemConfigChanged(printer->system);
}


//
// 'papplPrinterSetLocation()' - Set the location string.
//
// This function sets the printer's human-readable location string.  If `NULL`,
// the location is cleared.
//

void
papplPrinterSetLocation(
    pappl_printer_t *printer,		// I - Printer
    const char      *value)		// I - Location ("Bob's Office", etc.) or `NULL` for none
{
  if (!printer)
    return;

  _papplRWLockWrite(printer);

  free(printer->location);
  printer->location    = value ? strdup(value) : NULL;
  printer->config_time = time(NULL);

  _papplPrinterRegisterDNSSDNoLock(printer);

  _papplRWUnlock(printer);

  _papplSystemConfigChanged(printer->system);
}


//
// 'papplPrinterSetMaxActiveJobs()' - Set the maximum number of active jobs for
//                                    the printer.
//
// This function sets the maximum number of jobs that can be spooled on the
// printer at one time.
//
// > Note: This limit does not apply to streaming raster formats such as PWG
// > Raster since they are not spooled.
//

void
papplPrinterSetMaxActiveJobs(
    pappl_printer_t *printer,		// I - Printer
    int             max_active_jobs)	// I - Maximum number of active jobs, `0` for unlimited
{
  if (!printer || max_active_jobs < 0)
    return;

  _papplRWLockWrite(printer);

  printer->max_active_jobs = max_active_jobs;
  printer->config_time     = time(NULL);

  _papplRWUnlock(printer);

  _papplSystemConfigChanged(printer->system);
}


//
// 'papplPrinterSetMaxCompletedJobs()' - Set the maximum number of completed
//                                       jobs for the printer.
//
// This function sets the maximum number of aborted, canceled, or completed jobs
// that are retained in the job history.
//

void
papplPrinterSetMaxCompletedJobs(
    pappl_printer_t *printer,		// I - Printer
    int             max_completed_jobs)	// I - Maximum number of completed jobs, `0` for unlimited
{
  if (!printer || max_completed_jobs < 0)
    return;

  _papplRWLockWrite(printer);

  printer->max_completed_jobs = max_completed_jobs;
  printer->config_time        = time(NULL);

  _papplRWUnlock(printer);

  _papplSystemConfigChanged(printer->system);
}


//
// 'papplPrinterSetMaxPreservedJobs()' - Set the maximum number of preserved
//                                       jobs for the printer.
//
// This function sets the maximum number of aborted, canceled, or completed jobs
// that are preserved (with document data) in the job history.
//

void
papplPrinterSetMaxPreservedJobs(
    pappl_printer_t *printer,		// I - Printer
    int             max_preserved_jobs)	// I - Maximum number of preserved jobs, `0` for none
{
  if (!printer || max_preserved_jobs < 0)
    return;

  _papplRWLockWrite(printer);

  printer->max_preserved_jobs = max_preserved_jobs;
  printer->config_time        = time(NULL);

  _papplRWUnlock(printer);

  _papplSystemConfigChanged(printer->system);
}


//
// 'papplPrinterSetNextJobID()' - Set the next "job-id" value.
//
// This function sets the next unique positive integer identifier that will be
// used for a job.
//
// > Note: This function is normally only called once to restore the previous
// > state of the printer.
//

void
papplPrinterSetNextJobID(
    pappl_printer_t *printer,		// I - Printer
    int             next_job_id)	// I - Next "job-id" value
{
  if (!printer || next_job_id < 1)
    return;

  _papplRWLockWrite(printer);

  printer->next_job_id = next_job_id;
  printer->config_time = time(NULL);

  _papplRWUnlock(printer);

  _papplSystemConfigChanged(printer->system);
}


//
// 'papplPrinterSetOrganization()' - Set the organization name.
//
// This function sets the printer's organization name.  If `NULL` the value is
// cleared.
//

void
papplPrinterSetOrganization(
    pappl_printer_t *printer,		// I - Printer
    const char      *value)		// I - Organization name or `NULL` for none
{
  if (!printer)
    return;

  _papplRWLockWrite(printer);

  free(printer->organization);
  printer->organization = value ? strdup(value) : NULL;
  printer->config_time  = time(NULL);

  _papplRWUnlock(printer);

  _papplSystemConfigChanged(printer->system);
}


//
// 'papplPrinterSetOrganizationalUnit()' - Set the organizational unit name.
//
// This function sets the printer's organizational unit name.  If `NULL` the
// value is cleared.
//

void
papplPrinterSetOrganizationalUnit(
    pappl_printer_t *printer,		// I - Printer
    const char      *value)		// I - Organizational unit name or `NULL` for none
{
  if (!printer)
    return;

  _papplRWLockWrite(printer);

  free(printer->org_unit);
  printer->org_unit    = value ? strdup(value) : NULL;
  printer->config_time = time(NULL);

  _papplRWUnlock(printer);

  _papplSystemConfigChanged(printer->system);
}


//
// 'papplPrinterSetPrintGroup()' - Set the print authorization group, if any.
//
// This function sets the printer's authorization group.  If `NULL`, the group
// is cleared.
//
// > Note: The authorization group is only used if the system is created with a
// > named authorization service.
//

void
papplPrinterSetPrintGroup(
    pappl_printer_t *printer,		// I - Printer
    const char      *value)		// I - Print authorization group or `NULL` for none
{
  if (!printer)
    return;

  _papplRWLockWrite(printer);

  free(printer->print_group);
  printer->print_group = value ? strdup(value) : NULL;
  printer->config_time = time(NULL);

#if !_WIN32
  if (printer->print_group && strcmp(printer->print_group, "none"))
  {
    char		buffer[8192];	// Buffer for strings
    struct group	grpbuf,		// Group buffer
			*grp = NULL;	// Print group

    if (getgrnam_r(printer->print_group, &grpbuf, buffer, sizeof(buffer), &grp) || !grp)
      papplLogPrinter(printer, PAPPL_LOGLEVEL_ERROR, "Unable to find print group '%s'.", printer->print_group);
    else
      printer->print_gid = grp->gr_gid;
  }
  else
#endif // !_WIN32
    printer->print_gid = (gid_t)-1;

  _papplRWUnlock(printer);

  _papplSystemConfigChanged(printer->system);
}


//
// 'papplPrinterSetReasons()' - Add or remove values from
//                              "printer-state-reasons".
//
// This function updates the printer state reasons bitfield by clearing any bit
// values in the "remove" argument and setting any bit values in the "add"
// argument.
//

void
papplPrinterSetReasons(
    pappl_printer_t *printer,		// I - Printer
    pappl_preason_t add,		// I - "printer-state-reasons" bit values to add or `PAPPL_PREASON_NONE` for none
    pappl_preason_t remove)		// I - "printer-state-reasons" bit values to remove or `PAPPL_PREASON_NONE` for none
{
  if (!printer)
    return;

  _papplRWLockWrite(printer);

  printer->state_reasons &= ~remove;
  printer->state_reasons |= add;
  printer->state_time    = printer->status_time = time(NULL);

  _papplRWUnlock(printer);
}


//
// 'papplPrinterSetSupplies()' - Set/update the supplies for a printer.
//
// This function updates the supply information for the printer.
//

void
papplPrinterSetSupplies(
    pappl_printer_t *printer,		// I - Printer
    int             num_supplies,	// I - Number of supplies
    pappl_supply_t  *supplies)		// I - Array of supplies
{
  if (!printer || num_supplies < 0 || num_supplies > PAPPL_MAX_SUPPLY || (num_supplies > 0 && !supplies))
    return;

  _papplRWLockWrite(printer);

  printer->num_supply = num_supplies;
  memset(printer->supply, 0, sizeof(printer->supply));
  if (supplies)
    memcpy(printer->supply, supplies, (size_t)num_supplies * sizeof(pappl_supply_t));
  printer->state_time = time(NULL);

  _papplRWUnlock(printer);
}
