/*****************************************************************************
 * open.cpp : Advanced open dialog
 *****************************************************************************
 * Copyright © 2006-2009 the VideoLAN team
 * $Id$
 *
 * Authors: Jean-Baptiste Kempf <jb@videolan.org>
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "dialogs/open.hpp"

#include "dialogs_provider.hpp"

#include "recents.hpp"
#include "util/qt_dirs.hpp"

#include <QTabWidget>
#include <QGridLayout>
#include <QRegExp>
#include <QMenu>

#define DEBUG_QT 1

OpenDialog *OpenDialog::instance = NULL;

OpenDialog* OpenDialog::getInstance( QWidget *parent, intf_thread_t *p_intf,
        bool b_rawInstance, int _action_flag, bool b_selectMode, bool _b_pl )
{
    /* Creation */
    if( !instance )
        instance = new OpenDialog( parent, p_intf, b_selectMode,
                                   _action_flag, _b_pl );
    else if( !b_rawInstance )
    {
        /* Request the instance but change small details:
           - Button menu
           - Modality on top of the parent dialog */
        if( b_selectMode )
        {
            instance->setWindowModality( Qt::WindowModal );
            _action_flag = SELECT; /* This should be useless, but we never know
                                      if the call is correct */
        }
        instance->i_action_flag = _action_flag;
        instance->b_pl = _b_pl;
        instance->setMenuAction();
    }
    return instance;
}

OpenDialog::OpenDialog( QWidget *parent,
                        intf_thread_t *_p_intf,
                        bool b_selectMode,
                        int _action_flag,
                        bool _b_pl)  :  QVLCDialog( parent, _p_intf )
{
    i_action_flag = _action_flag;
    b_pl =_b_pl;

    /* Workaround the Win32 Vout that put the video on top at regular times */
#ifdef WIN32
    setWindowFlags( Qt::WindowStaysOnTopHint | Qt::Dialog );
#endif

    if( b_selectMode ) /* Select mode */
    {
        i_action_flag = SELECT;
        setWindowModality( Qt::WindowModal );
    }

    /* Basic Creation of the Window */
    ui.setupUi( this );
    setWindowTitle( qtr( "Open Media" ) );

    /* Tab definition and creation */
    fileOpenPanel    = new FileOpenPanel( this, p_intf );
    discOpenPanel    = new DiscOpenPanel( this, p_intf );
    netOpenPanel     = new NetOpenPanel( this, p_intf );
    captureOpenPanel = new CaptureOpenPanel( this, p_intf );

    /* Insert the tabs */
    ui.Tab->insertTab( OPEN_FILE_TAB, fileOpenPanel, qtr( "&File" ) );
    ui.Tab->insertTab( OPEN_DISC_TAB, discOpenPanel, qtr( "&Disc" ) );
    ui.Tab->insertTab( OPEN_NETWORK_TAB, netOpenPanel, qtr( "&Network" ) );
    ui.Tab->insertTab( OPEN_CAPTURE_TAB, captureOpenPanel,
                       qtr( "Capture &Device" ) );

    /* Hide the Slave input widgets */
    ui.slaveLabel->hide();
    ui.slaveText->hide();
    ui.slaveBrowseButton->hide();

    /* Buttons Creation */
    /* Play Button */
    playButton = ui.playButton;

    /* Cancel Button */
    cancelButton = new QPushButton( qtr( "&Cancel" ) );

    /* Select Button */
    selectButton = new QPushButton( qtr( "&Select" ) );

    /* Menu for the Play button */
    QMenu * openButtonMenu = new QMenu( "Open" );
    openButtonMenu->addAction( qtr( "&Enqueue" ), this, SLOT( enqueue() ),
                                    QKeySequence( "Alt+E" ) );
    openButtonMenu->addAction( qtr( "&Play" ), this, SLOT( play() ),
                                    QKeySequence( "Alt+P" ) );
    openButtonMenu->addAction( qtr( "&Stream" ), this, SLOT( stream() ) ,
                                    QKeySequence( "Alt+S" ) );
    openButtonMenu->addAction( qtr( "&Convert" ), this, SLOT( transcode() ) ,
                                    QKeySequence( "Alt+C" ) );

    ui.menuButton->setMenu( openButtonMenu );
    ui.menuButton->setIcon( QIcon( ":/down_arrow" ) );

    /* Add the three Buttons */
    ui.buttonsBox->addButton( selectButton, QDialogButtonBox::AcceptRole );
    ui.buttonsBox->addButton( cancelButton, QDialogButtonBox::RejectRole );

    /* At creation time, modify the default buttons */
    setMenuAction();

    /* Force MRL update on tab change */
    CONNECT( ui.Tab, currentChanged( int ), this, signalCurrent( int ) );

    CONNECT( fileOpenPanel, mrlUpdated( const QStringList&, const QString& ),
             this, updateMRL( const QStringList&, const QString& ) );
    CONNECT( netOpenPanel, mrlUpdated( const QStringList&, const QString& ),
             this, updateMRL( const QStringList&, const QString& ) );
    CONNECT( discOpenPanel, mrlUpdated( const QStringList&, const QString& ),
             this, updateMRL( const QStringList&, const QString& ) );
    CONNECT( captureOpenPanel, mrlUpdated( const QStringList&, const QString& ),
             this, updateMRL( const QStringList&, const QString& ) );

    CONNECT( fileOpenPanel, methodChanged( const QString& ),
             this, newCachingMethod( const QString& ) );
    CONNECT( netOpenPanel, methodChanged( const QString& ),
             this, newCachingMethod( const QString& ) );
    CONNECT( discOpenPanel, methodChanged( const QString& ),
             this, newCachingMethod( const QString& ) );
    CONNECT( captureOpenPanel, methodChanged( const QString& ),
             this, newCachingMethod( const QString& ) );

    /* Advanced frame Connects */
    CONNECT( ui.slaveCheckbox, toggled( bool ), this, updateMRL() );
    CONNECT( ui.slaveText, textChanged( const QString& ), this, updateMRL() );
    CONNECT( ui.cacheSpinBox, valueChanged( int ), this, updateMRL() );
    CONNECT( ui.startTimeDoubleSpinBox, valueChanged( double ), this, updateMRL() );
    BUTTONACT( ui.advancedCheckBox, toggleAdvancedPanel() );
    BUTTONACT( ui.slaveBrowseButton, browseInputSlave() );

    /* Buttons action */
    BUTTONACT( playButton, selectSlots() );
    BUTTONACT( selectButton, close() );
    BUTTONACT( cancelButton, cancel() );

    /* Hide the advancedPanel */
    if( !config_GetInt( p_intf, "qt-adv-options" ) )
        ui.advancedFrame->hide();
    else
        ui.advancedCheckBox->setChecked( true );

    /* Initialize caching */
    storedMethod = "";
    newCachingMethod( "file-caching" );

    setMinimumSize( sizeHint() );
    setMaximumWidth( 900 );
    resize( getSettings()->value( "opendialog-size", QSize( 500, 490 ) ).toSize() );
}

OpenDialog::~OpenDialog()
{
    getSettings()->setValue( "opendialog-size", size() );
}

/* Used by VLM dialog and inputSlave selection */
QString OpenDialog::getMRL( bool b_all )
{
    if( itemsMRL.size() == 0 ) return "";
    return b_all ? itemsMRL[0] + ui.advancedLineInput->text()
                 : itemsMRL[0];
}

/* Finish the dialog and decide if you open another one after */
void OpenDialog::setMenuAction()
{
    if( i_action_flag == SELECT )
    {
        playButton->hide();
        selectButton->show();
        selectButton->setDefault( true );
    }
    else
    {
        switch ( i_action_flag )
        {
        case OPEN_AND_STREAM:
            playButton->setText( qtr( "&Stream" ) );
            break;
        case OPEN_AND_SAVE:
            playButton->setText( qtr( "&Convert / Save" ) );
            break;
        case OPEN_AND_ENQUEUE:
            playButton->setText( qtr( "&Enqueue" ) );
            break;
        case OPEN_AND_PLAY:
        default:
            playButton->setText( qtr( "&Play" ) );
        }
        playButton->show();
        selectButton->hide();
        playButton->setDefault( true );
    }
}

void OpenDialog::showTab( int i_tab )
{
    if( i_tab == OPEN_CAPTURE_TAB ) captureOpenPanel->initialize();
    ui.Tab->setCurrentIndex( i_tab );
    show();
}

/* Function called on signal currentChanged triggered */
void OpenDialog::signalCurrent( int i_tab )
{
    if( i_tab == OPEN_CAPTURE_TAB ) captureOpenPanel->initialize();

    if( ui.Tab->currentWidget() != NULL )
        ( dynamic_cast<OpenPanel *>( ui.Tab->currentWidget() ) )->updateMRL();
}

void OpenDialog::toggleAdvancedPanel()
{
    if( ui.advancedFrame->isVisible() )
    {
        ui.advancedFrame->hide();
        if( size().isValid() )
            resize( size().width(), size().height()
                    - ui.advancedFrame->height() );
    }
    else
    {
        ui.advancedFrame->show();
        if( size().isValid() )
            resize( size().width(), size().height()
                    + ui.advancedFrame->height() );
    }
}

/***********
 * Actions *
 ***********/
/* If Cancel is pressed or escaped */
void OpenDialog::cancel()
{
    /* Clear the panels */
    for( int i = 0; i < OPEN_TAB_MAX; i++ )
        dynamic_cast<OpenPanel*>( ui.Tab->widget( i ) )->clear();

    /* Clear the variables */
    itemsMRL.clear();
    optionsMRL.clear();

    /* If in Select Mode, reject instead of hiding */
    if( i_action_flag == SELECT ) reject();
    else hide();
}

/* If EnterKey is pressed */
void OpenDialog::close()
{
    /* If in Select Mode, accept instead of selecting a Slot */
    if( i_action_flag == SELECT )
        accept();
    else
        selectSlots();
}

/* Play button */
void OpenDialog::selectSlots()
{
    switch ( i_action_flag )
    {
    case OPEN_AND_STREAM:
        stream();
        break;
    case OPEN_AND_SAVE:
        transcode();
        break;
    case OPEN_AND_ENQUEUE:
        enqueue();
        break;
    case OPEN_AND_PLAY:
    default:
        play();
    }
}

void OpenDialog::play()
{
    finish( false );
}

void OpenDialog::enqueue()
{
    finish( true );
}


void OpenDialog::finish( bool b_enqueue = false )
{
    toggleVisible();

    if( i_action_flag == SELECT )
    {
        accept();
        return;
    }

    /* Sort alphabetically */
    itemsMRL.sort();

    /* Go through the item list */
    for( int i = 0; i < itemsMRL.size(); i++ )
    {
        bool b_start = !i && !b_enqueue;

        input_item_t *p_input;
        p_input = input_item_New( p_intf, qtu( itemsMRL[i] ), NULL );

        /* Insert options only for the first element.
           We don't know how to edit that anyway. */
        if( i == 0 )
        {
            /* Take options from the UI, not from what we stored */
            QStringList optionsList = ui.advancedLineInput->text().split( " :" );

            /* Insert options */
            for( int j = 0; j < optionsList.size(); j++ )
            {
                QString qs = colon_unescape( optionsList[j] );
                if( !qs.isEmpty() )
                {
                    input_item_AddOption( p_input, qtu( qs ),
                                          VLC_INPUT_OPTION_TRUSTED );
#ifdef DEBUG_QT
                    msg_Warn( p_intf, "Input option: %s", qtu( qs ) );
#endif
                }
            }
        }

        /* Switch between enqueuing and starting the item */
        /* FIXME: playlist_AddInput() can fail */
        playlist_AddInput( THEPL, p_input,
                PLAYLIST_APPEND | ( b_start ? PLAYLIST_GO : PLAYLIST_PREPARSE ),
                PLAYLIST_END, b_pl ? true : false, pl_Unlocked );
        vlc_gc_decref( p_input );

        /* Do not add the current MRL if playlist_AddInput fail */
        RecentsMRL::getInstance( p_intf )->addRecent( itemsMRL[i] );
    }
}

void OpenDialog::transcode()
{
    stream( true );
}

void OpenDialog::stream( bool b_transcode_only )
{
    QString soutMRL = getMRL( false );
    if( soutMRL.isEmpty() ) return;
    toggleVisible();

    /* Dbg and send :D */
    msg_Dbg( p_intf, "MRL passed to the Sout: %s", qtu( soutMRL ) );
    THEDP->streamingDialog( this, soutMRL, b_transcode_only,
                            ui.advancedLineInput->text().split( " :" ) );
}

/* Update the MRL */
void OpenDialog::updateMRL( const QStringList& item, const QString& tempMRL )
{
    optionsMRL = tempMRL;
    itemsMRL = item;
    updateMRL();
}

void OpenDialog::updateMRL() {
    QString mrl = optionsMRL;
    if( ui.slaveCheckbox->isChecked() ) {
        mrl += " :input-slave=" + ui.slaveText->text();
    }
    int i_cache = config_GetInt( p_intf, qtu( storedMethod ) );
    if( i_cache != ui.cacheSpinBox->value() ) {
        mrl += QString( " :%1=%2" ).arg( storedMethod ).
                                  arg( ui.cacheSpinBox->value() );
    }
    if( ui.startTimeDoubleSpinBox->value() ) {
        mrl += " :start-time=" + QString::number( ui.startTimeDoubleSpinBox->value() );
    }
    ui.advancedLineInput->setText( mrl );
    ui.mrlLine->setText( itemsMRL.join( " " ) );
}

void OpenDialog::newCachingMethod( const QString& method )
{
    if( method != storedMethod ) {
        storedMethod = method;
        int i_value = config_GetInt( p_intf, qtu( storedMethod ) );
        ui.cacheSpinBox->setValue( i_value );
    }
}

QStringList OpenDialog::SeparateEntries( const QString& entries )
{
    bool b_quotes_mode = false;

    QStringList entries_array;
    QString entry;

    int index = 0;
    while( index < entries.size() )
    {
        int delim_pos = entries.indexOf( QRegExp( "\\s+|\"" ), index );
        if( delim_pos < 0 ) delim_pos = entries.size() - 1;
        entry += entries.mid( index, delim_pos - index + 1 );
        index = delim_pos + 1;

        if( entry.isEmpty() ) continue;

        if( !b_quotes_mode && entry.endsWith( "\"" ) )
        {
            /* Enters quotes mode */
            entry.truncate( entry.size() - 1 );
            b_quotes_mode = true;
        }
        else if( b_quotes_mode && entry.endsWith( "\"" ) )
        {
            /* Finished the quotes mode */
            entry.truncate( entry.size() - 1 );
            b_quotes_mode = false;
        }
        else if( !b_quotes_mode && !entry.endsWith( "\"" ) )
        {
            /* we found a non-quoted standalone string */
            if( index < entries.size() ||
                entry.endsWith( " " ) || entry.endsWith( "\t" ) ||
                entry.endsWith( "\r" ) || entry.endsWith( "\n" ) )
                entry.truncate( entry.size() - 1 );
            if( !entry.isEmpty() ) entries_array.append( entry );
            entry.clear();
        }
        else
        {;}
    }

    if( !entry.isEmpty() ) entries_array.append( entry );

    return entries_array;
}

void OpenDialog::browseInputSlave()
{
    OpenDialog *od = new OpenDialog( this, p_intf, true, SELECT );
    od->exec();
    ui.slaveText->setText( od->getMRL( false ) );
    delete od;
}
