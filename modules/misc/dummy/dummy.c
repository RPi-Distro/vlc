/*****************************************************************************
 * dummy.c : dummy plugin for vlc
 *****************************************************************************
 * Copyright (C) 2000, 2001 the VideoLAN team
 * $Id: 3f61dd9ab9b02ae934c3ee96ebfa01451865f6a9 $
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
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
#include <vlc_plugin.h>

#include "dummy.h"

static int OpenDummy(vlc_object_t *);

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define CHROMA_TEXT N_("Dummy image chroma format")
#define CHROMA_LONGTEXT N_( \
    "Force the dummy video output to create images using a specific chroma " \
    "format instead of trying to improve performances by using the most " \
    "efficient one.")

#define SAVE_TEXT N_("Save raw codec data")
#define SAVE_LONGTEXT N_( \
    "Save the raw codec data if you have selected/forced the dummy " \
    "decoder in the main options." )

#ifdef WIN32
#define QUIET_TEXT N_("Do not open a DOS command box interface")
#define QUIET_LONGTEXT N_( \
    "By default the dummy interface plugin will start a DOS command box. " \
    "Enabling the quiet mode will not bring this command box but can also " \
    "be pretty annoying when you want to stop VLC and no video window is " \
    "open." )
#endif

vlc_module_begin ()
    set_shortname( N_("Dummy"))
    set_description( N_("Dummy interface function") )
    set_capability( "interface", 0 )
    set_callbacks( OpenIntf, NULL )
#ifdef WIN32
    set_section( N_( "Dummy Interface" ), NULL )
    add_category_hint( N_("Interface"), NULL, false )
    add_bool( "dummy-quiet", false, NULL, QUIET_TEXT, QUIET_LONGTEXT, false )
#endif
    add_submodule ()
        set_description( N_("Dummy demux function") )
        set_capability( "access_demux", 0 )
        set_callbacks( OpenDemux, CloseDemux )
        add_shortcut( "vlc" )
    add_submodule ()
        set_section( N_( "Dummy decoder" ), NULL )
        set_description( N_("Dummy decoder function") )
        set_capability( "decoder", 0 )
        set_callbacks( OpenDecoder, CloseDecoder )
        set_category( CAT_INPUT )
        set_subcategory( SUBCAT_INPUT_SCODEC )
        add_bool( "dummy-save-es", false, NULL, SAVE_TEXT, SAVE_LONGTEXT, true )
    add_submodule ()
        set_section( N_( "Dump decoder" ), NULL )
        set_description( N_("Dump decoder function") )
        set_capability( "decoder", -1 )
        set_callbacks( OpenDecoderDump, CloseDecoder )
        add_shortcut( "dump" )
    add_submodule ()
        set_description( N_("Dummy encoder function") )
        set_capability( "encoder", 0 )
        set_callbacks( OpenEncoder, CloseEncoder )
    add_submodule ()
        set_description( N_("Dummy audio output function") )
        set_capability( "audio output", 1 )
        set_callbacks( OpenAudio, NULL )
    add_submodule ()
        set_description( N_("Dummy video output function") )
        set_section( N_( "Dummy Video output" ), NULL )
        set_capability( "vout display", 1 )
        set_callbacks( OpenVideo, CloseVideo )
        set_category( CAT_VIDEO )
        set_subcategory( SUBCAT_VIDEO_VOUT )
        add_category_hint( N_("Video"), NULL, false )
        add_string( "dummy-chroma", NULL, NULL, CHROMA_TEXT, CHROMA_LONGTEXT, true )
    add_submodule ()
        set_section( N_( "Stats video output" ), NULL )
        set_description( N_("Stats video output function") )
        set_capability( "vout display", 0 )
        add_shortcut( "stats" )
        set_callbacks( OpenVideoStat, CloseVideo )
    add_submodule ()
        set_description( N_("Dummy font renderer function") )
        set_capability( "text renderer", 1 )
        set_callbacks( OpenRenderer, NULL )
    add_submodule ()
        set_description( N_("libc memcpy") )
        set_capability( "memcpy", 50 )
        set_callbacks( OpenDummy, NULL )
        add_shortcut( "c" )
        add_shortcut( "libc" )
vlc_module_end ()

static int OpenDummy( vlc_object_t *obj )
{
    (void) obj;
    return VLC_SUCCESS;
}
