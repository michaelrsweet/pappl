//
// Bitmap container (_pappl_bb_t) code for managing a QR Code bitmap.
//
// The MIT License (MIT)
//
// This library is written and maintained by Richard Moore.
// Major parts were derived from Project Nayuki's library.
// Refactoring and cleanup by Michael R Sweet.
//
// Copyright © 2025-2026 by Michael R Sweet
// Copyright © 2017 Richard Moore     (https://github.com/ricmoo/QRCode)
// Copyright © 2017 Project Nayuki    (https://www.nayuki.io/page/qr-code-generator-library)
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//

//
// Special thanks to Nayuki (https://www.nayuki.io/) from which this library was
// heavily inspired and compared against.
//
// See: https://github.com/nayuki/QR-Code-generator/tree/master/cpp
//

#include "qrcode-private.h"
#include <stdlib.h>
#include <string.h>


//
// '_papplBBAppendBits()' - Append 1 or more bits to a bitmap.
//

void
_papplBBAppendBits(_pappl_bb_t *bb,	// I - Bitmap container
		   uint32_t    val,	// I - Value to add
		   uint8_t     length)	// I - Length of value in bits
{
  int8_t	i;			// Looping var
  uint8_t	bit,			// Current output bit
		*dataptr;		// Pointer into output buffer
  size_t	offset = bb->offset;	// Offset within bitmap
  uint32_t	vbit;			// Current input bit


  // Copy "length" bits from val to the bitmap...
  for (i = length - 1, dataptr = bb->data + offset / 8, bit = 128 >> (offset & 7), vbit = 1 << i; i >= 0; i --, offset ++, vbit /= 2)
  {
    // The original code just OR'd the bit but doesn't clear if the value bit
    // isn't set...
    if (val & vbit)
      *dataptr |= bit;

    // Move to the next bit...
    if (bit == 1)
    {
      dataptr ++;
      bit = 128;
    }
    else
    {
      bit /= 2;
    }
  }

  // Save the new bitmap offset...
  bb->offset = offset;
}


//
// '_papplBBDelete()' - Free memory associated with a bitmap.
//

void
_papplBBDelete(_pappl_bb_t *bb)		// I - Bitmap
{
  if (bb)
  {
    free(bb->data);
    free(bb);
  }
}


//
// '_papplBBGetBit()' - Get a pixel from a bitmap.
//

bool					// O - `true` if set, `false` if cleared
_papplBBGetBit(_pappl_bb_t *bb,		// I - Bitmap container
	       uint8_t     x,		// I - X position
	       uint8_t     y)		// I - Y position
{
  size_t offset = y * bb->width + x;	// Offset within container


  return ((bb->data[offset / 8] & (128 >> (offset & 7))) != 0);
}


//
// '_papplBBInvertBit()' - Invert or clear a pixel in the bitmap.
//

void
_papplBBInvertBit(_pappl_bb_t *bb,	// I - Bitmap container
		  uint8_t     x,	// I - X position
		  uint8_t     y,	// I - Y position
		  bool        invert)	// I - `true` to invert, `false` to clear
{
  size_t	offset = y * bb->width + x;
					// Offset for pixel
  uint8_t	mask = 128 >> (offset & 0x07);
  					// Bitmask for pixel
  bool		on = (bb->data[offset / 8] & mask) != 0;
					// Is the pixel already set?


  if (on ^ invert)
    bb->data[offset / 8] |= mask;
  else
    bb->data[offset / 8] &= ~mask;
}


//
// '_papplBBNewBitmap()' - Create a new bitmap buffer.
//

_pappl_bb_t *				// O - Buffer
_papplBBNewBitmap(size_t dim)		// I - Width and height
{
  _pappl_bb_t	*bb;			// Buffer


  if (dim < 1 || dim > 255)
    return (NULL);

  if ((bb = calloc(1, sizeof(_pappl_bb_t))) != NULL)
  {
    bb->datasize = (dim * dim + 7) / 8;
    bb->width    = (uint8_t)dim;

    if ((bb->data = (uint8_t *)calloc(1, bb->datasize)) == NULL)
    {
      free(bb);
      bb = NULL;
    }
  }

  return (bb);
}


//
// '_papplBBNewBuffer()' - Create a new linear bit buffer.
//

_pappl_bb_t *				// O - Buffer
_papplBBNewBuffer(size_t num_bits)	// I - Number of bits
{
  _pappl_bb_t	*bb;			// Buffer


  if ((bb = calloc(1, sizeof(_pappl_bb_t))) != NULL)
  {
    bb->datasize = (num_bits + 7) / 8;

    if ((bb->data = (uint8_t *)calloc(1, bb->datasize)) == NULL)
    {
      free(bb);
      bb = NULL;
    }
  }

  return (bb);
}


//
// '_papplBBSetBit()' - Set or clear a pixel in the bitmap.
//

void
_papplBBSetBit(_pappl_bb_t *bb,		// I - Bitmap container
	       uint8_t     x,		// I - X position
	       uint8_t     y,		// I - Y position
	       bool        on)		// I - `true` to set, `false` to clear
{
  size_t	offset = y * bb->width + x;
					// Offset for pixel
  uint8_t	mask = 128 >> (offset & 0x07);
					// Bitmask for pixel


  if (on)
    bb->data[offset / 8] |= mask;
  else
    bb->data[offset / 8] &= ~mask;
}
