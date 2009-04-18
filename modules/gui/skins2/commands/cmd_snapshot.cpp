/*****************************************************************************
 * cmd_snapshot.cpp
 *****************************************************************************
 * Copyright (C) 2006 the VideoLAN team
 * $Id: 23e5a37a306271d60b2d92044625af0ab764e5f9 $
 *
 * Authors: Olivier Teulière <ipkiss@via.ecp.fr>
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

#include "cmd_snapshot.hpp"
#include <vlc_input.h>
#include <vlc_vout.h>

void CmdSnapshot::execute()
{
    vout_thread_t *pVout;

    if( getIntf()->p_sys->p_input == NULL )
    {
        return;
    }

    pVout = (vout_thread_t *)vlc_object_find( getIntf()->p_sys->p_input,
                                              VLC_OBJECT_VOUT, FIND_CHILD );
    if( pVout )
    {
        // Take a snapshot
        vout_Control( pVout, VOUT_SNAPSHOT );
        vlc_object_release( pVout );
    }
}

