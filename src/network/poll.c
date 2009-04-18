/*****************************************************************************
 * poll.c: I/O event multiplexing
 *****************************************************************************
 * Copyright © 2007 Rémi Denis-Courmont
 * $Id: f6f9050effd9df1f7c1ffd4b4f2d0b293295d2de $
 *
 * Author: Rémi Denis-Courmont
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

#include <vlc_common.h>
#include <vlc_network.h>

#ifdef HAVE_POLL
struct pollfd;

int vlc_poll (struct pollfd *fds, unsigned nfds, int timeout)
{
    (void)fds; (void)nfds; (void)timeout;
    abort ();
}
#else /* !HAVE_POLL */
#include <string.h>
#include <stdlib.h>
#include <vlc_network.h>

int vlc_poll (struct pollfd *fds, unsigned nfds, int timeout)
{
    fd_set rdset, wrset, exset;
    struct timeval tv = { 0, 0 };
    int val = -1;

    FD_ZERO (&rdset);
    FD_ZERO (&wrset);
    FD_ZERO (&exset);
    for (unsigned i = 0; i < nfds; i++)
    {
        int fd = fds[i].fd;
        if (val < fd)
            val = fd;

        /* With POSIX, FD_SET & FD_ISSET are not defined if fd is negative or
	 * bigger or equal than FD_SETSIZE. That is one of the reasons why VLC
	 * uses poll() rather than select(). Most POSIX systems implement
	 * fd_set has a bit field with no sanity checks. This is especially bad
	 * on systems (such as BSD) that have no process open files limit by
	 * default, such that it is quite feasible to get fd >= FD_SETSIZE.
	 * The next instructions will result in a buffer overflow if run on
	 * a POSIX system, and the later FD_ISSET will do undefined memory
	 * access.
	 *
	 * With Winsock, fd_set is a table of integers. This is awfully slow.
	 * However, FD_SET and FD_ISSET silently and safely discard
	 * overflows. If it happens we will loose socket events. Note that
	 * most (if not all) Winsock SOCKET handles are actually bigger than
	 * FD_SETSIZE in terms of absolute value - they are not POSIX file
	 * descriptors. From Vista, there is a much nicer WSAPoll(), but Mingw
	 * is yet to support it.
	 *
	 * With BeOS, the situation is unknown (FIXME: document).
	 */
        if (fds[i].events & POLLIN)
            FD_SET (fd, &rdset);
        if (fds[i].events & POLLOUT)
            FD_SET (fd, &wrset);
        if (fds[i].events & POLLPRI)
            FD_SET (fd, &exset);
    }

    if (timeout >= 0)
    {
        div_t d = div (timeout, 1000);
        tv.tv_sec = d.quot;
        tv.tv_usec = d.rem * 1000;
    }

    val = select (val + 1, &rdset, &wrset, &exset,
                  (timeout >= 0) ? &tv : NULL);
    if (val == -1)
        return -1;

    for (unsigned i = 0; i < nfds; i++)
    {
        int fd = fds[i].fd;
        fds[i].revents = (FD_ISSET (fd, &rdset) ? POLLIN : 0)
                       | (FD_ISSET (fd, &wrset) ? POLLOUT : 0)
                       | (FD_ISSET (fd, &exset) ? POLLPRI : 0);
    }
    return val;
}
#endif /* !HAVE_POLL */
