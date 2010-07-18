/*****************************************************************************
 * init.c: Core functions : init, playlist, stream management
 *****************************************************************************
 * Copyright (C) 2005 the VideoLAN team
 * $Id: 6fdf33768a28ba4958ebe8a79885ef325005bd1f $
 *
 * Authors: Olivier Aubert <olivier.aubert@liris.univ-lyon1.fr>
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

#define __VLC__
#include <mediacontrol_internal.h>
#include <vlc/mediacontrol.h>

mediacontrol_Instance* mediacontrol_new( char** args, mediacontrol_Exception *exception )
{
    mediacontrol_Instance* retval;
    vlc_object_t *p_vlc;
    int p_vlc_id;
    char **ppsz_argv;
    int i_count = 0;
    int i_index;
    char **p_tmp;

    if( args )
    {
        for ( p_tmp = args ; *p_tmp != NULL ; p_tmp++ )
            i_count++;
    }

    ppsz_argv = malloc( ( i_count + 2 ) * sizeof( char * ) ) ;
    if( ! ppsz_argv )
    {
        exception->code = mediacontrol_InternalException;
        exception->message = "out of memory";
        return NULL;
    }
    ppsz_argv[0] = "vlc";
    for ( i_index = 0; i_index < i_count; i_index++ )
    {
        ppsz_argv[i_index + 1] = strdup( args[i_index] );
        if( ! ppsz_argv[i_index + 1] )
        {
            exception->code = mediacontrol_InternalException;
            exception->message = "out of memory";
            return NULL;
        }
    }

    ppsz_argv[i_count + 1] = NULL;

    p_vlc_id = VLC_Create();
    if( p_vlc_id < 0 )
    {
        exception->code = mediacontrol_InternalException;
        exception->message = strdup( "unable to create VLC" );
        return NULL;
    }

    p_vlc = ( vlc_object_t* )vlc_current_object( p_vlc_id );
    if( ! p_vlc )
    {
        exception->code = mediacontrol_InternalException;
        exception->message = strdup( "unable to find VLC object" );
        return NULL;
    }
    retval = ( mediacontrol_Instance* )malloc( sizeof( mediacontrol_Instance ) );
    if( ! retval )
    {
        exception->code = mediacontrol_InternalException;
        exception->message = strdup( "out of memory" );
        return NULL;
    }

    if( VLC_Init( p_vlc_id, i_count + 1, ppsz_argv ) != VLC_SUCCESS )
    {
        exception->code = mediacontrol_InternalException;
        exception->message = strdup( "cannot initialize VLC" );
        return NULL;
    }

    retval->p_vlc = p_vlc;
    retval->vlc_object_id = p_vlc_id;

    /* We can keep references on these, which should not change. Is it true ? */
    retval->p_playlist = vlc_object_find( p_vlc,
                                         VLC_OBJECT_PLAYLIST, FIND_ANYWHERE );
    retval->p_intf = vlc_object_find( p_vlc, VLC_OBJECT_INTF, FIND_ANYWHERE );

    if( ! retval->p_playlist || ! retval->p_intf )
    {
        exception->code = mediacontrol_InternalException;
        exception->message = strdup( "no interface available" );
        return NULL;
    }

    return retval;
}

void
mediacontrol_exit( mediacontrol_Instance *self )
{
    vlc_object_release( (vlc_object_t* )self->p_playlist );
    vlc_object_release( (vlc_object_t* )self->p_intf );
    vlc_object_release( (vlc_object_t*)self->p_vlc );

    VLC_CleanUp( self->vlc_object_id );
    VLC_Destroy( self->vlc_object_id );
}
