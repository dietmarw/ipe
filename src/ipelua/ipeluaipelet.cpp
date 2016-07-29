// --------------------------------------------------------------------
// ipeluaipelets.cpp
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

#ifdef WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

using namespace ipe;
using namespace ipelua;

typedef Ipelet *(*PNewIpeletFn)();

// --------------------------------------------------------------------

static int ipelet_destructor(lua_State *L)
{
  ipeDebug("Ipelet destructor");
  Ipelet **p = check_ipelet(L, 1);
  delete *p;
  *p = 0;
  return 0;
}

static int ipelet_tostring(lua_State *L)
{
  check_ipelet(L, 1);
  lua_pushfstring(L, "Ipelet@%p", lua_topointer(L, 1));
  return 1;
}

// --------------------------------------------------------------------

int ipelua::ipelet_constructor(lua_State *L)
{
  String fname(luaL_checkstring(L, 1));
#if defined(WIN32)
  String dllname = fname + ".dll";
#else
  String dllname = fname + ".so";
#endif
  ipeDebug("Loading dll '%s'", dllname.z());
  PNewIpeletFn pIpelet = 0;
#ifdef WIN32
  std::vector<wchar_t> wname;
  Platform::utf8ToWide(dllname.z(), wname);
  HMODULE hMod = LoadLibraryW(&wname[0]);
  if (hMod) {
    pIpelet = (PNewIpeletFn) GetProcAddress(hMod, "newIpelet");
    if (!pIpelet)
      pIpelet = (PNewIpeletFn) GetProcAddress(hMod, "_newIpelet");
  }
#else
  void *handle = dlopen(dllname.z(), RTLD_NOW);
#if defined(__APPLE__)
  if (!handle) {
    dllname = fname + ".dylib";
    handle = dlopen(dllname.z(), RTLD_NOW);
  }
#endif
  if (handle) {
    pIpelet = (PNewIpeletFn) dlsym(handle, "newIpelet");
    if (!pIpelet)
      pIpelet = (PNewIpeletFn) dlsym(handle, "_newIpelet");
  }
#endif
  if (pIpelet) {
    Ipelet **p = (Ipelet **) lua_newuserdata(L, sizeof(Ipelet *));
    *p = 0;
    luaL_getmetatable(L, "Ipe.ipelet");
    lua_setmetatable(L, -2);

    Ipelet *ipelet = pIpelet();
    if (ipelet == 0) {
      lua_pushnil(L);
      lua_pushstring(L, "ipelet returns no object");
      return 2;
    }

    if (ipelet->ipelibVersion() != IPELIB_VERSION) {
      delete ipelet;
      lua_pushnil(L);
      lua_pushstring(L, "ipelet linked against older version of Ipelib");
      return 2;
    }

    *p = ipelet;
    ipeDebug("Ipelet '%s' loaded", fname.z());
    return 1;
  } else {
    lua_pushnil(L);
#ifdef WIN32
    if (hMod)
      lua_pushfstring(L, "Error %d finding DLL entry point", GetLastError());
    else
      lua_pushfstring(L, "Error %d loading Ipelet DLL '%s'", GetLastError(),
		      dllname.z());
#else
    lua_pushfstring(L, "Error loading Ipelet '%s': %s", dllname.z(), dlerror());
#endif
    return 2;
  }
}

// --------------------------------------------------------------------

class Helper : public IpeletHelper {
public:
  Helper(lua_State *L0, int luahelper);
  ~Helper();
  virtual void message(const char *msg);
  virtual int messageBox(const char *text, const char *details,
			 int buttons);
  virtual bool getString(const char *prompt, String &str);
private:
  lua_State *L;
  int iHelper;
};

Helper::Helper(lua_State *L0, int luahelper)
{
  L = L0;
  iHelper = luahelper;
}

Helper::~Helper()
{
  luaL_unref(L, LUA_REGISTRYINDEX, iHelper);
}

void Helper::message(const char *msg)
{
  lua_rawgeti(L, LUA_REGISTRYINDEX, iHelper);
  lua_getfield(L, -1, "message");
  lua_pushvalue(L, -2); // luahelper
  lua_remove(L, -3);
  lua_pushstring(L, msg);
  lua_call(L, 2, 0);
}

int Helper::messageBox(const char *text, const char *details, int buttons)
{
  lua_rawgeti(L, LUA_REGISTRYINDEX, iHelper);
  lua_getfield(L, -1, "messageBox");
  lua_pushvalue(L, -2); // luahelper
  lua_remove(L, -3);
  lua_pushstring(L, text);
  if (details)
    lua_pushstring(L, details);
  else
    lua_pushnil(L);
  lua_pushnumber(L, buttons);
  lua_call(L, 4, 1);
  if (lua_isnumber(L, -1))
    return int(lua_tonumber(L, -1));
  else
    return 0;
}

bool Helper::getString(const char *prompt, String &str)
{
  lua_rawgeti(L, LUA_REGISTRYINDEX, iHelper);
  lua_getfield(L, -1, "getString");
  lua_pushvalue(L, -2); // luahelper
  lua_remove(L, -3);
  lua_pushstring(L, prompt);
  lua_call(L, 2, 1);
  if (lua_isstring(L, -1)) {
    str = lua_tostring(L, -1);
    return true;
  } else
    return false;
}

// --------------------------------------------------------------------

static int ipelet_run(lua_State *L)
{
  Ipelet **p = check_ipelet(L, 1);
  int num = (int)luaL_checkinteger(L, 2) - 1;  // Lua counts starting from one
  IpeletData data;
  data.iPage = check_page(L, 3)->page;
  data.iDoc = *check_document(L, 4);
  data.iPageNo = (int)luaL_checkinteger(L, 5);
  data.iView = (int)luaL_checkinteger(L, 6);
  data.iLayer = check_layer(L, 7, data.iPage);
  check_allattributes(L, 8, data.iAttributes);
  // data.iSnap;
  lua_pushvalue(L, 9);
  int luahelper = luaL_ref(L, LUA_REGISTRYINDEX);
  Helper helper(L, luahelper);
  bool result = (*p)->run(num, &data, &helper);
  lua_pushboolean(L, result);
  return 1;
}

// --------------------------------------------------------------------

static const struct luaL_Reg ipelet_methods[] = {
  { "__tostring", ipelet_tostring },
  { "__gc", ipelet_destructor },
  { "run", ipelet_run },
  { NULL, NULL }
};

// --------------------------------------------------------------------

int ipelua::open_ipelets(lua_State *L)
{
  make_metatable(L, "Ipe.ipelet", ipelet_methods);

  return 0;
}

// --------------------------------------------------------------------
