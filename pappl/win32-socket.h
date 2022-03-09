//
// Windows socket header file for the Printer Application Framework
//
// Copyright © 2021-2022 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#ifndef _PAPPL_WIN32_SOCKET_H_
#  define _PAPPL_WIN32_SOCKET_H_
#  include <winsock2.h>
#  include <windows.h>


//
// The BSD socket API is bolted on the side of Windows, so some names are
// the same and some are different...
//

typedef ULONG nfds_t;
#  define poll(fds,nfds,timeout)	WSAPoll(fds,nfds,timeout)

#endif // _PAPPL_WIN32_SOCKET_H_
