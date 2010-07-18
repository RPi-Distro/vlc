/*****************************************************************************
 * decoder.h: header for vlc decoders
 *****************************************************************************
 * Copyright (C) 2002 the VideoLAN team
 * $Id: 3e94b04e7abbc57a64a010f6f5b859e9a07cc27a $
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

#ifndef _VLC_DECODER_H
#define _VLC_DECODER_H 1

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
#include "vlc_block.h"
#include "vlc_video.h"
#include "audio_output.h"
#include "vlc_codec.h"

# ifdef __cplusplus
}
# endif

#endif /* <vlc/decoder.h> */
