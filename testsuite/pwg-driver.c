//
// PWG test driver for the Printer Application Framework
//
// Copyright © 2020 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

//
// Include necessary headers...
//

#include "testpappl.h"


//
// 'driver_callback()' - Driver callback.
//

bool					// O - `true` on success, `false` on failure
driver_callback(
    const char          *driver_name,	// I - Driver name
    const char          *device_uri,	// I - Device URI
    pappl_driver_data_t *driver_data,	// O - Driver data
    void                *data)		// I - Callback data (unused)
{
  (void)driver_name;
  (void)device_uri;
  (void)driver_data;
  (void)data;

  return (false);
}
