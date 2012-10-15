/**
 * \file vlcpulse.c
 * \brief PulseAudio support library for LibVLC plugins
 */
/*****************************************************************************
 * Copyright (C) 2009-2011 Rémi Denis-Courmont
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <pulse/pulseaudio.h>

#include "vlcpulse.h"
#include <assert.h>
#include <stdlib.h>
#include <locale.h>
#include <unistd.h>
#include <pwd.h>

#undef vlc_pa_error
void vlc_pa_error (vlc_object_t *obj, const char *msg, pa_context *ctx)
{
    msg_Err (obj, "%s: %s", msg, pa_strerror (pa_context_errno (ctx)));
}

static void context_state_cb (pa_context *ctx, void *userdata)
{
    pa_threaded_mainloop *mainloop = userdata;

    switch (pa_context_get_state(ctx))
    {
        case PA_CONTEXT_READY:
        case PA_CONTEXT_FAILED:
        case PA_CONTEXT_TERMINATED:
            pa_threaded_mainloop_signal (mainloop, 0);
        default:
            break;
    }
}

static bool context_wait (pa_context *ctx, pa_threaded_mainloop *mainloop)
{
    pa_context_state_t state;

    while ((state = pa_context_get_state (ctx)) != PA_CONTEXT_READY)
    {
        if (state == PA_CONTEXT_FAILED || state == PA_CONTEXT_TERMINATED)
            return -1;
        pa_threaded_mainloop_wait (mainloop);
    }
    return 0;
}

/**
 * Initializes the PulseAudio main loop and connects to the PulseAudio server.
 * @return a PulseAudio context on success, or NULL on error
 */
pa_context *vlc_pa_connect (vlc_object_t *obj, pa_threaded_mainloop **mlp)
{
    msg_Dbg (obj, "using library version %s", pa_get_library_version ());
    msg_Dbg (obj, " (compiled with version %s, protocol %u)",
             pa_get_headers_version (), PA_PROTOCOL_VERSION);

    /* Initialize main loop */
    pa_threaded_mainloop *mainloop = pa_threaded_mainloop_new ();
    if (unlikely(mainloop == NULL))
        return NULL;

    if (pa_threaded_mainloop_start (mainloop) < 0)
    {
        pa_threaded_mainloop_free (mainloop);
        return NULL;
    }

    /* Fill in context (client) properties */
    char *ua = var_InheritString (obj, "user-agent");
    pa_proplist *props = pa_proplist_new ();
    if (likely(props != NULL))
    {
        pa_proplist_sets (props, PA_PROP_APPLICATION_NAME, ua);
        pa_proplist_sets (props, PA_PROP_APPLICATION_ID, "org.VideoLAN.VLC");
        pa_proplist_sets (props, PA_PROP_APPLICATION_VERSION, PACKAGE_VERSION);
        pa_proplist_sets (props, PA_PROP_APPLICATION_ICON_NAME, PACKAGE_NAME);
        //pa_proplist_sets (props, PA_PROP_APPLICATION_LANGUAGE, _("C"));
        pa_proplist_sets (props, PA_PROP_APPLICATION_LANGUAGE,
                          setlocale (LC_MESSAGES, NULL));

        pa_proplist_setf (props, PA_PROP_APPLICATION_PROCESS_ID, "%lu",
                          (unsigned long) getpid ());
        //pa_proplist_sets (props, PA_PROP_APPLICATION_PROCESS_BINARY,
        //                  PACKAGE_NAME);

        for (size_t max = sysconf (_SC_GETPW_R_SIZE_MAX), len = max % 1024 + 1024;
             len < max; len += 1024)
        {
            struct passwd pwbuf, *pw;
            char buf[len];

            if (getpwuid_r (getuid (), &pwbuf, buf, sizeof (buf), &pw) == 0
             && pw != NULL)
                pa_proplist_sets (props, PA_PROP_APPLICATION_PROCESS_USER,
                                  pw->pw_name);
        }

        for (size_t max = sysconf (_SC_HOST_NAME_MAX), len = max % 1024 + 1024;
             len < max; len += 1024)
        {
            char hostname[len];

            if (gethostname (hostname, sizeof (hostname)) == 0)
                pa_proplist_sets (props, PA_PROP_APPLICATION_PROCESS_HOST,
                                  hostname);
        }

        const char *session = getenv ("XDG_SESSION_COOKIE");
        if (session != NULL)
        {
            pa_proplist_setf (props, PA_PROP_APPLICATION_PROCESS_MACHINE_ID,
                              "%.32s", session); /* XXX: is this valid? */
            pa_proplist_sets (props, PA_PROP_APPLICATION_PROCESS_SESSION_ID,
                              session);
        }
    }

    /* Connect to PulseAudio daemon */
    pa_context *ctx;
    pa_mainloop_api *api;

    pa_threaded_mainloop_lock (mainloop);
    api = pa_threaded_mainloop_get_api (mainloop);
    ctx = pa_context_new_with_proplist (api, ua, props);
    free (ua);
    if (props != NULL)
        pa_proplist_free (props);
    if (unlikely(ctx == NULL))
        goto fail;

    pa_context_set_state_callback (ctx, context_state_cb, mainloop);
    if (pa_context_connect (ctx, NULL, 0, NULL) < 0
     || context_wait (ctx, mainloop))
    {
        vlc_pa_error (obj, "PulseAudio server connection failure", ctx);
        pa_context_unref (ctx);
        goto fail;
    }
    msg_Dbg (obj, "connected %s to %s as client #%"PRIu32,
             pa_context_is_local (ctx) ? "locally" : "remotely",
             pa_context_get_server (ctx), pa_context_get_index (ctx));
    msg_Dbg (obj, "using protocol %"PRIu32", server protocol %"PRIu32,
             pa_context_get_protocol_version (ctx),
             pa_context_get_server_protocol_version (ctx));

    pa_threaded_mainloop_unlock (mainloop);
    *mlp = mainloop;
    return ctx;

fail:
    pa_threaded_mainloop_unlock (mainloop);
    pa_threaded_mainloop_stop (mainloop);
    pa_threaded_mainloop_free (mainloop);
    return NULL;
}

/**
 * Closes a connection to PulseAudio.
 */
void vlc_pa_disconnect (vlc_object_t *obj, pa_context *ctx,
                        pa_threaded_mainloop *mainloop)
{
    pa_threaded_mainloop_lock (mainloop);
    pa_context_disconnect (ctx);
    pa_context_set_state_callback (ctx, NULL, NULL);
    pa_context_unref (ctx);
    pa_threaded_mainloop_unlock (mainloop);

    pa_threaded_mainloop_stop (mainloop);
    pa_threaded_mainloop_free (mainloop);
    (void) obj;
}

/**
 * Frees a timer event.
 * \note Timer events can be created with pa_context_rttime_new().
 * \warning This function must be called from the mainloop,
 * or with the mainloop lock held.
 */
void vlc_pa_rttime_free (pa_threaded_mainloop *mainloop, pa_time_event *e)
{
    pa_mainloop_api *api = pa_threaded_mainloop_get_api (mainloop);

    api->time_free (e);
}

#undef vlc_pa_get_latency
/**
 * Gets latency of a PulseAudio stream.
 * \return the latency or VLC_TS_INVALID on error.
 */
mtime_t vlc_pa_get_latency(vlc_object_t *obj, pa_context *ctx, pa_stream *s)
{
    pa_usec_t latency;
    int negative;

    if (pa_stream_get_latency(s, &latency, &negative)) {
        if (pa_context_errno (ctx) != PA_ERR_NODATA)
            vlc_pa_error(obj, "unknown latency", ctx);
        return VLC_TS_INVALID;
    }
    return negative ? -latency : +latency;
}
