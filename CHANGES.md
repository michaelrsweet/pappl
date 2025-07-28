Changes in PAPPL
================


Changes in v1.4.10 (YYYY-MM-DD)
-------------------------------

- Changed the preferred/first printer URI to use the "ipps" scheme.
- Now suppress a duplicate 'auto' value for "media-source-supported" to work
  around a bug in the legacy-printer-app (Issue #394)
- Now show the default and supported "output-bin" options (Issue #393)
- Now log the TLS version and cipher suite, when available.
- Now create spool files with read-only permissions.


Changes in v1.4.9 (2025-03-20)
------------------------------

- Fixed a bug in job event notifications.
- Fixed a bug that would delay shutdown by 60 seconds.
- Fixed some notification bugs.
- Fixed validation error checking bug in `papplPrinterCreate` (Issue #385)
- Fixed page number that is passed to the raster endpage function.
- Disabled raw socket support on Windows.


Changes in v1.4.8 (2024-11-14)
------------------------------

- SECURITY: The web interface password didn't work properly (Issue #373)
- Now use the "listen-hostname" hostname as system hostname if a name is
  specified (Issue #369)


Changes in v1.4.7 (2024-08-15)
------------------------------

- PAM-based authentication did not work on Linux due to a glibc incompatibility
  (Issue #343)
- Fixed the web interface for setting the admin and print groups (Issue #356)
- Fixed the web interface for adding network printers on non-standard port
  numbers (Issue #360)
- Fixed some USB gadget error conditions.
- Fixed the Wi-Fi configuration web page.
- Fixed a logging deadlock issue.
- Fixed some threading issues.
- Fixed how PAPPL responds to an unsupported request character set.
- Fixed the "no-tls" server option.


Changes in v1.4.6 (2024-02-09)
------------------------------

- Fixed reporting of "printer-strings-languages-supported" attribute
  (Issue #328)
- Fixed saving of "print-darkness-default" and "print-speed-default" values
  (Issue #330 and #337)
- Fixed incoming "raw" print socket support (Issue #331 and #338)
- Fixed web interface support for "printer-darkness" (Issue #333)
- Fixed some issues discovered by OpenScanHub (Issue #335)
- Fixed localization of command-line (main loop) interface.


Changes in v1.4.5 (2024-01-26)
------------------------------

- Fixed `--disable-libpam` configure option.
- Fixed support for "finishings", "output-bin", and "sides" options.
- Fixed IEEE-1284 device ID generation code.
- Fixed crash in retrofit printer application (Issue #322)
- Fixed some Coverity-detected threading issues.


Changes in v1.4.4 (2023-12-21)
------------------------------

- Fixed "printer-settable-attributes-supported" value (Issue #311)
- Fixed `-n` support for setting number of copies (Issue #312)
- Fixed `papplPrinterSetDriverDefaults` didn't set the
  "orientation-requested-default" value (Issue #313)
- Fixed job file preservation logic.
- Fixed builds against current libcups3.


Changes in v1.4.3 (2023-11-15)
------------------------------

- Added "smi55357-device-uri" and "smi55357-driver" Printer Status attributes
  to Get-Printer-Attributes responses.
- Fixed missing mutex unlock in DNS-SD code (Issue #299)
- Fixed "printer-id" value for new printers (Issue #301)
- Fixed DNS-SD device list crash (Issue #302)
- Fixed Set-Printer-Attributes for "output-bin-default" and "sides-default"
  (Issue #305)
- Fixed default "copies" value with `papplJobCreateWithFile`.


Changes in v1.4.2 (2024-10-16)
------------------------------

- Fixed potential crash while listing devices (Issue #296)
- Fixed potential deadlock issue (Issue #297)
- Fixed loading of previous state (Issue #298)


Changes in v1.4.1 (2024-10-10)
------------------------------

- Fixed typos in the names of the `papplJobResume` and `papplJobSuspend`
  functions (Issue #295)


Changes in v1.4.0 (2024-09-28)
------------------------------

- Added support for "job-retain-until" (Issue #14)
- Added new PAPPL-Create-Printers operation, and the PAPPL mainloop API now
  auto-adds local printers the first time a server is run (Issue #245)
- Added new `papplDeviceRemoveScheme` and `papplDeviceRemoveTypes` APIs to
  disable unwanted device types (Issue #259)
- Added support for suspending and resuming jobs at copy boundaries (Issue #266)
- Added support for server configuration files (Issue #279)
- Now preserve the paused state of printers (Issue #286)
- Fixed reporting of "xxx-k-octet-supported" attributes.
- Fixed printing of 1/2/4-bit grayscale PNG images (Issue #267)
- Fixed USB serial number for DYMO printers (Issue #271)
- Fixed a potential buffer overflow in the logging code (Issue #272)
- Fixed DNS-SD advertisements when the server name is set to "localhost"
  (Issue #274)
- Fixed hostname change detection when using mDNSResponder (Issue #282)
- Fixed authentication cookie comparisons for simple password mode.
- Fixed a potential time-of-use issue with PAPPL-created directories.
- Fixed handling of trailing '%' in log format strings.
- Updated the `options` sub-command to list vendor options and values
  (Issue #255)
- Updated web interface to show the age of jobs (Issue #256)
- Updated "devices" sub-command to have the PAPPL server find the devices
  instead of doing it directly (Issue #262)
- Updated default logging to be less chatty (Issue #270)
- Updated the Wi-Fi configuration page to support hidden networks.
- Updated the Wi-Fi configuration page reload time to 30 seconds.
- Updated TLS certificate generation to support more types of certificates and
  to use modern OpenSSL/GNU TLS APIs.
