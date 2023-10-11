//
// DNS-SD support for the Printer Application Framework
//
// Copyright © 2019-2023 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#include "pappl-private.h"


//
// Local functions...
//

static void	dns_sd_printer_callback(cups_dnssd_service_t *service, pappl_printer_t *printer, cups_dnssd_flags_t flags);
static void	dns_sd_system_callback(cups_dnssd_service_t *service, pappl_system_t *system, cups_dnssd_flags_t flags);


//
// '_papplPrinterRegisterDNSSDNoLock()' - Register a printer's DNS-SD service.
//

bool					// O - `true` on success, `false` on failure
_papplPrinterRegisterDNSSDNoLock(
    pappl_printer_t *printer)		// I - Printer
{
  bool			ret = true;	// Return value
  pappl_system_t	*system = printer->system;
					// System
  uint32_t		if_index;	// Interface index
  size_t		num_txt;	// Number of DNS-SD TXT record key/value pairs
  cups_option_t		*txt;		// DNS-SD TXT record key/value pairs
  size_t		i,		// Looping var
			count;		// Number of values
  ipp_attribute_t	*color_supported,
			*document_format_supported,
			*printer_kind,
			*printer_uuid,
			*urf_supported;	// Printer attributes
  const char		*value;		// Value string
  char			adminurl[246],	// Admin URL
			pdl[252],	// List of supported formats
			kind[251],	// List of printer-kind values
			urf[252],	// List of supported URF values
			*ptr;		// Pointer into string
  char			regtype[256];	// DNS-SD service type
  char			product[248];	// Make and model (legacy)
  int			max_width;	// Maximum media width (legacy)
  const char		*papermax;	// PaperMax string value (legacy)


  if (!printer->dns_sd_name || !printer->system->is_running)
    return (false);

  papplLogPrinter(printer, PAPPL_LOGLEVEL_DEBUG, "Registering DNS-SD name '%s' on '%s'", printer->dns_sd_name, printer->system->hostname);

  if_index = !strcmp(system->hostname, "localhost") ? CUPS_DNSSD_IF_INDEX_LOCAL : CUPS_DNSSD_IF_INDEX_ANY;

  // Get attributes and values for the TXT record...
  color_supported           = ippFindAttribute(printer->driver_attrs, "color-supported", IPP_TAG_BOOLEAN);
  document_format_supported = ippFindAttribute(printer->driver_attrs, "document-format-supported", IPP_TAG_MIMETYPE);
  printer_kind              = ippFindAttribute(printer->driver_attrs, "printer-kind", IPP_TAG_KEYWORD);
  printer_uuid              = ippFindAttribute(printer->attrs, "printer-uuid", IPP_TAG_URI);
  urf_supported             = ippFindAttribute(printer->driver_attrs, "urf-supported", IPP_TAG_KEYWORD);

  for (i = 0, count = ippGetCount(document_format_supported), ptr = pdl, *ptr = '\0'; i < count; i ++)
  {
    value = ippGetString(document_format_supported, i, NULL);

    if (!strcasecmp(value, "application/octet-stream"))
      continue;

    if (ptr > pdl && ptr < (pdl + sizeof(pdl) - 1))
      *ptr++ = ',';

    cupsCopyString(ptr, value, sizeof(pdl) - (size_t)(ptr - pdl));
    ptr += strlen(ptr);

    if (ptr >= (pdl + sizeof(pdl) - 1))
      break;
  }

  kind[0] = '\0';
  for (i = 0, count = ippGetCount(printer_kind), ptr = kind; i < count; i ++)
  {
    value = ippGetString(printer_kind, i, NULL);

    if (ptr > kind && ptr < (kind + sizeof(kind) - 1))
      *ptr++ = ',';

    cupsCopyString(ptr, value, sizeof(kind) - (size_t)(ptr - kind));
    ptr += strlen(ptr);

    if (ptr >= (kind + sizeof(kind) - 1))
      break;
  }

  snprintf(product, sizeof(product), "(%s)", printer->driver_data.make_and_model);

  for (i = 0, max_width = 0; i < (size_t)printer->driver_data.num_media; i ++)
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

    cupsCopyString(ptr, value, sizeof(urf) - (size_t)(ptr - urf));
    ptr += strlen(ptr);

    if (ptr >= (urf + sizeof(urf) - 1))
      break;
  }

  httpAssembleURIf(HTTP_URI_CODING_ALL, adminurl, sizeof(adminurl), "http", NULL, printer->system->hostname, printer->system->port, "%s/", printer->uriname);

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

      cupsCopyString(base_dns_sd_name, printer->dns_sd_name, sizeof(base_dns_sd_name));
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

  num_txt = cupsAddOption("rp", printer->resource + 1, 0, &txt);
  num_txt = cupsAddOption("ty", printer->driver_data.make_and_model, num_txt, &txt);
  num_txt = cupsAddOption("adminurl", adminurl, num_txt, &txt);
  num_txt = cupsAddOption("note", printer->location ? printer->location : "", num_txt, &txt);
  num_txt = cupsAddOption("pdl", pdl, num_txt, &txt);
  num_txt = cupsAddOption("kind", kind, num_txt, &txt);
  if ((value = ippGetString(printer_uuid, 0, NULL)) != NULL)
    num_txt = cupsAddOption("UUID", value + 9, num_txt, &txt);
  if (urf[0])
    num_txt = cupsAddOption("URF", urf, num_txt, &txt);
  num_txt = cupsAddOption("Color", ippGetBoolean(color_supported, 0) ? "T" : "F", num_txt, &txt);
  num_txt = cupsAddOption("Duplex", (printer->driver_data.sides_supported & PAPPL_SIDES_TWO_SIDED_LONG_EDGE) ? "T" : "F", num_txt, &txt);
  num_txt = cupsAddOption("TLS", "1.3", num_txt, &txt);
  num_txt = cupsAddOption("txtvers", "1", num_txt, &txt);
  num_txt = cupsAddOption("qtotal", "1", num_txt, &txt);
  num_txt = cupsAddOption("priority", "0", num_txt, &txt);
  num_txt = cupsAddOption("mopria-certified", "1.3", num_txt, &txt);
  num_txt = cupsAddOption("product", product, num_txt, &txt);
  num_txt = cupsAddOption("Fax", "F", num_txt, &txt);
  num_txt = cupsAddOption("PaperMax", papermax, num_txt, &txt);
  num_txt = cupsAddOption("Scan", "F", num_txt, &txt);

  cupsDNSSDServiceDelete(printer->dns_sd_services);
  if ((printer->dns_sd_services = cupsDNSSDServiceNew(system->dns_sd, if_index, printer->dns_sd_name, (cups_dnssd_service_cb_t)dns_sd_printer_callback, (void *)printer)) == NULL)
    ret = false;

  // Register the _printer._tcp (LPD) service type with a port number of 0 to
  // defend our service name but not actually support LPD...
  if (ret)
    ret &= cupsDNSSDServiceAdd(printer->dns_sd_services, "_printer._tcp", /*domain*/NULL, system->hostname, /*port*/0, /*num_txt*/0, /*txt*/NULL);

  // Then register the corresponding IPP service types with the real port
  // number to advertise our printer...
  if (ret)
  {
    if (system->subtypes && *system->subtypes)
      snprintf(regtype, sizeof(regtype), "_ipp._tcp,%s", system->subtypes);
    else
      cupsCopyString(regtype, "_ipp._tcp", sizeof(regtype));

    ret &= cupsDNSSDServiceAdd(printer->dns_sd_services, regtype, /*domain*/NULL, system->hostname, (uint16_t)system->port, num_txt, txt);
  }

  if (ret && !(printer->system->options & PAPPL_SOPTIONS_NO_TLS))
  {
    if (system->subtypes && *system->subtypes)
      snprintf(regtype, sizeof(regtype), "_ipps._tcp,%s", system->subtypes);
    else
      cupsCopyString(regtype, "_ipps._tcp", sizeof(regtype));

    ret &= cupsDNSSDServiceAdd(printer->dns_sd_services, regtype, /*domain*/NULL, system->hostname, (uint16_t)system->port, num_txt, txt);
  }

  // Add a geo-location record...
  if (ret && printer->geo_location)
    ret &= cupsDNSSDServiceSetLocation(printer->dns_sd_services, printer->geo_location);

  // Add raw socket service...
  if (ret && (system->options & PAPPL_SOPTIONS_RAW_SOCKET) && printer->num_raw_listeners > 0)
  {
    num_txt = cupsRemoveOption("rp", num_txt, &txt);
    num_txt = cupsRemoveOption("TLS", num_txt, &txt);

    ret &= cupsDNSSDServiceAdd(printer->dns_sd_services, "_pdl-datastream._tcp", /*domain*/NULL, system->hostname, (uint16_t)(9099 + printer->printer_id), num_txt, txt);
  }

  cupsFreeOptions(num_txt, txt);

  // Add web interface...
  if (ret)
  {
    snprintf(adminurl, sizeof(adminurl), "%s/", printer->uriname);
    num_txt = cupsAddOption("path", adminurl, 0, &txt);

    ret &= cupsDNSSDServiceAdd(printer->dns_sd_services, "_http._tcp,_printer", /*domain*/NULL, system->hostname, (uint16_t)system->port, num_txt, txt);
    cupsFreeOptions(num_txt, txt);
  }

  // Commit service...
  if (ret)
    ret &= cupsDNSSDServicePublish(printer->dns_sd_services);

  if (!ret)
    _papplPrinterUnregisterDNSSDNoLock(printer);

  return (ret);
}


//
// '_papplPrinterUnregisterDNSSDNoLock()' - Unregister a printer's DNS-SD service.
//

void
_papplPrinterUnregisterDNSSDNoLock(
    pappl_printer_t *printer)		// I - Printer
{
  cupsDNSSDServiceDelete(printer->dns_sd_services);
  printer->dns_sd_services = NULL;
}


//
// '_papplSystemRegisterDNSSDNoLock()' - Register a system's DNS-SD service.
//

bool					// O - `true` on success, `false` on failure
_papplSystemRegisterDNSSDNoLock(
    pappl_system_t *system)		// I - System
{
  bool			ret = true;	// Return value
  size_t		num_txt;	// Number of DNS-SD TXT record key/value pairs
  cups_option_t		*txt;		// DNS-SD TXT record key/value pairs
  uint32_t		if_index;	// Interface index


  // Make sure we have all of the necessary information to register the system...
  if (!system->dns_sd_name || !system->hostname || !system->uuid || !system->is_running)
    return (false);

  papplLog(system, PAPPL_LOGLEVEL_DEBUG, "Registering DNS-SD name '%s' on '%s'", system->dns_sd_name, system->hostname);

  if_index = !strcmp(system->hostname, "localhost") ? CUPS_DNSSD_IF_INDEX_LOCAL : CUPS_DNSSD_IF_INDEX_ANY;

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

      cupsCopyString(base_dns_sd_name, system->dns_sd_name, sizeof(base_dns_sd_name));
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

  if (system->dns_sd_services)
    cupsDNSSDServiceDelete(system->dns_sd_services);

  if ((system->dns_sd_services = cupsDNSSDServiceNew(system->dns_sd, if_index, system->dns_sd_name, (cups_dnssd_service_cb_t)dns_sd_system_callback, (void *)system)) == NULL)
    ret = false;

  // Register the IPPS System service type...
  if (ret && !(system->options & PAPPL_SOPTIONS_NO_TLS))
  {
    num_txt = cupsAddOption("UUID", system->uuid + 9, 0, &txt);
    if (system->location != NULL)
      num_txt = cupsAddOption("note", system->location, num_txt, &txt);

    ret &= cupsDNSSDServiceAdd(system->dns_sd_services, "_ipps-system._tcp", /*domain*/NULL, system->hostname, (uint16_t)system->port, num_txt, txt);

    cupsFreeOptions(num_txt, txt);
  }

  // Then the web interface...
  if (ret && (system->options & PAPPL_SOPTIONS_MULTI_QUEUE))
    ret &= cupsDNSSDServiceAdd(system->dns_sd_services, "_http._tcp,_printer", /*domain*/NULL, system->hostname, (uint16_t)system->port, /*num_txt*/0, /*txt*/NULL);

  if (ret && system->geo_location)
    ret &= cupsDNSSDServiceSetLocation(system->dns_sd_services, system->geo_location);

  if (ret)
    ret &= cupsDNSSDServicePublish(system->dns_sd_services);

  if (!ret)
    _papplSystemUnregisterDNSSDNoLock(system);

  return (ret);
}


//
// '_papplSystemUnregisterDNSSDNoLock()' - Unregister a printer's DNS-SD service.
//

void
_papplSystemUnregisterDNSSDNoLock(
    pappl_system_t *system)		// I - System
{
  cupsDNSSDServiceDelete(system->dns_sd_services);
  system->dns_sd_services = NULL;
}


//
// 'dns_sd_printer_callback()' - Track changes to a printer's service registrations...
//

static void
dns_sd_printer_callback(
    cups_dnssd_service_t *service,	// I - DNS-SD service
    pappl_printer_t      *printer,	// I - Printer
    cups_dnssd_flags_t   flags)		// I - DNS-SD flags
{
  (void)service;

  if (flags & CUPS_DNSSD_FLAGS_COLLISION)
  {
    _papplRWLockWrite(printer->system);
    _papplRWLockWrite(printer);

    printer->dns_sd_collision             = true;
    printer->system->dns_sd_any_collision = true;

    _papplRWUnlock(printer);
    _papplRWUnlock(printer->system);
  }

#if 0
  if (flags & CUPS_DNSSD_FLAGS_HOST_CHANGE)
  {
  }
#endif // 0

  if (flags & CUPS_DNSSD_FLAGS_ERROR)
    papplLogPrinter(printer, PAPPL_LOGLEVEL_ERROR, "DNS-SD registration of '%s' failed.", printer->dns_sd_name);
}


//
// 'dns_sd_system_callback()' - Track changes to a system's service registrations...
//

static void
dns_sd_system_callback(
    cups_dnssd_service_t *service,	// I - DNS-SD service
    pappl_system_t       *system,	// I - System
    cups_dnssd_flags_t   flags)		// I - DNS-SD flags
{
  (void)service;

  if (flags & CUPS_DNSSD_FLAGS_COLLISION)
  {
    _papplRWLockWrite(system);

    system->dns_sd_collision     = true;
    system->dns_sd_any_collision = true;

    _papplRWUnlock(system);
  }

#if 0
  if (flags & CUPS_DNSSD_FLAGS_HOST_CHANGE)
  {
  }
#endif // 0

  if (flags & CUPS_DNSSD_FLAGS_ERROR)
    papplLog(system, PAPPL_LOGLEVEL_ERROR, "DNS-SD registration of '%s' failed.", system->dns_sd_name);
}
