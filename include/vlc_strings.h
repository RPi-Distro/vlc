/*****************************************************************************
 * vlc_strings.h: String functions
 *****************************************************************************
 * Copyright (C) 2006 the VideoLAN team
 * $Id: 6faae1b71e8075a3855b4f47b81e4e52687e701a $
 *
 * Authors: Antoine Cellerier <dionoea at videolan dot org>
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

#ifndef _VLC_STRINGS_H
#define _VLC_STRINGS_H 1

#include <vlc/vlc.h>

/**
 * \defgroup strings Strings
 * @{
 */

VLC_EXPORT( void, resolve_xml_special_chars, ( char *psz_value ) );
VLC_EXPORT( char *, convert_xml_special_chars, ( const char *psz_content ) );

/**
 * @}
 */

#endif
