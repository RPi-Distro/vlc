/*****************************************************************************
 * objects.c: Generic lua<->vlc object wrapper
 *****************************************************************************
 * Copyright (C) 2007-2008 the VideoLAN team
 * $Id: cc91faba0cbd69d0f6049cc88140c70e0fcccac5 $
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

#ifndef VLC_LUA_OBJECTS_H
#define VLC_LUA_OBJECTS_H

int __vlclua_push_vlc_object( lua_State *L, vlc_object_t *p_obj,
                              lua_CFunction pf_gc );
#define vlclua_push_vlc_object( a, b, c ) \
        __vlclua_push_vlc_object( a, VLC_OBJECT( b ), c )
int vlclua_gc_release( lua_State *L );

#endif

