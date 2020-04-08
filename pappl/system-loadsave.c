//
// System load/save functions for the Printer Application Framework
//
// Copyright © 2020 by Michael R Sweet.
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

static void	write_media_col(cups_file_t *fp, const char *name, pappl_media_col_t *media);


//
// 'papplSystemLoadState()' - Load the previous system state.
//

bool					// O - `true` on success, `false` on failure
papplSystemLoadState(
    pappl_system_t *system,		// I - System
    const char     *filename)		// I - File to load
{
  (void)system;
  (void)filename;

  return (false);
}


//
// 'papplSystemSaveState()' - Save the current system state.
//


bool					// O - `true` on success, `false` on failure
papplSystemSaveState(
    pappl_system_t *system,		// I - System
    const char     *filename)		// I - File to save
{
  int			i;		// Looping var
  cups_file_t		*fp;		// Output file
  pappl_printer_t	*printer;	// Current printer


  if ((fp = cupsFileOpen(filename, "w")) == NULL)
  {
    papplLog(system, PAPPL_LOGLEVEL_ERROR, "Unable to create system state file '%s': %s", filename, cupsLastErrorString());
    return (false);
  }

  papplLog(system, PAPPL_LOGLEVEL_INFO, "Saving system state to '%s'.", filename);

  pthread_rwlock_rdlock(&system->rwlock);

  if (system->dns_sd_name)
    cupsFilePutConf(fp, "DNSSDName", system->dns_sd_name);
  if (system->location)
    cupsFilePutConf(fp, "Location", system->location);
  if (system->geo_location)
    cupsFilePutConf(fp, "Geolocation", system->geo_location);
  if (system->organization)
    cupsFilePutConf(fp, "Organization", system->organization);
  if (system->org_unit)
    cupsFilePutConf(fp, "OrganizationalUnit", system->org_unit);
  if (system->contact.name[0])
    cupsFilePutConf(fp, "ContactName", system->contact.name);
  if (system->contact.email[0])
    cupsFilePutConf(fp, "ContactEMail", system->contact.email);
  if (system->contact.telephone[0])
    cupsFilePutConf(fp, "ContactTelephone", system->contact.telephone);

  if (system->admin_group)
    cupsFilePutConf(fp, "AdminGroup", system->admin_group);
  if (system->default_print_group)
    cupsFilePutConf(fp, "DefaultPrintGroup", system->default_print_group);
  if (system->password_hash[0])
    cupsFilePutConf(fp, "PasswordHash", system->password_hash);

  cupsFilePrintf(fp, "DefaultPrinterID %d\n", system->default_printer_id);
  cupsFilePrintf(fp, "NextPrinterID %d\n", system->next_printer_id);

  for (printer = (pappl_printer_t *)cupsArrayFirst(system->printers); printer; printer = (pappl_printer_t *)cupsArrayNext(system->printers))
  {
    if (printer->is_deleted)
      continue;

    cupsFilePrintf(fp, "<Printer %d>\n", printer->printer_id);
    cupsFilePutConf(fp, "Name", printer->name);
    cupsFilePutConf(fp, "DeviceURI", printer->device_uri);
    cupsFilePutConf(fp, "DriverName", printer->driver_name);
    if (printer->dns_sd_name)
      cupsFilePutConf(fp, "DNSSDName", printer->dns_sd_name);
    if (printer->location)
      cupsFilePutConf(fp, "Location", printer->location);
    if (printer->geo_location)
      cupsFilePutConf(fp, "Geolocation", printer->geo_location);
    if (printer->organization)
      cupsFilePutConf(fp, "Organization", printer->organization);
    if (printer->org_unit)
      cupsFilePutConf(fp, "OrganizationalUnit", printer->org_unit);
    if (printer->contact.name[0])
      cupsFilePutConf(fp, "ContactName", printer->contact.name);
    if (printer->contact.email[0])
      cupsFilePutConf(fp, "ContactEMail", printer->contact.email);
    if (printer->contact.telephone[0])
      cupsFilePutConf(fp, "ContactTelephone", printer->contact.telephone);
    if (printer->print_group)
      cupsFilePutConf(fp, "PrintGroup", printer->print_group);
    cupsFilePrintf(fp, "MaxActiveJobs %d\n", printer->max_active_jobs);
    cupsFilePrintf(fp, "NextJobId %d\n", printer->next_job_id);
    cupsFilePrintf(fp, "ImpressionsCompleted %d\n", printer->impcompleted);

    if (printer->driver_data.color_default)
      cupsFilePutConf(fp, "print-color-mode-default", _papplColorModeString(printer->driver_data.color_default));
    if (printer->driver_data.content_default)
      cupsFilePutConf(fp, "print-content-optimize-default", _papplContentString(printer->driver_data.content_default));
    if (printer->driver_data.quality_default)
      cupsFilePutConf(fp, "print-quality-default", ippEnumString("print-quality", printer->driver_data.quality_default));
    if (printer->driver_data.scaling_default)
      cupsFilePutConf(fp, "print-scaling-default", _papplScalingString(printer->driver_data.scaling_default));
    if (printer->driver_data.sides_default)
      cupsFilePutConf(fp, "sides-default", _papplSidesString(printer->driver_data.sides_default));
    if (printer->driver_data.x_default)
      cupsFilePrintf(fp, "priner-resolution-default %dx%ddpi\n", printer->driver_data.x_default, printer->driver_data.y_default);
    write_media_col(fp, "media-col-default", &printer->driver_data.media_default);

    for (i = 0; i < printer->driver_data.num_source; i ++)
    {
      if (printer->driver_data.media_ready[i].size_name[0])
      {
        char	name[128];		// Attribute name

        snprintf(name, sizeof(name), "media-col-ready%d", i);
        write_media_col(fp, name, printer->driver_data.media_ready + i);
      }
    }

    cupsFilePuts(fp, "</Printer>\n");
  }

  pthread_rwlock_unlock(&system->rwlock);

  cupsFileClose(fp);

  return (true);
}


//
// '()' - Write a media-col value...
//

static void
write_media_col(
    cups_file_t       *fp,		// I - File
    const char        *name,		// I - Attribute name
    pappl_media_col_t *media)		// I - Media value
{
  cupsFilePrintf(fp, "%s bottom=%d left=%d right=%d width=%d length=%d name='%s' source='%s' top=%d offset=%d tracking='%s' type='%s'\n", name, media->bottom_margin, media->left_margin, media->right_margin, media->size_width, media->size_length, media->size_name, media->source, media->top_margin, media->top_offset, media->tracking ? _papplMediaTrackingString(media->tracking) : "", media->type);
}

