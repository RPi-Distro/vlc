/*****************************************************************************
 * theme.hpp
 *****************************************************************************
 * Copyright (C) 2003 the VideoLAN team
 * $Id$
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

#ifndef THEME_HPP
#define THEME_HPP

#include "generic_bitmap.hpp"
#include "generic_font.hpp"
#include "generic_layout.hpp"
#include "popup.hpp"
#include "../src/window_manager.hpp"
#include "../commands/cmd_generic.hpp"
#include "../utils/bezier.hpp"
#include "../utils/variable.hpp"
#include "../utils/position.hpp"
#include "../controls/ctrl_generic.hpp"
#include <string>
#include <list>
#include <map>

class Builder;
class Interpreter;

/// Class storing the data of the current theme
class Theme: public SkinObject
{
    private:
        friend class Builder;
        friend class Interpreter;
    public:
        Theme( intf_thread_t *pIntf ): SkinObject( pIntf ),
            m_windowManager( getIntf() ) {}
        virtual ~Theme();

        void loadConfig();
        void saveConfig();

        GenericBitmap *getBitmapById( const string &id ) const;
        GenericFont *getFontById( const string &id ) const;
        Popup *getPopupById( const string &id ) const;
        TopWindow *getWindowById( const string &id ) const;
        GenericLayout *getLayoutById( const string &id ) const;
        CtrlGeneric *getControlById( const string &id ) const;
        Position *getPositionById( const string &id ) const;

        WindowManager &getWindowManager() { return m_windowManager; }

    private:
        /// Store the bitmaps by ID
        map<string, GenericBitmapPtr> m_bitmaps;
        /// Store the fonts by ID
        map<string, GenericFontPtr> m_fonts;
        /// Store the popups by ID
        map<string, PopupPtr> m_popups;
        /// Store the windows by ID
        map<string, TopWindowPtr> m_windows;
        /// Store the layouts by ID
        map<string, GenericLayoutPtr> m_layouts;
        /// Store the controls by ID
        map<string, CtrlGenericPtr> m_controls;
        /// Store the panel positions by ID
        map<string, PositionPtr> m_positions;
        /// Store the commands
        list<CmdGenericPtr> m_commands;
        /// Store the Bezier curves
        list<BezierPtr> m_curves;
        /// Store the variables
        list<VariablePtr> m_vars;

        WindowManager m_windowManager;
};


#endif
