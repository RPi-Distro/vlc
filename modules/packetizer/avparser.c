/*****************************************************************************
 * avparser.c
 *****************************************************************************
 * Copyright (C) 2015 VLC authors and VideoLAN
 * $Id: c5d2891f6a0ecdbb2d433225c04cccf1a6b49c3d $
 *
 * Authors: Denis Charmet <typx@videolan.org>
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
#include <vlc_codec.h>
#include <vlc_block.h>

#include "../codec/avcodec/avcodec.h"
#include "../codec/avcodec/avcommon.h"

#include "avparser.h"

#if !LIBAVCODEC_VERSION_CHECK( 55, 52, 0, 63, 100 )
#include <libavutil/mem.h>
static inline void avcodec_free_context( AVCodecContext **ctx )
{
    if( !*ctx )
        return;

    av_free( (*ctx)->extradata );
    av_free( *ctx );
    *ctx = NULL;
}
#endif

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#ifndef MERGE_FFMPEG
vlc_module_begin ()
    AVPARSER_MODULE
vlc_module_end ()
#endif

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
struct decoder_sys_t
{
    AVCodecParserContext * p_parser_ctx;
    AVCodecContext * p_codec_ctx;
    int i_offset;
};

static block_t * Packetize( decoder_t *, block_t ** );

/*****************************************************************************
 * OpenPacketizer: probe the packetizer and return score
 *****************************************************************************
 * Tries to launch a decoder and return score so that the interface is able
 * to choose.
 *****************************************************************************/
int OpenPacketizer( vlc_object_t *p_this )
{
    decoder_t     *p_dec = (decoder_t*)p_this;
    decoder_sys_t *p_sys;

    /* Restrict to VP9 for now */
    if( p_dec->fmt_in.i_codec != VLC_CODEC_VP9 )
        return VLC_EGENERIC;

    unsigned i_avcodec_id;

    if( !GetFfmpegCodec( p_dec->fmt_in.i_codec, NULL, &i_avcodec_id, NULL ) )
        return VLC_EGENERIC;

    /* init avcodec */
    vlc_init_avcodec(p_this);

    /* It is less likely to have a parser than a codec, start by that */
    AVCodecParserContext * p_ctx = av_parser_init( i_avcodec_id );
    if( !p_ctx )
        return VLC_EGENERIC;

    AVCodec * p_codec = avcodec_find_decoder( i_avcodec_id );
    if( unlikely( !p_codec ) )
    {
        av_parser_close( p_ctx );
        return VLC_EGENERIC;
    }

    AVCodecContext * p_codec_ctx = avcodec_alloc_context3( p_codec );
    if( unlikely( !p_codec_ctx ) )
    {
        av_parser_close( p_ctx );
        return VLC_ENOMEM;
    }

    p_dec->p_sys = p_sys = malloc( sizeof(*p_sys) );

    if( unlikely( !p_sys ) )
    {
        avcodec_free_context( &p_codec_ctx );
        av_parser_close( p_ctx );
        return VLC_ENOMEM;
    }
    p_dec->pf_packetize = Packetize;
    p_sys->p_parser_ctx = p_ctx;
    p_sys->p_codec_ctx = p_codec_ctx;
    p_sys->i_offset = 0;
    es_format_Copy( &p_dec->fmt_out, &p_dec->fmt_in );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * ClosePacketizer:
 *****************************************************************************/
void ClosePacketizer( vlc_object_t *p_this )
{
    decoder_t     *p_dec = (decoder_t*)p_this;
    avcodec_free_context( &p_dec->p_sys->p_codec_ctx );
    av_parser_close( p_dec->p_sys->p_parser_ctx );
    es_format_Clean( &p_dec->fmt_out );
    free( p_dec->p_sys );
}

/*****************************************************************************
 * Packetize: packetize a frame
 *****************************************************************************/
static block_t *Packetize ( decoder_t *p_dec, block_t **pp_block )
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    if( pp_block == NULL || *pp_block == NULL )
        return NULL;
    if( (*pp_block)->i_flags&(BLOCK_FLAG_DISCONTINUITY|BLOCK_FLAG_CORRUPTED) )
    {
        block_Release( *pp_block );
        return NULL;
    }

    block_t * p_block = *pp_block;

    uint8_t * p_indata = p_block->p_buffer + p_sys->i_offset;
    int i_inlen = p_block->i_buffer -  p_sys->i_offset;
    uint8_t * p_outdata;
    int i_outlen;

    if( p_sys->i_offset == i_inlen )
        goto out;

    p_sys->i_offset += av_parser_parse2( p_sys->p_parser_ctx, p_sys->p_codec_ctx,
                                         &p_outdata, &i_outlen, p_indata, i_inlen,
                                         p_block->i_pts, p_block->i_dts, -1);

    if( unlikely( i_outlen <= 0 || !p_outdata ) )
        goto out;

    block_t * p_ret = block_Alloc( i_outlen );

    if( unlikely ( !p_ret ) )
        goto out;

    memcpy( p_ret->p_buffer, p_outdata, i_outlen );
    p_ret->i_pts = p_block->i_pts;
    p_ret->i_dts = p_block->i_dts;

    p_block->i_pts = p_block->i_dts = VLC_TS_INVALID;

    return p_ret;

out:
    p_sys->i_offset = 0;
    block_Release( *pp_block );
    *pp_block = NULL;
    return NULL;
}

