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


//
// Local globals...
//

static bool		shutdown_system = false;
					// Set to true on signal


//
// Local functions...
//

static bool		create_listeners(pappl_system_t *system, const char *name, int port, int family);
static void		sigterm_handler(int sig);


//
// 'papplSystemCreate()' - Create a system object.
//

pappl_system_t *			// O - System object
papplSystemCreate(
    const char       *hostname,		// I - Hostname or `NULL` for auto
    int              port,		// I - Port number or `0` for auto
    const char       *subtypes,		// I - DNS-SD sub-types or `NULL` for none
    const char       *spooldir,		// I - Spool directory or `NULL` for default
    const char       *logfile,		// I - Log file or `NULL` for default
    pappl_loglevel_t loglevel,		// I - Log level
    const char       *auth_service,	// I - PAM authentication service or `NULL` for none
    const char       *admin_group)	// I - Administrative group or `NULL` for none
{
  pappl_system_t	*system;	// System object
  char			key[65];	// Session key
  const char		*tmpdir;	// Temporary directory


  // Allocate memory...
  if ((system = (pappl_system_t *)calloc(1, sizeof(pappl_system_t))) == NULL)
    return (NULL);

  // Initialize values...
  pthread_rwlock_init(&system->rwlock, NULL);

  if (hostname)
  {
    system->hostname = strdup(hostname);
    system->port     = port ? port : 8000 + (getuid() % 1000);
  }

  system->start_time      = time(NULL);
  system->directory       = spooldir ? strdup(spooldir) : NULL;
  system->logfd           = 2;
  system->logfile         = logfile ? strdup(logfile) : NULL;
  system->loglevel        = loglevel;
  system->next_client     = 1;
  system->next_printer_id = 1;
  system->admin_gid       = (gid_t)-1;

  if (subtypes)
    system->subtypes = strdup(subtypes);
  if (auth_service)
    system->auth_service = strdup(auth_service);
  if (admin_group)
    system->admin_group = strdup(admin_group);

  // Setup listeners...
  system->num_listeners = 0;

  if (system->hostname)
  {
    // Create listener sockets...
    const char *lishost;		// Listen hostname

    if (strcmp(system->hostname, "localhost"))
      lishost = NULL;
    else
      lishost = "localhost";

    if (system->port == 0)
      system->port = 9000 + (getuid() % 1000);

    create_listeners(system, lishost, system->port, AF_INET);
    create_listeners(system, lishost, system->port, AF_INET6);

    // Error out if we cannot listen to IPv4 or IPv6 addresses...
    if (system->num_listeners == 0)
      goto fatal;

    // Set the server credentials...
    cupsSetServerCredentials(NULL, system->hostname, 1);
  }

  // Initialize random data for a session key...
  snprintf(key, sizeof(key), "%08x%08x%08x%08x%08x%08x%08x%08x", _papplGetRand(), _papplGetRand(), _papplGetRand(), _papplGetRand(), _papplGetRand(), _papplGetRand(), _papplGetRand(), _papplGetRand());
  system->session_key = strdup(key);

  // Initialize DNS-SD as needed...
  if (system->subtypes)
    papplSystemInitDNSSD(system);

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

    snprintf(newspooldir, sizeof(newspooldir), "%s/lprint%d.d", tmpdir, (int)getuid());
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
    // Default log file is $TMPDIR/lprintUID.log...
    char newlogfile[256];		// Log filename

    snprintf(newlogfile, sizeof(newlogfile), "%s/lprint%d.log", tmpdir, (int)getuid());

    system->logfile = strdup(newlogfile);
  }

  if (!strcmp(system->logfile, "syslog"))
  {
    // Log to syslog...
    system->logfd = -1;
  }
  else if (!strcmp(system->logfile, "-"))
  {
    // Log to stderr...
    system->logfd = 2;
  }
  else if ((system->logfd = open(system->logfile, O_CREAT | O_WRONLY | O_APPEND | O_NOFOLLOW | O_CLOEXEC, 0600)) < 0)
  {
    // Fallback to stderr if we can't open the log file...
    perror(system->logfile);

    system->logfd = 2;
  }

  // Initialize authentication...
  if (system->auth_service && !strcmp(system->auth_service, "none"))
  {
    free(system->auth_service);
    system->auth_service = NULL;
  }

  if (system->admin_group && strcmp(system->admin_group, "none"))
  {
    char		buffer[8192];	// Buffer for strings
    struct group	grpbuf,		// Group buffer
			*grp = NULL;	// Admin group

    if (getgrnam_r(system->admin_group, &grpbuf, buffer, sizeof(buffer), &grp) || !grp)
      papplLog(system, PAPPL_LOGLEVEL_ERROR, "Unable to find admin-group '%s'.", system->admin_group);
    else
      system->admin_gid = grp->gr_gid;
  }

  return (system);

  // If we get here, something went wrong...
  fatal:

  papplSystemDelete(system);

  return (NULL);
}


//
// 'papplSystemDelete()' - Delete a system object.
//

void
papplSystemDelete(
    pappl_system_t *system)		// I - System object
{
  int	i;				// Looping var


  if (!system)
    return;

  free(system->hostname);
  free(system->directory);
  free(system->logfile);
  free(system->subtypes);
  free(system->auth_service);
  free(system->admin_group);
  free(system->session_key);

  if (system->logfd >= 0 && system->logfd != 2)
    close(system->logfd);

  for (i = 0; i < system->num_listeners; i ++)
    close(system->listeners[i].fd);

  cupsArrayDelete(system->printers);

  pthread_rwlock_destroy(&system->rwlock);

  free(system);
}


//
// 'papplSystemRun()' - Run the printer service.
//

void
papplSystemRun(pappl_system_t *system)// I - System
{
  int			i,		// Looping var
			count,		// Number of listeners that fired
			timeout;	// Timeout for poll()
  pappl_client_t	*client;	// New client


  // Catch important signals...
  papplLog(system, PAPPL_LOGLEVEL_INFO, "Starting main loop.");

  signal(SIGTERM, sigterm_handler);
  signal(SIGINT, sigterm_handler);

  // Loop until we are shutdown or have a hard error...
  while (!shutdown_system)
  {
    if (system->save_time || system->shutdown_time)
      timeout = 5;
    else
      timeout = 10;

    if (system->clean_time && (i = (int)(time(NULL) - system->clean_time)) < timeout)
      timeout = i;

    if ((count = poll(system->listeners, (nfds_t)system->num_listeners, timeout * 1000)) < 0 && errno != EINTR && errno != EAGAIN)
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
	  if ((client = papplClientCreate(system, system->listeners[i].fd)) != NULL)
	  {
	    if (pthread_create(&client->thread_id, NULL, (void *(*)(void *))_papplClientRun, client))
	    {
	      // Unable to create client thread...
	      papplLog(system, PAPPL_LOGLEVEL_ERROR, "Unable to create client thread: %s", strerror(errno));
	      papplClientDelete(client);
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

    if (system->save_time)
    {
      // Save the configuration...
      pthread_rwlock_rdlock(&system->rwlock);
//      save_config(system);
      pthread_rwlock_unlock(&system->rwlock);
      system->save_time = 0;
    }

    if (system->shutdown_time)
    {
      // Shutdown requested, see if we can do so safely...
      int		count = 0;	// Number of active jobs
      pappl_printer_t	*printer;	// Current printer

      // Force shutdown after 60 seconds
      if ((time(NULL) - system->shutdown_time) > 60)
        break;

      // Otherwise shutdown immediately if there are no more active jobs...
      pthread_rwlock_rdlock(&system->rwlock);
      for (printer = (pappl_printer_t *)cupsArrayFirst(system->printers); printer; printer = (pappl_printer_t *)cupsArrayNext(system->printers))
      {
        pthread_rwlock_rdlock(&printer->rwlock);
        count += cupsArrayCount(printer->active_jobs);
        pthread_rwlock_unlock(&printer->rwlock);
      }
      pthread_rwlock_unlock(&system->rwlock);

      if (count == 0)
        break;
    }

    // Clean out old jobs...
    if (system->clean_time && time(NULL) >= system->clean_time)
      papplSystemCleanJobs(system);
  }

  papplLog(system, PAPPL_LOGLEVEL_INFO, "Shutting down main loop.");

  if (system->save_time)
  {
    // Save the configuration...
    pthread_rwlock_rdlock(&system->rwlock);
//    save_config(system);
    pthread_rwlock_unlock(&system->rwlock);
  }
}


//
// '_papplSystemMakeUUID()' - Make a UUID for a system, printer, or job.
//
// Unlike httpAssembleUUID, this function does not introduce random data for
// printers and systems so the UUIDs are stable.
//

char *					// I - UUID string
_papplSystemMakeUUID(
    pappl_system_t *system,		// I - System
    const char      *printer_name,	// I - Printer name or `NULL` for none
    int             job_id,		// I - Job ID or `0` for none
    char            *buffer,		// I - String buffer
    size_t          bufsize)		// I - Size of buffer
{
  char			data[1024];	// Source string for MD5
  unsigned char		sha256[32];	// SHA-256 digest/sum


  // Build a version 3 UUID conforming to RFC 4122.
  //
  // Start with the SHA-256 sum of the hostname, port, object name and
  // number, and some random data on the end for jobs (to avoid duplicates).
  if (printer_name && job_id)
    snprintf(data, sizeof(data), "_PAPPL_JOB_:%s:%d:%s:%d:%08x", system->hostname, system->port, printer_name, job_id, _papplGetRand());
  else if (printer_name)
    snprintf(data, sizeof(data), "_PAPPL_PRINTER_:%s:%d:%s", system->hostname, system->port, printer_name);
  else
    snprintf(data, sizeof(data), "_PAPPL_SYSTEM_:%s:%d", system->hostname, system->port);

  cupsHashData("sha-256", (unsigned char *)data, strlen(data), sha256, sizeof(sha256));

  // Generate the UUID from the SHA-256...
  snprintf(buffer, bufsize, "urn:uuid:%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x", sha256[0], sha256[1], sha256[3], sha256[4], sha256[5], sha256[6], (sha256[10] & 15) | 0x30, sha256[11], (sha256[15] & 0x3f) | 0x40, sha256[16], sha256[20], sha256[21], sha256[25], sha256[26], sha256[30], sha256[31]);

  return (buffer);
}


//
// 'create_listeners()' - Create listener sockets.
//

static bool				// O - `true` on success or `false` on failure
create_listeners(
    pappl_system_t *system,		// I - System
    const char     *name,		// I - Host name or `NULL` for any address
    int            port,		// I - Port number
    int            family)		// I - Address family
{
  int			sock;		// Listener socket
  http_addrlist_t	*addrlist,	// Listen addresses
			*addr;		// Current address
  char			service[255];	// Service port


  snprintf(service, sizeof(service), "%d", port);
  if ((addrlist = httpAddrGetList(name, family, service)) == NULL)
  {
    papplLog(system, PAPPL_LOGLEVEL_ERROR, "Unable to lookup address(es) for %s:%d: %s", name ? name : "*", port, cupsLastErrorString());
    return (false);
  }

  for (addr = addrlist; addr; addr = addr->next)
  {
    if ((sock = httpAddrListen(&(addrlist->addr), port)) < 0)
    {
      char	temp[256];		// String address

      papplLog(system, PAPPL_LOGLEVEL_ERROR, "Unable to create listener socket for %s:%d: %s", httpAddrString(&addr->addr, temp, (int)sizeof(temp)), system->port, cupsLastErrorString());
    }
    else
    {
      system->listeners[system->num_listeners].fd        = sock;
      system->listeners[system->num_listeners ++].events = POLLIN;
    }
  }

  httpAddrFreeList(addrlist);

  return (true);
}


//
// 'sigterm_handler()' - SIGTERM handler.
//

static void
sigterm_handler(int sig)		// I - Signal (ignored)
{
  shutdown_system = true;
}
