/*****************************************************************************
 * specific.c: Win32 specific initilization
 *****************************************************************************
 * Copyright (C) 2001-2004, 2010 the VideoLAN team
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
 *          Gildas Bazin <gbazin@videolan.org>
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#define UNICODE
#include <vlc_common.h>
#include "../libvlc.h"
#include <vlc_playlist.h>
#include <vlc_url.h>

#include "../config/vlc_getopt.h"

#if !defined( UNDER_CE )
#   include <io.h>
#   include <fcntl.h>
#   include  <mmsystem.h>
#endif

#include <winsock.h>

/*****************************************************************************
 * system_Init: initialize winsock and misc other things.
 *****************************************************************************/
void system_Init( libvlc_int_t *p_this, int *pi_argc, const char *ppsz_argv[] )
{
    VLC_UNUSED( p_this ); VLC_UNUSED( pi_argc ); VLC_UNUSED( ppsz_argv );
    WSADATA Data;
    MEMORY_BASIC_INFORMATION mbi;

    /* Get our full path */
    char psz_path[MAX_PATH];
    char *psz_vlc;

    wchar_t psz_wpath[MAX_PATH];
    if( VirtualQuery(system_Init, &mbi, sizeof(mbi) ) )
    {
        HMODULE hMod = (HMODULE) mbi.AllocationBase;
        if( GetModuleFileName( hMod, psz_wpath, MAX_PATH ) )
        {
            WideCharToMultiByte( CP_UTF8, 0, psz_wpath, -1,
                                psz_path, MAX_PATH, NULL, NULL );
        }
        else psz_path[0] = '\0';
    }

    psz_vlc = strrchr( psz_path, '\\' );
    if( psz_vlc )
        *psz_vlc = '\0';

    {
        /* remove trailing \.libs from executable dir path if seen,
           we assume we are running vlc through libtool wrapper in build dir */
        size_t len = strlen(psz_path);
        if( len >= 5 && !stricmp(psz_path + len - 5, "\\.libs" ) )
            psz_path[len - 5] = '\0';
    }

    psz_vlcpath = strdup( psz_path );

    /* Set the default file-translation mode */
#if !defined( UNDER_CE )
    _fmode = _O_BINARY;
    _setmode( _fileno( stdin ), _O_BINARY ); /* Needed for pipes */

    timeBeginPeriod(5);
#endif

    /* Call mdate() once to make sure it is initialized properly */
    mdate();

    /* WinSock Library Init. */
    if( !WSAStartup( MAKEWORD( 2, 2 ), &Data ) )
    {
        /* Aah, pretty useless check, we should always have Winsock 2.2
         * since it appeared in Win98. */
        if( LOBYTE( Data.wVersion ) != 2 || HIBYTE( Data.wVersion ) != 2 )
            /* We could not find a suitable WinSock DLL. */
            WSACleanup( );
        else
            /* Everything went ok. */
            return;
    }

    /* Let's try with WinSock 1.1 */
    if( !WSAStartup( MAKEWORD( 1, 1 ), &Data ) )
    {
        /* Confirm that the WinSock DLL supports 1.1.*/
        if( LOBYTE( Data.wVersion ) != 1 || HIBYTE( Data.wVersion ) != 1 )
            /* We could not find a suitable WinSock DLL. */
            WSACleanup( );
        else
            /* Everything went ok. */
            return;
    }

    fprintf( stderr, "error: can't initialize WinSocks\n" );
}

/*****************************************************************************
 * system_Configure: check for system specific configuration options.
 *****************************************************************************/
static unsigned __stdcall IPCHelperThread( void * );
LRESULT CALLBACK WMCOPYWNDPROC( HWND, UINT, WPARAM, LPARAM );
static vlc_object_t *p_helper = NULL;
static unsigned long hIPCHelper;
static HANDLE hIPCHelperReady;

typedef struct
{
  int argc;
  int enqueue;
  char data[];
} vlc_ipc_data_t;

void system_Configure( libvlc_int_t *p_this, int i_argc, const char *const ppsz_argv[] )
{
#if !defined( UNDER_CE )
    /* Raise default priority of the current process */
#ifndef ABOVE_NORMAL_PRIORITY_CLASS
#   define ABOVE_NORMAL_PRIORITY_CLASS 0x00008000
#endif
    if( var_InheritBool( p_this, "high-priority" ) )
    {
        if( SetPriorityClass( GetCurrentProcess(), ABOVE_NORMAL_PRIORITY_CLASS )
             || SetPriorityClass( GetCurrentProcess(), HIGH_PRIORITY_CLASS ) )
        {
            msg_Dbg( p_this, "raised process priority" );
        }
        else
        {
            msg_Dbg( p_this, "could not raise process priority" );
        }
    }

    if( var_InheritBool( p_this, "one-instance" )
     || ( var_InheritBool( p_this, "one-instance-when-started-from-file" )
       && var_InheritBool( p_this, "started-from-file" ) ) )
    {
        HANDLE hmutex;

        msg_Info( p_this, "one instance mode ENABLED");

        /* Use a named mutex to check if another instance is already running */
        if( !( hmutex = CreateMutex( 0, TRUE, L"VLC ipc "VERSION ) ) )
        {
            /* Failed for some reason. Just ignore the option and go on as
             * normal. */
            msg_Err( p_this, "one instance mode DISABLED "
                     "(mutex couldn't be created)" );
            return;
        }

        if( GetLastError() != ERROR_ALREADY_EXISTS )
        {
            /* We are the 1st instance. */
            static const char typename[] = "ipc helper";
            p_helper =
                vlc_custom_create( p_this, sizeof(vlc_object_t),
                                   VLC_OBJECT_GENERIC, typename );

            /* Run the helper thread */
            hIPCHelperReady = CreateEvent( NULL, FALSE, FALSE, NULL );
            hIPCHelper = _beginthreadex( NULL, 0, IPCHelperThread, p_helper,
                                         0, NULL );
            if( hIPCHelper )
                WaitForSingleObject( hIPCHelperReady, INFINITE );
            else
            {
                msg_Err( p_this, "one instance mode DISABLED "
                         "(IPC helper thread couldn't be created)" );
                vlc_object_release (p_helper);
                p_helper = NULL;
            }
            vlc_object_attach (p_helper, p_this);
            CloseHandle( hIPCHelperReady );

            /* Initialization done.
             * Release the mutex to unblock other instances */
            ReleaseMutex( hmutex );
        }
        else
        {
            /* Another instance is running */

            HWND ipcwindow;

            /* Wait until the 1st instance is initialized */
            WaitForSingleObject( hmutex, INFINITE );

            /* Locate the window created by the IPC helper thread of the
             * 1st instance */
            if( !( ipcwindow = FindWindow( 0, L"VLC ipc "VERSION ) ) )
            {
                msg_Err( p_this, "one instance mode DISABLED "
                         "(couldn't find 1st instance of program)" );
                ReleaseMutex( hmutex );
                return;
            }

            /* We assume that the remaining parameters are filenames
             * and their input options */
            if( i_argc > 0 )
            {
                COPYDATASTRUCT wm_data;
                int i_opt;
                vlc_ipc_data_t *p_data;
                size_t i_data = sizeof (*p_data);

                for( i_opt = 0; i_opt < i_argc; i_opt++ )
                {
                    i_data += sizeof (size_t);
                    i_data += strlen( ppsz_argv[ i_opt ] ) + 1;
                }

                p_data = malloc( i_data );
                p_data->argc = i_argc;
                p_data->enqueue = var_InheritBool( p_this, "playlist-enqueue" );
                i_data = 0;
                for( i_opt = 0; i_opt < i_argc; i_opt++ )
                {
                    size_t i_len = strlen( ppsz_argv[ i_opt ] ) + 1;
                    /* Windows will never switch to an architecture
                     * with stronger alignment requirements, right. */
                    *((size_t *)(p_data->data + i_data)) = i_len;
                    i_data += sizeof (size_t);
                    memcpy( &p_data->data[i_data], ppsz_argv[ i_opt ], i_len );
                    i_data += i_len;
                }
                i_data += sizeof (*p_data);

                /* Send our playlist items to the 1st instance */
                wm_data.dwData = 0;
                wm_data.cbData = i_data;
                wm_data.lpData = p_data;
                SendMessage( ipcwindow, WM_COPYDATA, 0, (LPARAM)&wm_data );
            }

            /* Initialization done.
             * Release the mutex to unblock other instances */
            ReleaseMutex( hmutex );

            /* Bye bye */
            system_End( p_this );
            exit( 0 );
        }
    }

#endif
}

static unsigned __stdcall IPCHelperThread( void *data )
{
    vlc_object_t *p_this = data;
    HWND ipcwindow;
    MSG message;

    ipcwindow =
        CreateWindow( L"STATIC",                     /* name of window class */
                  L"VLC ipc "VERSION,               /* window title bar text */
                  0,                                         /* window style */
                  0,                                 /* default X coordinate */
                  0,                                 /* default Y coordinate */
                  0,                                         /* window width */
                  0,                                        /* window height */
                  NULL,                                  /* no parent window */
                  NULL,                            /* no menu in this window */
                  GetModuleHandle(NULL),  /* handle of this program instance */
                  NULL );                               /* sent to WM_CREATE */

    SetWindowLongPtr( ipcwindow, GWLP_WNDPROC, (LRESULT)WMCOPYWNDPROC );
    SetWindowLongPtr( ipcwindow, GWLP_USERDATA, (LONG_PTR)p_this );

    /* Signal the creation of the thread and events queue */
    SetEvent( hIPCHelperReady );

    while( GetMessage( &message, NULL, 0, 0 ) )
    {
        TranslateMessage( &message );
        DispatchMessage( &message );
    }
    return 0;
}

LRESULT CALLBACK WMCOPYWNDPROC( HWND hwnd, UINT uMsg, WPARAM wParam,
                                LPARAM lParam )
{
    if( uMsg == WM_QUIT  )
    {
        PostQuitMessage( 0 );
    }
    else if( uMsg == WM_COPYDATA )
    {
        COPYDATASTRUCT *pwm_data = (COPYDATASTRUCT*)lParam;
        vlc_object_t *p_this;
        playlist_t *p_playlist;

        p_this = (vlc_object_t *)
            (uintptr_t)GetWindowLongPtr( hwnd, GWLP_USERDATA );

        if( !p_this ) return 0;

        /* Add files to the playlist */
        p_playlist = pl_Get( p_this );

        if( pwm_data->lpData )
        {
            char **ppsz_argv;
            vlc_ipc_data_t *p_data = (vlc_ipc_data_t *)pwm_data->lpData;
            size_t i_data = 0;
            int i_argc = p_data->argc, i_opt, i_options;

            ppsz_argv = (char **)malloc( i_argc * sizeof(char *) );
            for( i_opt = 0; i_opt < i_argc; i_opt++ )
            {
                ppsz_argv[i_opt] = p_data->data + i_data + sizeof(int);
                i_data += sizeof(int) + *((int *)(p_data->data + i_data));
            }

            for( i_opt = 0; i_opt < i_argc; i_opt++ )
            {
                i_options = 0;

                /* Count the input options */
                while( i_opt + i_options + 1 < i_argc &&
                        *ppsz_argv[ i_opt + i_options + 1 ] == ':' )
                {
                    i_options++;
                }

                char *psz_URI = make_URI( ppsz_argv[i_opt] );
                playlist_AddExt( p_playlist, psz_URI,
                        NULL, PLAYLIST_APPEND |
                        ( ( i_opt || p_data->enqueue ) ? 0 : PLAYLIST_GO ),
                        PLAYLIST_END, -1,
                        i_options,
                        (char const **)( i_options ? &ppsz_argv[i_opt+1] : NULL ),
                        VLC_INPUT_OPTION_TRUSTED,
                        true, pl_Unlocked );

                i_opt += i_options;
                free( psz_URI );
            }

            free( ppsz_argv );
        }
    }

    return DefWindowProc( hwnd, uMsg, wParam, lParam );
}

/*****************************************************************************
 * system_End: terminate winsock.
 *****************************************************************************/
void system_End( libvlc_int_t *p_this )
{
    HWND ipcwindow;
    if( p_this )
    {
        free( psz_vlcpath );
        psz_vlcpath = NULL;
    }

    if (p_helper && p_helper->p_parent == VLC_OBJECT(p_this) )
    {
        /* this is the first instance (in a one-instance system)
         * it is the owner of the helper thread
         */
        if( ( ipcwindow = FindWindow( 0, L"VLC ipc "VERSION ) ) != 0 )
        {
            SendMessage( ipcwindow, WM_QUIT, 0, 0 );
        }

        /* FIXME: thread-safety... */
        vlc_object_release (p_helper);
        p_helper = NULL;
    }

#if !defined( UNDER_CE )
    timeEndPeriod(5);
#endif

    WSACleanup();
}
