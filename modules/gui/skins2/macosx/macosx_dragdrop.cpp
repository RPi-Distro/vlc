/*****************************************************************************
 * macosx_dragdrop.cpp
 *****************************************************************************
 * Copyright (C) 2003 the VideoLAN team
 * $Id: 966d16a5d20ec8234e34c15ddcf7f197744ec8d9 $
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

#ifdef MACOSX_SKINS

#include "macosx_dragdrop.hpp"


MacOSXDragDrop::MacOSXDragDrop( intf_thread_t *pIntf, bool playOnDrop ):
    SkinObject( pIntf ), m_playOnDrop( playOnDrop )
{
    // TODO
}


#endif
