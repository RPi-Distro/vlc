/*
 * events.c - libvlc smoke test
 *
 * $Id: 1ec047e7b580a6a359cb253836314f5c3f950ca5 $
 */

/**********************************************************************
 *  Copyright (C) 2007 Rémi Denis-Courmont.                           *
 *  Copyright (C) 2008 Pierre d'Herbemont.                            *
 *  This program is free software; you can redistribute and/or modify *
 *  it under the terms of the GNU General Public License as published *
 *  by the Free Software Foundation; version 2 of the license, or (at *
 *  your option) any later version.                                   *
 *                                                                    *
 *  This program is distributed in the hope that it will be useful,   *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of    *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.              *
 *  See the GNU General Public License for more details.              *
 *                                                                    *
 *  You should have received a copy of the GNU General Public License *
 *  along with this program; if not, you can get it from:             *
 *  http://www.gnu.org/copyleft/gpl.html                              *
 **********************************************************************/

#include "test.h"

/* This one is an internal API. We use it here to run tests that
 * don't depends on playback, and only test the event framework */
extern void libvlc_event_send( libvlc_event_manager_t *, libvlc_event_t *);

static void test_events_dummy_callback( const libvlc_event_t * event, void * user_data)
{
    (void)event;
    bool * callback_was_called = user_data;
    *callback_was_called = true;
}

static void test_events_callback_and_detach( const libvlc_event_t * event, void * user_data)
{
    bool * callback_was_called = user_data;
    libvlc_event_manager_t *em;

    em = libvlc_media_player_event_manager (event->p_obj);

    libvlc_event_detach (em, event->type, test_events_callback_and_detach, user_data);
    *callback_was_called = true;
}

static void test_event_type_reception( libvlc_event_manager_t * em, libvlc_event_type_t event_type, bool * callback_was_called )
{
    libvlc_event_t event;
    event.type = event_type;
    *callback_was_called = false;
    libvlc_event_send (em, &event);
    assert (*callback_was_called);
}

#include <string.h>

static void test_events (const char ** argv, int argc)
{
    libvlc_instance_t *vlc;
    libvlc_media_player_t *mi;
    libvlc_event_manager_t *em;
    bool callback_was_called;
    libvlc_event_type_t mi_events[] = {
        libvlc_MediaPlayerNothingSpecial,
        libvlc_MediaPlayerOpening,
        libvlc_MediaPlayerBuffering,
        libvlc_MediaPlayerPlaying,
        libvlc_MediaPlayerPaused,
        libvlc_MediaPlayerStopped,
        libvlc_MediaPlayerEndReached,
        libvlc_MediaPlayerEncounteredError,
        libvlc_MediaPlayerTimeChanged,
        libvlc_MediaPlayerPositionChanged,
    };
    int mi_events_len = sizeof(mi_events)/sizeof(*mi_events);

    log ("Testing events\n");

    vlc = libvlc_new (argc, argv);
    assert (vlc != NULL);

    mi = libvlc_media_player_new (vlc);
    assert (mi != NULL);

    em = libvlc_media_player_event_manager (mi);

    for( int i = 0; i < 0x700; i++ )
         strlen(libvlc_event_type_name( i ));

    log ("+ Testing attaching to Media Instance\n");

    for (int i = 0; i < mi_events_len; i++) {
        int ret;

        ret = libvlc_event_attach (em, mi_events[i],
                                   test_events_dummy_callback,
                                   &callback_was_called);
        assert(ret == 0);
    }

    log ("+ Testing event reception\n");

    for (int i = 0; i < mi_events_len; i++)
        test_event_type_reception (em, mi_events[i], &callback_was_called);

    log ("+ Testing event detaching while in the event callback\n");

    libvlc_event_t event;
    event.type = mi_events[mi_events_len-1];
    callback_was_called = false;

    libvlc_event_detach (em, mi_events[mi_events_len-1], test_events_dummy_callback, &callback_was_called);

    int val = libvlc_event_attach (em, mi_events[mi_events_len-1],
                                   test_events_callback_and_detach,
                                   &callback_was_called);
    assert (val == 0);

    libvlc_event_send (em, &event);
    assert( callback_was_called );

    callback_was_called = false;
    libvlc_event_send (em, &event);
    assert( !callback_was_called );

    log ("+ Testing regular detach()\n");

    for (int i = 0; i < mi_events_len - 1; i++)
        libvlc_event_detach (em, mi_events[i], test_events_dummy_callback, &callback_was_called);

    libvlc_media_player_release (mi);
    libvlc_release (vlc);
}

int main (void)
{
    test_init();

    test_events (test_defaults_args, test_defaults_nargs);

    return 0;
}
