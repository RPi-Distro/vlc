/*****************************************************************************
 * xa.c : xa file demux module for vlc
 *****************************************************************************
 * Copyright (C) 2005 Rémi Denis-Courmont
 * $Id: 850352543c303d5a2e547d953394294c64201e5a $
 *
 * Authors: Rémi Denis-Courmont <rem # videolan.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_demux.h>

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

vlc_module_begin ()
    set_description( N_("XA demuxer") )
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_DEMUX )
    set_capability( "demux", 10 )
    set_callbacks( Open, Close )
vlc_module_end ()

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int Demux  ( demux_t * );
static int Control( demux_t *, int i_query, va_list args );

struct demux_sys_t
{
    es_out_id_t     *p_es;

    int64_t         i_data_offset;
    unsigned int    i_data_size;
    unsigned int    i_block_frames;
    unsigned int    i_frame_size;
    unsigned int    i_bitrate;

    date_t          pts;
};

typedef struct xa_header_t
{
    char     xa_id[4];
    uint32_t iSize;

    uint16_t wFormatTag;
    uint16_t nChannels;
    uint32_t nSamplesPerSec;
    uint32_t nAvgBytesPerSec;
    uint16_t nBlockAlign;
    uint16_t wBitsPerSample;
} xa_header_t;

#define FRAME_LENGTH 28 /* samples per frame */

/*****************************************************************************
 * Open: check file and initializes structures
 *****************************************************************************/
static int Open( vlc_object_t * p_this )
{
    demux_t     *p_demux = (demux_t*)p_this;
    demux_sys_t *p_sys;
    xa_header_t p_xa;
    const uint8_t *p_buf;

    /* XA file heuristic */
    if( stream_Peek( p_demux->s, &p_buf, sizeof( p_xa ) )
            < (signed)sizeof( p_xa ) )
        return VLC_EGENERIC;

    memcpy( &p_xa, p_buf, sizeof( p_xa ) );
    if( ( strncmp( p_xa.xa_id, "XAI", 4 )
       && strncmp( p_xa.xa_id, "XAJ", 4 ) )
     || ( GetWLE( &p_xa.wFormatTag  ) != 0x0001)
     || ( GetWLE( &p_xa.wBitsPerSample ) != 16) )
        return VLC_EGENERIC;

    p_sys = malloc( sizeof( demux_sys_t ) );
    if( unlikely( p_sys == NULL ) )
        return VLC_ENOMEM;

    p_demux->pf_demux   = Demux;
    p_demux->pf_control = Control;
    p_demux->p_sys      = p_sys;
    p_sys->p_es         = NULL;

    /* skip XA header -- cannot fail */
    stream_Read( p_demux->s, NULL, sizeof( p_xa ) );

    es_format_t fmt;
    es_format_Init( &fmt, AUDIO_ES, VLC_FOURCC('X','A','J',0) );

    msg_Dbg( p_demux, "assuming EA ADPCM audio codec" );
    fmt.audio.i_rate = GetDWLE( &p_xa.nSamplesPerSec );
    fmt.audio.i_bytes_per_frame = 15 * GetWLE( &p_xa.nChannels );
    fmt.audio.i_frame_length = FRAME_LENGTH;

    fmt.audio.i_channels = GetWLE ( &p_xa.nChannels );
    fmt.audio.i_blockalign = fmt.audio.i_bytes_per_frame;
    fmt.audio.i_bitspersample = 16;
    fmt.i_bitrate = (fmt.audio.i_rate * fmt.audio.i_bytes_per_frame * 8)
                    / fmt.audio.i_frame_length;

    p_sys->i_data_offset = stream_Tell( p_demux->s );
    /* FIXME: better computation */
    p_sys->i_data_size = p_xa.iSize * 15 / 56;
    /* How many frames per block (1:1 is too CPU intensive) */
    p_sys->i_block_frames = fmt.audio.i_rate / (FRAME_LENGTH * 20) + 1;
    p_sys->i_frame_size = fmt.audio.i_bytes_per_frame;
    p_sys->i_bitrate = fmt.i_bitrate;

    msg_Dbg( p_demux, "fourcc: %4.4s, channels: %d, "
             "freq: %d Hz, bitrate: %dKo/s, blockalign: %d",
             (char *)&fmt.i_codec, fmt.audio.i_channels, fmt.audio.i_rate,
             fmt.i_bitrate / 8192, fmt.audio.i_blockalign );

    if( fmt.audio.i_rate == 0 || fmt.audio.i_channels == 0 )
    {
        free( p_sys );
        return VLC_EGENERIC;
    }

    p_sys->p_es = es_out_Add( p_demux->out, &fmt );

    date_Init( &p_sys->pts, fmt.audio.i_rate, 1 );
    date_Set( &p_sys->pts, VLC_TS_0 );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Demux: read packet and send them to decoders
 *****************************************************************************
 * Returns -1 in case of error, 0 in case of EOF, 1 otherwise
 *****************************************************************************/
static int Demux( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    block_t     *p_block;
    int64_t     i_offset;
    unsigned    i_frames = p_sys->i_block_frames;

    i_offset = stream_Tell( p_demux->s );

    if( p_sys->i_data_size > 0 &&
        i_offset >= p_sys->i_data_offset + p_sys->i_data_size )
    {
        /* EOF */
        return 0;
    }

    p_block = stream_Block( p_demux->s, p_sys->i_frame_size * i_frames );
    if( p_block == NULL )
    {
        msg_Warn( p_demux, "cannot read data" );
        return 0;
    }

    i_frames = p_block->i_buffer / p_sys->i_frame_size;
    p_block->i_dts = p_block->i_pts = date_Get( &p_sys->pts );
    es_out_Control( p_demux->out, ES_OUT_SET_PCR, p_block->i_pts );
    es_out_Send( p_demux->out, p_sys->p_es, p_block );

    date_Increment( &p_sys->pts, i_frames * FRAME_LENGTH );

    return 1;
}

/*****************************************************************************
 * Close: frees unused data
 *****************************************************************************/
static void Close ( vlc_object_t * p_this )
{
    demux_sys_t *p_sys  = ((demux_t *)p_this)->p_sys;

    free( p_sys );
}

/*****************************************************************************
 * Control:
 *****************************************************************************/
static int Control( demux_t *p_demux, int i_query, va_list args )
{
    demux_sys_t *p_sys  = p_demux->p_sys;

    return demux_vaControlHelper( p_demux->s, p_sys->i_data_offset,
                                   p_sys->i_data_size ? p_sys->i_data_offset
                                   + p_sys->i_data_size : -1,
                                   p_sys->i_bitrate, p_sys->i_frame_size,
                                   i_query, args );
}
