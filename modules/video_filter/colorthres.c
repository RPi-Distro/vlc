/*****************************************************************************
 * colorthres.c: Threshold color based on similarity to reference color
 *****************************************************************************
 * Copyright (C) 2000-2009 the VideoLAN team
 * $Id: c49ea7dc2bf44800ed1d91b334a37e08c1ebbaf2 $
 *
 * Authors: Sigmund Augdal <dnumgis@videolan.org>
 *          Antoine Cellerier <dionoea at videolan dot org>
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

#include <math.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_sout.h>

#include <vlc_filter.h>
#include "filter_picture.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Create    ( vlc_object_t * );
static void Destroy   ( vlc_object_t * );

static picture_t *Filter( filter_t *, picture_t * );
static picture_t *FilterPacked( filter_t *, picture_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define COLOR_TEXT N_("Color")
#define COLOR_LONGTEXT N_("Colors similar to this will be kept, others will be "\
    "grayscaled. This must be an hexadecimal (like HTML colors). The first two "\
    "chars are for red, then green, then blue. #000000 = black, #FF0000 = red,"\
    " #00FF00 = green, #FFFF00 = yellow (red + green), #FFFFFF = white" )
#define COLOR_HELP N_("Select one color in the video")
static const int pi_color_values[] = {
  0x00FF0000, 0x00FF00FF, 0x00FFFF00, 0x0000FF00, 0x000000FF, 0x0000FFFF };

static const char *const ppsz_color_descriptions[] = {
  N_("Red"), N_("Fuchsia"), N_("Yellow"), N_("Lime"), N_("Blue"), N_("Aqua") };

#define CFG_PREFIX "colorthres-"

vlc_module_begin ()
    set_description( N_("Color threshold filter") )
    set_shortname( N_("Color threshold" ))
    set_help(COLOR_HELP)
    set_category( CAT_VIDEO )
    set_subcategory( SUBCAT_VIDEO_VFILTER )
    set_capability( "video filter2", 0 )
    add_integer( CFG_PREFIX "color", 0x00FF0000, NULL, COLOR_TEXT,
                 COLOR_LONGTEXT, false )
        change_integer_list( pi_color_values, ppsz_color_descriptions, NULL )
    add_integer( CFG_PREFIX "saturationthres", 20, NULL,
                 N_("Saturaton threshold"), "", false )
    add_integer( CFG_PREFIX "similaritythres", 15, NULL,
                 N_("Similarity threshold"), "", false )
    set_callbacks( Create, Destroy )
vlc_module_end ()

static const char *const ppsz_filter_options[] = {
    "color", "saturationthres", "similaritythres", NULL
};

/*****************************************************************************
 * callback prototypes
 *****************************************************************************/
static int FilterCallback( vlc_object_t *, char const *,
                           vlc_value_t, vlc_value_t, void * );


/*****************************************************************************
 * filter_sys_t: adjust filter method descriptor
 *****************************************************************************/
struct filter_sys_t
{
    int i_simthres;
    int i_satthres;
    int i_color;
    vlc_mutex_t lock;
};

/*****************************************************************************
 * Create: allocates adjust video thread output method
 *****************************************************************************
 * This function allocates and initializes a adjust vout method.
 *****************************************************************************/
static int Create( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;
    filter_sys_t *p_sys;

    switch( p_filter->fmt_in.video.i_chroma )
    {
        CASE_PLANAR_YUV
            p_filter->pf_video_filter = Filter;
            break;

        CASE_PACKED_YUV_422
            p_filter->pf_video_filter = FilterPacked;
            break;

        default:
            msg_Err( p_filter, "Unsupported input chroma (%4.4s)",
                     (char*)&(p_filter->fmt_in.video.i_chroma) );
            return VLC_EGENERIC;
    }

    if( p_filter->fmt_in.video.i_chroma != p_filter->fmt_out.video.i_chroma )
    {
        msg_Err( p_filter, "Input and output chromas don't match" );
        return VLC_EGENERIC;
    }

    /* Allocate structure */
    p_sys = p_filter->p_sys = malloc( sizeof( filter_sys_t ) );
    if( p_filter->p_sys == NULL )
        return VLC_ENOMEM;

    config_ChainParse( p_filter, CFG_PREFIX, ppsz_filter_options,
                       p_filter->p_cfg );
    p_sys->i_color = var_CreateGetIntegerCommand( p_filter, CFG_PREFIX "color" );
    p_sys->i_simthres = var_CreateGetIntegerCommand( p_filter,
                                                     CFG_PREFIX "similaritythres" );
    p_sys->i_satthres = var_CreateGetIntegerCommand( p_filter,
                                                     CFG_PREFIX "saturationthres" );

    vlc_mutex_init( &p_sys->lock );

    var_AddCallback( p_filter, CFG_PREFIX "color", FilterCallback, NULL );
    var_AddCallback( p_filter, CFG_PREFIX "similaritythres", FilterCallback, NULL );
    var_AddCallback( p_filter, CFG_PREFIX "saturationthres", FilterCallback, NULL );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Destroy: destroy adjust video thread output method
 *****************************************************************************
 * Terminate an output method created by adjustCreateOutputMethod
 *****************************************************************************/
static void Destroy( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;

    var_DelCallback( p_filter, CFG_PREFIX "color", FilterCallback, NULL );
    var_DelCallback( p_filter, CFG_PREFIX "similaritythres", FilterCallback, NULL );
    var_DelCallback( p_filter, CFG_PREFIX "saturationthres", FilterCallback, NULL );

    vlc_mutex_destroy( &p_filter->p_sys->lock );
    free( p_filter->p_sys );
}

/*****************************************************************************
 * Render: displays previously rendered output
 *****************************************************************************
 * This function send the currently rendered image to adjust modified image,
 * waits until it is displayed and switch the two rendering buffers, preparing
 * next frame.
 *****************************************************************************/
static picture_t *Filter( filter_t *p_filter, picture_t *p_pic )
{
    picture_t *p_outpic;
    filter_sys_t *p_sys = p_filter->p_sys;

    vlc_mutex_lock( &p_sys->lock );
    int i_simthres = p_sys->i_simthres;
    int i_satthres = p_sys->i_satthres;
    int i_color = p_sys->i_color;
    vlc_mutex_unlock( &p_sys->lock );

    if( !p_pic ) return NULL;

    p_outpic = filter_NewPicture( p_filter );
    if( !p_outpic )
    {
        picture_Release( p_pic );
        return NULL;
    }

    /* Copy the Y plane */
    plane_CopyPixels( &p_outpic->p[Y_PLANE], &p_pic->p[Y_PLANE] );

    /*
     * Do the U and V planes
     */
    int i_red = ( i_color & 0xFF0000 ) >> 16;
    int i_green = ( i_color & 0xFF00 ) >> 8;
    int i_blue = i_color & 0xFF;
    int i_u = (int8_t)(( -38 * i_red - 74 * i_green +
                     112 * i_blue + 128) >> 8) + 128;
    int i_v = (int8_t)(( 112 * i_red  -  94 * i_green -
                      18 * i_blue + 128) >> 8) + 128;

    int refu = i_u - 0x80;         /*bright red*/
    int refv = i_v - 0x80;
    int reflength = sqrt(refu*refu+refv*refv);

    for( int y = 0; y < p_pic->p[U_PLANE].i_visible_lines; y++ )
    {
        uint8_t *p_src_u = &p_pic->p[U_PLANE].p_pixels[y * p_pic->p[U_PLANE].i_pitch];
        uint8_t *p_src_v = &p_pic->p[V_PLANE].p_pixels[y * p_pic->p[V_PLANE].i_pitch];
        uint8_t *p_dst_u = &p_outpic->p[U_PLANE].p_pixels[y * p_outpic->p[U_PLANE].i_pitch];
        uint8_t *p_dst_v = &p_outpic->p[V_PLANE].p_pixels[y * p_outpic->p[V_PLANE].i_pitch];

        for( int x = 0; x < p_pic->p[U_PLANE].i_visible_pitch; x++ )
        {
            /* Length of color vector */
            int inu = *p_src_u - 0x80;
            int inv = *p_src_v - 0x80;
            int length = sqrt(inu*inu+inv*inv);

            int diffu = refu * length - inu *reflength;
            int diffv = refv * length - inv *reflength;
            long long int difflen2=diffu*diffu;
            difflen2 +=diffv*diffv;
            long long int thres = length*reflength;
            thres *= thres;
            if( length > i_satthres && (difflen2*i_simthres< thres ) )
            {
                *p_dst_u++ = *p_src_u;
                *p_dst_v++ = *p_src_v;
            }
            else
            {
                *p_dst_u++ = 0x80;
                *p_dst_v++ = 0x80;
            }
            p_src_u++;
            p_src_v++;
        }
    }

    return CopyInfoAndRelease( p_outpic, p_pic );
}

static picture_t *FilterPacked( filter_t *p_filter, picture_t *p_pic )
{
    picture_t *p_outpic;
    filter_sys_t *p_sys = p_filter->p_sys;

    vlc_mutex_lock( &p_sys->lock );
    int i_simthres = p_sys->i_simthres;
    int i_satthres = p_sys->i_satthres;
    int i_color = p_sys->i_color;
    vlc_mutex_unlock( &p_sys->lock );

    if( !p_pic ) return NULL;

    p_outpic = filter_NewPicture( p_filter );
    if( !p_outpic )
    {
        picture_Release( p_pic );
        return NULL;
    }

    int i_y_offset, i_u_offset, i_v_offset;
    GetPackedYuvOffsets( p_filter->fmt_in.video.i_chroma,
                         &i_y_offset, &i_u_offset, &i_v_offset );

    /*
     * Copy Y and do the U and V planes
     */
    int i_red = ( i_color & 0xFF0000 ) >> 16;
    int i_green = ( i_color & 0xFF00 ) >> 8;
    int i_blue = i_color & 0xFF;
    int i_u = (int8_t)(( -38 * i_red - 74 * i_green +
                     112 * i_blue + 128) >> 8) + 128;
    int i_v = (int8_t)(( 112 * i_red  -  94 * i_green -
                      18 * i_blue + 128) >> 8) + 128;
    int refu = i_u - 0x80;         /*bright red*/
    int refv = i_v - 0x80;
    int reflength = sqrt(refu*refu+refv*refv);

    for( int y = 0; y < p_pic->p->i_visible_lines; y++ )
    {
        uint8_t *p_src = &p_pic->p->p_pixels[y * p_pic->p->i_pitch];
        uint8_t *p_dst = &p_outpic->p->p_pixels[y * p_outpic->p->i_pitch];

        for( int x = 0; x < p_pic->p->i_visible_pitch / 4; x++ )
        {
            p_dst[i_y_offset + 0] = p_src[i_y_offset + 0];
            p_dst[i_y_offset + 2] = p_src[i_y_offset + 2];

            /* Length of color vector */
            int inu = p_src[i_u_offset] - 0x80;
            int inv = p_src[i_v_offset] - 0x80;
            int length = sqrt(inu*inu+inv*inv);

            int diffu = refu * length - inu *reflength;
            int diffv = refv * length - inv *reflength;
            long long int difflen2=diffu*diffu;
            difflen2 +=diffv*diffv;
            long long int thres = length*reflength;
            thres *= thres;
            if( length > i_satthres && (difflen2*i_simthres< thres ) )
            {
                p_dst[i_u_offset] = p_src[i_u_offset];
                p_dst[i_v_offset] = p_src[i_v_offset];
            }
            else
            {
                p_dst[i_u_offset] = 0x80;
                p_dst[i_v_offset] = 0x80;
            }

            p_dst += 4;
            p_src += 4;
        }
    }

    return CopyInfoAndRelease( p_outpic, p_pic );
}

static int FilterCallback ( vlc_object_t *p_this, char const *psz_var,
                            vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    (void)oldval;    (void)p_data;
    filter_t *p_filter = (filter_t*)p_this;
    filter_sys_t *p_sys = p_filter->p_sys;

    if( !strcmp( psz_var, CFG_PREFIX "color" ) )
    {
        vlc_mutex_lock( &p_sys->lock );
        p_sys->i_color = newval.i_int;
        vlc_mutex_unlock( &p_sys->lock );
    }
    else if( !strcmp( psz_var, CFG_PREFIX "similaritythres" ) )
    {
        vlc_mutex_lock( &p_sys->lock );
        p_sys->i_simthres = newval.i_int;
        vlc_mutex_unlock( &p_sys->lock );
    }
    else /* CFG_PREFIX "saturationthres" */
    {
        vlc_mutex_lock( &p_sys->lock );
        p_sys->i_satthres = newval.i_int;
        vlc_mutex_unlock( &p_sys->lock );
    }

    return VLC_SUCCESS;
}
