PAPPL - Printer Application Framework
=====================================

[![Build Status](https://travis-ci.org/michaelrsweet/pappl.svg?branch=master)](https://travis-ci.org/github/michaelrsweet/pappl)
[![LGTM Grade](https://img.shields.io/lgtm/grade/cpp/github/michaelrsweet/pappl)](https://lgtm.com/projects/g/michaelrsweet/pappl/context:cpp)
[![LGTM Alerts](https://img.shields.io/lgtm/alerts/github/michaelrsweet/pappl)](https://lgtm.com/projects/g/michaelrsweet/pappl/)

PAPPL is a simple C-based framework/library for developing CUPS Printer
Applications, which are the recommended replacement for printer drivers.  I
have specifically developed PAPPL to support [LPrint][1] and a [Gutenprint][2]
Printer Application, but it is sufficiently general purpose to support any kind
of printer or driver that can be used on desktops, servers, and in embedded
environments.

PAPPL supports JPEG, PNG, PWG Raster, Apple Raster, and "raw" printing to
printers connected via USB and network (AppSocket/JetDirect) connections.
PAPPL provides access to the printer via its embedded [IPP Everywhere™][3]
service, either local to the computer or on your whole network, which can then
be discovered and used by any application.

The OpenPrinting group has [written a tutorial][12] showing how to migrate the
`rastertohp` driver from CUPS to a Printer Application, which can be used as a
recipe for migrating any CUPS driver.

PAPPL is licensed under the Apache License Version 2.0 with an exception
to allow linking against GPL2/LGPL2 software (like older versions of CUPS),
so it can be used freely in any project you'd like.  If you want to support
the development of this framework financially, please consider sponsoring me
through [Github][6].  I am also available to do consulting and/or development
through my company Lakeside Robotics (<https://www.lakesiderobotics.ca>).

My hope is that PAPPL will accelerate the adoption of IPP Everywhere™ and
make it easier for people to support other IPP-based licensing programs like
[AirPrint™][4] and [Mopria®][5].


Overview
--------

PAPPL embeds a multi-threaded IPP server and provides callbacks for a
variety of events that allows a GUI or command-line application to interact
with both the local user that is running the Printer Application and any
network clients that are submitting print jobs, querying printer status and
capabilities, and so forth.

PAPPL requires a POSIX-compliant host operating system such as Linux®, macOS®,
QNX®, or VxWorks®, a "make" utility that supports the `include` directive (like
GNU make), a C99-compatible C compiler such as GCC or Clang, and the
"pkg-config" utility.  It also requires the following support libraries:

- Avahi 0.8 or later for mDNS/DNS-SD support (except macOS)
- CUPS 2.2 or later for the CUPS libraries (libcups2/libcupsimage2)
- GNU TLS 3.0 or later (except on macOS) for TLS support
- JPEGLIB 9 or later for JPEG image support (optional for B&W printers)
- LIBPNG 1.6 or later for PNG image support (optional)
- LIBPAM for authentication support (optional)
- ZLIB 1.1 or later for compression support

Most development happens on a Mac, with testing on various Linux distributions
and a [Raspberry Pi Zero W][7] to ensure that memory and CPU requirements
remain low.


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


Legal Stuff
-----------

PAPPL is Copyright © 2019-2020 by Michael R Sweet.

This software is licensed under the Apache License Version 2.0 with an
(optional) exception to allow linking against GPL2/LGPL2 software (like older
versions of CUPS).  See the files "LICENSE" and "NOTICE" for more information.

This software is based loosely on the "ippeveprinter.c" code from [CUPS][11].


[1]: https://github.com/michaelrsweet/lprint
[2]: http://gutenprint.sf.net/
[3]: https://www.pwg.org/ipp/everywhere.html
[4]: https://support.apple.com/en-us/HT201311
[5]: https://mopria.org/
[6]: https://github.com/sponsors/michaelrsweet
[7]: https://www.raspberrypi.org/products/raspberry-pi-zero-w/
[8]: https://ftp.pwg.org/pub/pwg/candidates/cs-ippeve10-20130128-5100.14.pdf
[9]: https://ftp.pwg.org/pub/pwg/candidates/cs-ippeveselfcert10-20160219-5100.20.pdf
[10]: https://ftp.pwg.org/pub/pwg/candidates/cs-ippsystem10-20191122-5100.22.pdf
[11]: https://www.cups.org/
[12]: https://openprinting.github.io/documentation/02-designing-printer-drivers/
