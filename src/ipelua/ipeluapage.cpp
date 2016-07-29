// --------------------------------------------------------------------
// ipeluapage.cpp
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

#include "ipelua.h"

#include "ipepage.h"
#include "ipeiml.h"

using namespace ipe;
using namespace ipelua;

// --------------------------------------------------------------------

void ipelua::push_page(lua_State *L, Page *page, bool owned)
{
  SPage *p = (SPage *) lua_newuserdata(L, sizeof(SPage));
  p->owned = owned;
  luaL_getmetatable(L, "Ipe.page");
  lua_setmetatable(L, -2);
  p->page = page;
}

static int check_objno(lua_State *L, int i, Page *p, int extra = 0)
{
  int n = (int)luaL_checkinteger(L, i);
  luaL_argcheck(L, 1 <= n && n <= p->count() + extra,
		i, "invalid object index");
  return n - 1;
}

int ipelua::check_layer(lua_State *L, int i, Page *p)
{
  const char *name = luaL_checkstring(L, i);
  int l = p->findLayer(name);
  luaL_argcheck(L, l >= 0, i, "layer does not exist");
  return l;
}

int ipelua::check_viewno(lua_State *L, int i, Page *p, int extra)
{
  int n = (int)luaL_checkinteger(L, i);
  luaL_argcheck(L, 1 <= n && n <= p->countViews() + extra,
		i, "invalid view index");
  return n - 1;
}

int ipelua::page_constructor(lua_State *L)
{
  if (lua_isnoneornil(L, 1)) {
    push_page(L, Page::basic());
    return 1;
  } else {
    size_t len;
    const char *p = luaL_checklstring(L, 1, &len);
    Buffer data(p, len);
    BufferSource source(data);
    ImlParser parser(source);
    Page *page = parser.parsePageSelection();
    if (page) {
      push_page(L, page);
      return 1;
    } else
      return 0;
  }
}

static int page_destructor(lua_State *L)
{
  SPage *p = check_page(L, 1);
  if (p->owned)
    delete p->page;
  p->page = 0;
  return 0;
}

static int page_index(lua_State *L)
{
  Page *p = check_page(L, 1)->page;
  if (lua_type(L, 2) == LUA_TNUMBER) {
    int n = check_objno(L, 2, p);
    push_object(L, p->object(n), false);
  } else {
    const char *key = luaL_checkstring(L, 2);
    if (!luaL_getmetafield(L, 1, key))
      lua_pushnil(L);
  }
  return 1;
}

static int page_tostring(lua_State *L)
{
  check_page(L, 1);
  lua_pushfstring(L, "Page@%p", lua_topointer(L, 1));
  return 1;
}

static int page_len(lua_State *L)
{
  Page *p = check_page(L, 1)->page;
  lua_pushinteger(L, p->count());
  return 1;
}

static int page_clone(lua_State *L)
{
  Page *p = check_page(L, 1)->page;
  push_page(L, new Page(*p));
  return 1;
}

// --------------------------------------------------------------------

static void push_select(lua_State *L, TSelect sel)
{
  if (sel == ENotSelected)
    lua_pushnil(L);
  else if (sel == EPrimarySelected)
    lua_pushnumber(L, 1);
  else
    lua_pushnumber(L, 2);
}

static TSelect check_select(lua_State *L, int index)
{
  TSelect w = ENotSelected;
  if (!lua_isnoneornil(L, index)) {
    if ((int)luaL_checkinteger(L, index) == 1)
      w = EPrimarySelected;
    else
      w = ESecondarySelected;
  }
  return w;
}

// arguments: page, counter
static int page_object_iterator(lua_State *L)
{
  Page *p = check_page(L, 1)->page;
  int i = (int)luaL_checkinteger(L, 2);
  i = i + 1;
  if (i <= p->count()) {
    lua_pushnumber(L, i);                            // new counter
    push_object(L, p->object(i-1), false);           // object
    push_select(L, p->select(i-1));
    push_string(L, p->layer(p->layerOf(i-1)));       // layer
    return 4;
  } else
    return 0;
}

// returns object iterator for use in for loop
// returns iterator function, invariant state, control variable
static int page_objects(lua_State *L)
{
  (void) check_page(L, 1);
  lua_pushcfunction(L, page_object_iterator); // iterator function
  lua_pushvalue(L, 1);          // page
  lua_pushnumber(L, 0);         // counter
  return 3;
}

// --------------------------------------------------------------------

static int page_xml(lua_State *L)
{
  static const char * const option_names[] =
    { "ipepage", "ipeselection", 0 };
  Page *p = check_page(L, 1)->page;
  int t = luaL_checkoption(L, 2, 0, option_names);
  String data;
  StringStream stream(data);
  if (t == 0)
    p->saveAsIpePage(stream);
  else if (t == 1)
    p->saveSelection(stream);
  push_string(L, data);
  return 1;
}

static int page_layers(lua_State *L)
{
  Page *p = check_page(L, 1)->page;
  lua_createtable(L, 0, p->countLayers());
  for (int i = 0; i < p->countLayers(); ++i) {
    push_string(L, p->layer(i));
    lua_rawseti(L, -2, i + 1);
  }
  return 1;
}

static int page_countLayers(lua_State *L)
{
  Page *p = check_page(L, 1)->page;
  lua_pushinteger(L, p->countLayers());
  return 1;
}

static int page_isLocked(lua_State *L)
{
  Page *p = check_page(L, 1)->page;
  int n = check_layer(L, 2, p);
  lua_pushboolean(L, p->isLocked(n));
  return 1;
}

static int page_hasSnapping(lua_State *L)
{
  Page *p = check_page(L, 1)->page;
  int n = check_layer(L, 2, p);
  lua_pushboolean(L, p->hasSnapping(n));
  return 1;
}

static int page_setLocked(lua_State *L)
{
  Page *p = check_page(L, 1)->page;
  int n = check_layer(L, 2, p);
  p->setLocked(n, lua_toboolean(L, 3));
  return 0;
}

static int page_setSnapping(lua_State *L)
{
  Page *p = check_page(L, 1)->page;
  int n = check_layer(L, 2, p);
  p->setSnapping(n, lua_toboolean(L, 3));
  return 0;
}

static int page_renameLayer(lua_State *L)
{
  Page *p = check_page(L, 1)->page;
  const char *s1 = luaL_checkstring(L, 2);
  const char *s2 = luaL_checkstring(L, 3);
  p->renameLayer(s1, s2);
  return 0;
}

static int page_addLayer(lua_State *L)
{
  Page *p = check_page(L, 1)->page;
  if (lua_isnoneornil(L, 2)) {
    p->addLayer();
  } else {
    const char *s = luaL_checkstring(L, 2);
    p->addLayer(s);
  }
  push_string(L, p->layer(p->countLayers() - 1));
  return 1;
}

static int page_removeLayer(lua_State *L)
{
  Page *p = check_page(L, 1)->page;
  int n = check_layer(L, 2, p);
  p->removeLayer(p->layer(n));
  return 0;
}

static int page_moveLayer(lua_State *L)
{
  Page *p = check_page(L, 1)->page;
  int index = check_layer(L, 2, p);
  int newIndex = (int)luaL_checkinteger(L, 3) - 1;
  luaL_argcheck(L, 0 <= newIndex && newIndex < p->countLayers(),
		3, "invalid target index");
  p->moveLayer(index, newIndex);
  return 0;
}

// --------------------------------------------------------------------

static int page_select(lua_State *L)
{
  Page *p = check_page(L, 1)->page;
  int n = check_objno(L, 2, p);
  push_select(L, p->select(n));
  return 1;
}

static int page_setSelect(lua_State *L)
{
  Page *p = check_page(L, 1)->page;
  int n = check_objno(L, 2, p);
  TSelect w = check_select(L, 3);
  p->setSelect(n, w);
  return 0;
}

static int page_layerOf(lua_State *L)
{
  Page *p = check_page(L, 1)->page;
  int n = check_objno(L, 2, p);
  push_string(L, p->layer(p->layerOf(n)));
  return 1;
}

static int page_setLayerOf(lua_State *L)
{
  Page *p = check_page(L, 1)->page;
  int n = check_objno(L, 2, p);
  const char *s = luaL_checkstring(L, 3);
  int l = p->findLayer(s);
  luaL_argcheck(L, l >= 0, 3, "layer does not exist");
  p->setLayerOf(n, l);
  return 0;
}

static int page_bbox(lua_State *L)
{
  Page *p = check_page(L, 1)->page;
  int n = check_objno(L, 2, p);
  push_rect(L, p->bbox(n));
  return 1;
}

// use index nil to append
static int page_insert(lua_State *L)
{
  Page *p = check_page(L, 1)->page;
  int n;
  if (lua_isnil(L, 2))
    n = p->count();
  else
    n = check_objno(L, 2, p, 1);
  SObject *obj = check_object(L, 3);
  TSelect select = check_select(L, 4);
  int l = check_layer(L, 5, p);
  p->insert(n, select, l, obj->obj->clone());
  return 0;
}

static int page_remove(lua_State *L)
{
  Page *p = check_page(L, 1)->page;
  int n = check_objno(L, 2, p);
  p->remove(n);
  return 0;
}

static int page_replace(lua_State *L)
{
  Page *p = check_page(L, 1)->page;
  int n = check_objno(L, 2, p);
  SObject *obj = check_object(L, 3);
  p->replace(n, obj->obj->clone());
  return 0;
}

static int page_invalidateBBox(lua_State *L)
{
  Page *p = check_page(L, 1)->page;
  int n = check_objno(L, 2, p);
  p->invalidateBBox(n);
  return 0;
}

static int page_transform(lua_State *L)
{
  Page *p = check_page(L, 1)->page;
  int n = check_objno(L, 2, p);
  Matrix *m = check_matrix(L, 3);
  p->transform(n, *m);
  return 0;
}

static int page_distance(lua_State *L)
{
  Page *p = check_page(L, 1)->page;
  int n = check_objno(L, 2, p);
  Vector *v = check_vector(L, 3);
  double bound = luaL_checknumber(L, 4);
  lua_pushnumber(L, p->distance(n, *v, bound));
  return 1;
}

static int page_setAttribute(lua_State *L)
{
  Page *p = check_page(L, 1)->page;
  int n = check_objno(L, 2, p);
  Property prop = Property(luaL_checkoption(L, 3, NULL, property_names));
  Attribute value = check_property(prop, L, 4);
  lua_pushboolean(L, p->setAttribute(n, prop, value));
  return 1;
}

static int page_primarySelection(lua_State *L)
{
  Page *p = check_page(L, 1)->page;
  int prim = p->primarySelection();
  if (prim >= 0) {
    lua_pushnumber(L, prim + 1);
    return 1;
  } else
    return 0;
}

static int page_hasSelection(lua_State *L)
{
  Page *p = check_page(L, 1)->page;
  lua_pushboolean(L, p->hasSelection());
  return 1;
}

static int page_deselectAll(lua_State *L)
{
  Page *p = check_page(L, 1)->page;
  p->deselectAll();
  return 0;
}

static int page_ensurePrimarySelection(lua_State *L)
{
  Page *p = check_page(L, 1)->page;
  p->ensurePrimarySelection();
  return 0;
}

static int page_titles(lua_State *L)
{
  Page *p = check_page(L, 1)->page;
  lua_createtable(L, 3, 0);
  push_string(L, p->title());
  lua_setfield(L, -2, "title");
  if (!p->sectionUsesTitle(0)) {
    push_string(L, p->section(0));
    lua_setfield(L, -2, "section");
  }
  if (!p->sectionUsesTitle(1)) {
    push_string(L, p->section(1));
    lua_setfield(L, -2, "subsection");
  }
  return 1;
}

static int page_setTitles(lua_State *L)
{
  Page *p = check_page(L, 1)->page;
  luaL_checktype(L, 2, LUA_TTABLE);
  lua_getfield(L, 2, "title");
  if (lua_isstring(L, -1))
    p->setTitle(lua_tostring(L, -1));
  lua_getfield(L, 2, "section");
  if (lua_isstring(L, -1))
    p->setSection(0, false, lua_tostring(L, -1));
  else
    p->setSection(0, true, "");
  lua_getfield(L, 2, "subsection");
  if (lua_isstring(L, -1))
    p->setSection(1, false, lua_tostring(L, -1));
  else
    p->setSection(1, true, "");
  lua_pop(L, 3); // title, section, subsection
  return 0;
}

static int page_notes(lua_State *L)
{
  Page *p = check_page(L, 1)->page;
  push_string(L, p->notes());
  return 1;
}

static int page_setNotes(lua_State *L)
{
  Page *p = check_page(L, 1)->page;
  String n = luaL_checkstring(L, 2);
  p->setNotes(n);
  return 0;
}

static int page_marked(lua_State *L)
{
  Page *p = check_page(L, 1)->page;
  lua_pushboolean(L, p->marked());
  return 1;
}

static int page_setMarked(lua_State *L)
{
  Page *p = check_page(L, 1)->page;
  p->setMarked(lua_toboolean(L, 2));
  return 0;
}

// --------------------------------------------------------------------

static int page_countViews(lua_State *L)
{
  Page *p = check_page(L, 1)->page;
  lua_pushinteger(L, p->countViews());
  return 1;
}

static int page_effect(lua_State *L)
{
  Page *p = check_page(L, 1)->page;
  int n = check_viewno(L, 2, p);
  push_string(L, p->effect(n).string());
  return 1;
}

static int page_setEffect(lua_State *L)
{
  Page *p = check_page(L, 1)->page;
  int n = check_viewno(L, 2, p);
  const char *eff = luaL_checkstring(L, 3);
  p->setEffect(n, Attribute(true, eff));
  return 0;
}

static int page_active(lua_State *L)
{
  Page *p = check_page(L, 1)->page;
  int n = check_viewno(L, 2, p);
  push_string(L, p->active(n));
  return 1;
}

static int page_setActive(lua_State *L)
{
  Page *p = check_page(L, 1)->page;
  int n = check_viewno(L, 2, p);
  const char *name = luaL_checkstring(L, 3);
  p->setActive(n, name);
  return 0;
}

static int page_insertView(lua_State *L)
{
  Page *p = check_page(L, 1)->page;
  int n = check_viewno(L, 2, p, 1);
  const char *name = luaL_checkstring(L, 3);
  p->insertView(n, name);
  return 0;
}

static int page_removeView(lua_State *L)
{
  Page *p = check_page(L, 1)->page;
  int n = check_viewno(L, 2, p);
  p->removeView(n);
  return 0;
}

static int page_clearViews(lua_State *L)
{
  Page *p = check_page(L, 1)->page;
  p->clearViews();
  return 0;
}

static int page_markedView(lua_State *L)
{
  Page *p = check_page(L, 1)->page;
  int n = check_viewno(L, 2, p);
  lua_pushboolean(L, p->markedView(n));
  return 1;
}

static int page_setMarkedView(lua_State *L)
{
  Page *p = check_page(L, 1)->page;
  int n = check_viewno(L, 2, p);
  p->setMarkedView(n, lua_toboolean(L, 3));
  return 0;
}

// Either: view & layername
// Or: view & object index
static int page_visible(lua_State *L)
{
  Page *p = check_page(L, 1)->page;
  int vno = check_viewno(L, 2, p);
  if (lua_type(L, 3) == LUA_TNUMBER) {
    int objno = check_objno(L, 3, p);
    lua_pushboolean(L, p->objectVisible(vno, objno));
  } else {
    int l = check_layer(L, 3, p);
    lua_pushboolean(L, p->visible(vno, l));
  }
  return 1;
}

static int page_setVisible(lua_State *L)
{
  Page *p = check_page(L, 1)->page;
  int vno = check_viewno(L, 2, p);
  int l = check_layer(L, 3, p);
  bool vis = lua_toboolean(L, 4);
  p->setVisible(vno, p->layer(l), vis);
  return 0;
}

// --------------------------------------------------------------------

static int page_findedge(lua_State *L)
{
  Page *p = check_page(L, 1)->page;
  int view = check_viewno(L, 2, p, 0);
  Vector pos = *check_vector(L, 3);
  Snap snap;
  if (!snap.setEdge(pos, p, view))
    return 0;
  push_vector(L, snap.iOrigin);
  lua_pushnumber(L, snap.iDir);
  return 2;
}

// --------------------------------------------------------------------

static const struct luaL_Reg page_methods[] = {
  { "__index", page_index },
  { "__tostring", page_tostring },
  { "__gc", page_destructor },
  { "__len", page_len },
  { "clone", page_clone },
  { "objects", page_objects },
  { "countViews", page_countViews },
  { "countLayers", page_countLayers },
  { "xml", page_xml },
  { "layers", page_layers },
  { "isLocked", page_isLocked },
  { "hasSnapping", page_hasSnapping },
  { "setLocked", page_setLocked },
  { "setSnapping", page_setSnapping },
  { "renameLayer", page_renameLayer },
  { "addLayer", page_addLayer },
  { "removeLayer", page_removeLayer },
  { "moveLayer", page_moveLayer },
  { "select", page_select },
  { "setSelect", page_setSelect },
  { "layerOf", page_layerOf },
  { "setLayerOf", page_setLayerOf },
  { "effect", page_effect },
  { "setEffect", page_setEffect },
  { "active", page_active },
  { "setActive", page_setActive },
  { "insertView", page_insertView },
  { "removeView", page_removeView },
  { "clearViews", page_clearViews },
  { "markedView", page_markedView },
  { "setMarkedView", page_setMarkedView },
  { "visible", page_visible },
  { "setVisible", page_setVisible },
  { "bbox", page_bbox },
  { "insert", page_insert },
  { "remove", page_remove },
  { "replace", page_replace },
  { "invalidateBBox", page_invalidateBBox  },
  { "transform", page_transform },
  { "distance", page_distance },
  { "setAttribute", page_setAttribute },
  { "primarySelection", page_primarySelection },
  { "hasSelection", page_hasSelection },
  { "deselectAll", page_deselectAll },
  { "ensurePrimarySelection", page_ensurePrimarySelection },
  { "findEdge", page_findedge },
  { "titles", page_titles },
  { "setTitles", page_setTitles },
  { "notes", page_notes },
  { "setNotes", page_setNotes },
  { "marked", page_marked },
  { "setMarked", page_setMarked },
  { 0, 0 }
};

// --------------------------------------------------------------------

int ipelua::open_ipepage(lua_State *L)
{
  luaL_newmetatable(L, "Ipe.page");
  luaL_setfuncs(L, page_methods, 0);
  lua_pop(L, 1);

  return 0;
}

// --------------------------------------------------------------------

