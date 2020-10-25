Introduction
============

PAPPL is a simple C-based framework/library for developing CUPS Printer
Applications, which are the recommended replacement for printer drivers.  It is
sufficiently general purpose to support any kind of printer or driver that can
be used on desktops, servers, and in embedded environments.

PAPPL embeds a multi-threaded HTTP /
[IPP Everywhere™](https://www.pwg.org/ipp/everywhere.html)
server and provides callbacks for a variety of events that allows a GUI or
command-line application to interact with both the local user that is running
the Printer Application and any network clients that are submitting print jobs,
querying printer status and capabilities, and so forth.

PAPPL provides a simple driver interface for raster graphics printing, and
developers of CUPS Raster drivers will readily adapt to it.  PAPPL can also be
used with printers that support vector graphics printing although you'll have
to develop more code to support them.  Drivers provide configuration and
capability information to PAPPL, and PAPPL then calls the driver to print things
as needed.  PAPPL automatically supports printing of JPEG, PNG, PWG Raster,
Apple Raster, and "raw" files to printers connected via USB and network
(AppSocket/JetDirect) connections.  Other formats can be supported through
"filter" callbacks you register.

PAPPL is Copyright © 2019-2020 by Michael R Sweet and is licensed under the
Apache License Version 2.0 with an (optional) exception to allow linking against
GPL2/LGPL2 software (like older versions of CUPS), so it can be used *freely* in
any project you'd like.  See the files "LICENSE" and "NOTICE" in the source
distribution for more information.


Requirements
------------

PAPPL requires a POSIX-compliant host operating system such as Linux®, macOS®,
QNX®, or VxWorks®, a C99 compiler like Clang or GCC, a `make` program that
supports the `include` directive, and the following support libraries:

- CUPS 2.2 or later for the CUPS libraries (libcups2/libcupsimage2)
- GNU TLS 2.8 or later (except on macOS) for TLS support
- JPEGLIB 9 or later for JPEG image support
- LIBPNG 1.6 or later for PNG image support
- PAM for authentication support
- ZLIB 1.1 or later for compression support

Run the following commands to install the prerequisites on CentOS 7, Fedora 22,
and Red Hat Enterprise Linux 7:

    sudo yum groupinstall 'Development Tools'
    sudo yum install avahi-devel cups-devel gnutls-devel libjpeg-turbo-devel \
        libpam-devel libpng-devel libusbx-devel pam-devel zlib-devel

Run the following commands to install the prerequisites on CentOS 8, Fedora 23
or later, and Red Hat Enterprise Linux 8:

    sudo dnf groupinstall 'Development Tools'
    sudo dnf install avahi-devel cups-devel gnutls-devel libjpeg-turbo-devel \
        libpam-devel libpng-devel libusbx-devel pam-devel zlib-devel

Run the following commands to install the prerequisites on Debian GNU/Linux,
Raspbian, and Ubuntu:

    sudo apt-get install build-essential libavahi-client-dev libcups2-dev \
        libcupsimage2-dev libgnutls28-dev libjpeg-dev libpam-dev libpng-dev \
        libusb-1.0-0-dev zlib1g-dev

Finally, after installing Xcode from the AppStore run the following commands to
install the prerequisites on macOS:

    (install brew if necessary)
    brew install libjpeg
    brew install libpng
    brew install libusb

or download, build, and install libjpeg, libpng, and libusb from source.


Building PAPPL
--------------

PAPPL uses the usual `configure` script to generate a `make` file:

    ./configure [options]
    make

Use `./configure --help` to see a full list of options.

There is also an Xcode project under the `xcode` directory that can be used on
macOS:

    open xcode/pappl.xcodeproj

You can test the build by running the PAPPL test program:

    testsuite/testpappl


Installing PAPPL
----------------

Once you have successfully built PAPPL, install it using:

    sudo make install

By default everything will be installed under `/usr/local`.  Use the `--prefix`
configure option to override the base installation directory.  Set the
`DESTDIR`, `DSTROOT`, or `RPM_BUILD_ROOT` environment variables to redirect the
installation to a staging area, as is typically done for most software packaging
systems (using one of those environment variables...)

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

