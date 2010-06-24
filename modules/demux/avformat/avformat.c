/*****************************************************************************
 * avformat.c: demuxer and muxer using libavformat library
 *****************************************************************************
 * Copyright (C) 1999-2008 the VideoLAN team
 * $Id: b5974a42670d43768cdbba2d451be90303ee0f9c $
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Gildas Bazin <gbazin@videolan.org>
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

#ifndef MERGE_FFMPEG
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>

#include "avformat.h"

vlc_module_begin ()
#endif /* MERGE_FFMPEG */
    add_shortcut( "ffmpeg" )
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_DEMUX )
    set_description( N_("FFmpeg demuxer" ) )
    set_shortname( N_("Avformat") )
    set_capability( "demux", 2 )
    set_callbacks( OpenDemux, CloseDemux )

#ifdef ENABLE_SOUT
    /* mux submodule */
    add_submodule ()
    add_shortcut( "ffmpeg" )
    set_description( N_("FFmpeg muxer" ) )
    set_capability( "sout mux", 2 )
    add_string( "ffmpeg-mux", NULL, NULL, MUX_TEXT,
                MUX_LONGTEXT, true )
    set_callbacks( OpenMux, CloseMux )
#endif
#ifndef MERGE_FFMPEG
vlc_module_end ()
#endif
