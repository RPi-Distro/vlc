/*****************************************************************************
 * dummy.c
 *****************************************************************************
 * Copyright (C) 2001, 2002 the VideoLAN team
 * $Id: dfc193e01e11050279239014713f2cfc779834e7 $
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Eric Petit <titer@videolan.org>
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
#include <stdlib.h>

#include <vlc/vlc.h>
#include <vlc/sout.h>

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

vlc_module_begin();
    set_description( _("Dummy stream output") );
    set_shortname( _( "Dummy" ));
    set_capability( "sout access", 0 );
    set_category( CAT_SOUT );
    set_subcategory( SUBCAT_SOUT_ACO );
    add_shortcut( "dummy" );
    set_callbacks( Open, Close );
vlc_module_end();


/*****************************************************************************
 * Exported prototypes
 *****************************************************************************/
static int     Write( sout_access_out_t *, block_t * );
static int     Seek ( sout_access_out_t *, off_t  );

/*****************************************************************************
 * Open: open the file
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    sout_access_out_t   *p_access = (sout_access_out_t*)p_this;

    p_access->p_sys    = NULL;
    p_access->pf_write = Write;
    p_access->pf_seek  = Seek;

    msg_Dbg( p_access, "dummy stream output access opened" );
    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: close the target
 *****************************************************************************/
static void Close( vlc_object_t * p_this )
{
    sout_access_out_t   *p_access = (sout_access_out_t*)p_this;
    msg_Dbg( p_access, "dummy stream output access closed" );
}

/*****************************************************************************
 * Read: standard read on a file descriptor.
 *****************************************************************************/
static int Write( sout_access_out_t *p_access, block_t *p_buffer )
{
    int64_t i_write = 0;
    block_t *b = p_buffer;

    while( b )
    {
        i_write += b->i_buffer;

        b = b->p_next;
    }

    block_ChainRelease( p_buffer );

    return i_write;
}

/*****************************************************************************
 * Seek: seek to a specific location in a file
 *****************************************************************************/
static int Seek( sout_access_out_t *p_access, off_t i_pos )
{
    return 0;
}


