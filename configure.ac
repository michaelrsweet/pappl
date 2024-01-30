dnl
dnl Configuration script for the Printer Application Framework
dnl
dnl Copyright © 2019-2024 by Michael R Sweet
dnl
dnl Licensed under Apache License v2.0.  See the file "LICENSE" for more
dnl information.
dnl

dnl ***********************************************************************
dnl
dnl Note: Using autoheader or automake on this project will break the PAPPL
dnl build system.  Use "autoconf -f" to regenerate the configure script if
dnl you make changes to this file.
dnl
dnl ***********************************************************************


dnl We need at least autoconf 2.70 for --runstatedir...
AC_PREREQ([2.70])


dnl Package name and version...
AC_INIT([pappl], [1.4.6], [https://github.com/michaelrsweet/pappl/issues], [pappl], [https://www.msweet.org/pappl])
AC_CONFIG_HEADERS([config.h])

PAPPL_VERSION="AC_PACKAGE_VERSION"
PAPPL_VERSION_MAJOR="`echo AC_PACKAGE_VERSION | awk -F. '{print $1}'`"
PAPPL_VERSION_MINOR="`echo AC_PACKAGE_VERSION | awk -F. '{printf("%d\n",$2);}'`"

AC_SUBST([PAPPL_VERSION])
AC_SUBST([PAPPL_VERSION_MAJOR])
AC_SUBST([PAPPL_VERSION_MINOR])
AC_DEFINE_UNQUOTED([PAPPL_VERSION], "$PAPPL_VERSION", [Version number])
AC_DEFINE_UNQUOTED([PAPPL_VERSION_MAJOR], $PAPPL_VERSION_MAJOR, [Major version number])
AC_DEFINE_UNQUOTED([PAPPL_VERSION_MINOR], $PAPPL_VERSION_MINOR, [Minor version number])


dnl This line is provided to ensure that you don't run the autoheader program
dnl against this project.  Doing so is completely unsupported and WILL cause
dnl problems!
AH_TOP([#error "Somebody ran autoheader on this project which is unsupported and WILL cause problems."])


dnl Get the build and host platforms and split the host_os value
AC_CANONICAL_BUILD
AC_CANONICAL_HOST

[host_os_name="$(echo $host_os | sed -e '1,$s/[0-9.]*$//g')"]
[host_os_version="$(echo $host_os | sed -e '1,$s/^[^0-9.]*//g' | awk -F. '{print $1 $2}')"]
# Linux often does not yield an OS version we can use...
AS_IF([test "x$host_os_version" = x], [
    host_os_version="0"
])


dnl Compiler options...
CFLAGS="${CFLAGS:=}"
CPPFLAGS="${CPPFLAGS:=}"
DSOFLAGS="${DSOFLAGS:=}"
LDFLAGS="${LDFLAGS:=}"
LIBS="${LIBS:=}"
OPTIM="${OPTIM:=}"

AC_SUBST([DSOFLAGS])
AC_SUBST([OPTIM])


dnl Standard programs...
AC_PROG_CC
AC_PROG_RANLIB
AC_PATH_PROG([AR], [ar])
AC_PATH_PROGS([CODE_SIGN], [codesign true])
AC_PATH_PROG([MKDIR], [mkdir])
AC_PATH_PROG([RM], [rm])
AC_PATH_PROG([RMDIR], [rmdir])
AC_PATH_PROG([LN], [ln])


dnl Figure out the correct "ar" command flags...
AS_IF([test "$ac_cv_prog_ranlib" = ":"], [
    ARFLAGS="crs"
], [
    ARFLAGS="cr"
])
AC_SUBST([ARFLAGS])


dnl install-sh
AC_MSG_CHECKING([for install-sh script])
INSTALL="$(pwd)/install-sh"
AC_SUBST([INSTALL])
AC_MSG_RESULT([using $INSTALL])


dnl Check for pkg-config, which is used for some other tests later on...
AC_PATH_TOOL([PKGCONFIG], [pkg-config])

PKGCONFIG_CFLAGS="-I\${includedir}"
PKGCONFIG_LIBS="-L\${libdir} -lpappl"
PKGCONFIG_LIBS_PRIVATE="-lm"
PKGCONFIG_REQUIRES=""
AC_SUBST([PKGCONFIG_CFLAGS])
AC_SUBST([PKGCONFIG_LIBS])
AC_SUBST([PKGCONFIG_LIBS_PRIVATE])
AC_SUBST([PKGCONFIG_REQUIRES])


dnl CUPS
AC_PATH_TOOL([CUPSCONFIG], [cups-config])
AC_MSG_CHECKING([for CUPS library v2.2 or higher])
AS_IF([$PKGCONFIG --exists cups3], [
    CUPS_VERSION="$($PKGCONFIG --modversion cups3)"
    AC_MSG_RESULT([yes, v$CUPS_VERSION])

    CPPFLAGS="$CPPFLAGS $($PKGCONFIG --cflags cups3)"
    LIBS="$LIBS $($PKGCONFIG --libs cups3)"
    PKGCONFIG_CFLAGS="$PKGCONFIG_CFLAGS $($PKGCONFIG --cflags cups3)"
    PKGCONFIG_LIBS="$PKGCONFIG_LIBS $($PKGCONFIG --libs cups3)"

    AS_IF([test x$host_os_name = xdarwin], [
	CUPS_SERVERROOT="/private/etc/cups"
    ], [
	AS_IF([test "x$($PKGCONFIG --variable=prefix cups3)" = x/usr], [
	    CUPS_SERVERROOT="/etc/cups"
	], [
	    CUPS_SERVERROOT="$($PKGCONFIG --variable=prefix cups3)/etc/cups"
	])
    ])
], [$PKGCONFIG --exists cups], [
    CUPS_VERSION="$($PKGCONFIG --modversion cups)"
    AC_MSG_RESULT([yes, v$CUPS_VERSION])

    CPPFLAGS="$CPPFLAGS $($PKGCONFIG --cflags cups)"
    LIBS="$LIBS $($PKGCONFIG --libs cups)"
    PKGCONFIG_CFLAGS="$PKGCONFIG_CFLAGS $($PKGCONFIG --cflags cups)"
    PKGCONFIG_LIBS="$PKGCONFIG_LIBS $($PKGCONFIG --libs cups)"

    CUPS_SERVERROOT="$($PKGCONFIG --variable=cups_serverroot cups)"
    AS_IF([test "x$CUPS_SERVERROOT" = x], [
	AS_IF([test x$host_os_name = xdarwin], [
	    CUPS_SERVERROOT="/private/etc/cups"
	], [
	    AS_IF([test "x$($PKGCONFIG --variable=prefix cups)" = x/usr], [
		CUPS_SERVERROOT="/etc/cups"
	    ], [
		CUPS_SERVERROOT="$($PKGCONFIG --variable=prefix cups)/etc/cups"
	    ])
	])
    ])
], [
    AS_IF([test "x$CUPSCONFIG" = x], [
    	AC_MSG_RESULT(no)
	AC_MSG_ERROR([Sorry, this software requires libcups-dev.])
    ])

    CUPS_VERSION="$($CUPSCONFIG --api-version)"
    AS_CASE(["$CUPS_VERSION"],
	[1.* | 2.0 | 2.1], [
	    AC_MSG_RESULT(no)
	    AC_MSG_ERROR([Sorry, you need CUPS 2.2.0 or higher.])
    ])

    AC_MSG_RESULT([yes, v$CUPS_VERSION])

    CPPFLAGS="$CPPFLAGS $($CUPSCONFIG --cflags)"
    DSOFLAGS="$DSOFLAGS $($CUPSCONFIG --ldflags)"
    LDFLAGS="$LDFLAGS $($CUPSCONFIG --ldflags)"
    LIBS="$LIBS $($CUPSCONFIG --image --libs)"
    PKGCONFIG_CFLAGS="$PKGCONFIG_CFLAGS $($CUPSCONFIG --cflags)"
    PKGCONFIG_LIBS="$PKGCONFIG_LIBS $($CUPSCONFIG --image --libs)"

    CUPS_SERVERROOT="$($CUPSCONFIG --serverroot)"
])
AC_DEFINE_UNQUOTED(CUPS_SERVERROOT, "$CUPS_SERVERROOT", [Location of CUPS config files])

dnl String functions...
AC_CHECK_FUNCS([strlcpy])


dnl POSIX threads...
AC_CHECK_HEADER([pthread.h])

AS_IF([test x$ac_cv_header_pthread_h != xyes], [
    AC_MSG_ERROR([Sorry, this software requires POSIX threading support.])
])

for flag in -lpthreads -lpthread -pthread; do
    AC_MSG_CHECKING([for pthread_create using $flag])
    SAVELIBS="$LIBS"
    LIBS="$flag $LIBS"
    AC_LANG([C])
    AC_LINK_IFELSE([AC_LANG_PROGRAM([#include <pthread.h>],[pthread_create(0, 0, 0, 0);])], [
        have_pthread=yes
        PKGCONFIG_LIBS_PRIVATE="$PKGCONFIG_LIBS_PRIVATE $flag"
    ], [
        LIBS="$SAVELIBS"
    ])

    AS_IF([test x$have_pthread = xyes], [
	AC_MSG_RESULT([yes])
	CPPFLAGS="$CPPFLAGS -D_THREAD_SAFE -D_REENTRANT"
	break
    ], [
	AC_MSG_RESULT([no])
    ])
done


dnl DNS-SD support...
AC_ARG_WITH([dnssd], AS_HELP_STRING([--with-dnssd=LIBRARY], [set DNS-SD library (auto, avahi, mdnsresponder)]))
PKGCONFIG_AVAHI=""

AS_IF([test "x$with_dnssd" != xmdnsresponder -a "x$with_dnssd" != xno -a "x$PKGCONFIG" != x -a x$host_os_name != xdarwin], [
    AC_MSG_CHECKING([for Avahi])
    AS_IF([$PKGCONFIG --exists avahi-client], [
	AC_MSG_RESULT([yes])
	CPPFLAGS="$CPPFLAGS $($PKGCONFIG --cflags avahi-client)"
	LIBS="$LIBS $($PKGCONFIG --libs avahi-client)"
	PKGCONFIG_AVAHI="avahi-client"
	PKGCONFIG_LIBS_PRIVATE="$PKGCONFIG_LIBS_PRIVATE $($PKGCONFIG --libs avahi-client)"
	AC_DEFINE([HAVE_DNSSD], 1, [Have DNS-SD support?])
	AC_DEFINE([HAVE_AVAHI], 1, [Have Avahi?])
    ], [
	AC_MSG_RESULT([no])
	AS_IF([test x$with_dnssd = xavahi],
	    AC_MSG_ERROR([libavahi-client-dev needed for --with-dnssd=avahi.]))
    ])
], [test x$with_dnssd = xavahi], [
    AC_MSG_ERROR([pkgconfig and libavahi-client-dev needed for --with-dnssd=avahi.])
], [test x$with_dnssd != xavahi -a "x$with_dnssd" != xno], [
    AC_CHECK_HEADER(dns_sd.h, [
        AS_CASE(["$host_os_name"],
	[darwin*], [
	    # Darwin and macOS...
	    AC_DEFINE([HAVE_DNSSD], 1, [Have DNS-SD support?])
	    AC_DEFINE([HAVE_MDNSRESPONDER], 1, [Have mDNSResponder?)])
	    LIBS="$LIBS -framework CoreFoundation -framework SystemConfiguration"
	    PKGCONFIG_LIBS_PRIVATE="$PKGCONFIG_LIBS_PRIVATE -framework CoreFoundation -framework SystemConfiguration"
	], [*], [
	    # All others...
	    AC_MSG_CHECKING(for current version of dns_sd library)
	    SAVELIBS="$LIBS"
	    LIBS="$LIBS -ldns_sd"
	    AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include <dns_sd.h>]], [[
		int constant = kDNSServiceFlagsShareConnection;
		unsigned char txtRecord[100];
		uint8_t valueLen;
		TXTRecordGetValuePtr(sizeof(txtRecord), txtRecord, "value", &valueLen);]])
	    ],[
		AC_MSG_RESULT([yes])
		AC_DEFINE([HAVE_DNSSD], 1, [Have DNS-SD support?])
		AC_DEFINE([HAVE_MDNSRESPONDER], 1, [Have mDNSResponder?)])
		PKGCONFIG_LIBS_PRIVATE="$PKGCONFIG_LIBS_PRIVATE -ldns_sd"
	    ],[
		AC_MSG_RESULT([no])
		LIBS="$SAVELIBS"
		AS_IF([test x$with_dnssd = xmdnsresponder],
		    AC_MSG_ERROR([mDNSResponder required for --with-dnssd=mdnsresponder.]))
	    ])
	])
    ])
])
AC_SUBST([PKGCONFIG_AVAHI])

dnl TLS support...
PKGCONFIG_TLS=""
AC_SUBST([PKGCONFIG_TLS])

AC_ARG_WITH([tls], AS_HELP_STRING([--with-tls=...], [use gnutls or openssl/libressl for TLS support]))
AS_IF([test "x$with_tls" = x], [
    with_tls="yes"
], [test "$with_tls" != gnutls -a "$with_tls" != libressl -a "$with_tls" != openssl -a "$with_tls" != yes], [
    AC_MSG_ERROR([Unsupported --with-tls value "$with_tls" specified.])
])

have_tls="0"

dnl First look for OpenSSL/LibreSSL...
AS_IF([test $with_tls = yes -o $with_tls = libressl -o $with_tls = openssl], [
    AS_IF([test "x$PKGCONFIG" != x], [
        AC_MSG_CHECKING([for OpenSSL/LibreSSL package])
	AS_IF([$PKGCONFIG --exists openssl], [
	    AC_MSG_RESULT([yes])
	    have_tls="1"
	    AC_DEFINE([HAVE_OPENSSL], [1], [Do we have the OpenSSL/LibreSSL library?])
	    CPPFLAGS="$CPPFLAGS $($PKGCONFIG --cflags openssl)"
	    LIBS="$LIBS $($PKGCONFIG --libs openssl)"
	    PKGCONFIG_LIBS_PRIVATE="$PKGCONFIG_LIBS_PRIVATE $($PKGCONFIG --libs openssl)"
	    PKGCONFIG_TLS="openssl,"
	], [
	    AC_MSG_RESULT([no])
	])
    ])
])

AS_IF([test $have_tls = 0], [
    AS_IF([test $with_tls = yes -o $with_tls = gnutls], [
	AS_IF([test "x$PKGCONFIG" != x], [
	    AC_MSG_CHECKING([for GNU TLS package])
	    AS_IF([$PKGCONFIG --exists gnutls], [
		AC_MSG_RESULT([yes])
	        have_tls="1"
		AC_DEFINE(HAVE_GNUTLS, 1, [Have GNU TLS?])
		CPPFLAGS="$CPPFLAGS `$PKGCONFIG --cflags gnutls`"
		LIBS="$LIBS `$PKGCONFIG --libs gnutls`"
		PKGCONFIG_LIBS_PRIVATE="$PKGCONFIG_LIBS_PRIVATE $($PKGCONFIG --libs gnutls)"
		PKGCONFIG_TLS="gnutls >= 3.0,"
	    ], [
		AC_MSG_RESULT([no])
	    ])
	])
    ])
])

AS_IF([test $have_tls = 0], [
    AC_MSG_ERROR([TLS support is required.])
])


dnl Random number support...
AC_CHECK_HEADER([sys/random.h], [
    AC_DEFINE([HAVE_SYS_RANDOM_H], 1, [Have <sys/random.h> header?])
])
AC_CHECK_FUNCS([arc4random getrandom gnutls_rnd])


dnl libjpeg...
AC_ARG_ENABLE([libjpeg], AS_HELP_STRING([--enable-libjpeg], [use libjpeg for JPEG printing, default=auto]))

PKGCONFIG_LIBJPEG=""
AC_SUBST([PKGCONFIG_LIBJPEG])

AS_IF([test x$enable_libjpeg != xno], [
    have_jpeg=no

    AS_IF([test "x$PKGCONFIG" != x], [
	AC_MSG_CHECKING([for libjpeg via pkg-config])
	AS_IF([$PKGCONFIG --exists libjpeg], [
	    AC_MSG_RESULT([yes]);
	    AC_DEFINE([HAVE_LIBJPEG], 1, [Have JPEG library?])
	    CPPFLAGS="$CPPFLAGS `$PKGCONFIG --cflags libjpeg`"
	    LIBS="$LIBS `$PKGCONFIG --libs libjpeg`"
	    PKGCONFIG_LIBJPEG="libjpeg,"
	    PKGCONFIG_LIBS_PRIVATE="$PKGCONFIG_LIBS_PRIVATE $($PKGCONFIG --libs libjpeg)"
	    have_jpeg=yes
	], [
	    AC_MSG_RESULT([no]);
	])
    ])

    AS_IF([test $have_jpeg = no], [
	AC_SEARCH_LIBS(jpeg_start_decompress, turbojpeg jpeg, [
	    AC_DEFINE([HAVE_LIBJPEG], 1, [Have JPEG library?])
	    PKGCONFIG_LIB_PRIVATE="$PKGCONFIG_LIB_PRIVATE -l$ac_cv_search_jpeg_start_decompress"
	    have_jpeg=yes
	])
    ])

    AS_IF([test x$enable_libjpeg = xyes -a $have_jpeg = no], [
	AC_MSG_ERROR([libjpeg-dev 8 or later required for --enable-libjpeg.])
    ])
])


dnl libpng...
AC_ARG_ENABLE([libpng], AS_HELP_STRING([--enable-libpng], [use libpng for PNG printing, default=auto]))

PKGCONFIG_LIBPNG=""
AC_SUBST([PKGCONFIG_LIBPNG])

AS_IF([test "x$PKGCONFIG" != x -a x$enable_libpng != xno], [
    AC_MSG_CHECKING([for libpng-1.6.x])
    AS_IF([$PKGCONFIG --exists libpng16], [
	AC_MSG_RESULT([yes]);
	AC_DEFINE([HAVE_LIBPNG], 1, [Have PNG library?])
	CPPFLAGS="$CPPFLAGS `$PKGCONFIG --cflags libpng16`"
	LIBS="$LIBS `$PKGCONFIG --libs libpng16` -lz"
	PKGCONFIG_LIBPNG="libpng >= 1.6,"
	PKGCONFIG_LIBS_PRIVATE="$PKGCONFIG_LIBS_PRIVATE $($PKGCONFIG --libs libpng16)"
    ], [
	AC_MSG_RESULT([no]);
	AS_IF([test x$enable_libpng = xyes], [
	    AC_MSG_ERROR([libpng-dev 1.6 or later required for --enable-libpng.])
	])
    ])
], [test x$enable_libpng = xyes], [
    AC_MSG_ERROR([libpng-dev 1.6 or later required for --enable-libpng.])
])


dnl libusb...
AC_ARG_ENABLE([libusb], AS_HELP_STRING([--enable-libusb], [use libusb for USB printing, default=auto]))

PKGCONFIG_LIBUSB=""
AC_SUBST([PKGCONFIG_LIBUSB])

AS_IF([test "x$PKGCONFIG" != x -a x$enable_libusb != xno], [
    AC_MSG_CHECKING([for libusb-1.0])
    AS_IF([$PKGCONFIG --exists libusb-1.0], [
	AC_MSG_RESULT([yes])
	AC_DEFINE([HAVE_LIBUSB], 1, [Have USB library?])
	CPPFLAGS="$CPPFLAGS `$PKGCONFIG --cflags libusb-1.0`"
	LIBS="$LIBS `$PKGCONFIG --libs libusb-1.0`"
	PKGCONFIG_LIBUSB="libusb-1.0 >= 1.0,"
	PKGCONFIG_LIBS_PRIVATE="$PKGCONFIG_LIBS_PRIVATE $($PKGCONFIG --libs libusb-1.0)"
	AS_IF([test "x$host_os_name" = xdarwin], [
	    LIBS="$LIBS -framework IOKit"
	    PKGCONFIG_LIBS_PRIVATE="$PKGCONFIG_LIBS_PRIVATE -framework IOKit"
	])
    ], [
	AC_MSG_RESULT([no])
	AS_IF([test x$enable_libusb = xyes], [
	    AC_MSG_ERROR([libusb-1.0-0-dev required for --enable-libusb.])
	])
    ])
], [test x$enable_libusb = xyes], [
    AC_MSG_ERROR([pkgconfig and libusb-1.0-0-dev required for --enable-libusb.])
])


dnl PAM support...
AC_ARG_ENABLE([libpam], AS_HELP_STRING([--enable-libpam], [use libpam for authentication, default=auto]))

AS_IF([test x$enable_libpam != xno], [
    dnl PAM needs dlopen...
    AC_CHECK_LIB([dl], [dlopen])
    AC_CHECK_LIB([pam], [pam_start], [
	AC_DEFINE([HAVE_LIBPAM], 1, [Have PAM library?])
	LIBS="$LIBS -lpam"
	PKGCONFIG_LIBS_PRIVATE="$PKGCONFIG_LIBS_PRIVATE -lpam"
    ])

    dnl PAM has two "standard" locations for its header...
    AC_CHECK_HEADER(security/pam_appl.h, AC_DEFINE([HAVE_SECURITY_PAM_APPL_H], 1, [Have <security/pam_appl.h> header?]))
    AC_CHECK_HEADER(pam/pam_appl.h, AC_DEFINE([HAVE_PAM_PAM_APPL_H], 1, [Have <pam/pam_appl.h> header?]))

    AS_IF([test x$ac_pam_start = xno -a x$enable_libpam = xyes], [
	AC_MSG_ERROR([libpam-dev required for --enable-libpam.])
    ])
])


dnl System status UI...
AS_IF([test "$host_os_name" = darwin], [
    LIBS="-framework AppKit $LIBS"
    PKGCONFIG_LIBS_PRIVATE="$PKGCONFIG_LIBS_PRIVATE -framework AppKit"
    SYSTEM_STATUS="system-status-macos.o"
#], [
#    AS_IF([$PKGCONFIG --exists gnome-desktop-3.0], [
#        CPPFLAGS="$($PKGCONFIG --cflags gnome-desktop-3.0) $CPPFLAGS"
#        LIBS="$($PKGCONFIG --libs gnome-desktop-3.0) $LIBS"
#	PKGCONFIG_LIBS_PRIVATE="$($PKGCONFIG --libs gnome-desktop-3.0) $PKGCONFIG_LIBS_PRIVATE"
#        SYSTEM_STATUS="system-status-gnome.o"
#    ], [
#        SYSTEM_STATUS="system-status-dummy.o"
#    ])
])
AC_SUBST([SYSTEM_STATUS])


dnl Library target...
AC_ARG_ENABLE([static], AS_HELP_STRING([--disable-static], [do not install static library]))
AC_ARG_ENABLE([shared], AS_HELP_STRING([--disable-shared], [do not install shared library]))

AS_IF([test x$enable_shared != xno], [
    AS_IF([test "$host_os_name" = darwin], [
	LIBPAPPL="libpappl.1.dylib"
    ], [
	LIBPAPPL="libpappl.so.1"
    ])

    AS_IF([test x$enable_static != xno], [
        LIBPAPPL_STATIC="libpappl.a"
    ], [
	LIBPAPPL_STATIC=""
    ])
], [
    LIBPAPPL="libpappl.a"
    LIBPAPPL_STATIC=""
    PKGCONFIG_LIBS="$PKGCONFIG_LIBS $PKGCONFIG_LIBS_PRIVATE"
    PKGCONFIG_LIBS_PRIVATE=""
])

AC_SUBST([LIBPAPPL])
AC_SUBST([LIBPAPPL_STATIC])


dnl Extra compiler options...
AC_ARG_ENABLE([debug], AS_HELP_STRING([--enable-debug], [turn on debugging, default=no]))
AC_ARG_ENABLE([debug_printfs], AS_HELP_STRING([--enable-debug-printfs], [turn on debug messages, default=no]))
AC_ARG_ENABLE([maintainer], AS_HELP_STRING([--enable-maintainer], [turn on maintainer mode, default=no]))
AC_ARG_ENABLE([sanitizer], AS_HELP_STRING([--enable-sanitizer], [build with AddressSanitizer, default=no]))
AC_ARG_ENABLE([tsanitizer], AS_HELP_STRING([--enable-tsanitizer], [build with ThreadSanitizer, default=no]))

AS_IF([test x$enable_debug = xyes], [
    OPTIM="$OPTIM -g"
    CSFLAGS=""
], [
    OPTIM="$OPTIM -g -Os"
    CSFLAGS="-o runtime"
])
AS_IF([test x$enable_debug_printfs = xyes], [
    OPTIM="$OPTIM -DDEBUG"
])

AC_SUBST([CSFLAGS])

WARNINGS=""
AC_SUBST([WARNINGS])

AS_IF([test -n "$GCC"], [
    AS_IF([test x$enable_sanitizer = xyes], [
	# Use -fsanitize=address with debugging...
	OPTIM="$OPTIM -fsanitize=address"
	AS_IF([test x$enable_tsanitizer = xyes], [
	    AC_MSG_WARN([Ignoring --enable-tsanitizer since --enable-sanitizer was specified.])
	])
    ], [test x$enable_tsanitizer = xyes], [
	# Use -fsanitize=thread with debugging...
	OPTIM="$OPTIM -fsanitize=address"
    ], [
	# Otherwise use the Fortify enhancements to catch any unbounded
	# string operations...
	CPPFLAGS="$CPPFLAGS -D_FORTIFY_SOURCE=2"
    ])

    dnl Show all standard warnings + unused variables when compiling...
    WARNINGS="-Wall -Wunused"

    dnl Drop some not-useful/unreliable warnings...
    for warning in char-subscripts format-truncation format-y2k switch unused-result; do
	AC_MSG_CHECKING([whether compiler supports -Wno-$warning])

	OLDCFLAGS="$CFLAGS"
	CFLAGS="$CFLAGS -Wno-$warning -Werror"

	AC_COMPILE_IFELSE([AC_LANG_PROGRAM()], [
	    AC_MSG_RESULT(yes)
	    WARNINGS="$WARNINGS -Wno-$warning"
        ], [
	    AC_MSG_RESULT(no)
	])

	CFLAGS="$OLDCFLAGS"
    done

    dnl Maintainer mode enables -Werror...
    AS_IF([test x$enable_maintainer = xyes], [
	WARNINGS="$WARNINGS -Werror -Wno-error=deprecated"
    ])

    dnl See if PIE options are supported...
    AC_MSG_CHECKING(whether compiler supports -fPIE)
    OLDCFLAGS="$CFLAGS"
    AS_CASE(["$host_os_name"],
    [darwin*], [
	CFLAGS="$CFLAGS -fPIC -fPIE -Wl,-pie"
	AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[]], [[]])],[
	    OLDCFLAGS="-fPIC $OLDCFLAGS"
	    LDFLAGS="-fPIE -Wl,-pie $LDFLAGS"
	    AC_MSG_RESULT(yes)
	],[
	    AC_MSG_RESULT(no)
	])
    ], [*], [
	CFLAGS="$CFLAGS -fPIC -fPIE -pie"
	AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[]], [[]])],[
	    OLDCFLAGS="-fPIC $OLDCFLAGS"
	    LDFLAGS="-fPIE -pie $LDFLAGS"
	    AC_MSG_RESULT(yes)
	],[
	    AC_MSG_RESULT(no)
	])
    ])
    CFLAGS="$OLDCFLAGS"

    dnl OS-specific compiler options...
    AC_MSG_CHECKING([for OS-specific compiler options])
    AS_CASE(["$host_os_name"], [linux*], [
    	# Make sure we get the full set of 64-bit Linux APIs from the headers...
	CPPFLAGS="$CPPFLAGS -D__USE_MISC -D_GNU_SOURCE -D_TIME_BITS=64 -D_FILE_OFFSET_BITS=64"

	# Mark read-only sections as relocatable to random addresses...
	LDFLAGS="$LDFLAGS -Wl,-z,relro,-z,now"

        AC_MSG_RESULT([-D__USE_MISC -D_GNU_SOURCE -D_TIME_BITS=64 -D_FILE_OFFSET_BITS=64 -Wl,-z,relro,-z,now])
    ], [darwin*], [
        # When not building for debug, target macOS 11 or later, "universal"
        # binaries when possible...
        AS_IF([echo "$CPPFLAGS $CFLAGS $LDFLAGS $OPTIM" | grep -q "\\-arch "], [
            # Don't add architecture/min-version flags if they are already present
            AC_MSG_RESULT([none])
        ], [echo "$CPPFLAGS $CFLAGS $LDFLAGS $OPTIM" | grep -q "\\-mmacosx-version-"], [
            # Don't add architecture/min-version flags if they are already present
            AC_MSG_RESULT([none])
        ], [test "$host_os_version" -ge 200 -a x$enable_debug != xyes], [
	    # macOS 11.0 and higher support the Apple Silicon (arm64) CPUs
	    OPTIM="$OPTIM -mmacosx-version-min=11.0 -arch x86_64 -arch arm64"
            AC_MSG_RESULT([-mmacosx-version-min=11.0 -arch x86_64 -arch arm64])
	], [
	    # Don't add architecture/min-version flags if debug enabled
            AC_MSG_RESULT([none])
	])
    ], [*], [
        AC_MSG_RESULT([none])
    ])
])


dnl Extra linker options...
AC_ARG_WITH([dsoflags], AS_HELP_STRING([--with-dsoflags=...], [Specify additional DSOFLAGS]), [
    DSOFLAGS="$withval $DSOFLAGS"
])
AC_ARG_WITH([ldflags], AS_HELP_STRING([--with-ldflags=...], [Specify additional LDFLAGS]), [
    LDFLAGS="$withval $LDFLAGS"
])

dnl State and run directories for root servers...
AS_IF([test "$prefix" = NONE], [
    # Default prefix isn't bound until AC_OUTPUT...
    realprefix="/usr/local"
], [
    realprefix="$prefix"
])

AS_IF([test "$localstatedir" = "\${prefix}/var"], [
    # Default localstatedir isn't bound until AC_OUTPUT, but the GNU default
    # doesn't match FHS or macOS/Darwin guidance...
    AS_IF([test "$host_os_name" = darwin], [
        localstatedir="/private/var"
    ], [
        localstatedir="/var"
    ])
])

AC_ARG_WITH([papplstatedir], AS_HELP_STRING([--with-papplstatedir], [specify default location of printer application state and spool files (default=LOCALSTATEDIR)]), [
    papplstatedir="$withval"
], [
    papplstatedir="$localstatedir"
])
AC_DEFINE_UNQUOTED([PAPPL_STATEDIR], ["$papplstatedir"], [Location of PAPPL state and spool data (when run as root)])

AC_ARG_WITH([papplsockdir], AS_HELP_STRING([--with-papplsockdir], [specify default location of printer application domain sockets (default=/run or /private/var/run)]), [
    papplsockdir="$withval"
], [
    AS_IF([test "$host_os_name" = darwin], [
        papplsockdir="/private/var/run"
    ], [
        papplsockdir="/run"
    ])
])
AC_DEFINE_UNQUOTED([PAPPL_SOCKDIR], ["$papplsockdir"], [Location of PAPPL domain socket (when run as root)])


dnl Generate the Makefile and pkg-config file...
AC_CONFIG_FILES([Makedefs pappl/pappl.pc])
AC_OUTPUT
