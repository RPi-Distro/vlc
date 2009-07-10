/*****************************************************************************
 * tree.c: libvlc tags tree functions
 * Create a tree of the 'tags' of a media_list's medias.
 *****************************************************************************
 * Copyright (C) 2007 the VideoLAN team
 * $Id: fd68429940d60455666381ef9ec51dbbf1545f47 $
 *
 * Authors: Pierre d'Herbemont <pdherbemont # videolan.org>
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
# include <config.h>
#endif

#include <vlc/libvlc.h>
#include <vlc/libvlc_media.h>
#include <vlc/libvlc_media_list.h>
#include <vlc/libvlc_media_library.h>
#include <vlc/libvlc_events.h>

#include <vlc_common.h>

#include "libvlc_internal.h"

struct libvlc_media_library_t
{
    libvlc_event_manager_t * p_event_manager;
    libvlc_instance_t *      p_libvlc_instance;
    int                      i_refcount;
    libvlc_media_list_t *    p_mlist;
};


/*
 * Private functions
 */

/*
 * Public libvlc functions
 */

/**************************************************************************
 *       new (Public)
 **************************************************************************/
libvlc_media_library_t *
libvlc_media_library_new( libvlc_instance_t * p_inst,
                          libvlc_exception_t * p_e )
{
    (void)p_e;
    libvlc_media_library_t * p_mlib;

    p_mlib = malloc(sizeof(libvlc_media_library_t));

    if( !p_mlib )
        return NULL;

    p_mlib->p_libvlc_instance = p_inst;
    p_mlib->i_refcount = 1;
    p_mlib->p_mlist = NULL;

    p_mlib->p_event_manager = libvlc_event_manager_new( p_mlib, p_inst, p_e );

    return p_mlib;
}

/**************************************************************************
 *       release (Public)
 **************************************************************************/
void libvlc_media_library_release( libvlc_media_library_t * p_mlib )
{
    p_mlib->i_refcount--;

    if( p_mlib->i_refcount > 0 )
        return;

    libvlc_event_manager_release( p_mlib->p_event_manager );
    free( p_mlib );
}

/**************************************************************************
 *       retain (Public)
 **************************************************************************/
void libvlc_media_library_retain( libvlc_media_library_t * p_mlib )
{
    p_mlib->i_refcount++;
}

/**************************************************************************
 *       load (Public)
 *
 * It doesn't yet load the playlists
 **************************************************************************/
void
libvlc_media_library_load( libvlc_media_library_t * p_mlib,
                           libvlc_exception_t * p_e )
{
    char *psz_datadir = config_GetUserDataDir();
    char * psz_uri;

    if( !psz_datadir ) /* XXX: i doubt that this can ever happen */
    {
        libvlc_exception_raise( p_e, "Can't get data directory" );
        return;
    }

    if( asprintf( &psz_uri, "file/xspf-open://%s" DIR_SEP "ml.xsp",
                  psz_datadir ) == -1 )
    {
        free( psz_datadir );
        libvlc_exception_raise( p_e, "Can't get create the path" );
        return;
    }
    free( psz_datadir );
    if( p_mlib->p_mlist )
        libvlc_media_list_release( p_mlib->p_mlist );

    p_mlib->p_mlist = libvlc_media_list_new(
                        p_mlib->p_libvlc_instance,
                        p_e );

    libvlc_media_list_add_file_content( p_mlib->p_mlist, psz_uri, p_e );
    free( psz_uri );
    return;
}

/**************************************************************************
 *       save (Public)
 **************************************************************************/
void
libvlc_media_library_save( libvlc_media_library_t * p_mlib,
                           libvlc_exception_t * p_e )
{
    (void)p_mlib;
    libvlc_exception_raise( p_e, "Not supported" );
}

/**************************************************************************
 *        media_list (Public)
 **************************************************************************/
libvlc_media_list_t *
libvlc_media_library_media_list( libvlc_media_library_t * p_mlib,
                                     libvlc_exception_t * p_e )
{
    (void)p_e;
    if( p_mlib->p_mlist )
        libvlc_media_list_retain( p_mlib->p_mlist );
    return p_mlib->p_mlist;
}
