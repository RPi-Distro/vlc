/*****************************************************************************
 * playlist_internal.h : Playlist internals
 *****************************************************************************
 * Copyright (C) 1999-2008 the VideoLAN team
 * $Id: 959b8e3d9f1bf5cbd52e37be480fb131f187dd98 $
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
 *          Clément Stenac <zorglub@videolan.org>
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

#ifndef __LIBVLC_PLAYLIST_INTERNAL_H
# define __LIBVLC_PLAYLIST_INTERNAL_H 1

/**
 *  \file
 *  This file contain internal structures and function prototypes related
 *  to the playlist in vlc
 *
 * \defgroup vlc_playlist Playlist
 * @{
 */

#include "input/input_interface.h"
#include <assert.h>

#include "art.h"
#include "fetcher.h"
#include "preparser.h"

typedef struct vlc_sd_internal_t vlc_sd_internal_t;

typedef struct playlist_private_t
{
    playlist_t           public_data;
    playlist_preparser_t *p_preparser;  /**< Preparser data */
    playlist_fetcher_t   *p_fetcher;    /**< Meta and art fetcher data */

    playlist_item_array_t items_to_delete; /**< Array of items and nodes to
            delete... At the very end. This sucks. */

    vlc_sd_internal_t   **pp_sds;
    int                   i_sds;   /**< Number of service discovery modules */
    input_thread_t *      p_input;  /**< the input thread associated
                                     * with the current item */
    input_resource_t *   p_input_resource; /**< input resources */
    struct {
        /* Current status. These fields are readonly, only the playlist
         * main loop can touch it*/
        playlist_status_t   i_status;  /**< Current status of playlist */
        playlist_item_t *   p_item; /**< Currently playing/active item */
        playlist_item_t *   p_node; /**< Current node to play from */
    } status;

    struct {
        /* Request. Use this to give orders to the playlist main loop  */
        playlist_status_t   i_status; /**< requested playlist status */
        playlist_item_t *   p_node;   /**< requested node to play from */
        playlist_item_t *   p_item;   /**< requested item to play in the node */

        int                 i_skip;   /**< Number of items to skip */

        bool          b_request;/**< Set to true by the requester
                                           The playlist sets it back to false
                                           when processing the request */
        vlc_mutex_t         lock;     /**< Lock to protect request */
    } request;

    vlc_thread_t thread; /**< engine thread */
    vlc_mutex_t lock; /**< dah big playlist global lock */
    vlc_cond_t signal; /**< wakes up the playlist engine thread */

    int      i_last_playlist_id; /**< Last id to an item */
    bool     b_reset_currently_playing; /** Reset current item array */

    bool     b_tree; /**< Display as a tree */
    bool     b_doing_ml; /**< Doing media library stuff  get quicker */
    bool     b_auto_preparse;
    mtime_t  last_rebuild_date;

} playlist_private_t;

#define pl_priv( pl ) ((playlist_private_t *)(pl))

/*****************************************************************************
 * Prototypes
 *****************************************************************************/

/* Creation/Deletion */
playlist_t *playlist_Create( vlc_object_t * );
void playlist_Destroy( playlist_t * );

/* */
void playlist_Activate( playlist_t * );
void playlist_Deactivate( playlist_t * );
void pl_Deactivate (libvlc_int_t *);

/* */
playlist_item_t *playlist_ItemNewFromInput( playlist_t *p_playlist,
                                            input_item_t *p_input );

/* Engine */
playlist_item_t * get_current_status_item( playlist_t * p_playlist);
playlist_item_t * get_current_status_node( playlist_t * p_playlist );
void set_current_status_item( playlist_t *, playlist_item_t * );
void set_current_status_node( playlist_t *, playlist_item_t * );

/* Load/Save */
int playlist_MLLoad( playlist_t *p_playlist );
int playlist_MLDump( playlist_t *p_playlist );

/**********************************************************************
 * Item management
 **********************************************************************/

void playlist_SendAddNotify( playlist_t *p_playlist, int i_item_id,
                             int i_node_id, bool b_signal );

playlist_item_t * playlist_NodeAddInput( playlist_t *, input_item_t *,
        playlist_item_t *,int , int, bool );

playlist_item_t * playlist_InsertInputItemTree ( playlist_t *,
        playlist_item_t *, input_item_node_t *, int, bool );

/* Tree walking */
playlist_item_t *playlist_ItemFindFromInputAndRoot( playlist_t *p_playlist,
                                input_item_t *p_input, playlist_item_t *p_root,
                                bool );

int playlist_DeleteFromInputInParent( playlist_t *, input_item_t *,
                                      playlist_item_t *, bool );
int playlist_DeleteFromItemId( playlist_t*, int );
int playlist_ItemRelease( playlist_item_t * );

int playlist_NodeEmpty( playlist_t *, playlist_item_t *, bool );
int playlist_DeleteItem( playlist_t * p_playlist, playlist_item_t *, bool);


/**
 * @}
 */

#define PLAYLIST_DEBUG 1
//#undef PLAYLIST_DEBUG2

#ifdef PLAYLIST_DEBUG
 #define PL_DEBUG( msg, args... ) msg_Dbg( p_playlist, msg, ## args )
 #ifdef PLAYLIST_DEBUG2
  #define PL_DEBUG2( msg, args... ) msg_Dbg( p_playlist, msg, ## args )
 #else
  #define PL_DEBUG2( msg, args... ) {}
 #endif
#else
 #define PL_DEBUG( msg, args ... ) {}
 #define PL_DEBUG2( msg, args... ) {}
#endif

#define PLI_NAME( p ) p && p->p_input ? p->p_input->psz_name : "null"

#define PL_LOCK_IF( cond ) pl_lock_if( p_playlist, cond )
static inline void pl_lock_if( playlist_t * p_playlist, bool cond )
{
    if( cond ) PL_LOCK; else PL_ASSERT_LOCKED;
}

#define PL_UNLOCK_IF( cond ) pl_unlock_if( p_playlist, cond )
static inline void pl_unlock_if( playlist_t * p_playlist, bool cond )
{
    if( cond ) PL_UNLOCK;
}

#endif /* !__LIBVLC_PLAYLIST_INTERNAL_H */
