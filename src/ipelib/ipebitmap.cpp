// --------------------------------------------------------------------
// Bitmaps
// --------------------------------------------------------------------
/*

    This file is part of the extensible drawing editor Ipe.
    Copyright (C) 1993-2016  Otfried Cheong

    Ipe is free software; you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.

    As a special exception, you have permission to link Ipe with the
    CGAL library and distribute executables, as long as you follow the
    requirements of the Gnu General Public License in regard to all of
    the software in the executable aside from CGAL.

    Ipe is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
    or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
    License for more details.

    You should have received a copy of the GNU General Public License
    along with Ipe; if not, you can find it at
    "http://www.gnu.org/copyleft/gpl.html", or write to the Free
    Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#include "ipebitmap.h"
#include "ipeutils.h"
#include <zlib.h>

using namespace ipe;

extern bool dctDecode(Buffer dctData, Buffer pixelData, int components);

// --------------------------------------------------------------------

/*! \class ipe::Bitmap::MRenderData
  \ingroup base
  \brief Abstract base class for pixmap data stored by a client.
*/

Bitmap::MRenderData::~MRenderData()
{
  // nothing
}

/*! \class ipe::Bitmap
  \ingroup base
  \brief A bitmap.

  Bitmaps are explicitely shared using reference-counting.  Copying is
  cheap, so Bitmap objects are meant to be passed by value.

  The bitmap can cache data to speed up rendering. This data can be
  set only once (as the bitmap is conceptually immutable).

  The bitmap also provides a slot for short-term storage of an "object
  number".  The PDF embedder, for instance, sets it to the PDF object
  number when embedding the bitmap, and can reuse it when "drawing"
  the bitmap.
*/

//! Default constructor constructs null bitmap.
Bitmap::Bitmap()
{
  iImp = 0;
}

//! Create from XML stream.
Bitmap::Bitmap(const XmlAttributes &attr, String data)
{
  int length = init(attr);
  // decode data
  iImp->iData = Buffer(length);
  char *p = iImp->iData.data();
  if (attr["encoding"] ==  "base64") {
    Buffer dbuffer(data.data(), data.size());
    BufferSource source(dbuffer);
    Base64Source b64source(source);
    while (length-- > 0)
      *p++ = b64source.getChar();
  } else {
    Lex datalex(data);
    while (length-- > 0)
      *p++ = char(datalex.getHexByte());
  }
  computeChecksum();
}

//! Create from XML using external raw data
Bitmap::Bitmap(const XmlAttributes &attr, Buffer data)
{
  int length = init(attr);
  assert(length == data.size());
  iImp->iData = data;
  computeChecksum();
}

int Bitmap::init(const XmlAttributes &attr)
{
  iImp = new Imp;
  iImp->iRefCount = 1;
  iImp->iObjNum = Lex(attr["id"]).getInt();
  iImp->iRender = 0;
  iImp->iWidth = Lex(attr["width"]).getInt();
  iImp->iHeight = Lex(attr["height"]).getInt();
  iImp->iColorKey = -1;
  int length = Lex(attr["length"]).getInt();
  assert(iImp->iWidth > 0 && iImp->iHeight > 0);
  String cs = attr["ColorSpace"];
  if (cs == "DeviceGray") {
    iImp->iComponents = 1;
    iImp->iColorSpace = EDeviceGray;
  } else if (cs == "DeviceCMYK") {
    iImp->iComponents = 4;
    iImp->iColorSpace = EDeviceCMYK;
  } else {
    iImp->iComponents = 3;
    iImp->iColorSpace = EDeviceRGB;
  }
  String cc;
  if (iImp->iColorSpace == EDeviceRGB && attr.has("ColorKey", cc)) {
    iImp->iColorKey = Lex(cc).getHexNumber();
  }
  String fi = attr["Filter"];
  if (fi == "DCTDecode")
    iImp->iFilter = EDCTDecode;
  else if (fi == "FlateDecode")
    iImp->iFilter = EFlateDecode;
  else
    iImp->iFilter = EDirect;
  iImp->iBitsPerComponent = Lex(attr["BitsPerComponent"]).getInt();
  if (length == 0) {
    assert(iImp->iFilter == EDirect);
    int bitsPerRow = width() * components() * bitsPerComponent();
    int bytesPerRow = (bitsPerRow + 7) / 8;
    length = height() * bytesPerRow;
  }
  return length;
}

//! Create a new image
/*! \a filter specifies the format of the \a data.
  If \a filter is EDirect, then setting \a deflate
  compresses the bitmap and changes the filter to EFlateDecode.
 */
Bitmap::Bitmap(int width, int height,
	       TColorSpace colorSpace, int bitsPerComponent,
	       Buffer data, TFilter filter, bool deflate)
{
  iImp = new Imp;
  iImp->iRefCount = 1;
  iImp->iObjNum = -1;
  iImp->iRender = 0;
  iImp->iWidth = width;
  iImp->iHeight = height;
  iImp->iColorKey = -1;
  assert(iImp->iWidth > 0 && iImp->iHeight > 0);
  iImp->iColorSpace = colorSpace;
  switch (colorSpace) {
  case EDeviceGray:
    iImp->iComponents = 1;
    break;
  case EDeviceRGB:
    iImp->iComponents = 3;
    break;
  case EDeviceCMYK:
    iImp->iComponents = 4;
    break;
  }
  iImp->iFilter = filter;
  iImp->iBitsPerComponent = bitsPerComponent;
  if (deflate && filter == EDirect) {
    // optional deflation
    int deflatedSize;
    Buffer deflated = DeflateStream::deflate(data.data(), data.size(),
					     deflatedSize, 9);
    iImp->iData = Buffer(deflated.data(), deflatedSize);
    iImp->iFilter = EFlateDecode;
  } else
    iImp->iData = data;
  computeChecksum();
}

//! Copy constructor.
/*! Since Bitmaps are reference counted, this is very fast. */
Bitmap::Bitmap(const Bitmap &rhs)
{
  iImp = rhs.iImp;
  if (iImp)
    iImp->iRefCount++;
}

//! Destructor.
Bitmap::~Bitmap()
{
  if (iImp && --iImp->iRefCount == 0) {
    delete iImp->iRender;
    delete iImp;
  }
}

//! Assignment operator (takes care of reference counting).
/*! Very fast. */
Bitmap &Bitmap::operator=(const Bitmap &rhs)
{
  if (this != &rhs) {
    if (iImp && --iImp->iRefCount == 0)
      delete iImp;
    iImp = rhs.iImp;
    if (iImp)
      iImp->iRefCount++;
  }
  return *this;
}

//! Return rgb representation of the transparent color.
/*! Returns -1 if the bitmap is not color keyed. */
int Bitmap::colorKey() const
{
  return iImp->iColorKey;
}

//! Set transparent color.
/*! Use \a key == -1 to disable color key. */
void Bitmap::setColorKey(int key)
{
  iImp->iColorKey = key;
}

//! Save bitmap in XML stream.
void Bitmap::saveAsXml(Stream &stream, int id, int pdfObjNum) const
{
  assert(iImp);
  stream << "<bitmap";
  stream << " id=\"" << id << "\"";
  stream << " width=\"" << width() << "\"";
  stream << " height=\"" << height() << "\"";
  stream << " length=\"" << size() << "\"";
  switch (colorSpace()) {
  case EDeviceGray:
    stream << " ColorSpace=\"DeviceGray\"";
    break;
  case EDeviceRGB:
    stream << " ColorSpace=\"DeviceRGB\"";
    break;
  case EDeviceCMYK:
    stream << " ColorSpace=\"DeviceCMYK\"";
    break;
  }
  switch (filter()) {
  case EFlateDecode:
    stream << " Filter=\"FlateDecode\"";
    break;
  case EDCTDecode:
    stream << " Filter=\"DCTDecode\"";
    break;
  default:
    // no filter
    break;
  }
  stream << " BitsPerComponent=\"" << bitsPerComponent() << "\"";

  if (iImp->iColorKey >= 0) {
    char buf[10];
    sprintf(buf, "%x", iImp->iColorKey);
    stream << " ColorKey=\"" << buf << "\"";
  }

  if (pdfObjNum >= 0) {
    stream << " pdfObject=\"" << pdfObjNum << "\"/>\n";
  } else {
    // save data
    stream << " encoding=\"base64\">\n";
    const char *p = data();
    const char *fin = p + size();
    Base64Stream b64(stream);
    while (p != fin)
      b64.putChar(*p++);
    b64.close();
    stream << "</bitmap>\n";
  }
}

//! Set a cached bitmap for fast rendering.
void Bitmap::setRenderData(MRenderData *data) const
{
  assert(iImp && iImp->iRender == 0);
  iImp->iRender = data;
}

bool Bitmap::equal(Bitmap rhs) const
{
  if (iImp == rhs.iImp)
    return true;
  if (!iImp || !rhs.iImp)
    return false;

  if (iImp->iColorSpace != rhs.iImp->iColorSpace ||
      iImp->iBitsPerComponent != rhs.iImp->iBitsPerComponent ||
      iImp->iWidth != rhs.iImp->iWidth ||
      iImp->iHeight != rhs.iImp->iHeight ||
      iImp->iComponents != rhs.iImp->iComponents ||
      iImp->iColorKey != rhs.iImp->iColorKey ||
      iImp->iFilter != rhs.iImp->iFilter ||
      iImp->iChecksum != rhs.iImp->iChecksum ||
      iImp->iData.size() != rhs.iImp->iData.size())
    return false;
  // check actual data
  int len = iImp->iData.size();
  char *p = iImp->iData.data();
  char *q = rhs.iImp->iData.data();
  while (len--) {
    if (*p++ != *q++)
      return false;
  }
  return true;
}

void Bitmap::computeChecksum()
{
  int s = 0;
  int len = iImp->iData.size();
  char *p = iImp->iData.data();
  while (len--) {
    s = (s & 0x0fffffff) << 3;
    s += *p++;
  }
  iImp->iChecksum = s;
}

// --------------------------------------------------------------------

//! Convert bitmap data to a height x width pixel array in rgb format.
/*! Returns empty buffer if it cannot decode the bitmap information.
  Otherwise, returns a buffer of size width() * height() uint32_t's. */
Buffer Bitmap::pixelData() const
{
  ipeDebug("pixelData %d x %d x %d, %d", width(), height(),
	   components(), int(filter()));
  if (bitsPerComponent() != 8)
    return Buffer();
  Buffer stream = iImp->iData;
  Buffer pixels;
  if (filter() == EDirect) {
    pixels = stream;
  } else if (filter() == EFlateDecode) {
    // inflate data
    uLong inflatedSize = width() * height() * components();
    pixels = Buffer(inflatedSize);
    if (uncompress((Bytef *) pixels.data(), &inflatedSize,
		   (const Bytef *) stream.data(), stream.size()) != Z_OK
	|| pixels.size() != int(inflatedSize))
      return Buffer();
  } else if (filter() == EDCTDecode) {
    pixels = Buffer(width() * height() * components());
    if (!dctDecode(stream, pixels, components()))
      return Buffer();
  }
  Buffer data(height() * width() * sizeof(uint32_t));
  // convert pixels to data
  const char *p = pixels.data();
  uint32_t *q = (uint *) data.data();
  if (components() == 3) {
    uint colorKey = (iImp->iColorKey | 0xff000000);
    if (iImp->iColorKey < 0)
      colorKey = 0;
    for (int y = 0; y < iImp->iHeight; ++y) {
      for (int x = 0; x < iImp->iWidth; ++x) {
	uchar r = uchar(*p++);
	uchar g = uchar(*p++);
	uchar b = uchar(*p++);
	uint32_t pixel = 0xff000000 | (r << 16) | (g << 8) | b;
	if (pixel == colorKey)
	  *q++ = 0;
	else
	  *q++ = pixel;
      }
    }
  } else if (components() == 1) {
    for (int y = 0; y < iImp->iHeight; ++y) {
      for (int x = 0; x < iImp->iWidth; ++x) {
	uchar r = uchar(*p++);
	*q++ = 0xff000000 | (r << 16) | (r << 8) | r;
      }
    }
  }
  return data;
}

// --------------------------------------------------------------------

/*
 JPG reading code
 Copyright (c) 1996-2002 Han The Thanh, <thanh@pdftex.org>

 This code is part of pdfTeX.

 pdfTeX is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.
*/

#define JPG_GRAY  1     /* Gray color space, use /DeviceGray  */
#define JPG_RGB   3     /* RGB color space, use /DeviceRGB    */
#define JPG_CMYK  4     /* CMYK color space, use /DeviceCMYK  */

enum JPEG_MARKER {      /* JPEG marker codes                    */
  M_SOF0  = 0xc0,       /* baseline DCT                         */
  M_SOF1  = 0xc1,       /* extended sequential DCT              */
  M_SOF2  = 0xc2,       /* progressive DCT                      */
  M_SOF3  = 0xc3,       /* lossless (sequential)                */

  M_SOF5  = 0xc5,       /* differential sequential DCT          */
  M_SOF6  = 0xc6,       /* differential progressive DCT         */
  M_SOF7  = 0xc7,       /* differential lossless                */

  M_JPG   = 0xc8,       /* JPEG extensions                      */
  M_SOF9  = 0xc9,       /* extended sequential DCT              */
  M_SOF10 = 0xca,       /* progressive DCT                      */
  M_SOF11 = 0xcb,       /* lossless (sequential)                */

  M_SOF13 = 0xcd,       /* differential sequential DCT          */
  M_SOF14 = 0xce,       /* differential progressive DCT         */
  M_SOF15 = 0xcf,       /* differential lossless                */

  M_DHT   = 0xc4,       /* define Huffman tables                */

  M_DAC   = 0xcc,       /* define arithmetic conditioning table */

  M_RST0  = 0xd0,       /* restart                              */
  M_RST1  = 0xd1,       /* restart                              */
  M_RST2  = 0xd2,       /* restart                              */
  M_RST3  = 0xd3,       /* restart                              */
  M_RST4  = 0xd4,       /* restart                              */
  M_RST5  = 0xd5,       /* restart                              */
  M_RST6  = 0xd6,       /* restart                              */
  M_RST7  = 0xd7,       /* restart                              */

  M_SOI   = 0xd8,       /* start of image                       */
  M_EOI   = 0xd9,       /* end of image                         */
  M_SOS   = 0xda,       /* start of scan                        */
  M_DQT   = 0xdb,       /* define quantization tables           */
  M_DNL   = 0xdc,       /* define number of lines               */
  M_DRI   = 0xdd,       /* define restart interval              */
  M_DHP   = 0xde,       /* define hierarchical progression      */
  M_EXP   = 0xdf,       /* expand reference image(s)            */

  M_APP0  = 0xe0,       /* application marker, used for JFIF    */
  M_APP1  = 0xe1,       /* application marker                   */
  M_APP2  = 0xe2,       /* application marker                   */
  M_APP3  = 0xe3,       /* application marker                   */
  M_APP4  = 0xe4,       /* application marker                   */
  M_APP5  = 0xe5,       /* application marker                   */
  M_APP6  = 0xe6,       /* application marker                   */
  M_APP7  = 0xe7,       /* application marker                   */
  M_APP8  = 0xe8,       /* application marker                   */
  M_APP9  = 0xe9,       /* application marker                   */
  M_APP10 = 0xea,       /* application marker                   */
  M_APP11 = 0xeb,       /* application marker                   */
  M_APP12 = 0xec,       /* application marker                   */
  M_APP13 = 0xed,       /* application marker                   */
  M_APP14 = 0xee,       /* application marker, used by Adobe    */
  M_APP15 = 0xef,       /* application marker                   */

  M_JPG0  = 0xf0,       /* reserved for JPEG extensions         */
  M_JPG13 = 0xfd,       /* reserved for JPEG extensions         */
  M_COM   = 0xfe,       /* comment                              */

  M_TEM   = 0x01,       /* temporary use                        */
};

inline int read2bytes(FILE *f)
{
  uchar c1 = fgetc(f);
  uchar c2 = fgetc(f);
  return (c1 << 8) + c2;
}

// --------------------------------------------------------------------

//! Read information about JPEG image from file.
/*! Returns NULL on success, an error message otherwise. */
const char *Bitmap::readJpegInfo(FILE *file, int &width, int &height,
				 Vector &dotsPerInch,
				 TColorSpace &colorSpace,
				 int &bitsPerComponent)
{
  static char jpg_id[] = "JFIF";
  bool app0_seen = false;

  dotsPerInch = Vector(0, 0);

  if (read2bytes(file) != 0xFFD8) {
    return "The file does not appear to be a JPEG image";
  }

  for (;;) {
    int ch = fgetc(file);
    if (ch != 0xff)
      return "Reading JPEG image failed";
    do {
      ch = fgetc(file);
    } while (ch == 0xff);
    ipeDebug("JPEG tag %x", ch & 0xff);
    int fpos = ftell(file);
    switch (ch & 0xff) {

    case M_SOF5:
    case M_SOF6:
    case M_SOF7:
    case M_SOF9:
    case M_SOF10:
    case M_SOF11:
    case M_SOF13:
    case M_SOF14:
    case M_SOF15:
      return "Unsupported type of JPEG compression";

    case M_SOF0:
    case M_SOF1:
    case M_SOF2: // progressive DCT allowed since PDF-1.3
    case M_SOF3:
      read2bytes(file);    /* read segment length  */
      ch = fgetc(file);
      bitsPerComponent = ch;
      height = read2bytes(file);
      width = read2bytes(file);
      ch = fgetc(file);
      switch (ch & 0xff) {
      case JPG_GRAY:
	colorSpace = Bitmap::EDeviceGray;
        break;
      case JPG_RGB:
	colorSpace = Bitmap::EDeviceRGB;
        break;
      case JPG_CMYK:
	colorSpace = Bitmap::EDeviceCMYK;
	break;
      default:
	return "Unsupported color space in JPEG image";
      }
      fseek(file, 0, SEEK_SET);
      return nullptr;      //  success!

    case M_APP0: {
      int len = read2bytes(file);
      if (app0_seen) {
	fseek(file, fpos + len, SEEK_SET);
	break;
      }
      for (int i = 0; i < 5; i++) {
	ch = fgetc(file);
	if (ch != jpg_id[i]) {
	  return "Reading JPEG image failed";
	}
      }
      read2bytes(file); // JFIF version
      char units = fgetc(file);
      int xres = read2bytes(file);
      int yres = read2bytes(file);
      if (xres != 0 && yres != 0) {
	switch (units) {
	case 1: /* pixels per inch */
	  dotsPerInch = Vector(xres, yres);
	  break;
	case 2: /* pixels per cm */
	  dotsPerInch = Vector(xres * 2.54, yres * 2.54);
	  break;
	default: // 0: aspect ratio only
	  break;
	}
      }
      app0_seen = true;
      fseek(file, fpos + len, SEEK_SET);
      break; }

    case M_SOI:      // ignore markers without parameters
    case M_EOI:
    case M_TEM:
    case M_RST0:
    case M_RST1:
    case M_RST2:
    case M_RST3:
    case M_RST4:
    case M_RST5:
    case M_RST6:
    case M_RST7:
      break;

    default:         // skip variable length markers
	fseek(file, fpos + read2bytes(file), SEEK_SET);
      break;
    }
  }
}

//! Read JPEG image from file.
/*! Returns the image as a DCT-encoded Bitmap.
  Sets \a dotsPerInch if the image file contains a resolution,
  otherwise sets it to (0,0).
  If reading the file fails, returns a null Bitmap,
  and sets the error message \a errmsg.
*/
Bitmap Bitmap::readJpeg(const char *fname, Vector &dotsPerInch,
			const char * &errmsg)
{
  FILE *file = Platform::fopen(fname, "rb");
  if (!file) {
    errmsg = "Error opening file";
    return Bitmap();
  }

  int width, height, bitsPerComponent;
  TColorSpace colorSpace;

  errmsg = Bitmap::readJpegInfo(file, width, height, dotsPerInch,
				colorSpace, bitsPerComponent);
  fclose(file);
  if (errmsg)
    return Bitmap();

  String a = Platform::readFile(fname);
  return  Bitmap(width, height, colorSpace,
		 bitsPerComponent, Buffer(a.data(), a.size()),
		 Bitmap::EDCTDecode);
}

// --------------------------------------------------------------------
