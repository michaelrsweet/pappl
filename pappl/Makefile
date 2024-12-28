#
# Library makefile for the Printer Application Framework
#
# Copyright © 2020-2024 by Michael R Sweet
#
# Licensed under Apache License v2.0.  See the file "LICENSE" for more
# information.
#

include ../Makedefs

CFLAGS += $(shell pkg-config --cflags libxml-2.0)
LDFLAGS += $(shell pkg-config --libs libxml-2.0)

BASEOBJS =	\
		client.o \
		client-accessors.o \
		client-auth.o \
		client-loc.o \
		client-ipp.o \
		client-webif.o \
		contact.o \
		device.o \
		device-file.o \
		device-network.o \
		device-usb.o \
		dnssd.o \
		esclops.o \
		httpmon.o \
		job-accessors.o \
		job-filter.o \
		job-ipp.o \
		job-process.o \
		job-scan.o \
		job.o \
		link.o \
		loc.o \
		log.o \
		lookup.o \
		mainloop.o \
		mainloop-subcommands.o \
		mainloop-support.o \
		printer.o \
		printer-accessors.o \
		printer-driver.o \
		printer-ipp.o \
		printer-raw.o \
		printer-support.o \
		printer-usb.o \
		printer-webif.o \
		resource.o \
		scanner.o \
		scanner-accessors.o\
		scanner-driver.o \
		scanner-escl.o \
		scanner-webif.o \
		snmp.o \
		subscription.o \
		subscription-ipp.o \
		system.o \
		system-accessors.o \
		system-ipp.o \
		system-loadsave.o \
		system-loc.o \
		system-printer.o \
		system-subscription.o \
		system-webif.o \
		util.o

OBJS	=	\
		$(BASEOBJS) \
		$(SYSTEM_STATUS)

HEADERS	=	\
		base.h \
		client.h \
		device.h \
		job.h \
		loc.h \
		log.h \
		mainloop.h \
		pappl.h \
		printer.h \
		scanner.h \
		scanner-private.h \
		subscription.h \
		system.h \
		system-private.h

RESOURCES =	\
		icon-sm.png \
		icon-md.png \
		icon-lg.png \
		style.css

STRINGS =	\
		strings/de.strings \
		strings/en.strings \
		strings/es.strings \
		strings/fr.strings \
		strings/it.strings \
		strings/ja.strings \
		strings/nb-NO.strings \
		strings/pl.strings \
		strings/tr.strings

DOCFLAGS =	\
		--author "Michael R Sweet" \
		--copyright "Copyright (c) 2020-2024 by Michael R Sweet" \
		--docversion $(PAPPL_VERSION)

.SUFFIXES:	md
DOCFILES =	\
		device.md \
		hello-world.md \
		introduction.md \
		job.md \
		mainloop.md \
		printer.md \
		resource.md \
		system.md

TARGETS	=	$(LIBPAPPL) libpappl.a


# Make everything
all:		$(TARGETS)


# Clean everything
clean:
	$(RM) $(OBJS)
	$(RM) $(TARGETS)
	$(RM) libpappl.dylib libpappl.so parse-lock-log


# Clean all non-distribution files
distclean:	clean


# Update dependencies
depend:
	$(CC) -MM $(CFLAGS) `echo $(OBJS:.o=.c) | sed -e '1,$$s/-macos\.c/-macos.m/g'` | sed -e '1,$$s/ \/usr\/include\/[^ ]*//g' -e '1,$$s/ \/usr\/local\/include\/[^ ]*//g' >Dependencies


# Generate documentation using codedoc (https://www.msweet.org/codedoc)
doc:
	echo Generating pappl.html...
	codedoc $(DOCFLAGS) --title "PAPPL v$(PAPPL_VERSION_MAJOR).$(PAPPL_VERSION_MINOR) Programmers Manual" --coverimage ../doc/pappl-512.png --body ../doc/pappl-body.md pappl.xml $(HEADERS) $(BASEOBJS:.o=.c) >../doc/pappl.html
	echo Generating pappl.epub...
	codedoc $(DOCFLAGS) --title "PAPPL v$(PAPPL_VERSION_MAJOR).$(PAPPL_VERSION_MINOR) Programmers Manual" --coverimage epub.png --body ../doc/pappl-body.md pappl.xml --epub ../doc/pappl.epub
	$(RM) pappl.xml
	echo Generating pappl-client.3...
	codedoc $(DOCFLAGS) --title "pappl client functions" --man pappl-client --section 3 --body ../man/pappl-client-body.man --footer ../man/pappl-footer.man client.h client*.c >../man/pappl-client.3
	echo Generating pappl-device.3...
	codedoc $(DOCFLAGS) --title "pappl device functions" --man pappl-device --section 3 --body ../man/pappl-device-body.man --footer ../man/pappl-footer.man device.h device.c >../man/pappl-device.3
	echo Generating pappl-job.3...
	codedoc $(DOCFLAGS) --title "pappl job functions" --man pappl-job --section 3 --body ../man/pappl-job-body.man --footer ../man/pappl-footer.man job.h job*.c >../man/pappl-job.3
	echo Generating pappl-log.3...
	codedoc $(DOCFLAGS) --title "pappl logging functions" --man pappl-log --section 3 --body ../man/pappl-log-body.man --footer ../man/pappl-footer.man log.h log.c >../man/pappl-log.3
	echo Generating pappl-mainloop.3...
	codedoc $(DOCFLAGS) --title "pappl main loop functions" --man pappl-mainloop --section 3 --body ../man/pappl-mainloop-body.man --footer ../man/pappl-footer.man mainloop.h mainloop*.c >../man/pappl-mainloop.3
	echo Generating pappl-printer.3...
	codedoc $(DOCFLAGS) --title "pappl printer functions" --man pappl-printer --section 3 --body ../man/pappl-printer-body.man --footer ../man/pappl-footer.man printer.h printer*.c >../man/pappl-printer.3
	echo Generating pappl-resource.3...
	codedoc $(DOCFLAGS) --title "pappl resource functions" --man pappl-resource --section 3 --body ../man/pappl-resource-body.man --footer ../man/pappl-footer.man link.c resource.c >../man/pappl-resource.3
	echo Generating pappl-system.3...
	codedoc $(DOCFLAGS) --title "pappl system functions" --man pappl-system --section 3 --body ../man/pappl-system-body.man --footer ../man/pappl-footer.man system.h system*.c >../man/pappl-system.3


# Install everything
install:	$(TARGETS)
	echo Installing pappl-makeresheader in $(BUILDROOT)$(bindir)...
	$(INSTALL) -d -m 755 $(BUILDROOT)$(bindir)
	$(INSTALL) -c -m 755 makeresheader.sh $(BUILDROOT)$(bindir)/pappl-makeresheader
	echo Installing libpappl in $(BUILDROOT)$(libdir)...
	$(INSTALL) -d -m 755 $(BUILDROOT)$(libdir)
	$(INSTALL) -c -m 755 $(LIBPAPPL) $(BUILDROOT)$(libdir)
	if test "$(LIBPAPPL)" = libpappl.so.1; then \
	    $(RM) $(BUILDROOT)$(libdir)/libpappl.so; \
	    $(LN) -s libpappl.so.1 $(BUILDROOT)$(libdir)/libpappl.so; \
	elif test "$(LIBPAPPL)" = libpappl.1.dylib; then \
	    $(RM) $(BUILDROOT)$(libdir)/libpappl.dylib; \
	    $(LN) -s libpappl.1.dylib $(BUILDROOT)$(libdir)/libpappl.dylib; \
	fi
	if test "$(LIBPAPPL_STATIC)" = libpappl.a; then \
	    $(INSTALL) -c -m 644 libpappl.a $(BUILDROOT)$(libdir); \
	fi
	echo Installing pkg-config files in $(BUILDROOT)$(libdir)/pkgconfig...
	$(INSTALL) -d -m 755 $(BUILDROOT)$(libdir)/pkgconfig
	$(INSTALL) -c -m 644 pappl.pc $(BUILDROOT)$(libdir)/pkgconfig
	echo Installing data files in $(BUILDROOT)$(datadir)/pappl...
	$(INSTALL) -d -m 755 $(BUILDROOT)$(datadir)/pappl
	for file in $(RESOURCES); do \
	    $(INSTALL) -c -m 644 $$file $(BUILDROOT)$(datadir)/pappl; \
	done
	echo Installing header files in $(BUILDROOT)$(includedir)/pappl...
	$(INSTALL) -d -m 755 $(BUILDROOT)$(includedir)/pappl
	for file in $(HEADERS); do \
	    $(INSTALL) -c -m 644 $$file $(BUILDROOT)$(includedir)/pappl; \
	done


# Test everything
test:


# pappl exports file
libpappl1.def:		$(OBJS) Makefile
	echo Generating $@...
	echo "LIBRARY libpappl1" >libpappl1.def
	echo "VERSION 1.2" >>libpappl1.def
	echo "EXPORTS" >>libpappl1.def
	(nm $(OBJS) 2>/dev/null | grep "T _" | awk '{print $$3}') | \
		grep -v '^__' | sed -e '1,$$s/^_//' | sort >>libpappl1.def


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
	    -current_version $(PAPPL_VERSION_MAJOR).$(PAPPL_VERSION_MINOR) -compatibility_version 1.0 \
	    $(OBJS) $(LIBS)
	$(CODE_SIGN) $(CSFLAGS) -i org.msweet.pappl.`basename $@ .1.dylib` $@
	$(RM) `basename $@ .1.dylib`.dylib
	$(LN) $@ `basename $@ .1.dylib`.dylib


# parse-lock-log
parse-lock-log:	parse-lock-log.c
	echo Compiling and linking $@...
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<


# Static resource header...
resheader:	$(RESOURCES)
	echo Generating $@...
	./makeresheader.sh $(RESOURCES) >resource-private.h


# Update localization strings using StringUtils (<https://www.msweet.org/stringsutil>)
.PHONY: strings
strings:
	echo Updating base.strings...
	cp ipp.strings base.strings
	stringsutil -f base.strings -n _PAPPL_LOC scan *.[cm]
	for file in $(STRINGS); do \
		stringsutil -f $$file -c merge base.strings; \
	done
stringsh:
	echo Generating localization headers...
	for file in $(STRINGS); do \
		stringsutil -f $$file export strings/`basename $$file .strings`_strings.h; \
	done


# Analyze code with the Clang static analyzer <https://clang-analyzer.llvm.org>
clang:
	echo "clang $(CPPFLAGS) --analyze ..."
	clang $(CPPFLAGS) --analyze `echo $(OBJS:.o=.c) | sed -e '1,$$s/-macos\.c/-macos.m/g'` 2>clang.log
	rm -rf $(OBJS:.o=.plist)
	test -s clang.log && (echo "$(GHA_ERROR)Clang detected issues."; echo ""; cat clang.log; exit 1) || exit 0


# Analyze code using Cppcheck <http://cppcheck.sourceforge.net>
cppcheck:
	echo Analyzing code with Cppcheck...
	echo 'cppcheck $(CPPFLAGS) --template=gcc --suppressions-list=cppcheck.suppressions ...'
	cppcheck $(CPPFLAGS) --template=gcc --suppressions-list=cppcheck.suppressions `echo $(OBJS:.o=.c) | sed -e '1,$$s/-macos\.c/-macos.m/g'` 2>cppcheck.log
	test -s cppcheck.log && (echo "$(GHA_ERROR)Cppcheck detected issues."; echo ""; cat cppcheck.log; exit 1) || exit 0


# Dependencies
include Dependencies
