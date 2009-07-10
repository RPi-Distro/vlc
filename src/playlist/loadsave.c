/*****************************************************************************
 * loadsave.c : Playlist loading / saving functions
 *****************************************************************************
 * Copyright (C) 1999-2004 the VideoLAN team
 * $Id: 34378730761e03e6e9bf0046f938d27a3f89b2df $
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
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

#include <vlc_common.h>
#include <vlc_playlist.h>
#include <vlc_events.h>
#include "playlist_internal.h"
#include "config/configuration.h"
#include <vlc_charset.h>
#include <vlc_url.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

int playlist_Export( playlist_t * p_playlist, const char *psz_filename ,
                     playlist_item_t *p_export_root,const char *psz_type )
{
    module_t *p_module;
    playlist_export_t export;

    if( p_export_root == NULL ) return VLC_EGENERIC;

    msg_Dbg( p_playlist, "saving %s to file %s",
                    p_export_root->p_input->psz_name, psz_filename );

    /* Prepare the playlist_export_t structure */
    export.psz_filename = psz_filename ? strdup( psz_filename ) : NULL;
    export.p_file = utf8_fopen( psz_filename, "wt" );
    if( export.p_file == NULL )
    {
        msg_Err( p_playlist , "could not create playlist file %s (%m)",
                 psz_filename );
        free( export.psz_filename );
        return VLC_EGENERIC;
    }

    export.p_root = p_export_root;

    playlist_Lock( p_playlist );
    p_playlist->p_private = (void *)&export;

    /* And call the module ! All work is done now */
    p_module = module_need( p_playlist, "playlist export", psz_type, true);
    if( !p_module )
        msg_Warn( p_playlist, "exporting playlist failed" );
    else
        module_unneed( p_playlist , p_module );
    p_playlist->p_private = NULL;
    playlist_Unlock( p_playlist );

    /* Clean up */
    fclose( export.p_file );
    free( export.psz_filename );

    return p_module ? VLC_SUCCESS : VLC_ENOOBJ;
}

int playlist_Import( playlist_t *p_playlist, const char *psz_file )
{
    input_item_t *p_input;
    const char *const psz_option = "meta-file";
    char *psz_uri = make_URI( psz_file );

    if( psz_uri == NULL )
        return VLC_EGENERIC;

    p_input = input_item_NewExt( p_playlist, psz_uri, psz_file,
                                 1, &psz_option, VLC_INPUT_OPTION_TRUSTED, -1 );
    free( psz_uri );

    playlist_AddInput( p_playlist, p_input, PLAYLIST_APPEND, PLAYLIST_END,
                       true, false );
    return input_Read( p_playlist, p_input, true );
}

/*****************************************************************************
 * A subitem has been added to the Media Library (Event Callback)
 *****************************************************************************/
static void input_item_subitem_added( const vlc_event_t * p_event,
                                      void * user_data )
{
    playlist_t *p_playlist = user_data;
    input_item_t *p_item = p_event->u.input_item_subitem_added.p_new_child;

    /* playlist_AddInput() can fail, but we have no way to report that ..
     * Any way when it has failed, either the playlist is dying, either OOM */
    playlist_AddInput( p_playlist, p_item, PLAYLIST_APPEND, PLAYLIST_END,
            false, pl_Unlocked );
}

int playlist_MLLoad( playlist_t *p_playlist )
{
    char *psz_datadir;
    char *psz_uri = NULL;
    input_item_t *p_input;

    if( !config_GetInt( p_playlist, "media-library") )
        return VLC_SUCCESS;

    psz_datadir = config_GetUserDataDir();

    if( !psz_datadir ) /* XXX: This should never happen */
    {
        msg_Err( p_playlist, "no data directory, cannot load media library") ;
        return VLC_EGENERIC;
    }

    if( asprintf( &psz_uri, "%s" DIR_SEP "ml.xspf", psz_datadir ) != -1 )
    {   /* loosy check for media library file */
        struct stat st;
        int ret = utf8_stat( psz_uri , &st );
        free( psz_uri );
        if( ret )
        {
            free( psz_datadir );
            return VLC_EGENERIC;
        }
    }

    psz_uri = make_URI( psz_datadir );
    free( psz_datadir );
    psz_datadir = psz_uri;
    if( psz_datadir == NULL )
        return VLC_EGENERIC;

    /* Force XSPF demux (psz_datadir was a path, now it is a file URI) */
    if( asprintf( &psz_uri, "file/xspf-open%s/ml.xspf", psz_datadir+4 ) == -1 )
        psz_uri = NULL;
    free( psz_datadir );
    psz_datadir = NULL;
    if( psz_uri == NULL )
        return VLC_ENOMEM;

    const char *const options[1] = { "meta-file", };
    /* that option has to be cleaned in input_item_subitem_added() */
    /* vlc_gc_decref() in the same function */
    p_input = input_item_NewExt( p_playlist, psz_uri, _("Media Library"),
                                 1, options, VLC_INPUT_OPTION_TRUSTED, -1 );
    free( psz_uri );
    if( p_input == NULL )
        return VLC_EGENERIC;

    PL_LOCK;
    if( p_playlist->p_ml_onelevel->p_input )
        vlc_gc_decref( p_playlist->p_ml_onelevel->p_input );
    if( p_playlist->p_ml_category->p_input )
        vlc_gc_decref( p_playlist->p_ml_category->p_input );

    p_playlist->p_ml_onelevel->p_input =
    p_playlist->p_ml_category->p_input = p_input;
    /* We save the input at two different place, incref */
    vlc_gc_incref( p_input );
    vlc_gc_incref( p_input );

    vlc_event_attach( &p_input->event_manager, vlc_InputItemSubItemAdded,
                        input_item_subitem_added, p_playlist );

    pl_priv(p_playlist)->b_doing_ml = true;
    PL_UNLOCK;

    stats_TimerStart( p_playlist, "ML Load", STATS_TIMER_ML_LOAD );
    input_Read( p_playlist, p_input, true );
    stats_TimerStop( p_playlist,STATS_TIMER_ML_LOAD );

    PL_LOCK;
    pl_priv(p_playlist)->b_doing_ml = false;
    PL_UNLOCK;

    vlc_event_detach( &p_input->event_manager, vlc_InputItemSubItemAdded,
                        input_item_subitem_added, p_playlist );

    vlc_gc_decref( p_input );
    return VLC_SUCCESS;
}

int playlist_MLDump( playlist_t *p_playlist )
{
    char *psz_datadir;

    if( !config_GetInt( p_playlist, "media-library") )
        return VLC_SUCCESS;

    psz_datadir = config_GetUserDataDir();

    if( !psz_datadir ) /* XXX: This should never happen */
    {
        msg_Err( p_playlist, "no data directory, cannot save media library") ;
        return VLC_EGENERIC;
    }

    char psz_dirname[ strlen( psz_datadir ) + sizeof( DIR_SEP "ml.xspf")];
    strcpy( psz_dirname, psz_datadir );
    free( psz_datadir );
    if( config_CreateDir( (vlc_object_t *)p_playlist, psz_dirname ) )
    {
        return VLC_EGENERIC;
    }

    strcat( psz_dirname, DIR_SEP "ml.xspf" );

    stats_TimerStart( p_playlist, "ML Dump", STATS_TIMER_ML_DUMP );
    playlist_Export( p_playlist, psz_dirname, p_playlist->p_ml_category,
                     "export-xspf" );
    stats_TimerStop( p_playlist, STATS_TIMER_ML_DUMP );

    return VLC_SUCCESS;
}
