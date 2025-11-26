//
// Job processing (printing) functions for the Printer Application Framework
//
// Copyright © 2019-2025 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#include "pappl-private.h"


//
// Local functions...
//

static const char *cups_cspace_string(cups_cspace_t cspace);
static bool	filter_raw(pappl_job_t *job, int doc_number, pappl_pr_options_t *options, pappl_device_t *device);
static void	finish_job(pappl_job_t *job);
static bool	start_job(pappl_job_t *job);


//
// 'papplJobCreatePrintOptions()' - Create the printer options for a job.
//
// This function allocates a printer options structure and computes the print
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
    int              doc_number,	// I - Document number (`1` based)
    unsigned         num_pages,		// I - Number of pages (`0` for unknown)
    bool             color)		// I - Is the document in color?
{
  _pappl_doc_t		*doc;		// Document
  pappl_pr_options_t	*options;	// New options data
  size_t		i,		// Looping var
			count;		// Number of values
  ipp_attribute_t	*attr;		// Attribute
  pappl_printer_t	*printer = job->printer;
					// Printer
  cups_media_t		media;		// CUPS media value
  const char		*raster_type,	// Raster type for output
			*mono_type;	// Raster type for monochrome output
  static const char * const sheet_back[] =
  {					// "pwg-raster-document-sheet-back values
    "normal",
    "flipped",
    "rotated",
    "manual-tumble"
  };


  // Range check input...
  if (!job || doc_number < 0 || doc_number > job->num_documents)
    return (NULL);

  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "Getting options for num_pages=%u, color=%s", num_pages, color ? "true" : "false");

  // Clear all options...
  if ((options = calloc(1, sizeof(pappl_pr_options_t))) == NULL)
    return (NULL);

  if (doc_number)
    doc = job->documents + doc_number - 1;
  else
    doc = NULL;

  _papplRWLockRead(printer);

  // multiple-document-handling
  if ((attr = ippFindAttribute(job->attrs, "multiple-document-handling", IPP_TAG_KEYWORD)) != NULL)
    options->handling = _papplHandlingValue(ippGetString(attr, 0, NULL));
  else
    options->handling = printer->driver_data.handling_default;

  // copies
  if (options->handling != PAPPL_HANDLING_UNCOLLATED_COPIES)
  {
    // Collated copies are stored in the top-level options...
    options->copies = job->copies;
  }
  else
  {
    // Uncollated copies are stored in the raster header, so just 1 copy...
    options->copies = 1;
  }


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

  if (!doc || (attr = ippFindAttribute(doc->attrs, "media-col", IPP_TAG_BEGIN_COLLECTION)) == NULL)
    attr = ippFindAttribute(job->attrs, "media-col", IPP_TAG_BEGIN_COLLECTION);

  if (attr != NULL)
  {
    options->media.source[0] = '\0';

    _papplMediaColImport(ippGetCollection(attr, 0), &options->media);
  }
  else
  {
    if (!doc || (attr = ippFindAttribute(doc->attrs, "media", IPP_TAG_ZERO)) == NULL)
      attr = ippFindAttribute(job->attrs, "media", IPP_TAG_ZERO);

    if (attr != NULL)
    {
      const char	*pwg_name = ippGetString(attr, 0, NULL);
      pwg_media_t	*pwg_media = pwgMediaForPWG(pwg_name);

      if (pwg_name && pwg_media)
      {
	cupsCopyString(options->media.size_name, pwg_name, sizeof(options->media.size_name));
	options->media.size_width  = pwg_media->width;
	options->media.size_length = pwg_media->length;
	options->media.source[0]   = '\0';
      }
    }
  }

  if (!options->media.source[0])
  {
    for (i = 0; i < (size_t)printer->driver_data.num_source; i ++)
    {
      if (!strcmp(options->media.size_name, printer->driver_data.media_ready[i].size_name))
      {
        cupsCopyString(options->media.source, printer->driver_data.source[i], sizeof(options->media.source));
        break;
      }
    }

    if (!options->media.source[0])
      cupsCopyString(options->media.source, printer->driver_data.media_default.source, sizeof(options->media.source));
  }

  // orientation-requested
  if (doc && (attr = ippFindAttribute(doc->attrs, "orientation-requested", IPP_TAG_ENUM)) != NULL)
    options->orientation_requested = (ipp_orient_t)ippGetInteger(attr, 0);
  else if ((attr = ippFindAttribute(job->attrs, "orientation-requested", IPP_TAG_ENUM)) != NULL)
    options->orientation_requested = (ipp_orient_t)ippGetInteger(attr, 0);
  else if (printer->driver_data.orient_default)
    options->orientation_requested = printer->driver_data.orient_default;
  else
    options->orientation_requested = IPP_ORIENT_NONE;

  // output-bin
  if (printer->driver_data.num_bin > 0)
  {
    const char		*value;		// Attribute string value

    if (!doc || (value = ippGetString(ippFindAttribute(doc->attrs, "output-bin", IPP_TAG_ZERO), 0, NULL)) == NULL)
      value = ippGetString(ippFindAttribute(job->attrs, "output-bin", IPP_TAG_ZERO), 0, NULL);

    if (value != NULL)
      cupsCopyString(options->output_bin, value, sizeof(options->output_bin));
    else
      cupsCopyString(options->output_bin, printer->driver_data.bin[printer->driver_data.bin_default], sizeof(options->output_bin));
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
  if (doc && (attr = ippFindAttribute(doc->attrs, "print-color-mode", IPP_TAG_KEYWORD)) != NULL)
    options->print_color_mode = _papplColorModeValue(ippGetString(attr, 0, NULL));
  else if ((attr = ippFindAttribute(job->attrs, "print-color-mode", IPP_TAG_KEYWORD)) != NULL)
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
  if (doc && (attr = ippFindAttribute(doc->attrs, "print-content-optimize", IPP_TAG_KEYWORD)) != NULL)
    options->print_content_optimize = _papplContentValue(ippGetString(attr, 0, NULL));
  else if ((attr = ippFindAttribute(job->attrs, "print-content-optimize", IPP_TAG_KEYWORD)) != NULL)
    options->print_content_optimize = _papplContentValue(ippGetString(attr, 0, NULL));
  else
    options->print_content_optimize = printer->driver_data.content_default;

  // print-darkness
  if (doc && (attr = ippFindAttribute(doc->attrs, "print-darkness", IPP_TAG_INTEGER)) != NULL)
    options->print_darkness = ippGetInteger(attr, 0);
  else if ((attr = ippFindAttribute(job->attrs, "print-darkness", IPP_TAG_INTEGER)) != NULL)
    options->print_darkness = ippGetInteger(attr, 0);
  else
    options->print_darkness = printer->driver_data.darkness_default;

  options->darkness_configured = printer->driver_data.darkness_configured;

  // print-quality
  if (doc && (attr = ippFindAttribute(doc->attrs, "print-quality", IPP_TAG_ENUM)) != NULL)
    options->print_quality = (ipp_quality_t)ippGetInteger(attr, 0);
  else if ((attr = ippFindAttribute(job->attrs, "print-quality", IPP_TAG_ENUM)) != NULL)
    options->print_quality = (ipp_quality_t)ippGetInteger(attr, 0);
  else
    options->print_quality = printer->driver_data.quality_default;

  // print-scaling
  if (doc && (attr = ippFindAttribute(doc->attrs, "print-scaling", IPP_TAG_KEYWORD)) != NULL)
    options->print_scaling = _papplScalingValue(ippGetString(attr, 0, NULL));
  else if ((attr = ippFindAttribute(job->attrs, "print-scaling", IPP_TAG_KEYWORD)) != NULL)
    options->print_scaling = _papplScalingValue(ippGetString(attr, 0, NULL));
  else
    options->print_scaling = printer->driver_data.scaling_default;

  // print-speed
  if (doc && (attr = ippFindAttribute(doc->attrs, "print-speed", IPP_TAG_INTEGER)) != NULL)
    options->print_speed = ippGetInteger(attr, 0);
  else if ((attr = ippFindAttribute(job->attrs, "print-speed", IPP_TAG_INTEGER)) != NULL)
    options->print_speed = ippGetInteger(attr, 0);
  else
    options->print_speed = printer->driver_data.speed_default;

  // printer-resolution
  if (doc && (attr = ippFindAttribute(doc->attrs, "printer-resolution", IPP_TAG_RESOLUTION)) != NULL)
  {
    ipp_res_t	units;			// Resolution units

    options->printer_resolution[0] = ippGetResolution(attr, 0, options->printer_resolution + 1, &units);
  }
  else if ((attr = ippFindAttribute(job->attrs, "printer-resolution", IPP_TAG_RESOLUTION)) != NULL)
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
    i = (size_t)printer->driver_data.num_resolution / 2;
    options->printer_resolution[0] = printer->driver_data.x_resolution[i];
    options->printer_resolution[1] = printer->driver_data.y_resolution[i];
  }
  else
  {
    // print-quality=high
    i = (size_t)printer->driver_data.num_resolution - 1;
    options->printer_resolution[0] = printer->driver_data.x_resolution[i];
    options->printer_resolution[1] = printer->driver_data.y_resolution[i];
  }

  // sides
  if (doc && (attr = ippFindAttribute(doc->attrs, "sides", IPP_TAG_KEYWORD)) != NULL)
    options->sides = _papplSidesValue(ippGetString(attr, 0, NULL));
  else if ((attr = ippFindAttribute(job->attrs, "sides", IPP_TAG_KEYWORD)) != NULL)
    options->sides = _papplSidesValue(ippGetString(attr, 0, NULL));
  else if (printer->driver_data.sides_default != PAPPL_SIDES_ONE_SIDED && options->num_pages != 1)
    options->sides = printer->driver_data.sides_default;
  else
    options->sides = PAPPL_SIDES_ONE_SIDED;

  // Vendor options...
  for (i = 0; i < (size_t)printer->driver_data.num_vendor; i ++)
  {
    const char *name = printer->driver_data.vendor[i];
					// Vendor attribute name

    if (!doc || (attr = ippFindAttribute(doc->attrs, name, IPP_TAG_ZERO)) == NULL)
    {
      if ((attr = ippFindAttribute(job->attrs, name, IPP_TAG_ZERO)) == NULL)
      {
	char	defname[128];		// xxx-default attribute

	snprintf(defname, sizeof(defname), "%s-default", name);
	attr = ippFindAttribute(job->printer->attrs, defname, IPP_TAG_ZERO);
      }
    }

    if (attr)
    {
      char	value[1024];		// Value of attribute

      ippAttributeString(attr, value, sizeof(value));
      options->num_vendor = cupsAddOption(name, value, options->num_vendor, &options->vendor);
    }
  }

  // Figure out the PWG raster header...
  if (printer->driver_data.force_raster_type == PAPPL_RASTER_TYPE_BLACK_1)
  {
    // Force bitmap output...
    raster_type = mono_type = "black_1";

    if (options->print_color_mode == PAPPL_COLOR_MODE_BI_LEVEL || options->print_quality == IPP_QUALITY_DRAFT)
      memset(options->dither, 127, sizeof(options->dither));
    else if (options->print_content_optimize == PAPPL_CONTENT_PHOTO || (doc_number > 0 && !strcmp(job->documents[doc_number - 1].format, "image/jpeg")) || options->print_quality == IPP_QUALITY_HIGH)
      memcpy(options->dither, printer->driver_data.pdither, sizeof(options->dither));
    else
      memcpy(options->dither, printer->driver_data.gdither, sizeof(options->dither));
  }
  else if (options->print_color_mode == PAPPL_COLOR_MODE_COLOR)
  {
    // Color output...
    if (printer->driver_data.raster_types & PAPPL_RASTER_TYPE_SRGB_8)
      raster_type = "srgb_8";
    else if (printer->driver_data.raster_types & PAPPL_RASTER_TYPE_ADOBE_RGB_8)
      raster_type = "adobe-rgb_8";
    else
      raster_type = "rgb_8";

    if (printer->driver_data.raster_types & PAPPL_RASTER_TYPE_SGRAY_8)
      mono_type = "sgray_8";
    else
      mono_type = "black_8";
  }
  else
  {
    // Monochrome output...
    if (printer->driver_data.raster_types & PAPPL_RASTER_TYPE_SGRAY_8)
      raster_type = mono_type = "sgray_8";
    else
      raster_type = mono_type = "black_8";
  }

  if (options->print_quality == IPP_QUALITY_HIGH)
    memcpy(options->dither, printer->driver_data.pdither, sizeof(options->dither));
  else
    memcpy(options->dither, printer->driver_data.gdither, sizeof(options->dither));

  // Generate the raster header...
  memset(&media, 0, sizeof(media));

  cupsCopyString(media.media, options->media.size_name, sizeof(media.media));
  cupsCopyString(media.source, options->media.source, sizeof(media.source));
  cupsCopyString(media.type, options->media.type, sizeof(media.type));

  media.width  = options->media.size_width;
  media.length = options->media.size_length;
  media.bottom = options->media.bottom_margin;
  media.left   = options->media.left_margin;
  media.right  = options->media.right_margin;
  media.top    = options->media.top_margin;

  cupsRasterInitHeader(&options->header, &media, _papplContentString(options->print_content_optimize), options->print_quality, /*intent*/NULL, options->orientation_requested, _papplSidesString(options->sides), raster_type, options->printer_resolution[0], options->printer_resolution[1], sheet_back[printer->driver_data.duplex]);

  cupsRasterInitHeader(&options->mono_header, &media, _papplContentString(options->print_content_optimize), options->print_quality, /*intent*/NULL, options->orientation_requested, _papplSidesString(options->sides), mono_type, options->printer_resolution[0], options->printer_resolution[1], sheet_back[printer->driver_data.duplex]);

  if (options->handling == PAPPL_HANDLING_UNCOLLATED_COPIES)
  {
    // Uncollated copies are reported in the raster header...
    options->header.NumCopies      = (unsigned)job->copies;
    options->mono_header.NumCopies = (unsigned)job->copies;
  }
  else
  {
    // Collated copies are handled at the top level...
    options->header.NumCopies      = 1;
    options->mono_header.NumCopies = 1;
  }

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
  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "header.PWG_ImageBox=[%u %u %u %u]", options->header.cupsInteger[CUPS_RASTER_PWG_ImageBoxLeft], options->header.cupsInteger[CUPS_RASTER_PWG_ImageBoxTop], options->header.cupsInteger[CUPS_RASTER_PWG_ImageBoxRight], options->header.cupsInteger[CUPS_RASTER_PWG_ImageBoxBottom]);
  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "mono_header.cupsBitsPerColor=%u", options->mono_header.cupsBitsPerColor);
  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "mono_header.cupsBitsPerPixel=%u", options->mono_header.cupsBitsPerPixel);
  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "mono_header.cupsBytesPerLine=%u", options->mono_header.cupsBytesPerLine);
  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "mono_header.cupsColorOrder=%u", options->mono_header.cupsColorOrder);
  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "mono_header.cupsColorSpace=%u (%s)", options->mono_header.cupsColorSpace, cups_cspace_string(options->mono_header.cupsColorSpace));
  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "mono_header.cupsNumColors=%u", options->mono_header.cupsNumColors);
  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "num_pages=%u", options->num_pages);
  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "copies=%d", options->copies);
  if (printer->driver_data.finishings_supported)
  {
    char	finishings[1024],	// Finishings string
		*finptr = finishings,	// Pointer into finishings string
		*finend = finishings + sizeof(finishings) - 1;
					// End of finishings string

    if (options->finishings == PAPPL_FINISHINGS_NONE)
    {
      cupsCopyString(finishings, "none", sizeof(finishings));
    }
    else
    {
      pappl_finishings_t	f;	// Current finishing

      for (f = PAPPL_FINISHINGS_PUNCH; f <= PAPPL_FINISHINGS_STAPLE_DUAL_TOP && finptr < finend; f *= 2)
      {
        if (options->finishings & f)
        {
          if (finptr > finishings && finptr < finend)
            *finptr++ = ',';

          if (finptr < finend)
          {
            cupsCopyString(finptr, _papplFinishingsString(f), (size_t)(finend - finptr + 1));
            finptr += strlen(finptr);
          }
        }
      }
    }

    papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "finishings=0x%x (%s)", options->finishings, finishings);
  }
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
  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "multiple-document-handling=%s", _papplHandlingString(options->handling));
  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "orientation-requested=%d(%s)", (int)options->orientation_requested, ippEnumString("orientation-requested", (int)options->orientation_requested));
  if (options->output_bin[0])
    papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "output-bin='%s'", options->output_bin);
  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "page-ranges=%u-%u", options->first_page, options->last_page);
  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "print-color-mode='%s'", _papplColorModeString(options->print_color_mode));
  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "print-content-optimize='%s'", _papplContentString(options->print_content_optimize));
  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "print-darkness=%d", options->print_darkness);
  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "print-quality=%s", ippEnumString("print-quality", (int)options->print_quality));
  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "print-scaling='%s'", _papplScalingString(options->print_scaling));
  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "print-speed=%d", options->print_speed);
  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "printer-resolution=%dx%ddpi", options->printer_resolution[0], options->printer_resolution[1]);
  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "sides='%s'", _papplSidesString(options->sides));
  for (i = 0; i < (size_t)options->num_vendor; i ++)
    papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "%s=%s", options->vendor[i].name, options->vendor[i].value);

  _papplRWUnlock(printer);

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
    cupsFreeOptions((size_t)options->num_vendor, options->vendor);
    free(options);
  }
}


//
// '_papplJobProcess()' - Process a print job.
//

void *					// O - Thread exit status
_papplJobProcess(pappl_job_t *job)	// I - Job
{
  bool			started = false;// Have we started the job?
  int			copy,		// Current (collated) copy
			doc_number;	// Current document number
  _pappl_doc_t		*doc;		// Current document
  _pappl_mime_filter_t	*filter;	// Filter for printing
  _pappl_mime_inspector_t *inspector;	// Inspector for file format
  pappl_pr_driver_data_t driver_data;	// Printer driver data
  pappl_pr_options_t	*options[_PAPPL_MAX_DOCUMENTS + 1];
					// Print options


  memset(options, 0, sizeof(options));

  // Start processing the job...
  if (start_job(job))
  {
    // Get driver data...
    papplPrinterGetDriverData(papplJobGetPrinter(job), &driver_data);

    // Prepare options...
    for (doc_number = 1, doc = job->documents; doc_number <= job->num_documents; doc_number ++, doc ++)
    {
      if (!doc->impressions && (inspector = _papplSystemFindMIMEInspector(job->system, doc->format)) != NULL)
	(inspector->cb)(job, doc_number, &doc->impressions, &doc->impcolor, inspector->cbdata);

      options[doc_number] = papplJobCreatePrintOptions(job, doc_number, (unsigned)doc->impressions, doc->impcolor >= doc->impressions);

      if (doc->impcolor >= doc->impressions)
        job->is_color = true;
    }

    options[0] = papplJobCreatePrintOptions(job, 0, (unsigned)job->impressions, job->is_color);

    if (!(driver_data.rstartjob_cb)(job, options[0], job->printer->device))
    {
      papplLogJob(job, PAPPL_LOGLEVEL_ERROR, "Unable to start raster job.");
      goto abort_job;
    }

    started = true;

    for (copy = 0; copy < options[0]->copies; copy ++)
    {
      for (doc_number = 1, doc = job->documents; doc_number <= job->num_documents && job->state != IPP_JSTATE_ABORTED; doc_number ++, doc ++)
      {
        // Skip canceled documents...
        if (doc->state >= IPP_DSTATE_CANCELED)
          continue;

        if (!doc->processing)
          doc->processing = time(NULL);

        doc->state = IPP_DSTATE_PROCESSING;

        _papplPrinterUpdateProxyDocument(job->printer, job, doc_number);

	// Do file-specific conversions...
	papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "Processing document %d/%d...", doc_number, job->num_documents);

	if ((filter = _papplSystemFindMIMEFilter(job->system, doc->format, job->printer->driver_data.format)) == NULL)
	  filter =_papplSystemFindMIMEFilter(job->system, doc->format, "image/pwg-raster");

	if (filter)
	{
	  // Filter as needed...
	  if (!(filter->cb)(job, doc_number, options[doc_number], job->printer->device, filter->cbdata))
	    goto abort_job;
	}
	else if (job->printer->driver_data.format && job->printer->driver_data.printfile_cb && !strcmp(doc->format, job->printer->driver_data.format))
	{
	  // Send file raw...
	  if (!filter_raw(job, doc_number, options[doc_number], job->printer->device))
	    goto abort_job;
	}
	else
	{
	  // Abort a job we can't process...
	  papplLogJob(job, PAPPL_LOGLEVEL_ERROR, "Unable to process job with format '%s'.", doc->format);
	  goto abort_job;
	}

	// TODO: Send blank page when options->handling is PAPPL_HANDLING_SINGLE_DOCUMENT_NEW_SHEET and we have an odd number of sheets
      }

      papplJobSetCopiesCompleted(job, 1);
    }

    // End the job...
    started = false;
    if (!(driver_data.rendjob_cb)(job, options[0], job->printer->device))
    {
      papplLogJob(job, PAPPL_LOGLEVEL_ERROR, "Unable to end raster job.");
      goto abort_job;
    }

    // Free options and set document states...
    for (doc_number = 0, doc = job->documents; doc_number <= job->num_documents; doc_number ++, doc ++)
    {
      papplJobDeletePrintOptions(options[doc_number]);
      doc->state     = IPP_DSTATE_COMPLETED;
      doc->completed = time(NULL);

      _papplPrinterUpdateProxyDocument(job->printer, job, doc_number);
    }
  }

  // Move the job to a completed state...
  finish_job(job);

  return (NULL);

  // If we get here something went wrong...
  abort_job:

  job->state = IPP_JSTATE_ABORTED;

  if (started)
    (driver_data.rendjob_cb)(job, options[0], job->printer->device);

  // Free options and set document states...
  for (doc_number = 0, doc = job->documents; doc_number <= job->num_documents; doc_number ++, doc ++)
  {
    papplJobDeletePrintOptions(options[doc_number]);
    doc->state     = IPP_DSTATE_ABORTED;
    doc->completed = time(NULL);

    _papplPrinterUpdateProxyDocument(job->printer, job, doc_number);
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
  cups_page_header_t	header;		// Page header
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
  int			job_pages_per_set;
					// "job-pages-per-set" value, if any
  unsigned		next_copy;	// Next copy boundary


  // Start processing the job...
  job->streaming = true;

  if (!start_job(job))
    goto complete_job;

  // Open the raster stream...
  if ((ras = cupsRasterOpenIO((cups_raster_cb_t)httpRead, client->http, CUPS_RASTER_READ)) == NULL)
  {
    papplLogJob(job, PAPPL_LOGLEVEL_ERROR, "Unable to open raster stream from client - %s", cupsGetErrorString());
    job->state = IPP_JSTATE_ABORTED;
    goto complete_job;
  }

  // Prepare options...
  if (!cupsRasterReadHeader(ras, &header))
  {
    papplLogJob(job, PAPPL_LOGLEVEL_ERROR, "Unable to read raster stream from client - %s", cupsGetErrorString());
    job->state = IPP_JSTATE_ABORTED;
    goto complete_job;
  }

  if ((job_pages_per_set = ippGetInteger(ippFindAttribute(job->attrs, "job-pages-per-set", IPP_TAG_INTEGER), 0)) > 0)
  {
    // Use the job-pages-per-set value to set the number of impressions...
    papplJobSetImpressions(job, job_pages_per_set);

    // Track copies at page boundaries...
    next_copy = (unsigned)job_pages_per_set;
  }
  else
  {
    // Don't track copies...
    next_copy = 0;
  }

  if ((header_pages = header.cupsInteger[CUPS_RASTER_PWG_TotalPageCount]) > 0 && job_pages_per_set == 0)
    papplJobSetImpressions(job, (int)header.cupsInteger[CUPS_RASTER_PWG_TotalPageCount]);

  if ((options = papplJobCreatePrintOptions(job, 0, (unsigned)job->impressions, header.cupsBitsPerPixel > 8)) == NULL)
  {
    papplLogJob(job, PAPPL_LOGLEVEL_ERROR, "Unable to create options for job.");
    job->state = IPP_JSTATE_ABORTED;
    goto complete_job;
  }

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

    papplJobSetImpressionsCompleted(job, 1);

    papplLogJob(job, PAPPL_LOGLEVEL_INFO, "Page %u raster data is %ux%ux%u (%s)", page + 1, header.cupsWidth, header.cupsHeight, header.cupsBitsPerPixel, cups_cspace_string(header.cupsColorSpace));

    papplSystemAddEvent(printer->system, printer, job, PAPPL_EVENT_JOB_PROGRESS, NULL);

    // Set options for this page...
    papplJobDeletePrintOptions(options);
    options = papplJobCreatePrintOptions(job, 0, (unsigned)job->impressions, header.cupsBitsPerPixel > 8);

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

    page ++;

    if (page == next_copy)
    {
      // Report a completed copy...
      papplJobSetCopiesCompleted(job, 1);
      next_copy += (unsigned)job_pages_per_set;
    }

    if (job->is_canceled)
    {
      break;
    }
    else if (y < header.cupsHeight)
    {
      papplLogJob(job, PAPPL_LOGLEVEL_ERROR, "Unable to read page from raster stream from client - %s", cupsGetErrorString());
      job->state = IPP_JSTATE_ABORTED;
      break;
    }
  }
  while (cupsRasterReadHeader(ras, &header));

  if (next_copy == 0)
  {
    // Not tracking copies so record this as a single completed copy...
    papplJobSetCopiesCompleted(job, 1);
  }

  if (!(printer->driver_data.rendjob_cb)(job, options, job->printer->device))
    job->state = IPP_JSTATE_ABORTED;
  else if (header_pages == 0 && job_pages_per_set == 0)
    papplJobSetImpressions(job, (int)page);

  complete_job:

  papplJobDeletePrintOptions(options);

  if (httpGetState(client->http) == HTTP_STATE_POST_RECV)
  {
    // Flush excess data...
    char	buffer[8192];		// Read buffer

    while (httpRead(client->http, buffer, sizeof(buffer)) > 0)
      ;				// Read all document data
  }

  cupsRasterClose(ras);

  finish_job(job);
  return;
}


//
// 'papplJobResume()' - Resume processing of a job.
//

void
papplJobResume(pappl_job_t     *job,	// I - Job
              pappl_jreason_t remove)	// I - Reasons to remove from "job-state-reasons"
{
  // Range check input...
  if (!job)
    return;

  // Update state...
  _papplRWLockWrite(job);

  if (job->state == IPP_JSTATE_STOPPED)
  {
    job->state         = IPP_JSTATE_PENDING;
    job->state_reasons &= ~remove;
  }

  _papplRWUnlock(job);

  _papplRWLockWrite(job->printer);
  _papplPrinterCheckJobsNoLock(job->printer);
  _papplRWUnlock(job->printer);
}


//
// 'papplJobSuspend()' - Temporarily stop processing of a job.
//

void
papplJobSuspend(pappl_job_t     *job,	// I - Job
               pappl_jreason_t add)	// I - Reasons to add to "job-state-reasons"
{
  // Range check input...
  if (!job)
    return;

  // Update state...
  _papplRWLockWrite(job);

  if (job->state < IPP_JSTATE_STOPPED)
  {
    job->state         = IPP_JSTATE_STOPPED;
    job->state_reasons |= add;
  }

  _papplRWUnlock(job);
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
filter_raw(
    pappl_job_t        *job,		// I - Job
    int                doc_number,	// I - Document number (`1` based)
    pappl_pr_options_t *options,	// I - Options
    pappl_device_t     *device)		// I - Device
{
  papplJobSetImpressions(job, 1);

  if (!(job->printer->driver_data.printfile_cb)(job, doc_number, options, device))
    return (false);

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
  static const char * const job_states[] =
  {
    "Pending",
    "Held",
    "Processing",
    "Stopped",
    "Canceled",
    "Aborted",
    "Completed"
  };


  _papplRWLockWrite(printer);
  _papplRWLockWrite(job);

  if (job->is_canceled)
    job->state = IPP_JSTATE_CANCELED;
  else if (job->state == IPP_JSTATE_PROCESSING)
    job->state = IPP_JSTATE_COMPLETED;

  papplLogJob(job, PAPPL_LOGLEVEL_INFO, "%s, job-impressions-completed=%d.", job_states[job->state - IPP_JSTATE_PENDING], job->impcompleted);

  if (job->state >= IPP_JSTATE_CANCELED)
    job->completed = time(NULL);

  _papplPrinterUpdateProxyJobNoLock(printer, job);

  httpClose(job->proxy_http);
  job->proxy_http = NULL;

  free(job->proxy_resource);
  job->proxy_resource = NULL;

  _papplJobSetRetainNoLock(job);

  printer->processing_job = NULL;

  if (job->state >= IPP_JSTATE_CANCELED && !printer->max_preserved_jobs && !job->retain_until)
    _papplJobRemoveFiles(job);

  _papplSystemAddEventNoLock(job->system, job->printer, job, PAPPL_EVENT_JOB_COMPLETED, NULL);

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

  _papplRWUnlock(job);

  _papplSystemAddEventNoLock(printer->system, printer, NULL, PAPPL_EVENT_PRINTER_STATE_CHANGED, NULL);

// TODO: Is this necessary since we do it from the main loop?
//  if (printer->max_preserved_jobs > 0)
//    _papplPrinterCleanJobsNoLock(printer);

  _papplRWUnlock(printer);

  _papplSystemConfigChanged(printer->system);

  if (papplPrinterIsDeleted(printer))
  {
    papplPrinterDelete(printer);
    printer = NULL;
  }
  else if (!strncmp(printer->device_uri, "file:", 5) || cupsArrayGetCount(printer->active_jobs) == 0 || !printer->driver_data.keep_device_open)
  {
    // Close device and report IO metrics...
    pappl_devmetrics_t	metrics;	// Metrics for device IO

    _papplRWLockWrite(printer);

    papplDeviceGetMetrics(printer->device, &metrics);
    papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "Device read metrics: %lu requests, %lu bytes, %lu msecs", (unsigned long)metrics.read_requests, (unsigned long)metrics.read_bytes, (unsigned long)metrics.read_msecs);
    papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "Device write metrics: %lu requests, %lu bytes, %lu msecs", (unsigned long)metrics.write_requests, (unsigned long)metrics.write_bytes, (unsigned long)metrics.write_msecs);

//    papplLogPrinter(printer, PAPPL_LOGLEVEL_DEBUG, "Closing device for job %d.", job->job_id);

    papplDeviceClose(printer->device);
    printer->device = NULL;

    _papplRWUnlock(printer);
  }

  if (papplPrinterGetNumberOfActiveJobs(printer) > 0)
  {
    _papplRWLockWrite(printer);
    _papplPrinterCheckJobsNoLock(printer);
    _papplRWUnlock(printer);
  }
}


//
// 'start_job()' - Start processing a job...
//

static bool				// O - `true` on success, `false` otherwise
start_job(pappl_job_t *job)		// I - Job
{
  bool		ret = false;		// Return value
  pappl_printer_t *printer = job->printer;
					// Printer
  bool		first_open = true;	// Is this the first time we try to open the device?


  // Move the job to the 'processing' state...
  _papplRWLockWrite(printer);
  _papplRWLockWrite(job);

  papplLogJob(job, PAPPL_LOGLEVEL_INFO, "Starting print job.");

  job->state              = IPP_JSTATE_PROCESSING;
  job->processing         = time(NULL);
  printer->processing_job = job;

  if (printer->proxy_uri && ippFindAttribute(job->attrs, "parent-job-id", IPP_TAG_INTEGER))
  {
    // Connect to the proxy to report status updates...
    char	resource[1024];		// Resource path

    if (!job->proxy_http)
    {
      job->proxy_http = _papplPrinterConnectProxyNoLock(printer, resource, sizeof(resource));

      free(job->proxy_resource);
      if (job->proxy_http)
        job->proxy_resource = strdup(resource);
      else
        job->proxy_resource = NULL;
    }

    if (job->proxy_http && job->proxy_resource)
      _papplPrinterUpdateProxyJobNoLock(printer, job);
  }

  _papplSystemAddEventNoLock(printer->system, printer, job, PAPPL_EVENT_JOB_STATE_CHANGED, NULL);

  _papplRWUnlock(job);

  // Open the output device...
  if (printer->device_in_use)
  {
    papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "Waiting for device to become available.");

    while (printer->device_in_use && !printer->is_deleted && !job->is_canceled && papplSystemIsRunning(printer->system))
    {
      _papplRWUnlock(printer);
      sleep(1);
      _papplRWLockWrite(printer);
    }
  }

  while (!printer->device && !printer->is_deleted && !job->is_canceled && papplSystemIsRunning(printer->system))
  {
    printer->device = papplDeviceOpen(printer->device_uri, job, papplLogDevice, job->system);

    if (!printer->device && !printer->is_deleted && !job->is_canceled)
    {
      // Log that the printer is unavailable then sleep for 5 seconds to retry.
      if (first_open)
      {
        papplLogPrinter(printer, PAPPL_LOGLEVEL_ERROR, "Unable to open device '%s', pausing queue until printer becomes available.", printer->device_uri);
        first_open = false;

	printer->state      = IPP_PSTATE_STOPPED;
	printer->state_time = time(NULL);
      }
      else
      {
        papplLogPrinter(printer, PAPPL_LOGLEVEL_DEBUG, "Still unable to open device.");
      }

      _papplRWUnlock(printer);
      sleep(5);
      _papplRWLockWrite(printer);
    }
  }

  if (!papplSystemIsRunning(printer->system))
  {
    job->state = IPP_JSTATE_PENDING;

    _papplRWLockRead(job);
    _papplSystemAddEventNoLock(job->system, job->printer, job, PAPPL_EVENT_JOB_STATE_CHANGED, NULL);
    _papplRWUnlock(job);

    if (printer->device)
    {
      papplDeviceClose(printer->device);
      printer->device = NULL;
    }
  }

  if (printer->device)
  {
    // Move the printer to the 'processing' state...
    printer->state      = IPP_PSTATE_PROCESSING;
    printer->state_time = time(NULL);
    ret                 = true;
  }

  _papplSystemAddEventNoLock(printer->system, printer, NULL, PAPPL_EVENT_PRINTER_STATE_CHANGED, NULL);

  _papplRWUnlock(printer);

  return (ret);
}
