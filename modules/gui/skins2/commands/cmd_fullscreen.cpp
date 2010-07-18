/*****************************************************************************
 * cmd_fullscreen.cpp
 *****************************************************************************
 * Copyright (C) 2003 the VideoLAN team
 * $Id: a7f4f4354ab8110b59d39b6cd4b61e064a758a30 $
 *
 * Authors: Cyril Deguet     <asmax@via.ecp.fr>
 *          Olivier Teulière <ipkiss@via.ecp.fr>
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

#include "cmd_fullscreen.hpp"
#include <vlc/vout.h>


void CmdFullscreen::execute()
{
    vout_thread_t *pVout;

    if( getIntf()->p_sys->p_input == NULL )
    {
        return;
    }

    pVout = (vout_thread_t *)vlc_object_find( getIntf()->p_sys->p_input,
                                            VLC_OBJECT_VOUT, FIND_CHILD );
    if( !pVout )
        pVout = (vout_thread_t *)vlc_object_find( getIntf()->p_sys->p_input,
                                            VLC_OBJECT_VOUT, FIND_ANYWHERE );
    if( pVout )
    {
        // Switch to fullscreen
        pVout->i_changes |= VOUT_FULLSCREEN_CHANGE;
        vlc_object_release( pVout );
    }
}
