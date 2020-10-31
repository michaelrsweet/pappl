//
// System object for the Printer Application Framework
//
// Copyright © 2019-2020 by Michael R Sweet.
// Copyright © 2010-2019 by Apple Inc.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

//
// Include necessary headers...
//

#include "pappl-private.h"
#include "resource-private.h"
#include "device-private.h"


//
// Local globals...
//

static bool	shutdown_system = false;// Shutdown system?
static bool	restart_logging = false;// Restart logging?


//
// Local functions...
//

static void	make_attributes(pappl_system_t *system);
static void	sighup_handler(int sig);
static void	sigterm_handler(int sig);


//
// '_papplSystemAddPrinterIcons()' - (Re)add printer icon resources.
//

void
_papplSystemAddPrinterIcons(
    pappl_system_t  *system,		// I - System
    pappl_printer_t *printer)		// I - Printer
{
  char		path[256];		// Resource path
  pappl_icon_t	*icons = printer->driver_data.icons;
					// Printer icons


  snprintf(path, sizeof(path), "%s/icon-sm.png", printer->uriname);
  papplSystemRemoveResource(system, path);
  if (icons[0].filename[0])
    papplSystemAddResourceFile(system, path, "image/png", icons[0].filename);
  else if (icons[0].data && icons[0].datalen)
    papplSystemAddResourceData(system, path, "image/png", icons[0].data, icons[0].datalen);
  else
    papplSystemAddResourceData(system, path, "image/png", icon_sm_png, sizeof(icon_sm_png));

  snprintf(path, sizeof(path), "%s/icon-md.png", printer->uriname);
  papplSystemRemoveResource(system, path);
  if (icons[1].filename[0])
    papplSystemAddResourceFile(system, path, "image/png", icons[1].filename);
  else if (icons[1].data && icons[1].datalen)
    papplSystemAddResourceData(system, path, "image/png", icons[1].data, icons[1].datalen);
  else
    papplSystemAddResourceData(system, path, "image/png", icon_md_png, sizeof(icon_md_png));

  snprintf(path, sizeof(path), "%s/icon-lg.png", printer->uriname);
  papplSystemRemoveResource(system, path);
  if (icons[2].filename[0])
    papplSystemAddResourceFile(system, path, "image/png", icons[2].filename);
  else if (icons[2].data && icons[2].datalen)
    papplSystemAddResourceData(system, path, "image/png", icons[2].data, icons[2].datalen);
  else
    papplSystemAddResourceData(system, path, "image/png", icon_lg_png, sizeof(icon_lg_png));
}


//
// '_papplSystemConfigChanged()' - Mark the system configuration as changed.
//

void
_papplSystemConfigChanged(
    pappl_system_t *system)		// I - System
{
  pthread_rwlock_wrlock(&system->rwlock);

  if (system->is_running)
    system->config_changes ++;

  pthread_rwlock_unlock(&system->rwlock);
}


//
// 'papplSystemCreate()' - Create a system object.
//
// This function creates a new system object, which is responsible for managing
// all the printers, jobs, and resources used by the printer application.
//
// The "options" argument specifies which options are enabled for the server:
//
// - `PAPPL_SOPTIONS_NONE`: No options.
// - `PAPPL_SOPTIONS_DNSSD_HOST`: When resolving DNS-SD service name collisions,
//   use the DNS-SD hostname instead of a serial number or UUID.
// - `PAPPL_SOPTIONS_LOG`: Include the log file web page.
// - `PAPPL_SOPTIONS_MULTI_QUEUE`: Support multiple printers.
// - `PAPPL_SOPTIONS_NETWORK`: Include the network settings web page.
// - `PAPPL_SOPTIONS_RAW_SOCKET`: Accept jobs via raw sockets starting on port
//   9100.
// - `PAPPL_SOPTIONS_REMOTE_ADMIN`: Allow remote queue management.
// - `PAPPL_SOPTIONS_SECURITY`: Include the security settings web page.
// - `PAPPL_SOPTIONS_STANDARD`: Include the standard printer and job monitoring
//   web pages.
// - `PAPPL_SOPTIONS_TLS`: Include the TLS settings page.
// - `PAPPL_SOPTIONS_USB_PRINTER`: Accept jobs via USB for the default printer
//   (embedded Linux only).
//
// The "name" argument specifies a human-readable name for the system.
//
// The "port" argument specifies the port number to bind to.  A value of `0`
// will cause an available port number to be assigned when the first listener
// is added with the @link papplSystemAddListeners@ function.
//
// The "subtypes" argument specifies one or more comma-delimited DNS-SD service
// sub-types such as "_print" and "_universal".
//
// The "spooldir" argument specifies the location of job files.  If `NULL`, a
// temporary directory is created.
//
// The "logfile" argument specifies where to send log messages.  If `NULL`, the
// log messages are written to a temporary file.
//
// The "loglevel" argument specifies the initial logging level.
//
// The "auth_service" argument specifies a PAM authentication service name.  If
// `NULL`, no user authentication will be provided.
//
// The "tls_only" argument controls whether the printer application will accept
// unencrypted connections.  In general, this argument should always be `false`
// (allow unencrypted connections) since not all clients support encrypted
// printing.
//

pappl_system_t *			// O - System object
papplSystemCreate(
    pappl_soptions_t options,		// I - Server options
    const char       *name,		// I - System name
    int              port,		// I - Port number or `0` for auto
    const char       *subtypes,		// I - DNS-SD sub-types or `NULL` for none
    const char       *spooldir,		// I - Spool directory or `NULL` for default
    const char       *logfile,		// I - Log file or `NULL` for default
    pappl_loglevel_t loglevel,		// I - Log level
    const char       *auth_service,	// I - PAM authentication service or `NULL` for none
    bool             tls_only)		// I - Only support TLS connections?
{
  pappl_system_t	*system;	// System object
  const char		*tmpdir;	// Temporary directory


  if (!name)
    return (NULL);

  // Allocate memory...
  if ((system = (pappl_system_t *)calloc(1, sizeof(pappl_system_t))) == NULL)
    return (NULL);

  // Initialize values...
  pthread_rwlock_init(&system->rwlock, NULL);
  pthread_rwlock_init(&system->session_rwlock, NULL);

  system->options         = options;
  system->start_time      = time(NULL);
  system->name            = strdup(name);
  system->dns_sd_name     = strdup(name);
  system->port            = port;
  system->directory       = spooldir ? strdup(spooldir) : NULL;
  system->logfd           = -1;
  system->logfile         = logfile ? strdup(logfile) : NULL;
  system->loglevel        = loglevel;
  system->logmaxsize      = 1024 * 1024;
  system->next_client     = 1;
  system->next_printer_id = 1;
  system->subtypes        = subtypes ? strdup(subtypes) : NULL;
  system->tls_only        = tls_only;
  system->admin_gid       = (gid_t)-1;
  system->auth_service    = auth_service ? strdup(auth_service) : NULL;

  // Make sure the system name and UUID are initialized...
  papplSystemSetHostname(system, NULL);
  papplSystemSetUUID(system, NULL);

  // Set the system TLS credentials...
  cupsSetServerCredentials(NULL, system->hostname, 1);

  // See if the spool directory can be created...
  if ((tmpdir = getenv("TMPDIR")) == NULL)
#ifdef __APPLE__
    tmpdir = "/private/tmp";
#else
    tmpdir = "/tmp";
#endif // __APPLE__

  if (!system->directory)
  {
    char	newspooldir[256];	// Spool directory

    snprintf(newspooldir, sizeof(newspooldir), "%s/pappl%d.d", tmpdir, (int)getpid());
    system->directory = strdup(newspooldir);
  }

  if (mkdir(system->directory, 0700) && errno != EEXIST)
  {
    perror(system->directory);
    goto fatal;
  }

  // Initialize logging...
  if (system->loglevel == PAPPL_LOGLEVEL_UNSPEC)
    system->loglevel = PAPPL_LOGLEVEL_ERROR;

  if (!system->logfile)
  {
    // Default log file is $TMPDIR/papplUID.log...
    char newlogfile[256];		// Log filename

    snprintf(newlogfile, sizeof(newlogfile), "%s/pappl%d.log", tmpdir, (int)getpid());

    system->logfile = strdup(newlogfile);
  }

  _papplLogOpen(system);

  // Initialize authentication...
  if (system->auth_service && !strcmp(system->auth_service, "none"))
  {
    free(system->auth_service);
    system->auth_service = NULL;
  }

  // Initialize base filters...
#ifdef HAVE_LIBJPEG
  papplSystemAddMIMEFilter(system, "image/jpeg", "image/pwg-raster", _papplJobFilterJPEG, NULL);
#endif // HAVE_LIBJPEG
#ifdef HAVE_LIBPNG
  papplSystemAddMIMEFilter(system, "image/png", "image/pwg-raster", _papplJobFilterPNG, NULL);
#endif // HAVE_LIBPNG

  return (system);

  // If we get here, something went wrong...
  fatal:

  papplSystemDelete(system);

  return (NULL);
}


//
// 'papplSystemDelete()' - Delete a system object.
//
// > Note: A system object cannot be deleted while the system is running.
//

void
papplSystemDelete(
    pappl_system_t *system)		// I - System object
{
  int	i;				// Looping var


  if (!system || system->is_running)
    return;

  _papplSystemUnregisterDNSSDNoLock(system);

  free(system->uuid);
  free(system->name);
  free(system->dns_sd_name);
  free(system->hostname);
  free(system->server_header);
  free(system->directory);
  free(system->logfile);
  free(system->subtypes);
  free(system->auth_service);
  free(system->admin_group);
  free(system->default_print_group);

  if (system->logfd >= 0 && system->logfd != 2)
    close(system->logfd);

  for (i = 0; i < system->num_listeners; i ++)
    close(system->listeners[i].fd);

  cupsArrayDelete(system->filters);
  cupsArrayDelete(system->links);
  cupsArrayDelete(system->printers);
  cupsArrayDelete(system->resources);

  pthread_rwlock_destroy(&system->rwlock);
  pthread_rwlock_destroy(&system->session_rwlock);

  free(system);
}


//
// 'papplSystemRun()' - Run the printer application.
//
// This function runs the printer application, accepting new connections,
// handling requests, and processing jobs as needed.  It returns once the
// system is shutdown, either through an IPP request or `SIGTERM`.
//

void
papplSystemRun(pappl_system_t *system)	// I - System
{
  int			i,		// Looping var
			count;		// Number of listeners that fired
  pappl_client_t	*client;	// New client
  char			header[HTTP_MAX_VALUE];
					// Server: header value
  int			dns_sd_host_changes;
					// Current number of host name changes
  pappl_printer_t	*printer;	// Current printer


  // Range check...
  if (!system)
    return;

  if (system->is_running)
  {
    papplLog(system, PAPPL_LOGLEVEL_FATAL, "Tried to run system when already running.");
    return;
  }

  if (system->num_listeners == 0)
  {
    papplLog(system, PAPPL_LOGLEVEL_FATAL, "Tried to run system without listeners.");
    return;
  }

  system->is_running = true;

  // Add fallback resources...
  papplSystemAddResourceData(system, "/favicon.png", "image/png", icon_md_png, sizeof(icon_md_png));
  papplSystemAddResourceData(system, "/navicon.png", "image/png", icon_sm_png, sizeof(icon_sm_png));
  papplSystemAddResourceString(system, "/style.css", "text/css", style_css);

  if ((system->options & PAPPL_SOPTIONS_LOG) && system->logfile && strcmp(system->logfile, "-") && strcmp(system->logfile, "syslog"))
  {
    papplSystemAddResourceCallback(system, "/logfile.txt", "text/plain", (pappl_resource_cb_t)_papplSystemWebLogFile, system);
    papplSystemAddResourceCallback(system, "/logs", "text/html", (pappl_resource_cb_t)_papplSystemWebLogs, system);
  }

  if (system->options & PAPPL_SOPTIONS_STANDARD)
  {
    if (system->options & PAPPL_SOPTIONS_MULTI_QUEUE)
    {
      papplSystemAddResourceCallback(system, "/", "text/html", (pappl_resource_cb_t)_papplSystemWebHome, system);
      papplSystemAddResourceCallback(system, "/addprinter", "text/html", (pappl_resource_cb_t)_papplSystemWebAddPrinter, system);
    }
    if (system->options & PAPPL_SOPTIONS_MULTI_QUEUE)
      papplSystemAddResourceCallback(system, "/config", "text/html", (pappl_resource_cb_t)_papplSystemWebConfig, system);
    if (system->options & PAPPL_SOPTIONS_NETWORK)
      papplSystemAddResourceCallback(system, "/network", "text/html", (pappl_resource_cb_t)_papplSystemWebNetwork, system);
    if (system->options & PAPPL_SOPTIONS_SECURITY)
      papplSystemAddResourceCallback(system, "/security", "text/html", (pappl_resource_cb_t)_papplSystemWebSecurity, system);
#ifdef HAVE_GNUTLS
    if (system->options & PAPPL_SOPTIONS_TLS)
    {
      papplSystemAddResourceCallback(system, "/tls-install-crt", "text/html", (pappl_resource_cb_t)_papplSystemWebTLSInstall, system);
      papplSystemAddResourceCallback(system, "/tls-new-crt", "text/html", (pappl_resource_cb_t)_papplSystemWebTLSNew, system);
      papplSystemAddResourceCallback(system, "/tls-new-csr", "text/html", (pappl_resource_cb_t)_papplSystemWebTLSNew, system);
    }
#endif // HAVE_GNUTLS
  }

  // Catch important signals...
  papplLog(system, PAPPL_LOGLEVEL_INFO, "Starting system.");

  signal(SIGTERM, sigterm_handler);
  signal(SIGINT, sigterm_handler);
  signal(SIGHUP, sighup_handler);

  // Set the server header...
  free(system->server_header);
  if (system->versions[0].name[0])
  {
    char	safe_name[64],		// "Safe" name
		*safe_ptr;		// Pointer into "safe" name

    // Replace spaces and other not-allowed characters in the firmware name
    // with an underscore...
    strlcpy(safe_name, system->versions[0].name, sizeof(safe_name));
    for (safe_ptr = safe_name; *safe_ptr; safe_ptr ++)
    {
      if (*safe_ptr <= ' ' || *safe_ptr == '/' || *safe_ptr == 0x7f || (*safe_ptr & 0x80))
        *safe_ptr = '_';
    }

    // Format the server header using the sanitized firmware name and version...
    snprintf(header, sizeof(header), "%s/%s PAPPL/" PAPPL_VERSION " CUPS IPP/2.0", safe_name, system->versions[0].sversion);
  }
  else
  {
    // If no version information is registered, just say "unknown" for the
    // main name...
    strlcpy(header, "Unknown PAPPL/" PAPPL_VERSION " CUPS IPP/2.0", sizeof(header));
  }
  system->server_header = strdup(header);

  // Make the static attributes...
  make_attributes(system);

  // Advertise the system via DNS-SD as needed...
  if (system->dns_sd_name)
    _papplSystemRegisterDNSSDNoLock(system);

  // Start up printers...
  for (printer = (pappl_printer_t *)cupsArrayFirst(system->printers); printer; printer = (pappl_printer_t *)cupsArrayNext(system->printers))
  {
    // Advertise via DNS-SD as needed...
    if (printer->dns_sd_name)
      _papplPrinterRegisterDNSSDNoLock(printer);

    // Start the raw socket listeners as needed...
    if ((system->options & PAPPL_SOPTIONS_RAW_SOCKET) && printer->num_listeners > 0)
    {
      pthread_t	tid;		// Thread ID

      if (pthread_create(&tid, NULL, (void *(*)(void *))_papplPrinterRunRaw, printer))
      {
	// Unable to create listener thread...
	papplLogPrinter(printer, PAPPL_LOGLEVEL_ERROR, "Unable to create raw listener thread: %s", strerror(errno));
      }
      else
      {
	// Detach the main thread from the raw thread to prevent hangs...
	pthread_detach(tid);
      }
    }
  }

  // Start the USB listener as needed...
  if (system->options & PAPPL_SOPTIONS_USB_PRINTER)
  {
    // USB support is limited to a single (default) printer...
    if ((printer = papplSystemFindPrinter(system, NULL, system->default_printer_id, NULL)) != NULL)
    {
      pthread_t	tid;			// Thread ID

      if (pthread_create(&tid, NULL, (void *(*)(void *))_papplPrinterRunUSB, printer))
      {
	// Unable to create USB thread...
	papplLogPrinter(printer, PAPPL_LOGLEVEL_ERROR, "Unable to create USB printer thread: %s", strerror(errno));
      }
      else
      {
	// Detach the main thread from the raw thread to prevent hangs...
	pthread_detach(tid);
      }
    }
  }

  // Loop until we are shutdown or have a hard error...
  while (!shutdown_system)
  {
    if (restart_logging)
    {
      restart_logging = false;
      _papplLogOpen(system);
    }

    if ((count = poll(system->listeners, (nfds_t)system->num_listeners, 1000)) < 0 && errno != EINTR && errno != EAGAIN)
    {
      papplLog(system, PAPPL_LOGLEVEL_ERROR, "Unable to accept new connections: %s", strerror(errno));
      break;
    }

    if (count > 0)
    {
      // Accept client connections as needed...
      for (i = 0; i < system->num_listeners; i ++)
      {
	if (system->listeners[i].revents & POLLIN)
	{
	  if ((client = _papplClientCreate(system, system->listeners[i].fd)) != NULL)
	  {
	    if (pthread_create(&client->thread_id, NULL, (void *(*)(void *))_papplClientRun, client))
	    {
	      // Unable to create client thread...
	      papplLog(system, PAPPL_LOGLEVEL_ERROR, "Unable to create client thread: %s", strerror(errno));
	      _papplClientDelete(client);
	    }
	    else
	    {
	      // Detach the main thread from the client thread to prevent hangs...
	      pthread_detach(client->thread_id);
	    }
	  }
	}
      }
    }

    dns_sd_host_changes = _papplDNSSDGetHostChanges();

    if (system->dns_sd_any_collision || system->dns_sd_host_changes != dns_sd_host_changes)
    {
      // Handle name collisions...
      bool		force_dns_sd = system->dns_sd_host_changes != dns_sd_host_changes;
					// Force re-registration?

      if (force_dns_sd)
        papplSystemSetHostname(system, NULL);

      pthread_rwlock_rdlock(&system->rwlock);

      if (system->dns_sd_collision || force_dns_sd)
        _papplSystemRegisterDNSSDNoLock(system);

      for (printer = (pappl_printer_t *)cupsArrayFirst(system->printers); printer; printer = (pappl_printer_t *)cupsArrayNext(system->printers))
      {
        if (printer->dns_sd_collision || force_dns_sd)
          _papplPrinterRegisterDNSSDNoLock(printer);
      }

      system->dns_sd_any_collision = false;
      system->dns_sd_host_changes  = dns_sd_host_changes;

      pthread_rwlock_unlock(&system->rwlock);
    }

    if (system->config_changes > system->save_changes)
    {
      system->save_changes = system->config_changes;

      if (system->save_cb)
      {
        // Save the configuration...
	(system->save_cb)(system, system->save_cbdata);
      }
    }

    if (system->shutdown_time)
    {
      // Shutdown requested, see if we can do so safely...
      int		jcount = 0;	// Number of active jobs

      // Force shutdown after 60 seconds
      if ((time(NULL) - system->shutdown_time) > 60)
        break;

      // Otherwise shutdown immediately if there are no more active jobs...
      pthread_rwlock_rdlock(&system->rwlock);
      for (printer = (pappl_printer_t *)cupsArrayFirst(system->printers); printer; printer = (pappl_printer_t *)cupsArrayNext(system->printers))
      {
        pthread_rwlock_rdlock(&printer->rwlock);
        jcount += cupsArrayCount(printer->active_jobs);
        pthread_rwlock_unlock(&printer->rwlock);
      }
      pthread_rwlock_unlock(&system->rwlock);

      if (jcount == 0)
        break;
    }

    // Clean out old jobs...
    if (system->clean_time && time(NULL) >= system->clean_time)
      papplSystemCleanJobs(system);
  }

  papplLog(system, PAPPL_LOGLEVEL_INFO, "Shutting down system.");

  ippDelete(system->attrs);
  system->attrs = NULL;

  if (system->dns_sd_name)
    _papplSystemUnregisterDNSSDNoLock(system);

  for (printer = (pappl_printer_t *)cupsArrayFirst(system->printers); printer; printer = (pappl_printer_t *)cupsArrayNext(system->printers))
  {
    // Advertise via DNS-SD as needed...
    if (printer->dns_sd_name)
      _papplPrinterUnregisterDNSSDNoLock(printer);
  }

  if (system->save_changes < system->config_changes && system->save_cb)
  {
    // Save the configuration...
    (system->save_cb)(system, system->save_cbdata);
  }

  system->is_running = false;

  if (system->options & PAPPL_SOPTIONS_USB_PRINTER)
    sleep(2);
}


//
// '_papplSystemMakeUUID()' - Make a UUID for a system, printer, or job.
//
// Unlike httpAssembleUUID, this function does not introduce random data for
// printers so the UUIDs are stable.
//

char *					// I - UUID string
_papplSystemMakeUUID(
    pappl_system_t *system,		// I - System
    const char     *printer_name,	// I - Printer name or `NULL` for none
    int            job_id,		// I - Job ID or `0` for none
    char           *buffer,		// I - String buffer
    size_t         bufsize)		// I - Size of buffer
{
  char			data[1024];	// Source string for MD5
  unsigned char		sha256[32];	// SHA-256 digest/sum


  // Build a version 3 UUID conforming to RFC 4122.
  //
  // Start with the SHA2-256 sum of the hostname, port, object name and
  // number, and some random data on the end for jobs (to avoid duplicates).
  if (printer_name && job_id)
    snprintf(data, sizeof(data), "_PAPPL_JOB_:%s:%d:%s:%d:%08x", system->hostname, system->port, printer_name, job_id, _papplGetRand());
  else if (printer_name)
    snprintf(data, sizeof(data), "_PAPPL_PRINTER_:%s:%d:%s", system->hostname, system->port, printer_name);
  else
    snprintf(data, sizeof(data), "_PAPPL_SYSTEM_:%s:%d", system->hostname, system->port);

  cupsHashData("sha2-256", (unsigned char *)data, strlen(data), sha256, sizeof(sha256));

  // Generate the UUID from the SHA-256...
  snprintf(buffer, bufsize, "urn:uuid:%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x", sha256[0], sha256[1], sha256[3], sha256[4], sha256[5], sha256[6], (sha256[10] & 15) | 0x30, sha256[11], (sha256[15] & 0x3f) | 0x40, sha256[16], sha256[20], sha256[21], sha256[25], sha256[26], sha256[30], sha256[31]);

  return (buffer);
}


//
// 'make_attributes()' - Make the static attributes for the system.
//

static void
make_attributes(pappl_system_t *system)	// I - System
{
  int			i;		// Looping var
  ipp_attribute_t	*attr;		// Attribute
  static const char * const printer_creation_attributes_supported[] =
  {					// "printer-creation-attributes-supported" Values
    "copies-default",
    "finishings-col-default",
    "finishings-default",
    "media-col-default",
    "media-default",
    "orientation-requested-default",
    "print-color-mode-default",
    "print-content-optimize-default",
    "print-quality-default",
    "printer-contact-col",
    "printer-device-id",
    "printer-dns-sd-name",
    "printer-geo-location",
    "printer-location",
    "printer-name",
    "printer-resolution-default",
    "smi2699-device-command",
    "smi2699-device-uri"
  };
  static const char * const system_mandatory_printer_attributes[] =
  {					// "system-mandatory-printer-attributes" values
    "printer-name",
    "smi2699-device-command",
    "smi2699-device-uri"
  };
  static const char * const system_settable_attributes_supported[] =
  {					// "system-settable-attributes-supported" values
    "system-contact-col",
    "system-default-printer-id",
    "system-dns-sd-name",
    "system-geo-location",
    "system-location",
    "system-name",
    "system-organization",
    "system-organizational-unit"
  };


  system->attrs = ippNew();

  // printer-creation-attributes-supported
  ippAddStrings(system->attrs, IPP_TAG_SYSTEM, IPP_CONST_TAG(IPP_TAG_KEYWORD), "printer-creation-attributes-supported", (int)(sizeof(printer_creation_attributes_supported) / sizeof(printer_creation_attributes_supported[0])), NULL, printer_creation_attributes_supported);

  // smi2699-device-command-supported
  if (system->num_drivers > 0)
  {
    attr = ippAddStrings(system->attrs, IPP_TAG_SYSTEM, IPP_CONST_TAG(IPP_TAG_NAME), "smi2699-device-command-supported", system->num_drivers, NULL, NULL);

    for (i = 0; i < system->num_drivers; i ++)
      ippSetString(system->attrs, &attr, i, system->drivers[i].name);
  }

  // smi2699-device-uri-schemes-supported
  _papplDeviceAddSupportedSchemes(system->attrs);

  // system-mandatory-printer-attributes
  ippAddStrings(system->attrs, IPP_TAG_SYSTEM, IPP_CONST_TAG(IPP_TAG_KEYWORD), "system-mandatory-printer-attributes", (int)(sizeof(system_mandatory_printer_attributes) / sizeof(system_mandatory_printer_attributes[0])), NULL, system_mandatory_printer_attributes);

  // system-settable-attributes-supported
  ippAddStrings(system->attrs, IPP_TAG_SYSTEM, IPP_CONST_TAG(IPP_TAG_KEYWORD), "system-settable-attributes-supported", (int)(sizeof(system_settable_attributes_supported) / sizeof(system_settable_attributes_supported[0])), NULL, system_settable_attributes_supported);
}


//
// 'sighup_handler()' - SIGHUP handler
//

static void
sighup_handler(int sig)			// I - Signal (ignored)
{
  (void)sig;

  restart_logging = true;
}


//
// 'sigterm_handler()' - SIGTERM handler.
//

static void
sigterm_handler(int sig)		// I - Signal (ignored)
{
  (void)sig;

  shutdown_system = true;
}
