/*****************************************************************************
 * theora.c: theora decoder module making use of libtheora.
 *****************************************************************************
 * Copyright (C) 1999-2001 the VideoLAN team
 * $Id: d9d42f7a54e571574c4cad8a409761a5988d467f $
 *
 * Authors: Gildas Bazin <gbazin@videolan.org>
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
#include <vlc_codec.h>
#include <vlc_sout.h>
#include <vlc_input.h>
#include "../demux/xiph.h"

#include <ogg/ogg.h>

#include <theora/theora.h>

/*****************************************************************************
 * decoder_sys_t : theora decoder descriptor
 *****************************************************************************/
struct decoder_sys_t
{
    /* Module mode */
    bool b_packetizer;

    /*
     * Input properties
     */
    bool b_has_headers;

    /*
     * Theora properties
     */
    theora_info      ti;                        /* theora bitstream settings */
    theora_comment   tc;                            /* theora comment header */
    theora_state     td;                   /* theora bitstream user comments */

    /*
     * Decoding properties
     */
    bool b_decoded_first_keyframe;

    /*
     * Common properties
     */
    mtime_t i_pts;
};

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  OpenDecoder   ( vlc_object_t * );
static int  OpenPacketizer( vlc_object_t * );
static void CloseDecoder  ( vlc_object_t * );

static void *DecodeBlock  ( decoder_t *, block_t ** );
static int  ProcessHeaders( decoder_t * );
static void *ProcessPacket ( decoder_t *, ogg_packet *, block_t ** );

static picture_t *DecodePacket( decoder_t *, ogg_packet * );

static void ParseTheoraComments( decoder_t * );
static void theora_CopyPicture( picture_t *, yuv_buffer * );

static int  OpenEncoder( vlc_object_t *p_this );
static void CloseEncoder( vlc_object_t *p_this );
static block_t *Encode( encoder_t *p_enc, picture_t *p_pict );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define ENC_QUALITY_TEXT N_("Encoding quality")
#define ENC_QUALITY_LONGTEXT N_( \
  "Enforce a quality between 1 (low) and 10 (high), instead " \
  "of specifying a particular bitrate. This will produce a VBR stream." )

vlc_module_begin ()
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_VCODEC )
    set_shortname( "Theora" )
    set_description( N_("Theora video decoder") )
    set_capability( "decoder", 100 )
    set_callbacks( OpenDecoder, CloseDecoder )
    add_shortcut( "theora" )

    add_submodule ()
    set_description( N_("Theora video packetizer") )
    set_capability( "packetizer", 100 )
    set_callbacks( OpenPacketizer, CloseDecoder )
    add_shortcut( "theora" )

    add_submodule ()
    set_description( N_("Theora video encoder") )
    set_capability( "encoder", 150 )
    set_callbacks( OpenEncoder, CloseEncoder )
    add_shortcut( "theora" )

#   define ENC_CFG_PREFIX "sout-theora-"
    add_integer( ENC_CFG_PREFIX "quality", 2, NULL, ENC_QUALITY_TEXT,
                 ENC_QUALITY_LONGTEXT, false )
vlc_module_end ()

static const char *const ppsz_enc_options[] = {
    "quality", NULL
};

/*****************************************************************************
 * OpenDecoder: probe the decoder and return score
 *****************************************************************************/
static int OpenDecoder( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t*)p_this;
    decoder_sys_t *p_sys;

    if( p_dec->fmt_in.i_codec != VLC_CODEC_THEORA )
    {
        return VLC_EGENERIC;
    }

    /* Allocate the memory needed to store the decoder's structure */
    if( ( p_dec->p_sys = p_sys = malloc(sizeof(*p_sys)) ) == NULL )
        return VLC_ENOMEM;
    p_dec->p_sys->b_packetizer = false;
    p_sys->b_has_headers = false;
    p_sys->i_pts = VLC_TS_INVALID;
    p_sys->b_decoded_first_keyframe = false;

    /* Set output properties */
    p_dec->fmt_out.i_cat = VIDEO_ES;
    p_dec->fmt_out.i_codec = VLC_CODEC_I420;

    /* Set callbacks */
    p_dec->pf_decode_video = (picture_t *(*)(decoder_t *, block_t **))
        DecodeBlock;
    p_dec->pf_packetize    = (block_t *(*)(decoder_t *, block_t **))
        DecodeBlock;

    /* Init supporting Theora structures needed in header parsing */
    theora_comment_init( &p_sys->tc );
    theora_info_init( &p_sys->ti );

    return VLC_SUCCESS;
}

static int OpenPacketizer( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t*)p_this;

    int i_ret = OpenDecoder( p_this );

    if( i_ret == VLC_SUCCESS )
    {
        p_dec->p_sys->b_packetizer = true;
        p_dec->fmt_out.i_codec = VLC_CODEC_THEORA;
    }

    return i_ret;
}

/****************************************************************************
 * DecodeBlock: the whole thing
 ****************************************************************************
 * This function must be fed with ogg packets.
 ****************************************************************************/
static void *DecodeBlock( decoder_t *p_dec, block_t **pp_block )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    block_t *p_block;
    ogg_packet oggpacket;

    if( !pp_block || !*pp_block ) return NULL;

    p_block = *pp_block;

    /* Block to Ogg packet */
    oggpacket.packet = p_block->p_buffer;
    oggpacket.bytes = p_block->i_buffer;
    oggpacket.granulepos = p_block->i_dts;
    oggpacket.b_o_s = 0;
    oggpacket.e_o_s = 0;
    oggpacket.packetno = 0;

    /* Check for headers */
    if( !p_sys->b_has_headers )
    {
        if( ProcessHeaders( p_dec ) )
        {
            block_Release( *pp_block );
            return NULL;
        }
        p_sys->b_has_headers = true;
    }

    return ProcessPacket( p_dec, &oggpacket, pp_block );
}

/*****************************************************************************
 * ProcessHeaders: process Theora headers.
 *****************************************************************************/
static int ProcessHeaders( decoder_t *p_dec )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    ogg_packet oggpacket;

    unsigned pi_size[XIPH_MAX_HEADER_COUNT];
    void     *pp_data[XIPH_MAX_HEADER_COUNT];
    unsigned i_count;
    if( xiph_SplitHeaders( pi_size, pp_data, &i_count,
                           p_dec->fmt_in.i_extra, p_dec->fmt_in.p_extra) )
        return VLC_EGENERIC;
    if( i_count < 3 )
        goto error;

    oggpacket.granulepos = -1;
    oggpacket.e_o_s = 0;
    oggpacket.packetno = 0;

    /* Take care of the initial Vorbis header */
    oggpacket.b_o_s  = 1; /* yes this actually is a b_o_s packet :) */
    oggpacket.bytes  = pi_size[0];
    oggpacket.packet = pp_data[0];
    if( theora_decode_header( &p_sys->ti, &p_sys->tc, &oggpacket ) < 0 )
    {
        msg_Err( p_dec, "this bitstream does not contain Theora video data" );
        goto error;
    }

    /* Set output properties */
    if( !p_sys->b_packetizer )
    switch( p_sys->ti.pixelformat )
    {
      case OC_PF_420:
        p_dec->fmt_out.i_codec = VLC_CODEC_I420;
        break;
      case OC_PF_422:
        p_dec->fmt_out.i_codec = VLC_CODEC_I422;
        break;
      case OC_PF_444:
        p_dec->fmt_out.i_codec = VLC_CODEC_I444;
        break;
      case OC_PF_RSVD:
      default:
        msg_Err( p_dec, "unknown chroma in theora sample" );
        break;
    }
    p_dec->fmt_out.video.i_width = p_sys->ti.width;
    p_dec->fmt_out.video.i_height = p_sys->ti.height;
    if( p_sys->ti.frame_width && p_sys->ti.frame_height )
    {
        p_dec->fmt_out.video.i_visible_width = p_sys->ti.frame_width;
        p_dec->fmt_out.video.i_visible_height = p_sys->ti.frame_height;
        if( p_sys->ti.offset_x || p_sys->ti.offset_y )
        {
            p_dec->fmt_out.video.i_x_offset = p_sys->ti.offset_x;
            p_dec->fmt_out.video.i_y_offset = p_sys->ti.offset_y;
        }
    }

    if( p_sys->ti.aspect_denominator && p_sys->ti.aspect_numerator )
    {
        p_dec->fmt_out.video.i_sar_num = p_sys->ti.aspect_numerator;
        p_dec->fmt_out.video.i_sar_den = p_sys->ti.aspect_denominator;
    }
    else
    {
        p_dec->fmt_out.video.i_sar_num = 1;
        p_dec->fmt_out.video.i_sar_den = 1;
    }

    if( p_sys->ti.fps_numerator > 0 && p_sys->ti.fps_denominator > 0 )
    {
        p_dec->fmt_out.video.i_frame_rate = p_sys->ti.fps_numerator;
        p_dec->fmt_out.video.i_frame_rate_base = p_sys->ti.fps_denominator;
    }

    msg_Dbg( p_dec, "%dx%d %.02f fps video, frame content "
             "is %dx%d with offset (%d,%d)",
             p_sys->ti.width, p_sys->ti.height,
             (double)p_sys->ti.fps_numerator/p_sys->ti.fps_denominator,
             p_sys->ti.frame_width, p_sys->ti.frame_height,
             p_sys->ti.offset_x, p_sys->ti.offset_y );

    /* Sanity check that seems necessary for some corrupted files */
    if( p_sys->ti.width < p_sys->ti.frame_width ||
        p_sys->ti.height < p_sys->ti.frame_height )
    {
        msg_Warn( p_dec, "trying to correct invalid theora header "
                  "(frame size (%dx%d) is smaller than frame content (%d,%d))",
                  p_sys->ti.width, p_sys->ti.height,
                  p_sys->ti.frame_width, p_sys->ti.frame_height );

        if( p_sys->ti.width < p_sys->ti.frame_width )
            p_sys->ti.width = p_sys->ti.frame_width;
        if( p_sys->ti.height < p_sys->ti.frame_height )
            p_sys->ti.height = p_sys->ti.frame_height;
    }

    /* The next packet in order is the comments header */
    oggpacket.b_o_s  = 0;
    oggpacket.bytes  = pi_size[1];
    oggpacket.packet = pp_data[1];
    if( theora_decode_header( &p_sys->ti, &p_sys->tc, &oggpacket ) < 0 )
    {
        msg_Err( p_dec, "2nd Theora header is corrupted" );
        goto error;
    }

    ParseTheoraComments( p_dec );

    /* The next packet in order is the codebooks header
     * We need to watch out that this packet is not missing as a
     * missing or corrupted header is fatal. */
    oggpacket.b_o_s  = 0;
    oggpacket.bytes  = pi_size[2];
    oggpacket.packet = pp_data[2];
    if( theora_decode_header( &p_sys->ti, &p_sys->tc, &oggpacket ) < 0 )
    {
        msg_Err( p_dec, "3rd Theora header is corrupted" );
        goto error;
    }

    if( !p_sys->b_packetizer )
    {
        /* We have all the headers, initialize decoder */
        theora_decode_init( &p_sys->td, &p_sys->ti );
    }
    else
    {
        p_dec->fmt_out.i_extra = p_dec->fmt_in.i_extra;
        p_dec->fmt_out.p_extra = xrealloc( p_dec->fmt_out.p_extra,
                                                  p_dec->fmt_out.i_extra );
        memcpy( p_dec->fmt_out.p_extra,
                p_dec->fmt_in.p_extra, p_dec->fmt_out.i_extra );
    }

    for( unsigned i = 0; i < i_count; i++ )
        free( pp_data[i] );
    return VLC_SUCCESS;

error:
    for( unsigned i = 0; i < i_count; i++ )
        free( pp_data[i] );
    return VLC_EGENERIC;
}

/*****************************************************************************
 * ProcessPacket: processes a theora packet.
 *****************************************************************************/
static void *ProcessPacket( decoder_t *p_dec, ogg_packet *p_oggpacket,
                            block_t **pp_block )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    block_t *p_block = *pp_block;
    void *p_buf;

    if( ( p_block->i_flags&(BLOCK_FLAG_DISCONTINUITY|BLOCK_FLAG_CORRUPTED) ) != 0 )
    {
        /* Don't send the the first packet after a discontinuity to
         * theora_decode, otherwise we get purple/green display artifacts
         * appearing in the video output */
        return NULL;
    }

    /* Date management */
    if( p_block->i_pts > VLC_TS_INVALID && p_block->i_pts != p_sys->i_pts )
    {
        p_sys->i_pts = p_block->i_pts;
    }

    *pp_block = NULL; /* To avoid being fed the same packet again */

    if( p_sys->b_packetizer )
    {
        /* Date management */
        p_block->i_dts = p_block->i_pts = p_sys->i_pts;

        p_block->i_length = p_sys->i_pts - p_block->i_pts;

        p_buf = p_block;
    }
    else
    {
        p_buf = DecodePacket( p_dec, p_oggpacket );
        if( p_block )
            block_Release( p_block );
    }

    /* Date management */
    p_sys->i_pts += ( INT64_C(1000000) * p_sys->ti.fps_denominator /
                      p_sys->ti.fps_numerator ); /* 1 frame per packet */

    return p_buf;
}

/*****************************************************************************
 * DecodePacket: decodes a Theora packet.
 *****************************************************************************/
static picture_t *DecodePacket( decoder_t *p_dec, ogg_packet *p_oggpacket )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    picture_t *p_pic;
    yuv_buffer yuv;

    theora_decode_packetin( &p_sys->td, p_oggpacket );

    /* Check for keyframe */
    if( !(p_oggpacket->packet[0] & 0x80) /* data packet */ &&
        !(p_oggpacket->packet[0] & 0x40) /* intra frame */ )
        p_sys->b_decoded_first_keyframe = true;

    /* If we haven't seen a single keyframe yet, don't let Theora decode
     * anything, otherwise we'll get display artifacts.  (This is impossible
     * in the general case, but can happen if e.g. we play a network stream
     * using a timed URL, such that the server doesn't start the video with a
     * keyframe). */
    if( p_sys->b_decoded_first_keyframe )
        theora_decode_YUVout( &p_sys->td, &yuv );
    else
        return NULL;

    /* Get a new picture */
    p_pic = decoder_NewPicture( p_dec );
    if( !p_pic ) return NULL;

    theora_CopyPicture( p_pic, &yuv );

    p_pic->date = p_sys->i_pts;

    return p_pic;
}

/*****************************************************************************
 * ParseTheoraComments:
 *****************************************************************************/
static void ParseTheoraComments( decoder_t *p_dec )
{
    char *psz_name, *psz_value, *psz_comment;
    int i = 0;

    while ( i < p_dec->p_sys->tc.comments )
    {
        psz_comment = strdup( p_dec->p_sys->tc.user_comments[i] );
        if( !psz_comment )
            break;
        psz_name = psz_comment;
        psz_value = strchr( psz_comment, '=' );
        if( psz_value )
        {
            *psz_value = '\0';
            psz_value++;

            if( !p_dec->p_description )
                p_dec->p_description = vlc_meta_New();
            if( p_dec->p_description )
                vlc_meta_AddExtra( p_dec->p_description, psz_name, psz_value );
        }
        free( psz_comment );
        i++;
    }
}

/*****************************************************************************
 * CloseDecoder: theora decoder destruction
 *****************************************************************************/
static void CloseDecoder( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t *)p_this;
    decoder_sys_t *p_sys = p_dec->p_sys;

    theora_info_clear( &p_sys->ti );
    theora_comment_clear( &p_sys->tc );

    free( p_sys );
}

/*****************************************************************************
 * theora_CopyPicture: copy a picture from theora internal buffers to a
 *                     picture_t structure.
 *****************************************************************************/
static void theora_CopyPicture( picture_t *p_pic,
                                yuv_buffer *yuv )
{
    int i_plane, i_line, i_dst_stride, i_src_stride;
    uint8_t *p_dst, *p_src;

    for( i_plane = 0; i_plane < p_pic->i_planes; i_plane++ )
    {
        p_dst = p_pic->p[i_plane].p_pixels;
        p_src = i_plane ? (i_plane - 1 ? yuv->v : yuv->u ) : yuv->y;
        i_dst_stride  = p_pic->p[i_plane].i_pitch;
        i_src_stride  = i_plane ? yuv->uv_stride : yuv->y_stride;

        for( i_line = 0; i_line < p_pic->p[i_plane].i_lines; i_line++ )
        {
            vlc_memcpy( p_dst, p_src,
                        i_plane ? yuv->uv_width : yuv->y_width );
            p_src += i_src_stride;
            p_dst += i_dst_stride;
        }
    }
}

/*****************************************************************************
 * encoder_sys_t : theora encoder descriptor
 *****************************************************************************/
struct encoder_sys_t
{
    /*
     * Input properties
     */
    bool b_headers;

    /*
     * Theora properties
     */
    theora_info      ti;                        /* theora bitstream settings */
    theora_comment   tc;                            /* theora comment header */
    theora_state     td;                   /* theora bitstream user comments */

    int i_width, i_height;
};

/*****************************************************************************
 * OpenEncoder: probe the encoder and return score
 *****************************************************************************/
static int OpenEncoder( vlc_object_t *p_this )
{
    encoder_t *p_enc = (encoder_t *)p_this;
    encoder_sys_t *p_sys;
    int i_quality;

    if( p_enc->fmt_out.i_codec != VLC_CODEC_THEORA &&
        !p_enc->b_force )
    {
        return VLC_EGENERIC;
    }

    /* Allocate the memory needed to store the decoder's structure */
    if( ( p_sys = malloc(sizeof(encoder_sys_t)) ) == NULL )
        return VLC_ENOMEM;
    p_enc->p_sys = p_sys;

    p_enc->pf_encode_video = Encode;
    p_enc->fmt_in.i_codec = VLC_CODEC_I420;
    p_enc->fmt_out.i_codec = VLC_CODEC_THEORA;

    config_ChainParse( p_enc, ENC_CFG_PREFIX, ppsz_enc_options, p_enc->p_cfg );

    i_quality = var_GetInteger( p_enc, ENC_CFG_PREFIX "quality" );
    if( i_quality > 10 ) i_quality = 10;
    if( i_quality < 0 ) i_quality = 0;

    theora_info_init( &p_sys->ti );

    p_sys->ti.width = p_enc->fmt_in.video.i_width;
    p_sys->ti.height = p_enc->fmt_in.video.i_height;

    if( p_sys->ti.width % 16 || p_sys->ti.height % 16 )
    {
        /* Pictures from the transcoder should always have a pitch
         * which is a multiple of 16 */
        p_sys->ti.width = (p_sys->ti.width + 15) >> 4 << 4;
        p_sys->ti.height = (p_sys->ti.height + 15) >> 4 << 4;

        msg_Dbg( p_enc, "padding video from %dx%d to %dx%d",
                 p_enc->fmt_in.video.i_width, p_enc->fmt_in.video.i_height,
                 p_sys->ti.width, p_sys->ti.height );
    }

    p_sys->ti.frame_width = p_enc->fmt_in.video.i_width;
    p_sys->ti.frame_height = p_enc->fmt_in.video.i_height;
    p_sys->ti.offset_x = 0 /*frame_x_offset*/;
    p_sys->ti.offset_y = 0 /*frame_y_offset*/;

    p_sys->i_width = p_sys->ti.width;
    p_sys->i_height = p_sys->ti.height;

    if( !p_enc->fmt_in.video.i_frame_rate ||
        !p_enc->fmt_in.video.i_frame_rate_base )
    {
        p_sys->ti.fps_numerator = 25;
        p_sys->ti.fps_denominator = 1;
    }
    else
    {
        p_sys->ti.fps_numerator = p_enc->fmt_in.video.i_frame_rate;
        p_sys->ti.fps_denominator = p_enc->fmt_in.video.i_frame_rate_base;
    }

    if( p_enc->fmt_in.video.i_sar_num > 0 && p_enc->fmt_in.video.i_sar_den > 0 )
    {
        unsigned i_dst_num, i_dst_den;
        vlc_ureduce( &i_dst_num, &i_dst_den,
                     p_enc->fmt_in.video.i_sar_num,
                     p_enc->fmt_in.video.i_sar_den, 0 );
        p_sys->ti.aspect_numerator = i_dst_num;
        p_sys->ti.aspect_denominator = i_dst_den;
    }
    else
    {
        p_sys->ti.aspect_numerator = 4;
        p_sys->ti.aspect_denominator = 3;
    }

    p_sys->ti.target_bitrate = p_enc->fmt_out.i_bitrate;
    p_sys->ti.quality = ((float)i_quality) * 6.3;

    p_sys->ti.dropframes_p = 0;
    p_sys->ti.quick_p = 1;
    p_sys->ti.keyframe_auto_p = 1;
    p_sys->ti.keyframe_frequency = 64;
    p_sys->ti.keyframe_frequency_force = 64;
    p_sys->ti.keyframe_data_target_bitrate = p_enc->fmt_out.i_bitrate * 1.5;
    p_sys->ti.keyframe_auto_threshold = 80;
    p_sys->ti.keyframe_mindistance = 8;
    p_sys->ti.noise_sensitivity = 1;

    theora_encode_init( &p_sys->td, &p_sys->ti );
    theora_comment_init( &p_sys->tc );

    /* Create and store headers */
    for( int i = 0; i < 3; i++ )
    {
        ogg_packet header;

        if( i == 0 )
            theora_encode_header( &p_sys->td, &header );
        else if( i == 1 )
            theora_encode_comment( &p_sys->tc, &header );
        else
            theora_encode_tables( &p_sys->td, &header );

        if( xiph_AppendHeaders( &p_enc->fmt_out.i_extra, &p_enc->fmt_out.p_extra,
                                header.bytes, header.packet ) )
        {
            p_enc->fmt_out.i_extra = 0;
            p_enc->fmt_out.p_extra = NULL;
        }
    }
    return VLC_SUCCESS;
}

/****************************************************************************
 * Encode: the whole thing
 ****************************************************************************
 * This function spits out ogg packets.
 ****************************************************************************/
static block_t *Encode( encoder_t *p_enc, picture_t *p_pict )
{
    encoder_sys_t *p_sys = p_enc->p_sys;
    ogg_packet oggpacket;
    block_t *p_block;
    yuv_buffer yuv;
    int i;

    /* Sanity check */
    if( p_pict->p[0].i_pitch < (int)p_sys->i_width ||
        p_pict->p[0].i_lines < (int)p_sys->i_height )
    {
        msg_Warn( p_enc, "frame is smaller than encoding size"
                  "(%ix%i->%ix%i) -> dropping frame",
                  p_pict->p[0].i_pitch, p_pict->p[0].i_lines,
                  p_sys->i_width, p_sys->i_height );
        return NULL;
    }

    /* Fill padding */
    if( p_pict->p[0].i_visible_pitch < (int)p_sys->i_width )
    {
        for( i = 0; i < p_sys->i_height; i++ )
        {
            memset( p_pict->p[0].p_pixels + i * p_pict->p[0].i_pitch +
                    p_pict->p[0].i_visible_pitch,
                    *( p_pict->p[0].p_pixels + i * p_pict->p[0].i_pitch +
                       p_pict->p[0].i_visible_pitch - 1 ),
                    p_sys->i_width - p_pict->p[0].i_visible_pitch );
        }
        for( i = 0; i < p_sys->i_height / 2; i++ )
        {
            memset( p_pict->p[1].p_pixels + i * p_pict->p[1].i_pitch +
                    p_pict->p[1].i_visible_pitch,
                    *( p_pict->p[1].p_pixels + i * p_pict->p[1].i_pitch +
                       p_pict->p[1].i_visible_pitch - 1 ),
                    p_sys->i_width / 2 - p_pict->p[1].i_visible_pitch );
            memset( p_pict->p[2].p_pixels + i * p_pict->p[2].i_pitch +
                    p_pict->p[2].i_visible_pitch,
                    *( p_pict->p[2].p_pixels + i * p_pict->p[2].i_pitch +
                       p_pict->p[2].i_visible_pitch - 1 ),
                    p_sys->i_width / 2 - p_pict->p[2].i_visible_pitch );
        }
    }

    if( p_pict->p[0].i_visible_lines < (int)p_sys->i_height )
    {
        for( i = p_pict->p[0].i_visible_lines; i < p_sys->i_height; i++ )
        {
            memset( p_pict->p[0].p_pixels + i * p_pict->p[0].i_pitch, 0,
                    p_sys->i_width );
        }
        for( i = p_pict->p[1].i_visible_lines; i < p_sys->i_height / 2; i++ )
        {
            memset( p_pict->p[1].p_pixels + i * p_pict->p[1].i_pitch, 0x80,
                    p_sys->i_width / 2 );
            memset( p_pict->p[2].p_pixels + i * p_pict->p[2].i_pitch, 0x80,
                    p_sys->i_width / 2 );
        }
    }

    /* Theora is a one-frame-in, one-frame-out system. Submit a frame
     * for compression and pull out the packet. */

    yuv.y_width  = p_sys->i_width;
    yuv.y_height = p_sys->i_height;
    yuv.y_stride = p_pict->p[0].i_pitch;

    yuv.uv_width  = p_sys->i_width / 2;
    yuv.uv_height = p_sys->i_height / 2;
    yuv.uv_stride = p_pict->p[1].i_pitch;

    yuv.y = p_pict->p[0].p_pixels;
    yuv.u = p_pict->p[1].p_pixels;
    yuv.v = p_pict->p[2].p_pixels;

    if( theora_encode_YUVin( &p_sys->td, &yuv ) < 0 )
    {
        msg_Warn( p_enc, "failed encoding a frame" );
        return NULL;
    }

    theora_encode_packetout( &p_sys->td, 0, &oggpacket );

    /* Ogg packet to block */
    p_block = block_New( p_enc, oggpacket.bytes );
    memcpy( p_block->p_buffer, oggpacket.packet, oggpacket.bytes );
    p_block->i_dts = p_block->i_pts = p_pict->date;

    if( theora_packet_iskeyframe( &oggpacket ) )
    {
        p_block->i_flags |= BLOCK_FLAG_TYPE_I;
    }

    return p_block;
}

/*****************************************************************************
 * CloseEncoder: theora encoder destruction
 *****************************************************************************/
static void CloseEncoder( vlc_object_t *p_this )
{
    encoder_t *p_enc = (encoder_t *)p_this;
    encoder_sys_t *p_sys = p_enc->p_sys;

    theora_info_clear( &p_sys->ti );
    theora_comment_clear( &p_sys->tc );

    free( p_sys );
}
