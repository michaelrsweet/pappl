//
// Bluetooth utility functions
//
// Copyright © 2026 by Michael R Sweet.
// Copyright © 2026 John Beard.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#ifndef _PAPPL_BLUETOOTH_PRIVATE_H_
#  define _PAPPL_BLUETOOTH_PRIVATE_H_

#  include "base.h"


//
// Constants...
//

// Bluetooth MAC: 6 octets (IEEE 802-2014) = 12 hex chars.
// Colon-separated: "XX:XX:XX:XX:XX:XX" = 17 chars + NUL.
#define _PAPPL_BT_ADDR_SIZE		18

// Bare hex: "XXXXXXXXXXXX" = 12 chars + NUL.
#define _PAPPL_BT_ADDR_BARE_SIZE	13

// BlueZ device name.  The Bluetooth Core Specification v5.4, Vol 3,
// Part C, Section 8.1.2 limits the device name to 248 octets (UTF-8).
// Allow 248 chars + NUL, rounded up.
#define _PAPPL_BT_MAX_NAME		256

// IEEE-1284 device ID for a Bluetooth printer.
// Format: "NAME:<name>;BTADDR:<addr>;"
// Overhead = 15, addr = 17, separators = 2 + name max = 248 (BT spec).
// Total = 281.  Round up for safety.
#define _PAPPL_BT_MAX_DEVICE_ID		300


//
// Types...
//

typedef struct _pappl_bt_dev_s		// Bluetooth device data
{
  int		sock;			// Socket fd, or -1 if closed
  char		address[_PAPPL_BT_ADDR_SIZE];  // Normalized Bluetooth MAC
} _pappl_bt_dev_t;

typedef bool (*_pappl_bt_enum_cb_t)(	// Device enumeration callback
    const char *name,			// I - Device name from BlueZ cache
    const char *addr,			// I - Normalized Bluetooth MAC
    void       *data);			// I - Callback context


//
// Functions (common Bluetooth infrastructure)...
//

extern bool _papplBluetoothNormalizeAddress(const char *input, char *output, size_t output_size) _PAPPL_PRIVATE;
extern bool _papplBluetoothAddrToBare(const char *addr, char *buffer, size_t bufsize) _PAPPL_PRIVATE;
extern bool _papplBluetoothMakeDeviceId(const char *name, const char *addr, char *buffer, size_t bufsize) _PAPPL_PRIVATE;


//
// Functions (DBus/BlueZ device helpers)...
//

extern void _papplBluetoothLookupName(const char *normalized_addr, char *name_buf, size_t name_size) _PAPPL_PRIVATE;
extern bool _papplBluetoothEnumerateDevices(_pappl_bt_enum_cb_t cb, void *data) _PAPPL_PRIVATE;


//
// Functions (Bluetooth SPP transport)...
//

extern bool _papplBluetoothSppParseURI(const char *uri, char *address, size_t address_size, int *channel) _PAPPL_PRIVATE;
extern unsigned int _papplBluetoothSppRetryDelay(int error) _PAPPL_PRIVATE;


//
// Profile registration functions...
//

extern void _papplDeviceAddBTSppSchemeNoLock(void) _PAPPL_PRIVATE;

#endif // _PAPPL_BLUETOOTH_PRIVATE_H_