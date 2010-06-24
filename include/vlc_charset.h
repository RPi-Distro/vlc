/*****************************************************************************
 * charset.h: Unicode UTF-8 wrappers function
 *****************************************************************************
 * Copyright (C) 2003-2005 the VideoLAN team
 * Copyright © 2005-2010 Rémi Denis-Courmont
 * $Id: aaa9d58a6526d891d5a8cd10524a7debd10b24dd $
 *
 * Author: Rémi Denis-Courmont <rem # videolan,org>
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

#ifndef VLC_CHARSET_H
#define VLC_CHARSET_H 1

/**
 * \file
 * This files handles locale conversions in vlc
 */

#include <stdarg.h>

VLC_EXPORT( void, LocaleFree, ( const char * ) );
VLC_EXPORT( char *, FromLocale, ( const char * ) LIBVLC_USED );
VLC_EXPORT( char *, FromLocaleDup, ( const char * ) LIBVLC_USED );
VLC_EXPORT( char *, ToLocale, ( const char * ) LIBVLC_USED );
VLC_EXPORT( char *, ToLocaleDup, ( const char * ) LIBVLC_USED );

VLC_EXPORT( int, utf8_vfprintf, ( FILE *stream, const char *fmt, va_list ap ) );
VLC_EXPORT( int, utf8_fprintf, ( FILE *, const char *, ... ) LIBVLC_FORMAT( 2, 3 ) );

VLC_EXPORT( char *, EnsureUTF8, ( char * ) );
VLC_EXPORT( const char *, IsUTF8, ( const char * ) LIBVLC_USED );

#ifdef WIN32
LIBVLC_USED
static inline char *FromWide (const wchar_t *wide)
{
    size_t len = WideCharToMultiByte (CP_UTF8, 0, wide, -1, NULL, 0, NULL, NULL);
    if (len == 0)
        return NULL;

    char *out = (char *)malloc (len);

    if (out)
        WideCharToMultiByte (CP_UTF8, 0, wide, -1, out, len, NULL, NULL);
    return out;
}
#endif

/**
 * Converts a nul-terminated string from ISO-8859-1 to UTF-8.
 */
static inline char *FromLatin1 (const char *latin)
{
    char *str = (char *)malloc (2 * strlen (latin) + 1), *utf8 = str;
    unsigned char c;

    if (str == NULL)
        return NULL;

    while ((c = *(latin++)) != '\0')
    {
         if (c >= 0x80)
         {
             *(utf8++) = 0xC0 | (c >> 6);
             *(utf8++) = 0x80 | (c & 0x3F);
         }
         else
             *(utf8++) = c;
    }
    *(utf8++) = '\0';

    utf8 = (char *)realloc (str, utf8 - str);
    return utf8 ? utf8 : str;
}

VLC_EXPORT( char *, FromCharset, ( const char *charset, const void *data, size_t data_size ) LIBVLC_USED );

VLC_EXPORT( double, us_strtod, ( const char *, char ** ) LIBVLC_USED );
VLC_EXPORT( float, us_strtof, ( const char *, char ** ) LIBVLC_USED );
VLC_EXPORT( double, us_atof, ( const char * ) LIBVLC_USED );
VLC_EXPORT( int, us_asprintf, ( char **, const char *, ... ) LIBVLC_USED );

#endif
