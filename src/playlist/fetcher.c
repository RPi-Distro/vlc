/*****************************************************************************
 * fetcher.c: Art fetcher thread.
 *****************************************************************************
 * Copyright © 1999-2009 the VideoLAN team
 * $Id: c9384b4d2418cb1134f04042bfeca221b2d5879a $
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
#include <vlc_stream.h>
#include <limits.h>

#include "art.h"
#include "fetcher.h"
#include "playlist_internal.h"


/*****************************************************************************
 * Structures/definitions
 *****************************************************************************/
struct playlist_fetcher_t
{
    VLC_COMMON_MEMBERS;

    playlist_t      *p_playlist;

    vlc_thread_t    thread;
    vlc_mutex_t     lock;
    vlc_cond_t      wait;
    int             i_art_policy;
    int             i_waiting;
    input_item_t    **pp_waiting;

    DECL_ARRAY(playlist_album_t) albums;
};

static void *Thread( void * );


/*****************************************************************************
 * Public functions
 *****************************************************************************/
playlist_fetcher_t *playlist_fetcher_New( playlist_t *p_playlist )
{
    playlist_fetcher_t *p_fetcher =
        vlc_custom_create( p_playlist, sizeof(*p_fetcher),
                           VLC_OBJECT_GENERIC, "playlist fetcher" );

    if( !p_fetcher )
        return NULL;

    vlc_object_attach( p_fetcher, p_playlist );
    p_fetcher->p_playlist = p_playlist;
    vlc_mutex_init( &p_fetcher->lock );
    vlc_cond_init( &p_fetcher->wait );
    p_fetcher->i_waiting = 0;
    p_fetcher->pp_waiting = NULL;
    p_fetcher->i_art_policy = var_GetInteger( p_playlist, "album-art" );
    ARRAY_INIT( p_fetcher->albums );

    if( vlc_clone( &p_fetcher->thread, Thread, p_fetcher,
                   VLC_THREAD_PRIORITY_LOW ) )
    {
        msg_Err( p_fetcher, "cannot spawn secondary preparse thread" );
        vlc_object_release( p_fetcher );
        return NULL;
    }

    return p_fetcher;
}

void playlist_fetcher_Push( playlist_fetcher_t *p_fetcher, input_item_t *p_item )
{
    vlc_gc_incref( p_item );

    vlc_mutex_lock( &p_fetcher->lock );
    INSERT_ELEM( p_fetcher->pp_waiting, p_fetcher->i_waiting,
                 p_fetcher->i_waiting, p_item );
    vlc_cond_signal( &p_fetcher->wait );
    vlc_mutex_unlock( &p_fetcher->lock );
}

void playlist_fetcher_Delete( playlist_fetcher_t *p_fetcher )
{
    /* */
    vlc_object_kill( p_fetcher );

    /* Destroy the item meta-infos fetcher */
    vlc_cancel( p_fetcher->thread );
    vlc_join( p_fetcher->thread, NULL );

    while( p_fetcher->i_waiting > 0 )
    {   /* Any left-over unparsed item? */
        vlc_gc_decref( p_fetcher->pp_waiting[0] );
        REMOVE_ELEM( p_fetcher->pp_waiting, p_fetcher->i_waiting, 0 );
    }
    vlc_cond_destroy( &p_fetcher->wait );
    vlc_mutex_destroy( &p_fetcher->lock );
    vlc_object_release( p_fetcher );
}


/*****************************************************************************
 * Privates functions
 *****************************************************************************/
/**
 * This function locates the art associated to an input item.
 * Return codes:
 *   0 : Art is in cache or is a local file
 *   1 : Art found, need to download
 *  -X : Error/not found
 */
static int FindArt( playlist_fetcher_t *p_fetcher, input_item_t *p_item )
{
    int i_ret;
    module_t *p_module;
    char *psz_title, *psz_artist, *psz_album;

    psz_artist = input_item_GetArtist( p_item );
    psz_album = input_item_GetAlbum( p_item );
    psz_title = input_item_GetTitle( p_item );
    if( !psz_title )
        psz_title = input_item_GetName( p_item );

    if( !psz_title && !psz_artist && !psz_album )
        return VLC_EGENERIC;

    free( psz_title );

    /* If we already checked this album in this session, skip */
    if( psz_artist && psz_album )
    {
        FOREACH_ARRAY( playlist_album_t album, p_fetcher->albums )
            if( !strcmp( album.psz_artist, psz_artist ) &&
                !strcmp( album.psz_album, psz_album ) )
            {
                msg_Dbg( p_fetcher, " %s - %s has already been searched",
                         psz_artist, psz_album );
                /* TODO-fenrir if we cache art filename too, we can go faster */
                free( psz_artist );
                free( psz_album );
                if( album.b_found )
                {
                    if( !strncmp( album.psz_arturl, "file://", 7 ) )
                        input_item_SetArtURL( p_item, album.psz_arturl );
                    else /* Actually get URL from cache */
                        playlist_FindArtInCache( p_item );
                    return 0;
                }
                else
                {
                    return VLC_EGENERIC;
                }
            }
        FOREACH_END();
    }
    free( psz_artist );
    free( psz_album );

    playlist_FindArtInCache( p_item );

    char *psz_arturl = input_item_GetArtURL( p_item );
    if( psz_arturl )
    {
        /* We already have an URL */
        if( !strncmp( psz_arturl, "file://", strlen( "file://" ) ) )
        {
            free( psz_arturl );
            return 0; /* Art is in cache, no need to go further */
        }

        free( psz_arturl );

        /* Art need to be put in cache */
        return 1;
    }

    /* */
    psz_album = input_item_GetAlbum( p_item );
    psz_artist = input_item_GetArtist( p_item );
    if( psz_album && psz_artist )
    {
        msg_Dbg( p_fetcher, "searching art for %s - %s",
             psz_artist, psz_album );
    }
    else
    {
        psz_title = input_item_GetTitle( p_item );
        if( !psz_title )
            psz_title = input_item_GetName( p_item );

        msg_Dbg( p_fetcher, "searching art for %s", psz_title );
        free( psz_title );
    }

    /* Fetch the art url */
    p_fetcher->p_private = p_item;

    p_module = module_need( p_fetcher, "art finder", NULL, false );

    if( p_module )
    {
        module_unneed( p_fetcher, p_module );
        i_ret = 1;
    }
    else
    {
        msg_Dbg( p_fetcher, "unable to find art" );
        i_ret = VLC_EGENERIC;
    }

    /* Record this album */
    if( psz_artist && psz_album )
    {
        playlist_album_t a;
        a.psz_artist = psz_artist;
        a.psz_album = psz_album;
        a.psz_arturl = input_item_GetArtURL( p_item );
        a.b_found = (i_ret == VLC_EGENERIC ? false : true );
        ARRAY_APPEND( p_fetcher->albums, a );
    }
    else
    {
        free( psz_artist );
        free( psz_album );
    }

    return i_ret;
}

/**
 * Download the art using the URL or an art downloaded
 * This function should be called only if data is not already in cache
 */
static int DownloadArt( playlist_fetcher_t *p_fetcher, input_item_t *p_item )
{
    char *psz_arturl = input_item_GetArtURL( p_item );
    assert( *psz_arturl );

    if( !strncmp( psz_arturl , "file://", 7 ) )
    {
        msg_Dbg( p_fetcher, "Album art is local file, no need to cache" );
        free( psz_arturl );
        return VLC_SUCCESS;
    }

    if( !strncmp( psz_arturl , "APIC", 4 ) )
    {
        msg_Warn( p_fetcher, "APIC fetch not supported yet" );
        goto error;
    }

    stream_t *p_stream = stream_UrlNew( p_fetcher, psz_arturl );
    if( !p_stream )
        goto error;

    uint8_t *p_data = NULL;
    int i_data = 0;
    for( ;; )
    {
        int i_read = 65536;

        if( i_data >= INT_MAX - i_read )
            break;

        p_data = realloc( p_data, i_data + i_read );
        if( !p_data )
            break;

        i_read = stream_Read( p_stream, &p_data[i_data], i_read );
        if( i_read <= 0 )
            break;

        i_data += i_read;
    }
    stream_Delete( p_stream );

    if( p_data && i_data > 0 )
    {
        char *psz_type = strrchr( psz_arturl, '.' );
        if( psz_type && strlen( psz_type ) > 5 )
            psz_type = NULL; /* remove extension if it's > to 4 characters */

        playlist_SaveArt( p_fetcher->p_playlist, p_item, p_data, i_data, psz_type );
    }

    free( p_data );

    free( psz_arturl );
    return VLC_SUCCESS;

error:
    free( psz_arturl );
    return VLC_EGENERIC;
}


static int InputEvent( vlc_object_t *p_this, char const *psz_cmd,
                       vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    VLC_UNUSED(p_this); VLC_UNUSED(psz_cmd); VLC_UNUSED(oldval);
    playlist_fetcher_t *p_fetcher = p_data;

    if( newval.i_int == INPUT_EVENT_ITEM_META ||
        newval.i_int == INPUT_EVENT_DEAD )
        vlc_cond_signal( &p_fetcher->wait );

    return VLC_SUCCESS;
}


/* Check if it is not yet preparsed and if so wait for it
 * (at most 0.5s)
 * (This can happen if we fetch art on play)
 * FIXME this doesn't work if we need to fetch meta before art...
 */
static void WaitPreparsed( playlist_fetcher_t *p_fetcher, input_item_t *p_item )
{
    if( input_item_IsPreparsed( p_item ) )
        return;

    input_thread_t *p_input = playlist_CurrentInput( p_fetcher->p_playlist );
    if( !p_input )
        return;

    if( input_GetItem( p_input ) != p_item )
        goto exit;

    var_AddCallback( p_input, "intf-event", InputEvent, p_fetcher );

    const mtime_t i_deadline = mdate() + 500*1000;

    while( !p_input->b_eof && !p_input->b_error && !input_item_IsPreparsed( p_item ) )
    {
        /* A bit weird, but input_item_IsPreparsed does held the protected value */
        vlc_mutex_lock( &p_fetcher->lock );
        vlc_cond_timedwait( &p_fetcher->wait, &p_fetcher->lock, i_deadline );
        vlc_mutex_unlock( &p_fetcher->lock );

        if( i_deadline <= mdate() )
            break;
    }

    var_DelCallback( p_input, "intf-event", InputEvent, p_fetcher );

exit:
    vlc_object_release( p_input );
}

static void *Thread( void *p_data )
{
    playlist_fetcher_t *p_fetcher = p_data;
    playlist_t *p_playlist = p_fetcher->p_playlist;

    for( ;; )
    {
        input_item_t *p_item;

        /* Be sure to be cancellable before our queue is empty */
        vlc_testcancel();

        /* */
        vlc_mutex_lock( &p_fetcher->lock );
        mutex_cleanup_push( &p_fetcher->lock );

        while( p_fetcher->i_waiting == 0 )
            vlc_cond_wait( &p_fetcher->wait, &p_fetcher->lock );

        p_item = p_fetcher->pp_waiting[0];
        REMOVE_ELEM( p_fetcher->pp_waiting, p_fetcher->i_waiting, 0 );
        vlc_cleanup_run( );

        if( !p_item )
            continue;

        /* */
        int canc = vlc_savecancel();

        /* Wait that the input item is preparsed if it is being played */
        WaitPreparsed( p_fetcher, p_item );

        /* */
        if( !vlc_object_alive( p_fetcher ) )
            goto end;

        /* Find art, and download it if needed */
        int i_ret = FindArt( p_fetcher, p_item );

        /* */
        if( !vlc_object_alive( p_fetcher ) )
            goto end;

        if( i_ret == 1 )
            i_ret = DownloadArt( p_fetcher, p_item );

        /* */
        char *psz_name = input_item_GetName( p_item );
        if( !i_ret ) /* Art is now in cache */
        {
            PL_DEBUG( "found art for %s in cache", psz_name );
            input_item_SetArtFetched( p_item, true );
            var_SetInteger( p_playlist, "item-change", p_item->i_id );
        }
        else
        {
            PL_DEBUG( "art not found for %s", psz_name );
            input_item_SetArtNotFound( p_item, true );
        }
        free( psz_name );

    end:
        vlc_gc_decref( p_item );

        vlc_restorecancel( canc );

        int i_activity = var_GetInteger( p_playlist, "activity" );
        if( i_activity < 0 ) i_activity = 0;
        /* Sleep at least 1ms */
        msleep( (i_activity+1) * 1000 );
    }
    return NULL;
}


