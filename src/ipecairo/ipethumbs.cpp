// --------------------------------------------------------------------
// Making thumbnails of Ipe pages
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

#include "ipethumbs.h"

#include "ipecairopainter.h"
#include <cairo.h>

#ifdef CAIRO_HAS_SVG_SURFACE
#include <cairo-svg.h>
#endif
#ifdef CAIRO_HAS_PDF_SURFACE
#include <cairo-pdf.h>
#endif
#ifdef CAIRO_HAS_PS_SURFACE
#include <cairo-ps.h>
#endif

#ifdef WIN32
#include <windows.h>
#include <gdiplus.h>
#endif

using namespace ipe;

// --------------------------------------------------------------------

Thumbnail::Thumbnail(const Document *doc, int width)
{
  iDoc = doc;
  iWidth = width;

  iLayout = iDoc->cascade()->findLayout();
  Rect paper = iLayout->paper();
  iHeight = int(iWidth * paper.height() / paper.width());
  iZoom = iWidth / paper.width();
  iFonts = Fonts::New(doc->resources());
}

Thumbnail::~Thumbnail()
{
  delete iFonts;
}

Buffer Thumbnail::render(const Page *page, int view)
{
  Buffer buffer(iWidth * iHeight * 4);
  memset(buffer.data(), 0xff, iWidth * iHeight * 4);

  cairo_surface_t* surface =
    cairo_image_surface_create_for_data((uchar *) buffer.data(),
					CAIRO_FORMAT_ARGB32,
					iWidth, iHeight, iWidth * 4);
  cairo_t *cc = cairo_create(surface);
  cairo_scale(cc, iZoom, -iZoom);
  Vector offset = iLayout->iOrigin - iLayout->paper().topLeft();
  cairo_translate(cc, offset.x, offset.y);

  CairoPainter painter(iDoc->cascade(), iFonts, cc, iZoom, true);
  painter.pushMatrix();
  for (int i = 0; i < page->count(); ++i) {
    if (page->objectVisible(view, i))
      page->object(i)->draw(painter);
  }
  painter.popMatrix();
  cairo_surface_flush(surface);
  cairo_show_page(cc);
  cairo_destroy(cc);
  cairo_surface_destroy(surface);

  return buffer;
}

#ifdef WIN32
static bool getEncoderClsid(const WCHAR* format, CLSID &clsid)
{
  uint num = 0;
  uint size = 0;
  Gdiplus::GetImageEncodersSize(&num, &size);
  if (size == 0)
    return false;

  Gdiplus::ImageCodecInfo* pImageCodecInfo =
    (Gdiplus::ImageCodecInfo *) malloc(size);
  if (pImageCodecInfo == NULL)
    return false;

  Gdiplus::GetImageEncoders(num, size, pImageCodecInfo);
  for (uint j = 0; j < num; ++j) {
    if (!wcscmp(pImageCodecInfo[j].MimeType, format)) {
      clsid = pImageCodecInfo[j].Clsid;
      free(pImageCodecInfo);
      return true;
    }
  }
  free(pImageCodecInfo);
  return false;
}

void Thumbnail::savePNG(cairo_surface_t* surface, const char *dst)
{
  int w = cairo_image_surface_get_width(surface);
  int h = cairo_image_surface_get_height(surface);
  int stride = cairo_image_surface_get_stride(surface);
  uchar *data = cairo_image_surface_get_data(surface);

  Gdiplus::Bitmap *bitmap = new Gdiplus::Bitmap(w, h, stride,
						PixelFormat32bppARGB,
						data);

  CLSID pngClsid;
  getEncoderClsid(L"image/png", pngClsid);

  std::vector<wchar_t> wname;
  ipe::Platform::utf8ToWide(dst, wname);
  bitmap->Save(&wname[0], &pngClsid, NULL);
  delete bitmap;
}

#else
void Thumbnail::savePNG(cairo_surface_t* surface, const char *dst)
{
  cairo_surface_write_to_png(surface, dst);
}
#endif

bool Thumbnail::saveRender(TargetFormat fm, const char *dst,
			   const Page *page, int view, double zoom,
			   bool transparent, bool nocrop)
{
  Rect bbox;
  int wid, ht;
  if (nocrop) {
    bbox = iLayout->paper();
    wid = int(bbox.width() * zoom);
    ht = int(bbox.height() * zoom);
  } else {
    bbox = page->pageBBox(iDoc->cascade());
    wid = int(bbox.width() * zoom + 1);
    ht = int(bbox.height() * zoom + 1);
  }

  Buffer data;
  cairo_surface_t* surface = 0;

  if (fm == EPNG) {
    if (wid * ht > 20000000)
      return false;
    data = Buffer(wid * ht * 4);
    if (transparent)
      memset(data.data(), 0x00, wid * ht * 4);
    else
      memset(data.data(), 0xff, wid * ht * 4);
    surface = cairo_image_surface_create_for_data((uchar *) data.data(),
						  CAIRO_FORMAT_ARGB32,
						  wid, ht, wid * 4);
#ifdef CAIRO_HAS_SVG_SURFACE
  } else if (fm == ESVG) {
    surface = cairo_svg_surface_create(dst, wid, ht);
#endif
#ifdef CAIRO_HAS_PS_SURFACE
  } else if (fm == EPS) {
    surface = cairo_ps_surface_create(dst, wid, ht);
    cairo_ps_surface_set_eps(surface, true);
#endif
#ifdef CAIRO_HAS_PDF_SURFACE
  } else if (fm == EPDF) {
    surface = cairo_pdf_surface_create(dst, wid, ht);
#endif
  }

  cairo_t *cc = cairo_create(surface);
  cairo_scale(cc, zoom, -zoom);
  cairo_translate(cc, -bbox.topLeft().x, -bbox.topLeft().y);

  CairoPainter painter(iDoc->cascade(), iFonts, cc, zoom, true);
  // painter.Transform(IpeLinear(zoom, 0, 0, -zoom));
  // painter.Translate(-bbox.TopLeft());
  painter.pushMatrix();

  if (nocrop) {
    const Symbol *background =
      iDoc->cascade()->findSymbol(Attribute::BACKGROUND());
    if (background && page->findLayer("BACKGROUND") < 0)
      painter.drawSymbol(Attribute::BACKGROUND());
  }

  for (int i = 0; i < page->count(); ++i) {
    if (page->objectVisible(view, i))
      page->object(i)->draw(painter);
  }

  painter.popMatrix();
  cairo_surface_flush(surface);
  cairo_show_page(cc);

  if (fm == EPNG)
    savePNG(surface, dst);

  cairo_destroy(cc);
  cairo_surface_destroy(surface);
  return true;
}

// --------------------------------------------------------------------
