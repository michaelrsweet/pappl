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

#include "pappl.h"
#include "job-private.h"
#include "system-private.h"
#ifdef HAVE_LIBJPEG
#  include <setjmp.h>
#  include <jpeglib.h>
#endif // HAVE_LIBJPEG
#ifdef HAVE_LIBPNG
#  include <png.h>
#endif // HAVE_LIBPNG


//
// Local types...
//

#ifdef HAVE_LIBJPEG
typedef struct _pappl_jpeg_err_s	// JPEG error manager extension
{
  struct jpeg_error_mgr	jerr;			// JPEG error manager information
  jmp_buf	retbuf;				// setjmp() return buffer
  char		message[JMSG_LENGTH_MAX];	// Last error message
} _pappl_jpeg_err_t;
#endif // HAVE_LIBJPEG


//
// Local functions...
//

#ifdef HAVE_LIBJPEG
static void	jpeg_error_handler(j_common_ptr p) _PAPPL_NORETURN;
#endif // HAVE_LIBJPEG
#ifdef HAVE_LIBPNG
static void	png_error_func(png_structp pp, png_const_charp message);
static void	png_warning_func(png_structp pp, png_const_charp message);
#endif // HAVE_LIBPNG


//
// 'papplJobFilterImage()' - Filter an image in memory.
//
// This function will print a grayscale or sRGB image using the printer's raster
// driver interface, scaling and positioning the image as necessary based on
// the job options, and printing as many copies as requested.
//
// The image data is an array of grayscale ("depth" = `1`) or sRGB
// ("depth" = `3`) pixels starting at the top-left corner of the image.
//
// The image resolution ("ppi") is expressed in pixels per inch and is used for
// some "print-scaling" modes.  Pass `0` if the image has no explicit resolution
// information.
//

bool					// O - `true` on success, `false` otherwise
papplJobFilterImage(
    pappl_job_t         *job,		// I - Job
    pappl_device_t      *device,	// I - Device
    pappl_pr_options_t  *options,	// I - Print options
    const unsigned char *pixels,	// I - Pointer to the top-left corner of the image data
    int                 width,		// I - Width in columns
    int                 height,		// I - Height in lines
    int                 depth,		// I - Bytes per pixel (`1` for grayscale or `3` for sRGB)
    int                 ppi,		// I - Pixels per inch (`0` for unknown)
    bool		smoothing)	// I - `true` to smooth/interpolate the image, `false` for nearest-neighbor sampling
{
  bool			started = false;// Have we started the job?
  pappl_pr_driver_data_t driver_data;	// Printer driver data
  const unsigned char	*dither;	// Dither line
  int			ileft,		// Imageable left margin
			itop,		// Imageable top margin
			iwidth,		// Imageable width
			iheight;	// Imageable length/height
  unsigned char		white,		// White color
			*line = NULL,	// Output line
			*lineptr,	// Pointer in line
			byte,		// Byte in line
			bit;		// Current bit
  const unsigned char	*pixbase,	// Pointer to first pixel
			*pixline,	// Pointer to start of current line
			*pixptr,	// Pointer into image
			*pixend;	// End of image
  int			pixel0,		// Temporary pixel value
			pixel1,		// ...
			img_width,	// Rotated image width
			img_height,	// Rotated image height
			x,		// X position
			xsize,		// Scaled width
			xstart,		// X start position
			xend,		// X end position
			y,		// Y position
			ysize,		// Scaled height
			ystart,		// Y start position
			yend;		// Y end position
  int			xdir,		// X direction
			xerr,		// X error accumulator
			xmod,		// X modulus
			xstep,		// X step
			yerr,		// Y error accumulator
			ymod,		// Y modulus
			ystep,		// Y step
			ydir;		// Y direction


  // Images contain a single page/impression...
  papplJobSetImpressions(job, 1);

  if (options->print_scaling == PAPPL_SCALING_FILL)
  {
    // Scale to fill the entire media area...
    ileft   = 0;
    itop    = 0;
    iwidth  = (int)options->header.cupsWidth;
    iheight = (int)options->header.cupsHeight;
  }
  else
  {
    // Scale/center within the margins...
    ileft   = options->media.left_margin * options->printer_resolution[0] / 2540;
    itop    = options->media.top_margin * options->printer_resolution[1] / 2540;
    iwidth  = (int)options->header.cupsWidth - (options->media.left_margin + options->media.right_margin) * options->printer_resolution[0] / 2540;
    iheight = (int)options->header.cupsHeight - (options->media.bottom_margin + options->media.top_margin) * options->printer_resolution[1] / 2540;
  }

  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "ileft=%d, itop=%d, iwidth=%d, iheight=%d", ileft, itop, iwidth, iheight);

  if (iwidth <= 0 || iheight <= 0)
  {
    papplLogJob(job, PAPPL_LOGLEVEL_ERROR, "Invalid media size");
    return (false);
  }

  // Figure out the scaling and rotation of the image...
  if (options->orientation_requested == IPP_ORIENT_NONE)
  {
    if (width > height && options->header.cupsWidth < options->header.cupsHeight)
    {
      options->orientation_requested = IPP_ORIENT_LANDSCAPE;
      papplLogJob(job, PAPPL_LOGLEVEL_INFO, "Auto-orientation: landscape");
    }
    else
    {
      options->orientation_requested = IPP_ORIENT_PORTRAIT;
      papplLogJob(job, PAPPL_LOGLEVEL_INFO, "Auto-orientation: portrait");
    }
  }

  if (options->print_scaling == PAPPL_SCALING_AUTO || options->print_scaling == PAPPL_SCALING_AUTO_FIT)
  {
    if (ppi <= 0)
    {
      // No resolution information, so just force scaling the image to fit/fill
      xsize = iwidth + 1;
      ysize = iheight + 1;
    }
    else if (options->orientation_requested == IPP_ORIENT_PORTRAIT || options->orientation_requested == IPP_ORIENT_REVERSE_PORTRAIT)
    {
      xsize = width * options->printer_resolution[0] / ppi;
      ysize = height * options->printer_resolution[1] / ppi;
    }
    else
    {
      xsize = height * options->printer_resolution[0] / ppi;
      ysize = width * options->printer_resolution[1] / ppi;
    }

    if (xsize > iwidth || ysize > iheight)
    {
      // Scale to fit/fill based on "print-scaling" and margins...
      if (options->print_scaling == PAPPL_SCALING_AUTO && options->media.bottom_margin == 0 && options->media.left_margin == 0 && options->media.right_margin == 0 && options->media.top_margin == 0)
        options->print_scaling = PAPPL_SCALING_FILL;
      else
        options->print_scaling = PAPPL_SCALING_FIT;
    }
    else
    {
      // Do no scaling...
      options->print_scaling = PAPPL_SCALING_NONE;
    }
  }
  else if (options->print_scaling == PAPPL_SCALING_NONE && ppi <= 0)
  {
    // Force a default PPI value of 200, which fits a typical 1080p sized
    // screenshot on a standard letter/A4 page.
    ppi = 200;
  }

  switch (options->orientation_requested)
  {
    default :
    case IPP_ORIENT_PORTRAIT :
        pixbase    = pixels;
        img_width  = width;
        img_height = height;
        xdir       = (int)depth;
        ydir       = (int)depth * (int)width;

        if (options->print_scaling == PAPPL_SCALING_NONE)
        {
          // No scaling
	  xsize = img_width * options->printer_resolution[0] / ppi;
	  ysize = img_height * options->printer_resolution[1] / ppi;
        }
        else
	{
	  // Fit/fill
	  xsize = iwidth;
	  ysize = xsize * height / width;

	  if ((ysize > iheight && options->print_scaling == PAPPL_SCALING_FIT) || (ysize < iheight && options->print_scaling == PAPPL_SCALING_FILL))
	  {
	    ysize = iheight;
	    xsize = ysize * width / height;
	  }
	}
	break;

    case IPP_ORIENT_REVERSE_PORTRAIT :
        pixbase    = pixels + depth * width * height - depth;
        img_width  = width;
        img_height = height;
        xdir       = -(int)depth;
        ydir       = -(int)depth * (int)width;

        if (options->print_scaling == PAPPL_SCALING_NONE)
        {
          // No scaling
	  xsize = img_width * options->printer_resolution[0] / ppi;
	  ysize = img_height * options->printer_resolution[1] / ppi;
        }
        else
	{
	  // Fit/fill
	  xsize = iwidth;
	  ysize = xsize * height / width;

	  if ((ysize > iheight && options->print_scaling == PAPPL_SCALING_FIT) || (ysize < iheight && options->print_scaling == PAPPL_SCALING_FILL))
	  {
	    ysize = iheight;
	    xsize = ysize * width / height;
	  }
	}
	break;

    case IPP_ORIENT_LANDSCAPE : // 90 counter-clockwise
        pixbase    = pixels + depth * width - depth;
        img_width  = height;
        img_height = width;
        xdir       = (int)depth * (int)width;
        ydir       = -(int)depth;

        if (options->print_scaling == PAPPL_SCALING_NONE)
        {
          // No scaling
	  xsize = img_width * options->printer_resolution[0] / ppi;
	  ysize = img_height * options->printer_resolution[1] / ppi;
        }
        else
	{
	  // Fit/fill
	  xsize = iwidth;
	  ysize = xsize * width / height;

	  if ((ysize > iheight && options->print_scaling == PAPPL_SCALING_FIT) || (ysize < iheight && options->print_scaling == PAPPL_SCALING_FILL))
	  {
	    ysize = iheight;
	    xsize = ysize * height / width;
	  }
	}
	break;

    case IPP_ORIENT_REVERSE_LANDSCAPE : // 90 clockwise
        pixbase    = pixels + depth * (height - 1) * width;
        img_width  = height;
        img_height = width;
        xdir       = -(int)depth * (int)width;
        ydir       = (int)depth;

        if (options->print_scaling == PAPPL_SCALING_NONE)
        {
          // No scaling
	  xsize = img_width * options->printer_resolution[0] / ppi;
	  ysize = img_height * options->printer_resolution[1] / ppi;
        }
        else
	{
	  // Fit/fill
	  xsize = iwidth;
	  ysize = xsize * width / height;

	  if ((ysize > iheight && options->print_scaling == PAPPL_SCALING_FIT) || (ysize < iheight && options->print_scaling == PAPPL_SCALING_FILL))
	  {
	    ysize = iheight;
	    xsize = ysize * height / width;
	  }
	}
        break;
  }

  // Don't rotate in the driver...
  options->orientation_requested = IPP_ORIENT_PORTRAIT;

  xstart = ileft + (iwidth - xsize) / 2;
  xend   = xstart + xsize;
  ystart = itop + (iheight - ysize) / 2;
  yend   = ystart + ysize;

  xmod   = (int)(img_width % xsize);
  xstep  = (int)(img_width / xsize) * xdir;

  ymod   = (int)(img_height % ysize);
  ystep  = (int)(img_height / ysize) * ydir;

  if (xend > (int)options->header.cupsWidth)
    xend = (int)options->header.cupsWidth;

  if (yend > (int)options->header.cupsHeight)
    yend = (int)options->header.cupsHeight;

  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "xsize=%d, xstart=%d, xend=%d, xdir=%d, xmod=%d, xstep=%d", xsize, xstart, xend, xdir, xmod, xstep);
  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "ysize=%d, ystart=%d, yend=%d, ydir=%d, ymod=%d, ystep=%d", ysize, ystart, yend, ydir, ymod, ystep);

  papplPrinterGetDriverData(papplJobGetPrinter(job), &driver_data);

  if ((line = malloc(options->header.cupsBytesPerLine)) == NULL)
  {
    papplLogJob(job, PAPPL_LOGLEVEL_ERROR, "Unable to allocate memory for raster line.");
    goto abort_job;
  }

  // Start the job...
  if (!(driver_data.rstartjob_cb)(job, options, device))
  {
    papplLogJob(job, PAPPL_LOGLEVEL_ERROR, "Unable to start raster job.");
    goto abort_job;
  }

  started = true;

  if (options->header.cupsColorSpace == CUPS_CSPACE_K || options->header.cupsColorSpace == CUPS_CSPACE_CMYK)
    white = 0x00;
  else
    white = 0xff;

  pixend = pixels + width * height * depth;

  // Print every copy...
  while (papplJobGetCopiesCompleted(job) < papplJobGetCopies(job))
  {
    if (papplJobGetState(job) != IPP_JSTATE_PROCESSING || papplJobIsCanceled(job))
      break;

    if (!(driver_data.rstartpage_cb)(job, options, device, 1))
    {
      papplLogJob(job, PAPPL_LOGLEVEL_ERROR, "Unable to start raster page.");
      goto abort_job;
    }

    // Leading blank space...
    memset(line, white, options->header.cupsBytesPerLine);
    for (y = 0; y < ystart; y ++)
    {
      if (!(driver_data.rwriteline_cb)(job, options, device, (unsigned)y, line))
      {
	papplLogJob(job, PAPPL_LOGLEVEL_ERROR, "Unable to write raster line %u.", y);
	goto abort_job;
      }
    }

    if (ystart < 0)
    {
      pixline = pixbase - (ystart * ymod / ysize) * ydir;
      yerr    = -ymod / 2 - (ystart * ymod) % ysize;
    }
    else
    {
      pixline = pixbase;
      yerr    = -ymod / 2;
    }

    // Now RIP the image...
    for (; y < yend && !job->is_canceled; y ++)
    {
      pixptr = pixline;

      if (xstart < 0)
      {
	pixptr -= (xstart * xmod / xsize) * xdir;
	x    = 0;
	xerr = -xmod / 2 - (xstart * xmod) % xsize;
      }
      else
      {
	x    = xstart;
	xerr = -xmod / 2;
      }

      if (options->header.cupsBitsPerPixel == 1)
      {
        // Need to dither the image to 1-bit black...
	dither = options->dither[y & 15];

	for (lineptr = line + x / 8, bit = 128 >> (x & 7), byte = 0; x < xend; x ++)
	{
	  // Dither the current pixel...
	  if (*pixptr <= dither[x & 15])
	    byte |= bit;

	  // Advance to the next pixel...
	  pixptr += xstep;
	  xerr += xmod;
	  if (xerr >= (int)xsize)
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
      else if (options->header.cupsColorSpace == CUPS_CSPACE_K)
      {
        // Need to invert the image...
	for (lineptr = line + x; x < xend; x ++)
	{
	  // Copy an inverted grayscale pixel...
	  if (smoothing && yerr >= 0 && xerr >= 0)
	  {
	    const unsigned char	*rt = pixptr + xdir,
				*dn = pixptr + ydir,
				*dnrt = pixptr + xdir + ydir;
					// Pointers to adjacent pixels

	    if (rt < pixels || rt >= pixend)
	      rt = pixptr;
	    if (dn < pixels || dn >= pixend)
	      dn = pixptr;
	    if (dnrt < pixels || dnrt >= pixend)
	      dnrt = pixptr;

	    pixel0     = ((xsize - xerr) * *pixptr + xerr * *rt) / xsize;
            pixel1     = ((xsize - xerr) * *dn + xerr * *dnrt) / xsize;
	    *lineptr++ = (unsigned char)(255 - ((ysize - yerr) * pixel0 + yerr * pixel1) / ysize);
	  }
	  else
	  {
	    *lineptr++ = ~*pixptr;
	  }

	  // Advance to the next pixel...
	  pixptr += xstep;
	  xerr += xmod;
	  if (xerr >= (int)xsize)
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
        int bpp = (int)options->header.cupsBitsPerPixel / 8;

	for (lineptr = line + x * bpp; x < xend; x ++)
	{
	  // Copy a grayscale or RGB pixel...
	  if (smoothing && yerr >= 0 && xerr >= 0)
	  {
	    int			j;	// Looping var
	    const unsigned char	*rt = pixptr + xdir,
				*dn = pixptr + ydir,
				*dnrt = pixptr + xdir + ydir;
					// Pointers to adjacent pixels

	    if (rt < pixels || rt >= pixend)
	      rt = pixptr;
	    if (dn < pixels || dn >= pixend)
	      dn = pixptr;
	    if (dnrt < pixels || dnrt >= pixend)
	      dnrt = pixptr;

            for (j = 0; j < bpp; j ++)
            {
	      pixel0     = ((xsize - xerr) * pixptr[j] + xerr * rt[j]) / xsize;
              pixel1     = ((xsize - xerr) * dn[j] + xerr * dnrt[j]) / xsize;
	      *lineptr++ = (unsigned char)(((ysize - yerr) * pixel0 + yerr * pixel1) / ysize);
	    }
	  }
	  else
	  {
	    memcpy(lineptr, pixptr, (unsigned)bpp);
	    lineptr += bpp;
	  }

	  // Advance to the next pixel...
	  pixptr += xstep;
	  xerr += xmod;
	  if (xerr >= (int)xsize)
	  {
	    // Accumulated error has overflowed, advance another pixel...
	    xerr -= xsize;
	    pixptr += xdir;
	  }
	}
      }

      if (!(driver_data.rwriteline_cb)(job, options, device, (unsigned)y, line))
      {
	papplLogJob(job, PAPPL_LOGLEVEL_ERROR, "Unable to write raster line %u.", y);
	goto abort_job;
      }

      pixline += ystep;
      yerr += ymod;
      if (yerr >= ysize)
      {
        pixline += ydir;
        yerr -= ysize;
      }
    }

    // Trailing blank space...
    memset(line, white, options->header.cupsBytesPerLine);
    for (; y < (int)options->header.cupsHeight; y ++)
    {
      if (!(driver_data.rwriteline_cb)(job, options, device, (unsigned)y, line))
      {
	papplLogJob(job, PAPPL_LOGLEVEL_ERROR, "Unable to write raster line %u.", y);
	goto abort_job;
      }
    }

    // End the page...
    if (!(driver_data.rendpage_cb)(job, options, device, 1))
    {
      papplLogJob(job, PAPPL_LOGLEVEL_ERROR, "Unable to end raster page.");
      goto abort_job;
    }

    papplJobSetImpressionsCompleted(job, 1);
    papplJobSetCopiesCompleted(job, 1);
  }

  // End the job...
  if (!(driver_data.rendjob_cb)(job, options, device))
  {
    papplLogJob(job, PAPPL_LOGLEVEL_ERROR, "Unable to end raster job.");
    goto abort_job;
  }

  // Free memory and return...
  free(line);

  return (true);

  // Abort the job...
  abort_job:

  if (started)
    (driver_data.rendjob_cb)(job, options, device);

  free(line);

  return (false);
}


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
  const char		*filename;	// JPEG filename
  FILE			*fp;		// JPEG file
  pappl_pr_options_t	*options = NULL;// Job options
  struct jpeg_decompress_struct	dinfo;	// Decompressor info
  int			xdpi,		// X pixels per inch
			ydpi;		// Y pixels per inch
  _pappl_jpeg_err_t	jerr;		// Error handler info
  unsigned char		*pixels = NULL;	// Image pixels
  JSAMPROW		row;		// Sample row pointer
  bool			ret = false;	// Return value


  (void)data;

  // Open the JPEG file...
  filename = papplJobGetFilename(job);
  if ((fp = fopen(filename, "rb")) == NULL)
  {
    papplLogJob(job, PAPPL_LOGLEVEL_ERROR, "Unable to open JPEG file '%s': %s", filename, strerror(errno));
    return (false);
  }

  // Read the image header...
  jpeg_std_error(&jerr.jerr);
  jerr.jerr.error_exit = jpeg_error_handler;

  if (setjmp(jerr.retbuf))
  {
    // JPEG library errors are directed to this point...
    papplJobSetReasons(job, PAPPL_JREASON_DOCUMENT_FORMAT_ERROR, PAPPL_JREASON_NONE);
    papplLogJob(job, PAPPL_LOGLEVEL_ERROR, "Unable to open JPEG file '%s': %s", filename, jerr.message);
    ret = false;
    goto finish_jpeg;
  }

  dinfo.err = (struct jpeg_error_mgr *)&jerr;
  jpeg_create_decompress(&dinfo);
  jpeg_stdio_src(&dinfo, fp);
  jpeg_read_header(&dinfo, TRUE);

  // Get job options and request the image data in the format we need...
  options = papplJobCreatePrintOptions(job, 1, dinfo.num_components > 1);

  dinfo.quantize_colors = FALSE;

  if (options->header.cupsNumColors == 1)
  {
    dinfo.out_color_space      = JCS_GRAYSCALE;
    dinfo.out_color_components = 1;
    dinfo.output_components    = 1;
  }
  else
  {
    dinfo.out_color_space      = JCS_RGB;
    dinfo.out_color_components = 3;
    dinfo.output_components    = 3;
  }

  jpeg_calc_output_dimensions(&dinfo);

  papplLogJob(job, PAPPL_LOGLEVEL_INFO, "JPEG image dimensions are %ux%ux%d", dinfo.output_width, dinfo.output_height, dinfo.output_components);

  if (dinfo.output_width < 1 || dinfo.output_width > (JDIMENSION)job->system->max_image_width || dinfo.output_height < 1 || dinfo.output_height > (JDIMENSION)job->system->max_image_height || ((size_t)dinfo.output_width * (size_t)dinfo.output_height * (size_t)dinfo.output_components) > job->system->max_image_size)
  {
    papplLogJob(job, PAPPL_LOGLEVEL_ERROR, "JPEG image is too large to print.");
    papplJobSetReasons(job, PAPPL_JREASON_DOCUMENT_UNPRINTABLE_ERROR, PAPPL_JREASON_NONE);
    goto finish_jpeg;
  }

  if ((pixels = (unsigned char *)malloc((size_t)dinfo.output_width * (size_t)dinfo.output_height * (size_t)dinfo.output_components)) == NULL)
  {
    papplLogJob(job, PAPPL_LOGLEVEL_ERROR, "Unable to allocate memory for %ux%ux%d JPEG image.", dinfo.output_width, dinfo.output_height, dinfo.output_components);
    papplJobSetReasons(job, PAPPL_JREASON_ERRORS_DETECTED, PAPPL_JREASON_NONE);
    goto finish_jpeg;
  }

  jpeg_start_decompress(&dinfo);

  while (dinfo.output_scanline < dinfo.output_height)
  {
    row = (JSAMPROW)(pixels + (size_t)dinfo.output_scanline * (size_t)dinfo.output_width * (size_t)dinfo.output_components);
    jpeg_read_scanlines(&dinfo, &row, 1);
  }

  switch (dinfo.density_unit)
  {
    default :
    case 0 : // Unknown units
	xdpi = ydpi = 0;
	break;
    case 1 : // Dots-per-inch
	xdpi = dinfo.X_density;
	ydpi = dinfo.Y_density;
	break;
    case 2 : // Dots-per-centimeter
	xdpi = dinfo.X_density * 254 / 100;
	ydpi = dinfo.Y_density * 254 / 100;
	break;
  }

  papplLogJob(job, PAPPL_LOGLEVEL_INFO, "JPEG image resolution is %dx%ddpi", xdpi, ydpi);
  if (xdpi != ydpi)
  {
    papplLogJob(job, PAPPL_LOGLEVEL_ERROR, "JPEG image has non-square aspect ratio - not currently supported.");
    papplJobSetReasons(job, PAPPL_JREASON_DOCUMENT_UNPRINTABLE_ERROR, PAPPL_JREASON_NONE);
    goto finish_jpeg;
  }

  ret = papplJobFilterImage(job, device, options, pixels, (int)dinfo.output_width, (int)dinfo.output_height, dinfo.output_components, xdpi, true);

  jpeg_finish_decompress(&dinfo);

  finish_jpeg:

  papplJobDeletePrintOptions(options);
  free(pixels);
  jpeg_destroy_decompress(&dinfo);
  fclose(fp);

  return (ret);
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
  const char		*filename;	// Job filename
  FILE			*fp;		// PNG file
  pappl_pr_options_t	*options = NULL;// Job options
  png_structp		pp = NULL;	// PNG read pointer
  png_infop		info = NULL;	// PNG info pointers
  png_bytep		*rows = NULL;	// PNG row pointers
  png_color_16		bg;		// Background color
  int			i,		// Looping var
			color_type,	// PNG color mode
			width,		// Width in columns
			height,		// Height in lines
			depth,		// Bytes per pixel
			xdpi,		// X resolution
			ydpi;		// Y resolution
  unsigned char		*pixels = NULL;	// Image pixels
  bool			ret = false;	// Return value


  // Open the PNG file...
  (void)data;

  filename = papplJobGetFilename(job);
  if ((fp = fopen(filename, "rb")) == NULL)
  {
    papplLogJob(job, PAPPL_LOGLEVEL_ERROR, "Unable to open PNG file '%s': %s", filename, strerror(errno));
    return (false);
  }

  // Setup PNG data structures...
  if ((pp = png_create_read_struct(PNG_LIBPNG_VER_STRING, (png_voidp)job, png_error_func, png_warning_func)) == NULL)
  {
    papplJobSetReasons(job, PAPPL_JREASON_DOCUMENT_FORMAT_ERROR, PAPPL_JREASON_NONE);
    papplLogJob(job, PAPPL_LOGLEVEL_ERROR, "Unable to allocate memory for PNG file '%s': %s", job->filename, strerror(errno));
    goto finish_png;
  }

  if ((info = png_create_info_struct(pp)) == NULL)
  {
    papplJobSetReasons(job, PAPPL_JREASON_DOCUMENT_FORMAT_ERROR, PAPPL_JREASON_NONE);
    papplLogJob(job, PAPPL_LOGLEVEL_ERROR, "Unable to allocate memory for PNG file '%s': %s", job->filename, strerror(errno));
    goto finish_png;
  }

  if (setjmp(png_jmpbuf(pp)))
  {
    // If we get here, PNG loading failed and any errors/warnings were logged
    // via the corresponding callback functions...
    papplJobSetReasons(job, PAPPL_JREASON_DOCUMENT_FORMAT_ERROR, PAPPL_JREASON_NONE);
    goto finish_png;
  }

  // Start reading...
  png_init_io(pp, fp);

#  if defined(PNG_SKIP_sRGB_CHECK_PROFILE) && defined(PNG_SET_OPTION_SUPPORTED)
  // Don't throw errors with "invalid" sRGB profiles produced by Adobe apps.
  png_set_option(pp, PNG_SKIP_sRGB_CHECK_PROFILE, PNG_OPTION_ON);
#  endif // PNG_SKIP_sRGB_CHECK_PROFILE && PNG_SET_OPTION_SUPPORTED

  // Get the image dimensions and depth...
  png_read_info(pp, info);

  width      = (int)png_get_image_width(pp, info);
  height     = (int)png_get_image_height(pp, info);
  color_type = png_get_color_type(pp, info);

  if (color_type & PNG_COLOR_MASK_COLOR)
    depth = 3;
  else
    depth = 1;

  papplLogJob(job, PAPPL_LOGLEVEL_INFO, "PNG image dimensions are %dx%dx%d", width, height, depth);

  if (width < 1 || width > job->system->max_image_width || height < 1 || height > job->system->max_image_height || (size_t)(width * height * depth) > job->system->max_image_size)
  {
    papplLogJob(job, PAPPL_LOGLEVEL_ERROR, "PNG image is too large to print.");
    papplJobSetReasons(job, PAPPL_JREASON_DOCUMENT_UNPRINTABLE_ERROR, PAPPL_JREASON_NONE);
    goto finish_png;
  }

  xdpi = (int)png_get_x_pixels_per_inch(pp, info);
  ydpi = (int)png_get_y_pixels_per_inch(pp, info);

  papplLogJob(job, PAPPL_LOGLEVEL_INFO, "PNG image resolution is %dx%ddpi", xdpi, ydpi);

  if (xdpi != ydpi)
  {
    papplLogJob(job, PAPPL_LOGLEVEL_ERROR, "PNG image has non-square aspect ratio - not currently supported.");
    papplJobSetReasons(job, PAPPL_JREASON_DOCUMENT_UNPRINTABLE_ERROR, PAPPL_JREASON_NONE);
    goto finish_png;
  }

  // Set decoding options...
  if (png_get_valid(pp, info, PNG_INFO_tRNS))
  {
    // Map transparency to alpha
    png_set_tRNS_to_alpha(pp);
    color_type |= PNG_COLOR_MASK_ALPHA;
  }

#ifdef PNG_TRANSFORM_SCALE_16
  if (png_get_bit_depth(pp, info) > 8)
  {
    // Scale 16-bit values to 8-bit gamma-corrected ones
    png_set_scale_16(pp);
    papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "Scaling 16-bit PNG data to 8-bits.");
  }
#else
  if (png_get_bit_depth(pp, info) > 8)
  {
    // Strip the bottom bits of 16-bit values
    png_set_strip_16(pp);
    papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "Stripping 16-bit PNG data to 8-bits.");
  }
#endif // PNG_TRANSFORM_SCALE_16

  if (png_get_bit_depth(pp, info) < 8)
  {
    // Expand 1, 2, and 4-bit values to 8 bits
    if (depth == 1)
      png_set_expand_gray_1_2_4_to_8(pp);
    else
      png_set_packing(pp);
  }
  if (color_type & PNG_COLOR_MASK_PALETTE)
  {
    // Convert indexed images to RGB...
    png_set_palette_to_rgb(pp);
  }

  // Remove alpha by compositing over white...
  bg.red = bg.green = bg.blue = 65535;
  png_set_background(pp, &bg, PNG_BACKGROUND_GAMMA_SCREEN, 0, 1);

  // Allocate memory for the image...
  if ((pixels = (unsigned char *)calloc(1, (size_t)(width * height * depth))) == NULL)
  {
    papplLogJob(job, PAPPL_LOGLEVEL_ERROR, "Unable to allocate memory for PNG image: %s", strerror(errno));
    papplJobSetReasons(job, PAPPL_JREASON_DOCUMENT_UNPRINTABLE_ERROR, PAPPL_JREASON_NONE);
    goto finish_png;
  }

  if ((rows = (png_bytep *)calloc((size_t)height, sizeof(png_bytep))) == NULL)
  {
    papplLogJob(job, PAPPL_LOGLEVEL_ERROR, "Unable to allocate memory for PNG image: %s", strerror(errno));
    papplJobSetReasons(job, PAPPL_JREASON_DOCUMENT_UNPRINTABLE_ERROR, PAPPL_JREASON_NONE);
    goto finish_png;
  }

  for (i = 0; i < height; i ++)
    rows[i] = pixels + i * width * depth;

  // Read the image...
  for (i = png_set_interlace_handling(pp); i > 0; i --)
    png_read_rows(pp, rows, NULL, (png_uint_32)height);

  // Prepare options...
  options = papplJobCreatePrintOptions(job, 1, depth == 3);

  // Print the image...
  ret = papplJobFilterImage(job, device, options, pixels, width, height, depth, (int)png_get_x_pixels_per_inch(pp, info), false);

  // Finish up...
  finish_png:

  if (pp && info)
  {
    png_read_end(pp, info);
    png_destroy_read_struct(&pp, &info, NULL);
    pp   = NULL;
    info = NULL;
  }

  fclose(fp);
  fp = NULL;

  papplJobDeletePrintOptions(options);
  options = NULL;

  free(pixels);
  pixels = NULL;

  free(rows);
  rows = NULL;

  return (ret);
}
#endif // HAVE_LIBPNG


#ifdef HAVE_LIBJPEG
//
// 'jpeg_error_handler()' - Handle JPEG errors by not exiting.
//

static void
jpeg_error_handler(j_common_ptr p)	// I - JPEG data
{
  _pappl_jpeg_err_t	*jerr = (_pappl_jpeg_err_t *)p->err;
					// JPEG error handler


  // Save the error message in the string buffer...
  (jerr->jerr.format_message)(p, jerr->message);

  // Return to the point we called setjmp()...
  longjmp(jerr->retbuf, 1);
}
#endif // HAVE_LIBJPEG


#ifdef HAVE_LIBPNG
//
// 'png_error_func()' - PNG error message function.
//

static void
png_error_func(
    png_structp     pp,			// I - PNG pointer
    png_const_charp message)		// I - Error message
{
  pappl_job_t	*job = (pappl_job_t *)png_get_error_ptr(pp);
					// Job


  papplLogJob(job, PAPPL_LOGLEVEL_ERROR, "PNG: %s", message);
}


//
// 'png_warning_func()' - PNG warning message function.
//

static void
png_warning_func(
    png_structp     pp,			// I - PNG pointer
    png_const_charp message)		// I - Error message
{
  pappl_job_t	*job = (pappl_job_t *)png_get_error_ptr(pp);
					// Job


  papplLogJob(job, PAPPL_LOGLEVEL_WARN, "PNG: %s", message);
}
#endif // HAVE_LIBPNG
