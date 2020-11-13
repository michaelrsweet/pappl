PAPPL - Printer Application Framework
=====================================

![Version](https://img.shields.io/github/v/release/michaelrsweet/pappl?include_prereleases)
![Apache 2.0](https://img.shields.io/github/license/michaelrsweet/pappl)
[![Build Status](https://travis-ci.org/michaelrsweet/pappl.svg?branch=master)](https://travis-ci.org/github/michaelrsweet/pappl)
[![LGTM Grade](https://img.shields.io/lgtm/grade/cpp/github/michaelrsweet/pappl)](https://lgtm.com/projects/g/michaelrsweet/pappl/context:cpp)
[![LGTM Alerts](https://img.shields.io/lgtm/alerts/github/michaelrsweet/pappl)](https://lgtm.com/projects/g/michaelrsweet/pappl/)

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

My hope is that PAPPL will accelerate the adoption of IPP Everywhere™ and
make it easier for people to support other IPP-based licensing programs like
[AirPrint™][5] and [Mopria®][6].


Requirements
------------

PAPPL requires a POSIX-compliant host operating system such as Linux®, macOS®,
QNX®, or VxWorks®, a "make" utility that supports the `include` directive (like
GNU make), a C99-compatible C compiler such as GCC or Clang, and the
"pkg-config" utility.  It also requires the following support libraries:

- Avahi 0.8 or later for mDNS/DNS-SD support (except on macOS)
- CUPS 2.2 or later for the CUPS libraries (libcups2/libcupsimage2)
- GNU TLS 3.0 or later (except on macOS) for TLS support
- JPEGLIB 9 or later for JPEG image support (optional for B&W printers)
- LIBPNG 1.6 or later for PNG image support (optional)
- LIBPAM for authentication support (optional)
- ZLIB 1.1 or later for compression support

Most development happens on a Mac, with testing on various Linux distributions
and a [Raspberry Pi Zero W][7] to ensure that memory and CPU requirements
remain low.


Documentation and Examples
--------------------------

Documentation can be found in the "doc" and "man" directories.

The OpenPrinting group has [written a tutorial][8] showing how to migrate the
`rastertohp` driver from CUPS to a Printer Application using PAPPL, which can
be used as a recipe for migrating any CUPS driver.  This example is available
in the [hp-printer-app][9] project and is also discussed in the PAPPL
documentation.

The OpenPrinting group is also developing a [PostScript printer application][10]
using PAPPL to support the many otherwise unsupported PostScript and
Ghostscript-based printers.


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
[4]: https://github.com/sponsors/michaelrsweet
[5]: https://support.apple.com/en-us/HT201311
[6]: https://mopria.org/
[7]: https://www.raspberrypi.org/products/raspberry-pi-zero-w/
[8]: https://openprinting.github.io/documentation/02-designing-printer-drivers/
[9]: https://github.com/michaelsweet/hp-printer-app
[10]: https://github.com/openprinting/ps-printer-app
[11]: https://www.cups.org/
