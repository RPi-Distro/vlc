/*****************************************************************************
 * tta.c : The Lossless True Audio parser
 *****************************************************************************
 * Copyright (C) 2006 the VideoLAN team
 * $Id: e4009089fded134386df905a1ff2504a5f82bffd $
 *
 * Authors: Derk-Jan Hartman <hartman at videolan dot org>
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
#include <vlc/vlc.h>
#include <vlc/input.h>
#include <vlc_codec.h>
#include <math.h>

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open  ( vlc_object_t * );
static void Close ( vlc_object_t * );

vlc_module_begin();
    set_shortname( "TTA" );
    set_description( _("TTA demuxer") );
    set_category( CAT_INPUT );
    set_subcategory( SUBCAT_INPUT_DEMUX );
    set_capability( "demux2", 145 );

    set_callbacks( Open, Close );
    add_shortcut( "tta" );
vlc_module_end();

#define TTA_FRAMETIME 1.04489795918367346939

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int Demux  ( demux_t * );
static int Control( demux_t *, int, va_list );

struct demux_sys_t
{
    /* */
    es_out_id_t *p_es;

    /* */
    int      i_totalframes;
    int      i_currentframe;
    uint32_t *pi_seektable;
    int      i_datalength;
    int      i_framelength;

    /* */
    vlc_meta_t     *p_meta;
    int64_t        i_start;
};

/*****************************************************************************
 * Open: initializes ES structures
 *****************************************************************************/
static int Open( vlc_object_t * p_this )
{
    demux_t     *p_demux = (demux_t*)p_this;
    demux_sys_t *p_sys;
    es_format_t fmt;
    uint8_t     *p_peek;
    uint8_t     p_header[22];
    uint8_t     *p_seektable;
    int         i_seektable_size = 0, i;
    //char        psz_info[4096];
    //module_t    *p_id3;

    if( stream_Peek( p_demux->s, &p_peek, 4 ) < 4 )
        return VLC_EGENERIC;

    if( memcmp( p_peek, "TTA1", 4 ) )
    {
        if( !p_demux->b_force ) return VLC_EGENERIC;

        /* User forced */
        msg_Err( p_demux, "this doesn't look like a flac stream, "
                 "continuing anyway" );
    }

    if( stream_Read( p_demux->s, p_header, 22 ) < 22 )
        return VLC_EGENERIC;

    /* Fill p_demux fields */
    p_demux->pf_demux = Demux;
    p_demux->pf_control = Control;
    p_demux->p_sys = p_sys = malloc( sizeof( demux_sys_t ) );
    
    /* Read the metadata */
    es_format_Init( &fmt, AUDIO_ES, VLC_FOURCC( 'T', 'T', 'A', '1' ) );
    fmt.audio.i_channels = GetWLE( &p_header[6] );
    fmt.audio.i_bitspersample = GetWLE( &p_header[8] );
    fmt.audio.i_rate = GetDWLE( &p_header[10] );

    p_sys->i_datalength = GetDWLE( &p_header[14] );
    p_sys->i_framelength = TTA_FRAMETIME * fmt.audio.i_rate;

    p_sys->i_totalframes = p_sys->i_datalength / p_sys->i_framelength + 
                          ((p_sys->i_datalength % p_sys->i_framelength) ? 1 : 0);
    p_sys->i_currentframe = 0;

    i_seektable_size = sizeof(uint32_t)*p_sys->i_totalframes;
    p_seektable = (uint8_t *)malloc( i_seektable_size );
    stream_Read( p_demux->s, p_seektable, i_seektable_size );
    p_sys->pi_seektable = (uint32_t *)malloc(i_seektable_size);

    for( i = 0; i < p_sys->i_totalframes; i++ )
        p_sys->pi_seektable[i] = GetDWLE( &p_seektable[i*4] );

    stream_Read( p_demux->s, NULL, 4 ); /* CRC */

    /* Store the header and Seektable for avcodec */
    fmt.i_extra = 22 + (p_sys->i_totalframes * 4) + 4;
    fmt.p_extra = malloc( fmt.i_extra );
    memcpy( fmt.p_extra, p_header, 22 );
    memcpy( fmt.p_extra+22, p_seektable, fmt.i_extra -22 );

    p_sys->p_es = es_out_Add( p_demux->out, &fmt );
    free( p_seektable );
    p_sys->i_start = stream_Tell( p_demux->s );
    
#if 0
    /* Parse possible id3 header */
    if( ( p_id3 = module_Need( p_demux, "meta reader", NULL, 0 ) ) )
    {
        p_sys->p_meta = (vlc_meta_t *)p_demux->p_private;
        p_demux->p_private = NULL;
        module_Unneed( p_demux, p_id3 );
    }

    if( !p_sys->p_meta )
        p_sys->p_meta = vlc_meta_New();
#endif
    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: frees unused data
 *****************************************************************************/
static void Close( vlc_object_t * p_this )
{
    demux_t        *p_demux = (demux_t*)p_this;
    demux_sys_t    *p_sys = p_demux->p_sys;

    free( p_sys );
}

/*****************************************************************************
 * Demux:
 *****************************************************************************
 * Returns -1 in case of error, 0 in case of EOF, 1 otherwise
 *****************************************************************************/
static int Demux( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    block_t     *p_data;

    if( p_sys->i_currentframe > p_sys->i_totalframes )
        return 0;

    p_data = stream_Block( p_demux->s, p_sys->pi_seektable[p_sys->i_currentframe] );
    if( p_data == NULL ) return 0;
    p_data->i_dts = p_data->i_pts = (int64_t)(1 + I64C(1000000) * p_sys->i_currentframe) * TTA_FRAMETIME;

    p_sys->i_currentframe++;

    es_out_Control( p_demux->out, ES_OUT_SET_PCR, p_data->i_dts );
    es_out_Send( p_demux->out, p_sys->p_es, p_data );

    return 1;
}

/*****************************************************************************
 * Control:
 *****************************************************************************/
static int Control( demux_t *p_demux, int i_query, va_list args )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    double   f, *pf;
    int64_t i64, *pi64;

    switch( i_query )
    {
        case DEMUX_GET_POSITION:
            pf = (double*) va_arg( args, double* );
            i64 = stream_Size( p_demux->s ) - p_sys->i_start;
            if( i64 > 0 )
            {
                *pf = (double)(stream_Tell( p_demux->s ) - p_sys->i_start )/ (double)i64;
            }
            else
            {
                *pf = 0.0;
            }
            return VLC_SUCCESS;

        case DEMUX_SET_POSITION:
            f = (double)va_arg( args, double );
            i64 = (int64_t)(f * (stream_Size( p_demux->s ) - p_sys->i_start));
            es_out_Control( p_demux->out, ES_OUT_RESET_PCR );
            if( i64 > 0 )
            {
                int64_t tmp = 0;
                int     i;
                for( i=0; i < p_sys->i_totalframes && tmp+p_sys->pi_seektable[i] < i64; i++)
                {
                    tmp += p_sys->pi_seektable[i];
                }
                stream_Seek( p_demux->s, tmp+p_sys->i_start );
                p_sys->i_currentframe = i;
                return VLC_SUCCESS;
            }
            return VLC_EGENERIC;

        case DEMUX_GET_LENGTH:
            pi64 = (int64_t*)va_arg( args, int64_t * );
            *pi64 = I64C(1000000) * p_sys->i_totalframes * TTA_FRAMETIME;
            return VLC_SUCCESS;

        case DEMUX_GET_TIME:
            pi64 = (int64_t*)va_arg( args, int64_t * );
            *pi64 = I64C(1000000) * p_sys->i_currentframe * TTA_FRAMETIME;
            return VLC_SUCCESS;

        default:
            return VLC_EGENERIC;
    }
}
