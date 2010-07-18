/*****************************************************************************
 * sout_widgets.cpp : Widgets for stream output destination boxes
 ****************************************************************************
 * Copyright (C) 2007-2009 the VideoLAN team
 * Copyright (C) 2007 Société des arts technologiques
 * Copyright (C) 2007 Savoir-faire Linux
 * $Id: aa430b0fe5fe0ef59db2b38337cd6f40109cdbb5 $
 *
 * Authors: Jean-Baptiste Kempf <jb@videolan.org>
 *          Pierre-Luc Beaudoin <pierre-luc.beaudoin@savoirfairelinux.com>
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

#include "components/sout/sout_widgets.hpp"
#include "dialogs/sout.hpp"
#include "util/qt_dirs.hpp"
#include <vlc_intf_strings.h>

#include <QGroupBox>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QFileDialog>

#define I_FILE_SLASH_DIR \
    I_DIR_OR_FOLDER( N_("File/Directory"), N_("File/Folder") )

SoutInputBox::SoutInputBox( QWidget *_parent, const QString& mrl ) : QGroupBox( _parent )
{
    /**
     * Source Block
     **/
    setTitle( qtr( "Source" ) );
    QGridLayout *sourceLayout = new QGridLayout( this );

    QLabel *sourceLabel = new QLabel( qtr( "Source:" ) );
    sourceLayout->addWidget( sourceLabel, 0, 0 );

    sourceLine = new QLineEdit;
    sourceLine->setReadOnly( true );
    sourceLine->setText( mrl );
    sourceLabel->setBuddy( sourceLine );
    sourceLayout->addWidget( sourceLine, 0, 1 );

    QLabel *sourceTypeLabel = new QLabel( qtr( "Type:" ) );
    sourceLayout->addWidget( sourceTypeLabel, 1, 0 );
    sourceValueLabel = new QLabel;
    sourceLayout->addWidget( sourceValueLabel, 1, 1 );

    /* Line */
    QFrame *line = new QFrame;
    line->setFrameStyle( QFrame::HLine |QFrame::Sunken );
    sourceLayout->addWidget( line, 2, 0, 1, -1 );
}

void SoutInputBox::setMRL( const QString& mrl )
{
    sourceLine->setText( mrl );
    QString type;
    int i = mrl.indexOf( "://" );
    if( i != -1 )
    {
        type = mrl.left( i );
    }
    else
        type = qtr( I_FILE_SLASH_DIR );
    sourceValueLabel->setText( type );
}

#define CT( x ) connect( x, SIGNAL( textChanged( const QString& ) ), this, SIGNAL( mrlUpdated() ) );
#define CS( x ) connect( x, SIGNAL( valueChanged( int ) ), this, SIGNAL( mrlUpdated() ) );

/* FileDest Box */
FileDestBox::FileDestBox( QWidget *_parent ) : VirtualDestBox( _parent )
{
    QPushButton *fileSelectButton;
    QGridLayout *layout = new QGridLayout( this );

    QLabel *fileOutput = new QLabel(
         qtr( "This module writes the transcoded stream to a file."), this );
    layout->addWidget(fileOutput, 0, 0, 1, -1);

    QLabel *fileLabel = new QLabel( qtr( "Filename"), this );
    layout->addWidget(fileLabel, 1, 0, 1, 1);

    fileEdit = new QLineEdit(this);
    layout->addWidget(fileEdit, 1, 4, 1, 1);

    fileSelectButton = new QPushButton( qtr( "Browse..." ), this );
    QSizePolicy sizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    fileSelectButton->setSizePolicy(sizePolicy);

    layout->addWidget(fileSelectButton, 1, 5, 1, 1);
    CT( fileEdit );
    BUTTONACT( fileSelectButton, fileBrowse() );
}

QString FileDestBox::getMRL( const QString& mux )
{
    if( fileEdit->text().isEmpty() ) return "";

    SoutMrl m;
    m.begin( "file" );
    QString outputfile = fileEdit->text();
    if( !mux.isEmpty() )
    {
        if( outputfile.contains( QRegExp("\\..{2,4}$")) &&
            !outputfile.endsWith(mux) )
        {
           /* Replace the extension according to muxer */
           outputfile.replace(QRegExp("\\..{2,4}$"),"."+mux);
        } else if (!outputfile.endsWith( mux ) )
        {
           m.option( "mux", mux );
        }
    }
    m.option( "dst", outputfile );
    m.end();

    return m.getMrl();
}

void FileDestBox::fileBrowse()
{
    QString fileName = QFileDialog::getSaveFileName( this, qtr( "Save file..." ),
            "", qtr( "Containers (*.ps *.ts *.mpg *.ogg *.asf *.mp4 *.mov *.wav *.raw *.flv *.webm)" ) );
    fileEdit->setText( toNativeSeparators( fileName ) );
    emit mrlUpdated();
}



HTTPDestBox::HTTPDestBox( QWidget *_parent ) : VirtualDestBox( _parent )
{
    QGridLayout *layout = new QGridLayout( this );

    QLabel *httpOutput = new QLabel(
        qtr( "This module outputs the transcoded stream to a network via HTTP."),
        this );
    layout->addWidget(httpOutput, 0, 0, 1, -1);

    QLabel *HTTPLabel = new QLabel( qtr("Path"), this );
    QLabel *HTTPPortLabel = new QLabel( qtr("Port"), this );
    layout->addWidget(HTTPLabel, 2, 0, 1, 1);
    layout->addWidget(HTTPPortLabel, 1, 0, 1, 1);

    HTTPEdit = new QLineEdit(this);
    HTTPEdit->setText( "/" );

    HTTPPort = new QSpinBox(this);
    HTTPPort->setMaximumSize(QSize(90, 16777215));
    HTTPPort->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);
    HTTPPort->setMinimum(1);
    HTTPPort->setMaximum(65535);
    HTTPPort->setValue(8080);

    layout->addWidget(HTTPEdit, 2, 1, 1, 1);
    layout->addWidget(HTTPPort, 1, 1, 1, 1);
    CS( HTTPPort );
    CT( HTTPEdit );
}

QString HTTPDestBox::getMRL( const QString& mux )
{
    if( HTTPEdit->text().isEmpty() ) return "";

    QString path = HTTPEdit->text();
    if( path[0] != '/' )
        path.prepend( qfu("/") );
    QString port;
    port.setNum( HTTPPort->value() );
    QString dst = ":" + port + path;

    SoutMrl m;
    m.begin( "http" );
    /* Path-extension is primary muxer to use if possible,
       otherwise check for mux-choise and see that it isn't mp4
       then fallback to flv*/
    if ( !path.contains(QRegExp("\\..{2,3}$") ) )
    {
        if( !mux.isEmpty() && mux.compare("mp4") )
           m.option( "mux", mux );
        else
           m.option( "mux", "ffmpeg{mux=flv}" );
    }
    m.option( "dst", dst );
    m.end();

    return m.getMrl();
}

MMSHDestBox::MMSHDestBox( QWidget *_parent ) : VirtualDestBox( _parent )
{
    QGridLayout *layout = new QGridLayout( this );

    QLabel *mmshOutput = new QLabel(
        qtr( "This module outputs the transcoded stream to a network "
             " via the mms protocol." ), this );
    layout->addWidget(mmshOutput, 0, 0, 1, -1);

    QLabel *MMSHLabel = new QLabel( qtr("Address"), this );
    QLabel *MMSHPortLabel = new QLabel( qtr("Port"), this );
    layout->addWidget(MMSHLabel, 1, 0, 1, 1);
    layout->addWidget(MMSHPortLabel, 2, 0, 1, 1);

    MMSHEdit = new QLineEdit(this);
    MMSHEdit->setText( "0.0.0.0" );

    MMSHPort = new QSpinBox(this);
    MMSHPort->setMaximumSize(QSize(90, 16777215));
    MMSHPort->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);
    MMSHPort->setMinimum(1);
    MMSHPort->setMaximum(65535);
    MMSHPort->setValue(8080);

    layout->addWidget(MMSHEdit, 1, 1, 1, 1);
    layout->addWidget(MMSHPort, 2, 1, 1, 1);
    CS( MMSHPort );
    CT( MMSHEdit );
}

QString MMSHDestBox::getMRL( const QString& mux )
{
    if( MMSHEdit->text().isEmpty() ) return "";

    SoutMrl m;
    m.begin( "std" );
    m.option(  "access", "mmsh" );
    m.option( "mux", "asfh" );
    m.option( "dst", MMSHEdit->text(), MMSHPort->value() );
    m.end();

    return m.getMrl();
}


RTSPDestBox::RTSPDestBox( QWidget *_parent ) : VirtualDestBox( _parent )
{
    QGridLayout *layout = new QGridLayout( this );

    QLabel *rtspOutput = new QLabel(
        qtr( "This module outputs the transcoded stream to a network via "
             "RTSP." ), this );
    layout->addWidget( rtspOutput, 0, 0, 1, -1 );

    QLabel *RTSPLabel = new QLabel( qtr("Path"), this );
    QLabel *RTSPPortLabel = new QLabel( qtr("Port"), this );
    layout->addWidget( RTSPLabel, 2, 0, 1, 1 );
    layout->addWidget( RTSPPortLabel, 1, 0, 1, 1 );

    RTSPEdit = new QLineEdit( this );
    RTSPEdit->setText( "/" );

    RTSPPort = new QSpinBox( this );
    RTSPPort->setMaximumSize( QSize( 90, 16777215 ) );
    RTSPPort->setAlignment( Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter );
    RTSPPort->setMinimum( 1 );
    RTSPPort->setMaximum( 65535 );
    RTSPPort->setValue( 5544 );

    layout->addWidget( RTSPEdit, 2, 1, 1, 1 );
    layout->addWidget( RTSPPort, 1, 1, 1, 1 );
    CS( RTSPPort );
    CT( RTSPEdit );
}

QString RTSPDestBox::getMRL( const QString& mux )
{
    if( RTSPEdit->text().isEmpty() ) return "";

    QString path = RTSPEdit->text();
    if( path[0] != '/' )
        path.prepend( qfu("/") );
    QString port;
    port.setNum( RTSPPort->value() );
    QString sdp = "rtsp://:" + port + path;

    SoutMrl m;
    m.begin( "rtp" );
    m.option( "sdp", sdp );
    m.end();

    return m.getMrl();
}


UDPDestBox::UDPDestBox( QWidget *_parent ) : VirtualDestBox( _parent )
{
    QGridLayout *layout = new QGridLayout( this );

    QLabel *udpOutput = new QLabel(
        qtr( "This module outputs the transcoded stream to a network via UDP."),
        this );
    layout->addWidget(udpOutput, 0, 0, 1, -1);

    QLabel *UDPLabel = new QLabel( qtr("Address"), this );
    QLabel *UDPPortLabel = new QLabel( qtr("Port"), this );
    layout->addWidget(UDPLabel, 1, 0, 1, 1);
    layout->addWidget(UDPPortLabel, 2, 0, 1, 1);

    UDPEdit = new QLineEdit(this);

    UDPPort = new QSpinBox(this);
    UDPPort->setMaximumSize(QSize(90, 16777215));
    UDPPort->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);
    UDPPort->setMinimum(1);
    UDPPort->setMaximum(65535);
    UDPPort->setValue(1234);

    layout->addWidget(UDPEdit, 1, 1, 1, 1);
    layout->addWidget(UDPPort, 2, 1, 1, 1);
    CS( UDPPort );
    CT( UDPEdit );
}

QString UDPDestBox::getMRL( const QString& mux )
{
    if( UDPEdit->text().isEmpty() ) return "";

    SoutMrl m;
    m.begin( "udp" );
    /* udp output, ts-mux is really only reasonable one to use*/
    if( !mux.isEmpty() && !mux.compare("ts" ) )
        m.option( "mux", mux );
    m.option( "dst", UDPEdit->text(), UDPPort->value() );
    m.end();

    return m.getMrl();
}



RTPDestBox::RTPDestBox( QWidget *_parent, const char *_mux )
    : VirtualDestBox( _parent ), mux( _mux )
{
    QGridLayout *layout = new QGridLayout( this );

    QLabel *rtpOutput = new QLabel(
        qtr( "This module outputs the transcoded stream to a network via RTP."),
        this );
    layout->addWidget(rtpOutput, 0, 0, 1, -1);

    QLabel *RTPLabel = new QLabel( qtr("Address"), this );
    QLabel *RTPPortLabel = new QLabel( qtr("Base port"), this );
    layout->addWidget(RTPLabel, 1, 0, 1, 1);
    layout->addWidget(RTPPortLabel, 2, 0, 1, 1);

    RTPEdit = new QLineEdit(this);

    RTPPort = new QSpinBox(this);
    RTPPort->setMaximumSize(QSize(90, 16777215));
    RTPPort->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);
    RTPPort->setMinimum(1);
    RTPPort->setMaximum(65535);
    RTPPort->setValue(5004);

    layout->addWidget(RTPEdit, 1, 1, 1, 1);
    layout->addWidget(RTPPort, 2, 1, 1, 1);

    CS( RTPPort );
    CT( RTPEdit );
}

QString RTPDestBox::getMRL( const QString& )
{
    if( RTPEdit->text().isEmpty() ) return "";

    SoutMrl m;
    m.begin( "rtp" );
    m.option( "dst", RTPEdit->text() );
    m.option( "port", RTPPort->value() );
    /* mp4-mux ain't usable in rtp-output either */
    if( mux != NULL )
        m.option( "mux", qfu( mux ) );
    m.end();

    return m.getMrl();
}


ICEDestBox::ICEDestBox( QWidget *_parent ) : VirtualDestBox( _parent )
{
    QGridLayout *layout = new QGridLayout( this );

    QLabel *iceOutput = new QLabel(
        qtr( "This module outputs the transcoded stream to an Icecast server."),
        this );
    layout->addWidget(iceOutput, 0, 0, 1, -1);

    QLabel *ICELabel = new QLabel( qtr("Address"), this );
    QLabel *ICEPortLabel = new QLabel( qtr("Port"), this );
    layout->addWidget(ICELabel, 1, 0, 1, 1);
    layout->addWidget(ICEPortLabel, 2, 0, 1, 1);

    ICEEdit = new QLineEdit(this);

    ICEPort = new QSpinBox(this);
    ICEPort->setMaximumSize(QSize(90, 16777215));
    ICEPort->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);
    ICEPort->setMinimum(1);
    ICEPort->setMaximum(65535);
    ICEPort->setValue(8000);

    layout->addWidget(ICEEdit, 1, 1, 1, 1);
    layout->addWidget(ICEPort, 2, 1, 1, 1);

    QLabel *IcecastMountpointLabel = new QLabel( qtr( "Mount Point" ), this );
    QLabel *IcecastNameLabel = new QLabel( qtr( "Login:pass" ), this );
    ICEMountEdit = new QLineEdit( this );
    ICEPassEdit = new QLineEdit( this );
    layout->addWidget(IcecastMountpointLabel, 3, 0, 1, 1 );
    layout->addWidget(ICEMountEdit, 3, 1, 1, -1 );
    layout->addWidget(IcecastNameLabel, 4, 0, 1, 1 );
    layout->addWidget(ICEPassEdit, 4, 1, 1, -1 );

    CS( ICEPort );
    CT( ICEEdit );
    CT( ICEMountEdit );
    CT( ICEPassEdit );
}

#undef CS
#undef CT

QString ICEDestBox::getMRL( const QString& mux )
{
    if( ICEEdit->text().isEmpty() ) return "";

    SoutMrl m;
    m.begin( "std" );
    m.option( "access", "shout" );
    m.option( "mux", "ogg" );

    QString url = ICEPassEdit->text() + "@"
        + ICEEdit->text()
        + ":" + QString::number( ICEPort->value(), 10 )
        + "/" + ICEMountEdit->text();

    m.option( "dst", url );
    m.end();
    return m.getMrl();
}

