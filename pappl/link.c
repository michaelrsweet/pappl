//
// Link functions for the Printer Application Framework
//
// Copyright © 2020 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

//
// Include necessary headers...
//

#include "pappl-private.h"


//
// Local functions...
//

static int		compare_links(_pappl_link_t *a, _pappl_link_t *b);
static _pappl_link_t	*copy_link(_pappl_link_t *l);
static void		free_link(_pappl_link_t *l);


//
// 'papplPrinterAddLink()' - Add a printer link to the navigation header.
//
// This function adds a navigation link for a printer.  The "path_or_url"
// argument specifies a absolute path such as "/ipp/print/example/page" or an
// absolute URL such as "https://www.example.com/".  The "secure" argument
// specifies whether the link should redirect an absolute path to the secure
// ("https://.../path") web interface.
//

void
papplPrinterAddLink(
    pappl_printer_t *printer,		// I - Printer
    const char      *label,		// I - Label string
    const char      *path_or_url,	// I - Path or URL
    bool            secure)		// I - `true` to force HTTPS, `false` otherwise
{
  _pappl_link_t	l;			// Link


  if (!printer || !label || !path_or_url)
    return;

  pthread_rwlock_wrlock(&printer->rwlock);

  if (!printer->links)
    printer->links = cupsArrayNew3((cups_array_func_t)compare_links, NULL, NULL, 0, (cups_acopy_func_t)copy_link, (cups_afree_func_t)free_link);

  l.label       = (char *)label;
  l.path_or_url = (char *)path_or_url;
  l.secure      = secure;

  if (!cupsArrayFind(printer->links, &l))
    cupsArrayAdd(printer->links, &l);

  pthread_rwlock_unlock(&printer->rwlock);
}


//
// 'papplPrinterRemoveLink()' - Remove a printer link from the navigation header.
//
// This function removes the named link for the printer.
//

void
papplPrinterRemoveLink(
    pappl_printer_t *printer,		// I - Printer
    const char      *label)		// I - Label string
{
  _pappl_link_t	l;			// Link


  if (!printer || !label)
    return;

  pthread_rwlock_wrlock(&printer->rwlock);

  l.label = (char *)label;

  cupsArrayRemove(printer->links, &l);

  pthread_rwlock_unlock(&printer->rwlock);
}


//
// 'papplSystemAddLink()' - Add a link to the navigation header.
//
// This function adds a navigation link for the system.  The "path_or_url"
// argument specifies a absolute path such as "/page" or an absolute URL such
// as "https://www.example.com/".  The "secure" argument specifies whether the
// link should redirect an absolute path to the secure ("https://.../path") web
// interface.
//

void
papplSystemAddLink(
    pappl_system_t *system,		// I - System
    const char     *label,		// I - Label string
    const char     *path_or_url,	// I - Path or URL
    bool           secure)		// I - `true` to force HTTPS, `false` otherwise
{
  _pappl_link_t	l;			// Link


  if (!system || !label || !path_or_url)
    return;

  pthread_rwlock_wrlock(&system->rwlock);

  if (!system->links)
    system->links = cupsArrayNew3((cups_array_func_t)compare_links, NULL, NULL, 0, (cups_acopy_func_t)copy_link, (cups_afree_func_t)free_link);

  l.label       = (char *)label;
  l.path_or_url = (char *)path_or_url;
  l.secure      = secure;

  if (!cupsArrayFind(system->links, &l))
    cupsArrayAdd(system->links, &l);

  pthread_rwlock_unlock(&system->rwlock);
}


//
// 'papplSystemRemoveLink()' - Remove a link from the navigation header.
//
// This function removes the named link for the system.
//

void
papplSystemRemoveLink(
    pappl_system_t *system,		// I - System
    const char     *label)		// I - Label string
{
  _pappl_link_t	l;			// Link


  if (!system || !label)
    return;

  pthread_rwlock_wrlock(&system->rwlock);

  l.label = (char *)label;

  cupsArrayRemove(system->links, &l);

  pthread_rwlock_unlock(&system->rwlock);
}


//
// 'compare_links()' - Compare two links.
//

static int				// O - Result of comparison
compare_links(_pappl_link_t *a,		// I - First link
              _pappl_link_t *b)		// I - Second link
{
  return (strcmp(a->label, b->label));
}


//
// 'copy_link()' - Copy a link.
//

static _pappl_link_t *			// O - New link
copy_link(_pappl_link_t *l)		// I - Current link
{
  _pappl_link_t *newl = calloc(1, sizeof(_pappl_link_t));
					// New link


  if (newl)
  {
    newl->label       = strdup(l->label);
    newl->path_or_url = strdup(l->path_or_url);
    newl->secure      = l->secure;
  }

  return (newl);
}


//
// 'free_link()' - Free the memory used by a link.
//

static void
free_link(_pappl_link_t *l)		// I - Link
{
  free(l->label);
  free(l->path_or_url);
  free(l);
}
