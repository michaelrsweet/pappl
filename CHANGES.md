Changes in PAPPL
================

Changes in v1.2rc1
------------------

- Added explicit support for running macOS printer applications as a server.
- Fixed an issue with the default system callback for `papplMainloop`.


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
