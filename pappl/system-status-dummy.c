//
// Dummy system status UI for the Printer Application Framework
//
// Copyright © 2022 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

//
// Include necessary headers...
//

#include "pappl-private.h"


//
// '_papplSystemStatusCallback()' - Handle system events...
//

void
_papplSystemStatusCallback(
    pappl_system_t  *system,		// I - System
    pappl_printer_t *printer,		// I - Printer, if any
    pappl_job_t     *job,		// I - Job, if any
    pappl_event_t   event,		// I - Event
    void            *data)		// I - System UI data
{
  (void)system;
  (void)printer;
  (void)job;
  (void)event;
  (void)data;
}


//
// '_papplSystemStatusUI()' - Show/run the system status UI.
//

void
_papplSystemStatusUI(
    pappl_system_t *system)		// I - System
{
  (void)system;
}
