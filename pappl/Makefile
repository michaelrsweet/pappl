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
		printer-support.o \
		resource.o \
		snmp.o \
		system.o \
		system-accessors.o \
		webif.o

HEADERS	=	\
		base.h \
		client.h \
		device.h \
		job.h \
		log.h \
		pappl.h \
		printer.h \
		system.h

TARGETS	=	$(LIBPAPPL)


# Make everything
all:		$(TARGETS)


# Clean everything
clean:
	$(RM) -r $(OBJS) $(TARGETS)


# Clean all non-distribution files
distclean:	clean


# Update dependencies
depend:
	$(CC) -MM $(CFLAGS) $(OBJS:.o=.c) >Dependencies


# Install everything
install:	$(TARGETS)
	$(INSTALL) -d -m 755 $(BUILDROOT)$(libdir)
	$(INSTALL) -c -m 755 $(LIBPAPPL) $(BUILDROOT)$(libdir)
	$(INSTALL) -d -m 755 $(BUILDROOT)$(includedir)/pappl
	for file in $(HEADERS); do \
		$(INSTALL) -c -m 644 $$file $(BUILDROOT)$(includedir)/pappl; \
	done


# Test everything
test:


# libpappl.so.1 shared library
libpappl.so.1:	$(OBJS)
	echo Linking $@...
	$(CC) $(DSOFLAGS) -o $@ $(OBJS) $(LIBS)
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


# Dependencies
include Dependencies
