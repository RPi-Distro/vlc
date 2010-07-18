/*****************************************************************************
 * intf_eject.h: CD/DVD-ROM ejection handling functions
 *****************************************************************************
 * Copyright (C) 2001, 2002 the VideoLAN team
 * $Id: d644e609fcd79fc61de0b75fb67105da94accfff $
 *
 * Author: Julien Blache <jb@technologeek.org>
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

#define intf_Eject(a,b) __intf_Eject(VLC_OBJECT(a),b)
VLC_EXPORT( int, __intf_Eject, ( vlc_object_t *, const char * ) );

