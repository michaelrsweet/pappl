//
// QR Code data URL generator for the Printer Application Framework
//
// Copyright © 2025-2026 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#include "base-private.h"
#include "qrcode-private.h"
#include <zlib.h>


//
// Local constants...
//

#define QR_DATA_MAXSIZE	2048		// Maximum allowed size of "data:" URL
#define QR_DATA_PREFIX	"data:image/png;base64,"
					// "data:" URL prefix
#define QR_DATA_PREFLEN	22		// Length of "data:" URL prefix
#define QR_SCALE	4		// Nominal size of modules
#define QR_PADDING	4		// White padding around QR code
#define QR_PNG_NONE	0		// PNG "None" filter
#define QR_PNG_UP	2		// PNG "Up" filter


//
// Local functions...
//

static unsigned char *png_add_crc(unsigned char *pngdata, unsigned char *pngptr, unsigned char *pngend);
static unsigned char *png_add_unsigned(unsigned val, unsigned char *pngptr, unsigned char *pngend);
static bool	png_deflate(z_stream *z, uint8_t *line, size_t linelen);


//
// '_papplQRCodeMakeDataURL()' - Generate a "data:" URL containing a PNG image of a QR code.
//
// The returned URL must be freed using the `free` function.
//

char *					// O - "data:" URL or `NULL` on error
_papplMakeDataURL(_pappl_bb_t *qrcode)	// I - QR code bitmap
{
  char		*dataurl;		// "data:" URL
  size_t	dataurlsize,		// Size of "data:" URL
		pngbufsize;		// Size of PNG output buffer
  unsigned char	*pngbuf = NULL,		// PNG output buffer
		*pngptr,		// Pointer into PNG buffer
		*pngend,		// Pointer to end of PNG buffer
		*pngdata,		// Start of PNG chunk data
		line[1 + (QR_SCALE * (255 + 2 * QR_PADDING) + 7) / 8],
					// PNG bitmap line starting with filter byte
		*lineptr,		// Pointer into line
		bit;			// Current bit
  unsigned	size = QR_SCALE * (qrcode->width + 2 * QR_PADDING),
					// Size of image
		linelen = (size + 7) / 8,
					// Length of a line
		x,			// Current column in QR code
#if QR_SCALE != 4
		x0,			// Inner X looping var for scaling
#endif // QR_SCALE != 4
		y,			// Current line in QR code
		y0,			// Inner Y looping var for scaling
		xoff = (QR_SCALE * QR_PADDING) / 8,
		xmod = (QR_SCALE * QR_PADDING) & 7;
  z_stream	zstream;		// ZLIB compression stream


  // Allocate memory for the PNG image...
  pngbufsize = 3 * QR_DATA_MAXSIZE / 4 - QR_DATA_PREFLEN;

  if ((pngbuf = malloc(pngbufsize)) == NULL)
    return (NULL);

  pngptr = pngbuf;
  pngend = pngbuf + pngbufsize;

  // Add the PNG file header...
  *pngptr++ = 137;
  *pngptr++ = 80;
  *pngptr++ = 78;
  *pngptr++ = 71;
  *pngptr++ = 13;
  *pngptr++ = 10;
  *pngptr++ = 26;
  *pngptr++ = 10;

  // Add the IHDR chunk...
  pngptr    = png_add_unsigned(13, pngptr, pngend);
  pngdata   = pngptr;

  *pngptr++ = 'I';
  *pngptr++ = 'H';
  *pngptr++ = 'D';
  *pngptr++ = 'R';

  pngptr    = png_add_unsigned(size, pngptr, pngend);
					// Width
  pngptr    = png_add_unsigned(size, pngptr, pngend);
					// Height
  *pngptr++ = 1;			// Bit depth
  *pngptr++ = 0;			// Color type grayscale
  *pngptr++ = 0;			// Compression method 0 (deflate)
  *pngptr++ = 0;			// Filter method 0 (adaptive)
  *pngptr++ = 0;			// Interlace method 0 (no interlace)
  pngptr    = png_add_crc(pngdata, pngptr, pngend);

  // Add the IDAT chunk...
  pngptr    += 4;			// Leave room for length
  pngdata   = pngptr;

  *pngptr++ = 'I';
  *pngptr++ = 'D';
  *pngptr++ = 'A';
  *pngptr++ = 'T';

  // Initialize zlib compressor...
  memset(&zstream, 0, sizeof(zstream));
  if (deflateInit(&zstream, /*level*/9) < Z_OK)
  {
    free(pngbuf);
    return (NULL);
  }

  zstream.next_out  = (Bytef *)pngptr;
  zstream.avail_out = (uInt)(pngend - pngptr);

  // Add padding at the top...
  line[0] = QR_PNG_NONE;
  memset(line + 1, 0xff, linelen);

  if (!png_deflate(&zstream, line, linelen + 1))
    goto error;

  line[0] = QR_PNG_UP;
  memset(line + 1, 0, linelen);

  for (y = 1; y < (QR_SCALE * QR_PADDING); y ++)
  {
    if (!png_deflate(&zstream, line, linelen + 1))
      goto error;
  }

  // Add lines from the QR code...
  for (y = 0; y < qrcode->width; y ++)
  {
    // Scale the code horizontally to the current line
    memset(line + 1, 0xff, linelen);

#if QR_SCALE == 4
    bit = 0xf0 >> xmod;			// Optimize 4x scaling
#else
    bit = 128 >> xmod;
#endif // QR_SCALE == 4

    for (x = 0, lineptr = line + 1 + xoff; x < qrcode->width; x ++)
    {
      bool qrset = _papplBBGetBit(qrcode, (uint8_t)x, (uint8_t)y);

      // Repeat the module QR_SCALE times horizontally...
#if QR_SCALE == 4
      if (qrset)
        *lineptr ^= bit;

      if (bit == 0x0f)
      {
        lineptr ++;
        bit = 0xf0;
      }
      else
      {
        bit = 0x0f;
      }
#else
      for (x0 = 0; x0 < QR_SCALE; x0 ++)
      {
	if (qrset)
	  *lineptr ^= bit;

	if (bit == 1)
	{
	  lineptr ++;
	  bit = 128;
	}
	else
	{
	  bit = bit / 2;
	}
      }
#endif // QR_SCALE == 4
    }

    // Write the line QR_SCALE times...
    line[0] = QR_PNG_NONE;
    if (!png_deflate(&zstream, line, linelen + 1))
      goto error;

    line[0] = QR_PNG_UP;
    memset(line + 1, 0, linelen);

    for (y0 = 1; y0 < QR_SCALE; y0 ++)
    {
      if (!png_deflate(&zstream, line, linelen + 1))
	goto error;
    }
  }

  // Add padding at the bottom...
  line[0] = QR_PNG_NONE;
  memset(line + 1, 0xff, linelen);
  if (!png_deflate(&zstream, line, linelen + 1))
    goto error;

  for (y = 1; y < (QR_SCALE * QR_PADDING); y ++)
  {
    if (!png_deflate(&zstream, line, linelen + 1))
      goto error;
  }

  // Finish compression...
  zstream.next_in  = (Bytef *)line;
  zstream.avail_in = 0;

  if (deflate(&zstream, Z_FINISH) != Z_STREAM_END)
  {
    deflateEnd(&zstream);
    goto error;
  }

  // Add the length word and CRC...
  pngptr = (unsigned char *)zstream.next_out;

  if ((pngptr + 16) >= pngend)
    goto error;				// Too large

  png_add_unsigned((unsigned)(pngptr - pngdata - 4), pngdata - 4, pngend);
  pngptr = png_add_crc(pngdata, pngptr, pngend);

  // Add the IEND chunk...
  pngptr  = png_add_unsigned(0, pngptr, pngend);
  pngdata = pngptr;

  *pngptr++ = 'I';
  *pngptr++ = 'E';
  *pngptr++ = 'N';
  *pngptr++ = 'D';

  pngptr    = png_add_crc(pngdata, pngptr, pngend);

  // Now generate a "data:" URL of the form "data:image/png;base64,..."
  dataurlsize = QR_DATA_PREFLEN + 4 * (size_t)(pngptr - pngbuf + 2) / 3 + 1;
  if ((dataurl = calloc(1, dataurlsize)) == NULL)
    goto error;

  cupsCopyString(dataurl, QR_DATA_PREFIX, dataurlsize);
  httpEncode64(dataurl + QR_DATA_PREFLEN, dataurlsize - QR_DATA_PREFLEN, (char *)pngbuf, (size_t)(pngptr - pngbuf), /*url*/false);

  free(pngbuf);

  return (dataurl);


  // If we get here something bad happened...
  error:

  free(pngbuf);

  return (NULL);
}


//
// 'png_add_crc()' - Compute and append the chunk data CRC.
//

static unsigned char *			// O - Next byte in output buffer
png_add_crc(unsigned char *pngdata,	// I - Pointer to start of chunk data
            unsigned char *pngptr,	// I - Pointer to end of chunk data (where CRC goes)
            unsigned char *pngend)	// I - Pointer to end of PNG output buffer
{
  unsigned long		c;		// CRC value


  c = crc32(0, Z_NULL, 0);
  c = crc32(c, pngdata, (uInt)(pngptr - pngdata));

  // Append the CRC to the buffer...
  return (png_add_unsigned((unsigned)c, pngptr, pngend));
}


//
// 'png_add_unsigned()' - Add a 32-bit unsigned integer to the PNG buffer.
//

static unsigned char *			// O - Next byte in output buffer
png_add_unsigned(unsigned      val,	// I - Value to append
                 unsigned char *pngptr,	// I - Pointer into output buffer
                 unsigned char *pngend)	// I - Pointer to end of output buffer
{
  // Append the value to the buffer...
  if (pngptr < pngend)
    *pngptr++ = (val >> 24) & 0xff;
  if (pngptr < pngend)
    *pngptr++ = (val >> 16) & 0xff;
  if (pngptr < pngend)
    *pngptr++ = (val >> 8) & 0xff;
  if (pngptr < pngend)
    *pngptr++ = val & 0xff;

  return (pngptr);
}


//
// 'png_deflate()' - Compress a line in the image.
//

static bool				// O - `true` on success, `false` on error
png_deflate(z_stream *z,		// I - Deflate stream
            uint8_t  *line,		// I - Line buffer
            size_t   linelen)		// I - Line length
{
  z->next_in  = (Bytef *)line;
  z->avail_in = (uInt)linelen;

  if (deflate(z, Z_NO_FLUSH) < Z_OK)
  {
    deflateEnd(z);
    return (false);
  }

  return (true);
}
