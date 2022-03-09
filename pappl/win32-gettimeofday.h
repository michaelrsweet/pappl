//
// Windows gettimeofday header file for the Printer Application Framework
//
// Copyright © 2021-2022 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#ifndef _PAPPL_WIN32_GETTIMEOFDAY_H_
#  define _PAPPL_WIN32_GETTIMEOFDAY_H_
#  include <sys/timeb.h>
#  include <time.h>


//
// Functions...
//

extern int	gettimeofday(struct timeval *tv, void *tz);


#endif // _PAPPL_WIN32_GETTIMEOFDAY_H_
