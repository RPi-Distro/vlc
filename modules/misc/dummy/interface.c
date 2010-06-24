/*****************************************************************************
 * intf_dummy.c: dummy interface plugin
 *****************************************************************************
 * Copyright (C) 2000, 2001 the VideoLAN team
 * $Id: 11e46889b9ccf877a65990d4a121d70f5c092f8e $
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
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

#include <vlc_common.h>
#include <vlc_interface.h>

#include "dummy.h"

/*****************************************************************************
 * Open: initialize dummy interface
 *****************************************************************************/
int  OpenIntf ( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t*) p_this;

#ifdef WIN32
    bool b_quiet;
    b_quiet = var_InheritBool( p_intf, "dummy-quiet" );
    if( !b_quiet )
        CONSOLE_INTRO_MSG;
#endif

    msg_Info( p_intf, "using the dummy interface module..." );

    p_intf->pf_run = NULL;

    return VLC_SUCCESS;
}
