//
// Printer driver functions for the Printer Application Framework
//
// Copyright © 2020 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

//
// Include necessary headers...
//

#include "printer-private.h"


//
// 'papplPrinterGetDriverData()' - Get the current driver data.
//

pappl_driver_data_t *			// O - Driver data or `NULL` if none
papplPrinterGetDriverData(
    pappl_printer_t     *printer,	// I - Printer
    pappl_driver_data_t *data)		// I - Pointer to driver data structure to fill
{
  if (!printer || !printer->driver_name || !data)
  {
    if (data)
      memset(data, 0, sizeof(pappl_driver_data_t));

    return (NULL);
  }

  memcpy(data, &printer->driver_data, sizeof(pappl_driver_data_t));

  return (data);
}


//
// 'papplPrinterGetDriverName()' - Get the current driver name.
//

char *					// O - Driver name or `NULL` for none
papplPrinterGetDriverName(
    pappl_printer_t *printer,		// I - Printer
    char            *buffer,		// I - String buffer
    size_t          bufsize)		// I - Size of string buffer
{
  if (!printer || !printer->driver_name || !buffer || bufsize == 0)
  {
    if (buffer)
      *buffer = '\0';

    return (NULL);
  }

  pthread_rwlock_rdlock(&printer->rwlock);
  strlcpy(buffer, printer->driver_name, bufsize);
  pthread_rwlock_unlock(&printer->rwlock);

  return (buffer);
}


//
// 'papplPrinterSetDriverData()' - Set the driver data.
//

void
papplPrinterSetDriverData(
    pappl_printer_t     *printer,	// I - Printer
    pappl_driver_data_t *data)		// I - Driver data
{
  // TODO: implement me
  // Copy driver data then recreate driver_attrs
}
