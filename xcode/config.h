//
// Xcode configuration header file for the Printer Application Framework
//
// Copyright © 2019-2020 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

// Version numbers
#define PAPPL_VERSION		"1.0rc1"
#define PAPPL_VERSION_MAJOR	1
#define PAPPL_VERSION_MINOR	0


// DNS-SD (mDNSResponder or Avahi)
#define HAVE_DNSSD 1
/* #undef HAVE_AVAHI */


// GNU TLS
/* #undef HAVE_GNUTLS */


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
