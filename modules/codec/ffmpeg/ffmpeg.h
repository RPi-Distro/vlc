/*****************************************************************************
 * ffmpeg.h: decoder using the ffmpeg library
 *****************************************************************************
 * Copyright (C) 2001 the VideoLAN team
 * $Id: f2ea5ef3a73821145591172cfb79c3d8813f66ad $
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

#include "codecs.h"                                      /* BITMAPINFOHEADER */

#if LIBAVCODEC_BUILD >= 4663
#   define LIBAVCODEC_PP
#else
#   undef  LIBAVCODEC_PP
#endif

struct picture_t;
struct AVFrame;
struct AVCodecContext;
struct AVCodec;

void E_(InitLibavcodec)( vlc_object_t * );
int E_(GetFfmpegCodec) ( vlc_fourcc_t, int *, int *, char ** );
int E_(GetVlcFourcc)   ( int, int *, vlc_fourcc_t *, char ** );
int E_(GetFfmpegChroma)( vlc_fourcc_t );
vlc_fourcc_t E_(GetVlcChroma)( int );

/* Video decoder module */
int  E_( InitVideoDec )( decoder_t *, AVCodecContext *, AVCodec *,
                         int, char * );
void E_( EndVideoDec ) ( decoder_t * );
picture_t *E_( DecodeVideo ) ( decoder_t *, block_t ** );

/* Audio decoder module */
int  E_( InitAudioDec )( decoder_t *, AVCodecContext *, AVCodec *,
                         int, char * );
void E_( EndAudioDec ) ( decoder_t * );
aout_buffer_t *E_( DecodeAudio ) ( decoder_t *, block_t ** );

/* Chroma conversion module */
int  E_(OpenChroma)( vlc_object_t * );
void E_(CloseChroma)( vlc_object_t * );

/* Video encoder module */
int  E_(OpenEncoder) ( vlc_object_t * );
void E_(CloseEncoder)( vlc_object_t * );

/* Audio encoder module */
int  E_(OpenAudioEncoder) ( vlc_object_t * );
void E_(CloseAudioEncoder)( vlc_object_t * );

/* Demux module */
int  E_(OpenDemux) ( vlc_object_t * );
void E_(CloseDemux)( vlc_object_t * );

/* Mux module */
int  E_(OpenMux) ( vlc_object_t * );
void E_(CloseMux)( vlc_object_t * );

/* Video filter module */
int  E_(OpenFilter)( vlc_object_t * );
int  E_(OpenCropPadd)( vlc_object_t * );
void E_(CloseFilter)( vlc_object_t * );
int  E_(OpenDeinterlace)( vlc_object_t * );
void E_(CloseDeinterlace)( vlc_object_t * );

/* Postprocessing module */
void *E_(OpenPostproc)( decoder_t *, vlc_bool_t * );
int E_(InitPostproc)( decoder_t *, void *, int, int, int );
int E_(PostprocPict)( decoder_t *, void *, picture_t *, AVFrame * );
void E_(ClosePostproc)( decoder_t *, void * );

/*****************************************************************************
 * Module descriptor help strings
 *****************************************************************************/
#define DR_TEXT N_("Direct rendering")
/* FIXME Does somebody who knows what it does, explain */
#define DR_LONGTEXT N_("Direct rendering")

#define ERROR_TEXT N_("Error resilience")
#define ERROR_LONGTEXT N_( \
    "Ffmpeg can do error resilience.\n" \
    "However, with a buggy encoder (such as the ISO MPEG-4 encoder from M$) " \
    "this can produce a lot of errors.\n" \
    "Valid values range from 0 to 4 (0 disables all errors resilience).")

#define BUGS_TEXT N_("Workaround bugs")
#define BUGS_LONGTEXT N_( \
    "Try to fix some bugs:\n" \
    "1  autodetect\n" \
    "2  old msmpeg4\n" \
    "4  xvid interlaced\n" \
    "8  ump4 \n" \
    "16 no padding\n" \
    "32 ac vlc\n" \
    "64 Qpel chroma.\n" \
    "This must be the sum of the values. For example, to fix \"ac vlc\" and " \
    "\"ump4\", enter 40.")

#define HURRYUP_TEXT N_("Hurry up")
#define HURRYUP_LONGTEXT N_( \
    "The decoder can partially decode or skip frame(s) " \
    "when there is not enough time. It's useful with low CPU power " \
    "but it can produce distorted pictures.")

#define PP_Q_TEXT N_("Post processing quality")
#define PP_Q_LONGTEXT N_( \
    "Quality of post processing. Valid range is 0 to 6\n" \
    "Higher levels require considerable more CPU power, but produce " \
    "better looking pictures." )

#define DEBUG_TEXT N_( "Debug mask" )
#define DEBUG_LONGTEXT N_( "Set ffmpeg debug mask" )

/* TODO: Use a predefined list, with 0,1,2,4,7 */
#define VISMV_TEXT N_( "Visualize motion vectors" )
#define VISMV_LONGTEXT N_( \
    "You can overlay the motion vectors (arrows showing how the images move) "\
    "on the image. This value is a mask, based on these values:\n"\
    "1 - visualize forward predicted MVs of P frames\n" \
    "2 - visualize forward predicted MVs of B frames\n" \
    "4 - visualize backward predicted MVs of B frames\n" \
    "To visualize all vectors, the value should be 7." )

#define LOWRES_TEXT N_( "Low resolution decoding" )
#define LOWRES_LONGTEXT N_( "Only decode a low resolution version of " \
    "the video. This requires less processing power" )

#define SKIPLOOPF_TEXT N_( "Skip the loop filter for H.264 decoding" )
#define SKIPLOOPF_LONGTEXT N_( "Skipping the loop filter (aka deblocking) " \
    "usually has a detrimental effect on quality. However it provides a big " \
    "speedup for high definition streams." )

#define LIBAVCODEC_PP_TEXT N_("FFmpeg post processing filter chains")
/* FIXME (cut/past from ffmpeg */
#define LIBAVCODEC_PP_LONGTEXT \
N_("<filterName>[:<option>[:<option>...]][[,|/][-]<filterName>[:<option>...]]...\n" \
"long form example:\n" \
"vdeblock:autoq/hdeblock:autoq/linblenddeint    default,-vdeblock\n" \
"short form example:\n" \
"vb:a/hb:a/lb de,-vb\n" \
"more examples:\n" \
"tn:64:128:256\n" \
"Filters                        Options\n" \
"short  long name       short   long option     Description\n" \
"*      *               a       autoq           cpu power dependant enabler\n" \
"                       c       chrom           chrominance filtring enabled\n" \
"                       y       nochrom         chrominance filtring disabled\n" \
"hb     hdeblock        (2 Threshold)           horizontal deblocking filter\n" \
"       1. difference factor: default=64, higher -> more deblocking\n" \
"       2. flatness threshold: default=40, lower -> more deblocking\n" \
"                       the h & v deblocking filters share these\n" \
"                       so u cant set different thresholds for h / v\n" \
"vb     vdeblock        (2 Threshold)           vertical deblocking filter\n" \
"h1     x1hdeblock                              Experimental h deblock filter 1\n" \
"v1     x1vdeblock                              Experimental v deblock filter 1\n" \
"dr     dering                                  Deringing filter\n" \
"al     autolevels                              automatic brightness / contrast\n" \
"                       f       fullyrange      stretch luminance to (0..255)\n" \
"lb     linblenddeint                           linear blend deinterlacer\n" \
"li     linipoldeint                            linear interpolating deinterlace\n" \
"ci     cubicipoldeint                          cubic interpolating deinterlacer\n" \
"md     mediandeint                             median deinterlacer\n" \
"fd     ffmpegdeint                             ffmpeg deinterlacer\n" \
"de     default                                 hb:a,vb:a,dr:a,al\n" \
"fa     fast                                    h1:a,v1:a,dr:a,al\n" \
"tn     tmpnoise        (3 Thresholds)          Temporal Noise Reducer\n" \
"                       1. <= 2. <= 3.          larger -> stronger filtering\n" \
"fq     forceQuant      <quantizer>             Force quantizer\n")

/*
 * Encoder options
 */
#define ENC_CFG_PREFIX "sout-ffmpeg-"

#define ENC_KEYINT_TEXT N_( "Ratio of key frames" )
#define ENC_KEYINT_LONGTEXT N_( "Number of frames " \
  "that will be coded for one key frame." )

#define ENC_BFRAMES_TEXT N_( "Ratio of B frames" )
#define ENC_BFRAMES_LONGTEXT N_( "Number of " \
  "B frames that will be coded between two reference frames." )

#define ENC_VT_TEXT N_( "Video bitrate tolerance" )
#define ENC_VT_LONGTEXT N_( "Video bitrate tolerance in kbit/s." )

#define ENC_INTERLACE_TEXT N_( "Interlaced encoding" )
#define ENC_INTERLACE_LONGTEXT N_( "Enable dedicated " \
  "algorithms for interlaced frames." )

#define ENC_INTERLACE_ME_TEXT N_( "Interlaced motion estimation" )
#define ENC_INTERLACE_ME_LONGTEXT N_( "Enable interlaced " \
  "motion estimation algorithms. This requires more CPU." )

#define ENC_PRE_ME_TEXT N_( "Pre-motion estimation" )
#define ENC_PRE_ME_LONGTEXT N_( "Enable the pre-motion " \
  "estimation algorithm.")

#define ENC_RC_STRICT_TEXT N_( "Strict rate control" )
#define ENC_RC_STRICT_LONGTEXT N_( "Enable the strict rate " \
  "control algorithm." )

#define ENC_RC_BUF_TEXT N_( "Rate control buffer size" )
#define ENC_RC_BUF_LONGTEXT N_( "Rate control " \
  "buffer size (in kbytes). A bigger buffer will allow for better rate " \
  "control, but will cause a delay in the stream." )

#define ENC_RC_BUF_AGGR_TEXT N_( "Rate control buffer aggressiveness" )
#define ENC_RC_BUF_AGGR_LONGTEXT N_( "Rate control "\
  "buffer aggressiveness." )

#define ENC_IQUANT_FACTOR_TEXT N_( "I quantization factor" )
#define ENC_IQUANT_FACTOR_LONGTEXT N_(  \
  "Quantization factor of I frames, compared with P frames (for instance " \
  "1.0 => same qscale for I and P frames)." )

#define ENC_NOISE_RED_TEXT N_( "Noise reduction" )
#define ENC_NOISE_RED_LONGTEXT N_( "Enable a simple noise " \
  "reduction algorithm to lower the encoding length and bitrate, at the " \
  "expense of lower quality frames." )

#define ENC_MPEG4_MATRIX_TEXT N_( "MPEG4 quantization matrix" )
#define ENC_MPEG4_MATRIX_LONGTEXT N_( "Use the MPEG4 " \
  "quantization matrix for MPEG2 encoding. This generally yields a " \
  "better looking picture, while still retaining the compatibility with " \
  "standard MPEG2 decoders.")

#define ENC_HQ_TEXT N_( "Quality level" )
#define ENC_HQ_LONGTEXT N_( "Quality level " \
  "for the encoding of motions vectors (this can slow down the encoding " \
  "very much)." )

#define ENC_HURRYUP_TEXT N_( "Hurry up" )
#define ENC_HURRYUP_LONGTEXT N_( "The encoder " \
  "can make on-the-fly quality tradeoffs if your CPU can't keep up with " \
  "the encoding rate. It will disable trellis quantization, then the rate " \
  "distortion of motion vectors (hq), and raise the noise reduction " \
  "threshold to ease the encoder's task." )

#define ENC_QMIN_TEXT N_( "Minimum video quantizer scale" )
#define ENC_QMIN_LONGTEXT N_( "Minimum video " \
  "quantizer scale." )

#define ENC_QMAX_TEXT N_( "Maximum video quantizer scale" )
#define ENC_QMAX_LONGTEXT N_( "Maximum video " \
  "quantizer scale." )

#define ENC_TRELLIS_TEXT N_( "Trellis quantization" )
#define ENC_TRELLIS_LONGTEXT N_( "Enable trellis " \
  "quantization (rate distortion for block coefficients)." )

#define ENC_QSCALE_TEXT N_( "Fixed quantizer scale" )
#define ENC_QSCALE_LONGTEXT N_( "A fixed video " \
  "quantizer scale for VBR encoding (accepted values: 0.01 to 255.0)." )

#define ENC_STRICT_TEXT N_( "Strict standard compliance" )
#define ENC_STRICT_LONGTEXT N_( "Force a strict standard " \
  "compliance when encoding (accepted values: -1, 0, 1)." )

#define ENC_LUMI_MASKING_TEXT N_( "Luminance masking" )
#define ENC_LUMI_MASKING_LONGTEXT N_( "Raise the quantizer for " \
  "very bright macroblocks (default: 0.0)." )

#define ENC_DARK_MASKING_TEXT N_( "Darkness masking" )
#define ENC_DARK_MASKING_LONGTEXT N_( "Raise the quantizer for " \
  "very dark macroblocks (default: 0.0)." )

#define ENC_P_MASKING_TEXT N_( "Motion masking" )
#define ENC_P_MASKING_LONGTEXT N_( "Raise the quantizer for " \
  "macroblocks with a high temporal complexity (default: 0.0)." )

#define ENC_BORDER_MASKING_TEXT N_( "Border masking" )
#define ENC_BORDER_MASKING_LONGTEXT N_( "Raise the quantizer " \
  "for macroblocks at the border of the frame (default: 0.0)." )

#define ENC_LUMA_ELIM_TEXT N_( "Luminance elimination" )
#define ENC_LUMA_ELIM_LONGTEXT N_( "Eliminates luminance blocks when " \
  "the PSNR isn't much changed (default: 0.0). The H264 specification " \
  "recommends -4." )

#define ENC_CHROMA_ELIM_TEXT N_( "Chrominance elimination" )
#define ENC_CHROMA_ELIM_LONGTEXT N_( "Eliminates chrominance blocks when " \
  "the PSNR isn't much changed (default: 0.0). The H264 specification " \
  "recommends 7." )
