/*****************************************************************************
 * menus.hpp : Menus handling
 ****************************************************************************
 * Copyright (C) 2006 the VideoLAN team
 * $Id: f553679ba91f187ff45c64e5f9f51443a44fe768 $
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
 *          Jean-Baptiste Kempf <jb@videolan.org>
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

#ifndef QVLC_MENUS_H_
#define QVLC_MENUS_H_

#include "qt4.hpp"

#include <QObject>
#include <QAction>
#include <vector>

using namespace std;

class QMenu;
class QMenuBar;
class QSystemTrayIcon;

class MenuItemData : public QObject
{
    Q_OBJECT

public:
    MenuItemData( QObject* parent, vlc_object_t *_p_obj, int _i_type,
                  vlc_value_t _val, const char *_var ) : QObject( parent )
    {
        p_obj = _p_obj;
        if( p_obj )
            vlc_object_hold( p_obj );
        i_val_type = _i_type;
        val = _val;
        psz_var = strdup( _var );
    }
    virtual ~MenuItemData()
    {
        free( psz_var );
        if( ( i_val_type & VLC_VAR_TYPE) == VLC_VAR_STRING )
            free( val.psz_string );
        if( p_obj )
            vlc_object_release( p_obj );
    }

    vlc_object_t *p_obj;
    vlc_value_t val;
    char *psz_var;

private:
    int i_val_type;
};

class QVLCMenu : public QObject
{
    Q_OBJECT
    friend class MenuFunc;

public:
    /* Main bar creation */
    static void createMenuBar( MainInterface *mi, intf_thread_t * );

    /* Popups Menus */
    static void PopupMenu( intf_thread_t *, bool );
    static void AudioPopupMenu( intf_thread_t *, bool );
    static void VideoPopupMenu( intf_thread_t *, bool );
    static void MiscPopupMenu( intf_thread_t *, bool );

    /* Systray */
    static void updateSystrayMenu( MainInterface *, intf_thread_t  *,
                                   bool b_force_visible = false);

    /* Actions */
    static void DoAction( QObject * );

private:
    /* All main Menus */
    static QMenu *FileMenu( intf_thread_t *, QWidget * );
    static QMenu *SDMenu( intf_thread_t *, QWidget * );

    static QMenu *ToolsMenu( QMenu * );
    static QMenu *ToolsMenu( QWidget * );

    static QMenu *ViewMenu( intf_thread_t *, QWidget * );
    static QMenu *ViewMenu( intf_thread_t *, QMenu *, MainInterface * mi = NULL );

    static QMenu *InterfacesMenu( intf_thread_t *p_intf, QMenu * );
    static void ExtensionsMenu( intf_thread_t *p_intf, QMenu * );

    static QMenu *NavigMenu( intf_thread_t *, QMenu * );
    static QMenu *NavigMenu( intf_thread_t *, QWidget * );
    static QMenu *RebuildNavigMenu( intf_thread_t *, QMenu *);

    static QMenu *VideoMenu( intf_thread_t *, QMenu * );
    static QMenu *VideoMenu( intf_thread_t *, QWidget * );

    static QMenu *AudioMenu( intf_thread_t *, QMenu * );
    static QMenu *AudioMenu( intf_thread_t *, QWidget * );

    static QMenu *HelpMenu( QWidget * );

    /* Popups Menus */
    static void PopupMenuStaticEntries( QMenu *menu );
    static void PopupPlayEntries( QMenu *menu, intf_thread_t *p_intf,
                                         input_thread_t *p_input );
    static void PopupMenuControlEntries( QMenu *menu, intf_thread_t *p_intf );
    static void PopupMenuPlaylistControlEntries( QMenu *menu, intf_thread_t *p_intf );

    /* Generic automenu methods */
    static QMenu * Populate( intf_thread_t *, QMenu *current,
                             vector<const char*>&, vector<vlc_object_t *>& );

    static void CreateAndConnect( QMenu *, const char *, const QString&,
                                  const QString&, int, vlc_object_t *,
                                  vlc_value_t, int, bool c = false );
    static void UpdateItem( intf_thread_t *, QMenu *, const char *,
                            vlc_object_t *, bool );
    static int CreateChoicesMenu( QMenu *,const char *, vlc_object_t *, bool );

    /* recentMRL menu */
    static QMenu *recentsMenu;

public slots:
    static void updateRecents( intf_thread_t * );
};

class MenuFunc : public QObject
{
    Q_OBJECT

public:
    MenuFunc( QMenu *_menu, int _id ) : QObject( (QObject *)_menu ),
                                        menu( _menu ), id( _id ){}

    void doFunc( intf_thread_t *p_intf)
    {
        switch( id )
        {
            case 1: QVLCMenu::AudioMenu( p_intf, menu ); break;
            case 2: QVLCMenu::VideoMenu( p_intf, menu ); break;
            case 3: QVLCMenu::RebuildNavigMenu( p_intf, menu ); break;
            case 4: QVLCMenu::ViewMenu( p_intf, menu ); break;
        }
    }
private:
    QMenu *menu;
    int id;
};

#endif
