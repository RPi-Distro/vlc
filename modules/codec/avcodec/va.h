/*****************************************************************************
 * va.h: Video Acceleration API for avcodec
 *****************************************************************************
 * Copyright (C) 2009 Laurent Aimar
 * $Id: 6cb5bf48148d30795ebdd87e922141b71c3c6025 $
 *
 * Authors: Laurent Aimar <fenrir_AT_ videolan _DOT_ org>
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

#ifndef _VLC_VA_H
#define _VLC_VA_H 1

typedef struct vlc_va_t vlc_va_t;
struct vlc_va_t {
    char *description;

    int  (*setup)(vlc_va_t *, void **hw, vlc_fourcc_t *output,
                  int width, int height);
    int  (*get)(vlc_va_t *, AVFrame *frame);
    void (*release)(vlc_va_t *, AVFrame *frame);
    int  (*extract)(vlc_va_t *, picture_t *dst, AVFrame *src);
    void (*close)(vlc_va_t *);
};

static inline int vlc_va_Setup(vlc_va_t *va, void **hw, vlc_fourcc_t *output,
                                int width, int height)
{
    return va->setup(va, hw, output, width, height);
}
static inline int vlc_va_Get(vlc_va_t *va, AVFrame *frame)
{
    return va->get(va, frame);
}
static inline void vlc_va_Release(vlc_va_t *va, AVFrame *frame)
{
    va->release(va, frame);
}
static inline int vlc_va_Extract(vlc_va_t *va, picture_t *dst, AVFrame *src)
{
    return va->extract(va, dst, src);
}
static inline void vlc_va_Delete(vlc_va_t *va)
{
    va->close(va);
}

vlc_va_t *vlc_va_NewVaapi(int codec_id);
vlc_va_t *vlc_va_NewDxva2(vlc_object_t *log, int codec_id);

#endif

