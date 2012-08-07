/*****************************************************************************
 * fourcc.c: libavcodec <-> libvlc conversion routines
 *****************************************************************************
 * Copyright (C) 1999-2009 the VideoLAN team
 * $Id: 292faf7f87db7214f5928287f90b3232159a3d46 $
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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <vlc_common.h>
#include <vlc_codec.h>

#ifdef HAVE_LIBAVCODEC_AVCODEC_H
#   include <libavcodec/avcodec.h>
#else
#   include <avcodec.h>
#endif
#include "avcodec.h"

/*****************************************************************************
 * Codec fourcc -> ffmpeg_id mapping
 *****************************************************************************/
static const struct
{
    vlc_fourcc_t  i_fourcc;
    int  i_codec;
    int  i_cat;
} codecs_table[] =
{
    /*
     * Video Codecs
     */

    { VLC_CODEC_MPGV, CODEC_ID_MPEG2VIDEO, VIDEO_ES },
    { VLC_CODEC_MPGV, CODEC_ID_MPEG1VIDEO, VIDEO_ES },

    { VLC_CODEC_MP4V, CODEC_ID_MPEG4, VIDEO_ES },
    /* 3ivx delta 3.5 Unsupported
     * putting it here gives extreme distorted images
    { VLC_FOURCC('3','I','V','1'), CODEC_ID_MPEG4, VIDEO_ES },
    { VLC_FOURCC('3','i','v','1'), CODEC_ID_MPEG4, VIDEO_ES }, */

    { VLC_CODEC_DIV1, CODEC_ID_MSMPEG4V1, VIDEO_ES },
    { VLC_CODEC_DIV2, CODEC_ID_MSMPEG4V2, VIDEO_ES },
    { VLC_CODEC_DIV3, CODEC_ID_MSMPEG4V3, VIDEO_ES },

    { VLC_CODEC_SVQ1, CODEC_ID_SVQ1, VIDEO_ES },
    { VLC_CODEC_SVQ3, CODEC_ID_SVQ3, VIDEO_ES },

    { VLC_CODEC_H264, CODEC_ID_H264, VIDEO_ES },
    { VLC_CODEC_H263, CODEC_ID_H263, VIDEO_ES },
    { VLC_CODEC_H263I,CODEC_ID_H263I,VIDEO_ES },
    { VLC_CODEC_H263P,CODEC_ID_H263P,VIDEO_ES },

    { VLC_CODEC_FLV1, CODEC_ID_FLV1, VIDEO_ES },

    { VLC_CODEC_H261, CODEC_ID_H261, VIDEO_ES },
    { VLC_CODEC_FLIC, CODEC_ID_FLIC, VIDEO_ES },

    { VLC_CODEC_MJPG, CODEC_ID_MJPEG, VIDEO_ES },
    { VLC_CODEC_MJPGB,CODEC_ID_MJPEGB,VIDEO_ES },
    { VLC_CODEC_LJPG, CODEC_ID_LJPEG, VIDEO_ES },

    { VLC_CODEC_SP5X, CODEC_ID_SP5X, VIDEO_ES },

    { VLC_CODEC_DV,   CODEC_ID_DVVIDEO, VIDEO_ES },

    { VLC_CODEC_WMV1, CODEC_ID_WMV1, VIDEO_ES },
    { VLC_CODEC_WMV2, CODEC_ID_WMV2, VIDEO_ES },
    { VLC_CODEC_WMV3, CODEC_ID_WMV3, VIDEO_ES },
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT( 52, 90, 1 )
    { VLC_CODEC_WMVP, CODEC_ID_WMV3, VIDEO_ES },
#endif

    { VLC_CODEC_VC1,  CODEC_ID_VC1, VIDEO_ES },
    { VLC_CODEC_WMVA, CODEC_ID_VC1, VIDEO_ES },

    { VLC_CODEC_MSVIDEO1, CODEC_ID_MSVIDEO1, VIDEO_ES },
    { VLC_CODEC_MSRLE, CODEC_ID_MSRLE, VIDEO_ES },

    { VLC_CODEC_INDEO2, CODEC_ID_INDEO2, VIDEO_ES },
    /* Indeo Video Codecs (Quality of this decoder on ppc is not good) */
    { VLC_CODEC_INDEO3, CODEC_ID_INDEO3, VIDEO_ES },

    { VLC_CODEC_HUFFYUV, CODEC_ID_HUFFYUV, VIDEO_ES },
    { VLC_CODEC_FFVHUFF, CODEC_ID_FFVHUFF, VIDEO_ES },
    { VLC_CODEC_CYUV, CODEC_ID_CYUV, VIDEO_ES },

    { VLC_CODEC_VP3, CODEC_ID_VP3, VIDEO_ES },
    { VLC_CODEC_VP5, CODEC_ID_VP5, VIDEO_ES },
    { VLC_CODEC_VP6, CODEC_ID_VP6, VIDEO_ES },
    { VLC_CODEC_VP6F, CODEC_ID_VP6F, VIDEO_ES },
    { VLC_CODEC_VP6A, CODEC_ID_VP6A, VIDEO_ES },

    { VLC_CODEC_THEORA, CODEC_ID_THEORA, VIDEO_ES },

#if ( !defined( WORDS_BIGENDIAN ) )
    /* Asus Video (Another thing that doesn't work on PPC) */
    { VLC_CODEC_ASV1, CODEC_ID_ASV1, VIDEO_ES },
    { VLC_CODEC_ASV2, CODEC_ID_ASV2, VIDEO_ES },
#endif

    { VLC_CODEC_FFV1, CODEC_ID_FFV1, VIDEO_ES },

    { VLC_CODEC_VCR1, CODEC_ID_VCR1, VIDEO_ES },

    { VLC_CODEC_CLJR, CODEC_ID_CLJR, VIDEO_ES },

    /* Real Video */
    { VLC_CODEC_RV10, CODEC_ID_RV10, VIDEO_ES },
    { VLC_CODEC_RV13, CODEC_ID_RV10, VIDEO_ES },
    { VLC_CODEC_RV20, CODEC_ID_RV20, VIDEO_ES },
    { VLC_CODEC_RV30, CODEC_ID_RV30, VIDEO_ES },
    { VLC_CODEC_RV40, CODEC_ID_RV40, VIDEO_ES },

    { VLC_CODEC_RPZA, CODEC_ID_RPZA, VIDEO_ES },

    { VLC_CODEC_SMC, CODEC_ID_SMC, VIDEO_ES },

    { VLC_CODEC_CINEPAK, CODEC_ID_CINEPAK, VIDEO_ES },

    { VLC_CODEC_TSCC, CODEC_ID_TSCC, VIDEO_ES },

    { VLC_CODEC_CSCD, CODEC_ID_CSCD, VIDEO_ES },

    { VLC_CODEC_ZMBV, CODEC_ID_ZMBV, VIDEO_ES },

    { VLC_CODEC_VMNC, CODEC_ID_VMNC, VIDEO_ES },
    { VLC_CODEC_FRAPS, CODEC_ID_FRAPS, VIDEO_ES },

    { VLC_CODEC_TRUEMOTION1, CODEC_ID_TRUEMOTION1, VIDEO_ES },
    { VLC_CODEC_TRUEMOTION2, CODEC_ID_TRUEMOTION2, VIDEO_ES },

    { VLC_CODEC_SNOW, CODEC_ID_SNOW, VIDEO_ES },

    { VLC_CODEC_QTRLE, CODEC_ID_QTRLE, VIDEO_ES },

    { VLC_CODEC_QDRAW, CODEC_ID_QDRAW, VIDEO_ES },

    { VLC_CODEC_QPEG, CODEC_ID_QPEG, VIDEO_ES },

    { VLC_CODEC_ULTI, CODEC_ID_ULTI, VIDEO_ES },

    { VLC_CODEC_VIXL, CODEC_ID_VIXL, VIDEO_ES },

    { VLC_CODEC_LOCO, CODEC_ID_LOCO, VIDEO_ES },

    { VLC_CODEC_WNV1, CODEC_ID_WNV1, VIDEO_ES },

    { VLC_CODEC_AASC, CODEC_ID_AASC, VIDEO_ES },

    { VLC_CODEC_FLASHSV, CODEC_ID_FLASHSV, VIDEO_ES },
    { VLC_CODEC_KMVC, CODEC_ID_KMVC, VIDEO_ES },

    { VLC_CODEC_NUV, CODEC_ID_NUV, VIDEO_ES },

    { VLC_CODEC_SMACKVIDEO, CODEC_ID_SMACKVIDEO, VIDEO_ES },

    /* Chinese AVS - Untested */
    { VLC_CODEC_CAVS, CODEC_ID_CAVS, VIDEO_ES },

    /* Untested yet */
    { VLC_CODEC_DNXHD, CODEC_ID_DNXHD, VIDEO_ES },
    { VLC_CODEC_8BPS, CODEC_ID_8BPS, VIDEO_ES },

    { VLC_CODEC_MIMIC, CODEC_ID_MIMIC, VIDEO_ES },

    { VLC_CODEC_DIRAC, CODEC_ID_DIRAC, VIDEO_ES },

#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT( 52, 29, 0 )
    { VLC_CODEC_V210, CODEC_ID_V210, VIDEO_ES },
#endif

#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT( 52, 37, 1 )
    { VLC_CODEC_FRWU, CODEC_ID_FRWU, VIDEO_ES },
#endif

#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT( 52, 53, 0 )
    { VLC_CODEC_INDEO5, CODEC_ID_INDEO5, VIDEO_ES },
#endif

#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT( 52, 68, 2 )
    { VLC_CODEC_VP8, CODEC_ID_VP8, VIDEO_ES },
#endif

#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT( 52, 108, 2 )
    { VLC_CODEC_LAGARITH, CODEC_ID_LAGARITH, VIDEO_ES },
#endif

#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT( 52, 117, 0 )
    { VLC_CODEC_MXPEG, CODEC_ID_MXPEG, VIDEO_ES },
#endif

    /* Videogames Codecs */

    { VLC_CODEC_INTERPLAY, CODEC_ID_INTERPLAY_VIDEO, VIDEO_ES },

    { VLC_CODEC_IDCIN, CODEC_ID_IDCIN, VIDEO_ES },

    { VLC_CODEC_4XM, CODEC_ID_4XM, VIDEO_ES },

    { VLC_CODEC_ROQ, CODEC_ID_ROQ, VIDEO_ES },

    { VLC_CODEC_MDEC, CODEC_ID_MDEC, VIDEO_ES },

    { VLC_CODEC_VMDVIDEO, CODEC_ID_VMDVIDEO, VIDEO_ES },

    { VLC_CODEC_AMV, CODEC_ID_AMV, VIDEO_ES },

#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT( 53, 7, 0 )
    { VLC_CODEC_FLASHSV2, CODEC_ID_FLASHSV2, VIDEO_ES },
#endif

#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT( 53, 9, 0 )
    { VLC_CODEC_WMVP, CODEC_ID_WMV3IMAGE, VIDEO_ES },
    { VLC_CODEC_WMVP2, CODEC_ID_VC1IMAGE, VIDEO_ES },
#endif

#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT( 53, 15, 0 )
    { VLC_CODEC_PRORES, CODEC_ID_PRORES, VIDEO_ES },
#endif


#if 0
/*    UNTESTED VideoGames*/
    { VLC_FOURCC('W','C','3','V'), CODEC_ID_XAN_WC3,
      VIDEO_ES, "XAN wc3 Video" },
    { VLC_FOURCC('W','C','4','V'), CODEC_ID_XAN_WC4,
      VIDEO_ES, "XAN wc4 Video" },
    { VLC_FOURCC('S','T','3','C'), CODEC_ID_TXD,
      VIDEO_ES, "Renderware TeXture Dictionary" },
    { VLC_FOURCC('V','Q','A','V'), CODEC_ID_WS_VQA,
      VIDEO_ES, "WestWood Vector Quantized Animation" },
    { VLC_FOURCC('T','S','E','Q'), CODEC_ID_TIERTEXSEQVIDEO,
      VIDEO_ES, "Tiertex SEQ Video" },
    { VLC_FOURCC('D','X','A','1'), CODEC_ID_DXA,
      VIDEO_ES, "Feeble DXA Video" },
    { VLC_FOURCC('D','C','I','V'), CODEC_ID_DSICINVIDEO,
      VIDEO_ES, "Delphine CIN Video" },
    { VLC_FOURCC('T','H','P','V'), CODEC_ID_THP,
      VIDEO_ES, "THP Video" },
    { VLC_FOURCC('B','E','T','H'), CODEC_ID_BETHSOFTVID,
      VIDEO_ES, "THP Video" },
    { VLC_FOURCC('C','9','3','V'), CODEC_ID_C93,
      VIDEO_ES, "THP Video" },
#endif

    /*
     *  Image codecs
     */
    { VLC_CODEC_PNG, CODEC_ID_PNG, VIDEO_ES },
    { VLC_CODEC_PPM, CODEC_ID_PPM, VIDEO_ES },
    { VLC_CODEC_PGM, CODEC_ID_PGM, VIDEO_ES },
    { VLC_CODEC_PGMYUV, CODEC_ID_PGMYUV, VIDEO_ES },
    { VLC_CODEC_PAM, CODEC_ID_PAM, VIDEO_ES },
    { VLC_CODEC_JPEGLS, CODEC_ID_JPEGLS, VIDEO_ES },

    { VLC_CODEC_BMP, CODEC_ID_BMP, VIDEO_ES },

    { VLC_CODEC_TIFF, CODEC_ID_TIFF, VIDEO_ES },
    { VLC_CODEC_GIF, CODEC_ID_GIF, VIDEO_ES },
    { VLC_CODEC_TARGA, CODEC_ID_TARGA, VIDEO_ES },
    { VLC_CODEC_SGI, CODEC_ID_SGI, VIDEO_ES },
    { VLC_CODEC_JPEG2000, CODEC_ID_JPEG2000, VIDEO_ES },

    /*
     *  Audio Codecs
     */
    /* WMA family */
    { VLC_CODEC_WMA1, CODEC_ID_WMAV1, AUDIO_ES },
    { VLC_CODEC_WMA2, CODEC_ID_WMAV2, AUDIO_ES },
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT( 52, 35, 0 )
    { VLC_CODEC_WMAP, CODEC_ID_WMAPRO, AUDIO_ES },
#endif
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT( 52, 54, 0 )
    { VLC_CODEC_WMAS, CODEC_ID_WMAVOICE, AUDIO_ES },
#endif

    { VLC_CODEC_DVAUDIO, CODEC_ID_DVAUDIO, AUDIO_ES },

    { VLC_CODEC_MACE3, CODEC_ID_MACE3, AUDIO_ES },
    { VLC_CODEC_MACE6, CODEC_ID_MACE6, AUDIO_ES },

    { VLC_CODEC_MUSEPACK7, CODEC_ID_MUSEPACK7, AUDIO_ES },
    { VLC_CODEC_MUSEPACK8, CODEC_ID_MUSEPACK8, AUDIO_ES },

    { VLC_CODEC_RA_144, CODEC_ID_RA_144, AUDIO_ES },
    { VLC_CODEC_RA_288, CODEC_ID_RA_288, AUDIO_ES },

    { VLC_CODEC_A52, CODEC_ID_AC3, AUDIO_ES },
    { VLC_CODEC_EAC3, CODEC_ID_EAC3, AUDIO_ES },

    { VLC_CODEC_DTS, CODEC_ID_DTS, AUDIO_ES },

    { VLC_CODEC_MPGA, CODEC_ID_MP3, AUDIO_ES },
    { VLC_CODEC_MPGA, CODEC_ID_MP2, AUDIO_ES },

    { VLC_CODEC_MP4A, CODEC_ID_AAC, AUDIO_ES },
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT( 52, 26, 0 )
    { VLC_CODEC_ALS, CODEC_ID_MP4ALS, AUDIO_ES },
#endif
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT( 52, 94, 0 )
    { VLC_CODEC_MP4A, CODEC_ID_AAC_LATM, AUDIO_ES },
#endif

    { VLC_CODEC_INTERPLAY_DPCM, CODEC_ID_INTERPLAY_DPCM, AUDIO_ES },

    { VLC_CODEC_ROQ_DPCM, CODEC_ID_ROQ_DPCM, AUDIO_ES },

    { VLC_CODEC_DSICINAUDIO, CODEC_ID_DSICINAUDIO, AUDIO_ES },

    { VLC_CODEC_ADPCM_4XM, CODEC_ID_ADPCM_4XM, AUDIO_ES },
    { VLC_CODEC_ADPCM_EA, CODEC_ID_ADPCM_EA, AUDIO_ES },
    { VLC_CODEC_ADPCM_XA, CODEC_ID_ADPCM_XA, AUDIO_ES },
    { VLC_CODEC_ADPCM_ADX, CODEC_ID_ADPCM_ADX, AUDIO_ES },
    { VLC_CODEC_ADPCM_IMA_WS, CODEC_ID_ADPCM_IMA_WS, AUDIO_ES },
    { VLC_CODEC_ADPCM_MS, CODEC_ID_ADPCM_MS, AUDIO_ES },
    { VLC_CODEC_ADPCM_IMA_WAV, CODEC_ID_ADPCM_IMA_WAV, AUDIO_ES },
    { VLC_CODEC_ADPCM_IMA_AMV, CODEC_ID_ADPCM_IMA_AMV, AUDIO_ES },

    { VLC_CODEC_VMDAUDIO, CODEC_ID_VMDAUDIO, AUDIO_ES },

    { VLC_CODEC_ADPCM_G726, CODEC_ID_ADPCM_G726, AUDIO_ES },
    { VLC_CODEC_ADPCM_SWF, CODEC_ID_ADPCM_SWF, AUDIO_ES },

    { VLC_CODEC_AMR_NB, CODEC_ID_AMR_NB, AUDIO_ES },
    { VLC_CODEC_AMR_WB, CODEC_ID_AMR_WB, AUDIO_ES },

    { VLC_CODEC_GSM, CODEC_ID_GSM, AUDIO_ES },
    { VLC_CODEC_GSM_MS, CODEC_ID_GSM_MS, AUDIO_ES },

    { VLC_CODEC_QDM2, CODEC_ID_QDM2, AUDIO_ES },

    { VLC_CODEC_COOK, CODEC_ID_COOK, AUDIO_ES },

    { VLC_CODEC_TTA, CODEC_ID_TTA, AUDIO_ES },

    { VLC_CODEC_WAVPACK, CODEC_ID_WAVPACK, AUDIO_ES },

    { VLC_CODEC_ATRAC3, CODEC_ID_ATRAC3, AUDIO_ES },

#if LIBAVCODEC_VERSION_MAJOR < 54
    { VLC_CODEC_SONIC, CODEC_ID_SONIC, AUDIO_ES },
#endif

    { VLC_CODEC_IMC, CODEC_ID_IMC, AUDIO_ES },

    { VLC_CODEC_TRUESPEECH, CODEC_ID_TRUESPEECH, AUDIO_ES },

    { VLC_CODEC_NELLYMOSER, CODEC_ID_NELLYMOSER, AUDIO_ES },

    { VLC_CODEC_VORBIS, CODEC_ID_VORBIS, AUDIO_ES },

    { VLC_CODEC_QCELP, CODEC_ID_QCELP, AUDIO_ES },
    { VLC_CODEC_SPEEX, CODEC_ID_SPEEX, AUDIO_ES },
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT( 52, 34, 0 )
    { VLC_CODEC_TWINVQ, CODEC_ID_TWINVQ, AUDIO_ES },
#endif
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT( 52, 36, 0 )
    { VLC_CODEC_ATRAC1, CODEC_ID_ATRAC1, AUDIO_ES },
#endif
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT( 52, 47, 0 )
    { VLC_CODEC_SIPR, CODEC_ID_SIPR, AUDIO_ES },
#endif
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT( 52, 91, 0 )
    { VLC_CODEC_ADPCM_G722, CODEC_ID_ADPCM_G722, AUDIO_ES },
#endif

    /* Lossless */
    { VLC_CODEC_FLAC, CODEC_ID_FLAC, AUDIO_ES },

    { VLC_CODEC_ALAC, CODEC_ID_ALAC, AUDIO_ES },

    { VLC_CODEC_APE, CODEC_ID_APE, AUDIO_ES },

    { VLC_CODEC_SHORTEN, CODEC_ID_SHORTEN, AUDIO_ES },

    { VLC_CODEC_TRUEHD, CODEC_ID_TRUEHD, AUDIO_ES },
    { VLC_CODEC_MLP, CODEC_ID_MLP, AUDIO_ES },

#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT( 54, 5, 0 )
    { VLC_CODEC_WMAL, CODEC_ID_WMALOSSLESS, AUDIO_ES },
#endif


    /* PCM */
    { VLC_CODEC_S8, CODEC_ID_PCM_S8, AUDIO_ES },
    { VLC_CODEC_U8, CODEC_ID_PCM_U8, AUDIO_ES },
    { VLC_CODEC_S16L, CODEC_ID_PCM_S16LE, AUDIO_ES },
    { VLC_CODEC_S16B, CODEC_ID_PCM_S16BE, AUDIO_ES },
    { VLC_CODEC_U16L, CODEC_ID_PCM_U16LE, AUDIO_ES },
    { VLC_CODEC_U16B, CODEC_ID_PCM_U16BE, AUDIO_ES },
    { VLC_CODEC_S24L, CODEC_ID_PCM_S24LE, AUDIO_ES },
    { VLC_CODEC_S24B, CODEC_ID_PCM_S24BE, AUDIO_ES },
    { VLC_CODEC_U24L, CODEC_ID_PCM_U24LE, AUDIO_ES },
    { VLC_CODEC_U24B, CODEC_ID_PCM_U24BE, AUDIO_ES },
    { VLC_CODEC_S32L, CODEC_ID_PCM_S32LE, AUDIO_ES },
    { VLC_CODEC_S32B, CODEC_ID_PCM_S32BE, AUDIO_ES },
    { VLC_CODEC_U32L, CODEC_ID_PCM_U32LE, AUDIO_ES },
    { VLC_CODEC_U32B, CODEC_ID_PCM_U32BE, AUDIO_ES },
    { VLC_CODEC_ALAW, CODEC_ID_PCM_ALAW, AUDIO_ES },
    { VLC_CODEC_MULAW, CODEC_ID_PCM_MULAW, AUDIO_ES },
    { VLC_CODEC_S24DAUD, CODEC_ID_PCM_S24DAUD, AUDIO_ES },
#if ( !defined( WORDS_BIGENDIAN ) )
    { VLC_CODEC_FL32, CODEC_ID_PCM_F32LE, AUDIO_ES },
    { VLC_CODEC_FL64, CODEC_ID_PCM_F64LE, AUDIO_ES },
#else
    { VLC_CODEC_FL32, CODEC_ID_PCM_F32BE, AUDIO_ES },
    { VLC_CODEC_FL64, CODEC_ID_PCM_F64BE, AUDIO_ES },
#endif

    /* Subtitle streams */
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT( 52, 33, 0 )
    { VLC_CODEC_BD_PG, CODEC_ID_HDMV_PGS_SUBTITLE, SPU_ES },
#endif
    { VLC_CODEC_SPU, CODEC_ID_DVD_SUBTITLE, SPU_ES },
    { VLC_CODEC_DVBS, CODEC_ID_DVB_SUBTITLE, SPU_ES },
    { VLC_CODEC_SUBT, CODEC_ID_TEXT, SPU_ES },
    { VLC_CODEC_XSUB, CODEC_ID_XSUB, SPU_ES },
    { VLC_CODEC_SSA, CODEC_ID_SSA, SPU_ES },
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT( 52, 38, 0 )
    { VLC_CODEC_TELETEXT, CODEC_ID_DVB_TELETEXT, SPU_ES },
#endif

    { 0, 0, UNKNOWN_ES }
};

int GetFfmpegCodec( vlc_fourcc_t i_fourcc, int *pi_cat,
                    int *pi_ffmpeg_codec, const char **ppsz_name )
{
    i_fourcc = vlc_fourcc_GetCodec( UNKNOWN_ES, i_fourcc );
    for( unsigned i = 0; codecs_table[i].i_fourcc != 0; i++ )
    {
        if( codecs_table[i].i_fourcc == i_fourcc )
        {
            if( pi_cat ) *pi_cat = codecs_table[i].i_cat;
            if( pi_ffmpeg_codec ) *pi_ffmpeg_codec = codecs_table[i].i_codec;
            if( ppsz_name ) *ppsz_name = vlc_fourcc_GetDescription( UNKNOWN_ES, i_fourcc );//char *)codecs_table[i].psz_name;

            return true;
        }
    }
    return false;
}

int GetVlcFourcc( int i_ffmpeg_codec, int *pi_cat,
                  vlc_fourcc_t *pi_fourcc, const char **ppsz_name )
{
    for( unsigned i = 0; codecs_table[i].i_codec != 0; i++ )
    {
        if( codecs_table[i].i_codec == i_ffmpeg_codec )
        {
            if( pi_cat ) *pi_cat = codecs_table[i].i_cat;
            if( pi_fourcc ) *pi_fourcc = codecs_table[i].i_fourcc;
            if( ppsz_name ) *ppsz_name = vlc_fourcc_GetDescription( codecs_table[i].i_cat, codecs_table[i].i_fourcc );

            return true;
        }
    }
    return false;
}
