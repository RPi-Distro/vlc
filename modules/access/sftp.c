/*****************************************************************************
 * sftp.c: SFTP input module
 *****************************************************************************
 * Copyright (C) 2009 the VideoLAN team
 * $Id: 51d4b6712d33c867ec1f71fc8a48ce69ac65a6d7 $
 *
 * Authors: Rémi Duraffort <ivoire@videolan.org>
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

#include <assert.h>

#include <vlc_access.h>
#include <vlc_dialog.h>
#include <vlc_network.h>
#include <vlc_url.h>

#include <libssh2.h>
#include <libssh2_sftp.h>


/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open ( vlc_object_t* );
static void Close( vlc_object_t* );

#define CACHING_TEXT N_("Caching value in ms")
#define CACHING_LONGTEXT N_( \
  "Caching value for SFTP streams. This value should be set in milliseconds." )
#define USER_TEXT N_("SFTP user name")
#define USER_LONGTEXT N_("User name that will be used for the connection.")
#define PASS_TEXT N_("SFTP password")
#define PASS_LONGTEXT N_("Password that will be used for the connection.")
#define PORT_TEXT N_("SFTP port")
#define PORT_LONGTEXT N_("SFTP port number to use on the server")
#define MTU_TEXT N_("Read size")
#define MTU_LONGTEXT N_("Size of the request for reading access")

vlc_module_begin ()
    set_shortname( "SFTP" )
    set_description( N_("SFTP input") )
    set_capability( "access", 0 )
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_ACCESS )
    add_integer( "sftp-caching", 2 * DEFAULT_PTS_DELAY / 1000, NULL,
                     CACHING_TEXT, CACHING_LONGTEXT, true );
    add_integer( "sftp-readsize", 8192, NULL, MTU_TEXT, MTU_LONGTEXT, true )
    add_integer( "sftp-port", 22, NULL, PORT_TEXT, PORT_LONGTEXT, true )
    add_shortcut( "sftp" )
    set_callbacks( Open, Close )
vlc_module_end ()


/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static block_t* Block( access_t * );
static int      Seek( access_t *, uint64_t );
static int      Control( access_t *, int, va_list );


struct access_sys_t
{
    int i_socket;
    LIBSSH2_SESSION* ssh_session;
    LIBSSH2_SFTP* sftp_session;
    LIBSSH2_SFTP_HANDLE* file;
    int i_read_size;
};



/**
 * Connect to the sftp server and ask for a file
 * @param p_this: the vlc_object
 * @return VLC_SUCCESS if everything was fine
 */
static int Open( vlc_object_t* p_this )
{
    access_t*   p_access = (access_t*)p_this;
    access_sys_t* p_sys;
    char* psz_username = NULL;
    char* psz_password = NULL;
    int i_port;
    int i_ret;
    vlc_url_t url;

    if( !p_access->psz_path )
        return VLC_EGENERIC;

    STANDARD_BLOCK_ACCESS_INIT;

    /* Parse the URL */
    char* path = p_access->psz_path;
    vlc_UrlParse( &url, path, 0 );

    /* Check for some parameters */
    if( EMPTY_STR( url.psz_host ) )
    {
        msg_Err( p_access, "You might give a non empty host" );
        goto error;
    }

    /* If the user name is empty, ask the user */
    if( !EMPTY_STR( url.psz_username ) && url.psz_password )
    {
        psz_username = strdup( url.psz_username );
        psz_password = strdup( url.psz_password );
    }
    else
    {
        dialog_Login( p_access, &psz_username, &psz_password,
                      _("SFTP authentication"),
                      _("Please enter a valid login and password for the sftp "
                        "connexion to %s"), url.psz_host );
        if( EMPTY_STR(psz_username) || !psz_password )
            goto error;
    }

    if( url.i_port <= 0 )
        i_port = var_CreateGetInteger( p_access, "sftp-port" );
    else
        i_port = url.i_port;


    /* Connect to the server using a regular socket */
    p_sys->i_socket = net_Connect( p_access, url.psz_host, i_port, SOCK_STREAM, 0 );

    /* Create the ssh connexion and wait until the server answer */
    p_sys->ssh_session = libssh2_session_init();
    while( ( i_ret = libssh2_session_startup( p_sys->ssh_session,
                                              p_sys->i_socket ) )
           == LIBSSH2_ERROR_EAGAIN );

    if( i_ret != 0 )
    {
        msg_Err( p_access, "Impossible to open the connection to %s:%i", url.psz_host, i_port );
        goto error;
    }

    /* Ask for the fingerprint ... */
    // TODO: check it
    libssh2_session_set_blocking( p_sys->ssh_session, 1 );
    const char* fingerprint = libssh2_hostkey_hash( p_sys->ssh_session, LIBSSH2_HOSTKEY_HASH_MD5 );
    fprintf(stderr, "Fingerprint: ");
    for( int i = 0; i < 16; i++) {
        fprintf(stderr, "%02X ", (unsigned char)fingerprint[i]);
    }
    fprintf(stderr, "\n");

    //TODO: ask for the available auth methods

    /* send the login/password */
    if( libssh2_userauth_password( p_sys->ssh_session, psz_username, psz_password ) )
    {
        msg_Err( p_access, "Authentication by password failed" );
        goto error;
    }

    /* Create the sftp session */
    p_sys->sftp_session = libssh2_sftp_init( p_sys->ssh_session );

    if( !p_sys->sftp_session )
    {
        msg_Err( p_access, "Unable to initialize the SFTP session" );
        goto error;
    }

    /* Open the given file */
    p_sys->file = libssh2_sftp_open( p_sys->sftp_session, url.psz_path, LIBSSH2_FXF_READ, 0 );
    if( !p_sys->file )
    {
        msg_Err( p_access, "Unable to open the remote file %s", url.psz_path );
        goto error;
    }

    /* Get some informations */
    LIBSSH2_SFTP_ATTRIBUTES attributes;
    if( libssh2_sftp_stat( p_sys->sftp_session, url.psz_path, &attributes ) )
    {
        msg_Err( p_access, "Impossible to get informations about the remote file %s", url.psz_path );
        goto error;
    }
    p_access->info.i_size = attributes.filesize;

    /* Create the two variables */
    var_Create( p_access, "sftp-caching", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
    p_sys->i_read_size = var_CreateGetInteger( p_access, "sftp-readsize" );

    free( psz_password );
    free( psz_username );
    vlc_UrlClean( &url );
    return VLC_SUCCESS;

error:
    free( psz_password );
    free( psz_username );
    vlc_UrlClean( &url );
    free( p_sys );
    return VLC_EGENERIC;
}


/* Close: quit the module */
static void Close( vlc_object_t* p_this )
{
    access_t*   p_access = (access_t*)p_this;
    access_sys_t* p_sys = p_access->p_sys;

    libssh2_sftp_close_handle( p_sys->file );
    libssh2_sftp_shutdown( p_sys->sftp_session );

    libssh2_session_free( p_sys->ssh_session );
    free( p_sys );
}


static block_t* Block( access_t* p_access )
{
    if( p_access->info.b_eof )
        return NULL;

    /* Allocate the buffer we need */
    size_t i_len = __MIN( p_access->p_sys->i_read_size, p_access->info.i_size -
                                              p_access->info.i_pos );
    block_t* p_block = block_New( p_access, i_len );
    if( !p_block )
        return NULL;

    /* Read the specified size */
    ssize_t i_ret = libssh2_sftp_read( p_access->p_sys->file, (char*)p_block->p_buffer, i_len );

    if( i_ret < 0 )
    {
        block_Release( p_block );
        msg_Err( p_access, "read failed" );
        return NULL;
    }
    else if( i_ret == 0 )
    {
        p_access->info.b_eof = true;
        block_Release( p_block );
        return NULL;
    }
    else
    {
        p_access->info.i_pos += i_ret;
        return p_block;
    }
}


static int Seek( access_t* p_access, uint64_t i_pos )
{
    p_access->info.i_pos = i_pos;
    p_access->info.b_eof = false;

    libssh2_sftp_seek( p_access->p_sys->file, i_pos );
    return VLC_SUCCESS;
}


static int Control( access_t* p_access, int i_query, va_list args )
{
    bool*       pb_bool;
    int64_t*    pi_64;

    switch( i_query )
    {
    case ACCESS_CAN_SEEK:
        pb_bool = (bool*)va_arg( args, bool* );
        *pb_bool = true;
        break;

    case ACCESS_CAN_FASTSEEK:
        pb_bool = (bool*)va_arg( args, bool* );
        *pb_bool = false;
        break;

    case ACCESS_CAN_PAUSE:
    case ACCESS_CAN_CONTROL_PACE:
        pb_bool = (bool*)va_arg( args, bool* );
        *pb_bool = true;
        break;

    case ACCESS_GET_PTS_DELAY:
        pi_64 = (int64_t*)va_arg( args, int64_t* );
        *pi_64 = (int64_t)var_GetInteger( p_access, "sftp-caching" ) * INT64_C(1000);
        break;

    case ACCESS_SET_PAUSE_STATE:
        break;

    case ACCESS_GET_TITLE_INFO:
    case ACCESS_SET_TITLE:
    case ACCESS_SET_SEEKPOINT:
    case ACCESS_SET_PRIVATE_ID_STATE:
    case ACCESS_GET_META:
    case ACCESS_GET_PRIVATE_ID_STATE:
    case ACCESS_GET_CONTENT_TYPE:
        return VLC_EGENERIC;

    default:
        msg_Warn( p_access, "unimplemented query %d in control", i_query );
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

