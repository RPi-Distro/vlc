/*****************************************************************************
 * vlc_url.h: URL related macros
 *****************************************************************************
 * Copyright (C) 2002-2006 the VideoLAN team
 * $Id: d7e40d06d53523df601723442becf604e79dcb0f $
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
 *          Rémi Denis-Courmont <rem # videolan.org>
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

#ifndef __VLC_URL_H
# define __VLC_URL_H

typedef struct
{
    char *psz_protocol;
    char *psz_username;
    char *psz_password;
    char *psz_host;
    int  i_port;

    char *psz_path;

    char *psz_option;

    char *psz_buffer; /* to be freed */
} vlc_url_t;

/*****************************************************************************
 * vlc_UrlParse:
 *****************************************************************************
 * option : if != 0 then path is split at this char
 *
 * format [protocol://[login[:password]@]][host[:port]]/path[OPTIONoption]
 *****************************************************************************/
static inline void vlc_UrlParse( vlc_url_t *url, const char *psz_url,
                                 char option )
{
    char *psz_dup;
    char *psz_parse;
    char *p;

    url->psz_protocol = NULL;
    url->psz_username = NULL;
    url->psz_password = NULL;
    url->psz_host     = NULL;
    url->i_port       = 0;
    url->psz_path     = NULL;
    url->psz_option   = NULL;

    if( psz_url == NULL )
    {
        url->psz_buffer = NULL;
        return;
    }
    url->psz_buffer = psz_parse = psz_dup = strdup( psz_url );

    p  = strstr( psz_parse, ":/" );
    if( p != NULL )
    {
        /* we have a protocol */

        /* skip :// */
        *p++ = '\0';
        if( p[1] == '/' )
            p += 2;
        url->psz_protocol = psz_parse;
        psz_parse = p;
    }
    p = strchr( psz_parse, '@' );
    if( p != NULL )
    {
        /* We have a login */
        url->psz_username = psz_parse;
        *p++ = '\0';

        psz_parse = strchr( psz_parse, ':' );
        if( psz_parse != NULL )
        {
            /* We have a password */
            *psz_parse++ = '\0';
            url->psz_password = psz_parse;
        }

        psz_parse = p;
    }

    p = strchr( psz_parse, '/' );
    if( !p || psz_parse < p )
    {
        char *p2;

        /* We have a host[:port] */
        url->psz_host = strdup( psz_parse );
        if( p )
        {
            url->psz_host[p - psz_parse] = '\0';
        }

        if( *url->psz_host == '[' )
        {
            /* Ipv6 address */
            p2 = strchr( url->psz_host, ']' );
            if( p2 )
            {
                p2 = strchr( p2, ':' );
            }
        }
        else
        {
            p2 = strchr( url->psz_host, ':' );
        }
        if( p2 )
        {
            *p2++ = '\0';
            url->i_port = atoi( p2 );
        }
    }
    psz_parse = p;

    /* Now parse psz_path and psz_option */
    if( psz_parse )
    {
        url->psz_path = psz_parse;
        if( option != '\0' )
        {
            p = strchr( url->psz_path, option );
            if( p )
            {
                *p++ = '\0';
                url->psz_option = p;
            }
        }
    }
}

/*****************************************************************************
 * vlc_UrlClean:
 *****************************************************************************
 *
 *****************************************************************************/
static inline void vlc_UrlClean( vlc_url_t *url )
{
    if( url->psz_buffer ) free( url->psz_buffer );
    if( url->psz_host )   free( url->psz_host );

    url->psz_protocol = NULL;
    url->psz_username = NULL;
    url->psz_password = NULL;
    url->psz_host     = NULL;
    url->i_port       = 0;
    url->psz_path     = NULL;
    url->psz_option   = NULL;

    url->psz_buffer   = NULL;
}

VLC_EXPORT( char *, unescape_URI_duplicate, ( const char *psz ) );
VLC_EXPORT( void, unescape_URI, ( char *psz ) );
VLC_EXPORT( char *, decode_URI_duplicate, ( const char *psz ) );
VLC_EXPORT( void, decode_URI, ( char *psz ) );
VLC_EXPORT( char *, encode_URI_component, ( const char *psz ) );

static inline char *vlc_UrlEncode( const char *psz_url )
{
    /* FIXME: do not encode / : ? and & _when_ not needed */
    return encode_URI_component( psz_url );
}

/*****************************************************************************
 * vlc_UrlIsNotEncoded:
 *****************************************************************************
 * check if given string is not a valid URL and must hence be encoded
 *****************************************************************************/
#include <ctype.h>

static inline int vlc_UrlIsNotEncoded( const char *psz_url )
{
    const char *ptr;

    for( ptr = psz_url; *ptr; ptr++ )
    {
        char c = *ptr;

        if( c == '%' )
        {
            if( !isxdigit( ptr[1] ) || !isxdigit( ptr[2] ) )
                return 1; /* not encoded */
            ptr += 2;
        }
        else
        if(  ( (unsigned char)( c - 'a' ) < 26 )
          || ( (unsigned char)( c - 'A' ) < 26 )
          || ( (unsigned char)( c - '0' ) < 10 )
          || ( strchr( "-_.", c ) != NULL ) )
            return 1;
    }
    return 0; /* looks fine - but maybe it is not encoded */
}

/*****************************************************************************
 * vlc_b64_encode:
 *****************************************************************************
 *
 *****************************************************************************/
static inline char *vlc_b64_encode( const char *src )
{
    static const char b64[] =
           "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    size_t len = strlen( src );
    const uint8_t *in = (const uint8_t *)src;

    char *ret;
    char *dst = (char *)malloc( ( len + 4 ) * 4 / 3 );
    if( dst == NULL )
        return NULL;

    ret = dst;

    while( len > 0 )
    {
        /* pops (up to) 3 bytes of input, push 4 bytes */
        uint32_t v = *in++ << 24; // 1/3
        *dst++ = b64[v >> 26]; // 1/4
        v = v << 6;

        if( len >= 2 )
            v |= *in++ << 22; // 2/3
        *dst++ = b64[v >> 26]; // 2/4
        v = v << 6;

        if( len >= 3 )
            v |= *in++ << 20; // 3/3
        *dst++ = ( len >= 2 ) ? b64[v >> 26] : '='; // 3/4
        v = v << 6;

        *dst++ = ( len >= 3 ) ? b64[v >> 26] : '='; // 4/4

        len--;
        if( len > 0 )
        {
            len--;
            if( len > 0 )
                len--;
        }
    }

    *dst = '\0';

    return ret;
}
#endif
