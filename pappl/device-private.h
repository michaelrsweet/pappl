//
// Private device communication functions for the Printer Application Framework
//
// Copyright © 2019-2023 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#ifndef _PAPPL_DEVICE_PRIVATE_H_
#  define _PAPPL_DEVICE_PRIVATE_H_
#  include "base-private.h"
#  include "device.h"


//
// Constants...
//

#define PAPPL_DEVICE_BUFSIZE	8192	// Size of write buffer


//
// Types...
//

struct _pappl_device_s			// Device connection data
{
  pappl_devclose_cb_t	close_cb;		// Close callback
  pappl_deverror_cb_t	error_cb;		// Error callback
  pappl_devid_cb_t	id_cb;			// IEEE-1284 device ID callback
  pappl_devread_cb_t	read_cb;		// Read callback
  pappl_devstatus_cb_t	status_cb;		// Status callback
  pappl_devsupplies_cb_t supplies_cb;		// Supplies callback
  pappl_devwrite_cb_t	write_cb;		// Write callback

  void			*device_data,		// Data pointer for device
			*error_data;		// Data pointer for error callback

  char			buffer[PAPPL_DEVICE_BUFSIZE];
						// Write buffer
  size_t		bufused;		// Number of bytes in write buffer
  pappl_devmetrics_t	metrics;		// Device metrics
};

typedef void (*_pappl_devscheme_cb_t)(const char *scheme, void *data);

typedef struct _pappl_dinfo_s		// Device information
{
  char	*device_info,			// Device
	*device_uri,			// Device URI string
	*device_id;			// IEEE-1284 device ID string
} _pappl_dinfo_t;


//
// Functions...
//

extern void		_papplDeviceAddFileSchemeNoLock(void) _PAPPL_PRIVATE;
extern void		_papplDeviceAddNetworkSchemesNoLock(void) _PAPPL_PRIVATE;
extern void		_papplDeviceAddSchemeNoLock(const char *scheme, pappl_devtype_t dtype, pappl_devlist_cb_t list_cb, pappl_devopen_cb_t open_cb, pappl_devclose_cb_t close_cb, pappl_devread_cb_t read_cb, pappl_devwrite_cb_t write_cb, pappl_devstatus_cb_t status_cb, pappl_devsupplies_cb_t supplies_cb, pappl_devid_cb_t id_cb) _PAPPL_PRIVATE;
extern void		_papplDeviceAddSupportedSchemes(ipp_t *attrs) _PAPPL_PRIVATE;
extern void		_papplDeviceAddUSBSchemeNoLock(void) _PAPPL_PRIVATE;
extern void		_papplDeviceError(pappl_deverror_cb_t err_cb, void *err_data, const char *message, ...) _PAPPL_FORMAT(3,4) _PAPPL_PRIVATE;
extern bool		_papplDeviceInfoCallback(const char *device_info, const char *device_uri, const char *device_id, cups_array_t *devices) _PAPPL_PRIVATE;
extern cups_array_t	*_papplDeviceInfoCreateArray(void) _PAPPL_PRIVATE;


#endif // !_PAPPL_DEVICE_H_
