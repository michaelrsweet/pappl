#
# Documentation page makefile for the Printer Application Framework
#
# Copyright © 2020 by Michael R Sweet
#
# Licensed under Apache License v2.0.  See the file "LICENSE" for more
# information.
#

include ../Makedefs


DOCFILES =	\
		pappl.html \
		pappl-512.png \
		pappl-block.png


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
	echo Installing documentation in $(BUILDROOT)/$(datadir)/doc/pappl...
	$(INSTALL) -d -m 755 $(BUILDROOT)/$(datadir)/doc/pappl
	for file in $(DOCFILES); do \
		$(INSTALL) -c -m 644 $$file $(BUILDROOT)/$(datadir)/doc/pappl; \
	done


# Test everything
test:

