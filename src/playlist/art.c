/*****************************************************************************
 * art.c : Art metadata handling
 *****************************************************************************
 * Copyright (C) 1998-2008 the VideoLAN team
 * $Id: 8b65121b48f777ff5c081875bd6bc5531931342b $
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
#include <vlc_charset.h>
#include <vlc_strings.h>
#include <vlc_stream.h>
#include <vlc_url.h>

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
            utf8_mkdir( psz_newdir, 0700 );
        *psz = DIR_SEP_CHAR;
        psz++;
    }
    utf8_mkdir( psz_dir, 0700 );
}

static void ArtCacheGetDirPath( char *psz_dir,
                                const char *psz_title,
                                const char *psz_artist, const char *psz_album )
{
    char *psz_cachedir = config_GetCacheDir();

    if( !EMPTY_STR(psz_artist) && !EMPTY_STR(psz_album) )
    {
        char *psz_album_sanitized = filename_sanitize( psz_album );
        char *psz_artist_sanitized = filename_sanitize( psz_artist );
        snprintf( psz_dir, PATH_MAX, "%s" DIR_SEP
                  "art" DIR_SEP "artistalbum" DIR_SEP "%s" DIR_SEP "%s",
                  psz_cachedir, psz_artist_sanitized, psz_album_sanitized );
        free( psz_album_sanitized );
        free( psz_artist_sanitized );
    }
    else
    {
        char * psz_title_sanitized = filename_sanitize( psz_title );
        snprintf( psz_dir, PATH_MAX, "%s" DIR_SEP
                  "art" DIR_SEP "title" DIR_SEP "%s",
                  psz_cachedir, psz_title_sanitized );
        free( psz_title_sanitized );
    }
    free( psz_cachedir );
}

static char *ArtCachePath( input_item_t *p_item )
{
    char psz_path[PATH_MAX+1]; /* FIXME */

    vlc_mutex_lock( &p_item->lock );

    if( !p_item->p_meta )
        p_item->p_meta = vlc_meta_New();
    if( !p_item->p_meta )
    {
        vlc_mutex_unlock( &p_item->lock );
        return NULL;
    }

    const char *psz_artist = vlc_meta_Get( p_item->p_meta, vlc_meta_Artist );
    const char *psz_album = vlc_meta_Get( p_item->p_meta, vlc_meta_Album );
    const char *psz_title = vlc_meta_Get( p_item->p_meta, vlc_meta_Title );

    if( !psz_title )
        psz_title = p_item->psz_name;

    if( (!psz_artist || !psz_album ) && !psz_title )
    {
        vlc_mutex_unlock( &p_item->lock );
        return NULL;
    }

    ArtCacheGetDirPath( psz_path, psz_title, psz_artist, psz_album );

    vlc_mutex_unlock( &p_item->lock );

    return strdup( psz_path );
}

static char *ArtCacheName( input_item_t *p_item, const char *psz_type )
{
    char *psz_path = ArtCachePath( p_item );
    if( !psz_path )
        return NULL;

    ArtCacheCreateDir( psz_path );

    char *psz_ext = filename_sanitize( psz_type ? psz_type : "" );
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
    DIR *p_dir = utf8_opendir( psz_path );
    if( !p_dir )
    {
        free( psz_path );
        return VLC_EGENERIC;
    }

    bool b_found = false;
    char *psz_filename;
    while( !b_found && (psz_filename = utf8_readdir( p_dir )) )
    {
        if( !strncmp( psz_filename, "art", 3 ) )
        {
            char *psz_file;
            if( asprintf( &psz_file, "%s" DIR_SEP "%s",
                          psz_path, psz_filename ) < 0 )
                psz_file = NULL;
            if( psz_file )
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
    if( !utf8_stat( psz_filename, &s ) )
    {
        input_item_SetArtURL( p_item, psz_uri );
        free( psz_filename );
        free( psz_uri );
        return VLC_SUCCESS;
    }

    /* Dump it otherwise */
    FILE *f = utf8_fopen( psz_filename, "wb" );
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

