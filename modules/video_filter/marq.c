/*****************************************************************************
 * marq.c : marquee display video plugin for vlc
 *****************************************************************************
 * Copyright (C) 2003-2008 the VideoLAN team
 * $Id: 196c34ab9d0d5f0b8369c0a0607a4cde3baa60ed $
 *
 * Authors: Mark Moriarty
 *          Sigmund Augdal Helberg <dnumgis@videolan.org>
 *          Antoine Cellerier <dionoea . videolan \ org>
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

#include <vlc_filter.h>
#include <vlc_block.h>
#include <vlc_osd.h>

#include <vlc_strings.h>

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  CreateFilter ( vlc_object_t * );
static void DestroyFilter( vlc_object_t * );
static subpicture_t *Filter( filter_t *, mtime_t );


static int MarqueeCallback( vlc_object_t *p_this, char const *psz_var,
                            vlc_value_t oldval, vlc_value_t newval,
                            void *p_data );
static const int pi_color_values[] = {
               0xf0000000, 0x00000000, 0x00808080, 0x00C0C0C0,
               0x00FFFFFF, 0x00800000, 0x00FF0000, 0x00FF00FF, 0x00FFFF00,
               0x00808000, 0x00008000, 0x00008080, 0x0000FF00, 0x00800080,
               0x00000080, 0x000000FF, 0x0000FFFF};
static const char *const ppsz_color_descriptions[] = {
               N_("Default"), N_("Black"), N_("Gray"),
               N_("Silver"), N_("White"), N_("Maroon"), N_("Red"),
               N_("Fuchsia"), N_("Yellow"), N_("Olive"), N_("Green"),
               N_("Teal"), N_("Lime"), N_("Purple"), N_("Navy"), N_("Blue"),
               N_("Aqua") };

/*****************************************************************************
 * filter_sys_t: marquee filter descriptor
 *****************************************************************************/
struct filter_sys_t
{
    vlc_mutex_t lock;

    int i_xoff, i_yoff;  /* offsets for the display string in the video window */
    int i_pos; /* permit relative positioning (top, bottom, left, right, center) */
    int i_timeout;

    char *psz_marquee;    /* marquee string */

    text_style_t *p_style; /* font control */

    mtime_t last_time;
    mtime_t i_refresh;

    bool b_need_update;
};

#define MSG_TEXT N_("Text")
#define MSG_LONGTEXT N_( \
    "Marquee text to display. " \
    "(Available format strings: " \
    "Time related: %Y = year, %m = month, %d = day, %H = hour, " \
    "%M = minute, %S = second, ... " \
    "Meta data related: $a = artist, $b = album, $c = copyright, " \
    "$d = description, $e = encoded by, $g = genre, " \
    "$l = language, $n = track num, $p = now playing, " \
    "$r = rating, $s = subtitles language, $t = title, "\
    "$u = url, $A = date, " \
    "$B = audio bitrate (in kb/s), $C = chapter," \
    "$D = duration, $F = full name with path, $I = title, "\
    "$L = time left, " \
    "$N = name, $O = audio language, $P = position (in %), $R = rate, " \
    "$S = audio sample rate (in kHz), " \
    "$T = time, $U = publisher, $V = volume, $_ = new line) ")
#define POSX_TEXT N_("X offset")
#define POSX_LONGTEXT N_("X offset, from the left screen edge." )
#define POSY_TEXT N_("Y offset")
#define POSY_LONGTEXT N_("Y offset, down from the top." )
#define TIMEOUT_TEXT N_("Timeout")
#define TIMEOUT_LONGTEXT N_("Number of milliseconds the marquee must remain " \
                            "displayed. Default value is " \
                            "0 (remains forever).")
#define REFRESH_TEXT N_("Refresh period in ms")
#define REFRESH_LONGTEXT N_("Number of milliseconds between string updates. " \
                            "This is mainly useful when using meta data " \
                            "or time format string sequences.")
#define OPACITY_TEXT N_("Opacity")
#define OPACITY_LONGTEXT N_("Opacity (inverse of transparency) of " \
    "overlayed text. 0 = transparent, 255 = totally opaque. " )
#define SIZE_TEXT N_("Font size, pixels")
#define SIZE_LONGTEXT N_("Font size, in pixels. Default is -1 (use default " \
    "font size)." )

#define COLOR_TEXT N_("Color")
#define COLOR_LONGTEXT N_("Color of the text that will be rendered on "\
    "the video. This must be an hexadecimal (like HTML colors). The first two "\
    "chars are for red, then green, then blue. #000000 = black, #FF0000 = red,"\
    " #00FF00 = green, #FFFF00 = yellow (red + green), #FFFFFF = white" )

#define POS_TEXT N_("Marquee position")
#define POS_LONGTEXT N_( \
  "You can enforce the marquee position on the video " \
  "(0=center, 1=left, 2=right, 4=top, 8=bottom, you can " \
  "also use combinations of these values, eg 6 = top-right).")

static const int pi_pos_values[] = { 0, 1, 2, 4, 8, 5, 6, 9, 10 };
static const char *const ppsz_pos_descriptions[] =
     { N_("Center"), N_("Left"), N_("Right"), N_("Top"), N_("Bottom"),
     N_("Top-Left"), N_("Top-Right"), N_("Bottom-Left"), N_("Bottom-Right") };

#define CFG_PREFIX "marq-"

#define MARQUEE_HELP N_("Display text above the video")

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin ()
    set_capability( "sub filter", 0 )
    set_shortname( N_("Marquee" ))
    set_description( N_("Marquee display") )
    set_help(MARQUEE_HELP)
    set_callbacks( CreateFilter, DestroyFilter )
    set_category( CAT_VIDEO )
    set_subcategory( SUBCAT_VIDEO_SUBPIC )
    add_string( CFG_PREFIX "marquee", "VLC", NULL, MSG_TEXT, MSG_LONGTEXT,
                false )

    set_section( N_("Position"), NULL )
    add_integer( CFG_PREFIX "x", 0, NULL, POSX_TEXT, POSX_LONGTEXT, true )
    add_integer( CFG_PREFIX "y", 0, NULL, POSY_TEXT, POSY_LONGTEXT, true )
    add_integer( CFG_PREFIX "position", -1, NULL, POS_TEXT, POS_LONGTEXT, false )
        change_integer_list( pi_pos_values, ppsz_pos_descriptions, NULL )

    set_section( N_("Font"), NULL )
    /* 5 sets the default to top [1] left [4] */
    add_integer_with_range( CFG_PREFIX "opacity", 255, 0, 255, NULL,
        OPACITY_TEXT, OPACITY_LONGTEXT, false )
    add_integer( CFG_PREFIX "color", 0xFFFFFF, NULL, COLOR_TEXT, COLOR_LONGTEXT,
                 false )
        change_integer_list( pi_color_values, ppsz_color_descriptions, NULL )
    add_integer( CFG_PREFIX "size", -1, NULL, SIZE_TEXT, SIZE_LONGTEXT,
                 false )

    set_section( N_("Misc"), NULL )
    add_integer( CFG_PREFIX "timeout", 0, NULL, TIMEOUT_TEXT, TIMEOUT_LONGTEXT,
                 false )
    add_integer( CFG_PREFIX "refresh", 1000, NULL, REFRESH_TEXT,
                 REFRESH_LONGTEXT, false )

    add_shortcut( "time" )
    add_obsolete_string( "time-format" )
    add_obsolete_string( "time-x" )
    add_obsolete_string( "time-y" )
    add_obsolete_string( "time-position" )
    add_obsolete_string( "time-opacity" )
    add_obsolete_string( "time-color" )
    add_obsolete_string( "time-size" )
vlc_module_end ()

static const char *const ppsz_filter_options[] = {
    "marquee", "x", "y", "position", "color", "size", "timeout", "refresh",
    "opacity",
    NULL
};

/*****************************************************************************
 * CreateFilter: allocates marquee video filter
 *****************************************************************************/
static int CreateFilter( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;
    filter_sys_t *p_sys;

    /* Allocate structure */
    p_sys = p_filter->p_sys = malloc( sizeof( filter_sys_t ) );
    if( p_sys == NULL )
        return VLC_ENOMEM;

    vlc_mutex_init( &p_sys->lock );
    p_sys->p_style = text_style_New();

    config_ChainParse( p_filter, CFG_PREFIX, ppsz_filter_options,
                       p_filter->p_cfg );


#define CREATE_VAR( stor, type, var ) \
    p_sys->stor = var_CreateGet##type##Command( p_filter, var ); \
    var_AddCallback( p_filter, var, MarqueeCallback, p_sys );

    p_sys->b_need_update = true;
    CREATE_VAR( i_xoff, Integer, "marq-x" );
    CREATE_VAR( i_yoff, Integer, "marq-y" );
    CREATE_VAR( i_timeout,Integer, "marq-timeout" );
    p_sys->i_refresh = 1000 * var_CreateGetIntegerCommand( p_filter,
                                                           "marq-refresh" );
    var_AddCallback( p_filter, "marq-refresh", MarqueeCallback, p_sys );
    CREATE_VAR( i_pos, Integer, "marq-position" );
    CREATE_VAR( psz_marquee, String, "marq-marquee" );
    p_sys->p_style->i_font_alpha = 255 - var_CreateGetIntegerCommand( p_filter,
                                                            "marq-opacity" );
    var_AddCallback( p_filter, "marq-opacity", MarqueeCallback, p_sys );
    CREATE_VAR( p_style->i_font_color, Integer, "marq-color" );
    CREATE_VAR( p_style->i_font_size, Integer, "marq-size" );

    /* Misc init */
    p_filter->pf_sub_filter = Filter;
    p_sys->last_time = 0;

    return VLC_SUCCESS;
}
/*****************************************************************************
 * DestroyFilter: destroy marquee video filter
 *****************************************************************************/
static void DestroyFilter( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;
    filter_sys_t *p_sys = p_filter->p_sys;

    /* Delete the marquee variables */
#define DEL_VAR(var) \
    var_DelCallback( p_filter, var, MarqueeCallback, p_sys ); \
    var_Destroy( p_filter, var );
    DEL_VAR( "marq-x" );
    DEL_VAR( "marq-y" );
    DEL_VAR( "marq-timeout" );
    DEL_VAR( "marq-refresh" );
    DEL_VAR( "marq-position" );
    DEL_VAR( "marq-marquee" );
    DEL_VAR( "marq-opacity" );
    DEL_VAR( "marq-color" );
    DEL_VAR( "marq-size" );

    vlc_mutex_destroy( &p_sys->lock );
    text_style_Delete( p_sys->p_style );
    free( p_sys->psz_marquee );
    free( p_sys );
}

/****************************************************************************
 * Filter: the whole thing
 ****************************************************************************
 * This function outputs subpictures at regular time intervals.
 ****************************************************************************/
static subpicture_t *Filter( filter_t *p_filter, mtime_t date )
{
    filter_sys_t *p_sys = p_filter->p_sys;
    subpicture_t *p_spu = NULL;
    video_format_t fmt;

    vlc_mutex_lock( &p_sys->lock );
    if( p_sys->last_time + p_sys->i_refresh > date )
        goto out;
    if( p_sys->b_need_update == false )
        goto out;

    p_spu = filter_NewSubpicture( p_filter );
    if( !p_spu )
        goto out;

    memset( &fmt, 0, sizeof(video_format_t) );
    fmt.i_chroma = VLC_CODEC_TEXT;
    fmt.i_width = fmt.i_height = 0;
    fmt.i_x_offset = 0;
    fmt.i_y_offset = 0;
    p_spu->p_region = subpicture_region_New( &fmt );
    if( !p_spu->p_region )
    {
        p_filter->pf_sub_buffer_del( p_filter, p_spu );
        p_spu = NULL;
        goto out;
    }

    p_sys->last_time = date;

    if( !strchr( p_sys->psz_marquee, '%' )
     && !strchr( p_sys->psz_marquee, '$' ) )
        p_sys->b_need_update = false;

    p_spu->p_region->psz_text = str_format( p_filter, p_sys->psz_marquee );
    p_spu->i_start = date;
    p_spu->i_stop  = p_sys->i_timeout == 0 ? 0 : date + p_sys->i_timeout * 1000;
    p_spu->b_ephemer = true;

    /*  where to locate the string: */
    if( p_sys->i_pos < 0 )
    {   /*  set to an absolute xy */
        p_spu->p_region->i_align = OSD_ALIGN_LEFT | OSD_ALIGN_TOP;
        p_spu->b_absolute = true;
    }
    else
    {   /* set to one of the 9 relative locations */
        p_spu->p_region->i_align = p_sys->i_pos;
        p_spu->b_absolute = false;
    }

    p_spu->p_region->i_x = p_sys->i_xoff;
    p_spu->p_region->i_y = p_sys->i_yoff;

    p_spu->p_region->p_style = text_style_Duplicate( p_sys->p_style );

out:
    vlc_mutex_unlock( &p_sys->lock );
    return p_spu;
}

/**********************************************************************
 * Callback to update params on the fly
 **********************************************************************/
static int MarqueeCallback( vlc_object_t *p_this, char const *psz_var,
                            vlc_value_t oldval, vlc_value_t newval,
                            void *p_data )
{
    filter_sys_t *p_sys = (filter_sys_t *) p_data;

    VLC_UNUSED(oldval);
    VLC_UNUSED(p_this);

    vlc_mutex_lock( &p_sys->lock );
    if( !strncmp( psz_var, "marq-marquee", 7 ) )
    {
        free( p_sys->psz_marquee );
        p_sys->psz_marquee = strdup( newval.psz_string );
    }
    else if ( !strncmp( psz_var, "marq-x", 6 ) )
    {
        p_sys->i_xoff = newval.i_int;
    }
    else if ( !strncmp( psz_var, "marq-y", 6 ) )
    {
        p_sys->i_yoff = newval.i_int;
    }
    else if ( !strncmp( psz_var, "marq-color", 8 ) )  /* "marq-col" */
    {
        p_sys->p_style->i_font_color = newval.i_int;
    }
    else if ( !strncmp( psz_var, "marq-opacity", 8 ) ) /* "marq-opa" */
    {
        p_sys->p_style->i_font_alpha = 255 - newval.i_int;
    }
    else if ( !strncmp( psz_var, "marq-size", 6 ) )
    {
        p_sys->p_style->i_font_size = newval.i_int;
    }
    else if ( !strncmp( psz_var, "marq-timeout", 12 ) )
    {
        p_sys->i_timeout = newval.i_int;
    }
    else if ( !strncmp( psz_var, "marq-refresh", 12 ) )
    {
        p_sys->i_refresh = newval.i_int * 1000;
    }
    else if ( !strncmp( psz_var, "marq-position", 8 ) )
    /* willing to accept a match against marq-pos */
    {
        p_sys->i_pos = newval.i_int;
        p_sys->i_xoff = -1;       /* force to relative positioning */
    }
    p_sys->b_need_update = true;
    vlc_mutex_unlock( &p_sys->lock );
    return VLC_SUCCESS;
}
