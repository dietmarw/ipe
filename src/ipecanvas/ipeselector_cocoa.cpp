// -*- objc -*-
// --------------------------------------------------------------------
// PageSelector for Cocoa
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

#include "ipeselector_cocoa.h"

#include "ipecanvas.h"

#include <cairo.h>
#include <cairo-quartz.h>

using namespace ipe;

inline NSString *C2N(const char *s) {return [NSString stringWithUTF8String:s];}

extern CGContextRef ipeGetCGContext();

// --------------------------------------------------------------------

// not thread-safe, but don't know how to do it otherwise...
// currently this is no problem because we run this as a modal dialog.
NSSize thumbnail_size;

@implementation IpeSelectorProvider

- (int) count
{
  if (self.page >= 0)
    return self.doc->page(self.page)->countViews();
  else
    return self.doc->countPages();
}

- (NSString *) title:(int) index
{
  if (self.page >= 0)
    return [NSString stringWithFormat:@"View %d", index + 1];
  String t = self.doc->page(index)->title();
  if (t.empty())
    return [NSString stringWithFormat:@"Page %d", index + 1];
  else
    return [NSString stringWithFormat:@"%s: %d", t.z(), index + 1];
}

- (NSImage *) image:(int) index
{
  if (!self.images) {
    self.images = [NSMutableArray arrayWithCapacity:[self count]];
    for (int i = 0; i < [self count]; ++i)
      [self.images addObject:[NSNull null]];
  }
  if (self.images[index] == [NSNull null]) {
    Buffer b;
    if (self.page >= 0)
      b = self.thumb->render(self.doc->page(self.page), index);
    else {
      Page *p = self.doc->page(index);
      b = self.thumb->render(p, p->countViews() - 1);
    }
    self.images[index] = [self createImage:b];
  }
  return (NSImage *) self.images[index];
}

- (NSImage *) createImage:(Buffer) b
{
  // thumbnail has twice the resolution in case we have a retina
  // display
  int w = self.thumb->width();
  int h = self.thumb->height();
  NSSize size;
  size.width = w / 2.0;
  size.height = h / 2.0;
  return [NSImage imageWithSize:size
			flipped:YES
		 drawingHandler:^(NSRect rect) {
      cairo_surface_t *image =
	cairo_image_surface_create_for_data((uchar *) b.data(),
					    CAIRO_FORMAT_ARGB32,
					    w, h, 4 * w);
      CGContextRef ctx = ipeGetCGContext();
      cairo_surface_t *surface =
	cairo_quartz_surface_create_for_cg_context(ctx, w, h);
      cairo_t *cr = cairo_create(surface);
      cairo_set_source_surface(cr, image, 0.0, 0.0);
      cairo_matrix_t matrix;
      cairo_matrix_init_scale(&matrix, 2.0, 2.0);
      cairo_pattern_set_matrix(cairo_get_source(cr), &matrix);
      cairo_paint(cr);
      cairo_destroy(cr);
      cairo_surface_finish(surface);
      cairo_surface_destroy(surface);
      cairo_surface_destroy(image);
      return YES;
    }];
}

@end

// --------------------------------------------------------------------

@implementation IpeSelectorItem

@end

// --------------------------------------------------------------------

@implementation IpeSelectorView

- (id) initWithFrame:(NSRect) frameRect
{
  NSSize buttonSize = { thumbnail_size.width + 6, thumbnail_size.height + 30 };
  NSSize itemSize = { buttonSize.width + 20, buttonSize.height + 20 };
  const NSPoint buttonOrigin = { 10, 10 };

  self = [super initWithFrame:(NSRect){frameRect.origin, itemSize}];
  if (self) {
    NSButton *b = [[NSButton alloc]
		    initWithFrame:(NSRect){buttonOrigin, buttonSize}];
    b.imagePosition = NSImageAbove;
    b.buttonType = NSMomentaryPushInButton;
    b.action = @selector(ipePageSelected:);
    [self addSubview:b];
    self.button = b;
  }
  return self;
}

- (void) ipeSet:(IpeSelectorItem *) item
{
  self.button.tag = item.index;
  self.button.title = [item.provider title:item.index];
  self.button.image = [item.provider image:item.index];
}

@end

// --------------------------------------------------------------------

@implementation IpeSelectorPrototype

- (void) loadView
{
  self.view = [[IpeSelectorView alloc] initWithFrame:NSZeroRect];
}

- (void) setRepresentedObject:(id) representedObject
{
  [super setRepresentedObject:representedObject];
  [(IpeSelectorView *)[self view] ipeSet:representedObject];
}

@end

// --------------------------------------------------------------------

@interface IpeSelectorDelegate : NSObject <NSWindowDelegate>

- (void) ipePageSelected:(id) sender;

@end

@implementation IpeSelectorDelegate

- (void) ipePageSelected:(id) sender
{
  [NSApp stopModalWithCode:[sender tag]];
}

- (BOOL) windowShouldClose:(id) sender
{
  [NSApp stopModalWithCode:-1];
  return YES;
}

@end

// --------------------------------------------------------------------

int CanvasBase::selectPageOrView(Document *doc, int page, int startIndex,
				 int pageWidth, int width, int height)
{
  // for retina displays, create bitmap with twice the resolution
  Thumbnail thumbs(doc, 2 * pageWidth);

  thumbnail_size.width = thumbs.width() / 2.0;
  thumbnail_size.height = thumbs.height() / 2.0;

  NSPanel *panel = [[NSPanel alloc]
		     initWithContentRect:NSMakeRect(200., 100., width, height)
			       styleMask:NSTitledWindowMask|
		     NSResizableWindowMask|NSClosableWindowMask
				 backing:NSBackingStoreBuffered
				   defer:YES];

  panel.title = C2N((page >= 0) ? "Ipe: Select view" : "Ipe: Select page");

  IpeSelectorDelegate *delegate = [IpeSelectorDelegate new];
  panel.delegate = delegate;

  IpeSelectorProvider *provider = [IpeSelectorProvider new];
  provider.doc = doc;
  provider.thumb = &thumbs;
  provider.page = page;

  NSMutableArray *elements =
    [NSMutableArray arrayWithCapacity:[provider count]];

  for (int i = 0; i < [provider count]; ++i) {
    IpeSelectorItem *item = [IpeSelectorItem new];
    item.index = i;
    item.provider = provider;
    [elements addObject:item];
  }

  NSScrollView *scroll =
    [[NSScrollView alloc] initWithFrame:[panel.contentView frame]];
  scroll.autoresizingMask = NSViewWidthSizable|NSViewHeightSizable;
  scroll.hasVerticalScroller = YES;
  panel.contentView = scroll;

  NSCollectionView *cv = [[NSCollectionView alloc] initWithFrame:NSZeroRect];
  cv.autoresizingMask = NSViewWidthSizable|NSViewHeightSizable;
  scroll.documentView = cv;

  cv.itemPrototype = [IpeSelectorPrototype new];
  cv.content = elements;

  int result = [NSApp runModalForWindow:panel];
  return result;
}

// --------------------------------------------------------------------
