// -*- C++ -*-
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

#ifndef IPEBITMAP_H
#define IPEBITMAP_H

#include "ipebase.h"
#include "ipegeo.h"
#include "ipexml.h"

// --------------------------------------------------------------------

namespace ipe {

  class Bitmap {
  public:
    enum TFilter { EDirect, EFlateDecode, EDCTDecode };
    enum TColorSpace { EDeviceRGB, EDeviceGray, EDeviceCMYK };

    class MRenderData {
    public:
      virtual ~MRenderData();
    };

    Bitmap();
    Bitmap(int width, int height,
	   TColorSpace colorSpace, int bitsPerComponent,
	   Buffer data, TFilter filter, bool deflate = false);
    Bitmap(const XmlAttributes &attr, String data);
    Bitmap(const XmlAttributes &attr, Buffer data);

    Bitmap(const Bitmap &rhs);
    ~Bitmap();
    Bitmap &operator=(const Bitmap &rhs);

    void saveAsXml(Stream &stream, int id, int pdfObjNum = -1) const;

    inline bool isNull() const;
    bool equal(Bitmap rhs) const;

    inline TColorSpace colorSpace() const;
    inline TFilter filter() const;
    inline int components() const;
    inline int bitsPerComponent() const;
    inline int width() const;
    inline int height() const;

    int colorKey() const;
    void setColorKey(int key);

    inline const char *data() const;
    inline int size() const;

    inline int objNum() const;
    inline void setObjNum(int objNum) const;

    inline MRenderData *renderData() const;
    void setRenderData(MRenderData *data) const;

    Buffer pixelData() const;

    inline bool operator==(const Bitmap &rhs) const;
    inline bool operator!=(const Bitmap &rhs) const;
    inline bool operator<(const Bitmap &rhs) const;

    static const char *readJpegInfo(FILE *file, int &width, int &height,
				    Vector &dotsPerInch,
				    TColorSpace &colorSpace,
				    int &bitsPerComponent);

    static Bitmap readJpeg(const char *fname, Vector &dotsPerInch,
			   const char * &errmsg);
    static Bitmap readPNG(const char *fname, bool deflate,
			  Vector &dotsPerInch, const char * &errmsg);

  private:
    int init(const XmlAttributes &attr);
    void computeChecksum();

  private:
    struct Imp {
      int iRefCount;
      TColorSpace iColorSpace;
      int iBitsPerComponent;
      int iWidth;
      int iHeight;
      int iComponents;
      int iColorKey;
      Buffer iData;
      TFilter iFilter;
      int iChecksum;
      mutable int iObjNum;           // Object number (e.g. in PDF file)
      mutable MRenderData *iRender;  // cached pixmap to render it fast
    };

    Imp *iImp;
  };

  // --------------------------------------------------------------------

  //! Is this a null bitmap?
  inline bool Bitmap::isNull() const
  {
    return (iImp == 0);
  }

  //! Return the color space of the image.
  inline Bitmap::TColorSpace Bitmap::colorSpace() const
  {
    return iImp->iColorSpace;
  }

  //! Return number of components per pixel.
  inline int Bitmap::components() const
  {
    return iImp->iComponents;
  }

  //! Return the number of bits per component.
  inline int Bitmap::bitsPerComponent() const
  {
    return iImp->iBitsPerComponent;
  }

  //! Return width of pixel array.
  inline int Bitmap::width() const
  {
    return iImp->iWidth;
  }

  //! Return height of pixel array.
  inline int Bitmap::height() const
  {
    return iImp->iHeight;
  }

  //! Return the data filter of the image data.
  inline Bitmap::TFilter Bitmap::filter() const
  {
    return iImp->iFilter;
  }

  //! Return a pointer to the image data (in PDF arrangement).
  inline const char *Bitmap::data() const
  {
    return iImp->iData.data();
  }

  //! Return size (number of bytes) of image data (in PDF arrangement).
  inline int Bitmap::size() const
  {
    return iImp->iData.size();
  }

  //! Return object number of the bitmap.
  inline int Bitmap::objNum() const
  {
    return iImp->iObjNum;
  }

  //! Set object number of the bitmap.
  inline void Bitmap::setObjNum(int objNum) const
  {
    iImp->iObjNum = objNum;
  }

  //! Return cached bitmap for rendering.
  inline Bitmap::MRenderData *Bitmap::renderData() const
  {
    return iImp->iRender;
  }

  //! Two bitmaps are equal if they share the same data.
  inline bool Bitmap::operator==(const Bitmap &rhs) const
  {
    return iImp == rhs.iImp;
  }

  //! Two bitmaps are equal if they share the same data.
  inline bool Bitmap::operator!=(const Bitmap &rhs) const
  {
    return iImp != rhs.iImp;
  }

  //! Less operator, to be able to sort bitmaps.
  /*! The checksum is used, when it is equal, the shared address.
    This guarantees that bitmaps that are == (share their implementation)
    are next to each other, and blocks of them are next to blocks that
    are identical in contents. */
  inline bool Bitmap::operator<(const Bitmap &rhs) const
  {
    return (iImp->iChecksum < rhs.iImp->iChecksum ||
	    (iImp->iChecksum == rhs.iImp->iChecksum && iImp < rhs.iImp));
  }

} // namespace

// --------------------------------------------------------------------
#endif
