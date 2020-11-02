//
// Mainloop header file for the Printer Application Framework
//
// Copyright © 2020 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#ifndef _PAPPL_MAINLOOP_H_
#  define _PAPPL_MAINLOOP_H_


//
// Include necessary headers
//

#  include "base.h"
#  include "system.h"


//
// C++ magic...
//

#  ifdef __cplusplus
extern "C" {
#  endif // __cplusplus


//
// Types...
//

typedef const char *(*pappl_ml_autoadd_cb_t)(const char *device_info, const char *device_uri, const char *device_id, void *data);
					// Auto-add callback
typedef int (*pappl_ml_subcmd_cb_t)(const char *base_name, int num_options, cups_option_t *options, int num_files, char **files, void *data);
					// Sub-command callback
typedef pappl_system_t *(*pappl_ml_system_cb_t)(int num_options, cups_option_t *options, void *data);
					// System callback
typedef void (*pappl_ml_usage_cb_t)(void *data);
					// Program usage callback


//
// Functions...
//

extern int	papplMainloop(int argc, char *argv[], const char *version, const char *footer_html, int num_drivers, pappl_pr_driver_t *drivers, pappl_pr_driver_cb_t driver_cb, pappl_ml_autoadd_cb_t autoadd_cb, const char *subcmd_name, pappl_ml_subcmd_cb_t subcmd_cb, pappl_ml_system_cb_t system_cb, pappl_ml_usage_cb_t usage_cb, void *data) _PAPPL_PUBLIC;


//
// C++ magic...
//

#  ifdef __cplusplus
}
#  endif // __cplusplus


#endif // !_PAPPL_MAINLOOP_H_
