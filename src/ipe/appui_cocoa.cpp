// -*- objc -*-
// appui_cocoa.cpp
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
#include "ipecanvas_cocoa.h"
#include "controls_cocoa.h"

#include "ipelua.h"

#include <cstdio>
#include <cstdlib>

using namespace ipe;
using namespace ipelua;

#include "ipeuilayout_cocoa.h"

// in ipeui_cocoa:
extern NSImage *colorIcon(double red, double green, double blue, int pixels);

// --------------------------------------------------------------------

static const char * const snapbutton_action[] = {
  "snapvtx", "snapctl", "snapbd", "snapint",
  "snapgrid", "snapangle", "snapauto",
  "grid_visible" };

@interface IpeAction : NSObject
@property NSString *name;
@property NSString *title;
@property BOOL alwaysOn;
@property BOOL toggles;
// This is the base reference for the state of toggling actions:
@property BOOL state;

+ (instancetype) ipeAction:(NSString *) aName
		     title:(NSString *) aTitle
		  alwaysOn:(BOOL) aAlways
		   toggles:(BOOL) aToggles;
@end

@implementation IpeAction

+ (instancetype) ipeAction:(NSString *) aName
		     title:(NSString *) aTitle
		  alwaysOn:(BOOL) aAlways
		   toggles:(BOOL) aToggles
{
  IpeAction *Self = [[IpeAction alloc] init];
  Self.name = aName;
  Self.title = aTitle;
  Self.alwaysOn = aAlways;
  Self.toggles = aToggles;
  Self.state = NO;
  return Self;
}
@end

// --------------------------------------------------------------------

@implementation IpeMenuItem

- (instancetype) initWithTitle:(NSString *) aTitle
		     ipeAction:(NSString *) anIpeAction
		 keyEquivalent:(NSString *) aKey
{
  self = [super initWithTitle:aTitle
		       action:@selector(ipeMenuAction:)
		keyEquivalent:aKey];
  if (self) {
    _ipeAction = anIpeAction;
    // make sure these operations work in NSTextField and NSTextView
    if ([_ipeAction isEqualToString:@"copy"])
      self.action = @selector(copy:);
    else if ([_ipeAction isEqualToString:@"cut"])
      self.action = @selector(cut:);
    else if ([_ipeAction isEqualToString:@"paste"])
      self.action = @selector(paste:);
    else if ([_ipeAction isEqualToString:@"paste_at_cursor"])
      self.action = @selector(paste:);
    else if ([_ipeAction isEqualToString:@"delete"])
      self.action = @selector(delete:);
    else if ([_ipeAction isEqualToString:@"select_all"])
      self.action = @selector(selectAll:);
    else if ([_ipeAction isEqualToString:@"fullscreen"])
      self.action = @selector(toggleFullScreen:);
    // make sure these work when no window exists
    else if ([@[@"show_configuration", @"new_window", @"open",
		@"manual", @"about_ipelets"]
		 containsObject:anIpeAction])
      self.action = @selector(ipeAlwaysAction:);
  }
  return self;
}

@end

// --------------------------------------------------------------------

static NSImage *colorIcon(Color color, int pixels = 24)
{
  double red = color.iRed.toDouble();
  double green = color.iGreen.toDouble();
  double blue = color.iBlue.toDouble();
  return colorIcon(red, green, blue, pixels);
}

static NSImage *loadIcon(String action)
{
  String dir = ipeIconDirectory();
  String fname = dir + action + ".tiff";
  if (!Platform::fileExists(fname)) {
    fname = dir + action + ".png";
    if (!Platform::fileExists(fname))
      return nil;
  }
  return [[NSImage alloc] initWithContentsOfFile:I2N(fname)];
}

// --------------------------------------------------------------------

@interface IpeWindowDelegate :
  NSObject <NSWindowDelegate, NSToolbarDelegate, IpeControlsDelegate>

- (instancetype) initFor:(AppUi *) ui;
- (void) ipeMenuAction:(id) sender;
- (void) ipeToolbarAction:(id) sender;
- (void) ipeAbsoluteButton:(id) sender;
- (void) ipeSnapButton:(id) sender;
- (void) ipeSelectorChanged:(id) sender;
- (void) toggleSnapbarShown:(id) sender;
- (void) ipeSubmenu:(id) sender;
- (BOOL) ipeIsModified:(id) sender;
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
- (void) ipeMagnificationGesture:(NSGestureRecognizer *) gestureRecognizer;
#pragma clang diagnostic pop
@end

@implementation IpeWindowDelegate {
  AppUi * appui;
}

- (instancetype) initFor:(AppUi *) ui
{
  self = [super init];
  if (self) {
    appui = ui;
  }
  return self;
}

- (void) ipeSubmenu:(id) sender
{
  NSLog(@"ipeSubmenu: %@", sender);
}

- (BOOL) validateMenuItem:(NSMenuItem *) item
{
  if ([item respondsToSelector:@selector(ipeAction)]) {
    NSString *name = [item performSelector:@selector(ipeAction)];
    return appui->validateMenuItem(item, name);
  }
  if (item.action == @selector(toggleSnapbarShown:))
    return appui->validateMenuItem(item, @"snapbar");
  if (item.tag >= AppUi::ESubmenuGridSize && item.tag < AppUi::ESubmenuFin)
    appui->fillDynamicSubmenu(item);
  return YES;
}

- (BOOL) ipeIsModified:(id) sender
{
  return appui->isModified();
}

- (BOOL) windowShouldClose:(id) sender
{
  return appui->closeRequested();
}

// --------------------------------------------------------------------

// These are needed to enable them in other first responders
// like NSTextField and NSTextView
// Unfortunately undo and redo cannot be handled this way,
// as Cocoa would pick up the actions "undo" and "redo" and
// reroute them to the applications NSUndoManager.

- (void) copy:(id) sender
{
  [self ipeMenuAction:sender];
}

- (void) cut:(id) sender
{
  [self ipeMenuAction:sender];
}

- (void) paste:(id) sender
{
  [self ipeMenuAction:sender];
}

- (void) delete:(id) sender
{
  [self ipeMenuAction:sender];
}

- (void) selectAll:(id) sender
{
  [self ipeMenuAction:sender];
}

// --------------------------------------------------------------------

- (void) indicatorFired:(NSTimer *) timer
{
  appui->explain("", 0);
}

- (void) ipeMenuAction:(id) sender
{
  appui->action(N2I([sender ipeAction]));
}

- (void) ipeToolbarAction:(id) sender
{
  appui->action(N2I([sender itemIdentifier]));
}

- (void) ipeAbsoluteButton:(id) sender
{
  appui->absoluteButton([sender tag]);
}

- (void) ipeSnapButton:(id) sender
{
  appui->snapButton([sender tag]);
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
- (void) ipeMagnificationGesture:(NSGestureRecognizer *) gestureRecognizer
{
  NSMagnificationGestureRecognizer *p = (NSMagnificationGestureRecognizer *)
    gestureRecognizer;
  NSPoint q = [p locationInView:p.view];
  NSLog(@"magnify! %g %g %ld %g" , q.x, q.y, p.state, 1.0 + p.magnification);
}
#pragma clang diagnostic pop

- (void) ipeSelectorChanged:(id) sender
{
  appui->selectorChanged([sender tag]);
}

- (void) toggleSnapbarShown:(id) sender
{
  appui->toggleSnapbarShown();
}

// --------------------------------------------------------------------

- (void) pathViewAttributeChanged:(String) attr
{
  appui->action(attr);
}

- (void) pathViewPopup:(NSPoint) p
{
  appui->luaShowPathStylePopup(Vector(p.x, p.y));
}

- (void) bookmarkSelected:(int) index
{
  appui->luaBookmarkSelected(index);
}

- (void) layerMenuAt:(NSPoint) p forLayer:(NSString *) layer;
{
  appui->layerMenu(p, layer);
}

- (void) layerAction:(NSString *) actionName forLayer:(NSString *) layer
{
  appui->layerAction(actionName, layer);
}

// --------------------------------------------------------------------

- (void) windowDidEndLiveResize:(NSNotification *) notification
{
  appui->canvas()->update();
}

// --------------------------------------------------------------------

- (NSToolbarItem *) toolbar:(NSToolbar *) toolbar
      itemForItemIdentifier:(NSString *) itemIdentifier
  willBeInsertedIntoToolbar:(BOOL) flag
{
  NSToolbarItem *t =
    [[NSToolbarItem alloc] initWithItemIdentifier:itemIdentifier];
  t.image = loadIcon(N2I(itemIdentifier));
  if (![itemIdentifier hasPrefix:@"mode_"])
    t.visibilityPriority = NSToolbarItemVisibilityPriorityLow;
  t.action = @selector(ipeToolbarAction:);
  IpeAction *s = appui->findAction(itemIdentifier);
  if (s)
    t.toolTip = s.title;
  return t;
}

- (NSArray<NSString *> *) toolbarAllowedItemIdentifiers:(NSToolbar *) toolbar
{
  return @[ @"copy", @"cut", @"paste", @"delete", @"undo", @"redo",
	     @"zoom_in", @"zoom_out", @"fit_objects", @"fit_page",
	     @"fit_width", @"keyboard",
	     @"mode_select", @"mode_translate", @"mode_rotate",
	     @"mode_stretch", @"mode_shear", @"mode_pan", @"mode_shredder",
	     @"mode_label", @"mode_math", @"mode_paragraph",
	     @"mode_marks", @"mode_rectangles1", @"mode_rectangles2",
	     @"mode_rectangles3", @"mode_parallelogram", @"mode_lines",
	     @"mode_polygons", @"mode_splines", @"mode_splinegons",
	     @"mode_arc1", @"mode_arc2", @"mode_arc3",
	     @"mode_circle1", @"mode_circle2", @"mode_circle3",
	     @"mode_ink",
	     NSToolbarSpaceItemIdentifier,
	     NSToolbarFlexibleSpaceItemIdentifier,
	    ];
}

- (NSArray<NSString *> *) toolbarDefaultItemIdentifiers:(NSToolbar *) toolbar
{
  return @[ @"mode_select", @"mode_translate", @"mode_rotate",
	     @"mode_stretch", @"mode_shear", @"mode_pan", @"mode_shredder",
	     @"mode_label", @"mode_math", @"mode_paragraph",
	     @"mode_marks", @"mode_rectangles1", @"mode_rectangles2",
	     @"mode_rectangles3", @"mode_parallelogram", @"mode_lines",
	     @"mode_polygons", @"mode_splines", @"mode_splinegons",
	     @"mode_arc1", @"mode_arc2", @"mode_arc3",
	     @"mode_circle1", @"mode_circle2", @"mode_circle3",
	     @"mode_ink",
	   ];
}

- (NSArray *) toolbarSelectableItemIdentifiers: (NSToolbar *) toolbar
{
  return @[ @"mode_select", @"mode_translate", @"mode_rotate",
	     @"mode_stretch", @"mode_pan", @"mode_shredder",
	     @"mode_label", @"mode_math", @"mode_paragraph",
	     @"mode_marks", @"mode_rectangles1", @"mode_rectangles2",
	     @"mode_rectangles2", @"mode_rectangles3",
	     @"mode_parallelogram", @"mode_lines",
	     @"mode_polygons", @"mode_splines", @"mode_splinegons",
	     @"mode_arc1", @"mode_arc2", @"mode_arc3",
	     @"mode_circle1", @"mode_circle2", @"mode_circle3",
	     @"mode_ink" ];
}
@end

// --------------------------------------------------------------------

static bool build_menus = true;

void AppUi::addRootMenu(int id, const char *name)
{
  // menus are already in mainmenu.xib
}

void AppUi::addItem(NSMenu *menu, const char *title, const char *name)
{
  if (title == 0) {
    if (build_menus)
      [menu addItem:[NSMenuItem separatorItem]];
  } else {
    bool canUseWhileDrawing = false;
    bool toggles = false;
    if (name[0] == '@') {
      canUseWhileDrawing = true;
      name = name + 1;
    }
    if (name[0] == '*') {
      toggles = true;
      name += 1;
    }

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

    NSString *nsName = C2N(name);
    IpeAction *s = [IpeAction ipeAction:nsName
				  title:I2N(tooltip)
			       alwaysOn:canUseWhileDrawing
				toggles:toggles];
    iActions[nsName] = s;
    if (!build_menus)
      return;

    uint mask = 0;
    if (sc.hasPrefix("Control+")) {
      sc = sc.substr(8);
      mask |= NSControlKeyMask;
    }
    if (sc.hasPrefix("Ctrl+")) {
      sc = sc.substr(5);
      mask |= NSCommandKeyMask;
    }
    if (sc.hasPrefix("Command+")) {
      sc = sc.substr(8);
      mask |= NSCommandKeyMask;
    }
    if (sc.hasPrefix("Shift+")) {
      sc = sc.substr(6);
      mask |= NSShiftKeyMask;
    }
    if (sc.hasPrefix("Alt+")) {
      sc = sc.substr(4);
      mask |= NSAlternateKeyMask;
    }

    unichar code = 0;
    if (sc.size() == 1) {
      if ('A' <= sc[0] && sc[0] <= 'Z')
	code = sc[0] + 0x20;
      else
	code = sc[0];
    } else if (!sc.empty()) {
      if (sc[0] == 'F')
	code = 0xf703 + Lex(sc.substr(1)).getInt();
      else if (sc == "backspace")
	code = 8;
      else if (sc == "delete")
	code = 127;
      else if (sc == "Up")
	code = NSUpArrowFunctionKey;
      else if (sc == "Down")
	code = NSDownArrowFunctionKey;
      else if (sc == "Left")
	code = NSLeftArrowFunctionKey;
      else if (sc == "Right")
	code = NSRightArrowFunctionKey;
      else if (sc == "End")
	code = NSEndFunctionKey;
      else if (sc == "Home")
	code = NSHomeFunctionKey;
      else if (sc == "PgUp")
	code = NSPageUpFunctionKey;
      else if (sc == "PgDown")
	code = NSPageDownFunctionKey;
    }

    NSString *keyEq = @"";
    if (code)
      keyEq = [NSString stringWithFormat:@"%C", code];

    NSMenuItem *item = [[IpeMenuItem alloc]
			 initWithTitle:ipeui_set_mnemonic(I2N(title), nil)
			     ipeAction:C2N(name)
			 keyEquivalent:keyEq];
    [item setKeyEquivalentModifierMask:mask];
    [menu addItem:item];
  }
}

void AppUi::addItem(int id, const char *title, const char *name)
{
  NSMenu *menu = [[[NSApp mainMenu] itemAtIndex:(id + 1)] submenu];
  addItem(menu, title, name);
}

static NSMenu * __unsafe_unretained current_submenu = 0;

void AppUi::startSubMenu(int id, const char *name, int tag)
{
  if (!build_menus)
    return;

  NSMenu *menu = [[[NSApp mainMenu] itemAtIndex:(id + 1)] submenu];

  NSString *title = ipeui_set_mnemonic(C2N(name), nil);
  NSMenuItem *item = [[NSMenuItem alloc] initWithTitle:title
						action:NULL
					 keyEquivalent:@""];
  NSMenu *submenu = [[NSMenu alloc] initWithTitle:title];
  item.submenu = submenu;
  if (tag) {
    item.tag = tag;
    item.action = @selector(ipeSubmenu:);
  }
  [menu addItem:item];
  current_submenu = submenu;
}

void AppUi::addSubItem(const char *title, const char *name)
{
  addItem(current_submenu, title, name);
}

MENUHANDLE AppUi::endSubMenu()
{
  return current_submenu;
}

// --------------------------------------------------------------------

AppUi::AppUi(lua_State *L0, int model)
 : AppUiBase(L0, model)
{
  NSRect e = [[NSScreen mainScreen] frame];
  auto h = e.size.height;
  auto w = e.size.width;

  NSRect contentRect = NSMakeRect(0.125 * w, 0.125 * h, 0.75 * w, 0.75 * h);
  NSRect subRect = NSMakeRect(0., 0., 100., 100.);

  iIndicatorTimer = nil;
  iActionsEnabled = true;

  iWindow = [[NSWindow alloc]
	      initWithContentRect:contentRect
	                styleMask:NSTitledWindowMask|NSClosableWindowMask|
	      NSResizableWindowMask|NSMiniaturizableWindowMask
	                  backing:NSBackingStoreBuffered
	                    defer:YES];

  iView = [[IpeCanvasView alloc] initWithFrame:subRect];
  iCanvas = iView.canvas;
  iCanvas->setObserver(this);

  iDelegate = [[IpeWindowDelegate alloc] initFor:this];
  [iWindow setDelegate:iDelegate];
  [iWindow setAcceptsMouseMovedEvents:YES];

#if 0
  if ([iView respondsToSelector:@selector(addGestureRecognizer:)]) {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    NSGestureRecognizer *p = [[NSMagnificationGestureRecognizer alloc] init];
    p.target = iDelegate;
    p.action = @selector(ipeMagnificationGesture:);
    [iView addGestureRecognizer:p];
#pragma clang diagnostic pop
  }
#endif

  iActions = [NSMutableDictionary dictionaryWithCapacity:100];

  buildMenus();
  build_menus = false; // all Windows share the same main menu

  NSToolbar *tb = [[NSToolbar alloc] initWithIdentifier:@"Tools"];
  [tb setDelegate:iDelegate];
  [tb setDisplayMode:NSToolbarDisplayModeIconOnly];
  [tb setAllowsUserCustomization:YES];
  [tb setAutosavesConfiguration:YES];
  [tb setSizeMode:NSToolbarSizeModeSmall];
  [iWindow setToolbar:tb];

  iContent = [[NSView alloc] initWithFrame:contentRect];

  makePropertiesTool();

  iLayerBox = [[NSBox alloc] initWithFrame:subRect];
  iLayerBox.title = @"Layers";

  NSRect layerFrame = [iLayerBox.contentView frame];
  iLayerView = [[IpeLayerView alloc] initWithFrame:layerFrame];
  iLayerView.delegate = iDelegate;
  iLayerView.toolTip = @"Layers of this page";
  iLayerBox.contentView = iLayerView;

  iStatus = [[NSTextField alloc] initWithFrame:NSZeroRect];
  iStatus.editable = NO;
  iStatus.selectable = NO;
  iStatus.drawsBackground = NO;

  iSnapIndicator = [[NSTextField alloc] initWithFrame:NSZeroRect];
  iSnapIndicator.editable = NO;
  iSnapIndicator.selectable = NO;
  iSnapIndicator.drawsBackground = NO;
  iSnapIndicator.font = [NSFont userFixedPitchFontOfSize:11.0];

  iMouseIndicator = [[NSTextField alloc] initWithFrame:NSZeroRect];
  iMouseIndicator.editable = NO;
  iMouseIndicator.selectable = NO;
  iMouseIndicator.drawsBackground = NO;
  iMouseIndicator.font = [NSFont userFixedPitchFontOfSize:11.0];

  iZoomIndicator = [[NSTextField alloc] initWithFrame:NSZeroRect];
  iZoomIndicator.editable = NO;
  iZoomIndicator.selectable = NO;
  iZoomIndicator.drawsBackground = NO;

  makeSnapBar();

  addToLayout(iContent, iView);
  addToLayout(iContent, iPropertiesBox);
  addToLayout(iContent, iLayerBox);
  addToLayout(iContent, iSnapBar);
  addToLayout(iContent, iStatus);
  addToLayout(iContent, iSnapIndicator);
  addToLayout(iContent, iMouseIndicator);
  addToLayout(iContent, iZoomIndicator);

  layout(iSnapBar, iContent, "t=t");
  layout(iSnapBar, iContent, "r=r");
  layout(iSnapBar, iView, "l=l");
  layout(iPropertiesBox, iContent, "l=l");
  layout(iPropertiesBox, iContent, "t=t");
  layout(iPropertiesBox, iView, "r=l");
  layout(iView, iContent, "r=r");
  layout(iView, iStatus, "b=t");
  layout(iLayerBox, iContent, "l=l");
  layout(iLayerBox, iView, "r=l");
  layout(iLayerBox, iPropertiesBox, "t=b");
  layout(iLayerBox, iStatus, "b=t");
  layout(iStatus, iContent, "l=l");
  layout(iStatus, iContent, "b=b");
  layout(iStatus, iSnapIndicator, "r=l");
  layout(iSnapIndicator, iMouseIndicator, "r=l");
  layout(iMouseIndicator, iZoomIndicator, "r=l");
  layout(iZoomIndicator, iContent, "r=r");

  iViewToSnapBar = layout(iView, iSnapBar, "t=b", 0.0, 1.0, NO);
  iViewToTop = layout(iView, iContent, "t=t", 0.0, 1.0, NO);
  activateConstraint(iViewToSnapBar, YES);

  [iStatus
    setContentHuggingPriority:NSLayoutPriorityDefaultLow
	       forOrientation:NSLayoutConstraintOrientationHorizontal];
  [iSnapIndicator
    setContentHuggingPriority:NSLayoutPriorityDefaultHigh
	       forOrientation:NSLayoutConstraintOrientationHorizontal];
  [iMouseIndicator
    setContentHuggingPriority:NSLayoutPriorityDefaultHigh
	       forOrientation:NSLayoutConstraintOrientationHorizontal];
  [iZoomIndicator
    setContentHuggingPriority:NSLayoutPriorityDefaultHigh
	       forOrientation:NSLayoutConstraintOrientationHorizontal];

  [iWindow setContentView:iContent];

  setCheckMark("coordinates|", "points");
  setCheckMark("scaling|", "1");
}

AppUi::~AppUi()
{
  NSLog(@"~AppUi");
}

// --------------------------------------------------------------------

void AppUi::makePropertiesTool()
{
  iPropertiesBox = [[NSBox alloc] initWithFrame:NSMakeRect(0.,0.,50.,300.)];
  [iPropertiesBox setTitle:@"Properties"];

  for (int i = 0; i <= EUiSymbolSize; ++i) {
    iButton[i] = [[NSButton alloc] initWithFrame:NSMakeRect(0., 0., 20., 20.)];
    [iButton[i] setButtonType:NSMomentaryPushInButton];
    [iButton[i] setImagePosition:NSImageOnly];
    [iButton[i] setBezelStyle:NSRegularSquareBezelStyle];
    iButton[i].action = @selector(ipeAbsoluteButton:);
    iButton[i].tag = i;
    addToLayout(iPropertiesBox, iButton[i]);

    iSelector[i] = [[NSPopUpButton alloc]
		     initWithFrame:NSMakeRect(0., 0., 50., 20.)
			 pullsDown:NO];
    iSelector[i].target = iDelegate;
    iSelector[i].action = @selector(ipeSelectorChanged:);
    iSelector[i].tag = i;
    addToLayout(iPropertiesBox, iSelector[i]);
  }
  iButton[EUiMarkShape].enabled = NO;
  // [iButton[EUiMarkShape] setImageDimsWhenDisabled:NO];

  iModeIndicator = [[NSImageView alloc] initWithFrame:NSMakeRect(0, 0, 20, 20)];
  iModeIndicator.editable = NO;
  iModeIndicator.image = loadIcon("mode_select");
  addToLayout(iPropertiesBox, iModeIndicator);

  [iButton[EUiStroke] setImage:colorIcon(Color(1000, 0, 0))];
  [iButton[EUiFill] setImage:colorIcon(Color(1000, 1000, 0))];
  [iButton[EUiPen] setImage:loadIcon("pen")];
  [iButton[EUiTextSize] setImage:loadIcon("mode_label")];
  [iButton[EUiMarkShape] setImage:loadIcon("mode_marks")];
  [iButton[EUiSymbolSize] setImage:loadIcon("mode_marks")];

  [iButton[EUiStroke] setToolTip:@"Absolute stroke color"];
  [iButton[EUiFill] setToolTip:@"Absolute fill color"];
  [iButton[EUiPen] setToolTip:@"Absolute pen width"];
  [iButton[EUiTextSize] setToolTip:@"Absolute text size"];
  [iButton[EUiSymbolSize] setToolTip:@"Absolute symbol size"];

  [iSelector[EUiStroke] setToolTip:@"Symbolic stroke color"];
  [iSelector[EUiFill] setToolTip:@"Symbolic fill color"];
  [iSelector[EUiPen] setToolTip:@"Symbolic pen width"];
  [iSelector[EUiTextSize] setToolTip:@"Symbolic text size"];
  [iSelector[EUiMarkShape] setToolTip:@"Mark shape"];
  [iSelector[EUiSymbolSize] setToolTip:@"Symbolic symbol size"];

  iViewNumber = [[NSButton alloc] initWithFrame:NSMakeRect(0, 0, 10, 20)];
  [iViewNumber setButtonType:NSMomentaryPushInButton];
  [iViewNumber setTitle:@""];
  [iViewNumber setToolTip:@"Current view number"];
  [iViewNumber setImagePosition:NSNoImage];
  [iViewNumber setBezelStyle:NSRoundedBezelStyle];
  [iViewNumber setAction:@selector(ipeAbsoluteButton:)];
  [iViewNumber setTag:EUiView];
  addToLayout(iPropertiesBox, iViewNumber);

  iPageNumber = [[NSButton alloc] initWithFrame:NSMakeRect(0, 0, 10, 20)];
  [iPageNumber setButtonType:NSMomentaryPushInButton];
  [iPageNumber setTitle:@""];
  [iPageNumber setToolTip:@"Current page number"];
  [iPageNumber setImagePosition:NSNoImage];
  [iPageNumber setBezelStyle:NSRoundedBezelStyle];
  [iPageNumber setAction:@selector(ipeAbsoluteButton:)];
  [iPageNumber setTag:EUiPage];
  addToLayout(iPropertiesBox, iPageNumber);

  iPathView = [[IpePathView alloc] initWithFrame:NSMakeRect(0, 0, 150, 30)];
  iPathView.delegate = iDelegate;
  iPathView.toolTip =
    @"Toggle arrows, toggle fill mode, right-click for path style";
  addToLayout(iPropertiesBox, iPathView);

  iViewMarked = [[NSButton alloc] initWithFrame:NSMakeRect(0, 0, 10, 10)];
  [iViewMarked setButtonType:NSSwitchButton];
  [iViewMarked setTitle:@"Mark view"];
  [iViewMarked setToolTip:@"Current view marked"];
  [iViewMarked setAction:@selector(ipeAbsoluteButton:)];
  [iViewMarked setTag:EUiViewMarked];
  iViewMarked.font = [NSFont labelFontOfSize:9.0];
  addToLayout(iPropertiesBox, iViewMarked);

  iPageMarked = [[NSButton alloc] initWithFrame:NSMakeRect(0, 0, 10, 10)];
  [iPageMarked setButtonType:NSSwitchButton];
  [iPageMarked setTitle:@"Mark page"];
  [iPageMarked setToolTip:@"Current page marked"];
  [iPageMarked setAction:@selector(ipeAbsoluteButton:)];
  [iPageMarked setTag:EUiPageMarked];
  iPageMarked.font = [NSFont labelFontOfSize:9.0];
  addToLayout(iPropertiesBox, iPageMarked);

  NSView *inside = [iPropertiesBox contentView];
  id guide = layoutGuide(iPropertiesBox);
  layout(iButton[EUiStroke], inside, "t=t", 2.0);
  for (int i = 0; i <= EUiSymbolSize; ++i) {
    layout(iButton[i], iButton[i], "w=h");
    layout(iButton[i], inside, "l=l");
    layout(iButton[i], guide, "r=l");
    layout(guide, iSelector[i], "r=l");
    layout(iSelector[i], inside, "r=r");
    layout(iSelector[i], iButton[i], "y=y");
    if (i != EUiStroke && i != EUiTextSize)
      layout(iButton[i], iButton[i-1], "t=b", 2.0);
  }
  layout(guide, inside, "t=t");
  layout(guide, inside, "b=b");
  layout(iPathView, iButton[EUiPen], "t=b", 2.0);
  layout(iButton[EUiTextSize], iPathView, "t=b", 2.0);
  layout(iPathView, guide, "l=r");
  layout(iPathView, inside, "r=r");
  layout(iModeIndicator, iPathView, "t=t");
  layout(iModeIndicator, iPathView, "b=b");
  layout(iModeIndicator, inside, "l=l");
  layout(iModeIndicator, guide, "r=l");
  layout(iViewNumber, iButton[EUiSymbolSize], "t=b", 2.0);
  layout(iViewNumber, inside, "l=l");
  layout(iPageNumber, iViewNumber, "l=r", 2.0);
  layout(iPageNumber, iViewNumber, "t=t");
  layout(iPageNumber, inside, "r=r");
  layout(iViewMarked, iViewNumber, "t=b", 2.0);
  layout(iViewMarked, inside, "l=l");
  layout(iPageMarked, iViewMarked, "t=t");
  layout(iPageMarked, iViewMarked, "l>r", 2.0);
  layout(iPageMarked, inside, "r=r");
  layout(inside, iViewMarked, "b=b", 5.0);

  layout(guide, nil, "w=0", 5.0);
  layout(iPathView, iPathView, "h=w", 0.0, 0.3);
  layout(iViewNumber, iPageNumber, "w=w");
  layout(iModeIndicator, iModeIndicator, "w=h");
}

void AppUi::makeSnapBar()
{
  const auto MARGIN = 3.0;
  const auto PAD = 2.0;

  iSnapBar = [[NSView alloc] initWithFrame:NSMakeRect(0., 0., 600., 32.)];

  for (int i = 0; i < 8; ++i) {
    iSnapButton[i] = [[NSButton alloc] initWithFrame:NSMakeRect(0, 0, 32, 32)];
    iSnapButton[i].buttonType = NSPushOnPushOffButton;
    iSnapButton[i].imagePosition = NSImageOnly;
    iSnapButton[i].image = loadIcon(snapbutton_action[i]);
    iSnapButton[i].bezelStyle = NSRegularSquareBezelStyle;
    iSnapButton[i].action = @selector(ipeSnapButton:);
    iSnapButton[i].tag = i;
    IpeAction *s = findAction(C2N(snapbutton_action[i]));
    if (s)
      iSnapButton[i].toolTip = s.title;
    addToLayout(iSnapBar, iSnapButton[i]);
  }

  iSelector[EUiGridSize] =
    [[NSPopUpButton alloc] initWithFrame:NSMakeRect(0, 0, 100, 40)
			       pullsDown:NO];
  iSelector[EUiGridSize].toolTip = @"Grid size";
  iSelector[EUiGridSize].target = iDelegate;
  iSelector[EUiGridSize].action = @selector(ipeSelectorChanged:);
  iSelector[EUiGridSize].tag = EUiGridSize;
  addToLayout(iSnapBar, iSelector[EUiGridSize]);

  iSelector[EUiAngleSize] =
    [[NSPopUpButton alloc] initWithFrame:NSMakeRect(0, 0, 100, 40)
			       pullsDown:NO];
  iSelector[EUiAngleSize].toolTip = @"Angle for angular snap";
  iSelector[EUiAngleSize].target = iDelegate;
  iSelector[EUiAngleSize].action = @selector(ipeSelectorChanged:);
  iSelector[EUiAngleSize].tag = EUiAngleSize;
  addToLayout(iSnapBar, iSelector[EUiAngleSize]);

  for (int i = 0; i < 8; ++i) {
    layout(iSnapButton[i], iSnapBar, "t=t", MARGIN);
    layout(iSnapBar, iSnapButton[i], "b=b", MARGIN);
    if (0 < i && i < 5)
      layout(iSnapButton[i], iSnapButton[i-1], "l=r", PAD);
  }
  layout(iSelector[EUiGridSize], iSnapBar, "t=t", MARGIN);
  layout(iSelector[EUiAngleSize], iSnapBar, "t=t", MARGIN);
  layout(iSnapBar, iSelector[EUiGridSize], "b>b", MARGIN);
  layout(iSnapBar, iSelector[EUiAngleSize], "b>b", MARGIN);
  layout(iSelector[EUiGridSize], nil, "w>0", 160);
  layout(iSelector[EUiAngleSize], nil, "w>0", 100);

  layout(iSnapButton[0], iSnapBar, "l=l", MARGIN);
  layout(iSelector[EUiGridSize], iSnapButton[4], "l=r", PAD);
  layout(iSnapButton[5], iSelector[EUiGridSize], "l=r", PAD);
  layout(iSelector[EUiAngleSize], iSnapButton[5], "l=r", PAD);
  layout(iSnapButton[6], iSelector[EUiAngleSize], "l=r", PAD);
  layout(iSnapButton[7], iSnapButton[6], "l>r", PAD);
  layout(iSnapBar, iSnapButton[7], "r=r", MARGIN);
}

// --------------------------------------------------------------------

void AppUi::resetCombos()
{
  iInUiUpdate = true;
  for (int i = 0; i < EUiView; ++i)
    [iSelector[i] removeAllItems];
  iInUiUpdate = false;
}

void AppUi::addComboColors(AttributeSeq &sym, AttributeSeq &abs)
{
  iInUiUpdate = true;
  for (uint i = 0; i < sym.size(); ++i) {
    Color color = abs[i].color();
    NSImage *im = colorIcon(color, 12);
    String s = sym[i].string();
    [iSelector[EUiStroke] addItemWithTitle:I2N(s)];
    [iSelector[EUiFill] addItemWithTitle:I2N(s)];
    [iSelector[EUiStroke] lastItem].image = im;
    [iSelector[EUiFill] lastItem].image = im;
    iComboContents[EUiStroke].push_back(s);
    iComboContents[EUiFill].push_back(s);
  }
  iInUiUpdate = false;
}

void AppUi::addCombo(int sel, String s)
{
  iInUiUpdate = true;
  [iSelector[sel] addItemWithTitle:I2N(s)];
  iInUiUpdate = false;
}

void AppUi::setComboCurrent(int sel, int idx)
{
  iInUiUpdate = true;
  [iSelector[sel] selectItemAtIndex:idx];
  iInUiUpdate = false;
}

void AppUi::setButtonColor(int sel, Color color)
{
  [iButton[sel] setImage:colorIcon(color)];
}

void AppUi::setPathView(const AllAttributes &all, Cascade *sheet)
{
  [iPathView setAttributes:&all cascade:sheet];
}

void AppUi::setCheckMark(String name, Attribute a)
{
  setCheckMark(name + "|", a.string());
}

void AppUi::setCheckMark(String name, String value)
{
  // deselect all from category
  NSString *prefix = I2N(name);
  for (NSString *action in iActions) {
    if ([action hasPrefix:prefix])
      iActions[action].state = NO;
  }
  iActions[I2N(name + value)].state = YES;
}

void AppUi::setLayers(const Page *page, int view)
{
  iLayerNames.clear();
  for (int i = 0; i < page->countLayers(); ++i)
    iLayerNames.push_back(page->layer(i));

  [iLayerView setPage:page view:view];
}

void AppUi::setActionsEnabled(bool mode)
{
  iActionsEnabled = mode;
}

void AppUi::setNumbers(String vno, bool vm, String pno, bool pm)
{
  iViewNumber.title = I2N(vno);
  iPageNumber.title = I2N(pno);
  iViewMarked.state = vm ? NSOnState : NSOffState;
  iPageMarked.state = pm ? NSOnState : NSOffState;
  iViewNumber.enabled = !vno.empty();
  iViewMarked.enabled = !vno.empty();
  iPageNumber.enabled = !pno.empty();
  iPageMarked.enabled = !pno.empty();
}

BOOL AppUi::validateMenuItem(NSMenuItem *item, NSString *name)
{
  if ([name isEqualToString:@"snapbar"]) {
    item.title = [iSnapBar isHidden] ? @"Show Snap Toolbar" :
      @"Hide Snap Toolbar";
    return YES;
  }
  IpeAction *s = findAction(name);
  if (s) {
    if ([name isEqualToString:@"toggle_notes"])
      s.state = (iNotesPanel && [iNotesPanel isVisible]);
    else if ([name isEqualToString:@"toggle_bookmarks"])
      s.state = (iBookmarksPanel && [iBookmarksPanel isVisible]);
    [item setState:(s.state ? NSOnState : NSOffState)];
    return actionsEnabled() || s.alwaysOn;
  }
  return YES;
}

void AppUi::fillDynamicSubmenu(NSMenuItem *item)
{
  NSMenu *sm = item.submenu;
  [sm removeAllItems];
  if (item.tag == ESubmenuSelectLayer || item.tag == ESubmenuMoveLayer) {
    String cmd = (item.tag == ESubmenuSelectLayer) ?
      "selectinlayer-" : "movetolayer-";
    for (auto &name : iLayerNames) {
      NSMenuItem *mi = [[IpeMenuItem alloc]
			   initWithTitle:I2N(name)
			       ipeAction:I2N(cmd + name)
			   keyEquivalent:@""];
      [sm addItem:mi];
    }
  } else if (item.tag == ESubmenuTextStyle) {
    AttributeSeq seq;
    iCascade->allNames(ETextStyle, seq);
    String cmd { "textstyle|" };
    for (auto &attr : seq) {
      NSMenuItem *mi = [[IpeMenuItem alloc]
			   initWithTitle:I2N(attr.string())
			       ipeAction:I2N(cmd + attr.string())
			   keyEquivalent:@""];
      if (attr == iAll.iTextStyle)
	mi.state = NSOnState;
      [sm addItem:mi];
    }
  } else if (item.tag == ESubmenuGridSize || item.tag == ESubmenuAngleSize) {
    auto uisel = EUiGridSize;
    auto sel = EGridSize;
    String cmd = "gridsize|";
    if (item.tag == ESubmenuAngleSize) {
      uisel = EUiAngleSize;
      sel = EAngleSize;
      cmd = "anglesize|";
    }
    auto curr = iSelector[uisel].indexOfSelectedItem;
    auto count = 0;
    for (auto &name : iComboContents[uisel]) {
      NSMenuItem *mi = [[IpeMenuItem alloc]
			   initWithTitle:I2N(name)
			       ipeAction:I2N(cmd + name)
			   keyEquivalent:@""];
      if (count == curr)
	mi.state = NSOnState;
      ++count;
      [sm addItem:mi];
    }
  }
}

void AppUi::toggleSnapbarShown()
{
  iSnapBar.hidden = ![iSnapBar isHidden];
  if ([iSnapBar isHidden]) {
    activateConstraint(iViewToSnapBar, NO);
    activateConstraint(iViewToTop, YES);
  } else {
    activateConstraint(iViewToTop, NO);
    activateConstraint(iViewToSnapBar, YES);
  }
  iCanvas->update();
}

// --------------------------------------------------------------------

void AppUi::setNotes(String notes)
{
  if (iNotesField) {
    NSTextStorage *s = [iNotesField textStorage];
    NSAttributedString *n =
      [[NSAttributedString alloc] initWithString:I2N(notes)];
    [s setAttributedString:n];
  }
}

void AppUi::setBookmarks(int no, const String *s)
{
  [iBookmarksView setBookmarks:no fromStrings:s];
}

void AppUi::setToolVisible(int m, bool vis)
{
  if (m == 1) {
    if (vis && iBookmarksPanel == nil) {
      iBookmarksPanel = [[NSPanel alloc]
	  initWithContentRect:NSMakeRect(400.,800.,240.,480.)
		    styleMask:NSTitledWindowMask|NSClosableWindowMask|
	  NSResizableWindowMask|NSMiniaturizableWindowMask
		      backing:NSBackingStoreBuffered
			defer:YES];
      NSRect cFrame =[[iBookmarksPanel contentView] frame];
      iBookmarksView = [[IpeBookmarksView alloc] initWithFrame:cFrame];
      [iBookmarksView setDelegate:iDelegate];
      [iBookmarksPanel setContentView:iBookmarksView];
      [iBookmarksPanel setTitle:@"Ipe bookmarks"];
    }
    if (vis)
      [iBookmarksPanel orderFront:iWindow];
    else
      [iBookmarksPanel orderOut:iWindow];
  } else if (m == 2) {
    if (vis && iNotesPanel == nil) {
      iNotesPanel = [[NSPanel alloc]
	      initWithContentRect:NSMakeRect(400.,800.,240.,480.)
			styleMask:NSTitledWindowMask|NSClosableWindowMask|
		      NSResizableWindowMask|NSMiniaturizableWindowMask
			  backing:NSBackingStoreBuffered
			    defer:YES];
      NSRect cFrame =[[iNotesPanel contentView] frame];
      NSScrollView *scroll = [[NSScrollView alloc] initWithFrame:cFrame];
      iNotesField = [[NSTextView alloc] initWithFrame:cFrame];
      [iNotesField setEditable:NO];
      [iNotesField
	setAutoresizingMask:NSViewWidthSizable|NSViewHeightSizable];
      [scroll setDocumentView:iNotesField];
      [scroll setAutoresizingMask:NSViewWidthSizable|NSViewHeightSizable];
      [scroll setHasVerticalScroller:YES];
      [iNotesPanel setContentView:scroll];
      [iNotesPanel setTitle:@"Ipe page notes"];
    }
    if (vis)
      [iNotesPanel orderFront:iWindow];
    else
      [iNotesPanel orderOut:iWindow];
  }
}

// --------------------------------------------------------------------

void AppUi::layerMenu(NSPoint p, NSString *layer)
{
  luaShowLayerBoxPopup(Vector(p.x, p.y), N2I(layer));
}

void AppUi::layerAction(NSString *actionName, NSString *layer)
{
  luaLayerAction(N2I(actionName), N2I(layer));
}

// --------------------------------------------------------------------

void AppUi::absoluteButton(int sel)
{
  luaAbsoluteButton(selectorNames[sel]);
}

void AppUi::snapButton(int sel)
{
  NSString *a = C2N(snapbutton_action[sel]);
  IpeAction *s = findAction(a);
  if (s) {
    s.state = (iSnapButton[sel].state == NSOnState);
    luaAction(N2I(a));
  }
}

void AppUi::selectorChanged(int sel)
{
  if (iInUiUpdate)
    return;
  int idx = [iSelector[sel] indexOfSelectedItem];
  luaSelector(String(selectorNames[sel]), iComboContents[sel][idx]);
}

IpeAction *AppUi::findAction(NSString *name) const
{
  return iActions[name];
}

// Determine if an action is checked.
// Used for viewmarked, pagemarked, snapXXX, grid_visible,
// pretty_display, toggle_notes, toggle_bookmarks.
bool AppUi::actionState(const char *name)
{
  if (!strcmp(name, "viewmarked"))
    return iViewMarked.state == NSOnState;
  if (!strcmp(name, "pagemarked"))
    return iPageMarked.state == NSOnState;
  IpeAction *s = findAction(C2N(name));
  if (s)
    return s.state;
  return false;
}

// Check/uncheck an action.
// Used by Lua for snapangle and grid_visible
// Also to initialize mode_select
void AppUi::setActionState(const char *name, bool value)
{
  NSString *a = C2N(name);
  IpeAction *s = findAction(a);
  if (s && s.toggles)
    s.state = value;
  if ([a hasPrefix:@"mode_"]) {
    if (value) {
      iModeIndicator.image = loadIcon(name);
      iWindow.toolbar.selectedItemIdentifier = a;
    }
  } else if ([a hasPrefix:@"snap"] || [a isEqualToString:@"grid_visible"]) {
    for (int i = 0; i < 8; ++i) {
      if ([a isEqualToString:C2N(snapbutton_action[i])])
	iSnapButton[i].state = value ? NSOnState : NSOffState;
    }
  } else if ([a isEqualToString:@"shift_key"]) {
    int mod = s.state ? CanvasBase::EShift : 0;
    if (iCanvas)
      iCanvas->setAdditionalModifiers(mod);
  }
}

void AppUi::action(String name)
{
  ipeDebug("action %s", name.z());
  // Implement radio buttons
  int i = name.find("|");
  if (i >= 0)
    setCheckMark(name.left(i+1), name.substr(i+1));
  if (name.hasPrefix("mode_")) {
    setCheckMark("mode_", name.substr(5));
    setActionState(name.z(), true);
  }
  // Implement toggle actions
  IpeAction *s = findAction(I2N(name));
  if (s && s.toggles)
    setActionState(name.z(), !s.state);
  luaAction(name);
}

// --------------------------------------------------------------------

WINID AppUi::windowId()
{
  return iWindow;
}

void AppUi::setWindowCaption(bool mod, const char *s)
{
  [iWindow setTitle:[[NSString alloc] initWithUTF8String:s]];
}

void AppUi::showWindow(int width, int height)
{
  if (width > 0 && height > 0) {
    NSRect e = [[NSScreen mainScreen] frame];
    auto wd = e.size.width - width;
    auto hd = e.size.height - height;
    NSRect winr = NSMakeRect(0.5 * wd, 0.5 * hd, width, height);
    [iWindow setFrame:winr display:YES];
  }
  [iWindow makeKeyAndOrderFront:iWindow];
}

// show for t milliseconds, or permanently if t == 0
void AppUi::explain(const char *s, int t)
{
  if (iIndicatorTimer) {
    [iIndicatorTimer invalidate];
    iIndicatorTimer = nil;
  }
  if (t)
    iIndicatorTimer =
      [NSTimer scheduledTimerWithTimeInterval:t / 1000.0
				       target:iDelegate
				     selector:@selector(indicatorFired:)
				     userInfo:nil
				      repeats:NO];
  [iStatus setStringValue:C2N(s)];
}

void AppUi::setSnapIndicator(const char *s)
{
  [iSnapIndicator setStringValue:C2N(s)];
}

void AppUi::setMouseIndicator(const char *s)
{
  [iMouseIndicator setStringValue:C2N(s)];
}

void AppUi::setZoom(double zoom)
{
  iCanvas->setZoom(zoom);
  [iZoomIndicator setStringValue:
		    [NSString stringWithFormat:@"%3dppi", int(72.0 * zoom)]];
}

BOOL AppUi::closeRequested()
{
  // calls model
  lua_rawgeti(L, LUA_REGISTRYINDEX, iModel);
  lua_getfield(L, -1, "closeEvent");
  lua_pushvalue(L, -2); // model
  lua_remove(L, -3);
  lua_call(L, 1, 1);
  bool result = lua_toboolean(L, -1);
  return result;
}

BOOL AppUi::isModified()
{
  // calls model
  lua_rawgeti(L, LUA_REGISTRYINDEX, iModel);
  lua_getfield(L, -1, "isModified");
  lua_pushvalue(L, -2); // model
  lua_remove(L, -3);
  lua_call(L, 1, 1);
  bool result = lua_toboolean(L, -1);
  return result;
}

void AppUi::closeWindow()
{
  [iWindow performClose:iDelegate];
}

// --------------------------------------------------------------------

int AppUi::clipboard(lua_State *L)
{
  // bool allow_bitmap = lua_toboolean(L, 2);
  // NSImage can be read in the same way, but I don't
  // know how to convert it to a bitmap...
  NSPasteboard *pasteBoard = [NSPasteboard generalPasteboard];
  NSArray *arr = [pasteBoard readObjectsForClasses:@[[NSString class]]
					   options:@{ } ];
  if (arr && [arr count] > 0) {
    lua_pushstring(L, N2C(arr[0]));
    return 1;
  }
  return 0;
}

int AppUi::setClipboard(lua_State *L)
{
  const char *s = luaL_checkstring(L, 2);
  NSPasteboard *pasteBoard = [NSPasteboard generalPasteboard];
  [pasteBoard clearContents];
  [pasteBoard writeObjects:@[ C2N(s) ]];
  return 0;
}

// --------------------------------------------------------------------

AppUiBase *createAppUi(lua_State *L0, int model)
{
  return new AppUi(L0, model);
}

// --------------------------------------------------------------------
