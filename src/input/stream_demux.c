/*****************************************************************************
 * stream_demux.c
 *****************************************************************************
 * Copyright (C) 1999-2008 the VideoLAN team
 * $Id: fe96fdf567fd5f7636a51fbf36df6dfeace4e7f3 $
 *
 * Author: Laurent Aimar <fenrir _AT_ videolan _DOT_ org>
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
#include <limits.h>

#include "demux.h"
#include <libvlc.h>
#include <vlc_codec.h>

/****************************************************************************
 * stream_Demux*: create a demuxer for an outpout stream (allow demuxer chain)
 ****************************************************************************/
struct stream_sys_t
{
    /* Data buffer */
    block_fifo_t *p_fifo;
    block_t      *p_block;

    uint64_t    i_pos;

    /* Demuxer */
    char        *psz_name;
    es_out_t    *out;
    demux_t     *p_demux;

};

static int  DStreamRead   ( stream_t *, void *p_read, unsigned int i_read );
static int  DStreamPeek   ( stream_t *, const uint8_t **pp_peek, unsigned int i_peek );
static int  DStreamControl( stream_t *, int i_query, va_list );
static void DStreamDelete ( stream_t * );
static void* DStreamThread ( vlc_object_t * );


stream_t *stream_DemuxNew( demux_t *p_demux, const char *psz_demux, es_out_t *out )
{
    vlc_object_t *p_obj = VLC_OBJECT(p_demux);
    /* We create a stream reader, and launch a thread */
    stream_t     *s;
    stream_sys_t *p_sys;

    s = stream_CommonNew( p_obj );
    if( s == NULL )
        return NULL;
    s->p_input = p_demux->p_input;
    s->psz_path  = strdup(""); /* N/A */
    s->pf_read   = DStreamRead;
    s->pf_peek   = DStreamPeek;
    s->pf_control= DStreamControl;
    s->pf_destroy= DStreamDelete;

    s->p_sys = p_sys = malloc( sizeof( *p_sys) );
    if( !s->psz_path || !s->p_sys )
    {
        stream_CommonDelete( s );
        return NULL;
    }

    p_sys->i_pos = 0;
    p_sys->out = out;
    p_sys->p_demux = NULL;
    p_sys->p_block = NULL;
    p_sys->psz_name = strdup( psz_demux );

    /* decoder fifo */
    if( ( p_sys->p_fifo = block_FifoNew() ) == NULL )
    {
        stream_CommonDelete( s );
        free( p_sys->psz_name );
        free( p_sys );
        return NULL;
    }

    vlc_object_attach( s, p_obj );

    if( vlc_thread_create( s, "stream out", DStreamThread,
                           VLC_THREAD_PRIORITY_INPUT ) )
    {
        stream_CommonDelete( s );
        free( p_sys->psz_name );
        free( p_sys );
        return NULL;
    }

    return s;
}

void stream_DemuxSend( stream_t *s, block_t *p_block )
{
    stream_sys_t *p_sys = s->p_sys;
    if( p_block )
        block_FifoPut( p_sys->p_fifo, p_block );
}

static void DStreamDelete( stream_t *s )
{
    stream_sys_t *p_sys = s->p_sys;
    block_t *p_empty;

    vlc_object_kill( s );
    if( p_sys->p_demux )
        vlc_object_kill( p_sys->p_demux );
    p_empty = block_New( s, 1 ); p_empty->i_buffer = 0;
    block_FifoPut( p_sys->p_fifo, p_empty );
    vlc_thread_join( s );

    if( p_sys->p_demux )
        demux_Delete( p_sys->p_demux );
    if( p_sys->p_block )
        block_Release( p_sys->p_block );

    block_FifoRelease( p_sys->p_fifo );
    free( p_sys->psz_name );
    free( p_sys );
    stream_CommonDelete( s );
}


static int DStreamRead( stream_t *s, void *p_read, unsigned int i_read )
{
    stream_sys_t *p_sys = s->p_sys;
    uint8_t *p_out = p_read;
    int i_out = 0;

    //msg_Dbg( s, "DStreamRead: wanted %d bytes", i_read );

    while( !s->b_die && !s->b_error && i_read )
    {
        block_t *p_block = p_sys->p_block;
        int i_copy;

        if( !p_block )
        {
            p_block = block_FifoGet( p_sys->p_fifo );
            if( !p_block ) s->b_error = 1;
            p_sys->p_block = p_block;
        }

        if( p_block && i_read )
        {
            i_copy = __MIN( i_read, p_block->i_buffer );
            if( p_out && i_copy ) memcpy( p_out, p_block->p_buffer, i_copy );
            i_read -= i_copy;
            p_out += i_copy;
            i_out += i_copy;
            p_block->i_buffer -= i_copy;
            p_block->p_buffer += i_copy;

            if( !p_block->i_buffer )
            {
                block_Release( p_block );
                p_sys->p_block = NULL;
            }
        }
    }

    p_sys->i_pos += i_out;
    return i_out;
}

static int DStreamPeek( stream_t *s, const uint8_t **pp_peek, unsigned int i_peek )
{
    stream_sys_t *p_sys = s->p_sys;
    block_t **pp_block = &p_sys->p_block;
    int i_out = 0;
    *pp_peek = 0;

    //msg_Dbg( s, "DStreamPeek: wanted %d bytes", i_peek );

    while( !s->b_die && !s->b_error && i_peek )
    {
        int i_copy;

        if( !*pp_block )
        {
            *pp_block = block_FifoGet( p_sys->p_fifo );
            if( !*pp_block ) s->b_error = 1;
        }

        if( *pp_block && i_peek )
        {
            i_copy = __MIN( i_peek, (*pp_block)->i_buffer );
            i_peek -= i_copy;
            i_out += i_copy;

            if( i_peek ) pp_block = &(*pp_block)->p_next;
        }
    }

    if( p_sys->p_block )
    {
        p_sys->p_block = block_ChainGather( p_sys->p_block );
        *pp_peek = p_sys->p_block->p_buffer;
    }

    return i_out;
}

static int DStreamControl( stream_t *s, int i_query, va_list args )
{
    stream_sys_t *p_sys = s->p_sys;
    uint64_t    *p_i64;
    bool *p_b;

    switch( i_query )
    {
        case STREAM_GET_SIZE:
            p_i64 = va_arg( args, uint64_t * );
            *p_i64 = 0;
            return VLC_SUCCESS;

        case STREAM_CAN_SEEK:
            p_b = (bool*) va_arg( args, bool * );
            *p_b = false;
            return VLC_SUCCESS;

        case STREAM_CAN_FASTSEEK:
            p_b = (bool*) va_arg( args, bool * );
            *p_b = false;
            return VLC_SUCCESS;

        case STREAM_GET_POSITION:
            p_i64 = va_arg( args, uint64_t * );
            *p_i64 = p_sys->i_pos;
            return VLC_SUCCESS;

        case STREAM_SET_POSITION:
        {
            uint64_t i64 = va_arg( args, uint64_t );
            if( i64 < p_sys->i_pos )
                return VLC_EGENERIC;

            uint64_t i_skip = i64 - p_sys->i_pos;
            while( i_skip > 0 )
            {
                int i_read = DStreamRead( s, NULL, __MIN(i_skip, INT_MAX) );
                if( i_read <= 0 )
                    return VLC_EGENERIC;
                i_skip -= i_read;
            }
            return VLC_SUCCESS;
        }

        case STREAM_CONTROL_ACCESS:
        case STREAM_GET_CONTENT_TYPE:
        case STREAM_SET_RECORD_STATE:
            return VLC_EGENERIC;

        default:
            msg_Err( s, "invalid DStreamControl query=0x%x", i_query );
            return VLC_EGENERIC;
    }
}

static void* DStreamThread( vlc_object_t* p_this )
{
    stream_t *s = (stream_t *)p_this;
    stream_sys_t *p_sys = s->p_sys;
    demux_t *p_demux;
    int canc = vlc_savecancel();

    /* Create the demuxer */
    if( !(p_demux = demux_New( s, s->p_input, "", p_sys->psz_name, "", s, p_sys->out,
                               false )) )
    {
        return NULL;
    }

    /* stream_Demux cannot apply DVB filters.
     * Get all programs and let the E/S output sort them out. */
    demux_Control( p_demux, DEMUX_SET_GROUP, -1, NULL );
    p_sys->p_demux = p_demux;

    /* Main loop */
    while( !s->b_die && !p_demux->b_die )
    {
        if( demux_Demux( p_demux ) <= 0 ) break;
    }

    vlc_restorecancel( canc );
    vlc_object_kill( p_demux );
    return NULL;
}

