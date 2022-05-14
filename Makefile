#
# Top-level makefile for the Printer Application Framework
#
# Copyright © 2020-2022 by Michael R Sweet
#
# Licensed under Apache License v2.0.  See the file "LICENSE" for more
# information.
#

include Makedefs


# Source directories...
DIRS	=	\
		doc \
		man \
		pappl \
		testsuite


# Make all targets...
all:
	echo Using CC="$(CC)"
	echo Using CFLAGS="$(CFLAGS)"
	echo Using DSOFLAGS="$(DSOFLAGS)"
	echo Using LDFLAGS="$(LDFLAGS)"
	echo Using LIBS="$(LIBS)"
	for dir in $(DIRS); do \
		echo Making all in $$dir...; \
		(cd $$dir; $(MAKE) $(MFLAGS) all) || exit 1; \
	done


# Remove object and target files...
clean:
	$(RM) testpappl.state
	for dir in $(DIRS); do \
		echo Cleaning all in $$dir...; \
		(cd $$dir; $(MAKE) $(MFLAGS) clean) || exit 1; \
	done


# Remove all non-distribution files...
distclean:	clean
	$(RM) Makedefs config.h config.log config.status pappl/pappl.pc
	-$(RM) -r autom4te*.cache


# Make dependencies
depend:
	for dir in $(DIRS); do \
		echo Updating dependencies in $$dir...; \
		(cd $$dir; $(MAKE) $(MFLAGS) depend) || exit 1; \
	done


# Install everything...
install:
	for dir in $(DIRS); do \
		echo Installing in $$dir...; \
		(cd $$dir; $(MAKE) $(MFLAGS) install) || exit 1; \
	done


# Test everything...
test:
	for dir in $(DIRS); do \
		echo Testing in $$dir...; \
		(cd $$dir; $(MAKE) $(MFLAGS) test) || exit 1; \
	done


# Don't run top-level build targets in parallel...
.NOTPARALLEL:
