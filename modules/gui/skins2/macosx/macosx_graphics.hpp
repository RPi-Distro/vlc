/*****************************************************************************
 * macosx_graphics.hpp
 *****************************************************************************
 * Copyright (C) 2003 the VideoLAN team
 * $Id: 05ae82340e96145019816e7b33f7c304ddd1a2db $
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef MACOSX_GRAPHICS_HPP
#define MACOSX_GRAPHICS_HPP

#include "../src/os_graphics.hpp"
#include <Carbon/Carbon.h>

class GenericWindow;
class GenericBitmap;

/// MacOSX implementation of OSGraphics
class MacOSXGraphics: public OSGraphics
{
public:
    MacOSXGraphics( intf_thread_t *pIntf, int width, int height);

    virtual ~MacOSXGraphics();

    /// Clear the graphics
    virtual void clear();

    /// Draw another graphics on this one
    virtual void drawGraphics( const OSGraphics &rGraphics, int xSrc = 0,
                               int ySrc = 0, int xDest = 0, int yDest = 0,
                               int width = -1, int height = -1 );

    /// Render a bitmap on this graphics
    virtual void drawBitmap( const GenericBitmap &rBitmap, int xSrc = 0,
                             int ySrc = 0, int xDest = 0, int yDest = 0,
                             int width = -1, int height = -1,
                             bool blend = false );

    /// Draw a filled rectangle on the grahics (color is #RRGGBB)
    virtual void fillRect( int left, int top, int width, int height,
                           uint32_t color );

    /// Draw an empty rectangle on the grahics (color is #RRGGBB)
    virtual void drawRect( int left, int top, int width, int height,
                           uint32_t color );

    /// Set the shape of a window with the mask of this graphics.
    virtual void applyMaskToWindow( OSWindow &rWindow );

    /// Copy the graphics on a window
    virtual void copyToWindow( OSWindow &rWindow, int xSrc,
                               int ySrc, int width, int height,
                               int xDest, int yDest );

    /// Tell whether the pixel at the given position is visible
    virtual bool hit( int x, int y ) const;

    /// Getters
    virtual int getWidth() const { return m_width; }
    virtual int getHeight() const { return m_height; }

private:
    /// Size of the image
    int m_width, m_height;
};


#endif
