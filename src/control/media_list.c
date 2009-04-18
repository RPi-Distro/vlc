/*****************************************************************************
 * media_list.c: libvlc new API media list functions
 *****************************************************************************
 * Copyright (C) 2007 the VideoLAN team
 * $Id: 2eae4971ab4c644dad93b4b111326464378f3af6 $
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

typedef enum EventPlaceInTime {
    EventWillHappen,
    EventDidHappen
} EventPlaceInTime;

//#define DEBUG_MEDIA_LIST

#ifdef DEBUG_MEDIA_LIST
# define trace( fmt, ... ) printf( "%s(): " fmt, __FUNCTION__, ##__VA_ARGS__ )
#else
# define trace( ... )
#endif

/*
 * Private functions
 */



/**************************************************************************
 *       notify_item_addition (private)
 *
 * Do the appropriate action when an item is deleted.
 **************************************************************************/
static void
notify_item_addition( libvlc_media_list_t * p_mlist,
                      libvlc_media_t * p_md,
                      int index,
                      EventPlaceInTime event_status )
{
    libvlc_event_t event;

    /* Construct the event */
    if( event_status == EventDidHappen )
    {
        trace("item was added at index %d\n", index);
        event.type = libvlc_MediaListItemAdded;
        event.u.media_list_item_added.item = p_md;
        event.u.media_list_item_added.index = index;
    }
    else /* if( event_status == EventWillHappen ) */
    {
        event.type = libvlc_MediaListWillAddItem;
        event.u.media_list_will_add_item.item = p_md;
        event.u.media_list_will_add_item.index = index;
    }

    /* Send the event */
    libvlc_event_send( p_mlist->p_event_manager, &event );
}

/**************************************************************************
 *       notify_item_deletion (private)
 *
 * Do the appropriate action when an item is added.
 **************************************************************************/
static void
notify_item_deletion( libvlc_media_list_t * p_mlist,
                      libvlc_media_t * p_md,
                      int index,
                      EventPlaceInTime event_status )
{
    libvlc_event_t event;

    /* Construct the event */
    if( event_status == EventDidHappen )
    {
        trace("item at index %d was deleted\n", index);
        event.type = libvlc_MediaListItemDeleted;
        event.u.media_list_item_deleted.item = p_md;
        event.u.media_list_item_deleted.index = index;
    }
    else /* if( event_status == EventWillHappen ) */
    {
        event.type = libvlc_MediaListWillDeleteItem;
        event.u.media_list_will_delete_item.item = p_md;
        event.u.media_list_will_delete_item.index = index;
    }

    /* Send the event */
    libvlc_event_send( p_mlist->p_event_manager, &event );
}

/*
 * Public libvlc functions
 */

/**************************************************************************
 *       libvlc_media_list_new (Public)
 *
 * Init an object.
 **************************************************************************/
libvlc_media_list_t *
libvlc_media_list_new( libvlc_instance_t * p_inst,
                       libvlc_exception_t * p_e )
{
    libvlc_media_list_t * p_mlist;

    p_mlist = malloc(sizeof(libvlc_media_list_t));
    if( !p_mlist )
        return NULL;

    p_mlist->p_libvlc_instance = p_inst;
    p_mlist->p_event_manager = libvlc_event_manager_new( p_mlist, p_inst, p_e );

    /* Code for that one should be handled in flat_media_list.c */
    p_mlist->p_flat_mlist = NULL;
    p_mlist->b_read_only = false;

    libvlc_event_manager_register_event_type( p_mlist->p_event_manager,
            libvlc_MediaListItemAdded, p_e );
    libvlc_event_manager_register_event_type( p_mlist->p_event_manager,
            libvlc_MediaListWillAddItem, p_e );
    libvlc_event_manager_register_event_type( p_mlist->p_event_manager,
            libvlc_MediaListItemDeleted, p_e );
    libvlc_event_manager_register_event_type( p_mlist->p_event_manager,
            libvlc_MediaListWillDeleteItem, p_e );

    if( libvlc_exception_raised( p_e ) )
    {
        libvlc_event_manager_release( p_mlist->p_event_manager );
        free( p_mlist );
        return NULL;
    }

    vlc_mutex_init( &p_mlist->object_lock );

    vlc_array_init( &p_mlist->items );
    p_mlist->i_refcount = 1;
    p_mlist->p_md = NULL;

    return p_mlist;
}

/**************************************************************************
 *       libvlc_media_list_release (Public)
 *
 * Release an object.
 **************************************************************************/
void libvlc_media_list_release( libvlc_media_list_t * p_mlist )
{
    libvlc_media_t * p_md;
    int i;

    vlc_mutex_lock( &p_mlist->object_lock );
    p_mlist->i_refcount--;
    if( p_mlist->i_refcount > 0 )
    {
        vlc_mutex_unlock( &p_mlist->object_lock );
        return;
    }
    vlc_mutex_unlock( &p_mlist->object_lock );

    /* Refcount null, time to free */

    libvlc_event_manager_release( p_mlist->p_event_manager );

    if( p_mlist->p_md )
        libvlc_media_release( p_mlist->p_md );

    for ( i = 0; i < vlc_array_count( &p_mlist->items ); i++ )
    {
        p_md = vlc_array_item_at_index( &p_mlist->items, i );
        libvlc_media_release( p_md );
    }

    vlc_mutex_destroy( &p_mlist->object_lock );
    vlc_array_clear( &p_mlist->items );

    free( p_mlist );
}

/**************************************************************************
 *       libvlc_media_list_retain (Public)
 *
 * Increase an object refcount.
 **************************************************************************/
void libvlc_media_list_retain( libvlc_media_list_t * p_mlist )
{
    vlc_mutex_lock( &p_mlist->object_lock );
    p_mlist->i_refcount++;
    vlc_mutex_unlock( &p_mlist->object_lock );
}


/**************************************************************************
 *       add_file_content (Public)
 **************************************************************************/
void
libvlc_media_list_add_file_content( libvlc_media_list_t * p_mlist,
                                    const char * psz_uri,
                                    libvlc_exception_t * p_e )
{
    input_item_t * p_input_item;
    libvlc_media_t * p_md;

    p_input_item = input_item_NewExt( p_mlist->p_libvlc_instance->p_libvlc_int, psz_uri,
                                _("Media Library"), 0, NULL, -1 );

    if( !p_input_item )
    {
        libvlc_exception_raise( p_e, "Can't create an input item" );
        return;
    }

    p_md = libvlc_media_new_from_input_item(
            p_mlist->p_libvlc_instance,
            p_input_item, p_e );

    if( !p_md )
    {
        vlc_gc_decref( p_input_item );
        return;
    }

    libvlc_media_list_add_media( p_mlist, p_md, p_e );
    if( libvlc_exception_raised( p_e ) )
        return;

    input_Read( p_mlist->p_libvlc_instance->p_libvlc_int, p_input_item, true );

    return;
}

/**************************************************************************
 *       set_media (Public)
 **************************************************************************/
void libvlc_media_list_set_media( libvlc_media_list_t * p_mlist,
                                             libvlc_media_t * p_md,
                                             libvlc_exception_t * p_e)

{
    (void)p_e;
    vlc_mutex_lock( &p_mlist->object_lock );
    if( p_mlist->p_md )
        libvlc_media_release( p_mlist->p_md );
    libvlc_media_retain( p_md );
    p_mlist->p_md = p_md;
    vlc_mutex_unlock( &p_mlist->object_lock );
}

/**************************************************************************
 *       media (Public)
 *
 * If this media_list comes is a media's subitems,
 * This holds the corresponding media.
 * This md is also seen as the information holder for the media_list.
 * Indeed a media_list can have meta information through this
 * media.
 **************************************************************************/
libvlc_media_t *
libvlc_media_list_media( libvlc_media_list_t * p_mlist,
                                    libvlc_exception_t * p_e)
{
    libvlc_media_t *p_md;
    (void)p_e;

    vlc_mutex_lock( &p_mlist->object_lock );
    p_md = p_mlist->p_md;
    if( p_md )
        libvlc_media_retain( p_md );
    vlc_mutex_unlock( &p_mlist->object_lock );

    return p_md;
}

/**************************************************************************
 *       libvlc_media_list_count (Public)
 *
 * Lock should be hold when entering.
 **************************************************************************/
int libvlc_media_list_count( libvlc_media_list_t * p_mlist,
                             libvlc_exception_t * p_e )
{
    (void)p_e;
    return vlc_array_count( &p_mlist->items );
}

/**************************************************************************
 *       libvlc_media_list_add_media (Public)
 *
 * Lock should be hold when entering.
 **************************************************************************/
void libvlc_media_list_add_media(
                                   libvlc_media_list_t * p_mlist,
                                   libvlc_media_t * p_md,
                                   libvlc_exception_t * p_e )
{
    if( p_mlist->b_read_only )
    {
        /* We are read only from user side */
        libvlc_exception_raise( p_e, "Trying to write into a read-only media list." );
        return;
    }

    _libvlc_media_list_add_media( p_mlist, p_md, p_e );
}

/* LibVLC internal version */
void _libvlc_media_list_add_media(
                                   libvlc_media_list_t * p_mlist,
                                   libvlc_media_t * p_md,
                                   libvlc_exception_t * p_e )
{
    (void)p_e;
    libvlc_media_retain( p_md );

    notify_item_addition( p_mlist, p_md, vlc_array_count( &p_mlist->items ), EventWillHappen );
    vlc_array_append( &p_mlist->items, p_md );
    notify_item_addition( p_mlist, p_md, vlc_array_count( &p_mlist->items )-1, EventDidHappen );
}

/**************************************************************************
 *       libvlc_media_list_insert_media (Public)
 *
 * Lock should be hold when entering.
 **************************************************************************/
void libvlc_media_list_insert_media(
                                   libvlc_media_list_t * p_mlist,
                                   libvlc_media_t * p_md,
                                   int index,
                                   libvlc_exception_t * p_e )
{
    if( p_mlist->b_read_only )
    {
        /* We are read only from user side */
        libvlc_exception_raise( p_e, "Trying to write into a read-only media list." );
        return;
    }
    _libvlc_media_list_insert_media( p_mlist, p_md, index, p_e );
}

/* LibVLC internal version */
void _libvlc_media_list_insert_media(
                                   libvlc_media_list_t * p_mlist,
                                   libvlc_media_t * p_md,
                                   int index,
                                   libvlc_exception_t * p_e )
{
    (void)p_e;
    libvlc_media_retain( p_md );

    notify_item_addition( p_mlist, p_md, index, EventWillHappen );
    vlc_array_insert( &p_mlist->items, p_md, index );
    notify_item_addition( p_mlist, p_md, index, EventDidHappen );
}

/**************************************************************************
 *       libvlc_media_list_remove_index (Public)
 *
 * Lock should be hold when entering.
 **************************************************************************/
void libvlc_media_list_remove_index( libvlc_media_list_t * p_mlist,
                                     int index,
                                     libvlc_exception_t * p_e )
{
    if( p_mlist->b_read_only )
    {
        /* We are read only from user side */
        libvlc_exception_raise( p_e, "Trying to write into a read-only media list." );
        return;
    }
    _libvlc_media_list_remove_index( p_mlist, index, p_e );
}

/* LibVLC internal version */
void _libvlc_media_list_remove_index( libvlc_media_list_t * p_mlist,
                                     int index,
                                     libvlc_exception_t * p_e )
{

    libvlc_media_t * p_md;

    if( index < 0 || index >= vlc_array_count( &p_mlist->items ))
    {
        libvlc_exception_raise( p_e, "Index out of bounds exception");
        return;
    }

    p_md = vlc_array_item_at_index( &p_mlist->items, index );

    notify_item_deletion( p_mlist, p_md, index, EventWillHappen );
    vlc_array_remove( &p_mlist->items, index );
    notify_item_deletion( p_mlist, p_md, index, EventDidHappen );

    libvlc_media_release( p_md );
}

/**************************************************************************
 *       libvlc_media_list_item_at_index (Public)
 *
 * Lock should be hold when entering.
 **************************************************************************/
libvlc_media_t *
libvlc_media_list_item_at_index( libvlc_media_list_t * p_mlist,
                                 int index,
                                 libvlc_exception_t * p_e )
{
    VLC_UNUSED(p_e);

    if( index < 0 || index >= vlc_array_count( &p_mlist->items ))
    {
        libvlc_exception_raise( p_e, "Index out of bounds exception");
        return NULL;
    }

    libvlc_media_t * p_md;
    p_md = vlc_array_item_at_index( &p_mlist->items, index );
    libvlc_media_retain( p_md );
    return p_md;
}

/**************************************************************************
 *       libvlc_media_list_index_of_item (Public)
 *
 * Lock should be hold when entering.
 * Warning: this function would return the first matching item
 **************************************************************************/
int libvlc_media_list_index_of_item( libvlc_media_list_t * p_mlist,
                                     libvlc_media_t * p_searched_md,
                                     libvlc_exception_t * p_e )
{
    VLC_UNUSED(p_e);

    libvlc_media_t * p_md;
    int i;
    for ( i = 0; i < vlc_array_count( &p_mlist->items ); i++ )
    {
        p_md = vlc_array_item_at_index( &p_mlist->items, i );
        if( p_searched_md == p_md )
            return i;
    }
    return -1;
}

/**************************************************************************
 *       libvlc_media_list_is_readonly (Public)
 *
 * This indicates if this media list is read-only from a user point of view
 **************************************************************************/
int libvlc_media_list_is_readonly( libvlc_media_list_t * p_mlist )
{
    return p_mlist->b_read_only;
}

/**************************************************************************
 *       libvlc_media_list_lock (Public)
 *
 * The lock must be held in access operations. It is never used in the
 * Public method.
 **************************************************************************/
void libvlc_media_list_lock( libvlc_media_list_t * p_mlist )
{
    vlc_mutex_lock( &p_mlist->object_lock );
}


/**************************************************************************
 *       libvlc_media_list_unlock (Public)
 *
 * The lock must be held in access operations
 **************************************************************************/
void libvlc_media_list_unlock( libvlc_media_list_t * p_mlist )
{
    vlc_mutex_unlock( &p_mlist->object_lock );
}


/**************************************************************************
 *       libvlc_media_list_p_event_manager (Public)
 *
 * The p_event_manager is immutable, so you don't have to hold the lock
 **************************************************************************/
libvlc_event_manager_t *
libvlc_media_list_event_manager( libvlc_media_list_t * p_mlist,
                                    libvlc_exception_t * p_e )
{
    (void)p_e;
    return p_mlist->p_event_manager;
}
