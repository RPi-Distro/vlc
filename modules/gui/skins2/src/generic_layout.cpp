/*****************************************************************************
 * generic_layout.cpp
 *****************************************************************************
 * Copyright (C) 2003 the VideoLAN team
 * $Id: 731d8dd75c1fe87147fb2f7c0e2348ea4d599914 $
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

#include "generic_layout.hpp"
#include "top_window.hpp"
#include "os_factory.hpp"
#include "os_graphics.hpp"
#include "var_manager.hpp"
#include "anchor.hpp"
#include "../controls/ctrl_generic.hpp"
#include "../controls/ctrl_video.hpp"
#include "../utils/var_bool.hpp"
#include <set>


GenericLayout::GenericLayout( intf_thread_t *pIntf, int width, int height,
                              int minWidth, int maxWidth, int minHeight,
                              int maxHeight ):
    SkinObject( pIntf ), m_pWindow( NULL ), m_rect( 0, 0, width, height ),
    m_minWidth( minWidth ), m_maxWidth( maxWidth ),
    m_minHeight( minHeight ), m_maxHeight( maxHeight ), m_pVideoCtrlSet(),
    m_visible( false ), m_pVarActive( NULL )
{
    // Get the OSFactory
    OSFactory *pOsFactory = OSFactory::instance( getIntf() );
    // Create the graphics buffer
    m_pImage = pOsFactory->createOSGraphics( width, height );

    // Create the "active layout" variable and register it in the manager
    m_pVarActive = new VarBoolImpl( pIntf );
    VarManager::instance( pIntf )->registerVar( VariablePtr( m_pVarActive ) );
}


GenericLayout::~GenericLayout()
{
    delete m_pImage;

    list<Anchor*>::const_iterator it;
    for( it = m_anchorList.begin(); it != m_anchorList.end(); it++ )
    {
        delete *it;
    }

    list<LayeredControl>::const_iterator iter;
    for( iter = m_controlList.begin(); iter != m_controlList.end(); iter++ )
    {
        CtrlGeneric *pCtrl = (*iter).m_pControl;
        pCtrl->unsetLayout();
    }

}


void GenericLayout::setWindow( TopWindow *pWindow )
{
    m_pWindow = pWindow;
}


void GenericLayout::onControlCapture( const CtrlGeneric &rCtrl )
{
    // Just forward the request to the window
    TopWindow *pWindow = getWindow();
    if( pWindow )
    {
        pWindow->onControlCapture( rCtrl );
    }
}


void GenericLayout::onControlRelease( const CtrlGeneric &rCtrl )
{
    // Just forward the request to the window
    TopWindow *pWindow = getWindow();
    if( pWindow )
    {
        pWindow->onControlRelease( rCtrl );
    }
}


void GenericLayout::addControl( CtrlGeneric *pControl,
                                const Position &rPosition, int layer )
{
    if( pControl )
    {
        // Associate this layout to the control
        pControl->setLayout( this, rPosition );

        // Draw the control
        if( pControl->isVisible() )
            pControl->draw( *m_pImage, rPosition.getLeft(), rPosition.getTop() );

        // Add the control in the list.
        // This list must remain sorted by layer order
        list<LayeredControl>::iterator it;
        for( it = m_controlList.begin(); it != m_controlList.end(); it++ )
        {
            if( layer < (*it).m_layer )
            {
                m_controlList.insert( it, LayeredControl( pControl, layer ) );
                break;
            }
        }
        // If this control is in front of all the previous ones
        if( it == m_controlList.end() )
        {
            m_controlList.push_back( LayeredControl( pControl, layer ) );
        }

        // Check if it is a video control
        if( pControl->getType() == "video" )
        {
            m_pVideoCtrlSet.insert( (CtrlVideo*)pControl );
        }
    }
    else
    {
        msg_Dbg( getIntf(), "adding NULL control in the layout" );
    }
}


const list<LayeredControl> &GenericLayout::getControlList() const
{
    return m_controlList;
}


void GenericLayout::onControlUpdate( const CtrlGeneric &rCtrl,
                                     int width, int height,
                                     int xOffSet, int yOffSet )
{
    // The size is not valid, refresh the whole layout
    if( width <= 0 || height <= 0 )
    {
        refreshAll();
        return;
    }

    const Position *pPos = rCtrl.getPosition();
    if( pPos )
    {
        refreshRect( pPos->getLeft() + xOffSet,
                     pPos->getTop() + yOffSet,
                     width, height );
    }
}


void GenericLayout::resize( int width, int height )
{
    // Update the window size
    m_rect = SkinsRect( 0, 0 , width, height );

    // Recreate a new image
    if( m_pImage )
    {
        delete m_pImage;
        OSFactory *pOsFactory = OSFactory::instance( getIntf() );
        m_pImage = pOsFactory->createOSGraphics( width, height );
    }

    // Notify all the controls that the size has changed and redraw them
    list<LayeredControl>::const_iterator iter;
    for( iter = m_controlList.begin(); iter != m_controlList.end(); iter++ )
    {
        iter->m_pControl->onResize();
    }

    // Resize and refresh the associated window
    TopWindow *pWindow = getWindow();
    if( pWindow )
    {
        // Resize the window
        pWindow->resize( width, height );
        // Change the shape of the window and redraw it
        refreshAll();
    }
}


void GenericLayout::refreshAll()
{
    refreshRect( 0, 0, m_rect.getWidth(), m_rect.getHeight() );
}


void GenericLayout::refreshRect( int x, int y, int width, int height )
{
    // Do nothing if the layout is hidden
    if( !m_visible )
        return;

    // update the transparency global mask
    m_pImage->clear( x, y, width, height );

    // Draw all the controls of the layout
    list<LayeredControl>::const_iterator iter;
    list<LayeredControl>::const_iterator iterVideo = m_controlList.end();
    for( iter = m_controlList.begin(); iter != m_controlList.end(); iter++ )
    {
        CtrlGeneric *pCtrl = (*iter).m_pControl;
        const Position *pPos = pCtrl->getPosition();
        if( pPos && pCtrl->isVisible() )
        {
            pCtrl->draw( *m_pImage, pPos->getLeft(), pPos->getTop() );
        }
    }

    // Refresh the associated window
    TopWindow *pWindow = getWindow();
    if( pWindow )
    {
        // first apply new shape to the window
        pWindow->updateShape();

        // Check boundaries
        if( x < 0 )
            x = 0;
        if( y < 0)
            y = 0;
        if( x + width > m_rect.getWidth() )
            width = m_rect.getWidth() - x;
        if( y + height > m_rect.getHeight() )
            height = m_rect.getHeight() - y;

        pWindow->refresh( x, y, width, height );
    }
}


const list<Anchor*>& GenericLayout::getAnchorList() const
{
    return m_anchorList;
}


void GenericLayout::addAnchor( Anchor *pAnchor )
{
    m_anchorList.push_back( pAnchor );
}


void GenericLayout::onShow()
{
    m_visible = true;

    refreshAll();
}


void GenericLayout::onHide()
{
    m_visible = false;
}

