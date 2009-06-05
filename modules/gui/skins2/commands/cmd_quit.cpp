/*****************************************************************************
 * cmd_quit.cpp
 *****************************************************************************
 * Copyright (C) 2003 the VideoLAN team
 * $Id$
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

#ifdef HAVE_CONFIG_H
#   include "config.h"
#endif

#include <vlc_osd.h>
#include <vlc_playlist.h>

#include "cmd_quit.hpp"
#include "../src/os_factory.hpp"
#include "../src/os_loop.hpp"


void CmdQuit::execute()
{
    // Stop the playlist
    vout_OSDMessage( getIntf(), DEFAULT_CHAN, _( "Quit" ) );

    // Get the instance of OSFactory
    OSFactory *pOsFactory = OSFactory::instance( getIntf() );

    // Exit the main OS loop
    pOsFactory->getOSLoop()->exit();

    // Kill libvlc
    libvlc_Quit( getIntf()->p_libvlc );
}
