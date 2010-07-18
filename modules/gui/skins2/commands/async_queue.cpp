/*****************************************************************************
 * async_queue.cpp
 *****************************************************************************
 * Copyright (C) 2003 the VideoLAN team
 * $Id: beee424786c6d4c2179f954020d46b4226d2dcc2 $
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

#include "async_queue.hpp"
#include "../src/os_factory.hpp"
#include "../src/os_timer.hpp"


AsyncQueue::AsyncQueue( intf_thread_t *pIntf ): SkinObject( pIntf ),
    m_cmdFlush( this )
{
    // Initialize the mutex
    vlc_mutex_init( pIntf, &m_lock );

    // Create a timer
    OSFactory *pOsFactory = OSFactory::instance( pIntf );
    m_pTimer = pOsFactory->createOSTimer( m_cmdFlush );

    // Flush the queue every 10 ms
    m_pTimer->start( 10, false );
}


AsyncQueue::~AsyncQueue()
{
    delete( m_pTimer );
    vlc_mutex_destroy( &m_lock );
}


AsyncQueue *AsyncQueue::instance( intf_thread_t *pIntf )
{
    if( ! pIntf->p_sys->p_queue )
    {
        AsyncQueue *pQueue;
        pQueue = new AsyncQueue( pIntf );
        if( pQueue )
        {
             // Initialization succeeded
             pIntf->p_sys->p_queue = pQueue;
        }
     }
     return pIntf->p_sys->p_queue;
}


void AsyncQueue::destroy( intf_thread_t *pIntf )
{
    if( pIntf->p_sys->p_queue )
    {
        delete pIntf->p_sys->p_queue;
        pIntf->p_sys->p_queue = NULL;
    }
}


void AsyncQueue::push( const CmdGenericPtr &rcCommand, bool removePrev )
{
    if( removePrev )
    {
        // Remove the commands of the same type
        remove( rcCommand.get()->getType(), rcCommand );
    }
    m_cmdList.push_back( rcCommand );
}


void AsyncQueue::remove( const string &rType, const CmdGenericPtr &rcCommand )
{
    vlc_mutex_lock( &m_lock );

    list<CmdGenericPtr>::iterator it;
    for( it = m_cmdList.begin(); it != m_cmdList.end(); it++ )
    {
        // Remove the command if it is of the given type
        if( (*it).get()->getType() == rType )
        {
            // Maybe the command wants to check if it must really be
            // removed
            if( rcCommand.get()->checkRemove( (*it).get() ) == true )
            {
                list<CmdGenericPtr>::iterator itNew = it;
                itNew++;
                m_cmdList.erase( it );
                it = itNew;
            }
        }
    }

    vlc_mutex_unlock( &m_lock );
}


void AsyncQueue::flush()
{
    while (true)
    {
        vlc_mutex_lock( &m_lock );

        if( m_cmdList.size() > 0 )
        {
            // Pop the first command from the queue
            CmdGenericPtr cCommand = m_cmdList.front();
            m_cmdList.pop_front();

            // Unlock the mutex to avoid deadlocks if another thread wants to
            // enqueue/remove a command while this one is processed
            vlc_mutex_unlock( &m_lock );

            // Execute the command
            cCommand.get()->execute();
        }
        else
        {
            vlc_mutex_unlock( &m_lock );
            break;
        }
    }
}


void AsyncQueue::CmdFlush::execute()
{
    // Flush the queue
    m_pParent->flush();
}
