/*****************************************************************************
 * ffmpeg.c: video decoder using ffmpeg library
 *****************************************************************************
 * Copyright (C) 1999-2001 the VideoLAN team
 * $Id: 7bacb2eec7f29bf73a418bf3391171df854b78cb $
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
#include <vlc/vlc.h>
#include <vlc/decoder.h>

/* ffmpeg header */
#define HAVE_MMX 1
#ifdef HAVE_LIBAVCODEC_AVCODEC_H
#   include <libavcodec/avcodec.h>
#elif defined(HAVE_FFMPEG_AVCODEC_H)
#   include <ffmpeg/avcodec.h>
#else
#   include <avcodec.h>
#endif

#if LIBAVCODEC_BUILD < 4680
#   error You must have a libavcodec >= 4680 (get CVS)
#endif

#include "ffmpeg.h"

#ifdef LIBAVCODEC_PP
#   ifdef HAVE_POSTPROC_POSTPROCESS_H
#       include <postproc/postprocess.h>
#   else
#       include <libpostproc/postprocess.h>
#   endif
#endif

/*****************************************************************************
 * decoder_sys_t: decoder descriptor
 *****************************************************************************/
struct decoder_sys_t
{
    /* Common part between video and audio decoder */
    int i_cat;
    int i_codec_id;
    char *psz_namecodec;

    AVCodecContext *p_context;
    AVCodec        *p_codec;
};

/****************************************************************************
 * Local prototypes
 ****************************************************************************/
static int OpenDecoder( vlc_object_t * );
static void CloseDecoder( vlc_object_t * );

static int  nloopf_list[] = { 0, 1, 2, 3, 4 };
static char *nloopf_list_text[] =
  { N_("None"), N_("Non-ref"), N_("Bidir"), N_("Non-key"), N_("All") };

static char *enc_hq_list[] = { "rd", "bits", "simple" };
static char *enc_hq_list_text[] = { N_("rd"), N_("bits"), N_("simple") };
/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_shortname( "FFmpeg");
    set_category( CAT_INPUT );
    set_subcategory( SUBCAT_INPUT_SCODEC );
    /* decoder main module */
#if defined(MODULE_NAME_is_ffmpegaltivec) \
     || (defined(CAN_COMPILE_ALTIVEC) && !defined(NO_ALTIVEC_IN_FFMPEG))
    set_description( _("AltiVec FFmpeg audio/video decoder/encoder ((MS)MPEG4,SVQ1,H263,WMV,WMA)") );
    /*add_requirement( ALTIVEC );*/
    set_capability( "decoder", 71 );
#else
    set_description( _("FFmpeg audio/video decoder/encoder ((MS)MPEG4,SVQ1,H263,WMV,WMA)") );
    set_capability( "decoder", 70 );
#endif
    set_section( N_("Decoding") , NULL );
    set_callbacks( OpenDecoder, CloseDecoder );

    add_bool( "ffmpeg-dr", 1, NULL, DR_TEXT, DR_TEXT, VLC_TRUE );
    add_integer ( "ffmpeg-error-resilience", 1, NULL, ERROR_TEXT,
        ERROR_LONGTEXT, VLC_TRUE );
    add_integer ( "ffmpeg-workaround-bugs", 1, NULL, BUGS_TEXT, BUGS_LONGTEXT,
        VLC_FALSE );
    add_bool( "ffmpeg-hurry-up", 0, NULL, HURRYUP_TEXT, HURRYUP_LONGTEXT,
        VLC_FALSE );
    add_integer ( "ffmpeg-vismv", 0, NULL, VISMV_TEXT, VISMV_LONGTEXT,
        VLC_TRUE );
    add_integer ( "ffmpeg-lowres", 0, NULL, LOWRES_TEXT, LOWRES_LONGTEXT,
        VLC_TRUE );
        change_integer_range( 0, 2 );
    add_integer ( "ffmpeg-skiploopfilter", 0, NULL, SKIPLOOPF_TEXT,
                  SKIPLOOPF_LONGTEXT, VLC_TRUE );
        change_integer_list( nloopf_list, nloopf_list_text, 0 );

#ifdef LIBAVCODEC_PP
    add_integer( "ffmpeg-pp-q", 0, NULL, PP_Q_TEXT, PP_Q_LONGTEXT, VLC_FALSE );
    add_string( "ffmpeg-pp-name", "default", NULL, LIBAVCODEC_PP_TEXT,
        LIBAVCODEC_PP_LONGTEXT, VLC_TRUE );
#endif
    add_integer( "ffmpeg-debug", 0, NULL, DEBUG_TEXT, DEBUG_LONGTEXT,
                 VLC_TRUE );

    /* chroma conversion submodule */
    add_submodule();
    set_capability( "chroma", 50 );
    set_callbacks( E_(OpenChroma), E_(CloseChroma) );
    set_description( _("FFmpeg chroma conversion") );

    /* encoder submodule */
    add_submodule();
    set_section( N_("Encoding") , NULL );
    set_description( _("FFmpeg audio/video encoder") );
    set_capability( "encoder", 100 );
    set_callbacks( E_(OpenEncoder), E_(CloseEncoder) );

    add_string( ENC_CFG_PREFIX "hq", "simple", NULL, ENC_HQ_TEXT,
                ENC_HQ_LONGTEXT, VLC_FALSE );
        change_string_list( enc_hq_list, enc_hq_list_text, 0 );
    add_integer( ENC_CFG_PREFIX "keyint", 0, NULL, ENC_KEYINT_TEXT,
                 ENC_KEYINT_LONGTEXT, VLC_FALSE );
    add_integer( ENC_CFG_PREFIX "bframes", 0, NULL, ENC_BFRAMES_TEXT,
                 ENC_BFRAMES_LONGTEXT, VLC_FALSE );
    add_bool( ENC_CFG_PREFIX "hurry-up", 0, NULL, ENC_HURRYUP_TEXT,
              ENC_HURRYUP_LONGTEXT, VLC_FALSE );
    add_bool( ENC_CFG_PREFIX "interlace", 0, NULL, ENC_INTERLACE_TEXT,
              ENC_INTERLACE_LONGTEXT, VLC_TRUE );
    add_bool( ENC_CFG_PREFIX "interlace-me", 1, NULL, ENC_INTERLACE_ME_TEXT,
              ENC_INTERLACE_ME_LONGTEXT, VLC_TRUE );
    add_integer( ENC_CFG_PREFIX "vt", 0, NULL, ENC_VT_TEXT,
                 ENC_VT_LONGTEXT, VLC_TRUE );
    add_bool( ENC_CFG_PREFIX "pre-me", 0, NULL, ENC_PRE_ME_TEXT,
              ENC_PRE_ME_LONGTEXT, VLC_TRUE );
    add_bool( ENC_CFG_PREFIX "strict-rc", 0, NULL, ENC_RC_STRICT_TEXT,
              ENC_RC_STRICT_LONGTEXT, VLC_TRUE );
    add_integer( ENC_CFG_PREFIX "rc-buffer-size", 224*1024*8, NULL,
                 ENC_RC_BUF_TEXT, ENC_RC_BUF_LONGTEXT, VLC_TRUE );
    add_float( ENC_CFG_PREFIX "rc-buffer-aggressivity", 1.0, NULL,
               ENC_RC_BUF_AGGR_TEXT, ENC_RC_BUF_AGGR_LONGTEXT, VLC_TRUE );
    add_float( ENC_CFG_PREFIX "i-quant-factor", 0, NULL,
               ENC_IQUANT_FACTOR_TEXT, ENC_IQUANT_FACTOR_LONGTEXT, VLC_TRUE );
    add_integer( ENC_CFG_PREFIX "noise-reduction", 0, NULL,
                 ENC_NOISE_RED_TEXT, ENC_NOISE_RED_LONGTEXT, VLC_TRUE );
    add_bool( ENC_CFG_PREFIX "mpeg4-matrix", 0, NULL,
              ENC_MPEG4_MATRIX_TEXT, ENC_MPEG4_MATRIX_LONGTEXT, VLC_TRUE );
    add_integer( ENC_CFG_PREFIX "qmin", 0, NULL,
                 ENC_QMIN_TEXT, ENC_QMIN_LONGTEXT, VLC_TRUE );
    add_integer( ENC_CFG_PREFIX "qmax", 0, NULL,
                 ENC_QMAX_TEXT, ENC_QMAX_LONGTEXT, VLC_TRUE );
    add_bool( ENC_CFG_PREFIX "trellis", 0, NULL,
              ENC_TRELLIS_TEXT, ENC_TRELLIS_LONGTEXT, VLC_TRUE );
    add_float( ENC_CFG_PREFIX "qscale", 0, NULL,
               ENC_QSCALE_TEXT, ENC_QSCALE_LONGTEXT, VLC_TRUE );
    add_integer( ENC_CFG_PREFIX "strict", 0, NULL,
                 ENC_STRICT_TEXT, ENC_STRICT_LONGTEXT, VLC_TRUE );
    add_float( ENC_CFG_PREFIX "lumi-masking", 0.0, NULL,
               ENC_LUMI_MASKING_TEXT, ENC_LUMI_MASKING_LONGTEXT, VLC_TRUE );
    add_float( ENC_CFG_PREFIX "dark-masking", 0.0, NULL,
               ENC_DARK_MASKING_TEXT, ENC_DARK_MASKING_LONGTEXT, VLC_TRUE );
    add_float( ENC_CFG_PREFIX "p-masking", 0.0, NULL,
               ENC_P_MASKING_TEXT, ENC_P_MASKING_LONGTEXT, VLC_TRUE );
    add_float( ENC_CFG_PREFIX "border-masking", 0.0, NULL,
               ENC_BORDER_MASKING_TEXT, ENC_BORDER_MASKING_LONGTEXT, VLC_TRUE );
    add_integer( ENC_CFG_PREFIX "luma-elim-threshold", 0, NULL,
                 ENC_LUMA_ELIM_TEXT, ENC_LUMA_ELIM_LONGTEXT, VLC_TRUE );
    add_integer( ENC_CFG_PREFIX "chroma-elim-threshold", 0, NULL,
                 ENC_CHROMA_ELIM_TEXT, ENC_CHROMA_ELIM_LONGTEXT, VLC_TRUE );

    /* demux submodule */
    add_submodule();
    set_description( _("FFmpeg demuxer" ) );
    set_capability( "demux2", 2 );
    set_callbacks( E_(OpenDemux), E_(CloseDemux) );

    /* mux submodule */
    add_submodule();
    set_description( _("FFmpeg muxer" ) );
    set_capability( "sout mux", 2 );
    set_callbacks( E_(OpenMux), E_(CloseMux) );

    /* video filter submodule */
    add_submodule();
    set_capability( "video filter2", 50 );
    set_callbacks( E_(OpenFilter), E_(CloseFilter) );
    set_description( _("FFmpeg video filter") );

    /* crop/padd submodule */
    add_submodule();
    set_capability( "crop padd", 10 );
    set_callbacks( E_(OpenCropPadd), E_(CloseFilter) );
    set_description( _("FFmpeg crop padd filter") );

    /* video filter submodule */
    add_submodule();
    set_capability( "video filter2", 0 );
    set_callbacks( E_(OpenDeinterlace), E_(CloseDeinterlace) );
    set_description( _("FFmpeg deinterlace video filter") );
    add_shortcut( "ffmpeg-deinterlace" );

    var_Create( p_module->p_libvlc, "avcodec", VLC_VAR_MUTEX );
vlc_module_end();

/*****************************************************************************
 * OpenDecoder: probe the decoder and return score
 *****************************************************************************/
static int OpenDecoder( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t*) p_this;
    int i_cat, i_codec_id, i_result;
    char *psz_namecodec;

    AVCodecContext *p_context = NULL;
    AVCodec        *p_codec = NULL;

    /* *** determine codec type *** */
    if( !E_(GetFfmpegCodec)( p_dec->fmt_in.i_codec, &i_cat, &i_codec_id,
                             &psz_namecodec ) )
    {
        return VLC_EGENERIC;
    }

    /* Bail out if buggy decoder */
    if( i_codec_id == CODEC_ID_AAC )
    {
        msg_Dbg( p_dec, "refusing to use ffmpeg's (%s) decoder which is buggy",
                 psz_namecodec );
        return VLC_EGENERIC;
    }

    /* Initialization must be done before avcodec_find_decoder() */
    E_(InitLibavcodec)(p_this);

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
    p_context->debug = config_GetInt( p_dec, "ffmpeg-debug" );
    p_context->opaque = (void *)p_this;

    /* Set CPU capabilities */
    p_context->dsp_mask = 0;
    if( !(p_dec->p_libvlc->i_cpu & CPU_CAPABILITY_MMX) )
    {
        p_context->dsp_mask |= FF_MM_MMX;
    }
    if( !(p_dec->p_libvlc->i_cpu & CPU_CAPABILITY_MMXEXT) )
    {
        p_context->dsp_mask |= FF_MM_MMXEXT;
    }
    if( !(p_dec->p_libvlc->i_cpu & CPU_CAPABILITY_3DNOW) )
    {
        p_context->dsp_mask |= FF_MM_3DNOW;
    }
    if( !(p_dec->p_libvlc->i_cpu & CPU_CAPABILITY_SSE) )
    {
        p_context->dsp_mask |= FF_MM_SSE;
    }
    if( !(p_dec->p_libvlc->i_cpu & CPU_CAPABILITY_SSE2) )
    {
        p_context->dsp_mask |= FF_MM_SSE2;
    }

    switch( i_cat )
    {
    case VIDEO_ES:
        p_dec->b_need_packetized = VLC_TRUE;
        p_dec->pf_decode_video = E_(DecodeVideo);
        i_result = E_( InitVideoDec )( p_dec, p_context, p_codec,
                                       i_codec_id, psz_namecodec );
        break;
    case AUDIO_ES:
        p_dec->pf_decode_audio = E_(DecodeAudio);
        i_result = E_( InitAudioDec )( p_dec, p_context, p_codec,
                                       i_codec_id, psz_namecodec );
        break;
    default:
        i_result = VLC_EGENERIC;
    }

    if( i_result == VLC_SUCCESS ) p_dec->p_sys->i_cat = i_cat;

    return i_result;
}

/*****************************************************************************
 * CloseDecoder: decoder destruction
 *****************************************************************************/
static void CloseDecoder( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t *)p_this;
    decoder_sys_t *p_sys = p_dec->p_sys;
    vlc_value_t lockval;

    var_Get( p_dec->p_libvlc, "avcodec", &lockval );

    switch( p_sys->i_cat )
    {
    case AUDIO_ES:
        E_( EndAudioDec )( p_dec );
        break;
    case VIDEO_ES:
        E_( EndVideoDec )( p_dec );
        break;
    }

    if( p_sys->p_context )
    {
        if( p_sys->p_context->extradata )
            free( p_sys->p_context->extradata );
        p_sys->p_context->extradata = NULL;
        vlc_mutex_lock( lockval.p_address );
        avcodec_close( p_sys->p_context );
        vlc_mutex_unlock( lockval.p_address );
        msg_Dbg( p_dec, "ffmpeg codec (%s) stopped", p_sys->psz_namecodec );
        av_free( p_sys->p_context );
    }

    free( p_sys );
}

/*****************************************************************************
 * local Functions
 *****************************************************************************/
static void LibavcodecCallback( void *p_opaque, int i_level,
                                const char *psz_format, va_list va )
{
    int i_vlc_level;
    AVCodecContext *p_avctx = (AVCodecContext *)p_opaque;
    AVClass *p_avc;
    vlc_object_t *p_this;
    char *psz_new_format;
    const char *psz_item_name;

    p_avc = p_avctx ? p_avctx->av_class : 0;

#define cln p_avc->class_name
    /* Make sure we can get p_this back */
    if( !p_avctx || !p_avc || !cln ||
        cln[0]!='A' || cln[1]!='V' || cln[2]!='C' || cln[3]!='o' ||
        cln[4]!='d' || cln[5]!='e' || cln[6]!='c' )
    {
        if( i_level == AV_LOG_ERROR ) vfprintf( stderr, psz_format, va );
        return;
    }
#undef cln

    p_this = (vlc_object_t *)p_avctx->opaque;

    switch( i_level )
    {
    case AV_LOG_QUIET:
        i_vlc_level = VLC_MSG_ERR;
        break;
    case AV_LOG_ERROR:
        i_vlc_level = VLC_MSG_WARN;
        break;
    case AV_LOG_INFO:
        i_vlc_level = VLC_MSG_DBG;
        break;
    case AV_LOG_DEBUG:
        /* Print debug messages if they were requested */
        if( p_avctx->debug ) vfprintf( stderr, psz_format, va );
        return;
    default:
        return;
    }

    psz_item_name = p_avc->item_name(p_opaque);
    psz_new_format = malloc( strlen(psz_format) + strlen(psz_item_name)
                              + 18 + 5 );
    snprintf( psz_new_format, strlen(psz_format) + strlen(psz_item_name)
              + 18 + 5, "%s (%s@%p)", psz_format, p_avc->item_name(p_opaque), p_opaque );
    msg_GenericVa( p_this, MSG_QUEUE_NORMAL,i_vlc_level,
                    MODULE_STRING, psz_new_format, va );
    free( psz_new_format );
}

void E_(InitLibavcodec)( vlc_object_t *p_object )
{
    static int b_ffmpeginit = 0;
    vlc_value_t lockval;

    var_Get( p_object->p_libvlc, "avcodec", &lockval );
    vlc_mutex_lock( lockval.p_address );

    /* *** init ffmpeg library (libavcodec) *** */
    if( !b_ffmpeginit )
    {
        avcodec_init();
        avcodec_register_all();
        av_log_set_callback( LibavcodecCallback );
        b_ffmpeginit = 1;

        msg_Dbg( p_object, "libavcodec initialized (interface %d )",
                 LIBAVCODEC_BUILD );
    }
    else
    {
        msg_Dbg( p_object, "libavcodec already initialized" );
    }

    vlc_mutex_unlock( lockval.p_address );
}

/*****************************************************************************
 * Chroma fourcc -> ffmpeg_id mapping
 *****************************************************************************/
static struct
{
    vlc_fourcc_t  i_chroma;
    int  i_chroma_id;

} chroma_table[] =
{
    /* Planar YUV formats */
    { VLC_FOURCC('Y','U','V','A'), PIX_FMT_YUV444P }, /* Hack */
    { VLC_FOURCC('I','4','4','4'), PIX_FMT_YUV444P },
    { VLC_FOURCC('J','4','4','4'), PIX_FMT_YUVJ444P },
    { VLC_FOURCC('I','4','2','2'), PIX_FMT_YUV422P },
    { VLC_FOURCC('J','4','2','2'), PIX_FMT_YUVJ422P },
    { VLC_FOURCC('I','4','2','0'), PIX_FMT_YUV420P },
    { VLC_FOURCC('Y','V','1','2'), PIX_FMT_YUV420P },
    { VLC_FOURCC('I','Y','U','V'), PIX_FMT_YUV420P },
    { VLC_FOURCC('J','4','2','0'), PIX_FMT_YUVJ420P },
    { VLC_FOURCC('I','4','1','1'), PIX_FMT_YUV411P },
    { VLC_FOURCC('I','4','1','0'), PIX_FMT_YUV410P },
    { VLC_FOURCC('Y','V','U','9'), PIX_FMT_YUV410P },

    /* Packed YUV formats */
    { VLC_FOURCC('Y','U','Y','2'), PIX_FMT_YUV422 },
#if LIBAVCODEC_BUILD >= 4720
    { VLC_FOURCC('U','Y','V','Y'), PIX_FMT_UYVY422 },
#endif

    /* Packed RGB formats */
#if defined(WORDS_BIGENDIAN)
    { VLC_FOURCC('R','G','B','8'), PIX_FMT_BGR8 },
    { VLC_FOURCC('R','V','1','5'), PIX_FMT_BGR555 },
    { VLC_FOURCC('R','V','1','6'), PIX_FMT_BGR565 },
    { VLC_FOURCC('R','V','2','4'), PIX_FMT_BGR24 },
#else
#if defined(PIX_FMT_RGB8)
    { VLC_FOURCC('R','G','B','8'), PIX_FMT_RGB8 },
#endif
    { VLC_FOURCC('R','V','1','5'), PIX_FMT_RGB555 },
    { VLC_FOURCC('R','V','1','6'), PIX_FMT_RGB565 },
    { VLC_FOURCC('R','V','2','4'), PIX_FMT_RGB24 },
#endif
    { VLC_FOURCC('R','V','3','2'), PIX_FMT_RGBA32 },
#if defined(PIX_FMT_RGBA)
    { VLC_FOURCC('R','G','B','A'), PIX_FMT_RGBA },
#endif
    { VLC_FOURCC('G','R','E','Y'), PIX_FMT_GRAY8 },

    { 0, 0 }
};

int E_(GetFfmpegChroma)( vlc_fourcc_t i_chroma )
{
    int i;

    for( i = 0; chroma_table[i].i_chroma != 0; i++ )
    {
        if( chroma_table[i].i_chroma == i_chroma )
            return chroma_table[i].i_chroma_id;
    }
    return -1;
}

vlc_fourcc_t E_(GetVlcChroma)( int i_ffmpeg_chroma )
{
    int i;

    for( i = 0; chroma_table[i].i_chroma != 0; i++ )
    {
        if( chroma_table[i].i_chroma_id == i_ffmpeg_chroma )
            return chroma_table[i].i_chroma;
    }
    return 0;
}

/*****************************************************************************
 * Codec fourcc -> ffmpeg_id mapping
 *****************************************************************************/
static struct
{
    vlc_fourcc_t  i_fourcc;
    int  i_codec;
    int  i_cat;
    char *psz_name;

} codecs_table[] =
{
    /* MPEG-1 Video */
    { VLC_FOURCC('m','p','1','v'), CODEC_ID_MPEG1VIDEO,
      VIDEO_ES, "MPEG-1 Video" },
    { VLC_FOURCC('m','p','e','g'), CODEC_ID_MPEG1VIDEO,
      VIDEO_ES, "MPEG-1 Video" },
    { VLC_FOURCC('m','p','g','1'), CODEC_ID_MPEG1VIDEO,
      VIDEO_ES, "MPEG-1 Video" },
    { VLC_FOURCC('P','I','M','1'), CODEC_ID_MPEG1VIDEO,
      VIDEO_ES, "Pinnacle DC1000 (MPEG-1 Video)" },

    /* MPEG-2 Video */
    { VLC_FOURCC('m','p','2','v'), CODEC_ID_MPEG2VIDEO,
      VIDEO_ES, "MPEG-2 Video" },
    { VLC_FOURCC('M','P','E','G'), CODEC_ID_MPEG2VIDEO,
      VIDEO_ES, "MPEG-2 Video" },
    { VLC_FOURCC('m','p','g','v'), CODEC_ID_MPEG2VIDEO,
      VIDEO_ES, "MPEG-2 Video" },
    { VLC_FOURCC('m','p','g','2'), CODEC_ID_MPEG2VIDEO,
      VIDEO_ES, "MPEG-2 Video" },
    { VLC_FOURCC('h','d','v','1'), CODEC_ID_MPEG2VIDEO,
      VIDEO_ES, "HDV 720p30 (MPEG-2 Video)" },
    { VLC_FOURCC('h','d','v','2'), CODEC_ID_MPEG2VIDEO,
      VIDEO_ES, "Sony HDV (MPEG-2 Video)" },
    { VLC_FOURCC('h','d','v','3'), CODEC_ID_MPEG2VIDEO,
      VIDEO_ES, "FCP HDV (MPEG-2 Video)" },
    { VLC_FOURCC('h','d','v','5'), CODEC_ID_MPEG2VIDEO,
      VIDEO_ES, "HDV 720p25 (MPEG-2 Video)" },
    { VLC_FOURCC('m','x','5','n'), CODEC_ID_MPEG2VIDEO,
      VIDEO_ES, "MPEG2 IMX NTSC 525/60 50mb/s (FCP)" },
    { VLC_FOURCC('m','x','5','p'), CODEC_ID_MPEG2VIDEO,
      VIDEO_ES, "MPEG2 IMX PAL 625/60 50mb/s (FCP)" },
    { VLC_FOURCC('m','x','4','n'), CODEC_ID_MPEG2VIDEO,
      VIDEO_ES, "MPEG2 IMX NTSC 525/60 40mb/s (FCP)" },
    { VLC_FOURCC('m','x','4','p'), CODEC_ID_MPEG2VIDEO,
      VIDEO_ES, "MPEG2 IMX PAL 625/50 40mb/s (FCP)" },
     { VLC_FOURCC('m','x','3','n'), CODEC_ID_MPEG2VIDEO,
      VIDEO_ES, "MPEG2 IMX NTSC 525/60 30mb/s (FCP)" },
    { VLC_FOURCC('m','x','3','p'), CODEC_ID_MPEG2VIDEO,
      VIDEO_ES, "MPEG2 IMX NTSC 625/50 30mb/s (FCP)" },
    { VLC_FOURCC('x','d','v','2'), CODEC_ID_MPEG2VIDEO,
      VIDEO_ES, "XDCAM HD 1080i60" },
    { VLC_FOURCC('A','V','m','p'), CODEC_ID_MPEG2VIDEO,
      VIDEO_ES, "AVID IMX PAL" },
    /* ATI VCR2 */
    { VLC_FOURCC('V','C','R','2'), CODEC_ID_MPEG2VIDEO,
      VIDEO_ES, "ATI VCR2 Video" },
    { VLC_FOURCC('M','M','E','S'), CODEC_ID_MPEG2VIDEO,
      VIDEO_ES, "Matrox MPEG-2" },
    { VLC_FOURCC('m','m','e','s'), CODEC_ID_MPEG2VIDEO,
      VIDEO_ES, "Matrox MPEG-2" },

    /* MPEG-4 Video */
    { VLC_FOURCC('D','I','V','X'), CODEC_ID_MPEG4,
      VIDEO_ES, "MPEG-4 Video" },
    { VLC_FOURCC('d','i','v','x'), CODEC_ID_MPEG4,
      VIDEO_ES, "MPEG-4 Video" },
    { VLC_FOURCC('M','P','4','S'), CODEC_ID_MPEG4,
      VIDEO_ES, "MPEG-4 Video" },
    { VLC_FOURCC('m','p','4','s'), CODEC_ID_MPEG4,
      VIDEO_ES, "MPEG-4 Video" },
    { VLC_FOURCC('M','4','S','2'), CODEC_ID_MPEG4,
      VIDEO_ES, "MPEG-4 Video" },
    { VLC_FOURCC('m','4','s','2'), CODEC_ID_MPEG4,
      VIDEO_ES, "MPEG-4 Video" },
    { VLC_FOURCC('x','v','i','d'), CODEC_ID_MPEG4,
      VIDEO_ES, "MPEG-4 Video" },
    { VLC_FOURCC('X','V','I','D'), CODEC_ID_MPEG4,
      VIDEO_ES, "MPEG-4 Video" },
    { VLC_FOURCC('X','v','i','D'), CODEC_ID_MPEG4,
      VIDEO_ES, "MPEG-4 Video" },
    { VLC_FOURCC('X','V','I','X'), CODEC_ID_MPEG4,
      VIDEO_ES, "MPEG-4 Video" },
    { VLC_FOURCC('x','v','i','x'), CODEC_ID_MPEG4,
      VIDEO_ES, "MPEG-4 Video" },
    { VLC_FOURCC('D','X','5','0'), CODEC_ID_MPEG4,
      VIDEO_ES, "MPEG-4 Video" },
    { VLC_FOURCC('d','x','5','0'), CODEC_ID_MPEG4,
      VIDEO_ES, "MPEG-4 Video" },
    { VLC_FOURCC('B','L','Z','0'), CODEC_ID_MPEG4,
      VIDEO_ES, "Blizzard MPEG-4 Video" },
    { VLC_FOURCC('D','X','G','M'), CODEC_ID_MPEG4,
      VIDEO_ES, "Electronic Arts Game MPEG-4 Video" },
    { VLC_FOURCC('m','p','4','v'), CODEC_ID_MPEG4,
      VIDEO_ES, "MPEG-4 Video" },
    { VLC_FOURCC('M','P','4','V'), CODEC_ID_MPEG4,
      VIDEO_ES, "MPEG-4 Video" },
    { VLC_FOURCC( 4,  0,  0,  0 ), CODEC_ID_MPEG4,
      VIDEO_ES, "MPEG-4 Video" },
    { VLC_FOURCC('m','4','c','c'), CODEC_ID_MPEG4,
      VIDEO_ES, "MPEG-4 Video" },
    { VLC_FOURCC('M','4','C','C'), CODEC_ID_MPEG4,
      VIDEO_ES, "MPEG-4 Video" },
    { VLC_FOURCC('F','M','P','4'), CODEC_ID_MPEG4,
      VIDEO_ES, "MPEG-4 Video" },
    { VLC_FOURCC('f','m','p','4'), CODEC_ID_MPEG4,
      VIDEO_ES, "MPEG-4 Video" },
    /* 3ivx delta 3.5 Unsupported
     * putting it here gives extreme distorted images
    { VLC_FOURCC('3','I','V','1'), CODEC_ID_MPEG4,
      VIDEO_ES, "MPEG-4 Video" },
    { VLC_FOURCC('3','i','v','1'), CODEC_ID_MPEG4,
      VIDEO_ES, "MPEG-4 Video" }, */
    /* 3ivx delta 4 */
    { VLC_FOURCC('3','I','V','2'), CODEC_ID_MPEG4,
      VIDEO_ES, "MPEG-4 Video" },
    { VLC_FOURCC('3','i','v','2'), CODEC_ID_MPEG4,
      VIDEO_ES, "MPEG-4 Video" },
    { VLC_FOURCC('U','M','P','4'), CODEC_ID_MPEG4,
      VIDEO_ES, "UB MPEG-4 Video" },
    { VLC_FOURCC('W','V','1','F'), CODEC_ID_MPEG4,
      VIDEO_ES, "MPEG-4 Video" },
    { VLC_FOURCC('S','E','D','G'), CODEC_ID_MPEG4,
      VIDEO_ES, "Samsung MPEG-4 Video" },
    { VLC_FOURCC('R','M','P','4'), CODEC_ID_MPEG4,
      VIDEO_ES, "REALmagic MPEG-4 Video" },
    { VLC_FOURCC('H','D','X','4'), CODEC_ID_MPEG4,
      VIDEO_ES, "Jomigo HDX4 (MPEG-4 Video)" },
    { VLC_FOURCC('h','d','x','4'), CODEC_ID_MPEG4,
      VIDEO_ES, "Jomigo HDX4 (MPEG-4 Video)" },
    { VLC_FOURCC('S','M','P','4'), CODEC_ID_MPEG4,
      VIDEO_ES, "Samsung SMP4 (MPEG-4 Video)" },
    { VLC_FOURCC('f','v','f','w'), CODEC_ID_MPEG4,
      VIDEO_ES, "FFmpeg MPEG-4" },
    { VLC_FOURCC('F','V','F','W'), CODEC_ID_MPEG4,
      VIDEO_ES, "FFmpeg MPEG-4" },

    /* MSMPEG4 v1 */
    { VLC_FOURCC('D','I','V','1'), CODEC_ID_MSMPEG4V1,
      VIDEO_ES, "MS MPEG-4 Video v1" },
    { VLC_FOURCC('d','i','v','1'), CODEC_ID_MSMPEG4V1,
      VIDEO_ES, "MS MPEG-4 Video v1" },
    { VLC_FOURCC('M','P','G','4'), CODEC_ID_MSMPEG4V1,
      VIDEO_ES, "MS MPEG-4 Video v1" },
    { VLC_FOURCC('m','p','g','4'), CODEC_ID_MSMPEG4V1,
      VIDEO_ES, "MS MPEG-4 Video v1" },

    /* MSMPEG4 v2 */
    { VLC_FOURCC('D','I','V','2'), CODEC_ID_MSMPEG4V2,
      VIDEO_ES, "MS MPEG-4 Video v2" },
    { VLC_FOURCC('d','i','v','2'), CODEC_ID_MSMPEG4V2,
      VIDEO_ES, "MS MPEG-4 Video v2" },
    { VLC_FOURCC('M','P','4','2'), CODEC_ID_MSMPEG4V2,
      VIDEO_ES, "MS MPEG-4 Video v2" },
    { VLC_FOURCC('m','p','4','2'), CODEC_ID_MSMPEG4V2,
      VIDEO_ES, "MS MPEG-4 Video v2" },

    /* MSMPEG4 v3 / M$ mpeg4 v3 */
    { VLC_FOURCC('M','P','G','3'), CODEC_ID_MSMPEG4V3,
      VIDEO_ES, "MS MPEG-4 Video v3" },
    { VLC_FOURCC('m','p','g','3'), CODEC_ID_MSMPEG4V3,
      VIDEO_ES, "MS MPEG-4 Video v3" },
    { VLC_FOURCC('d','i','v','3'), CODEC_ID_MSMPEG4V3,
      VIDEO_ES, "MS MPEG-4 Video v3" },
    { VLC_FOURCC('M','P','4','3'), CODEC_ID_MSMPEG4V3,
      VIDEO_ES, "MS MPEG-4 Video v3" },
    { VLC_FOURCC('m','p','4','3'), CODEC_ID_MSMPEG4V3,
      VIDEO_ES, "MS MPEG-4 Video v3" },
    /* DivX 3.20 */
    { VLC_FOURCC('D','I','V','3'), CODEC_ID_MSMPEG4V3,
      VIDEO_ES, "MS MPEG-4 Video v3" },
    { VLC_FOURCC('D','I','V','4'), CODEC_ID_MSMPEG4V3,
      VIDEO_ES, "MS MPEG-4 Video v3" },
    { VLC_FOURCC('d','i','v','4'), CODEC_ID_MSMPEG4V3,
      VIDEO_ES, "MS MPEG-4 Video v3" },
    { VLC_FOURCC('D','I','V','5'), CODEC_ID_MSMPEG4V3,
      VIDEO_ES, "MS MPEG-4 Video v3" },
    { VLC_FOURCC('d','i','v','5'), CODEC_ID_MSMPEG4V3,
      VIDEO_ES, "MS MPEG-4 Video v3" },
    { VLC_FOURCC('D','I','V','6'), CODEC_ID_MSMPEG4V3,
      VIDEO_ES, "MS MPEG-4 Video v3" },
    { VLC_FOURCC('d','i','v','6'), CODEC_ID_MSMPEG4V3,
      VIDEO_ES, "MS MPEG-4 Video v3" },
    /* Cool Codec */
    { VLC_FOURCC('C','O','L','1'), CODEC_ID_MSMPEG4V3,
      VIDEO_ES, "MS MPEG-4 Video v3" },
    { VLC_FOURCC('c','o','l','1'), CODEC_ID_MSMPEG4V3,
      VIDEO_ES, "MS MPEG-4 Video v3" },
    { VLC_FOURCC('C','O','L','0'), CODEC_ID_MSMPEG4V3,
      VIDEO_ES, "MS MPEG-4 Video v3" },
    { VLC_FOURCC('c','o','l','0'), CODEC_ID_MSMPEG4V3,
      VIDEO_ES, "MS MPEG-4 Video v3" },
    /* AngelPotion stuff */
    { VLC_FOURCC('A','P','4','1'), CODEC_ID_MSMPEG4V3,
      VIDEO_ES, "MS MPEG-4 Video v3" },
    /* 3ivx doctered divx files */
    { VLC_FOURCC('3','I','V','D'), CODEC_ID_MSMPEG4V3,
      VIDEO_ES, "MS MPEG-4 Video v3" },
    { VLC_FOURCC('3','i','v','d'), CODEC_ID_MSMPEG4V3,
      VIDEO_ES, "MS MPEG-4 Video v3" },
    /* who knows? */
    { VLC_FOURCC('3','V','I','D'), CODEC_ID_MSMPEG4V3,
      VIDEO_ES, "MS MPEG-4 Video v3" },
    { VLC_FOURCC('3','v','i','d'), CODEC_ID_MSMPEG4V3,
      VIDEO_ES, "MS MPEG-4 Video v3" },

    /* Sorenson v1 */
    { VLC_FOURCC('S','V','Q','1'), CODEC_ID_SVQ1,
      VIDEO_ES, "SVQ-1 (Sorenson Video v1)" },
    { VLC_FOURCC('s','v','q','1'), CODEC_ID_SVQ1,
      VIDEO_ES, "SVQ-1 (Sorenson Video v1)" },
    { VLC_FOURCC('s','v','q','i'), CODEC_ID_SVQ1,
      VIDEO_ES, "SVQ-1 (Sorenson Video v1)" },

    /* Sorenson v3 */
    { VLC_FOURCC('S','V','Q','3'), CODEC_ID_SVQ3,
      VIDEO_ES, "SVQ-3 (Sorenson Video v3)" },

    /* h264 */
    { VLC_FOURCC('h','2','6','4'), CODEC_ID_H264,
      VIDEO_ES, "H264 - MPEG-4 AVC (part 10)" },
    { VLC_FOURCC('H','2','6','4'), CODEC_ID_H264,
      VIDEO_ES, "H264 - MPEG-4 AVC (part 10)" },
    { VLC_FOURCC('x','2','6','4'), CODEC_ID_H264,
      VIDEO_ES, "H264 - MPEG-4 AVC (part 10)" },
    { VLC_FOURCC('X','2','6','4'), CODEC_ID_H264,
      VIDEO_ES, "H264 - MPEG-4 AVC (part 10)" },
    /* avc1: special case h264 */
    { VLC_FOURCC('a','v','c','1'), CODEC_ID_H264,
      VIDEO_ES, "H264 - MPEG-4 AVC (part 10)" },
    { VLC_FOURCC('A','V','C','1'), CODEC_ID_H264,
      VIDEO_ES, "H264 - MPEG-4 AVC (part 10)" },
    { VLC_FOURCC('V','S','S','H'), CODEC_ID_H264,
      VIDEO_ES, "Vanguard VSS H264" },
    { VLC_FOURCC('V','S','S','W'), CODEC_ID_H264,
      VIDEO_ES, "Vanguard VSS H264" },
    { VLC_FOURCC('v','s','s','h'), CODEC_ID_H264,
      VIDEO_ES, "Vanguard VSS H264" },
    { VLC_FOURCC('D','A','V','C'), CODEC_ID_H264,
      VIDEO_ES, "Dicas MPEGable H.264/MPEG-4 AVC" },
    { VLC_FOURCC('d','a','v','c'), CODEC_ID_H264,
      VIDEO_ES, "Dicas MPEGable H.264/MPEG-4 AVC" },

/* H263 and H263i */
/* H263(+) is also known as Real Video 1.0 */

    /* H263 */
    { VLC_FOURCC('D','2','6','3'), CODEC_ID_H263,
      VIDEO_ES, "DEC H263" },
    { VLC_FOURCC('H','2','6','3'), CODEC_ID_H263,
      VIDEO_ES, "H263" },
    { VLC_FOURCC('h','2','6','3'), CODEC_ID_H263,
      VIDEO_ES, "H263" },
    { VLC_FOURCC('L','2','6','3'), CODEC_ID_H263,
      VIDEO_ES, "LEAD H263" },
    { VLC_FOURCC('s','2','6','3'), CODEC_ID_H263,
      VIDEO_ES, "H263" },
    { VLC_FOURCC('S','2','6','3'), CODEC_ID_H263,
      VIDEO_ES, "H263" },
    { VLC_FOURCC('M','2','6','3'), CODEC_ID_H263,
      VIDEO_ES, "Microsoft H263" },
    { VLC_FOURCC('X','2','6','3'), CODEC_ID_H263,
      VIDEO_ES, "Xirlink H263" },

    /* Zygo (partial) */
    { VLC_FOURCC('Z','y','G','o'), CODEC_ID_H263,
      VIDEO_ES, "ITU H263+" },

    /* H263i */
    { VLC_FOURCC('I','2','6','3'), CODEC_ID_H263I,
      VIDEO_ES, "I263.I" },
    { VLC_FOURCC('i','2','6','3'), CODEC_ID_H263I,
      VIDEO_ES, "I263.I" },

    /* H263P */
    { VLC_FOURCC('v','i','v','1'), CODEC_ID_H263P,
      VIDEO_ES, "H263+" },
    { VLC_FOURCC('v','i','v','O'), CODEC_ID_H263P,
      VIDEO_ES, "H263+" },
    { VLC_FOURCC('v','i','v','2'), CODEC_ID_H263P,
      VIDEO_ES, "H263+" },
    { VLC_FOURCC('U','2','6','3'), CODEC_ID_H263P,
      VIDEO_ES, "UB H263+" },
    { VLC_FOURCC('I','L','V','R'), CODEC_ID_H263P,
      VIDEO_ES, "ITU H263+" },

    /* Flash (H263) variant */
    { VLC_FOURCC('F','L','V','1'), CODEC_ID_FLV1,
      VIDEO_ES, "Flash Video" },

#if LIBAVCODEC_BUILD > 4716
    { VLC_FOURCC('H','2','6','1'), CODEC_ID_H261,
      VIDEO_ES, "H.261" },
    { VLC_FOURCC('h','2','6','1'), CODEC_ID_H261,
      VIDEO_ES, "H.261" },
#endif

#if LIBAVCODEC_BUILD > 4680
    { VLC_FOURCC('F','L','I','C'), CODEC_ID_FLIC,
      VIDEO_ES, "Flic Video" },
#endif

    /* MJPEG */
    { VLC_FOURCC( 'M', 'J', 'P', 'G' ), CODEC_ID_MJPEG,
      VIDEO_ES, "Motion JPEG Video" },
    { VLC_FOURCC( 'm', 'j', 'p', 'g' ), CODEC_ID_MJPEG,
      VIDEO_ES, "Motion JPEG Video" },
    { VLC_FOURCC( 'm', 'j', 'p', 'a' ), CODEC_ID_MJPEG, /* for mov file */
      VIDEO_ES, "Motion JPEG Video" },
    { VLC_FOURCC( 'j', 'p', 'e', 'g' ), CODEC_ID_MJPEG,
      VIDEO_ES, "Motion JPEG Video" },
    { VLC_FOURCC( 'J', 'P', 'E', 'G' ), CODEC_ID_MJPEG,
      VIDEO_ES, "Motion JPEG Video" },
    { VLC_FOURCC( 'J', 'F', 'I', 'F' ), CODEC_ID_MJPEG,
      VIDEO_ES, "Motion JPEG Video" },
    { VLC_FOURCC( 'J', 'P', 'G', 'L' ), CODEC_ID_MJPEG,
      VIDEO_ES, "Motion JPEG Video" },

    { VLC_FOURCC( 'm', 'j', 'p', 'b' ), CODEC_ID_MJPEGB, /* for mov file */
      VIDEO_ES, "Motion JPEG B Video" },
      
    { VLC_FOURCC( 'L','J','P','G' ), CODEC_ID_LJPEG,
      VIDEO_ES, "Lead Motion JPEG Video" },
    { VLC_FOURCC( 'L','J','P','G' ), CODEC_ID_MJPEG,
      VIDEO_ES, "Lead Motion JPEG Video" },
    /* AVID MJPEG */
    { VLC_FOURCC( 'A','V','R','n' ), CODEC_ID_MJPEG,
      VIDEO_ES, "Avid Motion JPEG" },
    { VLC_FOURCC( 'A','D','J','V' ), CODEC_ID_MJPEG,
      VIDEO_ES, "Avid Motion JPEG" },
    { VLC_FOURCC( 'd','m','b','1' ), CODEC_ID_MJPEG,
      VIDEO_ES, "Motion JPEG OpenDML Video" },

#if LIBAVCODEC_BUILD > 4680
    { VLC_FOURCC( 'S', 'P', '5', 'X' ), CODEC_ID_SP5X,
      VIDEO_ES, "Sunplus Motion JPEG Video" },
    { VLC_FOURCC( 'S','P','5','3' ), CODEC_ID_SP5X,
      VIDEO_ES, "Sunplus Motion JPEG Video" },
    { VLC_FOURCC( 'S','P','5','4' ), CODEC_ID_SP5X,
      VIDEO_ES, "Sunplus Motion JPEG Video" },
    { VLC_FOURCC( 'S','P','5','5' ), CODEC_ID_SP5X,
      VIDEO_ES, "Sunplus Motion JPEG Video" },
    { VLC_FOURCC( 'S','P','5','6' ), CODEC_ID_SP5X,
      VIDEO_ES, "Sunplus Motion JPEG Video" },
    { VLC_FOURCC( 'S','P','5','7' ), CODEC_ID_SP5X,
      VIDEO_ES, "Sunplus Motion JPEG Video" },
    { VLC_FOURCC( 'S','P','5','8' ), CODEC_ID_SP5X,
      VIDEO_ES, "Sunplus Motion JPEG Video" },
#endif

    /* DV */
    { VLC_FOURCC('d','v','s','l'), CODEC_ID_DVVIDEO,
      VIDEO_ES, "DV Video" },
    { VLC_FOURCC('d','v','s','d'), CODEC_ID_DVVIDEO,
      VIDEO_ES, "DV Video" },
    { VLC_FOURCC('D','V','S','D'), CODEC_ID_DVVIDEO,
      VIDEO_ES, "DV Video" },
    { VLC_FOURCC('d','v','h','d'), CODEC_ID_DVVIDEO,
      VIDEO_ES, "DV Video" },
    { VLC_FOURCC('d','v','c',' '), CODEC_ID_DVVIDEO,
      VIDEO_ES, "DV Video" },
    { VLC_FOURCC('d','v','c','p'), CODEC_ID_DVVIDEO,
      VIDEO_ES, "DV Video PAL" },
    { VLC_FOURCC('d','v','p',' '), CODEC_ID_DVVIDEO,
      VIDEO_ES, "DV Video Pro" },
    { VLC_FOURCC('d','v','p','p'), CODEC_ID_DVVIDEO,
      VIDEO_ES, "DV Video Pro PAL" },
    { VLC_FOURCC('C','D','V','C'), CODEC_ID_DVVIDEO,
      VIDEO_ES, "DV Video" },
    { VLC_FOURCC('c','d','v','c'), CODEC_ID_DVVIDEO,
      VIDEO_ES, "DV Video" },
    { VLC_FOURCC('d','v','5','p'), CODEC_ID_DVVIDEO,
      VIDEO_ES, "DV Video C Pro 50 PAL" },
    { VLC_FOURCC('d','v','5','n'), CODEC_ID_DVVIDEO,
      VIDEO_ES, "DV Video C Pro 50 NTSC" },
    { VLC_FOURCC('A','V','d','v'), CODEC_ID_DVVIDEO,
      VIDEO_ES, "AVID DV" },

    /* Windows Media Video */
    { VLC_FOURCC('W','M','V','1'), CODEC_ID_WMV1,
      VIDEO_ES, "Windows Media Video 1" },
    { VLC_FOURCC('w','m','v','1'), CODEC_ID_WMV1,
      VIDEO_ES, "Windows Media Video 1" },
    { VLC_FOURCC('W','M','V','2'), CODEC_ID_WMV2,
      VIDEO_ES, "Windows Media Video 2" },
    { VLC_FOURCC('w','m','v','2'), CODEC_ID_WMV2,
      VIDEO_ES, "Windows Media Video 2" },
#if LIBAVCODEC_BUILD >= ((51<<16)+(10<<8)+1)
    { VLC_FOURCC('W','M','V','3'), CODEC_ID_WMV3,
      VIDEO_ES, "Windows Media Video 3" },
    { VLC_FOURCC('w','m','v','3'), CODEC_ID_WMV3,
      VIDEO_ES, "Windows Media Video 3" },
    { VLC_FOURCC('W','V','C','1'), CODEC_ID_VC1,
      VIDEO_ES, "Windows Media Video VC1" },
    { VLC_FOURCC('w','v','c','1'), CODEC_ID_VC1,
      VIDEO_ES, "Windows Media Video VC1" },
#endif
#if 0
    /* WMVA is the VC-1 codec before the standardization proces,
       it is not bitstream compatible and deprecated  */
    { VLC_FOURCC('W','M','V','A'), CODEC_ID_VC1,
      VIDEO_ES, "Windows Media Video Advanced Profile" },
#endif

#if LIBAVCODEC_BUILD >= 4683
    /* Microsoft Video 1 */
    { VLC_FOURCC('M','S','V','C'), CODEC_ID_MSVIDEO1,
      VIDEO_ES, "Microsoft Video 1" },
    { VLC_FOURCC('m','s','v','c'), CODEC_ID_MSVIDEO1,
      VIDEO_ES, "Microsoft Video 1" },
    { VLC_FOURCC('C','R','A','M'), CODEC_ID_MSVIDEO1,
      VIDEO_ES, "Microsoft Video 1" },
    { VLC_FOURCC('c','r','a','m'), CODEC_ID_MSVIDEO1,
      VIDEO_ES, "Microsoft Video 1" },
    { VLC_FOURCC('W','H','A','M'), CODEC_ID_MSVIDEO1,
      VIDEO_ES, "Microsoft Video 1" },
    { VLC_FOURCC('w','h','a','m'), CODEC_ID_MSVIDEO1,
      VIDEO_ES, "Microsoft Video 1" },

    /* Microsoft RLE */
    { VLC_FOURCC('m','r','l','e'), CODEC_ID_MSRLE,
      VIDEO_ES, "Microsoft RLE Video" },
    { VLC_FOURCC('W','R','L','E'), CODEC_ID_MSRLE,
      VIDEO_ES, "Microsoft RLE Video" },
    { VLC_FOURCC(0x1,0x0,0x0,0x0), CODEC_ID_MSRLE,
      VIDEO_ES, "Microsoft RLE Video" },
    { VLC_FOURCC(0x2,0x0,0x0,0x0), CODEC_ID_MSRLE,
      VIDEO_ES, "Microsoft RLE Video" },
#endif

    /* Indeo Video Codecs (Quality of this decoder on ppc is not good) */
    { VLC_FOURCC('I','V','3','1'), CODEC_ID_INDEO3,
      VIDEO_ES, "Indeo Video v3" },
    { VLC_FOURCC('i','v','3','1'), CODEC_ID_INDEO3,
      VIDEO_ES, "Indeo Video v3" },
    { VLC_FOURCC('I','V','3','2'), CODEC_ID_INDEO3,
      VIDEO_ES, "Indeo Video v3" },
    { VLC_FOURCC('i','v','3','2'), CODEC_ID_INDEO3,
      VIDEO_ES, "Indeo Video v3" },

#if LIBAVCODEC_BUILD >= 4721
    { VLC_FOURCC('t','s','c','c'), CODEC_ID_TSCC,
      VIDEO_ES, "TechSmith Camtasia Screen Capture Video" },
#endif

    /* Huff YUV */
    { VLC_FOURCC('H','F','Y','U'), CODEC_ID_HUFFYUV,
      VIDEO_ES, "Huff YUV Video" },
    { VLC_FOURCC('F','F','V','H'), CODEC_ID_FFVHUFF,
      VIDEO_ES, "Huff YUV Video" },

    /* Creative YUV */
    { VLC_FOURCC('C','Y','U','V'), CODEC_ID_CYUV,
      VIDEO_ES, "Creative YUV Video" },
    { VLC_FOURCC('c','y','u','v'), CODEC_ID_CYUV,
      VIDEO_ES, "Creative YUV Video" },

    /* On2 VP3 Video Codecs */
    { VLC_FOURCC('V','P','3',' '), CODEC_ID_VP3,
      VIDEO_ES, "On2's VP3 Video" },
    { VLC_FOURCC('V','P','3','0'), CODEC_ID_VP3,
      VIDEO_ES, "On2's VP3 Video" },
    { VLC_FOURCC('V','P','3','1'), CODEC_ID_VP3,
      VIDEO_ES, "On2's VP3 Video" },
    { VLC_FOURCC('v','p','3','1'), CODEC_ID_VP3,
      VIDEO_ES, "On2's VP3 Video" },

#if LIBAVCODEC_BUILD >= ((51<<16)+(14<<8)+0)
    { VLC_FOURCC('V','P','5',' '), CODEC_ID_VP5,
      VIDEO_ES, "On2's VP5 Video" },
    { VLC_FOURCC('V','P','5','0'), CODEC_ID_VP5,
      VIDEO_ES, "On2's VP5 Video" },
    { VLC_FOURCC('V','P','6','2'), CODEC_ID_VP6,
      VIDEO_ES, "On2's VP6.2 Video" },
    { VLC_FOURCC('v','p','6','2'), CODEC_ID_VP6,
      VIDEO_ES, "On2's VP6.2 Video" },
    { VLC_FOURCC('V','P','6','F'), CODEC_ID_VP6F,
      VIDEO_ES, "On2's VP6.2 Video (Flash)" },
#endif

#if LIBAVCODEC_VERSION_INT >= ((51<<16)+(27<<8)+0)
    { VLC_FOURCC('V','P','6','0'), CODEC_ID_VP6,
      VIDEO_ES, "On2's VP6.0 Video" },
    { VLC_FOURCC('V','P','6','1'), CODEC_ID_VP6,
      VIDEO_ES, "On2's VP6.1 Video" },
#endif

#if LIBAVCODEC_VERSION_INT >= ((51<<16)+(47<<8)+0)
    { VLC_FOURCC('V','P','6','A'), CODEC_ID_VP6A,
      VIDEO_ES, "On2's VP6 A Video" },
#endif

#if LIBAVCODEC_BUILD >= 4685
    /* Xiph.org theora */
    { VLC_FOURCC('t','h','e','o'), CODEC_ID_THEORA,
      VIDEO_ES, "Xiph.org's Theora Video" },
    { VLC_FOURCC('T','h','r','a'), CODEC_ID_THEORA,
      VIDEO_ES, "Xiph.org's Theora Video" },
#endif

#if ( !defined( WORDS_BIGENDIAN ) )
    /* Asus Video (Another thing that doesn't work on PPC) */
    { VLC_FOURCC('A','S','V','1'), CODEC_ID_ASV1,
      VIDEO_ES, "Asus V1 Video" },
    { VLC_FOURCC('A','S','V','2'), CODEC_ID_ASV2,
      VIDEO_ES, "Asus V2 Video" },
#endif

    /* FFMPEG Video 1 (lossless codec) */
    { VLC_FOURCC('F','F','V','1'), CODEC_ID_FFV1,
      VIDEO_ES, "FFMpeg Video 1" },

    /* ATI VCR1 */
    { VLC_FOURCC('V','C','R','1'), CODEC_ID_VCR1,
      VIDEO_ES, "ATI VCR1 Video" },

    /* Cirrus Logic AccuPak */
    { VLC_FOURCC('C','L','J','R'), CODEC_ID_CLJR,
      VIDEO_ES, "Creative Logic AccuPak" },

    /* Real Video */
    { VLC_FOURCC('R','V','1','0'), CODEC_ID_RV10,
      VIDEO_ES, "Real Video 10" },
    { VLC_FOURCC('r','v','1','0'), CODEC_ID_RV10,
      VIDEO_ES, "Real Video 10" },
    { VLC_FOURCC('R','V','1','3'), CODEC_ID_RV10,
      VIDEO_ES, "Real Video 13" },
    { VLC_FOURCC('r','v','1','3'), CODEC_ID_RV10,
      VIDEO_ES, "Real Video 13" },
#if LIBAVCODEC_BUILD >= ((51<<16)+(15<<8)+1)
    { VLC_FOURCC('R','V','2','0'), CODEC_ID_RV20,
      VIDEO_ES, "Real Video 20" },
    { VLC_FOURCC('r','v','2','0'), CODEC_ID_RV20,
      VIDEO_ES, "Real Video 20" },
#endif

#if LIBAVCODEC_BUILD >= 4684
    /* Apple Video */
    { VLC_FOURCC('r','p','z','a'), CODEC_ID_RPZA,
      VIDEO_ES, "Apple Video" },
    { VLC_FOURCC('a','z','p','r'), CODEC_ID_RPZA,
      VIDEO_ES, "Apple Video" },

    { VLC_FOURCC('s','m','c',' '), CODEC_ID_SMC,
      VIDEO_ES, "Apple graphics" },

    /* Cinepak */
    { VLC_FOURCC('C','V','I','D'), CODEC_ID_CINEPAK,
      VIDEO_ES, "Cinepak Video" },
    { VLC_FOURCC('c','v','i','d'), CODEC_ID_CINEPAK,
      VIDEO_ES, "Cinepak Video" },

    /* Id Quake II CIN */
    { VLC_FOURCC('I','D','C','I'), CODEC_ID_IDCIN,
      VIDEO_ES, "Id Quake II CIN Video" },

    /* Screen Capture Video Codecs */
    { VLC_FOURCC('t','s','c','c'), CODEC_ID_TSCC,
      VIDEO_ES, "TechSmith Camtasia Screen Capture Video" },
    { VLC_FOURCC('T','S','C','C'), CODEC_ID_TSCC,
      VIDEO_ES, "TechSmith Camtasia Screen Capture Video" },

    { VLC_FOURCC('C','S','C','D'), CODEC_ID_CSCD,
      VIDEO_ES, "CamStudio Screen Codec" },
    { VLC_FOURCC('c','s','c','d'), CODEC_ID_CSCD,
      VIDEO_ES, "CamStudio Screen Codec" },

    { VLC_FOURCC('Z','M','B','V'), CODEC_ID_ZMBV,
      VIDEO_ES, "DosBox Capture Codec" },

    { VLC_FOURCC('F','P','S','1'), CODEC_ID_FRAPS,
      VIDEO_ES, "FRAPS: Realtime Video Capture" },
    { VLC_FOURCC('f','p','s','1'), CODEC_ID_FRAPS,
      VIDEO_ES, "FRAPS: Realtime Video Capture" },
    /* 4X Technologies */
    { VLC_FOURCC('4','x','m','v'), CODEC_ID_4XM,
      VIDEO_ES, "4X Technologies Video" },

#endif
#if LIBAVCODEC_VERSION_INT >= ((51<<16)+(13<<8)+0)
    { VLC_FOURCC('V','M','n','c'), CODEC_ID_VMNC,
      VIDEO_ES, "VMware Video" },
#endif

#if LIBAVCODEC_BUILD >= 4694
    /* Duck TrueMotion */
    { VLC_FOURCC('D','U','C','K'), CODEC_ID_TRUEMOTION1,
      VIDEO_ES, "Duck TrueMotion v1 Video" },
    { VLC_FOURCC('T','M','2','0'), CODEC_ID_TRUEMOTION2,
      VIDEO_ES, "Duck TrueMotion v2.0 Video" },
#endif

    /* Interplay MVE */
    { VLC_FOURCC('i','m','v','e'), CODEC_ID_INTERPLAY_VIDEO,
      VIDEO_ES, "Interplay MVE Video" },

    /* Id RoQ */
    { VLC_FOURCC('R','o','Q','v'), CODEC_ID_ROQ,
      VIDEO_ES, "Id RoQ Video" },

    /* Sony Playstation MDEC */
    { VLC_FOURCC('M','D','E','C'), CODEC_ID_MDEC,
      VIDEO_ES, "PSX MDEC Video" },

#if LIBAVCODEC_BUILD >= 4699
    /* Sierra VMD */
    { VLC_FOURCC('v','m','d','v'), CODEC_ID_VMDVIDEO,
      VIDEO_ES, "Sierra VMD Video" },
#endif

#if LIBAVCODEC_BUILD >= 4719
    /* FFMPEG's SNOW wavelet codec */
    { VLC_FOURCC('S','N','O','W'), CODEC_ID_SNOW,
      VIDEO_ES, "FFMpeg SNOW wavelet Video" },
    { VLC_FOURCC('s','n','o','w'), CODEC_ID_SNOW,
      VIDEO_ES, "FFMpeg SNOW wavelet Video" },
#endif

#if LIBAVCODEC_BUILD >= 4752
    { VLC_FOURCC('r','l','e',' '), CODEC_ID_QTRLE,
      VIDEO_ES, "Apple QuickTime RLE Video" },

    { VLC_FOURCC('q','d','r','w'), CODEC_ID_QDRAW,
      VIDEO_ES, "Apple QuickDraw Video" },

    { VLC_FOURCC('Q','P','E','G'), CODEC_ID_QPEG,
      VIDEO_ES, "QPEG Video" },
    { VLC_FOURCC('Q','1','.','0'), CODEC_ID_QPEG,
      VIDEO_ES, "QPEG Video" },
    { VLC_FOURCC('Q','1','.','1'), CODEC_ID_QPEG,
      VIDEO_ES, "QPEG Video" },

    { VLC_FOURCC('U','L','T','I'), CODEC_ID_ULTI,
      VIDEO_ES, "IBM Ultimotion Video" },

    { VLC_FOURCC('V','I','X','L'), CODEC_ID_VIXL,
      VIDEO_ES, "Miro/Pinnacle VideoXL Video" },
    { VLC_FOURCC('P','I','X','L'), CODEC_ID_VIXL,
      VIDEO_ES, "Pinnacle VideoXL Video" },

    { VLC_FOURCC('L','O','C','O'), CODEC_ID_LOCO,
      VIDEO_ES, "LOCO Video" },

    { VLC_FOURCC('W','N','V','1'), CODEC_ID_WNV1,
      VIDEO_ES, "Winnov WNV1 Video" },

    { VLC_FOURCC('A','A','S','C'), CODEC_ID_AASC,
      VIDEO_ES, "Autodesc RLE Video" },
#endif
#if LIBAVCODEC_BUILD >= 4753
    { VLC_FOURCC('I','V','2','0'), CODEC_ID_INDEO2,
      VIDEO_ES, "Indeo Video v2" },
    { VLC_FOURCC('R','T','2','1'), CODEC_ID_INDEO2,
      VIDEO_ES, "Indeo Video v2" },
#endif

        /* Flash Screen Video */
#if LIBAVCODEC_VERSION_INT >= ((51<<16)+(11<<8)+0)
    { VLC_FOURCC('F','S','V','1'), CODEC_ID_FLASHSV,
              VIDEO_ES, "Flash Screen Video" },
#endif
   { VLC_FOURCC('K','M','V','C'), CODEC_ID_KMVC,
      VIDEO_ES, "Karl Morton's Video Codec (Worms)" },

#if LIBAVCODEC_VERSION_INT >= ((51<<16)+(13<<8)+0)
    { VLC_FOURCC('N','U','V','1'), CODEC_ID_NUV,
      VIDEO_ES, "Nuppel Video" },
    { VLC_FOURCC('R','J','P','G'), CODEC_ID_NUV,
      VIDEO_ES, "Nuppel Video" },
#endif

#if LIBAVCODEC_VERSION_INT >= ((51<<16)+(8<<8)+0)
    /* CODEC_ID_SMACKVIDEO */
    { VLC_FOURCC('S','M','K','2'), CODEC_ID_SMACKVIDEO,
      VIDEO_ES, "Smacker Video" },
    { VLC_FOURCC('S','M','K','4'), CODEC_ID_SMACKVIDEO,
      VIDEO_ES, "Smacker Video" },
#endif

#if LIBAVCODEC_BUILD >= ((51<<16)+(13<<8)+0)
    { VLC_FOURCC('V','M','n','c'), CODEC_ID_VMNC,
      VIDEO_ES, "VMware Video" },
#endif

    /* Chinese AVS - Untested */
#if LIBAVCODEC_VERSION_INT >= ((51<<16)+(8<<8)+0)
    { VLC_FOURCC('C','A','V','S'), CODEC_ID_CAVS,
      VIDEO_ES, "Chinese AVS" },
#endif

#if LIBAVCODEC_VERSION_INT >= ((51<<16)+(40<<8)+0)
    /* Untested yet */
    { VLC_FOURCC('A','V','d','n'), CODEC_ID_DNXHD,
      VIDEO_ES, "DNxHD" },
#endif
    { VLC_FOURCC('8','B','P','S'), CODEC_ID_8BPS,
      VIDEO_ES, "8BPS" },

#if LIBAVCODEC_VERSION_INT >= ((51<<16)+(52<<8)+0)
    { VLC_FOURCC('M','L','2','O'), CODEC_ID_MIMIC,
      VIDEO_ES, "Mimic" },
#endif

    /* Videogames Codecs */

    /* Interplay MVE */
    { VLC_FOURCC('i','m','v','e'), CODEC_ID_INTERPLAY_VIDEO,
      VIDEO_ES, "Interplay MVE Video" },
    { VLC_FOURCC('I','N','P','V'), CODEC_ID_INTERPLAY_VIDEO,
      VIDEO_ES, "Interplay MVE Video" },

    /* Id Quake II CIN */
    { VLC_FOURCC('I','D','C','I'), CODEC_ID_IDCIN,
      VIDEO_ES, "Id Quake II CIN Video" },

    /* 4X Technologies */
    { VLC_FOURCC('4','x','m','v'), CODEC_ID_4XM,
      VIDEO_ES, "4X Technologies Video" },
    { VLC_FOURCC('4','X','M','V'), CODEC_ID_4XM,
      VIDEO_ES, "4X Technologies Video" },

    /* Id RoQ */
    { VLC_FOURCC('R','o','Q','v'), CODEC_ID_ROQ,
      VIDEO_ES, "Id RoQ Video" },

    /* Sony Playstation MDEC */
    { VLC_FOURCC('M','D','E','C'), CODEC_ID_MDEC,
      VIDEO_ES, "PSX MDEC Video" },

    /* Sierra VMD */
    { VLC_FOURCC('v','m','d','v'), CODEC_ID_VMDVIDEO,
      VIDEO_ES, "Sierra VMD Video" },
    { VLC_FOURCC('V','M','D','V'), CODEC_ID_VMDVIDEO,
      VIDEO_ES, "Sierra VMD Video" },

    /*
     *  Image codecs
     */

#if LIBAVCODEC_BUILD >= 4731
    { VLC_FOURCC('p','n','g',' '), CODEC_ID_PNG,
      VIDEO_ES, "PNG Image" },
    { VLC_FOURCC('p','p','m',' '), CODEC_ID_PPM,
      VIDEO_ES, "PPM Image" },
    { VLC_FOURCC('p','g','m',' '), CODEC_ID_PGM,
      VIDEO_ES, "PGM Image" },
    { VLC_FOURCC('p','g','m','y'), CODEC_ID_PGMYUV,
      VIDEO_ES, "PGM YUV Image" },
    { VLC_FOURCC('p','a','m',' '), CODEC_ID_PAM,
      VIDEO_ES, "PAM Image" },
    { VLC_FOURCC('M','J','L','S'), CODEC_ID_JPEGLS,
      VIDEO_ES, "PAM Image" },
#endif

#if LIBAVCODEC_BUILD >= ((51<<16)+(0<<8)+0)
    { VLC_FOURCC('b','m','p',' '), CODEC_ID_BMP,
      VIDEO_ES, "BMP Image" },
#endif

#if LIBAVCODEC_VERSION_INT >= ((51<<16)+(40<<8)+1)
    { VLC_FOURCC('t','i','f','f'), CODEC_ID_TIFF,
      VIDEO_ES, "TIFF Image" },
    { VLC_FOURCC('g','i','f',' '), CODEC_ID_GIF,
      VIDEO_ES, "GIF Image" },
    { VLC_FOURCC('t','g','a',' '), CODEC_ID_TARGA,
      VIDEO_ES, "Truevision Targa Image" },
    { VLC_FOURCC('m','t','g','a'), CODEC_ID_TARGA,
      VIDEO_ES, "Truevision Targa Image" },
    { VLC_FOURCC('M','T','G','A'), CODEC_ID_TARGA,
      VIDEO_ES, "Truevision Targa Image" },
#endif
#if LIBAVCODEC_VERSION_INT >= ((51<<16)+(40<<8)+3)
    { VLC_FOURCC('s','g','i',' '), CODEC_ID_SGI,
      VIDEO_ES, "SGI Image" },
#endif

    /*
     *  Audio Codecs
     */

    /* Windows Media Audio 1 */
    { VLC_FOURCC('W','M','A','1'), CODEC_ID_WMAV1,
      AUDIO_ES, "Windows Media Audio 1" },
    { VLC_FOURCC('w','m','a','1'), CODEC_ID_WMAV1,
      AUDIO_ES, "Windows Media Audio 1" },

    /* Windows Media Audio 2 */
    { VLC_FOURCC('W','M','A','2'), CODEC_ID_WMAV2,
      AUDIO_ES, "Windows Media Audio 2" },
    { VLC_FOURCC('w','m','a','2'), CODEC_ID_WMAV2,
      AUDIO_ES, "Windows Media Audio 2" },

    /* DV Audio */
    { VLC_FOURCC('d','v','a','u'), CODEC_ID_DVAUDIO,
      AUDIO_ES, "DV Audio" },

    /* MACE-3 Audio */
    { VLC_FOURCC('M','A','C','3'), CODEC_ID_MACE3,
      AUDIO_ES, "MACE-3 Audio" },

    /* MACE-6 Audio */
    { VLC_FOURCC('M','A','C','6'), CODEC_ID_MACE6,
      AUDIO_ES, "MACE-6 Audio" },

#if LIBAVCODEC_VERSION_INT >= ((51<<16)+(41<<8)+1)
    /* MUSEPACK7 Audio */
    { VLC_FOURCC('M','P','C',' '), CODEC_ID_MUSEPACK7,
      AUDIO_ES, "MUSEPACK7 Audio" },
#endif
#if LIBAVCODEC_VERSION_INT >= ((51<<16)+(48<<8)+0)
    /* MUSEPACK8 Audio */
    { VLC_FOURCC('M','P','C','K'), CODEC_ID_MUSEPACK8,
      AUDIO_ES, "MUSEPACK8 Audio" },
    { VLC_FOURCC('M','P','C','8'), CODEC_ID_MUSEPACK8,
      AUDIO_ES, "MUSEPACK8 Audio" },
#endif

    /* RealAudio 1.0 */
    { VLC_FOURCC('1','4','_','4'), CODEC_ID_RA_144,
      AUDIO_ES, "RealAudio 1.0" },
    { VLC_FOURCC('l','p','c','J'), CODEC_ID_RA_144,
      AUDIO_ES, "RealAudio 1.0" },

    /* RealAudio 2.0 */
    { VLC_FOURCC('2','8','_','8'), CODEC_ID_RA_288,
      AUDIO_ES, "RealAudio 2.0" },

    /* MPEG Audio layer 1/2/3 */
    { VLC_FOURCC('m','p','g','a'), CODEC_ID_MP2,
      AUDIO_ES, "MPEG Audio layer 1/2" },
    { VLC_FOURCC('m','p','3',' '), CODEC_ID_MP3,
      AUDIO_ES, "MPEG Audio layer 1/2/3" },
    { VLC_FOURCC('.','m','p','3'), CODEC_ID_MP3,
      AUDIO_ES, "MPEG Audio layer 1/2/3" },
    { VLC_FOURCC('M','P','3',' '), CODEC_ID_MP3,
      AUDIO_ES, "MPEG Audio layer 1/2/3" },
    { VLC_FOURCC('L','A','M','E'), CODEC_ID_MP3,
      AUDIO_ES, "MPEG Audio layer 1/2/3" },

    /* A52 Audio (aka AC3) */
    { VLC_FOURCC('a','5','2',' '), CODEC_ID_AC3,
      AUDIO_ES, "A52 Audio (aka AC3)" },
    { VLC_FOURCC('a','5','2','b'), CODEC_ID_AC3, /* VLC specific hack */
      AUDIO_ES, "A52 Audio (aka AC3)" },

#if LIBAVCODEC_BUILD >= 4719
    /* DTS Audio */
    { VLC_FOURCC('d','t','s',' '), CODEC_ID_DTS,
      AUDIO_ES, "DTS Audio" },
#endif

    /* AAC audio */
    { VLC_FOURCC('m','p','4','a'), CODEC_ID_AAC,
      AUDIO_ES, "MPEG AAC Audio" },

    /* AC-3 Audio (Dolby Digital) */
    { VLC_FOURCC('a','c','-','3'), CODEC_ID_AC3,
      AUDIO_ES, "AC-3 Audio (Dolby Digital)" },

    /* 4X Technologies */
    { VLC_FOURCC('4','x','m','a'), CODEC_ID_ADPCM_4XM,
      AUDIO_ES, "4X Technologies Audio" },

    /* EA ADPCM */
    { VLC_FOURCC('A','D','E','A'), CODEC_ID_ADPCM_EA,
      AUDIO_ES, "EA ADPCM Audio" },

    /* Interplay DPCM */
    { VLC_FOURCC('i','d','p','c'), CODEC_ID_INTERPLAY_DPCM,
      AUDIO_ES, "Interplay DPCM Audio" },

    /* Id RoQ */
    { VLC_FOURCC('R','o','Q','a'), CODEC_ID_ROQ_DPCM,
      AUDIO_ES, "Id RoQ DPCM Audio" },

#if LIBAVCODEC_VERSION_INT >= ((51<<16)+(27<<8)+0)
    /* DCIN Audio */
    { VLC_FOURCC('D','C','I','A'), CODEC_ID_DSICINAUDIO,
      AUDIO_ES, "Delphine CIN Audio" },
#endif

#if LIBAVCODEC_BUILD >= 4685
    /* Sony Playstation XA ADPCM */
    { VLC_FOURCC('x','a',' ',' '), CODEC_ID_ADPCM_XA,
      AUDIO_ES, "PSX XA ADPCM Audio" },

    /* ADX ADPCM */
    { VLC_FOURCC('a','d','x',' '), CODEC_ID_ADPCM_ADX,
      AUDIO_ES, "ADX ADPCM Audio" },
#endif

    /* Westwood ADPCM */
    { VLC_FOURCC('A','I','W','S'), CODEC_ID_ADPCM_IMA_WS,
      AUDIO_ES, "Westwood IMA ADPCM audio" },

#if LIBAVCODEC_BUILD >= 4699
    /* Sierra VMD */
    { VLC_FOURCC('v','m','d','a'), CODEC_ID_VMDAUDIO,
      AUDIO_ES, "Sierra VMD Audio" },
#endif

#if LIBAVCODEC_BUILD >= 4706
    /* G.726 ADPCM */
    { VLC_FOURCC('g','7','2','6'), CODEC_ID_ADPCM_G726,
      AUDIO_ES, "G.726 ADPCM Audio" },
#endif

#if LIBAVCODEC_BUILD >= 4683
    /* AMR */
    { VLC_FOURCC('s','a','m','r'), CODEC_ID_AMR_NB,
      AUDIO_ES, "AMR narrow band" },
    { VLC_FOURCC('s','a','w','b'), CODEC_ID_AMR_WB,
      AUDIO_ES, "AMR wide band" },
#endif

#if LIBAVCODEC_BUILD >= 4703
    /* FLAC */
    { VLC_FOURCC('f','l','a','c'), CODEC_ID_FLAC,
      AUDIO_ES, "FLAC (Free Lossless Audio Codec)" },
#endif

#if LIBAVCODEC_BUILD >= 4745
    /* ALAC */
    { VLC_FOURCC('a','l','a','c'), CODEC_ID_ALAC,
      AUDIO_ES, "Apple Lossless Audio Codec" },
#endif

#if LIBAVCODEC_BUILD >= ((50<<16)+(0<<8)+1)
    /* QDM2 */
    { VLC_FOURCC('Q','D','M','2'), CODEC_ID_QDM2,
      AUDIO_ES, "QDM2 Audio" },
#endif

#if LIBAVCODEC_BUILD >= ((51<<16)+(0<<8)+0)
    /* COOK */
    { VLC_FOURCC('c','o','o','k'), CODEC_ID_COOK,
      AUDIO_ES, "Cook Audio" },
#endif

#if LIBAVCODEC_BUILD >= ((51<<16)+(4<<8)+0)
    /* TTA: The Lossless True Audio */
    { VLC_FOURCC('T','T','A','1'), CODEC_ID_TTA,
      AUDIO_ES, "The Lossless True Audio" },
#endif

#if LIBAVCODEC_BUILD >= ((51<<16)+(8<<8)+0)
    /* Shorten */
    { VLC_FOURCC('s','h','n',' '), CODEC_ID_SHORTEN,
      AUDIO_ES, "Shorten Lossless Audio" },
#endif

#if LIBAVCODEC_BUILD >= ((51<<16)+(16<<8)+0)
    /* WavPack */
    { VLC_FOURCC('w','v','p','k'), CODEC_ID_WAVPACK,
      AUDIO_ES, "WavPack" },
    { VLC_FOURCC('W','V','P','K'), CODEC_ID_WAVPACK,
      AUDIO_ES, "WavPack" },
#endif

#if LIBAVCODEC_VERSION_INT >= ((51<<16)+(34<<8)+0)
    { VLC_FOURCC('g','s','m',' '), CODEC_ID_GSM,
      AUDIO_ES, "GSM Audio" },
    { VLC_FOURCC('g','s','m','s'), CODEC_ID_GSM_MS,
      AUDIO_ES, "Microsoft GSM Audio" },
#endif

#if LIBAVCODEC_VERSION_INT >= ((51<<16)+(40<<8)+4)
    { VLC_FOURCC('a','t','r','c'), CODEC_ID_ATRAC3,
      AUDIO_ES, "atrac 3" },
    { VLC_FOURCC(0x70,0x2,0x0,0x0), CODEC_ID_ATRAC3,
      AUDIO_ES, "atrac 3" },
#endif

    { VLC_FOURCC('S','O','N','C'), CODEC_ID_SONIC,
      AUDIO_ES, "Sonic" },

#if LIBAVCODEC_VERSION_INT >= ((51<<16)+(34<<8)+0)
    { VLC_FOURCC(0x1,0x4,0x0,0x0), CODEC_ID_IMC,
      AUDIO_ES, "IMC" },
#endif
    { VLC_FOURCC(0x22,0x0,0x0,0x0), CODEC_ID_TRUESPEECH,
      AUDIO_ES, "TrueSpeech" },
#if LIBAVCODEC_VERSION_INT >= ((51<<16)+(46<<8)+0)
    { VLC_FOURCC('N','E','L','L'), CODEC_ID_NELLYMOSER,
      AUDIO_ES, "NellyMoser ASAO" },
#endif
#if LIBAVCODEC_VERSION_INT >= ((51<<16)+(44<<8)+0)
    { VLC_FOURCC('A','P','E',' '), CODEC_ID_APE,
      AUDIO_ES, "Monkey's Audio" },
#endif

    /* PCM */
    { VLC_FOURCC('s','8',' ',' '), CODEC_ID_PCM_S8,
      AUDIO_ES, "PCM S8" },
    { VLC_FOURCC('u','8',' ',' '), CODEC_ID_PCM_U8,
      AUDIO_ES, "PCM U8" },
    { VLC_FOURCC('s','1','6','l'), CODEC_ID_PCM_S16LE,
      AUDIO_ES, "PCM S16 LE" },
    { VLC_FOURCC('s','1','6','b'), CODEC_ID_PCM_S16BE,
      AUDIO_ES, "PCM S16 BE" },
    { VLC_FOURCC('u','1','6','l'), CODEC_ID_PCM_U16LE,
      AUDIO_ES, "PCM U16 LE" },
    { VLC_FOURCC('u','1','6','b'), CODEC_ID_PCM_U16BE,
      AUDIO_ES, "PCM U16 BE" },
    { VLC_FOURCC('s','2','4','l'), CODEC_ID_PCM_S24LE,
      AUDIO_ES, "PCM S24 LE" },
    { VLC_FOURCC('s','2','4','b'), CODEC_ID_PCM_S24BE,
      AUDIO_ES, "PCM S24 BE" },
    { VLC_FOURCC('u','2','4','l'), CODEC_ID_PCM_U24LE,
      AUDIO_ES, "PCM U24 LE" },
    { VLC_FOURCC('u','2','4','b'), CODEC_ID_PCM_U24BE,
      AUDIO_ES, "PCM U24 BE" },
    { VLC_FOURCC('s','3','2','l'), CODEC_ID_PCM_S32LE,
      AUDIO_ES, "PCM S32 LE" },
    { VLC_FOURCC('s','3','2','b'), CODEC_ID_PCM_S32BE,
      AUDIO_ES, "PCM S32 BE" },
    { VLC_FOURCC('u','3','2','l'), CODEC_ID_PCM_U32LE,
      AUDIO_ES, "PCM U32 LE" },
    { VLC_FOURCC('u','3','2','b'), CODEC_ID_PCM_U32BE,
      AUDIO_ES, "PCM U32 BE" },
    { VLC_FOURCC('a','l','a','w'), CODEC_ID_PCM_ALAW,
      AUDIO_ES, "PCM ALAW" },
    { VLC_FOURCC('u','l','a','w'), CODEC_ID_PCM_MULAW,
      AUDIO_ES, "PCM ULAW" },
    { VLC_FOURCC('d','a','u','d'), CODEC_ID_PCM_S24DAUD,
      AUDIO_ES, "PCM ULAW" },

    {0}
};

int E_(GetFfmpegCodec)( vlc_fourcc_t i_fourcc, int *pi_cat,
                        int *pi_ffmpeg_codec, char **ppsz_name )
{
    int i;

    for( i = 0; codecs_table[i].i_fourcc != 0; i++ )
    {
        if( codecs_table[i].i_fourcc == i_fourcc )
        {
            if( pi_cat ) *pi_cat = codecs_table[i].i_cat;
            if( pi_ffmpeg_codec ) *pi_ffmpeg_codec = codecs_table[i].i_codec;
            if( ppsz_name ) *ppsz_name = codecs_table[i].psz_name;

            return VLC_TRUE;
        }
    }
    return VLC_FALSE;
}

int E_(GetVlcFourcc)( int i_ffmpeg_codec, int *pi_cat,
                      vlc_fourcc_t *pi_fourcc, char **ppsz_name )
{
    int i;

    for( i = 0; codecs_table[i].i_codec != 0; i++ )
    {
        if( codecs_table[i].i_codec == i_ffmpeg_codec )
        {
            if( pi_cat ) *pi_cat = codecs_table[i].i_cat;
            if( pi_fourcc ) *pi_fourcc = codecs_table[i].i_fourcc;
            if( ppsz_name ) *ppsz_name = codecs_table[i].psz_name;

            return VLC_TRUE;
        }
    }
    return VLC_FALSE;
}
