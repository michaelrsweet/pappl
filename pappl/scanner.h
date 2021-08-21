//
// Scanner header file for the Scanner Application Framework
//
// Copyright © 2019-2020 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#ifndef _PAPPL_SCANNER_H_
#  define _PAPPL_SCANNER_H_


//
// Include necessary headers...
//

#  include "base.h"
#  include "printer.h"


//
// C++ magic...
//

#  ifdef __cplusplus
extern "C" {
#  endif // __cplusplus


//
// Callback functions...
//

typedef ipp_status_t (*pappl_sc_scanfile_cb_t)(pappl_job_t *job, pappl_sc_options_t *options, pappl_device_t *device);
    					// Scan to a file job callback
typedef bool (*pappl_sc_rendjob_cb_t)(pappl_job_t *job, pappl_sc_options_t *options,pappl_device_t *device);
    					// End a raster job callback
typedef bool (*pappl_sc_rendpage_cb_t)(pappl_job_t *job, pappl_sc_options_t *options, pappl_device_t *device, unsigned page);
    					// End a raster page callback
typedef bool (*pappl_sc_rgetline_cb_t)(pappl_job_t *job, pappl_sc_options_t *options, pappl_device_t *device, unsigned char *line, size_t maxline);
    					// Write a line of raster graphics callback
typedef ipp_status_t (*pappl_sc_rstartjob_cb_t)(pappl_job_t *job, pappl_sc_options_t *options, pappl_device_t *device);
    					// Start a raster job callback
typedef bool (*pappl_sc_rstartpage_cb_t)(pappl_job_t *job, pappl_sc_options_t *options, pappl_device_t *device, unsigned page);
    					// Start a raster page callback, return false for no more pages
typedef bool (*pappl_sc_status_cb_t)(pappl_scanner_t *scanner);
    					// Update scanner status callback


//
// Structures...
//

struct pappl_sc_options_s		// Combined scan job options
{
  cups_page_header2_t	header;			// Raster header
  unsigned		num_pages;		// Number of pages in job
  unsigned		first_page;		// First page in page-ranges, starting at 1
  unsigned		last_page;		// Last page in page-ranges, starting at 1
  pappl_dither_t	dither;			// Dither array, if any
  int			copies;	 		// "copies" value
  pappl_finishings_t	finishings;		// "finishings" value(s)
  pappl_media_col_t	input_media;			// "input-media"/"media-col" value
  ipp_orient_t		input_orientation_requested;	// "input-orientation-requested" value    
  char			output_bin[64];		// "output-bin" value
  pappl_color_mode_t	input_color_mode;	// "input-color-mode" value
  int			input_brightness;		// "input_brightness" value
  int			input_sharpness;	// "input-sharpness" value
  ipp_quality_t		input_quality;		// "input-quality" value
  pappl_scaling_t	input_scaling;		// "input-scaling" value
  int			scan_speed;		// "scan-speed" value
  int			input_resolution[2];	// "input-resolution" value in dots per inch
  pappl_sides_t		input_sides;			// "input-sides" value
};

struct pappl_sc_driver_data_s		// Scanner driver data
{
  void				*extension;	// Extension data (managed by driver)
  
  pappl_sc_rendjob_cb_t		rendjob_cb;	// End raster job callback
  pappl_sc_rendpage_cb_t	rendpage_cb;	// End raster page callback
  pappl_sc_rstartjob_cb_t	rstartjob_cb;	// Start raster job callback
  pappl_sc_rstartpage_cb_t	rstartpage_cb;	// Start raster page callback
  pappl_sc_rgetline_cb_t	rgetline_cb;	// Write raster line callback
  pappl_sc_status_cb_t		status_cb;	// Status callback

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
extern void            papplPrinterSetScanner(pappl_printer_t *printer, pappl_scanner_t *scanner) _PAPPL_PUBLIC;


//
// C++ magic...
//

#  ifdef __cplusplus
}
#  endif // __cplusplus


#endif // !_PAPPL_SCANNER_H_
