/*****************************************************************************
 * dynamicoverlay_buffer.h : dynamic overlay buffer
 *****************************************************************************
 * Copyright (C) 2008 the VideoLAN team
 * $Id: e24518ad4a1fa349b8c256d2c2d9aad6c0b99fc8 $
 *
 * Author: SÃ¸ren BÃ¸g <avacore@videolan.org>
 *         Jean-Paul Saman <jpsaman@videolan.org>
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_osd.h>
#include <vlc_filter.h>

#include <ctype.h>

#include "dynamicoverlay.h"

/*****************************************************************************
 * buffer_t: Command and response buffer
 *****************************************************************************/

int BufferInit( buffer_t *p_buffer )
{
    memset( p_buffer, 0, sizeof( buffer_t ) );
    p_buffer->p_memory = NULL;
    p_buffer->p_begin = NULL;

    return VLC_SUCCESS;
}

int BufferDestroy( buffer_t *p_buffer )
{
    if( p_buffer->p_memory != NULL )
    {
        free( p_buffer->p_memory );
    }
    p_buffer->p_memory = NULL;
    p_buffer->p_begin = NULL;

    return VLC_SUCCESS;
}

char *BufferGetToken( buffer_t *p_buffer )
{
    char *p_char = p_buffer->p_begin;

    while( isspace( p_char[0] ) || p_char[0] == '\0' )
    {
        if( p_char <= (p_buffer->p_begin + p_buffer->i_length) )
            p_char++;
        else
            return NULL;
    }
    return p_char;
}

int BufferAdd( buffer_t *p_buffer, const char *p_data, size_t i_len )
{
    if( ( p_buffer->i_size - p_buffer->i_length -
          ( p_buffer->p_begin - p_buffer->p_memory ) ) < i_len )
    {
        /* We'll have to do some rearranging to fit the new data. */
        if( ( p_buffer->i_size - p_buffer->i_length ) >= i_len )
        {
            /* We have room in the current buffer, just need to move it */
            memmove( p_buffer->p_memory, p_buffer->p_begin,
                     p_buffer->i_length );
            p_buffer->p_begin = p_buffer->p_memory;
        }
        else
        {
            // We need a bigger buffer
            size_t i_newsize = 1024;
            while( i_newsize < p_buffer->i_length + i_len )
                i_newsize *= 2;
            /* TODO: Should I handle wrapping here? */

            /* I'm not using realloc here, as I can avoid a memcpy/memmove in
               some (most?) cases, and reset the start of the buffer. */
            char *p_newdata = malloc( i_newsize );
            if( p_newdata == NULL )
                return VLC_ENOMEM;
            if( p_buffer->p_begin != NULL )
            {
                memcpy( p_newdata, p_buffer->p_begin, p_buffer->i_length );
                free( p_buffer->p_memory );
            }
            p_buffer->p_memory = p_buffer->p_begin = p_newdata;
            p_buffer->i_size = i_newsize;
        }
    }

    /* Add the new data to the end of the current */
    memcpy( p_buffer->p_begin + p_buffer->i_length, p_data, i_len );
    p_buffer->i_length += i_len;
    return VLC_SUCCESS;
}

int BufferPrintf( buffer_t *p_buffer, const char *p_fmt, ... )
{
    int i_len;
    int status;
    char *psz_data;

    va_list va_list1, va_list2;
    va_start( va_list1, p_fmt );
    va_copy( va_list2, va_list1 );

    i_len = vsnprintf( NULL, 0, p_fmt, va_list1 );
    if( i_len < 0 )
        return VLC_EGENERIC;
    va_end( va_list1 );

    psz_data = malloc( i_len + 1 );
    if( psz_data == NULL ) {
        return VLC_ENOMEM;
    }
    if( vsnprintf( psz_data, i_len + 1, p_fmt, va_list2 ) != i_len )
    {
        return VLC_EGENERIC;
    }
    va_end( va_list2 );
    status = BufferAdd( p_buffer, psz_data, i_len );
    free( psz_data );
    return status;
}

int BufferDel( buffer_t *p_buffer, int i_len )
{
    p_buffer->i_length -= i_len;
    if( p_buffer->i_length == 0 )
    {
        /* No data, we can reset the buffer now. */
        p_buffer->p_begin = p_buffer->p_memory;
    }
    else
    {
        p_buffer->p_begin += i_len;
    }
    return VLC_SUCCESS;
}
