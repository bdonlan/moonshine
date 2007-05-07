//#include <glib.h>
//#include <gnet.h>
#include <slang.h>
#include <stdlib.h>

#include <glib.h>
#include "protocol.h"
#include "screen.h"
#include "ui.h"

static gboolean on_input(GIOChannel *input, GIOCondition cond, gpointer ui)
{
	if (cond & G_IO_IN) {
		haver_ui_getkey((HaverUI *)ui);
		return TRUE;
	} else {
		g_print("stdin error!");
		return FALSE;
	}
}

int main(int argc, char *argv[])
{
	haver_screen_init();
	HaverUI *ui = haver_ui_new();
	GMainLoop *loop   = g_main_loop_new(NULL, TRUE);
	GIOChannel *input = g_io_channel_unix_new (fileno(stdin));
	g_io_add_watch(input, G_IO_IN | G_IO_ERR | G_IO_HUP | G_IO_NVAL, on_input, ui);
	g_main_loop_run(loop);
	return 0;
}
