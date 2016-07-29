// -*- C++ -*-
// --------------------------------------------------------------------
// Special widgets for QT
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

#ifndef CONTROLS_QT_H
#define CONTROLS_QT_H

#include "ipelib.h"

#include <QListWidget>

using namespace ipe;

// --------------------------------------------------------------------

class LayerBox : public QListWidget {
  Q_OBJECT
public:
  LayerBox(QWidget *parent = 0);

  void set(const Page *page, int view);

signals:
  void activated(String name, String layer);
  void showLayerBoxPopup(Vector v, String layer);

public slots:
  void layerChanged(QListWidgetItem *item);

protected:
  virtual void mouseReleaseEvent(QMouseEvent *e);
  virtual void mousePressEvent(QMouseEvent *e);

  void addAction(QMenu *m, QListWidgetItem *item, String name,
		 const QString &text);

private:
  bool iInSet;
};

// --------------------------------------------------------------------

class PathView : public QWidget {
  Q_OBJECT

public:
#if QT_VERSION < 0x050000
  PathView(QWidget* parent = 0, Qt::WFlags f = 0);
#else
  PathView(QWidget* parent = 0, Qt::WindowFlags f = 0);
#endif

  void set(const AllAttributes &all, Cascade *sheet);

  virtual QSize sizeHint() const;

signals:
  void activated(String name);
  void showPathStylePopup(Vector v);

protected:
  virtual void paintEvent(QPaintEvent *ev);
  virtual void mouseReleaseEvent(QMouseEvent *e);
  virtual void mousePressEvent(QMouseEvent *e);
  virtual bool event(QEvent *ev);

private:
  Cascade *iCascade;
  AllAttributes iAll;
};

// --------------------------------------------------------------------

class PageSorter : public QListWidget {
  Q_OBJECT

public:
  PageSorter(Document *doc, int width, QWidget *parent = 0);

  int pageAt(int r) const;

public slots:
  void deletePages();
  void cutPages();
  void insertPages();

private:
  virtual void contextMenuEvent(QContextMenuEvent *event);

private:
  Document *iDoc;
  QList<QListWidgetItem *> iCutList;
  int iActionRow;
};

// --------------------------------------------------------------------
#endif
