/*****************************************************************************
 * recents.cpp : Recents MRL (menu)
 *****************************************************************************
 * Copyright © 2006-2008 the VideoLAN team
 * $Id: 48171782ba01625ecfd7b97b8a0733debfcda110 $
 *
 * Authors: Ludovic Fauvet <etix@l0cal.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * ( at your option ) any later version.
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

#ifndef QVLC_RECENTS_H_
#define QVLC_RECENTS_H_

#include "qt4.hpp"

#include <QObject>
#include <QList>
#include <QString>
#include <QRegExp>
#include <QSignalMapper>

#define RECENTS_LIST_SIZE 10

class RecentsMRL : public QObject
{
    Q_OBJECT

public:
    static RecentsMRL* getInstance( intf_thread_t* p_intf )
    {
        if(!instance)
            instance = new RecentsMRL( p_intf );
        return instance;
    }
    static void killInstance()
    {
        delete instance;
        instance = NULL;
    }

    void addRecent( const QString & );
    QList<QString> recents();
    QSignalMapper *signalMapper;

private:
    RecentsMRL( intf_thread_t* _p_intf );
    virtual ~RecentsMRL();

    static RecentsMRL *instance;

    void load();
    void save();
    intf_thread_t* p_intf;
    QList<QString> *stack;
    bool isActive;
    QRegExp *filter;

public slots:
    void clear();
};

#endif
