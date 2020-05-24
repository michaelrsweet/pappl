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


//
// C++ magic...
//

#  ifdef __cplusplus
extern "C" {
#  endif // __cplusplus


extern int    papplMain(int argc, char* argv[], pappl_driver_cb_t cb) _PAPPL_PUBLIC;
extern void   papplMainAddSystemOption(pappl_soptions_t option) _PAPPL_PUBLIC;
extern void   papplMainRemoveSystemOption(pappl_soptions_t option) _PAPPL_PUBLIC;


//
// C++ magic...
//

#  ifdef __cplusplus
}
#  endif // __cplusplus


#  endif // !_PAPPL_MAIN_H_
