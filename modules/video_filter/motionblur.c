/*****************************************************************************
 * motion_blur.c : motion blur filter for vlc
 *****************************************************************************
 * Copyright (C) 2000, 2001, 2002, 2003 the VideoLAN team
 * $Id: 6cca33932d749529caf11e196fd6b3b01f74a358 $
 *
 * Authors: Sigmund Augdal Helberg <dnumgis@videolan.org>
 *          Antoine Cellerier <dionoea &t videolan d.t org>
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
#include <vlc_sout.h>
#include <vlc_filter.h>
#include "filter_picture.h"

/*****************************************************************************
 * Local protypes
 *****************************************************************************/
static int  Create       ( vlc_object_t * );
static void Destroy      ( vlc_object_t * );
static picture_t *Filter ( filter_t *, picture_t * );
static void RenderBlur   ( filter_sys_t *, picture_t *, picture_t * );
static int MotionBlurCallback( vlc_object_t *, char const *,
                               vlc_value_t, vlc_value_t, void * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define FACTOR_TEXT N_("Blur factor (1-127)")
#define FACTOR_LONGTEXT N_("The degree of blurring from 1 to 127.")

#define FILTER_PREFIX "blur-"

vlc_module_begin ()
    set_shortname( N_("Motion blur") )
    set_description( N_("Motion blur filter") )
    set_capability( "video filter2", 0 )
    set_category( CAT_VIDEO )
    set_subcategory( SUBCAT_VIDEO_VFILTER )

    add_integer_with_range( FILTER_PREFIX "factor", 80, 1, 127, NULL,
                            FACTOR_TEXT, FACTOR_LONGTEXT, false )

    add_shortcut( "blur" )

    set_callbacks( Create, Destroy )
vlc_module_end ()

static const char *const ppsz_filter_options[] = {
    "factor", NULL
};

/*****************************************************************************
 * filter_sys_t
 *****************************************************************************/
struct filter_sys_t
{
    picture_t *p_tmp;
    bool      b_first;

    vlc_spinlock_t lock;
    int        i_factor;
};

/*****************************************************************************
 * Create
 *****************************************************************************/
static int Create( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;

    /* Allocate structure */
    p_filter->p_sys = malloc( sizeof( filter_sys_t ) );
    if( p_filter->p_sys == NULL )
        return VLC_ENOMEM;

    p_filter->p_sys->p_tmp = picture_NewFromFormat( &p_filter->fmt_in.video );
    if( !p_filter->p_sys->p_tmp )
    {
        free( p_filter->p_sys );
        return VLC_ENOMEM;
    }
    p_filter->p_sys->b_first = true;

    p_filter->pf_video_filter = Filter;

    config_ChainParse( p_filter, FILTER_PREFIX, ppsz_filter_options,
                       p_filter->p_cfg );

    p_filter->p_sys->i_factor =
        var_CreateGetIntegerCommand( p_filter, FILTER_PREFIX "factor" );
    vlc_spin_init( &p_filter->p_sys->lock );
    var_AddCallback( p_filter, FILTER_PREFIX "factor",
                     MotionBlurCallback, p_filter->p_sys );


    return VLC_SUCCESS;
}

/*****************************************************************************
 * Destroy
 *****************************************************************************/
static void Destroy( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;

    var_DelCallback( p_filter, FILTER_PREFIX "factor",
                     MotionBlurCallback, p_filter->p_sys );
    vlc_spin_destroy( &p_filter->p_sys->lock );

    picture_Release( p_filter->p_sys->p_tmp );
    free( p_filter->p_sys );
}

/*****************************************************************************
 * Filter
 *****************************************************************************/
static picture_t *Filter( filter_t *p_filter, picture_t *p_pic )
{
    picture_t * p_outpic;
    filter_sys_t *p_sys = p_filter->p_sys;

    if( !p_pic ) return NULL;

    p_outpic = filter_NewPicture( p_filter );
    if( !p_outpic )
    {
        picture_Release( p_pic );
        return NULL;
    }

    if( p_sys->b_first )
    {
        picture_CopyPixels( p_sys->p_tmp, p_pic );
        p_sys->b_first = false;
    }

    /* Get a new picture */
    RenderBlur( p_sys, p_pic, p_outpic );

    picture_CopyPixels( p_sys->p_tmp, p_outpic );

    return CopyInfoAndRelease( p_outpic, p_pic );
}

/*****************************************************************************
 * RenderBlur: renders a blurred picture
 *****************************************************************************/
static void RenderBlur( filter_sys_t *p_sys, picture_t *p_newpic,
                        picture_t *p_outpic )
{
    int i_plane;
    vlc_spin_lock( &p_sys->lock );
    const int i_oldfactor = p_sys->i_factor;
    vlc_spin_unlock( &p_sys->lock );
    int i_newfactor = 128 - i_oldfactor;

    for( i_plane = 0; i_plane < p_outpic->i_planes; i_plane++ )
    {
        uint8_t *p_old, *p_new, *p_out, *p_out_end, *p_out_line_end;
        const int i_visible_pitch = p_outpic->p[i_plane].i_visible_pitch;
        const int i_visible_lines = p_outpic->p[i_plane].i_visible_lines;

        p_out = p_outpic->p[i_plane].p_pixels;
        p_new = p_newpic->p[i_plane].p_pixels;
        p_old = p_sys->p_tmp->p[i_plane].p_pixels;
        p_out_end = p_out + p_outpic->p[i_plane].i_pitch * i_visible_lines;
        while ( p_out < p_out_end )
        {
            p_out_line_end = p_out + i_visible_pitch;

            while ( p_out < p_out_line_end )
            {
                *p_out++ = (((*p_old++) * i_oldfactor) +
                            ((*p_new++) * i_newfactor)) >> 7;
            }

            p_old += p_sys->p_tmp->p[i_plane].i_pitch - i_visible_pitch;
            p_new += p_newpic->p[i_plane].i_pitch     - i_visible_pitch;
            p_out += p_outpic->p[i_plane].i_pitch     - i_visible_pitch;
        }
    }
}

static int MotionBlurCallback( vlc_object_t *p_this, char const *psz_var,
                               vlc_value_t oldval, vlc_value_t newval,
                               void *p_data )
{
    VLC_UNUSED(p_this); VLC_UNUSED(oldval);
    filter_sys_t *p_sys = (filter_sys_t *)p_data;

    if( !strcmp( psz_var, FILTER_PREFIX "factor" ) )
    {
        vlc_spin_lock( &p_sys->lock );
        p_sys->i_factor = __MIN( 127, __MAX( 1, newval.i_int ) );
        vlc_spin_unlock( &p_sys->lock );
    }
    return VLC_SUCCESS;
}
