// -*- objc -*-
// pagesorter_cocoa.cpp
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

#include "appui_cocoa.h"
#include "ipethumbs.h"

#include "ipelua.h"

#include "ipeselector_cocoa.h"
#include "ipeuilayout_cocoa.h"

using namespace ipe;

extern NSSize thumbnail_size; // in ipeselector_cocoa.cpp

// --------------------------------------------------------------------

@interface IpePageSorterView : NSBox
@property (assign) NSImageView *image;
@property (assign) NSTextField *ititle;

- (void) ipeSet:(IpeSelectorItem *) item;
@end

@interface IpePageSorterPrototype : NSCollectionViewItem
@end

// --------------------------------------------------------------------

@implementation IpePageSorterView

- (id) initWithFrame:(NSRect) frameRect
{
  NSSize size = { thumbnail_size.width + 16, thumbnail_size.height + 24 };

  self = [super initWithFrame:(NSRect){frameRect.origin, size}];
  if (self) {
    self.titlePosition = NSNoTitle;
    self.boxType = NSBoxCustom;
    self.cornerRadius= 8.0;
    self.borderType = NSLineBorder;
    NSImageView *v =
      [[NSImageView alloc]
	initWithFrame:NSMakeRect(2., 16.,
				 thumbnail_size.width, thumbnail_size.height)];
    [self addSubview:v];
    self.image = v;
    NSTextField *t =
      [[NSTextField alloc]
	initWithFrame:NSMakeRect(2., 0., thumbnail_size.width, 18.)];
    t.editable = NO;
    t.selectable = NO;
    t.bordered= NO;
    t.drawsBackground = NO;
    t.alignment = NSCenterTextAlignment;
    [self addSubview:t];
    self.ititle = t;
  }
  return self;
}

- (void) ipeSet:(IpeSelectorItem *) item
{
  if (!item)
    return;
  self.image.image = [item.provider image:item.index];
  self.ititle.stringValue = [item.provider title:item.index];
}

@end

// --------------------------------------------------------------------

@implementation IpePageSorterPrototype

- (void) loadView
{
  self.view = [[IpePageSorterView alloc] initWithFrame:NSZeroRect];
}

- (void) setRepresentedObject:(id) representedObject
{
  [super setRepresentedObject:representedObject];
  [(IpePageSorterView *)[self view] ipeSet:representedObject];
}

- (void) setSelected:(BOOL) flag
{
  [super setSelected: flag];

  NSBox *view = (NSBox*) [self view];
  if (flag) {
    view.fillColor = [NSColor selectedControlColor];
    view.borderColor = [NSColor blackColor];
  } else {
    view.fillColor = [NSColor controlBackgroundColor];
    view.borderColor = [NSColor controlBackgroundColor];
  }
}

@end

// --------------------------------------------------------------------

@interface IpePageSorterDelegate : NSObject <NSWindowDelegate,
					       NSCollectionViewDelegate>

@property NSMutableArray <IpeSelectorItem *> *pages;
@property (assign) NSCollectionView *cv;

- (void) ipeAccept;
- (void) ipeReject;
- (void) ipeDelete;

@end

@implementation IpePageSorterDelegate

- (void) ipeAccept
{
  [NSApp stopModalWithCode:1];
}

- (void) ipeReject
{
  [NSApp stopModalWithCode:0];
}

- (void) ipeDelete
{
  NSIndexSet *idx = [self.cv selectionIndexes];
  NSMutableArray *els= [self mutableArrayValueForKey:@"pages"];
  [els removeObjectsAtIndexes:idx];
  self.cv.selectionIndexes = [NSIndexSet indexSet];
}

- (BOOL) windowShouldClose:(id) sender
{
  [NSApp stopModalWithCode:0];
  return YES;
}

- (BOOL) collectionView:(NSCollectionView *) cv
    writeItemsAtIndexes:(NSIndexSet *) indexes
	   toPasteboard:(NSPasteboard *)pasteboard
{
  NSData *indexData = [NSKeyedArchiver archivedDataWithRootObject:indexes];
  [pasteboard setData:indexData forType:@"ipePageSelectorDragId"];
  return YES;
}

- (BOOL) collectionView:(NSCollectionView *) cv
  canDragItemsAtIndexes:(NSIndexSet *) indexes
	      withEvent:(NSEvent *) event
{
  return YES;
}

- (BOOL) collectionView:(NSCollectionView *) cv
	     acceptDrop:(id<NSDraggingInfo>) draggingInfo
		  index:(NSInteger) index
	  dropOperation:(NSCollectionViewDropOperation) dropOperation
{
  NSPasteboard *pBoard = [draggingInfo draggingPasteboard];
  NSData *indexData = [pBoard dataForType:@"ipePageSelectorDragId"];
  NSIndexSet *idx = [NSKeyedUnarchiver unarchiveObjectWithData:indexData];
  NSMutableArray *els= [self mutableArrayValueForKey:@"pages"];
  int dest = index - [idx countOfIndexesInRange:NSMakeRange(0, index)];
  NSArray *move = [els objectsAtIndexes:idx];
  [els removeObjectsAtIndexes:idx];
  NSMutableIndexSet *nIdx = [NSMutableIndexSet new];
  for (int i = 0; i < [move count]; ++i)
    [nIdx addIndex:dest++];
  [els insertObjects:move atIndexes:nIdx];
  return YES;
}

- (NSDragOperation) collectionView:(NSCollectionView *) cv
		      validateDrop:(id<NSDraggingInfo>) draggingInfo
		     proposedIndex:(NSInteger *) proposedDropIndex
		     dropOperation:(NSCollectionViewDropOperation *) pdo
{
  return NSDragOperationMove;
}

@end

// --------------------------------------------------------------------

int AppUi::pageSorter(lua_State *L, Document *doc,
		      int width, int height, int thumbWidth)
{
  // double the resolution for retina displays
  Thumbnail thumbs(doc, 2 * thumbWidth);

  thumbnail_size.width = thumbs.width() / 2.0;
  thumbnail_size.height = thumbs.height() / 2.0;

  NSPanel *panel = [[NSPanel alloc]
		     initWithContentRect:NSMakeRect(200., 100., width, height)
			       styleMask:NSTitledWindowMask|
		     NSResizableWindowMask|NSClosableWindowMask
				 backing:NSBackingStoreBuffered
				   defer:YES];

  panel.title = @"Ipe: Page sorter";

  IpePageSorterDelegate *delegate = [IpePageSorterDelegate new];
  panel.delegate = delegate;

  IpeSelectorProvider *provider = [IpeSelectorProvider new];
  provider.doc = doc;
  provider.thumb = &thumbs;
  provider.page = -1;

  delegate.pages = [NSMutableArray arrayWithCapacity:[provider count]];

  for (int i = 0; i < [provider count]; ++i) {
    IpeSelectorItem *item = [IpeSelectorItem new];
    item.index = i;
    item.provider = provider;
    [delegate.pages addObject:item];
  }

  NSScrollView *scroll =
    [[NSScrollView alloc] initWithFrame:[panel.contentView frame]];
  scroll.autoresizingMask = NSViewWidthSizable|NSViewHeightSizable;
  scroll.hasVerticalScroller = YES;

  NSCollectionView *cv = [[NSCollectionView alloc] initWithFrame:NSZeroRect];
  cv.autoresizingMask = NSViewWidthSizable|NSViewHeightSizable;
  scroll.documentView = cv;

  delegate.cv = cv;

  cv.itemPrototype = [IpePageSorterPrototype new];
  // cv.content = delegate.pages;
  cv.selectable = YES;
  cv.allowsMultipleSelection = YES;
  [cv        bind:NSContentBinding
	 toObject:delegate
      withKeyPath:@"pages"
	  options:NULL];
  cv.delegate = delegate;
  [cv registerForDraggedTypes:@[@"ipePageSelectorDragId"]];

  NSButton *bOk = [[NSButton alloc] initWithFrame:NSZeroRect];
  bOk.buttonType = NSMomentaryPushInButton;
  bOk.title = @"Ok";
  bOk.imagePosition = NSNoImage;
  bOk.bezelStyle = NSRoundedBezelStyle;
  bOk.action = @selector(ipeAccept);
  bOk.target = delegate;

  NSButton *bCancel = [[NSButton alloc] initWithFrame:NSZeroRect];
  bCancel.buttonType = NSMomentaryPushInButton;
  bCancel.title = @"Cancel";
  bCancel.imagePosition = NSNoImage;
  bCancel.bezelStyle = NSRoundedBezelStyle;
  bCancel.action = @selector(ipeReject);
  bCancel.target = delegate;

  NSButton *bDelete = [[NSButton alloc] initWithFrame:NSZeroRect];
  bDelete.buttonType = NSMomentaryPushInButton;
  bDelete.title = @"Delete";
  bDelete.imagePosition = NSNoImage;
  bDelete.bezelStyle = NSRoundedBezelStyle;
  bDelete.action = @selector(ipeDelete);
  bDelete.target = delegate;

  NSView *content = [[NSView alloc] initWithFrame:NSZeroRect];
  content.autoresizingMask = NSViewWidthSizable|NSViewHeightSizable;
  panel.contentView = content;

  addToLayout(content, scroll);
  layout(scroll, content, "t=t", 12.0);
  layout(scroll, content, "l=l", 12.0);
  layout(content, scroll, "r=r", 12.0);

  addToLayout(content, bOk);
  addToLayout(content, bCancel);
  addToLayout(content, bDelete);
  layout(content, bOk, "r=r", 12.0);
  layout(bOk, scroll, "t=b", 12.0);
  layout(content, bOk, "b=b", 12.0);
  layout(bOk, bCancel, "l=r", 12.0);
  layout(bCancel, scroll, "t=b", 12.0);
  layout(content, bCancel, "b=b", 12.0);
  layout(bDelete, content, "l=l", 12.0);
  layout(bDelete, scroll, "t=b", 12.0);
  layout(content, bDelete, "b=b", 12.0);
  layout(bCancel, bDelete, "l>r", 12.0);
  layout(bOk, bCancel, "w=w");
  layout(bOk, bDelete, "w=w");

  [panel setDefaultButtonCell:[bOk cell]]; // changed rendering

  int result = [NSApp runModalForWindow:panel];
  if (result) {
    int n = [delegate.pages count];
    lua_createtable(L, n, 0);
    for (int i = 1; i <= n; ++i) {
      lua_pushinteger(L, delegate.pages[i-1].index + 1);
      lua_rawseti(L, -2, i);
    }
    return 1;
  } else
    return 0;
}

// --------------------------------------------------------------------


