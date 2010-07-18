/*****************************************************************************
 * simple_preferences.hpp : Simple prefs
 ****************************************************************************
 * Copyright (C) 2006 the VideoLAN team
 * $Id: 626de8279bd38084013990262409ecb0f38166a8 $
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
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

#ifndef _SIMPLEPREFS_H_
#define _SIMPLEPREFS_H_

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_interface.h>

#include "ui/sprefs_input.h"
#include "ui/sprefs_audio.h"
#include "ui/sprefs_video.h"
#include "ui/sprefs_subtitles.h"
#include "ui/sprefs_interface.h"

#ifdef WIN32
# include "util/registry.hpp"
#endif

#include <QWidget>

enum {
    SPrefsInterface = 0,
    SPrefsAudio,
    SPrefsVideo,
    SPrefsSubtitles,
    SPrefsInputAndCodecs,
    SPrefsHotkeys,
    SPrefsMax
};
#define SPrefsDefaultCat SPrefsInterface

enum {
    CachingCustom = 0,
    CachingLowest = 100,
    CachingLow    = 200,
    CachingNormal = 300,
    CachingHigh   = 750,
    CachingHigher = 2500
};

enum {
#ifdef WIN32
       directxW,
#else
       alsaW,
       ossW,
#endif
       fileW,
       audioOutCoB,
       normalizerChB,
       volLW,
       headphoneB,
       spdifChB,
};
enum { inputLE, cachingCoB };
enum { skinRB, qtRB, styleCB };

class ConfigControl;
class QComboBox;
class QLineEdit;
class QRadioButton;
class QCheckBox;
class QString;

#ifdef WIN32
class QTreeWidgetItem;
#endif

class SPrefsCatList : public QWidget
{
    Q_OBJECT
public:
    SPrefsCatList( intf_thread_t *, QWidget *, bool );
    virtual ~SPrefsCatList() {};
private:
    intf_thread_t *p_intf;
signals:
    void currentItemChanged( int );
public slots:
    void switchPanel( int );
};

class SPrefsPanel : public QWidget
{
    Q_OBJECT
public:
    SPrefsPanel( intf_thread_t *, QWidget *, int, bool );
    virtual ~SPrefsPanel();
    void apply();
    void clean();
private:
    intf_thread_t *p_intf;
    QList<ConfigControl *> controls;

    int number;

    QList<QWidget *> optionWidgets;
    QStringList qs_filter;

#ifdef WIN32
    QList<QTreeWidgetItem *> listAsso;
    bool addType( const char * psz_ext, QTreeWidgetItem*, QTreeWidgetItem*, QVLCRegistry* );
#endif

/* Display only the options for the selected audio output */
private slots:
    void lastfm_Changed( int );
    void updateAudioOptions( int );
    void updateAudioVolume( int );
#ifdef SYS_MINGW32
    void assoDialog();
    void saveAsso();
#endif
    void changeStyle( QString );
};

#endif
