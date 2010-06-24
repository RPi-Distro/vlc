/*****************************************************************************
 * maemo_interface.c : Interface creation of the maemo plugin
 *****************************************************************************
 * Copyright (C) 2008 the VideoLAN team
 * $Id: b6a2bd72a364fd6fed23ad8d3bec268c2a171a9e $
 *
 * Authors: Antoine Lejeune <phytos@videolan.org>
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

#include <vlc_common.h>

#include <gtk/gtk.h>

#include "maemo.h"
#include "maemo_callbacks.h"
#include "maemo_interface.h"
#include "maemo_input.h"

static void scan_maemo_for_media ( intf_thread_t *p_intf );
static void find_media_in_dir    ( const char *psz_dir, GList **pp_list );

static const char *ppsz_extensions[] =
    { "aac", "flac", "m4a", "m4p", "mka", "mp1", "mp2", "mp3",
      "ogg", "wav", "wma", "asf", "avi", "divx", "flv", "m1v",
      "m2v", "m4v", "mkv", "mov", "mpeg", "mpeg1", "mpeg2", "mpeg4",
      "mpg", "ogm", "wmv", NULL };

static const char *ppsz_media_dirs[] =
    { "/media/mmc1", "/media/mmc2", "/home/user/MyDocs/.videos", NULL };

void create_playlist( intf_thread_t *p_intf )
{
    GtkWidget *playlist;
    GtkWidget *scroll;
    GtkListStore *playlist_store;
    GtkTreeViewColumn *col;
    GtkCellRenderer *renderer;

    playlist = gtk_tree_view_new();

    playlist_store = gtk_list_store_new( 1, G_TYPE_STRING );
    p_intf->p_sys->p_playlist_store = playlist_store;

    gtk_tree_view_set_model( GTK_TREE_VIEW( playlist ),
                             GTK_TREE_MODEL( playlist_store ) );

    renderer = gtk_cell_renderer_text_new();
    col = gtk_tree_view_column_new_with_attributes( "File", renderer,
                                                    "text", 0, NULL );
    gtk_tree_view_append_column( GTK_TREE_VIEW( playlist ), col );

    g_object_set( playlist, "headers-visible", TRUE, NULL );
    scan_maemo_for_media( p_intf );

    scroll = gtk_scrolled_window_new( NULL, NULL );
    gtk_container_add( GTK_CONTAINER( scroll ), playlist );

    p_intf->p_sys->p_playlist_window = scroll;

    g_signal_connect( playlist, "row-activated",
                      G_CALLBACK( pl_row_activated_cb ), NULL );

    // Set callback with the vlc core
    var_AddCallback( p_intf->p_sys->p_playlist, "item-change",
                     item_changed_cb, p_intf );
    var_AddCallback( p_intf->p_sys->p_playlist, "item-current",
                     playlist_current_cb, p_intf );
    var_AddCallback( p_intf->p_sys->p_playlist, "activity",
                     activity_cb, p_intf );
}

void delete_playlist( intf_thread_t *p_intf )
{
    var_DelCallback( p_intf->p_sys->p_playlist, "item-change",
                     item_changed_cb, p_intf );
    var_DelCallback( p_intf->p_sys->p_playlist, "item-current",
                     playlist_current_cb, p_intf );
    var_DelCallback( p_intf->p_sys->p_playlist, "activity",
                     activity_cb, p_intf );
}

static void scan_maemo_for_media( intf_thread_t *p_intf )
{
    GtkListStore *playlist_store = GTK_LIST_STORE( p_intf->p_sys->p_playlist_store );
    GList *list = NULL;
    GtkTreeIter iter;

    for( int i = 0; ppsz_media_dirs[i]; i++ )
    {
        find_media_in_dir( ppsz_media_dirs[i], &list );
        msg_Dbg( p_intf, "Looking for media in %s", ppsz_media_dirs[i] );
    }

    list = g_list_first( list );
    while( list )
    {
        msg_Dbg( p_intf, "Adding : %s", (char *)list->data );
        gtk_list_store_append( playlist_store, &iter );
        gtk_list_store_set( playlist_store, &iter,
                            0, list->data, -1 );
        list = g_list_next( list );
    }
}

static void find_media_in_dir( const char *psz_dir, GList **pp_list )
{
    GDir *dir = NULL;
    const gchar *psz_name;
    char *psz_path;

    dir = g_dir_open( psz_dir, 0, NULL );
    if( !dir )
        return;
    while( ( psz_name = g_dir_read_name( dir ) ) != NULL )
    {
        psz_path = g_build_path( "/", psz_dir, psz_name, NULL );
        if( g_file_test( psz_path, G_FILE_TEST_IS_DIR ) &&
            !g_file_test( psz_path, G_FILE_TEST_IS_SYMLINK ) )
            find_media_in_dir( psz_path, pp_list );
        else
        {
            char *psz_ext = strrchr( psz_name, '.' );
            if( psz_ext )
            {
                psz_ext++;
                for( int i = 0; ppsz_extensions[i]; i++ )
                {
                    if( strcmp( psz_ext, ppsz_extensions[i] ) == 0 )
                    {
                        *pp_list = g_list_append( *pp_list, g_strdup( psz_path ) );
                        break;
                    }
                }
            }
        }
        g_free( psz_path );
    }

    g_dir_close( dir );
    return;
}
