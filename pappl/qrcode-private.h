//
// The MIT License (MIT)
//
// This library is written and maintained by Richard Moore.
// Major parts were derived from Project Nayuki's library.
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

#ifndef _PAPPL_QRCODE_PRIVATE_H_
#  define _PAPPL_QRCODE_PRIVATE_H_
#  include "base-private.h"
#  include <stdbool.h>
#  include <stdint.h>
#  include <sys/types.h>


//
// Constants...
//

// QR Code Format Encoding
#  define _PAPPL_QRMODE_BYTE           2


// Error Correction Code Levels
#  define _PAPPL_QRECC_LOW            0
#  define _PAPPL_QRECC_MEDIUM         1
#  define _PAPPL_QRECC_QUARTILE       2
#  define _PAPPL_QRECC_HIGH           3


// Version Numbers
#  define _PAPPL_QRVERSION_AUTO       0
#  define _PAPPL_QRVERSION_MIN        1
#  define _PAPPL_QRVERSION_MAX        40


//
// Types...
//

typedef struct _pappl_qrbb_s		// Bit bucket container
{
  uint32_t	bitOffsetOrWidth;	// Length of each line
  uint16_t	capacityBytes;		// Total size of data buffer
  uint8_t	*data;			// Data buffer
} _pappl_qrbb_t;

typedef struct _pappl_qrcode_s		// QR Code data
{
  uint8_t	version,		// Version
		size,			// Dimensions (SIZExSIZE)
		ecc,			// Error correction code level
		mode,			// Encoding mode (numeric, alphanumeric, and byte)
		mask,			// ???
		*modules;		// Bitmap
} _pappl_qrcode_t;


//
// Functions...
//

extern void	_papplQRBBAppendBits(_pappl_qrbb_t *bitBuffer, uint32_t val, uint8_t length) _PAPPL_INTERNAL;
extern bool	_papplQRBBGetBit(_pappl_qrbb_t *bitGrid, uint8_t x, uint8_t y) _PAPPL_INTERNAL;
extern uint16_t	_papplQRBBGetBufferSizeBytes(uint32_t bits) _PAPPL_INTERNAL;
extern uint16_t	_papplQRBBGetGridSizeBytes(uint8_t size) _PAPPL_INTERNAL;
extern void	_papplQRBBInitBuffer(_pappl_qrbb_t *bitBuffer, uint8_t *data, int32_t capacityBytes) _PAPPL_INTERNAL;
extern void	_papplQRBBInitGrid(_pappl_qrbb_t *bitGrid, uint8_t *data, uint8_t size) _PAPPL_INTERNAL;
extern void	_papplQRBBInvertBit(_pappl_qrbb_t *bitGrid, uint8_t x, uint8_t y, bool invert) _PAPPL_INTERNAL;
extern void	_papplQRBBSetBit(_pappl_qrbb_t *bitGrid, uint8_t x, uint8_t y, bool on) _PAPPL_INTERNAL;

extern uint16_t	_papplQRCodeGetBufferSize(uint8_t version) _PAPPL_INTERNAL;
extern bool	_papplQRCodeGetModule(_pappl_qrcode_t *qrcode, uint8_t x, uint8_t y) _PAPPL_INTERNAL;

extern int8_t	_papplQRCodeInitText(_pappl_qrcode_t *qrcode, uint8_t *modules, uint8_t version, uint8_t ecc, const char *data) _PAPPL_INTERNAL;
extern int8_t	_papplQRCodeInitBytes(_pappl_qrcode_t *qrcode, uint8_t *modules, uint8_t version, uint8_t ecc, uint8_t *data, uint16_t length) _PAPPL_INTERNAL;

extern char	*_papplQRCodeMakeDataURL(_pappl_qrcode_t *qrcode) _PAPPL_INTERNAL;


#endif  // _PAPPL_QRCODE_H_
