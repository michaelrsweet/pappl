//
// PapplMain test for the Printer Application Framework
//
// Copyright Â©Â 2020 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#include "testpappl.h"

int          // O - Exit status
testMain(int argc,    // Number of command line arguments
    char *argv[])     // Command line arguments
{
  pappl_soptions_t soption = PAPPL_SOPTIONS_MULTI_QUEUE | PAPPL_SOPTIONS_RAW_SOCKET | PAPPL_SOPTIONS_SECURITY | PAPPL_SOPTIONS_STANDARD | PAPPL_SOPTIONS_TLS;
  pappl_version_t versions[] =
  {
    { "Test System", "", "1.0 build 42", { 1, 0, 0, 42 } }
  };
  pappl_contact_t contact =
  {
    "Michael R Sweet", "msweet@example.com", "+1-705-555-1212"
  };
  const char *footer = "Copyright &copy; 2020 by Michael R Sweet. Provided under the terms of the <a href=\"https://www.apache.org/licenses/LICENSE-2.0\">Apache License 2.0</a>.";
  const char *cb_state = "testpappl.state";

  return (papplMain(argc, argv, test_setup_drivers, cb_state, footer, soption, (int)(sizeof(versions)/sizeof(versions[0])), versions, &contact, "geo:46.4707,-80.9961", "Lakeside Robotics", NULL, NULL));
}
