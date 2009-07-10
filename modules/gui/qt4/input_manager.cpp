/*****************************************************************************
 * input_manager.cpp : Manage an input and interact with its GUI elements
 ****************************************************************************
 * Copyright (C) 2006-2008 the VideoLAN team
 * $Id: 30c170ea3855178dc74eb014c289f44a49798224 $
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
 *          Ilkka Ollakka  <ileoo@videolan.org>
 *          Jean-Baptiste <jb@videolan.org>
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

#include "input_manager.hpp"
#include <vlc_keys.h>
#include <vlc_url.h>

#include <QApplication>

#include <assert.h>

static int ItemChanged( vlc_object_t *, const char *,
                        vlc_value_t, vlc_value_t, void * );
static int PLItemChanged( vlc_object_t *, const char *,
                        vlc_value_t, vlc_value_t, void * );
static int VolumeChanged( vlc_object_t *, const char *,
                        vlc_value_t, vlc_value_t, void * );

static int InputEvent( vlc_object_t *, const char *,
                       vlc_value_t, vlc_value_t, void * );
static int VbiEvent( vlc_object_t *, const char *,
                     vlc_value_t, vlc_value_t, void * );


/**********************************************************************
 * InputManager implementation
 **********************************************************************
 * The Input Manager can be the main one around the playlist
 * But can also be used for VLM dialog or similar
 **********************************************************************/

InputManager::InputManager( QObject *parent, intf_thread_t *_p_intf) :
                           QObject( parent ), p_intf( _p_intf )
{
    i_old_playing_status = END_S;
    oldName      = "";
    artUrl       = "";
    p_input      = NULL;
    i_rate       = 0;
    i_input_id   = 0;
    b_video      = false;
    timeA        = 0;
    timeB        = 0;
    f_cache      = -1.; /* impossible initial value, different from all */
}

InputManager::~InputManager()
{
    delInput();
}

/* Define the Input used.
   Add the callbacks on input
   p_input is held once here */
void InputManager::setInput( input_thread_t *_p_input )
{
    delInput();
    p_input = _p_input;
    if( p_input && !( p_input->b_dead || !vlc_object_alive (p_input) ) )
    {
        msg_Dbg( p_intf, "IM: Setting an input" );
        vlc_object_hold( p_input );
        emit statusChanged( PLAYING_S );
        UpdateName();
        UpdateArt();
        UpdateTeletext();
        UpdateNavigation();
        UpdateVout();
        addCallbacks();
        i_input_id = input_GetItem( p_input )->i_id;
    }
    else
    {
        p_input = NULL;
        i_input_id = 0;
        emit rateChanged( INPUT_RATE_DEFAULT );
    }
}

/* delete Input if it ever existed.
   Delete the callbacls on input
   p_input is released once here */
void InputManager::delInput()
{
    if( !p_input ) return;
    msg_Dbg( p_intf, "IM: Deleting the input" );

    delCallbacks();
    i_old_playing_status = END_S;
    i_input_id           = 0;
    oldName              = "";
    artUrl               = "";
    b_video              = false;
    timeA                = 0;
    timeB                = 0;

    vlc_object_release( p_input );
    p_input = NULL;

    emit positionUpdated( -1.0, 0 ,0 );
    emit rateChanged( INPUT_RATE_DEFAULT ); /* TODO: Do we want this ? */
    emit nameChanged( "" );
    emit chapterChanged( 0 );
    emit titleChanged( 0 );
    emit statusChanged( END_S );

    emit teletextPossible( false );
    emit AtoBchanged( false, false );
    emit voutChanged( false );
    emit voutListChanged( NULL, 0 );

    /* Reset all InfoPanels but stats */
    emit artChanged( NULL );
    emit infoChanged( NULL );
    emit metaChanged( (input_item_t *)NULL );

    emit encryptionChanged( false );
    emit recordingStateChanged( false );
}

/* Convert the event from the callbacks in actions */
void InputManager::customEvent( QEvent *event )
{
    int i_type = event->type();
    IMEvent *ple = static_cast<IMEvent *>(event);

    if( !hasInput() )
        return;

#ifndef NDEBUG
    if( i_type != PositionUpdate_Type &&
        i_type != StatisticsUpdate_Type &&
        i_type != ItemChanged_Type )
        msg_Dbg( p_intf, "New Event: type %i", i_type );
#endif

    /* Actions */
    switch( i_type )
    {
    case PositionUpdate_Type:
        UpdatePosition();
        break;
    case StatisticsUpdate_Type:
        UpdateStats();
        break;
    case ItemChanged_Type:
        /* Ignore ItemChanged_Type event that does not apply to our input */
        if( i_input_id == ple->i_id )
        {
            UpdateStatus();
            // UpdateName();
            UpdateArt();
            /* Update duration of file */
        }
        UpdateMeta( ple->i_id );
        break;
    case ItemStateChanged_Type:
        // TODO: Fusion with above state
        UpdateStatus();
        // UpdateName();
        // UpdateNavigation(); This shouldn't be useful now
        // UpdateTeletext(); Same
        break;
    case NameChanged_Type:
        UpdateName();
        break;
    case MetaChanged_Type:
        UpdateMeta();
        UpdateName(); /* Needed for NowPlaying */
        UpdateArt(); /* Art is part of meta in the core */
        break;
    case InfoChanged_Type:
        UpdateInfo();
        break;
    case ItemTitleChanged_Type:
        UpdateNavigation();
        UpdateName(); /* Display the name of the Chapter, if exists */
        break;
    case ItemRateChanged_Type:
        UpdateRate();
        break;
    case ItemEsChanged_Type:
        UpdateTeletext();
        // We don't do anything ES related. Why ?
        break;
    case ItemTeletextChanged_Type:
        UpdateTeletext();
        break;
    case InterfaceVoutUpdate_Type:
        UpdateVout();
        break;
    case SynchroChanged_Type:
        emit synchroChanged();
        break;
    case CachingEvent_Type:
        UpdateCaching();
        break;
    case BookmarksChanged_Type:
        emit bookmarksChanged();
        break;
    case InterfaceAoutUpdate_Type:
        UpdateAout();
        break;
    case RecordingEvent_Type:
        UpdateRecord();
        break;
    case ProgramChanged_Type:
        UpdateProgramEvent();
        break;
    default:
        msg_Warn( p_intf, "This shouldn't happen: %i", i_type );
        assert(0);
    }
}

/* Add the callbacks on Input. Self explanatory */
inline void InputManager::addCallbacks()
{
    var_AddCallback( p_input, "intf-event", InputEvent, this );
}

/* Delete the callbacks on Input. Self explanatory */
inline void InputManager::delCallbacks()
{
    var_DelCallback( p_input, "intf-event", InputEvent, this );
}

/* Static callbacks for IM */
static int ItemChanged( vlc_object_t *p_this, const char *psz_var,
                        vlc_value_t oldval, vlc_value_t newval, void *param )
{
    InputManager *im = (InputManager*)param;

    IMEvent *event = new IMEvent( ItemChanged_Type, newval.i_int );
    QApplication::postEvent( im, event );
    return VLC_SUCCESS;
}

static int InputEvent( vlc_object_t *p_this, const char *,
                       vlc_value_t, vlc_value_t newval, void *param )
{
    InputManager *im = (InputManager*)param;
    IMEvent *event;

    switch( newval.i_int )
    {
    case INPUT_EVENT_STATE:
        event = new IMEvent( ItemStateChanged_Type, 0 );
        break;
    case INPUT_EVENT_RATE:
        event = new IMEvent( ItemRateChanged_Type, 0 );
        break;
    case INPUT_EVENT_TIMES:
        event = new IMEvent( PositionUpdate_Type, 0 );
        break;

    case INPUT_EVENT_TITLE:
    case INPUT_EVENT_CHAPTER:
        event = new IMEvent( ItemTitleChanged_Type, 0 );
        break;

    case INPUT_EVENT_ES:
        event = new IMEvent( ItemEsChanged_Type, 0 );
        break;
    case INPUT_EVENT_TELETEXT:
        event = new IMEvent( ItemTeletextChanged_Type, 0 );
        break;

    case INPUT_EVENT_STATISTICS:
        event = new IMEvent( StatisticsUpdate_Type, 0 );
        break;

    case INPUT_EVENT_VOUT:
        event = new IMEvent( InterfaceVoutUpdate_Type, 0 );
        break;
    case INPUT_EVENT_AOUT:
        event = new IMEvent( InterfaceAoutUpdate_Type, 0 );
        break;

    case INPUT_EVENT_ITEM_META: /* Codec MetaData + Art */
        event = new IMEvent( MetaChanged_Type, 0 );
        break;
    case INPUT_EVENT_ITEM_INFO: /* Codec Info */
        event = new IMEvent( InfoChanged_Type, 0 );
        break;
    case INPUT_EVENT_ITEM_NAME:
        event = new IMEvent( NameChanged_Type, 0 );
        break;

    case INPUT_EVENT_AUDIO_DELAY:
    case INPUT_EVENT_SUBTITLE_DELAY:
        event = new IMEvent( SynchroChanged_Type, 0 );
        break;

    case INPUT_EVENT_CACHE:
        event = new IMEvent( CachingEvent_Type, 0 );
        break;

    case INPUT_EVENT_BOOKMARK:
        event = new IMEvent( BookmarksChanged_Type, 0 );
        break;

    case INPUT_EVENT_RECORD:
        event = new IMEvent( RecordingEvent_Type, 0 );
        break;

    case INPUT_EVENT_PROGRAM:
        /* This is for PID changes */
        event = new IMEvent( ProgramChanged_Type, 0 );
        break;

    case INPUT_EVENT_SIGNAL:
        /* This is for capture-card signals */
        /* event = new IMEvent( SignalChanged_Type, 0 );
        break; */
    default:
        event = NULL;
        break;
    }

    if( event )
        QApplication::postEvent( im, event );
    return VLC_SUCCESS;
}

static int VbiEvent( vlc_object_t *, const char *,
                     vlc_value_t, vlc_value_t, void *param )
{
    InputManager *im = (InputManager*)param;
    IMEvent *event = new IMEvent( ItemTeletextChanged_Type, 0 );

    QApplication::postEvent( im, event );
    return VLC_SUCCESS;
}

void InputManager::UpdatePosition()
{
    /* Update position */
    int i_length, i_time; /* Int is enough, since we store seconds */
    float f_pos;
    i_length = var_GetTime(  p_input , "length" ) / 1000000;
    i_time = var_GetTime(  p_input , "time") / 1000000;
    f_pos = var_GetFloat(  p_input , "position" );
    emit positionUpdated( f_pos, i_time, i_length );
}

void InputManager::UpdateNavigation()
{
    /* Update navigation status */
    vlc_value_t val; val.i_int = 0;

    if( hasInput() )
        var_Change( p_input, "title", VLC_VAR_CHOICESCOUNT, &val, NULL );

    if( val.i_int > 0 )
    {
        emit titleChanged( true );
        msg_Dbg( p_intf, "Title %i", val.i_int );
        /* p_input != NULL since val.i_int != 0 */
        val.i_int = 0;
        var_Change( p_input, "chapter", VLC_VAR_CHOICESCOUNT, &val, NULL );
        emit chapterChanged( (val.i_int > 0) );
        msg_Dbg( p_intf, "Chapter: %i", val.i_int );
    }
    else
        emit titleChanged( false );
}

void InputManager::UpdateStatus()
{
    /* Update playing status */
    vlc_value_t val; val.i_int = 0;
    var_Get( p_input, "state", &val );
    if( i_old_playing_status != val.i_int )
    {
        i_old_playing_status = val.i_int;
        emit statusChanged( val.i_int );
    }
}

void InputManager::UpdateRate()
{
    /* Update Rate */
    int i_new_rate = var_GetInteger( p_input, "rate");
    if( i_new_rate != i_rate )
    {
        i_rate = i_new_rate;
        /* Update rate */
        emit rateChanged( i_rate );
    }
}

void InputManager::UpdateName()
{
    /* Update text, name and nowplaying */
    QString text;

    /* Try to get the Title, then the Name */
    char *psz_name = input_item_GetTitleFbName( input_GetItem( p_input ) );

    /* Try to get the nowplaying */
    char *psz_nowplaying =
        input_item_GetNowPlaying( input_GetItem( p_input ) );
    if( !EMPTY_STR( psz_nowplaying ) )
    {
        text.sprintf( "%s - %s", psz_nowplaying, psz_name );
    }
    else  /* Do it ourself */
    {
        char *psz_artist = input_item_GetArtist( input_GetItem( p_input ) );

        if( !EMPTY_STR( psz_artist ) )
            text.sprintf( "%s - %s", psz_artist, psz_name );
        else
            text.sprintf( "%s", psz_name );

        free( psz_artist );
    }
    /* Free everything */
    free( psz_name );
    free( psz_nowplaying );

    /* If we have Nothing */
    if( text.isEmpty() )
    {
        psz_name = input_item_GetURI( input_GetItem( p_input ) );
        text.sprintf( "%s", psz_name );
        text = text.remove( 0, text.lastIndexOf( DIR_SEP ) + 1 );
        free( psz_name );
    }

    if( oldName != text )
    {
        emit nameChanged( text );
        oldName = text;
    }
}

bool InputManager::hasAudio()
{
    if( hasInput() )
    {
        vlc_value_t val;
        var_Change( p_input, "audio-es", VLC_VAR_CHOICESCOUNT, &val, NULL );
        return val.i_int > 0;
    }
    return false;
}

void InputManager::UpdateTeletext()
{
    if( hasInput() )
    {
        const bool b_enabled = var_CountChoices( p_input, "teletext-es" ) > 0;
        const int i_teletext_es = var_GetInteger( p_input, "teletext-es" );

        /* Teletext is possible. Show the buttons */
        emit teletextPossible( b_enabled );

        /* If Teletext is selected */
        if( b_enabled && i_teletext_es >= 0 )
        {
            /* Then, find the current page */
            int i_page = 100;
            bool b_transparent = false;

            vlc_object_t *p_vbi = (vlc_object_t *)
                vlc_object_find_name( p_input, "zvbi", FIND_CHILD );

            if( p_vbi )
            {
                /* We deleted it (if not here, it does not harm), because
                 * var_AddCallback will silently add a duplicated one */
                var_DelCallback( p_vbi, "vbi-page", VbiEvent, this );
                /* This callback is not remove explicitly, but interfaces
                 * are guaranted to outlive input */
                var_AddCallback( p_vbi, "vbi-page", VbiEvent, this );

                i_page = var_GetInteger( p_vbi, "vbi-page" );
                b_transparent = !var_GetBool( p_vbi, "vbi-opaque" );
                vlc_object_release( p_vbi );
            }
            emit newTelexPageSet( i_page );
            emit teletextTransparencyActivated( b_transparent );

        }
        emit teletextActivated( b_enabled && i_teletext_es >= 0 );
    }
    else
    {
        emit teletextActivated( false );
        emit teletextPossible( false );
    }
}

void InputManager::UpdateVout()
{
    if( hasInput() )
    {
        /* Get current vout lists from input */
        int i_vout;
        vout_thread_t **pp_vout;
        if( input_Control( p_input, INPUT_GET_VOUTS, &pp_vout, &i_vout ) )
        {
            i_vout = 0;
            pp_vout = NULL;
        }

        /* */
        emit voutListChanged( pp_vout, i_vout );

        /* */
        bool b_old_video = b_video;
        b_video = i_vout > 0;
        if( !!b_old_video != !!b_video )
            emit voutChanged( b_video );

        /* Release the vout list */
        for( int i = 0; i < i_vout; i++ )
            vlc_object_release( (vlc_object_t*)pp_vout[i] );
        free( pp_vout );
    }
}
void InputManager::UpdateAout()
{
    if( hasInput() )
    {
        /* TODO */
    }
}
void InputManager::UpdateCaching()
{
    if(!hasInput()) return;

    float f_newCache = var_GetFloat ( p_input, "cache" );
    if( f_newCache != f_cache )
    {
        f_cache = f_newCache;
        /* Update rate */
        emit cachingChanged( f_cache );
    }
}

void InputManager::requestArtUpdate()
{
    if( hasInput() )
    {
        playlist_t *p_playlist = pl_Hold( p_intf );
        playlist_AskForArtEnqueue( p_playlist, input_GetItem( p_input ), pl_Unlocked );
        pl_Release( p_intf );
    }
    else
    {
        /* No input will signal the cover art to update,
         * let's do it ourself */
        UpdateArt();
    }
}

void InputManager::UpdateArt()
{
    QString url;

    if( hasInput() )
    {
        char *psz_art = input_item_GetArtURL( input_GetItem( p_input ) );
        if( psz_art && !strncmp( psz_art, "file://", 7 ) &&
                decode_URI( psz_art + 7 ) )
#ifdef WIN32
            url = qfu( psz_art + 8 ); // Remove extra / starting on Win32.
#else
            url = qfu( psz_art + 7 );
#endif
        free( psz_art );
    }

    /* Update Art meta */
    emit artChanged( url );
}

inline void InputManager::UpdateStats()
{
    emit statisticsUpdated( input_GetItem( p_input ) );
}

inline void InputManager::UpdateMeta( int id )
{
    emit metaChanged( id );
}

inline void InputManager::UpdateMeta()
{
    emit metaChanged( input_GetItem( p_input ) );
}

inline void InputManager::UpdateInfo()
{
    emit infoChanged( input_GetItem( p_input ) );
}

void InputManager::UpdateRecord()
{
    if( hasInput() )
    {
        emit recordingStateChanged( var_GetBool( p_input, "record" ) );
    }
}

void InputManager::UpdateProgramEvent()
{
    if( hasInput() )
    {
        bool b_scrambled = var_GetBool( p_input, "program-scrambled" );
        emit encryptionChanged( b_scrambled );
    }
}

/* User update of the slider */
void InputManager::sliderUpdate( float new_pos )
{
    if( hasInput() )
        var_SetFloat( p_input, "position", new_pos );
}

/* User togglePlayPause */
void InputManager::togglePlayPause()
{
    if( hasInput() )
    {
        int state = var_GetInteger( p_input, "state" );
        state = ( state != PLAYING_S ) ? PLAYING_S : PAUSE_S;
        var_SetInteger( p_input, "state", state );
        emit statusChanged( state );
    }
}

void InputManager::sectionPrev()
{
    if( hasInput() )
    {
        int i_type = var_Type( p_input, "next-chapter" );
        var_SetVoid( p_input, (i_type & VLC_VAR_TYPE) != 0 ?
                            "prev-chapter":"prev-title" );
    }
}

void InputManager::sectionNext()
{
    if( hasInput() )
    {
        int i_type = var_Type( p_input, "next-chapter" );
        var_SetVoid( p_input, (i_type & VLC_VAR_TYPE) != 0 ?
                            "next-chapter":"next-title" );
    }
}

void InputManager::sectionMenu()
{
    if( hasInput() )
    {
        vlc_value_t val, text;

        if( var_Change( p_input, "title  0", VLC_VAR_GETLIST, &val, &text ) < 0 )
            return;

        /* XXX is it "Root" or "Title" we want here ?" (set 0 by default) */
        int root = 0;
        for( int i = 0; i < val.p_list->i_count; i++ )
        {
            if( !strcmp( text.p_list->p_values[i].psz_string, "Title" ) )
                root = i;
        }
        var_Change( p_input, "title  0", VLC_VAR_FREELIST, &val, &text );

        var_SetInteger( p_input, "title  0", root );
    }
}

/*
 *  Teletext Functions
 */

/* Set a new Teletext Page */
void InputManager::telexSetPage( int page )
{
    if( hasInput() )
    {
        const int i_teletext_es = var_GetInteger( p_input, "teletext-es" );

        if( i_teletext_es >= 0 )
        {
            vlc_object_t *p_vbi = (vlc_object_t *) vlc_object_find_name( p_input,
                        "zvbi", FIND_CHILD );
            if( p_vbi )
            {
                var_SetInteger( p_vbi, "vbi-page", page );
                vlc_object_release( p_vbi );
                emit newTelexPageSet( page );
            }
        }
    }
}

/* Set the transparency on teletext */
void InputManager::telexSetTransparency( bool b_transparentTelextext )
{
    if( hasInput() )
    {
        vlc_object_t *p_vbi = (vlc_object_t *) vlc_object_find_name( p_input,
                    "zvbi", FIND_CHILD );
        if( p_vbi )
        {
            var_SetBool( p_vbi, "vbi-opaque", !b_transparentTelextext );
            vlc_object_release( p_vbi );
            emit teletextTransparencyActivated( b_transparentTelextext );
        }
    }
}

void InputManager::activateTeletext( bool b_enable )
{
    vlc_value_t list;
    vlc_value_t text;
    if( hasInput() && !var_Change( p_input, "teletext-es", VLC_VAR_GETLIST, &list, &text ) )
    {
        if( list.p_list->i_count > 0 )
        {
            /* Prefer the page 100 if it is present */
            int i;
            for( i = 0; i < text.p_list->i_count; i++ )
            {
                /* The description is the page number as a string */
                const char *psz_page = text.p_list->p_values[i].psz_string;
                if( psz_page && !strcmp( psz_page, "100" ) )
                    break;
            }
            if( i >= list.p_list->i_count )
                i = 0;
            var_SetInteger( p_input, "spu-es", b_enable ? list.p_list->p_values[i].i_int : -1 );
        }
        var_Change( p_input, "teletext-es", VLC_VAR_FREELIST, &list, &text );
    }
}

void InputManager::reverse()
{
    if( hasInput() )
    {
        int i_rate = var_GetInteger( p_input, "rate" );
        var_SetInteger( p_input, "rate", -i_rate );
    }
}

void InputManager::slower()
{
    if( hasInput() )
        var_SetVoid( p_input, "rate-slower" );
}

void InputManager::faster()
{
    if( hasInput() )
        var_SetVoid( p_input, "rate-faster" );
}

void InputManager::littlefaster()
{
    var_SetInteger( p_intf->p_libvlc, "key-action", ACTIONID_RATE_FASTER_FINE );
}

void InputManager::littleslower()
{
    var_SetInteger( p_intf->p_libvlc, "key-action", ACTIONID_RATE_SLOWER_FINE );
}

void InputManager::normalRate()
{
    if( hasInput() )
        var_SetInteger( p_input, "rate", INPUT_RATE_DEFAULT );
}

void InputManager::setRate( int new_rate )
{
    if( hasInput() )
        var_SetInteger( p_input, "rate", new_rate );
}

void InputManager::jumpFwd()
{
    int i_interval = config_GetInt( p_input, "short-jump-size" );
    if( i_interval > 0 )
    {
        mtime_t val = (mtime_t)(i_interval) * 1000000L;
        var_SetTime( p_input, "time-offset", val );
    }
}

void InputManager::jumpBwd()
{
    int i_interval = config_GetInt( p_input, "short-jump-size" );
    if( i_interval > 0 )
    {
        mtime_t val = -1 *(mtime_t)(i_interval) * 1000000L;
        var_SetTime( p_input, "time-offset", val );
    }
}

void InputManager::setAtoB()
{
    if( !timeA )
    {
        timeA = var_GetTime( THEMIM->getInput(), "time"  );
    }
    else if( !timeB )
    {
        timeB = var_GetTime( THEMIM->getInput(), "time"  );
        var_SetTime( THEMIM->getInput(), "time" , timeA );
        CONNECT( this, positionUpdated( float, int, int ),
                 this, AtoBLoop( float, int, int ) );
    }
    else
    {
        timeA = 0;
        timeB = 0;
        disconnect( this, SIGNAL( positionUpdated( float, int, int ) ),
                    this, SLOT( AtoBLoop( float, int, int ) ) );
    }
    emit AtoBchanged( (timeA != 0 ), (timeB != 0 ) );
}

/* Function called regularly when in an AtoB loop */
void InputManager::AtoBLoop( float, int i_time, int )
{
    if( timeB )
    {
        if( ( i_time >= (int)( timeB/1000000 ) )
            || ( i_time < (int)( timeA/1000000 ) ) )
            var_SetTime( THEMIM->getInput(), "time" , timeA );
    }
}

/**********************************************************************
 * MainInputManager implementation. Wrap an input manager and
 * take care of updating the main playlist input.
 * Used in the main playlist Dialog
 **********************************************************************/
MainInputManager * MainInputManager::instance = NULL;

MainInputManager::MainInputManager( intf_thread_t *_p_intf )
                 : QObject(NULL), p_intf( _p_intf )
{
    p_input = NULL;
    im = new InputManager( this, p_intf );

    var_AddCallback( THEPL, "item-change", ItemChanged, im );
    var_AddCallback( THEPL, "item-current", PLItemChanged, this );
    var_AddCallback( THEPL, "activity", PLItemChanged, this );

    var_AddCallback( p_intf->p_libvlc, "volume-change", VolumeChanged, this );

    /* Warn our embedded IM about input changes */
    CONNECT( this, inputChanged( input_thread_t * ),
             im, setInput( input_thread_t * ) );

    /* emit check if playlist has already started playing */
    IMEvent *event = new IMEvent( ItemChanged_Type,
                                  var_GetInteger( THEPL, "item-current" ) );
    customEvent( event );
    delete event;
}

MainInputManager::~MainInputManager()
{
    if( p_input )
    {
       emit inputChanged( NULL );
       var_DelCallback( p_input, "state", PLItemChanged, this );
       vlc_object_release( p_input );
    }

    var_DelCallback( p_intf->p_libvlc, "volume-change", VolumeChanged, this );

    var_DelCallback( THEPL, "activity", PLItemChanged, this );
    var_DelCallback( THEPL, "item-change", ItemChanged, im );

    var_DelCallback( THEPL, "item-current", PLItemChanged, this );
}

vout_thread_t* MainInputManager::getVout()
{
    return p_input ? input_GetVout( p_input ) : NULL;
}

aout_instance_t * MainInputManager::getAout()
{
    return p_input ? input_GetAout( p_input ) : NULL;
}

void MainInputManager::customEvent( QEvent *event )
{
    int type = event->type();
    if ( type != ItemChanged_Type && type != VolumeChanged_Type )
        return;

    // msg_Dbg( p_intf, "New MainIM Event of type: %i", type );
    if( type == VolumeChanged_Type )
    {
        emit volumeChanged();
        return;
    }

    /* Should be PLItemChanged Event */
    if( !p_intf->p_sys->b_isDialogProvider )
    {
        if( p_input && ( p_input->b_dead || !vlc_object_alive (p_input) ) )
        {
            emit inputChanged( p_input );
            var_DelCallback( p_input, "state", PLItemChanged, this );
            vlc_object_release( p_input );
            p_input = NULL;
            return;
        }

        if( !p_input )
        {
            p_input = playlist_CurrentInput(THEPL);
            if( p_input )
            {
                var_AddCallback( p_input, "state", PLItemChanged, this );
                emit inputChanged( p_input );
            }
        }
    }
    else
    {
        /* remove previous stored p_input */
        if( p_input )
        {
            vlc_object_release( p_input );
            p_input = NULL;
        }
        /* we are working as a dialogs provider */
        playlist_t *p_playlist = pl_Hold( p_intf );
        p_input = playlist_CurrentInput( p_playlist );
        if( p_input )
        {
            emit inputChanged( p_input );
        }
        pl_Release( p_intf );
    }
}

/* Playlist Control functions */
void MainInputManager::stop()
{
   playlist_Stop( THEPL );
}

void MainInputManager::next()
{
   playlist_Next( THEPL );
}

void MainInputManager::prev()
{
   playlist_Prev( THEPL );
}

void MainInputManager::togglePlayPause()
{
    /* No input, play */
    if( !p_input )
        playlist_Play( THEPL );
    else
        getIM()->togglePlayPause();
}

void MainInputManager::activatePlayQuit( bool b_exit )
{
    var_SetBool( THEPL, "play-and-exit", b_exit );
}

/* Static callbacks for MIM */
static int PLItemChanged( vlc_object_t *p_this, const char *psz_var,
                        vlc_value_t oldval, vlc_value_t newval, void *param )
{
    MainInputManager *mim = (MainInputManager*)param;

    IMEvent *event = new IMEvent( ItemChanged_Type, newval.i_int );
    QApplication::postEvent( mim, event );
    return VLC_SUCCESS;
}

static int VolumeChanged( vlc_object_t *p_this, const char *psz_var,
                        vlc_value_t oldval, vlc_value_t newval, void *param )
{
    MainInputManager *mim = (MainInputManager*)param;

    IMEvent *event = new IMEvent( VolumeChanged_Type, newval.i_int );
    QApplication::postEvent( mim, event );
    return VLC_SUCCESS;
}

