//
// Main test suite file for the Printer Application Framework
//
// Copyright © 2020 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

//
// Include necessary headers...
//

#include "testpappl.h"


//
// 'main()' - Main entry for test suite.
//

int
main(int  argc,				// I - Number of command-line arguments
     char *argv[])			// I - Command-line arguments
{
  pappl_system_t	*system;	// System
  pappl_printer_t	*printer;	// Printer
  static pappl_version_t versions[1] =	// Software versions
  {
    { "Test System", "", "1.0 build 42", { 1, 0, 0, 42 } }
  };


  system = papplSystemCreate(PAPPL_SOPTIONS_ALL, "Test System", /* hostname */NULL, /* port */0, "_print,_universal", /* spooldir */NULL, /* logfile */"-", PAPPL_LOGLEVEL_DEBUG, /* auth_service */NULL, /* tls_only */false);
  papplSystemAddListeners(system, NULL);
  test_setup_drivers(system);
  papplSystemSetFooterHTML(system,
                           "Copyright &copy; 2020 by Michael R Sweet. "
                           "Provided under the terms of the <a href=\"https://www.apache.org/licenses/LICENSE-2.0\">Apache License 2.0</a>.");
  papplSystemSetVersions(system, (int)(sizeof(versions) / sizeof(versions[0])), versions);

  printer = papplPrinterCreate(system, /* printer_id */0, "Label Printer", "pwg_4inch-203dpi-black_1", "file:///dev/null");
  papplPrinterSetLocation(printer, "Test Lab 42");
  papplPrinterSetOrganization(printer, "Lakeside Robotics");
  papplPrinterSetDNSSDName(printer, "Label Printer");

  printer = papplPrinterCreate(system, /* printer_id */0, "Inkjet Printer", "pwg_common-300dpi-600dpi-srgb_8", "file:///dev/null");
  papplPrinterSetLocation(printer, "Test Lab 42");
  papplPrinterSetOrganization(printer, "Lakeside Robotics");
  papplPrinterSetDNSSDName(printer, "Inkjet Printer");

  papplSystemRun(system);

  return (0);
}
