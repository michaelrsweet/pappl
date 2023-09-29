//
// PWG test driver for the Printer Application Framework
//
// Copyright © 2020-2021 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

//
// Include necessary headers...
//

#define PWG_DRIVER 1
#include "testpappl.h"
#include <pappl/base-private.h>		// For papplCopyString
#include <cups/dir.h>
#include "label-png.h"


//
// Driver types...
//

typedef struct pwg_s
{
  cups_raster_t	*ras;			// PWG raster file
  size_t	colorants[4];		// Color usage
} pwg_job_data_t;


//
// Local globals...
//

static const char * const pwg_2inch_media[] =
{					// Supported media sizes for 2" label printer
  "oe_address-label_1.25x3.5in",
  "oe_lg-address-label_1.4x3.5in",
  "oe_multipurpose-label_2x2.3125in",

  "custom_max_2x3600in",
  "custom_min_0.25x0.25in"
};

static const char * const pwg_4inch_media[] =
{					// Supported media sizes for 4" label printer
  "oe_address-label_1.25x3.5in",
  "oe_lg-address-label_1.4x3.5in",
  "oe_multipurpose-label_2x2.3125in",
  "na_index-3x5_3x5in",
  "na_index-4x6_4x6in",
//  "na_letter_8.5x11in",

  "custom_max_4x3600in",
  "custom_min_0.25x0.25in"
};

static const char * const pwg_common_media[] =
{					// Supported media sizes for common printer
  "na_index-3x5_3x5in",
  "na_index-4x6_4x6in",
  "na_number-10_4.125x9.5in",
  "na_5x7_5x7in",
  "na_letter_8.5x11in",
  "na_legal_8.5x14in",

  "iso_a6_105x148mm",
  "iso_dl_110x220mm",
  "iso_a5_148x210mm",
  "iso_a4_210x297mm",

  "custom_max_8.5x14in",
  "custom_min_3x5in"
};


//
// Local functions...
//

static void	pwg_identify(pappl_printer_t *printer, pappl_identify_actions_t actions, const char *message);
static bool	pwg_print(pappl_job_t *job, pappl_pr_options_t *options, pappl_device_t *device);
static bool	pwg_rendjob(pappl_job_t *job, pappl_pr_options_t *options, pappl_device_t *device);
static bool	pwg_rendpage(pappl_job_t *job, pappl_pr_options_t *options, pappl_device_t *device, unsigned page);
static bool	pwg_rstartjob(pappl_job_t *job, pappl_pr_options_t *options, pappl_device_t *device);
static bool	pwg_rstartpage(pappl_job_t *job, pappl_pr_options_t *options, pappl_device_t *device, unsigned page);
static bool	pwg_rwriteline(pappl_job_t *job, pappl_pr_options_t *options, pappl_device_t *device, unsigned y, const unsigned char *line);
static bool	pwg_status(pappl_printer_t *printer);
static const char *pwg_testpage(pappl_printer_t *printer, char *buffer, size_t bufsize);


//
// 'pwg_autoadd()' - Auto-add callback.
//

const char *				// O - Driver name or `NULL` for none
pwg_autoadd(const char *device_info,	// I - Device information string (not used)
            const char *device_uri,	// I - Device URI (not used)
            const char *device_id,	// I - IEEE-1284 device ID
            void       *data)		// I - Callback data (not used)
{
  cups_len_t	num_did;		// Number of device ID pairs
  cups_option_t	*did;			// Device ID pairs
  const char	*cmd,			// Command set value
		*ret = NULL;		// Return value


  (void)device_info;
  (void)device_uri;
  (void)data;

  num_did = (cups_len_t)papplDeviceParseID(device_id, &did);

  if ((cmd = cupsGetOption("COMMAND SET", num_did, did)) == NULL)
    cmd = cupsGetOption("CMD", num_did, did);

  if (cmd && strstr(cmd, "PWGRaster") != NULL)
    ret = "pwg_common-300dpi-srgb_8";

  cupsFreeOptions(num_did, did);

  return (ret);
}


//
// 'pwg_callback()' - Driver callback.
//

bool					// O - `true` on success, `false` on failure
pwg_callback(
    pappl_system_t         *system,	// I - System
    const char             *driver_name,// I - Driver name
    const char             *device_uri,	// I - Device URI
    const char             *device_id,	// I - IEEE-1284 device ID string (not used)
    pappl_pr_driver_data_t *driver_data,// O - Driver data
    ipp_t                  **driver_attrs,
					// O - Driver attributes
    void                   *data)	// I - Callback data
{
  int	i;				// Looping var


  (void)device_id;

  if (!driver_name || !device_uri || !driver_data || !driver_attrs)
  {
    papplLog(system, PAPPL_LOGLEVEL_ERROR, "Driver callback called without required information.");
    return (false);
  }

  if (!data || (strcmp((const char *)data, "testpappl") && strcmp((const char *)data, "testmainloop")))
  {
    papplLog(system, PAPPL_LOGLEVEL_ERROR, "Driver callback called with bad data pointer.");
    return (false);
  }

  if (!strncmp(driver_name, "pwg_fail", 8))
  {
    papplLog(system, PAPPL_LOGLEVEL_ERROR, "Always-fails driver specified.");
    return (false);
  }

  if (strstr(driver_name, "-black_1"))
  {
    driver_data->raster_types = PAPPL_PWG_RASTER_TYPE_BLACK_1 | PAPPL_PWG_RASTER_TYPE_BLACK_8 | PAPPL_PWG_RASTER_TYPE_SGRAY_8;
    driver_data->force_raster_type = PAPPL_PWG_RASTER_TYPE_BLACK_1;
  }
  else if (strstr(driver_name, "-sgray_8"))
  {
    driver_data->raster_types = PAPPL_PWG_RASTER_TYPE_BLACK_1 | PAPPL_PWG_RASTER_TYPE_BLACK_8 | PAPPL_PWG_RASTER_TYPE_SGRAY_8;
  }
  else if (strstr(driver_name, "-srgb_8"))
  {
    driver_data->raster_types = PAPPL_PWG_RASTER_TYPE_BLACK_1 | PAPPL_PWG_RASTER_TYPE_BLACK_8 | PAPPL_PWG_RASTER_TYPE_SGRAY_8 | PAPPL_PWG_RASTER_TYPE_SRGB_8;
  }
  else
  {
    papplLog(system, PAPPL_LOGLEVEL_ERROR, "Unsupported driver name '%s'.", driver_name);
    return (false);
  }

  driver_data->identify_cb        = pwg_identify;
  driver_data->identify_default   = PAPPL_IDENTIFY_ACTIONS_SOUND;
  driver_data->identify_supported = PAPPL_IDENTIFY_ACTIONS_DISPLAY | PAPPL_IDENTIFY_ACTIONS_SOUND;
  driver_data->printfile_cb       = pwg_print;
  driver_data->rendjob_cb         = pwg_rendjob;
  driver_data->rendpage_cb        = pwg_rendpage;
  driver_data->rstartjob_cb       = pwg_rstartjob;
  driver_data->rstartpage_cb      = pwg_rstartpage;
  driver_data->rwriteline_cb      = pwg_rwriteline;
  driver_data->status_cb          = pwg_status;
  driver_data->testpage_cb        = pwg_testpage;
  driver_data->format             = "image/pwg-raster";
  driver_data->orient_default     = IPP_ORIENT_NONE;
  driver_data->quality_default    = IPP_QUALITY_NORMAL;

  driver_data->num_resolution = 0;
  if (strstr(driver_name, "-203dpi"))
  {
    driver_data->x_resolution[driver_data->num_resolution   ] = 203;
    driver_data->y_resolution[driver_data->num_resolution ++] = 203;
    driver_data->x_default = driver_data->y_default           = 203;
  }
  if (strstr(driver_name, "-300dpi"))
  {
    driver_data->x_resolution[driver_data->num_resolution   ] = 300;
    driver_data->y_resolution[driver_data->num_resolution ++] = 300;
    driver_data->x_default = driver_data->y_default           = 300;
  }
  if (strstr(driver_name, "-600dpi"))
  {
    driver_data->x_resolution[driver_data->num_resolution   ] = 600;
    driver_data->y_resolution[driver_data->num_resolution ++] = 600;
    driver_data->x_default = driver_data->y_default           = 600;
  }
  if (driver_data->num_resolution == 0)
  {
    papplLog(system, PAPPL_LOGLEVEL_ERROR, "No resolution information in driver name '%s'.", driver_name);
    return (false);
  }

  if (strstr(driver_name, "-pdf"))
    driver_data->format = "application/pdf";

  if (!strncmp(driver_name, "pwg_2inch-", 10))
  {
    papplCopyString(driver_data->make_and_model, "PWG 2-inch Label Printer", sizeof(driver_data->make_and_model));

    driver_data->kind       = PAPPL_KIND_LABEL | PAPPL_KIND_ROLL;
    driver_data->ppm        = 20;	// 20 labels per minute
    driver_data->left_right = 312;	// 1/16" left and right
    driver_data->bottom_top = 625;	// 1/8" top and bottom

    driver_data->num_media = (int)(sizeof(pwg_2inch_media) / sizeof(pwg_2inch_media[0]));
    memcpy((void *)driver_data->media, pwg_2inch_media, sizeof(pwg_2inch_media));

    driver_data->num_source = 1;
    driver_data->source[0]  = "main-roll";

    papplCopyString(driver_data->media_default.size_name, "oe_address-label_1.25x3.5in", sizeof(driver_data->media_default.size_name));
    papplCopyString(driver_data->media_ready[0].size_name, "oe_address-label_1.25x3.5in", sizeof(driver_data->media_ready[0].size_name));

    driver_data->darkness_configured = 53;
    driver_data->darkness_supported  = 16;
    driver_data->speed_supported[1]  = 8 * 2540;
  }
  else if (!strncmp(driver_name, "pwg_4inch-", 10))
  {
    papplCopyString(driver_data->make_and_model, "PWG 4-inch Label Printer", sizeof(driver_data->make_and_model));

    driver_data->kind       = PAPPL_KIND_LABEL | PAPPL_KIND_ROLL;
    driver_data->ppm        = 20;	// 20 labels per minute
    driver_data->left_right = 1;	// Not quite borderless left and right
    driver_data->bottom_top = 1;	// Not quite borderless top and bottom

    driver_data->num_media = (int)(sizeof(pwg_4inch_media) / sizeof(pwg_4inch_media[0]));
    memcpy((void *)driver_data->media, pwg_4inch_media, sizeof(pwg_4inch_media));

    driver_data->num_source = 1;
    driver_data->source[0]  = "main-roll";

    papplCopyString(driver_data->media_default.size_name, "na_index-4x6_4x6in", sizeof(driver_data->media_default.size_name));
    papplCopyString(driver_data->media_ready[0].size_name, "na_index-4x6_4x6in", sizeof(driver_data->media_ready[0].size_name));
    papplCopyString(driver_data->media_ready[1].size_name, "oe_address-label_1.25x3.5in", sizeof(driver_data->media_ready[1].size_name));

    driver_data->darkness_configured = 53;
    driver_data->darkness_supported  = 16;
    driver_data->speed_supported[1]  = 8 * 2540;
  }
  else if (!strncmp(driver_name, "pwg_common-", 11))
  {
    papplCopyString(driver_data->make_and_model, "PWG Office Printer", sizeof(driver_data->make_and_model));

    driver_data->has_supplies = true;
    driver_data->kind         = PAPPL_KIND_DOCUMENT | PAPPL_KIND_PHOTO | PAPPL_KIND_POSTCARD;
    driver_data->ppm          = 5;	// 5 mono pages per minute
    driver_data->ppm_color    = 2;	// 2 color pages per minute
    driver_data->left_right   = 423;	// 1/6" left and right
    driver_data->bottom_top   = 423;	// 1/6" top and bottom
    driver_data->borderless   = true;	// Also borderless sizes

    driver_data->finishings = PAPPL_FINISHINGS_PUNCH | PAPPL_FINISHINGS_STAPLE;

    driver_data->num_media = (int)(sizeof(pwg_common_media) / sizeof(pwg_common_media[0]));
    memcpy((void *)driver_data->media, pwg_common_media, sizeof(pwg_common_media));

    driver_data->num_source = 4;
    driver_data->source[0]  = "main";
    driver_data->source[1]  = "alternate";
    driver_data->source[2]  = "manual";
    driver_data->source[3]  = "by-pass-tray";

    if (driver_data->raster_types & PAPPL_PWG_RASTER_TYPE_SRGB_8)
    {
      // Color office printer gets two output bins...
      driver_data->num_bin = 2;
      driver_data->bin[0]  = "center";
      driver_data->bin[1]  = "rear";
    }
    else
    {
      // B&W office printer gets one output bin...
      driver_data->num_bin = 1;
      driver_data->bin[0]  = "center";
    }

    papplCopyString(driver_data->media_default.size_name, "na_letter_8.5x11in", sizeof(driver_data->media_default.size_name));
    papplCopyString(driver_data->media_ready[0].size_name, "na_letter_8.5x11in", sizeof(driver_data->media_ready[0].size_name));
    papplCopyString(driver_data->media_ready[1].size_name, "iso_a4_210x297mm", sizeof(driver_data->media_ready[1].size_name));

    driver_data->sides_supported = PAPPL_SIDES_ONE_SIDED | PAPPL_SIDES_TWO_SIDED_LONG_EDGE | PAPPL_SIDES_TWO_SIDED_SHORT_EDGE;
    driver_data->sides_default   = PAPPL_SIDES_TWO_SIDED_LONG_EDGE;
  }
  else
  {
    papplLog(system, PAPPL_LOGLEVEL_ERROR, "No dimension information in driver name '%s'.", driver_name);
    return (false);
  }

  if (!strncmp(driver_name, "pwg_common-", 11))
  {
    driver_data->color_supported = PAPPL_COLOR_MODE_AUTO | PAPPL_COLOR_MODE_AUTO_MONOCHROME | PAPPL_COLOR_MODE_COLOR | PAPPL_COLOR_MODE_MONOCHROME;
    driver_data->color_default   = PAPPL_COLOR_MODE_AUTO;

    driver_data->num_type = 8;
    driver_data->type[0]  = "stationery";
    driver_data->type[1]  = "stationery-letterhead";
    driver_data->type[2]  = "labels";
    driver_data->type[3]  = "photographic";
    driver_data->type[4]  = "photographic-glossy";
    driver_data->type[5]  = "photographic-matte";
    driver_data->type[6]  = "transparency";
    driver_data->type[7]  = "envelope";

    papplCopyString(driver_data->media_default.size_name, papplLocGetDefaultMediaSizeName(), sizeof(driver_data->media_default.size_name));
  }
  else
  {
    static const int integers[] =	// List of integers
    {
      1,
      2,
      3,
      5,
      7,
      11,
      13,
      17,
      19,
      23,
      29,
      31,
      37,
      41,
      43,
      47
    };
    static const char * const keywords[] =
    {					// List of keywords
      "one-fish",
      "two-fish",
      "red-fish",
      "blue-fish"
    };

    driver_data->color_supported = PAPPL_COLOR_MODE_AUTO | PAPPL_COLOR_MODE_MONOCHROME;
    driver_data->color_default   = PAPPL_COLOR_MODE_MONOCHROME;

    memset(driver_data->gdither, 127, sizeof(driver_data->gdither));

    driver_data->icons[0].data    = label_sm_png;
    driver_data->icons[0].datalen = sizeof(label_sm_png);
    driver_data->icons[1].data    = label_md_png;
    driver_data->icons[1].datalen = sizeof(label_md_png);
    driver_data->icons[2].data    = label_lg_png;
    driver_data->icons[2].datalen = sizeof(label_lg_png);

    driver_data->top_offset_supported[0] = -2000;
    driver_data->top_offset_supported[1] = 2000;

    driver_data->tracking_supported = PAPPL_MEDIA_TRACKING_MARK | PAPPL_MEDIA_TRACKING_CONTINUOUS;

    driver_data->num_type = 3;
    driver_data->type[0]  = "labels";
    driver_data->type[1]  = "continuous";
    driver_data->type[2]  = "labels-continuous";

    driver_data->sides_supported = PAPPL_SIDES_ONE_SIDED;
    driver_data->sides_default   = PAPPL_SIDES_ONE_SIDED;

    driver_data->num_vendor = 5;
    driver_data->vendor[0]  = "vendor-boolean";
    driver_data->vendor[1]  = "vendor-integer";
    driver_data->vendor[2]  = "vendor-keyword";
    driver_data->vendor[3]  = "vendor-range";
    driver_data->vendor[4]  = "vendor-text";

    *driver_attrs = ippNew();

    ippAddBoolean(*driver_attrs, IPP_TAG_PRINTER, "vendor-boolean-default", 1);
    ippAddBoolean(*driver_attrs, IPP_TAG_PRINTER, "vendor-boolean-supported", 1);

    ippAddInteger(*driver_attrs, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "vendor-integer-default", 7);
    ippAddIntegers(*driver_attrs, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "vendor-integer-supported", (int)(sizeof(integers) / sizeof(integers[0])), integers);

    ippAddString(*driver_attrs, IPP_TAG_PRINTER, IPP_TAG_KEYWORD, "vendor-keyword-default", NULL, "two-fish");
    ippAddStrings(*driver_attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "vendor-keyword-supported", (int)(sizeof(keywords) / sizeof(keywords[0])), NULL, keywords);

    ippAddInteger(*driver_attrs, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "vendor-range-default", 42);
    ippAddRange(*driver_attrs, IPP_TAG_PRINTER, "vendor-range-supported", -100, 100);

    ippAddString(*driver_attrs, IPP_TAG_PRINTER, IPP_TAG_TEXT, "vendor-text-default", NULL, "Hello, World!");
  }

  // Fill out ready and default media
  for (i = 0; i < driver_data->num_source; i ++)
  {
    pwg_media_t *pwg = pwgMediaForPWG(driver_data->media_ready[i].size_name);

    if (pwg)
    {
      driver_data->media_ready[i].bottom_margin = driver_data->bottom_top;
      driver_data->media_ready[i].left_margin   = driver_data->left_right;
      driver_data->media_ready[i].right_margin  = driver_data->left_right;
      driver_data->media_ready[i].size_width    = pwg->width;
      driver_data->media_ready[i].size_length   = pwg->length;
      driver_data->media_ready[i].top_margin    = driver_data->bottom_top;
      driver_data->media_ready[i].tracking      = PAPPL_MEDIA_TRACKING_MARK;
      papplCopyString(driver_data->media_ready[i].source, driver_data->source[i], sizeof(driver_data->media_ready[i].source));
      papplCopyString(driver_data->media_ready[i].type, driver_data->type[0], sizeof(driver_data->media_ready[i].type));
    }

    if (!strcmp(driver_data->media_default.size_name, driver_data->media_ready[i].size_name))
      driver_data->media_default = driver_data->media_ready[i];
  }

  return (true);
}


//
// 'pwg_identify()' - Identify the printer.
//

static void
pwg_identify(
    pappl_printer_t          *printer,	// I - Printer
    pappl_identify_actions_t actions,	// I - Actions to take
    const char               *message)	// I - Message, if any
{
  (void)printer;
  (void)actions;

  // TODO: Open console and send BEL character and message to it instead...
  putchar(7);
  if (message)
    puts(message);

  fflush(stdout);
}


//
// 'pwg_print()' - Print a file.
//

static bool				// O - `true` on success, `false` on failure
pwg_print(
    pappl_job_t        *job,		// I - Job
    pappl_pr_options_t *options,	// I - Job options (unused)
    pappl_device_t     *device)		// I - Print device (unused)
{
  int		fd;			// Input file
  ssize_t	bytes;			// Bytes read/written
  char		buffer[65536];		// Read/write buffer


  (void)options;

  papplJobSetImpressions(job, 1);

  if ((fd  = open(papplJobGetFilename(job), O_RDONLY)) < 0)
  {
    papplLogJob(job, PAPPL_LOGLEVEL_ERROR, "Unable to open print file '%s': %s", papplJobGetFilename(job), strerror(errno));
    return (false);
  }

  while ((bytes = read(fd, buffer, sizeof(buffer))) > 0)
    papplDeviceWrite(device, buffer, (size_t)bytes);

  close(fd);

  papplJobSetImpressionsCompleted(job, 1);

  return (true);
}


//
// 'pwg_rendjob()' - End a job.
//

static bool				// O - `true` on success, `false` on failure
pwg_rendjob(
    pappl_job_t        *job,		// I - Job
    pappl_pr_options_t *options,	// I - Job options (unused)
    pappl_device_t     *device)		// I - Print device (unused)
{
  pwg_job_data_t	*pwg = (pwg_job_data_t *)papplJobGetData(job);
					// Job data

  (void)options;
  (void)device;

  cupsRasterClose(pwg->ras);

  free(pwg);
  papplJobSetData(job, NULL);

  return (true);
}


//
// 'pwg_rendpage()' - End a page.
//

static bool				// O - `true` on success, `false` on failure
pwg_rendpage(
    pappl_job_t        *job,		// I - Job
    pappl_pr_options_t *options,	// I - Job options
    pappl_device_t     *device,		// I - Print device (unused)
    unsigned           page)		// I - Page number
{
  pwg_job_data_t	*pwg = (pwg_job_data_t *)papplJobGetData(job);
					// PWG driver data
  pappl_printer_t	*printer = papplJobGetPrinter(job);
  					// Printer
  pappl_supply_t	supplies[5];	// Supply-level data


  (void)device;
  (void)page;

  if (papplPrinterGetSupplies(printer, 5, supplies) == 5)
  {
    // Calculate ink usage from coverage - figure 100 pages at 10% for black,
    // 50 pages at 10% for CMY, and 200 pages at 10% for the waste tank...
    int i;				// Looping var
    int	c, m, y, k, w;			// Ink usage
    pappl_preason_t reasons = PAPPL_PREASON_NONE;
					// "printer-state-reasons" values

    papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "Calculating ink usage (%u,%u,%u,%u)", (unsigned)pwg->colorants[0], (unsigned)pwg->colorants[1], (unsigned)pwg->colorants[2], (unsigned)pwg->colorants[3]);

    c = (int)(pwg->colorants[0] / options->header.cupsWidth / options->header.cupsHeight / 5);
    m = (int)(pwg->colorants[1] / options->header.cupsWidth / options->header.cupsHeight / 5);
    y = (int)(pwg->colorants[2] / options->header.cupsWidth / options->header.cupsHeight / 5);
    k = (int)(pwg->colorants[3] / options->header.cupsWidth / options->header.cupsHeight / 10);
    w = (int)((pwg->colorants[0] + pwg->colorants[1] + pwg->colorants[2] + pwg->colorants[3]) / options->header.cupsWidth / options->header.cupsHeight / 20);

    // Keep levels between 0 and 100...
    if ((supplies[0].level -= c) < 0)
      supplies[0].level = 100;		// Auto-refill
    if ((supplies[1].level -= m) < 0)
      supplies[1].level = 100;		// Auto-refill
    if ((supplies[2].level -= y) < 0)
      supplies[2].level = 100;		// Auto-refill
    if ((supplies[3].level -= k) < 0)
      supplies[3].level = 100;		// Auto-refill
    if ((supplies[4].level += w) > 100)
      supplies[4].level = 0;		// Auto-replace

    // Update printer-state-reasons accordingly...
    for (i = 0; i < 4; i ++)
    {
      if (supplies[i].level == 0)
	reasons |= PAPPL_PREASON_MARKER_SUPPLY_EMPTY;
      else if (supplies[i].level < 10)
	reasons |= PAPPL_PREASON_MARKER_SUPPLY_LOW;
    }

    if (supplies[4].level == 100)
      reasons |= PAPPL_PREASON_MARKER_WASTE_FULL;
    else if (supplies[4].level >= 90)
      reasons |= PAPPL_PREASON_MARKER_WASTE_ALMOST_FULL;

    papplPrinterSetSupplies(printer, 5, supplies);
    papplPrinterSetReasons(printer, reasons, PAPPL_PREASON_DEVICE_STATUS);
  }

  return (true);
}


//
// 'pwg_rstartjob()' - Start a job.
//

static bool				// O - `true` on success, `false` on failure
pwg_rstartjob(
    pappl_job_t        *job,		// I - Job
    pappl_pr_options_t *options,	// I - Job options
    pappl_device_t     *device)		// I - Print device (unused)
{
  pwg_job_data_t *pwg = (pwg_job_data_t *)calloc(1, sizeof(pwg_job_data_t));
					// PWG driver data


  (void)options;

  papplJobSetData(job, pwg);

  pwg->ras = cupsRasterOpenIO((cups_raster_cb_t)papplDeviceWrite, device, CUPS_RASTER_WRITE_PWG);

  return (1);
}


//
// 'pwg_rstartpage()' - Start a page.
//

static bool				// O - `true` on success, `false` on failure
pwg_rstartpage(
    pappl_job_t        *job,		// I - Job
    pappl_pr_options_t *options,	// I - Job options
    pappl_device_t     *device,		// I - Print device (unused)
    unsigned           page)		// I - Page number
{
  pwg_job_data_t	*pwg = (pwg_job_data_t *)papplJobGetData(job);
					// PWG driver data

  (void)device;
  (void)page;

  memset(pwg->colorants, 0, sizeof(pwg->colorants));

  return (cupsRasterWriteHeader(pwg->ras, &options->header) != 0);
}


//
// 'pwg_rwriteline()' - Write a raster line.
//

static bool				// O - `true` on success, `false` on failure
pwg_rwriteline(
    pappl_job_t         *job,		// I - Job
    pappl_pr_options_t  *options,	// I - Job options
    pappl_device_t      *device,	// I - Print device (unused)
    unsigned            y,		// I - Line number
    const unsigned char *line)		// I - Line
{
  const unsigned char	*lineptr,	// Pointer into line
			*lineend;	// End of line
  pwg_job_data_t	*pwg = (pwg_job_data_t *)papplJobGetData(job);
					// PWG driver data

  (void)device;
  (void)y;

  // Add the colorant usage for this line (for simulation purposes - normally
  // this is tracked by the printer/ink cartridge...)
  lineend = line + options->header.cupsBytesPerLine;

  switch (options->header.cupsColorSpace)
  {
    case CUPS_CSPACE_K :
        if (options->header.cupsBitsPerPixel == 1)
        {
          // 1-bit K
	  static unsigned short amounts[256] =
	  {				// Amount of "ink" used for 8 pixels
	    0,    255,  255,  510,  255,  510,  510,  765,
	    255,  510,  510,  765,  510,  765,  765,  1020,
	    255,  510,  510,  765,  510,  765,  765,  1020,
	    510,  765,  765,  1020, 765,  1020, 1020, 1275,
	    255,  510,  510,  765,  510,  765,  765,  1020,
	    510,  765,  765,  1020, 765,  1020, 1020, 1275,
	    510,  765,  765,  1020, 765,  1020, 1020, 1275,
	    765,  1020, 1020, 1275, 1020, 1275, 1275, 1530,
	    255,  510,  510,  765,  510,  765,  765,  1020,
	    510,  765,  765,  1020, 765,  1020, 1020, 1275,
	    510,  765,  765,  1020, 765,  1020, 1020, 1275,
	    765,  1020, 1020, 1275, 1020, 1275, 1275, 1530,
	    510,  765,  765,  1020, 765,  1020, 1020, 1275,
	    765,  1020, 1020, 1275, 1020, 1275, 1275, 1530,
	    765,  1020, 1020, 1275, 1020, 1275, 1275, 1530,
	    1020, 1275, 1275, 1530, 1275, 1530, 1530, 1785,
	    255,  510,  510,  765,  510,  765,  765,  1020,
	    510,  765,  765,  1020, 765,  1020, 1020, 1275,
	    510,  765,  765,  1020, 765,  1020, 1020, 1275,
	    765,  1020, 1020, 1275, 1020, 1275, 1275, 1530,
	    510,  765,  765,  1020, 765,  1020, 1020, 1275,
	    765,  1020, 1020, 1275, 1020, 1275, 1275, 1530,
	    765,  1020, 1020, 1275, 1020, 1275, 1275, 1530,
	    1020, 1275, 1275, 1530, 1275, 1530, 1530, 1785,
	    510,  765,  765,  1020, 765,  1020, 1020, 1275,
	    765,  1020, 1020, 1275, 1020, 1275, 1275, 1530,
	    765,  1020, 1020, 1275, 1020, 1275, 1275, 1530,
	    1020, 1275, 1275, 1530, 1275, 1530, 1530, 1785,
	    765,  1020, 1020, 1275, 1020, 1275, 1275, 1530,
	    1020, 1275, 1275, 1530, 1275, 1530, 1530, 1785,
	    1020, 1275, 1275, 1530, 1275, 1530, 1530, 1785,
	    1275, 1530, 1530, 1785, 1530, 1785, 1785, 2040
	  };

          for (lineptr = line; lineptr < lineend; lineptr ++)
	    pwg->colorants[3] += amounts[*lineptr];
        }
        else
        {
          // 8-bit K
          for (lineptr = line; lineptr < lineend; lineptr ++)
            pwg->colorants[3] += *lineptr;
        }
        break;

    case CUPS_CSPACE_W :
    case CUPS_CSPACE_SW :
	// 8-bit W (luminance)
	for (lineptr = line; lineptr < lineend; lineptr ++)
	  pwg->colorants[3] += 255 - *lineptr;
        break;

    case CUPS_CSPACE_RGB :
    case CUPS_CSPACE_SRGB :
    case CUPS_CSPACE_ADOBERGB :
        // 24-bit RGB
	for (lineptr = line; lineptr < lineend; lineptr += 3)
        {
          // Convert RGB to CMYK using simple transform...
          unsigned char cc = 255 - lineptr[0];
          unsigned char cm = 255 - lineptr[1];
          unsigned char cy = 255 - lineptr[2];
          unsigned char ck = cc;

          if (ck > cm)
            ck = cm;
	  if (ck > cy)
	    ck = cy;

	  cc -= ck;
	  cm -= ck;
	  cy -= ck;

          pwg->colorants[0] += cc;
          pwg->colorants[1] += cm;
          pwg->colorants[2] += cy;
          pwg->colorants[3] += ck;
        }
        break;

    case CUPS_CSPACE_CMYK :
        // 32-bit CMYK
	for (lineptr = line; lineptr < lineend; lineptr += 4)
	{
	  pwg->colorants[0] += lineptr[0];
	  pwg->colorants[1] += lineptr[1];
	  pwg->colorants[2] += lineptr[2];
	  pwg->colorants[3] += lineptr[3];
	}
        break;

    default :
        break;
  }

  return (cupsRasterWritePixels(pwg->ras, (unsigned char *)line, options->header.cupsBytesPerLine) != 0);
}


//
// 'pwg_status()' - Get current printer status.
//

static bool				// O - `true` on success, `false` on failure
pwg_status(
    pappl_printer_t *printer)		// I - Printer
{
  if (!strncmp(papplPrinterGetDriverName(printer), "pwg_common-", 11))
  {
    // Supply levels...
    static pappl_supply_t supply[5] =	// Supply level data
    {
      { PAPPL_SUPPLY_COLOR_CYAN,     "Cyan Ink",       true, 100, PAPPL_SUPPLY_TYPE_INK },
      { PAPPL_SUPPLY_COLOR_MAGENTA,  "Magenta Ink",    true, 100, PAPPL_SUPPLY_TYPE_INK },
      { PAPPL_SUPPLY_COLOR_YELLOW,   "Yellow Ink",     true, 100, PAPPL_SUPPLY_TYPE_INK },
      { PAPPL_SUPPLY_COLOR_BLACK,    "Black Ink",      true, 100, PAPPL_SUPPLY_TYPE_INK },
      { PAPPL_SUPPLY_COLOR_NO_COLOR, "Waste Ink Tank", true, 0, PAPPL_SUPPLY_TYPE_WASTE_INK }
    };

    if (papplPrinterGetSupplies(printer, 0, NULL) == 0)
      papplPrinterSetSupplies(printer, (int)(sizeof(supply) / sizeof(supply[0])), supply);
  }

  // Every 10 seconds, set the "media-empty" reason for one second...
  if ((time(NULL) % 10) == 0)
    papplPrinterSetReasons(printer, PAPPL_PREASON_MEDIA_EMPTY, PAPPL_PREASON_NONE);
  else
    papplPrinterSetReasons(printer, PAPPL_PREASON_NONE, PAPPL_PREASON_MEDIA_EMPTY);

  return (true);
}


//
// 'pwg_testpage()' - Return a test page file to print
//

static const char *			// O - Filename or `NULL`
pwg_testpage(
    pappl_printer_t *printer,		// I - Printer
    char            *buffer,		// I - File Buffer
    size_t          bufsize)		// I - Buffer Size
{
  const char		*testfile;	// Test File
  pappl_pr_driver_data_t data;		// Driver data


  // Get the printer capabilities...
  papplPrinterGetDriverData(printer, &data);

  // Find the right test file...
  if (data.color_supported & PAPPL_COLOR_MODE_COLOR)
    testfile = "portrait-color.png";
  else
    testfile = "portrait-gray.png";

  papplCopyString(buffer, testfile, bufsize);
  if (access(buffer, R_OK))
    snprintf(buffer, bufsize, "testsuite/%s", testfile);

  if (access(buffer, R_OK))
  {
    *buffer = '\0';
    return (NULL);
  }
  else
  {
    return (buffer);
  }
}
