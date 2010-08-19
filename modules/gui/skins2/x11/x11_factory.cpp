/*****************************************************************************
 * x11_factory.cpp
 *****************************************************************************
 * Copyright (C) 2003 the VideoLAN team
 * $Id: abd2e5583ce31da066f6e4b05230747ac5a23dd4 $
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

#ifdef X11_SKINS

#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <X11/Xlib.h>
#include <limits.h>

#include "x11_factory.hpp"
#include "x11_display.hpp"
#include "x11_graphics.hpp"
#include "x11_loop.hpp"
#include "x11_popup.hpp"
#include "x11_timer.hpp"
#include "x11_window.hpp"
#include "x11_tooltip.hpp"

#include "../src/generic_window.hpp"

#include <vlc_common.h>
#include <vlc_xlib.h>

X11Factory::X11Factory( intf_thread_t *pIntf ): OSFactory( pIntf ),
    m_pDisplay( NULL ), m_pTimerLoop( NULL ), m_dirSep( "/" )
{
    // see init()
}


X11Factory::~X11Factory()
{
    delete m_pTimerLoop;
    delete m_pDisplay;
}


bool X11Factory::init()
{
    // make sure xlib is safe-thread
    if( !vlc_xlib_init( VLC_OBJECT(getIntf()) ) )
    {
        msg_Err( getIntf(), "initializing xlib for multi-threading failed" );
        return false;
    }

    // Create the X11 display
    m_pDisplay = new X11Display( getIntf() );

    // Get the display
    Display *pDisplay = m_pDisplay->getDisplay();
    if( pDisplay == NULL )
    {
        // Initialization failed
        return false;
    }

    // Create the timer loop
    m_pTimerLoop = new X11TimerLoop( getIntf(),
                                     ConnectionNumber( pDisplay ) );

    // Initialize the resource path
    char *datadir = config_GetUserDir( VLC_DATA_DIR );
    m_resourcePath.push_back( (string)datadir + "/skins2" );
    free( datadir );
    m_resourcePath.push_back( (string)"share/skins2" );
    datadir = config_GetDataDir( getIntf() );
    m_resourcePath.push_back( (string)datadir + "/skins2" );
    free( datadir );

    return true;
}


OSGraphics *X11Factory::createOSGraphics( int width, int height )
{
    return new X11Graphics( getIntf(), *m_pDisplay, width, height );
}


OSLoop *X11Factory::getOSLoop()
{
    return X11Loop::instance( getIntf(), *m_pDisplay );
}


void X11Factory::destroyOSLoop()
{
    X11Loop::destroy( getIntf() );
}

void X11Factory::minimize()
{
    XIconifyWindow( m_pDisplay->getDisplay(), m_pDisplay->getMainWindow(),
                    DefaultScreen( m_pDisplay->getDisplay() ) );
}

void X11Factory::restore()
{
    // TODO
}

void X11Factory::addInTray()
{
    // TODO
}

void X11Factory::removeFromTray()
{
    // TODO
}

void X11Factory::addInTaskBar()
{
    // TODO
}

void X11Factory::removeFromTaskBar()
{
    // TODO
}

OSTimer *X11Factory::createOSTimer( CmdGeneric &rCmd )
{
    return new X11Timer( getIntf(), rCmd );
}


OSWindow *X11Factory::createOSWindow( GenericWindow &rWindow, bool dragDrop,
                                      bool playOnDrop, OSWindow *pParent,
                                      GenericWindow::WindowType_t type )
{
    return new X11Window( getIntf(), rWindow, *m_pDisplay, dragDrop,
                          playOnDrop, (X11Window*)pParent, type );
}


OSTooltip *X11Factory::createOSTooltip()
{
    return new X11Tooltip( getIntf(), *m_pDisplay );
}


OSPopup *X11Factory::createOSPopup()
{
    return new X11Popup( getIntf(), *m_pDisplay );
}


int X11Factory::getScreenWidth() const
{
    Display *pDisplay = m_pDisplay->getDisplay();
    int screen = DefaultScreen( pDisplay );
    return DisplayWidth( pDisplay, screen );
}


int X11Factory::getScreenHeight() const
{
    Display *pDisplay = m_pDisplay->getDisplay();
    int screen = DefaultScreen( pDisplay );
    return DisplayHeight( pDisplay, screen );
}


SkinsRect X11Factory::getWorkArea() const
{
    // XXX
    return SkinsRect( 0, 0, getScreenWidth(), getScreenHeight() );
}


void X11Factory::getMousePos( int &rXPos, int &rYPos ) const
{
    Window rootReturn, childReturn;
    int winx, winy;
    unsigned int xmask;

    Display *pDisplay = m_pDisplay->getDisplay();
    Window root = DefaultRootWindow( pDisplay );
    XQueryPointer( pDisplay, root, &rootReturn, &childReturn,
                   &rXPos, &rYPos, &winx, &winy, &xmask );
}


void X11Factory::rmDir( const string &rPath )
{
    struct
    {
        struct dirent ent;
        char buf[NAME_MAX + 1];
    } buf;
    struct dirent *file;
    DIR *dir;

    dir = opendir( rPath.c_str() );
    if( !dir ) return;

    // Parse the directory and remove everything it contains
    while( readdir_r( dir, &buf.ent, &file ) == 0 && file != NULL )
    {
        struct stat statbuf;
        string filename = file->d_name;

        // Skip "." and ".."
        if( filename == "." || filename == ".." )
        {
            continue;
        }

        filename = rPath + "/" + filename;

        if( !stat( filename.c_str(), &statbuf ) && S_ISDIR(statbuf.st_mode) )
        {
            rmDir( filename );
        }
        else
        {
            unlink( filename.c_str() );
        }
    }

    // Close the directory
    closedir( dir );

    // And delete it
    rmdir( rPath.c_str() );
}


#endif
