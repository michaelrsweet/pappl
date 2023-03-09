//
// Common device support code for the Printer Application Framework
//
// Copyright © 2019-2023 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

//
// Include necessary headers...
//

#include "device-private.h"
#include "printer.h"
#include <stdarg.h>


//
// Types...
//

typedef struct _pappl_devscheme_s	// Device scheme data
{
  char			*scheme;		// URI scheme
  pappl_devtype_t	dtype;			// Device type
  pappl_devlist_cb_t	list_cb;		// List devices callback, if any
  pappl_devopen_cb_t	open_cb;		// Open callback
  pappl_devclose_cb_t	close_cb;		// Close callback
  pappl_devread_cb_t	read_cb;		// Read callback
  pappl_devwrite_cb_t	write_cb;		// Write callback
  pappl_devid_cb_t	id_cb;			// IEEE-1284 device ID callback, if any
  pappl_devstatus_cb_t	status_cb;		// Status callback, if any
  pappl_devsupplies_cb_t supplies_cb;		// Supplies callback, if any
} _pappl_devscheme_t;


//
// Local globals...
//

static pthread_rwlock_t	device_rwlock = PTHREAD_RWLOCK_INITIALIZER;
					// Reader/writer lock for device schemes
static cups_array_t	*device_schemes = NULL;
					// Array of device schemes


//
// Local functions...
//

static int		pappl_compare_schemes(_pappl_devscheme_t *a, _pappl_devscheme_t *b);
static void		pappl_create_schemes_no_lock(void);
static void		pappl_default_error_cb(const char *message, void *data);
static void		pappl_free_dinfo(_pappl_dinfo_t *d);
static ssize_t		pappl_write(pappl_device_t *device, const void *buffer, size_t bytes);


//
// '_papplDeviceAddSchemeNoLock()' - Add a device URI scheme with supply-level queries.
//
// This function registers a device URI scheme with PAPPL, so that devices using
// the named scheme can receive print data, report status information, and so
// forth.  PAPPL includes support for the following URI schemes:
//
// - `dnssd`: Network printers discovered using DNS-SD.
// - `file`: Character device files, plain files, and directories.
// - `snmp`: Network printers discovered using SNMPv1.
// - `socket`: Network printers using a hostname or numeric IP address.
// - `usb`: Class 1 (unidirectional) or 2 (bidirectional) USB printers.
//
// The "scheme" parameter specifies the URI scheme and must consist of lowercase
// letters, digits, "-", "_", and/or ".", for example "x-foo" or
// "com.example.bar".
//
// The "dtype" parameter specifies the device type and should be
// `PAPPL_DTYPE_CUSTOM_LOCAL` for locally connected printers and
// `PAPPL_DTYPE_CUSTOM_NETWORK` for network printers.
//
// Each of the callbacks corresponds to one of the `papplDevice` functions:
//
// - "list_cb": Implements discovery of devices (optional)
// - "open_cb": Opens communication with a device and allocates any device-
//   specific data as needed
// - "close_cb": Closes communication with a device and frees any device-
//   specific data as needed
// - "read_cb": Reads data from a device
// - "write_cb": Write data to a device
// - "status_cb": Gets basic printer state information from a device (optional)
// - "supplies_cb": Gets supply level information from a device (optional)
// - "id_cb": Gets the current IEEE-1284 device ID from a device (optional)
//
// The "open_cb" callback typically calls @link papplDeviceSetData@ to store a
// pointer to contextual information for the connection while the "close_cb",
// "id_cb", "read_cb", "write_cb", "status_cb", and "supplies_cb" callbacks
// typically call @link papplDeviceGetData@ to retrieve it.
//

void
_papplDeviceAddSchemeNoLock(
    const char             *scheme,	// I - URI scheme
    pappl_devtype_t        dtype,	// I - Device type (`PAPPL_DEVTYPE_CUSTOM_LOCAL` or `PAPPL_DEVTYPE_CUSTOM_NETWORK`)
    pappl_devlist_cb_t     list_cb,	// I - List devices callback, if any
    pappl_devopen_cb_t     open_cb,	// I - Open callback
    pappl_devclose_cb_t    close_cb,	// I - Close callback
    pappl_devread_cb_t     read_cb,	// I - Read callback
    pappl_devwrite_cb_t    write_cb,	// I - Write callback
    pappl_devstatus_cb_t   status_cb,	// I - Status callback, if any
    pappl_devsupplies_cb_t supplies_cb,	// I - Supply level callback, if any
    pappl_devid_cb_t       id_cb)	// I - IEEE-1284 device ID callback, if any
{
  _pappl_devscheme_t	*ds,		// Device URI scheme data
			dkey;		// Search key


  // Create the schemes array as needed...
  if (!device_schemes)
  {
    if ((device_schemes = cupsArrayNew((cups_array_cb_t)pappl_compare_schemes, NULL, NULL, 0, NULL, NULL)) == NULL)
      return;
  }

  dkey.scheme = (char *)scheme;

  if ((ds = (_pappl_devscheme_t *)cupsArrayFind(device_schemes, &dkey)) == NULL)
  {
    // Add the scheme...
    if ((ds = (_pappl_devscheme_t *)calloc(1, sizeof(_pappl_devscheme_t))) == NULL)
      return;

    if ((ds->scheme = strdup(scheme)) == NULL)
    {
      free(ds);
      return;
    }

    cupsArrayAdd(device_schemes, ds);
  }

  ds->dtype       = dtype;
  ds->list_cb     = list_cb;
  ds->open_cb     = open_cb;
  ds->close_cb    = close_cb;
  ds->read_cb     = read_cb;
  ds->write_cb    = write_cb;
  ds->status_cb   = status_cb;
  ds->supplies_cb = supplies_cb;
  ds->id_cb       = id_cb;
}


//
// 'papplDeviceAddScheme()' - Add a device URI scheme.
//
// This function registers a device URI scheme with PAPPL, so that devices using
// the named scheme can receive print data, report status information, and so
// forth.  PAPPL includes support for the following URI schemes:
//
// - `dnssd`: Network printers discovered using DNS-SD.
// - `file`: Character device files, plain files, and directories.
// - `snmp`: Network printers discovered using SNMPv1.
// - `socket`: Network printers using a hostname or numeric IP address.
// - `usb`: Class 1 (unidirectional) or 2 (bidirectional) USB printers.
//
// The "scheme" parameter specifies the URI scheme and must consist of lowercase
// letters, digits, "-", "_", and/or ".", for example "x-foo" or
// "com.example.bar".
//
// The "dtype" parameter specifies the device type and should be
// `PAPPL_DTYPE_CUSTOM_LOCAL` for locally connected printers and
// `PAPPL_DTYPE_CUSTOM_NETWORK` for network printers.
//
// Each of the callbacks corresponds to one of the `papplDevice` functions:
//
// - "list_cb": Implements discovery of devices (optional)
// - "open_cb": Opens communication with a device and allocates any device-
//   specific data as needed
// - "close_cb": Closes communication with a device and frees any device-
//   specific data as needed
// - "read_cb": Reads data from a device
// - "write_cb": Write data to a device
// - "status_cb": Gets basic printer state information from a device (optional)
// - "id_cb": Gets the current IEEE-1284 device ID from a device (optional)
//
// The "open_cb" callback typically calls @link papplDeviceSetData@ to store a
// pointer to contextual information for the connection while the "close_cb",
// "id_cb", "read_cb", "write_cb", and "status_cb" callbacks typically call
// @link papplDeviceGetData@ to retrieve it.
//

void
papplDeviceAddScheme(
    const char           *scheme,	// I - URI scheme
    pappl_devtype_t      dtype,		// I - Device type (`PAPPL_DEVTYPE_CUSTOM_LOCAL` or `PAPPL_DEVTYPE_CUSTOM_NETWORK`)
    pappl_devlist_cb_t   list_cb,	// I - List devices callback, if any
    pappl_devopen_cb_t   open_cb,	// I - Open callback
    pappl_devclose_cb_t  close_cb,	// I - Close callback
    pappl_devread_cb_t   read_cb,	// I - Read callback
    pappl_devwrite_cb_t  write_cb,	// I - Write callback
    pappl_devstatus_cb_t status_cb,	// I - Status callback, if any
    pappl_devid_cb_t     id_cb)		// I - IEEE-1284 device ID callback, if any
{
  papplDeviceAddScheme2(scheme, dtype, list_cb, open_cb, close_cb, read_cb, write_cb, status_cb, /*supplies_cb*/NULL, id_cb);
}


//
// 'papplDeviceAddScheme2()' - Add a device URI scheme with supply-level queries.
//
// This function registers a device URI scheme with PAPPL, so that devices using
// the named scheme can receive print data, report status information, and so
// forth.  PAPPL includes support for the following URI schemes:
//
// - `dnssd`: Network printers discovered using DNS-SD.
// - `file`: Character device files, plain files, and directories.
// - `snmp`: Network printers discovered using SNMPv1.
// - `socket`: Network printers using a hostname or numeric IP address.
// - `usb`: Class 1 (unidirectional) or 2 (bidirectional) USB printers.
//
// The "scheme" parameter specifies the URI scheme and must consist of lowercase
// letters, digits, "-", "_", and/or ".", for example "x-foo" or
// "com.example.bar".
//
// The "dtype" parameter specifies the device type and should be
// `PAPPL_DTYPE_CUSTOM_LOCAL` for locally connected printers and
// `PAPPL_DTYPE_CUSTOM_NETWORK` for network printers.
//
// Each of the callbacks corresponds to one of the `papplDevice` functions:
//
// - "list_cb": Implements discovery of devices (optional)
// - "open_cb": Opens communication with a device and allocates any device-
//   specific data as needed
// - "close_cb": Closes communication with a device and frees any device-
//   specific data as needed
// - "read_cb": Reads data from a device
// - "write_cb": Write data to a device
// - "status_cb": Gets basic printer state information from a device (optional)
// - "supplies_cb": Gets supply level information from a device (optional)
// - "id_cb": Gets the current IEEE-1284 device ID from a device (optional)
//
// The "open_cb" callback typically calls @link papplDeviceSetData@ to store a
// pointer to contextual information for the connection while the "close_cb",
// "id_cb", "read_cb", "write_cb", "status_cb", and "supplies_cb" callbacks
// typically call @link papplDeviceGetData@ to retrieve it.
//

void
papplDeviceAddScheme2(
    const char             *scheme,	// I - URI scheme
    pappl_devtype_t        dtype,	// I - Device type (`PAPPL_DEVTYPE_CUSTOM_LOCAL` or `PAPPL_DEVTYPE_CUSTOM_NETWORK`)
    pappl_devlist_cb_t     list_cb,	// I - List devices callback, if any
    pappl_devopen_cb_t     open_cb,	// I - Open callback
    pappl_devclose_cb_t    close_cb,	// I - Close callback
    pappl_devread_cb_t     read_cb,	// I - Read callback
    pappl_devwrite_cb_t    write_cb,	// I - Write callback
    pappl_devstatus_cb_t   status_cb,	// I - Status callback, if any
    pappl_devsupplies_cb_t supplies_cb,	// I - Supply level callback, if any
    pappl_devid_cb_t       id_cb)	// I - IEEE-1284 device ID callback, if any
{
  pthread_rwlock_wrlock(&device_rwlock);

  // Create the schemes array as needed...
  if (!device_schemes)
    pappl_create_schemes_no_lock();

  _papplDeviceAddSchemeNoLock(scheme, dtype, list_cb, open_cb, close_cb, read_cb, write_cb, status_cb, supplies_cb, id_cb);

  pthread_rwlock_unlock(&device_rwlock);
}


//
// '_papplDeviceAddSupportedSchemes()' - Add the available URI schemes.
//

void
_papplDeviceAddSupportedSchemes(
    ipp_t *attrs)			// I - Attributes
{
  cups_len_t		i;		// Looping var
  ipp_attribute_t	*attr;		// IPP attribute
  _pappl_devscheme_t	*devscheme;	// Current device scheme


  pthread_rwlock_rdlock(&device_rwlock);

  if (!device_schemes)
    pappl_create_schemes_no_lock();

  attr = ippAddStrings(attrs, IPP_TAG_SYSTEM, IPP_TAG_URISCHEME, "smi55357-device-uri-schemes-supported", IPP_NUM_CAST cupsArrayGetCount(device_schemes), NULL, NULL);

  for (i = 0, devscheme = (_pappl_devscheme_t *)cupsArrayGetFirst(device_schemes); devscheme; i ++, devscheme = (_pappl_devscheme_t *)cupsArrayGetNext(device_schemes))
    ippSetString(attrs, &attr, i, devscheme->scheme);

  pthread_rwlock_unlock(&device_rwlock);
}


//
// 'papplDeviceClose()' - Close a device connection.
//
// This function flushes any pending write data and closes the connection to a
// device.
//

void
papplDeviceClose(
    pappl_device_t *device)		// I - Device to close
{
  if (device)
  {
    if (device->bufused > 0)
      pappl_write(device, device->buffer, device->bufused);

    (device->close_cb)(device);
    free(device);
  }
}


//
// '_papplDeviceError()' - Report an error.
//

void
_papplDeviceError(
    pappl_deverror_cb_t err_cb,		// I - Error callback
    void                *err_data,	// I - Error callback data
    const char          *message,	// I - Printf-style message
    ...)				// I - Additional args as needed
{
  va_list	ap;			// Pointer to additional args
  char		buffer[8192];		// Formatted message


  if (!err_cb)
    return;

  va_start(ap, message);
  vsnprintf(buffer, sizeof(buffer), message, ap);
  va_end(ap);

  (*err_cb)(buffer, err_data);
}


//
// 'papplDeviceError()' - Report an error on a device.
//
// This function reports an error on a device using the client-supplied callback
// function.  It is normally called from any custom device URI scheme callbacks
// you implement.
//

void
papplDeviceError(
    pappl_device_t *device,		// I - Device
    const char     *message,		// I - Printf-style error message
    ...)				// I - Additional arguments as needed
{
  va_list	ap;			// Pointer to additional args
  char		buffer[8192];		// Formatted message


  if (!device || !device->error_cb)
    return;

  va_start(ap, message);
  vsnprintf(buffer, sizeof(buffer), message, ap);
  va_end(ap);

  (device->error_cb)(buffer, device->error_data);
}


//
// 'papplDeviceFlush()' - Flush any buffered data to the device.
//
// This function flushes any pending write data sent using the
// @link papplDevicePrintf@, @link papplDevicePuts@, or @link papplDeviceWrite@
// functions to the device.
//


void
papplDeviceFlush(pappl_device_t *device)// I - Device
{
  if (device && device->bufused > 0)
  {
    pappl_write(device, device->buffer, device->bufused);
    device->bufused = 0;
  }
}


//
// 'papplDeviceGetData()' - Get device-specific data.
//
// This function returns any device-specific data that has been set by the
// device open callback.  It is normally only called from any custom device URI
// scheme callbacks you implement.
//

void *					// O - Device data pointer
papplDeviceGetData(
    pappl_device_t *device)		// I - Device
{
  return (device ? device->device_data : NULL);
}


//
// 'papplDeviceGetID()' - Get the IEEE-1284 device ID.
//
// This function queries the IEEE-1284 device ID from the device and copies it
// to the provided buffer.  The buffer must be at least 64 bytes and should be
// at least 1024 bytes in length.
//
// > *Note:* This function can block for up to several seconds depending on
// > the type of connection.
//

char *					// O - IEEE-1284 device ID or `NULL` on failure
papplDeviceGetID(
    pappl_device_t *device,		// I - Device
    char           *buffer,		// I - Buffer for IEEE-1284 device ID
    size_t         bufsize)		// I - Size of buffer
{
  struct timeval	starttime,	// Start time
			endtime;	// End time
  char			*ret;		// Return value


  // Range check input...
  if (buffer)
    *buffer = '\0';

  if (!device || !device->id_cb || !buffer || bufsize < 64)
    return (NULL);

  // Get the device ID and collect timing metrics...
  gettimeofday(&starttime, NULL);

  ret = (device->id_cb)(device, buffer, bufsize);

  gettimeofday(&endtime, NULL);

  device->metrics.status_requests ++;
  device->metrics.status_msecs += (size_t)(1000 * (endtime.tv_sec - starttime.tv_sec) + (endtime.tv_usec - starttime.tv_usec) / 1000);

  // Return the device ID
  return (ret);
}


//
// 'papplDeviceGetMetrics()' - Get the device metrics.
//
// This function returns a copy of the device metrics data, which includes the
// number, length (in bytes), and duration (in milliseconds) of read, status,
// and write requests for the current session.  This information is normally
// used for performance measurement and optimization during development of a
// printer application.  It can also be useful diagnostic information.
//

pappl_devmetrics_t *			// O - Metrics data
papplDeviceGetMetrics(
    pappl_device_t     *device,		// I - Device
    pappl_devmetrics_t *metrics)	// I - Buffer for metrics data
{
  if (device && metrics)
    memcpy(metrics, &device->metrics, sizeof(pappl_devmetrics_t));
  else if (metrics)
    memset(metrics, 0, sizeof(pappl_devmetrics_t));

  return (metrics);
}


//
// 'papplDeviceGetDeviceStatus()' - Get the printer status bits.
//
// This function returns the current printer status bits, as applicable to the
// current device.
//
// The status bits for USB devices come from the original Centronics parallel
// printer "standard" which was later formally standardized in IEEE 1284-1984
// and the USB Device Class Definition for Printing Devices.  Some vendor
// extensions are also supported.
//
// The status bits for network devices come from the hrPrinterDetectedErrorState
// property that is defined in the SNMP Printer MIB v2 (RFC 3805).
//
// This function returns a @link pappl_preason_t@ bitfield which can be
// passed to the @link papplPrinterSetReasons@ function.  Use the
// @link PAPPL_PREASON_DEVICE_STATUS@ value as the value of the "remove"
// argument.
//
// > Note: This function can block for several seconds while getting the status
// > information.
//

pappl_preason_t				// O - IPP "printer-state-reasons" values
papplDeviceGetStatus(
    pappl_device_t *device)		// I - Device
{
  struct timeval	starttime,	// Start time
			endtime;	// End time
  pappl_preason_t	status = PAPPL_PREASON_NONE;
					// IPP "printer-state-reasons" values


  if (device)
  {
    gettimeofday(&starttime, NULL);

    if (device->status_cb)
      status = (device->status_cb)(device);

    gettimeofday(&endtime, NULL);

    device->metrics.status_requests ++;
    device->metrics.status_msecs += (size_t)(1000 * (endtime.tv_sec - starttime.tv_sec) + (endtime.tv_usec - starttime.tv_usec) / 1000);
  }

  return (status);
}


//
// 'papplDeviceGetSupplies()' - Get the current printer supplies.
//
// This function returns the number, type, and level of current printer supply
// levels, as applicable to the current device.
//
// The supply levels for network devices come from the prtSupplyTable and
// prtMarkerColorantTable properties that are defined in the SNMP Printer MIB
// v2 (RFC 3805).
//
// The supply levels for other devices are not standardized and must be queried
// using other methods.
//
// > Note: This function can block for several seconds while getting the supply
// > information.
//

int					// O - Number of supplies
papplDeviceGetSupplies(
    pappl_device_t *device,		// I - Device
    int            max_supplies,	// I - Maximum supplies
    pappl_supply_t *supplies)		// I - Supplies
{
  if (device && device->supplies_cb)
    return ((device->supplies_cb)(device, max_supplies, supplies));
  else
    return (0);
}


//
// '_papplDeviceInfoCallback()' - Add device information to the array.
//

bool					// O - `true` to continue, `false` to stop
_papplDeviceInfoCallback(
    const char   *device_info,		// I - Device info
    const char   *device_uri,		// I - Device URI
    const char   *device_id,		// I - Device ID
    cups_array_t *devices)		// I - Callback data (devices array)
{
  _pappl_dinfo_t	*d;		// Found device data


  // Allocate a found device...
  if ((d = malloc(sizeof(_pappl_dinfo_t))) != NULL)
  {
    // Make copies of the strings...
    d->device_info = strdup(device_info);
    d->device_uri  = strdup(device_uri);
    d->device_id   = strdup(device_id);

    if (d->device_info && d->device_uri && d->device_id)
    {
      // Add to device array
      cupsArrayAdd(devices, d);
    }
    else
    {
      // Free what got copied...
      free(d->device_info);
      free(d->device_uri);
      free(d->device_id);
      free(d);
    }
  }

  return (false);
}


//
// '_papplDeviceInfoCreateArray()' - Create an array for device information.
//

cups_array_t *				// O - Device info array
_papplDeviceInfoCreateArray(void)
{
  return (cupsArrayNew(/*compare_cb*/NULL, /*cb_data*/NULL, /*hash_cb*/NULL, /*hash_size*/0, /*copy_cb*/NULL, (cups_afree_cb_t)pappl_free_dinfo));
}


//
// 'papplDeviceIsSupported()' - Determine whether a given URI is supported.
//
// This function determines whether a given URI or URI scheme is supported as
// a device.
//

bool					// O - `true` if supported, `false` otherwise
papplDeviceIsSupported(
    const char *uri)			// I - URI
{
  char			scheme[32],	// Device scheme
			userpass[32],	// Device user/pass
			host[256],	// Device host
			resource[256];	// Device resource
  int			port;		// Device port
  _pappl_devscheme_t	key,		// Device search key
			*match;		// Matching key


  // Separate out the components of the URI...
  if (httpSeparateURI(HTTP_URI_CODING_ALL, uri, scheme, sizeof(scheme), userpass, sizeof(userpass), host, sizeof(host), &port, resource, sizeof(resource)) < HTTP_URI_STATUS_OK)
    return (false);

  // Files are OK if the resource path is writable...
  if (!strcmp(scheme, "file"))
  {
    char *options = strchr(resource, '?');
					// Device options, if any

    if (options)
      *options = '\0';			// Strip options before writability test

    return (!access(resource, W_OK));
  }

  // Make sure schemes are added...
  pthread_rwlock_rdlock(&device_rwlock);

  if (!device_schemes)
  {
    pthread_rwlock_unlock(&device_rwlock);
    pthread_rwlock_wrlock(&device_rwlock);

    if (!device_schemes)
      pappl_create_schemes_no_lock();
  }

  // Otherwise try to lookup the URI scheme...
  key.scheme = scheme;
  match      = (_pappl_devscheme_t *)cupsArrayFind(device_schemes, &key);

  pthread_rwlock_unlock(&device_rwlock);

  return (match != NULL);
}


//
// 'papplDeviceList()' - List available devices.
//
// This function lists the available devices, calling the "cb" function once per
// device that is discovered/listed.  The callback function receives the device
// URI, IEEE-1284 device ID (if any), and "data" pointer, and returns `true` to
// stop listing devices and `false` to continue.
//
// The "types" argument determines which devices are listed, for example
// `PAPPL_DEVTYPE_ALL` will list all types of devices while `PAPPL_DEVTYPE_USB` only
// lists USB printers.
//
// Any errors are reported using the supplied "err_cb" function.  If you specify
// `NULL` for this argument, errors are sent to `stderr`.
//
// > Note: This function will block (not return) until each of the device URI
// > schemes has reported all of the devices *or* the supplied callback function
// > returns `true`.
//

bool					// O - `true` if the callback returned `true`, `false` otherwise
papplDeviceList(
    pappl_devtype_t       types,		// I - Device types
    pappl_device_cb_t   cb,		// I - Callback function
    void                *data,		// I - User data for callback
    pappl_deverror_cb_t err_cb,		// I - Error callback or `NULL` for default
    void                *err_data)	// I - Data for error callback
{
  bool			ret = false;	// Return value
  _pappl_devscheme_t	*ds;		// Current device scheme


  pthread_rwlock_rdlock(&device_rwlock);

  if (!device_schemes)
  {
    pthread_rwlock_unlock(&device_rwlock);
    pthread_rwlock_wrlock(&device_rwlock);

    if (!device_schemes)
      pappl_create_schemes_no_lock();
  }

  if (!err_cb)
    err_cb = pappl_default_error_cb;

  for (ds = (_pappl_devscheme_t *)cupsArrayGetFirst(device_schemes); ds && !ret; ds = (_pappl_devscheme_t *)cupsArrayGetNext(device_schemes))
  {
    if ((types & ds->dtype) && ds->list_cb)
      ret = (ds->list_cb)(cb, data, err_cb, err_data);
  }

  pthread_rwlock_unlock(&device_rwlock);

  return (ret);
}


//
// 'papplDeviceOpen()' - Open a connection to a device.
//
// This function opens a connection to the specified device URI.  The "name"
// argument provides textual context for the connection and is usually the name
// (title) of the print job.
//
// Any errors are reported using the supplied "err_cb" function.  If you specify
// `NULL` for this argument, errors are sent to `stderr`.
//

pappl_device_t	*			// O - Device connection or `NULL` on error
papplDeviceOpen(
    const char          *device_uri,	// I - Device URI
    const char          *name,		// I - Job name
    pappl_deverror_cb_t err_cb,		// I - Error callback or `NULL` for default
    void                *err_data)	// I - Data for error callback
{
  _pappl_devscheme_t	*ds,		// Scheme
			dkey;		// Search key
  pappl_device_t	*device;	// Device structure
  char			scheme[32],	// URI scheme
			userpass[32],	// Username/password (not used)
			host[256],	// Host name or make
			resource[256],	// Resource path, if any
			*options;	// Pointer to options, if any
  int			port;		// Port number
  http_uri_status_t	status;		// URI status


  if (!device_uri)
  {
    _papplDeviceError(err_cb, err_data, "Bad NULL device URI.");
    return (NULL);
  }

  if ((status = httpSeparateURI(HTTP_URI_CODING_ALL, device_uri, scheme, sizeof(scheme), userpass, sizeof(userpass), host, sizeof(host), &port, resource, sizeof(resource))) < HTTP_URI_STATUS_OK)
  {
    _papplDeviceError(err_cb, err_data, "Bad device URI '%s': %s", device_uri, httpURIStatusString(status));
    return (NULL);
  }

  if ((options = strchr(resource, '?')) != NULL)
    *options++ = '\0';

  pthread_rwlock_rdlock(&device_rwlock);

  if (!device_schemes)
  {
    pthread_rwlock_unlock(&device_rwlock);
    pthread_rwlock_wrlock(&device_rwlock);

    if (!device_schemes)
      pappl_create_schemes_no_lock();
  }

  dkey.scheme = scheme;
  ds = (_pappl_devscheme_t *)cupsArrayFind(device_schemes, &dkey);

  pthread_rwlock_unlock(&device_rwlock);

  if (!ds)
  {
    _papplDeviceError(err_cb, err_data, "Unsupported device URI scheme '%s'.", scheme);
    return (NULL);
  }

  if ((device = calloc(1, sizeof(pappl_device_t))) == NULL)
  {
    _papplDeviceError(err_cb, err_data, "Unable to allocate memory for device: %s", strerror(errno));
    return (NULL);
  }

  device->close_cb     = ds->close_cb;
  device->error_cb     = err_cb ? err_cb : pappl_default_error_cb;
  device->error_data   = err_data;
  device->id_cb        = ds->id_cb;
  device->read_cb      = ds->read_cb;
  device->status_cb    = ds->status_cb;
  device->supplies_cb  = ds->supplies_cb;
  device->write_cb     = ds->write_cb;

  if (!(ds->open_cb)(device, device_uri, name))
  {
    free(device);
    return (NULL);
  }

  return (device);
}


//
// 'papplDeviceParseID()' - Parse an IEEE-1284 device ID string.
//
// This function parses an IEEE-1284 device ID string and returns an array of
// key/value pairs as a `cups_option_t` array.  The returned array must be
// freed using the `cupsFreeOptions` function.
//

int					// O - Number of key/value pairs
papplDeviceParseID(
    const char    *device_id,		// I - IEEE-1284 device ID string
    cups_option_t **pairs)		// O - Key/value pairs
{
  cups_len_t	num_pairs = 0;		// Number of key/value pairs
  char		name[256],		// Key name
		value[256],		// Value
		*ptr;			// Pointer into key/value


  // Range check input...
  if (pairs)
    *pairs = NULL;

  if (!device_id || !pairs)
    return (0);

  // Scan the IEEE-1284 device ID string...
  while (*device_id)
  {
    // Skip leading whitespace...
    while (*device_id && isspace(*device_id))
      device_id ++;

    if (!*device_id)
      break;

    // Get the key name...
    for (ptr = name; *device_id && *device_id != ':'; device_id ++)
    {
      if (ptr < (name + sizeof(name) - 1))
	*ptr++ = *device_id;
    }

    *ptr = '\0';

    if (*device_id != ':')
      break;

    device_id ++;

    // Skip leading whitespace in value...
    while (*device_id && isspace(*device_id))
      device_id ++;

    for (ptr = value; *device_id && *device_id != ';'; device_id ++)
    {
      if (ptr < (value + sizeof(value) - 1))
	*ptr++ = *device_id;
    }

    *ptr = '\0';

    if (*device_id == ';')
      device_id ++;

    num_pairs = cupsAddOption(name, value, num_pairs, pairs);
  }

  return ((int)num_pairs);
}


//
// 'papplDevicePrintf()' - Write a formatted string.
//
// This function buffers a formatted string that will be sent to the device.
// The "format" argument accepts all `printf` format specifiers and behaves
// identically to that function.
//
// Call the @link papplDeviceFlush@ function to ensure that the formatted string
// is immediately sent to the device.
//

ssize_t					// O - Number of characters or -1 on error
papplDevicePrintf(
    pappl_device_t *device,		// I - Device
    const char     *format,		// I - Printf-style format string
    ...)				// I - Additional args as needed
{
  va_list	ap;			// Pointer to additional args
  char		buffer[8192];		// Output buffer
  int		bytes;			// Bytes to write


  va_start(ap, format);
  bytes = vsnprintf(buffer, sizeof(buffer), format, ap);
  va_end(ap);

  if (bytes > 0)
    return (papplDeviceWrite(device, buffer, (size_t)bytes));
  else
    return ((ssize_t)bytes);
}


//
// 'papplDevicePuts()' - Write a literal string.
//
// This function buffers a literal string that will be sent to the device.
// Call the @link papplDeviceFlush@ function to ensure that the literal string
// is immediately sent to the device.
//

ssize_t					// O - Number of characters or -1 on error
papplDevicePuts(
    pappl_device_t *device,		// I - Device
    const char     *s)			// I - Literal string
{
  return (papplDeviceWrite(device, s, strlen(s)));
}


//
// 'papplDeviceRead()' - Read from a device.
//
// This function reads data from the device.  Depending on the device, this
// function may block indefinitely.
//

ssize_t					// O - Number of bytes read or -1 on error
papplDeviceRead(
    pappl_device_t *device,		// I - Device
    void           *buffer,		// I - Read buffer
    size_t         bytes)		// I - Max bytes to read
{
  struct timeval	starttime,	// Start time
			endtime;	// End time
  ssize_t		count;		// Bytes read this time


  if (!device || !device->read_cb)
    return (-1);

  // Make sure any pending IO is flushed...
  if (device->bufused > 0)
    papplDeviceFlush(device);

  gettimeofday(&starttime, NULL);

  count = (device->read_cb)(device, buffer, bytes);

  gettimeofday(&endtime, NULL);

  device->metrics.read_requests ++;
  device->metrics.read_msecs += (size_t)(1000 * (endtime.tv_sec - starttime.tv_sec) + (endtime.tv_usec - starttime.tv_usec) / 1000);
  if (count > 0)
    device->metrics.read_bytes += (size_t)count;

  return (count);
}


//
// 'papplDeviceRemoveScheme()' - Remove the named device URI scheme.
//
// This function removes support for the named device URI scheme.  Use only
// when you want to disable a URI scheme for security or functional reasons,
// for example to disable the "file" URI scheme.
//

void
papplDeviceRemoveScheme(
    const char *scheme)			// I - Device URI scheme to remove
{
  _pappl_devscheme_t	*ds,		// Device URI scheme data
			dkey;		// Search key


  pthread_rwlock_wrlock(&device_rwlock);

  // Create the schemes array as needed...
  if (!device_schemes)
    pappl_create_schemes_no_lock();

  // See if the scheme is added...
  dkey.scheme = (char *)scheme;

  if ((ds = (_pappl_devscheme_t *)cupsArrayFind(device_schemes, &dkey)) != NULL)
  {
    // Found it, now remove and free it...
    cupsArrayRemove(device_schemes, ds);

    free(ds->scheme);
    free(ds);
  }

  pthread_rwlock_unlock(&device_rwlock);
}


//
// 'papplDeviceRemoveTypes()' - Remove device URI schemes of the specified types.
//
// This function removes device URI schemes of the specified types.  Use only
// when you want to disable URI schemes for security or functional reasons,
// for example to disable all network URI schemes.
//

void
papplDeviceRemoveTypes(
    pappl_devtype_t types)		// I - Device types to remove
{
  _pappl_devscheme_t	*ds;		// Device URI scheme data


  pthread_rwlock_wrlock(&device_rwlock);

  // Create the schemes array as needed...
  if (!device_schemes)
    pappl_create_schemes_no_lock();

  // Find schemes that match the types...
  for (ds = (_pappl_devscheme_t *)cupsArrayGetFirst(device_schemes); ds; ds = (_pappl_devscheme_t *)cupsArrayGetNext(device_schemes))
  {
    if (ds->dtype & types)
    {
      // Matching type, remove and free it...
      cupsArrayRemove(device_schemes, ds);

      free(ds->scheme);
      free(ds);
    }
  }

  pthread_rwlock_unlock(&device_rwlock);
}


//
// 'papplDeviceSetData()' - Set device-specific data.
//
// This function sets any device-specific data needed to communicate with the
// device.  It is normally only called from the open callback that was
// registered for the device URI scheme.
//

void
papplDeviceSetData(
    pappl_device_t *device,		// I - Device
    void           *data)		// I - Device data pointer
{
  if (device)
    device->device_data = data;
}


//
// 'papplDeviceWrite()' - Write to a device.
//
// This function buffers data that will be sent to the device.  Call the
// @link papplDeviceFlush@ function to ensure that the data is immediately sent
// to the device.
//

ssize_t					// O - Number of bytes written or -1 on error
papplDeviceWrite(
    pappl_device_t *device,		// I - Device
    const void     *buffer,		// I - Write buffer
    size_t         bytes)		// I - Number of bytes to write
{
  if (!device)
    return (-1);

  if ((device->bufused + bytes) > sizeof(device->buffer))
  {
    // Flush the write buffer...
    if (pappl_write(device, device->buffer, device->bufused) < 0)
      return (-1);

    device->bufused = 0;
  }

  if (bytes < sizeof(device->buffer))
  {
    memcpy(device->buffer + device->bufused, buffer, bytes);
    device->bufused += bytes;
    return ((ssize_t)bytes);
  }

  return (pappl_write(device, buffer, bytes));
}


//
// 'pappl_compare_schemes()' - Compare two device URI schemes.
//

static int				// O - Result of comparison
pappl_compare_schemes(
    _pappl_devscheme_t *a,		// I - First URI scheme
    _pappl_devscheme_t *b)		// I - Second URI scheme
{
  return (strcmp(a->scheme, b->scheme));
}


//
// 'pappl_create_schemes_no_lock()' - Create the default device URI schemes.
//

static void
pappl_create_schemes_no_lock(void)
{
  _papplDeviceAddFileSchemeNoLock();
  _papplDeviceAddNetworkSchemesNoLock();
  _papplDeviceAddUSBSchemeNoLock();
}


//
// 'pappl_default_error_cb()' - Send device errors to stderr.
//

static void
pappl_default_error_cb(
    const char *message,		// I - Error message
    void       *data)			// I - Callback data (unused)
{
  (void)data;

  fprintf(stderr, "%s\n", message);
}


//
// 'pappl_free_dinfo()' - Free device information.
//

static void
pappl_free_dinfo(_pappl_dinfo_t *d)	// I - Device info
{
  free(d->device_info);
  free(d->device_uri);
  free(d->device_id);
  free(d);
}


//
// 'pappl_write()' - Write data to the device.
//

static ssize_t				// O - Number of bytes written or `-1` on error
pappl_write(pappl_device_t *device,	// I - Device
            const void     *buffer,	// I - Buffer
            size_t         bytes)	// I - Bytes to write
{
  struct timeval	starttime,	// Start time
			endtime;	// End time
  ssize_t		count;		// Total bytes written


  gettimeofday(&starttime, NULL);

  count = (device->write_cb)(device, buffer, bytes);

  gettimeofday(&endtime, NULL);

  device->metrics.write_requests ++;
  device->metrics.write_msecs += (size_t)(1000 * (endtime.tv_sec - starttime.tv_sec) + (endtime.tv_usec - starttime.tv_usec) / 1000);
  if (count > 0)
    device->metrics.write_bytes += (size_t)count;

  return (count);
}
