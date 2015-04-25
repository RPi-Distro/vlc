/*****************************************************************************
 * snapshot.c : vout internal snapshot
 *****************************************************************************
 * Copyright (C) 2009 Laurent Aimar
 * $Id: 7b89803740174cb81ad09718d4a7f997d37a6f18 $
 *
 * Authors: Gildas Bazin <gbazin _AT_ videolan _DOT_ org>
 *          Laurent Aimar <fenrir _AT_ videolan _DOT_ org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <assert.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <dirent.h>
#include <time.h>

#include <vlc_common.h>
#include <vlc_fs.h>
#include <vlc_strings.h>
#include <vlc_block.h>
#include <vlc_vout.h>

#include "snapshot.h"
#include "vout_internal.h"

/* */
void vout_snapshot_Init(vout_snapshot_t *snap)
{
    vlc_mutex_init(&snap->lock);
    vlc_cond_init(&snap->wait);

    snap->is_available = true;
    snap->request_count = 0;
    snap->picture = NULL;
}
void vout_snapshot_Clean(vout_snapshot_t *snap)
{
    picture_t *picture = snap->picture;
    while (picture) {
        picture_t *next = picture->p_next;
        picture_Release(picture);
        picture = next;
    }

    vlc_cond_destroy(&snap->wait);
    vlc_mutex_destroy(&snap->lock);
}

void vout_snapshot_End(vout_snapshot_t *snap)
{
    vlc_mutex_lock(&snap->lock);

    snap->is_available = false;

    vlc_cond_broadcast(&snap->wait);
    vlc_mutex_unlock(&snap->lock);
}

/* */
picture_t *vout_snapshot_Get(vout_snapshot_t *snap, mtime_t timeout)
{
    vlc_mutex_lock(&snap->lock);

    /* */
    snap->request_count++;

    /* */
    const mtime_t deadline = mdate() + timeout;
    while (snap->is_available && !snap->picture && mdate() < deadline)
        vlc_cond_timedwait(&snap->wait, &snap->lock, deadline);

    /* */
    picture_t *picture = snap->picture;
    if (picture)
        snap->picture = picture->p_next;
    else if (snap->request_count > 0)
        snap->request_count--;

    vlc_mutex_unlock(&snap->lock);

    return picture;
}

/* */
bool vout_snapshot_IsRequested(vout_snapshot_t *snap)
{
    bool has_request = false;
    if (!vlc_mutex_trylock(&snap->lock)) {
        has_request = snap->request_count > 0;
        vlc_mutex_unlock(&snap->lock);
    }
    return has_request;
}
void vout_snapshot_Set(vout_snapshot_t *snap,
                       const video_format_t *fmt,
                       const picture_t *picture)
{
    if (!fmt)
        fmt = &picture->format;

    vlc_mutex_lock(&snap->lock);
    while (snap->request_count > 0) {
        picture_t *dup = picture_NewFromFormat(fmt);
        if (!dup)
            break;

        picture_Copy(dup, picture);

        dup->p_next = snap->picture;
        snap->picture = dup;
        snap->request_count--;
    }
    vlc_cond_broadcast(&snap->wait);
    vlc_mutex_unlock(&snap->lock);
}
/* */
char *vout_snapshot_GetDirectory(void)
{
    return config_GetUserDir(VLC_PICTURES_DIR);
}
/* */
int vout_snapshot_SaveImage(char **name, int *sequential,
                             const block_t *image,
                             vout_thread_t *p_vout,
                             const vout_snapshot_save_cfg_t *cfg)
{
    /* */
    char *filename;
    DIR *pathdir = vlc_opendir(cfg->path);
    input_thread_t *input = (input_thread_t*)p_vout->p->input;
    if (pathdir != NULL) {
        /* The use specified a directory path */
        closedir(pathdir);

        /* */
        char *prefix = NULL;
        if (cfg->prefix_fmt)
            prefix = str_format(input, cfg->prefix_fmt);
        if (prefix)
            filename_sanitize(prefix);
        else {
            prefix = strdup("vlcsnap-");
            if (!prefix)
                goto error;
        }

        if (cfg->is_sequential) {
            for (int num = cfg->sequence; ; num++) {
                struct stat st;

                if (asprintf(&filename, "%s" DIR_SEP "%s%05d.%s",
                             cfg->path, prefix, num, cfg->format) < 0) {
                    free(prefix);
                    goto error;
                }
                if (vlc_stat(filename, &st)) {
                    *sequential = num;
                    break;
                }
                free(filename);
            }
        } else {
            struct timeval tv;
            struct tm curtime;
            char buffer[128];

            gettimeofday(&tv, NULL);
            if (localtime_r(&tv.tv_sec, &curtime) == NULL)
                gmtime_r(&tv.tv_sec, &curtime);
            if (strftime(buffer, sizeof(buffer), "%Y-%m-%d-%Hh%Mm%Ss",
                         &curtime) == 0)
                strcpy(buffer, "error");

            if (asprintf(&filename, "%s" DIR_SEP "%s%s%03u.%s",
                         cfg->path, prefix, buffer,
                         (unsigned)tv.tv_usec / 1000, cfg->format) < 0)
                filename = NULL;
        }
        free(prefix);
    } else {
        /* The user specified a full path name (including file name) */
        filename = str_format(input, cfg->path);
        path_sanitize(filename);
    }

    if (!filename)
        goto error;

    /* Save the snapshot */
    FILE *file = vlc_fopen(filename, "wb");
    if (!file) {
        msg_Err(p_vout, "Failed to open '%s'", filename);
        free(filename);
        goto error;
    }
    if (fwrite(image->p_buffer, image->i_buffer, 1, file) != 1) {
        msg_Err(p_vout, "Failed to write to '%s'", filename);
        fclose(file);
        free(filename);
        goto error;
    }
    fclose(file);

    /* */
    if (name)
        *name = filename;
    else
        free(filename);

    return VLC_SUCCESS;

error:
    msg_Err(p_vout, "could not save snapshot");
    return VLC_EGENERIC;
}

