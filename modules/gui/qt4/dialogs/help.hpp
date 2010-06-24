/*****************************************************************************
 * Help.hpp : Help and About dialogs
 ****************************************************************************
 * Copyright (C) 2007 the VideoLAN team
 * $Id: 0f1a2b2366a794fe2bf49d68f4c593e4b881bf2e $
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

#ifndef QVLC_HELP_DIALOG_H_
#define QVLC_HELP_DIALOG_H_ 1

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "qt4.hpp"

#include "util/qvlcframe.hpp"
#include "util/singleton.hpp"

class QPushButton;
class QTextBrowser;
class QLabel;
class QEvent;
class QPushButton;
class QTextEdit;

class HelpDialog : public QVLCFrame, public Singleton<HelpDialog>
{
    Q_OBJECT
private:
    HelpDialog( intf_thread_t * );
    virtual ~HelpDialog();

public slots:
    void close();

    friend class    Singleton<HelpDialog>;
};


class AboutDialog : public QVLCDialog, public Singleton<AboutDialog>
{
    Q_OBJECT

private:
    AboutDialog( intf_thread_t * );
    virtual ~AboutDialog();

public slots:
    void close();

    friend class    Singleton<AboutDialog>;
};

#ifdef UPDATE_CHECK

static const int UDOkEvent = QEvent::User + DialogEventType + 21;
static const int UDErrorEvent = QEvent::User + DialogEventType + 22;

class UpdateDialog : public QVLCFrame, public Singleton<UpdateDialog>
{
    Q_OBJECT
public:
    void updateNotify( bool );

private:
    UpdateDialog( intf_thread_t * );
    virtual ~UpdateDialog();

    update_t *p_update;
    QPushButton *updateButton;
    QLabel *updateLabelTop;
    QLabel *updateLabelDown;
    QTextEdit *updateText;
    void customEvent( QEvent * );
    bool b_checked;

private slots:
    void close();
    void UpdateOrDownload();

    friend class    Singleton<UpdateDialog>;
};
#endif

#endif
