/**
 * @file xvideo.c
 * @brief X C Bindings video output module for VLC media player
 */
/*****************************************************************************
 * Copyright © 2009 Rémi Denis-Courmont
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 ****************************************************************************/

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdlib.h>
#include <assert.h>

#include <xcb/xcb.h>
#include <xcb/shm.h>
#include <xcb/xv.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_vout_display.h>
#include <vlc_picture_pool.h>
#include <vlc_dialog.h>

#include "xcb_vlc.h"

#define ADAPTOR_TEXT N_("XVideo adaptor number")
#define ADAPTOR_LONGTEXT N_( \
    "XVideo hardware adaptor to use. By default, VLC will " \
    "use the first functional adaptor.")

#define SHM_TEXT N_("Use shared memory")
#define SHM_LONGTEXT N_( \
    "Use shared memory to communicate between VLC and the X server.")

static int  Open (vlc_object_t *);
static void Close (vlc_object_t *);

/*
 * Module descriptor
 */
vlc_module_begin ()
    set_shortname (N_("XVideo"))
    set_description (N_("XVideo output (XCB)"))
    set_category (CAT_VIDEO)
    set_subcategory (SUBCAT_VIDEO_VOUT)
    set_capability ("vout display", 155)
    set_callbacks (Open, Close)

    add_integer ("xvideo-adaptor", -1, NULL,
                 ADAPTOR_TEXT, ADAPTOR_LONGTEXT, true)
    add_bool ("x11-shm", true, NULL, SHM_TEXT, SHM_LONGTEXT, true)
        add_deprecated_alias ("xvideo-shm")
    add_shortcut ("xcb-xv")
    add_shortcut ("xv")
    add_shortcut ("xvideo")
vlc_module_end ()

#define MAX_PICTURES (VOUT_MAX_PICTURES)

struct vout_display_sys_t
{
    xcb_connection_t *conn;
    vout_window_t *embed;/* VLC window */

    xcb_cursor_t cursor; /* blank cursor */
    xcb_window_t window; /* drawable X window */
    xcb_gcontext_t gc;   /* context to put images */
    xcb_xv_port_t port;  /* XVideo port */
    uint32_t id;         /* XVideo format */
    uint16_t width;      /* display width */
    uint16_t height;     /* display height */
    uint32_t data_size;  /* picture byte size (for non-SHM) */
    bool     swap_uv;    /* U/V pointer must be swapped in a picture */
    bool shm;            /* whether to use MIT-SHM */
    bool visible;        /* whether it makes sense to draw at all */

    xcb_xv_query_image_attributes_reply_t *att;
    picture_pool_t *pool; /* picture pool */
    picture_resource_t resource[MAX_PICTURES];
};

static picture_pool_t *Pool (vout_display_t *, unsigned);
static void Display (vout_display_t *, picture_t *);
static int Control (vout_display_t *, int, va_list);
static void Manage (vout_display_t *);

/**
 * Check that the X server supports the XVideo extension.
 */
static bool CheckXVideo (vout_display_t *vd, xcb_connection_t *conn)
{
    xcb_xv_query_extension_reply_t *r;
    xcb_xv_query_extension_cookie_t ck = xcb_xv_query_extension (conn);
    bool ok = false;

    /* We need XVideo 2.2 for PutImage */
    r = xcb_xv_query_extension_reply (conn, ck, NULL);
    if (r == NULL)
        msg_Dbg (vd, "XVideo extension not available");
    else
    if (r->major != 2)
        msg_Dbg (vd, "XVideo extension v%"PRIu8".%"PRIu8" unknown",
                 r->major, r->minor);
    else
    if (r->minor < 2)
        msg_Dbg (vd, "XVideo extension v%"PRIu8".%"PRIu8" too old",
                 r->major, r->minor);
    else
    {
        msg_Dbg (vd, "using XVideo extension v%"PRIu8".%"PRIu8,
                 r->major, r->minor);
        ok = true;
    }
    free (r);
    return ok;
}

static vlc_fourcc_t ParseFormat (vout_display_t *vd,
                                 const xcb_xv_image_format_info_t *restrict f)
{
    if (f->byte_order != ORDER && f->bpp != 8)
        return 0; /* Argh! */

    switch (f->type)
    {
      case XCB_XV_IMAGE_FORMAT_INFO_TYPE_RGB:
        switch (f->num_planes)
        {
          case 1:
            switch (f->bpp)
            {
              case 32:
                if (f->depth == 24)
                    return VLC_CODEC_RGB32;
                if (f->depth == 32)
                    return 0; /* ARGB -> VLC cannot do that currently */
                break;
              case 24:
                if (f->depth == 24)
                    return VLC_CODEC_RGB24;
                break;
              case 16:
                if (f->depth == 16)
                    return VLC_CODEC_RGB16;
                if (f->depth == 15)
                    return VLC_CODEC_RGB15;
                break;
              case 8:
                if (f->depth == 8)
                    return VLC_CODEC_RGB8;
                break;
            }
            break;
        }
        msg_Err (vd, "unknown XVideo RGB format %"PRIx32" (%.4s)",
                 f->id, f->guid);
        msg_Dbg (vd, " %"PRIu8" planes, %"PRIu8" bits/pixel, "
                 "depth %"PRIu8, f->num_planes, f->bpp, f->depth);
        break;

      case XCB_XV_IMAGE_FORMAT_INFO_TYPE_YUV:
        if (f->u_sample_bits != f->v_sample_bits
         || f->vhorz_u_period != f->vhorz_v_period
         || f->vvert_u_period != f->vvert_v_period
         || f->y_sample_bits != 8 || f->u_sample_bits != 8
         || f->vhorz_y_period != 1 || f->vvert_y_period != 1)
            goto bad;
        switch (f->num_planes)
        {
          case 1:
            switch (f->bpp)
            {
              /*untested: case 24:
                if (f->vhorz_u_period == 1 && f->vvert_u_period == 1)
                    return VLC_CODEC_I444;
                break;*/
              case 16:
                if (f->vhorz_u_period == 2 && f->vvert_u_period == 1)
                {
                    if (!strcmp ((const char *)f->vcomp_order, "YUYV"))
                        return VLC_CODEC_YUYV;
                    if (!strcmp ((const char *)f->vcomp_order, "UYVY"))
                        return VLC_CODEC_UYVY;
                }
                break;
            }
            break;
          case 3:
            switch (f->bpp)
            {
              case 12:
                if (f->vhorz_u_period == 2 && f->vvert_u_period == 2)
                {
                    if (!strcmp ((const char *)f->vcomp_order, "YVU"))
                        return VLC_CODEC_YV12;
                    if (!strcmp ((const char *)f->vcomp_order, "YUV"))
                        return VLC_CODEC_I420;
                }
            }
            break;
        }
    bad:
        msg_Err (vd, "unknown XVideo YUV format %"PRIx32" (%.4s)", f->id,
                 f->guid);
        msg_Dbg (vd, " %"PRIu8" planes, %"PRIu32" bits/pixel, "
                 "%"PRIu32"/%"PRIu32"/%"PRIu32" bits/sample", f->num_planes,
                 f->bpp, f->y_sample_bits, f->u_sample_bits, f->v_sample_bits);
        msg_Dbg (vd, " period: %"PRIu32"/%"PRIu32"/%"PRIu32"x"
                 "%"PRIu32"/%"PRIu32"/%"PRIu32,
                 f->vhorz_y_period, f->vhorz_u_period, f->vhorz_v_period,
                 f->vvert_y_period, f->vvert_u_period, f->vvert_v_period);
        msg_Warn (vd, " order: %.32s", f->vcomp_order);
        break;
    }
    return 0;
}


static const xcb_xv_image_format_info_t *
FindFormat (vout_display_t *vd,
            vlc_fourcc_t chroma, const video_format_t *fmt,
            xcb_xv_port_t port,
            const xcb_xv_list_image_formats_reply_t *list,
            xcb_xv_query_image_attributes_reply_t **restrict pa)
{
    xcb_connection_t *conn = vd->sys->conn;
    const xcb_xv_image_format_info_t *f, *end;

#ifndef XCB_XV_OLD
    f = xcb_xv_list_image_formats_format (list);
#else
    f = (xcb_xv_image_format_info_t *) (list + 1);
#endif
    end = f + xcb_xv_list_image_formats_format_length (list);
    for (; f < end; f++)
    {
        if (chroma != ParseFormat (vd, f))
            continue;

        /* VLC pads scanline to 16 pixels internally */
        unsigned width = fmt->i_width;
        unsigned height = fmt->i_height;
        xcb_xv_query_image_attributes_reply_t *i;
        i = xcb_xv_query_image_attributes_reply (conn,
            xcb_xv_query_image_attributes (conn, port, f->id,
                                           width, height), NULL);
        if (i == NULL)
            continue;

        if (i->width != width || i->height != height)
        {
            msg_Warn (vd, "incompatible size %ux%u -> %"PRIu32"x%"PRIu32,
                      fmt->i_width, fmt->i_height,
                      i->width, i->height);
            var_Create (vd->p_libvlc, "xvideo-resolution-error", VLC_VAR_BOOL);
            if (!var_GetBool (vd->p_libvlc, "xvideo-resolution-error"))
            {
                dialog_FatalWait (vd, _("Video acceleration not available"),
                    _("Your video output acceleration driver does not support "
                      "the required resolution: %ux%u pixels. The maximum "
                      "supported resolution is %"PRIu32"x%"PRIu32".\n"
                      "Video output acceleration will be disabled. However, "
                      "rendering videos with overly large resolution "
                      "may cause severe performance degration."),
                                  width, height, i->width, i->height);
                var_SetBool (vd->p_libvlc, "xvideo-resolution-error", true);
            }
            free (i);
            continue;
        }
        *pa = i;
        return f;
    }
    return NULL;
}


/**
 * Probe the X server.
 */
static int Open (vlc_object_t *obj)
{
    vout_display_t *vd = (vout_display_t *)obj;
    vout_display_sys_t *p_sys = malloc (sizeof (*p_sys));

    if (!var_CreateGetBool (obj, "overlay"))
        return VLC_EGENERIC;
    if (p_sys == NULL)
        return VLC_ENOMEM;

    vd->sys = p_sys;

    /* Connect to X */
    xcb_connection_t *conn;
    const xcb_screen_t *screen;
    uint8_t depth;
    p_sys->embed = GetWindow (vd, &conn, &screen, &depth);
    if (p_sys->embed == NULL)
    {
        free (p_sys);
        return VLC_EGENERIC;
    }

    p_sys->conn = conn;
    p_sys->att = NULL;
    p_sys->pool = NULL;
    p_sys->swap_uv = false;

    if (!CheckXVideo (vd, conn))
    {
        msg_Warn (vd, "Please enable XVideo 2.2 for faster video display");
        goto error;
    }

    p_sys->window = xcb_generate_id (conn);
    xcb_pixmap_t pixmap = xcb_generate_id (conn);

    /* Cache adaptors infos */
    xcb_xv_query_adaptors_reply_t *adaptors =
        xcb_xv_query_adaptors_reply (conn,
            xcb_xv_query_adaptors (conn, p_sys->embed->handle.xid), NULL);
    if (adaptors == NULL)
        goto error;

    int forced_adaptor = var_CreateGetInteger (obj, "xvideo-adaptor");

    /* */
    video_format_t fmt = vd->fmt;
    bool found_adaptor = false;

    xcb_xv_adaptor_info_iterator_t it;
    for (it = xcb_xv_query_adaptors_info_iterator (adaptors);
         it.rem > 0 && !found_adaptor;
         xcb_xv_adaptor_info_next (&it))
    {
        const xcb_xv_adaptor_info_t *a = it.data;
        char *name;

        if (forced_adaptor != -1 && forced_adaptor != 0)
        {
            forced_adaptor--;
            continue;
        }

        if (!(a->type & XCB_XV_TYPE_IMAGE_MASK))
            continue;

        xcb_xv_list_image_formats_reply_t *r =
            xcb_xv_list_image_formats_reply (conn,
                xcb_xv_list_image_formats (conn, a->base_id), NULL);
        if (r == NULL)
            continue;

        /* Look for an image format */
        const xcb_xv_image_format_info_t *xfmt = NULL;
        const vlc_fourcc_t *chromas, chromas_default[] = {
            fmt.i_chroma,
            VLC_CODEC_RGB32,
            VLC_CODEC_RGB24,
            VLC_CODEC_RGB16,
            VLC_CODEC_RGB15,
            VLC_CODEC_YUYV,
            0
        };
        if (vlc_fourcc_IsYUV (fmt.i_chroma))
            chromas = vlc_fourcc_GetYUVFallback (fmt.i_chroma);
        else
            chromas = chromas_default;

        vlc_fourcc_t chroma;
        for (size_t i = 0; chromas[i]; i++)
        {
            chroma = chromas[i];

            /* Oink oink! */
            if ((chroma == VLC_CODEC_I420 || chroma == VLC_CODEC_YV12)
             && a->name_size >= 4
             && !memcmp ("OMAP", xcb_xv_adaptor_info_name (a), 4))
            {
                msg_Dbg (vd, "skipping slow I420 format");
                continue; /* OMAP framebuffer sucks at YUV 4:2:0 */
            }

            xfmt = FindFormat (vd, chroma, &fmt, a->base_id, r, &p_sys->att);
            if (xfmt != NULL)
            {
                p_sys->id = xfmt->id;
                p_sys->swap_uv = vlc_fourcc_AreUVPlanesSwapped (fmt.i_chroma,
                                                                chroma);
                if (!p_sys->swap_uv)
                    fmt.i_chroma = chroma;
                if (xfmt->type == XCB_XV_IMAGE_FORMAT_INFO_TYPE_RGB)
                {
                    fmt.i_rmask = xfmt->red_mask;
                    fmt.i_gmask = xfmt->green_mask;
                    fmt.i_bmask = xfmt->blue_mask;
                }
                break;
            }
        }
        free (r);
        if (xfmt == NULL) /* No acceptable image formats */
            continue;

        /* Grab a port */
        for (unsigned i = 0; i < a->num_ports; i++)
        {
             xcb_xv_port_t port = a->base_id + i;
             xcb_xv_grab_port_reply_t *gr =
                 xcb_xv_grab_port_reply (conn,
                     xcb_xv_grab_port (conn, port, XCB_CURRENT_TIME), NULL);
             uint8_t result = gr ? gr->result : 0xff;

             free (gr);
             if (result == 0)
             {
                 p_sys->port = port;
                 goto grabbed_port;
             }
             msg_Dbg (vd, "cannot grab port %"PRIu32": Xv error %"PRIu8, port,
                      result);
        }
        continue; /* No usable port */

    grabbed_port:
        /* Found port - initialize selected format */
        name = strndup (xcb_xv_adaptor_info_name (a), a->name_size);
        if (name != NULL)
        {
            msg_Dbg (vd, "using adaptor %s", name);
            free (name);
        }
        msg_Dbg (vd, "using port %"PRIu32, p_sys->port);
        msg_Dbg (vd, "using image format 0x%"PRIx32, p_sys->id);

        /* Look for an X11 visual, create a window */
        xcb_xv_format_t *f = xcb_xv_adaptor_info_formats (a);
        for (uint_fast16_t i = a->num_formats; i > 0; i--, f++)
        {
            if (f->depth != screen->root_depth)
                continue; /* this would fail anyway */

            uint32_t mask =
                XCB_CW_BACK_PIXMAP |
                XCB_CW_BACK_PIXEL |
                XCB_CW_BORDER_PIXMAP |
                XCB_CW_BORDER_PIXEL |
                XCB_CW_EVENT_MASK |
                XCB_CW_COLORMAP;
            const uint32_t list[] = {
                /* XCB_CW_BACK_PIXMAP */
                pixmap,
                /* XCB_CW_BACK_PIXEL */
                screen->black_pixel,
                /* XCB_CW_BORDER_PIXMAP */
                pixmap,
                /* XCB_CW_BORDER_PIXEL */
                screen->black_pixel,
                /* XCB_CW_EVENT_MASK */
                XCB_EVENT_MASK_VISIBILITY_CHANGE,
                /* XCB_CW_COLORMAP */
                screen->default_colormap,
            };

            xcb_void_cookie_t c;

            xcb_create_pixmap (conn, f->depth, pixmap, screen->root, 1, 1);
            c = xcb_create_window_checked (conn, f->depth, p_sys->window,
                 p_sys->embed->handle.xid, 0, 0, 1, 1, 0,
                 XCB_WINDOW_CLASS_INPUT_OUTPUT, f->visual, mask, list);

            if (!CheckError (vd, conn, "cannot create X11 window", c))
            {
                msg_Dbg (vd, "using X11 visual ID 0x%"PRIx32
                         " (depth: %"PRIu8")", f->visual, f->depth);
                msg_Dbg (vd, "using X11 window 0x%08"PRIx32, p_sys->window);
                goto created_window;
            }
        }
        xcb_xv_ungrab_port (conn, p_sys->port, XCB_CURRENT_TIME);
        msg_Dbg (vd, "no usable X11 visual");
        continue; /* No workable XVideo format (visual/depth) */

    created_window:
        found_adaptor = true;
        break;
    }
    free (adaptors);
    if (!found_adaptor)
    {
        msg_Err (vd, "no available XVideo adaptor");
        goto error;
    }
    else
    {
        xcb_map_window (conn, p_sys->window);

        vout_display_place_t place;

        vout_display_PlacePicture (&place, &vd->source, vd->cfg, false);
        p_sys->width  = place.width;
        p_sys->height = place.height;

        /* */
        const uint32_t values[] = {
            place.x, place.y, place.width, place.height };
        xcb_configure_window (conn, p_sys->window,
                              XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y |
                              XCB_CONFIG_WINDOW_WIDTH |
                              XCB_CONFIG_WINDOW_HEIGHT,
                              values);
    }
    p_sys->visible = false;

    /* Create graphic context */
    p_sys->gc = xcb_generate_id (conn);
    xcb_create_gc (conn, p_sys->gc, p_sys->window, 0, NULL);
    msg_Dbg (vd, "using X11 graphic context 0x%08"PRIx32, p_sys->gc);

    /* Create cursor */
    p_sys->cursor = CreateBlankCursor (conn, screen);

    CheckSHM (obj, conn, &p_sys->shm);

    /* */
    vout_display_info_t info = vd->info;
    info.has_pictures_invalid = false;

    /* Setup vout_display_t once everything is fine */
    vd->fmt = fmt;
    vd->info = info;

    vd->pool = Pool;
    vd->prepare = NULL;
    vd->display = Display;
    vd->control = Control;
    vd->manage = Manage;

    /* */
    vout_display_SendEventFullscreen (vd, false);
    unsigned width, height;
    if (!GetWindowSize (p_sys->embed, conn, &width, &height))
        vout_display_SendEventDisplaySize (vd, width, height, false);

    return VLC_SUCCESS;

error:
    Close (obj);
    return VLC_EGENERIC;
}


/**
 * Disconnect from the X server.
 */
static void Close (vlc_object_t *obj)
{
    vout_display_t *vd = (vout_display_t *)obj;
    vout_display_sys_t *p_sys = vd->sys;

    if (p_sys->pool)
    {
        for (unsigned i = 0; i < MAX_PICTURES; i++)
        {
            picture_resource_t *res = &p_sys->resource[i];

            if (!res->p->p_pixels)
                break;
            PictureResourceFree (res, NULL);
        }
        picture_pool_Delete (p_sys->pool);
    }

    /* show the default cursor */
    xcb_change_window_attributes (p_sys->conn, p_sys->embed->handle.xid, XCB_CW_CURSOR,
                                  &(uint32_t) { XCB_CURSOR_NONE });
    xcb_flush (p_sys->conn);

    free (p_sys->att);
    xcb_disconnect (p_sys->conn);
    vout_display_DeleteWindow (vd, p_sys->embed);
    free (p_sys);
}

/**
 * Return a direct buffer
 */
static picture_pool_t *Pool (vout_display_t *vd, unsigned requested_count)
{
    vout_display_sys_t *p_sys = vd->sys;
    (void)requested_count;

    if (!p_sys->pool)
    {
        memset (p_sys->resource, 0, sizeof(p_sys->resource));

        const uint32_t *pitches =
            xcb_xv_query_image_attributes_pitches (p_sys->att);
        const uint32_t *offsets =
            xcb_xv_query_image_attributes_offsets (p_sys->att);
        p_sys->data_size = p_sys->att->data_size;

        unsigned count;
        picture_t *pic_array[MAX_PICTURES];
        for (count = 0; count < MAX_PICTURES; count++)
        {
            picture_resource_t *res = &p_sys->resource[count];

            for (int i = 0; i < __MIN (p_sys->att->num_planes, PICTURE_PLANE_MAX); i++)
            {
                res->p[i].i_lines =
                    ((i + 1 < p_sys->att->num_planes ? offsets[i+1] :
                                                       p_sys->data_size) - offsets[i]) / pitches[i];
                res->p[i].i_pitch = pitches[i];
            }
            if (PictureResourceAlloc (vd, res, p_sys->att->data_size,
                                      p_sys->conn, p_sys->shm))
                break;

            /* Allocate further planes as specified by XVideo */
            /* We assume that offsets[0] is zero */
            for (int i = 1; i < __MIN (p_sys->att->num_planes, PICTURE_PLANE_MAX); i++)
                res->p[i].p_pixels = res->p[0].p_pixels + offsets[i];
            if (p_sys->swap_uv)
            {   /* YVU: swap U and V planes */
                uint8_t *buf = res->p[2].p_pixels;
                res->p[2].p_pixels = res->p[1].p_pixels;
                res->p[1].p_pixels = buf;
            }

            pic_array[count] = picture_NewFromResource (&vd->fmt, res);
            if (!pic_array[count])
            {
                PictureResourceFree (res, p_sys->conn);
                memset (res, 0, sizeof(*res));
                break;
            }
        }

        if (count == 0)
            return NULL;

        p_sys->pool = picture_pool_New (count, pic_array);
        /* TODO release picture resources if NULL */
        xcb_flush (p_sys->conn);
    }

    return p_sys->pool;
}

/**
 * Sends an image to the X server.
 */
static void Display (vout_display_t *vd, picture_t *pic)
{
    vout_display_sys_t *p_sys = vd->sys;
    xcb_shm_seg_t segment = pic->p_sys->segment;
    xcb_void_cookie_t ck;

    if (!p_sys->visible)
        goto out;
    if (segment)
        ck = xcb_xv_shm_put_image_checked (p_sys->conn, p_sys->port,
                              p_sys->window, p_sys->gc, segment, p_sys->id, 0,
                   /* Src: */ vd->source.i_x_offset,
                              vd->source.i_y_offset,
                              vd->source.i_visible_width,
                              vd->source.i_visible_height,
                   /* Dst: */ 0, 0, p_sys->width, p_sys->height,
                /* Memory: */ pic->p->i_pitch / pic->p->i_pixel_pitch,
                              pic->p->i_lines, false);
    else
        ck = xcb_xv_put_image_checked (p_sys->conn, p_sys->port, p_sys->window,
                          p_sys->gc, p_sys->id,
                          vd->source.i_x_offset,
                          vd->source.i_y_offset,
                          vd->source.i_visible_width,
                          vd->source.i_visible_height,
                          0, 0, p_sys->width, p_sys->height,
                          pic->p->i_pitch / pic->p->i_pixel_pitch,
                          pic->p->i_lines,
                          p_sys->data_size, pic->p->p_pixels);

    /* Wait for reply. See x11.c for rationale. */
    xcb_generic_error_t *e = xcb_request_check (p_sys->conn, ck);
    if (e != NULL)
    {
        msg_Dbg (vd, "%s: X11 error %d", "cannot put image", e->error_code);
        free (e);
    }
out:
    picture_Release (pic);
}

static int Control (vout_display_t *vd, int query, va_list ap)
{
    vout_display_sys_t *p_sys = vd->sys;

    switch (query)
    {
    case VOUT_DISPLAY_CHANGE_FULLSCREEN:
    {
        const vout_display_cfg_t *c = va_arg (ap, const vout_display_cfg_t *);
        return vout_window_SetFullScreen (p_sys->embed, c->is_fullscreen);
    }

    case VOUT_DISPLAY_CHANGE_DISPLAY_SIZE:
    case VOUT_DISPLAY_CHANGE_DISPLAY_FILLED:
    case VOUT_DISPLAY_CHANGE_ZOOM:
    case VOUT_DISPLAY_CHANGE_SOURCE_ASPECT:
    case VOUT_DISPLAY_CHANGE_SOURCE_CROP:
    {
        const vout_display_cfg_t *cfg;
        const video_format_t *source;
        bool is_forced = false;

        if (query == VOUT_DISPLAY_CHANGE_SOURCE_ASPECT
         || query == VOUT_DISPLAY_CHANGE_SOURCE_CROP)
        {
            source = (const video_format_t *)va_arg (ap, const video_format_t *);
            cfg = vd->cfg;
        }
        else
        {
            source = &vd->source;
            cfg = (const vout_display_cfg_t*)va_arg (ap, const vout_display_cfg_t *);
            if (query == VOUT_DISPLAY_CHANGE_DISPLAY_SIZE)
                is_forced = (bool)va_arg (ap, int);
        }

        /* */
        if (query == VOUT_DISPLAY_CHANGE_DISPLAY_SIZE
         && is_forced
         && (cfg->display.width  != vd->cfg->display.width
           ||cfg->display.height != vd->cfg->display.height)
         && vout_window_SetSize (p_sys->embed,
                                  cfg->display.width,
                                  cfg->display.height))
            return VLC_EGENERIC;

        vout_display_place_t place;
        vout_display_PlacePicture (&place, source, cfg, false);
        p_sys->width  = place.width;
        p_sys->height = place.height;

        /* Move the picture within the window */
        const uint32_t values[] = { place.x, place.y,
                                    place.width, place.height, };
        xcb_configure_window (p_sys->conn, p_sys->window,
                              XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y
                            | XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT,
                              values);
        xcb_flush (p_sys->conn);
        return VLC_SUCCESS;
    }
    case VOUT_DISPLAY_CHANGE_WINDOW_STATE:
    {
        unsigned state = va_arg (ap, unsigned);
        return vout_window_SetState (p_sys->embed, state);
    }

    /* Hide the mouse. It will be send when
     * vout_display_t::info.b_hide_mouse is false */
    case VOUT_DISPLAY_HIDE_MOUSE:
        xcb_change_window_attributes (p_sys->conn, p_sys->embed->handle.xid,
                                  XCB_CW_CURSOR, &(uint32_t){ p_sys->cursor });
        return VLC_SUCCESS;
    case VOUT_DISPLAY_RESET_PICTURES:
        assert(0);
    default:
        msg_Err (vd, "Unknown request in XCB vout display");
        return VLC_EGENERIC;
    }
}

static void Manage (vout_display_t *vd)
{
    vout_display_sys_t *p_sys = vd->sys;

    ManageEvent (vd, p_sys->conn, &p_sys->visible);
}

