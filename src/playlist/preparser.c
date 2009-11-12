/*****************************************************************************
 * preparse.c: Preparser thread.
 *****************************************************************************
 * Copyright © 1999-2009 the VideoLAN team
 * $Id$
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
 *          Clément Stenac <zorglub@videolan.org>
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

#include "art.h"
#include "fetcher.h"
#include "preparser.h"
#include "../input/input_interface.h"


/*****************************************************************************
 * Structures/definitions
 *****************************************************************************/
struct playlist_preparser_t
{
    playlist_t          *p_playlist;
    playlist_fetcher_t  *p_fetcher;

    vlc_thread_t    thread;
    vlc_mutex_t     lock;
    vlc_cond_t      wait;
    input_item_t  **pp_waiting;
    int             i_waiting;

    int             i_art_policy;
};

static void *Thread( void * );

/*****************************************************************************
 * Public functions
 *****************************************************************************/
playlist_preparser_t *playlist_preparser_New( playlist_t *p_playlist, playlist_fetcher_t *p_fetcher )
{
    playlist_preparser_t *p_preparser = malloc( sizeof(*p_preparser) );
    if( !p_preparser )
        return NULL;

    p_preparser->p_playlist = p_playlist;
    p_preparser->p_fetcher = p_fetcher;
    vlc_mutex_init( &p_preparser->lock );
    vlc_cond_init( &p_preparser->wait );
    p_preparser->i_art_policy = var_GetInteger( p_playlist, "album-art" );
    p_preparser->i_waiting = 0;
    p_preparser->pp_waiting = NULL;

    if( vlc_clone( &p_preparser->thread, Thread, p_preparser,
                   VLC_THREAD_PRIORITY_LOW ) )
    {
        msg_Err( p_playlist, "cannot spawn preparse thread" );
        free( p_preparser );
        return NULL;
    }
    return p_preparser;
}

void playlist_preparser_Push( playlist_preparser_t *p_preparser, input_item_t *p_item )
{
    vlc_gc_incref( p_item );

    vlc_mutex_lock( &p_preparser->lock );
    INSERT_ELEM( p_preparser->pp_waiting, p_preparser->i_waiting,
                 p_preparser->i_waiting, p_item );
    vlc_cond_signal( &p_preparser->wait );
    vlc_mutex_unlock( &p_preparser->lock );
}

void playlist_preparser_Delete( playlist_preparser_t *p_preparser )
{
    /* Destroy the item preparser */
    vlc_cancel( p_preparser->thread );
    vlc_join( p_preparser->thread, NULL );

    while( p_preparser->i_waiting > 0 )
    {   /* Any left-over unparsed item? */
        vlc_gc_decref( p_preparser->pp_waiting[0] );
        REMOVE_ELEM( p_preparser->pp_waiting, p_preparser->i_waiting, 0 );
    }
    vlc_cond_destroy( &p_preparser->wait );
    vlc_mutex_destroy( &p_preparser->lock );
    free( p_preparser );
}

/*****************************************************************************
 * Privates functions
 *****************************************************************************/
/**
 * This function preparses an item when needed.
 */
static void Preparse( playlist_t *p_playlist, input_item_t *p_item )
{
    vlc_mutex_lock( &p_item->lock );
    int i_type = p_item->i_type;
    vlc_mutex_unlock( &p_item->lock );

    if( i_type != ITEM_TYPE_FILE )
        return;

    stats_TimerStart( p_playlist, "Preparse run", STATS_TIMER_PREPARSE );

    /* Do not preparse if it is already done (like by playing it) */
    if( !input_item_IsPreparsed( p_item ) )
    {
        input_Preparse( VLC_OBJECT(p_playlist), p_item );
        input_item_SetPreparsed( p_item, true );

        var_SetInteger( p_playlist, "item-change", p_item->i_id );
    }

    stats_TimerStop( p_playlist, STATS_TIMER_PREPARSE );
}

/**
 * This function ask the fetcher object to fetch the art when needed
 */
static void Art( playlist_preparser_t *p_preparser, input_item_t *p_item )
{
    playlist_t *p_playlist = p_preparser->p_playlist;
    playlist_fetcher_t *p_fetcher = p_preparser->p_fetcher;

    bool b_fetch = false;
    /* If we haven't retrieved enough meta, add to secondary queue
     * which will run the "meta fetchers".
     * This only checks for meta, not for art
     * \todo don't do this for things we won't get meta for, like vids
     */

    vlc_mutex_lock( &p_item->lock );
    if( p_item->p_meta )
    {
        const char *psz_arturl = vlc_meta_Get( p_item->p_meta, vlc_meta_ArtworkURL );
        const char *psz_name = vlc_meta_Get( p_item->p_meta, vlc_meta_Title );

        if( p_preparser->i_art_policy == ALBUM_ART_ALL &&
            ( !psz_arturl || strncmp( psz_arturl, "file://", 7 ) ) )
        {
            msg_Dbg( p_playlist, "meta ok for %s, need to fetch art", psz_name );
            b_fetch = true;
        }
        else
        {
            msg_Dbg( p_playlist, "no fetch required for %s (art currently %s)",
                     psz_name, psz_arturl );
        }
    }
    vlc_mutex_unlock( &p_item->lock );

    if( b_fetch && p_fetcher )
        playlist_fetcher_Push( p_fetcher, p_item );
}

/**
 * This function does the preparsing and issues the art fetching requests
 */
static void *Thread( void *data )
{
    playlist_preparser_t *p_preparser = data;
    playlist_t *p_playlist = p_preparser->p_playlist;

    for( ;; )
    {
        input_item_t *p_current;

        /* Be sure to be cancellable before our queue is empty */
        vlc_testcancel();

        /* */
        vlc_mutex_lock( &p_preparser->lock );
        mutex_cleanup_push( &p_preparser->lock );

        while( p_preparser->i_waiting == 0 )
            vlc_cond_wait( &p_preparser->wait, &p_preparser->lock );

        p_current = p_preparser->pp_waiting[0];
        REMOVE_ELEM( p_preparser->pp_waiting, p_preparser->i_waiting, 0 );
        vlc_cleanup_run( );

        if( !p_current )
            continue;

        int canc = vlc_savecancel();

        Preparse( p_playlist, p_current );

        Art( p_preparser, p_current );

        vlc_restorecancel( canc );

        /* */
        int i_activity = var_GetInteger( p_playlist, "activity" );
        if( i_activity < 0 )
            i_activity = 0;

        /* Sleep at least 1ms */
        msleep( (i_activity+1) * 1000 );
    }
    return NULL;
}

