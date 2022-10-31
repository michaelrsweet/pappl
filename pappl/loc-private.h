//
// Private localization header file for the Printer Application Framework
//
// Copyright © 2022 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#ifndef _PAPPL_LOC_PRIVATE_H_
#  define _PAPPL_LOC_PRIVATE_H_
#  include "system-private.h"
#  include "printer-private.h"
#  include "loc.h"
#  ifdef __cplusplus
extern "C" {
#  endif // __cplusplus


//
// Structures...
//

typedef struct _pappl_locpair_s		// String key/text pair
{
  char			*key;			// Key text
  char			*text;			// Localized text
} _pappl_locpair_t;

struct _pappl_loc_s			// Localization data
{
  pthread_rwlock_t	rwlock;			// Reader/writer lock
  pappl_system_t	*system;		// Associated system
  char			*name;			// Language/locale name
  cups_array_t		*pairs;			// String pairs
};


//
// Functions...
//

extern int		_papplLocCompare(pappl_loc_t *a, pappl_loc_t *b) _PAPPL_PRIVATE;
extern pappl_loc_t	*_papplLocCreate(pappl_system_t *system, _pappl_resource_t *r) _PAPPL_PRIVATE;
extern void		_papplLocDelete(pappl_loc_t *loc) _PAPPL_PRIVATE;
extern void		_papplLocLoadAll(pappl_system_t *system) _PAPPL_PRIVATE;
extern void		_papplLocPrintf(FILE *fp, const char *message, ...) _PAPPL_FORMAT(2,3) _PAPPL_PRIVATE;


#  ifdef __cplusplus
}
#  endif // __cplusplus
#endif // !_PAPPL_LOC_PRIVATE_H_
