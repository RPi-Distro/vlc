/*****************************************************************************
 * libs.h: VLC Lua wrapper libraries
 *****************************************************************************
 * Copyright (C) 2008 the VideoLAN team
 * $Id: 8bca89c41d6f6bd8dff382567a14d51c061c10c1 $
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

#ifndef VLC_LUA_LIBS_H
#define VLC_LUA_LIBS_H

void luaopen_acl( lua_State * );
void luaopen_config( lua_State * );
void luaopen_dialog( lua_State *, void * );
void luaopen_httpd( lua_State * );
void luaopen_input( lua_State * );
void luaopen_msg( lua_State * );
void luaopen_misc( lua_State * );
void luaopen_net( lua_State * );
void luaopen_object( lua_State * );
void luaopen_osd( lua_State * );
void luaopen_playlist( lua_State * );
void luaopen_sd( lua_State * );
void luaopen_stream( lua_State * );
void luaopen_strings( lua_State * );
void luaopen_variables( lua_State * );
void luaopen_video( lua_State * );
void luaopen_vlm( lua_State * );
void luaopen_volume( lua_State * );
void luaopen_gettext( lua_State * );
void luaopen_input_item( lua_State *L, input_item_t *item );
void luaopen_xml( lua_State *L );
void luaopen_md5( lua_State *L );

#endif
