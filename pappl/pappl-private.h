//
// Private header file for the Printer Application Framework
//
// Copyright © 2019-2020 by Michael R Sweet.
// Copyright © 2010-2019 by Apple Inc.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#ifndef _PAPPL_PAPPL_PRIVATE_H_
#  define _PAPPL_PAPPL_PRIVATE_H_

//
// Include necessary headers...
//

#  include "device.h"
#  include "system-private.h"
#  include "client-private.h"
#  include "printer-private.h"
#  include "job-private.h"

#  include <limits.h>
#  include <poll.h>
#  include <sys/fcntl.h>
#  include <sys/stat.h>
#  include <sys/wait.h>

extern char **environ;


//
// Macros...
//

#  define _PAPPL_LOOKUP_STRING(bit,strings) _papplLookupString(bit, sizeof(strings) / sizeof(strings[0]), strings)
#  define _PAPPL_LOOKUP_VALUE(keyword,strings) _papplLookupValue(keyword, sizeof(strings) / sizeof(strings[0]), strings)


//
// Utility functions...
//

extern void		_papplCopyAttributes(ipp_t *to, ipp_t *from, cups_array_t *ra, ipp_tag_t group_tag, int quickcopy) _PAPPL_PRIVATE;
extern unsigned		_papplGetRand(void) _PAPPL_PRIVATE;
extern const char	*_papplLookupString(unsigned bit, size_t num_strings, const char * const *strings) _PAPPL_PRIVATE;
extern unsigned		_papplLookupValue(const char *keyword, size_t num_strings, const char * const *strings) _PAPPL_PRIVATE;


#endif // !_PAPPL_PAPPL_PRIVATE_H_
