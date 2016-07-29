// ipebitmap_win.cpp
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

#include <windows.h>
#include <gdiplus.h>
#include <shlwapi.h>

using namespace ipe;

// --------------------------------------------------------------------

typedef IStream* WINAPI (*LPFNSHCREATEMEMSTREAM)(const BYTE *, UINT);
static bool libLoaded = false;
static LPFNSHCREATEMEMSTREAM pSHCreateMemStream = 0;

static void flipRB(char *p, int n)
{
  const char *fin = p + n;
  char t;
  while (p < fin) {
    t = p[0];
    p[0] = p[2];
    p[2] = t;
    p += 3;
  }
}

bool dctDecode(Buffer dctData, Buffer pixelData, int components)
{
  if (!libLoaded) {
    libLoaded = true;
    HMODULE hDll = LoadLibraryA("shlwapi.dll");
    if (hDll)
      pSHCreateMemStream =
	(LPFNSHCREATEMEMSTREAM) GetProcAddress(hDll, (LPCSTR) 12);
  }
  if (!pSHCreateMemStream)
    return false;

  IStream *stream = pSHCreateMemStream((const BYTE *) dctData.data(),
				       dctData.size());
  if (!stream)
    return false;

  Gdiplus::Bitmap *bitmap = Gdiplus::Bitmap::FromStream(stream);
  if (bitmap->GetLastStatus() != Gdiplus::Ok) {
    delete bitmap;
    stream->Release();
    return false;
  }

  int w = bitmap->GetWidth();
  int h = bitmap->GetHeight();
  ipeDebug("dctDecode: %d x %d", w, h);

  Gdiplus::BitmapData* bitmapData = new Gdiplus::BitmapData;
  bitmapData->Scan0 = pixelData.data();
  bitmapData->Stride = w * 3;
  bitmapData->Width = w;
  bitmapData->Height = h;
  bitmapData->PixelFormat = PixelFormat24bppRGB;

  Gdiplus::Rect rect(0, 0, w, h);
  bitmap->LockBits(&rect,
		   Gdiplus::ImageLockModeRead |
		   Gdiplus::ImageLockModeUserInputBuf,
		   PixelFormat24bppRGB, bitmapData);
  bitmap->UnlockBits(bitmapData);
  delete bitmapData;
  delete bitmap;
  stream->Release();

  flipRB(pixelData.data(), 3 * w * h);
  return true;
}

// --------------------------------------------------------------------

static bool isGrayscale(Gdiplus::BitmapData *d)
{
  const char *p = (const char *) d->Scan0;
  for (uint h = 0; h < d->Height; ++h) {
    const char *q = p;
    char r, g, b;
    const char *fin = p + 4 * d->Width;
    while (q < fin) {
      b = *q++; g = *q++; r = *q++; ++q;
      if (b != g || b != r)
	return false;
    }
    p = p + d->Stride;
  }
  return true;
}

static int findColorKey(Gdiplus::BitmapData *d)
{
  int colorFound = -1;
  const uchar *p = (const uchar *) d->Scan0;
  for (uint h = 0; h < d->Height; ++h) {
    const uchar *q = p;
    uchar r, g, b, trans;
    const uchar *fin = p + 4 * d->Width;
    while (q < fin) {
      b = *q++; g = *q++; r = *q++; trans = *q++;
      if (trans != 0 && trans != 0xff)
	return -1;
      int color = (r << 16) | (g << 8) | b;
      if (trans == 0) { // transparent color found
	if (colorFound < 0)
	  colorFound = color;
	else if (colorFound != color)  // two different transparent colors
	  return -1;
      } else if (colorFound == color)  // opaque color found
	return -1;
    }
    p = p + d->Stride;
  }
  return colorFound;
}

static void copyPixels(Gdiplus::BitmapData *d, char *t, bool gray)
{
  const char *p = (const char *) d->Scan0;
  if (gray) {
    for (uint h = 0; h < d->Height; ++h) {
      const char *q = p;
      const char *fin = p + 4 * d->Width;
      while (q < fin) {
	*t++ = *q;
	q += 4;
      }
      p = p + d->Stride;
    }
  } else {
    for (uint h = 0; h < d->Height; ++h) {
      const char *q = p;
      const char *fin = p + 4 * d->Width;
      while (q < fin) {
	*t++ = q[2];
	*t++ = q[1];
	*t++ = q[0];
	q += 4;
      }
      p = p + d->Stride;
    }
  }
}


// The graphics file formats supported by GDI+ are
// BMP, GIF, JPEG, PNG, TIFF.
Bitmap Bitmap::readPNG(const char *fname, bool deflate,
		       Vector &dotsPerInch, const char * &errmsg)
{
  std::vector<wchar_t> wname;
  Platform::utf8ToWide(fname, wname);
  // load without color correction
  Gdiplus::Bitmap *bitmap = Gdiplus::Bitmap::FromFile(&wname[0], FALSE);
  if (bitmap->GetLastStatus() != Gdiplus::Ok) {
    delete bitmap;
    return Bitmap();
  }

  dotsPerInch = Vector(bitmap->GetHorizontalResolution(),
		       bitmap->GetVerticalResolution());

  int w = bitmap->GetWidth();
  int h = bitmap->GetHeight();

  Gdiplus::BitmapData* bitmapData = new Gdiplus::BitmapData;
  Gdiplus::Rect rect(0, 0, w, h);
  bitmap->LockBits(&rect, Gdiplus::ImageLockModeRead,
		   PixelFormat32bppARGB, bitmapData);

  bool gray = isGrayscale(bitmapData);
  int colorKey = findColorKey(bitmapData);

  TColorSpace cs = gray ? EDeviceGray : EDeviceRGB;
  int samples = gray ? 1 : 3;

  ipeDebug("gray: %d, %d x %d, %x", gray, w, h, colorKey);

  Buffer pixels(w * h * samples);
  copyPixels(bitmapData, pixels.data(), gray);

  Bitmap bm(w, h, cs, 8, pixels, Bitmap::EDirect, deflate);

  if (colorKey >= 0)
    bm.setColorKey(colorKey);

  bitmap->UnlockBits(bitmapData);
  delete bitmapData;
  delete bitmap;
  return bm;
}

// --------------------------------------------------------------------
