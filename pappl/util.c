//
// Utility functions for the Printer Application Framework
//
// Copyright © 2019-2025 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#include "base-private.h"
#ifdef HAVE_SYS_RANDOM_H
#  include <sys/random.h>
#endif // HAVE_SYS_RANDOM_H


//
// Local type...
//

typedef struct _pappl_copy_options_s	// Copy options
{
  cups_array_t	*ra;			// Requested attributes
  ipp_tag_t	group_tag;		// Group to copy
  int		quickcopy;		// Do a quick copy?
} _pappl_copy_options_t;


//
// Local functions...
//

static cups_bool_t	 copy_cb(_pappl_copy_options_t *options, ipp_t *dst, ipp_attribute_t *attr);
#if CUPS_VERSION_MAJOR < 3 && CUPS_VERSION_MINOR < 5
static ipp_attribute_t *copy_col(ipp_t *dst, ipp_attribute_t *srcattr, int quickcopy);
#endif // CUPS_VERSION_MAJOR < 3 && CUPS_VERSION_MINOR < 5


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
  _pappl_copy_options_t	options;	// Copy options


  options.ra        = ra;
  options.group_tag = group_tag;
  options.quickcopy = quickcopy;

  ippCopyAttributes(to, from, quickcopy, (ipp_copy_cb_t)copy_cb, &options);
}


//
// 'papplCopyString()' - Safely copy a C string.
//
// This function safely copies a C string to a destination buffer.
//
//

size_t
papplCopyString(char       *dst,	// I - Destination buffer
                const char *src,	// I - Source string
                size_t     dstsize)	// I - Destination size
{
#ifdef HAVE_STRLCPY
  return (strlcpy(dst, src, dstsize));

#else
  size_t srclen = strlen(src);		// Length of source string


  // Copy up to dstsize - 1 bytes
  dstsize --;

  if (srclen > dstsize)
    srclen = dstsize;

  memmove(dst, src, srclen);

  dst[srclen] = '\0';

  return (srclen);
#endif // HAVE_STRLCPY
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
    papplCopyString(name, "t", sizeof(name));
  }

  do
  {
    // Create a filename...
    if (ext)
      snprintf(fname, fnamesize, "%s/%s%08x.%s", papplGetTempDir(), name, papplGetRand(), ext);
    else
      snprintf(fname, fnamesize, "%s/%s%08x", papplGetTempDir(), name, papplGetRand());

    tries ++;
  }
  while ((fd = open(fname, O_WRONLY | O_CREAT | O_TRUNC | O_NOFOLLOW | O_CLOEXEC | O_EXCL | O_BINARY, 0600)) < 0 && tries < 100);

  return (fd);
}


//
// 'papplGetRand()' - Return a 32-bit pseudo-random number.
//
// This function returns a 32-bit pseudo-random number suitable for use as
// one-time identifiers or nonces.  On platforms that provide it, the random
// numbers are generated (or seeded) using system entropy.
//

unsigned				// O - Random number
papplGetRand(void)
{
#if _WIN32
  // rand_s uses real entropy...
  unsigned v;				// Random number


  rand_s(&v);

  return (v);

#elif defined(HAVE_ARC4RANDOM)
  // arc4random uses real entropy automatically...
  return (arc4random());

#else
#  ifdef HAVE_GETRANDOM
  // Linux has the getrandom function to get real entropy, but can fail...
  unsigned	buffer;			// Random number buffer

  if (getrandom(&buffer, sizeof(buffer), 0) == sizeof(buffer))
    return (buffer);

#  elif defined(HAVE_GNUTLS_RND)
  // GNU TLS has the gnutls_rnd function we can use as well, but can fail...
  unsigned	buffer;			// Random number buffer

  if (!gnutls_rnd(GNUTLS_RND_NONCE, &buffer, sizeof(buffer)))
    return (buffer);
#  endif // HAVE_GETRANDOM

  // If we get here then we were unable to get enough random data or the local
  // system doesn't have enough entropy.  Make some up...
  unsigned	i,			// Looping var
		temp;			// Temporary value
  static bool	first_time = true;	// First time we ran?
  static unsigned mt_state[624],	// Mersenne twister state
		mt_index;		// Mersenne twister index
  static pthread_mutex_t mt_mutex = PTHREAD_MUTEX_INITIALIZER;
					// Mutex to control access to state


  pthread_mutex_lock(&mt_mutex);

  if (first_time)
  {
    int		fd;			// "/dev/urandom" file
    struct timeval curtime;		// Current time

    // Seed the random number state...
    if ((fd = open("/dev/urandom", O_RDONLY)) >= 0)
    {
      // Read random entropy from the system...
      if (read(fd, mt_state, sizeof(mt_state[0])) < sizeof(mt_state[0]))
        mt_state[0] = 0;		// Force fallback...

      close(fd);
    }
    else
      mt_state[0] = 0;

    if (!mt_state[0])
    {
      // Fallback to using the current time in microseconds...
      gettimeofday(&curtime, NULL);
      mt_state[0] = (unsigned)(curtime.tv_sec + curtime.tv_usec);
    }

    mt_index = 0;

    for (i = 1; i < 624; i ++)
      mt_state[i] = (unsigned)((1812433253 * (mt_state[i - 1] ^ (mt_state[i - 1] >> 30))) + i);

    first_time = false;
  }

  if (mt_index == 0)
  {
    // Generate a sequence of random numbers...
    unsigned i1 = 1, i397 = 397;	// Looping vars

    for (i = 0; i < 624; i ++)
    {
      temp        = (mt_state[i] & 0x80000000) + (mt_state[i1] & 0x7fffffff);
      mt_state[i] = mt_state[i397] ^ (temp >> 1);

      if (temp & 1)
	mt_state[i] ^= 2567483615u;

      i1 ++;
      i397 ++;

      if (i1 == 624)
	i1 = 0;

      if (i397 == 624)
	i397 = 0;
    }
  }

  // Pull 32-bits of random data...
  temp = mt_state[mt_index ++];
  temp ^= temp >> 11;
  temp ^= (temp << 7) & 2636928640u;
  temp ^= (temp << 15) & 4022730752u;
  temp ^= temp >> 18;

  if (mt_index == 624)
    mt_index = 0;

  pthread_mutex_unlock(&mt_mutex);

  return (temp);
#endif // _WIN32
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
  static pthread_mutex_t tmpmutex = PTHREAD_MUTEX_INITIALIZER;
					// Mutex to control access


  pthread_mutex_lock(&tmpmutex);
  if (!tmppath[0])
  {
#if _WIN32
    char *tmpptr;			// Pointer into temporary directory

    // Check the TEMP environment variable...
    if ((tmpdir = getenv("TEMP")) != NULL)
    {
      papplCopyString(tmppath, tmpdir, sizeof(tmppath));
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
      papplCopyString(tmppath, tmpdir, sizeof(tmppath));
    }
    else
#  ifdef _CS_DARWIN_USER_TEMP_DIR
    // Use the Darwin configuration string value...
    if (!confstr(_CS_DARWIN_USER_TEMP_DIR, tmppath, sizeof(tmppath)))
    {
      // Fallback to /private/tmp...
      papplCopyString(tmppath, "/private/tmp", sizeof(tmppath));
    }
#  endif // _CS_DARWIN_USER_TEMP_DIR
    {
      // Fallback to /tmp...
      papplCopyString(tmppath, "/tmp", sizeof(tmppath));
    }
#endif // _WIN32
  }
  pthread_mutex_unlock(&tmpmutex);

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
// 'copy_cb()' - Callback for ippCopyAttributes.
//

static cups_bool_t			// O - CUPS_BOOL_TRUE to copy, CUPS_BOOL_FALSE to skip
copy_cb(_pappl_copy_options_t *options,	// I - Copy options
        ipp_t                 *dst,	// I - Destination message
        ipp_attribute_t       *attr)	// I - Source attribute
{
  ipp_tag_t	group = ippGetGroupTag(attr);
					// Attribute group
  const char	*name = ippGetName(attr);
					// Attribute name

  (void)dst;

  // Filter out attributes that are not requested...
  if ((options->group_tag != IPP_TAG_ZERO && group != options->group_tag && group != IPP_TAG_ZERO) || !name || (!strcmp(name, "media-col-database") && !cupsArrayFind(options->ra, (void *)name)))
    return (CUPS_BOOL_FALSE);

  if (options->ra && !cupsArrayFind(options->ra, (void *)name))
    return (CUPS_BOOL_FALSE);

#if CUPS_VERSION_MAJOR < 3 && CUPS_VERSION_MINOR < 5
  if (ippGetValueTag(attr) == IPP_TAG_BEGIN_COLLECTION)
  {
    // Copy the collection attribute manually since CUPS 2.4 and earlier has bugs...
    copy_col(dst, attr, options->quickcopy);
    return (CUPS_BOOL_FALSE);
  }
#endif // CUPS_VERSION_MAJOR < 3 && CUPS_VERSION_MINOR < 5

  // Tell ippCopyAttributes to copy this attribute...
  return (CUPS_BOOL_TRUE);
}


#if CUPS_VERSION_MAJOR < 3 && CUPS_VERSION_MINOR < 5
//
// 'copy_col()' - Copy a collection attribute.
//

static ipp_attribute_t *		// O - Destination attribute
copy_col(
    ipp_t           *dst,		// I - Destination IPP message
    ipp_attribute_t *srcattr,		// I - Attribute to copy
    int             quickcopy)		// I - 1 for a referenced copy, 0 for normal
{
  cups_len_t		i,		// Looping var
			count;		// Number of values
  ipp_attribute_t	*dstattr;	// Destination attribute


  // Copy it...
  count   = ippGetCount(srcattr);
  dstattr = ippAddCollections(dst, ippGetGroupTag(srcattr), ippGetName(srcattr), count, /*cols*/NULL);

  if (quickcopy)
  {
    // Do a quick (shallow) copy...
    for (i = 0; i < count; i ++)
      ippSetCollection(dst, &dstattr, i, ippGetCollection(srcattr, i));
  }
  else
  {
    // Do a deep copy...
    for (i = 0; i < count; i ++)
    {
      ipp_t	*dstcol = ippNew();	// Destination collection

      if (!dstcol)
      {
	ippDeleteAttribute(dst, dstattr);
	return (NULL);
      }

      _papplCopyAttributes(dstcol, ippGetCollection(srcattr, i), /*ra*/NULL, /*group_tag*/IPP_TAG_ZERO, quickcopy);

      ippSetCollection(dst, &dstattr, i, dstcol);
      ippDelete(dstcol);
    }
  }

  return (dstattr);
}
#endif // CUPS_VERSION_MAJOR < 3 && CUPS_VERSION_MINOR < 5
