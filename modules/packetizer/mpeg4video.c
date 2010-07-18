/*****************************************************************************
 * mpeg4video.c: mpeg 4 video packetizer
 *****************************************************************************
 * Copyright (C) 2001-2006 the VideoLAN team
 * $Id: fdffce8b2ca28ce831e6eb5ce46adb41d04ee441 $
 *
 * Authors: Gildas Bazin <gbazin@videolan.org>
 *          Laurent Aimar <fenrir@via.ecp.fr>
 *          Eric Petit <titer@videolan.org>
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <stdlib.h>                                      /* malloc(), free() */

#include <vlc/vlc.h>
#include <vlc/decoder.h>
#include <vlc/sout.h>
#include <vlc/input.h>                  /* hmmm, just for INPUT_RATE_DEFAULT */

#include "vlc_bits.h"
#include "vlc_block_helper.h"

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

vlc_module_begin();
    set_category( CAT_SOUT );
    set_subcategory( SUBCAT_SOUT_PACKETIZER );
    set_description( _("MPEG4 video packetizer") );
    set_capability( "packetizer", 50 );
    set_callbacks( Open, Close );
vlc_module_end();

/****************************************************************************
 * Local prototypes
 ****************************************************************************/
static block_t *Packetize( decoder_t *, block_t ** );

struct decoder_sys_t
{
    /*
     * Input properties
     */
    block_bytestream_t bytestream;
    int i_state;
    int i_offset;
    uint8_t p_startcode[3];

    /*
     * Common properties
     */
    mtime_t i_interpolated_pts;
    mtime_t i_interpolated_dts;
    mtime_t i_last_ref_pts;
    mtime_t i_last_time_ref;
    mtime_t i_time_ref;
    mtime_t i_last_time;
    mtime_t i_last_timeincr;

    unsigned int i_flags;

    int         i_fps_num;
    int         i_fps_den;
    int         i_last_incr;
    int         i_last_incr_diff;

    vlc_bool_t  b_frame;

    /* Current frame being built */
    block_t    *p_frame;
    block_t    **pp_last;
};

enum {
    STATE_NOSYNC,
    STATE_NEXT_SYNC
};

static block_t *ParseMPEGBlock( decoder_t *, block_t * );
static int ParseVOL( decoder_t *, es_format_t *, uint8_t *, int );
static int ParseVOP( decoder_t *, block_t * );
static int vlc_log2( unsigned int );

#define VIDEO_OBJECT_MASK                       0x01f
#define VIDEO_OBJECT_LAYER_MASK                 0x00f

#define VIDEO_OBJECT_START_CODE                 0x100
#define VIDEO_OBJECT_LAYER_START_CODE           0x120
#define VISUAL_OBJECT_SEQUENCE_START_CODE       0x1b0
#define VISUAL_OBJECT_SEQUENCE_END_CODE         0x1b1
#define USER_DATA_START_CODE                    0x1b2
#define GROUP_OF_VOP_START_CODE                 0x1b3
#define VIDEO_SESSION_ERROR_CODE                0x1b4
#define VISUAL_OBJECT_START_CODE                0x1b5
#define VOP_START_CODE                          0x1b6
#define FACE_OBJECT_START_CODE                  0x1ba
#define FACE_OBJECT_PLANE_START_CODE            0x1bb
#define MESH_OBJECT_START_CODE                  0x1bc
#define MESH_OBJECT_PLANE_START_CODE            0x1bd
#define STILL_TEXTURE_OBJECT_START_CODE         0x1be
#define TEXTURE_SPATIAL_LAYER_START_CODE        0x1bf
#define TEXTURE_SNR_LAYER_START_CODE            0x1c0

/*****************************************************************************
 * Open: probe the packetizer and return score
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    decoder_t     *p_dec = (decoder_t*)p_this;
    decoder_sys_t *p_sys;

    switch( p_dec->fmt_in.i_codec )
    {
        case VLC_FOURCC( 'm', '4', 's', '2'):
        case VLC_FOURCC( 'M', '4', 'S', '2'):
        case VLC_FOURCC( 'm', 'p', '4', 's'):
        case VLC_FOURCC( 'M', 'P', '4', 'S'):
        case VLC_FOURCC( 'm', 'p', '4', 'v'):
        case VLC_FOURCC( 'M', 'P', '4', 'V'):
        case VLC_FOURCC( 'D', 'I', 'V', 'X'):
        case VLC_FOURCC( 'd', 'i', 'v', 'x'):
        case VLC_FOURCC( 'X', 'V', 'I', 'D'):
        case VLC_FOURCC( 'X', 'v', 'i', 'D'):
        case VLC_FOURCC( 'x', 'v', 'i', 'd'):
        case VLC_FOURCC( 'D', 'X', '5', '0'):
        case VLC_FOURCC( 'd', 'x', '5', '0'):
        case VLC_FOURCC( 0x04, 0,   0,   0):
        case VLC_FOURCC( '3', 'I', 'V', '2'):
        case VLC_FOURCC( 'm', '4', 'c', 'c'):
        case VLC_FOURCC( 'M', '4', 'C', 'C'):
            break;

        default:
            return VLC_EGENERIC;
    }

    /* Allocate the memory needed to store the decoder's structure */
    if( ( p_dec->p_sys = p_sys = malloc( sizeof(decoder_sys_t) ) ) == NULL )
    {
        msg_Err( p_dec, "out of memory" );
        return VLC_EGENERIC;
    }
    memset( p_sys, 0, sizeof(decoder_sys_t) );

    /* Misc init */
    p_sys->i_state = STATE_NOSYNC;
    p_sys->bytestream = block_BytestreamInit( p_dec );
    p_sys->p_startcode[0] = 0;
    p_sys->p_startcode[1] = 0;
    p_sys->p_startcode[2] = 1;
    p_sys->i_offset = 0;
    p_sys->p_frame = NULL;
    p_sys->pp_last = &p_sys->p_frame;

    /* Setup properties */
    es_format_Copy( &p_dec->fmt_out, &p_dec->fmt_in );
    p_dec->fmt_out.i_codec = VLC_FOURCC( 'm', 'p', '4', 'v' );

    if( p_dec->fmt_in.i_extra )
    {
        /* We have a vol */
        p_dec->fmt_out.i_extra = p_dec->fmt_in.i_extra;
        p_dec->fmt_out.p_extra = malloc( p_dec->fmt_in.i_extra );
        memcpy( p_dec->fmt_out.p_extra, p_dec->fmt_in.p_extra,
                p_dec->fmt_in.i_extra );

        msg_Dbg( p_dec, "opening with vol size: %d", p_dec->fmt_in.i_extra );
        ParseVOL( p_dec, &p_dec->fmt_out,
                  p_dec->fmt_out.p_extra, p_dec->fmt_out.i_extra );
    }
    else
    {
        /* No vol, we'll have to look for one later on */
        p_dec->fmt_out.i_extra = 0;
        p_dec->fmt_out.p_extra = 0;
    }

    /* Set callback */
    p_dec->pf_packetize = Packetize;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: clean up the packetizer
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t*)p_this;

    block_BytestreamRelease( &p_dec->p_sys->bytestream );
    if( p_dec->p_sys->p_frame ) block_ChainRelease( p_dec->p_sys->p_frame );
    free( p_dec->p_sys );
}

/****************************************************************************
 * Packetize: the whole thing
 ****************************************************************************/
static block_t *Packetize( decoder_t *p_dec, block_t **pp_block )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    block_t       *p_pic;
    mtime_t       i_pts, i_dts;

    if( pp_block == NULL || *pp_block == NULL ) return NULL;

    if( (*pp_block)->i_flags & BLOCK_FLAG_DISCONTINUITY )
    {
        p_sys->i_state = STATE_NOSYNC;
        if( p_sys->p_frame ) block_ChainRelease( p_sys->p_frame );
        p_sys->p_frame = NULL;
        p_sys->pp_last = &p_sys->p_frame;
        block_Release( *pp_block );
        return NULL;
    }

    block_BytestreamPush( &p_sys->bytestream, *pp_block );

    while( 1 )
    {
        switch( p_sys->i_state )
        {

        case STATE_NOSYNC:
            if( block_FindStartcodeFromOffset( &p_sys->bytestream,
                    &p_sys->i_offset, p_sys->p_startcode, 3 ) == VLC_SUCCESS )
            {
                p_sys->i_state = STATE_NEXT_SYNC;
            }

            if( p_sys->i_offset )
            {
                block_SkipBytes( &p_sys->bytestream, p_sys->i_offset );
                p_sys->i_offset = 0;
                block_BytestreamFlush( &p_sys->bytestream );
            }

            if( p_sys->i_state != STATE_NEXT_SYNC )
            {
                /* Need more data */
                return NULL;
            }

            p_sys->i_offset = 1; /* To find next startcode */

        case STATE_NEXT_SYNC:
            /* TODO: If p_block == NULL, flush the buffer without checking the
             * next sync word */

            /* Find the next startcode */
            if( block_FindStartcodeFromOffset( &p_sys->bytestream,
                    &p_sys->i_offset, p_sys->p_startcode, 3 ) != VLC_SUCCESS )
            {
                /* Need more data */
                return NULL;
            }

            /* Get the new fragment and set the pts/dts */
            p_pic = block_New( p_dec, p_sys->i_offset );
            block_BytestreamFlush( &p_sys->bytestream );
            p_pic->i_pts = i_pts = p_sys->bytestream.p_block->i_pts;
            p_pic->i_dts = i_dts = p_sys->bytestream.p_block->i_dts;
            p_pic->i_rate = p_sys->bytestream.p_block->i_rate;

            block_GetBytes( &p_sys->bytestream, p_pic->p_buffer,
                            p_pic->i_buffer );

            p_sys->i_offset = 0;

            /* Get picture if any */
            if( !( p_pic = ParseMPEGBlock( p_dec, p_pic ) ) )
            {
                p_sys->i_state = STATE_NOSYNC;
                break;
            }

            /* don't reuse the same timestamps several times */
            if( i_pts == p_sys->bytestream.p_block->i_pts &&
                i_dts == p_sys->bytestream.p_block->i_dts )
            {
                p_sys->bytestream.p_block->i_pts = 0;
                p_sys->bytestream.p_block->i_dts = 0;
            }

            /* We've just started the stream, wait for the first PTS.
             * We discard here so we can still get the sequence header. */
            if( p_sys->i_interpolated_pts <= 0 &&
                p_sys->i_interpolated_dts <= 0 )
            {
                msg_Dbg( p_dec, "need a starting pts/dts" );
                p_sys->i_state = STATE_NOSYNC;
                block_Release( p_pic );
                break;
            }

            /* When starting the stream we can have the first frame with
             * a null DTS (i_interpolated_pts is initialized to 0) */
            if( !p_pic->i_dts ) p_pic->i_dts = p_pic->i_pts;

            /* So p_block doesn't get re-added several times */
            *pp_block = block_BytestreamPop( &p_sys->bytestream );

            p_sys->i_state = STATE_NOSYNC;

            return p_pic;
        }
    }
}

/*****************************************************************************
 * ParseMPEGBlock: Re-assemble fragments into a block containing a picture
 *****************************************************************************/
static block_t *ParseMPEGBlock( decoder_t *p_dec, block_t *p_frag )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    block_t *p_pic = NULL;

    if( p_frag->p_buffer[3] == 0xB0 || p_frag->p_buffer[3] == 0xB1 )
    {
#if 0
        /* Remove VOS start/end code from the original stream */
        block_Release( p_frag );
#else
        /* Append the block for now since ts/ps muxers rely on VOL
         * being present in the stream */
        block_ChainLastAppend( &p_sys->pp_last, p_frag );
#endif
        return NULL;
    }
    if( p_frag->p_buffer[3] >= 0x20 && p_frag->p_buffer[3] <= 0x2f )
    {
        /* Copy the complete VOL */
        if( p_dec->fmt_out.i_extra != p_frag->i_buffer )
        {
            p_dec->fmt_out.p_extra =
                realloc( p_dec->fmt_out.p_extra, p_frag->i_buffer );
            p_dec->fmt_out.i_extra = p_frag->i_buffer;
        }
        memcpy( p_dec->fmt_out.p_extra, p_frag->p_buffer, p_frag->i_buffer );
        ParseVOL( p_dec, &p_dec->fmt_out,
                  p_dec->fmt_out.p_extra, p_dec->fmt_out.i_extra );

#if 0
        /* Remove from the original stream */
        block_Release( p_frag );
#else
        /* Append the block for now since ts/ps muxers rely on VOL
         * being present in the stream */
        block_ChainLastAppend( &p_sys->pp_last, p_frag );
#endif
        return NULL;
    }
    else
    {
        if( !p_dec->fmt_out.i_extra )
        {
            msg_Warn( p_dec, "waiting for VOL" );
            block_Release( p_frag );
            return NULL;
        }

        /* Append the block */
        block_ChainLastAppend( &p_sys->pp_last, p_frag );
    }

    if( p_frag->p_buffer[3] == 0xb6 &&
        ParseVOP( p_dec, p_frag ) == VLC_SUCCESS )
    {
        /* We are dealing with a VOP */
        p_pic = block_ChainGather( p_sys->p_frame );
        p_pic->i_pts = p_sys->i_interpolated_pts;
        p_pic->i_dts = p_sys->i_interpolated_dts;

        /* Reset context */
        p_sys->p_frame = NULL;
        p_sys->pp_last = &p_sys->p_frame;
    }

    return p_pic;
}

/* ParseVOL:
 *  TODO:
 *      - support aspect ratio
 */
static int ParseVOL( decoder_t *p_dec, es_format_t *fmt,
                     uint8_t *p_vol, int i_vol )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    int i_vo_type, i_vo_ver_id, i_ar, i_shape;
    bs_t s;

    for( ;; )
    {
        if( p_vol[0] == 0x00 && p_vol[1] == 0x00 && p_vol[2] == 0x01 &&
            p_vol[3] >= 0x20 && p_vol[3] <= 0x2f ) break;

        p_vol++; i_vol--;
        if( i_vol <= 4 ) return VLC_EGENERIC;
    }

    bs_init( &s, &p_vol[4], i_vol - 4 );

    bs_skip( &s, 1 );   /* random access */
    i_vo_type = bs_read( &s, 8 );
    if( bs_read1( &s ) )
    {
        i_vo_ver_id = bs_read( &s, 4 );
        bs_skip( &s, 3 );
    }
    else
    {
        i_vo_ver_id = 1;
    }
    i_ar = bs_read( &s, 4 );
    if( i_ar == 0xf )
    {
        int i_ar_width, i_ar_height;

        i_ar_width = bs_read( &s, 8 );
        i_ar_height= bs_read( &s, 8 );
    }
    if( bs_read1( &s ) )
    {
        int i_chroma_format;
        int i_low_delay;

        /* vol control parameter */
        i_chroma_format = bs_read( &s, 2 );
        i_low_delay = bs_read1( &s );

        if( bs_read1( &s ) )
        {
            bs_skip( &s, 16 );
            bs_skip( &s, 16 );
            bs_skip( &s, 16 );
            bs_skip( &s, 3 );
            bs_skip( &s, 11 );
            bs_skip( &s, 1 );
            bs_skip( &s, 16 );
        }
    }
    /* shape 0->RECT, 1->BIN, 2->BIN_ONLY, 3->GRAY */
    i_shape = bs_read( &s, 2 );
    if( i_shape == 3 && i_vo_ver_id != 1 )
    {
        bs_skip( &s, 4 );
    }

    if( !bs_read1( &s ) ) return VLC_EGENERIC; /* Marker */

    p_sys->i_fps_num = bs_read( &s, 16 ); /* Time increment resolution*/
    if( !p_sys->i_fps_num ) p_sys->i_fps_num = 1;

    if( !bs_read1( &s ) ) return VLC_EGENERIC; /* Marker */

    if( bs_read1( &s ) )
    {
        int i_time_increment_bits = vlc_log2( p_sys->i_fps_num - 1 ) + 1;

        if( i_time_increment_bits < 1 ) i_time_increment_bits = 1;

        p_sys->i_fps_den = bs_read( &s, i_time_increment_bits );
    }
    if( i_shape == 0 )
    {
        bs_skip( &s, 1 );
        fmt->video.i_width = bs_read( &s, 13 );
        bs_skip( &s, 1 );
        fmt->video.i_height= bs_read( &s, 13 );
        bs_skip( &s, 1 );
    }

    return VLC_SUCCESS;
}

static int ParseVOP( decoder_t *p_dec, block_t *p_vop )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    int64_t i_time_increment, i_time_ref;
    int i_modulo_time_base = 0, i_time_increment_bits;
    bs_t s;

    bs_init( &s, &p_vop->p_buffer[4], p_vop->i_buffer - 4 );

    switch( bs_read( &s, 2 ) )
    {
    case 0:
        p_sys->i_flags = BLOCK_FLAG_TYPE_I;
        break;
    case 1:
        p_sys->i_flags = BLOCK_FLAG_TYPE_P;
        break;
    case 2:
        p_sys->i_flags = BLOCK_FLAG_TYPE_B;
        p_sys->b_frame = VLC_TRUE;
        break;
    case 3: /* gni ? */
        p_sys->i_flags = BLOCK_FLAG_TYPE_PB;
        break;
    }

    while( bs_read( &s, 1 ) ) i_modulo_time_base++;
    if( !bs_read1( &s ) ) return VLC_EGENERIC; /* Marker */

    /* VOP time increment */
    i_time_increment_bits = vlc_log2(p_dec->p_sys->i_fps_num - 1) + 1;
    if( i_time_increment_bits < 1 ) i_time_increment_bits = 1;
    i_time_increment = bs_read( &s, i_time_increment_bits );

    /* Interpolate PTS/DTS */
    if( !(p_sys->i_flags & BLOCK_FLAG_TYPE_B) )
    {
        p_sys->i_last_time_ref = p_sys->i_time_ref;
        p_sys->i_time_ref +=
            (i_modulo_time_base * p_dec->p_sys->i_fps_num);
        i_time_ref = p_sys->i_time_ref;
    }
    else
    {
        i_time_ref = p_sys->i_last_time_ref +
            (i_modulo_time_base * p_dec->p_sys->i_fps_num);
    }

#if 0
    msg_Err( p_dec, "interp pts/dts (%lli,%lli), pts/dts (%lli,%lli)",
             p_sys->i_interpolated_pts, p_sys->i_interpolated_dts,
             p_vop->i_pts, p_vop->i_dts );
#endif

    if( p_dec->p_sys->i_fps_num < 5 && /* Work-around buggy streams */
        p_dec->fmt_in.video.i_frame_rate > 0 &&
        p_dec->fmt_in.video.i_frame_rate_base > 0 )
    {
        p_sys->i_interpolated_pts += I64C(1000000) *
        p_dec->fmt_in.video.i_frame_rate_base *
        p_vop->i_rate / INPUT_RATE_DEFAULT /
        p_dec->fmt_in.video.i_frame_rate;
    }
    else if( p_dec->p_sys->i_fps_num )
        p_sys->i_interpolated_pts +=
            ( I64C(1000000) * (i_time_ref + i_time_increment -
              p_sys->i_last_time - p_sys->i_last_timeincr) *
              p_vop->i_rate / INPUT_RATE_DEFAULT /
              p_dec->p_sys->i_fps_num );

    p_sys->i_last_time = i_time_ref;
    p_sys->i_last_timeincr = i_time_increment;

    /* Correct interpolated dts when we receive a new pts/dts */
    if( p_vop->i_pts > 0 )
        p_sys->i_interpolated_pts = p_vop->i_pts;
    if( p_vop->i_dts > 0 )
        p_sys->i_interpolated_dts = p_vop->i_dts;

    if( (p_sys->i_flags & BLOCK_FLAG_TYPE_B) || !p_sys->b_frame )
    {
        /* Trivial case (DTS == PTS) */

        p_sys->i_interpolated_dts = p_sys->i_interpolated_pts;

        if( p_vop->i_pts > 0 )
            p_sys->i_interpolated_dts = p_vop->i_pts;
        if( p_vop->i_dts > 0 )
            p_sys->i_interpolated_dts = p_vop->i_dts;

        p_sys->i_interpolated_pts = p_sys->i_interpolated_dts;
    }
    else
    {
        if( p_sys->i_last_ref_pts > 0 )
            p_sys->i_interpolated_dts = p_sys->i_last_ref_pts;

        p_sys->i_last_ref_pts = p_sys->i_interpolated_pts;
    }

    return VLC_SUCCESS;
}

/* look at ffmpeg av_log2 ;) */
static int vlc_log2( unsigned int v )
{
    int n = 0;
    static const int vlc_log2_table[16] =
    {
        0,0,1,1,2,2,2,2, 3,3,3,3,3,3,3,3
    };

    if( v&0xffff0000 )
    {
        v >>= 16;
        n += 16;
    }
    if( v&0xff00 )
    {
        v >>= 8;
        n += 8;
    }
    if( v&0xf0 )
    {
        v >>= 4;
        n += 4;
    }
    n += vlc_log2_table[v];

    return n;
}
