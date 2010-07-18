/*****************************************************************************
 * macosx_window.cpp
 *****************************************************************************
 * Copyright (C) 2003 the VideoLAN team
 * $Id: ad88ead968f50c1d028cb8af2a667d47bb54e4b6 $
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

#include "macosx_window.hpp"


MacOSXWindow::MacOSXWindow( intf_thread_t *pIntf, GenericWindow &rWindow,
                            bool dragDrop, bool playOnDrop,
                            MacOSXWindow *pParentWindow ):
    OSWindow( pIntf ), m_pParent( pParentWindow ), m_dragDrop( dragDrop )
{
    // TODO
}


MacOSXWindow::~MacOSXWindow()
{
    // TODO
}


void MacOSXWindow::show( int left, int top ) const
{
    // TODO
}


void MacOSXWindow::hide() const
{
    // TODO
}


void MacOSXWindow::moveResize( int left, int top, int width, int height ) const
{
    // TODO
}


void MacOSXWindow::raise() const
{
    // TODO
}


void MacOSXWindow::setOpacity( uint8_t value ) const
{
    // TODO
}


void MacOSXWindow::toggleOnTop( bool onTop ) const
{
    // TODO
}

#endif
