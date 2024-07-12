PAPPL - Printer Application Framework
=====================================

![Version](https://img.shields.io/github/v/release/michaelrsweet/pappl?include_prereleases)
![Apache 2.0](https://img.shields.io/github/license/michaelrsweet/pappl)
![Build](https://github.com/michaelrsweet/pappl/workflows/Build/badge.svg)
[![Coverity Scan Status](https://img.shields.io/coverity/scan/22385.svg)](https://scan.coverity.com/projects/michaelrsweet-pappl)

PAPPL is a simple C-based framework/library for developing CUPS Printer
Applications, which are the recommended replacement for printer drivers.  It
was specifically developed to support [LPrint][1] and a future [Gutenprint][2]
Printer Application but is sufficiently general purpose to support any kind of
printer or driver that can be used on desktops, servers, and in embedded
environments.

PAPPL supports JPEG, PNG, PWG Raster, Apple Raster, and "raw" printing to
printers connected via USB and network (AppSocket/JetDirect) connections.
PAPPL provides an embedded [IPP Everywhere™][3] service that provides access
to printers locally or on your whole network.

PAPPL is licensed under the Apache License Version 2.0 with an exception
to allow linking against GPL2/LGPL2 software (like older versions of CUPS),
so it can be used freely in any project you'd like.  If you want to support
the development of this framework financially, please consider sponsoring me
through [Github][4].  I am also available to do consulting and/or development
through my company Lakeside Robotics (<https://www.lakesiderobotics.ca>).

PAPPL is helping to accelerate the adoption of IPP Everywhere™ and is making it
easier for developers to support other IPP-based licensing programs like
[AirPrint™][5] and [Mopria®][6].


Requirements
------------

PAPPL requires Microsoft® Windows® 10 or higher or a POSIX-compliant host
operating system such as Linux®, macOS®, QNX®, or VxWorks®.  On Windows, the
provided project files require Visual Studio 2019 or higher.  For POSIX hosts,
a "make" utility that supports the `include` directive (like GNU make), a
C99-compatible C compiler such as GCC or Clang, and the "pkg-config" utility
are required along with the following support libraries:

- Avahi (0.8 or later) or mDNSResponder for mDNS/DNS-SD support
- CUPS (2.2 or later) or libcups (3.0 or later) for the CUPS libraries
- GNU TLS (3.0 or later), LibreSSL (3.0 or later), or OpenSSL (1.1 or later)
  for TLS support
- JPEGLIB (8 or later) or libjpeg-turbo (2.0 or later) for JPEG image support
  (optional for B&W printers)
- LIBPNG (1.6 or later) for PNG image support (optional)
- LIBPAM for authentication support (optional)
- LIBUSB (1.0 or later) for USB printing support (optional)
- PAM for authentication support (optional)
- ZLIB (1.1 or later) for compression support
- LIBXML for xml parsing support

Most development happens on Intel and Apple Silicon Macs, with testing on
various Linux distributions, Windows 10+, and a [Raspberry Pi Zero W][7] to
ensure that memory and CPU requirements remain low.


Documentation and Examples
--------------------------

Documentation can be found in the "doc" and "man" directories.

The OpenPrinting group has [written a tutorial][8] showing how to migrate the
`rastertohp` driver from CUPS to a Printer Application using PAPPL, which can
be used as a recipe for migrating any CUPS driver.  This example is available
in the [hp-printer-app][9] project and is also discussed in the PAPPL
documentation.

The OpenPrinting group has also developed a [PostScript printer application][10]
using PAPPL to support the many otherwise unsupported PostScript and
Ghostscript-based printers.


Contributing Code and Translations
----------------------------------

Code contributions should be submitted as pull requests on the Github site:

    http://github.com/michaelrsweet/pappl/pulls

See the file "CONTRIBUTING.md" for more details.

PAPPL uses [Weblate][11] to manage the localization of the web interface and
IPP attributes and values shared by all printer applications, and those likewise
end up as pull requests on Github.


Legal Stuff
-----------

PAPPL is Copyright © 2019-2024 by Michael R Sweet.

This software is licensed under the Apache License Version 2.0 with an
(optional) exception to allow linking against GPL2/LGPL2 software (like older
versions of CUPS).  See the files "LICENSE" and "NOTICE" for more information.

This software is based loosely on the "ippeveprinter.c" code from [CUPS][12].


[1]: https://github.com/michaelrsweet/lprint
[2]: http://gutenprint.sf.net/
[3]: https://www.pwg.org/ipp/everywhere.html
[4]: https://github.com/sponsors/michaelrsweet
[5]: https://support.apple.com/en-us/HT201311
[6]: https://mopria.org/
[7]: https://www.raspberrypi.org/products/raspberry-pi-zero-w/
[8]: https://openprinting.github.io/documentation/02-designing-printer-drivers/
[9]: https://github.com/michaelrsweet/hp-printer-app
[10]: https://github.com/openprinting/ps-printer-app
[11]: https://hosted.weblate.org
[12]: https://openprinting.github.io/cups
