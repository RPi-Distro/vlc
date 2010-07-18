/*****************************************************************************
 * vlm_panel.cpp: VLM Panel
 *****************************************************************************
 * Copyright (C) 2000-2005 the VideoLAN team
 * $Id: 8305505fb3bd31e082cba0bf9165418b74c80422 $
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
 *
 * This program is free software; you can redistribute it and/OR MODIFy
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

#include "dialogs/vlm/vlm_panel.hpp"
#include "dialogs/vlm/vlm_wrapper.hpp"
#include "dialogs/vlm/vlm_stream.hpp"
#include "dialogs/vlm/vlm_streampanel.hpp"
#include <wx/statline.h>

#include "charset.h"

enum
{
    Notebook_Event,
    Timer_Event,
    Load_Event,
};

BEGIN_EVENT_TABLE( VLMPanel, wxPanel)
   EVT_TIMER( Timer_Event, VLMPanel::OnTimer )
   EVT_BUTTON( wxID_CLOSE, VLMPanel::OnClose )
   EVT_BUTTON( Load_Event, VLMPanel::OnLoad )
   EVT_BUTTON( wxID_SAVE, VLMPanel::OnSave )
END_EVENT_TABLE()


VLMPanel::VLMPanel( intf_thread_t *_p_intf, wxWindow *_p_parent ) :
        wxPanel( _p_parent, -1, wxDefaultPosition, wxDefaultSize ),
       timer( this, Timer_Event )
{
    p_intf = _p_intf;
    p_parent = _p_parent;

    p_vlm = new VLMWrapper( p_intf );
    p_vlm->AttachVLM();

    SetAutoLayout( TRUE );

    wxBoxSizer *panel_sizer = new wxBoxSizer( wxVERTICAL );

    p_notebook = new wxNotebook( this, Notebook_Event );
#if (!wxCHECK_VERSION(2,5,0))
    wxNotebookSizer *notebook_sizer = new wxNotebookSizer( p_notebook );
#endif
    p_notebook->AddPage( BroadcastPanel( p_notebook ), wxU( _("Broadcasts") ) );
#if 0
    p_notebook->AddPage( VODPanel( p_notebook ), wxU( _("VOD") ) );
#endif
#if (!wxCHECK_VERSION(2,5,0))
    panel_sizer->Add( notebook_sizer, 1, wxEXPAND | wxALL, 5 );
#else
    panel_sizer->Add( p_notebook, 1 , wxEXPAND | wxALL, 5 );
#endif

    wxBoxSizer *button_sizer = new wxBoxSizer( wxHORIZONTAL );
    button_sizer->Add( new wxButton( this, wxID_CLOSE, wxU(_("&Close") ) ) );
    button_sizer->Add( 0, 0, 1 );
    button_sizer->Add( new wxButton( this, Load_Event, wxU(_("Load") ) ), 0, wxRIGHT, 10 );
    button_sizer->Add( new wxButton( this, wxID_SAVE, wxU(_("&Save") ) ) );
    panel_sizer->Add( button_sizer, 0 , wxEXPAND | wxALL, 5 );

    panel_sizer->Layout();
    SetSizerAndFit( panel_sizer );

    Update();

    timer.Start( 300 );
}

VLMPanel::~VLMPanel()
{
    delete p_vlm;
}

void VLMPanel::OnTimer( wxTimerEvent& event )
{
    Update();
}

void VLMPanel::Update()
{
    unsigned int i;
    for( i = 0 ; i < broadcasts.size(); i++ )
    {
        ((VLMBroadcastStreamPanel *)broadcasts[i])->b_found = VLC_FALSE;
    }
    for( i = 0 ; i < vods.size(); i++ )
    {
        ((VLMVODStreamPanel *)vods[i])->b_found = VLC_FALSE;
    }

    p_vlm->LockVLM();
    /* Iterate over the media, to find the panels to add / remove */
    /* FIXME: This code should be better wrapped */
    for( i = 0; i < p_vlm->NbMedia(); i++ )
    {
        vlm_media_t *p_media = p_vlm->GetMedia( i );

        if( p_media->i_type == BROADCAST_TYPE )
        {
            vlc_bool_t b_foundthis = VLC_FALSE;
            for( unsigned int j = 0 ; j < broadcasts.size(); j++ )
            {
                VLMBroadcastStreamPanel *p_streamp =
                       (VLMBroadcastStreamPanel *)(broadcasts[j]);
                /* FIXME: dangerous .. */
                if( p_streamp->GetStream()->p_media ==  p_media )
                {
                    p_streamp->b_found = VLC_TRUE;
                    b_foundthis = VLC_TRUE;
                    break;
                }
            }
            /* Create the stream */
            if( !b_foundthis )
            {
                VLMBroadcastStream *p_broadcast =
                        new VLMBroadcastStream( p_intf, p_media, p_vlm );
                AppendBroadcast( p_broadcast );
            }
        }
        else if( p_media->i_type == VOD_TYPE )
        {
            vlc_bool_t b_foundthis = VLC_FALSE;
            for( unsigned int j = 0 ; i < vods.size(); i++ )
            {
                VLMVODStreamPanel *p_streamp = (VLMVODStreamPanel *)( vods[j] );
                if(  p_streamp->GetStream()->p_media ==  p_media )
                {
                    p_streamp->b_found = VLC_TRUE;
                    b_foundthis = VLC_TRUE;
                    break;
                }
            }
            /* Create it */
            if( !b_foundthis )
            {
                VLMVODStream *p_vod = new VLMVODStream(p_intf, p_media, p_vlm );
                AppendVOD( p_vod );
            }
        }
    }

    /* Those not marked as found must be removed */
    vector<VLMBroadcastStreamPanel *>::iterator it = broadcasts.begin();
    while( it < broadcasts.end() )
    {
        if( (*it)->b_found == VLC_FALSE )
        {
            vector<VLMBroadcastStreamPanel *>::iterator rem = it;
            it++;
            VLMBroadcastStreamPanel *p_remove = *rem;
            broadcasts.erase( rem );
            RemoveBroadcast( p_remove );
            delete p_remove;
        }
        else
            it++;
    }
    vector<VLMVODStreamPanel *>::iterator it2 = vods.begin();
    while( it2 < vods.end() )
    {
        if( (*it2)->b_found == VLC_FALSE )
        {
            vector<VLMVODStreamPanel *>::iterator rem = it2;
            it2++;
            VLMVODStreamPanel *p_remove = *rem;
            vods.erase( rem );
            RemoveVOD( p_remove );
            delete p_remove;
        }
        else
            it2++;
    }

    /* Update sliders*/
    for( unsigned int j = 0 ; j < broadcasts.size(); j++ )
    {
        VLMBroadcastStreamPanel *p_streamp =
                (VLMBroadcastStreamPanel *)( broadcasts[j] );
        p_streamp->Update();
    }
    p_vlm->UnlockVLM();
}

void VLMPanel::OnClose( wxCommandEvent &event )
{
    ((VLMFrame*)p_parent)->OnClose( *( new wxCloseEvent() ) );
}

void VLMPanel::OnLoad( wxCommandEvent &event )
{
    p_file_dialog = new wxFileDialog( NULL, wxT(""), wxT(""), wxT(""),
                                      wxT("*"), wxOPEN | wxMULTIPLE );
    if( p_file_dialog == NULL ) return;

    p_file_dialog->SetTitle( wxU(_("Load Configuration") ) );
    if( p_file_dialog->ShowModal() == wxID_OK )
    {
        vlm_Load( p_vlm->GetVLM(), p_file_dialog->GetPath().mb_str(wxConvUTF8));
    }
    Update();
}

void VLMPanel::OnSave( wxCommandEvent &event )
{
    p_file_dialog = new wxFileDialog( NULL, wxT(""), wxT(""), wxT(""),
                                      wxT("*"), wxSAVE | wxOVERWRITE_PROMPT );
    if( p_file_dialog == NULL ) return;

    p_file_dialog->SetTitle( wxU(_("Save Configuration") ) );
    if( p_file_dialog->ShowModal() == wxID_OK )
    {
        vlm_Save( p_vlm->GetVLM(), p_file_dialog->GetPath().mb_str(wxConvUTF8));
    }
}

/*************************
 * Broadcasts management
 *************************/
wxPanel * VLMPanel::BroadcastPanel( wxWindow *parent )
{
     broadcasts_panel = new wxPanel( parent, -1, wxDefaultPosition, wxSize( 500, 350 ) );
     broadcasts_sizer = new wxBoxSizer( wxVERTICAL );

     wxStaticBox *add_box = new wxStaticBox( broadcasts_panel, -1,
                                            wxU( _( "New broadcast") ) );
     wxStaticBoxSizer *box_sizer = new wxStaticBoxSizer( add_box, wxHORIZONTAL );
     box_sizer->Add( AddBroadcastPanel( broadcasts_panel), 0, wxEXPAND|wxALL, 5 );
     box_sizer->Layout();

     broadcasts_sizer->Add( box_sizer, 0, wxEXPAND|wxALL, 5 );

     wxStaticLine *static_line = new wxStaticLine( broadcasts_panel, wxID_ANY );
     broadcasts_sizer->Add( static_line, 0, wxEXPAND | wxALL, 5 );

     scrolled_broadcasts = new wxScrolledWindow( broadcasts_panel, -1,
                                wxDefaultPosition, wxDefaultSize,
                                wxBORDER_NONE | wxVSCROLL );

     scrolled_broadcasts_sizer = new wxBoxSizer( wxVERTICAL );
     scrolled_broadcasts->SetAutoLayout( TRUE );
     scrolled_broadcasts->SetScrollRate( 5,5 );
     scrolled_broadcasts->SetSizerAndFit( scrolled_broadcasts_sizer );

     broadcasts_sizer->Add( scrolled_broadcasts, 1, wxEXPAND| wxALL, 5 );
     broadcasts_sizer->Layout();

     broadcasts_panel->SetSizerAndFit( broadcasts_sizer );

     return broadcasts_panel;
}

wxPanel * VLMPanel::AddBroadcastPanel( wxPanel *panel )
{
     return new VLMAddStreamPanel( p_intf, panel, p_vlm, VLC_FALSE,
                                   VLC_TRUE );
}

void VLMPanel::AppendBroadcast( VLMBroadcastStream *p_broadcast )
{
    VLMBroadcastStreamPanel *p_new =
                   new VLMBroadcastStreamPanel( p_intf, scrolled_broadcasts,
                                                p_broadcast );
    p_new->b_found = VLC_TRUE;
    scrolled_broadcasts_sizer->Add( p_new, 0, wxEXPAND | wxALL, 5 );
    scrolled_broadcasts_sizer->Layout();
    scrolled_broadcasts->FitInside();
    broadcasts.push_back( p_new );
}

void VLMPanel::RemoveBroadcast( VLMBroadcastStreamPanel *p_streamp )
{
    scrolled_broadcasts_sizer->Remove( p_streamp );
    scrolled_broadcasts_sizer->Layout();
    scrolled_broadcasts->FitInside();
}

/*************************
 * VODS management
 *************************/
wxPanel * VLMPanel::VODPanel( wxWindow *parent )
{
     vods_panel = new wxPanel( parent, -1, wxDefaultPosition, wxSize( 500, 350 ) );
     return vods_panel;
}

wxPanel * VLMPanel::AddVODPanel( wxPanel *panel )
{
     return new VLMAddStreamPanel( p_intf, panel, p_vlm, VLC_FALSE,
                                   VLC_FALSE );
}

void VLMPanel::AppendVOD( VLMVODStream *p_vod )
{
    VLMVODStreamPanel *p_new =
                   new VLMVODStreamPanel( p_intf, scrolled_vods, p_vod );
    p_new->b_found = VLC_TRUE;
    scrolled_vods_sizer->Add( p_new, 0, wxEXPAND | wxALL, 5 );
    scrolled_vods_sizer->Layout();
    scrolled_vods->FitInside();
    vods.push_back( p_new );
}

void VLMPanel::RemoveVOD( VLMVODStreamPanel *p_streamp )
{
    scrolled_vods_sizer->Remove( p_streamp );
    scrolled_vods_sizer->Layout();
    scrolled_vods->FitInside();
}

/****************************************************************************
 * VLM Add Broadcast panel implementation
 *****************************************************************************/
enum
{
    Create_Event,
    Clear_Event,
    ChooseInput_Event,
    ChooseOutput_Event,
};

BEGIN_EVENT_TABLE( VLMAddStreamPanel, wxPanel)
   EVT_BUTTON( Create_Event, VLMAddStreamPanel::OnCreate )
   EVT_BUTTON( Clear_Event, VLMAddStreamPanel::OnClear )
   EVT_BUTTON( ChooseInput_Event, VLMAddStreamPanel::OnChooseInput )
   EVT_BUTTON( ChooseOutput_Event, VLMAddStreamPanel::OnChooseOutput )
END_EVENT_TABLE()

VLMAddStreamPanel::VLMAddStreamPanel( intf_thread_t *_p_intf,
                wxWindow *_p_parent, VLMWrapper *_p_vlm,
                vlc_bool_t _b_edit, vlc_bool_t _b_broadcast ):
                wxPanel( _p_parent, -1, wxDefaultPosition, wxDefaultSize )
{
    p_intf = _p_intf;
    p_parent = _p_parent;
    p_vlm = _p_vlm;
    b_edit = _b_edit;
    b_broadcast = _b_broadcast;

    p_open_dialog = NULL;
    p_sout_dialog = NULL;

    SetAutoLayout( TRUE );

    wxBoxSizer *panel_sizer = new wxBoxSizer( wxVERTICAL );

    wxFlexGridSizer *upper_sizer = new wxFlexGridSizer( 5, 2 );
    upper_sizer->AddGrowableCol( 1 );
    upper_sizer->AddGrowableCol( 3 );

    upper_sizer->Add( new wxStaticText( this, -1, wxU( _("Name") ) ), 0,
                                        wxALIGN_CENTER_VERTICAL, 0 );
    name_text = new wxTextCtrl( this, -1, wxU( "" ), wxDefaultPosition,
                                          wxSize( 150, -1 ) );
    upper_sizer->Add( name_text , 1, wxEXPAND | wxLEFT | wxRIGHT, 5 );

    upper_sizer->Add( new wxStaticText( this, -1, wxU( _("Input") ) ), 0,
                                        wxALIGN_CENTER_VERTICAL, 0 );
    input_text = new wxTextCtrl( this, -1, wxU( "" ) ,
                      wxDefaultPosition, wxSize( 150, -1 ) );
    upper_sizer->Add( input_text , 1, wxEXPAND | wxLEFT | wxRIGHT, 5 );
    upper_sizer->Add( new wxButton( this, ChooseInput_Event, wxU( _("Choose") )  ) );

    upper_sizer->Add( 0,0 );
    upper_sizer->Add( 0,0 );

    upper_sizer->Add( new wxStaticText( this, -1, wxU( _("Output") ) ), 0,
                                        wxALIGN_CENTER_VERTICAL, 0 );
    output_text = new wxTextCtrl( this, -1, wxU( "" ) ,
                      wxDefaultPosition, wxSize( 150, -1 ) );
    upper_sizer->Add( output_text, 1, wxEXPAND | wxLEFT | wxRIGHT, 5 );
    upper_sizer->Add( new wxButton( this, ChooseOutput_Event,
                      wxU( _("Choose") )  ) );

    panel_sizer->Add( upper_sizer, 0, wxEXPAND | wxALL, 5 );

    wxBoxSizer *lower_sizer = new wxBoxSizer( wxHORIZONTAL );
    enabled_checkbox = new wxCheckBox( this, -1, wxU( _("Enabled" ) ) );
    enabled_checkbox->SetValue( true );
    lower_sizer->Add( enabled_checkbox,  1 , wxEXPAND | wxALL , 5 );
    if( b_broadcast )
    {
        loop_checkbox = new wxCheckBox( this, -1, wxU( _("Loop" ) ) );
        lower_sizer->Add( loop_checkbox,  1 , wxEXPAND | wxALL , 5 );
    }

    if( !b_edit )
    {
        lower_sizer->Add( new wxButton( this, wxID_CLEAR,
                          wxU( _( "&Clear" )  ) ),
                         0 , wxEXPAND | wxALL , 5 );
    }
    lower_sizer->Add( new wxButton( this, Create_Event,
                          wxU( _( b_edit ? "OK" : "Create" ) ) ),
                      0 , wxEXPAND | wxALL , 5 );


    panel_sizer->Add( lower_sizer, 0 , wxEXPAND| wxALL, 5 );
    panel_sizer->Layout();
    SetSizerAndFit( panel_sizer );
}

VLMAddStreamPanel::~VLMAddStreamPanel()
{
}

void VLMAddStreamPanel::Load( VLMStream *p_stream )
{
    name_text->SetValue( wxU( p_stream->p_media->psz_name ) );
    name_text->SetEditable( false );
    if( p_stream->p_media->i_input > 0 )
    {
        input_text->SetValue( wxU( p_stream->p_media->input[0] ) );
    }
    output_text->SetValue( wxU( p_stream->p_media->psz_output ) );
    enabled_checkbox->SetValue( p_stream->p_media->b_enabled );
    if( b_broadcast)
        loop_checkbox->SetValue( p_stream->p_media->b_loop );
}

void VLMAddStreamPanel::OnCreate( wxCommandEvent &event )
{
    char *psz_name = wxFromLocale( name_text->GetValue() );
    char *psz_input = wxFromLocale(  input_text->GetValue() );
    char *psz_output = wxFromLocale( output_text->GetValue() );
    if( b_broadcast && ! b_edit )
    {
        p_vlm->AddBroadcast( psz_name, psz_input, psz_output,
                         enabled_checkbox->IsChecked() ? VLC_TRUE: VLC_FALSE,
                         loop_checkbox->IsChecked() ? VLC_TRUE : VLC_FALSE );
    }
    else if( b_broadcast && b_edit )
    {
        p_vlm->EditBroadcast( psz_name, psz_input, psz_output,
                        enabled_checkbox->IsChecked() ? VLC_TRUE: VLC_FALSE,
                        loop_checkbox->IsChecked() ? VLC_TRUE : VLC_FALSE );
    }
    else if( !b_broadcast && !b_edit )
    {
        p_vlm->AddVod( psz_name, psz_input, psz_output,
                       enabled_checkbox->IsChecked() ? VLC_TRUE: VLC_FALSE,
                       loop_checkbox->IsChecked() ? VLC_TRUE : VLC_FALSE );
    }
    else
    {
        p_vlm->EditVod( psz_name, psz_input, psz_output,
                        enabled_checkbox->IsChecked() ? VLC_TRUE: VLC_FALSE,
                        loop_checkbox->IsChecked() ? VLC_TRUE : VLC_FALSE );
    }
    wxLocaleFree( psz_name) ; wxLocaleFree( psz_input ) ;
    wxLocaleFree( psz_output);
    if( !b_edit )
        OnClear( event );
    if( b_edit )
        p_parent->Hide();
}

void VLMAddStreamPanel::OnClear( wxCommandEvent &event )
{
    name_text->SetValue( wxU("") );
    input_text->SetValue( wxU("") );
    output_text->SetValue( wxU("") );
}

void VLMAddStreamPanel::OnChooseInput( wxCommandEvent &event )
{
    if( p_open_dialog == NULL )
        p_open_dialog = new OpenDialog( p_intf, this, -1, -1, OPEN_STREAM );

    if( p_open_dialog && p_open_dialog->ShowModal() == wxID_OK )
    {
        input_text->SetValue( p_open_dialog->mrl[0] );
    }
}

void VLMAddStreamPanel::OnChooseOutput( wxCommandEvent &event )
{
    if( p_sout_dialog == NULL )
        p_sout_dialog = new SoutDialog( p_intf, this );

    if( p_sout_dialog && p_sout_dialog->ShowModal() == wxID_OK )
    {
        wxString sout = (p_sout_dialog->GetOptions())[0] ;
        sout = sout.AfterFirst( '=' );
        output_text->SetValue( sout );
    }
}

/****************************************************************************
 * VLM Frame implementation
 ****************************************************************************/
enum
{
};

BEGIN_EVENT_TABLE( VLMFrame, wxFrame )
    EVT_CLOSE( VLMFrame::OnClose )
END_EVENT_TABLE()

VLMFrame::VLMFrame( intf_thread_t *_p_intf, wxWindow *_p_parent ) :
        wxFrame( _p_parent, -1, wxU( _("VLM") ),
        wxDefaultPosition, wxSize( 640,480 ), wxDEFAULT_FRAME_STYLE )
{
    SetIcon( *_p_intf->p_sys->p_icon );

    wxBoxSizer *main_sizer = new wxBoxSizer( wxHORIZONTAL );
    vlm_panel = new VLMPanel( _p_intf, this );

#if defined(WIN32)
    main_sizer->Add( vlm_panel, 1, wxGROW, 0 );
#else
    main_sizer->Add( vlm_panel, 1, wxEXPAND | wxALL, 5 );
#endif
    main_sizer->Layout();
    SetSizerAndFit( main_sizer );
}

void VLMFrame::OnClose( wxCloseEvent& WXUNUSED(event) )
{
    Hide();
}

void VLMFrame::Update()
{
    vlm_panel->Update();
}

VLMFrame::~VLMFrame()
{
    delete vlm_panel;
}

/****************************************************************************
 * VLM Add stream Frame implementation
 ****************************************************************************/
VLMEditStreamFrame::VLMEditStreamFrame( intf_thread_t *_p_intf,
            wxWindow *_p_parent, VLMWrapper *_p_vlm, vlc_bool_t _b_broadcast,
            VLMStream *p_stream ) :
        wxFrame( _p_parent, -1, wxU( _("VLM stream") ),
        wxDefaultPosition, wxSize( 640,480 ), wxDEFAULT_FRAME_STYLE )
{
    SetIcon( *_p_intf->p_sys->p_icon );

    wxBoxSizer *main_sizer = new wxBoxSizer( wxHORIZONTAL );
    vlm_panel = new VLMAddStreamPanel( _p_intf, this, _p_vlm ,
                                       VLC_TRUE, _b_broadcast );

    vlm_panel->Load( p_stream );

    main_sizer->Add( vlm_panel, 1, wxEXPAND | wxALL, 5 );
    main_sizer->Layout();
    SetSizerAndFit( main_sizer );
}

VLMEditStreamFrame::~VLMEditStreamFrame()
{
}
