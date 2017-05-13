/*****************************************************************************
 * winvlc.c: the Windows VLC media player
 *****************************************************************************
 * Copyright (C) 1998-2011 the VideoLAN team
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

#ifndef UNICODE
#define UNICODE
#endif

#include <vlc/vlc.h>
#include <windows.h>
#include <shellapi.h>

#ifndef _WIN32_IE
#  define  _WIN32_IE 0x501
#endif
#include <fcntl.h>
#include <io.h>
#include <shlobj.h>
#include <wininet.h>
#define PSAPI_VERSION 1
#include <psapi.h>
#define HeapEnableTerminationOnCorruption (HEAP_INFORMATION_CLASS)1
static void check_crashdump(void);
LONG WINAPI vlc_exception_filter(struct _EXCEPTION_POINTERS *lpExceptionInfo);
static const wchar_t *crashdump_path;

static char *FromWide (const wchar_t *wide)
{
    size_t len;
    len = WideCharToMultiByte (CP_UTF8, 0, wide, -1, NULL, 0, NULL, NULL);

    char *out = (char *)malloc (len);
    if (out)
        WideCharToMultiByte (CP_UTF8, 0, wide, -1, out, len, NULL, NULL);
    return out;
}

#if (_WIN32_WINNT < _WIN32_WINNT_WIN8)
static BOOL SetDefaultDllDirectories_(DWORD flags)
{
    HMODULE h = GetModuleHandle(TEXT("kernel32.dll"));
    if (h == NULL)
        return FALSE;

    BOOL WINAPI (*SetDefaultDllDirectoriesReal)(DWORD);

    SetDefaultDllDirectoriesReal = GetProcAddress(h,
                                                  "SetDefaultDllDirectories");
    if (SetDefaultDllDirectoriesReal == NULL)
        return FALSE;

    return SetDefaultDllDirectoriesReal(flags);
}
# define SetDefaultDllDirectories SetDefaultDllDirectories_

#endif

static void PrioritizeSystem32(void)
{
#ifndef HAVE_PROCESS_MITIGATION_IMAGE_LOAD_POLICY
    typedef struct _PROCESS_MITIGATION_IMAGE_LOAD_POLICY {
      union {
        DWORD  Flags;
        struct {
          DWORD NoRemoteImages  :1;
          DWORD NoLowMandatoryLabelImages  :1;
          DWORD PreferSystem32Images  :1;
          DWORD ReservedFlags  :29;
        };
      };
    } PROCESS_MITIGATION_IMAGE_LOAD_POLICY;
#endif
#if _WIN32_WINNT < _WIN32_WINNT_WIN8
    BOOL WINAPI (*SetProcessMitigationPolicy)(PROCESS_MITIGATION_POLICY, PVOID, SIZE_T);
    HINSTANCE h_Kernel32 = GetModuleHandle(TEXT("kernel32.dll"));
    if ( !h_Kernel32 )
        return;
    SetProcessMitigationPolicy = (BOOL (WINAPI *)(PROCESS_MITIGATION_POLICY, PVOID, SIZE_T))
                                   GetProcAddress(h_Kernel32, "SetProcessMitigationPolicy");
    if (SetProcessMitigationPolicy == NULL)
        return;
#endif
    PROCESS_MITIGATION_IMAGE_LOAD_POLICY m = { .Flags = 0 };
    m.PreferSystem32Images = 1;
    SetProcessMitigationPolicy( 10 /* ProcessImageLoadPolicy */, &m, sizeof( m ) );
}

int WINAPI WinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance,
                    LPSTR lpCmdLine,
                    int nCmdShow )
{
    int argc;

    /* VLC does not change the thread locale, so gettext/libintil will use the
     * user default locale as reference. */
    /* gettext versions 0.18-0.18.1 will use the Windows Vista locale name
     * if the GETTEXT_MUI environment variable is set. If not set or if running
     * on Windows 2000/XP/2003 an hard-coded language ID list is used. This
     * putenv() call may become redundant with later versions of gettext. */
    putenv("GETTEXT_MUI=1");
#ifdef TOP_BUILDDIR
    putenv("VLC_PLUGIN_PATH=Z:"TOP_BUILDDIR"/modules");
    putenv("VLC_DATA_PATH=Z:"TOP_SRCDIR"/share");
#endif

    SetErrorMode(SEM_FAILCRITICALERRORS);
    HeapSetInformation(NULL, HeapEnableTerminationOnCorruption, NULL, 0);

    /* SetProcessDEPPolicy */
    HINSTANCE h_Kernel32 = LoadLibraryW(L"kernel32.dll");
    if(h_Kernel32)
    {
        BOOL (WINAPI * mySetProcessDEPPolicy)( DWORD dwFlags);
        BOOL (WINAPI * mySetDllDirectoryA)(const char* lpPathName);
# define PROCESS_DEP_ENABLE 1

        mySetProcessDEPPolicy = (BOOL WINAPI (*)(DWORD))
                            GetProcAddress(h_Kernel32, "SetProcessDEPPolicy");
        if(mySetProcessDEPPolicy)
            mySetProcessDEPPolicy(PROCESS_DEP_ENABLE);

        /* Do NOT load any library from cwd. */
        mySetDllDirectoryA = (BOOL WINAPI (*)(const char*))
                            GetProcAddress(h_Kernel32, "SetDllDirectoryA");
        if(mySetDllDirectoryA)
            mySetDllDirectoryA("");

        FreeLibrary(h_Kernel32);
    }

    SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_SYSTEM32);
    /***
     * Load DLLs from system32 before any other folder (when possible)
     */
    PrioritizeSystem32();

    /* Args */
    wchar_t **wargv = CommandLineToArgvW (GetCommandLine (), &argc);
    if (wargv == NULL)
        return 1;

    char *argv[argc + 3];
    BOOL crash_handling = TRUE;
    int j = 0;
    char *lang = NULL;

    argv[j++] = FromWide( L"--media-library" );
    argv[j++] = FromWide( L"--no-ignore-config" );
    for (int i = 1; i < argc; i++)
    {
        if(!wcscmp(wargv[i], L"--no-crashdump"))
        {
            crash_handling = FALSE;
            continue; /* don't give argument to libvlc */
        }
        if (!wcsncmp(wargv[i], L"--language", 10) )
        {
            if (i < argc - 1 && wcsncmp( wargv[i + 1], L"--", 2 ))
                lang = FromWide (wargv[++i]);
            continue;
        }

        argv[j++] = FromWide (wargv[i]);
    }

    argc = j;
    argv[argc] = NULL;
    LocalFree (wargv);

    if(crash_handling)
    {
        static wchar_t path[MAX_PATH];
        if( S_OK != SHGetFolderPathW( NULL, CSIDL_APPDATA | CSIDL_FLAG_CREATE,
                    NULL, SHGFP_TYPE_CURRENT, path ) )
            fprintf( stderr, "Can't open the vlc conf PATH\n" );
        _snwprintf( path+wcslen( path ), MAX_PATH,  L"%s", L"\\vlc\\crashdump" );
        crashdump_path = &path[0];

        check_crashdump();
        SetUnhandledExceptionFilter(vlc_exception_filter);
    }

    _setmode( STDIN_FILENO, _O_BINARY ); /* Needed for pipes */

    /* */
    if (!lang)
    {
        HKEY h_key;
        if( RegOpenKeyEx( HKEY_CURRENT_USER, TEXT("Software\\VideoLAN\\VLC\\"), 0, KEY_READ, &h_key )
                == ERROR_SUCCESS )
        {
            TCHAR szData[256];
            DWORD len = 256;
            if( RegQueryValueEx( h_key, TEXT("Lang"), NULL, NULL, (LPBYTE) &szData, &len ) == ERROR_SUCCESS )
                lang = FromWide( szData );
        }
    }

    if (lang && strncmp( lang, "auto", 4 ) )
    {
        char tmp[11];
        snprintf(tmp, 11, "LANG=%s", lang);
        putenv(tmp);
    }
    free(lang);

    /* Initialize libvlc */
    libvlc_instance_t *vlc;
    vlc = libvlc_new (argc, (const char **)argv);
    if (vlc != NULL)
    {
        libvlc_set_app_id (vlc, "org.VideoLAN.VLC", PACKAGE_VERSION,
                           PACKAGE_NAME);
        libvlc_set_user_agent (vlc, "VLC media player", "VLC/"PACKAGE_VERSION);
        libvlc_add_intf (vlc, "hotkeys,none");
        libvlc_add_intf (vlc, "globalhotkeys,none");
        libvlc_add_intf (vlc, NULL);
        libvlc_playlist_play (vlc, -1, 0, NULL);
        libvlc_wait (vlc);
        libvlc_release (vlc);
    }
    else
        MessageBox (NULL, TEXT("VLC media player could not start.\n"
                    "Either the command line options were invalid or no plugins were found.\n"),
                    TEXT("VLC media player"),
                    MB_OK|MB_ICONERROR);


    for (int i = 0; i < argc; i++)
        free (argv[i]);

    (void)hInstance; (void)hPrevInstance; (void)lpCmdLine; (void)nCmdShow;
    return 0;
}

/* Crashdumps handling */
static void check_crashdump(void)
{
    wchar_t mv_crashdump_path[MAX_PATH];
    wcscpy (mv_crashdump_path, crashdump_path);
    wcscat (mv_crashdump_path, L".mv");

    if (_wrename (crashdump_path, mv_crashdump_path))
        return;

    FILE * fd = _wfopen ( mv_crashdump_path, L"r, ccs=UTF-8" );
    if( !fd )
        return;
    fclose( fd );

    int answer = MessageBox( NULL, L"Ooops: VLC media player just crashed.\n" \
    "Would you like to send a bug report to the developers team?",
    L"VLC crash reporting", MB_YESNO);

    if(answer == IDYES)
    {
        HMODULE hWininet = LoadLibrary(TEXT("wininet.dll"));
        if (hWininet == NULL)
        {
            fprintf(stderr, "There was an error loading the network"
                    " 0x%08lx\n", (unsigned long)GetLastError());
            goto done;
        }

        HINTERNET (WINAPI *InternetOpenW_)(LPCWSTR ,DWORD dwAccessType,LPCWSTR lpszProxy,LPCWSTR lpszProxyBypass,DWORD dwFlags);
        HINTERNET (WINAPI *InternetConnectW_)(HINTERNET hInternet,LPCWSTR lpszServerName,INTERNET_PORT nServerPort,LPCWSTR lpszUserName,LPCWSTR lpszPassword,DWORD dwService,DWORD dwFlags,DWORD_PTR dwContext);
        BOOL (WINAPI *InternetCloseHandle_)(HINTERNET hInternet);
        BOOL (WINAPI *FtpPutFileW_)(HINTERNET hConnect,LPCWSTR lpszLocalFile,LPCWSTR lpszNewRemoteFile,DWORD dwFlags,DWORD_PTR dwContext);
        InternetOpenW_       = (void*)GetProcAddress(hWininet, "InternetOpenW");
        InternetConnectW_    = (void*)GetProcAddress(hWininet, "InternetConnectW");
        InternetCloseHandle_ = (void*)GetProcAddress(hWininet, "InternetCloseHandle");
        FtpPutFileW_         = (void*)GetProcAddress(hWininet, "FtpPutFileW");
        if (!InternetOpenW_ || !InternetConnectW_ || !InternetCloseHandle_ || !FtpPutFileW_)
        {
            fprintf(stderr, "There was an error loading the network API entries"
                    " 0x%08lx\n", (unsigned long)GetLastError());
            goto done;
        }

        HINTERNET Hint = InternetOpenW_(L"VLC Crash Reporter",
                INTERNET_OPEN_TYPE_PRECONFIG, NULL,NULL,0);
        if(Hint)
        {
            HINTERNET ftp = InternetConnectW_(Hint, L"crash.videolan.org",
                        INTERNET_DEFAULT_FTP_PORT, NULL, NULL,
                        INTERNET_SERVICE_FTP, INTERNET_FLAG_PASSIVE, 0);
            if(ftp)
            {
                SYSTEMTIME now;
                GetSystemTime(&now);
                wchar_t remote_file[MAX_PATH];
                _snwprintf(remote_file, MAX_PATH,
                        L"/crashes-win32/%04d%02d%02d%02d%02d%02d",
                        now.wYear, now.wMonth, now.wDay, now.wHour,
                        now.wMinute, now.wSecond );

                if( FtpPutFileW_( ftp, mv_crashdump_path, remote_file,
                            FTP_TRANSFER_TYPE_BINARY, 0) )
                    fprintf(stderr, "Report sent correctly to FTP.\n");
                else
                    MessageBox( NULL, L"There was an error while "\
                                "transferring the data to the FTP server.\n"\
                                "Thanks a lot for the help.",
                                L"Report sending failed", MB_OK);
                InternetCloseHandle_(ftp);
            }
            else
            {
                MessageBox( NULL, L"There was an error while connecting to " \
                                "the FTP server. "\
                                "Thanks a lot for the help.",
                                L"Report sending failed", MB_OK);
                fprintf(stderr,"Can't connect to FTP server 0x%08lu\n",
                        (unsigned long)GetLastError());
            }
            InternetCloseHandle_(Hint);
        }
        else
        {
              MessageBox( NULL, L"There was an error while connecting to the Internet.\n"\
                                "Thanks a lot for the help anyway.",
                                L"Report sending failed", MB_OK);
        }
done:
        if (hWininet != NULL)
            FreeLibrary(hWininet);
        MessageBox( NULL, L"Thanks a lot for helping improving VLC!",
                    L"VLC crash report" , MB_OK);
    }

    _wremove(mv_crashdump_path);
}

/*****************************************************************************
 * vlc_exception_filter: handles unhandled exceptions, like segfaults
 *****************************************************************************/
LONG WINAPI vlc_exception_filter(struct _EXCEPTION_POINTERS *lpExceptionInfo)
{
    if(IsDebuggerPresent())
    {
        //If a debugger is present, pass the exception to the debugger
        //with EXCEPTION_CONTINUE_SEARCH
        return EXCEPTION_CONTINUE_SEARCH;
    }
    else
    {
        fprintf( stderr, "unhandled vlc exception\n" );

        FILE * fd = _wfopen ( crashdump_path, L"w, ccs=UTF-8" );

        if( !fd )
        {
            fprintf( stderr, "\nerror while opening file" );
            exit( 1 );
        }

        OSVERSIONINFO osvi;
        ZeroMemory( &osvi, sizeof(OSVERSIONINFO) );
        osvi.dwOSVersionInfoSize = sizeof( OSVERSIONINFO );
        GetVersionEx( &osvi );

        fwprintf( fd, L"[version]\nOS=%d.%d.%d.%d.%ls\nVLC=" VERSION_MESSAGE,
                osvi.dwMajorVersion, osvi.dwMinorVersion, osvi.dwBuildNumber,
                osvi.dwPlatformId, osvi.szCSDVersion);

        const CONTEXT *const pContext = (const CONTEXT *)
            lpExceptionInfo->ContextRecord;
        const EXCEPTION_RECORD *const pException = (const EXCEPTION_RECORD *)
            lpExceptionInfo->ExceptionRecord;
        /* No nested exceptions for now */
        fwprintf( fd, L"\n\n[exceptions]\n%08x at %px", 
                pException->ExceptionCode, pException->ExceptionAddress );

        for( unsigned int i = 0; i < pException->NumberParameters; i++ )
            fwprintf( fd, L" | %p", pException->ExceptionInformation[i] );

#ifdef _WIN64
        fwprintf( fd, L"\n\n[context]\nRDI:%px\nRSI:%px\n" \
                    "RBX:%px\nRDX:%px\nRCX:%px\nRAX:%px\n" \
                    "RBP:%px\nRIP:%px\nRSP:%px\nR8:%px\n" \
                    "R9:%px\nR10:%px\nR11:%px\nR12:%px\n" \
                    "R13:%px\nR14:%px\nR15:%px\n",
                        pContext->Rdi,pContext->Rsi,pContext->Rbx,
                        pContext->Rdx,pContext->Rcx,pContext->Rax,
                        pContext->Rbp,pContext->Rip,pContext->Rsp,
                        pContext->R8,pContext->R9,pContext->R10,
                        pContext->R11,pContext->R12,pContext->R13,
                        pContext->R14,pContext->R15 );
#else
        fwprintf( fd, L"\n\n[context]\nEDI:%px\nESI:%px\n" \
                    "EBX:%px\nEDX:%px\nECX:%px\nEAX:%px\n" \
                    "EBP:%px\nEIP:%px\nESP:%px\n",
                        pContext->Edi,pContext->Esi,pContext->Ebx,
                        pContext->Edx,pContext->Ecx,pContext->Eax,
                        pContext->Ebp,pContext->Eip,pContext->Esp );
#endif

        fwprintf( fd, L"\n[stacktrace]\n#EIP|base|module\n" );

#ifdef _WIN64
        LPCVOID caller = (LPCVOID)pContext->Rip;
        LPVOID *pBase  = (LPVOID*)pContext->Rbp;
#else
        LPVOID *pBase  = (LPVOID*)pContext->Ebp;
        LPCVOID caller = (LPCVOID)pContext->Eip;
#endif
        for( unsigned frame = 0; frame <= 100; frame++ )
        {
            MEMORY_BASIC_INFORMATION mbi;
            wchar_t module[ 256 ];
            VirtualQuery( caller, &mbi, sizeof( mbi ) ) ;
            GetModuleFileName( mbi.AllocationBase, module, 256 );
            fwprintf( fd, L"%p|%ls\n", caller, module );

            if( IsBadReadPtr( pBase, 2 * sizeof( void* ) ) )
                break;

            /*The last BP points to NULL!*/
            caller = *(pBase + 1);
            if( !caller )
                break;
            pBase = *pBase;
            if( !pBase )
                break;
        }

        HANDLE hpid = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ,
                                        FALSE, GetCurrentProcessId());
        if (hpid) {
            HMODULE mods[1024];
            DWORD size;
            if (EnumProcessModules(hpid, mods, sizeof(mods), &size)) {
                fwprintf( fd, L"\n\n[modules]\n" );
                for (unsigned int i = 0; i < size / sizeof(HMODULE); i++) {
                    wchar_t module[ 256 ];
                    GetModuleFileName(mods[i], module, 256);
                    fwprintf( fd, L"%p|%ls\n", mods[i], module);
                }
            }
            CloseHandle(hpid);
        }

        fclose( fd );
        fflush( stderr );
        exit( 1 );
    }
}
