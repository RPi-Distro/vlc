/*****************************************************************************
 * intf.h: interface header for vlc
 *****************************************************************************
 * Copyright (C) 2002 the VideoLAN team
 * $Id: 58e29930c119790a84b1751dab022c6f3999b711 $
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

#ifndef _VLC_INTF_H
#define _VLC_INTF_H 1

# ifdef __cplusplus
extern "C" {
# endif

/*****************************************************************************
 * Required public headers
 *****************************************************************************/
#include <vlc/vlc.h>

/*****************************************************************************
 * Required internal headers
 *****************************************************************************/
#include "vlc_interface.h"
#include "vlc_es.h"
#include "vlc_input.h"
#include "intf_eject.h"
#include "vlc_playlist.h"

# ifdef __cplusplus
}
# endif

#endif /* <vlc/intf.h> */
