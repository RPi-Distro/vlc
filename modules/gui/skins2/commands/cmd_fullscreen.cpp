/*****************************************************************************
 * cmd_fullscreen.cpp
 *****************************************************************************
 * Copyright (C) 2003-2009 the VideoLAN team
 * $Id: b3bae4fcf0a2ebe9387e40a4720280424c30e137 $
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
#include <vlc_input.h>
#include <vlc_vout.h>


void CmdFullscreen::execute()
{

    if( getIntf()->p_sys->p_input == NULL )
        return;

    vout_thread_t *pVout = input_GetVout( getIntf()->p_sys->p_input );
    if( pVout )
    {
        // Switch to fullscreen
        var_SetBool( pVout, "fullscreen", !var_GetBool( pVout, "fullscreen" ) );
        vlc_object_release( pVout );
    }
}
