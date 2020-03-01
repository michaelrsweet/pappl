//
// Public header file for the Printer Application Framework
//
// Copyright © 2019-2020 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#ifndef _PAPPL_PAPPL_H_
#  define _PAPPL_PAPPL_H_

//
// Include necessary headers...
//

#  include <cups/cups.h>
#  include <cups/raster.h>


//
// New operations/tags defined in CUPS 2.3 and later...
//

#  if CUPS_VERSION_MAJOR == 2 && CUPS_VERSION_MINOR < 3
#    define IPP_OP_CREATE_PRINTER		(ipp_op_t)0x004C
#    define IPP_OP_DELETE_PRINTER		(ipp_op_t)0x004E
#    define IPP_OP_GET_PRINTERS			(ipp_op_t)0x004F
#    define IPP_OP_GET_SYSTEM_ATTRIBUTES	(ipp_op_t)0x005B
#    define IPP_OP_SET_SYSTEM_ATTRIBUTES	(ipp_op_t)0x0062
#    define IPP_OP_SHUTDOWN_ALL_PRINTERS	(ipp_op_t)0x0063

#    define IPP_TAG_SYSTEM			(ipp_tag_t)0x000A
#  endif // CUPS_VERSION_MAJOR == 2 && CUPS_VERSION_MINOR < 3


//
// Limits...
//

#  define PAPPL_MAX_MEDIA	256	// Maximum number of media sizes
#  define PAPPL_MAX_RESOLUTION	4	// Maximum number of printer resolutions
#  define PAPPL_MAX_SOURCE	16	// Maximum number of sources/rolls
#  define PAPPL_MAX_SUPPLY	32	// Maximum number of supplies
#  define PAPPL_MAX_TYPE	32	// Maximum number of media types


//
// Constants...
//

enum pappl_jreason_e			// IPP "job-state-reasons" bit values
{
  PAPPL_JREASON_NONE = 0x00000000,			// 'none'
  PAPPL_JREASON_ABORTED_BY_SYSTEM = 0x00000001,		// 'aborted-by-system'
  PAPPL_JREASON_COMPRESSION_ERROR = 0x00000002,		// 'compression-error'
  PAPPL_JREASON_DOCUMENT_FORMAT_ERROR = 0x00000004,	// 'document-format-error'
  PAPPL_JREASON_DOCUMENT_PASSWORD_ERROR = 0x00000008,	// 'document-password-error'
  PAPPL_JREASON_DOCUMENT_PERMISSION_ERROR = 0x00000010,	// 'document-permission-error'
  PAPPL_JREASON_DOCUMENT_UNPRINTABLE_ERROR = 0x00000020,// 'document-unprintable-error'
  PAPPL_JREASON_ERRORS_DETECTED = 0x00000040,		// 'errors-detected'
  PAPPL_JREASON_JOB_CANCELED_AT_DEVICE = 0x00000080,	// 'job-canceled-at-device'
  PAPPL_JREASON_JOB_CANCELED_BY_USER = 0x00000100,	// 'job-canceled-by-user'
  PAPPL_JREASON_JOB_COMPLETED_SUCCESSFULLY = 0x00000200,// 'job-completed-successfully'
  PAPPL_JREASON_JOB_COMPLETED_WITH_ERRORS = 0x00000400,	// 'job-completed-with-errors'
  PAPPL_JREASON_JOB_COMPLETED_WITH_WARNINGS = 0x00000800,// 'job-completed-with-warnings'
  PAPPL_JREASON_JOB_DATA_INSUFFICIENT = 0x00001000,	// 'job-data-insufficient'
  PAPPL_JREASON_JOB_INCOMING = 0x000002000,		// 'job-incoming'
  PAPPL_JREASON_JOB_PRINTING = 0x00004000,		// 'job-printing'
  PAPPL_JREASON_JOB_QUEUED = 0x00008000,		// 'job-queued'
  PAPPL_JREASON_JOB_SPOOLING = 0x00010000,		// 'job-spooling'
  PAPPL_JREASON_PRINTER_STOPPED = 0x00020000,		// 'printer-stopped'
  PAPPL_JREASON_PRINTER_STOPPED_PARTLY = 0x00040000,	// 'printer-stopped-partly'
  PAPPL_JREASON_PROCESSING_TO_STOP_POINT = 0x00080000,	// 'processing-to-stop-point'
  PAPPL_JREASON_QUEUED_IN_DEVICE = 0x00100000,		// 'queued-in-device'
  PAPPL_JREASON_WARNINGS_DETECTED = 0x00200000		// 'warnings-detected'
};
typedef unsigned int pappl_jreason_t;	// Bitfield for IPP "job-state-reasons" values

enum pappl_label_mode_e			// IPP "label-mode-xxx" bit values
{
  PAPPL_LABEL_MODE_APPLICATOR = 0x0001,			// 'applicator'
  PAPPL_LABEL_MODE_CUTTER = 0x0002,			// 'cutter'
  PAPPL_LABEL_MODE_CUTTER_DELAYED = 0x0004,		// 'cutter-delayed'
  PAPPL_LABEL_MODE_KIOSK = 0x0008,			// 'kiosk'
  PAPPL_LABEL_MODE_PEEL_OFF = 0x0010,			// 'peel-off'
  PAPPL_LABEL_MODE_PEEL_OFF_PREPEEL = 0x0020,		// 'peel-off-prepeel'
  PAPPL_LABEL_MODE_REWIND = 0x0040,			// 'rewind'
  PAPPL_LABEL_MODE_RFID = 0x0080,			// 'rfid'
  PAPPL_LABEL_MODE_TEAR_OFF = 0x0100			// 'tear-off'
};
typedef unsigned short pappl_label_mode_t;
					// Bitfield for IPP "label-mode-xxx" values

typedef enum pappl_loglevel_e		// Log levels
{
  PAPPL_LOGLEVEL_UNSPEC = -1,		// Not specified
  PAPPL_LOGLEVEL_DEBUG,			// Debug message
  PAPPL_LOGLEVEL_INFO,			// Informational message
  PAPPL_LOGLEVEL_WARN,			// Warning message
  PAPPL_LOGLEVEL_ERROR,			// Error message
  PAPPL_LOGLEVEL_FATAL			// Fatal message
} pappl_loglevel_t;

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


//
// Types and structures...
//

typedef struct pappl_client_s pappl_client_t;
					// Client connection
typedef struct pappl_job_s pappl_job_t;	// Job object
typedef struct pappl_printer_s pappl_printer_t;
					// Printer object
typedef struct pappl_system_s pappl_system_t;
					// System object

typedef struct pappl_media_col_s	// Media details
{
  int			bottom_margin,	// Bottom margin in hundredths of millimeters
			left_margin,	// Left margin in hundredths of millimeters
			right_margin,	// Right margin in hundredths of millimeters
			size_width,	// Width in hundredths of millimeters
			size_length;	// Height in hundredths of millimeters
  char			size_name[64],	// PWG media size name
			source[64];	// PWG media source name
  int			top_margin,	// Top margin in hundredths of millimeters
			top_offset;	// Top offset in hundredths of millimeters
  pappl_media_tracking_t tracking;	// Media tracking
  char			type[64];	// PWG media type name
} pappl_media_col_t;

typedef struct pappl_options_s		// Computed job options
{
  cups_page_header2_t	header;		// Raster header
  unsigned		num_pages;	// Number of pages in job
  const pappl_dither_t	*dither;	// Dither array
  int			copies;	 	// copies
  pappl_media_col_t	media;		// media/media-col
  ipp_orient_t		orientation_requested;
					// orientation-requested
  const char		*print_color_mode,
					// print-color-mode
			*print_content_optimize;
					// print-content-optimize
  int			print_darkness;	// print-darkness
  ipp_quality_t		print_quality;	// print-quality
  int			print_speed;	// print-speed
  int			printer_resolution[2];
					// printer-resolution
} pappl_options_t;

typedef int (*pappl_printfunc_t)(pappl_job_t *job, pappl_options_t *options);
					// Print a job
typedef int (*pappl_rendjobfunc_t)(pappl_job_t *job, pappl_options_t *options);
					// End a raster job
typedef int (*pappl_rendpagefunc_t)(pappl_job_t *job, pappl_options_t *options, unsigned page);
					// End a raster job
typedef int (*pappl_rstartjobfunc_t)(pappl_job_t *job, pappl_options_t *options);
					// Start a raster job
typedef int (*pappl_rstartpagefunc_t)(pappl_job_t *job, pappl_options_t *options, unsigned page);
					// Start a raster page
typedef int (*pappl_rwritefunc_t)(pappl_job_t *job, pappl_options_t *options, unsigned y, const unsigned char *line);
					// Write a line of raster graphics
typedef int (*pappl_statusfunc_t)(pappl_printer_t *printer);
					// Update printer status

typedef struct pappl_supply_s		// Supply data
{
  const char		*color;		// Colorant, if any
  const char		*description;	// Description
  int			is_consumed;	// Is this a supply that is consumed?
  int			level;		// Level (0-100, -1 = unknown)
  const char		*type;		// Type
} pappl_supply_t;

typedef struct pappl_driver_s		// Driver data
{
  pthread_rwlock_t	rwlock;		// Reader/writer lock
  char			*name;		// Name of driver
  ipp_t			*attrs;		// Capability attributes
  pappl_device_t	*device;	// Connection to device
  void			*job_data;	// Driver job data
  pappl_printfunc_t	print;		// Print (file) function
  pappl_rendjobfunc_t	rendjob;	// End raster job function
  pappl_rendpagefunc_t	rendpage;	// End raster page function
  pappl_rstartjobfunc_t rstartjob;	// Start raster job function
  pappl_rstartpagefunc_t rstartpage;	// Start raster page function
  pappl_rwritefunc_t	rwrite;		// Write raster line function
  pappl_statusfunc_t	status;		// Status function
  const char		*format;	// Printer-specific format
  int			num_resolution,	// Number of printer resolutions
			x_resolution[PAPPL_MAX_RESOLUTION],
			y_resolution[PAPPL_MAX_RESOLUTION];
					// Printer resolutions
  int			left_right,	// Left and right margins in hundredths of millimeters
			bottom_top;	// Bottom and top margins in hundredths of millimeters
  int			num_media;	// Number of supported media
  const char		*media[PAPPL_MAX_MEDIA];
					// Supported media
  pappl_media_col_t	media_default,	// Default media
			media_ready[PAPPL_MAX_SOURCE];
					// Ready media
  int			num_source;	// Number of media sources (rolls)
  const char		*source[PAPPL_MAX_SOURCE];
					// Media sources
  int			top_offset_supported[2];
					// media-top-offset-supported (0,0 for none)
  pappl_media_tracking_t tracking_supported;
					// media-tracking-supported
  int			num_type;	// Number of media types
  const char		*type[PAPPL_MAX_TYPE];
					// Media types
  pappl_label_mode_t	mode_configured,// label-mode-configured
			mode_supported;	// label-mode-supported
  int			tear_offset_configured,
					// label-tear-offset-configured
			tear_offset_supported[2];
					// label-tear-offset-supported (0,0 for none)
  int			speed_supported[2],// print-speed-supported (0,0 for none)
			speed_default;	// print-speed-default
  int			darkness_configured,
					// printer-darkness-configured
			darkness_supported;
					// printer-darkness-supported (0 for none)
  int			num_supply;	// Number of printer-supply
  pappl_supply_t	supply[PAPPL_MAX_SUPPLY];
					// printer-supply
} pappl_driver_t;






//
// Functions...
//








extern void		papplAddOptions(ipp_t *request, int num_options, cups_option_t *options);
extern void		papplAddPrinterURI(ipp_t *request, const char *printer_name, char *resource, size_t rsize);
extern void		papplCheckJobs(pappl_printer_t *printer);
extern void		papplCleanJobs(pappl_system_t *system);
extern http_t		*papplConnect(int auto_start);
extern http_t		*papplConnectURI(const char *printer_uri, char *resource, size_t rsize);
extern void		papplCopyAttributes(ipp_t *to, ipp_t *from, cups_array_t *ra, ipp_tag_t group_tag, int quickcopy);
extern pappl_client_t	*papplCreateClient(pappl_system_t *system, int sock);
extern pappl_job_t	*papplCreateJob(pappl_client_t *client);
extern int		papplCreateJobFile(pappl_job_t *job, char *fname, size_t fnamesize, const char *dir, const char *ext);
extern pappl_printer_t	*papplCreatePrinter(pappl_system_t *system, int printer_id, const char *printer_name, const char *driver_name, const char *device_uri, const char *location, const char *geo_location, const char *organization, const char *org_unit);
extern pappl_system_t	*papplCreateSystem(const char *hostname, int port, const char *subtypes, const char *spooldir, const char *logfile, pappl_loglevel_t loglevel, const char *auth_service, const char *admin_group);
extern void		papplDeleteClient(pappl_client_t *client);
extern void		papplDeleteJob(pappl_job_t *job);
extern void		papplDeletePrinter(pappl_printer_t *printer);
extern void		papplDeleteSystem(pappl_system_t *system);
extern int		papplDoAdd(int num_options, cups_option_t *options);
extern int		papplDoCancel(int num_options, cups_option_t *options);
extern int		papplDoDefault(int num_options, cups_option_t *options);
extern int		papplDoDelete(int num_options, cups_option_t *options);
extern int		papplDoDevices(int num_options, cups_option_t *options);
extern int		papplDoDrivers(int num_options, cups_option_t *options);
extern int		papplDoJobs(int num_options, cups_option_t *options);
extern int		papplDoModify(int num_options, cups_option_t *options);
extern int		papplDoOptions(int num_options, cups_option_t *options);
extern int		papplDoPrinters(int num_options, cups_option_t *options);
extern int		papplDoServer(int num_options, cups_option_t *options);
extern int		papplDoShutdown(int num_options, cups_option_t *options);
extern int		papplDoStatus(int num_options, cups_option_t *options);
extern int		papplDoSubmit(int num_files, char **files, int num_options, cups_option_t *options);
extern pappl_job_t	*papplFindJob(pappl_printer_t *printer, int job_id);
extern pappl_printer_t	*papplFindPrinter(pappl_system_t *system, const char *resource, int printer_id);
extern char		*papplGetDefaultPrinter(http_t *http, char *buffer, size_t bufsize);
extern char		*papplGetServerPath(char *buffer, size_t bufsize);
extern void		papplInitDNSSD(pappl_system_t *system);
extern http_status_t	papplIsAuthorized(pappl_client_t *client);
// Note: Log functions currently only support %d, %p, %s, %u, and %x!
extern void		papplLog(pappl_system_t *system, pappl_loglevel_t level, const char *message, ...);
extern void		papplLogAttributes(pappl_client_t *client, const char *title, ipp_t *ipp, int is_response);
extern void		papplLogClient(pappl_client_t *client, pappl_loglevel_t level, const char *message, ...) PAPPL_FORMAT(3, 4);
extern void		papplLogJob(pappl_job_t *job, pappl_loglevel_t level, const char *message, ...) PAPPL_FORMAT(3, 4);
extern void		papplLogPrinter(pappl_printer_t *printer, pappl_loglevel_t level, const char *message, ...) PAPPL_FORMAT(3, 4);
extern char		*papplMakeUUID(pappl_system_t *system, const char *printer_name, int job_id, char *buffer, size_t bufsize);
extern void		*papplProcessClient(pappl_client_t *client);
extern int		papplProcessHTTP(pappl_client_t *client);
extern int		papplProcessIPP(pappl_client_t *client);
extern void		*papplProcessJob(pappl_job_t *job);
extern unsigned		papplRand(void);
extern int		papplRegisterDNSSD(pappl_printer_t *printer);
extern int		papplRespondHTTP(pappl_client_t *client, http_status_t code, const char *content_coding, const char *type, size_t length);
extern void		papplRespondIPP(pappl_client_t *client, ipp_status_t status, const char *message, ...) PAPPL_FORMAT(3, 4);
extern void		papplRunSystem(pappl_system_t *system);
extern void		papplUnregisterDNSSD(pappl_printer_t *printer);

#endif // !_PAPPL_PAPPL_H_
