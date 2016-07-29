// --------------------------------------------------------------------
// Rendering fonts onto the canvas
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

#include "ipefonts.h"
#include "ipepdfparser.h"
#include "ipexml.h"

#include <string>

#include <ft2build.h>
#include FT_FREETYPE_H

#include <cairo-ft.h>

using namespace ipe;

// --------------------------------------------------------------------

struct Engine {
public:
  Engine();
  ~Engine();
  cairo_font_face_t *screenFont();

private:
  bool iScreenFontLoaded;
  cairo_font_face_t *iScreenFont;

public:
  bool iOk;
  FT_Library iLib;
  int iFacesLoaded;
  int iFacesUnloaded;
  int iFacesDiscarded;
};

// Auto-constructed and destructed Freetype engine.
static Engine engine;

// --------------------------------------------------------------------

Engine::Engine()
{
  iOk = false;
  iScreenFont = 0;
  iScreenFontLoaded = false;
  iFacesLoaded = 0;
  iFacesUnloaded = 0;
  iFacesDiscarded = 0;
  if (FT_Init_FreeType(&iLib))
    return;
  iOk = true;
}

Engine::~Engine()
{
  ipeDebug("Freetype engine: %d faces loaded, %d faces unloaded, "
	   "%d faces discarded",
	   iFacesLoaded, iFacesUnloaded, iFacesDiscarded);
  if (iScreenFont)
    cairo_font_face_destroy(iScreenFont);
  if (iOk)
    FT_Done_FreeType(iLib);
  // causes an assert in cairo to fail:
  // cairo_debug_reset_static_data();
  ipeDebug("Freetype engine done: %d faces discarded", iFacesDiscarded);
}

// --------------------------------------------------------------------

cairo_font_face_t *Engine::screenFont()
{
  if (!iScreenFontLoaded) {
    iScreenFontLoaded = true;
    iScreenFont = cairo_toy_font_face_create("Sans", CAIRO_FONT_SLANT_NORMAL,
					     CAIRO_FONT_WEIGHT_BOLD);
  }
  return iScreenFont;
}

// --------------------------------------------------------------------

/*! \class ipe::Fonts
  \ingroup cairo
  \brief Provides the fonts used to render text.
*/

Fonts *Fonts::New(const PdfResources *resources)
{
  if (!engine.iOk)
    return nullptr;
  return new Fonts(resources);
}

//! Private constructor
Fonts::Fonts(const PdfResources *resources) : iResources(resources)
{
  // nothing
}

//! Delete all the loaded Faces.
Fonts::~Fonts()
{
  for (auto &it : iFaces)
    delete it;
}

String Fonts::freetypeVersion()
{
  int major, minor, patch;
  FT_Library_Version(engine.iLib, &major, &minor, &patch);
  char buf[128];
  sprintf(buf, "Freetype %d.%d.%d / %d.%d.%d",
	  FREETYPE_MAJOR, FREETYPE_MINOR, FREETYPE_PATCH,
	  major, minor, patch);
  return String(buf);
}

//! Return a Cairo font to render to the screen w/o Latex font.
cairo_font_face_t *Fonts::screenFont()
{
  return engine.screenFont();
}

//! Get a typeface.
/*! Corresponds to a Freetype "face", or a PDF font resource.  A Face
  can be loaded at various sizes (transformations), resulting in
  individual FaceSize's. */
Face *Fonts::getFace(const PdfDict *d)
{
  auto it = std::find_if(iFaces.begin(), iFaces.end(),
			 [d](const Face *f) { return f->matches(d); } );
  if (it != iFaces.end())
    return *it;

  Face *face = new Face(d, iResources);
  iFaces.push_back(face);
  return face;
}

// --------------------------------------------------------------------

struct FaceData {
  Buffer iData;
  FT_Face iFace;
};

static void face_data_destroy(FaceData *face_data)
{
  ++engine.iFacesDiscarded;
  FT_Done_Face(face_data->iFace);  // discard Freetype face
  delete face_data;  // discard memory buffer
}

static const cairo_user_data_key_t datakey = { 0 };

/*! \class ipe::Face
  \ingroup cairo
  \brief A typeface (aka font), actually loaded (from a font file or PDF file).
*/

Face::Face(const PdfDict *d, const PdfResources *resources) noexcept
:  iFontDict(d), iResources(resources)
{
  /* d:
    /Type /Font
    /Subtype /Type1
    /Encoding 24 0 R
    /FirstChar 6
    /LastChar 49
    /Widths 25 0 R
    /BaseFont /YEHLEP+CMR10
    /FontDescriptor 7 0 R
  */
  const PdfObj *type = d->get("Type", nullptr);
  if (!type || !type->name() || type->name()->value() != "Font")
    return;
  type = d->get("Subtype", nullptr);
  if (!type || !type->name())
    return;
  String t = type->name()->value();
  const PdfDict *d0 = d;
  if (t == "Type0") {
    const PdfObj *desc = getPdf(d, "DescendantFonts");
    if (!desc || !desc->array())
      return;
    desc = desc->array()->obj(0, nullptr);
    if (!desc)
      return;
    if (desc->ref())
      desc = iResources->object(desc->ref()->value());
    if (!desc || !desc->dict())
      return;
    d = desc->dict();
    type = d->get("Subtype", nullptr);
    if (!type || !type->name())
      return;
    t = type->name()->value();
  }
  const PdfObj *name = getPdf(d, "BaseFont");
  if (!name || !name->name())
    return;
  iName = name->name()->value();
  ipeDebug("Font '%s' of type '%s'", iName.z(), t.z());

  if (t == "Type1")
    iType = FontType::Type1;
  else if (t == "Truetype")
    iType = FontType::Truetype;
  else if (t == "CIDFontType0")
    iType = FontType::CIDType0;
  else if (t == "CIDFontType2")
    iType = FontType::CIDType2;
  else {
    iType = FontType::Unsupported;
    return;
  }

  Buffer data;
  if (!getFontFile(d, data)) {
    ipeDebug("Failed to get font file for %s", iName.z());
    return;
  }

  if (FT_New_Memory_Face(engine.iLib, (const uchar *) data.data(),
			 data.size(), 0, &iFace))
    return;

  FaceData *face_data = new FaceData;
  face_data->iData = data;
  face_data->iFace = iFace;

  // see cairo_ft_font_face_create_for_ft_face docs,
  // it explains why the user_data is necessary
  iCairoFont = cairo_ft_font_face_create_for_ft_face(iFace, 0);
  cairo_status_t status =
    cairo_font_face_set_user_data(iCairoFont, &datakey, face_data,
				  (cairo_destroy_func_t) face_data_destroy);
  if (status) {
    ipeDebug("Failed to set user data for Cairo font");
    cairo_font_face_destroy(iCairoFont);
    FT_Done_Face(iFace);
    delete face_data;
    iCairoFont = nullptr;
    iFace = nullptr;
    return;
  }
  ++engine.iFacesLoaded;

  if (iType == FontType::CIDType0 || iType == FontType::CIDType2) {
    getCIDWidth(d);
    const PdfObj *enc = getPdf(d0, "Encoding");
    if (!enc || !enc->name())
      return;
    String encoding = enc->name()->value();
    if (encoding != "Identity-H")
      ipeDebug("Unsupported encoding: %s", encoding.z());
    // ipeDebug("FT Face has %d charmaps, is cid-keyed: %d",
    // iFace->num_charmaps, FT_IS_CID_KEYED(iFace));
  } else {
    getSimpleWidth(d);
    if (iType == FontType::Type1)
      getType1Encoding(d);
    else
      setupTruetypeEncoding();
  }
}

Face::~Face() noexcept
{
  if (iCairoFont) {
    ipeDebug("Done with Cairo face %s (%d references left)", iName.z(),
	     cairo_font_face_get_reference_count(iCairoFont));
    ++engine.iFacesUnloaded;
    cairo_font_face_destroy(iCairoFont);
  }
}

const PdfObj *Face::getPdf(const PdfDict *d, String key) const noexcept
{
  return iResources->getDeep(d, key);
}

// --------------------------------------------------------------------

int Face::width(int ch) const noexcept
{
  uint i = 0;
  while (i < iWidth.size()) {
    if (iWidth[i] <= ch && ch <= iWidth[i+1]) {
      // found interval
      if (iWidth[i+2] < 0)
	return -iWidth[i+1] - 1;
      return iWidth[i + 2 + (ch - iWidth[i])];
    } else {
      if (iWidth[i+2] < 0)
	i += 3;
      else
	i += 3 + (iWidth[i+1] - iWidth[i]);
    }
  }
  return iDefaultWidth;
}

int Face::glyphIndex(int ch) noexcept
{
  if (!iCairoFont)
    return 0;
  switch (iType) {
  case FontType::Type1:
    return iEncoding[ch];
  case FontType::Truetype:
    return FT_Get_Char_Index(iFace, (FT_ULong) ch);
  case FontType::CIDType0:
  case FontType::CIDType2:
    return ch; // for cid-keyed font, this is a cid
  default:
    return 0;
  }
}

// --------------------------------------------------------------------

void Face::getCIDWidth(const PdfDict *d) noexcept
{
  const PdfObj *dw = getPdf(d, "DW");
  const PdfObj *w = getPdf(d, "W");
  if (!dw || !dw->number())
    return;
  iDefaultWidth = int(dw->number()->value());
  if (!w || !w->array())
    return;
  int i = 0;
  while (i + 1 < w->array()->count()) {
    const PdfObj *obj = w->array()->obj(i, nullptr);
    if (!obj->number())
      return;
    int beg = int(obj->number()->value());
    obj = w->array()->obj(i+1, nullptr);
    if (obj->number()) {
      int fin = int(obj->number()->value());
      if (i+2 == w->array()->count())
	return;
      obj = w->array()->obj(i+2, nullptr);
      if (!obj || !obj->number())
	return;
      iWidth.push_back(beg);
      iWidth.push_back(fin);
      iWidth.push_back(-int(obj->number()->value()) - 1);
      i += 3;
    } else if (obj->array()) {
      int fin = beg + obj->array()->count() - 1;
      iWidth.push_back(beg);
      iWidth.push_back(fin);
      for (int j = 0; j < obj->array()->count(); ++j) {
	const PdfObj *val = obj->array()->obj(j, nullptr);
	iWidth.push_back(val->number() ? int(val->number()->value())
			    : 1000);
      }
      i += 2;
    } else
      return;
  }
}

void Face::getSimpleWidth(const PdfDict *d) noexcept
{
  const PdfObj *fc = getPdf(d, "FirstChar");
  const PdfObj *wid = getPdf(d, "Widths");
  if (!fc || !fc->number() || !wid || !wid->array())
    return;
  int firstChar = int(fc->number()->value());
  iWidth.push_back(firstChar);
  iWidth.push_back(firstChar + wid->array()->count() - 1);
  for (int i = 0; i < wid->array()->count(); ++i) {
    const PdfObj *obj = wid->array()->obj(i, nullptr);
    iWidth.push_back(obj->number() ? int(obj->number()->value()) : 0);
  }
  ipeDebug("Got %d widths entries", iWidth.size());
}

// --------------------------------------------------------------------

void Face::getType1Encoding(const PdfDict *d) noexcept
{
  const PdfObj *enc = getPdf(d, "Encoding");
  const PdfArray *darr = nullptr;
  if (enc && enc->dict()) {
    const PdfObj *diff = getPdf(enc->dict(), "Differences");
    if (diff && diff->array())
      darr = diff->array();
  }
  if (darr) { // have an encoding as expected
    String name[0x100];
    for (int i = 0; i < 0x100; ++i)
      name[i] = ".notdef";
    int idx = 0;
    for (int i = 0; i < darr->count(); ++i) {
      const PdfObj *obj = darr->obj(i, nullptr);
      if (obj->number())
	idx = int(obj->number()->value());
      else if (obj->name() && idx < 0x100)
	name[idx++] = obj->name()->value();
    }
    for (int i = 0; i < 0x100; ++i) {
      int glyph = FT_Get_Name_Index(iFace, const_cast<char *>(name[i].z()));
      iEncoding.push_back(glyph);
    }
    ipeDebug("Got %d encoding entries", iEncoding.size());
  } else {
    // font has no encoding
    for (int k = 0; k < iFace->num_charmaps; ++k) {
      if (iFace->charmaps[k]->encoding == FT_ENCODING_ADOBE_CUSTOM) {
	FT_Set_Charmap(iFace, iFace->charmaps[k]);
	break;
      }
    }
    // Do we need to check FT_Has_PS_Glyph_Names?
    for (int i = 0; i < 0x100; ++i)
      iEncoding.push_back(FT_Get_Char_Index(iFace, i));
  }
}

void Face::setupTruetypeEncoding() noexcept
{
  FT_Set_Charmap(iFace, iFace->charmaps[0]);
  if (iFace->charmaps[0]->platform_id != 1 ||
      iFace->charmaps[0]->encoding_id != 0) {
    ipeDebug("TrueType face %s has strange first charmap (of %d)",
	     iName.z(), iFace->num_charmaps);
    for (int i = 0; i < iFace->num_charmaps; ++i) {
      ipeDebug("Map %d has platform %d, encoding %d",
	       i, iFace->charmaps[i]->platform_id,
	       iFace->charmaps[i]->encoding_id);
    }
  }
}

bool Face::getFontFile(const PdfDict *d, Buffer &data) noexcept
{
  const PdfObj *fontDescriptor = getPdf(d, "FontDescriptor");
  if (!fontDescriptor || !fontDescriptor->dict())
    return false;
  const PdfDict *fd = fontDescriptor->dict();
  const PdfObj *fontFile = getPdf(fd, "FontFile");
  if (!fontFile)
    fontFile = getPdf(fd, "FontFile2");
  if (!fontFile)
    fontFile = getPdf(fd, "FontFile3");
  if (!fontFile || !fontFile->dict() || fontFile->dict()->stream().size() == 0)
    return false;
  data = fontFile->dict()->inflate();
  // Fix strange header in some pdftex fonts that will cause EPS
  // export to break.
  size_t m = data.size() > 1024 ? 1024 : data.size();
  std::string s { (const char *) data.data(), m };
  size_t i = s.find("FontDirectory");
  if (i != std::string::npos) {
    size_t j = s.find("{save true}{false}ifelse}{false}ifelse", i);
    if (j != std::string::npos)
      memset(data.data() + i, ' ', j - i + 38);
  }
  return true;
}

// --------------------------------------------------------------------
