/*****************************************************************************
 * deprecated.h:  libvlc deprecated API
 *****************************************************************************
 * Copyright (C) 1998-2008 the VideoLAN team
 * $Id: 2cb82e11034c42b79115efd043f41fdfd06aa1eb $
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
 *          Jean-Paul Saman <jpsaman@videolan.org>
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

#ifndef LIBVLC_DEPRECATED_H
#define LIBVLC_DEPRECATED_H 1

/**
 * \file
 * This file defines libvlc depreceated API
 */

/**
 * This is the legacy representation of a platform-specific drawable. Because
 * it cannot accomodate a pointer on most 64-bits platforms, it should not be
 * used anymore.
 */
typedef int libvlc_drawable_t;

# ifdef __cplusplus
extern "C" {
# endif

/**
 * Set the drawable where the media player should render its video output.
 *
 * On Windows 32-bits, a window handle (HWND) is expected.
 * On Windows 64-bits, this function will always fail.
 *
 * On OSX 32-bits, a CGrafPort is expected.
 * On OSX 64-bits, this function will always fail.
 *
 * On other platforms, an existing X11 window ID is expected. See
 * libvlc_media_player_set_xid() for details.
 *
 * \param p_mi the Media Player
 * \param drawable the libvlc_drawable_t where the media player
 *        should render its video
 * \param p_e an initialized exception pointer
 */
VLC_DEPRECATED_API void libvlc_media_player_set_drawable ( libvlc_media_player_t *, libvlc_drawable_t, libvlc_exception_t * );

/**
 * Get the drawable where the media player should render its video output
 *
 * \param p_mi the Media Player
 * \param p_e an initialized exception pointer
 * \return the libvlc_drawable_t where the media player
 *         should render its video
 */
VLC_DEPRECATED_API libvlc_drawable_t
                    libvlc_media_player_get_drawable ( libvlc_media_player_t *, libvlc_exception_t * );

/**
 * Set the default video output's parent.
 *
 * This setting will be used as default for any video output.
 *
 * \param p_instance libvlc instance
 * \param drawable the new parent window
 *                 (see libvlc_media_player_set_drawable() for details)
 * \param p_e an initialized exception pointer
 * @deprecated Use libvlc_media_player_set_drawable
 */
VLC_DEPRECATED_API void libvlc_video_set_parent( libvlc_instance_t *, libvlc_drawable_t, libvlc_exception_t * );

/**
 * Set the default video output parent.
 *
 * This setting will be used as default for all video outputs.
 *
 * \param p_instance libvlc instance
 * \param drawable the new parent window (Drawable on X11, CGrafPort on MacOSX, HWND on Win32)
 * \param p_e an initialized exception pointer
 * @deprecated Use libvlc_media_player_get_drawable
 */
VLC_DEPRECATED_API libvlc_drawable_t libvlc_video_get_parent( libvlc_instance_t *, libvlc_exception_t * );

/**
 * Does nothing. Do not use this function.
 */
VLC_DEPRECATED_API int libvlc_video_reparent( libvlc_media_player_t *, libvlc_drawable_t, libvlc_exception_t * );

/**
 * Resize the current video output window.
 * This might crash. Please use libvlc_video_set_scale() instead.
 *
 * \param p_mi media player instance
 * \param width new width for video output window
 * \param height new height for video output window
 * \param p_e an initialized exception pointer
 * \return the success status (boolean)
 */
VLC_DEPRECATED_API void libvlc_video_resize( libvlc_media_player_t *, int, int, libvlc_exception_t *);

/**
 * Tell windowless video output to redraw rectangular area (MacOS X only).
 * This might crash. Do not use this function.
 *
 * \param p_mi media player instance
 * \param area coordinates within video drawable
 * \param p_e an initialized exception pointer
 */
VLC_DEPRECATED_API void libvlc_video_redraw_rectangle( libvlc_media_player_t *, const libvlc_rectangle_t *, libvlc_exception_t * );

/**
 * Set the default video output size.
 * This setting will be used as default for all video outputs.
 *
 * \param p_instance libvlc instance
 * \param width new width for video drawable
 * \param height new height for video drawable
 * \param p_e an initialized exception pointer
 */
VLC_DEPRECATED_API void libvlc_video_set_size( libvlc_instance_t *, int, int, libvlc_exception_t * );

/**
 * Set the default video output viewport for a windowless video output
 * (MacOS X only). This might crash. Do not use this function.
 *
 * This setting will be used as default for all video outputs.
 *
 * \param p_instance libvlc instance
 * \param p_mi media player instance
 * \param view coordinates within video drawable
 * \param clip coordinates within video drawable
 * \param p_e an initialized exception pointer
 */
VLC_DEPRECATED_API void libvlc_video_set_viewport( libvlc_instance_t *, libvlc_media_player_t *, const libvlc_rectangle_t *, const libvlc_rectangle_t *, libvlc_exception_t * );

/*
 * This function shall not be used at all. It may lead to crash and race condition.
 */
VLC_DEPRECATED_API int libvlc_video_destroy( libvlc_media_player_t *, libvlc_exception_t *);

/*****************************************************************************
 * Playlist (Deprecated)
 *****************************************************************************/
/** \defgroup libvlc_playlist libvlc_playlist (Deprecated)
 * \ingroup libvlc
 * LibVLC Playlist handling (Deprecated)
 * @deprecated Use media_list
 * @{
 */

/**
 * Set the playlist's loop attribute. If set, the playlist runs continuously
 * and wraps around when it reaches the end.
 *
 * \param p_instance the playlist instance
 * \param loop the loop attribute. 1 sets looping, 0 disables it
 * \param p_e an initialized exception pointer
 */
VLC_DEPRECATED_API void libvlc_playlist_loop( libvlc_instance_t* , int,
                                          libvlc_exception_t * );

/**
 * Start playing.
 *
 * Additionnal playlist item options can be specified for addition to the
 * item before it is played.
 *
 * \param p_instance the playlist instance
 * \param i_id the item to play. If this is a negative number, the next
 *        item will be selected. Otherwise, the item with the given ID will be
 *        played
 * \param i_options the number of options to add to the item
 * \param ppsz_options the options to add to the item
 * \param p_e an initialized exception pointer
 */
VLC_DEPRECATED_API void libvlc_playlist_play( libvlc_instance_t*, int, int,
                                          char **, libvlc_exception_t * );

/**
 * Toggle the playlist's pause status.
 *
 * If the playlist was running, it is paused. If it was paused, it is resumed.
 *
 * \param p_instance the playlist instance to pause
 * \param p_e an initialized exception pointer
 */
VLC_DEPRECATED_API void libvlc_playlist_pause( libvlc_instance_t *,
                                           libvlc_exception_t * );

/**
 * Checks whether the playlist is running
 *
 * \param p_instance the playlist instance
 * \param p_e an initialized exception pointer
 * \return 0 if the playlist is stopped or paused, 1 if it is running
 */
VLC_DEPRECATED_API int libvlc_playlist_isplaying( libvlc_instance_t *,
                                              libvlc_exception_t * );

/**
 * Get the number of items in the playlist
 *
 * Expects the playlist instance to be locked already.
 *
 * \param p_instance the playlist instance
 * \param p_e an initialized exception pointer
 * \return the number of items
 */
VLC_DEPRECATED_API int libvlc_playlist_items_count( libvlc_instance_t *,
                                                libvlc_exception_t * );

VLC_DEPRECATED_API int libvlc_playlist_get_current_index( libvlc_instance_t *,
                                                 libvlc_exception_t *);
/**
 * Lock the playlist.
 *
 * \param p_instance the playlist instance
 */
VLC_DEPRECATED_API void libvlc_playlist_lock( libvlc_instance_t * );

/**
 * Unlock the playlist.
 *
 * \param p_instance the playlist instance
 */
VLC_DEPRECATED_API void libvlc_playlist_unlock( libvlc_instance_t * );

/**
 * Stop playing.
 *
 * \param p_instance the playlist instance to stop
 * \param p_e an initialized exception pointer
 */
VLC_DEPRECATED_API void libvlc_playlist_stop( libvlc_instance_t *,
                                          libvlc_exception_t * );

/**
 * Go to the next playlist item. If the playlist was stopped, playback
 * is started.
 *
 * \param p_instance the playlist instance
 * \param p_e an initialized exception pointer
 */
VLC_DEPRECATED_API void libvlc_playlist_next( libvlc_instance_t *,
                                          libvlc_exception_t * );

/**
 * Go to the previous playlist item. If the playlist was stopped, playback
 * is started.
 *
 * \param p_instance the playlist instance
 * \param p_e an initialized exception pointer
 */
VLC_DEPRECATED_API void libvlc_playlist_prev( libvlc_instance_t *,
                                          libvlc_exception_t * );

/**
 * Empty a playlist. All items in the playlist are removed.
 *
 * \param p_instance the playlist instance
 * \param p_e an initialized exception pointer
 */
VLC_DEPRECATED_API void libvlc_playlist_clear( libvlc_instance_t *,
                                           libvlc_exception_t * );

/**
 * Append an item to the playlist. The item is added at the end. If more
 * advanced options are required, \see libvlc_playlist_add_extended instead.
 *
 * \param p_instance the playlist instance
 * \param psz_uri the URI to open, using VLC format
 * \param psz_name a name that you might want to give or NULL
 * \param p_e an initialized exception pointer
 * \return the identifier of the new item
 */
VLC_DEPRECATED_API int libvlc_playlist_add( libvlc_instance_t *, const char *,
                                        const char *, libvlc_exception_t * );

/**
 * Append an item to the playlist. The item is added at the end, with
 * additional input options.
 *
 * \param p_instance the playlist instance
 * \param psz_uri the URI to open, using VLC format
 * \param psz_name a name that you might want to give or NULL
 * \param i_options the number of options to add
 * \param ppsz_options strings representing the options to add
 * \param p_e an initialized exception pointer
 * \return the identifier of the new item
 */
VLC_DEPRECATED_API int libvlc_playlist_add_extended( libvlc_instance_t *, const char *,
                                                 const char *, int, const char **,
                                                 libvlc_exception_t * );

/**
 * Append an item to the playlist. The item is added at the end, with
 * additional input options from an untrusted source.
 *
 * \param p_instance the playlist instance
 * \param psz_uri the URI to open, using VLC format
 * \param psz_name a name that you might want to give or NULL
 * \param i_options the number of options to add
 * \param ppsz_options strings representing the options to add
 * \param p_e an initialized exception pointer
 * \return the identifier of the new item
 */
VLC_DEPRECATED_API int libvlc_playlist_add_extended_untrusted( libvlc_instance_t *, const char *,
                                                 const char *, int, const char **,
                                                 libvlc_exception_t * );

/**
 * Delete the playlist item with the given ID.
 *
 * \param p_instance the playlist instance
 * \param i_id the id to remove
 * \param p_e an initialized exception pointer
 * \return 0 in case of success, a non-zero value otherwise
 */
VLC_DEPRECATED_API int libvlc_playlist_delete_item( libvlc_instance_t *, int,
                                                libvlc_exception_t * );

/** Get the input that is currently being played by the playlist.
 *
 * \param p_instance the playlist instance to use
 * \param p_e an initialized exception pointern
 * \return a media instance object
 */
VLC_DEPRECATED_API libvlc_media_player_t * libvlc_playlist_get_media_player(
                                libvlc_instance_t *, libvlc_exception_t * );

/** @}*/

# ifdef __cplusplus
}
# endif

#endif /* _LIBVLC_DEPRECATED_H */
