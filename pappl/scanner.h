//
// Public scanner header file for the Scanner Application Framework
//
// Copyright © 2019-2022 by Michael R Sweet.
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
#define PAPPL_MAX_FORMATS 5 // Most scanners support up to 5 different document formats
#define PAPPL_MAX_COLOR_MODES 3 // Most scanners support a few color modes: Black and White, Grayscale, Color
#define PAPPL_MAX_SOURCES 2 // MOST scanners offer two input sources: Flatbed and ADF
#define PAPPL_MAX_COLOR_SPACES 2 // Common color spaces like sRGB and AdobeRGB
#define PAPPL_MAX_MEDIA_TYPES 5 // Various media types like Plain, Photo, Card, etc.

//
// Constants...
//

typedef enum 
{
  PAPPL_SSTATE_IDLE,       // Scanner is idle
  PAPPL_SSTATE_PROCESSING, // Scanner is busy with some job or activity
  PAPPL_SSTATE_TESTING,    // Scanner is calibrating, preparing the unit
  PAPPL_SSTATE_STOPPED,    // Scanner stopped due to an error condition
  PAPPL_SSTATE_DOWN        // Scanner is unavailable
} escl_sstate_t;

enum pappl_sreason_t
{
  PAPPL_SREASON_NONE = 0x0000,             // 'none', no error, scanner is ready
  PAPPL_SREASON_IDLE = 0x0001,             // 'idle', scanner is idle
  PAPPL_SREASON_PROCESSING = 0x0002,       // 'processing', scanner is currently processing a job
  PAPPL_SREASON_TESTING = 0x0004,          // 'testing', scanner is in calibration or preparation mode
  PAPPL_SREASON_STOPPED = 0x0008,          // 'stopped', an error has occurred and the scanner has stopped
  PAPPL_SREASON_DOWN = 0x0010,             // 'down', the scanner is unavailable
};

// Define color modes supported by the scanner
typedef enum {
  BLACKANDWHITE1, // For black and white scans
  GRAYSCALE8,  // For grayscale scans
  RGB24       // For full color scans
} pappl_sc_color_mode_t;

// Define input sources for the scanner
typedef enum {
  FLATBED, // For flatbed scanners
  ADF,     // For automatic dopappl_sc_input_source_t;cument feeder
} 

//
// Callback functions...
//
typedef void (*pappl_sc_capabilities_cb_t)(pappl_scanner_t *scanner, pappl_sc_driver_data_t *data); // Callback for getting scanner capabilities
typedef void (*pappl_sc_job_create_cb_t)(pappl_job_t *job, pappl_sc_options_t *options, pappl_device_t *device); // Callback for creating a scan job
typedef void (*pappl_sc_job_delete_cb_t)(pappl_job_t *job); // Callback for deleting a scan job
typedef bool (*pappl_sc_data_cb_t)(pappl_job_t *job, pappl_device_t *device, void *buffer, size_t bufsize); // Callback for getting scan data
typedef void (*pappl_sc_status_cb_t)(pappl_scanner_t *scanner, pappl_sc_driver_data_t *data); // Callback for getting scanner status
typedef bool (*pappl_sc_job_cancel_cb_t)(pappl_job_t *job); // Callback for cancelling a scan job
typedef void (*pappl_sc_buffer_info_cb_t)(pappl_job_t *job, pappl_sc_options_t *options, pappl_device_t *device); // Callback for getting buffer information
typedef void (*pappl_sc_image_info_cb_t)(pappl_job_t *job, pappl_device_t *device, void *data); // Callback for getting image information

//
// Structures
//

typedef struct pappl_sc_options_s		// Combined scan job options
{
  char document_format[64];			// Desired output format (JPEG, PDF, TIFF)
  pappl_sc_color_mode_t color_mode;		// Color mode for the scan
  int resolution;				// Scanning resolution in DPI
  pappl_sc_input_source_t input_source;		// Selected input source
  bool duplex;					// Duplex scanning option
  struct {
  int width;					// Width of the scan area
  int height;					// Height of the scan area
  int x_offset;				// X offset for the scan area
  int y_offset;				// Y offset for the scan area
  } scan_area;
  struct {
  int brightness;				// Brightness adjustment
  int contrast;				// Contrast adjustment
  int gamma;					// Gamma adjustment
  int threshold;				// Threshold for black/white scans
  } adjustments;
  bool blank_page_removal;			// Automatically detect and remove blank pages
  unsigned num_pages;				// Number of pages to scan (for ADF)
} pappl_sc_options_t;

typedef struct pappl_sc_driver_data_s
{
  pappl_sc_capabilities_cb_t capabilities_cb; // Callback for getting scanner capabilities
  pappl_sc_job_create_cb_t job_create_cb; // Callback for creating a scan job
  pappl_sc_job_delete_cb_t job_delete_cb; // Callback for deleting a scan job
  pappl_sc_data_cb_t data_cb; // Callback for getting scan data
  pappl_sc_status_cb_t status_cb; // Callback for getting scanner status
  pappl_sc_job_cancel_cb_t job_cancel_cb; // Callback for cancelling a scan job
  pappl_sc_buffer_info_cb_t buffer_info_cb; // Callback for getting buffer information
  pappl_sc_image_info_cb_t image_info_cb; // Callback for getting image information

  char			make_and_model[128]; // Make and model of the scanner
  const char *document_formats_supported[PAPPL_MAX_FORMATS]; // Supported document formats (JPEG, PDF, TIFF)
  pappl_sc_color_mode_t color_modes_supported[PAPPL_MAX_COLOR_MODES]; // Supported color modes (BlackAndWhite1, Grayscale8, RGB24)
  int max_resolution;             // Maximum optical resolution in DPI
  pappl_sc_input_source_t input_sources_supported[PAPPL_MAX_SOURCES]; // Supported input sources (Platen, Feeder)
  bool duplex_supported;          // Duplex (double-sided) scanning support
  const char *color_spaces_supported[PAPPL_MAX_COLOR_SPACES]; // Supported color spaces (sRGB, AdobeRGB)
  int max_scan_area[2];           // Maximum scan area size (width, height)
  const char *media_type_supported[PAPPL_MAX_MEDIA_TYPES]; // Types of media that can be scanned (Plain, Photo, Card)
} pappl_sc_driver_data_t;

//
// Functions...
//

extern void		papplScannerAddLink(pappl_scanner_t *scanner, const char *label, const char *path_or_url, pappl_loptions_t options) _PAPPL_PUBLIC;

extern void		papplScannerCancelAllJobs(pappl_scanner_t *scanner) _PAPPL_PUBLIC;

extern void		papplScannerCloseDevice(pappl_scanner_t *scanner) _PAPPL_PUBLIC;

extern pappl_scanner_t	*papplScannerCreate(pappl_system_t *system, int scanner_id, const char *scanner_name, const char *driver_name, const char *device_id, const char *device_uri) _PAPPL_PUBLIC;
extern void		papplScannerDelete(pappl_Scanner_t *scanner) _PAPPL_PUBLIC;
extern void		papplScannerDisable(pappl_scanner_t *scanner) _PAPPL_PUBLIC;
extern void		papplScannerEnable(pappl_scanner_t *scanner) _PAPPL_PUBLIC;

extern pappl_job_t	*papplScannerFindJob(pappl_scanner_t *scanner, int job_id) _PAPPL_PUBLIC;

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

extern pappl_sreason_t	papplScannerGetReasons(pappl_printer_t *printer) _PAPPL_PUBLIC;
extern escl_sstate_t 	papplScannerGetState(pappl_printer_t *printer) _PAPPL_PUBLIC;

extern pappl_system_t	*papplScannerGetSystem(pappl_scanner_t *scanner) _PAPPL_PUBLIC;

extern void		papplScannerHTMLFooter(pappl_client_t *client) _PAPPL_PUBLIC;
extern void		papplScannerHTMLHeader(pappl_client_t *client, const char *title, int refresh) _PAPPL_PUBLIC;

extern bool		papplScannerIsAcceptingJobs(pappl_scanner_t *scanner) _PAPPL_PUBLIC;
extern bool		papplScannerIsDeleted(pappl_scanner_t *scanner) _PAPPL_PUBLIC;

extern bool		papplScannerIsHoldingNewJobs(pappl_scanner_t *printer) _PAPPL_PUBLIC;

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
