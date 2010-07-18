/*****************************************************************************
 * macosx_timer.cpp
 *****************************************************************************
 * Copyright (C) 2003 the VideoLAN team
 * $Id: 7c1d7cd08ee0b171fe80617c5cc908d596df9c2e $
 *
 * Authors: Cyril Deguet     <asmax@via.ecp.fr>
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

#ifdef MACOSX_SKINS

#include "macosx_timer.hpp"
#include "../commands/cmd_generic.hpp"


MacOSXTimer::MacOSXTimer( intf_thread_t *pIntf,  CmdGeneric &rCmd ):
    OSTimer( pIntf ), m_rCommand( rCmd )
{
    // TODO
}


MacOSXTimer::~MacOSXTimer()
{
    // TODO
    stop();
}


void MacOSXTimer::start( int delay, bool oneShot )
{
    // TODO
}


void MacOSXTimer::stop()
{
    // TODO
}

#endif
