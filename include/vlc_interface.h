/*****************************************************************************
 * vlc_interface.h: interface access for other threads
 * This library provides basic functions for threads to interact with user
 * interface, such as message output.
 *****************************************************************************
 * Copyright (C) 1999, 2000 the VideoLAN team
 * $Id: 4a8937c814b0b42eefe1e58c12b656bf2d1ac687 $
 *
 * Authors: Vincent Seguin <seguin@via.ecp.fr>
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


typedef struct intf_dialog_args_t intf_dialog_args_t;

/**
 * \file
 * This file contains structures and function prototypes for
 * interface management in vlc
 */


/*****************************************************************************
 * intf_thread_t: describe an interface thread
 *****************************************************************************
 * This struct describes all interface-specific data of the main (interface)
 * thread.
 *****************************************************************************/

/**
 * \defgroup vlc_interface Interface
 * These functions and structures are for interface management
 * @{
 */
struct intf_thread_t
{
    VLC_COMMON_MEMBERS

    /* Thread properties and locks */
    vlc_bool_t          b_block;
    vlc_bool_t          b_play;

    /* Specific interfaces */
    intf_console_t *    p_console;                               /** console */
    intf_sys_t *        p_sys;                          /** system interface */

    /** Interface module */
    module_t *   p_module;
    void      ( *pf_run )    ( intf_thread_t * ); /** Run function */

    /** Specific for dialogs providers */
    void ( *pf_show_dialog ) ( intf_thread_t *, int, int,
                               intf_dialog_args_t * );

    /** Interaction stuff */
    vlc_bool_t b_interaction;

    /** Video window callbacks */
    void * ( *pf_request_window ) ( intf_thread_t *, vout_thread_t *,
                                    int *, int *,
                                    unsigned int *, unsigned int * );
    void   ( *pf_release_window ) ( intf_thread_t *, void * );
    int    ( *pf_control_window ) ( intf_thread_t *, void *, int, va_list );

    /* XXX: new message passing stuff will go here */
    vlc_mutex_t  change_lock;
    vlc_bool_t   b_menu_change;
    vlc_bool_t   b_menu;

    /* Provides the ability to switch an interface on the fly */
    char *psz_switch_intf;
};

/*****************************************************************************
 * intf_dialog_args_t: arguments structure passed to a dialogs provider.
 *****************************************************************************
 * This struct describes the arguments passed to the dialogs provider.
 * For now they are only used with INTF_DIALOG_FILE_GENERIC.
 *****************************************************************************/
struct intf_dialog_args_t
{
    intf_thread_t *p_intf;
    char *psz_title;

    char **psz_results;
    int  i_results;

    void (*pf_callback) ( intf_dialog_args_t * );
    void *p_arg;

    /* Specifically for INTF_DIALOG_FILE_GENERIC */
    char *psz_extensions;
    vlc_bool_t b_save;
    vlc_bool_t b_multiple;

    /* Specific to INTF_DIALOG_INTERACTION */
    interaction_dialog_t *p_dialog;
};

/*****************************************************************************
 * Prototypes
 *****************************************************************************/
#define intf_Create(a,b,c,d) __intf_Create(VLC_OBJECT(a),b,c,d)
VLC_EXPORT( intf_thread_t *, __intf_Create,     ( vlc_object_t *, const char *, int, char ** ) );
VLC_EXPORT( int,               intf_RunThread,  ( intf_thread_t * ) );
VLC_EXPORT( void,              intf_StopThread, ( intf_thread_t * ) );
VLC_EXPORT( void,              intf_Destroy,    ( intf_thread_t * ) );

/*@}*/

/*****************************************************************************
 * Macros
 *****************************************************************************/
#if defined( WIN32 ) && !defined( UNDER_CE )
#    define CONSOLE_INTRO_MSG \
         if( !getenv( "PWD" ) || !getenv( "PS1" ) ) /* detect cygwin shell */ \
         { \
         AllocConsole(); \
         freopen( "CONOUT$", "w", stdout ); \
         freopen( "CONOUT$", "w", stderr ); \
         freopen( "CONIN$", "r", stdin ); \
         } \
         msg_Info( p_intf, COPYRIGHT_MESSAGE ); \
         msg_Info( p_intf, _("\nWarning: if you can't access the GUI " \
                             "anymore, open a command-line window, go to the " \
                             "directory where you installed VLC and run " \
                             "\"vlc -I wx\"\n") )
#else
#    define CONSOLE_INTRO_MSG
#endif

/* Interface dialog ids for dialog providers */
#define INTF_DIALOG_FILE_SIMPLE 1
#define INTF_DIALOG_FILE        2
#define INTF_DIALOG_DISC        3
#define INTF_DIALOG_NET         4
#define INTF_DIALOG_CAPTURE     5
#define INTF_DIALOG_SAT         6

#define INTF_DIALOG_DIRECTORY   7

#define INTF_DIALOG_STREAMWIZARD 8
#define INTF_DIALOG_WIZARD 9

#define INTF_DIALOG_PLAYLIST   10
#define INTF_DIALOG_MESSAGES   11
#define INTF_DIALOG_FILEINFO   12
#define INTF_DIALOG_PREFS      13
#define INTF_DIALOG_BOOKMARKS  14

#define INTF_DIALOG_POPUPMENU  20
#define INTF_DIALOG_AUDIOPOPUPMENU  21
#define INTF_DIALOG_VIDEOPOPUPMENU  22
#define INTF_DIALOG_MISCPOPUPMENU  23

#define INTF_DIALOG_FILE_GENERIC 30
#define INTF_DIALOG_INTERACTION 50

#define INTF_DIALOG_UPDATEVLC   90
#define INTF_DIALOG_VLM   91

#define INTF_DIALOG_EXIT       99

/* Useful text messages shared by interfaces */
#define INTF_ABOUT_MSG LICENSE_MSG

#define EXTENSIONS_AUDIO "*.a52;*.aac;*.ac3;*.dts;*.flac;*.m4a;*.m4p;*.mka;" \
                         "*.mod;*.mp1;*.mp2;*.mp3;*.ogg;*.spx;*.wav;*.wma;*.xm"

#define EXTENSIONS_VIDEO "*.asf;*.avi;*.divx;*.dv;*.m1v;*.m2v;*.m4v;*.mkv;" \
                         "*.mov;*.mp2;*.mp4;*.mpeg;*.mpeg1;*.mpeg2;*.mpeg4;" \
                         "*.mpg;*.ogg;*.ogm;*.ps;*.ts;*.vob;*.wmv"

#define EXTENSIONS_PLAYLIST "*.asx;*.b4s;*.m3u;*.pls;*.vlc;*.xspf"

#define EXTENSIONS_SUBTITLE "*.idx;*.srt;*.sub;*.utf"
