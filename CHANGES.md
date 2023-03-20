Changes in PAPPL
================

Changes in v1.4.0
-----------------

- Added support for "job-retain-until" (Issue #14)
- Added new PAPPL-Create-Printers operation, and the PAPPL mainloop API now
  auto-adds local printers the first time a server is run (Issue #245)
- Added new `papplDeviceRemoveScheme` and `papplDeviceRemoveTypes` APIs to
  disable unwanted device types (Issue #259)
- Fixed printing of 1/2/4-bit grayscale PNG images (Issue #267)
- Updated the `options` sub-command to list vendor options and values
  (Issue #255)
- Updated web interface to show the age of jobs (Issue #256)
- Updated "devices" sub-command to have the PAPPL server find the devices
  instead of doing it directly (Issue #262)
- Updated the Wi-Fi configuration page to support hidden networks.