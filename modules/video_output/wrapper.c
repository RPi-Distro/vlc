/*****************************************************************************
 * vout_display.c: "vout display" -> "video output" wrapper
 *****************************************************************************
 * Copyright (C) 2009 Laurent Aimar
 * $Id: b8db199946138c64eecd0bff608741c098c99485 $
 *
 * Authors: Laurent Aimar <fenrir _AT_ videolan _DOT_ org>
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

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_vout_display.h>
#include <vlc_vout_wrapper.h>
#include <vlc_vout.h>
#include "../video_filter/filter_common.h"
#include <assert.h>

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open (vlc_object_t *);
static void Close(vlc_object_t *);

vlc_module_begin()
    set_category( CAT_VIDEO )
    set_subcategory( SUBCAT_VIDEO_VOUT )

    set_description( "Transitional video display wrapper" )
    set_shortname( "Video display wrapper" )
    set_capability( "video output", 210 )
    set_callbacks( Open, Close )

vlc_module_end()

/*****************************************************************************
 *
 *****************************************************************************/
struct vout_sys_t {
    char           *title;
    vout_display_t *vd;
    bool           use_dr;
};

struct picture_sys_t {
    picture_t *direct;
};

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Init   (vout_thread_t *);
static void End    (vout_thread_t *);
static int  Manage (vout_thread_t *);
static void Render (vout_thread_t *, picture_t *);
static void Display(vout_thread_t *, picture_t *);

static void VoutGetDisplayCfg(vout_thread_t *,
                              vout_display_cfg_t *, const char *title);
#ifdef WIN32
static int  Forward(vlc_object_t *, char const *,
                    vlc_value_t, vlc_value_t, void *);
#endif

/*****************************************************************************
 *
 *****************************************************************************/
static int Open(vlc_object_t *object)
{
    vout_thread_t *vout = (vout_thread_t *)object;
    vout_sys_t *sys;

    msg_Dbg(vout, "Opening vout display wrapper");

    /* */
    sys = malloc(sizeof(*sys));
    if (!sys)
        return VLC_ENOMEM;

    sys->title = var_CreateGetNonEmptyString(vout, "video-title");

    /* */
    video_format_t source   = vout->fmt_render;
    source.i_visible_width  = source.i_width;
    source.i_visible_height = source.i_height;
    source.i_x_offset       = 0;
    source.i_y_offset       = 0;

    vout_display_state_t state;
    VoutGetDisplayCfg(vout, &state.cfg, sys->title);
    state.is_on_top = var_CreateGetBool(vout, "video-on-top");
    state.sar.num = 0;
    state.sar.den = 0;

    const mtime_t double_click_timeout = 300000;
    const mtime_t hide_timeout = var_CreateGetInteger(vout, "mouse-hide-timeout") * 1000;

    sys->vd = vout_NewDisplay(vout, &source, &state, "$vout",
                              double_click_timeout, hide_timeout);
    if (!sys->vd) {
        free(sys->title);
        free(sys);
        return VLC_EGENERIC;
    }

    /* */
#ifdef WIN32
    var_Create(vout, "direct3d-desktop", VLC_VAR_BOOL|VLC_VAR_DOINHERIT);
    var_AddCallback(vout, "direct3d-desktop", Forward, NULL);
    var_Create(vout, "video-wallpaper", VLC_VAR_BOOL|VLC_VAR_DOINHERIT);
    var_AddCallback(vout, "video-wallpaper", Forward, NULL);
#endif

    /* */
    vout->pf_init    = Init;
    vout->pf_end     = End;
    vout->pf_manage  = Manage;
    vout->pf_render  = Render;
    vout->pf_display = Display;
    vout->pf_control = NULL;
    vout->p_sys      = sys;

    return VLC_SUCCESS;
}

/*****************************************************************************
 *
 *****************************************************************************/
static void Close(vlc_object_t *object)
{
    vout_thread_t *vout = (vout_thread_t *)object;
    vout_sys_t *sys = vout->p_sys;

#ifdef WIN32
    var_DelCallback(vout, "direct3d-desktop", Forward, NULL);
    var_DelCallback(vout, "video-wallpaper", Forward, NULL);
#endif
    vout_DeleteDisplay(sys->vd, NULL);
    free(sys->title);
    free(sys );
}

/*****************************************************************************
 *
 *****************************************************************************/
static int Init(vout_thread_t *vout)
{
    vout_sys_t *sys = vout->p_sys;
    vout_display_t *vd = sys->vd;

    /* */
    video_format_t source = vd->source;

    vout->output.i_chroma = source.i_chroma;
    vout->output.i_width  = source.i_width;
    vout->output.i_height = source.i_height;
    vout->output.i_aspect = (int64_t)source.i_sar_num * source.i_width * VOUT_ASPECT_FACTOR / source.i_sar_den / source.i_height;
    vout->output.i_rmask  = source.i_rmask;
    vout->output.i_gmask  = source.i_gmask;
    vout->output.i_bmask  = source.i_bmask;
    vout->output.pf_setpalette = NULL; /* FIXME What to do ? Seems unused anyway */

    /* also set fmt_out (completly broken API) */
    vout->fmt_out.i_chroma         = vout->output.i_chroma;
    vout->fmt_out.i_width          =
    vout->fmt_out.i_visible_width  = vout->output.i_width;
    vout->fmt_out.i_height         =
    vout->fmt_out.i_visible_height = vout->output.i_height;
    vout->fmt_out.i_sar_num        = vout->output.i_aspect * vout->output.i_height;
    vout->fmt_out.i_sar_den        = VOUT_ASPECT_FACTOR    * vout->output.i_width;
    vout->fmt_out.i_x_offset       = 0;
    vout->fmt_out.i_y_offset       = 0;

    if (vout->fmt_in.i_visible_width  != source.i_visible_width ||
        vout->fmt_in.i_visible_height != source.i_visible_height ||
        vout->fmt_in.i_x_offset       != source.i_x_offset ||
        vout->fmt_in.i_y_offset       != source.i_y_offset )
        vout->i_changes |= VOUT_CROP_CHANGE;

    if (vout->b_on_top)
        vout_SetWindowState(vd, VOUT_WINDOW_STATE_ABOVE);

    /* XXX For non dr case, the current vout implementation force us to
     * create at most 1 direct picture (otherwise the buffers will be kept
     * referenced even through the Init/End.
     */
    sys->use_dr = !vout_IsDisplayFiltered(vd);
    const bool allow_dr = !vd->info.has_pictures_invalid && sys->use_dr;
    const int picture_max = allow_dr ? VOUT_MAX_PICTURES : 1;
    for (vout->output.i_pictures = 0;
            vout->output.i_pictures < picture_max;
                vout->output.i_pictures++) {
        /* Find an empty picture slot */
        picture_t *picture = NULL;
        for (int index = 0; index < VOUT_MAX_PICTURES; index++) {
            if (vout->p_picture[index].i_status == FREE_PICTURE) {
                picture = &vout->p_picture[index];
                break;
            }
        }
        if (!picture)
            break;
        memset(picture, 0, sizeof(*picture));

        picture->p_sys = malloc(sizeof(*picture->p_sys));

        if (sys->use_dr) {
            picture_pool_t *pool = vout_display_Pool(vd, picture_max);
            if (!pool)
                break;
            picture_t *direct = picture_pool_Get(pool);
            if (!direct)
                break;
            picture->format = direct->format;
            picture->i_planes = direct->i_planes;
            for (int i = 0; i < direct->i_planes; i++)
                picture->p[i] = direct->p[i];
            picture->b_slow = vd->info.is_slow;

            picture->p_sys->direct = direct;
        } else {
            vout_AllocatePicture(VLC_OBJECT(vd), picture,
                                 vd->source.i_chroma,
                                 vd->source.i_width, vd->source.i_height,
                                 vd->source.i_sar_num, vd->source.i_sar_den);
            if (!picture->i_planes)
                break;
            picture->p_sys->direct = NULL;
        }
        picture->i_status = DESTROYED_PICTURE;
        picture->i_type    = DIRECT_PICTURE;

        vout->output.pp_picture[vout->output.i_pictures] = picture;
    }
    return VLC_SUCCESS;
}

/*****************************************************************************
 *
 *****************************************************************************/
static void End(vout_thread_t *vout)
{
    vout_sys_t *sys = vout->p_sys;

    for (int i = 0; i < VOUT_MAX_PICTURES; i++) {
        picture_t *picture = &vout->p_picture[i];

        if (picture->i_type != DIRECT_PICTURE)
            continue;

        if (picture->p_sys->direct)
            picture_Release(picture->p_sys->direct);
        if (!sys->use_dr)
            free(picture->p_data_orig);
        free(picture->p_sys);

        picture->i_status = FREE_PICTURE;
    }
    if (sys->use_dr && vout_AreDisplayPicturesInvalid(sys->vd))
        vout_ManageDisplay(sys->vd, true);
}

/*****************************************************************************
 *
 *****************************************************************************/
static int Manage(vout_thread_t *vout)
{
    vout_sys_t *sys = vout->p_sys;
    vout_display_t *vd = sys->vd;

    while (vout->i_changes & (VOUT_FULLSCREEN_CHANGE |
                              VOUT_ASPECT_CHANGE |
                              VOUT_ZOOM_CHANGE |
                              VOUT_SCALE_CHANGE |
                              VOUT_ON_TOP_CHANGE |
                              VOUT_CROP_CHANGE)) {
        /* */
        if (vout->i_changes & VOUT_FULLSCREEN_CHANGE) {
            vout->b_fullscreen = !vout->b_fullscreen;

            var_SetBool(vout, "fullscreen", vout->b_fullscreen);
            vout_SetDisplayFullscreen(vd, vout->b_fullscreen);
            vout->i_changes &= ~VOUT_FULLSCREEN_CHANGE;
        }
        if (vout->i_changes & VOUT_ASPECT_CHANGE) {
            vout->output.i_aspect   = (int64_t)vout->fmt_in.i_sar_num * vout->fmt_in.i_width * VOUT_ASPECT_FACTOR /
                                      vout->fmt_in.i_sar_den / vout->fmt_in.i_height;
            vout->fmt_out.i_sar_num = vout->fmt_in.i_sar_num;
            vout->fmt_out.i_sar_den = vout->fmt_in.i_sar_den;

            vout_SetDisplayAspect(vd, vout->fmt_in.i_sar_num, vout->fmt_in.i_sar_den);

            vout->i_changes &= ~VOUT_ASPECT_CHANGE;
        }
        if (vout->i_changes & VOUT_ZOOM_CHANGE) {
            const float zoom = var_GetFloat(vout, "scale");

            unsigned den = ZOOM_FP_FACTOR;
            unsigned num = den * zoom;
            if (num < (ZOOM_FP_FACTOR+9) / 10)
                num = (ZOOM_FP_FACTOR+9) / 10;
            else if (num > ZOOM_FP_FACTOR * 10)
                num = ZOOM_FP_FACTOR * 10;

            vout_SetDisplayZoom(vd, num, den);

            vout->i_changes &= ~VOUT_ZOOM_CHANGE;
        }
        if (vout->i_changes & VOUT_SCALE_CHANGE) {
            const bool is_display_filled = var_GetBool(vout, "autoscale");

            vout_SetDisplayFilled(vd, is_display_filled);

            vout->i_changes &= ~VOUT_SCALE_CHANGE;
        }
        if (vout->i_changes & VOUT_ON_TOP_CHANGE) {
            vout_SetWindowState(vd, vout->b_on_top
                ? VOUT_WINDOW_STATE_ABOVE
                : VOUT_WINDOW_STATE_NORMAL);

            vout->i_changes &= ~VOUT_ON_TOP_CHANGE;
        }
        if (vout->i_changes & VOUT_CROP_CHANGE) {
            const video_format_t crop = vout->fmt_in;
            const video_format_t org = vout->fmt_render;
            /* FIXME because of rounding errors, the reconstructed ratio is wrong */
            unsigned num = 0;
            unsigned den = 0;
            if (crop.i_x_offset == org.i_x_offset &&
                crop.i_visible_width == org.i_visible_width &&
                crop.i_y_offset == org.i_y_offset + (org.i_visible_height - crop.i_visible_height)/2) {
                vlc_ureduce(&num, &den,
                            crop.i_visible_width * crop.i_sar_num,
                            crop.i_visible_height * crop.i_sar_den, 0);
            } else if (crop.i_y_offset == org.i_y_offset &&
                       crop.i_visible_height == org.i_visible_height &&
                       crop.i_x_offset == org.i_x_offset + (org.i_visible_width - crop.i_visible_width)/2) {
                vlc_ureduce(&num, &den,
                            crop.i_visible_width * crop.i_sar_num,
                            crop.i_visible_height * crop.i_sar_den, 0);
            }
            vout_SetDisplayCrop(vd, num, den,
                                crop.i_x_offset, crop.i_y_offset,
                                crop.i_visible_width, crop.i_visible_height);
            vout->i_changes &= ~VOUT_CROP_CHANGE;
        }

    }

    if (sys->use_dr && vout_AreDisplayPicturesInvalid(vd)) {
        vout->i_changes |= VOUT_PICTURE_BUFFERS_CHANGE;
    }
    vout_ManageDisplay(vd, !sys->use_dr);
    return VLC_SUCCESS;
}

/*****************************************************************************
 * Render
 *****************************************************************************/
static void Render(vout_thread_t *vout, picture_t *picture)
{
    vout_sys_t *sys = vout->p_sys;
    vout_display_t *vd = sys->vd;

    assert(sys->use_dr || !picture->p_sys->direct);
    assert(vout_IsDisplayFiltered(vd) == !sys->use_dr);

    if (sys->use_dr) {
        assert(picture->p_sys->direct);
        vout_display_Prepare(vd, picture->p_sys->direct);
    } else {
        picture_t *direct = picture->p_sys->direct = vout_FilterDisplay(vd, picture);
        if (direct) {
            vout_display_Prepare(vd, direct);
        }
    }
}

/*****************************************************************************
 *
 *****************************************************************************/
static void Display(vout_thread_t *vout, picture_t *picture)
{
    vout_sys_t *sys = vout->p_sys;
    vout_display_t *vd = sys->vd;

    picture_t *direct = picture->p_sys->direct;
    if (!direct)
        return;

    /* XXX This is a hack that will work with current vout_display_t modules */
    if (sys->use_dr)
        picture_Hold(direct);

     vout_display_Display(vd, direct);

     if (sys->use_dr) {
         for (int i = 0; i < picture->i_planes; i++) {
             picture->p[i].p_pixels = direct->p[i].p_pixels;
             picture->p[i].i_pitch  = direct->p[i].i_pitch;
             picture->p[i].i_lines  = direct->p[i].i_lines;
         }
     } else {
         picture->p_sys->direct = NULL;
     }
}

static void VoutGetDisplayCfg(vout_thread_t *vout, vout_display_cfg_t *cfg, const char *title)
{
    /* Load configuration */
    cfg->is_fullscreen = var_CreateGetBool(vout, "fullscreen");
    cfg->display.title = title;
    const int display_width = var_CreateGetInteger(vout, "width");
    const int display_height = var_CreateGetInteger(vout, "height");
    cfg->display.width   = display_width > 0  ? display_width  : 0;
    cfg->display.height  = display_height > 0 ? display_height : 0;
    cfg->is_display_filled  = var_CreateGetBool(vout, "autoscale");
    cfg->display.sar.num = 1; /* TODO monitor AR */
    cfg->display.sar.den = 1;
    unsigned zoom_den = 1000;
    unsigned zoom_num = zoom_den * var_CreateGetFloat(vout, "scale");
    vlc_ureduce(&zoom_num, &zoom_den, zoom_num, zoom_den, 0);
    cfg->zoom.num = zoom_num;
    cfg->zoom.den = zoom_den;
    cfg->align.vertical = VOUT_DISPLAY_ALIGN_CENTER;
    cfg->align.horizontal = VOUT_DISPLAY_ALIGN_CENTER;
    const int align_mask = var_CreateGetInteger(vout, "align");
    if (align_mask & 0x1)
        cfg->align.horizontal = VOUT_DISPLAY_ALIGN_LEFT;
    else if (align_mask & 0x2)
        cfg->align.horizontal = VOUT_DISPLAY_ALIGN_RIGHT;
    if (align_mask & 0x4)
        cfg->align.vertical = VOUT_DISPLAY_ALIGN_TOP;
    else if (align_mask & 0x8)
        cfg->align.vertical = VOUT_DISPLAY_ALIGN_BOTTOM;
}

#ifdef WIN32
static int Forward(vlc_object_t *object, char const *var,
                   vlc_value_t oldval, vlc_value_t newval, void *data)
{
    vout_thread_t *vout = (vout_thread_t*)object;

    VLC_UNUSED(oldval);
    VLC_UNUSED(data);
    return var_Set(vout->p_sys->vd, var, newval);
}
#endif
