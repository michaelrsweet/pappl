//
// PWG test scanner driver for the Printer Application Framework
//
// Copyright © 2024-2026 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#define PWG_SC_DRIVER 1
#include <pappl/pappl-private.h>
#include "testpappl.h"
#include <math.h>


//
// Local types...
//

typedef struct pwg_scan_job_s		// PWG scan job data
{
  unsigned		width_pixels;	// Width in pixels
  unsigned		height_pixels;	// Height in pixels
  unsigned		bpp;		// Bytes per pixel
  unsigned		page;		// Current page number
  unsigned		max_pages;	// Max pages for ADF (0 = platen)
  bool			canceled;	// Canceled flag
} pwg_scan_job_data_t;


//
// Local functions...
//

static bool	pwg_sc_rendjob(pappl_job_t *job, pappl_sc_options_t *options, pappl_device_t *device);
static bool	pwg_sc_rendpage(pappl_job_t *job, pappl_sc_options_t *options, pappl_device_t *device, unsigned page);
static bool	pwg_sc_rstartjob(pappl_job_t *job, pappl_sc_options_t *options, pappl_device_t *device);
static bool	pwg_sc_rstartpage(pappl_job_t *job, pappl_sc_options_t *options, pappl_device_t *device, unsigned page);
static bool	pwg_sc_rreadline(pappl_job_t *job, pappl_sc_options_t *options, pappl_device_t *device, unsigned y, unsigned char *line, size_t linesize);
static bool	pwg_sc_status(pappl_scanner_t *scanner);


//
// 'pwg_sc_autoadd()' - Auto-add callback for scanner.
//

const char *				// O - Driver name or `NULL`
pwg_sc_autoadd(
    const char *device_info,		// I - Device info (not used)
    const char *device_uri,		// I - Device URI (not used)
    const char *device_id,		// I - IEEE-1284 device ID
    void       *data)			// I - Callback data (not used)
{
  size_t	num_did;		// Number of device ID pairs
  cups_option_t	*did;			// Device ID pairs
  const char	*cmd,			// Command set value
		*ret = NULL;		// Return value


  (void)device_info;
  (void)device_uri;
  (void)data;

  num_did = papplDeviceParseID(device_id, &did);

  if ((cmd = cupsGetOption("COMMAND SET", num_did, did)) == NULL)
    cmd = cupsGetOption("CMD", num_did, did);

  if (cmd && strstr(cmd, "eSCL") != NULL)
    ret = "pwg_scanner";

  cupsFreeOptions(num_did, did);

  return (ret);
}


//
// 'pwg_sc_callback()' - Scanner driver callback.
//

bool					// O - `true` on success, `false` on failure
pwg_sc_callback(
    pappl_system_t         *system,	// I - System
    const char             *driver_name,// I - Driver name
    const char             *device_uri,	// I - Device URI
    const char             *device_id,	// I - IEEE-1284 device ID (not used)
    pappl_sc_driver_data_t *driver_data,// O - Driver data
    ipp_t                  **driver_attrs,
					// O - Driver attributes
    void                   *data)	// I - Callback data
{
  (void)device_id;
  (void)driver_attrs;

  if (!driver_name || !device_uri || !driver_data)
  {
    papplLog(system, PAPPL_LOGLEVEL_ERROR, "Scanner driver callback called without required information.");
    return (false);
  }

  if (!data || (strcmp((const char *)data, "testpappl") && strcmp((const char *)data, "testmainloop")))
  {
    papplLog(system, PAPPL_LOGLEVEL_ERROR, "Scanner driver callback called with bad data pointer.");
    return (false);
  }

  if (!strcmp(driver_name, "pwg_scanner") || !strcmp(driver_name, "pwg_scanner_adf"))
  {
    // Set identity...
    cupsCopyString(driver_data->make_and_model, "PWG Test Scanner", sizeof(driver_data->make_and_model));

    // Set callbacks...
    driver_data->rstartjob_cb  = pwg_sc_rstartjob;
    driver_data->rstartpage_cb = pwg_sc_rstartpage;
    driver_data->rreadline_cb  = pwg_sc_rreadline;
    driver_data->rendpage_cb   = pwg_sc_rendpage;
    driver_data->rendjob_cb    = pwg_sc_rendjob;
    driver_data->status_cb     = pwg_sc_status;

    // Set color modes...
    driver_data->color_supported = PAPPL_SCAN_COLOR_MODE_RGB_24 |
                                   PAPPL_SCAN_COLOR_MODE_GRAYSCALE_8 |
                                   PAPPL_SCAN_COLOR_MODE_BLACK_AND_WHITE_1;
    driver_data->color_default   = PAPPL_SCAN_COLOR_MODE_RGB_24;

    // Set intents (all mandatory per eSCL spec)...
    driver_data->intents_supported = PAPPL_SCAN_INTENT_DOCUMENT |
                                     PAPPL_SCAN_INTENT_PHOTO |
                                     PAPPL_SCAN_INTENT_PREVIEW |
                                     PAPPL_SCAN_INTENT_TEXT_AND_GRAPHIC;
    driver_data->intent_default    = PAPPL_SCAN_INTENT_DOCUMENT;

    // Set content types...
    driver_data->content_supported = PAPPL_SCAN_CONTENT_AUTO |
                                     PAPPL_SCAN_CONTENT_PHOTO |
                                     PAPPL_SCAN_CONTENT_TEXT |
                                     PAPPL_SCAN_CONTENT_TEXT_AND_PHOTO;
    driver_data->content_default   = PAPPL_SCAN_CONTENT_AUTO;

    // Set resolutions (75, 150, 300, 600 DPI)...
    driver_data->num_resolution = 4;
    driver_data->x_resolution[0] = 75;
    driver_data->y_resolution[0] = 75;
    driver_data->x_resolution[1] = 150;
    driver_data->y_resolution[1] = 150;
    driver_data->x_resolution[2] = 300;
    driver_data->y_resolution[2] = 300;
    driver_data->x_resolution[3] = 600;
    driver_data->y_resolution[3] = 600;
    driver_data->x_default       = 300;
    driver_data->y_default       = 300;

    // Set document formats (JPEG + PDF mandatory per eSCL spec)...
    driver_data->num_format = 2;
    cupsCopyString(driver_data->format[0], "image/jpeg", sizeof(driver_data->format[0]));
    cupsCopyString(driver_data->format[1], "application/pdf", sizeof(driver_data->format[1]));

    // Set platen scan area: US Letter (8.5" x 11" in 1/300")...
    driver_data->platen_max_width  = 2550;	// 8.5" * 300
    driver_data->platen_max_height = 3300;	// 11" * 300
    driver_data->platen_min_width  = 75;	// 0.25" * 300
    driver_data->platen_min_height = 75;	// 0.25" * 300

    if (!strcmp(driver_name, "pwg_scanner_adf"))
    {
      // ADF-capable scanner...
      driver_data->input_sources_supported = PAPPL_SCAN_INPUT_SOURCE_PLATEN |
                                             PAPPL_SCAN_INPUT_SOURCE_ADF;
      driver_data->input_source_default    = PAPPL_SCAN_INPUT_SOURCE_PLATEN;
      driver_data->duplex_supported        = true;
      driver_data->adf_max_width           = 2550;	// 8.5" * 300
      driver_data->adf_max_height          = 4200;	// 14" * 300
    }
    else
    {
      // Platen-only scanner...
      driver_data->input_sources_supported = PAPPL_SCAN_INPUT_SOURCE_PLATEN;
      driver_data->input_source_default    = PAPPL_SCAN_INPUT_SOURCE_PLATEN;
      driver_data->duplex_supported        = false;
    }

    // Set adjustment support...
    driver_data->brightness_supported = 1;
    driver_data->contrast_supported   = 1;
    driver_data->sharpen_supported    = 0;
    driver_data->threshold_supported  = 1;

    return (true);
  }
  else
  {
    papplLog(system, PAPPL_LOGLEVEL_ERROR, "Unsupported scanner driver name '%s'.", driver_name);
    return (false);
  }
}


//
// 'pwg_sc_rendjob()' - End a scan job.
//

static bool				// O - `true` on success
pwg_sc_rendjob(
    pappl_job_t        *job,		// I - Job
    pappl_sc_options_t *options,	// I - Scan options
    pappl_device_t     *device)		// I - Device (not used)
{
  pwg_scan_job_data_t	*data;		// Job data


  (void)options;
  (void)device;

  data = (pwg_scan_job_data_t *)papplJobGetData(job);
  if (data)
  {
    free(data);
    papplJobSetData(job, NULL);
  }

  return (true);
}


//
// 'pwg_sc_rendpage()' - End a scanned page.
//

static bool				// O - `true` on success
pwg_sc_rendpage(
    pappl_job_t        *job,		// I - Job
    pappl_sc_options_t *options,	// I - Scan options
    pappl_device_t     *device,		// I - Device (not used)
    unsigned           page)		// I - Page number (1-based)
{
  (void)job;
  (void)options;
  (void)device;
  (void)page;

  return (true);
}


//
// 'pwg_sc_rstartjob()' - Start a scan job.
//

static bool				// O - `true` on success
pwg_sc_rstartjob(
    pappl_job_t        *job,		// I - Job
    pappl_sc_options_t *options,	// I - Scan options
    pappl_device_t     *device)		// I - Device (not used)
{
  pwg_scan_job_data_t	*data;		// Job data


  (void)device;

  // Allocate job data...
  if ((data = (pwg_scan_job_data_t *)calloc(1, sizeof(pwg_scan_job_data_t))) == NULL)
    return (false);

  // Calculate image dimensions from options...
  data->width_pixels  = (unsigned)(options->scan_width * options->x_resolution / 300);
  data->height_pixels = (unsigned)(options->scan_height * options->y_resolution / 300);

  // Set bytes per pixel based on color mode...
  switch (options->color_mode)
  {
    case PAPPL_SCAN_COLOR_MODE_BLACK_AND_WHITE_1 :
        data->bpp = 0;			// Sub-byte: handled specially
        break;
    case PAPPL_SCAN_COLOR_MODE_GRAYSCALE_8 :
        data->bpp = 1;
        break;
    case PAPPL_SCAN_COLOR_MODE_GRAYSCALE_16 :
        data->bpp = 2;
        break;
    case PAPPL_SCAN_COLOR_MODE_RGB_24 :
    default :
        data->bpp = 3;
        break;
    case PAPPL_SCAN_COLOR_MODE_RGB_48 :
        data->bpp = 6;
        break;
  }

  // Determine number of pages for ADF simulation...
  if (options->input_source == PAPPL_SCAN_INPUT_SOURCE_ADF)
    data->max_pages = 3;		// Simulate 3-page ADF
  else
    data->max_pages = 0;		// Platen = 1 page

  data->page     = 0;
  data->canceled = false;

  papplJobSetData(job, data);

  return (true);
}


//
// 'pwg_sc_rstartpage()' - Start scanning a page.
//
// Returns `false` when there are no more pages (ADF exhausted).
//

static bool				// O - `true` if page ready, `false` if no more
pwg_sc_rstartpage(
    pappl_job_t        *job,		// I - Job
    pappl_sc_options_t *options,	// I - Scan options
    pappl_device_t     *device,		// I - Device (not used)
    unsigned           page)		// I - Page number (1-based)
{
  pwg_scan_job_data_t	*data;		// Job data


  (void)options;
  (void)device;

  data = (pwg_scan_job_data_t *)papplJobGetData(job);
  if (!data)
    return (false);

  // For platen: only 1 page...
  if (data->max_pages == 0)
  {
    if (page > 1)
      return (false);			// No more pages
  }
  else
  {
    // For ADF: simulate max_pages pages...
    if (page > data->max_pages)
      return (false);			// ADF exhausted
  }

  data->page = page;

  return (true);
}


//
// 'pwg_sc_rreadline()' - Read a scan line from the test scanner.
//
// Generates synthetic test patterns:
//   - Color mode (RGB): horizontal color gradient (R→G→B cycle)
//   - Grayscale: horizontal gray ramp
//   - B&W: checkerboard pattern
// The pattern varies per page number for multi-page verification.
//

static bool				// O - `true` on success, `false` on failure
pwg_sc_rreadline(
    pappl_job_t        *job,		// I - Job
    pappl_sc_options_t *options,	// I - Scan options
    pappl_device_t     *device,		// I - Device (not used)
    unsigned           y,		// I - Line number (0-based)
    unsigned char      *line,		// O - Line buffer
    size_t             linesize)	// I - Size of line buffer
{
  pwg_scan_job_data_t	*data;		// Job data
  unsigned		x;		// Current pixel
  unsigned		width;		// Width in pixels
  unsigned		page;		// Current page number
  unsigned char		phase;		// Pattern phase shift per page


  (void)device;
  (void)options;

  data = (pwg_scan_job_data_t *)papplJobGetData(job);
  if (!data)
    return (false);

  width = data->width_pixels;
  page  = data->page;
  phase = (unsigned char)(page * 64);	// Different pattern per page

  // Generate test pattern based on color mode...
  if (data->bpp == 0)
  {
    // Black & White 1-bit: checkerboard pattern, offset per page...
    unsigned	bw_width = (width + 7) / 8;
    unsigned	block     = 8;		// 8-pixel block checkerboard

    if (bw_width > linesize)
      bw_width = (unsigned)linesize;

    memset(line, 0, bw_width);

    for (x = 0; x < width; x ++)
    {
      bool pixel_on = (((x / block) + (y / block) + page) & 1) != 0;

      if (pixel_on)
        line[x / 8] |= (unsigned char)(0x80 >> (x & 7));
    }
  }
  else if (data->bpp == 1)
  {
    // Grayscale 8-bit: horizontal gray ramp with vertical bars...
    size_t bytes = width;

    if (bytes > linesize)
      bytes = linesize;

    for (x = 0; x < (unsigned)bytes; x ++)
    {
      line[x] = (unsigned char)((x * 255 / (width > 1 ? width - 1 : 1)) + phase);
    }
  }
  else if (data->bpp == 3)
  {
    // RGB 24-bit: color gradient cycling through R, G, B...
    size_t bytes = (size_t)width * 3;

    if (bytes > linesize)
      bytes = linesize;

    for (x = 0; x < width && (x * 3 + 2) < linesize; x ++)
    {
      unsigned	pos = x + (unsigned)phase;
      double	t   = (double)pos / (double)(width > 1 ? width - 1 : 1);

      // Rainbow gradient: R→Y→G→C→B→M→R...
      double	r, g, b;
      double	hue = fmod(t * 360.0 + (double)(y % 60) * 6.0, 360.0);
      double	c_val, x_val, m;

      c_val = 1.0;
      x_val = 1.0 - fabs(fmod(hue / 60.0, 2.0) - 1.0);
      m     = 0.0;

      if (hue < 60.0)
        { r = c_val; g = x_val; b = 0.0; }
      else if (hue < 120.0)
        { r = x_val; g = c_val; b = 0.0; }
      else if (hue < 180.0)
        { r = 0.0; g = c_val; b = x_val; }
      else if (hue < 240.0)
        { r = 0.0; g = x_val; b = c_val; }
      else if (hue < 300.0)
        { r = x_val; g = 0.0; b = c_val; }
      else
        { r = c_val; g = 0.0; b = x_val; }

      line[x * 3 + 0] = (unsigned char)((r + m) * 255.0);
      line[x * 3 + 1] = (unsigned char)((g + m) * 255.0);
      line[x * 3 + 2] = (unsigned char)((b + m) * 255.0);
    }
  }
  else
  {
    // Other modes: fill with gradient...
    size_t bytes = (size_t)width * data->bpp;

    if (bytes > linesize)
      bytes = linesize;

    for (x = 0; x < (unsigned)bytes; x ++)
      line[x] = (unsigned char)((x * 255 / (bytes > 1 ? bytes - 1 : 1)) + phase);
  }

  return (true);
}


//
// 'pwg_sc_status()' - Update scanner status.
//

static bool				// O - `true` on success
pwg_sc_status(
    pappl_scanner_t *scanner)		// I - Scanner
{
  (void)scanner;

  // Test scanner is always ready...
  return (true);
}
