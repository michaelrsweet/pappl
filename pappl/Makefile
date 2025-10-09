#
# Library makefile for the Printer Application Framework
#
# Copyright © 2020-2025 by Michael R Sweet
#
# Licensed under Apache License v2.0.  See the file "LICENSE" for more
# information.
#

include ../Makedefs


LIBOBJS	=	\
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
		httpmon.o \
		job-accessors.o \
		job-filter.o \
		job-ipp.o \
		job-process.o \
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
		printer-infra.o \
		printer-ipp.o \
		printer-proxy.o \
		printer-raw.o \
		printer-support.o \
		printer-usb.o \
		printer-webif.o \
		resource.o \
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
OBJS =		\
		$(LIBOBJS) \
		pappl-exec.o

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
		subscription.h \
		system.h

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
		strings/he.strings \
		strings/it.strings \
		strings/ja.strings \
		strings/nb-NO.strings \
		strings/pl.strings \
		strings/ta.strings \
		strings/tr.strings \
		strings/zh-Hans.strings

DOCFLAGS =	\
		--author "Michael R Sweet" \
		--copyright "Copyright (c) 2020-2025 by Michael R Sweet" \
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

TARGETS	=	$(LIBPAPPL) $(LINKPAPPL_STATIC) pappl-exec


# Make everything
all:		$(TARGETS)


# Clean everything
clean:
	$(RM) $(OBJS)
	$(RM) $(TARGETS)
	$(RM) libpappl.dylib libpappl2.dylib libpappl.so libpappl2.so parse-lock-log


# Clean all non-distribution files
distclean:	clean


# Update dependencies
depend:
	$(CC) -MM $(CFLAGS) `echo $(OBJS:.o=.c) | sed -e '1,$$s/-macos\.c/-macos.m/g'` | sed -e '1,$$s/ \/usr\/include\/[^ ]*//g' -e '1,$$s/ \/usr\/local\/include\/[^ ]*//g' >Dependencies


# Generate documentation using codedoc (https://www.msweet.org/codedoc)
doc:
	echo Generating pappl.html...
	codedoc $(DOCFLAGS) --title "PAPPL v$(PAPPL_VERSION_MAJOR).$(PAPPL_VERSION_MINOR) Programmers Manual" --coverimage ../doc/pappl-512.png --body ../doc/pappl-body.md pappl.xml $(HEADERS) $(BASEOBJS:.o=.c) >../doc/$(PAPPL_PREFIX).html
	echo Generating $(PAPPL_PREFIX).epub...
	codedoc $(DOCFLAGS) --title "PAPPL v$(PAPPL_VERSION_MAJOR).$(PAPPL_VERSION_MINOR) Programmers Manual" --coverimage epub.png --body ../doc/pappl-body.md pappl.xml --epub ../doc/$(PAPPL_PREFIX).epub
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
	echo Installing $(PAPPL_MAKERESHEADER) in $(BUILDROOT)$(bindir)...
	$(INSTALL) -d -m 755 $(BUILDROOT)$(bindir)
	$(INSTALL) -c -m 755 makeresheader.sh $(BUILDROOT)$(bindir)/$(PAPPL_PREFIX)-makeresheader
	$(INSTALL) -c -m 755 pappl-exec $(BUILDROOT)$(bindir)/$(PAPPL_PREFIX)-exec
	echo Installing $(LIBPAPPL) in $(BUILDROOT)$(libdir)...
	$(INSTALL) -d -m 755 $(BUILDROOT)$(libdir)
	$(INSTALL) -c -m 755 $(LIBPAPPL) $(BUILDROOT)$(libdir)
	if test "$(LIBPAPPL)" = libpappl.so.2; then \
	    $(RM) $(BUILDROOT)$(libdir)/libpappl.so; \
	    $(LN) -s libpappl.so.2 $(BUILDROOT)$(libdir)/libpappl.so; \
	elif test "$(LIBPAPPL)" = libpappl.2.dylib; then \
	    $(RM) $(BUILDROOT)$(libdir)/libpappl.dylib; \
	    $(LN) -s libpappl.2.dylib $(BUILDROOT)$(libdir)/libpappl.dylib; \
	elif test "$(LIBPAPPL)" = libpappl2.so.2; then \
	    $(RM) $(BUILDROOT)$(libdir)/libpappl2.so; \
	    $(LN) -s libpappl2.so.2 $(BUILDROOT)$(libdir)/libpappl2.so; \
	elif test "$(LIBPAPPL)" = libpappl2.2.dylib; then \
	    $(RM) $(BUILDROOT)$(libdir)/libpappl2.dylib; \
	    $(LN) -s libpappl2.2.dylib $(BUILDROOT)$(libdir)/libpappl2.dylib; \
	fi
	if test "x$(LIBPAPPL_STATIC)" != x; then \
	    echo Installing $(LIBPAPPL_STATIC) in $(BUILDROOT)$(libdir)...; \
	    $(INSTALL) -c -m 644 $(LIBPAPPL_STATIC) $(BUILDROOT)$(libdir); \
	fi
	echo Installing pkg-config files in $(BUILDROOT)$(libdir)/pkgconfig...
	$(INSTALL) -d -m 755 $(BUILDROOT)$(libdir)/pkgconfig
	$(INSTALL) -c -m 644 pappl.pc $(BUILDROOT)$(libdir)/pkgconfig/$(PAPPL_PC)
	echo Installing header files in $(BUILDROOT)$(includedir)/pappl...
	$(INSTALL) -d -m 755 $(BUILDROOT)$(includedir)/pappl
	for file in $(HEADERS); do \
	    $(INSTALL) -c -m 644 $$file $(BUILDROOT)$(includedir)/pappl; \
	done


# Test everything
test:


# pappl exports file
libpappl2.def:		$(LIBOBJS) Makefile
	echo Generating $@...
	echo "LIBRARY libpappl2" >$@
	echo "VERSION 2.0" >>$@
	echo "EXPORTS" >>$@
	(nm $(LIBOBJS) 2>/dev/null | grep "T _" | awk '{print $$3}') | \
		grep -v '^__' | sed -e '1,$$s/^_//' | sort >>$@


# pappl static library
libpappl2.a libpappl.a:		$(LIBOBJS)
	echo Archiving $@...
	$(RM) $@
	$(AR) $(ARFLAGS) $@ $(LIBOBJS)
	$(RANLIB) $@


# pappl shared library
libpappl2.so.2 libpappl.so.2:		$(LIBOBJS)
	echo Linking $@...
	$(CC) $(DSOFLAGS) -shared -o $@ -Wl,-soname,$@ $(LIBOBJS) $(LIBS)
	$(RM) `basename $@ .2`
	$(LN) $@ `basename $@ .2`


# pappl shared library (macOS)
libpappl2.2.dylib libpappl.2.dylib:	$(LIBOBJS)
	echo Linking $@...
	$(CC) $(DSOFLAGS) -dynamiclib -o $@ \
	    -install_name $(libdir)/$@ \
	    -current_version $(PAPPL_VERSION_MAJOR).$(PAPPL_VERSION_MINOR) -compatibility_version 2.0 \
	    $(LIBOBJS) $(LIBS)
	$(CODE_SIGN) $(CSFLAGS) -i org.msweet.pappl.`basename $@ .2.dylib` $@
	$(RM) `basename $@ .2.dylib`.dylib
	$(LN) $@ `basename $@ .2.dylib`.dylib


# pappl-exec helper tool
pappl-exec:	pappl-exec.o $(LIBPAPPL)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o $@ pappl-exec.o -L. $(LINKPAPPL) $(LIBS)
	$(CODE_SIGN) $(CSFLAGS) -i org.msweet.pappl.$@ $@


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
	stringsutil -f base.strings -n _PAPPL_LOC scan *.c
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
