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
#include <cups/dir.h>
#ifdef __linux
#  include <sys/ioctl.h>
#  include <sys/syscall.h>
#  include <linux/usb/g_printer.h>
#endif // __linux


//
// Local constants...
//

#define LINUX_USB_CONTROLLER	"/sys/class/udc"
#define LINUX_USB_GADGET	"/sys/kernel/config/usb_gadget/g1"


//
// Local functions...
//

#ifdef __linux
static void	disable_usb_printer(pappl_printer_t *printer);
static bool	enable_usb_printer(pappl_printer_t *printer);
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
  int		count;			// Number of file descriptors from poll()
  pappl_device_t *device = NULL;	// Printer port data
  char		buffer[8192];		// Print data buffer
  ssize_t	bytes;			// Bytes in buffer


  if (!enable_usb_printer(printer))
    return (NULL);

  if ((data.fd = open("/dev/g_printer0", O_RDWR | O_EXCL)) < 0)
  {
    papplLogPrinter(printer, PAPPL_LOGLEVEL_ERROR, "Unable to open USB printer gadget: %s", strerror(errno));
    return (NULL);
  }

  data.events = POLLIN | POLLRDNORM;

  papplLogPrinter(printer, PAPPL_LOGLEVEL_INFO, "Monitoring USB for incoming print jobs.");

  while (printer->system->is_running)
  {
    // TODO: Support read-back and status updates
    if ((count = poll(&data, 1, 1000)) < 0)
    {
      papplLogPrinter(printer, PAPPL_LOGLEVEL_ERROR, "USB poll failed: %s", strerror(errno));
      sleep(1);
    }
    else if (count > 0)
    {
      if (!device)
      {
        papplLogPrinter(printer, PAPPL_LOGLEVEL_INFO, "Starting USB print job.");

        while ((device = papplPrinterOpenDevice(printer)) == NULL)
        {
          papplLogPrinter(printer, PAPPL_LOGLEVEL_DEBUG, "Waiting for USB access.");
          sleep(1);
	}
      }

      if ((bytes = read(data.fd, buffer, sizeof(buffer))) > 0)
      {
        papplLogPrinter(printer, PAPPL_LOGLEVEL_DEBUG, "Read %d bytes from USB port.", (int)bytes);
        papplDeviceWrite(device, buffer, (size_t)bytes);
        papplDeviceFlush(device);
      }
      else
      {
        if (bytes < 0)
          papplLogPrinter(printer, PAPPL_LOGLEVEL_ERROR, "Read error from USB port: %s", strerror(errno));

	papplLogPrinter(printer, PAPPL_LOGLEVEL_INFO, "Finishing USB print job.");
	papplPrinterCloseDevice(printer);
	device = NULL;
      }
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

  papplLogPrinter(printer, PAPPL_LOGLEVEL_INFO, "Disabling USB for incoming print jobs.");

  disable_usb_printer(printer);

#else
  (void)printer;
#endif // __linux

  return (NULL);
}


//
// 'papplPrinterSetUSB()' - Set the USB vendor and product IDs for a printer.
//
// This function sets the USB vendor and product IDs for a printer as well as
// specifying USB gadget options when the printer is registered with the USB
// device controller.
//
// > Note: USB gadget functionality is currently only available when running
// > on Linux with compatible hardware such as the Raspberry Pi.
//

void
papplPrinterSetUSB(
    pappl_printer_t  *printer,		// I - Printer
    unsigned         vendor_id,		// I - USB vendor ID
    unsigned         product_id,	// I - USB product ID
    pappl_uoptions_t options,		// I - USB gadget options
    const char       *storagefile)	// I - USB storage file, if any
{
  if (printer)
  {
    printer->usb_vendor_id  = (unsigned short)vendor_id;
    printer->usb_product_id = (unsigned short)product_id;
    printer->usb_options    = options;

    free(printer->usb_storage);

    if (storagefile)
      printer->usb_storage = strdup(storagefile);
    else
      printer->usb_storage = NULL;
  }
}


#ifdef __linux
//
// 'disable_usb_printer()' - Disable the USB printer gadget module.
//

static void
disable_usb_printer(
    pappl_printer_t *printer)		// I - Printer
{
  const char		*gadget_dir = LINUX_USB_GADGET;
					// Gadget directory
  char			filename[1024];	// Filename
  cups_file_t		*fp;		// File


  snprintf(filename, sizeof(filename), "%s/UDC", gadget_dir);
  if ((fp = cupsFileOpen(filename, "w")) != NULL)
  {
    cupsFilePuts(fp, "\n");
    cupsFileClose(fp);
  }
  else
  {
    papplLogPrinter(printer, PAPPL_LOGLEVEL_ERROR, "Unable to create USB gadget file '%s': %s", filename, strerror(errno));
  }
}


//
// 'enable_usb_printer()' - Configure and enable the USB printer gadget module.
//

static bool				// O - `true` on success, `false` otherwise
enable_usb_printer(
    pappl_printer_t *printer)		// I - Printer
{
  const char		*gadget_dir = LINUX_USB_GADGET;
					// Gadget directory
  char			filename[1024],	// Filename
			destname[1024];	// Destination filename for symlinks
  cups_dir_t		*dir;		// Controller directory
  cups_dentry_t		*dent;		// Directory entry
  cups_file_t		*fp;		// File
  int			num_devid;	// Number of device ID values
  cups_option_t		*devid;		// Device ID values
  const char		*val;		// Value
  char			mfg[256],	// Manufacturer
			mdl[256],	// Model name
			sn[256];	// Serial number


  // Get the information for this printer - vendor ID, product ID, etc.
  num_devid = papplDeviceParse1284ID(printer->device_id, &devid);

  val = cupsGetOption("MANUFACTURER", num_devid, devid);
  if (!val)
    val = cupsGetOption("MFG", num_devid, devid);
  if (!val)
    val = cupsGetOption("MFR", num_devid, devid);

  if (val)
    strlcpy(mfg, val, sizeof(mfg));
  else
    strlcpy(mfg, "Unknown", sizeof(mfg));

  val = cupsGetOption("MODEL", num_devid, devid);
  if (!val)
    val = cupsGetOption("MDL", num_devid, devid);

  if (val)
    strlcpy(mdl, val, sizeof(mdl));
  else
    strlcpy(mdl, "Printer", sizeof(mdl));

  val = cupsGetOption("SERIALNUMBER", num_devid, devid);
  if (!val)
    val = cupsGetOption("SN", num_devid, devid);
  if (!val)
    val = cupsGetOption("SER", num_devid, devid);
  if (!val)
    val = cupsGetOption("SERN", num_devid, devid);
  if (!val && (val = strstr(printer->device_uri, "?serial=")) != NULL)
    val += 8;

  if (val)
    strlcpy(sn, val, sizeof(sn));
  else
    strlcpy(sn, "0", sizeof(sn));

  cupsFreeOptions(num_devid, devid);

  // Make sure the old-style gadget modules are not loaded, as they will tie
  // up the USB device controller and not allow our configfs-based gadgets to
  // be used.
  syscall(__NR_delete_module, "g_printer", O_NONBLOCK);
  syscall(__NR_delete_module, "g_serial", O_NONBLOCK);

  // Modern Linux kernels support USB gadgets through the configfs interface.
  // PAPPL takes control of this interface, so if you need (for example) a
  // serial gadget in addition to the printer gadget you need to specify that
  // with a call to papplPrinterSetUSB.
  //
  // The configfs interface lives under "/sys/kernel/config/usb_gadget/".  The
  // available USB Device Controllers can be found under "/sys/class/udc".  We
  // currently assume there will only be one of those and will expand the USB
  // gadget interface later as needed.
  //
  // The typical directory structure looks like this:
  //
  //   g1/
  //     idVendor (usb_vendor ID as a hex number, e.g. "0x12CD")
  //     idProduct (usb product ID as a hex number, e.g. "0x34AB")
  //     strings/0x409/
  //       manufacturer (manufacturer name string)
  //       product (model name string)
  //       serialnumber (serial number string)
  //     configs/c.1/
  //       symlink to functions/printer.g_printer0
  //     functions/printer.g_printer0
  //       pnp_string (IEEE-1284 device ID string)
  //     UDC (first entry from /sys/class/udc)

  // Create the gadget configuration files and directories...
  if (mkdir(gadget_dir, 0777) && errno != EEXIST)
  {
    papplLogPrinter(printer, PAPPL_LOGLEVEL_ERROR, "Unable to create USB gadget directory '%s': %s", gadget_dir, strerror(errno));
    return (false);
  }

  snprintf(filename, sizeof(filename), "%s/idVendor", gadget_dir);
  if ((fp = cupsFileOpen(filename, "w")) == NULL)
  {
    papplLogPrinter(printer, PAPPL_LOGLEVEL_ERROR, "Unable to create USB gadget file '%s': %s", filename, strerror(errno));
    return (false);
  }
  cupsFilePrintf(fp, "0x%04X\n", printer->usb_vendor_id);
  cupsFileClose(fp);

  snprintf(filename, sizeof(filename), "%s/idProduct", gadget_dir);
  if ((fp = cupsFileOpen(filename, "w")) == NULL)
  {
    papplLogPrinter(printer, PAPPL_LOGLEVEL_ERROR, "Unable to create USB gadget file '%s': %s", filename, strerror(errno));
    return (false);
  }
  cupsFilePrintf(fp, "0x%04X\n", printer->usb_product_id);
  cupsFileClose(fp);

  snprintf(filename, sizeof(filename), "%s/strings/0x409", gadget_dir);
  if (mkdir(filename, 0777) && errno != EEXIST)
  {
    papplLogPrinter(printer, PAPPL_LOGLEVEL_ERROR, "Unable to create USB gadget directory '%s': %s", filename, strerror(errno));
    return (false);
  }

  snprintf(filename, sizeof(filename), "%s/strings/0x409/manufacturer", gadget_dir);
  if ((fp = cupsFileOpen(filename, "w")) == NULL)
  {
    papplLogPrinter(printer, PAPPL_LOGLEVEL_ERROR, "Unable to create USB gadget file '%s': %s", filename, strerror(errno));
    return (false);
  }
  cupsFilePrintf(fp, "%s\n", mfg);
  cupsFileClose(fp);

  snprintf(filename, sizeof(filename), "%s/strings/0x409/product", gadget_dir);
  if ((fp = cupsFileOpen(filename, "w")) == NULL)
  {
    papplLogPrinter(printer, PAPPL_LOGLEVEL_ERROR, "Unable to create USB gadget file '%s': %s", filename, strerror(errno));
    return (false);
  }
  cupsFilePrintf(fp, "%s\n", mdl);
  cupsFileClose(fp);

  snprintf(filename, sizeof(filename), "%s/strings/0x409/serialnumber", gadget_dir);
  if ((fp = cupsFileOpen(filename, "w")) == NULL)
  {
    papplLogPrinter(printer, PAPPL_LOGLEVEL_ERROR, "Unable to create USB gadget file '%s': %s", filename, strerror(errno));
    return (false);
  }
  cupsFilePrintf(fp, "%s\n", sn);
  cupsFileClose(fp);

  snprintf(filename, sizeof(filename), "%s/configs/c.1", gadget_dir);
  if (mkdir(filename, 0777) && errno != EEXIST)
  {
    papplLogPrinter(printer, PAPPL_LOGLEVEL_ERROR, "Unable to create USB gadget directory '%s': %s", filename, strerror(errno));
    return (false);
  }

  snprintf(filename, sizeof(filename), "%s/functions/printer.g_printer0", gadget_dir);
  if (mkdir(filename, 0777) && errno != EEXIST)
  {
    papplLogPrinter(printer, PAPPL_LOGLEVEL_ERROR, "Unable to create USB gadget directory '%s': %s", filename, strerror(errno));
    return (false);
  }

  snprintf(filename, sizeof(filename), "%s/functions/printer.g_printer0/pnp_string", gadget_dir);
  if ((fp = cupsFileOpen(filename, "w")) == NULL)
  {
    papplLogPrinter(printer, PAPPL_LOGLEVEL_ERROR, "Unable to create USB gadget file '%s': %s", filename, strerror(errno));
    return (false);
  }
  cupsFilePrintf(fp, "%s\n", printer->device_id);
  cupsFileClose(fp);

  snprintf(filename, sizeof(filename), "%s/functions/printer.g_printer0/q_len", gadget_dir);
  if ((fp = cupsFileOpen(filename, "w")) == NULL)
  {
    papplLogPrinter(printer, PAPPL_LOGLEVEL_WARN, "Unable to create USB gadget file '%s': %s", filename, strerror(errno));
  }
  cupsFilePuts(fp, "10\n");
  cupsFileClose(fp);

  snprintf(filename, sizeof(filename), "%s/functions/printer.g_printer0", gadget_dir);
  snprintf(destname, sizeof(destname), "%s/configs/c.1/printer.g_printer0", gadget_dir);
  if (symlink(filename, destname) && errno != EEXIST)
  {
    papplLogPrinter(printer, PAPPL_LOGLEVEL_ERROR, "Unable to create USB gadget symlink '%s': %s", destname, strerror(errno));
    return (false);
  }

  // Add optional gadgets...
  if (printer->usb_options & PAPPL_UOPTIONS_SERIAL)
  {
    // Standard serial port...
    snprintf(filename, sizeof(filename), "%s/functions/acm.ttyGS0", gadget_dir);
    if (mkdir(filename, 0777) && errno != EEXIST)
    {
      papplLogPrinter(printer, PAPPL_LOGLEVEL_ERROR, "Unable to create USB gadget directory '%s': %s", filename, strerror(errno));
      return (false);
    }

    snprintf(destname, sizeof(destname), "%s/configs/c.1/acm.ttyGS0", gadget_dir);
    if (symlink(filename, destname) && errno != EEXIST)
    {
      papplLogPrinter(printer, PAPPL_LOGLEVEL_ERROR, "Unable to create USB gadget symlink '%s': %s", destname, strerror(errno));
      return (false);
    }
  }

  // Then assign this configuration to the first USB device controller
  if ((dir = cupsDirOpen(LINUX_USB_CONTROLLER)) == NULL)
  {
    papplLogPrinter(printer, PAPPL_LOGLEVEL_ERROR, "Unable to find USB device controller in '%s': %s", LINUX_USB_CONTROLLER, strerror(errno));
    return (false);
  }

  while ((dent = cupsDirRead(dir)) != NULL)
  {
    if (dent->filename[0] != '.' && dent->filename[0])
      break;
  }

  if (!dent)
  {
    papplLogPrinter(printer, PAPPL_LOGLEVEL_ERROR, "No USB device controller in '%s': %s", LINUX_USB_CONTROLLER, strerror(errno));
    cupsDirClose(dir);
    return (false);
  }

  papplLogPrinter(printer, PAPPL_LOGLEVEL_DEBUG, "Using UDC '%s' for USB gadgets.", dent->filename);

  snprintf(filename, sizeof(filename), "%s/UDC", gadget_dir);
  if ((fp = cupsFileOpen(filename, "w")) == NULL)
  {
    papplLogPrinter(printer, PAPPL_LOGLEVEL_ERROR, "Unable to create USB gadget file '%s': %s", filename, strerror(errno));
    cupsDirClose(dir);
    return (false);
  }
  cupsFilePrintf(fp, "%s\n", dent->filename);
  cupsFileClose(fp);

  cupsDirClose(dir);

  papplLogPrinter(printer, PAPPL_LOGLEVEL_INFO, "USB printer gadget configured.");

  return (true);
}
#endif // __linux
