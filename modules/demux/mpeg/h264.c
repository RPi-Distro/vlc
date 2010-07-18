/*****************************************************************************
 * h264.c : H264 Video demuxer
 *****************************************************************************
 * Copyright (C) 2002-2004 the VideoLAN team
 * $Id: a0e57b814e92fefc49aecddfe81ca32137db66b5 $
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_demux.h>
#include <vlc_codec.h>

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

#define FPS_TEXT N_("Frames per Second")
#define FPS_LONGTEXT N_("Desired frame rate for the H264 stream.")


vlc_module_begin ()
    set_shortname( "H264")
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_DEMUX )
    set_description( N_("H264 video demuxer" ) )
    set_capability( "demux", 0 )
    add_float( "h264-fps", 25.0, NULL, FPS_TEXT, FPS_LONGTEXT, true )
    set_callbacks( Open, Close )
    add_shortcut( "h264" )
vlc_module_end ()

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
struct demux_sys_t
{
    mtime_t     i_dts;
    es_out_id_t *p_es;

    float       f_fps;
    decoder_t *p_packetizer;
};

static int Demux( demux_t * );
static int Control( demux_t *, int, va_list );

#define H264_PACKET_SIZE 2048

/*****************************************************************************
 * Open: initializes demux structures
 *****************************************************************************/
static int Open( vlc_object_t * p_this )
{
    demux_t     *p_demux = (demux_t*)p_this;
    demux_sys_t *p_sys;
    const uint8_t *p_peek;
    es_format_t fmt;

    if( stream_Peek( p_demux->s, &p_peek, 5 ) < 5 ) return VLC_EGENERIC;

    if( p_peek[0] != 0x00 || p_peek[1] != 0x00 ||
        p_peek[2] != 0x00 || p_peek[3] != 0x01 ||
        (p_peek[4]&0x1F) != 7 ) /* SPS */
    {
        if( !p_demux->b_force )
        {
            msg_Warn( p_demux, "h264 module discarded (no startcode)" );
            return VLC_EGENERIC;
        }

        msg_Err( p_demux, "this doesn't look like a H264 ES stream, "
                 "continuing anyway" );
    }

    p_demux->pf_demux  = Demux;
    p_demux->pf_control= Control;
    p_demux->p_sys     = p_sys = malloc( sizeof( demux_sys_t ) );
    p_sys->p_es        = NULL;
    p_sys->i_dts       = 0;
    p_sys->f_fps       = var_CreateGetFloat( p_demux, "h264-fps" );
    if( p_sys->f_fps < 0.001 )
        p_sys->f_fps = 0.001;
    msg_Dbg( p_demux, "using %.2f fps", p_sys->f_fps );

    /* Load the mpegvideo packetizer */
    es_format_Init( &fmt, VIDEO_ES, VLC_CODEC_H264 );
    p_sys->p_packetizer = demux_PacketizerNew( p_demux, &fmt, "h264" );
    if( !p_sys->p_packetizer )
    {
        free( p_sys );
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: frees unused data
 *****************************************************************************/
static void Close( vlc_object_t * p_this )
{
    demux_t     *p_demux = (demux_t*)p_this;
    demux_sys_t *p_sys = p_demux->p_sys;

    demux_PacketizerDestroy( p_sys->p_packetizer );
    free( p_sys );
}

/*****************************************************************************
 * Demux: reads and demuxes data packets
 *****************************************************************************
 * Returns -1 in case of error, 0 in case of EOF, 1 otherwise
 *****************************************************************************/
static int Demux( demux_t *p_demux)
{
    demux_sys_t *p_sys = p_demux->p_sys;
    block_t *p_block_in, *p_block_out;

    if( ( p_block_in = stream_Block( p_demux->s, H264_PACKET_SIZE ) ) == NULL )
    {
        return 0;
    }

    /* m4v demuxer doesn't set pts/dts at all */
    p_block_in->i_dts = VLC_TS_0;
    p_block_in->i_pts = VLC_TS_0;

    while( (p_block_out = p_sys->p_packetizer->pf_packetize( p_sys->p_packetizer, &p_block_in )) )
    {
        while( p_block_out )
        {
            block_t *p_next = p_block_out->p_next;

            p_block_out->p_next = NULL;

            if( p_sys->p_es == NULL )
            {
                p_sys->p_packetizer->fmt_out.b_packetized = true;
                p_sys->p_es = es_out_Add( p_demux->out, &p_sys->p_packetizer->fmt_out);
            }

            es_out_Control( p_demux->out, ES_OUT_SET_PCR, p_sys->i_dts );
            p_block_out->i_dts = VLC_TS_0 + p_sys->i_dts;
            p_block_out->i_pts = VLC_TS_0 + p_sys->i_dts;

            es_out_Send( p_demux->out, p_sys->p_es, p_block_out );

            p_block_out = p_next;

            p_sys->i_dts += (int64_t)((double)1000000.0 / p_sys->f_fps);
        }
    }
    return 1;
}

/*****************************************************************************
 * Control:
 *****************************************************************************/
static int Control( demux_t *p_demux, int i_query, va_list args )
{
    /* demux_sys_t *p_sys  = p_demux->p_sys; */
    /* FIXME calculate the bitrate */
    if( i_query == DEMUX_SET_TIME )
        return VLC_EGENERIC;
    else
        return demux_vaControlHelper( p_demux->s,
                                       0, -1,
                                       0, 1, i_query, args );
}

