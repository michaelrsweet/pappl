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
// 'papplPrinterGetDefaultMedia()' - Get the default media.
//

void
papplPrinterGetDefaultMedia(
    pappl_printer_t   *printer,		// I - Printer
    pappl_media_col_t *media)		// O - Default media
{
  if (!printer || !media)
    return;

  pthread_rwlock_rdlock(&printer->rwlock);

  *media = printer->driver_data.media_default;

  pthread_rwlock_unlock(&printer->rwlock);
}


//
// 'papplPrinterGetDriverData()' - Get the current driver data.
//

pappl_driver_data_t *			// O - Driver data or `NULL` if none
papplPrinterGetDriverData(
    pappl_printer_t     *printer,	// I - Printer
    pappl_driver_data_t *data)		// I - Pointer to driver data structure to fill
{
  if (!printer || !printer->driver_name || !data)
  {
    if (data)
      memset(data, 0, sizeof(pappl_driver_data_t));

    return (NULL);
  }

  memcpy(data, &printer->driver_data, sizeof(pappl_driver_data_t));

  return (data);
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
// 'papplPrinterGetDriverName()' - Get the current driver name.
//

char *					// O - Driver name or `NULL` for none
papplPrinterGetDriverName(
    pappl_printer_t *printer,		// I - Printer
    char            *buffer,		// I - String buffer
    size_t          bufsize)		// I - Size of string buffer
{
  if (!printer || !printer->driver_name || !buffer || bufsize == 0)
  {
    if (buffer)
      *buffer = '\0';

    return (NULL);
  }

  pthread_rwlock_rdlock(&printer->rwlock);
  strlcpy(buffer, printer->driver_name, bufsize);
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
// 'papplPrinterGetMediaSources()' - Get the list of media sources.
//

int					// O - Number of media sources
papplPrinterGetMediaSources(
    pappl_printer_t   *printer,		// I - Printer
    int               max_sources,	// I - Maximum number of sources
    pappl_media_col_t *sources)		// I - Array for sources
{
  int	i;				// Looping var


  if (!printer || max_sources < 1 || !sources)
    return (0);

  memset(sources, 0, (size_t)max_sources * sizeof(pappl_media_col_t));

  pthread_rwlock_rdlock(&printer->rwlock);

  for (i = 0; i < printer->driver_data.num_source && i < max_sources; i ++)
    strlcpy(sources[i].source, printer->driver_data.source[i], sizeof(sources[0].source));

  pthread_rwlock_unlock(&printer->rwlock);

  return (i);
}


//
// 'papplPrinterGetMediaTypes()' - Get the list of media types.
//

int					// O - Number of media types.
papplPrinterGetMediaTypes(
    pappl_printer_t   *printer,		// I - Printer
    int               max_types,	// I - Maximum number of types
    pappl_media_col_t *types)		// I - Array for types
{
  int	i;				// Looping var


  if (!printer || max_types < 1 || !types)
    return (0);

  memset(types, 0, (size_t)max_types * sizeof(pappl_media_col_t));

  pthread_rwlock_rdlock(&printer->rwlock);

  for (i = 0; i < printer->driver_data.num_source && i < max_types; i ++)
    strlcpy(types[i].type, printer->driver_data.type[i], sizeof(types[0].type));

  pthread_rwlock_unlock(&printer->rwlock);

  return (i);
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
// 'papplPrinterGetReadyMedia()' - Get a list of ready (loaded) media.
//

int					// O - Number of ready media
papplPrinterGetReadyMedia(
    pappl_printer_t   *printer,		// I - Printer
    int               max_ready,	// I - Maximum number of ready media
    pappl_media_col_t *ready)		// I - Array for ready media
{
  int	count;				// Number of values to copy


  if (!printer || max_ready < 1 || !ready)
    return (0);

  memset(ready, 0, (size_t)max_ready * sizeof(pappl_media_col_t));

  pthread_rwlock_rdlock(&printer->rwlock);

  if (printer->driver_data.num_source > max_ready)
    count = max_ready;
  else
    count = printer->driver_data.num_source;

  memcpy(ready, printer->drvier_data.media_ready, (size_t)count * sizeof(pappl_media_col_t));

  pthread_rwlock_unlock(&printer->rwlock);

  return (count);
}


//
// 'papplPrinterGetReasons()' - Get the current "printer-state-reasons" bit values.
//

pappl_preason_t				// O - "printer-state-reasons" bit values
papplPrinterGetReasons(
    pappl_printer_t *printer)		// I - Printer
{
  return (printer ? printer->state_reasons : PAPPL_PREASON_NONE);
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
}


//
// 'papplPrinterGetSupportedMedia()' - Get the list of supported media values.
//

int					// O - Number of supported media
papplPrinterGetSupportedMedia(
    pappl_printer_t   *printer,		// I - Printer
    int               max_supported,	// I - Maximum number of media
    pappl_media_col_t *supported)	// I - Array for media
{
  int		i;			// Looping var
  pwg_media_t	*pwg;			// PWG media size info


  if (!printer || max_supported < 1 || !supported)
    return (0);

  memset(supported, 0, (size_t)max_supported * sizeof(pappl_media_col_t));

  pthread_rwlock_rdlock(&printer->rwlock);

  for (i = 0; i < printer->driver_data.num_media && i < max_supported; i ++)
  {
    strlcpy(supported[i].size_name, printer->driver_data.media[i], sizeof(supported[0].size_name));

    if ((pwg = pwgMediaForPWG(printer->driver_data.media[i])) != NULL)
    {
      supported[i].size_width    = pwg->width;
      supported[i].size_length   = pwg->length;
      supported[i].bottom_margin = printer->driver_data.bottom_top;
      supported[i].left_margin   = printer->driver_data.left_right;
      supported[i].right_margin  = printer->driver_data.left_right;
      supported[i].top_margin    = printer->driver_data.bottom_top;
    }
  }

  pthread_rwlock_unlock(&printer->rwlock);

  return (i);
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

  for (job = (pappl_job_t *)cupsArrayFirst(printers->active_jobs); job; job = (pappl_job_t *)cupsArrayNext(printers->active_jobs))
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

  for (job = (pappl_job_t *)cupsArrayFirst(printers->all_jobs); job; job = (pappl_job_t *)cupsArrayNext(printers->all_jobs))
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

  for (job = (pappl_job_t *)cupsArrayFirst(printers->completed_jobs); job; job = (pappl_job_t *)cupsArrayNext(printers->completed_jobs))
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
}


//
// 'papplPrinterSetDefaultMedia()' - Set the default media.
//

void
papplPrinterSetDefaultMedia(
    pappl_printer_t   *printer,		// I - Printer
    pappl_media_col_t *media)		// I - Default media
{
}


//
// '()' - .
//

void		papplPrinterSetDNSSDName(pappl_printer_t *printer, const char *value)
{
}


//
// '()' - .
//

void		papplPrinterSetDriverData(pappl_printer_t *printer, pappl_driver_data_t *data)
{
}


//
// '()' - .
//

void		papplPrinterSetGeoLocation(pappl_printer_t *printer, const char *value)
{
}


//
// '()' - .
//

void		papplPrinterSetImpressionsCompleted(pappl_printer_t *printer, int add)
{
}


//
// '()' - .
//

void		papplPrinterSetLocation(pappl_printer_t *printer, const char *value)
{
}


//
// '()' - .
//

void		papplPrinterSetMaxActiveJobs(pappl_printer_t *printer, int max_active_jobs)
{
}


//
// '()' - .
//

void		papplPrinterSetOrganization(pappl_printer_t *printer, const char *value)
{
}


//
// '()' - .
//

void		papplPrinterSetOrganizationalUnit(pappl_printer_t *printer, const char *value)
{
}


//
// '()' - .
//

void		papplPrinterSetPrintGroup(pappl_printer_t *printer, const char *value)
{
}


//
// '()' - .
//

void		papplPrinterSetReadyMedia(pappl_printer_t *printer, int num_ready, pappl_media_col_t *media_col)
{
}


//
// '()' - .
//

void		papplPrinterSetReasons(pappl_printer_t *printer, pappl_preason_t add, pappl_preason_t remove)
{
}


//
// '()' - .
//

void		papplPrinterSetSupplies(pappl_printer_t *printer, int num_supplies, pappl_supply_t *supplies)
{
}



