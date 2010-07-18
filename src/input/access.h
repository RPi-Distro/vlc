/*****************************************************************************
 * access.h: Input access functions
 *****************************************************************************
 * Copyright (C) 1998-2008 the VideoLAN team
 * Copyright (C) 2008 Laurent Aimar
 * $Id: 0e8739b5eaa5dd790e4777caa16d2323a3ecf1b2 $
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
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

#if defined(__PLUGIN__) || defined(__BUILTIN__) || !defined(__LIBVLC__)
# error This header file can only be included from LibVLC.
#endif

#ifndef _INPUT_ACCESS_H
#define _INPUT_ACCESS_H 1

#include <vlc_common.h>
#include <vlc_access.h>

#define access_New( a, b, c, d, e ) __access_New(VLC_OBJECT(a), b, c, d, e )
access_t * __access_New( vlc_object_t *p_obj, input_thread_t *p_input,
                         const char *psz_access, const char *psz_demux,
                         const char *psz_path );
void access_Delete( access_t * );

#endif

