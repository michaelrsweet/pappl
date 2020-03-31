#
# Test suite makefile for the Printer Application Framework
#
# Copyright © 2020 by Michael R Sweet
#
# Licensed under Apache License v2.0.  See the file "LICENSE" for more
# information.
#

include ../Makedefs


OBJS	=	\
		pwg-driver.o \
		testpappl.o

TARGETS	=	testpappl


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
install:


# Test everything
test:


# Test suite program
testpappl:	$(OBJS) ../pappl/libpappl.a
	echo Linking $@...
	$(CC) $(LDFLAGS) -o $@ $(OBJS) ../pappl/libpappl.a $(LIBS)
	$(CODE_SIGN) -s "$(CODESIGN_IDENTITY)" -o runtime --timestamp -i org.msweet.pappl.testpappl $@


# Static resource header...
resheader:
	echo Generating $@...
	../pappl/makeresource.sh label-*.png >label-png.h


# Dependencies
include Dependencies
