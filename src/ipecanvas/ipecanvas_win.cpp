// --------------------------------------------------------------------
// ipe::Canvas for Win
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

// for touch gestures:  need at least Windows 7
#define WINVER 0x0601
#define _WIN32_WINNT 0x0601

#include "ipecanvas_win.h"
#include "ipecairopainter.h"
#include "ipetool.h"

#include <cairo.h>
#include <cairo-win32.h>

#include <windows.h>
#include <windowsx.h>

using namespace ipe;

typedef WINBOOL WINAPI (*LPFNGETGESTUREINFO)(HGESTUREINFO hGestureInfo,
					     PGESTUREINFO pGestureInfo);
static LPFNGETGESTUREINFO pGetGestureInfo = 0;

const wchar_t Canvas::className[] = L"ipeCanvasWindowClass";

// --------------------------------------------------------------------

void Canvas::invalidate()
{
  InvalidateRect(hwnd, NULL, FALSE);
}

void Canvas::invalidate(int x, int y, int w, int h)
{
  RECT r;
  r.left = x;
  r.right = x + w;
  r.top = y;
  r.bottom = y + h;
  // ipeDebug("invalidate %d %d %d %d\n", r.left, r.right, r.top, r.bottom);
  InvalidateRect(hwnd, &r, FALSE);
}

// --------------------------------------------------------------------

void Canvas::key(WPARAM wParam, LPARAM lParam)
{
  int mod = 0;
  if (GetKeyState(VK_SHIFT) < 0)
    mod |= EShift;
  if (GetKeyState(VK_CONTROL) < 0)
    mod |= EControl;
  if (GetKeyState(VK_MENU) < 0)
    mod |= EAlt;
  // ipeDebug("Canvas::key(%x, %x) %x", wParam, lParam, mod);
  String t;
  t.append(char(wParam));
  iTool->key(t, mod);
}

void Canvas::button(int button, bool down, int mod, int xPos, int yPos)
{
  // ipeDebug("Canvas::button %d %d %d %d", button, down, xPos, yPos);
  POINT p;
  p.x = xPos;
  p.y = yPos;
  ClientToScreen(hwnd, &p);
  iGlobalPos = Vector(p.x, p.y);
  computeFifi(xPos, yPos);
  mod |= iAdditionalModifiers;
  if (iTool)
    iTool->mouseButton(button | mod, down);
  else if (down && iObserver)
    iObserver->canvasObserverMouseAction(button | mod);
}

void Canvas::mouseMove(int xPos, int yPos)
{
  // ipeDebug("Canvas::mouseMove %d %d", xPos, yPos);
  computeFifi(xPos, yPos);
  if (iTool)
    iTool->mouseMove();
  if (iObserver)
    iObserver->canvasObserverPositionChanged();
}

void Canvas::setCursor(TCursor cursor, double w, Color *color)
{
  if (cursor == EDotCursor) {
    // Windows switches to a dot automatically when using the pen,
    // not implemented
  } else {
    // HandCursor, CrossCursor not implemented,
    // but they are also not used in Ipe now.
  }
}

// --------------------------------------------------------------------

void Canvas::updateSize()
{
  RECT rc;
  GetClientRect(hwnd, &rc);
  iBWidth = iWidth = rc.right;
  iBHeight = iHeight = rc.bottom;
}

void Canvas::wndPaint()
{
  PAINTSTRUCT ps;
  HDC hdc = BeginPaint(hwnd, &ps);

  if (!iWidth)
    updateSize();
  refreshSurface();

  int x0 = ps.rcPaint.left;
  int y0 = ps.rcPaint.top;
  int w = ps.rcPaint.right - x0;
  int h = ps.rcPaint.bottom - y0;

  HBITMAP bits = CreateCompatibleBitmap(hdc, w, h);
  HDC bitsDC = CreateCompatibleDC(hdc);
  SelectObject(bitsDC, bits);
  cairo_surface_t *surface = cairo_win32_surface_create(bitsDC);

  cairo_t *cr = cairo_create(surface);
  cairo_translate(cr, -x0, -y0);
  cairo_set_source_surface(cr, iSurface, 0.0, 0.0);
  cairo_paint(cr);

  if (iFifiVisible)
    drawFifi(cr);

  if (iPage) {
    CairoPainter cp(iCascade, iFonts, cr, iZoom, false);
    cp.transform(canvasTfm());
    cp.pushMatrix();
    drawTool(cp);
    cp.popMatrix();
  }
  cairo_destroy(cr);
  cairo_surface_destroy(surface);
  BitBlt(hdc, x0, y0, w, h, bitsDC, 0, 0, SRCCOPY);
  DeleteDC(bitsDC);
  DeleteObject(bits);
  EndPaint(hwnd, &ps);
}

// --------------------------------------------------------------------

void Canvas::mouseButton(Canvas *canvas, int button, bool down,
			 WPARAM wParam, LPARAM lParam)
{
  if (canvas) {
    int xPos = GET_X_LPARAM(lParam);
    int yPos = GET_Y_LPARAM(lParam);
    int mod = 0;
    if (wParam & MK_SHIFT)
      mod |= EShift;
    if (wParam & MK_CONTROL)
      mod |= EControl;
    if (GetKeyState(VK_MENU) < 0)
      mod |= EAlt;
    canvas->button(button, down, mod, xPos, yPos);
  }
}

LRESULT Canvas::handlePanGesture(DWORD flags, POINTS &p)
{
  Vector v(p.x, -p.y);
  if (flags & GF_BEGIN) {
    iGestureStart = v;
    iGesturePan = pan();
  } else {
    v = v - iGestureStart;
    setPan(iGesturePan - (1.0/zoom()) * v);
    update();
  }
  return 0;
}

LRESULT Canvas::handleZoomGesture(DWORD flags, POINTS &p, ULONG d)
{
  if (flags & GF_BEGIN) {
    iGestureDist = d;
    iGestureZoom = zoom();
  } else {
    POINT q = { p.x, p.y };
    ScreenToClient(hwnd, &q);
    Vector origin = devToUser(Vector(q.x, q.y));
    Vector offset = iZoom * (pan() - origin);
    double nzoom = iGestureZoom * d / iGestureDist;
    setZoom(nzoom);
    setPan(origin + (1.0/nzoom) * offset);
    update();
    if (iObserver)
      // scroll wheel hasn't moved, but update display of ppi
      iObserver->canvasObserverWheelMoved(0, true);
  }
  return 0;
}

LRESULT CALLBACK Canvas::wndProc(HWND hwnd, UINT message, WPARAM wParam,
				 LPARAM lParam)
{
  Canvas *canvas = (Canvas *) GetWindowLongPtr(hwnd, GWLP_USERDATA);
  switch (message) {
  case WM_CREATE:
    {
      assert(canvas == 0);
      LPCREATESTRUCT p = (LPCREATESTRUCT) lParam;
      canvas = (Canvas *) p->lpCreateParams;
      canvas->hwnd = hwnd;
      SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR) canvas);
    }
    break;
  case WM_MOUSEACTIVATE:
    if (canvas)
      SetFocus(canvas->hwnd);
    return MA_ACTIVATE;
  case WM_CHAR:
    if (canvas && canvas->iTool) {
      canvas->key(wParam, lParam);
      return 0;
    }
    break;
  case WM_MOUSEWHEEL:
    if (canvas && canvas->iObserver)
      canvas->iObserver->canvasObserverWheelMoved
	(GET_WHEEL_DELTA_WPARAM(wParam) / 120.0, true);
    return 0;
  case WM_PAINT:
    if (canvas)
      canvas->wndPaint();
    return 0;
  case WM_SIZE:
    if (canvas)
      canvas->updateSize();
    break;
  case WM_GESTURE: {
    if (!pGetGestureInfo)
      break;  // GetGestureInfo not available (Windows Vista or older)
    GESTUREINFO gi = {0};
    gi.cbSize = sizeof(gi);
    BOOL res = pGetGestureInfo((HGESTUREINFO) lParam, &gi);
    if (res && canvas) {
      if (gi.dwID == GID_PAN)
	return canvas->handlePanGesture(gi.dwFlags, gi.ptsLocation);
      else if (gi.dwID == GID_ZOOM)
	return canvas->handleZoomGesture(gi.dwFlags, gi.ptsLocation,
					 gi.ullArguments);
    }
    break; }
  case WM_MOUSEMOVE:
    if (canvas) {
      int xPos = GET_X_LPARAM(lParam);
      int yPos = GET_Y_LPARAM(lParam);
      canvas->mouseMove(xPos, yPos);
    }
    break;
  case WM_LBUTTONDOWN:
  case WM_LBUTTONUP:
    mouseButton(canvas, 1, message == WM_LBUTTONDOWN, wParam, lParam);
    break;
  case WM_LBUTTONDBLCLK:
    mouseButton(canvas, 0x81, true, wParam, lParam);
    break;
  case WM_RBUTTONDOWN:
  case WM_RBUTTONUP:
    mouseButton(canvas, 2, message == WM_RBUTTONDOWN, wParam, lParam);
    break;
  case WM_MBUTTONDOWN:
  case WM_MBUTTONUP:
    mouseButton(canvas, 4, message == WM_MBUTTONDOWN, wParam, lParam);
    break;
  case WM_XBUTTONDOWN:
  case WM_XBUTTONUP: {
    int but = (HIWORD(wParam) == XBUTTON2) ? 0x10 : 0x08;
    mouseButton(canvas, but, message == WM_XBUTTONDOWN, wParam, lParam);
    break; }
  case WM_GETMINMAXINFO: {
    LPMINMAXINFO mm = LPMINMAXINFO(lParam);
    ipeDebug("Canvas MINMAX %ld %ld", mm->ptMinTrackSize.x,
	     mm->ptMinTrackSize.y);
    break; }
  case WM_DESTROY:
    ipeDebug("Windows canvas is destroyed");
    delete canvas;
    break;
  default:
    // ipeDebug("Canvas wndProc(%d)", message);
    break;
  }
  return DefWindowProc(hwnd, message, wParam, lParam);
}

// --------------------------------------------------------------------

Canvas::Canvas(HWND parent)
{
  HWND hwnd = CreateWindowExW(WS_EX_CLIENTEDGE, className, L"",
			      WS_CHILD | WS_VISIBLE,
			      0, 0, 0, 0,
			      parent, NULL,
			      (HINSTANCE) GetWindowLong(parent, GWL_HINSTANCE),
			      this);
  if (hwnd == NULL) {
    MessageBoxA(NULL, "Canvas creation failed!", "Error!",
		MB_ICONEXCLAMATION | MB_OK);
    exit(9);
  }
  assert(GetWindowLongPtr(hwnd, GWLP_USERDATA));
}

void Canvas::init(HINSTANCE hInstance)
{
  WNDCLASSEX wc;
  wc.cbSize = sizeof(WNDCLASSEX);
  wc.style = CS_DBLCLKS; // CS_VREDRAW | CS_HREDRAW;
  wc.lpfnWndProc = wndProc;
  wc.cbClsExtra	 = 0;
  wc.cbWndExtra	 = 0;
  wc.hInstance	 = hInstance;
  wc.hCursor = LoadCursor(NULL, IDC_ARROW);
  wc.hbrBackground = (HBRUSH) GetStockObject(NULL_BRUSH);
  wc.lpszMenuName = NULL;  // actually child id
  wc.lpszClassName = className;
  wc.hIcon = NULL;
  wc.hIconSm = NULL;

  if (!RegisterClassExW(&wc)) {
    MessageBoxA(NULL, "Canvas control registration failed!", "Error!",
		MB_ICONEXCLAMATION | MB_OK);
    exit(9);
  }

  // Check if GetGestureInfo is available (i.e. at least Windows 7)
  HMODULE hDll = LoadLibraryA("user32.dll");
  if (hDll)
    pGetGestureInfo = (LPFNGETGESTUREINFO)
      GetProcAddress(hDll, "GetGestureInfo");
}

// --------------------------------------------------------------------
