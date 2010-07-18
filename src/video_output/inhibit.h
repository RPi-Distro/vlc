/*****************************************************************************
 * inhibit.h: screen saver inhibition
 *****************************************************************************
 * Copyright (C) 2009 Rémi Denis-Courmont
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef LIBVLC_INHIBIT_H
# define LIVCLC_INHIBIT_H 1

# include <vlc_inhibit.h>

vlc_inhibit_t *vlc_inhibit_Create (vlc_object_t *, int_fast32_t);
void vlc_inhibit_Destroy (vlc_inhibit_t *);

static inline void vlc_inhibit_Set (vlc_inhibit_t *ih, bool suspend)
{
    ih->inhibit (ih, suspend);
}
#endif
