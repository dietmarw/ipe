// --------------------------------------------------------------------
// IpeImlParser
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

#include "ipeiml.h"
#include "ipeobject.h"
#include "ipegroup.h"
#include "ipepage.h"
#include "ipefactory.h"
#include "ipestyle.h"
#include "ipereference.h"

using namespace ipe;

// --------------------------------------------------------------------

/*! \class ipe::ImlParser
  \ingroup high
  \brief XML Parser for Ipe documents and style sheets.

  A recursive descent parser for the XML streams.

  After experimenting with various XML parsing frameworks, this turned
  out to work best for Ipe.

*/

ImlParser::ImlParser(DataSource &source)
  : XmlParser(source)
{
  // nothing
}

//! XML contents can refer to data in PDF.
/*! If the XML stream is embedded in a PDF file, XML contents can
  refer to PDF objects.  A derived parser must implement this method
  to access PDF data.

  It is assumed that PDF object \a objNum is a stream.  Its contents
  (uncompressed!) is returned in a buffer.
*/
Buffer ImlParser::pdfStream(int /* objNum */)
{
  return Buffer();
}

//! Read a complete  document from IML stream.
/*! Returns an error code. */
int ImlParser::parseDocument(Document &doc)
{
  Document::SProperties properties = doc.properties();

  String tag = parseToTag();
  if (tag == "?xml") {
    XmlAttributes attr;
    if (!parseAttributes(attr, true))
      return ESyntaxError;
    tag = parseToTag();
  }
  if (tag != "ipe")
    return ESyntaxError;

  XmlAttributes attr;
  if (!parseAttributes(attr))
    return ESyntaxError;

  Lex versLex(attr["version"]);
  int version;
  versLex >> version;
  if (version < OLDEST_FILE_FORMAT)
    return EVersionTooOld;
  if (version > IPELIB_VERSION)
    return EVersionTooRecent;

  attr.has("creator", properties.iCreator);

  tag = parseToTag();
  if (tag == "info") {
    XmlAttributes att;
    if (!parseAttributes(att))
      return ESyntaxError;

    properties.iTitle = att["title"];
    properties.iAuthor = att["author"];
    properties.iSubject = att["subject"];
    properties.iKeywords = att["keywords"];
    properties.iFullScreen = (att["pagemode"] == "fullscreen");
    properties.iNumberPages = (att["numberpages"] == "yes");
    properties.iCreated = att["created"];
    properties.iModified = att["modified"];
    String tex = att["tex"];
    if (tex == "pdftex")
      properties.iTexEngine = LatexType::Pdftex;
    else if (tex == "xetex")
      properties.iTexEngine = LatexType::Xetex;
    else if (tex == "luatex")
      properties.iTexEngine = LatexType::Luatex;

    tag = parseToTag();
  }
  if (tag == "preamble") {
    XmlAttributes att;
    if (!parseAttributes(att))
      return ESyntaxError;
    if (!parsePCDATA("preamble", properties.iPreamble))
      return ESyntaxError;

    tag = parseToTag();
  }

  // document created by default constructor already has standard stylesheet
  Cascade *cascade = doc.cascade();

  while (tag == "ipestyle" || tag == "bitmap") {
    if (tag == "ipestyle") {
      StyleSheet *sheet = new StyleSheet();
      if (!parseStyle(*sheet)) {
	delete sheet;
	return ESyntaxError;
      }
      cascade->insert(0, sheet);
    } else { // tag == "bitmap"
      if (!parseBitmap())
	return ESyntaxError;
    }
    tag = parseToTag();
  }

  while (tag == "page") {
    // read one page
    Page *page = new Page;
    doc.push_back(page);
    if (!parsePage(*page))
      return ESyntaxError;
    tag = parseToTag();
  }

  doc.setProperties(properties);
  if (tag != "/ipe")
    return ESyntaxError;
  return ESuccess;
}

//! Parse an Bitmap.
/*! On calling, stream must be just past \c bitmap. */
bool ImlParser::parseBitmap()
{
  XmlAttributes att;
  if (!parseAttributes(att))
    return false;
  String objNumStr;
  if (att.slash() && att.has("pdfObject", objNumStr)) {
    int objNum = Lex(objNumStr).getInt();
    // ipeDebug("ParseBitmap: objNum = %d", objNum);
    Bitmap bitmap(att, pdfStream(objNum));
    iBitmaps.push_back(bitmap);
  } else {
    String bits;
    if (!parsePCDATA("bitmap", bits))
      return false;
    Bitmap bitmap(att, bits);
    iBitmaps.push_back(bitmap);
  }
  return true;
}

//! Parse an Page.
/*! On calling, stream must be just past \c page. */
bool ImlParser::parsePage(Page &page)
{
  XmlAttributes att;
  if (!parseAttributes(att))
    return false;

  String str;
  if (att.has("title", str))
    page.setTitle(str);

  if (att.has("section", str))
    page.setSection(0, str.empty(), str);
  else
    page.setSection(0, false, String());

  if (att.has("subsection", str))
    page.setSection(1, str.empty(), str);
  else
    page.setSection(1, false, String());

  if (att["marked"] == "no")
    page.setMarked(false);

  String tag = parseToTag();

  if (tag == "notes") {
    XmlAttributes att;
    if (!parseAttributes(att))
      return ESyntaxError;
    if (!parsePCDATA("notes", str))
      return ESyntaxError;
    page.setNotes(str);

    tag = parseToTag();
  }

  while (tag == "layer") {
    XmlAttributes att;
    if (!parseAttributes(att))
      return false;
    page.addLayer(att["name"]);
    if (att["edit"] == "no")
      page.setLocked(page.countLayers() - 1, true);
    tag = parseToTag();
  }
  // default layer: 'alpha'
  if (page.countLayers() == 0)
    page.addLayer("alpha");

  while (tag == "view") {
    XmlAttributes att;
    if (!parseAttributes(att))
      return false;

    page.insertView(page.countViews(), att["active"]);
    String str;
    if (att.has("effect", str))
      page.setEffect(page.countViews() - 1, Attribute(true, str));

    Lex st(att["layers"]);
    st.skipWhitespace();
    String last;
    while (!st.eos()) {
      last = st.nextToken();
      page.setVisible(page.countViews() - 1, last, true);
      st.skipWhitespace();
    }

    if (!att.has("active", str)) {
      // if no layer visible must have active attribute
      if (last.empty())
	return false;
      page.setActive(page.countViews() - 1, last);
    }

    if (att["marked"] == "yes")
      page.setMarkedView(page.countViews() - 1, true);

    tag = parseToTag();
  }

  // default view: include all layers
  if (page.countViews() == 0) {
    int al = 0;
    while (al < page.countLayers() && page.isLocked(al))
      ++al;
    if (al == page.countLayers())
      return false; // no unlocked layer
    // need to synthesize a view
    page.insertView(0, page.layer(al));
    for (int i = 0; i < page.countLayers(); ++i)
      page.setVisible(0, page.layer(i), true);
  }

  int currentLayer = 0;
  String layerName;
  for (;;) {
    if (tag == "/page") {
      return true;
    }
    Object *obj = parseObject(tag, &page, &currentLayer);
    if (!obj)
      return false;
    page.insert(page.count(), ENotSelected, currentLayer, obj);
    tag = parseToTag();
  }
}

//! parse an \c <ipepage> element (used on clipboard).
Page *ImlParser::parsePageSelection()
{
  String tag = parseToTag();
  if (tag != "ipepage")
    return 0;
  XmlAttributes attr;
  if (!parseAttributes(attr))
    return 0;
  tag = parseToTag();

  while (tag == "bitmap") {
    if (!parseBitmap())
      return 0;
    tag = parseToTag();
  }

  if (tag != "page")
    return 0;

  std::unique_ptr<Page> page(new Page);
  if (!parsePage(*page))
    return 0;

  tag = parseToTag();
  if (tag != "/ipepage")
    return 0;
  return page.release();
}

#if 0
//! parse an \c <ipeselection> element.
/*! An PgObjectSeq is used to own the objects, but selection mode
  and layer are not set. */
bool ImlParser::parseSelection(PageObjectSeq &seq)
{
  String tag = parseToTag();
  if (tag != "ipeselection")
    return false;
  XmlAttributes attr;
  if (!parseAttributes(attr))
    return 0;
  tag = parseToTag();

  while (tag == "bitmap") {
    if (!parseBitmap())
      return false;
    tag = parseToTag();
  }

  for (;;) {
    if (tag == "/ipeselection")
      return true;
    Object *obj = parseObject(tag);
    if (!obj)
      return false;
    seq.push_back(PageObject(ENotSelected, 0, obj));
    tag = parseToTag();
  }
}
#endif

//! parse an Object.
/*! On calling, stream must be just past the tag. */
Object *ImlParser::parseObject(String tag, Page *page,
			       int *currentLayer)
{
  if (tag[0] == '/')
    return 0;

  XmlAttributes attr;
  if (!parseAttributes(attr))
    return 0;

  String layer;
  if (page && currentLayer && attr.has("layer", layer)) {
    for (int i = 0; i < page->countLayers(); ++i) {
      if (page->layer(i) == layer) {
	*currentLayer = i;
	break;
      }
    }
  }

  if (tag == "group") {
    Group group(attr);
    for (;;) {
      String tag = parseToTag();
      if (tag == "/group") {
	return new Group(group);
      }
      Object *obj = parseObject(tag);
      if (!obj)
	return 0;
      group.push_back(obj);
    }
  }

  String pcdata;
  if (!attr.slash() && !parsePCDATA(tag, pcdata))
    return 0;
  String bitmapId;
  if (tag == "image" && attr.has("bitmap", bitmapId)) {
    int objNum = Lex(bitmapId).getInt();
    Bitmap bitmap;
    for (std::vector<Bitmap>::const_iterator it = iBitmaps.begin();
	 it != iBitmaps.end(); ++it) {
      if (it->objNum() == objNum) {
	bitmap = *it;
	break;
      }
    }
    assert(!bitmap.isNull());
    return ObjectFactory::createImage(tag, attr, bitmap);
  } else
    return ObjectFactory::createObject(tag, attr, pcdata);
}

static inline bool symbolName(String s)
{
  return (!s.empty() && (('a' <= s[0] && s[0] <= 'z') ||
			 ('A' <= s[0] && s[0] <= 'Z')));
}

//! Parse a style sheet.
/*! On calling, stream must be just past the style tag. */
bool ImlParser::parseStyle(StyleSheet &sheet)
{
  XmlAttributes att;
  if (!parseAttributes(att))
    return false;
  String name;
  if (att.has("name", name))
    sheet.setName(name);

  String tag = parseToTag();
  while (tag != "/ipestyle") {
    if (tag == "bitmap") {
      if (!parseBitmap())
	return false;
    } else if (tag == "symbol") {
      if (!parseAttributes(att))
	return false;
      String tag1 = parseToTag();
      Object *obj = parseObject(tag1);
      if (!obj)
	return false;
      Symbol symbol(obj);
      String name = att["name"];
      if (!symbolName(name))
	return false;
      if (att["transformations"] == "rigid")
	symbol.iTransformations = ETransformationsRigidMotions;
      else if (att["transformations"] == "translations")
	symbol.iTransformations = ETransformationsTranslations;
      if (att["xform"] == "yes") {
	uint flags = Reference::flagsFromName(name);
	if ((flags & (Reference::EHasStroke|
		      Reference::EHasFill|
		      Reference::EHasPen|
		      Reference::EHasSize)) == 0) {
	  symbol.iXForm = true;
	  symbol.iTransformations = ETransformationsTranslations;
	}
      }
      sheet.addSymbol(Attribute(true, name), symbol);
      if (parseToTag() != "/symbol")
	return false;
    } else if (tag == "layout") {
      if (!parseAttributes(att) || !att.slash())
	return false;
      Layout layout;
      Lex lex1(att["paper"]);
      lex1 >> layout.iPaperSize.x >> layout.iPaperSize.y;
      Lex lex2(att["origin"]);
      lex2 >> layout.iOrigin.x >> layout.iOrigin.y;
      Lex lex3(att["frame"]);
      lex3 >> layout.iFrameSize.x >> layout.iFrameSize.y;
      layout.iParagraphSkip = Lex(att["skip"]).getDouble();
      layout.iCrop = !(att["crop"] == "no");
      sheet.setLayout(layout);
    } else if (tag == "textpad") {
      if (!parseAttributes(att) || !att.slash())
	return false;
      TextPadding pad;
      pad.iLeft = Lex(att["left"]).getDouble();
      pad.iRight = Lex(att["right"]).getDouble();
      pad.iTop = Lex(att["top"]).getDouble();
      pad.iBottom = Lex(att["bottom"]).getDouble();
      sheet.setTextPadding(pad);
    } else if (tag == "titlestyle") {
      if (!parseAttributes(att) || !att.slash())
	return false;
      StyleSheet::TitleStyle ts;
      ts.iDefined = true;
      Lex lex1(att["pos"]);
      lex1 >> ts.iPos.x >> ts.iPos.y;
      ts.iSize = Attribute::makeScalar(att["size"], Attribute::NORMAL());
      ts.iColor = Attribute::makeColor(att["color"], Attribute::BLACK());
      ts.iHorizontalAlignment = Text::makeHAlign(att["halign"], EAlignLeft);
      ts.iVerticalAlignment = Text::makeVAlign(att["valign"], EAlignBaseline);
      sheet.setTitleStyle(ts);
    } else if (tag == "pagenumberstyle") {
      if (!parseAttributes(att))
	return false;
      StyleSheet::PageNumberStyle pns;
      pns.iDefined = true;
      Lex lex1(att["pos"]);
      lex1 >> pns.iPos.x >> pns.iPos.y;
      pns.iSize = Attribute::makeTextSize(att["size"]);
      pns.iColor = Attribute::makeColor(att["color"], Attribute::BLACK());
      pns.iVerticalAlignment = Text::makeVAlign(att["valign"], EAlignBaseline);
      pns.iHorizontalAlignment = Text::makeHAlign(att["halign"], EAlignLeft);
      if (!att.slash() && !parsePCDATA(tag, pns.iText))
	return false;
      sheet.setPageNumberStyle(pns);
    } else if (tag == "preamble") {
      if (!parseAttributes(att))
	return false;
      String pcdata;
      if (!att.slash() && !parsePCDATA(tag, pcdata))
	return false;
      sheet.setPreamble(pcdata);
    } else if (tag == "pathstyle") {
      if (!parseAttributes(att) || !att.slash())
	return false;
      String str;
      if (att.has("cap", str))
	sheet.setLineCap(TLineCap(Lex(str).getInt() + 1));
      if (att.has("join", str))
	sheet.setLineJoin(TLineJoin(Lex(str).getInt() + 1));
      if (att.has("fillrule", str)) {
	if (str == "wind")
	  sheet.setFillRule(EWindRule);
	else if (str == "eofill")
	  sheet.setFillRule(EEvenOddRule);
      }
    } else if (tag == "color") {
      if (!parseAttributes(att) || !att.slash())
	return false;
      String name = att["name"];
      Attribute col =
	Attribute::makeColor(att["value"], Attribute::NORMAL());
      if (!symbolName(name) || !col.isColor())
	return false;
      sheet.add(EColor, Attribute(true, name), col);
    } else if (tag == "dashstyle") {
      if (!parseAttributes(att) || !att.slash())
	return false;
      String name = att["name"];
      Attribute dash = Attribute::makeDashStyle(att["value"]);
      if (!symbolName(name) || dash.isSymbolic())
	return false;
      sheet.add(EDashStyle, Attribute(true, name), dash);
    } else if (tag == "textsize") {
      if (!parseAttributes(att) || !att.slash())
	return false;
      String name = att["name"];
      Attribute value = Attribute::makeTextSize(att["value"]);
      if (!symbolName(name) || value.isSymbolic())
	return false;
      sheet.add(ETextSize, Attribute(true, name), value);
    } else if (tag == "textstretch") {
      if (!parseAttributes(att) || !att.slash())
	return false;
      String name = att["name"];
      Attribute value =
	Attribute::makeScalar(att["value"], Attribute::NORMAL());
      if (!symbolName(name) || value.isSymbolic())
	return false;
      sheet.add(ETextStretch, Attribute(true, name), value);
    } else if (tag == "gradient") {
      if (!parseAttributes(att) || att.slash())
	return false;
      String name = att["name"];
      if (!symbolName(name))
	return false;
      Gradient s;
      s.iType = (att["type"] == "radial") ?
	Gradient::ERadial : Gradient::EAxial;
      Lex lex(att["coords"]);
      if (s.iType == Gradient::ERadial)
	lex >> s.iV[0].x >> s.iV[0].y >> s.iRadius[0]
	    >> s.iV[1].x >> s.iV[1].y >> s.iRadius[1];
      else
	lex >> s.iV[0].x >> s.iV[0].y
	    >> s.iV[1].x >> s.iV[1].y;
      String str;
      s.iExtend = (att.has("extend",str) && str == "yes");
      if (att.has("matrix", str))
	s.iMatrix = Matrix(str);
      tag = parseToTag();
      while (tag == "stop") {
	if (!parseAttributes(att) || !att.slash())
	  return false;
	Gradient::Stop st;
	st.color = Color(att["color"]);
	st.offset = Lex(att["offset"]).getDouble();
	s.iStops.push_back(st);
	tag = parseToTag();
      }
      if (s.iStops.size() < 2)
	return false;
      if (s.iStops.front().offset != 0.0) {
	s.iStops.insert(s.iStops.begin(), s.iStops.front());
	s.iStops.front().offset = 0.0;
      }
      if (s.iStops.back().offset != 1.0) {
	s.iStops.push_back(s.iStops.back());
	s.iStops.back().offset = 1.0;
      }
      if (s.iStops.front().offset < 0.0 || s.iStops.front().offset > 1.0)
	return false;
      for (uint i = 1; i < s.iStops.size(); ++i) {
	if (s.iStops[i].offset < s.iStops[i-1].offset)
	  return false;
      }
      if (tag != "/gradient")
	return false;
      sheet.addGradient(Attribute(true, name), s);
    } else if (tag == "tiling") {
      if (!parseAttributes(att) || !att.slash())
	return false;
      String name = att["name"];
      if (!symbolName(name))
	return false;
      Tiling s;
      s.iAngle = Angle::Degrees(Lex(att["angle"]).getDouble());
      s.iStep = Lex(att["step"]).getDouble();
      s.iWidth = Lex(att["width"]).getDouble();
      sheet.addTiling(Attribute(true, name), s);
    } else if (tag == "effect") {
      if (!parseAttributes(att) || !att.slash())
	return false;
      String name = att["name"];
      if (!symbolName(name))
	return false;
      Effect s;
      String str;
      if (att.has("duration", str))
	s.iDuration = Lex(str).getInt();
      if (att.has("transition", str))
	s.iTransitionTime = Lex(str).getInt();
      if (att.has("effect", str))
	s.iEffect = Effect::TEffect(Lex(str).getInt());
      sheet.addEffect(Attribute(true, name), s);
    } else if (tag == "textstyle") {
      if (!parseAttributes(att) || !att.slash())
	return false;
      String name = att["name"];
      if (!symbolName(name))
	return false;
      String value = att["begin"];
      value += '\0';
      value += att["end"];
      sheet.add(ETextStyle, Attribute(true, name),
		Attribute(false, value));
    } else {
      Kind kind;
      if (tag == "pen")
	kind = EPen;
      else if (tag == "symbolsize")
	kind = ESymbolSize;
      else if (tag == "arrowsize")
	kind = EArrowSize;
      else if (tag == "gridsize")
	kind = EGridSize;
      else if (tag == "anglesize")
	kind = EAngleSize;
      else if (tag == "opacity")
	kind = EOpacity;
      else
	return false; // error
      if (!parseAttributes(att) || !att.slash())
	return false;
      String name = att["name"];
      Attribute value = Attribute::makeScalar(att["value"],
					      Attribute::NORMAL());
      if (name.empty() || value.isSymbolic())
	return false;
      sheet.add(kind, Attribute(true, name), value);
    }
    tag = parseToTag();
  }
  return true;
}

//! parse a complete style sheet.
/*! On calling, stream must be before the 'ipestyle' tag.
  A <?xml> tag is allowed.
 */
StyleSheet *ImlParser::parseStyleSheet()
{
  String tag = parseToTag();
  if (tag == "?xml") {
    XmlAttributes attr;
    if (!parseAttributes(attr, true))
      return 0;
    tag = parseToTag();
  }
  if (tag != "ipestyle")
    return 0;
  StyleSheet *sheet = new StyleSheet();
  if (parseStyle(*sheet))
    return sheet;
  delete sheet;
  return 0;
}

// --------------------------------------------------------------------
