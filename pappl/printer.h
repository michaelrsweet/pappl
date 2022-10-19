//
// Public printer header file for the Printer Application Framework
//
// Copyright © 2019-2022 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#ifndef _PAPPL_PRINTER_H_
#  define _PAPPL_PRINTER_H_
#  include "base.h"
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
#  define PAPPL_MAX_RESOLUTION	4	// Maximum number of printer resolutions
#  define PAPPL_MAX_SOURCE	16	// Maximum number of sources/rolls
#  define PAPPL_MAX_SUPPLY	32	// Maximum number of supplies
#  define PAPPL_MAX_TYPE	32	// Maximum number of media types
#  define PAPPL_MAX_VENDOR	32	// Maximum number of vendor extension attributes


//
// Constants...
//

enum pappl_color_mode_e			// IPP "print-color-mode" bit values
{
  PAPPL_COLOR_MODE_AUTO = 0x0001,		// 'auto' - Automatic color/monochrome print mode
  PAPPL_COLOR_MODE_AUTO_MONOCHROME = 0x0002,	// 'auto-monochrome' - Automatic monochrome/process monochrome print mode
  PAPPL_COLOR_MODE_BI_LEVEL = 0x0004,		// 'bi-level' - B&W (threshold) print mode
  PAPPL_COLOR_MODE_COLOR = 0x0008,		// 'color' - Full color print mode
  PAPPL_COLOR_MODE_MONOCHROME = 0x0010,		// 'monochrome' - Grayscale print mode using 1 color
  PAPPL_COLOR_MODE_PROCESS_MONOCHROME = 0x0020	// 'process-monochrome' - Grayscale print mode using multiple colors
};
typedef unsigned pappl_color_mode_t;	// Bitfield for IPP "print-color-mode" values

enum pappl_content_e			// IPP "print-content-optimize" bit values
{
  PAPPL_CONTENT_AUTO = 0x01,			// 'auto': Automatically determine content
  PAPPL_CONTENT_GRAPHIC = 0x02,			// 'graphic': Optimize for vector graphics
  PAPPL_CONTENT_PHOTO = 0x04,			// 'photo': Optimize for photos/continuous tone images
  PAPPL_CONTENT_TEXT = 0x08,			// 'text': Optimize for text
  PAPPL_CONTENT_TEXT_AND_GRAPHIC = 0x10		// 'text-and-graphic': Optimize for text and vector graphics
};
typedef unsigned pappl_content_t;	// Bitfield for IPP "print-content-optimize" values

typedef enum pappl_duplex_e		// Duplex printing support
{
  PAPPL_DUPLEX_NONE,				// No duplex printing support
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

enum pappl_identify_actions_e		// IPP "identify-actions" bit values
{
  PAPPL_IDENTIFY_ACTIONS_NONE = 0x0000,		// No actions
  PAPPL_IDENTIFY_ACTIONS_DISPLAY = 0x0001,	// 'display': Display a message
  PAPPL_IDENTIFY_ACTIONS_FLASH = 0x0002,	// 'flash': Flash the display or a light
  PAPPL_IDENTIFY_ACTIONS_SOUND = 0x0004,	// 'sound': Make a sound
  PAPPL_IDENTIFY_ACTIONS_SPEAK = 0x0008		// 'speak': Speak a message
};
typedef unsigned pappl_identify_actions_t;
					// Bitfield for IPP "identify-actions" values

enum pappl_kind_e			// IPP "printer-kind" bit values
{
  PAPPL_KIND_DISC = 0x0001,			// 'disc'
  PAPPL_KIND_DOCUMENT = 0x0002,			// 'document'
  PAPPL_KIND_ENVELOPE = 0x0004,			// 'envelope'
  PAPPL_KIND_LABEL = 0x0008,			// 'label'
  PAPPL_KIND_LARGE_FORMAT = 0x0010,		// 'large-format'
  PAPPL_KIND_PHOTO = 0x0020,			// 'photo'
  PAPPL_KIND_POSTCARD = 0x0040,			// 'postcard'
  PAPPL_KIND_RECEIPT = 0x0080,			// 'receipt'
  PAPPL_KIND_ROLL = 0x0100			// 'roll'
};
typedef unsigned pappl_kind_t;		// Bitfield for IPP "printer-kind" values

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

enum pappl_preason_e			// IPP "printer-state-reasons" bit values
{
  PAPPL_PREASON_NONE = 0x0000,			// 'none'
  PAPPL_PREASON_OTHER = 0x0001,			// 'other'
  PAPPL_PREASON_COVER_OPEN = 0x0002,		// 'cover-open'
  PAPPL_PREASON_INPUT_TRAY_MISSING = 0x0004,	// 'input-tray-missing'
  PAPPL_PREASON_MARKER_SUPPLY_EMPTY = 0x0008,	// 'marker-supply-empty'
  PAPPL_PREASON_MARKER_SUPPLY_LOW = 0x0010,	// 'marker-supply-low'
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
  PAPPL_PREASON_DOOR_OPEN = 0x8000,		// 'door-open'
  PAPPL_PREASON_IDENTIFY_PRINTER_REQUESTED = 0x10000,
						// 'identify-printer-requested'
  PAPPL_PREASON_DEVICE_STATUS = 0xefff		// Supported @link papplDeviceGetStatus@ bits
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
  PAPPL_PWG_RASTER_TYPE_RGB_16 = 0x0100,	// 16-bit per component (device) RGB
  PAPPL_PWG_RASTER_TYPE_SGRAY_8 = 0x0200,	// 8-bit grayscale with 2.2 gamma
  PAPPL_PWG_RASTER_TYPE_SGRAY_16 = 0x0400,	// 16-bit grayscale with 2.2 gamma
  PAPPL_PWG_RASTER_TYPE_SRGB_8 = 0x0800,	// 8-bit per component sRGB
  PAPPL_PWG_RASTER_TYPE_SRGB_16 = 0x1000	// 16-bit per component sRGB
};
typedef unsigned pappl_raster_type_t;	// Bitfield for IPP "pwg-raster-document-type-supported" values

enum pappl_scaling_e			// IPP "print-scaling" bit values
{
  PAPPL_SCALING_AUTO = 0x01,			// 'auto': Scale to fit (non-borderless) or fill (borderless) if larger, otherwise center
  PAPPL_SCALING_AUTO_FIT = 0x02,		// 'auto-fit': Scale to fit if larger, otherwise center
  PAPPL_SCALING_FILL = 0x04,			// 'fill': Scale to fill the media
  PAPPL_SCALING_FIT = 0x08,			// 'fit': Scale to fit within margins
  PAPPL_SCALING_NONE = 0x10			// 'none': No scaling (center/crop)
};
typedef unsigned pappl_scaling_t;	// Bitfield for IPP "print-scaling" values

enum pappl_sides_e			// IPP "sides" bit values
{
  PAPPL_SIDES_ONE_SIDED = 0x01,			// 'one-sided'
  PAPPL_SIDES_TWO_SIDED_LONG_EDGE = 0x02,	// 'two-sided-long-edge'
  PAPPL_SIDES_TWO_SIDED_SHORT_EDGE = 0x04,	// 'two-sided-short-edge'
};
typedef unsigned pappl_sides_t;		// Bitfield for IPP "sides" values

enum pappl_uoptions_e			// USB gadget options
{
  PAPPL_UOPTIONS_NONE = 0,			// No options (just USB printer)
  PAPPL_UOPTIONS_ETHERNET = 0x01,		// Include USB ethernet gadget
  PAPPL_UOPTIONS_SERIAL = 0x02,			// Include USB serial gadget
  PAPPL_UOPTIONS_STORAGE = 0x04,		// Include USB mass storage gadget
  PAPPL_UOPTIONS_STORAGE_READONLY = 0x08,	// USB mass storage gadget is read-only
  PAPPL_UOPTIONS_STORAGE_REMOVABLE = 0x10	// USB mass storage gadget is removable
};

typedef unsigned pappl_uoptions_t;	// USB gadget options bitfield

//
// Callback functions...
//

typedef void (*pappl_default_cb_t)(ipp_attribute_t *attr, void *data);
					// papplIterateDefaults callback

typedef void (*pappl_job_cb_t)(pappl_job_t *job, void *data);
					// papplIterateXxxJobs callback function

typedef void (*pappl_pr_delete_cb_t)(pappl_printer_t *printer, pappl_pr_driver_data_t *data);
					// Printer deletion callback
typedef void (*pappl_pr_identify_cb_t)(pappl_printer_t *printer, pappl_identify_actions_t actions, const char *message);
					// Identify-Printer callback
typedef bool (*pappl_pr_printfile_cb_t)(pappl_job_t *job, pappl_pr_options_t *options, pappl_device_t *device);
					// Print a "raw" job callback
typedef bool (*pappl_pr_rendjob_cb_t)(pappl_job_t *job, pappl_pr_options_t *options, pappl_device_t *device);
					// End a raster job callback
typedef bool (*pappl_pr_rendpage_cb_t)(pappl_job_t *job, pappl_pr_options_t *options, pappl_device_t *device, unsigned page);
					// End a raster page callback
typedef bool (*pappl_pr_rstartjob_cb_t)(pappl_job_t *job, pappl_pr_options_t *options, pappl_device_t *device);
					// Start a raster job callback
typedef bool (*pappl_pr_rstartpage_cb_t)(pappl_job_t *job, pappl_pr_options_t *options, pappl_device_t *device, unsigned page);
					// Start a raster page callback
typedef bool (*pappl_pr_rwriteline_cb_t)(pappl_job_t *job, pappl_pr_options_t *options, pappl_device_t *device, unsigned y, const unsigned char *line);
					// Write a line of raster graphics callback
typedef bool (*pappl_pr_status_cb_t)(pappl_printer_t *printer);
					// Update printer status callback
typedef const char *(*pappl_pr_testpage_cb_t)(pappl_printer_t *printer, char *buffer, size_t bufsize);
					// Print a test page callback
typedef ssize_t (*pappl_pr_usb_cb_t)(pappl_printer_t *printer, pappl_device_t *device, void *buffer, size_t bufsize, size_t bytes, void *data);
					// Raw USB IO callback


//
// Structures...
//

typedef struct pappl_icon_s 		// Printer PNG icon structure
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

struct pappl_pr_options_s		// Combined print job options
{
#  if CUPS_VERSION_MAJOR < 3
  cups_page_header2_t	header;			// Raster header
#  else
  cups_page_header_t	header;			// Raster header
#  endif // CUPS_VERSION_MAJOR < 3
  unsigned		num_pages;		// Number of pages in job
  unsigned		first_page;		// First page in page-ranges, starting at 1
  unsigned		last_page;		// Last page in page-ranges, starting at 1
  pappl_dither_t	dither;			// Dither array, if any
  int			copies;	 		// "copies" value
  pappl_finishings_t	finishings;		// "finishings" value(s)
  pappl_media_col_t	media;			// "media"/"media-col" value
  ipp_orient_t		orientation_requested;	// "orientation-requested" value
  char			output_bin[64];		// "output-bin" value
  pappl_color_mode_t	print_color_mode;	// "print-color-mode" value
  pappl_content_t	print_content_optimize;	// "print-content-optimize" value
  int			print_darkness;		// "print-darkness" value
  int			darkness_configured;	// "printer-darkness-configured" value
  ipp_quality_t		print_quality;		// "print-quality" value
  pappl_scaling_t	print_scaling;		// "print-scaling" value
  int			print_speed;		// "print-speed" value
  int			printer_resolution[2];	// "printer-resolution" value in dots per inch
  pappl_sides_t		sides;			// "sides" value
  int			num_vendor;		// Number of vendor options
  cups_option_t		*vendor;		// Vendor options
};

struct pappl_pr_driver_data_s		// Printer driver data
{
  void				*extension;	// Extension data (managed by driver)
  pappl_pr_delete_cb_t		delete_cb;	// Printer deletion callback
  pappl_pr_identify_cb_t	identify_cb;	// Identify-Printer callback
  pappl_pr_printfile_cb_t	printfile_cb;	// Print (raw) file callback
  pappl_pr_rendjob_cb_t		rendjob_cb;	// End raster job callback
  pappl_pr_rendpage_cb_t	rendpage_cb;	// End raster page callback
  pappl_pr_rstartjob_cb_t	rstartjob_cb;	// Start raster job callback
  pappl_pr_rstartpage_cb_t	rstartpage_cb;	// Start raster page callback
  pappl_pr_rwriteline_cb_t	rwriteline_cb;	// Write raster line callback
  pappl_pr_status_cb_t		status_cb;	// Status callback
  pappl_pr_testpage_cb_t	testpage_cb;	// Test page callback

  pappl_dither_t	gdither;		// 'auto', 'text', and 'graphic' dither array
  pappl_dither_t	pdither;		// 'photo' dither array
  const char		*format;		// Printer-specific format
  char			make_and_model[128];	// "printer-make-and-model" value
  int			ppm;			// "pages-per-minute" value
  int			ppm_color;		// "pages-per-minute-color" value, if any
  pappl_icon_t		icons[3];		// "printer-icons" values
  pappl_kind_t		kind;			// "printer-kind" values
  bool			has_supplies;		// Printer has supplies to report
  bool			input_face_up;		// Does input media come in face-up?
  bool			output_face_up;		// Does output media come out face-up?
  ipp_orient_t		orient_default;		// "orientation-requested-default" value
  pappl_color_mode_t	color_supported;	// "print-color-mode" values
  pappl_color_mode_t	color_default;		// "print-color-mode-default" value
  pappl_content_t	content_default;	// "print-content-default" value
  ipp_quality_t		quality_default;	// "print-quality-default" value
  pappl_scaling_t	scaling_default;	// "print-scaling-default" value
  pappl_raster_type_t	raster_types;		// "pwg-raster-document-type-supported" values
  pappl_raster_type_t	force_raster_type;	// Force a particular raster type?
  pappl_duplex_t	duplex;			// Duplex printing modes supported
  pappl_sides_t		sides_supported;	// "sides-supported" values
  pappl_sides_t		sides_default;		// "sides-default" value
  pappl_finishings_t	finishings;		// "finishings-supported" values
  int			num_resolution;		// Number of printer resolutions
  int			x_resolution[PAPPL_MAX_RESOLUTION];
						// Horizontal printer resolutions
  int			y_resolution[PAPPL_MAX_RESOLUTION];
						// Vertical printer resolutions
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
  int			speed_supported[2];	// print-speed-supported (0,0 for none)
  int			speed_default;		// print-speed-default
  int			darkness_default;	// print-darkness-default
  int			darkness_configured;	// printer-darkness-configured
  int			darkness_supported;	// printer/print-darkness-supported (0 for none)
  pappl_identify_actions_t identify_default;	// "identify-actions-default" values
  pappl_identify_actions_t identify_supported;	// "identify-actions-supported" values
  int			num_features;		// Number of "ipp-features-supported" values
  const char		*features[PAPPL_MAX_VENDOR];
						// "ipp-features-supported" values
  int			num_vendor;		// Number of vendor attributes
  const char		*vendor[PAPPL_MAX_VENDOR];
						// Vendor attribute names
};



//
// Functions...
//

extern void		papplPrinterAddLink(pappl_printer_t *printer, const char *label, const char *path_or_url, pappl_loptions_t options) _PAPPL_PUBLIC;

extern void		papplPrinterCancelAllJobs(pappl_printer_t *printer) _PAPPL_PUBLIC;

extern void		papplPrinterCloseDevice(pappl_printer_t *printer) _PAPPL_PUBLIC;

extern pappl_printer_t	*papplPrinterCreate(pappl_system_t *system, int printer_id, const char *printer_name, const char *driver_name, const char *device_id, const char *device_uri) _PAPPL_PUBLIC;
extern void		papplPrinterDelete(pappl_printer_t *printer) _PAPPL_PUBLIC;
extern void		papplPrinterDisable(pappl_printer_t *printer) _PAPPL_PUBLIC;
extern void		papplPrinterEnable(pappl_printer_t *printer) _PAPPL_PUBLIC;

extern pappl_job_t	*papplPrinterFindJob(pappl_printer_t *printer, int job_id) _PAPPL_PUBLIC;

extern pappl_contact_t	*papplPrinterGetContact(pappl_printer_t *printer, pappl_contact_t *contact) _PAPPL_PUBLIC;
extern const char	*papplPrinterGetDeviceID(pappl_printer_t *printer) _PAPPL_PUBLIC;
extern const char	*papplPrinterGetDeviceURI(pappl_printer_t *printer) _PAPPL_PUBLIC;
extern char		*papplPrinterGetDNSSDName(pappl_printer_t *printer, char *buffer, size_t bufsize) _PAPPL_PUBLIC;
extern ipp_t		*papplPrinterGetDriverAttributes(pappl_printer_t *printer) _PAPPL_PUBLIC;
extern pappl_pr_driver_data_t *papplPrinterGetDriverData(pappl_printer_t *printer, pappl_pr_driver_data_t *data) _PAPPL_PUBLIC;
extern const char	*papplPrinterGetDriverName(pappl_printer_t *printer) _PAPPL_PUBLIC;
extern char		*papplPrinterGetGeoLocation(pappl_printer_t *printer, char *buffer, size_t bufsize) _PAPPL_PUBLIC;
extern int		papplPrinterGetID(pappl_printer_t *printer) _PAPPL_PUBLIC;
extern int		papplPrinterGetImpressionsCompleted(pappl_printer_t *printer) _PAPPL_PUBLIC;
extern char		*papplPrinterGetLocation(pappl_printer_t *printer, char *buffer, size_t bufsize) _PAPPL_PUBLIC;
extern int		papplPrinterGetMaxActiveJobs(pappl_printer_t *printer) _PAPPL_PUBLIC;
extern int		papplPrinterGetMaxCompletedJobs(pappl_printer_t *printer) _PAPPL_PUBLIC;
extern int		papplPrinterGetMaxPreservedJobs(pappl_printer_t *printer) _PAPPL_PUBLIC;
extern const char	*papplPrinterGetName(pappl_printer_t *printer) _PAPPL_PUBLIC;
extern int		papplPrinterGetNextJobID(pappl_printer_t *printer) _PAPPL_PUBLIC;
extern int		papplPrinterGetNumberOfActiveJobs(pappl_printer_t *printer) _PAPPL_PUBLIC;
extern int		papplPrinterGetNumberOfCompletedJobs(pappl_printer_t *printer) _PAPPL_PUBLIC;
extern int		papplPrinterGetNumberOfJobs(pappl_printer_t *printer) _PAPPL_PUBLIC;
extern char		*papplPrinterGetOrganization(pappl_printer_t *printer, char *buffer, size_t bufsize) _PAPPL_PUBLIC;
extern char		*papplPrinterGetOrganizationalUnit(pappl_printer_t *printer, char *buffer, size_t bufsize) _PAPPL_PUBLIC;
extern char		*papplPrinterGetPath(pappl_printer_t *printer, const char *subpath, char *buffer, size_t bufsize) _PAPPL_PUBLIC;
extern char		*papplPrinterGetPrintGroup(pappl_printer_t *printer, char *buffer, size_t bufsize) _PAPPL_PUBLIC;
extern pappl_preason_t	papplPrinterGetReasons(pappl_printer_t *printer) _PAPPL_PUBLIC;
extern ipp_pstate_t	papplPrinterGetState(pappl_printer_t *printer) _PAPPL_PUBLIC;
extern int		papplPrinterGetSupplies(pappl_printer_t *printer, int max_supplies, pappl_supply_t *supplies) _PAPPL_PUBLIC;
extern pappl_system_t	*papplPrinterGetSystem(pappl_printer_t *printer) _PAPPL_PUBLIC;

extern void		papplPrinterHTMLFooter(pappl_client_t *client) _PAPPL_PUBLIC;
extern void		papplPrinterHTMLHeader(pappl_client_t *client, const char *title, int refresh) _PAPPL_PUBLIC;

extern bool		papplPrinterIsAcceptingJobs(pappl_printer_t *printer) _PAPPL_PUBLIC;

extern void		papplPrinterIterateActiveJobs(pappl_printer_t *printer, pappl_job_cb_t cb, void *data, int first_index, int limit) _PAPPL_PUBLIC;
extern void		papplPrinterIterateAllJobs(pappl_printer_t *printer, pappl_job_cb_t cb, void *data, int first_index, int limit) _PAPPL_PUBLIC;
extern void		papplPrinterIterateCompletedJobs(pappl_printer_t *printer, pappl_job_cb_t cb, void *data, int first_index, int limit) _PAPPL_PUBLIC;
extern pappl_device_t	*papplPrinterOpenDevice(pappl_printer_t *printer) _PAPPL_PUBLIC;
extern int		papplPrinterOpenFile(pappl_printer_t *printer, char *fname, size_t fnamesize, const char *directory, const char *resname, const char *ext, const char *mode) _PAPPL_PUBLIC;
extern void		papplPrinterPause(pappl_printer_t *printer) _PAPPL_PUBLIC;
extern void		papplPrinterRemoveLink(pappl_printer_t *printer, const char *label) _PAPPL_PUBLIC;
extern void		papplPrinterResume(pappl_printer_t *printer) _PAPPL_PUBLIC;
extern void		papplPrinterSetContact(pappl_printer_t *printer, pappl_contact_t *contact) _PAPPL_PUBLIC;
extern void		papplPrinterSetServiceContact(pappl_printer_t *printer, pappl_contact_t *service_contact) _PAPPL_PUBLIC;
extern void		papplPrinterSetDNSSDName(pappl_printer_t *printer, const char *value) _PAPPL_PUBLIC;
extern bool		papplPrinterSetDriverData(pappl_printer_t *printer, pappl_pr_driver_data_t *data, ipp_t *attrs) _PAPPL_PUBLIC;
extern bool		papplPrinterSetDriverDefaults(pappl_printer_t *printer, pappl_pr_driver_data_t *data, int num_vendor, cups_option_t *vendor) _PAPPL_PUBLIC;
extern void		papplPrinterSetGeoLocation(pappl_printer_t *printer, const char *value) _PAPPL_PUBLIC;
extern void		papplPrinterSetImpressionsCompleted(pappl_printer_t *printer, int add) _PAPPL_PUBLIC;
extern void		papplPrinterSetLocation(pappl_printer_t *printer, const char *value) _PAPPL_PUBLIC;
extern void		papplPrinterSetMaxActiveJobs(pappl_printer_t *printer, int max_active_jobs) _PAPPL_PUBLIC;
extern void		papplPrinterSetMaxCompletedJobs(pappl_printer_t *printer, int max_completed_jobs) _PAPPL_PUBLIC;
extern void		papplPrinterSetMaxPreservedJobs(pappl_printer_t *printer, int max_preserved_jobs) _PAPPL_PUBLIC;
extern void		papplPrinterSetNextJobID(pappl_printer_t *printer, int next_job_id) _PAPPL_PUBLIC;
extern void		papplPrinterSetOrganization(pappl_printer_t *printer, const char *value) _PAPPL_PUBLIC;
extern void		papplPrinterSetOrganizationalUnit(pappl_printer_t *printer, const char *value) _PAPPL_PUBLIC;
extern void		papplPrinterSetPrintGroup(pappl_printer_t *printer, const char *value) _PAPPL_PUBLIC;
extern bool		papplPrinterSetReadyMedia(pappl_printer_t *printer, int num_ready, pappl_media_col_t *ready) _PAPPL_PUBLIC;
extern void		papplPrinterSetReasons(pappl_printer_t *printer, pappl_preason_t add, pappl_preason_t remove) _PAPPL_PUBLIC;
extern void		papplPrinterSetSupplies(pappl_printer_t *printer, int num_supplies, pappl_supply_t *supplies) _PAPPL_PUBLIC;
extern void		papplPrinterSetUSB(pappl_printer_t *printer, unsigned vendor_id, unsigned product_id, pappl_uoptions_t options, const char *storagefile, pappl_pr_usb_cb_t usb_cb, void *usb_data) _PAPPL_PUBLIC;


#  ifdef __cplusplus
}
#  endif // __cplusplus
#endif // !_PAPPL_PRINTER_H_
