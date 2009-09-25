/*****************************************************************************
 * configuration.c: Generic lua<->vlc config interface
 *****************************************************************************
 * Copyright (C) 2007-2008 the VideoLAN team
 * $Id: 77a69c6d7328e1cb6eeb2e0ae7eb984844a0f31f $
 *
 * Authors: Antoine Cellerier <dionoea at videolan tod org>
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

#include <lua.h>        /* Low level lua C API */
#include <lauxlib.h>    /* Higher level C API */

#include "../vlc.h"
#include "../libs.h"

/*****************************************************************************
 * Config handling
 *****************************************************************************/
static int vlclua_config_get( lua_State *L )
{
    vlc_object_t * p_this = vlclua_get_this( L );
    const char *psz_name;
    psz_name = luaL_checkstring( L, 1 );
    switch( config_GetType( p_this, psz_name ) )
    {
        case VLC_VAR_MODULE:
        case VLC_VAR_STRING:
        case VLC_VAR_FILE:
        case VLC_VAR_DIRECTORY:
        {
            char *psz = config_GetPsz( p_this, psz_name );
            lua_pushstring( L, psz );
            free( psz );
            break;
        }

        case VLC_VAR_INTEGER:
            lua_pushinteger( L, config_GetInt( p_this, psz_name ) );
            break;

        case VLC_VAR_BOOL:
            lua_pushboolean( L, config_GetInt( p_this, psz_name ) );
            break;

        case VLC_VAR_FLOAT:
            lua_pushnumber( L, config_GetFloat( p_this, psz_name ) );
            break;

        default:
            return vlclua_error( L );

    }
    return 1;
}

static int vlclua_config_set( lua_State *L )
{
    vlc_object_t *p_this = vlclua_get_this( L );
    const char *psz_name;
    psz_name = luaL_checkstring( L, 1 );
    switch( config_GetType( p_this, psz_name ) )
    {
        case VLC_VAR_MODULE:
        case VLC_VAR_STRING:
        case VLC_VAR_FILE:
        case VLC_VAR_DIRECTORY:
            config_PutPsz( p_this, psz_name, luaL_checkstring( L, 2 ) );
            break;

        case VLC_VAR_INTEGER:
            config_PutInt( p_this, psz_name, luaL_checkint( L, 2 ) );
            break;

        case VLC_VAR_BOOL:
            config_PutInt( p_this, psz_name, luaL_checkboolean( L, 2 ) );
            break;

        case VLC_VAR_FLOAT:
            config_PutFloat( p_this, psz_name,
                             luaL_checknumber( L, 2 ) );
            break;

        default:
            return vlclua_error( L );
    }
    return 0;
}

/*****************************************************************************
 *
 *****************************************************************************/
static const luaL_Reg vlclua_config_reg[] = {
    { "get", vlclua_config_get },
    { "set", vlclua_config_set },
    { NULL, NULL }
};

void luaopen_config( lua_State *L )
{
    lua_newtable( L );
    luaL_register( L, NULL, vlclua_config_reg );
    lua_setfield( L, -2, "config" );
}
