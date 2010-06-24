/*****************************************************************************
 * alsa.c : Alsa input module for vlc
 *****************************************************************************
 * Copyright (C) 2002-2009 the VideoLAN team
 * $Id: 08a661eb7c54c294f7062b9abb0f62a6ec8da716 $
 *
 * Authors: Benjamin Pracht <bigben at videolan dot org>
 *          Richard Hosking <richard at hovis dot net>
 *          Antoine Cellerier <dionoea at videolan d.t org>
 *          Dennis Lou <dlou99 at yahoo dot com>
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

/*
 * ALSA support based on parts of
 * http://www.equalarea.com/paul/alsa-audio.html
 * and hints taken from alsa-utils (aplay/arecord)
 * http://www.alsa-project.org
 */

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_access.h>
#include <vlc_demux.h>
#include <vlc_input.h>

#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include <sys/soundcard.h>

#define ALSA_PCM_NEW_HW_PARAMS_API
#define ALSA_PCM_NEW_SW_PARAMS_API
#include <alsa/asoundlib.h>

#include <poll.h>

/*****************************************************************************
 * Module descriptior
 *****************************************************************************/

static int  DemuxOpen ( vlc_object_t * );
static void DemuxClose( vlc_object_t * );

#define STEREO_TEXT N_( "Stereo" )
#define STEREO_LONGTEXT N_( \
    "Capture the audio stream in stereo." )

#define SAMPLERATE_TEXT N_( "Samplerate" )
#define SAMPLERATE_LONGTEXT N_( \
    "Samplerate of the captured audio stream, in Hz (eg: 11025, 22050, 44100, 48000)" )

#define CACHING_TEXT N_("Caching value in ms")
#define CACHING_LONGTEXT N_( \
    "Caching value for Alsa captures. This " \
    "value should be set in milliseconds." )

#define HELP_TEXT N_( \
    "Use alsa:// to open the default audio input. If multiple audio " \
    "inputs are available, they will be listed in the vlc debug output. " \
    "To select hw:0,1 , use alsa://hw:0,1 ." )

#define ALSA_DEFAULT "hw"
#define CFG_PREFIX "alsa-"

vlc_module_begin()
    set_shortname( N_("Alsa") )
    set_description( N_("Alsa audio capture input") )
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_ACCESS )
    set_help( HELP_TEXT )

    add_shortcut( "alsa" )
    set_capability( "access_demux", 10 )
    set_callbacks( DemuxOpen, DemuxClose )

    add_bool( CFG_PREFIX "stereo", true, NULL, STEREO_TEXT, STEREO_LONGTEXT,
                true )
    add_integer( CFG_PREFIX "samplerate", 48000, NULL, SAMPLERATE_TEXT,
                SAMPLERATE_LONGTEXT, true )
    add_integer( CFG_PREFIX "caching", DEFAULT_PTS_DELAY / 1000, NULL,
                CACHING_TEXT, CACHING_LONGTEXT, true )
vlc_module_end()

/*****************************************************************************
 * Access: local prototypes
 *****************************************************************************/

static int DemuxControl( demux_t *, int, va_list );

static int Demux( demux_t * );

static block_t* GrabAudio( demux_t *p_demux );

static int OpenAudioDev( demux_t *, const char * );
static bool ProbeAudioDevAlsa( demux_t *, const char * );
static char *ListAvailableDevices( demux_t *, bool b_probe );

struct demux_sys_t
{
    /* Audio */
    int i_cache;
    unsigned int i_sample_rate;
    bool b_stereo;
    size_t i_max_frame_size;
    block_t *p_block;
    es_out_id_t *p_es;

    /* ALSA Audio */
    snd_pcm_t *p_alsa_pcm;
    size_t i_alsa_frame_size;
    int i_alsa_chunk_size;

    int64_t i_next_demux_date; /* Used to handle alsa:// as input-slave properly */
};

static int FindMainDevice( demux_t *p_demux, const char *psz_device )
{
    if( psz_device )
    {
        msg_Dbg( p_demux, "opening device '%s'", psz_device );
        if( ProbeAudioDevAlsa( p_demux, psz_device ) )
        {
            msg_Dbg( p_demux, "'%s' is an audio device", psz_device );
            OpenAudioDev( p_demux, psz_device );
        }
    }
    else if( ProbeAudioDevAlsa( p_demux, ALSA_DEFAULT ) )
    {
        msg_Dbg( p_demux, "'%s' is an audio device", ALSA_DEFAULT );
        OpenAudioDev( p_demux, ALSA_DEFAULT );
    }
    else if( ( psz_device = ListAvailableDevices( p_demux, true ) ) )
    {
        msg_Dbg( p_demux, "'%s' is an audio device", psz_device );
        OpenAudioDev( p_demux, psz_device );
        free( (char *)psz_device );
    }

    if( p_demux->p_sys->p_alsa_pcm == NULL )
        return VLC_EGENERIC;
    return VLC_SUCCESS;
}

static char *ListAvailableDevices( demux_t *p_demux, bool b_probe )
{
    snd_ctl_card_info_t *p_info = NULL;
    snd_ctl_card_info_alloca( &p_info );

    snd_pcm_info_t *p_pcminfo = NULL;
    snd_pcm_info_alloca( &p_pcminfo );

    if( !b_probe )
        msg_Dbg( p_demux, "Available alsa capture devices:" );
    int i_card = -1;
    while( !snd_card_next( &i_card ) && i_card >= 0 )
    {
        char psz_devname[10];
        snprintf( psz_devname, 10, "hw:%d", i_card );

        snd_ctl_t *p_ctl = NULL;
        if( snd_ctl_open( &p_ctl, psz_devname, 0 ) < 0 ) continue;

        snd_ctl_card_info( p_ctl, p_info );
        if( !b_probe )
            msg_Dbg( p_demux, "  %s (%s)",
                     snd_ctl_card_info_get_id( p_info ),
                     snd_ctl_card_info_get_name( p_info ) );

        int i_dev = -1;
        while( !snd_ctl_pcm_next_device( p_ctl, &i_dev ) && i_dev >= 0 )
        {
            snd_pcm_info_set_device( p_pcminfo, i_dev );
            snd_pcm_info_set_subdevice( p_pcminfo, 0 );
            snd_pcm_info_set_stream( p_pcminfo, SND_PCM_STREAM_CAPTURE );
            if( snd_ctl_pcm_info( p_ctl, p_pcminfo ) < 0 ) continue;

            if( !b_probe )
                msg_Dbg( p_demux, "    hw:%d,%d : %s (%s)", i_card, i_dev,
                         snd_pcm_info_get_id( p_pcminfo ),
                         snd_pcm_info_get_name( p_pcminfo ) );
            else
            {
                char *psz_device;
                if( asprintf( &psz_device, "hw:%d,%d", i_card, i_dev ) > 0 )
                {
                    if( ProbeAudioDevAlsa( p_demux, psz_device ) )
                    {
                        snd_ctl_close( p_ctl );
                        return psz_device;
                    }
                    else
                        free( psz_device );
                }
            }
        }

        snd_ctl_close( p_ctl );
    }
    return NULL;
}

/*****************************************************************************
 * DemuxOpen: opens alsa device, access_demux callback
 *****************************************************************************
 *
 * url: <alsa device>::::
 *
 *****************************************************************************/
static int DemuxOpen( vlc_object_t *p_this )
{
    demux_t     *p_demux = (demux_t*)p_this;
    demux_sys_t *p_sys;

    /* Only when selected */
    if( *p_demux->psz_access == '\0' ) return VLC_EGENERIC;

    /* Set up p_demux */
    p_demux->pf_control = DemuxControl;
    p_demux->pf_demux = Demux;
    p_demux->info.i_update = 0;
    p_demux->info.i_title = 0;
    p_demux->info.i_seekpoint = 0;

    p_demux->p_sys = p_sys = calloc( 1, sizeof( demux_sys_t ) );
    if( p_sys == NULL ) return VLC_ENOMEM;

    p_sys->i_sample_rate = var_CreateGetInteger( p_demux, CFG_PREFIX "samplerate" );
    p_sys->b_stereo = var_CreateGetBool( p_demux, CFG_PREFIX "stereo" );
    p_sys->i_cache = var_CreateGetInteger( p_demux, CFG_PREFIX "caching" );
    p_sys->p_es = NULL;
    p_sys->p_block = NULL;
    p_sys->i_next_demux_date = -1;

    const char *psz_device = NULL;
    if( p_demux->psz_path && *p_demux->psz_path )
        psz_device = p_demux->psz_path;
    else
        ListAvailableDevices( p_demux, false );

    if( FindMainDevice( p_demux, psz_device ) != VLC_SUCCESS )
    {
        if( p_demux->psz_path && *p_demux->psz_path )
            ListAvailableDevices( p_demux, false );
        DemuxClose( p_this );
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: close device, free resources
 *****************************************************************************/
static void DemuxClose( vlc_object_t *p_this )
{
    demux_t     *p_demux = (demux_t *)p_this;
    demux_sys_t *p_sys   = p_demux->p_sys;

    if( p_sys->p_alsa_pcm )
    {
        snd_pcm_close( p_sys->p_alsa_pcm );
    }

    if( p_sys->p_block ) block_Release( p_sys->p_block );

    free( p_sys );
}

/*****************************************************************************
 * DemuxControl:
 *****************************************************************************/
static int DemuxControl( demux_t *p_demux, int i_query, va_list args )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    switch( i_query )
    {
        /* Special for access_demux */
        case DEMUX_CAN_PAUSE:
        case DEMUX_CAN_SEEK:
        case DEMUX_SET_PAUSE_STATE:
        case DEMUX_CAN_CONTROL_PACE:
            *va_arg( args, bool * ) = false;
            return VLC_SUCCESS;

        case DEMUX_GET_PTS_DELAY:
            *va_arg( args, int64_t * ) = (int64_t)p_sys->i_cache * 1000;
            return VLC_SUCCESS;

        case DEMUX_GET_TIME:
            *va_arg( args, int64_t * ) = mdate();
            return VLC_SUCCESS;

        case DEMUX_SET_NEXT_DEMUX_TIME:
            p_sys->i_next_demux_date = va_arg( args, int64_t );
            return VLC_SUCCESS;

        /* TODO implement others */
        default:
            return VLC_EGENERIC;
    }

    return VLC_EGENERIC;
}

/*****************************************************************************
 * Demux: Processes the audio frame
 *****************************************************************************/
static int Demux( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    block_t *p_block = NULL;

    do
    {
        if( p_block )
        {
            es_out_Send( p_demux->out, p_sys->p_es, p_block );
            p_block = NULL;
        }

        /* Wait for data */
        int i_wait = snd_pcm_wait( p_sys->p_alsa_pcm, 10 ); /* See poll() comment in oss.c */
        switch( i_wait )
        {
            case 1:
            {
                p_block = GrabAudio( p_demux );
                if( p_block )
                    es_out_Control( p_demux->out, ES_OUT_SET_PCR, p_block->i_pts );
            }

            /* FIXME: this is a copy paste from below. Shouldn't be needed
             * twice. */
            case -EPIPE:
                /* xrun */
                snd_pcm_prepare( p_sys->p_alsa_pcm );
                break;
            case -ESTRPIPE:
            {
                /* suspend */
                int i_resume = snd_pcm_resume( p_sys->p_alsa_pcm );
                if( i_resume < 0 && i_resume != -EAGAIN ) snd_pcm_prepare( p_sys->p_alsa_pcm );
                break;
            }
            /* </FIXME> */
        }
    } while( p_block && p_sys->i_next_demux_date > 0 &&
             p_block->i_pts < p_sys->i_next_demux_date );

    if( p_block )
        es_out_Send( p_demux->out, p_sys->p_es, p_block );

    return 1;
}


/*****************************************************************************
 * GrabAudio: Grab an audio frame
 *****************************************************************************/
static block_t* GrabAudio( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    int i_read, i_correct;
    block_t *p_block;

    if( p_sys->p_block ) p_block = p_sys->p_block;
    else p_block = block_New( p_demux, p_sys->i_max_frame_size );

    if( !p_block )
    {
        msg_Warn( p_demux, "cannot get block" );
        return 0;
    }

    p_sys->p_block = p_block;

    /* ALSA */
    i_read = snd_pcm_readi( p_sys->p_alsa_pcm, p_block->p_buffer, p_sys->i_alsa_chunk_size );
    if( i_read <= 0 )
    {
        int i_resume;
        switch( i_read )
        {
            case -EAGAIN:
                break;
            case -EPIPE:
                /* xrun */
                snd_pcm_prepare( p_sys->p_alsa_pcm );
                break;
            case -ESTRPIPE:
                /* suspend */
                i_resume = snd_pcm_resume( p_sys->p_alsa_pcm );
                if( i_resume < 0 && i_resume != -EAGAIN ) snd_pcm_prepare( p_sys->p_alsa_pcm );
                break;
            default:
                msg_Err( p_demux, "Failed to read alsa frame (%s)", snd_strerror( i_read ) );
                return 0;
        }
    }
    else
    {
        /* convert from frames to bytes */
        i_read *= p_sys->i_alsa_frame_size;
    }

    if( i_read <= 0 ) return 0;

    p_block->i_buffer = i_read;
    p_sys->p_block = 0;

    /* Correct the date because of kernel buffering */
    i_correct = i_read;
    /* ALSA */
    int i_err;
    snd_pcm_sframes_t delay = 0;
    if( ( i_err = snd_pcm_delay( p_sys->p_alsa_pcm, &delay ) ) >= 0 )
    {
        size_t i_correction_delta = delay * p_sys->i_alsa_frame_size;
        /* Test for overrun */
        if( i_correction_delta > p_sys->i_max_frame_size )
        {
            msg_Warn( p_demux, "ALSA read overrun (%zu > %zu)",
                      i_correction_delta, p_sys->i_max_frame_size );
            i_correction_delta = p_sys->i_max_frame_size;
            snd_pcm_prepare( p_sys->p_alsa_pcm );
        }
        i_correct += i_correction_delta;
    }
    else
    {
        /* delay failed so reset */
        msg_Warn( p_demux, "ALSA snd_pcm_delay failed (%s)", snd_strerror( i_err ) );
        snd_pcm_prepare( p_sys->p_alsa_pcm );
    }

    /* Timestamp */
    p_block->i_pts = p_block->i_dts =
        mdate() - INT64_C(1000000) * (mtime_t)i_correct /
        2 / ( p_sys->b_stereo ? 2 : 1) / p_sys->i_sample_rate;

    return p_block;
}

/*****************************************************************************
 * OpenAudioDev: open and set up the audio device and probe for capabilities
 *****************************************************************************/
static int OpenAudioDevAlsa( demux_t *p_demux, const char *psz_device )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    p_sys->p_alsa_pcm = NULL;
    snd_pcm_hw_params_t *p_hw_params = NULL;
    snd_pcm_uframes_t buffer_size;
    snd_pcm_uframes_t chunk_size;

    /* ALSA */
    int i_err;

    if( ( i_err = snd_pcm_open( &p_sys->p_alsa_pcm, psz_device,
        SND_PCM_STREAM_CAPTURE, SND_PCM_NONBLOCK ) ) < 0)
    {
        msg_Err( p_demux, "Cannot open ALSA audio device %s (%s)",
                 psz_device, snd_strerror( i_err ) );
        goto adev_fail;
    }

    if( ( i_err = snd_pcm_nonblock( p_sys->p_alsa_pcm, 1 ) ) < 0)
    {
        msg_Err( p_demux, "Cannot set ALSA nonblock (%s)",
                 snd_strerror( i_err ) );
        goto adev_fail;
    }

    /* Begin setting hardware parameters */

    if( ( i_err = snd_pcm_hw_params_malloc( &p_hw_params ) ) < 0 )
    {
        msg_Err( p_demux,
                 "ALSA: cannot allocate hardware parameter structure (%s)",
                 snd_strerror( i_err ) );
        goto adev_fail;
    }

    if( ( i_err = snd_pcm_hw_params_any( p_sys->p_alsa_pcm, p_hw_params ) ) < 0 )
    {
        msg_Err( p_demux,
                "ALSA: cannot initialize hardware parameter structure (%s)",
                 snd_strerror( i_err ) );
        goto adev_fail;
    }

    /* Set Interleaved access */
    if( ( i_err = snd_pcm_hw_params_set_access( p_sys->p_alsa_pcm, p_hw_params, SND_PCM_ACCESS_RW_INTERLEAVED ) ) < 0 )
    {
        msg_Err( p_demux, "ALSA: cannot set access type (%s)",
                 snd_strerror( i_err ) );
        goto adev_fail;
    }

    /* Set 16 bit little endian */
    if( ( i_err = snd_pcm_hw_params_set_format( p_sys->p_alsa_pcm, p_hw_params, SND_PCM_FORMAT_S16_LE ) ) < 0 )
    {
        msg_Err( p_demux, "ALSA: cannot set sample format (%s)",
                 snd_strerror( i_err ) );
        goto adev_fail;
    }

    /* Set sample rate */
    i_err = snd_pcm_hw_params_set_rate_near( p_sys->p_alsa_pcm, p_hw_params, &p_sys->i_sample_rate, NULL );
    if( i_err < 0 )
    {
        msg_Err( p_demux, "ALSA: cannot set sample rate (%s)",
                 snd_strerror( i_err ) );
        goto adev_fail;
    }

    /* Set channels */
    unsigned int channels = p_sys->b_stereo ? 2 : 1;
    if( ( i_err = snd_pcm_hw_params_set_channels( p_sys->p_alsa_pcm, p_hw_params, channels ) ) < 0 )
    {
        channels = ( channels==1 ) ? 2 : 1;
        msg_Warn( p_demux, "ALSA: cannot set channel count (%s). "
                  "Trying with channels=%d",
                  snd_strerror( i_err ),
                  channels );
        if( ( i_err = snd_pcm_hw_params_set_channels( p_sys->p_alsa_pcm, p_hw_params, channels ) ) < 0 )
        {
            msg_Err( p_demux, "ALSA: cannot set channel count (%s)",
                     snd_strerror( i_err ) );
            goto adev_fail;
        }
        p_sys->b_stereo = ( channels == 2 );
    }

    /* Set metrics for buffer calculations later */
    unsigned int buffer_time;
    if( ( i_err = snd_pcm_hw_params_get_buffer_time_max(p_hw_params, &buffer_time, 0) ) < 0 )
    {
        msg_Err( p_demux, "ALSA: cannot get buffer time max (%s)",
                 snd_strerror( i_err ) );
        goto adev_fail;
    }
    if( buffer_time > 500000 ) buffer_time = 500000;

    /* Set period time */
    unsigned int period_time = buffer_time / 4;
    i_err = snd_pcm_hw_params_set_period_time_near( p_sys->p_alsa_pcm, p_hw_params, &period_time, 0 );
    if( i_err < 0 )
    {
        msg_Err( p_demux, "ALSA: cannot set period time (%s)",
                 snd_strerror( i_err ) );
        goto adev_fail;
    }

    /* Set buffer time */
    i_err = snd_pcm_hw_params_set_buffer_time_near( p_sys->p_alsa_pcm, p_hw_params, &buffer_time, 0 );
    if( i_err < 0 )
    {
        msg_Err( p_demux, "ALSA: cannot set buffer time (%s)",
                 snd_strerror( i_err ) );
        goto adev_fail;
    }

    /* Apply new hardware parameters */
    if( ( i_err = snd_pcm_hw_params( p_sys->p_alsa_pcm, p_hw_params ) ) < 0 )
    {
        msg_Err( p_demux, "ALSA: cannot set hw parameters (%s)",
                 snd_strerror( i_err ) );
        goto adev_fail;
    }

    /* Get various buffer metrics */
    snd_pcm_hw_params_get_period_size( p_hw_params, &chunk_size, 0 );
    snd_pcm_hw_params_get_buffer_size( p_hw_params, &buffer_size );
    if( chunk_size == buffer_size )
    {
        msg_Err( p_demux,
                 "ALSA: period cannot equal buffer size (%lu == %lu)",
                 chunk_size, buffer_size);
        goto adev_fail;
    }

    int bits_per_sample = snd_pcm_format_physical_width(SND_PCM_FORMAT_S16_LE);
    int bits_per_frame = bits_per_sample * channels;

    p_sys->i_alsa_chunk_size = chunk_size;
    p_sys->i_alsa_frame_size = bits_per_frame / 8;
    p_sys->i_max_frame_size = chunk_size * bits_per_frame / 8;

    snd_pcm_hw_params_free( p_hw_params );
    p_hw_params = NULL;

    /* Prep device */
    if( ( i_err = snd_pcm_prepare( p_sys->p_alsa_pcm ) ) < 0 )
    {
        msg_Err( p_demux,
                 "ALSA: cannot prepare audio interface for use (%s)",
                 snd_strerror( i_err ) );
        goto adev_fail;
    }

    snd_pcm_start( p_sys->p_alsa_pcm );

    return VLC_SUCCESS;

 adev_fail:

    if( p_hw_params ) snd_pcm_hw_params_free( p_hw_params );
    if( p_sys->p_alsa_pcm ) snd_pcm_close( p_sys->p_alsa_pcm );
    p_sys->p_alsa_pcm = NULL;

    return VLC_EGENERIC;

}

static int OpenAudioDev( demux_t *p_demux, const char *psz_device )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    if( OpenAudioDevAlsa( p_demux, psz_device ) != VLC_SUCCESS )
        return VLC_EGENERIC;

    msg_Dbg( p_demux, "opened adev=`%s' %s %dHz",
             psz_device, p_sys->b_stereo ? "stereo" : "mono",
             p_sys->i_sample_rate );

    es_format_t fmt;
    es_format_Init( &fmt, AUDIO_ES, VLC_FOURCC('a','r','a','w') );

    fmt.audio.i_channels = p_sys->b_stereo ? 2 : 1;
    fmt.audio.i_rate = p_sys->i_sample_rate;
    fmt.audio.i_bitspersample = 16;
    fmt.audio.i_blockalign = fmt.audio.i_channels * fmt.audio.i_bitspersample / 8;
    fmt.i_bitrate = fmt.audio.i_channels * fmt.audio.i_rate * fmt.audio.i_bitspersample;

    msg_Dbg( p_demux, "new audio es %d channels %dHz",
             fmt.audio.i_channels, fmt.audio.i_rate );

    p_sys->p_es = es_out_Add( p_demux->out, &fmt );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * ProbeAudioDevAlsa: probe audio for capabilities
 *****************************************************************************/
static bool ProbeAudioDevAlsa( demux_t *p_demux, const char *psz_device )
{
    int i_err;
    snd_pcm_t *p_alsa_pcm;

    if( ( i_err = snd_pcm_open( &p_alsa_pcm, psz_device, SND_PCM_STREAM_CAPTURE, SND_PCM_NONBLOCK ) ) < 0 )
    {
        msg_Err( p_demux, "cannot open device %s for ALSA audio (%s)", psz_device, snd_strerror( i_err ) );
        return false;
    }

    snd_pcm_close( p_alsa_pcm );

    return true;
}
