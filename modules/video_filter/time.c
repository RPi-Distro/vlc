/*****************************************************************************
 * time.c : time display video plugin for vlc
 *****************************************************************************
 * Copyright (C) 2003-2005 the VideoLAN team
 * $Id: d17c7c0ad3eed8feda3ed70dd6438f44c2839230 $
 *
 * Authors: Sigmund Augdal Helberg <dnumgis@videolan.org>
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
#include <stdlib.h>                                      /* malloc(), free() */
#include <string.h>

#include <time.h>

#include <vlc/vlc.h>
#include <vlc/vout.h>

#include "vlc_filter.h"
#include "vlc_block.h"
#include "vlc_osd.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  CreateFilter ( vlc_object_t * );
static void DestroyFilter( vlc_object_t * );
static subpicture_t *Filter( filter_t *, mtime_t );
static int TimeCallback( vlc_object_t *p_this, char const *psz_var,
                            vlc_value_t oldval, vlc_value_t newval,
                            void *p_data );
static int pi_color_values[] = { 0xf0000000, 0x00000000, 0x00808080, 0x00C0C0C0, 
               0x00FFFFFF, 0x00800000, 0x00FF0000, 0x00FF00FF, 0x00FFFF00, 
               0x00808000, 0x00008000, 0x00008080, 0x0000FF00, 0x00800080, 
               0x00000080, 0x000000FF, 0x0000FFFF}; 
static char *ppsz_color_descriptions[] = { N_("Default"), N_("Black"), 
               N_("Gray"), N_("Silver"), N_("White"), N_("Maroon"), N_("Red"),
               N_("Fuchsia"), N_("Yellow"), N_("Olive"), N_("Green"), 
               N_("Teal"), N_("Lime"), N_("Purple"), N_("Navy"), N_("Blue"), 
               N_("Aqua") };

/*****************************************************************************
 * filter_sys_t: time filter descriptor
 *****************************************************************************/
struct filter_sys_t
{
    int         i_xoff, i_yoff; /* offsets for the display string in the video window */
    char        *psz_format;    /* time format string */
    int         i_pos;          /* permit relative positioning (top, bottom, left, right, center) */
    text_style_t *p_style;      /* font control */

    time_t last_time;
};

#define MSG_TEXT N_("Time format string (%Y%m%d %H%M%S)")
#define MSG_LONGTEXT N_("Time format string (%Y = year, %m = month, %d = day, %H = hour, %M = minute, %S = second).")
#define POSX_TEXT N_("X offset")
#define POSX_LONGTEXT N_("X offset, from the left screen edge" )
#define POSY_TEXT N_("Y offset")
#define POSY_LONGTEXT N_("Y offset, down from the top" )
#define OPACITY_TEXT N_("Opacity")
#define OPACITY_LONGTEXT N_("Opacity (inverse of transparency) of " \
    "overlay text. 0 = transparent, 255 = totally opaque." )

#define SIZE_TEXT N_("Font size, pixels")
#define SIZE_LONGTEXT N_("Font size, in pixels. Default is -1 (use default " \
    "font size)." )

#define COLOR_TEXT N_("Color")
#define COLOR_LONGTEXT N_("Color of the text that will be rendered on "\
    "the video. This must be an hexadecimal (like HTML colors). The first two "\
    "chars are for red, then green, then blue. #000000 = black, #FF0000 = red,"\
    " #00FF00 = green, #FFFF00 = yellow (red + green), #FFFFFF = white" )

#define POS_TEXT N_("Text position")
#define POS_LONGTEXT N_( \
  "You can enforce the text position on the video " \
  "(0=center, 1=left, 2=right, 4=top, 8=bottom, you can " \
  "also use combinations of these values, e.g. 6 = top-right).")

static int pi_pos_values[] = { 0, 1, 2, 4, 8, 5, 6, 9, 10 };
static char *ppsz_pos_descriptions[] =
     { N_("Center"), N_("Left"), N_("Right"), N_("Top"), N_("Bottom"),
     N_("Top-Left"), N_("Top-Right"), N_("Bottom-Left"), N_("Bottom-Right") };

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_capability( "sub filter", 0 );
    set_shortname( _("Time overlay"));
    set_category( CAT_VIDEO );
    set_subcategory( SUBCAT_VIDEO_SUBPIC );
    set_callbacks( CreateFilter, DestroyFilter );
    add_string( "time-format", "%Y-%m-%d   %H:%M:%S", NULL, MSG_TEXT,
                MSG_LONGTEXT, VLC_TRUE );
    add_integer( "time-x", -1, NULL, POSX_TEXT, POSX_LONGTEXT, VLC_TRUE );
    add_integer( "time-y", 0, NULL, POSY_TEXT, POSY_LONGTEXT, VLC_TRUE );
    add_integer( "time-position", 9, NULL, POS_TEXT, POS_LONGTEXT, VLC_FALSE );
    /* 9 sets the default to bottom-left, minimizing jitter */
    change_integer_list( pi_pos_values, ppsz_pos_descriptions, 0 );
    add_integer_with_range( "time-opacity", 255, 0, 255, NULL,
        OPACITY_TEXT, OPACITY_LONGTEXT, VLC_FALSE );
    add_integer( "time-color", 0xFFFFFF, NULL, COLOR_TEXT, COLOR_LONGTEXT,
                 VLC_FALSE );
        change_integer_list( pi_color_values, ppsz_color_descriptions, 0 );
    add_integer( "time-size", -1, NULL, SIZE_TEXT, SIZE_LONGTEXT, VLC_FALSE );
    set_description( _("Time display sub filter") );
    add_shortcut( "time" );
vlc_module_end();

/*****************************************************************************
 * CreateFilter: allocates time video filter
 *****************************************************************************/
static int CreateFilter( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;
    filter_sys_t *p_sys;

    /* Allocate structure */
    p_sys = p_filter->p_sys = malloc( sizeof( filter_sys_t ) );
    if( p_sys == NULL )
    {
        msg_Err( p_filter, "out of memory" );
        return VLC_ENOMEM;
    }

    p_sys->p_style = malloc( sizeof( text_style_t ) );
    memcpy( p_sys->p_style, &default_text_style, sizeof( text_style_t ) );

    /* Hook used for callback variables */
    p_sys->i_xoff = var_CreateGetInteger( p_filter->p_libvlc , "time-x" );
    p_sys->i_yoff = var_CreateGetInteger( p_filter->p_libvlc , "time-y" );
    p_sys->psz_format = var_CreateGetString( p_filter->p_libvlc, "time-format" );
    p_sys->i_pos = var_CreateGetInteger( p_filter->p_libvlc , "time-position" );
    
    p_sys->p_style->i_font_alpha = 255 - var_CreateGetInteger( p_filter->p_libvlc , "time-opacity" );
    p_sys->p_style->i_font_color = var_CreateGetInteger( p_filter->p_libvlc , "time-color" );
    p_sys->p_style->i_font_size = var_CreateGetInteger( p_filter->p_libvlc , "time-size" );
   
    var_AddCallback( p_filter->p_libvlc, "time-x", TimeCallback, p_sys );
    var_AddCallback( p_filter->p_libvlc, "time-y", TimeCallback, p_sys );
    var_AddCallback( p_filter->p_libvlc, "time-format", TimeCallback, p_sys );
    var_AddCallback( p_filter->p_libvlc, "time-position", TimeCallback, p_sys );
    var_AddCallback( p_filter->p_libvlc, "time-color", TimeCallback, p_sys );
    var_AddCallback( p_filter->p_libvlc, "time-opacity", TimeCallback, p_sys );
    var_AddCallback( p_filter->p_libvlc, "time-size", TimeCallback, p_sys );

    /* Misc init */
    p_filter->pf_sub_filter = Filter;
    p_sys->last_time = ((time_t)-1);

    return VLC_SUCCESS;
}
/*****************************************************************************
 * DestroyFilter: destroy logo video filter
 *****************************************************************************/
static void DestroyFilter( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;
    filter_sys_t *p_sys = p_filter->p_sys;

    if( p_sys->p_style ) free( p_sys->p_style );
    if( p_sys->psz_format ) free( p_sys->psz_format );
    free( p_sys );

    /* Delete the time variables */
    var_Destroy( p_filter->p_libvlc , "time-format" );
    var_Destroy( p_filter->p_libvlc , "time-x" );
    var_Destroy( p_filter->p_libvlc , "time-y" );
    var_Destroy( p_filter->p_libvlc , "time-position" );
    var_Destroy( p_filter->p_libvlc , "time-color");
    var_Destroy( p_filter->p_libvlc , "time-opacity");
    var_Destroy( p_filter->p_libvlc , "time-size");
}

static char *FormatTime(char *tformat )
{
  char buffer[255];
  time_t curtime;
#if defined(HAVE_LOCALTIME_R)
  struct tm loctime;
#else
  struct tm *loctime;
#endif

  /* Get the current time.  */
  curtime = time( NULL );

  /* Convert it to local time representation.  */
#if defined(HAVE_LOCALTIME_R)
  localtime_r( &curtime, &loctime );
  strftime( buffer, 255, tformat, &loctime );
#else
  loctime = localtime( &curtime );
  strftime( buffer, 255, tformat, loctime );
#endif
  return strdup( buffer );
}

/****************************************************************************
 * Filter: the whole thing
 ****************************************************************************
 * This function outputs subpictures at regular time intervals.
 ****************************************************************************/
static subpicture_t *Filter( filter_t *p_filter, mtime_t date )
{
    filter_sys_t *p_sys = p_filter->p_sys;
    subpicture_t *p_spu;
    video_format_t fmt;

    if( p_sys->last_time == time( NULL ) ) return NULL;

    p_spu = p_filter->pf_sub_buffer_new( p_filter );
    if( !p_spu ) return NULL;

    memset( &fmt, 0, sizeof(video_format_t) );
    fmt.i_chroma = VLC_FOURCC('T','E','X','T');
    fmt.i_aspect = 0;
    fmt.i_width = fmt.i_height = 0;     
    fmt.i_x_offset = 0;
    fmt.i_y_offset = 0;
    p_spu->p_region = p_spu->pf_create_region( VLC_OBJECT(p_filter), &fmt );
    if( !p_spu->p_region )
    {
        p_filter->pf_sub_buffer_del( p_filter, p_spu );
        return NULL;
    }

    p_sys->last_time = time( NULL );

    p_spu->p_region->psz_text = FormatTime( p_sys->psz_format );
    p_spu->i_start = date;
    p_spu->i_stop  = 0;
    p_spu->b_ephemer = VLC_TRUE;

    /*  where to locate the string: */
    if( p_sys->i_xoff < 0 || p_sys->i_yoff < 0 )
    {   /* set to one of the 9 relative locations */
        p_spu->i_flags = p_sys->i_pos;
        p_spu->i_x = 0;
        p_spu->i_y = 0;
        p_spu->b_absolute = VLC_FALSE;
    }
    else
    {   /*  set to an absolute xy, referenced to upper left corner */
        p_spu->i_flags = OSD_ALIGN_LEFT | OSD_ALIGN_TOP;
        p_spu->i_x = p_sys->i_xoff;
        p_spu->i_y = p_sys->i_yoff;
        p_spu->b_absolute = VLC_TRUE;
    }
    p_spu->p_region->p_style = p_sys->p_style;

    return p_spu;
}
/**********************************************************************
 * Callback to update params on the fly
 **********************************************************************/
static int TimeCallback( vlc_object_t *p_this, char const *psz_var,
                            vlc_value_t oldval, vlc_value_t newval,
                            void *p_data )
{
    filter_sys_t *p_sys = (filter_sys_t *) p_data;

    if( !strncmp( psz_var, "time-format", 11 ) )
    {
        if( p_sys->psz_format ) free( p_sys->psz_format );
        p_sys->psz_format = strdup( newval.psz_string );
    }
    else if ( !strncmp( psz_var, "time-x", 6 ) )
    {
        p_sys->i_xoff = newval.i_int;
    }
    else if ( !strncmp( psz_var, "time-y", 6 ) )
    {
        p_sys->i_yoff = newval.i_int;
    }
    else if ( !strncmp( psz_var, "time-color", 8 ) )  /* "time-c" */ 
    {
        p_sys->p_style->i_font_color = newval.i_int;
    }
    else if ( !strncmp( psz_var, "time-opacity", 8 ) ) /* "time-o" */ 
    {
        p_sys->p_style->i_font_alpha = 255 - newval.i_int;
    }
    else if ( !strncmp( psz_var, "time-size", 6 ) )
    {
        p_sys->p_style->i_font_size = newval.i_int;
    }
    else if ( !strncmp( psz_var, "time-position", 8 ) )
    /* willing to accept a match against time-pos */
    {
        p_sys->i_pos = newval.i_int;
        p_sys->i_xoff = -1;       /* force to relative positioning */
    }
    return VLC_SUCCESS;
}
