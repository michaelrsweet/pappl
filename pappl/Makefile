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
		job-process.o \
		job.o \
		log.o \
		lookup.o \
		printer.o \
		printer-accessors.o \
		printer-driver.o \
		printer-support.o \
		printer-webif.o \
		resource.o \
		snmp.o \
		system.o \
		system-accessors.o \
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
		apple-touch-icon.png \
		icon-sm.png \
		icon-md.png \
		icon-lg.png \
		style.css

TARGETS	=	$(LIBPAPPL) libpappl_static.a


# Make everything
all:		$(TARGETS)


# Clean everything
clean:
	$(RM) -r $(OBJS) $(TARGETS)


# Clean all non-distribution files
distclean:	clean


# Update dependencies
depend:
	$(CC) -MM $(CFLAGS) $(OBJS:.o=.c) | sed -e '1,$$s/ \/usr\/include\/[^ ]*//g'  -e '1,$$s/ \/usr\/local\/include\/[^ ]*//g' >Dependencies


# Install everything
install:	$(TARGETS)
	if test $(LIBPAPPL) = pappl.framework; then \
		$(RM) -r $(BUILDROOT)/Library/Frameworks/pappl.framework; \
		ditto pappl.framework $(BUILDROOT)/Library/Frameworks; \
	else \
		$(INSTALL) -d -m 755 $(BUILDROOT)$(libdir); \
		$(INSTALL) -c -m 644 libpappl_static.a $(BUILDROOT)$(libdir); \
		$(INSTALL) -c -m 755 $(LIBPAPPL) $(BUILDROOT)$(libdir); \
		$(RM) $(BUILDROOT)$(libdir)/libpappl.so; \
		$(LN) -s libpappl.so.1 $(BUILDROOT)$(libdir)/libpappl.so; \
		$(INSTALL) -d -m 755 $(BUILDROOT)$(libdir)/pkgconfig; \
		$(INSTALL) -c -m 644 pappl.pc $(BUILDROOT)$(libdir)/pkgconfig; \
		$(INSTALL) -d -m 755 $(BUILDROOT)$(includedir)/pappl; \
		for file in $(HEADERS); do \
			$(INSTALL) -c -m 644 $$file $(BUILDROOT)$(includedir)/pappl; \
		done; \
	fi


# Test everything
test:


# pappl static library
libpappl_static.a:	$(OBJS)
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


# pappl framework
pappl.framework:	$(OBJS)
	echo Linking $@...
	$(RM) -r $@
	$(MKDIR) pappl.framework/Headers
	cp $(HEADERS) pappl.framework/Headers
	$(MKDIR) pappl.framework/Resources
	cp Info.plist pappl.framework/Resources
	$(CC) $(DSOFLAGS) -dynamiclib -o pappl.framework/pappl \
		-install_name @rpath/pappl.framework/pappl \
		-current_version 1.0 -compatibility_version 1.0.0 \
		$(OBJS) $(LIBS)
	$(CODE_SIGN) -s "$(CODESIGN_IDENTITY)" -o runtime --timestamp pappl.framework


# Static resource header...
resheader:	$(RESOURCES)
	echo Generating $@...
	./makeresource.sh $(RESOURCES) >resource-private.h


# Dependencies
include Dependencies
