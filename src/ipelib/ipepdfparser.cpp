// --------------------------------------------------------------------
// PDF parsing
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

#include "ipepdfparser.h"
#include "ipeutils.h"
#include <cstdlib>

using namespace ipe;

//------------------------------------------------------------------------

// A '1' in this array means the character is white space.
// A '1' or '2' means the character ends a name or command.
// '2' == () {} [] <> / %
static char specialChars[256] = {
  1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 1, 1, 0, 0,   // 0x
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,   // 1x
  1, 0, 0, 0, 0, 2, 0, 0, 2, 2, 0, 0, 0, 0, 0, 2,   // 2x
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 2, 0,   // 3x
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,   // 4x
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 2, 0, 0,   // 5x
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,   // 6x
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 2, 0, 0,   // 7x
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,   // 8x
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,   // 9x
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,   // ax
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,   // bx
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,   // cx
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,   // dx
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,   // ex
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0    // fx
};

// --------------------------------------------------------------------

inline int toInt(String &s)
{
  return std::strtol(s.z(), 0, 10);
}

// --------------------------------------------------------------------

/*! \class ipe::PdfObj
 * \ingroup base
 * \brief Abstract base class for PDF objects.
 */

//! Pure virtual destructor.
PdfObj::~PdfObj()
{
  // nothing
}

//! Return this object as PDF null object.
const PdfNull *PdfObj::null() const noexcept { return nullptr; }

//! Return this object as PDF bool object.
const PdfBool *PdfObj::boolean() const noexcept { return nullptr; }

//! Return this object as PDF number object.
const PdfNumber *PdfObj::number() const noexcept { return nullptr; }

//! Return this object as PDF string object.
const PdfString *PdfObj::string() const noexcept { return nullptr; }

//! Return this object as PDF name object.
const PdfName *PdfObj::name() const noexcept { return nullptr; }

//! Return this object as PDF reference object.
const PdfRef *PdfObj::ref() const noexcept { return nullptr; }

//! Return this object as PDF array object.
const PdfArray *PdfObj::array() const noexcept { return nullptr; }

//! Return this object as PDF dictionary object.
const PdfDict *PdfObj::dict() const noexcept { return nullptr; }

//! Return PDF representation of the object.
String PdfObj::repr() const noexcept
{
  String d;
  StringStream ss(d);
  write(ss);
  return d;
}

/*! \class ipe::PdfNull
 * \ingroup base
 * \brief The PDF null object.
 */
const PdfNull *PdfNull::null() const noexcept { return this; }

void PdfNull::write(Stream &stream, const PdfRenumber *) const noexcept
{
  stream << "null";
}

/*! \class ipe::PdfBool
 * \ingroup base
 * \brief The PDF bool object.
 */
const PdfBool *PdfBool::boolean() const noexcept { return this; }

void PdfBool::write(Stream &stream, const PdfRenumber *) const noexcept
{
  stream << (iValue ? "true" : "false");
}

/*! \class ipe::PdfNumber
 * \ingroup base
 * \brief The PDF number object.
 */
const PdfNumber *PdfNumber::number() const noexcept { return this; }

void PdfNumber::write(Stream &stream, const PdfRenumber *) const noexcept
{
  stream << iValue;
}

/*! \class ipe::PdfString
 * \ingroup base
 * \brief The PDF string object.
 */
const PdfString *PdfString::string() const noexcept { return this; }

void PdfString::write(Stream &stream, const PdfRenumber *) const noexcept
{
  if (iBinary) {
    stream << "<" << iValue << ">";
  } else {
    char octbuf[5];
    stream << "(";
    for (int i = 0; i < iValue.size(); ++i) {
      int ch = iValue[i];
      if ((0 <= ch && ch < 0x20) || ch == '\\' || ch == '(' || ch == ')') {
	sprintf(octbuf, "\\%.3o", (ch & 0xff));
	stream << octbuf;
      } else
	stream.putChar(ch);
    }
    stream << ")";
  }
}

//! Return value of string after decoding binary strings.
String PdfString::decode() const noexcept
{
  if (!iBinary)
    return iValue;
  String result;
  Lex lex(iValue);
  while (!lex.eos())
    result += char(lex.getHexByte());
  return result;
}

/*! \class ipe::PdfName
 * \ingroup base
 * \brief The PDF name object.
 */
const PdfName *PdfName::name() const noexcept { return this; }

void PdfName::write(Stream &stream, const PdfRenumber *) const noexcept
{
  stream << "/" << iValue;
}

/*! \class ipe::PdfRef
 * \ingroup base
 * \brief The PDF reference object (indirect object).
 */
const PdfRef *PdfRef::ref() const noexcept { return this; }

void PdfRef::write(Stream &stream, const PdfRenumber *renumber) const noexcept
{
  if (renumber) {
    auto it = renumber->find(iValue);
    if (it != renumber->end()) {
      stream << it->second << " 0 R";
      return;
    }
  }
  stream << iValue << " 0 R";
}

/*! \class ipe::PdfArray
 * \ingroup base
 * \brief The PDF array object.
 */
const PdfArray *PdfArray::array() const noexcept { return this; }

void PdfArray::write(Stream &stream, const PdfRenumber *renumber) const noexcept
{
  stream << "[";
  String sep = "";
  for (int i = 0; i < count(); ++i) {
    stream << sep;
    sep = " ";
    obj(i, 0)->write(stream, renumber);
  }
  stream << "]";
}

PdfArray::~PdfArray()
{
  for (std::vector<const PdfObj *>::iterator it = iObjects.begin();
       it != iObjects.end(); ++it) {
    delete *it;
    *it = 0;
  }
}

//! Append an object to array.
/*! Array takes ownership of the object. */
void PdfArray::append(const PdfObj *obj)
{
  iObjects.push_back(obj);
}

//! Return object with \a index in array.
/*! Indirect objects (references) are looked up if \a file is not
  nullptr, and the object referred to is returned (nullptr if it does not
  exist).  Object remains owned by array.
*/
const PdfObj *PdfArray::obj(int index, const PdfFile *file) const noexcept
{
  const PdfObj *obj = iObjects[index];
  if (file && obj->ref()) {
    int n = obj->ref()->value();
    return file->object(n);
  }
  return obj;
}

/*! \class ipe::PdfDict
 * \ingroup base
 * \brief The PDF dictionary and stream objects.

 A dictionary may or may not have attached stream data.
 */

const PdfDict *PdfDict::dict() const noexcept { return this; }

//! Return PDF representation of the PdfDict without the stream.
String PdfDict::dictRepr() const noexcept
{
  String d;
  StringStream ss(d);
  dictWrite(ss);
  return d;
}

void PdfDict::dictWrite(Stream &stream, const PdfRenumber *renumber)
  const noexcept
{
  stream << "<<";
  for (std::vector<Item>::const_iterator it = iItems.begin();
       it != iItems.end(); ++it) {
    if (it != iItems.begin())
      stream << " ";
    stream << "/" << it->iKey << " ";
    it->iVal->write(stream, renumber);
  }
  stream << ">>";
}

void PdfDict::write(Stream &stream, const PdfRenumber *renumber) const noexcept
{
  dictWrite(stream, renumber);
  if (iStream.size() > 0) {
    stream << "\nstream\n";
    for (int i = 0; i < iStream.size(); ++i)
      stream.putChar(iStream[i]);
    stream << "\nendstream";
  }
}

PdfDict::~PdfDict()
{
  for (std::vector<Item>::iterator it = iItems.begin();
       it != iItems.end(); ++it) {
    delete it->iVal;
    it->iVal = 0;
  }
}

//! Add stream data to this dictionary.
void PdfDict::setStream(const Buffer &stream)
{
  iStream = stream;
}

//! Add a (key, value) pair to the dictionary.
/*! Dictionary takes ownership of \a obj. */
void PdfDict::add(String key, const PdfObj *obj)
{
  Item item;
  item.iKey = key;
  item.iVal = obj;
  iItems.push_back(item);
}

//! Look up key in dictionary.
/*! Indirect objects (references) are looked up if \a file is not nullptr,
  and the object referred to is returned.
  Returns nullptr if key is not in dictionary.
*/
const PdfObj *PdfDict::get(String key, const PdfFile *file) const noexcept
{
  for (std::vector<Item>::const_iterator it = iItems.begin();
       it != iItems.end(); ++it) {
    if (it->iKey == key) {
      if (file && it->iVal->ref())
	return file->object(it->iVal->ref()->value());
      else
	return it->iVal;
    }
  }
  return nullptr; // not in dictionary
}

//! Retrieve a single number and stor in \a val.
bool PdfDict::getNumber(String key, double &val, const PdfFile *file)
  const noexcept
{
  const PdfObj *obj = get(key, file);
  if (!obj || !obj->number())
    return false;
  val = obj->number()->value();
  return true;
}

//! Retrieve an array of numbers and store in \a vals.
bool PdfDict::getNumberArray(String key, const PdfFile *file,
			     std::vector<double> &vals) const noexcept
{
  const PdfObj *obj = get(key, file);
  if (!obj || !obj->array())
    return false;
  vals.clear();
  for (int i = 0; i < obj->array()->count(); i++) {
    const PdfObj *a = obj->array()->obj(i, file);
    if (!a || !a->number())
      return false;
    vals.push_back(a->number()->value());
  }
  return true;
}

//! Is this stream compressed with flate compression?
bool PdfDict::deflated() const noexcept
{
  const PdfObj *f = get("Filter", nullptr);
  return !(!f || !f->name() || f->name()->value() != "FlateDecode");
}

//! Return the (uncompressed) stream data.
/*! This only handles the /Flate compression. */
Buffer PdfDict::inflate() const noexcept
{
  if (iStream.size() == 0 || !deflated())
    return iStream;

  String dest;
  BufferSource bsource(iStream);
  InflateSource source(bsource);

  int ch = source.getChar();
  while (ch != EOF) {
    dest += char(ch);
    ch = source.getChar();
  }
  return Buffer(dest.data(), dest.size());
}

// --------------------------------------------------------------------

/*! \class ipe::PdfParser
 * \ingroup base
 * \brief PDF parser

 The parser understands the syntax of PDF files, but very little of
 its semantics.  It is meant to be able to parse PDF documents created
 by Ipe for loading, and to extract information from PDF files created
 by Pdflatex or Xelatex.

 The parser reads a PDF file sequentially from front to back, ignores
 the contents of 'xref' sections, stores only generation 0 objects,
 and stops after reading the first 'trailer' section (so it cannot
 deal with files with incremental updates).  It cannot handle stream
 objects whose /Length entry has been deferred (using an indirect
 object).

*/

//! Construct with a data source.
PdfParser::PdfParser(DataSource &source)
  : iSource(source)
{
  iPos = 0;
  getChar();  // init iCh
  getToken(); // init iTok
}

//! Skip white space and comments.
void PdfParser::skipWhiteSpace()
{
  while (!eos() && (specialChars[iCh] == 1 || iCh == '%')) {
    // handle comment
    if (iCh == '%') {
      while (!eos() && iCh != '\n' && iCh != '\r')
	getChar();
    }
    getChar();
  }
}

//! Read the next token from the input stream.
void PdfParser::getToken()
{
  iTok.iString.erase();
  iTok.iType = PdfToken::EErr;
  skipWhiteSpace();
  if (eos())
    return; // Err

  // parse string
  if (iCh == '(') {
    int nest = 0;
    getChar();
    while (iCh != ')' || nest > 0) {
      if (eos())
	return; // Err
      if (iCh == '\\') {
	getChar();
	if ('0' <= iCh && iCh <= '9') {
	  // octal char code
	  char buf[4];
	  int i = 0;
	  buf[i++] = char(iCh);
	  getChar();
	  if ('0' <= iCh && iCh <= '9') {
	    buf[i++] = char(iCh);
	    getChar();
	  }
	  if ('0' <= iCh && iCh <= '9') {
	    buf[i++] = char(iCh);
	    getChar();
	  }
	  buf[i] = '\0';
	  iTok.iString.append(char(std::strtol(buf, 0, 8)));
	} else {
	  iTok.iString.append(char(iCh));
	  getChar();
	}
      } else {
	if (iCh == '(')
	  ++nest;
	else if (iCh == ')')
	  --nest;
	iTok.iString.append(char(iCh));
	getChar();
      }
    }
    getChar(); // skip closing ')'
    iTok.iType = PdfToken::EString;
    return;
  }

  if (iCh == '<') {
    getChar();
    // recognize dictionary separator "<<"
    if (iCh == '<') {
      getChar();
      iTok.iType = PdfToken::EDictBg;
      return;
    }
    // otherwise it's a binary string <hex>
    while (iCh != '>') {
      if (eos())
	return; // Err
      iTok.iString.append(char(iCh));
      getChar();
    }
    // We don't bother to decode it
    getChar(); // skip '>'
    iTok.iType = PdfToken::EStringBinary;
    return;
  }

  int ch = iCh;

  iTok.iString.append(char(iCh));
  getChar();

  // recognize array separators
  if (ch == '[') {
    iTok.iType = PdfToken::EArrayBg;
    return;
  } else if (ch == ']') {
    iTok.iType = PdfToken::EArrayEnd;
    return;
  }

  // recognize dictionary separator ">>"
  if (ch == '>') {
    if (iCh != '>')
      return; // Err
    getChar();
    iTok.iType = PdfToken::EDictEnd;
    return;
  }

  // collect all characters up to white-space or separator
  while (!specialChars[iCh]) {
    if (eos())
      return; // Err
    iTok.iString.append(char(iCh));
    getChar();
  }

  if (('0' <= ch && ch <= '9') || ch == '+' || ch == '-' || ch == '.')
    iTok.iType = PdfToken::ENumber;
  else if (ch == '/')
    iTok.iType = PdfToken::EName;
  else if (iTok.iString == "null")
    iTok.iType = PdfToken::ENull;
  else if (iTok.iString == "true")
    iTok.iType = PdfToken::ETrue;
  else if (iTok.iString == "false")
    iTok.iType = PdfToken::EFalse;
  else
    iTok.iType = PdfToken::EOp;
}

// --------------------------------------------------------------------

//! Parse elements of an array.
PdfArray *PdfParser::makeArray()
{
  std::unique_ptr<PdfArray> arr(new PdfArray);
  for (;;) {
    if (iTok.iType == PdfToken::EArrayEnd) {
      // finish array
      getToken();
      return arr.release();
    }
    // check for reference object
    if (iTok.iType == PdfToken::ENumber) {
      PdfToken t1 = iTok;
      getToken();
      if (iTok.iType == PdfToken::ENumber) {
	PdfToken t2 = iTok;
	getToken();
	if (iTok.iType == PdfToken::EOp && iTok.iString == "R") {
	  arr->append(new PdfRef(toInt(t1.iString)));
	  getToken();
	} else {
	  arr->append(new PdfNumber(Platform::toDouble(t1.iString)));
	  arr->append(new PdfNumber(Platform::toDouble(t2.iString)));
	}
      } else {
	arr->append(new PdfNumber(Platform::toDouble(t1.iString)));
      }
    } else {
      PdfObj *obj = getObject();
      if (!obj)
	return nullptr;
      arr->append(obj);
    }
  }
}

PdfDict *PdfParser::makeDict()
{
  std::unique_ptr<PdfDict> dict(new PdfDict);
  for (;;) {
    if (iTok.iType == PdfToken::EDictEnd) {
      // finish
      getToken();

      // check whether stream follows
      if (iTok.iType != PdfToken::EOp || iTok.iString != "stream")
	return dict.release();

      // time to read the stream
      while (!eos() && iCh != '\n')
	getChar();
      getChar(); // skip '\n'
      // now at beginning of stream
      const PdfObj *len = dict->get("Length", 0);
      if (len && len->ref()) {
	ipeDebug("/Length entry of dictionary is a reference.");
	return nullptr;
      }
      if (!len || !len->number())
	return nullptr;
      int bytes = int(len->number()->value());
      Buffer buf(bytes);
      char *p = buf.data();
      while (bytes--) {
	*p++ = char(iCh);
	getChar();
      }
      dict->setStream(buf);
      getToken();
      if (iTok.iType != PdfToken::EOp || iTok.iString != "endstream")
	return nullptr;
      getToken();
      return dict.release();
    }

    // must read name
    if (iTok.iType != PdfToken::EName)
      return nullptr;
    String name = iTok.iString.substr(1);
    getToken();

    // check for reference object
    if (iTok.iType == PdfToken::ENumber) {
      PdfToken t1 = iTok;
      getToken();
      if (iTok.iType == PdfToken::ENumber) {
	PdfToken t2 = iTok;
	getToken();
	if (iTok.iType == PdfToken::EOp && iTok.iString == "R") {
	  dict->add(name, new PdfRef(toInt(t1.iString)));
	  getToken();
	} else
	  return nullptr; // should be name or '>>'
      } else
	dict->add(name, new PdfNumber(Platform::toDouble(t1.iString)));
    } else {
      PdfObj *obj = getObject();
      if (!obj)
	return nullptr;
      dict->add(name, obj);
    }
  }
}

//! Read one object from input stream.
PdfObj *PdfParser::getObject()
{
  PdfToken tok = iTok;
  getToken();

  switch (tok.iType) {
  case PdfToken::ENumber:
    return new PdfNumber(Platform::toDouble(tok.iString));
  case PdfToken::EString:
    return new PdfString(tok.iString);
  case PdfToken::EStringBinary:
    return new PdfString(tok.iString, true);
  case PdfToken::EName:
    return new PdfName(tok.iString.substr(1));
  case PdfToken::ENull:
    return new PdfNull;
  case PdfToken::ETrue:
    return new PdfBool(true);
  case PdfToken::EFalse:
    return new PdfBool(false);
  case PdfToken::EArrayBg:
    return makeArray();
  case PdfToken::EDictBg:
    return makeDict();
    // anything else is an error
  case PdfToken::EErr:
  default:
    return nullptr;
  }
}

//! Parse an object definition (current token is object number).
PdfObj *PdfParser::getObjectDef()
{
  getToken();
  if (iTok.iType != PdfToken::ENumber || iTok.iString != "0")
    return nullptr;
  getToken();
  if (iTok.iType != PdfToken::EOp || iTok.iString != "obj")
    return nullptr;
  getToken();
  PdfObj *obj = getObject();
  if (!obj)
    return nullptr;
  if (iTok.iType != PdfToken::EOp || iTok.iString != "endobj")
    return nullptr;
  getToken();
  return obj;
}

//! Skip xref table (current token is 'xref')
void PdfParser::skipXRef()
{
  getToken(); // first object number
  getToken(); // number of objects
  int k = toInt(iTok.iString);
  getToken();
  while (k--) {
    getToken(); // obj num
    getToken(); // gen num
    getToken(); // n or f
  }
}

//! Parse trailer dictionary (current token is 'trailer')
PdfDict *PdfParser::getTrailer()
{
  getToken();
  if (iTok.iType != PdfToken::EDictBg)
    return nullptr;
  getToken();
  return makeDict();
}

// --------------------------------------------------------------------

/*! \class ipe::PdfFile
 * \ingroup base
 * \brief All information obtained by parsing a PDF file.
 */

bool PdfFile::parseObjectStream(const PdfDict *d)
{
  const PdfObj *objn = d->get("N", this);
  const PdfObj *objfirst = d->get("First", this);
  int n = objn->number() ? objn->number()->value() : -1;
  int first = objfirst->number() ? objfirst->number()->value() : -1;
  if (n < 0 || first < 0)
    return false;
  Buffer stream = d->inflate();
  BufferSource source(stream);
  PdfParser parser(source);
  std::vector<int> dir;
  for (int i = 0; i < 2 * n; ++i) {
    PdfToken t = parser.token();
    if (t.iType != PdfToken::ENumber)
      return false;
    dir.push_back(toInt(t.iString));
    parser.getToken();
  }
  for (int i = 0; i < n; ++i) {
    int num = dir[2*i];
    source.setPosition(first + dir[2*i+1]);
    parser.getChar();
    parser.getToken();
    PdfObj *obj = parser.getObject();
    if (!obj)
      return false;
    iObjects[num] = std::unique_ptr<const PdfObj>(obj);
  }
  return true;
}

//! Parse entire PDF stream, and store objects.
bool PdfFile::parse(DataSource &source)
{
  PdfParser parser(source);

  for (;;) {
    PdfToken t = parser.token();

    if (t.iType == PdfToken::ENumber) {
      // <num> 0 obj starts an object
      int num = toInt(t.iString);
      std::unique_ptr<const PdfObj> obj(parser.getObjectDef());
      if (!obj) {
	ipeDebug("Failed to get object %d", num);
	return false;
      }
      const PdfDict *d = obj->dict();
      const PdfObj *type = d ? d->get("Type", this) : nullptr;
      if (type && type->name() && type->name()->value() == "ObjStm") {
	if (!parseObjectStream(d))
	  return false;
      } else if (type && type->name() && type->name()->value() == "XRef") {
	iTrailer = std::unique_ptr<const PdfDict>(obj.release()->dict());
      } else
	iObjects[num] = std::move(obj);
    } else if (t.iType == PdfToken::EOp) {
      if (t.iString == "trailer") {
	iTrailer = std::unique_ptr<const PdfDict>(parser.getTrailer());
	if (!iTrailer) {
	  ipeDebug("Failed to get trailer");
	  return false;
	}
	return true;
      } else if (t.iString == "xref") {
	parser.skipXRef();
      } else if (t.iString == "startxref") {
	return true; // this is the end
      } else {
	ipeDebug("Weird token: %s", t.iString.z());
	// don't know what's happening
	return false;
      }
    } else {
      ipeDebug("Weird token type: %d %s", t.iType, t.iString.z());
      // don't know what's happening
      return false;
    }
  }
}

//! Return object with number \a num.
const PdfObj *PdfFile::object(int num) const noexcept
{
  auto got = iObjects.find(num);
  if (got != iObjects.end())
    return got->second.get();
  else
    return nullptr;
}

//! Take ownership of object with number \a num, remove from PdfFile.
std::unique_ptr<const PdfObj> PdfFile::take(int num)
{
  auto got = iObjects.find(num);
  if (got != iObjects.end()) {
    std::unique_ptr<const PdfObj> obj = std::move(got->second);
    iObjects.erase(got);
    return obj;
  } else
    return std::unique_ptr<const PdfObj>();
}

//! Return root catalog of PDF file.
const PdfDict *PdfFile::catalog() const noexcept
{
  const PdfObj *root = iTrailer->get("Root", this);
  assert(root && root->dict());
  return root->dict();
}

//! Return first page of the document.
const PdfDict *PdfFile::page() const noexcept
{
  const PdfObj *pages = catalog()->get("Pages", this);
  assert(pages && pages->dict());
  const PdfObj *kids = pages->dict()->get("Kids", this);
  assert(kids);
  if (!kids->array())
    return nullptr;
  const PdfObj *page = kids->array()->obj(0, this);
  // this should be page 1
  if (!page || !page->dict())
    return nullptr;
  return page->dict();
}

// --------------------------------------------------------------------
