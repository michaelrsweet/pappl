//
// Printer driver functions for the Printer Application Framework
//
// Copyright © 2020-2024 by Michael R Sweet.
//
// Licensed under Apache License v2.0. See the file "LICENSE" for more
// information.
//

#include "scanner-private.h"
#include "system-private.h"
#include <libxml/parser.h>
#include <libxml/tree.h>

//
// Local functions...
//
extern xmlDocPtr make_escl_attr(pappl_scanner_t *scanner);
extern const char *ScannerInputSourceString(pappl_sc_input_source_t value) _PAPPL_PRIVATE;
extern const char *ScannerResolutionString(int resolution) _PAPPL_PRIVATE;
extern const char *_papplScannerColorModeString(pappl_sc_color_mode_t value) _PAPPL_PRIVATE;

//
// 'papplScannerGetDriverData()' - Get the current scanner driver data.
//
// This function copies the current scanner driver data into the specified buffer.
//

pappl_sc_driver_data_t *           // O - Driver data or `NULL` if none
papplScannerGetDriverData(
  pappl_scanner_t        *scanner,  // I - Scanner
  pappl_sc_driver_data_t *data)     // I - Pointer to driver data structure to fill
{
  if (!scanner || !scanner->driver_name || !data)
  {
  if (data)
  _papplScannerInitDriverData(scanner, data);

  return (NULL);
  }

  memcpy(data, &scanner->driver_data, sizeof(pappl_sc_driver_data_t));

  return (data);
}


//
// 'papplScannerGetDriverName()' - Get the driver name for a scanner.
//
// This function returns the driver name for the scanner.
//

const char *                 // O - Driver name or `NULL` for none
papplScannerGetDriverName(
  pappl_scanner_t *scanner)  // I - Scanner
{
  return (scanner ? scanner->driver_name : NULL);
}

//
// 'papplScannerSetDriverData()' - Set the driver data.
//
// This function sets the driver data.
//

bool                           // O - `true` on success, `false` on failure
papplScannerSetDriverData(
  pappl_scanner_t        *scanner,  // I - Scanner
  pappl_sc_driver_data_t *data)     // I - Driver data
{
  if (!scanner || !data)
  return (false);

  // Note: For now, we assume the data is valid. We will add validation in later versions.

  _papplRWLockWrite(scanner);

  memcpy(&scanner->driver_data, data, sizeof(scanner->driver_data));

  scanner->config_time = time(NULL);

  _papplRWUnlock(scanner);

  return (true);
}

//
// 'papplScannerSetDriverDefaults()' - Set the default scan option values.
//
// This function validates and sets the scanner's default scan options.
//

bool                           // O - `true` on success, `false` on failure
papplScannerSetDriverDefaults(
  pappl_scanner_t        *scanner,  // I - Scanner
  pappl_sc_driver_data_t *data)     // I - Driver data
{
  if (!scanner || !data)
  return (false);

  // Note: For now, we assume the data is valid. We will add validation in later versions.

  _papplRWLockWrite(scanner);

  scanner->driver_data.default_color_mode     = data->default_color_mode;
  scanner->driver_data.default_resolution     = data->default_resolution;
  scanner->driver_data.default_input_source   = data->default_input_source;
  scanner->driver_data.default_media_type     = data->default_media_type;
  scanner->driver_data.default_document_format = data->default_document_format;
  scanner->driver_data.default_intent         = data->default_intent;
  scanner->driver_data.default_color_space    = data->default_color_space;

  scanner->config_time = time(NULL);

  _papplRWUnlock(scanner);

  return (true);
}

//
// _paplScannerInitDriverData() - Initialize the driver data.
//
// This function initializes the driver data through the callback function.

void
_papplScannerInitDriverData(
  pappl_scanner_t       *scanner, // I - Scanner
  pappl_sc_driver_data_t *d)      // I - Driver data
{
  memset(d, 0, sizeof(pappl_sc_driver_data_t));
  if (scanner->driver_data.capabilities_cb)
  {
  // Get the driver data from the callback function
  pappl_sc_driver_data_t callback_data = (scanner->driver_data.capabilities_cb)(scanner);
  *d = callback_data;
  }
}

//
// 'make_escl_attr()' - Generate the scanner attributes in eSCL format.
//

xmlDocPtr                    // O - XML document pointer
make_escl_attr(
  pappl_scanner_t *scanner)  // I - Scanner
{
  xmlDocPtr  doc = NULL;
  xmlNodePtr root_node = NULL;

  doc = xmlNewDoc(BAD_CAST "1.0");
  if (!doc)
  return (NULL);

  root_node = xmlNewNode(NULL, BAD_CAST "scan:ScannerCapabilities");
  if (!root_node)
  {
  xmlFreeDoc(doc);
  return (NULL);
  }
  xmlDocSetRootElement(doc, root_node);

  xmlNsPtr ns = xmlNewNs(root_node, BAD_CAST "http://schemas.hp.com/imaging/escl/2011/05/03", BAD_CAST "scan");
  xmlSetNs(root_node, ns);

  xmlNewChild(root_node, NULL, BAD_CAST "pwg:Version", BAD_CAST "2.0"); // Example value
  xmlNewChild(root_node, NULL, BAD_CAST "pwg:MakeAndModel", BAD_CAST scanner->driver_data.make_and_model);

  xmlNodePtr resolutions_node = xmlNewChild(root_node, NULL, BAD_CAST "scan:SupportedResolutions", NULL);
  for (int i = 0; i < MAX_RESOLUTIONS && scanner->driver_data.resolutions[i]; i++)
  {
  char res_str[16];
  snprintf(res_str, sizeof(res_str), "%d", scanner->driver_data.resolutions[i]);
  xmlNewChild(resolutions_node, NULL, BAD_CAST "scan:Resolution", BAD_CAST res_str);
  }

  xmlNodePtr formats_node = xmlNewChild(root_node, NULL, BAD_CAST "scan:DocumentFormatsSupported", NULL);
  for (int i = 0; i < PAPPL_MAX_FORMATS && scanner->driver_data.document_formats_supported[i]; i++)
  {
  xmlNewChild(formats_node, NULL, BAD_CAST "scan:DocumentFormat", BAD_CAST scanner->driver_data.document_formats_supported[i]);
  }

  xmlNodePtr color_modes_node = xmlNewChild(root_node, NULL, BAD_CAST "scan:ColorModesSupported", NULL);
  for (int i = 0; i < PAPPL_MAX_COLOR_MODES && scanner->driver_data.color_modes_supported[i]; i++)
  {
  xmlNewChild(color_modes_node, NULL, BAD_CAST "scan:ColorMode", BAD_CAST _papplScannerColorModeString(scanner->driver_data.color_modes_supported[i]));
  }

  xmlNodePtr input_sources_node = xmlNewChild(root_node, NULL, BAD_CAST "scan:InputSourcesSupported", NULL);
  for (int i = 0; i < PAPPL_MAX_SOURCES && scanner->driver_data.input_sources_supported[i]; i++)
  {
  xmlNewChild(input_sources_node, NULL, BAD_CAST "scan:InputSource", BAD_CAST ScannerInputSourceString(scanner->driver_data.input_sources_supported[i]));
  }

  xmlNewChild(root_node, NULL, BAD_CAST "scan:DuplexSupported", BAD_CAST (scanner->driver_data.duplex_supported ? "true" : "false"));

  xmlNodePtr color_spaces_node = xmlNewChild(root_node, NULL, BAD_CAST "scan:ColorSpacesSupported", NULL);
  for (int i = 0; i < PAPPL_MAX_COLOR_SPACES && scanner->driver_data.color_spaces_supported[i]; i++)
  {
  xmlNewChild(color_spaces_node, NULL, BAD_CAST "scan:ColorSpace", BAD_CAST scanner->driver_data.color_spaces_supported[i]);
  }

  char max_scan_area_str[64];
  snprintf(max_scan_area_str, sizeof(max_scan_area_str), "width=%d,height=%d",
   scanner->driver_data.max_scan_area[0],
   scanner->driver_data.max_scan_area[1]);
  xmlNewChild(root_node, NULL, BAD_CAST "scan:MaxScanArea", BAD_CAST max_scan_area_str);

  xmlNodePtr media_types_node = xmlNewChild(root_node, NULL, BAD_CAST "scan:MediaTypesSupported", NULL);
  for (int i = 0; i < PAPPL_MAX_MEDIA_TYPES && scanner->driver_data.media_type_supported[i]; i++)
  {
  xmlNewChild(media_types_node, NULL, BAD_CAST "scan:MediaType", BAD_CAST scanner->driver_data.media_type_supported[i]);
  }

  xmlNodePtr defaults_node = xmlNewChild(root_node, NULL, BAD_CAST "scan:Defaults", NULL);
  xmlNewChild(defaults_node, NULL, BAD_CAST "scan:DefaultResolution", BAD_CAST ScannerResolutionString(scanner->driver_data.default_resolution));
  xmlNewChild(defaults_node, NULL, BAD_CAST "scan:DefaultColorMode", BAD_CAST _papplScannerColorModeString(scanner->driver_data.default_color_mode));
  xmlNewChild(defaults_node, NULL, BAD_CAST "scan:DefaultInputSource", BAD_CAST ScannerInputSourceString(scanner->driver_data.default_input_source));

  char scan_region_str[64];
  snprintf(scan_region_str, sizeof(scan_region_str), "top=%d,left=%d,width=%d,height=%d",
   scanner->driver_data.scan_region_supported[0],
   scanner->driver_data.scan_region_supported[1],
   scanner->driver_data.scan_region_supported[2],
   scanner->driver_data.scan_region_supported[3]);
  xmlNewChild(root_node, NULL, BAD_CAST "scan:ScanRegionsSupported", BAD_CAST scan_region_str);

  xmlNodePtr mandatory_intents_node = xmlNewChild(root_node, NULL, BAD_CAST "scan:MandatoryIntents", NULL);
  for (int i = 0; i < 5 && scanner->driver_data.mandatory_intents[i]; i++)
  {
  xmlNewChild(mandatory_intents_node, NULL, BAD_CAST "scan:Intent", BAD_CAST scanner->driver_data.mandatory_intents[i]);
  }

  xmlNodePtr optional_intents_node = xmlNewChild(root_node, NULL, BAD_CAST "scan:OptionalIntents", NULL);
  for (int i = 0; i < 5 && scanner->driver_data.optional_intents[i]; i++)
  {
  xmlNewChild(optional_intents_node, NULL, BAD_CAST "scan:Intent", BAD_CAST scanner->driver_data.optional_intents[i]);
  }

  xmlNewChild(root_node, NULL, BAD_CAST "scan:CompressionSupported", BAD_CAST (scanner->driver_data.compression_supported ? "true" : "false"));

  xmlNewChild(root_node, NULL, BAD_CAST "scan:NoiseRemovalSupported", BAD_CAST (scanner->driver_data.noise_removal_supported ? "true" : "false"));

  xmlNewChild(root_node, NULL, BAD_CAST "scan:SharpeningSupported", BAD_CAST (scanner->driver_data.sharpening_supported ? "true" : "false"));

  xmlNewChild(root_node, NULL, BAD_CAST "scan:BinaryRenderingSupported", BAD_CAST (scanner->driver_data.binary_rendering_supported ? "true" : "false"));

  xmlNodePtr feed_directions_node = xmlNewChild(root_node, NULL, BAD_CAST "scan:FeedDirectionsSupported", NULL);
  for (int i = 0; i < 2 && scanner->driver_data.feed_direction_supported[i]; i++)
  {
  xmlNewChild(feed_directions_node, NULL, BAD_CAST "scan:FeedDirection", BAD_CAST scanner->driver_data.feed_direction_supported[i]);
  }

  return (doc);
}

// Converts input source to string
const char *ScannerInputSourceString(pappl_sc_input_source_t value)
{
  switch (value)
  {
  case PAPPL_FLATBED:
  return "Flatbed";
  case PAPPL_ADF:
  return "ADF";
  default:
  return "Unknown";
  }
}

// Converts resolution to string
const char *ScannerResolutionString(int resolution)
{
  static char res_str[32];
  snprintf(res_str, sizeof(res_str), "%d DPI", resolution);
  return res_str;
}

// Converts color mode to string
const char *_papplScannerColorModeString(pappl_sc_color_mode_t value)
{
  switch (value)
  {
    case PAPPL_BLACKANDWHITE1:
      return "BlackAndWhite1";
    case PAPPL_GRAYSCALE8:
      return "Grayscale8";
    case PAPPL_RGB24:
      return "RGB24";
    default:
      return "Unknown";
  }
}