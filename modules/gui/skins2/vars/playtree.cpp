/*****************************************************************************
 * playtree.cpp
 *****************************************************************************
 * Copyright (C) 2005 the VideoLAN team
 * $Id: 4f760efb62d18a564042f19c060e1cc0d0e8052e $
 *
 * Authors: Antoine Cellerier <dionoea@videolan.org>
 *          Clément Stenac <zorglub@videolan.org>
 *          Erwan Tulou    <erwan10@videolan.org>
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
# include "config.h"
#endif

#include <vlc_common.h>

#include "playtree.hpp"
#include <vlc_playlist.h>
#include <vlc_url.h>
#include "../utils/ustring.hpp"

Playtree::Playtree( intf_thread_t *pIntf )
    : VarTree( pIntf ), m_pPlaylist( pIntf->p_sys->p_playlist )
{
    getPositionVar().addObserver( this );
    buildTree();
}

Playtree::~Playtree()
{
    getPositionVar().delObserver( this );
}

void Playtree::delSelected()
{
    for( Iterator it = m_children.begin(); it != m_children.end(); )
    {
        if( it->isSelected() && !it->isReadonly() )
        {
            playlist_Lock( m_pPlaylist );

            playlist_item_t *pItem =
                playlist_ItemGetById( m_pPlaylist, it->getId() );
            if( pItem )
            {
                if( pItem->i_children == -1 )
                {
                    playlist_DeleteFromInput( m_pPlaylist, pItem->p_input,
                                              pl_Locked );
                }
                else
                {
                    playlist_NodeDelete( m_pPlaylist, pItem, true, false );
                }
            }
            playlist_Unlock( m_pPlaylist );

            it = it->getNextSiblingOrUncle();
        }
        else
        {
            it = getNextItem( it );
        }
    }
}

void Playtree::action( VarTree *pElem )
{
    playlist_Lock( m_pPlaylist );

    playlist_item_t *pItem =
        playlist_ItemGetById( m_pPlaylist, pElem->getId() );
    if( pItem )
    {
        playlist_Control( m_pPlaylist, PLAYLIST_VIEWPLAY,
                          pl_Locked, pItem->p_parent, pItem );
    }

    playlist_Unlock( m_pPlaylist );
}

void Playtree::onChange()
{
    buildTree();
    tree_update descr( tree_update::ResetAll, end() );
    notify( &descr );
}

void Playtree::onUpdateItem( int id )
{
    Iterator it = findById( id );
    if( it != m_children.end() )
    {
        // Update the item
        playlist_Lock( m_pPlaylist );
        playlist_item_t *pNode =
            playlist_ItemGetById( m_pPlaylist, it->getId() );
        if( !pNode )
        {
            playlist_Unlock( m_pPlaylist );
            return;
        }

        UString *pName = new UString( getIntf(), pNode->p_input->psz_name );
        playlist_Unlock( m_pPlaylist );

        if( *pName != *(it->getString()) )
        {
            it->setString( UStringPtr( pName ) );

            tree_update descr(
                tree_update::ItemUpdated, IteratorVisible( it, this ) );
            notify( &descr );
        }
    }
    else
    {
        msg_Warn( getIntf(), "cannot find node with id %d", id );
    }
}

void Playtree::onUpdateCurrent( bool b_active )
{
    if( b_active )
    {
        playlist_Lock( m_pPlaylist );

        playlist_item_t* current = playlist_CurrentPlayingItem( m_pPlaylist );
        if( !current )
        {
            playlist_Unlock( m_pPlaylist );
            return;
        }

        Iterator it = findById( current->i_id );
        if( it != m_children.end() )
        {
            it->setPlaying( true );

            tree_update descr(
                tree_update::ItemUpdated, IteratorVisible( it, this ) );
            notify( &descr );
        }

        playlist_Unlock( m_pPlaylist );
    }
    else
    {
        for( Iterator it = m_children.begin(); it != m_children.end();
             it = getNextItem( it ) )
        {
            if( it->isPlaying() )
            {
                it->setPlaying( false );

                tree_update descr(
                    tree_update::ItemUpdated, IteratorVisible( it, this ) );
                notify( &descr );
                break;
            }
        }
    }
}

void Playtree::onDelete( int i_id )
{
    Iterator it = findById( i_id ) ;
    if( it != m_children.end() )
    {
        VarTree* parent = it->parent();
        if( parent )
        {
            tree_update descr(
                tree_update::DeletingItem, IteratorVisible( it, this ) );
            notify( &descr );

            parent->removeChild( it );
            m_allItems.erase( i_id );

            tree_update descr2(
                tree_update::ItemDeleted, end() );
            notify( &descr2 );
        }
    }
}

void Playtree::onAppend( playlist_add_t *p_add )
{
    Iterator it_node = findById( p_add->i_node );
    if( it_node != m_children.end() )
    {
        playlist_Lock( m_pPlaylist );
        playlist_item_t *pItem =
            playlist_ItemGetById( m_pPlaylist, p_add->i_item );
        if( !pItem )
        {
            playlist_Unlock( m_pPlaylist );
            return;
        }

        int pos;
        for( pos = 0; pos < pItem->p_parent->i_children; pos++ )
            if( pItem->p_parent->pp_children[pos] == pItem ) break;

        UString *pName = new UString( getIntf(),
                                      pItem->p_input->psz_name );

        playlist_item_t* current = playlist_CurrentPlayingItem( m_pPlaylist );

        Iterator it = it_node->add(
            p_add->i_item, UStringPtr( pName ), false, pItem == current,
            false, pItem->i_flags & PLAYLIST_RO_FLAG, pos );

        m_allItems[pItem->i_id] = &*it;

        playlist_Unlock( m_pPlaylist );

        tree_update descr(
            tree_update::ItemInserted,
            IteratorVisible( it, this ) );
        notify( &descr );
    }
}

void Playtree::buildNode( playlist_item_t *pNode, VarTree &rTree )
{
    UString *pName = new UString( getIntf(), pNode->p_input->psz_name );
    Iterator it = rTree.add(
        pNode->i_id, UStringPtr( pName ), false,
        playlist_CurrentPlayingItem(m_pPlaylist) == pNode,
        false, pNode->i_flags & PLAYLIST_RO_FLAG );
    m_allItems[pNode->i_id] = &*it;

    for( int i = 0; i < pNode->i_children; i++ )
    {
        buildNode( pNode->pp_children[i], *it );
    }
}

void Playtree::buildTree()
{
    clear();
    playlist_Lock( m_pPlaylist );

    for( int i = 0; i < m_pPlaylist->p_root->i_children; i++ )
    {
        buildNode( m_pPlaylist->p_root->pp_children[i], *this );
    }

    playlist_Unlock( m_pPlaylist );
}

void Playtree::onUpdateSlider()
{
    tree_update descr( tree_update::SliderChanged, end() );
    notify( &descr );
}

void Playtree::insertItems( VarTree& elem, const list<string>& files, bool start )
{
    bool first = true;
    VarTree* p_elem = &elem;
    playlist_item_t* p_node = NULL;
    int i_pos = -1;

    playlist_Lock( m_pPlaylist );

    if( p_elem == this )
    {
        for( Iterator it = m_children.begin(); it != m_children.end(); ++it )
        {
            if( it->getId() == m_pPlaylist->p_local_category->i_id )
            {
                p_elem = &*it;
                break;
            }
        }
    }

    if( p_elem->getId() == m_pPlaylist->p_local_category->i_id )
    {
        p_node = m_pPlaylist->p_local_category;
        i_pos = 0;
        p_elem->setExpanded( true );
    }
    else if( p_elem->getId() == m_pPlaylist->p_ml_category->i_id )
    {
        p_node = m_pPlaylist->p_ml_category;
        i_pos = 0;
        p_elem->setExpanded( true );
    }
    else if( p_elem->size() && p_elem->isExpanded() )
    {
        p_node = playlist_ItemGetById( m_pPlaylist, p_elem->getId() );
        i_pos = 0;
    }
    else
    {
        p_node = playlist_ItemGetById( m_pPlaylist,
                                       p_elem->parent()->getId() );
        i_pos = p_elem->getIndex();
        i_pos++;
    }

    if( !p_node )
        goto fin;

    for( list<string>::const_iterator it = files.begin();
         it != files.end(); ++it, i_pos++, first = false )
    {
        char* psz_uri = make_URI( it->c_str(), NULL );
        if( !psz_uri )
            continue;

        input_item_t* pItem = input_item_New( psz_uri, NULL );
        if( pItem )
        {
            int i_mode = PLAYLIST_APPEND;
            if( first && start )
                i_mode |= PLAYLIST_GO;

            playlist_NodeAddInput( m_pPlaylist, pItem, p_node,
                                   i_mode, i_pos, pl_Locked );
        }
        free( psz_uri );
    }

fin:
    playlist_Unlock( m_pPlaylist );
}

VarTree::Iterator Playtree::findById( int id )
{
    map<int,VarTree*>::iterator it = m_allItems.find( id );
    if( it == m_allItems.end() )
        return m_children.end();
    else
        return it->second->getSelf();
}
