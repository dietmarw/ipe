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

#include "controls_qt.h"

#include "ipethumbs.h"

#include "ipecairopainter.h"

#include "ipecanvas_qt.h"

#include <QMenu>
#include <QContextMenuEvent>
#include <QPainter>
#include <QToolTip>

// --------------------------------------------------------------------

LayerBox::LayerBox(QWidget *parent)
  : QListWidget(parent)
{
  setFocusPolicy(Qt::NoFocus);
  setSelectionMode(NoSelection);
  iInSet = false;
  connect(this, SIGNAL(itemChanged(QListWidgetItem *)),
	  SLOT(layerChanged(QListWidgetItem *)));
}

void LayerBox::layerChanged(QListWidgetItem *item)
{
  if (iInSet)
    return;
  if (item->checkState() == Qt::Checked)
    emit activated("selecton", IpeQ(item->text()));
  else
    emit activated("selectoff", IpeQ(item->text()));
}

void LayerBox::set(const Page *page, int view)
{
  iInSet = true;
  clear();
  for (int i = 0; i < page->countLayers(); ++i) {
    // int i = idx[j].second;
    QListWidgetItem *item = new QListWidgetItem(QIpe(page->layer(i)), this);
    item->setFlags(Qt::ItemIsUserCheckable|Qt::ItemIsEnabled);
    item->setCheckState(page->visible(view, i) ? Qt::Checked : Qt::Unchecked);
    if (page->layer(i) == page->active(view))
      item->setBackgroundColor(Qt::yellow);
    if (page->isLocked(i))
      item->setBackground(QColor(255, 220, 220));
    if (!page->hasSnapping(i))
      item->setForeground(Qt::gray);
  }
  iInSet = false;
}

void LayerBox::mousePressEvent(QMouseEvent *ev)
{
  QListWidgetItem *item = itemAt(ev->pos());
  if (item && ev->button()== Qt::LeftButton && ev->x() > 30) {
    emit activated("active", IpeQ(item->text()));
  }
  QListWidget::mousePressEvent(ev);
}

void LayerBox::mouseReleaseEvent(QMouseEvent *ev)
{
  QListWidgetItem *item = itemAt(ev->pos());
  if (item && ev->button() == Qt::RightButton) {
    // make popup menu
    emit showLayerBoxPopup(Vector(ev->globalPos().x(), ev->globalPos().y()),
			   IpeQ(item->text()));
  } else
    QListWidget::mouseReleaseEvent(ev);
}

// --------------------------------------------------------------------

#if QT_VERSION < 0x050000
PathView::PathView(QWidget* parent, Qt::WFlags f) : QWidget(parent, f)
#else
PathView::PathView(QWidget* parent, Qt::WindowFlags f) : QWidget(parent, f)
#endif
{
  iCascade = 0;
}

void PathView::set(const AllAttributes &all, Cascade *sheet)
{
  iCascade = sheet;
  iAll = all;
  update();
}

void PathView::paintEvent(QPaintEvent *ev)
{
  QSize s = size();
  int w = s.width();
  int h = s.height();

  cairo_surface_t *sf = cairo_image_surface_create(CAIRO_FORMAT_RGB24, w, h);
  cairo_t *cc = cairo_create(sf);
  cairo_set_source_rgb(cc, 1, 1, 0.8);
  cairo_rectangle(cc, 0, 0, w, h);
  cairo_fill(cc);

  if (iCascade) {
    cairo_translate(cc, 0, h);
    double zoom = w / 70.0;
    cairo_scale(cc, zoom, -zoom);
    Vector v0 = (1.0/zoom) * Vector(0.1 * w, 0.5 * h);
    Vector v1 = (1.0/zoom) * Vector(0.7 * w, 0.5 * h);
    Vector u1 = (1.0/zoom) * Vector(0.88 * w, 0.8 * h);
    Vector u2 = (1.0/zoom) * Vector(0.80 * w, 0.5 * h);
    Vector u3 = (1.0/zoom) * Vector(0.88 * w, 0.2 * h);
    Vector u4 = (1.0/zoom) * Vector(0.96 * w, 0.5 * h);

    CairoPainter painter(iCascade, 0, cc, 3.0, false);
    painter.setPen(iAll.iPen);
    painter.setDashStyle(iAll.iDashStyle);
    painter.setStroke(iAll.iStroke);
    painter.setFill(iAll.iFill);
    painter.pushMatrix();
    painter.newPath();
    painter.moveTo(v0);
    painter.lineTo(v1);
    painter.drawPath(EStrokedOnly);
    if (iAll.iFArrow)
      Path::drawArrow(painter, v1, Angle(0),
		      iAll.iFArrowShape, iAll.iFArrowSize, 100.0);
    if (iAll.iRArrow)
      Path::drawArrow(painter, v0, Angle(IpePi),
		      iAll.iRArrowShape, iAll.iRArrowSize, 100.0);
    painter.setDashStyle(Attribute::NORMAL());
    painter.setTiling(iAll.iTiling);
    painter.newPath();
    painter.moveTo(u1);
    painter.lineTo(u2);
    painter.lineTo(u3);
    painter.lineTo(u4);
    painter.closePath();
    painter.drawPath(iAll.iPathMode);
    painter.popMatrix();
  }

  cairo_surface_flush(sf);
  cairo_destroy(cc);

  QPainter qPainter;
  qPainter.begin(this);
  QRect r = ev->rect();
  QImage bits(cairo_image_surface_get_data(sf), w, h, QImage::Format_RGB32);
  QRect source(r.left(), r.top(), r.width(), r.height());
  qPainter.drawImage(r, bits, source);
  qPainter.end();
  cairo_surface_destroy(sf);
}

QSize PathView::sizeHint() const
{
  return QSize(120, size().width() / 8.0);
}

void PathView::mousePressEvent(QMouseEvent *ev)
{
  QSize s = size();
  if (ev->button()== Qt::LeftButton && ev->x() < s.width() * 3 / 10) {
    emit activated(iAll.iRArrow ? "rarrow|false" : "rarrow|true");
  } else if (ev->button()== Qt::LeftButton &&
	     ev->x() > s.width() * 4 / 10 &&
	     ev->x() < s.width() * 72 / 100) {
    emit activated(iAll.iFArrow ? "farrow|false" : "farrow|true");
    update();
  } else if (ev->button()== Qt::LeftButton && ev->x() > s.width() * 78 / 100) {
    switch (iAll.iPathMode) {
    case EStrokedOnly:
      emit activated("pathmode|strokedfilled");
      break;
    case EStrokedAndFilled:
      emit activated("pathmode|filled");
      break;
    case EFilledOnly:
      emit activated("pathmode|stroked");
      break;
    }
    update();
  }
}

void PathView::mouseReleaseEvent(QMouseEvent *ev)
{
  if (ev->button()== Qt::RightButton)
    emit showPathStylePopup(Vector(ev->globalPos().x(), ev->globalPos().y()));
}

bool PathView::event(QEvent *ev)
{
  if (ev->type() != QEvent::ToolTip)
    return QWidget::event(ev);

  QHelpEvent *hev = (QHelpEvent *) ev;
  QSize s = size();

  QString tip;
  if (hev->x() < s.width() * 3 / 10)
    tip = "Toggle reverse arrow";
  else if (hev->x() > s.width() * 4 / 10 &&
	   hev->x() < s.width() * 72 / 100)
    tip = "Toggle forward arrow";
  else if (hev->x() > s.width() * 78 / 100)
    tip = "Toggle stroked/stroked & filled/filled";

  if (!tip.isNull())
    QToolTip::showText(hev->globalPos(), tip, this);
  return true;
}

// --------------------------------------------------------------------

PageSorter::PageSorter(Document *doc, int itemWidth, QWidget *parent)
  : QListWidget(parent)
{
  iDoc = doc;
  setViewMode(QListView::IconMode);
  setSelectionMode(QAbstractItemView::ExtendedSelection);
  setResizeMode(QListView::Adjust);
  setWrapping(true);
  setUniformItemSizes(true);
  setFlow(QListView::LeftToRight);
  setSpacing(10);
  setMovement(QListView::Static);

  Thumbnail r(iDoc, itemWidth);
  setGridSize(QSize(itemWidth, r.height() + 50));
  setIconSize(QSize(itemWidth, r.height()));

  for (int i = 0; i < doc->countPages(); ++i) {
    Page *p = doc->page(i);
    Buffer b = r.render(p, p->countViews() - 1);
    QImage bits((const uchar *) b.data(), itemWidth, r.height(),
		QImage::Format_RGB32);
    // need to copy bits since buffer b is temporary
    QIcon icon(QPixmap::fromImage(bits.copy()));

    QString s;
    QString t = QString::fromUtf8(p->title().z());
    if (t != "") {
      s.sprintf("%d: ", i+1);
      s += t;
    } else {
      s.sprintf("Page %d", i+1);
    }
    QListWidgetItem *item = new QListWidgetItem(icon, s);
    item->setFlags(Qt::ItemIsSelectable|Qt::ItemIsEnabled);
    item->setToolTip(s);
    item->setData(Qt::UserRole, QVariant(i)); // page number
    addItem(item);
  }
}

int PageSorter::pageAt(int r) const
{
  return item(r)->data(Qt::UserRole).toInt();
}

void PageSorter::deletePages()
{
  QList<QListWidgetItem *> items = selectedItems();
  for (int i = 0; i < items.count(); ++i) {
    int r = row(items[i]);
    QListWidgetItem *item = takeItem(r);
    delete item;
  }
}

void PageSorter::cutPages()
{
  // delete items in old cut list
  for (int i = 0; i < iCutList.count(); ++i)
    delete iCutList[i];
  iCutList.clear();

  QList<QListWidgetItem *> items = selectedItems();
  for (int i = 0; i < items.count(); ++i) {
    int r = row(items[i]);
    QListWidgetItem *item = takeItem(r);
    iCutList.append(item);
  }
}

void PageSorter::insertPages()
{
  // deselect everything
  for (int i = 0; i < count(); ++i)
    item(i)->setSelected(false);
  int r = (iActionRow >= 0) ? iActionRow : count();
  for (int i = 0; i < iCutList.count(); ++i) {
    insertItem(r, iCutList[i]);
    item(r++)->setSelected(true);
  }
  iCutList.clear();
}

void PageSorter::contextMenuEvent(QContextMenuEvent *ev)
{
  ev->accept();

  QListWidgetItem *item = itemAt(ev->pos());
  iActionRow = row(item);

  QMenu *m = new QMenu();
  QAction *action_delete = new QAction("&Delete", this);
  connect(action_delete, SIGNAL(triggered()), SLOT(deletePages()));
  QAction *action_cut = new QAction("&Cut", this);
  connect(action_cut, SIGNAL(triggered()), SLOT(cutPages()));
  QAction *action_insert = new QAction("&Insert", this);
  connect(action_insert, SIGNAL(triggered()), SLOT(insertPages()));
  if (selectedItems().count() > 0) {
    m->addAction(action_delete);
    m->addAction(action_cut);
  }
  if (iCutList.count() > 0)
    m->addAction(action_insert);
  m->exec(ev->globalPos());
  delete m;
}

// --------------------------------------------------------------------
