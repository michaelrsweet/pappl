//
// Xcode configuration header file for the Printer Application Framework
//
// Copyright © 2019-2026 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

// Version numbers
#define PAPPL_VERSION		"2.0b1"
#define PAPPL_VERSION_MAJOR	2
#define PAPPL_VERSION_MINOR	0


// Location of PAPPL state and spool data (when run as root)
#define PAPPL_STATEDIR		"/Library"


// Location of PAPPL domain socket (when run as root)
#define PAPPL_SOCKDIR		"/private/var/run"


// statfs/statvfs and the corresponding headers
#define HAVE_STATFS 1
/* #undef HAVE_STATVFS */
#define HAVE_SYS_MOUNT_H 1
/* #undef HAVE_SYS_STATFS_H */
/* #undef HAVE_SYS_STATVFS_H */
/* #undef HAVE_SYS_VFS_H */


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


// PDFio
#define HAVE_PDFIO 1


// landlock
/*#undef HAVE_LINUX_LANDLOCK_H */
