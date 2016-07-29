// -*- C++ -*-
// --------------------------------------------------------------------
// A page of a document.
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

#ifndef IPEPAGE_H
#define IPEPAGE_H

#include "ipetext.h"

// --------------------------------------------------------------------

namespace ipe {

  class StyleSheet;

  // --------------------------------------------------------------------

  class Page {
  public:
    explicit Page();

    static Page *basic();

    void saveAsXml(Stream &stream) const;
    void saveAsIpePage(Stream &stream) const;
    void saveSelection(Stream &stream) const;

    //! Return number of layers.
    inline int countLayers() const noexcept { return iLayers.size(); }
    //! Return name of layer \a index.
    inline String layer(int index) const noexcept {
      return iLayers[index].iName; }

    //! Is layer \a i locked?
    inline bool isLocked(int i) const noexcept {
      return !!(iLayers[i].iFlags & ELocked); }
    //! Does layer \a i have active snapping?
    inline bool hasSnapping(int i) const noexcept {
      return !(iLayers[i].iFlags & ENoSnapping); }
    bool objSnapsInView(int objNo, int view) const noexcept;

    void setLocked(int i, bool flag);
    void setSnapping(int i, bool flag);

    void moveLayer(int index, int newIndex);
    int findLayer(String name) const;
    void addLayer(String name);
    void addLayer();
    void removeLayer(String name);
    void renameLayer(String oldName, String newName);

    //! Return number of views.
    inline int countViews() const { return iViews.size(); }
    int countMarkedViews() const;
    //! Return effect of view.
    inline Attribute effect(int index) const { return iViews[index].iEffect; }
    void setEffect(int index, Attribute sym);
    //! Return active layer of view.
    inline String active(int index) const { return iViews[index].iActive; }
    //! Set active layer of view.
    void setActive(int index, String name);
    //! Return if view is marked.
    bool markedView(int index) const { return iViews[index].iMarked; }
    //! Set if view is marked.
    void setMarkedView(int index, bool marked);

    void insertView(int i, String active);
    void removeView(int i);
    void clearViews();

    //! Is \a layer visible in \a view?
    inline bool visible(int view, int layer) const {
      return iLayers[layer].iVisible[view]; }
    //! Is object at index \a objno visible in \a view?
    inline bool objectVisible(int view, int objno) const {
      return iLayers[layerOf(objno)].iVisible[view]; }

    void setVisible(int view, String layer, bool vis);

    //! Return title of this page.
    String title() const;
    void setTitle(String title);
    String section(int level) const;
    void setSection(int level, bool useTitle, String name);
    //! Does this section title reflect the page title?
    bool sectionUsesTitle(int level) const { return iUseTitle[level]; }
    const Text *titleText() const;
    void applyTitleStyle(const Cascade *sheet);

    //! Return if page is marked for printing.
    bool marked() const { return iMarked; }
    void setMarked(bool marked);

    //! Return notes for this page.
    String notes() const { return iNotes; }
    void setNotes(String notes);

    //! Return number of objects on the page.
    inline int count() const { return int(iObjects.size()); }

    //! Return object at index \a i.
    inline Object *object(int i) { return iObjects[i].iObject; }
    //! Return object at index \a i (const version).
    inline const Object *object(int i) const { return iObjects[i].iObject; }

    //! Return selection status of object at index \a i.
    inline TSelect select(int i) const { return iObjects[i].iSelect; }
    //! Return layer of object at index \a i.
    inline int layerOf(int i) const { return iObjects[i].iLayer; }

    //! Set selection status of object at index \a i.
    inline void setSelect(int i, TSelect sel) { iObjects[i].iSelect = sel; }
    //! Set layer of object at index \a i.
    inline void setLayerOf(int i, int layer) { iObjects[i].iLayer = layer; }

    Rect pageBBox(const Cascade *sheet) const;
    Rect viewBBox(const Cascade *sheet, int view) const;
    Rect bbox(int i) const;

    void transform(int i, const Matrix &m);
    double distance(int i, const Vector &v, double bound) const;
    void snapVtx(int i, const Vector &mouse, Vector &pos, double &bound) const;
    void snapCtl(int i, const Vector &mouse, Vector &pos, double &bound) const;
    void snapBnd(int i, const Vector &mouse, Vector &pos, double &bound) const;
    void invalidateBBox(int i) const;

    void insert(int i, TSelect sel, int layer, Object *obj);
    void append(TSelect sel, int layer, Object *obj);
    void remove(int i);
    void replace(int i, Object *obj);
    bool setAttribute(int i, Property prop, Attribute value);

    int primarySelection() const;
    bool hasSelection() const;
    void deselectAll();
    void ensurePrimarySelection();

  private:
    enum { ELocked = 0x01, ENoSnapping = 0x02 };

    struct SLayer {
    public:
      SLayer(String name);
    public:
      String iName;
      int iFlags;
      // Invariant: iVisible.size() == iViews.size()
      std::vector<bool> iVisible;
    };
    typedef std::vector<SLayer> LayerSeq;

    struct SView {
    public:
      SView() { iEffect = Attribute::NORMAL(); }
    public:
      Attribute iEffect;
      String iActive;
      bool iMarked;
    };
    typedef std::vector<SView> ViewSeq;

    struct SObject {
      SObject();
      SObject(const SObject &rhs);
      ~SObject();
      SObject &operator=(const SObject &rhs);

      TSelect iSelect;
      int iLayer;
      mutable Rect iBBox;
      Object *iObject;
    };
    typedef std::vector<SObject> ObjSeq;

    LayerSeq iLayers;
    ViewSeq iViews;

    String iTitle;
    Text iTitleObject;
    bool iUseTitle[2];
    String iSection[2];
    ObjSeq iObjects;
    String iNotes;
    bool iMarked;
  };

} // namespace

// --------------------------------------------------------------------
#endif
