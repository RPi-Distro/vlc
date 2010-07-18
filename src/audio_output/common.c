/*****************************************************************************
 * common.c : audio output management of common data structures
 *****************************************************************************
 * Copyright (C) 2002-2007 the VideoLAN team
 * $Id: 5d5da1e76db9fa4f297fdcc69d8798eaee91c2a8 $
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
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

#include <assert.h>

#include <vlc_common.h>
#include <vlc_aout.h>
#include "aout_internal.h"
#include "libvlc.h"

/*
 * Instances management (internal and external)
 */

#define AOUT_ASSERT_FIFO_LOCKED aout_assert_fifo_locked(p_aout, p_fifo)
static inline void aout_assert_fifo_locked( aout_instance_t * p_aout, aout_fifo_t * p_fifo )
{
#ifndef NDEBUG
    if( !p_aout )
        return;

    if( p_fifo == &p_aout->output.fifo )
        vlc_assert_locked( &p_aout->output_fifo_lock );
    else
    {
        int i;
        for( i = 0; i < p_aout->i_nb_inputs; i++ )
        {
            if( p_fifo == &p_aout->pp_inputs[i]->mixer.fifo)
            {
                vlc_assert_locked( &p_aout->input_fifos_lock );
                break;
            }
        }
        if( i == p_aout->i_nb_inputs )
            vlc_assert_locked( &p_aout->mixer_lock );
    }
#else
    (void)p_aout;
    (void)p_fifo;
#endif
}

/* Local functions */
static void aout_Destructor( vlc_object_t * p_this );

/*****************************************************************************
 * aout_New: initialize aout structure
 *****************************************************************************/
aout_instance_t * __aout_New( vlc_object_t * p_parent )
{
    aout_instance_t * p_aout;

    /* Allocate descriptor. */
    p_aout = vlc_custom_create( p_parent, sizeof( *p_aout ), VLC_OBJECT_AOUT,
                                "audio output" );
    if( p_aout == NULL )
    {
        return NULL;
    }

    /* Initialize members. */
    vlc_mutex_init( &p_aout->input_fifos_lock );
    vlc_mutex_init( &p_aout->mixer_lock );
    vlc_mutex_init( &p_aout->volume_vars_lock );
    vlc_mutex_init( &p_aout->output_fifo_lock );
    p_aout->i_nb_inputs = 0;
    p_aout->mixer_multiplier = 1.0;
    p_aout->p_mixer = NULL;
    p_aout->output.b_error = 1;
    p_aout->output.b_starving = 1;

    var_Create( p_aout, "intf-change", VLC_VAR_BOOL );
    var_SetBool( p_aout, "intf-change", true );

    vlc_object_set_destructor( p_aout, aout_Destructor );

    return p_aout;
}

/*****************************************************************************
 * aout_Destructor: destroy aout structure
 *****************************************************************************/
static void aout_Destructor( vlc_object_t * p_this )
{
    aout_instance_t * p_aout = (aout_instance_t *)p_this;
    vlc_mutex_destroy( &p_aout->input_fifos_lock );
    vlc_mutex_destroy( &p_aout->mixer_lock );
    vlc_mutex_destroy( &p_aout->volume_vars_lock );
    vlc_mutex_destroy( &p_aout->output_fifo_lock );
}

/* Lock ordering rules:
 *
 *            Vars Mixer Input IFIFO OFIFO (< Inner lock)
 * Vars        No!   Yes   Yes   Yes   Yes
 * Mixer       No!   No!   Yes   Yes   Yes
 * Input       No!   No!   No!   Yes   Yes
 * In FIFOs    No!   No!   No!   No!   Yes
 * Out FIFOs   No!   No!   No!   No!   No!
 * (^ Outer lock)
 */
#ifdef AOUT_DEBUG
/* Lock debugging */
static __thread unsigned aout_locks = 0;

void aout_lock (unsigned i)
{
    unsigned allowed;
    switch (i)
    {
        case VOLUME_VARS_LOCK:
            allowed = 0;
            break;
        case MIXER_LOCK:
            allowed = VOLUME_VARS_LOCK;
            break;
        case INPUT_LOCK:
            allowed = VOLUME_VARS_LOCK|MIXER_LOCK;
            break;
        case INPUT_FIFO_LOCK:
            allowed = VOLUME_VARS_LOCK|MIXER_LOCK|INPUT_LOCK;
            break;
        case OUTPUT_FIFO_LOCK:
            allowed = VOLUME_VARS_LOCK|MIXER_LOCK|INPUT_LOCK|INPUT_FIFO_LOCK;
            break;
        default:
            abort ();
    }

    if (aout_locks & ~allowed)
    {
        fprintf (stderr, "Illegal audio lock transition (%x -> %x)\n",
                 aout_locks, aout_locks|i);
        vlc_backtrace ();
        abort ();
    }
    aout_locks |= i;
}

void aout_unlock (unsigned i)
{
    assert (aout_locks & i);
    aout_locks &= ~i;
}
#endif

/*
 * Formats management (internal and external)
 */

/*****************************************************************************
 * aout_FormatNbChannels : return the number of channels
 *****************************************************************************/
unsigned int aout_FormatNbChannels( const audio_sample_format_t * p_format )
{
    static const uint32_t pi_channels[] =
        { AOUT_CHAN_CENTER, AOUT_CHAN_LEFT, AOUT_CHAN_RIGHT,
          AOUT_CHAN_REARCENTER, AOUT_CHAN_REARLEFT, AOUT_CHAN_REARRIGHT,
          AOUT_CHAN_MIDDLELEFT, AOUT_CHAN_MIDDLERIGHT, AOUT_CHAN_LFE };
    unsigned int i_nb = 0, i;

    for ( i = 0; i < sizeof(pi_channels)/sizeof(uint32_t); i++ )
    {
        if ( p_format->i_physical_channels & pi_channels[i] ) i_nb++;
    }

    return i_nb;
}

/*****************************************************************************
 * aout_BitsPerSample : get the number of bits per sample
 *****************************************************************************/
unsigned int aout_BitsPerSample( vlc_fourcc_t i_format )
{
    switch( vlc_fourcc_GetCodec( AUDIO_ES, i_format ) )
    {
    case VLC_CODEC_U8:
    case VLC_CODEC_S8:
        return 8;

    case VLC_CODEC_U16L:
    case VLC_CODEC_S16L:
    case VLC_CODEC_U16B:
    case VLC_CODEC_S16B:
        return 16;

    case VLC_CODEC_U24L:
    case VLC_CODEC_S24L:
    case VLC_CODEC_U24B:
    case VLC_CODEC_S24B:
        return 24;

    case VLC_CODEC_S32L:
    case VLC_CODEC_S32B:
    case VLC_CODEC_F32L:
    case VLC_CODEC_F32B:
    case VLC_CODEC_FI32:
        return 32;

    case VLC_CODEC_F64L:
    case VLC_CODEC_F64B:
        return 64;

    default:
        /* For these formats the caller has to indicate the parameters
         * by hand. */
        return 0;
    }
}

/*****************************************************************************
 * aout_FormatPrepare : compute the number of bytes per frame & frame length
 *****************************************************************************/
void aout_FormatPrepare( audio_sample_format_t * p_format )
{
    p_format->i_channels = aout_FormatNbChannels( p_format );
    p_format->i_bitspersample = aout_BitsPerSample( p_format->i_format );
    if( p_format->i_bitspersample > 0 )
    {
        p_format->i_bytes_per_frame = ( p_format->i_bitspersample / 8 )
                                    * aout_FormatNbChannels( p_format );
        p_format->i_frame_length = 1;
    }
}

/*****************************************************************************
 * aout_FormatPrintChannels : print a channel in a human-readable form
 *****************************************************************************/
const char * aout_FormatPrintChannels( const audio_sample_format_t * p_format )
{
    switch ( p_format->i_physical_channels & AOUT_CHAN_PHYSMASK )
    {
    case AOUT_CHAN_LEFT:
    case AOUT_CHAN_RIGHT:
    case AOUT_CHAN_CENTER:
        if ( (p_format->i_original_channels & AOUT_CHAN_CENTER)
              || (p_format->i_original_channels
                   & (AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT)) )
            return "Mono";
        else if ( p_format->i_original_channels & AOUT_CHAN_LEFT )
            return "Left";
        return "Right";
    case AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT:
        if ( p_format->i_original_channels & AOUT_CHAN_REVERSESTEREO )
        {
            if ( p_format->i_original_channels & AOUT_CHAN_DOLBYSTEREO )
                return "Dolby/Reverse";
            return "Stereo/Reverse";
        }
        else
        {
            if ( p_format->i_original_channels & AOUT_CHAN_DOLBYSTEREO )
                return "Dolby";
            else if ( p_format->i_original_channels & AOUT_CHAN_DUALMONO )
                return "Dual-mono";
            else if ( p_format->i_original_channels == AOUT_CHAN_CENTER )
                return "Stereo/Mono";
            else if ( !(p_format->i_original_channels & AOUT_CHAN_RIGHT) )
                return "Stereo/Left";
            else if ( !(p_format->i_original_channels & AOUT_CHAN_LEFT) )
                return "Stereo/Right";
            return "Stereo";
        }
    case AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER:
        return "3F";
    case AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_REARCENTER:
        return "2F1R";
    case AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER
          | AOUT_CHAN_REARCENTER:
        return "3F1R";
    case AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT
          | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT:
        return "2F2R";
    case AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT
          | AOUT_CHAN_MIDDLELEFT | AOUT_CHAN_MIDDLERIGHT:
        return "2F2M";
    case AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER
          | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT:
        return "3F2R";
    case AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER
          | AOUT_CHAN_MIDDLELEFT | AOUT_CHAN_MIDDLERIGHT:
        return "3F2M";

    case AOUT_CHAN_CENTER | AOUT_CHAN_LFE:
        if ( (p_format->i_original_channels & AOUT_CHAN_CENTER)
              || (p_format->i_original_channels
                   & (AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT)) )
            return "Mono/LFE";
        else if ( p_format->i_original_channels & AOUT_CHAN_LEFT )
            return "Left/LFE";
        return "Right/LFE";
    case AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_LFE:
        if ( p_format->i_original_channels & AOUT_CHAN_DOLBYSTEREO )
            return "Dolby/LFE";
        else if ( p_format->i_original_channels & AOUT_CHAN_DUALMONO )
            return "Dual-mono/LFE";
        else if ( p_format->i_original_channels == AOUT_CHAN_CENTER )
            return "Mono/LFE";
        else if ( !(p_format->i_original_channels & AOUT_CHAN_RIGHT) )
            return "Stereo/Left/LFE";
        else if ( !(p_format->i_original_channels & AOUT_CHAN_LEFT) )
            return "Stereo/Right/LFE";
         return "Stereo/LFE";
    case AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER | AOUT_CHAN_LFE:
        return "3F/LFE";
    case AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_REARCENTER
          | AOUT_CHAN_LFE:
        return "2F1R/LFE";
    case AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER
          | AOUT_CHAN_REARCENTER | AOUT_CHAN_LFE:
        return "3F1R/LFE";
    case AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT
          | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT | AOUT_CHAN_LFE:
        return "2F2R/LFE";
    case AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT
          | AOUT_CHAN_MIDDLELEFT | AOUT_CHAN_MIDDLERIGHT | AOUT_CHAN_LFE:
        return "2F2M/LFE";
    case AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER
          | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT | AOUT_CHAN_LFE:
        return "3F2R/LFE";
    case AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER
          | AOUT_CHAN_MIDDLELEFT | AOUT_CHAN_MIDDLERIGHT | AOUT_CHAN_LFE:
        return "3F2M/LFE";
    case AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER
          | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT | AOUT_CHAN_MIDDLELEFT
          | AOUT_CHAN_MIDDLERIGHT:
        return "3F2M2R";
    case AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER
          | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT | AOUT_CHAN_MIDDLELEFT
          | AOUT_CHAN_MIDDLERIGHT | AOUT_CHAN_LFE:
        return "3F2M2R/LFE";
    }

    return "ERROR";
}

/*****************************************************************************
 * aout_FormatPrint : print a format in a human-readable form
 *****************************************************************************/
void aout_FormatPrint( aout_instance_t * p_aout, const char * psz_text,
                       const audio_sample_format_t * p_format )
{
    msg_Dbg( p_aout, "%s '%4.4s' %d Hz %s frame=%d samples/%d bytes", psz_text,
             (char *)&p_format->i_format, p_format->i_rate,
             aout_FormatPrintChannels( p_format ),
             p_format->i_frame_length, p_format->i_bytes_per_frame );
}

/*****************************************************************************
 * aout_FormatsPrint : print two formats in a human-readable form
 *****************************************************************************/
void aout_FormatsPrint( aout_instance_t * p_aout, const char * psz_text,
                        const audio_sample_format_t * p_format1,
                        const audio_sample_format_t * p_format2 )
{
    msg_Dbg( p_aout, "%s '%4.4s'->'%4.4s' %d Hz->%d Hz %s->%s",
             psz_text,
             (char *)&p_format1->i_format, (char *)&p_format2->i_format,
             p_format1->i_rate, p_format2->i_rate,
             aout_FormatPrintChannels( p_format1 ),
             aout_FormatPrintChannels( p_format2 ) );
}


/*
 * FIFO management (internal) - please understand that solving race conditions
 * is _your_ job, ie. in the audio output you should own the mixer lock
 * before calling any of these functions.
 */

/*****************************************************************************
 * aout_FifoInit : initialize the members of a FIFO
 *****************************************************************************/
void aout_FifoInit( aout_instance_t * p_aout, aout_fifo_t * p_fifo,
                    uint32_t i_rate )
{
    AOUT_ASSERT_FIFO_LOCKED;

    if( i_rate == 0 )
    {
        msg_Err( p_aout, "initialising fifo with zero divider" );
    }

    p_fifo->p_first = NULL;
    p_fifo->pp_last = &p_fifo->p_first;
    date_Init( &p_fifo->end_date, i_rate, 1 );
}

/*****************************************************************************
 * aout_FifoPush : push a packet into the FIFO
 *****************************************************************************/
void aout_FifoPush( aout_instance_t * p_aout, aout_fifo_t * p_fifo,
                    aout_buffer_t * p_buffer )
{
    (void)p_aout;
    AOUT_ASSERT_FIFO_LOCKED;

    *p_fifo->pp_last = p_buffer;
    p_fifo->pp_last = &p_buffer->p_next;
    *p_fifo->pp_last = NULL;
    /* Enforce the continuity of the stream. */
    if ( date_Get( &p_fifo->end_date ) )
    {
        p_buffer->i_pts = date_Get( &p_fifo->end_date );
        p_buffer->i_length = date_Increment( &p_fifo->end_date,
                                             p_buffer->i_nb_samples );
        p_buffer->i_length -= p_buffer->i_pts;
    }
    else
    {
        date_Set( &p_fifo->end_date, p_buffer->i_pts + p_buffer->i_length );
    }
}

/*****************************************************************************
 * aout_FifoSet : set end_date and trash all buffers (because they aren't
 * properly dated)
 *****************************************************************************/
void aout_FifoSet( aout_instance_t * p_aout, aout_fifo_t * p_fifo,
                   mtime_t date )
{
    aout_buffer_t * p_buffer;
    (void)p_aout;
    AOUT_ASSERT_FIFO_LOCKED;

    date_Set( &p_fifo->end_date, date );
    p_buffer = p_fifo->p_first;
    while ( p_buffer != NULL )
    {
        aout_buffer_t * p_next = p_buffer->p_next;
        aout_BufferFree( p_buffer );
        p_buffer = p_next;
    }
    p_fifo->p_first = NULL;
    p_fifo->pp_last = &p_fifo->p_first;
}

/*****************************************************************************
 * aout_FifoMoveDates : Move forwards or backwards all dates in the FIFO
 *****************************************************************************/
void aout_FifoMoveDates( aout_instance_t * p_aout, aout_fifo_t * p_fifo,
                         mtime_t difference )
{
    aout_buffer_t * p_buffer;
    (void)p_aout;
    AOUT_ASSERT_FIFO_LOCKED;

    date_Move( &p_fifo->end_date, difference );
    p_buffer = p_fifo->p_first;
    while ( p_buffer != NULL )
    {
        p_buffer->i_pts += difference;
        p_buffer = p_buffer->p_next;
    }
}

/*****************************************************************************
 * aout_FifoNextStart : return the current end_date
 *****************************************************************************/
mtime_t aout_FifoNextStart( aout_instance_t * p_aout, aout_fifo_t * p_fifo )
{
    (void)p_aout;
    AOUT_ASSERT_FIFO_LOCKED;
    return date_Get( &p_fifo->end_date );
}

/*****************************************************************************
 * aout_FifoFirstDate : return the playing date of the first buffer in the
 * FIFO
 *****************************************************************************/
mtime_t aout_FifoFirstDate( aout_instance_t * p_aout, aout_fifo_t * p_fifo )
{
    (void)p_aout;
    AOUT_ASSERT_FIFO_LOCKED;
    return p_fifo->p_first ?  p_fifo->p_first->i_pts : 0;
}

/*****************************************************************************
 * aout_FifoPop : get the next buffer out of the FIFO
 *****************************************************************************/
aout_buffer_t * aout_FifoPop( aout_instance_t * p_aout, aout_fifo_t * p_fifo )
{
    aout_buffer_t * p_buffer;
    (void)p_aout;
    AOUT_ASSERT_FIFO_LOCKED;

    p_buffer = p_fifo->p_first;
    if ( p_buffer == NULL ) return NULL;
    p_fifo->p_first = p_buffer->p_next;
    if ( p_fifo->p_first == NULL )
    {
        p_fifo->pp_last = &p_fifo->p_first;
    }

    return p_buffer;
}

/*****************************************************************************
 * aout_FifoDestroy : destroy a FIFO and its buffers
 *****************************************************************************/
void aout_FifoDestroy( aout_instance_t * p_aout, aout_fifo_t * p_fifo )
{
    aout_buffer_t * p_buffer;
    (void)p_aout;
    AOUT_ASSERT_FIFO_LOCKED;

    p_buffer = p_fifo->p_first;
    while ( p_buffer != NULL )
    {
        aout_buffer_t * p_next = p_buffer->p_next;
        aout_BufferFree( p_buffer );
        p_buffer = p_next;
    }

    p_fifo->p_first = NULL;
    p_fifo->pp_last = &p_fifo->p_first;
}

/*****************************************************************************
 * aout_CheckChannelReorder : Check if we need to do some channel re-ordering
 *****************************************************************************/
int aout_CheckChannelReorder( const uint32_t *pi_chan_order_in,
                              const uint32_t *pi_chan_order_out,
                              uint32_t i_channel_mask,
                              int i_channels, int *pi_chan_table )
{
    bool b_chan_reorder = false;
    int i, j, k, l;

    if( i_channels > AOUT_CHAN_MAX )
        return false;

    if( pi_chan_order_in == NULL )
        pi_chan_order_in = pi_vlc_chan_order_wg4;
    if( pi_chan_order_out == NULL )
        pi_chan_order_out = pi_vlc_chan_order_wg4;

    for( i = 0, j = 0; pi_chan_order_in[i]; i++ )
    {
        if( !(i_channel_mask & pi_chan_order_in[i]) ) continue;

        for( k = 0, l = 0; pi_chan_order_in[i] != pi_chan_order_out[k]; k++ )
        {
            if( i_channel_mask & pi_chan_order_out[k] ) l++;
        }

        pi_chan_table[j++] = l;
    }

    for( i = 0; i < i_channels; i++ )
    {
        if( pi_chan_table[i] != i ) b_chan_reorder = true;
    }

    return b_chan_reorder;
}

/*****************************************************************************
 * aout_ChannelReorder :
 *****************************************************************************/
void aout_ChannelReorder( uint8_t *p_buf, int i_buffer,
                          int i_channels, const int *pi_chan_table,
                          int i_bits_per_sample )
{
    uint8_t p_tmp[AOUT_CHAN_MAX * 4];
    int i, j;

    if( i_bits_per_sample == 8 )
    {
        for( i = 0; i < i_buffer / i_channels; i++ )
        {
            for( j = 0; j < i_channels; j++ )
            {
                p_tmp[pi_chan_table[j]] = p_buf[j];
            }

            memcpy( p_buf, p_tmp, i_channels );
            p_buf += i_channels;
        }
    }
    else if( i_bits_per_sample == 16 )
    {
        for( i = 0; i < i_buffer / i_channels / 2; i++ )
        {
            for( j = 0; j < i_channels; j++ )
            {
                p_tmp[2 * pi_chan_table[j]]     = p_buf[2 * j];
                p_tmp[2 * pi_chan_table[j] + 1] = p_buf[2 * j + 1];
            }

            memcpy( p_buf, p_tmp, 2 * i_channels );
            p_buf += 2 * i_channels;
        }
    }
    else if( i_bits_per_sample == 24 )
    {
        for( i = 0; i < i_buffer / i_channels / 3; i++ )
        {
            for( j = 0; j < i_channels; j++ )
            {
                p_tmp[3 * pi_chan_table[j]]     = p_buf[3 * j];
                p_tmp[3 * pi_chan_table[j] + 1] = p_buf[3 * j + 1];
                p_tmp[3 * pi_chan_table[j] + 2] = p_buf[3 * j + 2];
            }

            memcpy( p_buf, p_tmp, 3 * i_channels );
            p_buf += 3 * i_channels;
        }
    }
    else if( i_bits_per_sample == 32 )
    {
        for( i = 0; i < i_buffer / i_channels / 4; i++ )
        {
            for( j = 0; j < i_channels; j++ )
            {
                p_tmp[4 * pi_chan_table[j]]     = p_buf[4 * j];
                p_tmp[4 * pi_chan_table[j] + 1] = p_buf[4 * j + 1];
                p_tmp[4 * pi_chan_table[j] + 2] = p_buf[4 * j + 2];
                p_tmp[4 * pi_chan_table[j] + 3] = p_buf[4 * j + 3];
            }

            memcpy( p_buf, p_tmp, 4 * i_channels );
            p_buf += 4 * i_channels;
        }
    }
}

/*****************************************************************************
 * aout_ChannelExtract:
 *****************************************************************************/
static inline void ExtractChannel( uint8_t *pi_dst, int i_dst_channels,
                                   const uint8_t *pi_src, int i_src_channels,
                                   int i_sample_count,
                                   const int *pi_selection, int i_bytes )
{
    for( int i = 0; i < i_sample_count; i++ )
    {
        for( int j = 0; j < i_dst_channels; j++ )
            memcpy( &pi_dst[j * i_bytes], &pi_src[pi_selection[j] * i_bytes], i_bytes );
        pi_dst += i_dst_channels * i_bytes;
        pi_src += i_src_channels * i_bytes;
    }
}

void aout_ChannelExtract( void *p_dst, int i_dst_channels,
                          const void *p_src, int i_src_channels,
                          int i_sample_count, const int *pi_selection, int i_bits_per_sample )
{
    /* It does not work in place */
    assert( p_dst != p_src );

    /* Force the compiler to inline for the specific cases so it can optimize */
    if( i_bits_per_sample == 8 )
        ExtractChannel( p_dst, i_dst_channels, p_src, i_src_channels, i_sample_count, pi_selection, 1 );
    else  if( i_bits_per_sample == 16 )
        ExtractChannel( p_dst, i_dst_channels, p_src, i_src_channels, i_sample_count, pi_selection, 2 );
    else  if( i_bits_per_sample == 24 )
        ExtractChannel( p_dst, i_dst_channels, p_src, i_src_channels, i_sample_count, pi_selection, 3 );
    else  if( i_bits_per_sample == 32 )
        ExtractChannel( p_dst, i_dst_channels, p_src, i_src_channels, i_sample_count, pi_selection, 4 );
    else  if( i_bits_per_sample == 64 )
        ExtractChannel( p_dst, i_dst_channels, p_src, i_src_channels, i_sample_count, pi_selection, 8 );
}

bool aout_CheckChannelExtraction( int *pi_selection,
                                  uint32_t *pi_layout, int *pi_channels,
                                  const uint32_t pi_order_dst[AOUT_CHAN_MAX],
                                  const uint32_t *pi_order_src, int i_channels )
{
    const uint32_t pi_order_dual_mono[] = { AOUT_CHAN_LEFT, AOUT_CHAN_RIGHT };
    uint32_t i_layout = 0;
    int i_out = 0;
    int pi_index[AOUT_CHAN_MAX];

    /* */
    if( !pi_order_dst )
        pi_order_dst = pi_vlc_chan_order_wg4;

    /* Detect special dual mono case */
    if( i_channels == 2 &&
        pi_order_src[0] == AOUT_CHAN_CENTER && pi_order_src[1] == AOUT_CHAN_CENTER )
    {
        i_layout |= AOUT_CHAN_DUALMONO;
        pi_order_src = pi_order_dual_mono;
    }

    /* */
    for( int i = 0; i < i_channels; i++ )
    {
        /* Ignore unknown or duplicated channels or not present in output */
        if( !pi_order_src[i] || (i_layout & pi_order_src[i]) )
            continue;

        for( int j = 0; j < AOUT_CHAN_MAX; j++ )
        {
            if( pi_order_dst[j] == pi_order_src[i] )
            {
                assert( i_out < AOUT_CHAN_MAX );
                pi_index[i_out++] = i;
                i_layout |= pi_order_src[i];
                break;
            }
        }
    }

    /* */
    for( int i = 0, j = 0; i < AOUT_CHAN_MAX; i++ )
    {
        for( int k = 0; k < i_out; k++ )
        {
            if( pi_order_dst[i] == pi_order_src[pi_index[k]] )
            {
                pi_selection[j++] = pi_index[k];
                break;
            }
        }
    }

    *pi_layout = i_layout;
    *pi_channels = i_out;

    for( int i = 0; i < i_out; i++ )
    {
        if( pi_selection[i] != i )
            return true;
    }
    return i_out == i_channels;
}

/*****************************************************************************
 * aout_BufferAlloc:
 *****************************************************************************/

aout_buffer_t *aout_BufferAlloc(aout_alloc_t *allocation, mtime_t microseconds,
        aout_buffer_t *old_buffer)
{
    if ( !allocation->b_alloc )
    {
        return old_buffer;
    }

    size_t i_alloc_size = (int)( (uint64_t)allocation->i_bytes_per_sec
                                        * (microseconds) / 1000000 + 1 );

    return block_Alloc( i_alloc_size );
}
