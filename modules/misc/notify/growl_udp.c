/*****************************************************************************
 * growl_udp.c : growl UDP notification plugin
 *****************************************************************************
 * Copyright (C) 2006 the VideoLAN team
 * $Id: 17ec991555a10684606aebe64cb05412870dfab3 $
 *
 * Authors: Jérôme Decoodt <djc -at- videolan -dot- org>
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
#include <vlc_playlist.h>
#include <vlc_meta.h>
#include <vlc_network.h>
#include <errno.h>
#include <vlc_md5.h>

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Open    ( vlc_object_t * );
static void Close   ( vlc_object_t * );

static int ItemChange( vlc_object_t *, const char *,
                       vlc_value_t, vlc_value_t, void * );

static int RegisterToGrowl( vlc_object_t *p_this );
static int NotifyToGrowl( vlc_object_t *p_this, const char *psz_desc );
static int CheckAndSend( vlc_object_t *p_this, uint8_t* p_data, int i_offset );
#define GROWL_MAX_LENGTH 256

/*****************************************************************************
 * Module descriptor
 ****************************************************************************/

#define SERVER_DEFAULT "127.0.0.1"
#define SERVER_TEXT N_("Server")
#define SERVER_LONGTEXT N_("This is the host to which Growl notifications " \
   "will be sent. By default, notifications are sent locally." )
#define PASS_DEFAULT ""
#define PASS_TEXT N_("Password")
#define PASS_LONGTEXT N_("Growl password on the Growl server.")
#define PORT_TEXT N_("UDP port")
#define PORT_LONGTEXT N_("Growl UDP port on the Growl server.")

vlc_module_begin();
    set_category( CAT_INTERFACE );
    set_subcategory( SUBCAT_INTERFACE_CONTROL );
    set_shortname( "Growl-UDP" );
    set_description( N_("Growl UDP Notification Plugin") );

    add_string( "growl-server", SERVER_DEFAULT, NULL,
                SERVER_TEXT, SERVER_LONGTEXT, false );
    add_password( "growl-password", PASS_DEFAULT, NULL,
                PASS_TEXT, PASS_LONGTEXT, false );
    add_integer( "growl-port", 9887, NULL,
                PORT_TEXT, PORT_LONGTEXT, true );

    set_capability( "interface", 0 );
    set_callbacks( Open, Close );
vlc_module_end();

/*****************************************************************************
 * Open: initialize and create stuff
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_this;

    playlist_t *p_playlist = pl_Yield( p_intf );
    var_AddCallback( p_playlist, "playlist-current", ItemChange, p_intf );
    pl_Release( p_intf );

    RegisterToGrowl( p_this );
    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: destroy interface stuff
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    playlist_t *p_playlist = pl_Yield( p_this );
    var_DelCallback( p_playlist, "playlist-current", ItemChange, p_this );
    pl_Release( p_this );
}

/*****************************************************************************
 * ItemChange: Playlist item change callback
 *****************************************************************************/
static int ItemChange( vlc_object_t *p_this, const char *psz_var,
                       vlc_value_t oldval, vlc_value_t newval, void *param )
{
    VLC_UNUSED(psz_var); VLC_UNUSED(oldval); VLC_UNUSED(newval);
    VLC_UNUSED(param);
    char psz_tmp[GROWL_MAX_LENGTH];
    char *psz_title = NULL;
    char *psz_artist = NULL;
    char *psz_album = NULL;
    input_thread_t *p_input;
    playlist_t *p_playlist = pl_Yield( p_this );

    p_input = p_playlist->p_input;
    pl_Release( p_this );

    if( !p_input ) return VLC_SUCCESS;
    vlc_object_yield( p_input );

    char *psz_name = input_item_GetName( input_GetItem( p_input ) );
    if( p_input->b_dead || !psz_name )
    {
        /* Not playing anything ... */
        free( psz_name );
        vlc_object_release( p_input );
        return VLC_SUCCESS;
    }
    free( psz_name );

    /* Playing something ... */
    input_item_t *p_item = input_GetItem( p_input );

    psz_title = input_item_GetTitle( p_item );
    if( psz_title == NULL || EMPTY_STR( psz_title ) )
    {
        free( psz_title );
        psz_title = input_item_GetName( input_GetItem( p_input ) );
        if( psz_title == NULL || EMPTY_STR( psz_title ) )
        {
            free( psz_title );
            vlc_object_release( p_input );
            return VLC_SUCCESS;
        }
    }

    psz_artist = input_item_GetArtist( p_item );
    if( EMPTY_STR( psz_artist ) ) FREENULL( psz_artist );
    psz_album = input_item_GetAlbum( p_item ) ;
    if( EMPTY_STR( psz_album ) ) FREENULL( psz_album );

    if( psz_artist && psz_album )
        snprintf( psz_tmp, GROWL_MAX_LENGTH, "%s\n%s [%s]",
                psz_title, psz_artist, psz_album );
    else if( psz_artist )
        snprintf( psz_tmp, GROWL_MAX_LENGTH, "%s\n%s", psz_title, psz_artist );
    else
        snprintf( psz_tmp, GROWL_MAX_LENGTH, "%s", psz_title );

    free( psz_title );
    free( psz_artist );
    free( psz_album );

    NotifyToGrowl( p_this, psz_tmp );

    vlc_object_release( p_input );
    return VLC_SUCCESS;
}

/*****************************************************************************
 * Growl specific functions
 *****************************************************************************/
#define GROWL_PROTOCOL_VERSION (1)
#define GROWL_TYPE_REGISTRATION (0)
#define GROWL_TYPE_NOTIFICATION (1)
#define APPLICATION_NAME "VLC media player"

#define insertstrlen( psz ) \
{ \
    uint16_t i_size = strlen( psz ); \
    psz_encoded[i++] = (i_size>>8)&0xFF; \
    psz_encoded[i++] = i_size&0xFF; \
}
/*****************************************************************************
 * RegisterToGrowl
 *****************************************************************************/
static int RegisterToGrowl( vlc_object_t *p_this )
{
    uint8_t *psz_encoded = malloc(100);
    uint8_t i_defaults = 0;
    static const char *psz_notifications[] = {"Now Playing", NULL};
    bool pb_defaults[] = {true, false};
    int i = 0, j;
    if( psz_encoded == NULL )
        return false;

    memset( psz_encoded, 0, sizeof(psz_encoded) );
    psz_encoded[i++] = GROWL_PROTOCOL_VERSION;
    psz_encoded[i++] = GROWL_TYPE_REGISTRATION;
    insertstrlen(APPLICATION_NAME);
    i+=2;
    strcpy( (char*)(psz_encoded+i), APPLICATION_NAME );
    i += strlen(APPLICATION_NAME);
    for( j = 0 ; psz_notifications[j] != NULL ; j++)
    {
        insertstrlen(psz_notifications[j]);
        strcpy( (char*)(psz_encoded+i), psz_notifications[j] );
        i += strlen(psz_notifications[j]);
    }
    psz_encoded[4] = j;
    for( j = 0 ; psz_notifications[j] != NULL ; j++)
        if(pb_defaults[j] == true)
        {
            psz_encoded[i++] = (uint8_t)j;
            i_defaults++;
        }
    psz_encoded[5] = i_defaults;

    CheckAndSend(p_this, psz_encoded, i);
    free( psz_encoded );
    return VLC_SUCCESS;
}

static int NotifyToGrowl( vlc_object_t *p_this, const char *psz_desc )
{
    const char *psz_type = "Now Playing", *psz_title = "Now Playing";
    uint8_t *psz_encoded = malloc(GROWL_MAX_LENGTH + 42);
    uint16_t flags;
    int i = 0;
    if( psz_encoded == NULL )
        return false;

    memset( psz_encoded, 0, sizeof(psz_encoded) );
    psz_encoded[i++] = GROWL_PROTOCOL_VERSION;
    psz_encoded[i++] = GROWL_TYPE_NOTIFICATION;
    flags = 0;
    psz_encoded[i++] = (flags>>8)&0xFF;
    psz_encoded[i++] = flags&0xFF;
    insertstrlen(psz_type);
    insertstrlen(psz_title);
    insertstrlen(psz_desc);
    insertstrlen(APPLICATION_NAME);
    strcpy( (char*)(psz_encoded+i), psz_type );
    i += strlen(psz_type);
    strcpy( (char*)(psz_encoded+i), psz_title );
    i += strlen(psz_title);
    strcpy( (char*)(psz_encoded+i), psz_desc );
    i += strlen(psz_desc);
    strcpy( (char*)(psz_encoded+i), APPLICATION_NAME );
    i += strlen(APPLICATION_NAME);

    CheckAndSend(p_this, psz_encoded, i);
    free( psz_encoded );
    return VLC_SUCCESS;
}

static int CheckAndSend( vlc_object_t *p_this, uint8_t* p_data, int i_offset )
{
    int i, i_handle;
    struct md5_s md5;
    intf_thread_t *p_intf = (intf_thread_t *)p_this;
    char *psz_password = config_GetPsz( p_intf, "growl-password" );
    char *psz_server = config_GetPsz( p_intf, "growl-server" );
    int i_port = config_GetInt( p_intf, "growl-port" );
    strcpy( (char*)(p_data+i_offset), psz_password );
    i = i_offset + strlen(psz_password);

    InitMD5( &md5 );
    AddMD5( &md5, p_data, i );
    EndMD5( &md5 );

    for( i = 0 ; i < 4 ; i++ )
    {
        md5.p_digest[i] = md5.p_digest[i];
        p_data[i_offset++] =  md5.p_digest[i]     &0xFF;
        p_data[i_offset++] = (md5.p_digest[i]>> 8)&0xFF;
        p_data[i_offset++] = (md5.p_digest[i]>>16)&0xFF;
        p_data[i_offset++] = (md5.p_digest[i]>>24)&0xFF;
    }

    i_handle = net_ConnectUDP( p_this, psz_server, i_port, 0 );
    if( i_handle == -1 )
    {
         msg_Err( p_this, "failed to open a connection (udp)" );
         free( psz_password);
         free( psz_server);
         return VLC_EGENERIC;
    }

    shutdown( i_handle, SHUT_RD );
    if( send( i_handle, p_data, i_offset, 0 )
          == -1 )
    {
        msg_Warn( p_this, "send error: %m" );
    }
    net_Close( i_handle );

    free( psz_password);
    free( psz_server);
    return VLC_SUCCESS;
}

#undef GROWL_PROTOCOL_VERSION
#undef GROWL_TYPE_REGISTRATION
#undef GROWL_TYPE_NOTIFICATION
#undef APPLICATION_NAME
#undef insertstrlen
