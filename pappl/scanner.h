//
// Scanner header file for the Scanner Application Framework
//
// Copyright © 2019-2020 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#ifndef _PAPPL_PRINTER_H_
#  define _PAPPL_PRINTER_H_


//
// Include necessary headers...
//

#  include "base.h"


//
// C++ magic...
//

#  ifdef __cplusplus
extern "C" {
#  endif // __cplusplus


//
// Limits...
//

#  define PAPPL_MAX_BIN		16	// Maximum number of output bins
#  define PAPPL_MAX_COLOR_MODE	8	// Maximum number of color modes
#  define PAPPL_MAX_MEDIA	256	// Maximum number of media sizes
#  define PAPPL_MAX_RASTER_TYPE	16	// Maximum number of raster types
#  define PAPPL_MAX_RESOLUTION	4	// Maximum number of scanner resolutions
#  define PAPPL_MAX_SOURCE	16	// Maximum number of sources/rolls
#  define PAPPL_MAX_TYPE	32	// Maximum number of media types
#  define PAPPL_MAX_VENDOR	32	// Maximum number of vendor extension attributes


//
// Constants...
//

enum pappl_color_mode_e			// IPP "scan-color-mode" bit values
{
  PAPPL_COLOR_MODE_AUTO = 0x0001,		// 'auto' - Automatic color/monochrome scan mode
  PAPPL_COLOR_MODE_AUTO_MONOCHROME = 0x0002,	// 'auto-monochrome' - Automatic monochrome/process monochrome scan mode
  PAPPL_COLOR_MODE_BI_LEVEL = 0x0004,		// 'bi-level' - B&W (threshold) scan mode
  PAPPL_COLOR_MODE_COLOR = 0x0008,		// 'color' - Full color scan mode
  PAPPL_COLOR_MODE_MONOCHROME = 0x0010,		// 'monochrome' - Grayscale scan mode using 1 color
  PAPPL_COLOR_MODE_PROCESS_MONOCHROME = 0x0020	// 'process-monochrome' - Grayscale scan mode using multiple colors
};
typedef unsigned pappl_color_mode_t;	// Bitfield for IPP "scan-color-mode" values

typedef enum pappl_duplex_e		// Duplex scanning support
{
  PAPPL_DUPLEX_NONE,				// No duplex scanning support
  PAPPL_DUPLEX_NORMAL,				// Duplex with normal back sides
  PAPPL_DUPLEX_FLIPPED,				// Duplex with flipped back sides
  PAPPL_DUPLEX_ROTATED,				// Duplex with back side rotated 180 degrees for long-edge duplex
  PAPPL_DUPLEX_MANUAL_TUMBLE			// Duplex with back side rotated 180 degrees for short-edge duplex
} pappl_duplex_t;

enum pappl_finishings_e			// IPP "finishings" bit values
{
  PAPPL_FINISHINGS_NONE = 0x0000,		// 'none'
  PAPPL_FINISHINGS_PUNCH = 0x0001,		// 'punch'
  PAPPL_FINISHINGS_STAPLE = 0x0002,		// 'staple'
  PAPPL_FINISHINGS_TRIM = 0x0004		// 'trim'
};
typedef unsigned pappl_finishings_t;	// Bitfield for IPP "finishings" values

enum pappl_label_mode_e			// IPP "label-mode-xxx" bit values
{
  PAPPL_LABEL_MODE_APPLICATOR = 0x0001,		// 'applicator'
  PAPPL_LABEL_MODE_CUTTER = 0x0002,		// 'cutter'
  PAPPL_LABEL_MODE_CUTTER_DELAYED = 0x0004,	// 'cutter-delayed'
  PAPPL_LABEL_MODE_KIOSK = 0x0008,		// 'kiosk'
  PAPPL_LABEL_MODE_PEEL_OFF = 0x0010,		// 'peel-off'
  PAPPL_LABEL_MODE_PEEL_OFF_PREPEEL = 0x0020,	// 'peel-off-prepeel'
  PAPPL_LABEL_MODE_REWIND = 0x0040,		// 'rewind'
  PAPPL_LABEL_MODE_RFID = 0x0080,		// 'rfid'
  PAPPL_LABEL_MODE_TEAR_OFF = 0x0100		// 'tear-off'
};
typedef unsigned short pappl_label_mode_t;
					// Bitfield for IPP "label-mode-xxx" values

enum pappl_media_tracking_e		// IPP "media-tracking" bit values
{
  PAPPL_MEDIA_TRACKING_CONTINUOUS = 0x0001,	// 'continuous'
  PAPPL_MEDIA_TRACKING_GAP = 0x0002,		// 'gap'
  PAPPL_MEDIA_TRACKING_MARK = 0x0004,		// 'mark'
  PAPPL_MEDIA_TRACKING_WEB = 0x0008		// 'web'
};
typedef unsigned short pappl_media_tracking_t;
					// Bitfield for IPP "media-tracking" values

enum pappl_preason_e			// IPP "scanner-state-reasons" bit values
{
  PAPPL_PREASON_NONE = 0x0000,			// 'none'
  PAPPL_PREASON_OTHER = 0x0001,			// 'other'
  PAPPL_PREASON_COVER_OPEN = 0x0002,		// 'cover-open'
  PAPPL_PREASON_INPUT_TRAY_MISSING = 0x0004,	// 'input-tray-missing'
  PAPPL_PREASON_MARKER_WASTE_ALMOST_FULL = 0x0020,
						// 'marker-waste-almost-full'
  PAPPL_PREASON_MARKER_WASTE_FULL = 0x0040,	// 'marker-waste-full'
  PAPPL_PREASON_MEDIA_EMPTY = 0x0080,		// 'media-empty'
  PAPPL_PREASON_MEDIA_JAM = 0x0100,		// 'media-jam'
  PAPPL_PREASON_MEDIA_LOW = 0x0200,		// 'media-low'
  PAPPL_PREASON_MEDIA_NEEDED = 0x0400,		// 'media-needed'
  PAPPL_PREASON_OFFLINE = 0x0800,		// 'offline'
  PAPPL_PREASON_SPOOL_AREA_FULL = 0x1000,	// 'spool-area-full'
  PAPPL_PREASON_TONER_EMPTY = 0x2000,		// 'toner-empty'
  PAPPL_PREASON_TONER_LOW = 0x4000,		// 'toner-low'

  PAPPL_PREASON_DEVICE_STATUS = 0x6fff		// Supported @link papplDeviceGetStatus@ bits
};

enum pappl_raster_type_e		// IPP "pwg-raster-document-type-supported" bit values
{
  PAPPL_PWG_RASTER_TYPE_NONE = 0x0000,		// Do not force a particular raster type
  PAPPL_PWG_RASTER_TYPE_ADOBE_RGB_8 = 0x0001,	// 8-bit per component AdobeRGB
  PAPPL_PWG_RASTER_TYPE_ADOBE_RGB_16 = 0x0002,	// 16-bit per component AdobeRGB
  PAPPL_PWG_RASTER_TYPE_BLACK_1 = 0x0004,	// 1-bit (device) black
  PAPPL_PWG_RASTER_TYPE_BLACK_8 = 0x0008,	// 8-bit (device) black
  PAPPL_PWG_RASTER_TYPE_BLACK_16 = 0x0010,	// 16-bit (device) black
  PAPPL_PWG_RASTER_TYPE_CMYK_8 = 0x0020,	// 8-bit per component (device) CMYK
  PAPPL_PWG_RASTER_TYPE_CMYK_16 = 0x0040,	// 16-bit per component (device) CMYK
  PAPPL_PWG_RASTER_TYPE_RGB_8 = 0x0080,		// 8-bit per component (device) RGB
  PAPPL_PWG_RASTER_TYPE_RGB_16 = 0x0100,	// 16-bit per component (device) RGB)
  PAPPL_PWG_RASTER_TYPE_SGRAY_8 = 0x0200,	// 8-bit grayscale with 2.2 gamma
  PAPPL_PWG_RASTER_TYPE_SGRAY_16 = 0x0400,	// 16-bit grayscale with 2.2 gamma
  PAPPL_PWG_RASTER_TYPE_SRGB_8 = 0x0800,	// 8-bit per component sRGB
  PAPPL_PWG_RASTER_TYPE_SRGB_16 = 0x1000	// 16-bit per component sRGB
};
typedef unsigned pappl_raster_type_t;	// Bitfield for IPP "pwg-raster-document-type-supported" values

enum pappl_scaling_e			// IPP "scan-scaling" bit values
{
  PAPPL_SCALING_AUTO = 0x01,			// 'auto': Scale to fit (non-borderless) or fill (borderless) if larger, otherwise center
  PAPPL_SCALING_AUTO_FIT = 0x02,		// 'auto-fit': Scale to fit if larger, otherwise center
  PAPPL_SCALING_FILL = 0x04,			// 'fill': Scale to fill the media
  PAPPL_SCALING_FIT = 0x08,			// 'fit': Scale to fit within margins
  PAPPL_SCALING_NONE = 0x10			// 'none': No scaling (center/crop)
};
typedef unsigned pappl_scaling_t;	// Bitfield for IPP "scan-scaling" values

enum pappl_sides_e			// IPP "sides" bit values
{
  PAPPL_SIDES_ONE_SIDED = 0x01,			// 'one-sided'
  PAPPL_SIDES_TWO_SIDED_LONG_EDGE = 0x02,	// 'two-sided-long-edge'
  PAPPL_SIDES_TWO_SIDED_SHORT_EDGE = 0x04,	// 'two-sided-short-edge'
};
typedef unsigned pappl_sides_t;		// Bitfield for IPP "sides" values


//
// Callback functions...
//

typedef void (*pappl_default_cb_t)(ipp_attribute_t *attr, void *data);
					// papplIterateDefaults callback

typedef void (*pappl_job_cb_t)(pappl_job_t *job, void *data);
					// papplIterateXxxJobs callback function

typedef void (*pappl_sc_delete_cb_t)(pappl_scanner_t *scanner, pappl_sc_driver_data_t *data);
					// Scanner deletion callback

typedef bool (*pappl_sc_rendjob_cb_t)(pappl_job_t *job, pappl_sc_options_t *options, pappl_device_t *device);
					// End a raster job callback
typedef bool (*pappl_sc_rendpage_cb_t)(pappl_job_t *job, pappl_sc_options_t *options, pappl_device_t *device, unsigned page);
					// End a raster page callback
typedef bool (*pappl_sc_rstartjob_cb_t)(pappl_job_t *job, pappl_sc_options_t *options, pappl_device_t *device);
					// Start a raster job callback
typedef bool (*pappl_sc_rstartpage_cb_t)(pappl_job_t *job, pappl_sc_options_t *options, pappl_device_t *device, unsigned page);
					// Start a raster page callback
typedef bool (*pappl_sc_rwriteline_cb_t)(pappl_job_t *job, pappl_sc_options_t *options, pappl_device_t *device, unsigned y, const unsigned char *line);
					// Write a line of raster graphics callback
typedef bool (*pappl_sc_status_cb_t)(pappl_scanner_t *scanner);
					// Update scanner status callback
typedef const char *(*pappl_sc_testpage_cb_t)(pappl_scanner_t *scanner, char *buffer, size_t bufsize);
					// Scan a test page callback


//
// Structures...
//

typedef struct pappl_icon_s 		// Scanner PNG icon structure
{
  char		filename[256];			// External filename, if any
  const void	*data;				// PNG icon data, if any
  size_t	datalen;			// Size of PNG icon data
} pappl_icon_t;

typedef struct pappl_media_col_s	// Media details structure
{
  int			bottom_margin;		// Bottom margin in hundredths of millimeters
  int			left_margin;		// Left margin in hundredths of millimeters
  int			left_offset;		// Left offset in hundredths of millimeters
  int			right_margin;		// Right margin in hundredths of millimeters
  int			size_width;		// Width in hundredths of millimeters
  int			size_length;		// Height in hundredths of millimeters
  char			size_name[64];		// PWG media size name
  char			source[64];		// PWG media source name
  int			top_margin;		// Top margin in hundredths of millimeters
  int			top_offset;		// Top offset in hundredths of millimeters
  pappl_media_tracking_t tracking;		// Media tracking
  char			type[64];		// PWG media type name
} pappl_media_col_t;

struct pappl_sc_options_s		// Combined scan job options
{
  cups_page_header2_t	header;			// Raster header
  unsigned		num_pages;		// Number of pages in job
  unsigned		first_page;		// First page in page-ranges, starting at 1
  unsigned		last_page;		// Last page in page-ranges, starting at 1
  pappl_dither_t	dither;			// Dither array, if any
  int			copies;	 		// "copies" value
  pappl_finishings_t	finishings;		// "finishings" value(s)
  pappl_media_col_t	media;			// "media"/"media-col" value
  ipp_orient_t		orientation_requested;	// "orientation-requested" value
  char			output_bin[64];		// "output-bin" value
  pappl_color_mode_t	scan_color_mode;	// "scan-color-mode" value
  int			scan_darkness;		// "scan-darkness" value
  int			darkness_configured;	// "scanner-darkness-configured" value
  ipp_quality_t		scan_quality;		// "scan-quality" value
  pappl_scaling_t	scan_scaling;		// "scan-scaling" value
  int			scan_speed;		// "scan-speed" value
  int			scanner_resolution[2];	// "scanner-resolution" value in dots per inch
  pappl_sides_t		sides;			// "sides" value
};

struct pappl_sc_driver_data_s		// Scanner driver data
{
  void				*extension;	// Extension data (managed by driver)
  pappl_sc_delete_cb_t		delete_cb;	// Scanner deletion callback

  pappl_sc_rendjob_cb_t		rendjob_cb;	// End raster job callback
  pappl_sc_rendpage_cb_t	rendpage_cb;	// End raster page callback
  pappl_sc_rstartjob_cb_t	rstartjob_cb;	// Start raster job callback
  pappl_sc_rstartpage_cb_t	rstartpage_cb;	// Start raster page callback
  pappl_sc_rwriteline_cb_t	rwriteline_cb;	// Write raster line callback
  pappl_sc_status_cb_t		status_cb;	// Status callback
  pappl_sc_testpage_cb_t	testpage_cb;	// Test page callback

  pappl_dither_t	gdither;		// 'auto', 'text', and 'graphic' dither array
  pappl_dither_t	pdither;		// 'photo' dither array
  const char		*format;		// Scanner-specific format
  char			make_and_model[128];	// "scanner-make-and-model" value
  int			ppm;			// "pages-per-minute" value
  int			ppm_color;		// "pages-per-minute-color" value, if any
  pappl_icon_t		icons[3];		// "scanner-icons" values
  bool			input_face_up;		// Does input media come in face-up?
  bool			output_face_up;		// Does output media come out face-up?
  ipp_orient_t		orient_default;		// "orientation-requested-default" value
  pappl_color_mode_t	color_supported;	// "scan-color-mode" values
  pappl_color_mode_t	color_default;		// "scan-color-mode-default" value
  ipp_quality_t		quality_default;	// "scan-quality-default" value
  pappl_scaling_t	scaling_default;	// "scan-scaling-default" value
  pappl_raster_type_t	raster_types;		// "pwg-raster-document-type-supported" values
  pappl_raster_type_t	force_raster_type;	// Force a particular raster type?
  pappl_duplex_t	duplex;			// Duplex scanning modes supported
  pappl_sides_t		sides_supported;	// "sides-supported" values
  pappl_sides_t		sides_default;		// "sides-default" value
  pappl_finishings_t	finishings;		// "finishings-supported" values
  int			num_resolution;		// Number of scanner resolutions
  int			x_resolution[PAPPL_MAX_RESOLUTION];
						// Horizontal scanner resolutions
  int			y_resolution[PAPPL_MAX_RESOLUTION];
						// Vertical scanner resolutions
  int			x_default;		// Default horizontal resolution
  int			y_default;		// Default vertical resolution
  bool			borderless;		// Borderless margins supported?
  int			left_right;		// Left and right margins in hundredths of millimeters
  int			bottom_top;		// Bottom and top margins in hundredths of millimeters
  int			num_media;		// Number of supported media
  const char		*media[PAPPL_MAX_MEDIA];// Supported media
  pappl_media_col_t	media_default;		// Default media
  pappl_media_col_t	media_ready[PAPPL_MAX_SOURCE];
						// Ready media
  int			num_source;		// Number of media sources (trays/rolls)
  const char		*source[PAPPL_MAX_SOURCE];
						// Media sources
  int			left_offset_supported[2];
						// media-left-offset-supported (0,0 for none)
  int			top_offset_supported[2];
						// media-top-offset-supported (0,0 for none)
  pappl_media_tracking_t tracking_supported;
						// media-tracking-supported
  int			num_type;		// Number of media types
  const char		*type[PAPPL_MAX_TYPE];	// Media types
  int			num_bin;		// Number of output bins
  const char		*bin[PAPPL_MAX_BIN];	// Output bins
  int			bin_default;		// Default output bin
  pappl_label_mode_t	mode_configured;	// label-mode-configured
  pappl_label_mode_t	mode_supported;		// label-mode-supported
  int			tear_offset_configured;	// label-tear-offset-configured
  int			tear_offset_supported[2];
						// label-tear-offset-supported (0,0 for none)
  int			speed_supported[2];	// scan-speed-supported (0,0 for none)
  int			speed_default;		// scan-speed-default
  int			darkness_default;	// scan-darkness-default
  int			darkness_configured;	// scanner-darkness-configured
  int			darkness_supported;	// scanner/scan-darkness-supported (0 for none)
  int			num_features;		// Number of "ipp-features-supported" values
};



//
// Functions...
//

extern void		papplScannerAddLink(pappl_scanner_t *scanner, const char *label, const char *path_or_url, pappl_loptions_t options) _PAPPL_PUBLIC;

extern void		papplScannerCancelAllJobs(pappl_scanner_t *scanner) _PAPPL_PUBLIC;

extern void		papplScannerCloseDevice(pappl_scanner_t *scanner) _PAPPL_PUBLIC;

extern pappl_scanner_t	*papplScannerCreate(pappl_system_t *system, int printer_id, const char *scanner_name, const char *driver_name, const char *device_id, const char *device_uri) _PAPPL_PUBLIC;
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
extern int		papplScannerGetMaxActiveJobs(pappl_scanner_t *scanner) _PAPPL_PUBLIC;
extern int		papplScannerGetMaxCompletedJobs(pappl_scanner_t *scanner) _PAPPL_PUBLIC;
extern const char	*papplScannerGetName(pappl_scanner_t *scanner) _PAPPL_PUBLIC;
extern int		papplScannerGetNextJobID(pappl_scanner_t *scanner) _PAPPL_PUBLIC;
extern int		papplScannerGetNumberOfActiveJobs(pappl_scanner_t *scanner) _PAPPL_PUBLIC;
extern int		papplScannerGetNumberOfCompletedJobs(pappl_scanner_t *scanner) _PAPPL_PUBLIC;
extern int		papplScannerGetNumberOfJobs(pappl_scanner_t *scanner) _PAPPL_PUBLIC;
extern char		*papplScannerGetOrganization(pappl_scanner_t *scanner, char *buffer, size_t bufsize) _PAPPL_PUBLIC;
extern char		*papplScannerGetOrganizationalUnit(pappl_scanner_t *scanner, char *buffer, size_t bufsize) _PAPPL_PUBLIC;
extern char		*papplScannerGetPath(pappl_scanner_t *scanner, const char *subpath, char *buffer, size_t bufsize) _PAPPL_PUBLIC;
extern char		*papplScannerGetScanGroup(pappl_scanner_t *scanner, char *buffer, size_t bufsize) _PAPPL_PUBLIC;
extern pappl_preason_t	papplScannerGetReasons(pappl_scanner_t *scanner) _PAPPL_PUBLIC;
extern ipp_pstate_t	papplScannerGetState(pappl_scanner_t *scanner) _PAPPL_PUBLIC;
extern pappl_system_t	*papplScannerGetSystem(pappl_scanner_t *scanner) _PAPPL_PUBLIC;

extern void		papplScannerHTMLFooter(pappl_client_t *client) _PAPPL_PUBLIC;
extern void		papplScannerHTMLHeader(pappl_client_t *client, const char *title, int refresh) _PAPPL_PUBLIC;

extern void		papplScannerIterateActiveJobs(pappl_scanner_t *scanner, pappl_job_cb_t cb, void *data, int first_index, int limit) _PAPPL_PUBLIC;
extern void		papplScannerIterateAllJobs(pappl_scanner_t *scanner, pappl_job_cb_t cb, void *data, int first_index, int limit) _PAPPL_PUBLIC;
extern void		papplScannerIterateCompletedJobs(pappl_scanner_t *scanner, pappl_job_cb_t cb, void *data, int first_index, int limit) _PAPPL_PUBLIC;
extern pappl_device_t	*papplScannerOpenDevice(pappl_scanner_t *scanner) _PAPPL_PUBLIC;
extern void		papplScannerPause(pappl_scanner_t *scanner) _PAPPL_PUBLIC;
extern void		papplScannerRemoveLink(pappl_scanner_t *scanner, const char *label) _PAPPL_PUBLIC;
extern void		papplScannerResume(pappl_scanner_t *scanner) _PAPPL_PUBLIC;
extern void		papplScannerSetContact(pappl_scanner_t *scanner, pappl_contact_t *contact) _PAPPL_PUBLIC;
extern void		papplScannerSetDNSSDName(pappl_scanner_t *scanner, const char *value) _PAPPL_PUBLIC;
extern bool		papplScannerSetDriverData(pappl_scanner_t *scanner, pappl_sc_driver_data_t *data, ipp_t *attrs) _PAPPL_PUBLIC;
extern bool		papplScannerSetDriverDefaults(pappl_scanner_t *scanner, pappl_sc_driver_data_t *data, int num_vendor, cups_option_t *vendor) _PAPPL_PUBLIC;
extern void		papplScannerSetGeoLocation(pappl_scanner_t *scanner, const char *value) _PAPPL_PUBLIC;
extern void		papplScannerSetImpressionsCompleted(pappl_scanner_t *scanner, int add) _PAPPL_PUBLIC;
extern void		papplScannerSetLocation(pappl_scanner_t *scanner, const char *value) _PAPPL_PUBLIC;
extern void		papplScannerSetMaxActiveJobs(pappl_scanner_t *scanner, int max_active_jobs) _PAPPL_PUBLIC;
extern void		papplScannerSetMaxCompletedJobs(pappl_scanner_t *scanner, int max_completed_jobs) _PAPPL_PUBLIC;
extern void		papplScannerSetNextJobID(pappl_scanner_t *scanner, int next_job_id) _PAPPL_PUBLIC;
extern void		papplScannerSetOrganization(pappl_scanner_t *scanner, const char *value) _PAPPL_PUBLIC;
extern void		papplScannerSetOrganizationalUnit(pappl_scanner_t *scanner, const char *value) _PAPPL_PUBLIC;
extern void		papplScannerSetScanGroup(pappl_scanner_t *scanner, const char *value) _PAPPL_PUBLIC;
extern bool		papplScannerSetReadyMedia(pappl_scanner_t *scanner, int num_ready, pappl_media_col_t *ready) _PAPPL_PUBLIC;
extern void		papplScannerSetReasons(pappl_scanner_t *scanner, pappl_preason_t add, pappl_preason_t remove) _PAPPL_PUBLIC;

//
// C++ magic...
//

#  ifdef __cplusplus
}
#  endif // __cplusplus


#endif // !_PAPPL_SCANNER_H_
