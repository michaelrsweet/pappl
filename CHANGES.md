Changes in PAPPL
================

Changes in v1.2b1
-----------------

- Added IPP notifications support with `papplSystemAddEvent` and
  `papplSubscriptionXxx` functions (Issue #191)
- Added `papplPrinterDisable` and `papplPrinterEnable` functions and proper
  support for the IPP "printer-is-accepting-jobs" attribute.
- Updated `papplPrinterSetReadyMedia` to support up to `PAPPL_MAX_SOURCE`
  media entries, regardless of the number of sources.
