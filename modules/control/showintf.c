/*****************************************************************************
 * showintf.c: control the display of the interface in fullscreen mode
 *****************************************************************************
 * Copyright (C) 2004 the VideoLAN team
 * $Id: 2f1149b5f4db8361ccfa73a53e1eea326b752500 $
 *
 * Authors: Olivier Teuliere <ipkiss@via.ecp.fr>
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
#include <vlc_vout.h>
#include <vlc_playlist.h>

#ifdef HAVE_UNISTD_H
#    include <unistd.h>
#endif

/*****************************************************************************
 * intf_sys_t: description and status of interface
 *****************************************************************************/
struct intf_sys_t
{
    vlc_mutex_t   lock;
    vlc_object_t *p_vout;
    bool          b_button_pressed;
    bool          b_triggered;
    int           i_threshold;
};

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
int  Open ( vlc_object_t * );
void Close( vlc_object_t * );
static void RunIntf( intf_thread_t *p_intf );
static int  MouseEvent( vlc_object_t *, char const *,
                        vlc_value_t, vlc_value_t, void * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define THRESHOLD_TEXT N_( "Threshold" )
#define THRESHOLD_LONGTEXT N_( "Height of the zone triggering the interface." )

vlc_module_begin ()
    set_shortname( "Showintf" )
    set_description( N_("Show interface with mouse") )

    set_capability( "interface", 0 )
    set_callbacks( Open, Close )

    set_category( CAT_INTERFACE )
    set_subcategory( SUBCAT_INTERFACE_CONTROL )

    add_integer( "showintf-threshold", 10, NULL, THRESHOLD_TEXT, THRESHOLD_LONGTEXT, true )
vlc_module_end ()

/*****************************************************************************
 * Open: initialize interface
 *****************************************************************************/
int Open( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_this;

    /* Allocate instance and initialize some members */
    intf_sys_t *p_sys = p_intf->p_sys = malloc( sizeof( intf_sys_t ) );
    if( p_sys == NULL )
        return VLC_ENOMEM;

    vlc_mutex_init( &p_sys->lock );
    p_sys->p_vout = NULL;
    p_sys->b_button_pressed = false;
    p_sys->b_triggered = false;
    p_sys->i_threshold = config_GetInt( p_intf, "showintf-threshold" );

    p_intf->pf_run = RunIntf;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: destroy interface
 *****************************************************************************/
void Close( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_this;

    /* Destroy structure */
    vlc_mutex_destroy( &p_intf->p_sys->lock );
    free( p_intf->p_sys );
}


/*****************************************************************************
 * RunIntf: main loop
 *****************************************************************************/
static void RunIntf( intf_thread_t *p_intf )
{
    int canc = vlc_savecancel( );

    /* Main loop */
    while( vlc_object_alive( p_intf ) )
    {
        vlc_mutex_lock( &p_intf->p_sys->lock );

        /* Notify the interfaces */
        if( p_intf->p_sys->b_triggered )
        {
            var_SetBool( p_intf->p_libvlc, "intf-show", true );
            p_intf->p_sys->b_triggered = false;
        }

        vlc_mutex_unlock( &p_intf->p_sys->lock );


        /* Take care of the video output */
        if( p_intf->p_sys->p_vout && !vlc_object_alive (p_intf->p_sys->p_vout) )
        {
            var_DelCallback( p_intf->p_sys->p_vout, "mouse-moved",
                             MouseEvent, p_intf );
            var_DelCallback( p_intf->p_sys->p_vout, "mouse-button-down",
                             MouseEvent, p_intf );
            vlc_object_release( p_intf->p_sys->p_vout );
            p_intf->p_sys->p_vout = NULL;
        }

        if( p_intf->p_sys->p_vout == NULL )
        {
            p_intf->p_sys->p_vout = vlc_object_find( p_intf, VLC_OBJECT_VOUT,
                                                     FIND_ANYWHERE );
            if( p_intf->p_sys->p_vout )
            {
                var_AddCallback( p_intf->p_sys->p_vout, "mouse-moved",
                                 MouseEvent, p_intf );
                var_AddCallback( p_intf->p_sys->p_vout, "mouse-button-down",
                                 MouseEvent, p_intf );
            }
        }

        /* Wait a bit */
        msleep( INTF_IDLE_SLEEP );
    }

    if( p_intf->p_sys->p_vout )
    {
        var_DelCallback( p_intf->p_sys->p_vout, "mouse-moved",
                         MouseEvent, p_intf );
        var_DelCallback( p_intf->p_sys->p_vout, "mouse-button-down",
                         MouseEvent, p_intf );
        vlc_object_release( p_intf->p_sys->p_vout );
    }
    vlc_restorecancel( canc );
}

/*****************************************************************************
 * MouseEvent: callback for mouse events
 *****************************************************************************/
static int MouseEvent( vlc_object_t *p_this, char const *psz_var,
                       vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    VLC_UNUSED(p_this); VLC_UNUSED(oldval); VLC_UNUSED(newval);

    int i_mouse_x, i_mouse_y;
    intf_thread_t *p_intf = (intf_thread_t *)p_data;

    /* Do nothing when the interface is already requested */
    if( p_intf->p_sys->b_triggered )
        return VLC_SUCCESS;

    /* Nothing to do when not in fullscreen mode */
    if( !var_GetBool( p_intf->p_sys->p_vout, "fullscreen" ) )
        return VLC_SUCCESS;

    vlc_mutex_lock( &p_intf->p_sys->lock );
    if( !strcmp( psz_var, "mouse-moved" ) && !p_intf->p_sys->b_button_pressed )
    {
        i_mouse_x = var_GetInteger( p_intf->p_sys->p_vout, "mouse-x" );
        i_mouse_y = var_GetInteger( p_intf->p_sys->p_vout, "mouse-y" );

        /* Very basic test, we even ignore the x value :) */
        if ( i_mouse_y < p_intf->p_sys->i_threshold )
        {
            msg_Dbg( p_intf, "interface showing requested" );
            p_intf->p_sys->b_triggered = true;
        }
    }

    /* We keep track of the button state to avoid interferences with the
     * gestures plugin */
    if( !p_intf->p_sys->b_button_pressed &&
        !strcmp( psz_var, "mouse-button-down" ) )
    {
        p_intf->p_sys->b_button_pressed = true;
    }
    if( p_intf->p_sys->b_button_pressed &&
        !strcmp( psz_var, "mouse-button-down" ) )
    {
        p_intf->p_sys->b_button_pressed = false;
    }

    vlc_mutex_unlock( &p_intf->p_sys->lock );

    return VLC_SUCCESS;
}
