/*****************************************************************************
 * hierarchical_node_media_list_view.c: libvlc hierarchical nodes media list
 * view functs.
 *****************************************************************************
 * Copyright (C) 2007 the VideoLAN team
 * $Id: 505ce8feddfa81191975da6c60bd0dfc38366a15 $
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

#include "libvlc_internal.h"
#include <vlc/libvlc.h>
#include <assert.h>
#include "vlc_arrays.h"

/* FIXME: This version is probably a bit overheaded, and we may want to store
 * the items in a vlc_array_t to speed everything up */

//#define DEBUG_HIERARCHICAL_VIEW

#ifdef DEBUG_HIERARCHICAL_VIEW
# define trace( fmt, ... ) printf( "[HIERARCHICAL_NODE] %s(): " fmt, __FUNCTION__, ##__VA_ARGS__ )
#else
# define trace( ... ) {}
#endif

struct libvlc_media_list_view_private_t
{
    vlc_array_t array;
};

/*
 * Private functions
 */

/**************************************************************************
 *       flat_media_list_view_count  (private)
 * (called by media_list_view_count)
 **************************************************************************/
static int
hierarch_node_media_list_view_count( libvlc_media_list_view_t * p_mlv,
                                libvlc_exception_t * p_e )
{
    /* FIXME: we may want to cache that */
    int i, ret, count = libvlc_media_list_count( p_mlv->p_mlist, p_e );
    libvlc_media_t * p_md;
    libvlc_media_list_t * p_submlist;
    ret = 0;
    trace("\n");
    for( i = 0; i < count; i++ )
    {
        p_md = libvlc_media_list_item_at_index( p_mlv->p_mlist, i, p_e );
        if( !p_md ) continue;
        p_submlist = libvlc_media_subitems( p_md, p_e );
        if( !p_submlist ) continue;
        libvlc_media_release( p_md );
        libvlc_media_list_release( p_submlist );
        ret++;
    }
    return ret;
}

/**************************************************************************
 *       flat_media_list_view_item_at_index  (private)
 * (called by flat_media_list_view_item_at_index)
 **************************************************************************/
static libvlc_media_t *
hierarch_node_media_list_view_item_at_index( libvlc_media_list_view_t * p_mlv,
                                        int index,
                                        libvlc_exception_t * p_e )
{
    /* FIXME: we may want to cache that */
    libvlc_media_t * p_md;
    libvlc_media_list_t * p_submlist;
    trace("%d\n", index);
    int i, current_index, count = libvlc_media_list_count( p_mlv->p_mlist, p_e );
    current_index = -1;
    for( i = 0; i < count; i++ )
    {
        p_md = libvlc_media_list_item_at_index( p_mlv->p_mlist, i, p_e );
        if( !p_md ) continue;
        p_submlist = libvlc_media_subitems( p_md, p_e );
        if( !p_submlist ) continue;
        libvlc_media_list_release( p_submlist );
        current_index++;
        if( current_index == index )
            return p_md;
        libvlc_media_release( p_md );
    }

    libvlc_exception_raise( p_e, "Index out of bound in Media List View" );
    return NULL;
}

/**************************************************************************
 *       flat_media_list_view_item_at_index  (private)
 * (called by flat_media_list_view_item_at_index)
 **************************************************************************/
static libvlc_media_list_view_t *
hierarch_node_media_list_view_children_at_index( libvlc_media_list_view_t * p_mlv,
                                            int index,
                                            libvlc_exception_t * p_e )
{
    libvlc_media_t * p_md;
    libvlc_media_list_t * p_submlist;
    libvlc_media_list_view_t * p_ret;
    p_md = hierarch_node_media_list_view_item_at_index( p_mlv, index, p_e );
    if( !p_md ) return NULL;
    p_submlist = libvlc_media_subitems( p_md, p_e );
    libvlc_media_release( p_md );
    if( !p_submlist ) return NULL;
    p_ret = libvlc_media_list_hierarchical_node_view( p_submlist, p_e );
    libvlc_media_list_release( p_submlist );

    return p_ret;
}

/* Helper */
static int
index_of_item( libvlc_media_list_view_t * p_mlv, libvlc_media_t * p_md )
{
    libvlc_media_t * p_iter_md;
    libvlc_media_list_t * p_submlist;

    int i, current_index, count = libvlc_media_list_count( p_mlv->p_mlist, NULL );
    current_index = -1;
    for( i = 0; i < count; i++ )
    {
        p_iter_md = libvlc_media_list_item_at_index( p_mlv->p_mlist, i, NULL );
        if( !p_iter_md ) continue;
        p_submlist = libvlc_media_subitems( p_iter_md, NULL );
        if( !p_submlist ) continue;
        libvlc_media_list_release( p_submlist );
        libvlc_media_release( p_iter_md );
        current_index++;
        if( p_md == p_iter_md )
            return current_index;
    }
    return -1;
}

static bool
item_is_already_added( libvlc_media_t * p_md )
{
    libvlc_media_list_t * p_submlist;

    p_submlist = libvlc_media_subitems( p_md, NULL );
    if( !p_submlist ) return false;
    int count = libvlc_media_list_count( p_submlist, NULL );
    libvlc_media_list_release( p_submlist );
    return count > 1;
}


/**************************************************************************
 *       media_list_(item|will)_* (private) (Event callback)
 **************************************************************************/
static void
items_subitems_added( const libvlc_event_t * p_event, void * user_data )
{
    libvlc_media_t * p_md;
    libvlc_media_list_view_t * p_mlv = user_data;
    int index;
    p_md = p_event->p_obj;
    if( !item_is_already_added( p_md ) )
    {
        index = index_of_item( p_mlv, p_md );
        trace("%d\n", index);
        if( index >= 0 )
        {
            libvlc_media_list_view_will_add_item( p_mlv, p_md, index );
            libvlc_media_list_view_item_added( p_mlv, p_md, index );
        }
    }
    else
    {
        trace("item already added\n");
    }
}

static void
media_list_item_added( const libvlc_event_t * p_event, void * user_data )
{
    libvlc_media_t * p_md;
    libvlc_media_list_view_t * p_mlv = user_data;
    int index;
    p_md = p_event->u.media_list_item_added.item;
    index = index_of_item( p_mlv, p_md );
    trace("%d\n", index);
    if( index >= 0)
        libvlc_media_list_view_item_added( p_mlv, p_md, index );
    libvlc_event_attach( p_md->p_event_manager, libvlc_MediaSubItemAdded,
                         items_subitems_added, p_mlv, NULL );
                         
}
static void
media_list_will_add_item( const libvlc_event_t * p_event, void * user_data )
{
    libvlc_media_t * p_md;
    libvlc_media_list_view_t * p_mlv = user_data;
    int index;
    p_md = p_event->u.media_list_will_add_item.item;
    index = index_of_item( p_mlv, p_md );
    trace("%d\n", index);
    if( index >= 0)
        libvlc_media_list_view_will_add_item( p_mlv, p_md, index );
}
static void
media_list_item_deleted( const libvlc_event_t * p_event, void * user_data )
{
    libvlc_media_t * p_md;
    libvlc_media_list_view_t * p_mlv = user_data;
    int index;
    p_md = p_event->u.media_list_item_deleted.item;
    index = index_of_item( p_mlv, p_md );
    trace("%d\n", index);
    if( index >= 0)
        libvlc_media_list_view_item_deleted( p_mlv, p_md, index );
    libvlc_event_detach( p_md->p_event_manager, libvlc_MediaSubItemAdded,
                         items_subitems_added, p_mlv, NULL );
}
static void
media_list_will_delete_item( const libvlc_event_t * p_event, void * user_data )
{
    libvlc_media_t * p_md;
    libvlc_media_list_view_t * p_mlv = user_data;
    int index;
    p_md = p_event->u.media_list_will_delete_item.item;
    index = index_of_item( p_mlv, p_md );
    trace("%d\n", index);
    if( index >= 0)
        libvlc_media_list_view_will_delete_item( p_mlv, p_md, index );
}


/*
 * Public libvlc functions
 */


/**************************************************************************
 *       flat_media_list_view_release (private)
 * (called by media_list_view_release)
 **************************************************************************/
static void
hierarch_node_media_list_view_release( libvlc_media_list_view_t * p_mlv )
{
    trace("\n");
    libvlc_event_detach( p_mlv->p_mlist->p_event_manager,
                         libvlc_MediaListItemAdded,
                         media_list_item_added, p_mlv, NULL );
    libvlc_event_detach( p_mlv->p_mlist->p_event_manager,
                         libvlc_MediaListWillAddItem,
                         media_list_will_add_item, p_mlv, NULL );
    libvlc_event_detach( p_mlv->p_mlist->p_event_manager,
                         libvlc_MediaListItemDeleted,
                         media_list_item_deleted, p_mlv, NULL );
    libvlc_event_detach( p_mlv->p_mlist->p_event_manager,
                         libvlc_MediaListWillDeleteItem,
                         media_list_will_delete_item, p_mlv, NULL );
}

/**************************************************************************
 *       libvlc_media_list_flat_view (Public)
 **************************************************************************/
libvlc_media_list_view_t *
libvlc_media_list_hierarchical_node_view( libvlc_media_list_t * p_mlist,
                                     libvlc_exception_t * p_e )
{
    trace("\n");
    libvlc_media_list_view_t * p_mlv;
    p_mlv = libvlc_media_list_view_new( p_mlist,
                                        hierarch_node_media_list_view_count,
                                        hierarch_node_media_list_view_item_at_index,
                                        hierarch_node_media_list_view_children_at_index,
                                        libvlc_media_list_hierarchical_node_view,
                                        hierarch_node_media_list_view_release,
                                        NULL,
                                        p_e );
    libvlc_media_list_lock( p_mlist );
    libvlc_event_attach( p_mlv->p_mlist->p_event_manager,
                         libvlc_MediaListItemAdded,
                         media_list_item_added, p_mlv, NULL );
    libvlc_event_attach( p_mlv->p_mlist->p_event_manager,
                         libvlc_MediaListWillAddItem,
                         media_list_will_add_item, p_mlv, NULL );
    libvlc_event_attach( p_mlv->p_mlist->p_event_manager,
                         libvlc_MediaListItemDeleted,
                         media_list_item_deleted, p_mlv, NULL );
    libvlc_event_attach( p_mlv->p_mlist->p_event_manager,
                         libvlc_MediaListWillDeleteItem,
                         media_list_will_delete_item, p_mlv, NULL );
    libvlc_media_list_unlock( p_mlist );
    return p_mlv;
}
