//
// Printer header file for the Printer Application Framework
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

					// "print-content-optimize" values
#  define PAPPL_PRINT_CONTENT_OPTIMIZE_AUTO	"auto"
						// Optimize automatically based on the document
#  define PAPPL_PRINT_CONTENT_OPTIMIZE_GRAPHIC	"graphic"
						// Optimize for vector graphics
#  define PAPPL_PRINT_CONTENT_OPTIMIZE_PHOTO	"photo"
						// Optimize for photos
#  define PAPPL_PRINT_CONTENT_OPTIMIZE_TEXT	"text"
						// Optimize for text
#  define PAPPL_PRINT_CONTENT_OPTIMIZE_TEXT_AND_GRAPHIC "text-and-graphic"
						// Optimize for text and vector graphics


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
  // TODO: Determine if there are other common finishers appropriate to support
  // in PAPPL.  The full list is very long...
};
typedef unsigned pappl_finishings_t;	// Bitfield for IPP "finishings" values

enum pappl_identify_actions_e		// IPP "identify-actions" bit values
{
  PAPPL_IDENTIFY_ACTIONS_DISPLAY = 0x0001,	// 'display'
  PAPPL_IDENTIFY_ACTIONS_FLASH = 0x0002,	// 'flash'
  PAPPL_IDENTIFY_ACTIONS_SOUND = 0x0004,	// 'sound'
  PAPPL_IDENTIFY_ACTIONS_SPEAK = 0x0008		// 'speak'
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
  PAPPL_MEDIA_TRACKING_MARK = 0x0002,		// 'mark'
  PAPPL_MEDIA_TRACKING_WEB = 0x0004		// 'web'
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
  PAPPL_PREASON_SPOOL_AREA_FULL = 0x0800,	// 'spool-area-full'
  PAPPL_PREASON_TONER_EMPTY = 0x1000,		// 'toner-empty'
  PAPPL_PREASON_TONER_LOW = 0x2000		// 'toner-low'
};
typedef unsigned int pappl_preason_t;	// Bitfield for IPP "printer-state-reasons" values

enum pappl_raster_type_e		// IPP "pwg-raster-document-type-supported" bit values
{
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

typedef enum pappl_supply_color_e	// "printer-supply" color values
{
  PAPPL_SUPPLY_COLOR_NO_COLOR,			// No color (waste tank, etc.)
  PAPPL_SUPPLY_COLOR_BLACK,			// Black ink/toner (photo or matte)
  PAPPL_SUPPLY_COLOR_CYAN,			// Cyan ink/toner
  PAPPL_SUPPLY_COLOR_GRAY,			// Gray ink (sometimes marketed as light gray)
  PAPPL_SUPPLY_COLOR_GREEN,			// Green ink
  PAPPL_SUPPLY_COLOR_LIGHT_CYAN,		// Light cyan ink
  PAPPL_SUPPLY_COLOR_LIGHT_GRAY,		// Light gray ink (sometimes marketed as light light gray)
  PAPPL_SUPPLY_COLOR_LIGHT_MAGENTA,		// Light magenta ink
  PAPPL_SUPPLY_COLOR_MAGENTA,			// Magenta ink/toner
  PAPPL_SUPPLY_COLOR_ORANGE,			// Orange ink
  PAPPL_SUPPLY_COLOR_VIOLET,			// Violet ink
  PAPPL_SUPPLY_COLOR_YELLOW			// Yellow ink/toner
} pappl_supply_color_t;

typedef enum pappl_supply_type_e	// IPP "printer-supply" type values
{
  PAPPL_SUPPLY_TYPE_BANDING_SUPPLY,		// Banding finisher supplies
  PAPPL_SUPPLY_TYPE_BINDING_SUPPLY,		// Binding finisher supplies
  PAPPL_SUPPLY_TYPE_CLEANER_UNIT,		// Cleaning unit
  PAPPL_SUPPLY_TYPE_CORONA_WIRE,		// Corona wire (laser printers)
  PAPPL_SUPPLY_TYPE_COVERS,			// Cover finisher supplies
  PAPPL_SUPPLY_TYPE_DEVELOPER,			// Developer supply
  PAPPL_SUPPLY_TYPE_FUSER_CLEANING_PAD,		// Fuser cleaning pad (laser printers)
  PAPPL_SUPPLY_TYPE_FUSER_OIL_WICK,		// Fuser oil wick (laser printers)
  PAPPL_SUPPLY_TYPE_FUSER_OIL,			// Fuser oil supply (laser printers)
  PAPPL_SUPPLY_TYPE_FUSER_OILER,		// Fuser oiler (laser printers)
  PAPPL_SUPPLY_TYPE_FUSER,			// Fuser (laser printers)
  PAPPL_SUPPLY_TYPE_INK_CARTRIDGE,		// Ink cartridge
  PAPPL_SUPPLY_TYPE_INK_RIBBON,			// Ink ribbon supply
  PAPPL_SUPPLY_TYPE_INK,			// Ink supply
  PAPPL_SUPPLY_TYPE_INSERTS,			// Insert finisher supplies
  PAPPL_SUPPLY_TYPE_OPC,			// Optical photoconductor (laser printers)
  PAPPL_SUPPLY_TYPE_PAPER_WRAP,			// Wrap finisher supplies
  PAPPL_SUPPLY_TYPE_RIBBON_WAX,			// Wax ribbon supply
  PAPPL_SUPPLY_TYPE_SOLID_WAX,			// Solid wax supply
  PAPPL_SUPPLY_TYPE_STAPLES,			// Staple finisher supplies
  PAPPL_SUPPLY_TYPE_STITCHING_WIRE,		// Staple/stitch finisher supplies
  PAPPL_SUPPLY_TYPE_TONER_CARTRIDGE,		// Toner cartridge
  PAPPL_SUPPLY_TYPE_TONER,			// Toner supply
  PAPPL_SUPPLY_TYPE_TRANSFER_UNIT,		// Transfer unit (laser printers)
  PAPPL_SUPPLY_TYPE_WASTE_INK,			// Waste ink
  PAPPL_SUPPLY_TYPE_WASTE_TONER,		// Waste toner
  PAPPL_SUPPLY_TYPE_WASTE_WATER,		// Waste water
  PAPPL_SUPPLY_TYPE_WASTE_WAX,			// Waste wax
  PAPPL_SUPPLY_TYPE_WATER			// Water supply
} pappl_supply_type_t;


//
// Callback functions...
//

typedef struct pappl_options_s pappl_options_t;
					// Combined job options

typedef void (*pappl_default_cb_t)(ipp_attribute_t *attr, void *data);
					// papplIterateDefaults callback function

typedef void (*pappl_identfunc_t)(pappl_printer_t *printer, pappl_identify_actions_t actions, const char *message);
					// Identify-Printer callback

typedef void (*pappl_job_cb_t)(pappl_job_t *job, void *data);
					// papplIterateXxxJobs callback function

typedef bool (*pappl_printfunc_t)(pappl_job_t *job, pappl_options_t *options, pappl_device_t *device);
					// Print a "raw" job callback
typedef bool (*pappl_rendjobfunc_t)(pappl_job_t *job, pappl_options_t *options, pappl_device_t *device);
					// End a raster job callback
typedef bool (*pappl_rendpagefunc_t)(pappl_job_t *job, pappl_options_t *options, pappl_device_t *device, unsigned page);
					// End a raster page callback
typedef bool (*pappl_rstartjobfunc_t)(pappl_job_t *job, pappl_options_t *options, pappl_device_t *device);
					// Start a raster job callback
typedef bool (*pappl_rstartpagefunc_t)(pappl_job_t *job, pappl_options_t *options, pappl_device_t *device, unsigned page);
					// Start a raster page callback
typedef bool (*pappl_rwritefunc_t)(pappl_job_t *job, pappl_options_t *options, pappl_device_t *device, unsigned y, const unsigned char *line);
					// Write a line of raster graphics callback
typedef bool (*pappl_statusfunc_t)(pappl_printer_t *printer);
					// Update printer status callback


//
// Structures...
//

typedef unsigned char pappl_dither_t[16][16];
                                        // 16x16 dither array

typedef struct pappl_icon_s 		// Printer PNG icon structure
{
  char		filename[256];			// External filename, if any
  const void	*data;				// PNG icon data, if any
  size_t	datalen;			// Size of PNG icon data
} pappl_icon_t;

typedef struct pappl_media_col_s	// Media details structure
{
  int			bottom_margin,		// Bottom margin in hundredths of millimeters
			left_margin,		// Left margin in hundredths of millimeters
			right_margin,		// Right margin in hundredths of millimeters
			size_width,		// Width in hundredths of millimeters
			size_length;		// Height in hundredths of millimeters
  char			size_name[64],		// PWG media size name
			source[64];		// PWG media source name
  int			top_margin,		// Top margin in hundredths of millimeters
			top_offset;		// Top offset in hundredths of millimeters
  pappl_media_tracking_t tracking;		// Media tracking
  char			type[64];		// PWG media type name
} pappl_media_col_t;

struct pappl_options_s			// Combined job options
{
  cups_page_header2_t	header;			// Raster header
  unsigned		num_pages;		// Number of pages in job
  pappl_dither_t	dither;			// Dither array, if any
  int			copies;	 		// "copies" value
  pappl_media_col_t	media;			// "media"/"media-col" value
  ipp_orient_t		orientation_requested;	// "orientation-requested" value
  pappl_color_mode_t	print_color_mode;	// "print-color-mode" value
  const char		*print_content_optimize;// "print-content-optimize" value
  int			print_darkness;		// "print-darkness" value
  ipp_quality_t		print_quality;		// "print-quality" value
  int			print_speed;		// "print-speed" value
  int			printer_resolution[2];	// "printer-resolution" value in dots per inch
};

typedef struct pappl_supply_s		// Supply data
{
  pappl_supply_color_t	color;			// Color, if any
  char			description[256];	// Description
  bool			is_consumed;		// Is this a supply that is consumed?
  int			level;			// Level (0-100, -1 = unknown)
  pappl_supply_type_t	type;			// Type
} pappl_supply_t;

struct pappl_driver_data_s		// Driver data
{
  pappl_identfunc_t	identify;		// Identify-Printer function
  pappl_printfunc_t	print;			// Print (file) function
  pappl_rendjobfunc_t	rendjob;		// End raster job function
  pappl_rendpagefunc_t	rendpage;		// End raster page function
  pappl_rstartjobfunc_t rstartjob;		// Start raster job function
  pappl_rstartpagefunc_t rstartpage;		// Start raster page function
  pappl_rwritefunc_t	rwrite;			// Write raster line function
  pappl_statusfunc_t	status;			// Status function
  const char		*format;		// Printer-specific format
  char			make_and_model[128];	// "printer-make-and-model" value
  pappl_icon_t		icons[3];		// "printer-icons" values
  pappl_kind_t		kind;			// "printer-kind" values
  bool			input_face_up,		// Does input media come in face-up?
			output_face_up;		// Does output media come out face-up?
  pappl_color_mode_t	color_modes;		// "print-color-mode" values
  pappl_raster_type_t	raster_types;		// "pwg-raster-document-type-supported" values
  pappl_duplex_t	duplex;			// Duplex printing modes supported
  pappl_finishings_t	finishings;		// "finishings" values
  int			num_resolution,		// Number of printer resolutions
			x_resolution[PAPPL_MAX_RESOLUTION],
			y_resolution[PAPPL_MAX_RESOLUTION];
						// Printer resolutions
  bool			borderless;		// Borderless margins supported?
  int			left_right,		// Left and right margins in hundredths of millimeters
			bottom_top;		// Bottom and top margins in hundredths of millimeters
  int			num_media;		// Number of supported media
  const char		*media[PAPPL_MAX_MEDIA];// Supported media
  pappl_media_col_t	media_default,		// Default media
			media_ready[PAPPL_MAX_SOURCE];
						// Ready media
  int			num_source;		// Number of media sources (trays/rolls)
  const char		*source[PAPPL_MAX_SOURCE];
						// Media sources
  int			top_offset_supported[2];
						// media-top-offset-supported (0,0 for none)
  pappl_media_tracking_t tracking_supported;
						// media-tracking-supported
  int			num_type;		// Number of media types
  const char		*type[PAPPL_MAX_TYPE];	// Media types
  int			num_bin;		// Number of output bins
  const char		*bin[PAPPL_MAX_BIN];	// Output bins
  pappl_label_mode_t	mode_configured,	// label-mode-configured
			mode_supported;		// label-mode-supported
  int			tear_offset_configured,	// label-tear-offset-configured
			tear_offset_supported[2];
						// label-tear-offset-supported (0,0 for none)
  int			speed_supported[2],	// print-speed-supported (0,0 for none)
			speed_default;		// print-speed-default
  int			darkness_configured,	// printer-darkness-configured
			darkness_supported;	// printer-darkness-supported (0 for none)
  pappl_identify_actions_t identify_default,	// "identify-actions-default" values
			identify_supported;	// "identify-actions-supported" values
  int			num_vendor;		// Number of vendor attributes
  const char		*vendor[PAPPL_MAX_VENDOR];
						// Vendor attribute names
};



//
// Functions...
//

extern void		papplPrinterCloseDevice(pappl_printer_t *printer) _PAPPL_PUBLIC;

extern pappl_printer_t	*papplPrinterCreate(pappl_system_t *system, int printer_id, const char *printer_name, const char *driver_name, const char *device_uri) _PAPPL_PUBLIC;
extern void		papplPrinterDelete(pappl_printer_t *printer) _PAPPL_PUBLIC;

extern pappl_job_t	*papplPrinterFindJob(pappl_printer_t *printer, int job_id) _PAPPL_PUBLIC;

extern int		papplPrinterGetActiveJobs(pappl_printer_t *printer) _PAPPL_PUBLIC;
extern int		papplPrinterGetDefaultInteger(pappl_printer_t *printer, const char *name) _PAPPL_PUBLIC;
extern void		papplPrinterGetDefaultMedia(pappl_printer_t *printer, pappl_media_col_t *media) _PAPPL_PUBLIC;
extern char		*papplPrinterGetDefaultString(pappl_printer_t *printer, const char *name, char *buffer, size_t bufsize) _PAPPL_PUBLIC;
extern pappl_driver_data_t *papplPrinterGetDriverData(pappl_printer_t *printer, pappl_driver_data_t *data) _PAPPL_PUBLIC;
extern char		*papplPrinterGetDNSSDName(pappl_printer_t *printer, char *buffer, size_t bufsize) _PAPPL_PUBLIC;
extern char		*papplPrinterGetDriverName(pappl_printer_t *printer, char *buffer, size_t bufsize) _PAPPL_PUBLIC;
extern char		*papplPrinterGetGeoLocation(pappl_printer_t *printer, char *buffer, size_t bufsize) _PAPPL_PUBLIC;
extern int		papplPrinterGetID(pappl_printer_t *printer) _PAPPL_PUBLIC;
extern int		papplPrinterGetImpressionsCompleted(pappl_printer_t *printer) _PAPPL_PUBLIC;
extern char		*papplPrinterGetLocation(pappl_printer_t *printer, char *buffer, size_t bufsize) _PAPPL_PUBLIC;
extern int		papplPrinterGetMaxActiveJobs(pappl_printer_t *printer) _PAPPL_PUBLIC;
extern int		papplPrinterGetMediaSources(pappl_printer_t *printer, int max_sources, pappl_media_col_t *sources) _PAPPL_PUBLIC;
extern int		papplPrinterGetMediaTypes(pappl_printer_t *printer, int max_types, pappl_media_col_t *types) _PAPPL_PUBLIC;
extern const char	*papplPrinterGetName(pappl_printer_t *printer) _PAPPL_PUBLIC;
extern int		papplPrinterGetNextJobId(pappl_printer_t *printer) _PAPPL_PUBLIC;
extern char		*papplPrinterGetOrganization(pappl_printer_t *printer, char *buffer, size_t bufsize) _PAPPL_PUBLIC;
extern char		*papplPrinterGetOrganizationalUnit(pappl_printer_t *printer, char *buffer, size_t bufsize) _PAPPL_PUBLIC;
extern char		*papplPrinterGetPrintGroup(pappl_printer_t *printer, char *buffer, size_t bufsize) _PAPPL_PUBLIC;
extern int		papplPrinterGetReadyMedia(pappl_printer_t *printer, int max_ready, pappl_media_col_t *ready) _PAPPL_PUBLIC;
extern pappl_preason_t	papplPrinterGetReasons(pappl_printer_t *printer) _PAPPL_PUBLIC;
extern ipp_pstate_t	papplPrinterGetState(pappl_printer_t *printer) _PAPPL_PUBLIC;
extern int		papplPrinterGetSupplies(pappl_printer_t *printer, int max_supplies, pappl_supply_t *supplies) _PAPPL_PUBLIC;
extern int		papplPrinterGetSupportedMedia(pappl_printer_t *printer, int max_supported, pappl_media_col_t *supported) _PAPPL_PUBLIC;
extern pappl_system_t	*papplPrinterGetSystem(pappl_printer_t *printer) _PAPPL_PUBLIC;

extern void		papplPrinterIterateActiveJobs(pappl_printer_t *printer, pappl_job_cb_t cb, void *data) _PAPPL_PUBLIC;
extern void		papplPrinterIterateAllJobs(pappl_printer_t *printer, pappl_job_cb_t cb, void *data) _PAPPL_PUBLIC;
extern void		papplPrinterIterateCompletedJobs(pappl_printer_t *printer, pappl_job_cb_t cb, void *data) _PAPPL_PUBLIC;

extern void		papplPrinterIterateDefaults(pappl_printer_t *printer, pappl_default_cb_t cb, void *data) _PAPPL_PUBLIC;

extern pappl_device_t	*papplPrinterOpenDevice(pappl_printer_t *printer) _PAPPL_PUBLIC;

extern void		papplPrinterSetDefaultInteger(pappl_printer_t *printer, const char *name, int value) _PAPPL_PUBLIC;
extern void		papplPrinterSetDefaultMedia(pappl_printer_t *printer, pappl_media_col_t *media) _PAPPL_PUBLIC;
extern void		papplPrinterSetDefaultString(pappl_printer_t *printer, const char *name, const char *value) _PAPPL_PUBLIC;
extern void		papplPrinterSetDNSSDName(pappl_printer_t *printer, const char *value) _PAPPL_PUBLIC;
extern void		papplPrinterSetDriverData(pappl_printer_t *printer, pappl_driver_data_t *data, ipp_t *attrs) _PAPPL_PUBLIC;
extern void		papplPrinterSetGeoLocation(pappl_printer_t *printer, const char *value) _PAPPL_PUBLIC;
extern void		papplPrinterSetImpressionsCompleted(pappl_printer_t *printer, int add) _PAPPL_PUBLIC;
extern void		papplPrinterSetLocation(pappl_printer_t *printer, const char *value) _PAPPL_PUBLIC;
extern void		papplPrinterSetMaxActiveJobs(pappl_printer_t *printer, int max_active_jobs) _PAPPL_PUBLIC;
extern void		papplPrinterSetOrganization(pappl_printer_t *printer, const char *value) _PAPPL_PUBLIC;
extern void		papplPrinterSetOrganizationalUnit(pappl_printer_t *printer, const char *value) _PAPPL_PUBLIC;
extern void		papplPrinterSetPrintGroup(pappl_printer_t *printer, const char *value) _PAPPL_PUBLIC;
extern void		papplPrinterSetReadyMedia(pappl_printer_t *printer, int num_ready, pappl_media_col_t *ready) _PAPPL_PUBLIC;
extern void		papplPrinterSetReasons(pappl_printer_t *printer, pappl_preason_t add, pappl_preason_t remove) _PAPPL_PUBLIC;
extern void		papplPrinterSetSupplies(pappl_printer_t *printer, int num_supplies, pappl_supply_t *supplies) _PAPPL_PUBLIC;

#endif // !_PAPPL_PRINTER_H_
