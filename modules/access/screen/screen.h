/*****************************************************************************
 * screen.h: Screen capture module.
 *****************************************************************************
 * Copyright (C) 2004-2008 the VideoLAN team
 * $Id: 28f1433dca9a2053e5d75290e129af855e36bbcd $
 *
 * Authors: Gildas Bazin <gbazin@videolan.org>
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

#include <vlc_input.h>
#include <vlc_access.h>
#include <vlc_demux.h>

#if !defined( HAVE_BEOS ) && !defined( HAVE_DARWIN )
#   define SCREEN_SUBSCREEN
#   define SCREEN_MOUSE
#endif

#ifdef SCREEN_MOUSE
#   include <vlc_image.h>
#endif

typedef struct screen_data_t screen_data_t;

struct demux_sys_t
{
    es_format_t fmt;
    es_out_id_t *es;

    float f_fps;
    mtime_t i_next_date;
    int i_incr;

#ifdef SCREEN_SUBSCREEN
    bool b_follow_mouse;
    unsigned int i_screen_height;
    unsigned int i_screen_width;

    unsigned int i_top;
    unsigned int i_left;
    unsigned int i_height;
    unsigned int i_width;
#endif

#ifdef SCREEN_MOUSE
    picture_t *p_mouse;
    filter_t *p_blend;
    picture_t dst;
#endif

    screen_data_t *p_data;
};

int      screen_InitCapture ( demux_t * );
int      screen_CloseCapture( demux_t * );
block_t *screen_Capture( demux_t * );

#ifdef SCREEN_SUBSCREEN
void FollowMouse( demux_sys_t *, int, int );
#endif
#ifdef SCREEN_MOUSE
void RenderCursor( demux_t *, int, int, uint8_t * );
#endif
