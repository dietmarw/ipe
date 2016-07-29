// --------------------------------------------------------------------
// Page sorter for Win32
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

#include "appui_win.h"
#include "ipecanvas_win.h"
#include "ipethumbs.h"
#include "ipeui_wstring.h"

#include "ipelua.h"

#include <windows.h>
#include <commctrl.h>

using namespace ipe;

// --------------------------------------------------------------------

#define IDBASE 9000
#define PAD 3
#define BORDER 6
#define BUTTONHEIGHT 14

static void buildFlags(std::vector<short> &t, DWORD flags)
{
  union {
    DWORD dw;
    short s[2];
  } a;
  a.dw = flags;
  t.push_back(a.s[0]);
  t.push_back(a.s[1]);
  t.push_back(0);
  t.push_back(0);
}

static void buildString(std::vector<short> &t, const char *s)
{
  const char *p = s;
  while (*p)
    t.push_back(*p++);
  t.push_back(0);
}

static void buildButton(std::vector<short> &t, UINT flags, int id,
			int h, int x, const char *s)
{
  if (t.size() % 2 != 0)
    t.push_back(0);
  buildFlags(t, flags|WS_CHILD|WS_VISIBLE|BS_TEXT|WS_TABSTOP|BS_FLAT);
  t.push_back(x);
  t.push_back(h - BORDER - BUTTONHEIGHT);
  t.push_back(64);
  t.push_back(BUTTONHEIGHT);
  t.push_back(IDBASE + id);
  t.push_back(0xffff);
  t.push_back(0x0080);
  buildString(t, s);
  t.push_back(0);
}

// --------------------------------------------------------------------

struct SData {
  HIMAGELIST hImageList;
  Document *doc;
  std::vector<int> pages;
  std::vector<int> cutlist;
};

static void insertItem(HWND h, SData *d, int index, int page)
{
  LVITEM lvI;
  lvI.mask = LVIF_TEXT | LVIF_IMAGE;
  lvI.iItem  = index;
  lvI.iImage = page;
  lvI.iSubItem = 0;

  Page *p = d->doc->page(page);
  char buf[16];
  String t = p->title();
  if (t != "")
    sprintf(buf, "%d: ", page+1);
  else
    sprintf(buf, "Page %d", page+1);
  WString ws(String(buf) + t);
  lvI.pszText = ws;
  ListView_InsertItem(h, &lvI);
}

static void populateView(HWND lv, SData *d)
{
  // TODO send WM_SETREDRAW
  ListView_DeleteAllItems(lv);
  for (int i = 0; i < int(d->pages.size()); ++i) {
    insertItem(lv, d, i, d->pages[i]);
  }
}

static void deleteItems(HWND lv, SData *d, bool cut)
{
  int n = d->pages.size();
  if (cut)
    d->cutlist.clear();
  for (int i = n-1; i >= 0; --i) {
    if (ListView_GetItemState(lv, i, LVIS_SELECTED) & LVIS_SELECTED) {
      if (cut)
	d->cutlist.insert(d->cutlist.begin(), d->pages[i]);
      d->pages.erase(d->pages.begin() + i);
    }
  }
}

static void insertItems(HWND lv, SData *d, int index)
{
  while (!d->cutlist.empty()) {
    int p = d->cutlist.back();
    d->cutlist.pop_back();
    d->pages.insert(d->pages.begin() + index, p);
  }
}

static void showPopup(HWND parent, POINT pt, int index, SData *d, HWND lv)
{
  int selcnt = ListView_GetSelectedCount(lv);
  ipeDebug("Index %d, selected %d", index, selcnt);
  if (index < 0 || selcnt == 0)
    return;

  ClientToScreen(parent, &pt);
  HMENU hMenu = CreatePopupMenu();
  if (selcnt > 0) {
    AppendMenuA(hMenu, MF_STRING, 1, "Delete");
    AppendMenuA(hMenu, MF_STRING, 2, "Cut");
  }
  if (!d->cutlist.empty()) {
    char buf[32];
    sprintf(buf, "Insert before page %d", d->pages[index] + 1);
    AppendMenuA(hMenu, MF_STRING, 3, buf);
    sprintf(buf, "Insert after page %d", d->pages[index] + 1);
    AppendMenuA(hMenu, MF_STRING, 4, buf);
  }
  int result = TrackPopupMenu(hMenu,
			      TPM_NONOTIFY | TPM_RETURNCMD | TPM_RIGHTBUTTON,
			      pt.x, pt.y, 0, parent, NULL);
  DestroyMenu(hMenu);
  switch (result) {
  case 1: // Delete
    deleteItems(lv, d, false);
    populateView(lv, d);
    break;
  case 2: // Cut
    deleteItems(lv, d, true);
    populateView(lv, d);
    break;
  case 3: // Insert before
    insertItems(lv, d, index);
    populateView(lv, d);
    break;
  case 4: // Insert after
    insertItems(lv, d, index+1);
    populateView(lv, d);
    break;
  default:
    // Menu canceled
    break;
  }
}

static BOOL CALLBACK dialogProc(HWND hwnd, UINT message,
				WPARAM wParam, LPARAM lParam)
{
  SData *d = (SData *) GetWindowLongPtr(hwnd, GWLP_USERDATA);
  HWND h = GetDlgItem(hwnd, IDBASE);

  switch (message) {
  case WM_INITDIALOG: {
    d = (SData *) lParam;
    SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR) d);
    SendMessage(h, LVM_SETIMAGELIST, (WPARAM) LVSIL_NORMAL,
		(LPARAM) d->hImageList);
    populateView(h, d);
    return TRUE; }
  case WM_COMMAND:
    switch (LOWORD(wParam)) {
    case IDBASE + 1: // Ok
      EndDialog(hwnd, 1);
      return TRUE;
    case IDBASE + 2: // Cancel
      EndDialog(hwnd, -1);
      return TRUE;
    default:
      return FALSE;
    }
  case WM_NOTIFY:
    switch (((LPNMHDR)lParam)->code) {
    case NM_RCLICK:
      showPopup(hwnd, ((LPNMITEMACTIVATE) lParam)->ptAction,
		((LPNMITEMACTIVATE) lParam)->iItem, d, h);
      return TRUE;
    default:
      return FALSE;
    }
  case WM_CLOSE:
    EndDialog(hwnd, -1);
    return TRUE;
  default:
    return FALSE;
  }
}

// --------------------------------------------------------------------

int AppUi::pageSorter(lua_State *L, Document *doc,
		      int width, int height, int thumbWidth)
{
  SData sData;
  sData.doc = doc;

  // Create image list
  Thumbnail r(doc, thumbWidth);
  // Image list will be destroyed when ListView is destroyed
  sData.hImageList = ImageList_Create(thumbWidth, r.height(), ILC_COLOR32,
				      doc->countPages(), 4);
  for (int i = 0; i < doc->countPages(); ++i) {
    sData.pages.push_back(i);
    Page *p = doc->page(i);
    Buffer bx = r.render(p, p->countViews() - 1);
    HBITMAP b = Canvas::createBitmap((unsigned char *) bx.data(),
				     thumbWidth, r.height());
    ImageList_Add(sData.hImageList, b, NULL);
  }

  // Convert to dialog units:
  LONG base = GetDialogBaseUnits();
  int baseX = LOWORD(base);
  int baseY = HIWORD(base);
  int w = width * 4 / baseX;
  int h = height * 8 / baseY;

  std::vector<short> t;

  // Dialog flags
  buildFlags(t, WS_POPUP | WS_BORDER | DS_SHELLFONT |
	     WS_SYSMENU | DS_MODALFRAME | WS_CAPTION);
  t.push_back(3);  // no of elements
  t.push_back(100);  // position of popup-window
  t.push_back(30);
  t.push_back(w);
  t.push_back(h);
  // menu
  t.push_back(0);
  // class
  t.push_back(0);
  // title
  buildString(t, "Page sorter");
  // for DS_SHELLFONT
  t.push_back(10);
  buildString(t,"MS Shell Dlg");
  // Page sorter control
  if (t.size() % 2 != 0)
    t.push_back(0);
  buildFlags(t, WS_CHILD|WS_VISIBLE|LVS_ICON|LVS_AUTOARRANGE|
	     WS_VSCROLL|WS_BORDER);
  t.push_back(BORDER);
  t.push_back(BORDER);
  t.push_back(w - 2 * BORDER);
  t.push_back(h - 2 * BORDER - PAD - BUTTONHEIGHT);
  t.push_back(IDBASE);
  buildString(t, WC_LISTVIEWA);
  t.push_back(0);
  t.push_back(0);
  buildButton(t, BS_DEFPUSHBUTTON, 1, h, w - BORDER - 128 - PAD, "Ok");
  buildButton(t, BS_PUSHBUTTON, 2, h, w - BORDER - 64, "Cancel");


  int res =
    DialogBoxIndirectParamW(NULL, (LPCDLGTEMPLATE) &t[0],
			    NULL, dialogProc, (LPARAM) &sData);

  if (res == 1) {
    int n = sData.pages.size();
    lua_createtable(L, n, 0);
    for (int i = 1; i <= n; ++i) {
      lua_pushinteger(L, sData.pages[i-1] + 1);
      lua_rawseti(L, -2, i);
    }
    return 1;
  } else
    return 0;
}

// --------------------------------------------------------------------
