Introduction
============

PAPPL is a simple C-based framework/library for developing CUPS Printer
Applications, which are the recommended replacement for printer drivers.  It is
sufficiently general purpose to support any kind of printer or driver that can
be used on desktops, servers, and in embedded environments.

PAPPL embeds a multi-threaded HTTP /
[IPP Everywhere™](https://www.pwg.org/ipp/everywhere.html)
server and provides callbacks for a variety of events that allows a GUI or
command-line application to interact with both the local user that is running
the Printer Application and any network clients that are submitting print jobs,
querying printer status and capabilities, and so forth.

PAPPL provides a simple driver interface for raster graphics printing, and
developers of CUPS Raster drivers will readily adapt to it.  PAPPL can also be
used with printers that support vector graphics printing although you'll have
to develop more code to support them.  Drivers provide configuration and
capability information to PAPPL, and PAPPL then calls the driver to print things
as needed.  PAPPL automatically supports printing of JPEG, PNG, PWG Raster,
Apple Raster, and "raw" files to printers connected via USB and network
(AppSocket/JetDirect) connections.  Other formats can be supported through
"filter" callbacks you register.

PAPPL is Copyright © 2019-2020 by Michael R Sweet and is licensed under the
Apache License Version 2.0 with an (optional) exception to allow linking against
GPL2/LGPL2 software (like older versions of CUPS), so it can be used *freely* in
any project you'd like.  See the files "LICENSE" and "NOTICE" in the source
distribution for more information.


Requirements
------------

PAPPL requires a POSIX-compliant host operating system such as Linux®, macOS®,
QNX®, or VxWorks®, a C99 compiler like Clang or GCC, a `make` program that
supports the `include` directive, and the following support libraries:

- CUPS 2.2 or later for the CUPS libraries (libcups2/libcupsimage2)
- GNU TLS 2.8 or later (except on macOS) for TLS support
- JPEGLIB 9 or later for JPEG image support
- LIBPNG 1.6 or later for PNG image support
- PAM for authentication support
- ZLIB 1.1 or later for compression support

Run the following commands to install the prerequisites on CentOS 7, Fedora 22,
and Red Hat Enterprise Linux 7:

    sudo yum groupinstall 'Development Tools'
    sudo yum install avahi-devel cups-devel gnutls-devel libjpeg-turbo-devel \
        libpam-devel libpng-devel libusbx-devel pam-devel zlib-devel

Run the following commands to install the prerequisites on CentOS 8, Fedora 23
or later, and Red Hat Enterprise Linux 8:

    sudo dnf groupinstall 'Development Tools'
    sudo dnf install avahi-devel cups-devel gnutls-devel libjpeg-turbo-devel \
        libpam-devel libpng-devel libusbx-devel pam-devel zlib-devel

Run the following commands to install the prerequisites on Debian GNU/Linux,
Raspbian, and Ubuntu:

    sudo apt-get install build-essential libavahi-client-dev libcups2-dev \
        libcupsimage2-dev libgnutls28-dev libjpeg-dev libpam-dev libpng-dev \
        libusb-1.0-0-dev zlib1g-dev

Finally, after installing Xcode from the AppStore run the following commands to
install the prerequisites on macOS:

    (install brew if necessary)
    brew install libjpeg
    brew install libpng
    brew install libusb

or download, build, and install libjpeg, libpng, and libusb from source.


Building PAPPL
--------------

PAPPL uses the usual `configure` script to generate a `make` file:

    ./configure [options]
    make

Use `./configure --help` to see a full list of options.

There is also an Xcode project under the `xcode` directory that can be used on
macOS:

    open xcode/pappl.xcodeproj

You can test the build by running the PAPPL test program:

    testsuite/testpappl


Installing PAPPL
----------------

Once you have successfully built PAPPL, install it using:

    sudo make install

By default everything will be installed under `/usr/local`.  Use the `--prefix`
configure option to override the base installation directory.  Set the
`DESTDIR`, `DSTROOT`, or `RPM_BUILD_ROOT` environment variables to redirect the
installation to a staging area, as is typically done for most software packaging
systems (using one of those environment variables...)


Hello, World!
=============

This chapter will guide you through creating a simple PCL printer application
based on the old CUPS "rastertohp" filter.  The complete code can be found in
the [hp-printer-app](https://github.com/michaelrsweet/hp-printer-app) project.


Detecting PAPPL
---------------

PAPPL can be detected using the `pkg-config` command, for example:

    if pkg-config --exists pappl; then
        ... 
    fi

In a makefile you can add the necessary compiler and linker options with:

    CFLAGS  +=      `pkg-config --cflags pappl`
    LIBS    +=      `pkg-config --libs pappl`


Header Files
------------

PAPPL provides a top-level header file that should be used:

    #include <pappl/pappl.h>

This header includes all of the base object headers in PAPPL as well as the
CUPS header files that provide the HTTP and IPP support functions.


The Main Loop
-------------

All printer applications require some sort of a main loop for processing IPP
requests, printing files, and so forth.  PAPPL provides the
[`papplMainloop`](#papplMainLoop) convenience function that provides a standard
command-line interface:

```
extern int
papplMainloop(int argc, char *argv[], const char *version,
              const char *footer_html, int num_drivers, pappl_driver_t *drivers,
              pappl_driver_cb_t driver_cb, pappl_ml_autoadd_cb_t autoadd_cb,
              const char *subcmd_name, pappl_ml_subcmd_cb_t subcmd_cb,
              pappl_ml_system_cb_t system_cb, pappl_ml_usage_cb_t usage_cb,
              void *data);
```

In the "hp-printer-app" project the `main` function just calls `papplMainloop`
to do all of the work:

```
int
main(int  argc, char *argv[])
{
  return (papplMainloop(argc, argv,
                        /*version*/"1.0",
                        /*footer_html*/NULL,
                        (int)(sizeof(pcl_drivers) / sizeof(pcl_drivers[0])),
                        pcl_drivers, pcl_callback, pcl_autoadd,
                        /*subcmd_name*/NULL, /*subcmd_cb*/NULL,
                        /*system_cb*/NULL,
                        /*usage_cb*/NULL,
                        /*data*/NULL));
}
```

As you can see, we pass in the command-line arguments, a version number ("1.0")
for our printer application, a list of drivers supported by the printer
application, a callback for the driver that will configure a printer for a
named driver, and a callback for automatically adding printers.

The "footer_html" argument can be provided to override the default footer text
that appears at the bottom of the web interface.  In this case we are passing
`NULL` to use the default.

The drivers list is a collection of names, descriptions, IEEE-1284 device IDs,
and extension pointers.  Ours looks like this:

```
static pappl_driver_t pcl_drivers[] =   // Driver information
{   /* name */          /* description */       /* device ID */ /* extension */
  { "hp_deskjet",       "HP Deskjet",           NULL,           NULL },
  { "hp_generic",       "Generic PCL",          "CMD:PCL;",     NULL },
  { "hp_laserjet",      "HP LaserJet",          NULL,           NULL }
};
```

[The driver callback](@) is responsible for providing the data associated with
each driver, while [the auto-add callback](@) tells PAPPL which driver to use
for the printers it finds.

The "subcmd\_name" and "subcmd\_cb" arguments specify a custom sub-command for
the printer application along with a function to call.  Since this printer
application has no custom commands, we pass `NULL` for both.

The "system_cb" argument specifies a callback for creating the
[system object](#the-system).  We pass `NULL` because we want the default
system object for this printer application, which supports multiple printers
and a web interface.

The "usage_cb" argument specifies a callback from displaying the program usage
message.  Passing `NULL` yields the default usage message.

The "data" argument specifies a pointer that will be passed to any of the
callback functions.  We pass `NULL` since we do not have any extra contextual
information to provide to the callbacks.

The `papplMainloop` function runs until all processing for the current
sub-command is complete, returning the exit status for the program.


The Driver Callback
-------------------

The PAPPL driver callback is called when the system is creating a new printer
object.  It receives pointers to the system, driver name, device URI, a
driver data structure, an IPP attributes pointer, and the callback data, and it
returns a boolean indicating whether the driver callback was successful:

```
typedef bool (*pappl_driver_cb_t)(pappl_system_t *system,
    const char *driver_name, const char *device_uri,
    pappl_driver_data_t *driver_data, ipp_t **driver_attrs, void *data);
```

A driver callback can communicate with the printer via its device URI as needed
to configure the driver, however our printer application doesn't need to do
that.

The first thing our `pcl_callback` function does is to set the printer
callbacks in the driver data structure:

```
driver_data->print           = pcl_print;
driver_data->rendjob         = pcl_rendjob;
driver_data->rendpage        = pcl_rendpage;
driver_data->rstartjob       = pcl_rstartjob;
driver_data->rstartpage      = pcl_rstartpage;
driver_data->rwriteline      = pcl_rwriteline;
driver_data->status          = pcl_status;
```

The `pcl_print` function prints a raw PCL file while the `pcl_r` functions print
raster graphics.  The `pcl_status` updates the printer status.

Next is the printer's native print format as a MIME media type, in this case HP
PCL:

```
driver_data->format          = "application/vnd.hp-pcl";
```

The default orientation and print quality follow:

```
driver_data->orient_default  = IPP_ORIENT_NONE;
driver_data->quality_default = IPP_QUALITY_NORMAL;
```

Then the values for the HP DeskJet driver:

```
if (!strcmp(driver_name, "hp_deskjet"))
{
  strncpy(driver_data->make_and_model, "HP DeskJet", sizeof(driver_data->make_and_model) - 1);

  driver_data->num_resolution  = 3;
  driver_data->x_resolution[0] = 150;
  driver_data->y_resolution[0] = 150;
  driver_data->x_resolution[1] = 300;
  driver_data->y_resolution[1] = 300;
  driver_data->x_resolution[2] = 600;
  driver_data->y_resolution[2] = 600;
  driver_data->x_default = driver_data->y_default = 300;

  driver_data->raster_types = PAPPL_PWG_RASTER_TYPE_BLACK_1 | PAPPL_PWG_RASTER_TYPE_BLACK_8 | PAPPL_PWG_RASTER_TYPE_SGRAY_8 | PAPPL_PWG_RASTER_TYPE_SRGB_8;

  driver_data->color_supported = PAPPL_COLOR_MODE_AUTO | PAPPL_COLOR_MODE_AUTO_MONOCHROME | PAPPL_COLOR_MODE_COLOR | PAPPL_COLOR_MODE_MONOCHROME;
  driver_data->color_default   = PAPPL_COLOR_MODE_AUTO;

  driver_data->num_media = (int)(sizeof(pcl_hp_deskjet_media) / sizeof(pcl_hp_deskjet_media[0]));
  memcpy(driver_data->media, pcl_hp_deskjet_media, sizeof(pcl_hp_deskjet_media));

  driver_data->sides_supported = PAPPL_SIDES_ONE_SIDED;
  driver_data->sides_default   = PAPPL_SIDES_ONE_SIDED;

  driver_data->num_source = 3;
  driver_data->source[0]  = "tray-1";
  driver_data->source[1]  = "manual";
  driver_data->source[2]  = "envelope";

  driver_data->num_type = 5;
  driver_data->type[0] = "stationery";
  driver_data->type[1] = "bond";
  driver_data->type[2] = "special";
  driver_data->type[3] = "transparency";
  driver_data->type[4] = "photographic-glossy";

  driver_data->left_right = 635;       // 1/4" left and right
  driver_data->bottom_top = 1270;      // 1/2" top and bottom

  for (i = 0; i < driver_data->num_source; i ++)
  {
    if (strcmp(driver_data->source[i], "envelope"))
      snprintf(driver_data->media_ready[i].size_name, sizeof(driver_data->media_ready[i].size_name), "na_letter_8.5x11in");
    else
      snprintf(driver_data->media_ready[i].size_name, sizeof(driver_data->media_ready[i].size_name), "env_10_4.125x9.5in");
  }
}
```

and the generic HP driver:

```
else if (!strcmp(driver_name, "hp_generic"))
{
  strncpy(driver_data->make_and_model, "Generic PCL Laser Printer", sizeof(driver_data->make_and_model) - 1);

  driver_data->num_resolution  = 2;
  driver_data->x_resolution[0] = 300;
  driver_data->y_resolution[0] = 300;
  driver_data->x_resolution[1] = 600;
  driver_data->y_resolution[1] = 600;
  driver_data->x_default = driver_data->y_default = 300;

  driver_data->raster_types = PAPPL_PWG_RASTER_TYPE_BLACK_1 | PAPPL_PWG_RASTER_TYPE_BLACK_8 | PAPPL_PWG_RASTER_TYPE_SGRAY_8;
  driver_data->force_raster_type = PAPPL_PWG_RASTER_TYPE_BLACK_1;

  driver_data->color_supported = PAPPL_COLOR_MODE_MONOCHROME;
  driver_data->color_default   = PAPPL_COLOR_MODE_MONOCHROME;

  driver_data->num_media = (int)(sizeof(pcl_generic_pcl_media) / sizeof(pcl_generic_pcl_media[0]));
  memcpy(driver_data->media, pcl_generic_pcl_media, sizeof(pcl_generic_pcl_media));

  driver_data->sides_supported = PAPPL_SIDES_ONE_SIDED | PAPPL_SIDES_TWO_SIDED_LONG_EDGE | PAPPL_SIDES_TWO_SIDED_SHORT_EDGE;
  driver_data->sides_default   = PAPPL_SIDES_ONE_SIDED;

  driver_data->num_source = 7;
  driver_data->source[0]  = "default";
  driver_data->source[1]  = "tray-1";
  driver_data->source[2]  = "tray-2";
  driver_data->source[3]  = "tray-3";
  driver_data->source[4]  = "tray-4";
  driver_data->source[5]  = "manual";
  driver_data->source[6]  = "envelope";

  driver_data->left_right = 635;      // 1/4" left and right
  driver_data->bottom_top = 423;      // 1/6" top and bottom

  for (i = 0; i < driver_data->num_source; i ++)
  {
    if (strcmp(driver_data->source[i], "envelope"))
      snprintf(driver_data->media_ready[i].size_name, sizeof(driver_data->media_ready[i].size_name), "na_letter_8.5x11in");
    else
      snprintf(driver_data->media_ready[i].size_name, sizeof(driver_data->media_ready[i].size_name), "env_10_4.125x9.5in");
  }
}
```

and the HP LaserJet driver:

```
else if (!strcmp(driver_name, "hp_laserjet"))
{
 strncpy(driver_data->make_and_model, "HP LaserJet", sizeof(driver_data->make_and_model) - 1);

  driver_data->num_resolution  = 3;
  driver_data->x_resolution[0] = 150;
  driver_data->y_resolution[0] = 150;
  driver_data->x_resolution[1] = 300;
  driver_data->y_resolution[1] = 300;
  driver_data->x_resolution[2] = 600;
  driver_data->y_resolution[2] = 600;
  driver_data->x_default = driver_data->y_default = 300;

  driver_data->raster_types = PAPPL_PWG_RASTER_TYPE_BLACK_1 | PAPPL_PWG_RASTER_TYPE_BLACK_8 | PAPPL_PWG_RASTER_TYPE_SGRAY_8;
  driver_data->force_raster_type = PAPPL_PWG_RASTER_TYPE_BLACK_1;

  driver_data->color_supported = PAPPL_COLOR_MODE_MONOCHROME;
  driver_data->color_default   = PAPPL_COLOR_MODE_MONOCHROME;

  driver_data->num_media = (int)(sizeof(pcl_hp_laserjet_media) / sizeof(pcl_hp_laserjet_media[0]));
  memcpy(driver_data->media, pcl_hp_laserjet_media, sizeof(pcl_hp_laserjet_media));

  driver_data->sides_supported = PAPPL_SIDES_ONE_SIDED | PAPPL_SIDES_TWO_SIDED_LONG_EDGE | PAPPL_SIDES_TWO_SIDED_SHORT_EDGE;
  driver_data->sides_default   = PAPPL_SIDES_ONE_SIDED;

  driver_data->num_source = 7;
  driver_data->source[0]  = "default";
  driver_data->source[1]  = "tray-1";
  driver_data->source[2]  = "tray-2";
  driver_data->source[3]  = "tray-3";
  driver_data->source[4]  = "tray-4";
  driver_data->source[5]  = "manual";
  driver_data->source[6]  = "envelope";

  driver_data->left_right = 635;       // 1/4" left and right
  driver_data->bottom_top = 1270;      // 1/2" top and bottom

  for (i = 0; i < driver_data->num_source; i ++)
  {
    if (strcmp(driver_data->source[i], "envelope"))
      snprintf(driver_data->media_ready[i].size_name, sizeof(driver_data->media_ready[i].size_name), "na_letter_8.5x11in");
    else
      snprintf(driver_data->media_ready[i].size_name, sizeof(driver_data->media_ready[i].size_name), "env_10_4.125x9.5in");
  }
}
else
{
  papplLog(system, PAPPL_LOGLEVEL_ERROR, "No dimension information in driver name '%s'.", driver_name);
  return (false);
}
```

Finally, we fill out the ready and default media for each media source (tray):

```
// Fill out ready and default media (default == ready media from the first source)
for (i = 0; i < driver_data->num_source; i ++)
{
  pwg_media_t *pwg = pwgMediaForPWG(driver_data->media_ready[i].size_name);

  if (pwg)
  {
    driver_data->media_ready[i].bottom_margin = driver_data->bottom_top;
    driver_data->media_ready[i].left_margin   = driver_data->left_right;
    driver_data->media_ready[i].right_margin  = driver_data->left_right;
    driver_data->media_ready[i].size_width    = pwg->width;
    driver_data->media_ready[i].size_length   = pwg->length;
    driver_data->media_ready[i].top_margin    = driver_data->bottom_top;
    snprintf(driver_data->media_ready[i].source, sizeof(driver_data->media_ready[i].source), "%s", driver_data->source[i]);
    snprintf(driver_data->media_ready[i].type, sizeof(driver_data->media_ready[i].type), "%s", driver_data->type[0]);
  }
}

driver_data->media_default = driver_data->media_ready[0];

return (true);
```


The Auto-Add Callback
---------------------

The PAPPL auto-add callback is called when processing the "autoadd" sub-command.
It is called for each new device and is responsible for returning the name of
the driver to be used for the device or `NULL` if no driver is available:

```
typedef const char *(*pappl_ml_autoadd_cb_t)(const char *device_info,
    const char *device_uri, const char *device_id, void *data);
```

Our `pcl_autoadd` function uses the IEEE-1284 device ID string to determine
whether one of the drivers will work.  The [`papplDeviceParse1284ID`](@@)
function splits the string into key/value pairs that can be looked up using the
`cupsGetOption` function:

```
const char      *ret = NULL;            // Return value
int             num_did;                // Number of device ID key/value pairs
cups_option_t   *did;                   // Device ID key/value pairs
const char      *cmd,                   // Command set value
                *pcl;                   // PCL command set pointer


// Parse the IEEE-1284 device ID to see if this is a printer we support...
num_did = papplDeviceParse1284ID(device_id, &did);
```

The two keys we care about are the "COMMAND SET" (also abbreviated as "CMD") for
the list of document formats the printer supports and "MODEL"/"MDL" for the
model name.  We are looking for the "PCL" format and one of the common model
names for HP printers:

```
// Look at the COMMAND SET (CMD) key for the list of printer languages,,,
if ((cmd = cupsGetOption("COMMAND SET", num_did, did)) == NULL)
  cmd = cupsGetOption("CMD", num_did, did);

if (cmd && (pcl = strstr(cmd, "PCL")) != NULL && (pcl[3] == ',' || !pcl[3]))
{
  // Printer supports HP PCL, now look at the MODEL (MDL) string to see if
  // it is one of the HP models or a generic PCL printer...
  const char *mdl;                      // Model name string

  if ((mdl = cupsGetOption("MODEL", num_did, did)) == NULL)
    mdl = cupsGetOption("MDL", num_did, did);

  if (mdl && (strstr(mdl, "DeskJet") || strstr(mdl, "Photosmart")))
    ret = "hp_deskjet";                 // HP DeskJet/Photosmart printer
  else if (mdl && strstr(mdl, "LaserJet"))
    ret = "hp_laserjet";                // HP LaserJet printer
  else
    ret = "hp_generic";                 // Some other PCL laser printer
}

cupsFreeOptions(num_did, did);

return (ret);
```


The System
==========

The system is an object that manages printers, resources, listeners, and client
connections.


Creating a System
-----------------


Adding Listeners
----------------


Loading and Saving State
------------------------


IPP Server
----------

PAPPL fully implements the [IPP Everywhere™ specification][8] and passes the
[IPP Everywhere™ Printer Self-Certification Manual][9] tests.  PAPPL also
implements several IPP extensions used for IPP-based licensing programs to
simplify certification, including the CUPS "marker-xxx" attributes, the
Get-Printer-Attributes operation using the resource path "/", and the
CUPS-Get-Printers operation.

When configured to support multiple printers, PAPPL implements a subset of the
[IPP System Service v1.0 specification][10] to allow creation, deletion, and
enumeration of printers.


DNS-SD Discovery
----------------

PAPPL takes care of registration of DNS-SD (Bonjour) services for each printer,
including the required sub-types, "flagship" LPD registrations, and recommended
naming and renaming behavior.


File Formats
------------

PAPPL supports JPEG, PNG, PWG Raster, and Apple Raster documents in all of the
standard color spaces and bit depths.  JPEG and PNG images are scaled to the
destination media size and print resolution.  PWG Raster and Apple Raster
documents are *not* scaled as they are normally sent at the proper resolution
and size by the print client.

PAPPL also allows drivers to advertise support for other "raw" formats that are
directly supported by the printer.  And applications can register filters for
other formats.


Driver Interface
----------------

PAPPL provides a simple driver interface for raster printing, and developers of
CUPS Raster drivers will readily adapt to it.  Drivers provide configuration
and capability information to PAPPL, and PAPPL then calls the driver to start
a job, start a page, output lines of graphics, end a page, and finally end a
job during the processing of a print job.

The driver interface supports 1-bit grayscale (clustered- or dispersed-dot)
and 1-bit bi-level (threshold) dithering using a 16x16 matrix, which is
sufficient to support most B&W printing needs.  Continuous tone printing is
supported using 8-bit and 16-bit per component sGray, sRGB, AdobeRGB, or
DeviceN (K, RGB, CMYK, etc.) raster data.

Drivers can also specify "raw" formats that the printer accepts directly - this
is most useful for printers that support industry standard formats such as FGL,
PCL, or ZPL which are produced directly by common shipping and billing
automation applications.  "Raw" files are submitted to the driver using a
separate "print file" interface, allowing the driver to add any printer-specific
commands that are needed to successfully print them.

Aside from printing functionality, drivers can also provide up-to-date status
and configuration information by querying the printer when requested by the
embedded server.  This is an improvement over the CUPS command file interface
and allows a PAPPL-based driver to provide details such as updated media
information.

Drivers can also support printer identification, usually a sound or a light on
the printer, which is a requirement for IPP Everywhere™ and is used to visually
or audibly isolate a particular printer for the user.


Embedded Web Interface
----------------------

The embedded server can also provide a web interface to the Printer Application,
and PAPPL includes a standard web interface that can be customized and/or
overridden.  Aside from the usual status monitoring functionality, the web
interface can be configured to allow remote users (with proper authentication)
to:

- Create and delete printers,
- Set the printer location and DNS-SD name,
- Configure the loaded media,
- Configure remote access accounts,
- Configure networking settings such as the hostname, and/or
- Update the TLS certificates used by the server.

You can also add custom pages and content using callbacks, static data, or
external files or directories.

> Note: An embedded web interface is required for IPP Everywhere™ conformance.
> The optional features allow a Printer Application to easily support the
> functionality required for other IPP-based licensing programs.

Devices
=======

PAPPL provides a simple device API for reading data from and writing data to a
printer, as well as get interface-specific status information.  PAPPL devices
are referenced using Universal Resource Identifiers (URIs), with the URI scheme
referring to the device interface, for example:

- "dnssd": Network (AppSocket) printers discovered via DNS-SD/mDNS (Bonjour),
- "file": Local files and directories,
- "snmp": Network (AppSocket) printers discovered via SNMPv1,
- "socket": Network (AppSocket) printers using a numeric IP address or hostname
  and optional port number, and
- "usb": Local USB printer.

PAPPL supports custom device URI schemes which are registered using the
[`papplDeviceAddScheme'](#papplDeviceAddScheme) function.

Devices are accessed via a pointer to the [`pappl_device_t`](#pappl_device_t)
structure.  Print drivers use either the current job's device pointer or call
[`papplPrinterOpenDevice`](#papplPrinterOpenDevice) to open a printer's device
and [`papplPrinterCloseDevice`](#papplPrinterCloseDevice) to close it.

The [`papplDeviceRead`](#papplDeviceRead) function reads data from a device.
Typically a driver only calls this function after sending a command to the
printer requesting some sort of information.  Since not all printers or
interfaces support reading, a print driver *must* be prepared for a read to
fail.

The [`papplDevicePrintf`](#papplDevicePrintf),
[`papplDevicePuts`](#papplDevicePuts), and
[`papplDeviceWrite`](#papplDeviceWrite) functions write data to a device.  The
first two send strings to the device while the last writes arbitrary data.

The [`papplDeviceGetStatus`](#papplDeviceGetStatus) function returns device-
specific state information as a [`pappl_preason_t`](#pappl_preason_t) bitfield.






Clients
=======

The PAPPL client functions provide access to client connections.  Client
connections and the life cycle of the `pappl_client_t` objects are managed
automatically by the system object for the printer application.

The `papplClientGet` functions get the current values for various client-
supplied request data:

- [`papplClientGetCSRFToken`](@@): Get the current CSRF token.
- [`papplClientGetCookie`](@@): Get the value of a named cookie.
- [`papplClientGetForm`](@@): Get form data from the client request.
- [`papplClientGetHTTP`](@@): Get the HTTP connection associate with a client.
- [`papplClientGetHostName`](@@): Get the host name used for the client request.
- [`papplClientGetHostPort`](@@): Get the host port used for the client request.
- [`papplClientGetJob`](@@): Get the job object associated with the client
  request.
- [`papplClientGetMethod`](@@): Get the HTTP request method.
- [`papplClientGetOperation`](@@): Get the IPP operation code.
- [`papplClientGetOptions`](@@): Get any options from the HTTP request URI.
- [`papplClientGetPrinter`](@@): Get the printer object associated with the
  client request.
- [`papplClientGetRequest`](@@): Get the IPP request.
- [`papplClientGetResponse`](@@): Get the IPP response.
- [`papplClientGetSystem`](@@): Get the system object associated with the
  client.
- [`papplClientGetURI`](@@): Get the HTTP request URI.
- [`papplClientGetUsername`](@@): Get the authenticated username, if any.


Responding to Client Requests
-----------------------------

The [`papplClientRespond`](@@) function starts a HTTP response to a client
request.  The [`papplClientHTMLHeader`](@@) and [`papplClientHTMLFooter`](@@)
functions send standard HTML headers and footers for the printer application's
configured web interface while the [`papplClientHTMLEscape`](@@),
[`papplClientHTMLPrintf`](@@), [`papplClientHTMLPuts`](@@), and
[`papplClientHTMLStartForm`](@@) functions send HTML messages or strings.  Use
the [`papplClientGetHTTP`](@@) and (CUPS) `httpWrite2` functions to send
arbitrary data in a client response.  Cookies can be included in web browser
requests using the [`papplClientSetCookie`](@@) function.

The [`papplClientRespondIPP`](@@) function starts an IPP response.  Use the
various CUPS `ippAdd` functions to add attributes to the response message.

The [`papplClientRespondRedirect`](@@) function sends a redirection response to
the client.


HTML Forms
----------

PAPPL provides the [`papplClientGetCSRFToken`](@@), [`papplClientGetForm`](@@),
[`papplClientHTMLStartForm`](@@), and [`papplClientValidateForm`](@@) functions
to securely manage HTML forms.

The [`papplClientHTMLStartForm`](@@) function starts a HTML form and inserts a
hidden variable containing a CSRF token that was generated by PAPPL from a
secure session key that is periodically updated.  Upon receipt of a follow-up
form submission request, the [`papplClientGetForm`](@@) and
[`papplClientValidateForm`](@@) functions can be used to securely read the form
data (including any file attachments) and validate the hidden CSRF token.


Authentication and Authorization
--------------------------------

PAPPL supports both user-based authentication using PAM modules and a simple
cookie-based password authentication mechanism that is used to limit
administrative access through the web interface.

The [`papplHTMLAuthorize`](@@) function authorizes access to the web interface
and handles displaying an authentication form on the client's web browser.
The return value indicates whether the client is authorized to access the web
page.

The [`papplIsAuthorized`](@@) function can be used to determine whether the
current client is authorized to perform administrative operations and is
normally only used for IPP clients.  Local users are always authorized while
remote users must provide credentials (typically a username and password) for
access.  This function will return an HTTP status code that can be provided to
the [`httpClientSendResponse`](@@) function. The value `HTTP_STATUS_CONTINUE`
indicates that authorization is granted and the request should continue.  The
[`papplGetUsername`](@@) function can be used to obtain the authenticated user
identity.









Printers
========

Printers are managed by the system and are represented by the
[`pappl_printer_t`](#pappl_printer_t) type.  Each printer is connected to a
device and uses a driver to process document data and produce output.  PAPPL
supports raster printers out-of-the-box and provides filter callbacks to support
other kinds of printers.


Creating a Printer
------------------


Writing Printer Drivers
-----------------------


File Filters
------------



Jobs
====


Resources
=========


The Main Loop
=============

