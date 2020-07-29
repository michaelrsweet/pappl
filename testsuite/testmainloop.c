//
// papplMainloop unit test for the Printer Application Framework
//
// Copyright © 2020 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#include "testpappl.h"


//
// Local functions...
//

static pappl_system_t	*system_cb(int num_options, cups_option_t *options, void *data);
static const char *driver_cb(const char *device_id);


//
// 'main()' - Main entry for unit test.
//

int					// O - Exit status
main(int  argc,				// I - Number of command line arguments
     char *argv[])			// I - Command line arguments
{
  return (papplMainloop(argc, argv, "1.0 build 42", /*usage_cb*/NULL, /*subcmd_name*/NULL, /*subcmd_cb*/NULL, system_cb, driver_cb, "testmainloop"));
}


//
// 'driver_cb()' - Get driver callback.
//

static const char *    // O - Driver name or `NULL`
driver_cb(const char *device_id)   // I - IEEE-1284 device ID
{
  return "pwg_common-300dpi-600dpi-srgb_8";
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
  pappl_soptions_t	soptions = PAPPL_SOPTIONS_MULTI_QUEUE | PAPPL_SOPTIONS_STANDARD | PAPPL_SOPTIONS_LOG | PAPPL_SOPTIONS_NETWORK | PAPPL_SOPTIONS_SECURITY | PAPPL_SOPTIONS_TLS;
					// System options
  static pappl_contact_t contact =	// Contact information
  {
    "John Q Admin",
    "jqadmin@example.org",
    "+1-705-555-1212"
  };
  static pappl_version_t versions[1] =	// Software versions
  {
    { "Test Application", "", "1.0 build 42", { 1, 0, 0, 42 } }
  };


  // Verify that the right callback data was sent to us...
  if (!data || strcmp((char *)data, "testmainloop"))
  {
    fprintf(stderr, "testmainloop: Bad callback data %p.\n", data);
    return (NULL);
  }

  // Parse options...
  if ((val = cupsGetOption("log-level", num_options, options)) != NULL)
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

  logfile     = cupsGetOption("log-file", num_options, options);
  hostname    = cupsGetOption("server-hostname", num_options, options);
  system_name = cupsGetOption("system-name", num_options, options);

  if ((val = cupsGetOption("server-port", num_options, options)) != NULL)
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
  if ((system = papplSystemCreate(soptions, system_name ? system_name : "testmainloop", port, "_print,_universal", cupsGetOption("spool-directory", num_options, options), logfile ? logfile : "-", loglevel, cupsGetOption("auth-service", num_options, options), /* tls_only */false)) == NULL)
    return (NULL);

  papplSystemAddListeners(system, NULL);
  papplSystemSetHostname(system, hostname);
  test_setup_drivers(system);

  papplSystemSetFooterHTML(system,
                           "Copyright &copy; 2020 by Michael R Sweet. "
                           "Provided under the terms of the <a href=\"https://www.apache.org/licenses/LICENSE-2.0\">Apache License 2.0</a>.");
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
