//
// Printer driver functions for the Printer Application Framework
//
// Copyright © 2020-2024 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#include "printer-private.h"
#include "system-private.h"


//
// Local functions...
//

static ipp_t	*make_attrs(pappl_system_t *system, pappl_printer_t *printer, pappl_pr_driver_data_t *data);
static bool	validate_defaults(pappl_printer_t *printer, pappl_pr_driver_data_t *driver_data, pappl_pr_driver_data_t *data);
static bool	validate_driver(pappl_printer_t *printer, pappl_pr_driver_data_t *data);
static bool	validate_ready(pappl_printer_t *printer, pappl_pr_driver_data_t *driver_data, int num_ready, pappl_media_col_t *ready);


//
// 'papplPrinterGetDriverAttributes()' - Get a copy of the current driver
//                                       attributes.
//
// This function returns a copy the current driver attributes. Use the
// `ippDelete` function to free the memory used for the attributes when you
// are done.
//

ipp_t *					// O - Copy of driver attributes
papplPrinterGetDriverAttributes(
    pappl_printer_t *printer)		// I - Printer
{
  ipp_t	*attrs;				// Copy of driver attributes


  if (!printer)
    return (NULL);

  _papplRWLockRead(printer);

  attrs = ippNew();
  ippCopyAttributes(attrs, printer->driver_attrs, 1, NULL, NULL);

  _papplRWUnlock(printer);

  return (attrs);
}


//
// 'papplPrinterGetDriverData()' - Get the current print driver data.
//
// This function copies the current print driver data, defaults, and ready
// (loaded) media information into the specified buffer.
//

pappl_pr_driver_data_t *		// O - Driver data or `NULL` if none
papplPrinterGetDriverData(
    pappl_printer_t        *printer,	// I - Printer
    pappl_pr_driver_data_t *data)	// I - Pointer to driver data structure to fill
{
  if (!printer || !printer->driver_name || !data)
  {
    if (data)
      _papplPrinterInitDriverData(data);

    return (NULL);
  }

  memcpy(data, &printer->driver_data, sizeof(pappl_pr_driver_data_t));

  return (data);
}


//
// 'papplPrinterGetDriverName()' - Get the driver name for a printer.
//
// This function returns the driver name for the printer.
//

const char *				// O - Driver name or `NULL` for none
papplPrinterGetDriverName(
    pappl_printer_t *printer)		// I - Printer
{
  return (printer ? printer->driver_name : NULL);
}


//
// '_papplPrinterInitDriverData()' - Initialize a print driver data structure.
//

void
_papplPrinterInitDriverData(
    pappl_pr_driver_data_t *d)		// I - Driver data
{
  static const pappl_dither_t clustered =
  {					// Clustered-Dot Dither Matrix
    {  96,  40,  48, 104, 140, 188, 196, 148,  97,  41,  49, 105, 141, 189, 197, 149 },
    {  32,   0,   8,  56, 180, 236, 244, 204,  33,   1,   9,  57, 181, 237, 245, 205 },
    {  88,  24,  16,  64, 172, 228, 252, 212,  89,  25,  17,  65, 173, 229, 253, 213 },
    { 120,  80,  72, 112, 132, 164, 220, 156, 121,  81,  73, 113, 133, 165, 221, 157 },
    { 136, 184, 192, 144, 100,  44,  52, 108, 137, 185, 193, 145, 101,  45,  53, 109 },
    { 176, 232, 240, 200,  36,   4,  12,  60, 177, 233, 241, 201,  37,   5,  13,  61 },
    { 168, 224, 248, 208,  92,  28,  20,  68, 169, 225, 249, 209,  93,  29,  21,  69 },
    { 128, 160, 216, 152, 124,  84,  76, 116, 129, 161, 217, 153, 125,  85,  77, 117 },
    {  98,  42,  50, 106, 142, 190, 198, 150,  99,  43,  51, 107, 143, 191, 199, 151 },
    {  34,   2,  10,  58, 182, 238, 246, 206,  35,   3,  11,  59, 183, 239, 247, 207 },
    {  90,  26,  18,  66, 174, 230, 254, 214,  91,  27,  19,  67, 175, 231, 254, 215 },
    { 122,  82,  74, 114, 134, 166, 222, 158, 123,  83,  75, 115, 135, 167, 223, 159 },
    { 138, 186, 194, 146, 102,  46,  54, 110, 139, 187, 195, 147, 103,  47,  55, 111 },
    { 178, 234, 242, 202,  38,   6,  14,  62, 179, 235, 243, 203,  39,   7,  15,  63 },
    { 170, 226, 250, 210,  94,  30,  22,  70, 171, 227, 251, 211,  95,  31,  23,  71 },
    { 130, 162, 218, 154, 126,  86,  78, 118, 131, 163, 219, 155, 127,  87,  79, 119 }
  };
  static const pappl_dither_t blue =	// Blue-noise dither array
  {
    { 111,  49, 142, 162, 113, 195,  71, 177, 201,  50, 151,  94,  66,  37,  85, 252 },
    {  25,  99, 239, 222,  32, 250, 148,  19,  38, 106, 220, 170, 194, 138,  13, 167 },
    { 125, 178,  79,  15,  65, 173, 123,  87, 213, 131, 247,  23, 116,  54, 229, 212 },
    {  41, 202, 152, 132, 189, 104,  53, 236, 161,  62,   1, 181,  77, 241, 147,  68 },
    {   2, 244,  56,  91, 230,   5, 204,  28, 187, 101, 144, 206,  33,  92, 190, 107 },
    { 223, 164, 114,  36, 214, 156, 139,  70, 245,  84, 226,  48, 126, 158,  17, 135 },
    {  83, 196,  21, 254,  76,  45, 179, 115,  12,  40, 169, 105, 253, 176, 211,  59 },
    { 100, 180, 145, 122, 172,  97, 235, 129, 215, 149, 199,   8,  72,  26, 238,  44 },
    { 232,  31,  69,  11, 205,  58,  18, 193,  88,  60, 112, 221, 140,  86, 120, 153 },
    { 208, 130, 243, 160, 224, 110,  34, 248, 165,  24, 234, 184,  52, 198, 171,   6 },
    { 108, 188,  51,  89, 137, 186, 154,  78,  47, 134,  98, 157,  35, 249,  95,  63 },
    {  16,  75, 219,  39,   0,  67, 228, 121, 197, 240,   3,  74, 127,  20, 227, 143 },
    { 246, 175, 119, 200, 251, 103, 146,  14, 209, 174, 109, 218, 192,  82, 203, 163 },
    {  29,  93, 150,  22, 166, 182,  55,  30,  90,  64,  42, 141, 168,  57, 117,  46 },
    { 216, 233,  61, 128,  81, 237, 217, 118, 159, 255, 185,  27, 242, 102,   4, 133 },
    {  73, 191,   9, 210,  43,  96,   7, 136, 231,  80,  10, 124, 225, 207, 155, 183 }
  };


  memset(d, 0, sizeof(pappl_pr_driver_data_t));
  memcpy(d->gdither, clustered, sizeof(d->gdither));
  memcpy(d->pdither, blue, sizeof(d->pdither));

  d->orient_default      = IPP_ORIENT_NONE;
  d->content_default     = PAPPL_CONTENT_AUTO;
  d->darkness_configured = 50;
  d->quality_default     = IPP_QUALITY_NORMAL;
  d->scaling_default     = PAPPL_SCALING_AUTO;
  d->sides_supported     = PAPPL_SIDES_ONE_SIDED;
  d->sides_default       = PAPPL_SIDES_ONE_SIDED;
}


//
// 'papplPrinterSetDriverData()' - Set the driver data.
//
// This function validates and sets the driver data, including all defaults and
// ready (loaded) media.
//
// > Note: This function regenerates all of the driver-specific capability
// > attributes like "media-col-database", "sides-supported", and so forth.
// > Use the @link papplPrinterSetDriverDefaults@ or
// > @link papplPrinterSetReadyMedia@ functions to efficiently change the
// > "xxx-default" or "xxx-ready" values, respectively.
//

bool					// O - `true` on success, `false` on failure
papplPrinterSetDriverData(
    pappl_printer_t        *printer,	// I - Printer
    pappl_pr_driver_data_t *data,	// I - Driver data
    ipp_t                  *attrs)	// I - Additional capability attributes or `NULL` for none
{
  if (!printer || !data)
    return (false);

  // Validate data...
  if (!validate_defaults(printer, data, data) || !validate_driver(printer, data) || !validate_ready(printer, data, data->num_source, data->media_ready))
    return (false);

  _papplRWLockWrite(printer);

  // Copy driver data to printer
  memcpy(&printer->driver_data, data, sizeof(printer->driver_data));
  printer->num_ready = data->num_source;

  // Create printer (capability) attributes based on driver data...
  ippDelete(printer->driver_attrs);
  printer->driver_attrs = make_attrs(printer->system, printer, &printer->driver_data);

  if (attrs)
    ippCopyAttributes(printer->driver_attrs, attrs, 0, NULL, NULL);

  _papplRWUnlock(printer);

  return (true);
}


//
// 'papplPrinterSetDriverDefaults()' - Set the default print option values.
//
// This function validates and sets the printer's default print options.
//
// > Note: Unlike @link papplPrinterSetPrintDriverData@, this function only
// > changes the "xxx_default" members of the driver data and is considered
// > lightweight.
//

bool					// O - `true` on success or `false` on failure
papplPrinterSetDriverDefaults(
    pappl_printer_t        *printer,	// I - Printer
    pappl_pr_driver_data_t *data,	// I - Driver data
    int                    num_vendor,	// I - Number of vendor options
    cups_option_t          *vendor)	// I - Vendor options
{
  int			i;		// Looping var
  const char		*value;		// Vendor value
  int			intvalue;	// Integer value
  char			*end,		// End of value
 			defname[128],	// xxx-default name
			supname[128];	// xxx-supported name
  ipp_attribute_t	*supported;	// xxx-supported attribute


  if (!printer || !data)
    return (false);

  if (!validate_defaults(printer, &printer->driver_data, data))
    return (false);

  _papplRWLockWrite(printer);

  // Copy xxx_default values...
  printer->driver_data.bin_default            = data->bin_default;
  printer->driver_data.color_default          = data->color_default;
  printer->driver_data.content_default        = data->content_default;
  printer->driver_data.darkness_configured    = data->darkness_configured;
  printer->driver_data.darkness_default       = data->darkness_default;
  printer->driver_data.identify_default       = data->identify_default;
  printer->driver_data.media_default          = data->media_default;
  printer->driver_data.mode_configured        = data->mode_configured;
  printer->driver_data.orient_default         = data->orient_default;
  printer->driver_data.quality_default        = data->quality_default;
  printer->driver_data.scaling_default        = data->scaling_default;
  printer->driver_data.sides_default          = data->sides_default;
  printer->driver_data.speed_default          = data->speed_default;
  printer->driver_data.tear_offset_configured = data->tear_offset_configured;
  printer->driver_data.x_default              = data->x_default;
  printer->driver_data.y_default              = data->y_default;

  // Copy any vendor-specific xxx-default values...
  for (i = 0; i < data->num_vendor; i ++)
  {
    if ((value = cupsGetOption(data->vendor[i], (cups_len_t)num_vendor, vendor)) == NULL)
      continue;

    snprintf(defname, sizeof(defname), "%s-default", data->vendor[i]);
    snprintf(supname, sizeof(supname), "%s-supported", data->vendor[i]);

    ippDeleteAttribute(printer->driver_attrs, ippFindAttribute(printer->driver_attrs, defname, IPP_TAG_ZERO));

    if ((supported = ippFindAttribute(printer->driver_attrs, supname, IPP_TAG_ZERO)) != NULL)
    {
      switch (ippGetValueTag(supported))
      {
        case IPP_TAG_INTEGER :
        case IPP_TAG_RANGE :
            intvalue = (int)strtol(value, &end, 10);
            if (errno != ERANGE && !*end)
              ippAddInteger(printer->driver_attrs, IPP_TAG_PRINTER, IPP_TAG_INTEGER, defname, intvalue);
            break;

        case IPP_TAG_BOOLEAN :
            ippAddBoolean(printer->driver_attrs, IPP_TAG_PRINTER, defname, !strcmp(value, "true") || !strcmp(value, "on"));
            break;

	case IPP_TAG_KEYWORD :
	    ippAddString(printer->driver_attrs, IPP_TAG_PRINTER, IPP_TAG_KEYWORD, defname, NULL, value);
	    break;

        default :
            papplLogPrinter(printer, PAPPL_LOGLEVEL_ERROR, "Driver '%s' attribute syntax not supported, only boolean, integer, keyword, and rangeOfInteger are supported.", supname);
            break;
      }
    }
    else
    {
      // Default to simple text values...
      ippAddString(printer->driver_attrs, IPP_TAG_PRINTER, IPP_TAG_TEXT, defname, NULL, value);
    }
  }

  printer->config_time = time(NULL);

  _papplRWUnlock(printer);

  _papplSystemConfigChanged(printer->system);

  return (true);
}


//
// 'papplPrinterSetReadyMedia()' - Set the ready (loaded) media.
//
// This function validates and sets the printer's ready (loaded) media.
//

bool					// O - `true` on success or `false` on failure
papplPrinterSetReadyMedia(
    pappl_printer_t   *printer,		// I - Printer
    int               num_ready,	// I - Number of ready media
    pappl_media_col_t *ready)		// I - Array of ready media
{
  int	i;				// Looping vars


  if (!printer || num_ready <= 0 || !ready)
    return (false);

  if (!validate_ready(printer, &printer->driver_data, num_ready, ready))
    return (false);

  _papplRWLockWrite(printer);

  // Copy new ready media to printer data...
  if (num_ready > PAPPL_MAX_SOURCE)
    num_ready = PAPPL_MAX_SOURCE;

  memset(printer->driver_data.media_ready, 0, sizeof(printer->driver_data.media_ready));
  memcpy(printer->driver_data.media_ready, ready, (size_t)num_ready * sizeof(pappl_media_col_t));
  printer->num_ready = num_ready;

  // Update default media from ready media...
  for (i = 0; i < num_ready; i ++)
  {
    if (!strcmp(ready[i].source, printer->driver_data.media_default.source))
    {
      printer->driver_data.media_default = ready[i];
      break;
    }
  }

  printer->state_time = time(NULL);

  _papplRWUnlock(printer);

  _papplSystemConfigChanged(printer->system);

  return (true);
}


//
// 'make_attrs()' - Make the capability attributes for the given driver data.
//

static ipp_t *				// O - Driver attributes
make_attrs(
    pappl_system_t         *system,	// I - System
    pappl_printer_t        *printer,	// I - Printer
    pappl_pr_driver_data_t *data)	// I - Driver data
{
  ipp_t			*attrs;		// Driver attributes
  bool			jpeg_supported,	// Is JPEG supported?
			pdf_supported;	// Is PDF supported?
  unsigned		bit;		// Current bit value
  cups_len_t		i, j,		// Looping vars
			num_values;	// Number of values
  const char		*svalues[PAPPL_MAX_MEDIA];
					// String values
  int			ivalues[PAPPL_MAX_MEDIA];
					// Integer values
  ipp_t			*cvalues[PAPPL_MAX_MEDIA * 2];
					// Collection values
  char			vvalues[PAPPL_MAX_VENDOR][128];
					// Vendor xxx-default values
  char			fn[32],		// FN (finishings) values
			*ptr;		// Pointer into value
  const char		*preferred;	// "document-format-preferred" value
  const char		*prefix;	// Prefix string
  const char		*max_name = NULL,// Maximum size
		    	*min_name = NULL;// Minimum size
  char			output_tray[256];// "printer-output-tray" value
  _pappl_mime_filter_t	*filter;	// Current filter
  ipp_attribute_t	*attr;		// Attribute
  static const char * const job_creation_attributes[] =
  {					// job-creation-attributes-supported values
    "copies",
    "document-format",
    "document-name",
    "ipp-attribute-fidelity",
    "job-hold-until",
    "job-hold-until-time",
    "job-name",
    "job-priority",
    "job-retain-until",
    "job-retain-until-interval",
    "job-retain-until-time",
    "media",
    "media-col",
    "multiple-document-handling",
    "orientation-requested",
    "print-color-mode",
    "print-content-optimize",
    "print-quality",
    "printer-resolution"
  };
  static const char * const media_col[] =
  {					// media-col-supported values
    "media-bottom-margin",
    "media-left-margin",
    "media-right-margin",
    "media-size",
    "media-size-name",
    "media-top-margin"
  };
  static const char * const pdf_versions_supported[] =
  {					// "pdf-versions-supported" values
    "adobe-1.3",
    "adobe-1.4",
    "adobe-1.5",
    "adobe-1.6",
    "iso-32000-1_2008",			// PDF 1.7
    "iso-32000-2_2017"			// PDF 2.0
  };
  static const char * const printer_settable_attributes[] =
  {					// printer-settable-attributes values
    "copies-default",
    "media-col-default",
    "media-col-ready",
    "media-default",
    "media-ready",
    "multiple-document-handling-default",
    "orientation-requested-default",
    "print-color-mode-default",
    "print-content-optimize-default",
    "print-quality-default",
    "printer-contact-col",
    "printer-geo-location",
    "printer-location",
    "printer-organization",
    "printer-organizational-unit",
    "printer-resolution-default"
  };


  // Are JPEG and PDF supported?
  jpeg_supported = _papplSystemFindMIMEFilter(system, "image/jpeg", "image/pwg-raster") != NULL || _papplSystemFindMIMEFilter(system, "image/jpeg", data->format) != NULL;
  pdf_supported  = (data->format && !strcmp(data->format, "application/pdf")) ||   _papplSystemFindMIMEFilter(system, "application/pdf", "image/pwg-raster") != NULL || _papplSystemFindMIMEFilter(system, "application/pdf", data->format) != NULL;
  papplLogPrinter(printer, PAPPL_LOGLEVEL_DEBUG, "JPEG is %s, PDF is %s.", jpeg_supported ? "supported" : "not supported", pdf_supported ? "supported" : "not supported");

  // Create an empty IPP message for the attributes...
  attrs = ippNew();


  // color-supported
  ippAddBoolean(attrs, IPP_TAG_PRINTER, "color-supported", data->ppm_color > 0);


  // document-format-supported
  num_values = 0;
  svalues[num_values ++] = "application/octet-stream";
  svalues[num_values ++] = "image/pwg-raster";
  svalues[num_values ++] = "image/urf";

  if (data->format && strcmp(data->format, "application/octet-stream"))
    svalues[num_values ++] = data->format;

  for (preferred = "image/urf", filter = (_pappl_mime_filter_t *)cupsArrayGetFirst(system->filters); filter; filter = (_pappl_mime_filter_t *)cupsArrayGetNext(system->filters))
  {
    if ((data->format && !strcmp(filter->dst, data->format)) || !strcmp(filter->dst, "image/pwg-raster"))
    {
      for (i = 0; i < num_values; i ++)
      {
        if (!strcmp(filter->src, svalues[i]))
          break;
      }

      if (i >= num_values && num_values < (int)(sizeof(svalues) / sizeof(svalues[0])))
      {
        svalues[num_values ++] = filter->src;

        if (!strcmp(filter->src, "application/pdf"))
          preferred = "application/pdf";
      }
    }
  }

  ippAddString(attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_MIMETYPE), "document-format-preferred", NULL, preferred);

  ippAddStrings(attrs, IPP_TAG_PRINTER, IPP_TAG_MIMETYPE, "document-format-supported", num_values, NULL, svalues);


  // Assemble finishings-xxx values...
  num_values = 0;
  cvalues[num_values   ] = ippNew();
  ippAddString(cvalues[num_values], IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "finishing-template", NULL, "none");
  ivalues[num_values   ] = IPP_FINISHINGS_NONE;
  svalues[num_values ++] = "none";

  papplCopyString(fn, "FN3", sizeof(fn));
  for (ptr = fn + 3, i = 0, bit = PAPPL_FINISHINGS_PUNCH; bit <= PAPPL_FINISHINGS_TRIM; i ++, bit *= 2)
  {
    if (data->finishings & bit)
    {
      cvalues[num_values   ] = ippNew();
      ippAddString(cvalues[num_values], IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "finishing-template", NULL, _papplFinishingsString(bit));
      ivalues[num_values   ] = (int)_papplFinishingsEnum(bit);
      svalues[num_values ++] = _papplFinishingsString(bit);

      snprintf(ptr, sizeof(fn) - (size_t)(ptr - fn), "-%d", (int)_papplFinishingsEnum(bit));
      ptr += strlen(ptr);
    }
  }
  *ptr = '\0';

  // finishing-template-supported
  ippAddStrings(attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "finishing-template-supported", num_values, NULL, svalues);

  // finishing-col-database
  ippAddCollections(attrs, IPP_TAG_PRINTER, "finishing-col-database", num_values, (const ipp_t **)cvalues);

  // finishing-col-default
  ippAddCollection(attrs, IPP_TAG_PRINTER, "finishing-col-default", cvalues[0]);

  // finishing-col-supported
  ippAddString(attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "finishing-col-supported", NULL, "finishing-template");

  // finishings-default
  ippAddInteger(attrs, IPP_TAG_PRINTER, IPP_TAG_ENUM, "finishings-default", IPP_FINISHINGS_NONE);

  // finishings-supported
  ippAddIntegers(attrs, IPP_TAG_PRINTER, IPP_TAG_ENUM, "finishings-supported", num_values, ivalues);

  for (i = 0; i < num_values; i ++)
    ippDelete(cvalues[i]);


  // identify-actions-supported
  for (num_values = 0, bit = PAPPL_IDENTIFY_ACTIONS_DISPLAY; bit <= PAPPL_IDENTIFY_ACTIONS_SPEAK; bit *= 2)
  {
    if (data->identify_supported & bit)
      svalues[num_values ++] = _papplIdentifyActionsString(bit);
  }

  if (num_values > 0)
    ippAddStrings(attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "identify-actions-supported", num_values, NULL, svalues);


  // ipp-features-supported
  if ((num_values = (cups_len_t)data->num_features) > PAPPL_MAX_VENDOR)
    num_values = PAPPL_MAX_VENDOR;

  if (num_values > 0)
    memcpy((void *)svalues, data->features, (size_t)num_values * sizeof(char *));

  svalues[num_values ++] = "ipp-everywhere";

  ippAddStrings(attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "ipp-features-supported", num_values, NULL, svalues);


  // job-creation-attributes-supported
  memcpy((void *)svalues, job_creation_attributes, sizeof(job_creation_attributes));
  num_values = (int)(sizeof(job_creation_attributes) / sizeof(job_creation_attributes[0]));

  if (data->finishings)
  {
    svalues[num_values ++] = "finishings";
    svalues[num_values ++] = "finishings-col";
  }

  if (data->num_bin)
    svalues[num_values ++] = "output-bin";

  if (pdf_supported)
    svalues[num_values ++] = "page-ranges";

  if (data->darkness_supported)
    svalues[num_values ++] = "print-darkness";

  if (data->speed_supported[1])
    svalues[num_values ++] = "print-speed";

  if (data->sides_supported != PAPPL_SIDES_ONE_SIDED)
    svalues[num_values ++] = "sides";

  for (i = 0; i < (cups_len_t)data->num_vendor && i < (cups_len_t)(sizeof(data->vendor) / sizeof(data->vendor[0])); i ++)
    svalues[num_values ++] = data->vendor[i];

  ippAddStrings(attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "job-creation-attributes-supported", num_values, NULL, svalues);


  if (jpeg_supported)
  {
    // jpeg-features-supported
    static const char * const jpeg_features_supported[] =
    {					// "jpeg-features-supported" values
      "arithmetic",
      "cmyk",
      "progressive"
    };

    ippAddStrings(attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "jpeg-features-supported", (int)(sizeof(jpeg_features_supported) / sizeof(jpeg_features_supported[0])), NULL, jpeg_features_supported);


    // jpeg-k-octets-supported
    int upper = 0, lower = ippGetRange(ippFindAttribute(printer->attrs, "job-k-octets-supported", IPP_TAG_RANGE), 0, &upper);
					// Range of k-octets-supported...
    ippAddRange(attrs, IPP_TAG_PRINTER, "jpeg-k-octets-supported", lower, upper);


    // jpeg-x-dimension-supported
    ippAddRange(attrs, IPP_TAG_PRINTER, "jpeg-x-dimension-supported", 0, system->max_image_width);


    // jpeg-y-dimension-supported
    ippAddRange(attrs, IPP_TAG_PRINTER, "jpeg-y-dimension-supported", 1, system->max_image_height);
  }


  // label-mode-supported
  for (num_values = 0, bit = PAPPL_LABEL_MODE_APPLICATOR; bit <= PAPPL_LABEL_MODE_TEAR_OFF; bit *= 2)
  {
    if (data->mode_supported & bit)
      svalues[num_values ++] = _papplLabelModeString((pappl_label_mode_t)bit);
  }

  if (num_values > 0)
    ippAddStrings(attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "label-mode-supported", num_values, NULL, svalues);


  // label-tear-offset-supported
  if (data->tear_offset_supported[0] || data->tear_offset_supported[1])
    ippAddRange(attrs, IPP_TAG_PRINTER, "label-tear-offset-supported", data->tear_offset_supported[0], data->tear_offset_supported[1]);


  // landscape-orientation-requested-preferred
  ippAddInteger(attrs, IPP_TAG_PRINTER, IPP_TAG_ENUM, "landscape-orientation-requested-preferred", IPP_ORIENT_LANDSCAPE);


  // max-page-ranges-supported
  if (pdf_supported)
    ippAddInteger(attrs, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "max-page-ranges-supported", 1);

  // media-bottom-margin-supported
  num_values = 0;
  if (data->borderless)
    ivalues[num_values ++] = 0;
  ivalues[num_values ++] = data->bottom_top;

  ippAddIntegers(attrs, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "media-bottom-margin-supported", num_values, ivalues);


  // media-col-database
  for (i = 0, num_values = 0; i < (cups_len_t)data->num_media; i ++)
  {
    if (!strncmp(data->media[i], "custom_max_", 11) || !strncmp(data->media[i], "roll_max_", 9))
    {
      max_name = data->media[i];
    }
    else if (!strncmp(data->media[i], "custom_min_", 11) || !strncmp(data->media[i], "roll_min_", 9))
    {
      min_name = data->media[i];
    }
    else
    {
      pappl_media_col_t	col;		// Media collection
      pwg_media_t	*pwg;		// PWG media size info

      memset(&col, 0, sizeof(col));
      papplCopyString(col.size_name, data->media[i], sizeof(col.size_name));
      if ((pwg = pwgMediaForPWG(data->media[i])) != NULL)
      {
	col.size_width  = pwg->width;
	col.size_length = pwg->length;
      }

      if (data->borderless && data->bottom_top > 0 && data->left_right > 0)
	cvalues[num_values ++] = _papplMediaColExport(data, &col, true);

      col.bottom_margin = col.top_margin = data->bottom_top;
      col.left_margin = col.right_margin = data->left_right;

      if ((cvalues[num_values] = _papplMediaColExport(data, &col, true)) != NULL)
        num_values ++;
    }
  }

  if (min_name && max_name)
  {
    pwg_media_t	*pwg,			// Current media size info
		max_pwg,		// PWG maximum media size info
		min_pwg;		// PWG minimum media size info
    ipp_t	*col;			// media-size collection

    if ((pwg = pwgMediaForPWG(max_name)) != NULL)
      max_pwg = *pwg;
    else
      memset(&max_pwg, 0, sizeof(max_pwg));

    if ((pwg = pwgMediaForPWG(min_name)) != NULL)
      min_pwg = *pwg;
    else
      memset(&min_pwg, 0, sizeof(min_pwg));

    col = ippNew();
    ippAddRange(col, IPP_TAG_PRINTER, "x-dimension", min_pwg.width, max_pwg.width);
    ippAddRange(col, IPP_TAG_PRINTER, "y-dimension", min_pwg.length, max_pwg.length);

    cvalues[num_values] = ippNew();
    ippAddCollection(cvalues[num_values], IPP_TAG_PRINTER, "media-size", col);
    if (data->borderless && data->bottom_top > 0 && data->left_right > 0)
    {
      ippAddInteger(cvalues[num_values], IPP_TAG_PRINTER, IPP_TAG_INTEGER, "media-bottom-margin", 0);
      ippAddInteger(cvalues[num_values], IPP_TAG_PRINTER, IPP_TAG_INTEGER, "media-left-margin", 0);
      ippAddInteger(cvalues[num_values], IPP_TAG_PRINTER, IPP_TAG_INTEGER, "media-right-margin", 0);
      ippAddInteger(cvalues[num_values ++], IPP_TAG_PRINTER, IPP_TAG_INTEGER, "media-top-margin", 0);

      cvalues[num_values] = ippNew();
      ippAddCollection(cvalues[num_values], IPP_TAG_PRINTER, "media-size", col);
    }

    ippAddInteger(cvalues[num_values], IPP_TAG_PRINTER, IPP_TAG_INTEGER, "media-bottom-margin", data->bottom_top);
    ippAddInteger(cvalues[num_values], IPP_TAG_PRINTER, IPP_TAG_INTEGER, "media-left-margin", data->left_right);
    ippAddInteger(cvalues[num_values], IPP_TAG_PRINTER, IPP_TAG_INTEGER, "media-right-margin", data->left_right);
    ippAddInteger(cvalues[num_values ++], IPP_TAG_PRINTER, IPP_TAG_INTEGER, "media-top-margin", data->bottom_top);

    ippDelete(col);
  }

  if (num_values > 0)
  {
    ippAddCollections(attrs, IPP_TAG_PRINTER, "media-col-database", num_values, (const ipp_t **)cvalues);
    for (i = 0; i < num_values; i ++)
      ippDelete(cvalues[i]);
  }


  // media-col-supported
  memcpy((void *)svalues, media_col, sizeof(media_col));
  num_values = (int)(sizeof(media_col) / sizeof(media_col[0]));

  if (data->left_offset_supported[1])
    svalues[num_values ++] = "media-left-offset";

  if (data->num_source)
    svalues[num_values ++] = "media-source";

  if (data->top_offset_supported[1])
    svalues[num_values ++] = "media-top-offset";

  if (data->tracking_supported)
    svalues[num_values ++] = "media-tracking";

  if (data->num_type)
    svalues[num_values ++] = "media-type";

  ippAddStrings(attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "media-col-supported", num_values, NULL, svalues);


  // media-left-margin-supported
  num_values = 0;
  if (data->borderless)
    ivalues[num_values ++] = 0;
  ivalues[num_values ++] = data->left_right;

  ippAddIntegers(attrs, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "media-left-margin-supported", num_values, ivalues);


  // media-left-offset-supported
  if (data->left_offset_supported[1])
    ippAddRange(attrs, IPP_TAG_PRINTER, "media-left-offset-supported", data->left_offset_supported[0], data->left_offset_supported[1]);


  // media-right-margin-supported
  num_values = 0;
  if (data->borderless)
    ivalues[num_values ++] = 0;
  ivalues[num_values ++] = data->left_right;

  ippAddIntegers(attrs, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "media-right-margin-supported", num_values, ivalues);


  // media-size-supported
  for (i = 0, num_values = 0; i < (cups_len_t)data->num_media; i ++)
  {
    pwg_media_t	*pwg;			// PWG media size info

    if (!strncmp(data->media[i], "custom_max_", 11) || !strncmp(data->media[i], "roll_max_", 9))
    {
      max_name = data->media[i];
    }
    else if (!strncmp(data->media[i], "custom_min_", 11) || !strncmp(data->media[i], "roll_min_", 9))
    {
      min_name = data->media[i];
    }
    else if ((pwg = pwgMediaForPWG(data->media[i])) != NULL)
    {
      cvalues[num_values] = ippNew();
      ippAddInteger(cvalues[num_values], IPP_TAG_PRINTER, IPP_TAG_INTEGER, "x-dimension", pwg->width);
      ippAddInteger(cvalues[num_values ++], IPP_TAG_PRINTER, IPP_TAG_INTEGER, "y-dimension", pwg->length);
    }
  }

  if (min_name && max_name)
  {
    pwg_media_t	*pwg,			// Current media size info
		max_pwg,		// PWG maximum media size info
		min_pwg;		// PWG minimum media size info

    if ((pwg = pwgMediaForPWG(max_name)) != NULL)
      max_pwg = *pwg;
    else
      memset(&max_pwg, 0, sizeof(max_pwg));

    if ((pwg = pwgMediaForPWG(min_name)) != NULL)
      min_pwg = *pwg;
    else
      memset(&min_pwg, 0, sizeof(min_pwg));

    cvalues[num_values] = ippNew();
    ippAddRange(cvalues[num_values], IPP_TAG_PRINTER, "x-dimension", min_pwg.width, max_pwg.width);
    ippAddRange(cvalues[num_values ++], IPP_TAG_PRINTER, "y-dimension", min_pwg.length, max_pwg.length);
  }

  if (num_values > 0)
  {
    ippAddCollections(attrs, IPP_TAG_PRINTER, "media-size-supported", num_values, (const ipp_t **)cvalues);
    for (i = 0; i < num_values; i ++)
      ippDelete(cvalues[i]);
  }


  // media-source-supported
  if ((num_values = (cups_len_t)data->num_source) > 0)
  {
    if (num_values > PAPPL_MAX_SOURCE)
      num_values = PAPPL_MAX_SOURCE;

    memcpy((void *)svalues, data->source, (size_t)num_values * sizeof(char *));
  }

  svalues[num_values ++] = "auto";

  ippAddStrings(attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "media-source-supported", num_values, NULL, svalues);

  // media-supported
  if (data->num_media)
    ippAddStrings(attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "media-supported", (cups_len_t)data->num_media, NULL, data->media);


  // media-top-margin-supported
  num_values = 0;
  if (data->borderless)
    ivalues[num_values ++] = 0;
  ivalues[num_values ++] = data->bottom_top;

  ippAddIntegers(attrs, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "media-top-margin-supported", num_values, ivalues);


  // media-top-offset-supported
  if (data->top_offset_supported[1])
    ippAddRange(attrs, IPP_TAG_PRINTER, "media-top-offset-supported", data->top_offset_supported[0], data->top_offset_supported[1]);


  // media-tracking-supported
  if (data->tracking_supported)
  {
    for (num_values = 0, bit = PAPPL_MEDIA_TRACKING_CONTINUOUS; bit <= PAPPL_MEDIA_TRACKING_WEB; bit *= 2)
    {
      if (data->tracking_supported & bit)
        svalues[num_values ++] = _papplMediaTrackingString((pappl_media_tracking_t)bit);
    }

    ippAddStrings(attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "media-tracking-supported", num_values, NULL, svalues);
  }


  // media-type-supported
  if (data->num_type)
    ippAddStrings(attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "media-type-supported", (cups_len_t)data->num_type, NULL, data->type);


  // mopria-certified (Mopria-specific attribute)
  if (!ippFindAttribute(attrs, "mopria-certified", IPP_TAG_ZERO))
    ippAddString(attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_TEXT), "mopria-certified", NULL, "1.3");


  // output-bin-supported
  if (data->num_bin)
    ippAddStrings(attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "output-bin-supported", (cups_len_t)data->num_bin, NULL, data->bin);
  else if (data->output_face_up)
    ippAddString(attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "output-bin-supported", NULL, "face-up");
  else
    ippAddString(attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "output-bin-supported", NULL, "face-down");


  // page-ranges-supported
  if (pdf_supported)
    ippAddBoolean(attrs, IPP_TAG_PRINTER, "page-ranges-supported", 1);


  // pages-per-minute
  if (data->ppm > 0)
    ippAddInteger(attrs, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "pages-per-minute", data->ppm);
  else
    ippAddInteger(attrs, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "pages-per-minute", 1);


  // pages-per-minute-color
  if (data->ppm_color > 0)
    ippAddInteger(attrs, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "pages-per-minute-color", data->ppm_color);


  if (pdf_supported)
  {
    // pdf-k-octets-supported
    int upper = 0, lower = ippGetRange(ippFindAttribute(printer->attrs, "job-k-octets-supported", IPP_TAG_RANGE), 0, &upper);
					// Range of k-octets-supported...

    ippAddRange(attrs, IPP_TAG_PRINTER, "pdf-k-octets-supported", lower, upper);


    // pdf-versions-supported
    ippAddStrings(attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "pdf-versions-supported", (int)(sizeof(pdf_versions_supported) / sizeof(pdf_versions_supported[0])), NULL, pdf_versions_supported);
  }


  // print-color-mode-supported
  for (num_values = 0, bit = PAPPL_COLOR_MODE_AUTO; bit <= PAPPL_COLOR_MODE_PROCESS_MONOCHROME; bit *= 2)
  {
    if (bit & data->color_supported)
      svalues[num_values ++] = _papplColorModeString(bit);
  }

  if (num_values > 0)
    ippAddStrings(attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "print-color-mode-supported", num_values, NULL, svalues);


  // print-darkness-supported
  if (data->darkness_supported)
    ippAddInteger(attrs, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "print-darkness-supported", 2 * data->darkness_supported);


  // print-speed-supported
  if (data->speed_supported[1])
    ippAddRange(attrs, IPP_TAG_PRINTER, "print-speed-supported", data->speed_supported[0], data->speed_supported[1]);


  // printer-darkness-supported
  if (data->darkness_supported)
    ippAddInteger(attrs, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "printer-darkness-supported", data->darkness_supported);


  // printer-device-id
  if (printer->device_id)
  {
    ippAddString(attrs, IPP_TAG_PRINTER, IPP_TAG_TEXT, "printer-device-id", NULL, printer->device_id);
  }
  else
  {
    // Generate printer-device-id value as needed...
    char	mfg[128],		// Manufacturer name
		*mdl,			// Model name
		cmd[128];		// Command (format) list
    ipp_attribute_t *formats;		// "document-format-supported" attribute
    cups_len_t	count;			// Number of values

    // Assume make and model are separated by a space...
    papplCopyString(mfg, data->make_and_model, sizeof(mfg));
    if ((mdl = strchr(mfg, ' ')) != NULL)
      *mdl++ = '\0';			// Nul-terminate the make
    else
      mdl = mfg;			// No separator, so assume the make and model are the same

    formats = ippFindAttribute(attrs, "document-format-supported", IPP_TAG_MIMETYPE);
    count   = ippGetCount(formats);
    for (i = 0, ptr = cmd; i < count; i ++)
    {
      const char *format = ippGetString(formats, i, NULL);
					// Current MIME media type

      if (!strcmp(format, "application/pdf"))
        format = "PDF";
      else if (!strcmp(format, "application/postscript"))
        format = "PS";
      else if (!strcmp(format, "application/vnd.eltron-epl"))
        format = "EPL";
      else if (!strcmp(format, "application/vnd.hp-pcl"))
        format = "PCL";
      else if (!strcmp(format, "application/vnd.sii-slp"))
        format = "SIISLP";
      else if (!strcmp(format, "application/vnd.tsc-tspl"))
        format = "TSPL";
      else if (!strcmp(format, "application/vnd.zebra-cpcl"))
        format = "CPCL";
      else if (!strcmp(format, "application/vnd.zebra-zpl"))
        format = "ZPL";
      else if (!strcmp(format, "image/jpeg"))
        format = "JPEG";
      else if (!strcmp(format, "image/png"))
        format = "PNG";
      else if (!strcmp(format, "image/pwg-raster"))
        format = "PWGRaster";
      else if (!strcmp(format, "image/urf"))
        format = "URF";
      else if (!strcmp(format, "text/plain"))
        format = "TXT";
      else if (!strcmp(format, "application/octet-stream"))
        continue;

      if (ptr > cmd)
        snprintf(ptr, sizeof(cmd) - (size_t)(ptr - cmd), ",%s", format);
      else
        papplCopyString(cmd, format, sizeof(cmd));

      ptr += strlen(ptr);
    }

    *ptr = '\0';

    ippAddStringf(attrs, IPP_TAG_PRINTER, IPP_TAG_TEXT, "printer-device-id", NULL, "MFG:%s;MDL:%s;CMD:%s;", mfg, mdl, cmd);
  }


  // printer-kind
  for (num_values = 0, bit = PAPPL_KIND_DISC; bit <= PAPPL_KIND_ROLL; bit *= 2)
  {
    if (bit & data->kind)
      svalues[num_values ++] = _papplKindString(bit);
  }

  if (num_values > 0)
    ippAddStrings(attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "printer-kind", num_values, NULL, svalues);


  // printer-make-and-model
  ippAddString(attrs, IPP_TAG_PRINTER, IPP_TAG_TEXT, "printer-make-and-model", NULL, data->make_and_model);


  // printer-output-tray
  if (data->num_bin > 0)
  {
    for (i = 0, attr = NULL; i < (cups_len_t)data->num_bin; i ++)
    {
      snprintf(output_tray, sizeof(output_tray), "type=unRemovableBin;maxcapacity=-2;remaining=-2;status=0;name=%s;%s", data->bin[i], data->output_face_up ? "stackingorder=lastToFirst;pagedelivery=faceUp;" : "stackingorder=firstToLast;pagedelivery=faceDown;");
      if (attr)
        ippSetOctetString(attrs, &attr, ippGetCount(attr), output_tray, (cups_len_t)strlen(output_tray));
      else
        attr = ippAddOctetString(attrs, IPP_TAG_PRINTER, "printer-output-tray", output_tray, (cups_len_t)strlen(output_tray));
    }
  }
  else if (data->output_face_up)
  {
    papplCopyString(output_tray, "type=unRemovableBin;maxcapacity=-2;remaining=-2;status=0;name=face-up;stackingorder=lastToFirst;pagedelivery=faceUp;", sizeof(output_tray));
    ippAddOctetString(attrs, IPP_TAG_PRINTER, "printer-output-tray", output_tray, (cups_len_t)strlen(output_tray));
  }
  else
  {
    papplCopyString(output_tray, "type=unRemovableBin;maxcapacity=-2;remaining=-2;status=0;name=face-down;stackingorder=firstToLast;pagedelivery=faceDown;", sizeof(output_tray));
    ippAddOctetString(attrs, IPP_TAG_PRINTER, "printer-output-tray", output_tray, (cups_len_t)strlen(output_tray));
  }


  // printer-resolution-supported
  if (data->num_resolution > 0)
    ippAddResolutions(attrs, IPP_TAG_PRINTER, "printer-resolution-supported", (cups_len_t)data->num_resolution, IPP_RES_PER_INCH, data->x_resolution, data->y_resolution);


  // printer-settable-attributes
  memcpy((void *)svalues, printer_settable_attributes, sizeof(printer_settable_attributes));
  num_values = (int)(sizeof(printer_settable_attributes) / sizeof(printer_settable_attributes[0]));

  if (data->finishings)
  {
    svalues[num_values ++] = "finishings-col-default";
    svalues[num_values ++] = "finishings-default";
  }

  if (data->mode_supported)
    svalues[num_values ++] = "label-mode-configured";

  if (data->tear_offset_supported[1])
    svalues[num_values ++] = "label-tear-off-configured";

  if (data->num_bin)
    svalues[num_values ++] = "output-bin-default";

  if (data->darkness_supported)
    svalues[num_values ++] = "print-darkness-default";

  if (data->speed_supported[1])
    svalues[num_values ++] = "print-speed-default";

  if (data->darkness_supported)
    svalues[num_values ++] = "printer-darkness-configured";
  if (system->wifi_join_cb)
  {
    svalues[num_values ++] = "printer-wifi-password";
    svalues[num_values ++] = "printer-wifi-ssid";
  }
  if (data->sides_supported != PAPPL_SIDES_ONE_SIDED)
    svalues[num_values ++] = "sides-default";
  for (i = 0; i < (cups_len_t)data->num_vendor && num_values < (int)(sizeof(svalues) / sizeof(svalues[0])); i ++)
  {
    snprintf(vvalues[i], sizeof(vvalues[0]), "%s-default", data->vendor[i]);
    svalues[num_values ++] = vvalues[i];
  }

  ippAddStrings(attrs, IPP_TAG_PRINTER, IPP_TAG_KEYWORD, "printer-settable-attributes", num_values, NULL, svalues);


  // pwg-raster-document-resolution-supported
  if (data->num_resolution > 0)
    ippAddResolutions(attrs, IPP_TAG_PRINTER, "pwg-raster-document-resolution-supported", (cups_len_t)data->num_resolution, IPP_RES_PER_INCH, data->x_resolution, data->y_resolution);


  // pwg-raster-document-sheet-back
  if (data->duplex)
  {
    static const char * const backs[] =	// "pwg-raster-document-sheet-back" values
    {
      "normal",
      "flipped",
      "rotated",
      "manual-tumble"
    };

    ippAddString(attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "pwg-raster-document-sheet-back", NULL, backs[data->duplex - 1]);
  }


  // pwg-raster-document-type-supported
  for (num_values = 0, bit = PAPPL_PWG_RASTER_TYPE_ADOBE_RGB_8; bit <= PAPPL_PWG_RASTER_TYPE_SRGB_16; bit *= 2)
  {
    if (data->raster_types & bit)
      svalues[num_values ++] = _papplRasterTypeString(bit);
  }

  if (num_values > 0)
    ippAddStrings(attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "pwg-raster-document-type-supported", num_values, NULL, svalues);


  // sides-supported
  if (data->sides_supported)
  {
    for (num_values = 0, bit = PAPPL_SIDES_ONE_SIDED; bit <= PAPPL_SIDES_TWO_SIDED_SHORT_EDGE; bit *= 2)
    {
      if (data->sides_supported & bit)
	svalues[num_values ++] = _papplSidesString(bit);
    }

    ippAddStrings(attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "sides-supported", num_values, NULL, svalues);
  }
  else
    ippAddString(attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "sides-supported", NULL, "one-sided");

  // urf-supported
  if (data->num_resolution > 0)
  {
    char	dm[32],			// DM (duplex mode) value
		is[256],		// IS (media-source) values
		mt[256],		// MT (media-type) values
		ob[256],		// OB (output-bin) values
		rs[32];			// RS (resolution) values

    num_values = 0;
    svalues[num_values ++] = "V1.5";
    svalues[num_values ++] = "W8";
    if (data->raster_types & PAPPL_PWG_RASTER_TYPE_SRGB_8)
      svalues[num_values ++] = "SRGB24";
    if (data->raster_types & PAPPL_PWG_RASTER_TYPE_ADOBE_RGB_16)
      svalues[num_values ++] = "ADOBERGB24-48";
    else if (data->raster_types & PAPPL_PWG_RASTER_TYPE_ADOBE_RGB_8)
      svalues[num_values ++] = "ADOBERGB24";
    if (data->raster_types & PAPPL_PWG_RASTER_TYPE_BLACK_16)
      svalues[num_values ++] = "DEVW8-16";
    else if (data->raster_types & PAPPL_PWG_RASTER_TYPE_BLACK_8)
      svalues[num_values ++] = "DEVW8";
    if (data->raster_types & PAPPL_PWG_RASTER_TYPE_RGB_16)
      svalues[num_values ++] = "DEVRGB24-48";
    else if (data->raster_types & PAPPL_PWG_RASTER_TYPE_RGB_8)
      svalues[num_values ++] = "DEVRGB24";
    if (data->raster_types & PAPPL_PWG_RASTER_TYPE_CMYK_16)
      svalues[num_values ++] = "DEVCMYK32-64";
    else if (data->raster_types & PAPPL_PWG_RASTER_TYPE_CMYK_8)
      svalues[num_values ++] = "DEVCMYK32";
    svalues[num_values ++] = "PQ3-4-5";
    if (data->duplex)
    {
      snprintf(dm, sizeof(dm), "DM%d", (int)data->duplex);
      svalues[num_values ++] = dm;
    }
    else if (data->sides_supported & PAPPL_SIDES_TWO_SIDED_LONG_EDGE)
      svalues[num_values ++] = "DM1";

    if (fn[0])
      svalues[num_values ++] = fn;

    if (data->num_source)
    {
      static const char * const iss[] =	// IS/"media-source" values
      {
        "auto",
        "main",
        "alternate",
        "large-capacity",
        "manual",
        "envelope",
        "disc",
        "photo",
        "hagaki",
        "main-roll",
        "alternate-roll",
        "top",
        "middle",
        "bottom",
        "side",
        "left",
        "right",
        "center",
        "rear",
        "by-pass-tray",			// a.k.a. multi-purpose tray
        "tray-1",
        "tray-2",
        "tray-3",
        "tray-4",
        "tray-5",
        "tray-6",
        "tray-7",
        "tray-8",
        "tray-9",
        "tray-10",
        "tray-11",
        "tray-12",
        "tray-13",
        "tray-14",
        "tray-15",
        "tray-16",
        "tray-17",
        "tray-18",
        "tray-19",
        "tray-20",
        "roll-1",
        "roll-2",
        "roll-3",
        "roll-4",
        "roll-5",
        "roll-6",
        "roll-7",
        "roll-8",
        "roll-9",
        "roll-10"
      };

      for (i = 0, ptr = is, *ptr = '\0', prefix = "IS"; i < (cups_len_t)data->num_source; i ++)
      {
        for (j = 0; j < (int)(sizeof(iss) / sizeof(iss[0])); j ++)
        {
          if (!strcmp(iss[j], data->source[i]))
          {
            snprintf(ptr, sizeof(is) - (size_t)(ptr - is), "%s%u", prefix, (unsigned)j);
            ptr += strlen(ptr);
            prefix = "-";
          }
        }
      }

      if (is[0])
        svalues[num_values ++] = is;
    }

    if (data->num_type)
    {
      static const char * const mts[] =	// MT/"media-type" values
      {
        "auto",
        "stationery",
        "transparency",
        "envelope",
        "cardstock",
        "labels",
        "stationery-letterhead",
        "disc",
        "photographic-matte",
        "photographic-satin",
        "photographic-semi-gloss",
        "photographic-glossy",
        "photographic-high-gloss",
        "other"
      };

      for (i = 0, ptr = mt, *ptr = '\0', prefix = "MT"; i < (cups_len_t)data->num_type; i ++)
      {
        for (j = 0; j < (int)(sizeof(mts) / sizeof(mts[0])); j ++)
        {
          if (!strcmp(mts[j], data->type[i]))
          {
            snprintf(ptr, sizeof(mt) - (size_t)(ptr - mt), "%s%u", prefix, (unsigned)j);
            ptr += strlen(ptr);
            prefix = "-";
          }
        }
      }

      if (mt[0])
        svalues[num_values ++] = mt;
    }

    if (data->num_bin)
    {
      static const char * const obs[] =	// OB/"output-bin" values
      {
        "auto",
        "top",
        "middle",
        "bottom",
        "side",
        "left",
        "right",
        "center",
        "rear",
        "face-up",
        "face-down",
        "large-capacity",
        "stacker",
        "my-mailbox",
        "mailbox-1",
        "mailbox-2",
        "mailbox-3",
        "mailbox-4",
        "mailbox-5",
        "mailbox-6",
        "mailbox-7",
        "mailbox-8",
        "mailbox-9",
        "mailbox-10",
        "stacker-1",
        "stacker-2",
        "stacker-3",
        "stacker-4",
        "stacker-5",
        "stacker-6",
        "stacker-7",
        "stacker-8",
        "stacker-9",
        "stacker-10",
        "tray-1",
        "tray-2",
        "tray-3",
        "tray-4",
        "tray-5",
        "tray-6",
        "tray-7",
        "tray-8",
        "tray-9",
        "tray-10"
      };

      for (i = 0, ptr = ob, *ptr = '\0', prefix = "OB"; i < (cups_len_t)data->num_bin; i ++)
      {
        for (j = 0; j < (int)(sizeof(obs) / sizeof(obs[0])); j ++)
        {
          if (!strcmp(obs[j], data->bin[i]))
          {
            snprintf(ptr, sizeof(ob) - (size_t)(ptr - ob), "%s%u", prefix, (unsigned)j);
            ptr += strlen(ptr);
            prefix = "-";
          }
        }
      }

      if (ob[0])
        svalues[num_values ++] = ob;
    }
    else if (data->output_face_up)
      svalues[num_values ++] = "OB9";
    else
      svalues[num_values ++] = "OB10";

    if (data->input_face_up)
      svalues[num_values ++] = "IFU0";

    if (data->output_face_up)
      svalues[num_values ++] = "OFU0";

    if (data->num_resolution == 1)
      snprintf(rs, sizeof(rs), "RS%d", data->x_resolution[0]);
    else
      snprintf(rs, sizeof(rs), "RS%d-%d", data->x_resolution[data->num_resolution - 2], data->x_resolution[data->num_resolution - 1]);

    svalues[num_values ++] = rs;

    ippAddStrings(attrs, IPP_TAG_PRINTER, IPP_TAG_KEYWORD, "urf-supported", num_values, NULL, svalues);
  }

  return (attrs);
}


//
// 'validate_defaults()' - Validate the printing defaults and supported values.
//

static bool				// O - `true` if valid, `false` otherwise
validate_defaults(
    pappl_printer_t        *printer,	// I - Printer
    pappl_pr_driver_data_t *driver_data,// I - Driver values
    pappl_pr_driver_data_t *data)	// I - Default values
{
  bool		ret = true;		// Return value
  int		i;			// Looping var
  int		max_width = 0,		// Maximum media width
		max_length = 0,		// Maximum media length
		min_width = 99999999,	// Minimum media width
		min_length = 99999999;	// Minimum media length
  pwg_media_t	*pwg;			// PWG media size


  if (!(data->identify_default & driver_data->identify_supported) && driver_data->identify_supported)
  {
    papplLogPrinter(printer, PAPPL_LOGLEVEL_ERROR, "Unsupported identify-actions-default=0x%04x", data->identify_default);
    ret = false;
  }
  else if (driver_data->identify_supported)
  {
    papplLogPrinter(printer, PAPPL_LOGLEVEL_DEBUG, "identify-actions-default=0x%04x", data->identify_default);
  }

  for (i = 0; i < driver_data->num_media; i ++)
  {
    if (!strcmp(driver_data->media[i], data->media_default.size_name))
      break;

    if ((pwg = pwgMediaForPWG(driver_data->media[i])) != NULL)
    {
      if (pwg->width > max_width)
        max_width = pwg->width;
      if (pwg->width < min_width)
        min_width = pwg->width;

      if (pwg->length > max_length)
        max_length = pwg->length;
      if (pwg->length < min_length)
        min_length = pwg->length;
    }
  }

  if (i < driver_data->num_media || (data->media_default.size_width >= min_width && data->media_default.size_width <= max_width && data->media_default.size_length >= min_length && data->media_default.size_length <= max_length))
  {
    papplLogPrinter(printer, PAPPL_LOGLEVEL_DEBUG, "media-default=%s", data->media_default.size_name);
  }
  else
  {
    papplLogPrinter(printer, PAPPL_LOGLEVEL_ERROR, "Unsupported media-default=%s", data->media_default.size_name);
    papplLogPrinter(printer, PAPPL_LOGLEVEL_DEBUG, "width=%d, length=%d", data->media_default.size_width, data->media_default.size_length);
    papplLogPrinter(printer, PAPPL_LOGLEVEL_DEBUG, "num_media=%d, min_width=%d, max_width=%d, min_length=%d, max_length=%d", driver_data->num_media, min_width, max_width, min_length, max_length);
    ret = false;
  }

  papplLogPrinter(printer, PAPPL_LOGLEVEL_DEBUG, "orientation-requested-default=%d(%s)", data->orient_default, ippEnumString("orientation-requested", (int)data->orient_default));

  if (!(data->color_default & driver_data->color_supported))
  {
    papplLogPrinter(printer, PAPPL_LOGLEVEL_ERROR, "Unsupported print-color-mode-default=%s(0x%04x)", _papplColorModeString(data->color_default), data->color_default);
    ret = false;
  }
  else
  {
    papplLogPrinter(printer, PAPPL_LOGLEVEL_DEBUG, "print-color-mode-default=%s(0x%04x)", _papplColorModeString(data->color_default), data->color_default);
  }

  papplLogPrinter(printer, PAPPL_LOGLEVEL_DEBUG, "print-content-default=%s(0x%04x)", _papplContentString(data->content_default), data->content_default);

  papplLogPrinter(printer, PAPPL_LOGLEVEL_DEBUG, "print-quality-default=%d(%s)", (int)data->quality_default, ippEnumString("print-quality", (int)data->quality_default));

  papplLogPrinter(printer, PAPPL_LOGLEVEL_DEBUG, "print-scaling-default=%s(0x%04x)", _papplScalingString(data->scaling_default), data->scaling_default);

  for (i = 0; i < driver_data->num_resolution; i ++)
  {
    if (data->x_default == driver_data->x_resolution[i] && data->y_default == driver_data->y_resolution[i])
      break;
  }
  if (i >= driver_data->num_resolution)
  {
    papplLogPrinter(printer, PAPPL_LOGLEVEL_ERROR, "Unsupported printer-resolution-default=%dx%ddpi", data->x_default, data->y_default);
    ret = false;
  }
  else
  {
    papplLogPrinter(printer, PAPPL_LOGLEVEL_DEBUG, "printer-resolution-default=%dx%ddpi", data->x_default, data->y_default);
  }

  if (!(data->sides_default & driver_data->sides_supported) && driver_data->sides_supported)
  {
    papplLogPrinter(printer, PAPPL_LOGLEVEL_ERROR, "Unsupported sides-default=%s(0x%04x)", _papplSidesString(data->sides_default), data->sides_default);
    ret = false;
  }
  else if (driver_data->sides_supported)
  {
    papplLogPrinter(printer, PAPPL_LOGLEVEL_DEBUG, "sides-default=%s(0x%04x)", _papplSidesString(data->sides_default), data->sides_default);
  }

  return (ret);
}


//
// 'validate_driver()' - Validate the driver-specific values.
//

static bool				// O - `true` if valid, `false` otherwise
validate_driver(
    pappl_printer_t        *printer,	// I - Printer
    pappl_pr_driver_data_t *data)	// I - Driver values
{
  bool		ret = true;		// Return value
  int		i,			// Looping variable
		num_icons;		// Number of printer icons
  const char	*venptr;		// Pointer into vendor name
  static const char * const icon_sizes[] =
  {					// Icon sizes
    "small-48x48",
    "medium-128x128",
    "large-512x512"
  };


  // Validate all driver fields and show debug/warning/fatal errors along the way.
  if (data->extension)
    papplLogPrinter(printer, PAPPL_LOGLEVEL_DEBUG, "Driver uses extension data (%p) and %sdelete function.", data->extension, data->delete_cb ? "" : "no ");

  if (!data->identify_cb)
    papplLogPrinter(printer, PAPPL_LOGLEVEL_WARN, "Driver does not support identification.");

  if (data->printfile_cb)
  {
    if (data->format)
    {
      papplLogPrinter(printer, PAPPL_LOGLEVEL_DEBUG, "Driver supports raw printing of '%s' files.", data->format);
    }
    else
    {
      papplLogPrinter(printer, PAPPL_LOGLEVEL_ERROR, "Driver supports raw printing but hasn't set the format.");
      ret = false;
    }
  }

  if (!data->rendjob_cb || !data->rendpage_cb || !data->rstartjob_cb || !data->rstartpage_cb || !data->rwriteline_cb)
  {
    papplLogPrinter(printer, PAPPL_LOGLEVEL_ERROR, "Driver does not provide required raster printing callbacks.");
    ret = false;
  }

  if (!data->status_cb)
    papplLogPrinter(printer, PAPPL_LOGLEVEL_WARN, "Driver does not support status updates.");

  if (!data->testpage_cb)
    papplLogPrinter(printer, PAPPL_LOGLEVEL_WARN, "Driver does not support a self-test page.");

  if (!data->make_and_model[0])
  {
    papplLogPrinter(printer, PAPPL_LOGLEVEL_ERROR, "Driver does not provide a make_and_model string.");
    ret = false;
  }

  if (data->ppm <= 0)
  {
    papplLogPrinter(printer, PAPPL_LOGLEVEL_ERROR, "Driver does not provide a valid ppm value (%d).", data->ppm);
    ret = false;
  }
  else
  {
    papplLogPrinter(printer, PAPPL_LOGLEVEL_DEBUG, "Driver reports ppm %d.", data->ppm);
  }

  if (data->ppm_color < 0 || data->ppm_color > data->ppm)
  {
    papplLogPrinter(printer, PAPPL_LOGLEVEL_ERROR, "Driver does not provide a valid ppm_color value (%d).", data->ppm_color);
    ret = false;
  }
  else
  {
    papplLogPrinter(printer, PAPPL_LOGLEVEL_DEBUG, "Driver reports ppm_color %d.", data->ppm_color);
  }

  for (i = 0, num_icons = 0; i < 3; i ++)
  {
    if (data->icons[i].filename[0])
    {
      papplLogPrinter(printer, PAPPL_LOGLEVEL_DEBUG, "Driver provides %s icon in file '%s'.", icon_sizes[i], data->icons[i].filename);
      num_icons ++;
    }
    else if (data->icons[i].data && data->icons[i].datalen > 0)
    {
      papplLogPrinter(printer, PAPPL_LOGLEVEL_DEBUG, "Driver provides %s icon in memory (%u bytes).", icon_sizes[i], (unsigned)data->icons[i].datalen);
      num_icons ++;
    }
  }

  if (num_icons == 0)
  {
    papplLogPrinter(printer, PAPPL_LOGLEVEL_WARN, "Driver does not provide printer icons, using defaults.");
  }

  if (!data->raster_types)
  {
    papplLogPrinter(printer, PAPPL_LOGLEVEL_ERROR, "Driver does not provide required raster types.");
    ret = false;
  }

  if (data->num_resolution <= 0)
  {
    papplLogPrinter(printer, PAPPL_LOGLEVEL_ERROR, "Driver does not provide required raster resolutions.");
    ret = false;
  }
  else
  {
    for (i = 0; i < data->num_resolution; i ++)
    {
      if (data->x_resolution[i] <= 0 || data->y_resolution[i] <= 0)
      {
	papplLogPrinter(printer, PAPPL_LOGLEVEL_ERROR, "Invalid driver raster resolution %dx%ddpi.", data->x_resolution[i], data->y_resolution[i]);
	ret = false;
      }
    }
  }

  if (data->left_right < 0)
  {
    papplLogPrinter(printer, PAPPL_LOGLEVEL_ERROR, "Invalid driver left/right margins value %d.", data->left_right);
    ret = false;
  }

  if (data->bottom_top < 0)
  {
    papplLogPrinter(printer, PAPPL_LOGLEVEL_ERROR, "Invalid driver bottom/top margins value %d.", data->bottom_top);
    ret = false;
  }

  for (i = 0; i < data->num_media; i ++)
  {
    if (!pwgMediaForPWG(data->media[i]))
    {
      papplLogPrinter(printer, PAPPL_LOGLEVEL_ERROR, "Invalid driver media value '%s'.", data->media[i]);
      ret = false;
    }
  }

  for (i = 0; i < data->num_vendor; i ++)
  {
    for (venptr = data->vendor[i]; *venptr; venptr ++)
    {
      int vench = *venptr & 255;	// Current character

      if (!isalnum(vench) && vench != '-' && vench != '_')
        break;
    }

    if (*venptr)
    {
      papplLogPrinter(printer, PAPPL_LOGLEVEL_ERROR, "Invalid vendor attribute name '%s'.", data->vendor[i]);
      ret = false;
    }
  }

  return (ret);
}


//
// 'validate_ready()' - Validate the ready media values.
//

static bool				// O - `true` if valid, `false` otherwise
validate_ready(
    pappl_printer_t        *printer,	// I - Printer
    pappl_pr_driver_data_t *driver_data,// I - Driver data
    int                    num_ready,	// I - Number of ready media values
    pappl_media_col_t      *ready)	// I - Ready media values
{
  bool		ret = true;		// Return value
  int		i, j;			// Looping vars
  int		max_width = 0,		// Maximum media width
		max_length = 0,		// Maximum media length
		min_width = 99999999,	// Minimum media width
		min_length = 99999999;	// Minimum media length
  pwg_media_t	*pwg;			// PWG media size


  if (num_ready > PAPPL_MAX_SOURCE)
    return (false);

  // Determine the range of media sizes...
  for (i = 0; i < driver_data->num_media; i ++)
  {
    if ((pwg = pwgMediaForPWG(driver_data->media[i])) != NULL)
    {
      if (pwg->width > max_width)
        max_width = pwg->width;
      if (pwg->width < min_width)
        min_width = pwg->width;

      if (pwg->length > max_length)
        max_length = pwg->length;
      if (pwg->length < min_length)
        min_length = pwg->length;
    }
  }

  for (i = 0; i < num_ready; i ++)
  {
    if (!ready[i].size_name[0])
      continue;

    if (!pwgMediaForPWG(ready[i].size_name))
    {
      papplLogPrinter(printer, PAPPL_LOGLEVEL_ERROR, "Invalid media-ready.media-size-name='%s'.", ready[i].size_name);
      ret = false;
    }
    else if (ready[i].size_width < min_width || ready[i].size_width > max_width || ready[i].size_length < min_length || ready[i].size_length > max_length)
    {
      papplLogPrinter(printer, PAPPL_LOGLEVEL_ERROR, "Unsupported media-ready.media-size=%.2fx%.2fmm.", ready[i].size_width * 0.01, ready[i].size_length * 0.01);
      ret = false;
    }

    if (ready[i].left_margin < driver_data->left_right && !driver_data->borderless)
    {
      papplLogPrinter(printer, PAPPL_LOGLEVEL_ERROR, "Unsupported media-ready.media-left-margin=%d.", ready[i].left_margin);
      ret = false;
    }

    if (ready[i].right_margin < driver_data->left_right && !driver_data->borderless)
    {
      papplLogPrinter(printer, PAPPL_LOGLEVEL_ERROR, "Unsupported media-ready.media-right-margin=%d.", ready[i].right_margin);
      ret = false;
    }

    if (ready[i].top_margin < driver_data->bottom_top && !driver_data->borderless)
    {
      papplLogPrinter(printer, PAPPL_LOGLEVEL_ERROR, "Unsupported media-ready.media-top-margin=%d.", ready[i].top_margin);
      ret = false;
    }

    if (ready[i].bottom_margin < driver_data->bottom_top && !driver_data->borderless)
    {
      papplLogPrinter(printer, PAPPL_LOGLEVEL_ERROR, "Unsupported media-ready.media-bottom-margin=%d.", ready[i].bottom_margin);
      ret = false;
    }

    for (j = 0; j < driver_data->num_source; j ++)
    {
      if (!strcmp(ready[i].source, driver_data->source[j]))
        break;
    }
    if (j >= driver_data->num_source)
    {
      papplLogPrinter(printer, PAPPL_LOGLEVEL_ERROR, "Unsupported media-ready.media-source='%s'.", ready[i].source);
      ret = false;
    }

    for (j = 0; j < driver_data->num_type; j ++)
    {
      if (!strcmp(ready[i].type, driver_data->type[j]))
        break;
    }
    if (j >= driver_data->num_type)
    {
      papplLogPrinter(printer, PAPPL_LOGLEVEL_ERROR, "Unsupported media-ready.media-type='%s'.", ready[i].type);
      ret = false;
    }
  }

  return (ret);
}
