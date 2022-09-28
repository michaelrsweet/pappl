//
// USB device support code for the Printer Application Framework
//
// Copyright © 2019-2021 by Michael R Sweet.
// Copyright © 2007-2019 by Apple Inc.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

//
// Include necessary headers...
//

//#define DEBUG 1
#include "device-private.h"
#include "printer.h"
#ifdef HAVE_LIBUSB
#  include <libusb.h>
#endif // HAVE_LIBUSB


//
// Local types...
//

#ifdef HAVE_LIBUSB
typedef struct _pappl_usb_dev_s		// USB device data
{
  struct libusb_device	*device;		// Device info
  struct libusb_device_handle *handle;		// Open handle to device
  int			conf,			// Configuration
			origconf,		// Original configuration
			iface,			// Interface
			ifacenum,		// Interface number
			altset,			// Alternate setting
			write_endp,		// Write endpoint
			read_endp,		// Read endpoint
			protocol;		// Protocol: 1 = Uni-di, 2 = Bi-di.
} _pappl_usb_dev_t;
#endif // HAVE_LIBUSB


//
// Local functions...
//

#ifdef HAVE_LIBUSB
static void		pappl_usb_close(pappl_device_t *device);
static bool		pappl_usb_find(pappl_device_cb_t cb, void *data, _pappl_usb_dev_t *device, pappl_deverror_cb_t err_cb, void *err_data);
static char		*pappl_usb_getid(pappl_device_t *device, char *buffer, size_t bufsize);
static bool		pappl_usb_list(pappl_device_cb_t cb, void *data, pappl_deverror_cb_t err_cb, void *err_data);
static bool		pappl_usb_open(pappl_device_t *device, const char *device_uri, const char *name);
static bool		pappl_usb_open_cb(const char *device_info, const char *device_uri, const char *device_id, void *data);
static ssize_t		pappl_usb_read(pappl_device_t *device, void *buffer, size_t bytes);
static pappl_preason_t	pappl_usb_status(pappl_device_t *device);
static ssize_t		pappl_usb_write(pappl_device_t *device, const void *buffer, size_t bytes);
#endif // HAVE_LIBUSB


//
// '_papplDeviceAddUSBScheme()' - Add the USB scheme.
//

void
_papplDeviceAddUSBScheme(void)
{
#ifdef HAVE_LIBUSB
  papplDeviceAddScheme("usb", PAPPL_DEVTYPE_USB, pappl_usb_list, pappl_usb_open, pappl_usb_close, pappl_usb_read, pappl_usb_write, pappl_usb_status, pappl_usb_getid);
#endif // HAVE_LIBUSB
}


#ifdef HAVE_LIBUSB
//
// 'pappl_usb_close()' - Close a USB device.
//

static void
pappl_usb_close(pappl_device_t *device)	// I - Device
{
  _pappl_usb_dev_t	*usb = (_pappl_usb_dev_t *)papplDeviceGetData(device);
					// USB device data


  libusb_close(usb->handle);
  libusb_unref_device(usb->device);

  free(usb);

  papplDeviceSetData(device, NULL);
}


//
// 'pappl_usb_find()' - Find a USB printer.
//

static bool				// O - `true` if found, `false` if not
pappl_usb_find(
    pappl_device_cb_t   cb,		// I - Callback function
    void                *data,		// I - User data pointer
    _pappl_usb_dev_t    *device,	// O - USB device info
    pappl_deverror_cb_t err_cb,		// I - Error callback
    void                *err_data)	// I - Error callback data
{
  ssize_t	err = 0,		// Current error
		i,			// Looping var
		num_udevs;		// Number of USB devices
  libusb_device	**udevs;		// USB devices


 /*
  * Get the list of connected USB devices...
  */

  device->device = NULL;
  device->handle = NULL;

  if ((err = libusb_init(NULL)) != 0)
  {
    _papplDeviceError(err_cb, err_data, "Unable to initialize USB access: %s", libusb_strerror((enum libusb_error)err));
    return (false);
  }

  num_udevs = libusb_get_device_list(NULL, &udevs);

  _PAPPL_DEBUG("pappl_usb_find: num_udevs=%d\n", (int)num_udevs);

  // Find the printers and do the callback until we find a match.
  for (i = 0; i < num_udevs; i ++)
  {
    libusb_device *udevice = udevs[i];	// Current device
    char	device_id[1024],	// Current device ID
		device_info[256],	// Current device description
		device_uri[1024];	// Current device URI
    struct libusb_device_descriptor devdesc;
					// Current device descriptor
    struct libusb_config_descriptor *confptr = NULL;
					// Pointer to current configuration
    const struct libusb_interface *ifaceptr = NULL;
					// Pointer to current interface
    const struct libusb_interface_descriptor *altptr = NULL;
					// Pointer to current alternate setting
    const struct libusb_endpoint_descriptor *endpptr = NULL;
					// Pointer to current endpoint
    uint8_t	conf,			// Current configuration
		iface,			// Current interface
		altset,			// Current alternate setting
		endp,			// Current endpoint
		read_endp,		// Current read endpoint
		write_endp;		// Current write endpoint

    // Ignore devices with no configuration data and anything that is not
    // a printer...
    if (libusb_get_device_descriptor(udevice, &devdesc) < 0)
    {
      _PAPPL_DEBUG("pappl_usb_find: udev%d - no descriptor.\n", (int)i);
      continue;
    }

    _PAPPL_DEBUG("pappl_usb_find: udev%d -\n", (int)i);
    _PAPPL_DEBUG("pappl_usb_find:     bLength=%d\n", devdesc.bLength);
    _PAPPL_DEBUG("pappl_usb_find:     bDescriptorType=%d\n", devdesc.bDescriptorType);
    _PAPPL_DEBUG("pappl_usb_find:     bcdUSB=%04x\n", devdesc.bcdUSB);
    _PAPPL_DEBUG("pappl_usb_find:     bDeviceClass=%d\n", devdesc.bDeviceClass);
    _PAPPL_DEBUG("pappl_usb_find:     bDeviceSubClass=%d\n", devdesc.bDeviceSubClass);
    _PAPPL_DEBUG("pappl_usb_find:     bDeviceProtocol=%d\n", devdesc.bDeviceProtocol);
    _PAPPL_DEBUG("pappl_usb_find:     bMaxPacketSize0=%d\n", devdesc.bMaxPacketSize0);
    _PAPPL_DEBUG("pappl_usb_find:     idVendor=0x%04x\n", devdesc.idVendor);
    _PAPPL_DEBUG("pappl_usb_find:     idProduct=0x%04x\n", devdesc.idProduct);
    _PAPPL_DEBUG("pappl_usb_find:     bcdDevice=%04x\n", devdesc.bcdDevice);
    _PAPPL_DEBUG("pappl_usb_find:     iManufacturer=%d\n", devdesc.iManufacturer);
    _PAPPL_DEBUG("pappl_usb_find:     iProduct=%d\n", devdesc.iProduct);
    _PAPPL_DEBUG("pappl_usb_find:     iSerialNumber=%d\n", devdesc.iSerialNumber);
    _PAPPL_DEBUG("pappl_usb_find:     bNumConfigurations=%d\n", devdesc.bNumConfigurations);

    if (!devdesc.bNumConfigurations || !devdesc.idVendor || !devdesc.idProduct)
      continue;

    if (devdesc.idVendor == 0x05ac)
      continue;				// Skip Apple devices...

    device->device     = udevice;
    device->handle     = NULL;
    device->conf       = -1;
    device->origconf   = -1;
    device->iface      = -1;
    device->ifacenum   = -1;
    device->altset     = -1;
    device->write_endp = -1;
    device->read_endp  = -1;
    device->protocol   = 0;

    for (conf = 0; conf < devdesc.bNumConfigurations; conf ++)
    {
      if (libusb_get_config_descriptor(udevice, conf, &confptr) < 0)
      {
        _PAPPL_DEBUG("pappl_usb_find:     conf%d - no descriptor\n", conf);
	continue;
      }

      _PAPPL_DEBUG("pappl_usb_find:     conf%d -\n", conf);
      _PAPPL_DEBUG("pappl_usb_find:         bLength=%d\n", confptr->bLength);
      _PAPPL_DEBUG("pappl_usb_find:         bDescriptorType=%d\n", confptr->bDescriptorType);
      _PAPPL_DEBUG("pappl_usb_find:         wTotalLength=%d\n", confptr->wTotalLength);
      _PAPPL_DEBUG("pappl_usb_find:         bNumInterfaces=%d\n", confptr->bNumInterfaces);
      _PAPPL_DEBUG("pappl_usb_find:         bConfigurationValue=%d\n", confptr->bConfigurationValue);
      _PAPPL_DEBUG("pappl_usb_find:         iConfiguration=%d\n", confptr->iConfiguration);
      _PAPPL_DEBUG("pappl_usb_find:         bmAttributes=%d\n", confptr->bmAttributes);
      _PAPPL_DEBUG("pappl_usb_find:         MaxPower=%d\n", confptr->MaxPower);
      _PAPPL_DEBUG("pappl_usb_find:         interface=%p\n", confptr->interface);
      _PAPPL_DEBUG("pappl_usb_find:         extra=%p\n", confptr->extra);
      _PAPPL_DEBUG("pappl_usb_find:         extra_length=%d\n", confptr->extra_length);

      // Some printers offer multiple interfaces...
      for (iface = 0, ifaceptr = confptr->interface; iface < confptr->bNumInterfaces; iface ++, ifaceptr ++)
      {
        if (!ifaceptr->altsetting)
        {
          _PAPPL_DEBUG("pappl_usb_find:         iface%d - no alternate setting\n", iface);
          continue;
        }

	_PAPPL_DEBUG("pappl_usb_find:         iface%d -\n", iface);
	_PAPPL_DEBUG("pappl_usb_find:             num_altsetting=%d\n", ifaceptr->num_altsetting);
	_PAPPL_DEBUG("pappl_usb_find:             altsetting=%p\n", ifaceptr->altsetting);

        device->protocol = 0;

	for (altset = 0, altptr = ifaceptr->altsetting; (int)altset < ifaceptr->num_altsetting; altset ++, altptr ++)
	{
	  _PAPPL_DEBUG("pappl_usb_find:             altset%d - bInterfaceClass=%d, bInterfaceSubClass=%d, bInterfaceProtocol=%d\n", altset, altptr->bInterfaceClass, altptr->bInterfaceSubClass, altptr->bInterfaceProtocol);

	  if (altptr->bInterfaceClass != LIBUSB_CLASS_PRINTER || altptr->bInterfaceSubClass != 1)
	    continue;

	  if (altptr->bInterfaceProtocol != 1 && altptr->bInterfaceProtocol != 2)
	    continue;

	  if (altptr->bInterfaceProtocol < device->protocol || altptr->bInterfaceProtocol > 2)
	    continue;

	  read_endp  = 0xff;
	  write_endp = 0xff;

	  for (endp = 0, endpptr = altptr->endpoint; endp < altptr->bNumEndpoints; endp ++, endpptr ++)
	  {
	    if ((endpptr->bmAttributes & LIBUSB_TRANSFER_TYPE_MASK) == LIBUSB_TRANSFER_TYPE_BULK)
	    {
	      if (endpptr->bEndpointAddress & LIBUSB_ENDPOINT_DIR_MASK)
		read_endp = endp;
	      else
		write_endp = endp;
	    }
	  }

	  if (write_endp != 0xff)
	  {
	    // Save the best match so far...
	    device->protocol   = altptr->bInterfaceProtocol;
	    device->altset     = altptr->bAlternateSetting;
	    device->ifacenum   = altptr->bInterfaceNumber;
	    device->write_endp = write_endp;
	    if (device->protocol > 1)
	      device->read_endp = read_endp;
	  }
	}

        _PAPPL_DEBUG("pappl_usb_find:             device->protocol=%d\n", device->protocol);

	if (device->protocol > 0)
	{
	  device->conf  = conf;
	  device->iface = iface;

	  if (!libusb_open(udevice, &device->handle))
	  {
	    uint8_t	current;	// Current configuration

	    // Opened the device, try to set the configuration...
	    if (libusb_control_transfer(device->handle, LIBUSB_REQUEST_TYPE_STANDARD | LIBUSB_ENDPOINT_IN | LIBUSB_RECIPIENT_DEVICE, 8, /* GET_CONFIGURATION */ 0, 0, (unsigned char *)&current, 1, 5000) < 0)
	      current = 0;

            if (confptr->bConfigurationValue != current)
            {
              // Select the configuration we want...
              if (libusb_set_configuration(device->handle, confptr->bConfigurationValue) < 0)
              {
                libusb_close(device->handle);
                device->handle = NULL;
              }
            }

#ifdef __linux
            if (device->handle)
            {
	      // Make sure the old, busted usblp kernel driver is not loaded...
	      if (libusb_kernel_driver_active(device->handle, device->iface) == 1)
	      {
		if ((err = libusb_detach_kernel_driver(device->handle, device->iface)) < 0 && err != LIBUSB_ERROR_NOT_FOUND)
		{
		  _papplDeviceError(err_cb, err_data, "Unable to detach usblp kernel driver for USB printer %04x:%04x: %s", devdesc.idVendor, devdesc.idProduct, libusb_strerror((enum libusb_error)err));
		  libusb_close(device->handle);
		  device->handle = NULL;
		}
	      }
	    }
#endif // __linux

            if (device->handle)
            {
              // Claim the interface...
              if ((err = libusb_claim_interface(device->handle, device->ifacenum)) < 0)
              {
		_papplDeviceError(err_cb, err_data, "Unable to claim USB interface: %s", libusb_strerror((enum libusb_error)err));
                libusb_close(device->handle);
                device->handle = NULL;
              }
            }

            if (device->handle && ifaceptr->num_altsetting > 1)
            {
              // Set the alternate setting as needed...
              if ((err = libusb_set_interface_alt_setting(device->handle, device->ifacenum, device->altset)) < 0)
              {
		_papplDeviceError(err_cb, err_data, "Unable to set alternate USB interface: %s", libusb_strerror((enum libusb_error)err));
                libusb_close(device->handle);
                device->handle = NULL;
              }
            }

            if (device->handle)
            {
              // Get the 1284 Device ID...
              if ((err = libusb_control_transfer(device->handle, LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_ENDPOINT_IN | LIBUSB_RECIPIENT_INTERFACE, 0, (uint16_t)device->conf, (uint16_t)((device->iface << 8) | device->altset), (unsigned char *)device_id, sizeof(device_id), 5000)) < 0)
              {
		_papplDeviceError(err_cb, err_data, "Unable to get IEEE-1284 device ID: %s", libusb_strerror((enum libusb_error)err));
                device_id[0] = '\0';
                libusb_close(device->handle);
                device->handle = NULL;
              }
              else
              {
                int length = ((device_id[0] & 255) << 8) | (device_id[1] & 255);

                if (length < 14 || length > (int)sizeof(device_id))
                  length = ((device_id[1] & 255) << 8) | (device_id[0] & 255);

                if (length > (int)(sizeof(device_id) - 2))
                  length = (int)(sizeof(device_id) - 2);
                else if (length < 2)
                  length = 0;
                else
                  length -= 2;

                if (length > 0)
                  memmove(device_id, device_id + 2, (size_t)length);
                device_id[length] = '\0';

                _PAPPL_DEBUG("pappl_usb_find:     device_id=\"%s\"\n", device_id);
              }
            }

            if (device->handle)
            {
              // Build the device URI...
              const char *make,		// Pointer to make
			*model,		// Pointer to model
			*serial = NULL;	// Pointer to serial number
	      char	*ptr,		// Pointer into device ID
			copy_did[1024],	// Copy of device ID
			temp[256];	// Temporary string for serial #

	      papplCopyString(copy_did, device_id, sizeof(copy_did));

              if ((make = strstr(copy_did, "MANUFACTURER:")) != NULL)
                make += 13;
              else if ((make = strstr(copy_did, "MFG:")) != NULL)
                make += 4;

              if ((model = strstr(copy_did, "MODEL:")) != NULL)
                model += 6;
              else if ((model = strstr(copy_did, "MDL:")) != NULL)
                model += 4;

              if ((serial = strstr(copy_did, "SERIALNUMBER:")) != NULL)
                serial += 12;
              else if ((serial = strstr(copy_did, "SERN:")) != NULL)
                serial += 5;
              else if ((serial = strstr(copy_did, "SN:")) != NULL)
                serial += 3;

              if (serial)
              {
                if ((ptr = strchr(serial, ';')) != NULL)
                  *ptr = '\0';
              }
              else
              {
                int length = libusb_get_string_descriptor_ascii(device->handle, devdesc.iSerialNumber, (unsigned char *)temp, sizeof(temp) - 1);
                if (length > 0)
                {
                  temp[length] = '\0';
                  serial       = temp;
                }
              }

              if (make)
              {
                if ((ptr = strchr(make, ';')) != NULL)
                  *ptr = '\0';
              }
              else
                make = "Unknown";

              if (model)
              {
                if ((ptr = strchr(model, ';')) != NULL)
                  *ptr = '\0';
              }
              else
                model = "Unknown";

              if (serial)
                httpAssembleURIf(HTTP_URI_CODING_ALL, device_uri, sizeof(device_uri), "usb", NULL, make, 0, "/%s?serial=%s", model, serial);
              else
                httpAssembleURIf(HTTP_URI_CODING_ALL, device_uri, sizeof(device_uri), "usb", NULL, make, 0, "/%s", model);

	      if (!strcmp(make, "HP") && !strncmp(model, "HP ", 3))
	        snprintf(device_info, sizeof(device_info), "%s (USB)", model);
	      else
	        snprintf(device_info, sizeof(device_info), "%s %s (USB)", make, model);

              if ((*cb)(device_info, device_uri, device_id, data))
              {
                _PAPPL_DEBUG("pappl_usb_find:     Found a match.\n");

		libusb_ref_device(device->device);

		if (device->read_endp != -1)
		  device->read_endp = confptr->interface[device->iface].altsetting[device->altset].endpoint[device->read_endp].bEndpointAddress;

		if (device->write_endp != -1)
		  device->write_endp = confptr->interface[device->iface].altsetting[device->altset].endpoint[device->write_endp].bEndpointAddress;

                goto match_found;
              }

	      libusb_close(device->handle);
	      device->handle = NULL;
            }
	  }
	}
      } // iface loop

      libusb_free_config_descriptor(confptr);
    } // conf loop
  }

  match_found:

  _PAPPL_DEBUG("pappl_usb_find: device->handle=%p\n", device->handle);

  // Clean up ....
  if (num_udevs >= 0)
    libusb_free_device_list(udevs, 1);

  return (device->handle != NULL);
}


//
// 'pappl_usb_getid()' - Get the current IEEE-1284 device ID.
//

static char *				// O - Device ID or `NULL` on error
pappl_usb_getid(
    pappl_device_t *device,		// I - Device
    char           *buffer,		// I - Buffer
    size_t         bufsize)		// I - Size of buffer
{
  _pappl_usb_dev_t	*usb = (_pappl_usb_dev_t *)papplDeviceGetData(device);
					// USB device data
  size_t		length;		// Length of device ID
  int			error;		// USB transfer error


  _PAPPL_DEBUG("pappl_usb_getid(device=%p, buffer=%p, bufsize=%ld) usb->conf=%d, ->iface=%d, ->altset=%d\n", device, buffer, (long)bufsize, usb->conf, usb->iface, usb->altset);

  // Get the 1284 Device ID...
  if ((error = libusb_control_transfer(usb->handle, LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_ENDPOINT_IN | LIBUSB_RECIPIENT_INTERFACE, 0, (uint16_t)usb->conf, (uint16_t)((usb->iface << 8) | usb->altset), (unsigned char *)buffer, (uint16_t)bufsize, 5000)) < 0)
  {
    papplDeviceError(device, "Unable to get IEEE-1284 device ID from USB port: %s", libusb_strerror((enum libusb_error)error));
    buffer[0] = '\0';
    return (NULL);
  }

  // Nul-terminate
  length = (size_t)(((buffer[0] & 255) << 8) | (buffer[1] & 255));
  if (length < 14 || length > bufsize)	// Some printers do it wrong (LSB)...
    length = (size_t)(((buffer[1] & 255) << 8) | (buffer[0] & 255));

  if (length > bufsize)
    length = bufsize;

  length -= 2;
  memmove(buffer, buffer + 2, length);
  buffer[length] = '\0';

  return (buffer);
}


//
// 'pappl_usb_list()' - List USB devices.
//

static bool				// O - `true` if found, `false` if not
pappl_usb_list(
    pappl_device_cb_t   cb,		// I - Callback function
    void                *data,		// I - User data pointer
    pappl_deverror_cb_t err_cb,		// I - Error callback
    void                *err_data)	// I - Error callback data
{
  _pappl_usb_dev_t	usb;		// USB device
  bool			ret;		// Return value


  ret = pappl_usb_find(cb, data, &usb, err_cb, err_data);

  if (usb.handle)
  {
    libusb_close(usb.handle);
    libusb_unref_device(usb.device);
  }

  return (ret);
}


//
// 'pappl_usb_open()' - Open a USB device.
//

static bool				// `true` on success, `false` on error
pappl_usb_open(
    pappl_device_t *device,		// I - Device
    const char     *device_uri,		// I - Device URI
    const char     *job_name)		// I - Job name (unused)
{
  _pappl_usb_dev_t	*usb;		// USB device


  (void)job_name;

  if ((usb = (_pappl_usb_dev_t *)calloc(1, sizeof(_pappl_usb_dev_t))) == NULL)
  {
    papplDeviceError(device, "Unable to allocate memory for USB device: %s", strerror(errno));
    return (false);
  }

  if (!pappl_usb_find(pappl_usb_open_cb, (void *)device_uri, usb, device->error_cb, device->error_data))
  {
    free(usb);
    return (false);
  }

  papplDeviceSetData(device, usb);

  return (true);
}


//
// 'pappl_usb_open_cb()' - Look for a matching device URI.
//

static bool				// O - `true` on match, `false` otherwise
pappl_usb_open_cb(
    const char *device_info,		// I - Description of device
    const char *device_uri,		// I - This device's URI
    const char *device_id,		// I - IEEE-1284 Device ID
    void       *data)			// I - URI we are looking for
{
  bool match = !strcmp(device_uri, (const char *)data);
					// Does this match?


  (void)device_info;
  (void)device_id;

  _PAPPL_DEBUG("pappl_usb_open_cb(device_info=\"%s\", device_uri=\"%s\", device_id=\"%s\", user_data=\"%s\") returning %s.\n", device_info, device_uri, device_id, (char *)data, match ? "true" : "false");

  return (match);
}


//
// 'pappl_usb_read()' - Read data from a USB device.
//

static ssize_t				// O - Bytes read
pappl_usb_read(pappl_device_t *device,	// I - Device
               void           *buffer,	// I - Read buffer
               size_t         bytes)	// I - Bytes to read
{
  _pappl_usb_dev_t	*usb = (_pappl_usb_dev_t *)papplDeviceGetData(device);
					// USB device data
  int			icount;		// Bytes that were read
  int			error;		// USB transfer error


  if (usb->read_endp < 0)
    return (-1);			// No read endpoint!

  if ((error = libusb_bulk_transfer(usb->handle, (unsigned char)usb->read_endp, buffer, (int)bytes, &icount, 10000)) < 0)
  {
    papplDeviceError(device, "Unable to read from USB port: %s",  libusb_strerror((enum libusb_error)error));
    return (-1);
  }
  else
    return ((ssize_t)icount);
}


//
// 'pappl_usb_status()' - Get the USB printer status.
//

static pappl_preason_t			// O - IPP "printer-state-reasons" values
pappl_usb_status(pappl_device_t *device)// I - Device
{
  _pappl_usb_dev_t	*usb = (_pappl_usb_dev_t *)papplDeviceGetData(device);
					// USB device data
  pappl_preason_t	status = PAPPL_PREASON_NONE;
					// IPP "printer-state-reasons" values
  unsigned char		port_status = 0x08;
					// Centronics port status byte
  int			error;		// USB transfer error


  if ((error = libusb_control_transfer(usb->handle, LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_ENDPOINT_IN | LIBUSB_RECIPIENT_INTERFACE, 1, 0, (uint16_t)(usb->iface << 8), &port_status, 1, 0)) < 0)
  {
    papplDeviceError(device, "Unable to get USB port status: %s",  libusb_strerror((enum libusb_error)error));
  }
  else
  {
    if (!(port_status & 0x08))
      status |= PAPPL_PREASON_OTHER;
    if (port_status & 0x20)
      status |= PAPPL_PREASON_MEDIA_EMPTY;
    if (port_status & 0x40)		// Vendor extension
      status |= PAPPL_PREASON_MEDIA_JAM;
    if (port_status & 0x80)		// Vendor extension
      status |= PAPPL_PREASON_COVER_OPEN;
  }

  return (status);
}


//
// 'pappl_usb_write()' - Write data to a USB device.
//

static ssize_t				// O - Bytes written
pappl_usb_write(pappl_device_t *device,	// I - Device
                const void     *buffer,	// I - Write buffer
                size_t         bytes)	// I - Bytes to write
{
  _pappl_usb_dev_t	*usb = (_pappl_usb_dev_t *)papplDeviceGetData(device);
					// USB device data
  int			icount;		// Bytes that were written
  int			error;		// USB transfer error


  if ((error = libusb_bulk_transfer(usb->handle, (unsigned char)usb->write_endp, (unsigned char *)buffer, (int)bytes, &icount, 0)) < 0)
  {
    papplDeviceError(device, "Unable to write %d bytes to USB port: %s", (int)bytes, libusb_strerror((enum libusb_error)error));
    return (-1);
  }
  else
    return ((ssize_t)icount);
}
#endif // HAVE_LIBUSB
