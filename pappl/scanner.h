//
// Public scanner header file for the Scanner Application Framework
//
// Copyright © 2019-2024 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#ifndef _PAPPL_SCANNER_H_
#  define _PAPPL_SCANNER_H_
#  include "base.h"
#include "system.h"
#  ifdef __cplusplus
extern "C" {
#  endif // __cplusplus

//
// Limits...
//
#define PAPPL_MAX_FORMATS 5 // Most scanners support a variety of document formats such as JPEG, PDF, TIFF, PNG, and BMP.
#define PAPPL_MAX_COLOR_MODES 3 // Most scanners support a few color modes: Black and White, Grayscale, Color
#define PAPPL_MAX_SOURCES 2 // MOST scanners offer two input sources: Flatbed and ADF
#define PAPPL_MAX_COLOR_SPACES 2 // Common color spaces like sRGB and AdobeRGB
#define PAPPL_MAX_MEDIA_TYPES 5 // Various media types like Plain, Photo, Card, etc.
#define MAX_RESOLUTIONS 5 // The number of resolutions supported by the scanner

//
// Constants...
//

// This defines the overall state of the scanner. It describes the scanner's operational state,
// focusing on what the scanner is currently doing or its readiness to perform tasks.
// The states are mutually exclusive, meaning the scanner can be in only one of these states at a time.
typedef enum
{
  ESCL_SSTATE_IDLE,       // Scanner is idle
  ESCL_SSTATE_PROCESSING, // Scanner is busy with some job or activity
  ESCL_SSTATE_TESTING,    // Scanner is calibrating, preparing the unit
  ESCL_SSTATE_STOPPED,    // Scanner stopped due to an error condition
  ESCL_SSTATE_DOWN        // Scanner is unavailable
} escl_sstate_t;

// This defines specific reasons for the scanner's state. These reasons can provide more detailed
// information about why the scanner is in its current state. Multiple reasons can be combined using
// bitwise operations, which means the scanner can have multiple reasons for being in a particular state at the same time.
typedef enum
{
  PAPPL_SREASON_NONE = 0x0000,             // 'none', no error, scanner is ready
  PAPPL_SREASON_IDLE = 0x0001,             // 'idle', scanner is idle
  PAPPL_SREASON_PROCESSING = 0x0002,       // 'processing', scanner is currently processing a job
  PAPPL_SREASON_TESTING = 0x0004,          // 'testing', scanner is in calibration or preparation mode
  PAPPL_SREASON_STOPPED = 0x0008,          // 'stopped', an error has occurred and the scanner has stopped
  PAPPL_SREASON_DOWN = 0x0010,             // 'down', the scanner is unavailable
} pappl_sreason_t;

// Define color modes supported by the scanner
typedef enum {
  PAPPL_BLACKANDWHITE1, // For black and white scans
  PAPPL_GRAYSCALE8,  // For grayscale scans
  PAPPL_RGB24       // For full color scans
} pappl_sc_color_mode_t;

// Define input sources for the scanner
typedef enum {
  PAPPL_FLATBED, // For flatbed scanners
  PAPPL_ADF,     // For automatic dopappl_sc_input_source_t;cument feeder
} pappl_sc_input_source_t;

typedef unsigned pappl_identify_sc_actions_t; // eSCL actions for identifying the scanner

//
// Structures
//

typedef struct pappl_icon_sc_s		// Scanner PNG icon structure
{
  char		filename[256];			// External filename, if any
  const void	*data;				// PNG icon data, if any
  size_t	datalen;			// Size of PNG icon data
} pappl_icon_sc_t;

typedef struct pappl_sc_options_s // Provides scan job options for the user after we have fetched the scanner driver data
{
  char document_format[64]; // Desired output format (JPEG, PDF, TIFF, PNG, BMP)
  pappl_sc_color_mode_t color_mode; // Color mode for the scan
  int resolution; // Scanning resolution in DPI
  pappl_sc_input_source_t input_source; // Selected input source
  bool duplex; // Duplex scanning option
  char intent[64]; // Scan intent (e.g., Document, Photo, Preview, etc.)
  struct {
  int width; // Width of the scan area
  int height; // Height of the scan area
  int x_offset; // X offset for the scan area
  int y_offset; // Y offset for the scan area
  } scan_area;
  struct {
  int brightness; // Brightness adjustment
  int contrast; // Contrast adjustment
  int gamma; // Gamma adjustment
  int threshold; // Threshold for black/white scans
  int saturation; // Saturation adjustment
  int sharpness; // Sharpness adjustment
  } adjustments;
  bool blank_page_removal; // Automatically detect and remove blank pages
  unsigned num_pages; // Number of pages to scan (for ADF)
  int compression_factor; // Compression factor for the scan
  bool noise_removal; // Noise removal option
  bool sharpening; // Sharpening option
} pappl_sc_options_t;

typedef struct pappl_sc_driver_data_s  // Initially polling the scanner driver for capabilities and settings
{
  pappl_sc_identify_cb_t identify_cb; // Callback for identifying the scanner
  pappl_sc_delete_cb_t		sc_delete_cb;	// Scanner deletion callback
  pappl_sc_capabilities_cb_t capabilities_cb; // Callback for getting scanner capabilities
  pappl_sc_job_create_cb_t job_create_cb; // Callback for creating a scan job
  pappl_sc_job_delete_cb_t job_delete_cb; // Callback for deleting a scan job
  pappl_sc_data_cb_t data_cb; // Callback for getting scan data
  pappl_sc_status_cb_t status_cb; // Callback for getting scanner status
  pappl_sc_job_complete_cb_t job_complete_cb; // Callback for completing a scan job
  pappl_sc_job_cancel_cb_t job_cancel_cb; // Callback for cancelling a scan job
  pappl_sc_buffer_info_cb_t buffer_info_cb; // Callback for getting buffer information
  pappl_sc_image_info_cb_t image_info_cb; // Callback for getting image information

  pappl_identify_sc_actions_t identify_default;	// "identify-actions-default" values
  pappl_identify_sc_actions_t identify_supported;	// "identify-actions-supported" values
  pappl_icon_sc_t		icons[3];		// "scanner-icons" values

  char make_and_model[128]; // Make and model of the scanner
  const char *document_formats_supported[PAPPL_MAX_FORMATS]; // Supported document formats (JPEG, PDF, TIFF, PNG, BMP)
  pappl_sc_color_mode_t color_modes_supported[PAPPL_MAX_COLOR_MODES]; // Supported color modes (BlackAndWhite1, Grayscale8, RGB24)
  int resolutions[MAX_RESOLUTIONS]; // All optical resolutions in DPI
  pappl_sc_input_source_t input_sources_supported[PAPPL_MAX_SOURCES]; // Supported input sources (Flatbed, ADF)
  bool duplex_supported; // Duplex (double-sided) scanning support
  const char *color_spaces_supported[PAPPL_MAX_COLOR_SPACES]; // Supported color spaces (sRGB, AdobeRGB)
  int max_scan_area[2]; // Maximum scan area size (width, height)
  const char *media_type_supported[PAPPL_MAX_MEDIA_TYPES]; // Types of media that can be scanned (Plain, Photo, Card)
  int default_resolution; // Default scanning resolution
  pappl_sc_color_mode_t default_color_mode; // Default color mode
  pappl_sc_input_source_t default_input_source; // Default input source
  int scan_region_supported[4]; // Supported scan regions (top, left, width, height)
  const char *mandatory_intents[5]; // Mandatory intents supported by the scanner (e.g., Document, Photo, TextAndGraphic, Preview, BusinessCard)
  const char *optional_intents[5]; // Optional intents supported by the scanner (e.g., Object, CustomIntent)

  struct {
  int brightness; // Brightness adjustment
  int contrast; // Contrast adjustment
  int gamma; // Gamma adjustment
  int threshold; // Threshold for black/white scans
  int saturation; // Saturation adjustment
  int sharpness; // Sharpness adjustment
  } adjustments;

  bool compression_supported; // Whether compression is supported
  bool noise_removal_supported; // Whether noise removal is supported
  bool sharpening_supported; // Whether sharpness adjustment is supported
  bool binary_rendering_supported; // Whether binary rendering is supported
  bool blank_page_removal_supported; // Whether blank page removal is supported

  const char *feed_direction_supported[2]; // Supported feed directions (e.g., LeftToRight, RightToLeft)
  const char *default_document_format; // Default document format
  const char *default_color_space; // Default color space
  int default_scan_area[2]; // Default scan area (width, height)
  const char *default_media_type; // Default media type
  const char *default_intent; // Default intent
} pappl_sc_driver_data_t;

//
// Functions...
//

extern void		papplScannerAddLink(pappl_scanner_t *scanner, const char *label, const char *path_or_url, pappl_loptions_t options) _PAPPL_PUBLIC;

extern void		papplScannerCloseDevice(pappl_scanner_t *scanner) _PAPPL_PUBLIC;

extern pappl_scanner_t	*papplScannerCreate(pappl_system_t *system, int scanner_id, const char *scanner_name, const char *driver_name, const char *device_id, const char *device_uri) _PAPPL_PUBLIC;
extern void		papplScannerDelete(pappl_scanner_t *scanner) _PAPPL_PUBLIC;
extern void		papplScannerDisable(pappl_scanner_t *scanner) _PAPPL_PUBLIC;
extern void		papplScannerEnable(pappl_scanner_t *scanner) _PAPPL_PUBLIC;

extern pappl_contact_t	*papplScannerGetContact(pappl_scanner_t *scanner, pappl_contact_t *contact) _PAPPL_PUBLIC;
extern const char	*papplScannerGetDeviceID(pappl_scanner_t *scanner) _PAPPL_PUBLIC;
extern const char	*papplScannerGetDeviceURI(pappl_scanner_t *scanner) _PAPPL_PUBLIC;
extern char		*papplScannerGetDNSSDName(pappl_scanner_t *scanner, char *buffer, size_t bufsize) _PAPPL_PUBLIC;

extern pappl_sc_driver_data_t		*papplScannerGetDriverData(pappl_scanner_t *scanner, pappl_sc_driver_data_t *data) _PAPPL_PUBLIC;
extern const char	*papplScannerGetDriverName(pappl_scanner_t *scanner) _PAPPL_PUBLIC;
extern char		*papplScannerGetGeoLocation(pappl_scanner_t *scanner, char *buffer, size_t bufsize) _PAPPL_PUBLIC;
extern int		papplScannerGetID(pappl_scanner_t *scanner) _PAPPL_PUBLIC;
extern char		*papplScannerGetLocation(pappl_scanner_t *scanner, char *buffer, size_t bufsize) _PAPPL_PUBLIC;

extern const char	*papplScannerGetName(pappl_scanner_t *scanner) _PAPPL_PUBLIC;
extern int		papplScannerGetNextJobID(pappl_scanner_t *scanner) _PAPPL_PUBLIC;

extern char		*papplScannerGetOrganization(pappl_scanner_t *scanner, char *buffer, size_t bufsize) _PAPPL_PUBLIC;
extern char		*papplScannerGetOrganizationalUnit(pappl_scanner_t *scanner, char *buffer, size_t bufsize) _PAPPL_PUBLIC;

extern char		*papplScannerGetPath(pappl_scanner_t *scanner, const char *subpath, char *buffer, size_t bufsize) _PAPPL_PUBLIC;

extern pappl_sreason_t	papplScannerGetReasons(pappl_scanner_t *scanner) _PAPPL_PUBLIC;
extern escl_sstate_t 	papplScannerGetState(pappl_scanner_t *scanner) _PAPPL_PUBLIC;

extern pappl_system_t	*papplScannerGetSystem(pappl_scanner_t *scanner) _PAPPL_PUBLIC;

extern void		papplScannerHTMLFooter(pappl_client_t *client) _PAPPL_PUBLIC;
extern void		papplScannerHTMLHeader(pappl_client_t *client, const char *title, int refresh) _PAPPL_PUBLIC;

extern bool		papplScannerIsAcceptingJobs(pappl_scanner_t *scanner) _PAPPL_PUBLIC;
extern bool		papplScannerIsDeleted(pappl_scanner_t *scanner) _PAPPL_PUBLIC;

extern pappl_device_t	*papplScannerOpenDevice(pappl_scanner_t *scanner) _PAPPL_PUBLIC;
extern int		papplScannerOpenFile(pappl_scanner_t *scanner, char *fname, size_t fnamesize, const char *directory, const char *resname, const char *ext, const char *mode) _PAPPL_PUBLIC;

extern void		papplScannerPause(pappl_scanner_t *printer) _PAPPL_PUBLIC;
extern void		papplScannerRemoveLink(pappl_scanner_t *scanner, const char *label) _PAPPL_PUBLIC;
extern void		papplScannerResume(pappl_scanner_t *scanner) _PAPPL_PUBLIC;

extern void		papplScannerSetContact(pappl_scanner_t *scanner, pappl_contact_t *contact) _PAPPL_PUBLIC;
extern void		papplScannerSetDNSSDName(pappl_scanner_t *scanner, const char *value) _PAPPL_PUBLIC;

extern bool		papplScannerSetDriverData(pappl_scanner_t *scanner, pappl_sc_driver_data_t *data) _PAPPL_PUBLIC;
extern bool		papplScannerSetDriverDefaults(pappl_scanner_t *scanner, pappl_sc_driver_data_t *data) _PAPPL_PUBLIC;

extern void		papplScannerSetGeoLocation(pappl_scanner_t *scanner, const char *value) _PAPPL_PUBLIC;
extern void		papplScannerSetLocation(pappl_scanner_t *scanner, const char *value) _PAPPL_PUBLIC;
extern void		papplScannerSetNextJobID(pappl_scanner_t *scanner, int next_job_id) _PAPPL_PUBLIC;

extern void		papplScannerSetOrganization(pappl_scanner_t *scanner, const char *value) _PAPPL_PUBLIC;
extern void		papplScannerSetOrganizationalUnit(pappl_scanner_t *scanner, const char *value) _PAPPL_PUBLIC;
extern void		papplScannerSetReasons(pappl_scanner_t *scanner, pappl_sreason_t add, pappl_sreason_t remove) _PAPPL_PUBLIC;

#  ifdef __cplusplus
}
#  endif // __cplusplus
#endif // !_PAPPL_SCANNER_H_
