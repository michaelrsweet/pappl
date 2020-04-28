//
// Printer accessor functions for the Printer Application Framework
//
// Copyright © 2020 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

//
// Include necessary headers...
//

#include "printer-private.h"
#include "system-private.h"


//
// 'papplPrinterCloseDevice()' - Close the device associated with the printer.
//

void
papplPrinterCloseDevice(
    pappl_printer_t *printer)		// I - Printer
{
  if (!printer || !printer->device || !printer->device_in_use || printer->processing_job)
    return;

  pthread_rwlock_wrlock(&printer->rwlock);

  papplDeviceClose(printer->device);

  printer->device        = NULL;
  printer->device_in_use = false;

  pthread_rwlock_unlock(&printer->rwlock);
}


//
// 'papplPrinterGetActiveJobs()' - Get the number of active (pending/processing) jobs.
//

int					// O - Number of jobs
papplPrinterGetActiveJobs(
    pappl_printer_t *printer)		// I - Printer
{
  int	num_jobs = 0;			// Number of jobs


  if (printer)
  {
    pthread_rwlock_rdlock(&printer->rwlock);
    num_jobs = cupsArrayCount(printer->active_jobs);
    pthread_rwlock_unlock(&printer->rwlock);
  }

  return (num_jobs);
}


//
// 'papplPrinterGetContact()' - Get the "printer-contact" value.
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

  pthread_rwlock_rdlock(&printer->rwlock);

  *contact = printer->contact;

  pthread_rwlock_unlock(&printer->rwlock);

  return (contact);
}


//
// 'papplPrinterGetDNSSDName()' - Get the current DNS-SD service name.
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

  pthread_rwlock_rdlock(&printer->rwlock);
  strlcpy(buffer, printer->dns_sd_name, bufsize);
  pthread_rwlock_unlock(&printer->rwlock);

  return (buffer);
}


//
// 'papplPrinterGetGeoLocation()' - Get the current geo-location as a "geo:" URI.
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

  pthread_rwlock_rdlock(&printer->rwlock);
  strlcpy(buffer, printer->geo_location, bufsize);
  pthread_rwlock_unlock(&printer->rwlock);

  return (buffer);
}


//
// 'papplPrinterGetID()' - Get the printer ID.
//

int					// O - "printer-id" value or `0` for none
papplPrinterGetID(
    pappl_printer_t *printer)		// I - Printer
{
  return (printer ? printer->printer_id : 0);
}


//
// 'papplPrinterGetImpressionsCompleted()' - Get the number of impressions (sides) that have been printed.
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

  pthread_rwlock_rdlock(&printer->rwlock);
  strlcpy(buffer, printer->location, bufsize);
  pthread_rwlock_unlock(&printer->rwlock);

  return (buffer);
}


//
// 'papplPrinterGetMaxActiveJobs()' - Get the maximum number of active (queued) jobs allowed by the printer.
//

int					// O - Maximum number of active jobs
papplPrinterGetMaxActiveJobs(
    pappl_printer_t *printer)		// I - Printer
{
  return (printer ? printer->max_active_jobs : 0);
}


//
// 'papplPrinterGetMaxCompletedJobs()' - Get the maximum number of jobs retained for history by the printer.
//

int					// O - Maximum number of completed jobs
papplPrinterGetMaxCompletedJobs(
    pappl_printer_t *printer)		// I - Printer
{
  return (printer ? printer->max_completed_jobs : 0);
}


//
// 'papplPrinterGetName()' - Get the printer name.
//

const char *				// O - Printer name or `NULL` for none
papplPrinterGetName(
    pappl_printer_t *printer)		// I - Printer
{
  return (printer ? printer->name : NULL);
}


//
// 'papplPrinterGetNextJobId()' - Get the next job ID.
//

int					// O - Next job ID or `0` for none
papplPrinterGetNextJobId(
    pappl_printer_t *printer)		// I - Printer
{
  return (printer ? printer->next_job_id : 0);
}


//
// 'papplPrinterGetNumberOfActiveJobs()' - Get the number of active print jobs.
//

int					// O - Number of active print jobs
papplPrinterGetNumberOfActiveJobs(
    pappl_printer_t *printer)		// I - Printer
{
  return (printer ? cupsArrayCount(printer->active_jobs) : 0);
}


//
// 'papplPrinterGetNumberOfCompletedJobs()' - Get the number of completed print jobs.
//

int					// O - Number of completed print jobs
papplPrinterGetNumberOfCompletedJobs(
    pappl_printer_t *printer)		// I - Printer
{
  return (printer ? cupsArrayCount(printer->completed_jobs) : 0);
}


//
// 'papplPrinterGetNumberOfJobs()' - Get the total number of print jobs.
//

int					// O - Total number of print jobs
papplPrinterGetNumberOfJobs(
    pappl_printer_t *printer)		// I - Printer
{
  return (printer ? cupsArrayCount(printer->all_jobs) : 0);
}


//
// 'papplPrinterGetOrganization()' - Get the organization name.
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

  pthread_rwlock_rdlock(&printer->rwlock);
  strlcpy(buffer, printer->organization, bufsize);
  pthread_rwlock_unlock(&printer->rwlock);

  return (buffer);
}


//
// 'papplPrinterGetOrganizationalUnit()' - Get the organizational unit name.
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

  pthread_rwlock_rdlock(&printer->rwlock);
  strlcpy(buffer, printer->org_unit, bufsize);
  pthread_rwlock_unlock(&printer->rwlock);

  return (buffer);
}


//
// 'papplPrinterGetPrintGroup()' - Get the print authorization group, if any.
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

  pthread_rwlock_rdlock(&printer->rwlock);
  strlcpy(buffer, printer->print_group, bufsize);
  pthread_rwlock_unlock(&printer->rwlock);

  return (buffer);
}


//
// 'papplPrinterGetReasons()' - Get the current "printer-state-reasons" bit values.
//

pappl_preason_t				// O - "printer-state-reasons" bit values
papplPrinterGetReasons(
    pappl_printer_t *printer)		// I - Printer
{
  if (!printer)
    return (PAPPL_PREASON_NONE);

  if (!printer->device_in_use && !printer->processing_job && (time(NULL) - printer->status_time) > 1 && printer->driver_data.status)
  {
    // Update printer status...
    (printer->driver_data.status)(printer);
    printer->status_time = time(NULL);
  }

  return (printer->state_reasons);
}


//
// 'papplPrinterGetState()' - Get the current "printer-state" value.
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

  pthread_rwlock_rdlock(&printer->rwlock);

  if ((count = printer->num_supply) > max_supplies)
    count = max_supplies;

  memcpy(supplies, printer->supply, (size_t)count * sizeof(pappl_supply_t));

  pthread_rwlock_unlock(&printer->rwlock);

  return (count);
}


//
// 'papplPrinterGetSystem()' - Get the system associated with the printer.
//

pappl_system_t *			// O - System
papplPrinterGetSystem(
    pappl_printer_t *printer)		// I - Printer
{
  return (printer ? printer->system : NULL);
}


//
// 'papplPrinterIterateActiveJobs()' - Iterate over the active jobs.
//

void
papplPrinterIterateActiveJobs(
    pappl_printer_t *printer,		// I - Printer
    pappl_job_cb_t  cb,			// I - Callback function
    void            *data)		// I - Callback data
{
  pappl_job_t	*job;			// Current job


  if (!printer || !cb)
    return;

  pthread_rwlock_rdlock(&printer->rwlock);

  for (job = (pappl_job_t *)cupsArrayFirst(printer->active_jobs); job; job = (pappl_job_t *)cupsArrayNext(printer->active_jobs))
    (cb)(job, data);

  pthread_rwlock_unlock(&printer->rwlock);
}


//
// 'papplPrinterIterateAllJobs()' - Iterate over all the jobs.
//

void
papplPrinterIterateAllJobs(
    pappl_printer_t *printer,		// I - Printer
    pappl_job_cb_t  cb,			// I - Callback function
    void            *data)		// I - Callback data
{
  pappl_job_t	*job;			// Current job


  if (!printer || !cb)
    return;

  pthread_rwlock_rdlock(&printer->rwlock);

  for (job = (pappl_job_t *)cupsArrayFirst(printer->all_jobs); job; job = (pappl_job_t *)cupsArrayNext(printer->all_jobs))
    (cb)(job, data);

  pthread_rwlock_unlock(&printer->rwlock);
}


//
// 'papplPrinterIterateCompletedJobs()' - Iterate over the completed jobs.
//

void
papplPrinterIterateCompletedJobs(
    pappl_printer_t *printer,		// I - Printer
    pappl_job_cb_t  cb,			// I - Callback function
    void            *data)		// I - Callback data
{
  pappl_job_t	*job;			// Current job


  if (!printer || !cb)
    return;

  pthread_rwlock_rdlock(&printer->rwlock);

  for (job = (pappl_job_t *)cupsArrayFirst(printer->completed_jobs); job; job = (pappl_job_t *)cupsArrayNext(printer->completed_jobs))
    (cb)(job, data);

  pthread_rwlock_unlock(&printer->rwlock);
}


//
// 'papplPrinterOpenDevice()' - Open the device associated with a printer.
//

pappl_device_t *			// O - Device or `NULL` if not possible
papplPrinterOpenDevice(
    pappl_printer_t *printer)		// I - Printer
{
  pappl_device_t	*device = NULL;	// Open device


  if (!printer || printer->device_in_use || printer->processing_job || !printer->device_uri)
    return (NULL);

  pthread_rwlock_wrlock(&printer->rwlock);

  if (!printer->device_in_use && !printer->processing_job)
  {
    printer->device        = device = papplDeviceOpen(printer->device_uri, papplLogDevice, printer->system);
    printer->device_in_use = device != NULL;
  }

  pthread_rwlock_wrlock(&printer->rwlock);

  return (device);
}


//
// 'papplPrinterSetContact()' - Set the "printer-contact" value.
//

void
papplPrinterSetContact(
    pappl_printer_t *printer,		// I - Printer
    pappl_contact_t *contact)		// I - Contact
{
  if (!printer || !contact)
    return;

  pthread_rwlock_wrlock(&printer->rwlock);

  printer->contact     = *contact;
  printer->config_time = time(NULL);

  pthread_rwlock_unlock(&printer->rwlock);

  _papplSystemConfigChanged(printer->system);
}


//
// 'papplPrinterSetDNSSDName()' - Set the DNS-SD service name.
//

void
papplPrinterSetDNSSDName(
    pappl_printer_t *printer,		// I - Printer
    const char      *value)		// I - DNS-SD service name or `NULL` for none
{
  if (!printer)
    return;

  pthread_rwlock_wrlock(&printer->rwlock);

  free(printer->dns_sd_name);
  printer->dns_sd_name      = value ? strdup(value) : NULL;
  printer->dns_sd_collision = false;
  printer->config_time      = time(NULL);

  if (!value)
    _papplPrinterUnregisterDNSSDNoLock(printer);
  else
    _papplPrinterRegisterDNSSDNoLock(printer);

  pthread_rwlock_unlock(&printer->rwlock);

  _papplSystemConfigChanged(printer->system);
}


//
// 'papplPrinterSetGeoLocation()' - Set the geo-location value as a "geo:" URI.
//

void
papplPrinterSetGeoLocation(
    pappl_printer_t *printer,		// I - Printer
    const char      *value)		// I - "geo:" URI or `NULL` for unknown
{
  if (!printer)
    return;

  pthread_rwlock_wrlock(&printer->rwlock);

  free(printer->geo_location);
  printer->geo_location = value ? strdup(value) : NULL;
  printer->config_time  = time(NULL);

// TODO: Uncomment once LOC records are registered
//  _papplPrinterRegisterDNSSDNoLock(printer);

  pthread_rwlock_unlock(&printer->rwlock);

  _papplSystemConfigChanged(printer->system);
}


//
// 'papplPrinterSetImpressionsCompleted()' - Add impressions (sides) to the total count of printed impressions.
//

void
papplPrinterSetImpressionsCompleted(
    pappl_printer_t *printer,		// I - Printer
    int             add)		// I - Number of impressions/sides to add
{
  if (!printer || add <= 0)
    return;

  pthread_rwlock_wrlock(&printer->rwlock);

  printer->impcompleted += add;
  printer->state_time   = time(NULL);

  pthread_rwlock_unlock(&printer->rwlock);

  _papplSystemConfigChanged(printer->system);
}


//
// 'papplPrinterSetLocation()' - Set the location string.
//

void
papplPrinterSetLocation(
    pappl_printer_t *printer,		// I - Printer
    const char      *value)		// I - Location ("Bob's Office", etc.) or `NULL` for none
{
  if (!printer)
    return;

  pthread_rwlock_wrlock(&printer->rwlock);

  free(printer->location);
  printer->location    = value ? strdup(value) : NULL;
  printer->config_time = time(NULL);

  _papplPrinterRegisterDNSSDNoLock(printer);

  pthread_rwlock_unlock(&printer->rwlock);

  _papplSystemConfigChanged(printer->system);
}


//
// 'papplPrinterSetMaxActiveJobs()' - Set the maximum number of active jobs for the printer.
//

void
papplPrinterSetMaxActiveJobs(
    pappl_printer_t *printer,		// I - Printer
    int             max_active_jobs)	// I - Maximum number of active jobs, `0` for unlimited
{
  if (!printer || max_active_jobs < 0)
    return;

  pthread_rwlock_wrlock(&printer->rwlock);

  printer->max_active_jobs = max_active_jobs;
  printer->config_time     = time(NULL);

  pthread_rwlock_unlock(&printer->rwlock);

  _papplSystemConfigChanged(printer->system);
}


//
// 'papplPrinterSetMaxCompletedJobs()' - Set the maximum number of completed jobs for the printer.
//

void
papplPrinterSetMaxCompletedJobs(
    pappl_printer_t *printer,		// I - Printer
    int             max_completed_jobs)	// I - Maximum number of completed jobs, `0` for unlimited
{
  if (!printer || max_completed_jobs < 0)
    return;

  pthread_rwlock_wrlock(&printer->rwlock);

  printer->max_completed_jobs = max_completed_jobs;
  printer->config_time        = time(NULL);

  pthread_rwlock_unlock(&printer->rwlock);

  _papplSystemConfigChanged(printer->system);
}


//
// 'papplPrinterSetOrganization()' - Set the organization name.
//

void
papplPrinterSetOrganization(
    pappl_printer_t *printer,		// I - Printer
    const char      *value)		// I - Organization name or `NULL` for none
{
  if (!printer)
    return;

  pthread_rwlock_wrlock(&printer->rwlock);

  free(printer->organization);
  printer->organization = value ? strdup(value) : NULL;
  printer->config_time  = time(NULL);

  pthread_rwlock_unlock(&printer->rwlock);

  _papplSystemConfigChanged(printer->system);
}


//
// 'papplPrinterSetOrganizationalUnit()' - Set the organizational unit name.
//

void
papplPrinterSetOrganizationalUnit(
    pappl_printer_t *printer,		// I - Printer
    const char      *value)		// I - Organizational unit name or `NULL` for none
{
  if (!printer)
    return;

  pthread_rwlock_wrlock(&printer->rwlock);

  free(printer->org_unit);
  printer->org_unit    = value ? strdup(value) : NULL;
  printer->config_time = time(NULL);

  pthread_rwlock_unlock(&printer->rwlock);

  _papplSystemConfigChanged(printer->system);
}


//
// 'papplPrinterSetPrintDefaults()' - Set the default print option values.
//
// Note: Unlike @link papplPrinterSetPrintDriverData@, this function only changes
// the "xxx_default" member of the driver data and is considered lightweight.
//

void
papplPrinterSetPrintDefaults(
    pappl_printer_t      *printer,	// I - Printer
    pappl_pdriver_data_t *data)		// I - Driver data
{
  if (!printer || !data)
    return;

  pthread_rwlock_wrlock(&printer->rwlock);

  printer->driver_data.color_default          = data->color_default;
  printer->driver_data.content_default        = data->content_default;
  printer->driver_data.quality_default        = data->quality_default;
  printer->driver_data.scaling_default        = data->scaling_default;
  printer->driver_data.sides_default          = data->sides_default;
  printer->driver_data.x_default              = data->x_default;
  printer->driver_data.y_default              = data->y_default;
  printer->driver_data.media_default          = data->media_default;
  printer->driver_data.speed_default          = data->speed_default;
  printer->driver_data.darkness_default       = data->darkness_default;
  printer->driver_data.mode_configured        = data->mode_configured;
  printer->driver_data.tear_offset_configured = data->tear_offset_configured;
  printer->driver_data.darkness_configured    = data->darkness_configured;
  printer->driver_data.identify_default       = data->identify_default;

  printer->config_time = time(NULL);

  pthread_rwlock_unlock(&printer->rwlock);

  _papplSystemConfigChanged(printer->system);
}


//
// 'papplPrinterSetPrintGroup()' - Set the print authorization group, if any.
//

void
papplPrinterSetPrintGroup(
    pappl_printer_t *printer,		// I - Printer
    const char      *value)		// I - Print authorization group or `NULL` for none
{
  if (!printer)
    return;

  pthread_rwlock_wrlock(&printer->rwlock);

  free(printer->print_group);
  printer->print_group = value ? strdup(value) : NULL;
  printer->config_time = time(NULL);

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
    printer->print_gid = (gid_t)-1;

  pthread_rwlock_unlock(&printer->rwlock);

  _papplSystemConfigChanged(printer->system);
}


//
// 'papplPrinterSetReadyMedia()' - Set the ready (loaded) media.
//

void
papplPrinterSetReadyMedia(
    pappl_printer_t   *printer,		// I - Printer
    int               num_ready,	// I - Number of ready media
    pappl_media_col_t *ready)		// I - Array of ready media
{
  if (!printer || num_ready <= 0 || !ready)
    return;

  pthread_rwlock_wrlock(&printer->rwlock);

  if (num_ready > printer->driver_data.num_source)
    num_ready = printer->driver_data.num_source;

  memset(printer->driver_data.media_ready, 0, sizeof(printer->driver_data.media_ready));
  memcpy(printer->driver_data.media_ready, ready, (size_t)num_ready * sizeof(pappl_media_col_t));
  printer->state_time = time(NULL);

  pthread_rwlock_unlock(&printer->rwlock);

  _papplSystemConfigChanged(printer->system);
}


//
// 'papplPrinterSetReasons()' - Add or remove values from "printer-state-reasons".
//

void
papplPrinterSetReasons(
    pappl_printer_t *printer,		// I - Printer
    pappl_preason_t add,		// I - "printer-state-reasons" bit values to add or `PAPPL_PREASON_NONE` for none
    pappl_preason_t remove)		// I - "printer-state-reasons" bit values to remove or `PAPPL_PREASON_NONE` for none
{
  if (!printer)
    return;

  pthread_rwlock_wrlock(&printer->rwlock);

  printer->state_reasons &= ~remove;
  printer->state_reasons |= add;
  printer->state_time    = printer->status_time = time(NULL);

  pthread_rwlock_unlock(&printer->rwlock);
}


//
// 'papplPrinterSetSupplies()' - Set/update the supplies for a printer.
//

void
papplPrinterSetSupplies(
    pappl_printer_t *printer,		// I - Printer
    int             num_supplies,	// I - Number of supplies
    pappl_supply_t  *supplies)		// I - Array of supplies
{
  if (!printer || num_supplies < 0 || num_supplies > PAPPL_MAX_SUPPLY || (num_supplies > 0 && !supplies))
    return;

  pthread_rwlock_wrlock(&printer->rwlock);

  printer->num_supply = num_supplies;
  memset(printer->supply, 0, sizeof(printer->supply));
  if (supplies)
    memcpy(printer->supply, supplies, num_supplies * sizeof(pappl_supply_t));
  printer->state_time = time(NULL);

  pthread_rwlock_unlock(&printer->rwlock);
}
