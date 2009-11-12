/*****************************************************************************
 * x264.c: h264 video encoder
 *****************************************************************************
 * Copyright (C) 2004-2006 the VideoLAN team
 * $Id: 6514d2dad4d2ccc1d5e6da601407dd7778ab5cd1 $
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
#include <vlc_vout.h>
#include <vlc_sout.h>
#include <vlc_codec.h>
#include <vlc_charset.h>

#ifdef PTW32_STATIC_LIB
#include <pthread.h>
#endif
#include <x264.h>

#define SOUT_CFG_PREFIX "sout-x264-"

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

/* Frame-type options */

#define KEYINT_TEXT N_("Maximum GOP size")
#define KEYINT_LONGTEXT N_( "Sets maximum interval between IDR-frames." \
    "Larger values save bits, thus improving quality for a given bitrate at " \
    "the cost of seeking precision." )

#define MIN_KEYINT_TEXT N_("Minimum GOP size")
#define MIN_KEYINT_LONGTEXT N_( "Sets minimum interval between IDR-frames. " \
    "In H.264, I-frames do not necessarily bound a closed GOP because it is " \
    "allowable for a P-frame to be predicted from more frames than just the " \
    "one frame before it (also see reference frame option). Therefore, " \
    "I-frames are not necessarily seekable. IDR-frames restrict subsequent " \
    "P-frames from referring to any frame prior to the IDR-frame. \n" \
    "If scenecuts appear within this interval, they are still encoded as " \
    "I-frames, but do not start a new GOP." )

#define SCENE_TEXT N_("Extra I-frames aggressivity" )
#define SCENE_LONGTEXT N_( "Scene-cut detection. Controls how " \
    "aggressively to insert extra I-frames. With small values of " \
    "scenecut, the codec often has " \
    "to force an I-frame when it would exceed keyint. " \
    "Good values of scenecut may find a better location for the " \
    "I-frame. Large values use more I-frames " \
    "than necessary, thus wasting bits. -1 disables scene-cut detection, so " \
    "I-frames are inserted only every other keyint frames, which probably " \
    "leads to ugly encoding artifacts. Range 1 to 100." )

#if X264_BUILD >= 55 /* r607 */ && X264_BUILD < 67 /* r1117 */
#define PRESCENE_TEXT N_("Faster, less precise scenecut detection" )
#define PRESCENE_LONGTEXT N_( "Faster, less precise scenecut detection. " \
    "Required and implied by multi-threading." )
#endif

#define BFRAMES_TEXT N_("B-frames between I and P")
#define BFRAMES_LONGTEXT N_( "Number of consecutive B-frames between I and " \
    "P-frames. Range 1 to 16." )

#define B_ADAPT_TEXT N_("Adaptive B-frame decision")
#if X264_BUILD >= 63
#define B_ADAPT_LONGTEXT N_( "Force the specified number of " \
    "consecutive B-frames to be used, except possibly before an I-frame." \
    "Range 0 to 2." )
#else
#define B_ADAPT_LONGTEXT N_( "Force the specified number of " \
    "consecutive B-frames to be used, except possibly before an I-frame." )
#endif

#define B_BIAS_TEXT N_("Influence (bias) B-frames usage")
#define B_BIAS_LONGTEXT N_( "Bias the choice to use B-frames. Positive values " \
    "cause more B-frames, negative values cause less B-frames." )


#define BPYRAMID_TEXT N_("Keep some B-frames as references")
#if X264_BUILD >= 78
#define BPYRAMID_LONGTEXT N_( "Allows B-frames to be used as references for " \
    "predicting other frames. Keeps the middle of 2+ consecutive B-frames " \
    "as a reference, and reorders frame appropriately.\n" \
    " - none: Disabled\n" \
    " - strict: Strictly hierarchical pyramid\n" \
    " - normal: Non-strict (not Blu-ray compatible)\n"\
    )
#else
#define BPYRAMID_LONGTEXT N_( "Allows B-frames to be used as references for " \
    "predicting other frames. Keeps the middle of 2+ consecutive B-frames " \
    "as a reference, and reorders frame appropriately." )
#endif

#define CABAC_TEXT N_("CABAC")
#define CABAC_LONGTEXT N_( "CABAC (Context-Adaptive Binary Arithmetic "\
    "Coding). Slightly slows down encoding and decoding, but should save " \
    "10 to 15% bitrate." )

#define REF_TEXT N_("Number of reference frames")
#define REF_LONGTEXT N_( "Number of previous frames used as predictors. " \
    "This is effective in Anime, but seems to make little difference in " \
    "live-action source material. Some decoders are unable to deal with " \
    "large frameref values. Range 1 to 16." )

#define NF_TEXT N_("Skip loop filter")
#define NF_LONGTEXT N_( "Deactivate the deblocking loop filter (decreases quality).")

#define FILTER_TEXT N_("Loop filter AlphaC0 and Beta parameters alpha:beta")
#define FILTER_LONGTEXT N_( "Loop filter AlphaC0 and Beta parameters. " \
    "Range -6 to 6 for both alpha and beta parameters. -6 means light " \
    "filter, 6 means strong.")
 
#define LEVEL_TEXT N_("H.264 level")
#define LEVEL_LONGTEXT N_( "Specify H.264 level (as defined by Annex A " \
    "of the standard). Levels are not enforced; it's up to the user to select " \
    "a level compatible with the rest of the encoding options. Range 1 to 5.1 " \
    "(10 to 51 is also allowed).")

#define PROFILE_TEXT N_("H.264 profile")
#define PROFILE_LONGTEXT N_("Specify H.264 profile which limits are enforced over" \
        "other settings" )

/* In order to play an interlaced output stream encoded by x264, a decoder needs
   mbaff support. r570 is using the 'mb' part and not 'aff' yet; so it's really
   'pure-interlaced' mode */
#if X264_BUILD >= 51 /* r570 */
#define INTERLACED_TEXT N_("Interlaced mode")
#define INTERLACED_LONGTEXT N_( "Pure-interlaced mode.")
#endif

/* Ratecontrol */

#define QP_TEXT N_("Set QP")
#define QP_LONGTEXT N_( "This selects the quantizer to use. " \
    "Lower values result in better fidelity, but higher bitrates. 26 is a " \
    "good default value. Range 0 (lossless) to 51." )

#define CRF_TEXT N_("Quality-based VBR")
#define CRF_LONGTEXT N_( "1-pass Quality-based VBR. Range 0 to 51." )

#define QPMIN_TEXT N_("Min QP")
#define QPMIN_LONGTEXT N_( "Minimum quantizer parameter. 15 to 35 seems to " \
    "be a useful range." )

#define QPMAX_TEXT N_("Max QP")
#define QPMAX_LONGTEXT N_( "Maximum quantizer parameter." )

#define QPSTEP_TEXT N_("Max QP step")
#define QPSTEP_LONGTEXT N_( "Max QP step between frames.")

#define RATETOL_TEXT N_("Average bitrate tolerance")
#define RATETOL_LONGTEXT N_( "Allowed variance in average " \
    "bitrate (in kbits/s).")

#define VBV_MAXRATE_TEXT N_("Max local bitrate")
#define VBV_MAXRATE_LONGTEXT N_( "Sets a maximum local bitrate (in kbits/s).")

#define VBV_BUFSIZE_TEXT N_("VBV buffer")
#define VBV_BUFSIZE_LONGTEXT N_( "Averaging period for the maximum " \
    "local bitrate (in kbits).")

#define VBV_INIT_TEXT N_("Initial VBV buffer occupancy")
#define VBV_INIT_LONGTEXT N_( "Sets the initial buffer occupancy as a " \
    "fraction of the buffer size. Range 0.0 to 1.0.")

#if X264_BUILD >= 59
#define AQ_MODE_TEXT N_("How AQ distributes bits")
#define AQ_MODE_LONGTEXT N_("Defines bitdistribution mode for AQ, default 1\n" \
        " - 0: Disabled\n"\
        " - 1: Current x264 default mode\n"\
        " - 2: uses log(var)^2 instead of log(var) and attempts to adapt strength per frame")

#define AQ_STRENGTH_TEXT N_("Strength of AQ")
#define AQ_STRENGTH_LONGTEXT N_("Strength to reduce blocking and blurring in flat\n"\
        "and textured areas, default 1.0 recommented to be between 0..2\n"\
        " - 0.5: weak AQ\n"\
        " - 1.5: strong AQ")
#endif

/* IP Ratio < 1 is technically valid but should never improve quality */
#define IPRATIO_TEXT N_("QP factor between I and P")
#define IPRATIO_LONGTEXT N_( "QP factor between I and P. Range 1.0 to 2.0.")

/* PB ratio < 1 is not valid and breaks ratecontrol */
#define PBRATIO_TEXT N_("QP factor between P and B")
#define PBRATIO_LONGTEXT N_( "QP factor between P and B. Range 1.0 to 2.0.")

#define CHROMA_QP_OFFSET_TEXT N_("QP difference between chroma and luma")
#define CHROMA_QP_OFFSET_LONGTEXT N_( "QP difference between chroma and luma.")

#define PASS_TEXT N_("Multipass ratecontrol")
#define PASS_LONGTEXT N_( "Multipass ratecontrol:\n" \
    " - 1: First pass, creates stats file\n" \
    " - 2: Last pass, does not overwrite stats file\n" \
    " - 3: Nth pass, overwrites stats file\n" )

#define QCOMP_TEXT N_("QP curve compression")
#define QCOMP_LONGTEXT N_( "QP curve compression. Range 0.0 (CBR) to 1.0 (QCP).")

#define CPLXBLUR_TEXT N_("Reduce fluctuations in QP")
#define CPLXBLUR_LONGTEXT N_( "This reduces the fluctuations in QP " \
    "before curve compression. Temporally blurs complexity.")

#define QBLUR_TEXT N_("Reduce fluctuations in QP")
#define QBLUR_LONGTEXT N_( "This reduces the fluctations in QP " \
    "after curve compression. Temporally blurs quants.")

/* Analysis */

#define ANALYSE_TEXT N_("Partitions to consider")
#define ANALYSE_LONGTEXT N_( "Partitions to consider in analyse mode: \n" \
    " - none  : \n" \
    " - fast  : i4x4\n" \
    " - normal: i4x4,p8x8,(i8x8)\n" \
    " - slow  : i4x4,p8x8,(i8x8),b8x8\n" \
    " - all   : i4x4,p8x8,(i8x8),b8x8,p4x4\n" \
    "(p4x4 requires p8x8. i8x8 requires 8x8dct).")

#define DIRECT_PRED_TEXT N_("Direct MV prediction mode")
#define DIRECT_PRED_LONGTEXT N_( "Direct MV prediction mode.")

#if X264_BUILD >= 52 /* r573 */
#define DIRECT_PRED_SIZE_TEXT N_("Direct prediction size")
#define DIRECT_PRED_SIZE_LONGTEXT N_( "Direct prediction size: "\
    " -  0: 4x4\n" \
    " -  1: 8x8\n" \
    " - -1: smallest possible according to level\n" )
#endif

#define WEIGHTB_TEXT N_("Weighted prediction for B-frames")
#define WEIGHTB_LONGTEXT N_( "Weighted prediction for B-frames.")

#define ME_TEXT N_("Integer pixel motion estimation method")
#if X264_BUILD >= 58 /* r728 */
#define ME_LONGTEXT N_( "Selects the motion estimation algorithm: "\
    " - dia: diamond search, radius 1 (fast)\n" \
    " - hex: hexagonal search, radius 2\n" \
    " - umh: uneven multi-hexagon search (better but slower)\n" \
    " - esa: exhaustive search (extremely slow, primarily for testing)\n" \
    " - tesa: hadamard exhaustive search (extremely slow, primarily for testing)\n" )
#else
#define ME_LONGTEXT N_( "Selects the motion estimation algorithm: "\
    " - dia: diamond search, radius 1 (fast)\n" \
    " - hex: hexagonal search, radius 2\n" \
    " - umh: uneven multi-hexagon search (better but slower)\n" \
    " - esa: exhaustive search (extremely slow, primarily for testing)\n" )
#endif

#if X264_BUILD >= 24
#define MERANGE_TEXT N_("Maximum motion vector search range")
#define MERANGE_LONGTEXT N_( "Maximum distance to search for " \
    "motion estimation, measured from predicted position(s). " \
    "Default of 16 is good for most footage, high motion sequences may " \
    "benefit from settings between 24 and 32. Range 0 to 64." )

#define MVRANGE_TEXT N_("Maximum motion vector length")
#define MVRANGE_LONGTEXT N_( "Maximum motion vector length in pixels. " \
    "-1 is automatic, based on level." )
#endif

#if X264_BUILD >= 55 /* r607 */
#define MVRANGE_THREAD_TEXT N_("Minimum buffer space between threads")
#define MVRANGE_THREAD_LONGTEXT N_( "Minimum buffer space between threads. " \
    "-1 is automatic, based on number of threads." )
#endif

#define SUBME_TEXT N_("Subpixel motion estimation and partition decision " \
    "quality")
#if X264_BUILD >= 65 
#define SUBME_MAX 9
#define SUBME_LONGTEXT N_( "This parameter controls quality versus speed " \
    "tradeoffs involved in the motion estimation decision process " \
    "(lower = quicker and higher = better quality). Range 1 to 9." )
#elif X264_BUILD >= 46 /* r477 */
#define SUBME_MAX 7
#define SUBME_LONGTEXT N_( "This parameter controls quality versus speed " \
    "tradeoffs involved in the motion estimation decision process " \
    "(lower = quicker and higher = better quality). Range 1 to 7." )
#elif X264_BUILD >= 30 /* r262 */
#define SUBME_MAX 6
#define SUBME_LONGTEXT N_( "This parameter controls quality versus speed " \
    "tradeoffs involved in the motion estimation decision process " \
    "(lower = quicker and higher = better quality). Range 1 to 6." )
#else
#define SUBME_MAX 5
#define SUBME_LONGTEXT N_( "This parameter controls quality versus speed " \
    "tradeoffs involved in the motion estimation decision process " \
    "(lower = quicker and higher = better quality). Range 1 to 5." )
#endif

#define B_RDO_TEXT N_("RD based mode decision for B-frames")
#define B_RDO_LONGTEXT N_( "RD based mode decision for B-frames. This " \
    "requires subme 6 (or higher).")

#define MIXED_REFS_TEXT N_("Decide references on a per partition basis")
#define MIXED_REFS_LONGTEXT N_( "Allows each 8x8 or 16x8 partition to " \
    "independently select a reference frame, as opposed to only one ref " \
    "per macroblock." )

#define CHROMA_ME_TEXT N_("Chroma in motion estimation")
#define CHROMA_ME_LONGTEXT N_( "Chroma ME for subpel and mode decision in " \
    "P-frames.")

#define BIME_TEXT N_("Jointly optimize both MVs in B-frames")
#define BIME_LONGTEXT N_( "Joint bidirectional motion refinement.")

#define TRANSFORM_8X8DCT_TEXT N_("Adaptive spatial transform size")
#define TRANSFORM_8X8DCT_LONGTEXT N_( \
    "SATD-based decision for 8x8 transform in inter-MBs.")

#define TRELLIS_TEXT N_("Trellis RD quantization" )
#define TRELLIS_LONGTEXT N_( "Trellis RD quantization: \n" \
    " - 0: disabled\n" \
    " - 1: enabled only on the final encode of a MB\n" \
    " - 2: enabled on all mode decisions\n" \
    "This requires CABAC." )

#define FAST_PSKIP_TEXT N_("Early SKIP detection on P-frames")
#define FAST_PSKIP_LONGTEXT N_( "Early SKIP detection on P-frames.")

#define DCT_DECIMATE_TEXT N_("Coefficient thresholding on P-frames")
#define DCT_DECIMATE_LONGTEXT N_( "Coefficient thresholding on P-frames." \
    "Eliminate dct blocks containing only a small single coefficient.")

/* Noise reduction 1 is too weak to measure, suggest at least 10 */
#define NR_TEXT N_("Noise reduction")
#define NR_LONGTEXT N_( "Dct-domain noise reduction. Adaptive pseudo-deadzone. " \
    "10 to 1000 seems to be a useful range." )

#if X264_BUILD >= 52 /* r573 */
#define DEADZONE_INTER_TEXT N_("Inter luma quantization deadzone")
#define DEADZONE_INTER_LONGTEXT N_( "Set the size of the inter luma quantization deadzone. " \
    "Range 0 to 32." )

#define DEADZONE_INTRA_TEXT N_("Intra luma quantization deadzone")
#define DEADZONE_INTRA_LONGTEXT N_( "Set the size of the intra luma quantization deadzone. " \
    "Range 0 to 32." )
#endif

/* Input/Output */

#if X264_BUILD >= 55 /* r607 */
#define NON_DETERMINISTIC_TEXT N_("Non-deterministic optimizations when threaded")
#define NON_DETERMINISTIC_LONGTEXT N_( "Slightly improve quality of SMP, " \
    "at the cost of repeatability.")
#endif

#define ASM_TEXT N_("CPU optimizations")
#define ASM_LONGTEXT N_( "Use assembler CPU optimizations.")

#define STATS_TEXT N_("Filename for 2 pass stats file")
#define STATS_LONGTEXT N_( "Filename for 2 pass stats file for multi-pass encoding.")

#define PSNR_TEXT N_("PSNR computation")
#define PSNR_LONGTEXT N_( "Compute and print PSNR stats. This has no effect on " \
    "the actual encoding quality." )

#define SSIM_TEXT N_("SSIM computation")
#define SSIM_LONGTEXT N_( "Compute and print SSIM stats. This has no effect on " \
    "the actual encoding quality." )

#define QUIET_TEXT N_("Quiet mode")
#define QUIET_LONGTEXT N_( "Quiet mode.")

#define VERBOSE_TEXT N_("Statistics")
#define VERBOSE_LONGTEXT N_( "Print stats for each frame.")

#if X264_BUILD >= 47 /* r518 */
#define SPS_ID_TEXT N_("SPS and PPS id numbers")
#define SPS_ID_LONGTEXT N_( "Set SPS and PPS id numbers to allow concatenating " \
    "streams with different settings.")
#endif

#define AUD_TEXT N_("Access unit delimiters")
#define AUD_LONGTEXT N_( "Generate access unit delimiter NAL units.")

#if X264_BUILD >= 24 && X264_BUILD < 58
static const char *const enc_me_list[] =
  { "dia", "hex", "umh", "esa" };
static const char *const enc_me_list_text[] =
  { N_("dia"), N_("hex"), N_("umh"), N_("esa") };
#endif

#if X264_BUILD >= 58 /* r728 */
static const char *const enc_me_list[] =
  { "dia", "hex", "umh", "esa", "tesa" };
static const char *const enc_me_list_text[] =
  { N_("dia"), N_("hex"), N_("umh"), N_("esa"), N_("tesa") };
#endif

static const char *const profile_list[] =
  { "baseline", "main", "high" };

#if X264_BUILD >= 78
static const char *const bpyramid_list[] =
  { "none", "strict", "normal" };
#endif

static const char *const enc_analyse_list[] =
  { "none", "fast", "normal", "slow", "all" };
static const char *const enc_analyse_list_text[] =
  { N_("none"), N_("fast"), N_("normal"), N_("slow"), N_("all") };

#if X264_BUILD >= 45 /* r457 */
static const char *const direct_pred_list[] =
  { "none", "spatial", "temporal", "auto" };
static const char *const direct_pred_list_text[] =
  { N_("none"), N_("spatial"), N_("temporal"), N_("auto") };
#else
static const char *const direct_pred_list[] =
  { "none", "spatial", "temporal" };
static const char *const direct_pred_list_text[] =
  { N_("none"), N_("spatial"), N_("temporal") };
#endif

vlc_module_begin ()
    set_description( N_("H.264/MPEG4 AVC encoder (x264)"))
    set_capability( "encoder", 200 )
    set_callbacks( Open, Close )
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_VCODEC )

/* Frame-type options */

    add_integer( SOUT_CFG_PREFIX "keyint", 250, NULL, KEYINT_TEXT,
                 KEYINT_LONGTEXT, false )

    add_integer( SOUT_CFG_PREFIX "min-keyint", 25, NULL, MIN_KEYINT_TEXT,
                 MIN_KEYINT_LONGTEXT, false )
        add_deprecated_alias( SOUT_CFG_PREFIX "keyint-min" ) /* Deprecated since 0.8.5 */

    add_integer( SOUT_CFG_PREFIX "scenecut", 40, NULL, SCENE_TEXT,
                 SCENE_LONGTEXT, false )
        change_integer_range( -1, 100 )

#if X264_BUILD >= 55 /* r607 */
#  if X264_BUILD < 67 /* r1117 */
    add_bool( SOUT_CFG_PREFIX "pre-scenecut", 0, NULL, PRESCENE_TEXT,
              PRESCENE_LONGTEXT, false )
#  else
    add_obsolete_bool( SOUT_CFG_PREFIX "pre-scenecut" )
#  endif
#endif

    add_integer( SOUT_CFG_PREFIX "bframes", 3, NULL, BFRAMES_TEXT,
                 BFRAMES_LONGTEXT, false )
        change_integer_range( 0, 16 )

#if X264_BUILD >= 63
    add_integer( SOUT_CFG_PREFIX "b-adapt", 1, NULL, B_ADAPT_TEXT,
                 B_ADAPT_LONGTEXT, false )
        change_integer_range( 0, 2 )
#elif  X264_BUILD >= 0x0013 /* r137 */
    add_bool( SOUT_CFG_PREFIX "b-adapt", 1, NULL, B_ADAPT_TEXT,
              B_ADAPT_LONGTEXT, false )
#endif

#if  X264_BUILD >= 0x0013 /* r137 */
    add_integer( SOUT_CFG_PREFIX "b-bias", 0, NULL, B_BIAS_TEXT,
                 B_BIAS_LONGTEXT, false )
        change_integer_range( -100, 100 )
#endif

#if X264_BUILD >= 78
    add_string( SOUT_CFG_PREFIX "bpyramid", "none", NULL, BPYRAMID_TEXT,
              BPYRAMID_LONGTEXT, false )
        change_string_list( bpyramid_list, bpyramid_list, 0 );
#else
    add_bool( SOUT_CFG_PREFIX "bpyramid", false, NULL, BPYRAMID_TEXT,
              BPYRAMID_LONGTEXT, false )
#endif

    add_bool( SOUT_CFG_PREFIX "cabac", 1, NULL, CABAC_TEXT, CABAC_LONGTEXT,
              false )

    add_integer( SOUT_CFG_PREFIX "ref", 3, NULL, REF_TEXT,
                 REF_LONGTEXT, false )
        change_integer_range( 1, 16 )
        add_deprecated_alias( SOUT_CFG_PREFIX "frameref" ) /* Deprecated since 0.8.5 */

    add_bool( SOUT_CFG_PREFIX "nf", 0, NULL, NF_TEXT,
              NF_LONGTEXT, false )
        add_deprecated_alias( SOUT_CFG_PREFIX "loopfilter" ) /* Deprecated since 0.8.5 */

    add_string( SOUT_CFG_PREFIX "deblock", "0:0", NULL, FILTER_TEXT,
                 FILTER_LONGTEXT, false )
        add_deprecated_alias( SOUT_CFG_PREFIX "filter" ) /* Deprecated since 0.8.6 */

    add_string( SOUT_CFG_PREFIX "level", "5.1", NULL, LEVEL_TEXT,
               LEVEL_LONGTEXT, false )

    add_string( SOUT_CFG_PREFIX "profile", "high", NULL, PROFILE_TEXT,
               PROFILE_LONGTEXT, false )
        change_string_list( profile_list, profile_list, 0 );

#if X264_BUILD >= 51 /* r570 */
    add_bool( SOUT_CFG_PREFIX "interlaced", 0, NULL, INTERLACED_TEXT, INTERLACED_LONGTEXT,
              false )
#endif

/* Ratecontrol */

    add_integer( SOUT_CFG_PREFIX "qp", -1, NULL, QP_TEXT, QP_LONGTEXT,
                 false )
        change_integer_range( -1, 51 ) /* QP 0 -> lossless encoding */

#if X264_BUILD >= 37 /* r334 */
    add_integer( SOUT_CFG_PREFIX "crf", 23, NULL, CRF_TEXT,
                 CRF_LONGTEXT, false )
        change_integer_range( 0, 51 )
#endif

    add_integer( SOUT_CFG_PREFIX "qpmin", 10, NULL, QPMIN_TEXT,
                 QPMIN_LONGTEXT, false )
        change_integer_range( 0, 51 )
        add_deprecated_alias( SOUT_CFG_PREFIX "qp-min" ) /* Deprecated since 0.8.5 */

    add_integer( SOUT_CFG_PREFIX "qpmax", 51, NULL, QPMAX_TEXT,
                 QPMAX_LONGTEXT, false )
        change_integer_range( 0, 51 )
        add_deprecated_alias( SOUT_CFG_PREFIX "qp-max" ) /* Deprecated since 0.8.5 */

    add_integer( SOUT_CFG_PREFIX "qpstep", 4, NULL, QPSTEP_TEXT,
                 QPSTEP_LONGTEXT, false )
        change_integer_range( 0, 51 )

    add_float( SOUT_CFG_PREFIX "ratetol", 1.0, NULL, RATETOL_TEXT,
               RATETOL_LONGTEXT, false )
        change_float_range( 0, 100 )
        add_deprecated_alias( SOUT_CFG_PREFIX "tolerance" ) /* Deprecated since 0.8.5 */

    add_integer( SOUT_CFG_PREFIX "vbv-maxrate", 0, NULL, VBV_MAXRATE_TEXT,
                 VBV_MAXRATE_LONGTEXT, false )

    add_integer( SOUT_CFG_PREFIX "vbv-bufsize", 0, NULL, VBV_BUFSIZE_TEXT,
                 VBV_BUFSIZE_LONGTEXT, false )

    add_float( SOUT_CFG_PREFIX "vbv-init", 0.9, NULL, VBV_INIT_TEXT,
               VBV_INIT_LONGTEXT, false )
        change_float_range( 0, 1 )

    add_float( SOUT_CFG_PREFIX "ipratio", 1.40, NULL, IPRATIO_TEXT,
               IPRATIO_LONGTEXT, false )
        change_float_range( 1, 2 )

    add_float( SOUT_CFG_PREFIX "pbratio", 1.30, NULL, PBRATIO_TEXT,
               PBRATIO_LONGTEXT, false )
        change_float_range( 1, 2 )

#if X264_BUILD >= 23 /* r190 */
    add_integer( SOUT_CFG_PREFIX "chroma-qp-offset", 0, NULL, CHROMA_QP_OFFSET_TEXT,
                 CHROMA_QP_OFFSET_LONGTEXT, false )
#endif

    add_integer( SOUT_CFG_PREFIX "pass", 0, NULL, PASS_TEXT,
                 PASS_LONGTEXT, false )
        change_integer_range( 0, 3 )

    add_float( SOUT_CFG_PREFIX "qcomp", 0.60, NULL, QCOMP_TEXT,
               QCOMP_LONGTEXT, false )
        change_float_range( 0, 1 )

    add_float( SOUT_CFG_PREFIX "cplxblur", 20.0, NULL, CPLXBLUR_TEXT,
               CPLXBLUR_LONGTEXT, false )

    add_float( SOUT_CFG_PREFIX "qblur", 0.5, NULL, QBLUR_TEXT,
               QBLUR_LONGTEXT, false )
#if X264_BUILD >= 59
    add_integer( SOUT_CFG_PREFIX "aq-mode", X264_AQ_VARIANCE, NULL, AQ_MODE_TEXT,
                 AQ_MODE_LONGTEXT, false )
         change_integer_range( 0, 2 )
    add_float( SOUT_CFG_PREFIX "aq-strength", 1.0, NULL, AQ_STRENGTH_TEXT,
               AQ_STRENGTH_LONGTEXT, false )
#endif

/* Analysis */

    /* x264 partitions = none (default). set at least "normal" mode. */
    add_string( SOUT_CFG_PREFIX "partitions", "normal", NULL, ANALYSE_TEXT,
                ANALYSE_LONGTEXT, false )
        change_string_list( enc_analyse_list, enc_analyse_list_text, 0 );
        add_deprecated_alias( SOUT_CFG_PREFIX "analyse" ) /* Deprecated since 0.8.6 */

    add_string( SOUT_CFG_PREFIX "direct", "spatial", NULL, DIRECT_PRED_TEXT,
                DIRECT_PRED_LONGTEXT, false )
        change_string_list( direct_pred_list, direct_pred_list_text, 0 );

#if X264_BUILD >= 52 /* r573 */
    add_integer( SOUT_CFG_PREFIX "direct-8x8", 1, NULL, DIRECT_PRED_SIZE_TEXT,
                 DIRECT_PRED_SIZE_LONGTEXT, false )
        change_integer_range( -1, 1 )
#endif

#if X264_BUILD >= 0x0012 /* r134 */
    add_bool( SOUT_CFG_PREFIX "weightb", 1, NULL, WEIGHTB_TEXT,
              WEIGHTB_LONGTEXT, false )
#endif

#if X264_BUILD >= 24 /* r221 */
    add_string( SOUT_CFG_PREFIX "me", "hex", NULL, ME_TEXT,
                ME_LONGTEXT, false )
        change_string_list( enc_me_list, enc_me_list_text, 0 );

    add_integer( SOUT_CFG_PREFIX "merange", 16, NULL, MERANGE_TEXT,
                 MERANGE_LONGTEXT, false )
        change_integer_range( 1, 64 )
#endif

    add_integer( SOUT_CFG_PREFIX "mvrange", -1, NULL, MVRANGE_TEXT,
                 MVRANGE_LONGTEXT, false )

#if X264_BUILD >= 55 /* r607 */
    add_integer( SOUT_CFG_PREFIX "mvrange-thread", -1, NULL, MVRANGE_THREAD_TEXT,
                 MVRANGE_THREAD_LONGTEXT, false )
#endif

    add_integer( SOUT_CFG_PREFIX "subme", 5, NULL, SUBME_TEXT,
                 SUBME_LONGTEXT, false )
        change_integer_range( 1, SUBME_MAX )
        add_deprecated_alias( SOUT_CFG_PREFIX "subpel" ) /* Deprecated since 0.8.5 */

#if X264_BUILD >= 41
# if X264_BUILD < 65 /* r368 */
    add_bool( SOUT_CFG_PREFIX "b-rdo", 0, NULL, B_RDO_TEXT,
              B_RDO_LONGTEXT, false )
# else
    add_obsolete_bool( SOUT_CFG_PREFIX "b-rdo" )
# endif
#endif

#if X264_BUILD >= 36 /* r318 */
    add_bool( SOUT_CFG_PREFIX "mixed-refs", 1, NULL, MIXED_REFS_TEXT,
              MIXED_REFS_LONGTEXT, false )
#endif

#if X264_BUILD >= 23 /* r171 */
    add_bool( SOUT_CFG_PREFIX "chroma-me", 1, NULL, CHROMA_ME_TEXT,
              CHROMA_ME_LONGTEXT, false )
#endif

#if X264_BUILD >= 43
# if X264_BUILD < 65 /* r390 */
    add_bool( SOUT_CFG_PREFIX "bime", 0, NULL, BIME_TEXT,
              BIME_LONGTEXT, false )
# else
    add_obsolete_bool( SOUT_CFG_PREFIX "bime" )
# endif
#endif

#if X264_BUILD >= 30 /* r251 */
    add_bool( SOUT_CFG_PREFIX "8x8dct", 0, NULL, TRANSFORM_8X8DCT_TEXT,
              TRANSFORM_8X8DCT_LONGTEXT, false )
#endif

#if X264_BUILD >= 39 /* r360 */
    add_integer( SOUT_CFG_PREFIX "trellis", 1, NULL, TRELLIS_TEXT,
                 TRELLIS_LONGTEXT, false )
        change_integer_range( 0, 2 )
#endif

#if X264_BUILD >= 42 /* r384 */
    add_bool( SOUT_CFG_PREFIX "fast-pskip", 1, NULL, FAST_PSKIP_TEXT,
              FAST_PSKIP_LONGTEXT, false )
#endif

#if X264_BUILD >= 46 /* r503 */
    add_bool( SOUT_CFG_PREFIX "dct-decimate", 1, NULL, DCT_DECIMATE_TEXT,
              DCT_DECIMATE_LONGTEXT, false )
#endif

#if X264_BUILD >= 44 /* r398 */
    add_integer( SOUT_CFG_PREFIX "nr", 0, NULL, NR_TEXT,
                 NR_LONGTEXT, false )
        change_integer_range( 0, 1000 )
#endif

#if X264_BUILD >= 52 /* r573 */
    add_integer( SOUT_CFG_PREFIX "deadzone-inter", 21, NULL, DEADZONE_INTER_TEXT,
                 DEADZONE_INTRA_LONGTEXT, false )
        change_integer_range( 0, 32 )

    add_integer( SOUT_CFG_PREFIX "deadzone-intra", 11, NULL, DEADZONE_INTRA_TEXT,
                 DEADZONE_INTRA_LONGTEXT, false )
        change_integer_range( 0, 32 )
#endif

/* Input/Output */

#if X264_BUILD >= 55 /* r607 */
    add_bool( SOUT_CFG_PREFIX "non-deterministic", 0, NULL, NON_DETERMINISTIC_TEXT,
              NON_DETERMINISTIC_LONGTEXT, false )
#endif

    add_bool( SOUT_CFG_PREFIX "asm", 1, NULL, ASM_TEXT,
              ASM_LONGTEXT, false )

    /* x264 psnr = 1 (default). disable PSNR computation for speed. */
    add_bool( SOUT_CFG_PREFIX "psnr", 0, NULL, PSNR_TEXT,
              PSNR_LONGTEXT, false )

#if X264_BUILD >= 50 /* r554 */
    /* x264 ssim = 1 (default). disable SSIM computation for speed. */
    add_bool( SOUT_CFG_PREFIX "ssim", 0, NULL, SSIM_TEXT,
              SSIM_LONGTEXT, false )
#endif

    add_bool( SOUT_CFG_PREFIX "quiet", 0, NULL, QUIET_TEXT,
              QUIET_LONGTEXT, false )

#if X264_BUILD >= 47 /* r518 */
    add_integer( SOUT_CFG_PREFIX "sps-id", 0, NULL, SPS_ID_TEXT,
                 SPS_ID_LONGTEXT, false )
#endif

    add_bool( SOUT_CFG_PREFIX "aud", 0, NULL, AUD_TEXT,
              AUD_LONGTEXT, false )

#if X264_BUILD >= 0x000e /* r81 */
    add_bool( SOUT_CFG_PREFIX "verbose", 0, NULL, VERBOSE_TEXT,
              VERBOSE_LONGTEXT, false )
#endif

    add_string( SOUT_CFG_PREFIX "stats", "x264_2pass.log", NULL, STATS_TEXT,
                STATS_LONGTEXT, false )

vlc_module_end ()

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static const char *const ppsz_sout_options[] = {
    "8x8dct", "analyse", "asm", "aud", "bframes", "bime", "bpyramid",
    "b-adapt", "b-bias", "b-rdo", "cabac", "chroma-me", "chroma-qp-offset",
    "cplxblur", "crf", "dct-decimate", "deadzone-inter", "deadzone-intra",
    "deblock", "direct", "direct-8x8", "filter", "fast-pskip", "frameref",
    "interlaced", "ipratio", "keyint", "keyint-min", "level", "loopfilter",
    "me", "merange", "min-keyint", "mixed-refs", "mvrange", "mvrange-thread",
    "nf", "non-deterministic", "nr", "partitions", "pass", "pbratio",
    "pre-scenecut", "psnr", "qblur", "qp", "qcomp", "qpstep", "qpmax",
    "qpmin", "qp-max", "qp-min", "quiet", "ratetol", "ref", "scenecut",
    "sps-id", "ssim", "stats", "subme", "subpel", "tolerance", "trellis",
    "verbose", "vbv-bufsize", "vbv-init", "vbv-maxrate", "weightb", "aq-mode",
    "aq-strength", "profile", NULL
};

static block_t *Encode( encoder_t *, picture_t * );

struct encoder_sys_t
{
    x264_t          *h;
    x264_param_t    param;

    int             i_buffer;
    uint8_t         *p_buffer;

    mtime_t         i_interpolated_dts;

    char *psz_stat_name;
};

/*****************************************************************************
 * Open: probe the encoder
 *****************************************************************************/
static int  Open ( vlc_object_t *p_this )
{
    encoder_t     *p_enc = (encoder_t *)p_this;
    encoder_sys_t *p_sys;
    vlc_value_t    val;
    int i_qmin = 0, i_qmax = 0;
    x264_nal_t    *nal;
    int i, i_nal;

    if( p_enc->fmt_out.i_codec != VLC_FOURCC( 'h', '2', '6', '4' ) &&
        !p_enc->b_force )
    {
        return VLC_EGENERIC;
    }

    /* X264_POINTVER or X264_VERSION are not available */
    msg_Dbg ( p_enc, "version x264 0.%d.X", X264_BUILD );

#if X264_BUILD < 37
    if( p_enc->fmt_in.video.i_width % 16 != 0 ||
        p_enc->fmt_in.video.i_height % 16 != 0 )
    {
        msg_Warn( p_enc, "size is not a multiple of 16 (%ix%i)",
                  p_enc->fmt_in.video.i_width, p_enc->fmt_in.video.i_height );

        if( p_enc->fmt_in.video.i_width < 16 ||
            p_enc->fmt_in.video.i_height < 16 )
        {
            msg_Err( p_enc, "video is too small to be cropped" );
            return VLC_EGENERIC;
        }

        msg_Warn( p_enc, "cropping video to %ix%i",
                  p_enc->fmt_in.video.i_width >> 4 << 4,
                  p_enc->fmt_in.video.i_height >> 4 << 4 );
    }
#endif

    config_ChainParse( p_enc, SOUT_CFG_PREFIX, ppsz_sout_options, p_enc->p_cfg );

    p_enc->fmt_out.i_cat = VIDEO_ES;
    p_enc->fmt_out.i_codec = VLC_FOURCC( 'h', '2', '6', '4' );
    p_enc->fmt_in.i_codec = VLC_FOURCC('I','4','2','0');

    p_enc->pf_encode_video = Encode;
    p_enc->pf_encode_audio = NULL;
    p_enc->p_sys = p_sys = malloc( sizeof( encoder_sys_t ) );
    if( !p_sys )
        return VLC_ENOMEM;
    p_sys->i_interpolated_dts = 0;
    p_sys->psz_stat_name = NULL;
    p_sys->p_buffer = NULL;

    x264_param_default( &p_sys->param );
    p_sys->param.i_width  = p_enc->fmt_in.video.i_width;
    p_sys->param.i_height = p_enc->fmt_in.video.i_height;
#if X264_BUILD < 37
    p_sys->param.i_width  = p_sys->param.i_width >> 4 << 4;
    p_sys->param.i_height = p_sys->param.i_height >> 4 << 4;
#endif


    var_Get( p_enc, SOUT_CFG_PREFIX "qcomp", &val );
    p_sys->param.rc.f_qcompress = val.f_float;

    var_Get( p_enc, SOUT_CFG_PREFIX "qpstep", &val );
    if( val.i_int >= 0 && val.i_int <= 51 ) p_sys->param.rc.i_qp_step = val.i_int;
    var_Get( p_enc, SOUT_CFG_PREFIX "qpmin", &val );
    if( val.i_int >= 0 && val.i_int <= 51 )
    {
        i_qmin = val.i_int;
        p_sys->param.rc.i_qp_min = i_qmin;
    }
    var_Get( p_enc, SOUT_CFG_PREFIX "qpmax", &val );
    if( val.i_int >= 0 && val.i_int <= 51 )
    {
        i_qmax = val.i_int;
        p_sys->param.rc.i_qp_max = i_qmax;
    }

    var_Get( p_enc, SOUT_CFG_PREFIX "qp", &val );
    if( val.i_int >= 0 && val.i_int <= 51 )
    {
        if( i_qmin > val.i_int ) i_qmin = val.i_int;
        if( i_qmax < val.i_int ) i_qmax = val.i_int;

        p_sys->param.rc.i_rc_method = X264_RC_CQP;
#if X264_BUILD >= 0x000a
        p_sys->param.rc.i_qp_constant = val.i_int;
        p_sys->param.rc.i_qp_min = i_qmin;
        p_sys->param.rc.i_qp_max = i_qmax;
#else
        p_sys->param.i_qp_constant = val.i_int;
#endif
    } else if( p_enc->fmt_out.i_bitrate > 0 )
    {
        /* set more to ABR if user specifies bitrate, but qp ain't defined */
        p_sys->param.rc.i_bitrate = p_enc->fmt_out.i_bitrate / 1000;
#if X264_BUILD < 48
    /* cbr = 1 overrides qp or crf and sets an average bitrate
       but maxrate = average bitrate is needed for "real" CBR */
        p_sys->param.rc.b_cbr=1;
#endif
        p_sys->param.rc.i_rc_method = X264_RC_ABR;
    }
    else /* Set default to CRF */
    {
#if X264_BUILD >= 37
        var_Get( p_enc, SOUT_CFG_PREFIX "crf", &val );
#   if X264_BUILD >= 48
        p_sys->param.rc.i_rc_method = X264_RC_CRF;
#   endif
        if( val.i_int > 0 && val.i_int <= 51 )
        {
#   if X264_BUILD >= 54
            p_sys->param.rc.f_rf_constant = val.i_int;
#   else
            p_sys->param.rc.i_rf_constant = val.i_int;
#   endif
        }
#endif
    }


#if X264_BUILD >= 24
    var_Get( p_enc, SOUT_CFG_PREFIX "ratetol", &val );
    p_sys->param.rc.f_rate_tolerance = val.f_float;

    var_Get( p_enc, SOUT_CFG_PREFIX "vbv-init", &val );
    p_sys->param.rc.f_vbv_buffer_init = val.f_float;

    var_Get( p_enc, SOUT_CFG_PREFIX "vbv-bufsize", &val );
    p_sys->param.rc.i_vbv_buffer_size = val.i_int;

    /* max bitrate = average bitrate -> CBR */
    var_Get( p_enc, SOUT_CFG_PREFIX "vbv-maxrate", &val );
#if X264_BUILD >= 48
    if( !val.i_int && p_sys->param.rc.i_rc_method == X264_RC_ABR )
        p_sys->param.rc.i_vbv_max_bitrate = p_sys->param.rc.i_bitrate;
    else
#endif
        p_sys->param.rc.i_vbv_max_bitrate = val.i_int;

#else
    p_sys->param.rc.i_rc_buffer_size = p_sys->param.rc.i_bitrate;
    p_sys->param.rc.i_rc_init_buffer = p_sys->param.rc.i_bitrate / 4;
#endif

    var_Get( p_enc, SOUT_CFG_PREFIX "cabac", &val );
    p_sys->param.b_cabac = val.b_bool;

    /* disable deblocking when nf (no loop filter) is enabled */
    var_Get( p_enc, SOUT_CFG_PREFIX "nf", &val );
    p_sys->param.b_deblocking_filter = !val.b_bool;

    var_Get( p_enc, SOUT_CFG_PREFIX "deblock", &val );
    if( val.psz_string )
    {
        char *p = strchr( val.psz_string, ':' );
        p_sys->param.i_deblocking_filter_alphac0 = atoi( val.psz_string );
        p_sys->param.i_deblocking_filter_beta = p ? atoi( p+1 ) : p_sys->param.i_deblocking_filter_alphac0;
        free( val.psz_string );
    }

    var_Get( p_enc, SOUT_CFG_PREFIX "level", &val );
    if( val.psz_string )
    {
        if( us_atof (val.psz_string) < 6 )
            p_sys->param.i_level_idc = (int) (10 * us_atof (val.psz_string)
                                              + .5);
        else
            p_sys->param.i_level_idc = atoi (val.psz_string);
        free( val.psz_string );
    }

#if X264_BUILD >= 51 /* r570 */
    var_Get( p_enc, SOUT_CFG_PREFIX "interlaced", &val );
    p_sys->param.b_interlaced = val.b_bool;
#endif

    var_Get( p_enc, SOUT_CFG_PREFIX "ipratio", &val );
    p_sys->param.rc.f_ip_factor = val.f_float;

    var_Get( p_enc, SOUT_CFG_PREFIX "pbratio", &val );
    p_sys->param.rc.f_pb_factor = val.f_float;

    var_Get( p_enc, SOUT_CFG_PREFIX "qcomp", &val );
    p_sys->param.rc.f_qcompress = val.f_float;

    var_Get( p_enc, SOUT_CFG_PREFIX "cplxblur", &val );
    p_sys->param.rc.f_complexity_blur = val.f_float;

    var_Get( p_enc, SOUT_CFG_PREFIX "qblur", &val );
    p_sys->param.rc.f_qblur = val.f_float;

#if X264_BUILD >= 59
    var_Get( p_enc, SOUT_CFG_PREFIX "aq-mode", &val );
    p_sys->param.rc.i_aq_mode = val.i_int;

    var_Get( p_enc, SOUT_CFG_PREFIX "aq-strength", &val );
    p_sys->param.rc.f_aq_strength = val.f_float;
#endif

#if X264_BUILD >= 0x000e
    var_Get( p_enc, SOUT_CFG_PREFIX "verbose", &val );
    if( val.b_bool ) p_sys->param.i_log_level = X264_LOG_DEBUG;
#endif

    var_Get( p_enc, SOUT_CFG_PREFIX "quiet", &val );
    if( val.b_bool ) p_sys->param.i_log_level = X264_LOG_NONE;

#if X264_BUILD >= 47 /* r518 */
    var_Get( p_enc, SOUT_CFG_PREFIX "sps-id", &val );
    if( val.i_int >= 0 ) p_sys->param.i_sps_id = val.i_int;
#endif

    var_Get( p_enc, SOUT_CFG_PREFIX "aud", &val );
    if( val.b_bool ) p_sys->param.b_aud = val.b_bool;

    var_Get( p_enc, SOUT_CFG_PREFIX "keyint", &val );
#if X264_BUILD >= 0x000e
    if( val.i_int > 0 ) p_sys->param.i_keyint_max = val.i_int;
#else
    if( val.i_int > 0 ) p_sys->param.i_iframe = val.i_int;
#endif

    var_Get( p_enc, SOUT_CFG_PREFIX "min-keyint", &val );
#if X264_BUILD >= 0x000e
    if( val.i_int > 0 ) p_sys->param.i_keyint_min = val.i_int;
#else
    if( val.i_int > 0 ) p_sys->param.i_idrframe = val.i_int;
#endif

    var_Get( p_enc, SOUT_CFG_PREFIX "bframes", &val );
    if( val.i_int >= 0 && val.i_int <= 16 )
        p_sys->param.i_bframe = val.i_int;

#if X264_BUILD >= 78
    var_Get( p_enc, SOUT_CFG_PREFIX "bpyramid", &val );
    p_sys->param.i_bframe_pyramid = X264_B_PYRAMID_NONE;
    if( !strcmp( val.psz_string, "none" ) )
    {
       p_sys->param.i_bframe_pyramid = X264_B_PYRAMID_NONE;
    } else if ( !strcmp( val.psz_string, "strict" ) )
    {
       p_sys->param.i_bframe_pyramid = X264_B_PYRAMID_STRICT;
    } else if ( !strcmp( val.psz_string, "normal" ) )
    {
       p_sys->param.i_bframe_pyramid = X264_B_PYRAMID_NORMAL;
    }
    free( val.psz_string );
#elif X264_BUILD >= 22
    var_Get( p_enc, SOUT_CFG_PREFIX "bpyramid", &val );
    p_sys->param.b_bframe_pyramid = val.b_bool;
#endif

    var_Get( p_enc, SOUT_CFG_PREFIX "ref", &val );
    if( val.i_int > 0 && val.i_int <= 15 )
        p_sys->param.i_frame_reference = val.i_int;

    var_Get( p_enc, SOUT_CFG_PREFIX "scenecut", &val );
#if X264_BUILD >= 0x000b
    if( val.i_int >= -1 && val.i_int <= 100 )
        p_sys->param.i_scenecut_threshold = val.i_int;
#endif

#if X264_BUILD >= 55 /* r607 */ && X264_BUILD < 67 /* r1117 */
    var_Get( p_enc, SOUT_CFG_PREFIX "pre-scenecut", &val );
    p_sys->param.b_pre_scenecut = val.b_bool;
#endif

#if X264_BUILD >= 55 /* r607 */
    var_Get( p_enc, SOUT_CFG_PREFIX "non-deterministic", &val );
    p_sys->param.b_deterministic = val.b_bool;
#endif

    var_Get( p_enc, SOUT_CFG_PREFIX "subme", &val );
    if( val.i_int >= 1 && val.i_int <= SUBME_MAX )
        p_sys->param.analyse.i_subpel_refine = val.i_int;

#if X264_BUILD >= 24
    var_Get( p_enc, SOUT_CFG_PREFIX "me", &val );
    if( !strcmp( val.psz_string, "dia" ) )
    {
        p_sys->param.analyse.i_me_method = X264_ME_DIA;
    }
    else if( !strcmp( val.psz_string, "hex" ) )
    {
        p_sys->param.analyse.i_me_method = X264_ME_HEX;
    }
    else if( !strcmp( val.psz_string, "umh" ) )
    {
        p_sys->param.analyse.i_me_method = X264_ME_UMH;
    }
    else if( !strcmp( val.psz_string, "esa" ) )
    {
        p_sys->param.analyse.i_me_method = X264_ME_ESA;
    }
#   if X264_BUILD >= 58 /* r728 */
    else if( !strcmp( val.psz_string, "tesa" ) )
    {
        p_sys->param.analyse.i_me_method = X264_ME_TESA;
    }
#   endif
    free( val.psz_string );

    var_Get( p_enc, SOUT_CFG_PREFIX "merange", &val );
    if( val.i_int >= 0 && val.i_int <= 64 )
        p_sys->param.analyse.i_me_range = val.i_int;

    var_Get( p_enc, SOUT_CFG_PREFIX "mvrange", &val );
        p_sys->param.analyse.i_mv_range = val.i_int;
#endif

#if X264_BUILD >= 55 /* r607 */
    var_Get( p_enc, SOUT_CFG_PREFIX "mvrange-thread", &val );
        p_sys->param.analyse.i_mv_range_thread = val.i_int;
#endif

    var_Get( p_enc, SOUT_CFG_PREFIX "direct", &val );
    if( !strcmp( val.psz_string, "none" ) )
    {
        p_sys->param.analyse.i_direct_mv_pred = X264_DIRECT_PRED_NONE;
    }
    else if( !strcmp( val.psz_string, "spatial" ) )
    {
        p_sys->param.analyse.i_direct_mv_pred = X264_DIRECT_PRED_SPATIAL;
    }
    else if( !strcmp( val.psz_string, "temporal" ) )
    {
        p_sys->param.analyse.i_direct_mv_pred = X264_DIRECT_PRED_TEMPORAL;
    }
#if X264_BUILD >= 45 /* r457 */
    else if( !strcmp( val.psz_string, "auto" ) )
    {
        p_sys->param.analyse.i_direct_mv_pred = X264_DIRECT_PRED_AUTO;
    }
#endif
    free( val.psz_string );

    var_Get( p_enc, SOUT_CFG_PREFIX "psnr", &val );
    p_sys->param.analyse.b_psnr = val.b_bool;

#if X264_BUILD >= 50 /* r554 */
    var_Get( p_enc, SOUT_CFG_PREFIX "ssim", &val );
    p_sys->param.analyse.b_ssim = val.b_bool;
#endif

#if X264_BUILD >= 0x0012
    var_Get( p_enc, SOUT_CFG_PREFIX "weightb", &val );
    p_sys->param.analyse.b_weighted_bipred = val.b_bool;
#endif

#if X264_BUILD >= 0x0013
    var_Get( p_enc, SOUT_CFG_PREFIX "b-adapt", &val );
#if X264_BUILD >= 63
    p_sys->param.i_bframe_adaptive = val.i_int;
#else
    p_sys->param.b_bframe_adaptive = val.b_bool;
#endif

    var_Get( p_enc, SOUT_CFG_PREFIX "b-bias", &val );
    if( val.i_int >= -100 && val.i_int <= 100 )
        p_sys->param.i_bframe_bias = val.i_int;
#endif

#if X264_BUILD >= 23
    var_Get( p_enc, SOUT_CFG_PREFIX "chroma-me", &val );
    p_sys->param.analyse.b_chroma_me = val.b_bool;
    var_Get( p_enc, SOUT_CFG_PREFIX "chroma-qp-offset", &val );
    p_sys->param.analyse.i_chroma_qp_offset = val.i_int;
#endif

#if X264_BUILD >= 36
    var_Get( p_enc, SOUT_CFG_PREFIX "mixed-refs", &val );
    p_sys->param.analyse.b_mixed_references = val.b_bool;
#endif

#if X264_BUILD >= 39
    var_Get( p_enc, SOUT_CFG_PREFIX "trellis", &val );
    if( val.i_int >= 0 && val.i_int <= 2 )
        p_sys->param.analyse.i_trellis = val.i_int;
#endif

#if X264_BUILD >= 41 && X264_BUILD < 65
    var_Get( p_enc, SOUT_CFG_PREFIX "b-rdo", &val );
    p_sys->param.analyse.b_bframe_rdo = val.b_bool;
#endif

#if X264_BUILD >= 42
    var_Get( p_enc, SOUT_CFG_PREFIX "fast-pskip", &val );
    p_sys->param.analyse.b_fast_pskip = val.b_bool;
#endif

#if X264_BUILD >= 43 && X264_BUILD < 65
    var_Get( p_enc, SOUT_CFG_PREFIX "bime", &val );
    p_sys->param.analyse.b_bidir_me = val.b_bool;
#endif

#if X264_BUILD >= 44
    var_Get( p_enc, SOUT_CFG_PREFIX "nr", &val );
    if( val.i_int >= 0 && val.i_int <= 1000 )
        p_sys->param.analyse.i_noise_reduction = val.i_int;
#endif

#if X264_BUILD >= 46
    var_Get( p_enc, SOUT_CFG_PREFIX "dct-decimate", &val );
    p_sys->param.analyse.b_dct_decimate = val.b_bool;
#endif

#if X264_BUILD >= 52
    var_Get( p_enc, SOUT_CFG_PREFIX "deadzone-inter", &val );
    if( val.i_int >= 0 && val.i_int <= 32 )
        p_sys->param.analyse.i_luma_deadzone[0] = val.i_int;

    var_Get( p_enc, SOUT_CFG_PREFIX "deadzone-intra", &val );
    if( val.i_int >= 0 && val.i_int <= 32 )
        p_sys->param.analyse.i_luma_deadzone[1] = val.i_int;

#if X264_BUILD <= 65
    var_Get( p_enc, SOUT_CFG_PREFIX "direct-8x8", &val );
    if( val.i_int >= -1 && val.i_int <= 1 )
        p_sys->param.analyse.i_direct_8x8_inference = val.i_int;
#endif
#endif

    var_Get( p_enc, SOUT_CFG_PREFIX "asm", &val );
    if( !val.b_bool ) p_sys->param.cpu = 0;

#ifndef X264_ANALYSE_BSUB16x16
#   define X264_ANALYSE_BSUB16x16 0
#endif
    var_Get( p_enc, SOUT_CFG_PREFIX "partitions", &val );
    if( !strcmp( val.psz_string, "none" ) )
    {
        p_sys->param.analyse.inter = 0;
    }
    else if( !strcmp( val.psz_string, "fast" ) )
    {
        p_sys->param.analyse.inter = X264_ANALYSE_I4x4;
    }
    else if( !strcmp( val.psz_string, "normal" ) )
    {
        p_sys->param.analyse.inter =
            X264_ANALYSE_I4x4 |
            X264_ANALYSE_PSUB16x16;
#ifdef X264_ANALYSE_I8x8
        p_sys->param.analyse.inter |= X264_ANALYSE_I8x8;
#endif
    }
    else if( !strcmp( val.psz_string, "slow" ) )
    {
        p_sys->param.analyse.inter =
            X264_ANALYSE_I4x4 |
            X264_ANALYSE_PSUB16x16 |
            X264_ANALYSE_BSUB16x16;
#ifdef X264_ANALYSE_I8x8
        p_sys->param.analyse.inter |= X264_ANALYSE_I8x8;
#endif
    }
    else if( !strcmp( val.psz_string, "all" ) )
    {
        p_sys->param.analyse.inter =
            X264_ANALYSE_I4x4 |
            X264_ANALYSE_PSUB16x16 |
            X264_ANALYSE_BSUB16x16 |
            X264_ANALYSE_PSUB8x8;
#ifdef X264_ANALYSE_I8x8
        p_sys->param.analyse.inter |= X264_ANALYSE_I8x8;
#endif
    }
    free( val.psz_string );

#if X264_BUILD >= 30
    var_Get( p_enc, SOUT_CFG_PREFIX "8x8dct", &val );
    p_sys->param.analyse.b_transform_8x8 = val.b_bool;
#endif

    if( p_enc->fmt_in.video.i_aspect > 0 )
    {
        int64_t i_num, i_den;
        unsigned int i_dst_num, i_dst_den;

        i_num = p_enc->fmt_in.video.i_aspect *
            (int64_t)p_enc->fmt_in.video.i_height;
        i_den = VOUT_ASPECT_FACTOR * p_enc->fmt_in.video.i_width;
        vlc_ureduce( &i_dst_num, &i_dst_den, i_num, i_den, 0 );

        p_sys->param.vui.i_sar_width = i_dst_num;
        p_sys->param.vui.i_sar_height = i_dst_den;
    }

    if( p_enc->fmt_in.video.i_frame_rate_base > 0 )
    {
        p_sys->param.i_fps_num = p_enc->fmt_in.video.i_frame_rate;
        p_sys->param.i_fps_den = p_enc->fmt_in.video.i_frame_rate_base;
    }

    /* x264 vbv-bufsize = 0 (default). if not provided set period
       in seconds for local maximum bitrate (cache/bufsize) based
       on average bitrate when use has told bitrate.
       vbv-buffer size is set to bitrate * secods between keyframes */
    if( !p_sys->param.rc.i_vbv_buffer_size &&
         p_sys->param.rc.i_rc_method == X264_RC_ABR &&
         p_sys->param.i_fps_num )
    {
        p_sys->param.rc.i_vbv_buffer_size = p_sys->param.rc.i_bitrate *
            p_sys->param.i_fps_den;
#if X264_BUILD >= 0x000e
        p_sys->param.rc.i_vbv_buffer_size *= p_sys->param.i_keyint_max;
#else
        p_sys->param.rc.i_vbv_buffer_size *= p_sys->param.i_idrframe;
#endif
        p_sys->param.rc.i_vbv_buffer_size /= p_sys->param.i_fps_num;
    }

    /* Check if user has given some profile (baseline,main,high) to limit
     * settings, and apply those*/
    var_Get( p_enc, SOUT_CFG_PREFIX "profile", &val );
    if( val.psz_string )
    {
        if( !strcasecmp( val.psz_string, "baseline" ) )
        {
            msg_Dbg( p_enc, "Limiting to baseline profile");
            p_sys->param.analyse.b_transform_8x8 = 0;
            p_sys->param.b_cabac = 0;
            p_sys->param.i_bframe = 0;
        }
        else if (!strcasecmp( val.psz_string, "main" ) )
        {
            msg_Dbg( p_enc, "Limiting to main-profile");
            p_sys->param.analyse.b_transform_8x8 = 0;
        }
        /* high profile don't restrict stuff*/
    }
    free( val.psz_string );


    unsigned i_cpu = vlc_CPU();
    if( !(i_cpu & CPU_CAPABILITY_MMX) )
    {
        p_sys->param.cpu &= ~X264_CPU_MMX;
    }
    if( !(i_cpu & CPU_CAPABILITY_MMXEXT) )
    {
        p_sys->param.cpu &= ~X264_CPU_MMXEXT;
    }
    if( !(i_cpu & CPU_CAPABILITY_SSE) )
    {
        p_sys->param.cpu &= ~X264_CPU_SSE;
    }
    if( !(i_cpu & CPU_CAPABILITY_SSE2) )
    {
        p_sys->param.cpu &= ~X264_CPU_SSE2;
    }

    /* BUILD 29 adds support for multi-threaded encoding while BUILD 49 (r543)
       also adds support for threads = 0 for automatically selecting an optimal
       value (cores * 1.5) based on detected CPUs. Default behavior for x264 is
       threads = 1, however VLC usage differs and uses threads = 0 (auto) by
       default unless ofcourse transcode threads is explicitly specified.. */
#if X264_BUILD >= 29
    p_sys->param.i_threads = p_enc->i_threads;
#endif

    var_Get( p_enc, SOUT_CFG_PREFIX "stats", &val );
    if( val.psz_string )
    {
        p_sys->param.rc.psz_stat_in  =
        p_sys->param.rc.psz_stat_out =
        p_sys->psz_stat_name         = val.psz_string;
    }

    var_Get( p_enc, SOUT_CFG_PREFIX "pass", &val );
    if( val.i_int > 0 && val.i_int <= 3 )
    {
        p_sys->param.rc.b_stat_write = val.i_int & 1;
        p_sys->param.rc.b_stat_read = val.i_int & 2;
    }

    /* We need to initialize pthreadw32 before we open the encoder,
       but only oncce for the whole application. Since pthreadw32
       doesn't keep a refcount, do it ourselves. */
#ifdef PTW32_STATIC_LIB
    vlc_value_t lock, count;

    var_Create( p_enc->p_libvlc, "pthread_win32_mutex", VLC_VAR_MUTEX );
    var_Get( p_enc->p_libvlc, "pthread_win32_mutex", &lock );
    vlc_mutex_lock( lock.p_address );

    var_Create( p_enc->p_libvlc, "pthread_win32_count", VLC_VAR_INTEGER );
    var_Get( p_enc->p_libvlc, "pthread_win32_count", &count );

    if( count.i_int == 0 )
    {
        msg_Dbg( p_enc, "initializing pthread-win32" );
        if( !pthread_win32_process_attach_np() || !pthread_win32_thread_attach_np() )   
        {
            msg_Warn( p_enc, "pthread Win32 Initialization failed" );
            vlc_mutex_unlock( lock.p_address );
            return VLC_EGENERIC;
        }
    }

    count.i_int++;
    var_Set( p_enc->p_libvlc, "pthread_win32_count", count );
    vlc_mutex_unlock( lock.p_address );

#endif

#if X264_BUILD >= 69
    /* Set lookahead value to lower than default,
     * as rtp-output without mux doesn't handle
     * difference that well yet*/
    p_sys->param.rc.i_lookahead=5;
#endif

    /* Open the encoder */
    p_sys->h = x264_encoder_open( &p_sys->param );

    if( p_sys->h == NULL )
    {
        msg_Err( p_enc, "cannot open x264 encoder" );
        Close( VLC_OBJECT(p_enc) );
        return VLC_EGENERIC;
    }

    /* alloc mem */
    p_sys->i_buffer = 4 * p_enc->fmt_in.video.i_width *
        p_enc->fmt_in.video.i_height + 1000;
    p_sys->p_buffer = malloc( p_sys->i_buffer );
    if( !p_sys->p_buffer )
    {
        Close( VLC_OBJECT(p_enc) );
        return VLC_ENOMEM;
    }

    /* get the globals headers */
    p_enc->fmt_out.i_extra = 0;
    p_enc->fmt_out.p_extra = NULL;

#if X264_BUILD >= 76
    p_enc->fmt_out.i_extra = x264_encoder_headers( p_sys->h, &nal, &i_nal );
    p_enc->fmt_out.p_extra = malloc( p_enc->fmt_out.i_extra );
    if( !p_enc->fmt_out.p_extra )
    {
        Close( VLC_OBJECT(p_enc) );
        return VLC_ENOMEM;
    }
    void *p_tmp = p_enc->fmt_out.p_extra;
    for( i = 0; i < i_nal; i++ )
    {
        memcpy( p_tmp, nal[i].p_payload, nal[i].i_payload );
        p_tmp += nal[i].i_payload;
    }
#else
    x264_encoder_headers( p_sys->h, &nal, &i_nal );
    for( i = 0; i < i_nal; i++ )
    {
        void *p_tmp;
        int i_size = p_sys->i_buffer;

        x264_nal_encode( p_sys->p_buffer, &i_size, 1, &nal[i] );

        p_tmp = realloc( p_enc->fmt_out.p_extra, p_enc->fmt_out.i_extra + i_size );
        if( !p_tmp )
        {
            Close( VLC_OBJECT(p_enc) );
            return VLC_ENOMEM;
        }
        p_enc->fmt_out.p_extra = p_tmp;

        memcpy( (uint8_t*)p_enc->fmt_out.p_extra + p_enc->fmt_out.i_extra,
                p_sys->p_buffer, i_size );

        p_enc->fmt_out.i_extra += i_size;
    }
#endif

    return VLC_SUCCESS;
}

/****************************************************************************
 * Encode:
 ****************************************************************************/
static block_t *Encode( encoder_t *p_enc, picture_t *p_pict )
{
    encoder_sys_t *p_sys = p_enc->p_sys;
    x264_picture_t pic;
    x264_nal_t *nal;
    block_t *p_block;
    int i_nal, i_out, i;

    /* init pic */
    memset( &pic, 0, sizeof( x264_picture_t ) );
    pic.i_pts = p_pict->date;
    pic.img.i_csp = X264_CSP_I420;
    pic.img.i_plane = p_pict->i_planes;
    for( i = 0; i < p_pict->i_planes; i++ )
    {
        pic.img.plane[i] = p_pict->p[i].p_pixels;
        pic.img.i_stride[i] = p_pict->p[i].i_pitch;
    }

#if X264_BUILD >= 0x0013
    x264_encoder_encode( p_sys->h, &nal, &i_nal, &pic, &pic );
#else
    x264_encoder_encode( p_sys->h, &nal, &i_nal, &pic );
#endif

    if( !i_nal ) return NULL;

    for( i = 0, i_out = 0; i < i_nal; i++ )
    {
#if X264_BUILD >= 76
        memcpy( p_sys->p_buffer + i_out, nal[i].p_payload, nal[i].i_payload );
        i_out += nal[i].i_payload;
#else
        int i_size = p_sys->i_buffer - i_out;
        x264_nal_encode( p_sys->p_buffer + i_out, &i_size, 1, &nal[i] );

        i_out += i_size;
#endif
    }

    p_block = block_New( p_enc, i_out );
    if( !p_block ) return NULL;
    memcpy( p_block->p_buffer, p_sys->p_buffer, i_out );

    if( pic.i_type == X264_TYPE_IDR || pic.i_type == X264_TYPE_I )
        p_block->i_flags |= BLOCK_FLAG_TYPE_I;
    else if( pic.i_type == X264_TYPE_P )
        p_block->i_flags |= BLOCK_FLAG_TYPE_P;
    else if( pic.i_type == X264_TYPE_B )
        p_block->i_flags |= BLOCK_FLAG_TYPE_B;

    /* This isn't really valid for streams with B-frames */
    p_block->i_length = INT64_C(1000000) *
        p_enc->fmt_in.video.i_frame_rate_base /
            p_enc->fmt_in.video.i_frame_rate;

    p_block->i_pts = pic.i_pts;

    if( p_sys->param.i_bframe > 0 )
    {
        if( p_block->i_flags & BLOCK_FLAG_TYPE_B )
        {
            /* FIXME : this is wrong if bpyramid is set */
            p_block->i_dts = p_block->i_pts;
            p_sys->i_interpolated_dts = p_block->i_dts;
        }
        else
        {
            if( p_sys->i_interpolated_dts )
            {
                p_block->i_dts = p_sys->i_interpolated_dts;
            }
            else
            {
                /* Let's put something sensible */
                p_block->i_dts = p_block->i_pts;
            }

            p_sys->i_interpolated_dts += p_block->i_length;
        }
    }
    else
    {
        p_block->i_dts = p_block->i_pts;
    }

    return p_block;
}

/*****************************************************************************
 * CloseEncoder: x264 encoder destruction
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    encoder_t     *p_enc = (encoder_t *)p_this;
    encoder_sys_t *p_sys = p_enc->p_sys;

    free( p_sys->psz_stat_name );

    if( p_sys->h )
        x264_encoder_close( p_sys->h );

#ifdef PTW32_STATIC_LIB
    vlc_value_t lock, count;

    var_Create( p_enc->p_libvlc, "pthread_win32_mutex", VLC_VAR_MUTEX );
    var_Get( p_enc->p_libvlc, "pthread_win32_mutex", &lock );
    vlc_mutex_lock( lock.p_address );

    var_Create( p_enc->p_libvlc, "pthread_win32_count", VLC_VAR_INTEGER );
    var_Get( p_enc->p_libvlc, "pthread_win32_count", &count );
    count.i_int--;
    var_Set( p_enc->p_libvlc, "pthread_win32_count", count );

    if( count.i_int == 0 )
    {
        pthread_win32_thread_detach_np();
        pthread_win32_process_detach_np();
        msg_Dbg( p_enc, "pthread-win32 deinitialized" );
    }
    vlc_mutex_unlock( lock.p_address );
#endif

    free( p_sys->p_buffer );
    free( p_sys );
}
