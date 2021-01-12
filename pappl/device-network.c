//
// Network device support code for the Printer Application Framework
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

#include "device-private.h"
#include "dnssd-private.h"
#include "snmp-private.h"
#include "printer.h"
#include <ifaddrs.h>
#include <net/if.h>


//
// Local types...
//

typedef struct _pappl_socket_s		// Socket device data
{
  int			fd;			// File descriptor connection to device
  char			*host;			// Hostname
  int			port;			// Port number
  http_addrlist_t	*list;			// Address list
} _pappl_socket_t;

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
static void		pappl_dnssd_query_cb(DNSServiceRef sdRef, DNSServiceFlags flags, uint32_t interfaceIndex, DNSServiceErrorType errorCode, const char *fullName, uint16_t rrtype, uint16_t rrclass, uint16_t rdlen, const void *rdata, uint32_t ttl, void *context);
static void		pappl_dnssd_resolve_cb(DNSServiceRef sdRef, DNSServiceFlags flags, uint32_t interfaceIndex, DNSServiceErrorType errorCode, const char *fullname, const char *hosttarget, uint16_t port, uint16_t txtLen, const unsigned char *txtRecord, void *context);
#  else
static void		pappl_dnssd_browse_cb(AvahiServiceBrowser *browser, AvahiIfIndex interface, AvahiProtocol protocol, AvahiBrowserEvent event, const char *serviceName, const char *serviceType, const char *replyDomain, AvahiLookupResultFlags flags, void *context);
static void		pappl_dnssd_query_cb(AvahiRecordBrowser *browser, AvahiIfIndex interface, AvahiProtocol protocol, AvahiBrowserEvent event, const char *name, uint16_t rrclass, uint16_t rrtype, const void *rdata, size_t rdlen, AvahiLookupResultFlags flags, void *context);
static void		pappl_dnssd_resolve_cb(AvahiServiceResolver *resolver, AvahiIfIndex interface, AvahiProtocol protocol, AvahiResolverEvent event, const char *name, const char *type, const char *domain, const char *host_name, const AvahiAddress *address, uint16_t port, AvahiStringList *txt, AvahiLookupResultFlags flags, void *context);
#  endif // HAVE_DNSSD
static int		pappl_dnssd_compare_devices(_pappl_dns_sd_dev_t *a, _pappl_dns_sd_dev_t *b);
static void		pappl_dnssd_free(_pappl_dns_sd_dev_t *d);
static _pappl_dns_sd_dev_t *pappl_dnssd_get_device(cups_array_t *devices, const char *serviceName, const char *replyDomain);
static bool		pappl_dnssd_list(pappl_device_cb_t cb, void *data, pappl_deverror_cb_t err_cb, void *err_data);
static void		pappl_dnssd_unescape(char *dst, const char *src, size_t dstsize);
#endif // HAVE_DNSSD || HAVE_AVAHI


static int		pappl_snmp_compare_devices(_pappl_snmp_dev_t *a, _pappl_snmp_dev_t *b);
static bool		pappl_snmp_find(pappl_device_cb_t cb, void *data, _pappl_socket_t *sock, pappl_deverror_cb_t err_cb, void *err_data);
static void		pappl_snmp_free(_pappl_snmp_dev_t *d);
static http_addrlist_t	*pappl_snmp_get_interface_addresses(void);
static bool		pappl_snmp_list(pappl_device_cb_t cb, void *data, pappl_deverror_cb_t err_cb, void *err_data);
static bool		pappl_snmp_open_cb(const char *device_info, const char *device_uri, const char *device_id, void *data);
static void		pappl_snmp_read_response(cups_array_t *devices, int fd, pappl_deverror_cb_t err_cb, void *err_data);

static void		pappl_socket_close(pappl_device_t *device);
static char		*pappl_socket_getid(pappl_device_t *device, char *buffer, size_t bufsize);
static bool		pappl_socket_open(pappl_device_t *device, const char *device_uri, const char *name);
static ssize_t		pappl_socket_read(pappl_device_t *device, void *buffer, size_t bytes);
static pappl_preason_t	pappl_socket_status(pappl_device_t *device);
static ssize_t		pappl_socket_write(pappl_device_t *device, const void *buffer, size_t bytes);


//
// '_papplDeviceAddNetworkSchemes()' - Add all of the supported network schemes.
//

void
_papplDeviceAddNetworkSchemes(void)
{
#if defined(HAVE_DNSSD) || defined(HAVE_AVAHI)
  papplDeviceAddScheme("dnssd", PAPPL_DEVTYPE_DNS_SD, pappl_dnssd_list, pappl_socket_open, pappl_socket_close, pappl_socket_read, pappl_socket_write, pappl_socket_status, pappl_socket_getid);
#endif // HAVE_DNSSD || HAVE_AVAHI
  papplDeviceAddScheme("snmp", PAPPL_DEVTYPE_SNMP, pappl_snmp_list, pappl_socket_open, pappl_socket_close, pappl_socket_read, pappl_socket_write, pappl_socket_status, pappl_socket_getid);
  papplDeviceAddScheme("socket", PAPPL_DEVTYPE_SOCKET, NULL, pappl_socket_open, pappl_socket_close, pappl_socket_read, pappl_socket_write, pappl_socket_status, pappl_socket_getid);
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
  _PAPPL_DEBUG("DEBUG: pappl_browse_cb(sdRef=%p, flags=%x, interfaceIndex=%d, errorCode=%d, serviceName=\"%s\", regtype=\"%s\", replyDomain=\"%s\", context=%p)\n", sdRef, flags, interfaceIndex, errorCode, serviceName, regtype, replyDomain, context);

  (void)regtype;

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

      if ((device->fullName = strdup(fullName)) == NULL)
      {
	cupsArrayRemove(devices, device);
	return (NULL);
      }
    }

    return (device);
  }

  // Yes, add the device...
  if ((device = calloc(sizeof(_pappl_dns_sd_dev_t), 1)) == NULL)
    return (NULL);

  if ((device->name = strdup(serviceName)) == NULL)
  {
    free(device);
    return (NULL);
  }

  device->domain = strdup(replyDomain);

  cupsArrayAdd(devices, device);

  // Set the "full name" of this service, which is used for queries...
#  ifdef HAVE_DNSSD
  DNSServiceConstructFullName(fullName, serviceName, "_pdl-datastream._tcp.", replyDomain);
#  else
  avahi_service_name_join(fullName, sizeof(fullName), serviceName, "_pdl-datastream._tcp.", replyDomain);
#  endif /* HAVE_DNSSD */

  if ((device->fullName = strdup(fullName)) == NULL)
  {
    cupsArrayRemove(devices, device);
    return (NULL);
  }

  // Query the TXT record for the device ID and make and model...
#ifdef HAVE_DNSSD
  device->ref = _papplDNSSDInit(NULL);

  DNSServiceQueryRecord(&(device->ref), kDNSServiceFlagsShareConnection, 0, device->fullName, kDNSServiceType_TXT, kDNSServiceClass_IN, pappl_dnssd_query_cb, device);
#else
  device->ref = avahi_record_browser_new(_papplDNSSDInit(NULL), AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC, device->fullName, AVAHI_DNS_CLASS_IN, AVAHI_DNS_TYPE_TXT, 0, pappl_dnssd_query_cb, device);
#endif /* HAVE_AVAHI */

  return (device);
}


//
// 'pappl_dnssd_list()' - List printers using DNS-SD.
//

static bool				// O - `true` if the callback returned `true`, `false` otherwise
pappl_dnssd_list(
    pappl_device_cb_t cb,		// I - Callback function
    void              *data,		// I - User data for callback
    pappl_deverror_cb_t err_cb,		// I - Error callback
    void              *err_data)	// I - Data for error callback
{
  bool			ret = false;	// Return value
  cups_array_t		*devices;	// DNS-SD devices
  _pappl_dns_sd_dev_t	*device;	// Current DNS-SD device
  char			device_name[1024],
					// Network device name
			device_uri[1024];
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
    _papplDeviceError(err_cb, err_data, "Unable to create service browser: %s (%d).", _papplDNSSDStrError(error), error);
    cupsArrayDelete(devices);
    return (ret);
  }

#  else
  if ((pdl_ref = avahi_service_browser_new(_papplDNSSDInit(NULL), AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC, "_pdl-datastream._tcp", NULL, 0, pappl_dnssd_browse_cb, devices)) == NULL)
  {
    _papplDeviceError(err_cb, err_data, "Unable to create service browser.");
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
    snprintf(device_name, sizeof(device_name), "%s (DNS-SD Network Printer)", device->name);

    if (device->uuid)
      httpAssembleURIf(HTTP_URI_CODING_ALL, device_uri, sizeof(device_uri), "dnssd", NULL, device->fullName, 0, "/?uuid=%s", device->uuid);
    else
      httpAssembleURI(HTTP_URI_CODING_ALL, device_uri, sizeof(device_uri), "dnssd", NULL, device->fullName, 0, "/");

    if ((*cb)(device_name, device_uri, device->device_id, data))
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
// 'pappl_dnssd_query_cb()' - Query a DNS-SD service.
//

#  ifdef HAVE_DNSSD
static void
pappl_dnssd_query_cb(
    DNSServiceRef       sdRef,		// I - Service reference
    DNSServiceFlags     flags,		// I - Data flags
    uint32_t            interfaceIndex,	// I - Interface (unused)
    DNSServiceErrorType errorCode,	// I - Error, if any
    const char          *fullName,	// I - Full service name
    uint16_t            rrtype,		// I - Record type
    uint16_t            rrclass,	// I - Record class
    uint16_t            rdlen,		// I - Length of record data
    const void          *rdata,		// I - Record data
    uint32_t            ttl,		// I - Time-to-live
    void                *context)	// I - Device
#  else
static void
pappl_dnssd_query_cb(
    AvahiRecordBrowser     *browser,	// I - Record browser */
    AvahiIfIndex           interfaceIndex,
					// I - Interface index (unused)
    AvahiProtocol          protocol,	// I - Network protocol (unused)
    AvahiBrowserEvent      event,	// I - What happened?
    const char             *fullName,	// I - Service name
    uint16_t               rrclass,	// I - Record clasa
    uint16_t               rrtype,	// I - Record type
    const void             *rdata,	// I - TXT record
    size_t                 rdlen,	// I - Length of TXT record
    AvahiLookupResultFlags flags,	// I - Flags
    void                   *context)	// I - Device
#  endif // HAVE_DNSSD
{
  char		*ptr;			// Pointer into string
  _pappl_dns_sd_dev_t	*device = (_pappl_dns_sd_dev_t *)context;
					// Device
  const uint8_t	*data,			// Pointer into data
		*datanext,		// Next key/value pair
		*dataend;		// End of entire TXT record
  uint8_t	datalen;		// Length of current key/value pair
  char		key[256],		// Key string
		value[256],		// Value string
		cmd[256],		// usb_CMD string
		mdl[256],		// usb_MDL string
		mfg[256],		// usb_MFG string
		pdl[256],		// pdl string
		product[256],		// product string
		ty[256],		// ty string
		device_id[1024];	// 1284 device ID */


#  ifdef HAVE_DNSSD
  // Only process "add" data...
  if (errorCode != kDNSServiceErr_NoError || !(flags & kDNSServiceFlagsAdd))
    return;

  (void)sdRef;
  (void)interfaceIndex;
  (void)fullName;
  (void)ttl;
  (void)rrtype;
  (void)rrclass;

#  else
  // Only process "add" data...
  if (event != AVAHI_BROWSER_NEW)
    return;

  (void)interfaceIndex;
  (void)protocol;
  (void)fullName;
  (void)rrclass;
  (void)rrtype;
  (void)flags;
#  endif /* HAVE_DNSSD */

  // Pull out the make and model and device ID data from the TXT record...
  cmd[0]     = '\0';
  mfg[0]     = '\0';
  mdl[0]     = '\0';
  pdl[0]     = '\0';
  product[0] = '\0';
  ty[0]      = '\0';

  for (data = rdata, dataend = data + rdlen; data < dataend; data = datanext)
  {
    // Read a key/value pair starting with an 8-bit length.  Since the length is
    // 8 bits and the size of the key/value buffers is 256, we don't need to
    // check for overflow...
    datalen = *data++;

    if (!datalen || (data + datalen) > dataend)
      break;

    datanext = data + datalen;

    for (ptr = key; data < datanext && *data != '='; data ++)
      *ptr++ = (char)*data;
    *ptr = '\0';

    if (data < datanext && *data == '=')
    {
      data ++;

      if (data < datanext)
	memcpy(value, data, (size_t)(datanext - data));
      value[datanext - data] = '\0';
    }
    else
    {
      continue;
    }

    if (!strcasecmp(key, "usb_CMD"))
      strlcpy(cmd, value, sizeof(cmd));
    else if (!strcasecmp(key, "usb_MDL"))
      strlcpy(mdl, value, sizeof(mdl));
    else if (!strcasecmp(key, "usb_MFG"))
      strlcpy(mfg, value, sizeof(mfg));
    else if (!strcasecmp(key, "pdl"))
      strlcpy(pdl, value, sizeof(pdl));
    else if (!strcasecmp(key, "product"))
      strlcpy(product, value, sizeof(product));
    else if (!strcasecmp(key, "ty"))
      strlcpy(ty, value, sizeof(ty));
  }

  // Synthesize values as needed...
  if (!cmd[0] && pdl[0])
  {
    int		i;			// Looping var
    char	*cmdptr,		// Pointer into CMD value
		*pdlptr;		// Pointer into pdl value
    static const char * const pdls[][2] =
    {					// MIME media type to command set mapping
      { "application/postscript", "PS" },
      { "application/vnd.canon-cpdl", "CPDL" },
      { "application/vnd.canon-lips", "LIPS" },
      { "application/vnd.hp-PCL", "PCL" },
      { "application/vnd.hp-PCLXL", "PCLXL" },
      { "application/vnd.ms-xpsdocument", "XPS" },
      { "image/jpeg", "JPEG" },
      { "image/tiff", "TIFF" }
    };

    for (i = 0, cmdptr = cmd; i < (int)(sizeof(pdls) / sizeof(pdls[0])); i ++)
    {
      if ((pdlptr = strstr(pdl, pdls[i][0])) != NULL)
      {
        if ((pdlptr == pdl || pdlptr[-1] == ',') && pdlptr[strlen(pdls[i][0])] == ',')
        {
          if (cmdptr > cmd && cmdptr < (cmd + sizeof(cmd) - 1))
            *cmdptr++ = ',';
	  strlcpy(cmdptr, pdls[i][1], sizeof(cmd) - (size_t)(cmdptr - cmd));
	  cmdptr += strlen(cmdptr);
        }
      }
    }
  }

  if (!ty[0] && product[0])
  {
    if (product[0] == '(')
    {
      strlcpy(ty, product + 1, sizeof(ty));
      if ((ptr = product + strlen(product) - 1) >= product && *ptr == ')')
        *ptr = '\0';
    }
    else
      strlcpy(ty, product, sizeof(ty));
  }

  if (!ty[0] && mfg[0] && mdl[0])
    snprintf(ty, sizeof(ty), "%s %s", mfg, mdl);

  if (!mfg[0] && ty[0])
  {
    strlcpy(mfg, ty, sizeof(mfg));
    if ((ptr = strchr(mfg, ' ')) != NULL)
      *ptr = '\0';
  }

  if (!mdl[0] && ty[0])
  {
    if ((ptr = strchr(ty, ' ')) != NULL)
      strlcpy(mdl, ptr + 1, sizeof(mdl));
    else
      strlcpy(mdl, ty, sizeof(mdl));
  }

  snprintf(device_id, sizeof(device_id), "MFG:%s;MDL:%s;CMD:%s;", mfg, mdl, cmd);

  // Save the make and model and IEEE-1284 device ID...
  device->device_id      = strdup(device_id);
  device->make_and_model = strdup(ty);
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
  (void)sdRef;
  (void)interfaceIndex;
  (void)fullname;
  (void)txtLen;
  (void)txtRecord;

  if (errorCode == kDNSServiceErr_NoError && (flags & kDNSServiceFlagsAdd))
  {
    _pappl_socket_t *sock = (_pappl_socket_t *)context;
					// Socket

    sock->host = strdup(host_name);
    sock->port = port;
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
    _pappl_socket_t *sock = (_pappl_socket_t *)context;
					// Socket

    sock->host = strdup(host_name);
    sock->port = port;
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
// 'pappl_snmp_find()' - Find an SNMP device.
//

static bool				// O - `true` if found, `false` if not
pappl_snmp_find(
    pappl_device_cb_t   cb,		// I - Callback function
    void                *data,		// I - User data pointer
    _pappl_socket_t     *sock,		// O - Device info
    pappl_deverror_cb_t err_cb,		// I - Error callback
    void                *err_data)	// I - Error callback data
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
    _papplDeviceError(err_cb, err_data, "Unable to open SNMP socket.");
    goto finished;
  }

  // Get the list of network interface broadcast addresses...
  if ((addrs = pappl_snmp_get_interface_addresses()) == NULL)
  {
    _papplDeviceError(err_cb, err_data, "Unable to get SNMP broadcast addresses.");
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
      _papplDeviceError(err_cb, err_data, "SNMP select() failed with error: %s", strerror(errno));
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
    char	info[256];		// Device description
    int		num_did;		// Number of device ID keys/values
    cups_option_t *did;			// Device ID keys/values
    const char	*make,			// Manufacturer
		*model;			// Model name

    // Skip LPD (port 515) and IPP (port 631) since they can't be raw sockets...
    if (cur_device->port == 515 || cur_device->port == 631 || !cur_device->uri)
      continue;

    num_did = papplDeviceParseID(cur_device->device_id, &did);

    if ((make = cupsGetOption("MANUFACTURER", num_did, did)) == NULL)
      if ((make = cupsGetOption("MFG", num_did, did)) == NULL)
        if ((make = cupsGetOption("MFGR", num_did, did)) == NULL)
          make = "Unknown";

    if ((model = cupsGetOption("MODEL", num_did, did)) == NULL)
      if ((model = cupsGetOption("MDL", num_did, did)) == NULL)
        model = "Printer";

    if (!strcmp(make, "HP") && !strncmp(model, "HP ", 3))
      snprintf(info, sizeof(info), "%s (Network Printer %s)", model, cur_device->uri + 7);
    else
      snprintf(info, sizeof(info), "%s %s (Network Printer %s)", make, model, cur_device->uri + 7);

    if ((*cb)(info, cur_device->uri, cur_device->device_id, data))
    {
      // Save the address and port...
      char	address_str[256];	// IP address as a string

      sock->host = strdup(httpAddrString(&cur_device->address, address_str, sizeof(address_str)));
      sock->port = cur_device->port;
      ret        = true;
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
      if ((current = calloc(1, sizeof(http_addrlist_t))) != NULL)
      {
	memcpy(&(current->addr), addr->ifa_broadaddr, sizeof(struct sockaddr_in));

	if (!last)
	  first = current;
	else
	  last->next = current;

	last = current;
      }
    }
  }

  // Free the original interface addresses and return...
  freeifaddrs(addrs);

  return (first);
}


//
// 'pappl_snmp_list()' - List SNMP printers.
//

static bool				// O - `true` if found, `false` otherwise
pappl_snmp_list(
    pappl_device_cb_t   cb,		// I - Device callback
    void                *data,		// I - Device callback data
    pappl_deverror_cb_t err_cb,		// I - Error callback
    void                *err_data)	// I - Error callback data
{
  _pappl_socket_t	sock;		// Socket data
  bool			ret;		// Return value


  memset(&sock, 0, sizeof(sock));

  ret = pappl_snmp_find(cb, data, &sock, err_cb, err_data);

  free(sock.host);

  return (ret);
}


//
// 'pappl_snmp_open_cb()' - Look for a matching device URI.
//

static bool				// O - `true` on match, `false` otherwise
pappl_snmp_open_cb(
    const char *device_info,		// I - Device description
    const char *device_uri,		// I - This device's URI
    const char *device_id,		// I - IEEE-1284 Device ID
    void       *data)			// I - URI we are looking for
{
  bool match = !strcmp(device_uri, (const char *)data);
					// Does this match?

  _PAPPL_DEBUG("pappl_snmp_open_cb(device_info=\"%s\", device_uri=\"%s\", device_id=\"%s\", user_data=\"%s\") = %s\n", device_info, device_uri, device_id, (char *)data, match ? "true" : "false");

  return (match);
}


//
// 'pappl_snmp_read_response()' - Read and parse a SNMP response.
//

static void
pappl_snmp_read_response(
    cups_array_t      *devices,		// Devices array
    int               fd,		// I - SNMP socket file descriptor
    pappl_deverror_cb_t err_cb,		// I - Error callback
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
    _papplDeviceError(err_cb, err_data, "Unable to read SNMP response data: %s", strerror(errno));
    return;
  }

  httpAddrString(&(packet.address), addrname, sizeof(addrname));

  // Look for the response status code in the SNMP message header
  if (packet.error)
  {
    _papplDeviceError(err_cb, err_data, "Bad SNMP packet from '%s': %s", addrname, packet.error);
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
        if ((temp = calloc(1, sizeof(_pappl_snmp_dev_t))) == NULL)
        {
          _PAPPL_DEBUG("pappl_snmp_read_response: Unable to allocate memory for device.\n");
          return;
        }

        temp->address  = packet.address;
        temp->addrname = strdup(addrname);
        temp->port     = 9100;  // Default port to use

        if (!temp->addrname)
        {
          _PAPPL_DEBUG("pappl_snmp_read_response: Unable to allocate memory for device name.\n");
          free(temp);
          return;
        }

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
          {
            device->port = packet.object_value.integer;
          }
          else if (packet.object_type == _PAPPL_ASN1_OCTET_STRING)
          {
            char *end;			// End of string

            device->port = (int)strtol((char *)packet.object_value.string.bytes, &end, 10);
            if (errno == ERANGE || *end)
              device->port = 0;
	  }
        }
	break;
  }
}


//
// 'pappl_socket_close()' - Close a network socket.
//

static void
pappl_socket_close(
    pappl_device_t *device)		// I - Device
{
  _pappl_socket_t	*sock;		// Socket device


  if ((sock = papplDeviceGetData(device)) == NULL)
    return;

  close(sock->fd);
  httpAddrFreeList(sock->list);
  free(sock);

  papplDeviceSetData(device, NULL);
}


//
// 'pappl_socket_getid()' - Get the current IEEE-1284 device ID via SNMP.
//

static char *				// O - Device ID or `NULL` on error
pappl_socket_getid(
    pappl_device_t *device,		// I - Device
    char           *buffer,		// I - Buffer
    size_t         bufsize)		// I - Size of buffer
{
  // TODO: Implement network query of IEEE-1284 device ID (Issue #95)
  (void)device;
  (void)buffer;
  (void)bufsize;

  return (NULL);
}


//
// 'pappl_socket_open()' - Open a network socket.
//

static bool				// O - `true` on success, `false` on failure
pappl_socket_open(
    pappl_device_t *device,		// I - Device
    const char     *device_uri,		// I - Device URI
    const char     *job_name)		// I - Job name
{
  _pappl_socket_t	*sock;		// Socket device
  char			scheme[32],	// URI scheme
			userpass[32],	// Username/password (not used)
			host[256],	// Host name or make
			resource[256],	// Resource path, if any
			*options;	// Pointer to options, if any
  int			port;		// Port number
  char			port_str[32];	// String for port number


  (void)job_name;

  // Allocate memory for the socket...
  if ((sock = (_pappl_socket_t *)calloc(1, sizeof(_pappl_socket_t))) == NULL)
  {
    papplDeviceError(device, "Unable to allocate memory for socket device: %s", strerror(errno));
    return (false);
  }

  // Split apart the URI...
  httpSeparateURI(HTTP_URI_CODING_ALL, device_uri, scheme, sizeof(scheme), userpass, sizeof(userpass), host, sizeof(host), &port, resource, sizeof(resource));

  if ((options = strchr(resource, '?')) != NULL)
    *options++ = '\0';

  if (!strcmp(scheme, "dnssd"))
  {
    // DNS-SD discovered device
#if defined(HAVE_DNSSD) || defined(HAVE_AVAHI)
    int			i;		// Looping var
    char		srvname[256],	// Service name
			*type,		// Service type
			*domain;	// Domain
    _pappl_dns_sd_t	master;		// DNS-SD context
#  ifdef HAVE_DNSSD
    int			error;		// Error code, if any
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
      pappl_dnssd_unescape(srvname, host, sizeof(srvname));

      master = _papplDNSSDInit(NULL);

#  ifdef HAVE_DNSSD
      resolver = master;
      if ((error = DNSServiceResolve(&resolver, kDNSServiceFlagsShareConnection, 0, srvname, type, domain, (DNSServiceResolveReply)pappl_dnssd_resolve_cb, sock)) != kDNSServiceErr_NoError)
      {
	papplDeviceError(device, "Unable to resolve '%s': %s", device_uri, _papplDNSSDStrError(error));
	goto error;
      }
#  else
      _papplDNSSDLock();

      if ((resolver = avahi_service_resolver_new(master, AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC, srvname, type, domain, AVAHI_PROTO_UNSPEC, 0, (AvahiServiceResolverCallback)pappl_dnssd_resolve_cb, sock)) == NULL)
      {
	papplDeviceError(device, "Unable to resolve '%s'.", device_uri);
	_papplDNSSDUnlock();
	goto error;
      }

      _papplDNSSDUnlock();
#  endif // HAVE_DNSSD

      // Wait up to 30 seconds for the resolve to complete...
      for (i = 0; i < 30000 && !sock->host; i ++)
	usleep(1000);

#  ifdef HAVE_DNSSD
      DNSServiceRefDeallocate(resolver);
#  else
      _papplDNSSDLock();
      avahi_service_resolver_free(resolver);
      _papplDNSSDUnlock();
#  endif // HAVE_DNSSD

      if (!sock->host)
      {
	papplDeviceError(device, "Unable to resolve '%s'.", device_uri);
	goto error;
      }
    }
#endif // HAVE_DNSSD || HAVE_AVAHI
  }
  else if (!strcmp(scheme, "snmp"))
  {
    // SNMP discovered device
    if (!pappl_snmp_find(pappl_snmp_open_cb, (void *)device_uri, sock, NULL, NULL))
      goto error;
  }
  else if (!strcmp(scheme, "socket"))
  {
    // Raw socket (JetDirect or similar)
    sock->host = strdup(host);
    sock->port = port;
  }

  // Lookup the address of the printer...
  snprintf(port_str, sizeof(port_str), "%d", sock->port);
  if ((sock->list = httpAddrGetList(sock->host, AF_UNSPEC, port_str)) == NULL)
  {
    papplDeviceError(device, "Unable to lookup '%s:%d': %s", sock->host, sock->port, cupsLastErrorString());
    goto error;
  }

  sock->fd = -1;

  httpAddrConnect2(sock->list, &sock->fd, 30000, NULL);

  if (sock->fd < 0)
  {
    papplDeviceError(device, "Unable to connect to '%s:%d': %s", sock->host, sock->port, cupsLastErrorString());
    goto error;
  }

  papplDeviceSetData(device, sock);

  _PAPPL_DEBUG("Connection successful, device fd = %d\n", sock->fd);

  return (true);

  // If we get here there was an error...
  error:

  free(sock->host);
  httpAddrFreeList(sock->list);
  free(sock);

  return (false);
}


//
// 'pappl_socket_read()' - Read from a network socket.
//

static ssize_t				// O - Number of bytes read
pappl_socket_read(
    pappl_device_t *device,		// I - Device
    void           *buffer,		// I - Buffer to read into
    size_t         bytes)		// I - Bytes to read
{
  _pappl_socket_t	*sock;		// Socket device
  ssize_t		count;		// Total bytes read
  struct pollfd		data;		// poll() data
  int			nfds;		// poll() return value


  if ((sock = papplDeviceGetData(device)) == NULL)
    return (-1);

  // Only read if we have data to read within 100ms...
  data.fd      = sock->fd;
  data.events  = POLLIN;
  data.revents = 0;

  while ((nfds = poll(&data, 1, 100)) < 0)
  {
    if (errno != EINTR && errno != EAGAIN)
      break;
  }

  if (nfds < 1 || !(data.revents & POLLIN))
    return (-1);

  // Read data from the socket, protecting against signals and busy kernels...
  while ((count = read(sock->fd, buffer, bytes)) < 0)
  {
    if (errno != EINTR && errno != EAGAIN)
      break;
  }

  return (count);
}


//
// 'pappl_socket_status()' - Get the current network device status.
//

static pappl_preason_t			// O - New "printer-state-reasons" values
pappl_socket_status(
    pappl_device_t *device)		// I - Device
{
  (void)device;

  return (PAPPL_PREASON_NONE);
}


//
// 'pappl_socket_write()' - Write to a network socket.
//

static ssize_t				// O - Number of bytes written
pappl_socket_write(
    pappl_device_t *device,		// I - Device
    const void     *buffer,		// I - Write buffer
    size_t         bytes)		// I - Bytes to write
{
  _pappl_socket_t	*sock;		// Socket device
  ssize_t		count,		// Total bytes written
			written;	// Bytes written this time
  const char		*ptr;		// Pointer into buffer


  if ((sock = papplDeviceGetData(device)) == NULL)
    return (-1);

  for (count = 0, ptr = (const char *)buffer; count < (ssize_t)bytes; count += written, ptr += written)
  {
    if ((written = write(sock->fd, ptr, bytes - (size_t)count)) < 0)
    {
      if (errno == EINTR || errno == EAGAIN)
      {
        written = 0;
	continue;
      }

      count = -1;
      break;
    }
  }

  return (count);
}


