/*****************************************************************************
 * Copyright © 2011 VideoLAN
 * $Id: d239d9984b9689ea7029fd6d8a6a8a5a9b201ae4 $
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
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef TIMETOOLTIP_H
#define TIMETOOLTIP_H

#include "qt.hpp"

#include <QWidget>
#include <QBitmap>

class QPaintEvent;
class QString;
class QFont;
class QRect;
class QPainterPath;

class TimeTooltip : public QWidget
{
    Q_OBJECT
public:
    explicit TimeTooltip( QWidget *parent = 0 );
    void setTip( const QPoint& pos, const QString& time, const QString& text );
    virtual void show();

protected:
    void paintEvent( QPaintEvent * ) Q_DECL_OVERRIDE;

private:
    void adjustPosition();
    void buildPath();
    QPoint mTarget;
    QString mTime;
    QString mText;
    QString mDisplayedText;
    QFont mFont;
    QRect mBox;
    QPainterPath mPainterPath;
    QBitmap mMask;
    int mTipX;
};

#endif // TIMETOOLTIP_H
