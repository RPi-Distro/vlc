/*****************************************************************************
 * rand.c : non-predictible random bytes generator
 *****************************************************************************
 * Copyright © 2007 Rémi Denis-Courmont
 * $Id: 28546271b3d13f6f95b52b0a0653f8a1b04210a3 $
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
#include <vlc_rand.h>

#ifndef WIN32
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <vlc_fs.h>

#include <vlc_md5.h>

/*
 * Pseudo-random number generator using a HMAC-MD5 in counter mode.
 * Probably not very secure (expert patches welcome) but definitely
 * better than rand() which is defined to be reproducible...
 */
#define BLOCK_SIZE 64

static uint8_t okey[BLOCK_SIZE], ikey[BLOCK_SIZE];

static void vlc_rand_init (void)
{
#if defined (__OpenBSD__) || defined (__OpenBSD_kernel__)
    static const char randfile[] = "/dev/random";
#else
    static const char randfile[] = "/dev/urandom";
#endif
    uint8_t key[BLOCK_SIZE];

    /* Get non-predictible value as key for HMAC */
    int fd = vlc_open (randfile, O_RDONLY);
    if (fd == -1)
        return; /* Uho! */

    for (size_t i = 0; i < sizeof (key);)
    {
         ssize_t val = read (fd, key + i, sizeof (key) - i);
         if (val > 0)
             i += val;
    }

    /* Precompute outer and inner keys for HMAC */
    for (size_t i = 0; i < sizeof (key); i++)
    {
        okey[i] = key[i] ^ 0x5c;
        ikey[i] = key[i] ^ 0x36;
    }

    close (fd);
}


void vlc_rand_bytes (void *buf, size_t len)
{
    static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
    static uint64_t counter = 0;

    uint64_t stamp = NTPtime64 ();

    while (len > 0)
    {
        uint64_t val;
        struct md5_s mdi, mdo;

        InitMD5 (&mdi);
        InitMD5 (&mdo);

        pthread_mutex_lock (&lock);
        if (counter == 0)
            vlc_rand_init ();
        val = counter++;

        AddMD5 (&mdi, ikey, sizeof (ikey));
        AddMD5 (&mdo, okey, sizeof (okey));
        pthread_mutex_unlock (&lock);

        AddMD5 (&mdi, &stamp, sizeof (stamp));
        AddMD5 (&mdi, &val, sizeof (val));
        EndMD5 (&mdi);
        AddMD5 (&mdo, mdi.p_digest, sizeof (mdi.p_digest));
        EndMD5 (&mdo);

        if (len < sizeof (mdo.p_digest))
        {
            memcpy (buf, mdo.p_digest, len);
            break;
        }

        memcpy (buf, mdo.p_digest, sizeof (mdo.p_digest));
        len -= sizeof (mdo.p_digest);
        buf = ((uint8_t *)buf) + sizeof (mdo.p_digest);
    }
}

#else /* WIN32 */

#include <wincrypt.h>

void vlc_rand_bytes (void *buf, size_t len)
{
    HCRYPTPROV hProv;
    size_t count = len;
    uint8_t *p_buf = (uint8_t *)buf;

    /* fill buffer with pseudo-random data */
    while (count > 0)
    {
        unsigned int val;
        val = rand();
        if (count < sizeof (val))
        {
            memcpy (p_buf, &val, count);
            break;
        }
 
        memcpy (p_buf, &val, sizeof (val));
        count -= sizeof (val);
        p_buf += sizeof (val);
    }

    /* acquire default encryption context */
    if( CryptAcquireContext(
        &hProv,            // Variable to hold returned handle.
        NULL,              // Use default key container.
        MS_DEF_PROV,       // Use default CSP.
        PROV_RSA_FULL,     // Type of provider to acquire.
        0) )
    {
        /* fill buffer with pseudo-random data, intial buffer content
           is used as auxillary random seed */
        CryptGenRandom(hProv, len, buf);
        CryptReleaseContext(hProv, 0);
    }
}
#endif

static struct
{
    bool           init;
    unsigned short subi[3];
    vlc_mutex_t    lock;
} rand48 = { false, { 0, 0, 0, }, VLC_STATIC_MUTEX, };

static void init_rand48 (void)
{
    if (!rand48.init)
    {
        vlc_rand_bytes (rand48.subi, sizeof (rand48.subi));
#if 0 // short would be more than 16-bits ?
        for (unsigned i = 0; i < 3; i++)
            subi[i] &= 0xffff;
#endif
    }
}

/**
 * PRNG uniformly distributed between 0.0 and 1.0 with 48-bits precision.
 *
 * @note Contrary to POSIX drand48(), this function is thread-safe.
 * @warning Series generated by this function are not reproducible.
 * Use erand48() if you need reproducible series.
 *
 * @return a double value within [0.0, 1.0] inclusive
 */
double vlc_drand48 (void)
{
    double ret;

    vlc_mutex_lock (&rand48.lock);
    init_rand48 ();
    ret = erand48 (rand48.subi);
    vlc_mutex_unlock (&rand48.lock);
    return ret;
}

/**
 * PRNG uniformly distributed between 0 and 2^32 - 1.
 *
 * @note Contrary to POSIX lrand48(), this function is thread-safe.
 * @warning Series generated by this function are not reproducible.
 * Use nrand48() if you need reproducible series.
 *
 * @return a double value within [0.0, 1.0] inclusive
 */
long vlc_lrand48 (void)
{
    long ret;

    vlc_mutex_lock (&rand48.lock);
    init_rand48 ();
    ret = nrand48 (rand48.subi);
    vlc_mutex_unlock (&rand48.lock);
    return ret;
}

/**
 * PRNG uniformly distributed between -2^32 and 2^32 - 1.
 *
 * @note Contrary to POSIX mrand48(), this function is thread-safe.
 * @warning Series generated by this function are not reproducible.
 * Use jrand48() if you need reproducible series.
 *
 * @return a double value within [0.0, 1.0] inclusive
 */
long vlc_mrand48 (void)
{
    long ret;

    vlc_mutex_lock (&rand48.lock);
    init_rand48 ();
    ret = jrand48 (rand48.subi);
    vlc_mutex_unlock (&rand48.lock);
    return ret;
}
