//
// Command line utilities for the Printer Application Framework
//
// Copyright © 2020-2023 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

//
// Include necessary headers
//

#include "pappl-private.h"
#if !_WIN32
#  include <libgen.h>
#endif // !_WIN32


//
// Local functions
//

static void	usage(const char *base_name, bool with_autoadd);


//
// 'papplMainloop()' - Run a standard main loop for printer applications.
//
// This function runs a standard main loop for a printer application.  The
// "argc" and "argv" arguments are those provided to the `main` function.
//
// The "version" argument specifies a numeric version number for the printer
// application that conforms to semantic versioning guidelines with up to four
// numbers, for example "1.2.3.4".
//
// The "footer_html" argument specifies HTML text to use in the footer of the
// web interface.  If `NULL`, the footer is omitted.
//
// The "num_drivers", "drivers", and "driver_cb" arguments specify a list of
// drivers and the driver callback for printers.  Specify `0` and `NULL` if
// the drivers are configured in the system callback.  The "autoadd_cb"
// argument specifies a callback for automatically adding new printers with the
// "autoadd" sub-command and for auto-detecting the driver when adding manually.
//
// The "usage_cb" argument specifies a callback that displays a usage/help
// summary.  If `NULL`, a generic summary is shown as needed.
//
// The "subcmd_name" and "subcmd_cb" arguments specify the name and a callback
// for a custom sub-command.  If `NULL`, no custom sub-commands will be
// supported.
//
// The "system_cb" argument specifies a function that will create a new
// `pappl_system_t` object.  If `NULL`, a default system object is created.
//
// The "data" argument provides application-specific data for each of the
// callbacks.
//

int					// O - Exit status
papplMainloop(
    int                   argc,		// I - Number of command line arguments
    char                  *argv[],	// I - Command line arguments
    const char            *version,	// I - Version number
    const char            *footer_html,	// I - Footer HTML or `NULL` for none
    int                   num_drivers,	// I - Number of drivers
    pappl_pr_driver_t     *drivers,	// I - Drivers
    pappl_pr_autoadd_cb_t autoadd_cb,	// I - Auto-add callback or `NULL` for none
    pappl_pr_driver_cb_t  driver_cb,	// I - Driver callback
    const char            *subcmd_name,	// I - Sub-command name or `NULL` for none
    pappl_ml_subcmd_cb_t  subcmd_cb,	// I - Sub-command callback or `NULL` for none
    pappl_ml_system_cb_t  system_cb,	// I - System callback or `NULL` for default
    pappl_ml_usage_cb_t   usage_cb,	// I - Usage callback or `NULL` for default
    void                  *data)	// I - Context pointer
{
  const char	*base_name;		// Base Name
  int		i, j;			// Looping vars
  const char	*opt;			// Option character
  const char	*subcommand = NULL;	// Sub-command
  cups_len_t	num_files = 0;		// File count
  char		*files[1000];		// Files array
  cups_len_t	num_options = 0;	// Number of options
  cups_option_t	*options = NULL;	// Options
  static const char * const subcommands[] =
  {					// List of standard sub-commands
    "add",
    "autoadd",
    "cancel",
    "default",
    "delete",
    "devices",
    "drivers",
    "jobs",
    "modify",
    "options",
    "pause",
    "printers",
    "resume",
    "server",
    "shutdown",
    "status",
    "submit"
  };
#ifdef __APPLE__
  const char	*server_argv[7];	// New command arguments
#endif // __APPLE__


  // Range check input...
  if (argc < 1 || !argv)
  {
    fputs("ERROR: No command-line arguments were passed to papplMainloop.\n", stderr);
    return (1);
  }

  if (!version)
  {
    fputs("ERROR: No version number string was passed to papplMainloop.\n", stderr);
    return (1);
  }

  // Save the path to the printer application and get the base name.
  _papplMainloopPath = argv[0];

#if _WIN32
  if ((base_name = strrchr(argv[0], '\\')) == NULL)
    if ((base_name = strrchr(argv[0], '/')) == NULL)
      base_name = argv[0];

#else
  base_name = basename(argv[0]);
#endif // _WIN32

#ifdef __APPLE__
  // When you click on the application icon, macOS' Finder will pass "-psn_..."
  // on the command-line.  We also recognize when running the application from
  // within the .app bundle.  If either is true, replace argc and argv to run a
  // server...
  if ((argc > 1 && !strncmp(argv[1], "-psn", 4)) || strstr(argv[0], ".app/Contents/MacOS/") != NULL)
  {
    server_argv[0] = argv[0];
    server_argv[1] = "server";
    server_argv[2] = "-o";
    server_argv[3] = "log-file=syslog";
    server_argv[4] = "-o";
    server_argv[5] = "log-level=info";
    server_argv[6] = NULL;

    argc = 6;
    argv = (char **)server_argv;
  }
#endif // __APPLE__

  // Parse the command-line...
  for (i = 1; i < argc; i ++)
  {
    if (!strcmp(argv[i], "--help"))
    {
      if (usage_cb)
        (*usage_cb)(data);
      else
        usage(base_name, autoadd_cb != NULL);

      return (0);
    }
    else if (!strcmp(argv[i], "--version"))
    {
      puts(version);
      return (0);
    }
    else if (!strcmp(argv[i], "--"))
    {
      // Filename follows
      i ++;

      if (i >= argc)
      {
        _papplLocPrintf(stderr, _PAPPL_LOC("%s: Missing filename after '--'."), base_name);
        return (1);
      }
      else if (num_files >= (int)(sizeof(files) / sizeof(files[0])))
      {
        _papplLocPrintf(stderr, _PAPPL_LOC("%s: Too many files."), base_name);
        return (1);
      }

      files[num_files ++] = argv[i];
    }
    else if (!strncmp(argv[i], "--", 2))
    {
      _papplLocPrintf(stderr, _PAPPL_LOC("%s: Unknown option '%s'."), base_name, argv[i]);
      return (1);
    }
    else if (argv[i][0] == '-' && argv[i][1])
    {
      for (opt = argv[i] + 1; *opt; opt ++)
      {
	switch (argv[i][1])
	{
          case 'a': // -a (cancel-all)
              num_options = cupsAddOption("cancel-all", "true", num_options, &options);
              break;

          case 'd': // -d PRINTER
              i ++;
              if (i >= argc)
              {
                _papplLocPrintf(stderr, _PAPPL_LOC("%s: Missing printer name after '-d'."), base_name);
                return (1);
              }

              num_options = cupsAddOption("printer-name", argv[i], num_options, &options);
	      break;

          case 'h': // -d HOST
              i ++;
              if (i >= argc)
              {
                _papplLocPrintf(stderr, _PAPPL_LOC("%s: Missing hostname after '-h'."), base_name);
                return (1);
              }

              num_options = cupsAddOption("server-hostname", argv[i], num_options, &options);
	      break;

          case 'j': // -j JOB-ID
              i ++;
              if (i >= argc)
              {
                _papplLocPrintf(stderr, _PAPPL_LOC("%s: Missing job ID after '-j'."), base_name);
                return (1);
              }

              num_options = cupsAddOption("job-id", argv[i], num_options, &options);
	      break;

          case 'm': // -m DRIVER-NAME
              i ++;
              if (i >= argc)
              {
                _papplLocPrintf(stderr, _PAPPL_LOC("%s: Missing driver name after '-m'."), base_name);
                return (1);
              }

              num_options = cupsAddOption("smi55357-driver", argv[i], num_options, &options);
	      break;

          case 'n': // -n COPIES
              i ++;
              if (i >= argc)
              {
                _papplLocPrintf(stderr, _PAPPL_LOC("%s: Missing copy count after '-n'."), base_name);
                return (1);
              }

              num_options = cupsAddOption("copies", argv[i], num_options, &options);
              break;

          case 'o': // -o "NAME=VALUE [... NAME=VALUE]"
              if (opt[1] && strchr(opt, '='))
              {
                _papplLocPrintf(stderr, _PAPPL_LOC("%s: Missing space after '-o'."), base_name);
                return (1);
              }

              i ++;
              if (i >= argc)
              {
                _papplLocPrintf(stderr, _PAPPL_LOC("%s: Missing option(s) after '-o'."), base_name);
                return (1);
              }

              num_options = cupsParseOptions(argv[i], num_options, &options);
	      break;

          case 't' : // -t TITLE
              i ++;
              if (i >= argc)
              {
                _papplLocPrintf(stderr, _PAPPL_LOC("%s: Missing title after '-t'."), base_name);
                return (1);
	      }

              num_options = cupsAddOption("job-name", argv[i], num_options, &options);
              break;

          case 'u': // -u PRINTER-URI
              i ++;
              if (i >= argc)
              {
                _papplLocPrintf(stderr, _PAPPL_LOC("%s: Missing printer URI after '-u'."), base_name);
                return (1);
              }

              num_options = cupsAddOption("printer-uri", argv[i], num_options, &options);
	      break;

          case 'v': // -v DEVICE-URI
              i ++;
              if (i >= argc)
              {
                _papplLocPrintf(stderr, _PAPPL_LOC("%s: Missing device URI after '-v'."), base_name);
                return (1);
              }

              num_options = cupsAddOption("smi55357-device-uri", argv[i], num_options, &options);
	      break;

          default:
              _papplLocPrintf(stderr, _PAPPL_LOC("%s: Unknown option '-%c'."), base_name, *opt);
              return (1);
        }
      }
    }
    else
    {
      // Skip "-" as needed...
      if (!strcmp(argv[i], "-"))
      {
        i ++;

        if (i >= argc)
          break;
      }

      // See if this is a standard sub-command...
      for (j = 0; j < (int)(sizeof(subcommands) / sizeof(subcommands[0])); j ++)
      {
        if (!strcmp(argv[i], subcommands[j]))
        {
          if (subcommand)
          {
            _papplLocPrintf(stderr, _PAPPL_LOC("%s: Cannot specify more than one sub-command."), base_name);
            return (1);
          }

          subcommand = argv[i];
          break;
	}
      }

      if (j >= (int)(sizeof(subcommands) / sizeof(subcommands[0])))
      {
        // Not a standard sub-command...
	if (subcmd_name && !strcmp(argv[i], subcmd_name))
        {
          // Extension sub-command...
          if (subcommand)
          {
            _papplLocPrintf(stderr, _PAPPL_LOC("%s: Cannot specify more than one sub-command."), base_name);
            return (1);
          }

          subcommand = argv[i];
        }
        else if (access(argv[i], R_OK))
        {
          _papplLocPrintf(stderr, _PAPPL_LOC("%s: Unable to access '%s': %s"), base_name, argv[i], strerror(errno));
          return (1);
        }
        else if (num_files < (int)(sizeof(files) / sizeof(files[0])))
        {
          // Filename...
	  files[num_files ++] = argv[i];
        }
        else
	{
	  // Too many files...
	  _papplLocPrintf(stderr, _PAPPL_LOC("%s: Too many files."), base_name);
	  return (1);
	}
      }
    }
  }

  // Process sub-commands
  if (!subcommand || !strcmp(subcommand, "submit"))
  {
    return (_papplMainloopSubmitJob(base_name, num_options, options, num_files, files));
  }
  else if (subcmd_name && !strcmp(subcommand, subcmd_name))
  {
    return ((subcmd_cb)(base_name, (int)num_options, options, (int)num_files, files, data));
  }
  else if (num_files > 0)
  {
    _papplLocPrintf(stderr, _PAPPL_LOC("%s: Sub-command '%s' does not accept files."), base_name, subcommand);
    return (1);
  }
  else if (!strcmp(subcommand, "add"))
  {
    return (_papplMainloopAddPrinter(base_name, num_options, options));
  }
  else if (!strcmp(subcommand, "autoadd"))
  {
    if (autoadd_cb)
    {
      return (_papplMainloopAutoAddPrinters(base_name, num_options, options));
    }
    else
    {
      _papplLocPrintf(stderr, _PAPPL_LOC("%s: Sub-command 'autoadd' is not supported."), base_name);
      return (1);
    }
  }
  else if (!strcmp(subcommand, "cancel"))
  {
    return (_papplMainloopCancelJob(base_name, num_options, options));
  }
  else if (!strcmp(subcommand, "default"))
  {
    return (_papplMainloopGetSetDefaultPrinter(base_name, num_options, options));
  }
  else if (!strcmp(subcommand, "delete"))
  {
    return (_papplMainloopDeletePrinter(base_name, num_options, options));
  }
  else if (!strcmp(subcommand, "devices"))
  {
    return (_papplMainloopShowDevices(base_name, num_options, options));
  }
  else if (!strcmp(subcommand, "drivers"))
  {
    return (_papplMainloopShowDrivers(base_name, (cups_len_t)num_drivers, drivers, autoadd_cb, driver_cb, num_options, options, system_cb, data));
  }
  else if (!strcmp(subcommand, "jobs"))
  {
    return (_papplMainloopShowJobs(base_name, num_options, options));
  }
  else if (!strcmp(subcommand, "modify"))
  {
    return (_papplMainloopModifyPrinter(base_name, num_options, options));
  }
  else if (!strcmp(subcommand, "options"))
  {
    return (_papplMainloopShowOptions(base_name, num_options, options));
  }
  else if (!strcmp(subcommand, "pause"))
  {
    return (_papplMainloopPausePrinter(base_name, num_options, options));
  }
  else if (!strcmp(subcommand, "printers"))
  {
    return (_papplMainloopShowPrinters(base_name, num_options, options));
  }
  else if (!strcmp(subcommand, "resume"))
  {
    return (_papplMainloopResumePrinter(base_name, num_options, options));
  }
  else if (!strcmp(subcommand, "server"))
  {
    return (_papplMainloopRunServer(base_name, version, footer_html, (cups_len_t)num_drivers, drivers, autoadd_cb, driver_cb, num_options, &options, system_cb, data));
  }
  else if (!strcmp(subcommand, "shutdown"))
  {
    return (_papplMainloopShutdownServer(base_name, num_options, options));
  }
  else if (!strcmp(subcommand, "status"))
  {
    return (_papplMainloopShowStatus(base_name, num_options, options));
  }
  else
  {
    // This should never happen...
    _papplLocPrintf(stderr, _PAPPL_LOC("%s: Unknown sub-command '%s'."), base_name, subcommand);
    return (1);
  }
}


//
// 'usage()' - Show default usage.
//

static void
usage(const char *base_name,		// I - Base name of application
      bool       with_autoadd)		// I - `true` if autoadd command is supported
{
  _papplLocPrintf(stdout, _PAPPL_LOC("Usage: %s SUB-COMMAND [OPTIONS] [FILENAME]\n"
				     "       %s [OPTIONS] [FILENAME]\n"
				     "       %s [OPTIONS] -"), base_name, base_name, base_name);
  puts("");
  _papplLocPrintf(stdout, _PAPPL_LOC("Sub-commands:"));
  _papplLocPrintf(stdout, _PAPPL_LOC("  add PRINTER      Add a printer."));
  if (with_autoadd)
    _papplLocPrintf(stdout, _PAPPL_LOC("  autoadd          Automatically add supported printers."));
  _papplLocPrintf(stdout, _PAPPL_LOC("  cancel           Cancel one or more jobs."));
  _papplLocPrintf(stdout, _PAPPL_LOC("  default          Set the default printer."));
  _papplLocPrintf(stdout, _PAPPL_LOC("  delete           Delete a printer."));
  _papplLocPrintf(stdout, _PAPPL_LOC("  devices          List devices."));
  _papplLocPrintf(stdout, _PAPPL_LOC("  drivers          List drivers."));
  _papplLocPrintf(stdout, _PAPPL_LOC("  jobs             List jobs."));
  _papplLocPrintf(stdout, _PAPPL_LOC("  modify           Modify a printer."));
  _papplLocPrintf(stdout, _PAPPL_LOC("  options          List printer options."));
  _papplLocPrintf(stdout, _PAPPL_LOC("  pause            Pause printing for a printer."));
  _papplLocPrintf(stdout, _PAPPL_LOC("  printers         List printers."));
  _papplLocPrintf(stdout, _PAPPL_LOC("  resume           Resume printing for a printer."));
  _papplLocPrintf(stdout, _PAPPL_LOC("  server           Run a server."));
  _papplLocPrintf(stdout, _PAPPL_LOC("  shutdown         Shutdown a running server."));
  _papplLocPrintf(stdout, _PAPPL_LOC("  status           Show server/printer/job status."));
  _papplLocPrintf(stdout, _PAPPL_LOC("  submit           Submit a file for printing."));
  puts("");
  _papplLocPrintf(stdout, _PAPPL_LOC("Options:"));
  _papplLocPrintf(stdout, _PAPPL_LOC("  -a               Cancel all jobs (cancel)."));
  _papplLocPrintf(stdout, _PAPPL_LOC("  -d PRINTER       Specify printer."));
  _papplLocPrintf(stdout, _PAPPL_LOC("  -j JOB-ID        Specify job ID (cancel)."));
  _papplLocPrintf(stdout, _PAPPL_LOC("  -m DRIVER-NAME   Specify driver (add/modify)."));
  _papplLocPrintf(stdout, _PAPPL_LOC("  -n COPIES        Specify number of copies (submit)."));
  _papplLocPrintf(stdout, _PAPPL_LOC("  -o NAME=VALUE    Specify option (add,modify,server,submit)."));
  _papplLocPrintf(stdout, _PAPPL_LOC("  -u URI           Specify ipp: or ipps: printer/server."));
  _papplLocPrintf(stdout, _PAPPL_LOC("  -v DEVICE-URI    Specify socket: or usb: device (add/modify)."));
}
