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
// 'papplSystemGet()' - .
//

int					// O - "default-printer-id" value
papplSystemGetDefaultPrinterID(
    pappl_system_t *system)		// I - System
{
  return (system ? system->default_printer_id : 0);
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
// 'papplSystemGetSessionKey()' - Get the current session key.
//

const char *				// O - Session key
papplSystemGetSessionKey(
    pappl_system_t *system)		// I - System
{
  return (system ? system->session_key : NULL);
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
// 'papplSystemSetDriverCallback()' - Set the driver callback.
//

void
papplSystemSetDriverCallback(
    pappl_system_t    *system,		// I - System
    pappl_driver_cb_t cb,		// I - Callback function
    void              *data)		// I - Callback data
{
  if (system)
  {
    pthread_rwlock_wrlock(&system->rwlock);
    system->driver_cb     = cb;
    system->driver_cbdata = data;
    pthread_rwlock_unlock(&system->rwlock);
  }
}


//
// 'papplSystemSetNextPrinterID()' - Set the next "printer-id" value.
//

void
papplSystemSetNextPrinterID(
    pappl_system_t *system,		// I - System
    int            next_printer_id)	// I - Next "printer-id" value
{
  if (system)
  {
    pthread_rwlock_wrlock(&system->rwlock);
    system->next_printer_id = next_printer_id;
    pthread_rwlock_unlock(&system->rwlock);
  }
}


//
// 'papplSystemSetOperationCallback()' - Set the IPP operation callback.
//

void
papplSystemSetOperationCallback(
    pappl_system_t    *system,		// I - System
    pappl_ipp_op_cb_t cb,		// I - Callback function
    void              *data)		// I - Callback data
{
  if (system)
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

void
papplSystemSetSaveCallback(
    pappl_system_t  *system,		// I - System
    pappl_save_cb_t cb,			// I - Callback function
    void            *data)		// I - Callback data
{
  if (system)
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
    papplLog(system, PAPPL_LOGLEVEL_INFO, "Unable to lookup address(es) for %s:%d: %s", name ? name : "*", port, cupsLastErrorString());
    return (false);
  }

  for (addr = addrlist; addr && system->num_listeners < _PAPPL_MAX_LISTENERS; addr = addr->next)
  {
    if ((sock = httpAddrListen(&(addrlist->addr), port)) < 0)
    {
      char	temp[256];		// String address

      papplLog(system, PAPPL_LOGLEVEL_ERROR, "Unable to create listener socket for %s:%d: %s", httpAddrString(&addr->addr, temp, (int)sizeof(temp)), system->port, cupsLastErrorString());
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
