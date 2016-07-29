// -*- objc -*-
// ipecanvas_cocoa.mm
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

#include "ipecanvas_cocoa.h"
#include "ipecairopainter.h"

#include <cairo.h>
#include <cairo-quartz.h>
#include <CoreGraphics/CoreGraphics.h>

using namespace ipe;

// --------------------------------------------------------------------

static CGContextRef gc_graphics_port()
{
  return (CGContextRef) [[NSGraphicsContext currentContext] graphicsPort];
}

#if MAC_OS_X_VERSION_MAX_ALLOWED >= 101000
static CGContextRef gc_cgcontext()
{
  return [[NSGraphicsContext currentContext] CGContext];
}
#endif

CGContextRef ipeGetCGContext()
{
#if MAC_OS_X_VERSION_MAX_ALLOWED >= 101000
  if ([NSGraphicsContext instancesRespondToSelector:@selector(CGContext)])
    return gc_cgcontext();
#endif
  return gc_graphics_port();
}

// --------------------------------------------------------------------

Canvas::Canvas(IpeCanvasView *view)
{
  iView = view;
  iLayer = NULL;

  iGCGetter = gc_graphics_port;
#if MAC_OS_X_VERSION_MAX_ALLOWED >= 101000
  if ([NSGraphicsContext instancesRespondToSelector:@selector(CGContext)])
    iGCGetter = gc_cgcontext;
#endif
}

Canvas::~Canvas()
{
  CGLayerRelease(iLayer);
}

void Canvas::invalidate()
{
  [iView setNeedsDisplayInRect:[iView bounds]];
}

void Canvas::invalidate(int x, int y, int w, int h)
{
  NSRect rect;
  rect.origin.x = x;
  rect.origin.y = iHeight - 1 - y - h;
  rect.size.width = w;
  rect.size.height = h;
  [iView setNeedsDisplayInRect:rect];
}

void Canvas::setCursor(TCursor cursor, double w, Color *color)
{
  // TODO: not implemented
  // [[NSCursor closedHandCursor] push];
}

void Canvas::refreshLayer()
{
  bool copyLayer = iRepaintObjects;
  refreshSurface();

  CGSize layerSize = { 0.0, 0.0 };
  if (iLayer)
    layerSize = CGLayerGetSize(iLayer);

  if (layerSize.width != iWidth || layerSize.height != iHeight) {
    // size has changed
    if (iLayer)
      CGLayerRelease(iLayer);
    iLayer = NULL;
  }

  if (!iLayer || copyLayer) {
    CGContextRef myContext = iGCGetter();
    if (!iLayer) {
      CGSize size = { CGFloat(iWidth), CGFloat(iHeight) };
      iLayer = CGLayerCreateWithContext(myContext, size, NULL);
    }
    // TODO:  Implement Quartz painter and draw directly into layer,
    // instead of maintaining a Cairo surface and blitting that?
    CGContextRef layerContext = CGLayerGetContext(iLayer);
    cairo_surface_t *surface =
      cairo_quartz_surface_create_for_cg_context(layerContext, iWidth, iHeight);
    cairo_t *cr = cairo_create(surface);
    cairo_set_source_surface(cr, iSurface, 0.0, 0.0);
    if (iWidth != iBWidth) {
      cairo_matrix_t matrix;
      cairo_matrix_init_scale(&matrix, iBWidth / iWidth, iBHeight / iHeight);
      cairo_pattern_set_matrix(cairo_get_source(cr), &matrix);
    }
    cairo_paint(cr);
    cairo_destroy(cr);
    cairo_surface_finish(surface);
    cairo_surface_destroy(surface);
  }
}

void Canvas::drawRect(NSRect rect)
{
  bool resize = [iView inLiveResize];
  NSSize s = [iView bounds].size;
  NSSize sb = [iView convertSizeToBacking:s];
  iWidth = s.width;
  iHeight = s.height;
  iBWidth = sb.width;
  iBHeight = sb.height;

  if (!resize)
    refreshLayer();

  CGContextRef myContext = iGCGetter();

  CGContextTranslateCTM(myContext, 0.0, iHeight);
  CGContextScaleCTM(myContext, 1.0, -1.0);

  if (iLayer)
    CGContextDrawLayerInRect(myContext, [iView bounds], iLayer);

  if (resize) // don't draw tool during live resize
    return;

  cairo_surface_t *surface =
    cairo_quartz_surface_create_for_cg_context(myContext, iWidth, iHeight);
  cairo_t *cr = cairo_create(surface);
  if (iFifiVisible)
    drawFifi(cr);  // draw with Quartz instead?
  if (iPage) {
    CairoPainter cp(iCascade, iFonts, cr, iZoom, false);
    cp.transform(canvasTfm());
    cp.pushMatrix();
    drawTool(cp);
    cp.popMatrix();
  }
  cairo_destroy(cr);
  cairo_surface_finish(surface);
  cairo_surface_destroy(surface);
}

static int getModifiers(NSEvent *event)
{
  uint modifierFlags = [event modifierFlags];
  int mod = 0;
  if (modifierFlags & NSShiftKeyMask)
    mod |= CanvasBase::EShift;
  if (modifierFlags & NSControlKeyMask)
    mod |= CanvasBase::EControl;
  if (modifierFlags & NSCommandKeyMask)
    mod |= CanvasBase::ECommand;
  if (modifierFlags & NSAlternateKeyMask)
    mod |= CanvasBase::EAlt;
  return mod;
}

void Canvas::button(bool down, NSEvent *event)
{
  // pw is in window coordinates, p in view coordinates,
  // ps in screen coordinates
  NSPoint pw = [event locationInWindow];
  NSRect rw = { pw, { 100.0, 100.0 }};
  NSRect rs = [[iView window] convertRectToScreen:rw];
  NSPoint ps = rs.origin;
  NSPoint p = [iView convertPoint:pw fromView:nil];
  // flip y-axis
  p.y = iHeight - 1 - p.y;
  // 0, 1, 2 for left, right, middle
  int cbutton = [event buttonNumber];
  // change to 1, 2, 4 ..
  int button = 1;
  while (cbutton--)
    button *= 2;
  if (down && button == 1 && [event clickCount] == 2)
    button = 0x81; // left double click
  iGlobalPos = Vector(ps.x, ps.y);
  computeFifi(p.x, p.y);
  int mod = getModifiers(event) | iAdditionalModifiers;
  if (iTool)
    iTool->mouseButton(button | mod, down);
  else if (down && iObserver)
    iObserver->canvasObserverMouseAction(button | mod);
}

void Canvas::mouseMove(NSEvent *event)
{
  NSPoint p = [iView convertPoint:[event locationInWindow] fromView:nil];
  computeFifi(p.x, iHeight - 1 - p.y);
  if (iTool)
    iTool->mouseMove();
  if (iObserver)
    iObserver->canvasObserverPositionChanged();
}

bool Canvas::key(NSEvent *event)
{
  if (iTool) {
    int mod = getModifiers(event);
    NSString  *characters = [event charactersIgnoringModifiers];
    String t(characters.UTF8String);
    return iTool->key(t, mod);
  } else
    return false;
}

// TODO:
// Use prefs to determine if we want panning rather than
// magnification changes (which can be done using gestures).
void Canvas::scrollWheel(NSEvent *event)
{
  if (!iObserver)
    return;
  Vector v(event.scrollingDeltaX, -event.scrollingDeltaY);
  // NSLog(@"scroll: %lu %g %g", event.phase, v.x, v.y);
  v = (1.0/zoom()) * v;
  if (event.phase == NSEventPhaseBegan || event.phase == NSEventPhaseNone)
    iGesturePan = pan();
  setPan(iGesturePan - v);
  update();
  double delta = [event scrollingDeltaY];
  bool precise = [event hasPreciseScrollingDeltas];
  iObserver->canvasObserverWheelMoved(delta, precise);
}

// --------------------------------------------------------------------

@implementation IpeCanvasView

- (instancetype) initWithFrame:(NSRect) rect
{
  if ([super initWithFrame:rect])
    _canvas = new Canvas(self);
  return self;
}

- (BOOL) acceptsFirstResponder
{
  return YES;
}

- (BOOL) isOpaque
{
  return YES;
}

- (void) drawRect:(NSRect) rect
{
  self.canvas->drawRect(rect);
}

- (void) mouseDown:(NSEvent *) event
{
  self.canvas->button(true, event);
}

- (void) mouseUp:(NSEvent *) event
{
  self.canvas->button(false, event);
}

- (void) mouseDragged:(NSEvent *) event
{
  self.canvas->mouseMove(event);
}

- (void) rightMouseDown:(NSEvent *) event
{
  self.canvas->button(true, event);
}

- (void) rightMouseUp:(NSEvent *) event
{
  self.canvas->button(false, event);
}

- (void) rightMouseDragged:(NSEvent *) event
{
  self.canvas->mouseMove(event);
}

- (void) otherMouseDown:(NSEvent *) event
{
  self.canvas->button(true, event);
}

- (void) otherMouseUp:(NSEvent *) event
{
  self.canvas->button(false, event);
}

- (void) otherMouseDragged:(NSEvent *) event
{
  self.canvas->mouseMove(event);
}

- (void) mouseMoved:(NSEvent *) event
{
  self.canvas->mouseMove(event);
}

- (void) keyDown:(NSEvent *) event
{
  if (!self.canvas->key(event))
    [super keyDown:event];
}

- (void) scrollWheel:(NSEvent *) event
{
  self.canvas->scrollWheel(event);
}

- (void) dealloc
{
  // TODO: Is this actually called?
  NSLog(@"IpeCanvasView:dealloc");
  delete self.canvas;
}

@end

// --------------------------------------------------------------------
