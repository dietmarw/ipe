// --------------------------------------------------------------------
// PageSelector for Win32
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

#include "ipecanvas_win.h"
#include "ipethumbs.h"

#include <windows.h>
#include <commctrl.h>

using namespace ipe;

// --------------------------------------------------------------------

#define IDBASE 9000
#define BORDER 6

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

// --------------------------------------------------------------------

struct SData {
  HIMAGELIST hImageList;
  Document *doc;
  int page;
  int startIndex;
};

static void populateView(HWND h, SData *d)
{
  LVITEM lvI;
  lvI.mask = LVIF_TEXT | LVIF_IMAGE;
  lvI.iSubItem = 0;
  if (d->page >= 0) {
    Page *p = d->doc->page(d->page);
    for (int i = 0; i < p->countViews(); ++i) {
      wchar_t s[16];
      swprintf(s, L"View %d", i+1);
      lvI.pszText = s;
      lvI.iItem  = i;
      lvI.iImage = i;
      ListView_InsertItem(h, &lvI);
    }
  } else {
    for (int i = 0; i < d->doc->countPages(); ++i) {
      Page *p = d->doc->page(i);
      String t = p->title();
      if (t != "") {
	char buf[16];
	sprintf(buf, "%d: ", i+1);
	String s = String(buf) + t;
	std::vector<wchar_t> wbuf;
	Platform::utf8ToWide(s.z(), wbuf);
	lvI.pszText = &wbuf[0];
      } else {
	wchar_t s[16];
	swprintf(s, L"Page %d", i+1);
	lvI.pszText = s;
      }
      lvI.iItem  = i;
      lvI.iImage = i;
      ListView_InsertItem(h, &lvI);
    }
  }

  SendMessage(h, LVM_ENSUREVISIBLE, (WPARAM) d->startIndex, FALSE);
}

static BOOL CALLBACK dialogProc(HWND hwnd, UINT message,
				WPARAM wParam, LPARAM lParam)
{
  SData *d = (SData *) GetWindowLongPtr(hwnd, GWLP_USERDATA);

  switch (message) {
  case WM_INITDIALOG: {
    d = (SData *) lParam;
    SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR) d);
    HWND h = GetDlgItem(hwnd, IDBASE);
    SendMessage(h, LVM_SETIMAGELIST, (WPARAM) LVSIL_NORMAL,
		(LPARAM) d->hImageList);
    populateView(h, d);
    return TRUE; }
  case WM_NOTIFY:
    switch (((LPNMHDR)lParam)->code) {
    case LVN_ITEMACTIVATE:
      EndDialog(hwnd, ((LPNMITEMACTIVATE) lParam)->iItem);
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

//! Show dialog to select a page or a view.
/*! If \a page is negative (the default), shows thumbnails of all
    pages of the document in a dialog.  If the user selects a page,
    the page number is returned. If the dialog is canceled, -1 is
    returned.

    If \a page is non-negative, all views of this page are shown, and
    the selected view number is returned.

    \a itemWidth is the width of the page thumbnails (the height is
    computed automatically).
*/
int CanvasBase::selectPageOrView(Document *doc, int page, int startIndex,
				 int pageWidth, int width, int height)
{
  // Create image list
  Thumbnail r(doc, pageWidth);
  int nItems = (page >= 0) ? doc->page(page)->countViews() :
    doc->countPages();
  // Image list will be destroyed when ListView is destroyed
  HIMAGELIST il = ImageList_Create(pageWidth, r.height(), ILC_COLOR32,
				   nItems, 4);
  if (page >= 0) {
    Page *p = doc->page(page);
    for (int i = 0; i < p->countViews(); ++i) {
      Buffer bx = r.render(p, i);
      HBITMAP b = Canvas::createBitmap((unsigned char *) bx.data(),
				       pageWidth, r.height());
      ImageList_Add(il, b, NULL);
    }
  } else {
    for (int i = 0; i < doc->countPages(); ++i) {
      Page *p = doc->page(i);
      Buffer bx = r.render(p, p->countViews() - 1);
      HBITMAP b = Canvas::createBitmap((unsigned char *) bx.data(),
				       pageWidth, r.height());
      ImageList_Add(il, b, NULL);
    }
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
  t.push_back(1);  // no of elements
  t.push_back(100);  // position of popup-window
  t.push_back(30);
  t.push_back(w);
  t.push_back(h);
  // menu
  t.push_back(0);
  // class
  t.push_back(0);
  // title
  buildString(t, (page >= 0) ? "Ipe: Select view" : "Ipe: Select page");
  // for DS_SHELLFONT
  t.push_back(10);
  buildString(t,"MS Shell Dlg");
  // Item
  if (t.size() % 2 != 0)
    t.push_back(0);
  buildFlags(t, WS_CHILD|WS_VISIBLE|LVS_ICON|
	     WS_VSCROLL|LVS_SINGLESEL|WS_BORDER);
  t.push_back(BORDER);
  t.push_back(BORDER);
  t.push_back(w - 2 * BORDER);
  t.push_back(h - 2 * BORDER);
  t.push_back(IDBASE);    // control id
  buildString(t, WC_LISTVIEWA);
  t.push_back(0);
  t.push_back(0);

  SData sData;
  sData.hImageList = il;
  sData.doc = doc;
  sData.page = page;
  sData.startIndex = startIndex;

  int res =
    DialogBoxIndirectParamW(NULL, (LPCDLGTEMPLATE) &t[0],
			    NULL, dialogProc, (LPARAM) &sData);
  return res;
}

HBITMAP Canvas::createBitmap(unsigned char *p, int w, int h)
{
  BITMAPINFO bmi;
  ZeroMemory(&bmi, sizeof(bmi));
  bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bmi.bmiHeader.biWidth = w;
  bmi.bmiHeader.biHeight = h;
  bmi.bmiHeader.biPlanes = 1;
  bmi.bmiHeader.biBitCount = 24;
  bmi.bmiHeader.biCompression = BI_RGB;
  void *pbits = 0;
  HBITMAP bm = CreateDIBSection(NULL, &bmi, DIB_RGB_COLORS,
				&pbits, NULL, 0);
  unsigned char *q = (unsigned char *) pbits;
  int stride = 3 * w;
  if (stride % 1)
    stride += 1;
  for (int y = 0; y < h; ++y) {
    unsigned char *src = p + 4 * w * y;
    unsigned char *dst = q + stride * (h - 1 - y);
    for (int x = 0; x < w; ++x) {
      *dst++ = *src++; // R
      *dst++ = *src++; // G
      *dst++ = *src++; // B
      ++src;  // skip A
    }
  }
  return bm;
}

// --------------------------------------------------------------------
