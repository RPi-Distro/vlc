/*****************************************************************************
 * netsync.c: synchronisation between several network clients.
 *****************************************************************************
 * Copyright (C) 2004-2009 the VideoLAN team
 * $Id: 455be184dcbb1f71c74630c12d707a43ecba0667 $
 *
 * Authors: Gildas Bazin <gbazin@videolan.org>
 *          Jean-Paul Saman <jpsaman@videolan.org>
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
#include <assert.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_interface.h>
#include <vlc_input.h>
#include <vlc_playlist.h>

#ifdef HAVE_UNISTD_H
#    include <unistd.h>
#endif
#include <sys/types.h>
#ifdef HAVE_POLL
#   include <poll.h>
#endif

#include <vlc_network.h>

#define NETSYNC_PORT 9875

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open (vlc_object_t *);
static void Close(vlc_object_t *);

#define NETSYNC_TEXT N_("Network master clock")
#define NETSYNC_LONGTEXT N_("When set then " \
  "this vlc instance shall dictate its clock for synchronisation" \
  "over clients listening on the masters network ip address")

#define MIP_TEXT N_("Master server ip address")
#define MIP_LONGTEXT N_("The IP address of " \
  "the network master clock to use for clock synchronisation.")

#define NETSYNC_TIMEOUT_TEXT N_("UDP timeout (in ms)")
#define NETSYNC_TIMEOUT_LONGTEXT N_("Amount of time (in ms) " \
  "to wait before aborting network reception of data.")

vlc_module_begin()
    set_shortname(N_("Network Sync"))
    set_description(N_("Network synchronisation"))
    set_category(CAT_ADVANCED)
    set_subcategory(SUBCAT_ADVANCED_MISC)

    add_bool("netsync-master", false, NULL,
              NETSYNC_TEXT, NETSYNC_LONGTEXT, true)
    add_string("netsync-master-ip", NULL, NULL, MIP_TEXT, MIP_LONGTEXT,
                true)
    add_integer("netsync-timeout", 500, NULL,
                 NETSYNC_TIMEOUT_TEXT, NETSYNC_TIMEOUT_LONGTEXT, true)

    set_capability("interface", 0)
    set_callbacks(Open, Close)
vlc_module_end()

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
struct intf_sys_t {
    int            fd;
    int            timeout;
    bool           is_master;
    playlist_t     *playlist;

    /* */
    input_thread_t *input;
    vlc_thread_t   thread;
};

static int PlaylistEvent(vlc_object_t *, char const *cmd,
                         vlc_value_t oldval, vlc_value_t newval, void *data);

/*****************************************************************************
 * Activate: initialize and create stuff
 *****************************************************************************/
static int Open(vlc_object_t *object)
{
    intf_thread_t *intf = (intf_thread_t*)object;
    intf_sys_t    *sys;
    int fd;

    if (!var_InheritBool(intf, "netsync-master")) {
        char *psz_master = var_InheritString(intf, "netsync-master-ip");
        if (psz_master == NULL) {
            msg_Err(intf, "master address not specified");
            return VLC_EGENERIC;
        }
        fd = net_ConnectUDP(VLC_OBJECT(intf), psz_master, NETSYNC_PORT, -1);
        free(psz_master);
    } else {
        fd = net_ListenUDP1(VLC_OBJECT(intf), NULL, NETSYNC_PORT);
    }

    if (fd == -1) {
        msg_Err(intf, "Netsync socket failure");
        return VLC_EGENERIC;
    }

    intf->pf_run = NULL;
    intf->p_sys = sys = malloc(sizeof(*sys));
    if (!sys) {
        net_Close(fd);
        return VLC_ENOMEM;
    }

    sys->fd = fd;
    sys->is_master = var_InheritBool(intf, "netsync-master");
    sys->timeout = var_InheritInteger(intf, "netsync-timeout");
    if (sys->timeout < 500)
        sys->timeout = 500;
    sys->playlist = pl_Get(intf);
    sys->input = NULL;

    var_AddCallback(sys->playlist, "input-current", PlaylistEvent, intf);
    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: destroy interface
 *****************************************************************************/
void Close(vlc_object_t *object)
{
    intf_thread_t *intf = (intf_thread_t*)object;
    intf_sys_t *sys = intf->p_sys;

    assert(sys->input == NULL);
    var_DelCallback(sys->playlist, "input-current", PlaylistEvent, intf);
    net_Close(sys->fd);
    free(sys);
}

static mtime_t GetPcrSystem(input_thread_t *input)
{
    int canc = vlc_savecancel();
    /* TODO use the delay */
    mtime_t system;
    if (input_GetPcrSystem(input, &system, NULL))
        system = -1;
    vlc_restorecancel(canc);

    return system;
}

static void *Master(void *handle)
{
    intf_thread_t *intf = handle;
    intf_sys_t *sys = intf->p_sys;
    for (;;) {
        struct pollfd ufd = { .fd = sys->fd, .events = POLLIN, };
        uint64_t data[2];

        if (poll(&ufd, 1, -1) <= 0)
            continue;

        /* We received something */
        struct sockaddr_storage from;
        unsigned struct_size = sizeof(from);
        recvfrom(sys->fd, data, sizeof(data), 0,
                 (struct sockaddr*)&from, &struct_size);

        mtime_t master_system = GetPcrSystem(sys->input);
        if (master_system < 0)
            continue;

        data[0] = hton64(mdate());
        data[1] = hton64(master_system);

        /* Reply to the sender */
        sendto(sys->fd, data, sizeof(data), 0,
               (struct sockaddr *)&from, struct_size);
#if 0
        /* not sure we need the client information to sync,
           since we are the master anyway */
        mtime_t client_system = ntoh64(data[0]);
        msg_Dbg(intf, "Master clockref: %"PRId64" -> %"PRId64", from %s "
                 "(date: %"PRId64")", client_system, master_system,
                 (from.ss_family == AF_INET) ? inet_ntoa(((struct sockaddr_in *)&from)->sin_addr)
                 : "non-IPv4", /*date*/ 0);
#endif
    }
}

static void *Slave(void *handle)
{
    intf_thread_t *intf = handle;
    intf_sys_t *sys = intf->p_sys;

    for (;;) {
        struct pollfd ufd = { .fd = sys->fd, .events = POLLIN, };
        uint64_t data[2];

        mtime_t system = GetPcrSystem(sys->input);
        if (system < 0)
            goto wait;

        /* Send clock request to the master */
        data[0] = hton64(system);

        const mtime_t send_date = mdate();
        if (send(sys->fd, data, sizeof(data[0]), 0) <= 0)
            goto wait;

        /* Don't block */
        int ret = poll(&ufd, 1, sys->timeout);
        if (ret == 0)
            continue;
        if (ret < 0)
            goto wait;

        const mtime_t receive_date = mdate();
        if (recv(sys->fd, data, sizeof(data), 0) <= 0)
            goto wait;

        const mtime_t master_date   = ntoh64(data[0]);
        const mtime_t master_system = ntoh64(data[1]);
        const mtime_t diff_date = receive_date -
                                  ((receive_date - send_date) / 2 + master_date);

        if (master_system > 0) {
            int canc = vlc_savecancel();

            mtime_t client_system;
            if (!input_GetPcrSystem(sys->input, &client_system, NULL)) {
                const mtime_t diff_system = client_system - master_system - diff_date;
                if (diff_system != 0) {
                    input_ModifyPcrSystem(sys->input, true, master_system - diff_date);
#if 0
                    msg_Dbg(intf, "Slave clockref: %"PRId64" -> %"PRId64" -> %"PRId64","
                             " clock diff: %"PRId64", diff: %"PRId64"",
                             system, master_system, client_system,
                             diff_system, diff_date);
#endif
                }
            }
            vlc_restorecancel(canc);
        }
    wait:
        msleep(INTF_IDLE_SLEEP);
    }
}

static int InputEvent(vlc_object_t *object, char const *cmd,
                      vlc_value_t oldval, vlc_value_t newval, void *data)
{
    VLC_UNUSED(cmd); VLC_UNUSED(oldval); VLC_UNUSED(object);
    intf_thread_t  *intf = data;
    intf_sys_t     *sys = intf->p_sys;

    if (newval.i_int == INPUT_EVENT_DEAD && sys->input) {
        msg_Err(intf, "InputEvent DEAD");
        vlc_cancel(sys->thread);
        vlc_join(sys->thread, NULL);
        vlc_object_release(sys->input);
        sys->input = NULL;
    }
    return VLC_SUCCESS;
}

static int PlaylistEvent(vlc_object_t *object, char const *cmd,
                         vlc_value_t oldval, vlc_value_t newval, void *data)
{
    VLC_UNUSED(cmd); VLC_UNUSED(oldval); VLC_UNUSED(object);
    intf_thread_t  *intf = data;
    intf_sys_t     *sys = intf->p_sys;

    input_thread_t *input = newval.p_address;
    assert(sys->input == NULL);
    sys->input = vlc_object_hold(input);
    if (vlc_clone(&sys->thread, sys->is_master ? Master : Slave, intf,
                  VLC_THREAD_PRIORITY_INPUT)) {
        vlc_object_release(input);
        return VLC_SUCCESS;
    }
    var_AddCallback(input, "intf-event", InputEvent, intf);
    return VLC_SUCCESS;
}

