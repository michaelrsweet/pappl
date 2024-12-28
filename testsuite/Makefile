#
# Test suite makefile for the Printer Application Framework
#
# Copyright © 2020-2024 by Michael R Sweet
#
# Licensed under Apache License v2.0.  See the file "LICENSE" for more
# information.
#

include ../Makedefs


OBJS	=	\
		pwg-driver.o \
		testhttpmon.o \
		testmainloop.o \
		testpappl.o

TARGETS	=	\
		testhttpmon \
		testmainloop \
		testpappl


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
test:		testpappl
	$(RM) testpappl.log
	$(RM) -r testpappl.output
	$(MKDIR) testpappl.output
	./testhttpmon 2>test.log
	./testpappl -c -l testpappl.log -L debug -o testpappl.output -t api,client,pwg-raster 2>test.log


# HTTP monitor unit test
testhttpmon:	testhttpmon.o ../pappl/libpappl.a
	echo Linking $@...
	$(CC) $(LDFLAGS) -o $@ testhttpmon.o ../pappl/libpappl.a $(LIBS)
	$(CODE_SIGN) $(CSFLAGS) -i org.msweet.pappl.$@ $@


# Test suite program
testpappl:	testpappl.o pwg-driver.o ../pappl/libpappl.a
	echo Linking $@...
	$(CC) $(LDFLAGS) -o $@ testpappl.o pwg-driver.o ../pappl/libpappl.a $(LIBS)
	$(CODE_SIGN) $(CSFLAGS) -i org.msweet.pappl.$@ $@


# Mainloop test program
testmainloop:	testmainloop.o pwg-driver.o ../pappl/libpappl.a
	echo Linking $@...
	$(CC) $(LDFLAGS) -o $@ testmainloop.o pwg-driver.o ../pappl/libpappl.a $(LIBS)
	$(CODE_SIGN) $(CSFLAGS) -i org.msweet.pappl.$@ $@


# Static resource header...
resheader:
	echo Generating $@...
	../pappl/makeresheader.sh label-*.png >label-png.h


# Dependencies
include Dependencies

LIBS=-ldl -lpthread -lcups -lavahi-common -lavahi-client -lssl -lcrypto -ljpeg -lpng16 -lz -lusb-1.0 -lpam -lxml2
