/*****************************************************************************
 * strings.c: String related functions
 *****************************************************************************
 * Copyright (C) 2006 VLC authors and VideoLAN
 * Copyright (C) 2008-2009 Rémi Denis-Courmont
 * $Id: 5e057ca65e5dc38c14a660fb155a9325b436917c $
 *
 * Authors: Antoine Cellerier <dionoea at videolan dot org>
 *          Daniel Stranger <vlc at schmaller dot de>
 *          Rémi Denis-Courmont <rem # videolan org>
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <assert.h>

/* Needed by str_format_time */
#include <time.h>
#include <limits.h>

/* Needed by str_format_meta */
#include <vlc_input.h>
#include <vlc_meta.h>
#include <vlc_playlist.h>
#include <vlc_aout_intf.h>

#include <vlc_strings.h>
#include <vlc_url.h>
#include <vlc_charset.h>
#include <vlc_fs.h>
#include <libvlc.h>
#include <errno.h>

/**
 * Decode encoded URI component. See also decode_URI().
 * \return decoded duplicated string
 */
char *decode_URI_duplicate( const char *psz )
{
    char *psz_dup = strdup( psz );
    decode_URI( psz_dup );
    return psz_dup;
}

/**
 * Decode an encoded URI component in place.
 * <b>This function does NOT decode entire URIs.</b>
 * It decodes components (e.g. host name, directory, file name).
 * Decoded URIs do not exist in the real world (see RFC3986 §2.4).
 * Complete URIs are always "encoded" (or they are syntaxically invalid).
 *
 * Note that URI encoding is different from Javascript escaping. Especially,
 * white spaces and Unicode non-ASCII code points are encoded differently.
 *
 * \return psz on success, NULL if it was not properly encoded
 */
char *decode_URI( char *psz )
{
    unsigned char *in = (unsigned char *)psz, *out = in, c;

    if( psz == NULL )
        return NULL;

    while( ( c = *in++ ) != '\0' )
    {
        switch( c )
        {
            case '%':
            {
                char hex[3];

                if( ( ( hex[0] = *in++ ) == 0 )
                 || ( ( hex[1] = *in++ ) == 0 ) )
                    return NULL;

                hex[2] = '\0';
                *out++ = (unsigned char)strtoul( hex, NULL, 0x10 );
                break;
            }

            default:
                /* Inserting non-ASCII or non-printable characters is unsafe,
                 * and no sane browser will send these unencoded */
                if( ( c < 32 ) || ( c > 127 ) )
                    *out++ = '?';
                else
                    *out++ = c;
        }
    }
    *out = '\0';
    return psz;
}

static inline bool isurisafe( int c )
{
    /* These are the _unreserved_ URI characters (RFC3986 §2.3) */
    return ( (unsigned char)( c - 'a' ) < 26 )
            || ( (unsigned char)( c - 'A' ) < 26 )
            || ( (unsigned char)( c - '0' ) < 10 )
            || ( strchr( "-._~", c ) != NULL );
}

static char *encode_URI_bytes (const char *psz_uri, size_t len)
{
    char *psz_enc = malloc (3 * len + 1), *out = psz_enc;
    if (psz_enc == NULL)
        return NULL;

    for (size_t i = 0; i < len; i++)
    {
        static const char hex[16] = "0123456789ABCDEF";
        uint8_t c = *psz_uri;

        if( isurisafe( c ) )
            *out++ = c;
        /* This is URI encoding, not HTTP forms:
         * Space is encoded as '%20', not '+'. */
        else
        {
            *out++ = '%';
            *out++ = hex[c >> 4];
            *out++ = hex[c & 0xf];
        }
        psz_uri++;
    }
    *out++ = '\0';

    out = realloc (psz_enc, out - psz_enc);
    return out ? out : psz_enc; /* realloc() can fail (safe) */
}

/**
 * Encodes a URI component (RFC3986 §2).
 *
 * @param psz_uri nul-terminated UTF-8 representation of the component.
 * Obviously, you can't pass a URI containing a nul character, but you don't
 * want to do that, do you?
 *
 * @return encoded string (must be free()'d), or NULL for ENOMEM.
 */
char *encode_URI_component( const char *psz_uri )
{
    return encode_URI_bytes (psz_uri, strlen (psz_uri));
}


static const struct xml_entity_s
{
    char    psz_entity[8];
    char    psz_char[4];
} xml_entities[] = {
    /* Important: this list has to be in alphabetical order (psz_entity-wise) */
    { "AElig;",  "Æ" },
    { "Aacute;", "Á" },
    { "Acirc;",  "Â" },
    { "Agrave;", "À" },
    { "Aring;",  "Å" },
    { "Atilde;", "Ã" },
    { "Auml;",   "Ä" },
    { "Ccedil;", "Ç" },
    { "Dagger;", "‡" },
    { "ETH;",    "Ð" },
    { "Eacute;", "É" },
    { "Ecirc;",  "Ê" },
    { "Egrave;", "È" },
    { "Euml;",   "Ë" },
    { "Iacute;", "Í" },
    { "Icirc;",  "Î" },
    { "Igrave;", "Ì" },
    { "Iuml;",   "Ï" },
    { "Ntilde;", "Ñ" },
    { "OElig;",  "Œ" },
    { "Oacute;", "Ó" },
    { "Ocirc;",  "Ô" },
    { "Ograve;", "Ò" },
    { "Oslash;", "Ø" },
    { "Otilde;", "Õ" },
    { "Ouml;",   "Ö" },
    { "Scaron;", "Š" },
    { "THORN;",  "Þ" },
    { "Uacute;", "Ú" },
    { "Ucirc;",  "Û" },
    { "Ugrave;", "Ù" },
    { "Uuml;",   "Ü" },
    { "Yacute;", "Ý" },
    { "Yuml;",   "Ÿ" },
    { "aacute;", "á" },
    { "acirc;",  "â" },
    { "acute;",  "´" },
    { "aelig;",  "æ" },
    { "agrave;", "à" },
    { "amp;",    "&" },
    { "apos;",   "'" },
    { "aring;",  "å" },
    { "atilde;", "ã" },
    { "auml;",   "ä" },
    { "bdquo;",  "„" },
    { "brvbar;", "¦" },
    { "ccedil;", "ç" },
    { "cedil;",  "¸" },
    { "cent;",   "¢" },
    { "circ;",   "ˆ" },
    { "copy;",   "©" },
    { "curren;", "¤" },
    { "dagger;", "†" },
    { "deg;",    "°" },
    { "divide;", "÷" },
    { "eacute;", "é" },
    { "ecirc;",  "ê" },
    { "egrave;", "è" },
    { "eth;",    "ð" },
    { "euml;",   "ë" },
    { "euro;",   "€" },
    { "frac12;", "½" },
    { "frac14;", "¼" },
    { "frac34;", "¾" },
    { "gt;",     ">" },
    { "hellip;", "…" },
    { "iacute;", "í" },
    { "icirc;",  "î" },
    { "iexcl;",  "¡" },
    { "igrave;", "ì" },
    { "iquest;", "¿" },
    { "iuml;",   "ï" },
    { "laquo;",  "«" },
    { "ldquo;",  "“" },
    { "lsaquo;", "‹" },
    { "lsquo;",  "‘" },
    { "lt;",     "<" },
    { "macr;",   "¯" },
    { "mdash;",  "—" },
    { "micro;",  "µ" },
    { "middot;", "·" },
    { "nbsp;",   "\xc2\xa0" },
    { "ndash;",  "–" },
    { "not;",    "¬" },
    { "ntilde;", "ñ" },
    { "oacute;", "ó" },
    { "ocirc;",  "ô" },
    { "oelig;",  "œ" },
    { "ograve;", "ò" },
    { "ordf;",   "ª" },
    { "ordm;",   "º" },
    { "oslash;", "ø" },
    { "otilde;", "õ" },
    { "ouml;",   "ö" },
    { "para;",   "¶" },
    { "permil;", "‰" },
    { "plusmn;", "±" },
    { "pound;",  "£" },
    { "quot;",   "\"" },
    { "raquo;",  "»" },
    { "rdquo;",  "”" },
    { "reg;",    "®" },
    { "rsaquo;", "›" },
    { "rsquo;",  "’" },
    { "sbquo;",  "‚" },
    { "scaron;", "š" },
    { "sect;",   "§" },
    { "shy;",    "­" },
    { "sup1;",   "¹" },
    { "sup2;",   "²" },
    { "sup3;",   "³" },
    { "szlig;",  "ß" },
    { "thorn;",  "þ" },
    { "tilde;",  "˜" },
    { "times;",  "×" },
    { "trade;",  "™" },
    { "uacute;", "ú" },
    { "ucirc;",  "û" },
    { "ugrave;", "ù" },
    { "uml;",    "¨" },
    { "uuml;",   "ü" },
    { "yacute;", "ý" },
    { "yen;",    "¥" },
    { "yuml;",   "ÿ" },
};

static int cmp_entity (const void *key, const void *elem)
{
    const struct xml_entity_s *ent = elem;
    const char *name = key;

    return strncmp (name, ent->psz_entity, strlen (ent->psz_entity));
}

/**
 * Converts "&lt;", "&gt;" and "&amp;" to "<", ">" and "&"
 * \param string to convert
 */
void resolve_xml_special_chars( char *psz_value )
{
    char *p_pos = psz_value;

    while ( *psz_value )
    {
        if( *psz_value == '&' )
        {
            if( psz_value[1] == '#' )
            {   /* &#xxx; Unicode code point */
                char *psz_end;
                unsigned long cp = strtoul( psz_value+2, &psz_end, 10 );
                if( *psz_end == ';' )
                {
                    psz_value = psz_end + 1;
                    if( cp == 0 )
                        (void)0; /* skip nuls */
                    else
                    if( cp <= 0x7F )
                    {
                        *p_pos =            cp;
                    }
                    else
                    /* Unicode code point outside ASCII.
                     * &#xxx; representation is longer than UTF-8 :) */
                    if( cp <= 0x7FF )
                    {
                        *p_pos++ = 0xC0 |  (cp >>  6);
                        *p_pos   = 0x80 |  (cp        & 0x3F);
                    }
                    else
                    if( cp <= 0xFFFF )
                    {
                        *p_pos++ = 0xE0 |  (cp >> 12);
                        *p_pos++ = 0x80 | ((cp >>  6) & 0x3F);
                        *p_pos   = 0x80 |  (cp        & 0x3F);
                    }
                    else
                    if( cp <= 0x1FFFFF ) /* Outside the BMP */
                    {   /* Unicode stops at 10FFFF, but who cares? */
                        *p_pos++ = 0xF0 |  (cp >> 18);
                        *p_pos++ = 0x80 | ((cp >> 12) & 0x3F);
                        *p_pos++ = 0x80 | ((cp >>  6) & 0x3F);
                        *p_pos   = 0x80 |  (cp        & 0x3F);
                    }
                }
                else
                {
                    /* Invalid entity number */
                    *p_pos = *psz_value;
                    psz_value++;
                }
            }
            else
            {   /* Well-known XML entity */
                const struct xml_entity_s *ent;

                ent = bsearch (psz_value + 1, xml_entities,
                               sizeof (xml_entities) / sizeof (*ent),
                               sizeof (*ent), cmp_entity);
                if (ent != NULL)
                {
                    size_t olen = strlen (ent->psz_char);
                    memcpy (p_pos, ent->psz_char, olen);
                    p_pos += olen - 1;
                    psz_value += strlen (ent->psz_entity) + 1;
                }
                else
                {   /* No match */
                    *p_pos = *psz_value;
                    psz_value++;
                }
            }
        }
        else
        {
            *p_pos = *psz_value;
            psz_value++;
        }

        p_pos++;
    }

    *p_pos = '\0';
}

/**
 * XML-encode an UTF-8 string
 * \param str nul-terminated UTF-8 byte sequence to XML-encode
 * \return XML encoded string or NULL on error
 * (errno is set to ENOMEM or EILSEQ as appropriate)
 */
char *convert_xml_special_chars (const char *str)
{
    assert (str != NULL);

    const size_t len = strlen (str);
    char *const buf = malloc (6 * len + 1), *ptr = buf;
    if (unlikely(buf == NULL))
        return NULL;

    size_t n;
    uint32_t cp;

    while ((n = vlc_towc (str, &cp)) != 0)
    {
        if (unlikely(n == (size_t)-1))
        {
            free (buf);
            errno = EILSEQ;
            return NULL;
        }

        if ((cp & ~0x0080) < 32 /* C0/C1 control codes */
         && memchr ("\x09\x0A\x0D\x85", cp, 4) == NULL)
            ptr += sprintf (ptr, "&#%"PRIu32";", cp);
        else
        switch (cp)
        {
            case '\"': strcpy (ptr, "&quot;"); ptr += 6; break;
            case '&':  strcpy (ptr, "&amp;");  ptr += 5; break;
            case '\'': strcpy (ptr, "&#39;");  ptr += 5; break;
            case '<':  strcpy (ptr, "&lt;");   ptr += 4; break;
            case '>':  strcpy (ptr, "&gt;");   ptr += 4; break;
            default:   memcpy (ptr, str, n);   ptr += n; break;
        }
        str += n;
    }
    *(ptr++) = '\0';

    ptr = realloc (buf, ptr - buf);
    return likely(ptr != NULL) ? ptr : buf; /* cannot fail */
}

/* Base64 encoding */
char *vlc_b64_encode_binary( const uint8_t *src, size_t i_src )
{
    static const char b64[] =
           "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    char *ret = malloc( ( i_src + 4 ) * 4 / 3 );
    char *dst = ret;

    if( dst == NULL )
        return NULL;

    while( i_src > 0 )
    {
        /* pops (up to) 3 bytes of input, push 4 bytes */
        uint32_t v;

        /* 1/3 -> 1/4 */
        v = *src++ << 24;
        *dst++ = b64[v >> 26];
        v = v << 6;

        /* 2/3 -> 2/4 */
        if( i_src >= 2 )
            v |= *src++ << 22;
        *dst++ = b64[v >> 26];
        v = v << 6;

        /* 3/3 -> 3/4 */
        if( i_src >= 3 )
            v |= *src++ << 20; // 3/3
        *dst++ = ( i_src >= 2 ) ? b64[v >> 26] : '='; // 3/4
        v = v << 6;

        /* -> 4/4 */
        *dst++ = ( i_src >= 3 ) ? b64[v >> 26] : '='; // 4/4

        if( i_src <= 3 )
            break;
        i_src -= 3;
    }

    *dst = '\0';

    return ret;
}

char *vlc_b64_encode( const char *src )
{
    if( src )
        return vlc_b64_encode_binary( (const uint8_t*)src, strlen(src) );
    else
        return vlc_b64_encode_binary( (const uint8_t*)"", 0 );
}

/* Base64 decoding */
size_t vlc_b64_decode_binary_to_buffer( uint8_t *p_dst, size_t i_dst, const char *p_src )
{
    static const int b64[256] = {
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* 00-0F */
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* 10-1F */
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,62,-1,-1,-1,63,  /* 20-2F */
        52,53,54,55,56,57,58,59,60,61,-1,-1,-1,-1,-1,-1,  /* 30-3F */
        -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,  /* 40-4F */
        15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,-1,  /* 50-5F */
        -1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,  /* 60-6F */
        41,42,43,44,45,46,47,48,49,50,51,-1,-1,-1,-1,-1,  /* 70-7F */
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* 80-8F */
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* 90-9F */
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* A0-AF */
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* B0-BF */
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* C0-CF */
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* D0-DF */
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* E0-EF */
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1   /* F0-FF */
    };
    uint8_t *p_start = p_dst;
    uint8_t *p = (uint8_t *)p_src;

    int i_level;
    int i_last;

    for( i_level = 0, i_last = 0; (size_t)( p_dst - p_start ) < i_dst && *p != '\0'; p++ )
    {
        const int c = b64[(unsigned int)*p];
        if( c == -1 )
            break;

        switch( i_level )
        {
            case 0:
                i_level++;
                break;
            case 1:
                *p_dst++ = ( i_last << 2 ) | ( ( c >> 4)&0x03 );
                i_level++;
                break;
            case 2:
                *p_dst++ = ( ( i_last << 4 )&0xf0 ) | ( ( c >> 2 )&0x0f );
                i_level++;
                break;
            case 3:
                *p_dst++ = ( ( i_last &0x03 ) << 6 ) | c;
                i_level = 0;
        }
        i_last = c;
    }

    return p_dst - p_start;
}
size_t vlc_b64_decode_binary( uint8_t **pp_dst, const char *psz_src )
{
    const int i_src = strlen( psz_src );
    uint8_t   *p_dst;

    *pp_dst = p_dst = malloc( i_src );
    if( !p_dst )
        return 0;
    return  vlc_b64_decode_binary_to_buffer( p_dst, i_src, psz_src );
}
char *vlc_b64_decode( const char *psz_src )
{
    const int i_src = strlen( psz_src );
    char *p_dst = malloc( i_src + 1 );
    size_t i_dst;
    if( !p_dst )
        return NULL;

    i_dst = vlc_b64_decode_binary_to_buffer( (uint8_t*)p_dst, i_src, psz_src );
    p_dst[i_dst] = '\0';

    return p_dst;
}

/**
 * Formats current time into a heap-allocated string.
 * @param tformat time format (as with C strftime())
 * @return an allocated string (must be free()'d), or NULL on memory error.
 */
char *str_format_time( const char *tformat )
{
    time_t curtime;
    struct tm loctime;

    if (strcmp (tformat, "") == 0)
        return strdup (""); /* corner case w.r.t. strftime() return value */

    /* Get the current time.  */
    time( &curtime );

    /* Convert it to local time representation.  */
    localtime_r( &curtime, &loctime );
    for (size_t buflen = strlen (tformat) + 32;; buflen += 32)
    {
        char *str = malloc (buflen);
        if (str == NULL)
            return NULL;

        size_t len = strftime (str, buflen, tformat, &loctime);
        if (len > 0)
        {
            char *ret = realloc (str, len + 1);
            return ret ? ret : str; /* <- this cannot fail */
        }
    }
    assert (0);
}

static void format_duration (char *buf, size_t len, int64_t duration)
{
    lldiv_t d;
    int sec;

    duration /= CLOCK_FREQ;
    d = lldiv (duration, 60);
    sec = d.rem;
    d = lldiv (d.quot, 60);
    snprintf (buf, len, "%02lld:%02d:%02d", d.quot, (int)d.rem, sec);
}

#define INSERT_STRING( string )                                     \
                    if( string != NULL )                            \
                    {                                               \
                        int len = strlen( string );                 \
                        dst = xrealloc( dst, i_size = i_size + len );\
                        memcpy( (dst+d), string, len );             \
                        d += len;                                   \
                        free( string );                             \
                    }

/* same than INSERT_STRING, except that string won't be freed */
#define INSERT_STRING_NO_FREE( string )                             \
                    {                                               \
                        int len = strlen( string );                 \
                        dst = xrealloc( dst, i_size = i_size + len );\
                        memcpy( dst+d, string, len );               \
                        d += len;                                   \
                    }
#undef str_format_meta
char *str_format_meta( vlc_object_t *p_object, const char *string )
{
    const char *s = string;
    bool b_is_format = false;
    bool b_empty_if_na = false;
    char buf[10];
    int i_size = strlen( string ) + 1; /* +1 to store '\0' */
    char *dst = strdup( string );
    if( !dst ) return NULL;
    int d = 0;

    input_thread_t *p_input = playlist_CurrentInput( pl_Get(p_object) );
    input_item_t *p_item = NULL;
    if( p_input )
    {
        p_item = input_GetItem(p_input);
    }

    while( *s )
    {
        if( b_is_format )
        {
            switch( *s )
            {
                case 'a':
                    if( p_item )
                    {
                        INSERT_STRING( input_item_GetArtist( p_item ) );
                    }
                    break;
                case 'b':
                    if( p_item )
                    {
                        INSERT_STRING( input_item_GetAlbum( p_item ) );
                    }
                    break;
                case 'c':
                    if( p_item )
                    {
                        INSERT_STRING( input_item_GetCopyright( p_item ) );
                    }
                    break;
                case 'd':
                    if( p_item )
                    {
                        INSERT_STRING( input_item_GetDescription( p_item ) );
                    }
                    break;
                case 'e':
                    if( p_item )
                    {
                        INSERT_STRING( input_item_GetEncodedBy( p_item ) );
                    }
                    break;
                case 'f':
                    if( p_item && p_item->p_stats )
                    {
                        vlc_mutex_lock( &p_item->p_stats->lock );
                        snprintf( buf, 10, "%"PRIi64,
                                  p_item->p_stats->i_displayed_pictures );
                        vlc_mutex_unlock( &p_item->p_stats->lock );
                    }
                    else
                        strcpy( buf, b_empty_if_na ? "" : "-" );
                    INSERT_STRING_NO_FREE( buf );
                    break;
                case 'g':
                    if( p_item )
                    {
                        INSERT_STRING( input_item_GetGenre( p_item ) );
                    }
                    break;
                case 'l':
                    if( p_item )
                    {
                        INSERT_STRING( input_item_GetLanguage( p_item ) );
                    }
                    break;
                case 'n':
                    if( p_item )
                    {
                        INSERT_STRING( input_item_GetTrackNum( p_item ) );
                    }
                    break;
                case 'p':
                    if( p_item )
                    {
                        INSERT_STRING( input_item_GetNowPlaying( p_item ) );
                    }
                    break;
                case 'r':
                    if( p_item )
                    {
                        INSERT_STRING( input_item_GetRating( p_item ) );
                    }
                    break;
                case 's':
                    {
                        char *psz_lang = NULL;
                        if( p_input )
                            psz_lang = var_GetNonEmptyString( p_input, "sub-language" );
                        if( psz_lang == NULL )
                            psz_lang = strdup( b_empty_if_na ? "" : "-" );
                        INSERT_STRING( psz_lang );
                        break;
                    }
                case 't':
                    if( p_item )
                    {
                        INSERT_STRING( input_item_GetTitle( p_item ) );
                    }
                    break;
                case 'u':
                    if( p_item )
                    {
                        INSERT_STRING( input_item_GetURL( p_item ) );
                    }
                    break;
                case 'A':
                    if( p_item )
                    {
                        INSERT_STRING( input_item_GetDate( p_item ) );
                    }
                    break;
                case 'B':
                    if( p_input )
                    {
                        snprintf( buf, 10, "%"PRId64,
                                  var_GetInteger( p_input, "bit-rate" )/1000 );
                    }
                    else
                        strcpy( buf, b_empty_if_na ? "" : "-" );
                    INSERT_STRING_NO_FREE( buf );
                    break;
                case 'C':
                    if( p_input )
                    {
                        snprintf( buf, 10, "%"PRId64,
                                  var_GetInteger( p_input, "chapter" ) );
                    }
                    else
                        strcpy( buf, b_empty_if_na ? "" : "-" );
                    INSERT_STRING_NO_FREE( buf );
                    break;
                case 'D':
                    if( p_item )
                    {
                        mtime_t i_duration = input_item_GetDuration( p_item );
                        format_duration (buf, sizeof (buf), i_duration);
                    }
                    else
                        strcpy( buf, b_empty_if_na ? "" : "--:--:--" );
                    INSERT_STRING_NO_FREE( buf );
                    break;
                case 'F':
                    if( p_item )
                    {
                        INSERT_STRING( input_item_GetURI( p_item ) );
                    }
                    break;
                case 'I':
                    if( p_input )
                    {
                        snprintf( buf, 10, "%"PRId64,
                                  var_GetInteger( p_input, "title" ) );
                    }
                    else
                        strcpy( buf, b_empty_if_na ? "" : "-" );
                    INSERT_STRING_NO_FREE( buf );
                    break;
                case 'L':
                    if( p_item && p_input )
                    {
                        mtime_t i_duration = input_item_GetDuration( p_item );
                        int64_t i_time = var_GetTime( p_input, "time" );
                        format_duration( buf, sizeof(buf),
                                         i_duration - i_time );
                    }
                    else
                        strcpy( buf, b_empty_if_na ? "" : "--:--:--" );
                    INSERT_STRING_NO_FREE( buf );
                    break;
                case 'N':
                    if( p_item )
                    {
                        INSERT_STRING( input_item_GetName( p_item ) );
                    }
                    break;
                case 'O':
                    {
                        char *lang = NULL;
                        if( p_input )
                            lang = var_GetNonEmptyString( p_input,
                                                          "audio-language" );
                        if( lang == NULL )
                            lang = strdup( b_empty_if_na ? "" : "-" );
                        INSERT_STRING( lang );
                        break;
                    }
                case 'P':
                    if( p_input )
                    {
                        snprintf( buf, 10, "%2.1lf",
                                  var_GetFloat( p_input, "position" ) * 100. );
                    }
                    else
                    {
                        snprintf( buf, 10, b_empty_if_na ? "" : "--.-%%" );
                    }
                    INSERT_STRING_NO_FREE( buf );
                    break;
                case 'R':
                    if( p_input )
                    {
                        float f = var_GetFloat( p_input, "rate" );
                        snprintf( buf, 10, "%.3f", f );
                    }
                    else
                        strcpy( buf, b_empty_if_na ? "" : "-" );
                    INSERT_STRING_NO_FREE( buf );
                    break;
                case 'S':
                    if( p_input )
                    {
                        int r = var_GetInteger( p_input, "sample-rate" );
                        snprintf( buf, 10, "%d.%d", r/1000, (r/100)%10 );
                    }
                    else
                        strcpy( buf, b_empty_if_na ? "" : "-" );
                    INSERT_STRING_NO_FREE( buf );
                    break;
                case 'T':
                    if( p_input )
                    {
                        int64_t i_time = var_GetTime( p_input, "time" );
                        format_duration( buf, sizeof(buf), i_time );
                    }
                    else
                        strcpy( buf, b_empty_if_na ? "" : "--:--:--" );
                    INSERT_STRING_NO_FREE( buf );
                    break;
                case 'U':
                    if( p_item )
                    {
                        INSERT_STRING( input_item_GetPublisher( p_item ) );
                    }
                    break;
                case 'V':
                    {
                        audio_volume_t volume = aout_VolumeGet( p_object );
                        snprintf( buf, 10, "%d", volume );
                        INSERT_STRING_NO_FREE( buf );
                        break;
                    }
                case '_':
                    *(dst+d) = '\n';
                    d++;
                    break;
                case 'Z':
                    if( p_item )
                    {
                        char *psz_now_playing = input_item_GetNowPlaying( p_item );
                        if( EMPTY_STR( psz_now_playing ) )
                        {
                            char *psz_temp = input_item_GetTitleFbName( p_item );
                            char *psz_artist = input_item_GetArtist( p_item );
                            if( !EMPTY_STR( psz_artist ) )
                            {
                                INSERT_STRING( psz_artist );
                                if ( !EMPTY_STR( psz_temp ) )
                                    INSERT_STRING_NO_FREE( " - " );
                            }
                            INSERT_STRING( psz_temp );
                        }
                        else
                            INSERT_STRING( psz_now_playing );
                    }
                    break;

                case ' ':
                    b_empty_if_na = true;
                    break;

                default:
                    *(dst+d) = *s;
                    d++;
                    break;
            }
            if( *s != ' ' )
                b_is_format = false;
        }
        else if( *s == '$' )
        {
            b_is_format = true;
            b_empty_if_na = false;
        }
        else
        {
            *(dst+d) = *s;
            d++;
        }
        s++;
    }
    *(dst+d) = '\0';

    if( p_input )
        vlc_object_release( p_input );

    return dst;
}
#undef INSERT_STRING
#undef INSERT_STRING_NO_FREE

#undef str_format
/**
 * Apply str format time and str format meta
 */
char *str_format( vlc_object_t *p_this, const char *psz_src )
{
    char *psz_buf1, *psz_buf2;
    psz_buf1 = str_format_time( psz_src );
    psz_buf2 = str_format_meta( p_this, psz_buf1 );
    free( psz_buf1 );
    return psz_buf2;
}

/**
 * Remove forbidden, potentially forbidden and otherwise evil characters from
 * filenames. This includes slashes, and popular characters like colon
 * (on Unix anyway), so this should only be used for automatically generated
 * filenames.
 * \warning Do not use this on full paths,
 * only single file names without any directory separator!
 */
void filename_sanitize( char *str )
{
    unsigned char c;

    /* Special file names, not allowed */
    if( !strcmp( str, "." ) || !strcmp( str, ".." ) )
    {
        while( *str )
            *(str++) = '_';
        return;
    }

    /* On platforms not using UTF-7, VLC cannot access non-Unicode paths.
     * Also, some file systems require Unicode file names.
     * NOTE: This may inserts '?' thus is done replacing '?' with '_'. */
    EnsureUTF8( str );

    /* Avoid leading spaces to please Windows. */
    while( (c = *str) != '\0' )
    {
        if( c != ' ' )
            break;
        *(str++) = '_';
    }

    char *start = str;

    while( (c = *str) != '\0' )
    {
        /* Non-printable characters are not a good idea */
        if( c < 32 )
            *str = '_';
        /* This is the list of characters not allowed by Microsoft.
         * We also black-list them on Unix as they may be confusing, and are
         * not supported by some file system types (notably CIFS). */
        else if( strchr( "/:\\*\"?|<>", c ) != NULL )
            *str = '_';
        str++;
    }

    /* Avoid trailing spaces also to please Windows. */
    while( str > start )
    {
        if( *(--str) != ' ' )
            break;
        *str = '_';
    }
}

/**
 * Remove forbidden characters from full paths (leaves slashes)
 */
void path_sanitize( char *str )
{
#if defined( WIN32 ) || defined( __OS2__ )
    /* check drive prefix if path is absolute */
    if( (((unsigned char)(str[0] - 'A') < 26)
      || ((unsigned char)(str[0] - 'a') < 26)) && (':' == str[1]) )
        str += 2;
#endif
    while( *str )
    {
#if defined( __APPLE__ )
        if( *str == ':' )
            *str = '_';
#elif defined( WIN32 ) || defined( __OS2__ )
        if( strchr( "*\"?:|<>", *str ) )
            *str = '_';
        if( *str == '/' )
            *str = DIR_SEP_CHAR;
#endif
        str++;
    }
}

#include <vlc_url.h>
#ifdef WIN32
# include <io.h>
#endif

/**
 * Convert a file path to a URI.
 * If already a URI, return a copy of the string.
 * @param path path to convert (or URI to copy)
 * @param scheme URI scheme to use (default is auto: "file", "fd" or "smb")
 * @return a nul-terminated URI string (use free() to release it),
 * or NULL in case of error
 */
char *make_URI (const char *path, const char *scheme)
{
    if (path == NULL)
        return NULL;
    if (scheme == NULL && !strcmp (path, "-"))
        return strdup ("fd://0"); // standard input
    if (strstr (path, "://") != NULL)
        return strdup (path); /* Already a URI */
    /* Note: VLC cannot handle URI schemes without double slash after the
     * scheme name (such as mailto: or news:). */

    char *buf;

#ifdef __OS2__
    char p[strlen (path) + 1];

    for (buf = p; *path; buf++, path++)
        *buf = (*path == '/') ? DIR_SEP_CHAR : *path;
    *buf = '\0';

    path = p;
#endif

#if defined( WIN32 ) || defined( __OS2__ )
    /* Drive letter */
    if (isalpha ((unsigned char)path[0]) && (path[1] == ':'))
    {
        if (asprintf (&buf, "%s:///%c:", scheme ? scheme : "file",
                      path[0]) == -1)
            buf = NULL;
        path += 2;
# warning Drive letter-relative path not implemented!
        if (path[0] != DIR_SEP_CHAR)
            return NULL;
    }
    else
#endif
    if (!strncmp (path, "\\\\", 2))
    {   /* Windows UNC paths */
#if !defined( WIN32 ) && !defined( __OS2__ )
        if (scheme != NULL)
            return NULL; /* remote files not supported */

        /* \\host\share\path -> smb://host/share/path */
        if (strchr (path + 2, '\\') != NULL)
        {   /* Convert backslashes to slashes */
            char *dup = strdup (path);
            if (dup == NULL)
                return NULL;
            for (size_t i = 2; dup[i]; i++)
                if (dup[i] == '\\')
                    dup[i] = DIR_SEP_CHAR;

            char *ret = make_URI (dup, scheme);
            free (dup);
            return ret;
        }
# define SMB_SCHEME "smb"
#else
        /* \\host\share\path -> file://host/share/path */
# define SMB_SCHEME "file"
#endif
        size_t hostlen = strcspn (path + 2, DIR_SEP);

        buf = malloc (sizeof (SMB_SCHEME) + 3 + hostlen);
        if (buf != NULL)
            snprintf (buf, sizeof (SMB_SCHEME) + 3 + hostlen,
                      SMB_SCHEME"://%s", path + 2);
        path += 2 + hostlen;

        if (path[0] == '\0')
            return buf; /* Hostname without path */
    }
    else
    if (path[0] != DIR_SEP_CHAR)
    {   /* Relative path: prepend the current working directory */
        char *cwd, *ret;

        if ((cwd = vlc_getcwd ()) == NULL)
            return NULL;
        if (asprintf (&buf, "%s"DIR_SEP"%s", cwd, path) == -1)
            buf = NULL;

        free (cwd);
        ret = (buf != NULL) ? make_URI (buf, scheme) : NULL;
        free (buf);
        return ret;
    }
    else
    if (asprintf (&buf, "%s://", scheme ? scheme : "file") == -1)
        buf = NULL;
    if (buf == NULL)
        return NULL;

    assert (path[0] == DIR_SEP_CHAR);

    /* Absolute file path */
    for (const char *ptr = path + 1;; ptr++)
    {
        size_t len = strcspn (ptr, DIR_SEP);
        char *component = encode_URI_bytes (ptr, len);
        if (component == NULL)
        {
            free (buf);
            return NULL;
        }
        char *uri;
        int val = asprintf (&uri, "%s/%s", buf, component);
        free (component);
        free (buf);
        if (val == -1)
            return NULL;
        buf = uri;
        ptr += len;
        if (*ptr == '\0')
            return buf;
    }
}

/**
 * Tries to convert a URI to a local (UTF-8-encoded) file path.
 * @param url URI to convert
 * @return NULL on error, a nul-terminated string otherwise
 * (use free() to release it)
 */
char *make_path (const char *url)
{
    char *ret = NULL;
    char *end;

    char *path = strstr (url, "://");
    if (path == NULL)
        return NULL; /* unsupported scheme or invalid syntax */

    end = memchr (url, '/', path - url);
    size_t schemelen = ((end != NULL) ? end : path) - url;
    path += 3; /* skip "://" */

    /* Remove HTML anchor if present */
    end = strchr (path, '#');
    if (end)
        path = strndup (path, end - path);
    else
        path = strdup (path);
    if (unlikely(path == NULL))
        return NULL; /* boom! */

    /* Decode path */
    decode_URI (path);

    if (schemelen == 4 && !strncasecmp (url, "file", 4))
    {
#if (!defined (WIN32) && !defined (__OS2__)) || defined (UNDER_CE)
        /* Leading slash => local path */
        if (*path == '/')
            return path;
        /* Local path disguised as a remote one */
        if (!strncasecmp (path, "localhost/", 10))
            return memmove (path, path + 9, strlen (path + 9) + 1);
#else
        for (char *p = strchr (path, '/'); p; p = strchr (p + 1, '/'))
            *p = '\\';

        /* Leading backslash => local path */
        if (*path == '\\')
            return memmove (path, path + 1, strlen (path + 1) + 1);
        /* Local path disguised as a remote one */
        if (!strncasecmp (path, "localhost\\", 10))
            return memmove (path, path + 10, strlen (path + 10) + 1);
        /* UNC path */
        if (*path && asprintf (&ret, "\\\\%s", path) == -1)
            ret = NULL;
#endif
        /* non-local path :-( */
    }
    else
    if (schemelen == 2 && !strncasecmp (url, "fd", 2))
    {
        int fd = strtol (path, &end, 0);

        if (*end)
            goto out;

#if !defined( WIN32 ) && !defined( __OS2__ )
        switch (fd)
        {
            case 0:
                ret = strdup ("/dev/stdin");
                break;
            case 1:
                ret = strdup ("/dev/stdout");
                break;
            case 2:
                ret = strdup ("/dev/stderr");
                break;
            default:
                if (asprintf (&ret, "/dev/fd/%d", fd) == -1)
                    ret = NULL;
        }
#else
        /* XXX: Does this work on WinCE? */
        if (fd < 2)
            ret = strdup ("CON");
        else
            ret = NULL;
#endif
    }

out:
    free (path);
    return ret; /* unknown scheme */
}

/*
  Decodes a duration as defined by ISO 8601
  http://en.wikipedia.org/wiki/ISO_8601#Durations
  @param str A null-terminated string to convert
  @return: The duration in seconds. -1 if an error occured.

  Exemple input string: "PT0H9M56.46S"
 */
time_t str_duration( const char *psz_duration )
{
    bool        timeDesignatorReached = false;
    time_t      res = 0;
    char*       end_ptr;

    if ( psz_duration == NULL )
        return -1;
    if ( ( *(psz_duration++) ) != 'P' )
        return -1;
    do
    {
        double number = strtod( psz_duration, &end_ptr );
        double      mul = 0;
        if ( psz_duration != end_ptr )
            psz_duration = end_ptr;
        switch( *psz_duration )
        {
            case 'M':
            {
                //M can mean month or minutes, if the 'T' flag has been reached.
                //We don't handle months though.
                if ( timeDesignatorReached == true )
                    mul = 60.0;
                break ;
            }
            case 'Y':
            case 'W':
                break ; //Don't handle this duration.
            case 'D':
                mul = 86400.0;
                break ;
            case 'T':
                timeDesignatorReached = true;
                break ;
            case 'H':
                mul = 3600.0;
                break ;
            case 'S':
                mul = 1.0;
                break ;
            default:
                break ;
        }
        res += (time_t)(mul * number);
        if ( *psz_duration )
            psz_duration++;
    } while ( *psz_duration );
    return res;
}
