#
# RPM spec file for the Printer Application Framework
#
# Copyright © 2020-2022 by Michael R Sweet
#
# Licensed under Apache License v2.0.  See the file "LICENSE" for more
# information.
#

Summary: Printer Application Framework (PAPPL)
Name: pappl
Version: 1.3.0
Release: 1
License: Apache 2.0
Group: Development/Libraries
Source: https://github.com/michaelrsweet/pappl/releases/download/release-%{version}/pappl-%{version}.tar.gz
Url: https://www.msweet.org/pappl
Packager: John Doe <johndoe@example.com>
Vendor: Michael R Sweet

BuildRequires: avahi-devel, cups-devel, gnutls-devel, libjpeg-turbo-devel, libpng-devel, libusbx-devel, pam-devel, zlib-devel
Requires: cups-devel

# Use buildroot so as not to disturb the version already installed
BuildRoot: /var/tmp/%{name}-root

%description
PAPPL is a simple C-based framework/library for developing CUPS Printer
Applications, which are the recommended replacement for printer drivers.

PAPPL supports JPEG, PNG, PWG Raster, Apple Raster, and "raw" printing to
printers connected via USB and network (AppSocket/JetDirect) connections.
PAPPL provides access to the printer via its embedded IPP Everywhere™ service,
either local to the computer or on your whole network, which can then be
discovered and used by any application.

PAPPL is licensed under the Apache License Version 2.0 with an exception
to allow linking against GPL2/LGPL2 software (like older versions of CUPS),
so it can be used freely in any project you'd like.

%package devel
Summary: PAPPL - development environment
Requires: %{name} = %{version}

%description devel
This package provides the PAPPL headers and development environment.

%prep
%setup

%build
CFLAGS="$RPM_OPT_FLAGS" CXXFLAGS="$RPM_OPT_FLAGS" LDFLAGS="$RPM_OPT_FLAGS" ./configure --enable-shared --prefix=/usr

# If we got this far, all prerequisite libraries must be here.
make

%install
# Make sure the RPM_BUILD_ROOT directory exists.
rm -rf $RPM_BUILD_ROOT

make BUILDROOT=$RPM_BUILD_ROOT install

%clean
rm -rf $RPM_BUILD_ROOT

%post
/sbin/ldconfig

%postun
/sbin/ldconfig

%files
%defattr(-,root,root)
%dir /usr/lib
/usr/lib/libpappl.so*

%files devel
%defattr(-,root,root)
%dir /usr/bin
/usr/bin/*
%dir /usr/include/pappl
/usr/include/pappl/*.h
%dir /usr/lib
/usr/lib/libpappl.a
%dir /usr/lib/pkgconfig
/usr/lib/pkgconfig/pappl.pc
%dir /usr/share/doc/pappl
/usr/share/doc/pappl/*
%dir /usr/share/man/man1
/usr/share/man/man1/*
%dir /usr/share/man/man3
/usr/share/man/man3/*
%dir /usr/share/pappl
/usr/share/pappl/*
