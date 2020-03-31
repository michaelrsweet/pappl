//
// System accessor functions for the Printer Application Framework
//
// Copyright © 2020 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

//
// Include necessary headers...
//

#include "system-private.h"


//
// Local functions...
//

static bool		add_listeners(pappl_system_t *system, const char *name, int port, int family);


//
// 'papplSystemAddListeners()' - Add network or domain socket listeners.
//
// The "name" parameter specifies a listener address.  Names starting with a
// slash (/) specify a UNIX domain socket path, otherwise the name is treated
// as a fully-qualified domain name or numeric IPv4 or IPv6 address.  If name
// is `NULL`, the "any" addresses are used.
//
// Listeners cannot be added after @link papplSystemRun@ is called.
//

bool					// O - `true` on success, `false` on failure
papplSystemAddListeners(
    pappl_system_t *system,		// I - System
    const char     *name)		// I - Hostname, domain socket filename, or `NULL`
{
  bool	ret;				// Return value


  if (!system)
  {
    return (false);
  }
  else if (system->is_running)
  {
    papplLog(system, PAPPL_LOGLEVEL_FATAL, "Tried to add listeners while system is running.");
    return (false);
  }

  if (name && *name == '/')
  {
    // Add a domain socket listener...
    ret = add_listeners(system, name, 0, AF_LOCAL);
  }
  else if (name && isdigit(*name & 255))
  {
    // Add IPv4 listener...
    ret = add_listeners(system, name, system->port, AF_INET);
  }
  else if (name && *name == '[')
  {
    // Add IPv6 listener...
    ret = add_listeners(system, name, system->port, AF_INET6);
  }
  else
  {
    // Add named listeners on both IPv4 and IPv6...
    ret = add_listeners(system, name, system->port, AF_INET) || add_listeners(system, name, system->port, AF_INET6);
  }

  return (ret);
}


//
// 'papplSystemGetAdminGroup()' - Get the current admin group, if any.
//

char *					// O - Admin group or `NULL` if none
papplSystemGetAdminGroup(
    pappl_system_t *system,		// I - System
    char           *buffer,		// I - String buffer
    size_t         bufsize)		// I - Size of string buffer
{
  char	*ret = NULL;			// Return value


  if (system && buffer && bufsize > 0)
  {
    pthread_rwlock_rdlock(&system->rwlock);

    if (system->admin_group)
    {
      strlcpy(buffer, system->admin_group, bufsize);
      ret = buffer;
    }
    else
      *buffer = '\0';

    pthread_rwlock_unlock(&system->rwlock);
  }
  else if (buffer)
    *buffer = '\0';

  return (ret);
}


//
// 'papplSystemGetAuthService()' - Get the PAM authorization service, if any.
//

const char *				// O - PAM authorization service or `NULL` if none
papplSystemGetAuthService(
    pappl_system_t *system) 	 	// I - System
{
  return (system ? system->auth_service : NULL);
}


//
// 'papplSystemGetDefaultPrinterID()' - Get the current "default-printer-id" value.
//

int					// O - "default-printer-id" value
papplSystemGetDefaultPrinterID(
    pappl_system_t *system)		// I - System
{
  return (system ? system->default_printer_id : 0);
}


//
// 'papplSystemGetDefaultPrintGroup()' - Get the default print group, if any.
//

char *					// O - Default print group or `NULL` if none
papplSystemGetDefaultPrintGroup(
    pappl_system_t *system,		// I - System
    char           *buffer,		// I - String buffer
    size_t         bufsize)		// I - Size of string buffer
{
  char	*ret = NULL;			// Return value


  if (system && buffer && bufsize > 0)
  {
    pthread_rwlock_rdlock(&system->rwlock);

    if (system->default_print_group)
    {
      strlcpy(buffer, system->default_print_group, bufsize);
      ret = buffer;
    }
    else
      *buffer = '\0';

    pthread_rwlock_unlock(&system->rwlock);
  }
  else if (buffer)
    *buffer = '\0';

  return (ret);
}


//
// 'papplSystemGetDNSSDName()' - Get the current DNS-SD service name.
//

char *					// O - Current DNS-SD service name or `NULL` for none
papplSystemGetDNSSDName(
    pappl_system_t *system,		// I - System
    char           *buffer,		// I - String buffer
    size_t         bufsize)		// I - Size of string buffer
{
  char	*ret = NULL;			// Return value


  if (system && buffer && bufsize > 0)
  {
    pthread_rwlock_rdlock(&system->rwlock);

    if (system->dns_sd_name)
    {
      strlcpy(buffer, system->dns_sd_name, bufsize);
      ret = buffer;
    }
    else
      *buffer = '\0';

    pthread_rwlock_unlock(&system->rwlock);
  }
  else if (buffer)
    *buffer = '\0';

  return (ret);
}


//
// 'papplSystemGetFirmware()' - Get the firmware name and version.
//

char *					// O - Firmware name or `NULL` for none
papplSystemGetFirmware(
    pappl_system_t *system,		// I - System
    char           *name,		// I - Name buffer
    size_t         namesize,		// I - Size of name buffer
    char           *sversion,		// I - String version buffer
    size_t         sversionsize,	// I - Size of string version buffer
    unsigned short version[4])		// O - Version number array or `NULL` for don't care
{
  char	*ret = NULL;			// Return value


  if (system && name && namesize > 0 && sversion && sversionsize > 0)
  {
    pthread_rwlock_rdlock(&system->rwlock);

    if (system->firmware_name)
    {
      strlcpy(name, system->firmware_name, namesize);
      ret = name;
    }
    else
      *name = '\0';

    if (system->firmware_sversion)
      strlcpy(sversion, system->firmware_sversion, sversionsize);

    if (version)
    {
      version[0] = system->firmware_version[0];
      version[1] = system->firmware_version[1];
      version[2] = system->firmware_version[2];
      version[3] = system->firmware_version[3];
    }

    pthread_rwlock_unlock(&system->rwlock);
  }
  else
  {
    if (name)
      *name = '\0';

    if (sversion)
      *sversion = '\0';

    if (version)
      version[0] = version[1] = version[2] = version[3] = 0;
  }

  return (ret);
}


//
// 'papplSystemGetFooterHTML()' - Get the footer HTML for the web interface, if any.
//

const char *				// O - Footer HTML or `NULL` if none
papplSystemGetFooterHTML(
    pappl_system_t *system)		// I - System
{
  return (system ? system->footer_html : NULL);
}


//
// 'papplSystemGetGeoLocation()' - Get the system geo-location string, if any.
//

char *					// O - "geo:" URI or `NULL` for none
papplSystemGetGeoLocation(
    pappl_system_t *system,		// I - System
    char           *buffer,		// I - String buffer
    size_t         bufsize)		// I - Size of string buffer
{
  char	*ret = NULL;			// Return value


  if (system && buffer && bufsize > 0)
  {
    pthread_rwlock_rdlock(&system->rwlock);

    if (system->geo_location)
    {
      strlcpy(buffer, system->geo_location, bufsize);
      ret = buffer;
    }
    else
      *buffer = '\0';

    pthread_rwlock_unlock(&system->rwlock);
  }
  else if (buffer)
    *buffer = '\0';

  return (ret);
}


//
// 'papplSystemGetLocation()' - Get the system location string, if any.
//

char *					// O - Location string or `NULL` for none
papplSystemGetLocation(
    pappl_system_t *system,		// I - System
    char           *buffer,		// I - String buffer
    size_t         bufsize)		// I - Size of string buffer
{
  char	*ret = NULL;			// Return value


  if (system && buffer && bufsize > 0)
  {
    pthread_rwlock_rdlock(&system->rwlock);

    if (system->location)
    {
      strlcpy(buffer, system->location, bufsize);
      ret = buffer;
    }
    else
      *buffer = '\0';

    pthread_rwlock_unlock(&system->rwlock);
  }
  else if (buffer)
    *buffer = '\0';

  return (ret);
}


//
// 'papplSystemGetName()' - Get the system name string, if any.
//

char *					// O - Name string or `NULL` for none
papplSystemGetName(
    pappl_system_t *system,		// I - System
    char           *buffer,		// I - String buffer
    size_t         bufsize)		// I - Size of string buffer
{
  char	*ret = NULL;			// Return value


  if (system && buffer && bufsize > 0)
  {
    pthread_rwlock_rdlock(&system->rwlock);

    if (system->name)
    {
      strlcpy(buffer, system->name, bufsize);
      ret = buffer;
    }
    else
      *buffer = '\0';

    pthread_rwlock_unlock(&system->rwlock);
  }
  else if (buffer)
    *buffer = '\0';

  return (ret);
}


//
// 'papplSystemGetNextPrinterID()' - Get the next "printer-id" value.
//

int					// O - Nxt "printer-id" value
papplSystemGetNextPrinterID(
    pappl_system_t *system)		// I - System
{
  return (system ? system->next_printer_id : 0);
}


//
// 'papplSystemGetOptions()' - Get the system options.
//

pappl_soptions_t			// O - Server options
papplSystemGetOptions(
    pappl_system_t *system)		// I - System
{
  return (system ? system->options : PAPPL_SOPTIONS_NONE);
}


//
// 'papplSystemGetServerHeader()' - Get the Server: header for HTTP responses.
//

const char *				// O - Server: header string or `NULL` for none
papplSystemGetServerHeader(
    pappl_system_t *system)		// I - System
{
  return (system ? system->server_header : NULL);
}


//
// 'papplSystemGetSessionKey()' - Get the current session key.
//
// The session key is used for web interface forms to provide CSRF protection
// and is refreshed periodically.
//

char *					// O - Session key
papplSystemGetSessionKey(
    pappl_system_t *system,		// I - System
    char           *buffer,		// I - String buffer
    size_t         bufsize)		// I - Size of string buffer
{
  time_t	curtime = time(NULL);	// Current time


  if (system && buffer && bufsize > 0)
  {
    pthread_rwlock_wrlock(&system->rwlock);

    if ((curtime - system->session_time) > 86400)
    {
      // Update session key with random data...
      snprintf(system->session_key, sizeof(system->session_key), "%08x%08x%08x%08x%08x%08x%08x%08x", _papplGetRand(), _papplGetRand(), _papplGetRand(), _papplGetRand(), _papplGetRand(), _papplGetRand(), _papplGetRand(), _papplGetRand());
      system->session_time = curtime;
    }

    strlcpy(buffer, system->session_key, bufsize);

    pthread_rwlock_unlock(&system->rwlock);
  }
  else if (buffer)
    *buffer = '\0';

  return (buffer);
}


//
// 'papplSystemGetTLSOnly()' - Get the TLS-only state of the system.
//

bool					// O - `true` if the system is only accepting TLS encrypted connections, `false` otherwise
papplSystemGetTLSOnly(
    pappl_system_t *system)		// I - System
{
  return (system ? system->tls_only : false);
}


//
// 'papplSystemGetUUID()' - Get the "system-uuid" value.
//

const char *				// O - "system-uuid" value
papplSystemGetUUID(
    pappl_system_t *system)		// I - System
{
  return (system ? system->uuid : NULL);
}


//
// 'papplSystemIteratePrinters()' - Iterate all of the printers.
//

void
papplSystemIteratePrinters(
    pappl_system_t     *system,		// I - System
    pappl_printer_cb_t cb,		// I - Callback function
    void               *data)		// I - Callback data
{
  pappl_printer_t	*printer;	// Current printer


  if (!system || !cb)
    return;

  pthread_rwlock_rdlock(&system->rwlock);
  for (printer = (pappl_printer_t *)cupsArrayFirst(system->printers); printer; printer = (pappl_printer_t *)cupsArrayNext(system->printers))
    (cb)(printer, data);
  pthread_rwlock_unlock(&system->rwlock);
}


//
// 'papplSystemSetAdminGroup()' - Set the administrative group.
//

void
papplSystemSetAdminGroup(
    pappl_system_t *system,		// I - System
    const char     *value)		// I - Admin group
{
  if (system)
  {
    pthread_rwlock_wrlock(&system->rwlock);

    free(system->admin_group);
    system->admin_group = value ? strdup(value) : NULL;

    if (system->admin_group && strcmp(system->admin_group, "none"))
    {
      char		buffer[8192];	// Buffer for strings
      struct group	grpbuf,		// Group buffer
			*grp = NULL;	// Admin group

      if (getgrnam_r(system->admin_group, &grpbuf, buffer, sizeof(buffer), &grp) || !grp)
	papplLog(system, PAPPL_LOGLEVEL_ERROR, "Unable to find admin group '%s'.", system->admin_group);
      else
	system->admin_gid = grp->gr_gid;
    }
    else
      system->admin_gid = (gid_t)-1;

    pthread_rwlock_unlock(&system->rwlock);
  }
}


//
// 'papplSystemSetDefaultPrinterID()' - Set the "default-printer-id" value.
//

void
papplSystemSetDefaultPrinterID(
    pappl_system_t *system,		// I - System
    int            default_printer_id)	// I - "default-printer-id" value
{
  if (system)
  {
    pthread_rwlock_wrlock(&system->rwlock);
    system->default_printer_id = default_printer_id;
    pthread_rwlock_unlock(&system->rwlock);
  }
}


//
// 'papplSystemSetDefaultPrintGroup()' - Set the default print group.
//

void
papplSystemSetDefaultPrintGroup(
    pappl_system_t *system,		// I - System
    const char     *value)		// I - Default print group or `NULL` for none
{
  if (system)
  {
    pthread_rwlock_wrlock(&system->rwlock);
    free(system->default_print_group);
    system->default_print_group = value ? strdup(value) : NULL;
    pthread_rwlock_unlock(&system->rwlock);
  }
}


//
// 'papplSystemSetDNSSDName()' - Set the DNS-SD service name.
//

void
papplSystemSetDNSSDName(
    pappl_system_t *system,		// I - System
    const char     *value)		// I - DNS-SD service name or `NULL` for none
{
  if (system)
  {
    pthread_rwlock_wrlock(&system->rwlock);

    free(system->dns_sd_name);
    system->dns_sd_name      = value ? strdup(value) : NULL;
    system->dns_sd_collision = false;
    system->config_time      = time(NULL);

    if (!value)
      _papplSystemUnregisterDNSSDNoLock(system);
    else
      _papplSystemRegisterDNSSDNoLock(system);

    pthread_rwlock_unlock(&system->rwlock);
  }
}


//
// 'papplSystemSetDrivers()' - Set the list of drivers and driver callback.
//

void
papplSystemSetDrivers(
    pappl_system_t     *system,		// I - System
    int                num_names,	// I - Number of driver names
    const char * const *names,		// I - Driver names array
    pappl_driver_cb_t  cb,		// I - Callback function
    void               *data)		// I - Callback data
{
  if (system)
  {
    pthread_rwlock_wrlock(&system->rwlock);

    system->config_time   = time(NULL);
    system->num_drivers   = num_names;
    system->drivers       = names;
    system->driver_cb     = cb;
    system->driver_cbdata = data;

    pthread_rwlock_unlock(&system->rwlock);
  }
}


//
// 'papplSystemSetFirmware()' - Set the firmware name and version.
//
// The firmware name can only be set prior to calling @link papplSystemRun@.
//

void
papplSystemSetFirmware(
    pappl_system_t *system,		// I - System
    const char     *name,		// I - Firmware name
    const char     *sversion,		// I - Firmware string version
    unsigned short version[4])		// I - Firmware version
{
  if (system && name && sversion && version && !system->is_running)
  {
    pthread_rwlock_wrlock(&system->rwlock);

    free(system->firmware_name);
    system->firmware_name       = strdup(name);
    system->firmware_sversion   = strdup(sversion);
    system->firmware_version[0] = version[0];
    system->firmware_version[1] = version[1];
    system->firmware_version[2] = version[2];
    system->firmware_version[3] = version[3];

    pthread_rwlock_unlock(&system->rwlock);
  }
}


//
// 'papplSystemSetFooterHTML()' - Set the footer HTML for the web interface.
//
// The footer HTML can only be set prior to calling @link papplSystemRun@.
//

void
papplSystemSetFooterHTML(
    pappl_system_t *system,		// I - System
    const char     *html)		// I - Footer HTML or `NULL` for none
{
  if (system && html && !system->is_running)
  {
    pthread_rwlock_wrlock(&system->rwlock);

    free(system->footer_html);
    system->footer_html = strdup(html);

    pthread_rwlock_unlock(&system->rwlock);
  }
}


//
// 'papplSystemSetGeoLocation()' - Set the geographic location string.
//

void
papplSystemSetGeoLocation(
    pappl_system_t *system,		// I - System
    const char     *value)		// I - "geo:" URI or `NULL` for none
{
  if (system)
  {
    pthread_rwlock_wrlock(&system->rwlock);

    free(system->geo_location);
    system->geo_location = value ? strdup(value) : NULL;
    system->config_time  = time(NULL);

// TODO: Uncomment once LOC registrations are implemented
//    _papplSystemRegisterDNSSDNoLock(system);

    pthread_rwlock_unlock(&system->rwlock);
  }
}


//
// 'papplSystemSetLocation()' - Set the system location string, if any.
//

void
papplSystemSetLocation(
    pappl_system_t *system,		// I - System
    const char     *value)		// I - Location or `NULL` for none
{
  if (system)
  {
    pthread_rwlock_wrlock(&system->rwlock);

    free(system->location);
    system->location    = value ? strdup(value) : NULL;
    system->config_time = time(NULL);

    _papplSystemRegisterDNSSDNoLock(system);

    pthread_rwlock_unlock(&system->rwlock);
  }
}


//
// 'papplSystemSetName()' - Set the system name.
//

void
papplSystemSetName(
    pappl_system_t *system,		// I - System
    const char     *value)		// I - System name
{
  if (system && value)
  {
    pthread_rwlock_wrlock(&system->rwlock);

    free(system->name);
    system->name        = strdup(value);
    system->config_time = time(NULL);

    pthread_rwlock_unlock(&system->rwlock);
  }
}


//
// 'papplSystemSetNextPrinterID()' - Set the next "printer-id" value.
//
// The next printer ID can only be set prior to calling @link papplSystemRun@.
//

void
papplSystemSetNextPrinterID(
    pappl_system_t *system,		// I - System
    int            next_printer_id)	// I - Next "printer-id" value
{
  if (system && !system->is_running)
  {
    pthread_rwlock_wrlock(&system->rwlock);
    system->next_printer_id = next_printer_id;
    pthread_rwlock_unlock(&system->rwlock);
  }
}


//
// 'papplSystemSetOperationCallback()' - Set the IPP operation callback.
//
// The operation callback can only be set prior to calling @link papplSystemRun@.
//

void
papplSystemSetOperationCallback(
    pappl_system_t    *system,		// I - System
    pappl_ipp_op_cb_t cb,		// I - Callback function
    void              *data)		// I - Callback data
{
  if (system && !system->is_running)
  {
    pthread_rwlock_wrlock(&system->rwlock);
    system->op_cb     = cb;
    system->op_cbdata = data;
    pthread_rwlock_unlock(&system->rwlock);
  }
}


//
// 'papplSystemSetSaveCallback()' - Set the save callback.
//
// The save callback can only be set prior to calling @link papplSystemRun@.
//

void
papplSystemSetSaveCallback(
    pappl_system_t  *system,		// I - System
    pappl_save_cb_t cb,			// I - Callback function
    void            *data)		// I - Callback data
{
  if (system && !system->is_running)
  {
    pthread_rwlock_wrlock(&system->rwlock);
    system->save_cb     = cb;
    system->save_cbdata = data;
    pthread_rwlock_unlock(&system->rwlock);
  }
}


//
// 'add_listeners()' - Create and add listener sockets to a system.
//

static bool				// O - `true` on success or `false` on failure
add_listeners(
    pappl_system_t *system,		// I - System
    const char     *name,		// I - Host name or `NULL` for any address
    int            port,		// I - Port number
    int            family)		// I - Address family
{
  int			sock;		// Listener socket
  http_addrlist_t	*addrlist,	// Listen addresses
			*addr;		// Current address
  char			service[255];	// Service port


  snprintf(service, sizeof(service), "%d", port);
  if ((addrlist = httpAddrGetList(name, family, service)) == NULL)
  {
    if (name && *name == '/')
      papplLog(system, PAPPL_LOGLEVEL_INFO, "Unable to lookup address(es) for '%s': %s", name, cupsLastErrorString());
    else
      papplLog(system, PAPPL_LOGLEVEL_INFO, "Unable to lookup address(es) for '%s:%d': %s", name ? name : "*", port, cupsLastErrorString());
    return (false);
  }

  if (name && *name == '/')
    papplLog(system, PAPPL_LOGLEVEL_INFO, "Listening for connections on '%s'.", name);
  else
    papplLog(system, PAPPL_LOGLEVEL_INFO, "Listening for connections on '%s:%d'.", name ? name : "*", system->port);

  for (addr = addrlist; addr && system->num_listeners < _PAPPL_MAX_LISTENERS; addr = addr->next)
  {
    if ((sock = httpAddrListen(&(addrlist->addr), port)) < 0)
    {
      char	temp[256];		// String address

      if (name && *name == '/')
	papplLog(system, PAPPL_LOGLEVEL_ERROR, "Unable to create listener socket for '%s': %s", name, cupsLastErrorString());
      else
	papplLog(system, PAPPL_LOGLEVEL_ERROR, "Unable to create listener socket for '%s:%d': %s", httpAddrString(&addr->addr, temp, (int)sizeof(temp)), system->port, cupsLastErrorString());
    }
    else
    {
      system->listeners[system->num_listeners].fd        = sock;
      system->listeners[system->num_listeners ++].events = POLLIN;
    }
  }

  httpAddrFreeList(addrlist);

  return (true);
}
