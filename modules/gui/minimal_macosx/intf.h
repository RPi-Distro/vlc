/*****************************************************************************
 * intf.h: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2002-2007 the VideoLAN team
 * $Id: 93ab46c657e18763ef1b7bd92b471afe64dde5a1 $
 *
 * Authors: Jon Lech Johansen <jon-vl@nanocrew.net>
 *          Christophe Massiot <massiot@via.ecp.fr>
 *          Derk-Jan Hartman <hartman at videolan dot org>
 *          Felix Kühne <fkuehne at videolan dot org>
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
#   include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_interface.h>
#include <vlc_playlist.h>
#include <vlc_vout.h>
#include <vlc_aout.h>
#include <vlc_input.h>

#include <Cocoa/Cocoa.h>

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/

/*****************************************************************************
 * intf_sys_t: description and status of the interface
 *****************************************************************************/
struct intf_sys_t
{
    int nothing_for_now;

    vlc_mutex_t lock;
    vlc_cond_t wait;
};

