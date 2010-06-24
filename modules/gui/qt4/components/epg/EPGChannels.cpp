/*****************************************************************************
 * EPGChannels.cpp: EPGChannels
 ****************************************************************************
 * Copyright © 2009-2010 VideoLAN
 *
 * Authors: Adrien Maglo <magsoft@videolan.org>
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

#include "EPGChannels.hpp"
#include "EPGView.hpp"

#include <QPainter>
#include <QFont>
#include <QPaintEvent>

EPGChannels::EPGChannels( QWidget *parent, EPGView *m_epgView )
    : QWidget( parent ), m_epgView( m_epgView ), m_offset( 0 )
{
    setContentsMargins( 0, 0, 0, 0 );
}

void EPGChannels::setOffset( int offset )
{
    m_offset = offset;
    update();
}

void EPGChannels::paintEvent( QPaintEvent *event )
{
    Q_UNUSED( event );

    QPainter p( this );

    /* Draw the top and the bottom lines. */
    p.drawLine( 0, 0, width() - 1, 0 );
    p.drawLine( 0, height() - 1, width(), height() - 1 );

    p.setFont( QFont( "Verdana", 8 ) );

    QList<QString> channels = m_epgView->getChannelList();

    for( int i = 0; i < channels.count(); ++i )
    {
        p.drawText( 0, - m_offset + ( i + 0.5 ) * TRACKS_HEIGHT - 4,
                    width(), 20, Qt::AlignLeft, channels[i] );

        int i_width = fontMetrics().width( channels[i] );
        if( width() < i_width )
            setMinimumWidth( i_width );
    }
}
