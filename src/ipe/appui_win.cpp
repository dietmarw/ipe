// --------------------------------------------------------------------
// AppUi for Win32
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
#include "controls_win.h"
#include "ipeui_wstring.h"

#include "ipelua.h"

// for version info only
#include "ipefonts.h"
#include <zlib.h>

#include <cstdio>
#include <cstdlib>

#include <windowsx.h>
#include <gdiplus.h>

#define IDI_MYICON 1
#define ID_STATUS_TIMER 1
#define IDC_STATUSBAR 7000
#define IDC_BOOKMARK 7100
#define IDC_NOTES 7200
#define IDC_LAYERS 7300
#define IDBASE 8000
#define TEXT_STYLE_BASE 8300
#define ID_LAYERLIST 8400
#define ID_SELECTOR_BASE 8500
#define ID_ABSOLUTE_BASE 8600
#define ID_PATHVIEW 8700
#define ID_MOVETOLAYER_BASE 8800
#define ID_SELECTINLAYER_BASE 9000
#define ID_GRIDSIZE_BASE 9100
#define ID_ANGLESIZE_BASE 9500

using namespace ipe;
using namespace ipelua;

// --------------------------------------------------------------------

const int TBICONSIZE = 24;

static HINSTANCE win_hInstance = 0;
static int win_nCmdShow = 0;

const wchar_t AppUi::className[] = L"ipeWindowClass";

// --------------------------------------------------------------------

static HBITMAP loadIcon(String action, int w, int h, int r0, int g0, int b0)
{
  String dir = ipeIconDirectory();
  String base = dir + action;
  String fname;
  int ww = w < 0 ? -w : w;
  if (ww >= 64 && Platform::fileExists(base + "_32x32@2x.png"))
    fname = base + "_32x32@2x.png";
  else if (ww >= 48 && Platform::fileExists(base + "@2x.png"))
    fname = base + "@2x.png";
  else if (ww >= 32 && Platform::fileExists(base + "_32x32.png"))
    fname = base + "_32x32.png";
  else
    fname = base + ".png";
  // ipeDebug("loadIcon %s at %d x %d from %s", action.z(), w, h, fname.z());
  if (!Platform::fileExists(fname))
    return NULL;

  WString wname(fname);
  Gdiplus::Bitmap *bitmap = Gdiplus::Bitmap::FromFile(wname, FALSE);
  if (bitmap->GetLastStatus() != Gdiplus::Ok) {
    delete bitmap;
    return NULL;
  }

  int wd = bitmap->GetWidth();
  int ht = bitmap->GetHeight();

  Gdiplus::BitmapData* bitmapData = new Gdiplus::BitmapData;
  Gdiplus::Rect rect(0, 0, wd, ht);
  bitmap->LockBits(&rect, Gdiplus::ImageLockModeRead,
		   PixelFormat32bppPARGB, bitmapData);

  if (w < 0) w = wd;
  if (h < 0) h = ht;

  BITMAPINFO bmi;
  ZeroMemory(&bmi, sizeof(bmi));
  bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bmi.bmiHeader.biWidth = w;
  bmi.bmiHeader.biHeight = h;
  bmi.bmiHeader.biPlanes = 1;
  bmi.bmiHeader.biBitCount = (r0 >= 0) ? 24 : 32;
  bmi.bmiHeader.biCompression = BI_RGB;
  void *pbits = 0;
  HBITMAP bm = CreateDIBSection(NULL, &bmi, DIB_RGB_COLORS, &pbits, NULL, 0);

  unsigned char *p = (unsigned char *) bitmapData->Scan0;
  unsigned char *q = (unsigned char *) pbits;
  int stride = bitmapData->Stride;

  if (r0 < 0) {
    // make it transparent
    for (unsigned char *r = q; r < q + 4 * w * h;)
      *r++ = 0x00;
    int x0 = wd < w ? (w - wd)/2 : 0;
    int y0 = ht < h ? (h - ht)/2 : 0;
    int xs = wd > w ? w : wd;
    int ys = ht > h ? h : ht;
    for (int y = 0; y < ys; ++y) {
      unsigned char *src = p + (ht - 1 - y) * stride;
      unsigned char *dst = q + (y0 + y) * (4 * w) + 4 * x0;
      int n = 4 * xs;
      while (n--)
	*dst++ = *src++;
    }
  } else {
    // mix with color r0, g0, b0
    // assumes w = wd, h = ht
    int dstride = (3 * w + 3) & (~3);

    for (int x = 0; x < w; ++x) {
      for (int y = 0; y < h; ++y) {
	unsigned char *src = p + (h - 1 - y) * stride + 4 * x;
	unsigned char *dst = q + y * dstride + 3 * x;
	int b1 = *src++;
	int g1 = *src++;
	int r1 = *src++;
	int trans = *src++;
	*dst++ = b1 + int((0xff - trans) * b0 / 0xff);
	*dst++ = g1 + int((0xff - trans) * g0 / 0xff);
	*dst++ = r1 + int((0xff - trans) * r0 / 0xff);
      }
    }
  }
  bitmap->UnlockBits(bitmapData);
  delete bitmapData;
  delete bitmap;
  return bm;
}

static int loadIcon(String action, HIMAGELIST il, int scale)
{
  int size = scale * TBICONSIZE / 100;
  HBITMAP bm = loadIcon(action, size, size, -1, -1, -1);
  if (bm == NULL)
    return -1;
  int r = ImageList_Add(il, bm, NULL);
  DeleteObject(bm);
  return r;
}

static HBITMAP loadButtonIcon(String action, int scale)
{
  int size = scale * 24 / 100;
  DWORD rgb = GetSysColor(COLOR_BTNFACE);
  int r0 = GetRValue(rgb);
  int g0 = GetGValue(rgb);
  int b0 = GetBValue(rgb);
  return loadIcon(action, -size, -1, r0, g0, b0);
}

static HBITMAP colorIcon(Color color, int scale)
{
  int r = int(color.iRed.internal() * 255 / 1000);
  int g = int(color.iGreen.internal() * 255 / 1000);
  int b = int(color.iBlue.internal() * 255 / 1000);
  COLORREF rgb = RGB(r, g, b);
  int cx = scale * 22 / 100;
  int cy = scale * 22 / 100;
  HDC hdc = GetDC(NULL);
  HDC memDC = CreateCompatibleDC(hdc);
  HBITMAP bm = CreateCompatibleBitmap(hdc, cx, cy);
  SelectObject(memDC, bm);
  for (int y = 0; y < cy; ++y) {
    for (int x = 0; x < cx; ++x) {
      SetPixel(memDC, x, y, rgb);
    }
  }
  DeleteDC(memDC);
  ReleaseDC(NULL, hdc);
  return bm;
}

// --------------------------------------------------------------------

void AppUi::addRootMenu(int id, const char *name)
{
  hRootMenu[id] = CreatePopupMenu();
  WString wname(name);
  AppendMenuW(hMenuBar, MF_STRING | MF_POPUP, UINT(hRootMenu[id]),
	      wname.data());
}

void AppUi::createAction(String name, String tooltip, bool canWhileDrawing)
{
  SAction s;
  s.name = String(name);
  s.icon = loadIcon(name, hIcons, iToolbarScale);
  s.tooltip = tooltip;
  s.alwaysOn = canWhileDrawing;
  iActions.push_back(s);
}

void AppUi::addItem(HMENU menu, const char *title, const char *name)
{
  if (title == 0) {
    AppendMenu(menu, MF_SEPARATOR, 0, 0);
  } else {
    bool canUseWhileDrawing = false;
    if (name[0] == '@') {
      canUseWhileDrawing = true;
      name = name + 1;
    }
    if (name[0] == '*')
      name += 1;  // in Win32 all menu items are checkable anyway

    // check for shortcut
    lua_getglobal(L, "shortcuts");
    lua_getfield(L, -1, name);
    String sc;
    if (lua_isstring(L, -1))
      sc = lua_tostring(L, -1);
    lua_pop(L, 2); // shortcuts, shortcuts[name]

    String tooltip = title;
    if (!sc.empty())
      tooltip = tooltip + " [" + sc + "]";
    createAction(name, tooltip, canUseWhileDrawing);

    if (sc.empty()) {
      WString wtitle(title);
      AppendMenuW(menu, MF_STRING, iActions.size() - 1 + IDBASE, wtitle);
    } else {
      String t = title;
      t += "\t";
      t += sc;
      WString wt(t);
      AppendMenu(menu, MF_STRING, iActions.size() - 1 + IDBASE, wt);
    }
  }
}

void AppUi::addItem(int id, const char *title, const char *name)
{
  addItem(hRootMenu[id], title, name);
}

static HMENU submenu = 0;

void AppUi::startSubMenu(int id, const char *name, int tag)
{
  submenu = CreatePopupMenu();
  WString wname(name);
  AppendMenuW(hRootMenu[id], MF_STRING | MF_POPUP, UINT(submenu), wname);
}

void AppUi::addSubItem(const char *title, const char *name)
{
  addItem(submenu, title, name);
}

MENUHANDLE AppUi::endSubMenu()
{
  return submenu;
}

HWND AppUi::createToolBar(HINSTANCE hInst)
{
  HWND tb = CreateWindowExW(0, TOOLBARCLASSNAMEW, NULL,
			    WS_CHILD | WS_VISIBLE |
			    TBSTYLE_TOOLTIPS |
			    CCS_NOPARENTALIGN | CCS_NORESIZE,
			    0, 0, 0, 0, hwnd, NULL, hInst, NULL);

  // needed to set version of control library to be used
  SendMessage(tb, TB_BUTTONSTRUCTSIZE, (WPARAM) sizeof(TBBUTTON), 0);
  SendMessage(tb, TB_SETIMAGELIST, 0, (LPARAM) hIcons);
  SendMessage(tb, TB_SETMAXTEXTROWS, 0, (LPARAM) 0);
  return tb;
}

void AppUi::addTButton(HWND tb, const char *name, int flags)
{
  TBBUTTON tbb;
  ZeroMemory(&tbb, sizeof(tbb));
  if (name == 0) {
    tbb.iBitmap = 1;
    tbb.fsStyle = BTNS_SEP | flags;
    SendMessageW(tb, TB_ADDBUTTONS, 1, (LPARAM) &tbb);
  } else {
    int i = findAction(name);
    tbb.iBitmap = iconId(name);
    tbb.fsState = TBSTATE_ENABLED;
    tbb.fsStyle = TBSTYLE_BUTTON | flags;
    tbb.idCommand = i + IDBASE;
    WString tt(iActions[i].tooltip);
    tbb.iString = INT_PTR(tt.data());
    SendMessageW(tb, TB_ADDBUTTONS, 1, (LPARAM) &tbb);
  }
}

void AppUi::setTooltip(HWND h, String tip, bool isComboBoxEx)
{
  if (isComboBoxEx)
    h = (HWND) SendMessage(h, CBEM_GETCOMBOCONTROL, 0, 0);
  TOOLINFO toolInfo = { 0 };
  toolInfo.cbSize = sizeof(toolInfo);
  toolInfo.hwnd = hwnd;
  toolInfo.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
  toolInfo.uId = (UINT_PTR) h;
  WString wtip(tip);
  toolInfo.lpszText = wtip.data();
  SendMessage(hTip, TTM_ADDTOOL, 0, (LPARAM) &toolInfo);
}

HWND AppUi::createButton(HINSTANCE hInst, int id, int flags)
{
  return CreateWindowExW(0, L"button", NULL, WS_CHILD | WS_VISIBLE | flags,
			 0, 0, 0, 0, hwnd, (HMENU) id, hInst, NULL);
}

void AppUi::toggleFullscreen()
{
  if (!iFullScreen) {
    HMONITOR hmon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = { sizeof(mi) };
    if (!GetMonitorInfo(hmon, &mi))
      return;

    iWasMaximized = IsZoomed(hwnd);
    if (iWasMaximized)
      SendMessage(hwnd, WM_SYSCOMMAND, SC_RESTORE, 0);
    GetWindowRect(hwnd, &iWindowRect);
    iWindowStyle = GetWindowLong(hwnd, GWL_STYLE);
    iWindowExStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
    SetWindowLong(hwnd, GWL_STYLE,
		  iWindowStyle & ~(WS_CAPTION | WS_THICKFRAME));
    SetWindowLong(hwnd, GWL_EXSTYLE,
		  iWindowExStyle & ~(WS_EX_DLGMODALFRAME | WS_EX_WINDOWEDGE |
				     WS_EX_CLIENTEDGE | WS_EX_STATICEDGE));
    SetWindowPos(hwnd, HWND_TOP,
		 mi.rcMonitor.left,
		 mi.rcMonitor.top,
		 mi.rcMonitor.right - mi.rcMonitor.left,
		 mi.rcMonitor.bottom - mi.rcMonitor.top,
		 SWP_SHOWWINDOW);
    iFullScreen = true;
  } else {
    SetWindowLong(hwnd, GWL_STYLE, iWindowStyle);
    SetWindowLong(hwnd, GWL_EXSTYLE, iWindowExStyle);

    SetWindowPos(hwnd, NULL, iWindowRect.left, iWindowRect.top,
		 iWindowRect.right - iWindowRect.left,
		 iWindowRect.bottom - iWindowRect.top,
		 SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    if (iWasMaximized)
      SendMessage(hwnd, WM_SYSCOMMAND, SC_MAXIMIZE, 0);
    iFullScreen = false;
  }
}

// --------------------------------------------------------------------

AppUi::AppUi(lua_State *L0, int model)
  : AppUiBase(L0, model)
{
  // windows procedure may receive WM_SIZE message before canvas is created
  iCanvas = 0;
  HWND hwnd = CreateWindowExW(WS_EX_CLIENTEDGE, className, L"Ipe",
			      WS_OVERLAPPEDWINDOW,
			      CW_USEDEFAULT, CW_USEDEFAULT,
			      CW_USEDEFAULT, CW_USEDEFAULT,
			      NULL, NULL, win_hInstance, this);

  if (hwnd == NULL) {
    MessageBoxW(NULL, L"AppUi window creation failed!", L"Error!",
		MB_ICONEXCLAMATION | MB_OK);
    exit(9);
  }
  assert(GetWindowLongPtr(hwnd, GWLP_USERDATA));
  iFullScreen = false;
}

static void insert_tb(HWND hRebar, REBARBANDINFO &rbBand, HWND tb, int size)
{
  rbBand.hwndChild = tb;
  rbBand.cxMinChild = size;
  rbBand.cx = size;
  SendMessage(hRebar, RB_INSERTBAND, (WPARAM)-1, (LPARAM) &rbBand);
}

// called during Window creation
void AppUi::initUi()
{
  int tbsize = iToolbarScale * TBICONSIZE / 100;
  hIcons = ImageList_Create(tbsize, tbsize, ILC_COLOR32, 20, 4);
  hColorIcons = ImageList_Create(uiscale(22), uiscale(22), ILC_COLOR32, 20, 4);
  HINSTANCE hInst = (HINSTANCE) GetWindowLong(hwnd, GWL_HINSTANCE);

  hMenuBar = CreateMenu();
  buildMenus();
  SetMenu(hwnd, hMenuBar);

  hStatusBar = CreateWindowExW(0, STATUSCLASSNAMEW, NULL,
			       WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
			       0, 0, 0, 0,
			       hwnd, (HMENU) IDC_STATUSBAR, hInst, NULL);

  hTip = CreateWindowEx(0, TOOLTIPS_CLASS, NULL,
			WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP,
			CW_USEDEFAULT, CW_USEDEFAULT,
			CW_USEDEFAULT, CW_USEDEFAULT,
			hwnd, NULL, hInst, NULL);
  SetWindowPos(hTip, HWND_TOPMOST, 0, 0, 0, 0,
	       SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);

  // ------------------------------------------------------------

  hSnapTools = createToolBar(hInst);

  addTButton(hSnapTools, "snapvtx", BTNS_CHECK);
  addTButton(hSnapTools, "snapctl", BTNS_CHECK);
  addTButton(hSnapTools, "snapbd", BTNS_CHECK);
  addTButton(hSnapTools, "snapint", BTNS_CHECK);
  addTButton(hSnapTools, "snapgrid", BTNS_CHECK);

  addTButton(hSnapTools, "snapangle", BTNS_CHECK);
  addTButton(hSnapTools, "snapauto", BTNS_CHECK);

#define SNAP_BUTTONS 7

  hEditTools = createToolBar(hInst);

  addTButton(hEditTools, "copy");
  addTButton(hEditTools, "cut");
  addTButton(hEditTools, "paste");
  addTButton(hEditTools, "delete");
  addTButton(hEditTools, "undo");
  addTButton(hEditTools, "redo");
  addTButton(hEditTools, "zoom_in");
  addTButton(hEditTools, "zoom_out");
  addTButton(hEditTools, "fit_objects");
  addTButton(hEditTools, "fit_page");
  addTButton(hEditTools, "fit_width");
  addTButton(hEditTools, "keyboard");
  createAction("shift_key", "Press the Shift key", true);
  addTButton(hEditTools, "shift_key", BTNS_CHECK);
  addTButton(hEditTools, "grid_visible", BTNS_CHECK);
  createAction("stop", "Abort object being drawn [Esc]", true);
  addTButton(hEditTools, "stop");

#define EDIT_BUTTONS 15

  hObjectTools = createToolBar(hInst);

  addTButton(hObjectTools, "mode_select", BTNS_CHECKGROUP);
  addTButton(hObjectTools, "mode_translate", BTNS_CHECKGROUP);
  addTButton(hObjectTools, "mode_rotate", BTNS_CHECKGROUP);
  addTButton(hObjectTools, "mode_stretch", BTNS_CHECKGROUP);
  addTButton(hObjectTools, "mode_shear", BTNS_CHECKGROUP);
  addTButton(hObjectTools, "mode_pan", BTNS_CHECKGROUP);
  addTButton(hObjectTools, "mode_shredder", BTNS_CHECKGROUP);
  addTButton(hObjectTools, 0, BTNS_CHECKGROUP);
  addTButton(hObjectTools, "mode_label", BTNS_CHECKGROUP);
  addTButton(hObjectTools, "mode_math", BTNS_CHECKGROUP);
  addTButton(hObjectTools, "mode_paragraph", BTNS_CHECKGROUP);
  addTButton(hObjectTools, "mode_marks", BTNS_CHECKGROUP);
  addTButton(hObjectTools, "mode_rectangles1", BTNS_CHECKGROUP);
  addTButton(hObjectTools, "mode_rectangles2", BTNS_CHECKGROUP);
  addTButton(hObjectTools, "mode_rectangles3", BTNS_CHECKGROUP);
  addTButton(hObjectTools, "mode_parallelogram", BTNS_CHECKGROUP);
  addTButton(hObjectTools, "mode_lines", BTNS_CHECKGROUP);
  addTButton(hObjectTools, "mode_polygons", BTNS_CHECKGROUP);
  addTButton(hObjectTools, "mode_splines", BTNS_CHECKGROUP);
  addTButton(hObjectTools, "mode_splinegons", BTNS_CHECKGROUP);
  addTButton(hObjectTools, "mode_arc1", BTNS_CHECKGROUP);
  addTButton(hObjectTools, "mode_arc2", BTNS_CHECKGROUP);
  addTButton(hObjectTools, "mode_arc3", BTNS_CHECKGROUP);
  addTButton(hObjectTools, "mode_circle1", BTNS_CHECKGROUP);
  addTButton(hObjectTools, "mode_circle2", BTNS_CHECKGROUP);
  addTButton(hObjectTools, "mode_circle3", BTNS_CHECKGROUP);
  addTButton(hObjectTools, "mode_ink", BTNS_CHECKGROUP);

#define MODE_BUTTONS 23

  // size is used for dropdown list!  Don't set to zero.
  for (int i = EUiGridSize; i <= EUiAngleSize; ++i)
    hSelector[i] =
      CreateWindowExW(0, WC_COMBOBOXEXW, NULL,
		      WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST,
		      0, 0, uiscale(100), uiscale(300), hwnd,
		      (HMENU) (ID_SELECTOR_BASE + i), hInst, NULL);

  hRebar = CreateWindowEx(WS_EX_TOOLWINDOW, REBARCLASSNAME, NULL,
			  WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS |
			  WS_CLIPCHILDREN | RBS_VARHEIGHT | RBS_AUTOSIZE |
			  CCS_NODIVIDER | RBS_BANDBORDERS,
			  0, 0, 0, 0,
			  hwnd, NULL,
			  hInst, NULL);

  // Initialize band info for all bands
  REBARBANDINFO rbBand = { sizeof(REBARBANDINFO) };
  rbBand.fMask  = RBBIM_STYLE // fStyle is valid.
    | RBBIM_CHILD       // hwndChild is valid.
    | RBBIM_CHILDSIZE   // child size members are valid.
    | RBBIM_SIZE;       // cx is valid.

  // Get the height of the toolbars.
  DWORD dwBtnSize =
    (DWORD) SendMessage(hEditTools, TB_GETBUTTONSIZE, 0, 0);

  rbBand.fStyle = RBBS_CHILDEDGE | RBBS_GRIPPERALWAYS | RBBS_HIDETITLE;
  rbBand.cyMinChild = HIWORD(dwBtnSize) + 4;
  rbBand.cyChild = HIWORD(dwBtnSize) + 4;
  int bw = LOWORD(dwBtnSize);

  lua_getglobal(L, "prefs");
  lua_getfield(L, -1, "toolbar_order");
  if (lua_istable(L, -1)) {
    int n = lua_rawlen(L, -1);
    for (int i = 1; i <= n; ++i) {
      lua_rawgeti(L, -1, i);
      if (lua_isstring(L, -1)) {
	String s = lua_tostring(L, -1);
	if (s == "edit")
	  insert_tb(hRebar, rbBand, hEditTools, EDIT_BUTTONS * bw);
	else if (s == "grid")
	  insert_tb(hRebar, rbBand, hSelector[EUiGridSize], 160);
	else if (s == "angle")
	  insert_tb(hRebar, rbBand, hSelector[EUiAngleSize], 100);
	else if (s == "snap")
	  insert_tb(hRebar, rbBand, hSnapTools, SNAP_BUTTONS * bw);
	else if (s == "mode")
	  insert_tb(hRebar, rbBand, hObjectTools, MODE_BUTTONS * bw);
      }
      lua_pop(L, 1);
    }
  }
  lua_pop(L, 2); // toolbar_order, prefs

  // ------------------------------------------------------------

  hProperties = CreateWindowExW(0, L"button", L"Properties",
				WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
				0, 100, 200, 280, hwnd, NULL, hInst, NULL);

  for (int i = 0; i <= EUiSymbolSize; ++i) {
    if (i != EUiMarkShape)
      hButton[i] = createButton(hInst, ID_ABSOLUTE_BASE + i);
  }
  // size is used for dropdown list!  Don't set to zero.
  for (int i = 0; i < EUiGridSize; ++i)
    hSelector[i] = CreateWindowExW(0, WC_COMBOBOXEXW, NULL,
				   WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST,
				   0, 0, uiscale(100), uiscale(300), hwnd,
				   (HMENU) (ID_SELECTOR_BASE + i), hInst, NULL);
  SendMessage(hButton[EUiPen], BM_SETIMAGE, IMAGE_BITMAP,
	      (LPARAM) loadButtonIcon("pen", iUiScale));
  SendMessage(hButton[EUiTextSize], BM_SETIMAGE, IMAGE_BITMAP,
	      (LPARAM) loadButtonIcon("mode_label", iUiScale));
  SendMessage(hButton[EUiSymbolSize], BM_SETIMAGE, IMAGE_BITMAP,
	      (LPARAM) loadButtonIcon("mode_marks", iUiScale));
  SendMessage(hButton[EUiStroke], BM_SETIMAGE, IMAGE_BITMAP,
	      (LPARAM) colorIcon(Color(1000, 0, 0), iUiScale));
  SendMessage(hButton[EUiFill], BM_SETIMAGE, IMAGE_BITMAP,
	      (LPARAM) colorIcon(Color(1000, 1000, 0), iUiScale));

  hButton[EUiMarkShape] =
    CreateWindowExW(0, L"static", NULL,
		    WS_CHILD | WS_VISIBLE | SS_BITMAP | SS_CENTERIMAGE,
		    0, 0, 0, 0, hwnd, NULL, hInst, NULL);
  SendMessage(hButton[EUiMarkShape], STM_SETIMAGE, IMAGE_BITMAP,
	      (LPARAM) loadButtonIcon("mode_marks", iUiScale));

  iPathView = new PathView(hwnd, ID_PATHVIEW);

  hViewNumber = createButton(hInst, ID_ABSOLUTE_BASE + EUiView,
			     BS_TEXT|BS_PUSHBUTTON);
  hPageNumber = createButton(hInst, ID_ABSOLUTE_BASE + EUiPage,
			     BS_TEXT|BS_PUSHBUTTON);
  hViewMarked = createButton(hInst, ID_ABSOLUTE_BASE + EUiViewMarked,
			     BS_AUTOCHECKBOX);
  hPageMarked = createButton(hInst, ID_ABSOLUTE_BASE + EUiPageMarked,
			     BS_AUTOCHECKBOX);

  hLayerGroup = CreateWindowExW(0, L"button", L"Layers",
				WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
				0, 0, 0, 0, hwnd, NULL, hInst, NULL);

  hLayers = CreateWindowExW(0, WC_LISTVIEWW, NULL,
			    WS_CHILD | WS_VISIBLE |
			    LVS_LIST | LVS_SINGLESEL,
			    0, 0, 0, 0, hwnd,
			    (HMENU) IDC_LAYERS,
			    hInst, NULL);
  ListView_SetExtendedListViewStyle(hLayers, LVS_EX_CHECKBOXES);

  hNotesGroup = CreateWindowExW(0, L"button", L"Notes",
				WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
				0, 0, 0, 0, hwnd, NULL, hInst, NULL);

  hNotes = CreateWindowExW(0, L"edit", NULL,
			   WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL |
			   ES_READONLY | ES_LEFT | ES_MULTILINE |
			   ES_AUTOVSCROLL,
			   0, 0, 0, 0, hwnd,
			   (HMENU) IDC_NOTES, hInst, NULL);

  hBookmarksGroup = CreateWindowExW(0, L"button", L"Bookmarks",
				    WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
				    0, 0, 0, 0, hwnd, NULL, hInst, NULL);

  hBookmarks = CreateWindowExW(0, L"listbox", NULL,
			       WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_BORDER |
			       LBS_HASSTRINGS | LBS_NOTIFY | LBS_NOSEL,
			       0, 0, 0, 0,
			       hwnd, (HMENU) IDC_BOOKMARK, hInst, NULL);

  Canvas *canvas = new Canvas(hwnd);
  hwndCanvas = canvas->windowId();
  iCanvas = canvas;
  iCanvas->setObserver(this);

  // Tooltips
  setTooltip(hButton[EUiStroke], "Absolute stroke color");
  setTooltip(hButton[EUiFill], "Absolute fill color");
  setTooltip(hButton[EUiPen], "Absolute pen width");
  setTooltip(hButton[EUiTextSize], "Absolute text size");
  setTooltip(hButton[EUiSymbolSize], "Absolute symbol size");

  setTooltip(hSelector[EUiStroke], "Symbolic stroke color", true);
  setTooltip(hSelector[EUiFill], "Symbolic fill color", true);
  setTooltip(hSelector[EUiPen], "Symbolic pen width", true);
  setTooltip(hSelector[EUiTextSize], "Symbolic text size", true);
  setTooltip(hSelector[EUiMarkShape], "Mark shape", true);
  setTooltip(hSelector[EUiSymbolSize], "Symbolic symbol size", true);

  setTooltip(hSelector[EUiGridSize], "Grid size", true);
  setTooltip(hSelector[EUiAngleSize], "Angle for angular snap", true);

  setTooltip(hViewNumber, "Current view number");
  setTooltip(hPageNumber, "Current page number");
  setTooltip(hNotes, "Notes for this page");
  setTooltip(hBookmarks, "Bookmarks of this document");
  setTooltip(hLayers, "Layers of this page");

  setCheckMark("coordinates|", "points");
  setCheckMark("scaling|", "1");
  setCheckMark("mode_", "select");

  SetFocus(hwndCanvas);

  hFont = CreateFontW(uiscale(18), 0, 0, 0, FW_DONTCARE,
		      FALSE, FALSE, FALSE, ANSI_CHARSET,
		      OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
		      DEFAULT_QUALITY,
		      DEFAULT_PITCH | FF_SWISS,
		      L"MS Shell Dlg");
  if (hFont != NULL) {
    SendMessage(hNotes, WM_SETFONT, WPARAM (hFont), TRUE);
    SendMessage(hNotesGroup, WM_SETFONT, WPARAM (hFont), TRUE);
    SendMessage(hBookmarksGroup, WM_SETFONT, WPARAM (hFont), TRUE);
    SendMessage(hBookmarks, WM_SETFONT, WPARAM (hFont), TRUE);
    SendMessage(hProperties, WM_SETFONT, WPARAM (hFont), TRUE);
    SendMessage(hLayerGroup, WM_SETFONT, WPARAM (hFont), TRUE);
    SendMessage(hViewNumber, WM_SETFONT, WPARAM (hFont), TRUE);
    SendMessage(hPageNumber, WM_SETFONT, WPARAM (hFont), TRUE);
  }
  // default is on, Lua code will turn off if necessary
  CheckMenuItem(hMenuBar, actionId("toggle_notes"), MF_CHECKED);
  CheckMenuItem(hMenuBar, actionId("toggle_bookmarks"), MF_CHECKED);
}

AppUi::~AppUi()
{
  // canvas is deleted by Windows
  ImageList_Destroy(hIcons);
  ImageList_Destroy(hColorIcons);
  KillTimer(hwnd, ID_STATUS_TIMER);
  DeleteObject(hFont);
}

void AppUi::layoutChildren(bool resizeRebar)
{
  RECT rc, rc1, rc2;
  GetClientRect(hwnd, &rc);
  SendMessage(hStatusBar, WM_SIZE, 0, 0);
  if (resizeRebar)
    MoveWindow(hRebar, 0, 0, rc.right - rc.left,  10, FALSE);
  GetClientRect(hStatusBar, &rc1);
  GetClientRect(hRebar, &rc2);
  int sbh = rc1.bottom - rc1.top;
  int tbh = rc2.bottom - rc2.top;
  int cvh = rc.bottom - sbh - tbh;
  RECT rect;
  rect.left = 0;
  rect.top = tbh;
  rect.right = uiscale(200);
  rect.bottom = rc.bottom - sbh;
  InvalidateRect(hwnd, &rect, TRUE);
  int pbh = uiscale(28 * 7 + 70);
  MoveWindow(hProperties, uiscale(8), tbh + uiscale(2), uiscale(180), pbh, TRUE);

  for (int i = 0; i <= EUiSymbolSize; ++i) {
    int y = uiscale(24 + 28 * i + (i > 2 ? 42 : 0));
    MoveWindow(hButton[i], uiscale(16), tbh + y,
	       uiscale(26), uiscale(26), TRUE);
    MoveWindow(hSelector[i], uiscale(50), tbh + y,
	       uiscale(132), uiscale(26), TRUE);
  }

  int y = tbh + uiscale(24 + 28 * 6 + 42);
  MoveWindow(hViewNumber, uiscale(16), y, uiscale(68), uiscale(26), TRUE);
  MoveWindow(hPageNumber, uiscale(116), y, uiscale(68), uiscale(26), TRUE);
  MoveWindow(hViewMarked, uiscale(85), y + uiscale(5),
	     uiscale(12), uiscale(12), TRUE);
  MoveWindow(hPageMarked, uiscale(100), y + uiscale(5),
	     uiscale(12), uiscale(12), TRUE);

  MoveWindow(iPathView->windowId(), uiscale(16), tbh + uiscale(24 + 28 * 3),
	     uiscale(168), uiscale(40), TRUE);

  MoveWindow(hLayerGroup, uiscale(8), tbh + uiscale(6) + pbh,
	     uiscale(180), cvh - pbh - uiscale(8), TRUE);
  MoveWindow(hLayers, uiscale(16), tbh + uiscale(24) + pbh,
	     uiscale(164), cvh - pbh - uiscale(32), TRUE);

  bool visNotes = IsWindowVisible(hNotes);
  bool visBookmarks = IsWindowVisible(hBookmarks);
  int cvw = (visNotes || visBookmarks) ?
    rc.right - uiscale(200 + iWidthNotesBookmarks) : rc.right - uiscale(200);
  MoveWindow(hwndCanvas, uiscale(200), tbh, cvw, cvh, TRUE);
  if (visNotes || visBookmarks) {
    rect.left = rc.right - uiscale(iWidthNotesBookmarks);
    rect.top = tbh;
    rect.right = rc.right;
    rect.bottom = rc.bottom - sbh;
    InvalidateRect(hwnd, &rect, TRUE);
  }

  int nth = (visNotes && visBookmarks) ? cvh / 2 - uiscale(4): cvh;
  int nty = tbh;
  if (visNotes) {
    MoveWindow(hNotesGroup, rc.right - uiscale(iWidthNotesBookmarks),
	       nty, uiscale(iWidthNotesBookmarks), nth, TRUE);
    MoveWindow(hNotes, rc.right - uiscale(iWidthNotesBookmarks - 6),
	       nty + uiscale(18), uiscale(iWidthNotesBookmarks - 14),
	       nth - uiscale(24), TRUE);
    nty += nth + uiscale(2);
  }
  if (visBookmarks) {
    MoveWindow(hBookmarksGroup, rc.right - uiscale(iWidthNotesBookmarks),
	       nty, uiscale(iWidthNotesBookmarks), nth, TRUE);
    MoveWindow(hBookmarks, rc.right - uiscale(iWidthNotesBookmarks - 6),
	       nty + uiscale(18),
	       uiscale(iWidthNotesBookmarks - 14), nth - uiscale(24), TRUE);
  }
  int statwidths[] = {rc.right - uiscale(320), rc.right - uiscale(220),
		      rc.right - uiscale(80), rc.right };
  SendMessage(hStatusBar, SB_SETPARTS, 4, (LPARAM) statwidths);
  iCanvas->update();
}

// --------------------------------------------------------------------

// add item to ComboBoxEx
static void addComboEx(HWND h, String s)
{
  WString ws(s);
  COMBOBOXEXITEM item;
  item.mask = CBEIF_TEXT;
  item.iItem = (INT_PTR) -1;
  item.pszText = ws.data();
  SendMessage(h, CBEM_INSERTITEM, 0, (LPARAM) &item);
}

void AppUi::resetCombos()
{
  for (int i = 0; i < EUiView; ++i)
    SendMessage(hSelector[i], CB_RESETCONTENT, 0, 0);
}

void AppUi::addComboColors(AttributeSeq &sym, AttributeSeq &abs)
{
  // clear all icons
  ImageList_Remove(hColorIcons, -1);
  COMBOBOXEXITEM item;
  item.mask = CBEIF_TEXT | CBEIF_IMAGE;
  item.iItem = (INT_PTR) -1;
  for (uint i = 0; i < sym.size(); ++i) {
    Color color = abs[i].color();
    HBITMAP bm = colorIcon(color, iUiScale);
    ImageList_Add(hColorIcons, bm, NULL);
    DeleteObject(bm);
    String s = sym[i].string();
    WString ws(s);
    item.iImage = i;
    item.iSelectedImage = i;
    item.pszText = ws.data();
    SendMessage(hSelector[EUiStroke], CBEM_INSERTITEM, 0, (LPARAM) &item);
    SendMessage(hSelector[EUiFill], CBEM_INSERTITEM, 0, (LPARAM) &item);
    iComboContents[EUiStroke].push_back(s);
    iComboContents[EUiFill].push_back(s);
  }
  SendMessage(hSelector[EUiStroke], CBEM_SETIMAGELIST, 0, (LPARAM) hColorIcons);
  SendMessage(hSelector[EUiFill], CBEM_SETIMAGELIST, 0, (LPARAM) hColorIcons);
}

void AppUi::addCombo(int sel, String s)
{
  addComboEx(hSelector[sel], s);
}

void AppUi::setComboCurrent(int sel, int idx)
{
  SendMessage(hSelector[sel], CB_SETCURSEL, idx, 0);
}

void AppUi::setButtonColor(int sel, Color color)
{
  // ipeDebug("Changing color for %d");
  SendMessage(hButton[sel], BM_SETIMAGE, IMAGE_BITMAP,
	      (LPARAM) colorIcon(color, iUiScale));
}

void AppUi::setPathView(const AllAttributes &all, Cascade *sheet)
{
  iPathView->set(all, sheet);
}

void AppUi::setCheckMark(String name, Attribute a)
{
  setCheckMark(name + "|", a.string());
}

void AppUi::setCheckMark(String name, String value)
{
  String sa = name;
  int na = sa.size();
  String sb = sa + value;
  int first = -1;
  int last = -1;
  int check = -1;
  for (int i = 0; i < int(iActions.size()); ++i) {
    if (iActions[i].name.left(na) == sa) {
      if (first < 0) first = IDBASE + i;
      last = IDBASE + i;
      if (iActions[i].name == sb)
	check = IDBASE + i;
    }
  }
  // ipeDebug("CheckMenuRadioItem %s %d %d %d", name.z(), first, last, check);
  if (check > 0)
    CheckMenuRadioItem(hMenuBar, first, last, check, MF_BYCOMMAND);

  if (name == "mode_") {
    for (int i = 0; i < int(iActions.size()); ++i) {
      if (iActions[i].name.left(na) == sa) {
	int id = IDBASE + i;
	int state = TBSTATE_ENABLED;
	if (id == check) state |= TBSTATE_CHECKED;
	SendMessage(hEditTools, TB_SETSTATE, id, (LPARAM) state);
	SendMessage(hSnapTools, TB_SETSTATE, id, (LPARAM) state);
	SendMessage(hObjectTools, TB_SETSTATE, id, (LPARAM) state);
      }
    }
  }
}

void AppUi::setLayers(const Page *page, int view)
{
  iLayerNames.clear();
  for (int i = 0; i < page->countLayers(); ++i)
    iLayerNames.push_back(page->layer(i));

  iSettingLayers = true;
  // TODO send WM_SETREDRAW
  ListView_DeleteAllItems(hLayers);

  LVITEM lvI;
  lvI.mask      = LVIF_TEXT | LVIF_PARAM;
  lvI.iSubItem  = 0;

  for (int i = 0; i < page->countLayers(); ++i) {
    lvI.iItem   = i;
    lvI.lParam = 0;
    if (page->layer(i) == page->active(view))
      lvI.lParam |=  1;
    if (page->isLocked(i))
      lvI.lParam |=  2;
    if (!page->hasSnapping(i))
      lvI.lParam |=  4;
    WString w(page->layer(i));
    lvI.pszText = w.data();
    ListView_InsertItem(hLayers, &lvI);
    ListView_SetCheckState(hLayers, i, page->visible(view, i));
  }
  iSettingLayers = false;
}

static void enableAction(HWND h, int id, int tstate)
{
  int ostate = SendMessage(h, TB_GETSTATE, id, 0);
  if (ostate != -1)
    SendMessage(h, TB_SETSTATE, id,
		(LPARAM) ((ostate & BST_CHECKED)|tstate));
}

void AppUi::setActionsEnabled(bool mode)
{
  int mstate = mode ? MF_ENABLED : MF_GRAYED;
  int tstate = mode ? TBSTATE_ENABLED : 0;
  for (int i = 0; i < int(iActions.size()); ++i) {
    int id = i + IDBASE;
    if (!iActions[i].alwaysOn) {
      EnableMenuItem(hMenuBar, id, mstate);
      enableAction(hEditTools, id, tstate);
      enableAction(hSnapTools, id, tstate);
      enableAction(hObjectTools, id, tstate);
    }
  }
  EnableWindow(hBookmarks, mode);
}

void AppUi::setNumbers(String vno, bool vm, String pno, bool pm)
{
  setWindowText(hViewNumber, vno.z());
  setWindowText(hPageNumber, pno.z());
  Button_SetCheck(hViewMarked, vm ? BST_CHECKED : BST_UNCHECKED);
  Button_SetCheck(hPageMarked, pm ? BST_CHECKED : BST_UNCHECKED);
  EnableWindow(hViewNumber, !vno.empty());
  EnableWindow(hViewMarked, !vno.empty());
  EnableWindow(hPageNumber, !pno.empty());
  EnableWindow(hPageMarked, !pno.empty());
}

void AppUi::setNotes(String notes)
{
  setWindowText(hNotes, notes.z());
}

void AppUi::closeRequested()
{
  // calls model
  lua_rawgeti(L, LUA_REGISTRYINDEX, iModel);
  lua_getfield(L, -1, "closeEvent");
  lua_pushvalue(L, -2); // model
  lua_remove(L, -3);
  lua_call(L, 1, 1);
  bool result = lua_toboolean(L, -1);
  if (result)
    DestroyWindow(hwnd);
}

void AppUi::closeWindow()
{
  SendMessage(hwnd, WM_CLOSE, 0, 0);
}

// Determine if an action is checked.
// Used for viewmarked, pagemarked, snapXXX, grid_visible,
// pretty_display, toggle_notes, toggle_bookmarks.
bool AppUi::actionState(const char *name)
{
  // ipeDebug("actionState %s", name);
  int idx = actionId(name);
  return GetMenuState(hMenuBar, idx, MF_CHECKED);
}

// Check/uncheck an action.
// Used by Lua for snapangle and grid_visible
// Also to initialize mode_select
// Used from AppUi::action to toggle checkable actions.
void AppUi::setActionState(const char *name, bool value)
{
  // ipeDebug("setActionState %s %d", name, value);
  int idx = actionId(name);
  CheckMenuItem(hMenuBar, idx, value ? MF_CHECKED : MF_UNCHECKED);
  int state = TBSTATE_ENABLED;
  if (value) state |= TBSTATE_CHECKED;
  SendMessage(hEditTools, TB_SETSTATE, idx, (LPARAM) state);
  SendMessage(hSnapTools, TB_SETSTATE, idx, (LPARAM) state);
  SendMessage(hObjectTools, TB_SETSTATE, idx, (LPARAM) state);
}

void AppUi::setBookmarks(int no, const String *s)
{
  SendMessage(hBookmarks, LB_RESETCONTENT, 0, 0);
  for (int i = 0; i < no; ++i)
    sendMessage(hBookmarks, LB_ADDSTRING, s[i].z());
}

void AppUi::setToolVisible(int m, bool vis)
{
  if (m == 1) {
    ShowWindow(hBookmarksGroup, vis ? SW_SHOW : SW_HIDE);
    ShowWindow(hBookmarks, vis ? SW_SHOW : SW_HIDE);
    CheckMenuItem(hMenuBar, actionId("toggle_bookmarks"),
		  vis ? MF_CHECKED : MF_UNCHECKED);
  } else if (m == 2) {
    ShowWindow(hNotesGroup, vis ? SW_SHOW : SW_HIDE);
    ShowWindow(hNotes, vis ? SW_SHOW : SW_HIDE);
    CheckMenuItem(hMenuBar, actionId("toggle_notes"),
		  vis ? MF_CHECKED : MF_UNCHECKED);
  }
  layoutChildren(false);
}

static void clearMenu(HMENU h)
{
  for (int i = GetMenuItemCount(h) - 1; i >= 0; --i)
    DeleteMenu(h, i, MF_BYPOSITION);
}

void AppUi::populateTextStyleMenu()
{
  AttributeSeq seq;
  iCascade->allNames(ETextStyle, seq);
  clearMenu(iTextStyleMenu);
  int check = 0;
  for (uint i = 0; i < seq.size(); ++i) {
    String s = seq[i].string();
    WString ws(s);
    AppendMenuW(iTextStyleMenu, MF_STRING, TEXT_STYLE_BASE + i, ws);
    if (s == iAll.iTextStyle.string())
      check = i;
  }
  CheckMenuRadioItem(iTextStyleMenu, TEXT_STYLE_BASE,
		     TEXT_STYLE_BASE + seq.size() - 1,
		     TEXT_STYLE_BASE + check, MF_BYCOMMAND);
}

void AppUi::populateSizeMenu(HMENU h, int sel, int base)
{
  clearMenu(h);
  auto i = 0;
  for (auto &name : iComboContents[sel]) {
    WString ws(name);
    AppendMenuW(h, MF_STRING, base + i, ws);
    ++i;
  }
  auto cur = SendMessage(hSelector[sel], CB_GETCURSEL, 0, 0);
  if (cur != CB_ERR)
    CheckMenuRadioItem(h, base, base + iComboContents[sel].size() - 1,
		       base + cur, MF_BYCOMMAND);
}

void AppUi::populateSizeMenus()
{
  populateSizeMenu(iGridSizeMenu, EUiGridSize, ID_GRIDSIZE_BASE);
  populateSizeMenu(iAngleSizeMenu, EUiAngleSize, ID_ANGLESIZE_BASE);
}

void AppUi::populateLayerMenus()
{
  clearMenu(iSelectLayerMenu);
  clearMenu(iMoveToLayerMenu);
  for (int i = 0; i < int(iLayerNames.size()); ++i) {
    WString ws(iLayerNames[i]);
    AppendMenuW(iSelectLayerMenu, MF_STRING, ID_SELECTINLAYER_BASE + i, ws);
    AppendMenuW(iMoveToLayerMenu, MF_STRING, ID_MOVETOLAYER_BASE + i, ws);
  }
}

int AppUi::findAction(const char *name) const
{
  for (int i = 0; i < int(iActions.size()); ++i) {
    if (iActions[i].name == name)
      return i;
  }
  return -1;
}

int AppUi::actionId(const char *name) const
{
  return findAction(name) + IDBASE;
}

int AppUi::iconId(const char *name) const
{
  int i = findAction(name);
  if (i >= 0 && iActions[i].icon >= 0)
    return iActions[i].icon;
  else
    return I_IMAGENONE;
}

int AppUi::actionInfo(lua_State *L) const
{
  const char *action = luaL_checkstring(L, 2);
  int i = findAction(action);
  lua_pushinteger(L, (i >= 0) ? i + IDBASE : 0);
  lua_pushboolean(L, (i >= 0) ? iActions[i].alwaysOn : 0);
  return 2;
}

void AppUi::cmd(int id, int notification)
{
  // ipeDebug("WM_COMMAND %d %d", id, notification);
  if (id == IDC_BOOKMARK && notification == LBN_SELCHANGE)
    luaBookmarkSelected(ListBox_GetCurSel(hBookmarks));
  else if (ID_ABSOLUTE_BASE <= id && id <= ID_ABSOLUTE_BASE + EUiPageMarked)
    luaAbsoluteButton(selectorNames[id - ID_ABSOLUTE_BASE]);
  else if (ID_SELECTOR_BASE <= id && id <= ID_SELECTOR_BASE + EUiAngleSize) {
    int sel = id - ID_SELECTOR_BASE;
    int idx = SendMessage(hSelector[sel], CB_GETCURSEL, 0, 0);
    luaSelector(String(selectorNames[sel]), iComboContents[sel][idx]);
  } else if (ID_SELECTINLAYER_BASE <= id &&
	     id < ID_SELECTINLAYER_BASE + int(iLayerNames.size())) {
    action(String("selectinlayer-") + iLayerNames[id - ID_SELECTINLAYER_BASE]);
  } else if (ID_MOVETOLAYER_BASE <= id &&
	     id < ID_MOVETOLAYER_BASE + int(iLayerNames.size())) {
    action(String("movetolayer-") + iLayerNames[id - ID_MOVETOLAYER_BASE]);
  } else if (ID_GRIDSIZE_BASE <= id &&
	     id < ID_GRIDSIZE_BASE + int(iComboContents[EUiGridSize].size())) {
    luaSelector("gridsize", iComboContents[EUiGridSize][id - ID_GRIDSIZE_BASE]);
  } else if (ID_ANGLESIZE_BASE <= id && id < ID_ANGLESIZE_BASE +
	     int(iComboContents[EUiAngleSize].size())) {
    luaSelector("anglesize",
		iComboContents[EUiAngleSize][id - ID_ANGLESIZE_BASE]);
  } else if (id == ID_PATHVIEW) {
    luaShowPathStylePopup(Vector(iPathView->popupPos().x,
				 iPathView->popupPos().y));
  } else if (id == ID_PATHVIEW+1) {
    action(iPathView->action());
  } else if (IDBASE <= id && id < IDBASE + int(iActions.size()))
    action(iActions[id - IDBASE].name);
  else
    ipeDebug("Unknown cmd %d", id);
}

static const char * const aboutText =
  "Ipe %d.%d.%d\n\n"
  "Copyright (c) 1993-2016 Otfried Cheong\n\n"
  "The extensible drawing editor Ipe creates figures "
  "in Postscript and PDF format, "
  "using LaTeX to format the text in the figures.\n"
  "Ipe relies on the following fine pieces of software:\n\n"
  " * Pdftex, Xetex, or Luatex\n"
  " * %s (%d kB used)\n" // Lua
  " * The font rendering library %s\n"
  " * The rendering library Cairo %s\n"
  " * The compression library zlib %s\n\n"
  "Ipe is released under the GNU Public License.\n"
  "See http://ipe.otfried.org for details.";

void AppUi::aboutIpe()
{
  int luaSize = lua_gc(L, LUA_GCCOUNT, 0);
  std::vector<char> buf(strlen(aboutText) + 200);
  sprintf(&buf[0], aboutText,
	  IPELIB_VERSION / 10000,
	  (IPELIB_VERSION / 100) % 100,
	  IPELIB_VERSION % 100,
	  LUA_RELEASE, luaSize,
	  Fonts::freetypeVersion().z(),
	  cairo_version_string(),
	  ZLIB_VERSION);

  MessageBoxA(hwnd, &buf[0], "About Ipe",
	      MB_OK | MB_ICONINFORMATION | MB_APPLMODAL);
}

void AppUi::action(String name)
{
  ipeDebug("action %s", name.z());
  int id = actionId(name.z());
  if (name == "fullscreen") {
    toggleFullscreen();
  } else if (name == "about")
    aboutIpe();
  else if (name == "shift_key") {
    if (iCanvas) {
      int mod = 0;
      if (SendMessage(hEditTools, TB_GETSTATE, (WPARAM) id, 0)
	  & TBSTATE_CHECKED)
	mod |= CanvasBase::EShift;
      iCanvas->setAdditionalModifiers(mod);
    }
  } else {
    // Implement radio buttons
    int i = name.find("|");
    if (i >= 0)
      setCheckMark(name.left(i+1), name.substr(i+1));
    if (name.hasPrefix("mode_"))
      setCheckMark("mode_", name.substr(5));
    if (name.hasPrefix("snap") || name == "grid_visible" ||
	name == "pretty_display" || name.hasPrefix("toggle_"))
      setActionState(name.z(), !GetMenuState(hMenuBar, id, MF_CHECKED));
    luaAction(name);
  }
}

WINID AppUi::windowId()
{
  return hwnd;
}

void AppUi::setWindowCaption(bool mod, const char *s)
{
  setWindowText(hwnd, s);
}

void AppUi::showWindow(int width, int height)
{
  SetWindowPos(hwnd, NULL, 0, 0, width, height, SWP_NOMOVE);
  ShowWindow(hwnd, win_nCmdShow);
  UpdateWindow(hwnd);
}

// show for t milliseconds, or permanently if t == 0
void AppUi::explain(const char *s, int t)
{
  if (t)
    SetTimer(hwnd, ID_STATUS_TIMER, t, NULL);
  else
    KillTimer(hwnd, ID_STATUS_TIMER);
  sendMessage(hStatusBar, SB_SETTEXT, s);
}

void AppUi::setSnapIndicator(const char *s)
{
  sendMessage(hStatusBar, SB_SETTEXT, s, 1);
}

void AppUi::setMouseIndicator(const char *s)
{
  sendMessage(hStatusBar, SB_SETTEXT, s, 2);
}

void AppUi::setZoom(double zoom)
{
  char s[32];
  sprintf(s, "%dppi", int(72.0 * zoom));
  iCanvas->setZoom(zoom);
  sendMessage(hStatusBar, SB_SETTEXT, s, 3);
}

// --------------------------------------------------------------------

static void listFormats()
{
  UINT format = 0;
  char name[256];
  while ((format = EnumClipboardFormats(format)) != 0) {
    GetClipboardFormatNameA(format, name, 256);
    ipeDebug("Clipboard format %d %s", format, name);
  }
}

static int clipboardBitmap(lua_State *L)
{
  HBITMAP bm = (HBITMAP) GetClipboardData(CF_BITMAP);
  BITMAPINFO bmi;
  BITMAPINFOHEADER *s = (BITMAPINFOHEADER *) &bmi;
  s->biSize = sizeof(BITMAPINFOHEADER);
  s->biBitCount = 0;
  s->biClrUsed = 0;
  HDC hdc = GetDC(NULL); // CreateCompatibleDC(NULL);
  if (GetDIBits(hdc, bm, 0, 0, NULL, &bmi, DIB_RGB_COLORS) == 0) {
    ipeDebug("AppUi::clipboard() GetDIBits failed");
    ReleaseDC(NULL, hdc);
    CloseClipboard();
    return 0;
  }
  int w = s->biWidth;
  int h = s->biHeight;
  Vector res(s->biXPelsPerMeter, s->biYPelsPerMeter);

  ipeDebug("AppUi::clipboard() bitmap: %d x %d resolution %g x %g %d %d",
	   w, h, res.x, res.y, s->biCompression, s->biBitCount);

  s->biPlanes = 1;
  s->biCompression = BI_RGB;
  s->biSizeImage = 0;

  Buffer pixels(w * h * 3);
  DWORD *bits = (DWORD *)
    VirtualAlloc(NULL, 4 * w, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

  char *p = pixels.data();
  for (int y = h-1; y >= 0; --y) {
    if (GetDIBits(hdc, bm, y, 1, bits, &bmi, DIB_RGB_COLORS) == 0) {
      ipeDebug("AppUi::clipboard() GetDIBits failed to retrieve bits");
      VirtualFree(bits, 0, MEM_RELEASE);
      ReleaseDC(NULL, hdc);
      CloseClipboard();
      return 0;
    }
    DWORD *r = bits;
    for (int x = 0; x < w; ++x) {
      DWORD rr = *r++;
      *p++ = (rr >> 16) & 0xff;
      *p++ = (rr >> 8) & 0xff;
      *p++ = rr & 0xff;
    }
  }

  Bitmap ibm(w, h, Bitmap::EDeviceRGB, 8, pixels, Bitmap::EDirect, true);
  Rect r(Vector::ZERO, Vector(w, h));
  Image *img = new Image(r, ibm);
  push_object(L, img);
  VirtualFree(bits, 0, MEM_RELEASE);
  ReleaseDC(NULL, hdc);
  CloseClipboard();
  return 1;
}

int AppUi::clipboard(lua_State *L)
{
  bool allow_bitmap = lua_toboolean(L, 2);
  OpenClipboard(hwnd);
  listFormats();
  UINT format = 0;
  while ((format = EnumClipboardFormats(format)) != 0) {
    if (format == CF_BITMAP || format == CF_UNICODETEXT)
      break;
  }
  if (allow_bitmap && format == CF_BITMAP)
    return clipboardBitmap(L);
  if (format == CF_UNICODETEXT){
    HGLOBAL hGlobal = GetClipboardData(CF_UNICODETEXT);
    if (hGlobal == NULL) {
      CloseClipboard();
      return 0;
    }
    String s((wchar_t *) GlobalLock(hGlobal));
    GlobalUnlock(hGlobal);
    CloseClipboard();
    lua_pushstring(L, s.z());
    return 1;
  }
  CloseClipboard();
  return 0;
}

int AppUi::setClipboard(lua_State *L)
{
  const char *s = luaL_checkstring(L, 2);
  WString ws(s);
  size_t size = (ws.size() + 1) * sizeof(wchar_t);
  HGLOBAL hGlobal = GlobalAlloc(GMEM_MOVEABLE, size);
  wchar_t *p = (wchar_t *) GlobalLock(hGlobal);
  memcpy(p, ws.data(), size);
  GlobalUnlock(hGlobal);
  OpenClipboard(hwnd);
  EmptyClipboard();
  if (SetClipboardData(CF_UNICODETEXT, hGlobal) == NULL)
    GlobalFree(hGlobal);  // clipboard didn't take the data
  CloseClipboard();
  return 0;
}

// --------------------------------------------------------------------

struct WindowCounter {
  int count;
  HWND hwnd;
};

BOOL CALLBACK AppUi::enumThreadWndProc(HWND hwnd, LPARAM lParam)
{
  WindowCounter *wc = (WindowCounter *) lParam;
  // ipeDebug("Enumerating Window %d", int(hwnd));
  wchar_t cname[100];
  if (GetClassNameW(hwnd, cname, 100) && !wcscmp(cname, className) &&
      wc->hwnd != hwnd)
    wc->count += 1;
  return TRUE;
}

LRESULT CALLBACK AppUi::wndProc(HWND hwnd, UINT message, WPARAM wParam,
				LPARAM lParam)
{
  AppUi *ui = (AppUi*) GetWindowLongPtr(hwnd, GWLP_USERDATA);
  switch (message) {
  case WM_CREATE:
    {
      LPCREATESTRUCT p = (LPCREATESTRUCT) lParam;
      ui = (AppUi *) p->lpCreateParams;
      ui->hwnd = hwnd;
      SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR) ui);
      ui->initUi();
    }
    break;
  case WM_INITMENUPOPUP:
    if (ui) {
      if (lParam == 2)
	ui->populateTextStyleMenu();
      else if (lParam == 3)
	ui->populateSizeMenus();
      else if (lParam == 6)
	ui->populateLayerMenus();
    }
    break;
  case WM_ACTIVATE:
    if (ui && ui->iCanvas && LOWORD(wParam))
      SetFocus(ui->hwndCanvas);
    return 0;
  case WM_COMMAND:
    if (ui)
      ui->cmd(LOWORD(wParam), HIWORD(wParam));
    break;
  case WM_CTLCOLORSTATIC:
    if (ui && ui->iCanvas && HWND(lParam) == ui->hNotes)
      return (INT_PTR) (HBRUSH) GetStockObject(WHITE_BRUSH);
    break;
  case WM_TIMER:
    if (ui && ui->iCanvas)
      SendMessageW(ui->hStatusBar, SB_SETTEXT, 0, (LPARAM) 0);
    return 0;
  case WM_SIZE:
    if (ui && ui->iCanvas)
      ui->layoutChildren(true);
    break;
  case WM_NOTIFY:
    switch (((LPNMHDR)lParam)->code) {
    case RBN_CHILDSIZE:
      if (ui && ui->iCanvas)
	ui->layoutChildren(false);
      break;
    case  NM_CUSTOMDRAW: {
      switch (((LPNMCUSTOMDRAW) lParam)->dwDrawStage) {
      case CDDS_PREPAINT:
	return CDRF_NOTIFYITEMDRAW;
      case CDDS_ITEMPREPAINT: {
	LPNMLVCUSTOMDRAW q = (LPNMLVCUSTOMDRAW) lParam;
	uint flags = q->nmcd.lItemlParam;
	if (flags & 0x01)   // active
	  q->clrTextBk = 0x0000ffff;
	if (flags & 0x02)   // locked
	  q->clrTextBk = RGB(255, 220, 220);
	if (flags & 0x04)   // no snapping
	  q->clrText = RGB(0, 0, 160);
	return CDRF_NEWFONT; }
      default:
	break;
      }
      break;
    }
    case NM_RCLICK:
      if (ui) {
	LPNMITEMACTIVATE nm = (LPNMITEMACTIVATE) lParam;
	HWND from = nm->hdr.hwndFrom;
	int l = nm->iItem;
	if (from == ui->hLayers && l >= 0) {
	  POINT pos = ((LPNMITEMACTIVATE) lParam)->ptAction;
	  ClientToScreen(ui->hLayers, &pos);
	  Vector v(pos.x, pos.y);
	  ui->luaShowLayerBoxPopup(v, ui->iLayerNames[l]);
	  return 1; // no default processing
	}
      }
      break;
    case LVN_ITEMACTIVATE:
      if (ui) {
	int l = ((LPNMITEMACTIVATE) lParam)->iItem;
	if (l >= 0) {
	  ui->luaLayerAction("active", ui->iLayerNames[l]);
	}
      }
      break;
    case LVN_ITEMCHANGED:
      if (ui && !ui->iSettingLayers) {
	int i = ((LPNMLISTVIEW) lParam)->iItem;
	if (i >= 0) {
	  bool checked = (ListView_GetCheckState(ui->hLayers, i) != 0);
	  ui->luaLayerAction(checked ? "selecton" : "selectoff",
			     ui->iLayerNames[i]);
	}
      }
      break;
    }
    break;
  case WM_CLOSE:
    if (ui) {
      ui->closeRequested();
      return 0; // do NOT allow default procedure to close
    }
    break;
  case WM_DESTROY:
    {
      delete ui;
      // only post quit message when this was the last window
      WindowCounter wc;
      wc.count = 0;
      wc.hwnd = hwnd;
      EnumThreadWindows(GetCurrentThreadId(), enumThreadWndProc, (LPARAM) &wc);
      if (wc.count == 0)
	PostQuitMessage(0);
    }
    break;
  default:
    break;
  }
  return DefWindowProc(hwnd, message, wParam, lParam);
}

// --------------------------------------------------------------------

bool AppUi::isDrawing(HWND target)
{
  // check that it really is an AppUi window
  WNDPROC w = (WNDPROC) GetWindowLongPtr(target, GWL_WNDPROC);
  if (w != AppUi::wndProc)
    return false;
  AppUi *ui = (AppUi*) GetWindowLongPtr(target, GWLP_USERDATA);
  return ui && ui->iCanvas && ui->iCanvas->tool();
}

void AppUi::init(HINSTANCE hInstance, int nCmdShow)
{
  WNDCLASSEX wc;
  wc.cbSize = sizeof(WNDCLASSEX);
  wc.style = 0;
  wc.lpfnWndProc = wndProc;
  wc.cbClsExtra	 = 0;
  wc.cbWndExtra	 = 0;
  wc.hInstance	 = hInstance;
  wc.hCursor = LoadCursor(NULL, IDC_ARROW);
  wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE+1);
  wc.lpszMenuName = NULL;
  wc.lpszClassName = className;
  wc.hIcon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_MYICON));
  wc.hIconSm =
    HICON(LoadImage(GetModuleHandle(NULL),
		    MAKEINTRESOURCE(IDI_MYICON), IMAGE_ICON, 16, 16, 0));

  if (!RegisterClassEx(&wc)) {
    MessageBoxA(NULL, "AppUi registration failed!", "Error!",
		MB_ICONEXCLAMATION | MB_OK);
    exit(9);
  }
  Canvas::init(hInstance);
  PathView::init(hInstance);
  // save for later use
  win_hInstance = hInstance;
  win_nCmdShow = nCmdShow;
}

AppUiBase *createAppUi(lua_State *L0, int model)
{
  return new AppUi(L0, model);
}

// --------------------------------------------------------------------
