/* XXX: The file should be refactored, I'm pretty sure the Handle->alive field
 * is not really needed, which would simplify the event handlers greatly. */

#include "moon.h"
#include "config.h"
#include <string.h>
#include <fcntl.h>

/* Handle structure {{{ */
typedef struct {
	LuaState *L;
	int callback;

	GIOChannel *channel;
	GQueue *queue;
	guint in_tag;
	guint out_tag;
	gboolean closed;
} Handle;
/* }}} */

/* Events that we need to call the callback on:
 * f("input")
 * f("invalid")
 * f("hangup")
 * f("idle")
 * f("error", nil)
 * f("error", error) */

/* {{{ Event Handlers */
static gboolean on_output(GIOChannel *ch, GIOCondition cond, gpointer data)/*{{{*/
{
	Handle *h = data;
	LuaState *L   = h->L;
	GQueue *queue = h->queue;
	
	if (g_queue_is_empty(h->queue)) {
		LuaState *L = h->L;
		moon_pushref(L, h->callback);
		lua_pushstring(L, "idle");
		if (lua_pcall(L, 1, 0, 0) != 0)
			g_warning("error running Handle callback for empty event: %s",
					lua_tostring(L, -1));
		return FALSE;
	} else {
		GError *err = NULL;
		GString *msg = (GString *)g_queue_pop_head(queue);
		gsize len = 0;
		GIOStatus status = g_io_channel_write_chars(h->channel, msg->str, msg->len, &len, &err);

		switch (status) {
			case G_IO_STATUS_NORMAL:
				if (len < msg->len) {
					g_string_erase(msg, 0, len);
					g_queue_push_head(queue, msg);
				} else if (len == msg->len) {
					g_string_free(msg, TRUE);
				} else
					g_assert_not_reached();
				break;
			case G_IO_STATUS_AGAIN:
				break;
			case G_IO_STATUS_EOF:
				/* TODO: is it possible to get EOF while writing to a socket? */
				g_assert_not_reached();
				break;
			case G_IO_STATUS_ERROR:
				g_assert(err != NULL);
				moon_pushref(L, h->callback);
				lua_pushstring(L, "error");
				moon_pusherror(L, err);
				g_error_free(err);
    			if (lua_pcall(L, 2, 0, 0) != 0)
    				g_warning("error running Handle callback on G_IO_STATUS_ERROR during write: %s",
    						lua_tostring(L, -1));
				return FALSE;
				break;
		}
		return TRUE;
	}
}/*}}}*/
static gboolean on_input(GIOChannel *ch, GIOCondition cond, gpointer data)/*{{{*/
{
	Handle *h   = data;
	LuaState *L = h->L;
	gboolean result = TRUE;

	moon_pushref(L, h->callback);

	if (cond & G_IO_NVAL) {
		lua_pushstring(L, "invalid");
		result = FALSE;
	}

	if (cond & G_IO_ERR) {
		lua_pushstring(L, "error");
		result = FALSE;
	}

	if (cond & G_IO_HUP) {
		lua_pushstring(L, "hangup");
		result = FALSE;
	}

	if (cond & G_IO_IN)
		lua_pushstring(L, "input");

	if (lua_pcall(L, 1, 0, 0) != 0)
    	g_warning("error running Handle callback for %s event: %s",
    			lua_tostring(L, -2),
    			lua_tostring(L, -1));
    
	return result;
}/*}}}*/
/* }}} */

/* Utility Functions {{{ */
static int handle_create(LuaState *L, GIOChannel *channel, int callback)
{
	Handle *h = moon_newclass(L, "Handle", sizeof(Handle));
	h->L = L;
	h->queue = g_queue_new();
	h->channel = channel;
	h->callback = callback;
	g_io_channel_set_encoding(h->channel, NULL, NULL);
	g_io_channel_set_buffered(h->channel, FALSE);
	g_io_channel_set_flags(h->channel, G_IO_FLAG_NONBLOCK, NULL);
	if (g_io_channel_get_flags(h->channel) & G_IO_FLAG_IS_READABLE)
		h->in_tag = g_io_add_watch(h->channel, G_IO_IN | G_IO_ERR | G_IO_HUP | G_IO_NVAL, on_input, h);
	else
		h->in_tag = g_io_add_watch(h->channel, G_IO_ERR | G_IO_HUP | G_IO_NVAL, on_input, h);
	h->closed = FALSE;
	return 1;
}
static void each_g_string(GString *msg, UNUSED gpointer data)
{
	g_string_free(msg, TRUE);
}
/* }}} */

/* Methods {{{ */
static int Handle_new(LuaState *L)/*{{{*/
{
	int fd       = luaL_checkinteger(L, 2);
	int callback = moon_ref(L, 3);
	GIOChannel *channel  = g_io_channel_unix_new(fd);
	return handle_create(L, channel, callback);
}/*}}}*/
static int Handle_open(LuaState *L)/*{{{*/
{
	const char *path = luaL_checkstring(L, 2);
	const char *mode = luaL_checkstring(L, 3);
	int callback     = moon_ref(L, 4);
	GError *error = NULL;
	GIOChannel *channel = g_io_channel_new_file(path, mode, &error);

	if (channel) {
		return handle_create(L, channel, callback);
	} else {
		moon_pusherror(L, error);
		g_error_free(error);
		return lua_error(L);
	}
}/*}}}*/
static int Handle_write(LuaState *L)/*{{{*/
{
	Handle *h = moon_checkclass(L, "Handle", 1);
	if (h->closed)
		luaL_error(L, "Cannot read from closed handle!");

	if (g_io_channel_get_flags(h->channel) & G_IO_FLAG_IS_WRITEABLE) {
		const char *str = luaL_checkstring(L, 2);
		if (g_queue_is_empty(h->queue))
			h->out_tag = g_io_add_watch(h->channel, G_IO_OUT, on_output, h);
		g_queue_push_tail(h->queue, g_string_new(str));
	} else {
		return luaL_error(L, "Cannot write to read-only Handle");
	}

	return 0;
}/*}}}*/
static int Handle_read(LuaState *L)/*{{{*/
{
	Handle *h = moon_checkclass(L, "Handle", 1);
	guint blocksize = luaL_optint(L, 2, 512);

	if (h->closed)
		luaL_error(L, "Cannot read from closed handle!");

	GError *err = NULL;
	gchar *str  = g_new0(gchar, blocksize);
	gsize len   = 0;
	GIOStatus status = g_io_channel_read_chars(h->channel, str, blocksize - 1, &len, &err);

	switch (status) {
		case G_IO_STATUS_NORMAL:
			lua_pushlstring(L, str, len);
			break;
		case G_IO_STATUS_AGAIN:
			break;
		case G_IO_STATUS_EOF:
			lua_pushnil(L);
			break;
		case G_IO_STATUS_ERROR:
			g_assert(err != NULL);
			moon_pusherror(L, err);
			g_error_free(err);
			g_free(str);
			return lua_error(L);
	}
	g_free(str);
	return 1;
}/*}}}*/
static int Handle_is_idle(LuaState *L)/*{{{*/
{
	Handle *h = moon_checkclass(L, "Handle", 1); 
	if (h->closed)
		luaL_error(L, "Cannot read from closed handle!");

	lua_pushboolean(L, g_queue_is_empty(h->queue));
	return 1;
}/*}}}*/
static int Handle_close(LuaState *L)/*{{{*/
{
	Handle *h = moon_checkclass(L, "Handle", 1);

	if (h->closed) return 0;

	if (!g_queue_is_empty(h->queue))
		g_source_remove(h->out_tag);
	if (h->in_tag > 0)
		g_source_remove(h->in_tag);
	moon_unref(L, h->callback);
	g_queue_foreach(h->queue, (GFunc)each_g_string, NULL);
	g_queue_free(h->queue);
	g_io_channel_shutdown(h->channel, TRUE, NULL);
	g_io_channel_unref(h->channel);
	h->closed = TRUE;
	return 0;
}/*}}}*/
static int Handle_tostring(LuaState *L)/*{{{*/
{
	char buff[32];
  	sprintf(buff, "%p", moon_toclass(L, "Handle", 1));
  	lua_pushfstring(L, "Handle (%s)", buff);
  	return 1;
}/*}}}*/
/* }}} */

/* Boilerplate {{{ */
static const LuaLReg Handle_methods[] = {
	{"new",     Handle_new},
	{"open",    Handle_open},
	{"write",   Handle_write},
	{"read",    Handle_read},
	{"is_idle", Handle_is_idle},
	{"close",   Handle_close},
	{0, 0}
};

static const LuaLReg Handle_meta[] = {
	{"__gc", Handle_close},
	{"__tostring", Handle_tostring},
	{0, 0}
};

int luaopen_handle(LuaState *L)
{
	moon_class_register(L, "Handle", Handle_methods, Handle_meta);
	return 1;
}
/* }}} */
