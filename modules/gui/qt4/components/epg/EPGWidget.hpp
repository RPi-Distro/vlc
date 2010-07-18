/*****************************************************************************
 * EPGWidget.h : EPGWidget
 ****************************************************************************
 * Copyright © 2009-2010 VideoLAN
 * $Id: ad03eac465e2cadb942a9b8fc53473f2d7e8b92d $
 *
 * Authors: Ludovic Fauvet <etix@l0cal.com>
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

#ifndef EPGWIDGET_H
#define EPGWIDGET_H

#include "EPGView.hpp"
#include "EPGEvent.hpp"
#include "EPGRuler.hpp"
#include "EPGChannels.hpp"

#include <vlc_common.h>
#include <vlc_epg.h>

#include <QWidget>
#include <QMultiMap>

class QDateTime;

class EPGWidget : public QWidget
{
    Q_OBJECT
public:
    explicit EPGWidget( QWidget* parent = 0 );

public slots:
    void setZoom( int level );
    void updateEPG( vlc_epg_t **pp_epg, int i_epg );

private:
    EPGRuler* m_rulerWidget;
    EPGView* m_epgView;
    EPGChannels *m_channelsWidget;

    QMultiMap<QString, EPGEvent*> m_events;

signals:
    void itemSelectionChanged( EPGEvent * );
};

#endif // EPGWIDGET_H
