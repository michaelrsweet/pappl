Changes in PAPPL
================

Changes in v1.2rc1
------------------

- Added explicit support for running macOS printer applications as a server.
- Added unit test support for the new SNMP-based supply level and status
  monitoring code.
- Updated USB gadget code to not enable gadget until system is started or USB
  options are set.
- Fixed DNS-SD advertising when adding a printer from the web interface.
- Fixed double "Supplies" buttons in the web interface.
- Fixed human-readable location fields in web interfaces.
- Fixed an issue with the default system callback for `papplMainloop`.
- Fixed an issue with `papplDeviceList` and DNS-SD discovery when there was no
  active system.
- Fixed printer compatibility issues with the new `papplDeviceGetSupplies` API.


Changes in v1.2b1
-----------------

- Added macOS menubar icon/menu (Issue #27)
- Added support for localization, with base localizations for English, French,
  German, Italian, Japanese, and Spanish (Issue #58)
- Added interpolation when printing JPEG images or when using the
  `papplJobFilterImage` function with smoothing enabled (Issue #64)
- Added `papplDeviceGetSupplies` API to query supply levels via SNMP (Issue #83)
- Added support for custom media sizes in millimeters (Issue #118)
- Added `papplPrinterGet/SetMaxPreservedJobs` API and reprint web interface
  (Issue #189)
- Added IPP notifications support with `papplSystemAddEvent` and
  `papplSubscriptionXxx` functions (Issue #191)
- Added `papplPrinterDisable` and `papplPrinterEnable` functions and proper
  support for the IPP "printer-is-accepting-jobs" attribute.
- Added OpenSSL/LibreSSL support (Issue #195)
- Added `papplSystemGet/SetMaxClients` API (Issue #198)
- Updated `papplPrinterSetReadyMedia` to support up to `PAPPL_MAX_SOURCE`
  media entries, regardless of the number of sources.
