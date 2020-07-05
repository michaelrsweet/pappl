//
// Private log header file for the Printer Application Framework
//
// Copyright © 2020 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#ifndef _PAPPL_LOG_PRIVATE_H_
#  define _PAPPL_LOG_PRIVATE_H_

//
//  inlcude necessary headers
//

#  include "base-private.h"
#  include  "log.h"

//
//  Functions
//

extern void   _papplLogCheck(pappl_system_t *system) _PAPPL_PRIVATE;
extern void   _papplPrinterIteratorStatusCallback(pappl_printer_t *printer, pappl_system_t *system) _PAPPL_PRIVATE;

#endif // !_PAPPL_LOG_PRIVATE_H_