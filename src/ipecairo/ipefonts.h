// -*- C++ -*-
// --------------------------------------------------------------------
// CanvasFonts maintains the Freetype fonts for the canvas
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

#ifndef IPEFONTS_H
#define IPEFONTS_H

#include "ipebase.h"
#include "ipegeo.h"
#include "iperesources.h"

#include <cairo.h>

//------------------------------------------------------------------------

struct FT_FaceRec_;

namespace ipe {

  class PdfResources;
  class PdfDict;

  enum class FontType { Type1, Truetype, CIDType0, CIDType2, Unsupported };

  class Face {
  public:
    Face(const PdfDict *d, const PdfResources *resources) noexcept;
    ~Face() noexcept;
    inline bool matches(const PdfDict *d) const noexcept {
      return d == iFontDict; }
    inline FontType type() const noexcept { return iType; }
    int width(int ch) const noexcept;
    int glyphIndex(int ch) noexcept;
    inline cairo_font_face_t *cairoFont() noexcept { return iCairoFont; }

  private:
    const PdfObj *getPdf(const PdfDict *d, String key) const noexcept;
    bool getFontFile(const PdfDict *d, Buffer &data) noexcept;
    void getSimpleWidth(const PdfDict *d) noexcept;
    void getType1Encoding(const PdfDict *d) noexcept;
    void setupTruetypeEncoding() noexcept;
    void getCIDWidth(const PdfDict *d) noexcept;

  private:
    const PdfDict *iFontDict;
    const PdfResources *iResources;
    FontType iType;
    String iName;
    cairo_font_face_t *iCairoFont { nullptr };
    FT_FaceRec_ *iFace { nullptr };
    std::vector<int> iEncoding;
    std::vector<int> iWidth;
    int iDefaultWidth { 1000 };
  };

  class Fonts {
  public:
    static Fonts *New(const PdfResources *resources);
    ~Fonts();
    Face *getFace(const PdfDict *d);
    static cairo_font_face_t *screenFont();
    static String freetypeVersion();
    const PdfResources *resources() const noexcept { return iResources; }

  private:
    Fonts(const PdfResources *resources);

  private:
    const PdfResources *iResources;

    typedef std::list<Face *> FaceSeq;
    FaceSeq iFaces;
  };

} // namespace

//------------------------------------------------------------------------
#endif
