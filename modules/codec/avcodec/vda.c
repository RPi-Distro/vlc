/*****************************************************************************
 * vda.c: VDA helpers for the libavcodec decoder
 *****************************************************************************
 * Copyright © 2012 VideoLAN
 *
 * Authors: Sebastien Zwickert <dilaroga@free.fr>
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <assert.h>

#include <vlc_common.h>
#include <vlc_vout.h>
#include <vlc_plugin.h>

#include <libavcodec/avcodec.h>

#include "avcodec.h"
#include "va.h"
#include "copy.h"

#include <libavcodec/vda.h>
#include <VideoDecodeAcceleration/VDADecoder.h>

static int Open( vlc_va_t *, int, const es_format_t * );
static void Close( vlc_va_t * );

static const int  nvda_pix_fmt_list[] = { 0, 1 };
static const char *const nvda_pix_fmt_list_text[] =
  { N_("420YpCbCr8Planar"), N_("422YpCbCr8") };

vlc_module_begin ()
    set_description( N_("Video Decode Acceleration Framework (VDA)") )
    set_capability( "hw decoder", 50 )
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_VCODEC )
    set_callbacks( Open, Close )
    add_integer ( "avcodec-vda-pix-fmt", 0, VDA_PIX_FMT_TEXT,
                  VDA_PIX_FMT_LONGTEXT, false)
        change_integer_list( nvda_pix_fmt_list, nvda_pix_fmt_list_text )
vlc_module_end ()

struct vlc_va_sys_t
{
    struct vda_context  hw_ctx;

    uint8_t             *p_extradata;
    int                 i_extradata;

    vlc_fourcc_t        i_chroma;

    copy_cache_t        image_cache;

    vlc_object_t        *p_log;

};

typedef struct vlc_va_sys_t vlc_va_vda_t;

static vlc_va_vda_t *vlc_va_vda_Get( vlc_va_t *p_va )
{
    return p_va->sys;
}

/*****************************************************************************
 * vda_Copy420YpCbCr8Planar: copy y420 CVPixelBuffer to picture_t
 *****************************************************************************/
static void vda_Copy420YpCbCr8Planar( picture_t *p_pic,
                                      CVPixelBufferRef buffer,
                                      unsigned i_width,
                                      unsigned i_height,
                                      copy_cache_t *cache )
{
    uint8_t *pp_plane[3];
    size_t  pi_pitch[3];

    if (!buffer)
        return;

    CVPixelBufferLockBaseAddress( buffer, 0 );

    for( int i = 0; i < 3; i++ )
    {
        pp_plane[i] = CVPixelBufferGetBaseAddressOfPlane( buffer, i );
        pi_pitch[i] = CVPixelBufferGetBytesPerRowOfPlane( buffer, i );
    }

    CopyFromYv12( p_pic, pp_plane, pi_pitch,
                  i_width, i_height, cache );

    CVPixelBufferUnlockBaseAddress( buffer, 0 );
}

/*****************************************************************************
 * vda_Copy422YpCbCr8: copy 2vuy CVPixelBuffer to picture_t
 *****************************************************************************/
static void vda_Copy422YpCbCr8( picture_t *p_pic,
                                CVPixelBufferRef buffer )
{
    int i_plane, i_line, i_dst_stride, i_src_stride;
    uint8_t *p_dst, *p_src;

    CVPixelBufferLockBaseAddress( buffer, 0 );

    for( i_plane = 0; i_plane < p_pic->i_planes; i_plane++ )
    {
        p_dst = p_pic->p[i_plane].p_pixels;
        p_src = CVPixelBufferGetBaseAddressOfPlane( buffer, i_plane );
        i_dst_stride  = p_pic->p[i_plane].i_pitch;
        i_src_stride  = CVPixelBufferGetBytesPerRowOfPlane( buffer, i_plane );

        for( i_line = 0; i_line < p_pic->p[i_plane].i_visible_lines ; i_line++ )
        {
            memcpy( p_dst, p_src, i_src_stride );

            p_src += i_src_stride;
            p_dst += i_dst_stride;
        }
    }

    CVPixelBufferUnlockBaseAddress( buffer, 0 );
}

static int Setup( vlc_va_t *external, void **pp_hw_ctx, vlc_fourcc_t *pi_chroma,
                  int i_width, int i_height )
{
    vlc_va_vda_t *p_va = vlc_va_vda_Get( external );

    if( p_va->hw_ctx.width == i_width
        && p_va->hw_ctx.height == i_height
        && p_va->hw_ctx.decoder )
    {
        *pp_hw_ctx = &p_va->hw_ctx;
        *pi_chroma = p_va->i_chroma;
        return VLC_SUCCESS;
    }

    if( p_va->hw_ctx.decoder )
    {
        ff_vda_destroy_decoder( &p_va->hw_ctx );
        goto ok;
    }

    memset( &p_va->hw_ctx, 0, sizeof(p_va->hw_ctx) );
    p_va->hw_ctx.width = i_width;
    p_va->hw_ctx.height = i_height;
    p_va->hw_ctx.format = 'avc1';
    p_va->hw_ctx.use_ref_buffer = 1;

    int i_pix_fmt = var_CreateGetInteger( p_va->p_log, "avcodec-vda-pix-fmt" );

    switch( i_pix_fmt )
    {
        case 1 :
            p_va->hw_ctx.cv_pix_fmt_type = kCVPixelFormatType_422YpCbCr8;
            p_va->i_chroma = VLC_CODEC_UYVY;
            msg_Dbg(p_va->p_log, "using pixel format 422YpCbCr8");
            break;
        case 0 :
        default :
            p_va->hw_ctx.cv_pix_fmt_type = kCVPixelFormatType_420YpCbCr8Planar;
            p_va->i_chroma = VLC_CODEC_I420;
            CopyInitCache( &p_va->image_cache, i_width );
            msg_Dbg(p_va->p_log, "using pixel format 420YpCbCr8Planar");
    }

ok:
    /* Setup the libavcodec hardware context */
    *pp_hw_ctx = &p_va->hw_ctx;
    *pi_chroma = p_va->i_chroma;

    /* create the decoder */
    int status = ff_vda_create_decoder( &p_va->hw_ctx,
                                        p_va->p_extradata,
                                        p_va->i_extradata );
    if( status )
    {
        msg_Err( p_va->p_log, "Failed to create decoder: %i", status );
        return VLC_EGENERIC;
    }
    else
        msg_Dbg( p_va->p_log, "VDA decoder created");

    return VLC_SUCCESS;
}

static int Get( vlc_va_t *external, AVFrame *p_ff )
{
    VLC_UNUSED( external );

    /* */
    for( int i = 0; i < 4; i++ )
    {
        p_ff->data[i] = NULL;
        p_ff->linesize[i] = 0;

        if( i == 0 || i == 3 )
        p_ff->data[i] = (uint8_t *)1; // dummy
    }

    return VLC_SUCCESS;
}

static int Extract( vlc_va_t *external, picture_t *p_picture, AVFrame *p_ff )
{
    vlc_va_vda_t *p_va = vlc_va_vda_Get( external );
    CVPixelBufferRef cv_buffer = ( CVPixelBufferRef )p_ff->data[3];

    if( !cv_buffer )
    {
        msg_Dbg( p_va->p_log, "Frame buffer is empty.");
        return VLC_EGENERIC;
    }
    if (!CVPixelBufferGetDataSize(cv_buffer) > 0)
    {
        msg_Dbg( p_va->p_log, "Empty frame buffer");
        return VLC_EGENERIC;
    }

    if( p_va->hw_ctx.cv_pix_fmt_type == kCVPixelFormatType_420YpCbCr8Planar )
    {
        if( !p_va->image_cache.buffer ) {
            CVPixelBufferRelease( cv_buffer );
            return VLC_EGENERIC;
        }

        vda_Copy420YpCbCr8Planar( p_picture,
                                  cv_buffer,
                                  p_va->hw_ctx.width,
                                  p_va->hw_ctx.height,
                                  &p_va->image_cache );
    }
    else
        vda_Copy422YpCbCr8( p_picture, cv_buffer );

    return VLC_SUCCESS;
}

static void Release( vlc_va_t *external, AVFrame *p_ff )
{
    VLC_UNUSED( external );
    CVPixelBufferRef cv_buffer = ( CVPixelBufferRef )p_ff->data[3];

    if ( cv_buffer )
        CVPixelBufferRelease( cv_buffer );
}

static void Close( vlc_va_t *external )
{
    vlc_va_vda_t *p_va = vlc_va_vda_Get( external );

    msg_Dbg(p_va->p_log, "destroying VDA decoder");

    ff_vda_destroy_decoder( &p_va->hw_ctx ) ;

    if( p_va->hw_ctx.cv_pix_fmt_type == kCVPixelFormatType_420YpCbCr8Planar )
        CopyCleanCache( &p_va->image_cache );

    free( p_va );
}

static int Open( vlc_va_t *external, int i_codec_id, const es_format_t *fmt )
{
    msg_Dbg( external, "opening VDA module" );
    if( i_codec_id != AV_CODEC_ID_H264 )
    {
        msg_Warn( external, "input codec isn't H264, canceling VDA decoding" );
        return VLC_EGENERIC;
    }

    if( fmt->p_extra == NULL || fmt->i_extra < 7 )
    {
        msg_Warn( external, "VDA requires extradata." );
        return VLC_EGENERIC;
    }

    vlc_va_vda_t *p_va = calloc( 1, sizeof(*p_va) );
    if( !p_va )
        return VLC_EGENERIC;

    p_va->p_log = VLC_OBJECT(external);
    p_va->p_extradata = fmt->p_extra;
    p_va->i_extradata = fmt->i_extra;

    external->sys = p_va;
    external->description = (char *)"VDA";
    external->pix_fmt = PIX_FMT_VDA_VLD;
    external->setup = Setup;
    external->get = Get;
    external->release = Release;
    external->extract = Extract;

    return VLC_SUCCESS;
}
