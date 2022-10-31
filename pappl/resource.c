//
// System resource implementation for the Printer Application Framework
//
// Copyright © 2019-2022 by Michael R Sweet.
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
// 'papplSystemAddResourceCallback()' - Add a dynamic resource that uses a
//                                      callback function.
//
// This function adds a dynamic resource at the specified path.  When a client
// GET or POST request is received at the specified path, the "cb" function
// will be called with the client pointer and "data" pointer to respond to it.
//
// Resource callbacks are most often used to implement custom web pages.
//
// > Note: Any custom web page that is added prior to calling the
// > @link papplSystemRun@ function will replace the corresponding standard web
// > page at the same path.
//

void
papplSystemAddResourceCallback(
    pappl_system_t      *system,	// I - System object
    const char          *path,		// I - Resource path
    const char          *format,	// I - MIME media type for content such as "text/html"
    pappl_resource_cb_t cb,		// I - Callback function
    void                *data)		// I - Callback data
{
  _pappl_resource_t	r;		// New resource


  if (!system || !path || path[0] != '/' || !format || !cb)
    return;

  memset(&r, 0, sizeof(r));

  r.path   = (char *)path;
  r.format = (char *)format;
  r.cb     = cb;
  r.cbdata = data;

  add_resource(system, &r);
}


//
// 'papplSystemAddResourceData()' - Add a static data resource.
//
// This function adds a static resource at the specified path.  The provided
// data is not copied to the resource and must remain stable for as long as the
// resource is added to the system.
//
// > Note: Any resource that is added prior to calling the @link papplSystemRun@
// > function will replace the corresponding standard resource at the same path.
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
// 'papplSystemAddResourceDirectory()' - Add external files in a directory as
//                                       resources.
//
// This function adds static resources from the specified directory under the
// specified path.  The directory is scanned and only those files present at the
// time of the call are available, and those files must remain stable for as
// long as the resources are added to the system..
//
// > Note: Any resource that is added prior to calling the @link papplSystemRun@
// > function will replace the corresponding standard resource at the same path.
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
    r.length        = (size_t)dent->fileinfo.st_size;

    add_resource(system, &r);
  }

  cupsDirClose(dir);
}


//
// 'papplSystemAddResourceFile()' - Add an external file as a resource.
//
// This function adds a static resource at the specified path.  The provided
// file is not copied to the resource and must remain stable for as long as the
// resource is added to the system.
//
// > Note: Any resource that is added prior to calling the @link papplSystemRun@
// > function will replace the corresponding standard resource at the same path.
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
  r.length        = (size_t)fileinfo.st_size;

  add_resource(system, &r);
}


//
// 'papplSystemAddResourceString()' - Add a static data resource as a C string.
//
// This function adds a static resource at the specified path.  The provided
// data is not copied to the resource and must remain stable for as long as the
// resource is added to the system.
//
// > Note: Any resource that is added prior to calling the @link papplSystemRun@
// > function will replace the corresponding standard resource at the same path.
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
// This function adds a static localization resource at the specified path.
// Localization files use the NeXTStep strings ("text/strings") format defined
// in PWG Candidate Standard 5100.13-2013.  The provided data is not copied to
// the resource and must remain stable for as long as the resource is added to
// the system.
//
// > Note: Any resource that is added prior to calling the @link papplSystemRun@
// > function will replace the corresponding standard resource at the same path.
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

  // Load the localization...
  _papplLocCreate(system, _papplSystemFindResourceForPath(system, path));
}


//
// 'papplSystemAddStringsFile()' - Add an external localization file resource.
//
// This function adds a static localization resource at the specified path.
// Localization files use the NeXTStep strings ("text/strings") format defined
// in PWG Candidate Standard 5100.13-2013.  The provided file is not copied to
// the resource and must remain stable for as long as the resource is added to
// the system.
//
// > Note: Any resource that is added prior to calling the @link papplSystemRun@
// > function will replace the corresponding standard resource at the same path.
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
  r.length        = (size_t)fileinfo.st_size;

  add_resource(system, &r);

  // Load the localization...
  _papplLocCreate(system, _papplSystemFindResourceForPath(system, path));
}


//
// '_papplSystemFindResourceForLanguage()' - Find a resource for a language.
//

_pappl_resource_t *			// O - Resource object
_papplSystemFindResourceForLanguage(
    pappl_system_t *system,		// I - System object
    const char     *language)		// I - Language
{
  _pappl_resource_t	*r;		// Resource


  if (!system || !system->resources || !language)
    return (NULL);

  _papplRWLockRead(system);

  for (r = (_pappl_resource_t *)cupsArrayGetFirst(system->resources); r; r = (_pappl_resource_t *)cupsArrayGetNext(system->resources))
  {
    if (r->format && !strcmp(r->format, "text/strings") && r->language && !strcmp(r->language, language))
      break;
  }

  _papplRWUnlock(system);

  return (r);
}


//
// '_papplSystemFindResourceForPath()' - Find a resource at a path.
//

_pappl_resource_t *			// O - Resource object
_papplSystemFindResourceForPath(
    pappl_system_t *system,		// I - System object
    const char     *path)		// I - Resource path
{
  _pappl_resource_t	key,		// Search key
			*match;		// Matching resource, if any
  char			altpath[1024];	// Alternate path


  if (!system || !system->resources || !path)
    return (NULL);

  key.path = (char *)path;

  _papplRWLockRead(system);

  if ((match = (_pappl_resource_t *)cupsArrayFind(system->resources, &key)) == NULL)
  {
    snprintf(altpath, sizeof(altpath), "%s/", path);
    key.path = altpath;
    match = (_pappl_resource_t *)cupsArrayFind(system->resources, &key);
  }

  _papplRWUnlock(system);

  return (match);
}


//
// 'papplSystemRemoveResource()' - Remove a resource at the specified path.
//
// This function removes a resource at the specified path.
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

  _papplRWLockWrite(system);

  if ((match = (_pappl_resource_t *)cupsArrayFind(system->resources, &key)) != NULL)
  {
    papplLog(system, PAPPL_LOGLEVEL_DEBUG, "Removing resource for '%s'.", path);
    cupsArrayRemove(system->resources, match);
  }

  _papplRWUnlock(system);
}


//
// 'add_resource()' - Add a resource object to a system object.
//

static void
add_resource(pappl_system_t    *system,	// I - System object
             _pappl_resource_t *r)	// I - Resource
{
  _papplRWLockWrite(system);

  if (!cupsArrayFind(system->resources, r))
  {
    papplLog(system, PAPPL_LOGLEVEL_DEBUG, "Adding resource for '%s'.", r->path);

    if (!system->resources)
      system->resources = cupsArrayNew((cups_array_cb_t)compare_resources, NULL, NULL, 0, (cups_acopy_cb_t)copy_resource, (cups_afree_cb_t)free_resource);

    cupsArrayAdd(system->resources, r);
  }

  _papplRWUnlock(system);
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
    newr->data          = r->data;
    newr->length        = r->length;
    newr->cb            = r->cb;
    newr->cbdata        = r->cbdata;

    if (r->filename)
      newr->filename = strdup(r->filename);
    if (r->language)
      newr->language = strdup(r->language);

    if (!newr->path || !newr->format || (r->filename && !newr->filename) || (r->language && !newr->language))
    {
      free_resource(newr);
      return (NULL);
    }
  }

  return (newr);
}


//
// 'free_resource()' - Free the memory used for a resource.
//

static void
free_resource(_pappl_resource_t *r)	// I - Resource
{
  free(r->path);
  free(r->format);
  free(r->filename);
  free(r->language);

  free(r);
}
