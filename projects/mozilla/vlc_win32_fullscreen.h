/*****************************************************************************
 * vlc_win32_fullscreen.h: a VLC plugin for Mozilla
 *****************************************************************************
 * Copyright (C) 2002-2011 the VideoLAN team
 * $Id: ed63daf4ff961e5b3b21ff536c8b63e376775402 $
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

#ifndef VLC_FULLSCREEN_H
#define VLC_FULLSCREEN_H

#ifdef _WIN32

///////////////////////
//VLCHolderWnd
///////////////////////
class VLCHolderWnd
{
public:
    static void RegisterWndClassName(HINSTANCE hInstance);
    static void UnRegisterWndClassName();
    static VLCHolderWnd* CreateHolderWindow(HWND hParentWnd);
    void DestroyWindow()
        {::DestroyWindow(_hWnd);};

private:
    static LPCTSTR getClassName(void)  { return TEXT("VLC ActiveX Window Holder Class"); };
    static LRESULT CALLBACK VLCHolderClassWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

private:
    static HINSTANCE _hinstance;
    static ATOM _holder_wndclass_atom;

private:
    VLCHolderWnd(HWND hWnd)
        :_hWnd(hWnd){};

public:
    HWND getHWND() const {return _hWnd;}

private:
    HWND _hWnd;
};

///////////////////////
//VLCFullScreenWnd
///////////////////////
class VLCWindowsManager;
class VLCFullScreenWnd
{
public:
    static void RegisterWndClassName(HINSTANCE hInstance);
    static void UnRegisterWndClassName();
    static VLCFullScreenWnd* CreateFSWindow(VLCWindowsManager* WM);
    void DestroyWindow()
        {::DestroyWindow(_hWnd);};

private:
    static LPCTSTR getClassName(void) { return TEXT("VLC ActiveX Fullscreen Class"); };
    static LRESULT CALLBACK FSWndWindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

    static LPCTSTR getControlsClassName(void) { return TEXT("VLC ActiveX Fullscreen Controls Class"); };
    static LRESULT CALLBACK FSControlsWndWindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

private:
    static HINSTANCE _hinstance;
    static ATOM _fullscreen_wndclass_atom;
    static ATOM _fullscreen_controls_wndclass_atom;

private:
    VLCFullScreenWnd(HWND hWnd, VLCWindowsManager* WM)
        :_hWnd(hWnd), _WindowsManager(WM),
         hControlsWnd(0), hToolTipWnd(0), hNewMessageBitmap(0), hFSButtonBitmap(0), hFSButton(0),
         hPauseBitmap(0), hPlayBitmap(0), hPlayPauseButton(0), hVideoPosScroll(0),
         hMuteButton(0), hVolumeBitmap(0), hVolumeMutedBitmap(0),
         hVolumeSlider(0), Last_WM_MOUSEMOVE_lParam(0){};

    ~VLCFullScreenWnd();

private:
    libvlc_media_player_t* getMD();

    bool IsPlaying()
    {
        libvlc_media_player_t* p_md = getMD();
        int is_playing = 0;
        if( p_md ){
            is_playing = libvlc_media_player_is_playing(p_md);
        }
        return is_playing!=0;
    }

private:
    void RegisterEvents();
    void UnRegisterEvents();

    void SetVideoPosScrollRangeByVideoLen();
    void SyncVideoPosScrollPosWithVideoPos();
    void SetVideoPos(float Pos); //0-start, 1-end

    void SyncVolumeSliderWithVLCVolume();
    void SetVLCVolumeBySliderPos(int CurScrollPos);

    void NeedShowControls();
    void NeedHideControls();

private:
    void CreateToolTip();

private:
    LPARAM Last_WM_MOUSEMOVE_lParam;

private:
    VLCWindowsManager* _WindowsManager;

private:
    HWND hControlsWnd;
    HWND hToolTipWnd;

    HICON hNewMessageBitmap;
    HANDLE hFSButtonBitmap;
    HWND hFSButton;

    HANDLE hPauseBitmap;
    HANDLE hPlayBitmap;
    HWND hPlayPauseButton;

    HWND hVideoPosScroll;

    HANDLE hVolumeBitmap;
    HANDLE hVolumeMutedBitmap;
    HWND hMuteButton;
    HWND hVolumeSlider;

private:
    void SetVideoPosScrollPosByVideoPos(int CurPos);
    static bool handle_position_changed_event_enabled;
    static void handle_position_changed_event(const libvlc_event_t* event, void *param);
    //static void handle_time_changed_event(const libvlc_event_t* event, void *param);
    static void handle_input_state_event(const libvlc_event_t* event, void *param);

public:
    HWND getHWND() const {return _hWnd;}

private:
    HWND _hWnd;

    int VideoPosShiftBits;
};

///////////////////////
//VLCWindowsManager
///////////////////////
class VLCWindowsManager
{
public:
    VLCWindowsManager(HMODULE hModule);
    ~VLCWindowsManager();

    void CreateWindows(HWND hWindowedParentWnd);
    void DestroyWindows();

    void LibVlcAttach(libvlc_media_player_t* p_md);
    void LibVlcDetach();

    void StartFullScreen();
    void EndFullScreen();
    void ToggleFullScreen();
    bool IsFullScreen();

    HMODULE getHModule() const {return _hModule;};
    VLCHolderWnd* getHolderWnd() const {return _HolderWnd;}
    VLCFullScreenWnd* getFullScreenWnd() const {return _FSWnd;}
    libvlc_media_player_t* getMD() {return _p_md;}

public:
    void setNewMessageFlag(bool Yes)
        {_b_new_messages_flag = Yes;};
    bool getNewMessageFlag() const
        {return _b_new_messages_flag;};

private:
    HMODULE _hModule;
    HWND _hWindowedParentWnd;

    libvlc_media_player_t* _p_md;

    VLCHolderWnd* _HolderWnd;
    VLCFullScreenWnd* _FSWnd;

    bool _b_new_messages_flag;
};

#endif //_WIN32

#endif //VLC_FULLSCREEN_H
