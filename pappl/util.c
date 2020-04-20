//
// Utility functions for the Printer Application Framework
//
// Copyright © 2019-2020 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

//
// Include necessary headers...
//

#include "base-private.h"
#ifdef HAVE_SYS_RANDOM_H
#  include <sys/random.h>
#endif // HAVE_SYS_RANDOM_H


//
// Local functions...
//

static int	filter_cb(_pappl_ipp_filter_t *filter, ipp_t *dst, ipp_attribute_t *attr);


//
// '_pappl_strlcpy()' - Safely copy a C string.
//

#ifndef HAVE_STRLCPY
size_t
_pappl_strlcpy(char       *dst,		// I - Destination buffer
               const char *src,		// I - Source string
               size_t     dstsize)	// I - Destination size
{
  size_t srclen = strlen(src);		// Length of source string


  // Copy up to dstsize - 1 bytes
  dstsize --;

  if (srclen > dstsize)
    srclen = dstsize;

  memmove(dst, src, srclen);

  dst[srclen] = '\0';

  return (srclen);
}
#endif // !HAVE_STRLCPY


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

  ippCopyAttributes(to, from, quickcopy, (ipp_copycb_t)filter_cb, &filter);
}


//
// '_papplGetRand()' - Return the best 32-bit random number we can.
//

unsigned				// O - Random number
_papplGetRand(void)
{
#ifdef HAVE_ARC4RANDOM
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

  // Fall back to random() seeded with the current time - not ideal, but for
  // our non-cryptographic purposes this is OK...
  static int first_time = 1;		// First time we ran?

  if (first_time)
  {
    srandom(time(NULL));
    first_time = 0;
  }

  return ((unsigned)random());
#endif // __APPLE__
}


//
// 'filter_cb()' - Filter printer attributes based on the requested array.
//

static int				// O - 1 to copy, 0 to ignore
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
    return (0);

  return (!filter->ra || cupsArrayFind(filter->ra, (void *)name) != NULL);
}
