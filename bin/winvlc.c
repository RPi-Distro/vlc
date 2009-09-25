/*****************************************************************************
 * winvlc.c: the Windows VLC player
 *****************************************************************************
 * Copyright (C) 1998-2008 the VideoLAN team
 *
 * Authors: Vincent Seguin <seguin@via.ecp.fr>
 *          Samuel Hocevar <sam@zoy.org>
 *          Gildas Bazin <gbazin@videolan.org>
 *          Derk-Jan Hartman <hartman at videolan dot org>
 *          Lots of other people, see the libvlc AUTHORS file
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
#include <vlc/vlc.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

#if !defined(UNDER_CE)
#   define  _WIN32_IE 0x500
#   include <shlobj.h>
#   include <tlhelp32.h>
#   include <wininet.h>
static void check_crashdump();
LONG WINAPI vlc_exception_filter(struct _EXCEPTION_POINTERS *lpExceptionInfo);
#endif

#ifndef UNDER_CE
static char *FromWide (const wchar_t *wide)
{
    size_t len;
    len = WideCharToMultiByte (CP_UTF8, 0, wide, -1, NULL, 0, NULL, NULL);

    char *out = (char *)malloc (len);
    if (out)
        WideCharToMultiByte (CP_UTF8, 0, wide, -1, out, len, NULL, NULL);
    return out;
}
#else
static int parse_cmdline (char *line, char ***argvp)
{
    char **argv = malloc (sizeof (char *));
    int argc = 0;

    while (*line != '\0')
    {
        char quote = 0;

        /* Skips white spaces */
        while (strchr ("\t ", *line))
            line++;
        if (!*line)
            break;

        /* Starts a new parameter */
        argv = realloc (argv, (argc + 2) * sizeof (char *));
        if (*line == '"')
        {
            quote = '"';
            line++;
        }
        argv[argc++] = line;

    more:
            while (*line && !strchr ("\t ", *line))
            line++;

    if (line > argv[argc - 1] && line[-1] == quote)
        /* End of quoted parameter */
        line[-1] = 0;
    else
        if (*line && quote)
    {
        /* Space within a quote */
        line++;
        goto more;
    }
    else
        /* End of unquoted parameter */
        if (*line)
            *line++ = 0;
    }
    argv[argc] = NULL;
    *argvp = argv;
    return argc;
}
#endif

int WINAPI WinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance,
#ifndef UNDER_CE
                    LPSTR lpCmdLine,
#else
                    LPWSTR lpCmdLine,
#endif
                    int nCmdShow )
{
    int argc, ret;
#ifndef UNDER_CE
    wchar_t **wargv = CommandLineToArgvW (GetCommandLine (), &argc);
    if (wargv == NULL)
        return 1;

    char *argv[argc + 1];
    BOOL crash_handling = TRUE;
    int j = 0;
    for (int i = 0; i < argc; i++)
    {
        if(!wcscmp(wargv[i], L"--no-crashdump"))
        {
            crash_handling = FALSE;
        }
        else
        {
            argv[j] = FromWide (wargv[i]);
            j++;
        }
    }

    argc = j;
    argv[argc] = NULL;
    LocalFree (wargv);

    if(crash_handling)
    {
        check_crashdump();
        SetUnhandledExceptionFilter(vlc_exception_filter);
    }

#else
    char **argv, psz_cmdline[wcslen(lpCmdLine) * 4];

    WideCharToMultiByte( CP_UTF8, 0, lpCmdLine, -1,
                         psz_cmdline, sizeof (psz_cmdline), NULL, NULL );

    argc = parse_cmdline (psz_cmdline, &argv);
#endif

    libvlc_exception_t ex, dummy;
    libvlc_exception_init (&ex);
    libvlc_exception_init (&dummy);

    /* Initialize libvlc */
    libvlc_instance_t *vlc;
    vlc = libvlc_new (argc - 1, (const char **)argv + 1, &ex);
    if (vlc != NULL)
    {
        libvlc_add_intf (vlc, "globalhotkeys,none", &ex);
        libvlc_add_intf (vlc, NULL, &ex);
        libvlc_playlist_play (vlc, -1, 0, NULL, &dummy);
        libvlc_wait (vlc);
        libvlc_release (vlc);
    }

    ret = libvlc_exception_raised (&ex);
    libvlc_exception_clear (&ex);
    libvlc_exception_clear (&dummy);

    for (int i = 0; i < argc; i++)
        free (argv[i]);

    (void)hInstance; (void)hPrevInstance; (void)lpCmdLine; (void)nCmdShow;
    return ret;
}

#if !defined( UNDER_CE )

static void get_crashdump_path(wchar_t * wdir)
{
    if( S_OK != SHGetFolderPathW( NULL,
                        CSIDL_APPDATA | CSIDL_FLAG_CREATE,
                        NULL, SHGFP_TYPE_CURRENT, wdir ) )
        fprintf( stderr, "Can't open the vlc conf PATH\n" );

    swprintf( wdir+wcslen( wdir ), L"%s", L"\\vlc\\crashdump" );
}

static void check_crashdump()
{
    wchar_t * wdir = (wchar_t *)malloc(sizeof(wchar_t)*MAX_PATH);
    get_crashdump_path(wdir);

    FILE * fd = _wfopen ( wdir, L"r, ccs=UTF-8" );
    if( fd )
    {
        fclose( fd );
        int answer = MessageBox( NULL, L"VLC media player just crashed." \
        " Do you want to send a bug report to the developers team?",
        L"VLC crash reporting", MB_YESNO);

        if(answer == IDYES)
        {
            HINTERNET Hint = InternetOpen(L"VLC Crash Reporter", INTERNET_OPEN_TYPE_PRECONFIG, NULL,NULL,0);
            if(Hint)
            {
                HINTERNET ftp = InternetConnect(Hint, L"crash.videolan.org", INTERNET_DEFAULT_FTP_PORT,
                                                NULL, NULL, INTERNET_SERVICE_FTP, 0, 0);
                if(ftp)
                {
                    SYSTEMTIME now;
                    GetSystemTime(&now);
                    wchar_t remote_file[MAX_PATH];
                    swprintf( remote_file, L"/crashs/%04d%02d%02d%02d%02d%02d",now.wYear,
                            now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond  );

                    FtpPutFile( ftp, wdir, remote_file, FTP_TRANSFER_TYPE_BINARY, 0);
                    InternetCloseHandle(ftp);
                }
                else
                    fprintf(stderr,"Can't connect to FTP server%d\n",GetLastError());
                InternetCloseHandle(Hint);
            }
        }

        _wremove(wdir);
    }
    free((void *)wdir);
}

/*****************************************************************************
 * vlc_exception_filter: handles unhandled exceptions, like segfaults
 *****************************************************************************/
LONG WINAPI vlc_exception_filter(struct _EXCEPTION_POINTERS *lpExceptionInfo)
{
    if(IsDebuggerPresent())
    {
        //If a debugger is present, pass the exception to the debugger with EXCEPTION_CONTINUE_SEARCH
        return EXCEPTION_CONTINUE_SEARCH;
    }
    else
    {
        fprintf( stderr, "unhandled vlc exception\n" );

        wchar_t * wdir = (wchar_t *)malloc(sizeof(wchar_t)*MAX_PATH);
        get_crashdump_path(wdir);
        FILE * fd = _wfopen ( wdir, L"w, ccs=UTF-8" );
        free((void *)wdir);

        if( !fd )
        {
            fprintf( stderr, "\nerror while opening file" );
            exit( 1 );
        }

        OSVERSIONINFO osvi;
        ZeroMemory( &osvi, sizeof(OSVERSIONINFO) );
        osvi.dwOSVersionInfoSize = sizeof( OSVERSIONINFO );
        GetVersionEx( &osvi );

        fwprintf( fd, L"[version]\nOS=%d.%d.%d.%d.%s\nVLC=" VERSION_MESSAGE, osvi.dwMajorVersion,
                                                               osvi.dwMinorVersion,
                                                               osvi.dwBuildNumber,
                                                               osvi.dwPlatformId,
                                                               osvi.szCSDVersion);

        const CONTEXT *const pContext = (const CONTEXT *)lpExceptionInfo->ContextRecord;
        const EXCEPTION_RECORD *const pException = (const EXCEPTION_RECORD *)lpExceptionInfo->ExceptionRecord;
        /*No nested exceptions for now*/
        fwprintf( fd, L"\n\n[exceptions]\n%08x at %08x",pException->ExceptionCode,
                                                pException->ExceptionAddress );
        if( pException->NumberParameters > 0 )
        {
            unsigned int i;
            for( i = 0; i < pException->NumberParameters; i++ )
                fprintf( fd, " | %08x", pException->ExceptionInformation[i] );
        }

        fwprintf( fd, L"\n\n[context]\nEDI:%08x\nESI:%08x\n" \
                    "EBX:%08x\nEDX:%08x\nECX:%08x\nEAX:%08x\n" \
                    "EBP:%08x\nEIP:%08x\nESP:%08x\n",
                        pContext->Edi,pContext->Esi,pContext->Ebx,
                        pContext->Edx,pContext->Ecx,pContext->Eax,
                        pContext->Ebp,pContext->Eip,pContext->Esp );

        fwprintf( fd, L"\n[stacktrace]\n#EIP|base|module\n" );

        wchar_t module[ 256 ];
        MEMORY_BASIC_INFORMATION mbi ;
        VirtualQuery( (DWORD *)pContext->Eip, &mbi, sizeof( mbi ) ) ;
        HINSTANCE hInstance = mbi.AllocationBase;
        GetModuleFileName( hInstance, module, 256 ) ;
        fwprintf( fd, L"%08x|%s\n", pContext->Eip, module );

        DWORD pEbp = pContext->Ebp;
        DWORD caller = *((DWORD*)pEbp + 1);

        do
        {
            VirtualQuery( (DWORD *)caller, &mbi, sizeof( mbi ) ) ;
            HINSTANCE hInstance = mbi.AllocationBase;
            GetModuleFileName( hInstance, module, 256 ) ;
            fwprintf( fd, L"%08x|%s\n", caller, module );
            pEbp = *(DWORD*)pEbp ;
            caller = *((DWORD*)pEbp + 1) ;
            /*The last EBP points to NULL!*/
        }while(caller);

        fclose( fd );
        fflush( stderr );
        exit( 1 );
    }
}
#endif
