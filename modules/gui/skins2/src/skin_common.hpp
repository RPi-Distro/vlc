/*****************************************************************************
 * skin_common.hpp
 *****************************************************************************
 * Copyright (C) 2003 the VideoLAN team
 * $Id: d680319d09e607f38f0f2ed2ea98b05051087c51 $
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifndef SKIN_COMMON_HPP
#define SKIN_COMMON_HPP

#include <vlc_common.h>
#include <vlc_interface.h>
#include <vlc_charset.h>
#include <vlc_fs.h>

#include <string>
using namespace std;

class AsyncQueue;
class Logger;
class Dialogs;
class Interpreter;
class OSFactory;
class OSLoop;
class VarManager;
class VlcProc;
class VoutManager;
class Theme;
class ThemeRepository;

#ifndef M_PI
#   define M_PI 3.14159265358979323846
#endif

#ifdef _MSC_VER
// turn off 'warning C4355: 'this' : used in base member initializer list'
#pragma warning ( disable:4355 )
// turn off 'identifier was truncated to '255' characters in the debug info'
#pragma warning ( disable:4786 )
#endif


/// Wrapper around FromLocale, to avoid the need to call LocaleFree()
static inline string sFromLocale( const string &rLocale )
{
    char *s = FromLocale( rLocale.c_str() );
    string res = s;
    LocaleFree( s );
    return res;
}

#ifdef WIN32
/// Wrapper around FromWide, to avoid the need to call free()
static inline string sFromWide( const wstring &rWide )
{
    char *s = FromWide( rWide.c_str() );
    string res = s;
    free( s );
    return res;
}
#endif

/// Wrapper around ToLocale, to avoid the need to call LocaleFree()
static inline string sToLocale( const string &rUTF8 )
{
    char *s = ToLocale( rUTF8.c_str() );
    string res = s;
    LocaleFree( s );
    return res;
}


//---------------------------------------------------------------------------
// intf_sys_t: description and status of skin interface
//---------------------------------------------------------------------------
struct intf_sys_t
{
    /// The input thread
    input_thread_t *p_input;

    /// The playlist thread
    playlist_t *p_playlist;

    /// Message bank subscription
    msg_subscription_t *p_sub;

    // "Singleton" objects: MUST be initialized to NULL !
    /// Logger
    Logger *p_logger;
    /// Asynchronous command queue
    AsyncQueue *p_queue;
    /// Dialog provider
    Dialogs *p_dialogs;
    /// Script interpreter
    Interpreter *p_interpreter;
    /// Factory for OS specific classes
    OSFactory *p_osFactory;
    /// Main OS specific message loop
    OSLoop *p_osLoop;
    /// Variable manager
    VarManager *p_varManager;
    /// VLC state handler
    VlcProc *p_vlcProc;
    /// Vout manager
    VoutManager *p_voutManager;
    /// Theme repository
    ThemeRepository *p_repository;

    /// Current theme
    Theme *p_theme;

    /// synchronisation at start of interface
    vlc_thread_t thread;
    vlc_mutex_t  init_lock;
    vlc_cond_t   init_wait;
    bool         b_ready;

    /// handle (vout windows)
    void*        handle;
    vlc_mutex_t  vout_lock;
    vlc_cond_t   vout_wait;
    bool         b_vout_ready;
};


/// Base class for all skin classes
class SkinObject
{
public:
    SkinObject( intf_thread_t *pIntf ): m_pIntf( pIntf ) { }
    virtual ~SkinObject() { }

    /// Getter (public because it is used in C callbacks in the win32
    /// interface)
    intf_thread_t *getIntf() const { return m_pIntf; }

private:
    intf_thread_t *m_pIntf;
};


#endif
