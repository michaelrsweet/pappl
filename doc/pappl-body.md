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

PAPPL is Copyright © 2019-2022 by Michael R Sweet and is licensed under the
Apache License Version 2.0 with an (optional) exception to allow linking against
GPL2/LGPL2 software (like older versions of CUPS), so it can be used *freely* in
any project you'd like.  See the files "LICENSE" and "NOTICE" in the source
distribution for more information.


Requirements
------------

PAPPL requires Microsoft® Windows® 10 or higher or a POSIX-compliant host
operating system such as Linux®, macOS®, QNX®, or VxWorks®.  On Windows, the
provided project files require Visual Studio 2019 or higher.  For POSIX hosts,
a "make" utility that supports the `include` directive (like GNU make), a
C99-compatible C compiler such as GCC or Clang, and the "pkg-config" utility
are required along with the following support libraries:

- Avahi (0.8 or later) or mDNSResponder for mDNS/DNS-SD support
- CUPS (2.2 or later) or libcups (3.0 or later) for the CUPS libraries
- GNU TLS (3.0 or later), LibreSSL (3.0 or later), or OpenSSL (1.1 or later)
  for TLS support
- JPEGLIB (8 or later) or libjpeg-turbo (2.0 or later) for JPEG image support
  (optional for B&W printers)
- LIBPNG (1.6 or later) for PNG image support (optional)
- LIBPAM for authentication support (optional)
- LIBUSB (1.0 or later) for USB printing support (optional)
- PAM for authentication support (optional)
- ZLIB (1.1 or later) for compression support


Getting Prerequisites
---------------------

CentOS 8/Fedora 23+/RHEL 8+:

    sudo dnf groupinstall 'Development Tools'
    sudo dnf install avahi-devel cups-devel libjpeg-turbo-devel \
        libpng-devel libssl-devel libusbx-devel pam-devel zlib-devel

Debian/Raspbian/Ubuntu:

    sudo apt-get install build-essential libavahi-client-dev libcups2-dev \
        libcupsimage2-dev libjpeg-dev libpam-dev libpng-dev libssl-dev \
        libusb-1.0-0-dev zlib1g-dev

macOS (after installing Xcode from the AppStore):

    (install brew if necessary from <https://brew.sh>)
    brew install libjpeg
    brew install libpng
    brew install libusb
    brew install openssl@3

or download, build, and install libjpeg, libpng, libusb, and OpenSSL from
source.

Windows (after installing Visual Studio 2019 or later) will automatically
install the prerequisites via NuGet packages.


Building PAPPL
--------------

PAPPL uses the usual `configure` script to generate a `make` file:

    ./configure [options]
    make

Use `./configure --help` to see a full list of options.

There is also an Xcode project under the `xcode` directory that can be used on
macOS:

    open xcode/pappl.xcodeproj

and a Visual Studio solution under the `vcnet` directory that must be used on
Windows.

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


Detecting PAPPL
---------------

PAPPL can be detected using the `pkg-config` command, for example:

    if pkg-config --exists pappl; then
        ...
    fi

In a makefile you can add the necessary compiler and linker options with:

```make
CFLAGS  +=      `pkg-config --cflags pappl`
LIBS    +=      `pkg-config --libs pappl`
```


Header Files
------------

PAPPL provides a top-level header file that should be used:

```c
#include <pappl/pappl.h>
```

This header includes all of the base object headers in PAPPL as well as the
CUPS header files that provide the HTTP and IPP support functions.


API Overview
============

PAPPL provides five main objects:

- [The System](@) (`pappl_system_t`): The main object that manages the whole
  printer application;
- [Clients](@) (`pappl_client_t`): The objects that manage client connections;
- [Devices](@) (`pappl_device_t`): The objects that manage printer connections;
- [Printers](@) (`pappl_printer_t`): The objects that manage printers; and
- [Jobs](@) (`pappl_job_t`): The objects that manage print jobs.

![PAPPL Block Diagram::100%](../doc/pappl-block.png)


The System
----------

The system is an object of type [`pappl_system_t`](@@) that manages client and
device connections, listeners, the log, printers, and resources.  It implements
a subset of the IPP System Service
([PWG 5100.22](https://ftp.pwg.org/pub/pwg/candidates/cs-ippsystem10-20191122-5100.22.pdf))
with each printer implementing IPP Everywhere™
([PWG 5100.14](https://ftp.pwg.org/pub/pwg/candidates/cs-ippeve11-20200515-5100.14.pdf))
and some extensions to provide compatibility with the full range of mobile and
desktop client devices.  In addition, it provides an optional embedded web
interface, raw socket printing, and USB printer gadget (Linux only).

A system object is created using the [`papplSystemCreate`](@@) function and
deleted using the [`papplSystemDelete`](@@) function.  Each system manages zero
or more [printers](@) which can be accessed using either of the following
functions:

- [`papplSystemFindPrinter`](@@): Finds the named or numbered print queue, and
- [`papplSystemIteratePrinters`](@@): Iterates all print queues managed by the
  system.

The [`papplSystemLoadState`](@@) function is often used to load system values
and printers from a prior run which used the [`papplSystemSaveState`](@@)
function.

IP and domain socket listeners are added using the
[`papplSystemAddListeners`](@@) function.

The `papplSystemGet` functions get various system values:

- [`papplSystemGetAdminGroup`](@@): Gets the administrative group name,
- [`papplSystemGetAuthService`](@@): Gets the PAM authorization service name,
- [`papplSystemGetContact`](@@): Gets the contact information for the system,
- [`papplSystemGetDefaultPrinterID`](@@): Gets the default printer's ID number,
- [`papplSystemGetDefaultPrintGroup`](@@): Gets the default print group name,
- [`papplSystemGetDNSSDName`](@@): Gets the system's DNS-SD service instance
  name,
- [`papplSystemGetFooterHTML`](@@): Gets the HTML to use at the bottom of the
  web interface,
- [`papplSystemGetGeoLocation`](@@): Gets the geographic location as a "geo:"
  URI,
- [`papplSystemGetHostName`](@@): Gets the hostname for the system,
- [`papplSystemGetHostPort`](@@): Gets the port number assigned to the system,
- [`papplSystemGetLocation`](@@): Gets the human-readable location,
- [`papplSystemGetLogLevel`](@@): Gets the current log level,
- [`papplSystemGetMaxClients`](@@): Gets the maximum number of simultaneous
  network clients that are allowed,
- [`papplSystemGetMaxLogSize`](@@): Gets the maximum log file size (when logging
  to a file),
- [`papplSystemGetMaxSubscriptions`](@@): Gets the maximum number of event
  subscriptions that are allowed,
- [`papplSystemGetName`](@@): Gets the name of the system that was passed to
  [`papplSystemCreate`](@@),
- [`papplSystemGetNextPrinterID`](@@): Gets the ID number that will be used for
  the next printer that is created,
- [`papplSystemGetOptions`](@@): Gets the system options that were passed to
  [`papplSystemCreate`](@@),
- [`papplSystemGetOrganization`](@@): Gets the organization name,
- [`papplSystemGetOrganizationalUnit`](@@): Gets the organizational unit name,
- [`papplSystemGetPassword`](@@): Gets the web interface access password,
- [`papplSystemGetServerHeader`](@@): Gets the HTTP "Server:" header value,
- [`papplSystemGetSessionKey`](@@): Gets the current cryptographic session key,
- [`papplSystemGetTLSOnly`](@@): Gets the "tlsonly" value that was passed to
  [`papplSystemCreate`](@@),
- [`papplSystemGetUUID`](@@): Gets the UUID assigned to the system, and
- [`papplSystemGetVersions`](@@): Gets the firmware version numbers that are
  reported to clients.

Similarly, the `papplSystemSet` functions set various system values:

- [`papplSystemSetAdminGroup`](@@): Sets the administrative group name,
- [`papplSystemSetContact`](@@): Sets the contact information for the system,
- [`papplSystemSetDefaultPrinterID`](@@): Sets the ID number of the default
  printer,
- [`papplSystemSetDefaultPrintGroup`](@@): Sets the default print group name,
- [`papplSystemSetPrinterDrivers`](@@): Sets the list of printer drivers,
- [`papplSystemSetDNSSDName`](@@): Sets the DNS-SD service instance name,
- [`papplSystemSetFooterHTML`](@@): Sets the HTML to use at the bottom of the
  web interface,
- [`papplSystemSetGeoLocation`](@@): Sets the geographic location of the system
  as a "geo:" URI,
- [`papplSystemSetHostName`](@@): Sets the system hostname,
- [`papplSystemSetLocation`](@@): Sets the human-readable location,
- [`papplSystemSetLogLevel`](@@): Sets the current log level,
- [`papplSystemSetMaxClients`](@@): Sets the maximum number of simultaneous
  network clients that are allowed,
- [`papplSystemSetMaxLogSize`](@@): Sets the maximum log file size (when logging
  to a file),
- [`papplSystemSetMaxSubscriptions`](@@): Sets the maximum number of event
  subscriptions that are allowed,
- [`papplSystemSetMIMECallback`](@@): Sets a MIME media type detection callback,
- [`papplSystemSetNextPrinterID`](@@): Sets the ID to use for the next printer
  that is created,
- [`papplSystemSetOperationCallback`](@@): Sets an IPP operation callback,
- [`papplSystemSetOrganization`](@@): Sets the organization name,
- [`papplSystemSetOrganizationalUnit`](@@): Sets the organizational unit name,
- [`papplSystemSetPassword`](@@): Sets the web interface access password,
- [`papplSystemSetSaveCallback`](@@): Sets a save callback, usually
  [`papplSystemSaveState`](@@), that is used to save configuration and state
  changes as the system runs,
- [`papplSystemSetUUID`](@@): Sets the UUID for the system, and
- [`papplSystemSetVersions`](@@): Sets the firmware versions that are reported
  to clients,


### Logging ###

The PAPPL logging functions record messages to the configured log file.  The
[`papplLog`](@@) records messages applying to the system as a whole while
[`papplLogClient`](@@), [`papplLogJob`](@@), and [`papplLogPrinter`](@@) record
messages specific to a client connection, print job, or printer respectively.

The "level" argument specifies a log level from debugging
(`PAPPL_LOGLEVEL_DEBUG`) to fatal (`PAPPL_LOGLEVEL_FATAL`) and is used to
determine whether the message is recorded to the log.

The "message" argument specifies the message using a `printf` format string.


### Navigation Links ###

Navigation links can be added to the web interface using the
[`papplSystemAddLink`](@@) function and removed using the
[`papplSystemRemoveLink`](@@) function.


### Run Loops ###

PAPPL provides two functions to manage its run loops:

- [`papplSystemRun`](@@): Runs a system object that you have created and
  configured, and
- [`papplMainloop`](@@): Handles processing standard command-line arguments and
  sub-commands for a printer application.

You can learn more about the second function in the chapter on the
[HP Printer Application Example](@).

The system will run until it receives a Shutdown-System request, a termination
signal, or you call the [`papplSystemShutdown`](@@) function.  You can test
whether is system is running with the [`papplSystmeIsRunning`](@@) function and
whether it has been shut down with the [`papplSystemIsShutdown`](@@) function.


### Resources ###

PAPPL provides several functions for adding static and dynamic resources to the
embedded HTTP server:

- [`papplSystemAddResourceCallback`](@@): Adds a callback to serve dynamic
  content (HTTP GET or POST requests),
- [`papplSystemAddResourceData`](@@): Adds a static resource using an
  `unsigned char` array,
- [`papplSystemAddResourceDirectory`](@@): Adds static resource files from a
  directory,
- [`papplSystemAddResourceFile`](@@): Adds a static resource file,
- [`papplSystemAddResourceString`](@@): Adds a static resource using a constant
  string,
- [`papplSystemAddStringsData`](@@): Adds a localization file from a constant
  string, and
- [`papplSystemAddStringsFile`](@@): Adds a localization file from a file.

Resources may be removed using the [`papplSystemRemoveResource`](@@) function.


Localization
------------

Resources added using the [`papplSystemAddStringsData`](@@) and
[`papplSystemAddStringsFile`](@@) functions are automatically used to localize
the web interface and are provided to clients to localize printer-specific
attributes and values.

PAPPL provides several additional localization functions that can be used to
localize your own dynamic content and/or main loop sub-command:

- [`papplClientGetLoc`](@@) returns the collection of localization strings for
  the client's preferred language,
- [`papplClientGetLocString`](@@) returns a localized string for the client's
  preferred language,
- [`papplLocFormatString`](@@) formats a localized string from a given
  collection,
- [`papplLocGetString`](@@) returns a localized string from a given
  collection, and
- [`papplSystemFindLoc`](@@) finds the collection of localization strings for
  the specified language.

Key to these functions is the [`pappl_loc_t`](@@) collection of localization
strings, which combines the strings from the built-in PAPPL localizations and
any strings file resources that have been added.


### Web Interface Localization ###

When localizing the web interface, PAPPL looks up strings for titles and labels
passed to [`papplClientHTMLHeader`](@@), [`papplClientHTMLPrinterHeader`](@@),
[`papplPrinterAddLink`](@@), and [`papplSystemAddLink`](@@), the footer HTML
passed to [`papplSystemSetFooterHTML`](@@), string versions passed to
[`papplSystemSetVersions`](@@), and attribute names and values for the
"Printing Defaults" web pages.  The key string for most localization strings is
the English text, for example the title "Example Page" might be localized to
French with the following line in the corresponding ".strings" file:

    "Example Page" = "Exemple de Page";

Attribute names are localized using the IPP attribute name string, for example
"smi32473-algorithm" might be localized as:

    /* English strings file */
    "smi32473-algorithm" = "Dithering Algorithm";

    /* French strings file */
    "smi32473-algorithm" = "Algorithme de Tramage";

Attribute values are localized by concatenating the attribute name with the
enum (numeric) or keyword (string) values.  For example, the values "ordered"
and "contone" might be localized as:

    /* English strings file */
    "smi32473-algorithm.ordered" = "Patterned";
    "smi32473-algorithm.contone" = "Continuous Tone";

    /* French strings file */
    "smi32473-algorithm.ordered" = "À Motifs";
    "smi32473-algorithm.contone" = "Tonalité Continue";

Web pages can display a localized HTML banner for the resource path, for
example:

    /* English strings file */
    "/" = "Example text for the root web page.";
    "/network" = "<p>Example text for the <em>network</em> web page.</p>";

    /* French strings file */
    "/" = "Exemple de texte pour la page Web racine.";
    "/network" = "<p>Exemple de texte pour la page Web du <em>réseau</em>.</p>";


Clients
-------

The PAPPL client functions provide access to client connections.  Client
connections and the life cycle of the [`pappl_client_t`](@@) objects are managed
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
- [`papplClientGetLoc`](@@): Get the client's preferred localization.
- [`papplClientGetLocString`](@@): Get a localized string using the client's preferred localization.
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


### Responding to Client Requests ###

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


### HTML Forms ###

PAPPL provides the [`papplClientGetCSRFToken`](@@), [`papplClientGetForm`](@@),
[`papplClientHTMLStartForm`](@@), and [`papplClientValidateForm`](@@) functions
to securely manage HTML forms.

The [`papplClientHTMLStartForm`](@@) function starts a HTML form and inserts a
hidden variable containing a CSRF token that was generated by PAPPL from a
secure session key that is periodically updated.  Upon receipt of a follow-up
form submission request, the [`papplClientGetForm`](@@) and
[`papplClientValidateForm`](@@) functions can be used to securely read the form
data (including any file attachments) and validate the hidden CSRF token.


### Authentication and Authorization ###

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


Devices
-------

The PAPPL device functions provide access to output device connections and to
list available output devices.  Output devices are accessed using Uniform
Resource Identifier (URI) strings such as "file:///path/to/file-or-directory",
"socket://11.22.33.44", and "usb://make/model?serial=number".  The follow URI
schemes are supported by PAPPL:

- "dnssd": Network (AppSocket) printers discovered via DNS-SD/mDNS (Bonjour),
- "file": Local files and directories,
- "snmp": Network (AppSocket) printers discovered via SNMPv1,
- "socket": Network (AppSocket) printers using a numeric IP address or hostname
  and optional port number, and
- "usb": Local USB printer.

Custom device URI schemes can be registered using the
[`papplDeviceAddScheme`](@@) function.

The [`papplDeviceList`](@@) function lists available output devices, providing
each available output device to the supplied callback function.  The list only
contains devices whose URI scheme supports discovery, at present USB printers
and network printers that advertise themselves using DNS-SD/mDNS and/or SNMPv1.

The [`papplDeviceOpen`](@@) function opens a connection to an output device
using its URI.  The [`papplDeviceClose`](@@) function closes the connection.

The [`papplDevicePrintf`](@@), [`papplDevicePuts`](@@), and
[`papplDeviceWrite`](@@) functions send data to the device, while the
[`papplDeviceRead`](@@) function reads data from the device.

The `papplDeviceGet` functions get various device values:

- [`papplDeviceGetID`](@@): Gets the current IEEE-1284 device ID string,
- [`papplDeviceGetMetrics`](@@): Gets statistical information about all
  communications with the device while it has been open, and
- [`papplDeviceGetStatus`](@@): Gets the hardware status of a device mapped
  to the [`pappl_preason_t`](@@) bitfield.


Printers
--------

Printers are managed by the system and are represented by the
[`pappl_printer_t`](@@) type.  Each printer is connected to a device and uses a
driver to process document data and produce output.  PAPPL supports raster
printers out-of-the-box and provides filter callbacks to support other kinds of
printers.

Printers are created using the [`papplPrinterCreate`](@@) function and deleted
using the [`papplPrinterDelete`](@@) function.  Each printer has zero or more
jobs that are pending, processing (printing), or completed which can be access
using any of the following functions:

- [`papplPrinterFindJob`](@@): Finds the numbered print job,
- [`papplPrinterIterateActiveJobs`](@@): Iterates active print jobs managed by
  the printer,
- [`papplPrinterIterateAllJobs`](@@): Iterates all print jobs managed by the
  printer, and
- [`papplPrinterIterateCompletedJobs`](@@): Iterates completed print jobs
  managed by the printer.

The `papplPrinterGet` functions get various printer values:

- [`papplPrinterGetContact`](@@): Gets the contact information,
- [`papplPrinterGetDeviceID`](@@): Gets the IEEE-1284 device ID,
- [`papplPrinterGetDeviceURI`](@@): Gets the device URI,
- [`papplPrinterGetDNSSDName`](@@): Gets the DNS-SD service instance name,
- [`papplPrinterGetDriverAttributes`](@@): Gets the driver IPP attributes,
- [`papplPrinterGetDriverData`](@@): Gets the driver data,
- [`papplPrinterGetDriverName`](@@): Gets the driver name,
- [`papplPrinterGetGeoLocation`](@@): Gets the geographic location as a "geo:"
  URI,
- [`papplPrinterGetID`](@@): Gets the ID number,
- [`papplPrinterGetImpressionsCompleted`](@@): Gets the number of impressions
  (sides) that have been printed,
- [`papplPrinterGetLocation`](@@): Gets the human-readable location,
- [`papplPrinterGetMaxActiveJobs`](@@): Gets the maximum number of simultaneous
  active (queued) jobs,
- [`papplPrinterGetMaxCompletedJobs`](@@): Gets the maximum number of completed
  jobs for the job history,
- [`papplPrinterGetMaxPreservedJobs`](@@): Gets the maximum number of preserved
  jobs (with document data) for the job history,
- [`papplPrinterGetName`](@@): Gets the name,
- [`papplPrinterGetNextJobID`](@@): Gets the ID number of the next job that is
  created,
- [`papplPrinterGetNumberOfActiveJobs`](@@): Gets the current number of active
  jobs,
- [`papplPrinterGetNumberOfCompletedJobs`](@@): Gets the current number of
  completed jobs in the job history,
- [`papplPrinterGetNumberOfJobs`](@@): Gets the total number of jobs in memory,
- [`papplPrinterGetOrganization`](@@): Gets the organization name,
- [`papplPrinterGetOrganizationalUnit`](@@): Gets the organizational unit name,
- [`papplPrinterGetPath`](@@): Gets the path of a printer web page,
- [`papplPrinterGetPrintGroup`](@@): Gets the print authorization group name,
- [`papplPrinterGetReasons`](@@): Gets the "printer-state-reasons" bitfield,
- [`papplPrinterGetState`](@@): Gets the "printer-state" value,
- [`papplPrinterGetSupplies`](@@): Gets the current supply levels, and
- [`papplPrinterGetSystem`](@@): Gets the system managing the printer.

Similarly, the `papplPrinterSet` functions set those values:

- [`papplPrinterSetContact`](@@): Sets the contact information,
- [`papplPrinterSetDNSSDName`](@@): Sets the DNS-SD service instance name,
- [`papplPrinterSetDriverData`](@@): Sets the driver data and attributes,
- [`papplPrinterSetDriverDefaults`](@@): Sets the driver defaults,
- [`papplPrinterSetGeoLocation`](@@): Sets the geographic location as a "geo:"
  URI,
- [`papplPrinterSetImpressionsCompleted`](@@): Sets the number of impressions
  that have been printed,
- [`papplPrinterSetLocation`](@@): Sets the human-readable location,
- [`papplPrinterSetMaxActiveJobs`](@@): Sets the maximum number of jobs that can
  be queued,
- [`papplPrinterSetMaxCompletedJobs`](@@): Sets the maximum number of completed
  jobs that are kept in the job history,
- [`papplPrinterSetMaxPreservedJobs`](@@): Sets the maximum number of preserved
  jobs (with document data) that are kept in the job history,
- [`papplPrinterSetNextJobID`](@@): Sets the ID number of the next job that is
  created,
- [`papplPrinterSetOrganization`](@@): Sets the organization name,
- [`papplPrinterSetOrganizationalUnit`](@@): Sets the organizational unit name,
- [`papplPrinterSetPrintGroup`](@@): Sets the print authorization group name,
- [`papplPrinterSetReadyMedia`](@@): Sets the ready (loaded) media,
- [`papplPrinterSetReasons`](@@): Sets or clears "printer-state-reasons" values,
- [`papplPrinterSetSupplies`](@@): Sets supply level information, and
- [`papplPrinterSetUSB`](@@): Sets the USB vendor ID, product ID, and
  configuration options.


### Accessing the Printer Device ###

When necessary, the device associated with a printer can be opened with the
[`papplPrinterOpenDevice`](@@) function and subsequently closed using the
[`papplPrinterCloseDevice`](@@) function.


### Controlling Printers ###

Printers are stopped using the [`papplPrinterPause`](@@) function and started
using the [`papplPrinterResume`](@@) function.  New jobs can be held using the
[`papplPrinterHoldNewJobs`](@@) function and later released for printing using
the [`papplPrinterReleaseHeldNewJobs`](@@) function.


### Navigation Links ###

Navigation links can be added to the web interface using the
[`papplPrinterAddLink`](@@) function and removed using the
[`papplPrinterRemoveLink`](@@) function.


Jobs
----

Jobs are managed by the system and are represented by the [`pappl_job_t`](@@)
type.  Jobs are created and deleted automatically by the system object for the
printer application.

The `papplJobGet` functions get the current values associated with a job:

- [`papplJobGetAttribute`](@@): Gets a named Job Template attribute,
- [`papplJobGetData`](@@): Gets driver-specific processing data,
- [`papplJobGetFilename`](@@): Gets the filename of the document data,
- [`papplJobGetFormat`](@@): Gets the MIME media type for the document data,
- [`papplJobGetID`](@@): Gets the job's numeric ID,
- [`papplJobGetImpressions`](@@): Gets the number of impressions (sides) in the
  document,
- [`papplJobGetImpressionsCompleted`](@@): Gets the number of impressions
  (sides) that have been printed,
- [`papplJobGetMessage`](@@): Gets the current processing message (if any),
- [`papplJobGetName`](@@): Gets the job name/title,
- [`papplJobGetPrinter`](@@): Gets the printer for the job,
- [`papplJobGetReasons`](@@): Gets the "job-state-reasons" bitfield,
- [`papplJobGetState`](@@): Gets the "job-state" value,
- [`papplJobGetTimeCompleted`](@@): Gets the UNIX time when the job completed,
  aborted, or was canceled,
- [`papplJobGetTimeCreated`](@@): Gets the UNIX time when the job was created,
- [`papplJobGetTimeProcessed`](@@): Gets the UNIX time when processing started,
  and
- [`papplJobGetUsername`](@@): Gets the name of the user that created the job.

Similarly, the `papplJobSet` functions set the current values associated with
a job:

- [`papplJobSetData`](@@): Sets driver-specific processing data,
- [`papplJobSetImpressions`](@@): Sets the number of impressions (sides) in the
  job,
- [`papplJobSetImpressionsCompleted`](@@): Updates the number of impressions
  (sides) that have been completed,
- [`papplJobSetMessage`](@@): Set the current processing message, and
- [`papplJobSetReasons`](@@): Sets or clears bits in the "job-state-reasons"
  bitfield.


### Controlling Jobs ###

The [`papplJobCancel`](@@) function cancels processing of a job while the
[`papplJobIsCanceled`](@@) function returns whether a job is in the canceled
state (`IPP_JSTATE_CANCELED`) or is in the process of being canceled
(`IPP_JSTATE_PROCESSING` and `PAPPL_JREASON_PROCESSING_TO_STOP_POINT`).

The [`papplJobHold`](@@) function holds a job while the [`papplJobRelease`](@@)
function releases a job for printing.


### Processing Jobs ###

PAPPL stores print options in [`pappl_pr_options_t`](@@) objects.   The
[`papplJobCreatePrintOptions`](@@) function creates a new print option object
and initializes it using the job's attributes and printer defaults.  The
creator of a print options object must free it using the
[`papplJobDeletePrintOptions`](@@) function.

The [`papplJobOpenFile`](@@) function opens a file associated with the job.
The file descriptor must be closed by the caller using the `close` function.
The primary document file for a job can be retrieved using the
[`papplJobGetFilename`](@@) function, and its format using the
[`papplJobGetFormat`](@@) function.

Filters allow a printer application to support different file formats.  PAPPL
includes raster filters for PWG and Apple raster documents (streamed) as well as
JPEG and PNG image files.  Filters for other formats or non-raster printers can
be added using the [`papplSystemAddMIMEFilter`](@@) function.

The [`papplJobFilterImage`](@@) function converts raw image data to raster data
suitable for the printer, and prints using the printer driver's raster
callbacks.  Raster filters that output a single page can use this function to
handle the details of scaling, cropping, color space conversion, and dithering
for raster printers.  A raster filter that needs to print more than one image
must use the raster callback functions in the [`pappl_pr_driver_data_t`](@@)
structure directly.

Filters that produce non-raster data can call the `papplDevice` functions to
directly communicate with the printer in its native language.


The HP Printer Application Example
==================================

This chapter will guide you through creating a simple PCL printer application
based on the old CUPS "rastertohp" filter.  The complete code can be found in
the [hp-printer-app](https://github.com/michaelrsweet/hp-printer-app) project
and serves as an overgrown "Hello, World!" program for PAPPL.


The Main Loop
-------------

All printer applications require some sort of a main loop for processing IPP
requests, printing files, and so forth.  PAPPL provides the
[`papplMainloop`](#papplMainLoop) convenience function that provides a standard
command-line interface, and in the "hp-printer-app" project the `main` function
just calls `papplMainloop` to do all of the work:

```c
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
and extension pointers.  Ours looks like this for HP DeskJet, HP LaserJet, and
a generic PCL driver:

```c
static pappl_pr_driver_t pcl_drivers[] =// Driver information
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
application does not have a custom sub-command, we pass `NULL` for both.

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

```c
typedef bool (*pappl_pr_driver_cb_t)(pappl_system_t *system,
    const char *driver_name, const char *device_uri,
    pappl_pr_driver_data_t *driver_data, ipp_t **driver_attrs, void *data);
```

A driver callback can communicate with the printer via its device URI as needed
to configure the driver, however our printer application doesn't need to do
that.

The first thing our `pcl_callback` function does is to set the printer
callbacks in the driver data structure:

```c
driver_data->printfile_cb    = pcl_print;
driver_data->rendjob_cb      = pcl_rendjob;
driver_data->rendpage_cb     = pcl_rendpage;
driver_data->rstartjob_cb    = pcl_rstartjob;
driver_data->rstartpage_cb   = pcl_rstartpage;
driver_data->rwriteline_cb   = pcl_rwriteline;
driver_data->status_cb       = pcl_status;
```

The `pcl_print` function prints a raw PCL file while the `pcl_r` functions print
raster graphics.  The `pcl_status` updates the printer status.

Next is the printer's native print format as a MIME media type, in this case HP
PCL:

```c
driver_data->format          = "application/vnd.hp-pcl";
```

The default orientation and print quality follow:

```c
driver_data->orient_default  = IPP_ORIENT_NONE;
driver_data->quality_default = IPP_QUALITY_NORMAL;
```

Then the values for the various drivers.  Here are the HP DeskJet driver
settings:

```c
if (!strcmp(driver_name, "hp_deskjet"))
{
  /* Make and model name */
  strncpy(driver_data->make_and_model, "HP DeskJet", sizeof(driver_data->make_and_model) - 1);

  /* Pages-per-minute for monochrome and color */
  driver_data->ppm       = 8;
  driver_data->ppm_color = 2;

  /* Three resolutions - 150dpi, 300dpi (default), and 600dpi */
  driver_data->num_resolution  = 3;
  driver_data->x_resolution[0] = 150;
  driver_data->y_resolution[0] = 150;
  driver_data->x_resolution[1] = 300;
  driver_data->y_resolution[1] = 300;
  driver_data->x_resolution[2] = 600;
  driver_data->y_resolution[2] = 600;
  driver_data->x_default = driver_data->y_default = 300;

  /* Four color spaces - black (1-bit and 8-bit), grayscale, and sRGB */
  driver_data->raster_types = PAPPL_PWG_RASTER_TYPE_BLACK_1 | PAPPL_PWG_RASTER_TYPE_BLACK_8 | PAPPL_PWG_RASTER_TYPE_SGRAY_8 | PAPPL_PWG_RASTER_TYPE_SRGB_8;

  /* Color modes: auto (default), monochrome, and color */
  driver_data->color_supported = PAPPL_COLOR_MODE_AUTO | PAPPL_COLOR_MODE_AUTO_MONOCHROME | PAPPL_COLOR_MODE_COLOR | PAPPL_COLOR_MODE_MONOCHROME;
  driver_data->color_default   = PAPPL_COLOR_MODE_AUTO;

  /* Media sizes with 1/4" left/right and 1/2" top/bottom margins*/
  driver_data->num_media = (int)(sizeof(pcl_hp_deskjet_media) / sizeof(pcl_hp_deskjet_media[0]));
  memcpy(driver_data->media, pcl_hp_deskjet_media, sizeof(pcl_hp_deskjet_media));

  driver_data->left_right = 635;       // 1/4" left and right
  driver_data->bottom_top = 1270;      // 1/2" top and bottom

  /* 1-sided printing only */
  driver_data->sides_supported = PAPPL_SIDES_ONE_SIDED;
  driver_data->sides_default   = PAPPL_SIDES_ONE_SIDED;

  /* Three paper trays (MSN names) */
  driver_data->num_source = 3;
  driver_data->source[0]  = "tray-1";
  driver_data->source[1]  = "manual";
  driver_data->source[2]  = "envelope";

  /* Five media types (MSN names) */
  driver_data->num_type = 5;
  driver_data->type[0] = "stationery";
  driver_data->type[1] = "bond";
  driver_data->type[2] = "special";
  driver_data->type[3] = "transparency";
  driver_data->type[4] = "photographic-glossy";
}
```

Finally, we fill out the ready and default media for each media source (tray),
putting US Letter paper in the regular trays and #10 envelopes in any envelope
tray:

```c
// Fill out ready and default media (default == ready media from the first source)
for (i = 0; i < driver_data->num_source; i ++)
{
  pwg_media_t *pwg;                   /* Media size information */

  /* Use US Letter for regular trays, #10 envelope for the envelope tray */
  if (!strcmp(driver_data->source[i], "envelope"))
    strncpy(driver_data->media_ready[i].size_name, "env_10_4.125x9.5in", sizeof(driver_data->media_ready[i].size_name) - 1);
  else
    strncpy(driver_data->media_ready[i].size_name, "na_letter_8.5x11in", sizeof(driver_data->media_ready[i].size_name) - 1);

  /* Set margin and size information */
  if ((pwg = pwgMediaForPWG(driver_data->media_ready[i].size_name)) != NULL)
  {
    driver_data->media_ready[i].bottom_margin = driver_data->bottom_top;
    driver_data->media_ready[i].left_margin   = driver_data->left_right;
    driver_data->media_ready[i].right_margin  = driver_data->left_right;
    driver_data->media_ready[i].size_width    = pwg->width;
    driver_data->media_ready[i].size_length   = pwg->length;
    driver_data->media_ready[i].top_margin    = driver_data->bottom_top;
    strncpy(driver_data->media_ready[i].source, driver_data->source[i], sizeof(driver_data->media_ready[i].source) - 1);
    strncpy(driver_data->media_ready[i].type, driver_data->type[0],  sizeof(driver_data->media_ready[i].type) - 1);
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

```c
typedef const char *(*pappl_ml_autoadd_cb_t)(const char *device_info,
    const char *device_uri, const char *device_id, void *data);
```

Our `pcl_autoadd` function uses the IEEE-1284 device ID string to determine
whether one of the drivers will work.  The [`papplDeviceParseID`](@@) function
splits the string into key/value pairs that can be looked up using the
`cupsGetOption` function:

```c
const char      *ret = NULL;            // Return value
int             num_did;                // Number of device ID key/value pairs
cups_option_t   *did;                   // Device ID key/value pairs
const char      *cmd,                   // Command set value
                *pcl;                   // PCL command set pointer


// Parse the IEEE-1284 device ID to see if this is a printer we support...
num_did = papplDeviceParseID(device_id, &did);
```

The two keys we care about are the "COMMAND SET" (also abbreviated as "CMD") for
the list of document formats the printer supports and "MODEL"/"MDL" for the
model name.  We are looking for the "PCL" format and one of the common model
names for HP printers:

```c
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


The File Printing Callback
--------------------------

The file printing callback is used when printing a "raw" (printer-ready) file
from a client:

```c
typedef bool (*pappl_pr_printfile_cb_t)(pappl_job_t *job,
    pappl_pr_options_t *options, pappl_device_t *device);
```

This callback will sometimes send some printer initialization commands followed
by the job file and then any cleanup commands.  It may also be able to count the
number of pages (impressions) in the file, although that is not a requirement.
For the HP Printer Application our `pcl_print` function just copies the file
from the job to the device and assumes that the file contains only one page:

```c
int     fd;                     // Job file
ssize_t bytes;                  // Bytes read/written
char    buffer[65536];          // Read/write buffer


papplJobSetImpressions(job, 1);

fd = open(papplJobGetFilename(job), O_RDONLY);

while ((bytes = read(fd, buffer, sizeof(buffer))) > 0)
{
  if (papplDeviceWrite(device, buffer, (size_t)bytes) < 0)
  {
    papplLogJob(job, PAPPL_LOGLEVEL_ERROR,
                "Unable to send %d bytes to printer.", (int)bytes);
    close(fd);
    return (false);
  }
}

close(fd);

papplJobSetImpressionsCompleted(job, 1);

return (true);
```


The Raster Printing Callbacks
-----------------------------

The PAPPL raster printing callbacks are used for printing PWG and Apple raster
documents, JPEG and PNG images, and other formats that end up as raster data:

```c
typedef bool (*pappl_pr_rstartjob_cb_t)(pappl_job_t *job,
    pappl_pr_options_t *options, pappl_device_t *device);

typedef bool (*pappl_pr_rstartpage_cb_t)(pappl_job_t *job,
    pappl_pr_options_t *options, pappl_device_t *device, unsigned page);

typedef bool (*pappl_pr_rwriteline_cb_t)(pappl_job_t *job,
    pappl_pr_options_t *options, pappl_device_t *device, unsigned y,
    const unsigned char *line);

typedef bool (*pappl_pr_rendpage_cb_t)(pappl_job_t *job,
    pappl_pr_options_t *options, pappl_device_t *device, unsigned page);

typedef bool (*pappl_pr_rendjob_cb_t)(pappl_job_t *job,
    pappl_pr_options_t *options, pappl_device_t *device);
```

Each of the raster printing callbacks is expected to send data to the printer
using the provided "device" pointer.  The "job" argument provides the current
job object and the "options" pointer provides the current
[print job options](#pappl_pr_options_s).  The "page" argument specifies the
current page number staring at `0`.

The `pappl_pr_rstartjob_cb_t` function is called at the beginning of a job to
allow the driver to initialize the printer for the current job.

The `pappl_pr_rstartpage_cb_t` function is called at the beginning of each page
to allow the driver to do any per-page initialization and/or memory allocations
and send any printer commands that are necessary to start a new page.

The `pappl_pr_rwriteline_cb_t` function is called for each raster line on the
page and is typically responsible for dithering and compressing the raster data
for the printer.

The `pappl_pr_rendpage_cb_t` function is called at the end of each page where
the driver will typically eject the current page.

The `pappl_pr_rendjob_cb_t` function is called at the end of a job to allow the
driver to send any cleanup commands to the printer.


The Identification Callback
---------------------------

The PAPPL identification callback is used to audibly or visibly identify the
printer being used:

```c
typedef void (*pappl_pr_identify_cb_t)(pappl_printer_t *printer,
    pappl_identify_actions_t actions, const char *message);
```

The most common identification method is `PAPPL_IDENTIFY_ACTIONS_SOUND` which
should have the printer make a sound.  The `PAPPL_IDENTIFY_ACTIONS_DISPLAY`
and `PAPPL_IDENTIFY_ACTIONS_SPEAK` methods use the "message" argument to display
or speak a message.  Finally, the `PAPPL_IDENTIFY_ACTIONS_FLASH` method flashes
a light on the printer.

The HP Printer Application does not have an identification callback since most
PCL printers lack a buzzer or light that can be remotely activated.

> *Note:* IPP Everywhere™ requires all printers to support identification.
> A warning message is logged by PAPPL whenever a driver does not set the
> `identify_cb` member of the printer driver data structure.


The Status Callback
-------------------

The PAPPL status callback is used to update the printer state, supply levels,
and/or ready media for the printer:

```c
typedef bool (*pappl_pr_status_cb_t)(pappl_printer_t *printer);
```

The callback can open a connection to the printer using the
[`papplPrinterOpenDevice`](@@) function.


The Self-Test Page Callback
---------------------------

The PAPPL self-test page callback is used to generate a self-test page for the
printer:

```c
typedef const char *(*pappl_printer_testpage_cb_t)(pappl_printer_t *printer,
    char *buffer, size_t bufsize);
```

When the callback returns a filename (copied to the specified buffer), that
file will be queued as a job for the printer.  The callback can also try opening
the device using the [`papplPrinterOpenDevice`](@@) function to send a printer
self-test command instead - in this case the callback must return `NULL` to
indicate there is no file to be printed.
