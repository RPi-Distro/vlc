/**
 * @file glx.c
 * @brief GLX video output module for VLC media player
 */
/*****************************************************************************
 * Copyright © 2004 the VideoLAN team
 * Copyright © 2009 Rémi Denis-Courmont
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 ****************************************************************************/

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdlib.h>
#include <assert.h>

#include <xcb/xcb.h>
#include <GL/glx.h>
#include <GL/glxext.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_xlib.h>
#include <vlc_vout_display.h>
#include <vlc_opengl.h>
#include "../opengl.h"

#include "xcb_vlc.h"

static int  Open (vlc_object_t *);
static void Close (vlc_object_t *);

/*
 * Module descriptor
 */
vlc_module_begin ()
    set_shortname (N_("GLX"))
    set_description (N_("OpenGL GLX video output (XCB)"))
    set_category (CAT_VIDEO)
    set_subcategory (SUBCAT_VIDEO_VOUT)
    set_capability ("vout display", 150)
    set_callbacks (Open, Close)

    add_shortcut ("xcb-glx", "glx", "opengl", "xid")
vlc_module_end ()

struct vout_display_sys_t
{
    Display *display; /* Xlib instance */
    xcb_connection_t *conn; /**< XCB connection */
    vout_window_t *embed; /* VLC window (when windowed) */

    xcb_cursor_t cursor; /* blank cursor */
    xcb_window_t window; /* drawable X window */
    xcb_window_t glwin; /* GLX window */
    bool visible; /* whether to draw */
    bool v1_3; /* whether GLX >= 1.3 is available */

    GLXContext ctx;
    vlc_gl_t gl;
    vout_display_opengl_t *vgl;
    picture_pool_t *pool; /* picture pool */
};

static picture_pool_t *Pool (vout_display_t *, unsigned);
static void PictureRender (vout_display_t *, picture_t *, subpicture_t *);
static void PictureDisplay (vout_display_t *, picture_t *, subpicture_t *);
static int Control (vout_display_t *, int, va_list);
static void Manage (vout_display_t *);

static void SwapBuffers (vlc_gl_t *gl);
static void *GetProcAddress (vlc_gl_t *gl, const char *);

static vout_window_t *MakeWindow (vout_display_t *vd)
{
    vout_window_cfg_t wnd_cfg;

    memset (&wnd_cfg, 0, sizeof (wnd_cfg));
    wnd_cfg.type = VOUT_WINDOW_TYPE_XID;
    wnd_cfg.x = var_InheritInteger (vd, "video-x");
    wnd_cfg.y = var_InheritInteger (vd, "video-y");
    wnd_cfg.width  = vd->cfg->display.width;
    wnd_cfg.height = vd->cfg->display.height;

    vout_window_t *wnd = vout_display_NewWindow (vd, &wnd_cfg);
    if (wnd == NULL)
        msg_Err (vd, "parent window not available");
    return wnd;
}

static const xcb_screen_t *
FindWindow (vout_display_t *vd, xcb_connection_t *conn,
            unsigned *restrict pnum, uint8_t *restrict pdepth,
            uint16_t *restrict pwidth, uint16_t *restrict pheight)
{
    vout_display_sys_t *sys = vd->sys;

    xcb_get_geometry_reply_t *geo =
        xcb_get_geometry_reply (conn,
            xcb_get_geometry (conn, sys->embed->handle.xid), NULL);
    if (geo == NULL)
    {
        msg_Err (vd, "parent window not valid");
        return NULL;
    }

    xcb_window_t root = geo->root;
    *pdepth = geo->depth;
    *pwidth = geo->width;
    *pheight = geo->height;
    free (geo);

    /* Find the selected screen */
    const xcb_setup_t *setup = xcb_get_setup (conn);
    const xcb_screen_t *screen = NULL;
    unsigned num = 0;

    for (xcb_screen_iterator_t i = xcb_setup_roots_iterator (setup);
         i.rem > 0;
         xcb_screen_next (&i))
    {
        if (i.data->root == root)
        {
            screen = i.data;
            break;
        }
        num++;
    }

    if (screen == NULL)
    {
        msg_Err (vd, "parent window screen not found");
        return NULL;
    }
    msg_Dbg (vd, "using screen 0x%"PRIx32 " (number: %u)", root, num);
    *pnum = num;
    return screen;
}

static bool CheckGLX (vout_display_t *vd, Display *dpy, bool *restrict pv13)
{
    int major, minor;
    bool ok = false;

    if (!glXQueryVersion (dpy, &major, &minor))
        msg_Dbg (vd, "GLX extension not available");
    else
    if (major != 1)
        msg_Dbg (vd, "GLX extension version %d.%d unknown", major, minor);
    else
    if (minor < 2)
        msg_Dbg (vd, "GLX extension version %d.%d too old", major, minor);
    else
    {
        msg_Dbg (vd, "using GLX extension version %d.%d", major, minor);
        ok = true;
        *pv13 = minor >= 3;
    }
    return ok;
}

static int CreateWindow (vout_display_t *vd, xcb_connection_t *conn,
                         uint_fast8_t depth, xcb_visualid_t vid,
                         uint_fast16_t width, uint_fast16_t height)
{
    vout_display_sys_t *sys = vd->sys;
    const uint32_t mask = XCB_CW_EVENT_MASK;
    const uint32_t values[] = {
        /* XCB_CW_EVENT_MASK */
        XCB_EVENT_MASK_VISIBILITY_CHANGE,
    };
    xcb_void_cookie_t cc, cm;

    cc = xcb_create_window_checked (conn, depth, sys->window,
                                    sys->embed->handle.xid, 0, 0,
                                    width, height, 0,
                                    XCB_WINDOW_CLASS_INPUT_OUTPUT,
                                    vid, mask, values);
    cm = xcb_map_window_checked (conn, sys->window);
    if (CheckError (vd, conn, "cannot create X11 window", cc)
     || CheckError (vd, conn, "cannot map X11 window", cm))
        return VLC_EGENERIC;

    msg_Dbg (vd, "using X11 window %08"PRIx32, sys->window);
    return VLC_SUCCESS;
}

/**
 * Probe the X server.
 */
static int Open (vlc_object_t *obj)
{
    if (!vlc_xlib_init (obj))
        return VLC_EGENERIC;

    vout_display_t *vd = (vout_display_t *)obj;
    vout_display_sys_t *sys = malloc (sizeof (*sys));

    if (sys == NULL)
        return VLC_ENOMEM;

    vd->sys = sys;
    sys->pool = NULL;
    sys->gl.sys = NULL;

    /* Get window */
    sys->embed = MakeWindow (vd);
    if (sys->embed == NULL)
    {
        free (sys);
        return VLC_EGENERIC;
    }

    /* Connect to X server */
    xcb_connection_t *conn = xcb_connect (sys->embed->display.x11, NULL);
    if (unlikely(xcb_connection_has_error (conn)))
    {
        vout_display_DeleteWindow (vd, sys->embed);
        free (sys);
        return VLC_EGENERIC;
    }

    Display *dpy = XOpenDisplay (sys->embed->display.x11);
    if (dpy == NULL)
    {
        xcb_disconnect (conn);
        vout_display_DeleteWindow (vd, sys->embed);
        free (sys);
        return VLC_EGENERIC;
    }
    sys->display = dpy;
    sys->conn = conn;
    sys->ctx = NULL;

    if (!CheckGLX (vd, dpy, &sys->v1_3))
        goto error;

    RegisterMouseEvents (obj, conn, sys->embed->handle.xid);

    /* Find window parameters */
    unsigned snum;
    uint8_t depth;
    uint16_t width, height;
    const xcb_screen_t *scr = FindWindow (vd, conn, &snum, &depth,
                                          &width, &height);
    if (scr == NULL)
        goto error;

    sys->window = xcb_generate_id (conn);

    /* Determine our pixel format */
    if (sys->v1_3)
    {   /* GLX 1.3 */
        static const int attr[] = {
            GLX_RED_SIZE, 5,
            GLX_GREEN_SIZE, 5,
            GLX_BLUE_SIZE, 5,
            GLX_DOUBLEBUFFER, True,
            GLX_X_RENDERABLE, True,
            GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT,
            None };

        xcb_get_window_attributes_reply_t *wa =
            xcb_get_window_attributes_reply (conn,
                xcb_get_window_attributes (conn, sys->embed->handle.xid),
                NULL);
        if (wa == NULL)
            goto error;
        xcb_visualid_t visual = wa->visual;
        free (wa);

        int nelem;
        GLXFBConfig *confs = glXChooseFBConfig (dpy, snum, attr, &nelem);
        if (confs == NULL)
        {
            msg_Err (vd, "no GLX frame bufer configurations");
            goto error;
        }

        GLXFBConfig conf;
        bool found = false;

        for (int i = 0; i < nelem && !found; i++)
        {
            conf = confs[i];

            XVisualInfo *vi = glXGetVisualFromFBConfig (dpy, conf);
            if (vi == NULL)
                continue;

            if (vi->visualid == visual)
                found = true;
            XFree (vi);
        }
        XFree (confs);

        if (!found)
        {
            msg_Err (vd, "no matching GLX frame buffer configuration");
            goto error;
        }

        sys->glwin = None;
        if (!CreateWindow (vd, conn, depth, 0 /* ??? */, width, height))
            sys->glwin = glXCreateWindow (dpy, conf, sys->window, NULL );
        if (sys->glwin == None)
        {
            msg_Err (vd, "cannot create GLX window");
            goto error;
        }

        /* Create an OpenGL context */
        sys->ctx = glXCreateNewContext (dpy, conf, GLX_RGBA_TYPE, NULL,
                                        True);
        if (sys->ctx == NULL)
        {
            msg_Err (vd, "cannot create GLX context");
            goto error;
        }

        if (!glXMakeContextCurrent (dpy, sys->glwin, sys->glwin, sys->ctx))
            goto error;
    }
    else
    {   /* GLX 1.2 */
        int attr[] = {
            GLX_RGBA,
            GLX_RED_SIZE, 5,
            GLX_GREEN_SIZE, 5,
            GLX_BLUE_SIZE, 5,
            GLX_DOUBLEBUFFER,
            None };

        XVisualInfo *vi = glXChooseVisual (dpy, snum, attr);
        if (vi == NULL)
        {
            msg_Err (vd, "cannot find GLX 1.2 visual" );
            goto error;
        }
        msg_Dbg (vd, "using GLX visual ID 0x%"PRIx32, (uint32_t)vi->visualid);

        if (CreateWindow (vd, conn, depth, 0 /* ??? */, width, height) == 0)
            sys->ctx = glXCreateContext (dpy, vi, 0, True);
        XFree (vi);
        if (sys->ctx == NULL)
        {
            msg_Err (vd, "cannot create GLX context");
            goto error;
        }

        if (glXMakeCurrent (dpy, sys->window, sys->ctx) == False)
            goto error;
        sys->glwin = sys->window;
    }

    const char *glx_extensions = glXQueryExtensionsString (dpy, snum);

    bool is_swap_interval_set = false;
#ifdef GLX_SGI_swap_control
    if (HasExtension (glx_extensions, "GLX_SGI_swap_control")) {
        PFNGLXSWAPINTERVALSGIPROC SwapIntervalSGI = (PFNGLXSWAPINTERVALSGIPROC)GetProcAddress (NULL, "glXSwapIntervalSGI");
        if (!is_swap_interval_set && SwapIntervalSGI)
            is_swap_interval_set = !SwapIntervalSGI (1);
    }
#endif
#ifdef GLX_EXT_swap_control
    if (HasExtension (glx_extensions, "GLX_EXT_swap_control")) {
        PFNGLXSWAPINTERVALEXTPROC SwapIntervalEXT = (PFNGLXSWAPINTERVALEXTPROC)GetProcAddress (NULL, "glXSwapIntervalEXT");
        if (!is_swap_interval_set && SwapIntervalEXT)
        {
            SwapIntervalEXT (dpy, sys->glwin, 1);
            is_swap_interval_set = true;
        }
    }
#endif

    /* Initialize common OpenGL video display */
    sys->gl.lock = NULL;
    sys->gl.unlock = NULL;
    sys->gl.swap = SwapBuffers;
    sys->gl.getProcAddress = GetProcAddress;
    sys->gl.sys = sys;

    const vlc_fourcc_t *subpicture_chromas;
    sys->vgl = vout_display_opengl_New (&vd->fmt, &subpicture_chromas, &sys->gl);
    if (!sys->vgl)
    {
        sys->gl.sys = NULL;
        goto error;
    }

    sys->cursor = CreateBlankCursor (conn, scr);
    sys->visible = false;

    /* */
    vout_display_info_t info = vd->info;
    info.has_pictures_invalid = false;
    info.has_event_thread = true;
    info.subpicture_chromas = subpicture_chromas;

    /* Setup vout_display_t once everything is fine */
    vd->info = info;

    vd->pool = Pool;
    vd->prepare = PictureRender;
    vd->display = PictureDisplay;
    vd->control = Control;
    vd->manage = Manage;

    /* */
    bool is_fullscreen = vd->cfg->is_fullscreen;
    if (is_fullscreen && vout_window_SetFullScreen (sys->embed, true))
        is_fullscreen = false;
    vout_display_SendEventFullscreen (vd, is_fullscreen);
    vout_display_SendEventDisplaySize (vd, width, height, is_fullscreen);

    return VLC_SUCCESS;

error:
    Close (obj);
    return VLC_EGENERIC;
}


/**
 * Disconnect from the X server.
 */
static void Close (vlc_object_t *obj)
{
    vout_display_t *vd = (vout_display_t *)obj;
    vout_display_sys_t *sys = vd->sys;
    Display *dpy = sys->display;

    if (sys->gl.sys != NULL)
        vout_display_opengl_Delete (sys->vgl);

    if (sys->ctx != NULL)
    {
        if (sys->v1_3)
            glXMakeContextCurrent (dpy, None, None, NULL);
        else
            glXMakeCurrent (dpy, None, NULL);
        glXDestroyContext (dpy, sys->ctx);
        if (sys->v1_3)
            glXDestroyWindow (dpy, sys->glwin);
    }
    XCloseDisplay (dpy);

    /* show the default cursor */
    xcb_change_window_attributes (sys->conn, sys->embed->handle.xid,
                               XCB_CW_CURSOR, &(uint32_t) { XCB_CURSOR_NONE });
    xcb_flush (sys->conn);
    xcb_disconnect (sys->conn);

    vout_display_DeleteWindow (vd, sys->embed);
    free (sys);
}

static void SwapBuffers (vlc_gl_t *gl)
{
    vout_display_sys_t *sys = gl->sys;

    glXSwapBuffers (sys->display, sys->glwin);
}

static void *GetProcAddress (vlc_gl_t *gl, const char *name)
{
    (void)gl;
#ifdef GLX_ARB_get_proc_address
    return glXGetProcAddressARB ((const GLubyte *)name);
#else
    return NULL;
#endif
}

/**
 * Return a direct buffer
 */
static picture_pool_t *Pool (vout_display_t *vd, unsigned requested_count)
{
    vout_display_sys_t *sys = vd->sys;

    if (!sys->pool)
        sys->pool = vout_display_opengl_GetPool (sys->vgl, requested_count);
    return sys->pool;
}

static void PictureRender (vout_display_t *vd, picture_t *pic, subpicture_t *subpicture)
{
    vout_display_sys_t *sys = vd->sys;

    vout_display_opengl_Prepare (sys->vgl, pic, subpicture);
}

static void PictureDisplay (vout_display_t *vd, picture_t *pic, subpicture_t *subpicture)
{
    vout_display_sys_t *sys = vd->sys;

    vout_display_opengl_Display (sys->vgl, &vd->source);
    picture_Release (pic);
    if (subpicture)
        subpicture_Delete(subpicture);
}

static int Control (vout_display_t *vd, int query, va_list ap)
{
    vout_display_sys_t *sys = vd->sys;

    switch (query)
    {
    case VOUT_DISPLAY_CHANGE_FULLSCREEN:
    {
        const vout_display_cfg_t *c = va_arg (ap, const vout_display_cfg_t *);
        return vout_window_SetFullScreen (sys->embed, c->is_fullscreen);
    }

    case VOUT_DISPLAY_CHANGE_WINDOW_STATE:
    {
        unsigned state = va_arg (ap, unsigned);
        return vout_window_SetState (sys->embed, state);
    }

    case VOUT_DISPLAY_CHANGE_DISPLAY_SIZE:
    case VOUT_DISPLAY_CHANGE_DISPLAY_FILLED:
    case VOUT_DISPLAY_CHANGE_ZOOM:
    case VOUT_DISPLAY_CHANGE_SOURCE_ASPECT:
    case VOUT_DISPLAY_CHANGE_SOURCE_CROP:
    {
        const vout_display_cfg_t *cfg;
        const video_format_t *source;
        bool is_forced = false;

        if (query == VOUT_DISPLAY_CHANGE_SOURCE_ASPECT
         || query == VOUT_DISPLAY_CHANGE_SOURCE_CROP)
        {
            source = (const video_format_t *)va_arg (ap, const video_format_t *);
            cfg = vd->cfg;
        }
        else
        {
            source = &vd->source;
            cfg = (const vout_display_cfg_t*)va_arg (ap, const vout_display_cfg_t *);
            if (query == VOUT_DISPLAY_CHANGE_DISPLAY_SIZE)
                is_forced = (bool)va_arg (ap, int);
        }

        /* */
        if (query == VOUT_DISPLAY_CHANGE_DISPLAY_SIZE
         && is_forced
         && (cfg->display.width  != vd->cfg->display.width
           ||cfg->display.height != vd->cfg->display.height)
         && vout_window_SetSize (sys->embed,
                                 cfg->display.width, cfg->display.height))
            return VLC_EGENERIC;

        vout_display_place_t place;
        vout_display_PlacePicture (&place, source, cfg, false);

        /* Move the picture within the window */
        const uint32_t values[] = { place.x, place.y,
                                    place.width, place.height, };
        xcb_void_cookie_t ck =
            xcb_configure_window_checked (sys->conn, sys->window,
                            XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y
                          | XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT,
                              values);
        if (CheckError (vd, sys->conn, "cannot resize X11 window", ck))
            return VLC_EGENERIC;

        glViewport (0, 0, place.width, place.height);
        return VLC_SUCCESS;
    }

    /* Hide the mouse. It will be send when
     * vout_display_t::info.b_hide_mouse is false */
    case VOUT_DISPLAY_HIDE_MOUSE:
        xcb_change_window_attributes (sys->conn, sys->embed->handle.xid,
                                    XCB_CW_CURSOR, &(uint32_t){ sys->cursor });
        xcb_flush (sys->conn);
        return VLC_SUCCESS;

    case VOUT_DISPLAY_GET_OPENGL:
    {
        vlc_gl_t **gl = va_arg (ap, vlc_gl_t **);
        *gl = &sys->gl;
        return VLC_SUCCESS;
    }

    case VOUT_DISPLAY_RESET_PICTURES:
        assert (0);
    default:
        msg_Err (vd, "Unknown request in XCB vout display");
        return VLC_EGENERIC;
    }
}

static void Manage (vout_display_t *vd)
{
    vout_display_sys_t *sys = vd->sys;

    ManageEvent (vd, sys->conn, &sys->visible);
}
