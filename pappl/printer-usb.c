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
#include <sys/utsname.h>
#ifdef __linux
#  include <sys/ioctl.h>
#  include <sys/syscall.h>
#  include <linux/usb/g_printer.h>
#endif // __linux


//
// Local functions...
//

#ifdef __linux
static bool	load_usb_printer(pappl_printer_t *printer);
static void	unload_usb_printer(void);
#endif // __linux


//
// '_papplPrinterRunUSB() ' - Run the USB printer thread.
//

void *					// O - Thread exit status (not used)
_papplPrinterRunUSB(
    pappl_printer_t *printer)		// I - Printer
{
#ifdef __linux
  struct pollfd	data;			// USB printer gadget listener
  pappl_device_t *device = NULL;	// Printer port data
  char		buffer[8192];		// Print data buffer
  ssize_t	bytes;			// Bytes in buffer


  if (!load_usb_printer(printer))
    return (NULL);

  if ((data.fd = open("/dev/g_printer0", O_RDWR | O_EXCL)) < 0)
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

  unload_usb_printer();

#else
  (void)printer;
#endif // __linux

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


#ifdef __linux
//
// 'load_usb_printer()' - Load the USB printer gadget module.
//

static bool				// O - `true` on success, `false` otherwise
load_usb_printer(
    pappl_printer_t *printer)		// I - Printer
{
  struct utsname	info;		// System information
  char			filename[1024],	// Module file name
			params[2048];	// Module parameters
  int			fd;		// Module file descriptor
  int			num_devid;	// Number of device ID values
  cups_option_t		*devid;		// Device ID values
  const char		*mfg,		// Manufacturer
			*mdl,		// Model name
			*sn;		// Serial number


  // Make sure the g_printer module is unloaded first...
  unload_usb_printer();

  // Then try opening the USB printer gadget driver at:
  //
  //   /lib/modules/`uname -r`/kernel/drivers/usb/gadget/legacy/g_printer.ko
  uname(&info);

  snprintf(filename, sizeof(filename), "/lib/modules/%s/kernel/drivers/usb/gadget/legacy/g_printer.ko", info.release);

  if ((fd = open(filename, O_RDONLY)) < 0)
  {
    papplLogPrinter(printer, PAPPL_LOGLEVEL_ERROR, "Unable to open USB printer gadget module at '%s': %s", filename, strerror(errno));
    return (false);
  }

  // Build the driver parameters for this printer - vendor ID, product ID, etc.
  num_devid = papplDeviceParse1284ID(printer->device_id, &devid);

  mfg = cupsGetOption("MANUFACTURER", num_devid, devid);
  if (!mfg)
    mfg = cupsGetOption("MFG", num_devid, devid);
  if (!mfg)
    mfg = cupsGetOption("MFR", num_devid, devid);
  if (!mfg)
    mfg = "Unknown";

  mdl = cupsGetOption("MODEL", num_devid, devid);
  if (!mdl)
    mdl = cupsGetOption("MDL", num_devid, devid);
  if (!mdl)
    mdl = "Printer";

  sn = cupsGetOption("SERIALNUMBER", num_devid, devid);
  if (!sn)
    sn = cupsGetOption("SN", num_devid, devid);
  if (!sn)
    sn = cupsGetOption("SER", num_devid, devid);
  if (!sn)
    sn = cupsGetOption("SERN", num_devid, devid);
  if (!sn)
    sn = "0";

  snprintf(params, sizeof(params), "idVendor=0x%04x idProduct=0x%04x bcdDevice=0x%04d iManufacturer='%s' iProduct='%s' iSerialNum='%s' iPNPstring='%s'", printer->vendor_id, printer->product_id, PAPPL_VERSION_MAJOR * 100 + PAPPL_VERSION_MINOR, mfg, mdl, sn, printer->device_id);

  cupsFreeOptions(num_devid, devid);

  // Ask the kernel to load the driver with the parameters for this printer...
  if (syscall(__NR_finit_module, fd, params, 0))
  {
    papplLogPrinter(printer, PAPPL_LOGLEVEL_ERROR, "Unable to load USB printer gadget module at '%s': %s", filename, strerror(errno));
    close(fd);
    return (false);
  }

  papplLogPrinter(printer, PAPPL_LOGLEVEL_DEBUG, "Loaded USB printer gadget module at '%s'.", filename);
  close(fd);

  return (true);
}


//
// 'unload_usb_printer()' - Unload the USB printer gadget module.
//

static void
unload_usb_printer(void)
{
  syscall(__NR_delete_module, "g_printer", O_NONBLOCK);
}
#endif // __linux

