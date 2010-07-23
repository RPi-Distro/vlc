/*****************************************************************************
 * EPGView.h : EPGView
 ****************************************************************************
 * Copyright © 2009-2010 VideoLAN
 * $Id: 6c26be11f202a0409c2a988a95b1e56acd3bf61c $
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

#ifndef EPGVIEW_H
#define EPGVIEW_H

#include "EPGEvent.hpp"

#include <QGraphicsView>
#include <QList>

#define TRACKS_HEIGHT 60

class QDateTime;
class EPGView : public QGraphicsView
{
Q_OBJECT

public:
    explicit EPGView( QWidget *parent = 0 );

    void            setScale( double scaleFactor );

    void            updateStartTime();
    const QDateTime& startTime();

    void            addEvent( EPGEvent* event );
    void            delEvent( EPGEvent* event );
    void            updateDuration();

    QList<QString>  getChannelList();

signals:
    void            startTimeChanged( const QDateTime& startTime );
    void            durationChanged( int seconds );
    void            eventFocusedChanged( EPGEvent * );
protected:

    QList<QString>  m_channels;
    QDateTime       m_startTime;
    int             m_scaleFactor;
    int             m_duration;

public slots:
    void eventFocused( EPGEvent * );
};

#endif // EPGVIEW_H
