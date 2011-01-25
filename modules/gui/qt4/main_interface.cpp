/*****************************************************************************
 * main_interface.cpp : Main interface
 ****************************************************************************
 * Copyright (C) 2006-2010 VideoLAN and AUTHORS
 * $Id: a3ad19717753ea5bedc745de3e614a7c67a2ef2a $
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
 *          Jean-Baptiste Kempf <jb@videolan.org>
 *          Ilkka Ollakka <ileoo@videolan.org>
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "qt4.hpp"

#include "main_interface.hpp"
#include "input_manager.hpp"                    // Creation
#include "actions_manager.hpp"                  // killInstance
#include "extensions_manager.hpp"               // killInstance

#include "util/customwidgets.hpp"               // qtEventToVLCKey, QVLCStackedWidget
#include "util/qt_dirs.hpp"                     // toNativeSeparators

#include "components/interface_widgets.hpp"     // bgWidget, videoWidget
#include "components/controller.hpp"            // controllers
#include "components/playlist/playlist.hpp"     // plWidget
#include "dialogs/firstrun.hpp"                 // First Run

#include "menus.hpp"                            // Menu creation
#include "recents.hpp"                          // RecentItems when DnD

#include <QCloseEvent>
#include <QKeyEvent>

#include <QUrl>
#include <QSize>
#include <QDate>

#include <QMenu>
#include <QMenuBar>
#include <QStatusBar>
#include <QLabel>
#include <QStackedWidget>

#include <vlc_keys.h>                       /* Wheel event */
#include <vlc_vout_display.h>               /* vout_thread_t and VOUT_ events */

// #define DEBUG_INTF

/* Callback prototypes */
static int PopupMenuCB( vlc_object_t *p_this, const char *psz_variable,
                        vlc_value_t old_val, vlc_value_t new_val, void *param );
static int IntfShowCB( vlc_object_t *p_this, const char *psz_variable,
                       vlc_value_t old_val, vlc_value_t new_val, void *param );

MainInterface::MainInterface( intf_thread_t *_p_intf ) : QVLCMW( _p_intf )
{
    /* Variables initialisation */
    bgWidget             = NULL;
    videoWidget          = NULL;
    playlistWidget       = NULL;
    stackCentralOldWidget= NULL;
#ifndef HAVE_MAEMO
    sysTray              = NULL;
#endif
    fullscreenControls   = NULL;
    cryptedLabel         = NULL;
    controls             = NULL;
    inputC               = NULL;

    b_hideAfterCreation  = false; // --qt-start-minimized
    playlistVisible      = false;
    input_name           = "";


    /* Ask for Privacy */
    FirstRun::CheckAndRun( this, p_intf );

    /**
     *  Configuration and settings
     *  Pre-building of interface
     **/
    /* Main settings */
    setFocusPolicy( Qt::StrongFocus );
    setAcceptDrops( true );
    setWindowRole( "vlc-main" );
    setWindowIcon( QApplication::windowIcon() );
    setWindowOpacity( var_InheritFloat( p_intf, "qt-opacity" ) );

    /* Is video in embedded in the UI or not */
    b_videoEmbedded = var_InheritBool( p_intf, "embedded-video" );

    /* Does the interface resize to video size or the opposite */
    b_autoresize = var_InheritBool( p_intf, "qt-video-autoresize" );

    /* Are we in the enhanced always-video mode or not ? */
    b_minimalView = var_InheritBool( p_intf, "qt-minimal-view" );

    /* Do we want anoying popups or not */
    b_notificationEnabled = var_InheritBool( p_intf, "qt-notification" );

    /* Set the other interface settings */
    settings = getSettings();
    settings->beginGroup( "MainWindow" );

    /* */
    b_plDocked = getSettings()->value( "pl-dock-status", true ).toBool();

    settings->endGroup( );

    /**************
     * Status Bar *
     **************/
    createStatusBar();

    /**************************
     *  UI and Widgets design
     **************************/
    setVLCWindowsTitle();

    /************
     * Menu Bar *
     ************/
    QVLCMenu::createMenuBar( this, p_intf );
    CONNECT( THEMIM->getIM(), voutListChanged( vout_thread_t **, int ),
             this, destroyPopupMenu() );

    createMainWidget( settings );
    /*********************************
     * Create the Systray Management *
     *********************************/
    initSystray();

    /********************
     * Input Manager    *
     ********************/
    MainInputManager::getInstance( p_intf );

#ifdef WIN32
    himl = NULL;
    p_taskbl = NULL;
    taskbar_wmsg = RegisterWindowMessage("TaskbarButtonCreated");
#endif

    /************************************************************
     * Connect the input manager to the GUI elements it manages *
     ************************************************************/
    /**
     * Connects on nameChanged()
     * Those connects are different because options can impeach them to trigger.
     **/
    /* Main Interface statusbar */
    CONNECT( THEMIM->getIM(), nameChanged( const QString& ),
             this, setName( const QString& ) );
    /* and systray */
#ifndef HAVE_MAEMO
    if( sysTray )
    {
        CONNECT( THEMIM->getIM(), nameChanged( const QString& ),
                 this, updateSystrayTooltipName( const QString& ) );
    }
#endif
    /* and title of the Main Interface*/
    if( var_InheritBool( p_intf, "qt-name-in-title" ) )
    {
        CONNECT( THEMIM->getIM(), nameChanged( const QString& ),
                 this, setVLCWindowsTitle( const QString& ) );
    }

    /**
     * CONNECTS on PLAY_STATUS
     **/
    /* Status on the systray */
#ifndef HAVE_MAEMO
    if( sysTray )
    {
        CONNECT( THEMIM->getIM(), statusChanged( int ),
                 this, updateSystrayTooltipStatus( int ) );
    }
#endif

    /* END CONNECTS ON IM */

    /* VideoWidget connects for asynchronous calls */
    b_videoFullScreen = false;
    b_videoOnTop = false;
    connect( this, SIGNAL(askGetVideo(WId*,int*,int*,unsigned*,unsigned *)),
             this, SLOT(getVideoSlot(WId*,int*,int*,unsigned*,unsigned*)),
             Qt::BlockingQueuedConnection );
    connect( this, SIGNAL(askReleaseVideo( void )),
             this, SLOT(releaseVideoSlot( void )),
             Qt::BlockingQueuedConnection );
    CONNECT( this, askVideoOnTop(bool), this, setVideoOnTop(bool));

    if( videoWidget )
    {
        if( b_autoresize )
        {
            CONNECT( this, askVideoToResize( unsigned int, unsigned int ),
                     this, setVideoSize( unsigned int, unsigned int ) );
            CONNECT( videoWidget, sizeChanged( int, int ),
                     this, resizeStack( int,  int ) );
        }
        CONNECT( this, askVideoSetFullScreen( bool ),
                 this, setVideoFullScreen( bool ) );
    }

    CONNECT( THEDP, toolBarConfUpdated(), this, recreateToolbars() );

    CONNECT( this, askToQuit(), THEDP, quit() );
    /** END of CONNECTS**/


    /************
     * Callbacks
     ************/
    var_AddCallback( p_intf->p_libvlc, "intf-show", IntfShowCB, p_intf );

    /* Register callback for the intf-popupmenu variable */
    var_AddCallback( p_intf->p_libvlc, "intf-popupmenu", PopupMenuCB, p_intf );

    /* Playlist */
    int i_plVis = settings->value( "MainWindow/playlist-visible", false ).toBool();

    if( i_plVis ) togglePlaylist();

    /**** FINAL SIZING and placement of interface */
    settings->beginGroup( "MainWindow" );
    QVLCTools::restoreWidgetPosition( settings, this, QSize(400, 100) );
    settings->endGroup();

    b_interfaceFullScreen = isFullScreen();

    /* Final sizing and showing */
    setVisible( !b_hideAfterCreation );

    setMinimumWidth( __MAX( controls->sizeHint().width(),
                            menuBar()->sizeHint().width() ) + 30 );

    /* Switch to minimal view if needed, must be called after the show() */
    if( b_minimalView )
        toggleMinimalView( true );
}

MainInterface::~MainInterface()
{
    /* Unsure we hide the videoWidget before destroying it */
    if( stackCentralOldWidget == videoWidget )
        showTab( bgWidget );

    if( videoWidget )
        releaseVideoSlot();

#ifdef WIN32
    if( himl )
        ImageList_Destroy( himl );
    if(p_taskbl)
        p_taskbl->vt->Release(p_taskbl);
    CoUninitialize();
#endif

    /* Be sure to kill the actionsManager... Only used in the MI and control */
    ActionsManager::killInstance();

    /* Idem */
    ExtensionsManager::killInstance();

    /* Delete the FSC controller */
    delete fullscreenControls;

    /* Save states */
    settings->beginGroup( "MainWindow" );

    settings->setValue( "pl-dock-status", b_plDocked );
    /* Save playlist state */
    if( playlistWidget )
        settings->setValue( "playlist-visible", playlistVisible );

    settings->setValue( "adv-controls",
                        getControlsVisibilityStatus() & CONTROLS_ADVANCED );

    /* Save the stackCentralW sizes */
    settings->setValue( "bgSize", stackWidgetsSizes[bgWidget] );
    settings->setValue( "playlistSize", stackWidgetsSizes[playlistWidget] );

    /* Save this size */
    QVLCTools::saveWidgetPosition(settings, this);

    settings->endGroup();

    /* Save undocked playlist size */
    if( playlistWidget && !isPlDocked() )
        QVLCTools::saveWidgetPosition( p_intf, "Playlist", playlistWidget );

    delete playlistWidget;

    delete statusBar();

    /* Unregister callbacks */
    var_DelCallback( p_intf->p_libvlc, "intf-show", IntfShowCB, p_intf );
    var_DelCallback( p_intf->p_libvlc, "intf-popupmenu", PopupMenuCB, p_intf );

    p_intf->p_sys->p_mi = NULL;
}

/*****************************
 *   Main UI handling        *
 *****************************/
void MainInterface::recreateToolbars()
{
    bool b_adv = getControlsVisibilityStatus() & CONTROLS_ADVANCED;

    settings->beginGroup( "MainWindow" );
    delete controls;
    delete inputC;

    controls = new ControlsWidget( p_intf, b_adv, this );
    inputC = new InputControlsWidget( p_intf, this );

    if( fullscreenControls )
    {
        delete fullscreenControls;
        fullscreenControls = new FullscreenControllerWidget( p_intf, this );
        CONNECT( fullscreenControls, keyPressed( QKeyEvent * ),
                 this, handleKeyPress( QKeyEvent * ) );
    }
    mainLayout->insertWidget( 2, inputC );
    mainLayout->insertWidget( settings->value( "ToolbarPos", 0 ).toInt() ? 0: 3,
                              controls );
    settings->endGroup();
}

void MainInterface::createMainWidget( QSettings *settings )
{
    /* Create the main Widget and the mainLayout */
    QWidget *main = new QWidget;
    setCentralWidget( main );
    mainLayout = new QVBoxLayout( main );
    main->setContentsMargins( 0, 0, 0, 0 );
    mainLayout->setSpacing( 0 ); mainLayout->setMargin( 0 );

    /* */
    stackCentralW = new QVLCStackedWidget( main );

    /* Bg Cone */
    bgWidget = new BackgroundWidget( p_intf );
    stackCentralW->addWidget( bgWidget );

    /* And video Outputs */
    if( b_videoEmbedded )
    {
        videoWidget = new VideoWidget( p_intf );
        stackCentralW->addWidget( videoWidget );
    }
    mainLayout->insertWidget( 1, stackCentralW );

    settings->beginGroup( "MainWindow" );
    stackWidgetsSizes[bgWidget] = settings->value( "bgSize", QSize( 400, 0 ) ).toSize();
    /* Resize even if no-auto-resize, because we are at creation */
    resizeStack( stackWidgetsSizes[bgWidget].width(), stackWidgetsSizes[bgWidget].height() );

    /* Create the CONTROLS Widget */
    controls = new ControlsWidget( p_intf,
                   settings->value( "adv-controls", false ).toBool(), this );
    inputC = new InputControlsWidget( p_intf, this );

    mainLayout->insertWidget( 2, inputC );
    mainLayout->insertWidget( settings->value( "ToolbarPos", 0 ).toInt() ? 0: 3,
                              controls );

    /* Visualisation, disabled for now, they SUCK */
    #if 0
    visualSelector = new VisualSelector( p_intf );
    mainLayout->insertWidget( 0, visualSelector );
    visualSelector->hide();
    #endif

    settings->endGroup();

    /* Enable the popup menu in the MI */
    main->setContextMenuPolicy( Qt::CustomContextMenu );
    CONNECT( main, customContextMenuRequested( const QPoint& ),
             this, popupMenu( const QPoint& ) );

    if ( depth() > 8 ) /* 8bit depth has too many issues with opacity */
        /* Create the FULLSCREEN CONTROLS Widget */
        if( var_InheritBool( p_intf, "qt-fs-controller" ) )
        {
            fullscreenControls = new FullscreenControllerWidget( p_intf, this );
            CONNECT( fullscreenControls, keyPressed( QKeyEvent * ),
                     this, handleKeyPress( QKeyEvent * ) );
        }
}

inline void MainInterface::initSystray()
{
#ifndef HAVE_MAEMO
    bool b_systrayAvailable = QSystemTrayIcon::isSystemTrayAvailable();
    bool b_systrayWanted = var_InheritBool( p_intf, "qt-system-tray" );

    if( var_InheritBool( p_intf, "qt-start-minimized") )
    {
        if( b_systrayAvailable )
        {
            b_systrayWanted = true;
            b_hideAfterCreation = true;
        }
        else
            msg_Err( p_intf, "cannot start minimized without system tray bar" );
    }

    if( b_systrayAvailable && b_systrayWanted )
        createSystray();
#endif
}

inline void MainInterface::createStatusBar()
{
    /****************
     *  Status Bar  *
     ****************/
    /* Widgets Creation*/
    QStatusBar *statusBarr = statusBar();

    TimeLabel *timeLabel = new TimeLabel( p_intf );
    nameLabel = new QLabel( this );
    nameLabel->setTextInteractionFlags( Qt::TextSelectableByMouse
                                      | Qt::TextSelectableByKeyboard );
    SpeedLabel *speedLabel = new SpeedLabel( p_intf, this );

    /* Styling those labels */
    timeLabel->setFrameStyle( QFrame::Sunken | QFrame::Panel );
    speedLabel->setFrameStyle( QFrame::Sunken | QFrame::Panel );
    nameLabel->setFrameStyle( QFrame::Sunken | QFrame::StyledPanel);
    timeLabel->setStyleSheet(
            "QLabel:hover { background-color: rgba(255, 255, 255, 50%) }" );
    speedLabel->setStyleSheet(
            "QLabel:hover { background-color: rgba(255, 255, 255, 50%) }" );

    /* and adding those */
    statusBarr->addWidget( nameLabel, 8 );
    statusBarr->addPermanentWidget( speedLabel, 0 );
    statusBarr->addPermanentWidget( timeLabel, 0 );

    /* timeLabel behaviour:
       - double clicking opens the goto time dialog
       - right-clicking and clicking just toggle between remaining and
         elapsed time.*/
    CONNECT( timeLabel, timeLabelDoubleClicked(), THEDP, gotoTimeDialog() );

    CONNECT( THEMIM->getIM(), encryptionChanged( bool ),
             this, showCryptedLabel( bool ) );

    CONNECT( THEMIM->getIM(), seekRequested( float ),
             timeLabel, setDisplayPosition( float ) );

    /* This shouldn't be necessary, but for somehow reason, the statusBarr
       starts at height of 20px and when a text is shown it needs more space.
       But, as the QMainWindow policy doesn't allow statusBar to change QMW's
       geometry, we need to force a height. If you have a better idea, please
       tell me -- jb
     */
    statusBarr->setFixedHeight( statusBarr->sizeHint().height() + 2 );
}

/**********************************************************************
 * Handling of sizing of the components
 **********************************************************************/

void MainInterface::debug()
{
#ifdef DEBUG_INTF
    msg_Dbg( p_intf, "size: %i - %i", size().height(), size().width() );
    msg_Dbg( p_intf, "sizeHint: %i - %i", sizeHint().height(), sizeHint().width() );
    msg_Dbg( p_intf, "minimumsize: %i - %i", minimumSize().height(), minimumSize().width() );

    msg_Dbg( p_intf, "Stack size: %i - %i", stackCentralW->size().height(), stackCentralW->size().width() );
    msg_Dbg( p_intf, "Stack sizeHint: %i - %i", stackCentralW->sizeHint().height(), stackCentralW->sizeHint().width() );
    msg_Dbg( p_intf, "Central size: %i - %i", centralWidget()->size().height(), centralWidget()->size().width() );
#endif
}

inline void MainInterface::showVideo() { showTab( videoWidget ); }
inline void MainInterface::restoreStackOldWidget()
            { showTab( stackCentralOldWidget ); }

inline void MainInterface::showTab( QWidget *widget )
{
#ifdef DEBUG_INTF
    msg_Warn( p_intf, "Old stackCentralOldWidget %i", stackCentralW->indexOf( stackCentralOldWidget ) );
#endif

    stackCentralOldWidget = stackCentralW->currentWidget();
    stackWidgetsSizes[stackCentralOldWidget] = stackCentralW->size();

    stackCentralW->setCurrentWidget( widget );
    if( b_autoresize )
        resizeStack( stackWidgetsSizes[widget].width(), stackWidgetsSizes[widget].height() );

#ifdef DEBUG_INTF
    msg_Warn( p_intf, "State change %i",  stackCentralW->currentIndex() );
    msg_Warn( p_intf, "New stackCentralOldWidget %i", stackCentralW->indexOf( stackCentralOldWidget ) );
#endif
}

void MainInterface::destroyPopupMenu()
{
    QVLCMenu::PopupMenu( p_intf, false );
}

void MainInterface::popupMenu( const QPoint &p )
{
    QVLCMenu::PopupMenu( p_intf, true );
}

void MainInterface::toggleFSC()
{
   if( !fullscreenControls ) return;

   IMEvent *eShow = new IMEvent( FullscreenControlToggle_Type, 0 );
   QApplication::postEvent( fullscreenControls, eShow );
}

/****************************************************************************
 * Video Handling
 ****************************************************************************/

/**
 * NOTE:
 * You must not change the state of this object or other Qt4 UI objects,
 * from the video output thread - only from the Qt4 UI main loop thread.
 * All window provider queries must be handled through signals or events.
 * That's why we have all those emit statements...
 */
WId MainInterface::getVideo( int *pi_x, int *pi_y,
                             unsigned int *pi_width, unsigned int *pi_height )
{
    if( !videoWidget )
        return 0;

    /* This is a blocking call signal. Results are returned through pointers.
     * Beware of deadlocks! */
    WId id;
    emit askGetVideo( &id, pi_x, pi_y, pi_width, pi_height );
    return id;
}

void MainInterface::getVideoSlot( WId *p_id, int *pi_x, int *pi_y,
                                  unsigned *pi_width, unsigned *pi_height )
{
    /* Request the videoWidget */
    WId ret = videoWidget->request( pi_x, pi_y,
                                    pi_width, pi_height, !b_autoresize );
    *p_id = ret;
    if( ret ) /* The videoWidget is available */
    {
        /* Consider the video active now */
        showVideo();

        /* Ask videoWidget to resize correctly, if we are in normal mode */
        if( !isFullScreen() && !isMaximized() && b_autoresize )
            videoWidget->SetSizing( *pi_width, *pi_height );
    }
}

/* Asynchronous call from the WindowClose function */
void MainInterface::releaseVideo( void )
{
    emit askReleaseVideo();
}

/* Function that is CONNECTED to the previous emit */
void MainInterface::releaseVideoSlot( void )
{
    /* This function is called when the embedded video window is destroyed,
     * or in the rare case that the embedded window is still here but the
     * Qt4 interface exits. */
    assert( videoWidget );
    videoWidget->release();
    setVideoOnTop( false );
    setVideoFullScreen( false );

    if( stackCentralW->currentWidget() == videoWidget )
        restoreStackOldWidget();

    /* We don't want to have a blank video to popup */
    stackCentralOldWidget = bgWidget;
}

void MainInterface::setVideoSize( unsigned int w, unsigned int h )
{
    if( !isFullScreen() && !isMaximized() )
        videoWidget->SetSizing( w, h );
}

void MainInterface::setVideoFullScreen( bool fs )
{
    b_videoFullScreen = fs;
    if( fs )
    {
        int numscreen = var_InheritInteger( p_intf, "qt-fullscreen-screennumber" );
        /* if user hasn't defined screennumber, or screennumber that is bigger
         * than current number of screens, take screennumber where current interface
         * is
         */
        if( numscreen == -1 || numscreen > QApplication::desktop()->numScreens() )
            numscreen = QApplication::desktop()->screenNumber( p_intf->p_sys->p_mi );

        QRect screenres = QApplication::desktop()->screenGeometry( numscreen );

        /* To be sure window is on proper-screen in xinerama */
#if HAS_QT46
        if( !screenres.contains( pos() ) && QApplication::desktop()->screenCount() > 1 )
#else
        if( !screenres.contains( pos() ) )
#endif
        {
            msg_Dbg( p_intf, "Moving video to correct screen");
            move( QPoint( screenres.x(), screenres.y() ) );
        }
        setMinimalView( true );
        setInterfaceFullScreen( true );
    }
    else
    {
        /* TODO do we want to restore screen and position ? (when
         * qt-fullscreen-screennumber is forced) */
        setMinimalView( b_minimalView );
        setInterfaceFullScreen( b_interfaceFullScreen );
    }
    videoWidget->sync();
}

/* Slot to change the video always-on-top flag.
 * Emit askVideoOnTop() to invoke this from other thread. */
void MainInterface::setVideoOnTop( bool on_top )
{
    b_videoOnTop = on_top;

    Qt::WindowFlags oldflags = windowFlags(), newflags;

    if( b_videoOnTop )
        newflags = oldflags | Qt::WindowStaysOnTopHint;
    else
        newflags = oldflags & ~Qt::WindowStaysOnTopHint;

    if( newflags != oldflags )
    {
        setWindowFlags( newflags );
        show(); /* necessary to apply window flags */
    }
}

/* Asynchronous call from WindowControl function */
int MainInterface::controlVideo( int i_query, va_list args )
{
    switch( i_query )
    {
    case VOUT_WINDOW_SET_SIZE:
    {
        unsigned int i_width  = va_arg( args, unsigned int );
        unsigned int i_height = va_arg( args, unsigned int );

        emit askVideoToResize( i_width, i_height );
        return VLC_SUCCESS;
    }
    case VOUT_WINDOW_SET_STATE:
    {
        unsigned i_arg = va_arg( args, unsigned );
        unsigned on_top = i_arg & VOUT_WINDOW_STATE_ABOVE;

        emit askVideoOnTop( on_top != 0 );
        return VLC_SUCCESS;
    }
    case VOUT_WINDOW_SET_FULLSCREEN:
    {
        bool b_fs = va_arg( args, int );

        emit askVideoSetFullScreen( b_fs );
        return VLC_SUCCESS;
    }
    default:
        msg_Warn( p_intf, "unsupported control query" );
        return VLC_EGENERIC;
    }
}

/*****************************************************************************
 * Playlist, Visualisation and Menus handling
 *****************************************************************************/
/**
 * Toggle the playlist widget or dialog
 **/
void MainInterface::createPlaylist()
{
    playlistWidget = new PlaylistWidget( p_intf, this );

    if( b_plDocked )
    {
        stackCentralW->addWidget( playlistWidget );
        stackWidgetsSizes[playlistWidget] = settings->value( "playlistSize", QSize( 500, 250 ) ).toSize();
    }
    else
    {
#ifdef WIN32
        playlistWidget->setParent( NULL );
#endif
        playlistWidget->setWindowFlags( Qt::Window );

        /* This will restore the geometry but will not work for position,
           because of parenting */
        QVLCTools::restoreWidgetPosition( p_intf, "Playlist",
                playlistWidget, QSize( 600, 300 ) );
    }
}

void MainInterface::togglePlaylist()
{
    if( !playlistWidget )
    {
        createPlaylist();
    }

    if( b_plDocked )
    {
        /* Playlist is not visible, show it */
        if( stackCentralW->currentWidget() != playlistWidget )
        {
            showTab( playlistWidget );
        }
        else /* Hide it! */
        {
            restoreStackOldWidget();
        }
        playlistVisible = ( stackCentralW->currentWidget() == playlistWidget );
    }
    else
    {
#ifdef WIN32
        playlistWidget->setParent( NULL );
#endif
        playlistWidget->setWindowFlags( Qt::Window );
        playlistVisible = !playlistVisible;
        playlistWidget->setVisible( playlistVisible );
    }
    debug();
}

void MainInterface::dockPlaylist( bool p_docked )
{
    if( b_plDocked == p_docked ) return;
    b_plDocked = p_docked;

    if( !playlistWidget ) return; /* Playlist wasn't created yet */
    if( !p_docked )
    {
        stackCentralW->removeWidget( playlistWidget );
#ifdef WIN32
        playlistWidget->setParent( NULL );
#endif
        playlistWidget->setWindowFlags( Qt::Window );
        QVLCTools::restoreWidgetPosition( p_intf, "Playlist",
                playlistWidget, QSize( 600, 300 ) );
        playlistWidget->show();
        restoreStackOldWidget();
    }
    else
    {
        QVLCTools::saveWidgetPosition( p_intf, "Playlist", playlistWidget );
        playlistWidget->setWindowFlags( Qt::Widget ); // Probably a Qt bug here
        // It would be logical that QStackWidget::addWidget reset the flags...
        stackCentralW->addWidget( playlistWidget );
        showTab( playlistWidget );
    }
    playlistVisible = true;
}

void MainInterface::setMinimalView( bool b_minimal )
{
    menuBar()->setVisible( !b_minimal );
    controls->setVisible( !b_minimal );
    statusBar()->setVisible( !b_minimal );
    inputC->setVisible( !b_minimal );
}

/*
  If b_minimal is false, then we are normalView
 */
void MainInterface::toggleMinimalView( bool b_minimal )
{
    if( !b_minimalView && b_autoresize ) /* Normal mode */
    {
        if( stackCentralW->currentWidget() == bgWidget )
        {
            if( stackCentralW->height() < 16 )
            {
                resizeStack( stackCentralW->width(), 100 );
            }
        }
    }
    b_minimalView = b_minimal;
    if( !b_videoFullScreen )
        setMinimalView( b_minimalView );

    emit minimalViewToggled( b_minimalView );
}

/* toggling advanced controls buttons */
void MainInterface::toggleAdvancedButtons()
{
    controls->toggleAdvanced();
//    if( fullscreenControls ) fullscreenControls->toggleAdvanced();
}

/* Get the visibility status of the controls (hidden or not, advanced or not) */
int MainInterface::getControlsVisibilityStatus()
{
    if( !controls ) return 0;
    return( (controls->isVisible() ? CONTROLS_VISIBLE : CONTROLS_HIDDEN )
                + CONTROLS_ADVANCED * controls->b_advancedVisible );
}

#if 0
void MainInterface::visual()
{
    if( !VISIBLE( visualSelector) )
    {
        visualSelector->show();
        if( !THEMIM->getIM()->hasVideo() )
        {
            /* Show the background widget */
        }
        visualSelectorEnabled = true;
    }
    else
    {
        /* Stop any currently running visualization */
        visualSelector->hide();
        visualSelectorEnabled = false;
    }
}
#endif

/************************************************************************
 * Other stuff
 ************************************************************************/
void MainInterface::setName( const QString& name )
{
    input_name = name; /* store it for the QSystray use */
    /* Display it in the status bar, but also as a Tooltip in case it doesn't
       fit in the label */
    nameLabel->setText( " " + name + " " );
    nameLabel->setToolTip( " " + name +" " );
}

/**
 * Give the decorations of the Main Window a correct Name.
 * If nothing is given, set it to VLC...
 **/
void MainInterface::setVLCWindowsTitle( const QString& aTitle )
{
    if( aTitle.isEmpty() )
    {
        setWindowTitle( qtr( "VLC media player" ) );
    }
    else
    {
        setWindowTitle( aTitle + " - " + qtr( "VLC media player" ) );
    }
}

void MainInterface::showCryptedLabel( bool b_show )
{
    if( cryptedLabel == NULL )
    {
        cryptedLabel = new QLabel;
        // The lock icon is not the right one for DRM protection/scrambled.
        //cryptedLabel->setPixmap( QPixmap( ":/lock" ) );
        cryptedLabel->setText( "DRM" );
        statusBar()->addWidget( cryptedLabel );
    }

    cryptedLabel->setVisible( b_show );
}

void MainInterface::showBuffering( float f_cache )
{
    QString amount = QString("Buffering: %1%").arg( (int)(100*f_cache) );
    statusBar()->showMessage( amount, 1000 );
}

/*****************************************************************************
 * Systray Icon and Systray Menu
 *****************************************************************************/
#ifndef HAVE_MAEMO
/**
 * Create a SystemTray icon and a menu that would go with it.
 * Connects to a click handler on the icon.
 **/
void MainInterface::createSystray()
{
    QIcon iconVLC;
    if( QDate::currentDate().dayOfYear() >= 354 )
        iconVLC =  QIcon( ":/logo/vlc128-christmas.png" );
    else
        iconVLC =  QIcon( ":/logo/vlc128.png" );
    sysTray = new QSystemTrayIcon( iconVLC, this );
    sysTray->setToolTip( qtr( "VLC media player" ));

    systrayMenu = new QMenu( qtr( "VLC media player" ), this );
    systrayMenu->setIcon( iconVLC );

    QVLCMenu::updateSystrayMenu( this, p_intf, true );
    sysTray->show();

    CONNECT( sysTray, activated( QSystemTrayIcon::ActivationReason ),
            this, handleSystrayClick( QSystemTrayIcon::ActivationReason ) );
}

/**
 * Updates the Systray Icon's menu and toggle the main interface
 */
void MainInterface::toggleUpdateSystrayMenu()
{
    /* If hidden, show it */
    if( isHidden() )
    {
        show();
        activateWindow();
    }
    else if( isMinimized() )
    {
        /* Minimized */
        showNormal();
        activateWindow();
    }
    else
    {
        /* Visible (possibly under other windows) */
#ifdef WIN32
        /* check if any visible window is above vlc in the z-order,
         * but ignore the ones always on top
         * and the ones which can't be activated */
        WINDOWINFO wi;
        HWND hwnd;
        wi.cbSize = sizeof( WINDOWINFO );
        for( hwnd = GetNextWindow( internalWinId(), GW_HWNDPREV );
                hwnd && ( !IsWindowVisible( hwnd ) ||
                    ( GetWindowInfo( hwnd, &wi ) &&
                      (wi.dwExStyle&WS_EX_NOACTIVATE) ) );
                hwnd = GetNextWindow( hwnd, GW_HWNDPREV ) );
            if( !hwnd || !GetWindowInfo( hwnd, &wi ) ||
                (wi.dwExStyle&WS_EX_TOPMOST) )
            {
                hide();
            }
            else
            {
                activateWindow();
            }
#else
        hide();
#endif
    }
    QVLCMenu::updateSystrayMenu( this, p_intf );
}

void MainInterface::handleSystrayClick(
                                    QSystemTrayIcon::ActivationReason reason )
{
    switch( reason )
    {
        case QSystemTrayIcon::Trigger:
        case QSystemTrayIcon::DoubleClick:
            toggleUpdateSystrayMenu();
            break;
        case QSystemTrayIcon::MiddleClick:
            sysTray->showMessage( qtr( "VLC media player" ),
                    qtr( "Control menu for the player" ),
                    QSystemTrayIcon::Information, 3000 );
            break;
        default:
            break;
    }
}

/**
 * Updates the name of the systray Icon tooltip.
 * Doesn't check if the systray exists, check before you call it.
 **/
void MainInterface::updateSystrayTooltipName( const QString& name )
{
    if( name.isEmpty() )
    {
        sysTray->setToolTip( qtr( "VLC media player" ) );
    }
    else
    {
        sysTray->setToolTip( name );
        if( b_notificationEnabled && ( isHidden() || isMinimized() ) )
        {
            sysTray->showMessage( qtr( "VLC media player" ), name,
                    QSystemTrayIcon::NoIcon, 3000 );
        }
    }

    QVLCMenu::updateSystrayMenu( this, p_intf );
}

/**
 * Updates the status of the systray Icon tooltip.
 * Doesn't check if the systray exists, check before you call it.
 **/
void MainInterface::updateSystrayTooltipStatus( int i_status )
{
    switch( i_status )
    {
    case PLAYING_S:
        sysTray->setToolTip( input_name );
        break;
    case PAUSE_S:
        sysTray->setToolTip( input_name + " - " + qtr( "Paused") );
        break;
    default:
        sysTray->setToolTip( qtr( "VLC media player" ) );
        break;
    }
    QVLCMenu::updateSystrayMenu( this, p_intf );
}
#endif

/************************************************************************
 * D&D Events
 ************************************************************************/
void MainInterface::dropEvent(QDropEvent *event)
{
    dropEventPlay( event, true );
}

void MainInterface::dropEventPlay( QDropEvent *event, bool b_play )
{
    event->setDropAction( Qt::CopyAction );
    if( !event->possibleActions() & Qt::CopyAction )
        return;

    const QMimeData *mimeData = event->mimeData();

    /* D&D of a subtitles file, add it on the fly */
    if( mimeData->urls().size() == 1 && THEMIM->getIM()->hasInput() )
    {
        if( !input_AddSubtitle( THEMIM->getInput(),
                 qtu( toNativeSeparators( mimeData->urls()[0].toLocalFile() ) ),
                 true ) )
        {
            event->accept();
            return;
        }
    }

    bool first = b_play;
    foreach( const QUrl &url, mimeData->urls() )
    {
        QString s = toNativeSeparators( url.toLocalFile() );

        if( s.length() > 0 ) {
            char* psz_uri = make_URI( qtu(s) );
            playlist_Add( THEPL, psz_uri, NULL,
                          PLAYLIST_APPEND | (first ? PLAYLIST_GO: PLAYLIST_PREPARSE),
                          PLAYLIST_END, true, pl_Unlocked );
            free( psz_uri );
            first = false;
            RecentsMRL::getInstance( p_intf )->addRecent( s );
        }
    }
    event->accept();
}
void MainInterface::dragEnterEvent(QDragEnterEvent *event)
{
     event->acceptProposedAction();
}
void MainInterface::dragMoveEvent(QDragMoveEvent *event)
{
     event->acceptProposedAction();
}
void MainInterface::dragLeaveEvent(QDragLeaveEvent *event)
{
     event->accept();
}

/************************************************************************
 * Events stuff
 ************************************************************************/
void MainInterface::keyPressEvent( QKeyEvent *e )
{
    handleKeyPress( e );
}

void MainInterface::handleKeyPress( QKeyEvent *e )
{
    if( ( e->modifiers() &  Qt::ControlModifier ) && ( e->key() == Qt::Key_H ) )
    {
        toggleMinimalView( !b_minimalView );
        e->accept();
    }

    int i_vlck = qtEventToVLCKey( e );
    if( i_vlck > 0 )
    {
        var_SetInteger( p_intf->p_libvlc, "key-pressed", i_vlck );
        e->accept();
    }
    else
        e->ignore();
}

void MainInterface::wheelEvent( QWheelEvent *e )
{
    int i_vlckey = qtWheelEventToVLCKey( e );
    var_SetInteger( p_intf->p_libvlc, "key-pressed", i_vlckey );
    e->accept();
}

void MainInterface::closeEvent( QCloseEvent *e )
{
    //hide();
    e->ignore();
    emit askToQuit();
}

void MainInterface::setInterfaceFullScreen( bool fs )
{
    if( fs )
        setWindowState( windowState() | Qt::WindowFullScreen );
    else
        setWindowState( windowState() & ~Qt::WindowFullScreen );
}
void MainInterface::toggleInterfaceFullScreen()
{
    b_interfaceFullScreen = !b_interfaceFullScreen;
    if( !b_videoFullScreen )
        setInterfaceFullScreen( b_interfaceFullScreen );
    emit fullscreenInterfaceToggled( b_interfaceFullScreen );
}

/*****************************************************************************
 * PopupMenuCB: callback triggered by the intf-popupmenu playlist variable.
 *  We don't show the menu directly here because we don't want the
 *  caller to block for a too long time.
 *****************************************************************************/
static int PopupMenuCB( vlc_object_t *p_this, const char *psz_variable,
                        vlc_value_t old_val, vlc_value_t new_val, void *param )
{
    intf_thread_t *p_intf = (intf_thread_t *)param;

    if( p_intf->pf_show_dialog )
    {
        p_intf->pf_show_dialog( p_intf, INTF_DIALOG_POPUPMENU,
                                new_val.b_bool, NULL );
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * IntfShowCB: callback triggered by the intf-show libvlc variable.
 *****************************************************************************/
static int IntfShowCB( vlc_object_t *p_this, const char *psz_variable,
                       vlc_value_t old_val, vlc_value_t new_val, void *param )
{
    intf_thread_t *p_intf = (intf_thread_t *)param;
    p_intf->p_sys->p_mi->toggleFSC();

    /* Show event */
     return VLC_SUCCESS;
}
