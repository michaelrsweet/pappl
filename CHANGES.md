Changes in PAPPL
================


Changes in v1.0b2
-----------------

- Added IEEE-1284 device ID to argument list for printer driver callbacks
  (Issue #70)
- Documentation updated (Issue #71)
- The main loop now shows an error message if an option is provided after "-o"
  without a space (Issue #80)
- Fixed test page and identify buttons (Issue #81)
- Code cleanup (Issue #82)
- Boolean vendor options are now shown as checkboxes (Issue #85)
- Added the "path" value for the DNS-SD printer web page, and added a
  registration for the system web page in multi-queue mode.
- `papplDeviceRead` now has a 100ms timeout for USB and network connections.
- Implemented back-channel and status updates for the USB printer gadget.
