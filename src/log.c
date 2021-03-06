/* Moonshine - a Lua-based chat client
 *
 * Copyright (C) 2010 Dylan William Hardison
 *
 * This file is part of Moonshine.
 *
 * Moonshine is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Moonshine is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Moonshine.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "moonshine/config.h"
#include "moonshine/log.h"
#include "moonshine/term.h"
#include <glib.h>


static MSLogItem *make_item
    (const gchar *log_domain,
     GLogLevelFlags log_level,
     const gchar *message,
     UNUSED gpointer unused_data)
{
    MSLogItem *item = g_new0(MSLogItem, 1);
    item->log_domain = g_strdup(log_domain);
    item->log_level  = log_level;
    item->message    = g_strdup(message);
    return item;
}

static void free_item(gpointer data)
{
    MSLogItem *item = data;
    g_free(item->log_domain);
    g_free(item->message);
    g_free(item);
}

MSLog *ms_log_new(void)
{
    MSLog *log = g_new0(MSLog, 1);
    log->buffer = g_sequence_new(free_item);

    return log;
}

void ms_log_handler
    (const gchar *log_domain,
     GLogLevelFlags log_level,
     const gchar *message,
     gpointer ud)
{
    MSLog *log = (MSLog *)ud;

    g_sequence_append(log->buffer, make_item(log_domain, log_level, message, NULL));
}

static void replay_each_item(gpointer data, UNUSED gpointer ud)
{
    MSLogItem *item = data;
    g_log(item->log_domain, item->log_level, "%s", item->message);
}

void ms_log_replay(MSLog *log, GLogFunc func, gpointer user_data)
{
    g_log_set_default_handler(func, user_data);
    g_sequence_foreach(log->buffer, replay_each_item, NULL);

    // remove everything
    g_sequence_remove_range( g_sequence_get_begin_iter(log->buffer), g_sequence_get_end_iter(log->buffer));
}

void ms_log_free(MSLog *log)
{
    ms_log_replay(log, g_log_default_handler, NULL);

    g_sequence_free(log->buffer);
    g_free(log);
}

