//
// Link functions for the Printer Application Framework
//
// Copyright © 2020-2022 by Michael R Sweet.
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
// absolute URL such as "https://www.example.com/".  The "options" argument
// specifies where the link is shown and whether the link should redirect an
// absolute path to the secure ("https://.../path") web interface.
//

void
papplPrinterAddLink(
    pappl_printer_t  *printer,		// I - Printer
    const char       *label,		// I - Label string
    const char       *path_or_url,	// I - Path or URL
    pappl_loptions_t options)		// I - Link options
{
  _pappl_link_t	l;			// Link


  if (!printer || !label || !path_or_url)
    return;

  _papplRWLockWrite(printer);

  if (!printer->links)
    printer->links = cupsArrayNew((cups_array_cb_t)compare_links, NULL, NULL, 0, (cups_acopy_cb_t)copy_link, (cups_afree_cb_t)free_link);

  l.label       = (char *)label;
  l.path_or_url = (char *)path_or_url;
  l.options     = options;

  if (!cupsArrayFind(printer->links, &l))
    cupsArrayAdd(printer->links, &l);

  _papplRWUnlock(printer);
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

  _papplRWLockWrite(printer);

  l.label = (char *)label;

  cupsArrayRemove(printer->links, &l);

  _papplRWUnlock(printer);
}


//
// 'papplSystemAddLink()' - Add a link to the navigation header.
//
// This function adds a navigation link for the system.  The "path_or_url"
// argument specifies a absolute path such as "/page" or an absolute URL such
// as "https://www.example.com/".  The "options" argument specifies where the
// link is shown and whether the link should redirect an absolute path to the
// secure ("https://.../path") web interface.
//

void
papplSystemAddLink(
    pappl_system_t   *system,		// I - System
    const char       *label,		// I - Label string
    const char       *path_or_url,	// I - Path or URL
    pappl_loptions_t options)		// I - Link options
{
  _pappl_link_t	l;			// Link


  if (!system || !label || !path_or_url)
    return;

  _papplRWLockWrite(system);

  if (!system->links)
    system->links = cupsArrayNew((cups_array_cb_t)compare_links, NULL, NULL, 0, (cups_acopy_cb_t)copy_link, (cups_afree_cb_t)free_link);

  l.label       = (char *)label;
  l.path_or_url = (char *)path_or_url;
  l.options     = options;

  if (!cupsArrayFind(system->links, &l))
    cupsArrayAdd(system->links, &l);

  _papplRWUnlock(system);
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

  _papplRWLockWrite(system);

  l.label = (char *)label;

  cupsArrayRemove(system->links, &l);

  _papplRWUnlock(system);
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
    newl->options     = l->options;

    if (!newl->label || !newl->path_or_url)
    {
      free_link(newl);
      return (NULL);
    }
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
