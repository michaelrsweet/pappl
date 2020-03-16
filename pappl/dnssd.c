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
static void DNSSD_API	dns_sd_callback(DNSServiceRef sdRef, DNSServiceFlags flags, DNSServiceErrorType errorCode, const char *name, const char *regtype, const char *domain, pappl_printer_t *printer);
static void		*dns_sd_run(void *data);
#elif defined(HAVE_AVAHI)
static void		dns_sd_callback(AvahiEntryGroup *p, AvahiEntryGroupState state, void *context);
static void		dns_sd_client_cb(AvahiClient *c, AvahiClientState state, void *userdata);
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

  if ((dns_sd_client = avahi_client_new(avahi_threaded_poll_get(system->dns_sd_master), AVAHI_CLIENT_NO_FAIL, dns_sd_client_cb, system, &error)) == NULL)
  {
    papplLog(system, PAPPL_LOGLEVEL_ERROR, "Unable to initialize DNS-SD (%d).", error);
    return;
  }

  avahi_threaded_poll_start(system->dns_sd_master);
#endif // HAVE_DNSSD
}


//
// '_papplPrinterRegisterDNSSD()' - Register a printer's DNS-SD service.
//

int					// O - 1 on success, 0 on failure
_papplPrinterRegisterDNSSD(
    pappl_printer_t *printer)		// I - Printer
{
#if defined(HAVE_DNSSD) || defined(HAVE_AVAHI)
  pappl_system_t	*system = printer->system;
					// System
  _pappl_txt_t		ipp_txt;	// DNS-SD IPP TXT record
  int			i,		// Looping var
			count;		// Number of values
  ipp_attribute_t	*document_format_supported,
			*printer_kind,
			*printer_location,
			*printer_make_and_model,
			*printer_more_info,
			*printer_uuid,
			*urf_supported;	// Printer attributes
  const char		*value;		// Value string
  char			formats[252],	// List of supported formats
			kind[251],	// List of printer-kind values
			urf[252],	// List of supported URF values
			*ptr;		// Pointer into string
  char			regtype[256];	// DNS-SD service type
#  ifdef HAVE_DNSSD
  DNSServiceErrorType	error;		// Error from mDNSResponder
#  endif // HAVE_DNSSD


  // Get attributes and values for the TXT record...
  document_format_supported = ippFindAttribute(printer->attrs, "document-format-supported", IPP_TAG_MIMETYPE);
  printer_kind              = ippFindAttribute(printer->attrs, "printer-kind", IPP_TAG_KEYWORD);
  printer_location          = ippFindAttribute(printer->attrs, "printer-location", IPP_TAG_TEXT);
  printer_make_and_model    = ippFindAttribute(printer->attrs, "printer-make-and-model", IPP_TAG_TEXT);
  printer_more_info         = ippFindAttribute(printer->attrs, "printer-more-info", IPP_TAG_URI);
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

    fprintf(stderr, "DNS-SD name collision, trying new DNS-SD service name '%s'.\n", printer->dns_sd_name);

    pthread_rwlock_unlock(&printer->rwlock);

    printer->dns_sd_collision = false;
  }
#endif // HAVE_DNSSD || HAVE_AVAHI

#ifdef HAVE_DNSSD
  // Build the TXT record for IPP...
  TXTRecordCreate(&ipp_txt, 1024, NULL);
  TXTRecordSetValue(&ipp_txt, "rp", (uint8_t)strlen(printer->resource) - 1, printer->resource + 1);
  if ((value = ippGetString(printer_make_and_model, 0, NULL)) != NULL)
    TXTRecordSetValue(&ipp_txt, "ty", (uint8_t)strlen(value), value);
  if ((value = ippGetString(printer_more_info, 0, NULL)) != NULL)
    TXTRecordSetValue(&ipp_txt, "adminurl", (uint8_t)strlen(value), value);
  if ((value = ippGetString(printer_location, 0, NULL)) != NULL)
    TXTRecordSetValue(&ipp_txt, "note", (uint8_t)strlen(value), value);
  TXTRecordSetValue(&ipp_txt, "pdl", (uint8_t)strlen(formats), formats);
  if (kind[0])
    TXTRecordSetValue(&ipp_txt, "kind", (uint8_t)strlen(kind), kind);
  if ((value = ippGetString(printer_uuid, 0, NULL)) != NULL)
    TXTRecordSetValue(&ipp_txt, "UUID", (uint8_t)strlen(value) - 9, value + 9);
  if (urf[0])
    TXTRecordSetValue(&ipp_txt, "URF", (uint8_t)strlen(urf), urf);
  TXTRecordSetValue(&ipp_txt, "TLS", 3, "1.2");
  TXTRecordSetValue(&ipp_txt, "txtvers", 1, "1");
  TXTRecordSetValue(&ipp_txt, "qtotal", 1, "1");

  // Register the _printer._tcp (LPD) service type with a port number of 0 to
  // defend our service name but not actually support LPD...
  if (printer->printer_ref)
    DNSServiceRefDeallocate(printer->printer_ref);

  printer->printer_ref = system->dns_sd_master;

  if ((error = DNSServiceRegister(&(printer->printer_ref), kDNSServiceFlagsShareConnection | kDNSServiceFlagsNoAutoRename, 0 /* interfaceIndex */, printer->dns_sd_name, "_printer._tcp", NULL /* domain */, NULL /* host */, 0 /* port */, 0 /* txtLen */, NULL /* txtRecord */, (DNSServiceRegisterReply)dns_sd_callback, printer)) != kDNSServiceErr_NoError)
  {
    papplLogPrinter(printer, PAPPL_LOGLEVEL_ERROR, "Unable to register '%s._printer._tcp': %d", printer->dns_sd_name, error);
    return (0);
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

  if ((error = DNSServiceRegister(&(printer->ipp_ref), kDNSServiceFlagsShareConnection | kDNSServiceFlagsNoAutoRename, 0 /* interfaceIndex */, printer->dns_sd_name, regtype, NULL /* domain */, system->name, htons(system->port), TXTRecordGetLength(&ipp_txt), TXTRecordGetBytesPtr(&ipp_txt), (DNSServiceRegisterReply)dns_sd_callback, printer)) != kDNSServiceErr_NoError)
  {
    papplLogPrinter(printer, PAPPL_LOGLEVEL_ERROR, "Unable to register \"%s.%s\": %d", printer->dns_sd_name, regtype, error);
    return (0);
  }

  if (printer->ipps_ref)
    DNSServiceRefDeallocate(printer->ipps_ref);

  printer->ipps_ref = system->dns_sd_master;

  if (system->subtypes && *system->subtypes)
    snprintf(regtype, sizeof(regtype), "_ipps._tcp,%s", system->subtypes);
  else
    strlcpy(regtype, "_ipps._tcp", sizeof(regtype));

  if ((error = DNSServiceRegister(&(printer->ipps_ref), kDNSServiceFlagsShareConnection | kDNSServiceFlagsNoAutoRename, 0 /* interfaceIndex */, printer->dns_sd_name, regtype, NULL /* domain */, system->name, htons(system->port), TXTRecordGetLength(&ipp_txt), TXTRecordGetBytesPtr(&ipp_txt), (DNSServiceRegisterReply)dns_sd_callback, printer)) != kDNSServiceErr_NoError)
  {
    papplLogPrinter(printer, PAPPL_LOGLEVEL_ERROR, "Unable to register \"%s.%s\": %d", printer->dns_sd_name, regtype, error);
    return (0);
  }

  // Register the geolocation of the service...
  // TODO: Add GEOLOCATION
  // register_geo(printer);

  // Similarly, register the _http._tcp,_printer (HTTP) service type with the
  // real port number to advertise our IPP printer...
  if (printer->http_ref)
    DNSServiceRefDeallocate(printer->http_ref);

  printer->http_ref = system->dns_sd_master;

  if ((error = DNSServiceRegister(&(printer->http_ref), kDNSServiceFlagsShareConnection | kDNSServiceFlagsNoAutoRename, 0 /* interfaceIndex */, printer->dns_sd_name, "_http._tcp,_printer", NULL /* domain */, system->name, htons(system->port), 0 /* txtLen */, NULL /* txtRecord */, (DNSServiceRegisterReply)dns_sd_callback, printer)) != kDNSServiceErr_NoError)
  {
    papplLogPrinter(printer, PAPPL_LOGLEVEL_ERROR, "Unable to register \"%s.%s\": %d", printer->dns_sd_name, "_http._tcp,_printer", error);
    return (0);
  }

  TXTRecordDeallocate(&ipp_txt);

#elif defined(HAVE_AVAHI)
  // Create the TXT record...
  ipp_txt = NULL;
  ipp_txt = avahi_string_list_add_printf(ipp_txt, "rp=%s", printer->resource + 1);
  if ((value = ippGetString(printer_make_and_model, 0, NULL)) != NULL)
    ipp_txt = avahi_string_list_add_printf(ipp_txt, "ty=%s", value);
  if ((value = ippGetString(printer_more_info, 0, NULL)) != NULL)
    ipp_txt = avahi_string_list_add_printf(ipp_txt, "adminurl=%s", value);
  if ((value = ippGetString(printer_location, 0, NULL)) != NULL)
    ipp_txt = avahi_string_list_add_printf(ipp_txt, "note=%s", value);
  ipp_txt = avahi_string_list_add_printf(ipp_txt, "pdl=%s", formats);
  if ((value = ippGetString(printer_uuid, 0, NULL)) != NULL)
    ipp_txt = avahi_string_list_add_printf(ipp_txt, "UUID=%s", value + 9);
  if (urf[0])
    ipp_txt = avahi_string_list_add_printf(ipp_txt, "URF=%s", urf);
  ipp_txt = avahi_string_list_add_printf(ipp_txt, "TLS=1.2");
  ipp_txt = avahi_string_list_add_printf(ipp_txt, "txtvers=1");
  ipp_txt = avahi_string_list_add_printf(ipp_txt, "qtotal=1");

  // Register _printer._tcp (LPD) with port 0 to reserve the service name...
  avahi_threaded_poll_lock(system->dns_sd_master);

  if (printer->dns_sd_ref)
    avahi_entry_group_free(printer->dns_sd_ref);

  printer->dns_sd_ref = avahi_entry_group_new(system->dns_sd_client, dns_sd_callback, printer);

  avahi_entry_group_add_service_strlst(printer->dns_sd_ref, AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC, 0, printer->dns_sd_name, "_printer._tcp", NULL, NULL, 0, NULL);

  // Then register the IPP/IPPS services...
  avahi_entry_group_add_service_strlst(printer->dns_sd_ref, AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC, 0, printer->dns_sd_name, "_ipp._tcp", NULL, system->name, system->port, ipp_txt);
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

  avahi_entry_group_add_service_strlst(printer->dns_sd_ref, AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC, 0, printer->dns_sd_name, "_ipps._tcp", NULL, system->name, system->port, ipp_txt);
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
  avahi_entry_group_add_service_strlst(printer->dns_sd_ref, AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC, 0, printer->dns_sd_name, "_http._tcp", NULL, system->name, system->port, NULL);
  avahi_entry_group_add_service_subtype(printer->dns_sd_ref, AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC, 0, printer->dns_sd_name, "_http._tcp", NULL, "_printer._sub._http._tcp");

  // Commit it...
  avahi_entry_group_commit(printer->dns_sd_ref);
  avahi_threaded_poll_unlock(system->dns_sd_master);

  avahi_string_list_free(ipp_txt);
#endif // HAVE_DNSSD

  return (1);
}


//
// '_papplPrinterUnregisterDNSSD()' - Unregister a printer's DNS-SD service.
//

void
_papplPrinterUnregisterDNSSD(
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
  avahi_threaded_poll_lock(dns_sd_master);

  if (printer->dns_sd_ref)
  {
    avahi_entry_group_free(printer->dns_sd_ref);
    printer->dns_sd_ref = NULL;
  }

  avahi_threaded_poll_unlock(dns_sd_master);

#else
  (void)printer;
#endif /* HAVE_DNSSD */
}


#ifdef HAVE_DNSSD
//
// 'dns_sd_callback()' - Handle DNS-SD registration events.
//

static void DNSSD_API
dns_sd_callback(
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
    printer->dns_sd_collision         = true;
    printer->system->dns_sd_collision = true;
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

#elif defined(HAVE_AVAHI)
//
// 'dns_sd_callback()' - Handle DNS-SD registration events.
//

static void
dns_sd_callback(
    AvahiEntryGroup      *srv,		// I - Service
    AvahiEntryGroupState state,		// I - Registration state
    void                 *context)	// I - Printer
{
  pappl_printer_t *printer = (pappl_printer_t *)context;
 					/* Printer */


  (void)srv;

  if (state == AVAHI_ENTRY_GROUP_COLLISION)
  {
    printer->dns_sd_collision         = true;
    printer->system->dns_sd_collision = true;
  }
}


//
//'dns_sd_client_cb()' - Client callback for Avahi.
//
// Called whenever the client or server state changes...
//

static void
dns_sd_client_cb(
    AvahiClient      *c,		// I - Client
    AvahiClientState state,		// I - Current state
    void             *userdata)		// I - System
{
  pappl_system_t	*system = (pappl_system_t *)userdata;
					// System


  if (!c)
    return;

  switch (state)
  {
    default :
        papplLog(system, PAPPL_LOG_INFO, "Ignored Avahi state %d.", state);
	break;

    case AVAHI_CLIENT_FAILURE:
	if (avahi_client_errno(c) == AVAHI_ERR_DISCONNECTED)
	{
	  papplLog(system, PAPPL_LOG_FATAL, "Avahi server crashed, shutting down.");
	  system->shutdown_time = time(NULL);
	}
	break;
  }
}
#endif // HAVE_DNSSD
