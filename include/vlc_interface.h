/*****************************************************************************
 * vlc_interface.h: interface access for other threads
 * This library provides basic functions for threads to interact with user
 * interface, such as message output.
 *****************************************************************************
 * Copyright (C) 1999, 2000 the VideoLAN team
 * $Id: 1a8cba3bb8551f7fd4845e78dafdd103145231ff $
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

#ifndef VLC_INTF_H_
#define VLC_INTF_H_

# ifdef __cplusplus
extern "C" {
# endif

typedef struct intf_dialog_args_t intf_dialog_args_t;

/**
 * \file
 * This file contains structures and function prototypes for
 * interface management in vlc
 */

/**
 * \defgroup vlc_interface Interface
 * These functions and structures are for interface management
 * @{
 */

typedef struct intf_sys_t intf_sys_t;

/** Describe all interface-specific data of the interface thread */
typedef struct intf_thread_t
{
    VLC_COMMON_MEMBERS

    struct intf_thread_t *p_next; /** LibVLC interfaces book keeping */
    /* Thread properties and locks */
#if defined( __APPLE__ ) || defined( WIN32 )
    bool          b_should_run_on_first_thread;
#endif

    /* Specific interfaces */
    intf_sys_t *        p_sys;                          /** system interface */
    char *              psz_intf;                    /** intf name specified */

    /** Interface module */
    module_t *   p_module;
    void      ( *pf_run )    ( struct intf_thread_t * ); /** Run function */

    /** Specific for dialogs providers */
    void ( *pf_show_dialog ) ( struct intf_thread_t *, int, int,
                               intf_dialog_args_t * );

    config_chain_t *p_cfg;
} intf_thread_t;

/** \brief Arguments passed to a dialogs provider
 *  This describes the arguments passed to the dialogs provider. They are
 *  mainly used with INTF_DIALOG_FILE_GENERIC.
 */
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
    bool b_save;
    bool b_multiple;

    /* Specific to INTF_DIALOG_INTERACTION */
    struct interaction_dialog_t *p_dialog;
};

/*****************************************************************************
 * Prototypes
 *****************************************************************************/
VLC_EXPORT( int, intf_Create, ( vlc_object_t *, const char * ) );
#define intf_Create(a,b) intf_Create(VLC_OBJECT(a),b)

VLC_EXPORT( int, intf_Eject, ( vlc_object_t *, const char * ) );
#define intf_Eject(a,b) intf_Eject(VLC_OBJECT(a),b)

VLC_EXPORT( void, libvlc_Quit, ( libvlc_int_t * ) );

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
         msg_Info( p_intf, "VLC media player - %s", VERSION_MESSAGE ); \
         msg_Info( p_intf, "%s", COPYRIGHT_MESSAGE ); \
         msg_Info( p_intf, _("\nWarning: if you can't access the GUI " \
                             "anymore, open a command-line window, go to the " \
                             "directory where you installed VLC and run " \
                             "\"vlc -I qt\"\n") )
#else
#    define CONSOLE_INTRO_MSG
#endif

/* Interface dialog ids for dialog providers */
typedef enum vlc_dialog {
    INTF_DIALOG_FILE_SIMPLE = 1,
    INTF_DIALOG_FILE,
    INTF_DIALOG_DISC,
    INTF_DIALOG_NET,
    INTF_DIALOG_CAPTURE,
    INTF_DIALOG_SAT,
    INTF_DIALOG_DIRECTORY,

    INTF_DIALOG_STREAMWIZARD,
    INTF_DIALOG_WIZARD,

    INTF_DIALOG_PLAYLIST,
    INTF_DIALOG_MESSAGES,
    INTF_DIALOG_FILEINFO,
    INTF_DIALOG_PREFS,
    INTF_DIALOG_BOOKMARKS,
    INTF_DIALOG_EXTENDED,

    INTF_DIALOG_POPUPMENU = 20,
    INTF_DIALOG_AUDIOPOPUPMENU,
    INTF_DIALOG_VIDEOPOPUPMENU,
    INTF_DIALOG_MISCPOPUPMENU,

    INTF_DIALOG_FILE_GENERIC = 30,
    INTF_DIALOG_INTERACTION = 50,

    INTF_DIALOG_UPDATEVLC = 90,
    INTF_DIALOG_VLM,

    INTF_DIALOG_EXIT = 99
} vlc_dialog_t;

/* Useful text messages shared by interfaces */
#define INTF_ABOUT_MSG LICENSE_MSG

#define EXTENSIONS_AUDIO \
    "*.a52;" \
    "*.aac;" \
    "*.ac3;" \
    "*.adt;" \
    "*.adts;" \
    "*.aif;"\
    "*.aifc;"\
    "*.aiff;"\
    "*.amr;" \
    "*.aob;" \
    "*.ape;" \
    "*.cda;" \
    "*.dts;" \
    "*.flac;"\
    "*.it;"  \
    "*.m4a;" \
    "*.m4p;" \
    "*.mid;" \
    "*.mka;" \
    "*.mlp;" \
    "*.mod;" \
    "*.mp1;" \
    "*.mp2;" \
    "*.mp3;" \
    "*.mpc;" \
    "*.oga;" \
    "*.ogg;" \
    "*.oma;" \
    "*.rmi;" \
    "*.s3m;" \
    "*.spx;" \
    "*.tta;" \
    "*.voc;" \
    "*.vqf;" \
    "*.w64;" \
    "*.wav;" \
    "*.wma;" \
    "*.wv;"  \
    "*.xa;"  \
    "*.xm"

#define EXTENSIONS_VIDEO "*.3g2;*.3gp;*.3gp2;*.3gpp;*.amv;*.asf;*.avi;*.bin;*.cue;*.divx;*.dv;*.flv;*.gxf;*.iso;*.m1v;*.m2v;" \
                         "*.m2t;*.m2ts;*.m4v;*.mkv;*.mov;*.mp2;*.mp2v;*.mp4;*.mp4v;*.mpa;*.mpe;*.mpeg;*.mpeg1;" \
                         "*.mpeg2;*.mpeg4;*.mpg;*.mpv2;*.mts;*.mxf;*.nsv;*.nuv;" \
                         "*.ogg;*.ogm;*.ogv;*.ogx;*.ps;" \
                         "*.rec;*.rm;*.rmvb;*.tod;*.ts;*.tts;*.vob;*.vro;*.webm;*.wmv"

#define EXTENSIONS_PLAYLIST "*.asx;*.b4s;*.ifo;*.m3u;*.m3u8;*.pls;*.ram;*.rar;*.sdp;*.vlc;*.xspf;*.zip"

#define EXTENSIONS_MEDIA EXTENSIONS_VIDEO ";" EXTENSIONS_AUDIO ";" \
                          EXTENSIONS_PLAYLIST

#define EXTENSIONS_SUBTITLE "*.cdg;*.idx;*.srt;*.sub;*.utf;*.ass;*.ssa;*.aqt;" \
                            "*.jss;*.psb;*.rt;*.smi"

/** \defgroup vlc_interaction Interaction
 * \ingroup vlc_interface
 * Interaction between user and modules
 * @{
 */

/**
 * This structure describes a piece of interaction with the user
 */
typedef struct interaction_dialog_t
{
    int             i_type;             ///< Type identifier
    char           *psz_title;          ///< Title
    char           *psz_description;    ///< Descriptor string
    char           *psz_default_button;  ///< default button title (~OK)
    char           *psz_alternate_button;///< alternate button title (~NO)
    /// other button title (optional,~Cancel)
    char           *psz_other_button;

    char           *psz_returned[1];    ///< returned responses from the user

    vlc_value_t     val;                ///< value coming from core for dialogue
    int             i_timeToGo;         ///< time (in sec) until shown progress is finished
    bool      b_cancelled;        ///< was the dialogue cancelled ?

    void *          p_private;          ///< Private interface data

    int             i_status;           ///< Dialog status;
    int             i_action;           ///< Action to perform;
    int             i_flags;            ///< Misc flags
    int             i_return;           ///< Return status

    vlc_object_t   *p_parent;           ///< The vlc object that asked
                                        //for interaction
    intf_thread_t  *p_interface;
    vlc_mutex_t    *p_lock;
} interaction_dialog_t;

/**
 * Possible flags . Dialog types
 */
#define DIALOG_GOT_ANSWER           0x01
#define DIALOG_YES_NO_CANCEL        0x02
#define DIALOG_LOGIN_PW_OK_CANCEL   0x04
#define DIALOG_PSZ_INPUT_OK_CANCEL  0x08
#define DIALOG_BLOCKING_ERROR       0x10
#define DIALOG_NONBLOCKING_ERROR    0x20
#define DIALOG_USER_PROGRESS        0x80
#define DIALOG_INTF_PROGRESS        0x100

/** Possible return codes */
enum
{
    DIALOG_OK_YES,
    DIALOG_NO,
    DIALOG_CANCELLED
};

/** Possible status  */
enum
{
    ANSWERED_DIALOG,            ///< Got "answer"
    DESTROYED_DIALOG,           ///< Interface has destroyed it
};

/** Possible actions */
enum
{
    INTERACT_NEW,
    INTERACT_UPDATE,
    INTERACT_HIDE,
    INTERACT_DESTROY
};

#define intf_UserStringInput( a, b, c, d ) (VLC_OBJECT(a),b,c,d, VLC_EGENERIC)
#define interaction_Register( t ) (t, VLC_EGENERIC)
#define interaction_Unregister( t ) (t, VLC_EGENERIC)


/** @} */
/** @} */

# ifdef __cplusplus
}
# endif
#endif
