//
// Network device support code for the Printer Application Framework
//
// Copyright © 2019-2023 by Michael R Sweet.
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
#include "printer-private.h"
#include <cups/transcode.h>
#if !_WIN32
#  include <ifaddrs.h>
#  include <net/if.h>
#endif // !_WIN32


//
// Local constants...
//

#define _PAPPL_MAX_SNMP_SUPPLY	32	// Maximum number of SNMP supplies
#define _PAPPL_SNMP_TIMEOUT	2.0	// Timeout for SNMP queries

// Generic enum values
#define _PAPPL_TC_other			1
#define _PAPPL_TC_unknown		2

// hrPrinterDetectedErrorState values
#define _PAPPL_TC_lowPaper		0x8000
#define _PAPPL_TC_noPaper		0x4000
#define _PAPPL_TC_lowToner		0x2000
#define _PAPPL_TC_noToner		0x1000
#define _PAPPL_TC_doorOpen		0x0800
#define _PAPPL_TC_jammed		0x0400
#define _PAPPL_TC_offline		0x0200
#define _PAPPL_TC_serviceRequested	0x0100
#define _PAPPL_TC_inputTrayMissing	0x0080
#define _PAPPL_TC_outputTrayMissing	0x0040
#define _PAPPL_TC_markerSupplyMissing	0x0020
#define _PAPPL_TC_outputNearFull	0x0010
#define _PAPPL_TC_outputFull		0x0008
#define _PAPPL_TC_inputTrayEmpty	0x0004
#define _PAPPL_TC_overduePreventMaint	0x0002

// prtMarkerSuppliesClass value
#define _PAPPL_TC_supplyThatIsConsumed	3

// prtMarkerSuppliesSupplyUnit value
#define _PAPPL_TC_percent		19

// prtLocalizationCharacterSet values
#define _PAPPL_TC_csASCII		3
#define _PAPPL_TC_csISOLatin1		4
#define _PAPPL_TC_csShiftJIS		17
#define _PAPPL_TC_csUTF8		106
#define _PAPPL_TC_csUnicode		1000 // UCS2 BE
#define _PAPPL_TC_csUCS4		1001 // UCS4 BE
#define _PAPPL_TC_csUnicodeASCII	1002
#define _PAPPL_TC_csUnicodeLatin1	1003
#define _PAPPL_TC_csUTF16BE		1013
#define _PAPPL_TC_csUTF16LE		1014
#define _PAPPL_TC_csUTF32		1017
#define _PAPPL_TC_csUTF32BE		1018
#define _PAPPL_TC_csUTF32LE		1019
#define _PAPPL_TC_csWindows31J		2024


//
// Local types...
//

typedef struct _pappl_socket_s		// Socket device data
{
  int			fd;			// File descriptor connection to device
  char			*host;			// Hostname
  int			port;			// Port number
  http_addrlist_t	*list,			// Address list
			*addr;			// Connected address
  int			snmp_fd,		// SNMP socket
			charset,		// Character set
			num_supplies;		// Number of supplies
  pappl_supply_t	supplies[_PAPPL_MAX_SNMP_SUPPLY];
						// Supplies
  int			colorants[_PAPPL_MAX_SNMP_SUPPLY],
						// Colorant indices
			levels[_PAPPL_MAX_SNMP_SUPPLY],
						// Current level
			max_capacities[_PAPPL_MAX_SNMP_SUPPLY],
						// Max capacity
			units[_PAPPL_MAX_SNMP_SUPPLY];
						// Supply units
} _pappl_socket_t;

typedef struct _pappl_dns_sd_dev_t	// DNS-SD browse data
{
#ifdef HAVE_MDNSRESPONDER
  DNSServiceRef		ref;			// Service reference for query
#endif // HAVE_MDNSRESPONDER
#ifdef HAVE_AVAHI
  AvahiRecordBrowser	*ref;			// Browser for query
#endif // HAVE_AVAHI
  pthread_mutex_t	mutex;			// Update lock
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
// Local globals...
//

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
static const int	hrPrinterDetectedErrorState[] = { 1,3,6,1,2,1,25,3,5,1,2,-1 };
					// Current status bits
#define _PAPPL_PRINTERMIBv2	1,3,6,1,2,1,43
static const int	prtGeneralCurrentLocalization[] = { _PAPPL_PRINTERMIBv2,5,1,1,2,1,-1 };
					// Current localization
static const int	prtLocalizationCharacterSet[] = { _PAPPL_PRINTERMIBv2,7,1,1,4,-1 };
					// Character set
static const int	prtMarkerSuppliesEntry[] = { _PAPPL_PRINTERMIBv2,11,1,1,-1 };
					// Supply entry
static const int	prtMarkerSuppliesLevel[] = { _PAPPL_PRINTERMIBv2,11,1,1,9,-1 };
					// Level
static const int	prtMarkerColorantValue[] = { _PAPPL_PRINTERMIBv2,12,1,1,4,-1 };
					// Colorant value


//
// Local functions...
//

#ifdef HAVE_DNSSD
#  ifdef HAVE_MDNSRESPONDER
static void 		pappl_dnssd_browse_cb(DNSServiceRef sdRef, DNSServiceFlags flags, uint32_t interfaceIndex, DNSServiceErrorType errorCode, const char *serviceName, const char *regtype, const char *replyDomain, void *context);
static void		pappl_dnssd_query_cb(DNSServiceRef sdRef, DNSServiceFlags flags, uint32_t interfaceIndex, DNSServiceErrorType errorCode, const char *fullName, uint16_t rrtype, uint16_t rrclass, uint16_t rdlen, const void *rdata, uint32_t ttl, void *context);
static void		pappl_dnssd_resolve_cb(DNSServiceRef sdRef, DNSServiceFlags flags, uint32_t interfaceIndex, DNSServiceErrorType errorCode, const char *fullname, const char *hosttarget, uint16_t port, uint16_t txtLen, const unsigned char *txtRecord, void *context);
#  else
static void		pappl_dnssd_browse_cb(AvahiServiceBrowser *browser, AvahiIfIndex interface, AvahiProtocol protocol, AvahiBrowserEvent event, const char *serviceName, const char *serviceType, const char *replyDomain, AvahiLookupResultFlags flags, void *context);
static void		pappl_dnssd_query_cb(AvahiRecordBrowser *browser, AvahiIfIndex interface, AvahiProtocol protocol, AvahiBrowserEvent event, const char *name, uint16_t rrclass, uint16_t rrtype, const void *rdata, size_t rdlen, AvahiLookupResultFlags flags, void *context);
static void		pappl_dnssd_resolve_cb(AvahiServiceResolver *resolver, AvahiIfIndex interface, AvahiProtocol protocol, AvahiResolverEvent event, const char *name, const char *type, const char *domain, const char *host_name, const AvahiAddress *address, uint16_t port, AvahiStringList *txt, AvahiLookupResultFlags flags, void *context);
#  endif // HAVE_MDNSRESPONDER
static int		pappl_dnssd_compare_devices(_pappl_dns_sd_dev_t *a, _pappl_dns_sd_dev_t *b);
static void		pappl_dnssd_free(_pappl_dns_sd_dev_t *d);
static _pappl_dns_sd_dev_t *pappl_dnssd_get_device(cups_array_t *devices, const char *serviceName, const char *replyDomain);
static bool		pappl_dnssd_list(pappl_device_cb_t cb, void *data, pappl_deverror_cb_t err_cb, void *err_data);
static void		pappl_dnssd_unescape(char *dst, const char *src, size_t dstsize);
#endif // HAVE_DNSSD


static int		pappl_snmp_compare_devices(_pappl_snmp_dev_t *a, _pappl_snmp_dev_t *b);
static bool		pappl_snmp_find(pappl_device_cb_t cb, void *data, _pappl_socket_t *sock, pappl_deverror_cb_t err_cb, void *err_data);
static void		pappl_snmp_free(_pappl_snmp_dev_t *d);
static http_addrlist_t	*pappl_snmp_get_interface_addresses(void);
static bool		pappl_snmp_list(pappl_device_cb_t cb, void *data, pappl_deverror_cb_t err_cb, void *err_data);
static bool		pappl_snmp_open_cb(const char *device_info, const char *device_uri, const char *device_id, void *data);
static void		pappl_snmp_read_response(cups_array_t *devices, int fd, pappl_deverror_cb_t err_cb, void *err_data);
static void		pappl_snmp_walk_cb(_pappl_snmp_t *packet, _pappl_socket_t *sock);

static void		pappl_socket_close(pappl_device_t *device);
static char		*pappl_socket_getid(pappl_device_t *device, char *buffer, size_t bufsize);
static bool		pappl_socket_open(pappl_device_t *device, const char *device_uri, const char *name);
static ssize_t		pappl_socket_read(pappl_device_t *device, void *buffer, size_t bytes);
static pappl_preason_t	pappl_socket_status(pappl_device_t *device);
static int		pappl_socket_supplies(pappl_device_t *device, int max_supplies, pappl_supply_t *supplies);
static ssize_t		pappl_socket_write(pappl_device_t *device, const void *buffer, size_t bytes);
static void		utf16_to_utf8(char *dst, const unsigned char *src, size_t srcsize, size_t dstsize, bool le);


//
// '_papplDeviceAddNetworkSchemesNoLock()' - Add all of the supported network schemes.
//

void
_papplDeviceAddNetworkSchemesNoLock(void)
{
#ifdef HAVE_DNSSD
  _papplDeviceAddSchemeNoLock("dnssd", PAPPL_DEVTYPE_DNS_SD, pappl_dnssd_list, pappl_socket_open, pappl_socket_close, pappl_socket_read, pappl_socket_write, pappl_socket_status, pappl_socket_supplies, pappl_socket_getid);
#endif // HAVE_DNSSD
  _papplDeviceAddSchemeNoLock("snmp", PAPPL_DEVTYPE_SNMP, pappl_snmp_list, pappl_socket_open, pappl_socket_close, pappl_socket_read, pappl_socket_write, pappl_socket_status, pappl_socket_supplies, pappl_socket_getid);
  _papplDeviceAddSchemeNoLock("socket", PAPPL_DEVTYPE_SOCKET, NULL, pappl_socket_open, pappl_socket_close, pappl_socket_read, pappl_socket_write, pappl_socket_status, pappl_socket_supplies, pappl_socket_getid);
}


#ifdef HAVE_DNSSD
#  ifdef HAVE_MDNSRESPONDER
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

  (void)sdRef;
  (void)interfaceIndex;
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
#  endif // HAVE_MDNSRESPONDER


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
  pthread_mutex_destroy(&d->mutex);
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

#  ifdef HAVE_MDNSRESPONDER
      DNSServiceConstructFullName(fullName, device->name, "_pdl-datastream._tcp.", replyDomain);
#  else
      avahi_service_name_join(fullName, sizeof(fullName), serviceName, "_pdl-datastream._tcp.", replyDomain);
#  endif // HAVE_MDNSRESPONDER

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

  pthread_mutex_init(&device->mutex, NULL);

  if ((device->name = strdup(serviceName)) == NULL)
  {
    free(device);
    return (NULL);
  }

  device->domain = strdup(replyDomain);

  cupsArrayAdd(devices, device);

  // Set the "full name" of this service, which is used for queries...
#  ifdef HAVE_MDNSRESPONDER
  DNSServiceConstructFullName(fullName, serviceName, "_pdl-datastream._tcp.", replyDomain);
#  else
  avahi_service_name_join(fullName, sizeof(fullName), serviceName, "_pdl-datastream._tcp.", replyDomain);
#  endif /* HAVE_MDNSRESPONDER */

  if ((device->fullName = strdup(fullName)) == NULL)
  {
    cupsArrayRemove(devices, device);
    return (NULL);
  }

  // Query the TXT record for the device ID and make and model...
#ifdef HAVE_MDNSRESPONDER
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
  cups_len_t		last_count;	// Last number of devices
  int			timeout;	// Timeout counter
#  ifdef HAVE_MDNSRESPONDER
  int			error;		// Error code, if any
  DNSServiceRef		pdl_ref;	// Browse reference for _pdl-datastream._tcp
#  else
  AvahiServiceBrowser	*pdl_ref;	// Browse reference for _pdl-datastream._tcp
#  endif // HAVE_MDNSRESPONDER


  devices = cupsArrayNew((cups_array_cb_t)pappl_dnssd_compare_devices, NULL, NULL, 0, NULL, (cups_afree_cb_t)pappl_dnssd_free);
  _PAPPL_DEBUG("pappl_dnssd_find: devices=%p\n", devices);

  _papplDNSSDLock();

#  ifdef HAVE_MDNSRESPONDER
  pdl_ref = _papplDNSSDInit(NULL);

  _PAPPL_DEBUG("pappl_dnssd_find: pdl_ref=%p (before)\n", pdl_ref);

  if ((error = DNSServiceBrowse(&pdl_ref, kDNSServiceFlagsShareConnection, 0, "_pdl-datastream._tcp", NULL, (DNSServiceBrowseReply)pappl_dnssd_browse_cb, devices)) != kDNSServiceErr_NoError)
  {
    _papplDeviceError(err_cb, err_data, "Unable to create service browser: %s (%d).", _papplDNSSDStrError(error), error);
    cupsArrayDelete(devices);
    return (ret);
  }

  _PAPPL_DEBUG("pappl_dnssd_find: pdl_ref=%p (after)\n", pdl_ref);

#  else
  if ((pdl_ref = avahi_service_browser_new(_papplDNSSDInit(NULL), AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC, "_pdl-datastream._tcp", NULL, 0, pappl_dnssd_browse_cb, devices)) == NULL)
  {
    _papplDeviceError(err_cb, err_data, "Unable to create service browser.");
    cupsArrayDelete(devices);
    return (ret);
  }
#  endif // HAVE_MDNSRESPONDER

  _papplDNSSDUnlock();

  // Wait up to 10 seconds for us to find all available devices...
  for (timeout = 10000, last_count = 0; timeout > 0; timeout -= 250)
  {
    // 250000 microseconds == 250 milliseconds
    _PAPPL_DEBUG("pappl_dnssd_find: timeout=%d, last_count=%u\n", timeout, (unsigned)last_count);
    usleep(250000);

    if (last_count == cupsArrayGetCount(devices))
      break;

    last_count = cupsArrayGetCount(devices);
  }

  _PAPPL_DEBUG("pappl_dnssd_find: timeout=%d, last_count=%u\n", timeout, (unsigned)last_count);

  // Stop browsing...
  _papplDNSSDLock();

#  ifdef HAVE_MDNSRESPONDER
  DNSServiceRefDeallocate(pdl_ref);
#  else
  avahi_service_browser_free(pdl_ref);
#  endif // HAVE_MDNSRESPONDER

  _papplDNSSDUnlock();

  // Do the callback for each of the devices...
  for (device = (_pappl_dns_sd_dev_t *)cupsArrayGetFirst(devices); device; device = (_pappl_dns_sd_dev_t *)cupsArrayGetNext(devices))
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

  // Free memory and return...
  cupsArrayDelete(devices);

  return (ret);
}


//
// 'pappl_dnssd_query_cb()' - Query a DNS-SD service.
//

#  ifdef HAVE_MDNSRESPONDER
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
#  endif // HAVE_MDNSRESPONDER
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


#  ifdef HAVE_MDNSRESPONDER
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
#  endif // HAVE_MDNSRESPONDER

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
      papplCopyString(cmd, value, sizeof(cmd));
    else if (!strcasecmp(key, "usb_MDL"))
      papplCopyString(mdl, value, sizeof(mdl));
    else if (!strcasecmp(key, "usb_MFG"))
      papplCopyString(mfg, value, sizeof(mfg));
    else if (!strcasecmp(key, "pdl"))
      papplCopyString(pdl, value, sizeof(pdl));
    else if (!strcasecmp(key, "product"))
      papplCopyString(product, value, sizeof(product));
    else if (!strcasecmp(key, "ty"))
      papplCopyString(ty, value, sizeof(ty));
  }

  // Synthesize values as needed...
  if (!cmd[0] && pdl[0])
  {
    size_t	i;			// Looping var
    char	*cmdptr,		// Pointer into CMD value
		*pdlptr,		// Pointer into pdl value
		mime[128],		// Current pdl MIME media type
		*mimeptr;		// Pointer into MIME media type
    static const char * const pdls[][2] =
    {					// MIME media type to command set mapping
      { "application/postscript", "PS" },
      { "application/vnd.canon-cpdl", "CPDL" },
      { "application/vnd.canon-lips", "LIPS" },
      { "application/vnd.hp-PCL", "PCL" },
      { "application/vnd.hp-PCLXL", "PCLXL" },
      { "application/vnd.ms-xpsdocument", "XPS" },
      { "image/jpeg", "JPEG" },
      { "image/pwg-raster", "PWGRaster" },
      { "image/tiff", "TIFF" },
      { "image/urf", "URF" }
    };

    for (pdlptr = pdl, cmdptr = cmd; *pdlptr;)
    {
      // Copy current MIME media type from pdl value...
      for (mimeptr = mime; *pdlptr && *pdlptr != ','; pdlptr ++)
      {
        if (mimeptr < (mime + sizeof(mime) - 1))
          *mimeptr++ = *pdlptr;
      }

      *mimeptr = '\0';

      if (*pdlptr)
        pdlptr ++;

      // See if it is a known MIME media type and map to the corresponding 1284
      // command-set name...
      for (i = 0; i < (sizeof(pdls) / sizeof(pdls[0])); i ++)
      {
	if (!strcasecmp(mime, pdls[i][0]))
	{
	  // MIME media type matches, append this CMD value...
	  if (cmdptr > cmd && cmdptr < (cmd + sizeof(cmd) - 1))
	    *cmdptr++ = ',';
	  papplCopyString(cmdptr, pdls[i][1], sizeof(cmd) - (size_t)(cmdptr - cmd));
	  cmdptr += strlen(cmdptr);
	}
      }
    }

    if (!strcmp(mfg, "EPSON"))
    {
      // Append ESC/P2 for EPSON printers...
      if (cmdptr > cmd)
        papplCopyString(cmdptr, ",ESCPL2", sizeof(cmd) - (size_t)(cmdptr - cmd));
      else
        papplCopyString(cmdptr, "ESCPL2", sizeof(cmd) - (size_t)(cmdptr - cmd));
    }
  }

  if (!ty[0] && product[0])
  {
    if (product[0] == '(')
    {
      papplCopyString(ty, product + 1, sizeof(ty));
      if ((ptr = product + strlen(product) - 1) >= product && *ptr == ')')
        *ptr = '\0';
    }
    else
      papplCopyString(ty, product, sizeof(ty));
  }

  if (!ty[0] && mfg[0] && mdl[0])
    snprintf(ty, sizeof(ty), "%s %s", mfg, mdl);

  if (!mfg[0] && ty[0])
  {
    papplCopyString(mfg, ty, sizeof(mfg));
    if ((ptr = strchr(mfg, ' ')) != NULL)
      *ptr = '\0';
  }

  if (!mdl[0] && ty[0])
  {
    if ((ptr = strchr(ty, ' ')) != NULL)
      papplCopyString(mdl, ptr + 1, sizeof(mdl));
    else
      papplCopyString(mdl, ty, sizeof(mdl));
  }

  snprintf(device_id, sizeof(device_id), "MFG:%s;MDL:%s;CMD:%s;", mfg, mdl, cmd);

  // Save the make and model and IEEE-1284 device ID...
  pthread_mutex_lock(&device->mutex);
  device->device_id      = strdup(device_id);
  device->make_and_model = strdup(ty);
  pthread_mutex_unlock(&device->mutex);
}


//
// 'pappl_dnssd_resolve_cb()' - Resolve a DNS-SD service.
//

#  ifdef HAVE_MDNSRESPONDER
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
  (void)flags;
  (void)interfaceIndex;
  (void)fullname;
  (void)txtLen;
  (void)txtRecord;

  _PAPPL_DEBUG("pappl_dnssd_resolve_cb(sdRef=%p, flags=0x%x, interfaceIndex=%u, errorCode=%d, fullname=\"%s\", host_name=\"%s\", port=%u, txtLen=%u, txtRecord=%p, context=%p)\n", sdRef, flags, interfaceIndex, errorCode, fullname, host_name, ntohs(port), txtLen, txtRecord, context);

  if (errorCode == kDNSServiceErr_NoError)
  {
    _pappl_socket_t *sock = (_pappl_socket_t *)context;
					// Socket

    sock->host = strdup(host_name);
    sock->port = ntohs(port);
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
#  endif // HAVE_MDNSRESPONDER


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
#endif // HAVE_DNSSD


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
  int			snmp_sock = -1;	// SNMP socket
  cups_len_t		last_count;	// Last devices count
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
  devices = cupsArrayNew((cups_array_cb_t)pappl_snmp_compare_devices, NULL, NULL, 0, NULL, (cups_afree_cb_t)pappl_snmp_free);

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
    _PAPPL_DEBUG("pappl_snmp_find: Sending SNMP device type get request to '%s'.\n", httpAddrGetString(&(addr->addr), temp, sizeof(temp)));

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
      if (last_count == cupsArrayGetCount(devices))
        break;

      last_count = cupsArrayGetCount(devices);
      _PAPPL_DEBUG("pappl_snmp_find: timeout=%d, last_count=%u\n", (int)(endtime - time(NULL)), (unsigned)last_count);
    }
  }

  _PAPPL_DEBUG("pappl_snmp_find: timeout=%d, last_count=%u\n", (int)(endtime - time(NULL)), (unsigned)last_count);

  // Report all of the devices we found...
  for (cur_device = (_pappl_snmp_dev_t *)cupsArrayGetFirst(devices); cur_device; cur_device = (_pappl_snmp_dev_t *)cupsArrayGetNext(devices))
  {
    char	info[256];		// Device description
    cups_len_t	num_did;		// Number of device ID keys/values
    cups_option_t *did;			// Device ID keys/values
    const char	*make,			// Manufacturer
		*model;			// Model name

    // Skip LPD (port 515) and IPP (port 631) since they can't be raw sockets...
    if (cur_device->port == 515 || cur_device->port == 631 || !cur_device->uri)
      continue;

    num_did = (cups_len_t)papplDeviceParseID(cur_device->device_id, &did);

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

      sock->host = strdup(httpAddrGetString(&cur_device->address, address_str, sizeof(address_str)));
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
#if _WIN32
  return (NULL);			// TODO: Implement WinSock equivalents

#else
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
#endif // _WIN32
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

  (void)device_info;
  (void)device_id;

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


  // Read the response data
  if (!_papplSNMPRead(fd, &packet, -1.0))
  {
    _papplDeviceError(err_cb, err_data, "Unable to read SNMP response data: %s", strerror(errno));
    return;
  }

  httpAddrGetString(&(packet.address), addrname, sizeof(addrname));

  // Look for the response status code in the SNMP message header
  if (packet.error)
  {
    _papplDeviceError(err_cb, err_data, "Bad SNMP packet from '%s': %s", addrname, packet.error);
    return;
  }

  _PAPPL_DEBUG("pappl_snmp_read_response: community=\"%s\"\n", packet.community);
  _PAPPL_DEBUG("pappl_snmp_read_response: request-id=%u\n", packet.request_id);
  _PAPPL_DEBUG("pappl_snmp_read_response: error-status=%d\n", packet.error_status);

  if (packet.error_status && packet.request_id != _PAPPL_SNMP_QUERY_DEVICE_TYPE)
    return;

  // Find a matching device in the cache
  for (device = (_pappl_snmp_dev_t *)cupsArrayGetFirst(devices); device; device = (_pappl_snmp_dev_t *)cupsArrayGetNext(devices))
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
// 'pappl_snmp_walk_cb()' - Update supply information.
//

static void
pappl_snmp_walk_cb(
    _pappl_snmp_t   *packet,		// I - SNMP packet
    _pappl_socket_t *sock)		// I - Socket device
{
  int	i, j,				// Looping vars
	element;			// Element in supply table
  char	*ptr;				// Pointer into colorant name
  static const pappl_supply_type_t types[] =
  {					// Supply types mapped from SNMP TC values
    PAPPL_SUPPLY_TYPE_OTHER,
    PAPPL_SUPPLY_TYPE_UNKNOWN,
    PAPPL_SUPPLY_TYPE_TONER,
    PAPPL_SUPPLY_TYPE_WASTE_TONER,
    PAPPL_SUPPLY_TYPE_INK,
    PAPPL_SUPPLY_TYPE_INK_CARTRIDGE,
    PAPPL_SUPPLY_TYPE_INK_RIBBON,
    PAPPL_SUPPLY_TYPE_WASTE_INK,
    PAPPL_SUPPLY_TYPE_OPC,
    PAPPL_SUPPLY_TYPE_DEVELOPER,
    PAPPL_SUPPLY_TYPE_FUSER_OIL,
    PAPPL_SUPPLY_TYPE_SOLID_WAX,
    PAPPL_SUPPLY_TYPE_RIBBON_WAX,
    PAPPL_SUPPLY_TYPE_WASTE_WAX,
    PAPPL_SUPPLY_TYPE_FUSER,
    PAPPL_SUPPLY_TYPE_CORONA_WIRE,
    PAPPL_SUPPLY_TYPE_FUSER_OIL_WICK,
    PAPPL_SUPPLY_TYPE_CLEANER_UNIT,
    PAPPL_SUPPLY_TYPE_FUSER_CLEANING_PAD,
    PAPPL_SUPPLY_TYPE_TRANSFER_UNIT,
    PAPPL_SUPPLY_TYPE_TONER_CARTRIDGE,
    PAPPL_SUPPLY_TYPE_FUSER_OILER,
    PAPPL_SUPPLY_TYPE_WATER,
    PAPPL_SUPPLY_TYPE_WASTE_WATER,
    PAPPL_SUPPLY_TYPE_GLUE_WATER_ADDITIVE,
    PAPPL_SUPPLY_TYPE_WASTE_PAPER,
    PAPPL_SUPPLY_TYPE_BINDING_SUPPLY,
    PAPPL_SUPPLY_TYPE_BANDING_SUPPLY,
    PAPPL_SUPPLY_TYPE_STITCHING_WIRE,
    PAPPL_SUPPLY_TYPE_SHRINK_WRAP,
    PAPPL_SUPPLY_TYPE_PAPER_WRAP,
    PAPPL_SUPPLY_TYPE_STAPLES,
    PAPPL_SUPPLY_TYPE_INSERTS,
    PAPPL_SUPPLY_TYPE_COVERS
  };


  if (_papplSNMPIsOIDPrefixed(packet, prtMarkerColorantValue) && packet->object_type == _PAPPL_ASN1_OCTET_STRING)
  {
    // Get colorant...
    i = packet->object_name[sizeof(prtMarkerColorantValue) / sizeof(prtMarkerColorantValue[0])];

    _PAPPL_DEBUG("pappl_snmp_walk_cb: prtMarkerColorantValue.1.%d = \"%s\"\n", i,
            (char *)packet->object_value.string.bytes);

    // Strip "ink" or "toner" off the end of the colorant name...
    if ((ptr = strstr((char *)packet->object_value.string.bytes, " ink")) != NULL)
      *ptr = '\0';
    else if ((ptr = strstr((char *)packet->object_value.string.bytes, " toner")) != NULL)
      *ptr = '\0';

    // Map to each supply using this colorant...
    for (j = 0; j < sock->num_supplies; j ++)
    {
      if (sock->colorants[j] == i)
        sock->supplies[j].color = _papplSupplyColorValue((char *)packet->object_value.string.bytes);
    }
  }
  else if (_papplSNMPIsOIDPrefixed(packet, prtMarkerSuppliesEntry))
  {
    // Get indices...
    element = packet->object_name[sizeof(prtMarkerSuppliesEntry) / sizeof(prtMarkerSuppliesEntry[0]) - 1];
    i       = packet->object_name[sizeof(prtMarkerSuppliesEntry) / sizeof(prtMarkerSuppliesEntry[0]) + 1];

    _PAPPL_DEBUG("pappl_snmp_walk_cb: prtMarkerSuppliesEntry.%d.%d\n", element, i);

    if (element < 1 || i < 1 || i > _PAPPL_MAX_SNMP_SUPPLY)
      return;

    if (i > sock->num_supplies)
      sock->num_supplies = i;

    i --;

    switch (element)
    {
      case 3 : // prtMarkerSuppliesColorantIndex
          if (packet->object_type == _PAPPL_ASN1_INTEGER)
            sock->colorants[i] = packet->object_value.integer;
	  break;
      case 4 : // prtMarkerSuppliesClass
          if (packet->object_type == _PAPPL_ASN1_INTEGER)
            sock->supplies[i].is_consumed = packet->object_value.integer == _PAPPL_TC_supplyThatIsConsumed;
          break;
      case 5 : // prtMarkerSuppliesType
          if (packet->object_type == _PAPPL_ASN1_INTEGER && packet->object_value.integer >= 1 && packet->object_value.integer <= (int)(sizeof(types) / sizeof(types[0])))
	    sock->supplies[i].type = types[packet->object_value.integer - 1];
          break;
      case 6 : // prtMarkerSuppliesDescription
          if (packet->object_type != _PAPPL_ASN1_OCTET_STRING)
            break;

	  switch (sock->charset)
	  {
	    case _PAPPL_TC_csASCII :
	    case _PAPPL_TC_csUTF8 :
	    case _PAPPL_TC_csUnicodeASCII :
		papplCopyString(sock->supplies[i].description, (char *)packet->object_value.string.bytes, sizeof(sock->supplies[i].description));
		break;

	    case _PAPPL_TC_csISOLatin1 :
	    case _PAPPL_TC_csUnicodeLatin1 :
		cupsCharsetToUTF8((cups_utf8_t *)sock->supplies[i].description, (char *)packet->object_value.string.bytes, (cups_len_t)sizeof(sock->supplies[i].description), CUPS_ENCODING_ISO8859_1);
		break;

	    case _PAPPL_TC_csShiftJIS :
	    case _PAPPL_TC_csWindows31J : /* Close enough for our purposes */
		cupsCharsetToUTF8((cups_utf8_t *)sock->supplies[i].description, (char *)packet->object_value.string.bytes, (cups_len_t)sizeof(sock->supplies[i].description), CUPS_ENCODING_JIS_X0213);
		break;

	    case _PAPPL_TC_csUCS4 :
	    case _PAPPL_TC_csUTF32 :
	    case _PAPPL_TC_csUTF32BE :
	    case _PAPPL_TC_csUTF32LE :
		cupsUTF32ToUTF8((cups_utf8_t *)sock->supplies[i].description, (cups_utf32_t *)packet->object_value.string.bytes, (cups_len_t)sizeof(sock->supplies[i].description));
		break;

	    case _PAPPL_TC_csUnicode :
	    case _PAPPL_TC_csUTF16BE :
	    case _PAPPL_TC_csUTF16LE :
		utf16_to_utf8(sock->supplies[i].description, packet->object_value.string.bytes, packet->object_value.string.num_bytes, sizeof(sock->supplies[i].description), sock->charset == _PAPPL_TC_csUTF16LE);
		break;

	    default :
		// If we get here, the printer is using an unknown character set and
		// we just want to copy characters that look like ASCII...
		{
		  char *src, *dst, *dstend;	// Pointers into strings

		  for (src = (char *)packet->object_value.string.bytes,
			   dst = sock->supplies[i].description, dstend = dst + sizeof(sock->supplies[i].description) - 1; *src && dst < dstend; src ++)
		  {
		    if ((*src & 0x80) || *src < ' ' || *src == 0x7f)
		      *dst++ = '?';
		    else
		      *dst++ = *src;
		  }

		  *dst = '\0';
		}
		break;
	  }
          break;
      case 7 : // prtMarkerSuppliesSupplyUnit
          if (packet->object_type == _PAPPL_ASN1_INTEGER && packet->object_value.integer == _PAPPL_TC_percent)
            sock->max_capacities[i] = 100;
          break;
      case 8 : // prtMarkerSuppliesMaxCapacity
          if (packet->object_type == _PAPPL_ASN1_INTEGER && sock->max_capacities[i] == 0 && packet->object_value.integer > 0)
	    sock->max_capacities[i] = packet->object_value.integer;
          break;
      case 9 : // prtMarkerSuppliesLevel
          if (packet->object_type == _PAPPL_ASN1_INTEGER)
	    sock->levels[i] = packet->object_value.integer;
          break;
    }
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

#if _WIN32
  closesocket(sock->fd);
  closesocket(sock->snmp_fd);
#else
  close(sock->fd);
  close(sock->snmp_fd);
#endif // _WIN32

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
  _pappl_socket_t	*sock;		// Socket device
  struct pollfd		data;		// poll() data
  _pappl_snmp_t		packet;		// Decoded packet


  *buffer = '\0';

  // Get the socket data...
  if ((sock = papplDeviceGetData(device)) == NULL)
    return (NULL);

  // Send queries to the printer...
  _papplSNMPWrite(sock->snmp_fd, &(sock->addr->addr), _PAPPL_SNMP_VERSION_1, _PAPPL_SNMP_COMMUNITY, _PAPPL_ASN1_GET_REQUEST, _PAPPL_SNMP_QUERY_DEVICE_ID, PWGPPMDeviceIdOID);
  _papplSNMPWrite(sock->snmp_fd, &(sock->addr->addr), _PAPPL_SNMP_VERSION_1, _PAPPL_SNMP_COMMUNITY, _PAPPL_ASN1_GET_REQUEST, _PAPPL_SNMP_QUERY_DEVICE_ID, HPDeviceIDOID);
  _papplSNMPWrite(sock->snmp_fd, &(sock->addr->addr), _PAPPL_SNMP_VERSION_1, _PAPPL_SNMP_COMMUNITY, _PAPPL_ASN1_GET_REQUEST, _PAPPL_SNMP_QUERY_DEVICE_ID, LexmarkDeviceIdOID);
  _papplSNMPWrite(sock->snmp_fd, &(sock->addr->addr), _PAPPL_SNMP_VERSION_1, _PAPPL_SNMP_COMMUNITY, _PAPPL_ASN1_GET_REQUEST, _PAPPL_SNMP_QUERY_DEVICE_ID, ZebraDeviceIDOID);

  // Wait up to 10 seconds to get a response...
  data.fd     = sock->snmp_fd;
  data.events = POLLIN;

  while (poll(&data, 1, 10000) > 0)
  {
    if (!_papplSNMPRead(sock->snmp_fd, &packet, -1.0))
      continue;

    if (packet.error || packet.error_status)
      continue;

    if (packet.object_type == _PAPPL_ASN1_OCTET_STRING)
    {
      char  *ptr;			// Pointer into device ID

      for (ptr = (char *)packet.object_value.string.bytes; *ptr; ptr ++)
      {
	if (*ptr == '\n')		// A lot of bad printers put a newline
	  *ptr = ';';
      }

      strncpy(buffer, (char *)packet.object_value.string.bytes, bufsize - 1);
      buffer[bufsize - 1] = '\0';
      break;
    }
  }

  return (*buffer ? buffer : NULL);
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

  sock->snmp_fd      = -1;
  sock->charset      = -1;
  sock->num_supplies = -1;

  // Split apart the URI...
  httpSeparateURI(HTTP_URI_CODING_ALL, device_uri, scheme, sizeof(scheme), userpass, sizeof(userpass), host, sizeof(host), &port, resource, sizeof(resource));

  if ((options = strchr(resource, '?')) != NULL)
    *options++ = '\0';

  if (!strcmp(scheme, "dnssd"))
  {
    // DNS-SD discovered device
#ifdef HAVE_DNSSD
    int			i;		// Looping var
    char		srvname[256],	// Service name
			*type,		// Service type
			*domain;	// Domain
    _pappl_dns_sd_t	master;		// DNS-SD context
#  ifdef HAVE_MDNSRESPONDER
    int			error;		// Error code, if any
    DNSServiceRef	resolver;	// Resolver
#  else
    AvahiServiceResolver *resolver;	// Resolver
#  endif // HAVE_MDNSRESPONDER

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

      _PAPPL_DEBUG("pappl_socket_open: host='%s', srvname='%s', type='%s', domain='%s'\n", host, srvname, type, domain);

#  ifdef HAVE_MDNSRESPONDER
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
#  endif // HAVE_MDNSRESPONDER

      // Wait up to 30 seconds for the resolve to complete...
      for (i = 0; i < 30000 && !sock->host; i ++)
	usleep(1000);

#  ifdef HAVE_MDNSRESPONDER
      DNSServiceRefDeallocate(resolver);
#  else
      _papplDNSSDLock();
      avahi_service_resolver_free(resolver);
      _papplDNSSDUnlock();
#  endif // HAVE_MDNSRESPONDER

      if (!sock->host)
      {
	papplDeviceError(device, "Unable to resolve '%s'.", device_uri);
	goto error;
      }
    }
#endif // HAVE_DNSSD
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
    papplDeviceError(device, "Unable to lookup '%s:%d': %s", sock->host, sock->port, cupsGetErrorString());
    goto error;
  }

  sock->fd   = -1;
  sock->addr = httpAddrConnect(sock->list, &sock->fd, 30000, NULL);

  if (sock->fd < 0)
  {
    papplDeviceError(device, "Unable to connect to '%s:%d': %s", sock->host, sock->port, cupsGetErrorString());
    goto error;
  }

  // Open SNMP socket...
  if ((sock->snmp_fd = _papplSNMPOpen(httpAddrGetFamily(&(sock->addr->addr)))) < 0)
  {
    papplDeviceError(device, "Unable to open SNMP socket.");
    return (false);
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

  while ((nfds = poll(&data, 1, 10000)) < 0)
  {
    if (errno != EINTR && errno != EAGAIN)
      break;
  }

  if (nfds < 1 || !(data.revents & POLLIN))
    return (-1);

  // Read data from the socket, protecting against signals and busy kernels...
#if _WIN32
  while ((count = recv(sock->fd, buffer, (int)bytes, 0)) < 0)
#else
  while ((count = read(sock->fd, buffer, bytes)) < 0)
#endif // _WIN32
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
  _pappl_socket_t	*sock;		// Socket device
  pappl_preason_t	reasons = PAPPL_PREASON_NONE;
					// "printer-state-reasons" values
  _pappl_snmp_t		packet;		// SNMP packet
  int			state;		// State bits


  // Get the device data...
  if ((sock = papplDeviceGetData(device)) == NULL)
    return (0);

  if (!_papplSNMPWrite(sock->snmp_fd, &(sock->addr->addr), _PAPPL_SNMP_VERSION_1, _PAPPL_SNMP_COMMUNITY, _PAPPL_ASN1_GET_REQUEST, 1, hrPrinterDetectedErrorState))
    return (reasons);

  if (!_papplSNMPRead(sock->snmp_fd, &packet, _PAPPL_SNMP_TIMEOUT) || packet.object_type != _PAPPL_ASN1_OCTET_STRING)
    return (reasons);

  if (packet.object_value.string.num_bytes == 2)
    state = (packet.object_value.string.bytes[0] << 8) | packet.object_value.string.bytes[1];
  else if (packet.object_value.string.num_bytes == 1)
    state = (packet.object_value.string.bytes[0] << 8);
  else
    state = 0;

  if (state & (_PAPPL_TC_noPaper | _PAPPL_TC_inputTrayEmpty))
    reasons |= PAPPL_PREASON_MEDIA_EMPTY;
  if (state & _PAPPL_TC_doorOpen)
    reasons |= PAPPL_PREASON_DOOR_OPEN;
  if (state & _PAPPL_TC_inputTrayMissing)
    reasons |= PAPPL_PREASON_INPUT_TRAY_MISSING;

  return (reasons);
}


//
// 'pappl_socket_supplies()' - Query supply levels via SNMP.
//

static int				// O - Number of supplies
pappl_socket_supplies(
    pappl_device_t *device,		// I - Device
    int            max_supplies,	// I - Maximum number of supply levels
    pappl_supply_t *supplies)		// I - Supply levels
{
  _pappl_socket_t	*sock;		// Socket device
  int			i;		// Looping var
#ifdef DEBUG
  char			temp[1024];	// OID string
#endif // DEBUG


  // Get the device data...
  _PAPPL_DEBUG("pappl_socket_supplies(device=%p, max_supplies=%d, supplies=%p)\n", device, max_supplies, supplies);

  if ((sock = papplDeviceGetData(device)) == NULL)
    return (0);

  // Get the current character set as needed...
  if (sock->charset < 0)
  {
    _pappl_snmp_t packet;		// SNMP packet
    int		oid[_PAPPL_SNMP_MAX_OID];
					// OID for property

    if (!_papplSNMPWrite(sock->snmp_fd, &(sock->addr->addr), _PAPPL_SNMP_VERSION_1, _PAPPL_SNMP_COMMUNITY, _PAPPL_ASN1_GET_REQUEST, 1, prtGeneralCurrentLocalization))
    {
      _PAPPL_DEBUG("pappl_socket_supplies: Unable to query prtGeneralCurrentLocalization\n");
      return (0);
    }

    if (!_papplSNMPRead(sock->snmp_fd, &packet, _PAPPL_SNMP_TIMEOUT) || packet.object_type != _PAPPL_ASN1_INTEGER)
    {
      _PAPPL_DEBUG("pappl_socket_supplies: Unable to read prtGeneralCurrentLocalization value.\n");
      return (0);
    }

    _papplSNMPCopyOID(oid, prtLocalizationCharacterSet, _PAPPL_SNMP_MAX_OID);
    oid[sizeof(prtLocalizationCharacterSet) / sizeof(prtLocalizationCharacterSet[0]) - 1] = packet.object_value.integer;
    oid[sizeof(prtLocalizationCharacterSet) / sizeof(prtLocalizationCharacterSet[0])] = 1;
    oid[sizeof(prtLocalizationCharacterSet) / sizeof(prtLocalizationCharacterSet[0]) + 1] = -1;

    _PAPPL_DEBUG("pappl_socket_supplies: Looking up %s.\n", _papplSNMPOIDToString(oid, temp, sizeof(temp)));

    if (!_papplSNMPWrite(sock->snmp_fd, &(sock->addr->addr), _PAPPL_SNMP_VERSION_1, _PAPPL_SNMP_COMMUNITY, _PAPPL_ASN1_GET_REQUEST, 1, oid))
    {
      _PAPPL_DEBUG("pappl_socket_supplies: Unable to query prtLocalizationCharacterSet.%d\n", packet.object_value.integer);
      return (0);
    }

    if (!_papplSNMPRead(sock->snmp_fd, &packet, _PAPPL_SNMP_TIMEOUT) || packet.object_type != _PAPPL_ASN1_INTEGER)
    {
      _PAPPL_DEBUG("pappl_socket_supplies: Unable to read prtLocalizationCharacterSet value.\n");
      return (0);
    }

    sock->charset = packet.object_value.integer;
    _PAPPL_DEBUG("pappl_socket_supplies: charset=%d\n", sock->charset);
  }

  // Query supplies...
  if (sock->num_supplies > 0)
  {
    // Just update the levels...
    _papplSNMPWalk(sock->snmp_fd, &(sock->addr->addr), _PAPPL_SNMP_VERSION_1, _PAPPL_SNMP_COMMUNITY, prtMarkerSuppliesLevel, _PAPPL_SNMP_TIMEOUT, (_pappl_snmp_cb_t)pappl_snmp_walk_cb, sock);
  }
  else
  {
    // Query all of the supply elements...
    _papplSNMPWalk(sock->snmp_fd, &(sock->addr->addr), _PAPPL_SNMP_VERSION_1, _PAPPL_SNMP_COMMUNITY, prtMarkerSuppliesEntry, _PAPPL_SNMP_TIMEOUT, (_pappl_snmp_cb_t)pappl_snmp_walk_cb, sock);
    _papplSNMPWalk(sock->snmp_fd, &(sock->addr->addr), _PAPPL_SNMP_VERSION_1, _PAPPL_SNMP_COMMUNITY, prtMarkerColorantValue, _PAPPL_SNMP_TIMEOUT, (_pappl_snmp_cb_t)pappl_snmp_walk_cb, sock);
  }

  // Update levels...
  for (i = 0; i < sock->num_supplies; i ++)
  {
    int percent;			// Supply level

    if (sock->max_capacities[i] > 0 && sock->levels[i]  >= 0)
      percent = 100 * sock->levels[i] / sock->max_capacities[i];
    else if (sock->levels[i] >= 0 && sock->levels[i] <= 100)
      percent = sock->levels[i];
    else
      percent = 50;

    if (sock->supplies[i].is_consumed)
      sock->supplies[i].level = percent;
    else
      sock->supplies[i].level = 100 - percent;
  }

  // Return the supplies that are cached in the socket device...
  if (sock->num_supplies > 0)
  {
    if (sock->num_supplies > max_supplies)
      memcpy(supplies, sock->supplies, (size_t)max_supplies * sizeof(pappl_supply_t));
    else
      memcpy(supplies, sock->supplies, (size_t)sock->num_supplies * sizeof(pappl_supply_t));
  }

  return (sock->num_supplies);
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
#if _WIN32
    if ((written = send(sock->fd, ptr, (int)(bytes - (size_t)count), 0)) < 0)
#else
    if ((written = write(sock->fd, ptr, bytes - (size_t)count)) < 0)
#endif // _WIN32
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


//
// 'utf16_to_utf8()' - Convert UTF-16 text to UTF-8.
//

static void
utf16_to_utf8(
    char                *dst,		// I - Destination buffer
    const unsigned char *src,		// I - Source string
    size_t		srcsize,	// I - Size of source string
    size_t              dstsize,	// I - Size of destination buffer
    bool                le)		// I - Source is little-endian?
{
  cups_utf32_t	ch,			// Current character
		temp[_PAPPL_SNMP_MAX_STRING],
					// UTF-32 string
		*ptr;			// Pointer into UTF-32 string


  for (ptr = temp; srcsize >= 2;)
  {
    if (le)
      ch = (cups_utf32_t)(src[0] | (src[1] << 8));
    else
      ch = (cups_utf32_t)((src[0] << 8) | src[1]);

    src += 2;
    srcsize -= 2;

    if (ch >= 0xd800 && ch <= 0xdbff && srcsize >= 2)
    {
      // Multi-word UTF-16 char...
      cups_utf32_t lch;			// Lower word


      if (le)
	lch = (cups_utf32_t)(src[0] | (src[1] << 8));
      else
	lch = (cups_utf32_t)((src[0] << 8) | src[1]);

      if (lch >= 0xdc00 && lch <= 0xdfff)
      {
	src += 2;
	srcsize -= 2;

	ch = (((ch & 0x3ff) << 10) | (lch & 0x3ff)) + 0x10000;
      }
    }

    if (ptr < (temp + _PAPPL_SNMP_MAX_STRING - 1))
      *ptr++ = ch;
  }

  *ptr = '\0';

  cupsUTF32ToUTF8((cups_utf8_t *)dst, temp, (cups_len_t)dstsize);
}
