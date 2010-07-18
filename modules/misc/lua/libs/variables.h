/*****************************************************************************
 * variables.h: Generic lua<->vlc variables interface
 *****************************************************************************
 * Copyright (C) 2007-2008 the VideoLAN team
 * $Id: 1155d7e745eb2fb86086a3a71f089e7fde8bcf30 $
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

#ifndef VLC_LUA_VARIABLES_H
#define VLC_LUA_VARIABLES_H

int vlclua_pushvalue( lua_State *L, int i_type, vlc_value_t val ); /* internal use only */

#define vlclua_var_toggle_or_set(a,b,c) \
    __vlclua_var_toggle_or_set(a,VLC_OBJECT(b),c)
int __vlclua_var_toggle_or_set( lua_State *, vlc_object_t *, const char * );

#endif

