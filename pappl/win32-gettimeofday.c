//
// Windows gettimeofday implementation for the Printer Application Framework
//
// Copyright © 2021 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

//
// Include necessary headers...
//

#  include "base-private.h"


//
// 'gettimeofday()' - Get the current date and time in seconds and microseconds.
//

int					// O - 0 on success, -1 on failure
gettimeofday(struct timeval *tv,	// I - Timeval struct
             void           *tz)	// I - Timezone
{
  struct _timeb timebuffer;		// Time buffer struct


  // Get the current time in seconds and milliseconds...
  _ftime(&timebuffer);

  // Convert to seconds and microseconds...
  tv->tv_sec  = (long)timebuffer.time;
  tv->tv_usec = timebuffer.millitm * 1000;

  return (0);
}
