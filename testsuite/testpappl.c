//
// Main test suite file for the Printer Application Framework
//
// Copyright © 2020-2026 by Michael R Sweet.
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
//   --get-id DEVICE-URI          Show IEEE-1284 device ID for URI
//   --get-status DEVICE-URI      Show printer status bits for URI
//   --get-supplies DEVICE-URI    Show supplies for URI
//   --help                       Show help
//   --list[-TYPE]                List devices (dns-sd, local, network, usb)
//   --no-tls                     Don't support TLS
//   --ps-query DEVICE-URI        Do a PostScript query to get the product string
//   --usb-ethernet               Enable the USB ethernet gadget
//   --usb-product-id PRODUCT-ID  Set the USB product ID number
//   --usb-readonly DISK-IMAGE    Enable the USB storage gadget in read-only mode
//   --usb-removable DISK-IMAGE   Enable the USB storage gadget in removable mode
//   --usb-serial                 Enable the USB serial gadget
//   --usb-storage DISK-IMAGE     Enable the USB storage gadget in read-write mode
//   --usb-vendor-id VENDOR-ID    Set the USB vendor ID number
//   --version                    Show version
//   -1                           Single queue
//   -A PAM-SERVICE               Enable authentication using PAM service
//   -c                           Do a clean run (no loading of state)
//   -d SPOOL-DIRECTORY           Set the spool directory
//   -l LOG-FILE                  Set the log file
//   -L LOG-LEVEL                 Set the log level (fatal, error, warn, info, debug)
//   -m DRIVER-NAME               Add a printer with the named driver
//   -p PORT                      Set the listen port (default auto)
//   -R                           Enable raw sockets
//   -t TEST-NAME                 Run the named test (see below)
//   -T                           Enable TLS-only mode
//   -U                           Enable USB printer gadget
//
// Tests:
//
//   all                  All of the following tests
//   api                  API tests
//   client               Simulated client tests
//   client-CLxRQ         Simulated CL clients and RQ requests tests
//   client-max           Simulated max clients and requests tests
//   idle-shutdown        Test idle shutdown functionality
//   infra                Test shared infrastructure extensions support
//   jpeg                 JPEG image tests
//   pdf                  PDF file tests
//   png                  PNG image tests
//   pwg-raster           PWG Raster tests
//

#include <pappl/system-private.h>
#include <cups/dir.h>
#include "testpappl.h"
#include "test.h"
#include <stdlib.h>
#include <limits.h>

#if _WIN32
#  define PATH_MAX	    MAX_PATH
#  define realpath(rel,abs) win32_realpath((rel), (abs))
static inline char *win32_realpath(const char *relpath, char *abspath)
{
  // Win32 version of POSIX realpath that returns proper forward slash directory delimiters and handles
  // DOS drive letters...
  char	temp[MAX_PATH],			// Temporary path buffer
	*tempptr = temp,		// Pointer into temp buffer
	*absptr = abspath,		// Pointer into abspath buffer
	*absend = abspath + MAX_PATH - 1;
					// End of abspath buffer


  // Get the full path with drive letter...
  if (!_fullpath(temp, relpath, sizeof(temp)))
    return (NULL);

  if (isalpha(*tempptr & 255) && tempptr[1] == ':')
  {
    if (*tempptr == (_getdrive() + '@'))
    {
      // Same drive so just skip the drive letter...
      tempptr += 2;
    }
    else
    {
      // Otherwise encode as "/L:"
      *absptr++ = '/';
      *absptr++ = *tempptr++;
      *absptr++ = *tempptr++;
    }
  }

  // Re-encode path using conventional forward slashes...
  while (*tempptr && absptr < absend)
  {
    if (*tempptr == '\\')
      *absptr++ = '/';
    else
      *absptr++ = *tempptr;

    tempptr ++;
  }

  *absptr = '\0';

  return (abspath);
}
#endif // _WIN32


//
// Constants...
//

#define _PAPPL_DEFAULT_CLIENTS	100
#define _PAPPL_DEFAULT_REQUESTS	1000
#define _PAPPL_HEARTBEAT_INTERVAL 30
#define _PAPPL_MAX_CLIENTS	1000
#define _PAPPL_MAX_REQUESTS	100000
#define _PAPPL_MAX_TIMER_COUNT	32
#define _PAPPL_TIMER_INTERVAL	5


//
// Macros...
//

#define _papplTestBegin(...)	heartbeat_update(), testBegin(__VA_ARGS__)
#define _papplTestEnd(b)	heartbeat_update(), testEnd(b)
#define _papplTestEndMessage(...) heartbeat_update(), testEndMessage(__VA_ARGS__)
#define _papplTestProgress()	heartbeat_update(), testProgress()


//
// Local globals...
//

static bool		all_tests_done = false;
					// All tests are done?
static char		current_ssid[32] = "";
					// Current wireless network
static size_t		event_count = 0;// Number of events that have been delivered
static pappl_event_t	event_mask = PAPPL_EVENT_NONE;
					// Events that have been delivered
static cups_mutex_t	heartbeat_mutex = CUPS_MUTEX_INITIALIZER;
					// Mutex for heartbeat
static size_t		heartbeat_count = 0;
					// Heartbeat counter
static int		output_count = 0;
					// Number of expected output files
static char		output_device_uri[1024] = "";
					// Device URI for output directory
static char		output_directory[1024] = "";
					// Output directory


//
// Local types...
//

typedef struct _pappl_testclient_s	// Client test data
{
  pappl_system_t	*system;	// System
  size_t		num_children,	// Number of child threads
			num_requests;	// Requests per child
  cups_mutex_t		mutex;		// Access mutex
  size_t		completed_count,// Number of completed children
			request_count;	// Number of requests
  cups_array_t		*errors;	// Errors, if any
  struct timeval	start, end;	// Start/end times
} _pappl_testclient_t;

typedef struct _pappl_testdata_s	// Test data
{
  cups_array_t		*names;		// Tests to run
  pappl_system_t	*system;	// System
  const char		*outdirname;	// Output directory
  bool			waitsystem;	// Wait for system to start?
  time_t		timer_start;	// Start time
  int			timer_count;	// Number of times the timer callback has been called
  time_t		timer_times[1000];
					// Timestamps for each timer callback
} _pappl_testdata_t;

typedef struct _pappl_testprinter_s	// Printer test data
{
  bool			pass;		// Pass/fail
  int			count;		// Number of printers
} _pappl_testprinter_t;


//
// Local functions...
//

static http_t	*connect_to_printer(pappl_system_t *system, bool remote, char *uri, size_t urisize);
static void	device_error_cb(void *err_data, const char *message);
static bool	device_list_cb(const char *device_info, const char *device_uri, const char *device_id, void *data);
static int	do_ps_query(const char *device_uri);
static void	event_cb(pappl_system_t *system, pappl_printer_t *printer, pappl_job_t *job, pappl_event_t event, void *data);
static bool	heartbeat_cb(pappl_system_t *system, void *data);
static void	heartbeat_update(void);
static bool	infra_deregister_cb(pappl_client_t *client, const char *device_uuid, pappl_printer_t *printer, void *data);
static pappl_printer_t *infra_register_cb(pappl_client_t *client, const char *device_uuid, pappl_printer_t *requested_printer, void *data);
static const char *make_raster_file(ipp_t *response, bool grayscale, char *tempname, size_t tempsize);
static void	*run_tests(_pappl_testdata_t *testdata);
static char	*strcopy_cb(const char *s, void *data);
static void	strfree_cb(char *s, void *data);
static bool	test_api(pappl_system_t *system);
static bool	test_api_printer(pappl_printer_t *printer);
static bool	test_api_printer_cb(pappl_printer_t *printer, _pappl_testprinter_t *tp);
static bool	test_client(pappl_system_t *system);
static void	*test_client_child(_pappl_testclient_t *data);
static bool	test_client_max(pappl_system_t *system, const char *name);
static bool	test_idle_shutdown(pappl_system_t *system);
static bool	test_infra(pappl_system_t *system);
static size_t	test_network_get_cb(pappl_system_t *system, void *data, size_t max_networks, pappl_network_t *networks);
static bool	test_network_set_cb(pappl_system_t *system, void *data, size_t num_networks, pappl_network_t *networks);
static bool	test_print_files(pappl_system_t *system, const char *prompt, const char *format, size_t num_files, const char * const *files);
static bool	test_pwg_raster(pappl_system_t *system);
static bool	test_wifi_join_cb(pappl_system_t *system, void *data, const char *ssid, const char *psk);
static size_t	test_wifi_list_cb(pappl_system_t *system, void *data, cups_dest_t **ssids);
static pappl_wifi_t *test_wifi_status_cb(pappl_system_t *system, void *data, pappl_wifi_t *wifi_data);
static bool	timer_cb(pappl_system_t *system, _pappl_testdata_t *data);
static int	usage(int status);


//
// 'main()' - Main entry for test suite.
//

int					// O - Exit status
main(int  argc,				// I - Number of command-line arguments
     char *argv[])			// I - Command-line arguments
{
  int			i,		// Looping var
			status = 0;	// Exit status
  const char		*opt,		// Current option
			*name = NULL,	// System name, if any
			*spool = NULL,	// Spool directory, if any
			*outdir = "testpappl.output",
					// Output directory
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
  char			outdirname[PATH_MAX];
					// Output directory name
  pappl_soptions_t	soptions = PAPPL_SOPTIONS_MULTI_QUEUE | PAPPL_SOPTIONS_WEB_INTERFACE | PAPPL_SOPTIONS_WEB_LOG | PAPPL_SOPTIONS_WEB_NETWORK | PAPPL_SOPTIONS_WEB_SECURITY | PAPPL_SOPTIONS_WEB_TLS | PAPPL_SOPTIONS_MULTI_DOCUMENT_JOBS | PAPPL_SOPTIONS_INFRA_PROXY | PAPPL_SOPTIONS_INFRA_SERVER;
					// System options
  pappl_system_t	*system;	// System
  pappl_printer_t	*printer;	// Printer
  _pappl_testdata_t	testdata;	// Test data
  cups_thread_t		testid = 0;	// Test thread ID
  pappl_uoptions_t	usb_options = PAPPL_UOPTIONS_NONE;
					// USB gadget options
  unsigned		usb_product_id = 0x8011,
					// USB product ID
			usb_vendor_id = 0x1209;
					// USB vendor ID
  const char		*usb_storagefile = NULL;
					// USB storage file, if any
  static pappl_contact_t contact =	// Contact information
  {
    "Michael R Sweet",
    "msweet@example.org",
    "+1-705-555-1212"
  };
  static pappl_version_t versions[1] =	// Software versions
  {
    { "Test System", "", "2.0 build 42", { 2, 0, 0, 42 } }
  };


  // Don't buffer stdout/stderr...
  setbuf(stdout, NULL);
  setbuf(stderr, NULL);

#if _WIN32
  // Windows builds put the executables under the "vcnet/Platform/Configuration" directory...
  if (!access("../../../testsuite", 0))
    _chdir("../../../testsuite");

  freopen("testpappl-stderr.log", "w", stderr);
#endif // _WIN32

  // Parse command-line options...
  models               = cupsArrayNew(/*cb*/NULL, /*cbdata*/NULL, /*hashcb*/NULL, /*hashsize*/0, /*copy_cb*/NULL, /*free_cb*/NULL);
  testdata.names       = cupsArrayNew(/*cb*/NULL, /*cbdata*/NULL, /*hashcb*/NULL, /*hashsize*/0, (cups_acopy_cb_t)strcopy_cb, (cups_afree_cb_t)strfree_cb);
  testdata.timer_count = 0;
  testdata.timer_start = time(NULL);

  for (i = 1; i < argc; i ++)
  {
    if (!strcmp(argv[i], "--get-id"))
    {
      pappl_device_t	*device;		// Device
      char		device_id[1024];	// Device ID string

      i ++;
      if (i >= argc)
      {
        fputs("testpappl: Missing device URI after '--get-id'.\n", stderr);
        status = 1;
        goto done;
      }

      if ((device = papplDeviceOpen(argv[i], NULL, NULL, NULL)) == NULL)
      {
        status = 1;
        goto done;
      }

      if (papplDeviceGetID(device, device_id, sizeof(device_id)))
        puts(device_id);
      else
        fprintf(stderr, "testpappl: No device ID for '%s'.\n", argv[i]);

      papplDeviceClose(device);
      goto done;
    }
    else if (!strcmp(argv[i], "--get-status"))
    {
      pappl_device_t	*device;	// Device
      pappl_preason_t	reasons;	// State reason bits

      i ++;
      if (i >= argc)
      {
        fputs("testpappl: Missing device URI after '--get-status'.\n", stderr);
        status = 1;
        goto done;
      }

      if ((device = papplDeviceOpen(argv[i], NULL, NULL, NULL)) == NULL)
      {
        status = 1;
        goto done;
      }

      reasons = papplDeviceGetStatus(device);
      papplDeviceClose(device);

      if (!reasons)
        puts("none");
      if (reasons & PAPPL_PREASON_OTHER)
        puts("other");
      if (reasons & PAPPL_PREASON_COVER_OPEN)
        puts("cover-open");
      if (reasons & PAPPL_PREASON_INPUT_TRAY_MISSING)
        puts("input-tray-missing");
      if (reasons & PAPPL_PREASON_MARKER_SUPPLY_EMPTY)
        puts("marker-supply-empty");
      if (reasons & PAPPL_PREASON_MARKER_SUPPLY_LOW)
        puts("marker-supply-low");
      if (reasons & PAPPL_PREASON_MARKER_WASTE_ALMOST_FULL)
        puts("marker-waste-almost-full");
      if (reasons & PAPPL_PREASON_MARKER_WASTE_FULL)
        puts("marker-waste-full");
      if (reasons & PAPPL_PREASON_MEDIA_EMPTY)
        puts("media-empty");
      if (reasons & PAPPL_PREASON_MEDIA_JAM)
        puts("media-jam");
      if (reasons & PAPPL_PREASON_MEDIA_LOW)
        puts("media-low");
      if (reasons & PAPPL_PREASON_MEDIA_NEEDED)
        puts("media-needed");
      if (reasons & PAPPL_PREASON_OFFLINE)
        puts("offline");
      if (reasons & PAPPL_PREASON_SPOOL_AREA_FULL)
        puts("spool-area-full");
      if (reasons & PAPPL_PREASON_TONER_EMPTY)
        puts("toner-empty");
      if (reasons & PAPPL_PREASON_TONER_LOW)
        puts("toner-low");
      if (reasons & PAPPL_PREASON_DOOR_OPEN)
        puts("door-open");
      if (reasons & PAPPL_PREASON_IDENTIFY_PRINTER_REQUESTED)
        puts("identify-printer-requested");

      goto done;
    }
    else if (!strcmp(argv[i], "--get-supplies"))
    {
      pappl_device_t	*device;	// Device
      size_t		j,		// Looping var
			num_supplies;	// Number of supplies
      pappl_supply_t	supplies[32];	// Supplies
      static const char * const supply_colors[] =
      {					// Supply colors
	"no-color",
	"black",
	"cyan",
	"gray",
	"green",
	"light-cyan",
	"light-gray",
	"light-magenta",
	"magenta",
	"orange",
	"violet",
	"yellow",
	"multi-color"
      };
      static const char * const supply_types[] =
      {					// Supply types
	"bandingSupply",
	"bindingSupply",
	"cleanerUnit",
	"coronaWire",
	"covers",
	"developer",
	"fuserCleaningPad",
	"fuserOilWick",
	"fuserOil",
	"fuserOiler",
	"fuser",
	"inkCartridge",
	"inkRibbon",
	"ink",
	"inserts",
	"opc",
	"paperWrap",
	"ribbonWax",
	"solidWax",
	"staples",
	"stitchingWire",
	"tonerCartridge",
	"toner",
	"transferUnit",
	"wasteInk",
	"wasteToner",
	"wasteWater",
	"wasteWax",
	"water",
	"glueWaterAdditive",
	"wastePaper",
	"shrinkWrap",
	"other",
	"unknown"
      };

      i ++;
      if (i >= argc)
      {
        fputs("testpappl: Missing device URI after '--get-supplies'.\n", stderr);
        status = 1;
        goto done;
      }

      if ((device = papplDeviceOpen(argv[i], NULL, NULL, NULL)) == NULL)
      {
        status = 1;
        goto done;
      }

      if ((num_supplies = papplDeviceGetSupplies(device, sizeof(supplies) / sizeof(supplies[0]), supplies)) > 0)
      {
        for (j = 0; j < num_supplies; j ++)
        {
          if (supplies[j].color != PAPPL_SUPPLY_COLOR_NO_COLOR)
	    printf("%40s: %d%% (%s, %s)\n", supplies[j].description, supplies[j].level, supply_types[supplies[j].type], supply_colors[supplies[j].color]);
	  else
	    printf("%40s: %d%% (%s)\n", supplies[j].description, supplies[j].level, supply_types[supplies[j].type]);
	}
      }
      else
      {
        fprintf(stderr, "testpappl: No supplies for '%s'.\n", argv[i]);
      }

      papplDeviceClose(device);
      goto done;
    }
    else if (!strcmp(argv[i], "--help"))
    {
      status = usage(0);
      goto done;
    }
    else if (!strcmp(argv[i], "--list"))
    {
      papplDeviceList(PAPPL_DEVTYPE_ALL, device_list_cb, NULL, device_error_cb, NULL);
      goto done;
    }
    else if (!strcmp(argv[i], "--list-dns-sd"))
    {
      papplDeviceList(PAPPL_DEVTYPE_DNS_SD, device_list_cb, NULL, device_error_cb, NULL);
      goto done;
    }
    else if (!strcmp(argv[i], "--list-ipp"))
    {
      papplDeviceList(PAPPL_DEVTYPE_IPP, device_list_cb, NULL, device_error_cb, NULL);
      goto done;
    }
    else if (!strcmp(argv[i], "--list-local"))
    {
      papplDeviceList(PAPPL_DEVTYPE_LOCAL, device_list_cb, NULL, device_error_cb, NULL);
      goto done;
    }
    else if (!strcmp(argv[i], "--list-network"))
    {
      papplDeviceList(PAPPL_DEVTYPE_NETWORK, device_list_cb, NULL, device_error_cb, NULL);
      goto done;
    }
    else if (!strcmp(argv[i], "--list-usb"))
    {
      papplDeviceList(PAPPL_DEVTYPE_USB, device_list_cb, NULL, device_error_cb, NULL);
      goto done;
    }
    else if (!strcmp(argv[i], "--no-tls"))
    {
      soptions |= PAPPL_SOPTIONS_NO_TLS;
    }
    else if (!strcmp(argv[i], "--ps-query"))
    {
      i ++;
      if (i < argc)
      {
        status = do_ps_query(argv[i]);
      }
      else
      {
        puts("testpappl: Missing device URI after '--ps-query'.");
        status = usage(1);
      }

      goto done;
    }
    else if (!strcmp(argv[i], "--usb-ethernet"))
    {
      usb_options |= PAPPL_UOPTIONS_ETHERNET;
    }
    else if (!strcmp(argv[i], "--usb-product-id"))
    {
      i ++;
      if (i < argc)
      {
        usb_product_id = (unsigned)strtol(argv[i], NULL, 0);
      }
      else
      {
        puts("testpappl: Missing product ID after '--usb-product-id'.");
        status = usage(1);
	goto done;
      }
    }
    else if (!strcmp(argv[i], "--usb-readonly"))
    {
      usb_options |= PAPPL_UOPTIONS_STORAGE | PAPPL_UOPTIONS_STORAGE_READONLY;

      i ++;
      if (i < argc)
      {
        usb_storagefile = argv[i];
      }
      else
      {
        puts("testpappl: Missing storage file after '--usb-readonly'.");
        status = usage(1);
	goto done;
      }
    }
    else if (!strcmp(argv[i], "--usb-removable"))
    {
      usb_options |= PAPPL_UOPTIONS_STORAGE | PAPPL_UOPTIONS_STORAGE_REMOVABLE;

      i ++;
      if (i < argc)
      {
        usb_storagefile = argv[i];
      }
      else
      {
        puts("testpappl: Missing storage file after '--usb-removable'.");
        status = usage(1);
	goto done;
      }
    }
    else if (!strcmp(argv[i], "--usb-serial"))
    {
      usb_options |= PAPPL_UOPTIONS_SERIAL;
    }
    else if (!strcmp(argv[i], "--usb-storage"))
    {
      usb_options |= PAPPL_UOPTIONS_STORAGE;

      i ++;
      if (i < argc)
      {
        usb_storagefile = argv[i];
      }
      else
      {
        puts("testpappl: Missing storage file after '--usb-storage'.");
        status = usage(1);
	goto done;
      }
    }
    else if (!strcmp(argv[i], "--usb-vendor-id"))
    {
      i ++;
      if (i < argc)
      {
        usb_vendor_id = (unsigned)strtol(argv[i], NULL, 0);
      }
      else
      {
        puts("testpappl: Missing vendor ID after '--usb-vendor-id'.");
        status = usage(1);
	goto done;
      }
    }
    else if (!strcmp(argv[i], "--version"))
    {
      puts(PAPPL_VERSION);
      goto done;
    }
    else if (!strncmp(argv[i], "--", 2))
    {
      printf("testpappl: Unknown option '%s'.\n", argv[i]);
      status = usage(1);
      goto done;
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
		status = usage(1);
		goto done;
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
		status = usage(1);
		goto done;
	      }
	      spool = argv[i];
              break;
          case 'l' : // -l LOG-FILE
              i ++;
              if (i >= argc)
              {
                puts("testpappl: Expected log file after '-l'.");
		status = usage(1);
		goto done;
	      }
	      log = argv[i];
              break;
          case 'L' : // -L LOG-LEVEL
              i ++;
              if (i >= argc)
              {
                puts("testpappl: Expected log level after '-L'.");
		status = usage(1);
		goto done;
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
		status = usage(1);
		goto done;
	      }
              break;
	  case 'm' : // -m DRIVER-NAME
	      i ++;
              if (i >= argc)
              {
                puts("testpappl: Expected driver name after '-m'.");
		status = usage(1);
		goto done;
	      }
	      cupsArrayAdd(models, argv[i]);
              break;
	  case 'o' : // -o OUTPUT-DIRECTORY
	      i ++;
	      if (i >= argc)
	      {
                puts("testpappl: Expected output directory after '-o'.");
		status = usage(1);
		goto done;
	      }
	      outdir = argv[i];
	      break;
          case 'p' : // -p PORT-NUMBER
              i ++;
              if (i >= argc || atoi(argv[i]) <= 0 || atoi(argv[i]) > 32767)
              {
                puts("testpappl: Expected port number after '-p'.");
		status = usage(1);
		goto done;
	      }
	      port = atoi(argv[i]);
              break;
	  case 'R' : // -R (enable raw sockets)
	      soptions |= PAPPL_SOPTIONS_RAW_SOCKET;
	      break;
	  case 't' : // -t TEST
	      i ++;
	      if (i >= argc)
	      {
                puts("testpappl: Expected test name after '-t'.");
		status = usage(1);
		goto done;
	      }

	      if (!strcmp(argv[i], "all") || !strncmp(argv[i], "all,", 4))
	      {
	        // Add all tests
		cupsArrayAdd(testdata.names, "api");
		cupsArrayAdd(testdata.names, "client");
		cupsArrayAdd(testdata.names, "infra");
		cupsArrayAdd(testdata.names, "jpeg");
		cupsArrayAdd(testdata.names, "pdf");
		cupsArrayAdd(testdata.names, "png");
		cupsArrayAdd(testdata.names, "pwg-raster");
		cupsArrayAdd(testdata.names, "client-10x1000");
	      }

	      if (!strncmp(argv[i], "all,", 4))
	      {
	        // Add additional comma-delimited tests
	        cupsArrayAddStrings(testdata.names, argv[i] + 4, ',');
	      }
	      else if (strcmp(argv[i], "all"))
	      {
	        // Add comma-delimited tests
	        cupsArrayAddStrings(testdata.names, argv[i], ',');
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
	      status = usage(1);
	      goto done;
        }
      }
    }
    else if (name)
    {
      printf("testpappl: Unexpected argument '%s'.\n", argv[i]);
      status = usage(1);
      goto done;
    }
    else
    {
      // "SERVER NAME"
      name = argv[i];
    }
  }

  // Clean the log and output directory if necessary
  if (clean && log && strcmp(log, "-") && strcmp(log, "syslog"))
    unlink(log);

  if (clean && outdir && strcmp(outdir, "."))
  {
    // Remove all PWG raster output files from output directory...
    cups_dir_t		*dir;		// Directory
    cups_dentry_t	*dent;		// Directory entry
    char		*ext,		// Extension on filename
			filename[1024];	// Filename

    if ((dir = cupsDirOpen(outdir)) != NULL)
    {
      while ((dent = cupsDirRead(dir)) != NULL)
      {
        // Only remove PWG raster files...
        if ((ext = strrchr(dent->filename, '.')) == NULL || strcmp(ext, ".pwg"))
          continue;

        // Remove this file...
        snprintf(filename, sizeof(filename), "%s/%s", outdir, dent->filename);
        unlink(filename);
      }

      cupsDirClose(dir);
    }
  }

  cupsCopyString(output_directory, outdir, sizeof(output_directory));

  // Initialize the system and any printers...
  system = papplSystemCreate(soptions, name ? name : "Test System", port, "_print,_universal", spool, log, level, auth, tls_only);
  papplSystemAddHostAlias(system, "bogus.example.com");
  papplSystemAddListeners(system, NULL);
  papplSystemAddTimerCallback(system, /*start*/0, _PAPPL_HEARTBEAT_INTERVAL, (pappl_timer_cb_t)heartbeat_cb, /*cb_data*/NULL);
  papplSystemAddTimerCallback(system, /*start*/0, _PAPPL_TIMER_INTERVAL, (pappl_timer_cb_t)timer_cb, /*cb_data*/&testdata);
  papplSystemSetEventCallback(system, event_cb, (void *)"testpappl");
  papplSystemSetPrinterDrivers(system, (int)(sizeof(pwg_drivers) / sizeof(pwg_drivers[0])), pwg_drivers, pwg_autoadd, /* create_cb */NULL, pwg_callback, "testpappl");
  papplSystemSetRegisterCallbacks(system, infra_register_cb, infra_deregister_cb, /*cbdata*/NULL);
  papplSystemSetWiFiCallbacks(system, test_wifi_join_cb, test_wifi_list_cb, test_wifi_status_cb, (void *)"testpappl");
  papplSystemAddLink(system, "Configuration", "/config", true);
  papplSystemSetFooterHTML(system,
                           "Copyright &copy; 2020-2025 by Michael R Sweet. "
                           "Provided under the terms of the <a href=\"https://www.apache.org/licenses/LICENSE-2.0\">Apache License 2.0</a>.");
  papplSystemSetNetworkCallbacks(system, test_network_get_cb, test_network_set_cb, (void *)"testnetwork");
  papplSystemSetSaveCallback(system, (pappl_save_cb_t)papplSystemSaveState, (void *)"testpappl.state");
  papplSystemSetVersions(system, (int)(sizeof(versions) / sizeof(versions[0])), versions);
  papplSystemAddStringsData(system, "/en.strings", "en", "\"/\" = \"This is a localized header for the system home page.\";\n\"/network\" = \"This is a localized header for the network configuration page.\";\n\"/printing\" = \"This is a localized header for all printing defaults pages.\";\n\"/Label_Printer/printing\" = \"This is a localized header for the label printer defaults page.\";\n");

  mkdir(outdir, 0777);

  httpAssembleURIf(HTTP_URI_CODING_ALL, output_device_uri, sizeof(output_device_uri), "file", NULL, NULL, 0, "%s?ext=pwg", realpath(outdir, outdirname));

  if (clean || !papplSystemLoadState(system, "testpappl.state"))
  {
    papplSystemSetContact(system, &contact);
    papplSystemSetDNSSDName(system, name ? name : "Test System");
    papplSystemSetGeoLocation(system, "geo:46.4707,-80.9961");
    papplSystemSetLocation(system, "Test Lab 42");
    papplSystemSetOrganization(system, "Lakeside Robotics");

    if (cupsArrayGetCount(models))
    {
      for (model = (const char *)cupsArrayGetFirst(models), i = 1; model; model = (const char *)cupsArrayGetNext(models), i ++)
      {
        char	pname[128];		// Printer name

        if (cupsArrayGetCount(models) == 1)
	  snprintf(pname, sizeof(pname), "%s", name ? name : "Test Printer");
        else
	  snprintf(pname, sizeof(pname), "%s %d", name ? name : "Test Printer", i);

	printer = papplPrinterCreate(system, /* printer_id */0, pname, model, "MFG:PWG;MDL:Test Printer;", output_device_uri);
	papplPrinterSetContact(printer, &contact);
	papplPrinterSetDNSSDName(printer, pname);
	papplPrinterSetGeoLocation(printer, "geo:46.4707,-80.9961");
	papplPrinterSetLocation(printer, "Test Lab 42");
	papplPrinterSetOrganization(printer, "Lakeside Robotics");
      }
    }
    else
    {
      printer = papplPrinterCreate(system, /* printer_id */0, "Office Printer", "pwg_common-300dpi-600dpi-srgb_8", "MFG:PWG;MDL:Office Printer;", output_device_uri);
      papplPrinterSetContact(printer, &contact);
      papplPrinterSetDNSSDName(printer, "Office Printer");
      papplPrinterSetGeoLocation(printer, "geo:46.4707,-80.9961");
      papplPrinterSetLocation(printer, "Test Lab 42");
      papplPrinterSetOrganization(printer, "Lakeside Robotics");
      papplPrinterSetMaxPreservedJobs(printer, 3);

      if (soptions & PAPPL_SOPTIONS_USB_PRINTER)
        papplPrinterSetUSB(printer, usb_vendor_id, usb_product_id, usb_options, usb_storagefile, /*usb_cb*/NULL, /*usb_data*/NULL);

      if (soptions & PAPPL_SOPTIONS_MULTI_QUEUE)
      {
	printer = papplPrinterCreate(system, /* printer_id */0, "Label Printer", "pwg_4inch-203dpi-black_1", "MFG:PWG;MDL:Label Printer;", output_device_uri);
	papplPrinterSetContact(printer, &contact);
	papplPrinterSetDNSSDName(printer, "Label Printer");
	// Not setting geo-location for label printer to ensure that DNS-SD works without a LOC record...
	papplPrinterSetLocation(printer, "Test Lab 42");
	papplPrinterSetOrganization(printer, "Lakeside Robotics");
      }
    }
  }

  // Run any test(s)...
  if (cupsArrayGetCount(testdata.names))
  {
    testdata.outdirname = outdirname;
    testdata.system     = system;

    if (cupsArrayGetCount(testdata.names) == 1 && !strcmp((char *)cupsArrayGetFirst(testdata.names), "api"))
    {
      // Running API test alone does not start system...
      testdata.waitsystem = false;
      status = run_tests(&testdata) != NULL;
      goto done;
    }

    testdata.waitsystem = true;

    if ((testid = cupsThreadCreate((void *(*)(void *))run_tests, &testdata)) == CUPS_THREAD_INVALID)
    {
      perror("Unable to start testing thread");
      status = 1;
      goto done;
    }
  }

  // Run the system...
  papplSystemRun(system);

  if (testid)
    status = cupsThreadWait(testid) != NULL;

  done:

  // Free memory before exiting to make ASAN happy on Linux...
  cupsArrayDelete(testdata.names);
  cupsArrayDelete(models);

  return (status);
}


//
// 'connect_to_printer()' - Connect to the system and return the printer URI.
//

static http_t *				// O - HTTP connection
connect_to_printer(
    pappl_system_t *system,		// I - System
    bool           remote,		// I - Remote connection
    char           *uri,		// I - URI buffer
    size_t         urisize)		// I - Size of URI buffer
{
  char	host[1024];			// Hostname


  if (remote)
    papplSystemGetHostName(system, host, sizeof(host));
  else
    cupsCopyString(host, "localhost", sizeof(host));

  httpAssembleURI(HTTP_URI_CODING_ALL, uri, urisize, "ipp", NULL, host, papplSystemGetHostPort(system), "/ipp/print");

  return (httpConnect(host, papplSystemGetHostPort(system), NULL, AF_UNSPEC, HTTP_ENCRYPTION_IF_REQUESTED, 1, 30000, NULL));
}


//
// 'device_error_cb()' - Show a device error message.
//

static void
device_error_cb(void       *err_data,	// I - Callback data (unused)
                const char *message)	// I - Error message
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
// 'do_ps_query()' - Try doing a simple PostScript device query.
//

static int				// O - Exit status
do_ps_query(const char *device_uri)	// I - Device URI
{
  pappl_device_t	*device;	// Connection to device
  char			buffer[8192];	// Read buffer
  ssize_t		bytes;		// Bytes read


  if ((device = papplDeviceOpen(device_uri, NULL, device_error_cb, NULL)) == NULL)
    return (1);

  papplDevicePuts(device, "\033%-12345X%!\nproduct print\n");
  papplDeviceFlush(device);

  if ((bytes = papplDeviceRead(device, buffer, sizeof(buffer) - 1)) > 0)
  {
    buffer[bytes] = '\0';
    puts(buffer);
  }
  else
  {
    puts("<<no response>>");
  }

  papplDeviceClose(device);
  return (0);
}


//
// 'event_cb()' - Accumulate events.
//

void
event_cb(pappl_system_t  *system,	// I - System
         pappl_printer_t *printer,	// I - Printer, if any
         pappl_job_t     *job,		// I - Job, if any
         pappl_event_t   event,		// I - Event
         void            *data)		// I - Callback data
{
//  fprintf(stderr, "event_cb: system=%p, printer=%p, job=%p, event=%08x, data=%p\n", system, printer, job, event, data);

  if (!data || strcmp((char *)data, "testpappl"))
  {
    fputs("testpappl: Bad event callback data.\n", stderr);
    exit(1);
  }

  if (!system)
  {
    fputs("testpappl: Bad system for event callback.\n", stderr);
    exit(1);
  }

  if ((event & PAPPL_EVENT_JOB_ALL) && !job)
  {
    fputs("testpappl: Missing job for event callback.\n", stderr);
    exit(1);
  }

  if ((event & PAPPL_EVENT_PRINTER_ALL) && !printer)
  {
    fputs("testpappl: Missing printer for event callback.\n", stderr);
    exit(1);
  }

  event_count ++;
  event_mask |= event;
}


//
// 'heartbeat_cb()' - Verify that the tests are still making progress.
//

static bool				// O - `true` to continue, `false` to stop
heartbeat_cb(pappl_system_t *system,	// I - System (unused)
             void           *data)	// I - Callback data (unused)
{
  static size_t	heartbeat_prev = 0;	// Previous heartbeat count


  (void)system;
  (void)data;

  cupsMutexLock(&heartbeat_mutex);

  // Abort if the heartbeat count hasn't incremented...
  _PAPPL_DEBUG("heartbeat_cb: heartbeat_count=%lu, heartbeat_prev=%lu\n", (unsigned long)heartbeat_count, (unsigned long)heartbeat_prev);

  if (heartbeat_count == heartbeat_prev)
    abort();

  heartbeat_prev = heartbeat_count;

  cupsMutexUnlock(&heartbeat_mutex);

  return (true);
}


//
// 'heartbeat_update()' - Update the heartbeat count...
//

static void
heartbeat_update(void)
{
  cupsMutexLock(&heartbeat_mutex);

  heartbeat_count ++;

  _PAPPL_DEBUG("heartbeat_update: heartbeat_count=%lu\n", (unsigned long)heartbeat_count);

  cupsMutexUnlock(&heartbeat_mutex);
}


//
// 'infra_deregister_cb()' - Deregister-Output-Device callback.
//

static bool				// O - `true` on success
infra_deregister_cb(
    pappl_client_t  *client,		// I - Client connection
    const char      *device_uuid,	// I - "output-device-uuid" value
    pappl_printer_t *printer,		// I - Printer
    void            *data)		// I - Callback data
{
  (void)client;
  (void)device_uuid;
  (void)data;

  /* Delete the printer if this is the last/only device... */
  return (papplPrinterGetNumberOfInfraDevices(printer) == 1);
}


//
// 'infra_register_cb()' - Register-Output-Device callback.
//

static pappl_printer_t *		// O - Printer to use
infra_register_cb(
    pappl_client_t  *client,		// I - Client connection
    const char      *device_uuid,	// I - "output-device-uuid" value
    pappl_printer_t *requested_printer,	// I - Requested printer, if any
    void            *data)		// I - Callback data
{
  pappl_printer_t *printer = NULL;

  (void)data;

  // Look for a printer using this device
  if ((printer = papplSystemFindInfraPrinter(papplClientGetSystem(client), device_uuid)) != NULL)
  {
    // (Re)add the device and return existing printer
    papplPrinterAddInfraDevice(printer, device_uuid);

    return (printer);
  }
  else if (requested_printer)
  {
    // If a printer is supplied, only allow it if it is an infra printer...
    if (papplPrinterIsInfra(requested_printer))
    {
      // Add the device and return existing printer
      papplPrinterAddInfraDevice(requested_printer, device_uuid);
      return (requested_printer);
    }
    else
    {
      return (NULL);
    }
  }
  else if (papplSystemGetNumberOfPrinters(papplClientGetSystem(client)) < 32)
  {
    // Return new printer
    return (papplPrinterCreateInfra(papplClientGetSystem(client), /*printer_id*/0, /*printer_name*/device_uuid + 9, /*num_device_uuids*/1, &device_uuid));
  }
  else
  {
    // Don't allow more than 32 printers...
    return (NULL);
  }
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
  size_t		i,              // Looping var
			count;          // Number of values
  ipp_attribute_t	*attr;          // Printer attribute
  const char		*type = NULL;   // Raster type (colorspace + bits)
  pwg_media_t		*media = NULL;  // Media size
  int			xdpi = 0,       // Horizontal resolution
			ydpi = 0;       // Vertical resolution
  int			fd;             // Temporary file
  cups_raster_t		*ras;           // Raster stream
  cups_page_header_t	header;         // Page header
  unsigned char		*line,          // Line of raster data
			*lineptr;       // Pointer into line
  unsigned		y,              // Current position on page
			xcount, ycount, // Current count for X and Y
			xrep, yrep,     // Repeat count for X and Y
			xoff, yoff,     // Offsets for X and Y
			yend;           // End Y value
  int			temprow,        // Row in template
			tempcolor;      // Template color
  const char		*template;      // Pointer into template
  const unsigned char	*color;         // Current color
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
    _papplTestEndMessage(false, "no default or ready media reported by printer");
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
    _papplTestEndMessage(false, "no supported raster resolutions");
    return (NULL);
  }

  if (!type)
  {
    _papplTestEndMessage(false, "no supported color spaces or bit depths");
    return (NULL);
  }

  // Make the raster context and details...
#if CUPS_VERSION_MAJOR < 3 && CUPS_VERSION_MINOR < 5
  if (!cupsRasterInitPWGHeader(&header, media, type, xdpi, ydpi, "one-sided", NULL))
  {
    _papplTestEndMessage(false, "unable to initialize raster context: %s", cupsRasterGetErrorString());
    return (NULL);
  }

#else // CUPS 2.5/CUPS 3.0+
  cups_media_t cups_media;		// CUPS media information

  memset(&cups_media, 0, sizeof(cups_media));
  cupsCopyString(cups_media.media, media->pwg, sizeof(cups_media.media));
  cups_media.width  = media->width;
  cups_media.length = media->length;

  if (!cupsRasterInitHeader(&header, &cups_media, /*optimize*/NULL, IPP_QUALITY_NORMAL, /*intent*/NULL, IPP_ORIENT_PORTRAIT, "one-sided", type, xdpi, ydpi, /*sheet_back*/NULL))
  {
    _papplTestEndMessage(false, "unable to initialize raster context: %s", cupsRasterGetErrorString());
    return (NULL);
  }
#endif // CUPS_VERSION_MAJOR < 3

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
    _papplTestEndMessage(false, "unable to allocate %u bytes for raster output: %s", header.cupsBytesPerLine, strerror(errno));
    return (NULL);
  }

  if ((fd = cupsCreateTempFd(NULL, NULL, tempname, tempsize)) < 0)
  {
    _papplTestEndMessage(false, "unable to create temporary print file: %s", strerror(errno));
    free(line);
    return (NULL);
  }

  if ((ras = cupsRasterOpen(fd, CUPS_RASTER_WRITE_PWG)) == NULL)
  {
    _papplTestEndMessage(false, "unable to open raster stream: %s", cupsRasterGetErrorString());
    close(fd);
    free(line);
    return (NULL);
  }

  // Write a single page consisting of the template dots repeated over the page.
  cupsRasterWriteHeader(ras, &header);

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
  time_t	curtime;		// Current time
  int		expected;		// Expected timer count
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
  static const char * const pdf_files[] =
  {					// List of PDF files to print
    "portrait-gray-a4.pdf",
    "portrait-gray-letter.pdf",
    "portrait-color-a4.pdf",
    "portrait-color-letter.pdf",
    "landscape-gray-a4.pdf",
    "landscape-gray-letter.pdf",
    "landscape-color-a4.pdf",
    "landscape-color-letter.pdf"
  };


  testMessage("Starting tests...");

  if (testdata->waitsystem)
  {
    // Wait for the system to start...
    do
    {
      sleep(1);
    }
    while (!papplSystemIsRunning(testdata->system));
  }

  // Run each test...
  for (name = (const char *)cupsArrayGetFirst(testdata->names); name && !ret && (!papplSystemIsShutdown(testdata->system) || !testdata->waitsystem); name = (const char *)cupsArrayGetNext(testdata->names))
  {
    if (!strcmp(name, "api"))
    {
      if (!test_api(testdata->system))
        ret = (void *)1;
    }
    else if (!strcmp(name, "client"))
    {
      if (!test_client(testdata->system))
        ret = (void *)1;
    }
    else if (!strncmp(name, "client-", 7))
    {
      if (!test_client_max(testdata->system, name))
        ret = (void *)1;
    }
    else if (!strcmp(name, "infra"))
    {
      if (!test_infra(testdata->system))
        ret = (void *)1;
    }
#ifdef HAVE_LIBJPEG
    else if (!strcmp(name, "jpeg"))
    {
      if (!test_print_files(testdata->system, "jpeg", "image/jpeg", sizeof(jpeg_files) / sizeof(jpeg_files[0]), jpeg_files))
        ret = (void *)1;
    }
#endif // HAVE_LIBJPEG
    else if (!strcmp(name, "pdf"))
    {
      if (!test_print_files(testdata->system, "pdf", "application/pdf", sizeof(pdf_files) / sizeof(pdf_files[0]), pdf_files))
        ret = (void *)1;
    }
#ifdef HAVE_LIBPNG
    else if (!strcmp(name, "png"))
    {
      if (!test_print_files(testdata->system, "png", "image/png", sizeof(png_files) / sizeof(png_files[0]), png_files))
        ret = (void *)1;
    }
#endif // HAVE_LIBPNG
    else if (!strcmp(name, "pwg-raster"))
    {
      if (!test_pwg_raster(testdata->system))
        ret = (void *)1;
    }
    else if (strcmp(name, "idle-shutdown"))
    {
      _papplTestBegin("%s", name);
      _papplTestEndMessage(false, "unknown test");
      ret = (void *)1;
    }
  }

  if (name)
  {
    do
    {
      printf("%s: SKIP\n", name);
#ifndef DEBUG
      if (!isatty(2))
	fprintf(stderr, "%s: SKIP\n", name);
#endif // !DEBUG
    }
    while ((name = (const char *)cupsArrayGetNext(testdata->names)) != NULL);
  }

  if (!ret && cupsArrayFind(testdata->names, (void *)"api"))
  {
    // papplSystemSetEventCallback
    _papplTestBegin("api: papplSystemSetEventCallback");
    if (event_count > 0 && event_mask == (PAPPL_EVENT_SYSTEM_CONFIG_CHANGED | PAPPL_EVENT_PRINTER_CREATED | PAPPL_EVENT_PRINTER_DELETED | PAPPL_EVENT_PRINTER_CONFIG_CHANGED | PAPPL_EVENT_PRINTER_STATE_CHANGED | PAPPL_EVENT_JOB_COMPLETED | PAPPL_EVENT_JOB_CREATED | PAPPL_EVENT_JOB_PROGRESS | PAPPL_EVENT_JOB_STATE_CHANGED))
    {
      _papplTestEndMessage(true, "count=%lu", (unsigned long)event_count);
    }
    else
    {
      int			i;		// Looping var
      pappl_event_t	event;		// Current event
      static const char * const events[31] =
      {					// IPP "notify-events" strings for bits
	"document-completed",
	"document-config-changed",
	"document-created",
	"document-fetchable",
	"document-state-changed",
	"document-stopped",

	"job-completed",
	"job-config-changed",
	"job-created",
	"job-fetchable",
	"job-progress",
	"job-state-changed",
	"job-stopped",

	"printer-config-changed",
	"printer-finishings-changed",
	"printer-media-changed",
	"printer-queue-order-changed",
	"printer-restarted",
	"printer-shutdown",
	"printer-state-changed",
	"printer-stopped",

	"resource-canceled",
	"resource-config-changed",
	"resource-created",
	"resource-installed",
	"resource-changed",

	"printer-created",
	"printer-deleted",

	"system-config-changed",
	"system-state-changed",
	"system-stopped"
      };

      _papplTestEndMessage(false, "count=%lu", (unsigned long)event_count);
      ret = (void *)1;

      if (event_mask == PAPPL_EVENT_NONE)
      {
	testError("api: No events captured.");
      }
      else
      {
	for (i = 0, event = PAPPL_EVENT_DOCUMENT_COMPLETED; event <= PAPPL_EVENT_SYSTEM_STOPPED; i ++, event *= 2)
	{
	  if (event_mask & event)
	    testError("api: Got notify-event='%s'", events[i]);
	}
      }
    }

    // papplSystemAddTimerCallback
    _papplTestBegin("api: papplSystemAddTimerCallback");
    curtime  = time(NULL);
    expected = (int)((curtime - testdata->timer_start + _PAPPL_TIMER_INTERVAL - 1) / _PAPPL_TIMER_INTERVAL);
    if (expected > _PAPPL_MAX_TIMER_COUNT)
      expected = _PAPPL_MAX_TIMER_COUNT;

    if (testdata->timer_count == 0 || testdata->timer_count > _PAPPL_MAX_TIMER_COUNT || abs(expected - testdata->timer_count) > 1)
    {
      int	i;				// Looping var

      _papplTestEndMessage(false, "timer_count=%d, expected=%d", testdata->timer_count, expected);
      for (i = 1; i < testdata->timer_count; i ++)
	testMessage("timer@%ld (%ld seconds)", (long)testdata->timer_times[i], (long)(testdata->timer_times[i] - testdata->timer_times[i - 1]));

      ret = (void *)1;
    }
    else
    {
      _papplTestEndMessage(true, "timer_count=%d", testdata->timer_count);
    }
  }

  // Summarize results...
  if ((dir = cupsDirOpen(testdata->outdirname)) != NULL)
  {
    while ((dent = cupsDirRead(dir)) != NULL)
    {
      if (!S_ISDIR(dent->fileinfo.st_mode))
      {
        files ++;
        total += dent->fileinfo.st_size;
      }
    }

    cupsDirClose(dir);
  }

  if (files != output_count)
    ret = (void *)1;

  // Idle shutdown test...
  if (!ret && cupsArrayFind(testdata->names, (void *)"idle-shutdown"))
  {
    if (!test_idle_shutdown(testdata->system))
      ret = (void *)1;
  }

  // Show final status...
  if (ret)
    printf("\nFAILED: %d of %d output file(s), %.1fMB\n", files, output_count, (double)total / 1048576.0);
  else
    printf("\nPASSED: %d of %d output file(s), %.1fMB\n", files, output_count, (double)total / 1048576.0);

  // Force shutdown
  papplSystemShutdown(testdata->system);

  all_tests_done = true;

  return (ret);
}


//
// 'strcopy_cb()' - Copy a string for an array.
//

static char *				// O - New string
strcopy_cb(const char *s,		// I - String
           void       *data)		// I - Callback data (unused)
{
  (void)data;

  return (strdup(s));
}


//
// 'strfree_cb()' - Free a string from an array.
//

static void
strfree_cb(char *s,			// I - String
           void *data)			// I - Callback data (unused)
{
  (void)data;

  free(s);
}


//
// 'test_api()' - Run API unit tests.
//

static bool				// O - `true` on success, `false` on failure
test_api(pappl_system_t *system)	// I - System
{
  bool			pass = true;	// Pass/fail state
  size_t		i, j;		// Looping vars
  pappl_contact_t	get_contact,	// Contact for "get" call
			set_contact;	// Contact for ", set" call
  int			get_int,	// Integer for "get" call
			set_int;	// Integer for ", set" call
  char			get_str[1024],	// Temporary string for "get" call
			set_str[1024];	// Temporary string for ", set" call
  size_t		get_nvers;	// Number of versions for "get" call
  pappl_version_t	get_vers[10],	// Versions for "get" call
			set_vers[10];	// Versions for ", set" call
  const char		*get_value;	// Value for "get" call
  pappl_loglevel_t	get_loglevel,	// Log level for "get" call
			set_loglevel;	// Log level for ", set" call
  size_t		get_size,	// Size for "get" call
			set_size;	// Size for ", set" call
  pappl_printer_t	*printer;	// Current printer
  pappl_loc_t		*loc;		// Current localization
  _pappl_testprinter_t	pdata;		// Printer test data
  const char		*key = "A printer with that name already exists.",
					// Key string
			*text;		// Localized text
  static const char * const languages[] =
  {
    "de",
    "en",
    "es",
    "fr",
    "it",
    "ja"
  };
  static const char * const set_locations[10][2] =
  {
    // Some wonders of the ancient world (all north-eastern portion of globe...)
    { "Great Pyramid of Giza",        "geo:29.979175,31.134358" },
    { "Temple of Artemis at Ephesus", "geo:37.949722,27.363889" },
    { "Statue of Zeus at Olympia",    "geo:37.637861,21.63" },
    { "Colossus of Rhodes",           "geo:36.451111,28.227778" },
    { "Lighthouse of Alexandria",     "geo:31.213889,29.885556" },

    // Other places
    { "Niagara Falls",                "geo:43.0828201,-79.0763516" },
    { "Grand Canyon",                 "geo:36.0545936,-112.2307085" },
    { "Christ the Redeemer",          "geo:-22.9691208,-43.2583044" },
    { "Great Barrier Reef",           "geo:-16.7546653,143.8322946" },
    { "Science North",                "geo:46.4707,-80.9961" }
  };
  static const char * const set_loglevels[] =
  {					// Log level constants
    "UNSPEC",
    "DEBUG",
    "INFO",
    "WARN",
    "ERROR",
    "FATAL"
  };


  for (i = 0; i < (int)(sizeof(languages) / sizeof(languages[0])); i ++)
  {
    // papplSystemFindLoc
    _papplTestBegin("api: papplSystemFindLoc('%s')", languages[i]);
    if ((loc = papplSystemFindLoc(system, languages[i])) == NULL)
    {
      _papplTestEnd(false);
      pass = false;
    }
    else
      _papplTestEnd(true);

    // papplLocGetString
    _papplTestBegin("api: papplLocGetString('%s')", key);
    if ((text = papplLocGetString(loc, key)) == NULL || text == key)
    {
      _papplTestEndMessage(false, "got %p", text);
      pass = false;
    }
    else if (!strcmp(key, text) && strcmp(languages[i], "en"))
    {
      _papplTestEndMessage(false, "not localized");
      pass = false;
    }
    else
      _papplTestEndMessage(true, "got '%s'", text);
  }

  // papplSystemFindLoc
  _papplTestBegin("api: papplSystemFindLoc('zz')");
  if ((loc = papplSystemFindLoc(system, "zz")) != NULL)
  {
    _papplTestEndMessage(false, "got %p", loc);
    pass = false;
  }
  else
    _papplTestEndMessage(true, "got NULL");

  // papplLocGetString
  _papplTestBegin("api: papplLocGetString('%s')", key);
  if ((text = papplLocGetString(loc, key)) != key)
  {
    _papplTestEndMessage(false, "got %p", text);
    pass = false;
  }
  else
    _papplTestEndMessage(true, "got key string");

  // papplSystemGet/SetAdminGroup
  _papplTestBegin("api: papplSystemGetAdminGroup");
  if (papplSystemGetAdminGroup(system, get_str, sizeof(get_str)))
  {
    _papplTestEndMessage(false, "got '%s', expected NULL", get_str);
    pass = false;
  }
  else
    _papplTestEnd(true);

  for (i = 0; i < 10; i ++)
  {
    snprintf(set_str, sizeof(set_str), "admin-%u", (unsigned)i);
    _papplTestBegin("api: papplSystemGet/SetAdminGroup('%s')", set_str);
    papplSystemSetAdminGroup(system, set_str);
    if (!papplSystemGetAdminGroup(system, get_str, sizeof(get_str)))
    {
      _papplTestEndMessage(false, "got NULL, expected '%s'", set_str);
      pass = false;
    }
    else if (strcmp(get_str, set_str))
    {
      _papplTestEndMessage(false, "got '%s', expected '%s'", get_str, set_str);
      pass = false;
    }
    else
      _papplTestEnd(true);
  }

  _papplTestBegin("api: papplSystemGet/SetAdminGroup(NULL)");
  papplSystemSetAdminGroup(system, NULL);
  if (papplSystemGetAdminGroup(system, get_str, sizeof(get_str)))
  {
    _papplTestEndMessage(false, "got '%s', expected NULL", get_str);
    pass = false;
  }
  else
    _papplTestEnd(true);

  // papplSystemGet/SetContact
  _papplTestBegin("api: papplSystemGetContact");
  if (!papplSystemGetContact(system, &get_contact))
  {
    _papplTestEndMessage(false, "got NULL, expected 'Michael R Sweet'");
    pass = false;
  }
  else if (strcmp(get_contact.name, "Michael R Sweet"))
  {
    _papplTestEndMessage(false, "got '%s', expected 'Michael R Sweet'", get_contact.name);
    pass = false;
  }
  else if (strcmp(get_contact.email, "msweet@example.org"))
  {
    _papplTestEndMessage(false, "got '%s', expected 'msweet@example.org'", get_contact.email);
    pass = false;
  }
  else if (strcmp(get_contact.telephone, "+1-705-555-1212"))
  {
    _papplTestEndMessage(false, "got '%s', expected '+1-705-555-1212'", get_contact.telephone);
    pass = false;
  }
  else
    _papplTestEnd(true);

  for (i = 0; i < 10; i ++)
  {
    snprintf(set_contact.name, sizeof(set_contact.name), "Admin %u", (unsigned)i);
    snprintf(set_contact.email, sizeof(set_contact.email), "admin-%u@example.org", (unsigned)i);
    snprintf(set_contact.telephone, sizeof(set_contact.telephone), "+1-705-555-%04u", (unsigned)i * 1111);

    _papplTestBegin("api: papplSystemGet/SetContact('%s')", set_contact.name);
    papplSystemSetContact(system, &set_contact);
    if (!papplSystemGetContact(system, &get_contact))
    {
      _papplTestEndMessage(false, "got NULL, expected '%s'", set_contact.name);
      pass = false;
    }
    else if (strcmp(get_contact.name, set_contact.name))
    {
      _papplTestEndMessage(false, "got '%s', expected '%s'", get_contact.name, set_contact.name);
      pass = false;
    }
    else if (strcmp(get_contact.email, set_contact.email))
    {
      _papplTestEndMessage(false, "got '%s', expected '%s'", get_contact.email, set_contact.email);
      pass = false;
    }
    else if (strcmp(get_contact.telephone, set_contact.telephone))
    {
      _papplTestEndMessage(false, "got '%s', expected '%s'", get_contact.telephone, set_contact.telephone);
      pass = false;
    }
    else
      _papplTestEnd(true);
  }

  // papplSystemGet/SetDefaultPrinterID
  _papplTestBegin("api: papplSystemGetDefaultPrinterID");
  if ((get_int = papplSystemGetDefaultPrinterID(system)) == 0)
  {
    _papplTestEndMessage(false, "got 0, expected > 0");
    pass = false;
  }
  else
    _papplTestEndMessage(true, "%d", get_int);

  for (set_int = 2; set_int >= 1; set_int --)
  {
    _papplTestBegin("api: papplSystemSetDefaultPrinterID(%d)", set_int);
    papplSystemSetDefaultPrinterID(system, set_int);
    if ((get_int = papplSystemGetDefaultPrinterID(system)) != set_int)
    {
      _papplTestEndMessage(false, "got %d, expected %d", get_int, set_int);
      pass = false;
    }
    else
      _papplTestEnd(true);
  }

  // papplSystemGet/SetDefaultPrintGroup
  _papplTestBegin("api: papplSystemGetDefaultPrintGroup");
  if (papplSystemGetDefaultPrintGroup(system, get_str, sizeof(get_str)))
  {
    _papplTestEndMessage(false, "got '%s', expected NULL", get_str);
    pass = false;
  }
  else
    _papplTestEnd(true);

  for (i = 0; i < 10; i ++)
  {
    snprintf(set_str, sizeof(set_str), "users-%u", (unsigned)i);
    _papplTestBegin("api: papplSystemGet/SetDefaultPrintGroup('%s')", set_str);
    papplSystemSetDefaultPrintGroup(system, set_str);
    if (!papplSystemGetDefaultPrintGroup(system, get_str, sizeof(get_str)))
    {
      _papplTestEndMessage(false, "got NULL, expected '%s'", set_str);
      pass = false;
    }
    else if (strcmp(get_str, set_str))
    {
      _papplTestEndMessage(false, "got '%s', expected '%s'", get_str, set_str);
      pass = false;
    }
    else
      _papplTestEnd(true);
  }

  _papplTestBegin("api: papplSystemGet/SetDefaultPrintGroup(NULL)");
  papplSystemSetDefaultPrintGroup(system, NULL);
  if (papplSystemGetDefaultPrintGroup(system, get_str, sizeof(get_str)))
  {
    _papplTestEndMessage(false, "got '%s', expected NULL", get_str);
    pass = false;
  }
  else
    _papplTestEnd(true);

  // papplSystemGet/SetDNSSDName
  _papplTestBegin("api: papplSystemGetDNSSDName");
  if (!papplSystemGetDNSSDName(system, get_str, sizeof(get_str)))
  {
    _papplTestEndMessage(false, "got NULL, expected 'Test System'");
    pass = false;
  }
  else if (strcmp(get_str, "Test System"))
  {
    _papplTestEndMessage(false, "got '%s', expected 'Test System'", get_str);
    pass = false;
  }
  else
    _papplTestEnd(true);

  for (i = 0; i < 10; i ++)
  {
    snprintf(set_str, sizeof(set_str), "System Test %c", (char)(i + 'A'));
    _papplTestBegin("api: papplSystemGet/SetDNSSDName('%s')", set_str);
    papplSystemSetDNSSDName(system, set_str);
    if (!papplSystemGetDNSSDName(system, get_str, sizeof(get_str)))
    {
      _papplTestEndMessage(false, "got NULL, expected '%s'", set_str);
      pass = false;
    }
    else if (strcmp(get_str, set_str))
    {
      _papplTestEndMessage(false, "got '%s', expected '%s'", get_str, set_str);
      pass = false;
    }
    else
      _papplTestEnd(true);
  }

  _papplTestBegin("api: papplSystemGet/SetDNSSDName(NULL)");
  papplSystemSetDNSSDName(system, NULL);
  if (papplSystemGetDNSSDName(system, get_str, sizeof(get_str)))
  {
    _papplTestEndMessage(false, "got '%s', expected NULL", get_str);
    pass = false;
  }
  else
    _papplTestEnd(true);

  // papplSystemGet/SetFooterHTML
  _papplTestBegin("api: papplSystemGetFooterHTML");
  if ((get_value = papplSystemGetFooterHTML(system)) == NULL)
  {
    _papplTestEndMessage(false, "got NULL, expected 'Copyright ...'");
    pass = false;
  }
  else if (strncmp(get_value, "Copyright &copy; 202", 20))
  {
    _papplTestEndMessage(false, "got '%s', expected 'Copyright ...'", get_value);
    pass = false;
  }
  else
    _papplTestEnd(true);

  _papplTestBegin("api: papplSystemSetFooterHTML('Mike wuz here.')");
  papplSystemSetFooterHTML(system, "Mike wuz here.");
  if ((get_value = papplSystemGetFooterHTML(system)) == NULL)
  {
    _papplTestEndMessage(false, "got NULL, expected 'Mike wuz here.'");
    pass = false;
  }
  else if (papplSystemIsRunning(system))
  {
    // System is running so we can't change the footer text anymore...
    if (strncmp(get_value, "Copyright &copy; 202", 20))
    {
      _papplTestEndMessage(false, "got '%s', expected 'Copyright ...'", get_value);
      pass = false;
    }
    else
      _papplTestEnd(true);
  }
  else
  {
    // System is not running so we can change the footer text...
    if (strcmp(get_value, "Mike wuz here."))
    {
      _papplTestEndMessage(false, "got '%s', expected 'Mike wuz here.'", get_value);
      pass = false;
    }
    else
      _papplTestEnd(true);
  }

  // papplSystemGet/SetGeoLocation
  _papplTestBegin("api: papplSystemGetGeoLocation");
  if (!papplSystemGetGeoLocation(system, get_str, sizeof(get_str)))
  {
    _papplTestEndMessage(false, "got NULL, expected 'geo:46.4707,-80.9961'");
    pass = false;
  }
  else if (strcmp(get_str, "geo:46.4707,-80.9961"))
  {
    _papplTestEndMessage(false, "got '%s', expected 'geo:46.4707,-80.9961'", get_str);
    pass = false;
  }
  else
    _papplTestEnd(true);

  _papplTestBegin("api: papplSystemGet/SetGeoLocation('bad-value')");
  papplSystemSetGeoLocation(system, "bad-value");
  if (!papplSystemGetGeoLocation(system, get_str, sizeof(get_str)))
  {
    _papplTestEndMessage(false, "got NULL, expected 'geo:46.4707,-80.9961'");
    pass = false;
  }
  else if (strcmp(get_str, "geo:46.4707,-80.9961"))
  {
    _papplTestEndMessage(false, "got '%s', expected 'geo:46.4707,-80.9961'", get_str);
    pass = false;
  }
  else
    _papplTestEnd(true);

  for (i = 0; i < (int)(sizeof(set_locations) / sizeof(set_locations[0])); i ++)
  {
    _papplTestBegin("api: papplSystemGet/SetGeoLocation('%s')", set_locations[i][1]);
    papplSystemSetGeoLocation(system, set_locations[i][1]);
    if (!papplSystemGetGeoLocation(system, get_str, sizeof(get_str)))
    {
      _papplTestEndMessage(false, "got NULL, expected '%s'", set_locations[i][1]);
      pass = false;
    }
    else if (strcmp(get_str, set_locations[i][1]))
    {
      _papplTestEndMessage(false, "got '%s', expected '%s'", get_str, set_locations[i][1]);
      pass = false;
    }
    else
      _papplTestEnd(true);
  }

  _papplTestBegin("api: papplSystemGet/SetGeoLocation(NULL)");
  papplSystemSetGeoLocation(system, NULL);
  if (papplSystemGetGeoLocation(system, get_str, sizeof(get_str)))
  {
    _papplTestEndMessage(false, "got '%s', expected NULL", get_str);
    pass = false;
  }
  else
    _papplTestEnd(true);

  // papplSystemGet/SetHostname
  _papplTestBegin("api: papplSystemGetHostname");
  if (!papplSystemGetHostName(system, get_str, sizeof(get_str)))
  {
    _papplTestEndMessage(false, "got NULL, expected '*.domain'");
    pass = false;
  }
  else if (!strchr(get_str, '.'))
  {
    _papplTestEndMessage(false, "got '%s', expected '*.domain'", get_str);
    pass = false;
  }
  else
    _papplTestEnd(true);

  for (i = 0; i < 10; i ++)
  {
    snprintf(set_str, sizeof(set_str), "example%u.org", (unsigned)i);
    _papplTestBegin("api: papplSystemGet/SetHostname('%s')", set_str);
    papplSystemSetHostName(system, set_str);
    if (!papplSystemGetHostName(system, get_str, sizeof(get_str)))
    {
      _papplTestEndMessage(false, "got NULL, expected '%s'", set_str);
      pass = false;
    }
    else if (strcmp(get_str, set_str))
    {
      _papplTestEndMessage(false, "got '%s', expected '%s'", get_str, set_str);
      pass = false;
    }
    else
      _papplTestEnd(true);
  }

  _papplTestBegin("api: papplSystemGet/SetHostName(NULL)");
  papplSystemSetHostName(system, NULL);
  if (!papplSystemGetHostName(system, get_str, sizeof(get_str)))
  {
    _papplTestEndMessage(false, "got NULL, expected '*.domain'");
    pass = false;
  }
  else if (!strchr(get_str, '.'))
  {
    _papplTestEndMessage(false, "got '%s', expected '*.domain'", get_str);
    pass = false;
  }
  else
    _papplTestEnd(true);

  // papplSystemGet/SetIdleShutdown
  _papplTestBegin("api: papplSystemGetIdleShutdown");
  if ((get_int = papplSystemGetIdleShutdown(system)) != 0)
  {
    _papplTestEndMessage(false, "got %d, expected 0", get_int);
    pass = false;
  }
  else
    _papplTestEnd(true);

  _papplTestBegin("api: papplSystemSetIdleShutdown(-1)");
  papplSystemSetIdleShutdown(system, -1);
  if ((get_int = papplSystemGetIdleShutdown(system)) != 0)
  {
    _papplTestEndMessage(false, "got %d, expected 0", get_int);
    pass = false;
  }
  else
    _papplTestEnd(true);

  _papplTestBegin("api: papplSystemSetIdleShutdown(30)");
  papplSystemSetIdleShutdown(system, 30);
  if ((get_int = papplSystemGetIdleShutdown(system)) != 30)
  {
    _papplTestEndMessage(false, "got %d, expected 30", get_int);
    pass = false;
  }
  else
    _papplTestEnd(true);

  _papplTestBegin("api: papplSystemSetIdleShutdown(0)");
  papplSystemSetIdleShutdown(system, 0);
  if ((get_int = papplSystemGetIdleShutdown(system)) != 0)
  {
    _papplTestEndMessage(false, "got %d, expected 0", get_int);
    pass = false;
  }
  else
    _papplTestEnd(true);

  _papplTestBegin("api: papplSystemSetIdleShutdown(90)");
  papplSystemSetIdleShutdown(system, 90);
  if ((get_int = papplSystemGetIdleShutdown(system)) != 90)
  {
    _papplTestEndMessage(false, "got %d, expected 30", get_int);
    pass = false;
  }
  else
    _papplTestEnd(true);

  // papplSystemGet/SetLocation
  _papplTestBegin("api: papplSystemGetLocation");
  if (!papplSystemGetLocation(system, get_str, sizeof(get_str)))
  {
    _papplTestEndMessage(false, "got NULL, expected 'Test Lab 42'");
    pass = false;
  }
  else if (strcmp(get_str, "Test Lab 42"))
  {
    _papplTestEndMessage(false, "got '%s', expected 'Test Lab 42'", get_str);
    pass = false;
  }
  else
    _papplTestEnd(true);

  for (i = 0; i < (int)(sizeof(set_locations) / sizeof(set_locations[0])); i ++)
  {
    _papplTestBegin("api: papplSystemGet/SetLocation('%s')", set_locations[i][0]);
    papplSystemSetLocation(system, set_locations[i][0]);
    if (!papplSystemGetLocation(system, get_str, sizeof(get_str)))
    {
      _papplTestEndMessage(false, "got NULL, expected '%s'", set_locations[i][0]);
      pass = false;
    }
    else if (strcmp(get_str, set_locations[i][0]))
    {
      _papplTestEndMessage(false, "got '%s', expected '%s'", get_str, set_locations[i][0]);
      pass = false;
    }
    else
      _papplTestEnd(true);
  }

  _papplTestBegin("api: papplSystemGet/SetLocation(NULL)");
  papplSystemSetLocation(system, NULL);
  if (papplSystemGetLocation(system, get_str, sizeof(get_str)))
  {
    _papplTestEndMessage(false, "got '%s', expected NULL", get_str);
    pass = false;
  }
  else
    _papplTestEnd(true);

  // papplSystemGet/SetLogLevel
  _papplTestBegin("api: papplSystemGetLogLevel");
  if (papplSystemGetLogLevel(system) == PAPPL_LOGLEVEL_UNSPEC)
  {
    _papplTestEndMessage(false, "got PAPPL_LOGLEVEL_UNSPEC, expected another PAPPL_LOGLEVEL_ value");
    pass = false;
  }
  else
    _papplTestEnd(true);

  for (set_loglevel = PAPPL_LOGLEVEL_FATAL; set_loglevel >= PAPPL_LOGLEVEL_DEBUG; set_loglevel --)
  {
    _papplTestBegin("api: papplSystemSetLogLevel(PAPPL_LOGLEVEL_%s)", set_loglevels[set_loglevel + 1]);
    papplSystemSetLogLevel(system, set_loglevel);
    if ((get_loglevel = papplSystemGetLogLevel(system)) != set_loglevel)
    {
      _papplTestEndMessage(false, "got PAPPL_LOGLEVEL_%s, expected PAPPL_LOGLEVEL_%s", set_loglevels[get_loglevel + 1], set_loglevels[set_loglevel + 1]);
      pass = false;
    }
    else
      _papplTestEnd(true);
  }

  // papplSystemGet/SetMaxLogSize
  _papplTestBegin("api: papplSystemGetMaxLogSize");
  if ((get_size = papplSystemGetMaxLogSize(system)) != (size_t)(1024 * 1024))
  {
    _papplTestEndMessage(false, "got %ld, expected %ld", (long)get_size, (long)(1024 * 1024));
    pass = false;
  }
  else
    _papplTestEnd(true);

  for (set_size = 0; set_size <= (16 * 1024 * 1024); set_size += 1024 * 1024)
  {
    _papplTestBegin("api: papplSystemSetMaxLogSize(%ld)", (long)set_size);
    papplSystemSetMaxLogSize(system, set_size);
    if ((get_size = papplSystemGetMaxLogSize(system)) != set_size)
    {
      _papplTestEndMessage(false, "got %ld, expected %ld", (long)get_size,  (long)set_size);
      pass = false;
    }
    else
      _papplTestEnd(true);
  }

  _papplTestBegin("api: papplSystemSetMaxLogSize(0)");
  papplSystemSetMaxLogSize(system, 0);
  if ((get_size = papplSystemGetMaxLogSize(system)) != 0)
  {
    _papplTestEndMessage(false, "got %ld, expected 0", (long)get_size);
    pass = false;
  }
  else
    _papplTestEnd(true);

  // papplSystemGet/SetNextPrinterID
  _papplTestBegin("api: papplSystemGetNextPrinterID");
  if ((get_int = papplSystemGetNextPrinterID(system)) != 3)
  {
    _papplTestEndMessage(false, "got %d, expected 3", get_int);
    pass = false;
  }
  else
    _papplTestEnd(true);

  set_int = (cupsGetRand() % 1000000) + 4;
  _papplTestBegin("api: papplSystemSetNextPrinterID(%d)", set_int);
  papplSystemSetNextPrinterID(system, set_int);
  if ((get_int = papplSystemGetNextPrinterID(system)) != set_int)
  {
    if (papplSystemIsRunning(system))
      _papplTestEnd(true);
    else
    {
      _papplTestEndMessage(false, "got %d, expected %d", get_int, set_int);
      pass = false;
    }
  }
  else
    _papplTestEnd(true);

  // papplSystemGet/SetOrganization
  _papplTestBegin("api: papplSystemGetOrganization");
  if (!papplSystemGetOrganization(system, get_str, sizeof(get_str)))
  {
    _papplTestEndMessage(false, "got NULL, expected 'Lakeside Robotics'");
    pass = false;
  }
  else if (strcmp(get_str, "Lakeside Robotics"))
  {
    _papplTestEndMessage(false, "got '%s', expected 'Lakeside Robotics'", get_str);
    pass = false;
  }
  else
    _papplTestEnd(true);

  for (i = 0; i < 10; i ++)
  {
    snprintf(set_str, sizeof(set_str), "Organization %c", (char)(i + 'A'));
    _papplTestBegin("api: papplSystemGet/SetOrganization('%s')", set_str);
    papplSystemSetOrganization(system, set_str);
    if (!papplSystemGetOrganization(system, get_str, sizeof(get_str)))
    {
      _papplTestEndMessage(false, "got NULL, expected '%s'", set_str);
      pass = false;
    }
    else if (strcmp(get_str, set_str))
    {
      _papplTestEndMessage(false, "got '%s', expected '%s'", get_str, set_str);
      pass = false;
    }
    else
      _papplTestEnd(true);
  }

  _papplTestBegin("api: papplSystemGet/SetOrganization(NULL)");
  papplSystemSetOrganization(system, NULL);
  if (papplSystemGetOrganization(system, get_str, sizeof(get_str)))
  {
    _papplTestEndMessage(false, "got '%s', expected NULL", get_str);
    pass = false;
  }
  else
    _papplTestEnd(true);

  // papplSystemGet/SetOrganizationalUnit
  _papplTestBegin("api: papplSystemGetOrganizationalUnit");
  if (papplSystemGetOrganizationalUnit(system, get_str, sizeof(get_str)))
  {
    _papplTestEndMessage(false, "got '%s', expected NULL", get_str);
    pass = false;
  }
  else
    _papplTestEnd(true);

  for (i = 0; i < 10; i ++)
  {
    snprintf(set_str, sizeof(set_str), "%c Team", (char)(i + 'A'));
    _papplTestBegin("api: papplSystemGet/SetOrganizationalUnit('%s')", set_str);
    papplSystemSetOrganizationalUnit(system, set_str);
    if (!papplSystemGetOrganizationalUnit(system, get_str, sizeof(get_str)))
    {
      _papplTestEndMessage(false, "got NULL, expected '%s'", set_str);
      pass = false;
    }
    else if (strcmp(get_str, set_str))
    {
      _papplTestEndMessage(false, "got '%s', expected '%s'", get_str, set_str);
      pass = false;
    }
    else
      _papplTestEnd(true);
  }

  _papplTestBegin("api: papplSystemGet/SetOrganizationalUnit(NULL)");
  papplSystemSetOrganizationalUnit(system, NULL);
  if (papplSystemGetOrganizationalUnit(system, get_str, sizeof(get_str)))
  {
    _papplTestEndMessage(false, "got '%s', expected NULL", get_str);
    pass = false;
  }
  else
    _papplTestEnd(true);

  // papplSystemGet/SetUUID
  _papplTestBegin("api: papplSystemGetUUID");
  if ((get_value = papplSystemGetUUID(system)) == NULL)
  {
    _papplTestEndMessage(false, "got NULL, expected 'urn:uuid:...'");
    pass = false;
  }
  else if (strncmp(get_value, "urn:uuid:", 9))
  {
    _papplTestEndMessage(false, "got '%s', expected 'urn:uuid:...'", get_value);
    pass = false;
  }
  else
    _papplTestEnd(true);

  for (i = 0; i < 10; i ++)
  {
    snprintf(set_str, sizeof(set_str), "urn:uuid:%04x%04x-%04x-%04x-%04x-%04x%04x%04x", (unsigned)(cupsGetRand() % 65536), (unsigned)(cupsGetRand() % 65536), (unsigned)(cupsGetRand() % 65536), (unsigned)(cupsGetRand() % 65536), (unsigned)(cupsGetRand() % 65536), (unsigned)(cupsGetRand() % 65536), (unsigned)(cupsGetRand() % 65536), (unsigned)(cupsGetRand() % 65536));
    _papplTestBegin("api: papplSystemGet/SetUUID('%s')", set_str);
    papplSystemSetUUID(system, set_str);
    if ((get_value = papplSystemGetUUID(system)) == NULL)
    {
      _papplTestEndMessage(false, "got NULL, expected '%s'", set_str);
      pass = false;
    }
    else if (papplSystemIsRunning(system))
    {
      if (!strcmp(get_value, set_str) || strncmp(get_value, "urn:uuid:", 9))
      {
	_papplTestEndMessage(false, "got '%s', expected different 'urn:uuid:...'", get_value);
	pass = false;
      }
      else
        _papplTestEnd(true);
    }
    else if (strcmp(get_value, set_str))
    {
      _papplTestEndMessage(false, "got '%s', expected '%s'", get_value, set_str);
      pass = false;
    }
    else
      _papplTestEnd(true);
  }

  _papplTestBegin("api: papplSystemGet/SetUUID(NULL)");
  if ((get_value = papplSystemGetUUID(system)) == NULL)
  {
    _papplTestEndMessage(false, "unable to get current UUID");
    pass = false;
  }
  else
  {
    cupsCopyString(get_str, get_value, sizeof(get_str));

    papplSystemSetUUID(system, NULL);
    if ((get_value = papplSystemGetUUID(system)) == NULL)
    {
      _papplTestEndMessage(false, "got NULL, expected 'urn:uuid:...'");
      pass = false;
    }
    else if (papplSystemIsRunning(system))
    {
      if (!strcmp(get_value, set_str) || strncmp(get_value, "urn:uuid:", 9))
      {
	_papplTestEndMessage(false, "got '%s', expected different 'urn:uuid:...'", get_value);
	pass = false;
      }
      else
	_papplTestEnd(true);
    }
    else if (!strcmp(get_value, set_str))
    {
      _papplTestEndMessage(false, "got '%s', expected different '%s'", get_value, set_str);
      pass = false;
    }
    else
      _papplTestEnd(true);
  }

  // papplSystemGet/SetVersions
  _papplTestBegin("api: papplSystemGetVersions");

  if ((get_nvers = papplSystemGetVersions(system, (int)(sizeof(get_vers) / sizeof(get_vers[0])), get_vers)) != 1)
  {
    _papplTestEndMessage(false, "got %u versions, expected 1", (unsigned)get_nvers);
    pass = false;
  }
  else if (strcmp(get_vers[0].name, "Test System") || strcmp(get_vers[0].sversion, "2.0 build 42"))
  {
    _papplTestEndMessage(false, "got '%s v%s', expected 'Test System v2.0m build 42'", get_vers[0].name, get_vers[0].sversion);
    pass = false;
  }
  else
    _papplTestEnd(true);

  for (i = 0; i < 10; i ++)
  {
    _papplTestBegin("api: papplSystemGet/SetVersions(%u)", (unsigned)i + 1);

    memset(set_vers + i, 0, sizeof(pappl_version_t));
    snprintf(set_vers[i].name, sizeof(set_vers[i].name), "Component %c", (char)('A' + i));
    set_vers[i].version[0] = (unsigned short)(i + 1);
    set_vers[i].version[1] = (unsigned short)(cupsGetRand() % 100);
    snprintf(set_vers[i].sversion, sizeof(set_vers[i].sversion), "%u.%02u", set_vers[i].version[0], set_vers[i].version[1]);

    papplSystemSetVersions(system, i + 1, set_vers);

    if ((get_nvers = papplSystemGetVersions(system, (int)(sizeof(get_vers) / sizeof(get_vers[0])), get_vers)) != (i + 1))
    {
      _papplTestEndMessage(false, "got %u versions, expected %u", (unsigned)get_nvers, (unsigned)i + 1);
      pass = false;
    }
    else
    {
      for (j = 0; j < get_nvers; j ++)
      {
        if (strcmp(get_vers[j].name, set_vers[j].name) || strcmp(get_vers[j].sversion, set_vers[j].sversion))
	{
	  _papplTestEndMessage(false, "got '%s v%s', expected '%s v%s'", get_vers[j].name, get_vers[j].sversion, set_vers[j].name, set_vers[j].sversion);
	  pass = false;
	  break;
	}
      }

      if (j >= get_nvers)
        _papplTestEnd(true);
    }
  }

  // papplSystemFindPrinter
  _papplTestBegin("api: papplSystemFindPrinter(default)");
  if ((printer = papplSystemFindPrinter(system, "/ipp/print", 0, NULL)) == NULL)
  {
    _papplTestEndMessage(false, "got NULL");
    pass = false;
  }
  else if (papplPrinterGetID(printer) != papplSystemGetDefaultPrinterID(system))
  {
    _papplTestEndMessage(false, "got printer #%d, expected #%d", papplPrinterGetID(printer), papplSystemGetDefaultPrinterID(system));
    pass = false;
  }
  else
    _papplTestEnd(true);

  for (set_int = 1; set_int < 3; set_int ++)
  {
    _papplTestBegin("api: papplSystemFindPrinter(%d)", set_int);
    if ((printer = papplSystemFindPrinter(system, NULL, set_int, NULL)) == NULL)
    {
      _papplTestEndMessage(false, "got NULL");
      pass = false;
    }
    else
    {
      _papplTestEnd(true);
      if (!test_api_printer(printer))
	pass = false;
    }
  }

  // papplPrinterCreate/Delete
  for (i = 0; i < 10; i ++)
  {
    char	name[128];		// Printer name

    snprintf(name, sizeof(name), "test%u", (unsigned)i);
    _papplTestBegin("api: papplPrinterCreate(%s)", name);
    if ((printer = papplPrinterCreate(system, 0, name, "pwg_common-300dpi-black_1-sgray_8", "MFG:PWG;MDL:Office Printer;CMD:PWGRaster;", "file:///dev/null")) == NULL)
    {
      _papplTestEndMessage(false, "got NULL");
      pass = false;
    }
    else
    {
      _papplTestEnd(true);

      get_int = papplPrinterGetID(printer);

      _papplTestBegin("api: papplPrinterDelete(%s)", name);
      papplPrinterDelete(printer);

      if (papplSystemFindPrinter(system, NULL, get_int, NULL) != NULL)
      {
        _papplTestEndMessage(false, "printer not deleted");
        pass = false;
      }
      else
      {
        _papplTestEnd(true);

	_papplTestBegin("api: papplPrinterCreate(%s again)", name);
	if ((printer = papplPrinterCreate(system, 0, name, "pwg_common-300dpi-black_1-sgray_8", "MFG:PWG;MDL:Office Printer;CMD:PWGRaster;", "file:///dev/null")) == NULL)
	{
	  _papplTestEndMessage(false, "got NULL");
	  pass = false;
	}
	else if (papplPrinterGetID(printer) == get_int)
	{
	  _papplTestEndMessage(false, "got the same printer ID");
	  pass = false;
	}
	else
	  _papplTestEnd(true);
      }
    }
  }

  // papplSystemIteratePrinters
  _papplTestBegin("api: papplSystemIteratePrinters");

  pdata.pass = true;
  pdata.count = 0;

  papplSystemIteratePrinters(system, (pappl_printer_cb_t)test_api_printer_cb, &pdata);

  if (pdata.count != 12)
  {
    _papplTestEndMessage(false, "got %d printers, expected 12", pdata.count);
    pass = false;
  }
  else if (!pdata.pass)
  {
    _papplTestEndMessage(false, "per-printer test failed");
    pass = false;
  }
  else
    _papplTestEnd(true);

  return (pass);
}


//
// 'test_api_printer()' - Test papplPrinter APIs.
//

static bool				// O - `true` on success, `false` on failure
test_api_printer(
    pappl_printer_t *printer)		// I - Printer
{
  bool			pass = true;	// Pass/fail for tests
  int			i;		// Looping vars
  pappl_contact_t	get_contact,	// Contact for "get" call
			set_contact;	// Contact for ", set" call
  int			get_int,	// Integer for "get" call
			set_int;	// Integer for ", set" call
  char			get_str[1024],	// Temporary string for "get" call
			set_str[1024];	// Temporary string for ", set" call
  const char		*get_ptr;	// Get string pointer
  bool			expected_null;	// Expected NULL string value?
  static const char * const set_locations[10][2] =
  {
    // Some wonders of the ancient world (all north-eastern portion of globe...)
    { "Great Pyramid of Giza",        "geo:29.979175,31.134358" },
    { "Temple of Artemis at Ephesus", "geo:37.949722,27.363889" },
    { "Statue of Zeus at Olympia",    "geo:37.637861,21.63" },
    { "Colossus of Rhodes",           "geo:36.451111,28.227778" },
    { "Lighthouse of Alexandria",     "geo:31.213889,29.885556" },

    // Other places
    { "Niagara Falls",                "geo:43.0828201,-79.0763516" },
    { "Grand Canyon",                 "geo:36.0545936,-112.2307085" },
    { "Christ the Redeemer",          "geo:-22.9691208,-43.2583044" },
    { "Great Barrier Reef",           "geo:-16.7546653,143.8322946" },
    { "Science North",                "geo:46.4707,-80.9961" }
  };


  // papplPrinterGet/SetContact
  _papplTestBegin("api: papplPrinterGetContact");
  if (!papplPrinterGetContact(printer, &get_contact))
  {
    _papplTestEndMessage(false, "got NULL, expected 'Michael R Sweet'");
    pass = false;
  }
  else if (strcmp(get_contact.name, "Michael R Sweet"))
  {
    _papplTestEndMessage(false, "got '%s', expected 'Michael R Sweet'", get_contact.name);
    pass = false;
  }
  else if (strcmp(get_contact.email, "msweet@example.org"))
  {
    _papplTestEndMessage(false, "got '%s', expected 'msweet@example.org'", get_contact.email);
    pass = false;
  }
  else if (strcmp(get_contact.telephone, "+1-705-555-1212"))
  {
    _papplTestEndMessage(false, "got '%s', expected '+1-705-555-1212'", get_contact.telephone);
    pass = false;
  }
  else
    _papplTestEnd(true);

  for (i = 0; i < 10; i ++)
  {
    snprintf(set_contact.name, sizeof(set_contact.name), "Admin %d", i);
    snprintf(set_contact.email, sizeof(set_contact.email), "admin-%d@example.org", i);
    snprintf(set_contact.telephone, sizeof(set_contact.telephone), "+1-705-555-%04d", i * 1111);

    _papplTestBegin("api: papplPrinterGet/SetContact('%s')", set_contact.name);
    papplPrinterSetContact(printer, &set_contact);
    if (!papplPrinterGetContact(printer, &get_contact))
    {
      _papplTestEndMessage(false, "got NULL, expected '%s'", set_contact.name);
      pass = false;
    }
    else if (strcmp(get_contact.name, set_contact.name))
    {
      _papplTestEndMessage(false, "got '%s', expected '%s'", get_contact.name, set_contact.name);
      pass = false;
    }
    else if (strcmp(get_contact.email, set_contact.email))
    {
      _papplTestEndMessage(false, "got '%s', expected '%s'", get_contact.email, set_contact.email);
      pass = false;
    }
    else if (strcmp(get_contact.telephone, set_contact.telephone))
    {
      _papplTestEndMessage(false, "got '%s', expected '%s'", get_contact.telephone, set_contact.telephone);
      pass = false;
    }
    else
      _papplTestEnd(true);
  }

  // papplPrinterGet/SetPrintGroup
  _papplTestBegin("api: papplPrinterGetPrintGroup");
  if (papplPrinterGetPrintGroup(printer, get_str, sizeof(get_str)))
  {
    _papplTestEndMessage(false, "got '%s', expected NULL", get_str);
    pass = false;
  }
  else
    _papplTestEnd(true);

  for (i = 0; i < 10; i ++)
  {
    snprintf(set_str, sizeof(set_str), "users-%d", i);
    _papplTestBegin("api: papplPrinterGet/SetPrintGroup('%s')", set_str);
    papplPrinterSetPrintGroup(printer, set_str);
    if (!papplPrinterGetPrintGroup(printer, get_str, sizeof(get_str)))
    {
      _papplTestEndMessage(false, "got NULL, expected '%s'", set_str);
      pass = false;
    }
    else if (strcmp(get_str, set_str))
    {
      _papplTestEndMessage(false, "got '%s', expected '%s'", get_str, set_str);
      pass = false;
    }
    else
      _papplTestEnd(true);
  }

  _papplTestBegin("api: papplPrinterGet/SetPrintGroup(NULL)");
  papplPrinterSetPrintGroup(printer, NULL);
  if (papplPrinterGetPrintGroup(printer, get_str, sizeof(get_str)))
  {
    _papplTestEndMessage(false, "got '%s', expected NULL", get_str);
    pass = false;
  }
  else
    _papplTestEnd(true);

  // papplPrinterGet/SetDNSSDName
  _papplTestBegin("api: papplPrinterGetDNSSDName");
  if (!papplPrinterGetDNSSDName(printer, get_str, sizeof(get_str)))
  {
    _papplTestEndMessage(false, "got NULL, expected string");
    pass = false;
  }
  else
    _papplTestEnd(true);

  for (i = 0; i < 10; i ++)
  {
    snprintf(set_str, sizeof(set_str), "Printer Test %c", i + 'A');
    _papplTestBegin("api: papplPrinterGet/SetDNSSDName('%s')", set_str);
    papplPrinterSetDNSSDName(printer, set_str);
    if (!papplPrinterGetDNSSDName(printer, get_str, sizeof(get_str)))
    {
      _papplTestEndMessage(false, "got NULL, expected '%s'", set_str);
      pass = false;
    }
    else if (strcmp(get_str, set_str))
    {
      _papplTestEndMessage(false, "got '%s', expected '%s'", get_str, set_str);
      pass = false;
    }
    else
      _papplTestEnd(true);
  }

  _papplTestBegin("api: papplPrinterGet/SetDNSSDName(NULL)");
  papplPrinterSetDNSSDName(printer, NULL);
  if (papplPrinterGetDNSSDName(printer, get_str, sizeof(get_str)))
  {
    _papplTestEndMessage(false, "got '%s', expected NULL", get_str);
    pass = false;
  }
  else
    _papplTestEnd(true);

  // papplPrinterGet/SetGeoLocation
  expected_null = !strcmp(papplPrinterGetName(printer), "Label Printer");

  _papplTestBegin("api: papplPrinterGetGeoLocation");
  get_ptr = papplPrinterGetGeoLocation(printer, get_str, sizeof(get_str));
  if (get_ptr && expected_null)
  {
    _papplTestEndMessage(false, "got '%s', expected NULL", get_str);
    pass = false;
  }
  else if (!get_ptr && !expected_null)
  {
    _papplTestEndMessage(false, "got NULL, expected 'geo:46.4707,-80.9961'");
    pass = false;
  }
  else if (get_ptr && strcmp(get_str, "geo:46.4707,-80.9961"))
  {
    _papplTestEndMessage(false, "got '%s', expected 'geo:46.4707,-80.9961'", get_str);
    pass = false;
  }
  else
    _papplTestEnd(true);

  _papplTestBegin("api: papplPrinterGet/SetGeoLocation('bad-value')");
  papplPrinterSetGeoLocation(printer, "bad-value");
  get_ptr = papplPrinterGetGeoLocation(printer, get_str, sizeof(get_str));
  if (get_ptr && expected_null)
  {
    _papplTestEndMessage(false, "got '%s', expected NULL", get_str);
    pass = false;
  }
  else if (!get_ptr && !expected_null)
  {
    _papplTestEndMessage(false, "got NULL, expected 'geo:46.4707,-80.9961'");
    pass = false;
  }
  else if (get_ptr && strcmp(get_str, "geo:46.4707,-80.9961"))
  {
    _papplTestEndMessage(false, "got '%s', expected 'geo:46.4707,-80.9961'", get_str);
    pass = false;
  }
  else
    _papplTestEnd(true);

  for (i = 0; i < (int)(sizeof(set_locations) / sizeof(set_locations[0])); i ++)
  {
    _papplTestBegin("api: papplPrinterGet/SetGeoLocation('%s')", set_locations[i][1]);
    papplPrinterSetGeoLocation(printer, set_locations[i][1]);
    if (!papplPrinterGetGeoLocation(printer, get_str, sizeof(get_str)))
    {
      _papplTestEndMessage(false, "got NULL, expected '%s'", set_locations[i][1]);
      pass = false;
    }
    else if (strcmp(get_str, set_locations[i][1]))
    {
      _papplTestEndMessage(false, "got '%s', expected '%s'", get_str, set_locations[i][1]);
      pass = false;
    }
    else
      _papplTestEnd(true);
  }

  _papplTestBegin("api: papplPrinterGet/SetGeoLocation(NULL)");
  papplPrinterSetGeoLocation(printer, NULL);
  if (papplPrinterGetGeoLocation(printer, get_str, sizeof(get_str)))
  {
    _papplTestEndMessage(false, "got '%s', expected NULL", get_str);
    pass = false;
  }
  else
    _papplTestEnd(true);

  // papplPrinterGet/SetLocation
  _papplTestBegin("api: papplPrinterGetLocation");
  if (!papplPrinterGetLocation(printer, get_str, sizeof(get_str)))
  {
    _papplTestEndMessage(false, "got NULL, expected 'Test Lab 42'");
    pass = false;
  }
  else if (strcmp(get_str, "Test Lab 42"))
  {
    _papplTestEndMessage(false, "got '%s', expected 'Test Lab 42'", get_str);
    pass = false;
  }
  else
    _papplTestEnd(true);

  for (i = 0; i < (int)(sizeof(set_locations) / sizeof(set_locations[0])); i ++)
  {
    _papplTestBegin("api: papplPrinterGet/SetLocation('%s')", set_locations[i][0]);
    papplPrinterSetLocation(printer, set_locations[i][0]);
    if (!papplPrinterGetLocation(printer, get_str, sizeof(get_str)))
    {
      _papplTestEndMessage(false, "got NULL, expected '%s'", set_locations[i][0]);
      pass = false;
    }
    else if (strcmp(get_str, set_locations[i][0]))
    {
      _papplTestEndMessage(false, "got '%s', expected '%s'", get_str, set_locations[i][0]);
      pass = false;
    }
    else
      _papplTestEnd(true);
  }

  _papplTestBegin("api: papplPrinterGet/SetLocation(NULL)");
  papplPrinterSetLocation(printer, NULL);
  if (papplPrinterGetLocation(printer, get_str, sizeof(get_str)))
  {
    _papplTestEndMessage(false, "got '%s', expected NULL", get_str);
    pass = false;
  }
  else
    _papplTestEnd(true);

  // papplPrinterGet/SetNextJobID
  _papplTestBegin("api: papplPrinterGetNextJobID");
  if ((get_int = papplPrinterGetNextJobID(printer)) != 1)
  {
    _papplTestEndMessage(false, "got %d, expected 1", get_int);
    pass = false;
  }
  else
    _papplTestEnd(true);

  set_int = (cupsGetRand() % 1000000) + 2;
  _papplTestBegin("api: papplPrinterSetNextJobID(%d)", set_int);
  papplPrinterSetNextJobID(printer, set_int);
  if ((get_int = papplPrinterGetNextJobID(printer)) != set_int)
  {
    _papplTestEndMessage(false, "got %d, expected %d", get_int, set_int);
    pass = false;
  }
  else
    _papplTestEnd(true);

  // papplPrinterGet/SetOrganization
  _papplTestBegin("api: papplPrinterGetOrganization");
  if (!papplPrinterGetOrganization(printer, get_str, sizeof(get_str)))
  {
    _papplTestEndMessage(false, "got NULL, expected 'Lakeside Robotics'");
    pass = false;
  }
  else if (strcmp(get_str, "Lakeside Robotics"))
  {
    _papplTestEndMessage(false, "got '%s', expected 'Lakeside Robotics'", get_str);
    pass = false;
  }
  else
    _papplTestEnd(true);

  for (i = 0; i < 10; i ++)
  {
    snprintf(set_str, sizeof(set_str), "Organization %c", i + 'A');
    _papplTestBegin("api: papplPrinterGet/SetOrganization('%s')", set_str);
    papplPrinterSetOrganization(printer, set_str);
    if (!papplPrinterGetOrganization(printer, get_str, sizeof(get_str)))
    {
      _papplTestEndMessage(false, "got NULL, expected '%s'", set_str);
      pass = false;
    }
    else if (strcmp(get_str, set_str))
    {
      _papplTestEndMessage(false, "got '%s', expected '%s'", get_str, set_str);
      pass = false;
    }
    else
      _papplTestEnd(true);
  }

  _papplTestBegin("api: papplPrinterGet/SetOrganization(NULL)");
  papplPrinterSetOrganization(printer, NULL);
  if (papplPrinterGetOrganization(printer, get_str, sizeof(get_str)))
  {
    _papplTestEndMessage(false, "got '%s', expected NULL", get_str);
    pass = false;
  }
  else
    _papplTestEnd(true);

  // papplPrinterGet/SetOrganizationalUnit
  _papplTestBegin("api: papplPrinterGetOrganizationalUnit");
  if (papplPrinterGetOrganizationalUnit(printer, get_str, sizeof(get_str)))
  {
    _papplTestEndMessage(false, "got '%s', expected NULL", get_str);
    pass = false;
  }
  else
    _papplTestEnd(true);

  for (i = 0; i < 10; i ++)
  {
    snprintf(set_str, sizeof(set_str), "%c Team", i + 'A');
    _papplTestBegin("api: papplPrinterGet/SetOrganizationalUnit('%s')", set_str);
    papplPrinterSetOrganizationalUnit(printer, set_str);
    if (!papplPrinterGetOrganizationalUnit(printer, get_str, sizeof(get_str)))
    {
      _papplTestEndMessage(false, "got NULL, expected '%s'", set_str);
      pass = false;
    }
    else if (strcmp(get_str, set_str))
    {
      _papplTestEndMessage(false, "got '%s', expected '%s'", get_str, set_str);
      pass = false;
    }
    else
      _papplTestEnd(true);
  }

  _papplTestBegin("api: papplPrinterGet/SetOrganizationalUnit(NULL)");
  papplPrinterSetOrganizationalUnit(printer, NULL);
  if (papplPrinterGetOrganizationalUnit(printer, get_str, sizeof(get_str)))
  {
    _papplTestEndMessage(false, "got '%s', expected NULL", get_str);
    pass = false;
  }
  else
    _papplTestEnd(true);

  return (pass);
}


//
// 'test_api_printer_cb()' - Iterator callback for testing printers.
//

static bool				// O - `true` to continue
test_api_printer_cb(
    pappl_printer_t      *printer,	// I - Printer
    _pappl_testprinter_t *tp)		// I - Printer test data
{
  tp->count ++;

  if (!printer)
    tp->pass = false;
  else if (!papplPrinterGetName(printer))
    tp->pass = false;
  else
  {
    char	get_str[128];		// Location string

    papplPrinterSetLocation(printer, "Nowhere");
    if (!papplPrinterGetLocation(printer, get_str, sizeof(get_str)) || strcmp(get_str, "Nowhere"))
      tp->pass = false;
  }

  return (true);
}


//
// 'test_client()' - Run simulated client tests.
//

static bool				// O - `true` on success, `false` on failure
test_client(pappl_system_t *system)	// I - System
{
  bool		ret = false;		// Return value
  http_t	*http;			// HTTP connection
  char		uri[1024],		// "printer-uri" value
		filename[1024] = "",	// Print file
	        outfile[1024];		// Output file
  ipp_t		*request,		// Request
		*response,		// Response
		*supported = NULL;	// Supported values
  ipp_attribute_t *attr;		// Attribute
  pappl_event_t	recv_events = PAPPL_EVENT_NONE;
					// Accumulated events
  int		i,			// Looping var
		job_id,			// "job-id" value
		subscription_id;	// "notify-subscription-id" value
  ipp_jstate_t	job_state;		// "job-state" value
  time_t	end;			// End time
  int		interval;		// Status query interval
  static const char * const events[] =	// "notify-events" attribute
  {
    "job-completed",
    "job-created",
    "job-progress",
    "job-state-changed",
    "printer-created",
    "printer-deleted",
    "printer-config-changed",
    "printer-state-changed"
  };
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
  _papplTestBegin("client: Connect to server");
  if ((http = connect_to_printer(system, false, uri, sizeof(uri))) == NULL)
  {
    _papplTestEndMessage(false, "%s", cupsGetErrorString());
    return (false);
  }
  else
  {
    _papplTestEnd(true);
  }

  // Test Get-System-Attributes
  _papplTestBegin("client: Get-System-Attributes");

  request = ippNewRequest(IPP_OP_GET_SYSTEM_ATTRIBUTES);
  ippAddString(request, IPP_TAG_OPERATION, IPP_CONST_TAG(IPP_TAG_URI), "system-uri", NULL, "ipp://localhost/ipp/system");
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name", NULL, cupsGetUser());

  response = cupsDoRequest(http, request, "/ipp/system");

  if (cupsGetError() != IPP_STATUS_OK)
  {
    _papplTestEndMessage(false, "%s", cupsGetErrorString());
    ippDelete(response);
    goto done;
  }
  else
  {
    for (i = 0; i < (int)(sizeof(sattrs) / sizeof(sattrs[0])); i ++)
    {
      if (!ippFindAttribute(response, sattrs[i], IPP_TAG_ZERO))
      {
	_papplTestEndMessage(false, "Missing required '%s' attribute in response", sattrs[i]);
	ippDelete(response);
	goto done;
      }
    }

    _papplTestEnd(true);
    ippDelete(response);
  }

  // Test Get-Printers
  _papplTestBegin("client: Get-Printers");

  request = ippNewRequest(IPP_OP_GET_PRINTERS);
  ippAddString(request, IPP_TAG_OPERATION, IPP_CONST_TAG(IPP_TAG_URI), "system-uri", NULL, "ipp://localhost/ipp/system");
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name", NULL, cupsGetUser());

  response = cupsDoRequest(http, request, "/ipp/system");

  if (cupsGetError() != IPP_STATUS_OK)
  {
    _papplTestEndMessage(false, "%s", cupsGetErrorString());
    ippDelete(response);
    goto done;
  }
  else
  {
    for (i = 0; i < (int)(sizeof(pattrs) / sizeof(pattrs[0])); i ++)
    {
      if (!ippFindAttribute(response, pattrs[i], IPP_TAG_ZERO))
      {
	_papplTestEndMessage(false, "Missing required '%s' attribute in response", pattrs[i]);
	ippDelete(response);
	goto done;
      }
    }

    _papplTestEnd(true);
    ippDelete(response);
  }

  // Test Get-Printer-Attributes on /
  _papplTestBegin("client: Get-Printer-Attributes=/");

  request = ippNewRequest(IPP_OP_GET_PRINTER_ATTRIBUTES);
  ippAddString(request, IPP_TAG_OPERATION, IPP_CONST_TAG(IPP_TAG_URI), "printer-uri", NULL, "ipp://localhost/");
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name", NULL, cupsGetUser());

  response = cupsDoRequest(http, request, "/");

  if (cupsGetError() != IPP_STATUS_OK)
  {
    _papplTestEndMessage(false, "%s", cupsGetErrorString());
    ippDelete(response);
    goto done;
  }
  else
  {
    for (i = 0; i < (int)(sizeof(pattrs) / sizeof(pattrs[0])); i ++)
    {
      if (!ippFindAttribute(response, pattrs[i], IPP_TAG_ZERO))
      {
	_papplTestEndMessage(false, "Missing required '%s' attribute in response", pattrs[i]);
	ippDelete(response);
        goto done;
      }
    }

    _papplTestEnd(true);
    ippDelete(response);
  }

  // Test Get-Printer-Attributes on /ipp/print
  _papplTestBegin("client: Get-Printer-Attributes=/ipp/print");

  request = ippNewRequest(IPP_OP_GET_PRINTER_ATTRIBUTES);
  ippAddString(request, IPP_TAG_OPERATION, IPP_CONST_TAG(IPP_TAG_URI), "printer-uri", NULL, "ipp://localhost/ipp/print");
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name", NULL, cupsGetUser());

  supported = cupsDoRequest(http, request, "/ipp/print");

  if (cupsGetError() != IPP_STATUS_OK)
  {
    _papplTestEndMessage(false, "%s", cupsGetErrorString());
    goto done;
  }
  else
  {
    for (i = 0; i < (int)(sizeof(pattrs) / sizeof(pattrs[0])); i ++)
    {
      if (!ippFindAttribute(supported, pattrs[i], IPP_TAG_ZERO))
      {
	_papplTestEndMessage(false, "Missing required '%s' attribute in response", pattrs[i]);
	goto done;
      }
    }

    _papplTestEnd(true);
  }

  // Create a system subscription for a variety of events...
  _papplTestBegin("client: Create-System-Subscriptions");

  request = ippNewRequest(IPP_OP_CREATE_SYSTEM_SUBSCRIPTIONS);
  ippAddString(request, IPP_TAG_OPERATION, IPP_CONST_TAG(IPP_TAG_URI), "system-uri", NULL, "ipp://localhost/ipp/system");
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name", NULL, cupsGetUser());

  ippAddStrings(request, IPP_TAG_SUBSCRIPTION, IPP_CONST_TAG(IPP_TAG_KEYWORD), "notify-events", sizeof(events) / sizeof(events[0]), NULL, events);
  ippAddInteger(request, IPP_TAG_SUBSCRIPTION, IPP_TAG_INTEGER, "notify-lease-duration", 60);
  ippAddString(request, IPP_TAG_SUBSCRIPTION, IPP_CONST_TAG(IPP_TAG_KEYWORD), "notify-pull-method", NULL, "ippget");

  response        = cupsDoRequest(http, request, "/ipp/system");
  subscription_id = ippGetInteger(ippFindAttribute(response, "notify-subscription-id", IPP_TAG_INTEGER), 0);
  ippDelete(response);

  if (cupsGetError() != IPP_STATUS_OK)
  {
    _papplTestEndMessage(false, "%s", cupsGetErrorString());
    goto done;
  }
  else if (subscription_id == 0)
  {
    _papplTestEndMessage(false, "missing required 'notify-subscription-id' attribute in response");
    goto done;
  }
  else
  {
    _papplTestEndMessage(true, "notify-subscription-id=%d", subscription_id);
  }

  end = time(NULL) + 70;

  // Verify the subscription exists...
  _papplTestBegin("client: Get-Subscription-Attributes");

  request = ippNewRequest(IPP_OP_GET_SUBSCRIPTION_ATTRIBUTES);
  ippAddString(request, IPP_TAG_OPERATION, IPP_CONST_TAG(IPP_TAG_URI), "system-uri", NULL, "ipp://localhost/ipp/system");
  ippAddInteger(request, IPP_TAG_OPERATION, IPP_TAG_INTEGER, "notify-subscription-id", subscription_id);
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name", NULL, cupsGetUser());

  response = cupsDoRequest(http, request, "/ipp/system");
  attr     = ippFindAttribute(response, "notify-events", IPP_TAG_KEYWORD);
  ippDelete(response);

  if (cupsGetError() != IPP_STATUS_OK)
  {
    printf("FAIL(%s)\n", cupsGetErrorString());
    goto done;
  }
  else if (!attr)
  {
    _papplTestEndMessage(false, "missing 'notify-events' attribute");
    goto done;
  }
  else
  {
    _papplTestEnd(true);
  }

  // Send a print job to get some events...
  _papplTestBegin("client: Make raster print file");
  if (!make_raster_file(supported, false, filename, sizeof(filename)))
    goto done;
  _papplTestEnd(true);

  _papplTestBegin("client: Print-Job (Raster)");
  request = ippNewRequest(IPP_OP_PRINT_JOB);
  ippAddString(request, IPP_TAG_OPERATION, IPP_CONST_TAG(IPP_TAG_URI), "printer-uri", NULL, "ipp://localhost/ipp/print");
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name", NULL, cupsGetUser());
  ippAddString(request, IPP_TAG_OPERATION, IPP_CONST_TAG(IPP_TAG_MIMETYPE), "document-format", NULL, "image/pwg-raster");
  ippAddString(request, IPP_TAG_OPERATION, IPP_CONST_TAG(IPP_TAG_NAME), "job-name", NULL, "Client Test Raster Job");

  response = cupsDoFileRequest(http, request, "/ipp/print", filename);
  job_id   = ippGetInteger(ippFindAttribute(response, "job-id", IPP_TAG_INTEGER), 0);

  ippDelete(response);

  if (cupsGetError() >= IPP_STATUS_ERROR_BAD_REQUEST)
  {
    _papplTestEndMessage(false, "%s", cupsGetErrorString());
    goto done;
  }

  _papplTestEndMessage(true, "job-id=%d", job_id);
  output_count ++;

#ifdef HAVE_LIBJPEG
  _papplTestBegin("client: Print-Job (JPEG w/hold)");
  request = ippNewRequest(IPP_OP_PRINT_JOB);
  ippAddString(request, IPP_TAG_OPERATION, IPP_CONST_TAG(IPP_TAG_URI), "printer-uri", NULL, "ipp://localhost/ipp/print");
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name", NULL, cupsGetUser());
  ippAddString(request, IPP_TAG_OPERATION, IPP_CONST_TAG(IPP_TAG_MIMETYPE), "document-format", NULL, "image/jpeg");
  ippAddString(request, IPP_TAG_OPERATION, IPP_CONST_TAG(IPP_TAG_NAME), "job-name", NULL, "Client Test JPEG Job with Hold");
  ippAddString(request, IPP_TAG_JOB, IPP_CONST_TAG(IPP_TAG_KEYWORD), "job-hold-until", NULL, "indefinite");

  if (access("portrait-color.jpg", R_OK))
    cupsCopyString(filename, "testsuite/portrait-color.jpg", sizeof(filename));
  else
    cupsCopyString(filename, "portrait-color.jpg", sizeof(filename));

  response = cupsDoFileRequest(http, request, "/ipp/print", filename);
  job_id   = ippGetInteger(ippFindAttribute(response, "job-id", IPP_TAG_INTEGER), 0);

  ippDelete(response);

  if (cupsGetError() >= IPP_STATUS_ERROR_BAD_REQUEST)
  {
    _papplTestEndMessage(false, "%s", cupsGetErrorString());
    goto done;
  }

  _papplTestEndMessage(true, "job-id=%d", job_id);

  _papplTestBegin("client: Release-Job (JPEG)");
  request = ippNewRequest(IPP_OP_RELEASE_JOB);
  ippAddString(request, IPP_TAG_OPERATION, IPP_CONST_TAG(IPP_TAG_URI), "printer-uri", NULL, "ipp://localhost/ipp/print");
  ippAddInteger(request, IPP_TAG_OPERATION, IPP_TAG_INTEGER, "job-id", job_id);
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name", NULL, cupsGetUser());

  ippDelete(cupsDoRequest(http, request, "/ipp/print"));

  if (cupsGetError() >= IPP_STATUS_ERROR_BAD_REQUEST)
  {
    _papplTestEndMessage(false, "%s: %s", ippErrorString(cupsGetError()), cupsGetErrorString());
    goto done;
  }

  _papplTestEnd(true);

  _papplTestBegin("client: Get-Job-Attributes (JPEG)");
  interval = 1;
  do
  {
    request = ippNewRequest(IPP_OP_GET_JOB_ATTRIBUTES);
    ippAddString(request, IPP_TAG_OPERATION, IPP_CONST_TAG(IPP_TAG_URI), "printer-uri", NULL, "ipp://localhost/ipp/print");
    ippAddInteger(request, IPP_TAG_OPERATION, IPP_TAG_INTEGER, "job-id", job_id);
    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name", NULL, cupsGetUser());

    response  = cupsDoRequest(http, request, "/ipp/print");
    job_state = (ipp_jstate_t)ippGetInteger(ippFindAttribute(response, "job-state", IPP_TAG_ENUM), 0);
    ippDelete(response);

    if (cupsGetError() == IPP_STATUS_OK && job_state < IPP_JSTATE_CANCELED)
    {
      _papplTestProgress();
      sleep(_PAPPL_FIB_VALUE(interval));
      interval = _PAPPL_FIB_NEXT(interval);
    }
  }
  while (cupsGetError() == IPP_STATUS_OK && job_state < IPP_JSTATE_CANCELED);

  if (cupsGetError() >= IPP_STATUS_ERROR_BAD_REQUEST)
  {
    _papplTestEndMessage(false, "%s", cupsGetErrorString());
    goto done;
  }

  _papplTestEndMessage(job_state == IPP_JSTATE_COMPLETED, "job-state=%s", ippEnumString("job-state", (int)job_state));
  output_count ++;

  _papplTestBegin("client: Create-Job (Multi-JPEG)");
  request = ippNewRequest(IPP_OP_CREATE_JOB);
  ippAddString(request, IPP_TAG_OPERATION, IPP_CONST_TAG(IPP_TAG_URI), "printer-uri", NULL, "ipp://localhost/ipp/print");
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name", NULL, cupsGetUser());
  ippAddString(request, IPP_TAG_OPERATION, IPP_CONST_TAG(IPP_TAG_NAME), "job-name", NULL, "Client Test Multi-JPEG Job");

  response = cupsDoRequest(http, request, "/ipp/print");
  job_id   = ippGetInteger(ippFindAttribute(response, "job-id", IPP_TAG_INTEGER), 0);

  ippDelete(response);

  if (cupsGetError() >= IPP_STATUS_ERROR_BAD_REQUEST)
  {
    _papplTestEndMessage(false, "%s", cupsGetErrorString());
    goto done;
  }

  _papplTestEndMessage(true, "job-id=%d", job_id);

  static const char * const jpeg_files[] =
  {
    "portrait-color.jpg",
    "portrait-gray.jpg",
    "landscape-color.jpg",
    "landscape-gray.jpg"
  };

  for (i = 0; i < (int)(sizeof(jpeg_files) / sizeof(jpeg_files[0])); i ++)
  {
    _papplTestBegin("client: Send-Document (%s)", jpeg_files[i]);

    request = ippNewRequest(IPP_OP_SEND_DOCUMENT);
    ippAddString(request, IPP_TAG_OPERATION, IPP_CONST_TAG(IPP_TAG_URI), "printer-uri", NULL, "ipp://localhost/ipp/print");
    ippAddInteger(request, IPP_TAG_OPERATION, IPP_TAG_INTEGER, "job-id", job_id);
    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name", NULL, cupsGetUser());
    ippAddString(request, IPP_TAG_OPERATION, IPP_CONST_TAG(IPP_TAG_MIMETYPE), "document-format", NULL, "image/jpeg");
    ippAddString(request, IPP_TAG_OPERATION, IPP_CONST_TAG(IPP_TAG_NAME), "document-name", NULL, jpeg_files[i]);
    ippAddBoolean(request, IPP_TAG_OPERATION, "last-document", i == (int)(sizeof(jpeg_files) / sizeof(jpeg_files[0]) - 1));

    if (access(jpeg_files[i], R_OK))
      snprintf(filename, sizeof(filename), "testsuite/%s", jpeg_files[i]);
    else
      cupsCopyString(filename, jpeg_files[i], sizeof(filename));

    ippDelete(cupsDoFileRequest(http, request, "/ipp/print", filename));

    if (cupsGetError() >= IPP_STATUS_ERROR_BAD_REQUEST)
    {
      _papplTestEndMessage(false, "%s", cupsGetErrorString());
      goto done;
    }

    _papplTestEnd(true);
  }

  _papplTestBegin("client: Get-Job-Attributes (Multi-JPEG)");
  interval = 1;
  do
  {
    request = ippNewRequest(IPP_OP_GET_JOB_ATTRIBUTES);
    ippAddString(request, IPP_TAG_OPERATION, IPP_CONST_TAG(IPP_TAG_URI), "printer-uri", NULL, "ipp://localhost/ipp/print");
    ippAddInteger(request, IPP_TAG_OPERATION, IPP_TAG_INTEGER, "job-id", job_id);
    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name", NULL, cupsGetUser());

    response  = cupsDoRequest(http, request, "/ipp/print");
    job_state = (ipp_jstate_t)ippGetInteger(ippFindAttribute(response, "job-state", IPP_TAG_ENUM), 0);
    ippDelete(response);

    if (cupsGetError() == IPP_STATUS_OK && job_state < IPP_JSTATE_CANCELED)
    {
      _papplTestProgress();
      sleep(_PAPPL_FIB_VALUE(interval));
      interval = _PAPPL_FIB_NEXT(interval);
    }
  }
  while (cupsGetError() == IPP_STATUS_OK && job_state < IPP_JSTATE_CANCELED);

  if (cupsGetError() >= IPP_STATUS_ERROR_BAD_REQUEST)
  {
    _papplTestEndMessage(false, "%s", cupsGetErrorString());
    goto done;
  }

  _papplTestEndMessage(job_state == IPP_JSTATE_COMPLETED, "job-state=%s", ippEnumString("job-state", (int)job_state));
  output_count ++;
#endif // HAVE_LIBJPEG

#ifdef HAVE_LIBPNG
  _papplTestBegin("client: Print-Job (PNG w/Hold)");
  request = ippNewRequest(IPP_OP_PRINT_JOB);
  ippAddString(request, IPP_TAG_OPERATION, IPP_CONST_TAG(IPP_TAG_URI), "printer-uri", NULL, "ipp://localhost/ipp/print");
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name", NULL, cupsGetUser());
  ippAddString(request, IPP_TAG_OPERATION, IPP_CONST_TAG(IPP_TAG_MIMETYPE), "document-format", NULL, "image/png");
  ippAddString(request, IPP_TAG_OPERATION, IPP_CONST_TAG(IPP_TAG_NAME), "job-name", NULL, "Client Test PNG Job with Hold");
  ippAddString(request, IPP_TAG_JOB, IPP_CONST_TAG(IPP_TAG_KEYWORD), "job-hold-until", NULL, "indefinite");

  if (access("portrait-color.png", R_OK))
    cupsCopyString(filename, "testsuite/portrait-color.png", sizeof(filename));
  else
    cupsCopyString(filename, "portrait-color.png", sizeof(filename));

  response = cupsDoFileRequest(http, request, "/ipp/print", filename);
  job_id   = ippGetInteger(ippFindAttribute(response, "job-id", IPP_TAG_INTEGER), 0);

  ippDelete(response);

  if (cupsGetError() >= IPP_STATUS_ERROR_BAD_REQUEST)
  {
    _papplTestEndMessage(false, "%s", cupsGetErrorString());
    goto done;
  }

  _papplTestEndMessage(true, "job-id=%d", job_id);

  _papplTestBegin("client: Release-Job (PNG)");
  request = ippNewRequest(IPP_OP_RELEASE_JOB);
  ippAddString(request, IPP_TAG_OPERATION, IPP_CONST_TAG(IPP_TAG_URI), "printer-uri", NULL, "ipp://localhost/ipp/print");
  ippAddInteger(request, IPP_TAG_OPERATION, IPP_TAG_INTEGER, "job-id", job_id);
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name", NULL, cupsGetUser());

  ippDelete(cupsDoRequest(http, request, "/ipp/print"));

  if (cupsGetError() >= IPP_STATUS_ERROR_BAD_REQUEST)
  {
    _papplTestEndMessage(false, "%s", cupsGetErrorString());
    goto done;
  }

  _papplTestEnd(true);

  _papplTestBegin("client: Get-Job-Attributes (PNG)");
  interval = 1;
  do
  {
    request = ippNewRequest(IPP_OP_GET_JOB_ATTRIBUTES);
    ippAddString(request, IPP_TAG_OPERATION, IPP_CONST_TAG(IPP_TAG_URI), "printer-uri", NULL, "ipp://localhost/ipp/print");
    ippAddInteger(request, IPP_TAG_OPERATION, IPP_TAG_INTEGER, "job-id", job_id);
    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name", NULL, cupsGetUser());

    response  = cupsDoRequest(http, request, "/ipp/print");
    job_state = (ipp_jstate_t)ippGetInteger(ippFindAttribute(response, "job-state", IPP_TAG_ENUM), 0);
    ippDelete(response);

    if (cupsGetError() == IPP_STATUS_OK && job_state < IPP_JSTATE_CANCELED)
    {
      _papplTestProgress();
      sleep(_PAPPL_FIB_VALUE(interval));
      interval = _PAPPL_FIB_NEXT(interval);
    }
  }
  while (cupsGetError() == IPP_STATUS_OK && job_state < IPP_JSTATE_CANCELED);

  if (cupsGetError() >= IPP_STATUS_ERROR_BAD_REQUEST)
  {
    _papplTestEndMessage(false, "%s", cupsGetErrorString());
    goto done;
  }

  _papplTestEndMessage(job_state == IPP_JSTATE_COMPLETED, "job-state=%s", ippEnumString("job-state", (int)job_state));
  output_count ++;
#endif // HAVE_LIBPNG

  // Hold-New-Jobs
  _papplTestBegin("client: Hold-New-Jobs");
  request = ippNewRequest(IPP_OP_HOLD_NEW_JOBS);
  ippAddString(request, IPP_TAG_OPERATION, IPP_CONST_TAG(IPP_TAG_URI), "printer-uri", NULL, "ipp://localhost/ipp/print");
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name", NULL, cupsGetUser());

  ippDelete(cupsDoRequest(http, request, "/ipp/print"));

  if (cupsGetError() >= IPP_STATUS_ERROR_BAD_REQUEST)
  {
    _papplTestEndMessage(false, "%s", cupsGetErrorString());
    goto done;
  }

  _papplTestEnd(true);

  _papplTestBegin("client: Print-Job (Raster 2)");
  request = ippNewRequest(IPP_OP_PRINT_JOB);
  ippAddString(request, IPP_TAG_OPERATION, IPP_CONST_TAG(IPP_TAG_URI), "printer-uri", NULL, "ipp://localhost/ipp/print");
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name", NULL, cupsGetUser());
  ippAddString(request, IPP_TAG_OPERATION, IPP_CONST_TAG(IPP_TAG_MIMETYPE), "document-format", NULL, "image/pwg-raster");
  ippAddString(request, IPP_TAG_OPERATION, IPP_CONST_TAG(IPP_TAG_NAME), "job-name", NULL, "Client Test Raster Job 2");

  ippDelete(cupsDoFileRequest(http, request, "/ipp/print", filename));

  if (cupsGetError() == IPP_STATUS_OK)
  {
    _papplTestEndMessage(false, "Job accepted but should have been rejected.");
    goto done;
  }

  sleep(1);
  snprintf(outfile, sizeof(outfile), "%s/Client Test Raster Job 2.pwg", output_directory);
  if (!access(outfile, 0))
  {
    _papplTestEndMessage(false, "Unexpected job output file created.");
    goto done;
  }

  _papplTestEnd(true);

#ifdef HAVE_LIBJPEG
  _papplTestBegin("client: Print-Job (JPEG w/o hold)");
  request = ippNewRequest(IPP_OP_PRINT_JOB);
  ippAddString(request, IPP_TAG_OPERATION, IPP_CONST_TAG(IPP_TAG_URI), "printer-uri", NULL, "ipp://localhost/ipp/print");
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name", NULL, cupsGetUser());
  ippAddString(request, IPP_TAG_OPERATION, IPP_CONST_TAG(IPP_TAG_MIMETYPE), "document-format", NULL, "image/jpeg");
  ippAddString(request, IPP_TAG_OPERATION, IPP_CONST_TAG(IPP_TAG_NAME), "job-name", NULL, "Client Test JPEG Job without Hold");

  if (access("portrait-color.jpg", R_OK))
    cupsCopyString(filename, "testsuite/portrait-color.jpg", sizeof(filename));
  else
    cupsCopyString(filename, "portrait-color.jpg", sizeof(filename));

  response  = cupsDoFileRequest(http, request, "/ipp/print", filename);
  job_id    = ippGetInteger(ippFindAttribute(response, "job-id", IPP_TAG_INTEGER), 0);
  job_state = (ipp_jstate_t)ippGetInteger(ippFindAttribute(response, "job-state", IPP_TAG_ENUM), 0);

  ippDelete(response);

  if (cupsGetError() >= IPP_STATUS_ERROR_BAD_REQUEST)
  {
    _papplTestEndMessage(false, "%s", cupsGetErrorString());
    goto done;
  }
  else if (job_state != IPP_JSTATE_HELD)
  {
    _papplTestEndMessage(false, "job-state is %s, expected pending-held", ippEnumString("job-state", (int)job_state));
    goto done;
  }

  sleep(1);
  snprintf(outfile, sizeof(outfile), "%s/Client Test JPEG Job without Hold.pwg", output_directory);
  if (!access(outfile, 0))
  {
    _papplTestEndMessage(false, "Unexpected job output file created.");
    goto done;
  }

  _papplTestEndMessage(true, "job-id=%d", job_id);
  output_count ++;
#endif // HAVE_LIBJPEG

  if (ippContainsString(ippFindAttribute(supported, "document-format-supported", IPP_TAG_MIMETYPE), "application/pdf"))
  {
    _papplTestBegin("client: Print-Job (PDF w/o Hold)");
    request = ippNewRequest(IPP_OP_PRINT_JOB);
    ippAddString(request, IPP_TAG_OPERATION, IPP_CONST_TAG(IPP_TAG_URI), "printer-uri", NULL, "ipp://localhost/ipp/print");
    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name", NULL, cupsGetUser());
    ippAddString(request, IPP_TAG_OPERATION, IPP_CONST_TAG(IPP_TAG_MIMETYPE), "document-format", NULL, "application/pdf");
    ippAddString(request, IPP_TAG_OPERATION, IPP_CONST_TAG(IPP_TAG_NAME), "job-name", NULL, "Client Test PDF Job without Hold");

    if (access("portrait-color-letter.pdf", R_OK))
      cupsCopyString(filename, "testsuite/portrait-color-letter.pdf", sizeof(filename));
    else
      cupsCopyString(filename, "portrait-color-letter.pdf", sizeof(filename));

    response  = cupsDoFileRequest(http, request, "/ipp/print", filename);
    job_id    = ippGetInteger(ippFindAttribute(response, "job-id", IPP_TAG_INTEGER), 0);
    job_state = (ipp_jstate_t)ippGetInteger(ippFindAttribute(response, "job-state", IPP_TAG_ENUM), 0);

    ippDelete(response);

    if (cupsGetError() >= IPP_STATUS_ERROR_BAD_REQUEST)
    {
      _papplTestEndMessage(false, "%s", cupsGetErrorString());
      goto done;
    }
    else if (job_state != IPP_JSTATE_HELD)
    {
      _papplTestEndMessage(false, "job-state is %s, expected pending-held", ippEnumString("job-state", (int)job_state));
      goto done;
    }

    sleep(1);
    snprintf(outfile, sizeof(outfile), "%s/Client Test PDF Job without Hold.pwg", output_directory);
    if (!access(outfile, 0))
    {
      _papplTestEndMessage(false, "Unexpected job output file created.");
      goto done;
    }

    _papplTestEndMessage(true, "job-id=%d", job_id);
    output_count ++;
  }

#ifdef HAVE_LIBPNG
  _papplTestBegin("client: Print-Job (PNG w/o Hold)");
  request = ippNewRequest(IPP_OP_PRINT_JOB);
  ippAddString(request, IPP_TAG_OPERATION, IPP_CONST_TAG(IPP_TAG_URI), "printer-uri", NULL, "ipp://localhost/ipp/print");
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name", NULL, cupsGetUser());
  ippAddString(request, IPP_TAG_OPERATION, IPP_CONST_TAG(IPP_TAG_MIMETYPE), "document-format", NULL, "image/png");
  ippAddString(request, IPP_TAG_OPERATION, IPP_CONST_TAG(IPP_TAG_NAME), "job-name", NULL, "Client Test PNG Job without Hold");

  if (access("portrait-color.png", R_OK))
    cupsCopyString(filename, "testsuite/portrait-color.png", sizeof(filename));
  else
    cupsCopyString(filename, "portrait-color.png", sizeof(filename));

  response  = cupsDoFileRequest(http, request, "/ipp/print", filename);
  job_id    = ippGetInteger(ippFindAttribute(response, "job-id", IPP_TAG_INTEGER), 0);
  job_state = (ipp_jstate_t)ippGetInteger(ippFindAttribute(response, "job-state", IPP_TAG_ENUM), 0);

  ippDelete(response);

  if (cupsGetError() >= IPP_STATUS_ERROR_BAD_REQUEST)
  {
    _papplTestEndMessage(false, "%s", cupsGetErrorString());
    goto done;
  }
  else if (job_state != IPP_JSTATE_HELD)
  {
    _papplTestEndMessage(false, "job-state is %s, expected pending-held", ippEnumString("job-state", (int)job_state));
    goto done;
  }

  sleep(1);
  snprintf(outfile, sizeof(outfile), "%s/Client Test PNG Job without Hold.pwg", output_directory);
  if (!access(outfile, 0))
  {
    _papplTestEndMessage(false, "Unexpected job output file created.");
    goto done;
  }

  _papplTestEndMessage(true, "job-id=%d", job_id);
  output_count ++;
#endif // HAVE_LIBPNG

  // Release-Held-New-Jobs
  _papplTestBegin("client: Release-Held-New-Jobs");
  request = ippNewRequest(IPP_OP_RELEASE_HELD_NEW_JOBS);
  ippAddString(request, IPP_TAG_OPERATION, IPP_CONST_TAG(IPP_TAG_URI), "printer-uri", NULL, "ipp://localhost/ipp/print");
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name", NULL, cupsGetUser());

  ippDelete(cupsDoRequest(http, request, "/ipp/print"));

  if (cupsGetError() >= IPP_STATUS_ERROR_BAD_REQUEST)
  {
    _papplTestEndMessage(false, "%s", cupsGetErrorString());
    goto done;
  }

  _papplTestEnd(true);

  // Get event notifications...
  _papplTestBegin("client: Get-Notifications");

  request = ippNewRequest(IPP_OP_GET_NOTIFICATIONS);
  ippAddString(request, IPP_TAG_OPERATION, IPP_CONST_TAG(IPP_TAG_URI), "system-uri", NULL, "ipp://localhost/ipp/system");
  ippAddInteger(request, IPP_TAG_OPERATION, IPP_TAG_INTEGER, "notify-subscription-ids", subscription_id);
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name", NULL, cupsGetUser());

  response = cupsDoRequest(http, request, "/ipp/system");

  for (attr = ippFindAttribute(response, "notify-subscribed-event", IPP_TAG_KEYWORD); attr; attr = ippFindNextAttribute(response, "notify-subscribed-event", IPP_TAG_KEYWORD))
  {
    const char *keyword = ippGetString(attr, 0, NULL);
					// "notify-subscribed-event" keyword value

    if (!strcmp(keyword, "job-created"))
    {
      recv_events |= PAPPL_EVENT_JOB_CREATED;
    }
    else if (!strcmp(keyword, "job-completed"))
    {
      recv_events |= PAPPL_EVENT_JOB_COMPLETED;
    }
    else if (!strcmp(keyword, "job-progress"))
    {
      recv_events |= PAPPL_EVENT_JOB_PROGRESS;
    }
    else if (!strcmp(keyword, "job-state-changed"))
    {
      recv_events |= PAPPL_EVENT_JOB_STATE_CHANGED;
    }
    else if (!strcmp(keyword, "printer-config-changed"))
    {
      recv_events |= PAPPL_EVENT_PRINTER_CONFIG_CHANGED;
    }
    else if (!strcmp(keyword, "printer-state-changed"))
    {
      recv_events |= PAPPL_EVENT_PRINTER_STATE_CHANGED;
    }
    else
    {
      _papplTestEndMessage(false, "Unexpected event '%s'", keyword);
      ippDelete(response);
      goto done;
    }
  }

  ippDelete(response);

  if (cupsGetError() != IPP_STATUS_OK)
  {
    _papplTestEndMessage(false, "%s", cupsGetErrorString());
    goto done;
  }
  else if (recv_events != (PAPPL_EVENT_JOB_COMPLETED | PAPPL_EVENT_JOB_CREATED | PAPPL_EVENT_JOB_PROGRESS | PAPPL_EVENT_JOB_STATE_CHANGED | PAPPL_EVENT_PRINTER_CONFIG_CHANGED | PAPPL_EVENT_PRINTER_STATE_CHANGED))
  {
    _papplTestEndMessage(false, "wrong events seen");
    goto done;
  }
  else
  {
    _papplTestEnd(true);
  }

  // PAPPL-Find-Devices
  _papplTestBegin("client: PAPPL-Find-Devices");
  request = ippNewRequest(PAPPL_IPP_OP_FIND_DEVICES);
  ippAddString(request, IPP_TAG_OPERATION, IPP_CONST_TAG(IPP_TAG_URI), "system-uri", NULL, "ipp://localhost/ipp/system");

  response = cupsDoRequest(http, request, "/ipp/system");

  if ((attr = ippFindAttribute(response, "smi55357-device-col", IPP_TAG_BEGIN_COLLECTION)) != NULL)
    _papplTestEndMessage(true, "%u devices found", (unsigned)ippGetCount(attr));
  else if (cupsGetError() == IPP_STATUS_ERROR_NOT_FOUND)
    _papplTestEndMessage(true, "no devices found");
  else
    _papplTestEndMessage(false, "failed: %s", cupsGetErrorString());

  ippDelete(response);

  // PAPPL-Find-Drivers
  _papplTestBegin("client: PAPPL-Find-Drivers");
  request = ippNewRequest(PAPPL_IPP_OP_FIND_DRIVERS);
  ippAddString(request, IPP_TAG_OPERATION, IPP_CONST_TAG(IPP_TAG_URI), "system-uri", NULL, "ipp://localhost/ipp/system");

  response = cupsDoRequest(http, request, "/ipp/system");

  if ((attr = ippFindAttribute(response, "smi55357-driver-col", IPP_TAG_BEGIN_COLLECTION)) != NULL)
    _papplTestEndMessage(true, "%u drivers found", (unsigned)ippGetCount(attr));
  else
    _papplTestEndMessage(false, "failed: %s", cupsGetErrorString());

  ippDelete(response);

  // PAPPL-Find-Drivers (good device-id)
  _papplTestBegin("client: PAPPL-Find-Drivers (good device-id)");
  request = ippNewRequest(PAPPL_IPP_OP_FIND_DRIVERS);
  ippAddString(request, IPP_TAG_OPERATION, IPP_CONST_TAG(IPP_TAG_URI), "system-uri", NULL, "ipp://localhost/ipp/system");
  ippAddString(request, IPP_TAG_OPERATION, IPP_CONST_TAG(IPP_TAG_TEXT), "smi55357-device-id", NULL, "MFG:Example;MDL:Printer;CMD:PWGRaster;");

  response = cupsDoRequest(http, request, "/ipp/system");

  if ((attr = ippFindAttribute(response, "smi55357-driver-col", IPP_TAG_BEGIN_COLLECTION)) != NULL)
    _papplTestEndMessage(true, "%u drivers found", (unsigned)ippGetCount(attr));
  else
    _papplTestEndMessage(false, "failed: %s", cupsGetErrorString());

  ippDelete(response);

  // PAPPL-Find-Drivers (bad device-id)
  _papplTestBegin("client: PAPPL-Find-Drivers (bad device-id)");
  request = ippNewRequest(PAPPL_IPP_OP_FIND_DRIVERS);
  ippAddString(request, IPP_TAG_OPERATION, IPP_CONST_TAG(IPP_TAG_URI), "system-uri", NULL, "ipp://localhost/ipp/system");
  ippAddString(request, IPP_TAG_OPERATION, IPP_CONST_TAG(IPP_TAG_TEXT), "smi55357-device-id", NULL, "MFG:Example;MDL:Printer;CMD:PCL;");

  response = cupsDoRequest(http, request, "/ipp/system");

  if ((attr = ippFindAttribute(response, "smi55357-driver-col", IPP_TAG_BEGIN_COLLECTION)) != NULL)
    _papplTestEndMessage(false, "%u drivers found", (unsigned)ippGetCount(attr));
  else if (cupsGetError() == IPP_STATUS_ERROR_NOT_FOUND)
    _papplTestEndMessage(true, "no drivers found");
  else
    _papplTestEndMessage(false, "failed: %s", cupsGetErrorString());

  ippDelete(response);

  // Verify that the subscription expires...
  _papplTestBegin("client: Get-Subscription-Attributes(expiration)");
  while (time(NULL) < end)
  {
    _papplTestProgress();
    sleep(5);
  }

  request = ippNewRequest(IPP_OP_GET_SUBSCRIPTION_ATTRIBUTES);
  ippAddString(request, IPP_TAG_OPERATION, IPP_CONST_TAG(IPP_TAG_URI), "system-uri", NULL, "ipp://localhost/ipp/system");
  ippAddInteger(request, IPP_TAG_OPERATION, IPP_TAG_INTEGER, "notify-subscription-id", subscription_id);
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name", NULL, cupsGetUser());

  httpConnectAgain(http, 30000, NULL);

  response = cupsDoRequest(http, request, "/ipp/system");
  attr     = ippFindAttribute(response, "notify-events", IPP_TAG_KEYWORD);
  ippDelete(response);

  if (cupsGetError() != IPP_STATUS_ERROR_NOT_FOUND)
  {
    _papplTestEndMessage(false, "%s", cupsGetErrorString());
    goto done;
  }
  else if (attr)
  {
    _papplTestEndMessage(false, "unexpected 'notify-events' attribute");
    goto done;
  }
  else
  {
    _papplTestEnd(true);
  }

  ret = true;

  // Clean up and return...
  done:

  ippDelete(supported);
  httpClose(http);

  return (ret);
}


//
// 'test_client_child()' - Test client child thread.
//

static void *				// O - Thread exit status
test_client_child(
    _pappl_testclient_t *data)		// I - Client data
{
  size_t	i;			// Looping var
  http_t	*http;			// HTTP connection
  char		printer_uri[1024];	// "printer-uri" value
  ipp_t		*request;		// IPP request
  char		error[1024];		// Error message


  // Connect to the server...
  if ((http = httpConnect("localhost", papplSystemGetHostPort(data->system), /*addrlist*/NULL, AF_UNSPEC, HTTP_ENCRYPTION_NEVER, /*blocking*/true, /*msec*/30000, /*cancel*/NULL)) == NULL)
  {
    snprintf(error, sizeof(error), "Unable to connect to 'localhost:%d': %s", papplSystemGetHostPort(data->system), cupsGetErrorString());
    cupsMutexLock(&data->mutex);
    data->completed_count ++;
    cupsArrayAdd(data->errors, error);
    cupsMutexUnlock(&data->mutex);

    return ((void *)1);
  }

  httpAssembleURI(HTTP_URI_CODING_ALL, printer_uri, sizeof(printer_uri), "ipp", /*userpass*/NULL, "localhost", papplSystemGetHostPort(data->system), "/ipp/print");

  // Do Get-Printer-Attributes requests
  for (i = 0; i < data->num_requests; i ++)
  {
    request = ippNewRequest(IPP_OP_GET_PRINTER_ATTRIBUTES);
    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri", NULL, printer_uri);

    ippDelete(cupsDoRequest(http, request, "/ipp/print"));

    cupsMutexLock(&data->mutex);
    data->request_count ++;
    if (cupsGetError() > IPP_STATUS_OK_EVENTS_COMPLETE)
    {
      snprintf(error, sizeof(error), "Get-Printer-Attributes: %s", cupsGetErrorString());
      cupsArrayAdd(data->errors, error);
    }
    gettimeofday(&data->end, NULL);
    cupsMutexUnlock(&data->mutex);
  }

  httpClose(http);

  cupsMutexLock(&data->mutex);
  data->completed_count ++;
  cupsMutexUnlock(&data->mutex);

  return (NULL);
}


//
// 'test_client_max()' - Test max clients throughput.
//

static bool				// O - Results of test
test_client_max(pappl_system_t *system,	// I - System
                const char     *name)	// I - Test name
{
  size_t		i;		// Looping var
  double		secs;		// Seconds
  cups_thread_t		threads[_PAPPL_MAX_CLIENTS];
					// Threads
  _pappl_testclient_t	data;		// Client data
  size_t		total,		// Total number of requests expected
			current,	// Number of requests
			last = 0;	// Last number of requests
  const char		*ptr;		// Pointer into name
  char			progress[13];	// Progress bar
  int			pcounter = 0;	// Progress counter


  // Don't rotate the logs for this test...
  papplSystemSetMaxLogSize(system, 0);

  // Prepare client data...
  memset(&data, 0, sizeof(data));
  memset(threads, 0, sizeof(threads));
  data.system = system;
  data.errors = cupsArrayNew(/*cb*/NULL, /*cbdata*/NULL, /*hashcb*/NULL, /*hashsize*/0, (cups_acopy_cb_t)strcopy_cb, (cups_afree_cb_t)strfree_cb);
  cupsMutexInit(&data.mutex);

  // Parse the name
  if (!strcmp(name, "client-max"))
  {
    // Test the maximum number of clients and requests
    data.num_children = _PAPPL_DEFAULT_CLIENTS;
    data.num_requests = _PAPPL_DEFAULT_REQUESTS;
  }
  else
  {
    // Parse "client-CLIENTSxREQUESTS"
    if ((data.num_children = (size_t)strtoul(name + 7, (char **)&ptr, 10)) > _PAPPL_MAX_CLIENTS)
    {
      fprintf(stderr, "testpappl: Reducing number of clients from %lu to %d.\n", (unsigned long)data.num_children, _PAPPL_MAX_CLIENTS);
      data.num_children = _PAPPL_MAX_CLIENTS;
    }
    else if (data.num_children == 0)
    {
      fprintf(stderr, "testpappl: Using %d clients.\n", _PAPPL_DEFAULT_CLIENTS);
      data.num_children = _PAPPL_DEFAULT_CLIENTS;
    }

    if (!ptr || *ptr != 'x')
    {
      // Use default number of requests
      data.num_requests = _PAPPL_DEFAULT_REQUESTS;
    }
    else if ((data.num_requests = (size_t)strtoul(ptr + 1, NULL, 10)) > _PAPPL_MAX_REQUESTS)
    {
      fprintf(stderr, "testpappl: Reducing number of requests from %lu to %d.\n", (unsigned long)data.num_requests, _PAPPL_MAX_REQUESTS);
      data.num_requests = _PAPPL_MAX_REQUESTS;
    }
    else if (data.num_requests == 0)
    {
      data.num_requests = _PAPPL_DEFAULT_REQUESTS;
      fprintf(stderr, "testpappl: Doing %d requests.\n", _PAPPL_DEFAULT_REQUESTS);
    }
  }

  papplSystemSetMaxClients(system, data.num_children + 1);

  total = data.num_children * data.num_requests;

  // Start client threads
  _papplTestBegin("%s", name);
  gettimeofday(&data.start, NULL);
  gettimeofday(&data.end, NULL);
  for (i = 0; i < data.num_children; i ++)
    threads[i] = cupsThreadCreate((cups_thread_func_t)test_client_child, &data);

  // Wait for threads to finish
  current = last;
  while (data.completed_count < data.num_children)
  {
    if (current > 0)
      fputs("\b\b\b\b\b\b\b\b\b\b\b\b", stdout);

    i = 10 * current / total;
    if (i == 10)
    {
      // Completed
      fputs("[==========]", stdout);
    }
    else
    {
      // Partial
      progress[0] = '[';
      if (i > 0)
        memset(progress + 1, '=', i);
      if (i < 9)
        memset(progress + i + 2, ' ', 10 - i);
      progress[i + 1] = "-\\|/"[pcounter & 3];
      progress[11] = ']';
      progress[12] = '\0';
      fputs(progress, stdout);

      pcounter ++;
    }
    fflush(stdout);
    sleep(1);

    cupsMutexLock(&data.mutex);
    current = data.request_count;
    cupsMutexUnlock(&data.mutex);
  }

  if (current > 0)
    fputs("\b\b\b\b\b\b\b\b\b\b\b\b", stdout);

  for (i = 0; i < data.num_children; i ++)
    cupsThreadWait(threads[i]);

  // Report on activity and return...
  secs = (double)(data.end.tv_sec - data.start.tv_sec) + 0.000001 * (data.end.tv_usec - data.start.tv_usec);
  i    = cupsArrayGetCount(data.errors);

  _papplTestEndMessage(i == 0, "%.3f seconds, %.0f requests/sec, %lu errors", secs, (double)data.request_count / secs, (unsigned long)i);
  for (ptr = (const char *)cupsArrayGetFirst(data.errors); ptr; ptr = (const char *)cupsArrayGetNext(data.errors))
    fprintf(stderr, "%s: %s\n", name, ptr);

  cupsArrayDelete(data.errors);

  return (i == 0);
}


//
// 'test_idle_shutdown()' - Test idle shutdown functionality.
//

static bool				// O - Results of test
test_idle_shutdown(
    pappl_system_t *system)		// I - System
{
  bool		ret = false;		// Return value
  time_t	end;			// End time


  // Verify that idle shutdown works...
  papplSystemSetIdleShutdown(system, 90);

  _papplTestBegin("idle-shutdown");

  end = time(NULL) + 100;
  while (time(NULL) < end)
  {
    _papplTestProgress();
    sleep(1);
  }

  if (papplSystemIsShutdown(system))
  {
    _papplTestEnd(true);
  }
  else
  {
    _papplTestEnd(false);
    goto done;
  }

  ret = true;

  done:

  return (ret);
}


//
// 'test_network_get_cb()' - Get test networks.
//

static pappl_network_t	test_networks[2];

static size_t				// O - Number of networks
test_network_get_cb(
    pappl_system_t  *system,		// I - System
    void            *data,		// I - Callback data
    size_t          max_networks,	// I - Maximum number of networks
    pappl_network_t *networks)		// I - Networks
{
  (void)system;
  (void)data;

  if (!test_networks[0].name[0])
  {
    // Initialize test networks: eth0 and wlan0
    size_t	i;			// Looping var
    static const char * const names[] =	// Network names
    {
      "Ethernet",
      "Wi-Fi"
    };
    static const char * const idents[] =// Network identities
    {
      "eth0",
      "wlan0"
    };

    for (i = 0; i < (sizeof(names) / sizeof(names[0])); i ++)
    {
      // Initialize a network interface
      cupsCopyString(test_networks[i].name, names[i], sizeof(test_networks[i].name));
      cupsCopyString(test_networks[i].ident, idents[i], sizeof(test_networks[i].name));

      test_networks[i].up       = true;
      test_networks[i].config4  = PAPPL_NETCONF_DHCP;
      test_networks[i].config6  = PAPPL_NETCONF_DHCP;

      test_networks[i].dns[0].ipv4.sin_family      = AF_INET;
      test_networks[i].dns[0].ipv4.sin_addr.s_addr = htonl(0x0a000101);

      test_networks[i].addr4.ipv4.sin_family        = AF_INET;
      test_networks[i].addr4.ipv4.sin_addr.s_addr   = htonl(0x0a000102 + i);

      test_networks[i].mask4.ipv4.sin_family        = AF_INET;
      test_networks[i].mask4.ipv4.sin_addr.s_addr   = htonl(0xffffff00);

      test_networks[i].gateway4.ipv4.sin_family      = AF_INET;
      test_networks[i].gateway4.ipv4.sin_addr.s_addr = htonl(0x0a000101);

      test_networks[i].linkaddr6.ipv6.sin6_family           = AF_INET6;
      test_networks[i].linkaddr6.ipv6.sin6_addr.s6_addr[0]  = 0xfe;
      test_networks[i].linkaddr6.ipv6.sin6_addr.s6_addr[1]  = 0x80;
      test_networks[i].linkaddr6.ipv6.sin6_addr.s6_addr[10] = cupsGetRand() & 255;
      test_networks[i].linkaddr6.ipv6.sin6_addr.s6_addr[11] = cupsGetRand() & 255;
      test_networks[i].linkaddr6.ipv6.sin6_addr.s6_addr[12] = cupsGetRand() & 255;
      test_networks[i].linkaddr6.ipv6.sin6_addr.s6_addr[13] = cupsGetRand() & 255;
      test_networks[i].linkaddr6.ipv6.sin6_addr.s6_addr[14] = cupsGetRand() & 255;
      test_networks[i].linkaddr6.ipv6.sin6_addr.s6_addr[15] = cupsGetRand() & 255;
      test_networks[i].linkaddr6.ipv6.sin6_scope_id         = (unsigned)i + 1;
    }
  }

  if (max_networks < 2)
    memcpy(networks, test_networks, max_networks * sizeof(pappl_network_t));
  else
    memcpy(networks, test_networks, sizeof(test_networks));

  return (2);
}


//
// 'test_infra()' - Run infra/proxy tests.
//

static bool				// O - `true` on success, `false` on failure
test_infra(pappl_system_t *system)	// I - System
{
  bool			ret = true;	// Return value
  pappl_printer_t	*itest;		// Infrastructure printer
  pappl_printer_t	*ptest;		// Local printer testing proxy functionality
  const char		*proxy_device_uuid;
					// Output device UUID
  const char		*proxy_name;	// Infrastructure common name/UUID
  char			proxy_uri[1024];// Printer URI for infrastructure printer
  const char		*proxy_uuid;	// Infrastructure printer UUID
  size_t		num_ijobs,	// Number of jobs on infrastructure printer
			num_pjobs;	// Number of jobs on proxy printer
  http_t		*http;		// HTTP connection
  char			resource[1024],	// Resource for infrastructure printer
			filename[1024];	// Print file
  ipp_t			*request,	// Request
			*response;	// Response
  size_t		i;		// Looping var
  int			job_id;		// "job-id" value
  ipp_jstate_t		job_state;	// "job-state" value
  static const char * const test_files[] =
  {					// List of files to print
    "portrait-gray.jpg",
    "portrait-gray.png",
    "portrait-color.jpg",
    "portrait-color.png",
    "landscape-gray.jpg",
    "landscape-gray.png",
    "landscape-color.jpg",
    "landscape-color.png"
  };


  _papplTestBegin("infra: papplPrinterCreate(ptest)");
  if ((ptest = papplPrinterCreate(system, /*printer_id*/0, "ProxyP", "pwg_common-300dpi-black_1-sgray_8", "MFG:PWG;MDL:Office Printer;CMD:PWGRaster;", output_device_uri)) != NULL)
  {
    _papplTestEnd(true);
  }
  else
  {
    _papplTestEnd(false);
    return (false);
  }

  _papplTestBegin("infra: papplPrinterGetUUID(ptest)");
  if ((proxy_device_uuid = papplPrinterGetUUID(ptest)) != NULL)
    _papplTestEndMessage(true, "%s", proxy_device_uuid);
  else
    _papplTestEnd(false);

  _papplTestBegin("infra: papplPrinterCreateInfra(itest)");
  if ((itest = papplPrinterCreateInfra(system, /*printer_id*/0, "InfrastructureP", /*num_device_uuids*/0, /*device_uuids*/NULL)) != NULL)
  {
    _papplTestEnd(true);
  }
  else
  {
    _papplTestEndMessage(false, "%s", strerror(errno));
    return (false);
  }

  _papplTestBegin("infra: papplPrinterGetUUID(itest)");
  if ((proxy_uuid = papplPrinterGetUUID(itest)) != NULL)
    _papplTestEndMessage(true, "%s", proxy_uuid);
  else
    _papplTestEnd(false);

  _papplTestBegin("infra: papplPrinterGetURI(itest)");
  if (papplPrinterGetURI(itest, proxy_uri, sizeof(proxy_uri)))
    _papplTestEndMessage(true, "%s", proxy_uri);
  else
    _papplTestEnd(false);

  _papplTestBegin("infra: papplPrinterGetUUID(itest)");
  if ((proxy_name = papplPrinterGetUUID(itest)) != NULL)
    _papplTestEndMessage(true, "%s", proxy_name);
  else
    _papplTestEnd(false);

  _papplTestBegin("infra: papplPrinterSetProxy(ptest pointing to itest)");
  papplPrinterSetProxy(ptest, /*client_id*/NULL, proxy_name, proxy_device_uuid, /*provider_uri*/NULL, /*token_url*/NULL, proxy_uri, proxy_uuid);
  _papplTestEnd(true);

  // Connect to system...
  _papplTestBegin("infra: httpConnectURI(%s)", proxy_uri);
  if ((http = httpConnectURI(proxy_uri, /*host*/NULL, /*hostsize*/0, /*port*/NULL, resource, sizeof(resource), /*blocking*/true, /*msec*/30000, /*cancel*/NULL, /*require_ca*/false)) == NULL)
  {
    _papplTestEndMessage(false, "%s", cupsGetErrorString());
    return (false);
  }
  else
  {
    _papplTestEnd(true);
  }

  // Send print jobs to infra test printer and see if they show up on the proxy test printer...
  for (i = 0; i < (sizeof(test_files) / sizeof(test_files[0])); i ++)
  {
    int	interval = 1;			// Interval between status queries...

    _papplTestBegin("infra: Print-Job (%s)", test_files[i]);
    output_count ++;

    request = ippNewRequest(IPP_OP_PRINT_JOB);
    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri", NULL, proxy_uri);
    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name", NULL, cupsGetUser());
    ippAddString(request, IPP_TAG_OPERATION, IPP_CONST_TAG(IPP_TAG_NAME), "job-name", NULL, test_files[i]);

    if (access(test_files[i], R_OK))
      snprintf(filename, sizeof(filename), "testsuite/%s", test_files[i]);
    else
      cupsCopyString(filename, test_files[i], sizeof(filename));

    response  = cupsDoFileRequest(http, request, resource, filename);
    job_id    = ippGetInteger(ippFindAttribute(response, "job-id", IPP_TAG_INTEGER), 0);
    job_state = (ipp_jstate_t)ippGetInteger(ippFindAttribute(response, "job-state", IPP_TAG_ENUM), 0);

    ippDelete(response);

    if (cupsGetError() >= IPP_STATUS_ERROR_BAD_REQUEST)
    {
      _papplTestEndMessage(false, "%s", cupsGetErrorString());
      ret = false;
      continue;
    }

    _papplTestEndMessage(true, "job-id=%d, job-state=%s", job_id, ippEnumString("job-state", (int)job_state));

    _papplTestBegin("infra: Get-Job-Attributes (%s)", test_files[i]);
    do
    {
      request = ippNewRequest(IPP_OP_GET_JOB_ATTRIBUTES);
      ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri", NULL, proxy_uri);
      ippAddInteger(request, IPP_TAG_OPERATION, IPP_TAG_INTEGER, "job-id", job_id);
      ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name", NULL, cupsGetUser());

      response  = cupsDoRequest(http, request, resource);
      job_state = (ipp_jstate_t)ippGetInteger(ippFindAttribute(response, "job-state", IPP_TAG_ENUM), 0);
      ippDelete(response);

      if (cupsGetError() == IPP_STATUS_OK && job_state < IPP_JSTATE_CANCELED)
      {
        _papplTestProgress();
	sleep(_PAPPL_FIB_VALUE(interval));
	interval = _PAPPL_FIB_NEXT(interval);
      }
    }
    while (cupsGetError() == IPP_STATUS_OK && job_state < IPP_JSTATE_CANCELED);

    if (cupsGetError() >= IPP_STATUS_ERROR_BAD_REQUEST)
    {
      _papplTestEndMessage(false, "%s", cupsGetErrorString());
      ret = false;
      continue;
    }

    _papplTestEndMessage(job_state == IPP_JSTATE_COMPLETED, "job-state=%s", ippEnumString("job-state", (int)job_state));
    if (job_state != IPP_JSTATE_COMPLETED)
      ret = false;
  }

  _papplTestBegin("infra: papplPrinterGetNumberOfJobs(itest)");
  if ((num_ijobs = papplPrinterGetNumberOfJobs(itest)) > 0)
  {
    _papplTestEndMessage(true, "%u jobs", (unsigned)num_ijobs);
  }
  else
  {
    _papplTestEndMessage(false, "No jobs");
    ret = false;
  }

  _papplTestBegin("infra: papplPrinterGetNumberOfJobs(ptest)");
  if ((num_pjobs = papplPrinterGetNumberOfJobs(ptest)) > 0)
  {
    if (num_pjobs == num_ijobs)
    {
      _papplTestEndMessage(true, "%u jobs", (unsigned)num_ijobs);
    }
    else
    {
      _papplTestEndMessage(false, "No jobs, expected %u jobs", (unsigned)num_ijobs);
      ret = false;
    }
  }
  else
  {
    if (num_ijobs > 0)
      _papplTestEndMessage(false, "No jobs, expected %u jobs", (unsigned)num_ijobs);
    else
      _papplTestEndMessage(false, "No jobs");

    ret = false;
  }

  return (ret);
}


//
// 'test_network_set_cb()' - Set test networks.
//

static bool				// O - `true` to indicate success
test_network_set_cb(
    pappl_system_t  *system,		// I - System
    void            *data,		// I - Callback data
    size_t          num_networks,	// I - Number of networks
    pappl_network_t *networks)		// I - Networks
{
  (void)system;
  (void)data;

  if (num_networks != 2)
    return (false);

  memcpy(test_networks, networks, sizeof(test_networks));

  return (true);
}


//
// 'test_print_files()' - Run file print tests.
//

static bool				// O - `true` on success, `false` on failure
test_print_files(
    pappl_system_t       *system,	// I - System
    const char           *prompt,	// I - Prompt for files
    const char           *format,	// I - MIME media type of files
    size_t               num_files,	// I - Number of files to print
    const char * const * files)		// I - Files to print
{
  size_t	i, j, k, m;		// Looping vars
  http_t	*http;			// HTTP connection
  char		uri[1024],		// "printer-uri" value
		filename[1024],		// Print file
		job_name[1024];		// "job_name" value
  ipp_t		*request,		// Request
		*response;		// Response
  ipp_attribute_t *format_supported;	// Supported formats
  int		job_id;			// "job-id" value
  ipp_jstate_t	job_state;		// "job-state" value
  int		interval;		// Interval between queries
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
  _papplTestBegin("%s: Connect to server", prompt);
  if ((http = connect_to_printer(system, true, uri, sizeof(uri))) == NULL)
  {
    _papplTestEndMessage(false, "Unable to connect: %s", cupsGetErrorString());
    return (false);
  }

  _papplTestEnd(true);

  // Check for file format support...
  _papplTestBegin("%s: Check for %s support", prompt, format);

  request = ippNewRequest(IPP_OP_GET_PRINTER_ATTRIBUTES);
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri", NULL, uri);
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name", NULL, cupsGetUser());
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_KEYWORD, "requested-attributes", NULL, "document-format-supported");

  response = cupsDoRequest(http, request, "/ipp/print");
  if ((format_supported = ippFindAttribute(response, "document-format-supported", IPP_TAG_MIMETYPE)) != NULL)
  {
    if (ippContainsString(format_supported, format))
    {
      _papplTestEndMessage(true, "%s is supported", format);
    }
    else
    {
      _papplTestEndMessage(true, "%s is not supported", format);
      ippDelete(response);
      httpClose(http);
      return (true);
    }
  }
  else
  {
    _papplTestEndMessage(false, "No document-format-supported attribute");
    ippDelete(response);
    httpClose(http);
    return (false);
  }

  // Print files...
  for (i = 0; i < num_files; i ++)
  {
    if (access(files[i], R_OK))
      snprintf(filename, sizeof(filename), "testsuite/%s", files[i]);
    else
      cupsCopyString(filename, files[i], sizeof(filename));

    for (j = 0; j < (sizeof(orients) / sizeof(orients[0])); j ++)
    {
      for (k = 0; k < (sizeof(modes) / sizeof(modes[0])); k ++)
      {
	for (m = 0; m < (sizeof(scalings) / sizeof(scalings[0])); m ++)
	{
	  // Stop the test if the system is shutdown (e.g. CTRL+C)
	  if (papplSystemIsShutdown(system))
	    return (false);

	  // Print the job...
	  snprintf(job_name, sizeof(job_name), "%s+%s+%s+%s", files[i], ippEnumString("orientation-requested", orients[j]), modes[k], scalings[m]);
	  _papplTestBegin("%s: Print-Job(%s)", prompt, job_name);

	  request = ippNewRequest(IPP_OP_PRINT_JOB);
	  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri", NULL, uri);
	  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name", NULL, cupsGetUser());
	  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_MIMETYPE, "document-format", NULL, format);
	  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "job-name", NULL, job_name);

          ippAddInteger(request, IPP_TAG_JOB, IPP_TAG_ENUM, "orientation-requested", orients[j]);
	  ippAddString(request, IPP_TAG_JOB, IPP_TAG_KEYWORD, "print-color-mode", NULL, modes[k]);
	  ippAddString(request, IPP_TAG_JOB, IPP_TAG_KEYWORD, "print-scaling", NULL, scalings[m]);

	  response = cupsDoFileRequest(http, request, "/ipp/print", filename);

	  if (cupsGetError() >= IPP_STATUS_ERROR_BAD_REQUEST)
	  {
	    _papplTestEndMessage(false, "%s", cupsGetErrorString());
	    ippDelete(response);
	    httpClose(http);
	    return (false);
	  }

	  job_id = ippGetInteger(ippFindAttribute(response, "job-id", IPP_TAG_INTEGER), 0);

          _papplTestEndMessage(true, "job-id=%d", job_id);
	  ippDelete(response);
	  output_count ++;

	  // Poll job status until completed...
	  interval = 1;
	  do
	  {
	    _papplTestProgress();
	    sleep(_PAPPL_FIB_VALUE(interval));
	    interval = _PAPPL_FIB_NEXT(interval);

	    _papplTestBegin("%s: Get-Job-Attributes(job-id=%d)", prompt, job_id);

	    request = ippNewRequest(IPP_OP_GET_JOB_ATTRIBUTES);
	    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri", NULL, uri);
	    ippAddInteger(request, IPP_TAG_OPERATION, IPP_TAG_INTEGER, "job-id", job_id);
	    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name", NULL, cupsGetUser());

	    response = cupsDoRequest(http, request, "/ipp/print");

	    if (cupsGetError() >= IPP_STATUS_ERROR_BAD_REQUEST)
	    {
	      _papplTestEndMessage(false, "%s", cupsGetErrorString());
	      httpClose(http);
	      ippDelete(response);
	      return (false);
	    }

	    job_state = (ipp_jstate_t)ippGetInteger(ippFindAttribute(response, "job-state", IPP_TAG_ENUM), 0);

            _papplTestEndMessage(job_state != (ipp_jstate_t)0, "job-state=%d", job_state);
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
  int		interval;		// Interval between queries
  static const char * const modes[] =	// "print-color-mode" values
  {
    "auto",
    "auto-monochrome",
    "color",
    "monochrome"
  };


  // Connect to system...
  _papplTestBegin("pwg-raster: Connect to server");
  if ((http = connect_to_printer(system, false, uri, sizeof(uri))) == NULL)
  {
    _papplTestEndMessage(false, "Unable to connect: %s", cupsGetErrorString());
    return (false);
  }
  _papplTestEnd(true);

  // Get printer capabilities
  _papplTestBegin("pwg-raster: Get-Printer-Attributes");

  request = ippNewRequest(IPP_OP_GET_PRINTER_ATTRIBUTES);
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri", NULL, "ipp://localhost/ipp/print");
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name", NULL, cupsGetUser());

  supported = cupsDoRequest(http, request, "/ipp/print");

  if (cupsGetError() != IPP_STATUS_OK)
  {
    _papplTestEndMessage(false, "%s", cupsGetErrorString());
    goto done;
  }

  if ((mode_supported = ippFindAttribute(supported, "print-color-mode-supported", IPP_TAG_KEYWORD)) == NULL)
  {
    _papplTestEndMessage(false, "missing required 'print-color-mode-supported' attribute in response");
    goto done;
  }

  _papplTestEnd(true);

  // Loop through the supported print-color-mode values...
  for (i = 0; i < (int)(sizeof(modes) / sizeof(modes[0])); i ++)
  {
    // Make raster data for this mode...
    _papplTestBegin("pwg-raster: Print-Job(%s)", modes[i]);

    if (!ippContainsString(mode_supported, modes[i]))
      continue;				// Not supported, skip

    if (!make_raster_file(supported, strstr(modes[i], "monochrome") != NULL, filename, sizeof(filename)))
      break;				// Error

    // Print the file...
    snprintf(job_name, sizeof(job_name), "pwg-raster-%s", modes[i]);

    do
    {
      request = ippNewRequest(IPP_OP_PRINT_JOB);
      ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri", NULL, uri);
      ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name", NULL, cupsGetUser());
      ippAddString(request, IPP_TAG_OPERATION, IPP_CONST_TAG(IPP_TAG_MIMETYPE), "document-format", NULL, "image/pwg-raster");
      ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "job-name", NULL, job_name);

      ippAddString(request, IPP_TAG_JOB, IPP_TAG_KEYWORD, "print-color-mode", NULL, modes[i]);

      response = cupsDoFileRequest(http, request, "/ipp/print", filename);
      if (cupsGetError() >= IPP_STATUS_ERROR_BAD_REQUEST)
      {
        ippDelete(response);
        response = NULL;
        sleep(1);
      }
    }
    while (cupsGetError() == IPP_STATUS_ERROR_BUSY);

    if (cupsGetError() >= IPP_STATUS_ERROR_BAD_REQUEST)
    {
      _papplTestEndMessage(false, "Unable to print %s: %s", job_name, cupsGetErrorString());
      goto done;
    }

    job_id = ippGetInteger(ippFindAttribute(response, "job-id", IPP_TAG_INTEGER), 0);

    ippDelete(response);

    _papplTestEndMessage(true, "job-id=%d", job_id);
    output_count ++;

    // Poll job status until completed...
    interval = 1;
    do
    {
      _papplTestProgress();
      sleep(_PAPPL_FIB_VALUE(interval));
      interval = _PAPPL_FIB_NEXT(interval);

      _papplTestBegin("pwg-raster: Get-Job-Attributes(job-id=%d)", job_id);

      request = ippNewRequest(IPP_OP_GET_JOB_ATTRIBUTES);
      ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri", NULL, uri);
      ippAddInteger(request, IPP_TAG_OPERATION, IPP_TAG_INTEGER, "job-id", job_id);
      ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name", NULL, cupsGetUser());

      response = cupsDoRequest(http, request, "/ipp/print");

      if (cupsGetError() >= IPP_STATUS_ERROR_BAD_REQUEST)
      {
	_papplTestEndMessage(false, "Unable to get job state for '%s': %s", job_name, cupsGetErrorString());
        goto done;
      }

      job_state = (ipp_jstate_t)ippGetInteger(ippFindAttribute(response, "job-state", IPP_TAG_ENUM), 0);

      _papplTestEndMessage(true, "job-state=%d", job_state);
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
// 'test_wifi_join_cb()' - Try joining a Wi-Fi network.
//
// Note: The code here is for a Raspberry Pi running the default Raspberry Pi
// OS using wpa_supplicant for Wi-Fi support.  Any existing wpa_supplicant.conf
// file is backed up.  And obviously this means that "testpappl" has to run as
// root.
//

static bool				// O - `true` on success, `false` otherwise
test_wifi_join_cb(
    pappl_system_t *sys,		// I - System
    void           *data,		// I - Callback data (should be "testpappl")
    const char     *ssid,		// I - Wi-Fi SSID name
    const char     *psk)		// I - Wi-Fi password
{
  cups_file_t	*infile,		// Old wpa_supplicant.conf file
		*outfile;		// New wpa_supplicant.conf file
  char		line[1024];		// Line from file


  // Range check input...
  if (!sys)
  {
    fputs("test_wifi_join_cb: System pointer is NULL.\n", stderr);
    return (false);
  }

  if (!data || strcmp((char *)data, "testpappl"))
  {
    fprintf(stderr, "test_wifi_join_cb: Bad callback data pointer %p.\n", data);
    return (false);
  }

  if (!ssid || !*ssid || !psk)
  {
    fprintf(stderr, "test_wifi_join_cb: Bad SSID '%s' or PSK '%s'.\n", ssid ? ssid : "(null)", psk ? psk : "(null)");
    return (false);
  }

  if (access("/etc/wpa_supplicant/wpa_supplicant.conf", W_OK))
  {
    // No write access to the wpa_supplicant configuration file, so just assume
    // that SSID == PSK is OK...
    bool ok = !strcmp(ssid, psk);	// Do SSID and PSK match?

    if (ok)
      cupsCopyString(current_ssid, ssid, sizeof(current_ssid));

    return (ok);
  }

  if (rename("/etc/wpa_supplicant/wpa_supplicant.conf", "/etc/wpa_supplicant/wpa_supplicant.conf.O") && errno != ENOENT)
  {
    perror("test_wifi_join_cb: Unable to backup '/etc/wpa_supplicant/wpa_supplicant.conf'");
    return (false);
  }

  if ((outfile = cupsFileOpen("/etc/wpa_supplicant/wpa_supplicant.conf", "w")) == NULL)
  {
    perror("test_wifi_join_cb: Unable to create new '/etc/wpa_supplicant/wpa_supplicant.conf' file");
    if (rename("/etc/wpa_supplicant/wpa_supplicant.conf.O", "/etc/wpa_supplicant/wpa_supplicant.conf") && errno != ENOENT)
      perror("test_wifi_join_cb: Unable to restore '/etc/wpa_supplicant/wpa_supplicant.conf'");
    return (false);
  }

  if ((infile = cupsFileOpen("/etc/wpa_supplicant/wpa_supplicant.conf.O", "r")) == NULL)
  {
    // Write standard header for config file on Raspberry Pi OS...
    cupsFilePuts(outfile, "ctrl_interface=DIR=/var/run/wpa_supplicant GROUP=netdev\n");
    cupsFilePuts(outfile, "update_config=1\n");
    // can't specify country for 5GHz... Locale is probably not set...
  }
  else
  {
    // Copy old config file up to the "network={"...  Real code might want to
    // preserve the old network lines to allow for roaming...
    while (cupsFileGets(infile, line, sizeof(line)))
    {
      if (!strncmp(line, "network={", 9))
        break;

      cupsFilePrintf(outfile, "%s\n", line);
    }

    cupsFileClose(infile);
  }

  // Write a network definition...  Production code needs to deal with special
  // characters!
  cupsFilePuts(outfile, "network={\n");
  cupsFilePrintf(outfile, "\tssid=\"%s\"\n", ssid);
  if (*psk)
    cupsFilePrintf(outfile, "\tpsk=\"%s\"\n", psk);
  else
    cupsFilePuts(outfile, "\tkey_mgmt=NONE\n");
  cupsFilePuts(outfile, "}\n");
  cupsFileClose(outfile);

  // Force re-association...
  if (system("wpa_cli -i wlan0 reconfigure"))
    return (false);

  return (!system("dhclient -v &"));
}


//
// 'test_wifi_list_cb()' - List available Wi-Fi networks.
//
// Note: The code here is for a Raspberry Pi running the default Raspberry Pi
// OS using wpa_supplicant for Wi-Fi support.  The Wi-Fi interface name needs
// to be "wlan0".
//

static size_t				// O - Number of Wi-Fi networks
test_wifi_list_cb(
    pappl_system_t *sys,		// I - System
    void           *data,		// I - Callback data (should be "testpappl")
    cups_dest_t    **ssids)		// O - Wi-Fi network list
{
  size_t	num_ssids = 0;		// Number of Wi-Fi networks
  cups_dest_t	*ssid;			// Current Wi-Fi network
#if !_WIN32
  FILE	*fp;				// Pipe to "iwlist" command
  char	line[1024],			// Line from command
	*start,				// Start of SSID
	*end;				// End of SSID
#endif // !_WIN32


  if (ssids)
    *ssids = NULL;

  if (!sys)
  {
    fputs("test_wifi_status_cb: System pointer is NULL.\n", stderr);
    return (0);
  }

  if (!data || strcmp((char *)data, "testpappl"))
  {
    fprintf(stderr, "test_wifi_status_cb: Bad callback data pointer %p.\n", data);
    return (0);
  }

  if (!ssids)
  {
    fputs("test_wifi_status_cb: ssid pointer is NULL.\n", stderr);
    return (0);
  }

#if _WIN32
  // Just return a dummy list for testing...
  num_ssids = cupsAddDest("One Fish", NULL, num_ssids, ssids);
  num_ssids = cupsAddDest("Two Fish", NULL, num_ssids, ssids);
  num_ssids = cupsAddDest("Red Fish", NULL, num_ssids, ssids);
  num_ssids = cupsAddDest("Blue Fish", NULL, num_ssids, ssids);

  if ((ssid = cupsGetDest(current_ssid, NULL, num_ssids, *ssids)) != NULL)
    ssid->is_default = true;

#else
  // See if we have the iw and iwlist commands...
  if (access("/sbin/iw", X_OK) || access("/sbin/iwlist", X_OK))
  {
    // No, return a dummy list for testing...
    num_ssids = cupsAddDest("One Fish", NULL, num_ssids, ssids);
    num_ssids = cupsAddDest("Two Fish", NULL, num_ssids, ssids);
    num_ssids = cupsAddDest("Red Fish", NULL, num_ssids, ssids);
    num_ssids = cupsAddDest("Blue Fish", NULL, num_ssids, ssids);

    if ((ssid = cupsGetDest(current_ssid, NULL, num_ssids, *ssids)) != NULL)
      ssid->is_default = true;

    return (num_ssids);
  }

  // Force a Wi-Fi scan...
  system("/sbin/iw dev wlan0 scan");

  sleep(1);

  // Then read back the list of Wi-Fi networks...
  if ((fp = popen("/sbin/iwlist wlan0 scanning", "r")) == NULL)
  {
    // Can't run command, so no Wi-Fi support...
    return (0);
  }

  while (fgets(line, sizeof(line), fp))
  {
    // Parse line of the form:
    //
    // ESSID:"ssid"
    if ((start = strstr(line, "ESSID:\"")) == NULL)
      continue;

    start += 7;

    if ((end = strchr(start, '\"')) != NULL)
    {
      *end = '\0';
      if (*start)
        num_ssids = cupsAddDest(start, NULL, num_ssids, ssids);
    }
  }

  pclose(fp);
#endif // _WIN32

  return (num_ssids);
}


//
// 'test_wifi_status_cb()' - Check the status of the current Wi-Fi network connection, if any.
//
// Note: The code here is for a Raspberry Pi running the default Raspberry Pi
// OS using wpa_supplicant for Wi-Fi support.  The Wi-Fi interface name needs
// to be "wlan0".
//

static pappl_wifi_t *			// O - Wi-Fi status or `NULL` on error
test_wifi_status_cb(
    pappl_system_t *system,		// I - System
    void           *data,		// I - Callback data (should be "testpappl")
    pappl_wifi_t   *wifi_data)		// I - Wi-Fi status buffer
{
#if !_WIN32
  FILE	*fp;				// Pipe to "iwgetid" command
  char	line[1024],			// Line from command
	*ptr;				// Pointer into line
#endif // !_WIN32


  // Range check input...
  if (wifi_data)
  {
    memset(wifi_data, 0, sizeof(pappl_wifi_t));
    wifi_data->state = PAPPL_WIFI_STATE_NOT_CONFIGURED;
  }

  if (!system)
  {
    fputs("test_wifi_status_cb: System pointer is NULL.\n", stderr);
    return (NULL);
  }

  if (!data || strcmp((char *)data, "testpappl"))
  {
    fprintf(stderr, "test_wifi_status_cb: Bad callback data pointer %p.\n", data);
    return (NULL);
  }

  if (!wifi_data)
  {
    fputs("test_wifi_status_cb: wifi_data pointer is NULL.\n", stderr);
    return (NULL);
  }

  if (current_ssid[0])
  {
    cupsCopyString(wifi_data->ssid, current_ssid, sizeof(wifi_data->ssid));
    wifi_data->state = PAPPL_WIFI_STATE_ON;
    return (wifi_data);
  }

#if !_WIN32
  // Fill in the Wi-Fi status...  This code only returns the 'not-configured' or
  // 'on' state values for simplicity, but production code should support all of
  // them.
  if (access("/sbin/iwgetid", X_OK))
    return (wifi_data);			// No iwgetid command...

  if ((fp = popen("/sbin/iwgetid", "r")) == NULL)
  {
    // Can't run command, so no Wi-Fi support...
    return (wifi_data);
  }

  if (fgets(line, sizeof(line), fp))
  {
    // Parse line of the form:
    //
    // ifname ESSID:"ssid"
    if ((ptr = strrchr(line, '\"')) != NULL)
      *ptr = '\0';			// Strip trailing quote

    if ((ptr = strchr(line, '\"')) != NULL)
    {
      // Skip leading quote and copy SSID...
      ptr ++;
      cupsCopyString(wifi_data->ssid, ptr, sizeof(wifi_data->ssid));
      wifi_data->state = PAPPL_WIFI_STATE_ON;
    }
  }

  pclose(fp);

  if (wifi_data->state == PAPPL_WIFI_STATE_NOT_CONFIGURED)
  {
    // Try reading the wpa_supplicant.conf file...
    if ((fp = fopen("/etc/wpa_supplicant/wpa_supplicant.conf", "r")) != NULL)
    {
      while (fgets(line, sizeof(line), fp))
      {
        if ((ptr = strstr(line, "ssid=\"")) != NULL)
        {
          cupsCopyString(wifi_data->ssid, ptr + 6, sizeof(wifi_data->ssid));
          if ((ptr = strchr(wifi_data->ssid, '\"')) != NULL)
            *ptr = '\0';

          wifi_data->state = PAPPL_WIFI_STATE_JOINING;
          break;
        }
      }

      fclose(fp);
    }
  }
#endif // !_WIN32

  return (wifi_data);
}


//
// 'timer_cb()' - Timer callback.
//

static bool				// O - `true` to continue, `false` to stop
timer_cb(pappl_system_t    *system,	// I - System
         _pappl_testdata_t *data)	// I - Test data
{
  (void)system;

  if (data->timer_count < (int)(sizeof(data->timer_times) / sizeof(data->timer_times[0])))
    data->timer_times[data->timer_count] = time(NULL);

  data->timer_count ++;

  papplLog(system, PAPPL_LOGLEVEL_DEBUG, "timer_cb: count=%d", data->timer_count);

  return (data->timer_count < _PAPPL_MAX_TIMER_COUNT);
}


//
// 'usage()' - Show usage.
//

static int				// O - Exit status
usage(int status)			// I - Exit status
{
  puts("Usage: testpappl [OPTIONS] [\"SERVER NAME\"]");
  puts("Options:");
  puts("");
  puts("  --get-id DEVICE-URI          Show IEEE-1284 device ID for URI.");
  puts("  --get-status DEVICE-URI      Show printer status for URI.");
  puts("  --get-supplies DEVICE-URI    Show supplies for URI.");
  puts("  --help                       Show help");
  puts("  --list                       List devices");
  puts("  --list-TYPE                  Lists devices of TYPE (dns-sd, local, network, usb)");
  puts("  --no-tls                     Do not support TLS");
  puts("  --ps-query DEVICE-URI        Do a PostScript query to get the product string.");
  puts("  --usb-ethernet               Enable a Ethernet gadget.");
  puts("  --usb-product-id PRODUCT-ID  Set the USB product ID (default is 0x8011).");
  puts("  --usb-readonly DISK-IMAGE    Enable a read-only mass storage gadget.");
  puts("  --usb-removable DISK-IMAGE   Enable a removable mass storage gadget.");
  puts("  --usb-serial                 Enable a serial port gadget.");
  puts("  --usb-storage DISK-IMAGE     Enable a read-write mass storage gadget.");
  puts("  --usb-vendor-id VENDOR-ID    Set the USB vendor ID (default is 0x1209).");
  puts("  --version                    Show version");
  puts("  -1                           Single queue");
  puts("  -A PAM-SERVICE               Enable authentication using PAM service");
  puts("  -c                           Do a clean run (no loading of state)");
  puts("  -d SPOOL-DIRECTORY           Set the spool directory");
  puts("  -l LOG-FILE                  Set the log file");
  puts("  -L LOG-LEVEL                 Set the log level (fatal, error, warn, info, debug)");
  puts("  -m DRIVER-NAME               Add a printer with the named driver");
  puts("  -o OUTPUT-DIRECTORY          Set the output directory (default '.')");
  puts("  -p PORT                      Set the listen port (default auto)");
  puts("  -R                           Enable raw sockets");
  puts("  -t TEST-NAME                 Run the named test (see below)");
  puts("  -T                           Enable TLS-only mode");
  puts("  -U                           Enable USB printer gadget");
  puts("");
  puts("Tests:");
  puts("");
  puts("  all                  All of the following tests");
  puts("  client               Simulated client tests");
  puts("  client-CLxRQ         Simulated CL clients and RQ requests tests");
  puts("  client-max           Simulated max clients tests");
  puts("  idle-shutdown        Test idle shutdown functionality");
  puts("  infra                Test shared infrastructure extension support");
  puts("  jpeg                 JPEG image tests");
  puts("  pdf                  PDF file tests");
  puts("  png                  PNG image tests");
  puts("  pwg-raster           PWG Raster tests");

  return (status);
}
