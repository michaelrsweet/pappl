#
# Man page makefile for the Printer Application Framework
#
# Copyright © 2020-2025 by Michael R Sweet
#
# Licensed under Apache License v2.0.  See the file "LICENSE" for more
# information.
#

include ../Makedefs


MAN1	=	\
		.1 \
		-exec.1 \
		-makeresheader.1

MAN3	=	\
		-client.3 \
		-device.3 \
		-job.3 \
		-log.3 \
		-mainloop.3 \
		-printer.3 \
		-resource.3 \
		-system.3


# Make everything
all:


# Clean everything
clean:


# Clean all non-distribution files
distclean:	clean


# Update dependencies
depend:


# Install everything
install:
	echo Installing man pages in $(BUILDROOT)/$(mandir)/man1...
	$(INSTALL) -d -m 755 $(BUILDROOT)/$(mandir)/man1
	for file in $(MAN1); do \
		$(INSTALL) -c -m 644 pappl$$file $(BUILDROOT)/$(mandir)/man1/$(PAPPL_PREFIX)$$file; \
	done
	echo Installing man pages in $(BUILDROOT)/$(mandir)/man3...
	$(INSTALL) -d -m 755 $(BUILDROOT)/$(mandir)/man3
	for file in $(MAN3); do \
		$(INSTALL) -c -m 644 pappl$$file $(BUILDROOT)/$(mandir)/man3/$(PAPPL_PREFIX)$$file; \
	done


# Test everything
test:

