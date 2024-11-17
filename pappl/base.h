//
// Base definitions for the Printer Application Framework
//
// Copyright © 2019-2023 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#ifndef _PAPPL_BASE_H_
#  define _PAPPL_BASE_H_
#  include <cups/cups.h>
#  include <cups/raster.h>
#  include <stdio.h>
#  include <stdlib.h>
#  include <string.h>
#  include <ctype.h>
#  include <errno.h>
#  include <fcntl.h>
#  include <stdbool.h>
#  include <sys/stat.h>
#  if _WIN32
#    include <io.h>
#    include <direct.h>
typedef int gid_t;
typedef int uid_t;
#  else
#    include <unistd.h>
#  endif // _WIN32
#  ifdef __cplusplus
extern "C" {
#  endif // __cplusplus


//
// IPP operations/tags...
//

#  if CUPS_VERSION_MAJOR == 2 && CUPS_VERSION_MINOR < 3
#    define IPP_OP_CREATE_PRINTER		(ipp_op_t)0x004C
#    define IPP_OP_DELETE_PRINTER		(ipp_op_t)0x004E
#    define IPP_OP_GET_PRINTERS			(ipp_op_t)0x004F
#    define IPP_OP_CREATE_SYSTEM_SUBSCRIPTIONS	(ipp_op_t)0x0058
#    define IPP_OP_DISABLE_ALL_PRINTERS		(ipp_op_t)0x0059
#    define IPP_OP_ENABLE_ALL_PRINTERS		(ipp_op_t)0x005A
#    define IPP_OP_GET_SYSTEM_ATTRIBUTES	(ipp_op_t)0x005B
#    define IPP_OP_GET_SYSTEM_SUPPORTED_VALUES	(ipp_op_t)0x005C
#    define IPP_OP_PAUSE_ALL_PRINTERS		(ipp_op_t)0x005D
#    define IPP_OP_PAUSE_ALL_PRINTERS_AFTER_CURRENT_JOB (ipp_op_t)0x005E
#    define IPP_OP_RESUME_ALL_PRINTERS		(ipp_op_t)0x0061
#    define IPP_OP_SET_SYSTEM_ATTRIBUTES	(ipp_op_t)0x0062
#    define IPP_OP_SHUTDOWN_ALL_PRINTERS	(ipp_op_t)0x0063

#    define IPP_TAG_SYSTEM			(ipp_tag_t)0x000A
#  endif // CUPS_VERSION_MAJOR == 2 && CUPS_VERSION_MINOR < 3

#  define IPP_OP_PAPPL_FIND_DEVICES	(ipp_op_t)0x402b
#  define IPP_OP_PAPPL_FIND_DRIVERS	(ipp_op_t)0x402c
#  define IPP_OP_PAPPL_CREATE_PRINTERS	(ipp_op_t)0x402d


//
// Visibility and other annotations...
//

#  if _WIN32
#    define _PAPPL_DEPRECATED(m)
#    define _PAPPL_INTERNAL
#    define _PAPPL_PRIVATE
#    define _PAPPL_PUBLIC
#    define _PAPPL_FORMAT(a,b)
#    define _PAPPL_NONNULL(...)
#    define _PAPPL_NORETURN
#  elif defined(__has_extension) || defined(__GNUC__)
#    define _PAPPL_DEPRECATED(m) __attribute__ ((deprecated(m))) __attribute__ ((visibility("default")))
#    define _PAPPL_INTERNAL	__attribute__ ((visibility("hidden")))
#    define _PAPPL_PRIVATE	__attribute__ ((visibility("default")))
#    define _PAPPL_PUBLIC	__attribute__ ((visibility("default")))
#    define _PAPPL_FORMAT(a,b)	__attribute__ ((__format__(__printf__, a,b)))
#    define _PAPPL_NONNULL(...) __attribute__ ((nonnull(__VA_ARGS__)))
#    define _PAPPL_NORETURN	__attribute__ ((noreturn))
#  else
#    define _PAPPL_DEPRECATED(m)
#    define _PAPPL_INTERNAL
#    define _PAPPL_PRIVATE
#    define _PAPPL_PUBLIC
#    define _PAPPL_FORMAT(a,b)
#    define _PAPPL_NONNULL(...)
#    define _PAPPL_NORETURN
#  endif // __has_extension || __GNUC__


//
// Common types...
//

typedef struct _pappl_client_s pappl_client_t;
					// Client connection object
typedef struct _pappl_device_s pappl_device_t;
					// Device connection object
typedef unsigned char pappl_dither_t[16][16];
                                        // 16x16 dither array
typedef struct pappl_pr_driver_data_s pappl_pr_driver_data_t;
					// Print driver data
typedef struct pappl_sc_driver_data_s pappl_sc_driver_data_t;
          // Scanner driver data
typedef struct _pappl_job_s pappl_job_t;// Job object
typedef struct _pappl_loc_s pappl_loc_t;// Localization data
typedef struct pappl_pr_options_s pappl_pr_options_t;
					// Combined print job options
typedef struct pappl_sc_options_s pappl_sc_options_t;
          // Combined scan job options
typedef unsigned int pappl_preason_t;	// Bitfield for IPP "printer-state-reasons" values
typedef struct _pappl_printer_s pappl_printer_t;
					// Printer object
typedef struct _pappl_scanner_s pappl_scanner_t;
          // Scanner object
typedef struct _pappl_subscription_s pappl_subscription_t;
					// Subscription object
typedef struct _pappl_system_s pappl_system_t;
					// System object

typedef struct pappl_contact_s		// Contact information
{
  char	name[256];				// Contact name
  char	email[256];				// Contact email address
  char	telephone[256];				// Contact phone number
} pappl_contact_t;

enum pappl_loptions_e			// Link option bits
{
  PAPPL_LOPTIONS_NAVIGATION = 0x0001,		// Link shown in navigation bar
  PAPPL_LOPTIONS_CONFIGURATION = 0x0002,	// Link shown in configuration section
  PAPPL_LOPTIONS_JOB = 0x0004,			// Link shown in job(s) section
  PAPPL_LOPTIONS_LOGGING = 0x0008,		// Link shown in logging section
  PAPPL_LOPTIONS_NETWORK = 0x0010,		// Link shown in network section
  PAPPL_LOPTIONS_PRINTER = 0x0020,		// Link shown in printer(s) section
  PAPPL_LOPTIONS_SECURITY = 0x0040,		// Link shown in security section
  PAPPL_LOPTIONS_STATUS = 0x0080,		// Link shown in status section
  PAPPL_LOPTIONS_TLS = 0x0100,			// Link shown in TLS section
  PAPPL_LOPTIONS_OTHER = 0x0200,		// Link shown in other section
  PAPPL_LOPTIONS_HTTPS_REQUIRED = 0x8000	// Link requires HTTPS
};
typedef unsigned short pappl_loptions_t;// Bitfield for link options

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
  PAPPL_SUPPLY_COLOR_YELLOW,			// Yellow ink/toner
  PAPPL_SUPPLY_COLOR_MULTIPLE			// Multiple-color ink
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
  PAPPL_SUPPLY_TYPE_WATER,			// Water supply
  PAPPL_SUPPLY_TYPE_GLUE_WATER_ADDITIVE,	// Glue water additive
  PAPPL_SUPPLY_TYPE_WASTE_PAPER,		// Waste paper
  PAPPL_SUPPLY_TYPE_SHRINK_WRAP,		// Shrink wrap
  PAPPL_SUPPLY_TYPE_OTHER,			// Other supply
  PAPPL_SUPPLY_TYPE_UNKNOWN			// Unknown supply
} pappl_supply_type_t;

typedef struct pappl_supply_s		// Supply data
{
  pappl_supply_color_t	color;			// Color, if any
  char			description[256];	// Description
  bool			is_consumed;		// Is this a supply that is consumed?
  int			level;			// Level (0-100, -1 = unknown)
  pappl_supply_type_t	type;			// Type
} pappl_supply_t;


//
// Utility functions...
//

extern size_t		papplCopyString(char *dst, const char *src, size_t dstsize) _PAPPL_PUBLIC;
extern int		papplCreateTempFile(char *fname, size_t fnamesize, const char *prefix, const char *ext) _PAPPL_PUBLIC;
extern unsigned		papplGetRand(void) _PAPPL_PUBLIC;
extern const char	*papplGetTempDir(void) _PAPPL_PUBLIC;


#  ifdef __cplusplus
}
#  endif // __cplusplus
#endif // !_PAPPL_BASE_H_
