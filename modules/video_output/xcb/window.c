/**
 * @file window.c
 * @brief X C Bindings window provider module for VLC media player
 */
/*****************************************************************************
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

#include <stdarg.h>
#include <assert.h>
#include <poll.h>
#include <unistd.h> /* gethostname() and sysconf() */
#include <limits.h> /* _POSIX_HOST_NAME_MAX */

#include <xcb/xcb.h>
typedef xcb_atom_t Atom;
#include <X11/Xatom.h> /* XA_WM_NAME */

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_vout_window.h>

#include "xcb_vlc.h"

#define XID_TEXT N_("ID of the video output X window")
#define XID_LONGTEXT N_( \
    "VLC can embed its video output in an existing X11 window. " \
    "This is the X identifier of that window (0 means none).")

static int  Open (vlc_object_t *);
static void Close (vlc_object_t *);
static int  EmOpen (vlc_object_t *);
static void EmClose (vlc_object_t *);

/*
 * Module descriptor
 */
vlc_module_begin ()
    set_shortname (N_("X window"))
    set_description (N_("X11 video window (XCB)"))
    set_category (CAT_VIDEO)
    set_subcategory (SUBCAT_VIDEO_VOUT)
    set_capability ("vout window xid", 10)
    set_callbacks (Open, Close)

    /* Obsolete since 1.1.0: */
    add_obsolete_bool ("x11-altfullscreen")
    add_obsolete_bool ("xvideo-altfullscreen")
    add_obsolete_bool ("xvmc-altfullscreen")
    add_obsolete_bool ("glx-altfullscreen")

    add_submodule ()
    set_shortname (N_("Drawable"))
    set_description (N_("Embedded window video"))
    set_category (CAT_VIDEO)
    set_subcategory (SUBCAT_VIDEO_VOUT)
    set_capability ("vout window xid", 70)
    set_callbacks (EmOpen, EmClose)

    add_integer ("drawable-xid", 0, NULL, XID_TEXT, XID_LONGTEXT, true)
        change_volatile ()

vlc_module_end ()

static int Control (vout_window_t *, int, va_list ap);
static void *Thread (void *);

#define MATCHBOX_HACK 1 /* Matchbox focus hack */

struct vout_window_sys_t
{
    xcb_connection_t *conn;
    key_handler_t *keys;
    vlc_thread_t thread;

    xcb_window_t root;
    xcb_atom_t wm_state;
    xcb_atom_t wm_state_above;
    xcb_atom_t wm_state_below;
    xcb_atom_t wm_state_fullscreen;
#ifdef MATCHBOX_HACK
    xcb_atom_t mb_current_app_window;
#endif
};

/** Set an X window property from a nul-terminated string */
static inline
void set_string (xcb_connection_t *conn, xcb_window_t window,
                 xcb_atom_t type, xcb_atom_t atom, const char *str)
{
    xcb_change_property (conn, XCB_PROP_MODE_REPLACE, window, atom, type,
                         /* format */ 8, strlen (str), str);
}

/** Set an X window string property */
static inline
void set_ascii_prop (xcb_connection_t *conn, xcb_window_t window,
                     xcb_atom_t atom, const char *value)
{
    set_string (conn, window, XA_STRING, atom, value);
}

static inline
void set_wm_hints (xcb_connection_t *conn, xcb_window_t window)
{
    static const uint32_t wm_hints[8] = {
        3, /* flags: Input, Initial state */
        1, /* input: True */
        1, /* initial state: Normal */
        0, 0, 0, 0, 0, /* Icon */
    };
    xcb_change_property (conn, XCB_PROP_MODE_REPLACE, window, XA_WM_HINTS,
                         XA_WM_HINTS, 32, 8, wm_hints);
}

/** Set the Window ICCCM client machine property */
static inline
void set_hostname_prop (xcb_connection_t *conn, xcb_window_t window)
{
    char* hostname;
    long host_name_max = sysconf (_SC_HOST_NAME_MAX);
    if (host_name_max <= 0) host_name_max = _POSIX_HOST_NAME_MAX;
    hostname = malloc (host_name_max);
    if(!hostname) return;

    if (gethostname (hostname, host_name_max) == 0)
    {
        hostname[host_name_max - 1] = '\0';
        set_ascii_prop (conn, window, XA_WM_CLIENT_MACHINE, hostname);
    }
    free(hostname);
}

/** Request the X11 server to internalize a string into an atom */
static inline
xcb_intern_atom_cookie_t intern_string (xcb_connection_t *c, const char *s)
{
    return xcb_intern_atom (c, 0, strlen (s), s);
}

/** Extract the X11 atom from an intern request cookie */
static
xcb_atom_t get_atom (xcb_connection_t *conn, xcb_intern_atom_cookie_t ck)
{
    xcb_intern_atom_reply_t *reply;
    xcb_atom_t atom;

    reply = xcb_intern_atom_reply (conn, ck, NULL);
    if (reply == NULL)
        return 0;

    atom = reply->atom;
    free (reply);
    return atom;
}

#define NET_WM_STATE_REMOVE 0
#define NET_WM_STATE_ADD    1
#define NET_WM_STATE_TOGGLE 2

static void CacheAtoms (vout_window_sys_t *p_sys)
{
    xcb_connection_t *conn = p_sys->conn;
    xcb_intern_atom_cookie_t wm_state_ck, wm_state_above_ck,
                             wm_state_below_ck, wm_state_fs_ck;

    wm_state_ck = intern_string (conn, "_NET_WM_STATE");
    wm_state_above_ck = intern_string (conn, "_NET_WM_STATE_ABOVE");
    wm_state_below_ck = intern_string (conn, "_NET_WM_STATE_BELOW");
    wm_state_fs_ck = intern_string (conn, "_NET_WM_STATE_FULLSCREEN");
#ifdef MATCHBOX_HACK
    xcb_intern_atom_cookie_t mb_current_app_window;
    mb_current_app_window = xcb_intern_atom (conn, true,
                                             strlen ("_MB_CURRENT_APP_WINDOW"),
                                             "_MB_CURRENT_APP_WINDOW");
#endif

    p_sys->wm_state = get_atom (conn, wm_state_ck);
    p_sys->wm_state_above = get_atom (conn, wm_state_above_ck);
    p_sys->wm_state_below = get_atom (conn, wm_state_below_ck);
    p_sys->wm_state_fullscreen = get_atom (conn, wm_state_fs_ck);
#ifdef MATCHBOX_HACK
    p_sys->mb_current_app_window = get_atom (conn, mb_current_app_window);
#endif
}

/**
 * Create an X11 window.
 */
static int Open (vlc_object_t *obj)
{
    vout_window_t *wnd = (vout_window_t *)obj;
    xcb_generic_error_t *err;
    xcb_void_cookie_t ck;

    vout_window_sys_t *p_sys = malloc (sizeof (*p_sys));
    if (p_sys == NULL)
        return VLC_ENOMEM;

    /* Connect to X */
    char *display = var_CreateGetNonEmptyString (wnd, "x11-display");
    int snum;

    xcb_connection_t *conn = xcb_connect (display, &snum);
    if (xcb_connection_has_error (conn) /*== NULL*/)
        goto error;

    /* Find configured screen */
    const xcb_setup_t *setup = xcb_get_setup (conn);
    const xcb_screen_t *scr = NULL;
    for (xcb_screen_iterator_t i = xcb_setup_roots_iterator (setup);
         i.rem > 0; xcb_screen_next (&i))
    {
        if (snum == 0)
        {
            scr = i.data;
            break;
        }
        snum--;
    }
    if (scr == NULL)
    {
        msg_Err (wnd, "bad X11 screen number");
        goto error;
    }

    /* Create window */
    const uint32_t mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
    uint32_t values[2] = {
        /* XCB_CW_BACK_PIXEL */
        scr->black_pixel,
        /* XCB_CW_EVENT_MASK */
        XCB_EVENT_MASK_KEY_PRESS,
    };

    xcb_window_t window = xcb_generate_id (conn);
    ck = xcb_create_window_checked (conn, scr->root_depth, window, scr->root,
                                    wnd->cfg->x, wnd->cfg->y,
                                    wnd->cfg->width, wnd->cfg->height, 0,
                                    XCB_WINDOW_CLASS_INPUT_OUTPUT,
                                    scr->root_visual, mask, values);
    err = xcb_request_check (conn, ck);
    if (err)
    {
        msg_Err (wnd, "creating window: X11 error %d", err->error_code);
        free (err);
        goto error;
    }

    wnd->handle.xid = window;
    wnd->display.x11 = display;
    wnd->control = Control;
    wnd->sys = p_sys;

    p_sys->conn = conn;
    if (var_CreateGetBool (obj, "keyboard-events"))
        p_sys->keys = CreateKeyHandler (obj, conn);
    else
        p_sys->keys = NULL;
    p_sys->root = scr->root;

    /* ICCCM
     * No cut&paste nor drag&drop, only Window Manager communication. */
    set_ascii_prop (conn, window, XA_WM_NAME,
    /* xgettext: This is a plain ASCII spelling of "VLC media player"
       for the ICCCM window name. This must be pure ASCII.
       The limitation is partially with ICCCM and partially with VLC.
       For Latin script languages, you may need to strip accents.
       For other scripts, you will need to transliterate into Latin. */
                    vlc_pgettext ("ASCII", "VLC media player"));

    set_ascii_prop (conn, window, XA_WM_ICON_NAME,
    /* xgettext: This is a plain ASCII spelling of "VLC"
       for the ICCCM window name. This must be pure ASCII. */
                    vlc_pgettext ("ASCII", "VLC"));
    set_wm_hints (conn, window);
    xcb_change_property (conn, XCB_PROP_MODE_REPLACE, window, XA_WM_CLASS,
                         XA_STRING, 8, 8, "vlc\0Vlc");
    set_hostname_prop (conn, window);

    /* EWMH */
    xcb_intern_atom_cookie_t utf8_string_ck
        = intern_string (conn, "UTF8_STRING");;
    xcb_intern_atom_cookie_t net_wm_name_ck
        = intern_string (conn, "_NET_WM_NAME");
    xcb_intern_atom_cookie_t net_wm_icon_name_ck
        = intern_string (conn, "_NET_WM_ICON_NAME");
    xcb_intern_atom_cookie_t wm_window_role_ck
        = intern_string (conn, "WM_WINDOW_ROLE");

    xcb_atom_t utf8 = get_atom (conn, utf8_string_ck);

    xcb_atom_t net_wm_name = get_atom (conn, net_wm_name_ck);
    char *title = var_CreateGetNonEmptyString (wnd, "video-title");
    if (title)
    {
        set_string (conn, window, utf8, net_wm_name, title);
        free (title);
    }
    else
        set_string (conn, window, utf8, net_wm_name, _("VLC media player"));

    xcb_atom_t net_wm_icon_name = get_atom (conn, net_wm_icon_name_ck);
    set_string (conn, window, utf8, net_wm_icon_name, _("VLC"));

    xcb_atom_t wm_window_role = get_atom (conn, wm_window_role_ck);
    set_ascii_prop (conn, window, wm_window_role, "vlc-video");

    /* Cache any EWMH atom we may need later */
    CacheAtoms (p_sys);
#ifdef MATCHBOX_HACK
    if (p_sys->mb_current_app_window)
    {
        uint32_t value = XCB_EVENT_MASK_PROPERTY_CHANGE;
        xcb_change_window_attributes (conn, scr->root,
                                      XCB_CW_EVENT_MASK, &value);
    }
#endif

    /* Make the window visible */
    xcb_map_window (conn, window);

    if (var_CreateGetBool (obj, "video-wallpaper"))
    {
        vout_window_SetState (wnd, VOUT_WINDOW_STATE_BELOW);
        vout_window_SetFullScreen (wnd, true);
    }

    /* Create the event thread. It will dequeue all events, so any checked
     * request from this thread must be completed at this point. */
    if ((p_sys->keys != NULL)
     && vlc_clone (&p_sys->thread, Thread, wnd, VLC_THREAD_PRIORITY_LOW))
        DestroyKeyHandler (p_sys->keys);

#ifdef MATCHBOX_HACK
    if (p_sys->mb_current_app_window)
        xcb_set_input_focus (p_sys->conn, XCB_INPUT_FOCUS_POINTER_ROOT,
                             wnd->handle.xid, XCB_CURRENT_TIME);
#endif
    xcb_flush (conn); /* Make sure map_window is sent (should be useless) */
    return VLC_SUCCESS;

error:
    xcb_disconnect (conn);
    free (display);
    free (p_sys);
    return VLC_EGENERIC;
}


/**
 * Destroys the X11 window.
 */
static void Close (vlc_object_t *obj)
{
    vout_window_t *wnd = (vout_window_t *)obj;
    vout_window_sys_t *p_sys = wnd->sys;
    xcb_connection_t *conn = p_sys->conn;

    if (p_sys->keys)
    {
        vlc_cancel (p_sys->thread);
        vlc_join (p_sys->thread, NULL);
        DestroyKeyHandler (p_sys->keys);
    }
    xcb_disconnect (conn);
    free (wnd->display.x11);
    free (p_sys);
}


/** Background thread for X11 events handling */
static void *Thread (void *data)
{
    vout_window_t *wnd = data;
    vout_window_sys_t *p_sys = wnd->sys;
    xcb_connection_t *conn = p_sys->conn;

    int fd = xcb_get_file_descriptor (conn);
    if (fd == -1)
        return NULL;

    for (;;)
    {
        xcb_generic_event_t *ev;
        struct pollfd ufd = { .fd = fd, .events = POLLIN, };

        poll (&ufd, 1, -1);

        int canc = vlc_savecancel ();
        while ((ev = xcb_poll_for_event (conn)) != NULL)
        {
            if (ProcessKeyEvent (p_sys->keys, ev) == 0)
                continue;
#ifdef MATCHBOX_HACK
            if (p_sys->mb_current_app_window
             && (ev->response_type & 0x7f) == XCB_PROPERTY_NOTIFY)
            {
                const xcb_property_notify_event_t *pne =
                    (xcb_property_notify_event_t *)ev;
                if (pne->atom == p_sys->mb_current_app_window
                 && pne->state == XCB_PROPERTY_NEW_VALUE)
                {
                    xcb_get_property_reply_t *r =
                        xcb_get_property_reply (conn,
                            xcb_get_property (conn, 0, pne->window, pne->atom,
                                              XA_WINDOW, 0, 4), NULL);
                    if (r != NULL
                     && !memcmp (xcb_get_property_value (r), &wnd->handle.xid,
                                 4))
                    {
                        msg_Dbg (wnd, "asking Matchbox for input focus");
                        xcb_set_input_focus (conn,
                                             XCB_INPUT_FOCUS_POINTER_ROOT,
                                             wnd->handle.xid, pne->time);
                        xcb_flush (conn);
                    }
                    free (r);
                }
            }
            else
#endif
                msg_Dbg (wnd, "unhandled event: %"PRIu8, ev->response_type);
            free (ev);
        }
        vlc_restorecancel (canc);

        if (xcb_connection_has_error (conn))
        {
            msg_Err (wnd, "X server failure");
            break;
        }
    }
    return NULL;
}

/** Changes the EWMH state of the window */
static void set_wm_state (vout_window_t *wnd, bool on, xcb_atom_t state)
{
    vout_window_sys_t *sys = wnd->sys;
    /* From EWMH "_WM_STATE" */
    xcb_client_message_event_t ev = {
         .response_type = XCB_CLIENT_MESSAGE,
         .format = 32,
         .window = wnd->handle.xid,
         .type = sys->wm_state,
    };

    ev.data.data32[0] = on ? NET_WM_STATE_ADD : NET_WM_STATE_REMOVE;
    ev.data.data32[1] = state;
    ev.data.data32[2] = 0;
    ev.data.data32[3] = 1;

    /* From ICCCM "Changing Window State" */
    xcb_send_event (sys->conn, 0, sys->root,
                    XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY |
                    XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT,
                    (const char *)&ev);
}


static int Control (vout_window_t *wnd, int cmd, va_list ap)
{
    vout_window_sys_t *p_sys = wnd->sys;
    xcb_connection_t *conn = p_sys->conn;

    switch (cmd)
    {
        case VOUT_WINDOW_SET_SIZE:
        {
            unsigned width = va_arg (ap, unsigned);
            unsigned height = va_arg (ap, unsigned);
            const uint32_t values[] = { width, height, };

            xcb_configure_window (conn, wnd->handle.xid,
                                  XCB_CONFIG_WINDOW_WIDTH |
                                  XCB_CONFIG_WINDOW_HEIGHT, values);
            break;
        }

        case VOUT_WINDOW_SET_STATE:
        {
            unsigned state = va_arg (ap, unsigned);
            bool above = (state & VOUT_WINDOW_STATE_ABOVE) != 0;
            bool below = (state & VOUT_WINDOW_STATE_BELOW) != 0;

            set_wm_state (wnd, above, p_sys->wm_state_above);
            set_wm_state (wnd, below, p_sys->wm_state_below);
            break;
        }

        case VOUT_WINDOW_SET_FULLSCREEN:
        {
            bool fs = va_arg (ap, int);
            if (!fs && var_GetBool (wnd, "video-wallpaper"))
                return VLC_EGENERIC;
            set_wm_state (wnd, fs, p_sys->wm_state_fullscreen);
            break;
        }

        default:
            msg_Err (wnd, "request %d not implemented", cmd);
            return VLC_EGENERIC;
    }
    xcb_flush (p_sys->conn);
    return VLC_SUCCESS;
}

/*** Embedded drawable support ***/

static vlc_mutex_t serializer = VLC_STATIC_MUTEX;

/** Acquire a drawable */
static int AcquireDrawable (vlc_object_t *obj, xcb_window_t window)
{
    xcb_window_t *used;
    size_t n = 0;

    if (var_Create (obj->p_libvlc, "xid-in-use", VLC_VAR_ADDRESS))
        return VLC_ENOMEM;

    /* Keep a list of busy drawables, so we don't overlap videos if there are
     * more than one video track in the stream. */
    vlc_mutex_lock (&serializer);
    used = var_GetAddress (obj->p_libvlc, "xid-in-use");
    if (used != NULL)
    {
        while (used[n])
        {
            if (used[n] == window)
                goto skip;
            n++;
        }
    }

    used = realloc (used, sizeof (*used) * (n + 2));
    if (used != NULL)
    {
        used[n] = window;
        used[n + 1] = 0;
        var_SetAddress (obj->p_libvlc, "xid-in-use", used);
    }
    else
    {
skip:
        msg_Warn (obj, "X11 drawable 0x%08"PRIx8" is busy", window);
        window = 0;
    }
    vlc_mutex_unlock (&serializer);

    return (window == 0) ? VLC_EGENERIC : VLC_SUCCESS;
}

/** Remove this drawable from the list of busy ones */
static void ReleaseDrawable (vlc_object_t *obj, xcb_window_t window)
{
    xcb_window_t *used;
    size_t n = 0;

    vlc_mutex_lock (&serializer);
    used = var_GetAddress (obj->p_libvlc, "xid-in-use");
    assert (used);
    while (used[n] != window)
    {
        assert (used[n]);
        n++;
    }
    do
        used[n] = used[n + 1];
    while (used[++n]);

    if (n == 0)
         var_SetAddress (obj->p_libvlc, "xid-in-use", NULL);
    vlc_mutex_unlock (&serializer);

    if (n == 0)
        free (used);
    /* Variables are reference-counted... */
    var_Destroy (obj->p_libvlc, "xid-in-use");
}

/**
 * Wrap an existing X11 window to embed the video.
 */
static int EmOpen (vlc_object_t *obj)
{
    vout_window_t *wnd = (vout_window_t *)obj;

    xcb_window_t window = var_CreateGetInteger (obj, "drawable-xid");
    if (window == 0)
        return VLC_EGENERIC;
    var_Destroy (obj, "drawable-xid");

    if (AcquireDrawable (obj, window))
        return VLC_EGENERIC;

    vout_window_sys_t *p_sys = malloc (sizeof (*p_sys));
    xcb_connection_t *conn = xcb_connect (NULL, NULL);
    if (p_sys == NULL || xcb_connection_has_error (conn))
        goto error;

    p_sys->keys = NULL;
    wnd->handle.xid = window;
    wnd->control = Control;
    wnd->sys = p_sys;

    p_sys->conn = conn;

    xcb_get_geometry_reply_t *geo =
        xcb_get_geometry_reply (conn, xcb_get_geometry (conn, window), NULL);
    if (geo == NULL)
    {
        msg_Err (obj, "bad X11 window 0x%08"PRIx8, window);
        goto error;
    }
    p_sys->root = geo->root;
    free (geo);

    if (var_CreateGetBool (obj, "keyboard-events"))
    {
        p_sys->keys = CreateKeyHandler (obj, conn);
        if (p_sys->keys != NULL)
        {
            const uint32_t mask = XCB_CW_EVENT_MASK;
            const uint32_t values[1] = {
                XCB_EVENT_MASK_KEY_PRESS,
            };
            xcb_change_window_attributes (conn, window, mask, values);
        }
    }

    CacheAtoms (p_sys);
    if ((p_sys->keys != NULL)
     && vlc_clone (&p_sys->thread, Thread, wnd, VLC_THREAD_PRIORITY_LOW))
        DestroyKeyHandler (p_sys->keys);

    xcb_flush (conn);

    return VLC_SUCCESS;

error:
    xcb_disconnect (conn);
    free (p_sys);
    ReleaseDrawable (obj, window);
    return VLC_EGENERIC;
}

static void EmClose (vlc_object_t *obj)
{
    vout_window_t *wnd = (vout_window_t *)obj;
    xcb_window_t window = wnd->handle.xid;

    Close (obj);
    ReleaseDrawable (obj, window);
}
