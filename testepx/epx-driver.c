//-----------------------------------------------------------------------------------------------------
// EPX driver for the Printer Application Framework
//
// Copyright © 2022 Printer Working Group
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#define EPX_DRIVER 1
#include "testepx.h"
#include <pappl/base-private.h>
#include <cups/dir.h>


//-----------------------------------------------------------------------------------------------------
// Driver types...

typedef struct pwg_s
{
    cups_raster_t	*ras;			// PWG raster file
    size_t	colorants[4];		// Color usage
} pwg_job_data_t;


//-----------------------------------------------------------------------------------------------------
// Local globals...

static const char * const pwg_common_media[] =
{					// Supported media sizes for common printer
    "na_letter_8.5x11in",
    "na_legal_8.5x14in",
    
    "iso_a4_210x297mm",
    
    "custom_max_8.5x14in",
    "custom_min_3x5in"
};


//-----------------------------------------------------------------------------------------------------
// Local functions...

static void	epx_identify(pappl_printer_t *printer, pappl_identify_actions_t actions, const char *message);
static bool	epx_print(pappl_job_t *job, pappl_pr_options_t *options, pappl_device_t *device);
static bool	epx_rendjob(pappl_job_t *job, pappl_pr_options_t *options, pappl_device_t *device);
static bool	epx_rendpage(pappl_job_t *job, pappl_pr_options_t *options, pappl_device_t *device, unsigned page);
static bool	epx_rstartjob(pappl_job_t *job, pappl_pr_options_t *options, pappl_device_t *device);
static bool	epx_rstartpage(pappl_job_t *job, pappl_pr_options_t *options, pappl_device_t *device, unsigned page);
static bool	epx_rwriteline(pappl_job_t *job, pappl_pr_options_t *options, pappl_device_t *device, unsigned y, const unsigned char *line);
static bool	epx_status(pappl_printer_t *printer);
static const char *epx_testpage(pappl_printer_t *printer, char *buffer, size_t bufsize);
static const char *epx_get_make_and_model_string(char *buffer, size_t bufsize);

//-----------------------------------------------------------------------------------------------------
// 'epx_pappl_autoadd_cb()' - Auto-add callback.
const char *				                    // O - Driver name or `NULL` for none
epx_pappl_autoadd_cb(const char *device_info,	// I - Device information string (not used)
                     const char *device_uri,	// I - Device URI (not used)
                     const char *device_id,	    // I - IEEE-1284 device ID
                     void       *data)		    // I - Callback data (not used)
{
    const char	*ret = NULL;		// Return value
    
    
    (void)device_info;
    (void)device_uri;
    (void)device_id;
    
    if (!data || strcmp((const char *)data, "testepx"))
    {
//        papplLog(system, PAPPL_LOGLEVEL_ERROR, "Driver callback called with bad data pointer.");
        fprintf(stderr, "Driver callback called with bad data pointer.");
    }
    else
    {
        ret = "epx-driver";
    }
    
    return (ret);
}


//-----------------------------------------------------------------------------------------------------
// 'epx_pappl_driver_cb()' - Driver callback for EPX.
bool					// O - `true` on success, `false` on failure
epx_pappl_driver_cb(
                    pappl_system_t         *system,	        // I - System
                    const char             *driver_name,    // I - Driver name
                    const char             *device_uri,	    // I - Device URI
                    const char             *device_id,	    // I - IEEE-1284 device ID string (not used)
                    pappl_pr_driver_data_t *driver_data,    // O - Driver data
                    ipp_t                  **driver_attrs,  // O - Driver attributes
                    void                   *data)	        // I - Callback data
{
    int i; // used in for loops later in the function
    (void)device_id; // Statement to make the compiler ignore that this isn't used
    
    if (!driver_name || !device_uri || !driver_data || !driver_attrs)
    {
        papplLog(system, PAPPL_LOGLEVEL_ERROR, "EPX Driver: Driver callback called without required information.");
        return (false);
    }
    
    if (!data || strcmp((const char *)data, "testepx"))
    {
        papplLog(system, PAPPL_LOGLEVEL_ERROR, "EPX Driver: Driver callback called with bad data pointer.");
        return (false);
    }
        
    if (strcmp(driver_name, "epx-driver"))
    {
        papplLog(system, PAPPL_LOGLEVEL_ERROR, "EPX Driver: Unsupported driver name '%s'.", driver_name);
        return (false);
    }
    
    // Callbacks
    driver_data->identify_cb        = epx_identify;
    driver_data->identify_default   = PAPPL_IDENTIFY_ACTIONS_SOUND;
    driver_data->identify_supported = PAPPL_IDENTIFY_ACTIONS_DISPLAY | PAPPL_IDENTIFY_ACTIONS_SOUND;
    driver_data->printfile_cb       = epx_print;
    driver_data->rendjob_cb         = epx_rendjob;
    driver_data->rendpage_cb        = epx_rendpage;
    driver_data->rstartjob_cb       = epx_rstartjob;
    driver_data->rstartpage_cb      = epx_rstartpage;
    driver_data->rwriteline_cb      = epx_rwriteline;
    driver_data->status_cb          = epx_status;
    driver_data->testpage_cb        = epx_testpage;

    // Printer attributes and information
    char makeAndModelString[256];
    if (NULL == epx_get_make_and_model_string(makeAndModelString, sizeof(makeAndModelString)))
        snprintf(makeAndModelString, sizeof(makeAndModelString), "UNKNOWN DON'T KNOW WHAT");
    strncpy(driver_data->make_and_model, makeAndModelString, sizeof(driver_data->make_and_model) - 1);
    
    driver_data->format             = "image/pwg-raster";
    driver_data->orient_default     = IPP_ORIENT_NONE;
    driver_data->quality_default    = IPP_QUALITY_NORMAL;
    driver_data->x_resolution[driver_data->num_resolution   ] = 300;
    driver_data->y_resolution[driver_data->num_resolution ++] = 300;
    driver_data->x_default = driver_data->y_default           = 300;
    /* Four color spaces - black (1-bit and 8-bit), grayscale, and sRGB */
    driver_data->raster_types = PAPPL_PWG_RASTER_TYPE_BLACK_1 | PAPPL_PWG_RASTER_TYPE_BLACK_8 | PAPPL_PWG_RASTER_TYPE_SGRAY_8 | PAPPL_PWG_RASTER_TYPE_SRGB_8;

    /* Color modes: auto (default), monochrome, and color */
    driver_data->color_supported = PAPPL_COLOR_MODE_AUTO | PAPPL_COLOR_MODE_AUTO_MONOCHROME | PAPPL_COLOR_MODE_COLOR | PAPPL_COLOR_MODE_MONOCHROME;
    driver_data->color_default   = PAPPL_COLOR_MODE_AUTO;
    
    driver_data->has_supplies = true;
    driver_data->kind         = PAPPL_KIND_DOCUMENT;
    driver_data->ppm          = 15;    // 5 mono pages per minute
    driver_data->ppm_color    = 12;    // 2 color pages per minute
    driver_data->left_right   = 423;    // 1/6" left and right
    driver_data->bottom_top   = 423;    // 1/6" top and bottom
    driver_data->borderless   = false;    // Also borderless sizes

    driver_data->finishings = PAPPL_FINISHINGS_NONE;

    // Media (media and media-col as well as sources and types)
    /* Three paper trays (MSN names) */
    driver_data->num_source = 3;
    driver_data->source[0]  = "tray-1";
    driver_data->source[1]  = "manual";
    driver_data->source[2]  = "envelope";
//    driver_data->num_source = 4;
//    driver_data->source[0]  = "main";
//    driver_data->source[1]  = "alternate";
//    driver_data->source[2]  = "manual";
//    driver_data->source[3]  = "by-pass-tray";

    /* Five media types (MSN names) */
    driver_data->num_type = 5;
    driver_data->type[0] = "stationery";
    driver_data->type[1] = "bond";
    driver_data->type[2] = "special";
    driver_data->type[3] = "transparency";
    driver_data->type[4] = "photographic-glossy";
    


    driver_data->num_media = (int)(sizeof(pwg_common_media) / sizeof(pwg_common_media[0]));
    memcpy((void *)driver_data->media, pwg_common_media, sizeof(pwg_common_media));
    
    // Fill out ready and default media (default == ready media from the first source)
    // NOTE: sources and types must be defined BEFORE this loop is run
    for (i = 0; i < driver_data->num_source; i ++)
    {
        pwg_media_t *pwg;                   /* Media size information */

        /* Use US Letter for regular trays, #10 envelope for the envelope tray */
        if (!strcmp(driver_data->source[i], "envelope"))
            strncpy(driver_data->media_ready[i].size_name, "env_10_4.125x9.5in", sizeof(driver_data->media_ready[i].size_name) - 1);
        else
            strncpy(driver_data->media_ready[i].size_name, "na_letter_8.5x11in", sizeof(driver_data->media_ready[i].size_name) - 1);
        
        /* Set margin and size information */
        if ((pwg = pwgMediaForPWG(driver_data->media_ready[i].size_name)) != NULL)
        {
            driver_data->media_ready[i].bottom_margin = driver_data->bottom_top;
            driver_data->media_ready[i].left_margin   = driver_data->left_right;
            driver_data->media_ready[i].right_margin  = driver_data->left_right;
            driver_data->media_ready[i].size_width    = pwg->width;
            driver_data->media_ready[i].size_length   = pwg->length;
            driver_data->media_ready[i].top_margin    = driver_data->bottom_top;
            strncpy(driver_data->media_ready[i].source, driver_data->source[i], sizeof(driver_data->media_ready[i].source) - 1);
            strncpy(driver_data->media_ready[i].type, driver_data->type[0],  sizeof(driver_data->media_ready[i].type) - 1);
        }
    }
    driver_data->media_default = driver_data->media_ready[0];


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

    papplCopyString(driver_data->media_ready[0].size_name, "na_letter_8.5x11in", sizeof(driver_data->media_ready[0].size_name));
    papplCopyString(driver_data->media_ready[1].size_name, "iso_a4_210x297mm", sizeof(driver_data->media_ready[1].size_name));

    driver_data->sides_supported = PAPPL_SIDES_ONE_SIDED | PAPPL_SIDES_TWO_SIDED_LONG_EDGE | PAPPL_SIDES_TWO_SIDED_SHORT_EDGE;
    driver_data->sides_default   = PAPPL_SIDES_TWO_SIDED_LONG_EDGE;
    
    // Enable all new non-deprecated EPX features
    driver_data->features[driver_data->num_features++] = "job-release";
    driver_data->features[driver_data->num_features++] = "job-storage";
    driver_data->features[driver_data->num_features++] = "print-policy";
    driver_data->features[driver_data->num_features++] = "proof-and-suspend";
    // driver_data->features[driver_data->num_features++] = "proof-print";

    papplLog(system, PAPPL_LOGLEVEL_INFO, "EPX Driver: epx_pappl_driver_cb() - completed successfully");
    return (true);
}


//-----------------------------------------------------------------------------------------------------
// 'epx_identify()' - Identify the printer.
static void
epx_identify(
             pappl_printer_t          *printer,	// I - Printer
             pappl_identify_actions_t actions,	// I - Actions to take
             const char               *message)	// I - Message, if any
{
    (void)printer;
    (void)actions;
    
    papplLogPrinter(printer, PAPPL_LOGLEVEL_INFO, "EPX Driver: Identify Printer for Printer '%s'", papplPrinterGetName(printer));

    // TODO: Open console and send BEL character and message to it instead...
    putchar(7);
    if (message)
        puts(message);
    
    fflush(stdout);
}


//-----------------------------------------------------------------------------------------------------
// 'epx_print()' - Print a file.
static bool				                // O - `true` on success, `false` on failure
epx_print(
          pappl_job_t        *job,		// I - Job
          pappl_pr_options_t *options,	// I - Job options (unused)
          pappl_device_t     *device)   // I - Print device (unused)
{
    int		    fd;                 // Input file
    ssize_t	    bytes;              // Bytes read/written
    char		buffer[65536];      // Read/write buffer
    
    
    (void)options;
    papplLogJob(job, PAPPL_LOGLEVEL_INFO, "EPX Driver: Printing Job '%s': %s", papplJobGetName(job), papplJobGetFilename(job));

    papplJobSetImpressions(job, 1);
    
    if ((fd  = open(papplJobGetFilename(job), O_RDONLY)) < 0)
    {
        papplLogJob(job, PAPPL_LOGLEVEL_ERROR, "EPX Driver: Unable to open print file '%s': %s", papplJobGetFilename(job), strerror(errno));
        return (false);
    }
    
    while ((bytes = read(fd, buffer, sizeof(buffer))) > 0)
        papplDeviceWrite(device, buffer, (size_t)bytes);
    
    close(fd);
    
    papplJobSetImpressionsCompleted(job, 1);
    
    return (true);
}


//-----------------------------------------------------------------------------------------------------
// 'epx_rendjob()' - End a job.
static bool				// O - `true` on success, `false` on failure
epx_rendjob(
            pappl_job_t        *job,		// I - Job
            pappl_pr_options_t *options,	// I - Job options (unused)
            pappl_device_t     *device)		// I - Print device (unused)
{
    pwg_job_data_t	*pwg = (pwg_job_data_t *)papplJobGetData(job);
    // Job data
    
    (void)options;
    (void)device;

    papplLogJob(job, PAPPL_LOGLEVEL_INFO, "EPX Driver: Ending Job '%s': %s", papplJobGetName(job), papplJobGetFilename(job));

    cupsRasterClose(pwg->ras);
    
    free(pwg);
    papplJobSetData(job, NULL);
    
    return (true);
}


//-----------------------------------------------------------------------------------------------------
// 'epx_rendpage()' - End a page.
static bool				// O - `true` on success, `false` on failure
epx_rendpage(
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
    
    papplLogJob(job, PAPPL_LOGLEVEL_INFO, "EPX Driver: Ending page for Job '%s': page %d", papplJobGetName(job), page);

    
    if (papplPrinterGetSupplies(printer, 5, supplies) == 5)
    {
        // Calculate ink usage from coverage - figure 100 pages at 10% for black,
        // 50 pages at 10% for CMY, and 200 pages at 10% for the waste tank...
        int i;				// Looping var
        int	c, m, y, k, w;			// Ink usage
        pappl_preason_t reasons = PAPPL_PREASON_NONE;
        // "printer-state-reasons" values
        
        papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "EPX Driver: Calculating ink usage (%u,%u,%u,%u)", (unsigned)pwg->colorants[0], (unsigned)pwg->colorants[1], (unsigned)pwg->colorants[2], (unsigned)pwg->colorants[3]);
        
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


//-----------------------------------------------------------------------------------------------------
// 'epx_rstartjob()' - Start a job.
static bool				// O - `true` on success, `false` on failure
epx_rstartjob(
              pappl_job_t        *job,		// I - Job
              pappl_pr_options_t *options,	// I - Job options
              pappl_device_t     *device)		// I - Print device (unused)
{
    pwg_job_data_t *pwg = (pwg_job_data_t *)calloc(1, sizeof(pwg_job_data_t));
    // PWG driver data
    
    
    (void)options;
    
    papplLogJob(job, PAPPL_LOGLEVEL_INFO, "EPX Driver: Starting Job '%s': %s", papplJobGetName(job), papplJobGetFilename(job));

    
    papplJobSetData(job, pwg);
    
    pwg->ras = cupsRasterOpenIO((cups_raster_cb_t)papplDeviceWrite, device, CUPS_RASTER_WRITE_PWG);
    
    return (1);
}


//-----------------------------------------------------------------------------------------------------
// 'epx_rstartpage()' - Start a page.
static bool				// O - `true` on success, `false` on failure
epx_rstartpage(
               pappl_job_t        *job,		// I - Job
               pappl_pr_options_t *options,	// I - Job options
               pappl_device_t     *device,		// I - Print device (unused)
               unsigned           page)		// I - Page number
{
    pwg_job_data_t	*pwg = (pwg_job_data_t *)papplJobGetData(job);
    // PWG driver data
    
    (void)device;
    (void)page;
 
    papplLogJob(job, PAPPL_LOGLEVEL_INFO, "EPX Driver: Starting page for Job '%s': page %d", papplJobGetName(job), page);

    memset(pwg->colorants, 0, sizeof(pwg->colorants));
    
    return (cupsRasterWriteHeader(pwg->ras, &options->header) != 0);
}


//-----------------------------------------------------------------------------------------------------
// 'epx_rwriteline()' - Write a raster line.
static bool				// O - `true` on success, `false` on failure
epx_rwriteline(
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
    
    papplLogJob(job, PAPPL_LOGLEVEL_INFO, "Writing line for Job '%s': line number %d", papplJobGetName(job), y);

    
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


//-----------------------------------------------------------------------------------------------------
// 'epx_status()' - Get current printer status.
static bool				// O - `true` on success, `false` on failure
epx_status(
           pappl_printer_t *printer)		// I - Printer
{
    
    papplLogPrinter(printer, PAPPL_LOGLEVEL_INFO, "EPX Driver: Status for Printer '%s'", papplPrinterGetName(printer));

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


//-----------------------------------------------------------------------------------------------------
// 'epx_testpage()' - Return a test page file to print
static const char *			                // O - Filename or `NULL`
epx_testpage(
             pappl_printer_t *printer,		// I - Printer
             char            *buffer,		// I - File Buffer
             size_t          bufsize)		// I - Buffer Size
{
    const char		*testfile;	// Test File
    pappl_pr_driver_data_t data;		// Driver data
    
    papplLogPrinter(printer, PAPPL_LOGLEVEL_INFO, "EPX Driver: Test page for Printer '%s'", papplPrinterGetName(printer));

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

//-----------------------------------------------------------------------------------------------------
// epx_get_make_and_model_string() - Return a "printer-make-and-model" string from the 1284 DeviceID
//
// TODO: Move this into pappl
static const char *                                         // Make and Model string
epx_get_make_and_model_string(char            *buffer,      // I - Buffer
                              size_t          bufsize       // I - Buffer size
                              )
{
    int kvpCount;
    cups_option_t *deviceIdKVPs;
    const char *mfg, *mdl;
    
    memset(buffer, 0, bufsize);  // Zero out the provided buffer
    kvpCount = papplDeviceParseID(epx_drivers[0].device_id, &deviceIdKVPs);
    mfg = cupsGetOption("MFG", (size_t)kvpCount, deviceIdKVPs);
    mdl = cupsGetOption("MDL", (size_t)kvpCount, deviceIdKVPs);
    
    if (NULL != mfg && NULL != mdl)
        snprintf(buffer, bufsize, "%s %s", mfg, mdl);
    
    cupsFreeOptions((size_t)kvpCount, deviceIdKVPs); // Need to manually free what was returned by papplDeviceParseID()

    return buffer;
}
