/*****************************************************************************
 * vlc_win32_fullscreen.h: a VLC plugin for Mozilla
 *****************************************************************************
 * Copyright (C) 2002-2011 the VideoLAN team
 * $Id: a679f598c1422a2e78547c1186e48781d1ef8742 $
 *
 * Authors: Sergey Radionov <rsatom@gmail.com>
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
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef _WIN32
#include <windows.h>
#include <commctrl.h>
#include <uxtheme.h>

#include <vlc/vlc.h>

#include "vlc_win32_fullscreen.h"

/////////////////////////////////
//VLCHolderWnd static members
HINSTANCE VLCHolderWnd::_hinstance = 0;
ATOM VLCHolderWnd::_holder_wndclass_atom = 0;


void VLCHolderWnd::RegisterWndClassName(HINSTANCE hInstance)
{
    //save hInstance for future use
    _hinstance = hInstance;

    WNDCLASS wClass;

    if( ! GetClassInfo(_hinstance, getClassName(), &wClass) )
    {
        wClass.style          = 0;//CS_NOCLOSE|CS_DBLCLKS;
        wClass.lpfnWndProc    = VLCHolderClassWndProc;
        wClass.cbClsExtra     = 0;
        wClass.cbWndExtra     = 0;
        wClass.hInstance      = _hinstance;
        wClass.hIcon          = NULL;
        wClass.hCursor        = LoadCursor(NULL, IDC_ARROW);
        wClass.hbrBackground  = (HBRUSH)(COLOR_3DFACE+1);
        wClass.lpszMenuName   = NULL;
        wClass.lpszClassName  = getClassName();

        _holder_wndclass_atom = RegisterClass(&wClass);
    }
    else
    {
        _holder_wndclass_atom = 0;
    }
}

void VLCHolderWnd::UnRegisterWndClassName()
{
    if(0 != _holder_wndclass_atom){
        UnregisterClass(MAKEINTATOM(_holder_wndclass_atom), _hinstance);
        _holder_wndclass_atom = 0;
    }
}

VLCHolderWnd* VLCHolderWnd::CreateHolderWindow(HWND hParentWnd/*VLCPlugin* p_instance*/)
{
    HWND hWnd = CreateWindow(getClassName(),
                             TEXT("Holder Window"),
                             WS_CHILD|WS_CLIPCHILDREN|WS_CLIPSIBLINGS|WS_VISIBLE|WS_DISABLED,
                             0, 0, 0, 0,
                             hParentWnd,
                             0,
                             VLCHolderWnd::_hinstance,
                             0
                             );

    if(hWnd)
        return reinterpret_cast<VLCHolderWnd*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));

    return 0;
}

LRESULT CALLBACK VLCHolderWnd::VLCHolderClassWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    VLCHolderWnd* h_data = reinterpret_cast<VLCHolderWnd*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));

    switch( uMsg )
    {
        case WM_CREATE:{
            CREATESTRUCT* CreateStruct = (CREATESTRUCT*)(lParam);

            h_data = new VLCHolderWnd(hWnd);
            SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(h_data));

            RECT ParentClientRect;
            GetClientRect(CreateStruct->hwndParent, &ParentClientRect);
            MoveWindow(hWnd, 0, 0, (ParentClientRect.right-ParentClientRect.left), (ParentClientRect.bottom-ParentClientRect.top), FALSE);
            break;
        }
        case WM_NCDESTROY:
            delete h_data;
            SetWindowLongPtr(hWnd, GWLP_USERDATA, 0);
            break;
        default:
            return DefWindowProc(hWnd, uMsg, wParam, lParam);
    }
};

/////////////////////////////////
//VLCFullScreenWnd static members
HINSTANCE VLCFullScreenWnd::_hinstance = 0;
ATOM VLCFullScreenWnd::_fullscreen_wndclass_atom = 0;
ATOM VLCFullScreenWnd::_fullscreen_controls_wndclass_atom = 0;

bool VLCFullScreenWnd::handle_position_changed_event_enabled = true;
void VLCFullScreenWnd::RegisterWndClassName(HINSTANCE hInstance)
{
    //save hInstance for future use
    _hinstance = hInstance;

    WNDCLASS wClass;

    memset(&wClass, 0 , sizeof(wClass));
    if( ! GetClassInfo(_hinstance,  getClassName(), &wClass) )
    {
        wClass.style          = CS_NOCLOSE|CS_DBLCLKS;
        wClass.lpfnWndProc    = FSWndWindowProc;
        wClass.cbClsExtra     = 0;
        wClass.cbWndExtra     = 0;
        wClass.hInstance      = _hinstance;
        wClass.hIcon          = NULL;
        wClass.hCursor        = LoadCursor(NULL, IDC_ARROW);
        wClass.hbrBackground  = (HBRUSH)(COLOR_3DFACE+1);
        wClass.lpszMenuName   = NULL;
        wClass.lpszClassName  = getClassName();

        _fullscreen_wndclass_atom = RegisterClass(&wClass);
    }
    else
    {
        _fullscreen_wndclass_atom = 0;
    }

    memset(&wClass, 0 , sizeof(wClass));
    if( ! GetClassInfo(_hinstance,  getControlsClassName(), &wClass) )
    {
        wClass.style          = CS_NOCLOSE;
        wClass.lpfnWndProc    = FSControlsWndWindowProc;
        wClass.cbClsExtra     = 0;
        wClass.cbWndExtra     = 0;
        wClass.hInstance      = _hinstance;
        wClass.hIcon          = NULL;
        wClass.hCursor        = LoadCursor(NULL, IDC_ARROW);
        wClass.hbrBackground  = (HBRUSH)(COLOR_3DFACE+1);
        wClass.lpszMenuName   = NULL;
        wClass.lpszClassName  = getControlsClassName();

        _fullscreen_controls_wndclass_atom = RegisterClass(&wClass);
    }
    else
    {
        _fullscreen_controls_wndclass_atom = 0;
    }
}
void VLCFullScreenWnd::UnRegisterWndClassName()
{
    if(0 != _fullscreen_wndclass_atom){
        UnregisterClass(MAKEINTATOM(_fullscreen_wndclass_atom), _hinstance);
        _fullscreen_wndclass_atom = 0;
    }

    if(0 != _fullscreen_controls_wndclass_atom){
        UnregisterClass(MAKEINTATOM(_fullscreen_controls_wndclass_atom), _hinstance);
        _fullscreen_controls_wndclass_atom = 0;
    }
}

LRESULT CALLBACK VLCFullScreenWnd::FSWndWindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    VLCFullScreenWnd* fs_data = reinterpret_cast<VLCFullScreenWnd *>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
    //VLCPlugin* p_instance = fs_data ? fs_data->_p_instance : 0;

    switch( uMsg )
    {
        case WM_CREATE:{
            CREATESTRUCT* CreateStruct = (CREATESTRUCT*)(lParam);
            //VLCHolderWnd* HolderWnd = (VLCHolderWnd*)CreateStruct->lpCreateParams;
            VLCWindowsManager* WM = (VLCWindowsManager*)CreateStruct->lpCreateParams;

            fs_data = new VLCFullScreenWnd(hWnd, WM);
            SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(fs_data));

            fs_data->hControlsWnd =
                CreateWindow(fs_data->getControlsClassName(),
                        TEXT("VLC ActiveX Full Screen Controls Window"),
                        WS_VISIBLE|WS_CHILD|WS_CLIPSIBLINGS,
                        0,
                        0,
                        0, 0,
                        hWnd,
                        0,
                        VLCFullScreenWnd::_hinstance,
                        (LPVOID) fs_data);

            break;
        }
        case WM_NCDESTROY:
            delete fs_data;
            SetWindowLongPtr(hWnd, GWLP_USERDATA, 0);
            break;
        case WM_MOUSEMOVE:{
            if(fs_data->Last_WM_MOUSEMOVE_lParam!=lParam){
                fs_data->Last_WM_MOUSEMOVE_lParam = lParam;
                fs_data->NeedShowControls();
            }
            break;
        }
        case WM_SHOWWINDOW:{
            if(FALSE==wParam){ //hidding
                fs_data->UnRegisterEvents();
                break;
            }
            else{//showing
                fs_data->RegisterEvents();
            }

            fs_data->NeedShowControls();

            //simulate lParam for WM_SIZE
            RECT ClientRect;
            GetClientRect(hWnd, &ClientRect);
            lParam = MAKELPARAM(ClientRect.right, ClientRect.bottom);
        }
        case WM_SIZE:{
            if(fs_data->_WindowsManager->IsFullScreen()){
                int new_client_width = LOWORD(lParam);
                int new_client_height = HIWORD(lParam);
                VLCHolderWnd* HolderWnd =  fs_data->_WindowsManager->getHolderWnd();
                SetWindowPos(HolderWnd->getHWND(), HWND_BOTTOM, 0, 0, new_client_width, new_client_height, SWP_NOACTIVATE|SWP_NOOWNERZORDER);
            }
            break;
        }
        case WM_LBUTTONDBLCLK:{
            fs_data->_WindowsManager->ToggleFullScreen();
            break;
        }
        default:
            return DefWindowProc(hWnd, uMsg, wParam, lParam);
    }
    return 0L;
};

#define ID_FS_SWITCH_FS 1
#define ID_FS_PLAY_PAUSE 2
#define ID_FS_VIDEO_POS_SCROLL 3
#define ID_FS_MUTE 4
#define ID_FS_VOLUME 5
#define ID_FS_LABEL 5

LRESULT CALLBACK VLCFullScreenWnd::FSControlsWndWindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    VLCFullScreenWnd* fs_data = reinterpret_cast<VLCFullScreenWnd *>(GetWindowLongPtr(hWnd, GWLP_USERDATA));

    switch( uMsg )
    {
        case WM_CREATE:{
            CREATESTRUCT* CreateStruct = (CREATESTRUCT*)(lParam);
            fs_data = (VLCFullScreenWnd*)CreateStruct->lpCreateParams;
            HMODULE hDllModule = fs_data->_WindowsManager->getHModule();

            SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(fs_data));

            fs_data->hNewMessageBitmap = (HICON)
                LoadImage(hDllModule, MAKEINTRESOURCE(8),
                          IMAGE_BITMAP, 0, 0, LR_LOADMAP3DCOLORS);

            const int ControlsHeigth = 21+2;
            const int ButtonsWidth = ControlsHeigth;
            const int ControlsSpace = 5;
            const int ScrollVOffset = (ControlsHeigth-GetSystemMetrics(SM_CXHSCROLL))/2;

            int HorizontalOffset = ControlsSpace;
            int ControlWidth = ButtonsWidth;
            fs_data->hFSButton =
                CreateWindow(TEXT("BUTTON"), TEXT("End Full Screen"), WS_CHILD|WS_VISIBLE|BS_BITMAP|BS_FLAT,
                             HorizontalOffset, ControlsSpace, ControlWidth, ControlsHeigth, hWnd, (HMENU)ID_FS_SWITCH_FS, 0, 0);
            fs_data->hFSButtonBitmap =
                LoadImage(hDllModule, MAKEINTRESOURCE(3),
                          IMAGE_BITMAP, 0, 0, LR_LOADMAP3DCOLORS);
            SendMessage(fs_data->hFSButton, BM_SETIMAGE, (WPARAM)IMAGE_BITMAP, (LPARAM)fs_data->hFSButtonBitmap);
            //SendMessage(fs_data->hFSButton, BM_SETIMAGE, (WPARAM)IMAGE_BITMAP, (LPARAM)fs_data->hNewMessageBitmap);
            HorizontalOffset+=ControlWidth+ControlsSpace;

            ControlWidth = ButtonsWidth;
            fs_data->hPlayPauseButton =
                CreateWindow(TEXT("BUTTON"), TEXT("Play/Pause"), WS_CHILD|WS_VISIBLE|BS_BITMAP|BS_FLAT,
                             HorizontalOffset, ControlsSpace, ControlWidth, ControlsHeigth, hWnd, (HMENU)ID_FS_PLAY_PAUSE, 0, 0);
            fs_data->hPlayBitmap =
                LoadImage(hDllModule, MAKEINTRESOURCE(4),
                          IMAGE_BITMAP, 0, 0, LR_LOADMAP3DCOLORS);
            fs_data->hPauseBitmap =
                LoadImage(hDllModule, MAKEINTRESOURCE(5),
                          IMAGE_BITMAP, 0, 0, LR_LOADMAP3DCOLORS);
            SendMessage(fs_data->hPlayPauseButton, BM_SETIMAGE, (WPARAM)IMAGE_BITMAP, (LPARAM)fs_data->hPauseBitmap);
            HorizontalOffset+=ControlWidth+ControlsSpace;

            ControlWidth = 200;
            int VideoPosControlHeight = 12;
            fs_data->hVideoPosScroll =
                CreateWindow(PROGRESS_CLASS, TEXT("Video Position"), WS_CHILD|WS_DISABLED|WS_VISIBLE|SBS_HORZ|SBS_TOPALIGN|PBS_SMOOTH,
                             HorizontalOffset, ControlsSpace+(ControlsHeigth-VideoPosControlHeight)/2, ControlWidth, VideoPosControlHeight, hWnd, (HMENU)ID_FS_VIDEO_POS_SCROLL, 0, 0);
            HMODULE hThModule = LoadLibrary(TEXT("UxTheme.dll"));
            if(hThModule){
                FARPROC proc = GetProcAddress(hThModule, "SetWindowTheme");
                typedef HRESULT (WINAPI* SetWindowThemeProc)(HWND, LPCWSTR, LPCWSTR);
                if(proc){
                    //SetWindowTheme(fs_data->hVideoPosScroll, L"", L"");
                    ((SetWindowThemeProc)proc)(fs_data->hVideoPosScroll, L"", L"");
                }
                FreeLibrary(hThModule);
            }
            HorizontalOffset+=ControlWidth+ControlsSpace;

            ControlWidth = ButtonsWidth;
            fs_data->hMuteButton =
                CreateWindow(TEXT("BUTTON"), TEXT("Mute"), WS_CHILD|WS_VISIBLE|BS_AUTOCHECKBOX|BS_PUSHLIKE|BS_BITMAP, //BS_FLAT
                             HorizontalOffset, ControlsSpace, ControlWidth, ControlsHeigth, hWnd, (HMENU)ID_FS_MUTE, 0, 0);
            fs_data->hVolumeBitmap =
                LoadImage(hDllModule, MAKEINTRESOURCE(6),
                          IMAGE_BITMAP, 0, 0, LR_LOADMAP3DCOLORS|LR_SHARED);
            fs_data->hVolumeMutedBitmap =
                LoadImage(hDllModule, MAKEINTRESOURCE(7),
                          IMAGE_BITMAP, 0, 0, LR_LOADMAP3DCOLORS|LR_SHARED);
            SendMessage(fs_data->hMuteButton, BM_SETIMAGE, (WPARAM)IMAGE_BITMAP, (LPARAM)fs_data->hVolumeBitmap);
            HorizontalOffset+=ControlWidth+ControlsSpace;

            ControlWidth = 100;
            fs_data->hVolumeSlider =
                CreateWindow(TRACKBAR_CLASS, TEXT("Volume"), WS_CHILD|WS_VISIBLE|TBS_HORZ|TBS_BOTTOM|TBS_AUTOTICKS,
                             HorizontalOffset, ControlsSpace, ControlWidth, 21, hWnd, (HMENU)ID_FS_VOLUME, 0, 0);
            HorizontalOffset+=ControlWidth+ControlsSpace;
            SendMessage(fs_data->hVolumeSlider, TBM_SETRANGE, FALSE, (LPARAM) MAKELONG (0, 100));
            SendMessage(fs_data->hVolumeSlider, TBM_SETTICFREQ, (WPARAM) 10, 0);
            SetWindowPos(hWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOSIZE|SWP_NOMOVE|SWP_NOACTIVATE|SWP_NOOWNERZORDER);

            int ControlWndWidth = HorizontalOffset;
            int ControlWndHeigth = ControlsSpace+ControlsHeigth+ControlsSpace;
            SetWindowPos(hWnd, HWND_TOPMOST, (GetSystemMetrics(SM_CXSCREEN)-ControlWndWidth)/2, 0,
                         ControlWndWidth, ControlWndHeigth, SWP_NOZORDER|SWP_NOOWNERZORDER|SWP_NOACTIVATE);

            //new message blinking timer
            SetTimer(hWnd, 2, 500, NULL);

            fs_data->CreateToolTip();

            break;
        }
        case WM_MOUSEMOVE:{
            fs_data->Last_WM_MOUSEMOVE_lParam = lParam;
            fs_data->NeedShowControls();//not allow close controll window while mouse within
            break;
        }
        case WM_LBUTTONUP:{
            POINT BtnUpPoint = {LOWORD(lParam), HIWORD(lParam)};
            RECT VideoPosRect;
            GetWindowRect(fs_data->hVideoPosScroll, &VideoPosRect);
            ClientToScreen(hWnd, &BtnUpPoint);
            if(PtInRect(&VideoPosRect, BtnUpPoint)){
                fs_data->SetVideoPos(float(BtnUpPoint.x-VideoPosRect.left)/(VideoPosRect.right-VideoPosRect.left));
            }
            break;
        }
        case WM_TIMER:{
            switch(wParam){
                case 1:{
                    POINT MousePoint;
                    GetCursorPos(&MousePoint);
                    RECT ControlWndRect;
                    GetWindowRect(fs_data->hControlsWnd, &ControlWndRect);
                    if(PtInRect(&ControlWndRect, MousePoint)||GetCapture()==fs_data->hVolumeSlider){
                        fs_data->NeedShowControls();//not allow close control window while mouse within
                    }
                    else{
                        fs_data->NeedHideControls();
                    }
                    break;
                }
                case 2:{
                    LRESULT lResult = SendMessage(fs_data->hFSButton, BM_GETIMAGE, (WPARAM)IMAGE_BITMAP, 0);
                    if((HANDLE)lResult == fs_data->hFSButtonBitmap){
                        if(fs_data->_WindowsManager->getNewMessageFlag()){
                            SendMessage(fs_data->hFSButton, BM_SETIMAGE, (WPARAM)IMAGE_BITMAP, (LPARAM)fs_data->hNewMessageBitmap);
                            fs_data->NeedShowControls();//not allow close controll window while new message exist
                        }
                    }
                    else{
                        SendMessage(fs_data->hFSButton, BM_SETIMAGE, (WPARAM)IMAGE_BITMAP, (LPARAM)fs_data->hFSButtonBitmap);
                    }

                    break;
                }
            }
            break;
        }
        case WM_SETCURSOR:{
            RECT VideoPosRect;
            GetWindowRect(fs_data->hVideoPosScroll, &VideoPosRect);
            DWORD dwMsgPos = GetMessagePos();
            POINT MsgPosPoint = {LOWORD(dwMsgPos), HIWORD(dwMsgPos)};
            if(PtInRect(&VideoPosRect, MsgPosPoint)){
                SetCursor(LoadCursor(NULL, IDC_HAND));
                return TRUE;
            }
            else{
                return DefWindowProc(hWnd, uMsg, wParam, lParam);
            }
            break;
        }
        case WM_NCDESTROY:
            break;
        case WM_COMMAND:{
            WORD NCode = HIWORD(wParam);
            WORD Control = LOWORD(wParam);
            switch(NCode){
                case BN_CLICKED:{
                    switch(Control){
                        case ID_FS_SWITCH_FS:
                            fs_data->_WindowsManager->ToggleFullScreen();
                            break;
                        case ID_FS_PLAY_PAUSE:{
                            libvlc_media_player_t* p_md = fs_data->getMD();
                            if( p_md ){
                                if(fs_data->IsPlaying()) libvlc_media_player_pause(p_md);
                                else libvlc_media_player_play(p_md);
                            }
                            break;
                        }
                        case ID_FS_MUTE:{
                            libvlc_media_player_t* p_md = fs_data->getMD();
                            if( p_md ){
                                libvlc_audio_set_mute(p_md, IsDlgButtonChecked(hWnd, ID_FS_MUTE));
                                fs_data->SyncVolumeSliderWithVLCVolume();
                            }
                            break;
                        }
                    }
                    break;
                }
            }
            break;
        }
        case WM_HSCROLL:
        case WM_VSCROLL: {
            libvlc_media_player_t* p_md = fs_data->getMD();
            if( p_md ){
                if(fs_data->hVolumeSlider==(HWND)lParam){
                    LRESULT SliderPos = SendMessage(fs_data->hVolumeSlider, (UINT) TBM_GETPOS, 0, 0);
                    fs_data->SetVLCVolumeBySliderPos(SliderPos);
                }
            }
            break;
        }
        default:
            return DefWindowProc(hWnd, uMsg, wParam, lParam);
    }
    return 0L;
};

VLCFullScreenWnd* VLCFullScreenWnd::CreateFSWindow(VLCWindowsManager* WM)
{
    HWND hWnd = CreateWindow(getClassName(),
                TEXT("VLC ActiveX Full Screen Window"),
                WS_POPUP|WS_CLIPCHILDREN,
                0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN),
                0,
                0,
                VLCFullScreenWnd::_hinstance,
                (LPVOID)WM
               );
    if(hWnd)
        return reinterpret_cast<VLCFullScreenWnd*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));

    return 0;
}

/////////////////////////////////////
//VLCFullScreenWnd members
VLCFullScreenWnd::~VLCFullScreenWnd()
{
    if(hToolTipWnd){
        ::DestroyWindow(hToolTipWnd);
        hToolTipWnd = 0;
    }

    DestroyIcon(hNewMessageBitmap);
    hNewMessageBitmap = 0;
    DeleteObject(hFSButtonBitmap);
    hFSButtonBitmap = 0;

    DeleteObject(hPauseBitmap);
    hPauseBitmap = 0;
    DeleteObject(hPlayBitmap);
    hPlayBitmap = 0;

    DeleteObject(hVolumeBitmap);
    hVolumeBitmap = 0;
    DeleteObject(hVolumeMutedBitmap);
    hVolumeMutedBitmap = 0;
}

libvlc_media_player_t* VLCFullScreenWnd::getMD()
{
    return _WindowsManager->getMD();
}

void VLCFullScreenWnd::NeedShowControls()
{
    if(!IsWindowVisible(hControlsWnd)){
        libvlc_media_player_t* p_md = getMD();
        if( p_md ){
            if(hVideoPosScroll){
                SetVideoPosScrollRangeByVideoLen();
                SyncVideoPosScrollPosWithVideoPos();
            }
            if(hVolumeSlider){
                SyncVolumeSliderWithVLCVolume();
            }
            if(hPlayPauseButton){
                SendMessage(hPlayPauseButton, BM_SETIMAGE, (WPARAM)IMAGE_BITMAP, (LPARAM)(IsPlaying()?hPauseBitmap:hPlayBitmap));
            }
        }
        ShowWindow(hControlsWnd, SW_SHOW);
    }
    //hide controls after 2 seconds
    SetTimer(hControlsWnd, 1, 2*1000, NULL);
}

void VLCFullScreenWnd::NeedHideControls()
{
    KillTimer(hControlsWnd, 1);
    ShowWindow(hControlsWnd, SW_HIDE);
}

void VLCFullScreenWnd::SyncVideoPosScrollPosWithVideoPos()
{
    libvlc_media_player_t* p_md = getMD();
    if( p_md ){
        libvlc_time_t pos = libvlc_media_player_get_time(p_md);
        SetVideoPosScrollPosByVideoPos(pos);
    }
}

void VLCFullScreenWnd::SetVideoPosScrollRangeByVideoLen()
{
    libvlc_media_player_t* p_md = getMD();
    if( p_md ){
        libvlc_time_t MaxLen = libvlc_media_player_get_length(p_md);
        VideoPosShiftBits = 0;
        while(MaxLen>0xffff){
            MaxLen >>= 1;
            ++VideoPosShiftBits;
        };
        SendMessage(hVideoPosScroll, (UINT)PBM_SETRANGE, 0, MAKELPARAM(0, MaxLen));
    }
}

void VLCFullScreenWnd::SetVideoPosScrollPosByVideoPos(int CurScrollPos)
{
    SendMessage(hVideoPosScroll, (UINT)PBM_SETPOS, (WPARAM) (CurScrollPos >> VideoPosShiftBits), 0);
}

void VLCFullScreenWnd::SetVideoPos(float Pos) //0-start, 1-end
{
    libvlc_media_player_t* p_md = getMD();
    if( p_md ){
        libvlc_media_player_set_time(p_md, libvlc_media_player_get_length(p_md)*Pos);
        SyncVideoPosScrollPosWithVideoPos();
    }
}

void VLCFullScreenWnd::SyncVolumeSliderWithVLCVolume()
{
    libvlc_media_player_t* p_md = getMD();
    if( p_md ){
        int vol = libvlc_audio_get_volume(p_md);
        const LRESULT SliderPos = SendMessage(hVolumeSlider, (UINT) TBM_GETPOS, 0, 0);
        if(SliderPos!=vol)
            SendMessage(hVolumeSlider, (UINT) TBM_SETPOS, (WPARAM) TRUE, (LPARAM) vol);

        bool muted = libvlc_audio_get_mute(p_md);
        int MuteButtonState = SendMessage(hMuteButton, (UINT) BM_GETCHECK, 0, 0);
        if((muted&&(BST_UNCHECKED==MuteButtonState))||(!muted&&(BST_CHECKED==MuteButtonState))){
            SendMessage(hMuteButton, BM_SETCHECK, (WPARAM)(muted?BST_CHECKED:BST_UNCHECKED), 0);
        }
        LRESULT lResult = SendMessage(hMuteButton, BM_GETIMAGE, (WPARAM)IMAGE_BITMAP, 0);
        if( (muted && ((HANDLE)lResult == hVolumeBitmap)) || (!muted&&((HANDLE)lResult == hVolumeMutedBitmap)) ){
            SendMessage(hMuteButton, BM_SETIMAGE, (WPARAM)IMAGE_BITMAP, (LPARAM)(muted?hVolumeMutedBitmap:hVolumeBitmap));
        }
    }
}

void VLCFullScreenWnd::SetVLCVolumeBySliderPos(int CurPos)
{
    libvlc_media_player_t* p_md = getMD();
    if( p_md ){
        libvlc_audio_set_volume(p_md, CurPos);
        if(0==CurPos){
            libvlc_audio_set_mute(p_md, IsDlgButtonChecked(getHWND(), ID_FS_MUTE));
        }
        SyncVolumeSliderWithVLCVolume();
    }
}

void VLCFullScreenWnd::CreateToolTip()
{
    hToolTipWnd = CreateWindowEx(WS_EX_TOPMOST,
            TOOLTIPS_CLASS,
            NULL,
            WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            this->getHWND(),
            NULL,
            _hinstance,
            NULL);

    SetWindowPos(hToolTipWnd,
            HWND_TOPMOST,
            0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);


    TOOLINFO ti;
    ti.cbSize = sizeof(TOOLINFO);
    ti.uFlags = TTF_SUBCLASS|TTF_IDISHWND;
    ti.hwnd = this->getHWND();
    ti.hinst = _hinstance;

    TCHAR HintText[100];
    RECT ActivateTTRect;

    //end fullscreen button tooltip
    GetWindowRect(this->hFSButton, &ActivateTTRect);
    GetWindowText(this->hFSButton, HintText, sizeof(HintText));
    ti.uId = (UINT_PTR)this->hFSButton;
    ti.rect = ActivateTTRect;
    ti.lpszText = HintText;
    SendMessage(hToolTipWnd, TTM_ADDTOOL, 0, (LPARAM) (LPTOOLINFO) &ti);

    //play/pause button tooltip
    GetWindowRect(this->hPlayPauseButton, &ActivateTTRect);
    GetWindowText(this->hPlayPauseButton, HintText, sizeof(HintText));
    ti.uId = (UINT_PTR)this->hPlayPauseButton;
    ti.rect = ActivateTTRect;
    ti.lpszText = HintText;
    SendMessage(hToolTipWnd, TTM_ADDTOOL, 0, (LPARAM) (LPTOOLINFO) &ti);

    //mute button tooltip
    GetWindowRect(this->hMuteButton, &ActivateTTRect);
    GetWindowText(this->hMuteButton, HintText, sizeof(HintText));
    ti.uId = (UINT_PTR)this->hMuteButton;
    ti.rect = ActivateTTRect;
    ti.lpszText = HintText;
    SendMessage(hToolTipWnd, TTM_ADDTOOL, 0, (LPARAM) (LPTOOLINFO) &ti);
}

/////////////////////////////////////
//VLCFullScreenWnd event handlers
void VLCFullScreenWnd::handle_position_changed_event(const libvlc_event_t* event, void *param)
{
    VLCFullScreenWnd* fs_data = reinterpret_cast<VLCFullScreenWnd *>(param);
    if(fs_data->handle_position_changed_event_enabled)
        fs_data->SyncVideoPosScrollPosWithVideoPos();
}

void VLCFullScreenWnd::handle_input_state_event(const libvlc_event_t* event, void *param)
{
    VLCFullScreenWnd* fs_data = reinterpret_cast<VLCFullScreenWnd *>(param);
    switch( event->type )
    {
        case libvlc_MediaPlayerNothingSpecial:
            break;
        case libvlc_MediaPlayerOpening:
            break;
        case libvlc_MediaPlayerBuffering:
            break;
        case libvlc_MediaPlayerPlaying:
            SendMessage(fs_data->hPlayPauseButton, BM_SETIMAGE, (WPARAM)IMAGE_BITMAP, (LPARAM)fs_data->hPauseBitmap);
            break;
        case libvlc_MediaPlayerPaused:
            SendMessage(fs_data->hPlayPauseButton, BM_SETIMAGE, (WPARAM)IMAGE_BITMAP, (LPARAM)fs_data->hPlayBitmap);
            break;
        case libvlc_MediaPlayerStopped:
            SendMessage(fs_data->hPlayPauseButton, BM_SETIMAGE, (WPARAM)IMAGE_BITMAP, (LPARAM)fs_data->hPlayBitmap);
            break;
        case libvlc_MediaPlayerForward:
            break;
        case libvlc_MediaPlayerBackward:
            break;
        case libvlc_MediaPlayerEndReached:
            break;
        case libvlc_MediaPlayerEncounteredError:
            break;
    }
}

void VLCFullScreenWnd::RegisterEvents()
{
    libvlc_media_player_t* p_md = getMD();
    if( p_md ){
        libvlc_event_manager_t *eventManager = NULL;

        eventManager = libvlc_media_player_event_manager(p_md);
        if(eventManager) {
//            libvlc_event_attach(eventManager, libvlc_MediaPlayerNothingSpecial,
//                                VLCFullScreenWnd::handle_input_state_event, this);
//            libvlc_event_attach(eventManager, libvlc_MediaPlayerOpening,
//                                VLCFullScreenWnd::handle_input_state_event, this);
//            libvlc_event_attach(eventManager, libvlc_MediaPlayerBuffering,
//                                VLCFullScreenWnd::handle_input_state_event, this);

            libvlc_event_attach(eventManager, libvlc_MediaPlayerPlaying,
                                VLCFullScreenWnd::handle_input_state_event, this);
            libvlc_event_attach(eventManager, libvlc_MediaPlayerPaused,
                                VLCFullScreenWnd::handle_input_state_event, this);
            libvlc_event_attach(eventManager, libvlc_MediaPlayerStopped,
                                VLCFullScreenWnd::handle_input_state_event, this);
//            libvlc_event_attach(eventManager, libvlc_MediaPlayerForward,
//                                VLCFullScreenWnd::handle_input_state_event, this);
//            libvlc_event_attach(eventManager, libvlc_MediaPlayerBackward,
//                                VLCFullScreenWnd::handle_input_state_event, this);
//            libvlc_event_attach(eventManager, libvlc_MediaPlayerEndReached,
//                                VLCFullScreenWnd::handle_input_state_event, this);
//            libvlc_event_attach(eventManager, libvlc_MediaPlayerEncounteredError,
//                                VLCFullScreenWnd::handle_input_state_event, this);

//            libvlc_event_attach(eventManager, libvlc_MediaPlayerTimeChanged,
//                                VLCFullScreenWnd::handle_time_changed_event, this);
            libvlc_event_attach(eventManager, libvlc_MediaPlayerPositionChanged,
                                VLCFullScreenWnd::handle_position_changed_event, this);
//            libvlc_event_attach(eventManager, libvlc_MediaPlayerSeekableChanged,
//                                handle_seekable_changed_event, this);
//            libvlc_event_attach(eventManager, libvlc_MediaPlayerPausableChanged,
//                                handle_pausable_changed_event, this);

        }
    }
}

void VLCFullScreenWnd::UnRegisterEvents()
{
    libvlc_media_player_t* p_md = getMD();
    if( p_md ){
        libvlc_event_manager_t *eventManager = NULL;

        eventManager = libvlc_media_player_event_manager(p_md);
        if(eventManager) {
//            libvlc_event_detach(eventManager, libvlc_MediaPlayerNothingSpecial,
//                                VLCFullScreenWnd::handle_input_state_event, this);
//            libvlc_event_detach(eventManager, libvlc_MediaPlayerOpening,
//                                VLCFullScreenWnd::handle_input_state_event, this);
//            libvlc_event_detach(eventManager, libvlc_MediaPlayerBuffering,
//                                VLCFullScreenWnd::handle_input_state_event, this);

            libvlc_event_detach(eventManager, libvlc_MediaPlayerPlaying,
                                handle_input_state_event, this);
            libvlc_event_detach(eventManager, libvlc_MediaPlayerPaused,
                                handle_input_state_event, this);
            libvlc_event_detach(eventManager, libvlc_MediaPlayerStopped,
                                handle_input_state_event, this);
//            libvlc_event_detach(eventManager, libvlc_MediaPlayerForward,
//                                VLCFullScreenWnd::handle_input_state_event, this);
//            libvlc_event_detach(eventManager, libvlc_MediaPlayerBackward,
//                                VLCFullScreenWnd::handle_input_state_event, this);
//            libvlc_event_detach(eventManager, libvlc_MediaPlayerEndReached,
//                                VLCFullScreenWnd::handle_input_state_event, this);
//            libvlc_event_detach(eventManager, libvlc_MediaPlayerEncounteredError,
//                                VLCFullScreenWnd::handle_input_state_event, this);

//            libvlc_event_detach(eventManager, libvlc_MediaPlayerTimeChanged,
//                                VLCFullScreenWnd::handle_time_changed_event, this);
            libvlc_event_detach(eventManager, libvlc_MediaPlayerPositionChanged,
                                VLCFullScreenWnd::handle_position_changed_event, this);
//            libvlc_event_detach(eventManager, libvlc_MediaPlayerSeekableChanged,
//                                handle_seekable_changed_event, this);
//            libvlc_event_detach(eventManager, libvlc_MediaPlayerPausableChanged,
//                                handle_pausable_changed_event, this);
        }
    }
}

///////////////////////
//VLCWindowsManager
///////////////////////
VLCWindowsManager::VLCWindowsManager(HMODULE hModule)
    :_hModule(hModule), _hWindowedParentWnd(0), _HolderWnd(0), _FSWnd(0), _p_md(0), _b_new_messages_flag(false)
{
    VLCHolderWnd::RegisterWndClassName(hModule);
    VLCFullScreenWnd::RegisterWndClassName(hModule);
}

VLCWindowsManager::~VLCWindowsManager()
{
    VLCHolderWnd::UnRegisterWndClassName();
    VLCFullScreenWnd::UnRegisterWndClassName();
}

void VLCWindowsManager::CreateWindows(HWND hWindowedParentWnd)
{
    _hWindowedParentWnd = hWindowedParentWnd;

    if(!_HolderWnd){
        _HolderWnd = VLCHolderWnd::CreateHolderWindow(hWindowedParentWnd);
    }

    if(!_FSWnd){
        _FSWnd= VLCFullScreenWnd::CreateFSWindow(this);
    }
}

void VLCWindowsManager::DestroyWindows()
{
    if(_FSWnd){
        _FSWnd->DestroyWindow();
    }
    _FSWnd = 0;

    if(_HolderWnd){
        _HolderWnd->DestroyWindow();
    }
    _HolderWnd = 0;
}

void VLCWindowsManager::LibVlcAttach(libvlc_media_player_t* p_md)
{
    if(!_HolderWnd)
        return;//VLCWindowsManager::CreateWindows was not called

    _p_md=p_md;
    libvlc_media_player_set_hwnd(p_md, _HolderWnd->getHWND());
}

void VLCWindowsManager::LibVlcDetach()
{
    if(_p_md){
        libvlc_media_player_set_hwnd(_p_md, 0);
    }

    _p_md=0;
}

void VLCWindowsManager::StartFullScreen()
{
    if(!_HolderWnd)
        return;//VLCWindowsManager::CreateWindows was not called

    if(getMD()&&!IsFullScreen()){
        if(!_FSWnd){
            _FSWnd= VLCFullScreenWnd::CreateFSWindow(this);
        }

        SetParent(_HolderWnd->getHWND(), _FSWnd->getHWND());
        SetWindowPos(_FSWnd->getHWND(), HWND_TOPMOST, 0, 0,
                     GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN), 0/** /SWP_NOZORDER/**/);

        ShowWindow(_FSWnd->getHWND(), SW_SHOW);
        ShowWindow(_hWindowedParentWnd, SW_HIDE);
    }
}

void VLCWindowsManager::EndFullScreen()
{
    if(!_HolderWnd)
        return;//VLCWindowsManager::CreateWindows was not called

    if(IsFullScreen()){
        SetParent(_HolderWnd->getHWND(), _hWindowedParentWnd);

        RECT WindowedParentRect;
        GetClientRect(_hWindowedParentWnd, &WindowedParentRect);
        MoveWindow(_HolderWnd->getHWND(), 0, 0, WindowedParentRect.right, WindowedParentRect.bottom, FALSE);

        ShowWindow(_hWindowedParentWnd, SW_SHOW);
        ShowWindow(_FSWnd->getHWND(), SW_HIDE);

        if(_FSWnd){
            _FSWnd->DestroyWindow();
        }
        _FSWnd = 0;
   }
}

void VLCWindowsManager::ToggleFullScreen()
{
    if(IsFullScreen()){
        EndFullScreen();
    }
    else{
        StartFullScreen();
    }
}

bool VLCWindowsManager::IsFullScreen()
{
    return 0!=_FSWnd && 0!=_HolderWnd && GetParent(_HolderWnd->getHWND())==_FSWnd->getHWND();
}

#endif //_WIN32
