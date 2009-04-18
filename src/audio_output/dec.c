/*****************************************************************************
 * dec.c : audio output API towards decoders
 *****************************************************************************
 * Copyright (C) 2002-2007 the VideoLAN team
 * $Id: 3ca6da1726b07091686c3484f3454ad34d9e1e7b $
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

#include <vlc_common.h>

#ifdef HAVE_ALLOCA_H
#   include <alloca.h>
#endif

#include <vlc_aout.h>
#include <vlc_input.h>

#include "aout_internal.h"

/** FIXME: Ugly but needed to access the counters */
#include "input/input_internal.h"

/*****************************************************************************
 * aout_DecNew : create a decoder
 *****************************************************************************/
static aout_input_t * DecNew( vlc_object_t * p_this, aout_instance_t * p_aout,
                              audio_sample_format_t *p_format,
                              audio_replay_gain_t *p_replay_gain )
{
    aout_input_t * p_input;
    input_thread_t * p_input_thread;

    /* Sanitize audio format */
    if( p_format->i_channels > 32 )
    {
        msg_Err( p_aout, "too many audio channels (%u)",
                 p_format->i_channels );
        return NULL;
    }
    if( p_format->i_channels <= 0 )
    {
        msg_Err( p_aout, "no audio channels" );
        return NULL;
    }

    if( p_format->i_rate > 192000 )
    {
        msg_Err( p_aout, "excessive audio sample frequency (%u)",
                 p_format->i_rate );
        return NULL;
    }
    if( p_format->i_rate < 4000 )
    {
        msg_Err( p_aout, "too low audio sample frequency (%u)",
                 p_format->i_rate );
        return NULL;
    }

    /* We can only be called by the decoder, so no need to lock
     * p_input->lock. */
    aout_lock_mixer( p_aout );

    if ( p_aout->i_nb_inputs >= AOUT_MAX_INPUTS )
    {
        msg_Err( p_aout, "too many inputs already (%d)", p_aout->i_nb_inputs );
        goto error;
    }

    p_input = malloc(sizeof(aout_input_t));
    if ( p_input == NULL )
        goto error;
    memset( p_input, 0, sizeof(aout_input_t) );

    vlc_mutex_init( &p_input->lock );

    p_input->b_changed = 0;
    p_input->b_error = 1;

    aout_FormatPrepare( p_format );

    memcpy( &p_input->input, p_format,
            sizeof(audio_sample_format_t) );
    if( p_replay_gain )
        p_input->replay_gain = *p_replay_gain;

    aout_lock_input_fifos( p_aout );
    p_aout->pp_inputs[p_aout->i_nb_inputs] = p_input;
    p_aout->i_nb_inputs++;

    if ( p_aout->mixer.b_error )
    {
        int i;

        var_Destroy( p_aout, "audio-device" );
        var_Destroy( p_aout, "audio-channels" );

        /* Recreate the output using the new format. */
        if ( aout_OutputNew( p_aout, p_format ) < 0 )
        {
            for ( i = 0; i < p_aout->i_nb_inputs - 1; i++ )
            {
                aout_lock_input( p_aout, p_aout->pp_inputs[i] );
                aout_InputDelete( p_aout, p_aout->pp_inputs[i] );
                aout_unlock_input( p_aout, p_aout->pp_inputs[i] );
            }
            aout_unlock_input_fifos( p_aout );
            aout_unlock_mixer( p_aout );
            return p_input;
        }

        /* Create other input streams. */
        for ( i = 0; i < p_aout->i_nb_inputs - 1; i++ )
        {
            aout_lock_input( p_aout, p_aout->pp_inputs[i] );
            aout_InputDelete( p_aout, p_aout->pp_inputs[i] );
            aout_InputNew( p_aout, p_aout->pp_inputs[i] );
            aout_unlock_input( p_aout, p_aout->pp_inputs[i] );
        }
    }
    else
    {
        aout_MixerDelete( p_aout );
    }

    if ( aout_MixerNew( p_aout ) == -1 )
    {
        aout_OutputDelete( p_aout );
        aout_unlock_input_fifos( p_aout );
        goto error;
    }

    aout_InputNew( p_aout, p_input );
    aout_unlock_input_fifos( p_aout );

    aout_unlock_mixer( p_aout );
    p_input->i_desync = var_CreateGetInteger( p_this, "audio-desync" ) * 1000;

    p_input_thread = (input_thread_t *)vlc_object_find( p_this,
                                           VLC_OBJECT_INPUT, FIND_PARENT );
    if( p_input_thread )
    {
        p_input->i_pts_delay = p_input_thread->i_pts_delay;
        p_input->i_pts_delay += p_input->i_desync;
        p_input->p_input_thread = p_input_thread;
        vlc_object_release( p_input_thread );
    }
    else
    {
        p_input->i_pts_delay = DEFAULT_PTS_DELAY;
        p_input->i_pts_delay += p_input->i_desync;
        p_input->p_input_thread = NULL;
    }

    return p_input;

error:
    aout_unlock_mixer( p_aout );
    return NULL;
}

aout_input_t * __aout_DecNew( vlc_object_t * p_this,
                              aout_instance_t ** pp_aout,
                              audio_sample_format_t * p_format,
                              audio_replay_gain_t *p_replay_gain )
{
    aout_instance_t *p_aout = *pp_aout;
    if ( p_aout == NULL )
    {
        msg_Dbg( p_this, "no aout present, spawning one" );
        p_aout = aout_New( p_this );

        /* Everything failed, I'm a loser, I just wanna die */
        if( p_aout == NULL )
            return NULL;

        vlc_object_attach( p_aout, p_this );
        *pp_aout = p_aout;
    }

    return DecNew( p_this, p_aout, p_format, p_replay_gain );
}

/*****************************************************************************
 * aout_DecDelete : delete a decoder
 *****************************************************************************/
int aout_DecDelete( aout_instance_t * p_aout, aout_input_t * p_input )
{
    int i_input;

    /* This function can only be called by the decoder itself, so no need
     * to lock p_input->lock. */
    aout_lock_mixer( p_aout );

    for ( i_input = 0; i_input < p_aout->i_nb_inputs; i_input++ )
    {
        if ( p_aout->pp_inputs[i_input] == p_input )
        {
            break;
        }
    }

    if ( i_input == p_aout->i_nb_inputs )
    {
        msg_Err( p_aout, "cannot find an input to delete" );
        aout_unlock_mixer( p_aout );
        return -1;
    }

    /* Remove the input from the list. */
    memmove( &p_aout->pp_inputs[i_input], &p_aout->pp_inputs[i_input + 1],
             (AOUT_MAX_INPUTS - i_input - 1) * sizeof(aout_input_t *) );
    p_aout->i_nb_inputs--;

    aout_InputDelete( p_aout, p_input );

    vlc_mutex_destroy( &p_input->lock );
    free( p_input );

    if ( !p_aout->i_nb_inputs )
    {
        aout_OutputDelete( p_aout );
        aout_MixerDelete( p_aout );
        if ( var_Type( p_aout, "audio-device" ) != 0 )
        {
            var_Destroy( p_aout, "audio-device" );
        }
        if ( var_Type( p_aout, "audio-channels" ) != 0 )
        {
            var_Destroy( p_aout, "audio-channels" );
        }
    }

    aout_unlock_mixer( p_aout );

    return 0;
}


/*
 * Buffer management
 */

/*****************************************************************************
 * aout_DecNewBuffer : ask for a new empty buffer
 *****************************************************************************/
aout_buffer_t * aout_DecNewBuffer( aout_input_t * p_input,
                                   size_t i_nb_samples )
{
    aout_buffer_t * p_buffer;
    mtime_t duration;

    aout_lock_input( NULL, p_input );

    if ( p_input->b_error )
    {
        aout_unlock_input( NULL, p_input );
        return NULL;
    }

    duration = (1000000 * (mtime_t)i_nb_samples) / p_input->input.i_rate;

    /* This necessarily allocates in the heap. */
    aout_BufferAlloc( &p_input->input_alloc, duration, NULL, p_buffer );
    if( p_buffer != NULL )
        p_buffer->i_nb_bytes = i_nb_samples * p_input->input.i_bytes_per_frame
                                  / p_input->input.i_frame_length;

    /* Suppose the decoder doesn't have more than one buffered buffer */
    p_input->b_changed = 0;

    aout_unlock_input( NULL, p_input );

    if( p_buffer == NULL )
        return NULL;

    p_buffer->i_nb_samples = i_nb_samples;
    p_buffer->start_date = p_buffer->end_date = 0;
    return p_buffer;
}

/*****************************************************************************
 * aout_DecDeleteBuffer : destroy an undecoded buffer
 *****************************************************************************/
void aout_DecDeleteBuffer( aout_instance_t * p_aout, aout_input_t * p_input,
                           aout_buffer_t * p_buffer )
{
    (void)p_aout; (void)p_input;
    aout_BufferFree( p_buffer );
}

/*****************************************************************************
 * aout_DecPlay : filter & mix the decoded buffer
 *****************************************************************************/
int aout_DecPlay( aout_instance_t * p_aout, aout_input_t * p_input,
                  aout_buffer_t * p_buffer, int i_input_rate )
{
    if ( p_buffer->start_date == 0 )
    {
        msg_Warn( p_aout, "non-dated buffer received" );
        aout_BufferFree( p_buffer );
        return -1;
    }

#ifndef FIXME
    /* This hack for #transcode{acodec=...}:display to work -- Courmisch */
    if( i_input_rate == 0 )
        i_input_rate = INPUT_RATE_DEFAULT;
#endif
    if( i_input_rate > INPUT_RATE_DEFAULT * AOUT_MAX_INPUT_RATE ||
        i_input_rate < INPUT_RATE_DEFAULT / AOUT_MAX_INPUT_RATE )
    {
        aout_BufferFree( p_buffer );
        return 0;
    }

    /* Apply the desynchronisation requested by the user */
    p_buffer->start_date += p_input->i_desync;
    p_buffer->end_date += p_input->i_desync;

    if ( p_buffer->start_date > mdate() + p_input->i_pts_delay +
         AOUT_MAX_ADVANCE_TIME )
    {
        msg_Warn( p_aout, "received buffer in the future (%"PRId64")",
                  p_buffer->start_date - mdate());
        if( p_input->p_input_thread )
        {
            vlc_mutex_lock( &p_input->p_input_thread->p->counters.counters_lock);
            stats_UpdateInteger( p_aout,
                           p_input->p_input_thread->p->counters.p_lost_abuffers,
                           1, NULL );
            vlc_mutex_unlock( &p_input->p_input_thread->p->counters.counters_lock);
        }
        aout_BufferFree( p_buffer );
        return -1;
    }

    p_buffer->end_date = p_buffer->start_date
                            + (mtime_t)p_buffer->i_nb_samples * 1000000
                                / p_input->input.i_rate;

    aout_lock_input( p_aout, p_input );

    if ( p_input->b_error )
    {
        aout_unlock_input( p_aout, p_input );
        aout_BufferFree( p_buffer );
        return -1;
    }

    if ( p_input->b_changed )
    {
        /* Maybe the allocation size has changed. Re-allocate a buffer. */
        aout_buffer_t * p_new_buffer;
        mtime_t duration = (1000000 * (mtime_t)p_buffer->i_nb_samples)
                            / p_input->input.i_rate;

        aout_BufferAlloc( &p_input->input_alloc, duration, NULL, p_new_buffer );
        vlc_memcpy( p_new_buffer->p_buffer, p_buffer->p_buffer,
                    p_buffer->i_nb_bytes );
        p_new_buffer->i_nb_samples = p_buffer->i_nb_samples;
        p_new_buffer->i_nb_bytes = p_buffer->i_nb_bytes;
        p_new_buffer->start_date = p_buffer->start_date;
        p_new_buffer->end_date = p_buffer->end_date;
        aout_BufferFree( p_buffer );
        p_buffer = p_new_buffer;
        p_input->b_changed = 0;
    }

    /* If the buffer is too early, wait a while. */
    mwait( p_buffer->start_date - AOUT_MAX_PREPARE_TIME );

    int ret = aout_InputPlay( p_aout, p_input, p_buffer, i_input_rate );

    aout_unlock_input( p_aout, p_input );

    if ( ret == -1 ) return -1;

    /* Run the mixer if it is able to run. */
    aout_lock_mixer( p_aout );
    aout_MixerRun( p_aout );
    if( p_input->p_input_thread )
    {
        vlc_mutex_lock( &p_input->p_input_thread->p->counters.counters_lock);
        stats_UpdateInteger( p_aout,
                             p_input->p_input_thread->p->counters.p_played_abuffers,
                             1, NULL );
        vlc_mutex_unlock( &p_input->p_input_thread->p->counters.counters_lock);
    }
    aout_unlock_mixer( p_aout );

    return 0;
}
