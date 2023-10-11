//
// Xcode configuration header file for the Printer Application Framework
//
// Copyright © 2019-2023 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

// Version numbers
#define PAPPL_VERSION		"2.0.0"
#define PAPPL_VERSION_MAJOR	2
#define PAPPL_VERSION_MINOR	0


// Location of PAPPL state and spool data (when run as root)
#define PAPPL_STATEDIR		"/Library"


// Location of PAPPL domain socket (when run as root)
#define PAPPL_SOCKDIR		"/private/var/run"


// Location of CUPS config files
#define CUPS_SERVERROOT		"/private/etc/cups"


// libjpeg
#define HAVE_LIBJPEG 1


// libpng
#define HAVE_LIBPNG 1


// libusb
#define HAVE_LIBUSB 1


// libpam
#define HAVE_LIBPAM 1
#define HAVE_SECURITY_PAM_APPL_H 1
/* #undef HAVE_PAM_PAM_APPL_H */
