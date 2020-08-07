//
// Common device support code for the Printer Application Framework
//
// Copyright © 2019-2020 by Michael R Sweet.
// Copyright © 2007-2019 by Apple Inc.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

//
// Include necessary headers...
//

#include "dnssd-private.h"
#include "snmp-private.h"
#include "device.h"
#include "printer.h"
#include <ifaddrs.h>
#include <net/if.h>
#include <stdarg.h>
#ifdef HAVE_LIBUSB
#  include <libusb.h>
#endif // HAVE_LIBUSB


//
// Options...
//

#define PAPPL_DEVICE_DEBUG	0	// Define to 1 to enable debug output file


//
// Constants...
//

#define PAPPL_DEVICE_BUFSIZE	8192	// Size of write buffer


//
// Types...
//

struct _pappl_device_s			// Device connection data
{
  int			fd;			// File descriptor connection to device
#if PAPPL_DEVICE_DEBUG
  int			debug_fd;		// Debugging copy of data sent
#endif // PAPPL_DEVICE_DEBUG
#ifdef HAVE_LIBUSB
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
#endif // HAVE_LIBUSB
  char			*host;			// Hostname
  int			port;			// Port number
  char			buffer[PAPPL_DEVICE_BUFSIZE];
						// Write buffer
  size_t		bufused;		// Number of bytes in write buffer
  pappl_dmetrics_t	metrics;		// Device metrics
};

typedef struct _pappl_dns_sd_dev_t	// DNS-SD browse data
{
#ifdef HAVE_DNSSD
  DNSServiceRef		ref;			// Service reference for query
#endif // HAVE_DNSSD
#ifdef HAVE_AVAHI
  AvahiRecordBrowser	*ref;			// Browser for query
#endif // HAVE_AVAHI
  char			*name,			// Service name
			*domain,		// Domain name
			*fullName,		// Full name
			*make_and_model,	// Make and model from TXT record
			*device_id,		// 1284 device ID from TXT record
			*uuid;			// UUID from TXT record
} _pappl_dns_sd_dev_t;

typedef struct _pappl_snmp_dev_s	// SNMP browse data
{
  http_addr_t	address;			// Address of device
  char		*addrname,			// Name of device
		*uri,				// Device URI
		*device_id;			// IEEE-1284 device id
  int		port;				// Port number
} _pappl_snmp_dev_t;

typedef enum _pappl_snmp_query_e	// SNMP query request IDs for each field
{
  _PAPPL_SNMP_QUERY_DEVICE_TYPE = 0x01,		// Device type OID
  _PAPPL_SNMP_QUERY_DEVICE_ID,			// IEEE-1284 device ID OIDs
  _PAPPL_SNMP_QUERY_DEVICE_SYSNAME,		// sysName OID
  _PAPPL_SNMP_QUERY_DEVICE_PORT			// Raw socket port number OIDs
} _pappl_snmp_query_t;


//
// Local functions...
//

#if defined(HAVE_DNSSD) || defined(HAVE_AVAHI)
#  ifdef HAVE_DNSSD
static void 		pappl_dnssd_browse_cb(DNSServiceRef sdRef, DNSServiceFlags flags, uint32_t interfaceIndex, DNSServiceErrorType errorCode, const char *serviceName, const char *regtype, const char *replyDomain, void *context);
static void		pappl_dnssd_resolve_cb(DNSServiceRef sdRef, DNSServiceFlags flags, uint32_t interfaceIndex, DNSServiceErrorType errorCode, const char *fullname, const char *hosttarget, uint16_t port, uint16_t txtLen, const unsigned char *txtRecord, void *context);
#  else
static void		pappl_dnssd_browse_cb(AvahiServiceBrowser *browser, AvahiIfIndex interface, AvahiProtocol protocol, AvahiBrowserEvent event, const char *serviceName, const char *serviceType, const char *replyDomain, AvahiLookupResultFlags flags, void *context);
static void		pappl_dnssd_resolve_cb(AvahiServiceResolver *resolver, AvahiIfIndex interface, AvahiProtocol protocol, AvahiResolverEvent event, const char *name, const char *type, const char *domain, const char *host_name, const AvahiAddress *address, uint16_t port, AvahiStringList *txt, AvahiLookupResultFlags flags, void *context);
#  endif // HAVE_DNSSD
static int		pappl_dnssd_compare_devices(_pappl_dns_sd_dev_t *a, _pappl_dns_sd_dev_t *b);
static bool		pappl_dnssd_find(pappl_device_cb_t cb, void *data, pappl_deverr_cb_t err_cb, void *err_data);
static void		pappl_dnssd_free(_pappl_dns_sd_dev_t *d);
static _pappl_dns_sd_dev_t *pappl_dnssd_get_device(cups_array_t *devices, const char *serviceName, const char *replyDomain);
static void		pappl_dnssd_unescape(char *dst, const char *src, size_t dstsize);
#endif // HAVE_DNSSD || HAVE_AVAHI

static void		pappl_error(pappl_deverr_cb_t err_cb, void *err_data, const char *message, ...) _PAPPL_FORMAT(3,4);

static int		pappl_snmp_compare_devices(_pappl_snmp_dev_t *a, _pappl_snmp_dev_t *b);
static int		pappl_snmp_connect(http_addr_t *addr, int port);
static bool		pappl_snmp_find(pappl_device_cb_t cb, void *data, pappl_device_t *device, pappl_deverr_cb_t err_cb, void *err_data);
static void		pappl_snmp_free(_pappl_snmp_dev_t *d);
static http_addrlist_t	*pappl_snmp_get_interface_addresses(void);
static bool		pappl_snmp_open_cb(const char *device_uri, const char *device_id, void *data);
static void		pappl_snmp_read_response(cups_array_t *devices, int fd, pappl_deverr_cb_t err_cb, void *err_data);

#ifdef HAVE_LIBUSB
static bool		pappl_usb_find(pappl_device_cb_t cb, void *data, pappl_device_t *device, pappl_deverr_cb_t err_cb, void *err_data);
static bool		pappl_usb_open_cb(const char *device_uri, const char *device_id, void *data);
#endif // HAVE_LIBUSB

static ssize_t		pappl_write(pappl_device_t *device, const void *buffer, size_t bytes);


//
// 'papplDeviceClose()' - Close a device connection.
//

void
papplDeviceClose(
    pappl_device_t *device)		// I - Device to close
{
  if (device)
  {
    if (device->bufused > 0)
      pappl_write(device, device->buffer, device->bufused);

#if PAPPL_DEVICE_DEBUG
    if (device->debug_fd >= 0)
      close(device->debug_fd);
#endif // PAPPL_DEVICE_DEBUG

    if (device->fd >= 0)
    {
      close(device->fd);
    }
#ifdef HAVE_LIBUSB
    else if (device->handle)
    {
      libusb_close(device->handle);
      libusb_unref_device(device->device);
    }
#endif // HAVE_LIBUSB

    free(device->host);
    free(device);
  }
}


//
// 'papplDeviceFlush()' - Flush any buffered data to the device.
//

void
papplDeviceFlush(pappl_device_t *device)// I - Device
{
  if (device && device->bufused > 0)
    pappl_write(device, device->buffer, device->bufused);
}


//
// '()' - Get the device metrics.
//

pappl_dmetrics_t *			// O - Metrics data
papplDeviceGetMetrics(
    pappl_device_t   *device,		// I - Device
    pappl_dmetrics_t *metrics)		// I - Buffer for metrics data
{
  if (device && metrics)
    memcpy(metrics, &device->metrics, sizeof(pappl_dmetrics_t));
  else if (metrics)
    memset(metrics, 0, sizeof(pappl_dmetrics_t));

  return (metrics);
}


//
// 'papplDeviceGetDeviceStatus()' - Get the printer status bits.
//
// The status bits for USB devices come from the original Centronics parallel
// printer "standard" which was later formally standardized in IEEE 1284-1984
// and the USB Device Class Definition for Printing Devices.  Some vendor
// extentions are also supported.
//
// The status bits for socket devices come from the hrPrinterDetectedErrorState
// property that is defined in the SNMP Printer MIB v2 (RFC 3805).
//
// This function returns a @link pappl_preason_t@ bitfield which can be
// passed to the @link papplPrinterSetReasons@ function.  Use the
// @link PAPPL_PREASON_DEVICE_STATUS@ value as the value of the `remove`
// argument.
//
// This function can block for several seconds while getting the status
// information.
//

pappl_preason_t				// O - IPP "printer-state-reasons" values
papplDeviceGetStatus(
    pappl_device_t *device)		// I - Device
{
  pappl_preason_t	status = PAPPL_PREASON_NONE;
					// IPP "printer-state-reasons" values


  // TODO: Add SNMP calls
#ifdef HAVE_LIBUSB
  if (device->handle)
  {
    unsigned char port_status = 0x08;	// Centronics port status byte

    if (libusb_control_transfer(device->handle, LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_ENDPOINT_IN | LIBUSB_RECIPIENT_INTERFACE, 1, device->conf, (device->iface << 8) | device->altset, &port_status, 1, 5000) >= 0)
    {
      if (!(port_status & 0x08))
        status |= PAPPL_PREASON_OTHER;
      if (port_status & 0x20)
        status |= PAPPL_PREASON_MEDIA_EMPTY;
      if (port_status & 0x40)
        status |= PAPPL_PREASON_MEDIA_JAM;
      if (port_status & 0x80)
        status |= PAPPL_PREASON_COVER_OPEN;
    }
  }
#endif // HAVE_LIBUSB

  return (status);
}


//
// 'papplDeviceList()' - List available devices.
//

bool					// O - `true` if the callback returned `true`, `false` otherwise
papplDeviceList(
    pappl_dtype_t     types,		// I - Device types
    pappl_device_cb_t cb,		// I - Callback function
    void              *data,		// I - User data for callback
    pappl_deverr_cb_t err_cb,		// I - Error callback
    void              *err_data)	// I - Data for error callback
{
  bool			ret = false;	// Return value
  pappl_device_t	junk;		// Dummy device data


#ifdef HAVE_LIBUSB
  if (types & PAPPL_DTYPE_USB)
  {
    ret = pappl_usb_find(cb, data, &junk, err_cb, err_data);

    if (junk.handle)
    {
      libusb_close(junk.handle);
      libusb_unref_device(junk.device);
    }
  }
#endif // HAVE_LIBUSB

#if defined(HAVE_DNSSD) || defined(HAVE_AVAHI)
  if (!ret && (types & PAPPL_DTYPE_DNS_SD))
    ret = pappl_dnssd_find(cb, data, err_cb, err_data);
#endif // HAVE_DNSSD || HAVE_AVAHI

  if (!ret && (types & PAPPL_DTYPE_SNMP))
    ret = pappl_snmp_find(cb, data, &junk, err_cb, err_data);

  return (ret);
}


//
// 'papplDeviceOpen()' - Open a connection to a device.
//
// The "file", "snmp", "socket", and "usb" URI schemes are currently supported.
//

pappl_device_t	*			// O - Device connection or `NULL` on error
papplDeviceOpen(
    const char        *device_uri,	// I - Device URI
    pappl_deverr_cb_t err_cb,		// I - Error callback
    void              *err_data)	// I - Data for error callback
{
  pappl_device_t	*device;	// Device structure
  char			scheme[32],	// URI scheme
			userpass[32],	// Username/password (not used)
			host[256],	// Host name or make
			resource[256],	// Resource path, if any
			*options;	// Pointer to options, if any
  int			port;		// Port number
  http_uri_status_t	status;		// URI status


  if (!device_uri)
  {
    pappl_error(err_cb, err_data, "Bad NULL device URI.");
    return (NULL);
  }

  if ((status = httpSeparateURI(HTTP_URI_CODING_ALL, device_uri, scheme, sizeof(scheme), userpass, sizeof(userpass), host, sizeof(host), &port, resource, sizeof(resource))) < HTTP_URI_STATUS_OK)
  {
    pappl_error(err_cb, err_data, "Bad device URI '%s': %s", device_uri, httpURIStatusString(status));
    return (NULL);
  }

  if ((options = strchr(resource, '?')) != NULL)
    *options++ = '\0';

  if ((device = calloc(1, sizeof(pappl_device_t))) != NULL)
  {
#if PAPPL_DEVICE_DEBUG
    const char *pappl_device_debug = getenv("PAPPL_DEVICE_DEBUG");
#endif // PAPPL_DEVICE_DEBUG

    if (!strcmp(scheme, "file"))
    {
      // Character device file...
      if ((device->fd = open(resource, O_RDWR | O_EXCL)) < 0)
      {
        pappl_error(err_cb, err_data, "Unable to open '%s': %s", resource, strerror(errno));
        goto error;
      }
    }
    else if (!strcmp(scheme, "snmp"))
    {
      // SNMP discovered device
      if (!pappl_snmp_find(pappl_snmp_open_cb, (void *)device_uri, device, err_cb, err_data))
        goto error;
    }
    else if (!strcmp(scheme, "socket"))
    {
      // Raw socket (JetDirect or similar)
#if defined(HAVE_DNSSD) || defined(HAVE_AVAHI)
      int		i;		// Looping var
      char		name[256],	// Service name
			*type,		// Service type
			*domain;	// Domain
      _pappl_dns_sd_t	master;		// DNS-SD context
#  ifdef HAVE_DNSSD
      int		error;		// Error code, if any
      DNSServiceRef	resolver;	// Resolver
#  else
      AvahiServiceResolver *resolver;	// Resolver
#  endif // HAVE_DNSSD

      if ((domain = strstr(host, "._tcp.")) != NULL)
      {
        // Truncate host at domain portion...
        domain += 5;
        *domain++ = '\0';

        // Then separate the service type portion...
        type = strstr(host, "._");
        *type ++ = '\0';

        // Unescape the service name...
        pappl_dnssd_unescape(name, host, sizeof(name));

        master = _papplDNSSDInit(NULL);

#  ifdef HAVE_DNSSD
        resolver = master;
        if ((error = DNSServiceResolve(&resolver, kDNSServiceFlagsShareConnection, 0, name, type, domain, (DNSServiceResolveReply)pappl_dnssd_resolve_cb, device)) != kDNSServiceErr_NoError)
        {
          pappl_error(err_cb, err_data, "Unable to resolve '%s': %s", device_uri, _papplDNSSDStrError(error));
          goto error;
        }
#  else
        _papplDNSSDLock();

        if ((resolver = avahi_service_resolver_new(master, AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC, name, type, domain, AVAHI_PROTO_UNSPEC, 0, (AvahiServiceResolverCallback)pappl_dnssd_resolve_cb, device)) == NULL)
        {
          pappl_error(err_cb, err_data, "Unable to resolve '%s'.", device_uri);
          _papplDNSSDUnlock();
          goto error;
        }

        _papplDNSSDUnlock();
#  endif // HAVE_DNSSD

        // Wait up to 30 seconds for the resolve to complete...
        for (i = 0; i < 30000 && !device->host; i ++)
          usleep(1000);

#  ifdef HAVE_DNSSD
        DNSServiceRefDeallocate(resolver);
#  else
        _papplDNSSDLock();
        avahi_service_resolver_free(resolver);
        _papplDNSSDUnlock();
#  endif // HAVE_DNSSD

        if (!device->host)
        {
          pappl_error(err_cb, err_data, "Unable to resolve '%s'.", device_uri);
          goto error;
	}
      }
      else
#endif // HAVE_DNSSD || HAVE_AVAHI
      {
        // Save the hostname and port...
        device->host = strdup(host);
        device->port = port;
      }

      char		port_str[32];	// String for port number
      http_addrlist_t	*list;		// Address list

      snprintf(port_str, sizeof(port_str), "%d", device->port);
      if ((list = httpAddrGetList(device->host, AF_UNSPEC, port_str)) == NULL)
      {
        pappl_error(err_cb, err_data, "Unable to lookup '%s:%d': %s", device->host, device->port, cupsLastErrorString());
        goto error;
      }

      device->fd = -1;

      httpAddrConnect2(list, &device->fd, 30000, NULL);
      httpAddrFreeList(list);

      if (device->fd < 0)
      {
        pappl_error(err_cb, err_data, "Unable to connect to '%s:%d': %s", host, port, cupsLastErrorString());
        goto error;
      }

      _PAPPL_DEBUG("Connection successful, device fd = %d\n", device->fd);
    }
#ifdef HAVE_LIBUSB
    else if (!strcmp(scheme, "usb"))
    {
      // USB printer class device
      device->fd = -1;

      if (!pappl_usb_find(pappl_usb_open_cb, (void *)device_uri, device, err_cb, err_data))
        goto error;
    }
#endif // HAVE_LIBUSB
    else
    {
      pappl_error(err_cb, err_data, "Unsupported device URI scheme '%s'.", scheme);
      goto error;
    }

#if PAPPL_DEVICE_DEBUG
    if (pappl_device_debug)
      device->debug_fd = open(pappl_device_debug, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    else
      device->debug_fd = -1;
#endif // PAPPL_DEVICE_DEBUG
  }

  return (device);

  error:

  free(device->host);
  free(device);

  return (NULL);
}


//
// 'papplDeviceParse1284ID()' - Parse an IEEE-1284 device ID string.
//

int					// O - Number of key/value pairs
papplDeviceParse1284ID(
    const char    *device_id,		// I - IEEE-1284 device ID string
    cups_option_t **pairs)		// O - Key/value pairs
{
  int	num_pairs = 0;			// Number of key/value pairs
  char	name[256],			// Key name
	value[256],			// Value
	*ptr;				// Pointer into key/value


  // Range check input...
  if (pairs)
    *pairs = NULL;

  if (!device_id || !pairs)
    return (0);

  // Scan the IEEE-1284 device ID string...
  while (*device_id)
  {
    // Skip leading whitespace...
    while (*device_id && isspace(*device_id))
      device_id ++;

    if (!*device_id)
      break;

    // Get the key name...
    for (ptr = name; *device_id && *device_id != ':'; device_id ++)
    {
      if (ptr < (name + sizeof(name) - 1))
	*ptr++ = *device_id;
    }

    *ptr = '\0';

    if (*device_id != ':')
      break;

    device_id ++;

    // Skip leading whitespace in value...
    while (*device_id && isspace(*device_id))
      device_id ++;

    for (ptr = value; *device_id && *device_id != ';'; device_id ++)
    {
      if (ptr < (value + sizeof(value) - 1))
	*ptr++ = *device_id;
    }

    *ptr = '\0';

    if (*device_id == ';')
      device_id ++;

    num_pairs = cupsAddOption(name, value, num_pairs, pairs);
  }

  return (num_pairs);
}


//
// 'papplDevicePrintf()' - Write a formatted string.
//

ssize_t					// O - Number of characters or -1 on error
papplDevicePrintf(
    pappl_device_t *device,		// I - Device
    const char      *format,		// I - Printf-style format string
    ...)				// I - Additional args as needed
{
  va_list	ap;			// Pointer to additional args
  char		buffer[8192];		// Output buffer


  va_start(ap, format);
  vsnprintf(buffer, sizeof(buffer), format, ap);
  va_end(ap);

  return (papplDeviceWrite(device, buffer, strlen(buffer)));
}


//
// 'papplDevicePuts()' - Write a literal string.
//

ssize_t					// O - Number of characters or -1 on error
papplDevicePuts(
    pappl_device_t *device,		// I - Device
    const char      *s)			// I - Literal string
{
  return (papplDeviceWrite(device, s, strlen(s)));
}


//
// 'papplDeviceRead()' - Read from a device.
//

ssize_t					// O - Number of bytes read or -1 on error
papplDeviceRead(
    pappl_device_t *device,		// I - Device
    void           *buffer,		// I - Read buffer
    size_t         bytes)		// I - Max bytes to read
{
  struct timeval	starttime,	// Start time
			endtime;	// End time
  ssize_t		count;		// Bytes read this time


  if (!device)
    return (-1);

  // Make sure any pending IO is flushed...
  if (device->bufused > 0)
    papplDeviceFlush(device);

  gettimeofday(&starttime, NULL);

  if (device->fd >= 0)
  {
    while ((count = read(device->fd, buffer, bytes)) < 0)
      if (errno != EINTR && errno != EAGAIN)
        break;
  }
#ifdef HAVE_LIBUSB
  else if (device->handle)
  {
    int	icount;				// Bytes that were read

    if (libusb_bulk_transfer(device->handle, device->read_endp, buffer, (int)bytes, &icount, 0) < 0)
      count = -1;
    else
      count = (ssize_t)icount;
  }
#endif // HAVE_LIBUSB
  else
    count = -1;

  gettimeofday(&endtime, NULL);

  device->metrics.read_requests ++;
  device->metrics.read_msecs += 1000 * (endtime.tv_sec - starttime.tv_sec) + (endtime.tv_usec - starttime.tv_usec) / 1000;
  if (count > 0)
    device->metrics.read_bytes += (size_t)count;

  return (count);
}


//
// 'papplDeviceWrite()' - Write to a device.
//

ssize_t					// O - Number of bytes written or -1 on error
papplDeviceWrite(
    pappl_device_t *device,		// I - Device
    const void     *buffer,		// I - Write buffer
    size_t         bytes)		// I - Number of bytes to write
{
  if (!device)
    return (-1);

  if ((device->bufused + bytes) > sizeof(device->buffer))
  {
    // Flush the write buffer...
    if (pappl_write(device, device->buffer, device->bufused) < 0)
      return (-1);

    device->bufused = 0;
  }

  if (bytes < sizeof(device->buffer))
  {
    memcpy(device->buffer + device->bufused, buffer, bytes);
    device->bufused += bytes;
    return (bytes);
  }

  return (pappl_write(device, buffer, bytes));
}


#if defined(HAVE_DNSSD) || defined(HAVE_AVAHI)
#  ifdef HAVE_DNSSD
//
// 'pappl_dnssd_browse_cb()' - Browse for DNS-SD devices.
//

static void
pappl_dnssd_browse_cb(
    DNSServiceRef       sdRef,		// I - Service reference
    DNSServiceFlags     flags,		// I - Option flags
    uint32_t            interfaceIndex,	// I - Interface number
    DNSServiceErrorType errorCode,	// I - Error, if any
    const char          *serviceName,	// I - Name of service/device
    const char          *regtype,	// I - Registration type
    const char          *replyDomain,	// I - Service domain
    void                *context)	// I - Devices array
{
  _PAPPL_DEBUG("DEBUG: pappl_browse_cb(sdRef=%p, flags=%x, "
                  "interfaceIndex=%d, errorCode=%d, serviceName=\"%s\", "
		  "replyDomain=\"%s\", context=%p)\n",
          sdRef, flags, interfaceIndex, errorCode,
	  serviceName, replyDomain, context);

  // Only process "add" data...
  if (errorCode == kDNSServiceErr_NoError && (flags & kDNSServiceFlagsAdd))
  {
    // Get the device...
    pappl_dnssd_get_device((cups_array_t *)context, serviceName, replyDomain);
  }
}


#  else
//
// 'pappl_dnssd_browse_cb()' - Browse for DNS-SD devices.
//

static void
pappl_dnssd_browse_cb(
    AvahiServiceBrowser    *browser,	// I - Browser
    AvahiIfIndex           interface,	// I - Interface index
    AvahiProtocol          protocol,	// I - Network protocol
    AvahiBrowserEvent      event,	// I - What happened
    const char             *name,	// I - Service name
    const char             *type,	// I - Service type
    const char             *domain,	// I - Domain
    AvahiLookupResultFlags flags,	// I - Flags
    void                   *context)	// I - Devices array
{
  if (event == AVAHI_BROWSER_NEW)
    pappl_dnssd_get_device((cups_array_t *)context, name, domain);
}
#  endif // HAVE_DNSSD


//
// 'pappl_dnssd_compare_devices()' - Compare two DNS-SD devices.
//

static int				// O - Result of comparison
pappl_dnssd_compare_devices(
    _pappl_dns_sd_dev_t *a,		// I - First device
    _pappl_dns_sd_dev_t *b)		// I - Second device
{
  _PAPPL_DEBUG("pappl_dnssd_compare_devices(a=%p(%s), b=%p(%s))\n", a, a->name, b, b->name);

  return (strcmp(a->name, b->name));
}


//
// 'pappl_dnssd_find()' - Find printers using DNS-SD.
//

static bool				// O - `true` if the callback returned `true`, `false` otherwise
pappl_dnssd_find(
    pappl_device_cb_t cb,		// I - Callback function
    void              *data,		// I - User data for callback
    pappl_deverr_cb_t err_cb,		// I - Error callback
    void              *err_data)	// I - Data for error callback
{
  bool			ret = false;	// Return value
  cups_array_t		*devices;	// DNS-SD devices
  _pappl_dns_sd_dev_t	*device;	// Current DNS-SD device
  char			device_uri[1024];
					// Network device URI
  int			last_count,	// Last number of devices
			timeout;	// Timeout counter
#  ifdef HAVE_DNSSD
  int			error;		// Error code, if any
  DNSServiceRef		pdl_ref;	// Browse reference for _pdl-datastream._tcp
#  else
  AvahiServiceBrowser	*pdl_ref;	// Browse reference for _pdl-datastream._tcp
#  endif // HAVE_DNSSD


  devices = cupsArrayNew3((cups_array_func_t)pappl_dnssd_compare_devices, NULL, NULL, 0, NULL, (cups_afree_func_t)pappl_dnssd_free);
  _PAPPL_DEBUG("pappl_dnssd_find: devices=%p\n", devices);

  _papplDNSSDLock();

#  ifdef HAVE_DNSSD
  pdl_ref = _papplDNSSDInit(NULL);

  if ((error = DNSServiceBrowse(&pdl_ref, kDNSServiceFlagsShareConnection, 0, "_pdl-datastream._tcp", NULL, (DNSServiceBrowseReply)pappl_dnssd_browse_cb, devices)) != kDNSServiceErr_NoError)
  {
    pappl_error(err_cb, err_data, "Unable to create service browser: %s (%d).", _papplDNSSDStrError(error), error);
    cupsArrayDelete(devices);
    return (ret);
  }

#  else
  if ((pdl_ref = avahi_service_browser_new(_papplDNSSDInit(NULL), AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC, "_pdl-datastream._tcp", NULL, 0, pappl_dnssd_browse_cb, devices)) == NULL)
  {
    pappl_error(err_cb, err_data, "Unable to create service browser.");
    cupsArrayDelete(devices);
    return (ret);
  }
#  endif // HAVE_DNSSD

  _papplDNSSDUnlock();

  // Wait up to 10 seconds for us to find all available devices...
  for (timeout = 10000, last_count = 0; timeout > 0; timeout -= 250)
  {
    // 250000 microseconds == 250 milliseconds
    _PAPPL_DEBUG("pappl_dnssd_find: timeout=%d, last_count=%d\n", timeout, last_count);
    usleep(250000);

    if (last_count == cupsArrayCount(devices))
      break;

    last_count = cupsArrayCount(devices);
  }

  _PAPPL_DEBUG("pappl_dnssd_find: timeout=%d, last_count=%d\n", timeout, last_count);

  // Do the callback for each of the devices...
  for (device = (_pappl_dns_sd_dev_t *)cupsArrayFirst(devices); device; device = (_pappl_dns_sd_dev_t *)cupsArrayNext(devices))
  {
    if (device->uuid)
      httpAssembleURIf(HTTP_URI_CODING_ALL, device_uri, sizeof(device_uri), "socket", NULL, device->fullName, 0, "/?uuid=%s", device->uuid);
    else
      httpAssembleURI(HTTP_URI_CODING_ALL, device_uri, sizeof(device_uri), "socket", NULL, device->fullName, 0, "/");

    if ((*cb)(device_uri, device->device_id, data))
    {
      ret = true;
      break;
    }
  }

  // Stop browsing and free memory...
  _papplDNSSDLock();

#  ifdef HAVE_DNSSD
  DNSServiceRefDeallocate(pdl_ref);
#  else
  avahi_service_browser_free(pdl_ref);
#  endif // HAVE_DNSSD

  _papplDNSSDUnlock();

  cupsArrayDelete(devices);

  return (ret);
}


//
// 'pappl_dnssd_free()' - Free the memory used for a DNS-SD device.
//

static void
pappl_dnssd_free(_pappl_dns_sd_dev_t *d)// I - Device
{
  // Free all memory...
  free(d->name);
  free(d->domain);
  free(d->fullName);
  free(d->make_and_model);
  free(d->device_id);
  free(d->uuid);
  free(d);
}


//
// 'pappl_dnssd_get_device()' - Create or update a DNS-SD device.
//

static _pappl_dns_sd_dev_t *		// O - Device
pappl_dnssd_get_device(
    cups_array_t *devices,		// I - Device array
    const char   *serviceName,		// I - Name of service/device
    const char   *replyDomain)		// I - Service domain
{
  _pappl_dns_sd_dev_t	key,		// Search key
			*device;	// Device
  char			fullName[1024];	// Full name for query


  _PAPPL_DEBUG("pappl_dnssd_get_device(devices=%p, serviceName=\"%s\", replyDomain=\"%s\")\n", devices, serviceName, replyDomain);

  // See if this is a new device...
  key.name = (char *)serviceName;

  if ((device = cupsArrayFind(devices, &key)) != NULL)
  {
    // Nope, see if this is for a different domain...
    if (!strcasecmp(device->domain, "local.") && strcasecmp(device->domain, replyDomain))
    {
      // Update the .local listing to use the "global" domain name instead.
      free(device->domain);
      device->domain = strdup(replyDomain);

#  ifdef HAVE_DNSSD
      DNSServiceConstructFullName(fullName, device->name, "_pdl-datastream._tcp.", replyDomain);
#  else
      avahi_service_name_join(fullName, sizeof(fullName), serviceName, "_pdl-datastream._tcp.", replyDomain);
#  endif // HAVE_DNSSD

      free(device->fullName);
      device->fullName = strdup(fullName);
    }

    return (device);
  }

  // Yes, add the device...
  device         = calloc(sizeof(_pappl_dns_sd_dev_t), 1);
  device->name   = strdup(serviceName);
  device->domain = strdup(replyDomain);

  cupsArrayAdd(devices, device);

  // Set the "full name" of this service, which is used for queries...
#  ifdef HAVE_DNSSD
  DNSServiceConstructFullName(fullName, serviceName, "_pdl-datastream._tcp.", replyDomain);
#  else
  avahi_service_name_join(fullName, sizeof(fullName), serviceName, "_pdl-datastream._tcp.", replyDomain);
#  endif /* HAVE_DNSSD */

  device->fullName = strdup(fullName);

  return (device);
}


//
// 'pappl_dnssd_resolve_cb()' - Resolve a DNS-SD service.
//

#  ifdef HAVE_DNSSD
static void
pappl_dnssd_resolve_cb(
    DNSServiceRef       sdRef,		// I - Service reference
    DNSServiceFlags     flags,		// I - Option flags
    uint32_t            interfaceIndex,	// I - Interface number
    DNSServiceErrorType errorCode,	// I - Error, if any
    const char          *fullname,	// I - Full service domain name
    const char          *host_name,	// I - Host name
    uint16_t            port,		// I - Port number
    uint16_t            txtLen,		// I - TXT record len
    const unsigned char *txtRecord,	// I - TXT record
    void                *context)	// I - Device
{
  if (errorCode == kDNSServiceErr_NoError && (flags & kDNSServiceFlagsAdd))
  {
    pappl_device_t *device = (pappl_device_t *)context;
					// Device

    device->host = strdup(host_name);
    device->port = port;
  }
}

#  else
//
// 'pappl_dnssd_resolve_cb()' - Resolve a DNS-SD service.
//

static void
pappl_dnssd_resolve_cb(
    AvahiServiceResolver   *resolver,	// I - Service resolver
    AvahiIfIndex           interface,	// I - Interface number
    AvahiProtocol          protocol,	// I - Network protocol
    AvahiResolverEvent     event,	// I - What happened
    const char             *name,	// I - Service name
    const char             *type,	// I - Service type
    const char             *domain,	// I - Domain
    const char             *host_name,	// I - Host name
    const AvahiAddress     *address,	// I - Address
    uint16_t               port,	// I - Port number
    AvahiStringList        *txt,	// I - TXT record
    AvahiLookupResultFlags flags,	// I - Flags
    void                   *context)	// I - Device
{
  if (!resolver)
    return;

  if (event == AVAHI_RESOLVER_FOUND)
  {
    pappl_device_t *device = (pappl_device_t *)context;
					// Device

    device->host = strdup(host_name);
    device->port = port;
  }
}
#  endif // HAVE_DNSSD


//
// 'pappl_dnssd_unescape()' - Unescape a service name.
//

static void
pappl_dnssd_unescape(
    char       *dst,			// I - Destination buffer
    const char *src,			// I - Source string
    size_t     dstsize)			// I - Size of destination buffer
{
  char	*dstend = dst + dstsize - 1;	// End of destination buffer


  while (*src && dst < dstend)
  {
    if (*src == '\\')
    {
      src ++;
      if (isdigit(src[0] & 255) && isdigit(src[1] & 255) &&
          isdigit(src[2] & 255))
      {
        *dst++ = ((((src[0] - '0') * 10) + src[1] - '0') * 10) + src[2] - '0';
	src += 3;
      }
      else
        *dst++ = *src++;
    }
    else
      *dst++ = *src ++;
  }

  *dst = '\0';
}
#endif // HAVE_DNSSD || HAVE_AVAHI


//
// 'pappl_error()' - Report an error.
//

static void
pappl_error(
    pappl_deverr_cb_t err_cb,		// I - Error callback
    void              *err_data,	// I - Error callback data
    const char        *message,		// I - Printf-style message
    ...)				// I - Additional args as needed
{
  va_list	ap;			// Pointer to additional args
  char		buffer[8192];		// Formatted message


  if (!err_cb)
    return;

  va_start(ap, message);
  vsnprintf(buffer, sizeof(buffer), message, ap);
  va_end(ap);

  (*err_cb)(buffer, err_data);
}


//
// 'pappl_snmp_compare_devices()' - Compare two SNMP devices.
//

static int				// O - Result of comparison
pappl_snmp_compare_devices(
    _pappl_snmp_dev_t *a,		// I - First device
    _pappl_snmp_dev_t *b)		// I - Second device
{
  int	ret = strcmp(a->addrname, b->addrname);
					// Return value


  _PAPPL_DEBUG("pappl_snmp_compare_devices(a=%p(%s), b=%p(%s)) = %d\n", a, a->addrname, b, b->addrname, ret);

  return (ret);
}


//
// 'pappl_snmp_connect()' - Connect to an address.
//

static int				// O - File descriptor or `-1` on error
pappl_snmp_connect(
    http_addr_t *addr,			// I - Device address
    int         port)			// I - Raw socket port
{
  int             fd = -1;		// File descriptor
  http_addrlist_t addrlist;		// Dummy address list


  // Initialize the address and list...
  addr->ipv4.sin_port = htons(port);
  addrlist.addr       = *addr;
  addrlist.next       = NULL;

  // Connect...
  httpAddrConnect(&addrlist, &fd);

  return (fd);
}


//
// 'pappl_snmp_find()' - Find an SNMP device.
//

static bool				// O - `true` if found, `false` if not
pappl_snmp_find(
    pappl_device_cb_t cb,		// I - Callback function
    void              *data,		// I - User data pointer
    pappl_device_t    *device,		// O - Device info
    pappl_deverr_cb_t err_cb,		// I - Error callback
    void              *err_data)	// I - Error callback data
{
  bool			ret = false;	// Return value
  cups_array_t		*devices = NULL;//  Device array
  int			snmp_sock = -1,	// SNMP socket
			last_count;	// Last devices count
  fd_set		input;		// Input set for select()
  struct timeval	timeout;	// Timeout for select()
  time_t		endtime;	// End time for scan
  http_addrlist_t	*addrs,		// List of addresses
			*addr;		// Current address
  _pappl_snmp_dev_t	*cur_device;	// Current device
#ifdef DEBUG
  char			temp[1024];	// Temporary address string
#endif // DEBUG
  static const int	DeviceTypeOID[] =
  {					 // Device Type OID
    1,3,6,1,2,1,25,3,2,1,2,1,-1
  };


  // Create an array to track SNMP devices...
  devices = cupsArrayNew3((cups_array_func_t)pappl_snmp_compare_devices, NULL, NULL, 0, NULL, (cups_afree_func_t)pappl_snmp_free);

  // Open SNMP socket...
  if ((snmp_sock = _papplSNMPOpen(AF_INET)) < 0)
  {
    pappl_error(err_cb, err_data, "Unable to open SNMP socket.");
    goto finished;
  }

  // Get the list of network interface broadcast addresses...
  if ((addrs = pappl_snmp_get_interface_addresses()) == NULL)
  {
    pappl_error(err_cb, err_data, "Unable to get SNMP broadcast addresses.");
    goto finished;
  }

  // Send queries to every broadcast address...
  for (addr = addrs; addr; addr = addr->next)
  {
    _PAPPL_DEBUG("pappl_snmp_find: Sending SNMP device type get request to '%s'.\n", httpAddrString(&(addr->addr), temp, sizeof(temp)));

    _papplSNMPWrite(snmp_sock, &(addr->addr), _PAPPL_SNMP_VERSION_1, _PAPPL_SNMP_COMMUNITY, _PAPPL_ASN1_GET_REQUEST, _PAPPL_SNMP_QUERY_DEVICE_TYPE, DeviceTypeOID);
  }

  // Free broadcast addresses (all done with them...)
  httpAddrFreeList(addrs);

  // Wait up to 30 seconds to discover printers via SNMP...
  FD_ZERO(&input);

  for (endtime = time(NULL) + 30, last_count = 0; time(NULL) < endtime;)
  {
    // Wait up to 2 seconds for more data...
    timeout.tv_sec  = 2;
    timeout.tv_usec = 0;

    FD_SET(snmp_sock, &input);

    _PAPPL_DEBUG("Running select() for %d.\n", snmp_sock);
    if (select(snmp_sock + 1, &input, NULL, NULL, &timeout) < 0)
    {
      pappl_error(err_cb, err_data, "SNMP select() failed with error: %s", strerror(errno));
      break;
    }

    if (FD_ISSET(snmp_sock, &input))
    {
      _PAPPL_DEBUG("pappl_snmp_find: Reading SNMP response.\n");
      pappl_snmp_read_response(devices, snmp_sock, err_cb, err_data);
    }
    else
    {
      if (last_count == cupsArrayCount(devices))
        break;

      last_count = cupsArrayCount(devices);
      _PAPPL_DEBUG("pappl_snmp_find: timeout=%d, last_count = %d\n", (int)(endtime - time(NULL)), last_count);
    }
  }

  _PAPPL_DEBUG("pappl_snmp_find: timeout=%d, last_count = %d\n", (int)(endtime - time(NULL)), last_count);

  // Report all of the devices we found...
  for (cur_device = (_pappl_snmp_dev_t *)cupsArrayFirst(devices); cur_device; cur_device = (_pappl_snmp_dev_t *)cupsArrayNext(devices))
  {
    // Skip LPD (port 515) and IPP (port 631) since they can't be raw sockets...
    if (cur_device->port == 515 || cur_device->port == 631)
      continue;

    if ((*cb)(cur_device->uri, cur_device->device_id, data))
    {
      // Connect to this printer if the callback says to...
      if ((device->fd = pappl_snmp_connect(&(cur_device->address), cur_device->port)) != -1)
      {
        device->host = strdup(cur_device->uri + 7);
        device->port = cur_device->port;
        ret          = true;
      }
      break;
    }
  }

  // Clean up and return...
  finished:

  cupsArrayDelete(devices);

  _papplSNMPClose(snmp_sock);

  return (ret);
}


//
// 'pappl_snmp_free()' - Free the memory used for SNMP device.
//

static void
pappl_snmp_free(_pappl_snmp_dev_t *d)	// I - SNMP device
{
  // Free all memory...
  free(d->addrname);
  free(d->device_id);
  free(d->uri);
  free(d);
}


//
// 'pappl_snmp_get_interface_addresses()' - Get interface broadcast addresses.
//

static http_addrlist_t *		// O - List of addresses
pappl_snmp_get_interface_addresses(void)
{
  struct ifaddrs	*addrs,		// Interface address list
			*addr;		// Current interface address
  http_addrlist_t	*first,		// First address in list
			*last,		// Last address in list
			*current;	// Current address


  // Get a list of network interfaces...
  if (getifaddrs(&addrs) < 0)
  {
    // Unable to get the list...
    return (NULL);
  }

  // Copy the broadcast addresses into a list of addresses...
  for (addr = addrs, first = NULL, last = NULL; addr; addr = addr->ifa_next)
  {
    if ((addr->ifa_flags & IFF_BROADCAST) && addr->ifa_broadaddr && addr->ifa_broadaddr->sa_family == AF_INET)
    {
      // Copy this IPv4 broadcast address...
      current = calloc(1, sizeof(http_addrlist_t));

      memcpy(&(current->addr), addr->ifa_broadaddr, sizeof(struct sockaddr_in));

      if (!last)
        first = current;
      else
        last->next = current;

      last = current;
    }
  }

  // Free the original interface addresses and return...
  freeifaddrs(addrs);

  return (first);
}


//
// 'pappl_snmp_open_cb()' - Look for a matching device URI.
//

static bool				// O - `true` on match, `false` otherwise
pappl_snmp_open_cb(
    const char *device_uri,		// I - This device's URI
    const char *device_id,		// I - IEEE-1284 Device ID
    void       *data)			// I - URI we are looking for
{
  bool match = !strcmp(device_uri, (const char *)data);
					// Does this match?

  _PAPPL_DEBUG("pappl_snmp_open_cb(device_uri=\"%s\", device_id=\"%s\", user_data=\"%s\") = %s\n", device_uri, device_id, (char *)data, match ? "true" : "false");

  return (match);
}


//
// 'pappl_snmp_read_response()' - Read and parse a SNMP response.
//

static void
pappl_snmp_read_response(
    cups_array_t      *devices,		// Devices array
    int               fd,		// I - SNMP socket file descriptor
    pappl_deverr_cb_t err_cb,		// I - Error callback
    void              *err_data)	// I - Data for error callback
{
  int			i;		// Looping variable
  _pappl_snmp_t		packet;		// Decoded packet
  _pappl_snmp_dev_t	*device,	// Matching device
			*temp;		// New device entry
  char			addrname[256];	// Source address name
  static const int	DevicePrinterOID[] = { 1,3,6,1,2,1,25,3,1,5,-1 };
					// Host MIB OID for "printer" type
  static const int	SysNameOID[] = { 1,3,6,1,2,1,1,5,0,-1 };
					// Host MIB sysName OID
  static const int	HPDeviceIDOID[] = { 1,3,6,1,4,1,11,2,3,9,1,1,7,0,-1 };
					// HP MIB IEEE-1284 Device ID OID
  static const int	LexmarkDeviceIdOID[] = { 1,3,6,1,4,1,641,2,1,2,1,3,1,-1 };
					// Lexmark MIB IEEE-1284 Device ID OID
  static const int	LexmarkPortOID[] = { 1,3,6,1,4,1,641,1,5,7,11,0,-1 };
					// Lexmark MIB raw socket port number OID
  static const int	ZebraDeviceIDOID[] = { 1,3,6,1,4,1,10642,1,3,0,-1 };
					// Zebra MIB IEEE-1284 Device ID OID
  static const int	ZebraPortOID[] = { 1,3,6,1,4,1,10642,20,10,20,15,2,1,10,1,-1 };
					// Zebra MIB raw socket port number OID
  static const int	PWGPPMDeviceIdOID[] = { 1,3,6,1,4,1,2699,1,2,1,2,1,1,3,1,-1 };
					// PWG Printer Port Monitor MIB IEEE-1284 Device ID OID
  static const int	PWGPPMPortOID[] = { 1,3,6,1,4,1,2699,1,2,1,3,1,1,6,1,1,-1 };
					// PWG Printer Port Monitor MIB raw socket port number OID
  static const int	RawTCPPortOID[] = { 1,3,6,1,4,1,683,6,3,1,4,17,0,-1 };
					// Extended Networks MIB (common) raw socket port number OID


  // Read the response data
  if (!_papplSNMPRead(fd, &packet, -1.0))
  {
    pappl_error(err_cb, err_data, "Unable to read SNMP response data: %s", strerror(errno));
    return;
  }

  httpAddrString(&(packet.address), addrname, sizeof(addrname));

  // Look for the response status code in the SNMP message header
  if (packet.error)
  {
    pappl_error(err_cb, err_data, "Bad SNMP packet from '%s': %s", addrname, packet.error);
    return;
  }

  _PAPPL_DEBUG("pappl_snmp_read_response: community=\"%s\"\n", packet.community);
  _PAPPL_DEBUG("pappl_snmp_read_response: request-id=%d\n", packet.request_id);
  _PAPPL_DEBUG("pappl_snmp_read_response: error-status=%d\n", packet.error_status);

  if (packet.error_status && packet.request_id != _PAPPL_SNMP_QUERY_DEVICE_TYPE)
    return;

  // Find a matching device in the cache
  for (device = (_pappl_snmp_dev_t *)cupsArrayFirst(devices); device; device = (_pappl_snmp_dev_t *)cupsArrayNext(devices))
  {
    if (!strcmp(device->addrname, addrname))
      break;
  }

  // Process the message
  switch (packet.request_id)
  {
    case _PAPPL_SNMP_QUERY_DEVICE_TYPE:
        if (device)
        {
          _PAPPL_DEBUG("pappl_snmp_read_response: Discarding duplicate device type for \"%s\".\n", addrname);
          return;
        }

        for (i = 0; DevicePrinterOID[i] >= 0; i ++)
        {
          if (DevicePrinterOID[i] != packet.object_value.oid[i])
          {
            _PAPPL_DEBUG("pappl_snmp_read_response: Discarding device (not printer).\n");
            return;
          }
        }

        if (packet.object_value.oid[i] >= 0)
        {
          _PAPPL_DEBUG("pappl_snmp_read_response: Discarding device (not printer).\n");
          return;
        }

        // Add the device and request the device data
        temp = calloc(1, sizeof(_pappl_snmp_dev_t));
        temp->address  = packet.address;
        temp->addrname = strdup(addrname);
        temp->port     = 9100;  // Default port to use

        cupsArrayAdd(devices, temp);

        _papplSNMPWrite(fd, &(packet.address), _PAPPL_SNMP_VERSION_1, packet.community, _PAPPL_ASN1_GET_REQUEST, _PAPPL_SNMP_QUERY_DEVICE_SYSNAME, SysNameOID);
        _papplSNMPWrite(fd, &(packet.address), _PAPPL_SNMP_VERSION_1, packet.community, _PAPPL_ASN1_GET_REQUEST, _PAPPL_SNMP_QUERY_DEVICE_ID, HPDeviceIDOID);
        _papplSNMPWrite(fd, &(packet.address), _PAPPL_SNMP_VERSION_1, packet.community, _PAPPL_ASN1_GET_REQUEST, _PAPPL_SNMP_QUERY_DEVICE_ID, LexmarkDeviceIdOID);
        _papplSNMPWrite(fd, &(packet.address), _PAPPL_SNMP_VERSION_1, packet.community, _PAPPL_ASN1_GET_REQUEST, _PAPPL_SNMP_QUERY_DEVICE_ID, PWGPPMDeviceIdOID);
        _papplSNMPWrite(fd, &(packet.address), _PAPPL_SNMP_VERSION_1, packet.community, _PAPPL_ASN1_GET_REQUEST, _PAPPL_SNMP_QUERY_DEVICE_ID, ZebraDeviceIDOID);
        _papplSNMPWrite(fd, &(packet.address), _PAPPL_SNMP_VERSION_1, packet.community, _PAPPL_ASN1_GET_REQUEST, _PAPPL_SNMP_QUERY_DEVICE_PORT, LexmarkPortOID);
        _papplSNMPWrite(fd, &(packet.address), _PAPPL_SNMP_VERSION_1, packet.community, _PAPPL_ASN1_GET_REQUEST, _PAPPL_SNMP_QUERY_DEVICE_PORT, ZebraPortOID);
        _papplSNMPWrite(fd, &(packet.address), _PAPPL_SNMP_VERSION_1, packet.community, _PAPPL_ASN1_GET_REQUEST, _PAPPL_SNMP_QUERY_DEVICE_PORT, PWGPPMPortOID);
        _papplSNMPWrite(fd, &(packet.address), _PAPPL_SNMP_VERSION_1, packet.community, _PAPPL_ASN1_GET_REQUEST, _PAPPL_SNMP_QUERY_DEVICE_PORT, RawTCPPortOID);
        break;

    case _PAPPL_SNMP_QUERY_DEVICE_ID:
        if (device && packet.object_type == _PAPPL_ASN1_OCTET_STRING && (!device->device_id || strlen(device->device_id) < packet.object_value.string.num_bytes))
        {
          char  *ptr;			// Pointer into device ID

          for (ptr = (char *)packet.object_value.string.bytes; *ptr; ptr ++)
          {
            if (*ptr == '\n')		// A lot of bad printers put a newline
              *ptr = ';';
          }

	  free(device->device_id);

          device->device_id = strdup((char *)packet.object_value.string.bytes);
        }
	break;

    case _PAPPL_SNMP_QUERY_DEVICE_SYSNAME:
        if (device && packet.object_type == _PAPPL_ASN1_OCTET_STRING && !device->uri)
        {
          char uri[2048];		// Device URI

          snprintf(uri, sizeof(uri), "snmp://%s", (char *)packet.object_value.string.bytes);
          device->uri = strdup(uri);
        }
	break;

    case _PAPPL_SNMP_QUERY_DEVICE_PORT:
        if (device)
        {
          if (packet.object_type == _PAPPL_ASN1_INTEGER)
            device->port = packet.object_value.integer;
          else if (packet.object_type == _PAPPL_ASN1_OCTET_STRING)
            device->port = atoi(((char *)packet.object_value.string.bytes));
        }
	break;
  }
}


#ifdef HAVE_LIBUSB
//
// 'pappl_usb_find()' - Find a USB printer.
//

static bool				// O - `true` if found, `false` if not
pappl_usb_find(
    pappl_device_cb_t cb,		// I - Callback function
    void              *data,		// I - User data pointer
    pappl_device_t    *device,		// O - Device info
    pappl_deverr_cb_t err_cb,		// I - Error callback
    void               *err_data)	// I - Error callback data
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
    pappl_error(err_cb, err_data, "Unable to initialize USB access: %s", libusb_strerror((enum libusb_error)err));
    return (false);
  }

  num_udevs = libusb_get_device_list(NULL, &udevs);

  _PAPPL_DEBUG("pappl_usb_find: num_udevs=%d\n", (int)num_udevs);

  // Find the printers and do the callback until we find a match.
  for (i = 0; i < num_udevs; i ++)
  {
    libusb_device *udevice = udevs[i];	// Current device
    char	device_id[1024],	// Current device ID
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

	for (altset = 0, altptr = ifaceptr->altsetting; (int)altset < ifaceptr->num_altsetting; altset ++, altptr ++)
	{
	  _PAPPL_DEBUG("pappl_usb_find:             altset%d - bInterfaceClass=%d, bInterfaceSubClass=%d, bInterfaceProtocol=%d\n", altset, altptr->bInterfaceClass, altptr->bInterfaceSubClass, altptr->bInterfaceProtocol);

	  if (altptr->bInterfaceClass != LIBUSB_CLASS_PRINTER || altptr->bInterfaceSubClass != 1)
	    continue;

	  if (altptr->bInterfaceProtocol != 1 && altptr->bInterfaceProtocol != 2)
	    continue;

	  if (altptr->bInterfaceProtocol < device->protocol)
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
		if ((err = libusb_detach_kernel_driver(device->handle, device->iface)) < 0)
		{
		  pappl_error(err_cb, err_data, "Unable to detach usblp kernel driver for USB printer %04x:%04x: %s", devdesc.idVendor, devdesc.idProduct, libusb_strerror((enum libusb_error)err));
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
		pappl_error(err_cb, err_data, "Unable to claim USB interface: %s", libusb_strerror((enum libusb_error)err));
                libusb_close(device->handle);
                device->handle = NULL;
              }
            }

            if (device->handle && ifaceptr->num_altsetting > 1)
            {
              // Set the alternate setting as needed...
              if ((err = libusb_set_interface_alt_setting(device->handle, device->ifacenum, device->altset)) < 0)
              {
		pappl_error(err_cb, err_data, "Unable to set alternate USB interface: %s", libusb_strerror((enum libusb_error)err));
                libusb_close(device->handle);
                device->handle = NULL;
              }
            }

            if (device->handle)
            {
              // Get the 1284 Device ID...
              if ((err = libusb_control_transfer(device->handle, LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_ENDPOINT_IN | LIBUSB_RECIPIENT_INTERFACE, 0, device->conf, (device->iface << 8) | device->altset, (unsigned char *)device_id, sizeof(device_id), 5000)) < 0)
              {
		pappl_error(err_cb, err_data, "Unable to get IEEE-1284 device ID: %s", libusb_strerror((enum libusb_error)err));
                device_id[0] = '\0';
                libusb_close(device->handle);
                device->handle = NULL;
              }
              else
              {
                int length = ((device_id[0] & 255) << 8) | (device_id[1] & 255);
                if (length < 14 || length > sizeof(device_id))
                  length = ((device_id[1] & 255) << 8) | (device_id[0] & 255);

                if (length > sizeof(device_id))
                  length = sizeof(device_id);

                length -= 2;
                memmove(device_id, device_id + 2, (size_t)length);
                device_id[length] = '\0';

                _PAPPL_DEBUG("pappl_usb_find:     device_id=\"%s\"\n", device_id);
              }
            }

            if (device->handle)
            {
              // Build the device URI...
              char	*make,		// Pointer to make
			*model,		// Pointer to model
			*serial = NULL,	// Pointer to serial number
			*ptr,		// Pointer into device ID
			copy_did[1024],	// Copy of device ID
			temp[256];	// Temporary string for serial #

	      strlcpy(copy_did, device_id, sizeof(copy_did));

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

              if ((*cb)(device_uri, device_id, data))
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
// 'pappl_usb_open_cb()' - Look for a matching device URI.
//

static bool				// O - `true` on match, `false` otherwise
pappl_usb_open_cb(
    const char *device_uri,		// I - This device's URI
    const char *device_id,		// I - IEEE-1284 Device ID
    void       *data)			// I - URI we are looking for
{
  bool match = !strcmp(device_uri, (const char *)data);
					// Does this match?

  _PAPPL_DEBUG("pappl_usb_open_cb(device_uri=\"%s\", device_id=\"%s\", user_data=\"%s\") returning %s.\n", device_uri, device_id, (char *)data, match ? "true" : "false");

  return (match);
}
#endif // HAVE_LIBUSB


//
// 'pappl_write()' - Write data to the device.
//

static ssize_t				// O - Number of bytes written or `-1` on error
pappl_write(pappl_device_t *device,	// I - Device
            const void     *buffer,	// I - Buffer
            size_t         bytes)	// I - Bytes to write
{
  struct timeval	starttime,	// Start time
			endtime;	// End time
  ssize_t		count;		// Total bytes written


#if PAPPL_DEVICE_DEBUG
  if (device->debug_fd >= 0)
    write(device->debug_fd, buffer, bytes);
#endif // PAPPL_DEVICE_DEBUG

  gettimeofday(&starttime, NULL);

  if (device->fd >= 0)
  {
    const char	*ptr;			// Pointer into buffer
    ssize_t	written;		// Bytes written this time

    for (count = 0, ptr = (const char *)buffer; count < (ssize_t)bytes; count += (size_t)written, ptr += written)
    {
      if ((written = write(device->fd, ptr, bytes - (size_t)count)) < 0)
      {
        if (errno == EINTR || errno == EAGAIN)
          continue;

        count = -1;
        break;
      }
    }
  }
#ifdef HAVE_LIBUSB
  else if (device->handle)
  {
    int	icount;				// Bytes that were written

    if (libusb_bulk_transfer(device->handle, device->write_endp, (unsigned char *)buffer, (int)bytes, &icount, 0) < 0)
      count = -1;
    else
      count = (ssize_t)icount;
  }
#endif // HAVE_LIBUSB
  else
    count = -1;

  gettimeofday(&endtime, NULL);

  device->metrics.write_requests ++;
  device->metrics.write_msecs += 1000 * (endtime.tv_sec - starttime.tv_sec) + (endtime.tv_usec - starttime.tv_usec) / 1000;
  if (count > 0)
    device->metrics.write_bytes += (size_t)count;

  return (count);
}
