//
// Bitmap container (_pappl_qrbb_t) code for managing a QR Code bitmap.
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
// '_papplQRBBAppendBits()' - Append 1 or more bits to a bitmap.
//

void
_papplQRBBAppendBits(
    _pappl_qrbb_t *bitBuffer,		// I - Bitmap container
    uint32_t      val,			// I - Value to add
    uint8_t       length)		// I - Length of value in bits
{
  int8_t	i;			// Looping var
  uint8_t	bit,			// Current output bit
		*dataptr;		// Pointer into output buffer
  uint32_t	offset = bitBuffer->bitOffsetOrWidth,
					// Offset within bitmap
		vbit;			// Current input bit


  // Copy "length" bits from val to the bitmap...
  for (i = length - 1, dataptr = bitBuffer->data + offset / 8, bit = 128 >> (offset & 7), vbit = 1 << i; i >= 0; i --, offset ++, vbit /= 2)
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
  bitBuffer->bitOffsetOrWidth = offset;
}


//
// '_papplQRBBGetBit()' - Get a pixel from a bitmap.
//

bool					// O - `true` if set, `false` if cleared
_papplQRBBGetBit(
    _pappl_qrbb_t *bitGrid,		// I - Bitmap container
    uint8_t       x,			// I - X position
    uint8_t       y)			// I - Y position
{
  uint32_t offset = y * bitGrid->bitOffsetOrWidth + x;
					// Offset within container


  return ((bitGrid->data[offset / 8] & (128 >> (offset & 7))) != 0);
}


//
// '_papplQRBBGetBufferSizeBytes()' - Get the number of bytes required to store N bits.
//

uint16_t				// I - Number of bytes
_papplQRBBGetBufferSizeBytes(
    uint32_t bits)			// I - Number of bits
{
  return ((bits + 7) / 8);
}


//
// '_papplQRBBGetGridSizeBytes()' - Get the required size of a bitmap buffer.
//

uint16_t				// O - Number of bytes required
_papplQRBBGetGridSizeBytes(uint8_t size)// I - Pixel size of the QR code
{
  return ((size * size + 7) / 8);
}


//
// '_papplQRBBInitBuffer()' - Initialize a bitmap container for a bit stream.
//

void
_papplQRBBInitBuffer(
    _pappl_qrbb_t *bitBuffer,		// I - Bitmap container
    uint8_t       *data,		// I - Data buffer
    int32_t       capacityBytes)	// I - Size of data buffer
{
  bitBuffer->bitOffsetOrWidth = 0;
  bitBuffer->capacityBytes    = capacityBytes;
  bitBuffer->data             = data;

  memset(data, 0, bitBuffer->capacityBytes);
}


//
// '_papplQRBBInitGrid()' - Initialize a bitmap container for a grid/image.
//
// The data buffer needs to be at least `_papplQRBBGetGridSizeBytes(size)` in
// length.
//

void
_papplQRBBInitGrid(
    _pappl_qrbb_t *bitGrid,		// I - Bitmap container
    uint8_t       *data,		// I - Data buffer
    uint8_t       size)			// I - Size of line in buffer
{
  bitGrid->bitOffsetOrWidth = size;
  bitGrid->capacityBytes    = _papplQRBBGetGridSizeBytes(size);
  bitGrid->data             = data;

  memset(data, 0, bitGrid->capacityBytes);
}


//
// '_papplQRBBInvertBit()' - Invert or clear a pixel in the bitmap.
//

void
_papplQRBBInvertBit(
    _pappl_qrbb_t *bitGrid,		// I - Bitmap container
    uint8_t       x,			// I - X position
    uint8_t       y,			// I - Y position
    bool          invert)		// I - `true` to invert, `false` to clear
{
  uint32_t	offset = y * bitGrid->bitOffsetOrWidth + x;
					// Offset for pixel
  uint8_t	mask = 128 >> (offset & 0x07);
  					// Bitmask for pixel
  bool		on = (bitGrid->data[offset / 8] & mask) != 0;
					// Is the pixel already set?


  if (on ^ invert)
    bitGrid->data[offset / 8] |= mask;
  else
    bitGrid->data[offset / 8] &= ~mask;
}


//
// '_papplQRBBSetBit()' - Set or clear a pixel in the bitmap.
//

void
_papplQRBBSetBit(
    _pappl_qrbb_t *bitGrid,		// I - Bitmap container
    uint8_t       x,			// I - X position
    uint8_t       y,			// I - Y position
    bool          on)			// I - `true` to set, `false` to clear
{
  uint32_t	offset = y * bitGrid->bitOffsetOrWidth + x;
					// Offset for pixel
  uint8_t	mask = 128 >> (offset & 0x07);
					// Bitmask for pixel


  if (on)
    bitGrid->data[offset / 8] |= mask;
  else
    bitGrid->data[offset / 8] &= ~mask;
}
