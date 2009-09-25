/*****************************************************************************
 * MessagesWindow.cpp: beos interface
 *****************************************************************************
 * Copyright (C) 1999, 2000, 2001 the VideoLAN team
 * $Id: b4b202933004508353f3f7c23b2c3c7999ae58ec $
 *
 * Authors: Eric Petit <titer@videolan.org>
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

/* BeOS headers */
#include <InterfaceKit.h>
#include <SupportKit.h>

/* VLC headers */
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_interface.h>

/* BeOS module headers */
#include "InterfaceWindow.h"
#include "MessagesWindow.h"

/*****************************************************************************
 * MessagesView::Pulse
 *****************************************************************************/
void MessagesView::Pulse()
{
    bool isScrolling = false;
    if( fScrollBar->LockLooper() )
    {
        float min, max;
        fScrollBar->GetRange( &min, &max );
        if( fScrollBar->Value() != max )
            isScrolling = true;
        fScrollBar->UnlockLooper();

    }

    int i_start, oldLength;
    const char * psz_module_type = NULL;
    rgb_color red = { 200, 0, 0 };
    rgb_color gray = { 150, 150, 150 };
    rgb_color green = { 0, 150, 0 };
    rgb_color orange = { 230, 180, 00 };
    rgb_color color;

    vlc_mutex_lock( p_sub->p_lock );
    int i_stop = *p_sub->pi_stop;
    vlc_mutex_unlock( p_sub->p_lock );

    if( p_sub->i_start != i_stop )
    {
        for( i_start = p_sub->i_start;
             i_start != i_stop;
                 i_start = (i_start+1) % VLC_MSG_QSIZE )
        {
            /* Add message */
            switch( p_sub->p_msg[i_start].i_type )
            {
                case VLC_MSG_INFO: color = green; break;
                case VLC_MSG_WARN: color = orange; break;
                case VLC_MSG_ERR: color = red; break;
                case VLC_MSG_DBG: color = gray; break;
            }

            psz_module_type = p_sub->p_msg[i_start].psz_object_type;

            if( LockLooper() )
            {
                oldLength = TextLength();
                BString string;
                string << p_sub->p_msg[i_start].psz_module
                    << " " << psz_module_type << " : "
                    << p_sub->p_msg[i_start].psz_msg << "\n";
                Insert( TextLength(), string.String(), strlen( string.String() ) );
                SetFontAndColor( oldLength, TextLength(), NULL, 0, &color );
                Draw( Bounds() );
                UnlockLooper();
            }
        }

        vlc_mutex_lock( p_sub->p_lock );
        p_sub->i_start = i_start;
        vlc_mutex_unlock( p_sub->p_lock );
    }

    /* Scroll at the end unless the is user is scrolling or selecting something */
    int32 start, end;
    GetSelection( &start, &end );
    if( !isScrolling && start == end && fScrollBar->LockLooper() )
    {
        float min, max;
        fScrollBar->GetRange( &min, &max );
        fScrollBar->SetValue( max );
        fScrollBar->UnlockLooper();
    }

    BTextView::Pulse();
}

/*****************************************************************************
 * MessagesWindow::MessagesWindow
 *****************************************************************************/
MessagesWindow::MessagesWindow( intf_thread_t * _p_intf,
                                BRect frame, const char * name )
    : BWindow( frame, name, B_FLOATING_WINDOW_LOOK, B_NORMAL_WINDOW_FEEL,
               B_NOT_ZOOMABLE ),
    p_intf(_p_intf)
{
    SetSizeLimits( 400, 2000, 200, 2000 );

    p_sub = msg_Subscribe( p_intf );
 
    BRect rect, textRect;

    rect = Bounds();
    rect.right -= B_V_SCROLL_BAR_WIDTH;
    textRect = rect;
    textRect.InsetBy( 5, 5 );
    fMessagesView = new MessagesView( p_sub,
                                      rect, "messages", textRect,
                                      B_FOLLOW_ALL, B_WILL_DRAW );
    fMessagesView->MakeEditable( false );
    fMessagesView->SetStylable( true );
    fScrollView = new BScrollView( "scrollview", fMessagesView, B_WILL_DRAW,
                                   B_FOLLOW_ALL, false, true );
    fMessagesView->fScrollBar = fScrollView->ScrollBar( B_VERTICAL );
    AddChild( fScrollView );
 
    /* start window thread in hidden state */
    Hide();
    Show();
}

/*****************************************************************************
 * MessagesWindow::~MessagesWindow
 *****************************************************************************/
MessagesWindow::~MessagesWindow()
{
     msg_Unsubscribe( p_intf, p_sub );
}

/*****************************************************************************
 * MessagesWindow::FrameResized
 *****************************************************************************/
void MessagesWindow::FrameResized( float width, float height )
{
    BWindow::FrameResized( width, height );
    BRect rect = fMessagesView->Bounds();
    rect.InsetBy( 5, 5 );
    fMessagesView->SetTextRect( rect );
}

/*****************************************************************************
 * MessagesWindow::QuitRequested
 *****************************************************************************/
bool MessagesWindow::QuitRequested()
{
    Hide();
    return false;
}

/*****************************************************************************
 * MessagesWindow::ReallyQuit
 *****************************************************************************/
void MessagesWindow::ReallyQuit()
{
    Lock();
    Hide();
    Quit();
}
