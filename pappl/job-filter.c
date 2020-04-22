//
// Job MIME filter functions for the Printer Application Framework
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
