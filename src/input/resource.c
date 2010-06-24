/*****************************************************************************
 * resource.c
 *****************************************************************************
 * Copyright (C) 2008 Laurent Aimar
 * $Id: ae890c4ee118ff305e547072aca6417c87561331 $
 *
 * Authors: Laurent Aimar < fenrir _AT_ videolan _DOT_ org >
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

#include <assert.h>

#include <vlc_common.h>
#include <vlc_vout.h>
#include <vlc_osd.h>
#include <vlc_aout.h>
#include <vlc_sout.h>
#include "../libvlc.h"
#include "../stream_output/stream_output.h"
#include "../audio_output/aout_internal.h"
#include "../video_output/vout_control.h"
#include "input_interface.h"
#include "resource.h"

struct input_resource_t
{
    /* This lock is used to serialize request and protect
     * our variables */
    vlc_mutex_t    lock;

    /* */
    input_thread_t *p_input;

    sout_instance_t *p_sout;
    vout_thread_t   *p_vout_free;

    /* This lock is used to protect vout resources access (for hold)
     * It is a special case because of embed video (possible deadlock
     * between vout window request and vout holds in some(qt4) interface)
     */
    vlc_mutex_t    lock_hold;

    /* You need lock+lock_hold to write to the following variables and
     * only lock or lock_hold to read them */

    int             i_vout;
    vout_thread_t   **pp_vout;

    aout_instance_t *p_aout;
};

/* */
static void DestroySout( input_resource_t *p_resource )
{
#ifdef ENABLE_SOUT
    if( p_resource->p_sout )
        sout_DeleteInstance( p_resource->p_sout );
#endif
    p_resource->p_sout = NULL;
}

static sout_instance_t *DetachSout( input_resource_t *p_resource )
{
    sout_instance_t *p_sout = p_resource->p_sout;
    p_resource->p_sout = NULL;

    return p_sout;
}

static sout_instance_t *RequestSout( input_resource_t *p_resource,
                                     sout_instance_t *p_sout, const char *psz_sout )
{
#ifdef ENABLE_SOUT
    if( !p_sout && !psz_sout )
    {
        if( p_resource->p_sout )
            msg_Dbg( p_resource->p_sout, "destroying useless sout" );
        DestroySout( p_resource );
        return NULL;
    }

    assert( p_resource->p_input );
    assert( !p_sout || ( !p_resource->p_sout && !psz_sout ) );

    /* Check the validity of the sout */
    if( p_resource->p_sout &&
        strcmp( p_resource->p_sout->psz_sout, psz_sout ) )
    {
        msg_Dbg( p_resource->p_input, "destroying unusable sout" );
        DestroySout( p_resource );
    }

    if( psz_sout )
    {
        if( p_resource->p_sout )
        {
            /* Reuse it */
            msg_Dbg( p_resource->p_input, "reusing sout" );
            msg_Dbg( p_resource->p_input, "you probably want to use gather stream_out" );
            vlc_object_attach( p_resource->p_sout, p_resource->p_input );
        }
        else
        {
            /* Create a new one */
            p_resource->p_sout = sout_NewInstance( p_resource->p_input, psz_sout );
        }

        p_sout = p_resource->p_sout;
        p_resource->p_sout = NULL;

        return p_sout;
    }
    else
    {
        vlc_object_detach( p_sout );
        p_resource->p_sout = p_sout;

        return NULL;
    }
#else
    return NULL;
#endif
}

/* */
static void DestroyVout( input_resource_t *p_resource )
{
    assert( p_resource->i_vout == 0 );

    if( p_resource->p_vout_free )
        vout_CloseAndRelease( p_resource->p_vout_free );

    p_resource->p_vout_free = NULL;
}
static vout_thread_t *DetachVout( input_resource_t *p_resource )
{
    vlc_assert_locked( &p_resource->lock );
    assert( p_resource->i_vout == 0 );
    vout_thread_t *p_vout = p_resource->p_vout_free;
    p_resource->p_vout_free = NULL;

    return p_vout;
}

static void DisplayVoutTitle( input_resource_t *p_resource,
                              vout_thread_t *p_vout )
{
    assert( p_resource->p_input );

    /* TODO display the title only one time for the same input ? */

    input_item_t *p_item = input_GetItem( p_resource->p_input );

    char *psz_nowplaying = input_item_GetNowPlaying( p_item );
    if( psz_nowplaying && *psz_nowplaying )
    {
        vout_DisplayTitle( p_vout, psz_nowplaying );
    }
    else
    {
        char *psz_artist = input_item_GetArtist( p_item );
        char *psz_name = input_item_GetTitle( p_item );

        if( !psz_name || *psz_name == '\0' )
        {
            free( psz_name );
            psz_name = input_item_GetName( p_item );
        }
        if( psz_artist && *psz_artist )
        {
            char *psz_string;
            if( asprintf( &psz_string, "%s - %s", psz_name, psz_artist ) != -1 )
            {
                vout_DisplayTitle( p_vout, psz_string );
                free( psz_string );
            }
        }
        else if( psz_name )
        {
            vout_DisplayTitle( p_vout, psz_name );
        }
        free( psz_name );
        free( psz_artist );
    }
    free( psz_nowplaying );
}
static vout_thread_t *RequestVout( input_resource_t *p_resource,
                                   vout_thread_t *p_vout, video_format_t *p_fmt,
                                   bool b_recycle )
{
    vlc_assert_locked( &p_resource->lock );

    if( !p_vout && !p_fmt )
    {
        if( p_resource->p_vout_free )
        {
            msg_Dbg( p_resource->p_vout_free, "destroying useless vout" );
            vout_CloseAndRelease( p_resource->p_vout_free );
            p_resource->p_vout_free = NULL;
        }
        return NULL;
    }

    assert( p_resource->p_input );
    if( p_fmt )
    {
        /* */
        if( !p_vout && p_resource->p_vout_free )
        {
            msg_Dbg( p_resource->p_input, "trying to reuse free vout" );
            p_vout = p_resource->p_vout_free;

            p_resource->p_vout_free = NULL;
        }
        else if( p_vout )
        {
            assert( p_vout != p_resource->p_vout_free );

            vlc_mutex_lock( &p_resource->lock_hold );
            TAB_REMOVE( p_resource->i_vout, p_resource->pp_vout, p_vout );
            vlc_mutex_unlock( &p_resource->lock_hold );
        }

        /* */
        p_vout = vout_Request( p_resource->p_input, p_vout, p_fmt );
        if( !p_vout )
            return NULL;

        DisplayVoutTitle( p_resource, p_vout );

        vlc_mutex_lock( &p_resource->lock_hold );
        TAB_APPEND( p_resource->i_vout, p_resource->pp_vout, p_vout );
        vlc_mutex_unlock( &p_resource->lock_hold );

        return p_vout;
    }
    else
    {
        assert( p_vout );

        vlc_mutex_lock( &p_resource->lock_hold );
        TAB_REMOVE( p_resource->i_vout, p_resource->pp_vout, p_vout );
        const int i_vout_active = p_resource->i_vout;
        vlc_mutex_unlock( &p_resource->lock_hold );

        if( p_resource->p_vout_free || i_vout_active > 0 || !b_recycle )
        {
            if( b_recycle )
                msg_Dbg( p_resource->p_input, "detroying vout (already one saved or active)" );
            vout_CloseAndRelease( p_vout );
        }
        else
        {
            msg_Dbg( p_resource->p_input, "saving a free vout" );
            vout_Flush( p_vout, 1 );
            spu_Control( vout_GetSpu( p_vout ), SPU_CHANNEL_CLEAR, -1 );

            p_resource->p_vout_free = p_vout;
        }
        return NULL;
    }
}
static vout_thread_t *HoldVout( input_resource_t *p_resource )
{
    /* TODO FIXME: p_resource->pp_vout order is NOT stable */
    vlc_mutex_lock( &p_resource->lock_hold );

    vout_thread_t *p_vout = p_resource->i_vout > 0 ? p_resource->pp_vout[0] : NULL;
    if( p_vout )
        vlc_object_hold( p_vout );

    vlc_mutex_unlock( &p_resource->lock_hold );

    return p_vout;
}

static void HoldVouts( input_resource_t *p_resource, vout_thread_t ***ppp_vout,
                       size_t *pi_vout )
{
    vout_thread_t **pp_vout;

    *pi_vout = 0;
    *ppp_vout = NULL;

    vlc_mutex_lock( &p_resource->lock_hold );

    if( p_resource->i_vout <= 0 )
        goto exit;

    pp_vout = malloc( p_resource->i_vout * sizeof(*pp_vout) );
    if( !pp_vout )
        goto exit;

    *ppp_vout = pp_vout;
    *pi_vout = p_resource->i_vout;

    for( int i = 0; i < p_resource->i_vout; i++ )
    {
        pp_vout[i] = p_resource->pp_vout[i];
        vlc_object_hold( pp_vout[i] );
    }

exit:
    vlc_mutex_unlock( &p_resource->lock_hold );
}

/* */
static void DestroyAout( input_resource_t *p_resource )
{
    if( p_resource->p_aout )
        vlc_object_release( p_resource->p_aout );
    p_resource->p_aout = NULL;
}
static aout_instance_t *DetachAout( input_resource_t *p_resource )
{
    vlc_assert_locked( &p_resource->lock );
    vlc_mutex_lock( &p_resource->lock_hold );

    aout_instance_t *p_aout = p_resource->p_aout;
    p_resource->p_aout = NULL;

    vlc_mutex_unlock( &p_resource->lock_hold );

    return p_aout;
}

static aout_instance_t *RequestAout( input_resource_t *p_resource, aout_instance_t *p_aout )
{
    vlc_assert_locked( &p_resource->lock );
    assert( p_resource->p_input );

    if( p_aout )
    {
        msg_Dbg( p_resource->p_input, "releasing aout" );
        vlc_object_release( p_aout );
        return NULL;
    }
    else
    {
        p_aout = p_resource->p_aout;
        if( !p_aout )
        {
            msg_Dbg( p_resource->p_input, "creating aout" );
            p_aout = aout_New( p_resource->p_input );

            vlc_mutex_lock( &p_resource->lock_hold );
            p_resource->p_aout = p_aout;
            vlc_mutex_unlock( &p_resource->lock_hold );
        }
        else
        {
            msg_Dbg( p_resource->p_input, "reusing aout" );
        }

        if( !p_aout )
            return NULL;

        vlc_object_detach( p_aout );
        vlc_object_attach( p_aout, p_resource->p_input );
        vlc_object_hold( p_aout );
        return p_aout;
    }
}
static aout_instance_t *HoldAout( input_resource_t *p_resource )
{
    vlc_mutex_lock( &p_resource->lock_hold );

    aout_instance_t *p_aout = p_resource->p_aout;
    if( p_aout )
        vlc_object_hold( p_aout );

    vlc_mutex_unlock( &p_resource->lock_hold );

    return p_aout;
}

/* */
input_resource_t *input_resource_New( void )
{
    input_resource_t *p_resource = calloc( 1, sizeof(*p_resource) );
    if( !p_resource )
        return NULL;

    vlc_mutex_init( &p_resource->lock );
    vlc_mutex_init( &p_resource->lock_hold );
    return p_resource;
}

void input_resource_Delete( input_resource_t *p_resource )
{
    DestroySout( p_resource );
    DestroyVout( p_resource );
    DestroyAout( p_resource );

    vlc_mutex_destroy( &p_resource->lock_hold );
    vlc_mutex_destroy( &p_resource->lock );
    free( p_resource );
}

void input_resource_SetInput( input_resource_t *p_resource, input_thread_t *p_input )
{
    vlc_mutex_lock( &p_resource->lock );

    if( p_resource->p_input && !p_input )
    {
        if( p_resource->p_aout )
            vlc_object_detach( p_resource->p_aout );

        assert( p_resource->i_vout == 0 );
        if( p_resource->p_vout_free )
            vlc_object_detach( p_resource->p_vout_free );

        if( p_resource->p_sout )
            vlc_object_detach( p_resource->p_sout );
    }

    /* */
    p_resource->p_input = p_input;

    vlc_mutex_unlock( &p_resource->lock );
}

input_resource_t *input_resource_Detach( input_resource_t *p_resource )
{
    input_resource_t *p_ret = input_resource_New();
    if( !p_ret )
        return NULL;

    vlc_mutex_lock( &p_resource->lock );
    assert( !p_resource->p_input );
    p_ret->p_sout = DetachSout( p_resource );
    p_ret->p_vout_free = DetachVout( p_resource );
    p_ret->p_aout = DetachAout( p_resource );
    vlc_mutex_unlock( &p_resource->lock );

    return p_ret;
}

vout_thread_t *input_resource_RequestVout( input_resource_t *p_resource,
                                            vout_thread_t *p_vout, video_format_t *p_fmt, bool b_recycle )
{
    vlc_mutex_lock( &p_resource->lock );
    vout_thread_t *p_ret = RequestVout( p_resource, p_vout, p_fmt, b_recycle );
    vlc_mutex_unlock( &p_resource->lock );

    return p_ret;
}
vout_thread_t *input_resource_HoldVout( input_resource_t *p_resource )
{
    return HoldVout( p_resource );
}

void input_resource_HoldVouts( input_resource_t *p_resource, vout_thread_t ***ppp_vout,
                               size_t *pi_vout )
{
    HoldVouts( p_resource, ppp_vout, pi_vout );
}

void input_resource_TerminateVout( input_resource_t *p_resource )
{
    input_resource_RequestVout( p_resource, NULL, NULL, false );
}
bool input_resource_HasVout( input_resource_t *p_resource )
{
    vlc_mutex_lock( &p_resource->lock );
    assert( !p_resource->p_input );
    const bool b_vout = p_resource->p_vout_free != NULL;
    vlc_mutex_unlock( &p_resource->lock );

    return b_vout;
}

/* */
aout_instance_t *input_resource_RequestAout( input_resource_t *p_resource, aout_instance_t *p_aout )
{
    vlc_mutex_lock( &p_resource->lock );
    aout_instance_t *p_ret = RequestAout( p_resource, p_aout );
    vlc_mutex_unlock( &p_resource->lock );

    return p_ret;
}
aout_instance_t *input_resource_HoldAout( input_resource_t *p_resource )
{
    return HoldAout( p_resource );
}
/* */
sout_instance_t *input_resource_RequestSout( input_resource_t *p_resource, sout_instance_t *p_sout, const char *psz_sout )
{
    vlc_mutex_lock( &p_resource->lock );
    sout_instance_t *p_ret = RequestSout( p_resource, p_sout, psz_sout );
    vlc_mutex_unlock( &p_resource->lock );

    return p_ret;
}
void input_resource_TerminateSout( input_resource_t *p_resource )
{
    input_resource_RequestSout( p_resource, NULL, NULL );
}

