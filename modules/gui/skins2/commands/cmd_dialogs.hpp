/*****************************************************************************
 * cmd_dialogs.hpp
 *****************************************************************************
 * Copyright (C) 2003 the VideoLAN team
 * $Id: d01de3da5c8df198fa93229b9e32a9e5861c97db $
 *
 * Authors: Cyril Deguet     <asmax@via.ecp.fr>
 *          Olivier Teulière <ipkiss@via.ecp.fr>
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

#ifndef CMD_DIALOGS_HPP
#define CMD_DIALOGS_HPP

#include "cmd_generic.hpp"
#include "../src/dialogs.hpp"
#include "cmd_change_skin.hpp"

#include <vlc_interface.h>

template<int TYPE = 0> class CmdDialogs;

// XXX use an enum instead
typedef CmdDialogs<1> CmdDlgChangeSkin;
typedef CmdDialogs<2> CmdDlgFileSimple;
typedef CmdDialogs<3> CmdDlgFile;
typedef CmdDialogs<4> CmdDlgDisc;
typedef CmdDialogs<5> CmdDlgNet;
typedef CmdDialogs<6> CmdDlgMessages;
typedef CmdDialogs<7> CmdDlgPrefs;
typedef CmdDialogs<8> CmdDlgFileInfo;

typedef CmdDialogs<11> CmdDlgAdd;
typedef CmdDialogs<12> CmdDlgPlaylistLoad;
typedef CmdDialogs<13> CmdDlgPlaylistSave;
typedef CmdDialogs<14> CmdDlgDirectory;
typedef CmdDialogs<15> CmdDlgStreamingWizard;
typedef CmdDialogs<16> CmdDlgPlaytreeLoad;
typedef CmdDialogs<17> CmdDlgPlaytreeSave;
typedef CmdDialogs<18> CmdDlgPlaylist;

typedef CmdDialogs<30> CmdDlgShowPopupMenu;
typedef CmdDialogs<31> CmdDlgHidePopupMenu;
typedef CmdDialogs<32> CmdDlgShowAudioPopupMenu;
typedef CmdDialogs<33> CmdDlgHideAudioPopupMenu;
typedef CmdDialogs<34> CmdDlgShowVideoPopupMenu;
typedef CmdDialogs<35> CmdDlgHideVideoPopupMenu;
typedef CmdDialogs<36> CmdDlgShowMiscPopupMenu;
typedef CmdDialogs<37> CmdDlgHideMiscPopupMenu;


/// Generic "Open dialog" command
template<int TYPE>
class CmdDialogs: public CmdGeneric
{
    public:
        CmdDialogs( intf_thread_t *pIntf ): CmdGeneric( pIntf ) {}
        virtual ~CmdDialogs() {}

        /// This method does the real job of the command
        virtual void execute()
        {
            /// Get the dialogs provider
            Dialogs *pDialogs = Dialogs::instance( getIntf() );
            if( pDialogs == NULL )
            {
                return;
            }

            switch( TYPE )
            {
                case 1:
                    pDialogs->showChangeSkin();
                    break;
                case 2:
                    pDialogs->showFileSimple( true );
                    break;
                case 3:
                    pDialogs->showFile( true );
                    break;
                case 4:
                    pDialogs->showDisc( true );
                    break;
                case 5:
                    pDialogs->showNet( true );
                    break;
                case 6:
                    pDialogs->showMessages();
                    break;
                case 7:
                    pDialogs->showPrefs();
                    break;
                case 8:
                    pDialogs->showFileInfo();
                    break;
               case 11:
                    pDialogs->showFile( false );
                    break;
                case 12:
                    pDialogs->showPlaylistLoad();
                    break;
                case 13:
                    pDialogs->showPlaylistSave();
                    break;
                case 14:
                    pDialogs->showDirectory( true );
                    break;
                case 15:
                    pDialogs->showStreamingWizard();
                    break;
                case 18:
                    pDialogs->showPlaylist();
                    break;
                case 30:
                    pDialogs->showPopupMenu( true, INTF_DIALOG_POPUPMENU );
                    break;
                case 31:
                    pDialogs->showPopupMenu( false, INTF_DIALOG_POPUPMENU );
                    break;
                case 32:
                    pDialogs->showPopupMenu( true, INTF_DIALOG_AUDIOPOPUPMENU );
                    break;
                case 33:
                    pDialogs->showPopupMenu( false,INTF_DIALOG_AUDIOPOPUPMENU );
                    break;
                case 34:
                    pDialogs->showPopupMenu( true, INTF_DIALOG_VIDEOPOPUPMENU );
                    break;
                case 35:
                    pDialogs->showPopupMenu( false,INTF_DIALOG_VIDEOPOPUPMENU );
                    break;
                 case 36:
                    pDialogs->showPopupMenu( true, INTF_DIALOG_MISCPOPUPMENU );
                    break;
                case 37:
                    pDialogs->showPopupMenu( false,INTF_DIALOG_MISCPOPUPMENU );
                    break;
                default:
                    msg_Warn( getIntf(), "unknown dialog type" );
                    break;
            }
        }

        /// Return the type of the command
        virtual string getType() const { return "dialog"; }
};

class CmdInteraction: public CmdGeneric
{
    public:
        CmdInteraction( intf_thread_t *pIntf, interaction_dialog_t *
                        p_dialog ): CmdGeneric( pIntf ), m_pDialog( p_dialog )
        {}
        virtual ~CmdInteraction() {}

        /// This method does the real job of the command
        virtual void execute()
        {
            /// Get the dialogs provider
            Dialogs *pDialogs = Dialogs::instance( getIntf() );
            if( pDialogs == NULL )
            {
                return;
            }
            pDialogs->showInteraction( m_pDialog );
        }

        virtual string getType() const { return "interaction"; }
    private:
        interaction_dialog_t *m_pDialog;
};

#endif
