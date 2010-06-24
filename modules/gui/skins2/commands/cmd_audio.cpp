/*****************************************************************************
 * cmd_audio.cpp
 *****************************************************************************
 * Copyright (C) 2003 the VideoLAN team
 * $Id: 5f9a54a9af49e303dbeeed0d81e30b1250ff1840 $
 *
 * Authors: Cyril Deguet     <asmax@via.ecp.fr>
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

#include "cmd_audio.hpp"
#include "../src/vlcproc.hpp"
#include <vlc_playlist.h>
#include <vlc_input.h>
#include <vlc_aout.h>
#include <string>

void CmdSetEqualizer::execute()
{
    playlist_t* pPlaylist = getIntf()->p_sys->p_playlist;

    aout_EnableFilter( pPlaylist, "equalizer", m_enable );
    VlcProc::instance( getIntf() )->update_equalizer();
}


