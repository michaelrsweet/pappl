//
// Visual Studio configuration header file for the Printer Application Framework
//
// Copyright © 2019-2024 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//


//
// Define MS runtime stuff in the standard headers...
//

#define _CRT_RAND_S


//
// Include necessary headers...
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <io.h>
#include <direct.h>
#include <process.h>


//
// Microsoft renames the POSIX functions to _name, and introduces
// a broken compatibility layer using the original names.  As a result,
// random crashes can occur when, for example, strdup() allocates memory
// from a different heap than used by malloc() and free().
//
// To avoid moronic problems like this, we #define the POSIX function
// names to the corresponding non-standard Microsoft names.
//

#define access		_access
#define close		_close
#define fileno		_fileno
#define lseek		_lseek
#define mkdir(d,p)	_mkdir(d)
#define open		_open
#define read	        _read
#define rmdir		_rmdir
#define snprintf	_snprintf
#define strdup		_strdup
#define unlink		_unlink
#define vsnprintf	_vsnprintf
#define write(f,b,l)	_write((f), (b), (unsigned)(l))


//
// Microsoft "safe" functions use a different argument order than POSIX...
//

#define gmtime_r(t,tm)	gmtime_s(tm,t)
#define localtime_r(t,tm) localtime_s(tm,t)


//
// Map the POSIX strcasecmp() and strncasecmp() functions to the Win32
// _stricmp() and _strnicmp() functions...
//

#define strcasecmp	_stricmp
#define strncasecmp	_strnicmp


//
// Map the POSIX sleep() and usleep() functions to the Win32 Sleep() function...
//

typedef unsigned long useconds_t;
#define sleep(X)	Sleep(1000 * (X))
#define usleep(X)	Sleep((X)/1000)


//
// POSIX getpid() is Windows GetCurrentProcessId()
//

#define getpid		GetCurrentProcessId


//
// Map various parameters to Posix style system calls
//

#  define F_OK		00
#  define X_OK		0
#  define W_OK		02
#  define R_OK		04
#  define O_CLOEXEC	0
#  define O_CREAT	_O_CREAT
#  define O_EXCL	_O_EXCL
#  define O_NOFOLLOW	0
#  define O_RDONLY	_O_RDONLY
#  define O_TRUNC	_O_TRUNC
#  define O_WRONLY	_O_WRONLY
#  define S_ISCHR(m)	((m) & S_IFCHR)
#  define S_ISDIR(m)	((m) & S_IFDIR)
#  define S_ISREG(m)	((m) & S_IFREG)


// Version numbers
#define PAPPL_VERSION "1.4.6"
#define PAPPL_VERSION_MAJOR 1
#define PAPPL_VERSION_MINOR 4


// Location of PAPPL state and spool data (when run as root)
#define PAPPL_STATEDIR "C:/CUPS/var"


// Location of PAPPL domain socket (when run as root)
/* #undef PAPPL_SOCKDIR */


// Location of CUPS config files
#define CUPS_SERVERROOT "C:/CUPS/etc"


// DNS-SD (mDNSResponder or Avahi)
#define HAVE_DNSSD 1
#define HAVE_MDNSRESPONDER 1
/* #undef HAVE_AVAHI */


// GNU TLS, LibreSSL/OpenSSL
/* #undef HAVE_GNUTLS */
#define HAVE_OPENSSL 1


// libjpeg
#define HAVE_LIBJPEG 1


// libpng
#define HAVE_LIBPNG 1


// libusb
/* #undef HAVE_LIBUSB */


// libpam
/* #undef HAVE_LIBPAM */
/* #undef HAVE_SECURITY_PAM_APPL_H */
/* #undef HAVE_PAM_PAM_APPL_H */


// String functions
/* #undef HAVE_STRLCPY */


// Random number support
/* #undef HAVE_SYS_RANDOM_H */
/* #undef HAVE_ARC4RANDOM */
/* #undef HAVE_GETRANDOM */
/* #undef HAVE_GNUTLS_RND */
