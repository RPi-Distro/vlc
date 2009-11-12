/*****************************************************************************
 * pda.h: private Gtk+ interface description
 *****************************************************************************
 * Copyright (C) 1999, 2000 the VideoLAN team
 * $Id$
 *
 * Authors: Jean-Paul Saman <jpsaman _at_ videolan _dot_ org>
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

#define MAX_ATEXIT                 10

/*****************************************************************************
 * intf_sys_t: description and status of Gtk+ interface
 *****************************************************************************/
struct intf_sys_t
{
    /* The gtk_main module */
    module_t           *p_gtk_main;

    /* windows and widgets */
    GtkWidget          *p_window;                             /* main window */
    GtkNotebook        *p_notebook;
    GtkHScale          *p_slider;
    GtkTreeView        *p_tvfile;
    GtkTreeView        *p_tvplaylist;

    /* slider */
    GtkLabel *          p_slider_label;
    GtkAdjustment *     p_adj;                   /* slider adjustment object */
    off_t               i_adj_oldvalue;  /* previous value -no FPU hardware  */
    float               f_adj_oldvalue;  /* previous value -with FPU hardware*/

    /* special actions */
    bool          b_playing;
    bool          b_window_changed;        /* window display toggled ? */
    bool          b_slider_free;                      /* slider status */

    /* Preference settings */
    bool          b_autoplayfile;

    /* The input thread */
    input_thread_t *    p_input;
};

/*****************************************************************************
 * Useful macro
 ****************************************************************************/
#define GTK_GET( type, nom ) GTK_##type( gtk_object_get_data( \
        GTK_OBJECT( p_intf->p_sys->p_window ), nom ) )

 
#define  GtkGetIntf( widget ) __GtkGetIntf( GTK_WIDGET( widget ) )
void * __GtkGetIntf( GtkWidget * );
