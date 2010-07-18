/*****************************************************************************
 * ctrl_video.cpp
 *****************************************************************************
 * Copyright (C) 2004 the VideoLAN team
 * $Id: 823cac46894955bf62fc484cef9de64c9bad1d90 $
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

#include "ctrl_video.hpp"
#include "../src/theme.hpp"
#include "../src/vout_window.hpp"
#include "../src/os_graphics.hpp"
#include "../src/vlcproc.hpp"
#include "../src/vout_manager.hpp"
#include "../src/window_manager.hpp"
#include "../commands/async_queue.hpp"
#include "../commands/cmd_resize.hpp"


CtrlVideo::CtrlVideo( intf_thread_t *pIntf, GenericLayout &rLayout,
                      bool autoResize, const UString &rHelp,
                      VarBool *pVisible ):
    CtrlGeneric( pIntf, rHelp, pVisible ), m_rLayout( rLayout ),
    m_xShift( 0 ), m_yShift( 0 ), m_bAutoResize( autoResize ),
    m_pVoutWindow( NULL ), m_bIsUseable( false )
{
    VarBool &rFullscreen = VlcProc::instance( getIntf() )->getFullscreenVar();
    rFullscreen.addObserver( this );

    // if global parameter set to no resize, override skins behavior
    if( !var_InheritBool( pIntf, "qt-video-autoresize" ) )
        m_bAutoResize = false;
}


CtrlVideo::~CtrlVideo()
{
    VarBool &rFullscreen = VlcProc::instance( getIntf() )->getFullscreenVar();
    rFullscreen.delObserver( this );
}


void CtrlVideo::handleEvent( EvtGeneric &rEvent )
{
}


bool CtrlVideo::mouseOver( int x, int y ) const
{
    return false;
}


void CtrlVideo::onResize()
{
    const Position *pPos = getPosition();
    if( pPos && m_pVoutWindow )
    {
        m_pVoutWindow->move( pPos->getLeft(), pPos->getTop() );
        m_pVoutWindow->resize( pPos->getWidth(), pPos->getHeight() );
    }
}


void CtrlVideo::onPositionChange()
{
    // Compute the difference between layout size and video size
    m_xShift = m_rLayout.getWidth() - getPosition()->getWidth();
    m_yShift = m_rLayout.getHeight() - getPosition()->getHeight();
}


void CtrlVideo::draw( OSGraphics &rImage, int xDest, int yDest )
{
    const Position *pPos = getPosition();
    if( pPos )
    {
        // Draw a black rectangle under the video to avoid transparency
        rImage.fillRect( pPos->getLeft(), pPos->getTop(), pPos->getWidth(),
                         pPos->getHeight(), 0 );
    }
}


void CtrlVideo::setLayout( GenericLayout *pLayout,
                           const Position &rPosition )
{
    CtrlGeneric::setLayout( pLayout, rPosition );
    m_pLayout->getActiveVar().addObserver( this );

    m_bIsUseable = isVisible() && m_pLayout->getActiveVar().get();

    // register Video Control
    VoutManager::instance( getIntf() )->registerCtrlVideo( this );

    msg_Dbg( getIntf(),"New VideoControl detected(%p), useability=%s",
                           this, m_bIsUseable ? "true" : "false" );
}


void CtrlVideo::unsetLayout()
{
    m_pLayout->getActiveVar().delObserver( this );
    CtrlGeneric::unsetLayout();
}


void CtrlVideo::resizeControl( int width, int height )
{
    WindowManager &rWindowManager =
        getIntf()->p_sys->p_theme->getWindowManager();

    const Position *pPos = getPosition();

    if( width != pPos->getWidth() || height != pPos->getHeight() )
    {
        // new layout dimensions
        int newWidth = width + m_xShift;
        int newHeight = height + m_yShift;

        // Resize the layout
        rWindowManager.startResize( m_rLayout, WindowManager::kResizeSE );
        rWindowManager.resize( m_rLayout, newWidth, newHeight );
        rWindowManager.stopResize();

        if( m_pVoutWindow )
        {
            m_pVoutWindow->resize( pPos->getWidth(), pPos->getHeight() );
            m_pVoutWindow->move( pPos->getLeft(), pPos->getTop() );
        }
    }
}


void CtrlVideo::onUpdate( Subject<VarBool> &rVariable, void *arg  )
{
    // Visibility changed
    if( &rVariable == m_pVisible )
    {
        msg_Dbg( getIntf(), "VideoCtrl : Visibility changed (visible=%d)",
                                  isVisible() );
        notifyLayout();
    }

    // Active Layout changed
    if( &rVariable == &m_pLayout->getActiveVar() )
    {
        msg_Dbg( getIntf(), "VideoCtrl : Active Layout changed (isActive=%d)",
                      m_pLayout->getActiveVar().get() );
    }

    VarBool &rFullscreen = VlcProc::instance( getIntf() )->getFullscreenVar();
    if( &rVariable == &rFullscreen )
    {
        msg_Dbg( getIntf(), "VideoCtrl : fullscreen toggled (fullscreen = %d)",
                      rFullscreen.get() );
    }

    m_bIsUseable = isVisible() &&
                   m_pLayout->getActiveVar().get() &&
                   !rFullscreen.get();

    if( m_bIsUseable && !isUsed() )
    {
        VoutManager::instance( getIntf() )->requestVout( this );
    }
    else if( !m_bIsUseable && isUsed() )
    {
        VoutManager::instance( getIntf() )->discardVout( this );
    }
}

void CtrlVideo::attachVoutWindow( VoutWindow* pVoutWindow, int width, int height )
{
    width = ( width < 0 ) ? pVoutWindow->getOriginalWidth() : width;
    height = ( height < 0 ) ? pVoutWindow->getOriginalHeight() : height;

    WindowManager &rWindowManager =
        getIntf()->p_sys->p_theme->getWindowManager();
    TopWindow* pWin = getWindow();
    rWindowManager.show( *pWin );

    if( m_bAutoResize && width && height )
    {
        int newWidth = width + m_xShift;
        int newHeight = height + m_yShift;

        rWindowManager.startResize( m_rLayout, WindowManager::kResizeSE );
        rWindowManager.resize( m_rLayout, newWidth, newHeight );
        rWindowManager.stopResize();
    }

    pVoutWindow->setCtrlVideo( this );

    m_pVoutWindow = pVoutWindow;
}


void CtrlVideo::detachVoutWindow( )
{
    m_pVoutWindow->setCtrlVideo( NULL );
    m_pVoutWindow = NULL;
}

