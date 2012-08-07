/*****************************************************************************
 * filesystem.c: POSIX file system helpers
 *****************************************************************************
 * Copyright (C) 2005-2006 VLC authors and VideoLAN
 * Copyright © 2005-2008 Rémi Denis-Courmont
 *
 * Authors: Rémi Denis-Courmont <rem # videolan.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
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
#include <vlc_charset.h>
#include <vlc_fs.h>
#include "libvlc.h" /* vlc_mkdir */

#include <assert.h>

#include <stdio.h>
#include <limits.h> /* NAME_MAX */
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>
#include <sys/socket.h>

#ifndef HAVE_LSTAT
# define lstat( a, b ) stat(a, b)
#endif

/**
 * Opens a system file handle.
 *
 * @param filename file path to open (with UTF-8 encoding)
 * @param flags open() flags, see the C library open() documentation
 * @return a file handle on success, -1 on error (see errno).
 * @note Contrary to standard open(), this function returns file handles
 * with the close-on-exec flag enabled.
 */
int vlc_open (const char *filename, int flags, ...)
{
    unsigned int mode = 0;
    va_list ap;

    va_start (ap, flags);
    if (flags & O_CREAT)
        mode = va_arg (ap, unsigned int);
    va_end (ap);

#ifdef O_CLOEXEC
    flags |= O_CLOEXEC;
#endif

    const char *local_name = ToLocale (filename);

    if (local_name == NULL)
    {
        errno = ENOENT;
        return -1;
    }

    int fd = open (local_name, flags, mode);
    if (fd != -1)
        fcntl (fd, F_SETFD, FD_CLOEXEC);

    LocaleFree (local_name);
    return fd;
}

/**
 * Opens a system file handle relative to an existing directory handle.
 *
 * @param dir directory file descriptor
 * @param filename file path to open (with UTF-8 encoding)
 * @param flags open() flags, see the C library open() documentation
 * @return a file handle on success, -1 on error (see errno).
 * @note Contrary to standard open(), this function returns file handles
 * with the close-on-exec flag enabled.
 */
int vlc_openat (int dir, const char *filename, int flags, ...)
{
    unsigned int mode = 0;
    va_list ap;

    va_start (ap, flags);
    if (flags & O_CREAT)
        mode = va_arg (ap, unsigned int);
    va_end (ap);

#ifdef O_CLOEXEC
    flags |= O_CLOEXEC;
#endif

    const char *local_name = ToLocale (filename);
    if (local_name == NULL)
    {
        errno = ENOENT;
        return -1;
    }

#ifdef HAVE_OPENAT
    int fd = openat (dir, local_name, flags, mode);
    if (fd != -1)
        fcntl (fd, F_SETFD, FD_CLOEXEC);
#else
    int fd = -1;
    errno = ENOSYS;
#endif

    LocaleFree (local_name);
    return fd;
}


/**
 * Creates a directory using UTF-8 paths.
 *
 * @param dirname a UTF-8 string with the name of the directory that you
 *        want to create.
 * @param mode directory permissions
 * @return 0 on success, -1 on error (see errno).
 */
int vlc_mkdir (const char *dirname, mode_t mode)
{
    char *locname = ToLocale (dirname);
    if (unlikely(locname == NULL))
    {
        errno = ENOENT;
        return -1;
    }

    int res = mkdir (locname, mode);
    LocaleFree (locname);
    return res;
}

/**
 * Opens a DIR pointer.
 *
 * @param dirname UTF-8 representation of the directory name
 * @return a pointer to the DIR struct, or NULL in case of error.
 * Release with standard closedir().
 */
DIR *vlc_opendir (const char *dirname)
{
    const char *local_name = ToLocale (dirname);
    if (unlikely(local_name == NULL))
    {
        errno = ENOENT;
        return NULL;
    }

    DIR *dir = opendir (local_name);
    LocaleFree (local_name);
    return dir;
}

/**
 * Reads the next file name from an open directory.
 *
 * @param dir The directory that is being read
 *
 * @return a UTF-8 string of the directory entry. Use free() to release it.
 * If there are no more entries in the directory, NULL is returned.
 * If an error occurs, errno is set and NULL is returned.
 */
char *vlc_readdir( DIR *dir )
{
    /* Beware that readdir_r() assumes <buf> is large enough to hold the result
     * dirent including the file name. A buffer overflow could occur otherwise.
     * In particular, pathconf() and _POSIX_NAME_MAX cannot be used here. */
    struct dirent *ent;
    char *path = NULL;

#if !defined(__OS2__) || !defined(__KLIBC__)
    long len = fpathconf (dirfd (dir), _PC_NAME_MAX);
#ifdef NAME_MAX
    /* POSIX says there shall we room for NAME_MAX bytes at all times */
    if (/*len == -1 ||*/ len < NAME_MAX)
        len = NAME_MAX;
#else
    /* OS is broken. Lets assume there is no files left. */
    if (len == -1)
        return NULL;
#endif
    len += offsetof (struct dirent, d_name) + 1;
#else /* __OS2__ && __KLIBC__ */
    /* In the implementation of Innotek LIBC, aka kLIBC on OS/2,
     * fpathconf (_PC_NAME_MAX) is broken, and errno is set to EBADF.
     * Moreover, d_name is not the last member of struct dirent.
     * So just allocate as many as the size of struct dirent. */
    long len = sizeof (struct dirent);
#endif

    struct dirent *buf = malloc (len);
    if (unlikely(buf == NULL))
        return NULL;

    int val = readdir_r (dir, buf, &ent);
    if (val != 0)
        errno = val;
    else if (ent != NULL)
#ifndef __APPLE__
        path = FromLocaleDup (ent->d_name);
#else
        path = FromCharset ("UTF-8-MAC", ent->d_name, strlen (ent->d_name));
#endif
    free (buf);
    return path;
}

static int vlc_statEx (const char *filename, struct stat *buf, bool deref)
{
    const char *local_name = ToLocale (filename);
    if (unlikely(local_name == NULL))
    {
        errno = ENOENT;
        return -1;
    }

    int res = deref ? stat (local_name, buf)
                    : lstat (local_name, buf);
    LocaleFree (local_name);
    return res;
}

/**
 * Finds file/inode information, as stat().
 * Consider using fstat() instead, if possible.
 *
 * @param filename UTF-8 file path
 */
int vlc_stat (const char *filename, struct stat *buf)
{
    return vlc_statEx (filename, buf, true);
}

/**
 * Finds file/inode information, as lstat().
 * Consider using fstat() instead, if possible.
 *
 * @param filename UTF-8 file path
 */
int vlc_lstat (const char *filename, struct stat *buf)
{
    return vlc_statEx (filename, buf, false);
}

/**
 * Removes a file.
 *
 * @param filename a UTF-8 string with the name of the file you want to delete.
 * @return A 0 return value indicates success. A -1 return value indicates an
 *        error, and an error code is stored in errno
 */
int vlc_unlink (const char *filename)
{
    const char *local_name = ToLocale (filename);
    if (unlikely(local_name == NULL))
    {
        errno = ENOENT;
        return -1;
    }

    int ret = unlink (local_name);
    LocaleFree (local_name);
    return ret;
}

/**
 * Moves a file atomically. This only works within a single file system.
 *
 * @param oldpath path to the file before the move
 * @param newpath intended path to the file after the move
 * @return A 0 return value indicates success. A -1 return value indicates an
 *        error, and an error code is stored in errno
 */
int vlc_rename (const char *oldpath, const char *newpath)
{
    const char *lo = ToLocale (oldpath);
    if (lo == NULL)
        goto error;

    const char *ln = ToLocale (newpath);
    if (ln == NULL)
    {
        LocaleFree (lo);
error:
        errno = ENOENT;
        return -1;
    }

    int ret = rename (lo, ln);
    LocaleFree (lo);
    LocaleFree (ln);
    return ret;
}

/**
 * Determines the current working directory.
 *
 * @return the current working directory (must be free()'d)
 *         or NULL on error
 */
char *vlc_getcwd (void)
{
    /* Try $PWD */
    const char *pwd = getenv ("PWD");
    if (pwd != NULL)
    {
        struct stat s1, s2;
        /* Make sure $PWD is correct */
        if (stat (pwd, &s1) == 0 && stat (".", &s2) == 0
         && s1.st_dev == s2.st_dev && s1.st_ino == s2.st_ino)
            return ToLocaleDup (pwd);
    }

    /* Otherwise iterate getcwd() until the buffer is big enough */
    long path_max = pathconf (".", _PC_PATH_MAX);
    size_t size = (path_max == -1 || path_max > 4096) ? 4096 : path_max;

    for (;; size *= 2)
    {
        char *buf = malloc (size);
        if (unlikely(buf == NULL))
            break;

        if (getcwd (buf, size) != NULL)
#ifdef ASSUME_UTF8
            return buf;
#else
        {
            char *ret = ToLocaleDup (buf);
            free (buf);
            return ret; /* success */
        }
#endif
        free (buf);

        if (errno != ERANGE)
            break;
    }
    return NULL;
}

/**
 * Duplicates a file descriptor. The new file descriptor has the close-on-exec
 * descriptor flag set.
 * @return a new file descriptor or -1
 */
int vlc_dup (int oldfd)
{
    int newfd;

#ifdef F_DUPFD_CLOEXEC
    newfd = fcntl (oldfd, F_DUPFD_CLOEXEC);
    if (unlikely(newfd == -1 && errno == EINVAL))
#endif
    {
        newfd = dup (oldfd);
        if (likely(newfd != -1))
            fcntl (newfd, F_SETFD, FD_CLOEXEC);
    }
    return newfd;
}

#ifdef __ANDROID__ /* && we support android < 2.3 */
/* pipe2() is declared and available since android-9 NDK,
 * although it is available in libc.a since android-3
 * We redefine the function here in order to be able to run
 * on versions of Android older than 2.3
 */
#include <sys/syscall.h>
//#include <sys/linux-syscalls.h> // fucking brokeness
int pipe2(int fds[2], int flags)
{
    return syscall(/*__NR_pipe2 */ 359, fds, flags);
}
#endif /* __ANDROID__ */

/**
 * Creates a pipe (see "man pipe" for further reference).
 */
int vlc_pipe (int fds[2])
{
#ifdef HAVE_PIPE2
    if (pipe2 (fds, O_CLOEXEC) == 0)
        return 0;
    if (errno != ENOSYS)
        return -1;
#endif

    if (pipe (fds))
        return -1;

    fcntl (fds[0], F_SETFD, FD_CLOEXEC);
    fcntl (fds[1], F_SETFD, FD_CLOEXEC);
    return 0;
}

#include <vlc_network.h>

/**
 * Creates a socket file descriptor. The new file descriptor has the
 * close-on-exec flag set.
 * @param pf protocol family
 * @param type socket type
 * @param proto network protocol
 * @param nonblock true to create a non-blocking socket
 * @return a new file descriptor or -1
 */
int vlc_socket (int pf, int type, int proto, bool nonblock)
{
    int fd;

#ifdef SOCK_CLOEXEC
    type |= SOCK_CLOEXEC;
    if (nonblock)
        type |= SOCK_NONBLOCK;
    fd = socket (pf, type, proto);
    if (fd != -1 || errno != EINVAL)
        return fd;

    type &= ~(SOCK_CLOEXEC|SOCK_NONBLOCK);
#endif

    fd = socket (pf, type, proto);
    if (fd == -1)
        return -1;

    fcntl (fd, F_SETFD, FD_CLOEXEC);
    if (nonblock)
        fcntl (fd, F_SETFL, fcntl (fd, F_GETFL, 0) | O_NONBLOCK);
    return fd;
}

/**
 * Accepts an inbound connection request on a listening socket.
 * The new file descriptor has the close-on-exec flag set.
 * @param lfd listening socket file descriptor
 * @param addr pointer to the peer address or NULL [OUT]
 * @param alen pointer to the length of the peer address or NULL [OUT]
 * @param nonblock whether to put the new socket in non-blocking mode
 * @return a new file descriptor, or -1 on error.
 */
int vlc_accept (int lfd, struct sockaddr *addr, socklen_t *alen, bool nonblock)
{
#ifdef HAVE_ACCEPT4
    int flags = SOCK_CLOEXEC;
    if (nonblock)
        flags |= SOCK_NONBLOCK;

    do
    {
        int fd = accept4 (lfd, addr, alen, flags);
        if (fd != -1)
            return fd;
    }
    while (errno == EINTR);

    if (errno != ENOSYS)
        return -1;
#endif

    do
    {
        int fd = accept (lfd, addr, alen);
        if (fd != -1)
        {
            fcntl (fd, F_SETFD, FD_CLOEXEC);
            if (nonblock)
                fcntl (fd, F_SETFL, fcntl (fd, F_GETFL, 0) | O_NONBLOCK);
            return fd;
        }
    }
    while (errno == EINTR);

    return -1;
}
