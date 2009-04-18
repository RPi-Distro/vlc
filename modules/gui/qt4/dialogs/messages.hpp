/*****************************************************************************
 * Messages.hpp : Information about a stream
 ****************************************************************************
 * Copyright (C) 2006-2007 the VideoLAN team
 * $Id: 4ca1b1744cef4b51a003fe9c4e546c38fb22df8b $
 *
 * Authors: Jean-Baptiste Kempf <jb (at) videolan.org>
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

#ifndef _MESSAGES_DIALOG_H_
#define _MESSAGES_DIALOG_H_

#include "util/qvlcframe.hpp"

class QTabWidget;
class QPushButton;
class QSpinBox;
class QGridLayout;
class QLabel;
class QTextEdit;
class QTreeWidget;
class QTreeWidgetItem;

class MessagesDialog : public QVLCFrame
{
    Q_OBJECT;
public:
    static MessagesDialog * getInstance( intf_thread_t *p_intf )
    {
        if( !instance)
            instance = new MessagesDialog( p_intf );
        return instance;
    }
    static void killInstance()
    {
        if( instance ) delete instance;
        instance = NULL;
    }

    virtual ~MessagesDialog(){ writeSettings( "messages" ); };

private:
    MessagesDialog( intf_thread_t * );
    static MessagesDialog *instance;
    QTabWidget *mainTab;
    QSpinBox *verbosityBox;
    QLabel *verbosityLabel;
    QTextEdit *messages;
    QTreeWidget *modulesTree;
    QPushButton *clearUpdateButton;
    QPushButton *saveLogButton;

private slots:
    void updateTab( int );
    void updateLog();
    void clearOrUpdate();
    bool save();
private:
    void clear();
    void updateTree();
    void buildTree( QTreeWidgetItem *, vlc_object_t * );
};

#endif
