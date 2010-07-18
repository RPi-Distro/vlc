/*****************************************************************************
 * export.c :  Playlist export module
 *****************************************************************************
 * Copyright (C) 2004 the VideoLAN team
 * $Id: af49869575f622687e1f3efb070850058168cb57 $
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
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
#include <vlc/vlc.h>

/***************************************************************************
 * Prototypes
 ***************************************************************************/
int Export_M3U    ( vlc_object_t *p_intf );
int Export_Old    ( vlc_object_t *p_intf );
int E_(xspf_export_playlist)( vlc_object_t *p_intf );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();

    set_category( CAT_PLAYLIST );
    set_subcategory( SUBCAT_PLAYLIST_EXPORT );
    add_submodule();
        set_description( _("M3U playlist exporter") );
        add_shortcut( "export-m3u" );
        set_capability( "playlist export" , 0);
        set_callbacks( Export_M3U , NULL );

    add_submodule();
        set_description( _("Old playlist exporter") );
        add_shortcut( "export-old" );
        set_capability( "playlist export" , 0);
        set_callbacks( Export_Old , NULL );

    add_submodule();
        set_description( _("XSPF playlist export") );
        add_shortcut( "export-xspf" );
        set_capability( "playlist export" , 0);
        set_callbacks( E_(xspf_export_playlist) , NULL );

vlc_module_end();
