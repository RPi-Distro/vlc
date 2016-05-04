/*****************************************************************************
 * strings.c: String related functions
 *****************************************************************************
 * Copyright (C) 2006 VLC authors and VideoLAN
 * Copyright (C) 2008-2009 Rémi Denis-Courmont
 * $Id: 1b86e9c153fabb1f01b430cef909a8bed1bec3e8 $
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
#include <math.h>

/* Needed by str_format_meta */
#include <vlc_input.h>
#include <vlc_meta.h>
#include <vlc_aout.h>

#include <vlc_strings.h>
#include <vlc_charset.h>
#include <libvlc.h>
#include <errno.h>

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
            {   /* &#DDD; or &#xHHHH; Unicode code point */
                char *psz_end;
                unsigned long cp;

                if( psz_value[2] == 'x' ) /* The x must be lower-case. */
                    cp = strtoul( psz_value + 3, &psz_end, 16 );
                else
                    cp = strtoul( psz_value + 2, &psz_end, 10 );

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

        switch (cp)
        {
            case '\"': strcpy (ptr, "&quot;"); ptr += 6; break;
            case '&':  strcpy (ptr, "&amp;");  ptr += 5; break;
            case '\'': strcpy (ptr, "&#39;");  ptr += 5; break;
            case '<':  strcpy (ptr, "&lt;");   ptr += 4; break;
            case '>':  strcpy (ptr, "&gt;");   ptr += 4; break;
            default:
                if (cp < 32) /* C0 code not allowed (except 9, 10 and 13) */
                    break;
                if (cp >= 128 && cp < 160) /* C1 code encoded (except 133) */
                {
                    ptr += sprintf (ptr, "&#%"PRIu32";", cp);
                    break;
                }
                /* fall through */
            case 9:
            case 10:
            case 13:
            case 133:
                memcpy (ptr, str, n);
                ptr += n;
                break;
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
        free (str);
    }
    assert (0);
}

static void write_duration(FILE *stream, int64_t duration)
{
    lldiv_t d;
    long long sec;

    duration /= CLOCK_FREQ;
    d = lldiv(duration, 60);
    sec = d.rem;
    d = lldiv(d.quot, 60);
    fprintf(stream, "%02lld:%02lld:%02lld", d.quot, d.rem, sec);
}

static int write_meta(FILE *stream, input_item_t *item, vlc_meta_type_t type)
{
    if (item == NULL)
        return EOF;

    char *value = input_item_GetMeta(item, type);
    if (value == NULL)
        return EOF;

    int ret = fputs(value, stream);
    free(value);
    return ret;
}

char *str_format_meta(input_thread_t *input, const char *s)
{
    char *str;
    size_t len;
#ifdef HAVE_OPEN_MEMSTREAM
    FILE *stream = open_memstream(&str, &len);
#elif defined( _WIN32 )
    FILE *stream = vlc_win32_tmpfile();
#else
    FILE *stream = tmpfile();
#endif
    if (stream == NULL)
        return NULL;

    input_item_t *item = (input != NULL) ? input_GetItem(input) : NULL;

    char c;
    bool b_is_format = false;
    bool b_empty_if_na = false;

    assert(s != NULL);

    while ((c = *s) != '\0')
    {
        s++;

        if (!b_is_format)
        {
            if (c == '$')
            {
                b_is_format = true;
                b_empty_if_na = false;
                continue;
            }

            fputc(c, stream);
            continue;
        }

        b_is_format = false;

        switch (c)
        {
            case 'a':
                write_meta(stream, item, vlc_meta_Artist);
                break;
            case 'b':
                write_meta(stream, item, vlc_meta_Album);
                break;
            case 'c':
                write_meta(stream, item, vlc_meta_Copyright);
                break;
            case 'd':
                write_meta(stream, item, vlc_meta_Description);
                break;
            case 'e':
                write_meta(stream, item, vlc_meta_EncodedBy);
                break;
            case 'f':
                if (item != NULL && item->p_stats != NULL)
                {
                    vlc_mutex_lock(&item->p_stats->lock);
                    fprintf(stream, "%"PRIi64,
                            item->p_stats->i_displayed_pictures);
                    vlc_mutex_unlock(&item->p_stats->lock);
                }
                else if (!b_empty_if_na)
                    fputc('-', stream);
                break;
            case 'g':
                write_meta(stream, item, vlc_meta_Genre);
                break;
            case 'l':
                write_meta(stream, item, vlc_meta_Language);
                break;
            case 'n':
                write_meta(stream, item, vlc_meta_TrackNumber);
                break;
            case 'p':
                if (item == NULL)
                    break;
                {
                    char *value = input_item_GetNowPlayingFb(item);
                    if (value == NULL)
                        break;

                    fputs(value, stream);
                    free(value);
                }
                break;
            case 'r':
                write_meta(stream, item, vlc_meta_Rating);
                break;
            case 's':
            {
                char *lang = NULL;

                if (input != NULL)
                    lang = var_GetNonEmptyString(input, "sub-language");
                if (lang != NULL)
                {
                    fputs(lang, stream);
                    free(lang);
                }
                else if (!b_empty_if_na)
                    fputc('-', stream);
                break;
            }
            case 't':
                write_meta(stream, item, vlc_meta_Title);
                break;
            case 'u':
                write_meta(stream, item, vlc_meta_URL);
                break;
            case 'A':
                write_meta(stream, item, vlc_meta_Date);
                break;
            case 'B':
                if (input != NULL)
                    fprintf(stream, "%"PRId64,
                            var_GetInteger(input, "bit-rate") / 1000);
                else if (!b_empty_if_na)
                    fputc('-', stream);
                break;
            case 'C':
                if (input != NULL)
                    fprintf(stream, "%"PRId64,
                            var_GetInteger(input, "chapter"));
                else if (!b_empty_if_na)
                    fputc('-', stream);
                break;
            case 'D':
                if (item != NULL)
                    write_duration(stream, input_item_GetDuration(item));
                else if (!b_empty_if_na)
                    fputs("--:--:--", stream);
                break;
            case 'F':
                if (item != NULL)
                {
                    char *uri = input_item_GetURI(item);
                    if (uri != NULL)
                    {
                        fputs(uri, stream);
                        free(uri);
                    }
                }
                break;
            case 'I':
                if (input != NULL)
                    fprintf(stream, "%"PRId64, var_GetInteger(input, "title"));
                else if (!b_empty_if_na)
                    fputc('-', stream);
                break;
            case 'L':
                if (item != NULL)
                {
                    assert(input != NULL);
                    write_duration(stream, input_item_GetDuration(item)
                                   - var_GetTime(input, "time"));
                }
                else if (!b_empty_if_na)
                    fputs("--:--:--", stream);
                break;
            case 'N':
                if (item != NULL)
                {
                    char *name = input_item_GetName(item);
                    if (name != NULL)
                    {
                        fputs(name, stream);
                        free(name);
                    }
                }
                break;
            case 'O':
            {
                char *lang = NULL;

                if (input != NULL)
                    lang = var_GetNonEmptyString(input, "audio-language");
                if (lang != NULL)
                {
                    fputs(lang, stream);
                    free(lang);
                }
                else if (!b_empty_if_na)
                    fputc('-', stream);
                break;
            }
            case 'P':
                if (input != NULL)
                    fprintf(stream, "%2.1f",
                            var_GetFloat(input, "position") * 100.f);
                else if (!b_empty_if_na)
                    fputs("--.-%", stream);
                break;
            case 'R':
                if (input != NULL)
                    fprintf(stream, "%.3f", var_GetFloat(input, "rate"));
                else if (!b_empty_if_na)
                    fputc('-', stream);
                break;
            case 'S':
                if (input != NULL)
                {
                    int rate = var_GetInteger(input, "sample-rate");
                    div_t dr = div((rate + 50) / 100, 10);

                    fprintf(stream, "%d.%01d", dr.quot, dr.rem);
                }
                else if (!b_empty_if_na)
                    fputc('-', stream);
                break;
            case 'T':
                if (input != NULL)
                    write_duration(stream, var_GetTime(input, "time"));
                else if (!b_empty_if_na)
                    fputs("--:--:--", stream);
                break;
            case 'U':
                write_meta(stream, item, vlc_meta_Publisher);
                break;
            case 'V':
            {
                float vol = 0.f;

                if (input != NULL)
                {
                    audio_output_t *aout = input_GetAout(input);
                    if (aout != NULL)
                    {
                        vol = aout_VolumeGet(aout);
                        vlc_object_release(aout);
                    }
                }
                if (vol >= 0.f)
                    fprintf(stream, "%ld", lroundf(vol * 256.f));
                else if (!b_empty_if_na)
                    fputs("---", stream);
                break;
            }
            case '_':
                fputc('\n', stream);
                break;
            case 'Z':
                if (item == NULL)
                    break;
                {
                    char *value = input_item_GetNowPlayingFb(item);
                    if (value != NULL)
                    {
                        fputs(value, stream);
                        free(value);
                    }
                    else
                    {
                        char *title = input_item_GetTitleFbName(item);

                        if (write_meta(stream, item, vlc_meta_Artist) >= 0
                            && title != NULL)
                            fputs(" - ", stream);

                        if (title != NULL)
                        {
                            fputs(title, stream);
                            free(title);
                        }
                    }
                }
                break;
            case ' ':
                b_empty_if_na = true;
                b_is_format = true;
                break;
            default:
                fputc(c, stream);
                break;
        }
    }

#ifdef HAVE_OPEN_MEMSTREAM
    return (fclose(stream) == 0) ? str : NULL;
#else
    len = ftell(stream);
    if (len != (size_t)-1)
    {
        rewind(stream);
        str = xmalloc(len + 1);
        fread(str, len, 1, stream);
        str[len] = '\0';
    }
    else
        str = NULL;
    fclose(stream);
    return str;
#endif
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
#if defined( _WIN32 ) || defined( __OS2__ )
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
#elif defined( _WIN32 ) || defined( __OS2__ )
        if( strchr( "*\"?:|<>", *str ) )
            *str = '_';
        if( *str == '/' )
            *str = DIR_SEP_CHAR;
#endif
        str++;
    }
}

/*
  Decodes a duration as defined by ISO 8601
  http://en.wikipedia.org/wiki/ISO_8601#Durations
  @param str A null-terminated string to convert
  @return: The duration in seconds. -1 if an error occurred.

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
