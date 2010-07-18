/*****************************************************************************
 * fake.c: decoder reading from a fake stream, outputting a fixed image
 *****************************************************************************
 * Copyright (C) 2005-2009 the VideoLAN team
 * $Id: 63d05ee6f3bc14bbe5cf36de2a7050b54ab411e7 $
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
 *          Jean-Paul Saman <jpsaman at m2x dot nl>
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
#include <vlc_codec.h>

#include <vlc_image.h>
#include <vlc_filter.h>

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  OpenDecoder   ( vlc_object_t * );
static void CloseDecoder  ( vlc_object_t * );

static picture_t *DecodeBlock  ( decoder_t *, block_t ** );
static int FakeCallback( vlc_object_t *, char const *,
                         vlc_value_t, vlc_value_t, void * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define FILE_TEXT N_("Image file")
#define FILE_LONGTEXT N_( \
    "Path of the image file for fake input." )
#define RELOAD_TEXT N_("Reload image file")
#define RELOAD_LONGTEXT N_( \
    "Reload image file every n seconds." )
#define WIDTH_TEXT N_("Video width")
#define WIDTH_LONGTEXT N_( \
    "Output video width." )
#define HEIGHT_TEXT N_("Video height")
#define HEIGHT_LONGTEXT N_( \
    "Output video height." )
#define KEEP_AR_TEXT N_("Keep aspect ratio")
#define KEEP_AR_LONGTEXT N_( \
    "Consider width and height as maximum values." )
#define ASPECT_RATIO_TEXT N_("Background aspect ratio")
#define ASPECT_RATIO_LONGTEXT N_( \
    "Aspect ratio of the image file (4:3, 16:9). Default is square pixels." )
#define DEINTERLACE_TEXT N_("Deinterlace video")
#define DEINTERLACE_LONGTEXT N_( \
    "Deinterlace the image after loading it." )
#define DEINTERLACE_MODULE_TEXT N_("Deinterlace module")
#define DEINTERLACE_MODULE_LONGTEXT N_( \
    "Deinterlace module to use." )
#define CHROMA_TEXT N_("Chroma used")
#define CHROMA_LONGTEXT N_( \
    "Force use of a specific chroma for output. Default is I420." )

static const char *const ppsz_deinterlace_type[] =
{
    "deinterlace", "ffmpeg-deinterlace"
};

vlc_module_begin ()
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_VCODEC )
    set_shortname( N_("Fake") )
    set_description( N_("Fake video decoder") )
    set_capability( "decoder", 1000 )
    set_callbacks( OpenDecoder, CloseDecoder )
    add_shortcut( "fake" )

    add_file( "fake-file", "", NULL, FILE_TEXT,
                FILE_LONGTEXT, false )
    add_integer( "fake-file-reload", 0, NULL, RELOAD_TEXT,
                RELOAD_LONGTEXT, false )
    add_integer( "fake-width", 0, NULL, WIDTH_TEXT,
                 WIDTH_LONGTEXT, true )
    add_integer( "fake-height", 0, NULL, HEIGHT_TEXT,
                 HEIGHT_LONGTEXT, true )
    add_bool( "fake-keep-ar", false, NULL, KEEP_AR_TEXT, KEEP_AR_LONGTEXT,
              true )
    add_string( "fake-aspect-ratio", "", NULL,
                ASPECT_RATIO_TEXT, ASPECT_RATIO_LONGTEXT, true )
    add_bool( "fake-deinterlace", false, NULL, DEINTERLACE_TEXT,
              DEINTERLACE_LONGTEXT, false )
    add_string( "fake-deinterlace-module", "deinterlace", NULL,
                DEINTERLACE_MODULE_TEXT, DEINTERLACE_MODULE_LONGTEXT,
                false )
        change_string_list( ppsz_deinterlace_type, 0, 0 )
    add_string( "fake-chroma", "I420", NULL, CHROMA_TEXT, CHROMA_LONGTEXT,
                true )
vlc_module_end ()

struct decoder_sys_t
{
    picture_t *p_image;
    vlc_mutex_t lock;

    bool  b_reload;
    mtime_t     i_reload;
    mtime_t     i_next;
};

/*****************************************************************************
 * OpenDecoder: probe the decoder and return score
 *****************************************************************************/
static int OpenDecoder( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t*)p_this;
    image_handler_t *p_handler;
    video_format_t fmt_in, fmt_out;
    picture_t *p_image;
    char *psz_file, *psz_chroma, *psz_string;
    bool b_keep_ar;
    int i_aspect = 0;
    int i_int;

    if( p_dec->fmt_in.i_codec != VLC_FOURCC('f','a','k','e') )
    {
        return VLC_EGENERIC;
    }

    p_dec->p_sys = calloc( 1, sizeof( *p_dec->p_sys ) );
    if( !p_dec->p_sys )
        return VLC_ENOMEM;

    psz_file = var_CreateGetNonEmptyStringCommand( p_dec, "fake-file" );
    if( !psz_file )
    {
        msg_Err( p_dec, "specify a file with --fake-file=..." );
        free( p_dec->p_sys );
        return VLC_EGENERIC;
    }

    memset( &fmt_in, 0, sizeof(fmt_in) );
    memset( &fmt_out, 0, sizeof(fmt_out) );

    i_int = var_CreateGetIntegerCommand( p_dec, "fake-file-reload" );
    if( i_int > 0)
    {
        p_dec->p_sys->b_reload = true;
        p_dec->p_sys->i_reload = (mtime_t)(i_int * 1000000);
        p_dec->p_sys->i_next   = (mtime_t)(p_dec->p_sys->i_reload + mdate());
    }

    psz_chroma = var_CreateGetString( p_dec, "fake-chroma" );
    fmt_out.i_chroma = vlc_fourcc_GetCodecFromString( VIDEO_ES, psz_chroma );
    if( !fmt_out.i_chroma )
    {
        msg_Warn( p_dec, "Invalid chroma (%s). Using I420.", psz_chroma );
        fmt_out.i_chroma = VLC_CODEC_I420;
    }
    free( psz_chroma );

    var_Create( p_dec, "fake-width", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
    var_Create( p_dec, "fake-height", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
    psz_string = var_CreateGetString( p_dec, "fake-aspect-ratio" );

    if( psz_string )
    {
        char *psz_parser = strchr( psz_string, ':' );

        if( psz_parser )
        {
            *psz_parser++ = '\0';
            i_aspect = atoi( psz_string )
                                   * VOUT_ASPECT_FACTOR / atoi( psz_parser );
        }
        free( psz_string );
    }

    b_keep_ar = var_CreateGetBool( p_dec, "fake-keep-ar" );

    if( !b_keep_ar )
    {
        fmt_out.i_width = var_GetInteger( p_dec, "fake-width" );
        fmt_out.i_height = var_GetInteger( p_dec, "fake-height" );
    }

    p_handler = image_HandlerCreate( p_dec );
    p_image = image_ReadUrl( p_handler, psz_file, &fmt_in, &fmt_out );
    image_HandlerDelete( p_handler );

    if ( p_image == NULL )
    {
        msg_Err( p_dec, "unable to read image file %s", psz_file );
        free( psz_file );
        free( p_dec->p_sys );
        return VLC_EGENERIC;
    }
    msg_Dbg( p_dec, "file %s loaded successfully", psz_file );

    free( psz_file );

    if ( b_keep_ar )
    {
        picture_t *p_old = p_image;
        int i_width, i_height;

        i_width = var_GetInteger( p_dec, "fake-width" );
        i_height = var_GetInteger( p_dec, "fake-height" );

        if ( i_width && i_height )
        {
            int i_image_ar = fmt_out.i_width * VOUT_ASPECT_FACTOR
                              / fmt_out.i_height;
            int i_region_ar = i_width * VOUT_ASPECT_FACTOR / i_height;
            fmt_in = fmt_out;

            if ( i_aspect == i_image_ar )
            {
                fmt_out.i_width = i_width;
                fmt_out.i_height = i_height;
            }
            else if ( i_image_ar > i_region_ar )
            {
                fmt_out.i_width = i_width;
                fmt_out.i_height = i_width * VOUT_ASPECT_FACTOR
                                    / i_image_ar;
                i_aspect = i_image_ar;
            }
            else
            {
                fmt_out.i_height = i_height;
                fmt_out.i_width = i_height * i_image_ar
                                    / VOUT_ASPECT_FACTOR;
                i_aspect = i_image_ar;
            }

            p_handler = image_HandlerCreate( p_dec );
            p_image = image_Convert( p_handler, p_old, &fmt_in, &fmt_out );
            image_HandlerDelete( p_handler );

            if ( p_image == NULL )
            {
                msg_Warn( p_dec, "couldn't load resizing module" );
                p_image = p_old;
                fmt_out = fmt_in;
            }
            else
            {
                picture_Release( p_old );
            }
        }
    }

    if ( i_aspect )
    {
        fmt_out.i_sar_num = i_aspect           * fmt_out.i_height;
        fmt_out.i_sar_den = VOUT_ASPECT_FACTOR * fmt_out.i_width;
    }
    else
    {
        fmt_out.i_sar_num = 1;
        fmt_out.i_sar_den = 1;
    }

    if( var_CreateGetBool( p_dec, "fake-deinterlace" ) )
    {
        picture_t *p_old = p_image;

        psz_string = var_CreateGetString( p_dec, "fake-deinterlace-module" );

        p_handler = image_HandlerCreate( p_dec );
        p_image = image_Filter( p_handler, p_old, &fmt_out, psz_string );
        image_HandlerDelete( p_handler );
        free( psz_string );

        if ( p_image == NULL )
        {
            msg_Warn( p_dec, "couldn't load deinterlace module" );
            p_image = p_old;
        }
        else
        {
            picture_Release( p_old );
        }
    }

    /* Set output properties */
    p_dec->fmt_out.i_cat = VIDEO_ES;
    p_dec->fmt_out.i_codec = fmt_out.i_chroma;
    p_dec->fmt_out.video = fmt_out;

    /* Set callbacks */
    p_dec->pf_decode_video = DecodeBlock;

    p_dec->p_sys->p_image = p_image;
    vlc_mutex_init( &p_dec->p_sys->lock );

    /* Add the callback when every variables are available */
    var_AddCallback( p_dec, "fake-file", FakeCallback, p_dec );
    var_AddCallback( p_dec, "fake-file-reload", FakeCallback , p_dec );

    return VLC_SUCCESS;
}

/****************************************************************************
 * DecodeBlock: the whole thing
 ****************************************************************************/
static picture_t *DecodeBlock( decoder_t *p_dec, block_t **pp_block )
{
    decoder_sys_t *p_sys = (decoder_sys_t*) p_dec->p_sys;
    picture_t *p_pic;

    if( pp_block == NULL || !*pp_block ) return NULL;
    p_pic = decoder_NewPicture( p_dec );
    if( p_pic == NULL )
    {
        msg_Err( p_dec, "cannot get picture" );
        goto error;
    }

    if( p_sys->b_reload && (mdate() >= p_sys->i_next) )
    {
        var_TriggerCallback( p_dec, "fake-file" );
        /* next period */
        p_sys->i_next = (mtime_t)(p_sys->i_reload + mdate());
    }
    vlc_mutex_lock( &p_dec->p_sys->lock );
    picture_Copy( p_pic, p_dec->p_sys->p_image );
    vlc_mutex_unlock( &p_dec->p_sys->lock );

    p_pic->date = (*pp_block)->i_pts;

error:
    block_Release( *pp_block );
    *pp_block = NULL;

    return p_pic;
}

/*****************************************************************************
 * CloseDecoder: fake decoder destruction
 *****************************************************************************/
static void CloseDecoder( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t *)p_this;
    picture_t *p_image = p_dec->p_sys->p_image;

    var_DelCallback( p_dec, "fake-file", FakeCallback, p_dec );
    var_DelCallback( p_dec, "fake-file-reload", FakeCallback , p_dec );

    if( p_image != NULL )
        picture_Release( p_image );

    vlc_mutex_destroy( &p_dec->p_sys->lock );
    free( p_dec->p_sys );
}

/*****************************************************************************
 * FakeCallback: Image source change callback.
 *****************************************************************************/
static int FakeCallback( vlc_object_t *p_this, char const *psz_var,
                         vlc_value_t oldval, vlc_value_t newval,
                         void *p_data )
{
    VLC_UNUSED(p_this); VLC_UNUSED(oldval);
    decoder_t *p_dec = (decoder_t *)p_data;

    if( !strcmp( psz_var, "fake-file" ) )
    {
        image_handler_t *p_handler;
        picture_t *p_new_image;
        video_format_t fmt_in, fmt_out;
        char *psz_file = newval.psz_string;
        picture_t *p_image;

        vlc_mutex_lock( &p_dec->p_sys->lock );
        p_image = p_dec->p_sys->p_image;

        if( !psz_file || !*psz_file )
        {
            msg_Err( p_dec, "fake-file value must be non empty." );
            vlc_mutex_unlock( &p_dec->p_sys->lock );
            return VLC_EGENERIC;
        }
        msg_Dbg( p_dec, "Changing fake-file to %s.", psz_file );

        memset( &fmt_in, 0, sizeof(fmt_in) );
        fmt_out = p_dec->fmt_out.video;
        p_handler = image_HandlerCreate( p_dec );
        p_new_image = image_ReadUrl( p_handler, psz_file, &fmt_in, &fmt_out );
        image_HandlerDelete( p_handler );

        if( !p_new_image )
        {
            msg_Err( p_dec, "error while reading image (%s)", psz_file );
            vlc_mutex_unlock( &p_dec->p_sys->lock );
            return VLC_EGENERIC;
        }

        p_dec->p_sys->p_image = p_new_image;
        picture_Release( p_image );
        vlc_mutex_unlock( &p_dec->p_sys->lock );
    }
    else if( !strcmp( psz_var, "fake-file-reload" ) )
    {
        if( newval.i_int > 0)
        {
            p_dec->p_sys->b_reload = true;
            p_dec->p_sys->i_reload = (mtime_t)(newval.i_int * 1000000);
            p_dec->p_sys->i_next   = (mtime_t)(p_dec->p_sys->i_reload + mdate());
        }
        else
        {
            p_dec->p_sys->b_reload = false;
        }
    }

    return VLC_SUCCESS;
}
