//
// Test suite header file for the Printer Application Framework
//
// Copyright © 2020 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#ifndef _TESTPAPPL_H_
#  define _TESTPAPPL_H_


//
// Include necessary headers...
//

#include <pappl/pappl.h>


//
// Functions...
//

extern bool	driver_callback(const char *driver_name, const char *device_uri, pappl_driver_data_t *driver_data, void *data);


#endif // !_TESTPAPPL_H_
