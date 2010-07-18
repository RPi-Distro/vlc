/*****************************************************************************
 * dialogs.cpp : wxWidgets plugin for vlc
 *****************************************************************************
 * Copyright (C) 2000-2004 the VideoLAN team
 * $Id: 9cd3673682165da2cc2196dfad942c1256c08213 $
 *
 * Authors: Gildas Bazin <gbazin@videolan.org>
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <stdlib.h>                                      /* malloc(), free() */
#include <errno.h>                                                 /* ENOMEM */
#include <string.h>                                            /* strerror() */
#include <stdio.h>

#include <vlc/vlc.h>
#include <vlc/aout.h>
#include <vlc/intf.h>

#include "charset.h"

#include "dialogs/vlm/vlm_panel.hpp"
#include "dialogs/bookmarks.hpp"
#include "dialogs/wizard.hpp"
#include "dialogs/playlist.hpp"
#include "dialogs/open.hpp"
#include "dialogs/updatevlc.hpp"
#include "dialogs/fileinfo.hpp"
#include "dialogs/iteminfo.hpp"
#include "dialogs/preferences.hpp"
#include "dialogs/messages.hpp"
#include "dialogs/interaction.hpp"
#include "interface.hpp"

/* include the icon graphic */
#include "../../../share/vlc32x32.xpm"

/* Dialogs Provider */
class DialogsProvider: public wxFrame
{
public:
    /* Constructor */
    DialogsProvider( intf_thread_t *p_intf, wxWindow *p_parent );
    virtual ~DialogsProvider();

private:
    void Open( int i_access_method, int i_arg );

    /* Event handlers (these functions should _not_ be virtual) */
    void OnUpdateVLC( wxCommandEvent& event );
    void OnVLM( wxCommandEvent& event );
    void OnInteraction( wxCommandEvent& event );
    void OnExit( wxCommandEvent& event );
    void OnPlaylist( wxCommandEvent& event );
    void OnMessages( wxCommandEvent& event );
    void OnFileInfo( wxCommandEvent& event );
    void OnPreferences( wxCommandEvent& event );
    void OnWizardDialog( wxCommandEvent& event );
    void OnBookmarks( wxCommandEvent& event );

    void OnOpenFileGeneric( wxCommandEvent& event );
    void OnOpenFileSimple( wxCommandEvent& event );
    void OnOpenDirectory( wxCommandEvent& event );
    void OnOpenFile( wxCommandEvent& event );
    void OnOpenDisc( wxCommandEvent& event );
    void OnOpenNet( wxCommandEvent& event );
    void OnOpenCapture( wxCommandEvent& event );
    void OnOpenSat( wxCommandEvent& event );

    void OnPopupMenu( wxCommandEvent& event );
    void OnAudioPopupMenu( wxCommandEvent& event );
    void OnVideoPopupMenu( wxCommandEvent& event );
    void OnMiscPopupMenu( wxCommandEvent& event );

    void OnIdle( wxIdleEvent& event );

    void OnExitThread( wxCommandEvent& event );

    DECLARE_EVENT_TABLE();

    intf_thread_t *p_intf;

public:
    /* Secondary windows */
    OpenDialog          *p_open_dialog;
    wxFileDialog        *p_file_dialog;
    wxDirDialog         *p_dir_dialog;
    Playlist            *p_playlist_dialog;
    Messages            *p_messages_dialog;
    FileInfo            *p_fileinfo_dialog;
    WizardDialog        *p_wizard_dialog;
    wxFrame             *p_prefs_dialog;
    wxFrame             *p_bookmarks_dialog;
    wxFileDialog        *p_file_generic_dialog;
    UpdateVLC           *p_updatevlc_dialog;
    VLMFrame            *p_vlm_dialog;
};

DEFINE_LOCAL_EVENT_TYPE( wxEVT_DIALOG );

BEGIN_EVENT_TABLE(DialogsProvider, wxFrame)
    /* Idle loop used to update some of the dialogs */
    EVT_IDLE(DialogsProvider::OnIdle)

    /* Custom wxDialog events */
    EVT_COMMAND(INTF_DIALOG_FILE, wxEVT_DIALOG, DialogsProvider::OnOpenFile)
    EVT_COMMAND(INTF_DIALOG_DISC, wxEVT_DIALOG, DialogsProvider::OnOpenDisc)
    EVT_COMMAND(INTF_DIALOG_NET, wxEVT_DIALOG, DialogsProvider::OnOpenNet)
    EVT_COMMAND(INTF_DIALOG_CAPTURE, wxEVT_DIALOG,
                DialogsProvider::OnOpenCapture)
    EVT_COMMAND(INTF_DIALOG_FILE_SIMPLE, wxEVT_DIALOG,
                DialogsProvider::OnOpenFileSimple)
    EVT_COMMAND(INTF_DIALOG_FILE_GENERIC, wxEVT_DIALOG,
                DialogsProvider::OnOpenFileGeneric)
    EVT_COMMAND(INTF_DIALOG_DIRECTORY, wxEVT_DIALOG,
                DialogsProvider::OnOpenDirectory)

    EVT_COMMAND(INTF_DIALOG_PLAYLIST, wxEVT_DIALOG,
                DialogsProvider::OnPlaylist)
    EVT_COMMAND(INTF_DIALOG_MESSAGES, wxEVT_DIALOG,
                DialogsProvider::OnMessages)
    EVT_COMMAND(INTF_DIALOG_PREFS, wxEVT_DIALOG,
                DialogsProvider::OnPreferences)
    EVT_COMMAND(INTF_DIALOG_WIZARD, wxEVT_DIALOG,
                DialogsProvider::OnWizardDialog)
    EVT_COMMAND(INTF_DIALOG_FILEINFO, wxEVT_DIALOG,
                DialogsProvider::OnFileInfo)
    EVT_COMMAND(INTF_DIALOG_BOOKMARKS, wxEVT_DIALOG,
                DialogsProvider::OnBookmarks)

    EVT_COMMAND(INTF_DIALOG_POPUPMENU, wxEVT_DIALOG,
                DialogsProvider::OnPopupMenu)
    EVT_COMMAND(INTF_DIALOG_AUDIOPOPUPMENU, wxEVT_DIALOG,
                DialogsProvider::OnAudioPopupMenu)
    EVT_COMMAND(INTF_DIALOG_VIDEOPOPUPMENU, wxEVT_DIALOG,
                DialogsProvider::OnVideoPopupMenu)
    EVT_COMMAND(INTF_DIALOG_MISCPOPUPMENU, wxEVT_DIALOG,
                DialogsProvider::OnMiscPopupMenu)

    EVT_COMMAND(INTF_DIALOG_EXIT, wxEVT_DIALOG,
                DialogsProvider::OnExitThread)
    EVT_COMMAND(INTF_DIALOG_UPDATEVLC, wxEVT_DIALOG,
                DialogsProvider::OnUpdateVLC)
    EVT_COMMAND(INTF_DIALOG_VLM, wxEVT_DIALOG,
                DialogsProvider::OnVLM)
    EVT_COMMAND( INTF_DIALOG_INTERACTION, wxEVT_DIALOG,
                DialogsProvider::OnInteraction )
END_EVENT_TABLE()

wxWindow *CreateDialogsProvider( intf_thread_t *p_intf, wxWindow *p_parent )
{
    return new DialogsProvider( p_intf, p_parent );
}

/*****************************************************************************
 * Constructor.
 *****************************************************************************/
DialogsProvider::DialogsProvider( intf_thread_t *_p_intf, wxWindow *p_parent )
  :  wxFrame( p_parent, -1, wxT("") )
{
    /* Initializations */
    p_intf = _p_intf;
    p_open_dialog = NULL;
    p_file_dialog = NULL;
    p_playlist_dialog = NULL;
    p_messages_dialog = NULL;
    p_fileinfo_dialog = NULL;
    p_prefs_dialog = NULL;
    p_file_generic_dialog = NULL;
    p_wizard_dialog = NULL;
    p_bookmarks_dialog = NULL;
    p_dir_dialog = NULL;
    p_updatevlc_dialog = NULL;
    p_vlm_dialog = NULL;

    /* Give our interface a nice little icon */
    p_intf->p_sys->p_icon = new wxIcon( vlc_xpm );

    /* Create the messages dialog so it can begin storing logs */
    p_messages_dialog = new Messages( p_intf, p_parent ? p_parent : this );

    /* Check if user wants to show the bookmarks dialog by default */
    wxCommandEvent dummy_event;
    if( config_GetInt( p_intf, "wx-bookmarks" ) )
        OnBookmarks( dummy_event );

    /* Intercept all menu events in our custom event handler */
    PushEventHandler( new MenuEvtHandler( p_intf, NULL ) );


    WindowSettings *ws = p_intf->p_sys->p_window_settings;
    wxPoint p;
    wxSize  s;
    bool    b_shown;

#define INIT( id, w, N, S ) \
    if( ws->GetSettings( WindowSettings::id, b_shown, p, s ) && b_shown ) \
    {                           \
        if( !w )                \
            w = N;              \
        w->SetSize( s );        \
        w->Move( p );           \
        w->S( true );           \
    }

    INIT( ID_PLAYLIST, p_playlist_dialog, new Playlist(p_intf,this), ShowPlaylist );
    INIT( ID_MESSAGES, p_messages_dialog, new Messages(p_intf,this), Show );
    INIT( ID_FILE_INFO, p_fileinfo_dialog, new FileInfo(p_intf,this), Show );
    INIT( ID_BOOKMARKS, p_bookmarks_dialog, new BookmarksDialog(p_intf,this), Show);
#undef INIT
}

DialogsProvider::~DialogsProvider()
{
    WindowSettings *ws = p_intf->p_sys->p_window_settings;

#define UPDATE(id,w)                                        \
  {                                                         \
    if( w && w->IsShown() && !w->IsIconized() )             \
        ws->SetSettings(  WindowSettings::id, true,         \
                          w->GetPosition(), w->GetSize() ); \
    else                                                    \
        ws->SetSettings(  WindowSettings::id, false );      \
  }

    UPDATE( ID_PLAYLIST,  p_playlist_dialog );
    UPDATE( ID_MESSAGES,  p_messages_dialog );
    UPDATE( ID_FILE_INFO, p_fileinfo_dialog );
    UPDATE( ID_BOOKMARKS, p_bookmarks_dialog );

#undef UPDATE

    PopEventHandler(true);

    /* Clean up */
    if( p_open_dialog )     delete p_open_dialog;
    if( p_prefs_dialog )    p_prefs_dialog->Destroy();
    if( p_file_dialog )     delete p_file_dialog;
    if( p_playlist_dialog ) delete p_playlist_dialog;
    if( p_messages_dialog ) delete p_messages_dialog;
    if( p_fileinfo_dialog ) delete p_fileinfo_dialog;
    if( p_file_generic_dialog ) delete p_file_generic_dialog;
    if( p_wizard_dialog ) delete p_wizard_dialog;
    if( p_bookmarks_dialog ) delete p_bookmarks_dialog;
    if( p_updatevlc_dialog ) delete p_updatevlc_dialog;
    if( p_vlm_dialog ) delete p_vlm_dialog;


    if( p_intf->p_sys->p_icon ) delete p_intf->p_sys->p_icon;

    /* We must set this here because on win32 this destructor will be
     * automatically called so we must not call it again on wxApp->OnExit().
     * There shouldn't be any race conditions as all this should be done
     * from the same thread. */
    p_intf->p_sys->p_wxwindow = NULL;
}

void DialogsProvider::OnIdle( wxIdleEvent& WXUNUSED(event) )
{
    /* Update the log window */
    if( p_messages_dialog )
        p_messages_dialog->UpdateLog();

    /* Update the playlist */
    if( p_playlist_dialog )
        p_playlist_dialog->UpdatePlaylist();

    /* Update the fileinfo windows */
    if( p_fileinfo_dialog )
        p_fileinfo_dialog->Update();
}

void DialogsProvider::OnPlaylist( wxCommandEvent& WXUNUSED(event) )
{
    /* Show/hide the playlist window */
    if( !p_playlist_dialog )
        p_playlist_dialog = new Playlist( p_intf, this );

    if( p_playlist_dialog )
    {
        p_playlist_dialog->ShowPlaylist( !p_playlist_dialog->IsShown() );
    }
}

void DialogsProvider::OnMessages( wxCommandEvent& WXUNUSED(event) )
{
    /* Show/hide the log window */
    if( !p_messages_dialog )
        p_messages_dialog = new Messages( p_intf, this );

    if( p_messages_dialog )
    {
        p_messages_dialog->Show( !p_messages_dialog->IsShown() );
    }
}

void DialogsProvider::OnFileInfo( wxCommandEvent& WXUNUSED(event) )
{
    /* Show/hide the file info window */
    if( !p_fileinfo_dialog )
        p_fileinfo_dialog = new FileInfo( p_intf, this );

    if( p_fileinfo_dialog )
    {
        p_fileinfo_dialog->Show( !p_fileinfo_dialog->IsShown() );
    }
}

void DialogsProvider::OnPreferences( wxCommandEvent& WXUNUSED(event) )
{
    /* Show/hide the open dialog */
    if( !p_prefs_dialog )
        p_prefs_dialog = new PrefsDialog( p_intf, this );

    if( p_prefs_dialog )
    {
        p_prefs_dialog->Show( !p_prefs_dialog->IsShown() );
    }
}

void DialogsProvider::OnWizardDialog( wxCommandEvent& WXUNUSED(event) )
{
    p_wizard_dialog = new WizardDialog( p_intf, this, NULL, 0, 0 );

    if( p_wizard_dialog )
    {
        p_wizard_dialog->Run();
        delete p_wizard_dialog;
    }

    p_wizard_dialog = NULL;
}

void DialogsProvider::OnBookmarks( wxCommandEvent& WXUNUSED(event) )
{
    /* Show/hide the open dialog */
    if( !p_bookmarks_dialog )
        p_bookmarks_dialog = new BookmarksDialog( p_intf, this );

    if( p_bookmarks_dialog )
    {
        p_bookmarks_dialog->Show( !p_bookmarks_dialog->IsShown() );
    }
}

void DialogsProvider::OnOpenFileGeneric( wxCommandEvent& event )
{
    intf_dialog_args_t *p_arg = (intf_dialog_args_t *)event.GetClientData();

    if( p_arg == NULL )
    {
        msg_Dbg( p_intf, "OnOpenFileGeneric() called with NULL arg" );
        return;
    }

    if( p_file_generic_dialog == NULL )
        p_file_generic_dialog = new wxFileDialog( NULL );

    if( p_file_generic_dialog )
    {
        p_file_generic_dialog->SetMessage( wxU(p_arg->psz_title) );
        p_file_generic_dialog->SetWildcard( wxU(p_arg->psz_extensions) );
        p_file_generic_dialog->SetWindowStyle( (p_arg->b_save ? wxSAVE : wxOPEN) |
                                         (p_arg->b_multiple ? wxMULTIPLE:0) );
    }

    if( p_file_generic_dialog &&
        p_file_generic_dialog->ShowModal() == wxID_OK )
    {
        wxArrayString paths;

        p_file_generic_dialog->GetPaths( paths );

        p_arg->i_results = paths.GetCount();
        p_arg->psz_results = (char **)malloc( p_arg->i_results *
                                              sizeof(char *) );
        for( size_t i = 0; i < paths.GetCount(); i++ )
        {
            p_arg->psz_results[i] = strdup( paths[i].mb_str(wxConvUTF8) );
        }
    }

    /* Callback */
    if( p_arg->pf_callback )
    {
        p_arg->pf_callback( p_arg );
    }

    if( p_arg->psz_results )
    {
        for( int i = 0; i < p_arg->i_results; i++ )
        {
            free( p_arg->psz_results[i] );
        }
        free( p_arg->psz_results );
    }
    if( p_arg->psz_title ) free( p_arg->psz_title );
    if( p_arg->psz_extensions ) free( p_arg->psz_extensions );

    free( p_arg );
}

void DialogsProvider::OnOpenFileSimple( wxCommandEvent& event )
{
    playlist_t *p_playlist =
        (playlist_t *)vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                       FIND_ANYWHERE );
    if( p_playlist == NULL )
    {
        return;
    }

    if( p_file_dialog == NULL )
        p_file_dialog = new wxFileDialog( NULL, wxU(_("Open File")),
            wxT(""), wxT(""), wxT("*"), wxOPEN | wxMULTIPLE );

	p_file_dialog->SetWildcard(wxU(_("All Files (*.*)|*"
        "|Sound Files (*.mp3, *.ogg, etc.)|" EXTENSIONS_AUDIO 
        "|Video Files (*.avi, *.mpg, etc.)|" EXTENSIONS_VIDEO 
        "|Playlist Files (*.m3u, *.pls, etc.)|" EXTENSIONS_PLAYLIST 
        "|Subtitle Files (*.srt, *.sub, etc.)|" EXTENSIONS_SUBTITLE)));

    if( p_file_dialog && p_file_dialog->ShowModal() == wxID_OK )
    {
        wxArrayString paths;

        p_file_dialog->GetPaths( paths );

        for( size_t i = 0; i < paths.GetCount(); i++ )
        {
            char *psz_utf8 = wxFromLocale( paths[i] );
            if( event.GetInt() )
                playlist_Add( p_playlist, psz_utf8, psz_utf8,
                              PLAYLIST_APPEND | (i ? 0 : PLAYLIST_GO) |
                              (i ? PLAYLIST_PREPARSE : 0 ),
                              PLAYLIST_END );
            else
                playlist_Add( p_playlist, psz_utf8, psz_utf8,
                              PLAYLIST_APPEND | PLAYLIST_PREPARSE , PLAYLIST_END );
            wxLocaleFree( psz_utf8 );
        }
    }

    vlc_object_release( p_playlist );
}

void DialogsProvider::OnOpenDirectory( wxCommandEvent& event )
{
    playlist_t *p_playlist =
        (playlist_t *)vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                       FIND_ANYWHERE );
    if( p_playlist == NULL )
    {
        return;
    }

    if( p_dir_dialog == NULL )
        p_dir_dialog = new wxDirDialog( NULL, wxU(_("Select a directory")) );

    if( p_dir_dialog && p_dir_dialog->ShowModal() == wxID_OK )
    {
        wxString path = p_dir_dialog->GetPath();
        char *psz_utf8 = wxFromLocale( path );
        playlist_Add( p_playlist, psz_utf8, psz_utf8,
                      PLAYLIST_APPEND | (event.GetInt() ? PLAYLIST_GO : 0),
                      PLAYLIST_END );
        wxLocaleFree( psz_utf8 );
    }

    vlc_object_release( p_playlist );
}

void DialogsProvider::OnOpenFile( wxCommandEvent& event )
{
    Open( FILE_ACCESS, event.GetInt() );
}

void DialogsProvider::OnOpenDisc( wxCommandEvent& event )
{
    Open( DISC_ACCESS, event.GetInt() );
}

void DialogsProvider::OnOpenNet( wxCommandEvent& event )
{
    Open( NET_ACCESS, event.GetInt() );
}

void DialogsProvider::OnOpenCapture( wxCommandEvent& event )
{
    Open( CAPTURE_ACCESS, event.GetInt() );
}

void DialogsProvider::Open( int i_access_method, int i_arg )
{
    /* Show/hide the open dialog */
    if( !p_open_dialog )
        p_open_dialog = new OpenDialog( p_intf, this, i_access_method, i_arg,
                                        OPEN_NORMAL );

    if( p_open_dialog )
    {
        p_open_dialog->Show( i_access_method, i_arg );
    }
}

void DialogsProvider::OnPopupMenu( wxCommandEvent& event )
{
    wxPoint mousepos = ScreenToClient( wxGetMousePosition() );
    ::PopupMenu( p_intf, this, mousepos );
}

void DialogsProvider::OnAudioPopupMenu( wxCommandEvent& event )
{
    wxPoint mousepos = ScreenToClient( wxGetMousePosition() );
    ::AudioPopupMenu( p_intf, this, mousepos );
}
void DialogsProvider::OnVideoPopupMenu( wxCommandEvent& event )
{
    wxPoint mousepos = ScreenToClient( wxGetMousePosition() );
    ::VideoPopupMenu( p_intf, this, mousepos );
}
void DialogsProvider::OnMiscPopupMenu( wxCommandEvent& event )
{
    wxPoint mousepos = ScreenToClient( wxGetMousePosition() );
    ::MiscPopupMenu( p_intf, this, mousepos );
}

void DialogsProvider::OnExitThread( wxCommandEvent& WXUNUSED(event) )
{
    wxTheApp->ExitMainLoop();
}

void DialogsProvider::OnUpdateVLC( wxCommandEvent& WXUNUSED(event) )
{
    /* Show/hide the file info window */
    /*if( !p_updatevlc_dialog )
        p_updatevlc_dialog = new UpdateVLC( p_intf, this );*/

    if( p_updatevlc_dialog )
    {
        p_updatevlc_dialog->Show( !p_updatevlc_dialog->IsShown() );
    }
}

void DialogsProvider::OnVLM( wxCommandEvent& WXUNUSED(event) )
{
    /* Show/hide the file info window */
    if( !p_vlm_dialog )
        p_vlm_dialog = new VLMFrame( p_intf, this );

    if( p_vlm_dialog )
    {
        p_vlm_dialog->Show( !p_vlm_dialog->IsShown() );
    }
}

void DialogsProvider::OnInteraction( wxCommandEvent& event )
{
    intf_dialog_args_t *p_arg = (intf_dialog_args_t *)event.GetClientData();
    interaction_dialog_t *p_dialog;
    InteractionDialog *p_wxdialog;

    if( p_arg == NULL )
    {
        msg_Dbg( p_intf, "OnInteraction() called with NULL arg" );
        return;
    }
    p_dialog = p_arg->p_dialog;

    /** \bug We store the interface object for the dialog in the p_private
     * field of the core dialog object. This is not safe if we change
     * interface while a dialog is loaded */

    switch( p_dialog->i_action )
    {
    case INTERACT_NEW:
        p_wxdialog = new InteractionDialog( p_intf, this, p_dialog );
        p_dialog->p_private = (void*)p_wxdialog;
        p_wxdialog->Show();
        break;
    case INTERACT_UPDATE:
        p_wxdialog = (InteractionDialog*)(p_dialog->p_private);
        if( p_wxdialog)
            p_wxdialog->Update();
        break;
    case INTERACT_HIDE:
        p_wxdialog = (InteractionDialog*)(p_dialog->p_private);
        if( p_wxdialog )
            p_wxdialog->Hide();
        p_dialog->i_status = HIDDEN_DIALOG;
        break;
    case INTERACT_DESTROY:
        p_wxdialog = (InteractionDialog*)(p_dialog->p_private);
        /// \todo
        p_dialog->i_status = DESTROYED_DIALOG;
        break;
    }
}
