// --------------------------------------------------------------------
// Lua bindings for Qt dialogs
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

#include "ipeui_qt.h"

#include <QApplication>
#include <QCheckBox>
#include <QCloseEvent>
#include <QColorDialog>
#include <QComboBox>
#include <QDateTime>
#include <QDialog>
#include <QFileDialog>
#include <QGridLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QShortcut>
#include <QSyntaxHighlighter>
#include <QTextEdit>
#include <QThread>
#include <QTimer>

// --------------------------------------------------------------------

inline void push_string(lua_State *L, const QString &str)
{
  lua_pushstring(L, str.toUtf8());
}

inline QString tostring(lua_State *L, int i)
{
  return QString::fromUtf8(lua_tostring(L, i));
}

inline QString checkstring(lua_State *L, int i)
{
  return QString::fromUtf8(luaL_checkstring(L, i));
}

// --------------------------------------------------------------------

class XmlHighlighter : public QSyntaxHighlighter
{
public:
  XmlHighlighter(QTextEdit *textEdit);
protected:
  void applyFormat(const QString &text, QRegExp &exp,
		   const QTextCharFormat &format);
  virtual void highlightBlock(const QString &text);
};

void XmlHighlighter::applyFormat(const QString &text, QRegExp &exp,
				 const QTextCharFormat &format)
{
  int index = text.indexOf(exp);
  while (index >= 0) {
    int length = exp.matchedLength();
    setFormat(index, length, format);
    index = text.indexOf(exp, index + length);
  }
}

void XmlHighlighter::highlightBlock(const QString &text)
{
  QTextCharFormat tagFormat, stringFormat, numberFormat;
  tagFormat.setFontWeight(QFont::Bold);
  tagFormat.setForeground(Qt::blue);
  stringFormat.setForeground(Qt::darkMagenta);
  numberFormat.setForeground(Qt::red);

  QRegExp tagExp( "<.*>" );
  QRegExp stringExp( "\"[a-zA-Z]*\"" );
  QRegExp numberExp( "[+|-]*[0-9]*.[0-9][0-9]*" );
  applyFormat(text, tagExp, tagFormat);
  applyFormat(text, stringExp, stringFormat);
  applyFormat(text, numberExp, numberFormat);
}

XmlHighlighter::XmlHighlighter(QTextEdit *textEdit)
  : QSyntaxHighlighter(textEdit)
{
  // nothing
}

// --------------------------------------------------------------------

class LatexHighlighter : public QSyntaxHighlighter
{
public:
  LatexHighlighter(QTextEdit *textEdit);
protected:
  void applyFormat(const QString &text, QRegExp &exp,
		   const QTextCharFormat &format);
  virtual void highlightBlock(const QString &text);
};

void LatexHighlighter::applyFormat(const QString &text, QRegExp &exp,
				   const QTextCharFormat &format)
{
  int index = text.indexOf(exp);
  while (index >= 0) {
    int length = exp.matchedLength();
    setFormat(index, length, format);
    index = text.indexOf(exp, index + length);
  }
}

void LatexHighlighter::highlightBlock(const QString &text)
{
  QTextCharFormat mathFormat, tagFormat;
  mathFormat.setForeground(Qt::red);
  tagFormat.setFontWeight(QFont::Bold);
  tagFormat.setForeground(Qt::blue);

  QRegExp mathExp( "\\$[^$]+\\$" );
  QRegExp tagExp( "\\\\[a-zA-Z]+" );
  applyFormat(text, mathExp, mathFormat);
  applyFormat(text, tagExp, tagFormat);
}

LatexHighlighter::LatexHighlighter(QTextEdit *textEdit)
  : QSyntaxHighlighter(textEdit)
{
  // nothing
}

// --------------------------------------------------------------------

PDialog::PDialog(lua_State *L0, WINID parent, const char *caption)
  : QDialog(parent), Dialog(L0, parent, caption)
{
  setWindowTitle(caption);
  QVBoxLayout *vlo = new QVBoxLayout;
  setLayout(vlo);
  iGrid = new QGridLayout;
  vlo->addLayout(iGrid);
  iButtonArea = new QHBoxLayout;
  vlo->addLayout(iButtonArea);
  iButtonArea->addStretch(1);
  QShortcut *shortcut = new QShortcut(QKeySequence("Ctrl+Return"), this);
  connect(shortcut, SIGNAL(activated()), SLOT(accept()));
}

PDialog::~PDialog()
{
  //
}

void PDialog::keyPressEvent(QKeyEvent *e)
{
  if (iIgnoreEscape && e->key() == Qt::Key_Escape)
    return;
  QDialog::keyPressEvent(e);
}

void PDialog::notifyLua()
{
  QObject *s = sender();
  for (uint i = 0; i < iWidgets.size(); ++i) {
    if (s == iWidgets[i]) {
      callLua(iElements[i].lua_method);
      return;
    }
  }
}

static void markupLog(QTextEdit *t, const QString &text)
{
  QTextDocument *doc = new QTextDocument(t);
  doc->setPlainText(text);
  QTextCursor cursor(doc);
  int curPos = 0;
  int errNo = 0;
  for (;;) {
    int nextErr = text.indexOf(QLatin1String("\n!"), curPos);
    if (nextErr < 0)
      break;

    int lines = 0;
    while (curPos < nextErr + 1) {
      if (text[curPos++] == QLatin1Char('\n'))
	++lines;
    }
    cursor.movePosition(QTextCursor::Down, QTextCursor::MoveAnchor, lines);
    int pos = cursor.position();
    cursor.movePosition(QTextCursor::Down);
    cursor.setPosition(pos, QTextCursor::KeepAnchor);
    ++errNo;
    QString s;
    s.sprintf("err%d", errNo);
    QTextCharFormat format;
    format.setBackground(Qt::yellow);
    format.setAnchorName(s);
    format.setAnchor(true);
    cursor.setCharFormat(format);
  }
  t->setDocument(doc);
  t->scrollToAnchor(QLatin1String("err1"));
}

void PDialog::setMapped(lua_State *L, int idx)
{
  SElement &m = iElements[idx];
  QWidget *w = iWidgets[idx];
  switch (m.type) {
  case ELabel:
    (qobject_cast<QLabel *>(w))->setText(QString::fromUtf8(m.text.c_str()));
    break;
  case ECheckBox:
    (qobject_cast<QCheckBox *>(w))->setChecked(m.value);
    break;
  case ETextEdit:
    (qobject_cast<QTextEdit *>(w))->setText(QString::fromUtf8(m.text.c_str()));
    break;
  case EInput:
    (qobject_cast<QLineEdit *>(w))->setText(QString::fromUtf8(m.text.c_str()));
    break;
  case EList:
    {
      QListWidget *l = qobject_cast<QListWidget *>(w);
      if (!lua_isnumber(L, 3)) {
	l->clear();
	for (int k = 0; k < int(m.items.size()); ++k)
	  l->addItem(QString::fromUtf8(m.items[k].c_str()));
      }
      l->setCurrentRow(m.value);
    }
    break;
  case ECombo:
    {
      QComboBox *b = qobject_cast<QComboBox *>(w);
      if (!lua_isnumber(L, 3)) {
	b->clear();
	for (int k = 0; k < int(m.items.size()); ++k)
	  b->addItem(QString::fromUtf8(m.items[k].c_str()));
      }
      b->setCurrentIndex(m.value);
    }
    break;
  default:
    break;  // EButton
  }
}

bool PDialog::buildAndRun(int w, int h)
{
  for (int i = 0; i < int(iElements.size()); ++i) {
    SElement &m = iElements[i];
    if (m.row < 0) {
      QPushButton *b = new QPushButton(QString::fromUtf8(m.text.c_str()), this);
      iWidgets.push_back(b);
      if (m.flags & EAccept) {
	b->setDefault(true);
	connect(b, SIGNAL(clicked()), SLOT(accept()));
      } else if (m.flags & EReject)
	connect(b, SIGNAL(clicked()), SLOT(reject()));
      else if (m.lua_method != LUA_NOREF)
	connect(b, SIGNAL(clicked()), SLOT(notifyLua()));
      iButtonArea->addWidget(b);
    } else {
      QWidget *w = 0;
      switch (m.type) {
      case ELabel:
	w = new QLabel(QString::fromUtf8(m.text.c_str()), this);
	break;
      case EButton:
	{
	  QPushButton *b = new QPushButton(QString::fromUtf8(m.text.c_str()), this);
	  if (m.flags & EAccept)
	    connect(b, SIGNAL(clicked()), SLOT(accept()));
	  else if (m.flags & EReject)
	    connect(b, SIGNAL(clicked()), SLOT(reject()));
	  else if (m.lua_method != LUA_NOREF)
	    connect(b, SIGNAL(clicked()), SLOT(notifyLua()));
	  w = b;
	}
	break;
      case ECheckBox:
	{
	  QCheckBox *ch = new QCheckBox(QString::fromUtf8(m.text.c_str()), this);
	  if (m.lua_method != LUA_NOREF)
	    connect(ch, SIGNAL(stateChanged(int)), SLOT(notifyLua()));
	  ch->setChecked(m.value);
	  w = ch;
	}
	break;
      case EInput:
	{
	  QLineEdit *e = new QLineEdit(this);
	  e->setText(QString::fromUtf8(m.text.c_str()));
	  if (m.flags & ESelectAll)
	    e->selectAll();
	  w = e;
	}
	break;
      case ETextEdit:
	{
	  QTextEdit *t = new QTextEdit(this);
	  t->setAcceptRichText(false);
	  if (m.flags & EReadOnly)
	    t->setReadOnly(true);
	  if (m.flags & EXml)
	    (void) new XmlHighlighter(t);
	  else if (m.flags & ELatex)
	    (void) new LatexHighlighter(t);
	  QString text = QString::fromUtf8(m.text.c_str());
	  if (m.flags & ELogFile) {
	    markupLog(t, text);
	  } else
	    t->setPlainText(text);
	  if (m.flags & ESelectAll)
	    t->selectAll();
	  w = t;
	}
	break;
      case ECombo:
	{
	  QComboBox *b = new QComboBox(this);
	  for (int k = 0; k < int(m.items.size()); ++k)
	    b->addItem(QString::fromUtf8(m.items[k].c_str()));
	  b->setCurrentIndex(m.value);
	  w = b;
	}
	break;
      case EList:
	{
	  QListWidget *l = new QListWidget(this);
	  for (int k = 0; k < int(m.items.size()); ++k)
	    l->addItem(QString::fromUtf8(m.items[k].c_str()));
	  w = l;
	}
	break;
      default:
	break;
      }
      iWidgets.push_back(w);
      gridlayout()->addWidget(w, m.row, m.col, m.rowspan, m.colspan);
      if (m.flags & EFocused)
	w->setFocus(Qt::OtherFocusReason);
    }
  }
  setMinimumSize(w, h);
  int result = exec();
  retrieveValues(); // for future reference
  return (result == QDialog::Accepted);
}

void PDialog::retrieveValues()
{
  for (int i = 0; i < int(iElements.size()); ++i) {
    SElement &m = iElements[i];
    QWidget *w = iWidgets[i];
    switch (m.type) {
    case EInput:
      m.text = std::move(std::string((qobject_cast<QLineEdit *>(w))->
				     text().toUtf8()));
      break;
    case ETextEdit:
      m.text = std::move(std::string((qobject_cast<QTextEdit *>(w))->
				     toPlainText().toUtf8()));
      break;
    case EList:
      {
	int r = (qobject_cast<QListWidget *>(w))->currentRow();
	m.value = (r >= 0) ? r : 0;
      }
      break;
    case ECombo:
      m.value = (qobject_cast<QComboBox *>(w))->currentIndex();
      break;
    case ECheckBox:
      m.value = qobject_cast<QCheckBox *>(w)->isChecked();
      break;
    default:
      break;  // label and button - nothing to do
    }
  }
}

void PDialog::enableItem(int idx, bool value)
{
  iWidgets[idx]->setEnabled(value);
}

void PDialog::acceptDialog(lua_State *L)
{
  int accept = lua_toboolean(L, 2);
  QDialog::done(accept);
}

// --------------------------------------------------------------------

static int dialog_constructor(lua_State *L)
{
  QWidget *parent = check_winid(L, 1);
  const char *s = luaL_checkstring(L, 2);

  Dialog **dlg = (Dialog **) lua_newuserdata(L, sizeof(Dialog *));
  *dlg = 0;
  luaL_getmetatable(L, "Ipe.dialog");
  lua_setmetatable(L, -2);
  *dlg = new PDialog(L, parent, s);
  return 1;
}

// --------------------------------------------------------------------

MenuAction::MenuAction(const QString &name, int number,
		       const QString &item, const QString &text,
		       QWidget *parent)
  : QAction(text, parent)
{
  iName = name;
  iItemName = item;
  iNumber = number;
}

// --------------------------------------------------------------------

PMenu::PMenu(WINID parent)
{
  iMenu = new QMenu();
}


PMenu::~PMenu()
{
  delete iMenu;
}

int PMenu::execute(lua_State *L)
{
  int vx = (int)luaL_checkinteger(L, 2);
  int vy = (int)luaL_checkinteger(L, 3);
  QAction *a = iMenu->exec(QPoint(vx, vy));
  MenuAction *ma = qobject_cast<MenuAction *>(a);
  if (ma) {
    push_string(L, ma->name());
    lua_pushnumber(L, ma->number());
    push_string(L, ma->itemName());
    return 3;
  }
  return 0;
}

#define SIZE 16

static QIcon colorIcon(double red, double green, double blue)
{
  QPixmap pixmap(SIZE, SIZE);
  pixmap.fill(QColor(int(red * 255.0 + 0.5),
		     int(green * 255.0 + 0.5),
		     int(blue * 255.0 + 0.5)));
  return QIcon(pixmap);
}

int PMenu::add(lua_State *L)
{
  QString name = checkstring(L, 2);
  QString title = checkstring(L, 3);
  if (lua_gettop(L) == 3) {
    iMenu->addAction(new MenuAction(name, 0, QString(), title, iMenu));
  } else {
    luaL_argcheck(L, lua_istable(L, 4), 4, "argument is not a table");
    bool hasmap = !lua_isnoneornil(L, 5) && lua_isfunction(L, 5);
    bool hastable = !hasmap && !lua_isnoneornil(L, 5);
    bool hascolor = !lua_isnoneornil(L, 6) && lua_isfunction(L, 6);
    bool hascheck = !hascolor && !lua_isnoneornil(L, 6);
    if (hastable)
      luaL_argcheck(L, lua_istable(L, 5), 5,
		    "argument is not a function or table");
    QString current;
    if (hascheck) {
      luaL_argcheck(L, lua_isstring(L, 6), 6,
		    "argument is not a function or string");
      current = checkstring(L, 6);
    }
    int no = lua_rawlen(L, 4);
    QMenu *sm = new QMenu(title, iMenu);
    for (int i = 1; i <= no; ++i) {
      lua_rawgeti(L, 4, i);
      luaL_argcheck(L, lua_isstring(L, -1), 4, "items must be strings");
      QString item = tostring(L, -1);
      QString text = item;
      if (hastable) {
	lua_rawgeti(L, 5, i);
	luaL_argcheck(L, lua_isstring(L, -1), 5, "labels must be strings");
	text = tostring(L, -1);
	lua_pop(L, 1);
      }
      if (hasmap) {
	lua_pushvalue(L, 5);   // function
	lua_pushnumber(L, i);  // index
 	lua_pushvalue(L, -3);  // name
	lua_call(L, 2, 1);     // function returns label
	luaL_argcheck(L, lua_isstring(L, -1), 5,
		      "function does not return string");
	text = tostring(L, -1);
	lua_pop(L, 1);         // pop result
      }
      MenuAction *ma = new MenuAction(name, i, item, text, sm);
      if (hascolor) {
	lua_pushvalue(L, 6);   // function
	lua_pushnumber(L, i);  // index
 	lua_pushvalue(L, -3);  // name
	lua_call(L, 2, 3);     // function returns red, green, blue
	double red = luaL_checknumber(L, -3);
	double green = luaL_checknumber(L, -2);
	double blue = luaL_checknumber(L, -1);
	lua_pop(L, 3);         // pop result
	QIcon icon = colorIcon(red, green, blue);
	ma->setIcon(icon);
	ma->setIconVisibleInMenu(true);
      }
      if (hascheck) {
	ma->setCheckable(true);
	ma->setChecked(item == current);
      }
      lua_pop(L, 1); // item
      sm->addAction(ma);
    }
    iMenu->addMenu(sm);
  }
  return 0;
}

static int menu_constructor(lua_State *L)
{
  QWidget *parent = check_winid(L, 1);
  Menu **m = (Menu **) lua_newuserdata(L, sizeof(PMenu *));
  *m = 0;
  luaL_getmetatable(L, "Ipe.menu");
  lua_setmetatable(L, -2);

  *m = new PMenu(parent);
  return 1;
}

// --------------------------------------------------------------------

class EditorThread : public QThread
{
public:
  EditorThread(const QString &cmd);

protected:
  virtual void run();
private:
  QString iCommand;
};

EditorThread::EditorThread(const QString &cmd) : QThread()
{
  iCommand = cmd;
}

void EditorThread::run()
{
  int result = std::system(iCommand.toUtf8());
  (void) result;
}

class EditorDialog : public QDialog
{
public:
  EditorDialog(QWidget *parent = 0);
protected:
  void closeEvent(QCloseEvent *ev);
};

EditorDialog::EditorDialog(QWidget *parent)
  : QDialog(parent)
{
  QGridLayout *lo = new QGridLayout;
  setLayout(lo);
  setWindowTitle("Ipe: waiting");
  QLabel *l = new QLabel("Waiting for external editor", this);
  lo->addWidget(l, 0, 0);
}

void EditorDialog::closeEvent(QCloseEvent *ev)
{
  ev->ignore();
}

static int ipeui_wait(lua_State *L)
{
  QString cmd = checkstring(L, 2);

  EditorDialog *dialog = new EditorDialog();

  EditorThread *thread = new EditorThread(cmd);
  dialog->connect(thread, SIGNAL(finished()), SLOT(accept()));

  thread->start();
  dialog->exec();
  delete dialog;
  delete thread;
  return 0;
}

// --------------------------------------------------------------------

PTimer::PTimer(lua_State *L0, int lua_object, const char *method)
  : Timer(L0, lua_object, method)
{
  iTimer = new QTimer();
  connect(iTimer, SIGNAL(timeout()), SLOT(elapsed()));
}

PTimer::~PTimer()
{
  delete iTimer;
}

void PTimer::elapsed()
{
  if (iSingleShot)
    iTimer->stop();
  callLua();
}

int PTimer::setInterval(lua_State *L)
{
  int t = (int)luaL_checkinteger(L, 2);
  iTimer->setInterval(t);
  return 0;
}

int PTimer::active(lua_State *L)
{
  lua_pushboolean(L, iTimer->isActive());
  return 1;
}

int PTimer::start(lua_State *L)
{
  iTimer->start();
  return 0;
}

int PTimer::stop(lua_State *L)
{
  iTimer->stop();
  return 0;
}

// --------------------------------------------------------------------

static int timer_constructor(lua_State *L)
{
  luaL_argcheck(L, lua_istable(L, 1), 1, "argument is not a table");
  const char *method = luaL_checkstring(L, 2);

  Timer **t = (Timer **) lua_newuserdata(L, sizeof(Timer *));
  *t = 0;
  luaL_getmetatable(L, "Ipe.timer");
  lua_setmetatable(L, -2);

  // create a table with weak reference to Lua object
  lua_createtable(L, 1, 1);
  lua_pushliteral(L, "v");
  lua_setfield(L, -2, "__mode");
  lua_pushvalue(L, -1);
  lua_setmetatable(L, -2);
  lua_pushvalue(L, 1);
  lua_rawseti(L, -2, 1);
  int lua_object = luaL_ref(L, LUA_REGISTRYINDEX);
  *t = new PTimer(L, lua_object, method);
  return 1;
}

// --------------------------------------------------------------------

static int ipeui_getColor(lua_State *L)
{
  QWidget *parent = check_winid(L, 1);
  QString title = checkstring(L, 2);
  QColor initial = QColor::fromRgbF(luaL_checknumber(L, 3),
				    luaL_checknumber(L, 4),
				    luaL_checknumber(L, 5));
#if QT_VERSION >= 0x040500
  QColor changed = QColorDialog::getColor(initial, parent, title);
#else
  QColor changed = QColorDialog::getColor(initial, parent);
#endif
  if (changed.isValid()) {
    lua_pushnumber(L, changed.redF());
    lua_pushnumber(L, changed.greenF());
    lua_pushnumber(L, changed.blueF());
    return 3;
  } else
    return 0;
}

// --------------------------------------------------------------------

static int ipeui_fileDialog(lua_State *L)
{
  static const char * const typenames[] = { "open", "save", 0 };

  QWidget *parent = check_winid(L, 1);
  int type = luaL_checkoption(L, 2, 0, typenames);
  QString caption = checkstring(L, 3);
  if (!lua_istable(L, 4))
    luaL_argerror(L, 4, "table expected for filters");
  QStringList filters;
  int nFilters = lua_rawlen(L, 4);
  for (int i = 1; i <= nFilters; i += 2) { // skip Windows versions
    lua_rawgeti(L, 4, i);
    luaL_argcheck(L, lua_isstring(L, -1), 4, "filter entry is not a string");
    filters += checkstring(L, -1);
    lua_pop(L, 1); // element i
  }

  QString dir;
  if (!lua_isnoneornil(L, 5))
    dir = checkstring(L, 5);
  QString name;
  if (!lua_isnoneornil(L, 6))
    name = checkstring(L, 6);
  int selected = 0;
  if (!lua_isnoneornil(L, 7))
    selected = luaL_checkinteger(L, 7);

  QFileDialog dialog(parent);
  dialog.setWindowTitle(caption);
  dialog.setNameFilters(filters);
  dialog.setConfirmOverwrite(false);

  if (type == 0) {
    dialog.setFileMode(QFileDialog::ExistingFile);
    dialog.setAcceptMode(QFileDialog::AcceptOpen);
  } else {
    dialog.setFileMode(QFileDialog::AnyFile);
    dialog.setAcceptMode(QFileDialog::AcceptSave);
  }

  if (selected)
    dialog.selectNameFilter(filters[selected-1]);
  if (!dir.isNull())
    dialog.setDirectory(dir);
  if (!name.isNull())
    dialog.selectFile(name);

  if (dialog.exec() == QDialog::Accepted) {
    QStringList fns = dialog.selectedFiles();
    if (!fns.isEmpty()) {
      push_string(L, fns[0]);
      QString f = dialog.selectedNameFilter();
      int sf = 0;
      while (filters[sf] != f && sf < nFilters/2)
	++sf;
      if (sf < nFilters/2)
	lua_pushinteger(L, sf + 1);
      return 2;
    }
  }
  return 0;
}

// --------------------------------------------------------------------

static int ipeui_messageBox(lua_State *L)
{
  static const char * const options[] =  {
    "none", "warning", "information", "question", "critical", 0 };
  static const char * const buttontype[] = {
    "ok", "okcancel", "yesnocancel", "discardcancel",
    "savediscardcancel", 0 };

  QWidget *parent = check_winid(L, 1);
  int type = luaL_checkoption(L, 2, "none", options);
  QString text = checkstring(L, 3);
  const char *details = 0;
  if (!lua_isnoneornil(L, 4))
    details = luaL_checkstring(L, 4);
  int buttons = 0;
  if (lua_isnumber(L, 5))
    buttons = (int)luaL_checkinteger(L, 5);
  else if (!lua_isnoneornil(L, 5))
    buttons = luaL_checkoption(L, 5, 0, buttontype);

  QMessageBox msgBox(parent);
  msgBox.setText(text);
  if (details)
    msgBox.setInformativeText(QString::fromUtf8(details));

  switch (type) {
  case 0:
  default:
    msgBox.setIcon(QMessageBox::NoIcon);
    break;
  case 1:
    msgBox.setIcon(QMessageBox::Warning);
    break;
  case 2:
    msgBox.setIcon(QMessageBox::Information);
    break;
  case 3:
    msgBox.setIcon(QMessageBox::Question);
    break;
  case 4:
    msgBox.setIcon(QMessageBox::Critical);
    break;
  }

  switch (buttons) {
  case 0:
  default:
    msgBox.setStandardButtons(QMessageBox::Ok);
    break;
  case 1:
    msgBox.setStandardButtons(QMessageBox::Ok|QMessageBox::Cancel);
    break;
  case 2:
    msgBox.setStandardButtons(QMessageBox::Yes|QMessageBox::No|
			      QMessageBox::Cancel);
    break;
  case 3:
    msgBox.setStandardButtons(QMessageBox::Discard|QMessageBox::Cancel);
    break;
  case 4:
    msgBox.setStandardButtons(QMessageBox::Save|QMessageBox::Discard|
			      QMessageBox::Cancel);
    break;
  }

  int ret = msgBox.exec();

  switch (ret) {
  case QMessageBox::Ok:
  case QMessageBox::Yes:
  case QMessageBox::Save:
    lua_pushnumber(L, 1);
    break;
  case QMessageBox::No:
  case QMessageBox::Discard:
    lua_pushnumber(L, 0);
    break;
  case QMessageBox::Cancel:
  default:
    lua_pushnumber(L, -1);
    break;
  }
  return 1;
}

// --------------------------------------------------------------------

static int ipeui_currentDateTime(lua_State *L)
{
  QDateTime dt = QDateTime::currentDateTime();
  QString mod;
  mod.sprintf("%04d%02d%02d%02d%02d%02d",
	      dt.date().year(), dt.date().month(), dt.date().day(),
	      dt.time().hour(), dt.time().minute(), dt.time().second());
  push_string(L, mod);
  return 1;
}

// --------------------------------------------------------------------

static const struct luaL_Reg ipeui_functions[] = {
  { "Dialog", dialog_constructor },
  { "Menu", menu_constructor },
  { "Timer", timer_constructor },
  { "getColor", ipeui_getColor },
  { "fileDialog", ipeui_fileDialog },
  { "messageBox", ipeui_messageBox },
  { "waitDialog", ipeui_wait },
  { "currentDateTime", ipeui_currentDateTime },
  { 0, 0},
};

// --------------------------------------------------------------------

int luaopen_ipeui(lua_State *L)
{
  luaL_newlib(L, ipeui_functions);
  lua_setglobal(L, "ipeui");
  luaopen_ipeui_common(L);
  return 0;
}

// --------------------------------------------------------------------
