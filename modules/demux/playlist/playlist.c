/*****************************************************************************
 * playlist.c :  Playlist import module
 *****************************************************************************
 * Copyright (C) 2004 the VideoLAN team
 * $Id: 8446fe44007b122ecda0b5a6ad0968dfa6d4dace $
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
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
#include <vlc_plugin.h>
#include <vlc_demux.h>
#include <vlc_url.h>
#ifdef WIN32
# include <ctype.h>
#endif

#include "playlist.h"

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define AUTOSTART_TEXT N_( "Auto start" )
#define AUTOSTART_LONGTEXT N_( "Automatically start playing the playlist " \
                "content once it's loaded." )

#define SHOW_ADULT_TEXT N_( "Show shoutcast adult content" )
#define SHOW_ADULT_LONGTEXT N_( "Show NC17 rated video streams when " \
                "using shoutcast video playlists." )

#define SKIP_ADS_TEXT N_( "Skip ads" )
#define SKIP_ADS_LONGTEXT N_( "Use playlist options usually used to prevent " \
    "ads skipping to detect ads and prevent adding them to the playlist." )

vlc_module_begin ()
    add_shortcut( "playlist" )
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_DEMUX )

    add_bool( "playlist-autostart", true, NULL,
              AUTOSTART_TEXT, AUTOSTART_LONGTEXT, false )

    add_obsolete_integer( "parent-item" ) /* removed since 1.1.0 */

    add_bool( "playlist-skip-ads", true, NULL,
              SKIP_ADS_TEXT, SKIP_ADS_LONGTEXT, false )

    set_shortname( N_("Playlist") )
    set_description( N_("Playlist") )
    add_submodule ()
        set_description( N_("M3U playlist import") )
        add_shortcut( "playlist" )
        add_shortcut( "m3u" )
        add_shortcut( "m3u8" )
        add_shortcut( "m3u-open" )
        set_capability( "demux", 10 )
        set_callbacks( Import_M3U, Close_M3U )
    add_submodule ()
        set_description( N_("RAM playlist import") )
        add_shortcut( "playlist" )
        add_shortcut( "ram-open" )
        set_capability( "demux", 10 )
        set_callbacks( Import_RAM, Close_RAM )
    add_submodule ()
        set_description( N_("PLS playlist import") )
        add_shortcut( "playlist" )
        add_shortcut( "pls-open" )
        set_capability( "demux", 10 )
        set_callbacks( Import_PLS, Close_PLS )
    add_submodule ()
        set_description( N_("B4S playlist import") )
        add_shortcut( "playlist" )
        add_shortcut( "b4s-open" )
        add_shortcut( "shout-b4s" )
        set_capability( "demux", 10 )
        set_callbacks( Import_B4S, Close_B4S )
    add_submodule ()
        set_description( N_("DVB playlist import") )
        add_shortcut( "playlist" )
        add_shortcut( "dvb-open" )
        set_capability( "demux", 10 )
        set_callbacks( Import_DVB, Close_DVB )
    add_submodule ()
        set_description( N_("Podcast parser") )
        add_shortcut( "playlist" )
        add_shortcut( "podcast" )
        set_capability( "demux", 10 )
        set_callbacks( Import_podcast, Close_podcast )
    add_submodule ()
        set_description( N_("XSPF playlist import") )
        add_shortcut( "playlist" )
        add_shortcut( "xspf-open" )
        set_capability( "demux", 10 )
        set_callbacks( Import_xspf, Close_xspf )
    add_submodule ()
        set_description( N_("New winamp 5.2 shoutcast import") )
        add_shortcut( "playlist" )
        add_shortcut( "shout-winamp" )
        set_capability( "demux", 10 )
        set_callbacks( Import_Shoutcast, Close_Shoutcast )
        add_bool( "shoutcast-show-adult", false, NULL,
                   SHOW_ADULT_TEXT, SHOW_ADULT_LONGTEXT, false )
    add_submodule ()
        set_description( N_("ASX playlist import") )
        add_shortcut( "playlist" )
        add_shortcut( "asx-open" )
        set_capability( "demux", 10 )
        set_callbacks( Import_ASX, Close_ASX )
    add_submodule ()
        set_description( N_("Kasenna MediaBase parser") )
        add_shortcut( "playlist" )
        add_shortcut( "sgimb" )
        set_capability( "demux", 10 )
        set_callbacks( Import_SGIMB, Close_SGIMB )
    add_submodule ()
        set_description( N_("QuickTime Media Link importer") )
        add_shortcut( "playlist" )
        add_shortcut( "qtl" )
        set_capability( "demux", 10 )
        set_callbacks( Import_QTL, Close_QTL )
    add_submodule ()
        set_description( N_("Google Video Playlist importer") )
        add_shortcut( "playlist" )
        add_shortcut( "gvp" )
        set_capability( "demux", 10 )
        set_callbacks( Import_GVP, Close_GVP )
    add_submodule ()
        set_description( N_("Dummy ifo demux") )
        add_shortcut( "playlist" )
        set_capability( "demux", 12 )
        set_callbacks( Import_IFO, Close_IFO )
    add_submodule ()
        set_description( N_("iTunes Music Library importer") )
        add_shortcut( "playlist" )
        add_shortcut( "itml" )
        set_capability( "demux", 10 )
        set_callbacks( Import_iTML, Close_iTML )
    add_submodule ()
        set_description( N_("WPL playlist import") )
        add_shortcut( "playlist" )
        add_shortcut( "wpl" )
        set_capability( "demux", 10 )
        set_callbacks( Import_WPL, Close_WPL )
    add_submodule ()
        set_description( N_("ZPL playlist import") )
        add_shortcut( "playlist" )
        add_shortcut( "zpl" )
        set_capability( "demux", 10 )
        set_callbacks( Import_ZPL, Close_ZPL )
vlc_module_end ()

input_item_t * GetCurrentItem(demux_t *p_demux)
{
    input_thread_t *p_input_thread = demux_GetParentInput( p_demux );
    input_item_t *p_current_input = input_GetItem( p_input_thread );
    vlc_gc_incref(p_current_input);
    vlc_object_release(p_input_thread);
    return p_current_input;
}

/**
 * Find directory part of the path to the playlist file, in case of
 * relative paths inside
 */
char *FindPrefix( demux_t *p_demux )
{
    char *psz_file;
    char *psz_prefix;
    char *psz_path;

    if( p_demux->psz_access && *p_demux->psz_access
     && strcasecmp( p_demux->psz_access, "file" ) )
    {
        if( asprintf( &psz_path,"%s://%s", p_demux->psz_access, p_demux->psz_path ) == -1 )
            return NULL;
    }
    else
    {
        psz_path = make_URI( p_demux->psz_path );
        if( psz_path == NULL )
            return NULL;
    }

#ifdef WIN32
    psz_file = strrchr( psz_path, '\\' );
    if( !psz_file )
#endif
    psz_file = strrchr( psz_path, '/' );

    if( psz_file )
        psz_prefix = strndup( psz_path, psz_file - psz_path + 1 );
    else
        psz_prefix = strdup( "" );
    free( psz_path );

    return psz_prefix;
}

/**
 * Add the directory part of the playlist file to the start of the
 * mrl, if the mrl is a relative file path
 */
char *ProcessMRL( const char *psz_mrl, const char *psz_prefix )
{
    /* Check for a protocol name.
     * for URL, we should look for "://"
     * for MRL (Media Resource Locator) ([[<access>][/<demux>]:][<source>]),
     * we should look for ":", so we end up looking simply for ":"
     * PB: on some file systems, ':' are valid characters though */

    /* Simple cases first */
    if( !psz_mrl || !*psz_mrl )
        return NULL;
    if( !psz_prefix || !*psz_prefix )
        goto uri;

    /* Check if the line specifies an absolute path */
    /* FIXME: that's wrong if the playlist is not a local file */
    if( *psz_mrl == DIR_SEP_CHAR )
        goto uri;
#ifdef WIN32
    /* Drive letter (this assumes URL scheme are not a single character) */
    if( isalpha(psz_mrl[0]) && psz_mrl[1] == ':' )
        goto uri;
#endif
    if( strstr( psz_mrl, "://" ) )
        return strdup( psz_mrl );

    /* This a relative path, prepend the prefix */
    char *ret;
    char *postfix = encode_URI_component( psz_mrl );
    /* FIXME: postfix may not be encoded correctly (esp. slashes) */
    if( postfix == NULL
     || asprintf( &ret, "%s%s", psz_prefix, postfix ) == -1 )
        ret = NULL;
    free( postfix );
    return ret;

uri:
    return make_URI( psz_mrl );
}
