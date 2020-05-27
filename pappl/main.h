//
// Main header file for the Printer Application Framework
//
// Copyright © 2020 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#ifndef _PAPPL_MAIN_H_
#  define _PAPPL_MAIN_H_


//
// Include necessary headers
//

#  include "base.h"
#  include "system.h"


typedef void (*pappl_driver_cb_t)(pappl_system_t *system);
typedef void (*pappl_usage_cb_t)();
typedef void (*pappl_error_cb_t)();


//
// C++ magic...
//

#  ifdef __cplusplus
extern "C" {
#  endif // __cplusplus


extern int    papplMain(int argc, char* argv[], pappl_driver_cb_t driver_cb, const char *cb_state, const char *footer, pappl_soptions_t soptions, int num_versions, pappl_version_t *sversion, pappl_contact_t *scontact, const char *geolocation, const char *organization, pappl_usage_cb_t usage_cb, pappl_error_cb_t error_cb) _PAPPL_PUBLIC;


//
// C++ magic...
//

#  ifdef __cplusplus
}
#  endif // __cplusplus


#  endif // !_PAPPL_MAIN_H_
