PAPPL provides a simple device API for reading data from and writing data to a
printer, as well as get interface-specific status information.  PAPPL devices
are referenced using Universal Resource Identifiers (URIs), with the URI scheme
referring to the device interface, for example:

- "dnssd": Network (AppSocket) printers discovered via DNS-SD/mDNS (Bonjour),
- "file": Local files and directories,
- "snmp": Network (AppSocket) printers discovered via SNMPv1,
- "socket": Network (AppSocket) printers using a numeric IP address or hostname
  and optional port number, and
- "usb": Local USB printer.

PAPPL supports custom device URI schemes which are registered using the
[`papplDeviceAddScheme'](#papplDeviceAddScheme) function.

Devices are accessed via a pointer to the [`pappl_device_t`](#pappl_device_t)
structure.  Print drivers use either the current job's device pointer or call
[`papplPrinterOpenDevice`](#papplPrinterOpenDevice) to open a printer's device
and [`papplPrinterCloseDevice`](#papplPrinterCloseDevice) to close it.

The [`papplDeviceRead`](#papplDeviceRead) function reads data from a device.
Typically a driver only calls this function after sending a command to the
printer requesting some sort of information.  Since not all printers or
interfaces support reading, a print driver *must* be prepared for a read to
fail.

The [`papplDevicePrintf`](#papplDevicePrintf),
[`papplDevicePuts`](#papplDevicePuts), and
[`papplDeviceWrite`](#papplDeviceWrite) functions write data to a device.  The
first two send strings to the device while the last writes arbitrary data.

The [`papplDeviceGetStatus`](#papplDeviceGetStatus) function returns device-
specific state information as a [`pappl_preason_t`](#pappl_preason_t) bitfield.
