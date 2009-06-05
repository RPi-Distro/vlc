/*****************************************************************************
 * snapshot.c : snapshot plugin for vlc
 *****************************************************************************
 * Copyright (C) 2002 the VideoLAN team
 * $Id: 2fc7c2af8f89885436fb48481c887723fef1b57a $
 *
 * Authors: Olivier Aubert <oaubert@lisi.univ-lyon1.fr>
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
 * This module is a pseudo video output that offers the possibility to
 * keep a cache of low-res snapshots.
 * The snapshot structure is defined in include/snapshot.h
 * In order to access the current snapshot cache, object variables are used:
 *   vout-snapshot-list-pointer : the pointer on the first element in the list
 *   vout-snapshot-datasize     : size of a snapshot
 *                           (also available in snapshot_t->i_datasize)
 *   vout-snapshot-cache-size   : size of the cache list
 *
 * It is used for the moment by the CORBA module and a specialized
 * python-vlc binding.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_vout.h>
#include <vlc_interface.h>
#include <vlc_input.h>

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Create    ( vlc_object_t * );
static void Destroy   ( vlc_object_t * );

static int  Init      ( vout_thread_t * );
static void End       ( vout_thread_t * );
static void Display   ( vout_thread_t *, picture_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define WIDTH_TEXT N_( "Snapshot width" )
#define WIDTH_LONGTEXT N_( "Width of the snapshot image." )

#define HEIGHT_TEXT N_( "Snapshot height" )
#define HEIGHT_LONGTEXT N_( "Height of the snapshot image." )

#define CHROMA_TEXT N_( "Chroma" )
#define CHROMA_LONGTEXT N_( "Output chroma for the snapshot image " \
                            "(a 4 character string, like \"RV32\")." )

#define CACHE_TEXT N_( "Cache size (number of images)" )
#define CACHE_LONGTEXT N_( "Snapshot cache size (number of images to keep)." )


vlc_module_begin ()
    set_description( N_( "Snapshot output" ) )
    set_shortname( N_("Snapshot") )

    set_category( CAT_VIDEO )
    set_subcategory( SUBCAT_VIDEO_VOUT )
    set_capability( "video output", 1 )

    add_integer( "vout-snapshot-width", 320, NULL, WIDTH_TEXT, WIDTH_LONGTEXT, false )
    add_integer( "vout-snapshot-height", 200, NULL, HEIGHT_TEXT, HEIGHT_LONGTEXT, false )
    add_string( "vout-snapshot-chroma", "RV32", NULL, CHROMA_TEXT, CHROMA_LONGTEXT, true )
        add_deprecated_alias( "snapshot-chroma" )
    add_integer( "vout-snapshot-cache-size", 50, NULL, CACHE_TEXT, CACHE_LONGTEXT, true )
        add_deprecated_alias( "snapshot-cache-size" )

    set_callbacks( Create, Destroy )
vlc_module_end ()

/*****************************************************************************
 * vout_sys_t: video output descriptor
 *****************************************************************************/
typedef struct snapshot_t
{
  uint8_t *p_data;   /* Data area */

  int i_width;       /* In pixels */
  int i_height;      /* In pixels */
  int i_datasize;    /* In bytes */
  mtime_t date;      /* Presentation time */
} snapshot_t;

struct vout_sys_t
{
    snapshot_t **p_list;    /* List of available snapshots */
    int i_index;            /* Index of the next available list member */
    int i_size;             /* Size of the cache */
    int i_datasize;         /* Size of an image */
    input_thread_t *p_input;             /* The input thread */
};

/*****************************************************************************
 * Create: allocates video thread
 *****************************************************************************
 * This function allocates and initializes a vout method.
 *****************************************************************************/
static int Create( vlc_object_t *p_this )
{
    vout_thread_t *p_vout = ( vout_thread_t * )p_this;

    /* Allocate instance and initialize some members */
    p_vout->p_sys = malloc( sizeof( vout_sys_t ) );
    if( ! p_vout->p_sys )
        return VLC_ENOMEM;

    var_Create( p_vout, "vout-snapshot-width", VLC_VAR_INTEGER );
    var_Create( p_vout, "vout-snapshot-height", VLC_VAR_INTEGER );
    var_Create( p_vout, "vout-snapshot-datasize", VLC_VAR_INTEGER );
    var_Create( p_vout, "vout-snapshot-cache-size", VLC_VAR_INTEGER );
    var_Create( p_vout, "vout-snapshot-list-pointer", VLC_VAR_ADDRESS );

    p_vout->pf_init = Init;
    p_vout->pf_end = End;
    p_vout->pf_manage = NULL;
    p_vout->pf_render = NULL;
    p_vout->pf_display = Display;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Init: initialize video thread
 *****************************************************************************/
static int Init( vout_thread_t *p_vout )
{
    int i_index;
    picture_t *p_pic;
    vlc_value_t val;
    char* psz_chroma;
    int i_chroma;
    int i_width;
    int i_height;
    int i_datasize;

    i_width  = config_GetInt( p_vout, "vout-snapshot-width" );
    i_height = config_GetInt( p_vout, "vout-snapshot-height" );

    psz_chroma = config_GetPsz( p_vout, "vout-snapshot-chroma" );
    if( psz_chroma )
    {
        if( strlen( psz_chroma ) < 4 )
        {
            msg_Err( p_vout, "vout-snapshot-chroma should be 4 characters long" );
            return VLC_EGENERIC;
        }
        i_chroma = VLC_FOURCC( psz_chroma[0], psz_chroma[1],
                               psz_chroma[2], psz_chroma[3] );
        free( psz_chroma );
    }
    else
    {
        msg_Err( p_vout, "Cannot find chroma information." );
        return VLC_EGENERIC;
    }

    I_OUTPUTPICTURES = 0;

    /* Initialize the output structure */
    p_vout->output.i_chroma = i_chroma;
    p_vout->output.pf_setpalette = NULL;
    p_vout->output.i_width = i_width;
    p_vout->output.i_height = i_height;
    p_vout->output.i_aspect = p_vout->output.i_width
                               * VOUT_ASPECT_FACTOR / p_vout->output.i_height;


    /* Define the bitmasks */
    switch( i_chroma )
    {
      case VLC_FOURCC( 'R','V','1','5' ):
        p_vout->output.i_rmask = 0x001f;
        p_vout->output.i_gmask = 0x03e0;
        p_vout->output.i_bmask = 0x7c00;
        break;

      case VLC_FOURCC( 'R','V','1','6' ):
        p_vout->output.i_rmask = 0x001f;
        p_vout->output.i_gmask = 0x07e0;
        p_vout->output.i_bmask = 0xf800;
        break;

      case VLC_FOURCC( 'R','V','2','4' ):
        p_vout->output.i_rmask = 0xff0000;
        p_vout->output.i_gmask = 0x00ff00;
        p_vout->output.i_bmask = 0x0000ff;
        break;

      case VLC_FOURCC( 'R','V','3','2' ):
        p_vout->output.i_rmask = 0xff0000;
        p_vout->output.i_gmask = 0x00ff00;
        p_vout->output.i_bmask = 0x0000ff;
        break;
    }

    /* Try to initialize 1 direct buffer */
    p_pic = NULL;

    /* Find an empty picture slot */
    for( i_index = 0 ; i_index < VOUT_MAX_PICTURES ; i_index++ )
    {
        if( p_vout->p_picture[ i_index ].i_status == FREE_PICTURE )
        {
            p_pic = p_vout->p_picture + i_index;
            break;
        }
    }

    /* Allocate the picture */
    if( p_pic == NULL )
    {
        return VLC_SUCCESS;
    }

    vout_AllocatePicture( VLC_OBJECT(p_vout), p_pic, p_vout->output.i_chroma,
                          p_vout->output.i_width, p_vout->output.i_height,
                          p_vout->output.i_aspect );

    if( p_pic->i_planes == 0 )
    {
        return VLC_EGENERIC;
    }

    p_pic->i_status = DESTROYED_PICTURE;
    p_pic->i_type   = DIRECT_PICTURE;

    PP_OUTPUTPICTURE[ I_OUTPUTPICTURES ] = p_pic;

    I_OUTPUTPICTURES++;


    /* Get datasize and set variables */
    i_datasize = i_width * i_height * p_pic->p->i_pixel_pitch;

    p_vout->p_sys->i_datasize = i_datasize;
    p_vout->p_sys->i_index = 0;
    p_vout->p_sys->i_size = config_GetInt( p_vout, "vout-snapshot-cache-size" );

    if( p_vout->p_sys->i_size < 2 )
    {
        msg_Err( p_vout, "vout-snapshot-cache-size must be at least 1." );
        return VLC_EGENERIC;
    }

    p_vout->p_sys->p_list = malloc( p_vout->p_sys->i_size * sizeof( snapshot_t * ) );

    if( p_vout->p_sys->p_list == NULL )
        return VLC_ENOMEM;

    /* Initialize the structures for the circular buffer */
    for( i_index = 0; i_index < p_vout->p_sys->i_size; i_index++ )
    {
        snapshot_t *p_snapshot = malloc( sizeof( snapshot_t ) );

        if( p_snapshot == NULL )
            return VLC_ENOMEM;

        p_snapshot->i_width = i_width;
        p_snapshot->i_height = i_height;
        p_snapshot->i_datasize = i_datasize;
        p_snapshot->date = 0;
        p_snapshot->p_data = malloc( i_datasize );
        if( p_snapshot->p_data == NULL )
        {
            free( p_snapshot );
            return VLC_ENOMEM;
        }
        p_vout->p_sys->p_list[i_index] = p_snapshot;
    }

    val.i_int = i_width;
    var_Set( p_vout, "vout-snapshot-width", val );
    val.i_int = i_height;
    var_Set( p_vout, "vout-snapshot-height", val );
    val.i_int = i_datasize;
    var_Set( p_vout, "vout-snapshot-datasize", val );

    val.i_int = p_vout->p_sys->i_size;
    var_Set( p_vout, "vout-snapshot-cache-size", val );

    val.p_address = p_vout->p_sys->p_list;
    var_Set( p_vout, "vout-snapshot-list-pointer", val );

    /* Get the p_input pointer (to access video times) */
    p_vout->p_sys->p_input = vlc_object_find( p_vout, VLC_OBJECT_INPUT,
                                              FIND_PARENT );

    if( !p_vout->p_sys->p_input )
        return VLC_ENOOBJ;

    if( var_Create( p_vout->p_sys->p_input, "vout-snapshot-id", VLC_VAR_INTEGER ) )
    {
        msg_Err( p_vout, "Cannot create vout-snapshot-id variable in p_input(%p).",
                 p_vout->p_sys->p_input );
        return VLC_EGENERIC;
    }

    /* Register the snapshot vout module at the input level */
    val.p_address = p_vout;

    if( var_Set( p_vout->p_sys->p_input, "vout-snapshot-id", val ) )
    {
        msg_Err( p_vout, "Cannot register vout-snapshot-id in p_input(%p).",
                 p_vout->p_sys->p_input );
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * End: terminate video thread output method
 *****************************************************************************/
static void End( vout_thread_t *p_vout )
{
    (void)p_vout;
}

/*****************************************************************************
 * Destroy: destroy video thread
 *****************************************************************************
 * Terminate an output method created by Create
 *****************************************************************************/
static void Destroy( vlc_object_t *p_this )
{
    vout_thread_t *p_vout = ( vout_thread_t * )p_this;
    int i_index;

    var_Destroy( p_vout->p_sys->p_input, "vout-snapshot-id" );

    vlc_object_release( p_vout->p_sys->p_input );
    var_Destroy( p_this, "vout-snapshot-width" );
    var_Destroy( p_this, "vout-snapshot-height" );
    var_Destroy( p_this, "vout-snapshot-datasize" );

    for( i_index = 0 ; i_index < p_vout->p_sys->i_size ; i_index++ )
    {
        free( p_vout->p_sys->p_list[ i_index ]->p_data );
    }
    free( p_vout->p_sys->p_list );
    /* Destroy structure */
    free( p_vout->p_sys );
}

/* Return the position in ms from the start of the movie */
static mtime_t snapshot_GetMovietime( vout_thread_t *p_vout )
{
    input_thread_t *p_input = p_vout->p_sys->p_input;
    if( !p_input )
        return 0;

    return var_GetTime( p_input, "time" ) / 1000;
}

/*****************************************************************************
 * Display: displays previously rendered output
 *****************************************************************************
 * This function copies the rendered picture into our circular buffer.
 *****************************************************************************/
static void Display( vout_thread_t *p_vout, picture_t *p_pic )
{
    int i_index;
    mtime_t i_date;

    i_index = p_vout->p_sys->i_index;

    vlc_memcpy( p_vout->p_sys->p_list[i_index]->p_data, p_pic->p->p_pixels,
                p_vout->p_sys->i_datasize );

    i_date = snapshot_GetMovietime( p_vout );

    p_vout->p_sys->p_list[i_index]->date = i_date;

    i_index++;

    if( i_index >= p_vout->p_sys->i_size )
    {
        i_index = 0;
    }

    p_vout->p_sys->i_index = i_index;
}

