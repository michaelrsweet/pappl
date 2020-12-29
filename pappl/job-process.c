//
// Job processing (printing) functions for the Printer Application Framework
//
// Copyright © 2019-2020 by Michael R Sweet.
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

static const char *cups_cspace_string(cups_cspace_t cspace);
static bool	filter_raw(pappl_job_t *job, pappl_device_t *device);
static void	finish_job(pappl_job_t *job);
static void	start_job(pappl_job_t *job);


//
// 'papplJobCreatePrintOptions()' - Create the printer options for a job.
//
// This function allocates a printer options stucture and computes the print
// options for a job based upon the Job Template attributes submitted in the
// print request and the default values set in the printer driver data.
//
// The "num_pages" and "color" arguments specify the number of pages and whether
// the document contains non-grayscale colors - this information typically comes
// from parsing the job file.
//

pappl_pr_options_t *			// O - Job options data or `NULL` on error
papplJobCreatePrintOptions(
    pappl_job_t      *job,		// I - Job
    unsigned         num_pages,		// I - Number of pages (`0` for unknown)
    bool             color)		// I - Is the document in color?
{
  pappl_pr_options_t	*options;	// New options data
  int			i,		// Looping var
			count;		// Number of values
  ipp_attribute_t	*attr;		// Attribute
  pappl_printer_t	*printer = job->printer;
					// Printer
  const char		*raster_type;	// Raster type for output
  static const char * const sheet_back[] =
  {					// "pwg-raster-document-sheet-back values
    "normal",
    "flipped",
    "rotated",
    "manual-tumble"
  };


  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "Getting options for num_pages=%u, color=%s", num_pages, color ? "true" : "false");

  // Clear all options...
  if ((options = calloc(1, sizeof(pappl_pr_options_t))) == NULL)
    return (NULL);

  options->media = printer->driver_data.media_default;

  pthread_rwlock_rdlock(&printer->rwlock);

  // copies
  if ((attr = ippFindAttribute(job->attrs, "copies", IPP_TAG_INTEGER)) != NULL)
    options->copies = ippGetInteger(attr, 0);
  else
    options->copies = 1;

  // finishings
  options->finishings = PAPPL_FINISHINGS_NONE;

  if ((attr = ippFindAttribute(job->attrs, "finishings", IPP_TAG_ENUM)) != NULL)
  {
    if (ippContainsInteger(attr, IPP_FINISHINGS_PUNCH))
      options->finishings |= PAPPL_FINISHINGS_PUNCH;
    if (ippContainsInteger(attr, IPP_FINISHINGS_STAPLE))
      options->finishings |= PAPPL_FINISHINGS_STAPLE;
    if (ippContainsInteger(attr, IPP_FINISHINGS_TRIM))
      options->finishings |= PAPPL_FINISHINGS_TRIM;
  }
  else if ((attr = ippFindAttribute(job->attrs, "finishings-col", IPP_TAG_BEGIN_COLLECTION)) != NULL)
  {
    for (i = 0, count = ippGetCount(attr); i < count; i ++)
    {
      ipp_t *col = ippGetCollection(attr, i);
					// "finishings-col" collection value
      const char *template = ippGetString(ippFindAttribute(col, "finishing-template", IPP_TAG_ZERO), 0, NULL);
					// "finishing-template" value

      if (template && !strcmp(template, "punch"))
        options->finishings |= PAPPL_FINISHINGS_PUNCH;
      else if (template && !strcmp(template, "staple"))
        options->finishings |= PAPPL_FINISHINGS_STAPLE;
      else if (template && !strcmp(template, "trim"))
        options->finishings |= PAPPL_FINISHINGS_TRIM;
    }
  }

  // media-xxx
  options->media = printer->driver_data.media_default;

  if ((attr = ippFindAttribute(job->attrs, "media-col", IPP_TAG_BEGIN_COLLECTION)) != NULL)
  {
    options->media.source[0] = '\0';

    _papplMediaColImport(ippGetCollection(attr, 0), &options->media);
  }
  else if ((attr = ippFindAttribute(job->attrs, "media", IPP_TAG_ZERO)) != NULL)
  {
    const char	*pwg_name = ippGetString(attr, 0, NULL);
    pwg_media_t	*pwg_media = pwgMediaForPWG(pwg_name);

    if (pwg_name && pwg_media)
    {
      strlcpy(options->media.size_name, pwg_name, sizeof(options->media.size_name));
      options->media.size_width  = pwg_media->width;
      options->media.size_length = pwg_media->length;
      options->media.source[0]   = '\0';
    }
  }

  if (!options->media.source[0])
  {
    for (i = 0; i < printer->driver_data.num_source; i ++)
    {
      if (!strcmp(options->media.size_name, printer->driver_data.media_ready[i].size_name))
      {
        strlcpy(options->media.source, printer->driver_data.source[i], sizeof(options->media.source));
        break;
      }
    }

    if (!options->media.source[0])
      strlcpy(options->media.source, printer->driver_data.media_default.source, sizeof(options->media.source));
  }

  // orientation-requested
  if ((attr = ippFindAttribute(job->attrs, "orientation-requested", IPP_TAG_ENUM)) != NULL)
    options->orientation_requested = (ipp_orient_t)ippGetInteger(attr, 0);
  else if (printer->driver_data.orient_default)
    options->orientation_requested = printer->driver_data.orient_default;
  else
    options->orientation_requested = IPP_ORIENT_NONE;

  // output-bin
  if (printer->driver_data.num_bin > 0)
  {
    const char		*value;		// Attribute string value

    if ((value = ippGetString(ippFindAttribute(job->attrs, "output-bin", IPP_TAG_ZERO), 0, NULL)) != NULL)
      strlcpy(options->output_bin, value, sizeof(options->output_bin));
    else
      strlcpy(options->output_bin, printer->driver_data.bin[printer->driver_data.bin_default], sizeof(options->output_bin));
  }

  // page-ranges
  if ((attr = ippFindAttribute(job->attrs, "page-ranges", IPP_TAG_RANGE)) != NULL && ippGetCount(attr) == 1)
  {
    int	last, first = ippGetRange(attr, 0, &last);
					// pages-ranges values

    if (first > (int)num_pages && num_pages != 0)
    {
      options->first_page = num_pages + 1;
      options->last_page  = num_pages + 1;
      options->num_pages  = 0;
    }
    else
    {
      options->first_page = (unsigned)first;

      if (last > (int)num_pages && num_pages != 0)
        options->last_page = num_pages;
      else
        options->last_page = (unsigned)last;

      options->num_pages = options->last_page - options->first_page + 1;
    }
  }
  else if (num_pages > 0)
  {
    options->first_page = 1;
    options->last_page  = num_pages;
    options->num_pages  = num_pages;
  }
  else
  {
    // Unknown number of pages...
    options->first_page = 1;
    options->last_page  = INT_MAX;
    options->num_pages  = 0;
  }

  // print-color-mode
  if ((attr = ippFindAttribute(job->attrs, "print-color-mode", IPP_TAG_KEYWORD)) != NULL)
    options->print_color_mode = _papplColorModeValue(ippGetString(attr, 0, NULL));
  else
    options->print_color_mode = printer->driver_data.color_default;

  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "print-color-mode=%s", _papplColorModeString(options->print_color_mode));

  if (options->print_color_mode == PAPPL_COLOR_MODE_AUTO)
  {
    if (color)
      options->print_color_mode = PAPPL_COLOR_MODE_COLOR;
    else
      options->print_color_mode = PAPPL_COLOR_MODE_MONOCHROME;

    papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "new print-color-mode=%s", _papplColorModeString(options->print_color_mode));
  }
  else if (options->print_color_mode == PAPPL_COLOR_MODE_AUTO_MONOCHROME)
  {
    options->print_color_mode = PAPPL_COLOR_MODE_MONOCHROME;

    papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "new print-color-mode=%s", _papplColorModeString(options->print_color_mode));
  }

  // print-content-optimize
  if ((attr = ippFindAttribute(job->attrs, "print-content-optimize", IPP_TAG_KEYWORD)) != NULL)
    options->print_content_optimize = _papplContentValue(ippGetString(attr, 0, NULL));
  else
    options->print_content_optimize = printer->driver_data.content_default;

  if (printer->driver_data.force_raster_type == PAPPL_PWG_RASTER_TYPE_BLACK_1)
  {
    // Force bitmap output...
    raster_type = "black_1";

    if (options->print_color_mode == PAPPL_COLOR_MODE_BI_LEVEL)
      memset(options->dither, 127, sizeof(options->dither));
    else if (options->print_content_optimize == PAPPL_CONTENT_PHOTO || !strcmp(job->format, "image/jpeg"))
      memcpy(options->dither, printer->driver_data.pdither, sizeof(options->dither));
    else
      memcpy(options->dither, printer->driver_data.gdither, sizeof(options->dither));
  }
  else if (options->print_color_mode == PAPPL_COLOR_MODE_COLOR)
  {
    // Color output...
    if (printer->driver_data.raster_types & PAPPL_PWG_RASTER_TYPE_SRGB_8)
      raster_type = "srgb_8";
    else if (printer->driver_data.raster_types & PAPPL_PWG_RASTER_TYPE_ADOBE_RGB_8)
      raster_type = "adobe-rgb_8";
    else
      raster_type = "rgb_8";
  }
  else
  {
    // Monochrome output...
    if (printer->driver_data.raster_types & PAPPL_PWG_RASTER_TYPE_SGRAY_8)
      raster_type = "sgray_8";
    else
      raster_type = "black_8";
  }

  // print-darkness
  if ((attr = ippFindAttribute(job->attrs, "print-darkness", IPP_TAG_INTEGER)) != NULL)
    options->print_darkness = ippGetInteger(attr, 0);
  else
    options->print_darkness = printer->driver_data.darkness_default;

  options->darkness_configured = printer->driver_data.darkness_configured;

  // print-quality
  if ((attr = ippFindAttribute(job->attrs, "print-quality", IPP_TAG_ENUM)) != NULL)
    options->print_quality = (ipp_quality_t)ippGetInteger(attr, 0);
  else
    options->print_quality = printer->driver_data.quality_default;

  // print-scaling
  if ((attr = ippFindAttribute(job->attrs, "print-scaling", IPP_TAG_KEYWORD)) != NULL)
    options->print_scaling = _papplScalingValue(ippGetString(attr, 0, NULL));
  else
    options->print_scaling = printer->driver_data.scaling_default;

  // print-speed
  if ((attr = ippFindAttribute(job->attrs, "print-speed", IPP_TAG_INTEGER)) != NULL)
    options->print_speed = ippGetInteger(attr, 0);
  else
    options->print_speed = printer->driver_data.speed_default;

  // printer-resolution
  if ((attr = ippFindAttribute(job->attrs, "printer-resolution", IPP_TAG_RESOLUTION)) != NULL)
  {
    ipp_res_t	units;			// Resolution units

    options->printer_resolution[0] = ippGetResolution(attr, 0, options->printer_resolution + 1, &units);
  }
  else if (options->print_quality == IPP_QUALITY_DRAFT)
  {
    // print-quality=draft
    options->printer_resolution[0] = printer->driver_data.x_resolution[0];
    options->printer_resolution[1] = printer->driver_data.y_resolution[0];
  }
  else if (options->print_quality == IPP_QUALITY_NORMAL)
  {
    // print-quality=normal
    i = printer->driver_data.num_resolution / 2;
    options->printer_resolution[0] = printer->driver_data.x_resolution[i];
    options->printer_resolution[1] = printer->driver_data.y_resolution[i];
  }
  else
  {
    // print-quality=high
    i = printer->driver_data.num_resolution - 1;
    options->printer_resolution[0] = printer->driver_data.x_resolution[i];
    options->printer_resolution[1] = printer->driver_data.y_resolution[i];
  }

  // sides
  if ((attr = ippFindAttribute(job->attrs, "sides", IPP_TAG_KEYWORD)) != NULL)
    options->sides = _papplSidesValue(ippGetString(attr, 0, NULL));
  else if (printer->driver_data.sides_default != PAPPL_SIDES_ONE_SIDED && options->num_pages != 1)
    options->sides = printer->driver_data.sides_default;
  else
    options->sides = PAPPL_SIDES_ONE_SIDED;

  // Vendor options...
  for (i = 0; i < printer->driver_data.num_vendor; i ++)
  {
    const char *name = printer->driver_data.vendor[i];
					// Vendor attribute name

    if ((attr = ippFindAttribute(job->attrs, name, IPP_TAG_ZERO)) == NULL)
    {
      char	defname[128];		// xxx-default attribute

      snprintf(defname, sizeof(defname), "%s-default", name);
      attr = ippFindAttribute(job->attrs, defname, IPP_TAG_ZERO);
    }

    if (attr)
    {
      char	value[1024];		// Value of attribute

      ippAttributeString(attr, value, sizeof(value));
      options->num_vendor = cupsAddOption(name, value, options->num_vendor, &options->vendor);
    }
  }

  // Figure out the PWG raster header...
  cupsRasterInitPWGHeader(&options->header, pwgMediaForPWG(options->media.size_name), raster_type, options->printer_resolution[0], options->printer_resolution[1], _papplSidesString(options->sides), sheet_back[printer->driver_data.duplex]);

  options->header.cupsInteger[CUPS_RASTER_PWG_TotalPageCount] = (unsigned)options->copies * options->num_pages;

  // Log options...
  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "header.cupsWidth=%u", options->header.cupsWidth);
  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "header.cupsHeight=%u", options->header.cupsHeight);
  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "header.cupsBitsPerColor=%u", options->header.cupsBitsPerColor);
  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "header.cupsBitsPerPixel=%u", options->header.cupsBitsPerPixel);
  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "header.cupsBytesPerLine=%u", options->header.cupsBytesPerLine);
  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "header.cupsColorOrder=%u", options->header.cupsColorOrder);
  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "header.cupsColorSpace=%u (%s)", options->header.cupsColorSpace, cups_cspace_string(options->header.cupsColorSpace));
  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "header.cupsNumColors=%u", options->header.cupsNumColors);
  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "header.HWResolution=[%u %u]", options->header.HWResolution[0], options->header.HWResolution[1]);

  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "num_pages=%u", options->num_pages);
  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "copies=%d", options->copies);
  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "finishings=0x%x", options->finishings);
  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "media-col.bottom-margin=%d", options->media.bottom_margin);
  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "media-col.left-margin=%d", options->media.left_margin);
  if (printer->driver_data.left_offset_supported[1])
    papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "media-col.left-offset=%d", options->media.left_offset);
  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "media-col.right-margin=%d", options->media.right_margin);
  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "media-col.size=%dx%d", options->media.size_width, options->media.size_length);
  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "media-col.size-name='%s'", options->media.size_name);
  if (printer->driver_data.num_source)
    papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "media-col.source='%s'", options->media.source);
  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "media-col.top-margin=%d", options->media.top_margin);
  if (printer->driver_data.top_offset_supported[1])
    papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "media-col.top-offset=%d", options->media.top_offset);
  if (printer->driver_data.tracking_supported)
    papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "media-col.tracking='%s'", _papplMediaTrackingString(options->media.tracking));
  if (printer->driver_data.num_type)
    papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "media-col.type='%s'", options->media.type);
  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "orientation-requested=%s", ippEnumString("orientation-requested", (int)options->orientation_requested));
  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "page-ranges=%u-%u", options->first_page, options->last_page);
  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "print-color-mode='%s'", _papplColorModeString(options->print_color_mode));
  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "print-content-optimize='%s'", _papplContentString(options->print_content_optimize));
  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "print-darkness=%d", options->print_darkness);
  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "print-quality=%s", ippEnumString("print-quality", (int)options->print_quality));
  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "print-scaling='%s'", _papplScalingString(options->print_scaling));
  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "print-speed=%d", options->print_speed);
  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "printer-resolution=%dx%ddpi", options->printer_resolution[0], options->printer_resolution[1]);

  for (i = 0; i < options->num_vendor; i ++)
    papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "%s=%s", options->vendor[i].name, options->vendor[i].value);

  pthread_rwlock_unlock(&printer->rwlock);

  return (options);
}


//
// 'papplJobDeletePrintOptions()' - Delete a job options structure.
//
// This function frees the memory used for a job options structure.
//

void
papplJobDeletePrintOptions(
    pappl_pr_options_t *options)		// I - Options
{
  if (options)
  {
    cupsFreeOptions(options->num_vendor, options->vendor);
    free(options);
  }
}


//
// '_papplJobProcess()' - Process a print job.
//

void *					// O - Thread exit status
_papplJobProcess(pappl_job_t *job)	// I - Job
{
  _pappl_mime_filter_t	*filter;	// Filter for printing


  // Start processing the job...
  start_job(job);

  // Do file-specific conversions...
  if ((filter = _papplSystemFindMIMEFilter(job->system, job->format, job->printer->driver_data.format)) == NULL)
    filter =_papplSystemFindMIMEFilter(job->system, job->format, "image/pwg-raster");

  if (filter)
  {
    if (!(filter->cb)(job, job->printer->device, filter->cbdata))
      job->state = IPP_JSTATE_ABORTED;
  }
  else if (!strcmp(job->format, job->printer->driver_data.format))
  {
    if (!filter_raw(job, job->printer->device))
      job->state = IPP_JSTATE_ABORTED;
  }
  else
  {
    // Abort a job we can't process...
    papplLogJob(job, PAPPL_LOGLEVEL_ERROR, "Unable to process job with format '%s'.", job->format);
    job->state = IPP_JSTATE_ABORTED;
  }

  // Move the job to a completed state...
  finish_job(job);

  return (NULL);
}


//
// '_papplJobProcessRaster()' - Process an Apple/PWG Raster file.
//

void
_papplJobProcessRaster(
    pappl_job_t    *job,		// I - Job
    pappl_client_t *client)		// I - Client
{
  pappl_printer_t	*printer = job->printer;
					// Printer for job
  pappl_pr_options_t	*options = NULL;// Job options
  cups_raster_t		*ras = NULL;	// Raster stream
  cups_page_header2_t	header;		// Page header
  unsigned		header_pages;	// Number of pages from page header
  const unsigned char	*dither;	// Dither line
  unsigned char		*pixels,	// Incoming pixel line
			*pixptr,	// Pixel pointer in line
			*line,		// Output (bitmap) line
			*lineptr,	// Pointer in line
			byte,		// Byte in line
			bit;		// Current bit
  unsigned		page = 0,	// Current page
			x,		// Current column
			y;		// Current line


  // Start processing the job...
  job->streaming = true;

  start_job(job);

  // Open the raster stream...
  if ((ras = cupsRasterOpenIO((cups_raster_iocb_t)httpRead2, client->http, CUPS_RASTER_READ)) == NULL)
  {
    papplLogJob(job, PAPPL_LOGLEVEL_ERROR, "Unable to open raster stream from client - %s", cupsLastErrorString());
    job->state = IPP_JSTATE_ABORTED;
    goto complete_job;
  }

  // Prepare options...
  if (!cupsRasterReadHeader2(ras, &header))
  {
    papplLogJob(job, PAPPL_LOGLEVEL_ERROR, "Unable to read raster stream from client - %s", cupsLastErrorString());
    job->state = IPP_JSTATE_ABORTED;
    goto complete_job;
  }

  if ((header_pages = header.cupsInteger[CUPS_RASTER_PWG_TotalPageCount]) > 0)
    papplJobSetImpressions(job, (int)header.cupsInteger[CUPS_RASTER_PWG_TotalPageCount]);

  options = papplJobCreatePrintOptions(job, (unsigned)job->impressions, header.cupsBitsPerPixel > 8);

  if (!(printer->driver_data.rstartjob_cb)(job, options, job->printer->device))
  {
    job->state = IPP_JSTATE_ABORTED;
    goto complete_job;
  }

  // Print pages...
  do
  {
    if (job->is_canceled)
      break;

    page ++;
    papplJobSetImpressionsCompleted(job, 1);

    papplLogJob(job, PAPPL_LOGLEVEL_INFO, "Page %u raster data is %ux%ux%u (%s)", page, header.cupsWidth, header.cupsHeight, header.cupsBitsPerPixel, cups_cspace_string(header.cupsColorSpace));

    // Set options for this page...
    papplJobDeletePrintOptions(options);
    options = papplJobCreatePrintOptions(job, (unsigned)job->impressions, header.cupsBitsPerPixel > 8);

    if (header.cupsWidth == 0 || header.cupsHeight == 0 || (header.cupsBitsPerColor != 1 && header.cupsBitsPerColor != 8) || header.cupsColorOrder != CUPS_ORDER_CHUNKED || (header.cupsBytesPerLine != ((header.cupsWidth * header.cupsBitsPerPixel + 7) / 8)))
    {
      papplLogJob(job, PAPPL_LOGLEVEL_ERROR, "Bad raster data seen.");
      papplJobSetReasons(job, PAPPL_JREASON_DOCUMENT_FORMAT_ERROR, PAPPL_JREASON_NONE);
      job->state = IPP_JSTATE_ABORTED;
      break;
    }

    if (header.cupsBitsPerPixel > 8 && !(printer->driver_data.color_supported & PAPPL_COLOR_MODE_COLOR))
    {
      papplLogJob(job, PAPPL_LOGLEVEL_ERROR, "Unsupported raster data seen.");
      papplJobSetReasons(job, PAPPL_JREASON_DOCUMENT_UNPRINTABLE_ERROR, PAPPL_JREASON_NONE);
      job->state = IPP_JSTATE_ABORTED;
      break;
    }

    if (options->header.cupsBitsPerPixel >= 8 && header.cupsBitsPerPixel >= 8)
      options->header = header;		// Use page header from client

    if (!(printer->driver_data.rstartpage_cb)(job, options, job->printer->device, page))
    {
      job->state = IPP_JSTATE_ABORTED;
      break;
    }

    if (options->header.cupsBytesPerLine > header.cupsBytesPerLine)
    {
      // Allocate enough space for the entire output line and clear to white
      if ((pixels = malloc(options->header.cupsBytesPerLine)) == NULL)
      {
        papplLogJob(job, PAPPL_LOGLEVEL_ERROR, "Unable to allocate raster line.");
	job->state = IPP_JSTATE_ABORTED;
	break;
      }

      if (options->header.cupsColorSpace == CUPS_CSPACE_K)
        memset(pixels, 0, options->header.cupsBytesPerLine);
      else
        memset(pixels, 255, options->header.cupsBytesPerLine);
    }
    else
    {
      // The input raster is at least as wide as the output raster...
      if ((pixels = malloc(header.cupsBytesPerLine)) == NULL)
      {
        papplLogJob(job, PAPPL_LOGLEVEL_ERROR, "Unable to allocate raster line.");
	job->state = IPP_JSTATE_ABORTED;
	break;
      }
    }

    if ((line = malloc(options->header.cupsBytesPerLine)) == NULL)
    {
      free(pixels);

      papplLogJob(job, PAPPL_LOGLEVEL_ERROR, "Unable to allocate raster line.");
      job->state = IPP_JSTATE_ABORTED;
      break;
    }

    for (y = 0; !job->is_canceled && y < header.cupsHeight && y < options->header.cupsHeight; y ++)
    {
      if (cupsRasterReadPixels(ras, pixels, header.cupsBytesPerLine))
      {
        if (header.cupsBitsPerPixel == 8 && options->header.cupsBitsPerPixel == 1)
        {
          // Dither the line...
	  dither = options->dither[y & 15];
	  memset(line, 0, options->header.cupsBytesPerLine);

          if (header.cupsColorSpace == CUPS_CSPACE_K)
          {
            // Black...
	    for (x = 0, lineptr = line, pixptr = pixels, bit = 128, byte = 0; x < header.cupsWidth; x ++, pixptr ++)
	    {
	      if (*pixptr > dither[x & 15])
	        byte |= bit;

	      if (bit == 1)
	      {
	        *lineptr++ = byte;
	        byte       = 0;
	        bit        = 128;
	      }
	      else
	        bit /= 2;
	    }

	    if (bit < 128)
	      *lineptr = byte;
	  }
	  else
	  {
	    // Grayscale to black...
	    for (x = 0, lineptr = line, pixptr = pixels, bit = 128, byte = 0; x < header.cupsWidth; x ++, pixptr ++)
	    {
	      if (*pixptr <= dither[x & 15])
	        byte |= bit;

	      if (bit == 1)
	      {
	        *lineptr++ = byte;
	        byte       = 0;
	        bit        = 128;
	      }
	      else
	        bit /= 2;
	    }

	    if (bit < 128)
	      *lineptr = byte;
	  }

          (printer->driver_data.rwriteline_cb)(job, options, job->printer->device, y, line);
        }
        else
          (printer->driver_data.rwriteline_cb)(job, options, job->printer->device, y, pixels);
      }
      else
        break;
    }

    if (!job->is_canceled && y < header.cupsHeight)
    {
      // Discard excess lines from client...
      while (y < header.cupsHeight)
      {
        cupsRasterReadPixels(ras, pixels, header.cupsBytesPerLine);
        y ++;
      }
    }
    else
    {
      // Pad missing lines with whitespace...
      if (header.cupsBitsPerPixel == 8 && options->header.cupsBitsPerPixel == 1)
      {
        memset(line, 0, options->header.cupsBytesPerLine);

        while (y < options->header.cupsHeight)
        {
	  (printer->driver_data.rwriteline_cb)(job, options, job->printer->device, y, line);
          y ++;
        }
      }
      else
      {
        if (header.cupsColorSpace == CUPS_CSPACE_K || header.cupsColorSpace == CUPS_CSPACE_CMYK)
          memset(pixels, 0x00, header.cupsBytesPerLine);
	else
          memset(pixels, 0xff, header.cupsBytesPerLine);

        while (y < options->header.cupsHeight)
        {
	  (printer->driver_data.rwriteline_cb)(job, options, job->printer->device, y, pixels);
          y ++;
        }
      }
    }

    free(pixels);
    free(line);

    if (!(printer->driver_data.rendpage_cb)(job, options, job->printer->device, page))
    {
      job->state = IPP_JSTATE_ABORTED;
      break;
    }

    if (job->is_canceled)
      break;
    else if (y < header.cupsHeight)
    {
      papplLogJob(job, PAPPL_LOGLEVEL_ERROR, "Unable to read page from raster stream from client - %s", cupsLastErrorString());
      job->state = IPP_JSTATE_ABORTED;
      break;
    }
  }
  while (cupsRasterReadHeader2(ras, &header));

  if (!(printer->driver_data.rendjob_cb)(job, options, job->printer->device))
    job->state = IPP_JSTATE_ABORTED;
  else if (header_pages == 0)
    papplJobSetImpressions(job, (int)page);

  complete_job:

  papplJobDeletePrintOptions(options);

  if (httpGetState(client->http) == HTTP_STATE_POST_RECV)
  {
    // Flush excess data...
    char	buffer[8192];		// Read buffer

    while (httpRead2(client->http, buffer, sizeof(buffer)) > 0)
      ;				// Read all document data
  }

  cupsRasterClose(ras);

  finish_job(job);
  return;
}


//
// 'cups_cspace_string()' - Get a string corresponding to a cupsColorSpace enum value.
//

static const char *			// O - cupsColorSpace string value
cups_cspace_string(
    cups_cspace_t value)		// I - cupsColorSpace enum value
{
  static const char * const cspace[] =	// cupsColorSpace values
  {
    "Gray",
    "RGB",
    "RGBA",
    "Black",
    "CMY",
    "YMC",
    "CMYK",
    "YMCK",
    "KCMY",
    "KCMYcm",
    "GMCK",
    "GMCS",
    "White",
    "Gold",
    "Silver",
    "CIE-XYZ",
    "CIE-Lab",
    "RGBW",
    "sGray",
    "sRGB",
    "Adobe-RGB",
    "21",
    "22",
    "23",
    "24",
    "25",
    "26",
    "27",
    "28",
    "29",
    "30",
    "31",
    "ICC-1",
    "ICC-2",
    "ICC-3",
    "ICC-4",
    "ICC-5",
    "ICC-6",
    "ICC-7",
    "ICC-8",
    "ICC-9",
    "ICC-10",
    "ICC-11",
    "ICC-12",
    "ICC-13",
    "ICC-14",
    "ICC-15",
    "47",
    "Device-1",
    "Device-2",
    "Device-3",
    "Device-4",
    "Device-5",
    "Device-6",
    "Device-7",
    "Device-8",
    "Device-9",
    "Device-10",
    "Device-11",
    "Device-12",
    "Device-13",
    "Device-14",
    "Device-15"
  };


  if (value >= CUPS_CSPACE_W && value <= CUPS_CSPACE_DEVICEF)
    return (cspace[value]);
  else
    return ("Unknown");
}


//
// 'filter_raw()' - "Filter" a raw print file.
//

static bool				// O - `true` on success, `false` otherwise
filter_raw(pappl_job_t    *job,		// I - Job
           pappl_device_t *device)	// I - Device
{
  pappl_pr_options_t	*options;	// Job options


  papplJobSetImpressions(job, 1);
  options = papplJobCreatePrintOptions(job, 1, false);

  if (!(job->printer->driver_data.printfile_cb)(job, options, device))
  {
    papplJobDeletePrintOptions(options);
    return (false);
  }

  papplJobDeletePrintOptions(options);
  papplJobSetImpressionsCompleted(job, 1);

  return (true);
}


//
// 'finish_job()' - Finish job processing...
//

static void
finish_job(pappl_job_t  *job)		// I - Job
{
  pappl_printer_t *printer = job->printer;
					// Printer


  pthread_rwlock_wrlock(&job->rwlock);
  pthread_rwlock_wrlock(&printer->rwlock);

  if (job->is_canceled)
    job->state = IPP_JSTATE_CANCELED;
  else if (job->state == IPP_JSTATE_PROCESSING)
    job->state = IPP_JSTATE_COMPLETED;

  papplLogJob(job, PAPPL_LOGLEVEL_INFO, "%s, job-impressions-completed=%d.", job->state == IPP_JSTATE_COMPLETED ? "Completed" : job->state == IPP_JSTATE_CANCELED ? "Canceled" : "Aborted", job->impcompleted);

  job->completed          = time(NULL);
  printer->processing_job = NULL;

  _papplJobRemoveFile(job);

  pthread_rwlock_unlock(&job->rwlock);

  if (printer->is_stopped)
  {
    // New printer-state is 'stopped'...
    printer->state      = IPP_PSTATE_STOPPED;
    printer->is_stopped = false;
  }
  else
  {
    // New printer-state is 'idle'...
    printer->state = IPP_PSTATE_IDLE;
  }

  printer->state_time = time(NULL);

  cupsArrayRemove(printer->active_jobs, job);
  cupsArrayAdd(printer->completed_jobs, job);

  printer->impcompleted += job->impcompleted;

  if (!job->system->clean_time)
    job->system->clean_time = time(NULL) + 60;

  pthread_rwlock_unlock(&printer->rwlock);

  _papplSystemConfigChanged(printer->system);

  if (printer->is_deleted)
  {
    papplPrinterDelete(printer);
  }
  else if (cupsArrayCount(printer->active_jobs) > 0)
  {
    _papplPrinterCheckJobs(printer);
  }
  else
  {
    pappl_devmetrics_t	metrics;	// Metrics for device IO

    pthread_rwlock_wrlock(&printer->rwlock);

    papplDeviceGetMetrics(printer->device, &metrics);
    papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "Device read metrics: %lu requests, %lu bytes, %lu msecs", metrics.read_requests, metrics.read_bytes, metrics.read_msecs);
    papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "Device write metrics: %lu requests, %lu bytes, %lu msecs", metrics.write_requests, metrics.write_bytes, metrics.write_msecs);

    papplDeviceClose(printer->device);
    printer->device = NULL;

    pthread_rwlock_unlock(&printer->rwlock);
  }
}


//
// 'start_job()' - Start processing a job...
//

static void
start_job(pappl_job_t *job)		// I - Job
{
  pappl_printer_t *printer = job->printer;
					// Printer
  bool	first_open = true;		// Is this the first time we try to open the device?


  // Move the job to the 'processing' state...
  pthread_rwlock_wrlock(&job->rwlock);
  pthread_rwlock_wrlock(&printer->rwlock);

  papplLogJob(job, PAPPL_LOGLEVEL_INFO, "Starting print job.");

  job->state              = IPP_JSTATE_PROCESSING;
  job->processing         = time(NULL);
  printer->processing_job = job;

  pthread_rwlock_wrlock(&job->rwlock);

  // Open the output device...
  while (!printer->device)
  {
    printer->device = papplDeviceOpen(printer->device_uri, job->name, papplLogDevice, job->system);

    if (!printer->device)
    {
      // Log that the printer is unavailable then sleep for 5 seconds to retry.
      if (first_open)
      {
        papplLogPrinter(printer, PAPPL_LOGLEVEL_ERROR, "Unable to open device '%s', pausing queue until printer becomes available.", printer->device_uri);
        first_open = false;

	printer->state      = IPP_PSTATE_STOPPED;
	printer->state_time = time(NULL);
      }

      sleep(5);
    }
  }

  // Move the printer to the 'processing' state...
  printer->state      = IPP_PSTATE_PROCESSING;
  printer->state_time = time(NULL);

  pthread_rwlock_unlock(&printer->rwlock);
}
