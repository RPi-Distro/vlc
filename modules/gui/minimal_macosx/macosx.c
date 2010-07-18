/*****************************************************************************
 * macosx.m: minimal Mac OS X module for vlc
 *****************************************************************************
 * Copyright (C) 2001-2007 the VideoLAN team
 * $Id: d7c95ea408d72ae24407191818717e7b86fcffd4 $
 *
 * Authors: Colin Delacroix <colin@zoy.org>
 *          Eugenio Jarosiewicz <ej0@cise.ufl.edu>
 *          Florian G. Pflug <fgp@phlo.org>
 *          Jon Lech Johansen <jon-vl@nanocrew.net>
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
#include <stdlib.h>                                      /* malloc(), free() */
#include <string.h>

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>

/*****************************************************************************
 * External prototypes
 *****************************************************************************/
int  OpenIntf     ( vlc_object_t * );
void CloseIntf    ( vlc_object_t * );

int  OpenVideoGL  ( vlc_object_t * );
void CloseVideoGL ( vlc_object_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

vlc_module_begin ()
    /* Minimal interface. see intf.m */
    set_shortname( "Minimal Macosx" )
    add_shortcut( "minimal_macosx" )
    add_shortcut( "miosx" )
    set_description( N_("Minimal Mac OS X interface") )
    set_capability( "interface", 50 )
    set_callbacks( OpenIntf, CloseIntf )
    set_category( CAT_INTERFACE )
    set_subcategory( SUBCAT_INTERFACE_MAIN )

    add_submodule ()
        /* Will be loaded even without interface module. see voutgl.m */
        add_shortcut( "minimal_macosx" )
        add_shortcut( "miosx" )
        set_description( N_("Minimal Mac OS X OpenGL video output (opens a borderless window)") )
        set_capability( "opengl provider", 50 )
        set_category( CAT_VIDEO)
        set_subcategory( SUBCAT_VIDEO_VOUT )
        set_callbacks( OpenVideoGL, CloseVideoGL )
vlc_module_end ()

