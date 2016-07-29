// --------------------------------------------------------------------
// Creating PDF output
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

#include "ipeimage.h"
#include "ipetext.h"
#include "ipepainter.h"
#include "ipegroup.h"
#include "ipereference.h"
#include "ipeutils.h"

#include "ipepdfwriter.h"
#include "ipepdfparser.h"
#include "iperesources.h"

using namespace ipe;

typedef std::vector<Bitmap>::const_iterator BmIter;

// --------------------------------------------------------------------

PdfPainter::PdfPainter(const Cascade *style, Stream &stream)
  : Painter(style), iStream(stream)
{
  State state;
  state.iStroke = Color(0,0,0);
  state.iFill = Color(0,0,0);
  state.iPen = Fixed(1);
  state.iDashStyle = "[]0";
  state.iLineCap = style->lineCap();
  state.iLineJoin = style->lineJoin();
  state.iOpacity = Fixed(1);
  iStream << state.iLineCap - 1 << " J "
	  << state.iLineJoin - 1 << " j\n";
  iActiveState.push_back(state);
}

void PdfPainter::doNewPath()
{
  drawAttributes();
}

void PdfPainter::doMoveTo(const Vector &v)
{
  iStream << v << " m\n";
}

void PdfPainter::doLineTo(const Vector &v)
{
  iStream << v << " l\n";
}

void PdfPainter::doCurveTo(const Vector &v1, const Vector &v2,
			   const Vector &v3)
{
  iStream << v1 << " " << v2 << " " << v3 << " c\n";
}

void PdfPainter::doClosePath()
{
  iStream << "h ";
}

void PdfPainter::doAddClipPath()
{
  iStream << "W* n ";
}

void PdfPainter::doPush()
{
  State state = iActiveState.back();
  iActiveState.push_back(state);
  iStream << "q ";
}

void PdfPainter::doPop()
{
  iActiveState.pop_back();
  iStream << "Q\n";
}

void PdfPainter::drawColor(Stream &stream, Color color,
			   const char *gray, const char *rgb)
{
  if (color.isGray())
    stream << color.iRed << " " << gray << "\n";
  else
    stream << color << " " << rgb << "\n";
}

void PdfPainter::drawAttributes()
{
  State &s = iState.back();
  State &sa = iActiveState.back();
  if (s.iDashStyle != sa.iDashStyle) {
    sa.iDashStyle = s.iDashStyle;
    iStream << s.iDashStyle << " d\n";
  }
  if (s.iPen != sa.iPen) {
    sa.iPen = s.iPen;
    iStream << s.iPen << " w\n";
  }
  if (s.iLineCap != sa.iLineCap) {
    sa.iLineCap = s.iLineCap;
    iStream << s.iLineCap - 1 << " J\n";
  }
  if (s.iLineJoin != sa.iLineJoin) {
    sa.iLineJoin = s.iLineJoin;
    iStream << s.iLineJoin - 1 << " j\n";
  }
  if (s.iStroke != sa.iStroke) {
    sa.iStroke = s.iStroke;
    drawColor(iStream, s.iStroke, "G", "RG");
  }
  if (s.iFill != sa.iFill || !s.iTiling.isNormal()) {
    sa.iFill = s.iFill;
    if (!s.iTiling.isNormal()) {
      iStream << "/PCS cs\n";
      s.iFill.saveRGB(iStream);
      iStream << " /Pat" << s.iTiling.index() << " scn\n";
    } else
      drawColor(iStream, s.iFill, "g", "rg");
  }
  drawOpacity();
}

static const char *opacityName(Fixed alpha)
{
  static char buf[12];
  sprintf(buf, "/alpha%03d", alpha.internal());
  return buf;
}

void PdfPainter::drawOpacity()
{
  State &s = iState.back();
  State &sa = iActiveState.back();
  if (s.iOpacity != sa.iOpacity) {
    sa.iOpacity = s.iOpacity;
    iStream << opacityName(s.iOpacity) << " gs\n";
  }
}

// If gradient fill is set, then StrokeAndFill does NOT stroke!
void PdfPainter::doDrawPath(TPathMode mode)
{
  bool eofill = (fillRule() == EEvenOddRule);

  // if (!mode)
  // iStream << "n\n"; // no op path

  const Gradient *g = 0;
  Attribute grad = iState.back().iGradient;
  if (!grad.isNormal())
    g = iCascade->findGradient(grad);

  if (g) {
    if (mode == EStrokedOnly)
      iStream << "S\n";
    else
      iStream << (eofill ? "q W* n " : "q W n ")
	      << matrix() * g->iMatrix << " cm /Grad" << grad.index()
	      << " sh Q\n";
  } else {
    if (mode == EFilledOnly)
      iStream << (eofill ? "f*\n" : "f\n");
    else if (mode == EStrokedOnly)
      iStream << "S\n";
    else
      iStream << (eofill ? "B*\n" : "B\n"); // fill and then stroke
  }
}

void PdfPainter::doDrawBitmap(Bitmap bitmap)
{
  if (bitmap.objNum() < 0)
    return;
  iStream << matrix() << " cm /Image" << bitmap.objNum() << " Do\n";
}

void PdfPainter::doDrawText(const Text *text)
{
  const Text::XForm *xf = text->getXForm();
  if (!xf)
    return;
  drawOpacity();
  pushMatrix();
  transform(Matrix(xf->iStretch, 0, 0, xf->iStretch, 0, 0));
  translate(xf->iTranslation);
  iStream << matrix() << " cm " ;
  iStream << "/" << xf->iName << " Do\n";
  popMatrix();
}

void PdfPainter::doDrawSymbol(Attribute symbol)
{
  const Symbol *sym = cascade()->findSymbol(symbol);
  if (!sym)
    return;
  if (sym->iXForm)
    iStream << "/Symbol" << symbol.index() << " Do\n";
  else
    sym->iObject->draw(*this);
}

// --------------------------------------------------------------------

/*! \class ipe::PdfWriter
  \brief Create PDF file.

  This class is responsible for the creation of a PDF file from the
  Ipe data. You have to create a PdfWriter first, providing a file
  that has been opened for (binary) writing and is empty.  Then call
  createPages() to embed the pages.  Optionally, call \c
  createXmlStream to embed a stream with the XML representation of the
  document. Finally, call \c createTrailer to complete the PDF
  document, and close the file.

  Some reserved PDF object numbers:

    - 0: Must be left empty (a PDF restriction).
    - 1:  XML stream.
    - 2: Parent of all pages objects.

*/

//! Create a PDF writer operating on this (open and empty) file.
PdfWriter::PdfWriter(TellStream &stream, const Document *doc,
		     const PdfResources *resources,
		     bool markedView, int fromPage, int toPage,
		     int compression)
  : iStream(stream), iDoc(doc), iResources(resources), iMarkedView(markedView),
    iFromPage(fromPage), iToPage(toPage)
{
  iCompressLevel = compression;
  iObjNum = 3;  // 0 - 2 are reserved
  iXmlStreamNum = -1; // no XML stream yet
  iExtGState = -1;
  iPatternNum = -1;
  iBookmarks = -1;

  if (iFromPage < 0 || iFromPage >= iDoc->countPages())
    iFromPage = 0;
  if (iToPage < iFromPage || iToPage >= iDoc->countPages())
    iToPage = iDoc->countPages() - 1;

  // mark all bitmaps as not embedded
  BitmapFinder bm;
  iDoc->findBitmaps(bm);
  int id = -1;
  for (std::vector<Bitmap>::iterator it = bm.iBitmaps.begin();
       it != bm.iBitmaps.end(); ++it) {
    it->setObjNum(id);
    --id;
  }

  iStream << "%PDF-1.4\n";

  // embed all fonts and other resources from Pdflatex
  embedResources();

  // embed all extgstate objects
  AttributeSeq os;
  iDoc->cascade()->allNames(EOpacity, os);
  if (os.size() > 0 || hasResource("ExtGState")) {
    iExtGState = startObject();
    iStream << "<<\n";
    for (uint i = 0; i < os.size(); ++i) {
      Attribute alpha = iDoc->cascade()->find(EOpacity, os[i]);
      assert(alpha.isNumber());
      iStream << opacityName(alpha.number()) << " << /CA " << alpha.number()
	      << " /ca " << alpha.number() << " >>\n";
    }
    embedResource("ExtGState");
    iStream << ">> endobj\n";
  }

  // embed all gradients
  AttributeSeq gs;
  iDoc->cascade()->allNames(EGradient, gs);
  for (uint i = 0; i < gs.size(); ++i) {
    const Gradient *g = iDoc->cascade()->findGradient(gs[i]);
    int num = startObject();
    iStream << "<<\n"
	    << " /ShadingType " << int(g->iType) << "\n"
	    << " /ColorSpace /DeviceRGB\n";
    if (g->iType == Gradient::EAxial)
      iStream << " /Coords [" << g->iV[0] << " " << g->iV[1] << "]\n";
    else
      iStream << " /Coords [" << g->iV[0] << " " << g->iRadius[0]
	      << " " << g->iV[1] << " " << g->iRadius[1] << "]\n";

    iStream << " /Extend [" << (g->iExtend ? "true true]\n" : "false false]\n");

    if (g->iStops.size() == 2) {
      iStream << " /Function << /FunctionType 2 /Domain [ 0 1 ] /N 1\n"
	      << "     /C0 [";
      g->iStops[0].color.saveRGB(iStream);
      iStream << "]\n" << "     /C1 [";
      g->iStops[1].color.saveRGB(iStream);
      iStream << "] >>\n";
    } else {
      // need to stitch
      iStream << " /Function <<\n"
	      << "  /FunctionType 3 /Domain [ 0 1 ]\n"
	      << "  /Bounds [";
      int count = 0;
      for (uint i = 1; i < g->iStops.size() - 1; ++i) {
	if (g->iStops[i].offset > g->iStops[i-1].offset) {
	  iStream << g->iStops[i].offset << " ";
	  ++count;
	}
      }
      iStream << "]\n  /Encode [";
      for (int i = 0; i <= count; ++i)
	iStream << "0.0 1.0 ";
      iStream << "]\n  /Functions [\n";
      for (uint i = 1; i < g->iStops.size(); ++i) {
	if (g->iStops[i].offset > g->iStops[i-1].offset) {
	  iStream << "   << /FunctionType 2 /Domain [ 0 1 ] /N 1 /C0 [";
	  g->iStops[i-1].color.saveRGB(iStream);
	  iStream << "] /C1 [";
	  g->iStops[i].color.saveRGB(iStream);
	  iStream << "] >>\n";
	}
      }
      iStream << "] >>\n";
    }
    iStream << ">> endobj\n";
    iGradients[gs[i].index()] = num;
  }

  // embed all tilings
  AttributeSeq ts;
  std::map<int, int> patterns;
  iDoc->cascade()->allNames(ETiling, ts);
  if (ts.size() > 0 || hasResource("Pattern")) {
    for (uint i = 0; i < ts.size(); ++i) {
      const Tiling *t = iDoc->cascade()->findTiling(ts[i]);
      Linear m(t->iAngle);
      int num = startObject();
      iStream << "<<\n"
	      << "/Type /Pattern\n"
	      << "/PatternType 1\n"  // tiling pattern
	      << "/PaintType 2\n"    // uncolored pattern
	      << "/TilingType 2\n"   // faster
	      << "/BBox [ 0 0 100 " << t->iStep << " ]\n"
	      << "/XStep 99\n"
	      << "/YStep " << t->iStep << "\n"
	      << "/Resources << >>\n"
	      << "/Matrix [" << m << " 0 0]\n";
      String s;
      StringStream ss(s);
      ss << "0 0 100 " << t->iWidth << " re f\n";
      createStream(s.data(), s.size(), false);
      patterns[ts[i].index()] = num;
    }

    // create pattern dictionary
    iPatternNum = startObject();
    iStream << "<<\n";
    for (uint i = 0; i < ts.size(); ++i) {
      iStream << "/Pat" << ts[i].index() << " "
	      << patterns[ts[i].index()] << " 0 R\n";
    }
    embedResource("Pattern");
    iStream << ">> endobj\n";
  }

  // embed all symbols with xform attribute
  AttributeSeq sys;
  iDoc->cascade()->allNames(ESymbol, sys);
  if (sys.size()) {
    for (uint i = 0; i < sys.size(); ++i) {
      const Symbol *sym = iDoc->cascade()->findSymbol(sys[i]);
      if (sym->iXForm) {
	// compute bbox for object
	BBoxPainter bboxPainter(iDoc->cascade());
	sym->iObject->draw(bboxPainter);
	Rect bbox = bboxPainter.bbox();
	// embed all bitmaps it uses
	BitmapFinder bm;
	sym->iObject->accept(bm);
	embedBitmaps(bm);
	int num = startObject();
	iStream << "<<\n";
	iStream << "/Type /XObject\n";
	iStream << "/Subtype /Form\n";
	iStream << "/BBox ["  << bbox << "]\n";
	createResources(bm);
	String s;
	StringStream ss(s);
	PdfPainter painter(iDoc->cascade(), ss);
	sym->iObject->draw(painter);
	createStream(s.data(), s.size(), false);
	iSymbols[sys[i].index()] = num;
      }
    }
  }
}

//! Destructor.
PdfWriter::~PdfWriter()
{
  // nothing
}

/*! Write the beginning of the next object: "no 0 obj " and save
  information about file position. Default argument uses next unused
  object number.  Returns number of new object. */
int PdfWriter::startObject(int objnum)
{
  if (objnum < 0)
    objnum = iObjNum++;
  iXref[objnum] = iStream.tell();
  iStream << objnum << " 0 obj ";
  return objnum;
}

bool PdfWriter::hasResource(String kind) const noexcept
{
  return iResources && iResources->resourcesOfKind(kind);
}

/*! Write all PDF resources to the PDF file, and record their object
  numbers.  Embeds nothing if \c resources is nullptr, but must be
  called nevertheless. */
void PdfWriter::embedResources()
{
  if (iResources) {
    const std::vector<int> &seq = iResources->embedSequence();
    for (auto &num : seq) {
      const PdfObj *obj = iResources->object(num);
      int embedNum = startObject();
      obj->write(iStream, &iResourceNumber);
      iStream << " endobj\n";
      iResourceNumber[num] = embedNum;
    }
  }
}

void PdfWriter::embedResource(String kind)
{
  if (!iResources)
    return;
  const PdfDict *d = iResources->resourcesOfKind(kind);
  if (!d)
    return;
  for (int i = 0; i < d->count(); ++i) {
    iStream << "/" << d->key(i) << " ";
    d->value(i)->write(iStream, &iResourceNumber);
    iStream << " ";
  }
}

//! Write a stream.
/*! Write a stream, either plain or compressed, depending on compress
  level.  Object must have been created with dictionary start having
  been written.
  If \a preCompressed is true, the data is already deflated.
*/
void PdfWriter::createStream(const char *data, int size, bool preCompressed)
{
  if (preCompressed) {
    iStream << "/Length " << size << " /Filter /FlateDecode >>\nstream\n";
    iStream.putRaw(data, size);
    iStream << "\nendstream endobj\n";
    return;
  }

  if (iCompressLevel > 0) {
    int deflatedSize;
    Buffer deflated = DeflateStream::deflate(data, size, deflatedSize,
						   iCompressLevel);
    iStream << "/Length " << deflatedSize
	    << " /Filter /FlateDecode >>\nstream\n";
    iStream.putRaw(deflated.data(), deflatedSize);
    iStream << "\nendstream endobj\n";
  } else {
    iStream << "/Length " << size << " >>\nstream\n";
    iStream.putRaw(data, size);
    iStream << "endstream endobj\n";
  }
}

// --------------------------------------------------------------------

void PdfWriter::embedBitmap(Bitmap bitmap)
{
  int objnum = startObject();
  iStream << "<<\n";
  iStream << "/Type /XObject\n";
  iStream << "/Subtype /Image\n";
  iStream << "/Width " << bitmap.width() << "\n";
  iStream << "/Height " << bitmap.height() << "\n";
  switch (bitmap.colorSpace()) {
  case Bitmap::EDeviceGray:
    iStream << "/ColorSpace /DeviceGray\n";
    break;
  case Bitmap::EDeviceRGB:
    iStream << "/ColorSpace /DeviceRGB\n";
    break;
  case Bitmap::EDeviceCMYK:
    iStream << "/ColorSpace /DeviceCMYK\n";
    // apparently JPEG CMYK images need this decode array??
    // iStream << "/Decode [1 0 1 0 1 0 1 0]\n";
    break;
  }
  switch (bitmap.filter()) {
  case Bitmap::EFlateDecode:
    iStream << "/Filter /FlateDecode\n";
    break;
  case Bitmap::EDCTDecode:
    iStream << "/Filter /DCTDecode\n";
    break;
  default:
    // no filter
    break;
  }
  iStream << "/BitsPerComponent " << bitmap.bitsPerComponent() << "\n";
  if (bitmap.colorKey() >= 0) {
    int r = (bitmap.colorKey() >> 16) & 0xff;
    int g = (bitmap.colorKey() >> 8) & 0xff;
    int b = bitmap.colorKey() & 0xff;
    iStream << "/Mask [" << r << " " << r << " " << g << " " << g
	    << " " << b << " " << b << "]\n";
  }
  iStream << "/Length " << bitmap.size() << "\n>> stream\n";
  iStream.putRaw(bitmap.data(), bitmap.size());
  iStream << "\nendstream endobj\n";
  bitmap.setObjNum(objnum);
}

void PdfWriter::embedBitmaps(const BitmapFinder &bm)
{
  for (BmIter it = bm.iBitmaps.begin(); it != bm.iBitmaps.end(); ++it) {
    BmIter it1 = std::find(iBitmaps.begin(), iBitmaps.end(), *it);
    if (it1 == iBitmaps.end()) {
      // look again, more carefully
      for (it1 = iBitmaps.begin();
	   it1 != iBitmaps.end() && !it1->equal(*it); ++it1)
	;
      if (it1 == iBitmaps.end())
	embedBitmap(*it); // not yet embedded
      else
	it->setObjNum(it1->objNum()); // identical Bitmap is embedded
      iBitmaps.push_back(*it);
    }
  }
}

void PdfWriter::createResources(const BitmapFinder &bm)
{
  // Resources:
  // ProcSet, ExtGState, ColorSpace, Pattern, Shading, XObject, Font
  // not used now: Properties
  // Font is only used inside XObject, no font resource needed on page
  // ProcSet
  iStream << "/Resources <<\n  /ProcSet [/PDF";
  if (iResources)
    iStream << "/Text";
  if (!bm.iBitmaps.empty())
    iStream << "/ImageB/ImageC";
  iStream << "]\n";
  // Shading
  if (iGradients.size() || hasResource("Shading")) {
    iStream << "  /Shading <<";
    for (std::map<int,int>::const_iterator it = iGradients.begin();
	 it != iGradients.end(); ++it)
      iStream << " /Grad" << it->first << " " << it->second << " 0 R";
    embedResource("Shading");
    iStream << " >>\n";
  }
  // ExtGState
  if (iExtGState >= 0)
    iStream << "  /ExtGState " << iExtGState << " 0 R\n";
  // ColorSpace
  if (iPatternNum >= 0 || hasResource("ColorSpace")) {
    iStream << "  /ColorSpace << /PCS [/Pattern /DeviceRGB] ";
    embedResource("ColorSpace");
    iStream << ">>\n";
  }
  // Pattern
  if (iPatternNum >= 0 || hasResource("Pattern"))
    iStream << "  /Pattern " << iPatternNum << " 0 R\n";
  // XObject
  if (!bm.iBitmaps.empty() || !iSymbols.empty() || hasResource("XObject")) {
    iStream << "  /XObject << ";
    for (BmIter it = bm.iBitmaps.begin(); it != bm.iBitmaps.end(); ++it) {
      // mention each PDF object only once
      BmIter it1;
      for (it1 = bm.iBitmaps.begin();
	   it1 != it && it1->objNum() != it->objNum(); it1++)
	;
      if (it1 == it)
	iStream << "/Image" << it->objNum() << " " << it->objNum() << " 0 R ";
    }
    for (std::map<int,int>::const_iterator it = iSymbols.begin();
	 it != iSymbols.end(); ++it)
      iStream << "/Symbol" << it->first << " " << it->second << " 0 R ";
    embedResource("XObject");
    iStream << ">>\n";
  }
  iStream << "  >>\n";
}

// --------------------------------------------------------------------

void PdfWriter::paintView(Stream &stream, int pno, int view)
{
  const Page *page = iDoc->page(pno);
  PdfPainter painter(iDoc->cascade(), stream);

  const Symbol *background =
    iDoc->cascade()->findSymbol(Attribute::BACKGROUND());
  if (background && page->findLayer("BACKGROUND") < 0)
    painter.drawSymbol(Attribute::BACKGROUND());

  if (iDoc->properties().iNumberPages && iResources) {
    const Text *pn = iResources->pageNumber(pno, view);
    if (pn)
      pn->draw(painter);
  }

  const Text *title = page->titleText();
  if (title)
    title->draw(painter);

  for (int i = 0; i < page->count(); ++i) {
    if (page->objectVisible(view, i))
      page->object(i)->draw(painter);
  }
}

//! create contents and page stream for this page view.
void PdfWriter::createPageView(int pno, int view)
{
  const Page *page = iDoc->page(pno);
  // Find bitmaps to embed
  BitmapFinder bm;
  const Symbol *background =
    iDoc->cascade()->findSymbol(Attribute::BACKGROUND());
  if (background && page->findLayer("BACKGROUND") < 0)
    background->iObject->accept(bm);
  bm.scanPage(page);
  // ipeDebug("# of bitmaps: %d", bm.iBitmaps.size());
  embedBitmaps(bm);
  // create page stream
  String pagedata;
  StringStream sstream(pagedata);
  if (iCompressLevel > 0) {
    DeflateStream dfStream(sstream, iCompressLevel);
    paintView(dfStream, pno, view);
    dfStream.close();
  } else
    paintView(sstream, pno, view);

  int firstLink = -1;
  int lastLink = -1;
  for (int i = 0; i < page->count(); ++i) {
    const Group *g = page->object(i)->asGroup();
    if (g && page->objectVisible(view, i) && !g->url().empty()) {
      lastLink = startObject();
      if (firstLink < 0) firstLink = lastLink;
      iStream << "<<\n"
	      << "/Type /Annot\n"
	      << "/Subtype /Link\n"
	      << "/Rect [" << page->bbox(i) << "]\n"
	      << "/A <</Type/Action/S/URI/URI";
      writeString(g->url());
      iStream << ">>\n>> endobj\n";
    }
  }
  int contentsobj = startObject();
  iStream << "<<\n";
  createStream(pagedata.data(), pagedata.size(), (iCompressLevel > 0));
  int pageobj = startObject();
  iStream << "<<\n";
  iStream << "/Type /Page\n";
  if (firstLink >= 0) {
    iStream << "/Annots [ ";
    while (firstLink <= lastLink)
      iStream << firstLink++ << " 0 R ";
    iStream << "]\n";
  }
  iStream << "/Contents " << contentsobj << " 0 R\n";
  // iStream << "/Rotate 0\n";
  createResources(bm);
  if (!page->effect(view).isNormal()) {
    const Effect *effect = iDoc->cascade()->findEffect(page->effect(view));
    if (effect)
      effect->pageDictionary(iStream);
  }
  const Layout *layout = iDoc->cascade()->findLayout();
  iStream << "/MediaBox [ " << layout->paper() << "]\n";

  int viewBBoxLayer = page->findLayer("VIEWBBOX");
  Rect bbox;
  if (viewBBoxLayer >= 0 && page->visible(view, viewBBoxLayer))
    bbox = page->viewBBox(iDoc->cascade(), view);
  else
    bbox = page->pageBBox(iDoc->cascade());
  if (layout->iCrop && !bbox.isEmpty())
    iStream << "/CropBox [" << bbox << "]\n";
  if (!bbox.isEmpty())
    iStream << "/ArtBox [" << bbox << "]\n";
  iStream << "/Parent 2 0 R\n";
  iStream << ">> endobj\n";
  iPageObjectNumbers.push_back(pageobj);
}

//! Create all PDF pages.
void PdfWriter::createPages()
{
  for (int page = iFromPage; page <= iToPage; ++page) {
    if (iMarkedView && !iDoc->page(page)->marked())
      continue;
    int nViews = iDoc->page(page)->countViews();
    if (iMarkedView) {
      bool shown = false;
      for (int view = 0; view < nViews; ++view) {
	if (iDoc->page(page)->markedView(view)) {
	  createPageView(page, view);
	  shown = true;
	}
      }
      if (!shown)
	createPageView(page, nViews - 1);
    } else {
      for (int view = 0; view < nViews; ++view)
	createPageView(page, view);
    }
  }
}

//! Create a stream containing the XML data.
void PdfWriter::createXmlStream(String xmldata, bool preCompressed)
{
  iXmlStreamNum = startObject(1);
  iStream << "<<\n/Type /Ipe\n";
  createStream(xmldata.data(), xmldata.size(), preCompressed);
}

//! Write a PDF string object to the PDF stream.
void PdfWriter::writeString(String text)
{
  // Check if it is all ASCII
  bool isAscii = true;
  for (int i = 0; isAscii && i < text.size(); ++i) {
    if (text[i] & 0x80)
      isAscii = false;
  }
  if (isAscii) {
    iStream << "(";
    for (int i = 0; i < text.size(); ++i) {
      char ch = text[i];
      switch (ch) {
      case '(':
      case ')':
      case '\\':
	iStream << "\\";
	// fall through
      default:
	iStream << ch;
	break;
      }
    }
    iStream << ")";
  } else {
    char buf[5];
    iStream << "<FEFF";
    for (int i = 0; i < text.size(); ) {
      sprintf(buf, "%04X", text.unicode(i));
      iStream << buf;
    }
    iStream << ">";
  }
}

// --------------------------------------------------------------------

struct Section {
  int iPage;
  int iSeqPage;
  int iObjNum;
  std::vector<int> iSubPages;
  std::vector<int> iSubSeqPages;
};

//! Create the bookmarks (PDF outline).
void PdfWriter::createBookmarks()
{
  // first collect all information
  std::vector<Section> sections;
  int seqPg = 0;
  for (int pg = iFromPage; pg <= iToPage; ++pg) {
    String s = iDoc->page(pg)->section(0);
    String ss = iDoc->page(pg)->section(1);
    if (!s.empty()) {
      Section sec;
      sec.iPage = pg;
      sec.iSeqPage = seqPg;
      sections.push_back(sec);
      // ipeDebug("section on page %d, seq %d", pg, seqPg);
    }
    if (!sections.empty() && !ss.empty()) {
      sections.back().iSubPages.push_back(pg);
      sections.back().iSubSeqPages.push_back(seqPg);
      // ipeDebug("subsection on page %d, seq %d", pg, seqPg);
    }
    seqPg += iMarkedView ? iDoc->page(pg)->countMarkedViews() :
      iDoc->page(pg)->countViews();
  }
  if (sections.empty())
    return;
  // reserve outline object
  iBookmarks = iObjNum++;
  // assign object numbers
  for (uint s = 0; s < sections.size(); ++s) {
    sections[s].iObjNum = iObjNum++;
    iObjNum += sections[s].iSubPages.size(); // leave space for subsections
  }
  // embed root
  startObject(iBookmarks);
  iStream << "<<\n/First " << sections[0].iObjNum << " 0 R\n"
	  << "/Count " << int(sections.size()) << "\n"
	  << "/Last " << sections.back().iObjNum << " 0 R\n>> endobj\n";
  for (uint s = 0; s < sections.size(); ++s) {
    int count = sections[s].iSubPages.size();
    int obj = sections[s].iObjNum;
    // embed section
    startObject(obj);
    iStream << "<<\n/Title ";
    writeString(iDoc->page(sections[s].iPage)->section(0));
    iStream << "\n/Parent " << iBookmarks << " 0 R\n"
	    << "/Dest [ " << iPageObjectNumbers[sections[s].iSeqPage]
	    << " 0 R /XYZ null null null ]\n";
    if (s > 0)
      iStream << "/Prev " << sections[s-1].iObjNum << " 0 R\n";
    if (s < sections.size() - 1)
      iStream << "/Next " << sections[s+1].iObjNum << " 0 R\n";
    if (count > 0)
      iStream << "/Count " << -count << "\n"
	      << "/First " << (obj + 1) << " 0 R\n"
	      << "/Last " << (obj + count) << " 0 R\n";
    iStream << ">> endobj\n";
    // using ids obj + 1 .. obj + count for the subsections
    for (int ss = 0; ss < count; ++ss) {
      int pageNo = sections[s].iSubPages[ss];
      int seqPageNo = sections[s].iSubSeqPages[ss];
      startObject(obj + ss + 1);
      iStream << "<<\n/Title ";
      writeString(iDoc->page(pageNo)->section(1));
      iStream << "\n/Parent " << obj << " 0 R\n"
	      << "/Dest [ " << iPageObjectNumbers[seqPageNo]
	      << " 0 R /XYZ null null null ]\n";
      if (ss > 0)
	iStream << "/Prev " << (obj + ss) << " 0 R\n";
      if (ss < count - 1)
	iStream << "/Next " << (obj + ss + 2) << " 0 R\n";
      iStream << ">> endobj\n";
    }
  }
}

// --------------------------------------------------------------------

//! Create the root objects and trailer of the PDF file.
void PdfWriter::createTrailer()
{
  const Document::SProperties &props = iDoc->properties();
  // create /Pages
  startObject(2);
  iStream << "<<\n" << "/Type /Pages\n";
  iStream << "/Count " << int(iPageObjectNumbers.size()) << "\n";
  iStream << "/Kids [ ";
  for (std::vector<int>::const_iterator it = iPageObjectNumbers.begin();
       it != iPageObjectNumbers.end(); ++it)
    iStream << (*it) << " 0 R ";
  iStream << "]\n>> endobj\n";
  // create /Catalog
  int catalogobj = startObject();
  iStream << "<<\n/Type /Catalog\n/Pages 2 0 R\n";
  if (props.iFullScreen)
    iStream << "/PageMode /FullScreen\n";
  if (iBookmarks >= 0) {
    if (!props.iFullScreen)
      iStream << "/PageMode /UseOutlines\n";
    iStream << "/Outlines " << iBookmarks << " 0 R\n";
  }
  if (iDoc->countTotalViews() > 1) {
    iStream << "/PageLabels << /Nums [ ";
    int count = 0;
    for (int page = 0; page < iDoc->countPages(); ++page) {
      if (!iMarkedView || iDoc->page(page)->marked()) {
	int nviews = iMarkedView ? iDoc->page(page)->countMarkedViews() :
	  iDoc->page(page)->countViews();
	if (nviews > 1) {
	  iStream << count << " <</S /D /P (" << (page + 1) << "-)>>";
	} else { // one view only!
	  iStream << count << " <</P (" << (page + 1) << ")>>";
	}
	count += nviews;
      }
    }
    iStream << "] >>\n";
  }
  iStream << ">> endobj\n";
  // create /Info
  int infoobj = startObject();
  iStream << "<<\n";
  if (!props.iCreator.empty()) {
    iStream << "/Creator (" << props.iCreator << ")\n";
    iStream << "/Producer (" << props.iCreator << ")\n";
  }
  if (!props.iTitle.empty()) {
    iStream << "/Title ";
    writeString(props.iTitle);
    iStream << "\n";
  }
  if (!props.iAuthor.empty()) {
    iStream << "/Author ";
    writeString(props.iAuthor);
    iStream << "\n";
  }
  if (!props.iSubject.empty()) {
    iStream << "/Subject ";
    writeString(props.iSubject);
    iStream << "\n";
  }
  if (!props.iKeywords.empty()) {
    iStream << "/Keywords ";
    writeString(props.iKeywords);
    iStream << "\n";
  }
  iStream << "/CreationDate (" << props.iCreated << ")\n";
  iStream << "/ModDate (" << props.iModified << ")\n";
  iStream << ">> endobj\n";
  // create Xref
  long xrefpos = iStream.tell();
  iStream << "xref\n0 " << iObjNum << "\n";
  for (int obj = 0; obj < iObjNum; ++obj) {
    std::map<int, long>::const_iterator it = iXref.find(obj);
    char s[12];
    if (it == iXref.end()) {
      std::sprintf(s, "%010d", obj);
      iStream << s << " 00000 f \n"; // note the final space!
    } else {
      std::sprintf(s, "%010ld", iXref[obj]);
      iStream << s << " 00000 n \n"; // note the final space!
    }
  }
  iStream << "trailer\n<<\n";
  iStream << "/Size " << iObjNum << "\n";
  iStream << "/Root " << catalogobj << " 0 R\n";
  iStream << "/Info " << infoobj << " 0 R\n";
  iStream << ">>\nstartxref\n" << int(xrefpos) << "\n%%EOF\n";
}

// --------------------------------------------------------------------
