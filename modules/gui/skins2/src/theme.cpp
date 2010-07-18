/*****************************************************************************
 * theme.cpp
 *****************************************************************************
 * Copyright (C) 2003 the VideoLAN team
 * $Id: b7df897b7d71e9af10b2d1f3c6a0f5ccc6885b4d $
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#include "theme.hpp"
#include "top_window.hpp"
#include <sstream>


Theme::~Theme()
{
    // Be sure things are destroyed in the right order (XXX check)
    m_layouts.clear();
    m_controls.clear();
    m_windows.clear();
    m_bitmaps.clear();
    m_fonts.clear();
    m_commands.clear();
    m_vars.clear();
    m_curves.clear();
}


void Theme::loadConfig()
{
    msg_Dbg( getIntf(), "loading theme configuration");

    // Get config from vlcrc file
    char *save = config_GetPsz( getIntf(), "skins2-config" );
    if( !save ) return;

    // Is there an existing config?
    if( !strcmp( save, "" ) )
    {
        // Show the windows as indicated by the XML file
        m_windowManager.showAll( true );
        free( save );
        return;
    }

    istringstream inStream(save);
    free( save );

    char sep;
    string winId, layId;
    int x, y, width, height, visible;
    bool somethingVisible = false;
    while( !inStream.eof() )
    {
        inStream >> sep;
        if( sep != '[' ) goto invalid;
        inStream >> winId >> layId >> x >> y >> width >> height >> visible >> sep >> ws;
        if( sep != ']' ) goto invalid;

        // Try to find the window and the layout
        map<string, TopWindowPtr>::const_iterator itWin;
        map<string, GenericLayoutPtr>::const_iterator itLay;
        itWin = m_windows.find( winId );
        itLay = m_layouts.find( layId );
        if( itWin == m_windows.end() || itLay == m_layouts.end() )
        {
            goto invalid;
        }
        TopWindow *pWin = itWin->second.get();
        GenericLayout *pLayout = itLay->second.get();

        // Restore the layout
        m_windowManager.setActiveLayout( *pWin, *pLayout );
        if( pLayout->getWidth() != width ||
            pLayout->getHeight() != height )
        {
            m_windowManager.startResize( *pLayout, WindowManager::kResizeSE );
            m_windowManager.resize( *pLayout, width, height );
            m_windowManager.stopResize();
        }
        // Move the window (which incidentally takes care of the anchoring)
        m_windowManager.startMove( *pWin );
        m_windowManager.move( *pWin, x, y );
        m_windowManager.stopMove();
        if( visible )
        {
            somethingVisible = true;
            m_windowManager.show( *pWin );
        }
    }

    if( !somethingVisible )
    {
        goto invalid;
    }
    return;

invalid:
    msg_Warn( getIntf(), "invalid config: %s", inStream.str().c_str() );
    // Restore the visibility defined in the theme
    m_windowManager.showAll( true );
}


void Theme::saveConfig()
{
    msg_Dbg( getIntf(), "saving theme configuration");

    map<string, TopWindowPtr>::const_iterator itWin;
    map<string, GenericLayoutPtr>::const_iterator itLay;
    ostringstream outStream;
    for( itWin = m_windows.begin(); itWin != m_windows.end(); itWin++ )
    {
        TopWindow *pWin = itWin->second.get();

        // Find the layout id for this window
        string layoutId;
        const GenericLayout *pLayout = &pWin->getActiveLayout();
        for( itLay = m_layouts.begin(); itLay != m_layouts.end(); itLay++ )
        {
            if( itLay->second.get() == pLayout )
            {
                layoutId = itLay->first;
            }
        }

        outStream << '[' << itWin->first << ' ' << layoutId << ' '
            << pWin->getLeft() << ' ' << pWin->getTop() << ' '
            << pLayout->getWidth() << ' ' << pLayout->getHeight() << ' '
            << (pWin->getVisibleVar().get() ? 1 : 0) << ']';
    }

    // Save config to file
    config_PutPsz( getIntf(), "skins2-config", outStream.str().c_str() );
}


// Takes an ID of the form "id1;id2;id3", and returns the object
// corresponding to the first valid ID. If no ID is valid, it returns NULL.
// XXX The string handling here probably could be improved.
template<class T> typename T::pointer
Theme::IDmap<T>::find_first_object( const string &id ) const
{
    string rightPart = id;
    string::size_type pos;
    do
    {
        pos = rightPart.find( ";" );
        string leftPart = rightPart.substr( 0, pos );

        typename T::pointer p = find_object( leftPart );
        if( p ) return p;

        if( pos != string::npos )
        {
            rightPart = rightPart.substr( pos, rightPart.size() );
            rightPart =
                rightPart.substr( rightPart.find_first_not_of( " \t;" ),
                                  rightPart.size() );
        }
    }
    while( pos != string::npos );
    return NULL;
}

GenericBitmap *Theme::getBitmapById( const string &id ) const
{
    return m_bitmaps.find_first_object( id );
}

GenericFont *Theme::getFontById( const string &id ) const
{
    return m_fonts.find_first_object( id );
}

