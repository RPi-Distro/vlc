/*****************************************************************************
 * art.c : Art metadata handling
 *****************************************************************************
 * Copyright (C) 1998-2008 the VideoLAN team
 * $Id: 315fd4e62b19ca5ed6678b132f25b52b6f101b65 $
 *
 * Authors: Antoine Cellerier <dionoea@videolan.org>
 *          Clément Stenac <zorglub@videolan.org
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
#include <vlc_fs.h>
#include <vlc_strings.h>
#include <vlc_stream.h>
#include <vlc_url.h>
#include <vlc_md5.h>

#include <limits.h>                                             /* PATH_MAX */

#ifdef HAVE_SYS_STAT_H
#   include <sys/stat.h>
#endif

#include "../libvlc.h"
#include "playlist_internal.h"

static void ArtCacheCreateDir( const char *psz_dir )
{
    char newdir[strlen( psz_dir ) + 1];
    strcpy( newdir, psz_dir );
    char * psz_newdir = newdir;
    char * psz = psz_newdir;

    while( *psz )
    {
        while( *psz && *psz != DIR_SEP_CHAR) psz++;
        if( !*psz ) break;
        *psz = 0;
        if( !EMPTY_STR( psz_newdir ) )
            vlc_mkdir( psz_newdir, 0700 );
        *psz = DIR_SEP_CHAR;
        psz++;
    }
    vlc_mkdir( psz_dir, 0700 );
}

static char* ArtCacheGetDirPath( const char *psz_arturl, const char *psz_artist,
                                 const char *psz_album )
{
    char *psz_dir;
    char *psz_cachedir = config_GetUserDir(VLC_CACHE_DIR);

    if( !EMPTY_STR(psz_artist) && !EMPTY_STR(psz_album) )
    {
        char *psz_album_sanitized = strdup( psz_album );
        filename_sanitize( psz_album_sanitized );
        char *psz_artist_sanitized = strdup( psz_artist );
        filename_sanitize( psz_artist_sanitized );
        if( asprintf( &psz_dir, "%s" DIR_SEP "art" DIR_SEP "artistalbum"
                      DIR_SEP "%s" DIR_SEP "%s", psz_cachedir,
                      psz_artist_sanitized, psz_album_sanitized ) == -1 )
            psz_dir = NULL;
        free( psz_album_sanitized );
        free( psz_artist_sanitized );
    }
    else
    {
        /* If artist or album missing cache by art download URL. The download
           URL will be md5 hashed to form a valid cache filename. We assume that
           psz_arturl is always the download URL and not the already hashed filename.
           (We should never need to call this function if art has already been
           downloaded anyway). */
        struct md5_s md5;
        InitMD5( &md5 );
        AddMD5( &md5, psz_arturl, strlen( psz_arturl ) );
        EndMD5( &md5 );
        char * psz_arturl_sanitized = psz_md5_hash( &md5 );
        if( asprintf( &psz_dir, "%s" DIR_SEP "art" DIR_SEP "arturl" DIR_SEP
                      "%s", psz_cachedir, psz_arturl_sanitized ) == -1 )
            psz_dir = NULL;
        free( psz_arturl_sanitized );
    }
    free( psz_cachedir );
    return psz_dir;
}

static char *ArtCachePath( input_item_t *p_item )
{
    char* psz_path = NULL;
    const char *psz_artist;
    const char *psz_album;
    const char *psz_arturl;

    vlc_mutex_lock( &p_item->lock );

    if( !p_item->p_meta )
        p_item->p_meta = vlc_meta_New();
    if( !p_item->p_meta )
        goto end;

    psz_artist = vlc_meta_Get( p_item->p_meta, vlc_meta_Artist );
    psz_album = vlc_meta_Get( p_item->p_meta, vlc_meta_Album );
    psz_arturl = vlc_meta_Get( p_item->p_meta, vlc_meta_ArtworkURL );

    if( (!psz_artist || !psz_album ) && !psz_arturl )
        goto end;

    psz_path = ArtCacheGetDirPath( psz_arturl, psz_artist, psz_album );

end:
    vlc_mutex_unlock( &p_item->lock );
    return psz_path;
}

static char *ArtCacheName( input_item_t *p_item, const char *psz_type )
{
    char *psz_path = ArtCachePath( p_item );
    if( !psz_path )
        return NULL;

    ArtCacheCreateDir( psz_path );

    char *psz_ext = strdup( psz_type ? psz_type : "" );
    filename_sanitize( psz_ext );
    char *psz_filename;
    if( asprintf( &psz_filename, "%s" DIR_SEP "art%s", psz_path, psz_ext ) < 0 )
        psz_filename = NULL;

    free( psz_ext );
    free( psz_path );

    return psz_filename;
}

/* */
int playlist_FindArtInCache( input_item_t *p_item )
{
    char *psz_path = ArtCachePath( p_item );

    if( !psz_path )
        return VLC_EGENERIC;

    /* Check if file exists */
    DIR *p_dir = vlc_opendir( psz_path );
    if( !p_dir )
    {
        free( psz_path );
        return VLC_EGENERIC;
    }

    bool b_found = false;
    char *psz_filename;
    while( !b_found && (psz_filename = vlc_readdir( p_dir )) )
    {
        if( !strncmp( psz_filename, "art", 3 ) )
        {
            char *psz_file;
            if( asprintf( &psz_file, "%s" DIR_SEP "%s",
                          psz_path, psz_filename ) != -1 )
            {
                char *psz_uri = make_URI( psz_file );
                if( psz_uri )
                {
                    input_item_SetArtURL( p_item, psz_uri );
                    free( psz_uri );
                }
                free( psz_file );
            }

            b_found = true;
        }
        free( psz_filename );
    }

    /* */
    closedir( p_dir );
    free( psz_path );
    return b_found ? VLC_SUCCESS : VLC_EGENERIC;
}


/* */
int playlist_SaveArt( playlist_t *p_playlist, input_item_t *p_item,
                      const uint8_t *p_buffer, int i_buffer, const char *psz_type )
{
    char *psz_filename = ArtCacheName( p_item, psz_type );

    if( !psz_filename )
        return VLC_EGENERIC;

    char *psz_uri = make_URI( psz_filename );
    if( !psz_uri )
    {
        free( psz_filename );
        return VLC_EGENERIC;
    }

    /* Check if we already dumped it */
    struct stat s;
    if( !vlc_stat( psz_filename, &s ) )
    {
        input_item_SetArtURL( p_item, psz_uri );
        free( psz_filename );
        free( psz_uri );
        return VLC_SUCCESS;
    }

    /* Dump it otherwise */
    FILE *f = vlc_fopen( psz_filename, "wb" );
    if( f )
    {
        if( fwrite( p_buffer, i_buffer, 1, f ) != 1 )
        {
            msg_Err( p_playlist, "%s: %m", psz_filename );
        }
        else
        {
            msg_Dbg( p_playlist, "album art saved to %s", psz_filename );
            input_item_SetArtURL( p_item, psz_uri );
        }
        fclose( f );
    }
    free( psz_filename );
    free( psz_uri );
    return VLC_SUCCESS;
}

