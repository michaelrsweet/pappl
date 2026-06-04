//
// Bluetooth SPP device support for the Printer Application Framework
//
// Implements a "bt-spp://" device URI scheme for printers connected via
// Bluetooth Classic Serial Port Profile (SPP / RFCOMM).
//
// URI format:
//   bt-spp://<BT_ADDR>[/<channel>]
//
//   BT_ADDR:  MAC address with optional separators (colons, hyphens, dots,
//             or bare hex), e.g. "0608BCA54BB5", "06:08:BC:A5:4B:B5"
//   channel:  Optional RFCOMM channel number (default: 1)
//
// It reports the BlueZ device name and Bluetooth address in the
// IEEE-1284 device ID so that applications can match it.
// TODO: Unsure about the matching logic.
//
// Copyright 2026 John Beard
// Copyright 2019-2026 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#include "bt-private.h"
#include "device-private.h"
#include "printer.h"

#include <stdio.h>
#include <string.h>


//
// '_papplBluetoothSppParseURI()' - Parse a bt-spp:// URI.
//
// Extracts the Bluetooth MAC address and optional RFCOMM channel number.
//
// Returns `true` on success, `false` if the URI is malformed.
//

bool
_papplBluetoothSppParseURI(
    const char *uri,			// I - URI string
    char       *addr,			// O - Address buffer
    size_t     addr_size,		// I - Size of address buffer
    int        *channel)		// O - RFCOMM channel number
{
  const char	*rest;			// Pointer after "bt-spp://"
  const char	*slash;			// Pointer to optional '/'


  // Must start with "bt-spp://"...
  if (strncmp(uri, "bt-spp://", 9) != 0)
    return (false);

  rest = uri + 9;

  // Must have a non-empty address part...
  if (*rest == '\0')
    return (false);

  // Find the optional channel separator...
  slash = strchr(rest, '/');

  // Extract and normalize the address portion...
  {
    size_t	len;			// Length of address portion
    char	buf[32];		// Raw address portion


    len = (slash != NULL) ? (size_t)(slash - rest) : strlen(rest);

    if (len >= sizeof(buf))
      return (false);

    memcpy(buf, rest, len);
    buf[len] = '\0';

    if (!_papplBluetoothNormalizeAddress(buf, addr, addr_size))
      return (false);
  }

  // Parse the optional channel number...
  if (slash != NULL)
  {
    char	*end;			// Pointer to first invalid character
    long	ch;			// Raw channel number


    ch = strtol(slash + 1, &end, 10);

    if (*end != '\0' || ch < 1 || ch > 30)
      return (false);

    *channel = (int)ch;
  }
  else
  {
    *channel = 1;
  }

  return (true);
}


#if defined(HAVE_LIBBLUETOOTH) && defined(HAVE_DBUS)

//
// Local constants...
//

// Maximum bt-spp:// URI length.
// Scheme "bt-spp://"(9) + bare-hex addr(12) + "/" + channel(2) = 23.
// Allocate 64 for safety margin.
#define _PAPPL_BT_SPP_MAX_URI		64


//
// Local types...
//

typedef struct _pappl_bt_spp_enum_s	// SPP enumeration context
{
  pappl_device_cb_t   cb;		// Device callback
  void                *data;		// Callback data
} _pappl_bt_spp_enum_t;


//
// Local functions...
//

static char		*pappl_bt_spp_getid(pappl_device_t *device, char *buffer, size_t bufsize);
static bool		pappl_bt_spp_list(pappl_devtype_t types, pappl_device_cb_t cb, void *data, pappl_deverror_cb_t err_cb, void *err_data);
static bool		pappl_bt_spp_open(pappl_device_t *device, const char *device_uri, pappl_job_t *job);

static bool		pappl_bt_spp_enum_cb(const char *name, const char *addr, void *data);

static int              pappl_bt_spp_rfcomm_connect(const char *addr, int channel);
static void             pappl_bt_spp_rfcomm_close(pappl_device_t *device);
static ssize_t          pappl_bt_spp_rfcomm_read(pappl_device_t *device, void *buffer, size_t bytes);
static ssize_t          pappl_bt_spp_rfcomm_write(pappl_device_t *device, const void *buffer, size_t bytes);
static pappl_preason_t  pappl_bt_spp_status(pappl_device_t *device);


//
// 'pappl_bt_spp_enum_cb()' - Build SPP URI and device ID for enumeration.
//

static bool				// O - `true` to stop enumeration
pappl_bt_spp_enum_cb(
    const char *name,			// I - Device name
    const char *addr,			// I - Normalized MAC ("XX:XX:XX:XX:XX:XX")
    void       *data)			// I - Enumeration context
{
  _pappl_bt_spp_enum_t	*ctx = (_pappl_bt_spp_enum_t *)data;
  char			device_uri[_PAPPL_BT_SPP_MAX_URI];
  char			device_id[_PAPPL_BT_MAX_DEVICE_ID];
  char			bare_hex[_PAPPL_BT_ADDR_BARE_SIZE];


  // Convert the normalized colon-separated MAC to bare hex for the URI.
  // Bare hex avoids ambiguity with the standard URI host:port parser
  // used by papplDeviceOpen.
  if (!_papplBluetoothAddrToBare(addr, bare_hex, sizeof(bare_hex)))
    return (false);

  // Build the bt-spp:// URI...
  snprintf(device_uri, sizeof(device_uri), "bt-spp://%s", bare_hex);

  // Build the IEEE-1284 device ID...
  if (!_papplBluetoothMakeDeviceId(name, addr, device_id, sizeof(device_id)))
    return (false);

  return ((*ctx->cb)(name, device_uri, device_id, ctx->data));
}


//
// 'pappl_bt_spp_list()' - List available Bluetooth SPP printers.
//

static bool				// O - `true` on stop, `false` otherwise
pappl_bt_spp_list(
    pappl_devtype_t     types,		// I - Device types to list
    pappl_device_cb_t   cb,		// I - Device callback
    void                *data,		// I - Callback data
    pappl_deverror_cb_t err_cb,		// I - Error callback (unused)
    void                *err_data)	// I - Error callback data (unused)
{
  (void)err_cb;
  (void)err_data;

  // Only list when CUSTOM_NETWORK type is requested...
  if ((types & PAPPL_DEVTYPE_CUSTOM_NETWORK) == 0)
    return (true);

  {
    _pappl_bt_spp_enum_t	ctx;


    ctx.cb   = cb;
    ctx.data = data;

    return (_papplBluetoothEnumerateDevices(pappl_bt_spp_enum_cb, &ctx));
  }
}


//
// 'pappl_bt_spp_open()' - Open a Bluetooth SPP (RFCOMM) connection.
//

static bool				// O - `true` on success, `false` otherwise
pappl_bt_spp_open(
    pappl_device_t *device,		// I - Device
    const char     *device_uri,		// I - Device URI
    pappl_job_t    *job)		// I - Job (unused)
{
  (void)job;

  char	addr[_PAPPL_BT_ADDR_SIZE];	// Normalized Bluetooth MAC
  int	channel;			// RFCOMM channel number
  int	sock;				// RFCOMM socket fd


  // Parse the bt-spp:// URI...
  if (!_papplBluetoothSppParseURI(device_uri, addr, sizeof(addr), &channel))
  {
    papplDeviceError(device, "Invalid bt-spp:// URI: %s", device_uri);
    return (false);
  }

  // Connect via RFCOMM...
  sock = pappl_bt_spp_rfcomm_connect(addr, channel);

  if (sock < 0)
  {
    papplDeviceError(device,
                     "Unable to connect to %s channel %d: %s",
                     addr, channel, strerror(errno));
    return (false);
  }

  // Allocate and store device data...
  {
    _pappl_bt_dev_t *bt;		// Bluetooth device data


    bt = (_pappl_bt_dev_t *)calloc(1, sizeof(_pappl_bt_dev_t));
    if (bt == NULL)
    {
      papplDeviceError(device, "Out of memory.");
      close(sock);
      return (false);
    }

    bt->sock = sock;
    cupsCopyString(bt->address, addr, sizeof(bt->address));

    papplDeviceSetData(device, bt);
  }

  return (true);
}


//
// 'pappl_bt_spp_getid()' - Get the IEEE-1284 device ID for a Bluetooth SPP printer.
//

static char *				// O - Device ID string
pappl_bt_spp_getid(
    pappl_device_t *device,		// I - Device
    char           *buffer,		// I - Device ID buffer
    size_t         bufsize)		// I - Size of buffer
{
  _pappl_bt_dev_t	*bt;		// Bluetooth device data
  const char		*addr;		// Normalized MAC address


  bt   = (_pappl_bt_dev_t *)papplDeviceGetData(device);
  addr = (bt != NULL) ? bt->address : "";

  if (bt == NULL || bt->address[0] == '\0')
  {
    buffer[0] = '\0';
    return (buffer);
  }

  {
    char	name[_PAPPL_BT_MAX_NAME];


    _papplBluetoothLookupName(addr, name, sizeof(name));

    if (!_papplBluetoothMakeDeviceId(name, addr, buffer, bufsize))
    {
      buffer[0] = '\0';
      return (NULL);
    }
  }

  return (buffer);
}


//
// ---------------------------------------------------------------------------
// RFCOMM transport helpers
// ---------------------------------------------------------------------------

#  ifdef __linux__
#    include <bluetooth/bluetooth.h>
#    include <bluetooth/rfcomm.h>
#    include <fcntl.h>
#    include <poll.h>
#  endif // __linux__


//
// 'pappl_bt_spp_rfcomm_connect()' - Create and connect an RFCOMM socket.
//

static int                              // O - Socket fd or -1 on error
pappl_bt_spp_rfcomm_connect(
    const char *addr,                   // I - Normalized Bluetooth MAC
    int        channel)                 // I - RFCOMM channel number
{
  int sock = -1;                      // RFCOMM socket fd


#  ifdef __linux__
  // Create the RFCOMM socket...
  sock = socket(AF_BLUETOOTH, SOCK_STREAM, BTPROTO_RFCOMM);

  if (sock < 0)
    return (-1);

  // Set non-blocking so we can enforce a connect timeout.  RFCOMM
  // connect() can block for 20+ seconds when the printer is out of
  // range or powered off, which is too long for comfort
  {
    int flags = fcntl(sock, F_GETFL, 0);


    if (flags >= 0)
      fcntl(sock, F_SETFL, flags | O_NONBLOCK);
  }

  // Build the remote address and connect...
  {
    struct sockaddr_rc remote = { 0 };


    remote.rc_family  = AF_BLUETOOTH;
    remote.rc_channel = (uint8_t)channel;
    str2ba(addr, &remote.rc_bdaddr);

    if (connect(sock, (struct sockaddr *)&remote, sizeof(remote)) < 0)
    {
      if (errno == EINPROGRESS)
      {
        // Wait up to 5 seconds for the connection to complete, and
        // retry on EINTR.
        const int     timeout_ms = 5000;
        struct pollfd pfd = { sock, POLLOUT, 0 };
        int           ret;

        do
        {
          ret = poll(&pfd, 1, timeout_ms);
        }
        while (ret < 0 && errno == EINTR);

        if (ret <= 0)
        {
          close(sock);
          if (ret == 0)
            errno = ETIMEDOUT;
          return (-1);
        }

        // Verify the connection actually succeeded...
        {
          int       err = 0;
          socklen_t len = (socklen_t)sizeof(err);


          if (getsockopt(sock, SOL_SOCKET, SO_ERROR, &err, &len) < 0 || err != 0)
          {
            close(sock);
            return (-1);
          }
        }
      }
      else
      {
        close(sock);
        return (-1);
      }
    }

    // Restore blocking mode for normal I/O...
    {
      int flags = fcntl(sock, F_GETFL, 0);


      if (flags >= 0)
        fcntl(sock, F_SETFL, flags & ~O_NONBLOCK);
    }
  }
#  else
  (void)addr;
  (void)channel;
  sock = -1;
  errno = ENOSYS;
#  endif // __linux__

  return (sock);
}


//
// 'pappl_bt_spp_rfcomm_close()' - Close an RFCOMM connection.
//

static void
pappl_bt_spp_rfcomm_close(
    pappl_device_t *device)             // I - Device
{
  _pappl_bt_dev_t *bt;                  // Bluetooth device data


  bt = (_pappl_bt_dev_t *)papplDeviceGetData(device);

  if (bt != NULL)
  {
    if (bt->sock >= 0)
      close(bt->sock);

    free(bt);
    papplDeviceSetData(device, NULL);
  }
}


//
// 'pappl_bt_spp_rfcomm_read()' - Read from an RFCOMM socket.
//

static ssize_t                          // O - Bytes read or -1 on error
pappl_bt_spp_rfcomm_read(
    pappl_device_t *device,             // I - Device
    void           *buffer,             // I - Read buffer
    size_t         bytes)               // I - Bytes to read
{
  _pappl_bt_dev_t *bt;                  // Bluetooth device data


  bt = (_pappl_bt_dev_t *)papplDeviceGetData(device);

  if (bt == NULL || bt->sock < 0)
  {
    errno = EBADF;
    return (-1);
  }

  return (read(bt->sock, buffer, bytes));
}


//
// 'pappl_bt_spp_rfcomm_write()' - Write to an RFCOMM socket.
//

static ssize_t                          // O - Bytes written or -1 on error
pappl_bt_spp_rfcomm_write(
    pappl_device_t *device,             // I - Device
    const void     *buffer,             // I - Write buffer
    size_t         bytes)               // I - Bytes to write
{
  _pappl_bt_dev_t *bt;                  // Bluetooth device data


  bt = (_pappl_bt_dev_t *)papplDeviceGetData(device);

  if (bt == NULL || bt->sock < 0)
  {
    errno = EBADF;
    return (-1);
  }

  return (write(bt->sock, buffer, bytes));
}


//
// 'pappl_bt_spp_status()' - Get printer status.
//
// The SPP transport has no standard status mechanism; report the
// printer as always idle.
//

static pappl_preason_t                  // O - Printer state reason bits
pappl_bt_spp_status(
    pappl_device_t *device)             // I - Device
{
  (void)device;
  return (PAPPL_PREASON_NONE);
}


//
// '_papplDeviceAddBTSppSchemeNoLock()' - Register the bt-spp:// scheme.
//

void
_papplDeviceAddBTSppSchemeNoLock(void)
{
  _papplDeviceAddSchemeNoLock(
      "bt-spp",                         // URI scheme name
      PAPPL_DEVTYPE_CUSTOM_NETWORK,     // Device type
      pappl_bt_spp_list,                // List/discovery callback
      pappl_bt_spp_open,                // Open callback
      pappl_bt_spp_rfcomm_close,        // Close callback
      pappl_bt_spp_rfcomm_read,         // Read callback
      pappl_bt_spp_rfcomm_write,        // Write callback
      pappl_bt_spp_status,              // Status callback
      NULL,                             // Supplies callback (not implemented)
      pappl_bt_spp_getid);              // Device ID callback
}


#endif // HAVE_LIBBLUETOOTH && HAVE_DBUS
