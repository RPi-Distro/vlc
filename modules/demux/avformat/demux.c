/*****************************************************************************
 * demux.c: demuxer using ffmpeg (libavformat).
 *****************************************************************************
 * Copyright (C) 2004-2007 the VideoLAN team
 * $Id: ef7957c0fe1e0f037133d1e2c80977cf2213120d $
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Gildas Bazin <gbazin@videolan.org>
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
#include <vlc_stream.h>
#include <vlc_meta.h>
#include <vlc_input.h>
#include <vlc_charset.h>
#include <vlc_avcodec.h>

/* ffmpeg header */
#if defined(HAVE_LIBAVFORMAT_AVFORMAT_H)
#   include <libavformat/avformat.h>
#elif defined(HAVE_FFMPEG_AVFORMAT_H)
#   include <ffmpeg/avformat.h>
#endif

#include "../../codec/avcodec/avcodec.h"
#include "avformat.h"

//#define AVFORMAT_DEBUG 1

/* Version checking */
#if defined(HAVE_FFMPEG_AVFORMAT_H) || defined(HAVE_LIBAVFORMAT_AVFORMAT_H)

#if (LIBAVCODEC_VERSION_INT >= ((51<<16)+(50<<8)+0) )
#   define HAVE_FFMPEG_CODEC_ATTACHMENT 1
#endif

#if (LIBAVFORMAT_VERSION_INT >= ((52<<16)+(15<<8)+0) )
#   define HAVE_FFMPEG_CHAPTERS 1
#endif

/*****************************************************************************
 * demux_sys_t: demux descriptor
 *****************************************************************************/
struct demux_sys_t
{
    ByteIOContext   io;
    int             io_buffer_size;
    uint8_t        *io_buffer;

    AVInputFormat  *fmt;
    AVFormatContext *ic;
    URLContext     url;
    URLProtocol    prot;

    int             i_tk;
    es_out_id_t     **tk;

    int64_t     i_pcr;
    int64_t     i_pcr_inc;
    int         i_pcr_tk;

    int                i_attachments;
    input_attachment_t **attachments;

    /* Only one title with seekpoints possible atm. */
    input_title_t *p_title;
};

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int Demux  ( demux_t *p_demux );
static int Control( demux_t *p_demux, int i_query, va_list args );

static int IORead( void *opaque, uint8_t *buf, int buf_size );
static int64_t IOSeek( void *opaque, int64_t offset, int whence );

static void UpdateSeekPoint( demux_t *p_demux, int64_t i_time );

/*****************************************************************************
 * Open
 *****************************************************************************/
int OpenDemux( vlc_object_t *p_this )
{
    demux_t       *p_demux = (demux_t*)p_this;
    demux_sys_t   *p_sys;
    AVProbeData   pd;
    AVInputFormat *fmt;
    unsigned int  i;
    int64_t       i_start_time = -1;

    /* Init Probe data */
    pd.filename = p_demux->psz_path;
    if( ( pd.buf_size = stream_Peek( p_demux->s, &pd.buf, 2048 ) ) <= 0 )
    {
        msg_Warn( p_demux, "cannot peek" );
        return VLC_EGENERIC;
    }

    av_register_all(); /* Can be called several times */

    /* Guess format */
    if( !( fmt = av_probe_input_format( &pd, 1 ) ) )
    {
        msg_Dbg( p_demux, "couldn't guess format" );
        return VLC_EGENERIC;
    }

    /* Don't try to handle MPEG unless forced */
    if( !p_demux->b_force &&
        ( !strcmp( fmt->name, "mpeg" ) ||
          !strcmp( fmt->name, "vcd" ) ||
          !strcmp( fmt->name, "vob" ) ||
          !strcmp( fmt->name, "mpegts" ) ||
          /* libavformat's redirector won't work */
          !strcmp( fmt->name, "redir" ) ||
          !strcmp( fmt->name, "sdp" ) ) )
    {
        return VLC_EGENERIC;
    }

    /* Don't trigger false alarms on bin files */
    if( !p_demux->b_force && !strcmp( fmt->name, "psxstr" ) )
    {
        int i_len;

        if( !p_demux->psz_path ) return VLC_EGENERIC;

        i_len = strlen( p_demux->psz_path );
        if( i_len < 4 ) return VLC_EGENERIC;

        if( strcasecmp( &p_demux->psz_path[i_len - 4], ".str" ) &&
            strcasecmp( &p_demux->psz_path[i_len - 4], ".xai" ) &&
            strcasecmp( &p_demux->psz_path[i_len - 3], ".xa" ) )
        {
            return VLC_EGENERIC;
        }
    }

    msg_Dbg( p_demux, "detected format: %s", fmt->name );

    /* Fill p_demux fields */
    p_demux->pf_demux = Demux;
    p_demux->pf_control = Control;
    p_demux->p_sys = p_sys = malloc( sizeof( demux_sys_t ) );
    p_sys->ic = 0;
    p_sys->fmt = fmt;
    p_sys->i_tk = 0;
    p_sys->tk = NULL;
    p_sys->i_pcr_tk = -1;
    p_sys->i_pcr = -1;
    TAB_INIT( p_sys->i_attachments, p_sys->attachments);
    p_sys->p_title = NULL;

    /* Create I/O wrapper */
    p_sys->io_buffer_size = 32768;  /* FIXME */
    p_sys->io_buffer = malloc( p_sys->io_buffer_size );
    p_sys->url.priv_data = p_demux;
    p_sys->url.prot = &p_sys->prot;
    p_sys->url.prot->name = "VLC I/O wrapper";
    p_sys->url.prot->url_open = 0;
    p_sys->url.prot->url_read =
                    (int (*) (URLContext *, unsigned char *, int))IORead;
    p_sys->url.prot->url_write = 0;
    p_sys->url.prot->url_seek =
                    (int64_t (*) (URLContext *, int64_t, int))IOSeek;
    p_sys->url.prot->url_close = 0;
    p_sys->url.prot->next = 0;
    init_put_byte( &p_sys->io, p_sys->io_buffer, p_sys->io_buffer_size,
                   0, &p_sys->url, IORead, NULL, IOSeek );

    /* Open it */
    if( av_open_input_stream( &p_sys->ic, &p_sys->io, p_demux->psz_path,
                              p_sys->fmt, NULL ) )
    {
        msg_Err( p_demux, "av_open_input_stream failed" );
        CloseDemux( p_this );
        return VLC_EGENERIC;
    }

    vlc_avcodec_lock(); /* avformat calls avcodec behind our back!!! */
    if( av_find_stream_info( p_sys->ic ) < 0 )
    {
        vlc_avcodec_unlock();
        msg_Err( p_demux, "av_find_stream_info failed" );
        CloseDemux( p_this );
        return VLC_EGENERIC;
    }
    vlc_avcodec_unlock();

    for( i = 0; i < p_sys->ic->nb_streams; i++ )
    {
        AVCodecContext *cc = p_sys->ic->streams[i]->codec;
        es_out_id_t  *es;
        es_format_t  fmt;
        vlc_fourcc_t fcc;
        const char *psz_type = "unknown";

        if( !GetVlcFourcc( cc->codec_id, NULL, &fcc, NULL ) )
            fcc = VLC_FOURCC( 'u', 'n', 'd', 'f' );

        switch( cc->codec_type )
        {
        case CODEC_TYPE_AUDIO:
            es_format_Init( &fmt, AUDIO_ES, fcc );
            fmt.audio.i_channels = cc->channels;
            fmt.audio.i_rate = cc->sample_rate;
#if LIBAVCODEC_VERSION_INT < ((52<<16)+(0<<8)+0)
            fmt.audio.i_bitspersample = cc->bits_per_sample;
#else
            fmt.audio.i_bitspersample = cc->bits_per_coded_sample;
#endif
            fmt.audio.i_blockalign = cc->block_align;
            psz_type = "audio";
            break;

        case CODEC_TYPE_VIDEO:
            es_format_Init( &fmt, VIDEO_ES, fcc );

            /* Special case for raw video data */
            if( cc->codec_id == CODEC_ID_RAWVIDEO )
            {
                msg_Dbg( p_demux, "raw video, pixel format: %i", cc->pix_fmt );
                if( GetVlcChroma( &fmt.video, cc->pix_fmt ) != VLC_SUCCESS)
                {
                    msg_Err( p_demux, "was unable to find a FourCC match for raw video" );
                }
                else
                    fmt.i_codec = fmt.video.i_chroma;
            }

            fmt.video.i_width = cc->width;
            fmt.video.i_height = cc->height;
            if( cc->palctrl )
            {
                fmt.video.p_palette = malloc( sizeof(video_palette_t) );
                *fmt.video.p_palette = *(video_palette_t *)cc->palctrl;
            }
            psz_type = "video";
            break;

        case CODEC_TYPE_SUBTITLE:
            es_format_Init( &fmt, SPU_ES, fcc );
            psz_type = "subtitle";
            break;

        default:
            es_format_Init( &fmt, UNKNOWN_ES, 0 );
#ifdef HAVE_FFMPEG_CODEC_ATTACHMENT
            if( cc->codec_type == CODEC_TYPE_ATTACHMENT )
            {
                input_attachment_t *p_attachment;
                psz_type = "attachment";
                if( cc->codec_id == CODEC_ID_TTF )
                {
                    p_attachment = vlc_input_attachment_New( p_sys->ic->streams[i]->filename, "application/x-truetype-font", NULL,
                                             cc->extradata, (int)cc->extradata_size );
                    TAB_APPEND( p_sys->i_attachments, p_sys->attachments, p_attachment );
                }
                else msg_Warn( p_demux, "unsupported attachment type in ffmpeg demux" );
            }
            break;
#endif

            if( cc->codec_type == CODEC_TYPE_DATA )
                psz_type = "data";

            msg_Warn( p_demux, "unsupported track type in ffmpeg demux" );
            break;
        }
        fmt.psz_language = p_sys->ic->streams[i]->language;

#ifdef HAVE_FFMPEG_CODEC_ATTACHMENT
        if( cc->codec_type != CODEC_TYPE_ATTACHMENT )
#endif
        {
            fmt.i_extra = cc->extradata_size;
            fmt.p_extra = cc->extradata;
        }
        es = es_out_Add( p_demux->out, &fmt );

        msg_Dbg( p_demux, "adding es: %s codec = %4.4s",
                 psz_type, (char*)&fcc );
        TAB_APPEND( p_sys->i_tk, p_sys->tk, es );
    }
    if( p_sys->ic->start_time != (int64_t)AV_NOPTS_VALUE )
        i_start_time = p_sys->ic->start_time * 1000000 / AV_TIME_BASE;

    msg_Dbg( p_demux, "AVFormat supported stream" );
    msg_Dbg( p_demux, "    - format = %s (%s)",
             p_sys->fmt->name, p_sys->fmt->long_name );
    msg_Dbg( p_demux, "    - start time = %"PRId64, i_start_time );
    msg_Dbg( p_demux, "    - duration = %"PRId64,
             ( p_sys->ic->duration != (int64_t)AV_NOPTS_VALUE ) ?
             p_sys->ic->duration * 1000000 / AV_TIME_BASE : -1 );

#ifdef HAVE_FFMPEG_CHAPTERS
    if( p_sys->ic->nb_chapters > 0 )
        p_sys->p_title = vlc_input_title_New();
    for( i = 0; i < p_sys->ic->nb_chapters; i++ )
    {
        seekpoint_t *s = vlc_seekpoint_New();

        if( p_sys->ic->chapters[i]->title )
        {
            s->psz_name = strdup( p_sys->ic->chapters[i]->title );
            EnsureUTF8( s->psz_name );
            msg_Dbg( p_demux, "    - chapter %d: %s", i, s->psz_name );
        }
        s->i_time_offset = p_sys->ic->chapters[i]->start * 1000000 *
            p_sys->ic->chapters[i]->time_base.num /
            p_sys->ic->chapters[i]->time_base.den -
            (i_start_time != -1 ? i_start_time : 0 );
        TAB_APPEND( p_sys->p_title->i_seekpoint, p_sys->p_title->seekpoint, s );
    }
#endif

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close
 *****************************************************************************/
void CloseDemux( vlc_object_t *p_this )
{
    demux_t     *p_demux = (demux_t*)p_this;
    demux_sys_t *p_sys = p_demux->p_sys;

    FREENULL( p_sys->tk );

    if( p_sys->ic ) av_close_input_stream( p_sys->ic );

    for( int i = 0; i < p_sys->i_attachments; i++ )
        free( p_sys->attachments[i] );
    TAB_CLEAN( p_sys->i_attachments, p_sys->attachments);

    if( p_sys->p_title )
        vlc_input_title_Delete( p_sys->p_title );

    free( p_sys->io_buffer );
    free( p_sys );
}

/*****************************************************************************
 * Demux:
 *****************************************************************************/
static int Demux( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    AVPacket    pkt;
    block_t     *p_frame;
    int64_t     i_start_time;

    /* Read a frame */
    if( av_read_frame( p_sys->ic, &pkt ) )
    {
        return 0;
    }
    if( pkt.stream_index < 0 || pkt.stream_index >= p_sys->i_tk )
    {
        av_free_packet( &pkt );
        return 1;
    }
    const AVStream *p_stream = p_sys->ic->streams[pkt.stream_index];
    if( p_stream->time_base.den <= 0 )
    {
        msg_Warn( p_demux, "Invalid time base for the stream %d", pkt.stream_index );
        av_free_packet( &pkt );
        return 1;
    }

    if( ( p_frame = block_New( p_demux, pkt.size ) ) == NULL )
    {
        av_free_packet( &pkt );
        return 0;
    }

    memcpy( p_frame->p_buffer, pkt.data, pkt.size );

    if( pkt.flags & PKT_FLAG_KEY )
        p_frame->i_flags |= BLOCK_FLAG_TYPE_I;

    i_start_time = ( p_sys->ic->start_time != (int64_t)AV_NOPTS_VALUE ) ?
        ( p_sys->ic->start_time * 1000000 / AV_TIME_BASE )  : 0;

    p_frame->i_dts = ( pkt.dts == (int64_t)AV_NOPTS_VALUE ) ?
        0 : (pkt.dts) * 1000000 *
        p_stream->time_base.num /
        p_stream->time_base.den - i_start_time;
    p_frame->i_pts = ( pkt.pts == (int64_t)AV_NOPTS_VALUE ) ?
        0 : (pkt.pts) * 1000000 *
        p_stream->time_base.num /
        p_stream->time_base.den - i_start_time;
    if( pkt.duration > 0 )
        p_frame->i_length = pkt.duration * 1000000 *
            p_stream->time_base.num /
            p_stream->time_base.den - i_start_time;

    if( pkt.dts != AV_NOPTS_VALUE && pkt.dts == pkt.pts &&
        p_stream->codec->codec_type == CODEC_TYPE_VIDEO )
    {
        /* Add here notoriously bugged file formats/samples regarding PTS */
        if( !strcmp( p_sys->fmt->name, "flv" ) )
            p_frame->i_pts = 0;
    }
#ifdef AVFORMAT_DEBUG
    msg_Dbg( p_demux, "tk[%d] dts=%"PRId64" pts=%"PRId64,
             pkt.stream_index, p_frame->i_dts, p_frame->i_pts );
#endif

    if( pkt.dts > 0  &&
        ( pkt.stream_index == p_sys->i_pcr_tk || p_sys->i_pcr_tk < 0 ) )
    {
        p_sys->i_pcr_tk = pkt.stream_index;
        p_sys->i_pcr = p_frame->i_dts;

        es_out_Control( p_demux->out, ES_OUT_SET_PCR, (int64_t)p_sys->i_pcr );
    }

    es_out_Send( p_demux->out, p_sys->tk[pkt.stream_index], p_frame );

    UpdateSeekPoint( p_demux, p_sys->i_pcr);
    av_free_packet( &pkt );
    return 1;
}

static void UpdateSeekPoint( demux_t *p_demux, int64_t i_time )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    int i;

    if( !p_sys->p_title )
        return;

    for( i = 0; i < p_sys->p_title->i_seekpoint; i++ )
    {
        if( i_time < p_sys->p_title->seekpoint[i]->i_time_offset )
            break;
    }
    i--;

    if( i != p_demux->info.i_seekpoint && i >= 0 )
    {
        p_demux->info.i_seekpoint = i;
        p_demux->info.i_update |= INPUT_UPDATE_SEEKPOINT;
    }
}

/*****************************************************************************
 * Control:
 *****************************************************************************/
static int Control( demux_t *p_demux, int i_query, va_list args )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    double f, *pf;
    int64_t i64, *pi64;

    switch( i_query )
    {
        case DEMUX_GET_POSITION:
            pf = (double*) va_arg( args, double* ); *pf = 0.0;
            i64 = stream_Size( p_demux->s );
            if( i64 > 0 )
            {
                *pf = (double)stream_Tell( p_demux->s ) / (double)i64;
            }

            if( (p_sys->ic->duration != (int64_t)AV_NOPTS_VALUE) && (p_sys->i_pcr > 0) )
            {
                *pf = (double)p_sys->i_pcr / (double)p_sys->ic->duration;
            }

            return VLC_SUCCESS;

        case DEMUX_SET_POSITION:
            f = (double) va_arg( args, double );
            i64 = stream_Tell( p_demux->s );
            if( p_sys->i_pcr > 0 )
            {
                i64 = p_sys->ic->duration * f;
                if( p_sys->ic->start_time != (int64_t)AV_NOPTS_VALUE )
                    i64 += p_sys->ic->start_time;

                msg_Warn( p_demux, "DEMUX_SET_POSITION: %"PRId64, i64 );

                /* If we have a duration, we prefer to seek by time
                   but if we don't, or if the seek fails, try BYTE seeking */
                if( p_sys->ic->duration == (int64_t)AV_NOPTS_VALUE ||
                    (av_seek_frame( p_sys->ic, -1, i64, 0 ) < 0) )
                {
                    int64_t i_size = stream_Size( p_demux->s );

                    msg_Warn( p_demux, "DEMUX_SET_BYTE_POSITION: %"PRId64, i64 );
                    if( av_seek_frame( p_sys->ic, -1, (i_size * f), AVSEEK_FLAG_BYTE ) < 0 )
                        return VLC_EGENERIC;
                }
                UpdateSeekPoint( p_demux, i64 );
                p_sys->i_pcr = -1; /* Invalidate time display */
            }
            return VLC_SUCCESS;

        case DEMUX_GET_LENGTH:
            pi64 = (int64_t*)va_arg( args, int64_t * );
            if( p_sys->ic->duration != (int64_t)AV_NOPTS_VALUE )
            {
                *pi64 = p_sys->ic->duration;
            }
            else *pi64 = 0;
            return VLC_SUCCESS;

        case DEMUX_GET_TIME:
            pi64 = (int64_t*)va_arg( args, int64_t * );
            *pi64 = p_sys->i_pcr;
            return VLC_SUCCESS;

        case DEMUX_SET_TIME:
            i64 = (int64_t)va_arg( args, int64_t );
            i64 = i64 *AV_TIME_BASE / 1000000;
            if( p_sys->ic->start_time != (int64_t)AV_NOPTS_VALUE )
                i64 += p_sys->ic->start_time;

            msg_Warn( p_demux, "DEMUX_SET_TIME: %"PRId64, i64 );

            if( av_seek_frame( p_sys->ic, -1, i64, 0 ) < 0 )
            {
                return VLC_EGENERIC;
            }
            p_sys->i_pcr = -1; /* Invalidate time display */
            UpdateSeekPoint( p_demux, i64 );
            return VLC_SUCCESS;

        case DEMUX_HAS_UNSUPPORTED_META:
        {
            bool *pb_bool = (bool*)va_arg( args, bool* );
            *pb_bool = true;
            return VLC_SUCCESS;
        }


        case DEMUX_GET_META:
        {
            vlc_meta_t *p_meta = (vlc_meta_t*)va_arg( args, vlc_meta_t* );

            if( !p_sys->ic->title[0] || !p_sys->ic->author[0] ||
                !p_sys->ic->copyright[0] || !p_sys->ic->comment[0] ||
                /*!p_sys->ic->album[0] ||*/ !p_sys->ic->genre[0] )
            {
                return VLC_EGENERIC;
            }

            if( p_sys->ic->title[0] )
                vlc_meta_SetTitle( p_meta, p_sys->ic->title );
            if( p_sys->ic->author[0] )
                vlc_meta_SetArtist( p_meta, p_sys->ic->author );
            if( p_sys->ic->copyright[0] )
                vlc_meta_SetCopyright( p_meta, p_sys->ic->copyright );
            if( p_sys->ic->comment[0] )
                vlc_meta_SetDescription( p_meta, p_sys->ic->comment );
            if( p_sys->ic->genre[0] )
                vlc_meta_SetGenre( p_meta, p_sys->ic->genre );
            return VLC_SUCCESS;
        }

        case DEMUX_GET_ATTACHMENTS:
        {
            input_attachment_t ***ppp_attach =
                (input_attachment_t***)va_arg( args, input_attachment_t*** );
            int *pi_int = (int*)va_arg( args, int * );
            int i;

            if( p_sys->i_attachments <= 0 )
                return VLC_EGENERIC;

            *pi_int = p_sys->i_attachments;;
            *ppp_attach = malloc( sizeof(input_attachment_t**) * p_sys->i_attachments );
            for( i = 0; i < p_sys->i_attachments; i++ )
                (*ppp_attach)[i] = vlc_input_attachment_Duplicate( p_sys->attachments[i] );
            return VLC_SUCCESS;
        }

        case DEMUX_GET_TITLE_INFO:
        {
            input_title_t ***ppp_title = (input_title_t***)va_arg( args, input_title_t*** );
            int *pi_int    = (int*)va_arg( args, int* );
            int *pi_title_offset = (int*)va_arg( args, int* );
            int *pi_seekpoint_offset = (int*)va_arg( args, int* );

            if( !p_sys->p_title )
                return VLC_EGENERIC;

            *pi_int = 1;
            *ppp_title = malloc( sizeof( input_title_t**) );
            (*ppp_title)[0] = vlc_input_title_Duplicate( p_sys->p_title );
            *pi_title_offset = 0;
            *pi_seekpoint_offset = 0;
            return VLC_SUCCESS;
        }
        case DEMUX_SET_TITLE:
        {
            const int i_title = (int)va_arg( args, int );
            if( !p_sys->p_title || i_title != 0 )
                return VLC_EGENERIC;
            return VLC_SUCCESS;
        }
        case DEMUX_SET_SEEKPOINT:
        {
            const int i_seekpoint = (int)va_arg( args, int );
            if( !p_sys->p_title )
                return VLC_EGENERIC;

            i64 = p_sys->p_title->seekpoint[i_seekpoint]->i_time_offset *AV_TIME_BASE / 1000000;
            if( p_sys->ic->start_time != (int64_t)AV_NOPTS_VALUE )
                i64 += p_sys->ic->start_time;

            msg_Warn( p_demux, "DEMUX_SET_TIME: %"PRId64, i64 );

            if( av_seek_frame( p_sys->ic, -1, i64, 0 ) < 0 )
            {
                return VLC_EGENERIC;
            }
            p_sys->i_pcr = -1; /* Invalidate time display */
            UpdateSeekPoint( p_demux, i64 );
            return VLC_SUCCESS;
        }


        default:
            return VLC_EGENERIC;
    }
}

/*****************************************************************************
 * I/O wrappers for libavformat
 *****************************************************************************/
static int IORead( void *opaque, uint8_t *buf, int buf_size )
{
    URLContext *p_url = opaque;
    demux_t *p_demux = p_url->priv_data;
    if( buf_size < 0 ) return -1;
    int i_ret = stream_Read( p_demux->s, buf, buf_size );
    return i_ret ? i_ret : -1;
}

static int64_t IOSeek( void *opaque, int64_t offset, int whence )
{
    URLContext *p_url = opaque;
    demux_t *p_demux = p_url->priv_data;
    int64_t i_absolute = (int64_t)offset;
    int64_t i_size = stream_Size( p_demux->s );

#ifdef AVFORMAT_DEBUG
    msg_Warn( p_demux, "IOSeek offset: %"PRId64", whence: %i", offset, whence );
#endif

    switch( whence )
    {
#ifdef AVSEEK_SIZE
        case AVSEEK_SIZE:
            return i_size;
#endif
        case SEEK_SET:
            i_absolute = (int64_t)offset;
            break;
        case SEEK_CUR:
            i_absolute = stream_Tell( p_demux->s ) + (int64_t)offset;
            break;
        case SEEK_END:
            i_absolute = i_size + (int64_t)offset;
            break;
        default:
            return -1;

    }

    if( i_absolute < 0 )
    {
        msg_Dbg( p_demux, "Trying to seek before the beginning" );
        return -1;
    }

    if( i_size > 0 && i_absolute >= i_size )
    {
        msg_Dbg( p_demux, "Trying to seek too far : EOF?" );
        return -1;
    }

    if( stream_Seek( p_demux->s, i_absolute ) )
    {
        msg_Warn( p_demux, "we were not allowed to seek, or EOF " );
        return -1;
    }

    return stream_Tell( p_demux->s );
}

#endif /* HAVE_LIBAVFORMAT_AVFORMAT_H */
