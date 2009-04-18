/*****************************************************************************
 * win32_window.hpp
 *****************************************************************************
 * Copyright (C) 2003 the VideoLAN team
 * $Id: 22e1cbe4e63dfbc8299b7b23b13d947559649525 $
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

#ifndef WIN32_WINDOW_HPP
#define WIN32_WINDOW_HPP

#include "../src/os_window.hpp"
#include <windows.h>
#include <ole2.h>   // LPDROPTARGET


/// Win32 implementation of OSWindow
class Win32Window: public OSWindow
{
    public:
        Win32Window( intf_thread_t *pIntf, GenericWindow &rWindow,
                     HINSTANCE hInst, HWND hParentWindow,
                     bool dragDrop, bool playOnDrop,
                     Win32Window *pParentWindow );
        virtual ~Win32Window();

        // Show the window
        virtual void show( int left, int top ) const;

        // Hide the window
        virtual void hide() const;

        /// Move and resize the window
        virtual void moveResize( int left, int top,
                                 int width, int height ) const;

        /// Bring the window on top
        virtual void raise() const;

        /// Set the opacity of the window (0 = transparent, 255 = opaque)
        virtual void setOpacity( uint8_t value ) const;

        /// Toggle the window on top
        virtual void toggleOnTop( bool onTop ) const;

        /// Getter for the window handle
        HWND getHandle() const { return m_hWnd; }

    private:
        /// Window handle
        HWND m_hWnd;
        /// Indicates whether the window handles drag&drop events
        bool m_dragDrop;
        /// Drop target
        LPDROPTARGET m_pDropTarget;
        /// Indicates whether the window is layered
        mutable bool m_isLayered;
        /// Parent window
        Win32Window *m_pParent;
};


#endif
