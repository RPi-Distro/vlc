/*****************************************************************************
 * directory.c: expands a directory (directory: access plug-in)
 *****************************************************************************
 * Copyright (C) 2002-2008 the VideoLAN team
 * $Id: f2b8aec07913e54c84e2e9ad9c31e0273c07d651 $
 *
 * Authors: Derk-Jan Hartman <hartman at videolan dot org>
 *          Rémi Denis-Courmont
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
#include "fs.h"
#include <vlc_access.h>

#include <sys/types.h>
#ifdef HAVE_SYS_STAT_H
#   include <sys/stat.h>
#endif

#ifdef HAVE_UNISTD_H
#   include <unistd.h>
#   include <fcntl.h>
#elif defined( WIN32 ) && !defined( UNDER_CE )
#   include <io.h>
#endif

#ifdef HAVE_DIRENT_H
#   include <dirent.h>
#endif
#ifdef __sun__
static inline int dirfd (DIR *dir)
{
    return dir->dd_fd;
}
#endif

#include <vlc_fs.h>
#include <vlc_url.h>
#include <vlc_strings.h>

enum
{
    MODE_NONE,
    MODE_COLLAPSE,
    MODE_EXPAND,
};

typedef struct directory_t directory_t;
struct directory_t
{
    directory_t *parent;
    DIR         *handle;
    char        *uri;
#ifndef WIN32
    struct stat  st;
#endif
#ifndef HAVE_OPENAT
    char         *path;
#endif
};

struct access_sys_t
{
    directory_t *current;
    DIR *handle;
    char *uri;
    char *ignored_exts;
    int mode;
    int i_item_count;
    char *psz_xspf_extension;
};

/*****************************************************************************
 * Open: open the directory
 *****************************************************************************/
int DirOpen( vlc_object_t *p_this )
{
    access_t *p_access = (access_t*)p_this;

    if( !p_access->psz_path )
        return VLC_EGENERIC;

    DIR *handle = vlc_opendir (p_access->psz_path);
    if (handle == NULL)
        return VLC_EGENERIC;

    return DirInit (p_access, handle);
}

int DirInit (access_t *p_access, DIR *handle)
{
    access_sys_t *p_sys = malloc (sizeof (*p_sys));
    if (unlikely(p_sys == NULL))
        goto error;

    char *uri;
    if (!strcmp (p_access->psz_access, "fd"))
    {
        if (asprintf (&uri, "fd://%s", p_access->psz_path) == -1)
            uri = NULL;
    }
    else
        uri = make_URI (p_access->psz_path);
    if (unlikely(uri == NULL))
        goto error;

    p_access->p_sys = p_sys;
    p_sys->current = NULL;
    p_sys->handle = handle;
    p_sys->uri = uri;
    p_sys->ignored_exts = var_InheritString (p_access, "ignore-filetypes");
    p_sys->i_item_count = 0;
    p_sys->psz_xspf_extension = strdup( "" );

    /* Handle mode */
    char *psz = var_InheritString (p_access, "recursive");
    if (psz == NULL || !strcasecmp (psz, "none"))
        p_sys->mode = MODE_NONE;
    else if( !strcasecmp( psz, "collapse" )  )
        p_sys->mode = MODE_COLLAPSE;
    else
        p_sys->mode = MODE_EXPAND;
    free( psz );

    access_InitFields(p_access);
    p_access->pf_read  = NULL;
    p_access->pf_block = DirBlock;
    p_access->pf_seek  = NULL;
    p_access->pf_control= DirControl;
    free (p_access->psz_demux);
    p_access->psz_demux = strdup ("xspf-open");

    return VLC_SUCCESS;

error:
    closedir (handle);
    free (p_sys);
    return VLC_EGENERIC;
}

/*****************************************************************************
 * Close: close the target
 *****************************************************************************/
void DirClose( vlc_object_t * p_this )
{
    access_t *p_access = (access_t*)p_this;
    access_sys_t *p_sys = p_access->p_sys;

    while (p_sys->current)
    {
        directory_t *current = p_sys->current;

        p_sys->current = current->parent;
        closedir (current->handle);
        free (current->uri);
#ifndef HAVE_OPENAT
        free (current->path);
#endif
        free (current);
    }

    /* corner case: Block() not called ever */
    if (p_sys->handle != NULL)
        closedir (p_sys->handle);
    free (p_sys->uri);

    free (p_sys->psz_xspf_extension);
    free (p_sys->ignored_exts);
    free (p_sys);
}

/* Detect directories that recurse into themselves. */
static bool has_inode_loop (const directory_t *dir)
{
#ifndef WIN32
    dev_t dev = dir->st.st_dev;
    ino_t inode = dir->st.st_ino;

    while ((dir = dir->parent) != NULL)
        if ((dir->st.st_dev == dev) && (dir->st.st_ino == inode))
            return true;
#else
# undef fstat
# define fstat( fd, st ) (0)
    VLC_UNUSED( dir );
#endif
    return false;
}

block_t *DirBlock (access_t *p_access)
{
    access_sys_t *p_sys = p_access->p_sys;
    directory_t *current = p_sys->current;

    if (p_access->info.b_eof)
        return NULL;

    if (current == NULL)
    {   /* Startup: send the XSPF header */
        static const char header[] =
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
            "<playlist version=\"1\" xmlns=\"http://xspf.org/ns/0/\" xmlns:vlc=\"http://www.videolan.org/vlc/playlist/ns/0/\">\n"
            " <trackList>\n";
        block_t *block = block_Alloc (sizeof (header) - 1);
        if (!block)
            goto fatal;
        memcpy (block->p_buffer, header, sizeof (header) - 1);

        /* "Open" the base directory */
        current = malloc (sizeof (*current));
        if (current == NULL)
        {
            block_Release (block);
            goto fatal;
        }
        current->parent = NULL;
        current->handle = p_sys->handle;
#ifndef HAVE_OPENAT
        current->path = strdup (p_access->psz_path);
#endif
        current->uri = p_sys->uri;
        if (fstat (dirfd (current->handle), &current->st))
        {
            free (current);
            block_Release (block);
            goto fatal;
        }

        p_sys->handle = NULL;
        p_sys->uri = NULL;
        p_sys->current = current;
        return block;
    }

    char *entry = vlc_readdir (current->handle);
    if (entry == NULL)
    {   /* End of directory, go back to parent */
        closedir (current->handle);
        p_sys->current = current->parent;
        free (current->uri);
#ifndef HAVE_OPENAT
        free (current->path);
#endif
        free (current);

        if (p_sys->current == NULL)
        {   /* End of XSPF playlist */
            char *footer;
            int len = asprintf( &footer, " </trackList>\n" \
                " <extension application=\"http://www.videolan.org/vlc/playlist/0\">\n" \
                "%s" \
                " </extension>\n" \
                "</playlist>\n", p_sys->psz_xspf_extension );
            if (unlikely(len == -1))
                goto fatal;

            block_t *block = block_heap_Alloc (footer, footer, len);
            if (unlikely(block == NULL))
                free (footer);
            p_access->info.b_eof = true;
            return block;
        }
        else
        {
            /* This was the end of a "subnode" */
            /* Write the ID to the extension */
            char *old_xspf_extension = p_sys->psz_xspf_extension;
            if (old_xspf_extension == NULL)
                goto fatal;

            int len2 = asprintf( &p_sys->psz_xspf_extension, "%s  </vlc:node>\n", old_xspf_extension );
            if (len2 == -1)
                goto fatal;
            free( old_xspf_extension );
        }
        return NULL;
    }

    /* Skip current, parent and hidden directories */
    if (entry[0] == '.')
    {
        free (entry);
        return NULL;
    }
    /* Handle recursion */
    if (p_sys->mode != MODE_COLLAPSE)
    {
        directory_t *sub = malloc (sizeof (*sub));
        if (sub == NULL)
        {
            free (entry);
            return NULL;
        }

        DIR *handle;
#ifdef HAVE_OPENAT
        int fd = vlc_openat (dirfd (current->handle), entry, O_RDONLY);
        if (fd != -1)
        {
            handle = fdopendir (fd);
            if (handle == NULL)
                close (fd);
        }
        else
            handle = NULL;
#else
        if (asprintf (&sub->path, "%s/%s", current->path, entry) != -1)
            handle = vlc_opendir (sub->path);
        else
            handle = NULL;
#endif
        if (handle != NULL)
        {
            sub->parent = current;
            sub->handle = handle;

            char *encoded = encode_URI_component (entry);
            if ((encoded == NULL)
             || (asprintf (&sub->uri, "%s/%s", current->uri, encoded) == -1))
                 sub->uri = NULL;
            free (encoded);

            if ((p_sys->mode == MODE_NONE)
             || fstat (dirfd (handle), &sub->st)
             || has_inode_loop (sub)
             || (sub->uri == NULL))
            {
                free (entry);
                closedir (handle);
                free (sub->uri);
                free (sub);
                return NULL;
            }
            p_sys->current = sub;

            /* Add node to xspf extension */
            char *old_xspf_extension = p_sys->psz_xspf_extension;
            if (old_xspf_extension == NULL)
            {
                free (entry);
                goto fatal;
            }

            char *title = convert_xml_special_chars (entry);
            free (entry);
            if (title == NULL
             || asprintf (&p_sys->psz_xspf_extension, "%s"
                          "  <vlc:node title=\"%s\">\n", old_xspf_extension,
                          title) == -1)
            {
                free (title);
                goto fatal;
            }
            free (title);
            free (old_xspf_extension);
            return NULL;
        }
        else
            free (sub);
    }

    /* Skip files with ignored extensions */
    if (p_sys->ignored_exts != NULL)
    {
        const char *ext = strrchr (entry, '.');
        if (ext != NULL)
        {
            size_t extlen = strlen (++ext);
            for (const char *type = p_sys->ignored_exts, *end;
                 type[0]; type = end + 1)
            {
                end = strchr (type, ',');
                if (end == NULL)
                    end = type + strlen (type);

                if (type + extlen == end
                 && !strncasecmp (ext, type, extlen))
                {
                    free (entry);
                    return NULL;
                }

                if (*end == '\0')
                    break;
            }
        }
    }

    char *encoded = encode_URI_component (entry);
    free (entry);
    if (encoded == NULL)
        goto fatal;
    int len = asprintf (&entry,
                        "  <track><location>%s/%s</location>\n" \
                        "   <extension application=\"http://www.videolan.org/vlc/playlist/0\">\n" \
                        "    <vlc:id>%d</vlc:id>\n" \
                        "   </extension>\n" \
                        "  </track>\n",
                        current->uri, encoded, p_sys->i_item_count++);
    free (encoded);
    if (len == -1)
        goto fatal;

    /* Write the ID to the extension */
    char *old_xspf_extension = p_sys->psz_xspf_extension;
    if (old_xspf_extension == NULL)
        goto fatal;

    int len2 = asprintf( &p_sys->psz_xspf_extension, "%s   <vlc:item tid=\"%i\" />\n",
                            old_xspf_extension, p_sys->i_item_count-1 );
    if (len2 == -1)
        goto fatal;
    free( old_xspf_extension );

    block_t *block = block_heap_Alloc (entry, entry, len);
    if (unlikely(block == NULL))
    {
        free (entry);
        goto fatal;
    }
    return block;

fatal:
    p_access->info.b_eof = true;
    return NULL;
}

/*****************************************************************************
 * Control:
 *****************************************************************************/
int DirControl( access_t *p_access, int i_query, va_list args )
{
    switch( i_query )
    {
        /* */
        case ACCESS_CAN_SEEK:
        case ACCESS_CAN_FASTSEEK:
            *va_arg( args, bool* ) = false;
            break;

        case ACCESS_CAN_PAUSE:
        case ACCESS_CAN_CONTROL_PACE:
            *va_arg( args, bool* ) = true;
            break;

        /* */
        case ACCESS_GET_PTS_DELAY:
            *va_arg( args, int64_t * ) = DEFAULT_PTS_DELAY * 1000;
            break;

        /* */
        case ACCESS_SET_PAUSE_STATE:
        case ACCESS_GET_TITLE_INFO:
        case ACCESS_SET_TITLE:
        case ACCESS_SET_SEEKPOINT:
        case ACCESS_SET_PRIVATE_ID_STATE:
        case ACCESS_GET_CONTENT_TYPE:
        case ACCESS_GET_META:
            return VLC_EGENERIC;

        default:
            msg_Warn( p_access, "unimplemented query in control" );
            return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}
