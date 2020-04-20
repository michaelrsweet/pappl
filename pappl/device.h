//
// Device communication functions for the Printer Application Framework
//
// Copyright © 2019-2020 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#ifndef _PAPPL_DEVICE_H_
#  define _PAPPL_DEVICE_H_

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
// Callback function types...
//

typedef bool (*pappl_device_cb_t)(const char *device_uri, const char *device_id, void *data);
					// Device callback - return `true` to stop, `false` to continue
typedef void (*pappl_deverr_cb_t)(const char *message, void *err_data);
					// Device error callback


//
// Functions...
//

extern void		papplDeviceClose(pappl_device_t *device) _PAPPL_PUBLIC;
extern pappl_preason_t	papplDeviceGetStatus(pappl_device_t *device) _PAPPL_PUBLIC;
extern bool		papplDeviceList(pappl_device_cb_t cb, void *data, pappl_deverr_cb_t err_cb, void *err_data) _PAPPL_PUBLIC;
extern pappl_device_t	*papplDeviceOpen(const char *device_uri, pappl_deverr_cb_t err_cb, void *err_data) _PAPPL_PUBLIC;
extern ssize_t		papplDevicePrintf(pappl_device_t *device, const char *format, ...) _PAPPL_PUBLIC _PAPPL_FORMAT(2, 3);
extern ssize_t		papplDevicePuts(pappl_device_t *device, const char *s) _PAPPL_PUBLIC;
extern ssize_t		papplDeviceRead(pappl_device_t *device, void *buffer, size_t bytes) _PAPPL_PUBLIC;
extern ssize_t		papplDeviceWrite(pappl_device_t *device, const void *buffer, size_t bytes) _PAPPL_PUBLIC;


//
// C++ magic...
//

#  ifdef __cplusplus
}
#  endif // __cplusplus


#endif // !_PAPPL_DEVICE_H_
