//
// Lookup functions for the Printer Application Framework
//
// Copyright © 2020-2024 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#include "base-private.h"


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
// '_papplLookupStrings()' - Lookup the string values for all set bits.
//

size_t        // O - number of keywords returned
_papplLookupStrings(
    unsigned            value,        // I - Bitfield value
    size_t              max_keywords, // I - Max number of keywords to return
    char                *keywords[],  // I - Array of char * to be assigned and returned
    size_t              num_strings,  // I - Number of strings
    const char * const  *strings)     // I - Strings
{
  size_t    i;              // Looking var
  unsigned  bit;            // Current bit
  size_t    counter = 0;    // Counter to be returned

  for (i = 0, bit = 1; i < num_strings && counter < max_keywords; i ++, bit *= 2)
  {
    if (value & bit)
    {
      keywords[counter] = (char*)strings[i];
      counter++;
    }
  }

  return counter;
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


  if (!value)
    return (0);

  for (i = 0, bit = 1; i < num_strings; i ++, bit *= 2)
  {
    if (!strcmp(strings[i], value))
      return (bit);
  }

  return (0);
}
