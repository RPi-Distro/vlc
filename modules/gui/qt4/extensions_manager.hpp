/*****************************************************************************
 * extensions_manager.hpp: Extensions manager for Qt
 ****************************************************************************
 * Copyright (C) 2009-2010 VideoLAN and authors
 * $Id: 17f5af95727cdc3a9121ef2bf9715990c5b2f67b $
 *
 * Authors: Jean-Philippe André < jpeg # videolan.org >
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

#ifndef EXTENSIONS_MANAGER_HPP
#define EXTENSIONS_MANAGER_HPP

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_extensions.h>

#include "qt4.hpp"

#include <QObject>
#include <QMenu>
#include <QSignalMapper>

class ExtensionsDialogProvider;

class ExtensionsManager : public QObject
{
    Q_OBJECT
public:
    static ExtensionsManager *getInstance( intf_thread_t *_p_intf,
                                           QObject *_parent = 0 )
    {
        if( !instance )
            instance = new ExtensionsManager( _p_intf, _parent );
        return instance;
    }
    static void killInstance()
    {
        delete instance;
        instance = NULL;
    }

    ExtensionsManager( intf_thread_t *p_intf, QObject *parent );
    virtual ~ExtensionsManager();

    inline bool isLoaded() { return p_extensions_manager != NULL; }
    inline bool cannotLoad() { return b_unloading || b_failed; }
    inline bool isUnloading() { return b_unloading; }
    void menu( QMenu *current );

    /** Get the extensions_manager_t if it is loaded and hold the object */
    extensions_manager_t* getManager()
    {
        if( !p_extensions_manager ) return NULL;
        vlc_object_hold( p_extensions_manager );
        return p_extensions_manager;
    }

public slots:
    bool loadExtensions();
    void unloadExtensions();
    void reloadExtensions();

private slots:
    void triggerMenu( int id );
    void inputChanged( input_thread_t *p_input );
    void playingChanged( int );

private:
    static ExtensionsManager* instance;
    intf_thread_t *p_intf;
    extensions_manager_t *p_extensions_manager;
    ExtensionsDialogProvider *p_edp;

    QSignalMapper *menuMapper;
    bool b_unloading;  ///< Work around threads + emit issues, see isUnloading
    bool b_failed; ///< Flag set to true if we could not load the module

signals:
    void extensionsUpdated();
};

#endif // EXTENSIONS_MANAGER_HPP
