#include <string.h>
#include <stdlib.h>
#include <glib.h>
#include <lua.h>
#include <lualib.h>

#include "config.h"
#include "keyboard.h"

struct _Keyboard {
	SLkeymap_Type *keymap;
	GIOChannel    *channel;
	lua_State     *lua;
};

static const char *readkey(Keyboard *kb)
{
	SLang_Key_Type *key = SLang_do_key(kb->keymap, (int (*)(void)) SLang_getkey);

	if (key == NULL || key->type != SLKEY_F_KEYSYM) {
		SLang_flush_input();
		return NULL;
	}
	return g_quark_to_string(key->f.keysym);
}

inline static void init_keymap(Keyboard *kb)
{
   	char esc_seq[10];

	g_assert(kb->keymap != NULL);

   	esc_seq[1] = 0;
   	for (int i = 1; i < 256; i++) {
   		if (i == 127)
   			continue;
   	   	esc_seq[0] = (char) i;
   	   	SLkm_define_keysym(esc_seq, g_quark_from_string(esc_seq), kb->keymap);
   	}
   	strcpy (esc_seq, "^(kX)");
   	for (int i = 0; i <= 9; i++)
   	{
   		GString *string = g_string_new("");
   		g_string_printf(string, "F%d", i);
	   	esc_seq[3] = '0' + i;
	   	keyboard_define(kb, esc_seq, string->str);
	   	g_string_free(string, TRUE);
   	}
   	keyboard_define(kb, "^(k;)", "F10");
   	keyboard_define(kb, "^(F1)", "F11");
   	keyboard_define(kb, "^(F2)", "F12");

   	keyboard_define(kb, "^(ku)", "UP");
   	keyboard_define(kb, "^(kd)", "DOWN");
   	keyboard_define(kb, "^(kl)", "LEFT");
   	keyboard_define(kb, "^(kr)", "RIGHT");
   	keyboard_define(kb, "^(kP)", "PPAGE");
   	keyboard_define(kb, "^(kN)", "NPAGE");
   	keyboard_define(kb, "^(kh)", "HOME");
   	keyboard_define(kb, "^(@7)", "END");
   	keyboard_define(kb, "^(K1)", "A1");
   	keyboard_define(kb, "^(K3)", "A3");
   	keyboard_define(kb, "^(K2)", "B2");
   	keyboard_define(kb, "^(K4)", "C1");
   	keyboard_define(kb, "^(K5)", "C3");
   	keyboard_define(kb, "^(%0)", "REDO");
   	keyboard_define(kb, "^(&8)", "UNDO");
   	keyboard_define(kb, "^(kb)", "BACKSPACE");
   	keyboard_define(kb, "^(@8)", "ENTER");
   	keyboard_define(kb, "^(kD)", "DELETE");
}

static gboolean on_input(
		GIOChannel *input, 
		GIOCondition cond,
		gpointer data)
{
	if (cond & G_IO_ERR || cond & G_IO_HUP || cond & G_IO_NVAL) {
		g_print("Got IO error");
		exit(1);
	} else if (cond & G_IO_IN) {
		Keyboard *kb = (Keyboard *)data;
		const char *s = readkey(kb);
		if (s) {
			lua_getglobal(kb->lua, "keypress");
			lua_pushstring(kb->lua, s);
			lua_call(kb->lua, 1, 0);
		}
		return TRUE;
	} else {
		return FALSE;
	}
}

Keyboard *keyboard_new(lua_State *L)
{
	Keyboard *kb  = g_new(Keyboard, 1);
	kb->keymap    = SLang_create_keymap("default", NULL);
	kb->channel   = g_io_channel_unix_new (fileno(stdin));
	kb->lua       = L;
	init_keymap(kb);
	g_io_add_watch(kb->channel, G_IO_IN | G_IO_ERR | G_IO_HUP | G_IO_NVAL, on_input, kb);

	return kb;
}

void keyboard_free(Keyboard *kb)
{
	/* how do I free kb->keymap? */
	g_io_channel_unref(kb->channel);
	g_free(kb);
}

void keyboard_define(Keyboard *kb, char *spec, char *name)
{
	g_assert(spec != NULL);
	g_assert(name != NULL);
	SLkm_define_keysym(spec, g_quark_from_string(name), kb->keymap);
}
