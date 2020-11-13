dnl
dnl Configuration script for the Printer Application Framework
dnl
dnl Copyright © 2019-2020 by Michael R Sweet
dnl
dnl Licensed under Apache License v2.0.  See the file "LICENSE" for more
dnl information.
dnl


dnl We need at least autoconf 2.60...
AC_PREREQ([2.69])


dnl Package name and version...
AC_INIT([pappl],[1.0b1],[https://github.com/michaelrsweet/pappl/issues],[pappl],[https://www.msweet.org/pappl])
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


dnl Get the build and host platforms and split the host_os value
AC_CANONICAL_BUILD
AC_CANONICAL_HOST

[host_os_name=`echo $host_os | sed -e '1,$s/[0-9.]*$//g'`]
[host_os_version=`echo $host_os | sed -e '1,$s/^[^0-9.]*//g' | awk -F. '{print $1 $2}'`]
# Linux often does not yield an OS version we can use...
if test "x$host_os_version" = x; then
        host_os_version="0"
fi


dnl Compiler options...
CFLAGS="${CFLAGS:=}"
CXXFLAGS="${CXXFLAGS:=}"
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
if test "$ac_cv_prog_ranlib" = ":"; then
    ARFLAGS="crs"
else
    ARFLAGS="cr"
fi
AC_SUBST(ARFLAGS)


dnl install-sh
AC_MSG_CHECKING([for install-sh script])
INSTALL="`pwd`/install-sh"
AC_SUBST(INSTALL)
AC_MSG_RESULT([using $INSTALL])


dnl CUPS
AC_PATH_TOOL(CUPSCONFIG, cups-config)

if test "x$CUPSCONFIG" = x; then
    AC_MSG_ERROR([Sorry, this software requires libcups-dev.])
fi

case "`$CUPSCONFIG --api-version`" in
    1.* | 2.0 | 2.1)
        AC_MSG_ERROR([Sorry, you need CUPS 2.2.0 or higher.])
        ;;
esac

dnl Check for pkg-config, which is used for some other tests later on...
AC_PATH_TOOL(PKGCONFIG, pkg-config)


dnl CUPS library...
AC_ARG_ENABLE([static_cups],
    [AS_HELP_STRING([--enable-static-cups],[use static CUPS libraries (default: no)])])

if test x$enable_static_cups = xyes; then
    have_static_cups=yes
    CFLAGS="$CFLAGS `cups-config --static --cflags`"
    DSOFLAGS="$DSOFLAGS `cups-config --static --ldflags`"
    LDFLAGS="$LDFLAGS `cups-config --static --ldflags`"
    LIBS="$LIBS `cups-config --image --static --libs`"
else
    have_static_cups=no
    CFLAGS="$CFLAGS `cups-config --cflags`"
    DSOFLAGS="$DSOFLAGS `cups-config --ldflags`"
    LDFLAGS="$LDFLAGS `cups-config --ldflags`"
    LIBS="$LIBS `cups-config --image --libs`"
fi


dnl String functions...
AC_CHECK_FUNCS(strlcpy)


dnl POSIX threads...
AC_CHECK_HEADER(pthread.h)

if test x$ac_cv_header_pthread_h != xyes; then
    AC_MSG_ERROR([Sorry, this software requires POSIX threading support.])
fi

for flag in -lpthreads -lpthread -pthread; do
    AC_MSG_CHECKING([for pthread_create using $flag])
    SAVELIBS="$LIBS"
    LIBS="$flag $LIBS"
    AC_LANG([C])
    AC_LINK_IFELSE([
        AC_LANG_PROGRAM([#include <pthread.h>],[pthread_create(0, 0, 0, 0);])],
        [have_pthread=yes],
        [LIBS="$SAVELIBS"])

    if test x$have_pthread = xyes; then
        AC_MSG_RESULT([yes])
        CFLAGS="$CFLAGS -D_THREAD_SAFE -D_REENTRANT"
        break
    else
        AC_MSG_RESULT([no])
    fi
done


dnl DNS-SD support...
AC_ARG_WITH([dnssd],
    [AS_HELP_STRING([--with-dnssd=LIBRARY],
        [set DNS-SD library (auto, avahi, mdnsresponder)])])

if test "x$with_dnssd" != xmdnsresponder -a "x$with_dnssd" != xno -a x$host_os_name != xdarwin; then
    PKG_CHECK_MODULES([avahi_client],[avahi-client],[have_avahi=yes],[have_avahi=no])
    if test "x$have_avahi" = "xyes"; then
        CFLAGS="$CFLAGS `$PKGCONFIG --cflags avahi-client`"
        LIBS="$LIBS `$PKGCONFIG --libs avahi-client`"
        AC_DEFINE([HAVE_AVAHI], 1, [DNS-SD (mDNSResponder or Avahi)])
    elif test "x$with_dnssd" = "xavahi"; then
        AC_MSG_ERROR([libavahi-client-dev needed for --with-dnssd=avahi.])
    fi
elif test x$with_dnssd = xavahi; then
    AC_MSG_ERROR([pkgconfig and libavahi-client-dev needed for --with-dnssd=avahi.])
elif test x$with_dnssd != xavahi -a "x$with_dnssd" != xno; then
    AC_CHECK_HEADER(dns_sd.h, [
        case "$host_os_name" in
            darwin*)
                # Darwin and macOS...
                AC_DEFINE([HAVE_DNSSD], 1, [DNS-SD (mDNSResponder or Avahi)])
                LIBS="$LIBS -framework CoreFoundation -framework SystemConfiguration"
                ;;
            *)
                # All others...
                AC_MSG_CHECKING(for current version of dns_sd library)
                SAVELIBS="$LIBS"
                LIBS="$LIBS -ldns_sd"
                AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include <dns_sd.h>]], [[
                    int constant = kDNSServiceFlagsShareConnection;
                    unsigned char txtRecord[100];
                    uint8_t valueLen;
                    TXTRecordGetValuePtr(sizeof(txtRecord),
                        txtRecord, "value", &valueLen);]])],[
                    AC_MSG_RESULT([yes])
                    AC_DEFINE([HAVE_DNSSD], 1, [DNS-SD (mDNSResponder or Avahi)])],[
                    AC_MSG_RESULT([no])
                    LIBS="$SAVELIBS"
                    if test x$with_dnssd = xmdnsresponder; then
                        AC_MSG_ERROR([mDNSResponder required for --with-dnssd=mdnsresponder.])
                    fi])
                ;;
        esac
    ])
fi

dnl GNU TLS...
have_gnutls=no
AC_ARG_ENABLE([gnutls],
    [AS_HELP_STRING([--disable-gnutls],[enable gnutls (default: yes)])])

if test "x$enable_gnutls" != "xno"; then
    PKG_CHECK_MODULES([gnutls],[gnutls >= 3.0],[have_gnutls=yes],[have_gnutls=no])
    if test "x$have_gnutls" = "xyes"; then
       AC_DEFINE([HAVE_GNUTLS], [], [Define GNUTLS])
       CFLAGS="$CFLAGS `$PKGCONFIG --cflags gnutls`"
       LIBS="$LIBS `$PKGCONFIG --libs gnutls`"
       PKGCONFIG_GNUTLS="gnutls >= 3.0,"
    elif test "x$enable_gnutls" = "xyes"; then
        AC_MSG_ERROR([no gnutls support])
    else
        PKGCONFIG_GNUTLS=""
        AC_MSG_NOTICE([no gnutls support])
    fi
fi
AC_SUBST(PKGCONFIG_GNUTLS)


dnl Random number support...
AC_CHECK_HEADER(sys/random.h, AC_DEFINE([HAVE_SYS_RANDOM_H], 1, [Have <sys/random.h> header?]))
AC_CHECK_FUNCS(arc4random getrandom gnutls_rnd)


dnl libjpeg...
have_libjpeg=no
AC_ARG_ENABLE([libjpeg],
    [AS_HELP_STRING([--disable-libjpeg],[use libjpeg for JPEG printing (default: yes)])])

if test x$enable_libjpeg != xno; then
    PKG_CHECK_MODULES([libjpeg],[libjpeg-1],[have_libjpeg=yes],[have_libjpeg=no])
    if test "x$have_libjpeg" = "xyes"; then
        AC_DEFINE([HAVE_LIBJPEG], 1, [libjpeg])
        CFLAGS="$CFLAGS `$PKGCONFIG --cflags libjpeg`"
        LIBS="$LIBS `$PKGCONFIG --libs libjpeg`"
        PKGCONFIG_LIBJPEG="libjpeg,"
    elif test "x$enable_libjpeg" = "xyes"; then
        AC_SEARCH_LIBS([jpeg_start_decompress],[turbojpeg jpeg],
            [AC_DEFINE([HAVE_LIBJPEG], 1, [libjpeg])],
            [AC_MSG_ERROR([libjpeg-dev 8 or later required for --enable-libjpeg])])
    else
        PKGCONFIG_LIBJPEG=""
        AC_MSG_NOTICE([no libjpeg support found])
    fi
fi
AC_SUBST(PKGCONFIG_LIBJPEG)

dnl libpng...
have_libpng=no
AC_ARG_ENABLE([libpng],
    [AS_HELP_STRING([--disable-libpng],[use libpng for PNG printing (default: yes)])])

if test x$enable_libpng != xno; then
    PKG_CHECK_MODULES([libpng16],[libpng16],[have_libpng=yes],[have_libpng=no])
    if test "x$have_libpng" = "xyes"; then
        AC_DEFINE([HAVE_LIBPNG], 1, [libpng])
        CFLAGS="$CFLAGS `$PKGCONFIG --cflags libpng16`"
        LIBS="$LIBS `$PKGCONFIG --libs libpng16` -lz"
        PKGCONFIG_LIBPNG="libpng >= 1.6,"
    elif test "x$enable_libpng" = "xyes"; then
        AC_MSG_ERROR([libpng-dev 1.6 or later required for --enable-libpng.])
    else
        PKGCONFIG_LIBPNG=""
        AC_MSG_NOTICE([no libpng support found])
    fi
fi

AC_SUBST(PKGCONFIG_LIBPNG)


dnl libusb...
have_libusb=no
AC_ARG_ENABLE([libusb],
    [AS_HELP_STRING([--disable-libusb],[use libusb for USB printing (default: yes)])])

if test x$enable_libusb != xno; then
    PKG_CHECK_MODULES([libusb],[libusb-1.0],[have_libusb=yes],[have_libusb=no])
    if test "x$have_libusb" = "xyes"; then
        AC_DEFINE([HAVE_LIBUSB], 1, [libusb])
        CFLAGS="$CFLAGS `$PKGCONFIG --cflags libusb-1.0`"
        LIBS="$LIBS `$PKGCONFIG --libs libusb-1.0`"
        if test "x$host_os_name" = xdarwin; then
            LIBS="$LIBS -framework IOKit"
        fi
        PKGCONFIG_LIBUSB="libusb-1.0 >= 1.0,"
    elif test "x$enable_libusb" = "xyes"; then
            AC_MSG_ERROR([libusb-1.0-0-dev required for --enable-libusb.])
    else
        PKGCONFIG_LIBUSB=""
        AC_MSG_NOTICE([no libusb support found])
    fi
fi
AC_SUBST(PKGCONFIG_LIBUSB)


dnl PAM support...
have_pam=no
AC_ARG_ENABLE([pam],
    [AS_HELP_STRING([--disable-pam],[disable PAM support (default: yes)])])

if test x$enable_pam != xno; then
    dnl PAM needs dlopen...
    AC_CHECK_LIB([dl], [dlopen])
    AC_CHECK_LIB([pam], [pam_start], [
        AC_DEFINE([HAVE_LIBPAM], 1, [libpam])
        LIBS="$LIBS -lpam"])

    dnl PAM has two "standard" locations for its header...
    AC_CHECK_HEADER(security/pam_appl.h, AC_DEFINE([HAVE_SECURITY_PAM_APPL_H], 1, [Have <security/pam_appl.h> header?]))
    AC_CHECK_HEADER(pam/pam_appl.h, AC_DEFINE([HAVE_PAM_PAM_APPL_H], 1, [Have <pam/pam_appl.h> header?]))

    if test x$ac_pam_start = xno -a x$enable_pam = xyes; then
        AC_MSG_ERROR([libpam-dev required for --enable-pam.])
    else
        have_pam=yes
    fi
fi


dnl Library target...
if test "$host_os_name" = darwin; then
    LIBPAPPL="libpappl.1.dylib"
else
    LIBPAPPL="libpappl.so.1"
fi
AC_SUBST(LIBPAPPL)


dnl Extra compiler options...
AC_ARG_ENABLE([debug],
    [AS_HELP_STRING([--enable-debug],[turn on debugging (default: no)])])

if test x$enable_debug = xyes; then
    OPTIM="-g"
    CSFLAGS=""
else
    OPTIM="-g -Os"
    CSFLAGS="-o runtime"
fi

AC_SUBST(CSFLAGS)
AC_SUBST(OPTIM)


AC_ARG_ENABLE([sanitizer],
    [AS_HELP_STRING([--enable-sanitizer],[build with AddressSanitizer (default: no)])])

if test -n "$GCC"; then
    if test x$enable_sanitizer = xyes; then
        # Use -fsanitize=address with debugging...
        OPTIM="-g -fsanitize=address"
    else
        # Otherwise use the Fortify enhancements to catch any unbounded
        # string operations...
        CFLAGS="$CFLAGS -D_FORTIFY_SOURCE=2"
        CXXFLAGS="$CXXFLAGS -D_FORTIFY_SOURCE=2"
    fi

    dnl Show all standard warnings + unused variables when compiling...
    CFLAGS="-Wall -Wunused -Wno-unknown-warning-option -Wno-unused-result -Wno-char-subscripts -Wno-format-y2k -Wno-maybe-uninitialized -Wno-switch $CFLAGS"

    dnl See if PIE options are supported...
    AC_MSG_CHECKING(whether compiler supports -fPIE)
    OLDCFLAGS="$CFLAGS"
    case "$host_os_name" in
        darwin*)
            CFLAGS="$CFLAGS -fPIC -fPIE -Wl,-pie"
            AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[]], [[]])],[
                OLDCFLAGS="-fPIC $OLDCFLAGS"
                LDFLAGS="-fPIE -Wl,-pie $LDFLAGS"
                AC_MSG_RESULT(yes)],[AC_MSG_RESULT(no)])
            ;;

        *)
            CFLAGS="$CFLAGS -fPIC -fPIE -pie"
            AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[]], [[]])],[
                OLDCFLAGS="-fPIC $OLDCFLAGS"
                LDFLAGS="-fPIE -pie $LDFLAGS"
                AC_MSG_RESULT(yes)],[AC_MSG_RESULT(no)])
            ;;
    esac
    CFLAGS="$OLDCFLAGS"
fi


dnl Extra linker options...
AC_ARG_WITH([dsoflags],
    [AS_HELP_STRING([--with-dsoflags=...], [Specify additional DSOFLAGS ])],
    [DSOFLAGS="$withval $DSOFLAGS"])
AC_ARG_WITH([ldflags],
    [AS_HELP_STRING([--with-ldflags=...], [Specify additional LDFLAGS ])],
    [LDFLAGS="$withval $LDFLAGS"])

dnl Generate the Makefile...
AC_CONFIG_FILES([Makedefs pappl/pappl.pc])
AC_OUTPUT

echo "

------------------------------------------------------------------
-- pappl compilation options

pappl version:        ${PACKAGE_VERSION}
static_cups:          $have_static_cups
dnssd:                $with_dnssd
GNUTLS:               $have_gnutls
libjpeg:              $have_libjpeg
libpng:               $have_libpng
libusb:               $have_libusb
pam:                  $have_pam
DSOFLAGS:             $DSOFLAGS
LDFLAGS:              $LDFLAGS
CFLAGS:               $CFLAGS
LIBS:                 $LIBS
------------------------------------------------------------------
"
