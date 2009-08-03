/*****************************************************************************
 * rtp.c: rtp stream output module
 *****************************************************************************
 * Copyright (C) 2003-2004 the VideoLAN team
 * Copyright © 2007-2008 Rémi Denis-Courmont
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
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
#include <vlc_sout.h>
#include <vlc_block.h>

#include <vlc_httpd.h>
#include <vlc_url.h>
#include <vlc_network.h>
#include <vlc_charset.h>
#include <vlc_strings.h>
#include <vlc_rand.h>
#include <srtp.h>

#include "rtp.h"

#ifdef HAVE_UNISTD_H
#   include <sys/types.h>
#   include <unistd.h>
#   include <fcntl.h>
#   include <sys/stat.h>
#endif
#ifdef HAVE_LINUX_DCCP_H
#   include <linux/dccp.h>
#endif
#ifndef IPPROTO_DCCP
# define IPPROTO_DCCP 33
#endif
#ifndef IPPROTO_UDPLITE
# define IPPROTO_UDPLITE 136
#endif

#include <errno.h>

#include <assert.h>

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

#define DEST_TEXT N_("Destination")
#define DEST_LONGTEXT N_( \
    "This is the output URL that will be used." )
#define SDP_TEXT N_("SDP")
#define SDP_LONGTEXT N_( \
    "This allows you to specify how the SDP (Session Descriptor) for this RTP "\
    "session will be made available. You must use an url: http://location to " \
    "access the SDP via HTTP, rtsp://location for RTSP access, and sap:// " \
    "for the SDP to be announced via SAP." )
#define SAP_TEXT N_("SAP announcing")
#define SAP_LONGTEXT N_("Announce this session with SAP.")
#define MUX_TEXT N_("Muxer")
#define MUX_LONGTEXT N_( \
    "This allows you to specify the muxer used for the streaming output. " \
    "Default is to use no muxer (standard RTP stream)." )

#define NAME_TEXT N_("Session name")
#define NAME_LONGTEXT N_( \
    "This is the name of the session that will be announced in the SDP " \
    "(Session Descriptor)." )
#define DESC_TEXT N_("Session description")
#define DESC_LONGTEXT N_( \
    "This allows you to give a short description with details about the stream, " \
    "that will be announced in the SDP (Session Descriptor)." )
#define URL_TEXT N_("Session URL")
#define URL_LONGTEXT N_( \
    "This allows you to give an URL with more details about the stream " \
    "(often the website of the streaming organization), that will " \
    "be announced in the SDP (Session Descriptor)." )
#define EMAIL_TEXT N_("Session email")
#define EMAIL_LONGTEXT N_( \
    "This allows you to give a contact mail address for the stream, that will " \
    "be announced in the SDP (Session Descriptor)." )
#define PHONE_TEXT N_("Session phone number")
#define PHONE_LONGTEXT N_( \
    "This allows you to give a contact telephone number for the stream, that will " \
    "be announced in the SDP (Session Descriptor)." )

#define PORT_TEXT N_("Port")
#define PORT_LONGTEXT N_( \
    "This allows you to specify the base port for the RTP streaming." )
#define PORT_AUDIO_TEXT N_("Audio port")
#define PORT_AUDIO_LONGTEXT N_( \
    "This allows you to specify the default audio port for the RTP streaming." )
#define PORT_VIDEO_TEXT N_("Video port")
#define PORT_VIDEO_LONGTEXT N_( \
    "This allows you to specify the default video port for the RTP streaming." )

#define TTL_TEXT N_("Hop limit (TTL)")
#define TTL_LONGTEXT N_( \
    "This is the hop limit (also known as \"Time-To-Live\" or TTL) of " \
    "the multicast packets sent by the stream output (-1 = use operating " \
    "system built-in default).")

#define RTCP_MUX_TEXT N_("RTP/RTCP multiplexing")
#define RTCP_MUX_LONGTEXT N_( \
    "This sends and receives RTCP packet multiplexed over the same port " \
    "as RTP packets." )

#define PROTO_TEXT N_("Transport protocol")
#define PROTO_LONGTEXT N_( \
    "This selects which transport protocol to use for RTP." )

#define SRTP_KEY_TEXT N_("SRTP key (hexadecimal)")
#define SRTP_KEY_LONGTEXT N_( \
    "RTP packets will be integrity-protected and ciphered "\
    "with this Secure RTP master shared secret key.")

#define SRTP_SALT_TEXT N_("SRTP salt (hexadecimal)")
#define SRTP_SALT_LONGTEXT N_( \
    "Secure RTP requires a (non-secret) master salt value.")

static const char *const ppsz_protos[] = {
    "dccp", "sctp", "tcp", "udp", "udplite",
};

static const char *const ppsz_protocols[] = {
    "DCCP", "SCTP", "TCP", "UDP", "UDP-Lite",
};

#define RFC3016_TEXT N_("MP4A LATM")
#define RFC3016_LONGTEXT N_( \
    "This allows you to stream MPEG4 LATM audio streams (see RFC3016)." )

static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

#define SOUT_CFG_PREFIX "sout-rtp-"
#define MAX_EMPTY_BLOCKS 200

vlc_module_begin ()
    set_shortname( N_("RTP"))
    set_description( N_("RTP stream output") )
    set_capability( "sout stream", 0 )
    add_shortcut( "rtp" )
    set_category( CAT_SOUT )
    set_subcategory( SUBCAT_SOUT_STREAM )

    add_string( SOUT_CFG_PREFIX "dst", "", NULL, DEST_TEXT,
                DEST_LONGTEXT, true )
    add_string( SOUT_CFG_PREFIX "sdp", "", NULL, SDP_TEXT,
                SDP_LONGTEXT, true )
    add_string( SOUT_CFG_PREFIX "mux", "", NULL, MUX_TEXT,
                MUX_LONGTEXT, true )
    add_bool( SOUT_CFG_PREFIX "sap", false, NULL, SAP_TEXT, SAP_LONGTEXT,
              true )

    add_string( SOUT_CFG_PREFIX "name", "", NULL, NAME_TEXT,
                NAME_LONGTEXT, true )
    add_string( SOUT_CFG_PREFIX "description", "", NULL, DESC_TEXT,
                DESC_LONGTEXT, true )
    add_string( SOUT_CFG_PREFIX "url", "", NULL, URL_TEXT,
                URL_LONGTEXT, true )
    add_string( SOUT_CFG_PREFIX "email", "", NULL, EMAIL_TEXT,
                EMAIL_LONGTEXT, true )
    add_string( SOUT_CFG_PREFIX "phone", "", NULL, PHONE_TEXT,
                PHONE_LONGTEXT, true )

    add_string( SOUT_CFG_PREFIX "proto", "udp", NULL, PROTO_TEXT,
                PROTO_LONGTEXT, false )
        change_string_list( ppsz_protos, ppsz_protocols, NULL )
    add_integer( SOUT_CFG_PREFIX "port", 5004, NULL, PORT_TEXT,
                 PORT_LONGTEXT, true )
    add_integer( SOUT_CFG_PREFIX "port-audio", 0, NULL, PORT_AUDIO_TEXT,
                 PORT_AUDIO_LONGTEXT, true )
    add_integer( SOUT_CFG_PREFIX "port-video", 0, NULL, PORT_VIDEO_TEXT,
                 PORT_VIDEO_LONGTEXT, true )

    add_integer( SOUT_CFG_PREFIX "ttl", -1, NULL, TTL_TEXT,
                 TTL_LONGTEXT, true )
    add_bool( SOUT_CFG_PREFIX "rtcp-mux", false, NULL,
              RTCP_MUX_TEXT, RTCP_MUX_LONGTEXT, false )

    add_string( SOUT_CFG_PREFIX "key", "", NULL,
                SRTP_KEY_TEXT, SRTP_KEY_LONGTEXT, false )
    add_string( SOUT_CFG_PREFIX "salt", "", NULL,
                SRTP_SALT_TEXT, SRTP_SALT_LONGTEXT, false )

    add_bool( SOUT_CFG_PREFIX "mp4a-latm", 0, NULL, RFC3016_TEXT,
                 RFC3016_LONGTEXT, false )

    set_callbacks( Open, Close )
vlc_module_end ()

/*****************************************************************************
 * Exported prototypes
 *****************************************************************************/
static const char *const ppsz_sout_options[] = {
    "dst", "name", "port", "port-audio", "port-video", "*sdp", "ttl", "mux",
    "sap", "description", "url", "email", "phone",
    "proto", "rtcp-mux", "key", "salt",
    "mp4a-latm", NULL
};

static sout_stream_id_t *Add ( sout_stream_t *, es_format_t * );
static int               Del ( sout_stream_t *, sout_stream_id_t * );
static int               Send( sout_stream_t *, sout_stream_id_t *,
                               block_t* );
static sout_stream_id_t *MuxAdd ( sout_stream_t *, es_format_t * );
static int               MuxDel ( sout_stream_t *, sout_stream_id_t * );
static int               MuxSend( sout_stream_t *, sout_stream_id_t *,
                                  block_t* );

static sout_access_out_t *GrabberCreate( sout_stream_t *p_sout );
static void* ThreadSend( vlc_object_t *p_this );

static void SDPHandleUrl( sout_stream_t *, const char * );

static int SapSetup( sout_stream_t *p_stream );
static int FileSetup( sout_stream_t *p_stream );
static int HttpSetup( sout_stream_t *p_stream, const vlc_url_t * );

struct sout_stream_sys_t
{
    /* SDP */
    char    *psz_sdp;
    vlc_mutex_t  lock_sdp;

    /* SDP to disk */
    bool b_export_sdp_file;
    char *psz_sdp_file;

    /* SDP via SAP */
    bool b_export_sap;
    session_descriptor_t *p_session;

    /* SDP via HTTP */
    httpd_host_t *p_httpd_host;
    httpd_file_t *p_httpd_file;

    /* RTSP */
    rtsp_stream_t *rtsp;

    /* */
    char     *psz_destination;
    uint32_t  payload_bitmap;
    uint16_t  i_port;
    uint16_t  i_port_audio;
    uint16_t  i_port_video;
    uint8_t   proto;
    bool      rtcp_mux;
    int       i_ttl:9;
    bool      b_latm;

    /* in case we do TS/PS over rtp */
    sout_mux_t        *p_mux;
    sout_access_out_t *p_grab;
    block_t           *packet;

    /* */
    vlc_mutex_t      lock_es;
    int              i_es;
    sout_stream_id_t **es;
};

typedef int (*pf_rtp_packetizer_t)( sout_stream_id_t *, block_t * );

typedef struct rtp_sink_t
{
    int rtp_fd;
    rtcp_sender_t *rtcp;
} rtp_sink_t;

struct sout_stream_id_t
{
    VLC_COMMON_MEMBERS

    sout_stream_t *p_stream;
    /* rtp field */
    uint16_t    i_sequence;
    uint8_t     i_payload_type;
    uint8_t     ssrc[4];

    /* for sdp */
    const char  *psz_enc;
    char        *psz_fmtp;
    int          i_clock_rate;
    int          i_port;
    int          i_cat;
    int          i_channels;
    int          i_bitrate;

    /* Packetizer specific fields */
    int                 i_mtu;
    srtp_session_t     *srtp;
    pf_rtp_packetizer_t pf_packetize;

    /* Packets sinks */
    vlc_mutex_t       lock_sink;
    int               sinkc;
    rtp_sink_t       *sinkv;
    rtsp_stream_id_t *rtsp_id;
    int              *listen_fd;

    block_fifo_t     *p_fifo;
    int64_t           i_caching;
};

/*****************************************************************************
 * Open:
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    sout_stream_t       *p_stream = (sout_stream_t*)p_this;
    sout_instance_t     *p_sout = p_stream->p_sout;
    sout_stream_sys_t   *p_sys = NULL;
    config_chain_t      *p_cfg = NULL;
    char                *psz;
    bool          b_rtsp = false;

    config_ChainParse( p_stream, SOUT_CFG_PREFIX,
                       ppsz_sout_options, p_stream->p_cfg );

    p_sys = malloc( sizeof( sout_stream_sys_t ) );
    if( p_sys == NULL )
        return VLC_ENOMEM;

    p_sys->psz_destination = var_GetNonEmptyString( p_stream, SOUT_CFG_PREFIX "dst" );

    p_sys->i_port       = var_GetInteger( p_stream, SOUT_CFG_PREFIX "port" );
    p_sys->i_port_audio = var_GetInteger( p_stream, SOUT_CFG_PREFIX "port-audio" );
    p_sys->i_port_video = var_GetInteger( p_stream, SOUT_CFG_PREFIX "port-video" );
    p_sys->rtcp_mux     = var_GetBool( p_stream, SOUT_CFG_PREFIX "rtcp-mux" );

    p_sys->psz_sdp_file = NULL;

    if( p_sys->i_port_audio && p_sys->i_port_video == p_sys->i_port_audio )
    {
        msg_Err( p_stream, "audio and video RTP port must be distinct" );
        free( p_sys->psz_destination );
        free( p_sys );
        return VLC_EGENERIC;
    }

    for( p_cfg = p_stream->p_cfg; p_cfg != NULL; p_cfg = p_cfg->p_next )
    {
        if( !strcmp( p_cfg->psz_name, "sdp" )
         && ( p_cfg->psz_value != NULL )
         && !strncasecmp( p_cfg->psz_value, "rtsp:", 5 ) )
        {
            b_rtsp = true;
            break;
        }
    }
    if( !b_rtsp )
    {
        psz = var_GetNonEmptyString( p_stream, SOUT_CFG_PREFIX "sdp" );
        if( psz != NULL )
        {
            if( !strncasecmp( psz, "rtsp:", 5 ) )
                b_rtsp = true;
            free( psz );
        }
    }

    /* Transport protocol */
    p_sys->proto = IPPROTO_UDP;
    psz = var_GetNonEmptyString (p_stream, SOUT_CFG_PREFIX"proto");

    if ((psz == NULL) || !strcasecmp (psz, "udp"))
        (void)0; /* default */
    else
    if (!strcasecmp (psz, "dccp"))
    {
        p_sys->proto = IPPROTO_DCCP;
        p_sys->rtcp_mux = true; /* Force RTP/RTCP mux */
    }
#if 0
    else
    if (!strcasecmp (psz, "sctp"))
    {
        p_sys->proto = IPPROTO_TCP;
        p_sys->rtcp_mux = true; /* Force RTP/RTCP mux */
    }
#endif
#if 0
    else
    if (!strcasecmp (psz, "tcp"))
    {
        p_sys->proto = IPPROTO_TCP;
        p_sys->rtcp_mux = true; /* Force RTP/RTCP mux */
    }
#endif
    else
    if (!strcasecmp (psz, "udplite") || !strcasecmp (psz, "udp-lite"))
        p_sys->proto = IPPROTO_UDPLITE;
    else
        msg_Warn (p_this, "unknown or unsupported transport protocol \"%s\"",
                  psz);
    free (psz);
    var_Create (p_this, "dccp-service", VLC_VAR_STRING);

    if( ( p_sys->psz_destination == NULL ) && !b_rtsp )
    {
        msg_Err( p_stream, "missing destination and not in RTSP mode" );
        free( p_sys );
        return VLC_EGENERIC;
    }

    p_sys->i_ttl = var_GetInteger( p_stream, SOUT_CFG_PREFIX "ttl" );
    if( p_sys->i_ttl == -1 )
    {
        /* Normally, we should let the default hop limit up to the core,
         * but we have to know it to build our SDP properly, which is why
         * we ask the core. FIXME: broken when neither sout-rtp-ttl nor
         * ttl are set. */
        p_sys->i_ttl = config_GetInt( p_stream, "ttl" );
    }

    p_sys->b_latm = var_GetBool( p_stream, SOUT_CFG_PREFIX "mp4a-latm" );

    p_sys->payload_bitmap = 0;
    p_sys->i_es = 0;
    p_sys->es   = NULL;
    p_sys->rtsp = NULL;
    p_sys->psz_sdp = NULL;

    p_sys->b_export_sap = false;
    p_sys->b_export_sdp_file = false;
    p_sys->p_session = NULL;

    p_sys->p_httpd_host = NULL;
    p_sys->p_httpd_file = NULL;

    p_stream->p_sys     = p_sys;

    vlc_mutex_init( &p_sys->lock_sdp );
    vlc_mutex_init( &p_sys->lock_es );

    psz = var_GetNonEmptyString( p_stream, SOUT_CFG_PREFIX "mux" );
    if( psz != NULL )
    {
        sout_stream_id_t *id;

        /* Check muxer type */
        if( strncasecmp( psz, "ps", 2 )
         && strncasecmp( psz, "mpeg1", 5 )
         && strncasecmp( psz, "ts", 2 ) )
        {
            msg_Err( p_stream, "unsupported muxer type for RTP (only TS/PS)" );
            free( psz );
            vlc_mutex_destroy( &p_sys->lock_sdp );
            vlc_mutex_destroy( &p_sys->lock_es );
            free( p_sys->psz_destination );
            free( p_sys );
            return VLC_EGENERIC;
        }

        p_sys->p_grab = GrabberCreate( p_stream );
        p_sys->p_mux = sout_MuxNew( p_sout, psz, p_sys->p_grab );
        free( psz );

        if( p_sys->p_mux == NULL )
        {
            msg_Err( p_stream, "cannot create muxer" );
            sout_AccessOutDelete( p_sys->p_grab );
            vlc_mutex_destroy( &p_sys->lock_sdp );
            vlc_mutex_destroy( &p_sys->lock_es );
            free( p_sys->psz_destination );
            free( p_sys );
            return VLC_EGENERIC;
        }

        id = Add( p_stream, NULL );
        if( id == NULL )
        {
            sout_MuxDelete( p_sys->p_mux );
            sout_AccessOutDelete( p_sys->p_grab );
            vlc_mutex_destroy( &p_sys->lock_sdp );
            vlc_mutex_destroy( &p_sys->lock_es );
            free( p_sys->psz_destination );
            free( p_sys );
            return VLC_EGENERIC;
        }

        p_sys->packet = NULL;

        p_stream->pf_add  = MuxAdd;
        p_stream->pf_del  = MuxDel;
        p_stream->pf_send = MuxSend;
    }
    else
    {
        p_sys->p_mux    = NULL;
        p_sys->p_grab   = NULL;

        p_stream->pf_add    = Add;
        p_stream->pf_del    = Del;
        p_stream->pf_send   = Send;
    }

    if( var_GetBool( p_stream, SOUT_CFG_PREFIX"sap" ) )
        SDPHandleUrl( p_stream, "sap" );

    psz = var_GetNonEmptyString( p_stream, SOUT_CFG_PREFIX "sdp" );
    if( psz != NULL )
    {
        config_chain_t *p_cfg;

        SDPHandleUrl( p_stream, psz );

        for( p_cfg = p_stream->p_cfg; p_cfg != NULL; p_cfg = p_cfg->p_next )
        {
            if( !strcmp( p_cfg->psz_name, "sdp" ) )
            {
                if( p_cfg->psz_value == NULL || *p_cfg->psz_value == '\0' )
                    continue;

                /* needed both :sout-rtp-sdp= and rtp{sdp=} can be used */
                if( !strcmp( p_cfg->psz_value, psz ) )
                    continue;

                SDPHandleUrl( p_stream, p_cfg->psz_value );
            }
        }
        free( psz );
    }

    /* update p_sout->i_out_pace_nocontrol */
    p_stream->p_sout->i_out_pace_nocontrol++;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close:
 *****************************************************************************/
static void Close( vlc_object_t * p_this )
{
    sout_stream_t     *p_stream = (sout_stream_t*)p_this;
    sout_stream_sys_t *p_sys = p_stream->p_sys;

    /* update p_sout->i_out_pace_nocontrol */
    p_stream->p_sout->i_out_pace_nocontrol--;

    if( p_sys->p_mux )
    {
        assert( p_sys->i_es == 1 );

        sout_MuxDelete( p_sys->p_mux );
        Del( p_stream, p_sys->es[0] );
        sout_AccessOutDelete( p_sys->p_grab );

        if( p_sys->packet )
        {
            block_Release( p_sys->packet );
        }
        if( p_sys->b_export_sap )
        {
            p_sys->p_mux = NULL;
            SapSetup( p_stream );
        }
    }

    if( p_sys->rtsp != NULL )
        RtspUnsetup( p_sys->rtsp );

    vlc_mutex_destroy( &p_sys->lock_sdp );
    vlc_mutex_destroy( &p_sys->lock_es );

    if( p_sys->p_httpd_file )
        httpd_FileDelete( p_sys->p_httpd_file );

    if( p_sys->p_httpd_host )
        httpd_HostDelete( p_sys->p_httpd_host );

    free( p_sys->psz_sdp );

    if( p_sys->b_export_sdp_file )
    {
#ifdef HAVE_UNISTD_H
        unlink( p_sys->psz_sdp_file );
#endif
        free( p_sys->psz_sdp_file );
    }
    free( p_sys->psz_destination );
    free( p_sys );
}

/*****************************************************************************
 * SDPHandleUrl:
 *****************************************************************************/
static void SDPHandleUrl( sout_stream_t *p_stream, const char *psz_url )
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;
    vlc_url_t url;

    vlc_UrlParse( &url, psz_url, 0 );
    if( url.psz_protocol && !strcasecmp( url.psz_protocol, "http" ) )
    {
        if( p_sys->p_httpd_file )
        {
            msg_Err( p_stream, "you can use sdp=http:// only once" );
            goto out;
        }

        if( HttpSetup( p_stream, &url ) )
        {
            msg_Err( p_stream, "cannot export SDP as HTTP" );
        }
    }
    else if( url.psz_protocol && !strcasecmp( url.psz_protocol, "rtsp" ) )
    {
        if( p_sys->rtsp != NULL )
        {
            msg_Err( p_stream, "you can use sdp=rtsp:// only once" );
            goto out;
        }

        /* FIXME test if destination is multicast or no destination at all */
        p_sys->rtsp = RtspSetup( p_stream, &url );
        if( p_sys->rtsp == NULL )
            msg_Err( p_stream, "cannot export SDP as RTSP" );
        else
        if( p_sys->p_mux != NULL )
        {
            sout_stream_id_t *id = p_sys->es[0];
            id->rtsp_id = RtspAddId( p_sys->rtsp, id, 0, GetDWBE( id->ssrc ),
                                     p_sys->psz_destination, p_sys->i_ttl,
                                     id->i_port, id->i_port + 1 );
        }
    }
    else if( ( url.psz_protocol && !strcasecmp( url.psz_protocol, "sap" ) ) ||
             ( url.psz_host && !strcasecmp( url.psz_host, "sap" ) ) )
    {
        p_sys->b_export_sap = true;
        SapSetup( p_stream );
    }
    else if( url.psz_protocol && !strcasecmp( url.psz_protocol, "file" ) )
    {
        if( p_sys->b_export_sdp_file )
        {
            msg_Err( p_stream, "you can use sdp=file:// only once" );
            goto out;
        }
        p_sys->b_export_sdp_file = true;
        psz_url = &psz_url[5];
        if( psz_url[0] == '/' && psz_url[1] == '/' )
            psz_url += 2;
        p_sys->psz_sdp_file = strdup( psz_url );
        decode_URI( p_sys->psz_sdp_file ); /* FIXME? */
        FileSetup( p_stream );
    }
    else
    {
        msg_Warn( p_stream, "unknown protocol for SDP (%s)",
                  url.psz_protocol );
    }

out:
    vlc_UrlClean( &url );
}

/*****************************************************************************
 * SDPGenerate
 *****************************************************************************/
/*static*/
char *SDPGenerate( const sout_stream_t *p_stream, const char *rtsp_url )
{
    const sout_stream_sys_t *p_sys = p_stream->p_sys;
    char *psz_sdp;
    struct sockaddr_storage dst;
    socklen_t dstlen;
    int i;
    /*
     * When we have a fixed destination (typically when we do multicast),
     * we need to put the actual port numbers in the SDP.
     * When there is no fixed destination, we only support RTSP unicast
     * on-demand setup, so we should rather let the clients decide which ports
     * to use.
     * When there is both a fixed destination and RTSP unicast, we need to
     * put port numbers used by the fixed destination, otherwise the SDP would
     * become totally incorrect for multicast use. It should be noted that
     * port numbers from SDP with RTSP are only "recommendation" from the
     * server to the clients (per RFC2326), so only broken clients will fail
     * to handle this properly. There is no solution but to use two differents
     * output chain with two different RTSP URLs if you need to handle this
     * scenario.
     */
    int inclport;

    if( p_sys->psz_destination != NULL )
    {
        inclport = 1;

        /* Oh boy, this is really ugly! (+ race condition on lock_es) */
        dstlen = sizeof( dst );
        if( p_sys->es[0]->listen_fd != NULL )
            getsockname( p_sys->es[0]->listen_fd[0],
                         (struct sockaddr *)&dst, &dstlen );
        else
            getpeername( p_sys->es[0]->sinkv[0].rtp_fd,
                         (struct sockaddr *)&dst, &dstlen );
    }
    else
    {
        inclport = 0;

        /* Dummy destination address for RTSP */
        memset (&dst, 0, sizeof( struct sockaddr_in ) );
        dst.ss_family = AF_INET;
#ifdef HAVE_SA_LEN
        dst.ss_len =
#endif
        dstlen = sizeof( struct sockaddr_in );
    }

    psz_sdp = vlc_sdp_Start( VLC_OBJECT( p_stream ), SOUT_CFG_PREFIX,
                             NULL, 0, (struct sockaddr *)&dst, dstlen );
    if( psz_sdp == NULL )
        return NULL;

    /* TODO: a=source-filter */
    if( p_sys->rtcp_mux )
        sdp_AddAttribute( &psz_sdp, "rtcp-mux", NULL );

    if( rtsp_url != NULL )
        sdp_AddAttribute ( &psz_sdp, "control", "%s", rtsp_url );

    /* FIXME: locking?! */
    for( i = 0; i < p_sys->i_es; i++ )
    {
        sout_stream_id_t *id = p_sys->es[i];
        const char *mime_major; /* major MIME type */
        const char *proto = "RTP/AVP"; /* protocol */

        switch( id->i_cat )
        {
            case VIDEO_ES:
                mime_major = "video";
                break;
            case AUDIO_ES:
                mime_major = "audio";
                break;
            case SPU_ES:
                mime_major = "text";
                break;
            default:
                continue;
        }

        if( rtsp_url == NULL )
        {
            switch( p_sys->proto )
            {
                case IPPROTO_UDP:
                    break;
                case IPPROTO_TCP:
                    proto = "TCP/RTP/AVP";
                    break;
                case IPPROTO_DCCP:
                    proto = "DCCP/RTP/AVP";
                    break;
                case IPPROTO_UDPLITE:
                    continue;
            }
        }

        sdp_AddMedia( &psz_sdp, mime_major, proto, inclport * id->i_port,
                      id->i_payload_type, false, id->i_bitrate,
                      id->psz_enc, id->i_clock_rate, id->i_channels,
                      id->psz_fmtp);

        if( !p_sys->rtcp_mux && (id->i_port & 1) ) /* cf RFC4566 §5.14 */
            sdp_AddAttribute ( &psz_sdp, "rtcp", "%u", id->i_port + 1 );

        if( rtsp_url != NULL )
        {
            assert( strlen( rtsp_url ) > 0 );
            bool addslash = ( rtsp_url[strlen( rtsp_url ) - 1] != '/' );
            sdp_AddAttribute ( &psz_sdp, "control",
                               addslash ? "%s/trackID=%u" : "%strackID=%u",
                               rtsp_url, i );
        }
        else
        {
            if( id->listen_fd != NULL )
                sdp_AddAttribute( &psz_sdp, "setup", "passive" );
            if( p_sys->proto == IPPROTO_DCCP )
                sdp_AddAttribute( &psz_sdp, "dccp-service-code",
                                  "SC:RTP%c", toupper( mime_major[0] ) );
        }
    }

    return psz_sdp;
}

/*****************************************************************************
 * RTP mux
 *****************************************************************************/

static void sprintf_hexa( char *s, uint8_t *p_data, int i_data )
{
    static const char hex[16] = "0123456789abcdef";
    int i;

    for( i = 0; i < i_data; i++ )
    {
        s[2*i+0] = hex[(p_data[i]>>4)&0xf];
        s[2*i+1] = hex[(p_data[i]   )&0xf];
    }
    s[2*i_data] = '\0';
}

/**
 * Shrink the MTU down to a fixed packetization time (for audio).
 */
static void
rtp_set_ptime (sout_stream_id_t *id, unsigned ptime_ms, size_t bytes)
{
    /* Samples per second */
    size_t spl = (id->i_clock_rate - 1) * ptime_ms / 1000 + 1;
    bytes *= id->i_channels;
    spl *= bytes;

    if (spl < rtp_mtu (id)) /* MTU is big enough for ptime */
        id->i_mtu = 12 + spl;
    else /* MTU is too small for ptime, align to a sample boundary */
        id->i_mtu = 12 + (((id->i_mtu - 12) / bytes) * bytes);
}

/** Add an ES as a new RTP stream */
static sout_stream_id_t *Add( sout_stream_t *p_stream, es_format_t *p_fmt )
{
    /* NOTE: As a special case, if we use a non-RTP
     * mux (TS/PS), then p_fmt is NULL. */
    sout_stream_sys_t *p_sys = p_stream->p_sys;
    sout_stream_id_t  *id;
    int               cscov = -1;
    char              *psz_sdp;

    if (0xffffffff == p_sys->payload_bitmap)
    {
        msg_Err (p_stream, "too many RTP elementary streams");
        return NULL;
    }

    /* Choose the port */
    uint16_t i_port = 0;
    if( p_fmt == NULL )
        ;
    else
    if( p_fmt->i_cat == AUDIO_ES && p_sys->i_port_audio > 0 )
        i_port = p_sys->i_port_audio;
    else
    if( p_fmt->i_cat == VIDEO_ES && p_sys->i_port_video > 0 )
        i_port = p_sys->i_port_video;

    /* We do not need the ES lock (p_sys->lock_es) here, because this is the
     * only one thread that can *modify* the ES table. The ES lock protects
     * the other threads from our modifications (TAB_APPEND, TAB_REMOVE). */
    for (int i = 0; i_port && (i < p_sys->i_es); i++)
         if (i_port == p_sys->es[i]->i_port)
             i_port = 0; /* Port already in use! */
    for (uint16_t p = p_sys->i_port; i_port == 0; p += 2)
    {
        if (p == 0)
        {
            msg_Err (p_stream, "too many RTP elementary streams");
            return NULL;
        }
        i_port = p;
        for (int i = 0; i_port && (i < p_sys->i_es); i++)
             if (p == p_sys->es[i]->i_port)
                 i_port = 0;
    }

    id = vlc_object_create( p_stream, sizeof( sout_stream_id_t ) );
    if( id == NULL )
        return NULL;
    vlc_object_attach( id, p_stream );

    id->p_stream   = p_stream;

    /* Look for free dymanic payload type */
    id->i_payload_type = 96;
    while (p_sys->payload_bitmap & (1 << (id->i_payload_type - 96)))
        id->i_payload_type++;
    assert (id->i_payload_type < 128);

    vlc_rand_bytes (&id->i_sequence, sizeof (id->i_sequence));
    vlc_rand_bytes (id->ssrc, sizeof (id->ssrc));

    id->psz_enc    = NULL;
    id->psz_fmtp   = NULL;
    id->i_clock_rate = 90000; /* most common case for video */
    id->i_channels = 0;
    id->i_port     = i_port;
    if( p_fmt != NULL )
    {
        id->i_cat  = p_fmt->i_cat;
        if( p_fmt->i_cat == AUDIO_ES )
        {
            id->i_clock_rate = p_fmt->audio.i_rate;
            id->i_channels = p_fmt->audio.i_channels;
        }
        id->i_bitrate = p_fmt->i_bitrate/1000; /* Stream bitrate in kbps */
    }
    else
    {
        id->i_cat  = VIDEO_ES;
        id->i_bitrate = 0;
    }

    id->i_mtu = config_GetInt( p_stream, "mtu" );
    if( id->i_mtu <= 12 + 16 )
        id->i_mtu = 576 - 20 - 8; /* pessimistic */
    msg_Dbg( p_stream, "maximum RTP packet size: %d bytes", id->i_mtu );

    id->srtp = NULL;
    id->pf_packetize = NULL;

    char *key = var_CreateGetNonEmptyString (p_stream, SOUT_CFG_PREFIX"key");
    if (key)
    {
        id->srtp = srtp_create (SRTP_ENCR_AES_CM, SRTP_AUTH_HMAC_SHA1, 10,
                                   SRTP_PRF_AES_CM, SRTP_RCC_MODE1);
        if (id->srtp == NULL)
        {
            free (key);
            goto error;
        }

        char *salt = var_CreateGetNonEmptyString (p_stream, SOUT_CFG_PREFIX"salt");
        errno = srtp_setkeystring (id->srtp, key, salt ? salt : "");
        free (salt);
        free (key);
        if (errno)
        {
            msg_Err (p_stream, "bad SRTP key/salt combination (%m)");
            goto error;
        }
        id->i_sequence = 0; /* FIXME: awful hack for libvlc_srtp */
    }

    vlc_mutex_init( &id->lock_sink );
    id->sinkc = 0;
    id->sinkv = NULL;
    id->rtsp_id = NULL;
    id->p_fifo = NULL;
    id->listen_fd = NULL;

    id->i_caching =
        (int64_t)1000 * var_GetInteger( p_stream, SOUT_CFG_PREFIX "caching");

    if( p_sys->psz_destination != NULL )
        switch( p_sys->proto )
        {
            case IPPROTO_DCCP:
            {
                const char *code;
                switch (id->i_cat)
                {
                    case VIDEO_ES: code = "RTPV";     break;
                    case AUDIO_ES: code = "RTPARTPV"; break;
                    case SPU_ES:   code = "RTPTRTPV"; break;
                    default:       code = "RTPORTPV"; break;
                }
                var_SetString (p_stream, "dccp-service", code);
            }   /* fall through */
            case IPPROTO_TCP:
                id->listen_fd = net_Listen( VLC_OBJECT(p_stream),
                                            p_sys->psz_destination, i_port,
                                            p_sys->proto );
                if( id->listen_fd == NULL )
                {
                    msg_Err( p_stream, "passive COMEDIA RTP socket failed" );
                    goto error;
                }
                break;

            default:
            {
                int ttl = (p_sys->i_ttl >= 0) ? p_sys->i_ttl : -1;
                int fd = net_ConnectDgram( p_stream, p_sys->psz_destination,
                                           i_port, ttl, p_sys->proto );
                if( fd == -1 )
                {
                    msg_Err( p_stream, "cannot create RTP socket" );
                    goto error;
                }
                rtp_add_sink( id, fd, p_sys->rtcp_mux );
            }
        }

    if( p_fmt == NULL )
    {
        char *psz = var_GetNonEmptyString( p_stream, SOUT_CFG_PREFIX "mux" );

        if( psz == NULL ) /* Uho! */
            ;
        else
        if( strncmp( psz, "ts", 2 ) == 0 )
        {
            id->i_payload_type = 33;
            id->psz_enc = "MP2T";
        }
        else
        {
            id->psz_enc = "MP2P";
        }
        free( psz );
    }
    else
    switch( p_fmt->i_codec )
    {
        case VLC_FOURCC( 'u', 'l', 'a', 'w' ):
            if( p_fmt->audio.i_channels == 1 && p_fmt->audio.i_rate == 8000 )
                id->i_payload_type = 0;
            id->psz_enc = "PCMU";
            id->pf_packetize = rtp_packetize_split;
            rtp_set_ptime (id, 20, 1);
            break;
        case VLC_FOURCC( 'a', 'l', 'a', 'w' ):
            if( p_fmt->audio.i_channels == 1 && p_fmt->audio.i_rate == 8000 )
                id->i_payload_type = 8;
            id->psz_enc = "PCMA";
            id->pf_packetize = rtp_packetize_split;
            rtp_set_ptime (id, 20, 1);
            break;
        case VLC_FOURCC( 's', '1', '6', 'b' ):
            if( p_fmt->audio.i_channels == 1 && p_fmt->audio.i_rate == 44100 )
            {
                id->i_payload_type = 11;
            }
            else if( p_fmt->audio.i_channels == 2 &&
                     p_fmt->audio.i_rate == 44100 )
            {
                id->i_payload_type = 10;
            }
            id->psz_enc = "L16";
            id->pf_packetize = rtp_packetize_split;
            rtp_set_ptime (id, 20, 2);
            break;
        case VLC_FOURCC( 'u', '8', ' ', ' ' ):
            id->psz_enc = "L8";
            id->pf_packetize = rtp_packetize_split;
            rtp_set_ptime (id, 20, 1);
            break;
        case VLC_FOURCC( 'm', 'p', 'g', 'a' ):
        case VLC_FOURCC( 'm', 'p', '3', ' ' ):
            id->i_payload_type = 14;
            id->psz_enc = "MPA";
            id->i_clock_rate = 90000; /* not 44100 */
            id->pf_packetize = rtp_packetize_mpa;
            break;
        case VLC_FOURCC( 'm', 'p', 'g', 'v' ):
            id->i_payload_type = 32;
            id->psz_enc = "MPV";
            id->pf_packetize = rtp_packetize_mpv;
            break;
        case VLC_FOURCC( 'G', '7', '2', '6' ):
        case VLC_FOURCC( 'g', '7', '2', '6' ):
            switch( p_fmt->i_bitrate / 1000 )
            {
            case 16:
                id->psz_enc = "G726-16";
                id->pf_packetize = rtp_packetize_g726_16;
                break;
            case 24:
                id->psz_enc = "G726-24";
                id->pf_packetize = rtp_packetize_g726_24;
                break;
            case 32:
                id->psz_enc = "G726-32";
                id->pf_packetize = rtp_packetize_g726_32;
                break;
            case 40:
                id->psz_enc = "G726-40";
                id->pf_packetize = rtp_packetize_g726_40;
                break;
            default:
                msg_Err( p_stream, "cannot add this stream (unsupported "
                         "G.726 bit rate: %u)", p_fmt->i_bitrate );
                goto error;
            }
            break;
        case VLC_FOURCC( 'a', '5', '2', ' ' ):
            id->psz_enc = "ac3";
            id->pf_packetize = rtp_packetize_ac3;
            break;
        case VLC_FOURCC( 'H', '2', '6', '3' ):
            id->psz_enc = "H263-1998";
            id->pf_packetize = rtp_packetize_h263;
            break;
        case VLC_FOURCC( 'h', '2', '6', '4' ):
            id->psz_enc = "H264";
            id->pf_packetize = rtp_packetize_h264;
            id->psz_fmtp = NULL;

            if( p_fmt->i_extra > 0 )
            {
                uint8_t *p_buffer = p_fmt->p_extra;
                int     i_buffer = p_fmt->i_extra;
                char    *p_64_sps = NULL;
                char    *p_64_pps = NULL;
                char    hexa[6+1];

                while( i_buffer > 4 &&
                       p_buffer[0] == 0 && p_buffer[1] == 0 &&
                       p_buffer[2] == 0 && p_buffer[3] == 1 )
                {
                    const int i_nal_type = p_buffer[4]&0x1f;
                    int i_offset;
                    int i_size      = 0;

                    msg_Dbg( p_stream, "we found a startcode for NAL with TYPE:%d", i_nal_type );

                    i_size = i_buffer;
                    for( i_offset = 4; i_offset+3 < i_buffer ; i_offset++)
                    {
                        if( !memcmp (p_buffer + i_offset, "\x00\x00\x00\x01", 4 ) )
                        {
                            /* we found another startcode */
                            i_size = i_offset;
                            break;
                        }
                    }
                    if( i_nal_type == 7 )
                    {
                        p_64_sps = vlc_b64_encode_binary( &p_buffer[4], i_size - 4 );
                        sprintf_hexa( hexa, &p_buffer[5], 3 );
                    }
                    else if( i_nal_type == 8 )
                    {
                        p_64_pps = vlc_b64_encode_binary( &p_buffer[4], i_size - 4 );
                    }
                    i_buffer -= i_size;
                    p_buffer += i_size;
                }
                /* */
                if( p_64_sps && p_64_pps &&
                    ( asprintf( &id->psz_fmtp,
                                "packetization-mode=1;profile-level-id=%s;"
                                "sprop-parameter-sets=%s,%s;", hexa, p_64_sps,
                                p_64_pps ) == -1 ) )
                    id->psz_fmtp = NULL;
                free( p_64_sps );
                free( p_64_pps );
            }
            if( !id->psz_fmtp )
                id->psz_fmtp = strdup( "packetization-mode=1" );
            break;

        case VLC_FOURCC( 'm', 'p', '4', 'v' ):
        {
            char hexa[2*p_fmt->i_extra +1];

            id->psz_enc = "MP4V-ES";
            id->pf_packetize = rtp_packetize_split;
            if( p_fmt->i_extra > 0 )
            {
                sprintf_hexa( hexa, p_fmt->p_extra, p_fmt->i_extra );
                if( asprintf( &id->psz_fmtp,
                              "profile-level-id=3; config=%s;", hexa ) == -1 )
                    id->psz_fmtp = NULL;
            }
            break;
        }
        case VLC_FOURCC( 'm', 'p', '4', 'a' ):
        {
            if(!p_sys->b_latm)
            {
                char hexa[2*p_fmt->i_extra +1];

                id->psz_enc = "mpeg4-generic";
                id->pf_packetize = rtp_packetize_mp4a;
                sprintf_hexa( hexa, p_fmt->p_extra, p_fmt->i_extra );
                if( asprintf( &id->psz_fmtp,
                              "streamtype=5; profile-level-id=15; "
                              "mode=AAC-hbr; config=%s; SizeLength=13; "
                              "IndexLength=3; IndexDeltaLength=3; Profile=1;",
                              hexa ) == -1 )
                    id->psz_fmtp = NULL;
            }
            else
            {
                char hexa[13];
                int i;
                unsigned char config[6];
                unsigned int aacsrates[15] = {
                    96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050,
                    16000, 12000, 11025, 8000, 7350, 0, 0 };

                for( i = 0; i < 15; i++ )
                    if( p_fmt->audio.i_rate == aacsrates[i] )
                        break;

                config[0]=0x40;
                config[1]=0;
                config[2]=0x20|i;
                config[3]=p_fmt->audio.i_channels<<4;
                config[4]=0x3f;
                config[5]=0xc0;

                id->psz_enc = "MP4A-LATM";
                id->pf_packetize = rtp_packetize_mp4a_latm;
                sprintf_hexa( hexa, config, 6 );
                if( asprintf( &id->psz_fmtp, "profile-level-id=15; "
                              "object=2; cpresent=0; config=%s", hexa ) == -1 )
                    id->psz_fmtp = NULL;
            }
            break;
        }
        case VLC_FOURCC( 's', 'a', 'm', 'r' ):
            id->psz_enc = "AMR";
            id->psz_fmtp = strdup( "octet-align=1" );
            id->pf_packetize = rtp_packetize_amr;
            break;
        case VLC_FOURCC( 's', 'a', 'w', 'b' ):
            id->psz_enc = "AMR-WB";
            id->psz_fmtp = strdup( "octet-align=1" );
            id->pf_packetize = rtp_packetize_amr;
            break;
        case VLC_FOURCC( 's', 'p', 'x', ' ' ):
            id->psz_enc = "SPEEX";
            id->pf_packetize = rtp_packetize_spx;
            break;
        case VLC_FOURCC( 't', '1', '4', '0' ):
            id->psz_enc = "t140" ;
            id->i_clock_rate = 1000;
            id->pf_packetize = rtp_packetize_t140;
            break;

        default:
            msg_Err( p_stream, "cannot add this stream (unsupported "
                     "codec: %4.4s)", (char*)&p_fmt->i_codec );
            goto error;
    }
    if (id->i_payload_type >= 96)
        /* Mark dynamic payload type in use */
        p_sys->payload_bitmap |= 1 << (id->i_payload_type - 96);

#if 0 /* No payload formats sets this at the moment */
    if( cscov != -1 )
        cscov += 8 /* UDP */ + 12 /* RTP */;
    if( id->sinkc > 0 )
        net_SetCSCov( id->sinkv[0].rtp_fd, cscov, -1 );
#endif

    if( p_sys->rtsp != NULL )
        id->rtsp_id = RtspAddId( p_sys->rtsp, id, p_sys->i_es,
                                 GetDWBE( id->ssrc ),
                                 p_sys->psz_destination,
                                 p_sys->i_ttl, id->i_port, id->i_port + 1 );

    id->p_fifo = block_FifoNew();
    if( vlc_thread_create( id, "RTP send thread", ThreadSend,
                           VLC_THREAD_PRIORITY_HIGHEST ) )
        goto error;

    /* Update p_sys context */
    vlc_mutex_lock( &p_sys->lock_es );
    TAB_APPEND( p_sys->i_es, p_sys->es, id );
    vlc_mutex_unlock( &p_sys->lock_es );

    psz_sdp = SDPGenerate( p_stream, NULL );

    vlc_mutex_lock( &p_sys->lock_sdp );
    free( p_sys->psz_sdp );
    p_sys->psz_sdp = psz_sdp;
    vlc_mutex_unlock( &p_sys->lock_sdp );

    msg_Dbg( p_stream, "sdp=\n%s", p_sys->psz_sdp );

    /* Update SDP (sap/file) */
    if( p_sys->b_export_sap ) SapSetup( p_stream );
    if( p_sys->b_export_sdp_file ) FileSetup( p_stream );

    return id;

error:
    Del( p_stream, id );
    return NULL;
}

static int Del( sout_stream_t *p_stream, sout_stream_id_t *id )
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;

    if( id->p_fifo != NULL )
    {
        vlc_object_kill( id );
        vlc_thread_join( id );
        block_FifoRelease( id->p_fifo );
    }

    vlc_mutex_lock( &p_sys->lock_es );
    TAB_REMOVE( p_sys->i_es, p_sys->es, id );
    vlc_mutex_unlock( &p_sys->lock_es );

    /* Release dynamic payload type */
    if (id->i_payload_type >= 96)
        p_sys->payload_bitmap &= ~(1 << (id->i_payload_type - 96));

    free( id->psz_fmtp );

    if( id->rtsp_id )
        RtspDelId( p_sys->rtsp, id->rtsp_id );
    if( id->sinkc > 0 )
        rtp_del_sink( id, id->sinkv[0].rtp_fd ); /* sink for explicit dst= */
    if( id->listen_fd != NULL )
        net_ListenClose( id->listen_fd );
    if( id->srtp != NULL )
        srtp_destroy( id->srtp );

    vlc_mutex_destroy( &id->lock_sink );

    /* Update SDP (sap/file) */
    if( p_sys->b_export_sap && !p_sys->p_mux ) SapSetup( p_stream );
    if( p_sys->b_export_sdp_file ) FileSetup( p_stream );

    vlc_object_detach( id );
    vlc_object_release( id );
    return VLC_SUCCESS;
}

static int Send( sout_stream_t *p_stream, sout_stream_id_t *id,
                 block_t *p_buffer )
{
    block_t *p_next;

    assert( p_stream->p_sys->p_mux == NULL );
    (void)p_stream;

    while( p_buffer != NULL )
    {
        p_next = p_buffer->p_next;
        if( id->pf_packetize( id, p_buffer ) )
            break;

        block_Release( p_buffer );
        p_buffer = p_next;
    }
    return VLC_SUCCESS;
}

/****************************************************************************
 * SAP:
 ****************************************************************************/
static int SapSetup( sout_stream_t *p_stream )
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;
    sout_instance_t   *p_sout = p_stream->p_sout;

    /* Remove the previous session */
    if( p_sys->p_session != NULL)
    {
        sout_AnnounceUnRegister( p_sout, p_sys->p_session);
        p_sys->p_session = NULL;
    }

    if( ( p_sys->i_es > 0 || p_sys->p_mux ) && p_sys->psz_sdp && *p_sys->psz_sdp )
    {
        announce_method_t *p_method = sout_SAPMethod();
        p_sys->p_session = sout_AnnounceRegisterSDP( p_sout,
                                                     p_sys->psz_sdp,
                                                     p_sys->psz_destination,
                                                     p_method );
        sout_MethodRelease( p_method );
    }

    return VLC_SUCCESS;
}

/****************************************************************************
* File:
****************************************************************************/
static int FileSetup( sout_stream_t *p_stream )
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;
    FILE            *f;

    if( p_sys->psz_sdp == NULL )
        return VLC_EGENERIC; /* too early */

    if( ( f = utf8_fopen( p_sys->psz_sdp_file, "wt" ) ) == NULL )
    {
        msg_Err( p_stream, "cannot open file '%s' (%m)",
                 p_sys->psz_sdp_file );
        return VLC_EGENERIC;
    }

    fputs( p_sys->psz_sdp, f );
    fclose( f );

    return VLC_SUCCESS;
}

/****************************************************************************
 * HTTP:
 ****************************************************************************/
static int  HttpCallback( httpd_file_sys_t *p_args,
                          httpd_file_t *, uint8_t *p_request,
                          uint8_t **pp_data, int *pi_data );

static int HttpSetup( sout_stream_t *p_stream, const vlc_url_t *url)
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;

    p_sys->p_httpd_host = httpd_HostNew( VLC_OBJECT(p_stream), url->psz_host,
                                         url->i_port > 0 ? url->i_port : 80 );
    if( p_sys->p_httpd_host )
    {
        p_sys->p_httpd_file = httpd_FileNew( p_sys->p_httpd_host,
                                             url->psz_path ? url->psz_path : "/",
                                             "application/sdp",
                                             NULL, NULL, NULL,
                                             HttpCallback, (void*)p_sys );
    }
    if( p_sys->p_httpd_file == NULL )
    {
        return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}

static int  HttpCallback( httpd_file_sys_t *p_args,
                          httpd_file_t *f, uint8_t *p_request,
                          uint8_t **pp_data, int *pi_data )
{
    VLC_UNUSED(f); VLC_UNUSED(p_request);
    sout_stream_sys_t *p_sys = (sout_stream_sys_t*)p_args;

    vlc_mutex_lock( &p_sys->lock_sdp );
    if( p_sys->psz_sdp && *p_sys->psz_sdp )
    {
        *pi_data = strlen( p_sys->psz_sdp );
        *pp_data = malloc( *pi_data );
        memcpy( *pp_data, p_sys->psz_sdp, *pi_data );
    }
    else
    {
        *pp_data = NULL;
        *pi_data = 0;
    }
    vlc_mutex_unlock( &p_sys->lock_sdp );

    return VLC_SUCCESS;
}

/****************************************************************************
 * RTP send
 ****************************************************************************/
static void* ThreadSend( vlc_object_t *p_this )
{
#ifdef WIN32
# define ECONNREFUSED WSAECONNREFUSED
# define ENOPROTOOPT  WSAENOPROTOOPT
# define EHOSTUNREACH WSAEHOSTUNREACH
# define ENETUNREACH  WSAENETUNREACH
# define ENETDOWN     WSAENETDOWN
# define ENOBUFS      WSAENOBUFS
# define EAGAIN       WSAEWOULDBLOCK
# define EWOULDBLOCK  WSAEWOULDBLOCK
#endif
    sout_stream_id_t *id = (sout_stream_id_t *)p_this;
    unsigned i_caching = id->i_caching;

    for (;;)
    {
        block_t *out = block_FifoGet( id->p_fifo );
        block_cleanup_push (out);

        if( id->srtp )
        {   /* FIXME: this is awfully inefficient */
            size_t len = out->i_buffer;
            out = block_Realloc( out, 0, len + 10 );
            out->i_buffer = len;

            int canc = vlc_savecancel ();
            int val = srtp_send( id->srtp, out->p_buffer, &len, len + 10 );
            vlc_restorecancel (canc);
            if( val )
            {
                errno = val;
                msg_Dbg( id, "SRTP sending error: %m" );
                block_Release( out );
                out = NULL;
            }
            else
                out->i_buffer = len;
        }

        if (out)
            mwait (out->i_dts + i_caching);
        vlc_cleanup_pop ();
        if (out == NULL)
            continue;

        ssize_t len = out->i_buffer;
        int canc = vlc_savecancel ();

        vlc_mutex_lock( &id->lock_sink );
        unsigned deadc = 0; /* How many dead sockets? */
        int deadv[id->sinkc]; /* Dead sockets list */

        for( int i = 0; i < id->sinkc; i++ )
        {
            if( !id->srtp ) /* FIXME: SRTCP support */
                SendRTCP( id->sinkv[i].rtcp, out );

            if( send( id->sinkv[i].rtp_fd, out->p_buffer, len, 0 ) >= 0 )
                continue;
            switch( net_errno )
            {
                /* Soft errors (e.g. ICMP): */
                case ECONNREFUSED: /* Port unreachable */
                case ENOPROTOOPT:
#ifdef EPROTO
                case EPROTO:       /* Protocol unreachable */
#endif
                case EHOSTUNREACH: /* Host unreachable */
                case ENETUNREACH:  /* Network unreachable */
                case ENETDOWN:     /* Entire network down */
                    send( id->sinkv[i].rtp_fd, out->p_buffer, len, 0 );
                /* Transient congestion: */
                case ENOMEM: /* out of socket buffers */
                case ENOBUFS:
                case EAGAIN:
#if (EAGAIN != EWOULDBLOCK)
                case EWOULDBLOCK:
#endif
                    continue;
            }

            deadv[deadc++] = id->sinkv[i].rtp_fd;
        }
        vlc_mutex_unlock( &id->lock_sink );
        block_Release( out );

        for( unsigned i = 0; i < deadc; i++ )
        {
            msg_Dbg( id, "removing socket %d", deadv[i] );
            rtp_del_sink( id, deadv[i] );
        }

        /* Hopefully we won't overflow the SO_MAXCONN accept queue */
        while( id->listen_fd != NULL )
        {
            int fd = net_Accept( id, id->listen_fd, 0 );
            if( fd == -1 )
                break;
            msg_Dbg( id, "adding socket %d", fd );
            rtp_add_sink( id, fd, true );
        }
        vlc_restorecancel (canc);
    }
    return NULL;
}

int rtp_add_sink( sout_stream_id_t *id, int fd, bool rtcp_mux )
{
    rtp_sink_t sink = { fd, NULL };
    sink.rtcp = OpenRTCP( VLC_OBJECT( id->p_stream ), fd, IPPROTO_UDP,
                          rtcp_mux );
    if( sink.rtcp == NULL )
        msg_Err( id, "RTCP failed!" );

    vlc_mutex_lock( &id->lock_sink );
    INSERT_ELEM( id->sinkv, id->sinkc, id->sinkc, sink );
    vlc_mutex_unlock( &id->lock_sink );
    return VLC_SUCCESS;
}

void rtp_del_sink( sout_stream_id_t *id, int fd )
{
    rtp_sink_t sink = { fd, NULL };

    /* NOTE: must be safe to use if fd is not included */
    vlc_mutex_lock( &id->lock_sink );
    for( int i = 0; i < id->sinkc; i++ )
    {
        if (id->sinkv[i].rtp_fd == fd)
        {
            sink = id->sinkv[i];
            REMOVE_ELEM( id->sinkv, id->sinkc, i );
            break;
        }
    }
    vlc_mutex_unlock( &id->lock_sink );

    CloseRTCP( sink.rtcp );
    net_Close( sink.rtp_fd );
}

uint16_t rtp_get_seq( const sout_stream_id_t *id )
{
    /* This will return values for the next packet.
     * Accounting for caching would not be totally trivial. */
    return id->i_sequence;
}

/* FIXME: this is pretty bad - if we remove and then insert an ES
 * the number will get unsynched from inside RTSP */
unsigned rtp_get_num( const sout_stream_id_t *id )
{
    sout_stream_sys_t *p_sys = id->p_stream->p_sys;
    int i;

    vlc_mutex_lock( &p_sys->lock_es );
    for( i = 0; i < p_sys->i_es; i++ )
    {
        if( id == p_sys->es[i] )
            break;
    }
    vlc_mutex_unlock( &p_sys->lock_es );

    return i;
}


void rtp_packetize_common( sout_stream_id_t *id, block_t *out,
                           int b_marker, int64_t i_pts )
{
    uint32_t i_timestamp = i_pts * (int64_t)id->i_clock_rate / INT64_C(1000000);

    out->p_buffer[0] = 0x80;
    out->p_buffer[1] = (b_marker?0x80:0x00)|id->i_payload_type;
    out->p_buffer[2] = ( id->i_sequence >> 8)&0xff;
    out->p_buffer[3] = ( id->i_sequence     )&0xff;
    out->p_buffer[4] = ( i_timestamp >> 24 )&0xff;
    out->p_buffer[5] = ( i_timestamp >> 16 )&0xff;
    out->p_buffer[6] = ( i_timestamp >>  8 )&0xff;
    out->p_buffer[7] = ( i_timestamp       )&0xff;

    memcpy( out->p_buffer + 8, id->ssrc, 4 );

    out->i_buffer = 12;
    id->i_sequence++;
}

void rtp_packetize_send( sout_stream_id_t *id, block_t *out )
{
    block_FifoPut( id->p_fifo, out );
}

/**
 * @return configured max RTP payload size (including payload type-specific
 * headers, excluding RTP and transport headers)
 */
size_t rtp_mtu (const sout_stream_id_t *id)
{
    return id->i_mtu - 12;
}

/*****************************************************************************
 * Non-RTP mux
 *****************************************************************************/

/** Add an ES to a non-RTP muxed stream */
static sout_stream_id_t *MuxAdd( sout_stream_t *p_stream, es_format_t *p_fmt )
{
    sout_input_t      *p_input;
    sout_mux_t *p_mux = p_stream->p_sys->p_mux;
    assert( p_mux != NULL );

    p_input = sout_MuxAddStream( p_mux, p_fmt );
    if( p_input == NULL )
    {
        msg_Err( p_stream, "cannot add this stream to the muxer" );
        return NULL;
    }

    return (sout_stream_id_t *)p_input;
}


static int MuxSend( sout_stream_t *p_stream, sout_stream_id_t *id,
                    block_t *p_buffer )
{
    sout_mux_t *p_mux = p_stream->p_sys->p_mux;
    assert( p_mux != NULL );

    sout_MuxSendBuffer( p_mux, (sout_input_t *)id, p_buffer );
    return VLC_SUCCESS;
}


/** Remove an ES from a non-RTP muxed stream */
static int MuxDel( sout_stream_t *p_stream, sout_stream_id_t *id )
{
    sout_mux_t *p_mux = p_stream->p_sys->p_mux;
    assert( p_mux != NULL );

    sout_MuxDeleteStream( p_mux, (sout_input_t *)id );
    return VLC_SUCCESS;
}


static ssize_t AccessOutGrabberWriteBuffer( sout_stream_t *p_stream,
                                            const block_t *p_buffer )
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;
    sout_stream_id_t *id = p_sys->es[0];

    int64_t  i_dts = p_buffer->i_dts;

    uint8_t         *p_data = p_buffer->p_buffer;
    size_t          i_data  = p_buffer->i_buffer;
    size_t          i_max   = id->i_mtu - 12;

    size_t i_packet = ( p_buffer->i_buffer + i_max - 1 ) / i_max;

    while( i_data > 0 )
    {
        size_t i_size;

        /* output complete packet */
        if( p_sys->packet &&
            p_sys->packet->i_buffer + i_data > i_max )
        {
            rtp_packetize_send( id, p_sys->packet );
            p_sys->packet = NULL;
        }

        if( p_sys->packet == NULL )
        {
            /* allocate a new packet */
            p_sys->packet = block_New( p_stream, id->i_mtu );
            rtp_packetize_common( id, p_sys->packet, 1, i_dts );
            p_sys->packet->i_dts = i_dts;
            p_sys->packet->i_length = p_buffer->i_length / i_packet;
            i_dts += p_sys->packet->i_length;
        }

        i_size = __MIN( i_data,
                        (unsigned)(id->i_mtu - p_sys->packet->i_buffer) );

        memcpy( &p_sys->packet->p_buffer[p_sys->packet->i_buffer],
                p_data, i_size );

        p_sys->packet->i_buffer += i_size;
        p_data += i_size;
        i_data -= i_size;
    }

    return VLC_SUCCESS;
}


static ssize_t AccessOutGrabberWrite( sout_access_out_t *p_access,
                                      block_t *p_buffer )
{
    sout_stream_t *p_stream = (sout_stream_t*)p_access->p_sys;

    while( p_buffer )
    {
        block_t *p_next;

        AccessOutGrabberWriteBuffer( p_stream, p_buffer );

        p_next = p_buffer->p_next;
        block_Release( p_buffer );
        p_buffer = p_next;
    }

    return VLC_SUCCESS;
}


static sout_access_out_t *GrabberCreate( sout_stream_t *p_stream )
{
    sout_access_out_t *p_grab;

    p_grab = vlc_object_create( p_stream->p_sout, sizeof( *p_grab ) );
    if( p_grab == NULL )
        return NULL;

    p_grab->p_module    = NULL;
    p_grab->psz_access  = strdup( "grab" );
    p_grab->p_cfg       = NULL;
    p_grab->psz_path    = strdup( "" );
    p_grab->p_sys       = (sout_access_out_sys_t *)p_stream;
    p_grab->pf_seek     = NULL;
    p_grab->pf_write    = AccessOutGrabberWrite;
    vlc_object_attach( p_grab, p_stream );
    return p_grab;
}
