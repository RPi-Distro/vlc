/*****************************************************************************
 * gnomevfs.c: GnomeVFS input
 *****************************************************************************
 * Copyright (C) 2005 VLC authors and VideoLAN
 * $Id: e4d7f1ab6af140de1b050f81a985e8a6b63bba8f $
 *
 * Authors: Benjamin Pracht <bigben -AT- videolan -DOT- org>
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_access.h>

#include <libgnomevfs/gnome-vfs.h>

#include <vlc_url.h>

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

vlc_module_begin ()
    set_description( N_("GnomeVFS input") )
    set_shortname( "GnomeVFS" )
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_ACCESS )
    set_capability( "access", 10 )
    add_shortcut( "gnomevfs" )
    set_callbacks( Open, Close )
vlc_module_end ()


/*****************************************************************************
 * Exported prototypes
 *****************************************************************************/
static int     Seek( access_t *, uint64_t );
static ssize_t Read( access_t *, uint8_t *, size_t );
static int     Control( access_t *, int, va_list );

struct access_sys_t
{
    char *psz_name;

    GnomeVFSHandle *p_handle;
    GnomeVFSFileInfo *p_file_info;

    bool b_local;
    bool b_seekable;
    bool b_pace_control;
};

/* NOTE: we do not handle memory errors in this plugin.
 * Underlying glib does not, so there is no point in doing it here either. */

/*****************************************************************************
 * Open: open the file
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    access_t       *p_access = (access_t*)p_this;
    access_sys_t   *p_sys = NULL;
    char           *psz_name = NULL;
    char           *psz_uri = NULL;
    char           *psz_unescaped = NULL;
    char           *psz_expand_tilde = NULL;
    GnomeVFSURI    *p_uri = NULL;
    GnomeVFSResult  i_ret;
    GnomeVFSHandle *p_handle = NULL;
    if( !(gnome_vfs_init()) )
    {
        msg_Warn( p_access, "couldn't initilize GnomeVFS" );
        return VLC_EGENERIC;
    }

    /* FIXME
       Since GnomeVFS segfaults on exit if we initialize it without trying to
       open a file with a valid protocol, try to open at least file:// */
    gnome_vfs_open( &p_handle, "file://", 5 );

    STANDARD_READ_ACCESS_INIT;

    p_sys->p_handle = p_handle;
    p_sys->b_pace_control = true;

    if( strcmp( "gnomevfs", p_access->psz_access ) &&
                                            *(p_access->psz_access) != '\0')
    {
        asprintf( &psz_name, "%s://%s", p_access->psz_access,
                  p_access->psz_location );
    }
    else
    {
        psz_name = strdup( p_access->psz_location );
    }
    psz_expand_tilde = gnome_vfs_expand_initial_tilde( psz_name );

    psz_unescaped = gnome_vfs_make_uri_from_shell_arg( psz_expand_tilde );

   /* gnome_vfs_make_uri_from_shell_arg will only escape the uri
      for relative paths. So we need to use
      gnome_vfs_escape_host_and_path_string in other cases. */

    if( !strcmp( psz_unescaped, psz_expand_tilde ) )
    {
    /* Now we are sure that we have a complete valid unescaped URI beginning
       with the protocol. We want to escape it. However, gnomevfs's escaping
       function are broken and will try to escape characters un the username/
       password field. So parse the URI with vlc_UrlParse ans only escape the
       path */

        vlc_url_t url;
        char *psz_escaped_path;
        char *psz_path_begin;

        vlc_UrlParse( &url, psz_unescaped, 0 );
        psz_escaped_path = gnome_vfs_escape_path_string( url.psz_path );

        if( psz_escaped_path )
        {
    /* Now let's reconstruct a valid URI from all that stuff */
            psz_path_begin = psz_unescaped + strlen( psz_unescaped )
                                           - strlen( url.psz_path );
            *psz_path_begin = '\0';
            asprintf( &psz_uri, "%s%s", psz_unescaped, psz_escaped_path );

            g_free( psz_escaped_path );
            g_free( psz_unescaped );
        }
        else
        {
            psz_uri = psz_unescaped;
        }
    }
    else
    {
        psz_uri = psz_unescaped;
    }

    g_free( psz_expand_tilde );
    p_uri = gnome_vfs_uri_new( psz_uri );
    if( p_uri )
    {
        p_sys->p_file_info = gnome_vfs_file_info_new();
        i_ret = gnome_vfs_get_file_info_uri( p_uri,
                                                p_sys->p_file_info, 8 );

        if( i_ret )
        {
            msg_Warn( p_access, "cannot get file info for uri %s (%s)",
                                psz_uri, gnome_vfs_result_to_string( i_ret ) );
            gnome_vfs_file_info_unref( p_sys->p_file_info );
            gnome_vfs_uri_unref( p_uri);
            free( p_sys );
            free( psz_uri );
            free( psz_name );
            return VLC_EGENERIC;
        }
    }
    else
    {
        msg_Warn( p_access, "cannot parse MRL %s or unsupported protocol", psz_name );
        free( psz_uri );
        free( p_sys );
        free( psz_name );
        return VLC_EGENERIC;
    }

    msg_Dbg( p_access, "opening file `%s'", psz_uri );
    i_ret = gnome_vfs_open( &(p_sys->p_handle), psz_uri, 5 );
    if( i_ret )
    {
        msg_Warn( p_access, "cannot open file %s: %s", psz_uri,
                                gnome_vfs_result_to_string( i_ret ) );

        gnome_vfs_uri_unref( p_uri);
        free( psz_uri );
        free( p_sys );
        free( psz_name );
        return VLC_EGENERIC;
    }

    if (GNOME_VFS_FILE_INFO_LOCAL( p_sys->p_file_info ))
    {
        p_sys->b_local = true;
    }

    if( p_sys->p_file_info->type == GNOME_VFS_FILE_TYPE_REGULAR ||
        p_sys->p_file_info->type == GNOME_VFS_FILE_TYPE_CHARACTER_DEVICE ||
        p_sys->p_file_info->type == GNOME_VFS_FILE_TYPE_BLOCK_DEVICE )
    {
        p_sys->b_seekable = true;
    }
    else if( p_sys->p_file_info->type == GNOME_VFS_FILE_TYPE_FIFO
              || p_sys->p_file_info->type == GNOME_VFS_FILE_TYPE_SOCKET )
    {
        p_sys->b_seekable = false;
    }
    else
    {
        msg_Err( p_access, "unknown file type for `%s'", psz_name );
        return VLC_EGENERIC;
    }

    if( p_sys->b_seekable
    && !(p_sys->p_file_info->valid_fields & GNOME_VFS_FILE_INFO_FIELDS_SIZE) )
    {
        /* FIXME that's bad because all others access will be probed */
        msg_Warn( p_access, "file %s size is unknown, aborting", psz_name );
        gnome_vfs_file_info_unref( p_sys->p_file_info );
        gnome_vfs_uri_unref( p_uri);
        free( p_sys );
        free( psz_uri );
        free( psz_name );
        return VLC_EGENERIC;
    }

    free( psz_uri );
    p_sys->psz_name = psz_name;
    gnome_vfs_uri_unref( p_uri);
    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: close the target
 *****************************************************************************/
static void Close( vlc_object_t * p_this )
{
    access_t     *p_access = (access_t*)p_this;
    access_sys_t *p_sys = p_access->p_sys;
    int i_result;

    i_result = gnome_vfs_close( p_sys->p_handle );
    if( i_result )
    {
         msg_Err( p_access, "cannot close %s: %s", p_sys->psz_name,
                                gnome_vfs_result_to_string( i_result ) );
    }

    gnome_vfs_file_info_unref( p_sys->p_file_info );

    free( p_sys->psz_name );
    free( p_sys );
}

/*****************************************************************************
 * Read: standard read on a file descriptor.
 *****************************************************************************/
static ssize_t Read( access_t *p_access, uint8_t *p_buffer, size_t i_len )
{
    access_sys_t *p_sys = p_access->p_sys;
    GnomeVFSFileSize i_read_len;
    int i_ret;

    i_ret = gnome_vfs_read( p_sys->p_handle, p_buffer,
                                  (GnomeVFSFileSize)i_len, &i_read_len );
    if( i_ret )
    {
        p_access->info.b_eof = true;
        if( i_ret != GNOME_VFS_ERROR_EOF )
        {
            msg_Err( p_access, "read failed (%s)",
                                    gnome_vfs_result_to_string( i_ret ) );
        }
    }

    p_access->info.i_pos += (int64_t)i_read_len;
    if( p_access->info.i_pos >= p_sys->p_file_info->size
     && p_sys->p_file_info->size != 0 && p_sys->b_local )
    {
        gnome_vfs_file_info_clear( p_sys->p_file_info );
        i_ret = gnome_vfs_get_file_info_from_handle( p_sys->p_handle,
                                                     p_sys->p_file_info, 8 );
        if( i_ret )
            msg_Warn( p_access, "couldn't get file properties again (%s)",
                      gnome_vfs_result_to_string( i_ret ) );
    }
    return (int)i_read_len;
}

/*****************************************************************************
 * Seek: seek to a specific location in a file
 *****************************************************************************/
static int Seek( access_t *p_access, uint64_t i_pos )
{
    access_sys_t *p_sys = p_access->p_sys;
    int i_ret;

    i_ret = gnome_vfs_seek( p_sys->p_handle, GNOME_VFS_SEEK_START,
                                            (GnomeVFSFileOffset)i_pos);
    if ( !i_ret )
    {
        p_access->info.i_pos = i_pos;
    }
    else
    {
        GnomeVFSFileSize i_offset;
        msg_Err( p_access, "cannot seek (%s)",
                                        gnome_vfs_result_to_string( i_ret ) );
        i_ret = gnome_vfs_tell( p_sys->p_handle, &i_offset );
        if( !i_ret )
        {
            msg_Err( p_access, "cannot tell the current position (%s)",
                                        gnome_vfs_result_to_string( i_ret ) );
            return VLC_EGENERIC;
        }
    }
    /* Reset eof */
    p_access->info.b_eof = false;

    /* FIXME */
    return VLC_SUCCESS;
}

/*****************************************************************************
 * Control:
 *****************************************************************************/
static int Control( access_t *p_access, int i_query, va_list args )
{
    access_sys_t *p_sys = p_access->p_sys;
    bool   *pb_bool;
    int64_t      *pi_64;

    switch( i_query )
    {
        case ACCESS_CAN_SEEK:
        case ACCESS_CAN_FASTSEEK:
            pb_bool = (bool*)va_arg( args, bool* );
            *pb_bool = p_sys->b_seekable;
            break;

        case ACCESS_CAN_PAUSE:
        case ACCESS_CAN_CONTROL_PACE:
            pb_bool = (bool*)va_arg( args, bool* );
            *pb_bool = p_sys->b_pace_control;
            break;

        case ACCESS_GET_SIZE:
            *va_arg( args, uint64_t * ) = p_sys->p_file_info->size;
            break;

        case ACCESS_GET_PTS_DELAY:
            pi_64 = (int64_t*)va_arg( args, int64_t * );
            *pi_64 = DEFAULT_PTS_DELAY; /* FIXME */
            break;

        case ACCESS_SET_PAUSE_STATE:
            /* Nothing to do */
            break;

        default:
            return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}
