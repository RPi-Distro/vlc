/*****************************************************************************
 * libvlc_media_list.h:  libvlc_media_list API
 *****************************************************************************
 * Copyright (C) 1998-2008 the VideoLAN team
 * $Id: d2f21515cd2cce6f46075c396cf16bef35a66374 $
 *
 * Authors: Pierre d'Herbemont
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

#ifndef LIBVLC_MEDIA_LIST_PLAYER_H
#define LIBVLC_MEDIA_LIST_PLAYER_H 1

/**
 * \file
 * This file defines libvlc_media_list_player API
 */

# ifdef __cplusplus
extern "C" {
# endif

/*****************************************************************************
 * Media List Player
 *****************************************************************************/
/** \defgroup libvlc_media_list_player libvlc_media_list_player
 * \ingroup libvlc
 * LibVLC Media List Player, play a media_list. You can see that as a media
 * instance subclass
 * @{
 */

typedef struct libvlc_media_list_player_t libvlc_media_list_player_t;

/**
 * Create new media_list_player.
 *
 * \param p_instance libvlc instance
 * \param p_e initialized exception instance
 * \return media list player instance
 */
VLC_PUBLIC_API libvlc_media_list_player_t *
    libvlc_media_list_player_new( libvlc_instance_t * p_instance,
                                  libvlc_exception_t * p_e );

/**
 * Release media_list_player.
 *
 * \param p_mlp media list player instance
 */
VLC_PUBLIC_API void
    libvlc_media_list_player_release( libvlc_media_list_player_t * p_mlp );

/**
 * Replace media player in media_list_player with this instance.
 *
 * \param p_mlp media list player instance
 * \param p_mi media player instance
 * \param p_e initialized exception instance
 */
VLC_PUBLIC_API void
    libvlc_media_list_player_set_media_player(
                                     libvlc_media_list_player_t * p_mlp,
                                     libvlc_media_player_t * p_mi,
                                     libvlc_exception_t * p_e );

VLC_PUBLIC_API void
    libvlc_media_list_player_set_media_list(
                                     libvlc_media_list_player_t * p_mlp,
                                     libvlc_media_list_t * p_mlist,
                                     libvlc_exception_t * p_e );

/**
 * Play media list
 *
 * \param p_mlp media list player instance
 * \param p_e initialized exception instance
 */
VLC_PUBLIC_API void
    libvlc_media_list_player_play( libvlc_media_list_player_t * p_mlp,
                                   libvlc_exception_t * p_e );

/**
 * Pause media list
 *
 * \param p_mlp media list player instance
 * \param p_e initialized exception instance
 */
VLC_PUBLIC_API void
    libvlc_media_list_player_pause( libvlc_media_list_player_t * p_mlp,
                                   libvlc_exception_t * p_e );

/**
 * Is media list playing?
 *
 * \param p_mlp media list player instance
 * \param p_e initialized exception instance
 * \return true for playing and false for not playing
 */
VLC_PUBLIC_API int
    libvlc_media_list_player_is_playing( libvlc_media_list_player_t * p_mlp,
                                         libvlc_exception_t * p_e );

/**
 * Get current libvlc_state of media list player
 *
 * \param p_mlp media list player instance
 * \param p_e initialized exception instance
 * \return libvlc_state_t for media list player
 */
VLC_PUBLIC_API libvlc_state_t
    libvlc_media_list_player_get_state( libvlc_media_list_player_t * p_mlp,
                                        libvlc_exception_t * p_e );

/**
 * Play media list item at position index
 *
 * \param p_mlp media list player instance
 * \param i_index index in media list to play
 * \param p_e initialized exception instance
 */
VLC_PUBLIC_API void
    libvlc_media_list_player_play_item_at_index(
                                   libvlc_media_list_player_t * p_mlp,
                                   int i_index,
                                   libvlc_exception_t * p_e );

VLC_PUBLIC_API void
    libvlc_media_list_player_play_item(
                                   libvlc_media_list_player_t * p_mlp,
                                   libvlc_media_t * p_md,
                                   libvlc_exception_t * p_e );

/**
 * Stop playing media list
 *
 * \param p_mlp media list player instance
 * \param p_e initialized exception instance
 */
VLC_PUBLIC_API void
    libvlc_media_list_player_stop( libvlc_media_list_player_t * p_mlp,
                                   libvlc_exception_t * p_e );

/**
 * Play next item from media list
 *
 * \param p_mlp media list player instance
 * \param p_e initialized exception instance
 */
VLC_PUBLIC_API void
    libvlc_media_list_player_next( libvlc_media_list_player_t * p_mlp,
                                   libvlc_exception_t * p_e );

/* NOTE: shouldn't there also be a libvlc_media_list_player_prev() */

/** @} media_list_player */

# ifdef __cplusplus
}
# endif

#endif /* LIBVLC_MEDIA_LIST_PLAYER_H */
