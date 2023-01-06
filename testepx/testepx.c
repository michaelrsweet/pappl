//---------------------------------------------------------------------------------------------------
//
// EPX Test Printer
//
// A virtual IPP Printer used to prototype IPP Enterprise Printing Extensions v2.0 (EPX)
//
// Copyright © 2022 Printer Working Group.
// Written by Smith Kennedy (HP Inc.)
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//
//---------------------------------------------------------------------------------------------------


#include "testepx.h"

#include <pappl/base-private.h>
#include <config.h>
#include <libgen.h>
#include <errno.h> // for checking the result of creating firstPrinter in the system callback
#include <sys/stat.h> // for making ;the OUTPUT_LOCATION programmatically


#define FOOTER_HTML             "Copyright © 2022 Printer Working Group."
#define EPX_VERSION_STRING          "0.1.0.1"
#define EPX_VERSION_L1_MAJOR    0
#define EPX_VERSION_L2_MINOR    1
#define EPX_VERSION_L3_PATCH    0
#define EPX_VERSION_L4_BUILD    1

#define PRINTER_NAME "EPX Test Printer"

#define OUTPUT_LOCATION "/tmp/epx"

static pappl_system_t *epx_system_cb(int optionCount, cups_option_t *options, void *data);
static const char *get_device_uri(void);
static const char *get_timestamp(void);
void epx_delete_printer_from_system(pappl_printer_t *printer, void *data);

//---------------------------------------------------------------------------------------------------
// main()

int main(int  argc, char *argv[])
{
    int result = 0;
    char *whoami = basename(argv[0]);
    
    int                     drivercount = EPX_DRIVER_COUNT;
    pappl_pr_driver_t       *drivers = epx_drivers;
    pappl_pr_autoadd_cb_t   autoadd_callback = epx_pappl_autoadd_cb;
    pappl_pr_driver_cb_t    driver_callback = epx_pappl_driver_cb;

    // Make the output location if it is missing; ignore if already there, otherwise bail
    // This is constantly making it with read only permissions so I need to figure out what I'm doing wrong
//    mode_t mask = umask(S_IRWXU & S_IRWXG & S_IRWXO);
//    result = mkdir(OUTPUT_LOCATION, mask);
//    if (-1 == result && (EEXIST != errno || ENOENT != errno))
//    {
//        perror(argv[0]);
//        fprintf(stderr, "%s - papplMainLoop(): mkdir failed with errno = %d \n", whoami, errno);
//        exit(EXIT_FAILURE);
//    }

    printf("%s - Starting papplMainLoop\n", whoami);
    
    result = papplMainloop(argc,                // I - Number of command line arguments
                           argv,                // I - Command line arguments
                           EPX_VERSION_STRING,  // I - Version number
                           FOOTER_HTML,         // I - Footer HTML or `NULL` for none
                           drivercount,         // I - Number of drivers
                           drivers,             // I - Drivers
                           autoadd_callback,    // I - Auto-add callback or `NULL` for none
                           driver_callback,     // I - Driver callback
                           NULL,                // I - Sub-command name or `NULL` for none
                           NULL,                // I - Sub-command callback or `NULL` for none
                           epx_system_cb,       // I - System callback or `NULL` for default
                           NULL,                // I - Usage callback or `NULL` for default
                           whoami);             // I - Context pointer
    
    printf("%s - papplMainLoop stopped with result %d\n", whoami, result);

    return result;
}

//---------------------------------------------------------------------------------------------------
// 'epx_system_cb()' - System callback to set up the system.

pappl_system_t *epx_system_cb(int           optionCount,   // I - Number of options
                              cups_option_t *options,      // I - Options
                              void          *data)         // I - Callback data
{
    pappl_system_t      *system;                // System object
    const char          *val,                   // Current option value
                        *hostname,              // Hostname, if any
                        *logfile,               // Log file, if any
                        *system_name;           // System name, if any
    pappl_loglevel_t    loglevel;               // Log level
    int                 port = 0;               // Port number, if any
    char                *whoami = (char*)data;  // Name of program
    pappl_printer_t     *firstPrinter;          // Default printer creation
    
    
    // System options
    static pappl_contact_t contact =    // Contact information
    {
        "Smith Kennedy",
        "epx@pwg.org",
        "+1-208-555-1212"
    };
    static pappl_version_t versions[1] =    // "Firmware" version info
    {
        {
            "Test Application",             // "xxx-firmware-name" value
            "",                             // "xxx-firmware-patches" value
            EPX_VERSION_STRING,             // "xxx-firmware-string-version" value
            {                               // "xxx-firmware-version" value (short[4])
                EPX_VERSION_L1_MAJOR,
                EPX_VERSION_L2_MINOR,
                EPX_VERSION_L3_PATCH,
                EPX_VERSION_L4_BUILD
            }
            
        }
    };
    

    
    // Verify that this was the right callback called by validating that data is what was provided in main()
    if (!data || strcmp(whoami, "testepx"))
    {
        fprintf(stderr, "%s - epx_system_cb: Bad callback data %p.\n", whoami, data);
        return (NULL);
    }
    
    // Parse options...
    if ((val = cupsGetOption("log-level", (cups_len_t)optionCount, options)) != NULL)
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
            fprintf(stderr, "%s - epx_system_cb: Bad log-level value '%s'.\n", whoami, val);
            return (NULL);
        }
    }
    else
    {
        loglevel = PAPPL_LOGLEVEL_UNSPEC;
    }
    
    logfile     = cupsGetOption("log-file", (cups_len_t)optionCount, options);
    hostname    = cupsGetOption("server-hostname", (cups_len_t)optionCount, options);
    system_name = cupsGetOption("system-name", (cups_len_t)optionCount, options);
    
    if ((val = cupsGetOption("server-port", (cups_len_t)optionCount, options)) != NULL)
    {
        if (!isdigit(*val & 255))
        {
            fprintf(stderr, "%s - epx_system_cb: Bad server-port value '%s'.\n", whoami, val);
            return (NULL);
        }
        else
        {
            port = atoi(val);
        }
    }
    
    system = papplSystemCreate(PAPPL_SOPTIONS_MULTI_QUEUE |                                         // Web interface won't work without this option...
                               PAPPL_SOPTIONS_WEB_LOG |
                               PAPPL_SOPTIONS_WEB_NETWORK |
                               PAPPL_SOPTIONS_WEB_SECURITY |
                               PAPPL_SOPTIONS_WEB_TLS |
                               PAPPL_SOPTIONS_WEB_INTERFACE,                                        // I - Server options (bitfield)
                               system_name ? system_name : "NoSystemName",                          // I - System name
                               port,                                                                // I - Port number or `0` for auto
                               "_print,_universal",                                                 // I - DNS-SD sub-types or `NULL` for none
                               cupsGetOption("spool-directory", (cups_len_t)optionCount, options),  // I - Spool directory or `NULL` for default
                               logfile ? logfile : "-",                                             // I - Log file or `NULL` for default
                               loglevel,                                                            // I - Log level
                               cupsGetOption("auth-service", (cups_len_t)optionCount, options),     // I - PAM authentication service or `NULL` for none
                               false);                                                              // I - Only support TLS connections?
    
    if (system == NULL)
        return (NULL);
    
    papplSystemAddListeners(system, NULL);
    papplSystemSetHostName(system, hostname); // TODO: Why is this even needed?
    
    papplSystemSetPrinterDrivers(system, (int)(sizeof(epx_drivers) / sizeof(epx_drivers[0])), epx_drivers, epx_pappl_autoadd_cb, /*create_cb*/NULL, epx_pappl_driver_cb, whoami);
    
    papplSystemSetFooterHTML(system, FOOTER_HTML);
    papplSystemSetSaveCallback(system, (pappl_save_cb_t)papplSystemSaveState, (void *)"/tmp/testepx.state");
    papplSystemSetVersions(system, (int)(sizeof(versions) / sizeof(versions[0])), versions);

    // Make all the earlier printers go away
    papplLog(system, PAPPL_LOGLEVEL_INFO, "Iterating on any existing printers that need to be cleaned up...");
    papplSystemIteratePrinters(system, epx_delete_printer_from_system, system);
    papplLog(system, PAPPL_LOGLEVEL_INFO, "Printer cleanup complete");

    
    if (!papplSystemLoadState(system, "/tmp/testepx.state"))
    {
        papplSystemSetContact(system, &contact);
        papplSystemSetDNSSDName(system, system_name ? system_name : "TestEPX System");
        papplSystemSetGeoLocation(system, "geo:43.617697,-116.199614"); // Idaho State Capitol in Boise
        papplSystemSetLocation(system, "Test Lab 42");
        papplSystemSetOrganization(system, "PWG");
    }
    
    //--------------------------------------------------------------------------------
    // Make a printer so that one doesn't have to be made manually 
    firstPrinter = papplSystemFindPrinter(system, NULL, 0, OUTPUT_LOCATION);
    if (NULL == firstPrinter)
    {
        papplLog(system, PAPPL_LOGLEVEL_INFO, "Printer \"%s\" NOT found - making a new printer...\n", PRINTER_NAME);
        firstPrinter = papplPrinterCreate(system, 0, PRINTER_NAME, epx_drivers[0].name, epx_drivers[0].device_id, OUTPUT_LOCATION);
        if (NULL == firstPrinter)
            papplLog(system, PAPPL_LOGLEVEL_ERROR, "Printer \"%s\" NOT created - ERRNO = %d.\n", PRINTER_NAME, errno);
        else
            papplLog(system, PAPPL_LOGLEVEL_INFO, "Printer \"%s\" created.\n", PRINTER_NAME);

    }
    else
    {
        papplLog(system, PAPPL_LOGLEVEL_INFO, "Printer \"%s\" found.\n", PRINTER_NAME);
    }

    
    return (system);
}

static const char *
get_device_uri()
{
    static char outputUri[256];
    memset(outputUri, 0, sizeof(outputUri));
    snprintf(outputUri, sizeof(outputUri), "%s-%s/", OUTPUT_LOCATION, get_timestamp());
    return outputUri;
}

static const char *
get_timestamp()
{
    static char outstr[200];
    time_t t;
    struct tm *tmp;

    t = time(NULL);
    tmp = localtime(&t);
    if (tmp == NULL)
        return NULL;

   if (strftime(outstr, sizeof(outstr), "%Y-%m-%d-%H-%M", tmp) == 0)
       return NULL;

    return outstr;
}

void
epx_delete_printer_from_system(pappl_printer_t *printer, void *data)
{
    char printerName[256];
    strncpy(printerName, papplPrinterGetName(printer), sizeof(printerName));
    papplLog((pappl_system_t *)data, PAPPL_LOGLEVEL_INFO, "DELETING PRINTER: '%s'", printerName);
    papplPrinterDisable(printer);
    papplPrinterDelete(printer);
    papplLog((pappl_system_t *)data, PAPPL_LOGLEVEL_INFO, "PRINTER DELETED: '%s'", printerName);
}
