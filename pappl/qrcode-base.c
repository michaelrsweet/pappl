//
// Code for generating QR Code (_pappl_qrcode_t) bitmaps.
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
// Local globals...
//

static const uint16_t NUM_ERROR_CORRECTION_CODEWORDS[4][40] =
{
  // 1,  2,  3,  4,  5,   6,   7,   8,   9,  10,  11,  12,  13,  14,  15,  16,  17,  18,  19,  20,  21,  22,  23,  24,   25,   26,   27,   28,   29,   30,   31,   32,   33,   34,   35,   36,   37,   38,   39,   40    Error correction level
  { 10, 16, 26, 36, 48,  64,  72,  88, 110, 130, 150, 176, 198, 216, 240, 280, 308, 338, 364, 416, 442, 476, 504, 560,  588,  644,  700,  728,  784,  812,  868,  924,  980, 1036, 1064, 1120, 1204, 1260, 1316, 1372},  // Medium
  {  7, 10, 15, 20, 26,  36,  40,  48,  60,  72,  80,  96, 104, 120, 132, 144, 168, 180, 196, 224, 224, 252, 270, 300,  312,  336,  360,  390,  420,  450,  480,  510,  540,  570,  570,  600,  630,  660,  720,  750},  // Low
  { 17, 28, 44, 64, 88, 112, 130, 156, 192, 224, 264, 308, 352, 384, 432, 480, 532, 588, 650, 700, 750, 816, 900, 960, 1050, 1110, 1200, 1260, 1350, 1440, 1530, 1620, 1710, 1800, 1890, 1980, 2100, 2220, 2310, 2430},  // High
  { 13, 22, 36, 52, 72,  96, 108, 132, 160, 192, 224, 260, 288, 320, 360, 408, 448, 504, 546, 600, 644, 690, 750, 810,  870,  952, 1020, 1050, 1140, 1200, 1290, 1350, 1440, 1530, 1590, 1680, 1770, 1860, 1950, 2040},  // Quartile
};

static const uint8_t NUM_ERROR_CORRECTION_BLOCKS[4][40] =
{
  // Version: (note that index 0 is for padding, and is set to an illegal value)
  // 1, 2, 3, 4, 5, 6, 7, 8, 9,10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40    Error correction level
  {  1, 1, 1, 2, 2, 4, 4, 4, 5, 5,  5,  8,  9,  9, 10, 10, 11, 13, 14, 16, 17, 17, 18, 20, 21, 23, 25, 26, 28, 29, 31, 33, 35, 37, 38, 40, 43, 45, 47, 49},  // Medium
  {  1, 1, 1, 1, 1, 2, 2, 2, 2, 4,  4,  4,  4,  4,  6,  6,  6,  6,  7,  8,  8,  9,  9, 10, 12, 12, 12, 13, 14, 15, 16, 17, 18, 19, 19, 20, 21, 22, 24, 25},  // Low
  {  1, 1, 2, 4, 4, 4, 5, 6, 8, 8, 11, 11, 16, 16, 18, 16, 19, 21, 25, 25, 25, 34, 30, 32, 35, 37, 40, 42, 45, 48, 51, 54, 57, 60, 63, 66, 70, 74, 77, 81},  // High
  {  1, 1, 2, 2, 4, 4, 6, 6, 8, 8,  8, 10, 12, 16, 12, 17, 16, 18, 21, 20, 23, 23, 25, 27, 29, 34, 34, 35, 38, 40, 43, 45, 48, 51, 53, 56, 59, 62, 65, 68},  // Quartile
};

static const uint16_t NUM_RAW_DATA_MODULES[40] =
{
  //1,   2,   3,   4,    5,    6,    7,    8,    9,   10,   11,   12,   13,   14,   15,   16,   17,
  208, 359, 567, 807, 1079, 1383, 1568, 1936, 2336, 2768, 3232, 3728, 4256, 4651, 5243, 5867, 6523,
  //18,   19,   20,   21,    22,    23,    24,    25,   26,    27,     28,    29,    30,    31,
  7211, 7931, 8683, 9252, 10068, 10916, 11796, 12708, 13652, 14628, 15371, 16411, 17483, 18587,
  // 32,    33,    34,    35,    36,    37,    38,    39,    40
  19723, 20891, 22091, 23008, 24272, 25568, 26896, 28256, 29648
};


//
// Local functions...
//

//
// 'get_max()' - Get the maximum of two integers.
//

static inline int			// O - Maximum value
get_max(int a,				// I - First value
        int b)				// I - Second value
{
  if (a > b)
    return (a);
  else
    return (b);
}


//
// 'applyMask()' - XOR the data modules.
//
// XORs the data modules in this QR Code with the given mask pattern.  Due to
// XOR's mathematical properties, calling `applyMask(m)` twice with the same
// value is equivalent to no change at all.  This means it is possible to apply
// a mask, undo it, and try another mask.  Note that a final well-formed QR Code
// symbol needs exactly one mask applied (not zero, not two, etc.).
//

static void
applyMask(_pappl_qrbb_t *modules,	// I - Bitmap container
          _pappl_qrbb_t *isFunction,	// I - Pattern
          uint8_t       mask)		// I - Mask function
{
  uint8_t size = modules->bitOffsetOrWidth;
					// Length of a line


  for (uint8_t y = 0; y < size; y++)
  {
    for (uint8_t x = 0; x < size; x++)
    {
      bool invert = false;		// Invert this pixel?

      if (_papplQRBBGetBit(isFunction, x, y))
        continue;

      switch (mask)
      {
	case 0 :
	    invert = (x + y) % 2 == 0;
	    break;
	case 1 :
	    invert = y % 2 == 0;
	    break;
	case 2 :
	    invert = x % 3 == 0;
	    break;
	case 3 :
	    invert = (x + y) % 3 == 0;
	    break;
	case 4 :
	    invert = (x / 3 + y / 2) % 2 == 0;
	    break;
	case 5 :
	    invert = x * y % 2 + x * y % 3 == 0;
	    break;
	case 6 :
	    invert = (x * y % 2 + x * y % 3) % 2 == 0;
	    break;
	case 7 :
	    invert = ((x + y) % 2 + x * y % 3) % 2 == 0;
	    break;
      }

      _papplQRBBInvertBit(modules, x, y, invert);
    }
  }
}


//
// 'setFunctionModule()' - Set a pixel in both the code and function bitmaps.
//

static void
setFunctionModule(
    _pappl_qrbb_t *modules,		// I - Bitmap container
    _pappl_qrbb_t *isFunction,		// I - Pattern
    uint8_t       x,			// I - X position
    uint8_t       y,			// I - Y position
    bool          on)			// I - `true` to set bitmap, `false` to clear it
{
  _papplQRBBSetBit(modules, x, y, on);
  _papplQRBBSetBit(isFunction, x, y, true);
}


//
// 'drawFinderPattern()' - Draw a 9x9 finder pattern.
//
// This function draws a 9x9 finder pattern including the border separator,
// with the center module at (x, y).
//

static void
drawFinderPattern(
    _pappl_qrbb_t *modules,		// I - Bitmap container
    _pappl_qrbb_t *isFunction,		// I - Pattern
    uint8_t       x,			// I - X position
    uint8_t       y)			// I - Y position
{
  uint8_t size = modules->bitOffsetOrWidth;
					// Width of bitmap


  for (int8_t i = -4; i <= 4; i ++)
  {
    for (int8_t j = -4; j <= 4; j ++)
    {
      uint8_t	dist = get_max(abs(i), abs(j));
					// Chebyshev/infinity norm
      int16_t	xx = x + j,		// X position
		yy = y + i;		// Y position

      if (xx >= 0 && xx < size && yy >= 0 && yy < size)
	setFunctionModule(modules, isFunction, xx, yy, dist != 2 && dist != 4);
    }
  }
}


//
// 'drawAlignmentPattern()' - Draw a 5x5 alignment pattern.
//
// This function draws a 5x5 alignment pattern, with the center module at
// (x, y).
//

static void
drawAlignmentPattern(
    _pappl_qrbb_t *modules,		// I - Bitmap container
    _pappl_qrbb_t *isFunction,		// I - Pattern
    uint8_t       x,			// I - X position
    uint8_t       y)			// I - Y position
{
  for (int8_t i = -2; i <= 2; i ++)
  {
    for (int8_t j = -2; j <= 2; j ++)
      setFunctionModule(modules, isFunction, x + j, y + i, get_max(abs(i), abs(j)) != 1);
  }
}


//
// 'drawFormatBits()' - Draw two copies of the format bits.
//
// This function draws two copies of the format bits (with its own error
// correction code) based on the given mask and this object's error correction
// level field.
//

static void
drawFormatBits(
    _pappl_qrbb_t *modules,		// I - Bitmap container
    _pappl_qrbb_t *isFunction,		// I - Pattern
    uint8_t       ecc,			// I - Error correction code
    uint8_t       mask)			// I - Mask
{
  uint8_t size = modules->bitOffsetOrWidth;
					// Width of bitmap


  // Calculate error correction code and pack bits
  uint32_t data = ecc << 3 | mask;	// errCorrLvl is uint2, mask is uint3
  uint32_t rem = data;

  for (int i = 0; i < 10; i++)
    rem = (rem << 1) ^ ((rem >> 9) * 0x537);

  data = data << 10 | rem;
  data ^= 0x5412;  // uint15

  // Draw first copy
  for (uint8_t i = 0; i <= 5; i ++)
    setFunctionModule(modules, isFunction, 8, i, ((data >> i) & 1) != 0);

  setFunctionModule(modules, isFunction, 8, 7, ((data >> 6) & 1) != 0);
  setFunctionModule(modules, isFunction, 8, 8, ((data >> 7) & 1) != 0);
  setFunctionModule(modules, isFunction, 7, 8, ((data >> 8) & 1) != 0);

  for (int8_t i = 9; i < 15; i ++)
    setFunctionModule(modules, isFunction, 14 - i, 8, ((data >> i) & 1) != 0);

  // Draw second copy
  for (int8_t i = 0; i <= 7; i ++)
    setFunctionModule(modules, isFunction, size - 1 - i, 8, ((data >> i) & 1) != 0);

  for (int8_t i = 8; i < 15; i ++)
    setFunctionModule(modules, isFunction, 8, size - 15 + i, ((data >> i) & 1) != 0);

  setFunctionModule(modules, isFunction, 8, size - 8, true);
}


//
// 'drawVersion()' - Draw two copies of the versions bits.
//
// This function draws two copies of the version bits (with its own error
// correction code), based on this object's version field (which only has an
// effect for 7 <= version <= 40).
//

static void
drawVersion(
    _pappl_qrbb_t *modules,		// I - Bitmap container
    _pappl_qrbb_t *isFunction,		// I - Pattern
    uint8_t       version)		// I - Version bits
{
  int8_t size = modules->bitOffsetOrWidth;
					// Width of bitmap


  // Don't output version bits for small QR codes...
  if (version < 7)
    return;

  // Calculate error correction code and pack bits
  uint32_t rem = version;  // version is uint6, in the range [7, 40]
  for (uint8_t i = 0; i < 12; i ++)
    rem = (rem << 1) ^ ((rem >> 11) * 0x1F25);

  uint32_t data = version << 12 | rem;  // uint18

  // Draw two copies
  for (uint8_t i = 0; i < 18; i ++)
  {
    bool	bit = ((data >> i) & 1) != 0;
					// Bit
    uint8_t	a = size - 11 + i % 3,	// X position
		b = i / 3;		// Y position

    setFunctionModule(modules, isFunction, a, b, bit);
    setFunctionModule(modules, isFunction, b, a, bit);
  }
}


//
// 'drawFunctionPatterns()' - Draw all of the patterns needed for the QR code.
//

static void
drawFunctionPatterns(
    _pappl_qrbb_t *modules,		// I - Bitmap container
    _pappl_qrbb_t *isFunction,		// I - Pattern
    uint8_t       version,		// I - Version bits
    uint8_t       ecc)			// I - Error correcting code
{
  uint8_t size = modules->bitOffsetOrWidth;
					// Width of bitmap


  // Draw the horizontal and vertical timing patterns
  for (uint8_t i = 0; i < size; i ++)
  {
    setFunctionModule(modules, isFunction, 6, i, i % 2 == 0);
    setFunctionModule(modules, isFunction, i, 6, i % 2 == 0);
  }

  // Draw 3 finder patterns (all corners except bottom right; overwrites some timing modules)
  drawFinderPattern(modules, isFunction, 3, 3);
  drawFinderPattern(modules, isFunction, size - 4, 3);
  drawFinderPattern(modules, isFunction, 3, size - 4);

  if (version > 1)
  {
    // Draw the numerous alignment patterns
    uint8_t alignCount = version / 7 + 2;
    uint8_t step;
    if (version != 32)
      step = (version * 4 + alignCount * 2 + 1) / (2 * alignCount - 2) * 2;  // ceil((size - 13) / (2*numAlign - 2)) * 2
    else				// C-C-C-Combo breaker!
      step = 26;

    uint8_t alignPositionIndex = alignCount - 1;
    uint8_t alignPosition[alignCount];

    alignPosition[0] = 6;

    uint8_t size = version * 4 + 17;
    for (uint8_t i = 0, pos = size - 7; i < alignCount - 1; i ++, pos -= step)
      alignPosition[alignPositionIndex --] = pos;

    for (uint8_t i = 0; i < alignCount; i ++)
    {
      for (uint8_t j = 0; j < alignCount; j ++)
      {
	if ((i == 0 && j == 0) || (i == 0 && j == alignCount - 1) || (i == alignCount - 1 && j == 0))
          continue;  // Skip the three finder corners

	drawAlignmentPattern(modules, isFunction, alignPosition[i], alignPosition[j]);
      }
    }
  }

  // Draw configuration data
  drawFormatBits(modules, isFunction, ecc, 0);
					// Dummy mask value; overwritten later in the constructor

  drawVersion(modules, isFunction, version);
}


//
// 'drawCodewords()' - Draw the given codewords.
//
// This function draws the given sequence of 8-bit codewords (data and error
// correction) onto the entire data area of this QR Code symbol.  Function
// modules need to be marked off before this is called.
//

static void
drawCodewords(
    _pappl_qrbb_t *modules,		// I - Bitmap container
    _pappl_qrbb_t *isFunction,		// I - Pattern
    _pappl_qrbb_t *codewords)		// I - Codewords
{
  uint32_t bitLength = codewords->bitOffsetOrWidth;
					// Length of codewords
  uint8_t *data = codewords->data;	// Codeword data
  uint8_t size = modules->bitOffsetOrWidth;
					// Width of bitmap
  uint32_t i = 0;			// Bit index into the data


  // Do the funny zigzag scan
  for (int16_t right = size - 1; right >= 1; right -= 2)
  {
    // Index of right column in each column pair
    if (right == 6)
      right = 5;

    for (uint8_t vert = 0; vert < size; vert ++)
    {
      // Vertical counter
      for (int j = 0; j < 2; j ++)
      {
	uint8_t	x = right - j;		// Actual x coordinate
	bool	upwards = ((right & 2) == 0) ^ (x < 6);
	uint8_t	y = upwards ? size - 1 - vert : vert;
					// Actual y coordinate

	if (!_papplQRBBGetBit(isFunction, x, y) && i < bitLength)
	{
	  _papplQRBBSetBit(modules, x, y, (data[i / 8] & (128 >> (i & 7))) != 0);
	  i ++;
	}

	// If there are any remainder bits (0 to 7), they are already
	// set to 0/false/white when the grid of modules was initialized
      }
    }
  }
}


#define _PAPPL_QRPENALTY_N1      3
#define _PAPPL_QRPENALTY_N2      3
#define _PAPPL_QRPENALTY_N3     40
#define _PAPPL_QRPENALTY_N4     10

//
// 'getPenaltyScore()' - Calculate the penalty score.
//
// This function calculates and returns the penalty score based on state of this
// QR Code's current modules.  This is used by the automatic mask choice
// algorithm to find the mask pattern that yields the lowest score.
//
// @TODO: This can be optimized by working with the bytes instead of bits.
//

static uint32_t				// O - Score
getPenaltyScore(_pappl_qrbb_t *modules)	// I - Bitmap
{
  uint32_t	result = 0;		// Score
  uint8_t	size = modules->bitOffsetOrWidth;
					// Width of bitmap


  // Adjacent modules in row having same color
  for (uint8_t y = 0; y < size; y ++)
  {
    bool colorX = _papplQRBBGetBit(modules, 0, y);
					// Current color

    for (uint8_t x = 1, runX = 1; x < size; x ++)
    {
      bool cx = _papplQRBBGetBit(modules, x, y);
					// This color

      if (cx != colorX)
      {
        // Start a new run...
	colorX = cx;
	runX   = 1;
      }
      else
      {
        // Continue a run...
	runX ++;

	if (runX == 5)
	  result += _PAPPL_QRPENALTY_N1;
	else if (runX > 5)
	  result ++;
      }
    }
  }

  // Adjacent modules in column having same color
  for (uint8_t x = 0; x < size; x ++)
  {
    bool colorY = _papplQRBBGetBit(modules, x, 0);
					// Current color

    for (uint8_t y = 1, runY = 1; y < size; y ++)
    {
      bool cy = _papplQRBBGetBit(modules, x, y);
					// This color

      if (cy != colorY)
      {
        // Start a new run...
	colorY = cy;
	runY = 1;
      }
      else
      {
        // Continue a run...
	runY ++;

	if (runY == 5)
	  result += _PAPPL_QRPENALTY_N1;
	else if (runY > 5)
	  result ++;
      }
    }
  }

  uint16_t black = 0;			// Number of black blocks

  for (uint8_t y = 0; y < size; y ++)
  {
    uint16_t bitsRow = 0, bitsCol = 0;	// Bits in a row/column

    for (uint8_t x = 0; x < size; x ++)
    {
      bool color = _papplQRBBGetBit(modules, x, y);
					// Current color

      // 2*2 blocks of modules having same color
      if (x > 0 && y > 0)
      {
	bool colorUL = _papplQRBBGetBit(modules, x - 1, y - 1);
	bool colorUR = _papplQRBBGetBit(modules, x, y - 1);
	bool colorL = _papplQRBBGetBit(modules, x - 1, y);
					// Colors in the corners

	if (color == colorUL && color == colorUR && color == colorL)
	  result += _PAPPL_QRPENALTY_N2;
      }

      // Finder-like pattern in rows and columns
      bitsRow = ((bitsRow << 1) & 0x7FF) | color;
      bitsCol = ((bitsCol << 1) & 0x7FF) | _papplQRBBGetBit(modules, y, x);

      // Needs 11 bits accumulated
      if (x >= 10)
      {
	if (bitsRow == 0x05D || bitsRow == 0x5D0)
	  result += _PAPPL_QRPENALTY_N3;

	if (bitsCol == 0x05D || bitsCol == 0x5D0)
	  result += _PAPPL_QRPENALTY_N3;
      }

      // Balance of black and white modules
      if (color)
        black ++;
    }
  }

  // Find smallest k such that (45-5k)% <= dark/total <= (55+5k)%
  uint16_t total = size * size;		// Number of pixels

  for (uint16_t k = 0; (black * 20) < ((9 - k) * total) || (black * 20) > ((11 + k) * total); k ++)
    result += _PAPPL_QRPENALTY_N4;

  // Return the score...
  return (result);
}


//
// 'rs_multiply()' - Multiply two numbers.
//

static uint8_t				// O - Result
rs_multiply(uint8_t x,			// I - First number
            uint8_t y)			// I - Second number
{
  // Russian peasant multiplication
  // See: https://en.wikipedia.org/wiki/Ancient_Egyptian_multiplication
  uint16_t z = 0;
  for (int8_t i = 7; i >= 0; i --)
  {
    z = (z << 1) ^ ((z >> 7) * 0x11D);
    z ^= ((y >> i) & 1) * x;
  }

  return (z);
}


//
// 'rs_init()' - Initialize a coefficient array.
//

static void
rs_init(uint8_t degree,			// I - Number of elements in array
        uint8_t *coeff)			// I - Coefficient array
{
  memset(coeff, 0, degree);
  coeff[degree - 1] = 1;

  // Compute the product polynomial (x - r^0) * (x - r^1) * (x - r^2) * ... * (x - r^{degree-1}),
  // drop the highest term, and store the rest of the coefficients in order of descending powers.
  // Note that r = 0x02, which is a generator element of this field GF(2^8/0x11D).
  uint16_t root = 1;
  for (uint8_t i = 0; i < degree; i ++)
  {
    // Multiply the current product by (x - r^i)
    for (uint8_t j = 0; j < degree; j ++)
    {
      coeff[j] = rs_multiply(coeff[j], root);

      if (j + 1 < degree)
	coeff[j] ^= coeff[j + 1];
    }

    root = (root << 1) ^ ((root >> 7) * 0x11D);  // Multiply by 0x02 mod GF(2^8/0x11D)
  }
}


//
// '()' - .
//

static void
rs_getRemainder(uint8_t degree,
                uint8_t *coeff,
                uint8_t *data,
                uint8_t length,
                uint8_t *result,
                uint8_t stride)
{
  // Compute the remainder by performing polynomial division
  for (uint8_t i = 0; i < length; i ++)
  {
    uint8_t factor = data[i] ^ result[0];

    for (uint8_t j = 1; j < degree; j ++)
      result[(j - 1) * stride] = result[j * stride];
    result[(degree - 1) * stride] = 0;

    for (uint8_t j = 0; j < degree; j ++)
      result[j * stride] ^= rs_multiply(coeff[j], factor);
  }
}


//
// '()' -

static int8_t
encodeDataCodewords(
    _pappl_qrbb_t *dataCodewords,
    const uint8_t *text,
    uint16_t      length,
    uint8_t       version)
{
  _papplQRBBAppendBits(dataCodewords, 1 << _PAPPL_QRMODE_BYTE, 4);
  _papplQRBBAppendBits(dataCodewords, length, version < 10 ? 8 : 16);
  for (uint16_t i = 0; i < length; i ++)
    _papplQRBBAppendBits(dataCodewords, (char)(text[i]), 8);

  return (_PAPPL_QRMODE_BYTE);
}


static void
performErrorCorrection(
    uint8_t       version,
    uint8_t       ecc,
    _pappl_qrbb_t *data)
{
  // See: http://www.thonky.com/qr-code-tutorial/structure-final-message
  uint8_t numBlocks = NUM_ERROR_CORRECTION_BLOCKS[ecc][version - 1];
  uint16_t totalEcc = NUM_ERROR_CORRECTION_CODEWORDS[ecc][version - 1];
  uint16_t moduleCount = NUM_RAW_DATA_MODULES[version - 1];
  uint8_t blockEccLen = totalEcc / numBlocks;
  uint8_t numShortBlocks = numBlocks - moduleCount / 8 % numBlocks;
  uint8_t shortBlockLen = moduleCount / 8 / numBlocks;
  uint8_t shortDataBlockLen = shortBlockLen - blockEccLen;
  uint8_t result[data->capacityBytes];

  memset(result, 0, sizeof(result));

  uint8_t coeff[blockEccLen];
  rs_init(blockEccLen, coeff);

  uint16_t offset = 0;
  uint8_t *dataBytes = data->data;

  // Interleave all short blocks
  for (uint8_t i = 0; i < shortDataBlockLen; i ++)
  {
    uint16_t index = i;
    uint8_t stride = shortDataBlockLen;

    for (uint8_t blockNum = 0; blockNum < numBlocks; blockNum ++)
    {
      result[offset++] = dataBytes[index];

      if (blockNum == numShortBlocks)
        stride ++;

      index += stride;
    }
  }

  // Version less than 5 only have short blocks
  {
    // Interleave long blocks
    uint16_t index = shortDataBlockLen * (numShortBlocks + 1);
    uint8_t stride = shortDataBlockLen;
    for (uint8_t blockNum = 0; blockNum < numBlocks - numShortBlocks; blockNum ++)
    {
      result[offset ++] = dataBytes[index];

      if (blockNum == 0)
        stride ++;

      index += stride;
    }
  }

  // Add all ecc blocks, interleaved
  uint8_t blockSize = shortDataBlockLen;
  for (uint8_t blockNum = 0; blockNum < numBlocks; blockNum ++)
  {
    if (blockNum == numShortBlocks)
      blockSize ++;

    rs_getRemainder(blockEccLen, coeff, dataBytes, blockSize, &result[offset + blockNum], numBlocks);
    dataBytes += blockSize;
  }

  memcpy(data->data, result, data->capacityBytes);
  data->bitOffsetOrWidth = moduleCount;
}


// We store the Format bits tightly packed into a single byte (each of the 4 modes is 2 bits)
// The format bits can be determined by _PAPPL_QRECC_FORMAT_BITS >> (2 * ecc)
static const uint8_t _PAPPL_QRECC_FORMAT_BITS = (0x02 << 6) | (0x03 << 4) | (0x00 << 2) | (0x01 << 0);


//
// '()' - .
//

uint16_t				// I - Size in bytes
_papplQRCodeGetBufferSize(
    uint8_t version)			// I - Version
{
  return (_papplQRBBGetGridSizeBytes(4 * version + 17));
}


//
// '()' - .
//

int8_t
_papplQRCodeInitBytes(
    _pappl_qrcode_t *qrcode,
    uint8_t         *modules,
    uint8_t         version,
    uint8_t         ecc,
    uint8_t         *data,
    uint16_t        length)
{
  static uint16_t maxlength[40][4] =
  {
    // Max bytes for each ECC and VERSION
    {   17,   14,   11,    7 },
    {   32,   26,   20,   14 },
    {   53,   42,   32,   24 },
    {   78,   62,   46,   34 },
    {  106,   84,   60,   44 },
    {  134,  106,   74,   58 },
    {  154,  122,   86,   64 },
    {  192,  152,  108,   84 },
    {  230,  180,  130,   98 },
    {  271,  213,  151,  119 },
    {  321,  251,  177,  137 },
    {  367,  287,  203,  155 },
    {  425,  331,  241,  177 },
    {  458,  362,  258,  194 },
    {  520,  412,  292,  220 },
    {  586,  450,  322,  250 },
    {  644,  504,  364,  280 },
    {  718,  560,  394,  310 },
    {  792,  624,  442,  338 },
    {  858,  666,  482,  382 },
    {  929,  711,  509,  403 },
    { 1003,  779,  565,  439 },
    { 1091,  857,  611,  461 },
    { 1171,  911,  661,  511 },
    { 1273,  997,  715,  535 },
    { 1367, 1059,  751,  593 },
    { 1465, 1125,  805,  625 },
    { 1528, 1190,  868,  658 },
    { 1628, 1264,  908,  698 },
    { 1732, 1370,  982,  742 },
    { 1840, 1452, 1030,  790 },
    { 1952, 1538, 1112,  842 },
    { 2068, 1628, 1168,  898 },
    { 2188, 1722, 1228,  958 },
    { 2303, 1809, 1283,  983 },
    { 2431, 1911, 1351, 1051 },
    { 2563, 1989, 1423, 1093 },
    { 2699, 2099, 1499, 1139 },
    { 2809, 2213, 1579, 1219 },
    { 2953, 2331, 1663, 1273 }
  };


  if (ecc < _PAPPL_QRECC_LOW || ecc > _PAPPL_QRECC_HIGH)
    return (-1);

  uint8_t eccFormatBits = (_PAPPL_QRECC_FORMAT_BITS >> (2 * ecc)) & 0x03;

  if (version == _PAPPL_QRVERSION_AUTO)
  {
    for (version = _PAPPL_QRVERSION_MIN; version <= _PAPPL_QRVERSION_MAX; version ++) {
      if (maxlength[version - 1][ecc] >= length)
        break;
    }

    if (version > _PAPPL_QRVERSION_MAX)
      return (-1);
  }
  else if (version < _PAPPL_QRVERSION_MIN || version > _PAPPL_QRVERSION_MAX)
  {
    return (-1);
  }

  uint16_t moduleCount = NUM_RAW_DATA_MODULES[version - 1];
  uint16_t dataCapacity = moduleCount / 8 - NUM_ERROR_CORRECTION_CODEWORDS[eccFormatBits][version - 1];

  if (length > maxlength[version - 1][ecc])
    return (-1);

  uint8_t size = version * 4 + 17;
  qrcode->version = version;
  qrcode->size = size;
  qrcode->ecc = ecc;
  qrcode->modules = modules;

  _pappl_qrbb_t codewords;
  uint8_t codewordBytes[_papplQRBBGetBufferSizeBytes(moduleCount)];
  _papplQRBBInitBuffer(&codewords, codewordBytes, (int32_t)sizeof(codewordBytes));

  // Place the data code words into the buffer
  if (encodeDataCodewords(&codewords, data, length, version) < 0)
    return (-1);

  qrcode->mode = _PAPPL_QRMODE_BYTE;

  // Add terminator and pad up to a byte if applicable
  uint32_t padding = (dataCapacity * 8) - codewords.bitOffsetOrWidth;
  if (padding > 4)
    padding = 4;

  _papplQRBBAppendBits(&codewords, 0, padding);
  _papplQRBBAppendBits(&codewords, 0, (8 - codewords.bitOffsetOrWidth % 8) % 8);

  // Pad with alternate bytes until data capacity is reached
  for (uint8_t padByte = 0xEC; codewords.bitOffsetOrWidth < (dataCapacity * 8); padByte ^= 0xEC ^ 0x11)
    _papplQRBBAppendBits(&codewords, padByte, 8);

  _pappl_qrbb_t modulesGrid;
  _papplQRBBInitGrid(&modulesGrid, modules, size);

  _pappl_qrbb_t isFunctionGrid;
  uint8_t isFunctionGridBytes[_papplQRBBGetGridSizeBytes(size)];
  _papplQRBBInitGrid(&isFunctionGrid, isFunctionGridBytes, size);

  // Draw function patterns, draw all codewords, do masking
  drawFunctionPatterns(&modulesGrid, &isFunctionGrid, version, eccFormatBits);
  performErrorCorrection(version, eccFormatBits, &codewords);
  drawCodewords(&modulesGrid, &isFunctionGrid, &codewords);

  // Find the best (lowest penalty) mask
  uint8_t mask = 0;
  int32_t minPenalty = INT32_MAX;
  for (uint8_t i = 0; i < 8; i ++)
  {
    drawFormatBits(&modulesGrid, &isFunctionGrid, eccFormatBits, i);
    applyMask(&modulesGrid, &isFunctionGrid, i);
    int penalty = getPenaltyScore(&modulesGrid);
    if (penalty < minPenalty)
    {
      mask = i;
      minPenalty = penalty;
    }
    applyMask(&modulesGrid, &isFunctionGrid, i);  // Undoes the mask due to XOR
  }

  qrcode->mask = mask;

  // Overwrite old format bits
  drawFormatBits(&modulesGrid, &isFunctionGrid, eccFormatBits, mask);

  // Apply the final choice of mask
  applyMask(&modulesGrid, &isFunctionGrid, mask);

  return (0);
}


//
// '()' - .
//

int8_t
_papplQRCodeInitText(
    _pappl_qrcode_t *qrcode,
    uint8_t         *modules,
    uint8_t         version,
    uint8_t         ecc,
    const char      *data)
{
  size_t length = strlen(data);

  if (length > 65535)
    return (-1);

  return (_papplQRCodeInitBytes(qrcode, modules, version, ecc, (uint8_t *)data, (uint16_t)length));
}


//
// '()' - .
//

bool
_papplQRCodeGetModule(
    _pappl_qrcode_t *qrcode,
    uint8_t         x,
    uint8_t         y)
{
  if (x >= qrcode->size || y >= qrcode->size)
    return (false);

  uint32_t offset = y * qrcode->size + x;

  return ((qrcode->modules[offset / 8] & (128 >> (offset & 7))) != 0);
}
