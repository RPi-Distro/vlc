/*****************************************************************************
 * dtstofloat32.c: DTS Coherent Acoustics decoder plugin for VLC.
 *   This plugin makes use of libdca to do the actual decoding
 *   (http://developers.videolan.org/libdca.html).
 *****************************************************************************
 * Copyright (C) 2001, 2002libdca the VideoLAN team
 * $Id: 98c5fa02597bbd602174e097169389b492fbc518 $
 *
 * Author: Gildas Bazin <gbazin@videolan.org>
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


#include <dca.h>                                       /* libdca header file */

#include <vlc_aout.h>
#include <vlc_block.h>
#include <vlc_filter.h>

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  OpenFilter ( vlc_object_t * );
static void CloseFilter( vlc_object_t * );
static block_t *Convert( filter_t *, block_t * );

/* libdca channel order
 * libdca currently only decodes 5.1, even if you have a DTS-ES source. */
static const uint32_t pi_channels_in[] =
{ AOUT_CHAN_CENTER, AOUT_CHAN_LEFT, AOUT_CHAN_RIGHT,
  AOUT_CHAN_REARCENTER, AOUT_CHAN_REARLEFT, AOUT_CHAN_REARRIGHT,
  AOUT_CHAN_LFE,
  0 };

/*****************************************************************************
 * Local structures
 *****************************************************************************/
struct filter_sys_t
{
    dca_state_t * p_libdca; /* libdca internal structure */
    bool b_dynrng; /* see below */
    int i_flags; /* libdca flags, see dtsdec/doc/libdts.txt */
    bool b_dontwarn;
    int i_nb_channels; /* number of float32 per sample */

    int pi_chan_table[AOUT_CHAN_MAX]; /* channel reordering */
};

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define DYNRNG_TEXT N_("DTS dynamic range compression")
#define DYNRNG_LONGTEXT N_( \
    "Dynamic range compression makes the loud sounds softer, and the soft " \
    "sounds louder, so you can more easily listen to the stream in a noisy " \
    "environment without disturbing anyone. If you disable the dynamic range "\
    "compression the playback will be more adapted to a movie theater or a " \
    "listening room.")

vlc_module_begin ()
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_ACODEC )
    set_shortname( "DCA" )
    set_description( N_("DTS Coherent Acoustics audio decoder") )
    add_bool( "dts-dynrng", true, NULL, DYNRNG_TEXT, DYNRNG_LONGTEXT, false )
    set_capability( "audio filter", 100 )
    set_callbacks( OpenFilter, CloseFilter )
vlc_module_end ()

/*****************************************************************************
 * Open:
 *****************************************************************************/
static int Open( vlc_object_t *p_this, filter_sys_t *p_sys,
                 audio_format_t input, audio_format_t output )
{
    p_sys->b_dynrng = var_InheritBool( p_this, "dts-dynrng" );
    p_sys->b_dontwarn = 0;

    /* We'll do our own downmixing, thanks. */
    p_sys->i_nb_channels = aout_FormatNbChannels( &output );
    switch ( (output.i_physical_channels & AOUT_CHAN_PHYSMASK)
              & ~AOUT_CHAN_LFE )
    {
    case AOUT_CHAN_CENTER:
        if ( (output.i_original_channels & AOUT_CHAN_CENTER)
              || (output.i_original_channels
                   & (AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT)) )
        {
            p_sys->i_flags = DCA_MONO;
        }
        break;

    case AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT:
        if ( output.i_original_channels & AOUT_CHAN_DOLBYSTEREO )
        {
            p_sys->i_flags = DCA_DOLBY;
        }
        else if ( input.i_original_channels == AOUT_CHAN_CENTER )
        {
            p_sys->i_flags = DCA_MONO;
        }
        else if ( input.i_original_channels & AOUT_CHAN_DUALMONO )
        {
            p_sys->i_flags = DCA_CHANNEL;
        }
        else
        {
            p_sys->i_flags = DCA_STEREO;
        }
        break;

    case AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER:
        p_sys->i_flags = DCA_3F;
        break;

    case AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_REARCENTER:
        p_sys->i_flags = DCA_2F1R;
        break;

    case AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER
          | AOUT_CHAN_REARCENTER:
        p_sys->i_flags = DCA_3F1R;
        break;

    case AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT
          | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT:
        p_sys->i_flags = DCA_2F2R;
        break;

    case AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER
          | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT:
        p_sys->i_flags = DCA_3F2R;
        break;

    default:
        msg_Warn( p_this, "unknown sample format!" );
        free( p_sys );
        return -1;
    }
    if ( output.i_physical_channels & AOUT_CHAN_LFE )
    {
        p_sys->i_flags |= DCA_LFE;
    }
    //p_sys->i_flags |= DCA_ADJUST_LEVEL;

    /* Initialize libdca */
    p_sys->p_libdca = dca_init( 0 );
    if( p_sys->p_libdca == NULL )
    {
        msg_Err( p_this, "unable to initialize libdca" );
        return VLC_EGENERIC;
    }

    aout_CheckChannelReorder( pi_channels_in, NULL,
                              output.i_physical_channels & AOUT_CHAN_PHYSMASK,
                              p_sys->i_nb_channels,
                              p_sys->pi_chan_table );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Interleave: helper function to interleave channels
 *****************************************************************************/
static void Interleave( float * p_out, const float * p_in, int i_nb_channels,
                        int *pi_chan_table )
{
    /* We do not only have to interleave, but also reorder the channels. */

    int i, j;
    for ( j = 0; j < i_nb_channels; j++ )
    {
        for ( i = 0; i < 256; i++ )
        {
            p_out[i * i_nb_channels + pi_chan_table[j]] = p_in[j * 256 + i];
        }
    }
}

/*****************************************************************************
 * Duplicate: helper function to duplicate a unique channel
 *****************************************************************************/
static void Duplicate( float * p_out, const float * p_in )
{
    int i;

    for ( i = 256; i--; )
    {
        *p_out++ = *p_in;
        *p_out++ = *p_in;
        p_in++;
    }
}

/*****************************************************************************
 * Exchange: helper function to exchange left & right channels
 *****************************************************************************/
static void Exchange( float * p_out, const float * p_in )
{
    int i;
    const float * p_first = p_in + 256;
    const float * p_second = p_in;

    for ( i = 0; i < 256; i++ )
    {
        *p_out++ = *p_first++;
        *p_out++ = *p_second++;
    }
}

/*****************************************************************************
 * DoWork: decode a DTS frame.
 *****************************************************************************/
static void DoWork( filter_t * p_filter,
                    aout_buffer_t * p_in_buf, aout_buffer_t * p_out_buf )
{
    filter_sys_t    *p_sys = p_filter->p_sys;
    sample_t        i_sample_level = 1;
    int             i_flags = p_sys->i_flags;
    int             i_bytes_per_block = 256 * p_sys->i_nb_channels
                      * sizeof(float);
    int             i;

    /*
     * Do the actual decoding now.
     */

    /* Needs to be called so the decoder knows which type of bitstream it is
     * dealing with. */
    int i_sample_rate, i_bit_rate, i_frame_length;
    if( !dca_syncinfo( p_sys->p_libdca, p_in_buf->p_buffer, &i_flags,
                       &i_sample_rate, &i_bit_rate, &i_frame_length ) )
    {
        msg_Warn( p_filter, "libdca couldn't sync on frame" );
        p_out_buf->i_nb_samples = p_out_buf->i_buffer = 0;
        return;
    }

    i_flags = p_sys->i_flags;
    dca_frame( p_sys->p_libdca, p_in_buf->p_buffer,
               &i_flags, &i_sample_level, 0 );

    if ( (i_flags & DCA_CHANNEL_MASK) != (p_sys->i_flags & DCA_CHANNEL_MASK)
          && !p_sys->b_dontwarn )
    {
        msg_Warn( p_filter,
                  "libdca couldn't do the requested downmix 0x%x->0x%x",
                  p_sys->i_flags  & DCA_CHANNEL_MASK,
                  i_flags & DCA_CHANNEL_MASK );

        p_sys->b_dontwarn = 1;
    }

    if( 0)//!p_sys->b_dynrng )
    {
        dca_dynrng( p_sys->p_libdca, NULL, NULL );
    }

    for ( i = 0; i < dca_blocks_num(p_sys->p_libdca); i++ )
    {
        sample_t * p_samples;

        if( dca_block( p_sys->p_libdca ) )
        {
            msg_Warn( p_filter, "dca_block failed for block %d", i );
            break;
        }

        p_samples = dca_samples( p_sys->p_libdca );

        if ( (p_sys->i_flags & DCA_CHANNEL_MASK) == DCA_MONO
              && (p_filter->fmt_out.audio.i_physical_channels
                   & (AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT)) )
        {
            Duplicate( (float *)(p_out_buf->p_buffer + i * i_bytes_per_block),
                       p_samples );
        }
        else if ( p_filter->fmt_out.audio.i_original_channels
                    & AOUT_CHAN_REVERSESTEREO )
        {
            Exchange( (float *)(p_out_buf->p_buffer + i * i_bytes_per_block),
                      p_samples );
        }
        else
        {
            /* Interleave the *$£%ù samples. */
            Interleave( (float *)(p_out_buf->p_buffer + i * i_bytes_per_block),
                        p_samples, p_sys->i_nb_channels, p_sys->pi_chan_table);
        }
    }

    p_out_buf->i_nb_samples = p_in_buf->i_nb_samples;
    p_out_buf->i_buffer = i_bytes_per_block * i;
}

/*****************************************************************************
 * OpenFilter:
 *****************************************************************************/
static int OpenFilter( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;
    filter_sys_t *p_sys;
    int i_ret;

    if( p_filter->fmt_in.i_codec != VLC_CODEC_DTS ||
        p_filter->fmt_out.audio.i_format == VLC_CODEC_SPDIFB ||
        p_filter->fmt_out.audio.i_format == VLC_CODEC_SPDIFL )
    {
        return VLC_EGENERIC;
    }

    p_filter->fmt_out.audio.i_format =
        p_filter->fmt_out.i_codec = VLC_CODEC_FL32;
    p_filter->fmt_out.audio.i_bitspersample =
        aout_BitsPerSample( p_filter->fmt_out.i_codec );

    /* Allocate the memory needed to store the module's structure */
    p_sys = p_filter->p_sys = malloc( sizeof(filter_sys_t) );
    if( p_sys == NULL )
        return VLC_ENOMEM;

    i_ret = Open( VLC_OBJECT(p_filter), p_sys,
                  p_filter->fmt_in.audio, p_filter->fmt_out.audio );

    p_filter->pf_audio_filter = Convert;
    p_filter->fmt_out.audio.i_rate = p_filter->fmt_in.audio.i_rate;

    return i_ret;
}

/*****************************************************************************
 * CloseFilter : deallocate data structures
 *****************************************************************************/
static void CloseFilter( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;
    filter_sys_t *p_sys = p_filter->p_sys;

    dca_free( p_sys->p_libdca );
    free( p_sys );
}

static block_t *Convert( filter_t *p_filter, block_t *p_block )
{
    if( !p_block || !p_block->i_nb_samples )
    {
        if( p_block )
            block_Release( p_block );
        return NULL;
    }

    size_t i_out_size = p_block->i_nb_samples *
      p_filter->fmt_out.audio.i_bitspersample *
        p_filter->fmt_out.audio.i_channels / 8;

    block_t *p_out = p_filter->pf_audio_buffer_new( p_filter, i_out_size );
    if( !p_out )
    {
        msg_Warn( p_filter, "can't get output buffer" );
        block_Release( p_block );
        return NULL;
    }

    p_out->i_nb_samples = p_block->i_nb_samples;
    p_out->i_dts = p_block->i_dts;
    p_out->i_pts = p_block->i_pts;
    p_out->i_length = p_block->i_length;

    DoWork( p_filter, p_block, p_out );

    block_Release( p_block );

    return p_out;
}
