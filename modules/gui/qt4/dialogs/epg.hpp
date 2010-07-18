/*****************************************************************************
 * epg.cpp : EPG Viewer dialog
 ****************************************************************************
 * Copyright © 2010 VideoLAN and AUTHORS
 *
 * Authors:    Jean-Baptiste Kempf <jb@videolan.org>
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
 * along with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef QVLC_EPG_DIALOG_H_
#define QVLC_EPG_DIALOG_H_ 1

#include "util/qvlcframe.hpp"

#include "util/singleton.hpp"

class QLabel;
class QTextEdit;
class EPGEvent;
class EPGWidget;

class EpgDialog : public QVLCFrame, public Singleton<EpgDialog>
{
    Q_OBJECT
private:
    EpgDialog( intf_thread_t * );
    virtual ~EpgDialog();

    EPGWidget *epg;
    QTextEdit *description;
    QLabel *title;

    friend class    Singleton<EpgDialog>;

private slots:
    void showEvent( EPGEvent * );
    void updateInfos();
};

#endif

