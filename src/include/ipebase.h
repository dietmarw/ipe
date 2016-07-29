// -*- C++ -*-
// --------------------------------------------------------------------
// Base header file --- must be included by all Ipe components.
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

#ifndef IPEBASE_H
#define IPEBASE_H

#include <cstdio>
#include <cstring>
#include <vector>
#include <map>
#include <unordered_map>
#include <list>
#include <algorithm>
#include <memory>

#ifdef IPESTRICT
#include "ipeosx.h"
#endif

// --------------------------------------------------------------------

#define IPE_PI 3.14159265358979323846

using uchar = unsigned char;
using uint = unsigned int;

#ifdef WIN32
#define IPESEP ('\\')
#else
#define IPESEP ('/')
#endif

#undef assert
#define assert(e) ((e) ? (void)0 : ipeAssertionFailed(__FILE__, __LINE__, #e))
extern void ipeAssertionFailed(const char *, int, const char *);

// --------------------------------------------------------------------

extern void ipeDebug(const char *msg, ...) noexcept;

namespace ipe {

  //! Ipelib version.
  /*! \ingroup base */
  const int IPELIB_VERSION = 70205;

  //! Oldest readable file format version.
  /*! \ingroup base */
  const int OLDEST_FILE_FORMAT = 70000;
  //! Current file format version.
  /*! \ingroup base */
  const int FILE_FORMAT = 70107;
  const int FILE_FORMAT_NEW = 70205; // group url, pagenumberstyle, decoration

  enum class LatexType { Default, Pdftex, Xetex, Luatex };

  // --------------------------------------------------------------------

  class Stream;

  class String {
  public:
    String() noexcept;
    String(const char *str) noexcept;
    String(const char *str, int len) noexcept;
    String(const String &rhs) noexcept;
    String(const String &rhs, int index, int len) noexcept;
    String &operator=(const String &rhs) noexcept;
#ifdef WIN32
    String(const wchar_t *data) noexcept;
#endif
    ~String() noexcept;
    static String withData(char *data, int len = 0) noexcept;
    //! Return character at index i.
    inline char operator[](int i) const noexcept { return iImp->iData[i]; }
    //! Is the string empty?
    inline bool empty() const noexcept { return (size() == 0); }
    //! Return read-only pointer to the data.
    const char *data() const noexcept { return iImp->iData; }
    //! Return number of bytes in the string.
    inline int size() const noexcept { return iImp->iSize; }
    //! Operator syntax for append.
    inline void operator+=(const String &rhs) noexcept { append(rhs); }
    //! Operator syntax for append.
    inline void operator+=(const char *rhs) noexcept { append(rhs); }
    //! Operator syntax for append.
    void operator+=(char ch) noexcept { append(ch); }
    //! Create substring.
    inline String substr(int i, int len = -1) const noexcept {
      return String(*this, i, len); }
    //! Create substring at the left.
    inline String left(int i) const noexcept {
      return String(*this, 0, i); }
    String right(int i) const noexcept;
    //! Operator !=.
    inline bool operator!=(const String &rhs) const noexcept {
      return !(*this == rhs); }
    //! Operator !=.
    inline bool operator!=(const char *rhs) const noexcept {
      return !(*this == rhs); }

    int find(char ch) const noexcept;
    int rfind(char ch) const noexcept;
    int find(const char *rhs) const noexcept;
    void erase() noexcept;
    void append(const String &rhs) noexcept;
    void append(const char *rhs) noexcept;
    void append(char ch) noexcept;
    bool hasPrefix(const char *rhs) const noexcept;
    bool operator==(const String &rhs) const noexcept;
    bool operator==(const char *rhs) const noexcept;
    bool operator<(const String &rhs) const noexcept;
    String operator+(const String &rhs) const noexcept;
    int unicode(int &index) const noexcept;
    String getLine(int &index) const noexcept;
    const char *z() const noexcept;
  private:
    void detach(int n) noexcept;
  private:
    struct Imp {
      int iRefCount;
      int iSize;
      int iCapacity;
      char *iData;
    };
    static Imp *theEmptyString;
    static Imp *emptyString() noexcept;
    Imp *iImp;
  };

  // --------------------------------------------------------------------

  class Fixed {
  public:
    explicit Fixed(int val) : iValue(val * 1000) { /* nothing */ }
    explicit Fixed() { /* nothing */ }
    inline static Fixed fromInternal(int val);
    static Fixed fromDouble(double val);
    inline int toInt() const { return iValue / 1000; }
    inline double toDouble() const { return iValue / 1000.0; }
    inline int internal() const { return iValue; }
    Fixed mult(int a, int b) const;
    inline bool operator==(const Fixed &rhs) const;
    inline bool operator!=(const Fixed &rhs) const;
    inline bool operator<(const Fixed &rhs) const;
  private:
    int iValue;

    friend Stream &operator<<(Stream &stream, const Fixed &f);
  };

  // --------------------------------------------------------------------

  class Lex {
  public:
    explicit Lex(String str);

    String token();
    String nextToken();
    int getInt();
    int getHexByte();
    Fixed getFixed();
    unsigned long int getHexNumber();
    double getDouble();
    //! Extract next character (not skipping anything).
    inline char getChar() {
      return iString[iPos++]; }
    void skipWhitespace();

    //! Operator syntax for getInt().
    inline Lex &operator>>(int &i) {
      i = getInt(); return *this; }
    //! Operator syntax for getDouble().
    inline Lex &operator>>(double &d) {
      d = getDouble(); return *this; }

    //! Operator syntax for getFixed().
    inline Lex &operator>>(Fixed &d) {
      d = getFixed(); return *this; }

    //! Mark the current position.
    inline void mark() {
      iMark = iPos; }
    //! Reset reader to the marked position.
    inline void fromMark() {
      iPos = iMark; }

    //! Return true if at end of string (not even whitespace left).
    inline bool eos() const {
      return (iPos == iString.size()); }

  private:
    String iString;
    int iPos;
    int iMark;
  };

  // --------------------------------------------------------------------

  class Buffer {
  public:
    Buffer() noexcept = default;
    ~Buffer() noexcept;
    Buffer(const Buffer &rhs) noexcept;
    Buffer(Buffer &&rhs) noexcept;
    Buffer &operator=(const Buffer &rhs) noexcept;
    Buffer &operator=(Buffer &&rhs) noexcept;

    explicit Buffer(int size) noexcept;
    explicit Buffer(const char *data, int size) noexcept;
    //! Character access.
    inline char &operator[](int index) noexcept { return iImp->iData[index]; }
    //! Character access (const version).
    inline const char &operator[](int index) const noexcept {
      return iImp->iData[index]; }
    //! Return size of buffer;
    inline int size() const noexcept { return iImp ? iImp->iSize : 0; }
    //! Return pointer to buffer data.
    inline char *data() noexcept { return iImp->iData; }
    //! Return pointer to buffer data (const version).
    inline const char *data() const noexcept { return iImp->iData; }

  private:
    struct Imp {
      int iRefCount;
      char *iData;
      int iSize;
    };
    Imp *iImp = { nullptr };
  };

  // --------------------------------------------------------------------

  class Stream {
  public:
    //! Virtual destructor.
    virtual ~Stream();
    //! Output character.
    virtual void putChar(char ch) = 0;
    //! Close the stream.  No more writing allowed!
    virtual void close();
    //! Output string.
    virtual void putString(String s);
    //! Output C string.
    virtual void putCString(const char *s);
  //! Output raw character data.
    virtual void putRaw(const char *data, int size);
    //! Output character.
    inline Stream &operator<<(char ch) { putChar(ch); return *this; }
    //! Output string.
    inline Stream &operator<<(const String &s) { putString(s); return *this; }
    //! Output C string.
    inline Stream &operator<<(const char *s) { putCString(s); return *this; }
    Stream &operator<<(int i);
    Stream &operator<<(double d);
    void putHexByte(char b);
    void putXmlString(String s);
  };

  /*! \class ipe::TellStream
    \ingroup base
    \brief Adds position feedback to IpeStream.
  */

  class TellStream : public Stream {
  public:
    virtual long tell() const = 0;
  };

  class StringStream : public TellStream {
  public:
    StringStream(String &string);
    virtual void putChar(char ch);
    virtual void putString(String s);
    virtual void putCString(const char *s);
    virtual void putRaw(const char *data, int size);
    virtual long tell() const;
  private:
    String &iString;
  };

  class FileStream : public TellStream {
  public:
    FileStream(std::FILE *file);
    virtual void putChar(char ch);
    virtual void putString(String s);
    virtual void putCString(const char *s);
    virtual void putRaw(const char *data, int size);
    virtual long tell() const;
  private:
    std::FILE *iFile;
  };

  // --------------------------------------------------------------------

  class DataSource {
  public:
    virtual ~DataSource() = 0;
    //! Get one more character, or EOF.
    virtual int getChar() = 0;
  };

  class FileSource : public DataSource {
  public:
    FileSource(std::FILE *file);
    virtual int getChar();
  private:
    std::FILE *iFile;
  };

  class BufferSource : public DataSource {
  public:
    BufferSource(const Buffer &buffer);
    virtual int getChar();
    void setPosition(int pos);
  private:
    const Buffer &iBuffer;
    int iPos;
  };

  // --------------------------------------------------------------------

  class Platform {
  public:
    using DebugHandler = void (*)(const char *);

#ifdef IPEBUNDLE
    static String ipeDir(const char *suffix, const char *fname = 0);
#endif
#ifdef WIN32
    static String winConv(const String &utf8, const String &encoding);
    static void utf8ToWide(const char *utf8, std::vector<wchar_t> &wbuf);
    static FILE *fopen(const char *fname, const char *mode);
    static int mkdir(const char *dname);
#else
    static FILE *fopen(const char *fname, const char *mode) {
      return ::fopen(fname, mode);
    }
#endif
    static int libVersion();
    static void initLib(int version);
    static void setDebug(bool debug);
    static String currentDirectory();
    static String latexDirectory();
    static String latexPath();
    static bool fileExists(String fname);
    static bool listDirectory(String path, std::vector<String> &files);
    static String readFile(String fname);
    static int runLatex(String dir, LatexType engine) noexcept;
    static double toDouble(String s);
  };

  // --------------------------------------------------------------------

  inline bool Fixed::operator==(const Fixed &rhs) const
  {
    return iValue == rhs.iValue;
  }

  inline bool Fixed::operator!=(const Fixed &rhs) const
  {
    return iValue != rhs.iValue;
  }

  inline bool Fixed::operator<(const Fixed &rhs) const
  {
    return iValue < rhs.iValue;
  }

  inline Fixed Fixed::fromInternal(int val)
  {
    Fixed f;
    f.iValue = val;
    return f;
  }

} // namespace

// --------------------------------------------------------------------
#endif
