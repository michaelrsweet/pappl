//
// Xcode configuration header file for the Printer Application Framework
//
// Copyright © 2019-2024 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

// Version numbers
#define PAPPL_VERSION		"1.4.6"
#define PAPPL_VERSION_MAJOR	1
#define PAPPL_VERSION_MINOR	4


// Location of PAPPL state and spool data (when run as root)
#define PAPPL_STATEDIR		"/Library"


// Location of PAPPL domain socket (when run as root)
#define PAPPL_SOCKDIR		"/private/var/run"


// Location of CUPS config files
#define CUPS_SERVERROOT		"/private/etc/cups"


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
#define HAVE_LIBUSB 1


// libpam
#define HAVE_LIBPAM 1
#define HAVE_SECURITY_PAM_APPL_H 1
/* #undef HAVE_PAM_PAM_APPL_H */


// String functions
#define HAVE_STRLCPY 1


// Random number support
#define HAVE_SYS_RANDOM_H 1
#define HAVE_ARC4RANDOM 1
/* #undef HAVE_GETRANDOM */
/* #undef HAVE_GNUTLS_RND */
