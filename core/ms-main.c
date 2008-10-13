#include <stdio.h>
#include <stdlib.h>
#include "moonshine/ms-lua.h"
#include "moonshine/ms-signal.h"
#include "moonshine/config.h"

static void init_paths(LuaState *L);
static int os_exit(LuaState *L);

int main(int argc, char *argv[])
{
	GMainLoop *loop = g_main_loop_new(NULL, FALSE);
	LuaState *L     = lua_open();
	luaL_openlibs(L);

	g_thread_init(NULL);
	ms_signal_init();
	init_paths(L);

	/* argv = { name = argv[0], argv[1], argv[n], ... } */
	lua_createtable(L, argc, 1);
	lua_pushstring(L, argv[0]);
	lua_setfield(L, -2, "name");
	for (int i = 1; i < argc; i++) {
		lua_pushstring(L, argv[i]);
		lua_rawseti(L, -2, i);
	}
	lua_setglobal(L, "argv");

	/* os.exit = os_exit */
	lua_getglobal(L, "os");
	lua_pushlightuserdata(L, loop);
	lua_pushcclosure(L, os_exit, 1);
	lua_setfield(L, -2, "exit");
	lua_pop(L, 1);
	
	ms_lua_require(L, "moonshine.prelude");
	ms_lua_require(L, "moonshine");
	ms_lua_call_hook(L, "startup_hook");
	g_main_loop_run(loop);
	ms_lua_call_hook(L, "shutdown_hook");

	/* The next three function calls are terribly order-dependent. */
	ms_signal_reset();
	lua_close(L);
	g_main_loop_unref(loop);

	exit(0);
}

static int os_exit(LuaState *L)
{
	GMainLoop *loop = lua_touserdata(L, lua_upvalueindex(1));
	g_assert(loop != NULL);

	g_main_loop_quit(loop);

	return 0;
}

static void init_paths(LuaState *L)
{
	const char *runtime = MOONSHINE_RUNTIME;
	const char *modules = MOONSHINE_MODULES;

	/* push the global package onto the stack */
	lua_getglobal(L, "package");

	/* Assign package.path = runtime */
	lua_pushstring(L, runtime);
	lua_setfield(L, -2, "path");

	/* Assign package.cpath = modules */
	lua_pushstring(L, modules);
	lua_setfield(L, -2, "cpath");

	/* remove package from the stack. */
	lua_pop(L, 1);
}
