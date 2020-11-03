//
// Private device communication functions for the Printer Application Framework
//
// Copyright © 2019-2020 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#ifndef _PAPPL_DEVICE_PRIVATE_H_
#  define _PAPPL_DEVICE_PRIVATE_H_

//
// Include necessary headers...
//

#  include "base-private.h"
#  include "device.h"


//
// C++ magic...
//

#  ifdef __cplusplus
extern "C" {
#  endif // __cplusplus


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
  pappl_devwrite_cb_t	write_cb;		// Write callback

  void			*device_data,		// Data pointer for device
			*error_data;		// Data pointer for error callback

  char			buffer[PAPPL_DEVICE_BUFSIZE];
						// Write buffer
  size_t		bufused;		// Number of bytes in write buffer
  pappl_devmetrics_t	metrics;		// Device metrics
};

typedef void (*_pappl_devscheme_cb_t)(const char *scheme, void *data);


//
// Functions...
//

extern void		_papplDeviceAddFileScheme(void) _PAPPL_PRIVATE;
extern void		_papplDeviceAddNetworkSchemes(void) _PAPPL_PRIVATE;
extern void		_papplDeviceAddSupportedSchemes(ipp_t *attrs);
extern void		_papplDeviceAddUSBScheme(void) _PAPPL_PRIVATE;
extern void		_papplDeviceError(pappl_deverror_cb_t err_cb, void *err_data, const char *message, ...) _PAPPL_FORMAT(3,4) _PAPPL_PRIVATE;


//
// C++ magic...
//

#  ifdef __cplusplus
}
#  endif // __cplusplus


#endif // !_PAPPL_DEVICE_H_
