dnl
dnl Configuration script for the Printer Application Framework
dnl
dnl Copyright © 2019-2021 by Michael R Sweet
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
AC_INIT([pappl], [1.1b1], [https://github.com/michaelrsweet/pappl/issues], [pappl], [https://www.msweet.org/pappl])
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


dnl GNU autoheader, automake, etc. will break the build system, this macro
dnl causes the autoheader utility to fail...
AC_DEFINE([PAPPL_DOES_NOT_USE_AUTOHEADER_OR_AUTOMAKE])


dnl Get the build and host platforms and split the host_os value
AC_CANONICAL_BUILD
AC_CANONICAL_HOST

[host_os_name=`echo $host_os | sed -e '1,$s/[0-9.]*$//g'`]
[host_os_version=`echo $host_os | sed -e '1,$s/^[^0-9.]*//g' | awk -F. '{print $1 $2}'`]
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

AC_SUBST(DSOFLAGS)


dnl Standard programs...
AC_PROG_CC(clang cc gcc)
AC_PROG_RANLIB
AC_PATH_PROG(AR,ar)
AC_PATH_PROGS(CODE_SIGN, codesign true)
AC_PATH_PROG(MKDIR,mkdir)
AC_PATH_PROG(RM,rm)
AC_PATH_PROG(LN,ln)


dnl Figure out the correct "ar" command flags...
AS_IF([test "$ac_cv_prog_ranlib" = ":"], [
    ARFLAGS="crs"
], [
    ARFLAGS="cr"
])
AC_SUBST(ARFLAGS)


dnl install-sh
AC_MSG_CHECKING([for install-sh script])
INSTALL="`pwd`/install-sh"
AC_SUBST(INSTALL)
AC_MSG_RESULT([using $INSTALL])


dnl CUPS
AC_PATH_TOOL(CUPSCONFIG, cups-config)

AS_IF([test "x$CUPSCONFIG" = x], [
    AC_MSG_ERROR([Sorry, this software requires libcups-dev.])
])

AS_CASE(["`$CUPSCONFIG --api-version`"],
    [1.* | 2.0 | 2.1], [
        AC_MSG_ERROR([Sorry, you need CUPS 2.2.0 or higher.])
])

CUPS_SERVERROOT="`$CUPSCONFIG --serverroot`"
AC_DEFINE_UNQUOTED(CUPS_SERVERROOT, "$CUPS_SERVERROOT", [Location of CUPS config files])


dnl Check for pkg-config, which is used for some other tests later on...
AC_PATH_TOOL(PKGCONFIG, pkg-config)


dnl CUPS library...
AC_ARG_ENABLE(static_cups,
    AS_HELP_STRING([--enable-static-cups], [use static CUPS libraries, default=no]))

AS_IF([test x$enable_static_cups = xyes], [
    CPPFLAGS="$CPPFLAGS `cups-config --static --cflags`"
    DSOFLAGS="$DSOFLAGS `cups-config --static --ldflags`"
    LDFLAGS="$LDFLAGS `cups-config --static --ldflags`"
    LIBS="$LIBS `cups-config --image --static --libs`"
], [
    CPPFLAGS="$CPPFLAGS `cups-config --cflags`"
    DSOFLAGS="$DSOFLAGS `cups-config --ldflags`"
    LDFLAGS="$LDFLAGS `cups-config --ldflags`"
    LIBS="$LIBS `cups-config --image --libs`"
])


dnl String functions...
AC_CHECK_FUNCS(strlcpy)


dnl POSIX threads...
AC_CHECK_HEADER(pthread.h)

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
AC_ARG_WITH(dnssd, AS_HELP_STRING([--with-dnssd=LIBRARY], [set DNS-SD library (auto, avahi, mdnsresponder)]))

AS_IF([test "x$with_dnssd" != xmdnsresponder -a "x$with_dnssd" != xno -a "x$PKGCONFIG" != x -a x$host_os_name != xdarwin], [
    AC_MSG_CHECKING(for Avahi)
    AS_IF([$PKGCONFIG --exists avahi-client], [
	AC_MSG_RESULT([yes])
	CPPFLAGS="$CPPFLAGS `$PKGCONFIG --cflags avahi-client`"
	LIBS="$LIBS `$PKGCONFIG --libs avahi-client`"
	AC_DEFINE([HAVE_AVAHI], 1, [DNS-SD (mDNSResponder or Avahi)])
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
	    AC_DEFINE([HAVE_DNSSD], 1, [DNS-SD (mDNSResponder or Avahi)])
	    LIBS="$LIBS -framework CoreFoundation -framework SystemConfiguration"
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
		AC_DEFINE([HAVE_DNSSD], 1, [DNS-SD (mDNSResponder or Avahi)])
	    ],[
		AC_MSG_RESULT([no])
		LIBS="$SAVELIBS"
		AS_IF([test x$with_dnssd = xmdnsresponder],
		    AC_MSG_ERROR([mDNSResponder required for --with-dnssd=mdnsresponder.]))
	    ])
	])
    ])
])


dnl GNU TLS...
PKGCONFIG_GNUTLS=""
AC_SUBST(PKGCONFIG_GNUTLS)

AS_IF([test "x$PKGCONFIG" != x], [
    AS_IF([$PKGCONFIG --exists gnutls], [
	CPPFLAGS="$CPPFLAGS `$PKGCONFIG --cflags gnutls`"
	LIBS="$LIBS `$PKGCONFIG --libs gnutls`"
	AC_DEFINE(HAVE_GNUTLS, 1, [GNU TLS])
	PKGCONFIG_GNUTLS="gnutls >= 3.0,"
    ])
])


dnl Random number support...
AC_CHECK_HEADER(sys/random.h, AC_DEFINE([HAVE_SYS_RANDOM_H], 1, [Have <sys/random.h> header?]))
AC_CHECK_FUNCS(arc4random getrandom gnutls_rnd)


dnl libjpeg...
AC_ARG_ENABLE(libjpeg, AS_HELP_STRING([--enable-libjpeg], [use libjpeg for JPEG printing, default=auto]))

PKGCONFIG_LIBJPEG=""
AC_SUBST(PKGCONFIG_LIBJPEG)

AS_IF([test x$enable_libjpeg != xno], [
    have_jpeg=no

    AS_IF([test "x$PKGCONFIG" != x], [
	AC_MSG_CHECKING([for libjpeg via pkg-config])
	AS_IF([$PKGCONFIG --exists libjpeg], [
	    AC_MSG_RESULT([yes]);
	    AC_DEFINE([HAVE_LIBJPEG], 1, [libjpeg])
	    CPPFLAGS="$CPPFLAGS `$PKGCONFIG --cflags libjpeg`"
	    LIBS="$LIBS `$PKGCONFIG --libs libjpeg`"
	    PKGCONFIG_LIBJPEG="libjpeg,"
	    have_jpeg=yes
	], [
	    AC_MSG_RESULT([no]);
	])
    ])

    AS_IF([test $have_jpeg = no], [
	AC_SEARCH_LIBS(jpeg_start_decompress, turbojpeg jpeg, [
	    AC_DEFINE([HAVE_LIBJPEG], 1, [libjpeg])
	    have_jpeg=yes
	])
    ])

    AS_IF([test x$enable_libjpeg = xyes -a $have_jpeg = no], [
	AC_MSG_ERROR([libjpeg-dev 8 or later required for --enable-libjpeg.])
    ])
])


dnl libpng...
AC_ARG_ENABLE(libpng, AS_HELP_STRING([--enable-libpng], [use libpng for PNG printing, default=auto]))

PKGCONFIG_LIBPNG=""
AC_SUBST(PKGCONFIG_LIBPNG)

AS_IF([test "x$PKGCONFIG" != x -a x$enable_libpng != xno], [
    AC_MSG_CHECKING([for libpng-1.6.x])
    AS_IF([$PKGCONFIG --exists libpng16], [
	AC_MSG_RESULT([yes]);
	AC_DEFINE([HAVE_LIBPNG], 1, [libpng])
	CPPFLAGS="$CPPFLAGS `$PKGCONFIG --cflags libpng16`"
	LIBS="$LIBS `$PKGCONFIG --libs libpng16` -lz"
	PKGCONFIG_LIBPNG="libpng >= 1.6,"
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
AC_ARG_ENABLE(libusb, AS_HELP_STRING([--enable-libusb], [use libusb for USB printing, default=auto]))

PKGCONFIG_LIBUSB=""
AC_SUBST(PKGCONFIG_LIBUSB)

AS_IF([test "x$PKGCONFIG" != x -a x$enable_libusb != xno], [
    AC_MSG_CHECKING([for libusb-1.0])
    AS_IF([$PKGCONFIG --exists libusb-1.0], [
	AC_MSG_RESULT([yes])
	AC_DEFINE([HAVE_LIBUSB], 1, [libusb])
	CPPFLAGS="$CPPFLAGS `$PKGCONFIG --cflags libusb-1.0`"
	LIBS="$LIBS `$PKGCONFIG --libs libusb-1.0`"
	AS_IF([test "x$host_os_name" = xdarwin], [
	    LIBS="$LIBS -framework IOKit"
	])
	PKGCONFIG_LIBUSB="libusb-1.0 >= 1.0,"
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
AC_ARG_ENABLE(pam, AS_HELP_STRING([--enable-libpam], [use libpam for authentication, default=auto]))

AS_IF([test x$enable_libpam != xno], [
    dnl PAM needs dlopen...
    AC_CHECK_LIB([dl], [dlopen])
    AC_CHECK_LIB([pam], [pam_start], [
	AC_DEFINE([HAVE_LIBPAM], 1, [libpam])
	LIBS="$LIBS -lpam"
    ])

    dnl PAM has two "standard" locations for its header...
    AC_CHECK_HEADER(security/pam_appl.h, AC_DEFINE([HAVE_SECURITY_PAM_APPL_H], 1, [Have <security/pam_appl.h> header?]))
    AC_CHECK_HEADER(pam/pam_appl.h, AC_DEFINE([HAVE_PAM_PAM_APPL_H], 1, [Have <pam/pam_appl.h> header?]))

    AS_IF([test x$ac_pam_start = xno -a x$enable_libpam = xyes], [
	AC_MSG_ERROR([libpam-dev required for --enable-libpam.])
    ])
])


dnl Library target...
AS_IF([test "$host_os_name" = darwin], [
    LIBPAPPL="libpappl.1.dylib"
], [
    LIBPAPPL="libpappl.so.1"
])
AC_SUBST(LIBPAPPL)


dnl Extra compiler options...
AC_ARG_ENABLE(debug, AS_HELP_STRING([--enable-debug], [turn on debugging, default=no]))
AC_ARG_ENABLE(maintainer, AS_HELP_STRING([--enable-maintainer], [turn on maintainer mode, default=no]))
AC_ARG_ENABLE(sanitizer, AS_HELP_STRING([--enable-sanitizer], [build with AddressSanitizer, default=no]))

AS_IF([test x$enable_debug = xyes], [
    OPTIM="-g"
    CSFLAGS=""
], [
    OPTIM="-g -Os"
    CSFLAGS="-o runtime"
])
WARNINGS=""
AC_SUBST(CSFLAGS)
AC_SUBST(OPTIM)
AC_SUBST(WARNINGS)

AS_IF([test -n "$GCC"], [
    AS_IF([test x$enable_sanitizer = xyes], [
	# Use -fsanitize=address with debugging...
	OPTIM="-g -fsanitize=address"
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
	WARNINGS="$WARNINGS -Werror"
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
    AS_CASE(["$host_os_name"],
    [linux*], [
	CPPFLAGS="$CPPFLAGS -D__USE_MISC -D_TIME_BITS=64"
    ])
])


dnl Extra linker options...
AC_ARG_WITH(dsoflags, AS_HELP_STRING([--with-dsoflags=...], [Specify additional DSOFLAGS]), [
    DSOFLAGS="$withval $DSOFLAGS"
])
AC_ARG_WITH(ldflags, AS_HELP_STRING([--with-ldflags=...], [Specify additional LDFLAGS]), [
    LDFLAGS="$withval $LDFLAGS"
])


dnl Generate the Makefile and pkg-config file...
AC_CONFIG_FILES([Makedefs pappl/pappl.pc])
AC_OUTPUT
