/*****************************************************************************
 * video_text.c : text manipulation functions
 *****************************************************************************
 * Copyright (C) 1999-2007 the VideoLAN team
 * $Id$
 *
 * Author: Sigmund Augdal Helberg <dnumgis@videolan.org>
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_vout.h>
#include <vlc_block.h>
#include <vlc_filter.h>
#include <vlc_osd.h>

/**
 * \brief Show text on the video for some time
 * \param p_vout pointer to the vout the text is to be showed on
 * \param i_channel Subpicture channel
 * \param psz_string The text to be shown
 * \param p_style Pointer to a struct with text style info
 * \param i_flags flags for alignment and such
 * \param i_hmargin horizontal margin in pixels
 * \param i_vmargin vertical margin in pixels
 * \param i_duration Amount of time the text is to be shown.
 */
int vout_ShowTextRelative( vout_thread_t *p_vout, int i_channel,
                           char *psz_string, text_style_t *p_style,
                           int i_flags, int i_hmargin, int i_vmargin,
                           mtime_t i_duration )
{
    mtime_t i_now = mdate();

    return vout_ShowTextAbsolute( p_vout, i_channel, psz_string,
                                  p_style, i_flags, i_hmargin, i_vmargin,
                                  i_now, i_now + i_duration );
}

/**
 * \brief Show text on the video from a given start date to a given end date
 * \param p_vout pointer to the vout the text is to be showed on
 * \param i_channel Subpicture channel
 * \param psz_string The text to be shown
 * \param p_style Pointer to a struct with text style info
 * \param i_flags flags for alignment and such
 * \param i_hmargin horizontal margin in pixels
 * \param i_vmargin vertical margin in pixels
 * \param i_start the time when this string is to appear on the video
 * \param i_stop the time when this string should stop to be displayed
 *               if this is 0 the string will be shown untill the next string
 *               is about to be shown
 */
int vout_ShowTextAbsolute( vout_thread_t *p_vout, int i_channel,
                           const char *psz_string, text_style_t *p_style,
                           int i_flags, int i_hmargin, int i_vmargin,
                           mtime_t i_start, mtime_t i_stop )
{
    (void)p_style;
    subpicture_t *p_spu;
    video_format_t fmt;
    /* (void)p_style; FIXME: <-- why ask for this if it's unused?!? */

    if( !psz_string ) return VLC_EGENERIC;

    p_spu = subpicture_New();
    if( !p_spu )
        return VLC_EGENERIC;

    p_spu->i_channel = i_channel;
    p_spu->i_start = i_start;
    p_spu->i_stop = i_stop;
    p_spu->b_ephemer = true;
    p_spu->b_absolute = false;
    p_spu->b_fade = true;


    /* Create a new subpicture region */
    memset( &fmt, 0, sizeof(video_format_t) );
    fmt.i_chroma = VLC_FOURCC('T','E','X','T');
    fmt.i_aspect = 0;
    fmt.i_width = fmt.i_height = 0;
    fmt.i_x_offset = fmt.i_y_offset = 0;
    p_spu->p_region = subpicture_region_New( &fmt );
    if( !p_spu->p_region )
    {
        msg_Err( p_vout, "cannot allocate SPU region" );
        subpicture_Delete( p_spu );
        return VLC_EGENERIC;
    }

    p_spu->p_region->psz_text = strdup( psz_string );
    p_spu->p_region->i_align = i_flags & SUBPICTURE_ALIGN_MASK;
    p_spu->p_region->i_x = i_hmargin;
    p_spu->p_region->i_y = i_vmargin;

    spu_DisplaySubpicture( p_vout->p_spu, p_spu );

    return VLC_SUCCESS;
}


/**
 * \brief Write an informative message at the default location,
 *        for the default duration and only if the OSD option is enabled.
 * \param p_caller The object that called the function.
 * \param i_channel Subpicture channel
 * \param psz_format printf style formatting
 **/
void __vout_OSDMessage( vlc_object_t *p_caller, int i_channel,
                        const char *psz_format, ... )
{
    vout_thread_t *p_vout;
    char *psz_string = NULL;
    va_list args;

    if( !config_GetInt( p_caller, "osd" ) ) return;

    p_vout = vlc_object_find( p_caller, VLC_OBJECT_VOUT, FIND_ANYWHERE );
    if( p_vout )
    {
        va_start( args, psz_format );
        if( vasprintf( &psz_string, psz_format, args ) != -1 )
        {
            vout_ShowTextRelative( p_vout, i_channel, psz_string, NULL,
                                   OSD_ALIGN_TOP|OSD_ALIGN_RIGHT,
                                   30 + p_vout->fmt_in.i_width
                                      - p_vout->fmt_in.i_visible_width
                                      - p_vout->fmt_in.i_x_offset,
                                   20 + p_vout->fmt_in.i_y_offset, 1000000 );
            free( psz_string );
        }
        vlc_object_release( p_vout );
        va_end( args );
    }
}
