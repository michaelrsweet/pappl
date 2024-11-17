//
// System accessor functions for the Printer Application Framework
//
// Copyright © 2020-2024 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#include "system-private.h"
#ifdef HAVE_LIBJPEG
#  include <jpeglib.h>
#  ifndef JPEG_LIB_VERSION_MAJOR	// Added in JPEGLIB 9
#    define JPEG_LIB_VERSION_MAJOR 8
#    define JPEG_LIB_VERSION_MINOR 0
#  endif // !JPEG_LIB_VERSION_MAJOR
#endif // HAVE_LIBJPEG
#ifdef HAVE_LIBPNG
#  include <png.h>
#endif // HAVE_LIBPNG
#ifndef _WIN32
#  include <sys/resource.h>
#endif // !_WIN32


//
// Local functions...
//

static bool		add_listeners(pappl_system_t *system, const char *name, int port, int family);
static int		compare_filters(_pappl_mime_filter_t *a, _pappl_mime_filter_t *b);
static int		compare_timers(_pappl_timer_t *a, _pappl_timer_t *b);
static _pappl_mime_filter_t *copy_filter(_pappl_mime_filter_t *f);


//
// 'papplSystemAddListeners()' - Add network or domain socket listeners.
//
// This function adds socket listeners.  The "name" parameter specifies the
// listener address.  Names starting with a slash (/) specify a UNIX domain
// socket path, otherwise the name is treated as a fully-qualified domain name
// or numeric IPv4 or IPv6 address.  If name is `NULL`, the "any" addresses are
// used ("0.0.0.0" and "[::]").
//
// Listeners cannot be added after @link papplSystemRun@ is called.
//

bool					// O - `true` on success, `false` on failure
papplSystemAddListeners(
    pappl_system_t *system,		// I - System
    const char     *name)		// I - Hostname, domain socket filename, or `NULL`
{
  bool	ret;				// Return value


  if (!system)
  {
    return (false);
  }
  else if (system->is_running)
  {
    papplLog(system, PAPPL_LOGLEVEL_FATAL, "Tried to add listeners while system is running.");
    return (false);
  }

#if !_WIN32
  if (name && *name == '/')
  {
    // Add a domain socket listener...
    ret = add_listeners(system, name, 0, AF_LOCAL);
    if (ret && !system->domain_path)
      system->domain_path = strdup(name);
  }
  else
#endif // !_WIN32
  if (name && isdigit(*name & 255))
  {
    // Add IPv4 listener...
    if (system->port)
    {
      ret = add_listeners(system, name, system->port, AF_INET);
    }
    else
    {
      int	port;			// Current port

#if _WIN32
      port = 7999;
#else
      port = 7999 + (getuid() % 1000);
#endif // _WIN32

      do
      {
        port ++;
        ret = add_listeners(system, name, port, AF_INET);
      }
      while (!ret && port < 10000);

      if (ret)
        system->port = port;
    }
  }
  else if (name && *name == '[')
  {
    // Add IPv6 listener...
    if (system->port)
    {
      ret = add_listeners(system, name, system->port, AF_INET6);
    }
    else
    {
      int	port;			// Current port

#if _WIN32
      port = 7999;
#else
      port = 7999 + (getuid() % 1000);
#endif // _WIN32

      do
      {
        port ++;
        ret = add_listeners(system, name, port, AF_INET6);
      }
      while (!ret && port < 10000);

      if (ret)
        system->port = port;
    }
  }
  else
  {
    // Add named listeners on both IPv4 and IPv6...
    if (name && strcasecmp(name, "*"))
    {
      // Listening on a specific hostname...
      _papplRWLockWrite(system);

      free(system->hostname);
      system->hostname      = strdup(name);
      system->is_listenhost = true;

      _papplRWUnlock(system);
    }

    if (system->port)
    {
      ret = add_listeners(system, name, system->port, AF_INET) ||
            add_listeners(system, name, system->port, AF_INET6);
    }
    else
    {
      int	port;		// Current port

#if _WIN32
      port = 7999;
#else
      port = 7999 + (getuid() % 1000);
#endif // _WIN32

      do
      {
        port ++;
        ret = add_listeners(system, name, port, AF_INET);
      }
      while (!ret && port < 10000);

      if (ret)
      {
        system->port = port;
        add_listeners(system, name, port, AF_INET6);
      }
    }
  }

  return (ret);
}


//
// 'papplSystemAddMIMEFilter()' - Add a file filter to the system.
//
// This function adds a file filter to the system to be used for processing
// different kinds of document data in print jobs.  The "srctype" and "dsttype"
// arguments specify the source and destination MIME media types as constant
// strings.  A destination MIME media type of "image/pwg-raster" specifies a
// filter that uses the driver's raster interface.  Other destination types
// imply direct submission to the output device using the `papplDeviceXxx`
// functions.
//
// > Note: This function may not be called while the system is running.
//

void
papplSystemAddMIMEFilter(
    pappl_system_t         *system,	// I - System
    const char             *srctype,	// I - Source MIME media type (constant) string
    const char             *dsttype,	// I - Destination MIME media type (constant) string
    pappl_mime_filter_cb_t cb,		// I - Filter callback function
    void                   *data)	// I - Filter callback data
{
  _pappl_mime_filter_t	key;		// Search key


  if (!system || system->is_running || !srctype || !dsttype || !cb)
    return;

  if (!system->filters)
    system->filters = cupsArrayNew((cups_array_cb_t)compare_filters, NULL, NULL, 0, (cups_acopy_cb_t)copy_filter, (cups_afree_cb_t)free);

  key.src    = srctype;
  key.dst    = dsttype;
  key.cb     = cb;
  key.cbdata = data;

  if (!cupsArrayFind(system->filters, &key))
  {
    papplLog(system, PAPPL_LOGLEVEL_DEBUG, "Adding '%s' to '%s' filter.", srctype, dsttype);
    cupsArrayAdd(system->filters, &key);
  }
}


//
// 'papplSystemAddTimerCallback()' - Add a timer callback to a system.
//
// This function schedules a function that will be called on the main run loop
// thread at the specified time and optionally every "interval" seconds
// thereafter.  The timing accuracy is typically within a few milliseconds but
// is not guaranteed.  Since the callback is run on the main run loop thread,
// functions should create a new thread for any long-running operations.
//
// The callback function receives the "system" and "cb_data" pointers and
// returns `true` to repeat the timer or `false` to remove it:
//
// ```
// bool my_timer_cb(pappl_system_t *system, void *cb_data)
// {
//   ... do periodic task ...
//   return (true); // repeat the timer
// }
// ```
//

bool					// O - `true` on success, `false` on error
papplSystemAddTimerCallback(
    pappl_system_t   *system,		// I - System
    time_t           start,		// I - Start time in seconds or `0` for the current time
    int              interval,		// I - Repeat interval in seconds or `0` for no repeat
    pappl_timer_cb_t cb,		// I - Callback function
    void             *cb_data)		// I - Callback data
{
  _pappl_timer_t	*newt;		// New timer


  // Range check input...
  if (!system || !cb || interval < 0)
    return (false);

  // Allocate the new timer...
  if ((newt = calloc(1, sizeof(_pappl_timer_t))) == NULL)
    return (false);

  _papplRWLockWrite(system);

  if (!system->timers)
    system->timers = cupsArrayNew((cups_array_cb_t)compare_timers, NULL, NULL, 0, NULL, NULL);

  newt->cb       = cb;
  newt->cb_data  = cb_data;
  newt->next     = start ? start : time(NULL) + interval;
  newt->interval = interval;

  cupsArrayAdd(system->timers, newt);

  _papplRWUnlock(system);

  return (true);
}


//
// '_papplSystemExportVersions()' - Export the firmware versions to IPP attributes...
//

void
_papplSystemExportVersions(
    pappl_system_t *system,		// I - System
    ipp_t          *ipp,		// I - IPP message
    ipp_tag_t      group_tag,		// I - Group (`IPP_TAG_PRINTER` or `IPP_TAG_SYSTEM`)
    cups_array_t   *ra)			// I - Requested attributes or `NULL` for all
{
  cups_len_t	i;			// Looping var
  ipp_attribute_t *attr;		// Attribute
  char		name[128];		// Attribute name
  const char	*name_prefix = (group_tag == IPP_TAG_PRINTER) ? "printer" : "system";
  const char	*values[20];		// String values
  char		cups_sversion[32];	// String version of libcups
#ifdef HAVE_LIBJPEG
  char		jpeg_sversion[32];	// String version of libjpeg
#endif // HAVE_LIBJPEG
  unsigned short version[4];		// Version of software components


  // "xxx-firmware-name"
  snprintf(name, sizeof(name), "%s-firmware-name", name_prefix);
  if (!ra || cupsArrayFind(ra, name))
  {
    for (i = 0; i < system->num_versions; i ++)
      values[i] = system->versions[i].name;

    values[i ++] = "PAPPL";

    values[i ++] = "libcups";

#ifdef HAVE_LIBJPEG
    values[i ++] = "libjpeg";
#endif // HAVE_LIBJPEG

#ifdef HAVE_LIBPNG
    values[i ++] = "libpng";
#endif // HAVE_LIBPNG

    ippAddStrings(ipp, group_tag, IPP_TAG_NAME, name, i, NULL, values);
  }

  // "xxx-firmware-patches"
  snprintf(name, sizeof(name), "%s-firmware-patches", name_prefix);
  if (!ra || cupsArrayFind(ra, name))
  {
    for (i = 0; i < system->num_versions; i ++)
      values[i] = system->versions[i].patches;

    values[i ++] = "";			// No patches for PAPPL

    values[i ++] = "";			// No patches for CUPS

#ifdef HAVE_LIBJPEG
    values[i ++] = "";			// No patches for libjpeg
#endif // HAVE_LIBJPEG

#ifdef HAVE_LIBPNG
    values[i ++] = "";			// No patches for libpng
#endif // HAVE_LIBPNG

    ippAddStrings(ipp, group_tag, IPP_TAG_TEXT, name, i, NULL, values);
  }

  // "xxx-firmware-string-version"
  snprintf(name, sizeof(name), "%s-firmware-string-version", name_prefix);
  if (!ra || cupsArrayFind(ra, name))
  {
    for (i = 0; i < system->num_versions; i ++)
      values[i] = system->versions[i].sversion;

    values[i ++] = PAPPL_VERSION;

    snprintf(cups_sversion, sizeof(cups_sversion), "%d.%d.%d", CUPS_VERSION_MAJOR, CUPS_VERSION_MINOR, CUPS_VERSION_PATCH);
    values[i ++] = cups_sversion;

#ifdef HAVE_LIBJPEG
    snprintf(jpeg_sversion, sizeof(jpeg_sversion), "%d.%d", JPEG_LIB_VERSION_MAJOR, JPEG_LIB_VERSION_MINOR);
    values[i ++] = jpeg_sversion;
#endif // HAVE_LIBJPEG

#ifdef HAVE_LIBPNG
    values[i ++] = png_libpng_ver;
#endif // HAVE_LIBPNG

    ippAddStrings(ipp, group_tag, IPP_TAG_TEXT, name, i, NULL, values);
  }

  // "xxx-firmware-version"
  snprintf(name, sizeof(name), "%s-firmware-version", name_prefix);
  if (!ra || cupsArrayFind(ra, name))
  {
    for (i = 0, attr = NULL; i < system->num_versions; i ++)
    {
      if (attr)
	ippSetOctetString(ipp, &attr, ippGetCount(attr), system->versions[i].version, (int)sizeof(system->versions[i].version));
      else
	attr = ippAddOctetString(ipp, group_tag, name, system->versions[i].version, (int)sizeof(system->versions[i].version));
    }

    memset(version, 0, sizeof(version));
    sscanf(PAPPL_VERSION, "%hu.%hu.%hu", version + 0, version + 1, version + 2);
    if (attr)
      ippSetOctetString(ipp, &attr, ippGetCount(attr), version, (int)sizeof(version));
    else
      attr = ippAddOctetString(ipp, group_tag, name, version, (int)sizeof(version));

    version[0] = CUPS_VERSION_MAJOR;
    version[1] = CUPS_VERSION_MINOR;
    version[2] = CUPS_VERSION_PATCH;
    ippSetOctetString(ipp, &attr, ippGetCount(attr), version, (int)sizeof(version));

#ifdef HAVE_LIBJPEG
    version[0] = JPEG_LIB_VERSION_MAJOR;
    version[1] = JPEG_LIB_VERSION_MINOR;
    version[2] = 0;
    ippSetOctetString(ipp, &attr, ippGetCount(attr), version, (int)sizeof(version));
#endif // HAVE_LIBJPEG

#ifdef HAVE_LIBPNG
    memset(version, 0, sizeof(version));
    sscanf(png_libpng_ver, "%hu.%hu.%hu", version + 0, version + 1, version + 2);
    ippSetOctetString(ipp, &attr, ippGetCount(attr), version, (int)sizeof(version));
#endif // HAVE_LIBPNG
  }
}


//
// '_papplSystemFindMIMEFilter()' - Find a filter for the given source and destination formats.
//

_pappl_mime_filter_t *			// O - Filter data
_papplSystemFindMIMEFilter(
    pappl_system_t *system,		// I - System
    const char     *srctype,		// I - Source MIME media type string
    const char     *dsttype)		// I - Destination MIME media type string
{
  _pappl_mime_filter_t	key,		// Search key
			*match;		// Matching filter


  if (!system || !srctype || !dsttype)
    return (NULL);

  _papplRWLockRead(system);

  key.src = srctype;
  key.dst = dsttype;

  match = (_pappl_mime_filter_t *)cupsArrayFind(system->filters, &key);

  _papplRWUnlock(system);

  return (match);
}


//
// 'papplSystemGetAdminGroup()' - Get the current administrative group, if any.
//
// This function copies the current administrative group, if any, to the
// specified buffer.
//

char *					// O - Admin group or `NULL` if none
papplSystemGetAdminGroup(
    pappl_system_t *system,		// I - System
    char           *buffer,		// I - String buffer
    size_t         bufsize)		// I - Size of string buffer
{
  char	*ret = NULL;			// Return value


  if (system && buffer && bufsize > 0)
  {
    _papplRWLockRead(system);

    if (system->admin_group)
    {
      papplCopyString(buffer, system->admin_group, bufsize);
      ret = buffer;
    }
    else
      *buffer = '\0';

    _papplRWUnlock(system);
  }
  else if (buffer)
    *buffer = '\0';

  return (ret);
}


//
// 'papplSystemGetAuthService()' - Get the PAM authorization service, if any.
//
// This function returns the PAM authorization service being used by the system
// for authentication, if any.
//

const char *				// O - PAM authorization service or `NULL` if none
papplSystemGetAuthService(
    pappl_system_t *system) 	 	// I - System
{
  return (system ? system->auth_service : NULL);
}


//
// 'papplSystemGetContact()' - Get the "system-contact" value.
//
// This function copies the current system contact information to the specified
// buffer.
//

pappl_contact_t *			// O - Contact
papplSystemGetContact(
    pappl_system_t  *system,		// I - System
    pappl_contact_t *contact)		// O - Contact
{
  if (!system || !contact)
  {
    if (contact)
      memset(contact, 0, sizeof(pappl_contact_t));

    return (contact);
  }

  _papplRWLockRead(system);

  *contact = system->contact;

  _papplRWUnlock(system);

  return (contact);
}


//
// 'papplSystemGetDefaultPrinterID()' - Get the current "default-printer-id" value.
//
// This function returns the positive integer identifier for the current
// default printer or `0` if there is no default printer.
//

int					// O - "default-printer-id" value
papplSystemGetDefaultPrinterID(
    pappl_system_t *system)		// I - System
{
  int ret = 0;				// Return value


  if (system)
  {
    _papplRWLockRead(system);
    ret = system->default_printer_id;
    _papplRWUnlock(system);
  }

  return (ret);
}

//
// 'papplSystemGetDefaultScannerID()' - Get the current "default-scanner-id" value.
//
// This function returns the positive integer identifier for the current
// default scanner or `0` if there is no default scanner.
//

int					// O - "default-scanner-id" value
papplSystemGetDefaultScannerID(
    pappl_system_t *system)		// I - System
{
  int ret = 0;				// Return value

  if (system)
  {
    _papplRWLockRead(system);
    ret = system->default_scanner_id;
    _papplRWUnlock(system);
  }
  return (ret);
}


//
// 'papplSystemGetDefaultPrintGroup()' - Get the default print group, if any.
//
// This function copies the current default print group, if any, to the
// specified buffer.
//

char *					// O - Default print group or `NULL` if none
papplSystemGetDefaultPrintGroup(
    pappl_system_t *system,		// I - System
    char           *buffer,		// I - String buffer
    size_t         bufsize)		// I - Size of string buffer
{
  char	*ret = NULL;			// Return value


  if (system && buffer && bufsize > 0)
  {
    _papplRWLockRead(system);

    if (system->default_print_group)
    {
      papplCopyString(buffer, system->default_print_group, bufsize);
      ret = buffer;
    }
    else
      *buffer = '\0';

    _papplRWUnlock(system);
  }
  else if (buffer)
    *buffer = '\0';

  return (ret);
}


//
// 'papplSystemGetDNSSDName()' - Get the current DNS-SD service name.
//
// This function copies the current DNS-SD service name of the system, if any,
// to the specified buffer.
//

char *					// O - Current DNS-SD service name or `NULL` for none
papplSystemGetDNSSDName(
    pappl_system_t *system,		// I - System
    char           *buffer,		// I - String buffer
    size_t         bufsize)		// I - Size of string buffer
{
  char	*ret = NULL;			// Return value


  if (system && buffer && bufsize > 0)
  {
    _papplRWLockRead(system);

    if (system->dns_sd_name)
    {
      papplCopyString(buffer, system->dns_sd_name, bufsize);
      ret = buffer;
    }
    else
      *buffer = '\0';

    _papplRWUnlock(system);
  }
  else if (buffer)
    *buffer = '\0';

  return (ret);
}


//
// 'papplSystemGetFooterHTML()' - Get the footer HTML for the web interface, if any.
//
// This function returns the HTML for the web page footer, if any.  The footer
// HTML can be set using the @link papplSystemSetFooterHTML@ function.
//

const char *				// O - Footer HTML or `NULL` if none
papplSystemGetFooterHTML(
    pappl_system_t *system)		// I - System
{
  return (system ? system->footer_html : NULL);
}


//
// 'papplSystemGetGeoLocation()' - Get the system geo-location string, if any.
//
// This function copies the current system geographic location as a "geo:" URI
// to the specified buffer.
//

char *					// O - "geo:" URI or `NULL` for none
papplSystemGetGeoLocation(
    pappl_system_t *system,		// I - System
    char           *buffer,		// I - String buffer
    size_t         bufsize)		// I - Size of string buffer
{
  char	*ret = NULL;			// Return value


  if (system && buffer && bufsize > 0)
  {
    _papplRWLockRead(system);

    if (system->geo_location)
    {
      papplCopyString(buffer, system->geo_location, bufsize);
      ret = buffer;
    }
    else
      *buffer = '\0';

    _papplRWUnlock(system);
  }
  else if (buffer)
    *buffer = '\0';

  return (ret);
}


//
// 'papplSystemGetHostname()' - Get the system hostname.
//
// This function is deprecated.  Use the @link papplSystemGetHostName@ function
// instead.
//
// @deprecated@ @exclude all@
//

char *					// O - Hostname
papplSystemGetHostname(
    pappl_system_t *system,		// I - System
    char           *buffer,		// I - String buffer
    size_t         bufsize)		// I - Size of string buffer
{
  return (papplSystemGetHostName(system, buffer, bufsize));
}


//
// 'papplSystemGetHostName()' - Get the system hostname.
//
// This function copies the current system hostname to the specified buffer.
//

char *					// O - Hostname
papplSystemGetHostName(
    pappl_system_t *system,		// I - System
    char           *buffer,		// I - String buffer
    size_t         bufsize)		// I - Size of string buffer
{
  char	*ret = NULL;			// Return value


  if (system && buffer && bufsize > 0)
  {
    _papplRWLockRead(system);

    if (system->hostname)
    {
      papplCopyString(buffer, system->hostname, bufsize);
      ret = buffer;
    }
    else
      *buffer = '\0';

    _papplRWUnlock(system);
  }
  else if (buffer)
    *buffer = '\0';

  return (ret);
}


//
// 'papplSystemGetHostPort()' - Get the port number for network connections to
//                              the system.
//
// This function returns the port number that is used for network connections
// to the system.
//

int					// O - Port number
papplSystemGetHostPort(
    pappl_system_t *system)		// I - System
{
  return (system ? system->port : 0);
}


//
// 'papplSystemGetLocation()' - Get the system location string, if any.
//
// This function copies the current human-readable location, if any, to the
// specified buffer.
//

char *					// O - Location string or `NULL` for none
papplSystemGetLocation(
    pappl_system_t *system,		// I - System
    char           *buffer,		// I - String buffer
    size_t         bufsize)		// I - Size of string buffer
{
  char	*ret = NULL;			// Return value


  if (system && buffer && bufsize > 0)
  {
    _papplRWLockRead(system);

    if (system->location)
    {
      papplCopyString(buffer, system->location, bufsize);
      ret = buffer;
    }
    else
      *buffer = '\0';

    _papplRWUnlock(system);
  }
  else if (buffer)
    *buffer = '\0';

  return (ret);
}


//
// 'papplSystemGetLogLevel()' - Get the system log level.
//
// This function returns the current system log level as an enumeration.
//

pappl_loglevel_t			// O - Log level
papplSystemGetLogLevel(
    pappl_system_t *system)		// I - System
{
  pappl_loglevel_t ret = PAPPL_LOGLEVEL_UNSPEC;
					// Return value


  if (system)
  {
    pthread_mutex_lock(&system->log_mutex);
    ret = system->log_level;
    pthread_mutex_unlock(&system->log_mutex);
  }

  return (ret);
}


//
// 'papplSystemGetMaxClients()' - Get the maximum number of clients.
//
// This function gets the maximum number of simultaneous clients that are
// allowed by the system.
//

int					// O - Maximum number of clients
papplSystemGetMaxClients(
    pappl_system_t *system)		// I - System
{
  int ret = 0;				// Return value


  if (system)
  {
    _papplRWLockRead(system);
    ret = system->max_clients;
    _papplRWUnlock(system);
  }

  return (ret);
}


//
// 'papplSystemGetMaxImageSize()' - Get the maximum supported size for images.
//
// This function retrieves the image size limits in bytes (uncompressed),
// columns, and lines.
//

size_t					// O - Maximum image size (uncompressed)
papplSystemGetMaxImageSize(
    pappl_system_t *system,		// I - System
    int            *max_width,		// O - Maximum width in columns
    int            *max_height)		// O - Maximum height in lines
{
  size_t	max_size;		// Maximum image file size


  // Range check input...
  if (!system)
  {
    if (max_width)
      *max_width = 0;
    if (max_height)
      *max_height = 0;

    return (0);
  }

  // Grab a snapshot of the limits...
  _papplRWLockRead(system);

  max_size = system->max_image_size;

  if (max_width)
    *max_width = system->max_image_width;

  if (max_height)
    *max_height = system->max_image_height;

  _papplRWUnlock(system);

  return (max_size);
}


//
// 'papplSystemGetMaxLogSize()' - Get the maximum log file size.
//
// This function gets the maximum log file size, which is only used when logging
// directly to a file.  When the limit is reached, the current log file is
// renamed to "filename.O" and a new log file is created.  Set the maximum size
// to `0` to disable log file rotation.
//
// The default maximum log file size is 1MiB or `1048576` bytes.
//

size_t					// O - Maximum log file size or `0` for none
papplSystemGetMaxLogSize(
    pappl_system_t *system)		// I - System
{
  size_t ret = 0;			// Return value


  if (system)
  {
    pthread_mutex_lock(&system->log_mutex);
    ret = system->log_max_size;
    pthread_mutex_unlock(&system->log_mutex);
  }

  return (ret);
}


//
// 'papplSystemGetMaxSubscriptions()' - Get the maximum number of event subscriptions.
//
// This function gets the maximum number of event subscriptions that are
// allowed.  A maximum of `0` means there is no limit.
//
// The default maximum number of event subscriptions is 100.
//

size_t					// O - Maximum number of subscriptions or `0`
papplSystemGetMaxSubscriptions(
    pappl_system_t *system)		// I - System
{
  size_t ret = 0;			// Return value


  if (system)
  {
    _papplRWLockRead(system);
    ret = system->max_subscriptions;
    _papplRWUnlock(system);
  }

  return (ret);
}


//
// 'papplSystemGetName()' - Get the system name.
//
// This function copies the current system name to the specified buffer.
//

char *					// O - Name string
papplSystemGetName(
    pappl_system_t *system,		// I - System
    char           *buffer,		// I - String buffer
    size_t         bufsize)		// I - Size of string buffer
{
  char	*ret = NULL;			// Return value


  if (system && buffer && bufsize > 0)
  {
    _papplRWLockRead(system);

    if (system->name)
    {
      papplCopyString(buffer, system->name, bufsize);
      ret = buffer;
    }
    else
      *buffer = '\0';

    _papplRWUnlock(system);
  }
  else if (buffer)
    *buffer = '\0';

  return (ret);
}


//
// 'papplSystemGetNextPrinterID()' - Get the next "printer-id" value.
//
// This function returns the positive integer identifier that will be used for
// the next printer that is created.
//

int					// O - Next "printer-id" value
papplSystemGetNextPrinterID(
    pappl_system_t *system)		// I - System
{
  int ret = 0;				// Return value


  if (system)
  {
    _papplRWLockRead(system);
    ret = system->next_printer_id;
    _papplRWUnlock(system);
  }

  return (ret);
}


//
// 'papplSystemGetOptions()' - Get the system options.
//
// This function returns the system options as a bitfield.
//

pappl_soptions_t			// O - System options
papplSystemGetOptions(
    pappl_system_t *system)		// I - System
{
  return (system ? system->options : PAPPL_SOPTIONS_NONE);
}


//
// 'papplSystemGetOrganization()' - Get the system organization string, if any.
//
// This function copies the current organization name, if any, to the
// specified buffer.
//

char *					// O - Organization string or `NULL` for none
papplSystemGetOrganization(
    pappl_system_t *system,		// I - System
    char           *buffer,		// I - String buffer
    size_t         bufsize)		// I - Size of string buffer
{
  char	*ret = NULL;			// Return value


  if (system && buffer && bufsize > 0)
  {
    _papplRWLockRead(system);

    if (system->organization)
    {
      papplCopyString(buffer, system->organization, bufsize);
      ret = buffer;
    }
    else
      *buffer = '\0';

    _papplRWUnlock(system);
  }
  else if (buffer)
    *buffer = '\0';

  return (ret);
}


//
// 'papplSystemGetOrganizationalUnit()' - Get the system organizational unit string, if any.
//
// This function copies the current organizational unit name, if any, to the
// specified buffer.
//

char *					// O - Organizational unit string or `NULL` for none
papplSystemGetOrganizationalUnit(
    pappl_system_t *system,		// I - System
    char           *buffer,		// I - String buffer
    size_t         bufsize)		// I - Size of string buffer
{
  char	*ret = NULL;			// Return value


  if (system && buffer && bufsize > 0)
  {
    _papplRWLockRead(system);

    if (system->org_unit)
    {
      papplCopyString(buffer, system->org_unit, bufsize);
      ret = buffer;
    }
    else
      *buffer = '\0';

    _papplRWUnlock(system);
  }
  else if (buffer)
    *buffer = '\0';

  return (ret);
}


//
// 'papplSystemGetPassword()' - Get the current web site access password.
//
// This function copies the current web site password hash, if any, to the
// specified buffer.
//
// Note: The access password is only used when the PAM authentication service
// is not set.
//

char *					// O - Password hash
papplSystemGetPassword(
    pappl_system_t *system,		// I - System
    char           *buffer,		// I - String buffer
    size_t         bufsize)		// I - Size of string buffer
{
  if (system && buffer && bufsize > 0)
  {
    _papplRWLockRead(system);

    papplCopyString(buffer, system->password_hash, bufsize);

    _papplRWUnlock(system);
  }
  else if (buffer)
    *buffer = '\0';

  return (buffer);
}


//
// 'papplSystemGetPort()' - Get the port number for network connections to the
//                          system.
//
// This function is deprecated.  Use the @link papplSystemGetHostName@ function
// instead.
//
// @deprecated@ @exclude all@
//

int					// O - Port number
papplSystemGetPort(
    pappl_system_t *system)		// I - System
{
  return (system ? system->port : 0);
}


//
// 'papplSystemGetServerHeader()' - Get the Server: header for HTTP responses.
//
// This function returns the value of the HTTP "Server:" header that is used
// by the system.
//

const char *				// O - Server: header string or `NULL` for none
papplSystemGetServerHeader(
    pappl_system_t *system)		// I - System
{
  return (system ? system->server_header : NULL);
}


//
// 'papplSystemGetSessionKey()' - Get the current session key.
//
// This function copies the current session key to the specified buffer.  The
// session key is used for web interface forms to provide CSRF protection and is
// refreshed periodically.
//

char *					// O - Session key
papplSystemGetSessionKey(
    pappl_system_t *system,		// I - System
    char           *buffer,		// I - String buffer
    size_t         bufsize)		// I - Size of string buffer
{
  time_t	curtime = time(NULL);	// Current time


  if (system && buffer && bufsize > 0)
  {
    pthread_mutex_lock(&system->session_mutex);
    if ((curtime - system->session_time) > 86400)
    {
      // Lock for updating the session key with random data...
      snprintf(system->session_key, sizeof(system->session_key), "%08x%08x%08x%08x%08x%08x%08x%08x", papplGetRand(), papplGetRand(), papplGetRand(), papplGetRand(), papplGetRand(), papplGetRand(), papplGetRand(), papplGetRand());
      system->session_time = curtime;
    }

    papplCopyString(buffer, system->session_key, bufsize);

    pthread_mutex_unlock(&system->session_mutex);
  }
  else if (buffer)
  {
    *buffer = '\0';
  }

  return (buffer);
}


//
// 'papplSystemGetTLSOnly()' - Get the TLS-only state of the system.
//
// This function returns whether the system will only accept encrypted
// connections.
//

bool					// O - `true` if the system is only accepting TLS encrypted connections, `false` otherwise
papplSystemGetTLSOnly(
    pappl_system_t *system)		// I - System
{
  return (system ? system->tls_only : false);
}


//
// 'papplSystemGetUUID()' - Get the "system-uuid" value.
//
// This function returns the system's UUID value.
//

const char *				// O - "system-uuid" value
papplSystemGetUUID(
    pappl_system_t *system)		// I - System
{
  const char *ret = NULL;		// Return value


  if (system)
  {
    _papplRWLockRead(system);
    ret = system->uuid;
    _papplRWUnlock(system);
  }

  return (ret);
}


//
// 'papplSystemGetVersions()' - Get the firmware names and versions.
//
// This function copies the system firmware information to the specified buffer.
// The return value is always the number of firmware versions that have been
// set using the @link papplSystemSetVersions@ function, regardless of the
// value of the "max_versions" argument.
//

int					// O - Number of firmware versions
papplSystemGetVersions(
    pappl_system_t  *system,		// I - System
    int             max_versions,	// I - Maximum number of versions to return
    pappl_version_t *versions)		// O - Versions array or `NULL` for don't care
{
  if (versions && max_versions > 0)
    memset(versions, 0, (size_t)max_versions * sizeof(pappl_version_t));

  if (system && versions && system->num_versions > 0)
  {
    _papplRWLockRead(system);

    if (max_versions > (int)system->num_versions)
      memcpy(versions, system->versions, (size_t)system->num_versions * sizeof(pappl_version_t));
    else
      memcpy(versions, system->versions, (size_t)max_versions * sizeof(pappl_version_t));

    _papplRWUnlock(system);
  }

  return (system ? (int)system->num_versions : 0);
}


//
// 'papplSystemHashPassword()' - Generate a password hash using salt and password strings.
//
// This function generates a password hash using the "salt" and "password"
// strings.  The "salt" string should be `NULL` to generate a new password hash
// or the value of an existing password hash to verify that a given plaintext
// "password" string matches the password hash.
//
// > Note: Hashed access passwords are only used when the PAM authentication
// > service is not set.
//

char *					// O - Hashed password
papplSystemHashPassword(
    pappl_system_t *system,		// I - System
    const char     *salt,		// I - Existing password hash or `NULL` to generate a new hash
    const char     *password,		// I - Plain-text password string
    char           *buffer,		// I - String buffer
    size_t         bufsize)		// I - Size of string buffer
{
  if (system && password && buffer && bufsize > 0)
  {
    char		nonce[100],	// Nonce string
			*ptr,		// Pointer into string
			temp[256];	// Temporary hash
    unsigned char	hash[32];	// SHA2-256 hash

    if (salt && strchr(salt, '~'))
    {
      // Copy existing nonce from the salt string...
      papplCopyString(nonce, salt, sizeof(nonce));
      if ((ptr = strchr(nonce, '~')) != NULL)
        *ptr = '\0';
    }
    else
    {
      // Generate a new random nonce...
      snprintf(nonce, sizeof(nonce), "%08x%08x", papplGetRand(), papplGetRand());
    }

    snprintf(temp, sizeof(temp), "%s:%s", nonce, password);
    cupsHashData("sha2-256", temp, strlen(temp), hash, sizeof(hash));
    cupsHashString(hash, sizeof(hash), temp, sizeof(temp));

    snprintf(buffer, bufsize, "%s~%s", nonce, temp);
  }
  else if (buffer)
    *buffer = '\0';

  return (buffer);
}


//
// 'papplSystemIsRunning()' - Return whether the system is running.
//
// This function returns whether the system is running.
//

bool					// O - `true` if the system is running, `false` otherwise
papplSystemIsRunning(
    pappl_system_t *system)		// I - System
{
  bool	is_running;			// Return value

  // Range check input
  if (!system)
    return (false);

  _papplRWLockRead(system);
  is_running = system->is_running;
  _papplRWUnlock(system);

  return (is_running);
}


//
// 'papplSystemIsShutdown()' - Return whether the system has been shutdown.
//
// This function returns whether the system is shutdown or scheduled to
// shutdown.
//

bool					// O - `true` if the system is shutdown, `false` otherwise
papplSystemIsShutdown(
    pappl_system_t *system)		// I - System
{
  bool ret = false;			// Return value


  if (system)
  {
    _papplRWLockRead(system);
    ret = !system->is_running || system->shutdown_time != 0;
    _papplRWUnlock(system);
  }

  return (ret);
}


//
// 'papplSystemIteratePrinters()' - Iterate all of the printers.
//
// This function iterates each of the printers managed by the system.  The
// "cb" function is called once per printer with the "system" and "data" values.
//

void
papplSystemIteratePrinters(
    pappl_system_t     *system,		// I - System
    pappl_printer_cb_t cb,		// I - Callback function
    void               *data)		// I - Callback data
{
  cups_len_t		i,		// Looping var
			count;		// Number of printers


  if (!system || !cb)
    return;

  // Loop through the printers.
  //
  // Note: Cannot use cupsArrayGetFirst/Last since other threads might be
  // enumerating the printers array.

  _papplRWLockRead(system);
  for (i = 0, count = cupsArrayGetCount(system->printers); i < count; i ++)
    (cb)((pappl_printer_t *)cupsArrayGetElement(system->printers, i), data);
  _papplRWUnlock(system);
}


//
// 'papplSystemMatchDriver()' - Match a driver to an IEEE-1284 device ID.
//

const char *
papplSystemMatchDriver(
    pappl_system_t *system,		// I - System
    const char     *device_id)		// I - IEEE-1284 device ID string
{
  cups_len_t	i;			// Looping var
  pappl_pr_driver_t *driver;		// Current driver
  const char	*drvstart,		// Start of key/value pair
		*drvend,		// End of key/value pair
		*didptr,		// Pointer into device ID
		*didend;		// End of device ID
  size_t	drvlen,			// Length of key/value pair
		didlen;			// Length of device ID


  if (!system)
    return (NULL);

  didlen = strlen(device_id);

  for (i = system->num_drivers, driver = system->drivers; i > 0; i --, driver ++)
  {
    if (!driver->device_id)
      continue;

    // Parse each of the driver's device ID pairs and compare against the
    // supplied device ID...
    drvstart = driver->device_id;
    while (*drvstart)
    {
      // Skip leading semicolons and whitespace (not valid, but sometimes
      // present...)
      while (*drvstart == ';' || isspace(*drvstart & 255))
        drvstart ++;

      if (!*drvstart)
        break;

      // Find the end of the current key:value pair...
      drvend = drvstart + 1;
      while (*drvend && *drvend != ';')
        drvend ++;

      if (*drvend == ';')
        drvend ++;

      drvlen = (size_t)(drvend - drvstart);

      // See if this string exists in the target device ID...
      didptr = device_id;
      didend = didptr + didlen - drvlen;
      while (didptr && didptr < didend)
      {
        if (!strncmp(didptr, drvstart, drvlen))
          break;

        if ((didptr = strchr(didptr, ';')) != NULL)
          didptr ++;
      }

      if (!didptr || didptr >= didend)
        break;

      drvstart = drvend;
    }

    if (!*drvstart)
      break;
  }

  if (i > 0)
    return (driver->name);
  else
    return (NULL);
}


//
// '_papplSystemNeedClean()' - Mark the system needing cleaning.
//

void
_papplSystemNeedClean(
    pappl_system_t *system)		// I - System
{
  _papplRWLockWrite(system);
  if (!system->clean_time)
    system->clean_time = time(NULL) + 60;
  _papplRWUnlock(system);
}


//
// 'papplSystemRemoveTimerCallback()' - Remove a timer callback.
//
// This function removes all matching timer callbacks from the specified system.
// Both the callback function and data must match to remove a timer.
//

void
papplSystemRemoveTimerCallback(
    pappl_system_t   *system,		// I - System
    pappl_timer_cb_t cb,		// I - Callback function
    void             *cb_data)		// I - Callback data
{
  _pappl_timer_t	*t;		// Current timer


  // Range check input...
  if (!system || !cb)
    return;

  // Loop through the timers and remove any matches...
  _papplRWLockWrite(system);

  for (t = (_pappl_timer_t *)cupsArrayGetFirst(system->timers); t; t = (_pappl_timer_t *)cupsArrayGetNext(system->timers))
  {
    if (t->cb == cb && t->cb_data == cb_data)
    {
      cupsArrayRemove(system->timers, t);
      free(t);
    }
  }

  _papplRWUnlock(system);
}


//
// 'papplSystemSetAdminGroup()' - Set the administrative group.
//
// This function sets the group name used for administrative requests such as
// adding or deleting a printer.
//
// > Note: The administrative group is only used when the PAM authorization
// > service is also set when the system is created.
//

void
papplSystemSetAdminGroup(
    pappl_system_t *system,		// I - System
    const char     *value)		// I - Admin group
{
  if (system)
  {
    _papplRWLockWrite(system);

    free(system->admin_group);
    system->admin_group = value ? strdup(value) : NULL;

#if !_WIN32 // TODO: Implement Windows admin group support
    if (system->admin_group && strcmp(system->admin_group, "none"))
    {
      char		buffer[8192];	// Buffer for strings
      struct group	grpbuf,		// Group buffer
			*grp = NULL;	// Admin group

      if (getgrnam_r(system->admin_group, &grpbuf, buffer, sizeof(buffer), &grp) || !grp)
	papplLog(system, PAPPL_LOGLEVEL_ERROR, "Unable to find admin group '%s'.", system->admin_group);
      else
	system->admin_gid = grp->gr_gid;
    }
    else
#endif // !_WIN32
      system->admin_gid = (gid_t)-1;

    _papplSystemConfigChanged(system);

    _papplRWUnlock(system);
  }
}


//
// 'papplSystemSetAuthCallback()' - Set an authentication callback for the specified scheme.
//
// This function sets the authentication callback that is used for Client
// requests.  The authentication callback is used for every Client request
// containing the WWW-Authenticate header (`HTTP_FIELD_WWW_AUTHENTICATE`).
// The callback returns one of the following status codes:
//
// - `HTTP_STATUS_CONTINUE` if the authentication succeeded,
// - `HTTP_STATUS_UNAUTHORIZED` if the authentication failed, or
// - `HTTP_STATUS_FORBIDDEN` if the authentication succeeded but the user is
//   not part of the specified group.
//
// > Note: The authentication callback can only be set prior to calling
// > @link papplSystemRun@.
//

void
papplSystemSetAuthCallback(
    pappl_system_t  *system,		// I - System
    const char      *auth_scheme,	// I - Authentication scheme
    pappl_auth_cb_t auth_cb,		// I - Callback function
    void            *auth_cbdata)	// I - Callback data
{
  if (system && !system->is_running)
  {
    free(system->auth_scheme);
    system->auth_scheme = auth_scheme ? strdup(auth_scheme) : NULL;
    system->auth_cb     = auth_cb;
    system->auth_cbdata = auth_cbdata;
  }
}

//
// 'papplSystemSetContact()' - Set the "system-contact" value.
//
// This function sets the system contact value.
//

void
papplSystemSetContact(
    pappl_system_t  *system,		// I - System
    pappl_contact_t *contact)		// I - Contact
{
  if (!system || !contact)
    return;

  _papplRWLockWrite(system);

  system->contact = *contact;

  _papplSystemConfigChanged(system);

  _papplRWUnlock(system);
}


//
// 'papplSystemSetDefaultPrinterID()' - Set the "default-printer-id" value.
//
// This function sets the default printer using its unique positive integer
// identifier.
//

void
papplSystemSetDefaultPrinterID(
    pappl_system_t *system,		// I - System
    int            default_printer_id)	// I - "default-printer-id" value
{
  if (system)
  {
    _papplRWLockWrite(system);

    system->default_printer_id = default_printer_id;

    _papplSystemConfigChanged(system);

    _papplRWUnlock(system);
  }
}

//
// 'papplSystemSetDefaultScannerID()' - Set the "default-scanner-id" value.
//
// This function sets the default scanner using its unique positive integer
// identifier.
//

void
papplSystemSetDefaultScannerID(
    pappl_system_t *system,		// I - System
    int            default_scanner_id)	// I - "default-scanner-id" value
{
  if (system)
  {
    _papplRWLockWrite(system);

    system->default_scanner_id = default_scanner_id;

    _papplSystemConfigChanged(system);

    _papplRWUnlock(system);
  }
}


//
// 'papplSystemSetDefaultPrintGroup()' - Set the default print group.
//
// This function sets the default group name used for print requests.
//
// > Note: The default print group is only used when the PAM authorization
// > service is also set when the system is created.
//

void
papplSystemSetDefaultPrintGroup(
    pappl_system_t *system,		// I - System
    const char     *value)		// I - Default print group or `NULL` for none
{
  if (system)
  {
    _papplRWLockWrite(system);

    free(system->default_print_group);
    system->default_print_group = value ? strdup(value) : NULL;

    _papplSystemConfigChanged(system);

    _papplRWUnlock(system);
  }
}


//
// 'papplSystemSetDNSSDName()' - Set the DNS-SD service name.
//
// This function sets the DNS-SD service name of the system.  If `NULL`, the
// DNS-SD registration is removed.
//

void
papplSystemSetDNSSDName(
    pappl_system_t *system,		// I - System
    const char     *value)		// I - DNS-SD service name or `NULL` for none
{
  if (system)
  {
    _papplRWLockWrite(system);

    free(system->dns_sd_name);
    system->dns_sd_name      = value ? strdup(value) : NULL;
    system->dns_sd_collision = false;
    system->dns_sd_serial    = 0;

    _papplSystemConfigChanged(system);

    if (!value)
      _papplSystemUnregisterDNSSDNoLock(system);
    else
      _papplSystemRegisterDNSSDNoLock(system);

    _papplRWUnlock(system);
  }
}


//
// 'papplSystemSetEventCallback()' - Set a callback for monitoring system events.
//
// This function sets a callback function to receive event notifications from
// the system.
//

void
papplSystemSetEventCallback(
    pappl_system_t   *system,		// I - System
    pappl_event_cb_t event_cb,		// I - Event callback function
    void             *event_data)	// I - Event callback data
{
  if (system && event_cb)
  {
    _papplRWLockWrite(system);

    system->event_cb   = event_cb;
    system->event_data = event_data;

    _papplRWUnlock(system);
  }
}

void
papplSystemSetScanEventCallback(
    pappl_system_t   *system,		// I - System
    pappl_scanner_event_cb_t scan_event_cb,		// I - Event callback function
    void             *scan_event_data)	// I - Event callback data
{
  if (system && scan_event_cb)
  {
    _papplRWLockWrite(system);

    system->scan_event_cb   = scan_event_cb;
    system->scan_event_data = scan_event_data;

    _papplRWUnlock(system);
  }
}

//
// 'papplSystemSetFooterHTML()' - Set the footer HTML for the web interface.
//
// This function sets the footer HTML for the web interface.
//
// > Note: The footer HTML can only be set prior to calling
// > @link papplSystemRun@.
//

void
papplSystemSetFooterHTML(
    pappl_system_t *system,		// I - System
    const char     *html)		// I - Footer HTML or `NULL` for none
{
  if (system && html && !system->is_running)
  {
    free(system->footer_html);
    system->footer_html = strdup(html);
  }
}


//
// 'papplSystemSetGeoLocation()' - Set the geographic location string.
//
// This function sets the geographic location of the system as a "geo:" URI.
// If `NULL`, the location is cleared.
//

void
papplSystemSetGeoLocation(
    pappl_system_t *system,		// I - System
    const char     *value)		// I - "geo:" URI or `NULL` for none
{
  float	lat, lon;			// Latitude and longitude from geo: URI


  // Validate geo-location - must be NULL or a "geo:" URI...
  if (value && *value && sscanf(value, "geo:%f,%f", &lat, &lon) != 2)
    return;

  if (system)
  {
    _papplRWLockWrite(system);

    free(system->geo_location);
    system->geo_location = value && *value ? strdup(value) : NULL;

    _papplSystemConfigChanged(system);

    _papplSystemRegisterDNSSDNoLock(system);

    _papplRWUnlock(system);
  }
}


//
// 'papplSystemSetHostname()' - Set the system hostname.
//
// This function is deprecated.  Use the @link papplSystemSetHostName@ function
// instead.
//
// @deprecated@ @exclude all@
//

void
papplSystemSetHostname(
    pappl_system_t *system,		// I - System
    const char     *value)		// I - Hostname or `NULL` for default
{
  papplSystemSetHostName(system, value);
}


//
// '_papplSystemSetHostNameNoLock()' - Set the system hostname without locking.
//
// This function sets the system hostname.  If `NULL`, the default hostname
// is used.
//

void
_papplSystemSetHostNameNoLock(
    pappl_system_t *system,		// I - System
    const char     *value)		// I - Hostname or `NULL` for default
{
  char	temp[1024],			// Temporary hostname string
	*ptr;				// Pointer in temporary hostname


  if (system->is_listenhost)
    return;

  if (value)
  {
#if !defined(__APPLE__) && !_WIN32
    cups_file_t	*fp;			// Hostname file

    if ((fp = cupsFileOpen("/etc/hostname", "w")) != NULL)
    {
      cupsFilePrintf(fp, "%s\n", value);
      cupsFileClose(fp);
    }
#endif // !__APPLE__ && !_WIN32

#ifdef HAVE_AVAHI
    _pappl_dns_sd_t	master = _papplDNSSDInit(system);
					// DNS-SD master reference

    if (master)
      avahi_client_set_host_name(master, value);
#endif // HAVE_AVAHI

#if !_WIN32
    sethostname(value, (int)strlen(value));
#endif // !_WIN32
  }
  else
  {
    _papplDNSSDCopyHostName(temp, sizeof(temp));

    if ((ptr = strstr(temp, ".lan")) != NULL && !ptr[4])
    {
      // Replace hostname.lan with hostname.local
      papplCopyString(ptr, ".local", sizeof(temp) - (size_t)(ptr - temp));
    }
    else if (!strrchr(temp, '.'))
    {
      // No domain information, so append .local to hostname...
      ptr = temp + strlen(temp);
      papplCopyString(ptr, ".local", sizeof(temp) - (size_t)(ptr - temp));
    }

    value = temp;
  }

  if (system->hostname && strcasecmp(system->hostname, value) && system->is_running)
  {
    // Force an update of all DNS-SD registrations...
    system->dns_sd_host_changes = -1;
  }

  // Save the new hostname value
  free(system->hostname);
  system->hostname = strdup(value);

  // Set the system TLS credentials...
  cupsSetServerCredentials(NULL, system->hostname, 1);
}


//
// 'papplSystemSetHostName()' - Set the system hostname.
//
// This function sets the system hostname.  If `NULL`, the default hostname
// is used.
//

void
papplSystemSetHostName(
    pappl_system_t *system,		// I - System
    const char     *value)		// I - Hostname or `NULL` for default
{
  if (system)
  {
    _papplRWLockWrite(system);
    _papplSystemSetHostNameNoLock(system, value);
    _papplRWUnlock(system);
  }
}


//
// 'papplSystemSetLocation()' - Set the system location string, if any.
//
// This function sets the human-readable location of the system.  If `NULL`,
// the location is cleared.
//

void
papplSystemSetLocation(
    pappl_system_t *system,		// I - System
    const char     *value)		// I - Location or `NULL` for none
{
  if (system)
  {
    _papplRWLockWrite(system);

    free(system->location);
    system->location    = value ? strdup(value) : NULL;

    _papplSystemConfigChanged(system);

    _papplSystemRegisterDNSSDNoLock(system);

    _papplRWUnlock(system);
  }
}


//
// 'papplSystemSetLogLevel()' - Set the system log level
//
// This function sets the log level as an enumeration.
//

void
papplSystemSetLogLevel(
    pappl_system_t       *system,	// I - System
    pappl_loglevel_t     loglevel)  	// I - Log Level
{
  if (system)
  {
    pthread_mutex_lock(&system->log_mutex);

    system->log_level = loglevel;

    _papplSystemConfigChanged(system);

    pthread_mutex_unlock(&system->log_mutex);
  }
}


//
// 'papplSystemSetMaxClients()' - Set the maximum number of clients.
//
// This function sets the maximum number of simultaneous clients that are
// allowed by the system from 0 (auto) to 32768 (half of the available TCP
// port numbers).
//
// The default maximum number of clients is based on available system resources.
//

void
papplSystemSetMaxClients(
    pappl_system_t *system,		// I - System
    int            max_clients)		// I - Maximum number of clients or `0` for auto
{
  if (!system)
    return;

  if (max_clients <= 0)
  {
    // Determine the maximum number of clients to support...
#ifdef _WIN32
    max_clients = 100;			// Use a default of 100...

#else
    struct rlimit	file_limits,	// Current file descriptor limits
			mem_limits;	// Current memory limits

    max_clients = 100;			// Use a default of 100...

    if (!getrlimit(RLIMIT_NOFILE, &file_limits) && !getrlimit(RLIMIT_DATA, &mem_limits))
    {
      // Calculate a maximum number of clients...
      int max_files, max_mem;		// Maximum files and memory

      if (file_limits.rlim_cur != file_limits.rlim_max && file_limits.rlim_cur < 65536)
      {
        // Try increasing the limit to the maximum allowed...
        if (file_limits.rlim_max > 65536)
	  file_limits.rlim_cur = 65536;
        else
	  file_limits.rlim_cur = file_limits.rlim_max;

        if (setrlimit(RLIMIT_NOFILE, &file_limits))
          getrlimit(RLIMIT_NOFILE, &file_limits);
      }

      // Max clients based on file descriptors is 1/2 the limit...
      if (file_limits.rlim_cur == RLIM_INFINITY)
        max_files = 32768;
      else
        max_files = (int)(file_limits.rlim_cur / 2);

      // Max clients based on memory is 1/64k the limit...
      if (mem_limits.rlim_cur == RLIM_INFINITY)
        max_mem = 32768;
      else
        max_mem = (int)(mem_limits.rlim_cur / 65536);

      // Use min(max_files,max_mem)...
      if (max_files > max_mem)
        max_clients = max_mem;
      else
        max_clients = max_files;
    }
#endif // _WIN32
  }

  // Restrict max_clients to <= 32768...
  if (max_clients > 32768)
    max_clients = 32768;

  // Set the new value...
  _papplRWLockWrite(system);

  system->max_clients = max_clients;

  _papplSystemConfigChanged(system);

  _papplRWUnlock(system);
}


//
// 'papplSystemSetMaxImageSize()' - Set the maximum allowed JPEG/PNG image sizes.
//
// This function sets the maximum size allowed for JPEG and PNG images.  The
// default limits are 16384x16384 and 1/10th the maximum memory the current
// process can use or 1GiB, whichever is less.
//

void
papplSystemSetMaxImageSize(
    pappl_system_t *system,		// I - System
    size_t         max_size,		// I - Maximum image size (uncompressed) or `0` for default
    int            max_width,		// I - Maximum image width in columns or `0` for default
    int            max_height)		// I - Maximum image height in lines or `0` for default
{
  // Range check input...
  if (!system || max_width < 0 || max_height < 0)
    return;

  if (max_size == 0)
  {
    // By default, limit images to 1/10th available memory...
#if _WIN32
    MEMORYSTATUSEX	statex;		// Memory status

    if (GlobalMemoryStatusEx(&statex))
      max_size = (size_t)statex.ullTotalPhys / 10;
    else
      max_size = 16 * 1024 * 1024;

#else
    struct rlimit	limit;		// Memory limits

    if (getrlimit(RLIMIT_DATA, &limit))
      max_size = 16 * 1024 * 1024;
    else
      max_size = limit.rlim_cur / 10;
#endif // _WIN32
  }

  // Don't allow overlarge limits...
  if (max_size > (1024 * 1024 * 1024))	// Max 1GB total size
    max_size = 1024 * 1024 * 1024;
  if (max_width > 65535)		// Max 65535 wide
    max_width = 65535;
  if (max_height > 65535)		// Max 65535 high
    max_height = 65535;

  // Update values
  _papplRWLockWrite(system);

  system->max_image_size   = max_size;
  system->max_image_width  = max_width == 0 ? 16384 : max_width;
  system->max_image_height = max_height == 0 ? 16384 : max_height;

  _papplSystemConfigChanged(system);

  _papplRWUnlock(system);
}


//
// 'papplSystemSetMaxLogSize()' - Set the maximum log file size in bytes.
//
// This function sets the maximum log file size in bytes, which is only used
// when logging directly to a file.  When the limit is reached, the current log
// file is renamed to "filename.O" and a new log file is created.  Set the
// maximum size to `0` to disable log file rotation.
//
// The default maximum log file size is 1MiB or `1048576` bytes.
//

void
papplSystemSetMaxLogSize(
    pappl_system_t *system,		// I - System
    size_t         maxsize)		// I - Maximum log size in bytes or `0` for none
{
  if (system)
  {
    pthread_mutex_lock(&system->log_mutex);

    system->log_max_size = maxsize;

    _papplSystemConfigChanged(system);

    pthread_mutex_unlock(&system->log_mutex);
  }
}


//
// 'papplSystemSetMaxSubscriptions()' - Set the maximum number of event subscriptions.
//
// This function Sets the maximum number of event subscriptions that are
// allowed.  A maximum of `0` means there is no limit.
//
// The default maximum number of event subscriptions is `100`.
//

void
papplSystemSetMaxSubscriptions(
    pappl_system_t *system,		// I - System
    size_t         max_subscriptions)	// I - Maximum number of subscriptions or `0` for no limit
{
  if (system)
  {
    _papplRWLockWrite(system);

    system->max_subscriptions = max_subscriptions;

    _papplSystemConfigChanged(system);

    _papplRWUnlock(system);
  }
}


//
// 'papplSystemSetMIMECallback()' - Set the MIME typing callback for the system.
//
// This function sets a custom MIME typing callback for the system.  The MIME
// typing callback extends the built-in MIME typing support for other media
// types that are supported by the application, typically vendor print formats.
//
// The callback function receives a buffer containing the initial bytes of the
// document data, the length of the buffer, and the callback data.  It can then
// return `NULL` if the content is not recognized or a constant string
// containing the MIME media type, for example "application/vnd.hp-pcl" for
// HP PCL print data.
//

void
papplSystemSetMIMECallback(
    pappl_system_t   *system,		// I - System
    pappl_mime_cb_t  cb,		// I - Callback function
    void             *data)		// I - Callback data
{
  if (system)
  {
    _papplRWLockWrite(system);

    system->mime_cb     = cb;
    system->mime_cbdata = data;

    _papplRWUnlock(system);
  }
}


//
// 'papplSystemSetNetworkCallbacks()' - Set the network configuration callbacks.
//
// This function sets the network configuration callbacks for a system.  The
// "get" callback reads the configuration of all network interfaces and stores
// them in an array of @link pappl_network_t@ structures that is passed to the
// callback.  The "set" callback writes the configuration of all network
// interfaces and returns a boolean value indicating whether the configuration
// has been written successfully.
//

void
papplSystemSetNetworkCallbacks(
    pappl_system_t         *system,	// I - System
    pappl_network_get_cb_t get_cb,	// I - "Get networks" callback
    pappl_network_set_cb_t set_cb,	// I - "Set networks" callback
    void                   *cb_data)	// I - Callback data
{
  // Range check input...
  if (system && (get_cb != NULL) == (set_cb != NULL))
  {
    _papplRWLockWrite(system);

    system->network_get_cb = get_cb;
    system->network_set_cb = set_cb;
    system->network_cbdata = cb_data;

    _papplRWUnlock(system);
  }
}


//
// 'papplSystemSetNextPrinterID()' - Set the next "printer-id" value.
//
// This function sets the unique positive integer identifier that will be used
// for the next printer that is created.  It is typically only called as part
// of restoring the state of a system.
//
// > Note: The next printer ID can only be set prior to calling
// > @link papplSystemRun@.
//

void
papplSystemSetNextPrinterID(
    pappl_system_t *system,		// I - System
    int            next_printer_id)	// I - Next "printer-id" value
{
  if (system && !system->is_running)
  {
    _papplRWLockWrite(system);

    system->next_printer_id = next_printer_id;

    _papplSystemConfigChanged(system);

    _papplRWUnlock(system);
  }
}

//
// 'papplSystemSetNextScannerID()' - Set the next "scanner-id" value.
//
// This function sets the unique positive integer identifier that will be used
// for the next scanner that is created.  It is typically only called as part
// of restoring the state of a system.
//
// > Note: The next scanner ID can only be set prior to calling
// > @link papplSystemRun@.
//

void
papplSystemSetNextScannerID(
    pappl_system_t *system,		// I - System
    int            next_scanner_id)	// I - Next "scanner-id" value
{
  if (system && !system->is_running)
  {
    _papplRWLockWrite(system);

    system->next_scanner_id = next_scanner_id;

    _papplSystemConfigChanged(system);

    _papplRWUnlock(system);
  }
}

//
// 'papplSystemSetOperationCallback()' - Set the IPP operation callback.
//
// This function sets a custom IPP operation handler for the system that is
// called for any IPP operations that are not handled by the built-in IPP
// services.
//
// > Note: The operation callback can only be set prior to calling
// > @link papplSystemRun@.
//

void
papplSystemSetOperationCallback(
    pappl_system_t    *system,		// I - System
    pappl_ipp_op_cb_t cb,		// I - Callback function
    void              *data)		// I - Callback data
{
  if (system && !system->is_running)
  {
    _papplRWLockWrite(system);
    system->op_cb     = cb;
    system->op_cbdata = data;
    _papplRWUnlock(system);
  }
}


//
// 'papplSystemSetOrganization()' - Set the system organization string, if any.
//
// This function sets the organization name for the system.  If `NULL`, the
// name is cleared.
//

void
papplSystemSetOrganization(
    pappl_system_t *system,		// I - System
    const char     *value)		// I - Organization or `NULL` for none
{
  if (system)
  {
    _papplRWLockWrite(system);

    free(system->organization);
    system->organization = value ? strdup(value) : NULL;

    _papplSystemConfigChanged(system);

    _papplRWUnlock(system);
  }
}


//
// 'papplSystemSetOrganizationalUnit()' - Set the system organizational unit
//                                        string, if any.
//
// This function sets the organizational unit name for the system.  If `NULL`,
// the name is cleared.
//

void
papplSystemSetOrganizationalUnit(
    pappl_system_t *system,		// I - System
    const char     *value)		// I - Organizational unit or `NULL` for none
{
  if (system)
  {
    _papplRWLockWrite(system);

    free(system->org_unit);
    system->org_unit = value ? strdup(value) : NULL;

    _papplSystemConfigChanged(system);

    _papplRWUnlock(system);
  }
}


//
// 'papplSystemSetPassword()' - Set the access password hash string.
//
// This function sets the hash for the web access password.  The hash string is
// generated using the @link papplSystemHashPassword@ function.
//
// > Note: The access password is only used when the PAM authentication service
// > is not set.
//

void
papplSystemSetPassword(
    pappl_system_t *system,		// I - System
    const char     *hash)		// I - Hash string
{
  if (system && hash)
  {
    _papplRWLockWrite(system);

    papplCopyString(system->password_hash, hash, sizeof(system->password_hash));

    _papplSystemConfigChanged(system);

    _papplRWUnlock(system);
  }
}


//
// 'papplSystemSetPrinterDrivers()' - Set the list of drivers and the driver
//                                    callbacks.
//
// This function sets the lists of printer drivers, the optional auto-add
// callback function, the optional creation callback, and the required driver
// initialization callback function.
//
// The auto-add callback ("autoadd_cb") finds a compatible driver name for the
// specified printer.  It is used when the client or user specifies the "auto"
// driver name, and for the "autoadd" sub-command for the `papplMainloop` API.
//
// The creation callback ("create_cb") is called at the end of printer creation
// to make any common changes or additions to a new printer.  It is typically
// used to add extra web pages, add per-printer static resources, and/or
// initialize the contact and location information.
//
// The driver initialization callback ("driver_cb") is called to initialize the
// `pappl_pr_driver_data_t` structure, which provides all of the printer
// capabilities and callbacks for printing.
//

void
papplSystemSetPrinterDrivers(
    pappl_system_t        *system,	// I - System
    int                   num_drivers,	// I - Number of drivers
    pappl_pr_driver_t     *drivers,	// I - Drivers
    pappl_pr_autoadd_cb_t autoadd_cb,	// I - Auto-add callback function or `NULL` if none
    pappl_pr_create_cb_t  create_cb,	// I - Printer creation callback function or `NULL` if none
    pappl_pr_driver_cb_t  driver_cb,	// I - Driver initialization callback function
    void                  *data)	// I - Callback data
{
  if (system)
  {
    _papplRWLockWrite(system);

    system->num_drivers   = (cups_len_t)num_drivers;
    system->drivers       = drivers;
    system->autoadd_cb    = autoadd_cb;
    system->create_cb     = create_cb;
    system->driver_cb     = driver_cb;
    system->driver_cbdata = data;

    _papplRWUnlock(system);
  }
}

//
// 'papplSystemSetScannerDrivers()' - Set the list of drivers and the driver
//                                    callbacks for scanners.
//

void
papplSystemSetScannerDrivers(
    pappl_system_t            *system,          // I - System
    int                       num_drivers,      // I - Number of drivers
    pappl_sc_driver_t         *drivers,         // I - Scanner drivers
    pappl_sc_identify_cb_t    identify_cb,      // I - Identify scanner callback or `NULL` if none
    pappl_sc_create_cb_t      create_cb,        // I - Scanner creation callback or `NULL` if none
    pappl_sc_driver_cb_t      driver_cb,        // I - Driver initialization callback
    pappl_sc_delete_cb_t      sc_delete_cb,     // I - Scanner delete callback or `NULL` if none
    pappl_sc_capabilities_cb_t capabilities_cb, // I - Scanner capabilities callback
    pappl_sc_job_create_cb_t  job_create_cb,    // I - Job creation callback
    pappl_sc_job_delete_cb_t  job_delete_cb,    // I - Job delete callback
    pappl_sc_data_cb_t        data_cb,          // I - Data processing callback
    pappl_sc_status_cb_t      status_cb,        // I - Status callback
    pappl_sc_job_complete_cb_t job_complete_cb, // I - Job completion callback
    pappl_sc_job_cancel_cb_t  job_cancel_cb,    // I - Job cancel callback
    pappl_sc_buffer_info_cb_t buffer_info_cb,   // I - Buffer information callback
    pappl_sc_image_info_cb_t  image_info_cb,    // I - Image information callback
    void                      *data)            // I - Callback data
{
  if (system)
  {
    _papplRWLockWrite(system);

    // Set the system's scanner-related fields
    system->num_scanner_drivers = (cups_len_t)num_drivers;
    system->scanner_drivers  = drivers;
    system->identify_cb      = identify_cb;
    system->sc_delete_cb     = sc_delete_cb;
    system->capabilities_cb  = capabilities_cb;
    system->job_create_cb    = job_create_cb;
    system->job_delete_cb    = job_delete_cb;
    system->data_cb          = data_cb;
    system->status_cb        = status_cb;
    system->job_complete_cb  = job_complete_cb;
    system->job_cancel_cb    = job_cancel_cb;
    system->buffer_info_cb   = buffer_info_cb;
    system->image_info_cb    = image_info_cb;
    system->sc_driver_cbdata = data;

    _papplRWUnlock(system);
  }
}

//
// 'papplSystemSetSaveCallback()' - Set the save callback.
//
// This function sets a callback that is used to periodically save the current
// system state.  Typically the callback function ("cb") is
// @link papplSystemSaveState@ and the callback data ("data") is the name of
// the state file:
//
// ```
// |papplSystemSetSaveCallback(system, (pappl_save_cb_t)papplSystemSaveState,
// |    (void *)filename);
// ```
//
// > Note: The save callback can only be set prior to calling
// > @link papplSystemRun@.
//

void
papplSystemSetSaveCallback(
    pappl_system_t  *system,		// I - System
    pappl_save_cb_t cb,			// I - Callback function
    void            *data)		// I - Callback data
{
  if (system && !system->is_running)
  {
    _papplRWLockWrite(system);
    system->save_cb     = cb;
    system->save_cbdata = data;
    _papplRWUnlock(system);
  }
}


//
// 'papplSystemSetUUID()' - Set the system UUID.
//
// This function sets the system UUID value, overriding the default (generated)
// value.  It is typically used when restoring the state of a previous
// incarnation of the system.
//
// > Note: The UUID can only be set prior to calling @link papplSystemRun@.
//

void
papplSystemSetUUID(
    pappl_system_t *system,		// I - System
    const char     *value)		// I - UUID
{
  if (system && !system->is_running)
  {
    _papplRWLockWrite(system);

    free(system->uuid);

    if (value)
    {
      system->uuid = strdup(value);
    }
    else
    {
      char uuid[64];			// UUID value

      _papplSystemMakeUUID(system, NULL, 0, uuid, sizeof(uuid));
      system->uuid = strdup(uuid);
    }

    _papplSystemRegisterDNSSDNoLock(system);

    _papplRWUnlock(system);
  }
}


//
// 'papplSystemSetVersions()' - Set the firmware names and versions.
//
// This function sets the names and versions of each firmware/software component
// of the printer application.
//

void
papplSystemSetVersions(
    pappl_system_t  *system,		// I - System
    int             num_versions,	// I - Number of versions
    pappl_version_t *versions)		// I - Firmware versions
{
  if (system && num_versions && versions)
  {
    _papplRWLockWrite(system);

    if (num_versions > (int)(sizeof(system->versions) / sizeof(system->versions[0])))
      system->num_versions = sizeof(system->versions) / sizeof(system->versions[0]);
    else
      system->num_versions = (cups_len_t)num_versions;

    memcpy(system->versions, versions, (size_t)system->num_versions * sizeof(pappl_version_t));

    _papplRWUnlock(system);
  }
}


//
// 'papplSystemSetWiFiCallbacks()' - Set Wi-Fi callbacks.
//
// This function sets the 802.11 Wi-Fi interface callbacks for the system.  The
// "join_cb" is used to join a Wi-Fi network, the "list_cb" is used to list
// available networks, and the "status_cb" is used to query the current Wi-Fi
// network connection status and Secure Set Identifier (SSID).  The "join_cb"
// and "status_cb" functions are used to support getting and setting the IPP
// "printer-wifi-state", "printer-wifi-ssid", and "printer-wifi-password"
// attributes, while the "list_cb" function enables changing the Wi-Fi network
// from the network web interface, if enabled.
//
// Note: The Wi-Fi callbacks can only be set prior to calling
// @link papplSystemRun@.
//

void
papplSystemSetWiFiCallbacks(
    pappl_system_t         *system,	// I - System
    pappl_wifi_join_cb_t   join_cb,	// I - Join callback
    pappl_wifi_list_cb_t   list_cb,	// I - List callback
    pappl_wifi_status_cb_t status_cb,	// I - Status callback
    void                   *data)	// I - Callback data pointer
{
  if (system && !system->is_running && join_cb && status_cb)
  {
    _papplRWLockWrite(system);
    system->wifi_join_cb   = join_cb;
    system->wifi_list_cb   = list_cb;
    system->wifi_status_cb = status_cb;
    system->wifi_cbdata    = data;
    _papplRWUnlock(system);
  }
}


//
// 'add_listeners()' - Create and add listener sockets to a system.
//

static bool				// O - `true` on success or `false` on failure
add_listeners(
    pappl_system_t *system,		// I - System
    const char     *name,		// I - Host name or `NULL` for any address
    int            port,		// I - Port number
    int            family)		// I - Address family
{
  bool			ret = false;	// Return value
  int			sock;		// Listener socket
  http_addrlist_t	*addrlist,	// Listen addresses
			*addr;		// Current address
  char			service[255];	// Service port


  if (name && (!strcmp(name, "*") || !*name))
    name = NULL;

  snprintf(service, sizeof(service), "%d", port);
  if ((addrlist = httpAddrGetList(name, family, service)) == NULL)
  {
    if (name && *name == '/')
      papplLog(system, PAPPL_LOGLEVEL_INFO, "Unable to lookup address(es) for '%s': %s", name, cupsGetErrorString());
    else
      papplLog(system, PAPPL_LOGLEVEL_INFO, "Unable to lookup address(es) for '%s:%d': %s", name ? name : "*", port, cupsGetErrorString());
  }
  else
  {
    for (addr = addrlist; addr && system->num_listeners < _PAPPL_MAX_LISTENERS; addr = addr->next)
    {
      if ((sock = httpAddrListen(&(addrlist->addr), port)) < 0)
      {
	char	temp[256];		// String address

	if (system->port)
	{
	  if (name && *name == '/')
	    papplLog(system, PAPPL_LOGLEVEL_ERROR, "Unable to create listener socket for '%s': %s", name, cupsGetErrorString());
	  else
	    papplLog(system, PAPPL_LOGLEVEL_ERROR, "Unable to create listener socket for '%s:%d': %s", httpAddrGetString(&addr->addr, temp, (cups_len_t)sizeof(temp)), system->port, cupsGetErrorString());
	}
      }
      else
      {
        ret = true;

	system->listeners[system->num_listeners].fd        = sock;
	system->listeners[system->num_listeners ++].events = POLLIN;

	if (name && *name == '/')
	  papplLog(system, PAPPL_LOGLEVEL_INFO, "Listening for connections on '%s'.", name);
	else
	  papplLog(system, PAPPL_LOGLEVEL_INFO, "Listening for connections on '%s:%d'.", name ? name : "*", port);
      }
    }

    httpAddrFreeList(addrlist);
  }

  return (ret);
}


//
// 'compare_filters()' - Compare two filters.
//

static int				// O - Result of comparison
compare_filters(_pappl_mime_filter_t *a,// I - First filter
                _pappl_mime_filter_t *b)// I - Second filter
{
  int	result = strcmp(a->src, b->src);

  if (!result)
    result = strcmp(a->dst, b->dst);

  return (result);
}


//
// 'compare_timers()' - Compare two timers.
//

static int				// O - Result of comparison
compare_timers(_pappl_timer_t *a,	// I - First timer
               _pappl_timer_t *b)	// I - Second timer
{
  if (a->next < b->next)
    return (-1);
  else if (a->next > b->next)
    return (1);
  else if (a < b)
    return (-1);
  else if (a > b)
    return (1);
  else
    return (0);
}


//
// 'copy_filter()' - Copy a filter definition.
//

static _pappl_mime_filter_t *		// O - New filter
copy_filter(_pappl_mime_filter_t *f)	// I - Filter definition
{
  _pappl_mime_filter_t	*newf = calloc(1, sizeof(_pappl_mime_filter_t));
					// New filter


  if (newf)
    memcpy(newf, f, sizeof(_pappl_mime_filter_t));

  return (newf);
}
