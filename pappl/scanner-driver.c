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

// Declare the capability structure and the callback function
typedef struct {
  char make_and_model[256];
  const char *document_formats_supported[PAPPL_MAX_FORMATS];
  pappl_sc_color_mode_t color_modes_supported[PAPPL_MAX_COLOR_MODES];
  int resolutions[MAX_RESOLUTIONS];
  pappl_sc_input_source_t input_sources_supported[PAPPL_MAX_SOURCES];
  bool duplex_supported;
  float max_scan_area[2];
} capability_t;

capability_t *capability_cb(pappl_scanner_t *scanner);
const char *pappl_sc_color_mode_str(pappl_sc_color_mode_t mode);
const char *pappl_sc_input_source_str(pappl_sc_input_source_t source);

//
// 'papplScannerGetDriverName()' - Get the driver name for a scanner.
//
// This function returns the driver name for the scanner.
//

const char *                // O - Driver name or `NULL` for none
papplScannerGetDriverName(
  pappl_scanner_t *scanner)        // I - Scanner
{
  return (scanner ? scanner->driver_name : NULL);
}

//
// 'papplScannerGetDriverData()' - Get the current scan driver data.
//
// This function copies the current scan driver data.
//

pappl_sc_driver_data_t *        // O - Driver data or `NULL` if none
papplScannerGetDriverData(
  pappl_scanner_t        *scanner,    // I - Scanner
  pappl_sc_driver_data_t *data)    // I - Pointer to driver data structure to fill
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
// '_papplScannerInitDriverData()' - Initialize a scan driver data structure.
//

void
_papplScannerInitDriverData(
  pappl_scanner_t        *scanner,    // I - Scanner
  pappl_sc_driver_data_t *d)          // I - Driver data
{
  capability_t *capability = capability_cb(scanner);
  if (capability)
  {
    strncpy(d->make_and_model, capability->make_and_model, sizeof(d->make_and_model) - 1);
    d->make_and_model[sizeof(d->make_and_model) - 1] = '\0'; // Ensure null-termination

    for (int i = 0; i < PAPPL_MAX_FORMATS && capability->document_formats_supported[i]; i++)
    {
      d->document_formats_supported[i] = capability->document_formats_supported[i];
    }

    for (int i = 0; i < PAPPL_MAX_COLOR_MODES && capability->color_modes_supported[i]; i++)
    {
      d->color_modes_supported[i] = capability->color_modes_supported[i];
    }

    for (int i = 0; i < MAX_RESOLUTIONS && capability->resolutions[i]; i++)
    {
      d->resolutions[i] = capability->resolutions[i];
    }

    for (int i = 0; i < PAPPL_MAX_SOURCES && capability->input_sources_supported[i]; i++)
    {
      d->input_sources_supported[i] = capability->input_sources_supported[i];
    }

    d->duplex_supported = capability->duplex_supported;

    d->max_scan_area[0] = capability->max_scan_area[0];
    d->max_scan_area[1] = capability->max_scan_area[1];

  }
}

//
// 'papplScannerSetDriverData()' - Set the driver data.
//
// This function validates and sets the driver data.
//
//

bool                    // O - `true` on success, `false` on failure
papplScannerSetDriverData(
  pappl_scanner_t        *scanner,    // I - Scanner
  pappl_sc_driver_data_t *data)    // I - Driver data
{
  if (!scanner || !data)
  return (false);

  _papplRWLockWrite(scanner);

  memcpy(&scanner->driver_data, data, sizeof(scanner->driver_data));

  _papplRWUnlock(scanner);

  return (true);
}

xmlDocPtr make_escl_attr(pappl_scanner_t *scanner)
{
  xmlDocPtr doc = xmlNewDoc(BAD_CAST "1.0");
  xmlNodePtr root_node = xmlNewNode(NULL, BAD_CAST "eSCL");
  xmlDocSetRootElement(doc, root_node);

  capability_t *capability = capability_cb(scanner);
  if (!capability) {
    return doc;
  }

  xmlNewChild(root_node, NULL, BAD_CAST "MakeAndModel", BAD_CAST capability->make_and_model);

  xmlNodePtr formats_node = xmlNewChild(root_node, NULL, BAD_CAST "DocumentFormatsSupported", NULL);
  for (int i = 0; i < PAPPL_MAX_FORMATS && capability->document_formats_supported[i]; i++) {
    xmlNewChild(formats_node, NULL, BAD_CAST "Format", BAD_CAST capability->document_formats_supported[i]);
  }


  xmlNodePtr color_modes_node = xmlNewChild(root_node, NULL, BAD_CAST "ColorModesSupported", NULL);
  for (int i = 0; i < PAPPL_MAX_COLOR_MODES && capability->color_modes_supported[i]; i++) {
    xmlNewChild(color_modes_node, NULL, BAD_CAST "ColorMode", BAD_CAST pappl_sc_color_mode_str(capability->color_modes_supported[i]));
  }

  xmlNodePtr resolutions_node = xmlNewChild(root_node, NULL, BAD_CAST "Resolutions", NULL);
  for (int i = 0; i < MAX_RESOLUTIONS && capability->resolutions[i]; i++) {
    char resolution_str[10];
    snprintf(resolution_str, sizeof(resolution_str), "%d", capability->resolutions[i]);
    xmlNewChild(resolutions_node, NULL, BAD_CAST "Resolution", BAD_CAST resolution_str);
  }

  xmlNodePtr input_sources_node = xmlNewChild(root_node, NULL, BAD_CAST "InputSourcesSupported", NULL);
  for (int i = 0; i < PAPPL_MAX_SOURCES && capability->input_sources_supported[i]; i++) {
    xmlNewChild(input_sources_node, NULL, BAD_CAST "InputSource", BAD_CAST pappl_sc_input_source_str(capability->input_sources_supported[i]));
  }

  xmlNewChild(root_node, NULL, BAD_CAST "DuplexSupported", BAD_CAST (capability->duplex_supported ? "true" : "false"));

  xmlNodePtr max_scan_area_node = xmlNewChild(root_node, NULL, BAD_CAST "MaxScanArea", NULL);
  char max_scan_width[10], max_scan_height[10];
  snprintf(max_scan_width, sizeof(max_scan_width), "%.2f", capability->max_scan_area[0]);
  snprintf(max_scan_height, sizeof(max_scan_height), "%.2f", capability->max_scan_area[1]);
  xmlNewChild(max_scan_area_node, NULL, BAD_CAST "Width", BAD_CAST max_scan_width);
  xmlNewChild(max_scan_area_node, NULL, BAD_CAST "Height", BAD_CAST max_scan_height);

  return doc;
}

const char *pappl_sc_color_mode_str(pappl_sc_color_mode_t mode) {
  switch (mode) {
    case PAPPL_BLACKANDWHITE1:
      return "BlackAndWhite";
    case PAPPL_GRAYSCALE8:
      return "Grayscale";
    case PAPPL_RGB24:
      return "RGB";
    default:
      return "Unknown";
  }
}

const char *pappl_sc_input_source_str(pappl_sc_input_source_t source) {
  switch (source) {
    case PAPPL_FLATBED:
      return "Flatbed";
    case PAPPL_ADF:
      return "ADF";
    default:
      return "Unknown";
  }
}
