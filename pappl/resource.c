//
// System resource implementation for the Printer Application Framework
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
#include <cups/dir.h>


//
// Local functions...
//

static void		add_resource(pappl_system_t *system, _pappl_resource_t *r);
static int		compare_resources(_pappl_resource_t *a, _pappl_resource_t *b);
static _pappl_resource_t *copy_resource(_pappl_resource_t *r);
static void		free_resource(_pappl_resource_t *r);


//
// 'papplSystemAddResourceCallback()' - Add a dynamic resource that uses a callback function.
//

void
papplSystemAddResourceCallback(
    pappl_system_t      *system,	// I - System object
    const char          *label,		// I - Label (for top-level pages) or `NULL` for none
    const char          *path,		// I - Resource path
    const char          *format,	// I - MIME media type for content such as "text/html"
    bool                secure,		// I - `true` if the page must use HTTPS, `false` otherwise
    pappl_resource_cb_t cb,		// I - Callback function
    void                *data)		// I - Callback data
{
  _pappl_resource_t	r;		// New resource


  if (!system || !path || path[0] != '/' || !format || !cb)
    return;

  memset(&r, 0, sizeof(r));

  r.label  = (char *)label;
  r.path   = (char *)path;
  r.format = (char *)format;
  r.secure = secure;
  r.cb     = cb;
  r.cbdata = data;

  add_resource(system, &r);
}


//
// 'papplSystemAddResourceData()' - Add a static data resource.
//
// The provided data is not copied to the resource and must remain stable for
// as long as the resource is added to the system.
//

void
papplSystemAddResourceData(
    pappl_system_t *system,		// I - System object
    const char     *path,		// I - Resource path
    const char     *format,		// I - MIME media type such as "image/png"
    const void     *data,		// I - Resource data
    size_t         datalen)		// I - Size of resource data
{
  _pappl_resource_t	r;		// New resource


  if (!system || !path || path[0] != '/' || !format || !data || datalen == 0)
    return;

  memset(&r, 0, sizeof(r));

  r.path          = (char *)path;
  r.format        = (char *)format;
  r.last_modified = time(NULL);
  r.data          = data;
  r.length        = datalen;

  add_resource(system, &r);
}


//
// 'papplSystemAddResourceDirectory()' - Add external files in a directory as resources.
//

void
papplSystemAddResourceDirectory(
    pappl_system_t *system,		// I - System object
    const char     *basepath,		// I - Base resource path
    const char     *directory)		// I - Directory containing resource files
{
  cups_dir_t		*dir;		// Directory pointer
  cups_dentry_t		*dent;		// Current directory entry
  char			filename[1024],	// External filename
			path[1024],	// Resource path
			*ext;		// Extension on filename
  const char		*format;	// MIME media type
  _pappl_resource_t	r;		// New resource


  if (!system || !basepath || !directory)
    return;

  // Read all files in the directory...
  if ((dir = cupsDirOpen(directory)) == NULL)
    return;

  while ((dent = cupsDirRead(dir)) != NULL)
  {
    // Skip dot files, directories, and files without an extension...
    if (dent->filename[0] == '.' || !S_ISREG(dent->fileinfo.st_mode) || (ext = strrchr(dent->filename, '.')) == NULL)
      continue;

    // See if this is an extension we recognize...
    if (!strcmp(ext, ".css"))
      format = "text/css";
    else if (!strcmp(ext, ".html"))
      format = "text/html";
    else if (!strcmp(ext, ".icc"))
      format = "application/vnd.iccprofile";
    else if (!strcmp(ext, ".jpg") || !strcmp(ext, ".jpeg"))
      format = "image/jpeg";
    else if (!strcmp(ext, ".pdf"))
      format = "application/pdf";
    else if (!strcmp(ext, ".png"))
      format = "image/png";
    else if (!strcmp(ext, ".strings"))
      format = "text/strings";
    else if (!strcmp(ext, ".txt"))
      format = "text/plain";
    else
      continue;

    // Add the file...
    snprintf(filename, sizeof(filename), "%s/%s", directory, dent->filename);
    snprintf(path, sizeof(path), "%s/%s", basepath, dent->filename);

    memset(&r, 0, sizeof(r));

    r.path          = (char *)path;
    r.format        = (char *)format;
    r.filename      = (char *)filename;
    r.last_modified = dent->fileinfo.st_mtime;
    r.length        = dent->fileinfo.st_size;

    add_resource(system, &r);
  }

  cupsDirClose(dir);
}


//
// 'papplSystemAddResourceFile()' - Add an external file as a resource.
//

void
papplSystemAddResourceFile(
    pappl_system_t *system,		// I - System object
    const char     *path,		// I - Resource path
    const char     *format,		// I - MIME media type such as "image/png"
    const char     *filename)		// I - Filename
{
  _pappl_resource_t	r;		// New resource
  struct stat		fileinfo;	// File information


  if (!system || !path || path[0] != '/' || !format || !filename || stat(filename, &fileinfo))
    return;

  memset(&r, 0, sizeof(r));

  r.path          = (char *)path;
  r.format        = (char *)format;
  r.filename      = (char *)filename;
  r.last_modified = fileinfo.st_mtime;
  r.length        = fileinfo.st_size;

  add_resource(system, &r);
}


//
// 'papplSystemAddResourceString()' - Add a static data resource as a C string.
//
// The provided data is not copied to the resource and must remain stable for
// as long as the resource is added to the system.
//

void
papplSystemAddResourceString(
    pappl_system_t *system,		// I - System object
    const char     *path,		// I - Resource path
    const char     *format,		// I - MIME media type such as "image/png"
    const char     *data)		// I - Resource data
{
  _pappl_resource_t	r;		// New resource


  if (!system || !path || path[0] != '/' || !format || !data)
    return;

  memset(&r, 0, sizeof(r));

  r.path          = (char *)path;
  r.format        = (char *)format;
  r.last_modified = time(NULL);
  r.data          = data;
  r.length        = strlen(data);

  add_resource(system, &r);
}


//
// 'papplSystemAddStringsData()' - Add a static localization file resource.
//
// Localization files use the NeXTStep strings ("text/strings") format defined
// in PWG Candidate Standard 5100.13-2013.  The provided data is not copied to
// the resource and must remain stable for as long as the resource is added to
// the system.
//

void
papplSystemAddStringsData(
    pappl_system_t *system,		// I - System object
    const char     *path,		// I - Resource path
    const char     *language,		// I - ISO language tag such as "en-US", "fr-CA", etc.
    const char     *data)		// I - Nul-terminated string containing strings file data
{
  _pappl_resource_t	r;		// New resource


  if (!system || !path || path[0] != '/' || !language || !data || !*data)
    return;

  memset(&r, 0, sizeof(r));

  r.path          = (char *)path;
  r.format        = (char *)"text/strings";
  r.language      = (char *)language;
  r.last_modified = time(NULL);
  r.data          = data;
  r.length        = strlen(data);

  add_resource(system, &r);
}


//
// 'papplSystemAddStringsFile()' - Add an external localization file resource.
//

void
papplSystemAddStringsFile(
    pappl_system_t *system,		// I - System object
    const char     *path,		// I - Resource path
    const char     *language,		// I - ISO language tag such as "en-US", "fr-CA", etc.
    const char     *filename)		// I - Filename
{
  _pappl_resource_t	r;		// New resource
  struct stat		fileinfo;	// File information


  if (!system || !path || path[0] != '/' || !language || !filename || stat(filename, &fileinfo))
    return;

  memset(&r, 0, sizeof(r));

  r.path          = (char *)path;
  r.format        = (char *)"text/strings";
  r.filename      = (char *)filename;
  r.language      = (char *)language;
  r.last_modified = fileinfo.st_mtime;
  r.length        = fileinfo.st_size;

  add_resource(system, &r);
}


//
// '_papplSystemFindResource()' - Find a resource at a path.
//

_pappl_resource_t *			// O - Resource object
_papplSystemFindResource(
    pappl_system_t *system,		// I - System object
    const char     *path)		// I - Resource path
{
  _pappl_resource_t	key,		// Search key
			*match;		// Matching resource, if any


  if (!system || !system->resources || !path)
    return (NULL);

  key.path = (char *)path;

  pthread_rwlock_rdlock(&system->rwlock);

  match = (_pappl_resource_t *)cupsArrayFind(system->resources, &key);

  pthread_rwlock_unlock(&system->rwlock);

  return (match);
}


//
// 'papplSystemRemoveResource()' - Remove a resource at the specified path.
//

void
papplSystemRemoveResource(
    pappl_system_t *system,		// I - System object
    const char     *path)		// I - Resource path
{
  _pappl_resource_t	key,		// Search key
			*match;		// Matching resource, if any


  if (!system || !system->resources || !path)
    return;

  key.path = (char *)path;

  pthread_rwlock_wrlock(&system->rwlock);

  if ((match = (_pappl_resource_t *)cupsArrayFind(system->resources, &key)) != NULL)
  {
    papplLog(system, PAPPL_LOGLEVEL_DEBUG, "Removing resource for '%s'.", path);
    cupsArrayRemove(system->resources, match);
  }

  pthread_rwlock_unlock(&system->rwlock);
}


//
// 'add_resource()' - Add a resource object to a system object.
//

static void
add_resource(pappl_system_t    *system,	// I - System object
             _pappl_resource_t *r)	// I - Resource
{
  pthread_rwlock_wrlock(&system->rwlock);

  if (!cupsArrayFind(system->resources, r))
  {
    papplLog(system, PAPPL_LOGLEVEL_DEBUG, "Adding resource for '%s'.", r->path);

    if (!system->resources)
      system->resources = cupsArrayNew3((cups_array_func_t)compare_resources, NULL, NULL, 0, (cups_acopy_func_t)copy_resource, (cups_afree_func_t)free_resource);

    cupsArrayAdd(system->resources, r);
  }

  pthread_rwlock_unlock(&system->rwlock);
}


//
// 'compare_resources()' - Compare the path of two resources.
//

static int				// O - Result of comparison
compare_resources(_pappl_resource_t *a,	// I - First resource
                  _pappl_resource_t *b)	// I - Second resource
{
  return (strcmp(a->path, b->path));
}


//
// 'copy_resource()' - Make a copy of some resource data.
//

static _pappl_resource_t *		// O - New resource
copy_resource(_pappl_resource_t *r)	// I - Resource to copy
{
  _pappl_resource_t	*newr;		// New resource


  if ((newr = (_pappl_resource_t *)calloc(1, sizeof(_pappl_resource_t))) != NULL)
  {
    newr->path          = strdup(r->path);
    newr->format        = strdup(r->format);
    newr->last_modified = r->last_modified;
    newr->secure        = r->secure;
    newr->data          = r->data;
    newr->length        = r->length;
    newr->cb            = r->cb;
    newr->cbdata        = r->cbdata;

    if (r->label)
      newr->label = strdup(r->label);
    if (r->filename)
      newr->filename = strdup(r->filename);
    if (r->language)
      newr->language = strdup(r->language);
  }

  return (newr);
}


//
// 'free_resource()' - Free the memory used for a resource.
//

static void
free_resource(_pappl_resource_t *r)	// I - Resource
{
  free(r->label);
  free(r->path);
  free(r->format);
  free(r->filename);
  free(r->language);

  free(r);
}
