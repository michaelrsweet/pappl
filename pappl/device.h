//
// Device communication functions for the Printer Application Framework
//
// Copyright © 2019-2022 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#ifndef _PAPPL_DEVICE_H_
#  define _PAPPL_DEVICE_H_
#  include "base.h"
#  ifdef __cplusplus
extern "C" {
#  endif // __cplusplus


//
// Types...
//

typedef struct pappl_devmetrics_s	// Device metrics
{
  size_t	read_bytes;			// Total number of bytes read
  size_t	read_requests;			// Total number of read requests
  size_t	read_msecs;			// Total number of milliseconds spent reading
  size_t	status_requests;		// Total number of status requests
  size_t	status_msecs;			// Total number of milliseconds spent getting status
  size_t	write_bytes;			// Total number of bytes written
  size_t	write_requests;			// Total number of write requests
  size_t	write_msecs;			// Total number of milliseconds spent writing
} pappl_devmetrics_t;

enum pappl_devtype_e			// Device type bit values
{
  PAPPL_DEVTYPE_FILE = 0x01,			// Local file/directory
  PAPPL_DEVTYPE_USB = 0x02,			// USB printers
  PAPPL_DEVTYPE_SERIAL = 0x04,			// Serial printers (not currently implemented) @private@
  PAPPL_DEVTYPE_CUSTOM_LOCAL = 0x08,		// Local printer using a custom interface or protocol
  PAPPL_DEVTYPE_SOCKET = 0x10,			// Network printers using raw socket
  PAPPL_DEVTYPE_DNS_SD = 0x20,			// Network printers discovered via DNS-SD/mDNS
  PAPPL_DEVTYPE_SNMP = 0x40,			// Network printers discovered via SNMP
  PAPPL_DEVTYPE_CUSTOM_NETWORK = 0x80,		// Network printer using a custom interface or protocol
  PAPPL_DEVTYPE_LOCAL = 0x0f,			// All local printers
  PAPPL_DEVTYPE_NETWORK = 0xf0,			// All network printers
  PAPPL_DEVTYPE_ALL = 0xff			// All printers
};
typedef unsigned pappl_devtype_t;		// Device type bitfield

typedef bool (*pappl_device_cb_t)(const char *device_info, const char *device_uri, const char *device_id, void *data);
					// Device callback - return `true` to stop, `false` to continue
typedef void (*pappl_devclose_cb_t)(pappl_device_t *device);
					// Device close callback
typedef void (*pappl_deverror_cb_t)(const char *message, void *err_data);
					// Device error callback
typedef char *(*pappl_devid_cb_t)(pappl_device_t *device, char *buffer, size_t bufsize);
					// Device ID callback
typedef bool (*pappl_devlist_cb_t)(pappl_device_cb_t cb, void *data, pappl_deverror_cb_t err_cb, void *err_data);
					// Device list callback
typedef bool (*pappl_devopen_cb_t)(pappl_device_t *device, const char *device_uri, const char *name);
					// Device open callback
typedef ssize_t (*pappl_devread_cb_t)(pappl_device_t *device, void *buffer, size_t bytes);
					// Device read callback
typedef pappl_preason_t (*pappl_devstatus_cb_t)(pappl_device_t *device);
					// Device status callback
typedef int (*pappl_devsupplies_cb_t)(pappl_device_t *device, int max_supplies, pappl_supply_t *supplies);
					// Device supplies callback
typedef ssize_t (*pappl_devwrite_cb_t)(pappl_device_t *device, const void *buffer, size_t bytes);
					// Device write callback


//
// Functions...
//

extern void		papplDeviceAddScheme(const char *scheme, pappl_devtype_t dtype, pappl_devlist_cb_t list_cb, pappl_devopen_cb_t open_cb, pappl_devclose_cb_t close_cb, pappl_devread_cb_t read_cb, pappl_devwrite_cb_t write_cb, pappl_devstatus_cb_t status_cb, pappl_devid_cb_t id_cb) _PAPPL_PUBLIC;
extern void		papplDeviceAddScheme2(const char *scheme, pappl_devtype_t dtype, pappl_devlist_cb_t list_cb, pappl_devopen_cb_t open_cb, pappl_devclose_cb_t close_cb, pappl_devread_cb_t read_cb, pappl_devwrite_cb_t write_cb, pappl_devstatus_cb_t status_cb, pappl_devsupplies_cb_t supplies_cb, pappl_devid_cb_t id_cb) _PAPPL_PUBLIC;
extern void		papplDeviceClose(pappl_device_t *device) _PAPPL_PUBLIC;
extern void		papplDeviceError(pappl_device_t *device, const char *message, ...) _PAPPL_PUBLIC _PAPPL_FORMAT(2,3);
extern void		papplDeviceFlush(pappl_device_t *device) _PAPPL_PUBLIC;
extern void		*papplDeviceGetData(pappl_device_t *device) _PAPPL_PUBLIC;
extern char		*papplDeviceGetID(pappl_device_t *device, char *buffer, size_t bufsize) _PAPPL_PUBLIC;
extern pappl_devmetrics_t *papplDeviceGetMetrics(pappl_device_t *device, pappl_devmetrics_t *metrics) _PAPPL_PUBLIC;
extern pappl_preason_t	papplDeviceGetStatus(pappl_device_t *device) _PAPPL_PUBLIC;
extern int		papplDeviceGetSupplies(pappl_device_t *device, int max_supplies, pappl_supply_t *supplies) _PAPPL_PUBLIC;
extern bool		papplDeviceIsSupported(const char *uri) _PAPPL_PUBLIC;
extern bool		papplDeviceList(pappl_devtype_t types, pappl_device_cb_t cb, void *data, pappl_deverror_cb_t err_cb, void *err_data) _PAPPL_PUBLIC;
extern pappl_device_t	*papplDeviceOpen(const char *device_uri, const char *name, pappl_deverror_cb_t err_cb, void *err_data) _PAPPL_PUBLIC;
extern int		papplDeviceParseID(const char *device_id, cups_option_t **pairs) _PAPPL_PUBLIC;
extern ssize_t		papplDevicePrintf(pappl_device_t *device, const char *format, ...) _PAPPL_PUBLIC _PAPPL_FORMAT(2, 3);
extern ssize_t		papplDevicePuts(pappl_device_t *device, const char *s) _PAPPL_PUBLIC;
extern ssize_t		papplDeviceRead(pappl_device_t *device, void *buffer, size_t bytes) _PAPPL_PUBLIC;
extern void		papplDeviceRemoveScheme(const char *scheme) _PAPPL_PUBLIC;
extern void		papplDeviceRemoveTypes(pappl_devtype_t types) _PAPPL_PUBLIC;
extern void		papplDeviceSetData(pappl_device_t *device, void *data) _PAPPL_PUBLIC;
extern ssize_t		papplDeviceWrite(pappl_device_t *device, const void *buffer, size_t bytes) _PAPPL_PUBLIC;


#  ifdef __cplusplus
}
#  endif // __cplusplus
#endif // !_PAPPL_DEVICE_H_
