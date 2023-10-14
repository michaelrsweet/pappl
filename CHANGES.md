Changes in PAPPL
================


Changes in v2.0b1
-----------------

- Now require libcups v3 or higher.
- Increased `PAPPL_MAX_TYPE` to 128 (Issue #268)
- Updated APIs to use `size_t` for counts instead of `int`, for compatibility
  with libcups v3 (Issue #221)
- Fixed potential crash while listing devices (Issue #296)
- Fixed potential deadlock issue (Issue #297)
- Fixed loading of previous state (Issue #298)
