//
// USB printer class support for the Printer Application Framework
//
// Copyright © 2019-2020 by Michael R Sweet.
// Copyright © 2010-2019 by Apple Inc.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

//
// Include necessary headers...
//

#include "pappl-private.h"
#ifdef __linux
#  include <sys/ioctl.h>
#  include <linux/usb/g_printer.h>
#endif // __linux


//
// Local functions...
//


//
// '_papplPrinterRunUSB() ' - Run the USB printer thread.
//

void *					// O - Thread exit status (not used)
_papplPrinterRunUSB(
    pappl_printer_t *printer)		// I - Printer
{
//#ifdef __linux
  struct pollfd	data;			// USB printer gadget listener
  pappl_device_t *device = NULL;	// Printer port data
  char		buffer[8192];		// Print data buffer
  ssize_t	bytes;			// Bytes in buffer


  if ((data.fd = open("/dev/g_printer", O_RDWR | O_EXCL)) < 0)
  {
    papplLogPrinter(printer, PAPPL_LOGLEVEL_ERROR, "Unable to open USB printer gadget: %s", strerror(errno));
    return (NULL);
  }

  data.events = POLLIN;

  while (printer->system->is_running)
  {
    // TODO: Support read-back and status updates
    if (poll(&data, 1, 10000) > 0)
    {
      if (!device)
      {
        while ((device = papplPrinterOpenDevice(printer)) == NULL)
        {
          papplLogPrinter(printer, PAPPL_LOGLEVEL_DEBUG, "Waiting for USB access.");
          sleep(1);
	}

        papplLogPrinter(printer, PAPPL_LOGLEVEL_INFO, "Starting USB print job.");
      }

      if ((bytes = read(data.fd, buffer, sizeof(buffer))) > 0)
        papplDeviceWrite(device, buffer, (size_t)bytes);
    }
    else if (device)
    {
      papplLogPrinter(printer, PAPPL_LOGLEVEL_INFO, "Finishing USB print job.");
      papplPrinterCloseDevice(printer);
      device = NULL;
    }
  }

  if (device)
  {
    papplLogPrinter(printer, PAPPL_LOGLEVEL_INFO, "Finishing USB print job.");
    papplPrinterCloseDevice(printer);
  }

//#else
//  (void)printer;
//#endif // __linux

  return (NULL);
}


//
// 'papplPrinterSetUSB()' - Set the USB vendor and product IDs for a printer.
//

void
papplPrinterSetUSB(
    pappl_printer_t *printer,		// I - Printer
    unsigned        vendor_id,		// I - USB vendor ID
    unsigned        product_id)		// I - USB product ID
{
  if (printer)
  {
    printer->vendor_id  = (unsigned short)vendor_id;
    printer->product_id = (unsigned short)product_id;
  }
}
