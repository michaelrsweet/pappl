Changes in PAPPL
================


Changes in v1.0.2
-----------------

- The `papplSystemSetVersions` function now allows changes while the system is
  running (Issue #123)
- The DNS-SD support functions did not handle when the Avahi daemon is not
  running (Issue #129)
- Deleting and adding a printer with the same name will cause a crash
  (Issue #141)
- The `papplPrinterSetDriverDefaults` function did not validate the defaults
  against the actual driver data.
- The IPP interface no longer allows the Create-Printer operation for single
  queue applications.
- Stopping a printer application with `SIGTERM` now behaves the same as sending
  a Shutdown-System request.
- Added more unit tests to testpappl.


Changes in v1.0.1
-----------------

- Documentation updates (Issue #105)
- The `papplSystemLoadState` function did not load vendor attribute defaults
  correctly (Issue #103)
- Vendor options without "xxx-supported" attributes are no longer shown on the
  printing defaults page (Issue #104)
- Added support for Windows 10/Mopria clients that incorrectly convert the
  printer resource path to lowercase (Issue #106)
- The `papplSystemLoadState` function now calls the printer driver's status
  callback after loading the printer's attributes (Issue #107)
- Added additional error handling for memory allocations throughout the library
  (Issue #113)
- Fixed an issue with validation of custom media sizes (Issue #120)
- Partially-discovered SNMP printers would cause a crash (Issue #121)
- The "copies-supported" attribute was not report correctly.
- Job operations that targeted a non-existent job yielded the wrong status code.
- Printing a test page from the web interface did not trigger a reload to update
  the printer and job state.
- The TLS web page was hardcoded to use "/etc/cups" for the CUPS server root.
- Fixed file output when the job name contains a '/'.
- Updated 1-bit driver output to support "photo" dither array for high print
  quality.
- PAPPL now (re)creates the spool directory as needed.
- Coverity: Added missing NULL checks.
- Coverity: Fixed file descriptor leaks.
- Coverity: Fixed some locking issues.
- Coverity: Fixed printer-darkness-configured bug in `papplSystemSaveState`.
- Coverity: Fixed an error handling bug in the file printing code for the PWG
  test driver.
- Coverity: Removed dead code.


Changes in v1.0.0
-----------------

- `papplSystemLoadState` would not load printers whose device IDs contained the
  `#` character (Issue #92)
- Passing "auto" for the driver name would cause a crash if there was no auto-
  add callback.
- Added `papplPrinterGetPath` API to get the path for a printer web page
  (Issue #97)
- The `papplPrinterAddLink` and `papplSystemAddLink` functions now accept an
  "options" argument instead of the "secure" boolean in order to allow links to
  be added to multiple places on the web interface in addition to requesting a
  secure (HTTPS) link (Issue #98)


Changes in v1.0rc1
------------------

- Added IEEE-1284 device ID to argument list for printer driver callbacks
  (Issue #70)
- Documentation updated (Issue #71)
- Printers discovered via DNS-SD now report their IEEE-1284 device ID string
  (Issue #73)
- The "auto-add" callback is now part of the system's printer driver interface,
  allowing IPP, web, and command-line clients to access it (Issue #74)
- Now save state after deleting a printer (Issue #75)
- Now check whether a named printer already exists (Issue #76)
- Support for "output-bin-default" was missing from the web interface
  (Issue #77)
- Fixed support for vendor options at the command-line (Issue #79)
- The main loop now shows an error message if an option is provided after "-o"
  without a space (Issue #80)
- Fixed test page and identify buttons (Issue #81)
- Code cleanup (Issue #82)
- Boolean vendor options are now shown as checkboxes (Issue #85)
- Made several improvements to the web interface for adding printers (Issue #86)
- `papplSystemLoadState` no longer crashes when it cannot create a printer
  (Issue #87)
- Fixed a crash bug in the "autoadd" command provided by `papplMainloop`
  (Issue #89)
- Added a printer creation callback to `papplSystemSetPrinterDrivers` that is
  run after a printer is created (Issue #90)
- Added the "path" value for the DNS-SD printer web page, and added a
  registration for the system web page in multi-queue mode.
- `papplDeviceRead` now has a 100ms timeout for USB and network connections.
- Implemented back-channel and status updates for the USB printer gadget.
- Finished implementation of test suite for major code paths/job processing
  functionality.
- Fixed a bug in the log rotation code.
- Fixed some threading bugs with the various object lists managed by the
  system.


Changes in v1.0b1
-----------------

- Initial beta release.
