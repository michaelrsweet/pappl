//
// PapplMain test for the Printer Application Framework
//
// Copyright Â©Â 2020 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#include "testpappl.h"

int
testMain(int argc,    // Number of command line arguments
    char *argv[])     // Command line arguments
{
  papplMainAddSystemOption(PAPPL_SOPTIONS_LOG);
  papplMainRemoveSystemOption(PAPPL_SOPTIONS_MULTI_QUEUE);
  return (papplMain(argc, argv, test_setup_drivers));
}
