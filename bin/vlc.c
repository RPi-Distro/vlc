/*****************************************************************************
 * vlc.c: the VLC player
 *****************************************************************************
 * Copyright (C) 1998-2008 the VideoLAN team
 * $Id: 9b580e1e7766d513e5f68658a02f3cfae104b5ea $
 *
 * Authors: Vincent Seguin <seguin@via.ecp.fr>
 *          Samuel Hocevar <sam@zoy.org>
 *          Gildas Bazin <gbazin@videolan.org>
 *          Derk-Jan Hartman <hartman at videolan dot org>
 *          Lots of other people, see the libvlc AUTHORS file
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc/vlc.h>
#include <stdio.h>
#include <stdlib.h>
#include <locale.h>

#ifdef __APPLE__
#include <string.h>
#endif


/* Explicit HACK */
extern void LocaleFree (const char *);
extern char *FromLocale (const char *);
extern void vlc_enable_override (void);

#include <signal.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>
#include <dlfcn.h>

#ifdef HAVE_MAEMO
static void dummy_handler (int signum)
{
    (void) signum;
}
#endif

/*****************************************************************************
 * main: parse command line, start interface and spawn threads.
 *****************************************************************************/
int main( int i_argc, const char *ppsz_argv[] )
{
    /* The so-called POSIX-compliant MacOS X reportedly processes SIGPIPE even
     * if it is blocked in all thread. Also some libraries want SIGPIPE blocked
     * as they have no clue about signal masks.
     * Note: this is NOT an excuse for not protecting against SIGPIPE. If
     * LibVLC runs outside of VLC, we cannot rely on this code snippet. */
    signal (SIGPIPE, SIG_IGN);
    /* Restore default for SIGCHLD in case parent ignores it. */
    signal (SIGCHLD, SIG_DFL);

#ifdef HAVE_SETENV
# ifndef NDEBUG
    /* Activate malloc checking routines to detect heap corruptions. */
    setenv ("MALLOC_CHECK_", "2", 1);

    /* Disable the ugly Gnome crash dialog so that we properly segfault */
    setenv ("GNOME_DISABLE_CRASH_DIALOG", "1", 1);
# endif

    /* Clear the X.Org startup notification ID. Otherwise the UI might try to
     * change the environment while the process is multi-threaded. That could
     * crash. Screw you X.Org. Next time write a thread-safe specification. */
    unsetenv ("DESKTOP_STARTUP_ID");
#endif

#ifndef ALLOW_RUN_AS_ROOT
    if (geteuid () == 0)
    {
        fprintf (stderr, "VLC is not supposed to be run as root. Sorry.\n"
        "If you need to use real-time priorities and/or privileged TCP ports\n"
        "you can use %s-wrapper (make sure it is Set-UID root and\n"
        "cannot be run by non-trusted users first).\n", ppsz_argv[0]);
        return 1;
    }
#endif

    setlocale (LC_ALL, "");

#ifndef __APPLE__
    /* This clutters OSX GUI error logs */
    fprintf( stderr, "VLC media player %s (revision %s)\n",
             libvlc_get_version(), libvlc_get_changeset() );
#endif

    /* Synchronously intercepted POSIX signals.
     *
     * In a threaded program such as VLC, the only sane way to handle signals
     * is to block them in all threads but one - this is the only way to
     * predict which thread will receive them. If any piece of code depends
     * on delivery of one of this signal it is intrinsically not thread-safe
     * and MUST NOT be used in VLC, whether we like it or not.
     * There is only one exception: if the signal is raised with
     * pthread_kill() - we do not use this in LibVLC but some pthread
     * implementations use them internally. You should really use conditions
     * for thread synchronization anyway.
     *
     * Signal that request a clean shutdown, and force an unclean shutdown
     * if they are triggered again 2+ seconds later.
     * We have to handle SIGTERM cleanly because of daemon mode.
     * Note that we set the signals after the vlc_create call. */
    static const int sigs[] = {
        SIGINT, SIGHUP, SIGQUIT, SIGTERM,
    /* Signals that cause a no-op:
     * - SIGPIPE might happen with sockets and would crash VLC. It MUST be
     *   blocked by any LibVLC-dependent application, not just VLC.
     * - SIGCHLD comes after exec*() (such as httpd CGI support) and must
     *   be dequeued to cleanup zombie processes.
     */
        SIGPIPE, SIGCHLD
    };

    sigset_t set;
    sigemptyset (&set);
    for (unsigned i = 0; i < sizeof (sigs) / sizeof (sigs[0]); i++)
        sigaddset (&set, sigs[i]);
#ifdef HAVE_MAEMO
    sigaddset (&set, SIGRTMIN);
    {
        struct sigaction act = { .sa_handler = dummy_handler, };
        sigaction (SIGRTMIN, &act, NULL);
    }
#endif

    /* Block all these signals */
    pthread_sigmask (SIG_BLOCK, &set, NULL);
    sigdelset (&set, SIGPIPE);
    sigdelset (&set, SIGCHLD);

    /* Note that FromLocale() can be used before libvlc is initialized */
    const char *argv[i_argc + 3];
    int argc = 0;

    argv[argc++] = "--no-ignore-config";
#ifdef TOP_BUILDDIR
    argv[argc++] = FromLocale ("--plugin-path="TOP_BUILDDIR"/modules");
#endif
#ifdef TOP_SRCDIR
    argv[argc++] = FromLocale ("--data-path="TOP_SRCDIR"/share");
#endif

    int i = 1;
#ifdef __APPLE__
    /* When VLC.app is run by double clicking in Mac OS X, the 2nd arg
     * is the PSN - process serial number (a unique PID-ish thingie)
     * still ok for real Darwin & when run from command line
     * for example -psn_0_9306113 */
    if(i_argc >= 2 && !strncmp( ppsz_argv[1] , "-psn" , 4 ))
        i = 2;
#endif
    for (; i < i_argc; i++)
        if ((argv[argc++] = FromLocale (ppsz_argv[i])) == NULL)
            return 1; // BOOM!
    argv[argc] = NULL;

    vlc_enable_override ();

    /* Initialize libvlc */
    libvlc_instance_t *vlc = libvlc_new (argc, argv);

    if (vlc != NULL)
    {
        libvlc_set_user_agent (vlc, "VLC media player", NULL);

        if (libvlc_add_intf (vlc, "signals"))
            pthread_sigmask (SIG_UNBLOCK, &set, NULL);
#if !defined (HAVE_MAEMO) && !defined __APPLE__
        libvlc_add_intf (vlc, "globalhotkeys,none");
#endif
        if (libvlc_add_intf (vlc, NULL) == 0)
        {
            libvlc_playlist_play (vlc, -1, 0, NULL);
            libvlc_wait (vlc);
        }
        libvlc_release (vlc);
    }

    for (int i = 1; i < argc; i++)
        LocaleFree (argv[i]);

    /* Do not run exit handlers. Some of them are buggy (e.g. KDE IO scheduler)
     * and crash. Also some will crash because their library may be already
     * unloaded (dlclose()). */
    _exit (0);
}
