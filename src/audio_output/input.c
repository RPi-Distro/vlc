/*****************************************************************************
 * input.c : internal management of input streams for the audio output
 *****************************************************************************
 * Copyright (C) 2002-2007 the VideoLAN team
 * $Id: 2ff470b6a9326ff59a349805cb58f53c6d340562 $
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

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <assert.h>

#include <vlc_input.h>
#include <vlc_vout.h>                  /* for vout_Request */

#include <vlc_aout.h>
#include <vlc_filter.h>
#include <libvlc.h>

#include "aout_internal.h"

#define AOUT_ASSERT_MIXER_LOCKED vlc_assert_locked( &p_aout->mixer_lock )
#define AOUT_ASSERT_INPUT_LOCKED vlc_assert_locked( &p_input->lock )

static void inputFailure( aout_instance_t *, aout_input_t *, const char * );
static void inputDrop( aout_input_t *, aout_buffer_t * );
static void inputResamplingStop( aout_input_t *p_input );

static int VisualizationCallback( vlc_object_t *, char const *,
                                  vlc_value_t, vlc_value_t, void * );
static int EqualizerCallback( vlc_object_t *, char const *,
                              vlc_value_t, vlc_value_t, void * );
static int ReplayGainCallback( vlc_object_t *, char const *,
                               vlc_value_t, vlc_value_t, void * );
static void ReplayGainSelect( aout_instance_t *, aout_input_t * );

static vout_thread_t *RequestVout( void *,
                                   vout_thread_t *, video_format_t *, bool );

/*****************************************************************************
 * aout_InputNew : allocate a new input and rework the filter pipeline
 *****************************************************************************/
int aout_InputNew( aout_instance_t * p_aout, aout_input_t * p_input, const aout_request_vout_t *p_request_vout )
{
    audio_sample_format_t chain_input_format;
    audio_sample_format_t chain_output_format;
    vlc_value_t val, text;
    char *psz_filters, *psz_visual, *psz_scaletempo;
    int i_visual;

    aout_FormatPrint( p_aout, "input", &p_input->input );

    p_input->i_nb_resamplers = p_input->i_nb_filters = 0;

    /* Prepare FIFO. */
    aout_FifoInit( p_aout, &p_input->mixer.fifo, p_aout->mixer_format.i_rate );
    p_input->mixer.begin = NULL;

    /* */
    if( p_request_vout )
    {
        p_input->request_vout = *p_request_vout;
    }
    else
    {
        p_input->request_vout.pf_request_vout = RequestVout;
        p_input->request_vout.p_private = p_aout;
    }

    /* Prepare format structure */
    chain_input_format  = p_input->input;
    chain_output_format = p_aout->mixer_format;
    chain_output_format.i_rate = p_input->input.i_rate;
    aout_FormatPrepare( &chain_output_format );

    /* Now add user filters */
    if( var_Type( p_aout, "visual" ) == 0 )
    {
        var_Create( p_aout, "visual", VLC_VAR_STRING | VLC_VAR_HASCHOICE );
        text.psz_string = _("Visualizations");
        var_Change( p_aout, "visual", VLC_VAR_SETTEXT, &text, NULL );
        val.psz_string = (char*)""; text.psz_string = _("Disable");
        var_Change( p_aout, "visual", VLC_VAR_ADDCHOICE, &val, &text );
        val.psz_string = (char*)"spectrometer"; text.psz_string = _("Spectrometer");
        var_Change( p_aout, "visual", VLC_VAR_ADDCHOICE, &val, &text );
        val.psz_string = (char*)"scope"; text.psz_string = _("Scope");
        var_Change( p_aout, "visual", VLC_VAR_ADDCHOICE, &val, &text );
        val.psz_string = (char*)"spectrum"; text.psz_string = _("Spectrum");
        var_Change( p_aout, "visual", VLC_VAR_ADDCHOICE, &val, &text );
        val.psz_string = (char*)"vuMeter"; text.psz_string = _("Vu meter");
        var_Change( p_aout, "visual", VLC_VAR_ADDCHOICE, &val, &text );

        /* Look for goom plugin */
        if( module_exists( "goom" ) )
        {
            val.psz_string = (char*)"goom"; text.psz_string = (char*)"Goom";
            var_Change( p_aout, "visual", VLC_VAR_ADDCHOICE, &val, &text );
        }

        /* Look for libprojectM plugin */
        if( module_exists( "projectm" ) )
        {
            val.psz_string = (char*)"projectm"; text.psz_string = (char*)"projectM";
            var_Change( p_aout, "visual", VLC_VAR_ADDCHOICE, &val, &text );
        }

        if( var_Get( p_aout, "effect-list", &val ) == VLC_SUCCESS )
        {
            var_SetString( p_aout, "visual", val.psz_string );
            free( val.psz_string );
        }
        var_AddCallback( p_aout, "visual", VisualizationCallback, NULL );
    }

    if( var_Type( p_aout, "equalizer" ) == 0 )
    {
        module_config_t *p_config;
        int i;

        p_config = config_FindConfig( VLC_OBJECT(p_aout), "equalizer-preset" );
        if( p_config && p_config->i_list )
        {
               var_Create( p_aout, "equalizer",
                           VLC_VAR_STRING | VLC_VAR_HASCHOICE );
            text.psz_string = _("Equalizer");
            var_Change( p_aout, "equalizer", VLC_VAR_SETTEXT, &text, NULL );

            val.psz_string = (char*)""; text.psz_string = _("Disable");
            var_Change( p_aout, "equalizer", VLC_VAR_ADDCHOICE, &val, &text );

            for( i = 0; i < p_config->i_list; i++ )
            {
                val.psz_string = (char *)p_config->ppsz_list[i];
                text.psz_string = (char *)p_config->ppsz_list_text[i];
                var_Change( p_aout, "equalizer", VLC_VAR_ADDCHOICE,
                            &val, &text );
            }

            var_AddCallback( p_aout, "equalizer", EqualizerCallback, NULL );
        }
    }

    if( var_Type( p_aout, "audio-filter" ) == 0 )
    {
        var_Create( p_aout, "audio-filter",
                    VLC_VAR_STRING | VLC_VAR_DOINHERIT );
        text.psz_string = _("Audio filters");
        var_Change( p_aout, "audio-filter", VLC_VAR_SETTEXT, &text, NULL );
    }
    if( var_Type( p_aout, "audio-visual" ) == 0 )
    {
        var_Create( p_aout, "audio-visual",
                    VLC_VAR_STRING | VLC_VAR_DOINHERIT );
        text.psz_string = _("Audio visualizations");
        var_Change( p_aout, "audio-visual", VLC_VAR_SETTEXT, &text, NULL );
    }

    if( var_Type( p_aout, "audio-replay-gain-mode" ) == 0 )
    {
        module_config_t *p_config;
        int i;

        p_config = config_FindConfig( VLC_OBJECT(p_aout), "audio-replay-gain-mode" );
        if( p_config && p_config->i_list )
        {
            var_Create( p_aout, "audio-replay-gain-mode",
                        VLC_VAR_STRING | VLC_VAR_DOINHERIT );

            text.psz_string = _("Replay gain");
            var_Change( p_aout, "audio-replay-gain-mode", VLC_VAR_SETTEXT, &text, NULL );

            for( i = 0; i < p_config->i_list; i++ )
            {
                val.psz_string = (char *)p_config->ppsz_list[i];
                text.psz_string = (char *)p_config->ppsz_list_text[i];
                var_Change( p_aout, "audio-replay-gain-mode", VLC_VAR_ADDCHOICE,
                            &val, &text );
            }

            var_AddCallback( p_aout, "audio-replay-gain-mode", ReplayGainCallback, NULL );
        }
    }
    if( var_Type( p_aout, "audio-replay-gain-preamp" ) == 0 )
    {
        var_Create( p_aout, "audio-replay-gain-preamp",
                    VLC_VAR_FLOAT | VLC_VAR_DOINHERIT );
    }
    if( var_Type( p_aout, "audio-replay-gain-default" ) == 0 )
    {
        var_Create( p_aout, "audio-replay-gain-default",
                    VLC_VAR_FLOAT | VLC_VAR_DOINHERIT );
    }
    if( var_Type( p_aout, "audio-replay-gain-peak-protection" ) == 0 )
    {
        var_Create( p_aout, "audio-replay-gain-peak-protection",
                    VLC_VAR_BOOL | VLC_VAR_DOINHERIT );
    }
    if( var_Type( p_aout, "audio-time-stretch" ) == 0 )
    {
        var_Create( p_aout, "audio-time-stretch",
                    VLC_VAR_BOOL | VLC_VAR_DOINHERIT );
    }

    psz_filters = var_GetString( p_aout, "audio-filter" );
    psz_visual = var_GetString( p_aout, "audio-visual");
    psz_scaletempo = var_GetBool( p_aout, "audio-time-stretch" ) ? strdup( "scaletempo" ) : NULL;

    p_input->b_recycle_vout = psz_visual && *psz_visual;

    /* parse user filter lists */
    char *const ppsz_array[] = { psz_scaletempo, psz_filters, psz_visual };
    p_input->p_playback_rate_filter = NULL;

    for( i_visual = 0; i_visual < 3 && !AOUT_FMT_NON_LINEAR(&chain_output_format); i_visual++ )
    {
        char *psz_next = NULL;
        char *psz_parser = ppsz_array[i_visual];

        if( psz_parser == NULL || !*psz_parser )
            continue;

        while( psz_parser && *psz_parser )
        {
            filter_t * p_filter = NULL;

            if( p_input->i_nb_filters >= AOUT_MAX_FILTERS )
            {
                msg_Dbg( p_aout, "max filters reached (%d)", AOUT_MAX_FILTERS );
                break;
            }

            while( *psz_parser == ' ' && *psz_parser == ':' )
            {
                psz_parser++;
            }
            if( ( psz_next = strchr( psz_parser , ':'  ) ) )
            {
                *psz_next++ = '\0';
            }
            if( *psz_parser =='\0' )
            {
                break;
            }

            /* Create a VLC object */
            static const char typename[] = "audio filter";
            p_filter = vlc_custom_create( p_aout, sizeof(*p_filter),
                                          VLC_OBJECT_GENERIC, typename );
            if( p_filter == NULL )
            {
                msg_Err( p_aout, "cannot add user filter %s (skipped)",
                         psz_parser );
                psz_parser = psz_next;
                continue;
            }

            vlc_object_attach( p_filter , p_aout );

            p_filter->p_owner = malloc( sizeof(*p_filter->p_owner) );
            p_filter->p_owner->p_aout  = p_aout;
            p_filter->p_owner->p_input = p_input;

            /* request format */
            memcpy( &p_filter->fmt_in.audio, &chain_output_format,
                    sizeof(audio_sample_format_t) );
            p_filter->fmt_in.i_codec = chain_output_format.i_format;
            memcpy( &p_filter->fmt_out.audio, &chain_output_format,
                    sizeof(audio_sample_format_t) );
            p_filter->fmt_out.i_codec = chain_output_format.i_format;
            p_filter->pf_audio_buffer_new = aout_FilterBufferNew;

            /* try to find the requested filter */
            if( i_visual == 2 ) /* this can only be a visualization module */
            {
                p_filter->p_module = module_need( p_filter, "visualization2",
                                                  psz_parser, true );
            }
            else /* this can be a audio filter module as well as a visualization module */
            {
                p_filter->p_module = module_need( p_filter, "audio filter",
                                              psz_parser, true );

                if ( p_filter->p_module == NULL )
                {
                    /* if the filter requested a special format, retry */
                    if ( !( AOUT_FMTS_IDENTICAL( &p_filter->fmt_in.audio,
                                                 &chain_input_format )
                            && AOUT_FMTS_IDENTICAL( &p_filter->fmt_out.audio,
                                                    &chain_output_format ) ) )
                    {
                        aout_FormatPrepare( &p_filter->fmt_in.audio );
                        aout_FormatPrepare( &p_filter->fmt_out.audio );
                        p_filter->p_module = module_need( p_filter,
                                                          "audio filter",
                                                          psz_parser, true );
                    }
                    /* try visual filters */
                    else
                    {
                        memcpy( &p_filter->fmt_in.audio, &chain_output_format,
                                sizeof(audio_sample_format_t) );
                        memcpy( &p_filter->fmt_out.audio, &chain_output_format,
                                sizeof(audio_sample_format_t) );
                        p_filter->p_module = module_need( p_filter,
                                                          "visualization2",
                                                          psz_parser, true );
                    }
                }
            }

            /* failure */
            if ( p_filter->p_module == NULL )
            {
                msg_Err( p_aout, "cannot add user filter %s (skipped)",
                         psz_parser );

                free( p_filter->p_owner );
                vlc_object_release( p_filter );

                psz_parser = psz_next;
                continue;
            }

            /* complete the filter chain if necessary */
            if ( !AOUT_FMTS_IDENTICAL( &chain_input_format,
                                       &p_filter->fmt_in.audio ) )
            {
                if ( aout_FiltersCreatePipeline( p_aout, p_input->pp_filters,
                                                 &p_input->i_nb_filters,
                                                 &chain_input_format,
                                                 &p_filter->fmt_in.audio ) < 0 )
                {
                    msg_Err( p_aout, "cannot add user filter %s (skipped)",
                             psz_parser );

                    module_unneed( p_filter, p_filter->p_module );
                    free( p_filter->p_owner );
                    vlc_object_release( p_filter );

                    psz_parser = psz_next;
                    continue;
                }
            }

            /* success */
            p_input->pp_filters[p_input->i_nb_filters++] = p_filter;
            memcpy( &chain_input_format, &p_filter->fmt_out.audio,
                    sizeof( audio_sample_format_t ) );

            if( i_visual == 0 ) /* scaletempo */
                p_input->p_playback_rate_filter = p_filter;

            /* next filter if any */
            psz_parser = psz_next;
        }
    }
    free( psz_visual );
    free( psz_filters );
    free( psz_scaletempo );

    /* complete the filter chain if necessary */
    if ( !AOUT_FMTS_IDENTICAL( &chain_input_format, &chain_output_format ) )
    {
        if ( aout_FiltersCreatePipeline( p_aout, p_input->pp_filters,
                                         &p_input->i_nb_filters,
                                         &chain_input_format,
                                         &chain_output_format ) < 0 )
        {
            inputFailure( p_aout, p_input, "couldn't set an input pipeline" );
            return -1;
        }
    }

    /* Prepare hints for the buffer allocator. */
    p_input->input_alloc.b_alloc = true;
    p_input->input_alloc.i_bytes_per_sec = -1;

    /* Create resamplers. */
    if ( !AOUT_FMT_NON_LINEAR( &p_aout->mixer_format ) )
    {
        chain_output_format.i_rate = (__MAX(p_input->input.i_rate,
                                            p_aout->mixer_format.i_rate)
                                 * (100 + AOUT_MAX_RESAMPLING)) / 100;
        if ( chain_output_format.i_rate == p_aout->mixer_format.i_rate )
        {
            /* Just in case... */
            chain_output_format.i_rate++;
        }
        if ( aout_FiltersCreatePipeline( p_aout, p_input->pp_resamplers,
                                         &p_input->i_nb_resamplers,
                                         &chain_output_format,
                                         &p_aout->mixer_format ) < 0 )
        {
            inputFailure( p_aout, p_input, "couldn't set a resampler pipeline");
            return -1;
        }

        aout_FiltersHintBuffers( p_aout, p_input->pp_resamplers,
                                 p_input->i_nb_resamplers,
                                 &p_input->input_alloc );
        p_input->input_alloc.b_alloc = true;

        /* Setup the initial rate of the resampler */
        p_input->pp_resamplers[0]->fmt_in.audio.i_rate = p_input->input.i_rate;
    }
    p_input->i_resampling_type = AOUT_RESAMPLING_NONE;

    if( ! p_input->p_playback_rate_filter && p_input->i_nb_resamplers > 0 )
    {
        p_input->p_playback_rate_filter = p_input->pp_resamplers[0];
    }

    aout_FiltersHintBuffers( p_aout, p_input->pp_filters,
                             p_input->i_nb_filters,
                             &p_input->input_alloc );
    p_input->input_alloc.b_alloc = true;

    /* i_bytes_per_sec is still == -1 if no filters */
    p_input->input_alloc.i_bytes_per_sec = __MAX(
                                    p_input->input_alloc.i_bytes_per_sec,
                                    (int)(p_input->input.i_bytes_per_frame
                                     * p_input->input.i_rate
                                     / p_input->input.i_frame_length) );

    ReplayGainSelect( p_aout, p_input );

    /* Success */
    p_input->b_error = false;
    p_input->i_last_input_rate = INPUT_RATE_DEFAULT;

    return 0;
}

/*****************************************************************************
 * aout_InputDelete : delete an input
 *****************************************************************************
 * This function must be entered with the mixer lock.
 *****************************************************************************/
int aout_InputDelete( aout_instance_t * p_aout, aout_input_t * p_input )
{
    AOUT_ASSERT_MIXER_LOCKED;
    if ( p_input->b_error )
        return 0;

    /* XXX We need to update b_recycle_vout before calling aout_FiltersDestroyPipeline.
     * FIXME They can be a race condition if audio-visual is updated between
     * aout_InputDelete and aout_InputNew.
     */
    char *psz_visual = var_GetString( p_aout, "audio-visual");
    p_input->b_recycle_vout = psz_visual && *psz_visual;
    free( psz_visual );

    aout_FiltersDestroyPipeline( p_aout, p_input->pp_filters,
                                 p_input->i_nb_filters );
    p_input->i_nb_filters = 0;
    aout_FiltersDestroyPipeline( p_aout, p_input->pp_resamplers,
                                 p_input->i_nb_resamplers );
    p_input->i_nb_resamplers = 0;
    aout_FifoDestroy( p_aout, &p_input->mixer.fifo );

    return 0;
}

/*****************************************************************************
 * aout_InputCheckAndRestart : restart an input
 *****************************************************************************
 * This function must be entered with the input and mixer lock.
 *****************************************************************************/
void aout_InputCheckAndRestart( aout_instance_t * p_aout, aout_input_t * p_input )
{
    AOUT_ASSERT_MIXER_LOCKED;
    AOUT_ASSERT_INPUT_LOCKED;

    if( !p_input->b_restart )
        return;

    aout_lock_input_fifos( p_aout );

    /* A little trick to avoid loosing our input fifo and properties */

    uint8_t *p_first_byte_to_mix = p_input->mixer.begin;
    aout_fifo_t fifo = p_input->mixer.fifo;
    bool b_paused = p_input->b_paused;
    mtime_t i_pause_date = p_input->i_pause_date;

    aout_FifoInit( p_aout, &p_input->mixer.fifo, p_aout->mixer_format.i_rate );

    aout_InputDelete( p_aout, p_input );

    aout_InputNew( p_aout, p_input, &p_input->request_vout );
    p_input->mixer.begin = p_first_byte_to_mix;
    p_input->mixer.fifo = fifo;
    p_input->b_paused = b_paused;
    p_input->i_pause_date = i_pause_date;

    p_input->b_restart = false;

    aout_unlock_input_fifos( p_aout );
}
/*****************************************************************************
 * aout_InputPlay : play a buffer
 *****************************************************************************
 * This function must be entered with the input lock.
 *****************************************************************************/
/* XXX Do not activate it !! */
//#define AOUT_PROCESS_BEFORE_CHEKS
int aout_InputPlay( aout_instance_t * p_aout, aout_input_t * p_input,
                    aout_buffer_t * p_buffer, int i_input_rate )
{
    mtime_t start_date;
    AOUT_ASSERT_INPUT_LOCKED;

    if( i_input_rate != INPUT_RATE_DEFAULT && p_input->p_playback_rate_filter == NULL )
    {
        inputDrop( p_input, p_buffer );
        return 0;
    }

#ifdef AOUT_PROCESS_BEFORE_CHEKS
    /* Run pre-filters. */
    aout_FiltersPlay( p_aout, p_input->pp_filters, p_input->i_nb_filters,
                      &p_buffer );
    if( !p_buffer )
        return 0;

    /* Actually run the resampler now. */
    if ( p_input->i_nb_resamplers > 0 )
    {
        const mtime_t i_date = p_buffer->i_pts;
        aout_FiltersPlay( p_aout, p_input->pp_resamplers,
                          p_input->i_nb_resamplers,
                          &p_buffer );
    }

    if( !p_buffer )
        return 0;
    if( p_buffer->i_nb_samples <= 0 )
    {
        block_Release( p_buffer );
        return 0;
    }
#endif

    /* Handle input rate change, but keep drift correction */
    if( i_input_rate != p_input->i_last_input_rate )
    {
        unsigned int * const pi_rate = &p_input->p_playback_rate_filter->fmt_in.audio.i_rate;
#define F(r,ir) ( INPUT_RATE_DEFAULT * (r) / (ir) )
        const int i_delta = *pi_rate - F(p_input->input.i_rate,p_input->i_last_input_rate);
        *pi_rate = F(p_input->input.i_rate + i_delta, i_input_rate);
#undef F
        p_input->i_last_input_rate = i_input_rate;
    }

    /* We don't care if someone changes the start date behind our back after
     * this. We'll deal with that when pushing the buffer, and compensate
     * with the next incoming buffer. */
    aout_lock_input_fifos( p_aout );
    start_date = aout_FifoNextStart( p_aout, &p_input->mixer.fifo );
    aout_unlock_input_fifos( p_aout );

    if ( start_date != 0 && start_date < mdate() )
    {
        /* The decoder is _very_ late. This can only happen if the user
         * pauses the stream (or if the decoder is buggy, which cannot
         * happen :). */
        msg_Warn( p_aout, "computed PTS is out of range (%"PRId64"), "
                  "clearing out", mdate() - start_date );
        aout_lock_input_fifos( p_aout );
        aout_FifoSet( p_aout, &p_input->mixer.fifo, 0 );
        p_input->mixer.begin = NULL;
        aout_unlock_input_fifos( p_aout );
        if ( p_input->i_resampling_type != AOUT_RESAMPLING_NONE )
            msg_Warn( p_aout, "timing screwed, stopping resampling" );
        inputResamplingStop( p_input );
        p_buffer->i_flags |= BLOCK_FLAG_DISCONTINUITY;
        start_date = 0;
    }

    if ( p_buffer->i_pts < mdate() + AOUT_MIN_PREPARE_TIME )
    {
        /* The decoder gives us f*cked up PTS. It's its business, but we
         * can't present it anyway, so drop the buffer. */
        msg_Warn( p_aout, "PTS is out of range (%"PRId64"), dropping buffer",
                  mdate() - p_buffer->i_pts );

        inputDrop( p_input, p_buffer );
        inputResamplingStop( p_input );
        return 0;
    }

    /* If the audio drift is too big then it's not worth trying to resample
     * the audio. */
    mtime_t i_pts_tolerance = 3 * AOUT_PTS_TOLERANCE * i_input_rate / INPUT_RATE_DEFAULT;
    if ( start_date != 0 &&
         ( start_date < p_buffer->i_pts - i_pts_tolerance ) )
    {
        msg_Warn( p_aout, "audio drift is too big (%"PRId64"), clearing out",
                  start_date - p_buffer->i_pts );
        aout_lock_input_fifos( p_aout );
        aout_FifoSet( p_aout, &p_input->mixer.fifo, 0 );
        p_input->mixer.begin = NULL;
        aout_unlock_input_fifos( p_aout );
        if ( p_input->i_resampling_type != AOUT_RESAMPLING_NONE )
            msg_Warn( p_aout, "timing screwed, stopping resampling" );
        inputResamplingStop( p_input );
        p_buffer->i_flags |= BLOCK_FLAG_DISCONTINUITY;
        start_date = 0;
    }
    else if ( start_date != 0 &&
              ( start_date > p_buffer->i_pts + i_pts_tolerance) )
    {
        msg_Warn( p_aout, "audio drift is too big (%"PRId64"), dropping buffer",
                  start_date - p_buffer->i_pts );
        inputDrop( p_input, p_buffer );
        return 0;
    }

    if ( start_date == 0 ) start_date = p_buffer->i_pts;

#ifndef AOUT_PROCESS_BEFORE_CHEKS
    /* Run pre-filters. */
    aout_FiltersPlay( p_input->pp_filters, p_input->i_nb_filters, &p_buffer );
    if( !p_buffer )
        return 0;
#endif

    /* Run the resampler if needed.
     * We first need to calculate the output rate of this resampler. */
    if ( ( p_input->i_resampling_type == AOUT_RESAMPLING_NONE ) &&
         ( start_date < p_buffer->i_pts - AOUT_PTS_TOLERANCE
           || start_date > p_buffer->i_pts + AOUT_PTS_TOLERANCE ) &&
         p_input->i_nb_resamplers > 0 )
    {
        /* Can happen in several circumstances :
         * 1. A problem at the input (clock drift)
         * 2. A small pause triggered by the user
         * 3. Some delay in the output stage, causing a loss of lip
         *    synchronization
         * Solution : resample the buffer to avoid a scratch.
         */
        mtime_t drift = p_buffer->i_pts - start_date;

        p_input->i_resamp_start_date = mdate();
        p_input->i_resamp_start_drift = (int)drift;

        if ( drift > 0 )
            p_input->i_resampling_type = AOUT_RESAMPLING_DOWN;
        else
            p_input->i_resampling_type = AOUT_RESAMPLING_UP;

        msg_Warn( p_aout, "buffer is %"PRId64" %s, triggering %ssampling",
                          drift > 0 ? drift : -drift,
                          drift > 0 ? "in advance" : "late",
                          drift > 0 ? "down" : "up");
    }

    if ( p_input->i_resampling_type != AOUT_RESAMPLING_NONE )
    {
        /* Resampling has been triggered previously (because of dates
         * mismatch). We want the resampling to happen progressively so
         * it isn't too audible to the listener. */

        if( p_input->i_resampling_type == AOUT_RESAMPLING_UP )
        {
            p_input->pp_resamplers[0]->fmt_in.audio.i_rate += 2; /* Hz */
        }
        else
        {
            p_input->pp_resamplers[0]->fmt_in.audio.i_rate -= 2; /* Hz */
        }

        /* Check if everything is back to normal, in which case we can stop the
         * resampling */
        unsigned int i_nominal_rate =
          (p_input->pp_resamplers[0] == p_input->p_playback_rate_filter)
          ? INPUT_RATE_DEFAULT * p_input->input.i_rate / i_input_rate
          : p_input->input.i_rate;
        if( p_input->pp_resamplers[0]->fmt_in.audio.i_rate == i_nominal_rate )
        {
            p_input->i_resampling_type = AOUT_RESAMPLING_NONE;
            msg_Warn( p_aout, "resampling stopped after %"PRIi64" usec "
                      "(drift: %"PRIi64")",
                      mdate() - p_input->i_resamp_start_date,
                      p_buffer->i_pts - start_date);
        }
        else if( abs( (int)(p_buffer->i_pts - start_date) ) <
                 abs( p_input->i_resamp_start_drift ) / 2 )
        {
            /* if we reduced the drift from half, then it is time to switch
             * back the resampling direction. */
            if( p_input->i_resampling_type == AOUT_RESAMPLING_UP )
                p_input->i_resampling_type = AOUT_RESAMPLING_DOWN;
            else
                p_input->i_resampling_type = AOUT_RESAMPLING_UP;
            p_input->i_resamp_start_drift = 0;
        }
        else if( p_input->i_resamp_start_drift &&
                 ( abs( (int)(p_buffer->i_pts - start_date) ) >
                   abs( p_input->i_resamp_start_drift ) * 3 / 2 ) )
        {
            /* If the drift is increasing and not decreasing, than something
             * is bad. We'd better stop the resampling right now. */
            msg_Warn( p_aout, "timing screwed, stopping resampling" );
            inputResamplingStop( p_input );
            p_buffer->i_flags |= BLOCK_FLAG_DISCONTINUITY;
        }
    }

#ifndef AOUT_PROCESS_BEFORE_CHEKS
    /* Actually run the resampler now. */
    if ( p_input->i_nb_resamplers > 0 )
    {
        aout_FiltersPlay( p_input->pp_resamplers, p_input->i_nb_resamplers,
                          &p_buffer );
    }

    if( !p_buffer )
        return 0;
    if( p_buffer->i_nb_samples <= 0 )
    {
        block_Release( p_buffer );
        return 0;
    }
#endif

    /* Adding the start date will be managed by aout_FifoPush(). */
    p_buffer->i_pts = start_date;

    aout_lock_input_fifos( p_aout );
    aout_FifoPush( p_aout, &p_input->mixer.fifo, p_buffer );
    aout_unlock_input_fifos( p_aout );
    return 0;
}

/*****************************************************************************
 * static functions
 *****************************************************************************/

static void inputFailure( aout_instance_t * p_aout, aout_input_t * p_input,
                          const char * psz_error_message )
{
    /* error message */
    msg_Err( p_aout, "%s", psz_error_message );

    /* clean up */
    aout_FiltersDestroyPipeline( p_aout, p_input->pp_filters,
                                 p_input->i_nb_filters );
    aout_FiltersDestroyPipeline( p_aout, p_input->pp_resamplers,
                                 p_input->i_nb_resamplers );
    aout_FifoDestroy( p_aout, &p_input->mixer.fifo );
    var_Destroy( p_aout, "visual" );
    var_Destroy( p_aout, "equalizer" );
    var_Destroy( p_aout, "audio-filter" );
    var_Destroy( p_aout, "audio-visual" );

    var_Destroy( p_aout, "audio-replay-gain-mode" );
    var_Destroy( p_aout, "audio-replay-gain-default" );
    var_Destroy( p_aout, "audio-replay-gain-preamp" );
    var_Destroy( p_aout, "audio-replay-gain-peak-protection" );

    /* error flag */
    p_input->b_error = 1;
}

static void inputDrop( aout_input_t *p_input, aout_buffer_t *p_buffer )
{
    aout_BufferFree( p_buffer );

    p_input->i_buffer_lost++;
}

static void inputResamplingStop( aout_input_t *p_input )
{
    p_input->i_resampling_type = AOUT_RESAMPLING_NONE;
    if( p_input->i_nb_resamplers != 0 )
    {
        p_input->pp_resamplers[0]->fmt_in.audio.i_rate =
            ( p_input->pp_resamplers[0] == p_input->p_playback_rate_filter )
            ? INPUT_RATE_DEFAULT * p_input->input.i_rate / p_input->i_last_input_rate
            : p_input->input.i_rate;
    }
}

static vout_thread_t *RequestVout( void *p_private,
                                   vout_thread_t *p_vout, video_format_t *p_fmt, bool b_recycle )
{
    aout_instance_t *p_aout = p_private;
    VLC_UNUSED(b_recycle);
    return vout_Request( p_aout, p_vout, p_fmt );
}

vout_thread_t *aout_filter_RequestVout( filter_t *p_filter,
                                        vout_thread_t *p_vout, video_format_t *p_fmt )
{
    aout_input_t *p_input = p_filter->p_owner->p_input;
    aout_request_vout_t *p_request = &p_input->request_vout;

    /* XXX: this only works from audio input */
    /* If you want to use visualization filters from another place, you will
     * need to add a new pf_aout_request_vout callback or store a pointer
     * to aout_request_vout_t inside filter_t (i.e. a level of indirection). */

    return p_request->pf_request_vout( p_request->p_private,
                                       p_vout, p_fmt, p_input->b_recycle_vout );
}

static int ChangeFiltersString( aout_instance_t * p_aout, const char* psz_variable,
                                 const char *psz_name, bool b_add )
{
    return AoutChangeFilterString( VLC_OBJECT(p_aout), p_aout,
                                   psz_variable, psz_name, b_add ) ? 1 : 0;
}

static int VisualizationCallback( vlc_object_t *p_this, char const *psz_cmd,
                       vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    aout_instance_t *p_aout = (aout_instance_t *)p_this;
    char *psz_mode = newval.psz_string;
    (void)psz_cmd; (void)oldval; (void)p_data;

    if( !psz_mode || !*psz_mode )
    {
        ChangeFiltersString( p_aout, "audio-visual", "goom", false );
        ChangeFiltersString( p_aout, "audio-visual", "visual", false );
        ChangeFiltersString( p_aout, "audio-visual", "projectm", false );
    }
    else
    {
        if( !strcmp( "goom", psz_mode ) )
        {
            ChangeFiltersString( p_aout, "audio-visual", "visual", false );
            ChangeFiltersString( p_aout, "audio-visual", "goom", true );
            ChangeFiltersString( p_aout, "audio-visual", "projectm", false );
        }
        else if( !strcmp( "projectm", psz_mode ) )
        {
            ChangeFiltersString( p_aout, "audio-visual", "visual", false );
            ChangeFiltersString( p_aout, "audio-visual", "goom", false );
            ChangeFiltersString( p_aout, "audio-visual", "projectm", true );
        }
        else
        {
            var_Create( p_aout, "effect-list", VLC_VAR_STRING );
            var_SetString( p_aout, "effect-list", psz_mode );

            ChangeFiltersString( p_aout, "audio-visual", "goom", false );
            ChangeFiltersString( p_aout, "audio-visual", "visual", true );
            ChangeFiltersString( p_aout, "audio-visual", "projectm", false );
        }
    }

    /* That sucks */
    AoutInputsMarkToRestart( p_aout );

    return VLC_SUCCESS;
}

static int EqualizerCallback( vlc_object_t *p_this, char const *psz_cmd,
                       vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    aout_instance_t *p_aout = (aout_instance_t *)p_this;
    char *psz_mode = newval.psz_string;
    int i_ret;
    (void)psz_cmd; (void)oldval; (void)p_data;

    if( !psz_mode || !*psz_mode )
    {
        i_ret = ChangeFiltersString( p_aout, "audio-filter", "equalizer",
                                     false );
    }
    else
    {
        var_Create( p_aout, "equalizer-preset", VLC_VAR_STRING );
        var_SetString( p_aout, "equalizer-preset", psz_mode );
        i_ret = ChangeFiltersString( p_aout, "audio-filter", "equalizer",
                                     true );
    }

    /* That sucks */
    if( i_ret == 1 )
        AoutInputsMarkToRestart( p_aout );
    return VLC_SUCCESS;
}

static int ReplayGainCallback( vlc_object_t *p_this, char const *psz_cmd,
                               vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    VLC_UNUSED(psz_cmd); VLC_UNUSED(oldval);
    VLC_UNUSED(newval); VLC_UNUSED(p_data);
    aout_instance_t *p_aout = (aout_instance_t *)p_this;
    int i;

    aout_lock_mixer( p_aout );
    for( i = 0; i < p_aout->i_nb_inputs; i++ )
        ReplayGainSelect( p_aout, p_aout->pp_inputs[i] );

    /* Restart the mixer (a trivial mixer may be in use) */
    if( p_aout->p_mixer )
        aout_MixerMultiplierSet( p_aout, p_aout->mixer_multiplier );
    aout_unlock_mixer( p_aout );

    return VLC_SUCCESS;
}

static void ReplayGainSelect( aout_instance_t *p_aout, aout_input_t *p_input )
{
    char *psz_replay_gain = var_GetNonEmptyString( p_aout,
                                                   "audio-replay-gain-mode" );
    int i_mode;
    int i_use;
    float f_gain;

    p_input->mixer.multiplier = 1.0;

    if( !psz_replay_gain )
        return;

    /* Find select mode */
    if( !strcmp( psz_replay_gain, "track" ) )
        i_mode = AUDIO_REPLAY_GAIN_TRACK;
    else if( !strcmp( psz_replay_gain, "album" ) )
        i_mode = AUDIO_REPLAY_GAIN_ALBUM;
    else
        i_mode = AUDIO_REPLAY_GAIN_MAX;

    /* If the select mode is not available, prefer the other one */
    i_use = i_mode;
    if( i_use != AUDIO_REPLAY_GAIN_MAX && !p_input->replay_gain.pb_gain[i_use] )
    {
        for( i_use = 0; i_use < AUDIO_REPLAY_GAIN_MAX; i_use++ )
        {
            if( p_input->replay_gain.pb_gain[i_use] )
                break;
        }
    }

    /* */
    if( i_use != AUDIO_REPLAY_GAIN_MAX )
        f_gain = p_input->replay_gain.pf_gain[i_use] + var_GetFloat( p_aout, "audio-replay-gain-preamp" );
    else if( i_mode != AUDIO_REPLAY_GAIN_MAX )
        f_gain = var_GetFloat( p_aout, "audio-replay-gain-default" );
    else
        f_gain = 0.0;
    p_input->mixer.multiplier = pow( 10.0, f_gain / 20.0 );

    /* */
    if( p_input->replay_gain.pb_peak[i_use] &&
        var_GetBool( p_aout, "audio-replay-gain-peak-protection" ) &&
        p_input->replay_gain.pf_peak[i_use] * p_input->mixer.multiplier > 1.0 )
    {
        p_input->mixer.multiplier = 1.0f / p_input->replay_gain.pf_peak[i_use];
    }

    free( psz_replay_gain );
}

