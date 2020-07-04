//
// HP-Printer app for the Printer Application Framework
//
// Copyright © 2020 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

//
// Include necessary headers...
//

# include <pappl/pappl.h>


//
// Types...
//

typedef struct pcl_s            // Job data
{
  unsigned char *planes[4],     // Output buffers
		  *comp_buffer;   // Compression buffer
  unsigned 	  num_planes,     // Number of color planes
		  feed;           // Number of lines to skip
} pcl_t;


//
// Local globals...
//

static const char * const pcl_hp_deskjet_media[] =
{       // Supported media sizes for HP Deskjet printers
  "na_letter_8.5x11in",
  "na_legal_8.5x14in",
  "executive_7x10in",
  "na_tabloid_11x17in",
  "iso_a3_11.7x16.5in",
  "iso_a4_8.3x11.7in",
  "iso_a5_5.8x8.3in",
  "jis_b5_7.2x10.1in",
  "env_b5_6.9x9.8in",
  "env_10_4.125x9.5in",
  "env_c5_6.4x9in",
  "env_dl_8.66x4.33in",
  "env_monarch_3.875x7.5in"
};

static const char * const pcl_generic_pcl_media[] =
{       // Supported media sizes for Generic PCL printers
  "na_letter_8.5x11in",
  "na_legal_8.5x14in",
  "executive_7x10in",
  "na_tabloid_11x17in",
  "iso_a3_11.7x16.5in",
  "iso_a4_8.3x11.7in",
  "iso_a5_5.8x8.3in",
  "jis_b5_7.2x10.1in",
  "env_b5_6.9x9.8in",
  "env_10_4.125x9.5in",
  "env_c5_6.4x9in",
  "env_dl_8.66x4.33in",
  "env_monarch_3.875x7.5in"
};

static const char * const pcl_hp_laserjet_media[] =
{       // Supported media sizes for HP Laserjet printers
  "na_letter_8.5x11in",
  "na_legal_8.5x14in",
  "executive_7x10in",
  "na_tabloid_11x17in",
  "iso_a3_11.7x16.5in",
  "iso_a4_8.3x11.7in",
  "iso_a5_5.8x8.3in",
  "jis_b5_7.2x10.1in",
  "env_b5_6.9x9.8in",
  "env_10_4.125x9.5in",
  "env_c5_6.4x9in",
  "env_dl_8.66x4.33in",
  "env_monarch_3.875x7.5in"
};


//
// Local functions...
//

static bool   pcl_callback(pappl_system_t *system, const char *driver_name, const char *device_uri, pappl_pdriver_data_t *driver_data, ipp_t **driver_attrs, void *data);
static void   pcl_compress_data(pappl_job_t *job, pappl_device_t *device, unsigned char *line, unsigned length, unsigned plane, unsigned type);
static void   pcl_identify(pappl_printer_t *printer, pappl_identify_actions_t actions, const char *message);
static bool   pcl_print(pappl_job_t *job, pappl_poptions_t *options, pappl_device_t *device);
static bool   pcl_rendjob(pappl_job_t *job, pappl_poptions_t *options, pappl_device_t *device);
static bool   pcl_rendpage(pappl_job_t *job, pappl_poptions_t *options, pappl_device_t *device, unsigned page);
static bool   pcl_rstartjob(pappl_job_t *job, pappl_poptions_t *options, pappl_device_t *device);
static bool   pcl_rstartpage(pappl_job_t *job, pappl_poptions_t *options, pappl_device_t *device, unsigned page);
static bool   pcl_rwrite(pappl_job_t *job, pappl_poptions_t *options, pappl_device_t *device, unsigned y, const unsigned char *pixels);
static void   pcl_setup(pappl_system_t *system);
static bool   pcl_status(pappl_printer_t *printer);
#ifndef HAVE_STRLCPY
  static size_t strlcpy(char *dst, const char *src, size_t dstsize);
#endif // !HAVE_STRLCPY
static pappl_system_t   *system_cb(int num_options, cups_option_t *options, void *data);


//
// 'main()' - Main entry for the hp-printer-app.
//

int
main(int  argc,				// I - Number of command-line arguments
     char *argv[])			// I - Command-line arguments
{
  papplMainloop(argc, argv, "1.0", NULL, NULL, NULL, system_cb, "hp_printer_app");
  return (0);
}


//
// 'pcl_callback()' - PCL callback.
//

static bool				   // O - `true` on success, `false` on failure
pcl_callback(
    pappl_system_t       *system,	   // I - System
    const char           *driver_name,   // I - Driver name
    const char           *device_uri,	   // I - Device URI
    pappl_pdriver_data_t *driver_data,   // O - Driver data
    ipp_t                **driver_attrs, // O - Driver attributes
    void                 *data)	   // I - Callback data
{
  int   i;                               // Looping variable


  if (!driver_name || !device_uri || !driver_data || !driver_attrs)
  {
    papplLog(system, PAPPL_LOGLEVEL_ERROR, "Driver callback called without required information.");
    return (false);
  }

  if (!data || strcmp((const char *)data, "hp_printer_app"))
  {
    papplLog(system, PAPPL_LOGLEVEL_ERROR, "Driver callback called with bad data pointer.");
    return (false);
  }

  driver_data->identify           = pcl_identify;
  driver_data->identify_default   = PAPPL_IDENTIFY_ACTIONS_SOUND;
  driver_data->identify_supported = PAPPL_IDENTIFY_ACTIONS_DISPLAY | PAPPL_IDENTIFY_ACTIONS_SOUND;
  driver_data->print              = pcl_print;
  driver_data->rendjob            = pcl_rendjob;
  driver_data->rendpage           = pcl_rendpage;
  driver_data->rstartjob          = pcl_rstartjob;
  driver_data->rstartpage         = pcl_rstartpage;
  driver_data->rwrite             = pcl_rwrite;
  driver_data->status             = pcl_status;
  driver_data->format             = "application/vnd.hp-postscript";
  driver_data->orient_default     = IPP_ORIENT_NONE;
  driver_data->quality_default    = IPP_QUALITY_NORMAL;

  if (!strcmp(driver_name, "hp_deskjet"))
  {
    strlcpy(driver_data->make_and_model, "HP DeskJet", sizeof(driver_data->make_and_model));

    driver_data->num_resolution  = 3;
    driver_data->x_resolution[0] = 150;
    driver_data->y_resolution[0] = 150;
    driver_data->x_resolution[1] = 300;
    driver_data->y_resolution[1] = 300;
    driver_data->x_resolution[2] = 600;
    driver_data->y_resolution[2] = 600;
    driver_data->x_default = driver_data->y_default = 300;

    driver_data->raster_types = PAPPL_PWG_RASTER_TYPE_BLACK_1 | PAPPL_PWG_RASTER_TYPE_BLACK_8 | PAPPL_PWG_RASTER_TYPE_SGRAY_8 | PAPPL_PWG_RASTER_TYPE_SRGB_8;

    driver_data->color_supported = PAPPL_COLOR_MODE_AUTO | PAPPL_COLOR_MODE_AUTO_MONOCHROME | PAPPL_COLOR_MODE_COLOR | PAPPL_COLOR_MODE_MONOCHROME;
    driver_data->color_default   = PAPPL_COLOR_MODE_AUTO;

    driver_data->num_media = (int)(sizeof(pcl_hp_deskjet_media) / sizeof(pcl_hp_deskjet_media[0]));
    memcpy(driver_data->media, pcl_hp_deskjet_media, sizeof(pcl_hp_deskjet_media));

    driver_data->sides_supported = PAPPL_SIDES_ONE_SIDED;
    driver_data->sides_default   = PAPPL_SIDES_ONE_SIDED;

    driver_data->num_source = 3;
    driver_data->source[0]  = "tray-1";
    driver_data->source[1]  = "manual";
    driver_data->source[2]  = "envelope";

    driver_data->num_type = 5;
    driver_data->type[0] = "stationery";
    driver_data->type[1] = "bond";
    driver_data->type[2] = "special";
    driver_data->type[3] = "transparency";
    driver_data->type[4] = "photographic-glossy";

    driver_data->left_right = 635;	 // 1/4" left and right
    driver_data->bottom_top = 1270;	 // 1/2" top and bottom

    for (i = 0; i < driver_data->num_source; i ++)
    {
      if (strcmp(driver_data->source[i], "envelope"))
        snprintf(driver_data->media_ready[i].size_name, sizeof(driver_data->media_ready[i].size_name), "na_letter_8.5x11in");
      else
        snprintf(driver_data->media_ready[i].size_name, sizeof(driver_data->media_ready[i].size_name), "env_10_4.125x9.5in");
    }
  }
  else if (!strcmp(driver_name, "hp_generic"))
  {
    strlcpy(driver_data->make_and_model, "Generic PCL Laser Printer", sizeof(driver_data->make_and_model));

    driver_data->num_resolution  = 2;
    driver_data->x_resolution[0] = 300;
    driver_data->y_resolution[0] = 300;
    driver_data->x_resolution[1] = 600;
    driver_data->y_resolution[1] = 600;
    driver_data->x_default = driver_data->y_default = 300;

    driver_data->raster_types = PAPPL_PWG_RASTER_TYPE_BLACK_1 | PAPPL_PWG_RASTER_TYPE_BLACK_8 | PAPPL_PWG_RASTER_TYPE_SGRAY_8;
    driver_data->force_raster_type = PAPPL_PWG_RASTER_TYPE_BLACK_1;

    driver_data->color_supported = PAPPL_COLOR_MODE_MONOCHROME;
    driver_data->color_default   = PAPPL_COLOR_MODE_MONOCHROME;

    driver_data->num_media = (int)(sizeof(pcl_generic_pcl_media) / sizeof(pcl_generic_pcl_media[0]));
    memcpy(driver_data->media, pcl_generic_pcl_media, sizeof(pcl_generic_pcl_media));

    driver_data->sides_supported = PAPPL_SIDES_ONE_SIDED | PAPPL_SIDES_TWO_SIDED_LONG_EDGE | PAPPL_SIDES_TWO_SIDED_SHORT_EDGE;
    driver_data->sides_default   = PAPPL_SIDES_ONE_SIDED;

    driver_data->num_source = 7;
    driver_data->source[0]  = "default";
    driver_data->source[1]  = "tray-1";
    driver_data->source[2]  = "tray-2";
    driver_data->source[3]  = "tray-3";
    driver_data->source[4]  = "tray-4";
    driver_data->source[5]  = "manual";
    driver_data->source[6]  = "envelope";

    driver_data->left_right = 635;	// 1/4" left and right
    driver_data->bottom_top = 423;	// 1/6" top and bottom

    for (i = 0; i < driver_data->num_source; i ++)
    {
      if (strcmp(driver_data->source[i], "envelope"))
        snprintf(driver_data->media_ready[i].size_name, sizeof(driver_data->media_ready[i].size_name), "na_letter_8.5x11in");
      else
        snprintf(driver_data->media_ready[i].size_name, sizeof(driver_data->media_ready[i].size_name), "env_10_4.125x9.5in");
    }
  }
  else if (!strcmp(driver_name, "hp_laserjet"))
  {
   strlcpy(driver_data->make_and_model, "HP LaserJet", sizeof(driver_data->make_and_model));

    driver_data->num_resolution  = 3;
    driver_data->x_resolution[0] = 150;
    driver_data->y_resolution[0] = 150;
    driver_data->x_resolution[1] = 300;
    driver_data->y_resolution[1] = 300;
    driver_data->x_resolution[2] = 600;
    driver_data->y_resolution[2] = 600;
    driver_data->x_default = driver_data->y_default = 300;

    driver_data->raster_types = PAPPL_PWG_RASTER_TYPE_BLACK_1 | PAPPL_PWG_RASTER_TYPE_BLACK_8 | PAPPL_PWG_RASTER_TYPE_SGRAY_8;
    driver_data->force_raster_type = PAPPL_PWG_RASTER_TYPE_BLACK_1;

    driver_data->color_supported = PAPPL_COLOR_MODE_MONOCHROME;
    driver_data->color_default   = PAPPL_COLOR_MODE_MONOCHROME;

    driver_data->num_media = (int)(sizeof(pcl_hp_laserjet_media) / sizeof(pcl_hp_laserjet_media[0]));
    memcpy(driver_data->media, pcl_hp_laserjet_media, sizeof(pcl_hp_laserjet_media));

    driver_data->sides_supported = PAPPL_SIDES_ONE_SIDED | PAPPL_SIDES_TWO_SIDED_LONG_EDGE | PAPPL_SIDES_TWO_SIDED_SHORT_EDGE;
    driver_data->sides_default   = PAPPL_SIDES_ONE_SIDED;

    driver_data->num_source = 7;
    driver_data->source[0]  = "default";
    driver_data->source[1]  = "tray-1";
    driver_data->source[2]  = "tray-2";
    driver_data->source[3]  = "tray-3";
    driver_data->source[4]  = "tray-4";
    driver_data->source[5]  = "manual";
    driver_data->source[6]  = "envelope";

    driver_data->left_right = 635;	 // 1/4" left and right
    driver_data->bottom_top = 1270;	 // 1/2" top and bottom

    for (i = 0; i < driver_data->num_source; i ++)
    {
      if (strcmp(driver_data->source[i], "envelope"))
        snprintf(driver_data->media_ready[i].size_name, sizeof(driver_data->media_ready[i].size_name), "na_letter_8.5x11in");
      else
        snprintf(driver_data->media_ready[i].size_name, sizeof(driver_data->media_ready[i].size_name), "env_10_4.125x9.5in");
    }
  }
  else
  {
    papplLog(system, PAPPL_LOGLEVEL_ERROR, "No dimension information in driver name '%s'.", driver_name);
    return (false);
  }

  // Fill out ready and default media (default == ready media from the first source)
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
      snprintf(driver_data->media_ready[i].source, sizeof(driver_data->media_ready[i].source), "%s", driver_data->source[i]);
      snprintf(driver_data->media_ready[i].type, sizeof(driver_data->media_ready[i].type), "%s", driver_data->type[0]);
    }
  }

  driver_data->media_default = driver_data->media_ready[0];

  return (true);
}


//
// 'pcl_compress_data()' - Compress a line of graphics.
//

static void
pcl_compress_data(
    pappl_job_t    *job,        // I - Job object
    pappl_device_t *device,     // I - Device
    unsigned char  *line,       // I - Data to compress
    unsigned       length,      // I - Number of bytes
    unsigned       plane,       // I - Color plane
    unsigned       type)        // I - Type of compression
{
  unsigned char    *line_ptr,   // Current byte pointer
                   *line_end,   // End-of-line byte pointer
                   *comp_ptr,   // Pointer into compression buffer
                   *start;      // Start of compression sequence
  unsigned         count;       // Count of bytes for output
  pcl_t      *pcl = (pcl_t *)papplJobGetData(job);
                                // Job data


  switch (type)
  {
    default : // No compression...
      line_ptr = line;
      line_end = line + length;
      break;

    case 1 :  // Do run-length encoding...
      line_end = line + length;
      for (line_ptr = line, comp_ptr = pcl->comp_buffer; line_ptr < line_end; comp_ptr += 2, line_ptr += count)
      {
        count = 1;
        while ((line_ptr + count) < line_end && line_ptr[0] == line_ptr[count] && count < 256)
          count ++;

        comp_ptr[0] = (unsigned char)(count - 1);
        comp_ptr[1] = line_ptr[0];
      }

      line_ptr = pcl->comp_buffer;
      line_end = comp_ptr;
      break;

    case 2 :  // Do TIFF pack-bits encoding...
      line_ptr = line;
      line_end = line + length;
      comp_ptr = pcl->comp_buffer;

      while (line_ptr < line_end)
      {
        if ((line_ptr + 1) >= line_end)
        {
          // Single byte on the end...

          *comp_ptr++ = 0x00;
          *comp_ptr++ = *line_ptr++;
        }
        else if (line_ptr[0] == line_ptr[1])
        {
          // Repeated sequence...

          line_ptr ++;
          count = 2;

          while (line_ptr < (line_end - 1) && line_ptr[0] == line_ptr[1] && count < 127)
          {
            line_ptr ++;
            count ++;
          }

          *comp_ptr++ = (unsigned char)(257 - count);
          *comp_ptr++ = *line_ptr++;
        }
        else
        {
          // Non-repeated sequence...

          start    = line_ptr;
          line_ptr ++;
          count    = 1;

          while (line_ptr < (line_end - 1) && line_ptr[0] != line_ptr[1] && count < 127)
          {
            line_ptr ++;
            count ++;
          }

          *comp_ptr++ = (unsigned char)(count - 1);

          memcpy(comp_ptr, start, count);
          comp_ptr += count;
        }
	    }

      line_ptr = pcl->comp_buffer;
      line_end = comp_ptr;
	break;
  }

  //
  // Set the length of the data and write a raster plane...
  //

  papplDevicePrintf(device, "\033*b%d%c", (int)(line_end - line_ptr), plane);
  papplDeviceWrite(device, line_ptr, (size_t)(line_end - line_ptr));
}


//
// 'pcl_identify()' - Identify the printer.
//

static void
pcl_identify(
    pappl_printer_t          *printer,	// I - Printer
    pappl_identify_actions_t actions, 	// I - Actions to take
    const char               *message)	// I - Message, if any
{
  (void)printer;
  (void)actions;

  // Identify a printer using display, flash, sound or speech.
}


//
// 'pcl_print()' - Print file.
//

static bool                           // O - `true` on success, `false` on failure
pcl_print(
    pappl_job_t      *job,            // I - Job
    pappl_poptions_t *options,        // I - Options
    pappl_device_t   *device)         // I - Device
{
  int		       infd;	        // Input file
  ssize_t	       bytes;	        // Bytes read/written
  char	       buffer[65536];	// Read/write buffer


  papplJobSetImpressions(job, 1);

  infd  = open(papplJobGetFilename(job), O_RDONLY);

  while ((bytes = read(infd, buffer, sizeof(buffer))) > 0)
  {
    if (papplDeviceWrite(device, buffer, (size_t)bytes) < 0)
    {
      papplLogJob(job, PAPPL_LOGLEVEL_ERROR, "Unable to send %d bytes to printer.", (int)bytes);
      close(infd);
      return (false);
    }
  }

  close(infd);

  papplJobSetImpressionsCompleted(job, 1);

  return (true);
}


//
// 'pcl_rendjob()' - End a job.
//

static bool                     // O - `true` on success, `false` on failure
pcl_rendjob(
    pappl_job_t      *job,      // I - Job
    pappl_poptions_t *options,  // I - Options
    pappl_device_t   *device)   // I - Device
{
  pcl_t	       *pcl = (pcl_t *)papplJobGetData(job);
				  // Job data


  (void)options;

  free(pcl);
  papplJobSetData(job, NULL);

  return (true);
}


//
// 'pcl_rendpage()' - End a page.
//

static bool                     // O - `true` on success, `false` on failure
pcl_rendpage(
    pappl_job_t      *job,      // I - Job
    pappl_poptions_t *options,  // I - Job options
    pappl_device_t   *device,   // I - Device
    unsigned         page)      // I - Page number
{
  pcl_t        *pcl = (pcl_t *)papplJobGetData(job);
                                // Job data


  // Eject the current page...

  if (pcl->num_planes > 1)
  {
    papplDevicePuts(device, "\033*rC"); // End color GFX

    if (!((&(options->header))->Duplex && (page & 1)))
      papplDevicePuts(device, "\033&l0H");  // Eject current page
  }
  else
  {
    papplDevicePuts(device, "\033*r0B");  // End GFX

    if (!((&(options->header))->Duplex && (page & 1)))
      papplDevicePuts(device, "\014");  // Eject current page
  }

  papplDeviceFlush(device);

  // Free memory...

  free(pcl->planes[0]);

  if (pcl->comp_buffer)
    free(pcl->comp_buffer);

  return (true);
}


//
// 'pcl_rstartjob()' - Start a job.
//

static bool                     // O - `true` on success, `false` on failure
pcl_rstartjob(
    pappl_job_t      *job,      // I - Job
    pappl_poptions_t *options,  // I - Job options
    pappl_device_t   *device)   // I - Device
{
  pcl_t        *pcl = (pcl_t *)calloc(1, sizeof(pcl_t));
				  // Job data


  (void)options;

  papplJobSetData(job, pcl);

  papplDevicePrintf(device, "\033E"); // PCL reset sequence

  return (true);
}


//
// 'pcl_rstartpage()' - Start a page.
//

static bool                      // O - `true` on success, `false` on failure
pcl_rstartpage(
    pappl_job_t       *job,       // I - Job
    pappl_poptions_t  *options,   // I - Job options
    pappl_device_t    *device,    // I - Device
    unsigned          page)       // I - Page number
{
  unsigned            plane,    // Looping var
                      length;   // Bytes to write
  cups_page_header2_t *header = &(options->header);
                                // Page header
  pcl_t               *pcl = (pcl_t *)papplJobGetData(job);
                                // Job data


  //
  // Setup printer/job attributes...
  //

  if ((!header->Duplex || (page & 1)) && header->MediaPosition)
    papplDevicePrintf(device, "\033&l%dH", header->MediaPosition);  // Set media position

  if (!header->Duplex || (page & 1))
  {
    // Set the media size...

    papplDevicePuts(device, "\033&l6D\033&k12H"); // Set 6 LPI, 10 CPI
    papplDevicePuts(device, "\033&l0O");  // Set portrait orientation

    // Set page size
    switch (header->PageSize[1])
    {
      case 540 : // Monarch Envelope
          papplDevicePuts(device, "\033&l80A");
	  break;

      case 595 : // A5
          papplDevicePuts(device, "\033&l25A");
	  break;

      case 624 : // DL Envelope
          papplDevicePuts(device, "\033&l90A");
	  break;

      case 649 : // C5 Envelope
          papplDevicePuts(device, "\033&l91A");
	  break;

      case 684 : // COM-10 Envelope
          papplDevicePuts(device, "\033&l81A");
	  break;

      case 709 : // B5 Envelope
          papplDevicePuts(device, "\033&l100A");
	  break;

      case 756 : // Executive
          papplDevicePuts(device, "\033&l1A");
	  break;

      case 792 : // Letter
          papplDevicePuts(device, "\033&l2A");
	  break;

      case 842 : // A4
          papplDevicePuts(device, "\033&l26A");
	  break;

      case 1008 : // Legal
          papplDevicePuts(device, "\033&l3A");
	  break;

      case 1191 : // A3
          papplDevicePuts(device, "\033&l27A");
	  break;

      case 1224 : // Tabloid
          papplDevicePuts(device, "\033&l6A");
	  break;
    }

    papplDevicePrintf(device, "\033&l%dP", header->PageSize[1] / 12); // Set page length
    papplDevicePuts(device, "\033&l0E");  // Set top margin to 0

    // Set other job options...

    papplDevicePrintf(device, "\033&l%dX", header->NumCopies);  // Set number of copies

    if (header->cupsMediaType)
      papplDevicePrintf(device, "\033&l%dM", header->cupsMediaType);  // Set media type

    int mode = header->Duplex ? 1 + header->Tumble != 0 : 0;

    papplDevicePrintf(device, "\033&l%dS", mode); // Set duplex mode
    papplDevicePuts(device, "\033&l0L");  // Turn off perforation skip
  }
  else
    papplDevicePuts(device, "\033&a2G");  // Set back side

  // Set graphics mode...

  papplDevicePrintf(device, "\033*t%uR", header->HWResolution[0]);  // Set resolution

  if (header->cupsColorSpace == CUPS_CSPACE_SRGB)
  {
    pcl->num_planes = 4;
    papplDevicePuts(device, "\033*r-4U"); // Set KCMY graphics
  }
  else
    pcl->num_planes = 1;  // Black&white graphics

  // Set size and position of graphics...

  papplDevicePrintf(device, "\033*r%uS", header->cupsWidth);  // Set width
  papplDevicePrintf(device, "\033*r%uT", header->cupsHeight); // Set height

  papplDevicePuts(device, "\033&a0H");  // Set horizontal position

  papplDevicePrintf(device, "\033&a%.0fV", 0.2835 * ((&(options->media))->size_length - (&(options->media))->top_margin));
                                                             // Set vertical position

  papplDevicePuts(device, "\033*r1A");  // Start graphics

  if (header->cupsCompression)
    papplDevicePrintf(device, "\033*b%uM", header->cupsCompression);  // Set compression

  pcl->feed = 0; // No blank lines yet
  length = (header->cupsWidth + 7) / 8;

  // Allocate memory for a line of graphics...

  if ((pcl->planes[0] = malloc(length * pcl->num_planes)) == NULL)
  {
    papplLogJob(job, PAPPL_LOGLEVEL_ERROR, "Malloc failure...\n");
    return (false);
  }

  for (plane = 1; plane < pcl->num_planes; plane ++)
    pcl->planes[plane] = pcl->planes[0] + plane * length;

  if (header->cupsCompression)
    pcl->comp_buffer = malloc(header->cupsBytesPerLine * 2 + 2);
  else
    pcl->comp_buffer = NULL;

  return (true);
}


//
// 'pcl_rwrite()' - Write a line.
//

static bool                         // O - `true` on success, `false` on failure
pcl_rwrite(
    pappl_job_t         *job,       // I - Job
    pappl_poptions_t    *options,   // I - Job options
    pappl_device_t      *device,    // I - Device
    unsigned            y,          // I - Line number
    const unsigned char *pixels)      // I - Line
{
  cups_page_header2_t   *header = &(options->header);
                                    // Page header
  pcl_t           *pcl = (pcl_t *)papplJobGetData(job);
                                    // Job data
  unsigned	          plane,      // Current plane
		          bytes,      // Bytes to write
		          x;          // Current column
  unsigned char	  bit,	    // Current plane data
		          *pixptr,// Pixel pointer in line
		          *cptr,      // Pointer into c-plane
		          *mptr,      // Pointer into m-plane
		          *yptr,      // Pointer into y-plane
		          *kptr,      // Pointer into k-plane
		          byte;      // Byte in line
  const unsigned char	*dither;	// Dither line


  pcl->planes[0] = (unsigned char *)strdup((const char *)pixels);

  if (pcl->planes[0][0] || memcmp(pcl->planes[0], pcl->planes[0] + 1, header->cupsBytesPerLine - 1))
  {
    // Output whitespace as needed...

    if (pcl->feed > 0)
    {
      papplDevicePrintf(device, "\033*b%dY", pcl->feed);
      pcl->feed = 0;
    }

    // Write bitmap data as needed...

    bytes = (header->cupsWidth + 7) / 8;
    dither = options->dither[y & 15];

    if (pcl->num_planes > 1)
    {
      // RGB
      memset(pcl->planes, 0, pcl->num_planes * bytes);

      for (x = 0, cptr = pcl->planes[0], mptr = pcl->planes[1], yptr = pcl->planes[2], kptr = pcl->planes[3], pixptr = pixels, bit = 128; x < header->cupsWidth; x ++)
      {
        if (*pixptr ++ <= dither[x & 15])
          *cptr |= bit;
        if (*pixptr ++ <= dither[x & 15])
          *mptr |= bit;
        if (*pixptr ++ <= dither[x & 15])
          *yptr |= bit;

        if (bit == 1)
        {
          *kptr = *cptr & *mptr & *yptr;
          byte  = ~ *kptr ++;
          bit   = 128;
          *cptr ++ &= byte;
          *mptr ++ &= byte;
          *yptr ++ &= byte;
        }
        else
          bit /= 2;
      }
    }
    else if (header->cupsBitsPerPixel == 8)
    {
      memset(pcl->planes, 0, bytes);

      if (header->cupsColorSpace == CUPS_CSPACE_K)
      {
        // 8 bit black
        for (x = 0, kptr = pcl->planes[0], pixptr = pixels, bit = 128, byte = 0; x < header->cupsWidth; x ++, pixptr ++)
        {
          if (*pixptr > dither[x & 15])
            byte |= bit;

          if (bit == 1)
          {
            *kptr++ = byte;
            byte    = 0;
            bit     = 128;
          }
          else
            bit /= 2;
        }

        if (bit < 128)
          *kptr = byte;
      }
      else
      {
        // 8 bit gray
        for (x = 0, kptr = pcl->planes[0], pixptr = pixels, bit = 128, byte = 0; x < header->cupsWidth; x ++, pixptr ++)
        {
          if (*pixptr <= dither[x & 15])
            byte |= bit;

          if (bit == 1)
          {
            *kptr++ = byte;
            byte    = 0;
            bit     = 128;
          }
          else
            bit /= 2;
        }

        if (bit < 128)
          *kptr = byte;
      }
    }

    for (plane = 0; plane < pcl->num_planes; plane ++)
    {
      pcl_compress_data(job, device, pcl->planes[plane], bytes, plane < (pcl->num_planes - 1) ? 'V' : 'W', header->cupsCompression);
    }

    papplDeviceFlush(device);
  }
  else
    pcl->feed ++;

  return (true);
}


//
// 'pcl_setup()' - Setup PCL drivers.
//

static void
pcl_setup(
    pappl_system_t *system)      // I - System
{
  static const char * const names[] =   // Driver names
  {
    "hp_deskjet",
    "hp_generic",
    "hp_laserjet"
  };

  static const char * const desc[] =    // Driver descriptions
  {
    "HP Deskjet",
    "Generic PCL",
    "HP Laserjet"
  };

  papplSystemSetPrintDrivers(system, (int)(sizeof(names) / sizeof(names[0])), names, desc, pcl_callback, "hp_printer_app");
}


//
// 'pcl_status()' - Get printer status.
//

static bool                   // O - `true` on success, `false` on failure
pcl_status(
    pappl_printer_t *printer) // I - Printer
{
  char	driver_name[256];     // Driver name


  if (!strcmp(papplPrinterGetDriverName(printer, driver_name, sizeof(driver_name)), "hp_deskjet"))
  {
    static pappl_supply_t supply[5] =	// Supply level data
    {
      { PAPPL_SUPPLY_COLOR_CYAN,     "Cyan Ink",       true, 100, PAPPL_SUPPLY_TYPE_INK },
      { PAPPL_SUPPLY_COLOR_MAGENTA,  "Magenta Ink",    true, 100, PAPPL_SUPPLY_TYPE_INK },
      { PAPPL_SUPPLY_COLOR_YELLOW,   "Yellow Ink",     true, 100, PAPPL_SUPPLY_TYPE_INK },
      { PAPPL_SUPPLY_COLOR_BLACK,    "Black Ink",      true, 100, PAPPL_SUPPLY_TYPE_INK },
      { PAPPL_SUPPLY_COLOR_NO_COLOR, "Waste Ink Tank", true, 0, PAPPL_SUPPLY_TYPE_WASTE_INK }
    };

    if (papplPrinterGetSupplies(printer, 0, supply) == 0)
      papplPrinterSetSupplies(printer, (int)(sizeof(supply) / sizeof(supply[0])), supply);
  }
  else
  {
    static pappl_supply_t supply[2] =	// Supply level data
    {
      { PAPPL_SUPPLY_COLOR_BLACK,    "Black Toner", true, 100, PAPPL_SUPPLY_TYPE_TONER },
      { PAPPL_SUPPLY_COLOR_NO_COLOR, "Waste Toner", true, 0, PAPPL_SUPPLY_TYPE_WASTE_TONER }
    };

    if (papplPrinterGetSupplies(printer, 0, supply) == 0)
      papplPrinterSetSupplies(printer, (int)(sizeof(supply) / sizeof(supply[0])), supply);
  }

  return (true);
}


//
// 'strlcpy()' - Safely copy a C string.
//

#ifndef HAVE_STRLCPY
static size_t
strlcpy(char       *dst,		// I - Destination buffer
               const char *src,		// I - Source string
               size_t     dstsize)	// I - Destination size
{
  size_t srclen = strlen(src);		// Length of source string


  // Copy up to dstsize - 1 bytes
  dstsize --;

  if (srclen > dstsize)
    srclen = dstsize;

  memmove(dst, src, srclen);

  dst[srclen] = '\0';

  return (srclen);
}
#endif // !HAVE_STRLCPY


//
// 'system_cb()' - System callback.
//

pappl_system_t *			// O - New system object
system_cb(int           num_options,	// I - Number of options
	  cups_option_t *options,	// I - Options
	  void          *data)		// I - Callback data
{
  pappl_system_t	*system;	// System object
  const char		*val,		// Current option value
			*hostname,	// Hostname, if any
			*logfile,	// Log file, if any
			*system_name;	// System name, if any
  pappl_loglevel_t	loglevel;	// Log level
  int			port = 0;	// Port number, if any
  pappl_soptions_t	soptions = PAPPL_SOPTIONS_MULTI_QUEUE | PAPPL_SOPTIONS_STANDARD | PAPPL_SOPTIONS_LOG | PAPPL_SOPTIONS_NETWORK | PAPPL_SOPTIONS_SECURITY | PAPPL_SOPTIONS_TLS;
					// System options
  static pappl_version_t versions[1] =	// Software versions
  {
    { "HP Printer App", "", "1.0", { 1, 0, 0, 0 } }
  };


  // Parse options...
  if ((val = cupsGetOption("log-level", num_options, options)) != NULL)
  {
    if (!strcmp(val, "fatal"))
      loglevel = PAPPL_LOGLEVEL_FATAL;
    else if (!strcmp(val, "error"))
      loglevel = PAPPL_LOGLEVEL_ERROR;
    else if (!strcmp(val, "warn"))
      loglevel = PAPPL_LOGLEVEL_WARN;
    else if (!strcmp(val, "info"))
      loglevel = PAPPL_LOGLEVEL_INFO;
    else if (!strcmp(val, "debug"))
      loglevel = PAPPL_LOGLEVEL_DEBUG;
    else
    {
      fprintf(stderr, "hp_printer_app: Bad log-level value '%s'.\n", val);
      return (NULL);
    }
  }
  else
    loglevel = PAPPL_LOGLEVEL_UNSPEC;

  logfile     = cupsGetOption("log-file", num_options, options);
  hostname    = cupsGetOption("server-hostname", num_options, options);
  system_name = cupsGetOption("system-name", num_options, options);

  if ((val = cupsGetOption("server-port", num_options, options)) != NULL)
  {
    if (!isdigit(*val & 255))
    {
      fprintf(stderr, "hp_printer_app: Bad server-port value '%s'.\n", val);
      return (NULL);
    }
    else
      port = atoi(val);
  }

  // Create the system object...
  if ((system = papplSystemCreate(soptions, system_name ? system_name : "HP Printer app", port, "_print,_universal", cupsGetOption("spool-directory", num_options, options), logfile ? logfile : "-", loglevel, cupsGetOption("auth-service", num_options, options), /* tls_only */false)) == NULL)
    return (NULL);

  papplSystemAddListeners(system, NULL);
  papplSystemSetHostname(system, hostname);
  pcl_setup(system);

  papplSystemSetFooterHTML(system,
                           "Copyright &copy; 2020 by Michael R Sweet. "
                           "Provided under the terms of the <a href=\"https://www.apache.org/licenses/LICENSE-2.0\">Apache License 2.0</a>.");
  papplSystemSetSaveCallback(system, (pappl_save_cb_t)papplSystemSaveState, (void *)"/tmp/hp_printer_app.state");
  papplSystemSetVersions(system, (int)(sizeof(versions) / sizeof(versions[0])), versions);

  if (!papplSystemLoadState(system, "/tmp/hp_printer_app.state"))
    papplSystemSetDNSSDName(system, system_name ? system_name : "HP Printer app");

  return (system);
}
