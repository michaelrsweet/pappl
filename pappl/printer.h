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
// Limits...
//

#  define PAPPL_MAX_COLOR_MODE	8	// Maximum number of color modes
#  define PAPPL_MAX_MEDIA	256	// Maximum number of media sizes
#  define PAPPL_MAX_RASTER_TYPE	16	// Maximum number of raster types
#  define PAPPL_MAX_RESOLUTION	4	// Maximum number of printer resolutions
#  define PAPPL_MAX_SOURCE	16	// Maximum number of sources/rolls
#  define PAPPL_MAX_SUPPLY	32	// Maximum number of supplies
#  define PAPPL_MAX_TYPE	32	// Maximum number of media types


//
// Constants...
//

					// "print-color-mode" values
#  define PAPPL_PRINT_COLOR_MODE_AUTO		"auto"
						// Automatic color/monochrome print mode
#  define PAPPL_PRINT_COLOR_MODE_AUTO_MONOCHROME "auto-monochrome"
						// Automatic monochrome/process monochrome print mode
#  define PAPPL_PRINT_COLOR_MODE_BI_LEVEL	"bi-level"
						// B&W (threshold) print mode
#  define PAPPL_PRINT_COLOR_MODE_COLOR		"color"
						// Full color print mode
#  define PAPPL_PRINT_COLOR_MODE_MONOCHROME	"monochrome"
						// Grayscale print mode using 1 color
#  define PAPPL_PRINT_COLOR_MODE_PROCESS_MONOCHROME "process-monochrome"
						// Grayscale print mode using multiple colors

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

					// "printer-supply" color values
#  define PAPPL_SUPPLY_COLOR_BLACK		"black"
						// Black ink/toner (photo or matte)
#  define PAPPL_SUPPLY_COLOR_CYAN		"cyan"
						// Cyan ink/toner
#  define PAPPL_SUPPLY_COLOR_GRAY		"gray"
						// Gray ink (sometimes marketed as light gray)
#  define PAPPL_SUPPLY_COLOR_GREEN		"green"
						// Green ink
#  define PAPPL_SUPPLY_COLOR_LIGHT_CYAN		"light-cyan"
						// Light cyan ink
#  define PAPPL_SUPPLY_COLOR_LIGHT_GRAY		"light-gray"
						// Light gray ink (sometimes marketed as light light gray)
#  define PAPPL_SUPPLY_COLOR_LIGHT_MAGENTA	"light-magenta"
						// Light magenta ink
#  define PAPPL_SUPPLY_COLOR_MAGENTA		"magenta"
						// Magenta ink/toner
#  define PAPPL_SUPPLY_COLOR_NO_COLOR		"no-color"
						// No color (waste tank, etc.)
#  define PAPPL_SUPPLY_COLOR_ORANGE		"orange"
						// Orange ink
#  define PAPPL_SUPPLY_COLOR_VIOLET		"violet"
						// Violet ink
#  define PAPPL_SUPPLY_COLOR_YELLOW		"yellow"
						// Yellow ink/toner

					// "pwg-raster-type-supported" values
#  define PAPPL_PWG_RASTER_TYPE_ADOBE_RGB_8	"adobe-rgb_8"
						// 8-bit per component AdobeRGB
#  define PAPPL_PWG_RASTER_TYPE_ADOBE_RGB_16	"adobe-rgb_16"
						// 16-bit per component AdobeRGB
#  define PAPPL_PWG_RASTER_TYPE_BLACK_1		"black_1"
						// 1-bit (device) black
#  define PAPPL_PWG_RASTER_TYPE_BLACK_8		"black_8"
						// 8-bit (device) black
#  define PAPPL_PWG_RASTER_TYPE_BLACK_16	"black_16"
						// 16-bit (device) black
#  define PAPPL_PWG_RASTER_TYPE_CMYK_8		"cmyk_8"
						// 8-bit per component (device) CMYK
#  define PAPPL_PWG_RASTER_TYPE_CMYK_16		"cmyk_16"
						// 16-bit per component (device) CMYK
#  define PAPPL_PWG_RASTER_TYPE_RGB_8		"rgb_8"
						// 8-bit per component (device) RGB
#  define PAPPL_PWG_RASTER_TYPE_RGB_8		"rgb_16"
						// 16-bit per component (device) RGB)
#  define PAPPL_PWG_RASTER_TYPE_SGRAY_8		"sgray_8"
						// 8-bit grayscale with 2.2 gamma
#  define PAPPL_PWG_RASTER_TYPE_SGRAY_16	"sgray_16"
						// 16-bit grayscale with 2.2 gamma
#  define PAPPL_PWG_RASTER_TYPE_SRGB_8		"srgb_8"
						// 8-bit per component sRGB
#  define PAPPL_PWG_RASTER_TYPE_SRGB_8		"srgb_16"
						// 16-bit per component sRGB


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
  PAPPL_MEDIA_TRACKING_CONTINUOUS = 0x0001,		// 'continuous'
  PAPPL_MEDIA_TRACKING_MARK = 0x0002,			// 'mark'
  PAPPL_MEDIA_TRACKING_WEB = 0x0004			// 'web'
};
typedef unsigned short pappl_media_tracking_t;
					// Bitfield for IPP "media-tracking" values

enum pappl_preason_e			// IPP "printer-state-reasons" bit values
{
  PAPPL_PREASON_NONE = 0x0000,				// 'none'
  PAPPL_PREASON_OTHER = 0x0001,				// 'other'
  PAPPL_PREASON_COVER_OPEN = 0x0002,			// 'cover-open'
  PAPPL_PREASON_INPUT_TRAY_MISSING = 0x0004,		// 'input-tray-missing'
  PAPPL_PREASON_MARKER_SUPPLY_EMPTY = 0x0008,		// 'marker-supply-empty'
  PAPPL_PREASON_MARKER_SUPPLY_LOW = 0x0010,		// 'marker-supply-low'
  PAPPL_PREASON_MARKER_WASTE_ALMOST_FULL = 0x0020,	// 'marker-waste-almost-full'
  PAPPL_PREASON_MARKER_WASTE_FULL = 0x0040,		// 'marker-waste-full'
  PAPPL_PREASON_MEDIA_EMPTY = 0x0080,			// 'media-empty'
  PAPPL_PREASON_MEDIA_JAM = 0x0100,			// 'media-jam'
  PAPPL_PREASON_MEDIA_LOW = 0x0200,			// 'media-low'
  PAPPL_PREASON_MEDIA_NEEDED = 0x0400,			// 'media-needed'
  PAPPL_PREASON_SPOOL_AREA_FULL = 0x0800,		// 'spool-area-full'
  PAPPL_PREASON_TONER_EMPTY = 0x1000,			// 'toner-empty'
  PAPPL_PREASON_TONER_LOW = 0x2000			// 'toner-low'
};
typedef unsigned int pappl_preason_t;	// Bitfield for IPP "printer-state-reasons" values

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
// Structures...
//

typedef struct pappl_driver_data_s	// Driver data
{
  pappl_printfunc_t	print;			// Print (file) function
  pappl_rendjobfunc_t	rendjob;		// End raster job function
  pappl_rendpagefunc_t	rendpage;		// End raster page function
  pappl_rstartjobfunc_t rstartjob;		// Start raster job function
  pappl_rstartpagefunc_t rstartpage;		// Start raster page function
  pappl_rwritefunc_t	rwrite;			// Write raster line function
  pappl_statusfunc_t	status;			// Status function
  const char		*format;		// Printer-specific format
  int			num_color_mode;		// Number of "print-color-mode" values
  const char		*color_mode[PAPPL_MAX_COLOR_MODE];
						// "print-color-mode" values
  int			num_raster_type;	// Number of "pwg-raster-document-type-supported" values
  const char		*raster_type[PAPPL_MAX_RASTER_TYPE];
						// "pwg-raster-document-type-supported" values
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
  int			num_source;		// Number of media sources (rolls)
  const char		*source[PAPPL_MAX_SOURCE];
						// Media sources
  int			top_offset_supported[2];
						// media-top-offset-supported (0,0 for none)
  pappl_media_tracking_t tracking_supported;
						// media-tracking-supported
  int			num_type;		// Number of media types
  const char		*type[PAPPL_MAX_TYPE];	// Media types
  pappl_label_mode_t	mode_configured,	// label-mode-configured
			mode_supported;		// label-mode-supported
  int			tear_offset_configured,	// label-tear-offset-configured
			tear_offset_supported[2];
						// label-tear-offset-supported (0,0 for none)
  int			speed_supported[2],	// print-speed-supported (0,0 for none)
			speed_default;		// print-speed-default
  int			darkness_configured,	// printer-darkness-configured
			darkness_supported;	// printer-darkness-supported (0 for none)
} pappl_driver_data_t;

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

typedef struct pappl_options_s		// Combined job options
{
  cups_page_header2_t	header;			// Raster header
  unsigned		num_pages;		// Number of pages in job
  const pappl_dither_t	*dither;		// Dither array, if any
  int			copies;	 		// "copies" value
  pappl_media_col_t	media;			// "media"/"media-col" value
  ipp_orient_t		orientation_requested;	// "orientation-requested" value
  const char		*print_color_mode,	// "print-color-mode" value
			*print_content_optimize;// "print-content-optimize" value
  int			print_darkness;		// "print-darkness" value
  ipp_quality_t		print_quality;		// "print-quality" value
  int			print_speed;		// "print-speed" value
  int			printer_resolution[2];	// "printer-resolution" value in dots per inch
} pappl_options_t;

typedef struct pappl_supply_s		// Supply data
{
  const char		*color;			// Colorant, if any
  const char		*description;		// Description
  bool			is_consumed;		// Is this a supply that is consumed?
  int			level;			// Level (0-100, -1 = unknown)
  pappl_supply_type_t	type;			// Type
} pappl_supply_t;


//
// Callback functions...
//

typedef int (*pappl_printfunc_t)(pappl_job_t *job, pappl_options_t *options);
					// Print a "raw" job
typedef int (*pappl_rendjobfunc_t)(pappl_job_t *job, pappl_options_t *options);
					// End a raster job
typedef int (*pappl_rendpagefunc_t)(pappl_job_t *job, pappl_options_t *options, unsigned page);
					// End a raster page
typedef int (*pappl_rstartjobfunc_t)(pappl_job_t *job, pappl_options_t *options);
					// Start a raster job
typedef int (*pappl_rstartpagefunc_t)(pappl_job_t *job, pappl_options_t *options, unsigned page);
					// Start a raster page
typedef int (*pappl_rwritefunc_t)(pappl_job_t *job, pappl_options_t *options, unsigned y, const unsigned char *line);
					// Write a line of raster graphics
typedef int (*pappl_statusfunc_t)(pappl_printer_t *printer);
					// Update printer status

#if 0
  pthread_rwlock_t	rwlock;		// Reader/writer lock
  char			*name;		// Name of driver
  pappl_device_t	*device;	// Connection to device
  void			*job_data;	// Driver job data
  int			num_supply;	// Number of printer-supply
  pappl_supply_t	supply[PAPPL_MAX_SUPPLY];
					// printer-supply
  ipp_t			*driver_attrs;	// Capability attributes
#endif /* 0 */


//
// Functions...
//

extern pappl_printer_t	*papplPrinterCreate(pappl_system_t *system, int printer_id, const char *printer_name, const char *driver_name, const char *device_uri);
extern void		papplPrinterDelete(pappl_printer_t *printer);

extern pappl_driver_data_t *papplPrinterGetDriverData(pappl_printer_t *printer);
extern const char	*papplPrinterGetDNSSDName(pappl_printer_t *printer);
extern const char	*papplPrinterGetDriverName(pappl_printer_t *printer);
extern const char	*papplPrinterGetGeoLocation(pappl_printer_t *printer);
extern const char	*papplPrinterGetLocation(pappl_printer_t *printer);
extern const char	*papplPrinterGetOrganization(pappl_printer_t *printer);
extern const char	*papplPrinterGetOrganizationalUnit(pappl_printer_t *printer);
extern pappl_system_t	*papplPrinterGetSystem(pappl_printer_t *printer);

extern pappl_device_t	*papplPrinterOpenDevice(pappl_printer_t *printer);

extern void		papplPrinterSetDNSSDName(pappl_printer_t *printer, const char *value);
extern void		papplPrinterSetDriverData(pappl_printer_t *printer, pappl_driver_data_t *data);
extern void		papplPrinterSetGeoLocation(pappl_printer_t *printer, const char *value);
extern void		papplPrinterSetLocation(pappl_printer_t *printer, const char *value);
extern void		papplPrinterSetOrganization(pappl_printer_t *printer, const char *value);
extern void		papplPrinterSetOrganizationalUnit(pappl_printer_t *printer, const char *value);
extern void		papplPrinterSetReasons(pappl_printer_t *printer, pappl_preason_t add, pappl_preason_t remove);
extern void		papplPrinterSetSupplies(pappl_printer_t *printer, int num_supplies, pappl_supply_t *supplies);

#endif // !_PAPPL_PAPPL_H_
