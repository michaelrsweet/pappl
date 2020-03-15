//
// Lookup functions for the Printer Application Framework
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
// '_papplLookupString()' - Lookup the string value for a bit.
//

const char *				// O - Keyword or `NULL`
_papplLookupString(
    unsigned           value,		// I - Bit value
    size_t             num_strings,	// I - Number of strings
    const char * const *strings)	// I - Strings
{
  size_t	i;			// Looking var
  unsigned	bit;			// Current bit


  for (i = 0, bit = 1; i < num_strings; i ++, bit *= 2)
  {
    if (bit == value)
      return (strings[i]);
  }

  return (NULL);
}


//
// '_papplLookupValue()' - Lookup the bit value for a string.
//

unsigned				// O - Bit value or `0`
_papplLookupValue(
    const char         *value,		// I - Keyword value
    size_t             num_strings,	// I - Number of strings
    const char * const *strings)	// I - Strings
{
  size_t	i;			// Looking var
  unsigned	bit;			// Current bit


  for (i = 0, bit = 1; i < num_strings; i ++, bit *= 2)
  {
    if (!strcmp(strings[i], value))
      return (bit);
  }

  return (0);
}
