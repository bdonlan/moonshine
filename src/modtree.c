/* vim: set ft=c noexpandtab ts=4 sw=4 tw=80: */

/* Moonshine - a Lua-based chat client
 *
 * Copyright (C) 2010 Bryan Donlan, Dylan William Hardison
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

#include <moonshine/config.h>
#include <moonshine/lua.h>
#include <moonshine/lua_var.h>
#include "avl/avl.h"

#define CLASS "moonshine.tree"

struct luatree/*{{{*/
{
    struct avl_tree_t *avltree;
    MSLuaVar *keycmp; /* use strcmp if null */
};/*}}}*/

struct luaitem/*{{{*/
{
    MSLuaVar *key, *value;
    struct luatree *tree;
};/*}}}*/

static void free_item(void *vp_item)/*{{{*/
{
    struct luaitem *item = vp_item;

    ms_lua_var_unref(item->key);
    ms_lua_var_unref(item->value);

    g_free(item);
}/*}}}*/

static int compare_keys(const void *vp_b)/*{{{*/
{
    const struct luaitem *b = vp_b;
    struct luatree *tree = b->tree;

    int result;

    if (tree->keycmp) {
        LuaState *L = ms_lua_var_push(tree->keycmp);
        lua_pushvalue(L, -2); /* re-push key A */
        LuaState *Lb = ms_lua_var_push(b->key);
        g_assert(L == Lb);

        lua_call(L, 2, 1);

        if (lua_type(L, -1) != LUA_TNUMBER)
            return luaL_error(L, "tree key compare: return value from comparator is not an integer");

        result = lua_tointeger(L, -1);
        lua_pop(L, 1);
    } else {
        size_t lenA, lenB;
        const char *strA, *strB;

        LuaState *L = ms_lua_var_push(b->key);

        /* builtin strcmp */
        strA = lua_tolstring(L, -2, &lenA);
        strB = lua_tolstring(L, -1, &lenB);

        result = memcmp(strA, strB, (lenA > lenB ? lenB : lenA) + 1);
        lua_pop(L, 1);
    }

    return result;
}/*}}}*/

/* On entry: Key in -1, value in -2 */
static avl_node_t *create_node(LuaState *L, struct luatree *tree)/*{{{*/
{
    struct luaitem *item = g_malloc(sizeof(*item));
    item->tree  = tree;
    item->key   = ms_lua_var_new_full(L, -1, LUA_TNONE, TRUE);
    item->value = ms_lua_var_new_full(L, -2, LUA_TNONE, TRUE);

    avl_node_t *node = avl_init_node(g_malloc(sizeof(*node)), item);

    return node;
}/*}}}*/

/* On entry: Key in -1, value in -2
 *
 * On exit: Old value (or nil) in -1. both entry values popped
 */

static void do_insert_replace(LuaState *L, struct luatree *tree)/*{{{*/
{
    avl_node_t *node;
    /* We can't just use the builtin avl tree insert, as it doesn't have a way
     * to easily replace a node without doing a second search... and doesn't
     * know about the lua stack.
     *
     * So, open-code it here.
     */

    if (!tree->avltree->top) {
        avl_insert_top(tree->avltree, create_node(L, tree));
        goto ret_nil;
    }

    switch (avl_search_closest(tree->avltree, &node)) {
        case -1:
            avl_insert_before(tree->avltree, node, create_node(L, tree));
            goto ret_nil;
        case 1:
            avl_insert_after(tree->avltree, node, create_node(L, tree));
            goto ret_nil;
        case 0:
            {
                /* Replace the existing value */
                struct luaitem *item = node->item;
                MSLuaVar *old_value = item->value;
                item->value = ms_lua_var_new_full(L, -2, LUA_TNONE, TRUE);

                /* Clean up, then push the old value onto the stack */
                lua_pop(L, 2);
                ms_lua_var_push(old_value);
                return;
            }
    }

ret_nil:
    lua_pop(L, 2);
    lua_pushnil(L);
}/*}}}*/


/* Lua interface */

static int tree_new(LuaState *L)/*{{{*/
{
    MSLuaVar *keycmp = NULL;

    if (!lua_isnoneornil(L, 2)) {
        luaL_checktype(L, 2, LUA_TFUNCTION);
        keycmp = ms_lua_var_new_full(L, 2, LUA_TNONE, TRUE);
    }

    struct luatree *tree = ms_lua_newclass(L, CLASS, sizeof(struct luatree));
    tree->keycmp = keycmp;
    tree->avltree = avl_alloc_tree(compare_keys, free_item);

    return 1;
}/*}}}*/

static int tree_gc(LuaState *L)/*{{{*/
{
    struct luatree *tree = ms_lua_checkclass(L, CLASS, 1);
    avl_free_tree(tree->avltree);
    if (tree->keycmp)
        ms_lua_var_unref(tree->keycmp);
    return 0;
}/*}}}*/

static int tree_tostring(LuaState *L)/*{{{*/
{
    char buff[32];
    sprintf(buff, "%p", ms_lua_toclass(L, CLASS, 1));
    lua_pushfstring(L, "Tree (%s)", buff);
    return 1;
}/*}}}*/

static int tree_insert(LuaState *L)/*{{{*/
{
    struct luatree *tree = ms_lua_toclass(L, CLASS, 1);
    luaL_argcheck(L, lua_gettop(L) == 3, lua_gettop(L), "Wrong number of arguments (need 3)");

    /* key at -1, value at -2 */
    lua_pushvalue(L, 3); /* value */
    lua_pushvalue(L, 2); /* key */

    do_insert_replace(L, tree);
    return 1;
}/*}}}*/

static int tree_delete(LuaState *L)/*{{{*/
{
    struct luatree *tree = ms_lua_checkclass(L, CLASS, 1);
    luaL_argcheck(L, lua_gettop(L) == 2, lua_gettop(L), "Wrong number of arguments (need 2)");

    lua_pushvalue(L, 2); /* put key at -1 */
    avl_node_t *node = avl_search(tree->avltree);
    lua_pop(L, 1); /* remove key */

    if (!node) {
        lua_pushnil(L);
        return 1;
    }

    struct luaitem *item = node->item;
    ms_lua_var_push(item->value);
    avl_delete_node(tree->avltree, node);

    return 1;
}/*}}}*/

static int tree_clear(LuaState *L)/*{{{*/
{
    struct luatree *tree = ms_lua_checkclass(L, CLASS, 1);
    luaL_argcheck(L, lua_gettop(L) == 1, lua_gettop(L), "Wrong number of arguments (need 1)");

    avl_free_nodes(tree->avltree);
    avl_init_tree(tree->avltree, compare_keys, free_item);

    return 0;
}/*}}}*/

static int tree_find(LuaState *L)/*{{{*/
{
    struct luatree *tree = ms_lua_checkclass(L, CLASS, 1);
    luaL_argcheck(L, lua_gettop(L) == 2, lua_gettop(L), "Wrong number of arguments (need 2)");

    lua_pushvalue(L, 2); /* put key at -1 */
    avl_node_t *node = avl_search(tree->avltree);
    lua_pop(L, 1); /* remove key */

    if (!node) {
        lua_pushnil(L);
        lua_pushnil(L);
    } else {
        struct luaitem *item = node->item;
        ms_lua_var_push(item->value);
        lua_pushinteger(L, avl_index(node));
    }

    return 2;
}/*}}}*/

static int tree_find_near(LuaState *L)/*{{{*/
{
    struct luatree *tree = ms_lua_checkclass(L, CLASS, 1);
    luaL_argcheck(L, lua_gettop(L) == 2, lua_gettop(L), "Wrong number of arguments (need 2)");

    lua_pushvalue(L, 2); /* put key at -1 */
    avl_node_t *node;
    int ret = avl_search_closest(tree->avltree, &node);
    lua_pop(L, 1); /* remove key */

    if (!node) {
        lua_pushnil(L);
        lua_pushnil(L);
        lua_pushnil(L);
    } else {
        struct luaitem *item = node->item;
        ms_lua_var_push(item->key);
        ms_lua_var_push(item->value);
        lua_pushinteger(L, avl_index(node));
    }
    lua_pushinteger(L, ret);

    return 4;
}/*}}}*/

static int tree_lookup_index(LuaState *L)/*{{{*/
{
    struct luatree *tree = ms_lua_checkclass(L, CLASS, 1);
    luaL_argcheck(L, lua_gettop(L) == 2, lua_gettop(L), "Wrong number of arguments (need 2)");
    int v = luaL_checkinteger(L, 2);

    avl_node_t *node = avl_at(tree->avltree, v);
    if (!node) {
        lua_pushnil(L);
        lua_pushnil(L);
    } else {
        struct luaitem *item = node->item;
        ms_lua_var_push(item->key);
        ms_lua_var_push(item->value);
    }
    return 2;
}/*}}}*/

static int tree_size(LuaState *L)/*{{{*/
{
    struct luatree *tree = ms_lua_checkclass(L, CLASS, 1);
    luaL_argcheck(L, lua_gettop(L) == 1, lua_gettop(L), "Wrong number of arguments (need 1)");

    lua_pushinteger(L, avl_count(tree->avltree));
    return 1;
}/*}}}*/

static const LuaLReg tree_methods[] = {/*{{{*/
    {"new", tree_new},
    {"insert", tree_insert},
    {"delete", tree_delete},
    {"clear", tree_clear},
    {"find",tree_find},
    {"find_near",tree_find_near},
    {"lookup_index",tree_lookup_index},
    {"size",tree_size},
    {0, 0}
};/*}}}*/

static const LuaLReg tree_meta[] = {/*{{{*/
    {"__gc", tree_gc},
    {"__tostring", tree_tostring},
    {0, 0}
};/*}}}*/

int luaopen_moonshine_tree(LuaState *L)/*{{{*/
{
    ms_lua_use_env(L);
    ms_lua_class_register(L, CLASS, tree_methods, tree_meta);
    return 1;
}/*}}}*/
