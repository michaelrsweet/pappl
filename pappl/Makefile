#
# Library makefile for the Printer Application Framework
#
# Copyright © 2020 by Michael R Sweet
#
# Licensed under Apache License v2.0.  See the file "LICENSE" for more
# information.
#

include ../Makedefs


OBJS	=	\
		client.o \
		client-accessors.o \
		client-auth.o \
		client-webif.o \
		contact.o \
		device.o \
		dnssd.o \
		ipp.o \
		job-accessors.o \
		job-filter.o \
		job-process.o \
		job.o \
		log.o \
		lookup.o \
		printer.o \
		printer-accessors.o \
		printer-driver.o \
		printer-raw.o \
		printer-support.o \
		printer-webif.o \
		resource.o \
		snmp.o \
		system.o \
		system-accessors.o \
		system-loadsave.o \
		system-webif.o \
		util.o

HEADERS	=	\
		base.h \
		client.h \
		device.h \
		job.h \
		log.h \
		pappl.h \
		printer.h \
		system.h

RESOURCES =	\
		icon-sm.png \
		icon-md.png \
		icon-lg.png \
		style.css

TARGETS	=	$(LIBPAPPL) libpappl.a


# Make everything
all:		$(TARGETS)


# Clean everything
clean:
	$(RM) $(OBJS)
	$(RM) $(TARGETS)
	$(RM) libpappl.dylib libpappl.so


# Clean all non-distribution files
distclean:	clean


# Update dependencies
depend:
	$(CC) -MM $(CFLAGS) $(OBJS:.o=.c) | sed -e '1,$$s/ \/usr\/include\/[^ ]*//g'  -e '1,$$s/ \/usr\/local\/include\/[^ ]*//g' >Dependencies


# Generate documentation uaing codedoc (https://www.msweet.org/codedoc)
doc:
	echo Generating pappl.html...
	codedoc pappl.xml $(HEADERS) $(OBJS:.o=.c) >../doc/pappl.html
	echo Generating pappl.epub...
	codedoc pappl.xml --epub ../doc/pappl.epub
	$(RM) pappl.xml
	echo Generating pappl-client.3...
	codedoc --man pappl-client --section 3 client.h client*.c >../man/pappl-client.3
	echo Generating pappl-device.3...
	codedoc --man pappl-device --section 3 device.h device.c >../man/pappl-device.3
	echo Generating pappl-job.3...
	codedoc --man pappl-job --section 3 job.h job*.c >../man/pappl-job.3
	echo Generating pappl-log.3...
	codedoc --man pappl-log --section 3 log.h log.c >../man/pappl-log.3
	echo Generating pappl-printer.3...
	codedoc --man pappl-printer --section 3 printer.h printer*.c >../man/pappl-printer.3
	echo Generating pappl-system.3...
	codedoc --man pappl-system --section 3 system.h system*.c >../man/pappl-system.3


# Install everything
install:	$(TARGETS)
	echo Installing libpappl in $(BUILDROOT)$(libdir)...
	$(INSTALL) -d -m 755 $(BUILDROOT)$(libdir)
	$(INSTALL) -c -m 644 libpappl.a $(BUILDROOT)$(libdir)
	$(INSTALL) -c -m 755 $(LIBPAPPL) $(BUILDROOT)$(libdir)
	if test "$(LIBPAPPL)" = libpappl.so.1; then \
		$(RM) $(BUILDROOT)$(libdir)/libpappl.so; \
		$(LN) -s libpappl.so.1 $(BUILDROOT)$(libdir)/libpappl.so; \
	elif test "$(LIBPAPPL)" = libpappl.1.dylib; then \
		$(RM) $(BUILDROOT)$(libdir)/libpappl.dylib; \
		$(LN) -s libpappl.1.dylib $(BUILDROOT)$(libdir)/libpappl.dylib; \
	fi
	echo Installing pkg-config files in $(BUILDROOT)$(libdir)/pkgconfig...
	$(INSTALL) -d -m 755 $(BUILDROOT)$(libdir)/pkgconfig
	$(INSTALL) -c -m 644 pappl.pc $(BUILDROOT)$(libdir)/pkgconfig
	echo Installing header files in $(BUILDROOT)$(includedir)/pappl...
	$(INSTALL) -d -m 755 $(BUILDROOT)$(includedir)/pappl
	for file in $(HEADERS); do \
		$(INSTALL) -c -m 644 $$file $(BUILDROOT)$(includedir)/pappl; \
	done


# Test everything
test:


# pappl static library
libpappl.a:		$(OBJS)
	echo Archiving $@...
	$(RM) $@
	$(AR) $(ARFLAGS) $@ $(OBJS)
	$(RANLIB) $@


# pappl shared library
libpappl.so.1:		$(OBJS)
	echo Linking $@...
	$(CC) $(DSOFLAGS) -shared -o $@ -Wl,-soname,$@ $(OBJS) $(LIBS)
	$(RM) `basename $@ .1`
	$(LN) $@ `basename $@ .1`


# pappl shared library (macOS)
libpappl.1.dylib:	$(OBJS)
	echo Linking $@...
	$(CC) $(DSOFLAGS) -dynamiclib -o $@ \
		-install_name $(libdir)/$@ \
		-current_version $(PAPPL_VERSION) -compatibility_version 1.0.0 \
		$(OBJS) $(LIBS)
	$(CODE_SIGN) -s "$(CODESIGN_IDENTITY)" -o runtime --timestamp -i org.msweet.pappl.libpappl $@
	$(RM) `basename $@ .1.dylib`.dylib
	$(LN) $@ `basename $@ .1.dylib`.dylib


# Static resource header...
resheader:	$(RESOURCES)
	echo Generating $@...
	./makeresource.sh $(RESOURCES) >resource-private.h


# Dependencies
include Dependencies
