//
// Printer support functions for the Printer Application Framework
//
// Copyright © 2020-2022 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

//
// Include necessary headers...
//

#include "pappl-private.h"


const char * const _pappl_color_modes[] =
{
  "auto",
  "auto-monochrome",
  "bi-level",
  "color",
  "monochrome",
  "process-monochrome"
};

const char * const _pappl_contents[] =
{
  "auto",
  "graphic",
  "photo",
  "text",
  "text-and-graphic"
};

const char * const _pappl_identify_actions[] =
{
  "display",
  "flash",
  "sound",
  "speak"
};

const char * const _pappl_job_password_repertoires[] =
{
  "iana_us-ascii_digits",
  "iana_us-ascii_letters",
  "iana_us-ascii_complex",
  "iana_us-ascii_any",
  "iana_utf-8_digits",
  "iana_utf-8_letters",
  "iana_utf-8_any",
  "vendor_vendor"
};

const char * const _pappl_kinds[] =
{
  "disc",
  "document",
  "envelope",
  "label",
  "large-format",
  "photo",
  "postcard",
  "receipt",
  "roll"
};

const char * const _pappl_label_modes[] =
{
  "applicator",
  "cutter",
  "cutter-delayed",
  "kiosk",
  "peel-off",
  "peel-off-prepeel",
  "rewind",
  "rfid",
  "tear-off"
};

const char * const _pappl_marker_colors[] =
{
  "#777777",
  "#000000",
  "#00FFFF",
  "#777777",
  "#00CC00",
  "#77FFFF",
  "#CCCCCC",
  "#FFCCFF",
  "#FF00FF",
  "#FF7700",
  "#770077",
  "#FFFF00",
  "#00FFFF#FF00FF#FFFF00"
};

const char * const _pappl_marker_types[] =
{
  "banding-supply",
  "binding-supply",
  "cleaner-unit",
  "corona-wire",
  "covers",
  "developer",
  "fuser-cleaning-pad",
  "fuser-oil-wick",
  "fuser-oil",
  "fuser-oiler",
  "fuser",
  "ink-cartridge",
  "ink-ribbon",
  "ink",
  "inserts",
  "opc",
  "paper-wrap",
  "ribbon-wax",
  "solid-wax",
  "staples",
  "stitching-wire",
  "toner-cartridge",
  "toner",
  "transfer-unit",
  "waste-ink",
  "waste-toner",
  "waste-water",
  "waste-wax",
  "water",
  "glue-water-additive",
  "waste-paper",
  "shrink-wrap",
  "other",
  "unknown"
};

const char * const _pappl_media_trackings[] =
{
  "continuous",
  "gap",
  "mark",
  "web"
};

const char * const _pappl_preasons[] =
{
  "other",
  "cover-open",
  "input-tray-missing",
  "marker-supply-empty",
  "marker-supply-low",
  "marker-waste-almost-full",
  "marker-waste-full",
  "media-empty",
  "media-jam",
  "media-low",
  "media-needed",
  "offline",
  "spool-area-full",
  "toner-empty",
  "toner-low",
  "door-open",
  "identify-printer-requested"
};

const char * const _pappl_raster_types[] =
{
  "adobe-rgb_8",
  "adobe-rgb_16",
  "black_1",
  "black_8",
  "black_16",
  "cmyk_8",
  "cmyk_8",
  "rgb_8",
  "rgb_16",
  "sgray_8",
  "sgray_16",
  "srgb_8",
  "srgb_16"
};

const char * const _pappl_release_actions[] =
{
  "none",
  "button-press",
  "job-password",
  "owner-authorized"
};

const char * const _pappl_scalings[] =
{
  "auto",
  "auto-fit",
  "fill",
  "fit",
  "none"
};

const char * const _pappl_sides[] =
{
  "one-sided",
  "two-sided-long-edge",
  "two-sided-short-edge"
};

const char * const _pappl_st_access[] =
{
  "group",
  "owner",
  "public"
};

const char * const _pappl_st_disposition[] =
{
  "print-and-store",
  "store-only"
};

const char * const _pappl_supply_colors[] =
{
  "no-color",
  "black",
  "cyan",
  "gray",
  "green",
  "light-cyan",
  "light-gray",
  "light-magenta",
  "magenta",
  "orange",
  "violet",
  "yellow",
  "multi-color"
};

const char * const _pappl_supply_types[] =
{
  "bandingSupply",
  "bindingSupply",
  "cleanerUnit",
  "coronaWire",
  "covers",
  "developer",
  "fuserCleaningPad",
  "fuserOilWick",
  "fuserOil",
  "fuserOiler",
  "fuser",
  "inkCartridge",
  "inkRibbon",
  "ink",
  "inserts",
  "opc",
  "paperWrap",
  "ribbonWax",
  "solidWax",
  "staples",
  "stitchingWire",
  "tonerCartridge",
  "toner",
  "transferUnit",
  "wasteInk",
  "wasteToner",
  "wasteWater",
  "wasteWax",
  "water",
  "glueWaterAdditive",
  "wastePaper",
  "shrinkWrap",
  "other",
  "unknown"
};

const char * const _pappl_which_jobs[] =
{
  "aborted",
  "all",
  "canceled",
  "completed",
  "fetchable",
  "not-completed",
  "pending",
  "pending-held",
  "processing",
  "processing-stopped",
  "proof-and-suspend",
  "proof-print",
  "stored-group",
  "stored-owner",
  "stored-public",
  "saved"
};

//
// '_papplColorModeString()' - Return the keyword value associated with the IPP "print-color-mode" bit value.
//

const char *				// O - IPP "print-color-mode" keyword value
_papplColorModeString(
    pappl_color_mode_t value)		// I - IPP "print-color-mode" bit value
{
  return (_PAPPL_LOOKUP_STRING(value, _pappl_color_modes));
}


//
// '_papplColorModeValue()' - Return the bit value associated with the IPP "print-color-mode" keyword value.
//

pappl_color_mode_t			// O - IPP "print-color-mode" bit value
_papplColorModeValue(const char *value)	// I - IPP "print-color-mode" keyword value
{
  return ((pappl_color_mode_t)_PAPPL_LOOKUP_VALUE(value, _pappl_color_modes));
}


//
// '_papplContentString()' - Return the keyword associated with an IPP "print-content-optimize" bit value.
//

const char *				// O - IPP "print-content-optimize" keyword value
_papplContentString(
    pappl_content_t value)		// I - IPP "print-content-optimize" bit value
{
  return (_PAPPL_LOOKUP_STRING(value, _pappl_contents));
}


//
// '_papplContentValue()' - Return the bit value associated with an IPP "print-content-optimize" keyword value.
//

pappl_content_t				// O - IPP "print-content-optimize" bit value
_papplContentValue(const char *value)	// I - IPP "print-content-optimize" keyword value
{
  return ((pappl_content_t)_PAPPL_LOOKUP_VALUE(value, _pappl_contents));
}


//
// '_papplCreateMediaSize()' - Create a media-size collection.
//

ipp_t *					// O - Collection value
_papplCreateMediaSize(
    const char *size_name)		// I - Media size name
{
  pwg_media_t	*pwg = pwgMediaForPWG(size_name);
					// Size information

  if (pwg)
  {
    ipp_t *col = ippNew();		// Collection value

    ippAddInteger(col, IPP_TAG_ZERO, IPP_TAG_INTEGER, "x-dimension", pwg->width);
    ippAddInteger(col, IPP_TAG_ZERO, IPP_TAG_INTEGER, "y-dimension", pwg->length);

    return (col);
  }
  else
  {
    return (NULL);
  }
}


//
// '_papplIdentifyActionsString()' - Return the keyword value associated with the IPP "identify-actions" bit value.
//

const char *				// O - IPP "identify-actions" keyword value
_papplIdentifyActionsString(
    pappl_identify_actions_t v)		// I - IPP "identify-actions" bit value
{
  return (_PAPPL_LOOKUP_STRING(v, _pappl_identify_actions));
}


//
// '_papplIdentifyActionsValue()' - Return the bit value associated with the IPP "identify-actions" keyword value.
//

pappl_identify_actions_t		// O - IPP "identify-actions" bit value
_papplIdentifyActionsValue(
    const char *s)			// I - IPP "identify-actions" keyword value
{
  return ((pappl_identify_actions_t)_PAPPL_LOOKUP_VALUE(s, _pappl_identify_actions));
}


//
// '_papplKindString()' - Return the keyword value associated with the IPP "printer-kind" bit value.
//

const char *				// O - IPP "printer-kind" keyword value
_papplKindString(
    pappl_kind_t value)			// I - IPP "printer-kind" bit value
{
  return (_PAPPL_LOOKUP_STRING(value, _pappl_kinds));
}


//
// '_papplLabelModeString()' - Return the keyword value associated with the IPP "label-mode-xxx" bit value.
//

const char *				// O - IPP "label-mode-xxx" keyword value
_papplLabelModeString(
    pappl_label_mode_t value)		// I - IPP "label-mode-xxx" bit value
{
  return (_PAPPL_LOOKUP_STRING(value, _pappl_label_modes));
}


//
// '_papplLabelModeValue()' - Return the bit value associated with the IPP "label-mode-xxx" keyword value.
//

pappl_label_mode_t			// O - IPP "label-mode-xxx" bit value
_papplLabelModeValue(const char *value)	// I - IPP "label-mode-xxx" keyword value
{
  return ((pappl_label_mode_t)_PAPPL_LOOKUP_VALUE(value, _pappl_label_modes));
}


//
// '_papplMarkerColorString()' - Return the IPP "marker-colors" name string associated with the supply color enumeration value.
//

const char *				// O - IPP "marker-colors" name string
_papplMarkerColorString(
    pappl_supply_color_t value)		// I - Supply color enumeration value
{
  if (value >= PAPPL_SUPPLY_COLOR_NO_COLOR && value <= PAPPL_SUPPLY_COLOR_YELLOW)
    return (_pappl_marker_colors[(int)value]);
  else
    return ("unknown");
}


//
// '_papplMarkerTypeString()' - Return the IPP "marker-types" keyword associated with the supply type enumeration value.
//

const char *				// O - IPP "marker-types" keyword
_papplMarkerTypeString(
    pappl_supply_type_t value)		// I - Supply type enumeration value
{
  if (value >= PAPPL_SUPPLY_TYPE_BANDING_SUPPLY && value <= PAPPL_SUPPLY_TYPE_UNKNOWN)
    return (_pappl_marker_types[(int)value]);
  else
    return ("unknown");
}


//
// '_papplMediaColExport()' - Convert media values to a collection value.
//

ipp_t *					// O - IPP "media-col" value
_papplMediaColExport(
    pappl_pr_driver_data_t *driver_data,// I - Driver data
    pappl_media_col_t      *media,	// I - Media values
    bool                   db)		// I - Create a "media-col-database" value?
{
  ipp_t		*col = NULL,		// Collection value
		*size = _papplCreateMediaSize(media->size_name);
					// media-size value


  if (size)
  {
    col = ippNew();

    ippAddInteger(col, IPP_TAG_ZERO, IPP_TAG_INTEGER, "media-bottom-margin", media->bottom_margin);
    ippAddInteger(col, IPP_TAG_ZERO, IPP_TAG_INTEGER, "media-left-margin", media->left_margin);
    if (driver_data->left_offset_supported[1] && !db)
      ippAddInteger(col, IPP_TAG_ZERO, IPP_TAG_INTEGER, "media-left-offset", media->left_offset);
    ippAddInteger(col, IPP_TAG_ZERO, IPP_TAG_INTEGER, "media-right-margin", media->right_margin);
    ippAddCollection(col, IPP_TAG_ZERO, "media-size", size);
    ippDelete(size);
    ippAddString(col, IPP_TAG_ZERO, IPP_TAG_KEYWORD, "media-size-name", NULL, media->size_name);
    if (driver_data->num_source > 0 && media->source[0])
      ippAddString(col, IPP_TAG_ZERO, IPP_TAG_KEYWORD, "media-source", NULL, media->source);
    ippAddInteger(col, IPP_TAG_ZERO, IPP_TAG_INTEGER, "media-top-margin", media->top_margin);
    if (driver_data->top_offset_supported[1] && !db)
      ippAddInteger(col, IPP_TAG_ZERO, IPP_TAG_INTEGER, "media-top-offset", media->top_offset);
    if (driver_data->tracking_supported && media->tracking)
      ippAddString(col, IPP_TAG_ZERO, IPP_CONST_TAG(IPP_TAG_KEYWORD), "media-tracking", NULL, _papplMediaTrackingString(media->tracking));
    if (driver_data->num_type > 0 && media->type[0])
      ippAddString(col, IPP_TAG_ZERO, IPP_TAG_KEYWORD, "media-type", NULL, media->type);
  }

  return (col);
}


//
// '_papplMediaColImport()' - Convert a collection value to media values.
//

void
_papplMediaColImport(
    ipp_t             *col,		// I - IPP "media-col" value
    pappl_media_col_t *media)		// O - Media values
{
  ipp_attribute_t	*size_name = ippFindAttribute(col, "media-size-name", IPP_TAG_ZERO),
			*x_dimension = ippFindAttribute(col, "media-size/x-dimension", IPP_TAG_INTEGER),
			*y_dimension = ippFindAttribute(col, "media-size/y-dimension", IPP_TAG_INTEGER),
			*bottom_margin = ippFindAttribute(col, "media-bottom-margin", IPP_TAG_INTEGER),
			*left_margin = ippFindAttribute(col, "media-left-margin", IPP_TAG_INTEGER),
			*left_offset = ippFindAttribute(col, "media-left-offset", IPP_TAG_INTEGER),
			*right_margin = ippFindAttribute(col, "media-right-margin", IPP_TAG_INTEGER),
			*source = ippFindAttribute(col, "media-source", IPP_TAG_ZERO),
			*top_margin = ippFindAttribute(col, "media-top-margin", IPP_TAG_INTEGER),
			*top_offset = ippFindAttribute(col, "media-top-offset", IPP_TAG_INTEGER),
			*tracking = ippFindAttribute(col, "media-tracking", IPP_TAG_ZERO),
			*type = ippFindAttribute(col, "media-type", IPP_TAG_ZERO);


  if (size_name)
  {
    const char	*pwg_name = ippGetString(size_name, 0, NULL);
    pwg_media_t	*pwg_media = pwgMediaForPWG(pwg_name);

    papplCopyString(media->size_name, pwg_name, sizeof(media->size_name));
    media->size_width  = pwg_media->width;
    media->size_length = pwg_media->length;
  }
  else if (x_dimension && y_dimension)
  {
    pwg_media_t	*pwg_media = pwgMediaForSize(ippGetInteger(x_dimension, 0), ippGetInteger(y_dimension, 0));

    papplCopyString(media->size_name, pwg_media->pwg, sizeof(media->size_name));
    media->size_width  = pwg_media->width;
    media->size_length = pwg_media->length;
  }

  if (bottom_margin)
    media->bottom_margin = ippGetInteger(bottom_margin, 0);
  if (left_margin)
    media->left_margin = ippGetInteger(left_margin, 0);
  if (left_offset)
    media->left_offset = ippGetInteger(left_offset, 0);
  if (right_margin)
    media->right_margin = ippGetInteger(right_margin, 0);
  if (source)
    papplCopyString(media->source, ippGetString(source, 0, NULL), sizeof(media->source));
  if (top_margin)
    media->top_margin = ippGetInteger(top_margin, 0);
  if (top_offset)
    media->top_offset = ippGetInteger(top_offset, 0);
  if (tracking)
    media->tracking = _papplMediaTrackingValue(ippGetString(tracking, 0, NULL));
  if (type)
    papplCopyString(media->type, ippGetString(type, 0, NULL), sizeof(media->type));
}



//
// '_papplMediaTrackingString()' - Return the keyword value associated with the IPP "media-tracking" bit value.
//

const char *				// O - IPP "media-tracking" keyword value
_papplMediaTrackingString(
    pappl_media_tracking_t value)	// I - IPP "media-tracking" bit value
{
  return (_PAPPL_LOOKUP_STRING(value, _pappl_media_trackings));
}


//
// '_papplMediaTrackingValue()' - Return the bit value associated with the IPP "media-tracking" keyword value.
//

pappl_media_tracking_t			// O - IPP "media-tracking" bit value
_papplMediaTrackingValue(
    const char *value)			// I - IPP "media-tracking" keyword value
{
  return ((pappl_media_tracking_t)_PAPPL_LOOKUP_VALUE(value, _pappl_media_trackings));
}


//
// '_papplPasswordRepertoireString()' - Return the keyword value associated with the IPP "job-password-repertoire" bit value.
//

const char *        // O - IPP "job-password-repertoire-configured" keyword value
_papplPasswordRepertoireString(
    pappl_pw_repertoire_t value)    // I - IPP "job-password-repertoire-configured" bit value
{
  return (_PAPPL_LOOKUP_STRING(value, _pappl_job_password_repertoires));
}


//
// '_papplPrinterReasonString()' - Return the keyword value associated with the IPP "printer-state-reasons" bit value.
//

const char *				// O - IPP "printer-state-reasons" keyword value
_papplPrinterReasonString(
    pappl_preason_t value)		// I - IPP "printer-state-reasons" bit value
{
  if (value == PAPPL_PREASON_NONE)
    return ("none");
  else
    return (_PAPPL_LOOKUP_STRING(value, _pappl_preasons));
}


//
// '_papplPrinterReasonValue()' - Return the bit value associated with the IPP "printer-state-reasons" keyword value.
//

pappl_preason_t				// O - IPP "printer-state-reasons" bit value
_papplPrinterReasonValue(
    const char *value)			// I - IPP "printer-state-reasons" keyword value
{
  return ((pappl_preason_t)_PAPPL_LOOKUP_VALUE(value, _pappl_preasons));
}



//
// '_papplRasterTypeString()' - Return the keyword associated with an IPP "pwg-raster-document-type-supported" bit value.
//

const char *				// O - IPP "pwg-raster-document-type-supported" keyword value
_papplRasterTypeString(
    pappl_raster_type_t value)		// I - IPP "pwg-raster-document-type-supported" bit value
{
  return (_PAPPL_LOOKUP_STRING(value, _pappl_raster_types));
}


//
// '_papplReleaseActionString()' - Return the keyword value associated with the IPP "job-release-action" bit value.
//

const char *        // O - IPP "job-release-action" keyword value
_papplReleaseActionString(
    pappl_release_action_t value)    // I - IPP "job-release-action-default" bit value
{
  return (_PAPPL_LOOKUP_STRING(value, _pappl_release_actions));
}


//
// '_papplScalingString()' - Return the keyword associated with an IPP "print-scaling" bit value.
//

const char *				// O - IPP "print-scaling" keyword value
_papplScalingString(
    pappl_scaling_t value)		// I - IPP "print-scaling" bit value
{
  return (_PAPPL_LOOKUP_STRING(value, _pappl_scalings));
}


//
// '_papplScalingValue()' - Return the bit value associated with an IPP "print-scaling" keyword value.
//

pappl_scaling_t				// O - IPP "print-scaling" bit value
_papplScalingValue(const char *value)	// I - IPP "print-scaling" keyword value
{
  return ((pappl_scaling_t)_PAPPL_LOOKUP_VALUE(value, _pappl_scalings));
}


//
// '_papplSidesString()' - Return the keyword associated with an IPP "sides" bit value.
//

const char *				// O - IPP "sides" keyword value
_papplSidesString(pappl_sides_t value)	// I - IPP "sides" bit value
{
  return (_PAPPL_LOOKUP_STRING(value, _pappl_sides));
}


//
// '_papplSidesValue()' - Return the bit value associated with an IPP "sides" keyword value.
//

pappl_sides_t				// O - IPP "sides" bit value
_papplSidesValue(const char *value)	// I - IPP "sides" keyword value
{
  return ((pappl_sides_t)_PAPPL_LOOKUP_VALUE(value, _pappl_sides));
}


//
// '_papplStorageAccessString()' - Return the keyword associated with an IPP "job-storage-access" bit value.
//

const char *                                            // O - IPP "job-storage-access" keyword value
_papplStorageAccessString(pappl_st_access_t value) // I - IPP "job-storage-access" bit value
{
  return (_PAPPL_LOOKUP_STRING(value, _pappl_st_access));
}


//
// '_papplStorageAccessValue()' - Return the enum value associated with an IPP "job-storage-access" keyword value.
//

pappl_st_access_t                      // O - IPP "job-storage-access" bit value
_papplStorageAccessValue(const char *value) // I - IPP "job-storage-access" keyword value
{
  return ((pappl_st_access_t)_PAPPL_LOOKUP_VALUE(value, _pappl_st_access));
}


//
// '_papplStorageDispositionString()' - Return the keyword associated with an IPP "job-storage-disposition" bit value.
//

const char *                                                      // O - IPP "job-storage-disposition" keyword value
_papplStorageDispositionString(pappl_st_disposition_t value) // I - IPP "job-storage-disposition" bit value
{
  return (_PAPPL_LOOKUP_STRING(value, _pappl_st_disposition));
}


//
// '_papplStorageDispositionValue()' - Return the enum value associated with an IPP "job-storage-disposition" keyword value.
//

pappl_st_disposition_t                            // O - IPP "job-storage-disposition" bit value
_papplStorageDispositionValue(const char *value)  // I - IPP "job-storage-disposition" keyword value
{
  return ((pappl_st_disposition_t)_PAPPL_LOOKUP_VALUE(value, _pappl_st_disposition));
}


//
// '_papplSupplyColorString()' - Return the IPP "printer-supply" color string associated with the supply color enumeration value.
//

const char *				// O - IPP "printer-supply" color string
_papplSupplyColorString(
    pappl_supply_color_t value)		// I - Supply color enumeration value
{
  if (value >= PAPPL_SUPPLY_COLOR_NO_COLOR && value <= PAPPL_SUPPLY_COLOR_MULTIPLE)
    return (_pappl_supply_colors[(int)value]);
  else
    return ("unknown");
}


//
// '_papplSupplyColorValue()' - Return the IPP "printer-supply" color enumeration value associated with the supply color string.
//

pappl_supply_color_t			// O - IPP "printer-supply" color enumeration value
_papplSupplyColorValue(
    const char *value)			// I - Supply color string
{
  int	i;				// Looping var


  for (i = 0; i < (int)(sizeof(_pappl_supply_colors) / sizeof(_pappl_supply_colors[0])); i ++)
  {
    if (!strcmp(value, _pappl_supply_colors[i]))
      return ((pappl_supply_color_t)i);
  }

  if (!strcmp(value, "color"))
    return (PAPPL_SUPPLY_COLOR_MULTIPLE);

  return (PAPPL_SUPPLY_COLOR_NO_COLOR);
}


//
// '_papplSupplyTypeString()' - Return the IPP "printer-supply" type string associated with the supply type enumeration value.
//

const char *				// O - IPP "printer-supply" type string
_papplSupplyTypeString(
    pappl_supply_type_t value)		// I - Supply type enumeration value
{
  if (value >= PAPPL_SUPPLY_TYPE_BANDING_SUPPLY && value <= PAPPL_SUPPLY_TYPE_UNKNOWN)
    return (_pappl_supply_types[(int)value]);
  else
    return ("unknown");
}


//
// '_papplSupplyColorValue()' - Return the IPP "printer-supply" color enumeration value associated with the supply color string.
//

pappl_supply_type_t			// O - IPP "printer-supply" type enumeration value
_papplSupplyTypeValue(
    const char *value)			// I - Supply type string
{
  int	i;				// Looping var


  for (i = 0; i < (int)(sizeof(_pappl_supply_types) / sizeof(_pappl_supply_types[0])); i ++)
  {
    if (!strcmp(value, _pappl_supply_types[i]))
      return ((pappl_supply_type_t)i);
  }

  return (PAPPL_SUPPLY_TYPE_UNKNOWN);
}
