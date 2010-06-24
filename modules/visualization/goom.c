/*****************************************************************************
 * goom.c: based on libgoom (see http://ios.free.fr/?page=projet&quoi=1)
 *****************************************************************************
 * Copyright (C) 2003 the VideoLAN team
 * $Id: cb847e4e6249c7e853e6862223c92e7221b6927d $
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Gildas Bazin <gbazin@videolan.org>
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
#include <vlc_aout.h>
#include <vlc_vout.h>
#include <vlc_block.h>
#include <vlc_input.h>
#include <vlc_filter.h>
#include <vlc_playlist.h>

#ifdef USE_GOOM_TREE
#   ifdef OLD_GOOM
#       include "goom_core.h"
#       define PluginInfo void
#       define goom_update(a,b,c,d,e,f) goom_update(b,c,d,e,f)
#       define goom_close(a) goom_close()
#       define goom_init(a,b) NULL; goom_init(a,b,0); goom_set_font(0,0,0)
#   else
#       include "goom.h"
#   endif
#else
#   include <goom/goom.h>
#endif

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open         ( vlc_object_t * );
static void Close        ( vlc_object_t * );

#define WIDTH_TEXT N_("Goom display width")
#define HEIGHT_TEXT N_("Goom display height")
#define RES_LONGTEXT N_("This allows you to set the resolution of the " \
  "Goom display (bigger resolution will be prettier but more CPU intensive).")

#define SPEED_TEXT N_("Goom animation speed")
#define SPEED_LONGTEXT N_("This allows you to set the animation speed " \
  "(between 1 and 10, defaults to 6).")

#define MAX_SPEED 10

vlc_module_begin ()
    set_shortname( N_("Goom"))
    set_description( N_("Goom effect") )
    set_category( CAT_AUDIO )
    set_subcategory( SUBCAT_AUDIO_VISUAL )
    set_capability( "visualization2", 0 )
    add_integer( "goom-width", 800, NULL,
                 WIDTH_TEXT, RES_LONGTEXT, false )
    add_integer( "goom-height", 640, NULL,
                 HEIGHT_TEXT, RES_LONGTEXT, false )
    add_integer( "goom-speed", 6, NULL,
                 SPEED_TEXT, SPEED_LONGTEXT, false )
    set_callbacks( Open, Close )
    add_shortcut( "goom" )
vlc_module_end ()

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
#define MAX_BLOCKS 100
#define GOOM_DELAY 400000

typedef struct
{
    VLC_COMMON_MEMBERS
    vout_thread_t *p_vout;

    char          *psz_title;

    vlc_mutex_t   lock;
    vlc_cond_t    wait;

    /* Audio properties */
    unsigned i_channels;

    /* Audio samples queue */
    block_t       *pp_blocks[MAX_BLOCKS];
    int           i_blocks;

    date_t        date;

} goom_thread_t;

struct filter_sys_t
{
    goom_thread_t *p_thread;

};

static block_t *DoWork ( filter_t *, block_t * );

static void* Thread   ( vlc_object_t * );

static char *TitleGet( vlc_object_t * );

/*****************************************************************************
 * Open: open a scope effect plugin
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    filter_t       *p_filter = (filter_t *)p_this;
    filter_sys_t   *p_sys;
    goom_thread_t  *p_thread;
    int             width, height;
    video_format_t fmt;


    if( p_filter->fmt_in.audio.i_format != VLC_CODEC_FL32 ||
         p_filter->fmt_out.audio.i_format != VLC_CODEC_FL32 )
    {
        msg_Warn( p_filter, "bad input or output format" );
        return VLC_EGENERIC;
    }
    if( !AOUT_FMTS_SIMILAR( &p_filter->fmt_in.audio, &p_filter->fmt_out.audio ) )
    {
        msg_Warn( p_filter, "input and output formats are not similar" );
        return VLC_EGENERIC;
    }

    p_filter->pf_audio_filter = DoWork;

    /* Allocate structure */
    p_sys = p_filter->p_sys = malloc( sizeof( filter_sys_t ) );

    /* Create goom thread */
    p_sys->p_thread = p_thread =
        vlc_object_create( p_filter, sizeof( goom_thread_t ) );
    vlc_object_attach( p_thread, p_this );

    width = var_CreateGetInteger( p_thread, "goom-width" );
    height = var_CreateGetInteger( p_thread, "goom-height" );

    memset( &fmt, 0, sizeof(video_format_t) );

    fmt.i_width = fmt.i_visible_width = width;
    fmt.i_height = fmt.i_visible_height = height;
    fmt.i_chroma = VLC_CODEC_RGB32;
    fmt.i_sar_num = fmt.i_sar_den = 1;

    p_thread->p_vout = aout_filter_RequestVout( p_filter, NULL, &fmt );
    if( p_thread->p_vout == NULL )
    {
        msg_Err( p_filter, "no suitable vout module" );
        vlc_object_release( p_thread );
        free( p_sys );
        return VLC_EGENERIC;
    }
    vlc_mutex_init( &p_thread->lock );
    vlc_cond_init( &p_thread->wait );

    p_thread->i_blocks = 0;
    date_Init( &p_thread->date, p_filter->fmt_out.audio.i_rate, 1 );
    date_Set( &p_thread->date, 0 );
    p_thread->i_channels = aout_FormatNbChannels( &p_filter->fmt_in.audio );

    p_thread->psz_title = TitleGet( VLC_OBJECT( p_filter ) );

    if( vlc_thread_create( p_thread, "Goom Update Thread", Thread,
                           VLC_THREAD_PRIORITY_LOW ) )
    {
        msg_Err( p_filter, "cannot lauch goom thread" );
        vlc_object_release( p_thread->p_vout );
        vlc_mutex_destroy( &p_thread->lock );
        vlc_cond_destroy( &p_thread->wait );
        free( p_thread->psz_title );
        vlc_object_release( p_thread );
        free( p_sys );
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * DoWork: process samples buffer
 *****************************************************************************
 * This function queues the audio buffer to be processed by the goom thread
 *****************************************************************************/
static block_t *DoWork( filter_t *p_filter, block_t *p_in_buf )
{
    filter_sys_t *p_sys = p_filter->p_sys;
    block_t *p_block;

    /* Queue sample */
    vlc_mutex_lock( &p_sys->p_thread->lock );
    if( p_sys->p_thread->i_blocks == MAX_BLOCKS )
    {
        vlc_mutex_unlock( &p_sys->p_thread->lock );
        return p_in_buf;
    }

    p_block = block_New( p_sys->p_thread, p_in_buf->i_buffer );
    if( !p_block )
    {
        vlc_mutex_unlock( &p_sys->p_thread->lock );
        return p_in_buf;
    }
    memcpy( p_block->p_buffer, p_in_buf->p_buffer, p_in_buf->i_buffer );
    p_block->i_pts = p_in_buf->i_pts;

    p_sys->p_thread->pp_blocks[p_sys->p_thread->i_blocks++] = p_block;

    vlc_cond_signal( &p_sys->p_thread->wait );
    vlc_mutex_unlock( &p_sys->p_thread->lock );

    return p_in_buf;
}

/*****************************************************************************
 * float to s16 conversion
 *****************************************************************************/
static inline int16_t FloatToInt16( float f )
{
    if( f >= 1.0 )
        return 32767;
    else if( f < -1.0 )
        return -32768;
    else
        return (int16_t)( f * 32768.0 );
}

/*****************************************************************************
 * Fill buffer
 *****************************************************************************/
static int FillBuffer( int16_t *p_data, int *pi_data,
                       date_t *pi_date, date_t *pi_date_end,
                       goom_thread_t *p_this )
{
    int i_samples = 0;
    block_t *p_block;

    while( *pi_data < 512 )
    {
        if( !p_this->i_blocks ) return VLC_EGENERIC;

        p_block = p_this->pp_blocks[0];
        i_samples = __MIN( (unsigned)(512 - *pi_data),
                p_block->i_buffer / sizeof(float) / p_this->i_channels );

        /* Date management */
        if( p_block->i_pts > VLC_TS_INVALID &&
            p_block->i_pts != date_Get( pi_date_end ) )
        {
           date_Set( pi_date_end, p_block->i_pts );
        }
        p_block->i_pts = VLC_TS_INVALID;

        date_Increment( pi_date_end, i_samples );

        while( i_samples > 0 )
        {
            float *p_float = (float *)p_block->p_buffer;

            p_data[*pi_data] = FloatToInt16( p_float[0] );
            if( p_this->i_channels > 1 )
                p_data[512 + *pi_data] = FloatToInt16( p_float[1] );

            (*pi_data)++;
            p_block->p_buffer += (sizeof(float) * p_this->i_channels);
            p_block->i_buffer -= (sizeof(float) * p_this->i_channels);
            i_samples--;
        }

        if( !p_block->i_buffer )
        {
            block_Release( p_block );
            p_this->i_blocks--;
            if( p_this->i_blocks )
                memmove( p_this->pp_blocks, p_this->pp_blocks + 1,
                         p_this->i_blocks * sizeof(block_t *) );
        }
    }

    *pi_date = *pi_date_end;
    *pi_data = 0;
    return VLC_SUCCESS;
}

/*****************************************************************************
 * Thread:
 *****************************************************************************/
static void* Thread( vlc_object_t *p_this )
{
    goom_thread_t *p_thread = (goom_thread_t*)p_this;
    int width, height, speed;
    date_t i_pts;
    int16_t p_data[2][512];
    int i_data = 0, i_count = 0;
    PluginInfo *p_plugin_info;
    int canc = vlc_savecancel ();

    width = var_GetInteger( p_this, "goom-width" );
    height = var_GetInteger( p_this, "goom-height" );

    speed = var_CreateGetInteger( p_thread, "goom-speed" );
    speed = MAX_SPEED - speed;
    if( speed < 0 ) speed = 0;

    p_plugin_info = goom_init( width, height );

    while( vlc_object_alive (p_thread) )
    {
        uint32_t  *plane;
        picture_t *p_pic;

        /* goom_update is damn slow, so just copy data and release the lock */
        vlc_mutex_lock( &p_thread->lock );
        if( FillBuffer( (int16_t *)p_data, &i_data, &i_pts,
                        &p_thread->date, p_thread ) != VLC_SUCCESS )
            vlc_cond_wait( &p_thread->wait, &p_thread->lock );
        vlc_mutex_unlock( &p_thread->lock );

        /* Speed selection */
        if( speed && (++i_count % (speed+1)) ) continue;

        /* Frame dropping if necessary */
        if( date_Get( &i_pts ) + GOOM_DELAY <= mdate() ) continue;

        plane = goom_update( p_plugin_info, p_data, 0, 0.0,
                             p_thread->psz_title, NULL );

        free( p_thread->psz_title );
        p_thread->psz_title = NULL;

        while( !( p_pic = vout_CreatePicture( p_thread->p_vout, 0, 0, 0 ) ) &&
               vlc_object_alive (p_thread) )
        {
            msleep( VOUT_OUTMEM_SLEEP );
        }

        if( p_pic == NULL ) break;

        memcpy( p_pic->p[0].p_pixels, plane, width * height * 4 );

        p_pic->date = date_Get( &i_pts ) + GOOM_DELAY;
        vout_DisplayPicture( p_thread->p_vout, p_pic );
    }

    goom_close( p_plugin_info );
    vlc_restorecancel (canc);
    return NULL;
}

/*****************************************************************************
 * Close: close the plugin
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    filter_t     *p_filter = (filter_t *)p_this;
    filter_sys_t *p_sys = p_filter->p_sys;

    /* Stop Goom Thread */
    vlc_object_kill( p_sys->p_thread );

    vlc_mutex_lock( &p_sys->p_thread->lock );
    vlc_cond_signal( &p_sys->p_thread->wait );
    vlc_mutex_unlock( &p_sys->p_thread->lock );

    vlc_thread_join( p_sys->p_thread );

    /* Free data */
    aout_filter_RequestVout( p_filter, p_sys->p_thread->p_vout, 0 );
    vlc_mutex_destroy( &p_sys->p_thread->lock );
    vlc_cond_destroy( &p_sys->p_thread->wait );

    while( p_sys->p_thread->i_blocks-- )
    {
        block_Release( p_sys->p_thread->pp_blocks[p_sys->p_thread->i_blocks] );
    }

    vlc_object_release( p_sys->p_thread );

    free( p_sys );
}

static char *TitleGet( vlc_object_t *p_this )
{
    input_thread_t *p_input = playlist_CurrentInput( pl_Get( p_this ) );
    if( !p_input )
        return NULL;

    char *psz_title = input_item_GetTitle( input_GetItem( p_input ) );
    if( EMPTY_STR( psz_title ) )
    {
        free( psz_title );

        char *psz_uri = input_item_GetURI( input_GetItem( p_input ) );
        const char *psz = strrchr( psz_uri, '/' );
        if( psz )
        {
            psz_title = strdup( psz + 1 );
            free( psz_uri );
        }
        else
            psz_title = psz_uri;
    }
    vlc_object_release( p_input );
    return psz_title;
}
