/*****************************************************************************
 * Controller_widget.cpp : Controller Widget for the controllers
 ****************************************************************************
 * Copyright (C) 2006-2008 the VideoLAN team
 * $Id: 5b5e62bf5631b94da26aaa11c53c075501892192 $
 *
 * Authors: Jean-Baptiste Kempf <jb@videolan.org>
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "controller_widget.hpp"
#include "controller.hpp"

#include "input_manager.hpp"         /* Get notification of Volume Change */
#include "util/input_slider.hpp"     /* SoundSlider */

#include <vlc_aout_intf.h>           /* Volume functions */

#include <QLabel>
#include <QHBoxLayout>
#include <QMenu>
#include <QWidgetAction>
#include <QMouseEvent>

SoundWidget::SoundWidget( QWidget *_parent, intf_thread_t * _p_intf,
                          bool b_shiny, bool b_special )
                         : QWidget( _parent ), p_intf( _p_intf),
                           b_is_muted( false ), b_ignore_valuechanged( false )
{
    /* We need a layout for this widget */
    QHBoxLayout *layout = new QHBoxLayout( this );
    layout->setSpacing( 0 ); layout->setMargin( 0 );

    /* We need a Label for the pix */
    volMuteLabel = new QLabel;
    volMuteLabel->setPixmap( QPixmap( ":/toolbar/volume-medium" ) );

    /* We might need a subLayout too */
    QVBoxLayout *subLayout;

    volMuteLabel->installEventFilter( this );

    /* Normal View, click on icon mutes */
    if( !b_special )
    {
        volumeMenu = NULL; subLayout = NULL;
        volumeControlWidget = NULL;

        /* And add the label */
        layout->addWidget( volMuteLabel, 0, Qt::AlignBottom );
    }
    else
    {
        /* Special view, click on button shows the slider */
        b_shiny = false;

        volumeControlWidget = new QFrame;
        subLayout = new QVBoxLayout( volumeControlWidget );
        subLayout->setContentsMargins( 4, 4, 4, 4 );
        volumeMenu = new QMenu( this );

        QWidgetAction *widgetAction = new QWidgetAction( volumeControlWidget );
        widgetAction->setDefaultWidget( volumeControlWidget );
        volumeMenu->addAction( widgetAction );

        /* And add the label */
        layout->addWidget( volMuteLabel );
    }

    /* Slider creation: shiny or clean */
    if( b_shiny )
    {
        volumeSlider = new SoundSlider( this,
            config_GetInt( p_intf, "volume-step" ),
            false,
            var_InheritString( p_intf, "qt-slider-colours" ) );
    }
    else
    {
        volumeSlider = new QSlider( NULL );
        volumeSlider->setAttribute( Qt::WA_MacSmallSize);
        volumeSlider->setOrientation( b_special ? Qt::Vertical
                                                : Qt::Horizontal );
        volumeSlider->setMaximum( 200 );
    }

    volumeSlider->setFocusPolicy( Qt::NoFocus );
    if( b_special )
        subLayout->addWidget( volumeSlider );
    else
        layout->addWidget( volumeSlider, 0, Qt::AlignBottom  );

    /* Set the volume from the config */
    libUpdateVolume();
    /* Force the update at build time in order to have a muted icon if needed */
    updateMuteStatus();

    /* Volume control connection */
    volumeSlider->setTracking( true );
    CONNECT( volumeSlider, valueChanged( int ), this, valueChangedFilter( int ) );
    CONNECT( this, valueReallyChanged( int ), this, userUpdateVolume( int ) );
    CONNECT( THEMIM, volumeChanged( void ), this, libUpdateVolume( void ) );
    CONNECT( THEMIM, soundMuteChanged( void ), this, updateMuteStatus( void ) );
}

SoundWidget::~SoundWidget()
{
    delete volumeSlider;
    delete volumeControlWidget;
}

void SoundWidget::refreshLabels()
{
    int i_sliderVolume = volumeSlider->value();

    if( b_is_muted )
    {
        volMuteLabel->setPixmap( QPixmap(":/toolbar/volume-muted" ) );
        volMuteLabel->setToolTip(qfu(vlc_pgettext("Tooltip|Unmute", "Unmute")));
        return;
    }

    if( i_sliderVolume < VOLUME_MAX / 3 )
        volMuteLabel->setPixmap( QPixmap( ":/toolbar/volume-low" ) );
    else if( i_sliderVolume > (VOLUME_MAX * 2 / 3 ) )
        volMuteLabel->setPixmap( QPixmap( ":/toolbar/volume-high" ) );
    else volMuteLabel->setPixmap( QPixmap( ":/toolbar/volume-medium" ) );
    volMuteLabel->setToolTip( qfu(vlc_pgettext("Tooltip|Mute", "Mute")) );
}

/* volumeSlider changed value event slot */
void SoundWidget::userUpdateVolume( int i_sliderVolume )
{
    /* Only if volume is set by user action on slider */
    setMuted( false );
    playlist_t *p_playlist = pl_Get( p_intf );
    int i_res = i_sliderVolume * (AOUT_VOLUME_DEFAULT * 2) / VOLUME_MAX;
    aout_VolumeSet( p_playlist, i_res );
    refreshLabels();
}

/* libvlc changed value event slot */
void SoundWidget::libUpdateVolume()
{
    /* Audio part */
    audio_volume_t i_volume;
    playlist_t *p_playlist = pl_Get( p_intf );

    i_volume = aout_VolumeGet( p_playlist );
    i_volume = (i_volume * VOLUME_MAX ) / (AOUT_VOLUME_DEFAULT * 2);

    if ( i_volume - volumeSlider->value() != 0 )
    {
        b_ignore_valuechanged = true;
        volumeSlider->setValue( i_volume );
        b_ignore_valuechanged = false;
    }
    refreshLabels();
}

void SoundWidget::valueChangedFilter( int i_val )
{
    /* valueChanged is also emitted when the lib setValue() */
    if ( !b_ignore_valuechanged ) emit valueReallyChanged( i_val );
}

/* libvlc mute/unmute event slot */
void SoundWidget::updateMuteStatus()
{
    playlist_t *p_playlist = pl_Get( p_intf );
    b_is_muted = aout_IsMuted( VLC_OBJECT(p_playlist) );

    SoundSlider *soundSlider = qobject_cast<SoundSlider *>(volumeSlider);
    if( soundSlider )
        soundSlider->setMuted( b_is_muted );
    refreshLabels();
}

void SoundWidget::showVolumeMenu( QPoint pos )
{
    volumeMenu->setFixedHeight( volumeMenu->sizeHint().height() );
    volumeMenu->exec( QCursor::pos() - pos - QPoint( 0, volumeMenu->height()/2 )
                          + QPoint( width(), height() /2) );
}

void SoundWidget::setMuted( bool mute )
{
    b_is_muted = mute;
    playlist_t *p_playlist = pl_Get( p_intf );
    aout_SetMute( VLC_OBJECT(p_playlist), NULL, mute );
}

bool SoundWidget::eventFilter( QObject *obj, QEvent *e )
{
    VLC_UNUSED( obj );
    if( e->type() == QEvent::MouseButtonPress )
    {
        QMouseEvent *event = static_cast<QMouseEvent*>(e);
        if( event->button() != Qt::RightButton )
        {
            if( volumeSlider->orientation() ==  Qt::Vertical )
            {
                showVolumeMenu( event->pos() );
            }
            else
            {
                setMuted( !b_is_muted );
            }
            e->accept();
            return true;
        }
    }
    e->ignore();
    return false;
}

/**
 * Play Button
 **/
void PlayButton::updateButtonIcons( bool b_playing )
{
    setIcon( b_playing ? QIcon( ":/toolbar/pause_b" ) : QIcon( ":/toolbar/play_b" ) );
    setToolTip( b_playing ? qtr( "Pause the playback" )
                          : qtr( I_PLAY_TOOLTIP ) );
}

void AtoB_Button::updateButtonIcons( bool timeA, bool timeB )
{
    if( !timeA && !timeB)
    {
        setIcon( QIcon( ":/toolbar/atob_nob" ) );
        setToolTip( qtr( "Loop from point A to point B continuously\n"
                         "Click to set point A" ) );
    }
    else if( timeA && !timeB )
    {
        setIcon( QIcon( ":/toolbar/atob_noa" ) );
        setToolTip( qtr( "Click to set point B" ) );
    }
    else if( timeA && timeB )
    {
        setIcon( QIcon( ":/toolbar/atob" ) );
        setToolTip( qtr( "Stop the A to B loop" ) );
    }
}

void LoopButton::updateButtonIcons( int value )
{
    setChecked( value != NORMAL );
    setIcon( ( value == REPEAT_ONE ) ? QIcon( ":/buttons/playlist/repeat_one" )
                                     : QIcon( ":/buttons/playlist/repeat_all" ) );
}

void AspectRatioComboBox::updateRatios()
{
    /* Clear the list before updating */
    clear();
    vlc_value_t val_list, text_list;
    vout_thread_t* p_vout = THEMIM->getVout();

    /* Disable if there is no vout */
    if( p_vout == NULL )
    {
        addItem( qtr("Aspect Ratio") );
        setDisabled( true );
        return;
    }

    var_Change( p_vout, "aspect-ratio", VLC_VAR_GETLIST, &val_list, &text_list );
    for( int i = 0; i < val_list.p_list->i_count; i++ )
        addItem( qfu( text_list.p_list->p_values[i].psz_string ),
                 QString( val_list.p_list->p_values[i].psz_string ) );
    setEnabled( true );
    var_FreeList( &val_list, &text_list );
    vlc_object_release( p_vout );
}

void AspectRatioComboBox::updateAspectRatio( int x )
{
    vout_thread_t* p_vout = THEMIM->getVout();
    if( p_vout && x >= 0 )
    {
        var_SetString( p_vout, "aspect-ratio", qtu( itemData(x).toString() ) );
        vlc_object_release( p_vout );
    }
}

