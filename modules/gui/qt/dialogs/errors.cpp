/*****************************************************************************
 * errors.cpp : Errors
 ****************************************************************************
 * Copyright ( C ) 2006 the VideoLAN team
 * $Id: 698c4c7d91f461a88cabacddd5cb518772104585 $
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
 *          Jean-Baptiste Kempf <jb@videolan.org>
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

#include "dialogs/errors.hpp"

#include <QTextCursor>
#include <QTextEdit>
#include <QCheckBox>
#include <QGridLayout>
#include <QDialogButtonBox>
#include <QPushButton>

ErrorsDialog::ErrorsDialog( intf_thread_t *_p_intf )
             : QVLCDialog( (QWidget*)_p_intf->p_sys->p_mi, _p_intf )
{
    setWindowTitle( qtr( "Errors" ) );
    setWindowRole( "vlc-errors" );
    resize( 500 , 300 );

    QGridLayout *layout = new QGridLayout( this );

    QDialogButtonBox *buttonBox = new QDialogButtonBox( Qt::Horizontal, this );
    QPushButton *clearButton = new QPushButton( qtr( "Cl&ear" ), this );
    buttonBox->addButton( clearButton, QDialogButtonBox::ActionRole );
    buttonBox->addButton( new QPushButton( qtr("&Close"), this ), QDialogButtonBox::RejectRole );

    messages = new QTextEdit();
    messages->setReadOnly( true );
    messages->setHorizontalScrollBarPolicy( Qt::ScrollBarAlwaysOff );
    stopShowing = new QCheckBox( qtr( "Hide future errors" ) );

    layout->addWidget( messages, 0, 0, 1, 3 );
    layout->addWidget( stopShowing, 1, 0 );
    layout->addWidget( buttonBox, 1, 2 );

    CONNECT( buttonBox, rejected(), this, close() );
    BUTTONACT( clearButton, clear() );
    BUTTONACT( stopShowing, dontShow() );
}

void ErrorsDialog::addError( const QString& title, const QString& text )
{
    add( true, title, text );
}

/*void ErrorsDialog::addWarning( QString title, QString text )
{
    add( false, title, text );
}*/

void ErrorsDialog::add( bool error, const QString& title, const QString& text )
{
    if( stopShowing->isChecked() ) return;
    messages->textCursor().movePosition( QTextCursor::End );
    messages->setTextColor( error ? "red" : "yellow" );
    messages->insertPlainText( title + QString( ":\n" ) );
    messages->setTextColor( "black" );
    messages->insertPlainText( text + QString( "\n" ) );
    messages->ensureCursorVisible();
    show();
}

void ErrorsDialog::close()
{
    hide();
}

void ErrorsDialog::clear()
{
    messages->clear();
}

void ErrorsDialog::dontShow()
{
    if( stopShowing->isChecked() )
    {
        config_PutInt( p_intf, "qt-show-errors", 0 );
    }
}
