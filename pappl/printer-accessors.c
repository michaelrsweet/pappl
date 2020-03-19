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
// 'papplPrinterGetDefaultInteger()' - Get the "xxx-default" integer/enum value.
//

int					// O - Default value
papplPrinterGetDefaultInteger(
    pappl_printer_t *printer,		// I - Printer
    const char      *name)		// I - Attribute name without "-default"
{
  char			defname[256];	// "xxx-default" name
  ipp_attribute_t	*attr;		// "xxx-default" attribute
  int			ret = 0;	// Return value

  if (!printer || !name)
    return (0);

  pthread_rwlock_rdlock(&printer->rwlock);

  snprintf(defname, sizeof(defname), "%s-default", name);
  if ((attr = ippFindAttribute(printer->driver_attrs, defname, IPP_TAG_ZERO)) != NULL)
    ret = ippGetInteger(attr, 0);

  pthread_rwlock_unlock(&printer->rwlock);

  return (ret);
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
// 'papplPrinterGetDefaultString()' - Get the "xxx-default" string (keyword) value.
//

char *					// O - Default value or `NULL` for none
papplPrinterGetDefaultString(
    pappl_printer_t *printer,		// I - Printer
    const char      *name,		// I - Attribute name without "-default"
    char            *buffer,		// I - String buffer
    size_t          bufsize)		// I - Size of string buffer
{
  char			defname[256];	// "xxx-default" name
  ipp_attribute_t	*attr;		// "xxx-default" attribute
  char			*ret = NULL;	// Return value


  if (!printer || !name || !buffer || bufsize < 1)
    return (0);

  pthread_rwlock_rdlock(&printer->rwlock);

  snprintf(defname, sizeof(defname), "%s-default", name);
  if ((attr = ippFindAttribute(printer->driver_attrs, defname, IPP_TAG_KEYWORD)) != NULL)
  {
    strlcpy(buffer, ippGetString(attr, 0, NULL), bufsize);
    ret = buffer;
  }

  pthread_rwlock_unlock(&printer->rwlock);

  return (ret);
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

  memcpy(ready, printer->driver_data.media_ready, (size_t)count * sizeof(pappl_media_col_t));

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
  int	count;				// Number of supplies


  if (!printer || max_supplies < 1 || !supplies)
    return (0);

  memset(supplies, 0, (size_t)max_supplies * sizeof(pappl_supply_t));

  pthread_rwlock_rdlock(&printer->rwlock);

  if ((count = printer->num_supply) > max_supplies)
    count = max_supplies;

  memcpy(supplies, printer->supply, (size_t)count * sizeof(pappl_supply_t));

  pthread_rwlock_unlock(&printer->rwlock);

  return (count);
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
// 'papplPrinterIterateDefaults()' - Iterate over the "xxx-default" attributes for a printer.
//

void
papplPrinterIterateDefaults(
    pappl_printer_t    *printer,	// I - Printer
    pappl_default_cb_t cb,		// I - Callback function
    void               *data)		// I - Callback data
{
  ipp_attribute_t	*attr;		// Current attribute
  const char		*name;		// Attribute name


  if (!printer || !cb)
    return;

  pthread_rwlock_rdlock(&printer->rwlock);

  for (attr = ippFirstAttribute(printer->driver_attrs); attr; attr = ippNextAttribute(printer->driver_attrs))
  {
    if ((name = ippGetName(attr)) != NULL && strstr(name, "-default"))
      (cb)(attr, data);
  }

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
// 'papplPrinterSetDefaultInteger()' - Set the "xxx-default" integer/enum value.
//

void
papplPrinterSetDefaultInteger(
    pappl_printer_t *printer,		// I - Printer
    const char      *name,		// I - Attribute name without "-default"
    int             value)		// I - Integer value
{
  char			defname[256];	// xxx-default name
  ipp_attribute_t	*attr;		// xxx-default attribute


  if (!printer || !name || !printer->driver_attrs)
    return;

  pthread_rwlock_wrlock(&printer->rwlock);

  snprintf(defname, sizeof(defname), "%s-default", name);
  if ((attr = ippFindAttribute(printer->driver_attrs, defname, IPP_TAG_ZERO)) != NULL)
    ippSetInteger(printer->driver_attrs, &attr, 0, value);
  else if (!strcmp(name, "finishings") || !strcmp(name, "print-quality"))
    ippAddInteger(printer->driver_attrs, IPP_TAG_PRINTER, IPP_TAG_ENUM, defname, value);
  else
    ippAddInteger(printer->driver_attrs, IPP_TAG_PRINTER, IPP_TAG_INTEGER, defname, value);

  pthread_rwlock_unlock(&printer->rwlock);
}


//
// 'papplPrinterSetDefaultMedia()' - Set the default media.
//

void
papplPrinterSetDefaultMedia(
    pappl_printer_t   *printer,		// I - Printer
    pappl_media_col_t *media)		// I - Default media
{
  if (!printer || !media)
    return;

  pthread_rwlock_wrlock(&printer->rwlock);

  memcpy(&printer->driver_data.media_default, media, sizeof(pappl_media_col_t));
  printer->config_time = time(NULL);

  pthread_rwlock_unlock(&printer->rwlock);
}


//
// 'papplPrinterSetDefaultString()' - Set the "xxx-default" string (keyword) value.
//
// Note: Use the @link papplPrinterSetDefaultMedia@ function to set the default
// media values.
//

void
papplPrinterSetDefaultString(
    pappl_printer_t *printer,		// I - Printer
    const char      *name,		// I - Attribute name without "-default"
    const char      *value)		// I - String (keyword) value
{
  char			defname[256];	// xxx-default name
  ipp_attribute_t	*attr;		// xxx-default attribute


  if (!printer || !name || !printer->driver_attrs || !value)
    return;

  pthread_rwlock_wrlock(&printer->rwlock);

  snprintf(defname, sizeof(defname), "%s-default", name);
  if ((attr = ippFindAttribute(printer->driver_attrs, defname, IPP_TAG_KEYWORD)) != NULL)
    ippSetString(printer->driver_attrs, &attr, 0, value);
  else
    ippAddString(printer->driver_attrs, IPP_TAG_PRINTER, IPP_TAG_KEYWORD, defname, NULL, value);

  pthread_rwlock_unlock(&printer->rwlock);
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
}


//
// 'papplPrinterSetImpressionsCompleted()' - Add impressions (side) to the total count of printed impressions.
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

  printer->state_reasons |= add;
  printer->state_reasons &= ~remove;
  printer->state_time    = time(NULL);

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
}
