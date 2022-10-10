//
// Public localization header file for the Printer Application Framework
//
// Copyright © 2022 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#ifndef _PAPPL_LOC_H_
#  define _PAPPL_LOC_H_
#  include "base.h"
#  ifdef __cplusplus
extern "C" {
#  endif // __cplusplus


//
// Functions...
//

extern const char	*papplLocFormatString(pappl_loc_t *loc, char *buffer, size_t bufsize, const char *key, ...) _PAPPL_FORMAT(4,5) _PAPPL_PUBLIC;
extern const char	*papplLocGetDefaultMediaSizeName(void) _PAPPL_PUBLIC;
extern const char	*papplLocGetString(pappl_loc_t *loc, const char *key) _PAPPL_PUBLIC;


#  ifdef __cplusplus
}
#  endif // __cplusplus
#endif // !_PAPPL_LOC_H_
