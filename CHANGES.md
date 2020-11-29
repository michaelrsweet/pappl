Changes in PAPPL
================


Changes in v1.0b2
-----------------

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
