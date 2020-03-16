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
// 'papplSystemAddListener()' - Add a network or domain socket listener.
//

void
papplSystemAddListener(
    pappl_system_t *system,		// I - System
    const char     *name)		// I - Hostname, domain socket filename, or `NULL`
{
  (void)system;
  (void)name;
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
