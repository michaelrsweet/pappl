//
// Main test suite file for the Printer Application Framework
//
// Copyright © 2020 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//
// Usage:
//
//   testpappl [OPTIONS] ["SERVER NAME"]
//
// Options:
//
//   --help               Show help
//   --list[-TYPE]        List devices (dns-sd, local, network, usb)
//   --version            Show version
//   -1                   Single queue
//   -A PAM-SERVICE       Enable authentication using PAM service
//   -c                   Do a clean run (no loading of state)
//   -d SPOOL-DIRECTORY   Set the spool directory
//   -l LOG-FILE          Set the log file
//   -L LOG-LEVEL         Set the log level (fatal, error, warn, info, debug)
//   -m DRIVER-NAME       Add a printer with the named driver
//   -p PORT              Set the listen port (default auto)
//   -t TEST-NAME         Run the named test (see below)
//   -T                   Enable TLS-only mode
//   -U                   Enable USB printer gadget
//
// Tests:
//
//   all                  All of the following tests
//   client               Simulated client tests
//   jpeg                 JPEG image tests
//   png                  PNG image tests
//   pwg-raster           PWG Raster tests
//

//
// Include necessary headers...
//

#include <pappl/base-private.h>
#include <cups/dir.h>
#include "testpappl.h"
#include <stdlib.h>
#include <limits.h>


//
// Local types...
//

typedef struct _pappl_testdata_s	// Test data
{
  cups_array_t		*names;		// Tests to run
  pappl_system_t	*system;	// System
  const char		*outdirname;	// Output directory
} _pappl_testdata_t;


//
// Local functions...
//

static http_t	*connect_to_printer(pappl_system_t *system, char *uri, size_t urisize);
static void	device_error_cb(const char *message, void *err_data);
static bool	device_list_cb(const char *device_info, const char *device_uri, const char *device_id, void *data);
static const char *make_raster_file(ipp_t *response, bool grayscale, char *tempname, size_t tempsize);
static void	*run_tests(_pappl_testdata_t *testdata);
static bool	test_client(pappl_system_t *system);
#if defined(HAVE_LIBJPEG) || defined(HAVE_LIBPNG)
static bool	test_image_files(pappl_system_t *system, const char *prompt, const char *format, int num_files, const char * const *files);
#endif // HAVE_LIBJPEG || HAVE_LIBPNG
static bool	test_pwg_raster(pappl_system_t *system);
static int	usage(int status);


//
// 'main()' - Main entry for test suite.
//

int
main(int  argc,				// I - Number of command-line arguments
     char *argv[])			// I - Command-line arguments
{
  int			i;		// Looping var
  const char		*opt,		// Current option
			*name = NULL,	// System name, if any
			*spool = NULL,	// Spool directory, if any
			*outdir = ".",	// Output directory
			*log = NULL,	// Log file, if any
			*auth = NULL,	// Auth service, if any
			*model;		// Current printer model
  cups_array_t		*models;	// Printer models, if any
  int			port = 0;	// Port number, if any
  pappl_loglevel_t	level = PAPPL_LOGLEVEL_DEBUG;
  					// Log level
  bool			clean = false,	// Clean run?
			tls_only = false;
					// Restrict to TLS only?
  char			outdirname[PATH_MAX],
					// Output directory name
			device_uri[1024];
					// Device URI for printers
  pappl_soptions_t	soptions = PAPPL_SOPTIONS_MULTI_QUEUE | PAPPL_SOPTIONS_WEB_INTERFACE | PAPPL_SOPTIONS_WEB_LOG | PAPPL_SOPTIONS_WEB_NETWORK | PAPPL_SOPTIONS_WEB_SECURITY | PAPPL_SOPTIONS_WEB_TLS | PAPPL_SOPTIONS_RAW_SOCKET;
					// System options
  pappl_system_t	*system;	// System
  pappl_printer_t	*printer;	// Printer
  _pappl_testdata_t	testdata;	// Test data
  pthread_t		testid = 0;	// Test thread ID
  static pappl_contact_t contact =	// Contact information
  {
    "Michael R Sweet",
    "msweet@example.org",
    "+1-705-555-1212"
  };
  static pappl_version_t versions[1] =	// Software versions
  {
    { "Test System", "", "1.0 build 42", { 1, 0, 0, 42 } }
  };


  // Parse command-line options...
  models         = cupsArrayNew(NULL, NULL);
  testdata.names = cupsArrayNew(NULL, NULL);

  for (i = 1; i < argc; i ++)
  {
    if (!strcmp(argv[i], "--help"))
    {
      return (usage(0));
    }
    else if (!strcmp(argv[i], "--list"))
    {
      papplDeviceList(PAPPL_DEVTYPE_ALL, device_list_cb, NULL, device_error_cb, NULL);
      return (0);
    }
    else if (!strcmp(argv[i], "--list-dns-sd"))
    {
      papplDeviceList(PAPPL_DEVTYPE_DNS_SD, device_list_cb, NULL, device_error_cb, NULL);
      return (0);
    }
    else if (!strcmp(argv[i], "--list-local"))
    {
      papplDeviceList(PAPPL_DEVTYPE_LOCAL, device_list_cb, NULL, device_error_cb, NULL);
      return (0);
    }
    else if (!strcmp(argv[i], "--list-network"))
    {
      papplDeviceList(PAPPL_DEVTYPE_NETWORK, device_list_cb, NULL, device_error_cb, NULL);
      return (0);
    }
    else if (!strcmp(argv[i], "--list-usb"))
    {
      papplDeviceList(PAPPL_DEVTYPE_USB, device_list_cb, NULL, device_error_cb, NULL);
      return (0);
    }
    else if (!strcmp(argv[i], "--version"))
    {
      puts(PAPPL_VERSION);
      return (0);
    }
    else if (!strncmp(argv[i], "--", 2))
    {
      printf("testpappl: Unknown option '%s'.\n", argv[i]);
      return (usage(1));
    }
    else if (argv[i][0] == '-')
    {
      for (opt = argv[i] + 1; *opt; opt ++)
      {
        switch (*opt)
        {
          case '1' : // -1 (single queue)
              soptions &= (pappl_soptions_t)~PAPPL_SOPTIONS_MULTI_QUEUE;
              break;
          case 'A' : // -A PAM-SERVICE
              i ++;
              if (i >= argc)
              {
                puts("testpappl: Expected PAM service name after '-A'.");
                return (usage(1));
	      }
	      auth = argv[i];
              break;
          case 'c' : // -c (clean run)
              clean = true;
              break;
          case 'd' : // -d SPOOL-DIRECTORY
              i ++;
              if (i >= argc)
              {
                puts("testpappl: Expected spool directory after '-d'.");
                return (usage(1));
	      }
	      spool = argv[i];
              break;
          case 'l' : // -l LOG-FILE
              i ++;
              if (i >= argc)
              {
                puts("testpappl: Expected log file after '-l'.");
                return (usage(1));
	      }
	      log = argv[i];
              break;
          case 'L' : // -L LOG-LEVEL
              i ++;
              if (i >= argc)
              {
                puts("testpappl: Expected log level after '-L'.");
                return (usage(1));
	      }

              if (!strcmp(argv[i], "fatal"))
	      {
                level = PAPPL_LOGLEVEL_FATAL;
	      }
	      else if (!strcmp(argv[i], "error"))
	      {
                level = PAPPL_LOGLEVEL_ERROR;
	      }
	      else if (!strcmp(argv[i], "warn"))
	      {
                level = PAPPL_LOGLEVEL_WARN;
	      }
	      else if (!strcmp(argv[i], "info"))
	      {
                level = PAPPL_LOGLEVEL_INFO;
	      }
	      else if (!strcmp(argv[i], "debug"))
	      {
                level = PAPPL_LOGLEVEL_DEBUG;
	      }
	      else
	      {
	        printf("testpappl: Unknown log level '%s'.\n", argv[i]);
	        return (usage(1));
	      }
              break;
	  case 'm' : // -m DRIVER-NAME
	      i ++;
              if (i >= argc)
              {
                puts("testpappl: Expected driver name after '-m'.");
                return (usage(1));
	      }
	      cupsArrayAdd(models, argv[i]);
              break;
	  case 'o' : // -o OUTPUT-DIRECTORY
	      i ++;
	      if (i >= argc)
	      {
                puts("testpappl: Expected output directory after '-o'.");
                return (usage(1));
	      }
	      outdir = argv[i];
	      break;
          case 'p' : // -p PORT-NUMBER
              i ++;
              if (i >= argc || atoi(argv[i]) <= 0 || atoi(argv[i]) > 32767)
              {
                puts("testpappl: Expected port number after '-p'.");
                return (usage(1));
	      }
	      port = atoi(argv[i]);
              break;
	  case 't' : // -t TEST
	      i ++;
	      if (i >= argc)
	      {
                puts("testpappl: Expected test name after '-t'.");
                return (usage(1));
	      }

	      if (!strcmp(argv[i], "all"))
	      {
		cupsArrayAdd(testdata.names, "client");
		cupsArrayAdd(testdata.names, "jpeg");
		cupsArrayAdd(testdata.names, "png");
		cupsArrayAdd(testdata.names, "pwg-raster");
	      }
	      else
	      {
		cupsArrayAdd(testdata.names, argv[i]);
	      }
	      break;
	  case 'T' : // -T (TLS only)
	      tls_only = true;
	      break;
	  case 'U' : // -U (USB printer gadget)
	      soptions |= PAPPL_SOPTIONS_USB_PRINTER;
	      break;
	  default :
	      printf("testpappl: Unknown option '-%c'.\n", *opt);
	      return (usage(1));
        }
      }
    }
    else if (name)
    {
      printf("testpappl: Unexpected argument '%s'.\n", argv[i]);
      return (usage(1));
    }
    else
    {
      // "SERVER NAME"
      name = argv[i];
    }
  }

  // Initialize the system and any printers...
  system = papplSystemCreate(soptions, name ? name : "Test System", port, "_print,_universal", spool, log, level, auth, tls_only);
  papplSystemAddListeners(system, NULL);
  papplSystemSetPrinterDrivers(system, (int)(sizeof(pwg_drivers) / sizeof(pwg_drivers[0])), pwg_drivers, /* autoadd_cb */NULL, /* create_cb */NULL, pwg_callback, "testpappl");
  papplSystemAddLink(system, "Configuration", "/config", true);
  papplSystemSetFooterHTML(system,
                           "Copyright &copy; 2020 by Michael R Sweet. "
                           "Provided under the terms of the <a href=\"https://www.apache.org/licenses/LICENSE-2.0\">Apache License 2.0</a>.");
  papplSystemSetSaveCallback(system, (pappl_save_cb_t)papplSystemSaveState, (void *)"testpappl.state");
  papplSystemSetVersions(system, (int)(sizeof(versions) / sizeof(versions[0])), versions);

  httpAssembleURIf(HTTP_URI_CODING_ALL, device_uri, sizeof(device_uri), "file", NULL, NULL, 0, "%s?ext=pwg", realpath(outdir, outdirname));

  if (clean || !papplSystemLoadState(system, "testpappl.state"))
  {
    papplSystemSetContact(system, &contact);
    papplSystemSetDNSSDName(system, name ? name : "Test System");
    papplSystemSetGeoLocation(system, "geo:46.4707,-80.9961");
    papplSystemSetLocation(system, "Test Lab 42");
    papplSystemSetOrganization(system, "Lakeside Robotics");

    if (cupsArrayCount(models))
    {
      for (model = (const char *)cupsArrayFirst(models), i = 1; model; model = (const char *)cupsArrayNext(models), i ++)
      {
        char	pname[128];		// Printer name

        if (cupsArrayCount(models) == 1)
	  snprintf(pname, sizeof(pname), "%s", name ? name : "Test Printer");
        else
	  snprintf(pname, sizeof(pname), "%s %d", name ? name : "Test Printer", i);

	printer = papplPrinterCreate(system, /* printer_id */0, pname, model, "MFG:PWG;MDL:Test Printer;", device_uri);
	papplPrinterSetContact(printer, &contact);
	papplPrinterSetDNSSDName(printer, pname);
	papplPrinterSetGeoLocation(printer, "geo:46.4707,-80.9961");
	papplPrinterSetLocation(printer, "Test Lab 42");
	papplPrinterSetOrganization(printer, "Lakeside Robotics");
      }
    }
    else
    {
      printer = papplPrinterCreate(system, /* printer_id */0, "Office Printer", "pwg_common-300dpi-600dpi-srgb_8", "MFG:PWG;MDL:Office Printer;", device_uri);
      papplPrinterSetContact(printer, &contact);
      papplPrinterSetDNSSDName(printer, "Office Printer");
      papplPrinterSetGeoLocation(printer, "geo:46.4707,-80.9961");
      papplPrinterSetLocation(printer, "Test Lab 42");
      papplPrinterSetOrganization(printer, "Lakeside Robotics");

      if (soptions & PAPPL_SOPTIONS_MULTI_QUEUE)
      {
	printer = papplPrinterCreate(system, /* printer_id */0, "Label Printer", "pwg_4inch-203dpi-black_1", "MFG:PWG;MDL:Label Printer;", device_uri);
	papplPrinterSetContact(printer, &contact);
	papplPrinterSetDNSSDName(printer, "Label Printer");
	papplPrinterSetGeoLocation(printer, "geo:46.4707,-80.9961");
	papplPrinterSetLocation(printer, "Test Lab 42");
	papplPrinterSetOrganization(printer, "Lakeside Robotics");
      }
    }
  }

  cupsArrayDelete(models);

  // Run any test(s)...
  if (cupsArrayCount(testdata.names))
  {
    testdata.outdirname = outdirname;
    testdata.system     = system;

    if (pthread_create(&testid, NULL, (void *(*)(void *))run_tests, &testdata))
    {
      perror("Unable to start testing thread");
      return (1);
    }
  }

  // Run the system...
  papplSystemRun(system);

  if (testid)
  {
    void *ret;				// Return value from testing thread

    if (pthread_join(testid, &ret))
    {
      perror("Unable to get testing thread status");
      return (1);
    }
    else
      return (ret != NULL);
  }

  return (0);
}


//
// 'connect_to_printer()' - Connect to the system and return the printer URI.
//

static http_t *				// O - HTTP connection
connect_to_printer(
    pappl_system_t *system,		// I - System
    char           *uri,		// I - URI buffer
    size_t         urisize)		// I - Size of URI buffer
{
  httpAssembleURI(HTTP_URI_CODING_ALL, uri, (int)urisize, "ipp", NULL, "localhost", papplSystemGetPort(system), "/ipp/print");

  return (httpConnect2("localhost", papplSystemGetPort(system), NULL, AF_UNSPEC, HTTP_ENCRYPTION_IF_REQUESTED, 1, 30000, NULL));
}


//
// 'device_error_cb()' - Show a device error message.
//

static void
device_error_cb(const char *message,	// I - Error message
                void       *err_data)	// I - Callback data (unused)
{
  (void)err_data;

  printf("testpappl: %s\n", message);
}


//
// 'device_list_cb()' - List a device.
//

static bool				// O - `true` to stop, `false` to continue
device_list_cb(const char *device_info,	// I - Device description
               const char *device_uri,	// I - Device URI
               const char *device_id,	// I - IEEE-1284 device ID
               void       *data)	// I - Callback data (unused)
{
  (void)data;

  printf("%s\n    %s\n    %s\n", device_info, device_uri, device_id);

  return (false);
}


//
// 'make_raster_file()' - Create a temporary PWG raster file.
//
// Note: Adapted from CUPS "testclient.c"...
//

static const char *                     // O - Print filename
make_raster_file(ipp_t      *response,  // I - Printer attributes
                 bool       grayscale,  // I - Force grayscale?
                 char       *tempname,  // I - Temporary filename buffer
                 size_t     tempsize)   // I - Size of temp file buffer
{
  int                   i,              // Looping var
                        count;          // Number of values
  ipp_attribute_t       *attr;          // Printer attribute
  const char            *type = NULL;   // Raster type (colorspace + bits)
  pwg_media_t           *media = NULL;  // Media size
  int                   xdpi = 0,       // Horizontal resolution
                        ydpi = 0;       // Vertical resolution
  int                   fd;             // Temporary file
  cups_raster_t         *ras;           // Raster stream
  cups_page_header2_t   header;         // Page header
  unsigned char         *line,          // Line of raster data
                        *lineptr;       // Pointer into line
  unsigned              y,              // Current position on page
                        xcount, ycount, // Current count for X and Y
                        xrep, yrep,     // Repeat count for X and Y
                        xoff, yoff,     // Offsets for X and Y
                        yend;           // End Y value
  int                   temprow,        // Row in template
                        tempcolor;      // Template color
  const char            *template;      // Pointer into template
  const unsigned char   *color;         // Current color
  static const unsigned char colors[][3] =
  {                                     // Colors for test
    { 191, 191, 191 },
    { 127, 127, 127 },
    {  63,  63,  63 },
    {   0,   0,   0 },
    { 255,   0,   0 },
    { 255, 127,   0 },
    { 255, 255,   0 },
    { 127, 255,   0 },
    {   0, 255,   0 },
    {   0, 255, 127 },
    {   0, 255, 255 },
    {   0, 127, 255 },
    {   0,   0, 255 },
    { 127,   0, 255 },
    { 255,   0, 255 }
  };
  static const char * const templates[] =
  {                                     // Raster template
    "PPPP     A    PPPP   PPPP   L      TTTTT  EEEEE   SSS   TTTTT          000     1     222    333      4   55555   66    77777   888    999   ",
    "P   P   A A   P   P  P   P  L        T    E      S   S    T           0   0   11    2   2  3   3  4  4   5      6          7  8   8  9   9  ",
    "P   P  A   A  P   P  P   P  L        T    E      S        T           0   0    1        2      3  4  4   5      6         7   8   8  9   9  ",
    "PPPP   AAAAA  PPPP   PPPP   L        T    EEEE    SSS     T           0 0 0    1      22    333   44444   555   6666      7    888    9999  ",
    "P      A   A  P      P      L        T    E          S    T           0   0    1     2         3     4       5  6   6    7    8   8      9  ",
    "P      A   A  P      P      L        T    E      S   S    T           0   0    1    2      3   3     4   5   5  6   6    7    8   8      9  ",
    "P      A   A  P      P      LLLLL    T    EEEEE   SSS     T            000    111   22222   333      4    555    666     7     888     99   ",
    "                                                                                                                                            "
  };


  // Figure out the the media, resolution, and color mode...
  if ((attr = ippFindAttribute(response, "media-ready", IPP_TAG_KEYWORD)) != NULL)
  {
    // Use ready media...
    if (ippContainsString(attr, "na_letter_8.5x11in"))
      media = pwgMediaForPWG("na_letter_8.5x11in");
    else if (ippContainsString(attr, "iso_a4_210x297mm"))
      media = pwgMediaForPWG("iso_a4_210x297mm");
    else
      media = pwgMediaForPWG(ippGetString(attr, 0, NULL));
  }
  else if ((attr = ippFindAttribute(response, "media-default", IPP_TAG_KEYWORD)) != NULL)
  {
    // Use default media...
    media = pwgMediaForPWG(ippGetString(attr, 0, NULL));
  }
  else
  {
    puts("FAIL (No default or ready media reported by printer)");
    return (NULL);
  }

  if ((attr = ippFindAttribute(response, "pwg-raster-document-resolution-supported", IPP_TAG_RESOLUTION)) != NULL)
  {
    for (i = 0, count = ippGetCount(attr); i < count; i ++)
    {
      int tempxdpi, tempydpi;
      ipp_res_t tempunits;

      tempxdpi = ippGetResolution(attr, 0, &tempydpi, &tempunits);

      if (i == 0 || tempxdpi < xdpi || tempydpi < ydpi)
      {
        xdpi = tempxdpi;
        ydpi = tempydpi;
      }
    }

    if ((attr = ippFindAttribute(response, "pwg-raster-document-type-supported", IPP_TAG_KEYWORD)) != NULL)
    {
      if (!grayscale && ippContainsString(attr, "srgb_8"))
        type = "srgb_8";
      else if (ippContainsString(attr, "sgray_8"))
        type = "sgray_8";
    }
  }

  if (xdpi < 72 || ydpi < 72)
  {
    puts("FAIL (No supported raster resolutions)");
    return (NULL);
  }

  if (!type)
  {
    puts("FAIL (No supported color spaces or bit depths)");
    return (NULL);
  }

  // Make the raster context and details...
  if (!cupsRasterInitPWGHeader(&header, media, type, xdpi, ydpi, "one-sided", NULL))
  {
    printf("FAIL (Unable to initialize raster context: %s)\n", cupsRasterErrorString());
    return (NULL);
  }

  header.cupsInteger[CUPS_RASTER_PWG_TotalPageCount] = 1;

  if (header.cupsWidth > (2 * header.HWResolution[0]))
  {
    xoff = header.HWResolution[0] / 2;
    yoff = header.HWResolution[1] / 2;
  }
  else
  {
    xoff = header.HWResolution[0] / 4;
    yoff = header.HWResolution[1] / 4;
  }

  xrep = (header.cupsWidth - 2 * xoff) / 140;
  yrep = xrep * header.HWResolution[1] / header.HWResolution[0];
  yend = header.cupsHeight - yoff;

  // Prepare the raster file...
  if ((line = malloc(header.cupsBytesPerLine)) == NULL)
  {
    printf("FAIL (Unable to allocate %u bytes for raster output: %s)\n", header.cupsBytesPerLine, strerror(errno));
    return (NULL);
  }

  if ((fd = cupsTempFd(tempname, (int)tempsize)) < 0)
  {
    printf("FAIL (Unable to create temporary print file: %s)\n", strerror(errno));
    free(line);
    return (NULL);
  }

  if ((ras = cupsRasterOpen(fd, CUPS_RASTER_WRITE_PWG)) == NULL)
  {
    printf("FAIL (Unable to open raster stream: %s)\n", cupsRasterErrorString());
    close(fd);
    free(line);
    return (NULL);
  }

  // Write a single page consisting of the template dots repeated over the page.
  cupsRasterWriteHeader2(ras, &header);

  memset(line, 0xff, header.cupsBytesPerLine);

  for (y = 0; y < yoff; y ++)
    cupsRasterWritePixels(ras, line, header.cupsBytesPerLine);

  for (temprow = 0, tempcolor = 0; y < yend;)
  {
    template = templates[temprow];
    color    = colors[tempcolor];

    temprow ++;
    if (temprow >= (int)(sizeof(templates) / sizeof(templates[0])))
    {
      temprow = 0;
      tempcolor ++;
      if (tempcolor >= (int)(sizeof(colors) / sizeof(colors[0])))
        tempcolor = 0;
      else if (tempcolor > 3 && header.cupsColorSpace == CUPS_CSPACE_SW)
        tempcolor = 0;
    }

    memset(line, 0xff, header.cupsBytesPerLine);

    if (header.cupsColorSpace == CUPS_CSPACE_SW)
    {
      // Do grayscale output...
      for (lineptr = line + xoff; *template; template ++)
      {
        if (*template != ' ')
        {
          for (xcount = xrep; xcount > 0; xcount --)
            *lineptr++ = *color;
        }
        else
        {
          lineptr += xrep;
        }
      }
    }
    else
    {
      // Do color output...
      for (lineptr = line + 3 * xoff; *template; template ++)
      {
        if (*template != ' ')
        {
          for (xcount = xrep; xcount > 0; xcount --, lineptr += 3)
            memcpy(lineptr, color, 3);
        }
        else
        {
          lineptr += 3 * xrep;
        }
      }
    }

    for (ycount = yrep; ycount > 0 && y < yend; ycount --, y ++)
      cupsRasterWritePixels(ras, line, header.cupsBytesPerLine);
  }

  memset(line, 0xff, header.cupsBytesPerLine);

  for (y = 0; y < header.cupsHeight; y ++)
    cupsRasterWritePixels(ras, line, header.cupsBytesPerLine);

  free(line);

  cupsRasterClose(ras);

  close(fd);

  return (tempname);
}


//
// 'run_tests()' - Run named tests.
//

static void *				// O - Thread status
run_tests(_pappl_testdata_t *testdata)	// I - Testing data
{
  const char	*name;			// Test name
  void		*ret = NULL;		// Return thread status
  cups_dir_t	*dir;			// Output directory
  cups_dentry_t	*dent;			// Output file
  int		files = 0;		// Total file count
  off_t		total = 0;		// Total output size
#ifdef HAVE_LIBJPEG
  static const char * const jpeg_files[] =
  {					// List of JPEG files to print
    "portrait-gray.jpg",
    "portrait-color.jpg",
    "landscape-gray.jpg",
    "landscape-color.jpg"
  };
#endif // HAVE_LIBJPEG
#ifdef HAVE_LIBPNG
  static const char * const png_files[] =
  {					// List of PNG files to print
    "portrait-gray.png",
    "portrait-color.png",
    "landscape-gray.png",
    "landscape-color.png"
  };
#endif // HAVE_LIBPNG

  // Wait for the system to start...
  while (!papplSystemIsRunning(testdata->system))
    sleep(1);

  // Run each test...
  for (name = (const char *)cupsArrayFirst(testdata->names); name && !ret && !papplSystemIsShutdown(testdata->system); name = (const char *)cupsArrayNext(testdata->names))
  {
    printf("%s: ", name);
    fflush(stdout);

    if (!strcmp(name, "client"))
    {
      if (!test_client(testdata->system))
        ret = (void *)1;
      else
        puts("PASS");
    }
    else if (!strcmp(name, "jpeg"))
    {
#ifdef HAVE_LIBJPEG
      if (!test_image_files(testdata->system, "jpeg", "image/jpeg", (int)(sizeof(jpeg_files) / sizeof(jpeg_files[0])), jpeg_files))
        ret = (void *)1;
      else
        puts("PASS");
#else
      puts("SKIP");
#endif // HAVE_LIBJPEG
    }
    else if (!strcmp(name, "png"))
    {
#ifdef HAVE_LIBPNG
      if (!test_image_files(testdata->system, "png", "image/png", (int)(sizeof(png_files) / sizeof(png_files[0])), png_files))
        ret = (void *)1;
      else
        puts("PASS");
#else
      puts("SKIP");
#endif // HAVE_LIBPNG
    }
    else if (!strcmp(name, "pwg-raster"))
    {
      if (!test_pwg_raster(testdata->system))
        ret = (void *)1;
      else
        puts("PASS");
    }
    else
    {
      puts("UNKNOWN TEST");
      ret = (void *)1;
    }
  }

  // Summarize results...
  if ((dir = cupsDirOpen(testdata->outdirname)) != NULL)
  {
    while ((dent = cupsDirRead(dir)) != NULL)
    {
      if (S_ISREG(dent->fileinfo.st_mode))
      {
        files ++;
        total += dent->fileinfo.st_size;
      }
    }

    cupsDirClose(dir);
  }

  papplSystemShutdown(testdata->system);

  if (ret)
    printf("\nFAILED: %d output file(s), %.1fMB\n", files, total / 1048576.0);
  else
    printf("\nPASSED: %d output file(s), %.1fMB\n", files, total / 1048576.0);

  return (ret);
}


//
// 'test_client()' - Run simulated client tests.
//

static bool				// O - `true` on success, `false` on failure
test_client(pappl_system_t *system)	// I - System
{
  http_t	*http;			// HTTP connection
  char		uri[1024];		// "printer-uri" value
  ipp_t		*request,		// Request
		*response;		// Response
  int		i;			// Looping var
  static const char * const pattrs[] =	// Printer attributes
  {
    "printer-contact-col",
    "printer-current-time",
    "printer-geo-location",
    "printer-location",
    "printer-name",
    "printer-state",
    "printer-state-reasons",
    "printer-uuid",
    "printer-uri-supported"
  };
  static const char * const sattrs[] =	// System attributes
  {
    "system-contact-col",
    "system-current-time",
    "system-geo-location",
    "system-location",
    "system-name",
    "system-state",
    "system-state-reasons",
    "system-uuid",
    "system-xri-supported"
  };


  // Connect to system...
  if ((http = connect_to_printer(system, uri, sizeof(uri))) == NULL)
  {
    printf("FAIL (Unable to connect: %s)\n", cupsLastErrorString());
    return (false);
  }

  // Test Get-System-Attributes
  fputs("Get-System-Attributes ", stdout);

  request = ippNewRequest(IPP_OP_GET_SYSTEM_ATTRIBUTES);
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "system-uri", NULL, "ipp://localhost/ipp/system");
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name", NULL, cupsUser());

  response = cupsDoRequest(http, request, "/ipp/system");

  if (cupsLastError() != IPP_STATUS_OK)
  {
    printf("FAIL (%s)\n", cupsLastErrorString());
    httpClose(http);
    ippDelete(response);
    return (false);
  }
  else
  {
    for (i = 0; i < (int)(sizeof(sattrs) / sizeof(sattrs[0])); i ++)
    {
      if (!ippFindAttribute(response, sattrs[i], IPP_TAG_ZERO))
      {
	printf("FAIL (Missing required '%s' attribute in response)\n", sattrs[i]);
	httpClose(http);
	ippDelete(response);
	return (false);
      }
    }

    ippDelete(response);
  }

  // Test Get-Printers
  fputs("\nclient: Get-Printers ", stdout);

  request = ippNewRequest(IPP_OP_GET_PRINTERS);
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "system-uri", NULL, "ipp://localhost/ipp/system");
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name", NULL, cupsUser());

  response = cupsDoRequest(http, request, "/ipp/system");

  if (cupsLastError() != IPP_STATUS_OK)
  {
    printf("FAIL (%s)\n", cupsLastErrorString());
    httpClose(http);
    ippDelete(response);
    return (false);
  }
  else
  {
    for (i = 0; i < (int)(sizeof(pattrs) / sizeof(pattrs[0])); i ++)
    {
      if (!ippFindAttribute(response, pattrs[i], IPP_TAG_ZERO))
      {
	printf("FAIL (Missing required '%s' attribute in response)\n", sattrs[i]);
	httpClose(http);
	ippDelete(response);
	return (false);
      }
    }

    ippDelete(response);
  }

  // Test Get-Printer-Attributes on /
  fputs("\nclient: Get-Printer-Attributes=/ ", stdout);

  request = ippNewRequest(IPP_OP_GET_PRINTER_ATTRIBUTES);
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri", NULL, "ipp://localhost/");
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name", NULL, cupsUser());

  response = cupsDoRequest(http, request, "/");

  if (cupsLastError() != IPP_STATUS_OK)
  {
    printf("FAIL (%s)\n", cupsLastErrorString());
    httpClose(http);
    ippDelete(response);
    return (false);
  }
  else
  {
    for (i = 0; i < (int)(sizeof(pattrs) / sizeof(pattrs[0])); i ++)
    {
      if (!ippFindAttribute(response, pattrs[i], IPP_TAG_ZERO))
      {
	printf("FAIL (Missing required '%s' attribute in response)\n", sattrs[i]);
	httpClose(http);
	ippDelete(response);
	return (false);
      }
    }

    ippDelete(response);
  }

  // Test Get-Printer-Attributes on /ipp/print
  fputs("\nclient: Get-Printer-Attributes=/ipp/print ", stdout);

  request = ippNewRequest(IPP_OP_GET_PRINTER_ATTRIBUTES);
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri", NULL, "ipp://localhost/ipp/print");
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name", NULL, cupsUser());

  response = cupsDoRequest(http, request, "/ipp/print");

  if (cupsLastError() != IPP_STATUS_OK)
  {
    printf("FAIL (%s)\n", cupsLastErrorString());
    httpClose(http);
    ippDelete(response);
    return (false);
  }
  else
  {
    for (i = 0; i < (int)(sizeof(pattrs) / sizeof(pattrs[0])); i ++)
    {
      if (!ippFindAttribute(response, pattrs[i], IPP_TAG_ZERO))
      {
	printf("FAIL (Missing required '%s' attribute in response)\n", sattrs[i]);
	httpClose(http);
	ippDelete(response);
	return (false);
      }
    }

    ippDelete(response);
  }

  httpClose(http);

  return (true);
}


#if defined(HAVE_LIBJPEG) || defined(HAVE_LIBPNG)
//
// 'test_image_files()' - Run image file tests.
//

static bool				// O - `true` on success, `false` on failure
test_image_files(
    pappl_system_t       *system,	// I - System
    const char           *prompt,	// I - Prompt for files
    const char           *format,	// I - MIME media type of files
    int                  num_files,	// I - Number of files to print
    const char * const * files)		// I - Files to print
{
  int		i, j, k, m;		// Looping vars
  http_t	*http;			// HTTP connection
  char		uri[1024],		// "printer-uri" value
		filename[1024],		// Print file
		job_name[1024];		// "job_name" value
  ipp_t		*request,		// Request
		*response;		// Response
  int		job_id;			// "job-id" value
  ipp_jstate_t	job_state;		// "job-state" value
  static const int orients[] =		// "orientation-requested" values
  {
    IPP_ORIENT_NONE,
    IPP_ORIENT_PORTRAIT,
    IPP_ORIENT_LANDSCAPE,
    IPP_ORIENT_REVERSE_PORTRAIT,
    IPP_ORIENT_REVERSE_LANDSCAPE
  };
  static const char * const modes[] =	// "print-color-mode" values
  {
    "auto",
    "color",
    "monochrome"
  };
  static const char * const scalings[] =// "print-scaling" values
  {
    "auto",
    "auto-fit",
    "fill",
    "fit",
    "none"
  };


  // Connect to system...
  if ((http = connect_to_printer(system, uri, sizeof(uri))) == NULL)
  {
    printf("FAIL (Unable to connect: %s)\n", cupsLastErrorString());
    return (false);
  }

  // Print files...
  for (i = 0; i < num_files; i ++)
  {
    if (access(files[i], R_OK))
      snprintf(filename, sizeof(filename), "testsuite/%s", files[i]);
    else
      strlcpy(filename, files[i], sizeof(filename));

    for (j = 0; j < (int)(sizeof(orients) / sizeof(orients[0])); j ++)
    {
      for (k = 0; k < (int)(sizeof(modes) / sizeof(modes[0])); k ++)
      {
	for (m = 0; m < (int)(sizeof(scalings) / sizeof(scalings[0])); m ++)
	{
	  // Stop the test if the system is shutdown (e.g. CTRL+C)
	  if (papplSystemIsShutdown(system))
	    return (false);

	  // Print the job...
	  snprintf(job_name, sizeof(job_name), "%s+%s+%s+%s", files[i], ippEnumString("orientation-requested", orients[j]), modes[k], scalings[m]);

	  request = ippNewRequest(IPP_OP_PRINT_JOB);
	  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri", NULL, uri);
	  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name", NULL, cupsUser());
	  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_MIMETYPE, "document-format", NULL, format);
	  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "job-name", NULL, job_name);

          ippAddInteger(request, IPP_TAG_JOB, IPP_TAG_ENUM, "orientation-requested", orients[j]);
	  ippAddString(request, IPP_TAG_JOB, IPP_TAG_KEYWORD, "print-color-mode", NULL, modes[k]);
	  ippAddString(request, IPP_TAG_JOB, IPP_TAG_KEYWORD, "print-scaling", NULL, scalings[m]);

	  response = cupsDoFileRequest(http, request, "/ipp/print", filename);

	  if (cupsLastError() >= IPP_STATUS_ERROR_BAD_REQUEST)
	  {
	    printf("FAIL (Unable to print %s: %s)\n", job_name, cupsLastErrorString());
	    ippDelete(response);
	    httpClose(http);
	    return (false);
	  }

	  job_id = ippGetInteger(ippFindAttribute(response, "job-id", IPP_TAG_INTEGER), 0);

	  ippDelete(response);

	  printf("%s (job-id=%d)\n%s: ", job_name, job_id, prompt);
	  fflush(stdout);

	  // Poll job status until completed...
	  do
	  {
	    sleep(1);

	    request = ippNewRequest(IPP_OP_GET_JOB_ATTRIBUTES);
	    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri", NULL, uri);
	    ippAddInteger(request, IPP_TAG_OPERATION, IPP_TAG_INTEGER, "job-id", job_id);
	    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name", NULL, cupsUser());

	    response = cupsDoRequest(http, request, "/ipp/print");

	    if (cupsLastError() >= IPP_STATUS_ERROR_BAD_REQUEST)
	    {
	      printf("FAIL (Unable to get job state for '%s': %s)\n", job_name, cupsLastErrorString());
	      httpClose(http);
	      ippDelete(response);
	      return (false);
	    }

	    job_state = (ipp_jstate_t)ippGetInteger(ippFindAttribute(response, "job-state", IPP_TAG_ENUM), 0);

	    ippDelete(response);
	  }
	  while (job_state < IPP_JSTATE_CANCELED);
	}
      }
    }
  }

  httpClose(http);

  return (true);
}
#endif // HAVE_LIBJPEG || HAVE_LIBPNG


//
// 'test_pwg_raster()' - Run PWG Raster tests.
//

static bool				// O - `true` on success, `false` on failure
test_pwg_raster(pappl_system_t *system)	// I - System
{
  bool		ret = false;		// Return value
  http_t	*http = NULL;		// HTTP connection
  char		uri[1024],		// "printer-uri" value
		filename[1024] = "",	// Print file
		job_name[1024];		// "job_name" value
  ipp_t		*request,		// IPP request
		*response,		// IPP response
		*supported = NULL;	// Supported attributes
  ipp_attribute_t *mode_supported;	// "print-color-mode-supported" attribute
  int		i;			// Looping var
  int		job_id;			// "job-id" value
  ipp_jstate_t	job_state;		// "job-state" value
  static const char * const modes[] =	// "print-color-mode" values
  {
    "auto",
    "auto-monochrome",
    "color",
    "monochrome"
  };


  // Connect to system...
  if ((http = connect_to_printer(system, uri, sizeof(uri))) == NULL)
  {
    printf("FAIL (Unable to connect: %s)\n", cupsLastErrorString());
    return (false);
  }

  // Get printer capabilities
  fputs("Get-Printer-Attributes: ", stdout);

  request = ippNewRequest(IPP_OP_GET_PRINTER_ATTRIBUTES);
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri", NULL, "ipp://localhost/ipp/print");
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name", NULL, cupsUser());

  supported = cupsDoRequest(http, request, "/ipp/print");

  if (cupsLastError() != IPP_STATUS_OK)
  {
    printf("FAIL (%s)\n", cupsLastErrorString());
    goto done;
  }

  if ((mode_supported = ippFindAttribute(supported, "print-color-mode-supported", IPP_TAG_KEYWORD)) == NULL)
  {
    puts("FAIL (Missing required 'print-color-mode-supported' attribute in response)");
    goto done;
  }

  // Loop through the supported print-color-mode values...
  for (i = 0; i < (int)(sizeof(modes) / sizeof(modes[0])); i ++)
  {
    // Make raster data for this mode...
    printf("\npwg-raster: %s: ", modes[i]);
    fflush(stdout);

    if (!ippContainsString(mode_supported, modes[i]))
      continue;				// Not supported, skip

    if (!make_raster_file(supported, strstr(modes[i], "monochrome") != NULL, filename, sizeof(filename)))
      break;				// Error

    // Print the file...
    snprintf(job_name, sizeof(job_name), "pwg-raster-%s", modes[i]);

    request = ippNewRequest(IPP_OP_PRINT_JOB);
    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri", NULL, uri);
    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name", NULL, cupsUser());
    ippAddString(request, IPP_TAG_OPERATION, IPP_CONST_TAG(IPP_TAG_MIMETYPE), "document-format", NULL, "image/pwg-raster");
    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "job-name", NULL, job_name);

    ippAddString(request, IPP_TAG_JOB, IPP_TAG_KEYWORD, "print-color-mode", NULL, modes[i]);

    response = cupsDoFileRequest(http, request, "/ipp/print", filename);

    if (cupsLastError() >= IPP_STATUS_ERROR_BAD_REQUEST)
    {
      printf("FAIL (Unable to print %s: %s)\n", job_name, cupsLastErrorString());
      goto done;
    }

    job_id = ippGetInteger(ippFindAttribute(response, "job-id", IPP_TAG_INTEGER), 0);

    ippDelete(response);

    printf("job-id=%d ", job_id);
    fflush(stdout);

    // Poll job status until completed...
    do
    {
      sleep(1);

      request = ippNewRequest(IPP_OP_GET_JOB_ATTRIBUTES);
      ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri", NULL, uri);
      ippAddInteger(request, IPP_TAG_OPERATION, IPP_TAG_INTEGER, "job-id", job_id);
      ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name", NULL, cupsUser());

      response = cupsDoRequest(http, request, "/ipp/print");

      if (cupsLastError() >= IPP_STATUS_ERROR_BAD_REQUEST)
      {
	printf("FAIL (Unable to get job state for '%s': %s)\n", job_name, cupsLastErrorString());
        goto done;
      }

      job_state = (ipp_jstate_t)ippGetInteger(ippFindAttribute(response, "job-state", IPP_TAG_ENUM), 0);

      ippDelete(response);
    }
    while (job_state < IPP_JSTATE_CANCELED);

    // Cleanup...
    unlink(filename);
  }

  // If we complete the loop without errors, it is a successful run...
  ret = true;

  done:

  if (filename[0])
    unlink(filename);

  httpClose(http);
  ippDelete(supported);

  return (ret);
}


//
// 'usage()' - Show usage.
//

static int				// O - Exit status
usage(int status)			// I - Exit status
{
  puts("Usage: testpappl [OPTIONS] [\"SERVER NAME\"]");
  puts("Options:");
  puts("  --help               Show help");
  puts("  --list               List devices");
  puts("  --list-TYPE          Lists devices of TYPE (dns-sd, local, network, usb)");
  puts("  --version            Show version");
  puts("  -1                   Single queue");
  puts("  -A PAM-SERVICE       Enable authentication using PAM service");
  puts("  -c                   Do a clean run (no loading of state)");
  puts("  -d SPOOL-DIRECTORY   Set the spool directory");
  puts("  -l LOG-FILE          Set the log file");
  puts("  -L LOG-LEVEL         Set the log level (fatal, error, warn, info, debug)");
  puts("  -m DRIVER-NAME       Add a printer with the named driver");
  puts("  -o OUTPUT-DIRECTORY  Set the output directory (default '.')");
  puts("  -p PORT              Set the listen port (default auto)");
  puts("  -t TEST-NAME         Run the named test (see below)");
  puts("  -T                   Enable TLS-only mode");
  puts("  -U                   Enable USB printer gadget");
  puts("");
  puts("Tests:");
  puts("  all                  All of the following tests");
  puts("  client               Simulated client tests");
  puts("  jpeg                 JPEG image tests");
  puts("  png                  PNG image tests");
  puts("  pwg-raster           PWG Raster tests");

  return (status);
}
