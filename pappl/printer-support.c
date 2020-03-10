//
// Printer support functions for the Printer Application Framework
//
// Copyright © 2020 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

//
// Include necessary headers...
//

#include "printer-private.h"


//
// Local functions...
//

static const char	*pappl_lookup_string(unsigned bit, size_t num_strings, const char * const *strings);
static unsigned		pappl_lookup_value(const char *keyword, size_t num_strings, const char * const *strings);

#define PAPPL_LOOKUP_STRING(bit,strings) pappl_lookup_string(bit, sizeof(strings) / sizeof(strings[0]), strings)
#define PAPPL_LOOKUP_VALUE(keyword,strings) pappl_lookup_value(keyword, sizeof(strings) / sizeof(strings[0]), strings)


//
// Local globals...
//

static const char * const pappl_color_modes[] =
{
  "auto",
  "auto-monochrome",
  "bi-level",
  "color",
  "monochrome",
  "process-monochrome"
};

static const char * const pappl_label_modes[] =
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

static const char * const pappl_cups_marker_types[] =
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
  "water"
};

static const char * const pappl_media_trackings[] =
{
  "continuous",
  "mark",
  "web"
};

static const char * const pappl_preasons[] =
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
  "spool-area-full",
  "toner-empty",
  "toner-low"
};

static const char * const pappl_raster_types[] =
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

static const char * const pappl_supply_colors[] =
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
  "yellow"
};

static const char * const pappl_supply_types[] =
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
  "water"
};


//
// '_papplColorModeString()' - Return the keyword value associated with the IPP "print-color-mode" bit value.
//

const char *				// O - IPP "print-color-mode" keyword value
_papplColorModeString(
    pappl_color_mode_t value)		// I - IPP "print-color-mode" bit value
{
  return (PAPPL_LOOKUP_STRING(value, pappl_color_modes));
}


//
// '_papplColorModeValue()' - Return the bit value associated with the IPP "print-color-mode" keyword value.
//

pappl_color_mode_t			// O - IPP "print-color-mode" bit value
_papplColorModeValue(const char *value)	// I - IPP "print-color-mode" keyword value
{
  return ((pappl_color_mode_t)PAPPL_LOOKUP_VALUE(value, pappl_color_modes));
}


//
// '_papplCreateMediaSize()' - Create a media-size collection.
//

ipp_t *					// O - Collection value
_papplCreateMediaSize(
    const char *size_name)		// I - Media size name
{
  ipp_t		*col = ippNew();	// Collection value
  pwg_media_t	*pwg = pwgMediaForPWG(size_name);
					// Size information

  ippAddInteger(col, IPP_TAG_ZERO, IPP_TAG_INTEGER, "x-dimension", pwg->width);
  ippAddInteger(col, IPP_TAG_ZERO, IPP_TAG_INTEGER, "y-dimension", pwg->length);

  return (col);
}


//
// '_papplLabelModeString()' - Return the keyword value associated with the IPP "label-mode-xxx" git value.
//

const char *				// O - IPP "label-mode-xxx" keyword value
_papplLabelModeString(
    pappl_label_mode_t v)		// I - IPP "label-mode-xxx" bit value
{
  switch (v)
  {
    case PAPPL_LABEL_MODE_APPLICATOR :
        return ("applicator");

    case PAPPL_LABEL_MODE_CUTTER :
        return ("cutter");

    case PAPPL_LABEL_MODE_CUTTER_DELAYED :
        return ("cutter-delayed");

    case PAPPL_LABEL_MODE_KIOSK :
        return ("kiosk");

    case PAPPL_LABEL_MODE_PEEL_OFF :
        return ("peel-off");

    case PAPPL_LABEL_MODE_PEEL_OFF_PREPEEL :
        return ("peel-off-prepeel");

    case PAPPL_LABEL_MODE_REWIND :
        return ("rewind");

    case PAPPL_LABEL_MODE_RFID :
        return ("rfid");

    case PAPPL_LABEL_MODE_TEAR_OFF :
        return ("tear-off");

    default :
        return ("unknown");
  }
}


//
// '_papplLabelModeValue()' - Return the bit value associated with the IPP "label-mode-xxx" keyword value.
//

pappl_label_mode_t			// O - IPP "label-mode-xxx" bit value
_papplLabelModeValue(const char *s)	// I - IPP "label-mode-xxx" keyword value
{
  if (!strcmp(s, "applicator"))
    return (PAPPL_LABEL_MODE_APPLICATOR);
  else if (!strcmp(s, "cutter"))
    return (PAPPL_LABEL_MODE_CUTTER);
  else if (!strcmp(s, "cutter-delayed"))
    return (PAPPL_LABEL_MODE_CUTTER_DELAYED);
  else if (!strcmp(s, "kiosk"))
    return (PAPPL_LABEL_MODE_KIOSK);
  else if (!strcmp(s, "peel-off"))
    return (PAPPL_LABEL_MODE_PEEL_OFF);
  else if (!strcmp(s, "peel-off-prepeel"))
    return (PAPPL_LABEL_MODE_PEEL_OFF_PREPEEL);
  else if (!strcmp(s, "rewind"))
    return (PAPPL_LABEL_MODE_REWIND);
  else if (!strcmp(s, "rfid"))
    return (PAPPL_LABEL_MODE_RFID);
  else if (!strcmp(s, "tear-off"))
    return (PAPPL_LABEL_MODE_TEAR_OFF);
  else
    return (0);
}


//
// '_papplMediaColExport()' - Convert media values to a collection value.
//

ipp_t *					// O - IPP "media-col" value
_papplMediaColExport(
    pappl_media_col_t *media,		// I - Media values
    bool              db)		// I - Create a "media-col-database" value?
{
  ipp_t		*col = ippNew(),	// Collection value
		*size = _papplCreateMediaSize(media->size_name);
					// media-size value


  ippAddInteger(col, IPP_TAG_ZERO, IPP_TAG_INTEGER, "media-bottom-margin", media->bottom_margin);
  ippAddInteger(col, IPP_TAG_ZERO, IPP_TAG_INTEGER, "media-left-margin", media->left_margin);
  ippAddInteger(col, IPP_TAG_ZERO, IPP_TAG_INTEGER, "media-right-margin", media->right_margin);
  ippAddCollection(col, IPP_TAG_ZERO, "media-size", size);
  ippDelete(size);
  ippAddString(col, IPP_TAG_ZERO, IPP_TAG_KEYWORD, "media-size-name", NULL, media->size_name);
  if (media->source[0])
    ippAddString(col, IPP_TAG_ZERO, IPP_TAG_KEYWORD, "media-source", NULL, media->source);
  ippAddInteger(col, IPP_TAG_ZERO, IPP_TAG_INTEGER, "media-top-margin", media->top_margin);
  if (!db)
    ippAddInteger(col, IPP_TAG_ZERO, IPP_TAG_INTEGER, "media-top-offset", media->top_offset);
  if (media->tracking)
    ippAddString(col, IPP_TAG_ZERO, IPP_CONST_TAG(IPP_TAG_KEYWORD), "media-tracking", NULL, _papplMediaTrackingString(media->tracking));
  if (media->type[0])
    ippAddString(col, IPP_TAG_ZERO, IPP_TAG_KEYWORD, "media-type", NULL, media->type);

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

    strlcpy(media->size_name, pwg_name, sizeof(media->size_name));
    media->size_width  = pwg_media->width;
    media->size_length = pwg_media->length;
  }
  else if (x_dimension && y_dimension)
  {
    pwg_media_t	*pwg_media = pwgMediaForSize(ippGetInteger(x_dimension, 0), ippGetInteger(y_dimension, 0));

    strlcpy(media->size_name, pwg_media->pwg, sizeof(media->size_name));
    media->size_width  = pwg_media->width;
    media->size_length = pwg_media->length;
  }

  if (bottom_margin)
    media->bottom_margin = ippGetInteger(top_offset, 0);
  if (left_margin)
    media->left_margin = ippGetInteger(top_offset, 0);
  if (right_margin)
    media->right_margin = ippGetInteger(top_offset, 0);
  if (source)
    strlcpy(media->source, ippGetString(source, 0, NULL), sizeof(media->source));
  if (top_margin)
    media->top_margin = ippGetInteger(top_offset, 0);
  if (top_offset)
    media->top_offset = ippGetInteger(top_offset, 0);
  if (tracking)
    media->tracking = _papplMediaTrackingValue(ippGetString(tracking, 0, NULL));
  if (type)
    strlcpy(media->type, ippGetString(type, 0, NULL), sizeof(media->type));
}



//
// '_papplMediaTrackingString()' - Return the keyword value associated with the IPP "media-tracking" bit value.
//

const char *				// O - IPP "media-tracking" keyword value
_papplMediaTrackingString(
    pappl_media_tracking_t v)		// I - IPP "media-tracking" bit value
{
  if (v == PAPPL_MEDIA_TRACKING_CONTINUOUS)
    return ("continuous");
  else if (v == PAPPL_MEDIA_TRACKING_MARK)
    return ("mark");
  else if (v == PAPPL_MEDIA_TRACKING_WEB)
    return ("web");
  else
    return ("unknown");
}


//
// '_papplMediaTrackingValue()' - Return the bit value associated with the IPP "media-tracking" keyword value.
//

pappl_media_tracking_t			// O - IPP "media-tracking" bit value
_papplMediaTrackingValue(const char *s)	// I - IPP "media-tracking" keyword value
{
  if (!strcmp(s, "continuous"))
    return (PAPPL_MEDIA_TRACKING_CONTINUOUS);
  else if (!strcmp(s, "mark"))
    return (PAPPL_MEDIA_TRACKING_MARK);
  else if (!strcmp(s, "web"))
    return (PAPPL_MEDIA_TRACKING_WEB);
  else
    return (0);
}


//
// '_papplPrinterReasonString()' - Return the keyword value associated with the IPP "printer-state-reasons" bit value.
//

const char *				// O - IPP "printer-state-reasons" keyword value
_papplPrinterReasonString(
    pappl_preason_t value)		// I - IPP 'printer-state-reasons" bit value
{
  if (value == PAPPL_PREASON_NONE)
    return ("none");
  else
    return (PAPPL_LOOKUP_STRING(value, pappl_preasons));
}


//
// '_papplPrinterReasonValue()' - Return the bit value associated with the IPP "printer-state-reasons" keyword value.
//

pappl_preason_t				// O - IPP 'printer-state-reasons" bit value
_papplPrinterReasonValue(
    const char *value)			// I - IPP 'printer-state-reasons" keyword value
{
  return ((pappl_preason_t)PAPPL_LOOKUP_VALUE(value, pappl_preasons));
}



//
// '_pappl()' .
//

const char	*_papplRasterTypeString(pappl_raster_type_t value)
{
}


//
// '_pappl()' .
//

pappl_raster_type_t _papplRasterTypeValue(const char *value)
{
}



//
// '_pappl()' .
//

const char	*_papplSupplyColorString(pappl_supply_color_t value)
{
}


//
// '_pappl()' .
//

pappl_supply_color_t _papplSupplyColorValue(const char *value)
{
}



//
// '_pappl()' .
//

const char	*_papplSupplyTypeString(pappl_supply_type_t value)
{
}


//
// '_pappl()' .
//

pappl_supply_type_t _papplSupplyTypeValue(const char *value)
{
}


//
// 'pappl_lookup_string()' - Lookup the string value for a bit.
//

static const char *			// O - Keyword or `NULL`
pappl_lookup_string(
    unsigned           value,		// I - Bit value
    size_t             num_strings,	// I - Number of strings
    const char * const *strings)	// I - Strings
{
  size_t	i;			// Looking var
  unsigned	bit;			// Current bit


  for (i = 0, bit = 1; i < num_strings; i ++, bit *= 2)
  {
    if (bit == value)
      return (strings[i]);
  }

  return (NULL);
}


//
// 'pappl_lookup_value()' - Lookup the bit value for a string.
//

static unsigned				// O - Bit value or `0`
pappl_lookup_value(
    const char         *value,		// I - Keyword value
    size_t             num_strings,	// I - Number of strings
    const char * const *strings)	// I - Strings
{
  size_t	i;			// Looking var
  unsigned	bit;			// Current bit


  for (i = 0, bit = 1; i < num_strings; i ++, bit *= 2)
  {
    if (!strcmp(strings[i], value))
      return (bit);
  }

  return (0);
}
