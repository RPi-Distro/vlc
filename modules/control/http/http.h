/*****************************************************************************
 * http.h: Headers for the HTTP interface
 *****************************************************************************
 * Copyright (C) 2001-2007 the VideoLAN team
 * $Id: ac32dc9f553da13e8418cb3db7c298e8e394ae7d $
 *
 * Authors: Gildas Bazin <gbazin@netcourrier.com>
 *          Laurent Aimar <fenrir@via.ecp.fr>
 *          Christophe Massiot <massiot@via.ecp.fr>
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

#ifndef _HTTP_H_
#define _HTTP_H_

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <vlc_common.h>
#include <stdlib.h>
#include <strings.h>
#include <vlc_interface.h>
#include <vlc_playlist.h>

#include <vlc_aout.h>
#include <vlc_vout.h> /* for fullscreen */

#include <vlc_httpd.h>
#include <vlc_vlm.h>
#include <vlc_network.h>
#include <vlc_acl.h>

#ifdef HAVE_UNISTD_H
#   include <unistd.h>
#elif defined( WIN32 ) && !defined( UNDER_CE )
#   include <io.h>
#endif

#ifdef HAVE_DIRENT_H
#   include <dirent.h>
#endif

/* stat() support for large files on win32 */
#if defined( WIN32 ) && !defined( UNDER_CE )
#   define stat _stati64
#endif

/** \defgroup http_intf HTTP Interface
 * This is the HTTP remote control interface. It is fully customizable
 * by writing HTML pages using custom <vlc> tags.
 *
 * These tags use so-called macros.
 *
 * These macros can manipulate variables. For more complex operations,
 * a custom RPN evaluator with many built-in functions is provided.
 * @{
 */

/*****************************************************************************
 * Local defines
 *****************************************************************************/
#define MAX_DIR_SIZE 2560
#define STACK_MAX 100        //< Maximum RPN stack size


/*****************************************************************************
 * Utility functions
 *****************************************************************************/

/** \defgroup http_utils Utilities
 * \ingroup http_intf
 * Utilities
 * @{
 */

/* File and directory functions */

/** This function recursively parses a directory and adds all files */
int ParseDirectory( intf_thread_t *p_intf, char *psz_root,
                        char *psz_dir );
/** This function loads a file into a buffer */
int FileLoad( FILE *f, char **pp_data, int *pi_data );
/** This function returns the real path of a file or directory */
char *RealPath( const char *psz_src );

/** This command parses the "seek" command for the HTTP interface
 * and performs the requested action */
void HandleSeek( intf_thread_t *p_intf, char *p_value );

/* URI Handling functions */

/** This function extracts the value for a given argument name
 * from an HTTP request */
const char *ExtractURIValue( const char *restrict psz_uri,
                           const char *restrict psz_name,
                           char *restrict psz_value, size_t i_value_max );
char *ExtractURIString( const char *restrict psz_uri,
                            const char *restrict psz_name );
/** \todo Describe this function */
int TestURIParam( char *psz_uri, const char *psz_name );

/** This function parses a MRL */
input_item_t *MRLParse( intf_thread_t *, const char *psz, char *psz_name );

/** Return the first word from a string (works in-place) */
char *FirstWord( char *psz, char *new );

/**@}*/

/****************************************************************************
 * Variable handling functions
 ****************************************************************************/

/** \defgroup http_vars Macro variables
 * \ingroup http_intf
 * These variables can be used in the <vlc> macros and in the RPN evaluator.
 * The variables make a tree: each variable can have an arbitrary
 * number of "children" variables.
 * A number of helper functions are provided to manipulate the main variable
 * structure
 * @{
 */

/**
 * \struct mvar_t
 * This structure defines a macro variable
 */
typedef struct mvar_s
{
    char *name;                 ///< Variable name
    char *value;                ///< Variable value

    int           i_field;      ///< Number of children variables
    struct mvar_s **field;      ///< Children variables array
} mvar_t;


/** This function creates a new variable */
mvar_t  *mvar_New( const char *name, const char *value );
/** This function deletes a variable */
void     mvar_Delete( mvar_t *v );
/** This function adds f to the children variables of v, at last position */
void     mvar_AppendVar( mvar_t *v, mvar_t *f );
/** This function duplicates a variable */
mvar_t  *mvar_Duplicate( const mvar_t *v );
/** This function adds f to the children variables of v, at fist position */
void     mvar_PushVar( mvar_t *v, mvar_t *f );
/** This function removes f from the children variables of v */
void     mvar_RemoveVar( mvar_t *v, mvar_t *f );
/** This function retrieves the child variable named "name" */
mvar_t  *mvar_GetVar( mvar_t *s, const char *name );
/** This function retrieves the value of the child variable named "field" */
const char *mvar_GetValue( mvar_t *v, const char *field );
/** This function creates a variable with the given name and value and
 * adds it as first child of vars */
void     mvar_PushNewVar( mvar_t *vars, const char *name,
                              const char *value );
/** This function creates a variable with the given name and value and
 * adds it as last child of vars */
void     mvar_AppendNewVar( mvar_t *vars, const char *name,
                                const char *value );
/** @} */

/** \defgroup http_sets Sets *
 * \ingroup http_intf
 * Sets are an application of the macro variables. There are a number of
 * predefined functions that will give you variables whose children represent
 * VLC internal data (playlist, stream info, ...)
 * @{
 */

/** This function creates a set variable which represents a series of integer
 * The arg parameter must be of the form "start[:stop[:step]]"  */
mvar_t *mvar_IntegerSetNew( const char *name, const char *arg );

/** This function creates a set variable with a list of SD plugins */
mvar_t *mvar_ServicesSetNew( intf_thread_t *p_intf, char *name );

/** This function creates a set variable with the contents of the playlist */
mvar_t *mvar_PlaylistSetNew( intf_thread_t *p_intf, char *name,
                                 playlist_t *p_pl );
/** This function creates a set variable with the contents of the Stream
 * and media info box */
mvar_t *mvar_InfoSetNew( char *name, input_thread_t *p_input );
/** This function creates a set variable with the input parameters */
mvar_t *mvar_InputVarSetNew( intf_thread_t *p_intf, char *name,
                                 input_thread_t *p_input,
                                 const char *psz_variable );
/** This function creates a set variable representing the files of the psz_dir
 * directory */
mvar_t *mvar_FileSetNew( intf_thread_t *p_intf, char *name,
                             char *psz_dir );
/** This function creates a set variable representing the VLM streams */
mvar_t *mvar_VlmSetNew( char *name, vlm_t *vlm );

/** This function converts the listing of a playlist node into a mvar set */
void PlaylistListNode( intf_thread_t *p_intf, playlist_t *p_pl,
                           playlist_item_t *p_node, char *name, mvar_t *s,
                           int i_depth );

/**@}*/

/*****************************************************************************
 * RPN Evaluator
 *****************************************************************************/

/** \defgroup http_rpn RPN Evaluator
 * \ingroup http_intf
 * @{
 */

/**
 * \struct rpn_stack_t
 * This structure represents a stack of RPN commands for the HTTP interface
 * It is attached to a request
 */
typedef struct
{
    char *stack[STACK_MAX];
    int  i_stack;
} rpn_stack_t;

/** This function creates the RPN evaluator stack */
void SSInit( rpn_stack_t * );
/** This function cleans the evaluator stack */
void SSClean( rpn_stack_t * );
/* Evaluate and execute the RPN Stack */
void  EvaluateRPN( intf_thread_t *p_intf, mvar_t  *vars,
                       rpn_stack_t *st, char *exp );

/* Push an operand on top of the RPN stack */
void SSPush  ( rpn_stack_t *, const char * );
/* Remove the first operand from the RPN Stack */
char *SSPop  ( rpn_stack_t * );
/* Pushes an operand at a given position in the stack */
void SSPushN ( rpn_stack_t *, int );
/* Removes an operand at the given position in the stack */
int  SSPopN  ( rpn_stack_t *, mvar_t  * );

/**@}*/


/****************************************************************************
 * Macro handling (<vlc ... stuff)
 ****************************************************************************/

/** \defgroup http_macros <vlc> Macros Handling
 * \ingroup http_intf
 * A macro is a code snippet in the HTML page looking like
 * <vlc id="macro_id" param1="value1" param2="value2">
 * Macros string ids are mapped to macro types, and specific handling code
 * must be written for each macro type
 * @{
 */


/** \struct macro_t
 * This structure represents a HTTP Interface macro.
 */
typedef struct
{
    char *id;           ///< Macro ID string
    char *param1;       ///< First parameter
    char *param2;       ///< Second parameter
} macro_t;

/** This function parses a file for macros */
void Execute( httpd_file_sys_t *p_args,
                  char *p_request, int i_request,
                  char **pp_data, int *pi_data,
                  char **pp_dst,
                  const char *_src, const char *_end );

/**@}*/

/**
 * Core stuff
 */
/** \struct httpd_file_sys_t
 * This structure represent a single HTML file to be parsed by the macros
 * handling engine */
struct httpd_file_sys_t
{
    intf_thread_t    *p_intf;
    httpd_file_t     *p_file;
    httpd_redirect_t *p_redir;
    httpd_redirect_t *p_redir2;

    char          *file;
    char          *name;

    bool    b_html, b_handler;

    /* inited for each access */
    rpn_stack_t   stack;
    mvar_t        *vars;
};

/** \struct http_association_t
 * Structure associating an extension to an external program
 */
typedef struct http_association_t
{
    char                *psz_ext;
    int                 i_argc;
    char                **ppsz_argv;
} http_association_t;

/** \struct httpd_handler_sys_t
 * This structure represent a single CGI file to be parsed by the macros
 * handling engine */
struct httpd_handler_sys_t
{
    httpd_file_sys_t file;

    /* HACK ALERT: this is added below so that casting httpd_handler_sys_t
     * to httpd_file_sys_t works */
    httpd_handler_t  *p_handler;
    http_association_t *p_association;
};

/** \struct intf_sys_t
 * Internal service structure for the HTTP interface
 */
struct intf_sys_t
{
    httpd_host_t        *p_httpd_host;

    int                 i_files;
    httpd_file_sys_t    **pp_files;

    int                 i_handlers;
    http_association_t  **pp_handlers;
    httpd_handler_t     *p_art_handler;

    playlist_t          *p_playlist;
    input_thread_t      *p_input;
    vlm_t               *p_vlm;

    char                *psz_address;
    unsigned short      i_port;
};

/** This function is the main HTTPD Callback used by the HTTP Interface */
int HttpCallback( httpd_file_sys_t *p_args,
                      httpd_file_t *,
                      uint8_t *p_request,
                      uint8_t **pp_data, int *pi_data );
/** This function is the HTTPD Callback used for CGIs */
int  HandlerCallback( httpd_handler_sys_t *p_args,
                          httpd_handler_t *p_handler, char *_p_url,
                          uint8_t *_p_request, int i_type,
                          uint8_t *_p_in, int i_in,
                          char *psz_remote_addr, char *psz_remote_host,
                          uint8_t **_pp_data, int *pi_data );
/**@}*/

#endif

