#
# Documentation page makefile for the Printer Application Framework
#
# Copyright © 2020-2024 by Michael R Sweet
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
	echo Installing documentation in $(BUILDROOT)/$(docdir)...
	$(INSTALL) -d -m 755 $(BUILDROOT)/$(docdir)
	for file in $(DOCFILES); do \
		$(INSTALL) -c -m 644 $$file $(BUILDROOT)/$(docdir); \
	done


# Test everything
test:

