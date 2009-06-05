/*****************************************************************************
 * misc.c
 *****************************************************************************
 * Copyright (C) 2007-2008 the VideoLAN team
 * $Id$
 *
 * Authors: Antoine Cellerier <dionoea at videolan tod org>
 *          Pierre d'Herbemont <pdherbemont # videolan.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#ifndef  _GNU_SOURCE
#   define  _GNU_SOURCE
#endif

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_meta.h>
#include <vlc_charset.h>
#include <vlc_aout.h>
#include <vlc_interface.h>

#include <lua.h>        /* Low level lua C API */
#include <lauxlib.h>    /* Higher level C API */
#include <lualib.h>     /* Lua libs */

#include "../vlc.h"
#include "../libs.h"

/*****************************************************************************
 * Internal lua<->vlc utils
 *****************************************************************************/
vlc_object_t * vlclua_get_this( lua_State *L )
{
    vlc_object_t * p_this;
    lua_getglobal( L, "vlc" );
    lua_getfield( L, -1, "private" );
    p_this = (vlc_object_t*)lua_topointer( L, lua_gettop( L ) );
    lua_pop( L, 2 );
    return p_this;
}

/*****************************************************************************
 * VLC error code translation
 *****************************************************************************/
int vlclua_push_ret( lua_State *L, int i_error )
{
    lua_pushnumber( L, i_error );
    lua_pushstring( L, vlc_error( i_error ) );
    return 2;
}

/*****************************************************************************
 * Get the VLC version string
 *****************************************************************************/
static int vlclua_version( lua_State *L )
{
    lua_pushstring( L, VLC_Version() );
    return 1;
}

/*****************************************************************************
 * Get the VLC copyright
 *****************************************************************************/
static int vlclua_copyright( lua_State *L )
{
    lua_pushstring( L, COPYRIGHT_MESSAGE );
    return 1;
}

/*****************************************************************************
 * Get the VLC license msg/disclaimer
 *****************************************************************************/
static int vlclua_license( lua_State *L )
{
    lua_pushstring( L, LICENSE_MSG );
    return 1;
}

/*****************************************************************************
 * Quit VLC
 *****************************************************************************/
static int vlclua_quit( lua_State *L )
{
    vlc_object_t *p_this = vlclua_get_this( L );
    /* The rc.c code also stops the playlist ... not sure if this is needed
     * though. */
    libvlc_Quit( p_this->p_libvlc );
    return 0;
}

/*****************************************************************************
 * Global properties getters
 *****************************************************************************/
static int vlclua_datadir( lua_State *L )
{
    lua_pushstring( L, config_GetDataDir() );
    return 1;
}

static int vlclua_userdatadir( lua_State *L )
{
    lua_pushstring( L, config_GetUserDataDir() );
    return 1;
}

static int vlclua_homedir( lua_State *L )
{
    lua_pushstring( L, config_GetHomeDir() );
    return 1;
}

static int vlclua_configdir( lua_State *L )
{
    char *dir = config_GetUserConfDir();
    lua_pushstring( L, dir );
    free( dir );
    return 1;
}

static int vlclua_cachedir( lua_State *L )
{
    char *dir = config_GetCacheDir();
    lua_pushstring( L, dir );
    free( dir );
    return 1;
}

static int vlclua_datadir_list( lua_State *L )
{
    const char *psz_dirname = luaL_checkstring( L, 1 );
    char  *ppsz_dir_list[] = { NULL, NULL, NULL, NULL };
    char **ppsz_dir = ppsz_dir_list;
    int i = 1;

    if( vlclua_dir_list( psz_dirname, ppsz_dir_list ) != VLC_SUCCESS )
        return 0;
    lua_newtable( L );
    for( ; *ppsz_dir; ppsz_dir++ )
    {
        lua_pushstring( L, *ppsz_dir );
        lua_rawseti( L, -2, i );
        i ++;
    }
    vlclua_dir_list_free( ppsz_dir_list );
    return 1;
}
/*****************************************************************************
 *
 *****************************************************************************/
static int vlclua_lock_and_wait( lua_State *L )
{
    intf_thread_t *p_intf = (intf_thread_t *)vlclua_get_this( L );
    intf_sys_t *p_sys = p_intf->p_sys;

    vlc_mutex_lock( &p_sys->lock );
    while( !p_sys->exiting )
        vlc_cond_wait( &p_sys->wait, &p_sys->lock );
    vlc_mutex_unlock( &p_sys->lock );
    lua_pushboolean( L, 1 );
    return 1;
}

static int vlclua_mdate( lua_State *L )
{
    lua_pushnumber( L, mdate() );
    return 1;
}

static int vlclua_mwait( lua_State *L )
{
    double f = luaL_checknumber( L, 1 );
    mwait( (int64_t)f );
    return 0;
}

static int vlclua_intf_should_die( lua_State *L )
{
    intf_thread_t *p_intf = (intf_thread_t*)vlclua_get_this( L );
    lua_pushboolean( L, p_intf->p_sys->exiting );
    return 1;
}

/*****************************************************************************
 *
 *****************************************************************************/
static const luaL_Reg vlclua_misc_reg[] = {
    { "version", vlclua_version },
    { "copyright", vlclua_copyright },
    { "license", vlclua_license },

    { "datadir", vlclua_datadir },
    { "userdatadir", vlclua_userdatadir },
    { "homedir", vlclua_homedir },
    { "configdir", vlclua_configdir },
    { "cachedir", vlclua_cachedir },
    { "datadir_list", vlclua_datadir_list },

    { "mdate", vlclua_mdate },
    { "mwait", vlclua_mwait },

    { "lock_and_wait", vlclua_lock_and_wait },

    { "should_die", vlclua_intf_should_die },
    { "quit", vlclua_quit },

    { NULL, NULL }
};

void luaopen_misc( lua_State *L )
{
    lua_newtable( L );
    luaL_register( L, NULL, vlclua_misc_reg );
    lua_setfield( L, -2, "misc" );
}
