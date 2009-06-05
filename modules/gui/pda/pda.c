/*****************************************************************************
 * pda.c : PDA Gtk2 plugin for vlc
 *****************************************************************************
 * Copyright (C) 2002 the VideoLAN team
 * $Id: 2b74125970b4cfd0019d2b28bc1e1fe33a3f4d61 $
 *
 * Authors: Jean-Paul Saman <jpsaman  _at_ videolan _dot_ org>
 *          Marc Ariberti <marcari@videolan.org>
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
#include <errno.h>                                                 /* ENOMEM */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_input.h>
#include <vlc_interface.h>
#include <vlc_playlist.h>

#include <gtk/gtk.h>

#include "pda_callbacks.h"
#include "pda_interface.h"
#include "pda_support.h"
#include "pda.h"

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
static int  Open         ( vlc_object_t * );
static void Close        ( vlc_object_t * );
static void Run          ( intf_thread_t * );

static int Manage        ( intf_thread_t *p_intf );
void GtkDisplayDate  ( GtkAdjustment *p_adj, gpointer userdata );
gint GtkModeManage   ( intf_thread_t * p_intf );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define AUTOPLAYFILE_TEXT  N_("Autoplay selected file")
#define AUTOPLAYFILE_LONGTEXT N_("Automatically play a file when selected in the "\
        "file selection list")

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin ()
    set_description( N_("PDA Linux Gtk2+ interface") )
    set_category( CAT_INTERFACE )
    set_subcategory( SUBCAT_INTERFACE_MAIN )
    set_capability( "interface", 0 )
    set_callbacks( Open, Close )
    add_shortcut( "pda" )
vlc_module_end ()

/*****************************************************************************
 * Open: initialize and create window
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_this;

    /* Allocate instance and initialize some members */
    p_intf->p_sys = malloc( sizeof( intf_sys_t ) );
    if( p_intf->p_sys == NULL )
        return VLC_ENOMEM;

#ifdef NEED_GTK2_MAIN
    msg_Dbg( p_intf, "Using gui-helper" );
    p_intf->p_sys->p_gtk_main =
        module_need( p_this, "gui-helper", "gtk2", true );
    if( p_intf->p_sys->p_gtk_main == NULL )
    {
        free( p_intf->p_sys );
        return VLC_ENOMOD;
    }
#endif

    /* Initialize Gtk+ thread */
    p_intf->p_sys->p_input = NULL;

    p_intf->p_sys->b_autoplayfile = 1;
    p_intf->p_sys->b_playing = 0;
    p_intf->p_sys->b_slider_free = 1;

    p_intf->pf_run = Run;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: destroy interface window
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_this;

    if( p_intf->p_sys->p_input )
    {
        vlc_object_release( p_intf->p_sys->p_input );
    }

#ifdef NEED_GTK2_MAIN
    msg_Dbg( p_intf, "Releasing gui-helper" );
    module_unneed( p_intf, p_intf->p_sys->p_gtk_main );
#endif

    /* Destroy structure */
    free( p_intf->p_sys );
}

/*****************************************************************************
 * Run: Gtk+ thread
 *****************************************************************************
 * this part of the interface is in a separate thread so that we can call
 * gtk_main() from within it without annoying the rest of the program.
 *****************************************************************************/
static void Run( intf_thread_t *p_intf )
{
#ifndef NEED_GTK2_MAIN
    /* gtk_init needs to know the command line. We don't care, so we
     * give it an empty one */
    char  *p_args[] = { "", NULL };
    char **pp_args  = p_args;
    int    i_args   = 1;
    int    i_dummy;
#endif
    playlist_t        *p_playlist;
    GtkCellRenderer   *p_renderer = NULL;
    GtkTreeViewColumn *p_column   = NULL;
    GtkListStore      *p_filelist = NULL;
    GtkListStore      *p_playlist_store = NULL;
    int canc = vlc_savecancel();

#ifndef NEED_GTK2_MAIN
    gtk_set_locale ();
    msg_Dbg( p_intf, "Starting pda GTK2+ interface" );
    gtk_init( &i_args, &pp_args );
#else
    /* Initialize Gtk+ */
    msg_Dbg( p_intf, "Starting pda GTK2+ interface thread" );
    gdk_threads_enter();
#endif

    /* Create some useful widgets that will certainly be used */
    add_pixmap_directory(config_GetDataDir());

    p_intf->p_sys->p_window = create_pda();
    if (p_intf->p_sys->p_window == NULL)
    {
        msg_Err( p_intf, "unable to create pda interface" );
    }

    /* Store p_intf to keep an eye on it */
    gtk_object_set_data( GTK_OBJECT(p_intf->p_sys->p_window),
                         "p_intf", p_intf );

    /* Set the title of the main window */
    gtk_window_set_title( GTK_WINDOW(p_intf->p_sys->p_window),
                          VOUT_TITLE " (PDA Linux interface)");

    /* Get the notebook object */
    p_intf->p_sys->p_notebook = GTK_NOTEBOOK( gtk_object_get_data(
        GTK_OBJECT( p_intf->p_sys->p_window ), "notebook" ) );

    /* Get the slider object */
    p_intf->p_sys->p_slider = (GtkHScale*) lookup_widget( p_intf->p_sys->p_window, "timeSlider" );
    p_intf->p_sys->p_slider_label = (GtkLabel*) lookup_widget( p_intf->p_sys->p_window, "timeLabel" );
    if (p_intf->p_sys->p_slider == NULL)
        msg_Err( p_intf, "Time slider widget not found." );
    if (p_intf->p_sys->p_slider_label == NULL)
        msg_Err( p_intf, "Time label widget not found." );

    /* Connect the date display to the slider */
    p_intf->p_sys->p_adj = gtk_range_get_adjustment( GTK_RANGE(p_intf->p_sys->p_slider) );
    if (p_intf->p_sys->p_adj == NULL)
        msg_Err( p_intf, "Adjustment range not found." );
    g_signal_connect( GTK_OBJECT( p_intf->p_sys->p_adj ), "value_changed",
                         G_CALLBACK( GtkDisplayDate ), p_intf );
    p_intf->p_sys->f_adj_oldvalue = 0;
    p_intf->p_sys->i_adj_oldvalue = 0;

    /* BEGIN OF FILEVIEW GTK_TREE_VIEW */
    p_intf->p_sys->p_tvfile = NULL;
    p_intf->p_sys->p_tvfile = (GtkTreeView *) lookup_widget( p_intf->p_sys->p_window,
                                                             "tvFileList");
    if (NULL == p_intf->p_sys->p_tvfile)
       msg_Err(p_intf, "Error obtaining pointer to File List");

    /* Insert columns 0 */
    p_renderer = gtk_cell_renderer_text_new ();
    gtk_tree_view_insert_column_with_attributes(p_intf->p_sys->p_tvfile, 0, (gchar *) N_("Filename"), p_renderer, NULL);
    p_column = gtk_tree_view_get_column(p_intf->p_sys->p_tvfile, 0 );
    gtk_tree_view_column_add_attribute(p_column, p_renderer, "text", 0 );
    gtk_tree_view_column_set_sort_column_id(p_column, 0);
    /* Insert columns 1 */
    p_renderer = gtk_cell_renderer_text_new ();
    gtk_tree_view_insert_column_with_attributes(p_intf->p_sys->p_tvfile, 1, (gchar *) N_("Permissions"), p_renderer, NULL);
    p_column = gtk_tree_view_get_column(p_intf->p_sys->p_tvfile, 1 );
    gtk_tree_view_column_add_attribute(p_column, p_renderer, "text", 1 );
    gtk_tree_view_column_set_sort_column_id(p_column, 1);
    /* Insert columns 2 */
    p_renderer = gtk_cell_renderer_text_new ();
    gtk_tree_view_insert_column_with_attributes(p_intf->p_sys->p_tvfile, 2, (gchar *) N_("Size"), p_renderer, NULL);
    p_column = gtk_tree_view_get_column(p_intf->p_sys->p_tvfile, 2 );
    gtk_tree_view_column_add_attribute(p_column, p_renderer, "text", 2 );
    gtk_tree_view_column_set_sort_column_id(p_column, 2);
    /* Insert columns 3 */
    p_renderer = gtk_cell_renderer_text_new ();
    gtk_tree_view_insert_column_with_attributes(p_intf->p_sys->p_tvfile, 3, (gchar *) N_("Owner"), p_renderer, NULL);
    p_column = gtk_tree_view_get_column(p_intf->p_sys->p_tvfile, 3 );
    gtk_tree_view_column_add_attribute(p_column, p_renderer, "text", 3 );
    gtk_tree_view_column_set_sort_column_id(p_column, 3);
    /* Insert columns 4 */
    p_renderer = gtk_cell_renderer_text_new ();
    gtk_tree_view_insert_column_with_attributes(p_intf->p_sys->p_tvfile, 4, (gchar *) N_("Group"), p_renderer, NULL);
    p_column = gtk_tree_view_get_column(p_intf->p_sys->p_tvfile, 4 );
    gtk_tree_view_column_add_attribute(p_column, p_renderer, "text", 4 );
    gtk_tree_view_column_set_sort_column_id(p_column, 4);

    /* Get new directory listing */
    p_filelist = gtk_list_store_new (5,
                G_TYPE_STRING, /* Filename */
                G_TYPE_STRING, /* permissions */
                G_TYPE_UINT64, /* File size */
                G_TYPE_STRING, /* Owner */
                G_TYPE_STRING);/* Group */
    ReadDirectory(p_intf, p_filelist, (char*)".");
    gtk_tree_view_set_model(GTK_TREE_VIEW(p_intf->p_sys->p_tvfile), GTK_TREE_MODEL(p_filelist));
    g_object_unref(p_filelist);     /* Model will be released by GtkTreeView */
    gtk_tree_selection_set_mode(gtk_tree_view_get_selection(GTK_TREE_VIEW(p_intf->p_sys->p_tvfile)),GTK_SELECTION_MULTIPLE);

    /* Column properties */
    gtk_tree_view_set_headers_visible(p_intf->p_sys->p_tvfile, TRUE);
    gtk_tree_view_columns_autosize(p_intf->p_sys->p_tvfile);
    gtk_tree_view_set_headers_clickable(GTK_TREE_VIEW(p_intf->p_sys->p_tvfile),TRUE);
    /* END OF FILEVIEW GTK_TREE_VIEW */

    /* BEGIN OF PLAYLIST GTK_TREE_VIEW */
    p_intf->p_sys->p_tvplaylist = NULL;
    p_intf->p_sys->p_tvplaylist = (GtkTreeView *) lookup_widget( p_intf->p_sys->p_window, "tvPlaylist");
    if (NULL == p_intf->p_sys->p_tvplaylist)
       msg_Err(p_intf, "Error obtaining pointer to Play List");

    /* Columns 1 */
    p_renderer = gtk_cell_renderer_text_new ();
    gtk_tree_view_insert_column_with_attributes(p_intf->p_sys->p_tvplaylist, 0, (gchar *) N_("Filename"), p_renderer, NULL);
    p_column = gtk_tree_view_get_column(p_intf->p_sys->p_tvplaylist, 0 );
    gtk_tree_view_column_add_attribute(p_column, p_renderer, "text", 0 );
    gtk_tree_view_column_set_sort_column_id(p_column, 0);
    /* Column 2 */
    p_renderer = gtk_cell_renderer_text_new ();
    gtk_tree_view_insert_column_with_attributes(p_intf->p_sys->p_tvplaylist, 1, (gchar *) N_("Time"), p_renderer, NULL);
    p_column = gtk_tree_view_get_column(p_intf->p_sys->p_tvplaylist, 1 );
    gtk_tree_view_column_add_attribute(p_column, p_renderer, "text", 1 );
    gtk_tree_view_column_set_sort_column_id(p_column, 1);
#if 0
    /* Column 3 - is a hidden column used for reliable deleting items from the underlying playlist */
    p_renderer = gtk_cell_renderer_text_new ();
    gtk_tree_view_insert_column_with_attributes(p_intf->p_sys->p_tvplaylist, 2, (gchar *) N_("Index"), p_renderer, NULL);
    p_column = gtk_tree_view_get_column(p_intf->p_sys->p_tvplaylist, 2 );
    gtk_tree_view_column_add_attribute(p_column, p_renderer, "text", 2 );
    gtk_tree_view_column_set_sort_column_id(p_column, 2);
#endif
    /* update the playlist */
    p_playlist = pl_Hold( p_intf );
    p_playlist_store = gtk_list_store_new (3,
                G_TYPE_STRING, /* Filename */
                G_TYPE_STRING, /* Time */
                G_TYPE_UINT);  /* Hidden index */
    PlaylistRebuildListStore(p_intf,p_playlist_store, p_playlist);
    gtk_tree_view_set_model(GTK_TREE_VIEW(p_intf->p_sys->p_tvplaylist), GTK_TREE_MODEL(p_playlist_store));
    g_object_unref(p_playlist_store);
    pl_Release( p_intf ); /* Free the playlist */
    gtk_tree_selection_set_mode(gtk_tree_view_get_selection(GTK_TREE_VIEW(p_intf->p_sys->p_tvplaylist)),GTK_SELECTION_MULTIPLE);

    /* Column properties */
    gtk_tree_view_set_headers_visible(p_intf->p_sys->p_tvplaylist, TRUE);
    gtk_tree_view_columns_autosize(p_intf->p_sys->p_tvplaylist);
    gtk_tree_view_set_headers_clickable(p_intf->p_sys->p_tvplaylist, TRUE);
    /* END OF PLAYLIST GTK_TREE_VIEW */

    /* Hide the Preference TAB for now. */
    GtkWidget *p_preference_tab = NULL;
    p_preference_tab = gtk_notebook_get_nth_page(p_intf->p_sys->p_notebook,5);
    if (p_preference_tab != NULL)
      gtk_widget_hide(p_preference_tab);

    /* Show the control window */
    gtk_widget_show( p_intf->p_sys->p_window );

#ifdef NEED_GTK2_MAIN
    msg_Dbg( p_intf, "Manage GTK keyboard events using threads" );
    while( vlc_object_alive( p_intf ) )
    {
        Manage( p_intf );

        /* Sleep to avoid using all CPU - since some interfaces need to
         * access keyboard events, a 100ms delay is a good compromise */
        gdk_threads_leave();
        if (vlc_CPU() & CPU_CAPABILITY_FPU)
            msleep( INTF_IDLE_SLEEP );
        else
            msleep( 1000 );
        gdk_threads_enter();
    }
#else
    msg_Dbg( p_intf, "Manage GTK keyboard events using timeouts" );
    /* Sleep to avoid using all CPU - since some interfaces needs to access
     * keyboard events, a 1000ms delay is a good compromise */
    if (vlc_CPU() & CPU_CAPABILITY_FPU)
        i_dummy = gtk_timeout_add( INTF_IDLE_SLEEP / 1000, (GtkFunction)Manage, p_intf );
    else
        i_dummy = gtk_timeout_add( 1000, (GtkFunction)Manage, p_intf );

    /* Enter Gtk mode */
    gtk_main();
    /* Remove the timeout */
    gtk_timeout_remove( i_dummy );
#endif

    gtk_object_destroy( GTK_OBJECT(p_intf->p_sys->p_window) );
#ifdef NEED_GTK2_MAIN
    gdk_threads_leave();
#endif
    vlc_restorecancel(canc);
}

/* following functions are local */

/*****************************************************************************
 * Manage: manage main thread messages
 *****************************************************************************
 * In this function, called approx. 10 times a second, we check what the
 * main program wanted to tell us.
 *****************************************************************************/
static int Manage( intf_thread_t *p_intf )
{
    GtkListStore *p_liststore;

    /* Update the input */
    if( p_intf->p_sys->p_input == NULL )
    {
        p_intf->p_sys->p_input = vlc_object_find( p_intf, VLC_OBJECT_INPUT,
                                                          FIND_ANYWHERE );
    }
    else if( p_intf->p_sys->p_input->b_dead )
    {
        vlc_object_release( p_intf->p_sys->p_input );
        p_intf->p_sys->p_input = NULL;
    }

    if( p_intf->p_sys->p_input )
    {
        input_thread_t *p_input = p_intf->p_sys->p_input;
        int64_t i_time = 0, i_length = 0;

        if( vlc_object_alive (p_input) )
        {
            playlist_t *p_playlist;

            GtkModeManage( p_intf );
            p_intf->p_sys->b_playing = 1;

            /* update playlist interface */
            p_playlist = pl_Hold( p_intf );
            if (p_playlist != NULL)
            {
                p_liststore = gtk_list_store_new (3,
                                            G_TYPE_STRING,
                                            G_TYPE_STRING,
                                            G_TYPE_UINT);  /* Hidden index */
                PlaylistRebuildListStore(p_intf, p_liststore, p_playlist);
                gtk_tree_view_set_model(p_intf->p_sys->p_tvplaylist, (GtkTreeModel*) p_liststore);
                g_object_unref(p_liststore);
                pl_Release( p_intf );
            }

            /* Manage the slider */
            i_time = var_GetTime( p_intf->p_sys->p_input, "time" );
            i_length = var_GetTime( p_intf->p_sys->p_input, "length" );

            if (vlc_CPU() & CPU_CAPABILITY_FPU)
            {
                /* Manage the slider for CPU_CAPABILITY_FPU hardware */
                if( p_intf->p_sys->b_playing )
                {
                    float newvalue = p_intf->p_sys->p_adj->value;

                    /* If the user hasn't touched the slider since the last time,
                     * then the input can safely change it */
                    if( newvalue == p_intf->p_sys->f_adj_oldvalue )
                    {
                        /* Update the value */
                        p_intf->p_sys->p_adj->value =
                        p_intf->p_sys->f_adj_oldvalue =
                            ( 100 * i_time ) / i_length;
                        g_signal_emit_by_name( GTK_OBJECT( p_intf->p_sys->p_adj ),
                                                 "value_changed" );
                    }
                    /* Otherwise, send message to the input if the user has
                     * finished dragging the slider */
                    else if( p_intf->p_sys->b_slider_free )
                    {
                        double f_pos = (double)newvalue / 100.0;

                        /* release the lock to be able to seek */
                        var_SetFloat( p_input, "position", f_pos );

                        /* Update the old value */
                        p_intf->p_sys->f_adj_oldvalue = newvalue;
                    }
                }
            }
            else
            {
                /* Manage the slider without CPU_CAPABILITY_FPU hardware */
                if( p_intf->p_sys->b_playing )
                {
                    off_t newvalue = p_intf->p_sys->p_adj->value;

                    /* If the user hasn't touched the slider since the last time,
                     * then the input can safely change it */
                    if( newvalue == p_intf->p_sys->i_adj_oldvalue )
                    {
                        /* Update the value */
                        p_intf->p_sys->p_adj->value =
                        p_intf->p_sys->i_adj_oldvalue =
                            ( 100 * i_time ) / i_length;
                        g_signal_emit_by_name( GTK_OBJECT( p_intf->p_sys->p_adj ),
                                                 "value_changed" );
                    }
                    /* Otherwise, send message to the input if the user has
                     * finished dragging the slider */
                    else if( p_intf->p_sys->b_slider_free )
                    {
                        double f_pos = (double)newvalue / 100.0;

                        /* release the lock to be able to seek */
                        var_SetFloat( p_input, "position", f_pos );

                        /* Update the old value */
                        p_intf->p_sys->i_adj_oldvalue = newvalue;
                    }
                }
            }
        }
    }
    else if( p_intf->p_sys->b_playing && vlc_object_alive( p_intf ) )
    {
        GtkModeManage( p_intf );
        p_intf->p_sys->b_playing = 0;
    }

#ifndef NEED_GTK2_MAIN
    if( !vlc_object_alive( p_intf ) )
    {
        /* Prepare to die, young Skywalker */
        gtk_main_quit();

        return FALSE;
    }
#endif

    return TRUE;
}

/*****************************************************************************
 * GtkDisplayDate: display stream date
 *****************************************************************************
 * This function displays the current date related to the position in
 * the stream. It is called whenever the slider changes its value.
 * The lock has to be taken before you call the function.
 *****************************************************************************/
void GtkDisplayDate( GtkAdjustment *p_adj, gpointer userdata )
{
    (void)p_adj;

    intf_thread_t *p_intf;

    p_intf = (intf_thread_t*) userdata;
    if (p_intf == NULL)
        return;

    if( p_intf->p_sys->p_input )
    {
        char psz_time[ MSTRTIME_MAX_SIZE ];
        int64_t i_seconds;

        i_seconds = var_GetTime( p_intf->p_sys->p_input, "time" ) / INT64_C(1000000 );
        secstotimestr( psz_time, i_seconds );

        gtk_label_set_text( GTK_LABEL( p_intf->p_sys->p_slider_label ),
                            psz_time );
     }
}

/*****************************************************************************
 * GtkModeManage: actualize the aspect of the interface whenever the input
 *                changes.
 *****************************************************************************
 * The lock has to be taken before you call the function.
 *****************************************************************************/
gint GtkModeManage( intf_thread_t * p_intf )
{
    GtkWidget *     p_slider = NULL;
    bool      b_control;

    if ( p_intf->p_sys->p_window == NULL )
        msg_Err( p_intf, "Main widget not found" );

    p_slider = lookup_widget( p_intf->p_sys->p_window, "timeSlider");
    if (p_slider == NULL)
        msg_Err( p_intf, "Slider widget not found" );

    /* controls unavailable */
    b_control = 0;

    /* show the box related to current input mode */
    if( p_intf->p_sys->p_input )
    {
        /* initialize and show slider for seekable streams */
        {
            gtk_widget_show( GTK_WIDGET( p_slider ) );
        }

        /* control buttons for free pace streams */
        b_control = var_GetBool( p_intf->p_sys->p_input, "can-rate" );

        msg_Dbg( p_intf, "stream has changed, refreshing interface" );
    }

    /* set control items */
    gtk_widget_set_sensitive( lookup_widget( p_intf->p_sys->p_window, "tbRewind"), b_control );
    gtk_widget_set_sensitive( lookup_widget( p_intf->p_sys->p_window, "tbPause"), b_control );
    gtk_widget_set_sensitive( lookup_widget( p_intf->p_sys->p_window, "tbForward"), b_control );
    return TRUE;
}
