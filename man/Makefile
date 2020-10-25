#
# Man page makefile for the Printer Application Framework
#
# Copyright © 2020 by Michael R Sweet
#
# Licensed under Apache License v2.0.  See the file "LICENSE" for more
# information.
#

include ../Makedefs


MAN1	=	\
		pappl.1 \
		pappl-makeresheader.1

MAN3	=	\
		pappl-client.3 \
		pappl-device.3 \
		pappl-job.3 \
		pappl-log.3 \
		pappl-mainloop.3 \
		pappl-printer.3 \
		pappl-resource.3 \
		pappl-system.3


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
		$(INSTALL) -c -m 644 $$file $(BUILDROOT)/$(mandir)/man1; \
	done
	echo Installing man pages in $(BUILDROOT)/$(mandir)/man3...
	$(INSTALL) -d -m 755 $(BUILDROOT)/$(mandir)/man3
	for file in $(MAN3); do \
		$(INSTALL) -c -m 644 $$file $(BUILDROOT)/$(mandir)/man3; \
	done


# Test everything
test:

