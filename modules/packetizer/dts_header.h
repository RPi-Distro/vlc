/*****************************************************************************
 * dts_header.c: parse DTS audio headers info
 *****************************************************************************
 * Copyright (C) 2004-2016 VLC authors and VideoLAN
 * $Id: 6787161cd90c398da0486907412bb574c1d390cc $
 *
 * Authors: Gildas Bazin <gbazin@netcourrier.com>
 *          Laurent Aimar
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#define VLC_DTS_HEADER_SIZE 14

#define PROFILE_DTS_INVALID -1
#define PROFILE_DTS 0
#define PROFILE_DTS_HD 1

typedef struct
{
    bool            b_substream;
    unsigned int    i_rate;
    unsigned int    i_bitrate;
    unsigned int    i_frame_size;
    unsigned int    i_frame_length;
    uint16_t        i_physical_channels;
    uint16_t        i_chan_mode;
} vlc_dts_header_t;

int     vlc_dts_header_Parse( vlc_dts_header_t *p_header,
                              const void *p_buffer, size_t i_buffer);

bool    vlc_dts_header_IsSync( const void *p_buffer, size_t i_buffer );
