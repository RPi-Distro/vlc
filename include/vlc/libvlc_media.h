/*****************************************************************************
 * libvlc.h:  libvlc external API
 *****************************************************************************
 * Copyright (C) 1998-2009 the VideoLAN team
 * $Id: 18fcd216d4e5e2bc0319f7082848d18d456b468a $
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
 *          Jean-Paul Saman <jpsaman@videolan.org>
 *          Pierre d'Herbemont <pdherbemont@videolan.org>
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

/**
 * \file
 * This file defines libvlc_media external API
 */

#ifndef VLC_LIBVLC_MEDIA_H
#define VLC_LIBVLC_MEDIA_H 1

/*****************************************************************************
 * media
 *****************************************************************************/
/** \defgroup libvlc_media libvlc_media
 * \ingroup libvlc
 * LibVLC Media
 * @{
 */

typedef struct libvlc_media_t libvlc_media_t;

/* Meta Handling */
/** defgroup libvlc_meta libvlc_meta
 * \ingroup libvlc_media
 * LibVLC Media Meta
 * @{
 */

typedef enum libvlc_meta_t {
    libvlc_meta_Title,
    libvlc_meta_Artist,
    libvlc_meta_Genre,
    libvlc_meta_Copyright,
    libvlc_meta_Album,
    libvlc_meta_TrackNumber,
    libvlc_meta_Description,
    libvlc_meta_Rating,
    libvlc_meta_Date,
    libvlc_meta_Setting,
    libvlc_meta_URL,
    libvlc_meta_Language,
    libvlc_meta_NowPlaying,
    libvlc_meta_Publisher,
    libvlc_meta_EncodedBy,
    libvlc_meta_ArtworkURL,
    libvlc_meta_TrackID,
    /* Add new meta types HERE */
} libvlc_meta_t;

/** @}*/

/**
 * Note the order of libvlc_state_t enum must match exactly the order of
 * @see mediacontrol_PlayerStatus, @see input_state_e enums,
 * and VideoLAN.LibVLC.State (at bindings/cil/src/media.cs).
 *
 * Expected states by web plugins are:
 * IDLE/CLOSE=0, OPENING=1, BUFFERING=2, PLAYING=3, PAUSED=4,
 * STOPPING=5, ENDED=6, ERROR=7
 */
typedef enum libvlc_state_t
{
    libvlc_NothingSpecial=0,
    libvlc_Opening,
    libvlc_Buffering,
    libvlc_Playing,
    libvlc_Paused,
    libvlc_Stopped,
    libvlc_Ended,
    libvlc_Error
} libvlc_state_t;

/**
 * Create a media with the given MRL.
 *
 * \param p_instance the instance
 * \param psz_mrl the MRL to read
 * \param p_e an initialized exception pointer
 * \return the newly created media
 */
VLC_PUBLIC_API libvlc_media_t * libvlc_media_new(
                                   libvlc_instance_t *p_instance,
                                   const char * psz_mrl,
                                   libvlc_exception_t *p_e );

/**
 * Create a media as an empty node with the passed name.
 *
 * \param p_instance the instance
 * \param psz_name the name of the node
 * \param p_e an initialized exception pointer
 * \return the new empty media
 */
VLC_PUBLIC_API libvlc_media_t * libvlc_media_new_as_node(
                                   libvlc_instance_t *p_instance,
                                   const char * psz_name,
                                   libvlc_exception_t *p_e );

/**
 * Add an option to the media.
 *
 * This option will be used to determine how the media_player will
 * read the media. This allows to use VLC's advanced
 * reading/streaming options on a per-media basis.
 *
 * The options are detailed in vlc --long-help, for instance "--sout-all"
 *
 * \param p_instance the instance
 * \param ppsz_options the options (as a string)
 * \param p_e an initialized exception pointer
 */
VLC_PUBLIC_API void libvlc_media_add_option(
                                   libvlc_media_t * p_md,
                                   const char * ppsz_options,
                                   libvlc_exception_t * p_e );
/**
 * Add an option to the media from an untrusted source.
 *
 * This option will be used to determine how the media_player will
 * read the media. This allows to use VLC's advanced
 * reading/streaming options on a per-media basis.
 *
 * The options are detailed in vlc --long-help, for instance "--sout-all"
 *
 * \param p_instance the instance
 * \param ppsz_options the options (as a string)
 * \param p_e an initialized exception pointer
 */
VLC_PUBLIC_API void libvlc_media_add_option_untrusted(
                                   libvlc_media_t * p_md,
                                   const char * ppsz_options,
                                   libvlc_exception_t * p_e );


/**
 * Retain a reference to a media descriptor object (libvlc_media_t). Use
 * libvlc_media_release() to decrement the reference count of a
 * media descriptor object.
 *
 * \param p_meta_desc a media descriptor object.
 */
VLC_PUBLIC_API void libvlc_media_retain(
                                   libvlc_media_t *p_meta_desc );

/**
 * Decrement the reference count of a media descriptor object. If the
 * reference count is 0, then libvlc_media_release() will release the
 * media descriptor object. It will send out an libvlc_MediaFreed event
 * to all listeners. If the media descriptor object has been released it
 * should not be used again.
 *
 * \param p_meta_desc a media descriptor object.
 */
VLC_PUBLIC_API void libvlc_media_release(
                                   libvlc_media_t *p_meta_desc );


/**
 * Get the media resource locator (mrl) from a media descriptor object
 *
 * \param p_md a media descriptor object
 * \param p_e an initialized exception object
 * \return string with mrl of media descriptor object
 */
VLC_PUBLIC_API char * libvlc_media_get_mrl( libvlc_media_t * p_md,
                                                       libvlc_exception_t * p_e );

/**
 * Duplicate a media descriptor object.
 *
 * \param p_meta_desc a media descriptor object.
 */
VLC_PUBLIC_API libvlc_media_t * libvlc_media_duplicate( libvlc_media_t * );

/**
 * Read the meta of the media.
 *
 * \param p_meta_desc the media to read
 * \param e_meta_desc the meta to read
 * \param p_e an initialized exception pointer
 * \return the media's meta
 */
VLC_PUBLIC_API char * libvlc_media_get_meta(
                                   libvlc_media_t *p_meta_desc,
                                   libvlc_meta_t e_meta,
                                   libvlc_exception_t *p_e );
/**
 * Get current state of media descriptor object. Possible media states
 * are defined in libvlc_structures.c ( libvlc_NothingSpecial=0,
 * libvlc_Opening, libvlc_Buffering, libvlc_Playing, libvlc_Paused,
 * libvlc_Stopped, libvlc_Ended,
 * libvlc_Error).
 *
 * @see libvlc_state_t
 * \param p_meta_desc a media descriptor object
 * \param p_e an initialized exception object
 * \return state of media descriptor object
 */
VLC_PUBLIC_API libvlc_state_t libvlc_media_get_state(
                                   libvlc_media_t *p_meta_desc,
                                   libvlc_exception_t *p_e );


/**
 * Get subitems of media descriptor object. This will increment
 * the reference count of supplied media descriptor object. Use
 * libvlc_media_list_release() to decrement the reference counting.
 *
 * \param p_md media descriptor object
 * \param p_e initalized exception object
 * \return list of media descriptor subitems or NULL
 */

/* This method uses libvlc_media_list_t, however, media_list usage is optionnal
 * and this is here for convenience */
#define VLC_FORWARD_DECLARE_OBJECT(a) struct a

VLC_PUBLIC_API VLC_FORWARD_DECLARE_OBJECT(libvlc_media_list_t *)
libvlc_media_subitems( libvlc_media_t *p_md,
                      libvlc_exception_t *p_e );

/**
 * Get event manager from media descriptor object.
 * NOTE: this function doesn't increment reference counting.
 *
 * \param p_md a media descriptor object
 * \param p_e an initialized exception object
 * \return event manager object
 */
VLC_PUBLIC_API libvlc_event_manager_t *
    libvlc_media_event_manager( libvlc_media_t * p_md,
                                           libvlc_exception_t * p_e );

/**
 * Get duration of media descriptor object item.
 *
 * \param p_md media descriptor object
 * \param p_e an initialized exception object
 * \return duration of media item
 */
VLC_PUBLIC_API libvlc_time_t
   libvlc_media_get_duration( libvlc_media_t * p_md,
                                         libvlc_exception_t * p_e );

/**
 * Get preparsed status for media descriptor object.
 *
 * \param p_md media descriptor object
 * \param p_e an initialized exception object
 * \return true if media object has been preparsed otherwise it returns false
 */
VLC_PUBLIC_API int
   libvlc_media_is_preparsed( libvlc_media_t * p_md,
                                         libvlc_exception_t * p_e );

/**
 * Sets media descriptor's user_data. user_data is specialized data
 * accessed by the host application, VLC.framework uses it as a pointer to
 * an native object that references a libvlc_media_t pointer
 *
 * \param p_md media descriptor object
 * \param p_new_user_data pointer to user data
 * \param p_e an initialized exception object
 */
VLC_PUBLIC_API void
    libvlc_media_set_user_data( libvlc_media_t * p_md,
                                           void * p_new_user_data,
                                           libvlc_exception_t * p_e);

/**
 * Get media descriptor's user_data. user_data is specialized data
 * accessed by the host application, VLC.framework uses it as a pointer to
 * an native object that references a libvlc_media_t pointer
 *
 * \param p_md media descriptor object
 * \param p_e an initialized exception object
 */
VLC_PUBLIC_API void *
    libvlc_media_get_user_data( libvlc_media_t * p_md,
                                           libvlc_exception_t * p_e);

/** @}*/

#endif /* VLC_LIBVLC_MEDIA_H */
