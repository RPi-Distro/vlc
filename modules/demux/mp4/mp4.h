/*****************************************************************************
 * mp4.h : MP4 file input module for vlc
 *****************************************************************************
 * Copyright (C) 2001-2004, 2010, 2014 VLC authors and VideoLAN
 *
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
#ifndef _VLC_MP4_H
#define _VLC_MP4_H 1

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include "libmp4.h"

/* Contain all information about a chunk */
typedef struct
{
    uint64_t     i_offset; /* absolute position of this chunk in the file */
    uint32_t     i_sample_description_index; /* index for SampleEntry to use */
    uint32_t     i_sample_count; /* how many samples in this chunk */
    uint32_t     i_sample_first; /* index of the first sample in this chunk */
    uint32_t     i_sample; /* index of the next sample to read in this chunk */

    /* now provide way to calculate pts, dts, and offset without too
        much memory and with fast access */

    /* with this we can calculate dts/pts without waste memory */
    uint64_t     i_first_dts;   /* DTS of the first sample */
    uint64_t     i_last_dts;    /* DTS of the last sample */

    uint32_t     i_entries_dts;
    uint32_t     *p_sample_count_dts;
    uint32_t     *p_sample_delta_dts;   /* dts delta */

    uint32_t     i_entries_pts;
    uint32_t     *p_sample_count_pts;
    int32_t      *p_sample_offset_pts;  /* pts-dts */

    uint8_t      **p_sample_data;     /* set when b_fragmented is true */
    uint32_t     *p_sample_size;
    /* TODO if needed add pts
        but quickly *add* support for edts and seeking */

} mp4_chunk_t;

 /* Contain all needed information for read all track with vlc */
typedef struct
{
    unsigned int i_track_ID;/* this should be unique */

    int b_ok;               /* The track is usable */
    int b_enable;           /* is the trak enable by default */
    bool b_selected;  /* is the trak being played */
    bool b_chapter;   /* True when used for chapter only */

    bool b_mac_encoding;

    es_format_t fmt;
    es_out_id_t *p_es;

    /* display size only ! */
    int i_width;
    int i_height;
    float f_rotation;

    /* more internal data */
    uint32_t        i_timescale;    /* time scale for this track only */
    uint16_t        current_qid;    /* Smooth Streaming quality level ID */

    /* elst */
    int             i_elst;         /* current elst */
    int64_t         i_elst_time;    /* current elst start time (in movie time scale)*/
    MP4_Box_t       *p_elst;        /* elst (could be NULL) */

    /* give the next sample to read, i_chunk is to find quickly where
      the sample is located */
    uint32_t         i_sample;       /* next sample to read */
    uint32_t         i_chunk;        /* chunk where next sample is stored */
    /* total count of chunk and sample */
    uint32_t         i_chunk_count;
    uint32_t         i_sample_count;

    mp4_chunk_t    *chunk; /* always defined  for each chunk */
    mp4_chunk_t    *cchunk; /* current chunk if b_fragmented is true */

    /* sample size, p_sample_size defined only if i_sample_size == 0
        else i_sample_size is size for all sample */
    uint32_t         i_sample_size;
    uint32_t         *p_sample_size; /* XXX perhaps add file offset if take
                                    too much time to do sumations each time*/

    uint32_t     i_sample_first; /* i_sample_first value
                                                   of the next chunk */
    uint64_t     i_first_dts;    /* i_first_dts value
                                                   of the next chunk */

    MP4_Box_t *p_stbl;  /* will contain all timing information */
    MP4_Box_t *p_stsd;  /* will contain all data to initialize decoder */
    MP4_Box_t *p_sample;/* point on actual sdsd */

    bool b_drms;
    bool b_has_non_empty_cchunk;
    bool b_codec_need_restart;
    void      *p_drms;
    MP4_Box_t *p_skcr;

    mtime_t i_time;

    struct
    {
        /* for moof parsing */
        MP4_Box_t *p_traf;
        MP4_Box_t *p_tfhd;
        MP4_Box_t *p_trun;
        uint64_t   i_traf_base_offset;
    } context;

} mp4_track_t;

typedef struct mp4_fragment_t mp4_fragment_t;
struct mp4_fragment_t
{
    uint64_t i_chunk_range_min_offset;
    uint64_t i_chunk_range_max_offset;
    uint64_t i_duration;
    MP4_Box_t *p_moox;
    mp4_fragment_t *p_next;
};

#endif
