/*****************************************************************************
 * Controller.cpp : Controller for the main interface
 ****************************************************************************
 * Copyright (C) 2006-2009 the VideoLAN team
 * $Id: c995f19c7c9dbaa014fdf7705f4c2a9d034d2acd $
 *
 * Authors: Jean-Baptiste Kempf <jb@videolan.org>
 *          Ilkka Ollakka <ileoo@videolan.org>
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

#include <vlc_vout.h>
#include <vlc_keys.h>

#include "components/controller.hpp"
#include "components/controller_widget.hpp"
#include "components/interface_widgets.hpp"

#include "dialogs_provider.hpp" /* Opening Dialogs */
#include "input_manager.hpp"
#include "actions_manager.hpp"

#include "util/input_slider.hpp" /* InputSlider */
#include "util/customwidgets.hpp" /* qEventToKey */

#include <QSpacerItem>
#include <QToolButton>
#include <QHBoxLayout>
#include <QSignalMapper>
#include <QTimer>

//#define DEBUG_LAYOUT 1

/**********************************************************************
 * TEH controls
 **********************************************************************/

/******
 * This is an abstract Toolbar/Controller
 * This has helper to create any toolbar, any buttons and to manage the actions
 *
 *****/
AbstractController::AbstractController( intf_thread_t * _p_i, QWidget *_parent )
                   : QFrame( _parent )
{
    p_intf = _p_i;
    advControls = NULL;

    /* Main action provider */
    toolbarActionsMapper = new QSignalMapper( this );
    CONNECT( toolbarActionsMapper, mapped( int ),
             ActionsManager::getInstance( p_intf  ), doAction( int ) );
    CONNECT( THEMIM->getIM(), statusChanged( int ), this, setStatus( int ) );

    setSizePolicy( QSizePolicy::MinimumExpanding, QSizePolicy::Fixed );
}

/* Reemit some signals on status Change to activate some buttons */
void AbstractController::setStatus( int status )
{
    bool b_hasInput = THEMIM->getIM()->hasInput();
    /* Activate the interface buttons according to the presence of the input */
    emit inputExists( b_hasInput );

    emit inputPlaying( status == PLAYING_S );

    emit inputIsRecordable( b_hasInput &&
                            var_GetBool( THEMIM->getInput(), "can-record" ) );

    emit inputIsTrickPlayable( b_hasInput &&
                            var_GetBool( THEMIM->getInput(), "can-rewind" ) );
}

/* Generic button setup */
void AbstractController::setupButton( QAbstractButton *aButton )
{
    static QSizePolicy sizePolicy( QSizePolicy::Fixed, QSizePolicy::Fixed );
    sizePolicy.setHorizontalStretch( 0 );
    sizePolicy.setVerticalStretch( 0 );

    aButton->setSizePolicy( sizePolicy );
    aButton->setFixedSize( QSize( 26, 26 ) );
    aButton->setIconSize( QSize( 20, 20 ) );
    aButton->setFocusPolicy( Qt::NoFocus );
}

/* Open the generic config line for the toolbar, parse it
 * and create the widgets accordingly */
void AbstractController::parseAndCreate( const QString& config,
                                         QBoxLayout *controlLayout )
{
    QStringList list = config.split( ";", QString::SkipEmptyParts ) ;
    for( int i = 0; i < list.size(); i++ )
    {
        QStringList list2 = list.at( i ).split( "-" );
        if( list2.size() < 1 )
        {
            msg_Warn( p_intf, "Parsing error. Report this" );
            continue;
        }

        bool ok;
        int i_option = WIDGET_NORMAL;
        buttonType_e i_type = (buttonType_e)list2.at( 0 ).toInt( &ok );
        if( !ok )
        {
            msg_Warn( p_intf, "Parsing error 0. Please report this" );
            continue;
        }

        if( list2.size() > 1 )
        {
            i_option = list2.at( 1 ).toInt( &ok );
            if( !ok )
            {
                msg_Warn( p_intf, "Parsing error 1. Please report this" );
                continue;
            }
        }

        createAndAddWidget( controlLayout, -1, i_type, i_option );
    }
}

void AbstractController::createAndAddWidget( QBoxLayout *controlLayout,
                                             int i_index,
                                             buttonType_e i_type,
                                             int i_option )
{
    /* Special case for SPACERS, who aren't QWidgets */
    if( i_type == WIDGET_SPACER )
    {
        controlLayout->insertSpacing( i_index, 12 );
        return;
    }

    if(  i_type == WIDGET_SPACER_EXTEND )
    {
        controlLayout->insertStretch( i_index, 12 );
        return;
    }

    QWidget *widg = createWidget( i_type, i_option );
    if( !widg ) return;

    controlLayout->insertWidget( i_index, widg );
}


#define CONNECT_MAP( a ) CONNECT( a, clicked(),  toolbarActionsMapper, map() )
#define SET_MAPPING( a, b ) toolbarActionsMapper->setMapping( a , b )
#define CONNECT_MAP_SET( a, b ) \
    CONNECT_MAP( a ); \
    SET_MAPPING( a, b );
#define BUTTON_SET_BAR( a_button ) \
    a_button->setToolTip( qtr( tooltipL[button] ) ); \
    a_button->setIcon( QIcon( iconL[button] ) );
#define BUTTON_SET_BAR2( button, image, tooltip ) \
    button->setToolTip( tooltip );          \
    button->setIcon( QIcon( ":/"#image ) );

#define ENABLE_ON_VIDEO( a ) \
    CONNECT( THEMIM->getIM(), voutChanged( bool ), a, setEnabled( bool ) ); \
    a->setEnabled( THEMIM->getIM()->hasVideo() ); /* TODO: is this necessary? when input is started before the interface? */

#define ENABLE_ON_INPUT( a ) \
    CONNECT( this, inputExists( bool ), a, setEnabled( bool ) ); \
    a->setEnabled( THEMIM->getIM()->hasInput() ); /* TODO: is this necessary? when input is started before the interface? */

#define NORMAL_BUTTON( name )                           \
    QToolButton * name ## Button = new QToolButton;     \
    setupButton( name ## Button );                      \
    CONNECT_MAP_SET( name ## Button, name ## _ACTION ); \
    BUTTON_SET_BAR( name ## Button );                   \
    widget = name ## Button;

QWidget *AbstractController::createWidget( buttonType_e button, int options )
{

    bool b_flat  = options & WIDGET_FLAT;
    bool b_big   = options & WIDGET_BIG;
    bool b_shiny = options & WIDGET_SHINY;
    bool b_special = false;

    QWidget *widget = NULL;
    switch( button )
    {
    case PLAY_BUTTON: {
        PlayButton *playButton = new PlayButton;
        setupButton( playButton );
        BUTTON_SET_BAR(  playButton );
        CONNECT_MAP_SET( playButton, PLAY_ACTION );
        CONNECT( this, inputPlaying( bool ),
                 playButton, updateButton( bool ));
        widget = playButton;
        }
        break;
    case STOP_BUTTON:{
        NORMAL_BUTTON( STOP );
        }
        break;
    case OPEN_BUTTON:{
        NORMAL_BUTTON( OPEN );
        }
        break;
    case PREVIOUS_BUTTON:{
        NORMAL_BUTTON( PREVIOUS );
        }
        break;
    case NEXT_BUTTON: {
        NORMAL_BUTTON( NEXT );
        }
        break;
    case SLOWER_BUTTON:{
        NORMAL_BUTTON( SLOWER );
        ENABLE_ON_INPUT( SLOWERButton );
        }
        break;
    case FASTER_BUTTON:{
        NORMAL_BUTTON( FASTER );
        ENABLE_ON_INPUT( FASTERButton );
        }
        break;
    case FRAME_BUTTON: {
        NORMAL_BUTTON( FRAME );
        ENABLE_ON_VIDEO( FRAMEButton );
        }
        break;
    case FULLSCREEN_BUTTON:
    case DEFULLSCREEN_BUTTON:
        {
        NORMAL_BUTTON( FULLSCREEN );
        ENABLE_ON_VIDEO( FULLSCREENButton );
        }
        break;
    case EXTENDED_BUTTON:{
        NORMAL_BUTTON( EXTENDED );
        }
        break;
    case PLAYLIST_BUTTON:{
        NORMAL_BUTTON( PLAYLIST );
        }
        break;
    case SNAPSHOT_BUTTON:{
        NORMAL_BUTTON( SNAPSHOT );
        ENABLE_ON_VIDEO( SNAPSHOTButton );
        }
        break;
    case RECORD_BUTTON:{
        QToolButton *recordButton = new QToolButton;
        setupButton( recordButton );
        CONNECT_MAP_SET( recordButton, RECORD_ACTION );
        BUTTON_SET_BAR(  recordButton );
        ENABLE_ON_INPUT( recordButton );
        recordButton->setCheckable( true );
        CONNECT( THEMIM->getIM(), recordingStateChanged( bool ),
                 recordButton, setChecked( bool ) );
        widget = recordButton;
        }
        break;
    case ATOB_BUTTON: {
        AtoB_Button *ABButton = new AtoB_Button;
        setupButton( ABButton );
        ABButton->setShortcut( qtr("Shift+L") );
        BUTTON_SET_BAR( ABButton );
        ENABLE_ON_INPUT( ABButton );
        CONNECT_MAP_SET( ABButton, ATOB_ACTION );
        CONNECT( THEMIM->getIM(), AtoBchanged( bool, bool),
                 ABButton, setIcons( bool, bool ) );
        widget = ABButton;
        }
        break;
    case INPUT_SLIDER: {
        InputSlider *slider = new InputSlider( Qt::Horizontal, NULL );

        /* Update the position when the IM has changed */
        CONNECT( THEMIM->getIM(), positionUpdated( float, int64_t, int ),
                slider, setPosition( float, int64_t, int ) );
        /* And update the IM, when the position has changed */
        CONNECT( slider, sliderDragged( float ),
                 THEMIM->getIM(), sliderUpdate( float ) );
        widget = slider;
        }
        break;
    case MENU_BUTTONS:
        widget = discFrame();
        widget->hide();
        break;
    case TELETEXT_BUTTONS:
        widget = telexFrame();
        widget->hide();
        break;
    case VOLUME_SPECIAL:
        b_special = true;
    case VOLUME:
        {
            SoundWidget *snd = new SoundWidget( this, p_intf, b_shiny, b_special );
            widget = snd;
        }
        break;
    case TIME_LABEL:
        {
            TimeLabel *timeLabel = new TimeLabel( p_intf );
            widget = timeLabel;
        }
        break;
    case SPLITTER:
        {
            QFrame *line = new QFrame;
            line->setFrameShape( QFrame::VLine );
            line->setFrameShadow( QFrame::Raised );
            line->setLineWidth( 0 );
            line->setMidLineWidth( 1 );
            widget = line;
        }
        break;
    case ADVANCED_CONTROLLER:
        {
            advControls = new AdvControlsWidget( p_intf, this );
            widget = advControls;
        }
        break;
    case REVERSE_BUTTON:{
        QToolButton *reverseButton = new QToolButton;
        setupButton( reverseButton );
        CONNECT_MAP_SET( reverseButton, REVERSE_ACTION );
        BUTTON_SET_BAR(  reverseButton );
        reverseButton->setCheckable( true );
        /* You should, of COURSE change this to the correct event,
           when/if we have one, that tells us if trickplay is possible . */
        CONNECT( this, inputIsTrickPlayable( bool ), reverseButton, setVisible( bool ) );
        reverseButton->setVisible( false );
        widget = reverseButton;
        }
        break;
    case SKIP_BACK_BUTTON: {
        NORMAL_BUTTON( SKIP_BACK );
        ENABLE_ON_INPUT( SKIP_BACKButton );
        }
        break;
    case SKIP_FW_BUTTON: {
        NORMAL_BUTTON( SKIP_FW );
        ENABLE_ON_INPUT( SKIP_FWButton );
        }
        break;
    case QUIT_BUTTON: {
        NORMAL_BUTTON( QUIT );
        }
        break;
    case RANDOM_BUTTON: {
        NORMAL_BUTTON( RANDOM );
        RANDOMButton->setCheckable( true );
        RANDOMButton->setChecked( var_GetBool( THEPL, "random" ) );
        CONNECT( THEMIM, randomChanged( bool ),
                 RANDOMButton, setChecked( bool ) );
        }
        break;
    case LOOP_BUTTON:{
        LoopButton *loopButton = new LoopButton;
        setupButton( loopButton );
        loopButton->setToolTip( qtr( "Click to toggle between loop one, loop all" ) );
        loopButton->setCheckable( true );
        loopButton->updateIcons( NORMAL );
        CONNECT( THEMIM, repeatLoopChanged( int ), loopButton, updateIcons( int ) );
        CONNECT( loopButton, clicked(), THEMIM, loopRepeatLoopStatus() );
        widget = loopButton;
        }
        break;
    case INFO_BUTTON: {
        NORMAL_BUTTON( INFO );
        }
        break;
    default:
        msg_Warn( p_intf, "This should not happen %i", button );
        break;
    }

    /* Customize Buttons */
    if( b_flat || b_big )
    {
        QFrame *frame = qobject_cast<QFrame *>(widget);
        if( frame )
        {
            QList<QToolButton *> allTButtons = frame->findChildren<QToolButton *>();
            for( int i = 0; i < allTButtons.size(); i++ )
                applyAttributes( allTButtons[i], b_flat, b_big );
        }
        else
        {
            QToolButton *tmpButton = qobject_cast<QToolButton *>(widget);
            if( tmpButton )
                applyAttributes( tmpButton, b_flat, b_big );
        }
    }
    return widget;
}
#undef NORMAL_BUTTON

void AbstractController::applyAttributes( QToolButton *tmpButton, bool b_flat, bool b_big )
{
    if( tmpButton )
    {
        if( b_flat )
            tmpButton->setAutoRaise( b_flat );
        if( b_big )
        {
            tmpButton->setFixedSize( QSize( 32, 32 ) );
            tmpButton->setIconSize( QSize( 26, 26 ) );
        }
    }
}

QFrame *AbstractController::discFrame()
{
    /** Disc and Menus handling */
    QFrame *discFrame = new QFrame( this );

    QHBoxLayout *discLayout = new QHBoxLayout( discFrame );
    discLayout->setSpacing( 0 ); discLayout->setMargin( 0 );

    QToolButton *prevSectionButton = new QToolButton( discFrame );
    setupButton( prevSectionButton );
    BUTTON_SET_BAR2( prevSectionButton, toolbar/dvd_prev,
            qtr("Previous Chapter/Title" ) );
    discLayout->addWidget( prevSectionButton );

    QToolButton *menuButton = new QToolButton( discFrame );
    setupButton( menuButton );
    discLayout->addWidget( menuButton );
    BUTTON_SET_BAR2( menuButton, toolbar/dvd_menu, qtr( "Menu" ) );

    QToolButton *nextSectionButton = new QToolButton( discFrame );
    setupButton( nextSectionButton );
    discLayout->addWidget( nextSectionButton );
    BUTTON_SET_BAR2( nextSectionButton, toolbar/dvd_next,
            qtr("Next Chapter/Title" ) );

    /* Change the navigation button display when the IM
       navigation changes */
    CONNECT( THEMIM->getIM(), titleChanged( bool ),
            discFrame, setVisible( bool ) );
    CONNECT( THEMIM->getIM(), chapterChanged( bool ),
            menuButton, setVisible( bool ) );
    /* Changes the IM navigation when triggered on the nav buttons */
    CONNECT( prevSectionButton, clicked(), THEMIM->getIM(),
            sectionPrev() );
    CONNECT( nextSectionButton, clicked(), THEMIM->getIM(),
            sectionNext() );
    CONNECT( menuButton, clicked(), THEMIM->getIM(),
            sectionMenu() );

    return discFrame;
}

QFrame *AbstractController::telexFrame()
{
    /**
     * Telextext QFrame
     **/
    QFrame *telexFrame = new QFrame( this );
    QHBoxLayout *telexLayout = new QHBoxLayout( telexFrame );
    telexLayout->setSpacing( 0 ); telexLayout->setMargin( 0 );
    CONNECT( THEMIM->getIM(), teletextPossible( bool ),
             telexFrame, setVisible( bool ) );

    /* On/Off button */
    QToolButton *telexOn = new QToolButton;
    setupButton( telexOn );
    BUTTON_SET_BAR2( telexOn, toolbar/tv, qtr( "Teletext Activation" ) );
    telexOn->setEnabled( false );
    telexOn->setCheckable( true );

    telexLayout->addWidget( telexOn );

    /* Teletext Activation and set */
    CONNECT( telexOn, clicked( bool ),
             THEMIM->getIM(), activateTeletext( bool ) );
    CONNECT( THEMIM->getIM(), teletextPossible( bool ),
             telexOn, setEnabled( bool ) );

    /* Transparency button */
    QToolButton *telexTransparent = new QToolButton;
    setupButton( telexTransparent );
    BUTTON_SET_BAR2( telexTransparent, toolbar/tvtelx,
                     qtr( "Toggle Transparency " ) );
    telexTransparent->setEnabled( false );
    telexTransparent->setCheckable( true );
    telexLayout->addWidget( telexTransparent );

    /* Transparency change and set */
    CONNECT( telexTransparent, clicked( bool ),
            THEMIM->getIM(), telexSetTransparency( bool ) );
    CONNECT( THEMIM->getIM(), teletextTransparencyActivated( bool ),
             telexTransparent, setChecked( bool ) );


    /* Page setting */
    QSpinBox *telexPage = new QSpinBox( telexFrame );
    telexPage->setRange( 0, 999 );
    telexPage->setValue( 100 );
    telexPage->setAccelerated( true );
    telexPage->setWrapping( true );
    telexPage->setAlignment( Qt::AlignRight );
    telexPage->setSizePolicy( QSizePolicy::Preferred, QSizePolicy::Minimum );
    telexPage->setEnabled( false );
    telexLayout->addWidget( telexPage );

    /* Page change and set */
    CONNECT( telexPage, valueChanged( int ),
            THEMIM->getIM(), telexSetPage( int ) );
    CONNECT( THEMIM->getIM(), newTelexPageSet( int ),
            telexPage, setValue( int ) );

    CONNECT( THEMIM->getIM(), teletextActivated( bool ), telexPage, setEnabled( bool ) );
    CONNECT( THEMIM->getIM(), teletextActivated( bool ), telexTransparent, setEnabled( bool ) );
    CONNECT( THEMIM->getIM(), teletextActivated( bool ), telexOn, setChecked( bool ) );
    return telexFrame;
}
#undef CONNECT_MAP
#undef SET_MAPPING
#undef CONNECT_MAP_SET
#undef BUTTON_SET_BAR
#undef BUTTON_SET_BAR2
#undef ENABLE_ON_VIDEO
#undef ENABLE_ON_INPUT

#include <QHBoxLayout>
/*****************************
 * DA Control Widget !
 *****************************/
ControlsWidget::ControlsWidget( intf_thread_t *_p_i,
                                bool b_advControls,
                                QWidget *_parent ) :
                                AbstractController( _p_i, _parent )
{
    /* advanced Controls handling */
    b_advancedVisible = b_advControls;
#if DEBUG_LAYOUT
    setStyleSheet( " background: red ");
#endif

    QVBoxLayout *controlLayout = new QVBoxLayout( this );
    controlLayout->setContentsMargins( 4, 1, 4, 0 );
    controlLayout->setSpacing( 0 );
    QHBoxLayout *controlLayout1 = new QHBoxLayout;
    controlLayout1->setSpacing( 0 ); controlLayout1->setMargin( 0 );

    QString line1 = getSettings()->value( "MainToolbar1", MAIN_TB1_DEFAULT )
                                        .toString();
    parseAndCreate( line1, controlLayout1 );

    QHBoxLayout *controlLayout2 = new QHBoxLayout;
    controlLayout2->setSpacing( 0 ); controlLayout2->setMargin( 0 );
    QString line2 = getSettings()->value( "MainToolbar2", MAIN_TB2_DEFAULT )
                                        .toString();
    parseAndCreate( line2, controlLayout2 );

    if( !b_advancedVisible && advControls ) advControls->hide();

    controlLayout->addLayout( controlLayout1 );
    controlLayout->addLayout( controlLayout2 );
}

ControlsWidget::~ControlsWidget()
{}

void ControlsWidget::toggleAdvanced()
{
    if( !advControls ) return;

    if( !b_advancedVisible )
    {
        advControls->show();
        b_advancedVisible = true;
    }
    else
    {
        advControls->hide();
        b_advancedVisible = false;
    }
    emit advancedControlsToggled( b_advancedVisible );
}

AdvControlsWidget::AdvControlsWidget( intf_thread_t *_p_i, QWidget *_parent ) :
                                     AbstractController( _p_i, _parent )
{
    controlLayout = new QHBoxLayout( this );
    controlLayout->setMargin( 0 );
    controlLayout->setSpacing( 0 );
#if DEBUG_LAYOUT
    setStyleSheet( " background: orange ");
#endif


    QString line = getSettings()->value( "AdvToolbar", ADV_TB_DEFAULT )
        .toString();
    parseAndCreate( line, controlLayout );
}

InputControlsWidget::InputControlsWidget( intf_thread_t *_p_i, QWidget *_parent ) :
                                     AbstractController( _p_i, _parent )
{
    controlLayout = new QHBoxLayout( this );
    controlLayout->setMargin( 0 );
    controlLayout->setSpacing( 0 );
#if DEBUG_LAYOUT
    setStyleSheet( " background: green ");
#endif

    QString line = getSettings()->value( "InputToolbar", INPT_TB_DEFAULT ).toString();
    parseAndCreate( line, controlLayout );
}
/**********************************************************************
 * Fullscrenn control widget
 **********************************************************************/
FullscreenControllerWidget::FullscreenControllerWidget( intf_thread_t *_p_i, QWidget *_parent )
                           : AbstractController( _p_i, _parent )
{
    i_mouse_last_x      = -1;
    i_mouse_last_y      = -1;
    b_mouse_over        = false;
    i_mouse_last_move_x = -1;
    i_mouse_last_move_y = -1;
#if HAVE_TRANSPARENCY
    b_slow_hide_begin   = false;
    i_slow_hide_timeout = 1;
#endif
    b_fullscreen        = false;
    i_hide_timeout      = 1;
    i_screennumber      = -1;

    vout.clear();

    setWindowFlags( Qt::ToolTip );
    setMinimumWidth( 600 );

    setFrameShape( QFrame::StyledPanel );
    setFrameStyle( QFrame::Sunken );
    setSizePolicy( QSizePolicy::Minimum, QSizePolicy::Minimum );

    QVBoxLayout *controlLayout2 = new QVBoxLayout( this );
    controlLayout2->setContentsMargins( 4, 6, 4, 2 );

    /* First line */
    InputControlsWidget *inputC = new InputControlsWidget( p_intf, this );
    controlLayout2->addWidget( inputC );

    controlLayout = new QHBoxLayout;
    QString line = getSettings()->value( "MainWindow/FSCtoolbar", FSC_TB_DEFAULT ).toString();
    parseAndCreate( line, controlLayout );
    controlLayout2->addLayout( controlLayout );

    /* hiding timer */
    p_hideTimer = new QTimer( this );
    CONNECT( p_hideTimer, timeout(), this, hideFSC() );
    p_hideTimer->setSingleShot( true );

    /* slow hiding timer */
#if HAVE_TRANSPARENCY
    p_slowHideTimer = new QTimer( this );
    CONNECT( p_slowHideTimer, timeout(), this, slowHideFSC() );
#endif

    vlc_mutex_init_recursive( &lock );

    DCONNECT( THEMIM->getIM(), voutListChanged( vout_thread_t **, int ),
              this, setVoutList( vout_thread_t **, int ) );

    /* First Move */
    QRect rect1 = getSettings()->value( "FullScreen/screen" ).toRect();
    QPoint pos1 = getSettings()->value( "FullScreen/pos" ).toPoint();
    int number =  var_InheritInteger( p_intf, "qt-fullscreen-screennumber" );
    if( number == -1 || number > QApplication::desktop()->numScreens() )
        number = QApplication::desktop()->screenNumber( p_intf->p_sys->p_mi );

    QRect rect = QApplication::desktop()->screenGeometry( number );
    if( rect == rect1 && rect.contains( pos1, true ) )
    {
        move( pos1 );
        i_screennumber = number;
        screenRes = QApplication::desktop()->screenGeometry(number);
    }
    else
    {
        centerFSC( number );
    }

}

FullscreenControllerWidget::~FullscreenControllerWidget()
{
    QPoint pos1 = pos();
    QRect rect1 = QApplication::desktop()->screenGeometry( pos1 );
    getSettings()->setValue( "FullScreen/pos", pos1 );
    getSettings()->setValue( "FullScreen/screen", rect1 );

    setVoutList( NULL, 0 );
    vlc_mutex_destroy( &lock );
}

void FullscreenControllerWidget::centerFSC( int number )
{
    screenRes = QApplication::desktop()->screenGeometry(number);

    /* screen has changed, calculate new position */
    QPoint pos = QPoint( screenRes.x() + (screenRes.width() / 2) - (sizeHint().width() / 2),
            screenRes.y() + screenRes.height() - sizeHint().height());
    move( pos );

    i_screennumber = number;
}

/**
 * Show fullscreen controller
 */
void FullscreenControllerWidget::showFSC()
{
    adjustSize();

    int number = QApplication::desktop()->screenNumber( p_intf->p_sys->p_mi );

    if( number != i_screennumber ||
        screenRes != QApplication::desktop()->screenGeometry(number) )
    {
        centerFSC( number );
        msg_Dbg( p_intf, "Recentering the Fullscreen Controller" );
    }

#if HAVE_TRANSPARENCY
    setWindowOpacity( var_InheritFloat( p_intf, "qt-fs-opacity" )  );
#endif

    show();
}

/**
 * Plane to hide fullscreen controller
 */
void FullscreenControllerWidget::planHideFSC()
{
    vlc_mutex_lock( &lock );
    int i_timeout = i_hide_timeout;
    vlc_mutex_unlock( &lock );

    p_hideTimer->start( i_timeout );

#if HAVE_TRANSPARENCY
    b_slow_hide_begin = true;
    i_slow_hide_timeout = i_timeout;
    p_slowHideTimer->start( i_slow_hide_timeout / 2 );
#endif
}

/**
 * Hidding fullscreen controller slowly
 * Linux: need composite manager
 * Windows: it is blinking, so it can be enabled by define TRASPARENCY
 */
void FullscreenControllerWidget::slowHideFSC()
{
#if HAVE_TRANSPARENCY
    if( b_slow_hide_begin )
    {
        b_slow_hide_begin = false;

        p_slowHideTimer->stop();
        /* the last part of time divided to 100 pieces */
        p_slowHideTimer->start( (int)( i_slow_hide_timeout / 2 / ( windowOpacity() * 100 ) ) );

    }
    else
    {
         if ( windowOpacity() > 0.0 )
         {
             /* we should use 0.01 because of 100 pieces ^^^
                but than it cannt be done in time */
             setWindowOpacity( windowOpacity() - 0.02 );
         }

         if ( windowOpacity() <= 0.0 )
             p_slowHideTimer->stop();
    }
#endif
}

/**
 * event handling
 * events: show, hide, start timer for hidding
 */
void FullscreenControllerWidget::customEvent( QEvent *event )
{
    bool b_fs;

    switch( event->type() )
    {
        /* This is used when the 'i' hotkey is used, to force quick toggle */
        case FullscreenControlToggle_Type:
            vlc_mutex_lock( &lock );
            b_fs = b_fullscreen;
            vlc_mutex_unlock( &lock );

            if( b_fs )
            {
                if( isHidden() )
                {
                    p_hideTimer->stop();
                    showFSC();
                }
                else
                    hideFSC();
            }
            break;
        /* Event called to Show the FSC on mouseChanged() */
        case FullscreenControlShow_Type:
            vlc_mutex_lock( &lock );
            b_fs = b_fullscreen;
            vlc_mutex_unlock( &lock );

            if( b_fs )
                showFSC();

            break;
        /* Start the timer to hide later, called usually with above case */
        case FullscreenControlPlanHide_Type:
            if( !b_mouse_over ) // Only if the mouse is not over FSC
                planHideFSC();
            break;
        /* Hide */
        case FullscreenControlHide_Type:
            hideFSC();
            break;
        default:
            break;
    }
}

/**
 * On mouse move
 * moving with FSC
 */
void FullscreenControllerWidget::mouseMoveEvent( QMouseEvent *event )
{
    if( event->buttons() == Qt::LeftButton )
    {
        if( i_mouse_last_x == -1 || i_mouse_last_y == -1 )
            return;

        int i_moveX = event->globalX() - i_mouse_last_x;
        int i_moveY = event->globalY() - i_mouse_last_y;

        move( x() + i_moveX, y() + i_moveY );

        i_mouse_last_x = event->globalX();
        i_mouse_last_y = event->globalY();
    }
}

/**
 * On mouse press
 * store position of cursor
 */
void FullscreenControllerWidget::mousePressEvent( QMouseEvent *event )
{
    i_mouse_last_x = event->globalX();
    i_mouse_last_y = event->globalY();
    event->accept();
}

void FullscreenControllerWidget::mouseReleaseEvent( QMouseEvent *event )
{
    i_mouse_last_x = -1;
    i_mouse_last_y = -1;
    event->accept();
}

/**
 * On mouse go above FSC
 */
void FullscreenControllerWidget::enterEvent( QEvent *event )
{
    b_mouse_over = true;

    p_hideTimer->stop();
#if HAVE_TRANSPARENCY
    p_slowHideTimer->stop();
    setWindowOpacity( DEFAULT_OPACITY );
#endif
    event->accept();
}

/**
 * On mouse go out from FSC
 */
void FullscreenControllerWidget::leaveEvent( QEvent *event )
{
    planHideFSC();

    b_mouse_over = false;
    event->accept();
}

/**
 * When you get pressed key, send it to video output
 */
void FullscreenControllerWidget::keyPressEvent( QKeyEvent *event )
{
    emit keyPressed( event );
}

/* */
static int FullscreenControllerWidgetFullscreenChanged( vlc_object_t *vlc_object,
                const char *variable, vlc_value_t old_val,
                vlc_value_t new_val,  void *data )
{
    vout_thread_t *p_vout = (vout_thread_t *) vlc_object;

    msg_Dbg( p_vout, "Qt4: Fullscreen state changed" );
    FullscreenControllerWidget *p_fs = (FullscreenControllerWidget *)data;

    p_fs->fullscreenChanged( p_vout, new_val.b_bool, var_GetInteger( p_vout, "mouse-hide-timeout" ) );

    return VLC_SUCCESS;
}
/* */
static int FullscreenControllerWidgetMouseMoved( vlc_object_t *vlc_object, const char *variable,
                                                 vlc_value_t old_val, vlc_value_t new_val,
                                                 void *data )
{
    vout_thread_t *p_vout = (vout_thread_t *)vlc_object;
    FullscreenControllerWidget *p_fs = (FullscreenControllerWidget *)data;

    /* Get the value from the Vout - Trust the vout more than Qt */
    p_fs->mouseChanged( p_vout, new_val.coords.x, new_val.coords.y );

    return VLC_SUCCESS;
}

/**
 * It is call to update the list of vout handled by the fullscreen controller
 */
void FullscreenControllerWidget::setVoutList( vout_thread_t **pp_vout, int i_vout )
{
    QList<vout_thread_t*> del;
    QList<vout_thread_t*> add;

    QList<vout_thread_t*> set;

    /* */
    for( int i = 0; i < i_vout; i++ )
        set += pp_vout[i];

    /* Vout to remove */
    vlc_mutex_lock( &lock );
    foreach( vout_thread_t *p_vout, vout )
    {
        if( !set.contains( p_vout ) )
            del += p_vout;
    }
    vlc_mutex_unlock( &lock );

    foreach( vout_thread_t *p_vout, del )
    {
        var_DelCallback( p_vout, "fullscreen",
                         FullscreenControllerWidgetFullscreenChanged, this );
        vlc_mutex_lock( &lock );
        fullscreenChanged( p_vout, false, 0 );
        vout.removeAll( p_vout );
        vlc_mutex_unlock( &lock );

        vlc_object_release( VLC_OBJECT(p_vout) );
    }

    /* Vout to track */
    vlc_mutex_lock( &lock );
    foreach( vout_thread_t *p_vout, set )
    {
        if( !vout.contains( p_vout ) )
            add += p_vout;
    }
    vlc_mutex_unlock( &lock );

    foreach( vout_thread_t *p_vout, add )
    {
        vlc_object_hold( VLC_OBJECT(p_vout) );

        vlc_mutex_lock( &lock );
        vout.append( p_vout );
        var_AddCallback( p_vout, "fullscreen",
                         FullscreenControllerWidgetFullscreenChanged, this );
        /* I miss a add and fire */
        fullscreenChanged( p_vout, var_GetBool( p_vout, "fullscreen" ),
                           var_GetInteger( p_vout, "mouse-hide-timeout" ) );
        vlc_mutex_unlock( &lock );
    }
}
/**
 * Register and unregister callback for mouse moving
 */
void FullscreenControllerWidget::fullscreenChanged( vout_thread_t *p_vout,
        bool b_fs, int i_timeout )
{
    /* FIXME - multiple vout (ie multiple mouse position ?) and thread safety if multiple vout ? */

    vlc_mutex_lock( &lock );
    /* Entering fullscreen, register callback */
    if( b_fs && !b_fullscreen )
    {
        msg_Dbg( p_vout, "Qt: Entering Fullscreen" );
        b_fullscreen = true;
        i_hide_timeout = i_timeout;
        var_AddCallback( p_vout, "mouse-moved",
                FullscreenControllerWidgetMouseMoved, this );
    }
    /* Quitting fullscreen, unregistering callback */
    else if( !b_fs && b_fullscreen )
    {
        msg_Dbg( p_vout, "Qt: Quitting Fullscreen" );
        b_fullscreen = false;
        i_hide_timeout = i_timeout;
        var_DelCallback( p_vout, "mouse-moved",
                FullscreenControllerWidgetMouseMoved, this );

        /* Force fs hidding */
        IMEvent *eHide = new IMEvent( FullscreenControlHide_Type, 0 );
        QApplication::postEvent( this, eHide );
    }
    vlc_mutex_unlock( &lock );
}

/**
 * Mouse change callback (show/hide the controller on mouse movement)
 */
void FullscreenControllerWidget::mouseChanged( vout_thread_t *p_vout, int i_mousex, int i_mousey )
{
    bool b_toShow;

    /* FIXME - multiple vout (ie multiple mouse position ?) and thread safety if multiple vout ? */

    b_toShow = false;
    if( ( i_mouse_last_move_x == -1 || i_mouse_last_move_y == -1 ) ||
        ( abs( i_mouse_last_move_x - i_mousex ) > 2 ||
          abs( i_mouse_last_move_y - i_mousey ) > 2 ) )
    {
        i_mouse_last_move_x = i_mousex;
        i_mouse_last_move_y = i_mousey;
        b_toShow = true;
    }

    if( b_toShow )
    {
        /* Show event */
        IMEvent *eShow = new IMEvent( FullscreenControlShow_Type, 0 );
        QApplication::postEvent( this, eShow );

        /* Plan hide event */
        IMEvent *eHide = new IMEvent( FullscreenControlPlanHide_Type, 0 );
        QApplication::postEvent( this, eHide );
    }
}

