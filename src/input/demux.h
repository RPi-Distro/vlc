/*****************************************************************************
 * demux.h: Input demux functions
 *****************************************************************************
 * Copyright (C) 1998-2008 the VideoLAN team
 * Copyright (C) 2008 Laurent Aimar
 * $Id: 084185f590b516143ce81e2208248f5b7e838443 $
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

#ifndef _INPUT_DEMUX_H
#define _INPUT_DEMUX_H 1

#include <vlc_common.h>
#include <vlc_demux.h>

#include "stream.h"

/* stream_t *s could be null and then it mean a access+demux in one */
#define demux_New( a, b, c, d, e, f,g ) __demux_New(VLC_OBJECT(a),b,c,d,e,f,g)
demux_t *__demux_New( vlc_object_t *p_obj, const char *psz_access, const char *psz_demux, const char *psz_path, stream_t *s, es_out_t *out, bool );

void demux_Delete( demux_t * );

static inline int demux_Demux( demux_t *p_demux )
{
    return p_demux->pf_demux( p_demux );
}
static inline int demux_vaControl( demux_t *p_demux, int i_query, va_list args )
{
    return p_demux->pf_control( p_demux, i_query, args );
}
static inline int demux_Control( demux_t *p_demux, int i_query, ... )
{
    va_list args;
    int     i_result;

    va_start( args, i_query );
    i_result = demux_vaControl( p_demux, i_query, args );
    va_end( args );
    return i_result;
}

#endif

