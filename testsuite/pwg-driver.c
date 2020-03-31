//
// PWG test driver for the Printer Application Framework
//
// Copyright © 2020 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

//
// Include necessary headers...
//

#include "testpappl.h"
#include <pappl/base-private.h>		// For strlcpy
#include "label-png.h"


//
// Driver types...
//

typedef struct pwg_s
{
  int		fd;			// Output file descriptor
  cups_raster_t	*ras;			// PWG raster file
} pwg_job_data_t;


//
// Local globals...
//

static const char * const pwg_2inch_media[] =
{					// Supported media sizes for 2" label printer
  "oe_address-label_1.25x3.5in",
  "oe_lg-address-label_1.4x3.5in",
  "oe_multipurpose-label_2x2.3125in",

  "roll_max_2x3600in",
  "roll_min_0.25x0.25in"
};

static const char * const pwg_4inch_media[] =
{					// Supported media sizes for 4" label printer
  "oe_address-label_1.25x3.5in",
  "oe_lg-address-label_1.4x3.5in",
  "oe_multipurpose-label_2x2.3125in",
  "na_index-3x5_3x5in",
  "na_index-4x6_4x6in",

  "roll_max_4x3600in",
  "roll_min_0.25x0.25in"
};

static const char * const pwg_common_media[] =
{					// Supported media sizes for common printer
  "na_index-3x5_3x5in",
  "na_index-4x6_4x6in",
  "na_number-10_4.125x9.5in",
  "na_5x7_5x7in",
  "na_letter-8.5x11in",
  "na_legal-8.5x14in",

  "iso_a6_105x148mm",
  "iso_dl_110x220mm",
  "iso_a5_148x210mm",
  "iso_a4_210x297mm",

  "roll_max_8.5x3600in",
  "roll_min_3x5in"
};


//
// Local functions...
//

static bool	pwg_callback(pappl_system_t *system, const char *driver_name, const char *device_uri, pappl_driver_data_t *driver_data, ipp_t **driver_attrs, void *data);
static void	pwg_identify(pappl_printer_t *printer, pappl_identify_actions_t actions, const char *message);
static bool	pwg_print(pappl_job_t *job, pappl_options_t *options);
static bool	pwg_rendjob(pappl_job_t *job, pappl_options_t *options);
static bool	pwg_rendpage(pappl_job_t *job, pappl_options_t *options, unsigned page);
static bool	pwg_rstartjob(pappl_job_t *job, pappl_options_t *options);
static bool	pwg_rstartpage(pappl_job_t *job, pappl_options_t *options, unsigned page);
static bool	pwg_rwrite(pappl_job_t *job, pappl_options_t *options, unsigned y, const unsigned char *line);
static bool	pwg_status(pappl_printer_t *printer);


//
// 'test_setup_drivers()' - Set the drivers list and callback.
//

void
test_setup_drivers(
    pappl_system_t *system)		// I - System
{
  static const char * const names[] =	// Driver names
  {
    "pwg_2inch-203dpi-black_1",
    "pwg_2inch-300dpi-black_1",
    "pwg_4inch-203dpi-black_1",
    "pwg_4inch-300dpi-black_1",
    "pwg_common-300dpi-black_1",
    "pwg_common-300dpi-sgray_8",
    "pwg_common-300dpi-srgb_8",
    "pwg_common-300dpi-600dpi-black_1",
    "pwg_common-300dpi-600dpi-sgray_8",
    "pwg_common-300dpi-600dpi-srgb_8"
  };


  papplSystemSetDrivers(system, (int)(sizeof(names) / sizeof(names[0])), names, pwg_callback, "testpappl");
}


//
// 'pwg_callback()' - Driver callback.
//

static bool				// O - `true` on success, `false` on failure
pwg_callback(
    pappl_system_t      *system,	// I - System
    const char          *driver_name,	// I - Driver name
    const char          *device_uri,	// I - Device URI
    pappl_driver_data_t *driver_data,	// O - Driver data
    ipp_t               **driver_attrs,	// O - Driver attributes
    void                *data)		// I - Callback data
{
  int	i;				// Looping var


  if (!driver_name || !device_uri || !driver_data || !driver_attrs)
  {
    papplLog(system, PAPPL_LOGLEVEL_ERROR, "Driver callback called without required information.");
    return (false);
  }

  if (strcmp(device_uri, "file:///dev/null"))
  {
    papplLog(system, PAPPL_LOGLEVEL_ERROR, "Unsupported device URI '%s'.", device_uri);
    return (false);
  }

  if (!data || strcmp((const char *)data, "testpappl"))
  {
    papplLog(system, PAPPL_LOGLEVEL_ERROR, "Driver callback called with bad data pointer.");
    return (false);
  }

  if (strstr(driver_name, "-black_1"))
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

  driver_data->identify           = pwg_identify;
  driver_data->identify_default   = PAPPL_IDENTIFY_ACTIONS_SOUND;
  driver_data->identify_supported = PAPPL_IDENTIFY_ACTIONS_SOUND;
  driver_data->print              = pwg_print;
  driver_data->rendjob            = pwg_rendjob;
  driver_data->rendpage           = pwg_rendpage;
  driver_data->rstartjob          = pwg_rstartjob;
  driver_data->rstartpage         = pwg_rstartpage;
  driver_data->rwrite             = pwg_rwrite;
  driver_data->status             = pwg_status;
  driver_data->format             = "image/pwg-raster";

  driver_data->num_resolution = 0;
  if (strstr(driver_name, "-203dpi"))
  {
    driver_data->x_resolution[driver_data->num_resolution   ] = 203;
    driver_data->y_resolution[driver_data->num_resolution ++] = 203;
  }
  if (strstr(driver_name, "-300dpi"))
  {
    driver_data->x_resolution[driver_data->num_resolution   ] = 300;
    driver_data->y_resolution[driver_data->num_resolution ++] = 300;
  }
  if (strstr(driver_name, "-600dpi"))
  {
    driver_data->x_resolution[driver_data->num_resolution   ] = 600;
    driver_data->y_resolution[driver_data->num_resolution ++] = 600;
  }
  if (driver_data->num_resolution == 0)
  {
    papplLog(system, PAPPL_LOGLEVEL_ERROR, "No resolution information in driver name '%s'.", driver_name);
    return (false);
  }

  if (!strncmp(driver_name, "pwg_2inch-", 10))
  {
    strlcpy(driver_data->make_and_model, "PWG 2-inch Label Printer", sizeof(driver_data->make_and_model));
    driver_data->kind       = PAPPL_KIND_LABEL | PAPPL_KIND_ROLL;
    driver_data->left_right = 312;	// 1/16" left and right
    driver_data->bottom_top = 625;	// 1/8" top and bottom

    driver_data->num_media = (int)(sizeof(pwg_2inch_media) / sizeof(pwg_2inch_media[0]));
    memcpy(driver_data->media, pwg_2inch_media, sizeof(pwg_2inch_media));

    driver_data->num_source = 1;
    driver_data->source[0]  = "main-roll";

    strlcpy(driver_data->media_ready[0].size_name, "oe_address-label_1.25x3.5in", sizeof(driver_data->media_ready[0].size_name));
  }
  else if (!strncmp(driver_name, "pwg_4inch-", 10))
  {
    strlcpy(driver_data->make_and_model, "PWG 4-inch Label Printer", sizeof(driver_data->make_and_model));

    driver_data->kind       = PAPPL_KIND_LABEL | PAPPL_KIND_ROLL;
    driver_data->left_right = 312;	// 1/16" left and right
    driver_data->bottom_top = 625;	// 1/8" top and bottom

    driver_data->num_media = (int)(sizeof(pwg_4inch_media) / sizeof(pwg_4inch_media[0]));
    memcpy(driver_data->media, pwg_4inch_media, sizeof(pwg_4inch_media));

    driver_data->num_source = 2;
    driver_data->source[0]  = "main-roll";
    driver_data->source[1]  = "alternate-roll";

    strlcpy(driver_data->media_ready[0].size_name, "oe_address-label_1.25x3.5in", sizeof(driver_data->media_ready[0].size_name));
    strlcpy(driver_data->media_ready[1].size_name, "na_index-4x6_4x6in", sizeof(driver_data->media_ready[1].size_name));
  }
  else if (!strncmp(driver_name, "pwg_common-", 11))
  {
    strlcpy(driver_data->make_and_model, "PWG Office Printer", sizeof(driver_data->make_and_model));

    driver_data->kind       = PAPPL_KIND_DOCUMENT | PAPPL_KIND_PHOTO | PAPPL_KIND_POSTCARD | PAPPL_KIND_ROLL;
    driver_data->left_right = 423;	// 1/6" left and right
    driver_data->bottom_top = 423;	// 1/6" top and bottom

    driver_data->num_media = (int)(sizeof(pwg_common_media) / sizeof(pwg_common_media[0]));
    memcpy(driver_data->media, pwg_common_media, sizeof(pwg_common_media));

    driver_data->num_source = 4;
    driver_data->source[0]  = "main";
    driver_data->source[1]  = "alternate";
    driver_data->source[2]  = "manual";
    driver_data->source[3]  = "by-pass-tray";

    strlcpy(driver_data->media_ready[0].size_name, "na_letter_8.5x11in", sizeof(driver_data->media_ready[0].size_name));
    strlcpy(driver_data->media_ready[1].size_name, "iso_a4_210x297mm", sizeof(driver_data->media_ready[1].size_name));
  }
  else
  {
    papplLog(system, PAPPL_LOGLEVEL_ERROR, "No dimension information in driver name '%s'.", driver_name);
    return (false);
  }

  if (!strncmp(driver_name, "pwg_common-", 11))
  {
    driver_data->num_type = 7;
    driver_data->type[0]  = "stationery";
    driver_data->type[1]  = "stationery-letterhead";
    driver_data->type[2]  = "labels";
    driver_data->type[3]  = "photographic";
    driver_data->type[4]  = "photographic-glossy";
    driver_data->type[5]  = "photographic-matte";
    driver_data->type[6]  = "transparency";

    driver_data->media_default.bottom_margin = driver_data->bottom_top;
    driver_data->media_default.left_margin   = driver_data->left_right;
    driver_data->media_default.right_margin  = driver_data->left_right;
    driver_data->media_default.size_width    = 21590;
    driver_data->media_default.size_length   = 27940;
    driver_data->media_default.top_margin    = driver_data->bottom_top;
    strlcpy(driver_data->media_default.size_name, "na_letter_8.5x11in", sizeof(driver_data->media_default.size_name));
    strlcpy(driver_data->media_default.source, "main", sizeof(driver_data->media_default.source));
    strlcpy(driver_data->media_default.type, "stationery", sizeof(driver_data->media_default.type));
  }
  else
  {
    driver_data->icons[0].data    = label_sm_png;
    driver_data->icons[0].datalen = sizeof(label_sm_png);
    driver_data->icons[1].data    = label_md_png;
    driver_data->icons[1].datalen = sizeof(label_md_png);
    driver_data->icons[2].data    = label_lg_png;
    driver_data->icons[2].datalen = sizeof(label_lg_png);

    driver_data->tracking_supported = PAPPL_MEDIA_TRACKING_MARK | PAPPL_MEDIA_TRACKING_CONTINUOUS;

    driver_data->num_type = 3;
    driver_data->type[0]  = "continuous";
    driver_data->type[1]  = "labels";
    driver_data->type[2]  = "labels-continuous";

    driver_data->media_default.bottom_margin = driver_data->bottom_top;
    driver_data->media_default.left_margin   = driver_data->left_right;
    driver_data->media_default.right_margin  = driver_data->left_right;
    driver_data->media_default.size_width    = 3175;
    driver_data->media_default.size_length   = 8890;
    driver_data->media_default.top_margin    = driver_data->bottom_top;
    driver_data->media_default.tracking      = PAPPL_MEDIA_TRACKING_MARK;
    strlcpy(driver_data->media_default.size_name, "oe_address-label_1.25x3.5in", sizeof(driver_data->media_default.size_name));
    strlcpy(driver_data->media_default.source, "main-roll", sizeof(driver_data->media_default.source));
    strlcpy(driver_data->media_default.type, "labels", sizeof(driver_data->media_default.type));
  }

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
      strlcpy(driver_data->media_ready[i].source, driver_data->source[i], sizeof(driver_data->media_ready[i].source));
      strlcpy(driver_data->media_ready[i].type, "labels", sizeof(driver_data->media_ready[i].type));
    }
  }

//  driver_data->num_supply = 0;

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
  (void)message;

  // TODO: Open console and send BEL character to it instead...
  putchar(7);
  fflush(stdout);
}


//
// 'pwg_print()' - Print a file.
//

static bool				// O - `true` on success, `false` on failure
pwg_print(
    pappl_job_t     *job,		// I - Job
    pappl_options_t *options)		// I - Job options
{
  int		infd,			// Input file
		outfd;			// Output file
  char		outname[1024];		// Output filename
  ssize_t	bytes;			// Bytes read/written
  char		buffer[65536];		// Read/write buffer


  papplJobSetImpressions(job, 1);

  infd  = open(papplJobGetFilename(job), O_RDONLY);
  outfd = papplJobCreateFile(job, outname, sizeof(outname), ".", "pwg");

  while ((bytes = read(infd, buffer, sizeof(buffer))) > 0)
    write(outfd, buffer, (size_t)bytes);

  close(infd);
  close(outfd);

  papplJobSetImpressionsCompleted(job, 1);

  return (true);
}


//
// 'pwg_rendjob()' - End a job.
//

static bool				// O - `true` on success, `false` on failure
pwg_rendjob(
    pappl_job_t     *job,		// I - Job
    pappl_options_t *options)		// I - Job options
{
  pwg_job_data_t	*pwg = (pwg_job_data_t *)papplJobGetData(job);
					// Job data

  (void)options;

  cupsRasterClose(pwg->ras);
  close(pwg->fd);

  free(pwg);
  papplJobSetData(job, NULL);

  return (true);
}


//
// 'pwg_rendpage()' - End a page.
//

static bool				// O - `true` on success, `false` on failure
pwg_rendpage(
    pappl_job_t     *job,		// I - Job
    pappl_options_t *options,		// I - Job options
    unsigned         page)		// I - Page number
{
  (void)job;
  (void)options;
  (void)page;

  return (true);
}


//
// 'pwg_rstartjob()' - Start a job.
//

static bool				// O - `true` on success, `false` on failure
pwg_rstartjob(
    pappl_job_t     *job,		// I - Job
    pappl_options_t *options)		// I - Job options
{
  pwg_job_data_t	*pwg = (pwg_job_data_t *)calloc(1, sizeof(pwg_job_data_t));
					// PWG driver data
  char		outname[1024];		// Output filename


  (void)options;

  papplJobSetData(job, pwg);

  pwg->fd  = papplJobCreateFile(job, outname, sizeof(outname), ".", "pwg");
  pwg->ras = cupsRasterOpen(pwg->fd, CUPS_RASTER_WRITE_PWG);

  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "Writing PWG output to '%s'.", outname);

  return (1);
}


//
// 'pwg_rstartpage()' - Start a page.
//

static bool				// O - `true` on success, `false` on failure
pwg_rstartpage(
    pappl_job_t     *job,		// I - Job
    pappl_options_t *options,		// I - Job options
    unsigned        page)		// I - Page number
{
  pwg_job_data_t	*pwg = (pwg_job_data_t *)papplJobGetData(job);
					// PWG driver data

  (void)page;

  return (cupsRasterWriteHeader2(pwg->ras, &options->header) != 0);
}


//
// 'pwg_rwrite()' - Write a raster line.
//

static bool				// O - `true` on success, `false` on failure
pwg_rwrite(
    pappl_job_t         *job,		// I - Job
    pappl_options_t     *options,	// I - Job options
    unsigned            y,		// I - Line number
    const unsigned char *line)		// I - Line
{
  pwg_job_data_t	*pwg = (pwg_job_data_t *)papplJobGetData(job);
					// PWG driver data

  (void)y;

  return (cupsRasterWritePixels(pwg->ras, (unsigned char *)line, options->header.cupsBytesPerLine) != 0);
}


//
// 'pwg_status()' - Get current printer status.
//

static bool				// O - `true` on success, `false` on failure
pwg_status(
    pappl_printer_t *printer)		// I - Printer
{
  (void)printer;

  puts("Printer status callback.");

  return (true);
}
