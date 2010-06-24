/*****************************************************************************
 * lirc.c : lirc module for vlc
 *****************************************************************************
 * Copyright (C) 2003-2005 the VideoLAN team
 * $Id: 5688c97ab106c2e64038f3d05faf42d113b62ff3 $
 *
 * Author: Sigmund Augdal Helberg <dnumgis@videolan.org>
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

#include <fcntl.h>

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_interface.h>
#include <vlc_osd.h>
#include <vlc_keys.h>

#ifdef HAVE_POLL
# include <poll.h>
#endif

#include <lirc/lirc_client.h>

#define LIRC_TEXT N_("Change the lirc configuration file")
#define LIRC_LONGTEXT N_( \
    "Tell lirc to read this configuration file. By default it " \
    "searches in the users home directory." )

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open    ( vlc_object_t * );
static void Close   ( vlc_object_t * );

vlc_module_begin ()
    set_shortname( N_("Infrared") )
    set_category( CAT_INTERFACE )
    set_subcategory( SUBCAT_INTERFACE_CONTROL )
    set_description( N_("Infrared remote control interface") )
    set_capability( "interface", 0 )
    set_callbacks( Open, Close )

    add_string( "lirc-file", NULL, NULL,
                LIRC_TEXT, LIRC_LONGTEXT, true )
vlc_module_end ()

/*****************************************************************************
 * intf_sys_t: description and status of FB interface
 *****************************************************************************/
struct intf_sys_t
{
    struct lirc_config *config;

    int i_fd;
};

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static void Run( intf_thread_t * );

static void Process( intf_thread_t * );

/*****************************************************************************
 * Open: initialize interface
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_this;
    intf_sys_t *p_sys;
    char *psz_file;

    /* Allocate instance and initialize some members */
    p_intf->p_sys = p_sys = malloc( sizeof( intf_sys_t ) );
    if( p_sys == NULL )
        return VLC_ENOMEM;

    p_intf->pf_run = Run;

    p_sys->i_fd = lirc_init( "vlc", 1 );
    if( p_sys->i_fd == -1 )
    {
        msg_Err( p_intf, "lirc initialisation failed" );
        goto exit;
    }

    /* We want polling */
    fcntl( p_sys->i_fd, F_SETFL, fcntl( p_sys->i_fd, F_GETFL ) | O_NONBLOCK );

    /* Read the configuration file */
    psz_file = var_CreateGetNonEmptyString( p_intf, "lirc-file" );
    if( lirc_readconfig( psz_file, &p_sys->config, NULL ) != 0 )
    {
        msg_Err( p_intf, "failure while reading lirc config" );
        free( psz_file );
        goto exit;
    }
    free( psz_file );

    return VLC_SUCCESS;

exit:
    if( p_sys->i_fd != -1 )
        lirc_deinit();
    free( p_sys );
    return VLC_EGENERIC;
}

/*****************************************************************************
 * Close: destroy interface
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_this;
    intf_sys_t *p_sys = p_intf->p_sys;

    /* Destroy structure */
    lirc_freeconfig( p_sys->config );
    lirc_deinit();
    free( p_sys );
}

/*****************************************************************************
 * Run: main loop
 *****************************************************************************/
static void Run( intf_thread_t *p_intf )
{
    intf_sys_t *p_sys = p_intf->p_sys;

    for( ;; )
    {
        /* Wait for data */
        struct pollfd ufd = { .fd = p_sys->i_fd, .events = POLLIN, .revents = 0 };
        if( poll( &ufd, 1, -1 ) == -1 )
            break;

        /* Process */
        int canc = vlc_savecancel();
        Process( p_intf );
        vlc_restorecancel(canc);
    }
}

static void Process( intf_thread_t *p_intf )
{
    for( ;; )
    {
        char *code, *c;
        if( lirc_nextcode( &code ) )
            return;

        if( code == NULL )
            return;

        while( vlc_object_alive( p_intf )
                && (lirc_code2char( p_intf->p_sys->config, code, &c ) == 0)
                && (c != NULL) )
        {
            if( !strncmp( "key-", c, 4 ) )
            {
                vlc_key_t i_key = vlc_GetActionId( c );
                if( i_key )
                    var_SetInteger( p_intf->p_libvlc, "key-action", i_key );
                else
                    msg_Err( p_intf, "Unknown hotkey '%s'", c );
            }
            else if( !strncmp( "menu ", c, 5)  )
            {
                if( !strncmp( c, "menu on", 7 ) ||
                    !strncmp( c, "menu show", 9 ))
                    osd_MenuShow( VLC_OBJECT(p_intf) );
                else if( !strncmp( c, "menu off", 8 ) ||
                         !strncmp( c, "menu hide", 9 ) )
                    osd_MenuHide( VLC_OBJECT(p_intf) );
                else if( !strncmp( c, "menu up", 7 ) )
                    osd_MenuUp( VLC_OBJECT(p_intf) );
                else if( !strncmp( c, "menu down", 9 ) )
                    osd_MenuDown( VLC_OBJECT(p_intf) );
                else if( !strncmp( c, "menu left", 9 ) )
                    osd_MenuPrev( VLC_OBJECT(p_intf) );
                else if( !strncmp( c, "menu right", 10 ) )
                    osd_MenuNext( VLC_OBJECT(p_intf) );
                else if( !strncmp( c, "menu select", 11 ) )
                    osd_MenuActivate( VLC_OBJECT(p_intf) );
                else
                {
                    msg_Err( p_intf, "Please provide one of the following parameters:" );
                    msg_Err( p_intf, "[on|off|up|down|left|right|select]" );
                    break;
                }
            }
            else
            {
                msg_Err( p_intf, "this doesn't appear to be a valid keycombo "
                                 "lirc sent us. Please look at the "
                                 "doc/lirc/example.lirc file in VLC" );
                break;
            }
        }
        free( code );
    }
}
