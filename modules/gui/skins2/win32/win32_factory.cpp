/*****************************************************************************
 * win32_factory.cpp
 *****************************************************************************
 * Copyright (C) 2003 the VideoLAN team
 * $Id: 7e0b5f96e989ad8387ea9828c370dc5fafdab636 $
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

#ifdef WIN32_SKINS

#include "win32_factory.hpp"
#include "win32_graphics.hpp"
#include "win32_timer.hpp"
#include "win32_window.hpp"
#include "win32_tooltip.hpp"
#include "win32_popup.hpp"
#include "win32_loop.hpp"
#include "../src/theme.hpp"
#include "../src/window_manager.hpp"
#include "../src/generic_window.hpp"
#include "../commands/cmd_dialogs.hpp"
#include "../commands/cmd_minimize.hpp"

// Custom message for the notifications of the system tray
#define MY_WM_TRAYACTION (WM_APP + 1)


LRESULT CALLBACK Win32Proc( HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
    // Get pointer to thread info: should only work with the parent window
    intf_thread_t *p_intf = (intf_thread_t *)GetWindowLongPtr( hwnd,
        GWLP_USERDATA );

    // If doesn't exist, treat windows message normally
    if( p_intf == NULL || p_intf->p_sys->p_osFactory == NULL )
    {
        return DefWindowProc( hwnd, uMsg, wParam, lParam );
    }

    Win32Factory *pFactory = (Win32Factory*)Win32Factory::instance( p_intf );

    if( hwnd == pFactory->getParentWindow() )
    {
        if( uMsg == WM_SYSCOMMAND )
        {
            // If closing parent window
            if( wParam == SC_CLOSE )
            {
                libvlc_Quit( p_intf->p_libvlc );
                return 0;
            }
            else if( wParam == SC_MINIMIZE )
            {
                pFactory->minimize();
                return 0;
            }
            else if( wParam == SC_RESTORE )
            {
                pFactory->restore();
                return 0;
            }
            else
            {
                msg_Dbg( p_intf, "WM_SYSCOMMAND %i", wParam );
            }
        }
        // Handle systray notifications
        else if( uMsg == MY_WM_TRAYACTION )
        {
            if( (UINT)lParam == WM_LBUTTONDOWN )
            {
                p_intf->p_sys->p_theme->getWindowManager().raiseAll();
                CmdDlgHidePopupMenu aCmdPopup( p_intf );
                aCmdPopup.execute();
            }
            else if( (UINT)lParam == WM_RBUTTONDOWN )
            {
                CmdDlgShowPopupMenu aCmdPopup( p_intf );
                aCmdPopup.execute();
            }
            else if( (UINT)lParam == WM_LBUTTONDBLCLK )
            {
                CmdRestore aCmdRestore( p_intf );
                aCmdRestore.execute();
            }
        }
    }

    // If hwnd does not match any window or message not processed
    return DefWindowProc( hwnd, uMsg, wParam, lParam );
}


Win32Factory::Win32Factory( intf_thread_t *pIntf ):
    OSFactory( pIntf ), TransparentBlt( NULL ), AlphaBlend( NULL ),
    SetLayeredWindowAttributes( NULL ), m_hParentWindow( NULL ),
    m_dirSep( "\\" )
{
    // see init()
}


bool Win32Factory::init()
{
    // Get instance handle
    m_hInst = GetModuleHandle( NULL );
    if( m_hInst == NULL )
    {
        msg_Err( getIntf(), "Cannot get module handle" );
    }

    // Create window class
    WNDCLASS skinWindowClass;
    skinWindowClass.style = CS_DBLCLKS;
    skinWindowClass.lpfnWndProc = (WNDPROC) Win32Proc;
    skinWindowClass.lpszClassName = _T("SkinWindowClass");
    skinWindowClass.lpszMenuName = NULL;
    skinWindowClass.cbClsExtra = 0;
    skinWindowClass.cbWndExtra = 0;
    skinWindowClass.hbrBackground = NULL;
    skinWindowClass.hCursor = LoadCursor( NULL, IDC_ARROW );
    skinWindowClass.hIcon = LoadIcon( m_hInst, _T("VLC_ICON") );
    skinWindowClass.hInstance = m_hInst;

    // Register class and check it
    if( !RegisterClass( &skinWindowClass ) )
    {
        WNDCLASS wndclass;

        // Check why it failed. If it's because the class already exists
        // then fine, otherwise return with an error.
        if( !GetClassInfo( m_hInst, _T("SkinWindowClass"), &wndclass ) )
        {
            msg_Err( getIntf(), "cannot register window class" );
            return false;
        }
    }

    // Create Window
    m_hParentWindow = CreateWindowEx( WS_EX_TOOLWINDOW, _T("SkinWindowClass"),
        _T("VLC media player"), WS_POPUP | WS_SYSMENU | WS_MINIMIZEBOX,
        -200, -200, 0, 0, 0, 0, m_hInst, 0 );
    if( m_hParentWindow == NULL )
    {
        msg_Err( getIntf(), "cannot create parent window" );
        return false;
    }

    // Store with it a pointer to the interface thread
    SetWindowLongPtr( m_hParentWindow, GWLP_USERDATA, (LONG_PTR)getIntf() );

    // We do it this way otherwise CreateWindowEx will fail
    // if WS_EX_LAYERED is not supported
    SetWindowLongPtr( m_hParentWindow, GWL_EXSTYLE,
                      GetWindowLongPtr( m_hParentWindow, GWL_EXSTYLE ) |
                      WS_EX_LAYERED );

    ShowWindow( m_hParentWindow, SW_SHOW );

    // Initialize the systray icon
    m_trayIcon.cbSize = sizeof( NOTIFYICONDATA );
    m_trayIcon.hWnd = m_hParentWindow;
    m_trayIcon.uID = 42;
    m_trayIcon.uFlags = NIF_ICON|NIF_TIP|NIF_MESSAGE;
    m_trayIcon.uCallbackMessage = MY_WM_TRAYACTION;
    m_trayIcon.hIcon = LoadIcon( m_hInst, _T("VLC_ICON") );
    strcpy( m_trayIcon.szTip, "VLC media player" );

    // Show the systray icon if needed
    if( var_InheritBool( getIntf(), "skins2-systray" ) )
    {
        addInTray();
    }

    // Show the task in the task bar if needed
    if( var_InheritBool( getIntf(), "skins2-taskbar" ) )
    {
        addInTaskBar();
    }

    // Initialize the OLE library (for drag & drop)
    OleInitialize( NULL );

    // We dynamically load msimg32.dll to get a pointer to TransparentBlt()
    m_hMsimg32 = LoadLibrary( _T("msimg32.dll") );
    if( !m_hMsimg32 ||
        !( TransparentBlt =
            (BOOL (WINAPI*)(HDC, int, int, int, int,
                            HDC, int, int, int, int, unsigned int))
            GetProcAddress( m_hMsimg32, _T("TransparentBlt") ) ) )
    {
        TransparentBlt = NULL;
        msg_Dbg( getIntf(), "couldn't find TransparentBlt(), "
                 "falling back to BitBlt()" );
    }
    if( !m_hMsimg32 ||
        !( AlphaBlend =
            (BOOL (WINAPI*)( HDC, int, int, int, int, HDC, int, int,
                              int, int, BLENDFUNCTION ))
            GetProcAddress( m_hMsimg32, _T("AlphaBlend") ) ) )
    {
        AlphaBlend = NULL;
        msg_Dbg( getIntf(), "couldn't find AlphaBlend()" );
    }

    // Idem for user32.dll and SetLayeredWindowAttributes()
    m_hUser32 = LoadLibrary( _T("user32.dll") );
    if( !m_hUser32 ||
        !( SetLayeredWindowAttributes =
            (BOOL (WINAPI *)(HWND, COLORREF, BYTE, DWORD))
            GetProcAddress( m_hUser32, _T("SetLayeredWindowAttributes") ) ) )
    {
        SetLayeredWindowAttributes = NULL;
        msg_Dbg( getIntf(), "couldn't find SetLayeredWindowAttributes()" );
    }

    // Initialize the resource path
    char *datadir = config_GetUserDir( VLC_DATA_DIR );
    m_resourcePath.push_back( (string)datadir + "\\skins" );
    free( datadir );
    datadir = config_GetDataDir( getIntf() );
    m_resourcePath.push_back( (string)datadir + "\\skins" );
    m_resourcePath.push_back( (string)datadir + "\\skins2" );
    m_resourcePath.push_back( (string)datadir + "\\share\\skins" );
    m_resourcePath.push_back( (string)datadir + "\\share\\skins2" );
    free( datadir );

    // All went well
    return true;
}


Win32Factory::~Win32Factory()
{
    // Uninitialize the OLE library
    OleUninitialize();

    // Remove the systray icon
    removeFromTray();

    if( m_hParentWindow ) DestroyWindow( m_hParentWindow );

    // Unload msimg32.dll and user32.dll
    if( m_hMsimg32 )
        FreeLibrary( m_hMsimg32 );
    if( m_hUser32 )
        FreeLibrary( m_hUser32 );
}


OSGraphics *Win32Factory::createOSGraphics( int width, int height )
{
    return new Win32Graphics( getIntf(), width, height );
}


OSLoop *Win32Factory::getOSLoop()
{
    return Win32Loop::instance( getIntf() );
}


void Win32Factory::destroyOSLoop()
{
    Win32Loop::destroy( getIntf() );
}

void Win32Factory::minimize()
{
    /* Make sure no tooltip is visible first */
    getIntf()->p_sys->p_theme->getWindowManager().hideTooltip();

    ShowWindow( m_hParentWindow, SW_MINIMIZE );
}

void Win32Factory::restore()
{
    ShowWindow( m_hParentWindow, SW_RESTORE );
}

void Win32Factory::addInTray()
{
    Shell_NotifyIcon( NIM_ADD, &m_trayIcon );
}

void Win32Factory::removeFromTray()
{
    Shell_NotifyIcon( NIM_DELETE, &m_trayIcon );
}

void Win32Factory::addInTaskBar()
{
    ShowWindow( m_hParentWindow, SW_HIDE );
    SetWindowLongPtr( m_hParentWindow, GWL_EXSTYLE,
                      WS_EX_LAYERED|WS_EX_APPWINDOW );
    ShowWindow( m_hParentWindow, SW_SHOW );
}

void Win32Factory::removeFromTaskBar()
{
    ShowWindow( m_hParentWindow, SW_HIDE );
    SetWindowLongPtr( m_hParentWindow, GWL_EXSTYLE,
                      WS_EX_LAYERED|WS_EX_TOOLWINDOW );
    ShowWindow( m_hParentWindow, SW_SHOW );
}

OSTimer *Win32Factory::createOSTimer( CmdGeneric &rCmd )
{
    return new Win32Timer( getIntf(), rCmd, m_hParentWindow );
}


OSWindow *Win32Factory::createOSWindow( GenericWindow &rWindow, bool dragDrop,
                                        bool playOnDrop, OSWindow *pParent,
                                        GenericWindow::WindowType_t type )
{
    return new Win32Window( getIntf(), rWindow, m_hInst, m_hParentWindow,
                            dragDrop, playOnDrop, (Win32Window*)pParent, type );
}


OSTooltip *Win32Factory::createOSTooltip()
{
    return new Win32Tooltip( getIntf(), m_hInst, m_hParentWindow );
}


OSPopup *Win32Factory::createOSPopup()
{
    // XXX FIXME: this way of getting the handle really sucks!
    // In fact, the clean way would be to have in Builder::addPopup() a call
    // to pPopup->associateToWindow() (to be written)... but the problem is
    // that there is no way to access the OS-dependent window handle from a
    // GenericWindow (we cannot even access the OSWindow).
    if( m_windowMap.begin() == m_windowMap.end() )
    {
        msg_Err( getIntf(), "no window has been created before the popup!" );
        return NULL;
    }

    return new Win32Popup( getIntf(), m_windowMap.begin()->first );
}


int Win32Factory::getScreenWidth() const
{
    return GetSystemMetrics(SM_CXSCREEN);

}


int Win32Factory::getScreenHeight() const
{
    return GetSystemMetrics(SM_CYSCREEN);
}


SkinsRect Win32Factory::getWorkArea() const
{
    RECT r;
    SystemParametersInfo( SPI_GETWORKAREA, 0, &r, 0 );
    // Fill a Rect object
    return  SkinsRect( r.left, r.top, r.right, r.bottom );
}


void Win32Factory::getMousePos( int &rXPos, int &rYPos ) const
{
    POINT mousePos;
    GetCursorPos( &mousePos );
    rXPos = mousePos.x;
    rYPos = mousePos.y;
}


void Win32Factory::changeCursor( CursorType_t type ) const
{
    LPCTSTR id;
    switch( type )
    {
    default:
    case kDefaultArrow: id = IDC_ARROW;    break;
    case kResizeNWSE:   id = IDC_SIZENWSE; break;
    case kResizeNS:     id = IDC_SIZENS;   break;
    case kResizeWE:     id = IDC_SIZEWE;   break;
    case kResizeNESW:   id = IDC_SIZENESW; break;
    }

    HCURSOR hCurs = LoadCursor( NULL, id );
    SetCursor( hCurs );
}


void Win32Factory::rmDir( const string &rPath )
{
    WIN32_FIND_DATA find;
    string file;
    string findFiles = rPath + "\\*";
    HANDLE handle    = FindFirstFile( findFiles.c_str(), &find );

    while( handle != INVALID_HANDLE_VALUE )
    {
        // If file is neither "." nor ".."
        if( strcmp( find.cFileName, "." ) && strcmp( find.cFileName, ".." ) )
        {
            // Set file name
            file = rPath + "\\" + (string)find.cFileName;

            // If file is a directory, delete it recursively
            if( find.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY )
            {
                rmDir( file );
            }
            // Else, it is a file so simply delete it
            else
            {
                DeleteFile( file.c_str() );
            }
        }

        // If no more file in directory, exit while
        if( !FindNextFile( handle, &find ) )
            break;
    }

    // Now directory is empty so can be removed
    FindClose( handle );
    RemoveDirectory( rPath.c_str() );
}

#endif
