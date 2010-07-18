/*****************************************************************************
 * live555.cpp : LIVE555 Streaming Media support.
 *****************************************************************************
 * Copyright (C) 2003-2006 the VideoLAN team
 * $Id: 3c75243cc80454fce534bfe74fecb6daf49d31fa $
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
#include <stdlib.h>                                      /* malloc(), free() */

#include <vlc/vlc.h>
#include <vlc/input.h>
#include <vlc_interaction.h>
#include <vlc_url.h>

#include "network.h"

#include <iostream>
#include <limits.h>

#if defined( WIN32 )
#   include <winsock2.h>
#endif

#include "BasicUsageEnvironment.hh"
#include "GroupsockHelper.hh"
#include "liveMedia.hh"

extern "C" {
#include "../access/mms/asf.h"  /* Who said ugly ? */
}

#if (LIVEMEDIA_LIBRARY_VERSION_INT < 1089936000)
#define RECLAIM_ENV(env) delete (env)
#else
#define RECLAIM_ENV(env) (env)->reclaim()
#endif

using namespace std;

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

#define CACHING_TEXT N_("Caching value (ms)")
#define CACHING_LONGTEXT N_( \
    "Allows you to modify the default caching value for RTSP streams. This " \
    "value should be set in millisecond units." )

#define KASENNA_TEXT N_( "Kasenna RTSP dialect")
#define KASENNA_LONGTEXT N_( "Kasenna servers use an old and unstandard " \
    "dialect of RTSP. When you set this parameter, VLC will try this dialect "\
    "for communication. In this mode you cannot connect to normal RTSP servers." )

#define USER_TEXT N_("RTSP user name")
#define USER_LONGTEXT N_("Allows you to modify the user name that will " \
    "be used for authenticating the connection.")
#define PASS_TEXT N_("RTSP password")
#define PASS_LONGTEXT N_("Allows you to modify the password that will be " \
    "used for the connection.")

vlc_module_begin();
    set_description( _("RTP/RTSP/SDP demuxer (using Live555)" ) );
    set_capability( "demux2", 50 );
    set_shortname( "RTP/RTSP");
    set_callbacks( Open, Close );
    add_shortcut( "live" );
    add_shortcut( "livedotcom" );
    set_category( CAT_INPUT );
    set_subcategory( SUBCAT_INPUT_DEMUX );

    add_submodule();
        set_description( _("RTSP/RTP access and demux") );
        add_shortcut( "rtsp" );
        add_shortcut( "sdp" );
        set_capability( "access_demux", 0 );
        set_callbacks( Open, Close );
        add_bool( "rtsp-tcp", 0, NULL,
                  N_("Use RTP over RTSP (TCP)"),
                  N_("Use RTP over RTSP (TCP)"), VLC_TRUE );
        add_integer( "rtp-client-port", -1, NULL,
                  N_("Client port"),
                  N_("Port to use for the RTP source of the session"), VLC_TRUE );
#if LIVEMEDIA_LIBRARY_VERSION_INT > 1130457500
        add_bool( "rtsp-http", 0, NULL,
                  N_("Tunnel RTSP and RTP over HTTP"),
                  N_("Tunnel RTSP and RTP over HTTP"), VLC_TRUE );
        add_integer( "rtsp-http-port", 80, NULL,
                  N_("HTTP tunnel port"),
                  N_("Port to use for tunneling the RTSP/RTP over HTTP."),
                  VLC_TRUE );
#endif
        add_integer("rtsp-caching", 4 * DEFAULT_PTS_DELAY / 1000, NULL,
                    CACHING_TEXT, CACHING_LONGTEXT, VLC_TRUE );
        add_bool(   "rtsp-kasenna", VLC_FALSE, NULL, KASENNA_TEXT,
                    KASENNA_LONGTEXT, VLC_TRUE );
        add_string( "rtsp-user", NULL, NULL, USER_TEXT,
                    USER_LONGTEXT, VLC_TRUE );
        add_string( "rtsp-pwd", NULL, NULL, PASS_TEXT,
                    PASS_LONGTEXT, VLC_TRUE );
vlc_module_end();


/*****************************************************************************
 * Local prototypes
 *****************************************************************************/

typedef struct
{
    demux_t     *p_demux;

    vlc_bool_t   b_quicktime;
    vlc_bool_t   b_muxed;
    vlc_bool_t   b_asf;

    es_format_t  fmt;
    es_out_id_t  *p_es;

    stream_t     *p_out_muxed;    /* for muxed stream */

    RTPSource    *rtpSource;
    FramedSource *readSource;
    vlc_bool_t   b_rtcp_sync;

    uint8_t      *p_buffer;
    unsigned int  i_buffer;

    char         waiting;

    int64_t      i_pts;

} live_track_t;

struct timeout_thread_t
{
    VLC_COMMON_MEMBERS

    int64_t      i_remain;
    vlc_bool_t   b_handle_keep_alive;
    demux_sys_t  *p_sys;
};

struct demux_sys_t
{
    char            *p_sdp;    /* XXX mallocated */
    char            *psz_path; /* URL-encoded path */
    vlc_url_t       url;

    MediaSession     *ms;
    TaskScheduler    *scheduler;
    UsageEnvironment *env ;
    RTSPClient       *rtsp;

    /* */
    int              i_track;
    live_track_t     **track;   /* XXX mallocated */
    int64_t          i_pcr;
    int64_t          i_pcr_start;
    int64_t          i_pcr_previous;
    int64_t          i_pcr_repeatdate;
    int              i_pcr_repeats;

    /* Asf */
    asf_header_t     asfh;
    stream_t         *p_out_asf;

    /* */
    int64_t          i_length;
    int64_t          i_start;

    /* timeout thread information */
    int              i_timeout;     /* session timeout value in seconds */
    vlc_bool_t       b_timeout_call;/* mark to send an RTSP call to prevent server timeout */
    timeout_thread_t *p_timeout;    /* the actual thread that makes sure we don't timeout */

    /* */
    vlc_bool_t       b_multicast;   /* true if one of the tracks is multicasted */
    vlc_bool_t       b_no_data;     /* true if we never receive any data */
    int              i_no_data_ti;  /* consecutive number of TaskInterrupt */

    char             event;
};

static int Demux  ( demux_t * );
static int Control( demux_t *, int, va_list );

static int Connect      ( demux_t * );
static int SessionsSetup( demux_t * );
static int Play         ( demux_t *);
static int ParseASF     ( demux_t * );
static int RollOverTcp  ( demux_t * );

static void StreamRead  ( void *, unsigned int, unsigned int,
                          struct timeval, unsigned int );
static void StreamClose ( void * );
static void TaskInterrupt( void * );

static void TimeoutPrevention( timeout_thread_t * );

#if LIVEMEDIA_LIBRARY_VERSION_INT >= 1117756800
static unsigned char* parseH264ConfigStr( char const* configStr,
                                          unsigned int& configSize );
#endif

/*****************************************************************************
 * DemuxOpen:
 *****************************************************************************/
static int  Open ( vlc_object_t *p_this )
{
    demux_t     *p_demux = (demux_t*)p_this;
    demux_sys_t *p_sys = NULL;

    MediaSubsessionIterator *iter   = NULL;
    MediaSubsession         *sub    = NULL;
    int i_return;

    if( p_demux->s )
    {
        /* See if it looks like a SDP
           v, o, s fields are mandatory and in this order */
        uint8_t *p_peek;
        if( stream_Peek( p_demux->s, &p_peek, 7 ) < 7 ) return VLC_EGENERIC;

        if( memcmp( (char*)p_peek, "v=0\r\n", 5 ) &&
            memcmp( (char*)p_peek, "v=0\n", 4 ) &&
            ( p_peek[0] < 'a' || p_peek[0] > 'z' || p_peek[1] != '=' ) )
        {
            return VLC_EGENERIC;
        }
    }
    else
    {
        var_Create( p_demux, "rtsp-caching", VLC_VAR_INTEGER|VLC_VAR_DOINHERIT );
    }

    p_demux->pf_demux  = Demux;
    p_demux->pf_control= Control;
    p_demux->p_sys     = p_sys = (demux_sys_t*)malloc( sizeof( demux_sys_t ) );
    if( !p_sys ) return VLC_ENOMEM;

    p_sys->p_sdp = NULL;
    p_sys->scheduler = NULL;
    p_sys->env = NULL;
    p_sys->ms = NULL;
    p_sys->rtsp = NULL;
    p_sys->i_track = 0;
    p_sys->track   = NULL;
    p_sys->i_pcr   = 0;
    p_sys->i_pcr_start = 0;
    p_sys->i_pcr_previous = 0;
    p_sys->i_pcr_repeatdate = 0;
    p_sys->i_pcr_repeats = 0;
    p_sys->i_length = 0;
    p_sys->i_start = 0;
    p_sys->p_out_asf = NULL;
    p_sys->b_no_data = VLC_TRUE;
    p_sys->i_no_data_ti = 0;
    p_sys->p_timeout = NULL;
    p_sys->i_timeout = 0;
    p_sys->b_timeout_call = VLC_FALSE;
    p_sys->b_multicast = VLC_FALSE;
    p_sys->psz_path = strdup( p_demux->psz_path );

    /* parse URL for rtsp://[user:[passwd]@]serverip:port/options */
    vlc_UrlParse( &p_sys->url, p_sys->psz_path, 0 );

    if( ( p_sys->scheduler = BasicTaskScheduler::createNew() ) == NULL )
    {
        msg_Err( p_demux, "BasicTaskScheduler::createNew failed" );
        goto error;
    }
    if( !( p_sys->env = BasicUsageEnvironment::createNew(*p_sys->scheduler) ) )
    {
        msg_Err( p_demux, "BasicUsageEnvironment::createNew failed" );
        goto error;
    }

    if( strcasecmp( p_demux->psz_access, "sdp" ) )
    {
        char *p = p_sys->psz_path;
        while( (p = strchr( p, ' ' )) != NULL ) *p = '+';
    }

    if( p_demux->s != NULL )
    {
        /* Gather the complete sdp file */
        int     i_sdp       = 0;
        int     i_sdp_max   = 1000;
        uint8_t *p_sdp      = (uint8_t*) malloc( i_sdp_max );

        if( !p_sdp )
            goto error;

        for( ;; )
        {
            int i_read = stream_Read( p_demux->s, &p_sdp[i_sdp],
                                      i_sdp_max - i_sdp - 1 );

            if( p_demux->b_die || p_demux->b_error )
            {
                free( p_sdp );
                goto error;
            }

            if( i_read < 0 )
            {
                msg_Err( p_demux, "failed to read SDP" );
                free( p_sdp );
                goto error;
            }

            i_sdp += i_read;

            if( i_read < i_sdp_max - i_sdp - 1 )
            {
                p_sdp[i_sdp] = '\0';
                break;
            }

            i_sdp_max += 1000;
            p_sdp = (uint8_t*)realloc( p_sdp, i_sdp_max );
        }
        p_sys->p_sdp = (char*)p_sdp;
    }
    else if( p_demux->s == NULL && !strcasecmp( p_demux->psz_access, "sdp" ) )
    {
        /* sdp:// link from SAP */
        p_sys->p_sdp = strdup( p_sys->psz_path );
    }
    else if( ( i_return = Connect( p_demux ) ) != VLC_SUCCESS )
    {
        msg_Err( p_demux, "Failed to connect with rtsp://%s", p_sys->psz_path );
        goto error;
    }

    if( p_sys->p_sdp == NULL )
    {
        msg_Err( p_demux, "Failed to retrieve the RTSP Session Description" );
        goto error;
    }

    /* Create the session from the SDP */
    if( !( p_sys->ms = MediaSession::createNew( *p_sys->env, p_sys->p_sdp ) ) )
    {
        msg_Err( p_demux, "Could not create the RTSP Session: %s",
            p_sys->env->getResultMsg() );
        goto error;
    }

    i_return = SessionsSetup( p_demux );
    if( i_return != VLC_SUCCESS )
    {
        msg_Err( p_demux, "Nothing to play for rtsp://%s", p_sys->psz_path );
        goto error;
    }

    /* Retrieve the duration if possible */
    p_sys->i_length = (int64_t)( p_sys->ms->playEndTime() * 1000000.0 );
    if( p_sys->i_length < 0 )
        p_sys->i_length = -1;

    i_return = Play( p_demux );
    if( i_return != VLC_SUCCESS )
        goto error;

    /* Create all es struct */
    iter = new MediaSubsessionIterator( *p_sys->ms );
    while( ( sub = iter->next() ) != NULL )
    {
        live_track_t *tk;

        if( p_demux->b_die || p_demux->b_error )
        {
            delete iter;
            goto error;
        }

        /* Check if we will receive data from this subsession for this track */
        if( sub->readSource() == NULL ) continue;

        tk = (live_track_t*)malloc( sizeof( live_track_t ) );
	if( !tk ) 
        {
            delete iter;
            goto error;
        }
        tk->p_demux = p_demux;
        tk->waiting = 0;
        tk->i_pts   = 0;
        tk->b_quicktime = VLC_FALSE;
        tk->b_muxed     = VLC_FALSE;
        tk->b_asf       = VLC_FALSE;
        tk->b_rtcp_sync = VLC_FALSE;
        tk->p_out_muxed = NULL;
        tk->p_es        = NULL;
        tk->i_buffer    = 65536;
        tk->p_buffer    = (uint8_t *)malloc( 65536 );

        if( !tk->p_buffer )
        {
            delete iter;
            goto error;
        }

        /* Value taken from mplayer */
        if( !strcmp( sub->mediumName(), "audio" ) )
        {
            es_format_Init( &tk->fmt, AUDIO_ES, VLC_FOURCC('u','n','d','f') );
            tk->fmt.audio.i_channels = sub->numChannels();
            tk->fmt.audio.i_rate = sub->rtpTimestampFrequency();

            if( !strcmp( sub->codecName(), "MPA" ) ||
                !strcmp( sub->codecName(), "MPA-ROBUST" ) ||
                !strcmp( sub->codecName(), "X-MP3-DRAFT-00" ) )
            {
                tk->fmt.i_codec = VLC_FOURCC( 'm', 'p', 'g', 'a' );
                tk->fmt.audio.i_rate = 0;
            }
            else if( !strcmp( sub->codecName(), "AC3" ) )
            {
                tk->fmt.i_codec = VLC_FOURCC( 'a', '5', '2', ' ' );
                tk->fmt.audio.i_rate = 0;
            }
            else if( !strcmp( sub->codecName(), "L16" ) )
            {
                tk->fmt.i_codec = VLC_FOURCC( 't', 'w', 'o', 's' );
                tk->fmt.audio.i_bitspersample = 16;
            }
            else if( !strcmp( sub->codecName(), "L8" ) )
            {
                tk->fmt.i_codec = VLC_FOURCC( 'a', 'r', 'a', 'w' );
                tk->fmt.audio.i_bitspersample = 8;
            }
            else if( !strcmp( sub->codecName(), "PCMU" ) )
            {
                tk->fmt.i_codec = VLC_FOURCC( 'u', 'l', 'a', 'w' );
            }
            else if( !strcmp( sub->codecName(), "PCMA" ) )
            {
                tk->fmt.i_codec = VLC_FOURCC( 'a', 'l', 'a', 'w' );
            }
            else if( !strncmp( sub->codecName(), "G726", 4 ) )
            {
                tk->fmt.i_codec = VLC_FOURCC( 'g', '7', '2', '6' ); 
                tk->fmt.audio.i_rate = 8000;
                tk->fmt.audio.i_channels = 1;
                if( !strcmp( sub->codecName()+5, "40" ) )
                    tk->fmt.i_bitrate = 40000;
                else if( !strcmp( sub->codecName()+5, "32" ) )
                    tk->fmt.i_bitrate = 32000;
                else if( !strcmp( sub->codecName()+5, "24" ) )
                    tk->fmt.i_bitrate = 24000;
                else if( !strcmp( sub->codecName()+5, "16" ) )
                    tk->fmt.i_bitrate = 16000;
            }
            else if( !strcmp( sub->codecName(), "AMR" ) )
            {
                tk->fmt.i_codec = VLC_FOURCC( 's', 'a', 'm', 'r' );
            }
            else if( !strcmp( sub->codecName(), "AMR-WB" ) )
            {
                tk->fmt.i_codec = VLC_FOURCC( 's', 'a', 'w', 'b' );
            }
            else if( !strcmp( sub->codecName(), "MP4A-LATM" ) )
            {
                unsigned int i_extra;
                uint8_t      *p_extra;

                tk->fmt.i_codec = VLC_FOURCC( 'm', 'p', '4', 'a' );

                if( ( p_extra = parseStreamMuxConfigStr( sub->fmtp_config(),
                                                         i_extra ) ) )
                {
                    tk->fmt.i_extra = i_extra;
                    tk->fmt.p_extra = malloc( i_extra );
                    memcpy( tk->fmt.p_extra, p_extra, i_extra );
                    delete[] p_extra;
                }

#if LIVEMEDIA_LIBRARY_VERSION_INT >= 1141257600
                /* Because the "faad" decoder does not handle the LATM data length field
                   at the start of each returned LATM frame, tell the RTP source to omit it. */
                ((MPEG4LATMAudioRTPSource*)sub->rtpSource())->omitLATMDataLengthField();
#endif
            }
            else if( !strcmp( sub->codecName(), "MPEG4-GENERIC" ) )
            {
                unsigned int i_extra;
                uint8_t      *p_extra;

                tk->fmt.i_codec = VLC_FOURCC( 'm', 'p', '4', 'a' );

                if( ( p_extra = parseGeneralConfigStr( sub->fmtp_config(),
                                                       i_extra ) ) )
                {
                    tk->fmt.i_extra = i_extra;
                    tk->fmt.p_extra = malloc( i_extra );
                    memcpy( tk->fmt.p_extra, p_extra, i_extra );
                    delete[] p_extra;
                }
            }
            else if( !strcmp( sub->codecName(), "X-ASF-PF" ) )
            {
                tk->b_asf = VLC_TRUE;
                if( p_sys->p_out_asf == NULL )
                    p_sys->p_out_asf = stream_DemuxNew( p_demux, "asf",
                                                        p_demux->out );
            }
        }
        else if( !strcmp( sub->mediumName(), "video" ) )
        {
            es_format_Init( &tk->fmt, VIDEO_ES, VLC_FOURCC('u','n','d','f') );
            if( !strcmp( sub->codecName(), "MPV" ) )
            {
                tk->fmt.i_codec = VLC_FOURCC( 'm', 'p', 'g', 'v' );
            }
            else if( !strcmp( sub->codecName(), "H263" ) ||
                     !strcmp( sub->codecName(), "H263-1998" ) ||
                     !strcmp( sub->codecName(), "H263-2000" ) )
            {
                tk->fmt.i_codec = VLC_FOURCC( 'H', '2', '6', '3' );
            }
            else if( !strcmp( sub->codecName(), "H261" ) )
            {
                tk->fmt.i_codec = VLC_FOURCC( 'H', '2', '6', '1' );
            }
            else if( !strcmp( sub->codecName(), "H264" ) )
            {
#if LIVEMEDIA_LIBRARY_VERSION_INT >= 1117756800
                unsigned int i_extra = 0;
                uint8_t      *p_extra = NULL;
#endif
                tk->fmt.i_codec = VLC_FOURCC( 'h', '2', '6', '4' );
                tk->fmt.b_packetized = VLC_FALSE;

                /* XXX not the right minimal version I fear */
#if LIVEMEDIA_LIBRARY_VERSION_INT >= 1117756800
                if((p_extra=parseH264ConfigStr( sub->fmtp_spropparametersets(),
                                                i_extra ) ) )
                {
                    tk->fmt.i_extra = i_extra;
                    tk->fmt.p_extra = malloc( i_extra );
                    memcpy( tk->fmt.p_extra, p_extra, i_extra );

                    delete[] p_extra;
                }
#endif
            }
            else if( !strcmp( sub->codecName(), "JPEG" ) )
            {
                tk->fmt.i_codec = VLC_FOURCC( 'M', 'J', 'P', 'G' );
            }
            else if( !strcmp( sub->codecName(), "MP4V-ES" ) )
            {
                unsigned int i_extra;
                uint8_t      *p_extra;

                tk->fmt.i_codec = VLC_FOURCC( 'm', 'p', '4', 'v' );

                if( ( p_extra = parseGeneralConfigStr( sub->fmtp_config(),
                                                       i_extra ) ) )
                {
                    tk->fmt.i_extra = i_extra;
                    tk->fmt.p_extra = malloc( i_extra );
                    memcpy( tk->fmt.p_extra, p_extra, i_extra );
                    delete[] p_extra;
                }
            }
            else if( !strcmp( sub->codecName(), "X-QT" ) ||
                     !strcmp( sub->codecName(), "X-QUICKTIME" ) ||
                     !strcmp( sub->codecName(), "X-QDM" ) ||
                     !strcmp( sub->codecName(), "X-SV3V-ES" )  ||
                     !strcmp( sub->codecName(), "X-SORENSONVIDEO" ) )
            {
                tk->b_quicktime = VLC_TRUE;
            }
            else if( !strcmp( sub->codecName(), "MP2T" ) )
            {
                tk->b_muxed = VLC_TRUE;
                tk->p_out_muxed = stream_DemuxNew( p_demux, "ts", p_demux->out );
            }
            else if( !strcmp( sub->codecName(), "MP2P" ) ||
                     !strcmp( sub->codecName(), "MP1S" ) )
            {
                tk->b_muxed = VLC_TRUE;
                tk->p_out_muxed = stream_DemuxNew( p_demux, "ps",
                                                   p_demux->out );
            }
            else if( !strcmp( sub->codecName(), "X-ASF-PF" ) )
            {
                tk->b_asf = VLC_TRUE;
                if( p_sys->p_out_asf == NULL )
                    p_sys->p_out_asf = stream_DemuxNew( p_demux, "asf",
                                                        p_demux->out );;
            }
        }

        if( !tk->b_quicktime && !tk->b_muxed && !tk->b_asf )
        {
            tk->p_es = es_out_Add( p_demux->out, &tk->fmt );
        }

        if( sub->rtcpInstance() != NULL )
        {
            sub->rtcpInstance()->setByeHandler( StreamClose, tk );
        }

        if( tk->p_es || tk->b_quicktime || tk->b_muxed || tk->b_asf )
        {
            tk->readSource = sub->readSource();
            tk->rtpSource  = sub->rtpSource();

            /* Append */
            p_sys->track = (live_track_t**)realloc( p_sys->track, sizeof( live_track_t ) * ( p_sys->i_track + 1 ) );
            p_sys->track[p_sys->i_track++] = tk;
        }
        else
        {
            /* BUG ??? */
            msg_Err( p_demux, "unusable RTSP track. this should not happen" );
            free( tk );
        }
    }

    delete iter;

    if( p_sys->p_out_asf && ParseASF( p_demux ) )
    {
        msg_Err( p_demux, "cannot find a usable asf header" );
        /* TODO Clean tracks */
        goto error;
    }

    if( p_sys->i_track <= 0 )
        goto error;

    return VLC_SUCCESS;

error:
    if( p_sys->p_out_asf ) stream_DemuxDelete( p_sys->p_out_asf );
    if( p_sys->ms ) Medium::close( p_sys->ms );
    if( p_sys->rtsp ) RTSPClient::close( p_sys->rtsp );
    if( p_sys->env ) RECLAIM_ENV(p_sys->env);
    if( p_sys->p_timeout )
    {
        p_sys->p_timeout->b_die = VLC_TRUE;
        vlc_thread_join( p_sys->p_timeout );
        vlc_object_detach( p_sys->p_timeout );
        vlc_object_destroy( p_sys->p_timeout );
    }
    if( p_sys->scheduler ) delete p_sys->scheduler;
    if( p_sys->p_sdp ) free( p_sys->p_sdp );
    if( p_sys->psz_path ) free( p_sys->psz_path );

    vlc_UrlClean( &p_sys->url );

    free( p_sys );
    return VLC_EGENERIC;
}

/*****************************************************************************
 * DemuxClose:
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    demux_t *p_demux = (demux_t*)p_this;
    demux_sys_t *p_sys = p_demux->p_sys;
    int i;

    for( i = 0; i < p_sys->i_track; i++ )
    {
        live_track_t *tk = p_sys->track[i];

        if( tk->b_muxed ) stream_DemuxDelete( tk->p_out_muxed );
        free( tk->p_buffer );
        free( tk );
    }

    if( p_sys->i_track ) free( p_sys->track );
    if( p_sys->p_out_asf ) stream_DemuxDelete( p_sys->p_out_asf );

    if( p_sys->rtsp && p_sys->ms )
    {
        /* TEARDOWN */
        p_sys->rtsp->teardownMediaSession( *p_sys->ms );
    }

    Medium::close( p_sys->ms );

    if( p_sys->p_timeout )
    {
        p_sys->p_timeout->b_die = VLC_TRUE;
        vlc_thread_join( p_sys->p_timeout );
        vlc_object_detach( p_sys->p_timeout );
        vlc_object_destroy( p_sys->p_timeout );
    }

    if( p_sys->rtsp ) RTSPClient::close( p_sys->rtsp );
    if( p_sys->env ) RECLAIM_ENV( p_sys->env );
    if( p_sys->scheduler ) delete p_sys->scheduler;
    if( p_sys->p_sdp ) free( p_sys->p_sdp );
    if( p_sys->psz_path ) free( p_sys->psz_path );

    vlc_UrlClean( &p_sys->url );

    free( p_sys );
}

/*****************************************************************************
 * Connect: connects to the RTSP server to setup the session DESCRIBE
 *****************************************************************************/
static int Connect( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    Authenticator authenticator;
    vlc_bool_t b_firsttime = VLC_TRUE; // HACK FOR LIVE555

    char *psz_user    = NULL;
    char *psz_pwd     = NULL;
    char *psz_url     = NULL;
    char *psz_options = NULL;
    char *p_sdp       = NULL;
    int  i_http_port  = 0;
    int  i_ret        = VLC_SUCCESS;

    psz_url = (char*)malloc( strlen( p_sys->psz_path ) + 8 );
    if( !psz_url ) return VLC_ENOMEM;

    if( p_sys->url.psz_username || p_sys->url.psz_password )
    {
        sprintf( psz_url, "rtsp://%s%s", p_sys->url.psz_host,
                 p_sys->url.psz_path );

        psz_user = strdup( p_sys->url.psz_username );
        psz_pwd  = strdup( p_sys->url.psz_password );
    }
    else
    {
        sprintf( psz_url, "rtsp://%s", p_sys->psz_path );

        psz_user = var_CreateGetString( p_demux, "rtsp-user" );
        psz_pwd  = var_CreateGetString( p_demux, "rtsp-pwd" );
    }

createnew:
    if( p_demux->b_die || p_demux->b_error )
    {
        free( psz_user );
        free( psz_pwd );
        free( psz_url );
        return VLC_EGENERIC;
    }

    if( var_CreateGetBool( p_demux, "rtsp-http" ) )
        i_http_port = var_CreateGetInteger( p_demux, "rtsp-http-port" );

#if LIVEMEDIA_LIBRARY_VERSION_INT > 1130457500
    if( ( p_sys->rtsp = RTSPClient::createNew( *p_sys->env,
          p_demux->p_libvlc->i_verbose > 1,
          "VLC media player", i_http_port ) ) == NULL )
#else
    if( ( p_sys->rtsp = RTSPClient::createNew( *p_sys->env,
          p_demux->p_libvlc->i_verbose > 1,
          "VLC media player" ) ) == NULL )
#endif
    {
        msg_Err( p_demux, "RTSPClient::createNew failed (%s)",
                 p_sys->env->getResultMsg() );
        free( psz_user );
        free( psz_pwd );
        free( psz_url );
        return VLC_EGENERIC;
    }

    /* Kasenna enables KeepAlive by analysing the User-Agent string. 
     * Appending _KA to the string should be enough to enable this feature, 
     * however, there is a bug where the _KA doesn't get parsed from the 
     * standard User-Agent string as used above. The but is probably due to
     * spaces in the string or the string being too long when Live55 also
     * appends it's own name to the string */
    if( var_CreateGetBool( p_demux, "rtsp-kasenna" ))
    {
#if LIVEMEDIA_LIBRARY_VERSION_INT > 1130457500
        p_sys->rtsp->setUserAgentString( "VLC_MEDIA_PLAYER_KA" );
#endif
    }

describe:
    authenticator.setUsernameAndPassword( (const char*)psz_user,
                                          (const char*)psz_pwd );

    psz_options = p_sys->rtsp->sendOptionsCmd( psz_url, psz_user, psz_pwd,
                                               &authenticator );
    if( psz_options ) delete [] psz_options;

    p_sdp = p_sys->rtsp->describeURL( psz_url, &authenticator,
                    var_CreateGetBool( p_demux, "rtsp-kasenna" ) );
    if( p_sdp == NULL )
    {
        /* failure occurred */
        uint32_t i_code = 0;
        const char *psz_error = p_sys->env->getResultMsg();

        if( var_GetBool( p_demux, "rtsp-http" ) )
        {
            sscanf( psz_error, "%*s %*s HTTP GET %*s HTTP/%*u.%*u %3u %*s",
                    &i_code );
        }
        else
        {
            const char *psz_tmp = strstr( psz_error, "RTSP" );
            sscanf( psz_tmp, "RTSP/%*s%3u", &i_code );
        }
        msg_Dbg( p_demux, "DESCRIBE failed with %d: [%s]", i_code, psz_error );

        if( b_firsttime )
        { /* HACK FOR LIVE555 */
            i_code = 0;
            b_firsttime = VLC_FALSE;
        }

        if( i_code == 401 )
        {
            int i_result;
            msg_Dbg( p_demux, "authentication failed" );

            if( psz_user ) free( psz_user );
            if( psz_pwd ) free( psz_pwd );
            psz_user = psz_pwd = NULL;

            i_result = intf_UserLoginPassword( p_demux, _("RTSP authentication"),
                           _("Please enter a valid login name and a password."),
                                                   &psz_user, &psz_pwd );
            if( i_result == DIALOG_OK_YES )
            {
                msg_Dbg( p_demux, "retrying with user=%s, pwd=%s",
                         psz_user, psz_pwd );
                goto describe;
            }
        }
        else if( (i_code != 0) && !var_GetBool( p_demux, "rtsp-http" ) )
        {
            /* Perhaps a firewall is being annoying. Try HTTP tunneling mode */
            vlc_value_t val;
            val.b_bool = VLC_TRUE;
            msg_Dbg( p_demux, "we will now try HTTP tunneling mode" );
            var_Set( p_demux, "rtsp-http", val );
            if( p_sys->rtsp ) RTSPClient::close( p_sys->rtsp );
            p_sys->rtsp = NULL;
            goto createnew;
        }
        else
        {
            msg_Dbg( p_demux, "connection timeout, retrying" );
            if( p_sys->rtsp ) RTSPClient::close( p_sys->rtsp );
            p_sys->rtsp = NULL;
            goto createnew;
        }
        i_ret = VLC_EGENERIC;
    }

    /* malloc-ated copy */
    if( psz_url ) free( psz_url );
    if( psz_user ) free( psz_user );
    if( psz_pwd ) free( psz_pwd );

    if( p_sys->p_sdp ) free( p_sys->p_sdp );
    p_sys->p_sdp = NULL;
    if( p_sdp ) p_sys->p_sdp = strdup( (char*)p_sdp );
    delete[] p_sdp;

    return i_ret;
}

/*****************************************************************************
 * Connect: prepares the subsessions and does the SETUP
 *****************************************************************************/
static int SessionsSetup( demux_t *p_demux )
{
    demux_sys_t             *p_sys  = p_demux->p_sys;
    MediaSubsessionIterator *iter   = NULL;
    MediaSubsession         *sub    = NULL;

    vlc_bool_t     b_rtsp_tcp = VLC_FALSE;
    int            i_client_port;
    int            i_active_sessions = 0;
    int            i_return = VLC_SUCCESS;
    unsigned int   i_buffer = 0;
    unsigned const thresh = 200000; /* RTP reorder threshold .2 second (default .1) */

    b_rtsp_tcp    = var_CreateGetBool( p_demux, "rtsp-tcp" ) ||
                    var_GetBool( p_demux, "rtsp-http" );
    i_client_port = var_CreateGetInteger( p_demux, "rtp-client-port" );

    /* Initialise each media subsession */
    iter = new MediaSubsessionIterator( *p_sys->ms );
    while( ( sub = iter->next() ) != NULL )
    {
        Boolean bInit;

        /* Value taken from mplayer */
        if( !strcmp( sub->mediumName(), "audio" ) )
            i_buffer = 100000;
        else if( !strcmp( sub->mediumName(), "video" ) )
            i_buffer = 2000000;
        else continue;

        if( i_client_port != -1 )
        {
            sub->setClientPortNum( i_client_port );
            i_client_port += 2;
        }

        if( strcasestr( sub->codecName(), "REAL" ) )
        {
            msg_Info( p_demux, "real codec detected, using real-RTSP instead" );
            i_return = VLC_EGENERIC;
            break;
        }

        if( !strcmp( sub->codecName(), "X-ASF-PF" ) )
            bInit = sub->initiate( 4 ); /* Constant ? */
        else
            bInit = sub->initiate();

        if( !bInit )
        {
            msg_Warn( p_demux, "RTP subsession '%s/%s' failed (%s)",
                      sub->mediumName(), sub->codecName(),
                      p_sys->env->getResultMsg() );
        }
        else
        {
            if( sub->rtpSource() != NULL )
            {
                int fd = sub->rtpSource()->RTPgs()->socketNum();

                /* Increase the buffer size */
                if( i_buffer > 0 )
                    increaseReceiveBufferTo( *p_sys->env, fd, i_buffer );

                /* Increase the RTP reorder timebuffer just a bit */
                sub->rtpSource()->setPacketReorderingThresholdTime(thresh);
            }

            msg_Dbg( p_demux, "RTP subsession '%s/%s'", sub->mediumName(),
                     sub->codecName() );

            /* Issue the SETUP */
            if( p_sys->rtsp )
            {
                if( !( p_sys->rtsp->setupMediaSubsession( *sub, False,
                                                   b_rtsp_tcp ? True : False ) ) )
                {
                    /* if we get an unsupported transport error, toggle TCP use and try again */
                    if( !strstr(p_sys->env->getResultMsg(), "461 Unsupported Transport")
                     || !( p_sys->rtsp->setupMediaSubsession( *sub, False,
                                                   b_rtsp_tcp ? False : True ) ) )
                    {
                        msg_Err( p_demux, "SETUP of'%s/%s' failed %s", sub->mediumName(),
                                 sub->codecName(), p_sys->env->getResultMsg() );
                        continue;
                    }
                    else i_active_sessions++;
                }
                else i_active_sessions++;
            } else i_active_sessions++; /* we don't really know, let's just hope it's there */

            if( !p_sys->b_multicast )
            {
                /* Check, because we need diff. rollover behaviour for multicast */
                p_sys->b_multicast = IsMulticastAddress( sub->connectionEndpointAddress() );
            }
        }
    }
    delete iter;
    if( i_active_sessions <= 0 ) i_return = VLC_EGENERIC;
    return i_return;
}

/*****************************************************************************
 * Play: starts the actual playback of the stream
 *****************************************************************************/
static int Play( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    if( p_sys->rtsp )
    {
        /* The PLAY */
        if( !p_sys->rtsp->playMediaSession( *p_sys->ms ) )
        {
            msg_Err( p_demux, "RTSP PLAY failed %s", p_sys->env->getResultMsg() );
            return VLC_EGENERIC;
        }

        /* Retrieve the timeout value and set up a timeout prevention thread */
#if LIVEMEDIA_LIBRARY_VERSION_INT >= 1138089600
        p_sys->i_timeout = p_sys->rtsp->sessionTimeoutParameter();
#endif
        if( p_sys->i_timeout <= 0 )
            p_sys->i_timeout = 60; /* default value from RFC2326 */

        /* start timeout-thread only on x-asf streams (wms), it has rtcp support but doesn't
         * seem to use it for liveness/keep-alive, get_parameter seems to work for it. get_parameter
         * doesn't work with dss 5.5.4 & 5.5.5, they seems to work with rtcp */
        if( !p_sys->p_timeout && p_sys->p_out_asf )
        {
            msg_Dbg( p_demux, "We have a timeout of %d seconds",  p_sys->i_timeout );
            p_sys->p_timeout = (timeout_thread_t *)vlc_object_create( p_demux, sizeof(timeout_thread_t) );
            p_sys->p_timeout->p_sys = p_demux->p_sys; /* lol, object recursion :D */
            if( vlc_thread_create( p_sys->p_timeout, "liveMedia-timeout", TimeoutPrevention,
                                   VLC_THREAD_PRIORITY_LOW, VLC_TRUE ) )
            {
                msg_Err( p_demux, "cannot spawn liveMedia timeout thread" );
                vlc_object_destroy( p_sys->p_timeout );
            }
            msg_Dbg( p_demux, "spawned timeout thread" );
            vlc_object_attach( p_sys->p_timeout, p_demux );
        }
    }
    return VLC_SUCCESS;
}


/*****************************************************************************
 * Demux:
 *****************************************************************************/
static int Demux( demux_t *p_demux )
{
    demux_sys_t    *p_sys = p_demux->p_sys;
    TaskToken      task;

    vlc_bool_t      b_send_pcr = VLC_TRUE;
    int64_t         i_pcr = 0;
    int             i;

    /* Check if we need to send the server a Keep-A-Live signal */
    if( p_sys->b_timeout_call && p_sys->rtsp && p_sys->ms )
    {
#if LIVEMEDIA_LIBRARY_VERSION_INT >= 1138089600
        char *psz_bye = NULL;
        p_sys->rtsp->getMediaSessionParameter( *p_sys->ms, NULL, psz_bye );
#endif
        p_sys->b_timeout_call = VLC_FALSE;
    }

    for( i = 0; i < p_sys->i_track; i++ )
    {
        live_track_t *tk = p_sys->track[i];

        if( tk->b_asf || tk->b_muxed )
            b_send_pcr = VLC_FALSE;

        if( i_pcr == 0 )
        {
            i_pcr = tk->i_pts;
        }
        else if( tk->i_pts != 0 && i_pcr > tk->i_pts )
        {
            i_pcr = tk->i_pts ;
        }
    }
    if( i_pcr != p_sys->i_pcr && i_pcr > 0 )
    {
        p_sys->i_pcr = i_pcr;

        if( b_send_pcr )
            es_out_Control( p_demux->out, ES_OUT_SET_PCR, i_pcr );
        if( p_sys->i_pcr_start <= 0 || p_sys->i_pcr_start > i_pcr ||
            ( p_sys->i_length > 0 && i_pcr - p_sys->i_pcr_start > p_sys->i_length ) )
        {
            p_sys->i_pcr_start = i_pcr;
        }
    }

#if 0
    /* Disabled because it's simply not reliable enough */

    /* When a On Demand QT stream ends, the last frame keeps going with the same PCR/PTS value */
    /* This tests for that, so we can later decide to end this session */
    if( i_pcr > 0 && p_sys->i_pcr == p_sys->i_pcr_previous )
    {
        if( p_sys->i_pcr_repeats == 0 )
            p_sys->i_pcr_repeatdate = mdate();
        p_sys->i_pcr_repeats++;
    }
    else
    {
        p_sys->i_pcr_previous = p_sys->i_pcr;
        p_sys->i_pcr_repeatdate = 0;
        p_sys->i_pcr_repeats = 0;
    }

    if( p_sys->i_pcr_repeats > 5 && mdate() > p_sys->i_pcr_repeatdate + 1000000 )
    {
        /* We need at least 5 repeats over at least a second of time before we EOF */
        msg_Dbg( p_demux, "suspect EOF due to end of VoD session" );
        return 0;
    }
#endif

    /* First warn we want to read data */
    p_sys->event = 0;
    for( i = 0; i < p_sys->i_track; i++ )
    {
        live_track_t *tk = p_sys->track[i];

        if( tk->waiting == 0 )
        {
            tk->waiting = 1;
            tk->readSource->getNextFrame( tk->p_buffer, tk->i_buffer,
                                          StreamRead, tk, StreamClose, tk );
        }
    }
    /* Create a task that will be called if we wait more than 300ms */
    task = p_sys->scheduler->scheduleDelayedTask( 300000, TaskInterrupt, p_demux );

    /* Do the read */
    p_sys->scheduler->doEventLoop( &p_sys->event );

    /* remove the task */
    p_sys->scheduler->unscheduleDelayedTask( task );

    /* Check for gap in pts value */
    for( i = 0; i < p_sys->i_track; i++ )
    {
        live_track_t *tk = p_sys->track[i];

        if( !tk->b_muxed && !tk->b_rtcp_sync &&
            tk->rtpSource && tk->rtpSource->hasBeenSynchronizedUsingRTCP() )
        {
            msg_Dbg( p_demux, "tk->rtpSource->hasBeenSynchronizedUsingRTCP()" );

            es_out_Control( p_demux->out, ES_OUT_RESET_PCR );
            tk->b_rtcp_sync = VLC_TRUE;

            /* reset PCR and PCR start, mmh won't work well for multi-stream I fear */
            tk->i_pts = 0;
            p_sys->i_pcr_start = 0;
            p_sys->i_pcr = 0;
            i_pcr = 0;
        }
    }

    if( p_sys->b_multicast && p_sys->b_no_data && p_sys->i_no_data_ti > 120 )
    {
        /* FIXME Make this configurable
        msg_Err( p_demux, "no multicast data received in 36s, aborting" );
        return 0;
        */
    }
    else if( !p_sys->b_multicast && p_sys->b_no_data && p_sys->i_no_data_ti > 34 )
    {
        vlc_bool_t b_rtsp_tcp = var_GetBool( p_demux, "rtsp-tcp" ) ||
                                var_GetBool( p_demux, "rtsp-http" );

        if( !b_rtsp_tcp && p_sys->rtsp && p_sys->ms )
        {
            msg_Warn( p_demux, "no data received in 10s. Switching to TCP" );
            if( RollOverTcp( p_demux ) )
            {
                msg_Err( p_demux, "TCP rollover failed, aborting" );
                return 0;
            }
        }
        else if( p_sys->i_no_data_ti > 34 )
        {
            msg_Err( p_demux, "no data received in 10s, aborting" );
            return 0;
        }
    }
    else if( !p_sys->b_multicast && p_sys->b_no_data && p_sys->i_no_data_ti > 34 )
    {
        /* EOF ? */
        msg_Warn( p_demux, "no data received in 10s, eof ?" );
        return 0;
    }

    return p_demux->b_error ? 0 : 1;
}

/*****************************************************************************
 * Control:
 *****************************************************************************/
static int Control( demux_t *p_demux, int i_query, va_list args )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    int64_t *pi64;
    double  *pf, f;
    vlc_bool_t *pb, b_bool;

    switch( i_query )
    {
        case DEMUX_GET_TIME:
            pi64 = (int64_t*)va_arg( args, int64_t * );
            *pi64 = p_sys->i_pcr - p_sys->i_pcr_start + p_sys->i_start;
            return VLC_SUCCESS;

        case DEMUX_GET_LENGTH:
            pi64 = (int64_t*)va_arg( args, int64_t * );
            *pi64 = p_sys->i_length;
            return VLC_SUCCESS;

        case DEMUX_GET_POSITION:
            pf = (double*)va_arg( args, double* );
            if( p_sys->i_length > 0 )
            {
                *pf = (double)( p_sys->i_pcr - p_sys->i_pcr_start +
                                p_sys->i_start ) / (double)(p_sys->i_length);
            }
            else
            {
                *pf = 0.0;
            }
            return VLC_SUCCESS;

        case DEMUX_SET_POSITION:
            float time;

            f = (double)va_arg( args, double );
            if( p_sys->rtsp && p_sys->i_length > 0 )
            {
                time = f * (double)p_sys->i_length / 1000000.0;   /* in second */
                if( !p_sys->rtsp->playMediaSession( *p_sys->ms, time ) )
                {
                    msg_Err( p_demux, "PLAY failed %s",
                        p_sys->env->getResultMsg() );
                    return VLC_EGENERIC;
                }
                p_sys->i_start = (int64_t)(f * (double)p_sys->i_length);
                p_sys->i_pcr_start = 0;
                p_sys->i_pcr       = 0;

                return VLC_SUCCESS;
            }
            return VLC_SUCCESS;

        /* Special for access_demux */
        case DEMUX_CAN_PAUSE:
            pb = (vlc_bool_t*)va_arg( args, vlc_bool_t * );
            if( p_sys->rtsp && p_sys->i_length )
                /* Not always true, but will be handled in SET_PAUSE_STATE */
                *pb = VLC_TRUE;
            else
                *pb = VLC_FALSE;
            return VLC_SUCCESS;

        case DEMUX_CAN_CONTROL_PACE:
            pb = (vlc_bool_t*)va_arg( args, vlc_bool_t * );

#if 0       /* Disable for now until we have a clock synchro algo
             * which works with something else than MPEG over UDP */
            *pb = VLC_FALSE;
#else
            *pb = VLC_TRUE;
#endif
            return VLC_SUCCESS;

        case DEMUX_SET_PAUSE_STATE:
            double d_npt;

            d_npt = ( (double)( p_sys->i_pcr - p_sys->i_pcr_start +
                                p_sys->i_start ) ) / 1000000.00;

            b_bool = (vlc_bool_t)va_arg( args, vlc_bool_t );
            if( p_sys->rtsp == NULL )
                return VLC_EGENERIC;

            if( ( b_bool && !p_sys->rtsp->pauseMediaSession( *p_sys->ms ) ) ||
                    ( !b_bool && !p_sys->rtsp->playMediaSession( *p_sys->ms,
                      d_npt > 0 ? d_npt : -1 ) ) )
            {
                    msg_Err( p_demux, "PLAY or PAUSE failed %s", p_sys->env->getResultMsg() );
                    return VLC_EGENERIC;
            }

            /* When we Pause, we'll need the TimeoutPrevention thread to
             * handle sending the "Keep Alive" message to the server. 
             * Unfortunately Live555 isn't thread safe and so can't 
             * do this normally while the main Demux thread is handling
             * a live stream. We end up with the Timeout thread blocking
             * waiting for a response from the server. So when we PAUSE
             * we set a flag that the TimeoutPrevention function will check
             * and if it's set, it will trigger the GET_PARAMETER message */
            if( b_bool && p_sys->p_timeout != NULL )
                p_sys->p_timeout->b_handle_keep_alive = VLC_TRUE;
            else if( !b_bool && p_sys->p_timeout != NULL ) 
                p_sys->p_timeout->b_handle_keep_alive = VLC_FALSE;
#if 0
            /* reset PCR and PCR start, mmh won't work well for multi-stream I fear */
            for( i = 0; i < p_sys->i_track; i++ )
            {
                p_sys->track[i]->i_pts = 0;
            }
            p_sys->i_pcr_start = 0; /* FIXME Wrong */
            p_sys->i_pcr = 0;
#endif
            return VLC_SUCCESS;

        case DEMUX_GET_TITLE_INFO:
        case DEMUX_SET_TITLE:
        case DEMUX_SET_SEEKPOINT:
            return VLC_EGENERIC;

        case DEMUX_GET_PTS_DELAY:
            pi64 = (int64_t*)va_arg( args, int64_t * );
            *pi64 = (int64_t)var_GetInteger( p_demux, "rtsp-caching" ) * 1000;
            return VLC_SUCCESS;

        default:
            return VLC_EGENERIC;
    }
}

/*****************************************************************************
 * RollOverTcp: reopen the rtsp into TCP mode
 * XXX: ugly, a lot of code are duplicated from Open()
 *****************************************************************************/
static int RollOverTcp( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    MediaSubsessionIterator *iter = 0;
    MediaSubsession *sub;
    int i_tk;
    int i_return = VLC_SUCCESS;

    var_SetBool( p_demux, "rtsp-tcp", VLC_TRUE );

    if( p_sys->p_out_asf ) stream_DemuxDelete( p_sys->p_out_asf );

    /* We close the old RTSP session */
    p_sys->rtsp->teardownMediaSession( *p_sys->ms );

    Medium::close( p_sys->ms );
    RTSPClient::close( p_sys->rtsp );

    p_sys->ms = NULL;
    p_sys->rtsp = NULL;
    p_sys->p_out_asf = NULL;

    /* Reopen rtsp client */
    i_return = Connect( p_demux );
    if( i_return != VLC_SUCCESS )
    {
        msg_Err( p_demux, "Failed to connect with rtsp://%s", p_sys->psz_path );
        goto error;
    }

    if( p_sys->p_sdp == NULL )
    {
        msg_Err( p_demux, "Failed to retrieve the RTSP Session Description" );
        goto error;
    }

    /* Create the session from the SDP */
    if( !( p_sys->ms = MediaSession::createNew( *p_sys->env, p_sys->p_sdp ) ) )
    {
        msg_Err( p_demux, "Could not create the RTSP Session: %s",
            p_sys->env->getResultMsg() );
        goto error;
    }

    i_return = SessionsSetup( p_demux );
    if( i_return != VLC_SUCCESS )
    {
        msg_Err( p_demux, "Nothing to play for rtsp://%s", p_sys->psz_path );
        goto error;
    }

    /* Retrieve the duration if possible */
    p_sys->i_length = (int64_t)( p_sys->ms->playEndTime() * 1000000.0 );
    if( p_sys->i_length < 0 )
        p_sys->i_length = -1;

    i_return = Play( p_demux );
    if( i_return != VLC_SUCCESS )
        goto error;

    /* Update all tracks */
    iter = new MediaSubsessionIterator( *p_sys->ms );
    i_tk = 0;
    while( ( sub = iter->next() ) != NULL )
    {
        live_track_t *tk;

        if( sub->readSource() == NULL )
            continue;
        if( i_tk >= p_sys->i_track )
        {
            msg_Err( p_demux, "WTF !" );
            goto error;
        }

        tk = p_sys->track[i_tk];

        /* Reset state */
        tk->waiting = 0;
        tk->i_pts   = 0;
        tk->b_rtcp_sync = VLC_FALSE;

        if( sub->rtcpInstance() != NULL )
            sub->rtcpInstance()->setByeHandler( StreamClose, tk );

        tk->readSource = sub->readSource();
        tk->rtpSource  = sub->rtpSource();

        i_tk++;
    }
    delete iter;

    return VLC_SUCCESS;

error:
    if( iter ) delete iter;
    return VLC_EGENERIC;
}


/*****************************************************************************
 *
 *****************************************************************************/
static void StreamRead( void *p_private, unsigned int i_size,
                        unsigned int i_truncated_bytes, struct timeval pts,
                        unsigned int duration )
{
    live_track_t   *tk = (live_track_t*)p_private;
    demux_t        *p_demux = tk->p_demux;
    demux_sys_t    *p_sys = p_demux->p_sys;
    block_t        *p_block;

    int64_t i_pts = (uint64_t)pts.tv_sec * UI64C(1000000) +
        (uint64_t)pts.tv_usec;

    /* XXX Beurk beurk beurk Avoid having negative value XXX */
    i_pts &= UI64C(0x00ffffffffffffff);

    if( tk->b_quicktime && tk->p_es == NULL )
    {
        QuickTimeGenericRTPSource *qtRTPSource =
            (QuickTimeGenericRTPSource*)tk->rtpSource;
        QuickTimeGenericRTPSource::QTState &qtState = qtRTPSource->qtState;
        uint8_t *sdAtom = (uint8_t*)&qtState.sdAtom[4];

        if( qtState.sdAtomSize < 16 + 32 )
        {
            /* invalid */
            p_sys->event = 0xff;
            tk->waiting = 0;
            return;
        }
        tk->fmt.i_codec = VLC_FOURCC(sdAtom[0],sdAtom[1],sdAtom[2],sdAtom[3]);
        tk->fmt.video.i_width  = (sdAtom[28] << 8) | sdAtom[29];
        tk->fmt.video.i_height = (sdAtom[30] << 8) | sdAtom[31];

        if( tk->fmt.i_codec == VLC_FOURCC('a', 'v', 'c', '1') )
        {
            uint8_t *pos = (uint8_t*)qtRTPSource->qtState.sdAtom + 86;
            uint8_t *endpos = (uint8_t*)qtRTPSource->qtState.sdAtom
                              + qtRTPSource->qtState.sdAtomSize;
            while (pos+8 < endpos) {
                unsigned int atomLength = pos[0]<<24 | pos[1]<<16 | pos[2]<<8 | pos[3];
                if( (atomLength == 0) || (atomLength > (unsigned int)(endpos-pos))) break;
                if( memcmp(pos+4, "avcC", 4) == 0 &&
                    atomLength > 8 &&
                    atomLength <= INT_MAX)
                {
                    tk->fmt.i_extra = atomLength-8;
                    tk->fmt.p_extra = malloc( tk->fmt.i_extra );
                    if( !tk->fmt.p_extra )
                        tk->fmt.i_extra = 0;
                    else memcpy(tk->fmt.p_extra, pos+8, atomLength-8);
                    break;
                }
                pos += atomLength;
            }
        }
        else
        {
            tk->fmt.i_extra        = qtState.sdAtomSize - 16;
            tk->fmt.p_extra        = malloc( tk->fmt.i_extra );
            if( !tk->fmt.p_extra )
                tk->fmt.i_extra = 0;
            else memcpy( tk->fmt.p_extra, &sdAtom[12], tk->fmt.i_extra );
        }
        tk->p_es = es_out_Add( p_demux->out, &tk->fmt );
    }

#if 0
    fprintf( stderr, "StreamRead size=%d pts=%lld\n",
             i_size,
             pts.tv_sec * 1000000LL + pts.tv_usec );
#endif

    /* grow buffer if it looks like buffer is too small, but don't eat
     * up all the memory on strange streams */
    if( i_truncated_bytes > 0 && tk->i_buffer < 2000000 )
    {
        void *p_tmp;
        msg_Dbg( p_demux, "lost %d bytes", i_truncated_bytes );
        msg_Dbg( p_demux, "increasing buffer size to %d", tk->i_buffer * 2 );
        tk->i_buffer *= 2;
        p_tmp = realloc( tk->p_buffer, tk->i_buffer );
        if (p_tmp == NULL)
        {
            msg_Warn( p_demux, "realloc failed" );
        }
        else
        {
            tk->p_buffer = (uint8_t*)p_tmp;
        }
    }
    if( i_size > tk->i_buffer )
    {
        msg_Warn( p_demux, "buffer overflow" );
    }
    /* FIXME could i_size be > buffer size ? */
    if( tk->fmt.i_codec == VLC_FOURCC('s','a','m','r') ||
        tk->fmt.i_codec == VLC_FOURCC('s','a','w','b') )
    {
        AMRAudioSource *amrSource = (AMRAudioSource*)tk->readSource;

        p_block = block_New( p_demux, i_size + 1 );
        p_block->p_buffer[0] = amrSource->lastFrameHeader();
        memcpy( p_block->p_buffer + 1, tk->p_buffer, i_size );
    }
    else if( tk->fmt.i_codec == VLC_FOURCC('H','2','6','1') )
    {
#if LIVEMEDIA_LIBRARY_VERSION_INT >= 1081468800
        H261VideoRTPSource *h261Source = (H261VideoRTPSource*)tk->rtpSource;
        uint32_t header = h261Source->lastSpecialHeader();
#else
        uint32_t header = 0;
        msg_Warn( p_demux, "need livemedia library >= \"2004.04.09\"" );
#endif
        p_block = block_New( p_demux, i_size + 4 );
        memcpy( p_block->p_buffer, &header, 4 );
        memcpy( p_block->p_buffer + 4, tk->p_buffer, i_size );

        if( tk->rtpSource->curPacketMarkerBit() )
            p_block->i_flags |= BLOCK_FLAG_END_OF_FRAME;
    }
    else if( tk->fmt.i_codec == VLC_FOURCC('h','2','6','4') )
    {
        if( (tk->p_buffer[0] & 0x1f) >= 24 )
            msg_Warn( p_demux, "unsupported NAL type for H264" );

        /* Normal NAL type */
        p_block = block_New( p_demux, i_size + 4 );
        p_block->p_buffer[0] = 0x00;
        p_block->p_buffer[1] = 0x00;
        p_block->p_buffer[2] = 0x00;
        p_block->p_buffer[3] = 0x01;
        memcpy( &p_block->p_buffer[4], tk->p_buffer, i_size );
    }
    else if( tk->b_asf )
    {
        int i_copy = __MIN( p_sys->asfh.i_min_data_packet_size, (int)i_size );
        p_block = block_New( p_demux, p_sys->asfh.i_min_data_packet_size );

        memcpy( p_block->p_buffer, tk->p_buffer, i_copy );
    }
    else
    {
        p_block = block_New( p_demux, i_size );
        memcpy( p_block->p_buffer, tk->p_buffer, i_size );
    }

    if( i_pts != tk->i_pts && !tk->b_muxed )
    {
        p_block->i_dts = ( tk->fmt.i_cat == VIDEO_ES ) ? 0 : i_pts;
        p_block->i_pts = i_pts;
    }

    if( tk->b_muxed )
    {
        stream_DemuxSend( tk->p_out_muxed, p_block );
    }
    else if( tk->b_asf )
    {
        stream_DemuxSend( p_sys->p_out_asf, p_block );
    }
    else
    {
        es_out_Send( p_demux->out, tk->p_es, p_block );
    }

    /* warn that's ok */
    p_sys->event = 0xff;

    /* we have read data */
    tk->waiting = 0;
    p_demux->p_sys->b_no_data = VLC_FALSE;
    p_demux->p_sys->i_no_data_ti = 0;

    if( i_pts > 0 && !tk->b_muxed )
    {
        tk->i_pts = i_pts;
    }
}

/*****************************************************************************
 *
 *****************************************************************************/
static void StreamClose( void *p_private )
{
    live_track_t   *tk = (live_track_t*)p_private;
    demux_t        *p_demux = tk->p_demux;
    demux_sys_t    *p_sys = p_demux->p_sys;

    msg_Dbg( p_demux, "StreamClose" );

    p_sys->event = 0xff;
    p_demux->b_error = VLC_TRUE;
}


/*****************************************************************************
 *
 *****************************************************************************/
static void TaskInterrupt( void *p_private )
{
    demux_t *p_demux = (demux_t*)p_private;

    p_demux->p_sys->i_no_data_ti++;

    /* Avoid lock */
    p_demux->p_sys->event = 0xff;
}

/*****************************************************************************
 *  
 *****************************************************************************/
static void TimeoutPrevention( timeout_thread_t *p_timeout )
{
    p_timeout->b_die = VLC_FALSE;
    p_timeout->i_remain = (int64_t)p_timeout->p_sys->i_timeout -2;
    p_timeout->i_remain *= 1000000;

    vlc_thread_ready( p_timeout );

    /* Avoid lock */
    while( !p_timeout->b_die )
    {
        if( p_timeout->i_remain <= 0 )
        {
            char *psz_bye = NULL;
            p_timeout->i_remain = (int64_t)p_timeout->p_sys->i_timeout -2;
            p_timeout->i_remain *= 1000000;
            msg_Dbg( p_timeout, "reset the timeout timer" );
            if( p_timeout->b_handle_keep_alive == VLC_TRUE)
            {
#if LIVEMEDIA_LIBRARY_VERSION_INT >= 1138089600
                p_timeout->p_sys->rtsp->getMediaSessionParameter( *p_timeout->p_sys->ms, NULL, psz_bye );
#endif
                p_timeout->p_sys->b_timeout_call = VLC_FALSE;
            }
            else
            {
                p_timeout->p_sys->b_timeout_call = VLC_TRUE;
            }
        }
        p_timeout->i_remain -= 200000;
        msleep( 200000 ); /* 200 ms */
    }
}

/*****************************************************************************
 *
 *****************************************************************************/
static int b64_decode( char *dest, char *src );

static int ParseASF( demux_t *p_demux )
{
    demux_sys_t    *p_sys = p_demux->p_sys;

    const char *psz_marker = "a=pgmpu:data:application/vnd.ms.wms-hdr.asfv1;base64,";
    char *psz_asf = strcasestr( p_sys->p_sdp, psz_marker );
    char *psz_end;
    block_t *p_header;

    /* Parse the asf header */
    if( psz_asf == NULL )
        return VLC_EGENERIC;

    psz_asf += strlen( psz_marker );
    psz_asf = strdup( psz_asf );    /* Duplicate it */
    psz_end = strchr( psz_asf, '\n' );

    while( psz_end > psz_asf && ( *psz_end == '\n' || *psz_end == '\r' ) )
        *psz_end-- = '\0';

    if( psz_asf >= psz_end )
    {
        free( psz_asf );
        return VLC_EGENERIC;
    }

    /* Always smaller */
    p_header = block_New( p_demux, psz_end - psz_asf );
    p_header->i_buffer = b64_decode( (char*)p_header->p_buffer, psz_asf );
    //msg_Dbg( p_demux, "Size=%d Hdrb64=%s", p_header->i_buffer, psz_asf );
    if( p_header->i_buffer <= 0 )
    {
        free( psz_asf );
        return VLC_EGENERIC;
    }

    /* Parse it to get packet size */
    E_(asf_HeaderParse)( &p_sys->asfh, p_header->p_buffer, p_header->i_buffer );

    /* Send it to demuxer */
    stream_DemuxSend( p_sys->p_out_asf, p_header );

    free( psz_asf );
    return VLC_SUCCESS;
}

#if LIVEMEDIA_LIBRARY_VERSION_INT >= 1117756800
static unsigned char* parseH264ConfigStr( char const* configStr,
                                          unsigned int& configSize )
{
    char *dup, *psz;
    int i, i_records = 1;

    if( configSize )
    configSize = 0;

    if( configStr == NULL || *configStr == '\0' )
        return NULL;

    psz = dup = strdup( configStr );

    /* Count the number of comma's */
    for( psz = dup; *psz != '\0'; ++psz )
    {
        if( *psz == ',')
        {
            ++i_records;
            *psz = '\0';
        }
    }

    unsigned char *cfg = new unsigned char[5 * strlen(dup)];
    psz = dup;
    for( i = 0; i < i_records; i++ )
    {
        cfg[configSize++] = 0x00;
        cfg[configSize++] = 0x00;
        cfg[configSize++] = 0x00;
        cfg[configSize++] = 0x01;

        configSize += b64_decode( (char*)&cfg[configSize], psz );
        psz += strlen(psz)+1;
    }

    if( dup ) free( dup );
    return cfg;
}
#endif

/*char b64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";*/
static int b64_decode( char *dest, char *src )
{
    const char *dest_start = dest;
    int  i_level;
    int  last = 0;
    int  b64[256] = {
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

    for( i_level = 0; *src != '\0'; src++ )
    {
        int  c;

        c = b64[(unsigned int)*src];
        if( c == -1 )
        {
            continue;
        }

        switch( i_level )
        {
            case 0:
                i_level++;
                break;
            case 1:
                *dest++ = ( last << 2 ) | ( ( c >> 4)&0x03 );
                i_level++;
                break;
            case 2:
                *dest++ = ( ( last << 4 )&0xf0 ) | ( ( c >> 2 )&0x0f );
                i_level++;
                break;
            case 3:
                *dest++ = ( ( last &0x03 ) << 6 ) | c;
                i_level = 0;
        }
        last = c;
    }

    *dest = '\0';

    return dest - dest_start;
}
