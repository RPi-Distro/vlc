/*****************************************************************************
 * xcommon.c: Functions common to the X11 and XVideo plugins
 *****************************************************************************
 * Copyright (C) 1998-2006 the VideoLAN team
 * $Id: adc5e791cee026df57ea24f9863b536cff060a3c $
 *
 * Authors: Vincent Seguin <seguin@via.ecp.fr>
 *          Sam Hocevar <sam@zoy.org>
 *          David Kennedy <dkennedy@tinytoad.com>
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
#include <vlc_interface.h>
#include <vlc_playlist.h>
#include <vlc_vout.h>
#include <vlc_window.h>
#include <vlc_keys.h>

#include <errno.h>                                                 /* ENOMEM */

#ifdef HAVE_MACHINE_PARAM_H
    /* BSD */
#   include <machine/param.h>
#   include <sys/types.h>                                  /* typedef ushort */
#   include <sys/ipc.h>
#endif

#ifndef WIN32
#   include <netinet/in.h>                            /* BSD: struct in_addr */
#endif

#ifdef HAVE_XSP
#include <X11/extensions/Xsp.h>
#endif

#ifdef HAVE_SYS_SHM_H
#   include <sys/shm.h>                                /* shmget(), shmctl() */
#endif

#include <X11/Xlib.h>
#include <X11/Xproto.h>
#include <X11/Xmd.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/XF86keysym.h>
#ifdef HAVE_SYS_SHM_H
#   include <X11/extensions/XShm.h>
#endif
#ifdef DPMSINFO_IN_DPMS_H
#   include <X11/extensions/dpms.h>
#endif

#if defined(MODULE_NAME_IS_xvideo) || defined(MODULE_NAME_IS_xvmc)
#   include <X11/extensions/Xv.h>
#   include <X11/extensions/Xvlib.h>
#endif

#ifdef MODULE_NAME_IS_glx
#   include <GL/glx.h>
#endif

#ifdef HAVE_XINERAMA
#   include <X11/extensions/Xinerama.h>
#endif

#ifdef HAVE_X11_EXTENSIONS_XF86VMODE_H
#   include <X11/extensions/xf86vmode.h>
#endif

#ifdef MODULE_NAME_IS_xvmc
#   include <X11/extensions/vldXvMC.h>
#   include "../../codec/xvmc/accel_xvmc.h"
#endif

#include "xcommon.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
int  Activate   ( vlc_object_t * );
void Deactivate ( vlc_object_t * );

static int  InitVideo      ( vout_thread_t * );
static void EndVideo       ( vout_thread_t * );
static void DisplayVideo   ( vout_thread_t *, picture_t * );
static int  ManageVideo    ( vout_thread_t * );
static int  Control        ( vout_thread_t *, int, va_list );

static int  InitDisplay    ( vout_thread_t * );

static int  CreateWindow   ( vout_thread_t *, x11_window_t * );
static void DestroyWindow  ( vout_thread_t *, x11_window_t * );

static int  NewPicture     ( vout_thread_t *, picture_t * );
static void FreePicture    ( vout_thread_t *, picture_t * );

#ifdef HAVE_SYS_SHM_H
static int i_shm_major = 0;
#endif

static void ToggleFullScreen      ( vout_thread_t * );

static void EnableXScreenSaver    ( vout_thread_t * );
static void DisableXScreenSaver   ( vout_thread_t * );

static void CreateCursor   ( vout_thread_t * );
static void DestroyCursor  ( vout_thread_t * );
static void ToggleCursor   ( vout_thread_t * );

#if defined(MODULE_NAME_IS_xvideo) || defined(MODULE_NAME_IS_xvmc)
static int  XVideoGetPort    ( vout_thread_t *, vlc_fourcc_t, picture_heap_t * );
static void XVideoReleasePort( vout_thread_t *, int );
#endif

#ifdef MODULE_NAME_IS_x11
static void SetPalette     ( vout_thread_t *,
                             uint16_t *, uint16_t *, uint16_t * );
#endif

#ifdef MODULE_NAME_IS_xvmc
static void RenderVideo    ( vout_thread_t *, picture_t * );
static int  xvmc_check_yv12( Display *display, XvPortID port );
static void xvmc_update_XV_DOUBLE_BUFFER( vout_thread_t *p_vout );
#endif

static void TestNetWMSupport( vout_thread_t * );
static int ConvertKey( int );

static int WindowOnTop( vout_thread_t *, bool );

static int X11ErrorHandler( Display *, XErrorEvent * );

#ifdef HAVE_XSP
static void EnablePixelDoubling( vout_thread_t *p_vout );
static void DisablePixelDoubling( vout_thread_t *p_vout );
#endif

#ifdef HAVE_OSSO
static const int i_backlight_on_interval = 300;
#endif



/*****************************************************************************
 * Activate: allocate X11 video thread output method
 *****************************************************************************
 * This function allocate and initialize a X11 vout method. It uses some of the
 * vout properties to choose the window size, and change them according to the
 * actual properties of the display.
 *****************************************************************************/
int Activate ( vlc_object_t *p_this )
{
    vout_thread_t *p_vout = (vout_thread_t *)p_this;
    char *        psz_display;
#if defined(MODULE_NAME_IS_xvmc)
    char *psz_value;
#endif
#if defined(MODULE_NAME_IS_xvideo) || defined(MODULE_NAME_IS_xvmc)
    char *       psz_chroma;
    vlc_fourcc_t i_chroma = 0;
    bool   b_chroma = 0;
#endif

    p_vout->pf_init = InitVideo;
    p_vout->pf_end = EndVideo;
    p_vout->pf_manage = ManageVideo;
#ifdef MODULE_NAME_IS_xvmc
    p_vout->pf_render = RenderVideo;
#else
    p_vout->pf_render = NULL;
#endif
    p_vout->pf_display = DisplayVideo;
    p_vout->pf_control = Control;

    /* Allocate structure */
    p_vout->p_sys = malloc( sizeof( vout_sys_t ) );
    if( p_vout->p_sys == NULL )
        return VLC_ENOMEM;

    /* key and mouse event handling */
    p_vout->p_sys->i_vout_event = var_CreateGetInteger( p_vout, "vout-event" );

    /* Open display, using the "display" config variable or the DISPLAY
     * environment variable */
    psz_display = config_GetPsz( p_vout, MODULE_STRING "-display" );

    p_vout->p_sys->p_display = XOpenDisplay( psz_display );

    if( p_vout->p_sys->p_display == NULL )                         /* error */
    {
        msg_Err( p_vout, "cannot open display %s",
                         XDisplayName( psz_display ) );
        free( p_vout->p_sys );
        free( psz_display );
        return VLC_EGENERIC;
    }
    free( psz_display );

    /* Replace error handler so we can intercept some non-fatal errors */
    XSetErrorHandler( X11ErrorHandler );

    /* Get a screen ID matching the XOpenDisplay return value */
    p_vout->p_sys->i_screen = DefaultScreen( p_vout->p_sys->p_display );

#if defined(MODULE_NAME_IS_xvideo) || defined(MODULE_NAME_IS_xvmc)
    psz_chroma = config_GetPsz( p_vout, "xvideo-chroma" );
    if( psz_chroma )
    {
        if( strlen( psz_chroma ) >= 4 )
        {
            /* Do not use direct assignment because we are not sure of the
             * alignment. */
            memcpy(&i_chroma, psz_chroma, 4);
            b_chroma = 1;
        }

        free( psz_chroma );
    }

    if( b_chroma )
    {
        msg_Dbg( p_vout, "forcing chroma 0x%.8x (%4.4s)",
                 i_chroma, (char*)&i_chroma );
    }
    else
    {
        i_chroma = p_vout->render.i_chroma;
    }

    /* Check that we have access to an XVideo port providing this chroma */
    p_vout->p_sys->i_xvport = XVideoGetPort( p_vout, VLC2X11_FOURCC(i_chroma),
                                             &p_vout->output );
    if( p_vout->p_sys->i_xvport < 0 )
    {
        /* If a specific chroma format was requested, then we don't try to
         * be cleverer than the user. He knew pretty well what he wanted. */
        if( b_chroma )
        {
            XCloseDisplay( p_vout->p_sys->p_display );
            free( p_vout->p_sys );
            return VLC_EGENERIC;
        }

        /* It failed, but it's not completely lost ! We try to open an
         * XVideo port for an YUY2 picture. We'll need to do an YUV
         * conversion, but at least it has got scaling. */
        p_vout->p_sys->i_xvport =
                        XVideoGetPort( p_vout, X11_FOURCC('Y','U','Y','2'),
                                               &p_vout->output );
        if( p_vout->p_sys->i_xvport < 0 )
        {
            /* It failed, but it's not completely lost ! We try to open an
             * XVideo port for a simple 16bpp RGB picture. We'll need to do
             * an YUV conversion, but at least it has got scaling. */
            p_vout->p_sys->i_xvport =
                            XVideoGetPort( p_vout, X11_FOURCC('R','V','1','6'),
                                                   &p_vout->output );
            if( p_vout->p_sys->i_xvport < 0 )
            {
                XCloseDisplay( p_vout->p_sys->p_display );
                free( p_vout->p_sys );
                return VLC_EGENERIC;
            }
        }
    }
    p_vout->output.i_chroma = X112VLC_FOURCC(p_vout->output.i_chroma);
#elif defined(MODULE_NAME_IS_glx)
    {
        int i_opcode, i_evt, i_err = 0;
        int i_maj, i_min = 0;

        /* Check for GLX extension */
        if( !XQueryExtension( p_vout->p_sys->p_display, "GLX",
                              &i_opcode, &i_evt, &i_err ) )
        {
            msg_Err( p_this, "GLX extension not supported" );
            XCloseDisplay( p_vout->p_sys->p_display );
            free( p_vout->p_sys );
            return VLC_EGENERIC;
        }
        if( !glXQueryExtension( p_vout->p_sys->p_display, &i_err, &i_evt ) )
        {
            msg_Err( p_this, "glXQueryExtension failed" );
            XCloseDisplay( p_vout->p_sys->p_display );
            free( p_vout->p_sys );
            return VLC_EGENERIC;
        }

        /* Check GLX version */
        if (!glXQueryVersion( p_vout->p_sys->p_display, &i_maj, &i_min ) )
        {
            msg_Err( p_this, "glXQueryVersion failed" );
            XCloseDisplay( p_vout->p_sys->p_display );
            free( p_vout->p_sys );
            return VLC_EGENERIC;
        }
        if( i_maj <= 0 || ((i_maj == 1) && (i_min < 3)) )
        {
            p_vout->p_sys->b_glx13 = false;
            msg_Dbg( p_this, "using GLX 1.2 API" );
        }
        else
        {
            p_vout->p_sys->b_glx13 = true;
            msg_Dbg( p_this, "using GLX 1.3 API" );
        }
    }
#endif

    /* Create blank cursor (for mouse cursor autohiding) */
    p_vout->p_sys->i_time_mouse_last_moved = mdate();
    p_vout->p_sys->i_mouse_hide_timeout =
        var_GetInteger(p_vout, "mouse-hide-timeout") * 1000;
    p_vout->p_sys->b_mouse_pointer_visible = 1;
    CreateCursor( p_vout );

    /* Set main window's size */
    p_vout->p_sys->original_window.i_width = p_vout->i_window_width;
    p_vout->p_sys->original_window.i_height = p_vout->i_window_height;
    var_Create( p_vout, "video-title", VLC_VAR_STRING | VLC_VAR_DOINHERIT );
    /* Spawn base window - this window will include the video output window,
     * but also command buttons, subtitles and other indicators */
    if( CreateWindow( p_vout, &p_vout->p_sys->original_window ) )
    {
        msg_Err( p_vout, "cannot create X11 window" );
        DestroyCursor( p_vout );
        XCloseDisplay( p_vout->p_sys->p_display );
        free( p_vout->p_sys );
        return VLC_EGENERIC;
    }

    /* Open and initialize device. */
    if( InitDisplay( p_vout ) )
    {
        msg_Err( p_vout, "cannot initialize X11 display" );
        DestroyCursor( p_vout );
        DestroyWindow( p_vout, &p_vout->p_sys->original_window );
        XCloseDisplay( p_vout->p_sys->p_display );
        free( p_vout->p_sys );
        return VLC_EGENERIC;
    }

    /* Disable screen saver */
    DisableXScreenSaver( p_vout );

    /* Misc init */
    p_vout->p_sys->b_altfullscreen = 0;
    p_vout->p_sys->i_time_button_last_pressed = 0;

    TestNetWMSupport( p_vout );

#ifdef MODULE_NAME_IS_xvmc
    p_vout->p_sys->p_last_subtitle_save = NULL;
    psz_value = config_GetPsz( p_vout, "xvmc-deinterlace-mode" );

    /* Look what method was requested */
    //var_Create( p_vout, "xvmc-deinterlace-mode", VLC_VAR_STRING );
    //var_Change( p_vout, "xvmc-deinterlace-mode", VLC_VAR_INHERITVALUE, &val, NULL );
    if( psz_value )
    {
        if( (strcmp(psz_value, "bob") == 0) ||
            (strcmp(psz_value, "blend") == 0) )
           p_vout->p_sys->xvmc_deinterlace_method = 2;
        else if (strcmp(psz_value, "discard") == 0)
           p_vout->p_sys->xvmc_deinterlace_method = 1;
        else
           p_vout->p_sys->xvmc_deinterlace_method = 0;
        free(psz_value );
    }
    else
        p_vout->p_sys->xvmc_deinterlace_method = 0;

    /* Look what method was requested */
    //var_Create( p_vout, "xvmc-crop-style", VLC_VAR_STRING );
    //var_Change( p_vout, "xvmc-crop-style", VLC_VAR_INHERITVALUE, &val, NULL );
    psz_value = config_GetPsz( p_vout, "xvmc-crop-style" );

    if( psz_value )
    {
        if( strncmp( psz_value, "eq", 2 ) == 0 )
           p_vout->p_sys->xvmc_crop_style = 1;
        else if( strncmp( psz_value, "4-16", 4 ) == 0)
           p_vout->p_sys->xvmc_crop_style = 2;
        else if( strncmp( psz_value, "16-4", 4 ) == 0)
           p_vout->p_sys->xvmc_crop_style = 3;
        else
           p_vout->p_sys->xvmc_crop_style = 0;
        free( psz_value );
    }
    else
        p_vout->p_sys->xvmc_crop_style = 0;

    msg_Dbg(p_vout, "Deinterlace = %d", p_vout->p_sys->xvmc_deinterlace_method);
    msg_Dbg(p_vout, "Crop = %d", p_vout->p_sys->xvmc_crop_style);

    if( checkXvMCCap( p_vout ) == VLC_EGENERIC )
    {
        msg_Err( p_vout, "no XVMC capability found" );
        Deactivate( p_this );
        return VLC_EGENERIC;
    }
    subpicture_t sub_pic;
    sub_pic.p_sys = NULL;
    p_vout->p_sys->last_date = 0;
#endif

#ifdef HAVE_XSP
    p_vout->p_sys->i_hw_scale = 1;
#endif

#ifdef HAVE_OSSO
    p_vout->p_sys->i_backlight_on_counter = i_backlight_on_interval;
    p_vout->p_sys->p_octx = osso_initialize( "vlc", VERSION, 0, NULL );
    if ( p_vout->p_sys->p_octx == NULL ) {
        msg_Err( p_vout, "Could not get osso context" );
    } else {
        msg_Dbg( p_vout, "Initialized osso context" );
    }
#endif

    /* Variable to indicate if the window should be on top of others */
    /* Trigger a callback right now */
    var_TriggerCallback( p_vout, "video-on-top" );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Deactivate: destroy X11 video thread output method
 *****************************************************************************
 * Terminate an output method created by Open
 *****************************************************************************/
void Deactivate ( vlc_object_t *p_this )
{
    vout_thread_t *p_vout = (vout_thread_t *)p_this;

    /* If the fullscreen window is still open, close it */
    if( p_vout->b_fullscreen )
    {
        ToggleFullScreen( p_vout );
    }

    /* Restore cursor if it was blanked */
    if( !p_vout->p_sys->b_mouse_pointer_visible )
    {
        ToggleCursor( p_vout );
    }

#ifdef MODULE_NAME_IS_x11
    /* Destroy colormap */
    if( XDefaultDepth(p_vout->p_sys->p_display, p_vout->p_sys->i_screen) == 8 )
    {
        XFreeColormap( p_vout->p_sys->p_display, p_vout->p_sys->colormap );
    }
#elif defined(MODULE_NAME_IS_xvideo)
    XVideoReleasePort( p_vout, p_vout->p_sys->i_xvport );
#elif defined(MODULE_NAME_IS_xvmc)
    if( p_vout->p_sys->xvmc_cap )
    {
        xvmc_context_writer_lock( &p_vout->p_sys->xvmc_lock );
        xxmc_dispose_context( p_vout );
        if( p_vout->p_sys->old_subpic )
        {
            xxmc_xvmc_free_subpicture( p_vout, p_vout->p_sys->old_subpic );
            p_vout->p_sys->old_subpic = NULL;
        }
        if( p_vout->p_sys->new_subpic )
        {
            xxmc_xvmc_free_subpicture( p_vout, p_vout->p_sys->new_subpic );
            p_vout->p_sys->new_subpic = NULL;
        }
        free( p_vout->p_sys->xvmc_cap );
        xvmc_context_writer_unlock( &p_vout->p_sys->xvmc_lock );
    }
#endif

#ifdef HAVE_XSP
    DisablePixelDoubling(p_vout);
#endif

    DestroyCursor( p_vout );
    EnableXScreenSaver( p_vout );
    DestroyWindow( p_vout, &p_vout->p_sys->original_window );
    XCloseDisplay( p_vout->p_sys->p_display );

    /* Destroy structure */
#ifdef MODULE_NAME_IS_xvmc
    free_context_lock( &p_vout->p_sys->xvmc_lock );
#endif

#ifdef HAVE_OSSO
    if ( p_vout->p_sys->p_octx != NULL ) {
        msg_Dbg( p_vout, "Deinitializing osso context" );
        osso_deinitialize( p_vout->p_sys->p_octx );
    }
#endif

    free( p_vout->p_sys );
}

#ifdef MODULE_NAME_IS_xvmc

#define XINE_IMGFMT_YV12 (('2'<<24)|('1'<<16)|('V'<<8)|'Y')

/* called xlocked */
static int xvmc_check_yv12( Display *display, XvPortID port )
{
    XvImageFormatValues *formatValues;
    int                  formats;
    int                  i;

    formatValues = XvListImageFormats( display, port, &formats );

    for( i = 0; i < formats; i++ )
    {
        if( ( formatValues[i].id == XINE_IMGFMT_YV12 ) &&
            ( !( strncmp( formatValues[i].guid, "YV12", 4 ) ) ) )
        {
            XFree (formatValues);
            return 0;
        }
    }

    XFree (formatValues);
    return 1;
}

static void xvmc_sync_surface( vout_thread_t *p_vout, XvMCSurface * srf )
{
    XvMCSyncSurface( p_vout->p_sys->p_display, srf );
}

static void xvmc_update_XV_DOUBLE_BUFFER( vout_thread_t *p_vout )
{
    Atom         atom;
    int          xv_double_buffer;

    xv_double_buffer = 1;

    XLockDisplay( p_vout->p_sys->p_display );
    atom = XInternAtom( p_vout->p_sys->p_display, "XV_DOUBLE_BUFFER", False );
#if 0
    XvSetPortAttribute (p_vout->p_sys->p_display, p_vout->p_sys->i_xvport, atom, xv_double_buffer);
#endif
    XvMCSetAttribute( p_vout->p_sys->p_display, &p_vout->p_sys->context, atom, xv_double_buffer );
    XUnlockDisplay( p_vout->p_sys->p_display );

    //xprintf(this->xine, XINE_VERBOSITY_DEBUG,
    //    "video_out_xxmc: double buffering mode = %d\n", xv_double_buffer);
}

static void RenderVideo( vout_thread_t *p_vout, picture_t *p_pic )
{
    vlc_xxmc_t *xxmc = NULL;

    xvmc_context_reader_lock( &p_vout->p_sys->xvmc_lock );

    xxmc = &p_pic->p_sys->xxmc_data;
    if( (!xxmc->decoded ||
        !xxmc_xvmc_surface_valid( p_vout, p_pic->p_sys->xvmc_surf )) )
    {
        xvmc_context_reader_unlock( &p_vout->p_sys->xvmc_lock );
        return;
    }

#if 0
    vlc_mutex_lock( &p_vout->lastsubtitle_lock );
    if (p_vout->p_sys->p_last_subtitle != NULL)
    {
        if( p_vout->p_sys->p_last_subtitle_save != p_vout->p_sys->p_last_subtitle )
        {
            p_vout->p_sys->new_subpic =
                xxmc_xvmc_alloc_subpicture( p_vout, &p_vout->p_sys->context,
                    p_vout->p_sys->xvmc_width,
                    p_vout->p_sys->xvmc_height,
                    p_vout->p_sys->xvmc_cap[p_vout->p_sys->xvmc_cur_cap].subPicType.id );

            if (p_vout->p_sys->new_subpic)
            {
                XVMCLOCKDISPLAY( p_vout->p_sys->p_display );
                XvMCClearSubpicture( p_vout->p_sys->p_display,
                        p_vout->p_sys->new_subpic,
                        0,
                        0,
                        p_vout->p_sys->xvmc_width,
                        p_vout->p_sys->xvmc_height,
                        0x00 );
                XVMCUNLOCKDISPLAY( p_vout->p_sys->p_display );
                clear_xx44_palette( &p_vout->p_sys->palette );

                if( sub_pic.p_sys == NULL )
                {
                    sub_pic.p_sys = malloc( sizeof( picture_sys_t ) );
                    if( sub_pic.p_sys != NULL )
                    {
                        sub_pic.p_sys->p_vout = p_vout;
                        sub_pic.p_sys->xvmc_surf = NULL;
                        sub_pic.p_sys->p_image = p_vout->p_sys->subImage;
                    }
                }
                sub_pic.p_sys->p_image = p_vout->p_sys->subImage;
                sub_pic.p->p_pixels = sub_pic.p_sys->p_image->data;
                sub_pic.p->i_pitch = p_vout->output.i_width;

                memset( p_vout->p_sys->subImage->data, 0,
                        (p_vout->p_sys->subImage->width * p_vout->p_sys->subImage->height) );

                if (p_vout->p_last_subtitle != NULL)
                {
                    blend_xx44( p_vout->p_sys->subImage->data,
                                p_vout->p_last_subtitle,
                                p_vout->p_sys->subImage->width,
                                p_vout->p_sys->subImage->height,
                                p_vout->p_sys->subImage->width,
                                &p_vout->p_sys->palette,
                                (p_vout->p_sys->subImage->id == FOURCC_IA44) );
                }

                XVMCLOCKDISPLAY( p_vout->p_sys->p_display );
                XvMCCompositeSubpicture( p_vout->p_sys->p_display,
                                         p_vout->p_sys->new_subpic,
                                         p_vout->p_sys->subImage,
                                         0, /* overlay->x */
                                         0, /* overlay->y */
                                         p_vout->output.i_width, /* overlay->width, */
                                         p_vout->output.i_height, /* overlay->height */
                                         0, /* overlay->x */
                                         0 ); /*overlay->y */
                XVMCUNLOCKDISPLAY( p_vout->p_sys->p_display );
                if (p_vout->p_sys->old_subpic)
                {
                    xxmc_xvmc_free_subpicture( p_vout,
                                               p_vout->p_sys->old_subpic);
                    p_vout->p_sys->old_subpic = NULL;
                }
                if (p_vout->p_sys->new_subpic)
                {
                    p_vout->p_sys->old_subpic = p_vout->p_sys->new_subpic;
                    p_vout->p_sys->new_subpic = NULL;
                    xx44_to_xvmc_palette( &p_vout->p_sys->palette,
                            p_vout->p_sys->xvmc_palette,
                            0,
                            p_vout->p_sys->old_subpic->num_palette_entries,
                            p_vout->p_sys->old_subpic->entry_bytes,
                            p_vout->p_sys->old_subpic->component_order );
                    XVMCLOCKDISPLAY( p_vout->p_sys->p_display );
                    XvMCSetSubpicturePalette( p_vout->p_sys->p_display,
                                              p_vout->p_sys->old_subpic,
                                              p_vout->p_sys->xvmc_palette );
                    XvMCFlushSubpicture( p_vout->p_sys->p_display,
                                         p_vout->p_sys->old_subpic);
                    XvMCSyncSubpicture( p_vout->p_sys->p_display,
                                        p_vout->p_sys->old_subpic );
                    XVMCUNLOCKDISPLAY( p_vout->p_sys->p_display );
                }

                XVMCLOCKDISPLAY( p_vout->p_sys->p_display);
                if (p_vout->p_sys->xvmc_backend_subpic )
                {
                    XvMCBlendSubpicture( p_vout->p_sys->p_display,
                                         p_pic->p_sys->xvmc_surf,
                                         p_vout->p_sys->old_subpic,
                                         0,
                                         0,
                                         p_vout->p_sys->xvmc_width,
                                         p_vout->p_sys->xvmc_height,
                                         0,
                                         0,
                                         p_vout->p_sys->xvmc_width,
                                         p_vout->p_sys->xvmc_height );
                }
                else
                {
                    XvMCBlendSubpicture2( p_vout->p_sys->p_display,
                                          p_pic->p_sys->xvmc_surf,
                                          p_pic->p_sys->xvmc_surf,
                                          p_vout->p_sys->old_subpic,
                                          0,
                                          0,
                                          p_vout->p_sys->xvmc_width,
                                          p_vout->p_sys->xvmc_height,
                                          0,
                                          0,
                                          p_vout->p_sys->xvmc_width,
                                          p_vout->p_sys->xvmc_height );
               }
               XVMCUNLOCKDISPLAY(p_vout->p_sys->p_display);
            }
        }
        else
        {
            XVMCLOCKDISPLAY( p_vout->p_sys->p_display );
            if( p_vout->p_sys->xvmc_backend_subpic )
            {
                XvMCBlendSubpicture( p_vout->p_sys->p_display,
                                     p_pic->p_sys->xvmc_surf,
                                     p_vout->p_sys->old_subpic,
                                     0, 0,
                                     p_vout->p_sys->xvmc_width,
                                     p_vout->p_sys->xvmc_height,
                                     0, 0,
                                     p_vout->p_sys->xvmc_width,
                                     p_vout->p_sys->xvmc_height );
            }
            else
            {
                XvMCBlendSubpicture2( p_vout->p_sys->p_display,
                                      p_pic->p_sys->xvmc_surf,
                                      p_pic->p_sys->xvmc_surf,
                                      p_vout->p_sys->old_subpic,
                                      0, 0,
                                      p_vout->p_sys->xvmc_width,
                                      p_vout->p_sys->xvmc_height,
                                      0, 0,
                                      p_vout->p_sys->xvmc_width,
                                      p_vout->p_sys->xvmc_height );
            }
            XVMCUNLOCKDISPLAY( p_vout->p_sys->p_display );
        }
    }
    p_vout->p_sys->p_last_subtitle_save = p_vout->p_last_subtitle;

    vlc_mutex_unlock( &p_vout->lastsubtitle_lock );
#endif
    xvmc_context_reader_unlock( &p_vout->p_sys->xvmc_lock );
}
#endif

#ifdef HAVE_XSP
/*****************************************************************************
 * EnablePixelDoubling: Enables pixel doubling
 *****************************************************************************
 * Checks if the double size image fits in current window, and enables pixel
 * doubling accordingly. The i_hw_scale is the integer scaling factor.
 *****************************************************************************/
static void EnablePixelDoubling( vout_thread_t *p_vout )
{
    int i_hor_scale = ( p_vout->p_sys->p_win->i_width ) / p_vout->render.i_width;
    int i_vert_scale =  ( p_vout->p_sys->p_win->i_height ) / p_vout->render.i_height;
    if ( ( i_hor_scale > 1 ) && ( i_vert_scale > 1 ) ) {
        p_vout->p_sys->i_hw_scale = 2;
        msg_Dbg( p_vout, "Enabling pixel doubling, scaling factor %d", p_vout->p_sys->i_hw_scale );
        XSPSetPixelDoubling( p_vout->p_sys->p_display, 0, 1 );
    }
}

/*****************************************************************************
 * DisablePixelDoubling: Disables pixel doubling
 *****************************************************************************
 * The scaling factor i_hw_scale is reset to the no-scaling value 1.
 *****************************************************************************/
static void DisablePixelDoubling( vout_thread_t *p_vout )
{
    if ( p_vout->p_sys->i_hw_scale > 1 ) {
        msg_Dbg( p_vout, "Disabling pixel doubling" );
        XSPSetPixelDoubling( p_vout->p_sys->p_display, 0, 0 );
        p_vout->p_sys->i_hw_scale = 1;
    }
}
#endif



/*****************************************************************************
 * InitVideo: initialize X11 video thread output method
 *****************************************************************************
 * This function create the XImages needed by the output thread. It is called
 * at the beginning of the thread, but also each time the window is resized.
 *****************************************************************************/
static int InitVideo( vout_thread_t *p_vout )
{
    unsigned int i_index = 0;
    picture_t *p_pic;

    I_OUTPUTPICTURES = 0;

#if defined(MODULE_NAME_IS_xvideo) || defined(MODULE_NAME_IS_xvmc)
    /* Initialize the output structure; we already found an XVideo port,
     * and the corresponding chroma we will be using. Since we can
     * arbitrary scale, stick to the coordinates and aspect. */
    p_vout->output.i_width  = p_vout->render.i_width;
    p_vout->output.i_height = p_vout->render.i_height;
    p_vout->output.i_aspect = p_vout->render.i_aspect;

    p_vout->fmt_out = p_vout->fmt_in;
    p_vout->fmt_out.i_chroma = p_vout->output.i_chroma;

#if XvVersion < 2 || ( XvVersion == 2 && XvRevision < 2 )
    switch( p_vout->output.i_chroma )
    {
        case VLC_FOURCC('R','V','1','6'):
#if defined( WORDS_BIGENDIAN )
            p_vout->output.i_rmask = 0xf800;
            p_vout->output.i_gmask = 0x07e0;
            p_vout->output.i_bmask = 0x001f;
#else
            p_vout->output.i_rmask = 0x001f;
            p_vout->output.i_gmask = 0x07e0;
            p_vout->output.i_bmask = 0xf800;
#endif
            break;
        case VLC_FOURCC('R','V','1','5'):
#if defined( WORDS_BIGENDIAN )
            p_vout->output.i_rmask = 0x7c00;
            p_vout->output.i_gmask = 0x03e0;
            p_vout->output.i_bmask = 0x001f;
#else
            p_vout->output.i_rmask = 0x001f;
            p_vout->output.i_gmask = 0x03e0;
            p_vout->output.i_bmask = 0x7c00;
#endif
            break;
    }
#endif

#elif defined(MODULE_NAME_IS_x11)
    /* Initialize the output structure: RGB with square pixels, whatever
     * the input format is, since it's the only format we know */
    switch( p_vout->p_sys->i_screen_depth )
    {
        case 8: /* FIXME: set the palette */
            p_vout->output.i_chroma = VLC_FOURCC('R','G','B','2'); break;
        case 15:
            p_vout->output.i_chroma = VLC_FOURCC('R','V','1','5'); break;
        case 16:
            p_vout->output.i_chroma = VLC_FOURCC('R','V','1','6'); break;
        case 24:
        case 32:
            p_vout->output.i_chroma = VLC_FOURCC('R','V','3','2'); break;
        default:
            msg_Err( p_vout, "unknown screen depth %i",
                     p_vout->p_sys->i_screen_depth );
            return VLC_SUCCESS;
    }

#ifdef HAVE_XSP
    vout_PlacePicture( p_vout, p_vout->p_sys->p_win->i_width  / p_vout->p_sys->i_hw_scale,
                       p_vout->p_sys->p_win->i_height  / p_vout->p_sys->i_hw_scale,
                       &i_index, &i_index,
                       &p_vout->fmt_out.i_visible_width,
                       &p_vout->fmt_out.i_visible_height );
#else
    vout_PlacePicture( p_vout, p_vout->p_sys->p_win->i_width,
                       p_vout->p_sys->p_win->i_height,
                       &i_index, &i_index,
                       &p_vout->fmt_out.i_visible_width,
                       &p_vout->fmt_out.i_visible_height );
#endif

    p_vout->fmt_out.i_chroma = p_vout->output.i_chroma;

    p_vout->output.i_width = p_vout->fmt_out.i_width =
        p_vout->fmt_out.i_visible_width * p_vout->fmt_in.i_width /
        p_vout->fmt_in.i_visible_width;
    p_vout->output.i_height = p_vout->fmt_out.i_height =
        p_vout->fmt_out.i_visible_height * p_vout->fmt_in.i_height /
        p_vout->fmt_in.i_visible_height;
    p_vout->fmt_out.i_x_offset =
        p_vout->fmt_out.i_visible_width * p_vout->fmt_in.i_x_offset /
        p_vout->fmt_in.i_visible_width;
    p_vout->fmt_out.i_y_offset =
        p_vout->fmt_out.i_visible_height * p_vout->fmt_in.i_y_offset /
        p_vout->fmt_in.i_visible_height;

    p_vout->fmt_out.i_sar_num = p_vout->fmt_out.i_sar_den = 1;
    p_vout->output.i_aspect = p_vout->fmt_out.i_aspect =
        p_vout->fmt_out.i_width * VOUT_ASPECT_FACTOR /p_vout->fmt_out.i_height;

    msg_Dbg( p_vout, "x11 image size %ix%i (%i,%i,%ix%i)",
             p_vout->fmt_out.i_width, p_vout->fmt_out.i_height,
             p_vout->fmt_out.i_x_offset, p_vout->fmt_out.i_y_offset,
             p_vout->fmt_out.i_visible_width,
             p_vout->fmt_out.i_visible_height );
#endif

    /* Try to initialize up to MAX_DIRECTBUFFERS direct buffers */
    while( I_OUTPUTPICTURES < MAX_DIRECTBUFFERS )
    {
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
        if( p_pic == NULL || NewPicture( p_vout, p_pic ) )
        {
            break;
        }

        p_pic->i_status = DESTROYED_PICTURE;
        p_pic->i_type   = DIRECT_PICTURE;

        PP_OUTPUTPICTURE[ I_OUTPUTPICTURES ] = p_pic;

        I_OUTPUTPICTURES++;
    }

    if( p_vout->output.i_chroma == VLC_FOURCC('Y','V','1','2') )
    {
        /* U and V inverted compared to I420
         * Fixme: this should be handled by the vout core */
        p_vout->output.i_chroma = VLC_FOURCC('I','4','2','0');
        p_vout->fmt_out.i_chroma = VLC_FOURCC('I','4','2','0');
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * DisplayVideo: displays previously rendered output
 *****************************************************************************
 * This function sends the currently rendered image to X11 server.
 * (The Xv extension takes care of "double-buffering".)
 *****************************************************************************/
static void DisplayVideo( vout_thread_t *p_vout, picture_t *p_pic )
{
    unsigned int i_width, i_height, i_x, i_y;

    vout_PlacePicture( p_vout, p_vout->p_sys->p_win->i_width,
                       p_vout->p_sys->p_win->i_height,
                       &i_x, &i_y, &i_width, &i_height );

#ifdef MODULE_NAME_IS_xvmc
    xvmc_context_reader_lock( &p_vout->p_sys->xvmc_lock );

    vlc_xxmc_t *xxmc = &p_pic->p_sys->xxmc_data;
    if( !xxmc->decoded ||
        !xxmc_xvmc_surface_valid( p_vout, p_pic->p_sys->xvmc_surf ) )
    {
      msg_Dbg( p_vout, "DisplayVideo decoded=%d\tsurfacevalid=%d",
               xxmc->decoded,
               xxmc_xvmc_surface_valid( p_vout, p_pic->p_sys->xvmc_surf ) );
      xvmc_context_reader_unlock( &p_vout->p_sys->xvmc_lock );
      return;
    }

    int src_width = p_vout->output.i_width;
    int src_height = p_vout->output.i_height;
    int src_x, src_y;

    if( p_vout->p_sys->xvmc_crop_style == 1 )
    {
        src_x = 20;
        src_y = 20;
        src_width -= 40;
        src_height -= 40;
    }
    else if( p_vout->p_sys->xvmc_crop_style == 2 )
    {
        src_x = 20;
        src_y = 40;
        src_width -= 40;
        src_height -= 80;
    }
    else if( p_vout->p_sys->xvmc_crop_style == 3 )
    {
        src_x = 40;
        src_y = 20;
        src_width -= 80;
        src_height -= 40;
    }
    else
    {
        src_x = 0;
        src_y = 0;
    }

    int first_field;
    if( p_vout->p_sys->xvmc_deinterlace_method > 0 )
    {   /* BOB DEINTERLACE */
        if( (p_pic->p_sys->nb_display == 0) ||
            (p_vout->p_sys->xvmc_deinterlace_method == 1) )
        {
            first_field = (p_pic->b_top_field_first) ?
                                XVMC_BOTTOM_FIELD : XVMC_TOP_FIELD;
        }
        else
        {
            first_field = (p_pic->b_top_field_first) ?
                                XVMC_TOP_FIELD : XVMC_BOTTOM_FIELD;
        }
    }
    else
    {
        first_field = XVMC_FRAME_PICTURE;
     }

    XVMCLOCKDISPLAY( p_vout->p_sys->p_display );
    XvMCFlushSurface( p_vout->p_sys->p_display, p_pic->p_sys->xvmc_surf );
    /* XvMCSyncSurface(p_vout->p_sys->p_display, p_picture->p_sys->xvmc_surf); */
    XvMCPutSurface( p_vout->p_sys->p_display,
                    p_pic->p_sys->xvmc_surf,
                    p_vout->p_sys->p_win->video_window,
                    src_x,
                    src_y,
                    src_width,
                    src_height,
                    0 /*dest_x*/,
                    0 /*dest_y*/,
                    i_width,
                    i_height,
                    first_field);

    XVMCUNLOCKDISPLAY( p_vout->p_sys->p_display );
    if( p_vout->p_sys->xvmc_deinterlace_method == 2 )
    {   /* BOB DEINTERLACE */
        if( p_pic->p_sys->nb_display == 0 )/* && ((t2-t1) < 15000)) */
        {
            mtime_t last_date = p_pic->date;

            vlc_mutex_lock( &p_vout->picture_lock );
            if( !p_vout->p_sys->last_date )
            {
                p_pic->date += 20000;
            }
            else
            {
                p_pic->date = ((3 * p_pic->date -
                                    p_vout->p_sys->last_date) / 2 );
            }
            p_vout->p_sys->last_date = last_date;
            p_pic->b_force = 1;
            p_pic->p_sys->nb_display = 1;
            vlc_mutex_unlock( &p_vout->picture_lock );
        }
        else
        {
            p_pic->p_sys->nb_display = 0;
            p_pic->b_force = 0;
        }
    }
    xvmc_context_reader_unlock( &p_vout->p_sys->xvmc_lock );
#endif

#ifdef HAVE_SYS_SHM_H
    if( p_vout->p_sys->i_shm_opcode )
    {
        /* Display rendered image using shared memory extension */
#if defined(MODULE_NAME_IS_xvideo) || defined(MODULE_NAME_IS_xvmc)
        XvShmPutImage( p_vout->p_sys->p_display, p_vout->p_sys->i_xvport,
                       p_vout->p_sys->p_win->video_window,
                       p_vout->p_sys->p_win->gc, p_pic->p_sys->p_image,
                       p_vout->fmt_out.i_x_offset,
                       p_vout->fmt_out.i_y_offset,
                       p_vout->fmt_out.i_visible_width,
                       p_vout->fmt_out.i_visible_height,
                       0 /*dest_x*/, 0 /*dest_y*/, i_width, i_height,
                       False /* Don't put True here or you'll waste your CPU */ );
#else
        XShmPutImage( p_vout->p_sys->p_display,
                      p_vout->p_sys->p_win->video_window,
                      p_vout->p_sys->p_win->gc, p_pic->p_sys->p_image,
                      p_vout->fmt_out.i_x_offset,
                      p_vout->fmt_out.i_y_offset,
                      0 /*dest_x*/, 0 /*dest_y*/,
                      p_vout->fmt_out.i_visible_width,
                      p_vout->fmt_out.i_visible_height,
                      False /* Don't put True here ! */ );
#endif
    }
    else
#endif /* HAVE_SYS_SHM_H */
    {
        /* Use standard XPutImage -- this is gonna be slow ! */
#if defined(MODULE_NAME_IS_xvideo) || defined(MODULE_NAME_IS_xvmc)
        XvPutImage( p_vout->p_sys->p_display, p_vout->p_sys->i_xvport,
                    p_vout->p_sys->p_win->video_window,
                    p_vout->p_sys->p_win->gc, p_pic->p_sys->p_image,
                    p_vout->fmt_out.i_x_offset,
                    p_vout->fmt_out.i_y_offset,
                    p_vout->fmt_out.i_visible_width,
                    p_vout->fmt_out.i_visible_height,
                    0 /*dest_x*/, 0 /*dest_y*/, i_width, i_height );
#else
        XPutImage( p_vout->p_sys->p_display,
                   p_vout->p_sys->p_win->video_window,
                   p_vout->p_sys->p_win->gc, p_pic->p_sys->p_image,
                   p_vout->fmt_out.i_x_offset,
                   p_vout->fmt_out.i_y_offset,
                   0 /*dest_x*/, 0 /*dest_y*/,
                   p_vout->fmt_out.i_visible_width,
                   p_vout->fmt_out.i_visible_height );
#endif
    }

    /* Make sure the command is sent now - do NOT use XFlush !*/
    XSync( p_vout->p_sys->p_display, False );
}

/*****************************************************************************
 * ManageVideo: handle X11 events
 *****************************************************************************
 * This function should be called regularly by video output thread. It manages
 * X11 events and allows window resizing. It returns a non null value on
 * error.
 *****************************************************************************/
static int ManageVideo( vout_thread_t *p_vout )
{
    XEvent      xevent;                                         /* X11 event */
    vlc_value_t val;

#ifdef MODULE_NAME_IS_xvmc
    xvmc_context_reader_lock( &p_vout->p_sys->xvmc_lock );
#endif

    /* Handle events from the owner window */
    if( p_vout->p_sys->p_win->owner_window )
    {
        while( XCheckWindowEvent( p_vout->p_sys->p_display,
                                p_vout->p_sys->p_win->owner_window->handle.xid,
                                  StructureNotifyMask, &xevent ) == True )
        {
            /* ConfigureNotify event: prepare  */
            if( xevent.type == ConfigureNotify )
            {
                /* Update dimensions */
                XResizeWindow( p_vout->p_sys->p_display,
                               p_vout->p_sys->p_win->base_window,
                               xevent.xconfigure.width,
                               xevent.xconfigure.height );
            }
        }
    }

    /* Handle X11 events: ConfigureNotify events are parsed to know if the
     * output window's size changed, MapNotify and UnmapNotify to know if the
     * window is mapped (and if the display is useful), and ClientMessages
     * to intercept window destruction requests */

    while( XCheckWindowEvent( p_vout->p_sys->p_display,
                              p_vout->p_sys->p_win->base_window,
                              StructureNotifyMask | KeyPressMask |
                              ButtonPressMask | ButtonReleaseMask |
                              PointerMotionMask | Button1MotionMask , &xevent )
           == True )
    {
        /* ConfigureNotify event: prepare  */
        if( xevent.type == ConfigureNotify )
        {
            if( (unsigned int)xevent.xconfigure.width
                   != p_vout->p_sys->p_win->i_width
              || (unsigned int)xevent.xconfigure.height
                    != p_vout->p_sys->p_win->i_height )
            {
                /* Update dimensions */
                p_vout->i_changes |= VOUT_SIZE_CHANGE;
                p_vout->p_sys->p_win->i_width = xevent.xconfigure.width;
                p_vout->p_sys->p_win->i_height = xevent.xconfigure.height;
            }
        }
        /* Keyboard event */
        else if( xevent.type == KeyPress )
        {
            unsigned int state = xevent.xkey.state;
            KeySym x_key_symbol;
            char i_key;                                   /* ISO Latin-1 key */

            /* We may have keys like F1 trough F12, ESC ... */
            x_key_symbol = XKeycodeToKeysym( p_vout->p_sys->p_display,
                                             xevent.xkey.keycode, 0 );
            val.i_int = ConvertKey( (int)x_key_symbol );

            xevent.xkey.state &= ~ShiftMask;
            xevent.xkey.state &= ~ControlMask;
            xevent.xkey.state &= ~Mod1Mask;

            if( !val.i_int &&
                XLookupString( &xevent.xkey, &i_key, 1, NULL, NULL ) )
            {
                /* "Normal Keys"
                 * The reason why I use this instead of XK_0 is that
                 * with XLookupString, we don't have to care about
                 * keymaps. */
                val.i_int = i_key;
            }

            if( val.i_int )
            {
                if( state & ShiftMask )
                {
                    val.i_int |= KEY_MODIFIER_SHIFT;
                }
                if( state & ControlMask )
                {
                    val.i_int |= KEY_MODIFIER_CTRL;
                }
                if( state & Mod1Mask )
                {
                    val.i_int |= KEY_MODIFIER_ALT;
                }
                var_Set( p_vout->p_libvlc, "key-pressed", val );
            }
        }
        /* Mouse click */
        else if( xevent.type == ButtonPress )
        {
            switch( ((XButtonEvent *)&xevent)->button )
            {
                case Button1:
                    var_Get( p_vout, "mouse-button-down", &val );
                    val.i_int |= 1;
                    var_Set( p_vout, "mouse-button-down", val );

                    var_SetBool( p_vout->p_libvlc, "intf-popupmenu", false );

                    /* detect double-clicks */
                    if( ( ((XButtonEvent *)&xevent)->time -
                          p_vout->p_sys->i_time_button_last_pressed ) < 300 )
                    {
                        p_vout->i_changes |= VOUT_FULLSCREEN_CHANGE;
                    }

                    p_vout->p_sys->i_time_button_last_pressed =
                        ((XButtonEvent *)&xevent)->time;
                    break;
                case Button2:
                    var_Get( p_vout, "mouse-button-down", &val );
                    val.i_int |= 2;
                    var_Set( p_vout, "mouse-button-down", val );
                    break;

                case Button3:
                    var_Get( p_vout, "mouse-button-down", &val );
                    val.i_int |= 4;
                    var_Set( p_vout, "mouse-button-down", val );
                    var_SetBool( p_vout->p_libvlc, "intf-popupmenu", true );
                    break;

                case Button4:
                    var_Get( p_vout, "mouse-button-down", &val );
                    val.i_int |= 8;
                    var_Set( p_vout, "mouse-button-down", val );
                    break;

                case Button5:
                    var_Get( p_vout, "mouse-button-down", &val );
                    val.i_int |= 16;
                    var_Set( p_vout, "mouse-button-down", val );
                    break;
            }
        }
        /* Mouse release */
        else if( xevent.type == ButtonRelease )
        {
            switch( ((XButtonEvent *)&xevent)->button )
            {
                case Button1:
                    {
                        var_Get( p_vout, "mouse-button-down", &val );
                        val.i_int &= ~1;
                        var_Set( p_vout, "mouse-button-down", val );

                        var_SetBool( p_vout, "mouse-clicked", true );
                    }
                    break;

                case Button2:
                    {
                        var_Get( p_vout, "mouse-button-down", &val );
                        val.i_int &= ~2;
                        var_Set( p_vout, "mouse-button-down", val );

                        var_Get( p_vout->p_libvlc, "intf-show", &val );
                        val.b_bool = !val.b_bool;
                        var_Set( p_vout->p_libvlc, "intf-show", val );
                    }
                    break;

                case Button3:
                    {
                        var_Get( p_vout, "mouse-button-down", &val );
                        val.i_int &= ~4;
                        var_Set( p_vout, "mouse-button-down", val );

                    }
                    break;

                case Button4:
                    var_Get( p_vout, "mouse-button-down", &val );
                    val.i_int &= ~8;
                    var_Set( p_vout, "mouse-button-down", val );
                    break;

                case Button5:
                    var_Get( p_vout, "mouse-button-down", &val );
                    val.i_int &= ~16;
                    var_Set( p_vout, "mouse-button-down", val );
                    break;

            }
        }
        /* Mouse move */
        else if( xevent.type == MotionNotify )
        {
            unsigned int i_width, i_height, i_x, i_y;

            /* somewhat different use for vout_PlacePicture:
             * here the values are needed to give to mouse coordinates
             * in the original picture space */
            vout_PlacePicture( p_vout, p_vout->p_sys->p_win->i_width,
                               p_vout->p_sys->p_win->i_height,
                               &i_x, &i_y, &i_width, &i_height );

            /* Compute the x coordinate and check if the value is
               in [0,p_vout->fmt_in.i_visible_width] */
            val.i_int = ( xevent.xmotion.x - i_x ) *
                p_vout->fmt_in.i_visible_width / i_width +
                p_vout->fmt_in.i_x_offset;

            if( (int)(xevent.xmotion.x - i_x) < 0 )
                val.i_int = 0;
            else if( (unsigned int)val.i_int > p_vout->fmt_in.i_visible_width )
                val.i_int = p_vout->fmt_in.i_visible_width;

            var_Set( p_vout, "mouse-x", val );

            /* compute the y coordinate and check if the value is
               in [0,p_vout->fmt_in.i_visible_height] */
            val.i_int = ( xevent.xmotion.y - i_y ) *
                p_vout->fmt_in.i_visible_height / i_height +
                p_vout->fmt_in.i_y_offset;

            if( (int)(xevent.xmotion.y - i_y) < 0 )
                val.i_int = 0;
            else if( (unsigned int)val.i_int > p_vout->fmt_in.i_visible_height )
                val.i_int = p_vout->fmt_in.i_visible_height;

            var_Set( p_vout, "mouse-y", val );

            var_SetBool( p_vout, "mouse-moved", true );

            p_vout->p_sys->i_time_mouse_last_moved = mdate();
            if( ! p_vout->p_sys->b_mouse_pointer_visible )
            {
                ToggleCursor( p_vout );
            }
        }
        else if( xevent.type == ReparentNotify /* XXX: why do we get this? */
                  || xevent.type == MapNotify
                  || xevent.type == UnmapNotify )
        {
            /* Ignore these events */
        }
        else /* Other events */
        {
            msg_Warn( p_vout, "unhandled event %d received", xevent.type );
        }
    }

    /* Handle events for video output sub-window */
    while( XCheckWindowEvent( p_vout->p_sys->p_display,
                              p_vout->p_sys->p_win->video_window,
                              ExposureMask, &xevent ) == True )
    {
        /* Window exposed (only handled if stream playback is paused) */
        if( xevent.type == Expose )
        {
            if( ((XExposeEvent *)&xevent)->count == 0 )
            {
                /* (if this is the last a collection of expose events...) */

#if defined(MODULE_NAME_IS_xvideo)
                x11_window_t *p_win = p_vout->p_sys->p_win;

                /* Paint the colour key if needed */
                if( p_vout->p_sys->b_paint_colourkey &&
                    xevent.xexpose.window == p_win->video_window )
                {
                    XSetForeground( p_vout->p_sys->p_display,
                                    p_win->gc, p_vout->p_sys->i_colourkey );
                    XFillRectangle( p_vout->p_sys->p_display,
                                    p_win->video_window, p_win->gc, 0, 0,
                                    p_win->i_width, p_win->i_height );
                }
#endif

#if 0
                if( p_vout->p_libvlc->p_input_bank->pp_input[0] != NULL )
                {
                    if( PAUSE_S == p_vout->p_libvlc->p_input_bank->pp_input[0]
                                                   ->stream.control.i_status )
                    {
                        /* XVideoDisplay( p_vout )*/;
                    }
                }
#endif
            }
        }
    }

    /* ClientMessage event - only WM_PROTOCOLS with WM_DELETE_WINDOW data
     * are handled - according to the man pages, the format is always 32
     * in this case */
    while( XCheckTypedEvent( p_vout->p_sys->p_display,
                             ClientMessage, &xevent ) )
    {
        if( (xevent.xclient.message_type == p_vout->p_sys->p_win->wm_protocols)
               && ((Atom)xevent.xclient.data.l[0]
                     == p_vout->p_sys->p_win->wm_delete_window ) )
        {
            /* the user wants to close the window */
            playlist_t * p_playlist = pl_Hold( p_vout );
            if( p_playlist != NULL )
            {
                playlist_Stop( p_playlist );
                pl_Release( p_vout );
            }
        }
    }

    /*
     * Fullscreen Change
     */
    if ( p_vout->i_changes & VOUT_FULLSCREEN_CHANGE )
    {
        /* Update the object variable and trigger callback */
        var_SetBool( p_vout, "fullscreen", !p_vout->b_fullscreen );

        ToggleFullScreen( p_vout );
        p_vout->i_changes &= ~VOUT_FULLSCREEN_CHANGE;
    }

    /* autoscale toggle */
    if( p_vout->i_changes & VOUT_SCALE_CHANGE )
    {
        p_vout->i_changes &= ~VOUT_SCALE_CHANGE;

        p_vout->b_autoscale = var_GetBool( p_vout, "autoscale" );
        p_vout->i_zoom = ZOOM_FP_FACTOR;

        p_vout->i_changes |= VOUT_SIZE_CHANGE;
    }

    /* scaling factor */
    if( p_vout->i_changes & VOUT_ZOOM_CHANGE )
    {
        p_vout->i_changes &= ~VOUT_ZOOM_CHANGE;

        p_vout->b_autoscale = false;
        p_vout->i_zoom =
            (int)( ZOOM_FP_FACTOR * var_GetFloat( p_vout, "scale" ) );

        p_vout->i_changes |= VOUT_SIZE_CHANGE;
    }

    if( p_vout->i_changes & VOUT_CROP_CHANGE ||
        p_vout->i_changes & VOUT_ASPECT_CHANGE )
    {
        p_vout->i_changes &= ~VOUT_CROP_CHANGE;
        p_vout->i_changes &= ~VOUT_ASPECT_CHANGE;

        p_vout->fmt_out.i_x_offset = p_vout->fmt_in.i_x_offset;
        p_vout->fmt_out.i_y_offset = p_vout->fmt_in.i_y_offset;
        p_vout->fmt_out.i_visible_width = p_vout->fmt_in.i_visible_width;
        p_vout->fmt_out.i_visible_height = p_vout->fmt_in.i_visible_height;
        p_vout->fmt_out.i_aspect = p_vout->fmt_in.i_aspect;
        p_vout->fmt_out.i_sar_num = p_vout->fmt_in.i_sar_num;
        p_vout->fmt_out.i_sar_den = p_vout->fmt_in.i_sar_den;
        p_vout->output.i_aspect = p_vout->fmt_in.i_aspect;

        p_vout->i_changes |= VOUT_SIZE_CHANGE;
    }

    /*
     * Size change
     *
     * (Needs to be placed after VOUT_FULLSREEN_CHANGE because we can activate
     *  the size flag inside the fullscreen routine)
     */
    if( p_vout->i_changes & VOUT_SIZE_CHANGE )
    {
        unsigned int i_width, i_height, i_x, i_y;

#ifdef MODULE_NAME_IS_x11
        /* We need to signal the vout thread about the size change because it
         * is doing the rescaling */
#else
        p_vout->i_changes &= ~VOUT_SIZE_CHANGE;
#endif

        vout_PlacePicture( p_vout, p_vout->p_sys->p_win->i_width,
                           p_vout->p_sys->p_win->i_height,
                           &i_x, &i_y, &i_width, &i_height );

        XMoveResizeWindow( p_vout->p_sys->p_display,
                           p_vout->p_sys->p_win->video_window,
                           i_x, i_y, i_width, i_height );
    }

    /* cursor hiding depending on --vout-event option
     *      activated if:
     *            value = 1 (Fullsupport) (default value)
     *         or value = 2 (Fullscreen-Only) and condition met
     */
    bool b_vout_event = (   ( p_vout->p_sys->i_vout_event == 1 )
                         || ( p_vout->p_sys->i_vout_event == 2 && p_vout->b_fullscreen )
                        );

    /* Autohide Cursour */
    if( mdate() - p_vout->p_sys->i_time_mouse_last_moved >
        p_vout->p_sys->i_mouse_hide_timeout )
    {
        /* Hide the mouse automatically */
        if( b_vout_event && p_vout->p_sys->b_mouse_pointer_visible )
        {
            ToggleCursor( p_vout );
        }
    }

#ifdef MODULE_NAME_IS_xvmc
    xvmc_context_reader_unlock( &p_vout->p_sys->xvmc_lock );
#endif

#ifdef HAVE_OSSO
    if ( p_vout->p_sys->p_octx != NULL ) {
        if ( p_vout->p_sys->i_backlight_on_counter == i_backlight_on_interval ) {
            if ( osso_display_blanking_pause( p_vout->p_sys->p_octx ) != OSSO_OK ) {
                msg_Err( p_vout, "Could not disable backlight blanking" );
        } else {
                msg_Dbg( p_vout, "Backlight blanking disabled" );
            }
            p_vout->p_sys->i_backlight_on_counter = 0;
        } else {
            p_vout->p_sys->i_backlight_on_counter ++;
        }
    }
#endif
    return 0;
}

/*****************************************************************************
 * EndVideo: terminate X11 video thread output method
 *****************************************************************************
 * Destroy the X11 XImages created by Init. It is called at the end of
 * the thread, but also each time the window is resized.
 *****************************************************************************/
static void EndVideo( vout_thread_t *p_vout )
{
    int i_index;

    /* Free the direct buffers we allocated */
    for( i_index = I_OUTPUTPICTURES ; i_index ; )
    {
        i_index--;
        FreePicture( p_vout, PP_OUTPUTPICTURE[ i_index ] );
    }
}

/* following functions are local */

/*****************************************************************************
 * CreateWindow: open and set-up X11 main window
 *****************************************************************************/
static int CreateWindow( vout_thread_t *p_vout, x11_window_t *p_win )
{
    XSizeHints              xsize_hints;
    XSetWindowAttributes    xwindow_attributes;
    XGCValues               xgcvalues;
    XEvent                  xevent;

    bool              b_map_notify = false;
    vlc_value_t             val;

    /* Prepare window manager hints and properties */
    p_win->wm_protocols =
             XInternAtom( p_vout->p_sys->p_display, "WM_PROTOCOLS", True );
    p_win->wm_delete_window =
             XInternAtom( p_vout->p_sys->p_display, "WM_DELETE_WINDOW", True );

    /* Never have a 0-pixel-wide window */
    xsize_hints.min_width = 2;
    xsize_hints.min_height = 1;

    /* Prepare window attributes */
    xwindow_attributes.backing_store = Always;       /* save the hidden part */
    xwindow_attributes.background_pixel = BlackPixel(p_vout->p_sys->p_display,
                                                     p_vout->p_sys->i_screen);
    xwindow_attributes.event_mask = ExposureMask | StructureNotifyMask;

    if( !p_vout->b_fullscreen )
    {
        p_win->owner_window = vout_RequestXWindow( p_vout, &p_win->i_x,
                              &p_win->i_y, &p_win->i_width, &p_win->i_height );
        xsize_hints.base_width  = xsize_hints.width = p_win->i_width;
        xsize_hints.base_height = xsize_hints.height = p_win->i_height;
        xsize_hints.flags       = PSize | PMinSize;

        if( p_win->i_x >=0 || p_win->i_y >= 0 )
        {
            xsize_hints.x = p_win->i_x;
            xsize_hints.y = p_win->i_y;
            xsize_hints.flags |= PPosition;
        }
    }
    else
    {
        /* Fullscreen window size and position */
        p_win->owner_window = NULL;
        p_win->i_x = p_win->i_y = 0;
        p_win->i_width =
            DisplayWidth( p_vout->p_sys->p_display, p_vout->p_sys->i_screen );
        p_win->i_height =
            DisplayHeight( p_vout->p_sys->p_display, p_vout->p_sys->i_screen );
    }

    if( !p_win->owner_window )
    {
        /* Create the window and set hints - the window must receive
         * ConfigureNotify events, and until it is displayed, Expose and
         * MapNotify events. */

        p_win->base_window =
            XCreateWindow( p_vout->p_sys->p_display,
                           DefaultRootWindow( p_vout->p_sys->p_display ),
                           p_win->i_x, p_win->i_y,
                           p_win->i_width, p_win->i_height,
                           0,
                           0, InputOutput, 0,
                           CWBackingStore | CWBackPixel | CWEventMask,
                           &xwindow_attributes );

        var_Get( p_vout, "video-title", &val );
        if( !val.psz_string || !*val.psz_string )
        {
            XStoreName( p_vout->p_sys->p_display, p_win->base_window,
#ifdef MODULE_NAME_IS_x11
                        VOUT_TITLE " (X11 output)"
#elif defined(MODULE_NAME_IS_glx)
                        VOUT_TITLE " (GLX output)"
#else
                        VOUT_TITLE " (XVideo output)"
#endif
              );
        }
        else
        {
            XStoreName( p_vout->p_sys->p_display,
                        p_win->base_window, val.psz_string );
        }
        free( val.psz_string );

        if( !p_vout->b_fullscreen )
        {
            const char *argv[] = { "vlc", NULL };

            /* Set window manager hints and properties: size hints, command,
             * window's name, and accepted protocols */
            XSetWMNormalHints( p_vout->p_sys->p_display,
                               p_win->base_window, &xsize_hints );
            XSetCommand( p_vout->p_sys->p_display, p_win->base_window,
                         (char**)argv, 1 );

            if( !var_GetBool( p_vout, "video-deco") )
            {
                Atom prop;
                mwmhints_t mwmhints;

                mwmhints.flags = MWM_HINTS_DECORATIONS;
                mwmhints.decorations = False;

                prop = XInternAtom( p_vout->p_sys->p_display, "_MOTIF_WM_HINTS",
                                    False );

                XChangeProperty( p_vout->p_sys->p_display,
                                 p_win->base_window,
                                 prop, prop, 32, PropModeReplace,
                                 (unsigned char *)&mwmhints,
                                 PROP_MWM_HINTS_ELEMENTS );
            }
        }
    }
    else
    {
        Window dummy1;
        int dummy2, dummy3;
        unsigned int dummy4, dummy5;

        /* Select events we are interested in. */
        XSelectInput( p_vout->p_sys->p_display,
                      p_win->owner_window->handle.xid, StructureNotifyMask );

        /* Get the parent window's geometry information */
        XGetGeometry( p_vout->p_sys->p_display,
                      p_win->owner_window->handle.xid,
                      &dummy1, &dummy2, &dummy3,
                      &p_win->i_width,
                      &p_win->i_height,
                      &dummy4, &dummy5 );

        /* From man XSelectInput: only one client at a time can select a
         * ButtonPress event, so we need to open a new window anyway. */
        p_win->base_window =
            XCreateWindow( p_vout->p_sys->p_display,
                           p_win->owner_window->handle.xid,
                           0, 0,
                           p_win->i_width, p_win->i_height,
                           0,
                           0, CopyFromParent, 0,
                           CWBackingStore | CWBackPixel | CWEventMask,
                           &xwindow_attributes );
    }

    if( (p_win->wm_protocols == None)        /* use WM_DELETE_WINDOW */
        || (p_win->wm_delete_window == None)
        || !XSetWMProtocols( p_vout->p_sys->p_display, p_win->base_window,
                             &p_win->wm_delete_window, 1 ) )
    {
        /* WM_DELETE_WINDOW is not supported by window manager */
        msg_Warn( p_vout, "missing or bad window manager" );
    }

    /* Creation of a graphic context that doesn't generate a GraphicsExpose
     * event when using functions like XCopyArea */
    xgcvalues.graphics_exposures = False;
    p_win->gc = XCreateGC( p_vout->p_sys->p_display,
                           p_win->base_window,
                           GCGraphicsExposures, &xgcvalues );

    /* Wait till the window is mapped */
    XMapWindow( p_vout->p_sys->p_display, p_win->base_window );
    do
    {
        XWindowEvent( p_vout->p_sys->p_display, p_win->base_window,
                      SubstructureNotifyMask | StructureNotifyMask, &xevent);
        if( (xevent.type == MapNotify)
                 && (xevent.xmap.window == p_win->base_window) )
        {
            b_map_notify = true;
        }
        else if( (xevent.type == ConfigureNotify)
                 && (xevent.xconfigure.window == p_win->base_window) )
        {
            p_win->i_width = xevent.xconfigure.width;
            p_win->i_height = xevent.xconfigure.height;
        }
    } while( !b_map_notify );

    /* key and mouse events handling depending on --vout-event option
     *      activated if:
     *            value = 1 (Fullsupport) (default value)
     *         or value = 2 (Fullscreen-Only) and condition met
     */
    bool b_vout_event = (   ( p_vout->p_sys->i_vout_event == 1 )
                         || ( p_vout->p_sys->i_vout_event == 2 && p_vout->b_fullscreen )
                        );
    if ( b_vout_event )
        XSelectInput( p_vout->p_sys->p_display, p_win->base_window,
                      StructureNotifyMask | KeyPressMask |
                      ButtonPressMask | ButtonReleaseMask |
                      PointerMotionMask );

#ifdef MODULE_NAME_IS_x11
    if( XDefaultDepth(p_vout->p_sys->p_display, p_vout->p_sys->i_screen) == 8 )
    {
        /* Allocate a new palette */
        p_vout->p_sys->colormap =
            XCreateColormap( p_vout->p_sys->p_display,
                             DefaultRootWindow( p_vout->p_sys->p_display ),
                             DefaultVisual( p_vout->p_sys->p_display,
                                            p_vout->p_sys->i_screen ),
                             AllocAll );

        xwindow_attributes.colormap = p_vout->p_sys->colormap;
        XChangeWindowAttributes( p_vout->p_sys->p_display, p_win->base_window,
                                 CWColormap, &xwindow_attributes );
    }
#endif

    /* Create video output sub-window. */
    p_win->video_window =  XCreateSimpleWindow(
                                      p_vout->p_sys->p_display,
                                      p_win->base_window, 0, 0,
                                      p_win->i_width, p_win->i_height,
                                      0,
                                      BlackPixel( p_vout->p_sys->p_display,
                                                  p_vout->p_sys->i_screen ),
                                      WhitePixel( p_vout->p_sys->p_display,
                                                  p_vout->p_sys->i_screen ) );

    XSetWindowBackground( p_vout->p_sys->p_display, p_win->video_window,
                          BlackPixel( p_vout->p_sys->p_display,
                                      p_vout->p_sys->i_screen ) );

    XMapWindow( p_vout->p_sys->p_display, p_win->video_window );
    XSelectInput( p_vout->p_sys->p_display, p_win->video_window,
                  ExposureMask );

    /* make sure the video window will be centered in the next ManageVideo() */
    p_vout->i_changes |= VOUT_SIZE_CHANGE;

    /* If the cursor was formerly blank than blank it again */
    if( !p_vout->p_sys->b_mouse_pointer_visible )
    {
        ToggleCursor( p_vout );
        ToggleCursor( p_vout );
    }

    /* Do NOT use XFlush here ! */
    XSync( p_vout->p_sys->p_display, False );

    /* At this stage, the window is open, displayed, and ready to
     * receive data */
    p_vout->p_sys->p_win = p_win;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * DestroyWindow: destroy the window
 *****************************************************************************
 *
 *****************************************************************************/
static void DestroyWindow( vout_thread_t *p_vout, x11_window_t *p_win )
{
    /* Do NOT use XFlush here ! */
    XSync( p_vout->p_sys->p_display, False );

    if( p_win->video_window != None )
        XDestroyWindow( p_vout->p_sys->p_display, p_win->video_window );

    XFreeGC( p_vout->p_sys->p_display, p_win->gc );

    XUnmapWindow( p_vout->p_sys->p_display, p_win->base_window );
    XDestroyWindow( p_vout->p_sys->p_display, p_win->base_window );

    /* make sure base window is destroyed before proceeding further */
    bool b_destroy_notify = false;
    do
    {
        XEvent      xevent;
        XWindowEvent( p_vout->p_sys->p_display, p_win->base_window,
                      SubstructureNotifyMask | StructureNotifyMask, &xevent);
        if( (xevent.type == DestroyNotify)
                 && (xevent.xmap.window == p_win->base_window) )
        {
            b_destroy_notify = true;
        }
    } while( !b_destroy_notify );

    vout_ReleaseWindow( p_win->owner_window );
}

/*****************************************************************************
 * NewPicture: allocate a picture
 *****************************************************************************
 * Returns 0 on success, -1 otherwise
 *****************************************************************************/
static int NewPicture( vout_thread_t *p_vout, picture_t *p_pic )
{
#ifndef MODULE_NAME_IS_glx

#if defined(MODULE_NAME_IS_xvideo) || defined(MODULE_NAME_IS_xvmc)
    int i_plane;
#endif

    /* We know the chroma, allocate a buffer which will be used
     * directly by the decoder */
    p_pic->p_sys = malloc( sizeof( picture_sys_t ) );

    if( p_pic->p_sys == NULL )
    {
        return -1;
    }

#ifdef MODULE_NAME_IS_xvmc
    p_pic->p_sys->p_vout = p_vout;
    p_pic->p_sys->xvmc_surf = NULL;
    p_pic->p_sys->xxmc_data.decoded = 0;
    p_pic->p_sys->xxmc_data.proc_xxmc_update_frame = xxmc_do_update_frame;
    //    p_pic->p_accel_data = &p_pic->p_sys->xxmc_data;
    p_pic->p_sys->nb_display = 0;
#endif

    /* Fill in picture_t fields */
    vout_InitPicture( VLC_OBJECT(p_vout), p_pic, p_vout->output.i_chroma,
                      p_vout->output.i_width, p_vout->output.i_height,
                      p_vout->output.i_aspect );

#ifdef HAVE_SYS_SHM_H
    if( p_vout->p_sys->i_shm_opcode )
    {
        /* Create image using XShm extension */
        p_pic->p_sys->p_image =
            CreateShmImage( p_vout, p_vout->p_sys->p_display,
#if defined(MODULE_NAME_IS_xvideo) || defined(MODULE_NAME_IS_xvmc)
                            p_vout->p_sys->i_xvport,
                            VLC2X11_FOURCC(p_vout->output.i_chroma),
#else
                            p_vout->p_sys->p_visual,
                            p_vout->p_sys->i_screen_depth,
#endif
                            &p_pic->p_sys->shminfo,
                            p_vout->output.i_width, p_vout->output.i_height );
    }

    if( !p_vout->p_sys->i_shm_opcode || !p_pic->p_sys->p_image )
#endif /* HAVE_SYS_SHM_H */
    {
        /* Create image without XShm extension */
        p_pic->p_sys->p_image =
            CreateImage( p_vout, p_vout->p_sys->p_display,
#if defined(MODULE_NAME_IS_xvideo) || defined(MODULE_NAME_IS_xvmc)
                         p_vout->p_sys->i_xvport,
                         VLC2X11_FOURCC(p_vout->output.i_chroma),
                         p_pic->format.i_bits_per_pixel,
#else
                         p_vout->p_sys->p_visual,
                         p_vout->p_sys->i_screen_depth,
                         p_vout->p_sys->i_bytes_per_pixel,
#endif
                         p_vout->output.i_width, p_vout->output.i_height );

#ifdef HAVE_SYS_SHM_H
        if( p_pic->p_sys->p_image && p_vout->p_sys->i_shm_opcode )
        {
            msg_Warn( p_vout, "couldn't create SHM image, disabling SHM" );
            p_vout->p_sys->i_shm_opcode = 0;
        }
#endif /* HAVE_SYS_SHM_H */
    }

    if( p_pic->p_sys->p_image == NULL )
    {
        free( p_pic->p_sys );
        return -1;
    }

    switch( p_vout->output.i_chroma )
    {
#if defined(MODULE_NAME_IS_xvideo) || defined(MODULE_NAME_IS_xvmc)
        case VLC_FOURCC('I','4','2','0'):
        case VLC_FOURCC('Y','V','1','2'):
        case VLC_FOURCC('Y','2','1','1'):
        case VLC_FOURCC('Y','U','Y','2'):
        case VLC_FOURCC('U','Y','V','Y'):
        case VLC_FOURCC('R','V','1','5'):
        case VLC_FOURCC('R','V','1','6'):
        case VLC_FOURCC('R','V','2','4'): /* Fixme: pixel pitch == 4 ? */
        case VLC_FOURCC('R','V','3','2'):

            for( i_plane = 0; i_plane < p_pic->p_sys->p_image->num_planes;
                 i_plane++ )
            {
                p_pic->p[i_plane].p_pixels = (uint8_t*)p_pic->p_sys->p_image->data
                    + p_pic->p_sys->p_image->offsets[i_plane];
                p_pic->p[i_plane].i_pitch =
                    p_pic->p_sys->p_image->pitches[i_plane];
            }
            if( p_vout->output.i_chroma == VLC_FOURCC('Y','V','1','2') )
            {
                /* U and V inverted compared to I420
                 * Fixme: this should be handled by the vout core */
                p_pic->U_PIXELS = (uint8_t*)p_pic->p_sys->p_image->data
                    + p_pic->p_sys->p_image->offsets[2];
                p_pic->V_PIXELS = (uint8_t*)p_pic->p_sys->p_image->data
                    + p_pic->p_sys->p_image->offsets[1];
            }

            break;

#else
        case VLC_FOURCC('R','G','B','2'):
        case VLC_FOURCC('R','V','1','6'):
        case VLC_FOURCC('R','V','1','5'):
        case VLC_FOURCC('R','V','2','4'):
        case VLC_FOURCC('R','V','3','2'):

            p_pic->p->i_lines = p_pic->p_sys->p_image->height;
            p_pic->p->i_visible_lines = p_pic->p_sys->p_image->height;
            p_pic->p->p_pixels = (uint8_t*)p_pic->p_sys->p_image->data
                                  + p_pic->p_sys->p_image->xoffset;
            p_pic->p->i_pitch = p_pic->p_sys->p_image->bytes_per_line;

            /* p_pic->p->i_pixel_pitch = 4 for RV24 but this should be set
             * properly by vout_InitPicture() */
            p_pic->p->i_visible_pitch = p_pic->p->i_pixel_pitch
                                         * p_pic->p_sys->p_image->width;
            break;
#endif

        default:
            /* Unknown chroma, tell the guy to get lost */
            IMAGE_FREE( p_pic->p_sys->p_image );
            free( p_pic->p_sys );
            msg_Err( p_vout, "never heard of chroma 0x%.8x (%4.4s)",
                     p_vout->output.i_chroma, (char*)&p_vout->output.i_chroma );
            p_pic->i_planes = 0;
            return -1;
    }
#else

    VLC_UNUSED(p_vout); VLC_UNUSED(p_pic);

#endif /* !MODULE_NAME_IS_glx */

    return 0;
}

/*****************************************************************************
 * FreePicture: destroy a picture allocated with NewPicture
 *****************************************************************************
 * Destroy XImage AND associated data. If using Shm, detach shared memory
 * segment from server and process, then free it. The XDestroyImage manpage
 * says that both the image structure _and_ the data pointed to by the
 * image structure are freed, so no need to free p_image->data.
 *****************************************************************************/
static void FreePicture( vout_thread_t *p_vout, picture_t *p_pic )
{
    /* The order of operations is correct */
#ifdef HAVE_SYS_SHM_H
    if( p_vout->p_sys->i_shm_opcode )
    {
        XShmDetach( p_vout->p_sys->p_display, &p_pic->p_sys->shminfo );
        IMAGE_FREE( p_pic->p_sys->p_image );

        shmctl( p_pic->p_sys->shminfo.shmid, IPC_RMID, 0 );
        if( shmdt( p_pic->p_sys->shminfo.shmaddr ) )
        {
            msg_Err( p_vout, "cannot detach shared memory (%m)" );
        }
    }
    else
#endif
    {
        IMAGE_FREE( p_pic->p_sys->p_image );
    }

#ifdef MODULE_NAME_IS_xvmc
    if( p_pic->p_sys->xvmc_surf != NULL )
    {
        xxmc_xvmc_free_surface(p_vout , p_pic->p_sys->xvmc_surf);
        p_pic->p_sys->xvmc_surf = NULL;
    }
#endif

    /* Do NOT use XFlush here ! */
    XSync( p_vout->p_sys->p_display, False );

    free( p_pic->p_sys );
}

/*****************************************************************************
 * ToggleFullScreen: Enable or disable full screen mode
 *****************************************************************************
 * This function will switch between fullscreen and window mode.
 *****************************************************************************/
static void ToggleFullScreen ( vout_thread_t *p_vout )
{
    Atom prop;
    mwmhints_t mwmhints;
    XSetWindowAttributes attributes;

#ifdef HAVE_XINERAMA
    int i_d1, i_d2;
#endif

    p_vout->b_fullscreen = !p_vout->b_fullscreen;

    if( p_vout->b_fullscreen )
    {
        msg_Dbg( p_vout, "entering fullscreen mode" );

        /* Getting current window position */
        Window root_win;
        Window* child_windows;
        unsigned int num_child_windows;
        Window parent_win;
        Window child_win;
        XWindowAttributes win_attr;
        int screen_x,screen_y,win_width,win_height;

        XGetWindowAttributes(
                p_vout->p_sys->p_display,
                p_vout->p_sys->p_win->video_window,
                &win_attr);

        XQueryTree(
                p_vout->p_sys->p_display,
                p_vout->p_sys->p_win->video_window,
                &root_win,
                &parent_win,
                &child_windows,
                &num_child_windows);
        XFree(child_windows);

        XTranslateCoordinates(
                p_vout->p_sys->p_display,
                parent_win, win_attr.root,
                win_attr.x,win_attr.y,
                &screen_x,&screen_y,
                &child_win);

        win_width = p_vout->p_sys->p_win->i_width;
        win_height = p_vout->p_sys->p_win->i_height;
        msg_Dbg( p_vout, "X %d/%d Y %d/%d",
            win_width, screen_x, win_height, screen_y );
        /* screen_x and screen_y are current position */

        p_vout->p_sys->b_altfullscreen =
            config_GetInt( p_vout, MODULE_STRING "-altfullscreen" );

        XUnmapWindow( p_vout->p_sys->p_display,
                      p_vout->p_sys->p_win->base_window );

        p_vout->p_sys->p_win = &p_vout->p_sys->fullscreen_window;

        CreateWindow( p_vout, p_vout->p_sys->p_win );
        XDestroyWindow( p_vout->p_sys->p_display,
                        p_vout->p_sys->fullscreen_window.video_window );
        XReparentWindow( p_vout->p_sys->p_display,
                         p_vout->p_sys->original_window.video_window,
                         p_vout->p_sys->fullscreen_window.base_window, 0, 0 );
        p_vout->p_sys->fullscreen_window.video_window =
            p_vout->p_sys->original_window.video_window;

        /* To my knowledge there are two ways to create a borderless window.
         * There's the generic way which is to tell x to bypass the window
         * manager, but this creates problems with the focus of other
         * applications.
         * The other way is to use the motif property "_MOTIF_WM_HINTS" which
         * luckily seems to be supported by most window managers. */
        if( !p_vout->p_sys->b_altfullscreen )
        {
            mwmhints.flags = MWM_HINTS_DECORATIONS;
            mwmhints.decorations = False;

            prop = XInternAtom( p_vout->p_sys->p_display, "_MOTIF_WM_HINTS",
                                False );
            XChangeProperty( p_vout->p_sys->p_display,
                             p_vout->p_sys->p_win->base_window,
                             prop, prop, 32, PropModeReplace,
                             (unsigned char *)&mwmhints,
                             PROP_MWM_HINTS_ELEMENTS );
        }
        else
        {
            /* brute force way to remove decorations */
            attributes.override_redirect = True;
            XChangeWindowAttributes( p_vout->p_sys->p_display,
                                     p_vout->p_sys->p_win->base_window,
                                     CWOverrideRedirect,
                                     &attributes);
        }

        /* Make sure the change is effective */
        XReparentWindow( p_vout->p_sys->p_display,
                         p_vout->p_sys->p_win->base_window,
                         DefaultRootWindow( p_vout->p_sys->p_display ), 0, 0 );

        if( p_vout->p_sys->b_net_wm_state_fullscreen )
        {
            XClientMessageEvent event = {
                .type = ClientMessage,
                .window = p_vout->p_sys->p_win->base_window,
                .message_type = p_vout->p_sys->net_wm_state,
                .format = 32,
                .data = {
                    .l = {
                        1, /* set property */
                        p_vout->p_sys->net_wm_state_fullscreen,
                        0,
                        1,
                    },
                },
            };

            XSendEvent( p_vout->p_sys->p_display,
                        DefaultRootWindow( p_vout->p_sys->p_display ),
                        False, SubstructureNotifyMask|SubstructureRedirectMask,
                        (XEvent*)&event );
        }

/* "bad fullscreen" - set this to 0. doing fullscreen this way is problematic
 * for many reasons and basically fights with the window manager as the wm
 * reparents AND vlc goes and reparents - multiple times. don't do it. it just
 * makes it more inefficient and less "nice" to the x11 citizenry. this turns
 * it off */
#define BADFS 0
/* explicitly asking for focus when you fullscreened is a little silly. the
 * window manager SHOULD be handling this itself based on its own focus
 * policies. if the user is "using" a given xinerama/xrandr screen or x11
 * multihead screen AND vlc wants to fullscreen the wm should also focus it
 * as its the only thing on the screen. if vlc fullscreens and its on
 * "another monitor" to the one the user is using - this may "steal" the focus
 * as really the wm should be deciding if, on fullscreening of a window
 * the focus should go there or not, so let the wm decided */
#define APPFOCUS 0

#ifdef HAVE_XINERAMA
        if( XineramaQueryExtension( p_vout->p_sys->p_display, &i_d1, &i_d2 ) &&
            XineramaIsActive( p_vout->p_sys->p_display ) )
        {
            XineramaScreenInfo *screens;   /* infos for xinerama */
            int i_num_screens;

            msg_Dbg( p_vout, "using XFree Xinerama extension");

#define SCREEN p_vout->p_sys->p_win->i_screen

            /* Get Information about Xinerama (num of screens) */
            screens = XineramaQueryScreens( p_vout->p_sys->p_display,
                                            &i_num_screens );

            SCREEN = config_GetInt( p_vout,
                                        MODULE_STRING "-xineramascreen" );

            /* just check that user has entered a good value,
             * otherwise use that screen where window is */
            if( SCREEN >= i_num_screens || SCREEN < 0 )
            {
                int overlapping=0;
                int rightmost_left=0;
                int leftmost_right=0;
                int bottommost_top=0;
                int topmost_bottom=0;
                int best_screen=0;
                int best_overlapping=0;
                int dx,dy;
                msg_Dbg( p_vout, "requested screen number invalid (%d/%d)", SCREEN, i_num_screens );
#define left ( screens[SCREEN].x_org )
#define right ( left + screens[SCREEN].width )
#define top screens[SCREEN].y_org
#define bottom ( top + screens[SCREEN].height )

                /* Code mostly same as http://superswitcher.googlecode.com/svn/trunk/src/xinerama.c
                 * by Nigel Tao, as it was pretty clean implemention what's needed here. Checks what display
                 * contains most of the window, and use that as fullscreen screen instead screen what
                 * contains videowindows 0.0 */
                 for( SCREEN = i_num_screens-1; SCREEN >= 0; SCREEN--)
                 {
                     rightmost_left = __MAX( left, screen_x );
                     leftmost_right = __MIN( right, screen_x + win_width );
                     bottommost_top = __MAX( top, screen_y );
                     topmost_bottom = __MIN( bottom , screen_y + win_height );
                     dx = leftmost_right - rightmost_left;
                     dy = topmost_bottom - bottommost_top;
                     overlapping=0;
                     if ( dx > 0 && dy > 0 )
                         overlapping = dx*dy;
                     if( SCREEN == (i_num_screens-1) ||
                             overlapping > best_overlapping )
                     {
                         best_overlapping = overlapping;
                         best_screen = SCREEN;
                     }
                 }
                 msg_Dbg( p_vout, "setting best screen to %d", best_screen );
                 SCREEN = best_screen;
#undef bottom
#undef top
#undef right
#undef left
            }

            /* Get the X/Y upper left corner coordinate of the above screen */
            p_vout->p_sys->p_win->i_x = screens[SCREEN].x_org;
            p_vout->p_sys->p_win->i_y = screens[SCREEN].y_org;

            /* Set the Height/width to the screen resolution */
            p_vout->p_sys->p_win->i_width = screens[SCREEN].width;
            p_vout->p_sys->p_win->i_height = screens[SCREEN].height;

            XFree(screens);

#undef SCREEN

        }
        else
#endif
        {
            /* The window wasn't necessarily created at the requested size */
            p_vout->p_sys->p_win->i_x = p_vout->p_sys->p_win->i_y = 0;

#ifdef HAVE_XF86VIDMODE
            XF86VidModeModeLine mode;
            int i_dummy;

            if( XF86VidModeGetModeLine( p_vout->p_sys->p_display,
                                        p_vout->p_sys->i_screen, &i_dummy,
                                        &mode ) )
            {
                p_vout->p_sys->p_win->i_width = mode.hdisplay;
                p_vout->p_sys->p_win->i_height = mode.vdisplay;

                /* move cursor to the middle of the window to prevent
                 * unwanted display move if the display is smaller than the
                 * full desktop */
                XWarpPointer( p_vout->p_sys->p_display, None,
                              p_vout->p_sys->p_win->base_window, 0, 0, 0, 0,
                              mode.hdisplay / 2 , mode.vdisplay / 2 );
                /* force desktop view to upper left corner */
                XF86VidModeSetViewPort( p_vout->p_sys->p_display,
                                        p_vout->p_sys->i_screen, 0, 0 );
            }
            else
#endif
            {
                p_vout->p_sys->p_win->i_width =
                    DisplayWidth( p_vout->p_sys->p_display,
                                p_vout->p_sys->i_screen );
                p_vout->p_sys->p_win->i_height =
                    DisplayHeight( p_vout->p_sys->p_display,
                                p_vout->p_sys->i_screen );
            }

        }

        XMoveResizeWindow( p_vout->p_sys->p_display,
                           p_vout->p_sys->p_win->base_window,
                           p_vout->p_sys->p_win->i_x,
                           p_vout->p_sys->p_win->i_y,
                           p_vout->p_sys->p_win->i_width,
                           p_vout->p_sys->p_win->i_height );

#ifdef HAVE_XSP
        EnablePixelDoubling( p_vout );
#endif

#if APPFOCUS // RASTER: let the wm do focus policy
        /* Activate the window (give it the focus) */
        XClientMessageEvent event;

        memset( &event, 0, sizeof( XClientMessageEvent ) );

        event.type = ClientMessage;
        event.message_type =
           XInternAtom( p_vout->p_sys->p_display, "_NET_ACTIVE_WINDOW", False );
        event.display = p_vout->p_sys->p_display;
        event.window = p_vout->p_sys->p_win->base_window;
        event.format = 32;
        event.data.l[ 0 ] = 1; /* source indication (1 = from an application */
        event.data.l[ 1 ] = 0; /* timestamp */
        event.data.l[ 2 ] = 0; /* requestor's currently active window */
        /* XXX: window manager would be more likely to obey if we already have
         * an active window (and give it to the event), such as an interface */

        XSendEvent( p_vout->p_sys->p_display,
                    DefaultRootWindow( p_vout->p_sys->p_display ),
                    False, SubstructureRedirectMask,
                    (XEvent*)&event );
#endif
    }
    else
    {
        msg_Dbg( p_vout, "leaving fullscreen mode" );

#ifdef HAVE_XSP
        DisablePixelDoubling( p_vout );
#endif

        XReparentWindow( p_vout->p_sys->p_display,
                         p_vout->p_sys->original_window.video_window,
                         p_vout->p_sys->original_window.base_window, 0, 0 );

        p_vout->p_sys->fullscreen_window.video_window = None;
        DestroyWindow( p_vout, &p_vout->p_sys->fullscreen_window );
        p_vout->p_sys->p_win = &p_vout->p_sys->original_window;

        XMapWindow( p_vout->p_sys->p_display,
                    p_vout->p_sys->p_win->base_window );
    }

    /* Unfortunately, using XSync() here is not enough to ensure the
     * window has already been mapped because the XMapWindow() request
     * has not necessarily been sent directly to our window (remember,
     * the call is first redirected to the window manager) */

#if BADFS // RASTER: this is silly... if we have already mapped before
    XEvent xevent;
    do
    {
        XWindowEvent( p_vout->p_sys->p_display,
                      p_vout->p_sys->p_win->base_window,
                      StructureNotifyMask, &xevent );
    } while( xevent.type != MapNotify );
#else
   XSync(p_vout->p_sys->p_display, False);
#endif

    /* Be careful, this can generate a BadMatch error if the window is not
     * already mapped by the server (see above) */
    XSetInputFocus(p_vout->p_sys->p_display,
                   p_vout->p_sys->p_win->base_window,
                   RevertToParent,
                   CurrentTime);

    /* signal that the size needs to be updated */
    p_vout->i_changes |= VOUT_SIZE_CHANGE;
}

/*****************************************************************************
 * EnableXScreenSaver: enable screen saver
 *****************************************************************************
 * This function enables the screen saver on a display after it has been
 * disabled by XDisableScreenSaver.
 * FIXME: what happens if multiple vlc sessions are running at the same
 *        time ???
 *****************************************************************************/
static void EnableXScreenSaver( vout_thread_t *p_vout )
{
#ifdef DPMSINFO_IN_DPMS_H
    int dummy;
#endif

    if( p_vout->p_sys->i_ss_timeout )
    {
        XSetScreenSaver( p_vout->p_sys->p_display, p_vout->p_sys->i_ss_timeout,
                         p_vout->p_sys->i_ss_interval,
                         p_vout->p_sys->i_ss_blanking,
                         p_vout->p_sys->i_ss_exposure );
    }

    /* Restore DPMS settings */
#ifdef DPMSINFO_IN_DPMS_H
    if( DPMSQueryExtension( p_vout->p_sys->p_display, &dummy, &dummy ) )
    {
        if( p_vout->p_sys->b_ss_dpms )
        {
            DPMSEnable( p_vout->p_sys->p_display );
        }
    }
#endif
}

/*****************************************************************************
 * DisableXScreenSaver: disable screen saver
 *****************************************************************************
 * See XEnableXScreenSaver
 *****************************************************************************/
static void DisableXScreenSaver( vout_thread_t *p_vout )
{
#ifdef DPMSINFO_IN_DPMS_H
    int dummy;
#endif

    /* Save screen saver information */
    XGetScreenSaver( p_vout->p_sys->p_display, &p_vout->p_sys->i_ss_timeout,
                     &p_vout->p_sys->i_ss_interval,
                     &p_vout->p_sys->i_ss_blanking,
                     &p_vout->p_sys->i_ss_exposure );

    /* Disable screen saver */
    if( p_vout->p_sys->i_ss_timeout )
    {
        XSetScreenSaver( p_vout->p_sys->p_display, 0,
                         p_vout->p_sys->i_ss_interval,
                         p_vout->p_sys->i_ss_blanking,
                         p_vout->p_sys->i_ss_exposure );
    }

    /* Disable DPMS */
#ifdef DPMSINFO_IN_DPMS_H
    if( DPMSQueryExtension( p_vout->p_sys->p_display, &dummy, &dummy ) )
    {
        CARD16 unused;
        /* Save DPMS current state */
        DPMSInfo( p_vout->p_sys->p_display, &unused,
                  &p_vout->p_sys->b_ss_dpms );
        DPMSDisable( p_vout->p_sys->p_display );
   }
#endif
}

/*****************************************************************************
 * CreateCursor: create a blank mouse pointer
 *****************************************************************************/
static void CreateCursor( vout_thread_t *p_vout )
{
    XColor cursor_color;

    p_vout->p_sys->cursor_pixmap =
        XCreatePixmap( p_vout->p_sys->p_display,
                       DefaultRootWindow( p_vout->p_sys->p_display ),
                       1, 1, 1 );

    XParseColor( p_vout->p_sys->p_display,
                 XCreateColormap( p_vout->p_sys->p_display,
                                  DefaultRootWindow(
                                                    p_vout->p_sys->p_display ),
                                  DefaultVisual(
                                                p_vout->p_sys->p_display,
                                                p_vout->p_sys->i_screen ),
                                  AllocNone ),
                 "black", &cursor_color );

    p_vout->p_sys->blank_cursor =
        XCreatePixmapCursor( p_vout->p_sys->p_display,
                             p_vout->p_sys->cursor_pixmap,
                             p_vout->p_sys->cursor_pixmap,
                             &cursor_color, &cursor_color, 1, 1 );
}

/*****************************************************************************
 * DestroyCursor: destroy the blank mouse pointer
 *****************************************************************************/
static void DestroyCursor( vout_thread_t *p_vout )
{
    XFreePixmap( p_vout->p_sys->p_display, p_vout->p_sys->cursor_pixmap );
}

/*****************************************************************************
 * ToggleCursor: hide or show the mouse pointer
 *****************************************************************************
 * This function hides the X pointer if it is visible by setting the pointer
 * sprite to a blank one. To show it again, we disable the sprite.
 *****************************************************************************/
static void ToggleCursor( vout_thread_t *p_vout )
{
    if( p_vout->p_sys->b_mouse_pointer_visible )
    {
        XDefineCursor( p_vout->p_sys->p_display,
                       p_vout->p_sys->p_win->base_window,
                       p_vout->p_sys->blank_cursor );
        p_vout->p_sys->b_mouse_pointer_visible = 0;
    }
    else
    {
        XUndefineCursor( p_vout->p_sys->p_display,
                         p_vout->p_sys->p_win->base_window );
        p_vout->p_sys->b_mouse_pointer_visible = 1;
    }
}

#if defined(MODULE_NAME_IS_xvideo) || defined(MODULE_NAME_IS_xvmc)
/*****************************************************************************
 * XVideoGetPort: get YUV12 port
 *****************************************************************************/
static int XVideoGetPort( vout_thread_t *p_vout,
                          vlc_fourcc_t i_chroma, picture_heap_t *p_heap )
{
    XvAdaptorInfo *p_adaptor;
    unsigned int i;
    unsigned int i_adaptor, i_num_adaptors;
    int i_requested_adaptor;
    int i_selected_port;

    switch( XvQueryExtension( p_vout->p_sys->p_display, &i, &i, &i, &i, &i ) )
    {
        case Success:
            break;

        case XvBadExtension:
            msg_Warn( p_vout, "XvBadExtension" );
            return -1;

        case XvBadAlloc:
            msg_Warn( p_vout, "XvBadAlloc" );
            return -1;

        default:
            msg_Warn( p_vout, "XvQueryExtension failed" );
            return -1;
    }

    switch( XvQueryAdaptors( p_vout->p_sys->p_display,
                             DefaultRootWindow( p_vout->p_sys->p_display ),
                             &i_num_adaptors, &p_adaptor ) )
    {
        case Success:
            break;

        case XvBadExtension:
            msg_Warn( p_vout, "XvBadExtension for XvQueryAdaptors" );
            return -1;

        case XvBadAlloc:
            msg_Warn( p_vout, "XvBadAlloc for XvQueryAdaptors" );
            return -1;

        default:
            msg_Warn( p_vout, "XvQueryAdaptors failed" );
            return -1;
    }

    i_selected_port = -1;
#ifdef MODULE_NAME_IS_xvmc
    i_requested_adaptor = config_GetInt( p_vout, "xvmc-adaptor" );
#else
    i_requested_adaptor = config_GetInt( p_vout, "xvideo-adaptor" );
#endif
    for( i_adaptor = 0; i_adaptor < i_num_adaptors; ++i_adaptor )
    {
        XvImageFormatValues *p_formats;
        int i_format, i_num_formats;
        int i_port;

        /* If we requested an adaptor and it's not this one, we aren't
         * interested */
        if( i_requested_adaptor != -1 && ((int)i_adaptor != i_requested_adaptor) )
        {
            continue;
        }

        /* If the adaptor doesn't have the required properties, skip it */
        if( !( p_adaptor[ i_adaptor ].type & XvInputMask ) ||
            !( p_adaptor[ i_adaptor ].type & XvImageMask ) )
        {
            continue;
        }

        /* Check that adaptor supports our requested format... */
        p_formats = XvListImageFormats( p_vout->p_sys->p_display,
                                        p_adaptor[i_adaptor].base_id,
                                        &i_num_formats );

        for( i_format = 0;
             i_format < i_num_formats && ( i_selected_port == -1 );
             i_format++ )
        {
            XvAttribute     *p_attr;
            int             i_attr, i_num_attributes;
            Atom            autopaint = None, colorkey = None;

            /* If this is not the format we want, or at least a
             * similar one, forget it */
            if( !vout_ChromaCmp( p_formats[ i_format ].id, i_chroma ) )
            {
                continue;
            }

            /* Look for the first available port supporting this format */
            for( i_port = p_adaptor[i_adaptor].base_id;
                 ( i_port < (int)(p_adaptor[i_adaptor].base_id
                                   + p_adaptor[i_adaptor].num_ports) )
                   && ( i_selected_port == -1 );
                 i_port++ )
            {
                if( XvGrabPort( p_vout->p_sys->p_display, i_port, CurrentTime )
                     == Success )
                {
                    i_selected_port = i_port;
                    p_heap->i_chroma = p_formats[ i_format ].id;
#if XvVersion > 2 || ( XvVersion == 2 && XvRevision >= 2 )
                    p_heap->i_rmask = p_formats[ i_format ].red_mask;
                    p_heap->i_gmask = p_formats[ i_format ].green_mask;
                    p_heap->i_bmask = p_formats[ i_format ].blue_mask;
#endif
                }
            }

            /* If no free port was found, forget it */
            if( i_selected_port == -1 )
            {
                continue;
            }

            /* If we found a port, print information about it */
            msg_Dbg( p_vout, "adaptor %i, port %i, format 0x%x (%4.4s) %s",
                     i_adaptor, i_selected_port, p_formats[ i_format ].id,
                     (char *)&p_formats[ i_format ].id,
                     ( p_formats[ i_format ].format == XvPacked ) ?
                         "packed" : "planar" );

            /* Use XV_AUTOPAINT_COLORKEY if supported, otherwise we will
             * manually paint the colour key */
            p_attr = XvQueryPortAttributes( p_vout->p_sys->p_display,
                                            i_selected_port,
                                            &i_num_attributes );

            for( i_attr = 0; i_attr < i_num_attributes; i_attr++ )
            {
                if( !strcmp( p_attr[i_attr].name, "XV_AUTOPAINT_COLORKEY" ) )
                {
                    autopaint = XInternAtom( p_vout->p_sys->p_display,
                                             "XV_AUTOPAINT_COLORKEY", False );
                    XvSetPortAttribute( p_vout->p_sys->p_display,
                                        i_selected_port, autopaint, 1 );
                }
                if( !strcmp( p_attr[i_attr].name, "XV_COLORKEY" ) )
                {
                    /* Find out the default colour key */
                    colorkey = XInternAtom( p_vout->p_sys->p_display,
                                            "XV_COLORKEY", False );
                    XvGetPortAttribute( p_vout->p_sys->p_display,
                                        i_selected_port, colorkey,
                                        &p_vout->p_sys->i_colourkey );
                }
            }
            p_vout->p_sys->b_paint_colourkey =
                autopaint == None && colorkey != None;

            if( p_attr != NULL )
            {
                XFree( p_attr );
            }
        }

        if( p_formats != NULL )
        {
            XFree( p_formats );
        }

    }

    if( i_num_adaptors > 0 )
    {
        XvFreeAdaptorInfo( p_adaptor );
    }

    if( i_selected_port == -1 )
    {
        int i_chroma_tmp = X112VLC_FOURCC( i_chroma );
        if( i_requested_adaptor == -1 )
        {
            msg_Warn( p_vout, "no free XVideo port found for format "
                      "0x%.8x (%4.4s)", i_chroma_tmp, (char*)&i_chroma_tmp );
        }
        else
        {
            msg_Warn( p_vout, "XVideo adaptor %i does not have a free "
                      "XVideo port for format 0x%.8x (%4.4s)",
                      i_requested_adaptor, i_chroma_tmp, (char*)&i_chroma_tmp );
        }
    }

    return i_selected_port;
}

/*****************************************************************************
 * XVideoReleasePort: release YUV12 port
 *****************************************************************************/
static void XVideoReleasePort( vout_thread_t *p_vout, int i_port )
{
    XvUngrabPort( p_vout->p_sys->p_display, i_port, CurrentTime );
}
#endif

/*****************************************************************************
 * InitDisplay: open and initialize X11 device
 *****************************************************************************
 * Create a window according to video output given size, and set other
 * properties according to the display properties.
 *****************************************************************************/
static int InitDisplay( vout_thread_t *p_vout )
{
#ifdef MODULE_NAME_IS_x11
    XPixmapFormatValues *       p_formats;                 /* pixmap formats */
    XVisualInfo *               p_xvisual;            /* visuals information */
    XVisualInfo                 xvisual_template;         /* visual template */
    int                         i_count, i;                    /* array size */
#endif

#ifdef HAVE_SYS_SHM_H
    p_vout->p_sys->i_shm_opcode = 0;

#ifndef MODULE_NAME_IS_glx
    if( config_GetInt( p_vout, MODULE_STRING "-shm" ) > 0 )
    {
        int major, evt, err;

        if( XQueryExtension( p_vout->p_sys->p_display, "MIT-SHM", &major,
                             &evt, &err )
         && XShmQueryExtension( p_vout->p_sys->p_display ) )
            p_vout->p_sys->i_shm_opcode = major;

        if( p_vout->p_sys->i_shm_opcode )
        {
            int minor;
            Bool pixmaps;

            XShmQueryVersion( p_vout->p_sys->p_display, &major, &minor,
                              &pixmaps );
            msg_Dbg( p_vout, "XShm video extension v%d.%d "
                     "(with%s pixmaps, opcode: %d)",
                     major, minor, pixmaps ? "" : "out",
                     p_vout->p_sys->i_shm_opcode );
        }
        else
            msg_Warn( p_vout, "XShm video extension not available" );
    }
    else
        msg_Dbg( p_vout, "XShm video extension disabled" );
#endif
#endif

#ifdef MODULE_NAME_IS_xvideo
    /* XXX The brightness and contrast values should be read from environment
     * XXX variables... */
#if 0
    XVideoSetAttribute( p_vout, "XV_BRIGHTNESS", 0.5 );
    XVideoSetAttribute( p_vout, "XV_CONTRAST",   0.5 );
#endif
#endif

#ifdef MODULE_NAME_IS_x11
    /* Initialize structure */
    p_vout->p_sys->i_screen = DefaultScreen( p_vout->p_sys->p_display );

    /* Get screen depth */
    p_vout->p_sys->i_screen_depth = XDefaultDepth( p_vout->p_sys->p_display,
                                                   p_vout->p_sys->i_screen );
    switch( p_vout->p_sys->i_screen_depth )
    {
    case 8:
        /*
         * Screen depth is 8bpp. Use PseudoColor visual with private colormap.
         */
        xvisual_template.screen =   p_vout->p_sys->i_screen;
        xvisual_template.class =    DirectColor;
        p_xvisual = XGetVisualInfo( p_vout->p_sys->p_display,
                                    VisualScreenMask | VisualClassMask,
                                    &xvisual_template, &i_count );
        if( p_xvisual == NULL )
        {
            msg_Err( p_vout, "no PseudoColor visual available" );
            return VLC_EGENERIC;
        }
        p_vout->p_sys->i_bytes_per_pixel = 1;
        p_vout->output.pf_setpalette = SetPalette;
        break;
    case 15:
    case 16:
    case 24:
    default:
        /*
         * Screen depth is higher than 8bpp. TrueColor visual is used.
         */
        xvisual_template.screen =   p_vout->p_sys->i_screen;
        xvisual_template.class =    TrueColor;
/* In some cases, we get a truecolor class adaptor that has a different
   color depth. So try to get a real true color one first */
        xvisual_template.depth =    p_vout->p_sys->i_screen_depth;

        p_xvisual = XGetVisualInfo( p_vout->p_sys->p_display,
                                    VisualScreenMask | VisualClassMask |
                                    VisualDepthMask,
                                    &xvisual_template, &i_count );
        if( p_xvisual == NULL )
        {
            msg_Warn( p_vout, "No screen matching the required color depth" );
            p_xvisual = XGetVisualInfo( p_vout->p_sys->p_display,
                                    VisualScreenMask | VisualClassMask,
                                    &xvisual_template, &i_count );
            if( p_xvisual == NULL )
            {

                msg_Err( p_vout, "no TrueColor visual available" );
                return VLC_EGENERIC;
            }
        }

        p_vout->output.i_rmask = p_xvisual->red_mask;
        p_vout->output.i_gmask = p_xvisual->green_mask;
        p_vout->output.i_bmask = p_xvisual->blue_mask;

        /* There is no difference yet between 3 and 4 Bpp. The only way
         * to find the actual number of bytes per pixel is to list supported
         * pixmap formats. */
        p_formats = XListPixmapFormats( p_vout->p_sys->p_display, &i_count );
        p_vout->p_sys->i_bytes_per_pixel = 0;

        for( i = 0; i < i_count; i++ )
        {
            /* Under XFree4.0, the list contains pixmap formats available
             * through all video depths ; so we have to check against current
             * depth. */
            if( p_formats[i].depth == (int)p_vout->p_sys->i_screen_depth )
            {
                if( p_formats[i].bits_per_pixel / 8
                        > (int)p_vout->p_sys->i_bytes_per_pixel )
                {
                    p_vout->p_sys->i_bytes_per_pixel =
                        p_formats[i].bits_per_pixel / 8;
                }
            }
        }
        if( p_formats ) XFree( p_formats );

        break;
    }
    p_vout->p_sys->p_visual = p_xvisual->visual;
    XFree( p_xvisual );
#endif

    return VLC_SUCCESS;
}

#ifndef MODULE_NAME_IS_glx

#ifdef HAVE_SYS_SHM_H
/*****************************************************************************
 * CreateShmImage: create an XImage or XvImage using shared memory extension
 *****************************************************************************
 * Prepare an XImage or XvImage for display function.
 * The order of the operations respects the recommandations of the mit-shm
 * document by J.Corbet and K.Packard. Most of the parameters were copied from
 * there. See http://ftp.xfree86.org/pub/XFree86/4.0/doc/mit-shm.TXT
 *****************************************************************************/
IMAGE_TYPE * CreateShmImage( vout_thread_t *p_vout,
                                    Display* p_display, EXTRA_ARGS_SHM,
                                    int i_width, int i_height )
{
    IMAGE_TYPE *p_image;
    Status result;

    /* Create XImage / XvImage */
#ifdef MODULE_NAME_IS_xvideo
    p_image = XvShmCreateImage( p_display, i_xvport, i_chroma, 0,
                                i_width, i_height, p_shm );
#elif defined(MODULE_NAME_IS_xvmc)
    p_image = XvShmCreateImage( p_display, i_xvport, i_chroma, 0,
                                i_width, i_height, p_shm );
#else
    p_image = XShmCreateImage( p_display, p_visual, i_depth, ZPixmap, 0,
                               p_shm, i_width, i_height );
#endif
    if( p_image == NULL )
    {
        msg_Err( p_vout, "image creation failed" );
        return NULL;
    }

    /* For too big image, the buffer returned is sometimes too small, prevent
     * VLC to segfault because of it
     * FIXME is it normal ? Is there a way to detect it
     * before (XvQueryBestSize did not) ? */
    if( p_image->width < i_width || p_image->height < i_height )
    {
        msg_Err( p_vout, "cannot allocate shared image data with the right size "
                         "(%dx%d instead of %dx%d)",
                         p_image->width, p_image->height,
                         i_width, i_height );
        IMAGE_FREE( p_image );
        return NULL;
    }

    /* Allocate shared memory segment. */
    p_shm->shmid = shmget( IPC_PRIVATE, DATA_SIZE(p_image), IPC_CREAT | 0600 );
    if( p_shm->shmid < 0 )
    {
        msg_Err( p_vout, "cannot allocate shared image data (%m)" );
        IMAGE_FREE( p_image );
        return NULL;
    }

    /* Attach shared memory segment to process (read/write) */
    p_shm->shmaddr = p_image->data = shmat( p_shm->shmid, 0, 0 );
    if(! p_shm->shmaddr )
    {
        msg_Err( p_vout, "cannot attach shared memory (%m)" );
        IMAGE_FREE( p_image );
        shmctl( p_shm->shmid, IPC_RMID, 0 );
        return NULL;
    }

    /* Read-only data. We won't be using XShmGetImage */
    p_shm->readOnly = True;

    /* Attach shared memory segment to X server */
    XSynchronize( p_display, True );
    i_shm_major = p_vout->p_sys->i_shm_opcode;
    result = XShmAttach( p_display, p_shm );
    if( result == False || !i_shm_major )
    {
        msg_Err( p_vout, "cannot attach shared memory to X server" );
        IMAGE_FREE( p_image );
        shmctl( p_shm->shmid, IPC_RMID, 0 );
        shmdt( p_shm->shmaddr );
        return NULL;
    }
    XSynchronize( p_display, False );

    /* Send image to X server. This instruction is required, since having
     * built a Shm XImage and not using it causes an error on XCloseDisplay,
     * and remember NOT to use XFlush ! */
    XSync( p_display, False );

#if 0
    /* Mark the shm segment to be removed when there are no more
     * attachements, so it is automatic on process exit or after shmdt */
    shmctl( p_shm->shmid, IPC_RMID, 0 );
#endif

    return p_image;
}
#endif

/*****************************************************************************
 * CreateImage: create an XImage or XvImage
 *****************************************************************************
 * Create a simple image used as a buffer.
 *****************************************************************************/
static IMAGE_TYPE * CreateImage( vout_thread_t *p_vout,
                                 Display *p_display, EXTRA_ARGS,
                                 int i_width, int i_height )
{
    uint8_t *    p_data;                          /* image data storage zone */
    IMAGE_TYPE *p_image;
#ifdef MODULE_NAME_IS_x11
    int         i_quantum;                     /* XImage quantum (see below) */
    int         i_bytes_per_line;
#endif

    /* Allocate memory for image */
#ifdef MODULE_NAME_IS_xvideo
    p_data = malloc( i_width * i_height * i_bits_per_pixel / 8 );
#elif defined(MODULE_NAME_IS_x11)
    i_bytes_per_line = i_width * i_bytes_per_pixel;
    p_data = malloc( i_bytes_per_line * i_height );
#endif
    if( !p_data )
        return NULL;

#ifdef MODULE_NAME_IS_x11
    /* Optimize the quantum of a scanline regarding its size - the quantum is
       a diviser of the number of bits between the start of two scanlines. */
    if( i_bytes_per_line & 0xf )
    {
        i_quantum = 0x8;
    }
    else if( i_bytes_per_line & 0x10 )
    {
        i_quantum = 0x10;
    }
    else
    {
        i_quantum = 0x20;
    }
#endif

    /* Create XImage. p_data will be automatically freed */
#ifdef MODULE_NAME_IS_xvideo
    p_image = XvCreateImage( p_display, i_xvport, i_chroma,
                             (char *)p_data, i_width, i_height );
#elif defined(MODULE_NAME_IS_x11)
    p_image = XCreateImage( p_display, p_visual, i_depth, ZPixmap, 0,
                            (char *)p_data, i_width, i_height, i_quantum, 0 );
#endif
    if( p_image == NULL )
    {
        msg_Err( p_vout, "XCreateImage() failed" );
        free( p_data );
        return NULL;
    }

    return p_image;
}

#endif
/*****************************************************************************
 * X11ErrorHandler: replace error handler so we can intercept some of them
 *****************************************************************************/
static int X11ErrorHandler( Display * display, XErrorEvent * event )
{
    char txt[1024];

    XGetErrorText( display, event->error_code, txt, sizeof( txt ) );
    fprintf( stderr,
             "[????????] x11 video output error: X11 request %u.%u failed "
              "with error code %u:\n %s\n",
             event->request_code, event->minor_code, event->error_code, txt );

    switch( event->request_code )
    {
    case X_SetInputFocus:
        /* Ignore errors on XSetInputFocus()
         * (they happen when a window is not yet mapped) */
        return 0;
    }

#ifdef HAVE_SYS_SHM_H
    if( event->request_code == i_shm_major ) /* MIT-SHM */
    {
        fprintf( stderr,
                 "[????????] x11 video output notice:"
                 " buggy X11 server claims shared memory\n"
                 "[????????] x11 video output notice:"
                 " support though it does not work (OpenSSH?)\n" );
        return i_shm_major = 0;
    }
#endif

#ifndef HAVE_OSSO
    XSetErrorHandler(NULL);
    return (XSetErrorHandler(X11ErrorHandler))( display, event );
#else
    /* Work-around Maemo Xvideo bug */
    return 0;
#endif
}

#ifdef MODULE_NAME_IS_x11
/*****************************************************************************
 * SetPalette: sets an 8 bpp palette
 *****************************************************************************
 * This function sets the palette given as an argument. It does not return
 * anything, but could later send information on which colors it was unable
 * to set.
 *****************************************************************************/
static void SetPalette( vout_thread_t *p_vout,
                        uint16_t *red, uint16_t *green, uint16_t *blue )
{
    int i;
    XColor p_colors[255];

    /* allocate palette */
    for( i = 0; i < 255; i++ )
    {
        /* kludge: colors are indexed reversely because color 255 seems
         * to be reserved for black even if we try to set it to white */
        p_colors[ i ].pixel = 255 - i;
        p_colors[ i ].pad   = 0;
        p_colors[ i ].flags = DoRed | DoGreen | DoBlue;
        p_colors[ i ].red   = red[ 255 - i ];
        p_colors[ i ].blue  = blue[ 255 - i ];
        p_colors[ i ].green = green[ 255 - i ];
    }

    XStoreColors( p_vout->p_sys->p_display,
                  p_vout->p_sys->colormap, p_colors, 255 );
}
#endif

/*****************************************************************************
 * Control: control facility for the vout
 *****************************************************************************/
static int Control( vout_thread_t *p_vout, int i_query, va_list args )
{
    bool b_arg;
    unsigned int i_width, i_height;

    switch( i_query )
    {
        case VOUT_SET_SIZE:
            if( p_vout->p_sys->p_win->owner_window )
                return vout_ControlWindow( p_vout->p_sys->p_win->owner_window,
                                           i_query, args);

            i_width  = va_arg( args, unsigned int );
            i_height = va_arg( args, unsigned int );
            if( !i_width ) i_width = p_vout->i_window_width;
            if( !i_height ) i_height = p_vout->i_window_height;

#ifdef MODULE_NAME_IS_xvmc
            xvmc_context_reader_lock( &p_vout->p_sys->xvmc_lock );
#endif
            /* Update dimensions */
            XResizeWindow( p_vout->p_sys->p_display,
                           p_vout->p_sys->p_win->base_window,
                           i_width, i_height );
#ifdef MODULE_NAME_IS_xvmc
            xvmc_context_reader_unlock( &p_vout->p_sys->xvmc_lock );
#endif
            return VLC_SUCCESS;

        case VOUT_SET_STAY_ON_TOP:
            if( p_vout->p_sys->p_win->owner_window )
                return vout_ControlWindow( p_vout->p_sys->p_win->owner_window,
                                           i_query, args);

            b_arg = (bool) va_arg( args, int );
#ifdef MODULE_NAME_IS_xvmc
            xvmc_context_reader_lock( &p_vout->p_sys->xvmc_lock );
#endif
            WindowOnTop( p_vout, b_arg );
#ifdef MODULE_NAME_IS_xvmc
            xvmc_context_reader_unlock( &p_vout->p_sys->xvmc_lock );
#endif
            return VLC_SUCCESS;

       default:
            return VLC_EGENERIC;
    }
}

/*****************************************************************************
 * TestNetWMSupport: tests for Extended Window Manager Hints support
 *****************************************************************************/
static void TestNetWMSupport( vout_thread_t *p_vout )
{
    int i_ret, i_format;
    unsigned long i, i_items, i_bytesafter;
    Atom net_wm_supported;
    union { Atom *p_atom; unsigned char *p_char; } p_args;

    p_args.p_atom = NULL;

    p_vout->p_sys->b_net_wm_state_fullscreen =
    p_vout->p_sys->b_net_wm_state_above =
    p_vout->p_sys->b_net_wm_state_below =
    p_vout->p_sys->b_net_wm_state_stays_on_top =
        false;

    net_wm_supported =
        XInternAtom( p_vout->p_sys->p_display, "_NET_SUPPORTED", False );

    i_ret = XGetWindowProperty( p_vout->p_sys->p_display,
                                DefaultRootWindow( p_vout->p_sys->p_display ),
                                net_wm_supported,
                                0, 16384, False, AnyPropertyType,
                                &net_wm_supported,
                                &i_format, &i_items, &i_bytesafter,
                                (unsigned char **)&p_args );

    if( i_ret != Success || i_items == 0 ) return;

    msg_Dbg( p_vout, "Window manager supports NetWM" );

    p_vout->p_sys->net_wm_state =
        XInternAtom( p_vout->p_sys->p_display, "_NET_WM_STATE", False );
    p_vout->p_sys->net_wm_state_fullscreen =
        XInternAtom( p_vout->p_sys->p_display, "_NET_WM_STATE_FULLSCREEN",
                     False );
    p_vout->p_sys->net_wm_state_above =
        XInternAtom( p_vout->p_sys->p_display, "_NET_WM_STATE_ABOVE", False );
    p_vout->p_sys->net_wm_state_below =
        XInternAtom( p_vout->p_sys->p_display, "_NET_WM_STATE_BELOW", False );
    p_vout->p_sys->net_wm_state_stays_on_top =
        XInternAtom( p_vout->p_sys->p_display, "_NET_WM_STATE_STAYS_ON_TOP",
                     False );

    for( i = 0; i < i_items; i++ )
    {
        if( p_args.p_atom[i] == p_vout->p_sys->net_wm_state_fullscreen )
        {
            msg_Dbg( p_vout,
                     "Window manager supports _NET_WM_STATE_FULLSCREEN" );
            p_vout->p_sys->b_net_wm_state_fullscreen = true;
        }
        else if( p_args.p_atom[i] == p_vout->p_sys->net_wm_state_above )
        {
            msg_Dbg( p_vout, "Window manager supports _NET_WM_STATE_ABOVE" );
            p_vout->p_sys->b_net_wm_state_above = true;
        }
        else if( p_args.p_atom[i] == p_vout->p_sys->net_wm_state_below )
        {
            msg_Dbg( p_vout, "Window manager supports _NET_WM_STATE_BELOW" );
            p_vout->p_sys->b_net_wm_state_below = true;
        }
        else if( p_args.p_atom[i] == p_vout->p_sys->net_wm_state_stays_on_top )
        {
            msg_Dbg( p_vout,
                     "Window manager supports _NET_WM_STATE_STAYS_ON_TOP" );
            p_vout->p_sys->b_net_wm_state_stays_on_top = true;
        }
    }

    XFree( p_args.p_atom );
}

/*****************************************************************************
 * Key events handling
 *****************************************************************************/
static const struct
{
    int i_x11key;
    int i_vlckey;

} x11keys_to_vlckeys[] =
{
    { XK_F1, KEY_F1 }, { XK_F2, KEY_F2 }, { XK_F3, KEY_F3 }, { XK_F4, KEY_F4 },
    { XK_F5, KEY_F5 }, { XK_F6, KEY_F6 }, { XK_F7, KEY_F7 }, { XK_F8, KEY_F8 },
    { XK_F9, KEY_F9 }, { XK_F10, KEY_F10 }, { XK_F11, KEY_F11 },
    { XK_F12, KEY_F12 },

    { XK_Return, KEY_ENTER },
    { XK_KP_Enter, KEY_ENTER },
    { XK_space, KEY_SPACE },
    { XK_Escape, KEY_ESC },

    { XK_Menu, KEY_MENU },
    { XK_Left, KEY_LEFT },
    { XK_Right, KEY_RIGHT },
    { XK_Up, KEY_UP },
    { XK_Down, KEY_DOWN },

    { XK_Home, KEY_HOME },
    { XK_End, KEY_END },
    { XK_Page_Up, KEY_PAGEUP },
    { XK_Page_Down, KEY_PAGEDOWN },

    { XK_Insert, KEY_INSERT },
    { XK_Delete, KEY_DELETE },
    { XF86XK_AudioNext, KEY_MEDIA_NEXT_TRACK},
    { XF86XK_AudioPrev, KEY_MEDIA_PREV_TRACK},
    { XF86XK_AudioMute, KEY_VOLUME_MUTE },
    { XF86XK_AudioLowerVolume, KEY_VOLUME_DOWN },
    { XF86XK_AudioRaiseVolume, KEY_VOLUME_UP },
    { XF86XK_AudioPlay, KEY_MEDIA_PLAY_PAUSE },
    { XF86XK_AudioPause, KEY_MEDIA_PLAY_PAUSE },

    { 0, 0 }
};

static int ConvertKey( int i_key )
{
    int i;

    for( i = 0; x11keys_to_vlckeys[i].i_x11key != 0; i++ )
    {
        if( x11keys_to_vlckeys[i].i_x11key == i_key )
        {
            return x11keys_to_vlckeys[i].i_vlckey;
        }
    }

    return 0;
}

/*****************************************************************************
 * WindowOnTop: Switches the "always on top" state of the video window.
 *****************************************************************************/
static int WindowOnTop( vout_thread_t *p_vout, bool b_on_top )
{
    XClientMessageEvent event;

    memset( &event, 0, sizeof( XClientMessageEvent ) );
    event.type = ClientMessage;
    event.message_type = p_vout->p_sys->net_wm_state;
    event.display = p_vout->p_sys->p_display;
    event.window = p_vout->p_sys->p_win->base_window;
    event.format = 32;
    event.data.l[ 0 ] = b_on_top; /* set property */

    if( p_vout->p_sys->b_net_wm_state_stays_on_top )
        event.data.l[ 1 ] = p_vout->p_sys->net_wm_state_stays_on_top;
    else if( p_vout->p_sys->b_net_wm_state_above )
        /* use _NET_WM_STATE_ABOVE if window manager
         * doesn't handle _NET_WM_STATE_STAYS_ON_TOP */
        event.data.l[ 1 ] = p_vout->p_sys->net_wm_state_above;
    else
        return VLC_EGENERIC;

    XSendEvent( p_vout->p_sys->p_display,
                DefaultRootWindow( p_vout->p_sys->p_display ),
                False, SubstructureRedirectMask,
                (XEvent*)&event );
    return VLC_SUCCESS;
}
