//
// Public scanner header file for the Printer Application Framework
//
// Copyright © 2019-2026 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#ifndef _PAPPL_SCANNER_H_
#  define _PAPPL_SCANNER_H_
#  include "base.h"
#  ifdef __cplusplus
extern "C" {
#  endif // __cplusplus


//
// Limits...
//

#  define PAPPL_MAX_SCAN_COLOR_MODE	8	// Maximum number of scan color modes
#  define PAPPL_MAX_SCAN_FORMAT		10	// Maximum number of document formats
#  define PAPPL_MAX_SCAN_INTENT		8	// Maximum number of intents
#  define PAPPL_MAX_SCAN_RESOLUTION	16	// Maximum number of scan resolutions
#  define PAPPL_MAX_SCAN_SOURCE		4	// Maximum number of input sources


//
// Constants...
//

enum pappl_scan_color_mode_e		// eSCL "ColorMode" bit values
{
  PAPPL_SCAN_COLOR_MODE_BLACK_AND_WHITE_1 = 0x0001,
					// 'BlackAndWhite1' - 1-bit B&W
  PAPPL_SCAN_COLOR_MODE_GRAYSCALE_8 = 0x0002,
					// 'Grayscale8' - 8-bit grayscale
  PAPPL_SCAN_COLOR_MODE_GRAYSCALE_16 = 0x0004,
					// 'Grayscale16' - 16-bit grayscale
  PAPPL_SCAN_COLOR_MODE_RGB_24 = 0x0008,// 'RGB24' - 24-bit color
  PAPPL_SCAN_COLOR_MODE_RGB_48 = 0x0010	// 'RGB48' - 48-bit color
};
typedef unsigned pappl_scan_color_mode_t;
					// Bitfield for eSCL "ColorMode" values

enum pappl_scan_content_type_e		// eSCL "ContentType" bit values
{
  PAPPL_SCAN_CONTENT_AUTO = 0x01,	// 'Auto' - scanner decides
  PAPPL_SCAN_CONTENT_HALFTONE = 0x02,	// 'Halftone'
  PAPPL_SCAN_CONTENT_LINE_ART = 0x04,	// 'LineArt'
  PAPPL_SCAN_CONTENT_MAGAZINE = 0x08,	// 'Magazine'
  PAPPL_SCAN_CONTENT_PHOTO = 0x10,	// 'Photo'
  PAPPL_SCAN_CONTENT_TEXT = 0x20,	// 'Text'
  PAPPL_SCAN_CONTENT_TEXT_AND_PHOTO = 0x40
					// 'TextAndPhoto'
};
typedef unsigned pappl_scan_content_t;	// Bitfield for eSCL "ContentType" values

enum pappl_scan_input_source_e		// eSCL "InputSource" bit values
{
  PAPPL_SCAN_INPUT_SOURCE_PLATEN = 0x01,// 'Platen' - flatbed glass
  PAPPL_SCAN_INPUT_SOURCE_ADF = 0x02,	// 'Adf' - automatic document feeder
  PAPPL_SCAN_INPUT_SOURCE_CAMERA = 0x04	// 'Camera' - camera-based input
};
typedef unsigned pappl_scan_input_source_t;
					// Bitfield for eSCL "InputSource" values

enum pappl_scan_intent_e		// eSCL "Intent" bit values
{
  PAPPL_SCAN_INTENT_DOCUMENT = 0x01,	// 'Document'
  PAPPL_SCAN_INTENT_PHOTO = 0x02,	// 'Photo'
  PAPPL_SCAN_INTENT_PREVIEW = 0x04,	// 'Preview'
  PAPPL_SCAN_INTENT_TEXT_AND_GRAPHIC = 0x08,
					// 'TextAndGraphic'
  PAPPL_SCAN_INTENT_BUSINESS_CARD = 0x10
					// 'BusinessCard'
};
typedef unsigned pappl_scan_intent_t;	// Bitfield for eSCL "Intent" values


//
// Structures...
//

struct pappl_sc_driver_s			// Scanner driver information
{
  const char	*name;				// Driver name
  const char	*description;			// Driver description (make and model)
  const char	*device_id;			// IEEE-1284 device ID
  void		*extension;			// Extension data pointer
};


//
// Callback function types...
//

typedef void (*pappl_sc_delete_cb_t)(pappl_scanner_t *scanner, pappl_sc_driver_data_t *data);
					// Scanner deletion callback
typedef bool (*pappl_sc_rendjob_cb_t)(pappl_job_t *job, pappl_sc_options_t *options, pappl_device_t *device);
					// End a scan job callback
typedef bool (*pappl_sc_rendpage_cb_t)(pappl_job_t *job, pappl_sc_options_t *options, pappl_device_t *device, unsigned page);
					// End a scan page callback
typedef bool (*pappl_sc_rstartjob_cb_t)(pappl_job_t *job, pappl_sc_options_t *options, pappl_device_t *device);
					// Start a scan job callback
typedef bool (*pappl_sc_rstartpage_cb_t)(pappl_job_t *job, pappl_sc_options_t *options, pappl_device_t *device, unsigned page);
					// Start a scan page callback, returns
					// `false` when no more pages (ADF done)
typedef bool (*pappl_sc_rreadline_cb_t)(pappl_job_t *job, pappl_sc_options_t *options, pappl_device_t *device, unsigned y, unsigned char *line, size_t linesize);
					// Read a line of raster from scanner
typedef bool (*pappl_sc_status_cb_t)(pappl_scanner_t *scanner);
					// Update scanner status callback


//
// Structures...
//

struct pappl_sc_options_s		// Combined scan job options
{
  pappl_scan_intent_t	intent;		// eSCL "Intent" value
  pappl_scan_input_source_t input_source;
					// eSCL "InputSource" value
  pappl_scan_color_mode_t color_mode;	// eSCL "ColorMode" value
  pappl_scan_content_t	content_type;	// eSCL "ContentType" value
  int			x_resolution;	// Horizontal resolution in DPI
  int			y_resolution;	// Vertical resolution in DPI
  int			scan_x;		// X offset in 1/300"
  int			scan_y;		// Y offset in 1/300"
  int			scan_width;	// Width in 1/300"
  int			scan_height;	// Height in 1/300"
  char			format[256];	// MIME type (image/jpeg, application/pdf)
  bool			duplex;		// Duplex ADF scanning?
  int			brightness;	// Brightness adjustment (-100 to 100)
  int			contrast;	// Contrast adjustment (-100 to 100)
  int			compression;	// Compression quality (1-100)
  int			threshold;	// B&W threshold (0-255)
  int			sharpen;	// Sharpening level (0-100)
};

struct pappl_sc_driver_data_s		// Scanner driver data
{
  void			*extension;	// Extension data (managed by driver)

  // Callbacks...
  pappl_sc_delete_cb_t	delete_cb;	// Scanner deletion callback
  pappl_sc_rstartjob_cb_t rstartjob_cb;	// Start scan job callback
  pappl_sc_rstartpage_cb_t rstartpage_cb;
					// Start scan page callback
  pappl_sc_rreadline_cb_t rreadline_cb;	// Read scan line callback
  pappl_sc_rendpage_cb_t rendpage_cb;	// End scan page callback
  pappl_sc_rendjob_cb_t	rendjob_cb;	// End scan job callback
  pappl_sc_status_cb_t	status_cb;	// Update status callback

  // Scanner identity...
  char			make_and_model[128];
					// "scanner-make-and-model" value
  pappl_icon_t		icons[3];	// "scanner-icons" values

  // Color mode capabilities...
  pappl_scan_color_mode_t color_supported;
					// Supported color modes
  pappl_scan_color_mode_t color_default;// Default color mode

  // Intent capabilities...
  pappl_scan_intent_t	intents_supported;
					// Supported intents
  pappl_scan_intent_t	intent_default;	// Default intent

  // Input source capabilities...
  pappl_scan_input_source_t input_sources_supported;
					// Supported input sources
  pappl_scan_input_source_t input_source_default;
					// Default input source

  // Content type capabilities...
  pappl_scan_content_t	content_supported;
					// Supported content types
  pappl_scan_content_t	content_default;// Default content type

  // Duplex support...
  bool			duplex_supported;
					// ADF duplex support?

  // Document format capabilities...
  size_t		num_format;	// Number of supported formats
  char			format[PAPPL_MAX_SCAN_FORMAT][256];
					// Supported MIME types

  // Resolution capabilities...
  size_t		num_resolution;	// Number of resolutions
  int			x_resolution[PAPPL_MAX_SCAN_RESOLUTION];
					// Horizontal resolutions in DPI
  int			y_resolution[PAPPL_MAX_SCAN_RESOLUTION];
					// Vertical resolutions in DPI
  int			x_default;	// Default horizontal resolution
  int			y_default;	// Default vertical resolution

  // Scan area limits (in 1/300 inch)...
  int			platen_min_width;
					// Platen minimum width
  int			platen_min_height;
					// Platen minimum height
  int			platen_max_width;
					// Platen maximum width
  int			platen_max_height;
					// Platen maximum height
  int			adf_min_width;	// ADF minimum width
  int			adf_min_height;	// ADF minimum height
  int			adf_max_width;	// ADF maximum width
  int			adf_max_height;	// ADF maximum height

  // Image adjustment capabilities (0 = not supported)...
  int			brightness_supported;
					// Non-zero if supported
  int			contrast_supported;
					// Non-zero if supported
  int			sharpen_supported;
					// Non-zero if supported
  int			threshold_supported;
					// Non-zero if supported
};


//
// Functions...
//

extern void		papplScannerAddLink(pappl_scanner_t *scanner, const char *label, const char *path_or_url, pappl_loptions_t options) _PAPPL_PUBLIC;
extern void		papplScannerCancelAllJobs(pappl_scanner_t *scanner) _PAPPL_PUBLIC;
extern void		papplScannerCloseDevice(pappl_scanner_t *scanner) _PAPPL_PUBLIC;

extern pappl_scanner_t	*papplScannerCreate(pappl_system_t *system, int scanner_id, const char *scanner_name, const char *driver_name, const char *device_id, const char *device_uri) _PAPPL_PUBLIC;

extern void		papplScannerDelete(pappl_scanner_t *scanner) _PAPPL_PUBLIC;

extern pappl_job_t	*papplScannerFindJob(pappl_scanner_t *scanner, int job_id) _PAPPL_PUBLIC;

extern pappl_contact_t	*papplScannerGetContact(pappl_scanner_t *scanner, pappl_contact_t *contact) _PAPPL_PUBLIC;
extern const char	*papplScannerGetDeviceID(pappl_scanner_t *scanner) _PAPPL_PUBLIC;
extern const char	*papplScannerGetDeviceURI(pappl_scanner_t *scanner) _PAPPL_PUBLIC;
extern char		*papplScannerGetDNSSDName(pappl_scanner_t *scanner, char *buffer, size_t bufsize) _PAPPL_PUBLIC;
extern ipp_t		*papplScannerGetDriverAttributes(pappl_scanner_t *scanner) _PAPPL_PUBLIC;
extern pappl_sc_driver_data_t *papplScannerGetDriverData(pappl_scanner_t *scanner, pappl_sc_driver_data_t *data) _PAPPL_PUBLIC;
extern const char	*papplScannerGetDriverName(pappl_scanner_t *scanner) _PAPPL_PUBLIC;
extern char		*papplScannerGetGeoLocation(pappl_scanner_t *scanner, char *buffer, size_t bufsize) _PAPPL_PUBLIC;
extern int		papplScannerGetID(pappl_scanner_t *scanner) _PAPPL_PUBLIC;
extern int		papplScannerGetImpressionsCompleted(pappl_scanner_t *scanner) _PAPPL_PUBLIC;
extern char		*papplScannerGetLocation(pappl_scanner_t *scanner, char *buffer, size_t bufsize) _PAPPL_PUBLIC;
extern size_t		papplScannerGetMaxActiveJobs(pappl_scanner_t *scanner) _PAPPL_PUBLIC;
extern size_t		papplScannerGetMaxCompletedJobs(pappl_scanner_t *scanner) _PAPPL_PUBLIC;
extern const char	*papplScannerGetName(pappl_scanner_t *scanner) _PAPPL_PUBLIC;
extern int		papplScannerGetNextJobID(pappl_scanner_t *scanner) _PAPPL_PUBLIC;
extern size_t		papplScannerGetNumberOfActiveJobs(pappl_scanner_t *scanner) _PAPPL_PUBLIC;
extern size_t		papplScannerGetNumberOfCompletedJobs(pappl_scanner_t *scanner) _PAPPL_PUBLIC;
extern size_t		papplScannerGetNumberOfJobs(pappl_scanner_t *scanner) _PAPPL_PUBLIC;
extern char		*papplScannerGetOrganization(pappl_scanner_t *scanner, char *buffer, size_t bufsize) _PAPPL_PUBLIC;
extern char		*papplScannerGetOrganizationalUnit(pappl_scanner_t *scanner, char *buffer, size_t bufsize) _PAPPL_PUBLIC;
extern char		*papplScannerGetPath(pappl_scanner_t *scanner, const char *subpath, char *buffer, size_t bufsize) _PAPPL_PUBLIC;
extern pappl_preason_t	papplScannerGetReasons(pappl_scanner_t *scanner) _PAPPL_PUBLIC;
extern char		*papplScannerGetScanGroup(pappl_scanner_t *scanner, char *buffer, size_t bufsize) _PAPPL_PUBLIC;
extern ipp_pstate_t	papplScannerGetState(pappl_scanner_t *scanner) _PAPPL_PUBLIC;
extern pappl_system_t	*papplScannerGetSystem(pappl_scanner_t *scanner) _PAPPL_PUBLIC;

extern bool		papplScannerIsDeleted(pappl_scanner_t *scanner) _PAPPL_PUBLIC;

extern void		papplScannerIterateActiveJobs(pappl_scanner_t *scanner, pappl_job_cb_t cb, void *data, size_t job_index, size_t limit) _PAPPL_PUBLIC;
extern void		papplScannerIterateAllJobs(pappl_scanner_t *scanner, pappl_job_cb_t cb, void *data, size_t job_index, size_t limit) _PAPPL_PUBLIC;
extern void		papplScannerIterateCompletedJobs(pappl_scanner_t *scanner, pappl_job_cb_t cb, void *data, size_t job_index, size_t limit) _PAPPL_PUBLIC;

extern pappl_device_t	*papplScannerOpenDevice(pappl_scanner_t *scanner) _PAPPL_PUBLIC;

extern void		papplScannerSetContact(pappl_scanner_t *scanner, pappl_contact_t *contact) _PAPPL_PUBLIC;
extern void		papplScannerSetDNSSDName(pappl_scanner_t *scanner, const char *value) _PAPPL_PUBLIC;
extern bool		papplScannerSetDriverData(pappl_scanner_t *scanner, pappl_sc_driver_data_t *data, ipp_t *attrs) _PAPPL_PUBLIC;
extern bool		papplScannerSetDriverDefaults(pappl_scanner_t *scanner, pappl_sc_driver_data_t *data) _PAPPL_PUBLIC;
extern void		papplScannerSetGeoLocation(pappl_scanner_t *scanner, const char *value) _PAPPL_PUBLIC;
extern void		papplScannerSetImpressionsCompleted(pappl_scanner_t *scanner, int add) _PAPPL_PUBLIC;
extern void		papplScannerSetLocation(pappl_scanner_t *scanner, const char *value) _PAPPL_PUBLIC;
extern void		papplScannerSetMaxActiveJobs(pappl_scanner_t *scanner, size_t max_active_jobs) _PAPPL_PUBLIC;
extern void		papplScannerSetMaxCompletedJobs(pappl_scanner_t *scanner, size_t max_completed_jobs) _PAPPL_PUBLIC;
extern void		papplScannerSetNextJobID(pappl_scanner_t *scanner, int next_job_id) _PAPPL_PUBLIC;
extern void		papplScannerSetOrganization(pappl_scanner_t *scanner, const char *value) _PAPPL_PUBLIC;
extern void		papplScannerSetOrganizationalUnit(pappl_scanner_t *scanner, const char *value) _PAPPL_PUBLIC;
extern void		papplScannerSetReasons(pappl_scanner_t *scanner, pappl_preason_t add, pappl_preason_t remove) _PAPPL_PUBLIC;
extern void		papplScannerSetScanGroup(pappl_scanner_t *scanner, const char *value) _PAPPL_PUBLIC;

extern pappl_scanner_t	*papplPrinterGetScanner(pappl_printer_t *printer) _PAPPL_PUBLIC;
extern void		papplPrinterSetScanner(pappl_printer_t *printer, pappl_scanner_t *scanner) _PAPPL_PUBLIC;


#  ifdef __cplusplus
}
#  endif // __cplusplus
#endif // !_PAPPL_SCANNER_H_
