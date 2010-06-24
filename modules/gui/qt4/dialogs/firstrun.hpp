/*****************************************************************************
 * firstrun : First Run dialogs
 ****************************************************************************
 * Copyright © 2009 VideoLAN
 * $Id: f45351344c3ff10890de28971e137321e6a87db0 $
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
 * along with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#include "qt4.hpp"

#include <QWidget>
#include <QSettings>

class QCheckBox;
class FirstRun : public QWidget
{
    Q_OBJECT
    public:
        static void CheckAndRun( QWidget *_p, intf_thread_t *p_intf )
        {
            if( getSettings()->value( "IsFirstRun", 1 ).toInt() )
            {
                if( var_InheritBool( p_intf, "qt-privacy-ask") )
                {
                    new FirstRun( _p, p_intf );
                }
                getSettings()->setValue( "IsFirstRun", 0 );
            }
        }
        FirstRun( QWidget *, intf_thread_t * );
    private:
        QCheckBox *checkbox, *checkbox2;
        intf_thread_t *p_intf;
        void buildPrivDialog();
    private slots:
        void save();
};

