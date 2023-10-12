//
// Utility functions for the Printer Application Framework
//
// Copyright © 2019-2023 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#include "base-private.h"


//
// Local functions...
//

static bool	filter_cb(_pappl_ipp_filter_t *filter, ipp_t *dst, ipp_attribute_t *attr);


//
// '_papplCopyAttributes()' - Copy attributes from one message to another.
//

void
_papplCopyAttributes(
    ipp_t        *to,			// I - Destination request
    ipp_t        *from,			// I - Source request
    cups_array_t *ra,			// I - Requested attributes
    ipp_tag_t    group_tag,		// I - Group to copy
    int          quickcopy)		// I - Do a quick copy?
{
  _pappl_ipp_filter_t	filter;		// Filter data


  filter.ra        = ra;
  filter.group_tag = group_tag;

  ippCopyAttributes(to, from, quickcopy, (ipp_copy_cb_t)filter_cb, &filter);
}


//
// 'papplCreateTempFile()' - Create a temporary file.
//

int					// O - File descriptor or `-1` on error
papplCreateTempFile(
    char       *fname,			// I - Filename buffer
    size_t     fnamesize,		// I - Size of filename buffer
    const char *prefix,			// I - Prefix for filename
    const char *ext)			// I - Filename extension, if any
{
  int	fd,				// File descriptor
	tries = 0;			// Number of tries
  char	name[64],			// "Safe" filename
	*nameptr;			// Pointer into filename


  // Range check input...
  if (!fname || fnamesize < 256 || (prefix && strstr(prefix, "../")) || (ext && strstr(ext, "../")))
  {
    if (fname)
      *fname = '\0';

    return (-1);
  }

  if (prefix)
  {
    // Make a name from the prefix argument...
    for (nameptr = name; *prefix && nameptr < (name + sizeof(name) - 1); prefix ++)
    {
      if (isalnum(*prefix & 255) || *prefix == '-' || *prefix == '.')
      {
	*nameptr++ = (char)tolower(*prefix & 255);
      }
      else
      {
	*nameptr++ = '_';

	while (prefix[1] && !isalnum(prefix[1] & 255) && prefix[1] != '-' && prefix[1] != '.')
	  prefix ++;
      }
    }

    *nameptr = '\0';
  }
  else
  {
    // Use a prefix of "t"...
    cupsCopyString(name, "t", sizeof(name));
  }

  do
  {
    // Create a filename...
    if (ext)
      snprintf(fname, fnamesize, "%s/%s%08x.%s", papplGetTempDir(), name, cupsGetRand(), ext);
    else
      snprintf(fname, fnamesize, "%s/%s%08x", papplGetTempDir(), name, cupsGetRand());

    tries ++;
  }
  while ((fd = open(fname, O_WRONLY | O_CREAT | O_TRUNC | O_NOFOLLOW | O_CLOEXEC | O_EXCL | O_BINARY, 0600)) < 0 && tries < 100);

  return (fd);
}


//
// 'papplGetTempDir()' - Get the temporary directory.
//
// This function gets the current temporary directory.
//
// Note: On Windows, the path separators in the temporary directory are
// converted to forward slashes as needed for consistency.
//

const char *				// O - Temporary directory
papplGetTempDir(void)
{
  const char  *tmpdir;			// Temporary directory
  static char tmppath[1024] = "";	// Temporary directory buffer
  static cups_mutex_t tmpmutex = CUPS_MUTEX_INITIALIZER;
					// Mutex to control access


  cupsMutexLock(&tmpmutex);
  if (!tmppath[0])
  {
#if _WIN32
    char *tmpptr;			// Pointer into temporary directory

    // Check the TEMP environment variable...
    if ((tmpdir = getenv("TEMP")) != NULL)
    {
      cupsCopyString(tmppath, tmpdir, sizeof(tmppath));
    }
    else
    {
      // Otherwise use the Windows API to get the user/system default location...
      GetTempPathA(sizeof(tmppath), tmppath);
    }

    // Convert \ to /...
    for (tmpptr = tmppath; *tmpptr; tmpptr ++)
    {
      if (*tmpptr == '\\')
        *tmpptr = '/';
    }

    // Remove trailing /, if any...
    if ((tmpptr = tmppath + strlen(tmppath) - 1) > tmppath && *tmpptr == '/')
      *tmpptr = '\0';

#else
    // Check the TMPDIR environment variable...
    if ((tmpdir = getenv("TMPDIR")) != NULL && !access(tmpdir, W_OK))
    {
      // Set and writable, use it!
      cupsCopyString(tmppath, tmpdir, sizeof(tmppath));
    }
    else
#  ifdef _CS_DARWIN_USER_TEMP_DIR
    // Use the Darwin configuration string value...
    if (!confstr(_CS_DARWIN_USER_TEMP_DIR, tmppath, sizeof(tmppath)))
    {
      // Fallback to /private/tmp...
      cupsCopyString(tmppath, "/private/tmp", sizeof(tmppath));
    }
#  endif // _CS_DARWIN_USER_TEMP_DIR
    {
      // Fallback to /tmp...
      cupsCopyString(tmppath, "/tmp", sizeof(tmppath));
    }
#endif // _WIN32
  }
  cupsMutexUnlock(&tmpmutex);

  return (tmppath);
}


//
// '_papplIsEqual()' - Compare two strings for equality in constant time.
//

bool					// O - `true` on match, `false` on non-match
_papplIsEqual(const char *a,		// I - First string
              const char *b)		// I - Second string
{
  bool	result = true;			// Result


  // Loop through both strings, noting any differences...
  while (*a && *b)
  {
    result &= *a == *b;
    a ++;
    b ++;
  }

  // Return, capturing the equality of the last characters...
  return (result && *a == *b);
}


//
// 'filter_cb()' - Filter printer attributes based on the requested array.
//

static bool				// O - `true` to copy, `false` to ignore
filter_cb(_pappl_ipp_filter_t *filter,	// I - Filter parameters
          ipp_t               *dst,	// I - Destination (unused)
	  ipp_attribute_t     *attr)	// I - Source attribute
{
  // Filter attributes as needed...
#ifndef _WIN32 /* Avoid MS compiler bug */
  (void)dst;
#endif /* !_WIN32 */

  ipp_tag_t group = ippGetGroupTag(attr);
  const char *name = ippGetName(attr);

  if ((filter->group_tag != IPP_TAG_ZERO && group != filter->group_tag && group != IPP_TAG_ZERO) || !name || (!strcmp(name, "media-col-database") && !cupsArrayFind(filter->ra, (void *)name)))
    return (false);

  return (!filter->ra || cupsArrayFind(filter->ra, (void *)name) != NULL);
}
