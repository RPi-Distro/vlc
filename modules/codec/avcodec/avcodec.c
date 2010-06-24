/*****************************************************************************
 * avcodec.c: video and audio decoder and encoder using libavcodec
 *****************************************************************************
 * Copyright (C) 1999-2008 the VideoLAN team
 * $Id: 7313a9b503291939af63bb02bd9eb16b2560841b $
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Gildas Bazin <gbazin@videolan.org>
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
#include <vlc_avcodec.h>
#include <vlc_cpu.h>

/* ffmpeg header */
#define HAVE_MMX 1
#ifdef HAVE_LIBAVCODEC_AVCODEC_H
#   include <libavcodec/avcodec.h>
#elif defined(HAVE_FFMPEG_AVCODEC_H)
#   include <ffmpeg/avcodec.h>
#else
#   include <avcodec.h>
#endif

#include "avcodec.h"
#include "avutil.h"

#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT( 51, 48, 0 )
#   error You must update libavcodec to a version >= 51.48.0
#elif LIBAVCODEC_VERSION_INT < AV_VERSION_INT( 52, 25, 0 )
#   warning You should update libavcodec to get subtitle support
#endif

/*****************************************************************************
 * decoder_sys_t: decoder descriptor
 *****************************************************************************/
struct decoder_sys_t
{
    /* Common part between video and audio decoder */
    FFMPEG_COMMON_MEMBERS
};

/****************************************************************************
 * Local prototypes
 ****************************************************************************/
static int OpenDecoder( vlc_object_t * );
static void CloseDecoder( vlc_object_t * );

static const int  nloopf_list[] = { 0, 1, 2, 3, 4 };
static const char *const nloopf_list_text[] =
  { N_("None"), N_("Non-ref"), N_("Bidir"), N_("Non-key"), N_("All") };

#ifdef ENABLE_SOUT
static const char *const enc_hq_list[] = { "rd", "bits", "simple" };
static const char *const enc_hq_list_text[] = {
    N_("rd"), N_("bits"), N_("simple") };
#endif

#ifdef MERGE_FFMPEG
# include "../../demux/avformat/avformat.h"
# include "../../access/avio.h"
#endif

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define MODULE_DESCRIPTION N_( "Various audio and video decoders/encoders " \
        "delivered by the FFmpeg library. This includes (MS)MPEG4, DivX, SV1,"\
        "H261, H263, H264, WMV, WMA, AAC, AMR, DV, MJPEG and other codecs")

vlc_module_begin ()
    set_shortname( "FFmpeg")
    add_shortcut( "ffmpeg" )
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_VCODEC )
    /* decoder main module */
#if defined(MODULE_NAME_is_ffmpegaltivec) \
     || (defined(CAN_COMPILE_ALTIVEC) && !defined(NO_ALTIVEC_IN_FFMPEG))
    set_description( N_("AltiVec FFmpeg audio/video decoder ((MS)MPEG4,SVQ1,H263,WMV,WMA)") )
    /*add_requirement( ALTIVEC )*/
    set_capability( "decoder", 71 )
#else
    set_description( N_("FFmpeg audio/video decoder") )
    set_help( MODULE_DESCRIPTION )
    set_capability( "decoder", 70 )
#endif
    set_section( N_("Decoding") , NULL )
    set_callbacks( OpenDecoder, CloseDecoder )


    add_bool( "ffmpeg-dr", true, NULL, DR_TEXT, DR_TEXT, true )
    add_integer ( "ffmpeg-error-resilience", 1, NULL, ERROR_TEXT,
        ERROR_LONGTEXT, true )
    add_integer ( "ffmpeg-workaround-bugs", 1, NULL, BUGS_TEXT, BUGS_LONGTEXT,
        false )
    add_bool( "ffmpeg-hurry-up", true, NULL, HURRYUP_TEXT, HURRYUP_LONGTEXT,
        false )
    add_integer( "ffmpeg-skip-frame", 0, NULL, SKIP_FRAME_TEXT,
        SKIP_FRAME_LONGTEXT, true )
        change_integer_range( -1, 4 )
    add_integer( "ffmpeg-skip-idct", 0, NULL, SKIP_IDCT_TEXT,
        SKIP_IDCT_LONGTEXT, true )
        change_integer_range( -1, 4 )
    add_integer ( "ffmpeg-vismv", 0, NULL, VISMV_TEXT, VISMV_LONGTEXT,
        true )
    add_integer ( "ffmpeg-lowres", 0, NULL, LOWRES_TEXT, LOWRES_LONGTEXT,
        true )
        change_integer_range( 0, 2 )
    add_bool( "ffmpeg-fast", false, NULL, FAST_TEXT, FAST_LONGTEXT, true )
    add_integer ( "ffmpeg-skiploopfilter", 0, NULL, SKIPLOOPF_TEXT,
                  SKIPLOOPF_LONGTEXT, true )
        change_safe ()
        change_integer_list( nloopf_list, nloopf_list_text, NULL )

    add_integer( "ffmpeg-debug", 0, NULL, DEBUG_TEXT, DEBUG_LONGTEXT,
                 true )
#if defined(HAVE_AVCODEC_VAAPI) || defined(HAVE_AVCODEC_DXVA2)
    add_bool( "ffmpeg-hw", false, NULL, HW_TEXT, HW_LONGTEXT, true )
#endif

#ifdef ENABLE_SOUT
    /* encoder submodule */
    add_submodule ()
    add_shortcut( "ffmpeg" )
    set_section( N_("Encoding") , NULL )
    set_description( N_("FFmpeg audio/video encoder") )
    set_capability( "encoder", 100 )
    set_callbacks( OpenEncoder, CloseEncoder )

    add_string( ENC_CFG_PREFIX "hq", "simple", NULL, ENC_HQ_TEXT,
                ENC_HQ_LONGTEXT, false )
        change_string_list( enc_hq_list, enc_hq_list_text, 0 )
    add_integer( ENC_CFG_PREFIX "keyint", 0, NULL, ENC_KEYINT_TEXT,
                 ENC_KEYINT_LONGTEXT, false )
    add_integer( ENC_CFG_PREFIX "bframes", 0, NULL, ENC_BFRAMES_TEXT,
                 ENC_BFRAMES_LONGTEXT, false )
    add_bool( ENC_CFG_PREFIX "hurry-up", false, NULL, ENC_HURRYUP_TEXT,
              ENC_HURRYUP_LONGTEXT, false )
    add_bool( ENC_CFG_PREFIX "interlace", false, NULL, ENC_INTERLACE_TEXT,
              ENC_INTERLACE_LONGTEXT, true )
    add_bool( ENC_CFG_PREFIX "interlace-me", true, NULL, ENC_INTERLACE_ME_TEXT,
              ENC_INTERLACE_ME_LONGTEXT, true )
    add_integer( ENC_CFG_PREFIX "vt", 0, NULL, ENC_VT_TEXT,
                 ENC_VT_LONGTEXT, true )
    add_bool( ENC_CFG_PREFIX "pre-me", false, NULL, ENC_PRE_ME_TEXT,
              ENC_PRE_ME_LONGTEXT, true )
    add_integer( ENC_CFG_PREFIX "rc-buffer-size", 224*1024*8, NULL,
                 ENC_RC_BUF_TEXT, ENC_RC_BUF_LONGTEXT, true )
    add_float( ENC_CFG_PREFIX "rc-buffer-aggressivity", 1.0, NULL,
               ENC_RC_BUF_AGGR_TEXT, ENC_RC_BUF_AGGR_LONGTEXT, true )
    add_float( ENC_CFG_PREFIX "i-quant-factor", 0, NULL,
               ENC_IQUANT_FACTOR_TEXT, ENC_IQUANT_FACTOR_LONGTEXT, true )
    add_integer( ENC_CFG_PREFIX "noise-reduction", 0, NULL,
                 ENC_NOISE_RED_TEXT, ENC_NOISE_RED_LONGTEXT, true )
    add_bool( ENC_CFG_PREFIX "mpeg4-matrix", false, NULL,
              ENC_MPEG4_MATRIX_TEXT, ENC_MPEG4_MATRIX_LONGTEXT, true )
    add_integer( ENC_CFG_PREFIX "qmin", 0, NULL,
                 ENC_QMIN_TEXT, ENC_QMIN_LONGTEXT, true )
    add_integer( ENC_CFG_PREFIX "qmax", 0, NULL,
                 ENC_QMAX_TEXT, ENC_QMAX_LONGTEXT, true )
    add_bool( ENC_CFG_PREFIX "trellis", false, NULL,
              ENC_TRELLIS_TEXT, ENC_TRELLIS_LONGTEXT, true )
    add_float( ENC_CFG_PREFIX "qscale", 0, NULL,
               ENC_QSCALE_TEXT, ENC_QSCALE_LONGTEXT, true )
    add_integer( ENC_CFG_PREFIX "strict", 0, NULL,
                 ENC_STRICT_TEXT, ENC_STRICT_LONGTEXT, true )
    add_float( ENC_CFG_PREFIX "lumi-masking", 0.0, NULL,
               ENC_LUMI_MASKING_TEXT, ENC_LUMI_MASKING_LONGTEXT, true )
    add_float( ENC_CFG_PREFIX "dark-masking", 0.0, NULL,
               ENC_DARK_MASKING_TEXT, ENC_DARK_MASKING_LONGTEXT, true )
    add_float( ENC_CFG_PREFIX "p-masking", 0.0, NULL,
               ENC_P_MASKING_TEXT, ENC_P_MASKING_LONGTEXT, true )
    add_float( ENC_CFG_PREFIX "border-masking", 0.0, NULL,
               ENC_BORDER_MASKING_TEXT, ENC_BORDER_MASKING_LONGTEXT, true )
    add_integer( ENC_CFG_PREFIX "luma-elim-threshold", 0, NULL,
                 ENC_LUMA_ELIM_TEXT, ENC_LUMA_ELIM_LONGTEXT, true )
    add_integer( ENC_CFG_PREFIX "chroma-elim-threshold", 0, NULL,
                 ENC_CHROMA_ELIM_TEXT, ENC_CHROMA_ELIM_LONGTEXT, true )

    /* Audio AAC encoder profile */
    add_string( ENC_CFG_PREFIX "aac-profile", "low", NULL,
                ENC_PROFILE_TEXT, ENC_PROFILE_LONGTEXT, true )
#endif /* ENABLE_SOUT */

    /* video filter submodule */
    add_submodule ()
    set_capability( "video filter2", 0 )
    set_callbacks( OpenDeinterlace, CloseDeinterlace )
    set_description( N_("FFmpeg deinterlace video filter") )
    add_shortcut( "ffmpeg-deinterlace" )

#ifdef MERGE_FFMPEG
    add_submodule ()
#   include "../../demux/avformat/avformat.c"
    add_submodule ()
        AVIO_MODULE
#endif
vlc_module_end ()

/*****************************************************************************
 * OpenDecoder: probe the decoder and return score
 *****************************************************************************/
static int OpenDecoder( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t*) p_this;
    int i_cat, i_codec_id, i_result;
    const char *psz_namecodec;

    AVCodecContext *p_context = NULL;
    AVCodec        *p_codec = NULL;

    /* *** determine codec type *** */
    if( !GetFfmpegCodec( p_dec->fmt_in.i_codec, &i_cat, &i_codec_id,
                             &psz_namecodec ) )
    {
        return VLC_EGENERIC;
    }

    /* Initialization must be done before avcodec_find_decoder() */
    InitLibavcodec(p_this);

    /* *** ask ffmpeg for a decoder *** */
    p_codec = avcodec_find_decoder( i_codec_id );
    if( !p_codec )
    {
        msg_Dbg( p_dec, "codec not found (%s)", psz_namecodec );
        return VLC_EGENERIC;
    }

    /* *** get a p_context *** */
    p_context = avcodec_alloc_context();
    if( !p_context )
        return VLC_ENOMEM;
    p_context->debug = var_InheritInteger( p_dec, "ffmpeg-debug" );
    p_context->opaque = (void *)p_this;

    /* Set CPU capabilities */
    unsigned i_cpu = vlc_CPU();
    p_context->dsp_mask = 0;
    if( !(i_cpu & CPU_CAPABILITY_MMX) )
    {
        p_context->dsp_mask |= FF_MM_MMX;
    }
    if( !(i_cpu & CPU_CAPABILITY_MMXEXT) )
    {
        p_context->dsp_mask |= FF_MM_MMXEXT;
    }
    if( !(i_cpu & CPU_CAPABILITY_3DNOW) )
    {
        p_context->dsp_mask |= FF_MM_3DNOW;
    }
    if( !(i_cpu & CPU_CAPABILITY_SSE) )
    {
        p_context->dsp_mask |= FF_MM_SSE;
    }
    if( !(i_cpu & CPU_CAPABILITY_SSE2) )
    {
        p_context->dsp_mask |= FF_MM_SSE2;
    }
#ifdef FF_MM_SSE3
    if( !(i_cpu & CPU_CAPABILITY_SSE3) )
        p_context->dsp_mask |= FF_MM_SSE3;
#endif
#ifdef FF_MM_SSSE3
    if( !(i_cpu & CPU_CAPABILITY_SSSE3) )
        p_context->dsp_mask |= FF_MM_SSSE3;
#endif
#ifdef FF_MM_SSE4
    if( !(i_cpu & CPU_CAPABILITY_SSE4_1) )
        p_context->dsp_mask |= FF_MM_SSE4;
#endif
#ifdef FF_MM_SSE42
    if( !(i_cpu & CPU_CAPABILITY_SSE4_2) )
        p_context->dsp_mask |= FF_MM_SSE42;
#endif

    p_dec->b_need_packetized = true;
    switch( i_cat )
    {
    case VIDEO_ES:
        p_dec->pf_decode_video = DecodeVideo;
        i_result =  InitVideoDec ( p_dec, p_context, p_codec,
                                       i_codec_id, psz_namecodec );
        break;
    case AUDIO_ES:
        p_dec->pf_decode_audio = DecodeAudio;
        i_result =  InitAudioDec ( p_dec, p_context, p_codec,
                                       i_codec_id, psz_namecodec );
        break;
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT( 52, 25, 0 )
    case SPU_ES:
        p_dec->pf_decode_sub = DecodeSubtitle;
        i_result =  InitSubtitleDec( p_dec, p_context, p_codec,
                                     i_codec_id, psz_namecodec );
        break;
#endif
    default:
        i_result = VLC_EGENERIC;
    }

    if( i_result == VLC_SUCCESS )
    {
        p_dec->p_sys->i_cat = i_cat;
        if( p_context->profile != FF_PROFILE_UNKNOWN)
            p_dec->fmt_in.i_profile = p_context->profile;
        if( p_context->level != FF_LEVEL_UNKNOWN)
            p_dec->fmt_in.i_level = p_context->level;
    }

    return i_result;
}

/*****************************************************************************
 * CloseDecoder: decoder destruction
 *****************************************************************************/
static void CloseDecoder( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t *)p_this;
    decoder_sys_t *p_sys = p_dec->p_sys;

    switch( p_sys->i_cat )
    {
    case AUDIO_ES:
         EndAudioDec ( p_dec );
        break;
    case VIDEO_ES:
         EndVideoDec ( p_dec );
        break;
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT( 52, 25, 0 )
    case SPU_ES:
         EndSubtitleDec( p_dec );
        break;
#endif
    }

    if( p_sys->p_context )
    {
        free( p_sys->p_context->extradata );
        p_sys->p_context->extradata = NULL;

        if( !p_sys->b_delayed_open )
        {
            vlc_avcodec_lock();
            avcodec_close( p_sys->p_context );
            vlc_avcodec_unlock();
        }
        msg_Dbg( p_dec, "ffmpeg codec (%s) stopped", p_sys->psz_namecodec );
        av_free( p_sys->p_context );
    }

    free( p_sys );
}

void InitLibavcodec( vlc_object_t *p_object )
{
    static bool b_ffmpeginit = false;

    vlc_avcodec_lock();

    /* *** init ffmpeg library (libavcodec) *** */
    if( !b_ffmpeginit )
    {
        avcodec_init();
        avcodec_register_all();
        av_log_set_callback( LibavutilCallback );
        b_ffmpeginit = true;

        msg_Dbg( p_object, "libavcodec initialized (interface 0x%x)",
                 LIBAVCODEC_VERSION_INT );
    }
    else
    {
        msg_Dbg( p_object, "libavcodec already initialized" );
    }

    vlc_avcodec_unlock();
}
