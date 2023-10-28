Changes in PAPPL
================


Changes in v2.0b1
-----------------

- Now require libcups v3 or higher.
- Increased `PAPPL_MAX_TYPE` to 128 (Issue #268)
- Added "smi55357-device-uri" and "smi55357-driver" Printer Status attributes
  to Get-Printer-Attributes responses.
- Updated `papplDeviceOpen` and `pappl_devopen_cb_t` to accept a `pappl_job_t`
  pointer instead of just the job name string.
- Updated APIs to use `size_t` for counts instead of `int`, for compatibility
  with libcups v3 (Issue #221)
- Fixed potential crash while listing devices (Issue #296)
- Fixed potential deadlock issue (Issue #297)
- Fixed loading of previous state (Issue #298)
- Fixed "printer-id" value for new printers (Issue #301)
