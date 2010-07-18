/*****************************************************************************
 * vlc_rand.h: RNG
 *****************************************************************************
 * Copyright © 2007 Rémi Denis-Courmont
 * $Id: 0023d7cdaf6869e0de68a1e6412906d08a21f8f4 $
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

#ifndef VLC_RAND_H
# define VLC_RAND_H

/**
 * \file
 * This file defined random number generator function in vlc
 */

VLC_EXPORT( void, vlc_rand_bytes, (void *buf, size_t len) );

/* Interlocked (but not reproducible) functions for the POSIX PRNG */
VLC_EXPORT( double, vlc_drand48, (void) LIBVLC_USED );
VLC_EXPORT( long, vlc_lrand48, (void) LIBVLC_USED );
VLC_EXPORT( long, vlc_mrand48, (void) LIBVLC_USED );

#endif
