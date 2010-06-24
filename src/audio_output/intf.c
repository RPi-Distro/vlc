/*****************************************************************************
 * intf.c : audio output API towards the interface modules
 *****************************************************************************
 * Copyright (C) 2002-2007 the VideoLAN team
 * $Id: 493ad5c71bb22c768390ecac596b70adc92d0f99 $
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

#include <stdio.h>
#include <stdlib.h>                            /* calloc(), malloc(), free() */
#include <string.h>

#include <vlc_aout.h>
#include "aout_internal.h"

#include <vlc_playlist.h>

static aout_instance_t *findAout (vlc_object_t *obj)
{
    input_thread_t *(*pf_find_input) (vlc_object_t *);

    pf_find_input = var_GetAddress (obj, "find-input-callback");
    if (unlikely(pf_find_input == NULL))
        return NULL;

    input_thread_t *p_input = pf_find_input (obj);
    if (p_input == NULL)
       return NULL;

    aout_instance_t *p_aout = input_GetAout (p_input);
    vlc_object_release (p_input);
    return p_aout;
}
#define findAout(o) findAout(VLC_OBJECT(o))

/*
 * Volume management
 *
 * The hardware volume cannot be set if the output module gets deleted, so
 * we must take the mixer lock. The software volume cannot be set while the
 * mixer is running, so we need the mixer lock (too).
 *
 * Here is a schematic of the i_volume range :
 *
 * |------------------------------+---------------------------------------|
 * 0                           pi_soft                                   1024
 *
 * Between 0 and pi_soft, the volume is done in hardware by the output
 * module. Above, the output module will change p_aout->mixer.i_multiplier
 * (done in software). This scaling may result * in cropping errors and
 * should be avoided as much as possible.
 *
 * It is legal to have *pi_soft == 0, and do everything in software.
 * It is also legal to have *pi_soft == 1024, and completely avoid
 * software scaling. However, some streams (esp. A/52) are encoded with
 * a very low volume and users may complain.
 */

enum {
    SET_MUTE=1,
    SET_VOLUME=2,
    INCREMENT_VOLUME=4,
    TOGGLE_MUTE=8
};

/*****************************************************************************
 * doVolumeChanges : handle all volume changes. Internal use only to ease
 *                   variables locking.
 *****************************************************************************/
static
int doVolumeChanges( unsigned action, vlc_object_t * p_object, int i_nb_steps,
                audio_volume_t i_volume, audio_volume_t * i_return_volume,
                bool b_mute )
{
    int i_result = VLC_SUCCESS;
    int i_volume_step = 1, i_new_volume = 0;
    bool b_var_mute = false;
    aout_instance_t *p_aout = findAout( p_object );

    if ( p_aout ) aout_lock_volume( p_aout );

    b_var_mute = var_GetBool( p_object, "volume-muted");

    const bool b_unmute_condition = ( b_var_mute
                && ( /* Unmute: on increments */
                    ( action == INCREMENT_VOLUME )
                    || /* On explicit unmute */
                    ( ( action == SET_MUTE ) && !b_mute )
                    || /* On toggle from muted */
                    ( action == TOGGLE_MUTE )
                ));

    const bool b_mute_condition = ( !b_var_mute
                    && ( /* explicit */
                        ( ( action == SET_MUTE ) && b_mute )
                        || /* or toggle */
                        ( action == TOGGLE_MUTE )
                    ));

    /* If muting or unmuting when play hasn't started */
    if ( action == SET_MUTE && !b_unmute_condition && !b_mute_condition )
    {
        if ( p_aout )
        {
            aout_unlock_volume( p_aout );
            vlc_object_release( p_aout );
        }
        return i_result;
    }

    /* On UnMute */
    if ( b_unmute_condition )
    {
        /* Restore saved volume */
        i_volume = var_GetInteger( p_object, "saved-volume" );
        var_SetBool( p_object, "volume-muted", false );
    }
    else if ( b_mute_condition )
    {
        /* We need an initial value to backup later */
        i_volume = config_GetInt( p_object, "volume" );
    }

    if ( action == INCREMENT_VOLUME )
    {
        i_volume_step = var_InheritInteger( p_object, "volume-step" );

        if ( !b_unmute_condition )
            i_volume = config_GetInt( p_object, "volume" );

        i_new_volume = (int) i_volume + i_volume_step * i_nb_steps;

        if ( i_new_volume > AOUT_VOLUME_MAX )
            i_volume = AOUT_VOLUME_MAX;
        else if ( i_new_volume < AOUT_VOLUME_MIN )
            i_volume = AOUT_VOLUME_MIN;
        else
            i_volume = i_new_volume;
    }

    var_SetInteger( p_object, "saved-volume" , i_volume );

    /* On Mute */
    if ( b_mute_condition )
    {
        i_volume = AOUT_VOLUME_MIN;
        var_SetBool( p_object, "volume-muted", true );
    }

    /* Commit volume changes */
    config_PutInt( p_object, "volume", i_volume );

    if ( p_aout )
    {
        aout_lock_mixer( p_aout );
        aout_lock_input_fifos( p_aout );
            if ( p_aout->p_mixer )
                i_result = p_aout->output.pf_volume_set( p_aout, i_volume );
        aout_unlock_input_fifos( p_aout );
        aout_unlock_mixer( p_aout );
    }

    /* trigger callbacks */
    var_TriggerCallback( p_object, "volume-change" );
    if ( p_aout )
    {
        var_SetBool( p_aout, "intf-change", true );
        aout_unlock_volume( p_aout );
        vlc_object_release( p_aout );
    }

    if ( i_return_volume != NULL )
         *i_return_volume = i_volume;
    return i_result;
}

#undef aout_VolumeGet
/*****************************************************************************
 * aout_VolumeGet : get the volume of the output device
 *****************************************************************************/
int aout_VolumeGet( vlc_object_t * p_object, audio_volume_t * pi_volume )
{
    int i_result = 0;
    aout_instance_t * p_aout = findAout( p_object );

    if ( pi_volume == NULL ) return -1;

    if ( p_aout == NULL )
    {
        *pi_volume = (audio_volume_t)config_GetInt( p_object, "volume" );
        return 0;
    }

    aout_lock_volume( p_aout );
    aout_lock_mixer( p_aout );
    if ( p_aout->p_mixer )
    {
        i_result = p_aout->output.pf_volume_get( p_aout, pi_volume );
    }
    else
    {
        *pi_volume = (audio_volume_t)config_GetInt( p_object, "volume" );
    }
    aout_unlock_mixer( p_aout );
    aout_unlock_volume( p_aout );

    vlc_object_release( p_aout );
    return i_result;
}

#undef aout_VolumeSet
/*****************************************************************************
 * aout_VolumeSet : set the volume of the output device
 *****************************************************************************/
int aout_VolumeSet( vlc_object_t * p_object, audio_volume_t i_volume )
{
    return doVolumeChanges( SET_VOLUME, p_object, 1, i_volume, NULL, true );
}

#undef aout_VolumeUp
/*****************************************************************************
 * aout_VolumeUp : raise the output volume
 *****************************************************************************
 * If pi_volume != NULL, *pi_volume will contain the volume at the end of the
 * function.
 *****************************************************************************/
int aout_VolumeUp( vlc_object_t * p_object, int i_nb_steps,
                   audio_volume_t * pi_volume )
{
    return doVolumeChanges( INCREMENT_VOLUME, p_object, i_nb_steps, 0, pi_volume, true );
}

#undef aout_VolumeDown
/*****************************************************************************
 * aout_VolumeDown : lower the output volume
 *****************************************************************************
 * If pi_volume != NULL, *pi_volume will contain the volume at the end of the
 * function.
 *****************************************************************************/
int aout_VolumeDown( vlc_object_t * p_object, int i_nb_steps,
                     audio_volume_t * pi_volume )
{
    return aout_VolumeUp( p_object, -i_nb_steps, pi_volume );
}

#undef aout_ToggleMute
/*****************************************************************************
 * aout_ToggleMute : Mute/un-mute the output volume
 *****************************************************************************
 * If pi_volume != NULL, *pi_volume will contain the volume at the end of the
 * function (muted => 0).
 *****************************************************************************/
int aout_ToggleMute( vlc_object_t * p_object, audio_volume_t * pi_volume )
{
    return doVolumeChanges( TOGGLE_MUTE, p_object, 1, 0, pi_volume, true );
}

/*****************************************************************************
 * aout_IsMuted : Get the output volume mute status
 *****************************************************************************/
bool aout_IsMuted( vlc_object_t * p_object )
{
    bool b_return_val;
    aout_instance_t * p_aout = findAout( p_object );
    if ( p_aout ) aout_lock_volume( p_aout );
    b_return_val = var_GetBool( p_object, "volume-muted");
    if ( p_aout )
    {
        aout_unlock_volume( p_aout );
        vlc_object_release( p_aout );
    }
    return b_return_val;
}

/*****************************************************************************
 * aout_SetMute : Sets mute status
 *****************************************************************************
 * If pi_volume != NULL, *pi_volume will contain the volume at the end of the
 * function (muted => 0).
 *****************************************************************************/
int aout_SetMute( vlc_object_t * p_object, audio_volume_t * pi_volume,
                  bool b_mute )
{
    return doVolumeChanges( SET_MUTE, p_object, 1, 0, pi_volume, b_mute );
}

/*
 * The next functions are not supposed to be called by the interface, but
 * are placeholders for software-only scaling.
 */

/* Meant to be called by the output plug-in's Open(). */
void aout_VolumeSoftInit( aout_instance_t * p_aout )
{
    int i_volume;

    p_aout->output.pf_volume_get = aout_VolumeSoftGet;
    p_aout->output.pf_volume_set = aout_VolumeSoftSet;

    i_volume = config_GetInt( p_aout, "volume" );
    if ( i_volume < AOUT_VOLUME_MIN )
    {
        i_volume = AOUT_VOLUME_DEFAULT;
    }
    else if ( i_volume > AOUT_VOLUME_MAX )
    {
        i_volume = AOUT_VOLUME_MAX;
    }

    aout_VolumeSoftSet( p_aout, (audio_volume_t)i_volume );
}

/* Placeholder for pf_volume_get(). */
int aout_VolumeSoftGet( aout_instance_t * p_aout, audio_volume_t * pi_volume )
{
    *pi_volume = p_aout->output.i_volume;
    return 0;
}


/* Placeholder for pf_volume_set(). */
int aout_VolumeSoftSet( aout_instance_t * p_aout, audio_volume_t i_volume )
{
    aout_MixerMultiplierSet( p_aout, (float)i_volume / AOUT_VOLUME_DEFAULT );
    p_aout->output.i_volume = i_volume;
    return 0;
}

/*
 * The next functions are not supposed to be called by the interface, but
 * are placeholders for unsupported scaling.
 */

/* Meant to be called by the output plug-in's Open(). */
void aout_VolumeNoneInit( aout_instance_t * p_aout )
{
    p_aout->output.pf_volume_get = aout_VolumeNoneGet;
    p_aout->output.pf_volume_set = aout_VolumeNoneSet;
}

/* Placeholder for pf_volume_get(). */
int aout_VolumeNoneGet( aout_instance_t * p_aout, audio_volume_t * pi_volume )
{
    (void)p_aout; (void)pi_volume;
    return -1;
}

/* Placeholder for pf_volume_set(). */
int aout_VolumeNoneSet( aout_instance_t * p_aout, audio_volume_t i_volume )
{
    (void)p_aout; (void)i_volume;
    return -1;
}


/*
 * Pipelines management
 */

/*****************************************************************************
 * aout_Restart : re-open the output device and rebuild the input and output
 *                pipelines
 *****************************************************************************
 * This function is used whenever the parameters of the output plug-in are
 * changed (eg. selecting S/PDIF or PCM).
 *****************************************************************************/
static int aout_Restart( aout_instance_t * p_aout )
{
    int i;
    bool b_error = 0;

    aout_lock_mixer( p_aout );

    if ( p_aout->i_nb_inputs == 0 )
    {
        aout_unlock_mixer( p_aout );
        msg_Err( p_aout, "no decoder thread" );
        return -1;
    }

    for ( i = 0; i < p_aout->i_nb_inputs; i++ )
    {
        aout_lock_input( p_aout, p_aout->pp_inputs[i] );
        aout_lock_input_fifos( p_aout );
        aout_InputDelete( p_aout, p_aout->pp_inputs[i] );
        aout_unlock_input_fifos( p_aout );
    }

    /* Lock all inputs. */
    aout_lock_input_fifos( p_aout );
    aout_MixerDelete( p_aout );

    /* Re-open the output plug-in. */
    aout_OutputDelete( p_aout );

    if ( aout_OutputNew( p_aout, &p_aout->pp_inputs[0]->input ) == -1 )
    {
        /* Release all locks and report the error. */
        for ( i = 0; i < p_aout->i_nb_inputs; i++ )
        {
            vlc_mutex_unlock( &p_aout->pp_inputs[i]->lock );
        }
        aout_unlock_input_fifos( p_aout );
        aout_unlock_mixer( p_aout );
        return -1;
    }

    if ( aout_MixerNew( p_aout ) == -1 )
    {
        aout_OutputDelete( p_aout );
        for ( i = 0; i < p_aout->i_nb_inputs; i++ )
        {
            vlc_mutex_unlock( &p_aout->pp_inputs[i]->lock );
        }
        aout_unlock_input_fifos( p_aout );
        aout_unlock_mixer( p_aout );
        return -1;
    }

    /* Re-open all inputs. */
    for ( i = 0; i < p_aout->i_nb_inputs; i++ )
    {
        aout_input_t * p_input = p_aout->pp_inputs[i];
        b_error |= aout_InputNew( p_aout, p_input, &p_input->request_vout );
        p_input->b_changed = 1;
        aout_unlock_input( p_aout, p_input );
    }

    aout_unlock_input_fifos( p_aout );
    aout_unlock_mixer( p_aout );

    return b_error;
}

/*****************************************************************************
 * aout_FindAndRestart : find the audio output instance and restart
 *****************************************************************************
 * This is used for callbacks of the configuration variables, and we believe
 * that when those are changed, it is a significant change which implies
 * rebuilding the audio-device and audio-channels variables.
 *****************************************************************************/
int aout_FindAndRestart( vlc_object_t * p_this, const char *psz_name,
                         vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    aout_instance_t * p_aout = findAout( pl_Get(p_this) );

    (void)psz_name; (void)oldval; (void)newval; (void)p_data;
    if ( p_aout == NULL ) return VLC_SUCCESS;

    var_Destroy( p_aout, "audio-device" );
    var_Destroy( p_aout, "audio-channels" );

    aout_Restart( p_aout );
    vlc_object_release( p_aout );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * aout_ChannelsRestart : change the audio device or channels and restart
 *****************************************************************************/
int aout_ChannelsRestart( vlc_object_t * p_this, const char * psz_variable,
                          vlc_value_t oldval, vlc_value_t newval,
                          void *p_data )
{
    aout_instance_t * p_aout = (aout_instance_t *)p_this;
    (void)oldval; (void)newval; (void)p_data;

    if ( !strcmp( psz_variable, "audio-device" ) )
    {
        /* This is supposed to be a significant change and supposes
         * rebuilding the channel choices. */
        var_Destroy( p_aout, "audio-channels" );
    }
    aout_Restart( p_aout );
    return 0;
}

#undef aout_EnableFilter
/** Enable or disable an audio filter
 * \param p_this a vlc object
 * \param psz_name name of the filter
 * \param b_add are we adding or removing the filter ?
 */
void aout_EnableFilter( vlc_object_t *p_this, const char *psz_name,
                        bool b_add )
{
    aout_instance_t *p_aout = findAout( p_this );

    if( AoutChangeFilterString( p_this, p_aout, "audio-filter", psz_name, b_add ) )
    {
        if( p_aout )
            AoutInputsMarkToRestart( p_aout );
    }

    if( p_aout )
        vlc_object_release( p_aout );
}
