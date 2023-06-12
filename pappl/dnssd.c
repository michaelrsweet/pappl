//
// DNS-SD support for the Printer Application Framework
//
// Copyright © 2019-2022 by Michael R Sweet.
// Copyright © 2010-2019 by Apple Inc.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

//
// Include necessary headers...
//

#include "pappl-private.h"


//
// Constants...
//

#ifdef HAVE_AVAHI
#  define AVAHI_DNS_TYPE_LOC 29		// Per RFC 1876
#endif // HAVE_AVAHI


//
// Local globals...
//

static int		pappl_dns_sd_host_name_changes = 0;
					// Number of host name changes/collisions
static _pappl_dns_sd_t	pappl_dns_sd_master = NULL;
					// DNS-SD master reference
static pthread_mutex_t	pappl_dns_sd_mutex = PTHREAD_MUTEX_INITIALIZER;
					// DNS-SD master mutex
#ifdef HAVE_AVAHI
static AvahiThreadedPoll *pappl_dns_sd_poll = NULL;
					// Avahi background thread
#endif // HAVE_AVAHI


//
// Local functions...
//

static void		dns_sd_geo_to_loc(const char *geo, unsigned char loc[16]);
#ifdef HAVE_MDNSRESPONDER
static void DNSSD_API	dns_sd_printer_callback(DNSServiceRef sdRef, DNSServiceFlags flags, DNSServiceErrorType errorCode, const char *name, const char *regtype, const char *domain, pappl_printer_t *printer);
static void		*dns_sd_run(void *data);
static void DNSSD_API	dns_sd_system_callback(DNSServiceRef sdRef, DNSServiceFlags flags, DNSServiceErrorType errorCode, const char *name, const char *regtype, const char *domain, pappl_system_t *system);
#elif defined(HAVE_AVAHI)
static void		dns_sd_client_cb(AvahiClient *c, AvahiClientState state, void *data);
static void		dns_sd_printer_callback(AvahiEntryGroup *p, AvahiEntryGroupState state, pappl_printer_t *printer);
static void		dns_sd_system_callback(AvahiEntryGroup *p, AvahiEntryGroupState state, pappl_system_t *system);
#endif // HAVE_MDNSRESPONDER


//
// '_papplDNSSDGetHostChanges()' - Get the number of host name changes/collisions so far.
//

int					// O - Number of host name changes/collisions
_papplDNSSDGetHostChanges(void)
{
  return (pappl_dns_sd_host_name_changes);
}


//
// '_papplDNSSDInit()' - Initialize DNS-SD services.
//

_pappl_dns_sd_t				// O - DNS-SD master reference
_papplDNSSDInit(
    pappl_system_t *system)		// I - System
{
#ifdef HAVE_MDNSRESPONDER
  int		error;			// Error code, if any
  pthread_t	tid;			// Thread ID


  pthread_mutex_lock(&pappl_dns_sd_mutex);

  if (pappl_dns_sd_master)
  {
    pthread_mutex_unlock(&pappl_dns_sd_mutex);
    return (pappl_dns_sd_master);
  }

  if ((error = DNSServiceCreateConnection(&pappl_dns_sd_master)) == kDNSServiceErr_NoError)
  {
    if (pthread_create(&tid, NULL, dns_sd_run, system))
    {
      papplLog(system, PAPPL_LOGLEVEL_ERROR, "Unable to create DNS-SD thread: %s", strerror(errno));
      DNSServiceRefDeallocate(pappl_dns_sd_master);
      pappl_dns_sd_master = NULL;
    }
    else
      pthread_detach(tid);
  }
  else
  {
    papplLog(system, PAPPL_LOGLEVEL_ERROR, "Unable to initialize DNS-SD: %s", _papplDNSSDStrError(error));
    DNSServiceRefDeallocate(pappl_dns_sd_master);
    pappl_dns_sd_master = NULL;
  }

  pthread_mutex_unlock(&pappl_dns_sd_mutex);

#elif defined(HAVE_AVAHI)
  int error;				// Error code, if any


  pthread_mutex_lock(&pappl_dns_sd_mutex);

  if (pappl_dns_sd_master)
  {
    pthread_mutex_unlock(&pappl_dns_sd_mutex);
    return (pappl_dns_sd_master);
  }

  if ((pappl_dns_sd_poll = avahi_threaded_poll_new()) == NULL)
  {
    // Unable to create the background thread...
    papplLog(system, PAPPL_LOGLEVEL_ERROR, "Unable to initialize DNS-SD thread: %s", strerror(errno));
  }
  else if ((pappl_dns_sd_master = avahi_client_new(avahi_threaded_poll_get(pappl_dns_sd_poll), AVAHI_CLIENT_NO_FAIL, (AvahiClientCallback)dns_sd_client_cb, system, &error)) == NULL)
  {
    // Unable to create the client...
    papplLog(system, PAPPL_LOGLEVEL_ERROR, "Unable to initialize DNS-SD: %s", _papplDNSSDStrError(error));
    avahi_threaded_poll_free(pappl_dns_sd_poll);
    pappl_dns_sd_poll = NULL;
  }
  else
  {
    // Start the background thread...
    avahi_threaded_poll_start(pappl_dns_sd_poll);
  }

  pthread_mutex_unlock(&pappl_dns_sd_mutex);
#endif // HAVE_MDNSRESPONDER

  return (pappl_dns_sd_master);
}


//
// '_papplDNSSDLock()' - Grab a lock to make DNS-SD changes.
//

void
_papplDNSSDLock(void)
{
#ifdef HAVE_AVAHI
  if (pappl_dns_sd_poll)
    avahi_threaded_poll_lock(pappl_dns_sd_poll);
#endif // HAVE_AVAHI
}


//
// '_papplDNSSDStrError()' - Return a string for the given DNS-SD error code.
//

const char *				// O - Error message
_papplDNSSDStrError(int error)		// I - Error code
{
#ifdef HAVE_MDNSRESPONDER
  switch (error)
  {
    case kDNSServiceErr_NoError :
        return ("No error");

    case kDNSServiceErr_Unknown :
    default :
        return ("Unknown error");

    case kDNSServiceErr_NoSuchName :
        return ("Name not found");

    case kDNSServiceErr_NoMemory :
        return ("Out of memory");

    case kDNSServiceErr_BadParam :
        return ("Bad parameter");

    case kDNSServiceErr_BadReference :
        return ("Bad service reference");

    case kDNSServiceErr_BadState :
        return ("Bad state");

    case kDNSServiceErr_BadFlags :
        return ("Bad flags argument");

    case kDNSServiceErr_Unsupported :
        return ("Unsupported feature");

    case kDNSServiceErr_NotInitialized :
        return ("Not initialized");

    case kDNSServiceErr_AlreadyRegistered :
        return ("Name already registered");

    case kDNSServiceErr_NameConflict :
        return ("Name conflicts");

    case kDNSServiceErr_Invalid :
        return ("Invalid argument");

    case kDNSServiceErr_Firewall :
        return ("Firewall prevents access");

    case kDNSServiceErr_Incompatible :
        return ("Client library incompatible with background daemon");

    case kDNSServiceErr_BadInterfaceIndex :
        return ("Bad interface index");

    case kDNSServiceErr_Refused :
        return ("Connection refused");

    case kDNSServiceErr_NoSuchRecord :
        return ("DNS record not found");

    case kDNSServiceErr_NoAuth :
        return ("No authoritative answer");

    case kDNSServiceErr_NoSuchKey :
        return ("TXT record key not found");

    case kDNSServiceErr_NATTraversal :
        return ("Unable to traverse via NAT");

    case kDNSServiceErr_DoubleNAT :
        return ("Double NAT is in use");

    case kDNSServiceErr_BadTime :
        return ("Bad time value");

    case kDNSServiceErr_BadSig :
        return ("Bad signal");

    case kDNSServiceErr_BadKey :
        return ("Bad TXT record key");

    case kDNSServiceErr_Transient :
        return ("Transient error");

    case kDNSServiceErr_ServiceNotRunning :
        return ("Background daemon not running");

    case kDNSServiceErr_NATPortMappingUnsupported :
        return ("NAT doesn't support PCP, NAT-PMP or UPnP");

    case kDNSServiceErr_NATPortMappingDisabled :
        return ("NAT supports PCP, NAT-PMP or UPnP, but it's disabled by the administrator");

    case kDNSServiceErr_NoRouter :
        return ("No router configured, probably no network connectivity");

    case kDNSServiceErr_PollingMode :
        return ("Polling error");

    case kDNSServiceErr_Timeout :
        return ("Timeout");
#if !_WIN32
    case kDNSServiceErr_DefunctConnection :
        return ("Connection lost");
#endif // !_WIN32
  }

#elif defined(HAVE_AVAHI)
  return (avahi_strerror(error));

#else
  return ("");
#endif // HAVE_MDNSRESPONDER
}


//
// '_papplDNSSDUnlock()' - Release a lock after making DNS-SD changes.
//

void
_papplDNSSDUnlock(void)
{
#ifdef HAVE_AVAHI
  if (pappl_dns_sd_poll)
    avahi_threaded_poll_unlock(pappl_dns_sd_poll);
#endif // HAVE_AVAHI
}


//
// '_papplPrinterRegisterDNSSDNoLock()' - Register a printer's DNS-SD service.
//

bool					// O - `true` on success, `false` on failure
_papplPrinterRegisterDNSSDNoLock(
    pappl_printer_t *printer)		// I - Printer
{
  bool			ret = true;	// Return value
#ifdef HAVE_DNSSD
  pappl_system_t	*system = printer->system;
					// System
  uint32_t		if_index;	// Interface index
  _pappl_txt_t		txt;		// DNS-SD TXT record
  cups_len_t		i,		// Looping var
			count;		// Number of values
  ipp_attribute_t	*color_supported,
			*document_format_supported,
			*printer_kind,
			*printer_uuid,
			*urf_supported;	// Printer attributes
  const char		*value;		// Value string
  char			adminurl[246],	// Admin URL
			formats[252],	// List of supported formats
			kind[251],	// List of printer-kind values
			urf[252],	// List of supported URF values
			*ptr;		// Pointer into string
  char			regtype[256];	// DNS-SD service type
  char			product[248];	// Make and model (legacy)
  int			max_width;	// Maximum media width (legacy)
  const char		*papermax;	// PaperMax string value (legacy)
#  ifdef HAVE_MDNSRESPONDER
  DNSServiceErrorType	error;		// Error from mDNSResponder
#  else
  int			error;		// Error from Avahi
  char			fullname[256];	// Full service name
#  endif // HAVE_MDNSRESPONDER
  _pappl_dns_sd_t	master;		// DNS-SD master reference


  if (!printer->dns_sd_name || !printer->system->is_running)
    return (false);

  papplLogPrinter(printer, PAPPL_LOGLEVEL_DEBUG, "Registering DNS-SD name '%s' on '%s'", printer->dns_sd_name, printer->system->hostname);

#  ifdef HAVE_MDNSRESPONDER
  if_index = !strcmp(system->hostname, "localhost") ? kDNSServiceInterfaceIndexLocalOnly : kDNSServiceInterfaceIndexAny;
#  else
  if_index = !strcmp(system->hostname, "localhost") ? if_nametoindex("lo") : AVAHI_IF_UNSPEC;
#  endif // HAVE_MDNSRESPONDER

  // Get attributes and values for the TXT record...
  color_supported           = ippFindAttribute(printer->driver_attrs, "color-supported", IPP_TAG_BOOLEAN);
  document_format_supported = ippFindAttribute(printer->driver_attrs, "document-format-supported", IPP_TAG_MIMETYPE);
  printer_kind              = ippFindAttribute(printer->driver_attrs, "printer-kind", IPP_TAG_KEYWORD);
  printer_uuid              = ippFindAttribute(printer->attrs, "printer-uuid", IPP_TAG_URI);
  urf_supported             = ippFindAttribute(printer->driver_attrs, "urf-supported", IPP_TAG_KEYWORD);

  for (i = 0, count = ippGetCount(document_format_supported), ptr = formats; i < count; i ++)
  {
    value = ippGetString(document_format_supported, i, NULL);

    if (!strcasecmp(value, "application/octet-stream"))
      continue;

    if (ptr > formats && ptr < (formats + sizeof(formats) - 1))
      *ptr++ = ',';

    papplCopyString(ptr, value, sizeof(formats) - (size_t)(ptr - formats));
    ptr += strlen(ptr);

    if (ptr >= (formats + sizeof(formats) - 1))
      break;
  }

  kind[0] = '\0';
  for (i = 0, count = ippGetCount(printer_kind), ptr = kind; i < count; i ++)
  {
    value = ippGetString(printer_kind, i, NULL);

    if (ptr > kind && ptr < (kind + sizeof(kind) - 1))
      *ptr++ = ',';

    papplCopyString(ptr, value, sizeof(kind) - (size_t)(ptr - kind));
    ptr += strlen(ptr);

    if (ptr >= (kind + sizeof(kind) - 1))
      break;
  }

  snprintf(product, sizeof(product), "(%s)", printer->driver_data.make_and_model);

  for (i = 0, max_width = 0; i < (cups_len_t)printer->driver_data.num_media; i ++)
  {
    pwg_media_t *media = pwgMediaForPWG(printer->driver_data.media[i]);
					// Current media size

    if (media && media->width > max_width)
      max_width = media->width;
  }

  if (max_width < 21000)
    papermax = "<legal-A4";
  else if (max_width < 29700)
    papermax = "legal-A4";
  else if (max_width < 42000)
    papermax = "tabloid-A3";
  else if (max_width < 59400)
    papermax = "isoC-A2";
  else
    papermax = ">isoC-A2";

  urf[0] = '\0';
  for (i = 0, count = ippGetCount(urf_supported), ptr = urf; i < count; i ++)
  {
    value = ippGetString(urf_supported, i, NULL);

    if (ptr > urf && ptr < (urf + sizeof(urf) - 1))
      *ptr++ = ',';

    papplCopyString(ptr, value, sizeof(urf) - (size_t)(ptr - urf));
    ptr += strlen(ptr);

    if (ptr >= (urf + sizeof(urf) - 1))
      break;
  }

  httpAssembleURIf(HTTP_URI_CODING_ALL, adminurl, sizeof(adminurl), "http", NULL, printer->system->hostname, printer->system->port, "%s/", printer->uriname);

  if (printer->geo_location)
    dns_sd_geo_to_loc(printer->geo_location, printer->dns_sd_loc);

  // Rename the service as needed...
  if (printer->dns_sd_collision)
  {
    char	new_dns_sd_name[256];	// New DNS-SD name
    const char	*serial = strstr(printer->device_uri, "?serial=");
					// Serial number
    const char	*uuid = ippGetString(printer_uuid, 0, NULL);
					// "printer-uuid" value

    printer->dns_sd_serial ++;

    if (printer->dns_sd_serial == 1)
    {
      if (printer->system->options & PAPPL_SOPTIONS_DNSSD_HOST)
	snprintf(new_dns_sd_name, sizeof(new_dns_sd_name), "%s (%s)", printer->dns_sd_name, printer->system->hostname);
      else if (serial)
	snprintf(new_dns_sd_name, sizeof(new_dns_sd_name), "%s (%s)", printer->dns_sd_name, serial + 8);
      else
	snprintf(new_dns_sd_name, sizeof(new_dns_sd_name), "%s (%c%c%c%c%c%c)", printer->dns_sd_name, toupper(uuid[39]), toupper(uuid[40]), toupper(uuid[41]), toupper(uuid[42]), toupper(uuid[43]), toupper(uuid[44]));
    }
    else
    {
      char	base_dns_sd_name[256];	// Base DNS-SD name

      papplCopyString(base_dns_sd_name, printer->dns_sd_name, sizeof(base_dns_sd_name));
      if ((ptr = strrchr(base_dns_sd_name, '(')) != NULL)
        *ptr = '\0';

      snprintf(new_dns_sd_name, sizeof(new_dns_sd_name), "%s(%d)", base_dns_sd_name, printer->dns_sd_serial);
    }

    free(printer->dns_sd_name);
    if ((printer->dns_sd_name = strdup(new_dns_sd_name)) != NULL)
    {
      papplLogPrinter(printer, PAPPL_LOGLEVEL_INFO, "DNS-SD name collision, trying new DNS-SD service name '%s'.", printer->dns_sd_name);

      printer->dns_sd_collision = false;
    }
    else
    {
      papplLogPrinter(printer, PAPPL_LOGLEVEL_INFO, "DNS-SD name collision, failed to allocate new DNS-SD service name.");
      return (false);
    }
  }

  if ((master = _papplDNSSDInit(printer->system)) == NULL)
    return (false);
#endif // HAVE_DNSSD

#ifdef HAVE_MDNSRESPONDER
  // Build the TXT record for IPP...
  TXTRecordCreate(&txt, 1024, NULL);
  TXTRecordSetValue(&txt, "rp", (uint8_t)strlen(printer->resource) - 1, printer->resource + 1);
  if (printer->driver_data.make_and_model[0])
    TXTRecordSetValue(&txt, "ty", (uint8_t)strlen(printer->driver_data.make_and_model), printer->driver_data.make_and_model);
  TXTRecordSetValue(&txt, "adminurl", (uint8_t)strlen(adminurl), adminurl);
  if (printer->location)
    TXTRecordSetValue(&txt, "note", (uint8_t)strlen(printer->location), printer->location);
  else
    TXTRecordSetValue(&txt, "note", 0, "");
  TXTRecordSetValue(&txt, "pdl", (uint8_t)strlen(formats), formats);
  if (kind[0])
    TXTRecordSetValue(&txt, "kind", (uint8_t)strlen(kind), kind);
  if ((value = ippGetString(printer_uuid, 0, NULL)) != NULL)
    TXTRecordSetValue(&txt, "UUID", (uint8_t)strlen(value) - 9, value + 9);
  if (urf[0])
    TXTRecordSetValue(&txt, "URF", (uint8_t)strlen(urf), urf);
  TXTRecordSetValue(&txt, "Color", 1, ippGetBoolean(color_supported, 0) ? "T" : "F");
  TXTRecordSetValue(&txt, "Duplex", 1, (printer->driver_data.sides_supported & PAPPL_SIDES_TWO_SIDED_LONG_EDGE) ? "T" : "F");
  TXTRecordSetValue(&txt, "TLS", 3, "1.2");
  TXTRecordSetValue(&txt, "txtvers", 1, "1");
  TXTRecordSetValue(&txt, "qtotal", 1, "1");
  TXTRecordSetValue(&txt, "priority", 1, "0");
  TXTRecordSetValue(&txt, "mopria-certified", 3, "1.3");

  // Legacy keys...
  TXTRecordSetValue(&txt, "product", (uint8_t)strlen(product), product);
  TXTRecordSetValue(&txt, "Fax", 1, "F");
  TXTRecordSetValue(&txt, "PaperMax", (uint8_t)strlen(papermax), papermax);
  TXTRecordSetValue(&txt, "Scan", 1, "F");

  // Register the _printer._tcp (LPD) service type with a port number of 0 to
  // defend our service name but not actually support LPD...
  if (printer->dns_sd_printer_ref)
    DNSServiceRefDeallocate(printer->dns_sd_printer_ref);

  printer->dns_sd_printer_ref = master;

  if ((error = DNSServiceRegister(&printer->dns_sd_printer_ref, kDNSServiceFlagsShareConnection | kDNSServiceFlagsNoAutoRename, if_index, printer->dns_sd_name, "_printer._tcp", NULL /* domain */, NULL /* host */, 0 /* port */, 0 /* txtLen */, NULL /* txtRecord */, (DNSServiceRegisterReply)dns_sd_printer_callback, printer)) != kDNSServiceErr_NoError)
  {
    papplLogPrinter(printer, PAPPL_LOGLEVEL_ERROR, "Unable to register '%s._printer._tcp': %s", printer->dns_sd_name, _papplDNSSDStrError(error));
    ret = false;
  }

  // Then register the corresponding IPP service types with the real port
  // number to advertise our printer...
  if (printer->dns_sd_ipp_ref)
    DNSServiceRefDeallocate(printer->dns_sd_ipp_ref);

  printer->dns_sd_ipp_ref = master;

  if (system->subtypes && *system->subtypes)
    snprintf(regtype, sizeof(regtype), "_ipp._tcp,%s", system->subtypes);
  else
    papplCopyString(regtype, "_ipp._tcp", sizeof(regtype));

  if ((error = DNSServiceRegister(&printer->dns_sd_ipp_ref, kDNSServiceFlagsShareConnection | kDNSServiceFlagsNoAutoRename, if_index, printer->dns_sd_name, regtype, NULL /* domain */, system->hostname, htons(system->port), TXTRecordGetLength(&txt), TXTRecordGetBytesPtr(&txt), (DNSServiceRegisterReply)dns_sd_printer_callback, printer)) != kDNSServiceErr_NoError)
  {
    papplLogPrinter(printer, PAPPL_LOGLEVEL_ERROR, "Unable to register '%s.%s': %s", printer->dns_sd_name, regtype, _papplDNSSDStrError(error));
    ret = false;
  }

  if (printer->geo_location && ret)
  {
    if ((error = DNSServiceAddRecord(printer->dns_sd_ipp_ref, &printer->dns_sd_ipp_loc_ref, 0, kDNSServiceType_LOC, sizeof(printer->dns_sd_loc), printer->dns_sd_loc, 0)) != kDNSServiceErr_NoError)
    {
      papplLogPrinter(printer, PAPPL_LOGLEVEL_ERROR, "Unable to register LOC record for '%s.%s': %s", printer->dns_sd_name, regtype, _papplDNSSDStrError(error));
      ret = false;
    }
  }

  if (printer->dns_sd_ipps_ref)
    DNSServiceRefDeallocate(printer->dns_sd_ipps_ref);

  if (!(printer->system->options & PAPPL_SOPTIONS_NO_TLS))
  {
    printer->dns_sd_ipps_ref = master;

    if (system->subtypes && *system->subtypes)
      snprintf(regtype, sizeof(regtype), "_ipps._tcp,%s", system->subtypes);
    else
      papplCopyString(regtype, "_ipps._tcp", sizeof(regtype));

    if ((error = DNSServiceRegister(&printer->dns_sd_ipps_ref, kDNSServiceFlagsShareConnection | kDNSServiceFlagsNoAutoRename, if_index, printer->dns_sd_name, regtype, NULL /* domain */, system->hostname, htons(system->port), TXTRecordGetLength(&txt), TXTRecordGetBytesPtr(&txt), (DNSServiceRegisterReply)dns_sd_printer_callback, printer)) != kDNSServiceErr_NoError)
    {
      papplLogPrinter(printer, PAPPL_LOGLEVEL_ERROR, "Unable to register '%s.%s': %s", printer->dns_sd_name, regtype, _papplDNSSDStrError(error));
      ret = false;
    }

    if (printer->geo_location && ret)
    {
      if ((error = DNSServiceAddRecord(printer->dns_sd_ipps_ref, &printer->dns_sd_ipps_loc_ref, 0, kDNSServiceType_LOC, sizeof(printer->dns_sd_loc), printer->dns_sd_loc, 0)) != kDNSServiceErr_NoError)
      {
	papplLogPrinter(printer, PAPPL_LOGLEVEL_ERROR, "Unable to register LOC record for '%s.%s': %s", printer->dns_sd_name, regtype, _papplDNSSDStrError(error));
	ret = false;
      }
    }
  }
  else
    printer->dns_sd_ipps_ref = NULL;

  TXTRecordDeallocate(&txt);

  if ((system->options & PAPPL_SOPTIONS_RAW_SOCKET) && printer->num_raw_listeners > 0)
  {
    // Register a PDL datastream (raw socket) service...
    TXTRecordCreate(&txt, 1024, NULL);
    if (printer->driver_data.make_and_model[0])
      TXTRecordSetValue(&txt, "ty", (uint8_t)strlen(printer->driver_data.make_and_model), printer->driver_data.make_and_model);
    TXTRecordSetValue(&txt, "adminurl", (uint8_t)strlen(adminurl), adminurl);
    if (printer->location)
      TXTRecordSetValue(&txt, "note", (uint8_t)strlen(printer->location), printer->location);
    else
      TXTRecordSetValue(&txt, "note", 0, "");
    TXTRecordSetValue(&txt, "pdl", (uint8_t)strlen(formats), formats);
    if ((value = ippGetString(printer_uuid, 0, NULL)) != NULL)
      TXTRecordSetValue(&txt, "UUID", (uint8_t)strlen(value) - 9, value + 9);
    TXTRecordSetValue(&txt, "Color", 1, ippGetBoolean(color_supported, 0) ? "T" : "F");
    TXTRecordSetValue(&txt, "Duplex", 1, (printer->driver_data.sides_supported & PAPPL_SIDES_TWO_SIDED_LONG_EDGE) ? "T" : "F");
    TXTRecordSetValue(&txt, "txtvers", 1, "1");
    TXTRecordSetValue(&txt, "qtotal", 1, "1");
    TXTRecordSetValue(&txt, "priority", 3, "100");

    // Legacy keys...
    TXTRecordSetValue(&txt, "product", (uint8_t)strlen(product), product);
    TXTRecordSetValue(&txt, "Fax", 1, "F");
    TXTRecordSetValue(&txt, "PaperMax", (uint8_t)strlen(papermax), papermax);
    TXTRecordSetValue(&txt, "Scan", 1, "F");

    if (printer->dns_sd_pdl_ref)
      DNSServiceRefDeallocate(printer->dns_sd_pdl_ref);

    printer->dns_sd_pdl_ref = master;

    if ((error = DNSServiceRegister(&printer->dns_sd_pdl_ref, kDNSServiceFlagsShareConnection | kDNSServiceFlagsNoAutoRename, if_index, printer->dns_sd_name, "_pdl-datastream._tcp", NULL /* domain */, system->hostname, htons(9099 + printer->printer_id), TXTRecordGetLength(&txt), TXTRecordGetBytesPtr(&txt), (DNSServiceRegisterReply)dns_sd_printer_callback, printer)) != kDNSServiceErr_NoError)
    {
      papplLogPrinter(printer, PAPPL_LOGLEVEL_ERROR, "Unable to register '%s.%s': %s", printer->dns_sd_name, "_pdl-datastream._tcp", _papplDNSSDStrError(error));
      ret = false;
    }

    TXTRecordDeallocate(&txt);
  }

  // Register the _http._tcp,_printer (HTTP) service type with the real port
  // number to advertise our web interface...
  if (printer->dns_sd_http_ref)
    DNSServiceRefDeallocate(printer->dns_sd_http_ref);

  snprintf(adminurl, sizeof(adminurl), "%s/", printer->uriname);

  TXTRecordCreate(&txt, 1024, NULL);
  TXTRecordSetValue(&txt, "path", (uint8_t)strlen(adminurl), adminurl);

  printer->dns_sd_http_ref = master;

  if ((error = DNSServiceRegister(&printer->dns_sd_http_ref, kDNSServiceFlagsShareConnection | kDNSServiceFlagsNoAutoRename, if_index, printer->dns_sd_name, "_http._tcp,_printer", NULL /* domain */, system->hostname, htons(system->port), TXTRecordGetLength(&txt), TXTRecordGetBytesPtr(&txt), (DNSServiceRegisterReply)dns_sd_printer_callback, printer)) != kDNSServiceErr_NoError)
  {
    papplLogPrinter(printer, PAPPL_LOGLEVEL_ERROR, "Unable to register '%s.%s': %s", printer->dns_sd_name, "_http._tcp,_printer", _papplDNSSDStrError(error));
    ret = false;
  }

  TXTRecordDeallocate(&txt);

#elif defined(HAVE_AVAHI)
  // Create the TXT record...
  txt = NULL;
  txt = avahi_string_list_add_printf(txt, "rp=%s", printer->resource + 1);
  if (printer->driver_data.make_and_model[0])
    txt = avahi_string_list_add_printf(txt, "ty=%s", printer->driver_data.make_and_model);
  txt = avahi_string_list_add_printf(txt, "adminurl=%s", adminurl);
  txt = avahi_string_list_add_printf(txt, "note=%s", printer->location ? printer->location : "");
  txt = avahi_string_list_add_printf(txt, "pdl=%s", formats);
  if (kind[0])
    txt = avahi_string_list_add_printf(txt, "kind=%s", kind);
  if ((value = ippGetString(printer_uuid, 0, NULL)) != NULL)
    txt = avahi_string_list_add_printf(txt, "UUID=%s", value + 9);
  if (urf[0])
    txt = avahi_string_list_add_printf(txt, "URF=%s", urf);
  txt = avahi_string_list_add_printf(txt, "TLS=1.2");
  txt = avahi_string_list_add_printf(txt, "Color=%s", ippGetBoolean(color_supported, 0) ? "T" : "F");
  txt = avahi_string_list_add_printf(txt, "Duplex=%s", (printer->driver_data.sides_supported & PAPPL_SIDES_TWO_SIDED_LONG_EDGE) ? "T" : "F");
  txt = avahi_string_list_add_printf(txt, "txtvers=1");
  txt = avahi_string_list_add_printf(txt, "qtotal=1");
  txt = avahi_string_list_add_printf(txt, "priority=0");
  txt = avahi_string_list_add_printf(txt, "mopria-certified=1.3");

  // Legacy keys...
  txt = avahi_string_list_add_printf(txt, "product=%s", product);
  txt = avahi_string_list_add_printf(txt, "Fax=F");
  txt = avahi_string_list_add_printf(txt, "PaperMax=%s", papermax);
  txt = avahi_string_list_add_printf(txt, "Scan=F");

  // Register _printer._tcp (LPD) with port 0 to reserve the service name...
  _papplDNSSDLock();

  if (printer->dns_sd_ref)
    avahi_entry_group_free(printer->dns_sd_ref);

  if ((printer->dns_sd_ref = avahi_entry_group_new(master, (AvahiEntryGroupCallback)dns_sd_printer_callback, printer)) == NULL)
  {
    papplLogPrinter(printer, PAPPL_LOGLEVEL_ERROR, "Unable to register printer, is the Avahi daemon running?");
    _papplDNSSDUnlock();
    avahi_string_list_free(txt);
    return (false);
  }

  if ((error = avahi_entry_group_add_service_strlst(printer->dns_sd_ref, if_index, AVAHI_PROTO_UNSPEC, 0, printer->dns_sd_name, "_printer._tcp", NULL, NULL, 0, NULL)) < 0)
  {
    papplLogPrinter(printer, PAPPL_LOGLEVEL_ERROR, "Unable to register '%s._printer._tcp': %s", printer->dns_sd_name, _papplDNSSDStrError(error));
    ret = false;
  }

  // Then register the IPP/IPPS services...
  if ((error = avahi_entry_group_add_service_strlst(printer->dns_sd_ref, if_index, AVAHI_PROTO_UNSPEC, 0, printer->dns_sd_name, "_ipp._tcp", NULL, system->hostname, system->port, txt)) < 0)
  {
    papplLogPrinter(printer, PAPPL_LOGLEVEL_ERROR, "Unable to register '%s._ipp._tcp': %s", printer->dns_sd_name, _papplDNSSDStrError(error));
    ret = false;
  }

  if (system->subtypes && *system->subtypes)
  {
    char *temptypes = strdup(system->subtypes), *start, *end;
					// Pointers into sub-types...

    for (start = temptypes; start && *start; start = end)
    {
      if ((end = strchr(start, ',')) != NULL)
	*end++ = '\0';
      else
	end = start + strlen(start);

      snprintf(regtype, sizeof(regtype), "%s._sub._ipp._tcp", start);
      if ((error = avahi_entry_group_add_service_subtype(printer->dns_sd_ref, if_index, AVAHI_PROTO_UNSPEC, 0, printer->dns_sd_name, "_ipp._tcp", NULL, regtype)) < 0)
      {
        papplLogPrinter(printer, PAPPL_LOGLEVEL_ERROR, "Unable to register '%s.%s': %s", printer->dns_sd_name, regtype, _papplDNSSDStrError(error));
        ret = false;
      }
    }

    free(temptypes);
  }

  if (!(printer->system->options & PAPPL_SOPTIONS_NO_TLS))
  {
    if ((error = avahi_entry_group_add_service_strlst(printer->dns_sd_ref, if_index, AVAHI_PROTO_UNSPEC, 0, printer->dns_sd_name, "_ipps._tcp", NULL, system->hostname, system->port, txt)) < 0)
    {
      papplLogPrinter(printer, PAPPL_LOGLEVEL_ERROR, "Unable to register '%s._ipps._tcp': %s", printer->dns_sd_name, _papplDNSSDStrError(error));
      ret = false;
    }

    if (system->subtypes && *system->subtypes)
    {
      char *temptypes = strdup(system->subtypes), *start, *end;
					  // Pointers into sub-types...

      for (start = temptypes; start && *start; start = end)
      {
	if ((end = strchr(start, ',')) != NULL)
	  *end++ = '\0';
	else
	  end = start + strlen(start);

	snprintf(regtype, sizeof(regtype), "%s._sub._ipps._tcp", start);
	if ((error = avahi_entry_group_add_service_subtype(printer->dns_sd_ref, if_index, AVAHI_PROTO_UNSPEC, 0, printer->dns_sd_name, "_ipps._tcp", NULL, regtype)) < 0)
	{
	  papplLogPrinter(printer, PAPPL_LOGLEVEL_ERROR, "Unable to register '%s.%s': %s", printer->dns_sd_name, regtype, _papplDNSSDStrError(error));
	  ret = false;
	}
      }

      free(temptypes);
    }
  }

  avahi_string_list_free(txt);

  if ((system->options & PAPPL_SOPTIONS_RAW_SOCKET) && printer->num_raw_listeners > 0)
  {
    // Register a PDL datastream (raw socket) service...
    txt = NULL;
    if (printer->driver_data.make_and_model[0])
      txt = avahi_string_list_add_printf(txt, "ty=%s", printer->driver_data.make_and_model);
    txt = avahi_string_list_add_printf(txt, "adminurl=%s", adminurl);
    txt = avahi_string_list_add_printf(txt, "note=%s", printer->location ? printer->location : "");
    txt = avahi_string_list_add_printf(txt, "pdl=%s", formats);
    if ((value = ippGetString(printer_uuid, 0, NULL)) != NULL)
      txt = avahi_string_list_add_printf(txt, "UUID=%s", value + 9);
    txt = avahi_string_list_add_printf(txt, "Color=%s", ippGetBoolean(color_supported, 0) ? "T" : "F");
    txt = avahi_string_list_add_printf(txt, "Duplex=%s", (printer->driver_data.sides_supported & PAPPL_SIDES_TWO_SIDED_LONG_EDGE) ? "T" : "F");
    txt = avahi_string_list_add_printf(txt, "txtvers=1");
    txt = avahi_string_list_add_printf(txt, "qtotal=1");
    txt = avahi_string_list_add_printf(txt, "priority=100");

    // Legacy keys...
    txt = avahi_string_list_add_printf(txt, "product=%s", product);
    txt = avahi_string_list_add_printf(txt, "Fax=F");
    txt = avahi_string_list_add_printf(txt, "PaperMax=%s", papermax);
    txt = avahi_string_list_add_printf(txt, "Scan=F");

    if ((error = avahi_entry_group_add_service_strlst(printer->dns_sd_ref, if_index, AVAHI_PROTO_UNSPEC, 0, printer->dns_sd_name, "_pdl-datastream._tcp", NULL, system->hostname, 9099 + printer->printer_id, txt)) < 0)
    {
      papplLogPrinter(printer, PAPPL_LOGLEVEL_ERROR, "Unable to register '%s._pdl-datastream._tcp': %s", printer->dns_sd_name, _papplDNSSDStrError(error));
      ret = false;
    }

    avahi_string_list_free(txt);
  }

  // Register the geolocation of the service...
  if (printer->geo_location && ret)
  {
    snprintf(fullname, sizeof(fullname), "%s._ipp._tcp.local.", printer->dns_sd_name);

    if ((error = avahi_entry_group_add_record(printer->dns_sd_ref, if_index, AVAHI_PROTO_UNSPEC, 0, fullname, AVAHI_DNS_CLASS_IN, AVAHI_DNS_TYPE_LOC, 75 * 60, printer->dns_sd_loc, sizeof(printer->dns_sd_loc))) < 0)
    {
      papplLogPrinter(printer, PAPPL_LOGLEVEL_ERROR, "Unable to register LOC record for '%s': %s", fullname, _papplDNSSDStrError(error));
      ret = false;
    }

    snprintf(fullname, sizeof(fullname), "%s._ipps._tcp.local.", printer->dns_sd_name);

    if ((error = avahi_entry_group_add_record(printer->dns_sd_ref, if_index, AVAHI_PROTO_UNSPEC, 0, fullname, AVAHI_DNS_CLASS_IN, AVAHI_DNS_TYPE_LOC, 75 * 60, printer->dns_sd_loc, sizeof(printer->dns_sd_loc))) < 0)
    {
      papplLogPrinter(printer, PAPPL_LOGLEVEL_ERROR, "Unable to register LOC record for '%s': %s", fullname, _papplDNSSDStrError(error));
      ret = false;
    }
  }

  // Finally _http.tcp (HTTP) for the web interface...
  txt = NULL;
  txt = avahi_string_list_add_printf(txt, "path=%s/", printer->uriname);

  avahi_entry_group_add_service_strlst(printer->dns_sd_ref, if_index, AVAHI_PROTO_UNSPEC, 0, printer->dns_sd_name, "_http._tcp", NULL, system->hostname, system->port, txt);
  avahi_entry_group_add_service_subtype(printer->dns_sd_ref, if_index, AVAHI_PROTO_UNSPEC, 0, printer->dns_sd_name, "_http._tcp", NULL, "_printer._sub._http._tcp");

  avahi_string_list_free(txt);

  // Commit it...
  avahi_entry_group_commit(printer->dns_sd_ref);
  _papplDNSSDUnlock();
#endif // HAVE_MDNSRESPONDER

  return (ret);
}


//
// '_papplPrinterUnregisterDNSSDNoLock()' - Unregister a printer's DNS-SD service.
//

void
_papplPrinterUnregisterDNSSDNoLock(
    pappl_printer_t *printer)		// I - Printer
{
#if HAVE_MDNSRESPONDER
  if (printer->dns_sd_printer_ref)
  {
    DNSServiceRefDeallocate(printer->dns_sd_printer_ref);
    printer->dns_sd_printer_ref = NULL;
  }
  if (printer->dns_sd_ipp_ref)
  {
    DNSServiceRefDeallocate(printer->dns_sd_ipp_ref);
    printer->dns_sd_ipp_ref     = NULL;
    printer->dns_sd_ipp_loc_ref = NULL;
  }
  if (printer->dns_sd_ipps_ref)
  {
    DNSServiceRefDeallocate(printer->dns_sd_ipps_ref);
    printer->dns_sd_ipps_ref     = NULL;
    printer->dns_sd_ipps_loc_ref = NULL;
  }
  if (printer->dns_sd_http_ref)
  {
    DNSServiceRefDeallocate(printer->dns_sd_http_ref);
    printer->dns_sd_http_ref = NULL;
  }

#elif defined(HAVE_AVAHI)
  _papplDNSSDLock();

  if (printer->dns_sd_ref)
  {
    avahi_entry_group_free(printer->dns_sd_ref);
    printer->dns_sd_ref = NULL;
  }

  _papplDNSSDUnlock();

#else
  (void)printer;
#endif // HAVE_MDNSRESPONDER
}


//
// '_papplSystemRegisterDNSSDNoLock()' - Register a system's DNS-SD service.
//

bool					// O - `true` on success, `false` on failure
_papplSystemRegisterDNSSDNoLock(
    pappl_system_t *system)		// I - System
{
  bool			ret = true;	// Return value
#ifdef HAVE_DNSSD
  _pappl_dns_sd_t	master;		// DNS-SD master reference
  _pappl_txt_t		txt;		// DNS-SD TXT record
  uint32_t		if_index;	// Interface index
#  ifdef HAVE_MDNSRESPONDER
  DNSServiceErrorType	error;		// Error from mDNSResponder
#  else
  int			error;		// Error from Avahi
  char			fullname[256];	// Full name of services
#  endif // HAVE_MDNSRESPONDER


  // Make sure we have all of the necessary information to register the system...
  if (!system->dns_sd_name || !system->hostname || !system->uuid || !system->is_running)
    return (false);

  papplLog(system, PAPPL_LOGLEVEL_DEBUG, "Registering DNS-SD name '%s' on '%s'", system->dns_sd_name, system->hostname);

#  ifdef HAVE_MDNSRESPONDER
  if_index = !strcmp(system->hostname, "localhost") ? kDNSServiceInterfaceIndexLocalOnly : kDNSServiceInterfaceIndexAny;
#  else
  if_index = !strcmp(system->hostname, "localhost") ? if_nametoindex("lo") : AVAHI_IF_UNSPEC;
#  endif // HAVE_MDNSRESPONDER

  if (system->geo_location)
    dns_sd_geo_to_loc(system->geo_location, system->dns_sd_loc);

  // Rename the service as needed...
  if (system->dns_sd_collision)
  {
    char	new_dns_sd_name[256];	// New DNS-SD name

    system->dns_sd_serial ++;

    if (system->dns_sd_serial == 1)
    {
      if (system->options & PAPPL_SOPTIONS_DNSSD_HOST)
	snprintf(new_dns_sd_name, sizeof(new_dns_sd_name), "%s (%s)", system->dns_sd_name, system->hostname);
      else
	snprintf(new_dns_sd_name, sizeof(new_dns_sd_name), "%s (%c%c%c%c%c%c)", system->dns_sd_name, toupper(system->uuid[39]), toupper(system->uuid[40]), toupper(system->uuid[41]), toupper(system->uuid[42]), toupper(system->uuid[43]), toupper(system->uuid[44]));
    }
    else
    {
      char	base_dns_sd_name[256],	// Base DNS-SD name
		*ptr;			// Pointer into name

      papplCopyString(base_dns_sd_name, system->dns_sd_name, sizeof(base_dns_sd_name));
      if ((ptr = strrchr(base_dns_sd_name, '(')) != NULL)
        *ptr = '\0';

      snprintf(new_dns_sd_name, sizeof(new_dns_sd_name), "%s(%d)", base_dns_sd_name, system->dns_sd_serial);
    }

    free(system->dns_sd_name);

    if ((system->dns_sd_name = strdup(new_dns_sd_name)) != NULL)
    {
      papplLog(system, PAPPL_LOGLEVEL_INFO, "DNS-SD name collision, trying new DNS-SD service name '%s'.", system->dns_sd_name);

      system->dns_sd_collision = false;
    }
    else
    {
      papplLog(system, PAPPL_LOGLEVEL_ERROR, "DNS-SD name collision, unable to allocate new DNS-SD service name.");
      return (false);
    }
  }

  if ((master = _papplDNSSDInit(system)) == NULL)
    return (false);
#endif // HAVE_DNSSD

#ifdef HAVE_MDNSRESPONDER
  // Build the TXT record...
  TXTRecordCreate(&txt, 1024, NULL);
  if (system->location != NULL)
    TXTRecordSetValue(&txt, "note", (uint8_t)strlen(system->location), system->location);
  TXTRecordSetValue(&txt, "UUID", (uint8_t)strlen(system->uuid) - 9, system->uuid + 9);

  // Then register the corresponding IPPS service type to advertise our system...
  if (system->dns_sd_ipps_ref)
    DNSServiceRefDeallocate(system->dns_sd_ipps_ref);

  if (!(system->options & PAPPL_SOPTIONS_NO_TLS))
  {
    system->dns_sd_ipps_ref = master;

    if ((error = DNSServiceRegister(&system->dns_sd_ipps_ref, kDNSServiceFlagsShareConnection | kDNSServiceFlagsNoAutoRename, if_index, system->dns_sd_name, "_ipps-system._tcp", NULL /* domain */, system->hostname, htons(system->port), TXTRecordGetLength(&txt), TXTRecordGetBytesPtr(&txt), (DNSServiceRegisterReply)dns_sd_system_callback, system)) != kDNSServiceErr_NoError)
    {
      papplLog(system, PAPPL_LOGLEVEL_ERROR, "Unable to register '%s._ipps-system._tcp': %s", system->dns_sd_name, _papplDNSSDStrError(error));
      ret = false;
    }

    if (system->geo_location && ret)
    {
      papplLog(system, PAPPL_LOGLEVEL_DEBUG, "Registering LOC record for '%s._ipps-system._tcp' with data %02X %02X %02X %02X %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X", system->dns_sd_name, system->dns_sd_loc[0], system->dns_sd_loc[1], system->dns_sd_loc[2], system->dns_sd_loc[3], system->dns_sd_loc[4], system->dns_sd_loc[5], system->dns_sd_loc[6], system->dns_sd_loc[7], system->dns_sd_loc[8], system->dns_sd_loc[9], system->dns_sd_loc[10], system->dns_sd_loc[11], system->dns_sd_loc[12], system->dns_sd_loc[13], system->dns_sd_loc[14], system->dns_sd_loc[15]);

      if ((error = DNSServiceAddRecord(system->dns_sd_ipps_ref, &system->dns_sd_loc_ref, 0, kDNSServiceType_LOC, sizeof(system->dns_sd_loc), system->dns_sd_loc, 0)) != kDNSServiceErr_NoError)
      {
	papplLog(system, PAPPL_LOGLEVEL_ERROR, "Unable to register LOC record for '%s._ipps-system._tcp': %s", system->dns_sd_name, _papplDNSSDStrError(error));
	ret = false;
      }
    }
  }
  else
    system->dns_sd_ipps_ref = NULL;

  TXTRecordDeallocate(&txt);

  // Register the _http._tcp,_printer (HTTP) service type with the real port
  // number to advertise our web interface...
  if (system->options & PAPPL_SOPTIONS_MULTI_QUEUE)
  {
    if (system->dns_sd_http_ref)
      DNSServiceRefDeallocate(system->dns_sd_http_ref);

    system->dns_sd_http_ref = master;

    if ((error = DNSServiceRegister(&system->dns_sd_http_ref, kDNSServiceFlagsShareConnection | kDNSServiceFlagsNoAutoRename, if_index, system->dns_sd_name, "_http._tcp,_printer", NULL /* domain */, system->hostname, htons(system->port), 0 /* txtlen */, NULL /* txt */, (DNSServiceRegisterReply)dns_sd_system_callback, system)) != kDNSServiceErr_NoError)
    {
      papplLog(system, PAPPL_LOGLEVEL_ERROR, "Unable to register '%s.%s': %s", system->dns_sd_name, "_http._tcp,_printer", _papplDNSSDStrError(error));
      ret = false;
    }
  }

#elif defined(HAVE_AVAHI)
  // Create the TXT record...
  txt = NULL;
  if (system->location)
    txt = avahi_string_list_add_printf(txt, "note=%s", system->location);
  txt = avahi_string_list_add_printf(txt, "UUID=%s", system->uuid + 9);

  // Register _printer._tcp (LPD) with port 0 to reserve the service name...
  _papplDNSSDLock();

  if (system->dns_sd_ref)
    avahi_entry_group_free(system->dns_sd_ref);

  if ((system->dns_sd_ref = avahi_entry_group_new(master, (AvahiEntryGroupCallback)dns_sd_system_callback, system)) == NULL)
  {
    papplLog(system, PAPPL_LOGLEVEL_ERROR, "Unable to register system, is the Avahi daemon running?");
    _papplDNSSDUnlock();
    avahi_string_list_free(txt);
    return (false);
  }

  if (!(system->options & PAPPL_SOPTIONS_NO_TLS))
  {
    if ((error = avahi_entry_group_add_service_strlst(system->dns_sd_ref, if_index, AVAHI_PROTO_UNSPEC, 0, system->dns_sd_name, "_ipps-system._tcp", NULL, system->hostname, system->port, txt)) < 0)
    {
      papplLog(system, PAPPL_LOGLEVEL_ERROR, "Unable to register '%s._ipps-system._tcp': %s", system->dns_sd_name, _papplDNSSDStrError(error));
      ret = false;
    }

    // Register the geolocation of the service...
    if (system->geo_location && ret)
    {
      snprintf(fullname, sizeof(fullname), "%s._ipps-system._tcp.local.", system->dns_sd_name);

      papplLog(system, PAPPL_LOGLEVEL_DEBUG, "Registering LOC record for '%s' with data %02X %02X %02X %02X %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X", fullname, system->dns_sd_loc[0], system->dns_sd_loc[1], system->dns_sd_loc[2], system->dns_sd_loc[3], system->dns_sd_loc[4], system->dns_sd_loc[5], system->dns_sd_loc[6], system->dns_sd_loc[7], system->dns_sd_loc[8], system->dns_sd_loc[9], system->dns_sd_loc[10], system->dns_sd_loc[11], system->dns_sd_loc[12], system->dns_sd_loc[13], system->dns_sd_loc[14], system->dns_sd_loc[15]);

      if ((error = avahi_entry_group_add_record(system->dns_sd_ref, if_index, AVAHI_PROTO_UNSPEC, 0, fullname, AVAHI_DNS_CLASS_IN, AVAHI_DNS_TYPE_LOC, 75 * 60, system->dns_sd_loc, sizeof(system->dns_sd_loc))) < 0)
      {
	papplLog(system, PAPPL_LOGLEVEL_ERROR, "Unable to register LOC record for '%s': %s", fullname, _papplDNSSDStrError(error));
	ret = false;
      }
    }
  }

  // Finally _http.tcp (HTTP) for the web interface...
  if (system->options & PAPPL_SOPTIONS_MULTI_QUEUE)
  {
    avahi_entry_group_add_service_strlst(system->dns_sd_ref, if_index, AVAHI_PROTO_UNSPEC, 0, system->dns_sd_name, "_http._tcp", NULL, system->hostname, system->port, NULL);
    avahi_entry_group_add_service_subtype(system->dns_sd_ref, if_index, AVAHI_PROTO_UNSPEC, 0, system->dns_sd_name, "_http._tcp", NULL, "_printer._sub._http._tcp");
  }

  // Commit it...
  avahi_entry_group_commit(system->dns_sd_ref);
  _papplDNSSDUnlock();

  avahi_string_list_free(txt);
#endif // HAVE_MDNSRESPONDER

  return (ret);
}


//
// '_papplSystemUnregisterDNSSDNoLock()' - Unregister a printer's DNS-SD service.
//

void
_papplSystemUnregisterDNSSDNoLock(
    pappl_system_t *system)		// I - System
{
#if HAVE_MDNSRESPONDER
  if (system->dns_sd_ipps_ref)
  {
    DNSServiceRefDeallocate(system->dns_sd_ipps_ref);
    system->dns_sd_ipps_ref = NULL;
  }

  if (system->dns_sd_http_ref)
  {
    DNSServiceRefDeallocate(system->dns_sd_http_ref);
    system->dns_sd_http_ref = NULL;
  }

#elif defined(HAVE_AVAHI)
  _papplDNSSDLock();

  if (system->dns_sd_ref)
  {
    avahi_entry_group_free(system->dns_sd_ref);
    system->dns_sd_ref = NULL;
  }

  _papplDNSSDUnlock();

#else
  (void)system;
#endif // HAVE_MDNSRESPONDER
}


//
// 'dns_sd_geo_to_loc()' - Convert a "geo:" URI to a DNS LOC record.
//

static void
dns_sd_geo_to_loc(const char    *geo,	// I - "geo:" URI
                  unsigned char loc[16])// O - DNS LOC record
{
  double	lat = 0.0, lon = 0.0;	// Latitude and longitude in degrees
  double	alt = 0.0;		// Altitude in meters
  unsigned int	lat_ksec, lon_ksec;	// Latitude and longitude in thousandths of arc seconds, biased by 2^31
  unsigned int	alt_cm;			// Altitude in centimeters, biased by 10,000,000cm

  // Pull apart the "geo:" URI and convert to the integer representation for
  // the LOC record...
  sscanf(geo, "geo:%lf,%lf,%lf", &lat, &lon, &alt);
  lat_ksec = (unsigned)((int)(lat * 3600000.0) + 0x40000000 + 0x40000000);
  lon_ksec = (unsigned)((int)(lon * 3600000.0) + 0x40000000 + 0x40000000);
  alt_cm   = (unsigned)((int)(alt * 100.0) + 10000000);

  // Build the LOC record...
  loc[0]  = 0x00;			// Version
  loc[1]  = 0x11;			// Size (10cm)
  loc[2]  = 0x11;			// Horizontal precision (10cm)
  loc[3]  = 0x11;			// Vertical precision (10cm)
  loc[4]  = (unsigned char)(lat_ksec >> 24);
					// Latitude (32-bit big-endian)
  loc[5]  = (unsigned char)(lat_ksec >> 16);
  loc[6]  = (unsigned char)(lat_ksec >> 8);
  loc[7]  = (unsigned char)(lat_ksec);
  loc[8]  = (unsigned char)(lon_ksec >> 24);
					// Latitude (32-bit big-endian)
  loc[9]  = (unsigned char)(lon_ksec >> 16);
  loc[10] = (unsigned char)(lon_ksec >> 8);
  loc[11] = (unsigned char)(lon_ksec);
  loc[12] = (unsigned char)(alt_cm >> 24);
					// Altitude (32-bit big-endian)
  loc[13] = (unsigned char)(alt_cm >> 16);
  loc[14] = (unsigned char)(alt_cm >> 8);
  loc[15] = (unsigned char)(alt_cm);
}


#ifdef HAVE_MDNSRESPONDER
//
// 'dns_sd_printer_callback()' - Handle DNS-SD printer registration events.
//

static void DNSSD_API
dns_sd_printer_callback(
    DNSServiceRef       sdRef,		// I - Service reference
    DNSServiceFlags     flags,		// I - Status flags
    DNSServiceErrorType errorCode,	// I - Error, if any
    const char          *name,		// I - Service name
    const char          *regtype,	// I - Service type
    const char          *domain,	// I - Domain for service
    pappl_printer_t     *printer)	// I - Printer
{
  (void)name;
  (void)sdRef;
  (void)flags;
  (void)domain;

  if (errorCode == kDNSServiceErr_NameConflict)
  {
    _papplRWLockWrite(printer->system);
    _papplRWLockWrite(printer);

    printer->dns_sd_collision             = true;
    printer->system->dns_sd_any_collision = true;

    _papplRWUnlock(printer);
    _papplRWUnlock(printer->system);
  }
  else if (errorCode)
  {
    papplLogPrinter(printer, PAPPL_LOGLEVEL_ERROR, "DNSServiceRegister for '%s' failed with error %d (%s).", regtype, (int)errorCode, _papplDNSSDStrError(errorCode));
    return;
  }
}


//
// 'dns_sd_run()' - Handle DNS-SD traffic.
//

static void *				// O - Exit status
dns_sd_run(void *data)			// I - System object
{
  int		err;			// Status
  pappl_system_t *system = (pappl_system_t *)data;
					// System object
  struct pollfd	pfd;			// Poll data


  pfd.events = POLLIN | POLLERR;
  pfd.fd     = DNSServiceRefSockFD(pappl_dns_sd_master);

  for (;;)
  {
#if _WIN32
    if (poll(&pfd, 1, 1000) < 0 && WSAGetLastError() == WSAEINTR)
#else
    if (poll(&pfd, 1, 1000) < 0 && errno != EINTR && errno != EAGAIN)
#endif // _WIN32
    {
      papplLog(system, PAPPL_LOGLEVEL_ERROR, "DNS-SD poll failed: %s", strerror(errno));
      break;
    }

    if (pfd.revents & POLLIN)
    {
      if ((err = DNSServiceProcessResult(pappl_dns_sd_master)) != kDNSServiceErr_NoError)
      {
	papplLog(system, PAPPL_LOGLEVEL_ERROR, "DNSServiceProcessResult returned %d (%s).", err, _papplDNSSDStrError(err));
	break;
      }
    }
    else if (pfd.revents)
      break;
  }

  return (NULL);
}


//
// 'dns_sd_system_callback()' - Handle DNS-SD system registration events.
//

static void DNSSD_API
dns_sd_system_callback(
    DNSServiceRef       sdRef,		// I - Service reference
    DNSServiceFlags     flags,		// I - Status flags
    DNSServiceErrorType errorCode,	// I - Error, if any
    const char          *name,		// I - Service name
    const char          *regtype,	// I - Service type
    const char          *domain,	// I - Domain for service
    pappl_system_t      *system)	// I - System
{
  (void)sdRef;
  (void)flags;
  (void)name;
  (void)domain;

  if (errorCode == kDNSServiceErr_NameConflict)
  {
    _papplRWLockWrite(system);
    system->dns_sd_collision     = true;
    system->dns_sd_any_collision = true;
    _papplRWUnlock(system);
  }
  else if (errorCode)
  {
    papplLog(system, PAPPL_LOGLEVEL_ERROR, "DNSServiceRegister for '%s' failed with error %d (%s).", regtype, (int)errorCode, _papplDNSSDStrError(errorCode));
    return;
  }
}


#elif defined(HAVE_AVAHI)
//
//'dns_sd_client_cb()' - Client callback for Avahi.
//
// Called whenever the client or server state changes...
//

static void
dns_sd_client_cb(
    AvahiClient      *c,		// I - Client
    AvahiClientState state,		// I - Current state
    void             *data)		// I - Callback data (unused)
{
  (void)data;


  if (!c)
    return;

  if (state == AVAHI_CLIENT_FAILURE)
  {
    if (avahi_client_errno(c) == AVAHI_ERR_DISCONNECTED)
    {
      fputs("Avahi server crashed.\n", stderr);
    }
  }
  else if (state == AVAHI_CLIENT_S_RUNNING)
    pappl_dns_sd_host_name_changes ++;
}


//
// 'dns_sd_printer_callback()' - Handle DNS-SD printer registration events.
//

static void
dns_sd_printer_callback(
    AvahiEntryGroup      *srv,		// I - Service
    AvahiEntryGroupState state,		// I - Registration state
    pappl_printer_t      *printer)	// I - Printer
{
  (void)srv;

  if (state == AVAHI_ENTRY_GROUP_COLLISION)
  {
    _papplRWLockWrite(printer->system);
    _papplRWLockWrite(printer);
    printer->dns_sd_collision             = true;
    printer->system->dns_sd_any_collision = true;
    _papplRWUnlock(printer);
    _papplRWUnlock(printer->system);
  }
}


//
// 'dns_sd_system_callback()' - Handle DNS-SD system registration events.
//

static void
dns_sd_system_callback(
    AvahiEntryGroup      *srv,		// I - Service
    AvahiEntryGroupState state,		// I - Registration state
    pappl_system_t      *system)	// I - System
{
  (void)srv;

  if (state == AVAHI_ENTRY_GROUP_COLLISION)
  {
    _papplRWLockWrite(system);
    system->dns_sd_collision     = true;
    system->dns_sd_any_collision = true;
    _papplRWUnlock(system);
  }
}
#endif // HAVE_MDNSRESPONDER
