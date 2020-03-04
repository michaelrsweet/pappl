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

#include "system-private.h"


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
// 'lprintCreateSystem()' - Create a system object.
//

pappl_system_t *			// O - System object
lprintCreateSystem(
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

    if ((system->listeners[system->num_listeners].fd = create_listener(lishost, system->port, AF_INET)) < 0)
      papplLog(system, PAPPL_LOGLEVEL_ERROR, "Unable to create IPv4 listener for %s:%d: %s", lishost ? lishost : "*", system->port, strerror(errno));
    else
      system->listeners[system->num_listeners ++].events = POLLIN;

    if ((system->listeners[system->num_listeners].fd = create_listener(lishost, system->port, AF_INET6)) < 0)
      papplLog(system, PAPPL_LOGLEVEL_ERROR, "Unable to create IPv6 listener for %s:%d: %s", lishost ? lishost : "*", system->port, strerror(errno));
    else
      system->listeners[system->num_listeners ++].events = POLLIN;

    // Error out if we cannot listen to IPv4 or IPv6 addresses...
    if (system->num_listeners == 1)
      goto fatal;

    // Set the server credentials...
    cupsSetServerCredentials(NULL, system->hostname, 1);
  }

  // Initialize random data for a session key...
  snprintf(key, sizeof(key), "%08x%08x%08x%08x%08x%08x%08x%08x", lprintRand(), lprintRand(), lprintRand(), lprintRand(), lprintRand(), lprintRand(), lprintRand(), lprintRand());
  system->session_key = strdup(key);

  // Initialize DNS-SD as needed...
  if (system->subtypes)
    lprintInitDNSSD(system);

  // Load printers...
  if (!load_config(system))
    goto fatal;

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

  papplLog(system, PAPPL_LOGLEVEL_INFO, "System configuration loaded, %d printers.", cupsArrayCount(system->printers));
  papplLog(system, PAPPL_LOGLEVEL_INFO, "Listening for local connections at '%s'.", sockname);
  if (system->hostname)
    papplLog(system, PAPPL_LOGLEVEL_INFO, "Listening for TCP connections at '%s' on port %d.", system->hostname, system->port);

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

  lprintDeleteSystem(system);

  return (NULL);
}


//
// 'lprintDeleteSystem()' - Delete a system object.
//

void
lprintDeleteSystem(
    pappl_system_t *system)		// I - System object
{
  int	i;				// Looping var
  char	sockname[256];			// Domain socket filename


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

  unlink(lprintGetServerPath(sockname, sizeof(sockname)));
}


//
// 'lprintRunSystem()' - Run the printer service.
//

void
lprintRunSystem(pappl_system_t *system)// I - System
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
	  if ((client = lprintCreateClient(system, system->listeners[i].fd)) != NULL)
	  {
	    if (pthread_create(&client->thread_id, NULL, (void *(*)(void *))lprintProcessClient, client))
	    {
	      // Unable to create client thread...
	      papplLog(system, PAPPL_LOGLEVEL_ERROR, "Unable to create client thread: %s", strerror(errno));
	      lprintDeleteClient(client);
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
      save_config(system);
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
      lprintCleanJobs(system);
  }

  papplLog(system, PAPPL_LOGLEVEL_INFO, "Shutting down main loop.");

  if (system->save_time)
  {
    // Save the configuration...
    pthread_rwlock_rdlock(&system->rwlock);
    save_config(system);
    pthread_rwlock_unlock(&system->rwlock);
  }
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
  const char		*host;		// Listen hostname
  char			service[255];	// Service port


  snprintf(service, sizeof(service), "%d", port);
  if ((addrlist = httpAddrGetList(name, family, service)) == NULL)
  {
    return (-1);

  // Create listener sockets...
  if (strcmp(name, "localhost"))
    host = NULL;
  else
    host = "localhost";

  if (port == 0)
    port = 9000 + (getuid() % 1000);

  if ((system->listeners[system->num_listeners].fd = create_listener(lishost, system->port, AF_INET)) < 0)
    papplLog(system, PAPPL_LOGLEVEL_ERROR, "Unable to create IPv4 listener for %s:%d: %s", lishost ? lishost : "*", system->port, strerror(errno));
  else
    system->listeners[system->num_listeners ++].events = POLLIN;

  sock = httpAddrListen(&(addrlist->addr), port);

  httpAddrFreeList(addrlist);

  return (sock);
}


//
// 'sigterm_handler()' - SIGTERM handler.
//

static void
sigterm_handler(int sig)		// I - Signal (ignored)
{
  shutdown_system = true;
}
