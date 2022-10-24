Changes in PAPPL
================

Changes in v1.3b1
-----------------

- Added debug logging for device management.
- Added support for PNG image scaling using embedded resolution information
  (Issue #65)
- Added `papplLocGetDefaultMediaSizeName` function to get the default media size
  for the current country (Issue #167)
- Added support for localized banners at the top of printer and system web pages
  (Issue #183)
- Added timer APIs to manage periodic tasks (Issue #208)
- Added support for network configuration via callbacks (Issue #217)
- Added APIs to limit the maximum size of JPEG/PNG images (Issue #224)
- Changed names of PAPPL-specific attributes to use "smi55357" prefix.
- Fixed a device race condition with job processing.
- Fixed a initialization timing issue with USB gadgets on newer Linux kernels.
- Fixed a potential memory underflow with USB device IDs.
- Fixed web interface support for vendor text options (Issue #142)
- Fixed a potential value overflow when reading SNMP OIDs (Issue #210)
- Fixed more CUPS 2.2.x compatibility issues (Issue #212)
- Fixed a 100% CPU usage bug when cleaning the job history (Issue #218)
- Fixed the default values of `--with-papplstatedir` and `--with-papplsockdir`
  to use the `localstatedir` value (Issue #219)
- Fixed storage of label offsets for printers that implement them.
- Updated PAPPL to conform to the new prototype PWG 5100.13 specification
  (Issue #216)
