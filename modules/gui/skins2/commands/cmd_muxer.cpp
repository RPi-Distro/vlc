/*****************************************************************************
 * cmd_muxer.cpp
 *****************************************************************************
 * Copyright (C) 2005 the VideoLAN team
 * $Id: 9b652ebe3c90dc50cea32a019d48053803bfa06b $
 *
 * Authors: Olivier Teulière <ipkiss@via.ecp.fr>
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

#include "cmd_muxer.hpp"


void CmdMuxer::execute()
{
    list<CmdGeneric*>::const_iterator it;
    for( it = m_list.begin(); it != m_list.end(); it++ )
    {
        (*it)->execute();
    }
}
