/*****************************************************************************
 * i420_ymga.c : YUV to YUV conversion module for vlc
 *****************************************************************************
 * Copyright (C) 2000, 2001 the VideoLAN team
 * $Id$
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
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

#include <errno.h>                                                 /* ENOMEM */

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_filter.h>
#include <vlc_vout.h>

#define SRC_FOURCC  "I420,IYUV,YV12"
#define DEST_FOURCC "YMGA"

/*****************************************************************************
 * Local and extern prototypes.
 *****************************************************************************/
static int  Activate   ( vlc_object_t * );
static void I420_YMGA  ( filter_t *, picture_t *, picture_t * );
static picture_t *I420_YMGA_Filter( filter_t *, picture_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin ()
#if defined (MODULE_NAME_IS_i420_ymga)
    set_description( N_("Conversions from " SRC_FOURCC " to " DEST_FOURCC) )
    set_capability( "video filter2", 80 )
#elif defined (MODULE_NAME_IS_i420_ymga_mmx)
    set_description( N_("MMX conversions from " SRC_FOURCC " to " DEST_FOURCC) )
    set_capability( "video filter2", 100 )
    add_requirement( MMX )
#endif
    set_callbacks( Activate, NULL )
vlc_module_end ()

/*****************************************************************************
 * Activate: allocate a chroma function
 *****************************************************************************
 * This function allocates and initializes a chroma function
 *****************************************************************************/
static int Activate( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;

    if( p_filter->fmt_in.video.i_width & 1
     || p_filter->fmt_in.video.i_height & 1 )
    {
        return -1;
    }

    if( p_filter->fmt_in.video.i_width != p_filter->fmt_out.video.i_width
     || p_filter->fmt_in.video.i_height != p_filter->fmt_out.video.i_height )
        return -1;

    switch( p_filter->fmt_in.video.i_chroma )
    {
        case VLC_FOURCC('Y','V','1','2'):
        case VLC_FOURCC('I','4','2','0'):
        case VLC_FOURCC('I','Y','U','V'):
            switch( p_filter->fmt_out.video.i_chroma )
            {
                case VLC_FOURCC('Y','M','G','A'):
                    p_filter->pf_video_filter = I420_YMGA_Filter;
                    break;

                default:
                    return -1;
            }
            break;

        default:
            return -1;
    }

    return 0;
}

/* Following functions are local */

VIDEO_FILTER_WRAPPER( I420_YMGA )

/*****************************************************************************
 * I420_YMGA: planar YUV 4:2:0 to Matrox's planar/packed YUV 4:2:0
 *****************************************************************************/
static void I420_YMGA( filter_t *p_filter, picture_t *p_source,
                                           picture_t *p_dest )
{
    uint8_t *p_uv = p_dest->U_PIXELS;
    uint8_t *p_u = p_source->U_PIXELS;
    uint8_t *p_v = p_source->V_PIXELS;

    int i_x;

    /* Copy the Y part */
    vlc_memcpy( p_dest->Y_PIXELS, p_source->Y_PIXELS,
                p_dest->p[Y_PLANE].i_pitch * p_dest->p[Y_PLANE].i_visible_lines );

    /* Copy the U:V part */
    for( i_x = p_dest->p[U_PLANE].i_pitch * p_dest->p[U_PLANE].i_visible_lines / 64;
         i_x--; )
    {
#if defined (MODULE_NAME_IS_i420_ymga)
        *p_uv++ = *p_u++; *p_uv++ = *p_v++; *p_uv++ = *p_u++; *p_uv++ = *p_v++;
        *p_uv++ = *p_u++; *p_uv++ = *p_v++; *p_uv++ = *p_u++; *p_uv++ = *p_v++;
        *p_uv++ = *p_u++; *p_uv++ = *p_v++; *p_uv++ = *p_u++; *p_uv++ = *p_v++;
        *p_uv++ = *p_u++; *p_uv++ = *p_v++; *p_uv++ = *p_u++; *p_uv++ = *p_v++;
        *p_uv++ = *p_u++; *p_uv++ = *p_v++; *p_uv++ = *p_u++; *p_uv++ = *p_v++;
        *p_uv++ = *p_u++; *p_uv++ = *p_v++; *p_uv++ = *p_u++; *p_uv++ = *p_v++;
        *p_uv++ = *p_u++; *p_uv++ = *p_v++; *p_uv++ = *p_u++; *p_uv++ = *p_v++;
        *p_uv++ = *p_u++; *p_uv++ = *p_v++; *p_uv++ = *p_u++; *p_uv++ = *p_v++;
#else
        __asm__( ".p2align 5 \n\
        movd       (%0), %%mm0  # Load 4 Cr   00 00 00 00 v3 v2 v1 v0     \n\
        movd      4(%0), %%mm2  # Load 4 Cr   00 00 00 00 v3 v2 v1 v0     \n\
        movd      8(%0), %%mm4  # Load 4 Cr   00 00 00 00 v3 v2 v1 v0     \n\
        movd     12(%0), %%mm6  # Load 4 Cr   00 00 00 00 v3 v2 v1 v0     \n\
        movd       (%1), %%mm1  # Load 4 Cb   00 00 00 00 u3 u2 u1 u0     \n\
        movd      4(%1), %%mm3  # Load 4 Cb   00 00 00 00 u3 u2 u1 u0     \n\
        movd      8(%1), %%mm5  # Load 4 Cb   00 00 00 00 u3 u2 u1 u0     \n\
        movd     12(%1), %%mm7  # Load 4 Cb   00 00 00 00 u3 u2 u1 u0     \n\
        punpcklbw %%mm1, %%mm0  #             u3 v3 u2 v2 u1 v1 u0 v0     \n\
        punpcklbw %%mm3, %%mm2  #             u3 v3 u2 v2 u1 v1 u0 v0     \n\
        punpcklbw %%mm5, %%mm4  #             u3 v3 u2 v2 u1 v1 u0 v0     \n\
        punpcklbw %%mm7, %%mm6  #             u3 v3 u2 v2 u1 v1 u0 v0     \n\
        movq      %%mm0, (%2)   # Store CrCb                              \n\
        movq      %%mm2, 8(%2)  # Store CrCb                              \n\
        movq      %%mm4, 16(%2) # Store CrCb                              \n\
        movq      %%mm6, 24(%2) # Store CrCb"
        : : "r" (p_v), "r" (p_u), "r" (p_uv) );

        p_v += 16; p_u += 16; p_uv += 32;
#endif
    }
}

