/*****************************************************************************
 * fetcher.c: Art fetcher thread.
 *****************************************************************************
 * Copyright © 1999-2009 the VideoLAN team
 * $Id: 2075c0dfdbde6a626486c208b7354aa43a24a551 $
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

#include <assert.h>

#include <vlc_common.h>
#include <vlc_playlist.h>
#include <vlc_stream.h>
#include <limits.h>
#include <vlc_art_finder.h>
#include <vlc_memory.h>
#include <vlc_demux.h>

#include "art.h"
#include "fetcher.h"
#include "playlist_internal.h"

/*****************************************************************************
 * Structures/definitions
 *****************************************************************************/
struct playlist_fetcher_t
{
    playlist_t      *p_playlist;

    vlc_mutex_t     lock;
    vlc_cond_t      wait;
    bool            b_live;
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
    playlist_fetcher_t *p_fetcher = malloc( sizeof(*p_fetcher) );
    if( !p_fetcher )
        return NULL;

    p_fetcher->p_playlist = p_playlist;
    vlc_mutex_init( &p_fetcher->lock );
    vlc_cond_init( &p_fetcher->wait );
    p_fetcher->b_live = false;
    p_fetcher->i_waiting = 0;
    p_fetcher->pp_waiting = NULL;
    p_fetcher->i_art_policy = var_GetInteger( p_playlist, "album-art" );
    ARRAY_INIT( p_fetcher->albums );

    return p_fetcher;
}

void playlist_fetcher_Push( playlist_fetcher_t *p_fetcher,
                            input_item_t *p_item )
{
    vlc_gc_incref( p_item );

    vlc_mutex_lock( &p_fetcher->lock );
    INSERT_ELEM( p_fetcher->pp_waiting, p_fetcher->i_waiting,
                 p_fetcher->i_waiting, p_item );
    if( !p_fetcher->b_live )
    {
        vlc_thread_t th;

        if( vlc_clone( &th, Thread, p_fetcher, VLC_THREAD_PRIORITY_LOW ) )
            msg_Err( p_fetcher->p_playlist,
                     "cannot spawn secondary preparse thread" );
        else
        {
            vlc_detach( th );
            p_fetcher->b_live = true;
        }
    }
    vlc_mutex_unlock( &p_fetcher->lock );
}

void playlist_fetcher_Delete( playlist_fetcher_t *p_fetcher )
{
    vlc_mutex_lock( &p_fetcher->lock );
    /* Remove any left-over item, the fetcher will exit */
    while( p_fetcher->i_waiting > 0 )
    {
        vlc_gc_decref( p_fetcher->pp_waiting[0] );
        REMOVE_ELEM( p_fetcher->pp_waiting, p_fetcher->i_waiting, 0 );
    }

    while( p_fetcher->b_live )
        vlc_cond_wait( &p_fetcher->wait, &p_fetcher->lock );
    vlc_mutex_unlock( &p_fetcher->lock );

    vlc_cond_destroy( &p_fetcher->wait );
    vlc_mutex_destroy( &p_fetcher->lock );
    free( p_fetcher );
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

    char *psz_artist = input_item_GetArtist( p_item );
    char *psz_album = input_item_GetAlbum( p_item );
    char *psz_title = input_item_GetTitle( p_item );
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
                msg_Dbg( p_fetcher->p_playlist,
                         " %s - %s has already been searched",
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
        msg_Dbg( p_fetcher->p_playlist, "searching art for %s - %s",
             psz_artist, psz_album );
    }
    else
    {
        psz_title = input_item_GetTitle( p_item );
        if( !psz_title )
            psz_title = input_item_GetName( p_item );

        msg_Dbg( p_fetcher->p_playlist, "searching art for %s", psz_title );
        free( psz_title );
    }

    /* Fetch the art url */
    i_ret = VLC_EGENERIC;

    vlc_object_t *p_parent = VLC_OBJECT(p_fetcher->p_playlist);
    art_finder_t *p_finder =
        vlc_custom_create( p_parent, sizeof( *p_finder ), VLC_OBJECT_GENERIC,
                           "art finder" );
    if( p_finder != NULL)
    {
        module_t *p_module;

        vlc_object_attach( p_finder, p_parent );
        p_finder->p_item = p_item;

        p_module = module_need( p_finder, "art finder", NULL, false );
        if( p_module )
        {
            module_unneed( p_finder, p_module );
            /* Try immediately if found in cache by download URL */
            if( !playlist_FindArtInCache( p_item ) )
                i_ret = 0;
            else
                i_ret = 1;
        }
        vlc_object_release( p_finder );
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
        msg_Dbg( p_fetcher->p_playlist,
                 "Album art is local file, no need to cache" );
        free( psz_arturl );
        return VLC_SUCCESS;
    }

    if( !strncmp( psz_arturl , "APIC", 4 ) )
    {
        msg_Warn( p_fetcher->p_playlist, "APIC fetch not supported yet" );
        goto error;
    }

    stream_t *p_stream = stream_UrlNew( p_fetcher->p_playlist, psz_arturl );
    if( !p_stream )
        goto error;

    uint8_t *p_data = NULL;
    int i_data = 0;
    for( ;; )
    {
        int i_read = 65536;

        if( i_data >= INT_MAX - i_read )
            break;

        p_data = realloc_or_free( p_data, i_data + i_read );
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

/**
 * FetchMeta, run the "meta fetcher". They are going to do network
 * connections, and gather information upon the playing media.
 * (even artwork).
 */
static void FetchMeta( playlist_fetcher_t *p_fetcher, input_item_t *p_item )
{
    demux_meta_t *p_demux_meta = vlc_custom_create(p_fetcher->p_playlist,
                                       sizeof(*p_demux_meta),
                                       VLC_OBJECT_GENERIC, "demux meta" );
    if( !p_demux_meta )
        return;

    vlc_object_attach( p_demux_meta, p_fetcher->p_playlist );
    p_demux_meta->p_demux = NULL;
    p_demux_meta->p_item = p_item;

    module_t *p_meta_fetcher = module_need( p_demux_meta, "meta fetcher", NULL, false );
    if( p_meta_fetcher )
        module_unneed( p_demux_meta, p_meta_fetcher );
    vlc_object_release( p_demux_meta );
}

static int InputEvent( vlc_object_t *p_this, char const *psz_cmd,
                       vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    VLC_UNUSED(p_this); VLC_UNUSED(psz_cmd); VLC_UNUSED(oldval);
    vlc_cond_t *p_cond = p_data;

    if( newval.i_int == INPUT_EVENT_ITEM_META ||
        newval.i_int == INPUT_EVENT_DEAD )
        vlc_cond_signal( p_cond );

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

    vlc_cond_t cond;
    vlc_cond_init( &cond );
    var_AddCallback( p_input, "intf-event", InputEvent, &cond );

    const mtime_t i_deadline = mdate() + 500*1000;
    bool b_timeout = false;

    while( !p_input->b_eof && !p_input->b_error
        && !input_item_IsPreparsed( p_item ) && !b_timeout )
    {
        /* A bit weird, but input_item_IsPreparsed holds the protected value */
        /* FIXME: locking looks wrong here */
        vlc_mutex_lock( &p_fetcher->lock );
        if( vlc_cond_timedwait( &cond, &p_fetcher->lock, i_deadline ) )
            b_timeout = true;
        vlc_mutex_unlock( &p_fetcher->lock );
    }

    var_DelCallback( p_input, "intf-event", InputEvent, &cond );
    vlc_cond_destroy( &cond );

exit:
    vlc_object_release( p_input );
}

static void *Thread( void *p_data )
{
    playlist_fetcher_t *p_fetcher = p_data;
    playlist_t *p_playlist = p_fetcher->p_playlist;

    for( ;; )
    {
        input_item_t *p_item = NULL;

        vlc_mutex_lock( &p_fetcher->lock );
        if( p_fetcher->i_waiting != 0 )
        {
            p_item = p_fetcher->pp_waiting[0];
            REMOVE_ELEM( p_fetcher->pp_waiting, p_fetcher->i_waiting, 0 );
        }
        else
        {
            p_fetcher->b_live = false;
            vlc_cond_signal( &p_fetcher->wait );
        }
        vlc_mutex_unlock( &p_fetcher->lock );

        if( !p_item )
            break;

        /* */

        /* Wait that the input item is preparsed if it is being played */
        WaitPreparsed( p_fetcher, p_item );

        /* Triggers "meta fetcher", eventually fetch meta on the network.
         * They are identical to "meta reader" expect that may actually
         * takes time. That's why they are running here.
         * The result of this fetch is not cached. */
        FetchMeta( p_fetcher, p_item );

        /* Find art, and download it if needed */
        int i_ret = FindArt( p_fetcher, p_item );
        if( i_ret == 1 )
            i_ret = DownloadArt( p_fetcher, p_item );

        /* */
        char *psz_name = input_item_GetName( p_item );
        if( !i_ret ) /* Art is now in cache */
        {
            PL_DEBUG( "found art for %s in cache", psz_name );
            input_item_SetArtFetched( p_item, true );
            var_SetAddress( p_playlist, "item-change", p_item );
        }
        else
        {
            PL_DEBUG( "art not found for %s", psz_name );
            input_item_SetArtNotFound( p_item, true );
        }
        free( psz_name );
        vlc_gc_decref( p_item );
    }
    return NULL;
}
