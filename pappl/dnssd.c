//
// DNS-SD support for the Printer Application Framework
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


//
// Local functions...
//

#ifdef HAVE_DNSSD
static void DNSSD_API	dns_sd_printer_callback(DNSServiceRef sdRef, DNSServiceFlags flags, DNSServiceErrorType errorCode, const char *name, const char *regtype, const char *domain, pappl_printer_t *printer);
static void		*dns_sd_run(void *data);
static void DNSSD_API	dns_sd_system_callback(DNSServiceRef sdRef, DNSServiceFlags flags, DNSServiceErrorType errorCode, const char *name, const char *regtype, const char *domain, pappl_system_t *system);
#elif defined(HAVE_AVAHI)
static void		dns_sd_client_cb(AvahiClient *c, AvahiClientState state, pappl_system_t *system);
static void		dns_sd_printer_callback(AvahiEntryGroup *p, AvahiEntryGroupState state, pappl_printer_t *printer);
static void		dns_sd_system_callback(AvahiEntryGroup *p, AvahiEntryGroupState state, pappl_system_t *system);
#endif // HAVE_DNSSD


//
// '_papplSystemInitDNSSD()' - Initialize DNS-SD registration threads...
//

void
_papplSystemInitDNSSD(
    pappl_system_t *system)		// I - System
{
#ifdef HAVE_DNSSD
  int		err;			// Status
  pthread_t	tid;			// Thread ID


  if ((err = DNSServiceCreateConnection(&system->dns_sd_master)) != kDNSServiceErr_NoError)
  {
    papplLog(system, PAPPL_LOGLEVEL_ERROR, "Unable to initialize DNS-SD (%d).", err);
    return;
  }

  if (pthread_create(&tid, NULL, dns_sd_run, system))
  {
    papplLog(system, PAPPL_LOGLEVEL_ERROR, "Unable to create DNS-SD thread - %s", strerror(errno));
    return;
  }

  pthread_detach(tid);

#elif defined(HAVE_AVAHI)
  int error;			// Error code, if any

  if ((system->dns_sd_master = avahi_threaded_poll_new()) == NULL)
  {
    papplLog(system, PAPPL_LOGLEVEL_ERROR, "Unable to initialize DNS-SD.");
    return;
  }

  if ((system->dns_sd_client = avahi_client_new(avahi_threaded_poll_get(system->dns_sd_master), AVAHI_CLIENT_NO_FAIL, (AvahiClientCallback)dns_sd_client_cb, system, &error)) == NULL)
  {
    papplLog(system, PAPPL_LOGLEVEL_ERROR, "Unable to initialize DNS-SD (%d).", error);
    return;
  }

  avahi_threaded_poll_start(system->dns_sd_master);
#endif // HAVE_DNSSD
}


//
// '_papplPrinterRegisterDNSSDNoLock()' - Register a printer's DNS-SD service.
//

bool					// O - `true` on success, `false` on failure
_papplPrinterRegisterDNSSDNoLock(
    pappl_printer_t *printer)		// I - Printer
{
#if defined(HAVE_DNSSD) || defined(HAVE_AVAHI)
  pappl_system_t	*system = printer->system;
					// System
  _pappl_txt_t		txt;		// DNS-SD TXT record
  int			i,		// Looping var
			count;		// Number of values
  ipp_attribute_t	*document_format_supported,
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
#  ifdef HAVE_DNSSD
  DNSServiceErrorType	error;		// Error from mDNSResponder
#  endif // HAVE_DNSSD


  if (!printer->dns_sd_name)
    return (false);

  // Get attributes and values for the TXT record...
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

    strlcpy(ptr, value, sizeof(formats) - (size_t)(ptr - formats));
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

    strlcpy(ptr, value, sizeof(kind) - (size_t)(ptr - kind));
    ptr += strlen(ptr);

    if (ptr >= (kind + sizeof(kind) - 1))
      break;
  }

  urf[0] = '\0';
  for (i = 0, count = ippGetCount(urf_supported), ptr = urf; i < count; i ++)
  {
    value = ippGetString(urf_supported, i, NULL);

    if (ptr > urf && ptr < (urf + sizeof(urf) - 1))
      *ptr++ = ',';

    strlcpy(ptr, value, sizeof(urf) - (size_t)(ptr - urf));
    ptr += strlen(ptr);

    if (ptr >= (urf + sizeof(urf) - 1))
      break;
  }

  httpAssembleURIf(HTTP_URI_CODING_ALL, adminurl, sizeof(adminurl), "https", NULL, printer->system->hostname, printer->system->port, "/status/%d", printer->printer_id);

  // Rename the service as needed...
  if (printer->dns_sd_collision)
  {
    char	new_dns_sd_name[256];	/* New DNS-SD name */
    const char	*uuid = ippGetString(printer_uuid, 0, NULL);
					/* "printer-uuid" value */

    pthread_rwlock_wrlock(&printer->rwlock);

    snprintf(new_dns_sd_name, sizeof(new_dns_sd_name), "%s (%c%c%c%c%c%c)", printer->dns_sd_name, toupper(uuid[39]), toupper(uuid[40]), toupper(uuid[41]), toupper(uuid[42]), toupper(uuid[43]), toupper(uuid[44]));

    free(printer->dns_sd_name);
    printer->dns_sd_name = strdup(new_dns_sd_name);

    papplLogPrinter(printer, PAPPL_LOGLEVEL_INFO, "DNS-SD name collision, trying new DNS-SD service name '%s'.", printer->dns_sd_name);

    pthread_rwlock_unlock(&printer->rwlock);

    printer->dns_sd_collision = false;
  }
#endif // HAVE_DNSSD || HAVE_AVAHI

#ifdef HAVE_DNSSD
  // Build the TXT record for IPP...
  TXTRecordCreate(&txt, 1024, NULL);
  TXTRecordSetValue(&txt, "rp", (uint8_t)strlen(printer->resource) - 1, printer->resource + 1);
  if (printer->driver_data.make_and_model[0])
    TXTRecordSetValue(&txt, "ty", (uint8_t)strlen(printer->driver_data.make_and_model), printer->driver_data.make_and_model);
  TXTRecordSetValue(&txt, "adminurl", (uint8_t)strlen(adminurl), adminurl);
  if (printer->location)
    TXTRecordSetValue(&txt, "note", (uint8_t)strlen(printer->location), printer->location);
  TXTRecordSetValue(&txt, "pdl", (uint8_t)strlen(formats), formats);
  if (kind[0])
    TXTRecordSetValue(&txt, "kind", (uint8_t)strlen(kind), kind);
  if ((value = ippGetString(printer_uuid, 0, NULL)) != NULL)
    TXTRecordSetValue(&txt, "UUID", (uint8_t)strlen(value) - 9, value + 9);
  if (urf[0])
    TXTRecordSetValue(&txt, "URF", (uint8_t)strlen(urf), urf);
  TXTRecordSetValue(&txt, "TLS", 3, "1.2");
  TXTRecordSetValue(&txt, "txtvers", 1, "1");
  TXTRecordSetValue(&txt, "qtotal", 1, "1");

  // Register the _printer._tcp (LPD) service type with a port number of 0 to
  // defend our service name but not actually support LPD...
  if (printer->printer_ref)
    DNSServiceRefDeallocate(printer->printer_ref);

  printer->printer_ref = system->dns_sd_master;

  if ((error = DNSServiceRegister(&(printer->printer_ref), kDNSServiceFlagsShareConnection | kDNSServiceFlagsNoAutoRename, 0 /* interfaceIndex */, printer->dns_sd_name, "_printer._tcp", NULL /* domain */, NULL /* host */, 0 /* port */, 0 /* txtLen */, NULL /* txtRecord */, (DNSServiceRegisterReply)dns_sd_printer_callback, printer)) != kDNSServiceErr_NoError)
  {
    papplLogPrinter(printer, PAPPL_LOGLEVEL_ERROR, "Unable to register '%s._printer._tcp': %d", printer->dns_sd_name, error);
    return (false);
  }

  // Then register the corresponding IPP service types with the real port
  // number to advertise our printer...
  if (printer->ipp_ref)
    DNSServiceRefDeallocate(printer->ipp_ref);

  printer->ipp_ref = system->dns_sd_master;

  if (system->subtypes && *system->subtypes)
    snprintf(regtype, sizeof(regtype), "_ipp._tcp,%s", system->subtypes);
  else
    strlcpy(regtype, "_ipp._tcp", sizeof(regtype));

  if ((error = DNSServiceRegister(&(printer->ipp_ref), kDNSServiceFlagsShareConnection | kDNSServiceFlagsNoAutoRename, 0 /* interfaceIndex */, printer->dns_sd_name, regtype, NULL /* domain */, system->hostname, htons(system->port), TXTRecordGetLength(&txt), TXTRecordGetBytesPtr(&txt), (DNSServiceRegisterReply)dns_sd_printer_callback, printer)) != kDNSServiceErr_NoError)
  {
    papplLogPrinter(printer, PAPPL_LOGLEVEL_ERROR, "Unable to register \"%s.%s\": %d", printer->dns_sd_name, regtype, error);
    return (false);
  }

  if (printer->ipps_ref)
    DNSServiceRefDeallocate(printer->ipps_ref);

  printer->ipps_ref = system->dns_sd_master;

  if (system->subtypes && *system->subtypes)
    snprintf(regtype, sizeof(regtype), "_ipps._tcp,%s", system->subtypes);
  else
    strlcpy(regtype, "_ipps._tcp", sizeof(regtype));

  if ((error = DNSServiceRegister(&(printer->ipps_ref), kDNSServiceFlagsShareConnection | kDNSServiceFlagsNoAutoRename, 0 /* interfaceIndex */, printer->dns_sd_name, regtype, NULL /* domain */, system->hostname, htons(system->port), TXTRecordGetLength(&txt), TXTRecordGetBytesPtr(&txt), (DNSServiceRegisterReply)dns_sd_printer_callback, printer)) != kDNSServiceErr_NoError)
  {
    papplLogPrinter(printer, PAPPL_LOGLEVEL_ERROR, "Unable to register \"%s.%s\": %d", printer->dns_sd_name, regtype, error);
    return (false);
  }

  // Register the geolocation of the service...
  // TODO: Add GEOLOCATION
  // register_geo(printer);

  // Similarly, register the _http._tcp,_printer (HTTP) service type with the
  // real port number to advertise our IPP printer...
  if (printer->http_ref)
    DNSServiceRefDeallocate(printer->http_ref);

  printer->http_ref = system->dns_sd_master;

  if ((error = DNSServiceRegister(&(printer->http_ref), kDNSServiceFlagsShareConnection | kDNSServiceFlagsNoAutoRename, 0 /* interfaceIndex */, printer->dns_sd_name, "_http._tcp,_printer", NULL /* domain */, system->hostname, htons(system->port), 0 /* txtLen */, NULL /* txtRecord */, (DNSServiceRegisterReply)dns_sd_printer_callback, printer)) != kDNSServiceErr_NoError)
  {
    papplLogPrinter(printer, PAPPL_LOGLEVEL_ERROR, "Unable to register \"%s.%s\": %d", printer->dns_sd_name, "_http._tcp,_printer", error);
    return (false);
  }

  TXTRecordDeallocate(&txt);

#elif defined(HAVE_AVAHI)
  // Create the TXT record...
  txt = NULL;
  txt = avahi_string_list_add_printf(txt, "rp=%s", printer->resource + 1);
  if (printer->driver_data.make_and_model[0])
    txt = avahi_string_list_add_printf(txt, "ty=%s", printer->driver_data.make_and_model[0]);
  txt = avahi_string_list_add_printf(txt, "adminurl=%s", adminurl);
  if (printer->location)
    txt = avahi_string_list_add_printf(txt, "note=%s", printer->location);
  txt = avahi_string_list_add_printf(txt, "pdl=%s", formats);
  if ((value = ippGetString(printer_uuid, 0, NULL)) != NULL)
    txt = avahi_string_list_add_printf(txt, "UUID=%s", value + 9);
  if (urf[0])
    txt = avahi_string_list_add_printf(txt, "URF=%s", urf);
  txt = avahi_string_list_add_printf(txt, "TLS=1.2");
  txt = avahi_string_list_add_printf(txt, "txtvers=1");
  txt = avahi_string_list_add_printf(txt, "qtotal=1");

  // Register _printer._tcp (LPD) with port 0 to reserve the service name...
  avahi_threaded_poll_lock(system->dns_sd_master);

  if (printer->dns_sd_ref)
    avahi_entry_group_free(printer->dns_sd_ref);

  printer->dns_sd_ref = avahi_entry_group_new(system->dns_sd_client, (AvahiEntryGroupCallback)dns_sd_printer_callback, printer);

  avahi_entry_group_add_service_strlst(printer->dns_sd_ref, AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC, 0, printer->dns_sd_name, "_printer._tcp", NULL, NULL, 0, NULL);

  // Then register the IPP/IPPS services...
  avahi_entry_group_add_service_strlst(printer->dns_sd_ref, AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC, 0, printer->dns_sd_name, "_ipp._tcp", NULL, system->hostname, system->port, txt);
  if (system->subtypes && *system->subtypes)
  {
    char *temptypes = strdup(system->subtypes), *start, *end;

    for (start = temptypes; *start; start = end)
    {
      if ((end = strchr(start, ',')) != NULL)
	*end++ = '\0';
      else
	end = start + strlen(start);

      snprintf(regtype, sizeof(regtype), "%s._sub._ipp._tcp", start);
      avahi_entry_group_add_service_subtype(printer->dns_sd_ref, AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC, 0, printer->dns_sd_name, "_ipp._tcp", NULL, regtype);
    }

    free(temptypes);
  }

  avahi_entry_group_add_service_strlst(printer->dns_sd_ref, AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC, 0, printer->dns_sd_name, "_ipps._tcp", NULL, system->hostname, system->port, txt);
  if (system->subtypes && *system->subtypes)
  {
    char *temptypes = strdup(system->subtypes), *start, *end;

    for (start = temptypes; *start; start = end)
    {
      if ((end = strchr(start, ',')) != NULL)
	*end++ = '\0';
      else
	end = start + strlen(start);

      snprintf(regtype, sizeof(regtype), "%s._sub._ipps._tcp", start);
      avahi_entry_group_add_service_subtype(printer->dns_sd_ref, AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC, 0, printer->dns_sd_name, "_ipps._tcp", NULL, regtype);
    }

    free(temptypes);
  }

  // Register the geolocation of the service...
  // TODO: Add GEOLOCATION
  // register_geo(printer);

  // Finally _http.tcp (HTTP) for the web interface...
  avahi_entry_group_add_service_strlst(printer->dns_sd_ref, AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC, 0, printer->dns_sd_name, "_http._tcp", NULL, system->hostname, system->port, NULL);
  avahi_entry_group_add_service_subtype(printer->dns_sd_ref, AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC, 0, printer->dns_sd_name, "_http._tcp", NULL, "_printer._sub._http._tcp");

  // Commit it...
  avahi_entry_group_commit(printer->dns_sd_ref);
  avahi_threaded_poll_unlock(system->dns_sd_master);

  avahi_string_list_free(txt);
#endif // HAVE_DNSSD

  return (true);
}


//
// '_papplPrinterUnregisterDNSSDNoLock()' - Unregister a printer's DNS-SD service.
//

void
_papplPrinterUnregisterDNSSDNoLock(
    pappl_printer_t *printer)		// I - Printer
{
#if HAVE_DNSSD
// TODO: Add GEOLOCATION support
//  if (printer->geo_ref)
//  {
//    DNSServiceRemoveRecord(printer->printer_ref, printer->geo_ref, 0);
//    printer->geo_ref = NULL;
//  }
  if (printer->printer_ref)
  {
    DNSServiceRefDeallocate(printer->printer_ref);
    printer->printer_ref = NULL;
  }
  if (printer->ipp_ref)
  {
    DNSServiceRefDeallocate(printer->ipp_ref);
    printer->ipp_ref = NULL;
  }
  if (printer->ipps_ref)
  {
    DNSServiceRefDeallocate(printer->ipps_ref);
    printer->ipps_ref = NULL;
  }
  if (printer->http_ref)
  {
    DNSServiceRefDeallocate(printer->http_ref);
    printer->http_ref = NULL;
  }

#elif defined(HAVE_AVAHI)
  avahi_threaded_poll_lock(printer->system->dns_sd_master);

  if (printer->dns_sd_ref)
  {
    avahi_entry_group_free(printer->dns_sd_ref);
    printer->dns_sd_ref = NULL;
  }

  avahi_threaded_poll_unlock(printer->system->dns_sd_master);

#else
  (void)printer;
#endif /* HAVE_DNSSD */
}


//
// '_papplSystemRegisterDNSSDNoLock()' - Register a system's DNS-SD service.
//

bool					// O - `true` on success, `false` on failure
_papplSystemRegisterDNSSDNoLock(
    pappl_system_t *system)		// I - System
{
#if defined(HAVE_DNSSD) || defined(HAVE_AVAHI)
  _pappl_txt_t		txt;		// DNS-SD TXT record
#  ifdef HAVE_DNSSD
  DNSServiceErrorType	error;		// Error from mDNSResponder
#  endif // HAVE_DNSSD

  // Rename the service as needed...
  if (system->dns_sd_collision)
  {
    char	new_dns_sd_name[256];	/* New DNS-SD name */

    pthread_rwlock_wrlock(&system->rwlock);

    snprintf(new_dns_sd_name, sizeof(new_dns_sd_name), "%s (%c%c%c%c%c%c)", system->dns_sd_name, toupper(system->uuid[39]), toupper(system->uuid[40]), toupper(system->uuid[41]), toupper(system->uuid[42]), toupper(system->uuid[43]), toupper(system->uuid[44]));

    free(system->dns_sd_name);
    system->dns_sd_name = strdup(new_dns_sd_name);

    papplLog(system, PAPPL_LOGLEVEL_INFO, "DNS-SD name collision, trying new DNS-SD service name '%s'.", system->dns_sd_name);

    pthread_rwlock_unlock(&system->rwlock);

    system->dns_sd_collision = false;
  }
#endif // HAVE_DNSSD || HAVE_AVAHI

#ifdef HAVE_DNSSD
  // Build the TXT record...
  TXTRecordCreate(&txt, 1024, NULL);
  if (system->location != NULL)
    TXTRecordSetValue(&txt, "note", (uint8_t)strlen(system->location), system->location);
  TXTRecordSetValue(&txt, "UUID", (uint8_t)strlen(system->uuid) - 9, system->uuid + 9);

  // Then register the corresponding IPPS service type to advertise our system...
  if (system->ipps_ref)
    DNSServiceRefDeallocate(system->ipps_ref);

  system->ipps_ref = system->dns_sd_master;

  if ((error = DNSServiceRegister(&(system->ipps_ref), kDNSServiceFlagsShareConnection | kDNSServiceFlagsNoAutoRename, 0 /* interfaceIndex */, system->dns_sd_name, "_ipps-system._tcp", NULL /* domain */, system->hostname, htons(system->port), TXTRecordGetLength(&txt), TXTRecordGetBytesPtr(&txt), (DNSServiceRegisterReply)dns_sd_system_callback, system)) != kDNSServiceErr_NoError)
  {
    papplLog(system, PAPPL_LOGLEVEL_ERROR, "Unable to register \"%s._ipps-system._tcp\": %d", system->dns_sd_name, error);
    return (false);
  }

  // Register the geolocation of the service...
  // TODO: Add GEOLOCATION
  // register_geo(printer);

  TXTRecordDeallocate(&txt);

#elif defined(HAVE_AVAHI)
  // Create the TXT record...
  txt = NULL;
  if (system->location)
    txt = avahi_string_list_add_printf(txt, "note=%s", system->location);
  txt = avahi_string_list_add_printf(txt, "UUID=%s", system->uuid + 9);

  // Register _printer._tcp (LPD) with port 0 to reserve the service name...
  avahi_threaded_poll_lock(system->dns_sd_master);

  if (system->dns_sd_ref)
    avahi_entry_group_free(system->dns_sd_ref);

  system->dns_sd_ref = avahi_entry_group_new(system->dns_sd_client, (AvahiEntryGroupCallback)dns_sd_system_callback, system);

  avahi_entry_group_add_service_strlst(system->dns_sd_ref, AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC, 0, system->dns_sd_name, "_ipps-system._tcp", NULL, system->hostname, system->port, txt);

  // Register the geolocation of the service...
  // TODO: Add GEOLOCATION
  // register_geo(printer);

  // Commit it...
  avahi_entry_group_commit(system->dns_sd_ref);
  avahi_threaded_poll_unlock(system->dns_sd_master);

  avahi_string_list_free(txt);
#endif // HAVE_DNSSD

  return (1);
}


//
// '_papplSystemUnregisterDNSSDNoLock()' - Unregister a printer's DNS-SD service.
//

void
_papplSystemUnregisterDNSSDNoLock(
    pappl_system_t *system)		// I - System
{
#if HAVE_DNSSD
// TODO: Add GEOLOCATION support
//  if (system->geo_ref)
//  {
//    DNSServiceRemoveRecord(system->printer_ref, system->geo_ref, 0);
//    system->geo_ref = NULL;
//  }
  if (system->ipps_ref)
  {
    DNSServiceRefDeallocate(system->ipps_ref);
    system->ipps_ref = NULL;
  }

#elif defined(HAVE_AVAHI)
  avahi_threaded_poll_lock(system->dns_sd_master);

  if (system->dns_sd_ref)
  {
    avahi_entry_group_free(system->dns_sd_ref);
    system->dns_sd_ref = NULL;
  }

  avahi_threaded_poll_unlock(system->dns_sd_master);

#else
  (void)printer;
#endif /* HAVE_DNSSD */
}


#ifdef HAVE_DNSSD
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
  (void)sdRef;
  (void)flags;
  (void)domain;

  if (errorCode == kDNSServiceErr_NameConflict)
  {
    printer->dns_sd_collision             = true;
    printer->system->dns_sd_any_collision = true;
  }
  else if (errorCode)
  {
    papplLogPrinter(printer, PAPPL_LOGLEVEL_ERROR, "DNSServiceRegister for '%s' failed with error %d.", regtype, (int)errorCode);
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

  for (;;)
  {
    if ((err = DNSServiceProcessResult(system->dns_sd_master)) != kDNSServiceErr_NoError)
    {
      papplLog(system, PAPPL_LOGLEVEL_ERROR, "DNSServiceProcessResult returned %d.", err);
      break;
    }
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
  (void)domain;

  if (errorCode == kDNSServiceErr_NameConflict)
  {
    system->dns_sd_collision     = true;
    system->dns_sd_any_collision = true;
  }
  else if (errorCode)
  {
    papplLog(system, PAPPL_LOGLEVEL_ERROR, "DNSServiceRegister for '%s' failed with error %d.", regtype, (int)errorCode);
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
    pappl_system_t   *system)		// I - System
{
  if (!c)
    return;

  switch (state)
  {
    default :
        papplLog(system, PAPPL_LOGLEVEL_INFO, "Ignored Avahi state %d.", state);
	break;

    case AVAHI_CLIENT_FAILURE:
	if (avahi_client_errno(c) == AVAHI_ERR_DISCONNECTED)
	{
	  papplLog(system, PAPPL_LOGLEVEL_FATAL, "Avahi server crashed, shutting down.");
	  system->shutdown_time = time(NULL);
	}
	break;
  }
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
    printer->dns_sd_collision             = true;
    printer->system->dns_sd_any_collision = true;
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
    system->dns_sd_collision     = true;
    system->dns_sd_any_collision = true;
  }
}
#endif // HAVE_DNSSD
