/*****************************************************************************
 * msn.c : msn title plugin
 *****************************************************************************
 * Copyright (C) 2005 the VideoLAN team
 * $Id: 45553ff49b5534ee4ed202d695d6e86034ca9ca3 $
 *
 * Authors: Antoine Cellerier <dionoea -at- videolan -dot- org>
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
#include <vlc_interface.h>
#include <vlc_meta.h>
#include <vlc_playlist.h>
#include <vlc_strings.h>

/*****************************************************************************
 * intf_sys_t: description and status of log interface
 *****************************************************************************/
struct intf_sys_t
{
    char *psz_format;
};

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Open    ( vlc_object_t * );
static void Close   ( vlc_object_t * );

static int ItemChange( vlc_object_t *, const char *,
                       vlc_value_t, vlc_value_t, void * );
static int SendToMSN( const char * psz_msg );

#define MSN_MAX_LENGTH 256

/*****************************************************************************
 * Module descriptor
 *****************************************************************************
 * This module should be used on windows with MSN (i think that you need to
 * have MSN 7 or newer) to "advertise" what you are playing in VLC.
 * You need to enable the "What I'm Listening To" option in MSN.
 *****************************************************************************/
#define FORMAT_DEFAULT "{0} - {1}"
#define FORMAT_TEXT N_("Title format string")
#define FORMAT_LONGTEXT N_("Format of the string to send to MSN " \
"{0} Artist, {1} Title, {2} Album. Defaults to \"Artist - Title\" ({0} - {1}).")

vlc_module_begin();
    set_category( CAT_INTERFACE );
    set_subcategory( SUBCAT_INTERFACE_CONTROL );
    set_shortname( "MSN" );
    set_description( N_("MSN Now-Playing") );

    add_string( "msn-format", FORMAT_DEFAULT, NULL,
                FORMAT_TEXT, FORMAT_LONGTEXT, false );

    set_capability( "interface", 0 );
    set_callbacks( Open, Close );
vlc_module_end();

/*****************************************************************************
 * Open: initialize and create stuff
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_this;
    playlist_t *p_playlist;

    MALLOC_ERR( p_intf->p_sys, intf_sys_t );

    p_intf->p_sys->psz_format = config_GetPsz( p_intf, "msn-format" );
    if( !p_intf->p_sys->psz_format )
    {
        msg_Dbg( p_intf, "no format provided" );
        p_intf->p_sys->psz_format = strdup( FORMAT_DEFAULT );
    }
    msg_Dbg( p_intf, "using format: %s", p_intf->p_sys->psz_format );

    p_playlist = pl_Yield( p_intf );
    var_AddCallback( p_playlist, "item-change", ItemChange, p_intf );
    var_AddCallback( p_playlist, "playlist-current", ItemChange, p_intf );
    pl_Release( p_intf );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: destroy interface stuff
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_this;
    playlist_t *p_playlist = pl_Yield( p_this );

    /* clear the MSN stuff ... else it looks like we're still playing
     * something although VLC (or the MSN plugin) is closed */
    SendToMSN( "\\0Music\\01\\0\\0\\0\\0\\0\\0\\0" );

    var_DelCallback( p_playlist, "item-change", ItemChange, p_intf );
    var_DelCallback( p_playlist, "playlist-current", ItemChange, p_intf );
    pl_Release( p_this );

    /* Destroy structure */
    free( p_intf->p_sys->psz_format );
    free( p_intf->p_sys );
}

/*****************************************************************************
 * ItemChange: Playlist item change callback
 *****************************************************************************/
static int ItemChange( vlc_object_t *p_this, const char *psz_var,
                       vlc_value_t oldval, vlc_value_t newval, void *param )
{
    (void)psz_var;    (void)oldval;    (void)newval;
    intf_thread_t *p_intf = (intf_thread_t *)param;
    char psz_tmp[MSN_MAX_LENGTH];
    char *psz_title = NULL;
    char *psz_artist = NULL;
    char *psz_album = NULL;
    char *psz_buf = NULL;
    input_thread_t *p_input;
    playlist_t *p_playlist = pl_Yield( p_this );

    p_input = p_playlist->p_input;
    pl_Release( p_this );

    if( !p_input ) return VLC_SUCCESS;
    vlc_object_yield( p_input );

    if( p_input->b_dead || !input_GetItem(p_input)->psz_name )
    {
        /* Not playing anything ... */
        SendToMSN( "\\0Music\\01\\0\\0\\0\\0\\0\\0\\0" );
        vlc_object_release( p_input );
        return VLC_SUCCESS;
    }

    /* Playing something ... */
    psz_artist = input_item_GetArtist( input_GetItem( p_input ) );
    psz_album = input_item_GetAlbum( input_GetItem( p_input ) );
    psz_title = input_item_GetTitle( input_GetItem( p_input ) );
    if( !psz_artist ) psz_artist = strdup( "" );
    if( !psz_album ) psz_artist = strdup( "" );
    if( !psz_title )
        psz_title = input_item_GetName( input_GetItem( p_input ) );

    psz_buf = str_format_meta( p_this, p_intf->p_sys->psz_format );

    snprintf( psz_tmp,
              MSN_MAX_LENGTH,
              "\\0Music\\01\\0%s\\0%s\\0%s\\0%s\\0\\0\\0",
              psz_buf,
              psz_artist,
              psz_title,
              psz_album );
    free( psz_buf );
    free( psz_title );
    free( psz_artist );
    free( psz_album );

    SendToMSN( (const char*)psz_tmp );
    vlc_object_release( p_input );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * SendToMSN
 *****************************************************************************/
static int SendToMSN( const char *psz_msg )
{
    COPYDATASTRUCT msndata;
    HWND msnui = NULL;

    wchar_t buffer[MSN_MAX_LENGTH];

    mbstowcs( buffer, psz_msg, MSN_MAX_LENGTH );

    msndata.dwData = 0x547;
    msndata.lpData = &buffer;
    msndata.cbData = (lstrlenW(buffer)*2)+2;

    while( ( msnui = FindWindowEx( NULL, msnui, "MsnMsgrUIManager", NULL ) ) )
    {
        SendMessage(msnui, WM_COPYDATA, (WPARAM)NULL, (LPARAM)&msndata);
    }

    return VLC_SUCCESS;
}
