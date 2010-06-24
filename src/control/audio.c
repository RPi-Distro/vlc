/*****************************************************************************
 * libvlc_audio.c: New libvlc audio control API
 *****************************************************************************
 * Copyright (C) 2006 the VideoLAN team
 * $Id: 2c5ed8ae74fae3ae08acd9b3da912039f31eb45f $
 *
 * Authors: Filippo Carone <filippo@carone.org>
 *          Jean-Paul Saman <jpsaman _at_ m2x _dot_ nl>
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
# include "config.h"
#endif

#include <assert.h>

#include <vlc/libvlc.h>
#include <vlc/libvlc_media.h>
#include <vlc/libvlc_media_player.h>

#include <vlc_common.h>
#include <vlc_input.h>
#include <vlc_aout.h>

#include "libvlc_internal.h"
#include "media_player_internal.h"

/*
 * Remember to release the returned aout_instance_t since it is locked at
 * the end of this function.
 */
static aout_instance_t *GetAOut( libvlc_media_player_t *mp )
{
    assert( mp != NULL );

    input_thread_t *p_input = libvlc_get_input_thread( mp );
    if( p_input == NULL )
        return NULL;

    aout_instance_t * p_aout = input_GetAout( p_input );
    vlc_object_release( p_input );
    if( p_aout == NULL )
        libvlc_printerr( "No active audio output" );
    return p_aout;
}

/*****************************************
 * Get the list of available audio outputs
 *****************************************/
libvlc_audio_output_t *
        libvlc_audio_output_list_get( libvlc_instance_t *p_instance )
{
    VLC_UNUSED( p_instance );
    libvlc_audio_output_t *p_list = NULL,
                          *p_actual = NULL,
                          *p_previous = NULL;
    module_t **module_list = module_list_get( NULL );

    for (size_t i = 0; module_list[i]; i++)
    {
        module_t *p_module = module_list[i];

        if( module_provides( p_module, "audio output" ) )
        {
            if( p_actual == NULL)
            {
                p_actual = ( libvlc_audio_output_t * )
                    malloc( sizeof( libvlc_audio_output_t ) );
                if( p_actual == NULL )
                {
                    libvlc_printerr( "Not enough memory" );
                    libvlc_audio_output_list_release( p_list );
                    module_list_free( module_list );
                    return NULL;
                }
                if( p_list == NULL )
                {
                    p_list = p_actual;
                    p_previous = p_actual;
                }
            }
            p_actual->psz_name = strdup( module_get_object( p_module ) );
            p_actual->psz_description = strdup( module_get_name( p_module, true )  );
            p_actual->p_next = NULL;
            if( p_previous != p_actual ) /* not first item */
                p_previous->p_next = p_actual;
            p_previous = p_actual;
            p_actual = p_actual->p_next;
        }
    }

    module_list_free( module_list );

    return p_list;
}

/********************************************
 * Free the list of available audio outputs
 ***********************************************/
void libvlc_audio_output_list_release( libvlc_audio_output_t *p_list )
{
    libvlc_audio_output_t *p_actual, *p_before;
    p_actual = p_list;

    while ( p_actual )
    {
        free( p_actual->psz_name );
        free( p_actual->psz_description );
        p_before = p_actual;
        p_actual = p_before->p_next;
        free( p_before );
    }
}


/***********************
 * Set the audio output.
 ***********************/
int libvlc_audio_output_set( libvlc_media_player_t *mp, const char *psz_name )
{
    if( !module_exists( psz_name ) )
        return -1;
    var_SetString( mp, "aout", psz_name );
    return 0;
}

/****************************
 * Get count of devices.
 *****************************/
int libvlc_audio_output_device_count( libvlc_instance_t *p_instance,
                                      const char *psz_audio_output )
{
    char *psz_config_name;
    if( !psz_audio_output )
        return 0;
    if( asprintf( &psz_config_name, "%s-audio-device", psz_audio_output ) == -1 )
        return 0;

    module_config_t *p_module_config = config_FindConfig(
        VLC_OBJECT( p_instance->p_libvlc_int ), psz_config_name );

    if( p_module_config && p_module_config->pf_update_list )
    {
        vlc_value_t val;
        val.psz_string = strdup( p_module_config->value.psz );

        p_module_config->pf_update_list(
            VLC_OBJECT( p_instance->p_libvlc_int ), psz_config_name, val, val, NULL );
        free( val.psz_string );
        free( psz_config_name );

        return p_module_config->i_list;
    }

    free( psz_config_name );
    return 0;
}

/********************************
 * Get long name of device
 *********************************/
char * libvlc_audio_output_device_longname( libvlc_instance_t *p_instance,
                                            const char *psz_audio_output,
                                            int i_device )
{
    char *psz_config_name;
    if( !psz_audio_output )
        return NULL;
    if( asprintf( &psz_config_name, "%s-audio-device", psz_audio_output ) == -1 )
        return NULL;

    module_config_t *p_module_config = config_FindConfig(
        VLC_OBJECT( p_instance->p_libvlc_int ), psz_config_name );

    if( p_module_config )
    {
        // refresh if there arent devices
        if( p_module_config->i_list < 2 && p_module_config->pf_update_list )
        {
            vlc_value_t val;
            val.psz_string = strdup( p_module_config->value.psz );

            p_module_config->pf_update_list(
                VLC_OBJECT( p_instance->p_libvlc_int ), psz_config_name, val, val, NULL );
            free( val.psz_string );
        }
        free( psz_config_name );

        if( i_device >= 0 && i_device < p_module_config->i_list )
        {
            if( p_module_config->ppsz_list_text[i_device] )
                return strdup( p_module_config->ppsz_list_text[i_device] );
            else
                return strdup( p_module_config->ppsz_list[i_device] );
        }
    }

    free( psz_config_name );
    return NULL;
}

/********************************
 * Get id name of device
 *********************************/
char * libvlc_audio_output_device_id( libvlc_instance_t *p_instance,
                                      const char *psz_audio_output,
                                      int i_device )
{
    char *psz_config_name;
    if( !psz_audio_output )
        return NULL;
    if( asprintf( &psz_config_name, "%s-audio-device", psz_audio_output ) == -1)
        return NULL;

    module_config_t *p_module_config = config_FindConfig(
        VLC_OBJECT( p_instance->p_libvlc_int ), psz_config_name );

    if( p_module_config )
    {
        // refresh if there arent devices
        if( p_module_config->i_list < 2 && p_module_config->pf_update_list )
        {
            vlc_value_t val;
            val.psz_string = strdup( p_module_config->value.psz );

            p_module_config->pf_update_list(
                VLC_OBJECT( p_instance->p_libvlc_int ), psz_config_name, val, val, NULL );
            free( val.psz_string );
        }
        free( psz_config_name );

        if( i_device >= 0 && i_device < p_module_config->i_list )
            return strdup( p_module_config->ppsz_list[i_device] );

    }

    free( psz_config_name );
    return NULL;
}

/*****************************
 * Set device for using
 *****************************/
void libvlc_audio_output_device_set( libvlc_media_player_t *mp,
                                     const char *psz_audio_output,
                                     const char *psz_device_id )
{
    char *psz_config_name;
    if( !psz_audio_output || !psz_device_id )
        return;
    if( asprintf( &psz_config_name, "%s-audio-device", psz_audio_output ) == -1 )
        return;
    if( !var_Type( mp, psz_audio_output ) )
        /* Don't recreate the same variable over and over and over... */
        var_Create( mp, psz_audio_output, VLC_VAR_STRING );
    var_SetString( mp, psz_config_name, psz_device_id );
    free( psz_config_name );
}

/*****************************************************************************
 * libvlc_audio_output_get_device_type : Get the current audio device type
 *****************************************************************************/
int libvlc_audio_output_get_device_type( libvlc_media_player_t *mp )
{
    aout_instance_t *p_aout = GetAOut( mp );
    if( p_aout )
    {
        int i_device_type = var_GetInteger( p_aout, "audio-device" );
        vlc_object_release( p_aout );
        return i_device_type;
    }
    return libvlc_AudioOutputDevice_Error;
}

/*****************************************************************************
 * libvlc_audio_output_set_device_type : Set the audio device type
 *****************************************************************************/
void libvlc_audio_output_set_device_type( libvlc_media_player_t *mp,
                                          int device_type )
{
    aout_instance_t *p_aout = GetAOut( mp );
    if( !p_aout )
        return;
    if( var_SetInteger( p_aout, "audio-device", device_type ) < 0 )
        libvlc_printerr( "Error setting audio device" );
    vlc_object_release( p_aout );
}

/*****************************************************************************
 * libvlc_audio_get_mute : Get the volume state, true if muted
 *****************************************************************************/
void libvlc_audio_toggle_mute( libvlc_media_player_t *mp )
{
    aout_ToggleMute( mp, NULL );
}

int libvlc_audio_get_mute( libvlc_media_player_t *mp )
{
    return (libvlc_audio_get_volume(mp) == 0);
}

void libvlc_audio_set_mute( libvlc_media_player_t *mp, int mute )
{
    aout_SetMute( VLC_OBJECT(mp), NULL, !!mute );
}

/*****************************************************************************
 * libvlc_audio_get_volume : Get the current volume (range 0-200 %)
 *****************************************************************************/
int libvlc_audio_get_volume( libvlc_media_player_t *mp )
{
    audio_volume_t i_volume;

    aout_VolumeGet( mp, &i_volume );

    return (i_volume*200+AOUT_VOLUME_MAX/2)/AOUT_VOLUME_MAX;
}


/*****************************************************************************
 * libvlc_audio_set_volume : Set the current volume
 *****************************************************************************/
int libvlc_audio_set_volume( libvlc_media_player_t *mp, int i_volume )
{
    if( i_volume < 0 || i_volume > 200 )
    {
        libvlc_printerr( "Volume out of range" );
        return -1;
    }

    i_volume = (i_volume * AOUT_VOLUME_MAX + 100) / 200;
    aout_VolumeSet( mp, i_volume );
    return 0;
}

/*****************************************************************************
 * libvlc_audio_get_track_count : Get the number of available audio tracks
 *****************************************************************************/
int libvlc_audio_get_track_count( libvlc_media_player_t *p_mi )
{
    input_thread_t *p_input_thread = libvlc_get_input_thread( p_mi );
    int i_track_count;

    if( !p_input_thread )
        return -1;

    i_track_count = var_CountChoices( p_input_thread, "audio-es" );

    vlc_object_release( p_input_thread );
    return i_track_count;
}

/*****************************************************************************
 * libvlc_audio_get_track_description : Get the description of available audio tracks
 *****************************************************************************/
libvlc_track_description_t *
        libvlc_audio_get_track_description( libvlc_media_player_t *p_mi )
{
    return libvlc_get_track_description( p_mi, "audio-es" );
}

/*****************************************************************************
 * libvlc_audio_get_track : Get the current audio track
 *****************************************************************************/
int libvlc_audio_get_track( libvlc_media_player_t *p_mi )
{
    input_thread_t *p_input_thread = libvlc_get_input_thread( p_mi );
    vlc_value_t val_list;
    vlc_value_t val;
    int i_track = -1;
    int i;

    if( !p_input_thread )
        return -1;

    if( var_Get( p_input_thread, "audio-es", &val ) < 0 )
    {
        vlc_object_release( p_input_thread );
        libvlc_printerr( "Audio track information not found" );
        return -1;
    }

    var_Change( p_input_thread, "audio-es", VLC_VAR_GETCHOICES, &val_list, NULL );
    for( i = 0; i < val_list.p_list->i_count; i++ )
    {
        if( val_list.p_list->p_values[i].i_int == val.i_int )
        {
            i_track = i;
            break;
        }
    }
    var_FreeList( &val_list, NULL );
    vlc_object_release( p_input_thread );
    return i_track;
}

/*****************************************************************************
 * libvlc_audio_set_track : Set the current audio track
 *****************************************************************************/
int libvlc_audio_set_track( libvlc_media_player_t *p_mi, int i_track )
{
    input_thread_t *p_input_thread = libvlc_get_input_thread( p_mi );
    vlc_value_t val_list;
    vlc_value_t newval;
    int i_ret;

    if( !p_input_thread )
        return -1;

    var_Change( p_input_thread, "audio-es", VLC_VAR_GETCHOICES, &val_list, NULL );
    if( (i_track < 0) || (i_track > val_list.p_list->i_count) )
    {
        libvlc_printerr( "Audio track out of range" );
        i_ret = -1;
        goto end;
    }

    newval = val_list.p_list->p_values[i_track];
    i_ret = var_Set( p_input_thread, "audio-es", newval );
    if( i_ret < 0 )
    {
        libvlc_printerr( "Audio track out of range" ); /* Race... */
        i_ret = -1;
        goto end;
    }
    i_ret = 0;

end:
    var_FreeList( &val_list, NULL );
    vlc_object_release( p_input_thread );
    return i_ret;
}

/*****************************************************************************
 * libvlc_audio_get_channel : Get the current audio channel
 *****************************************************************************/
int libvlc_audio_get_channel( libvlc_media_player_t *mp )
{
    aout_instance_t *p_aout = GetAOut( mp );
    if( !p_aout )
        return 0;

    int val = var_GetInteger( p_aout, "audio-channels" );
    vlc_object_release( p_aout );
    return val;
}

/*****************************************************************************
 * libvlc_audio_set_channel : Set the current audio channel
 *****************************************************************************/
int libvlc_audio_set_channel( libvlc_media_player_t *mp, int channel )
{
    aout_instance_t *p_aout = GetAOut( mp );
    int ret = 0;

    if( !p_aout )
        return -1;

    if( var_SetInteger( p_aout, "audio-channels", channel ) < 0 )
    {
        libvlc_printerr( "Audio channel out of range" );
        ret = -1;
    }
    vlc_object_release( p_aout );
    return ret;
}
