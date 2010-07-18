/*****************************************************************************
 * avcodec.h: decoder and encoder using libavcodec
 *****************************************************************************
 * Copyright (C) 2001-2008 the VideoLAN team
 * $Id: b14b9572e07d39b1cca614f88046ba9071af3b17 $
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

/* VLC <-> avcodec tables */
int GetFfmpegCodec( vlc_fourcc_t i_fourcc, int *pi_cat,
                    int *pi_ffmpeg_codec, const char **ppsz_name );
int GetVlcFourcc( int i_ffmpeg_codec, int *pi_cat,
                  vlc_fourcc_t *pi_fourcc, const char **ppsz_name );
int TestFfmpegChroma( const int i_ffmpeg_id, const vlc_fourcc_t i_vlc_fourcc );
int GetFfmpegChroma( int *i_ffmpeg_chroma, const video_format_t fmt );
int GetVlcChroma( video_format_t *fmt, const int i_ffmpeg_chroma );
void GetVlcAudioFormat( vlc_fourcc_t *, unsigned *pi_bits, int i_sample_fmt );


picture_t * DecodeVideo    ( decoder_t *, block_t ** );
aout_buffer_t * DecodeAudio( decoder_t *, block_t ** );
subpicture_t *DecodeSubtitle( decoder_t *p_dec, block_t ** );

/* Video encoder module */
int  OpenEncoder ( vlc_object_t * );
void CloseEncoder( vlc_object_t * );

/* Audio encoder module */
int  OpenAudioEncoder ( vlc_object_t * );
void CloseAudioEncoder( vlc_object_t * );

/* Deinterlace video filter module */
int  OpenDeinterlace( vlc_object_t * );
void CloseDeinterlace( vlc_object_t * );

void InitLibavcodec( vlc_object_t *p_object );

/* Video Decoder */
int InitVideoDec( decoder_t *p_dec, AVCodecContext *p_context,
                  AVCodec *p_codec, int i_codec_id, const char *psz_namecodec );
void EndVideoDec( decoder_t *p_dec );

/* Audio Decoder */
int InitAudioDec( decoder_t *p_dec, AVCodecContext *p_context,
                  AVCodec *p_codec, int i_codec_id, const char *psz_namecodec );
void EndAudioDec( decoder_t *p_dec );

/* Subtitle Decoder */
int InitSubtitleDec( decoder_t *p_dec, AVCodecContext *p_context,
                     AVCodec *p_codec, int i_codec_id, const char *psz_namecodec );
void EndSubtitleDec( decoder_t *p_dec );


/*****************************************************************************
 * Module descriptor help strings
 *****************************************************************************/
#define DR_TEXT N_("Direct rendering")
/* FIXME Does somebody who knows what it does, explain */
#define DR_LONGTEXT N_("Direct rendering")

#define ERROR_TEXT N_("Error resilience")
#define ERROR_LONGTEXT N_( \
    "FFmpeg can do error resilience.\n" \
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

#define FAST_TEXT N_("Allow speed tricks")
#define FAST_LONGTEXT N_( \
    "Allow non specification compliant speedup tricks. Faster but error-prone.")

#define SKIP_FRAME_TEXT N_("Skip frame (default=0)")
#define SKIP_FRAME_LONGTEXT N_( \
    "Force skipping of frames to speed up decoding " \
    "(-1=None, 0=Default, 1=B-frames, 2=P-frames, 3=B+P frames, 4=all frames)." )

#define SKIP_IDCT_TEXT N_("Skip idct (default=0)")
#define SKIP_IDCT_LONGTEXT N_( \
    "Force skipping of idct to speed up decoding for frame types" \
    "(-1=None, 0=Default, 1=B-frames, 2=P-frames, 3=B+P frames, 4=all frames)." )

#define DEBUG_TEXT N_( "Debug mask" )
#define DEBUG_LONGTEXT N_( "Set FFmpeg debug mask" )

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

#define HW_TEXT N_("Hardware decoding")
#define HW_LONGTEXT N_("This allows hardware decoding when available.")

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

#define ENC_PROFILE_TEXT N_( "Specify AAC audio profile to use" )
#define ENC_PROFILE_LONGTEXT N_( "Specify the AAC audio profile to use " \
   "for encoding the audio bitstream. It takes the following options: " \
   "main, low, ssr (not supported) and ltp (default: main)" )

#define FFMPEG_COMMON_MEMBERS   \
    int i_cat;                  \
    int i_codec_id;             \
    const char *psz_namecodec;  \
    AVCodecContext *p_context;  \
    AVCodec        *p_codec;    \
    bool b_delayed_open;

#ifndef AV_VERSION_INT
#   define AV_VERSION_INT(a, b, c) ((a)<<16 | (b)<<8 | (c))
#endif

/* Uncomment it to enable compilation with vaapi/dxva2 (you also must change the build
 * system) */
//#define HAVE_AVCODEC_VAAPI 1
//#define HAVE_AVCODEC_DXVA2 1
