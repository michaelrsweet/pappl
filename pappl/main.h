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


typedef void (*pappl_main_usage_cb_t)(void *data);
typedef bool (*pappl_main_subcommand_cb_t)(char *base_name, char *subcommand, int num_options, cups_option_t *options, int num_files, char **files, void *data);
typedef pappl_system_t *(*pappl_main_system_cb_t)(int num_options, cups_option_t *options, void *data);


//
// C++ magic...
//

#  ifdef __cplusplus
extern "C" {
#  endif // __cplusplus


extern int papplMain(int argc, char* argv[], pappl_main_usage_cb_t usage_cb, pappl_main_subcommand_cb_t subcommand_cb, pappl_main_system_cb_t system_cb, void *data) _PAPPL_PUBLIC;


//
// C++ magic...
//

#  ifdef __cplusplus
}
#  endif // __cplusplus


#  endif // !_PAPPL_MAIN_H_
