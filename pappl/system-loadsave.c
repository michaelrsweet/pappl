//
// System load/save functions for the Printer Application Framework
//
// Copyright © 2020-2023 by Michael R Sweet.
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

static cups_len_t add_time(const char *name, time_t value, cups_len_t num_options, cups_option_t **options);
static void	parse_contact(char *value, pappl_contact_t *contact);
static void	parse_media_col(char *value, pappl_media_col_t *media);
static char	*read_line(cups_file_t *fp, char *line, size_t linesize, char **value, int *linenum);
static void	write_contact(cups_file_t *fp, pappl_contact_t *contact);
static void	write_media_col(cups_file_t *fp, const char *name, pappl_media_col_t *media);
static void	write_options(cups_file_t *fp, const char *name, cups_len_t num_options, cups_option_t *options);


//
// 'papplSystemLoadState()' - Load the previous system state.
//
// This function loads the previous system state from a file created by the
// @link papplSystemSaveState@ function.  The system state contains all of the
// system object values, the list of printers, and the jobs for each printer.
//
// When loading a printer definition, if the printer cannot be created (e.g.,
// because the driver name is no longer valid) then that printer and all of its
// job history will be lost.  In the case of a bad driver name, a printer
// application's driver callback can perform any necessary mapping of the driver
// name, including the use its auto-add callback to find a compatible new
// driver.
//
// > Note: This function must be called prior to @link papplSystemRun@.
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
			*ptr,		// Pointer into line/value
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
      papplLog(system, PAPPL_LOGLEVEL_ERROR, "Unable to open system state file '%s': %s", filename, cupsGetErrorString());

    return (false);
  }

  // Read lines from the state file...
  papplLog(system, PAPPL_LOGLEVEL_INFO, "Loading system state from '%s'.", filename);

  linenum = 0;
  while (read_line(fp, line, sizeof(line), &value, &linenum))
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
      papplSystemSetDefaultPrinterID(system, (int)strtol(value, NULL, 10));
    else if (!strcasecmp(line, "MaxImageSize") && value)
    {
      long	max_size;		// Maximum (uncompressed) size
      int	max_width,		// Maximum width in columns
		max_height;		// Maximum height in lines

      if (sscanf(value, "%ld%d%d", &max_size, &max_width, &max_height) == 3)
        papplSystemSetMaxImageSize(system, (size_t)max_size, max_width, max_height);
    }
    else if (!strcasecmp(line, "NextPrinterID") && value)
      papplSystemSetNextPrinterID(system, (int)strtol(value, NULL, 10));
    else if (!strcasecmp(line, "UUID") && value)
    {
      if ((system->uuid = strdup(value)) == NULL)
      {
        papplLog(system, PAPPL_LOGLEVEL_ERROR, "Unable to allocate memory for system UUID.");
        return (false);
      }
    }
    else if (!strcasecmp(line, "<Printer") && value)
    {
      // Read a printer...
      cups_len_t	num_options;	// Number of options
      cups_option_t	*options = NULL;// Options
      const char	*printer_id,	// Printer ID
			*printer_name,	// Printer name
			*printer_state,	// Printer state
			*device_id,	// Device ID
			*device_uri,	// Device URI
			*driver_name;	// Driver name
      pappl_printer_t	*printer;	// Current printer


      if ((num_options = cupsParseOptions(value, 0, &options)) != 5 || (printer_id = cupsGetOption("id", num_options, options)) == NULL || strtol(printer_id, NULL, 10) <= 0 || (printer_name = cupsGetOption("name", num_options, options)) == NULL || (device_id = cupsGetOption("did", num_options, options)) == NULL || (device_uri = cupsGetOption("uri", num_options, options)) == NULL || (driver_name = cupsGetOption("driver", num_options, options)) == NULL)
      {
        papplLog(system, PAPPL_LOGLEVEL_ERROR, "Bad printer definition on line %d of '%s'.", linenum, filename);
        break;
      }

      if ((printer = papplPrinterCreate(system, (int)strtol(printer_id, NULL, 10), printer_name, driver_name, device_id, device_uri)) == NULL)
      {
	if (errno == EEXIST)
	  papplLog(system, PAPPL_LOGLEVEL_ERROR, "Printer '%s' already exists, dropping duplicate printer and job history in state file.", printer_name);
	else if (errno == EIO)
	  papplLog(system, PAPPL_LOGLEVEL_ERROR, "Dropping printer '%s' and its job history because the driver ('%s') is no longer supported.", printer_name, driver_name);
	else
	  papplLog(system, PAPPL_LOGLEVEL_ERROR, "Dropping printer '%s' and its job history because an error occurred: %s", printer_name, strerror(errno));
      }

      if ((system->options & PAPPL_SOPTIONS_MULTI_QUEUE) && (printer_state = cupsGetOption("state", num_options, options)) != NULL && (ipp_pstate_t)atoi(printer_state) == IPP_PSTATE_STOPPED)
        papplPrinterPause(printer);

      while (read_line(fp, line, sizeof(line), &value, &linenum))
      {
        if (!strcasecmp(line, "</Printer>"))
          break;
	else if (!printer)
	  continue;
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
	else if (!strcasecmp(line, "HoldNewJobs"))
	  printer->hold_new_jobs = true;
	else if (!strcasecmp(line, "PrintGroup"))
	  papplPrinterSetPrintGroup(printer, value);
	else if (!strcasecmp(line, "MaxActiveJobs") && value)
	  papplPrinterSetMaxActiveJobs(printer, (int)strtol(value, NULL, 10));
	else if (!strcasecmp(line, "MaxCompletedJobs") && value)
	  papplPrinterSetMaxCompletedJobs(printer, (int)strtol(value, NULL, 10));
	else if (!strcasecmp(line, "NextJobId") && value)
	  papplPrinterSetNextJobID(printer, (int)strtol(value, NULL, 10));
	else if (!strcasecmp(line, "ImpressionsCompleted") && value)
	  papplPrinterSetImpressionsCompleted(printer, (int)strtol(value, NULL, 10));
	else if (!strcasecmp(line, "identify-actions-default"))
	  printer->driver_data.identify_default = _papplIdentifyActionsValue(value);
	else if (!strcasecmp(line, "label-mode-configured"))
	  printer->driver_data.mode_configured = _papplLabelModeValue(value);
	else if (!strcasecmp(line, "label-tear-offset-configured") && value)
	  printer->driver_data.tear_offset_configured = (int)strtol(value, NULL, 10);
	else if (!strcasecmp(line, "media-col-default"))
	  parse_media_col(value, &printer->driver_data.media_default);
	else if (!strncasecmp(line, "media-col-ready", 15))
	{
	  if ((i = (int)strtol(line + 15, NULL, 10)) >= 0 && i < PAPPL_MAX_SOURCE)
	    parse_media_col(value, printer->driver_data.media_ready + i);
	}
	else if (!strcasecmp(line, "orientation-requested-default"))
	  printer->driver_data.orient_default = (ipp_orient_t)ippEnumValue("orientation-requested", value);
	else if (!strcasecmp(line, "output-bin-default") && value)
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
	else if (!strcasecmp(line, "print-darkness-default") && value)
	  printer->driver_data.darkness_default = (int)strtol(value, NULL, 10);
	else if (!strcasecmp(line, "print-quality-default"))
	  printer->driver_data.quality_default = (ipp_quality_t)ippEnumValue("print-quality", value);
	else if (!strcasecmp(line, "print-scaling-default"))
	  printer->driver_data.scaling_default = _papplScalingValue(value);
	else if (!strcasecmp(line, "print-speed-default") && value)
	  printer->driver_data.speed_default = (int)strtol(value, NULL, 10);
	else if (!strcasecmp(line, "printer-darkness-configured") && value)
	  printer->driver_data.darkness_configured = (int)strtol(value, NULL, 10);
	else if (!strcasecmp(line, "printer-resolution-default") && value)
	  sscanf(value, "%dx%ddpi", &printer->driver_data.x_default, &printer->driver_data.y_default);
	else if (!strcasecmp(line, "sides-default"))
	  printer->driver_data.sides_default = _papplSidesValue(value);
        else if ((ptr = strstr(line, "-default")) != NULL)
        {
          char	defname[128],		// xxx-default name
	      	supname[128];		// xxx-supported name
	  ipp_attribute_t *attr;	// Attribute

          *ptr = '\0';

          snprintf(defname, sizeof(defname), "%s-default", line);
          snprintf(supname, sizeof(supname), "%s-supported", line);

          if (!value)
            value = ptr;

	  ippDeleteAttribute(printer->driver_attrs, ippFindAttribute(printer->driver_attrs, defname, IPP_TAG_ZERO));

          if ((attr = ippFindAttribute(printer->driver_attrs, supname, IPP_TAG_ZERO)) != NULL)
          {
            switch (ippGetValueTag(attr))
            {
              case IPP_TAG_BOOLEAN :
                  ippAddBoolean(printer->driver_attrs, IPP_TAG_PRINTER, defname, !strcmp(value, "true"));
                  break;

              case IPP_TAG_INTEGER :
              case IPP_TAG_RANGE :
                  ippAddInteger(printer->driver_attrs, IPP_TAG_PRINTER, IPP_TAG_INTEGER, defname, (int)strtol(value, NULL, 10));
                  break;

              case IPP_TAG_KEYWORD :
		  ippAddString(printer->driver_attrs, IPP_TAG_PRINTER, IPP_TAG_KEYWORD, defname, NULL, value);
                  break;

              default :
                  break;
            }
	  }
          else
          {
            ippAddString(printer->driver_attrs, IPP_TAG_PRINTER, IPP_TAG_TEXT, defname, NULL, value);
          }
        }
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

	  if ((job_id = cupsGetOption("id", num_options, options)) == NULL || strtol(job_id, NULL, 10) <= 0 || (job_name = cupsGetOption("name", num_options, options)) == NULL || (job_username = cupsGetOption("username", num_options, options)) == NULL || (job_format = cupsGetOption("format", num_options, options)) == NULL)
	  {
	    papplLog(system, PAPPL_LOGLEVEL_ERROR, "Bad Job definition on line %d of '%s'.", linenum, filename);
	    break;
	  }

	  if ((job = _papplJobCreate(printer, (int)strtol(job_id, NULL, 10), job_username, job_format, job_name, NULL)) == NULL)
	  {
	    papplLog(system, PAPPL_LOGLEVEL_ERROR, "Error creating job %s for printer %s", job_name, printer->name);
	    break;
	  }

	  if ((job_value = cupsGetOption("filename", num_options, options)) != NULL)
	  {
	    if ((job->filename = strdup(job_value)) == NULL)
	    {
	      papplLog(system, PAPPL_LOGLEVEL_ERROR, "Error creating job %s for printer %s", job_name, printer->name);
	      break;
	    }
	  }

	  if ((job_value = cupsGetOption("state", num_options, options)) != NULL)
	    job->state = (ipp_jstate_t)strtol(job_value, NULL, 10);
	  if ((job_value = cupsGetOption("state_reasons", num_options, options)) != NULL)
	    job->state_reasons = (ipp_jstate_t)strtol(job_value, NULL, 10);
	  if ((job_value = cupsGetOption("created", num_options, options)) != NULL)
	    job->created = strtol(job_value, NULL, 10);
	  if ((job_value = cupsGetOption("processing", num_options, options)) != NULL)
	    job->processing = strtol(job_value, NULL, 10);
	  if ((job_value = cupsGetOption("completed", num_options, options)) != NULL)
	    job->completed = strtol(job_value, NULL, 10);
	  if ((job_value = cupsGetOption("impressions", num_options, options)) != NULL)
	    job->impressions = (int)strtol(job_value, NULL, 10);
	  if ((job_value = cupsGetOption("imcompleted", num_options, options)) != NULL)
	    job->impcompleted = (int)strtol(job_value, NULL, 10);

	  // Add the job to printer completed jobs array...
	  if (job->state < IPP_JSTATE_STOPPED)
	  {
	    // Load the file attributes from the spool directory...
	    int		attr_fd;	// Attribute file descriptor
	    char	job_attr_filename[256];
					// Attribute filename

	    if ((attr_fd = papplJobOpenFile(job, job_attr_filename, sizeof(job_attr_filename), system->directory, "ipp", "r")) < 0)
	    {
	      if (errno != ENOENT)
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

      // Loaded all printer attributes, call the status callback (if any) to
      // update the current printer state...
      if (printer && printer->driver_data.status_cb)
        (printer->driver_data.status_cb)(printer);
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
  cups_len_t		i, j,		// Looping vars
			count;		// Number of printers
  cups_file_t		*fp;		// Output file
  pappl_printer_t	*printer;	// Current printer
  pappl_job_t		*job;		// Current Job


  if ((fp = cupsFileOpen(filename, "w")) == NULL)
  {
    papplLog(system, PAPPL_LOGLEVEL_ERROR, "Unable to create system state file '%s': %s", filename, cupsGetErrorString());
    return (false);
  }

  papplLog(system, PAPPL_LOGLEVEL_INFO, "Saving system state to '%s'.", filename);

  _papplRWLockRead(system);

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
  cupsFilePrintf(fp, "MaxImageSize %ld %d %d\n", (long)system->max_image_size, system->max_image_width, system->max_image_height);
  cupsFilePrintf(fp, "NextPrinterID %d\n", system->next_printer_id);
  cupsFilePutConf(fp, "UUID", system->uuid);

  // Loop through the printers.
  //
  // Note: Cannot use cupsArrayGetFirst/Last since other threads might be
  // enumerating the printers array.
  for (i = 0, count = cupsArrayGetCount(system->printers); i < count; i ++)
  {
    cups_len_t		jcount;		// Number of jobs
    cups_len_t		num_options = 0;// Number of options
    cups_option_t	*options = NULL;// Options

    printer = (pappl_printer_t *)cupsArrayGetElement(system->printers, i);

    if (printer->is_deleted)
      continue;

    _papplRWLockRead(printer);

    num_options = cupsAddIntegerOption("id", printer->printer_id, num_options, &options);
    num_options = cupsAddOption("name", printer->name, num_options, &options);
    num_options = cupsAddOption("did", printer->device_id ? printer->device_id : "", num_options, &options);
    num_options = cupsAddOption("uri", printer->device_uri, num_options, &options);
    num_options = cupsAddOption("driver", printer->driver_name, num_options, &options);

    if (system->options & PAPPL_SOPTIONS_MULTI_QUEUE)
      num_options = cupsAddIntegerOption("state", (int)printer->state, num_options, &options);

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
    if (printer->hold_new_jobs)
      cupsFilePuts(fp, "HoldNewJobs\n");
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

    for (j = 0; j < (cups_len_t)printer->driver_data.num_source; j ++)
    {
      if (printer->driver_data.media_ready[j].size_name[0])
      {
        char	name[128];		// Attribute name

        snprintf(name, sizeof(name), "media-col-ready%u", (unsigned)j);
        write_media_col(fp, name, printer->driver_data.media_ready + j);
      }
    }
    if (printer->driver_data.orient_default)
      cupsFilePutConf(fp, "orientation-requested-default", ippEnumString("orientation-requested", (int)printer->driver_data.orient_default));
    if (printer->driver_data.bin_default && printer->driver_data.num_bin > 0)
      cupsFilePutConf(fp, "output-bin-default", printer->driver_data.bin[printer->driver_data.bin_default]);
    if (printer->driver_data.color_default)
      cupsFilePutConf(fp, "print-color-mode-default", _papplColorModeString(printer->driver_data.color_default));
    if (printer->driver_data.content_default)
      cupsFilePutConf(fp, "print-content-optimize-default", _papplContentString(printer->driver_data.content_default));
    if (printer->driver_data.darkness_default)
      cupsFilePrintf(fp, "print-darkness-default %d\n", printer->driver_data.darkness_default);
    if (printer->driver_data.quality_default)
      cupsFilePutConf(fp, "print-quality-default", ippEnumString("print-quality", (int)printer->driver_data.quality_default));
    if (printer->driver_data.scaling_default)
      cupsFilePutConf(fp, "print-scaling-default", _papplScalingString(printer->driver_data.scaling_default));
    if (printer->driver_data.darkness_configured)
      cupsFilePrintf(fp, "printer-darkness-configured %d\n", printer->driver_data.darkness_configured);
    if (printer->driver_data.sides_default)
      cupsFilePutConf(fp, "sides-default", _papplSidesString(printer->driver_data.sides_default));
    if (printer->driver_data.x_default)
      cupsFilePrintf(fp, "printer-resolution-default %dx%ddpi\n", printer->driver_data.x_default, printer->driver_data.y_default);
    for (j = 0; j < (cups_len_t)printer->driver_data.num_vendor; j ++)
    {
      char	defname[128],		// xxx-default name
	      	defvalue[1024];		// xxx-default value

      snprintf(defname, sizeof(defname), "%s-default", printer->driver_data.vendor[j]);
      ippAttributeString(ippFindAttribute(printer->driver_attrs, defname, IPP_TAG_ZERO), defvalue, sizeof(defvalue));

      cupsFilePutConf(fp, defname, defvalue);
    }

    // Note: Cannot use cupsArrayGetFirst/Last since other threads might be
    // enumerating the all_jobs array.

    for (j = 0, jcount = cupsArrayGetCount(printer->all_jobs); j < jcount; j ++)
    {
      job = (pappl_job_t *)cupsArrayGetElement(printer->all_jobs, j);

      _papplRWLockRead(job);

      // Add basic job attributes...
      num_options = 0;
      num_options = cupsAddIntegerOption("id", job->job_id, num_options, &options);
      num_options = cupsAddOption("name", job->name, num_options, &options);
      num_options = cupsAddOption("username", job->username, num_options, &options);
      num_options = cupsAddOption("format", job->format, num_options, &options);

      if (job->filename)
        num_options = cupsAddOption("filename", job->filename, num_options, &options);
      if (job->is_canceled)
        num_options = cupsAddIntegerOption("state", (int)IPP_JSTATE_CANCELED, num_options, &options);
      else if (job->state)
        num_options = cupsAddIntegerOption("state", (int)job->state, num_options, &options);
      if (job->state_reasons)
        num_options = cupsAddIntegerOption("state_reasons", (int)job->state_reasons, num_options, &options);
      if (job->created)
        num_options = add_time("created", job->created, num_options, &options);
      if (job->processing)
        num_options = add_time("processing", job->processing, num_options, &options);
      if (job->completed)
        num_options = add_time("completed", job->completed, num_options, &options);
      else if (job->is_canceled)
        num_options = add_time("completed", time(NULL), num_options, &options);
      if (job->impressions)
        num_options = cupsAddIntegerOption("impressions", job->impressions, num_options, &options);
      if (job->impcompleted)
        num_options = cupsAddIntegerOption("imcompleted", job->impcompleted, num_options, &options);

      if (job->attrs)
      {
	int	attr_fd;		// Attribute file descriptor
	char	job_attr_filename[1024];// Attribute filename

        // Save job attributes to file in spool directory...
        if (job->state < IPP_JSTATE_STOPPED)
        {
          if ((attr_fd = papplJobOpenFile(job, job_attr_filename, sizeof(job_attr_filename), system->directory, "ipp", "w")) < 0)
          {
            papplLog(system, PAPPL_LOGLEVEL_ERROR, "Unable to create file for job attributes: '%s'.", job_attr_filename);
            _papplRWUnlock(job);
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

      _papplRWUnlock(job);
    }

    cupsFilePuts(fp, "</Printer>\n");

    _papplRWUnlock(printer);
  }

  _papplRWUnlock(system);

  cupsFileClose(fp);

  return (true);
}


//
// 'add_time()' - Add a time_t value as an option.
//

static cups_len_t			// O  - New number of options
add_time(const char    *name,		// I  - Name
	 time_t        value,		// I  - Value
	 cups_len_t    num_options,	// I  - Number of options
	 cups_option_t **options)	// IO - Options
{
  char	buffer[100];			// Value string buffer


  // Format the number as a long integer...
  snprintf(buffer, sizeof(buffer), "%ld", (long)value);

  // Add the option wih the string...
  return (cupsAddOption(name, buffer, num_options, options));
}


//
// 'parse_contact()' - Parse a contact value.
//

static void
parse_contact(char            *value,	// I - Value
              pappl_contact_t *contact)	// O - Contact
{
  cups_len_t	i,			// Looping var
		num_options;		// Number of options
  cups_option_t	*options = NULL,	// Options
		*option;		// Current option


  memset(contact, 0, sizeof(pappl_contact_t));
  num_options = cupsParseOptions(value, 0, &options);

  for (i = num_options, option = options; i > 0; i --, option ++)
  {
    if (!strcasecmp(option->name, "name"))
      papplCopyString(contact->name, option->value, sizeof(contact->name));
    else if (!strcasecmp(option->name, "email"))
      papplCopyString(contact->email, option->value, sizeof(contact->email));
    else if (!strcasecmp(option->name, "telephone"))
      papplCopyString(contact->telephone, option->value, sizeof(contact->telephone));
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
  cups_len_t	i,			// Looping var
		num_options;		// Number of options
  cups_option_t	*options = NULL,	// Options
		*option;		// Current option


  memset(media, 0, sizeof(pappl_media_col_t));
  num_options = cupsParseOptions(value, 0, &options);

  for (i = num_options, option = options; i > 0; i --, option ++)
  {
    if (!strcasecmp(option->name, "bottom"))
      media->bottom_margin = (int)strtol(option->value, NULL, 10);
    else if (!strcasecmp(option->name, "left"))
      media->left_margin = (int)strtol(option->value, NULL, 10);
    else if (!strcasecmp(option->name, "left-offset"))
      media->left_offset = (int)strtol(option->value, NULL, 10);
    else if (!strcasecmp(option->name, "right"))
      media->right_margin = (int)strtol(option->value, NULL, 10);
    else if (!strcasecmp(option->name, "name"))
      papplCopyString(media->size_name, option->value, sizeof(media->size_name));
    else if (!strcasecmp(option->name, "width"))
      media->size_width = (int)strtol(option->value, NULL, 10);
    else if (!strcasecmp(option->name, "length"))
      media->size_length = (int)strtol(option->value, NULL, 10);
    else if (!strcasecmp(option->name, "source"))
      papplCopyString(media->source, option->value, sizeof(media->source));
    else if (!strcasecmp(option->name, "top"))
      media->top_margin = (int)strtol(option->value, NULL, 10);
    else if (!strcasecmp(option->name, "offset") || !strcasecmp(option->name, "top-offset"))
      media->top_offset = (int)strtol(option->value, NULL, 10);
    else if (!strcasecmp(option->name, "tracking"))
      media->tracking = _papplMediaTrackingValue(option->value);
    else if (!strcasecmp(option->name, "type"))
      papplCopyString(media->type, option->value, sizeof(media->type));
  }

  cupsFreeOptions(num_options, options);
}


//
// 'read_line()' - Read a line from the state file.
//
// This function is like `cupsFileGetConf`, except that it doesn't support
// comments since the state files are not meant to be edited or maintained by
// humans.
//

static char *				// O  - Line or `NULL` on EOF
read_line(cups_file_t *fp,		// I  - File
          char        *line,		// I  - Line buffer
          size_t      linesize,		// I  - Size of line buffer
          char        **value,		// O  - Value portion of line
          int         *linenum)		// IO - Current line number
{
  char	*ptr;				// Pointer into line


  // Try reading a line from the file...
  *value = NULL;

  if (!cupsFileGets(fp, line, linesize))
    return (NULL);

  // Got it, bump the line number...
  (*linenum) ++;

  // If we have "something value" then split at the whitespace...
  if ((ptr = strchr(line, ' ')) != NULL)
  {
    *ptr++ = '\0';
    *value = ptr;
  }

  // Strip the trailing ">" for "<something value(s)>"
  if (line[0] == '<' && *value && (ptr = *value + strlen(*value) - 1) >= *value && *ptr == '>')
    *ptr = '\0';

  return (line);
}


//
// 'write_contact()' - Write an "xxx-contact" value.
//

static void
write_contact(cups_file_t     *fp,	// I - File
              pappl_contact_t *contact)	// I - Contact
{
  cups_len_t	num_options = 0;	// Number of options
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
  cups_len_t	num_options = 0;	// Number of options
  cups_option_t	*options = NULL;	// Options


  if (media->bottom_margin)
    num_options = cupsAddIntegerOption("bottom", media->bottom_margin, num_options, &options);
  if (media->left_margin)
    num_options = cupsAddIntegerOption("left", media->left_margin, num_options, &options);
  if (media->left_offset)
    num_options = cupsAddIntegerOption("left-offset", media->left_offset, num_options, &options);
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
    num_options = cupsAddIntegerOption("top-offset", media->top_offset, num_options, &options);
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
              cups_len_t    num_options,// I - Number of options
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
          cupsFileWrite(fp, start, (size_t)(ptr - start));

	cupsFilePutChar(fp, '\\');
	start = ptr;
      }
    }

    if (ptr > start)
      cupsFileWrite(fp, start, (size_t)(ptr - start));

    cupsFilePutChar(fp, '\"');

    num_options --;
    options ++;
  }

  if (*name == '<')
    cupsFilePuts(fp, ">\n");
  else
    cupsFilePutChar(fp, '\n');
}
