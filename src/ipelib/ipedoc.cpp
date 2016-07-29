// --------------------------------------------------------------------
// The Ipe document.
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

#include "ipedoc.h"
#include "ipeiml.h"
#include "ipestyle.h"
#include "ipereference.h"
#include "ipepainter.h"
#include "ipeutils.h"
#include "ipepdfparser.h"
#include "ipepdfwriter.h"
#include "ipelatex.h"

#include <errno.h>

using namespace ipe;

// --------------------------------------------------------------------

/*! \defgroup doc Ipe Document
  \brief The classes managing an Ipe document.

  The main class, Document, represents an entire Ipe document, and
  allows you to load, save, access, and modify such a document.

  Other classes represent pages, layers, and views of a document.
  Another important class is the StyleSheet, which maps symbolic
  attributes to absolute values.
*/

/*! \class ipe::Document
  \ingroup doc
  \brief The model for an Ipe document.

  The Document class represents the contents of an Ipe document, and
  all the methods necessary to load, save, and modify it.
*/

//! Construct an empty document for filling by a client.
/*! As constructed, it has no pages, A4 media, and
  only the standard style sheet. */
Document::Document()
{
  iResources = nullptr;
  iCascade = new Cascade();
  iCascade->insert(0, StyleSheet::standard());
}

//! Destructor.
Document::~Document()
{
  for (int i = 0; i < countPages(); ++i)
    delete page(i);
  delete iCascade;
  delete iResources;
}

//! Copy constructor.
Document::Document(const Document &rhs)
{
  iCascade = new Cascade(*rhs.iCascade);
  for (int i = 0; i < rhs.countPages(); ++i)
    iPages.push_back(new Page(*rhs.page(i)));
  iProperties = rhs.iProperties;
  iResources = nullptr;
}

// ---------------------------------------------------------------------

String readLine(DataSource &source)
{
  String s;
  int ch = source.getChar();
  while (ch != EOF && ch != '\n') {
    s += char(ch);
    ch = source.getChar();
  }
  return s;
}

//! Determine format of file in \a source.
Document::TFormat Document::fileFormat(DataSource &source)
{
  String s1 = readLine(source);
  String s2 = readLine(source);
  if (s1.substr(0, 5) == "<?xml" ||
      s1.substr(0, 9) == "<!DOCTYPE" ||
      s1.substr(0, 4) == "<ipe")
    return EXml;
  if (s1.substr(0, 4) == "%PDF")
    return EPdf;  // let's assume it contains an Ipe stream
  if (s1.substr(0, 4) == "%!PS") {
    if (s2.substr(0, 11) != "%%Creator: ")
      return EUnknown;
    if (s2.substr(11, 6) == "Ipelib" || s2.substr(11, 4) == "xpdf")
      return EEps;
    if (s2.substr(11, 3) == "Ipe")
      return EIpe5;
    return EUnknown;
  }
  if (s1.substr(0, 5) == "%\\Ipe" || s1.substr(0, 6) == "%\\MIPE")
    return EIpe5;
  return EUnknown;
}

//! Determine format of file from filename \a fn.
Document::TFormat Document::formatFromFilename(String fn)
{
  if (fn.size() < 5)
    return Document::EUnknown;
  // fn = fn.toLower();
  String s = fn.right(4);
  if (s == ".xml" || s == ".ipe")
    return Document::EXml;
  else if (s == ".pdf")
    return Document::EPdf;
  else if (s == ".eps")
    return Document::EEps;
  else
    return Document::EUnknown;
}

// --------------------------------------------------------------------

class PdfStreamParser : public ImlParser {
public:
  explicit PdfStreamParser(PdfFile &loader, DataSource &source);
  virtual Buffer pdfStream(int objNum);
private:
  PdfFile &iLoader;
};

PdfStreamParser::PdfStreamParser(PdfFile &loader, DataSource &source)
  : ImlParser(source), iLoader(loader)
{
  // nothing
}

Buffer PdfStreamParser::pdfStream(int objNum)
{
  const PdfObj *obj = iLoader.object(objNum);
  if (!obj || !obj->dict() || obj->dict()->stream().size() == 0)
    return Buffer();
  return obj->dict()->stream();
}

// --------------------------------------------------------------------

class PsSource : public DataSource {
public:
  PsSource(DataSource &source) : iSource(source) { /* nothing */ }
  bool skipToXml();
  String readLine();
  Buffer image(int index) const;
  int getNext() const;
  inline bool deflated() const { return iDeflated; }

  virtual int getChar();
private:
  DataSource &iSource;
  std::vector<Buffer> iImages;
  bool iEos;
  bool iDeflated;
};

int PsSource::getChar()
{
  int ch = iSource.getChar();
  if (ch == '\n')
    iSource.getChar(); // remove '%'
  return ch;
}

String PsSource::readLine()
{
  String s;
  int ch = iSource.getChar();
  while (ch != EOF && ch != '\n') {
    s += char(ch);
    ch = iSource.getChar();
  }
  iEos = (ch == EOF);
  return s;
}

Buffer PsSource::image(int index) const
{
  if (1 <= index && index <= int(iImages.size()))
    return iImages[index - 1];
  else
    return Buffer();
}

bool PsSource::skipToXml()
{
  iDeflated = false;

  String s1 = readLine();
  String s2 = readLine();

  if (s1.substr(0, 11) != "%!PS-Adobe-" ||
      s2.substr(0, 17) != "%%Creator: Ipelib")
    return false;

  do {
    s1 = readLine();
    if (s1.substr(0, 17) == "%%BeginIpeImage: ") {
      Lex lex(s1.substr(17));
      int num, len;
      lex >> num >> len;
      if (num != int(iImages.size() + 1))
	return false;
      (void) readLine();  // skip 'image'
      Buffer buf(len);
      A85Source a85(iSource);
      char *p = buf.data();
      char *p1 = p + buf.size();
      while (p < p1) {
	int ch = a85.getChar();
	if (ch == EOF)
	  return false;
	*p++ = char(ch);
      }
      iImages.push_back(buf);
    }
  } while (!iEos && s1.substr(0, 13) != "%%BeginIpeXml");

  iDeflated = (s1.substr(13, 14) == ": /FlateDecode");
  if (iEos)
    return false;
  (void) iSource.getChar(); // skip '%' before <ipe>
  return true;
}

class PsStreamParser : public ImlParser {
public:
  explicit PsStreamParser(DataSource &source, PsSource &psSource);
  virtual Buffer pdfStream(int objNum);
private:
  PsSource &iPsSource;
};

PsStreamParser::PsStreamParser(DataSource &source, PsSource &psSource)
  : ImlParser(source), iPsSource(psSource)
{
  // nothing
}

Buffer PsStreamParser::pdfStream(int objNum)
{
  return iPsSource.image(objNum);
}


// --------------------------------------------------------------------

Document *doParse(Document *self, ImlParser &parser, int &reason)
{
  int res = parser.parseDocument(*self);
  if (res) {
    delete self;
    self = 0;
    if (res == ImlParser::ESyntaxError)
      reason = parser.parsePosition();
    else
      reason = -res;
  }
  return self;
}

Document *doParseXml(DataSource &source, int &reason)
{
  Document *self = new Document;
  ImlParser parser(source);
  return doParse(self, parser, reason);
}

Document *doParsePs(DataSource &source, int &reason)
{
  PsSource psSource(source);
  reason = Document::EFileOpenError; // could not find Xml stream
  if (!psSource.skipToXml())
    return 0;

  Document *self = new Document;
  if (psSource.deflated()) {
    A85Source a85(psSource);
    InflateSource source(a85);
    PsStreamParser parser(source, psSource);
    return doParse(self, parser, reason);
  } else {
    PsStreamParser parser(psSource, psSource);
    return doParse(self, parser, reason);
  }
}

Document *doParsePdf(DataSource &source, int &reason)
{
  PdfFile loader;
  reason = Document::ENotAnIpeFile;
  if (!loader.parse(source))  // could not parse PDF container
    return 0;
  const PdfObj *obj = loader.object(1);
  // was the object really created by Ipe?
  if (!obj || !obj->dict())
    return 0;
  const PdfObj *type = obj->dict()->get("Type", 0);
  if (!type || !type->name() || type->name()->value() != "Ipe")
    return 0;

  Buffer buffer = obj->dict()->stream();
  BufferSource xml(buffer);

  Document *self = new Document;
  if (obj->dict()->deflated()) {
    InflateSource xml1(xml);
    PdfStreamParser parser(loader, xml1);
    return doParse(self, parser, reason);
  } else {
    PdfStreamParser parser(loader, xml);
    return doParse(self, parser, reason);
  }
}

//! Construct a document from an input stream.
/*! Returns 0 if the stream couldn't be parsed, and a reason
  explaining that in \a reason.  If \a reason is positive, it is a
  file (stream) offset where parsing failed.  If \a reason is
  negative, it is an error code, see Document::LoadErrors.
*/
Document *Document::load(DataSource &source, TFormat format,
			 int &reason)
{
  if (format == EXml)
    return doParseXml(source, reason);

  if (format == EPdf)
    return doParsePdf(source, reason);

  if (format == EEps)
    return doParsePs(source, reason);

  reason = (format == EIpe5) ? EVersionTooOld : ENotAnIpeFile;
  return 0;
}

Document *Document::load(const char *fname, int &reason)
{
  reason = EFileOpenError;
  std::FILE *fd = Platform::fopen(fname, "rb");
  if (!fd)
    return 0;
  FileSource source(fd);
  TFormat format = fileFormat(source);
  std::rewind(fd);
  Document *self = load(source, format, reason);
  std::fclose(fd);
  return self;
}

Document *Document::loadWithErrorReport(const char *fname)
{
  int reason;
  Document *doc = load(fname, reason);
  if (doc)
    return doc;

  fprintf(stderr, "Could not read Ipe file '%s'\n", fname);
  switch (reason) {
  case Document::EVersionTooOld:
    fprintf(stderr, "The Ipe version of this document is too old.\n"
	    "Please convert it using 'ipe6upgrade'.\n");
    break;
  case Document::EVersionTooRecent:
    fprintf(stderr, "The document was created by a newer version of Ipe.\n"
	    "Please upgrade your Ipe installation.\n");
    break;
  case Document::EFileOpenError:
    perror("Error opening the file");
    break;
  case Document::ENotAnIpeFile:
    fprintf(stderr, "The document was not created by Ipe.\n");
    break;
  default:
    fprintf(stderr, "Error parsing the document at position %d\n.", reason);
    break;
  }
  return 0;
}

// --------------------------------------------------------------------

//! Save in a stream.
/*! Returns true if sucessful.
*/
bool Document::save(TellStream &stream, TFormat format, uint flags) const
{
  if (format == EXml) {
    stream << "<?xml version=\"1.0\"?>\n";
    stream << "<!DOCTYPE ipe SYSTEM \"ipe.dtd\">\n";
    saveAsXml(stream);
    return true;
  }

  int compresslevel = 9;
  if (flags & ENoZip)
    compresslevel = 0;

  if (format == EPdf) {
    PdfWriter writer(stream, this, iResources, (flags & EMarkedView),
		     0, -1, compresslevel);
    writer.createPages();
    writer.createBookmarks();
    if (!(flags & EExport)) {
      String xmlData;
      StringStream stream(xmlData);
      if (compresslevel > 0) {
	DeflateStream dfStream(stream, compresslevel);
	// all bitmaps have been embedded and carry correct object number
	saveAsXml(dfStream, true);
	dfStream.close();
	writer.createXmlStream(xmlData, true);
      } else {
	saveAsXml(stream, true);
	writer.createXmlStream(xmlData, false);
      }
    }
    writer.createTrailer();
    return true;
  }

  return false;
}

bool Document::save(const char *fname, TFormat format, uint flags) const
{
  std::FILE *fd = Platform::fopen(fname, "wb");
  if (!fd)
    return false;
  FileStream stream(fd);
  bool result = save(stream, format, flags);
  std::fclose(fd);
  return result;
}

//! Export a single view to PDF
bool Document::exportView(const char *fname, TFormat format, uint flags,
			  int pno, int vno) const
{
  if (format != EPdf)
    return false;

  int compresslevel = 9;
  if (flags & ENoZip)
    compresslevel = 0;

  std::FILE *fd = Platform::fopen(fname, "wb");
  if (!fd)
    return false;
  FileStream stream(fd);

  PdfWriter writer(stream, this, iResources, (flags & EMarkedView),
		   pno, pno, compresslevel);
  writer.createPageView(pno, vno);
  writer.createTrailer();
  std::fclose(fd);
  return true;
}

//! Export a range of pages to PDF.
bool Document::exportPages(const char *fname, uint flags,
			   int fromPage, int toPage) const
{
  int compresslevel = 9;
  if (flags & ENoZip)
    compresslevel = 0;
  std::FILE *fd = Platform::fopen(fname, "wb");
  if (!fd)
    return false;
  FileStream stream(fd);
  PdfWriter writer(stream, this, iResources, (flags & EMarkedView),
		   fromPage, toPage, compresslevel);
  writer.createPages();
  writer.createTrailer();
  std::fclose(fd);
  return true;
}

// --------------------------------------------------------------------

//! Create a list of all bitmaps in the document.
void Document::findBitmaps(BitmapFinder &bm) const
{
  for (int i = 0; i < countPages(); ++i)
    bm.scanPage(page(i));
  // also need to look at all templates
  AttributeSeq seq;
  iCascade->allNames(ESymbol, seq);
  for (AttributeSeq::iterator it = seq.begin(); it != seq.end(); ++it) {
    const Symbol *symbol = iCascade->findSymbol(*it);
    symbol->iObject->accept(bm);
  }
  std::sort(bm.iBitmaps.begin(), bm.iBitmaps.end());
}

static int checkFileFormatVersion(const Document *doc) noexcept {
  // check file format update to 7.2.5
  for (int i = 0; i < doc->countPages(); ++i) {
    const Page *p = doc->page(i);
    for (int j = 0; j < p->count(); ++j) {
      const Group *g = p->object(j)->asGroup();
      if (g) {
	if (!g->url().empty() || !g->getAttribute(EPropDecoration).isNormal())
	  return ipe::FILE_FORMAT_NEW;
      }
    }
  }
  for (int i = 0; i < doc->cascade()->count() - 1; ++i) {
    // skip standard stylesheet
    if (doc->cascade()->sheet(i)->pageNumberStyle())
      return ipe::FILE_FORMAT_NEW;
  }
  return ipe::FILE_FORMAT;
}

//! Save in XML format into an Stream.
void Document::saveAsXml(Stream &stream, bool usePdfBitmaps) const
{
  int ff = checkFileFormatVersion(this);
  stream << "<ipe version=\"" << ff << "\"";
  if (!iProperties.iCreator.empty())
    stream << " creator=\"" << iProperties.iCreator << "\"";
  stream << ">\n";
  String info;
  StringStream infoStr(info);
  infoStr << "<info";
  if (!iProperties.iCreated.empty())
    infoStr << " created=\"" << iProperties.iCreated << "\"";
  if (!iProperties.iModified.empty())
    infoStr << " modified=\"" << iProperties.iModified << "\"";
  if (!iProperties.iTitle.empty()) {
    infoStr << " title=\"";
    infoStr.putXmlString(iProperties.iTitle);
    infoStr << "\"";
  }
  if (!iProperties.iAuthor.empty()) {
    infoStr << " author=\"";
    infoStr.putXmlString(iProperties.iAuthor);
    infoStr << "\"";
  }
  if (!iProperties.iSubject.empty()) {
    infoStr << " subject=\"";
    infoStr.putXmlString(iProperties.iSubject);
    infoStr << "\"";
  }
  if (!iProperties.iKeywords.empty()) {
    infoStr << " keywords=\"";
    infoStr.putXmlString(iProperties.iKeywords);
    infoStr << "\"";
  }
  if (iProperties.iFullScreen) {
    infoStr << " pagemode=\"fullscreen\"";
  }
  if (iProperties.iNumberPages) {
    infoStr << " numberpages=\"yes\"";
  }
  switch (iProperties.iTexEngine) {
  case LatexType::Pdftex:
    infoStr << " tex=\"pdftex\"";
    break;
  case LatexType::Xetex:
    infoStr << " tex=\"xetex\"";
    break;
  case LatexType::Luatex:
    infoStr << " tex=\"luatex\"";
    break;
  case LatexType::Default:
  default:
    break;
  }
  infoStr << "/>\n";
  if (info.size() > 10)
    stream << info;

  if (!iProperties.iPreamble.empty()) {
    stream << "<preamble>";
    stream.putXmlString(iProperties.iPreamble);
    stream << "</preamble>\n";
  }

  // save bitmaps
  BitmapFinder bm;
  findBitmaps(bm);
  if (!bm.iBitmaps.empty()) {
    int id = 1;
    Bitmap prev;
    for (std::vector<Bitmap>::iterator it = bm.iBitmaps.begin();
	 it != bm.iBitmaps.end(); ++it) {
      if (!it->equal(prev)) {
	if (usePdfBitmaps) {
	  it->saveAsXml(stream, it->objNum(), it->objNum());
	} else {
	  it->saveAsXml(stream, id);
	  it->setObjNum(id);
	}
      } else
	it->setObjNum(prev.objNum()); // noop if prev == it
      prev = *it;
      ++id;
    }
  }

  // now save style sheet
  iCascade->saveAsXml(stream);

  // save pages
  for (int i = 0; i < countPages(); ++i)
    page(i)->saveAsXml(stream);
  stream << "</ipe>\n";
}

// --------------------------------------------------------------------

//! Set document properties.
void Document::setProperties(const SProperties &props)
{
  iProperties = props;
}

//! Replace the entire style sheet cascade.
/*! Takes ownership of \a cascade, and returns the original cascade. */
Cascade *Document::replaceCascade(Cascade *sheets)
{
  Cascade *old = iCascade;
  iCascade = sheets;
  return old;
}

//! Check all symbolic attributes in the document.
/*!  This function verifies that all symbolic attributes in the
  document are defined in the style sheet. It appends to \a seq all
  symbolic attributes (in no particular order, but without duplicates)
  that are NOT defined.

  Returns \c true if there are no undefined symbolic attributes in the
  document.
*/
bool Document::checkStyle(AttributeSeq &seq) const
{
  for (int i = 0; i < countPages(); ++i) {
    for (int j = 0; j < page(i)->count(); ++j) {
      page(i)->object(j)->checkStyle(cascade(), seq);
    }
  }
  return (seq.size() == 0);
}

//! Update the PDF resources (after running latex).
/*! Takes ownership. */
void Document::setResources(PdfResources *resources)
{
  delete iResources;
  iResources = resources;
}

#if 0
//! Load a style sheet and add at top of cascade.
bool Document::addStyleSheet(DataSource &source)
{
  ImlParser parser(source);
  StyleSheet *sheet = parser.parseStyleSheet();
  if (sheet) {
    sheet->setCascade(iCascade);
    setStyleSheet(sheet);
    return true;
  } else
    return false;
}
#endif

//! Return total number of views in all pages.
int Document::countTotalViews() const
{
  int views = 0;
  for (int i = 0; i < countPages(); ++i) {
    int nviews = page(i)->countViews();
    views += (nviews > 0) ? nviews : 1;
  }
  return views;
}

//! Insert a new page.
/*! The page is inserted at index \a no. */
void Document::insert(int no, Page *page)
{
  iPages.insert(iPages.begin() + no, page);
}

//! Append a new page.
void Document::push_back(Page *page)
{
  iPages.push_back(page);
}

//! Replace page.
/*! Returns the original page. */
Page *Document::set(int no, Page *page)
{
  Page *p = iPages[no];
  iPages[no] = page;
  return p;
}

//! Remove a page.
/*! Returns the page that has been removed.  */
Page *Document::remove(int no)
{
  Page *p = iPages[no];
  iPages.erase(iPages.begin() + no);
  return p;
}

// --------------------------------------------------------------------

//! Run PdfLatex or Xelatex
int Document::runLatex(String &texLog)
{
  texLog = "";
  Latex converter(cascade(), iProperties.iTexEngine);

  AttributeSeq seq;
  cascade()->allNames(ESymbol, seq);

  for (AttributeSeq::iterator it = seq.begin(); it != seq.end(); ++it) {
    const Symbol *sym = cascade()->findSymbol(*it);
    if (sym)
      converter.scanObject(sym->iObject);
  }

  int count = 0;
  for (int i = 0; i < countPages(); ++i)
    count = converter.scanPage(page(i));
  if (count == 0)
    return ErrNoText;

  if (iProperties.iNumberPages) {
    for (int i = 0; i < countPages(); ++i) {
      int nviews = page(i)->countViews();
      for (int j = 0; j < nviews; ++j)
	converter.addPageNumber(i, j, countPages(), nviews);
    }
  }

  // First we need a directory
  String latexDir = Platform::latexDirectory();
  if (latexDir.empty())
    return ErrNoDir;

  String texFile = latexDir + "ipetemp.tex";
  String pdfFile = latexDir + "ipetemp.pdf";
  String logFile = latexDir + "ipetemp.log";

  std::remove(logFile.z());

  std::FILE *file = Platform::fopen(texFile.z(), "wb");
  if (!file)
    return ErrWritingSource;
  FileStream stream(file);
  int err = converter.createLatexSource(stream, properties().iPreamble);
  std::fclose(file);

  if (err < 0)
    return ErrWritingSource;

  int result = Platform::runLatex(latexDir, iProperties.iTexEngine);

  if (result != 0 && result != 1)
    return ErrRunLatex;

  // Check log file for Pdflatex version and errors
  texLog = Platform::readFile(logFile);
  if (texLog.left(14) != "This is pdfTeX" &&
      texLog.left(15) != "This is pdfeTeX" &&
      texLog.left(13) != "This is XeTeX" &&
      texLog.left(14) != "This is LuaTeX" &&
      texLog.left(22) != "entering extended mode")
    return ErrRunLatex;
  int i = texLog.find('\n');
  if (i < 0)
    return ErrRunLatex;
  String version = texLog.substr(8, i);
  ipeDebug("%s", version.z());
  // Check for error
  if (texLog.find("\n!") >= 0)
    return ErrLatex;

  std::FILE *pdfF = Platform::fopen(pdfFile.z(), "rb");
  if (!pdfF)
    return ErrLatex;
  FileSource source(pdfF);
  bool okay = (converter.readPdf(source) && converter.updateTextObjects());
  std::fclose(pdfF);

  if (okay) {
    setResources(converter.takeResources());
    resources()->show();
    return ErrNone;
  } else
    return ErrLatexOutput;
}

//! Run Pdflatex (suitable for console applications)
/*! Success/error is reported on stderr. */
int Document::runLatex()
{
  String logFile;
  switch (runLatex(logFile)) {
  case ErrNoText:
    fprintf(stderr, "No text objects in document, no need to run Pdflatex.\n");
    return 0;
  case ErrNoDir:
    fprintf(stderr, "Directory '%s' does not exist and cannot be created.\n",
	    "latexdir");
    return 1;
  case ErrWritingSource:
    fprintf(stderr, "Error writing Latex source.\n");
    return 1;
  case ErrOldPdfLatex:
    fprintf(stderr, "Your installed version of Pdflatex is too old.\n");
    return 1;
  case ErrRunLatex:
    fprintf(stderr, "There was an error trying to run Pdflatex.\n");
    return 1;
  case ErrLatex:
    fprintf(stderr, "There were Latex errors.\n");
    return 1;
  case ErrLatexOutput:
    fprintf(stderr, "There was an error reading the Pdflatex output.\n");
    return 1;
  case ErrNone:
  default:
    fprintf(stderr, "Pdflatex was run sucessfully.\n");
    return 0;
  }
}

// --------------------------------------------------------------------
