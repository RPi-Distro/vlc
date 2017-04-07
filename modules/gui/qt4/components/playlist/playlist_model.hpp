/*****************************************************************************
 * playlist_model.hpp : Model for a playlist tree
 ****************************************************************************
 * Copyright (C) 2006-2011 the VideoLAN team
 * $Id: 1f71ac9d2a0dcb9b1179c4b599e807a81b5280f2 $
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
 *          Jakob Leben <jleben@videolan.org>
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

#ifndef _PLAYLIST_MODEL_H_
#define _PLAYLIST_MODEL_H_

#include "qt4.hpp"

#include <vlc_input.h>
#include <vlc_playlist.h>
#include "vlc_model.hpp"
#include "playlist_item.hpp"

#include <QObject>
#include <QEvent>
#include <QSignalMapper>
#include <QMimeData>
#include <QAbstractItemModel>
#include <QVariant>
#include <QModelIndex>
#include <QAction>

class PLItem;
class PLSelector;
class PlMimeData;

class PLModel : public VLCModel
{
    Q_OBJECT

public:
    PLModel( playlist_t *, intf_thread_t *,
             playlist_item_t *, QObject *parent = 0 );
    virtual ~PLModel();

    /* Qt main PLModel */
    static PLModel* getPLModel( intf_thread_t *p_intf )
    {
        if(!p_intf->p_sys->pl_model )
        {
            playlist_Lock( THEPL );
            playlist_item_t *p_root = THEPL->p_playing;
            playlist_Unlock( THEPL );
            p_intf->p_sys->pl_model = new PLModel( THEPL, p_intf, p_root, NULL );
        }

        return p_intf->p_sys->pl_model;
    }

    /*** QAbstractItemModel subclassing ***/

    /* Data structure */
    virtual QVariant data( const QModelIndex &index, const int role ) const;
    virtual int rowCount( const QModelIndex &parent = QModelIndex() ) const;
    virtual Qt::ItemFlags flags( const QModelIndex &index ) const;
    virtual QModelIndex index( const int r, const int c, const QModelIndex &parent ) const;
    virtual QModelIndex parent( const QModelIndex &index ) const;

    /* Drag and Drop */
    virtual Qt::DropActions supportedDropActions() const;
    virtual QMimeData* mimeData( const QModelIndexList &indexes ) const;
    virtual bool dropMimeData( const QMimeData *data, Qt::DropAction action,
                      int row, int column, const QModelIndex &target );
    virtual QStringList mimeTypes() const;

    /* Sort */
    virtual void sort( const int column, Qt::SortOrder order = Qt::AscendingOrder );

    /*** VLCModelSubInterface subclassing ***/
    virtual void rebuild( playlist_item_t * p = NULL );
    virtual void doDelete( QModelIndexList selected );
    virtual void createNode( QModelIndex index, QString name );
    virtual void renameNode( QModelIndex index, QString name );
    virtual void removeAll();

    /* Lookups */
    virtual QModelIndex rootIndex() const;
    virtual void filter( const QString& search_text, const QModelIndex & root, bool b_recursive );
    virtual QModelIndex currentIndex() const;
    virtual QModelIndex indexByPLID( const int i_plid, const int c ) const;
    virtual QModelIndex indexByInputItemID( const int i_inputitem_id, const int c ) const;
    virtual bool isTree() const;
    virtual bool canEdit() const;
    virtual bool action( QAction *action, const QModelIndexList &indexes );
    virtual bool isSupportedAction( actions action, const QModelIndex & ) const;

protected:
    /* VLCModel subclassing */
    bool isParent( const QModelIndex &index, const QModelIndex &current) const;
    bool isLeaf( const QModelIndex &index ) const;
    PLItem *getItem( const QModelIndex & index ) const;

private:
    /* General */
    PLItem *rootItem;

    playlist_t *p_playlist;

    /* Custom model private methods */
    /* Lookups */
    QModelIndex index( PLItem *, const int c ) const;

    /* Shallow actions (do not affect core playlist) */
    void updateTreeItem( PLItem * );
    void removeItem ( PLItem * );
    void recurseDelete( QList<AbstractPLItem*> children, QModelIndexList *fullList );
    void takeItem( PLItem * ); //will not delete item
    void insertChildren( PLItem *node, QList<PLItem*>& items, int i_pos );
    /* ...of which  the following will not update the views */
    void updateChildren( PLItem * );
    void updateChildren( playlist_item_t *, PLItem * );

    /* Deep actions (affect core playlist) */
    void dropAppendCopy( const PlMimeData * data, PLItem *target, int pos );
    void dropMove( const PlMimeData * data, PLItem *target, int new_pos );

    /* */
    void sort( QModelIndex caller, QModelIndex rootIndex, const int column, Qt::SortOrder order );

    /* Lookups */
    PLItem *findByPLId( PLItem *, int i_plitemid ) const;
    PLItem *findByInputId( PLItem *, int i_input_itemid ) const;
    PLItem *findInner(PLItem *, int i_id, bool b_isinputid ) const;
    enum pl_nodetype
    {
        ROOTTYPE_CURRENT_PLAYING,
        ROOTTYPE_MEDIA_LIBRARY,
        ROOTTYPE_OTHER
    };
    pl_nodetype getPLRootType() const;

    /* */
    QString latestSearch;

private slots:
    void processInputItemUpdate( input_item_t *);
    void processInputItemUpdate();
    void processItemRemoval( int i_pl_itemid );
    void processItemAppend( int i_pl_itemid, int i_pl_itemidparent );
    void activateItem( playlist_item_t *p_item );
    void activateItem( const QModelIndex &index );
};

class PlMimeData : public QMimeData
{
    Q_OBJECT

public:
    PlMimeData() {}
    virtual ~PlMimeData();
    void appendItem( input_item_t *p_item );
    QList<input_item_t*> inputItems() const;
    QStringList formats () const;

private:
    QList<input_item_t*> _inputItems;
    QMimeData *_mimeData;
};

#endif
