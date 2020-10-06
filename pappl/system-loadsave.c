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

static void	parse_contact(char *value, pappl_contact_t *contact);
static void	parse_media_col(char *value, pappl_media_col_t *media);
static void	write_contact(cups_file_t *fp, pappl_contact_t *contact);
static void	write_media_col(cups_file_t *fp, const char *name, pappl_media_col_t *media);
static void	write_options(cups_file_t *fp, const char *name, int num_options, cups_option_t *options);


//
// 'papplSystemLoadState()' - Load the previous system state.
//
// This function loads the previous system state from a file created by the
// @link papplSystemSaveState@ function.
//
// Note: This function must be called prior to @link papplSystemRun@.
//

bool					// O - `true` on success, `false` on failure
papplSystemLoadState(
    pappl_system_t *system,		// I - System
    const char     *filename)		// I - File to load
{
  int			i;		// Looping var
  cups_file_t		*fp;		// Output file
  int			linenum;	// Line number
  char			line[2048],	// Line from file
			*value;		// Value from line


  // Range check input...
  if (!system || !filename)
  {
    return (false);
  }
  else if (system->is_running)
  {
    papplLog(system, PAPPL_LOGLEVEL_ERROR, "Cannot load system state while running.");
    return (false);
  }

  // Open the state file...
  if ((fp = cupsFileOpen(filename, "r")) == NULL)
  {
    if (errno != ENOENT)
      papplLog(system, PAPPL_LOGLEVEL_ERROR, "Unable to open system state file '%s': %s", filename, cupsLastErrorString());

    return (false);
  }

  // Read lines from the state file...
  papplLog(system, PAPPL_LOGLEVEL_INFO, "Loading system state from '%s'.", filename);

  linenum = 0;
  while (cupsFileGetConf(fp, line, sizeof(line), &value, &linenum))
  {
    if (!strcasecmp(line, "DNSSDName"))
      papplSystemSetDNSSDName(system, value);
    else if (!strcasecmp(line, "Location"))
      papplSystemSetLocation(system, value);
    else if (!strcasecmp(line, "GeoLocation"))
      papplSystemSetGeoLocation(system, value);
    else if (!strcasecmp(line, "Organization"))
      papplSystemSetOrganization(system, value);
    else if (!strcasecmp(line, "OrganizationalUnit"))
      papplSystemSetOrganizationalUnit(system, value);
    else if (!strcasecmp(line, "Contact"))
    {
      pappl_contact_t	contact;		// "system-contact" value

      parse_contact(value, &contact);
      papplSystemSetContact(system, &contact);
    }
    else if (!strcasecmp(line, "AdminGroup"))
      papplSystemSetAdminGroup(system, value);
    else if (!strcasecmp(line, "DefaultPrintGroup"))
      papplSystemSetDefaultPrintGroup(system, value);
    else if (!strcasecmp(line, "Password"))
      papplSystemSetPassword(system, value);
    else if (!strcasecmp(line, "DefaultPrinterID") && value)
      system->default_printer_id = atoi(value);
    else if (!strcasecmp(line, "NextPrinterID") && value)
      system->next_printer_id = atoi(value);
    else if (!strcasecmp(line, "UUID") && value)
      system->uuid = strdup(value);
    else if (!strcasecmp(line, "<Printer") && value)
    {
      // Read a printer...
      int		num_options;	// Number of options
      cups_option_t	*options = NULL;// Options
      const char	*printer_id,	// Printer ID
			*printer_name,	// Printer name
			*device_id,	// Device ID
			*device_uri,	// Device URI
			*driver_name;	// Driver name
      pappl_printer_t	*printer;	// Current printer


      if ((num_options = cupsParseOptions(value, 0, &options)) != 5 || (printer_id = cupsGetOption("id", num_options, options)) == NULL || atoi(printer_id) <= 0 || (printer_name = cupsGetOption("name", num_options, options)) == NULL || (device_id = cupsGetOption("did", num_options, options)) == NULL || (device_uri = cupsGetOption("uri", num_options, options)) == NULL || (driver_name = cupsGetOption("driver", num_options, options)) == NULL)
      {
        papplLog(system, PAPPL_LOGLEVEL_ERROR, "Bad printer definition on line %d of '%s'.", linenum, filename);
        break;
      }

      printer = papplPrinterCreate(system, PAPPL_SERVICE_TYPE_PRINT, atoi(printer_id), printer_name, driver_name, device_id, device_uri);

      while (cupsFileGetConf(fp, line, sizeof(line), &value, &linenum))
      {
        if (!strcasecmp(line, "</Printer>"))
          break;
	else if (!strcasecmp(line, "DNSSDName"))
	  papplPrinterSetDNSSDName(printer, value);
	else if (!strcasecmp(line, "Location"))
	  papplPrinterSetLocation(printer, value);
	else if (!strcasecmp(line, "GeoLocation"))
	  papplPrinterSetGeoLocation(printer, value);
	else if (!strcasecmp(line, "Organization"))
	  papplPrinterSetOrganization(printer, value);
	else if (!strcasecmp(line, "OrganizationalUnit"))
	  papplPrinterSetOrganizationalUnit(printer, value);
	else if (!strcasecmp(line, "Contact"))
	{
	  pappl_contact_t	contact;// "printer-contact" value

	  parse_contact(value, &contact);
	  papplPrinterSetContact(printer, &contact);
	}
	else if (!strcasecmp(line, "PrintGroup"))
	  papplPrinterSetPrintGroup(printer, value);
	else if (!strcasecmp(line, "MaxActiveJobs"))
	  papplPrinterSetMaxActiveJobs(printer, atoi(value));
	else if (!strcasecmp(line, "MaxCompletedJobs"))
	  papplPrinterSetMaxCompletedJobs(printer, atoi(value));
	else if (!strcasecmp(line, "NextJobId"))
	  printer->next_job_id = atoi(value);
	else if (!strcasecmp(line, "ImpressionsCompleted"))
	  printer->impcompleted = atoi(value);
	else if (!strcasecmp(line, "identify-actions-default"))
	  printer->driver_data.identify_default = _papplIdentifyActionsValue(value);
	else if (!strcasecmp(line, "label-mode-configured"))
	  printer->driver_data.mode_configured = _papplLabelModeValue(value);
	else if (!strcasecmp(line, "label-tear-offset-configured"))
	  printer->driver_data.tear_offset_configured = atoi(value);
	else if (!strcasecmp(line, "media-col-default"))
	  parse_media_col(value, &printer->driver_data.media_default);
	else if (!strncasecmp(line, "media-col-ready", 15))
	{
	  if ((i = atoi(line + 15)) >= 0 && i < PAPPL_MAX_SOURCE)
	    parse_media_col(value, printer->driver_data.media_ready + i);
	}
	else if (!strcasecmp(line, "orientation-requested-default"))
	  printer->driver_data.orient_default = (ipp_orient_t)ippEnumValue("orientation-requested", value);
	else if (!strcasecmp(line, "output-bin-default"))
	{
	  for (i = 0; i < printer->driver_data.num_bin; i ++)
	  {
	    if (!strcmp(value, printer->driver_data.bin[i]))
	    {
	      printer->driver_data.bin_default = i;
	      break;
	    }
	  }
	}
	else if (!strcasecmp(line, "print-color-mode-default"))
	  printer->driver_data.color_default = _papplColorModeValue(value);
	else if (!strcasecmp(line, "print-content-optimize-default"))
	  printer->driver_data.content_default = _papplContentValue(value);
	else if (!strcasecmp(line, "print-darkness-default"))
	  printer->driver_data.darkness_default = atoi(value);
	else if (!strcasecmp(line, "print-quality-default"))
	  printer->driver_data.quality_default = (ipp_quality_t)ippEnumValue("print-quality", value);
	else if (!strcasecmp(line, "print-scaling-default"))
	  printer->driver_data.scaling_default = _papplScalingValue(value);
	else if (!strcasecmp(line, "print-speed-default"))
	  printer->driver_data.speed_default = atoi(value);
	else if (!strcasecmp(line, "printer-darkness-configured"))
	  printer->driver_data.darkness_configured = atoi(value);
	else if (!strcasecmp(line, "printer-resolution-default") && value)
	  sscanf(value, "%dx%ddpi", &printer->driver_data.x_default, &printer->driver_data.y_default);
	else if (!strcasecmp(line, "sides-default"))
	  printer->driver_data.sides_default = _papplSidesValue(value);
	else if (!strcasecmp(line, "Job") && value)
	{
	  // Read printer job
	  pappl_job_t	*job;		// Current Job
	  struct stat	jobbuf;		// Job file buffer
	  const char	*job_name,	// Job name
			*job_id,	// Job ID
			*job_username,	// Job username
			*job_format,	// Job format
			*job_value;	// Job option value

	  num_options = cupsParseOptions(value, 0, &options);

	  if ((job_id = cupsGetOption("id", num_options, options)) == NULL || atoi(job_id) <= 0 || (job_name = cupsGetOption("name", num_options, options)) == NULL || (job_username = cupsGetOption("username", num_options, options)) == NULL || (job_format = cupsGetOption("format", num_options, options)) == NULL)
	  {
	    papplLog(system, PAPPL_LOGLEVEL_ERROR, "Bad Job definition on line %d of '%s'.", linenum, filename);
	    break;
	  }

	  if ((job = _papplJobCreate(printer, atoi(job_id), job_username, job_format, job_name, NULL)) == NULL)
	  {
	    papplLog(system, PAPPL_LOGLEVEL_ERROR, "Error creating job %s for printer %s", job_name, printer->name);
	    break;
	  }

	  if ((job_value = cupsGetOption("filename", num_options, options)) != NULL)
	    job->filename = strdup(job_value);
	  if ((job_value = cupsGetOption("state", num_options, options)) != NULL)
	    job->state = (ipp_jstate_t)atoi(job_value);
	  if ((job_value = cupsGetOption("state_reasons", num_options, options)) != NULL)
	    job->state_reasons = (ipp_jstate_t)atoi(job_value);
	  if ((job_value = cupsGetOption("created", num_options, options)) != NULL)
	    job->created = atol(job_value);
	  if ((job_value = cupsGetOption("processing", num_options, options)) != NULL)
	    job->processing = atol(job_value);
	  if ((job_value = cupsGetOption("completed", num_options, options)) != NULL)
	    job->completed = atol(job_value);
	  if ((job_value = cupsGetOption("impressions", num_options, options)) != NULL)
	    job->impressions = atoi(job_value);
	  if ((job_value = cupsGetOption("imcompleted", num_options, options)) != NULL)
	    job->impcompleted = atoi(job_value);

	  // Add the job to printer completed jobs array...
	  if (job->state < IPP_JSTATE_STOPPED)
	  {
	    // Load the file attributes from the spool directory...
	    int		attr_fd;	// Attribute file descriptor
	    char	job_attr_filename[256];		// Attribute filename

	    if ((attr_fd = papplJobOpenFile(job, job_attr_filename, sizeof(job_attr_filename), system->directory, "ipp", "r")) < 0)
	    {
	      papplLog(system, PAPPL_LOGLEVEL_ERROR, "Unable to open file for job attributes: '%s'.", job_attr_filename);
	      continue;
	    }

	    ippReadFile(attr_fd, job->attrs);
	    close(attr_fd);

	    if (!job->filename || stat(job->filename, &jobbuf))
	    {
	      // If file removed, then set job state to aborted...
	      job->state = IPP_JSTATE_ABORTED;
	    }
	    else
	    {
	      // Add the job to printer active jobs array...
	      cupsArrayAdd(printer->active_jobs, job);
	    }
	  }
	  else
	  {
	    // Add job to printer completed jobs...
	    cupsArrayAdd(printer->completed_jobs, job);
	  }
	}
	else
	  papplLog(system, PAPPL_LOGLEVEL_WARN, "Unknown printer directive '%s' on line %d of '%s'.", line, linenum, filename);
      }
    }
    else
    {
      papplLog(system, PAPPL_LOGLEVEL_WARN, "Unknown directive '%s' on line %d of '%s'.", line, linenum, filename);
    }
  }

  cupsFileClose(fp);

  return (true);
}


//
// 'papplSystemSaveState()' - Save the current system state.
//
// This function saves the current system state to a file.  It is typically
// used with the @link papplSystemSetSaveCallback@ function to periodically
// save the state:
//
// ```
// |papplSystemSetSaveCallback(system, (pappl_save_cb_t)papplSystemSaveState,
// |    (void *)filename);
// ```
//

bool					// O - `true` on success, `false` on failure
papplSystemSaveState(
    pappl_system_t *system,		// I - System
    const char     *filename)		// I - File to save
{
  int			i;		// Looping var
  cups_file_t		*fp;		// Output file
  pappl_printer_t	*printer;	// Current printer
  pappl_job_t		*job;		// Current Job


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
  write_contact(fp, &system->contact);
  if (system->admin_group)
    cupsFilePutConf(fp, "AdminGroup", system->admin_group);
  if (system->default_print_group)
    cupsFilePutConf(fp, "DefaultPrintGroup", system->default_print_group);
  if (system->password_hash[0])
    cupsFilePutConf(fp, "Password", system->password_hash);
  cupsFilePrintf(fp, "DefaultPrinterID %d\n", system->default_printer_id);
  cupsFilePrintf(fp, "NextPrinterID %d\n", system->next_printer_id);
  cupsFilePutConf(fp, "UUID", system->uuid);

  for (printer = (pappl_printer_t *)cupsArrayFirst(system->printers); printer; printer = (pappl_printer_t *)cupsArrayNext(system->printers))
  {
    int			num_options = 0;// Number of options
    cups_option_t	*options = NULL;// Options

    if (printer->is_deleted)
      continue;

    num_options = cupsAddIntegerOption("id", printer->printer_id, num_options, &options);
    num_options = cupsAddOption("name", printer->name, num_options, &options);
    num_options = cupsAddOption("did", printer->device_id ? printer->device_id : "", num_options, &options);
    num_options = cupsAddOption("uri", printer->device_uri, num_options, &options);
    num_options = cupsAddOption("driver", printer->driver_name, num_options, &options);

    write_options(fp, "<Printer", num_options, options);
    cupsFreeOptions(num_options, options);

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
    write_contact(fp, &printer->contact);
    if (printer->print_group)
      cupsFilePutConf(fp, "PrintGroup", printer->print_group);
    cupsFilePrintf(fp, "MaxActiveJobs %d\n", printer->max_active_jobs);
    cupsFilePrintf(fp, "MaxCompletedJobs %d\n", printer->max_completed_jobs);
    cupsFilePrintf(fp, "NextJobId %d\n", printer->next_job_id);
    cupsFilePrintf(fp, "ImpressionsCompleted %d\n", printer->impcompleted);

    if (printer->driver_data.identify_default)
      cupsFilePutConf(fp, "identify-actions-default", _papplIdentifyActionsString(printer->driver_data.identify_default));

    if (printer->driver_data.mode_configured)
      cupsFilePutConf(fp, "label-mode-configured", _papplLabelModeString(printer->driver_data.mode_configured));
    if (printer->driver_data.tear_offset_configured)
      cupsFilePrintf(fp, "label-tear-offset-configured %d\n", printer->driver_data.tear_offset_configured);

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
    if (printer->driver_data.orient_default)
      cupsFilePutConf(fp, "orientation-requested-default", ippEnumString("orientation-requested", printer->driver_data.orient_default));
    if (printer->driver_data.bin_default && printer->driver_data.num_bin > 0)
      cupsFilePutConf(fp, "output-bin-default", printer->driver_data.bin[printer->driver_data.bin_default]);
    if (printer->driver_data.color_default)
      cupsFilePutConf(fp, "print-color-mode-default", _papplColorModeString(printer->driver_data.color_default));
    if (printer->driver_data.content_default)
      cupsFilePutConf(fp, "print-content-optimize-default", _papplContentString(printer->driver_data.content_default));
    if (printer->driver_data.darkness_default)
      cupsFilePrintf(fp, "print-darkness-default %d\n", printer->driver_data.darkness_default);
    if (printer->driver_data.quality_default)
      cupsFilePutConf(fp, "print-quality-default", ippEnumString("print-quality", printer->driver_data.quality_default));
    if (printer->driver_data.scaling_default)
      cupsFilePutConf(fp, "print-scaling-default", _papplScalingString(printer->driver_data.scaling_default));
    if (printer->driver_data.darkness_default)
      cupsFilePrintf(fp, "printer-darkness-configured %d\n", printer->driver_data.darkness_configured);
    if (printer->driver_data.sides_default)
      cupsFilePutConf(fp, "sides-default", _papplSidesString(printer->driver_data.sides_default));
    if (printer->driver_data.x_default)
      cupsFilePrintf(fp, "printer-resolution-default %dx%ddpi\n", printer->driver_data.x_default, printer->driver_data.y_default);

    for (job = (pappl_job_t *)cupsArrayFirst(printer->all_jobs); job; job = (pappl_job_t *)cupsArrayNext(printer->all_jobs))
    {
      // Add basic job attributes...
      num_options = 0;
      num_options = cupsAddIntegerOption("id", job->job_id, num_options, &options);
      num_options = cupsAddOption("name", job->name, num_options, &options);
      num_options = cupsAddOption("username", job->username, num_options, &options);
      num_options = cupsAddOption("format", job->format, num_options, &options);

      if (job->filename)
        num_options = cupsAddOption("filename", job->filename, num_options, &options);
      if (job->state)
        num_options = cupsAddIntegerOption("state", job->state, num_options, &options);
      if (job->state_reasons)
        num_options = cupsAddIntegerOption("state_reasons", job->state_reasons, num_options, &options);
      if (job->created)
        num_options = cupsAddIntegerOption("created", (int)job->created, num_options, &options);
      if (job->processing)
        num_options = cupsAddIntegerOption("processing", (int)job->processing, num_options, &options);
      if (job->completed)
        num_options = cupsAddIntegerOption("completed", (int)job->completed, num_options, &options);
      if (job->impressions)
        num_options = cupsAddIntegerOption("impressions", job->impressions, num_options, &options);
      if (job->impcompleted)
        num_options = cupsAddIntegerOption("imcompleted", job->impcompleted, num_options, &options);

      if (job->attrs)
      {
	int	attr_fd;	// Attribute file descriptor
	char	job_attr_filename[1024];	// Attribute filename

        // Save job attributes to file in spool directory...
        if (job->state < IPP_JSTATE_STOPPED)
        {
          if ((attr_fd = papplJobOpenFile(job, job_attr_filename, sizeof(job_attr_filename), system->directory, "ipp", "w")) < 0)
          {
            papplLog(system, PAPPL_LOGLEVEL_ERROR, "Unable to create file for job attributes: '%s'.", job_attr_filename);
            continue;
          }

          ippWriteFile(attr_fd, job->attrs);
          close(attr_fd);
        }
        else
        {
          // If job completed or aborted, remove job-attributes file...
          papplJobOpenFile(job, job_attr_filename, sizeof(job_attr_filename), system->directory, "ipp", "x");
        }
      }

      write_options(fp, "Job", num_options, options);
      cupsFreeOptions(num_options, options);
    }

    cupsFilePuts(fp, "</Printer>\n");
  }

  pthread_rwlock_unlock(&system->rwlock);

  cupsFileClose(fp);

  return (true);
}


//
// 'parse_contact()' - Parse a contact value.
//

static void
parse_contact(char            *value,	// I - Value
              pappl_contact_t *contact)	// O - Contact
{
  int		i,			// Looping var
		num_options;		// Number of options
  cups_option_t	*options = NULL,	// Options
		*option;		// Current option


  memset(contact, 0, sizeof(pappl_contact_t));
  num_options = cupsParseOptions(value, 0, &options);

  for (i = num_options, option = options; i > 0; i --, option ++)
  {
    if (!strcasecmp(option->name, "name"))
      strlcpy(contact->name, option->value, sizeof(contact->name));
    else if (!strcasecmp(option->name, "email"))
      strlcpy(contact->email, option->value, sizeof(contact->email));
    else if (!strcasecmp(option->name, "telephone"))
      strlcpy(contact->telephone, option->value, sizeof(contact->telephone));
  }

  cupsFreeOptions(num_options, options);
}


//
// 'parse_media_col()' - Parse a media-col value.
//

static void
parse_media_col(
    char              *value,		// I - Value
    pappl_media_col_t *media)		// O - Media collection
{
  int		i,			// Looping var
		num_options;		// Number of options
  cups_option_t	*options = NULL,	// Options
		*option;		// Current option


  memset(media, 0, sizeof(pappl_media_col_t));
  num_options = cupsParseOptions(value, 0, &options);

  for (i = num_options, option = options; i > 0; i --, option ++)
  {
    if (!strcasecmp(option->name, "bottom"))
      media->bottom_margin = atoi(option->value);
    else if (!strcasecmp(option->name, "left"))
      media->left_margin = atoi(option->value);
    else if (!strcasecmp(option->name, "right"))
      media->right_margin = atoi(option->value);
    else if (!strcasecmp(option->name, "name"))
      strlcpy(media->size_name, option->value, sizeof(media->size_name));
    else if (!strcasecmp(option->name, "width"))
      media->size_width = atoi(option->value);
    else if (!strcasecmp(option->name, "length"))
      media->size_length = atoi(option->value);
    else if (!strcasecmp(option->name, "source"))
      strlcpy(media->source, option->value, sizeof(media->source));
    else if (!strcasecmp(option->name, "top"))
      media->top_margin = atoi(option->value);
    else if (!strcasecmp(option->name, "offset"))
      media->top_offset = atoi(option->value);
    else if (!strcasecmp(option->name, "tracking"))
      media->tracking = _papplMediaTrackingValue(option->value);
    else if (!strcasecmp(option->name, "type"))
      strlcpy(media->type, option->value, sizeof(media->type));
  }

  cupsFreeOptions(num_options, options);
}


//
// 'write_contact()' - Write an "xxx-contact" value.
//

static void
write_contact(cups_file_t     *fp,	// I - File
              pappl_contact_t *contact)	// I - Contact
{
  int		num_options = 0;	// Number of options
  cups_option_t	*options = NULL;	// Options


  if (contact->name[0])
    num_options = cupsAddOption("name", contact->name, num_options, &options);
  if (contact->email[0])
    num_options = cupsAddOption("email", contact->email, num_options, &options);
  if (contact->telephone[0])
    num_options = cupsAddOption("telephone", contact->telephone, num_options, &options);

  write_options(fp, "Contact", num_options, options);
  cupsFreeOptions(num_options, options);
}


//
// 'write_media_col()' - Write a media-col value...
//

static void
write_media_col(
    cups_file_t       *fp,		// I - File
    const char        *name,		// I - Attribute name
    pappl_media_col_t *media)		// I - Media value
{
  int		num_options = 0;	// Number of options
  cups_option_t	*options = NULL;	// Options


  if (media->bottom_margin)
    num_options = cupsAddIntegerOption("bottom", media->bottom_margin, num_options, &options);
  if (media->left_margin)
    num_options = cupsAddIntegerOption("left", media->left_margin, num_options, &options);
  if (media->right_margin)
    num_options = cupsAddIntegerOption("right", media->right_margin, num_options, &options);
  if (media->size_name[0])
    num_options = cupsAddOption("name", media->size_name, num_options, &options);
  if (media->size_width)
    num_options = cupsAddIntegerOption("width", media->size_width, num_options, &options);
  if (media->size_length)
    num_options = cupsAddIntegerOption("length", media->size_length, num_options, &options);
  if (media->source[0])
    num_options = cupsAddOption("source", media->source, num_options, &options);
  if (media->top_margin)
    num_options = cupsAddIntegerOption("top", media->top_margin, num_options, &options);
  if (media->top_offset)
    num_options = cupsAddIntegerOption("offset", media->top_offset, num_options, &options);
  if (media->tracking)
    num_options = cupsAddOption("tracking", _papplMediaTrackingString(media->tracking), num_options, &options);
  if (media->type[0])
    num_options = cupsAddOption("type", media->type, num_options, &options);

  write_options(fp, name, num_options, options);
  cupsFreeOptions(num_options, options);
}


//
// 'write_options()' - Write a CUPS options array value...
//

static void
write_options(cups_file_t   *fp,	// I - File
              const char    *name,	// I - Attribute name
              int           num_options,// I - Number of options
              cups_option_t *options)	// I - Options
{
  const char	*start,			// Start of current subset
                *ptr;			// Pointer into value


  cupsFilePuts(fp, name);
  while (num_options > 0)
  {
    cupsFilePrintf(fp, " %s=\"", options->name);

    for (start = options->value, ptr = start; *ptr; ptr ++)
    {
      if (*ptr == '\\' || *ptr == '\"')
      {
        if (ptr > start)
          cupsFileWrite(fp, start, ptr - start);

	cupsFilePutChar(fp, '\\');
	start = ptr;
      }
    }

    if (ptr > start)
      cupsFileWrite(fp, start, ptr - start);

    cupsFilePutChar(fp, '\"');

    num_options --;
    options ++;
  }

  if (*name == '<')
    cupsFilePuts(fp, ">\n");
  else
    cupsFilePutChar(fp, '\n');
}
