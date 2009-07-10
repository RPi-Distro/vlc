/*****************************************************************************
 * http.c: HTTP input module
 *****************************************************************************
 * Copyright (C) 2001-2008 the VideoLAN team
 * $Id: e21a4b1c1c0ca7cb11968b6828e4f4040bc52af8 $
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Christophe Massiot <massiot@via.ecp.fr>
 *          Rémi Denis-Courmont <rem # videolan.org>
 *          Antoine Cellerier <dionoea at videolan dot org>
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
#include <vlc_plugin.h>


#include <vlc_access.h>

#include <vlc_dialog.h>
#include <vlc_meta.h>
#include <vlc_network.h>
#include <vlc_url.h>
#include <vlc_tls.h>
#include <vlc_strings.h>
#include <vlc_charset.h>
#include <vlc_input.h>
#include <vlc_md5.h>

#ifdef HAVE_ZLIB_H
#   include <zlib.h>
#endif

#include <assert.h>

#ifdef HAVE_LIBPROXY
#    include <proxy.h>
#endif
/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

#define PROXY_TEXT N_("HTTP proxy")
#define PROXY_LONGTEXT N_( \
    "HTTP proxy to be used It must be of the form " \
    "http://[user@]myproxy.mydomain:myport/ ; " \
    "if empty, the http_proxy environment variable will be tried." )

#define PROXY_PASS_TEXT N_("HTTP proxy password")
#define PROXY_PASS_LONGTEXT N_( \
    "If your HTTP proxy requires a password, set it here." )

#define CACHING_TEXT N_("Caching value in ms")
#define CACHING_LONGTEXT N_( \
    "Caching value for HTTP streams. This " \
    "value should be set in milliseconds." )

#define AGENT_TEXT N_("HTTP user agent")
#define AGENT_LONGTEXT N_("User agent that will be " \
    "used for the connection.")

#define RECONNECT_TEXT N_("Auto re-connect")
#define RECONNECT_LONGTEXT N_( \
    "Automatically try to reconnect to the stream in case of a sudden " \
    "disconnect." )

#define CONTINUOUS_TEXT N_("Continuous stream")
#define CONTINUOUS_LONGTEXT N_("Read a file that is " \
    "being constantly updated (for example, a JPG file on a server). " \
    "You should not globally enable this option as it will break all other " \
    "types of HTTP streams." )

#define FORWARD_COOKIES_TEXT N_("Forward Cookies")
#define FORWARD_COOKIES_LONGTEXT N_("Forward Cookies across http redirections ")

vlc_module_begin ()
    set_description( N_("HTTP input") )
    set_capability( "access", 0 )
    set_shortname( N_( "HTTP(S)" ) )
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_ACCESS )

    add_string( "http-proxy", NULL, NULL, PROXY_TEXT, PROXY_LONGTEXT,
                false )
    add_password( "http-proxy-pwd", NULL, NULL,
                  PROXY_PASS_TEXT, PROXY_PASS_LONGTEXT, false )
    add_integer( "http-caching", 4 * DEFAULT_PTS_DELAY / 1000, NULL,
                 CACHING_TEXT, CACHING_LONGTEXT, true )
        change_safe()
    add_string( "http-user-agent", COPYRIGHT_MESSAGE , NULL, AGENT_TEXT,
                AGENT_LONGTEXT, true )
    add_bool( "http-reconnect", 0, NULL, RECONNECT_TEXT,
              RECONNECT_LONGTEXT, true )
    add_bool( "http-continuous", 0, NULL, CONTINUOUS_TEXT,
              CONTINUOUS_LONGTEXT, true )
        change_safe()
    add_bool( "http-forward-cookies", true, NULL, FORWARD_COOKIES_TEXT,
              FORWARD_COOKIES_LONGTEXT, true )
    add_obsolete_string("http-user")
    add_obsolete_string("http-pwd")
    add_shortcut( "http" )
    add_shortcut( "https" )
    add_shortcut( "unsv" )
    add_shortcut( "itpc" ) /* iTunes Podcast */
    add_shortcut( "icyx" )
    set_callbacks( Open, Close )
vlc_module_end ()

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/

/* RFC 2617: Basic and Digest Access Authentication */
typedef struct http_auth_t
{
    char *psz_realm;
    char *psz_domain;
    char *psz_nonce;
    char *psz_opaque;
    char *psz_stale;
    char *psz_algorithm;
    char *psz_qop;
    int i_nonce;
    char *psz_cnonce;
    char *psz_HA1; /* stored H(A1) value if algorithm = "MD5-sess" */
} http_auth_t;

struct access_sys_t
{
    int fd;
    tls_session_t *p_tls;
    v_socket_t    *p_vs;

    /* From uri */
    vlc_url_t url;
    char    *psz_user_agent;
    http_auth_t auth;

    /* Proxy */
    bool b_proxy;
    vlc_url_t  proxy;
    http_auth_t proxy_auth;
    char       *psz_proxy_passbuf;

    /* */
    int        i_code;
    const char *psz_protocol;
    int        i_version;

    char       *psz_mime;
    char       *psz_pragma;
    char       *psz_location;
    bool b_mms;
    bool b_icecast;
    bool b_ssl;
#ifdef HAVE_ZLIB_H
    bool b_compressed;
    struct
    {
        z_stream   stream;
        uint8_t   *p_buffer;
    } inflate;
#endif

    bool b_chunked;
    int64_t    i_chunk;

    int        i_icy_meta;
    int64_t    i_icy_offset;
    char       *psz_icy_name;
    char       *psz_icy_genre;
    char       *psz_icy_title;

    int64_t i_remaining;

    bool b_seekable;
    bool b_reconnect;
    bool b_continuous;
    bool b_pace_control;
    bool b_persist;

    vlc_array_t * cookies;
};

/* */
static int OpenWithCookies( vlc_object_t *p_this, vlc_array_t *cookies );

/* */
static ssize_t Read( access_t *, uint8_t *, size_t );
static ssize_t ReadCompressed( access_t *, uint8_t *, size_t );
static int Seek( access_t *, int64_t );
static int Control( access_t *, int, va_list );

/* */
static int Connect( access_t *, int64_t );
static int Request( access_t *p_access, int64_t i_tell );
static void Disconnect( access_t * );

/* Small Cookie utilities. Cookies support is partial. */
static char * cookie_get_content( const char * cookie );
static char * cookie_get_domain( const char * cookie );
static char * cookie_get_name( const char * cookie );
static void cookie_append( vlc_array_t * cookies, char * cookie );


static void AuthParseHeader( access_t *p_access, const char *psz_header,
                             http_auth_t *p_auth );
static void AuthReply( access_t *p_acces, const char *psz_prefix,
                       vlc_url_t *p_url, http_auth_t *p_auth );
static int AuthCheckReply( access_t *p_access, const char *psz_header,
                           vlc_url_t *p_url, http_auth_t *p_auth );
static void AuthReset( http_auth_t *p_auth );

/*****************************************************************************
 * Open:
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    return OpenWithCookies( p_this, NULL );
}

static int OpenWithCookies( vlc_object_t *p_this, vlc_array_t *cookies )
{
    access_t     *p_access = (access_t*)p_this;
    access_sys_t *p_sys;
    char         *psz, *p;
    /* Only forward an store cookies if the corresponding option is activated */
    bool   b_forward_cookies = var_CreateGetBool( p_access, "http-forward-cookies" );
    vlc_array_t * saved_cookies = b_forward_cookies ? (cookies ? cookies : vlc_array_new()) : NULL;

    /* Set up p_access */
    STANDARD_READ_ACCESS_INIT;
#ifdef HAVE_ZLIB_H
    p_access->pf_read = ReadCompressed;
#endif
    p_sys->fd = -1;
    p_sys->b_proxy = false;
    p_sys->psz_proxy_passbuf = NULL;
    p_sys->i_version = 1;
    p_sys->b_seekable = true;
    p_sys->psz_mime = NULL;
    p_sys->psz_pragma = NULL;
    p_sys->b_mms = false;
    p_sys->b_icecast = false;
    p_sys->psz_location = NULL;
    p_sys->psz_user_agent = NULL;
    p_sys->b_pace_control = true;
    p_sys->b_ssl = false;
#ifdef HAVE_ZLIB_H
    p_sys->b_compressed = false;
    /* 15 is the max windowBits, +32 to enable optional gzip decoding */
    if( inflateInit2( &p_sys->inflate.stream, 32+15 ) != Z_OK )
        msg_Warn( p_access, "Error during zlib initialisation: %s",
                  p_sys->inflate.stream.msg );
    if( zlibCompileFlags() & (1<<17) )
        msg_Warn( p_access, "Your zlib was compiled without gzip support." );
    p_sys->inflate.p_buffer = NULL;
#endif
    p_sys->p_tls = NULL;
    p_sys->p_vs = NULL;
    p_sys->i_icy_meta = 0;
    p_sys->i_icy_offset = 0;
    p_sys->psz_icy_name = NULL;
    p_sys->psz_icy_genre = NULL;
    p_sys->psz_icy_title = NULL;
    p_sys->i_remaining = 0;
    p_sys->b_persist = false;
    p_access->info.i_size = -1;
    p_access->info.i_pos  = 0;
    p_access->info.b_eof  = false;

    p_sys->cookies = saved_cookies;

    /* Parse URI - remove spaces */
    p = psz = strdup( p_access->psz_path );
    while( (p = strchr( p, ' ' )) != NULL )
        *p = '+';
    vlc_UrlParse( &p_sys->url, psz, 0 );
    free( psz );

    if( p_sys->url.psz_host == NULL || *p_sys->url.psz_host == '\0' )
    {
        msg_Warn( p_access, "invalid host" );
        goto error;
    }
    if( !strncmp( p_access->psz_access, "https", 5 ) )
    {
        /* HTTP over SSL */
        p_sys->b_ssl = true;
        if( p_sys->url.i_port <= 0 )
            p_sys->url.i_port = 443;
    }
    else
    {
        if( p_sys->url.i_port <= 0 )
            p_sys->url.i_port = 80;
    }

    /* Do user agent */
    p_sys->psz_user_agent = var_CreateGetString( p_access, "http-user-agent" );

    /* Check proxy */
    psz = var_CreateGetNonEmptyString( p_access, "http-proxy" );
    if( psz )
    {
        p_sys->b_proxy = true;
        vlc_UrlParse( &p_sys->proxy, psz, 0 );
        free( psz );
    }
#ifdef HAVE_LIBPROXY
    else
    {
        pxProxyFactory *pf = px_proxy_factory_new();
        if (pf)
        {
            char *buf;
            int i;
            i=asprintf(&buf, "%s://%s", p_access->psz_access, p_access->psz_path);
            if (i >= 0)
            {
                msg_Dbg(p_access, "asking libproxy about url '%s'", buf);
                char **proxies = px_proxy_factory_get_proxies(pf, buf);
                if (proxies[0])
                {
                    msg_Dbg(p_access, "libproxy suggest to use '%s'", proxies[0]);
                    if(strcmp(proxies[0],"direct://") != 0)
                    {
                        p_sys->b_proxy = true;
                        vlc_UrlParse( &p_sys->proxy, proxies[0], 0);
                    }
                }
                for(i=0;proxies[i];i++) free(proxies[i]);
                free(proxies);
                free(buf);
            }
            px_proxy_factory_free(pf);
        }
        else
        {
            msg_Err(p_access, "Allocating memory for libproxy failed");
        }
    }
#elif HAVE_GETENV
    else
    {
        psz = getenv( "http_proxy" );
        if( psz )
        {
            p_sys->b_proxy = true;
            vlc_UrlParse( &p_sys->proxy, psz, 0 );
        }
    }
#endif
    if( psz ) /* No, this is NOT a use-after-free error */
    {
        psz = var_CreateGetNonEmptyString( p_access, "http-proxy-pwd" );
        if( psz )
            p_sys->proxy.psz_password = p_sys->psz_proxy_passbuf = psz;
    }

    if( p_sys->b_proxy )
    {
        if( p_sys->proxy.psz_host == NULL || *p_sys->proxy.psz_host == '\0' )
        {
            msg_Warn( p_access, "invalid proxy host" );
            goto error;
        }
        if( p_sys->proxy.i_port <= 0 )
        {
            p_sys->proxy.i_port = 80;
        }
    }

    msg_Dbg( p_access, "http: server='%s' port=%d file='%s",
             p_sys->url.psz_host, p_sys->url.i_port, p_sys->url.psz_path );
    if( p_sys->b_proxy )
    {
        msg_Dbg( p_access, "      proxy %s:%d", p_sys->proxy.psz_host,
                 p_sys->proxy.i_port );
    }
    if( p_sys->url.psz_username && *p_sys->url.psz_username )
    {
        msg_Dbg( p_access, "      user='%s'", p_sys->url.psz_username );
    }

    p_sys->b_reconnect = var_CreateGetBool( p_access, "http-reconnect" );
    p_sys->b_continuous = var_CreateGetBool( p_access, "http-continuous" );

connect:
    /* Connect */
    switch( Connect( p_access, 0 ) )
    {
        case -1:
            goto error;

        case -2:
            /* Retry with http 1.0 */
            msg_Dbg( p_access, "switching to HTTP version 1.0" );
            p_sys->i_version = 0;
            p_sys->b_seekable = false;

            if( !vlc_object_alive (p_access) || Connect( p_access, 0 ) )
                goto error;

#ifndef NDEBUG
        case 0:
            break;

        default:
            msg_Err( p_access, "You should not be here" );
            abort();
#endif
    }

    if( p_sys->i_code == 401 )
    {
        char *psz_login, *psz_password;
        /* FIXME ? */
        if( p_sys->url.psz_username && p_sys->url.psz_password &&
            p_sys->auth.psz_nonce && p_sys->auth.i_nonce == 0 )
        {
            Disconnect( p_access );
            goto connect;
        }
        msg_Dbg( p_access, "authentication failed for realm %s",
                 p_sys->auth.psz_realm );
        dialog_Login( p_access, &psz_login, &psz_password,
                      _("HTTP authentication"),
             _("Please enter a valid login name and a password for realm %s."),
                      p_sys->auth.psz_realm );
        if( psz_login != NULL && psz_password != NULL )
        {
            msg_Dbg( p_access, "retrying with user=%s", psz_login );
            p_sys->url.psz_username = psz_login;
            p_sys->url.psz_password = psz_password;
            Disconnect( p_access );
            goto connect;
        }
        else
        {
            free( psz_login );
            free( psz_password );
            goto error;
        }
    }

    if( ( p_sys->i_code == 301 || p_sys->i_code == 302 ||
          p_sys->i_code == 303 || p_sys->i_code == 307 ) &&
        p_sys->psz_location && *p_sys->psz_location )
    {
        msg_Dbg( p_access, "redirection to %s", p_sys->psz_location );

        /* Do not accept redirection outside of HTTP works */
        if( strncmp( p_sys->psz_location, "http", 4 )
         || ( ( p_sys->psz_location[4] != ':' ) /* HTTP */
           && strncmp( p_sys->psz_location + 4, "s:", 2 ) /* HTTP/SSL */ ) )
        {
            msg_Err( p_access, "insecure redirection ignored" );
            goto error;
        }
        free( p_access->psz_path );
        p_access->psz_path = strdup( p_sys->psz_location );
        /* Clean up current Open() run */
        vlc_UrlClean( &p_sys->url );
        AuthReset( &p_sys->auth );
        vlc_UrlClean( &p_sys->proxy );
        free( p_sys->psz_proxy_passbuf );
        AuthReset( &p_sys->proxy_auth );
        free( p_sys->psz_mime );
        free( p_sys->psz_pragma );
        free( p_sys->psz_location );
        free( p_sys->psz_user_agent );

        Disconnect( p_access );
        cookies = p_sys->cookies;
#ifdef HAVE_ZLIB_H
        inflateEnd( &p_sys->inflate.stream );
#endif
        free( p_sys );

        /* Do new Open() run with new data */
        return OpenWithCookies( p_this, cookies );
    }

    if( p_sys->b_mms )
    {
        msg_Dbg( p_access, "this is actually a live mms server, BAIL" );
        goto error;
    }

    if( !strcmp( p_sys->psz_protocol, "ICY" ) || p_sys->b_icecast )
    {
        if( p_sys->psz_mime && strcasecmp( p_sys->psz_mime, "application/ogg" ) )
        {
            if( !strcasecmp( p_sys->psz_mime, "video/nsv" ) ||
                !strcasecmp( p_sys->psz_mime, "video/nsa" ) )
            {
                free( p_access->psz_demux );
                p_access->psz_demux = strdup( "nsv" );
            }
            else if( !strcasecmp( p_sys->psz_mime, "audio/aac" ) ||
                     !strcasecmp( p_sys->psz_mime, "audio/aacp" ) )
            {
                free( p_access->psz_demux );
                p_access->psz_demux = strdup( "m4a" );
            }
            else if( !strcasecmp( p_sys->psz_mime, "audio/mpeg" ) )
            {
                free( p_access->psz_demux );
                p_access->psz_demux = strdup( "mp3" );
            }

            msg_Info( p_access, "Raw-audio server found, %s demuxer selected",
                      p_access->psz_demux );

#if 0       /* Doesn't work really well because of the pre-buffering in
             * shoutcast servers (the buffer content will be sent as fast as
             * possible). */
            p_sys->b_pace_control = false;
#endif
        }
        else if( !p_sys->psz_mime )
        {
            free( p_access->psz_demux );
            /* Shoutcast */
            p_access->psz_demux = strdup( "mp3" );
        }
        /* else probably Ogg Vorbis */
    }
    else if( !strcasecmp( p_access->psz_access, "unsv" ) &&
             p_sys->psz_mime &&
             !strcasecmp( p_sys->psz_mime, "misc/ultravox" ) )
    {
        free( p_access->psz_demux );
        /* Grrrr! detect ultravox server and force NSV demuxer */
        p_access->psz_demux = strdup( "nsv" );
    }
    else if( !strcmp( p_access->psz_access, "itpc" ) )
    {
        free( p_access->psz_demux );
        p_access->psz_demux = strdup( "podcast" );
    }
    else if( p_sys->psz_mime &&
             !strncasecmp( p_sys->psz_mime, "application/xspf+xml", 20 ) &&
             ( memchr( " ;\t", p_sys->psz_mime[20], 4 ) != NULL ) )
    {
        free( p_access->psz_demux );
        p_access->psz_demux = strdup( "xspf-open" );
    }

    if( p_sys->b_reconnect ) msg_Dbg( p_access, "auto re-connect enabled" );

    /* PTS delay */
    var_Create( p_access, "http-caching", VLC_VAR_INTEGER |VLC_VAR_DOINHERIT );

    return VLC_SUCCESS;

error:
    vlc_UrlClean( &p_sys->url );
    vlc_UrlClean( &p_sys->proxy );
    free( p_sys->psz_proxy_passbuf );
    free( p_sys->psz_mime );
    free( p_sys->psz_pragma );
    free( p_sys->psz_location );
    free( p_sys->psz_user_agent );

    Disconnect( p_access );

    if( p_sys->cookies )
    {
        int i;
        for( i = 0; i < vlc_array_count( p_sys->cookies ); i++ )
            free(vlc_array_item_at_index( p_sys->cookies, i ));
        vlc_array_destroy( p_sys->cookies );
    }

#ifdef HAVE_ZLIB_H
    inflateEnd( &p_sys->inflate.stream );
#endif
    free( p_sys );
    return VLC_EGENERIC;
}

/*****************************************************************************
 * Close:
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    access_t     *p_access = (access_t*)p_this;
    access_sys_t *p_sys = p_access->p_sys;

    vlc_UrlClean( &p_sys->url );
    AuthReset( &p_sys->auth );
    vlc_UrlClean( &p_sys->proxy );
    AuthReset( &p_sys->proxy_auth );

    free( p_sys->psz_mime );
    free( p_sys->psz_pragma );
    free( p_sys->psz_location );

    free( p_sys->psz_icy_name );
    free( p_sys->psz_icy_genre );
    free( p_sys->psz_icy_title );

    free( p_sys->psz_user_agent );

    Disconnect( p_access );

    if( p_sys->cookies )
    {
        int i;
        for( i = 0; i < vlc_array_count( p_sys->cookies ); i++ )
            free(vlc_array_item_at_index( p_sys->cookies, i ));
        vlc_array_destroy( p_sys->cookies );
    }

#ifdef HAVE_ZLIB_H
    inflateEnd( &p_sys->inflate.stream );
    free( p_sys->inflate.p_buffer );
#endif

    free( p_sys );
}

/*****************************************************************************
 * Read: Read up to i_len bytes from the http connection and place in
 * p_buffer. Return the actual number of bytes read
 *****************************************************************************/
static int ReadICYMeta( access_t *p_access );
static ssize_t Read( access_t *p_access, uint8_t *p_buffer, size_t i_len )
{
    access_sys_t *p_sys = p_access->p_sys;
    int i_read;

    if( p_sys->fd == -1 )
    {
        p_access->info.b_eof = true;
        return 0;
    }

    if( p_access->info.i_size >= 0 &&
        i_len + p_access->info.i_pos > p_access->info.i_size )
    {
        if( ( i_len = p_access->info.i_size - p_access->info.i_pos ) == 0 )
        {
            p_access->info.b_eof = true;
            return 0;
        }
    }

    if( p_sys->b_chunked )
    {
        if( p_sys->i_chunk < 0 )
        {
            p_access->info.b_eof = true;
            return 0;
        }

        if( p_sys->i_chunk <= 0 )
        {
            char *psz = net_Gets( VLC_OBJECT(p_access), p_sys->fd, p_sys->p_vs );
            /* read the chunk header */
            if( psz == NULL )
            {
                /* fatal error - end of file */
                msg_Dbg( p_access, "failed reading chunk-header line" );
                return 0;
            }
            p_sys->i_chunk = strtoll( psz, NULL, 16 );
            free( psz );

            if( p_sys->i_chunk <= 0 )   /* eof */
            {
                p_sys->i_chunk = -1;
                p_access->info.b_eof = true;
                return 0;
            }
        }

        if( i_len > p_sys->i_chunk )
        {
            i_len = p_sys->i_chunk;
        }
    }
    else if( p_access->info.i_size != -1 && (int64_t)i_len > p_sys->i_remaining) {
        /* Only ask for the remaining length */
        i_len = (size_t)p_sys->i_remaining;
        if(i_len == 0) {
            p_access->info.b_eof = true;
            return 0;
        }
    }


    if( p_sys->i_icy_meta > 0 && p_access->info.i_pos-p_sys->i_icy_offset > 0 )
    {
        int64_t i_next = p_sys->i_icy_meta -
                                    (p_access->info.i_pos - p_sys->i_icy_offset ) % p_sys->i_icy_meta;

        if( i_next == p_sys->i_icy_meta )
        {
            if( ReadICYMeta( p_access ) )
            {
                p_access->info.b_eof = true;
                return -1;
            }
        }
        if( i_len > i_next )
            i_len = i_next;
    }

    i_read = net_Read( p_access, p_sys->fd, p_sys->p_vs, p_buffer, i_len, false );

    if( i_read > 0 )
    {
        p_access->info.i_pos += i_read;

        if( p_sys->b_chunked )
        {
            p_sys->i_chunk -= i_read;
            if( p_sys->i_chunk <= 0 )
            {
                /* read the empty line */
                char *psz = net_Gets( VLC_OBJECT(p_access), p_sys->fd, p_sys->p_vs );
                free( psz );
            }
        }
    }
    else if( i_read <= 0 )
    {
        /*
         * I very much doubt that this will work.
         * If i_read == 0, the connection *IS* dead, so the only
         * sensible thing to do is Disconnect() and then retry.
         * Otherwise, I got recv() completely wrong. -- Courmisch
         */
        if( p_sys->b_continuous )
        {
            Request( p_access, 0 );
            p_sys->b_continuous = false;
            i_read = Read( p_access, p_buffer, i_len );
            p_sys->b_continuous = true;
        }
        Disconnect( p_access );
        if( p_sys->b_reconnect )
        {
            msg_Dbg( p_access, "got disconnected, trying to reconnect" );
            if( Connect( p_access, p_access->info.i_pos ) )
            {
                msg_Dbg( p_access, "reconnection failed" );
            }
            else
            {
                p_sys->b_reconnect = false;
                i_read = Read( p_access, p_buffer, i_len );
                p_sys->b_reconnect = true;
            }
        }

        if( i_read == 0 )
            p_access->info.b_eof = true;
        else if( i_read < 0 )
            p_access->b_error = true;
    }

    if( p_access->info.i_size != -1 )
    {
        p_sys->i_remaining -= i_read;
    }

    return i_read;
}

static int ReadICYMeta( access_t *p_access )
{
    access_sys_t *p_sys = p_access->p_sys;

    uint8_t buffer;
    char *p, *psz_meta;
    int i_read;

    /* Read meta data length */
    i_read = net_Read( p_access, p_sys->fd, p_sys->p_vs, &buffer, 1,
                       true );
    if( i_read <= 0 )
        return VLC_EGENERIC;
    if( buffer == 0 )
        return VLC_SUCCESS;

    i_read = buffer << 4;
    /* msg_Dbg( p_access, "ICY meta size=%u", i_read); */

    psz_meta = malloc( i_read + 1 );
    if( net_Read( p_access, p_sys->fd, p_sys->p_vs,
                  (uint8_t *)psz_meta, i_read, true ) != i_read )
    {
        free( psz_meta );
        return VLC_EGENERIC;
    }

    psz_meta[i_read] = '\0'; /* Just in case */

    /* msg_Dbg( p_access, "icy-meta=%s", psz_meta ); */

    /* Now parse the meta */
    /* Look for StreamTitle= */
    p = strcasestr( (char *)psz_meta, "StreamTitle=" );
    if( p )
    {
        p += strlen( "StreamTitle=" );
        if( *p == '\'' || *p == '"' )
        {
            char closing[] = { p[0], ';', '\0' };
            char *psz = strstr( &p[1], closing );
            if( !psz )
                psz = strchr( &p[1], ';' );

            if( psz ) *psz = '\0';
        }
        else
        {
            char *psz = strchr( &p[1], ';' );
            if( psz ) *psz = '\0';
        }

        if( !p_sys->psz_icy_title ||
            strcmp( p_sys->psz_icy_title, &p[1] ) )
        {
            free( p_sys->psz_icy_title );
            char *psz_tmp = strdup( &p[1] );
            p_sys->psz_icy_title = EnsureUTF8( psz_tmp );
            if( !p_sys->psz_icy_title )
                free( psz_tmp );
            p_access->info.i_update |= INPUT_UPDATE_META;

            msg_Dbg( p_access, "New Title=%s", p_sys->psz_icy_title );
        }
    }
    free( psz_meta );

    return VLC_SUCCESS;
}

#ifdef HAVE_ZLIB_H
static ssize_t ReadCompressed( access_t *p_access, uint8_t *p_buffer,
                               size_t i_len )
{
    access_sys_t *p_sys = p_access->p_sys;

    if( p_sys->b_compressed )
    {
        int i_ret;

        if( !p_sys->inflate.p_buffer )
            p_sys->inflate.p_buffer = malloc( 256 * 1024 );

        if( p_sys->inflate.stream.avail_in == 0 )
        {
            ssize_t i_read = Read( p_access, p_sys->inflate.p_buffer + p_sys->inflate.stream.avail_in, 256 * 1024 );
            if( i_read <= 0 ) return i_read;
            p_sys->inflate.stream.next_in = p_sys->inflate.p_buffer;
            p_sys->inflate.stream.avail_in = i_read;
        }

        p_sys->inflate.stream.avail_out = i_len;
        p_sys->inflate.stream.next_out = p_buffer;

        i_ret = inflate( &p_sys->inflate.stream, Z_SYNC_FLUSH );
        msg_Warn( p_access, "inflate return value: %d, %s", i_ret, p_sys->inflate.stream.msg );

        return i_len - p_sys->inflate.stream.avail_out;
    }
    else
    {
        return Read( p_access, p_buffer, i_len );
    }
}
#endif

/*****************************************************************************
 * Seek: close and re-open a connection at the right place
 *****************************************************************************/
static int Seek( access_t *p_access, int64_t i_pos )
{
    msg_Dbg( p_access, "trying to seek to %"PRId64, i_pos );

    Disconnect( p_access );

    if( p_access->info.i_size
     && (uint64_t)i_pos >= (uint64_t)p_access->info.i_size ) {
        msg_Err( p_access, "seek to far" );
        int retval = Seek( p_access, p_access->info.i_size - 1 );
        if( retval == VLC_SUCCESS ) {
            uint8_t p_buffer[2];
            Read( p_access, p_buffer, 1);
            p_access->info.b_eof  = false;
        }
        return retval;
    }
    if( Connect( p_access, i_pos ) )
    {
        msg_Err( p_access, "seek failed" );
        p_access->info.b_eof = true;
        return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}

/*****************************************************************************
 * Control:
 *****************************************************************************/
static int Control( access_t *p_access, int i_query, va_list args )
{
    access_sys_t *p_sys = p_access->p_sys;
    bool       *pb_bool;
    int64_t    *pi_64;
    vlc_meta_t *p_meta;

    switch( i_query )
    {
        /* */
        case ACCESS_CAN_SEEK:
            pb_bool = (bool*)va_arg( args, bool* );
            *pb_bool = p_sys->b_seekable;
            break;
        case ACCESS_CAN_FASTSEEK:
            pb_bool = (bool*)va_arg( args, bool* );
            *pb_bool = false;
            break;
        case ACCESS_CAN_PAUSE:
        case ACCESS_CAN_CONTROL_PACE:
            pb_bool = (bool*)va_arg( args, bool* );

#if 0       /* Disable for now until we have a clock synchro algo
             * which works with something else than MPEG over UDP */
            *pb_bool = p_sys->b_pace_control;
#endif
            *pb_bool = true;
            break;

        /* */
        case ACCESS_GET_PTS_DELAY:
            pi_64 = (int64_t*)va_arg( args, int64_t * );
            *pi_64 = (int64_t)var_GetInteger( p_access, "http-caching" ) * 1000;
            break;

        /* */
        case ACCESS_SET_PAUSE_STATE:
            break;

        case ACCESS_GET_META:
            p_meta = (vlc_meta_t*)va_arg( args, vlc_meta_t* );

            if( p_sys->psz_icy_name )
                vlc_meta_Set( p_meta, vlc_meta_Title, p_sys->psz_icy_name );
            if( p_sys->psz_icy_genre )
                vlc_meta_Set( p_meta, vlc_meta_Genre, p_sys->psz_icy_genre );
            if( p_sys->psz_icy_title )
                vlc_meta_Set( p_meta, vlc_meta_NowPlaying, p_sys->psz_icy_title );
            break;

        case ACCESS_GET_CONTENT_TYPE:
            *va_arg( args, char ** ) =
                p_sys->psz_mime ? strdup( p_sys->psz_mime ) : NULL;
            break;

        case ACCESS_GET_TITLE_INFO:
        case ACCESS_SET_TITLE:
        case ACCESS_SET_SEEKPOINT:
        case ACCESS_SET_PRIVATE_ID_STATE:
            return VLC_EGENERIC;

        default:
            msg_Warn( p_access, "unimplemented query in control" );
            return VLC_EGENERIC;

    }
    return VLC_SUCCESS;
}

/*****************************************************************************
 * Connect:
 *****************************************************************************/
static int Connect( access_t *p_access, int64_t i_tell )
{
    access_sys_t   *p_sys = p_access->p_sys;
    vlc_url_t      srv = p_sys->b_proxy ? p_sys->proxy : p_sys->url;

    /* Clean info */
    free( p_sys->psz_location );
    free( p_sys->psz_mime );
    free( p_sys->psz_pragma );

    free( p_sys->psz_icy_genre );
    free( p_sys->psz_icy_name );
    free( p_sys->psz_icy_title );


    p_sys->psz_location = NULL;
    p_sys->psz_mime = NULL;
    p_sys->psz_pragma = NULL;
    p_sys->b_mms = false;
    p_sys->b_chunked = false;
    p_sys->i_chunk = 0;
    p_sys->i_icy_meta = 0;
    p_sys->i_icy_offset = i_tell;
    p_sys->psz_icy_name = NULL;
    p_sys->psz_icy_genre = NULL;
    p_sys->psz_icy_title = NULL;
    p_sys->i_remaining = 0;
    p_sys->b_persist = false;

    p_access->info.i_size = -1;
    p_access->info.i_pos  = i_tell;
    p_access->info.b_eof  = false;

    /* Open connection */
    assert( p_sys->fd == -1 ); /* No open sockets (leaking fds is BAD) */
    p_sys->fd = net_ConnectTCP( p_access, srv.psz_host, srv.i_port );
    if( p_sys->fd == -1 )
    {
        msg_Err( p_access, "cannot connect to %s:%d", srv.psz_host, srv.i_port );
        return -1;
    }
    setsockopt (p_sys->fd, SOL_SOCKET, SO_KEEPALIVE, &(int){ 1 }, sizeof (int));

    /* Initialize TLS/SSL session */
    if( p_sys->b_ssl == true )
    {
        /* CONNECT to establish TLS tunnel through HTTP proxy */
        if( p_sys->b_proxy )
        {
            char *psz;
            unsigned i_status = 0;

            if( p_sys->i_version == 0 )
            {
                /* CONNECT is not in HTTP/1.0 */
                Disconnect( p_access );
                return -1;
            }

            net_Printf( VLC_OBJECT(p_access), p_sys->fd, NULL,
                        "CONNECT %s:%d HTTP/1.%d\r\nHost: %s:%d\r\n\r\n",
                        p_sys->url.psz_host, p_sys->url.i_port,
                        p_sys->i_version,
                        p_sys->url.psz_host, p_sys->url.i_port);

            psz = net_Gets( VLC_OBJECT(p_access), p_sys->fd, NULL );
            if( psz == NULL )
            {
                msg_Err( p_access, "cannot establish HTTP/TLS tunnel" );
                Disconnect( p_access );
                return -1;
            }

            sscanf( psz, "HTTP/%*u.%*u %3u", &i_status );
            free( psz );

            if( ( i_status / 100 ) != 2 )
            {
                msg_Err( p_access, "HTTP/TLS tunnel through proxy denied" );
                Disconnect( p_access );
                return -1;
            }

            do
            {
                psz = net_Gets( VLC_OBJECT(p_access), p_sys->fd, NULL );
                if( psz == NULL )
                {
                    msg_Err( p_access, "HTTP proxy connection failed" );
                    Disconnect( p_access );
                    return -1;
                }

                if( *psz == '\0' )
                    i_status = 0;

                free( psz );

                if( !vlc_object_alive (p_access) || p_access->b_error )
                {
                    Disconnect( p_access );
                    return -1;
                }
            }
            while( i_status );
        }

        /* TLS/SSL handshake */
        p_sys->p_tls = tls_ClientCreate( VLC_OBJECT(p_access), p_sys->fd,
                                         p_sys->url.psz_host );
        if( p_sys->p_tls == NULL )
        {
            msg_Err( p_access, "cannot establish HTTP/TLS session" );
            Disconnect( p_access );
            return -1;
        }
        p_sys->p_vs = &p_sys->p_tls->sock;
    }

    return Request( p_access, i_tell ) ? -2 : 0;
}


static int Request( access_t *p_access, int64_t i_tell )
{
    access_sys_t   *p_sys = p_access->p_sys;
    char           *psz ;
    v_socket_t     *pvs = p_sys->p_vs;
    p_sys->b_persist = false;

    p_sys->i_remaining = 0;
    if( p_sys->b_proxy )
    {
        if( p_sys->url.psz_path )
        {
            net_Printf( VLC_OBJECT(p_access), p_sys->fd, NULL,
                        "GET http://%s:%d%s HTTP/1.%d\r\n",
                        p_sys->url.psz_host, p_sys->url.i_port,
                        p_sys->url.psz_path, p_sys->i_version );
        }
        else
        {
            net_Printf( VLC_OBJECT(p_access), p_sys->fd, NULL,
                        "GET http://%s:%d/ HTTP/1.%d\r\n",
                        p_sys->url.psz_host, p_sys->url.i_port,
                        p_sys->i_version );
        }
    }
    else
    {
        const char *psz_path = p_sys->url.psz_path;
        if( !psz_path || !*psz_path )
        {
            psz_path = "/";
        }
        if( p_sys->url.i_port != (pvs ? 443 : 80) )
        {
            net_Printf( VLC_OBJECT(p_access), p_sys->fd, pvs,
                        "GET %s HTTP/1.%d\r\nHost: %s:%d\r\n",
                        psz_path, p_sys->i_version, p_sys->url.psz_host,
                        p_sys->url.i_port );
        }
        else
        {
            net_Printf( VLC_OBJECT(p_access), p_sys->fd, pvs,
                        "GET %s HTTP/1.%d\r\nHost: %s\r\n",
                        psz_path, p_sys->i_version, p_sys->url.psz_host );
        }
    }
    /* User Agent */
    net_Printf( VLC_OBJECT(p_access), p_sys->fd, pvs, "User-Agent: %s\r\n",
                p_sys->psz_user_agent );
    /* Offset */
    if( p_sys->i_version == 1 && ! p_sys->b_continuous )
    {
        p_sys->b_persist = true;
        net_Printf( VLC_OBJECT(p_access), p_sys->fd, pvs,
                    "Range: bytes=%"PRIu64"-\r\n", i_tell );
    }

    /* Cookies */
    if( p_sys->cookies )
    {
        int i;
        for( i = 0; i < vlc_array_count( p_sys->cookies ); i++ )
        {
            const char * cookie = vlc_array_item_at_index( p_sys->cookies, i );
            char * psz_cookie_content = cookie_get_content( cookie );
            char * psz_cookie_domain = cookie_get_domain( cookie );

            assert( psz_cookie_content );

            /* FIXME: This is clearly not conforming to the rfc */
            bool is_in_right_domain = (!psz_cookie_domain || strstr( p_sys->url.psz_host, psz_cookie_domain ));

            if( is_in_right_domain )
            {
                msg_Dbg( p_access, "Sending Cookie %s", psz_cookie_content );
                if( net_Printf( VLC_OBJECT(p_access), p_sys->fd, pvs, "Cookie: %s\r\n", psz_cookie_content ) < 0 )
                    msg_Err( p_access, "failed to send Cookie" );
            }
            free( psz_cookie_content );
            free( psz_cookie_domain );
        }
    }

    /* Authentication */
    if( p_sys->url.psz_username || p_sys->url.psz_password )
        AuthReply( p_access, "", &p_sys->url, &p_sys->auth );

    /* Proxy Authentication */
    if( p_sys->proxy.psz_username || p_sys->proxy.psz_password )
        AuthReply( p_access, "Proxy-", &p_sys->proxy, &p_sys->proxy_auth );

    /* ICY meta data request */
    net_Printf( VLC_OBJECT(p_access), p_sys->fd, pvs, "Icy-MetaData: 1\r\n" );


    if( net_Printf( VLC_OBJECT(p_access), p_sys->fd, pvs, "\r\n" ) < 0 )
    {
        msg_Err( p_access, "failed to send request" );
        Disconnect( p_access );
        return VLC_EGENERIC;
    }

    /* Read Answer */
    if( ( psz = net_Gets( VLC_OBJECT(p_access), p_sys->fd, pvs ) ) == NULL )
    {
        msg_Err( p_access, "failed to read answer" );
        goto error;
    }
    if( !strncmp( psz, "HTTP/1.", 7 ) )
    {
        p_sys->psz_protocol = "HTTP";
        p_sys->i_code = atoi( &psz[9] );
    }
    else if( !strncmp( psz, "ICY", 3 ) )
    {
        p_sys->psz_protocol = "ICY";
        p_sys->i_code = atoi( &psz[4] );
        p_sys->b_reconnect = true;
    }
    else
    {
        msg_Err( p_access, "invalid HTTP reply '%s'", psz );
        free( psz );
        goto error;
    }
    msg_Dbg( p_access, "protocol '%s' answer code %d",
             p_sys->psz_protocol, p_sys->i_code );
    if( !strcmp( p_sys->psz_protocol, "ICY" ) )
    {
        p_sys->b_seekable = false;
    }
    if( p_sys->i_code != 206 && p_sys->i_code != 401 )
    {
        p_sys->b_seekable = false;
    }
    /* Authentication error - We'll have to display the dialog */
    if( p_sys->i_code == 401 )
    {

    }
    /* Other fatal error */
    else if( p_sys->i_code >= 400 )
    {
        msg_Err( p_access, "error: %s", psz );
        free( psz );
        goto error;
    }
    free( psz );

    for( ;; )
    {
        char *psz = net_Gets( VLC_OBJECT(p_access), p_sys->fd, pvs );
        char *p;

        if( psz == NULL )
        {
            msg_Err( p_access, "failed to read answer" );
            goto error;
        }

        if( !vlc_object_alive (p_access) || p_access->b_error )
        {
            free( psz );
            goto error;
        }

        /* msg_Dbg( p_input, "Line=%s", psz ); */
        if( *psz == '\0' )
        {
            free( psz );
            break;
        }

        if( ( p = strchr( psz, ':' ) ) == NULL )
        {
            msg_Err( p_access, "malformed header line: %s", psz );
            free( psz );
            goto error;
        }
        *p++ = '\0';
        while( *p == ' ' ) p++;

        if( !strcasecmp( psz, "Content-Length" ) )
        {
            int64_t i_size = i_tell + (p_sys->i_remaining = atoll( p ));
            if(i_size > p_access->info.i_size) {
                p_access->info.i_size = i_size;
            }
            msg_Dbg( p_access, "this frame size=%"PRId64, p_sys->i_remaining );
        }
        else if( !strcasecmp( psz, "Content-Range" ) ) {
            int64_t i_ntell = i_tell;
            int64_t i_nend = (p_access->info.i_size > 0)?(p_access->info.i_size - 1):i_tell;
            int64_t i_nsize = p_access->info.i_size;
            sscanf(p,"bytes %"PRId64"-%"PRId64"/%"PRId64,&i_ntell,&i_nend,&i_nsize);
            if(i_nend > i_ntell ) {
                p_access->info.i_pos = i_ntell;
                p_sys->i_remaining = i_nend+1-i_ntell;
                int64_t i_size = (i_nsize > i_nend) ? i_nsize : (i_nend + 1);
                if(i_size > p_access->info.i_size) {
                    p_access->info.i_size = i_size;
                }
                msg_Dbg( p_access, "stream size=%"PRId64",pos=%"PRId64",remaining=%"PRId64,i_nsize,i_ntell,p_sys->i_remaining);
            }
        }
        else if( !strcasecmp( psz, "Connection" ) ) {
            msg_Dbg( p_access, "Connection: %s",p );
            int i = -1;
            sscanf(p, "close%n",&i);
            if( i >= 0 ) {
                p_sys->b_persist = false;
            }
        }
        else if( !strcasecmp( psz, "Location" ) )
        {
            char * psz_new_loc;

            /* This does not follow RFC 2068, but yet if the url is not absolute,
             * handle it as everyone does. */
            if( p[0] == '/' )
            {
                const char *psz_http_ext = p_sys->b_ssl ? "s" : "" ;

                if( p_sys->url.i_port == ( p_sys->b_ssl ? 443 : 80 ) )
                {
                    if( asprintf(&psz_new_loc, "http%s://%s%s", psz_http_ext,
                                 p_sys->url.psz_host, p) < 0 )
                        goto error;
                }
                else
                {
                    if( asprintf(&psz_new_loc, "http%s://%s:%d%s", psz_http_ext,
                                 p_sys->url.psz_host, p_sys->url.i_port, p) < 0 )
                        goto error;
                }
            }
            else
            {
                psz_new_loc = strdup( p );
            }

            free( p_sys->psz_location );
            p_sys->psz_location = psz_new_loc;
        }
        else if( !strcasecmp( psz, "Content-Type" ) )
        {
            free( p_sys->psz_mime );
            p_sys->psz_mime = strdup( p );
            msg_Dbg( p_access, "Content-Type: %s", p_sys->psz_mime );
        }
        else if( !strcasecmp( psz, "Content-Encoding" ) )
        {
            msg_Dbg( p_access, "Content-Encoding: %s", p );
            if( strcasecmp( p, "identity" ) )
#ifdef HAVE_ZLIB_H
                p_sys->b_compressed = true;
#else
                msg_Warn( p_access, "Compressed content not supported. Rebuild with zlib support." );
#endif
        }
        else if( !strcasecmp( psz, "Pragma" ) )
        {
            if( !strcasecmp( psz, "Pragma: features" ) )
                p_sys->b_mms = true;
            free( p_sys->psz_pragma );
            p_sys->psz_pragma = strdup( p );
            msg_Dbg( p_access, "Pragma: %s", p_sys->psz_pragma );
        }
        else if( !strcasecmp( psz, "Server" ) )
        {
            msg_Dbg( p_access, "Server: %s", p );
            if( !strncasecmp( p, "Icecast", 7 ) ||
                !strncasecmp( p, "Nanocaster", 10 ) )
            {
                /* Remember if this is Icecast
                 * we need to force demux in this case without breaking
                 *  autodetection */

                /* Let live 365 streams (nanocaster) piggyback on the icecast
                 * routine. They look very similar */

                p_sys->b_reconnect = true;
                p_sys->b_pace_control = false;
                p_sys->b_icecast = true;
            }
        }
        else if( !strcasecmp( psz, "Transfer-Encoding" ) )
        {
            msg_Dbg( p_access, "Transfer-Encoding: %s", p );
            if( !strncasecmp( p, "chunked", 7 ) )
            {
                p_sys->b_chunked = true;
            }
        }
        else if( !strcasecmp( psz, "Icy-MetaInt" ) )
        {
            msg_Dbg( p_access, "Icy-MetaInt: %s", p );
            p_sys->i_icy_meta = atoi( p );
            if( p_sys->i_icy_meta < 0 )
                p_sys->i_icy_meta = 0;
            if( p_sys->i_icy_meta > 0 )
                p_sys->b_icecast = true;

            msg_Warn( p_access, "ICY metaint=%d", p_sys->i_icy_meta );
        }
        else if( !strcasecmp( psz, "Icy-Name" ) )
        {
            free( p_sys->psz_icy_name );
            char *psz_tmp = strdup( p );
            p_sys->psz_icy_name = EnsureUTF8( psz_tmp );
            if( !p_sys->psz_icy_name )
                free( psz_tmp );
            msg_Dbg( p_access, "Icy-Name: %s", p_sys->psz_icy_name );

            p_sys->b_icecast = true; /* be on the safeside. set it here as well. */
            p_sys->b_reconnect = true;
            p_sys->b_pace_control = false;
        }
        else if( !strcasecmp( psz, "Icy-Genre" ) )
        {
            free( p_sys->psz_icy_genre );
            char *psz_tmp = strdup( p );
            p_sys->psz_icy_genre = EnsureUTF8( psz_tmp );
            if( !p_sys->psz_icy_genre )
                free( psz_tmp );
            msg_Dbg( p_access, "Icy-Genre: %s", p_sys->psz_icy_genre );
        }
        else if( !strncasecmp( psz, "Icy-Notice", 10 ) )
        {
            msg_Dbg( p_access, "Icy-Notice: %s", p );
        }
        else if( !strncasecmp( psz, "icy-", 4 ) ||
                 !strncasecmp( psz, "ice-", 4 ) ||
                 !strncasecmp( psz, "x-audiocast", 11 ) )
        {
            msg_Dbg( p_access, "Meta-Info: %s: %s", psz, p );
        }
        else if( !strcasecmp( psz, "Set-Cookie" ) )
        {
            if( p_sys->cookies )
            {
                msg_Dbg( p_access, "Accepting Cookie: %s", p );
                cookie_append( p_sys->cookies, strdup(p) );
            }
            else
                msg_Dbg( p_access, "We have a Cookie we won't remember: %s", p );
        }
        else if( !strcasecmp( psz, "www-authenticate" ) )
        {
            msg_Dbg( p_access, "Authentication header: %s", p );
            AuthParseHeader( p_access, p, &p_sys->auth );
        }
        else if( !strcasecmp( psz, "proxy-authenticate" ) )
        {
            msg_Dbg( p_access, "Proxy authentication header: %s", p );
            AuthParseHeader( p_access, p, &p_sys->proxy_auth );
        }
        else if( !strcasecmp( psz, "authentication-info" ) )
        {
            msg_Dbg( p_access, "Authentication Info header: %s", p );
            if( AuthCheckReply( p_access, p, &p_sys->url, &p_sys->auth ) )
                goto error;
        }
        else if( !strcasecmp( psz, "proxy-authentication-info" ) )
        {
            msg_Dbg( p_access, "Proxy Authentication Info header: %s", p );
            if( AuthCheckReply( p_access, p, &p_sys->proxy, &p_sys->proxy_auth ) )
                goto error;
        }

        free( psz );
    }
    /* We close the stream for zero length data, unless of course the
     * server has already promised to do this for us.
     */
    if( p_access->info.i_size != -1 && p_sys->i_remaining == 0 && p_sys->b_persist ) {
        Disconnect( p_access );
    }
    return VLC_SUCCESS;

error:
    Disconnect( p_access );
    return VLC_EGENERIC;
}

/*****************************************************************************
 * Disconnect:
 *****************************************************************************/
static void Disconnect( access_t *p_access )
{
    access_sys_t *p_sys = p_access->p_sys;

    if( p_sys->p_tls != NULL)
    {
        tls_ClientDelete( p_sys->p_tls );
        p_sys->p_tls = NULL;
        p_sys->p_vs = NULL;
    }
    if( p_sys->fd != -1)
    {
        net_Close(p_sys->fd);
        p_sys->fd = -1;
    }

}

/*****************************************************************************
 * Cookies (FIXME: we may want to rewrite that using a nice structure to hold
 * them) (FIXME: only support the "domain=" param)
 *****************************************************************************/

/* Get the NAME=VALUE part of the Cookie */
static char * cookie_get_content( const char * cookie )
{
    char * ret = strdup( cookie );
    if( !ret ) return NULL;
    char * str = ret;
    /* Look for a ';' */
    while( *str && *str != ';' ) str++;
    /* Replace it by a end-char */
    if( *str == ';' ) *str = 0;
    return ret;
}

/* Get the domain where the cookie is stored */
static char * cookie_get_domain( const char * cookie )
{
    const char * str = cookie;
    static const char domain[] = "domain=";
    if( !str )
        return NULL;
    /* Look for a ';' */
    while( *str )
    {
        if( !strncmp( str, domain, sizeof(domain) - 1 /* minus \0 */ ) )
        {
            str += sizeof(domain) - 1 /* minus \0 */;
            char * ret = strdup( str );
            /* Now remove the next ';' if present */
            char * ret_iter = ret;
            while( *ret_iter && *ret_iter != ';' ) ret_iter++;
            if( *ret_iter == ';' )
                *ret_iter = 0;
            return ret;
        }
        /* Go to next ';' field */
        while( *str && *str != ';' ) str++;
        if( *str == ';' ) str++;
        /* skip blank */
        while( *str && *str == ' ' ) str++;
    }
    return NULL;
}

/* Get NAME in the NAME=VALUE field */
static char * cookie_get_name( const char * cookie )
{
    char * ret = cookie_get_content( cookie ); /* NAME=VALUE */
    if( !ret ) return NULL;
    char * str = ret;
    while( *str && *str != '=' ) str++;
    *str = 0;
    return ret;
}

/* Add a cookie in cookies, checking to see how it should be added */
static void cookie_append( vlc_array_t * cookies, char * cookie )
{
    int i;

    if( !cookie )
        return;

    char * cookie_name = cookie_get_name( cookie );

    /* Don't send invalid cookies */
    if( !cookie_name )
        return;

    char * cookie_domain = cookie_get_domain( cookie );
    for( i = 0; i < vlc_array_count( cookies ); i++ )
    {
        char * current_cookie = vlc_array_item_at_index( cookies, i );
        char * current_cookie_name = cookie_get_name( current_cookie );
        char * current_cookie_domain = cookie_get_domain( current_cookie );

        assert( current_cookie_name );

        bool is_domain_matching = ( cookie_domain && current_cookie_domain &&
                                         !strcmp( cookie_domain, current_cookie_domain ) );

        if( is_domain_matching && !strcmp( cookie_name, current_cookie_name )  )
        {
            /* Remove previous value for this cookie */
            free( current_cookie );
            vlc_array_remove( cookies, i );

            /* Clean */
            free( current_cookie_name );
            free( current_cookie_domain );
            break;
        }
        free( current_cookie_name );
        free( current_cookie_domain );
    }
    free( cookie_name );
    free( cookie_domain );
    vlc_array_append( cookies, cookie );
}

/*****************************************************************************
 * "RFC 2617: Basic and Digest Access Authentication" header parsing
 *****************************************************************************/
static char *AuthGetParam( const char *psz_header, const char *psz_param )
{
    char psz_what[strlen(psz_param)+3];
    sprintf( psz_what, "%s=\"", psz_param );
    psz_header = strstr( psz_header, psz_what );
    if( psz_header )
    {
        const char *psz_end;
        psz_header += strlen( psz_what );
        psz_end = strchr( psz_header, '"' );
        if( !psz_end ) /* Invalid since we should have a closing quote */
            return strdup( psz_header );
        return strndup( psz_header, psz_end - psz_header );
    }
    else
    {
        return NULL;
    }
}

static char *AuthGetParamNoQuotes( const char *psz_header, const char *psz_param )
{
    char psz_what[strlen(psz_param)+2];
    sprintf( psz_what, "%s=", psz_param );
    psz_header = strstr( psz_header, psz_what );
    if( psz_header )
    {
        const char *psz_end;
        psz_header += strlen( psz_what );
        psz_end = strchr( psz_header, ',' );
        /* XXX: Do we need to filter out trailing space between the value and
         * the comma/end of line? */
        if( !psz_end ) /* Can be valid if this is the last parameter */
            return strdup( psz_header );
        return strndup( psz_header, psz_end - psz_header );
    }
    else
    {
        return NULL;
    }
}

static void AuthParseHeader( access_t *p_access, const char *psz_header,
                             http_auth_t *p_auth )
{
    /* FIXME: multiple auth methods can be listed (comma seperated) */

    /* 2 Basic Authentication Scheme */
    if( !strncasecmp( psz_header, "Basic ", strlen( "Basic " ) ) )
    {
        msg_Dbg( p_access, "Using Basic Authentication" );
        psz_header += strlen( "Basic " );
        p_auth->psz_realm = AuthGetParam( psz_header, "realm" );
        if( !p_auth->psz_realm )
            msg_Warn( p_access, "Basic Authentication: "
                      "Mandatory 'realm' parameter is missing" );
    }
    /* 3 Digest Access Authentication Scheme */
    else if( !strncasecmp( psz_header, "Digest ", strlen( "Digest " ) ) )
    {
        msg_Dbg( p_access, "Using Digest Access Authentication" );
        if( p_auth->psz_nonce ) return; /* FIXME */
        psz_header += strlen( "Digest " );
        p_auth->psz_realm = AuthGetParam( psz_header, "realm" );
        p_auth->psz_domain = AuthGetParam( psz_header, "domain" );
        p_auth->psz_nonce = AuthGetParam( psz_header, "nonce" );
        p_auth->psz_opaque = AuthGetParam( psz_header, "opaque" );
        p_auth->psz_stale = AuthGetParamNoQuotes( psz_header, "stale" );
        p_auth->psz_algorithm = AuthGetParamNoQuotes( psz_header, "algorithm" );
        p_auth->psz_qop = AuthGetParam( psz_header, "qop" );
        p_auth->i_nonce = 0;
        /* printf("realm: |%s|\ndomain: |%s|\nnonce: |%s|\nopaque: |%s|\n"
                  "stale: |%s|\nalgorithm: |%s|\nqop: |%s|\n",
                  p_auth->psz_realm,p_auth->psz_domain,p_auth->psz_nonce,
                  p_auth->psz_opaque,p_auth->psz_stale,p_auth->psz_algorithm,
                  p_auth->psz_qop); */
        if( !p_auth->psz_realm )
            msg_Warn( p_access, "Digest Access Authentication: "
                      "Mandatory 'realm' parameter is missing" );
        if( !p_auth->psz_nonce )
            msg_Warn( p_access, "Digest Access Authentication: "
                      "Mandatory 'nonce' parameter is missing" );
        if( p_auth->psz_qop ) /* FIXME: parse the qop list */
        {
            char *psz_tmp = strchr( p_auth->psz_qop, ',' );
            if( psz_tmp ) *psz_tmp = '\0';
        }
    }
    else
    {
        const char *psz_end = strchr( psz_header, ' ' );
        if( psz_end )
            msg_Warn( p_access, "Unknown authentication scheme: '%*s'",
                      (int)(psz_end - psz_header), psz_header );
        else
            msg_Warn( p_access, "Unknown authentication scheme: '%s'",
                      psz_header );
    }
}

static char *AuthDigest( access_t *p_access, vlc_url_t *p_url,
                         http_auth_t *p_auth, const char *psz_method )
{
    (void)p_access;
    const char *psz_username = p_url->psz_username ? p_url->psz_username : "";
    const char *psz_password = p_url->psz_password ? p_url->psz_password : "";

    char *psz_HA1 = NULL;
    char *psz_HA2 = NULL;
    char *psz_response = NULL;
    struct md5_s md5;

    /* H(A1) */
    if( p_auth->psz_HA1 )
    {
        psz_HA1 = strdup( p_auth->psz_HA1 );
        if( !psz_HA1 ) goto error;
    }
    else
    {
        InitMD5( &md5 );
        AddMD5( &md5, psz_username, strlen( psz_username ) );
        AddMD5( &md5, ":", 1 );
        AddMD5( &md5, p_auth->psz_realm, strlen( p_auth->psz_realm ) );
        AddMD5( &md5, ":", 1 );
        AddMD5( &md5, psz_password, strlen( psz_password ) );
        EndMD5( &md5 );

        psz_HA1 = psz_md5_hash( &md5 );
        if( !psz_HA1 ) goto error;

        if( p_auth->psz_algorithm
            && !strcmp( p_auth->psz_algorithm, "MD5-sess" ) )
        {
            InitMD5( &md5 );
            AddMD5( &md5, psz_HA1, 32 );
            free( psz_HA1 );
            AddMD5( &md5, ":", 1 );
            AddMD5( &md5, p_auth->psz_nonce, strlen( p_auth->psz_nonce ) );
            AddMD5( &md5, ":", 1 );
            AddMD5( &md5, p_auth->psz_cnonce, strlen( p_auth->psz_cnonce ) );
            EndMD5( &md5 );

            psz_HA1 = psz_md5_hash( &md5 );
            if( !psz_HA1 ) goto error;
            p_auth->psz_HA1 = strdup( psz_HA1 );
            if( !p_auth->psz_HA1 ) goto error;
        }
    }

    /* H(A2) */
    InitMD5( &md5 );
    if( *psz_method )
        AddMD5( &md5, psz_method, strlen( psz_method ) );
    AddMD5( &md5, ":", 1 );
    if( p_url->psz_path )
        AddMD5( &md5, p_url->psz_path, strlen( p_url->psz_path ) );
    else
        AddMD5( &md5, "/", 1 );
    if( p_auth->psz_qop && !strcmp( p_auth->psz_qop, "auth-int" ) )
    {
        char *psz_ent;
        struct md5_s ent;
        InitMD5( &ent );
        AddMD5( &ent, "", 0 ); /* XXX: entity-body. should be ok for GET */
        EndMD5( &ent );
        psz_ent = psz_md5_hash( &ent );
        if( !psz_ent ) goto error;
        AddMD5( &md5, ":", 1 );
        AddMD5( &md5, psz_ent, 32 );
        free( psz_ent );
    }
    EndMD5( &md5 );
    psz_HA2 = psz_md5_hash( &md5 );
    if( !psz_HA2 ) goto error;

    /* Request digest */
    InitMD5( &md5 );
    AddMD5( &md5, psz_HA1, 32 );
    AddMD5( &md5, ":", 1 );
    AddMD5( &md5, p_auth->psz_nonce, strlen( p_auth->psz_nonce ) );
    AddMD5( &md5, ":", 1 );
    if( p_auth->psz_qop
        && ( !strcmp( p_auth->psz_qop, "auth" )
             || !strcmp( p_auth->psz_qop, "auth-int" ) ) )
    {
        char psz_inonce[9];
        snprintf( psz_inonce, 9, "%08x", p_auth->i_nonce );
        AddMD5( &md5, psz_inonce, 8 );
        AddMD5( &md5, ":", 1 );
        AddMD5( &md5, p_auth->psz_cnonce, strlen( p_auth->psz_cnonce ) );
        AddMD5( &md5, ":", 1 );
        AddMD5( &md5, p_auth->psz_qop, strlen( p_auth->psz_qop ) );
        AddMD5( &md5, ":", 1 );
    }
    AddMD5( &md5, psz_HA2, 32 );
    EndMD5( &md5 );
    psz_response = psz_md5_hash( &md5 );

    error:
        free( psz_HA1 );
        free( psz_HA2 );
        return psz_response;
}


static void AuthReply( access_t *p_access, const char *psz_prefix,
                       vlc_url_t *p_url, http_auth_t *p_auth )
{
    access_sys_t *p_sys = p_access->p_sys;
    v_socket_t     *pvs = p_sys->p_vs;

    const char *psz_username = p_url->psz_username ? p_url->psz_username : "";
    const char *psz_password = p_url->psz_password ? p_url->psz_password : "";

    if( p_auth->psz_nonce )
    {
        /* Digest Access Authentication */
        char *psz_response;

        if(    p_auth->psz_algorithm
            && strcmp( p_auth->psz_algorithm, "MD5" )
            && strcmp( p_auth->psz_algorithm, "MD5-sess" ) )
        {
            msg_Err( p_access, "Digest Access Authentication: "
                     "Unknown algorithm '%s'", p_auth->psz_algorithm );
            return;
        }

        if( p_auth->psz_qop || !p_auth->psz_cnonce )
        {
            /* FIXME: needs to be really random to prevent man in the middle
             * attacks */
            free( p_auth->psz_cnonce );
            p_auth->psz_cnonce = strdup( "Some random string FIXME" );
        }
        p_auth->i_nonce ++;

        psz_response = AuthDigest( p_access, p_url, p_auth, "GET" );
        if( !psz_response ) return;

        net_Printf( VLC_OBJECT(p_access), p_sys->fd, pvs,
                    "%sAuthorization: Digest "
                    /* Mandatory parameters */
                    "username=\"%s\", "
                    "realm=\"%s\", "
                    "nonce=\"%s\", "
                    "uri=\"%s\", "
                    "response=\"%s\", "
                    /* Optional parameters */
                    "%s%s%s" /* algorithm */
                    "%s%s%s" /* cnonce */
                    "%s%s%s" /* opaque */
                    "%s%s%s" /* message qop */
                    "%s%08x%s" /* nonce count */
                    "\r\n",
                    /* Mandatory parameters */
                    psz_prefix,
                    psz_username,
                    p_auth->psz_realm,
                    p_auth->psz_nonce,
                    p_url->psz_path ? p_url->psz_path : "/",
                    psz_response,
                    /* Optional parameters */
                    p_auth->psz_algorithm ? "algorithm=\"" : "",
                    p_auth->psz_algorithm ? p_auth->psz_algorithm : "",
                    p_auth->psz_algorithm ? "\", " : "",
                    p_auth->psz_cnonce ? "cnonce=\"" : "",
                    p_auth->psz_cnonce ? p_auth->psz_cnonce : "",
                    p_auth->psz_cnonce ? "\", " : "",
                    p_auth->psz_opaque ? "opaque=\"" : "",
                    p_auth->psz_opaque ? p_auth->psz_opaque : "",
                    p_auth->psz_opaque ? "\", " : "",
                    p_auth->psz_qop ? "qop=\"" : "",
                    p_auth->psz_qop ? p_auth->psz_qop : "",
                    p_auth->psz_qop ? "\", " : "",
                    p_auth->i_nonce ? "nc=\"" : "uglyhack=\"", /* Will be parsed as an unhandled extension */
                    p_auth->i_nonce,
                    p_auth->i_nonce ? "\"" : "\""
                  );

        free( psz_response );
    }
    else
    {
        /* Basic Access Authentication */
        char buf[strlen( psz_username ) + strlen( psz_password ) + 2];
        char *b64;

        snprintf( buf, sizeof( buf ), "%s:%s", psz_username, psz_password );
        b64 = vlc_b64_encode( buf );

        if( b64 != NULL )
        {
             net_Printf( VLC_OBJECT(p_access), p_sys->fd, pvs,
                         "%sAuthorization: Basic %s\r\n", psz_prefix, b64 );
             free( b64 );
        }
    }
}

static int AuthCheckReply( access_t *p_access, const char *psz_header,
                           vlc_url_t *p_url, http_auth_t *p_auth )
{
    int i_ret = VLC_EGENERIC;
    char *psz_nextnonce = AuthGetParam( psz_header, "nextnonce" );
    char *psz_qop = AuthGetParamNoQuotes( psz_header, "qop" );
    char *psz_rspauth = AuthGetParam( psz_header, "rspauth" );
    char *psz_cnonce = AuthGetParam( psz_header, "cnonce" );
    char *psz_nc = AuthGetParamNoQuotes( psz_header, "nc" );

    if( psz_cnonce )
    {
        char *psz_digest;

        if( strcmp( psz_cnonce, p_auth->psz_cnonce ) )
        {
            msg_Err( p_access, "HTTP Digest Access Authentication: server replied with a different client nonce value." );
            goto error;
        }

        if( psz_nc )
        {
            int i_nonce;
            i_nonce = strtol( psz_nc, NULL, 16 );
            if( i_nonce != p_auth->i_nonce )
            {
                msg_Err( p_access, "HTTP Digest Access Authentication: server replied with a different nonce count value." );
                goto error;
            }
        }

        if( psz_qop && p_auth->psz_qop && strcmp( psz_qop, p_auth->psz_qop ) )
            msg_Warn( p_access, "HTTP Digest Access Authentication: server replied using a different 'quality of protection' option" );

        /* All the clear text values match, let's now check the response
         * digest */
        psz_digest = AuthDigest( p_access, p_url, p_auth, "" );
        if( strcmp( psz_digest, psz_rspauth ) )
        {
            msg_Err( p_access, "HTTP Digest Access Authentication: server replied with an invalid response digest (expected value: %s).", psz_digest );
            free( psz_digest );
            goto error;
        }
        free( psz_digest );
    }

    if( psz_nextnonce )
    {
        free( p_auth->psz_nonce );
        p_auth->psz_nonce = psz_nextnonce;
        psz_nextnonce = NULL;
    }

    i_ret = VLC_SUCCESS;
    error:
        free( psz_nextnonce );
        free( psz_qop );
        free( psz_rspauth );
        free( psz_cnonce );
        free( psz_nc );

    return i_ret;
}

static void AuthReset( http_auth_t *p_auth )
{
    FREENULL( p_auth->psz_realm );
    FREENULL( p_auth->psz_domain );
    FREENULL( p_auth->psz_nonce );
    FREENULL( p_auth->psz_opaque );
    FREENULL( p_auth->psz_stale );
    FREENULL( p_auth->psz_algorithm );
    FREENULL( p_auth->psz_qop );
    p_auth->i_nonce = 0;
    FREENULL( p_auth->psz_cnonce );
    FREENULL( p_auth->psz_HA1 );
}
