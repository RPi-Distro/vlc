/*****************************************************************************
 * cmd_playlist.cpp
 *****************************************************************************
 * Copyright (C) 2003 the VideoLAN team
 * $Id$
 *
 * Authors: Cyril Deguet     <asmax@via.ecp.fr>
 *          Olivier Teulière <ipkiss@via.ecp.fr>
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

#include "cmd_playlist.hpp"
#include <vlc_playlist.h>
#include "../src/vlcproc.hpp"
#include "../utils/var_bool.hpp"

void CmdPlaylistDel::execute()
{
    m_rList.delSelected();
}

void CmdPlaylistNext::execute()
{
    playlist_t *pPlaylist = getIntf()->p_sys->p_playlist;
    if( pPlaylist != NULL )
    {
        playlist_Next( pPlaylist );
    }
}


void CmdPlaylistPrevious::execute()
{
    playlist_t *pPlaylist = getIntf()->p_sys->p_playlist;
    if( pPlaylist != NULL )
    {
        playlist_Prev( pPlaylist );
    }
}


void CmdPlaylistRandom::execute()
{
    playlist_t *pPlaylist = getIntf()->p_sys->p_playlist;
    if( pPlaylist != NULL )
    {
        vlc_value_t val;
        val.b_bool = m_value;
        var_Set( pPlaylist , "random", val);
    }
}


void CmdPlaylistLoop::execute()
{
    playlist_t *pPlaylist = getIntf()->p_sys->p_playlist;
    if( pPlaylist != NULL )
    {
        vlc_value_t val;
        val.b_bool = m_value;
        var_Set( pPlaylist , "loop", val);
    }
}


void CmdPlaylistRepeat::execute()
{
    playlist_t *pPlaylist = getIntf()->p_sys->p_playlist;
    if( pPlaylist != NULL )
    {
        vlc_value_t val;
        val.b_bool = m_value;
        var_Set( pPlaylist , "repeat", val);
    }
}


void CmdPlaylistLoad::execute()
{
    playlist_t *pPlaylist = getIntf()->p_sys->p_playlist;
    if( pPlaylist != NULL )
        playlist_Import( pPlaylist, m_file.c_str() );
}


void CmdPlaylistSave::execute()
{
    playlist_t *pPlaylist = getIntf()->p_sys->p_playlist;
    if( pPlaylist != NULL )
    {
        static const char psz_xspf[] = "export-xspf",
                          psz_m3u[] = "export-m3u",
                          psz_html[] = "export-html";
        const char *psz_module;
        if( m_file.find( ".xsp", 0 ) != string::npos )
            psz_module = psz_xspf;
        else if( m_file.find( "m3u", 0 ) != string::npos )
            psz_module = psz_m3u;
        else if( m_file.find( "html", 0 ) != string::npos )
            psz_module = psz_html;
        else
        {
            msg_Err( getIntf(), "Impossible to recognise the file type" );
            return;
        }

        playlist_Export( pPlaylist, m_file.c_str(), pPlaylist->p_local_category, psz_module );
    }
}
