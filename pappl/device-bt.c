//
// Common Bluetooth device support for the Printer Application Framework
//
// Provides shared infrastructure for Bluetooth device URI schemes.
// Profile-specific modules (e.g. device-bt-spp.c) register their
// own scheme callbacks and delegate to the helpers defined here.
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

#include <ctype.h>
#include <stdio.h>


//
// '_papplBluetoothNormalizeAddress()' - Normalize a Bluetooth MAC address.
//
// Accepts bare hex, colon-separated, hyphen-separated, or dot-separated
// formats and normalizes to uppercase colon-separated.
//
// Returns `true` on success, `false` if the input is not a valid
// 6-octet Bluetooth MAC address.
//

bool
_papplBluetoothNormalizeAddress(
    const char *input,            // I - Input address string
    char       *output,            // I - Output buffer
    size_t     output_size)        // I - Size of output buffer
{
  int  hex_len = 0;      // Number of hex digits collected
  char  hex[12];      // Raw hex digits (no separators)


  // Range-check input...
  if (input == NULL)
    return (false);

  // Collect exactly 12 hex digits, skipping any non-hex separators...
  for (; *input != '\0' && hex_len < 12; input++)
  {
    if (isxdigit((unsigned char)*input))
      hex[hex_len++] = (char)toupper((unsigned char)*input);
  }

  // Reject if there are trailing hex digits after the first 12...
  for (; *input != '\0'; input++)
  {
    if (isxdigit((unsigned char)*input))
      return (false);
  }

  // Reject if we did not collect exactly 12 hex digits...
  if (hex_len != 12)
    return (false);

  if (output_size < _PAPPL_BT_ADDR_SIZE)
    return (false);

  // Format as uppercase colon-separated MAC...
  snprintf(output, output_size,
           "%c%c:%c%c:%c%c:%c%c:%c%c:%c%c",
           hex[0], hex[1], hex[2], hex[3], hex[4], hex[5],
           hex[6], hex[7], hex[8], hex[9], hex[10], hex[11]);

  return (true);
}


//
// '_papplBluetoothMakeDeviceId()' - Build an IEEE-1284 device ID string.
//
// Constructs a device ID in the form:
//   "NAME:<name>;BTADDR:<addr>;"
//
// If `name` is empty or NULL the MAC address is used as the name.
// This function is a pure string operation and does not access
// filesystem or Bluetooth hardware.
//

bool          // O - true on success, false on failure (bad address, truncation)
_papplBluetoothMakeDeviceId(
    const char *name,      // I - Friendly device name (may be NULL)
    const char *addr,      // I - Normalized Bluetooth MAC
    char       *buffer,    // I - Output buffer
    size_t     bufsize)    // I - Size of output buffer
{
  const char  *display;    // Name to use for display


  // Use the address as the display name if no friendly name is available...
  if (name == NULL || name[0] == '\0')
    display = addr;
  else
    display = name;

  // Sanity-check the address...
  if (addr == NULL || addr[0] == '\0')
    return (false);

  // Build the device ID string...
  if (snprintf(buffer, bufsize, "NAME:%s;BTADDR:%s;", display, addr) >= (int)bufsize)
    return (false);

  return (true);
}


//
// '_papplBluetoothAddrToBare()' - Convert colon-separated MAC to bare hex.
//
// Strips colons from a normalized (uppercase colon-separated) Bluetooth
// MAC address to produce a bare 12-character hex string.  Used when a
// compact representation is needed, e.g., in bt-spp:// URIs where colons
// would be misinterpreted by the standard URI host:port parser.
//
// The input must already be normalized; this function does not validate.
//
// Returns `true` if the input was successfully converted, `false` if the input
// is NULL or the output buffer is too small (needs at least 13 bytes).
//

bool          // O - Success
_papplBluetoothAddrToBare(
    const char *addr,      // I - Normalized MAC ("XX:XX:XX:XX:XX:XX")
    char       *buffer,    // I - Output buffer
    size_t     bufsize)    // I - Size of output buffer (>= 13)
{
  const char  *src;      // Source pointer
  char    *dst;      // Destination pointer


  if (addr == NULL || bufsize < _PAPPL_BT_ADDR_BARE_SIZE)
    return (false);

  for (src = addr, dst = buffer; *src != '\0'; src++)
  {
    if (*src != ':')
      *dst++ = *src;
  }

  *dst = '\0';

  return (true);
}


#if defined(HAVE_LIBBLUETOOTH) && defined(HAVE_DBUS)

#  include <dbus/dbus.h>


//
// Local types...
//

// Per-device callback for _papplBluetoothForEachDevice.
// addr and name are NUL-terminated strings copied from the D-Bus
// response.  Return true to stop iteration early.
typedef bool (*_pappl_bt_raw_device_cb_t)(const char *addr, const char *name, void *data);


//
// Local functions...
//

static bool    pappl_bt_dbus_foreach_device(DBusMessageIter *obj_iter, char *addr_buf, size_t addr_size, char *name_buf, size_t name_size);
static bool    _papplBluetoothForEachDevice(_pappl_bt_raw_device_cb_t cb, void *data);


//
// 'pappl_bt_dbus_foreach_device()' - Iterate over BlueZ Device1 objects.
//
// Parses one dict entry from a GetManagedObjects response.  If the entry
// represents a `org.bluez.Device1` object, extracts the `Address` and
// `Name` (or `Alias`) properties and copies them into caller-provided
// buffers.
//
// All needed properties are extracted in a single pass over the `a{sv}`
// dict so that property ordering in the D-Bus message does not affect
// correctness.
//
// Returns `false` when there are no more entries in the outer array.
// The caller does NOT need to advance `obj_iter` between calls. It is
// advanced internally.
//

static bool        // O - `true` if a device was found
pappl_bt_dbus_foreach_device(
    DBusMessageIter  *obj_iter,    // I - Outer array iterator
    char             *addr_buf,    // I - Address buffer
    size_t           addr_size,    // I - Size of address buffer
    char             *name_buf,    // I - Name buffer
    size_t           name_size)    // I - Size of name buffer
{
  DBusMessageIter  obj_entry;  // Dict entry: {o a{sa{sv}}}
  DBusMessageIter  iface_array;  // Interfaces array: a{sa{sv}}
  const char    *obj_path;  // Object path (unused)
  int      iface_type;  // Current type in iface array


  *addr_buf = '\0';
  *name_buf = '\0';

  while ((iface_type = dbus_message_iter_get_arg_type(obj_iter)) != DBUS_TYPE_INVALID)
  {
    if (iface_type != DBUS_TYPE_DICT_ENTRY)
    {
      dbus_message_iter_next(obj_iter);
      continue;
    }

    dbus_message_iter_recurse(obj_iter, &obj_entry);

    // First element: object path (o)...
    dbus_message_iter_get_basic(&obj_entry, &obj_path);
    (void)obj_path; // Unused

    dbus_message_iter_next(&obj_entry);

    // Second element: interfaces array a{sa{sv}}...
    dbus_message_iter_recurse(&obj_entry, &iface_array);

    // Scan interfaces for org.bluez.Device1...
    {
      DBusMessageIter  iface_entry;  // Dict entry: {s a{sv}}
      int    iface_field;  // Current type in iface entry


      while ((iface_field = dbus_message_iter_get_arg_type(&iface_array)) != DBUS_TYPE_INVALID)
      {
        if (iface_field != DBUS_TYPE_DICT_ENTRY)
        {
          dbus_message_iter_next(&iface_array);
          continue;
        }

        dbus_message_iter_recurse(&iface_array, &iface_entry);

        // Interface name (s)...
        {
          const char *iface_name;  // Interface name string


          dbus_message_iter_get_basic(&iface_entry, &iface_name);

          if (strcmp(iface_name, "org.bluez.Device1") == 0)
          {
            DBusMessageIter  props_array;  // Properties array: a{sv}
            const char    *found_addr = NULL;
            const char    *found_name = NULL;
            const char    *found_alias = NULL;


            dbus_message_iter_next(&iface_entry);
            dbus_message_iter_recurse(&iface_entry, &props_array);

            // Single pass over all properties.
            // D-Bus dict entries are unordered, so we collect every
            // property we need in one traversal.
            {
              int prop_type;

              while ((prop_type = dbus_message_iter_get_arg_type(&props_array)) != DBUS_TYPE_INVALID)
              {
                if (prop_type == DBUS_TYPE_DICT_ENTRY)
                {
                  DBusMessageIter  prop_entry;
                  DBusMessageIter  variant;
                  const char    *prop_name;


                  dbus_message_iter_recurse(&props_array, &prop_entry);
                  dbus_message_iter_get_basic(&prop_entry, &prop_name);

                  if (strcmp(prop_name, "Address") == 0)
                  {
                    dbus_message_iter_next(&prop_entry);
                    dbus_message_iter_recurse(&prop_entry, &variant);
                    if (dbus_message_iter_get_arg_type(&variant) == DBUS_TYPE_STRING)
                      dbus_message_iter_get_basic(&variant, &found_addr);
                  }
                  else if (strcmp(prop_name, "Name") == 0)
                  {
                    dbus_message_iter_next(&prop_entry);
                    dbus_message_iter_recurse(&prop_entry, &variant);
                    if (dbus_message_iter_get_arg_type(&variant) == DBUS_TYPE_STRING)
                      dbus_message_iter_get_basic(&variant, &found_name);
                  }
                  else if (strcmp(prop_name, "Alias") == 0)
                  {
                    dbus_message_iter_next(&prop_entry);
                    dbus_message_iter_recurse(&prop_entry, &variant);
                    if (dbus_message_iter_get_arg_type(&variant) == DBUS_TYPE_STRING)
                      dbus_message_iter_get_basic(&variant, &found_alias);
                  }
                }

                dbus_message_iter_next(&props_array);
              }
            }

            cupsCopyString(addr_buf, found_addr ? found_addr : "", addr_size);

            // Prefer Name; fall back to Alias...
            if (found_name != NULL && found_name[0] != '\0')
              cupsCopyString(name_buf, found_name, name_size);
            else if (found_alias != NULL && found_alias[0] != '\0')
              cupsCopyString(name_buf, found_alias, name_size);

            dbus_message_iter_next(obj_iter);
            return (true);
          }
        }

        dbus_message_iter_next(&iface_array);
      }
    }

    dbus_message_iter_next(obj_iter);
  }

  return (false);
}


//
// '_papplBluetoothForEachDevice()' - Iterate over paired Bluetooth devices.
//
// Connects to the BlueZ D-Bus API and enumerates Device1 objects.
// For each device found, calls the provided callback with the raw
// address and device name (both NUL-terminated, copied from the D-Bus
// response).
//
// Returns true if the callback requested an early stop, false if all
// devices were enumerated or an error occurred.
//

static bool        // O - true if stopped early
_papplBluetoothForEachDevice(
    _pappl_bt_raw_device_cb_t cb,  // I - Per-device callback
    void                      *data)  // I - Callback context
{
  DBusConnection *conn;             // D-Bus system bus connection
  DBusError      error;             // D-Bus error
  DBusMessage    *request = NULL;   // GetManagedObjects request
  DBusMessage    *response = NULL;  // GetManagedObjects response
  bool           stopped = false;   // Early-stop flag
  const int      timeout_ms = 5000;


  // Connect to the system bus...
  dbus_error_init(&error);

  conn = dbus_bus_get(DBUS_BUS_SYSTEM, &error);
  if (conn == NULL)
  {
    dbus_error_free(&error);
    return (false);
  }

  // Call org.freedesktop.DBus.ObjectManager.GetManagedObjects...
  request = dbus_message_new_method_call(
      "org.bluez",
      "/",
      "org.freedesktop.DBus.ObjectManager",
      "GetManagedObjects");

  if (request == NULL)
  {
    dbus_connection_unref(conn);
    return (false);
  }

  response = dbus_connection_send_with_reply_and_block(
      conn, request, timeout_ms, &error);
  dbus_message_unref(request);

  if (response == NULL)
  {
    dbus_error_free(&error);
    dbus_connection_unref(conn);
    return (false);
  }

  // Parse the response and invoke the callback for each device...
  {
    DBusMessageIter  top_iter;  // Top-level iterator (ARRAY)
    DBusMessageIter  obj_iter;  // Array element iterator


    dbus_message_iter_init(response, &top_iter);

    if (dbus_message_iter_get_arg_type(&top_iter) == DBUS_TYPE_ARRAY)
    {
      dbus_message_iter_recurse(&top_iter, &obj_iter);

      while (true)
      {
        char  dev_addr[_PAPPL_BT_ADDR_SIZE];
        char  dev_name[_PAPPL_BT_MAX_NAME];


        if (!pappl_bt_dbus_foreach_device(&obj_iter,
                                          dev_addr, sizeof(dev_addr),
                                          dev_name, sizeof(dev_name)))
          break;

        if ((*cb)(dev_addr, dev_name, data))
        {
          stopped = true;
          break;
        }
      }
    }
  }

  dbus_message_unref(response);
  dbus_connection_unref(conn);

  return (stopped);
}


//
// '_papplBluetoothLookupName()' - Look up a BlueZ device name via D-Bus.
//
// Queries the BlueZ D-Bus API (org.bluez.Device1) via the system bus to
// look up the friendly name for the given Bluetooth MAC address.
//
// If the system bus is not available or the device is not found, the
// output buffer is set to an empty string.
//

typedef struct _pappl_bt_lookup_ctx_s  // Lookup callback context
{
  const char  *normalized_addr;  // Address to match
  char    *name_buf;    // Name output buffer
  size_t  name_size;    // Size of name buffer
} _pappl_bt_lookup_ctx_t;


// Lookup callback: match address and copy name...
static bool
pappl_bt_lookup_cb(
    const char *addr,
    const char *name,
    void       *data)
{
  _pappl_bt_lookup_ctx_t *ctx = (_pappl_bt_lookup_ctx_t *)data;

  if (addr[0] != '\0' && strcmp(addr, ctx->normalized_addr) == 0)
  {
    if (name[0] != '\0')
      cupsCopyString(ctx->name_buf, name, ctx->name_size);
    return (true);
  }

  return (false);
}


void
_papplBluetoothLookupName(
    const char *normalized_addr,  // I - Normalized Bluetooth MAC
    char       *name_buf,    // I - Name buffer
    size_t     name_size)    // I - Size of name buffer
{
  _pappl_bt_lookup_ctx_t ctx;


  name_buf[0] = '\0';

  ctx.normalized_addr = normalized_addr;
  ctx.name_buf        = name_buf;
  ctx.name_size       = name_size;

  _papplBluetoothForEachDevice(pappl_bt_lookup_cb, &ctx);
}


//
// '_papplBluetoothEnumerateDevices()' - Enumerate paired Bluetooth devices.
//
// Queries the BlueZ D-Bus API (org.freedesktop.DBus.ObjectManager) via
// the system bus to discover paired Bluetooth devices.
//
// For each device found, the supplied callback is invoked with the
// friendly device name and normalized address.  The callback is
// responsible for building any profile-specific URI and device ID.
//
// Returns `true` if enumeration was stopped early (callback returned
// `true`), `false` if all devices were enumerated.
//

typedef struct _pappl_bt_enum_ctx_s  // Enumeration callback context
{
  _pappl_bt_enum_cb_t cb;    // External device callback
  void                *data;    // External callback data
} _pappl_bt_enum_ctx_t;


// Enumeration callback: normalize address, build display name, invoke
// the external callback...
static bool
pappl_bt_enum_cb(
    const char *addr,
    const char *name,
    void       *data)
{
  _pappl_bt_enum_ctx_t  *ctx = (_pappl_bt_enum_ctx_t *)data;
  char      normalized_addr[_PAPPL_BT_ADDR_SIZE];
  char      device_name[_PAPPL_BT_MAX_NAME];


  // Skip devices without a valid address...
  if (addr[0] == '\0')
    return (false);

  // Normalize the address...
  if (!_papplBluetoothNormalizeAddress(addr,
                                       normalized_addr,
                                       sizeof(normalized_addr)))
    return (false);

  // Determine the display name...
  if (name[0] != '\0')
    cupsCopyString(device_name, name, sizeof(device_name));
  else
    cupsCopyString(device_name, normalized_addr, sizeof(device_name));

  // Report to the caller...
  return ((*ctx->cb)(device_name, normalized_addr, ctx->data));
}


bool          // O - `true` if stopped early
_papplBluetoothEnumerateDevices(
    _pappl_bt_enum_cb_t cb,    // I - Callback for each device
    void                *data)    // I - Callback context
{
  _pappl_bt_enum_ctx_t ctx;


  ctx.cb   = cb;
  ctx.data = data;

  return (_papplBluetoothForEachDevice(pappl_bt_enum_cb, &ctx));
}


//
// '_papplDeviceAddBluetoothSchemesNoLock()' - Register all Bluetooth schemes.
//
// Called by the device subsystem during initialisation.  Each Bluetooth
// profile registers its own URI scheme via `_papplDeviceAddSchemeNoLock()`.
//

void
_papplDeviceAddBluetoothSchemesNoLock(void)
{
  _papplDeviceAddBTSppSchemeNoLock();
}


#endif // HAVE_LIBBLUETOOTH && HAVE_DBUS
