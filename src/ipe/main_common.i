// --------------------------------------------------------------------
// Common code for all platforms
// --------------------------------------------------------------------

#include <cstdio>
#include <cstdlib>

extern int luaopen_ipeui(lua_State *L);
extern int luaopen_appui(lua_State *L);

// --------------------------------------------------------------------

String ipeIconDirectory()
{
  static String iconDir;
  if (iconDir.empty()) {
    const char *p = getenv("IPEICONDIR");
#ifdef IPEBUNDLE
    String s(p ? p : Platform::ipeDir("icons"));
#else
    String s(p ? p : IPEICONDIR);
#endif    
    if (s.right(1) != "/")
      s += IPESEP;
    iconDir = s;
  }
  return iconDir;
}

static int traceback (lua_State *L)
{
  if (!lua_isstring(L, 1))  /* 'message' not a string? */
    return 1;  /* keep it intact */
  lua_rawgeti(L, LUA_REGISTRYINDEX, LUA_RIDX_GLOBALS);
  lua_getfield(L, -1, "debug");
  if (!lua_istable(L, -1)) {
    lua_pop(L, 2);
    return 1;
  }
  lua_getfield(L, -1, "traceback");
  if (!lua_isfunction(L, -1)) {
    lua_pop(L, 3);
    return 1;
  }
  lua_pushvalue(L, 1);    // pass error message
  lua_pushinteger(L, 2);  // skip this function and traceback
  lua_call(L, 2, 1);      // call debug.traceback
  return 1;
}

static void setup_config(lua_State *L, const char *var, 
			 const char *env, const char *conf)
{
  const char *p = env ? getenv(env) : 0;
#ifdef IPEBUNDLE
  push_string(L, p ? p : Platform::ipeDir(conf));
#else
  lua_pushstring(L, p ? p : conf);
#endif
  lua_setfield(L, -2, var);
}

// --------------------------------------------------------------------

/* Replacement for Lua's tonumber function,
   which is locale-dependent. */
static int ipe_tonumber(lua_State *L)
{
  const char *s = luaL_checkstring(L, 1);
  lua_pushnumber(L, Platform::toDouble(s));
  return 1;
}

static lua_State *setup_lua()
{
  lua_State *L = luaL_newstate();
  luaL_openlibs(L);
  luaopen_ipe(L);
  luaopen_ipeui(L);
  luaopen_appui(L);
  return L;
}

static bool lua_run_ipe(lua_State *L, lua_CFunction fn)
{
  lua_pushcfunction(L, fn);
  lua_setglobal(L, "mainloop");
  
  // run Ipe
  lua_pushcfunction(L, traceback);
  assert(luaL_loadstring(L, "require \"main\"") == 0);

  if (lua_pcall(L, 0, 0, -2)) {
    const char *errmsg = lua_tostring(L, -1);
    fprintf(stderr, "%s\n", errmsg);
    return false;
  }
  return true;
}

// --------------------------------------------------------------------
