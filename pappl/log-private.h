//
// Private log header file for the Printer Application Framework
//
// Copyright © 2020-2022 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#ifndef _PAPPL_LOG_PRIVATE_H_
#  define _PAPPL_LOG_PRIVATE_H_
#  include "base-private.h"
#  include "log.h"


//
// Functions...
//

extern void	_papplLogAttributes(pappl_client_t *client, const char *title, ipp_t *ipp, bool is_response) _PAPPL_PRIVATE;
extern void	_papplLogOpen(pappl_system_t *system) _PAPPL_PRIVATE;

#endif // !_PAPPL_LOG_PRIVATE_H_
