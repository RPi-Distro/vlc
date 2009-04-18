/*****************************************************************************
 * mem_stream.c: stream_t wrapper around memory buffer
 *****************************************************************************
 * Copyright (C) 1999-2004 the VideoLAN team
 * $Id: fd38b113d6aad1752abfc5cf17a871c1edc4513a $
 *
 * Authors: Sigmund Augdal Helberg <dnumgis@videolan.org>
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>

#include "input_internal.h"

struct stream_sys_t
{
    bool  i_preserve_memory;
    int64_t     i_pos;      /* Current reading offset */
    int64_t     i_size;
    uint8_t    *p_buffer;

};

static int  Read   ( stream_t *, void *p_read, unsigned int i_read );
static int  Peek   ( stream_t *, const uint8_t **pp_peek, unsigned int i_read );
static int  Control( stream_t *, int i_query, va_list );
static void Delete ( stream_t * );

/**
 * Create a stream from a memory buffer
 *
 * \param p_this the calling vlc_object
 * \param p_buffer the memory buffer for the stream
 * \param i_buffer the size of the buffer
 * \param i_preserve_memory if this is set to false the memory buffer
 *        pointed to by p_buffer is freed on stream_Destroy
 */
stream_t *__stream_MemoryNew( vlc_object_t *p_this, uint8_t *p_buffer,
                              int64_t i_size, bool i_preserve_memory )
{
    stream_t *s = vlc_stream_create( p_this );
    stream_sys_t *p_sys;

    if( !s ) return NULL;

    s->p_sys = p_sys = malloc( sizeof( stream_sys_t ) );
    p_sys->i_pos = 0;
    p_sys->i_size = i_size;
    p_sys->p_buffer = p_buffer;
    p_sys->i_preserve_memory = i_preserve_memory;

    s->pf_read    = Read;
    s->pf_peek    = Peek;
    s->pf_control = Control;
    s->pf_destroy = Delete;

    s->i_char_width = 1;
    s->b_little_endian = false;
    vlc_object_attach( s, p_this );

    return s;
}

static void Delete( stream_t *s )
{
    if( !s->p_sys->i_preserve_memory ) free( s->p_sys->p_buffer );
    free( s->p_sys );
    vlc_object_detach( s );
    vlc_object_release( s );
}

/****************************************************************************
 * AStreamControl:
 ****************************************************************************/
static int Control( stream_t *s, int i_query, va_list args )
{
    stream_sys_t *p_sys = s->p_sys;

    bool *p_bool;
    int64_t    *pi_64, i_64;
    int        i_int;

    switch( i_query )
    {
        case STREAM_GET_SIZE:
            pi_64 = (int64_t*)va_arg( args, int64_t * );
            *pi_64 = p_sys->i_size;
            break;

        case STREAM_CAN_SEEK:
            p_bool = (bool*)va_arg( args, bool * );
            *p_bool = true;
            break;

        case STREAM_CAN_FASTSEEK:
            p_bool = (bool*)va_arg( args, bool * );
            *p_bool = true;
            break;

        case STREAM_GET_POSITION:
            pi_64 = (int64_t*)va_arg( args, int64_t * );
            *pi_64 = p_sys->i_pos;
            break;

        case STREAM_SET_POSITION:
            i_64 = (int64_t)va_arg( args, int64_t );
            i_64 = __MAX( i_64, 0 );
            i_64 = __MIN( i_64, s->p_sys->i_size );
            p_sys->i_pos = i_64;
            break;

        case STREAM_GET_MTU:
        case STREAM_GET_CONTENT_TYPE:
            return VLC_EGENERIC;

        case STREAM_CONTROL_ACCESS:
            i_int = (int) va_arg( args, int );
            msg_Err( s, "Hey, what are you thinking ?"
                     "DON'T USE STREAM_CONTROL_ACCESS !!!" );
            return VLC_EGENERIC;

        default:
            msg_Err( s, "invalid stream_vaControl query=0x%x", i_query );
            return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}

static int Read( stream_t *s, void *p_read, unsigned int i_read )
{
    stream_sys_t *p_sys = s->p_sys;
    int i_res = __MIN( i_read, p_sys->i_size - p_sys->i_pos );
    memcpy( p_read, p_sys->p_buffer + p_sys->i_pos, i_res );
    p_sys->i_pos += i_res;
    return i_res;
}

static int Peek( stream_t *s, const uint8_t **pp_peek, unsigned int i_read )
{
    stream_sys_t *p_sys = s->p_sys;
    int i_res = __MIN( i_read, p_sys->i_size - p_sys->i_pos );
    *pp_peek = p_sys->p_buffer + p_sys->i_pos;
    return i_res;
}
