//
// USB printer class support for the Printer Application Framework
//
// Copyright © 2019-2021 by Michael R Sweet.
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
#  include <sys/mount.h>
#  include <sys/syscall.h>
#  include <linux/usb/functionfs.h>
#  include <linux/usb/g_printer.h>
#endif // __linux


//
// Local constants...
//

#ifdef __linux
#  define NUM_IPP_USB		3
#  define LINUX_USB_CONTROLLER	"/sys/class/udc"
#  define LINUX_USB_GADGET	"/sys/kernel/config/usb_gadget/g1"
#  define LINUX_IPPUSB_FFSPATH	"/dev/ffs-ippusb%d"
#endif // __linux


//
// Local types...
//

#ifdef __linux
typedef struct _ipp_usb_descriptors_s	// IPP-USB descriptors
{
  struct usb_functionfs_descs_head_v2
		header;			// Descriptor header
  __le32	fs_count;		// Number of full-speed endpoints
  __le32	hs_count;		// Number of high-speed endpoints
  __le32	ss_count;		// Number of super-speed endpoints
  struct
  {
    struct usb_interface_descriptor
			intf;			// Interface descriptor
    struct usb_endpoint_descriptor_no_audio
			ipp_to_printer,		// IPP/HTTP requests
			ipp_to_host;		// IPP/HTTP responses
  } __attribute__((packed))
		fs_descs,		// Full-speed endpoints
		hs_descs,		// High-speed endpoints
		ss_descs;		// Super-speed endpoints
} __attribute__((packed)) _ipp_usb_descriptors_t;


typedef struct _ipp_usb_iface_s		// IPP-USB interface data
{
  pappl_printer_t *printer;		// Printer
  int		number,			// Interface number (0-N)
		ipp_control,		// IPP-USB control file
		ipp_to_printer,		// IPP/HTTP requests file
		ipp_to_host,		// IPP/HTTP responses file
		ipp_sock;		// Local IPP socket connection, if any
  http_addrlist_t *addrlist;		// Local socket address
  pthread_t	host_thread,		// Thread ID for "to host" comm
		printer_thread;		// Thread ID for "to printer" comm
} _ipp_usb_iface_t;
#endif // __linux


//
// Local functions...
//

#ifdef __linux
static bool	create_directory(pappl_printer_t *printer, const char *filename);
static bool	create_ipp_usb_iface(pappl_printer_t *printer, int number, _ipp_usb_iface_t *iface);
static bool	create_string_file(pappl_printer_t *printer, const char *filename, const char *data);
static bool	create_symlink(pappl_printer_t *printer, const char *filename, const char *destname);
static void	delete_ipp_usb_iface(_ipp_usb_iface_t *data);
static void	disable_usb_printer(pappl_printer_t *printer, _ipp_usb_iface_t *ifaces);
static bool	enable_usb_printer(pappl_printer_t *printer, _ipp_usb_iface_t *ifaces);
static void	*run_ipp_usb_to_host(_ipp_usb_iface_t *iface);
static void	*run_ipp_usb_to_printer(_ipp_usb_iface_t *iface);
#endif // __linux


//
// '_papplPrinterRunUSB() ' - Run the USB printer thread.
//

void *					// O - Thread exit status (not used)
_papplPrinterRunUSB(
    pappl_printer_t *printer)		// I - Printer
{
#ifdef __linux
  int		i;			// Looping var
  struct pollfd	data[NUM_IPP_USB + 1];	// USB printer gadget listeners
  _ipp_usb_iface_t ifaces[NUM_IPP_USB];	// IPP-USB gadget interfaces
  int		count;			// Number of file descriptors from poll()
  pappl_device_t *device = NULL;	// Printer port data
  char		buffer[8192];		// Print data buffer
  ssize_t	bytes;			// Bytes in buffer
  time_t	status_time = 0;	// Last port status update


  printer->usb_active = enable_usb_printer(printer, ifaces);

  if (!printer->usb_active)
  {
    disable_usb_printer(printer, ifaces);
    return (NULL);
  }

  sleep(1);

  if ((data[0].fd = open("/dev/g_printer0", O_RDWR | O_EXCL)) < 0)
  {
    papplLogPrinter(printer, PAPPL_LOGLEVEL_ERROR, "Unable to open USB printer gadget: %s", strerror(errno));
    disable_usb_printer(printer, ifaces);
    return (NULL);
  }

  data[0].events = POLLIN | POLLRDNORM;

  for (i = 0; i < NUM_IPP_USB; i ++)
  {
    data[i + 1].fd     = ifaces[i].ipp_control;
    data[i + 1].events = POLLIN;
  }

  papplLogPrinter(printer, PAPPL_LOGLEVEL_INFO, "Monitoring USB for incoming print jobs.");

  while (!printer->is_deleted && printer->system->is_running)
  {
    if ((count = poll(data, NUM_IPP_USB + 1, 1000)) < 0)
    {
      papplLogPrinter(printer, PAPPL_LOGLEVEL_ERROR, "USB poll failed: %s", strerror(errno));

      if (printer->is_deleted || !printer->system->is_running)
        break;

      sleep(1);
    }
    else if (printer->is_deleted || !printer->system->is_running)
    {
      break;
    }
    else if (count > 0)
    {
      if (data[0].revents)
      {
        if (!device)
        {
          papplLogPrinter(printer, PAPPL_LOGLEVEL_INFO, "Starting USB print job.");

          while (!printer->is_deleted && (device = papplPrinterOpenDevice(printer)) == NULL)
          {
            papplLogPrinter(printer, PAPPL_LOGLEVEL_DEBUG, "Waiting for USB access.");
            sleep(1);
	  }

	  if (printer->is_deleted || !printer->system->is_running)
	  {
	    papplPrinterCloseDevice(printer);
	    break;
	  }

          // Start looking for back-channel data and port status
          status_time = 0;
          data[0].events = POLLIN | POLLOUT | POLLRDNORM | POLLWRNORM;
        }

        if ((time(NULL) - status_time) >= 1)
        {
          // Update port status once a second...
          pappl_preason_t reasons = papplDeviceGetStatus(device);
					// Current USB status bits
          unsigned char	port_status = PRINTER_NOT_ERROR | PRINTER_SELECTED;
					// Current port status bits

          if (reasons & PAPPL_PREASON_OTHER)
            port_status &= ~PRINTER_NOT_ERROR;
	  if (reasons & PAPPL_PREASON_MEDIA_EMPTY)
	    port_status |= PRINTER_PAPER_EMPTY;
	  if (reasons & PAPPL_PREASON_MEDIA_JAM)
	    port_status |= 0x40;	// Extension
	  if (reasons & PAPPL_PREASON_COVER_OPEN)
	    port_status |= 0x80;	// Extension

          ioctl(data[0].fd, GADGET_SET_PRINTER_STATUS, (unsigned char)port_status);

          status_time = time(NULL);
        }

        if (data[0].revents & POLLRDNORM)
        {
	  if ((bytes = read(data[0].fd, buffer, sizeof(buffer))) > 0)
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

        if (data[0].revents & POLLWRNORM)
        {
	  if ((bytes = papplDeviceRead(device, buffer, sizeof(buffer))) > 0)
	  {
	    papplLogPrinter(printer, PAPPL_LOGLEVEL_DEBUG, "Read %d bytes from printer.", (int)bytes);
	    write(data[0].fd, buffer, (size_t)bytes);
	  }
        }
      }

      // Check for IPP-USB control messages...
      for (i = 0; i < NUM_IPP_USB; i ++)
      {
        if (data[i + 1].revents)
        {
          struct usb_functionfs_event *event;
					// IPP-USB control event

	  if (read(ifaces[i].ipp_control, buffer, sizeof(buffer)) < 0)
	  {
	    papplLogPrinter(printer, PAPPL_LOGLEVEL_WARN, "IPP-USB event read error: %s", strerror(errno));
            continue;
	  }

          event = (struct usb_functionfs_event *)buffer;

	  switch (event->type)
	  {
	    case FUNCTIONFS_BIND :
		papplLogPrinter(printer, PAPPL_LOGLEVEL_DEBUG, "BIND%d", i);
		break;
	    case FUNCTIONFS_UNBIND :
		papplLogPrinter(printer, PAPPL_LOGLEVEL_DEBUG, "UNBIND%d", i);
		break;
	    case FUNCTIONFS_ENABLE :
		papplLogPrinter(printer, PAPPL_LOGLEVEL_DEBUG, "ENABLE%d", i);
		break;
	    case FUNCTIONFS_DISABLE :
		papplLogPrinter(printer, PAPPL_LOGLEVEL_DEBUG, "DISABLE%d", i);
		break;
	    case FUNCTIONFS_SETUP :
		papplLogPrinter(printer, PAPPL_LOGLEVEL_DEBUG, "SETUP%d bRequestType=%d, bRequest=%d, wValue=%d, wIndex=%d, wLength=%d", i, event->u.setup.bRequestType, event->u.setup.bRequest, le16toh(event->u.setup.wValue), le16toh(event->u.setup.wIndex), le16toh(event->u.setup.wLength));
		break;
	    case FUNCTIONFS_SUSPEND :
		papplLogPrinter(printer, PAPPL_LOGLEVEL_DEBUG, "SUSPEND%d", i);
		break;
	    case FUNCTIONFS_RESUME :
		papplLogPrinter(printer, PAPPL_LOGLEVEL_DEBUG, "RESUME%d", i);
		break;
	    default :
		papplLogPrinter(printer, PAPPL_LOGLEVEL_DEBUG, "UNKNOWN%d type=%d", i, event->type);
		break;
	  }
        }
      }
    }
    else
    {
      // No new data...
      if (device)
      {
        // Finish talking to the printer...
        papplLogPrinter(printer, PAPPL_LOGLEVEL_INFO, "Finishing USB print job.");
        papplPrinterCloseDevice(printer);
        device = NULL;

        // Stop doing back-channel data
        data[0].events = POLLIN | POLLRDNORM;
      }

      // Sleep 1ms to prevent excessive CPU usage...
      usleep(1000);
    }
  }

  if (device)
  {
    papplLogPrinter(printer, PAPPL_LOGLEVEL_INFO, "Finishing USB print job.");
    papplPrinterCloseDevice(printer);
  }

  papplLogPrinter(printer, PAPPL_LOGLEVEL_INFO, "Disabling USB for incoming print jobs.");

  disable_usb_printer(printer, ifaces);

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
// > on Linux with compatible hardware such as the Raspberry Pi Zero and 4B.
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
// 'create_directory()' - Create a directory.
//

static bool				// O - `true` on success, `false` otherwise
create_directory(
    pappl_printer_t *printer,		// I - Printer
    const char      *filename)		// I - Directory path
{
  if (mkdir(filename, 0777) && errno != EEXIST)
  {
    papplLogPrinter(printer, PAPPL_LOGLEVEL_ERROR, "Unable to create USB gadget directory '%s': %s", filename, strerror(errno));
    return (false);
  }
  else
    return (true);
}


//
// 'create_ipp_usb()' - Create an IPP-USB gadget instance.
//
// Each instance provides a single socket pair.
//

static bool				// O - `true` on success, `false` otherwise
create_ipp_usb_iface(
    pappl_printer_t  *printer,		// I - Printer
    int              number,		// I - Interface number (0-N)
    _ipp_usb_iface_t *iface)		// O - IPP-USB interface data
{
  char		filename[1024],		// Filename
		destname[1024],		// Destination filename for symlink
		devpath[256];		// Device directory
  _ipp_usb_descriptors_t descriptors;	// IPP-USB descriptors
  struct usb_functionfs_strings_head strings;
					// IPP-USB strings (none)


  // Initialize IPP-USB data...
  iface->printer        = printer;
  iface->host_thread    = 0;
  iface->printer_thread = 0;
  iface->number         = number;
  iface->ipp_control    = -1;
  iface->ipp_to_printer = -1;
  iface->ipp_to_host    = -1;
  iface->ipp_sock       = -1;
  iface->addrlist       = NULL;

  // Start by creating the function in the configfs directory...
  snprintf(filename, sizeof(filename), LINUX_USB_GADGET "/functions/ffs.ippusb%d", number);
  if (!create_directory(printer, filename))
    return (false);			// Failed

  snprintf(destname, sizeof(destname), LINUX_USB_GADGET "/configs/c.1/ffs.ippusb%d", number);
  if (!create_symlink(printer, filename, destname))
    return (false);			// Failed

  // Then mount the filesystem...
  snprintf(filename, sizeof(filename), "ippusb%d", number);
  snprintf(devpath, sizeof(devpath), LINUX_IPPUSB_FFSPATH, number);
  if (!create_directory(printer, devpath))
    return (false);			// Failed

  if (mount(filename, devpath, "functionfs", 0, NULL) && errno != EBUSY)
  {
    papplLogPrinter(printer, PAPPL_LOGLEVEL_ERROR, "Unable to mount USB gadget filesystem '%s': %s", devpath, strerror(errno));
    return (false);
  }

  // Try opening the control file...
  snprintf(filename, sizeof(filename), "%s/ep0", devpath);
  if ((iface->ipp_control = open(filename, O_RDWR)) < 0)
  {
    papplLogPrinter(printer, PAPPL_LOGLEVEL_ERROR, "Unable to open USB gadget control file '%s': %s", filename, strerror(errno));
    return (false);
  }

  // Now fill out the USB descriptors...
  memset(&descriptors, 0, sizeof(descriptors));

  descriptors.header.magic  = htole32(FUNCTIONFS_DESCRIPTORS_MAGIC_V2);
  descriptors.header.flags  = htole32(FUNCTIONFS_HAS_FS_DESC | FUNCTIONFS_HAS_HS_DESC | FUNCTIONFS_HAS_SS_DESC);
  descriptors.header.length = htole32(sizeof(descriptors));
  descriptors.fs_count      = htole32(3);
  descriptors.hs_count      = htole32(3);
  descriptors.ss_count      = htole32(3);

  descriptors.fs_descs.intf.bLength            = sizeof(descriptors.fs_descs.intf);
  descriptors.fs_descs.intf.bDescriptorType    = USB_DT_INTERFACE;
  descriptors.fs_descs.intf.bNumEndpoints      = 2;
  descriptors.fs_descs.intf.bInterfaceClass    = USB_CLASS_PRINTER;
  descriptors.fs_descs.intf.bInterfaceSubClass = 1;
  descriptors.fs_descs.intf.bInterfaceProtocol = 4; // IPP-USB

  descriptors.fs_descs.ipp_to_printer.bLength          = sizeof(descriptors.fs_descs.ipp_to_printer);
  descriptors.fs_descs.ipp_to_printer.bDescriptorType  = USB_DT_ENDPOINT;
  descriptors.fs_descs.ipp_to_printer.bEndpointAddress = 1 | USB_DIR_OUT;
  descriptors.fs_descs.ipp_to_printer.bmAttributes     = USB_ENDPOINT_XFER_BULK;

  descriptors.fs_descs.ipp_to_host.bLength          = sizeof(descriptors.fs_descs.ipp_to_host);
  descriptors.fs_descs.ipp_to_host.bDescriptorType  = USB_DT_ENDPOINT;
  descriptors.fs_descs.ipp_to_host.bEndpointAddress = 2 | USB_DIR_IN;
  descriptors.fs_descs.ipp_to_host.bmAttributes     = USB_ENDPOINT_XFER_BULK;

  descriptors.hs_descs.intf.bLength            = sizeof(descriptors.hs_descs.intf);
  descriptors.hs_descs.intf.bDescriptorType    = USB_DT_INTERFACE;
  descriptors.hs_descs.intf.bNumEndpoints      = 2;
  descriptors.hs_descs.intf.bInterfaceClass    = USB_CLASS_PRINTER;
  descriptors.hs_descs.intf.bInterfaceSubClass = 1;
  descriptors.hs_descs.intf.bInterfaceProtocol = 4; // IPP-USB

  descriptors.hs_descs.ipp_to_printer.bLength          = sizeof(descriptors.hs_descs.ipp_to_printer);
  descriptors.hs_descs.ipp_to_printer.bDescriptorType  = USB_DT_ENDPOINT;
  descriptors.hs_descs.ipp_to_printer.bEndpointAddress = 1 | USB_DIR_OUT;
  descriptors.hs_descs.ipp_to_printer.bmAttributes     = USB_ENDPOINT_XFER_BULK;
  descriptors.hs_descs.ipp_to_printer.wMaxPacketSize   = htole16(512);

  descriptors.hs_descs.ipp_to_host.bLength          = sizeof(descriptors.hs_descs.ipp_to_host);
  descriptors.hs_descs.ipp_to_host.bDescriptorType  = USB_DT_ENDPOINT;
  descriptors.hs_descs.ipp_to_host.bEndpointAddress = 2 | USB_DIR_IN;
  descriptors.hs_descs.ipp_to_host.bmAttributes     = USB_ENDPOINT_XFER_BULK;
  descriptors.hs_descs.ipp_to_host.wMaxPacketSize   = htole16(512);

  descriptors.ss_descs.intf.bLength            = sizeof(descriptors.ss_descs.intf);
  descriptors.ss_descs.intf.bDescriptorType    = USB_DT_INTERFACE;
  descriptors.ss_descs.intf.bNumEndpoints      = 2;
  descriptors.ss_descs.intf.bInterfaceClass    = USB_CLASS_PRINTER;
  descriptors.ss_descs.intf.bInterfaceSubClass = 1;
  descriptors.ss_descs.intf.bInterfaceProtocol = 4; // IPP-USB

  descriptors.ss_descs.ipp_to_printer.bLength          = sizeof(descriptors.ss_descs.ipp_to_printer);
  descriptors.ss_descs.ipp_to_printer.bDescriptorType  = USB_DT_ENDPOINT;
  descriptors.ss_descs.ipp_to_printer.bEndpointAddress = 1 | USB_DIR_OUT;
  descriptors.ss_descs.ipp_to_printer.bmAttributes     = USB_ENDPOINT_XFER_BULK;
  descriptors.ss_descs.ipp_to_printer.wMaxPacketSize   = htole16(1024);

  descriptors.ss_descs.ipp_to_host.bLength          = sizeof(descriptors.ss_descs.ipp_to_host);
  descriptors.ss_descs.ipp_to_host.bDescriptorType  = USB_DT_ENDPOINT;
  descriptors.ss_descs.ipp_to_host.bEndpointAddress = 2 | USB_DIR_IN;
  descriptors.ss_descs.ipp_to_host.bmAttributes     = USB_ENDPOINT_XFER_BULK;
  descriptors.ss_descs.ipp_to_host.wMaxPacketSize   = htole16(1024);

  // and the strings (none)...
  memset(&strings, 0, sizeof(strings));
  strings.magic  = htole32(FUNCTIONFS_STRINGS_MAGIC);
  strings.length = htole32(sizeof(strings));

  // Now write the descriptors and strings values...
  if (write(iface->ipp_control, &descriptors, sizeof(descriptors)) < 0)
  {
    papplLogPrinter(printer, PAPPL_LOGLEVEL_ERROR, "Unable to write IPP-USB descriptors to gadget control file '%s': %s", filename, strerror(errno));
    return (false);
  }

  if (write(iface->ipp_control, &strings, sizeof(strings)) < 0)
  {
    papplLogPrinter(printer, PAPPL_LOGLEVEL_ERROR, "Unable to write IPP-USB strings to gadget control file '%s': %s", filename, strerror(errno));
    return (false);
  }

  // At this point the endpoints should be accessible...
  snprintf(filename, sizeof(filename), "%s/ep1", devpath);
  if ((iface->ipp_to_printer = open(filename, O_RDONLY)) < 0)
  {
    papplLogPrinter(printer, PAPPL_LOGLEVEL_ERROR, "Unable to open IPP-USB gadget printer endpoint file '%s': %s", filename, strerror(errno));
    return (false);
  }

  snprintf(filename, sizeof(filename), "%s/ep2", devpath);
  if ((iface->ipp_to_host = open(filename, O_WRONLY)) < 0)
  {
    papplLogPrinter(printer, PAPPL_LOGLEVEL_ERROR, "Unable to open IPP-USB gadget host endpoint file '%s': %s", filename, strerror(errno));
    return (false);
  }

  // Find the address for the local TCP/IP socket...
  snprintf(filename, sizeof(filename), "%d", printer->system->port);
  if ((iface->addrlist = httpAddrGetList("localhost", AF_UNSPEC, filename)) == NULL)
  {
    papplLogPrinter(printer, PAPPL_LOGLEVEL_ERROR, "Unable to lookup 'localhost:%d' for IPP USB gadget: %s", printer->system->port, cupsLastErrorString());
    return (false);
  }

  // Start a thread to relay IPP/HTTP messages between USB and TCP/IP...
  if (pthread_create(&iface->printer_thread, NULL, (void *(*)(void *))run_ipp_usb_to_printer, iface))
  {
    papplLogPrinter(printer, PAPPL_LOGLEVEL_ERROR, "Unable to start IPP-USB gadget thread for endpoint %d: %s", number, strerror(errno));
    return (false);
  }

  // If we got this far, everything is setup!
  return (true);
}


//
// 'create_string_file()' - Create a file containing the specified string.
//
// A newline is automatically added as needed.
//

static bool				// O - `true` on success, `false` otherwise
create_string_file(
    pappl_printer_t *printer,		// I - Printer
    const char      *filename,		// I - File path
    const char      *data)		// I - Contents of file
{
  cups_file_t	*fp;			// File pointer


  if ((fp = cupsFileOpen(filename, "w")) == NULL)
  {
    papplLogPrinter(printer, PAPPL_LOGLEVEL_ERROR, "Unable to create USB gadget file '%s': %s", filename, strerror(errno));
    return (false);
  }

  if (!*data || data[strlen(data) - 1] != '\n')
    cupsFilePrintf(fp, "%s\n", data);
  else
    cupsFilePuts(fp, data);

  if (cupsFileClose(fp))
  {
    papplLogPrinter(printer, PAPPL_LOGLEVEL_ERROR, "Unable to create USB gadget file '%s': %s", filename, strerror(errno));
    return (false);
  }

  return (true);
}


//
// 'create_symlink()' - Create a symbolic link.
//

static bool				// O - `true` on success, `false` otherwise
create_symlink(
    pappl_printer_t *printer,		// I - Printer
    const char      *filename,		// I - Source filename
    const char      *destname)		// I - Destination filename
{
  if (symlink(filename, destname) && errno != EEXIST)
  {
    papplLogPrinter(printer, PAPPL_LOGLEVEL_ERROR, "Unable to create USB gadget symlink '%s': %s", filename, strerror(errno));
    return (false);
  }
  else
    return (true);
}


//
// 'delete_ipp_usb_iface()' - Delete an IPP-USB gadget interface.
//

static void
delete_ipp_usb_iface(
    _ipp_usb_iface_t *iface)		// I - IPP-USB data
{
  char	devpath[256];			// IPP-USB device path


  if (iface->ipp_control >= 0)
  {
    close(iface->ipp_control);
    iface->ipp_control = -1;
  }

  if (iface->host_thread)
  {
    pthread_cancel(iface->host_thread);
    iface->host_thread = 0;
  }

  if (iface->printer_thread)
  {
    pthread_cancel(iface->printer_thread);
    iface->printer_thread = 0;
  }

  if (iface->ipp_to_printer >= 0)
  {
    close(iface->ipp_to_printer);
    iface->ipp_to_printer = -1;
  }

  if (iface->ipp_to_host >= 0)
  {
    close(iface->ipp_to_host);
    iface->ipp_to_host = -1;
  }

  if (iface->ipp_sock >= 0)
  {
    close(iface->ipp_sock);
    iface->ipp_sock = -1;
  }

  httpAddrFreeList(iface->addrlist);
  iface->addrlist = NULL;

  snprintf(devpath, sizeof(devpath), LINUX_IPPUSB_FFSPATH, iface->number);
  if (umount(devpath))
    papplLogPrinter(iface->printer, PAPPL_LOGLEVEL_ERROR, "Unable to unmount '%s': %s", devpath, strerror(errno));
}


//
// 'disable_usb_printer()' - Disable the USB printer gadget module.
//

static void
disable_usb_printer(
    pappl_printer_t  *printer,		// I - Printer
    _ipp_usb_iface_t *ifaces)		// I - IPP-USB interfaces
{
  int			i;		// Looping var
  const char		*gadget_dir = LINUX_USB_GADGET;
					// Gadget directory
  char			filename[1024];	// Filename


  snprintf(filename, sizeof(filename), "%s/UDC", gadget_dir);
  if (!create_string_file(printer, filename, "\n"))
  {
    papplLogPrinter(printer, PAPPL_LOGLEVEL_ERROR, "Unable to create USB gadget file '%s': %s", filename, strerror(errno));
  }

  for (i = 0; i < NUM_IPP_USB; i ++)
    delete_ipp_usb_iface(ifaces + i);

  printer->usb_active = false;
}


//
// 'enable_usb_printer()' - Configure and enable the USB printer gadget module.
//

static bool				// O - `true` on success, `false` otherwise
enable_usb_printer(
    pappl_printer_t  *printer,		// I - Printer
    _ipp_usb_iface_t *ifaces)		// I - IPP-USB interfaces
{
  int			i;		// Looping var
  const char		*gadget_dir = LINUX_USB_GADGET;
					// Gadget directory
  char			filename[1024],	// Filename
			destname[1024];	// Destination filename for symlinks
  cups_dir_t		*dir;		// Controller directory
  cups_dentry_t		*dent;		// Directory entry
  int			num_devid;	// Number of device ID values
  cups_option_t		*devid;		// Device ID values
  const char		*val;		// Value
  char			temp[1024],	// Temporary string
 			mfg[256],	// Manufacturer
			mdl[256],	// Model name
			sn[256];	// Serial number


  // Get the information for this printer - vendor ID, product ID, etc.
  num_devid = papplDeviceParseID(printer->device_id, &devid);

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
  if (!create_directory(printer, gadget_dir))
    return (false);

  snprintf(filename, sizeof(filename), "%s/idVendor", gadget_dir);
  snprintf(temp, sizeof(temp), "0x%04x\n", printer->usb_vendor_id);
  if (!create_string_file(printer, filename, temp))
    return (false);

  snprintf(filename, sizeof(filename), "%s/idProduct", gadget_dir);
  snprintf(temp, sizeof(temp), "0x%04x\n", printer->usb_product_id);
  if (!create_string_file(printer, filename, temp))
    return (false);

  snprintf(filename, sizeof(filename), "%s/strings/0x409", gadget_dir);
  if (!create_directory(printer, filename))
    return (false);

  snprintf(filename, sizeof(filename), "%s/strings/0x409/manufacturer", gadget_dir);
  if (!create_string_file(printer, filename, mfg))
    return (false);

  snprintf(filename, sizeof(filename), "%s/strings/0x409/product", gadget_dir);
  if (!create_string_file(printer, filename, mdl))
    return (false);

  snprintf(filename, sizeof(filename), "%s/strings/0x409/serialnumber", gadget_dir);
  if (!create_string_file(printer, filename, sn))
    return (false);

  snprintf(filename, sizeof(filename), "%s/configs/c.1", gadget_dir);
  if (!create_directory(printer, filename))
    return (false);

  // Create the legacy USB printer gadget...
  snprintf(filename, sizeof(filename), "%s/functions/printer.g_printer0", gadget_dir);
  if (!create_directory(printer, filename))
    return (false);

  snprintf(filename, sizeof(filename), "%s/functions/printer.g_printer0/pnp_string", gadget_dir);
  if (!create_string_file(printer, filename, printer->device_id))
    return (false);

  snprintf(filename, sizeof(filename), "%s/functions/printer.g_printer0/q_len", gadget_dir);
  // Note: Cannot error out on this since q_len cannot be changed once the
  // f_printer module is instantiated - see EMBEDDED.md for details and a patch
  create_string_file(printer, filename, "10\n");

  snprintf(filename, sizeof(filename), "%s/functions/printer.g_printer0", gadget_dir);
  snprintf(destname, sizeof(destname), "%s/configs/c.1/printer.g_printer0", gadget_dir);
  if (!create_symlink(printer, filename, destname))
    return (false);

  // Create the IPP-USB printer gadget...
  for (i = 0; i < NUM_IPP_USB; i ++)
  {
    if (!create_ipp_usb_iface(printer, i, ifaces + i))
    {
      while (i > 0)
      {
        i --;
        delete_ipp_usb_iface(ifaces + i);
      }

      return (false);
    }
  }

  // Add optional gadgets...
  if (printer->usb_options & PAPPL_UOPTIONS_ETHERNET)
  {
    // Standard USB-Ethernet interface...
    snprintf(filename, sizeof(filename), "%s/functions/ncm.usb0", gadget_dir);
    if (!create_directory(printer, filename))
      goto error;

    snprintf(destname, sizeof(destname), "%s/configs/c.1/ncm.usb0", gadget_dir);
    if (!create_symlink(printer, filename, destname))
      goto error;
  }

  if (printer->usb_options & PAPPL_UOPTIONS_SERIAL)
  {
    // Standard serial port...
    snprintf(filename, sizeof(filename), "%s/functions/acm.ttyGS0", gadget_dir);
    if (!create_directory(printer, filename))
      goto error;

    snprintf(destname, sizeof(destname), "%s/configs/c.1/acm.ttyGS0", gadget_dir);
    if (!create_symlink(printer, filename, destname))
      goto error;
  }

  if ((printer->usb_options & PAPPL_UOPTIONS_STORAGE) && printer->usb_storage)
  {
    // Standard USB mass storage device...
    snprintf(filename, sizeof(filename), "%s/functions/mass_storage.0", gadget_dir);
    if (!create_directory(printer, filename))
      goto error;

    snprintf(filename, sizeof(filename), "%s/functions/mass_storage.0/lun.0/file", gadget_dir);
    if (!create_string_file(printer, filename, printer->usb_storage))
      goto error;

    if (printer->usb_options & PAPPL_UOPTIONS_STORAGE_READONLY)
    {
      snprintf(filename, sizeof(filename), "%s/functions/mass_storage.0/lun.0/ro", gadget_dir);
      if (!create_string_file(printer, filename, "1\n"))
        goto error;
    }

    if (printer->usb_options & PAPPL_UOPTIONS_STORAGE_REMOVABLE)
    {
      snprintf(filename, sizeof(filename), "%s/functions/mass_storage.0/lun.0/removable", gadget_dir);
      if (!create_string_file(printer, filename, "1\n"))
        goto error;
    }

    snprintf(filename, sizeof(filename), "%s/functions/mass_storage.0", gadget_dir);
    snprintf(destname, sizeof(destname), "%s/configs/c.1/mass_storage.0", gadget_dir);
    if (!create_symlink(printer, filename, destname))
      goto error;
  }

  // Then assign this configuration to the first USB device controller
  if ((dir = cupsDirOpen(LINUX_USB_CONTROLLER)) == NULL)
  {
    papplLogPrinter(printer, PAPPL_LOGLEVEL_ERROR, "Unable to find USB device controller in '%s': %s", LINUX_USB_CONTROLLER, strerror(errno));
    goto error;
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
    goto error;
  }

  papplLogPrinter(printer, PAPPL_LOGLEVEL_DEBUG, "Using UDC '%s' for USB gadgets.", dent->filename);

  snprintf(filename, sizeof(filename), "%s/UDC", gadget_dir);
  if (!create_string_file(printer, filename, dent->filename))
  {
    cupsDirClose(dir);
    goto error;
  }

  cupsDirClose(dir);

  papplLogPrinter(printer, PAPPL_LOGLEVEL_INFO, "USB printer gadget configured.");

  return (true);

  // If we get here we need to tear down the IPP-USB interfaces...
  error:

  for (i = 0; i < NUM_IPP_USB; i ++)
    delete_ipp_usb_iface(ifaces + i);

  return (false);
}


//
// 'run_ipp_usb_to_host()' - Run an I/O thread from the printer to the host.
//
// This sends IPP/HTTP responses back to the host.
//

static void *				// O - Thread exit status
run_ipp_usb_to_host(
    _ipp_usb_iface_t *iface)		// I - Thread data
{
  char		buffer[8192];		// I/O buffer
  ssize_t	bytes;			// Bytes read
  struct pollfd	poll_data;		// poll() data


  printf("TOHOST%d: Starting for socket %d.\n", iface->number, iface->ipp_sock);

  poll_data.fd     = iface->ipp_sock;
  poll_data.events = POLLIN | POLLHUP | POLLERR;

  while (!iface->printer->is_deleted && iface->printer->system->is_running)
  {
    if (poll(&poll_data, 1, 1000) > 0)
    {
      papplLogPrinter(iface->printer, PAPPL_LOGLEVEL_DEBUG, "TOHOST%d: Reading from socket %d.", iface->number, iface->ipp_sock);

      if ((bytes = read(iface->ipp_sock, buffer, sizeof(buffer))) > 0)
      {
	papplLogPrinter(iface->printer, PAPPL_LOGLEVEL_DEBUG, "TOHOST%d: Sending %d bytes.", iface->number, (int)bytes);

	if (write(iface->ipp_to_host, buffer, (size_t)bytes) < 0)
	{
	  papplLogPrinter(iface->printer, PAPPL_LOGLEVEL_ERROR, "TOHOST%d: Error sending data to host: %s", iface->number, strerror(errno));
	  break;
	}
	else
	  papplLogPrinter(iface->printer, PAPPL_LOGLEVEL_DEBUG, "TOHOST%d: Success", iface->number);
      }
      else if (bytes < 0)
      {
	// Error on socket
	if (errno == EAGAIN || errno == EINTR)
	  continue;			// Ignore

	// Close socket...
	if (errno == EPIPE || errno == ECONNRESET)
	  papplLogPrinter(iface->printer, PAPPL_LOGLEVEL_INFO, "TOHOST%d: Socket %d closed.", iface->number, iface->ipp_sock);
	else
	  papplLogPrinter(iface->printer, PAPPL_LOGLEVEL_ERROR, "TOHOST%d: Unable to read data from socket: %s", iface->number, strerror(errno));
	break;
      }
      else if (bytes == 0)
      {
        // Closed connection
        papplLogPrinter(iface->printer, PAPPL_LOGLEVEL_INFO, "TOHOST%d: Socket %d closed.", iface->number, iface->ipp_sock);
        break;
      }
    }
  }

  papplLogPrinter(iface->printer, PAPPL_LOGLEVEL_DEBUG, "TOHOST%d: Shutting down socket %d.", iface->number, iface->ipp_sock);

  close(iface->ipp_sock);
  iface->ipp_sock    = -1;
  iface->host_thread = 0;

  return (NULL);
}


//
// 'run_ipp_usb_to_printer()' - Run an I/O thread from the host to the printer.
//

static void *				// O - Thread exit status
run_ipp_usb_to_printer(
    _ipp_usb_iface_t *iface)		// I - Thread data
{
  char		buffer[8192];		// I/O buffer
  ssize_t	bytes;			// Bytes read


  printf("TOPRINTER%d: Starting.\n", iface->number);

  while (!iface->printer->is_deleted && iface->printer->system->is_running)
  {
    if ((bytes = read(iface->ipp_to_printer, buffer, sizeof(buffer))) > 0)
    {
      if (iface->ipp_sock < 0)
      {
	if (!httpAddrConnect2(iface->addrlist, &iface->ipp_sock, 10000, NULL))
	{
	  papplLogPrinter(iface->printer, PAPPL_LOGLEVEL_ERROR, "TOPRINTER%d: Unable to connect to local socket: %s", iface->number, strerror(errno));
	  break;
	}

	papplLogPrinter(iface->printer, PAPPL_LOGLEVEL_INFO, "TOPRINTER%d: Opened socket %d.", iface->number, iface->ipp_sock);
	if (pthread_create(&iface->host_thread, NULL, (void *(*)(void *))run_ipp_usb_to_host, iface))
	{
	  papplLogPrinter(iface->printer, PAPPL_LOGLEVEL_ERROR, "TOPRINTER%d: Unable to start socket IO thread: %s", iface->number, strerror(errno));
	  break;
	}
      }

      papplLogPrinter(iface->printer, PAPPL_LOGLEVEL_DEBUG, "TOPRINTER%d: Writing %d bytes to socket %d.", iface->number, (int)bytes, iface->ipp_sock);

      if (write(iface->ipp_sock, buffer, (size_t)bytes) < 0)
      {
        papplLogPrinter(iface->printer, PAPPL_LOGLEVEL_ERROR, "TOPRINTER%d: Unable to write data to socket: %s", iface->number, strerror(errno));
        break;
      }
    }
  }

  papplLogPrinter(iface->printer, PAPPL_LOGLEVEL_INFO, "TOPRINTER%d: Shutting down.", iface->number);

  if (iface->ipp_sock >= 0)
  {
    pthread_cancel(iface->host_thread);
    close(iface->ipp_sock);
    iface->ipp_sock = -1;
    iface->host_thread = 0;
  }

  iface->printer_thread = 0;

  return (NULL);
}
#endif // __linux
