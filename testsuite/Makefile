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
	echo "./testhttpmon"
	./testhttpmon 2>test.log
	echo "./testpappl -c -l testpappl.log -L debug -d testpappl.spool -o testpappl.output -t api,client,pwg-raster,infra,client-10x1000,idle-shutdown"
	./testpappl -c -l testpappl.log -L debug -d testpappl.spool -o testpappl.output -t api,infra 2>test.log
#	./testpappl -c -l testpappl.log -L debug -d testpappl.spool -o testpappl.output -t api,client,pwg-raster,infra,client-10x1000,idle-shutdown 2>test.log


# HTTP monitor unit test
testhttpmon:	testhttpmon.o ../pappl/$(LINKPAPPL_STATIC)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o $@ testhttpmon.o ../pappl/$(LINKPAPPL_STATIC) $(LIBS)
	$(CODE_SIGN) $(CSFLAGS) -i org.msweet.pappl.$@ $@


# Test suite program
testpappl:	testpappl.o pwg-driver.o ../pappl/$(LINKPAPPL_STATIC)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o $@ testpappl.o pwg-driver.o ../pappl/$(LINKPAPPL_STATIC) $(LIBS)
	$(CODE_SIGN) $(CSFLAGS) -i org.msweet.pappl.$@ $@


# Mainloop test program
testmainloop:	testmainloop.o pwg-driver.o ../pappl/$(LINKPAPPL_STATIC)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o $@ testmainloop.o pwg-driver.o ../pappl/$(LINKPAPPL_STATIC) $(LIBS)
	$(CODE_SIGN) $(CSFLAGS) -i org.msweet.pappl.$@ $@


# Static resource header...
resheader:
	echo Generating $@...
	../pappl/makeresheader.sh label-*.png >label-png.h


# Dependencies
include Dependencies
