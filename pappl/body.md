Hello, World!
=============

This chapter will guide you through creating a simple PCL printer application
based on the old CUPS "rastertohp" filter.  The complete code can be found in
the [hp-printer-app](https://github.com/michaelrsweet/hp-printer-app) project.


The System
==========

The system is an object that manages printers, resources, listeners, and client
connections.


Creating a System
-----------------


Adding Listeners
----------------


Loading and Saving State
------------------------


IPP Server
----------

PAPPL fully implements the [IPP Everywhere™ specification][8] and passes the
[IPP Everywhere™ Printer Self-Certification Manual][9] tests.  PAPPL also
implements several IPP extensions used for IPP-based licensing programs to
simplify certification, including the CUPS "marker-xxx" attributes, the
Get-Printer-Attributes operation using the resource path "/", and the
CUPS-Get-Printers operation.

When configured to support multiple printers, PAPPL implements a subset of the
[IPP System Service v1.0 specification][10] to allow creation, deletion, and
enumeration of printers.


DNS-SD Discovery
----------------

PAPPL takes care of registration of DNS-SD (Bonjour) services for each printer,
including the required sub-types, "flagship" LPD registrations, and recommended
naming and renaming behavior.


File Formats
------------

PAPPL supports JPEG, PNG, PWG Raster, and Apple Raster documents in all of the
standard color spaces and bit depths.  JPEG and PNG images are scaled to the
destination media size and print resolution.  PWG Raster and Apple Raster
documents are *not* scaled as they are normally sent at the proper resolution
and size by the print client.

PAPPL also allows drivers to advertise support for other "raw" formats that are
directly supported by the printer.  And applications can register filters for
other formats.


Driver Interface
----------------

PAPPL provides a simple driver interface for raster printing, and developers of
CUPS Raster drivers will readily adapt to it.  Drivers provide configuration
and capability information to PAPPL, and PAPPL then calls the driver to start
a job, start a page, output lines of graphics, end a page, and finally end a
job during the processing of a print job.

The driver interface supports 1-bit grayscale (clustered- or dispersed-dot)
and 1-bit bi-level (threshold) dithering using a 16x16 matrix, which is
sufficient to support most B&W printing needs.  Continuous tone printing is
supported using 8-bit and 16-bit per component sGray, sRGB, AdobeRGB, or
DeviceN (K, RGB, CMYK, etc.) raster data.

Drivers can also specify "raw" formats that the printer accepts directly - this
is most useful for printers that support industry standard formats such as FGL,
PCL, or ZPL which are produced directly by common shipping and billing
automation applications.  "Raw" files are submitted to the driver using a
separate "print file" interface, allowing the driver to add any printer-specific
commands that are needed to successfully print them.

Aside from printing functionality, drivers can also provide up-to-date status
and configuration information by querying the printer when requested by the
embedded server.  This is an improvement over the CUPS command file interface
and allows a PAPPL-based driver to provide details such as updated media
information.

Drivers can also support printer identification, usually a sound or a light on
the printer, which is a requirement for IPP Everywhere™ and is used to visually
or audibly isolate a particular printer for the user.


Embedded Web Interface
----------------------

The embedded server can also provide a web interface to the Printer Application,
and PAPPL includes a standard web interface that can be customized and/or
overridden.  Aside from the usual status monitoring functionality, the web
interface can be configured to allow remote users (with proper authentication)
to:

- Create and delete printers,
- Set the printer location and DNS-SD name,
- Configure the loaded media,
- Configure remote access accounts,
- Configure networking settings such as the hostname, and/or
- Update the TLS certificates used by the server.

You can also add custom pages and content using callbacks, static data, or
external files or directories.

> Note: An embedded web interface is required for IPP Everywhere™ conformance.
> The optional features allow a Printer Application to easily support the
> functionality required for other IPP-based licensing programs.

Devices
=======

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

Printers
========

Printers are managed by the system and are represented by the
[`pappl_printer_t`](#pappl_printer_t) type.  Each printer is connected to a
device and uses a driver to process document data and produce output.  PAPPL
supports raster printers out-of-the-box and provides filter callbacks to support
other kinds of printers.


Creating a Printer
------------------


Writing Printer Drivers
-----------------------


File Filters
------------



Jobs
====


Resources
=========


The Main Loop
=============

