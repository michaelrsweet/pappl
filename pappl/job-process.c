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
#include "dither-private.h"
#ifdef HAVE_LIBJPEG
#  include <jpeglib.h>
#endif // HAVE_LIBJPEG
#ifdef HAVE_LIBPNG
#  include <png.h>
#endif // HAVE_LIBPNG


//
// Local functions...
//

static const char *cups_cspace_string(cups_cspace_t cspace);
static bool	filter_raw(pappl_job_t *job, pappl_device_t *device);
static void	finish_job(pappl_job_t *job);
static void	start_job(pappl_job_t *job);


//
// '_papplJobFilterJPEG()' - Filter a JPEG image file.
//

#ifdef HAVE_LIBJPEG
bool
_papplJobFilterJPEG(
    pappl_job_t    *job,		// I - Job
    pappl_device_t *device,		// I - Device
    void           *data)		// I - Filter data (unused)
{
  (void)job;
  (void)device;
  (void)data;

  return (false);
}
#endif // HAVE_LIBJPEG


//
// 'process_png()' - Process a PNG image file.
//

#ifdef HAVE_LIBPNG
bool					// O - `true` on success and `false` otherwise
_papplJobFilterPNG(
    pappl_job_t    *job,		// I - Job
    pappl_device_t *device,		// I - Device
    void           *data)		// I - Filter data (unused)
{
  int			i;		// Looping var
  pappl_printer_t	*printer = job->printer;
					// Printer
  const unsigned char	*dither;	// Dither line
  pappl_options_t	options;	// Job options
  png_image		png;		// PNG image data
  png_color		bg;		// Background color
  unsigned		ileft,		// Imageable left margin
			itop,		// Imageable top margin
			iwidth,		// Imageable width
			iheight;	// Imageable length/height
  unsigned char		white,		// White color
			*line = NULL,	// Output line
			*lineptr,	// Pointer in line
			byte,		// Byte in line
			*pixels = NULL,	// Pixels in image
			*pixbase,	// Pointer to first pixel
			*pixptr,	// Pointer into image
			bit;		// Current bit
  unsigned		png_width,	// Rotated PNG width
			png_height,	// Rotated PNG height
			x,		// X position
			xsize,		// Scaled width
			xstep,		// X step
			xstart,		// X start position
			xend,		// X end position
			y,		// Y position
			ysize,		// Scaled height
			ystart,		// Y start position
			yend;		// Y end position
  int			png_bpp,	// Bytes per pixel
			xdir,
			xerr,		// X error accumulator
			xmod,		// X modulus
			ydir;


  // Load the PNG...
  (void)data;

  memset(&png, 0, sizeof(png));
  png.version = PNG_IMAGE_VERSION;

  bg.red = bg.green = bg.blue = 255;

  png_image_begin_read_from_file(&png, job->filename);

  if (png.warning_or_error & PNG_IMAGE_ERROR)
  {
    papplLogJob(job, PAPPL_LOGLEVEL_ERROR, "Unable to open PNG file '%s' - %s", job->filename, png.message);
    goto abort_job;
  }

  papplLogJob(job, PAPPL_LOGLEVEL_INFO, "PNG image is %ux%u", png.width, png.height);

  // Prepare options...
  papplJobGetOptions(job, &options, 1, (png.format & PNG_FORMAT_FLAG_COLOR) != 0);
  options.header.cupsInteger[CUPS_RASTER_PWG_TotalPageCount] = options.copies;
  papplJobSetImpressions(job, 1);

  if (options.print_scaling == PAPPL_SCALING_FILL)
  {
    // Scale to fill the entire media area...
    ileft   = 0;
    itop    = 0;
    iwidth  = options.header.cupsWidth;
    iheight = options.header.cupsHeight;
  }
  else
  {
    // Scale/center within the margins...
    ileft   = options.media.left_margin * options.printer_resolution[0] / 2540;
    itop    = options.media.top_margin * options.printer_resolution[1] / 2540;
    iwidth  = options.header.cupsWidth - (options.media.left_margin + options.media.right_margin) * options.printer_resolution[0] / 2540;
    iheight = options.header.cupsHeight - (options.media.bottom_margin + options.media.top_margin) * options.printer_resolution[1] / 2540;
  }

  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "ileft=%u, itop=%u, iwidth=%u, iheight=%u", ileft, itop, iwidth, iheight);

  if (iwidth == 0 || iheight == 0)
  {
    papplLogJob(job, PAPPL_LOGLEVEL_ERROR, "Invalid media size");
    goto abort_job;
  }

  if (png.format & PNG_FORMAT_FLAG_COLOR)
  {
    png.format = PNG_FORMAT_RGB;
    png_bpp    = 3;
  }
  else
  {
    png.format = PNG_FORMAT_GRAY;
    png_bpp    = 1;
  }

  pixels = malloc(PNG_IMAGE_SIZE(png));

  png_image_finish_read(&png, &bg, pixels, 0, NULL);

  if (png.warning_or_error & PNG_IMAGE_ERROR)
  {
    papplLogJob(job, PAPPL_LOGLEVEL_ERROR, "Unable to open PNG file '%s' - %s", job->filename, png.message);
    goto abort_job;
  }

  // Figure out the scaling and rotation of the image...
  if (options.orientation_requested == IPP_ORIENT_NONE)
  {
    if (png.width > png.height && options.header.cupsWidth < options.header.cupsHeight)
    {
      options.orientation_requested = IPP_ORIENT_LANDSCAPE;
      papplLogJob(job, PAPPL_LOGLEVEL_INFO, "Auto-orientation: landscape");
    }
    else
    {
      options.orientation_requested = IPP_ORIENT_PORTRAIT;
      papplLogJob(job, PAPPL_LOGLEVEL_INFO, "Auto-orientation: portrait");
    }
  }

  switch (options.orientation_requested)
  {
    default :
    case IPP_ORIENT_PORTRAIT :
        pixbase    = pixels;
        png_width  = png.width;
        png_height = png.height;
        xdir       = png_bpp;
        ydir       = png_bpp * png.width;

	xsize = iwidth;
	ysize = xsize * png.height / png.width;
	if (ysize > iheight)
	{
	  ysize = iheight;
	  xsize = ysize * png.width / png.height;
	}
	break;

    case IPP_ORIENT_REVERSE_PORTRAIT :
        pixbase    = pixels + png_bpp * png.width * png.height - png_bpp;
        png_width  = png.width;
        png_height = png.height;
        xdir       = -png_bpp;
        ydir       = -png_bpp * png.width;

	xsize = iwidth;
	ysize = xsize * png.height / png.width;
	if (ysize > iheight)
	{
	  ysize = iheight;
	  xsize = ysize * png.width / png.height;
	}
	break;

    case IPP_ORIENT_LANDSCAPE : // 90 counter-clockwise
        pixbase    = pixels + png.width - png_bpp;
        png_width  = png.height;
        png_height = png.width;
        xdir       = png_bpp * png.width;
        ydir       = -png_bpp;

	xsize = iwidth;
	ysize = xsize * png.width / png.height;
	if (ysize > iheight)
	{
	  ysize = iheight;
	  xsize = ysize * png.height / png.width;
	}
	break;

    case IPP_ORIENT_REVERSE_LANDSCAPE : // 90 clockwise
        pixbase    = pixels + png_bpp * (png.height - 1) * png.width;
        png_width  = png.height;
        png_height = png.width;
        xdir       = -png_bpp * png.width;
        ydir       = png_bpp;

	xsize = iwidth;
	ysize = xsize * png.width / png.height;
	if (ysize > iheight)
	{
	  ysize = iheight;
	  xsize = ysize * png.height / png.width;
	}
        break;
  }

  xstart = ileft + (iwidth - xsize) / 2;
  xend   = xstart + xsize;
  ystart = itop + (iheight - ysize) / 2;
  yend   = ystart + ysize;

  xmod   = png_width % xsize;
  xstep  = (png_width / xsize) * xdir;

  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "xsize=%u, xstart=%u, xend=%u, xdir=%d, xmod=%d, xstep=%d", xsize, xstart, xend, xdir, xmod, xstep);
  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "ysize=%u, ystart=%u, yend=%u, ydir=%d", ysize, ystart, yend, ydir);

  // Start the job...
  if (!(printer->driver_data.rstartjob)(job, &options, device))
  {
    papplLogJob(job, PAPPL_LOGLEVEL_ERROR, "Unable to start raster job.");
    goto abort_job;
  }

  if (options.header.cupsColorSpace == CUPS_CSPACE_K || options.header.cupsColorSpace == CUPS_CSPACE_CMYK)
    white = 0x00;
  else
    white = 0xff;

  line = malloc(options.header.cupsBytesPerLine);

  // Print every copy...
  for (i = 0; i < options.copies; i ++)
  {
    papplJobSetImpressionsCompleted(job, 1);

    if (!(printer->driver_data.rstartpage)(job, &options, device, 1))
    {
      papplLogJob(job, PAPPL_LOGLEVEL_ERROR, "Unable to start raster page.");
      goto abort_job;
    }

    // Leading blank space...
    memset(line, white, options.header.cupsBytesPerLine);
    for (y = 0; y < ystart; y ++)
    {
      if (!(printer->driver_data.rwrite)(job, &options, device, y, line))
      {
	papplLogJob(job, PAPPL_LOGLEVEL_ERROR, "Unable to write raster line %u.", y);
	goto abort_job;
      }
    }

    // Now RIP the image...
    for (; y < yend; y ++)
    {
      pixptr = pixbase + ydir * (int)((y - ystart + 1) * png_height / ysize);

      if (options.header.cupsBitsPerPixel == 1)
      {
        // Need to dither the image to 1-bit black...
	dither = options.dither[y & 15];

	for (x = xstart, lineptr = line + x / 8, bit = 128 >> (x & 7), byte = 0, xerr = xmod / 2; x < xend; x ++)
	{
	  // Dither the current pixel...
	  if (*pixptr <= dither[x & 15])
	    byte |= bit;

	  // Advance to the next pixel...
	  pixptr += xstep;
	  xerr += xmod;
	  if (xerr >= xsize)
	  {
	    // Accumulated error has overflowed, advance another pixel...
	    xerr -= xsize;
	    pixptr += xdir;
	  }

	  // and the next bit
	  if (bit == 1)
	  {
	    // Current byte is "full", save it...
	    *lineptr++ = byte;
	    byte = 0;
	    bit  = 128;
	  }
	  else
	    bit /= 2;
	}

	if (bit < 128)
	  *lineptr = byte;
      }
      else if (options.header.cupsColorSpace == CUPS_CSPACE_K)
      {
        // Need to invert the image...
	for (x = xstart, lineptr = line + x, xerr = xmod / 2; x < xend; x ++)
	{
	  // Copy an inverted grayscale pixel...
	  *lineptr++ = ~*pixptr;

	  // Advance to the next pixel...
	  pixptr += xstep;
	  xerr += xmod;
	  if (xerr >= xsize)
	  {
	    // Accumulated error has overflowed, advance another pixel...
	    xerr -= xsize;
	    pixptr += xdir;
	  }
	}
      }
      else
      {
        // Need to copy the image...
        unsigned bpp = options.header.cupsBitsPerPixel / 8;

	for (x = xstart, lineptr = line + x * bpp, xerr = xmod / 2; x < xend; x ++)
	{
	  // Copy a grayscale or RGB pixel...
	  memcpy(lineptr, pixptr, bpp);
	  lineptr += bpp;

	  // Advance to the next pixel...
	  pixptr += xstep;
	  xerr += xmod;
	  if (xerr >= xsize)
	  {
	    // Accumulated error has overflowed, advance another pixel...
	    xerr -= xsize;
	    pixptr += xdir;
	  }
	}
      }

      if (!(printer->driver_data.rwrite)(job, &options, device, y, line))
      {
	papplLogJob(job, PAPPL_LOGLEVEL_ERROR, "Unable to write raster line %u.", y);
	goto abort_job;
      }
    }

    // Trailing blank space...
    memset(line, white, options.header.cupsBytesPerLine);
    for (; y < options.header.cupsHeight; y ++)
    {
      if (!(printer->driver_data.rwrite)(job, &options, device, y, line))
      {
	papplLogJob(job, PAPPL_LOGLEVEL_ERROR, "Unable to write raster line %u.", y);
	goto abort_job;
      }
    }

    // End the page...
    if (!(printer->driver_data.rendpage)(job, &options, device, 1))
    {
      papplLogJob(job, PAPPL_LOGLEVEL_ERROR, "Unable to end raster page.");
      goto abort_job;
    }

    job->impcompleted ++;
  }

  // End the job...
  if (!(printer->driver_data.rendjob)(job, &options, device))
  {
    papplLogJob(job, PAPPL_LOGLEVEL_ERROR, "Unable to end raster job.");
    goto abort_job;
  }

  // Free the image data when we're done...
  png_image_free(&png);
  free(pixels);
  free(line);

  return (true);

  // If we get there then something bad happened...
  abort_job:

  job->state = IPP_JSTATE_ABORTED;

  // Free the image data when we're done...
  png_image_free(&png);
  free(pixels);
  free(line);

  return (false);
}
#endif // HAVE_LIBPNG


//
// 'papplJobGetOptions()' - Get the options for a job.
//
// The "num_pages" and "color" arguments specify the number of pages and whether
// the document contains non-grayscale colors - this information typically comes
// from parsing the job file.
//

pappl_options_t *			// O - Job options data or `NULL` on error
papplJobGetOptions(
    pappl_job_t     *job,		// I - Job
    pappl_options_t *options,		// I - Job options data
    unsigned        num_pages,		// I - Number of pages
    bool            color)		// I - Is the document in color?
{
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
  memset(options, 0, sizeof(pappl_options_t));

  options->num_pages = num_pages;
  options->media     = printer->driver_data.media_default;

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
  else
    options->orientation_requested = IPP_ORIENT_NONE;

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

  if (printer->driver_data.force_raster_type == PAPPL_PWG_RASTER_TYPE_BLACK_1)
  {
    // Force bitmap output...
    raster_type = "black_1";

    if (options->print_color_mode == PAPPL_COLOR_MODE_BI_LEVEL)
      memcpy(options->dither, dithert, sizeof(options->dither));
    else
      memcpy(options->dither, ditherc, sizeof(options->dither));
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

  // print-content-optimize
  if ((attr = ippFindAttribute(job->attrs, "print-content-optimize", IPP_TAG_KEYWORD)) != NULL)
    options->print_content_optimize = _papplContentValue(ippGetString(attr, 0, NULL));
  else
    options->print_content_optimize = PAPPL_CONTENT_AUTO;

  // print-darkness
  if ((attr = ippFindAttribute(job->attrs, "print-darkness", IPP_TAG_INTEGER)) != NULL)
    options->print_darkness = ippGetInteger(attr, 0);

  options->darkness_configured = job->printer->driver_data.darkness_configured;

  // print-quality
  if ((attr = ippFindAttribute(job->attrs, "print-quality", IPP_TAG_ENUM)) != NULL)
    options->print_quality = (ipp_quality_t)ippGetInteger(attr, 0);
  else
    options->print_quality = IPP_QUALITY_NORMAL;

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
  else if (printer->driver_data.sides_default != PAPPL_SIDES_ONE_SIDED && num_pages > 1)
    options->sides = printer->driver_data.sides_default;
  else
    options->sides = PAPPL_SIDES_ONE_SIDED;

  // Figure out the PWG raster header...
  cupsRasterInitPWGHeader(&options->header, pwgMediaForPWG(options->media.size_name), raster_type, options->printer_resolution[0], options->printer_resolution[1], _papplSidesString(options->sides), sheet_back[printer->driver_data.duplex]);

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
  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "media-col.right-margin=%d", options->media.right_margin);
  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "media-col.size=%dx%d", options->media.size_width, options->media.size_length);
  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "media-col.size-name='%s'", options->media.size_name);
  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "media-col.source='%s'", options->media.source);
  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "media-col.top-margin=%d", options->media.top_margin);
  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "media-col.top-offset=%d", options->media.top_offset);
  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "media-col.tracking='%s'", _papplMediaTrackingString(options->media.tracking));
  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "media-col.type='%s'", options->media.type);
  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "orientation-requested=%s", ippEnumString("orientation-requested", (int)options->orientation_requested));
  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "print-color-mode='%s'", _papplColorModeString(options->print_color_mode));
  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "print-content-optimize='%s'", _papplContentString(options->print_content_optimize));
  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "print-darkness=%d", options->print_darkness);
  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "print-quality=%s", ippEnumString("print-quality", (int)options->print_quality));
  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "print-scaling='%s'", _papplScalingString(options->print_scaling));
  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "print-speed=%d", options->print_speed);
  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "printer-resolution=%dx%ddpi", options->printer_resolution[0], options->printer_resolution[1]);

  pthread_rwlock_unlock(&printer->rwlock);

  return (options);
}


//
// 'lprintProcessJob()' - Process a print job.
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
  pappl_options_t	options;	// Job options
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

  papplJobGetOptions(job, &options, job->impressions, header.cupsBitsPerPixel > 8);

  if (!(printer->driver_data.rstartjob)(job, &options, job->printer->device))
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
    papplJobGetOptions(job, &options, job->impressions, header.cupsBitsPerPixel > 8);

    if (options.header.cupsBitsPerPixel >= 8 && header.cupsBitsPerPixel >= 8)
      options.header = header;		// Use page header from client

    if (!(printer->driver_data.rstartpage)(job, &options, job->printer->device, page))
    {
      job->state = IPP_JSTATE_ABORTED;
      break;
    }

    pixels = malloc(header.cupsBytesPerLine);
    line   = malloc(options.header.cupsBytesPerLine);

    for (y = 0; !job->is_canceled && y < header.cupsHeight; y ++)
    {
      if (cupsRasterReadPixels(ras, pixels, header.cupsBytesPerLine))
      {
        if (header.cupsBitsPerPixel == 8 && options.header.cupsBitsPerPixel == 1)
        {
          // Dither the line...
	  dither = options.dither[y & 15];
	  memset(line, 0, options.header.cupsBytesPerLine);

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

          (printer->driver_data.rwrite)(job, &options, job->printer->device, y, line);
        }
        else
          (printer->driver_data.rwrite)(job, &options, job->printer->device, y, pixels);
      }
      else
        break;
    }

    free(pixels);
    free(line);

    if (!(printer->driver_data.rendpage)(job, &options, job->printer->device, page))
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

  if (!(printer->driver_data.rendjob)(job, &options, job->printer->device))
    job->state = IPP_JSTATE_ABORTED;
  else if (header_pages == 0)
    papplJobSetImpressions(job, (int)page);

  complete_job:

  if (httpGetState(client->http) == HTTP_STATE_POST_RECV)
  {
    // Flush excess data...
    char	buffer[8192];		// Read buffer

    while (httpRead2(client->http, buffer, sizeof(buffer)) > 0);
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
  pappl_options_t	options;	// Job options


  papplJobSetImpressions(job, 1);
  papplJobGetOptions(job, &options, 1, false);

  if (!(job->printer->driver_data.print)(job, &options, device))
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
  pthread_rwlock_wrlock(&job->rwlock);

  if (job->is_canceled)
    job->state = IPP_JSTATE_CANCELED;
  else if (job->state == IPP_JSTATE_PROCESSING)
    job->state = IPP_JSTATE_COMPLETED;

  job->completed               = time(NULL);
  job->printer->state          = IPP_PSTATE_IDLE;
  job->printer->state_time     = time(NULL);
  job->printer->processing_job = NULL;

  pthread_rwlock_unlock(&job->rwlock);

  pthread_rwlock_wrlock(&job->printer->rwlock);

  cupsArrayRemove(job->printer->active_jobs, job);
  cupsArrayAdd(job->printer->completed_jobs, job);

  job->printer->impcompleted += job->impcompleted;

  if (!job->system->clean_time)
    job->system->clean_time = time(NULL) + 60;

  pthread_rwlock_unlock(&job->printer->rwlock);

  _papplSystemConfigChanged(job->printer->system);

  if (job->printer->is_deleted)
  {
    papplPrinterDelete(job->printer);
  }
  else if (cupsArrayCount(job->printer->active_jobs) > 0)
  {
    _papplPrinterCheckJobs(job->printer);
  }
  else
  {
    pthread_rwlock_wrlock(&job->printer->rwlock);

    papplDeviceClose(job->printer->device);
    job->printer->device = NULL;

    pthread_rwlock_unlock(&job->printer->rwlock);
  }
}


//
// 'start_job()' - Start processing a job...
//

static void
start_job(pappl_job_t *job)		// I - Job
{
  bool	first_open = true;		// Is this the first time we try to open the device?


  // Move the job to the 'processing' state...
  pthread_rwlock_wrlock(&job->rwlock);

  job->state                   = IPP_JSTATE_PROCESSING;
  job->processing              = time(NULL);
  job->printer->processing_job = job;

  pthread_rwlock_wrlock(&job->rwlock);

  // Open the output device...
  pthread_rwlock_wrlock(&job->printer->rwlock);

  while (!job->printer->device)
  {
    job->printer->device = papplDeviceOpen(job->printer->device_uri, papplLogDevice, job->system);

    if (!job->printer->device)
    {
      // Log that the printer is unavailable then sleep for 5 seconds to retry.
      if (first_open)
      {
        papplLogPrinter(job->printer, PAPPL_LOGLEVEL_ERROR, "Unable to open device '%s', pausing queue until printer becomes available.", job->printer->device_uri);
        first_open = false;

	job->printer->state      = IPP_PSTATE_STOPPED;
	job->printer->state_time = time(NULL);
      }

      sleep(5);
    }
  }

  // Move the printer to the 'processing' state...
  job->printer->state      = IPP_PSTATE_PROCESSING;
  job->printer->state_time = time(NULL);

  pthread_rwlock_unlock(&job->printer->rwlock);
}
