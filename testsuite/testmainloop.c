//
// papplMainloop unit test for the Printer Application Framework
//
// Copyright © 2020-2022 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#include "testpappl.h"
#include <pappl/base-private.h>
#include <config.h>


//
// Local constants...
//

#define FOOTER_HTML	"Copyright &copy; 2020-2022 by Michael R Sweet. Provided under the terms of the <a href=\"https://www.apache.org/licenses/LICENSE-2.0\">Apache License 2.0</a>."
#define VERSION_STRING	"1.2 build 42"


//
// Local functions...
//

static pappl_system_t	*system_cb(int num_options, cups_option_t *options, void *data);


//
// 'main()' - Main entry for unit test.
//

int					// O - Exit status
main(int  argc,				// I - Number of command line arguments
     char *argv[])			// I - Command line arguments
{
  if (getenv("PAPPL_USE_SYSTEM_CB"))
    return (papplMainloop(argc, argv, VERSION_STRING, FOOTER_HTML, (int)(sizeof(pwg_drivers) / sizeof(pwg_drivers[0])), pwg_drivers, /*autoadd_cb*/NULL, pwg_callback, /*subcmd_name*/NULL, /*subcmd_cb*/NULL, system_cb, /*usage_cb*/NULL, "testmainloop"));
  else
    return (papplMainloop(argc, argv, VERSION_STRING, FOOTER_HTML, (int)(sizeof(pwg_drivers) / sizeof(pwg_drivers[0])), pwg_drivers, /*autoadd_cb*/NULL, pwg_callback, /*subcmd_name*/NULL, /*subcmd_cb*/NULL, /*system_cb*/NULL, /*usage_cb*/NULL, "testmainloop"));
}


//
// 'system_cb()' - System callback.
//

pappl_system_t *			// O - New system object
system_cb(int           num_options,	// I - Number of options
	  cups_option_t *options,	// I - Options
	  void          *data)		// I - Callback data
{
  pappl_system_t	*system;	// System object
  const char		*val,		// Current option value
			*hostname,	// Hostname, if any
			*logfile,	// Log file, if any
			*system_name;	// System name, if any
  pappl_loglevel_t	loglevel;	// Log level
  int			port = 0;	// Port number, if any
  pappl_soptions_t	soptions = PAPPL_SOPTIONS_MULTI_QUEUE | PAPPL_SOPTIONS_WEB_INTERFACE | PAPPL_SOPTIONS_WEB_LOG | PAPPL_SOPTIONS_WEB_NETWORK | PAPPL_SOPTIONS_WEB_SECURITY | PAPPL_SOPTIONS_WEB_TLS;
					// System options
  static pappl_contact_t contact =	// Contact information
  {
    "John Q Admin",
    "jqadmin@example.org",
    "+1-705-555-1212"
  };
  static pappl_version_t versions[1] =	// Software versions
  {
    { "Test Application", "", VERSION_STRING, { PAPPL_VERSION_MAJOR, PAPPL_VERSION_MINOR, 0, 42 } }
  };


  // Verify that the right callback data was sent to us...
  if (!data || strcmp((char *)data, "testmainloop"))
  {
    fprintf(stderr, "testmainloop: Bad callback data %p.\n", data);
    return (NULL);
  }

  // Parse options...
  if ((val = cupsGetOption("log-level", (cups_len_t)num_options, options)) != NULL)
  {
    if (!strcmp(val, "fatal"))
      loglevel = PAPPL_LOGLEVEL_FATAL;
    else if (!strcmp(val, "error"))
      loglevel = PAPPL_LOGLEVEL_ERROR;
    else if (!strcmp(val, "warn"))
      loglevel = PAPPL_LOGLEVEL_WARN;
    else if (!strcmp(val, "info"))
      loglevel = PAPPL_LOGLEVEL_INFO;
    else if (!strcmp(val, "debug"))
      loglevel = PAPPL_LOGLEVEL_DEBUG;
    else
    {
      fprintf(stderr, "testmainloop: Bad log-level value '%s'.\n", val);
      return (NULL);
    }
  }
  else
    loglevel = PAPPL_LOGLEVEL_UNSPEC;

  logfile     = cupsGetOption("log-file", (cups_len_t)num_options, options);
  hostname    = cupsGetOption("server-hostname", (cups_len_t)num_options, options);
  system_name = cupsGetOption("system-name", (cups_len_t)num_options, options);

  if ((val = cupsGetOption("server-port", (cups_len_t)num_options, options)) != NULL)
  {
    if (!isdigit(*val & 255))
    {
      fprintf(stderr, "testmainloop: Bad server-port value '%s'.\n", val);
      return (NULL);
    }
    else
      port = atoi(val);
  }

  // Create the system object...
  if ((system = papplSystemCreate(soptions, system_name ? system_name : "testmainloop", port, "_print,_universal", cupsGetOption("spool-directory", (cups_len_t)num_options, options), logfile ? logfile : "-", loglevel, cupsGetOption("auth-service", (cups_len_t)num_options, options), /* tls_only */false)) == NULL)
    return (NULL);

  papplSystemAddListeners(system, NULL);
  papplSystemSetHostName(system, hostname);

  papplSystemSetPrinterDrivers(system, (int)(sizeof(pwg_drivers) / sizeof(pwg_drivers[0])), pwg_drivers, pwg_autoadd, /*create_cb*/NULL, pwg_callback, "testmainloop");

  papplSystemSetFooterHTML(system, FOOTER_HTML);
  papplSystemSetSaveCallback(system, (pappl_save_cb_t)papplSystemSaveState, (void *)"/tmp/testmainloop.state");
  papplSystemSetVersions(system, (int)(sizeof(versions) / sizeof(versions[0])), versions);

  if (!papplSystemLoadState(system, "/tmp/testmainloop.state"))
  {
    papplSystemSetContact(system, &contact);
    papplSystemSetDNSSDName(system, system_name ? system_name : "Test Mainloop");
    papplSystemSetGeoLocation(system, "geo:46.4707,-80.9961");
    papplSystemSetLocation(system, "Test Lab 42");
    papplSystemSetOrganization(system, "Example Company");
  }

  return (system);
}
