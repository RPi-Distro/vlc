/*****************************************************************************
 * wingdi.c : Win32 / WinCE GDI video output plugin for vlc
 *****************************************************************************
 * Copyright (C) 2002-2009 the VideoLAN team
 * $Id: 619e664fddec95255058c757c941b093fa3ed7bc $
 *
 * Authors: Gildas Bazin <gbazin@videolan.org>
 *          Samuel Hocevar <sam@zoy.org>
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include <assert.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_playlist.h>
#include <vlc_vout_display.h>

#include <windows.h>
#include <commctrl.h>

#include "common.h"

#ifndef WS_NONAVDONEBUTTON
#   define WS_NONAVDONEBUTTON 0
#endif

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open (vlc_object_t *);
static void Close(vlc_object_t *);

vlc_module_begin ()
    set_category(CAT_VIDEO)
    set_subcategory(SUBCAT_VIDEO_VOUT)
#ifdef MODULE_NAME_IS_wingapi
    set_shortname("GAPI")
    set_description(N_("Windows GAPI video output"))
    set_capability("vout display", 20)
#else
    set_shortname("GDI")
    set_description(N_("Windows GDI video output"))
    set_capability("vout display", 10)
#endif
    set_callbacks(Open, Close)
vlc_module_end ()


/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static picture_pool_t *Pool  (vout_display_t *, unsigned);
static void           Display(vout_display_t *, picture_t *);
static int            Control(vout_display_t *, int, va_list);
static void           Manage (vout_display_t *);

static int            Init(vout_display_t *, video_format_t *, int, int);
static void           Clean(vout_display_t *);

/* */
static int Open(vlc_object_t *object)
{
    vout_display_t *vd = (vout_display_t *)object;
    vout_display_sys_t *sys;

    vd->sys = sys = calloc(1, sizeof(*sys));
    if (!sys)
        return VLC_ENOMEM;

#ifdef MODULE_NAME_IS_wingapi
    /* Load GAPI */
    sys->gapi_dll = LoadLibrary(_T("GX.DLL"));
    if (!sys->gapi_dll) {
        msg_Warn(vd, "failed loading gx.dll");
        free(sys);
        return VLC_EGENERIC;
    }

    GXOpenDisplay = (void *)GetProcAddress(sys->gapi_dll,
        _T("?GXOpenDisplay@@YAHPAUHWND__@@K@Z"));
    GXCloseDisplay = (void *)GetProcAddress(sys->gapi_dll,
        _T("?GXCloseDisplay@@YAHXZ"));
    GXBeginDraw = (void *)GetProcAddress(sys->gapi_dll,
        _T("?GXBeginDraw@@YAPAXXZ"));
    GXEndDraw = (void *)GetProcAddress(sys->gapi_dll,
        _T("?GXEndDraw@@YAHXZ"));
    GXGetDisplayProperties = (void *)GetProcAddress(sys->gapi_dll,
        _T("?GXGetDisplayProperties@@YA?AUGXDisplayProperties@@XZ"));
    GXSuspend = (void *)GetProcAddress(sys->gapi_dll,
        _T("?GXSuspend@@YAHXZ"));
    GXResume = GetProcAddress(sys->gapi_dll,
        _T("?GXResume@@YAHXZ"));

    if (!GXOpenDisplay || !GXCloseDisplay ||
        !GXBeginDraw || !GXEndDraw ||
        !GXGetDisplayProperties || !GXSuspend || !GXResume) {
        msg_Err(vd, "failed GetProcAddress on gapi.dll");
        free(sys);
        return VLC_EGENERIC;
    }

    msg_Dbg(vd, "GAPI DLL loaded");
#endif

    if (CommonInit(vd))
        goto error;

    /* */
    video_format_t fmt = vd->fmt;
    if (Init(vd, &fmt, fmt.i_width, fmt.i_height))
        goto error;

    vout_display_info_t info = vd->info;
    info.is_slow              = false;
    info.has_double_click     = true;
    info.has_hide_mouse       = false;
    info.has_pictures_invalid = true;

    /* */
    vd->fmt  = fmt;
    vd->info = info;

    vd->pool    = Pool;
    vd->prepare = NULL;
    vd->display = Display;
    vd->manage  = Manage;
    vd->control = Control;
    return VLC_SUCCESS;

error:
    Close(VLC_OBJECT(vd));
    return VLC_EGENERIC;
}

/* */
static void Close(vlc_object_t *object)
{
    vout_display_t *vd = (vout_display_t *)object;

    Clean(vd);

    CommonClean(vd);

#ifdef MODULE_NAME_IS_wingapi
    FreeLibrary(vd->sys->gapi_dll);
#endif

    free(vd->sys);
}

/* */
static picture_pool_t *Pool(vout_display_t *vd, unsigned count)
{
    VLC_UNUSED(count);
    return vd->sys->pool;
}
static void Display(vout_display_t *vd, picture_t *picture)
{
    vout_display_sys_t *sys = vd->sys;

#ifdef MODULE_NAME_IS_wingapi
    /* */
#else
#define rect_src vd->sys->rect_src
#define rect_src_clipped vd->sys->rect_src_clipped
#define rect_dest vd->sys->rect_dest
#define rect_dest_clipped vd->sys->rect_dest_clipped
    RECT rect_dst = rect_dest_clipped;
    HDC hdc = GetDC(sys->hvideownd);

    OffsetRect(&rect_dst, -rect_dest.left, -rect_dest.top);
    SelectObject(sys->off_dc, sys->off_bitmap);

    if (rect_dest_clipped.right - rect_dest_clipped.left !=
        rect_src_clipped.right - rect_src_clipped.left ||
        rect_dest_clipped.bottom - rect_dest_clipped.top !=
        rect_src_clipped.bottom - rect_src_clipped.top) {
        StretchBlt(hdc, rect_dst.left, rect_dst.top,
                   rect_dst.right, rect_dst.bottom,
                   sys->off_dc,
                   rect_src_clipped.left,  rect_src_clipped.top,
                   rect_src_clipped.right, rect_src_clipped.bottom,
                   SRCCOPY);
    } else {
        BitBlt(hdc, rect_dst.left, rect_dst.top,
               rect_dst.right, rect_dst.bottom,
               sys->off_dc,
               rect_src_clipped.left, rect_src_clipped.top,
               SRCCOPY);
    }

    ReleaseDC(sys->hvideownd, hdc);
#undef rect_src
#undef rect_src_clipped
#undef rect_dest
#undef rect_dest_clipped
#endif
    /* TODO */
    picture_Release(picture);

    CommonDisplay(vd);
}
static int Control(vout_display_t *vd, int query, va_list args)
{
    switch (query) {
    case VOUT_DISPLAY_RESET_PICTURES:
        assert(0);
        return VLC_EGENERIC;
    default:
        return CommonControl(vd, query, args);
    }

}
static void Manage(vout_display_t *vd)
{
    CommonManage(vd);
}

#ifdef MODULE_NAME_IS_wingapi
struct picture_sys_t {
    vout_display_t *vd;
};

static int Lock(picture_t *picture)
{
    vout_display_t *vd = picture->p_sys->vd;
    vout_display_sys_t *sys = vd->sys;

    /* */
    if (sys->rect_dest_clipped.right  - sys->rect_dest_clipped.left != vd->fmt.i_width ||
        sys->rect_dest_clipped.bottom - sys->rect_dest_clipped.top  != vd->fmt.i_height)
        return VLC_EGENERIC;

    /* */
    GXDisplayProperties gxdisplayprop = GXGetDisplayProperties();
    uint8_t *p_pic_buffer = GXBeginDraw();
    if (!p_pic_buffer) {
        msg_Err(vd, "GXBeginDraw error %d ", GetLastError());
        return VLC_EGENERIC;
    }
    p_pic_buffer += sys->rect_dest.top  * gxdisplayprop.cbyPitch +
                    sys->rect_dest.left * gxdisplayprop.cbxPitch;

    /* */
    picture->p[0].i_pitch  = gxdisplayprop.cbyPitch;
    picture->p[0].p_pixels = p_pic_buffer;

    return VLC_SUCCESS;
}
static void Unlock(picture_t *picture)
{
    vout_display_t *vd = picture->p_sys->vd;

    GXEndDraw();
}
#endif

static int Init(vout_display_t *vd,
                video_format_t *fmt, int width, int height)
{
    vout_display_sys_t *sys = vd->sys;

    /* */
    RECT *display = &sys->rect_display;
    display->left   = 0;
    display->top    = 0;
#ifdef MODULE_NAME_IS_wingapi
    display->right  = GXGetDisplayProperties().cxWidth;
    display->bottom = GXGetDisplayProperties().cyHeight;
#else
    display->right  = GetSystemMetrics(SM_CXSCREEN);;
    display->bottom = GetSystemMetrics(SM_CYSCREEN);;
#endif

    /* Initialize an offscreen bitmap for direct buffer operations. */

    /* */
    HDC window_dc = GetDC(sys->hvideownd);

    /* */
#ifdef MODULE_NAME_IS_wingapi
    GXDisplayProperties gx_displayprop = GXGetDisplayProperties();
    sys->i_depth = gx_displayprop.cBPP;
#else

    sys->i_depth = GetDeviceCaps(window_dc, PLANES) *
                   GetDeviceCaps(window_dc, BITSPIXEL);
#endif

    /* */
    msg_Dbg(vd, "GDI depth is %i", sys->i_depth);
    switch (sys->i_depth) {
    case 8:
        fmt->i_chroma = VLC_CODEC_RGB8;
        break;
    case 15:
        fmt->i_chroma = VLC_CODEC_RGB15;
        fmt->i_rmask  = 0x7c00;
        fmt->i_gmask  = 0x03e0;
        fmt->i_bmask  = 0x001f;
        break;
    case 16:
        fmt->i_chroma = VLC_CODEC_RGB16;
        fmt->i_rmask  = 0xf800;
        fmt->i_gmask  = 0x07e0;
        fmt->i_bmask  = 0x001f;
        break;
    case 24:
        fmt->i_chroma = VLC_CODEC_RGB24;
        fmt->i_rmask  = 0x00ff0000;
        fmt->i_gmask  = 0x0000ff00;
        fmt->i_bmask  = 0x000000ff;
        break;
    case 32:
        fmt->i_chroma = VLC_CODEC_RGB32;
        fmt->i_rmask  = 0x00ff0000;
        fmt->i_gmask  = 0x0000ff00;
        fmt->i_bmask  = 0x000000ff;
        break;
    default:
        msg_Err(vd, "screen depth %i not supported", sys->i_depth);
        return VLC_EGENERIC;
    }
    fmt->i_width  = width;
    fmt->i_height = height;

    uint8_t *p_pic_buffer;
    int     i_pic_pitch;
#ifdef MODULE_NAME_IS_wingapi
    GXOpenDisplay(sys->hvideownd, GX_FULLSCREEN);
    EventThreadUpdateTitle(sys->event, VOUT_TITLE " (WinGAPI output)");

    /* Filled by pool::lock() */
    p_pic_buffer = NULL;
    i_pic_pitch  = 0;
#else
    /* Initialize offscreen bitmap */
    BITMAPINFO *bi = &sys->bitmapinfo;
    memset(bi, 0, sizeof(BITMAPINFO) + 3 * sizeof(RGBQUAD));
    if (sys->i_depth > 8) {
        ((DWORD*)bi->bmiColors)[0] = fmt->i_rmask;
        ((DWORD*)bi->bmiColors)[1] = fmt->i_gmask;
        ((DWORD*)bi->bmiColors)[2] = fmt->i_bmask;;
    }

    BITMAPINFOHEADER *bih = &sys->bitmapinfo.bmiHeader;
    bih->biSize = sizeof(BITMAPINFOHEADER);
    bih->biSizeImage     = 0;
    bih->biPlanes        = 1;
    bih->biCompression   = (sys->i_depth == 15 ||
                            sys->i_depth == 16) ? BI_BITFIELDS : BI_RGB;
    bih->biBitCount      = sys->i_depth;
    bih->biWidth         = fmt->i_width;
    bih->biHeight        = -fmt->i_height;
    bih->biClrImportant  = 0;
    bih->biClrUsed       = 0;
    bih->biXPelsPerMeter = 0;
    bih->biYPelsPerMeter = 0;

    i_pic_pitch = bih->biBitCount * bih->biWidth / 8;
    sys->off_bitmap = CreateDIBSection(window_dc,
                                       (BITMAPINFO *)bih,
                                       DIB_RGB_COLORS,
                                       &p_pic_buffer, NULL, 0);

    sys->off_dc = CreateCompatibleDC(window_dc);

    SelectObject(sys->off_dc, sys->off_bitmap);
    ReleaseDC(sys->hvideownd, window_dc);

    EventThreadUpdateTitle(sys->event, VOUT_TITLE " (WinGDI output)");
#endif

    /* */
    picture_resource_t rsc;
    memset(&rsc, 0, sizeof(rsc));
#ifdef MODULE_NAME_IS_wingapi
    rsc.p_sys = malloc(sizeof(*rsc.p_sys));
    if (!rsc.p_sys)
        return VLC_EGENERIC;
    rsc.p_sys->vd = vd;
#endif
    rsc.p[0].p_pixels = p_pic_buffer;
    rsc.p[0].i_lines  = fmt->i_height;
    rsc.p[0].i_pitch  = i_pic_pitch;;

    picture_t *picture = picture_NewFromResource(fmt, &rsc);
    if (picture) {
        picture_pool_configuration_t cfg;
        memset(&cfg, 0, sizeof(cfg));
        cfg.picture_count = 1;
        cfg.picture = &picture;
#ifdef MODULE_NAME_IS_wingapi
        cfg.lock    = Lock;
        cfg.unlock  = Unlock;
#endif
        sys->pool = picture_pool_NewExtended(&cfg);
    } else {
        free(rsc.p_sys);
        sys->pool = NULL;
    }

    UpdateRects(vd, NULL, NULL, true);

    return VLC_SUCCESS;
}

static void Clean(vout_display_t *vd)
{
    vout_display_sys_t *sys = vd->sys;

    if (sys->pool)
        picture_pool_Delete(sys->pool);
    sys->pool = NULL;

#ifdef MODULE_NAME_IS_wingapi
    GXCloseDisplay();
#else
    if (sys->off_dc)
        DeleteDC(sys->off_dc);
    if (sys->off_bitmap)
        DeleteObject(sys->off_bitmap);
#endif
}

