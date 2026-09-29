//
// Scanner driver functions for the Printer Application Framework
//
// Copyright © 2024-2026 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#include "pappl-private.h"


//
// Local functions...
//

static const char	*scan_color_mode_string(pappl_scan_color_mode_t value);
static const char	*scan_content_string(pappl_scan_content_t value);
static const char	*scan_input_source_string(pappl_scan_input_source_t value);
static const char	*scan_intent_string(pappl_scan_intent_t value);
static ipp_t		*make_attrs(pappl_system_t *system, pappl_scanner_t *scanner, pappl_sc_driver_data_t *data);
static bool		validate_defaults(pappl_scanner_t *scanner, pappl_sc_driver_data_t *driver_data, pappl_sc_driver_data_t *data);
static bool		validate_driver(pappl_scanner_t *scanner, pappl_sc_driver_data_t *data);


//
// '_papplScannerCopyAttributesNoLock()' - Copy scanner attributes to an IPP
//                                        response.
//
// This function copies all scanner capability, state, and dynamic attributes
// to an IPP response message.  The caller must hold the scanner read lock.
//

void
_papplScannerCopyAttributesNoLock(
    pappl_scanner_t *scanner,		// I - Scanner
    ipp_t           *ipp,		// I - IPP message
    cups_array_t    *ra)		// I - Requested attributes or NULL for all
{
  size_t		i,		// Looping var
			num_values;	// Number of values
  const char		*svalues[100];	// String values


  // Copy static and driver-specific attributes...
  _papplCopyAttributes(ipp, scanner->attrs, ra, IPP_TAG_ZERO, true);
  _papplCopyAttributes(ipp, scanner->driver_attrs, ra, IPP_TAG_ZERO, false);

  // Copy scanner state...
  _papplScannerCopyStateNoLock(scanner, IPP_TAG_PRINTER, ipp, ra);

  // Dynamic defaults...
  if (!ra || cupsArrayFind(ra, "scan-color-mode-default"))
  {
    const char *v = scan_color_mode_string(scanner->driver_data.color_default);

    if (v)
      ippAddString(ipp, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "scan-color-mode-default", NULL, v);
  }

  if (!ra || cupsArrayFind(ra, "input-source-default"))
  {
    const char *v = scan_input_source_string(scanner->driver_data.input_source_default);

    if (v)
      ippAddString(ipp, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "input-source-default", NULL, v);
  }

  if (!ra || cupsArrayFind(ra, "scan-intent-default"))
  {
    const char *v = scan_intent_string(scanner->driver_data.intent_default);

    if (v)
      ippAddString(ipp, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "scan-intent-default", NULL, v);
  }

  if (!ra || cupsArrayFind(ra, "scan-content-type-default"))
  {
    const char *v = scan_content_string(scanner->driver_data.content_default);

    if (v)
      ippAddString(ipp, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "scan-content-type-default", NULL, v);
  }

  if (!ra || cupsArrayFind(ra, "scanner-resolution-default"))
    ippAddResolution(ipp, IPP_TAG_PRINTER, "scanner-resolution-default", IPP_RES_PER_INCH, scanner->driver_data.x_default, scanner->driver_data.y_default);

  // Document format default (first format in list)...
  if ((!ra || cupsArrayFind(ra, "document-format-default")) && scanner->driver_data.num_format > 0)
    ippAddString(ipp, IPP_TAG_PRINTER, IPP_TAG_MIMETYPE, "document-format-default", NULL, scanner->driver_data.format[0]);

  // scanner-contact-col...
  if (!ra || cupsArrayFind(ra, "scanner-contact-col"))
  {
    ipp_t	*col = ippNew();	// Contact collection

    if (scanner->contact.name[0])
      ippAddString(col, IPP_TAG_ZERO, IPP_TAG_NAME, "contact-name", NULL, scanner->contact.name);
    if (scanner->contact.email[0])
      ippAddString(col, IPP_TAG_ZERO, IPP_TAG_NAME, "contact-email", NULL, scanner->contact.email);
    if (scanner->contact.telephone[0])
      ippAddString(col, IPP_TAG_ZERO, IPP_TAG_URI, "contact-telephone", NULL, scanner->contact.telephone);

    ippAddCollection(ipp, IPP_TAG_PRINTER, "scanner-contact-col", col);
    ippDelete(col);
  }

  // scanner-dns-sd-name...
  if (!ra || cupsArrayFind(ra, "scanner-dns-sd-name"))
  {
    if (scanner->dns_sd_name)
      ippAddString(ipp, IPP_TAG_PRINTER, IPP_TAG_NAME, "scanner-dns-sd-name", NULL, scanner->dns_sd_name);
    else
      ippAddString(ipp, IPP_TAG_PRINTER, IPP_TAG_NAME, "scanner-dns-sd-name", NULL, "");
  }

  // scanner-geo-location...
  if (!ra || cupsArrayFind(ra, "scanner-geo-location"))
  {
    if (scanner->geo_location)
      ippAddString(ipp, IPP_TAG_PRINTER, IPP_TAG_URI, "scanner-geo-location", NULL, scanner->geo_location);
    else
      ippAddOutOfBand(ipp, IPP_TAG_PRINTER, IPP_TAG_UNKNOWN, "scanner-geo-location");
  }

  // scanner-impressions-completed...
  if (!ra || cupsArrayFind(ra, "scanner-impressions-completed"))
    ippAddInteger(ipp, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "scanner-impressions-completed", scanner->impcompleted);

  // scanner-location...
  if (!ra || cupsArrayFind(ra, "scanner-location"))
    ippAddString(ipp, IPP_TAG_PRINTER, IPP_TAG_TEXT, "scanner-location", NULL, scanner->location ? scanner->location : "");

  // scanner-organization / scanner-organizational-unit...
  if (!ra || cupsArrayFind(ra, "scanner-organization"))
    ippAddString(ipp, IPP_TAG_PRINTER, IPP_TAG_TEXT, "scanner-organization", NULL, scanner->organization ? scanner->organization : "");

  if (!ra || cupsArrayFind(ra, "scanner-organizational-unit"))
    ippAddString(ipp, IPP_TAG_PRINTER, IPP_TAG_TEXT, "scanner-organizational-unit", NULL, scanner->org_unit ? scanner->org_unit : "");

  // scanner-settable-attributes...
  if (!ra || cupsArrayFind(ra, "scanner-settable-attributes"))
  {
    static const char * const settable[] =
    {
      "scan-color-mode-default",
      "input-source-default",
      "scan-intent-default",
      "scan-content-type-default",
      "scanner-contact-col",
      "scanner-dns-sd-name",
      "scanner-geo-location",
      "scanner-location",
      "scanner-organization",
      "scanner-organizational-unit",
      "scanner-resolution-default"
    };

    ippAddStrings(ipp, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "scanner-settable-attributes", (size_t)(sizeof(settable) / sizeof(settable[0])), NULL, settable);
  }

  // scanner-up-time...
  if (!ra || cupsArrayFind(ra, "scanner-up-time"))
    ippAddInteger(ipp, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "scanner-up-time", (int)(time(NULL) - scanner->start_time));

  // scanner-uuid...
  if (!ra || cupsArrayFind(ra, "scanner-uuid"))
    ippAddString(ipp, IPP_TAG_PRINTER, IPP_TAG_URI, "scanner-uuid", NULL, scanner->uuid);

  // scanner-config-change-time...
  if (!ra || cupsArrayFind(ra, "scanner-config-change-time"))
    ippAddInteger(ipp, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "scanner-config-change-time", (int)(scanner->config_time - scanner->start_time));

  // scanner-config-change-date-time...
  if (!ra || cupsArrayFind(ra, "scanner-config-change-date-time"))
    ippAddDate(ipp, IPP_TAG_PRINTER, "scanner-config-change-date-time", ippTimeToDate(scanner->config_time));

  // queued-scan-job-count...
  if (!ra || cupsArrayFind(ra, "queued-scan-job-count"))
    ippAddInteger(ipp, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "queued-scan-job-count", (int)cupsArrayGetCount(scanner->active_jobs));

  // Document format supported (dynamic, for convenience)...
  if (!ra || cupsArrayFind(ra, "document-format-supported"))
  {
    num_values = scanner->driver_data.num_format;
    if (num_values > 100)
      num_values = 100;

    for (i = 0; i < num_values; i ++)
      svalues[i] = scanner->driver_data.format[i];

    if (num_values > 0)
      ippAddStrings(ipp, IPP_TAG_PRINTER, IPP_TAG_MIMETYPE, "document-format-supported", num_values, NULL, svalues);
  }
}


//
// '_papplScannerCopyStateNoLock()' - Copy the scanner-state-xxx attributes.
//
// This function copies the scanner state, state reasons, and state message
// attributes to an IPP response.  The caller must hold the scanner read lock.
//

void
_papplScannerCopyStateNoLock(
    pappl_scanner_t *scanner,		// I - Scanner
    ipp_tag_t       group_tag,		// I - Group tag
    ipp_t           *ipp,		// I - IPP message
    cups_array_t    *ra)		// I - Requested attributes or NULL
{
  // scanner-is-accepting-jobs...
  if (!ra || cupsArrayFind(ra, "scanner-is-accepting-jobs"))
    ippAddBoolean(ipp, group_tag, "scanner-is-accepting-jobs", !scanner->is_deleted);

  // scanner-state...
  if (!ra || cupsArrayFind(ra, "scanner-state"))
    ippAddInteger(ipp, group_tag, IPP_TAG_ENUM, "scanner-state", (int)scanner->state);

  // scanner-state-message...
  if (!ra || cupsArrayFind(ra, "scanner-state-message"))
  {
    static const char * const messages[] = { "Idle.", "Scanning.", "Stopped." };

    ippAddString(ipp, group_tag, IPP_CONST_TAG(IPP_TAG_TEXT), "scanner-state-message", NULL, messages[scanner->state - IPP_PSTATE_IDLE]);
  }

  // scanner-state-reasons...
  if (!ra || cupsArrayFind(ra, "scanner-state-reasons"))
  {
    ipp_attribute_t	*attr = NULL;	// scanner-state-reasons

    if (scanner->state_reasons == PAPPL_PREASON_NONE)
    {
      if (scanner->state == IPP_PSTATE_STOPPED)
        attr = ippAddString(ipp, group_tag, IPP_CONST_TAG(IPP_TAG_KEYWORD), "scanner-state-reasons", NULL, "paused");
      else
        attr = ippAddString(ipp, group_tag, IPP_CONST_TAG(IPP_TAG_KEYWORD), "scanner-state-reasons", NULL, "none");
    }
    else
    {
      pappl_preason_t	bit;		// Reason bit

      for (bit = PAPPL_PREASON_OTHER; bit <= PAPPL_PREASON_IDENTIFY_PRINTER_REQUESTED; bit *= 2)
      {
        if (scanner->state_reasons & bit)
        {
          if (attr)
            ippSetString(ipp, &attr, ippGetCount(attr), _papplPrinterReasonString(bit));
          else
            attr = ippAddString(ipp, group_tag, IPP_CONST_TAG(IPP_TAG_KEYWORD), "scanner-state-reasons", NULL, _papplPrinterReasonString(bit));
        }
      }

      if (scanner->state == IPP_PSTATE_STOPPED && !attr)
        attr = ippAddString(ipp, group_tag, IPP_CONST_TAG(IPP_TAG_KEYWORD), "scanner-state-reasons", NULL, "paused");
    }

    (void)attr;
  }

  // scanner-state-change-time...
  if (!ra || cupsArrayFind(ra, "scanner-state-change-time"))
    ippAddInteger(ipp, group_tag, IPP_TAG_INTEGER, "scanner-state-change-time", (int)(scanner->state_time - scanner->start_time));

  // scanner-state-change-date-time...
  if (!ra || cupsArrayFind(ra, "scanner-state-change-date-time"))
    ippAddDate(ipp, group_tag, "scanner-state-change-date-time", ippTimeToDate(scanner->state_time));
}


//
// '_papplScannerInitDriverData()' - Initialize scanner driver data to defaults.
//

void
_papplScannerInitDriverData(
    pappl_sc_driver_data_t *d)		// I - Driver data
{
  // Zero everything...
  memset(d, 0, sizeof(pappl_sc_driver_data_t));

  // Set default color mode...
  d->color_supported = PAPPL_SCAN_COLOR_MODE_RGB_24 | PAPPL_SCAN_COLOR_MODE_GRAYSCALE_8;
  d->color_default   = PAPPL_SCAN_COLOR_MODE_RGB_24;

  // Set default intents (all mandatory per eSCL spec)...
  d->intents_supported = PAPPL_SCAN_INTENT_DOCUMENT | PAPPL_SCAN_INTENT_PHOTO | PAPPL_SCAN_INTENT_PREVIEW | PAPPL_SCAN_INTENT_TEXT_AND_GRAPHIC;
  d->intent_default    = PAPPL_SCAN_INTENT_DOCUMENT;

  // Set default input source...
  d->input_sources_supported = PAPPL_SCAN_INPUT_SOURCE_PLATEN;
  d->input_source_default    = PAPPL_SCAN_INPUT_SOURCE_PLATEN;

  // Set default content type...
  d->content_supported = PAPPL_SCAN_CONTENT_AUTO;
  d->content_default   = PAPPL_SCAN_CONTENT_AUTO;

  // Set default resolution (300 DPI)...
  d->num_resolution    = 1;
  d->x_resolution[0]   = 300;
  d->y_resolution[0]   = 300;
  d->x_default         = 300;
  d->y_default         = 300;

  // Set default document formats (JPEG and PDF are mandatory per eSCL spec)...
  d->num_format = 2;
  cupsCopyString(d->format[0], "image/jpeg", sizeof(d->format[0]));
  cupsCopyString(d->format[1], "application/pdf", sizeof(d->format[1]));

  // Set default scan area to US Letter (8.5" x 11" in 1/300")...
  d->platen_min_width  = 1;
  d->platen_min_height = 1;
  d->platen_max_width  = 2550;		// 8.5 * 300
  d->platen_max_height = 3300;		// 11.0 * 300
}


//
// 'papplScannerSetDriverData()' - Set the driver data for a scanner.
//
// This function validates and sets the driver data, including all capabilities
// and defaults.
//
// > Note: This function regenerates all of the driver-specific capability
// > attributes like "scan-color-mode-supported", "scanner-resolution-supported",
// > and so forth.  Use @link papplScannerSetDriverDefaults@ to efficiently
// > change the default values only.
//

bool					// O - `true` on success, `false` on failure
papplScannerSetDriverData(
    pappl_scanner_t        *scanner,	// I - Scanner
    pappl_sc_driver_data_t *data,	// I - Driver data
    ipp_t                  *attrs)	// I - Additional capability attributes or `NULL` for none
{
  if (!scanner || !data)
    return (false);

  // Validate data...
  if (!validate_defaults(scanner, data, data) || !validate_driver(scanner, data))
    return (false);

  _papplRWLockWrite(scanner);

  // Copy driver data to scanner...
  memcpy(&scanner->driver_data, data, sizeof(scanner->driver_data));

  // Create scanner (capability) attributes based on driver data...
  ippDelete(scanner->driver_attrs);
  scanner->driver_attrs = make_attrs(scanner->system, scanner, &scanner->driver_data);

  if (attrs)
    ippCopyAttributes(scanner->driver_attrs, attrs, 0, NULL, NULL);

  _papplRWUnlock(scanner);

  return (true);
}


//
// 'papplScannerSetDriverDefaults()' - Set default values in the driver data.
//
// This function validates and sets the scanner's default scan option values.
//
// > Note: Unlike @link papplScannerSetDriverData@, this function only
// > changes the "xxx_default" members of the driver data and is considered
// > lightweight.
//

bool					// O - `true` on success, `false` on failure
papplScannerSetDriverDefaults(
    pappl_scanner_t        *scanner,	// I - Scanner
    pappl_sc_driver_data_t *data)	// I - Driver data with new defaults
{
  if (!scanner || !data)
    return (false);

  if (!validate_defaults(scanner, &scanner->driver_data, data))
    return (false);

  _papplRWLockWrite(scanner);

  // Copy default values only...
  scanner->driver_data.color_default        = data->color_default;
  scanner->driver_data.intent_default       = data->intent_default;
  scanner->driver_data.input_source_default = data->input_source_default;
  scanner->driver_data.content_default      = data->content_default;
  scanner->driver_data.x_default            = data->x_default;
  scanner->driver_data.y_default            = data->y_default;

  scanner->config_time = time(NULL);

  _papplRWUnlock(scanner);

  _papplSystemConfigChanged(scanner->system);

  return (true);
}


//
// 'make_attrs()' - Make the capability attributes for the given scanner driver
//                  data.
//

static ipp_t *				// O - Driver attributes
make_attrs(
    pappl_system_t         *system,	// I - System
    pappl_scanner_t        *scanner,	// I - Scanner
    pappl_sc_driver_data_t *data)	// I - Driver data
{
  ipp_t			*attrs;		// Driver attributes
  size_t		i,		// Looping var
			num_values;	// Number of values
  const char		*svalues[100];	// String values


  (void)system;

  // Create an empty IPP message for the attributes...
  attrs = ippNew();


  // document-format-supported
  num_values = data->num_format;
  if (num_values > PAPPL_MAX_SCAN_FORMAT)
    num_values = PAPPL_MAX_SCAN_FORMAT;

  for (i = 0; i < num_values; i ++)
    svalues[i] = data->format[i];

  if (num_values > 0)
    ippAddStrings(attrs, IPP_TAG_PRINTER, IPP_TAG_MIMETYPE, "document-format-supported", num_values, NULL, svalues);


  // duplex-supported
  ippAddBoolean(attrs, IPP_TAG_PRINTER, "duplex-supported", data->duplex_supported);


  // input-source-supported
  for (num_values = 0, i = PAPPL_SCAN_INPUT_SOURCE_PLATEN; i <= PAPPL_SCAN_INPUT_SOURCE_CAMERA; i *= 2)
  {
    if (data->input_sources_supported & i)
    {
      const char *v = scan_input_source_string((pappl_scan_input_source_t)i);

      if (v)
        svalues[num_values ++] = v;
    }
  }

  if (num_values > 0)
    ippAddStrings(attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "input-source-supported", num_values, NULL, svalues);


  // scan-color-mode-supported
  for (num_values = 0, i = PAPPL_SCAN_COLOR_MODE_BLACK_AND_WHITE_1; i <= PAPPL_SCAN_COLOR_MODE_RGB_48; i *= 2)
  {
    if (data->color_supported & i)
    {
      const char *v = scan_color_mode_string((pappl_scan_color_mode_t)i);

      if (v)
        svalues[num_values ++] = v;
    }
  }

  if (num_values > 0)
    ippAddStrings(attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "scan-color-mode-supported", num_values, NULL, svalues);


  // scan-content-type-supported
  for (num_values = 0, i = PAPPL_SCAN_CONTENT_AUTO; i <= PAPPL_SCAN_CONTENT_TEXT_AND_PHOTO; i *= 2)
  {
    if (data->content_supported & i)
    {
      const char *v = scan_content_string((pappl_scan_content_t)i);

      if (v)
        svalues[num_values ++] = v;
    }
  }

  if (num_values > 0)
    ippAddStrings(attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "scan-content-type-supported", num_values, NULL, svalues);


  // scan-intent-supported
  for (num_values = 0, i = PAPPL_SCAN_INTENT_DOCUMENT; i <= PAPPL_SCAN_INTENT_BUSINESS_CARD; i *= 2)
  {
    if (data->intents_supported & i)
    {
      const char *v = scan_intent_string((pappl_scan_intent_t)i);

      if (v)
        svalues[num_values ++] = v;
    }
  }

  if (num_values > 0)
    ippAddStrings(attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "scan-intent-supported", num_values, NULL, svalues);


  // scanner-make-and-model
  ippAddString(attrs, IPP_TAG_PRINTER, IPP_TAG_TEXT, "scanner-make-and-model", NULL, data->make_and_model);


  // scanner-resolution-supported
  if (data->num_resolution > 0)
    ippAddResolutions(attrs, IPP_TAG_PRINTER, "scanner-resolution-supported", data->num_resolution, IPP_RES_PER_INCH, data->x_resolution, data->y_resolution);


  // Scan area attributes (platen)...
  if (data->input_sources_supported & PAPPL_SCAN_INPUT_SOURCE_PLATEN)
  {
    ippAddInteger(attrs, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "platen-min-width", data->platen_min_width);
    ippAddInteger(attrs, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "platen-min-height", data->platen_min_height);
    ippAddInteger(attrs, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "platen-max-width", data->platen_max_width);
    ippAddInteger(attrs, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "platen-max-height", data->platen_max_height);
  }

  // Scan area attributes (ADF)...
  if (data->input_sources_supported & PAPPL_SCAN_INPUT_SOURCE_ADF)
  {
    ippAddInteger(attrs, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "adf-min-width", data->adf_min_width);
    ippAddInteger(attrs, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "adf-min-height", data->adf_min_height);
    ippAddInteger(attrs, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "adf-max-width", data->adf_max_width);
    ippAddInteger(attrs, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "adf-max-height", data->adf_max_height);
  }


  // Image adjustment capabilities...
  if (data->brightness_supported)
    ippAddBoolean(attrs, IPP_TAG_PRINTER, "brightness-supported", true);

  if (data->contrast_supported)
    ippAddBoolean(attrs, IPP_TAG_PRINTER, "contrast-supported", true);

  if (data->sharpen_supported)
    ippAddBoolean(attrs, IPP_TAG_PRINTER, "sharpen-supported", true);

  if (data->threshold_supported)
    ippAddBoolean(attrs, IPP_TAG_PRINTER, "threshold-supported", true);


  return (attrs);
}


//
// 'scan_color_mode_string()' - Return the keyword for a scan color mode bit
//                               value.
//

static const char *			// O - Keyword or NULL
scan_color_mode_string(
    pappl_scan_color_mode_t value)	// I - Color mode bit value
{
  switch (value)
  {
    case PAPPL_SCAN_COLOR_MODE_BLACK_AND_WHITE_1 :
        return ("black-and-white");
    case PAPPL_SCAN_COLOR_MODE_GRAYSCALE_8 :
        return ("grayscale");
    case PAPPL_SCAN_COLOR_MODE_GRAYSCALE_16 :
        return ("grayscale16");
    case PAPPL_SCAN_COLOR_MODE_RGB_24 :
        return ("color");
    case PAPPL_SCAN_COLOR_MODE_RGB_48 :
        return ("color48");
    default :
        return (NULL);
  }
}


//
// 'scan_content_string()' - Return the keyword for a scan content type bit
//                            value.
//

static const char *			// O - Keyword or NULL
scan_content_string(
    pappl_scan_content_t value)		// I - Content type bit value
{
  switch (value)
  {
    case PAPPL_SCAN_CONTENT_AUTO :
        return ("auto");
    case PAPPL_SCAN_CONTENT_HALFTONE :
        return ("halftone");
    case PAPPL_SCAN_CONTENT_LINE_ART :
        return ("line-art");
    case PAPPL_SCAN_CONTENT_MAGAZINE :
        return ("magazine");
    case PAPPL_SCAN_CONTENT_PHOTO :
        return ("photo");
    case PAPPL_SCAN_CONTENT_TEXT :
        return ("text");
    case PAPPL_SCAN_CONTENT_TEXT_AND_PHOTO :
        return ("text-and-photo");
    default :
        return (NULL);
  }
}


//
// 'scan_input_source_string()' - Return the keyword for a scan input source
//                                 bit value.
//

static const char *			// O - Keyword or NULL
scan_input_source_string(
    pappl_scan_input_source_t value)	// I - Input source bit value
{
  switch (value)
  {
    case PAPPL_SCAN_INPUT_SOURCE_PLATEN :
        return ("platen");
    case PAPPL_SCAN_INPUT_SOURCE_ADF :
        return ("adf");
    case PAPPL_SCAN_INPUT_SOURCE_CAMERA :
        return ("camera");
    default :
        return (NULL);
  }
}


//
// 'scan_intent_string()' - Return the keyword for a scan intent bit value.
//

static const char *			// O - Keyword or NULL
scan_intent_string(
    pappl_scan_intent_t value)		// I - Intent bit value
{
  switch (value)
  {
    case PAPPL_SCAN_INTENT_DOCUMENT :
        return ("document");
    case PAPPL_SCAN_INTENT_PHOTO :
        return ("photo");
    case PAPPL_SCAN_INTENT_PREVIEW :
        return ("preview");
    case PAPPL_SCAN_INTENT_TEXT_AND_GRAPHIC :
        return ("text-and-graphic");
    case PAPPL_SCAN_INTENT_BUSINESS_CARD :
        return ("business-card");
    default :
        return (NULL);
  }
}


//
// 'validate_defaults()' - Validate the scanner default values against the
//                          supported capabilities.
//

static bool				// O - `true` if valid, `false` otherwise
validate_defaults(
    pappl_scanner_t        *scanner,	// I - Scanner
    pappl_sc_driver_data_t *driver_data,// I - Driver values (supported caps)
    pappl_sc_driver_data_t *data)	// I - Default values to validate
{
  bool		ret = true;		// Return value
  size_t	i;			// Looping var


  // Validate color mode default...
  if (!(data->color_default & driver_data->color_supported))
  {
    papplLog(scanner->system, PAPPL_LOGLEVEL_ERROR, "Scanner '%s': Unsupported scan-color-mode-default=0x%04x", scanner->name, data->color_default);
    ret = false;
  }
  else
  {
    papplLog(scanner->system, PAPPL_LOGLEVEL_DEBUG, "Scanner '%s': scan-color-mode-default=%s(0x%04x)", scanner->name, scan_color_mode_string(data->color_default), data->color_default);
  }

  // Validate intent default...
  if (!(data->intent_default & driver_data->intents_supported))
  {
    papplLog(scanner->system, PAPPL_LOGLEVEL_ERROR, "Scanner '%s': Unsupported scan-intent-default=0x%04x", scanner->name, data->intent_default);
    ret = false;
  }
  else
  {
    papplLog(scanner->system, PAPPL_LOGLEVEL_DEBUG, "Scanner '%s': scan-intent-default=%s(0x%04x)", scanner->name, scan_intent_string(data->intent_default), data->intent_default);
  }

  // Validate input source default...
  if (!(data->input_source_default & driver_data->input_sources_supported))
  {
    papplLog(scanner->system, PAPPL_LOGLEVEL_ERROR, "Scanner '%s': Unsupported input-source-default=0x%04x", scanner->name, data->input_source_default);
    ret = false;
  }
  else
  {
    papplLog(scanner->system, PAPPL_LOGLEVEL_DEBUG, "Scanner '%s': input-source-default=%s(0x%04x)", scanner->name, scan_input_source_string(data->input_source_default), data->input_source_default);
  }

  // Validate content type default...
  if (data->content_default && !(data->content_default & driver_data->content_supported))
  {
    papplLog(scanner->system, PAPPL_LOGLEVEL_ERROR, "Scanner '%s': Unsupported scan-content-type-default=0x%04x", scanner->name, data->content_default);
    ret = false;
  }
  else
  {
    papplLog(scanner->system, PAPPL_LOGLEVEL_DEBUG, "Scanner '%s': scan-content-type-default=%s(0x%04x)", scanner->name, scan_content_string(data->content_default), data->content_default);
  }

  // Validate resolution default...
  for (i = 0; i < driver_data->num_resolution; i ++)
  {
    if (data->x_default == driver_data->x_resolution[i] && data->y_default == driver_data->y_resolution[i])
      break;
  }

  if (i >= driver_data->num_resolution && driver_data->num_resolution > 0)
  {
    papplLog(scanner->system, PAPPL_LOGLEVEL_ERROR, "Scanner '%s': Unsupported scanner-resolution-default=%dx%ddpi", scanner->name, data->x_default, data->y_default);
    ret = false;
  }
  else
  {
    papplLog(scanner->system, PAPPL_LOGLEVEL_DEBUG, "Scanner '%s': scanner-resolution-default=%dx%ddpi", scanner->name, data->x_default, data->y_default);
  }

  return (ret);
}


//
// 'validate_driver()' - Validate the scanner driver-specific values.
//

static bool				// O - `true` if valid, `false` otherwise
validate_driver(
    pappl_scanner_t        *scanner,	// I - Scanner
    pappl_sc_driver_data_t *data)	// I - Driver values
{
  bool		ret = true;		// Return value
  size_t	i,			// Looping var
		num_icons;		// Number of scanner icons
  static const char * const icon_sizes[] =
  {					// Icon sizes
    "small-48x48",
    "medium-128x128",
    "large-512x512"
  };


  // Validate all driver fields and show debug/warning/fatal errors...
  if (data->extension)
    papplLog(scanner->system, PAPPL_LOGLEVEL_DEBUG, "Scanner '%s': Driver uses extension data (%p) and %sdelete function.", scanner->name, data->extension, data->delete_cb ? "" : "no ");

  // Verify required raster scanning callbacks...
  if (!data->rstartjob_cb || !data->rstartpage_cb || !data->rreadline_cb || !data->rendpage_cb || !data->rendjob_cb)
  {
    papplLog(scanner->system, PAPPL_LOGLEVEL_ERROR, "Scanner '%s': Driver does not provide required raster scanning callbacks.", scanner->name);
    ret = false;
  }

  if (!data->status_cb)
    papplLog(scanner->system, PAPPL_LOGLEVEL_WARN, "Scanner '%s': Driver does not support status updates.", scanner->name);

  // Verify make and model...
  if (!data->make_and_model[0])
  {
    papplLog(scanner->system, PAPPL_LOGLEVEL_ERROR, "Scanner '%s': Driver does not provide a make_and_model string.", scanner->name);
    ret = false;
  }

  // Verify icons...
  for (i = 0, num_icons = 0; i < 3; i ++)
  {
    if (data->icons[i].filename[0])
    {
      papplLog(scanner->system, PAPPL_LOGLEVEL_DEBUG, "Scanner '%s': Driver provides %s icon in file '%s'.", scanner->name, icon_sizes[i], data->icons[i].filename);
      num_icons ++;
    }
    else if (data->icons[i].data && data->icons[i].datalen > 0)
    {
      papplLog(scanner->system, PAPPL_LOGLEVEL_DEBUG, "Scanner '%s': Driver provides %s icon in memory (%u bytes).", scanner->name, icon_sizes[i], (unsigned)data->icons[i].datalen);
      num_icons ++;
    }
  }

  if (num_icons == 0)
    papplLog(scanner->system, PAPPL_LOGLEVEL_WARN, "Scanner '%s': Driver does not provide scanner icons, using defaults.", scanner->name);

  // Verify at least one document format...
  if (data->num_format == 0)
  {
    papplLog(scanner->system, PAPPL_LOGLEVEL_ERROR, "Scanner '%s': Driver does not provide any document formats.", scanner->name);
    ret = false;
  }
  else
  {
    papplLog(scanner->system, PAPPL_LOGLEVEL_DEBUG, "Scanner '%s': Driver supports %u document format(s).", scanner->name, (unsigned)data->num_format);
  }

  // Verify at least one resolution...
  if (data->num_resolution == 0)
  {
    papplLog(scanner->system, PAPPL_LOGLEVEL_ERROR, "Scanner '%s': Driver does not provide any scan resolutions.", scanner->name);
    ret = false;
  }
  else
  {
    for (i = 0; i < data->num_resolution; i ++)
    {
      if (data->x_resolution[i] <= 0 || data->y_resolution[i] <= 0)
      {
        papplLog(scanner->system, PAPPL_LOGLEVEL_ERROR, "Scanner '%s': Invalid scan resolution %dx%ddpi.", scanner->name, data->x_resolution[i], data->y_resolution[i]);
        ret = false;
      }
    }
  }

  // Verify at least one color mode...
  if (!data->color_supported)
  {
    papplLog(scanner->system, PAPPL_LOGLEVEL_ERROR, "Scanner '%s': Driver does not provide any color modes.", scanner->name);
    ret = false;
  }

  // Verify at least one input source...
  if (!data->input_sources_supported)
  {
    papplLog(scanner->system, PAPPL_LOGLEVEL_ERROR, "Scanner '%s': Driver does not provide any input sources.", scanner->name);
    ret = false;
  }

  // Verify scan area dimensions for platen...
  if (data->input_sources_supported & PAPPL_SCAN_INPUT_SOURCE_PLATEN)
  {
    if (data->platen_max_width <= 0 || data->platen_max_height <= 0)
    {
      papplLog(scanner->system, PAPPL_LOGLEVEL_ERROR, "Scanner '%s': Invalid platen scan area (%dx%d).", scanner->name, data->platen_max_width, data->platen_max_height);
      ret = false;
    }
    else
    {
      papplLog(scanner->system, PAPPL_LOGLEVEL_DEBUG, "Scanner '%s': Platen area %dx%d (1/300\").", scanner->name, data->platen_max_width, data->platen_max_height);
    }
  }

  // Verify scan area dimensions for ADF...
  if (data->input_sources_supported & PAPPL_SCAN_INPUT_SOURCE_ADF)
  {
    if (data->adf_max_width <= 0 || data->adf_max_height <= 0)
    {
      papplLog(scanner->system, PAPPL_LOGLEVEL_ERROR, "Scanner '%s': Invalid ADF scan area (%dx%d).", scanner->name, data->adf_max_width, data->adf_max_height);
      ret = false;
    }
    else
    {
      papplLog(scanner->system, PAPPL_LOGLEVEL_DEBUG, "Scanner '%s': ADF area %dx%d (1/300\").", scanner->name, data->adf_max_width, data->adf_max_height);
    }
  }

  return (ret);
}
