/*****************************************************************************
 * ifo.c: Dummy ifo demux to enable opening DVDs rips by double cliking on VIDEO_TS.IFO
 *****************************************************************************
 * Copyright (C) 2007 the VideoLAN team
 * $Id: 54d29db9dd48790dcde3259038a19b0f0a403eec $
 *
 * Authors: Antoine Cellerier <dionoea @t videolan d.t org>
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
#include <vlc_demux.h>

#include "playlist.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int Demux( demux_t *p_demux);
static int DemuxDVD_VR( demux_t *p_demux);
static int Control( demux_t *p_demux, int i_query, va_list args );

/*****************************************************************************
 * Import_IFO: main import function
 *****************************************************************************/
int Import_IFO( vlc_object_t *p_this )
{
    demux_t *p_demux = (demux_t *)p_this;

    size_t len = strlen( p_demux->psz_path );

    char *psz_file = p_demux->psz_path + len - strlen( "VIDEO_TS.IFO" );
    /* Valid filenames are :
     *  - VIDEO_TS.IFO
     *  - VTS_XX_X.IFO where X are digits
     */
    if( len > strlen( "VIDEO_TS.IFO" )
        && ( !strcasecmp( psz_file, "VIDEO_TS.IFO" )
        || (!strncasecmp( psz_file, "VTS_", 4 )
        && !strcasecmp( psz_file + strlen( "VTS_00_0" ) , ".IFO" ) ) ) )
    {
        int i_peek;
        const uint8_t *p_peek;
        i_peek = stream_Peek( p_demux->s, &p_peek, 8 );

        if( i_peek != 8 || memcmp( p_peek, "DVDVIDEO", 8 ) )
            return VLC_EGENERIC;

        p_demux->pf_demux = Demux;
    }
    /* Valid filename for DVD-VR is VR_MANGR.IFO */
    else if( len >= 12 && !strcmp( &p_demux->psz_path[len-12], "VR_MANGR.IFO" ) )
    {
        int i_peek;
        const uint8_t *p_peek;
        i_peek = stream_Peek( p_demux->s, &p_peek, 8 );

        if( i_peek != 8 || memcmp( p_peek, "DVD_RTR_", 8 ) )
            return VLC_EGENERIC;

        p_demux->pf_demux = DemuxDVD_VR;
    }
    else
        return VLC_EGENERIC;

//    STANDARD_DEMUX_INIT_MSG( "found valid VIDEO_TS.IFO" )
    p_demux->pf_control = Control;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Deactivate: frees unused data
 *****************************************************************************/
void Close_IFO( vlc_object_t *p_this )
{
    VLC_UNUSED(p_this);
}

static int Demux( demux_t *p_demux )
{
    size_t len = strlen( "dvd://" ) + strlen( p_demux->psz_path )
               - strlen( "VIDEO_TS.IFO" );
    char *psz_url;

    psz_url = malloc( len+1 );
    if( !psz_url )
        return 0;
    snprintf( psz_url, len+1, "dvd://%s", p_demux->psz_path );

    input_item_t *p_current_input = GetCurrentItem(p_demux);
    input_item_t *p_input = input_item_New( p_demux, psz_url, psz_url );
    input_item_PostSubItem( p_current_input, p_input );
    vlc_gc_decref( p_input );

    vlc_gc_decref(p_current_input);
    free( psz_url );

    return 0; /* Needed for correct operation of go back */
}

static int DemuxDVD_VR( demux_t *p_demux )
{
    char *psz_url = strdup( p_demux->psz_path );

    if( !psz_url )
        return 0;

    size_t len = strlen( psz_url );

    strncpy( psz_url + len - 12, "VR_MOVIE.VRO", 12 );

    input_item_t *p_current_input = GetCurrentItem(p_demux);
    input_item_t *p_input = input_item_New( p_demux, psz_url, psz_url );
    input_item_PostSubItem( p_current_input, p_input );

    vlc_gc_decref( p_input );

    vlc_gc_decref(p_current_input);
    free( psz_url );

    return 0; /* Needed for correct operation of go back */
}


static int Control( demux_t *p_demux, int i_query, va_list args )
{
    VLC_UNUSED(p_demux); VLC_UNUSED(i_query); VLC_UNUSED(args);
    return VLC_EGENERIC;
}
