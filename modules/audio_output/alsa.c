/*****************************************************************************
 * alsa.c : alsa plugin for vlc
 *****************************************************************************
 * Copyright (C) 2000-2010 the VideoLAN team
 * Copyright (C) 2009-2011 Rémi Denis-Courmont
 *
 * Authors: Henri Fallon <henri@videolan.org> - Original Author
 *          Jeffrey Baker <jwbaker@acm.org> - Port to ALSA 1.0 API
 *          John Paul Lorenti <jpl31@columbia.edu> - Device selection
 *          Arnaud de Bossoreille de Ribou <bozo@via.ecp.fr> - S/PDIF and aout3
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

#include <assert.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_dialog.h>
#include <vlc_aout.h>
#include <vlc_cpu.h>

#include <alsa/asoundlib.h>
#include <alsa/version.h>

/** Private data for an ALSA PCM playback stream */
struct aout_sys_t
{
    snd_pcm_t *pcm;
};

#define A52_FRAME_NB 1536

static int Open (vlc_object_t *);
static void Close (vlc_object_t *);
static int FindDevicesCallback( vlc_object_t *p_this, char const *psz_name,
                                vlc_value_t newval, vlc_value_t oldval, void *p_unused );
static void GetDevices (vlc_object_t *, module_config_t *, const char *);

#define AUDIO_DEV_TEXT N_("Audio output device")
#define AUDIO_DEV_LONGTEXT N_("Audio output device (using ALSA syntax).")
static const char *const devices[] = {
    "default",
};
static const char *const devices_text[] = {
    N_("Default"),
};

#define AUDIO_CHAN_TEXT N_("Audio output channels")
#define AUDIO_CHAN_LONGTEXT N_("Channels available for audio output." \
    "If the input has more channels than the output, it will be down-mixed." \
    "This parameter is ignored when digital pass-through is active.")
static const int channels[] = {
    AOUT_CHAN_CENTER, AOUT_CHANS_STEREO, AOUT_CHANS_4_0, AOUT_CHANS_4_1,
    AOUT_CHANS_5_0, AOUT_CHANS_5_1,
};
static const char *const channels_text[] = {
    N_("Mono"), N_("Stereo"), N_("Surround 4.0"), N_("Surround 4.1"),
    N_("Surround 5.0"), N_("Surround 5.1"),
};

vlc_module_begin ()
    set_shortname( "ALSA" )
    set_description( N_("ALSA audio output") )
    set_category( CAT_AUDIO )
    set_subcategory( SUBCAT_AUDIO_AOUT )
    add_string ("alsa-audio-device", "default",
                AUDIO_DEV_TEXT, AUDIO_DEV_LONGTEXT, false)
        change_string_list( devices, devices_text, FindDevicesCallback )
        change_action_add( FindDevicesCallback, N_("Refresh list") )
    add_integer ("alsa-audio-channels", AOUT_CHANS_FRONT,
                 AUDIO_CHAN_TEXT, AUDIO_CHAN_LONGTEXT, false)
        change_integer_list (channels, channels_text)
    set_capability( "audio output", 150 )
    set_callbacks( Open, Close )
vlc_module_end ()


/** Helper for ALSA -> VLC debugging output */
static void Dump (vlc_object_t *obj, const char *msg,
                  int (*cb)(void *, snd_output_t *), void *p)
{
    snd_output_t *output;
    char *str;

    if (unlikely(snd_output_buffer_open (&output)))
        return;

    int val = cb (p, output);
    if (val)
    {
        msg_Warn (obj, "cannot get info: %s", snd_strerror (val));
        return;
    }

    size_t len = snd_output_buffer_string (output, &str);
    if (len > 0 && str[len - 1])
        len--; /* strip trailing newline */
    msg_Dbg (obj, "%s%.*s", msg, (int)len, str);
    snd_output_close (output);
}
#define Dump(o, m, cb, p) \
        Dump(VLC_OBJECT(o), m, (int (*)(void *, snd_output_t *))(cb), p)

static void DumpDevice (vlc_object_t *obj, snd_pcm_t *pcm)
{
    snd_pcm_info_t *info;

    Dump (obj, " ", snd_pcm_dump, pcm);
    snd_pcm_info_alloca (&info);
    if (snd_pcm_info (pcm, info) == 0)
    {
        msg_Dbg (obj, " device name   : %s", snd_pcm_info_get_name (info));
        msg_Dbg (obj, " device ID     : %s", snd_pcm_info_get_id (info));
        msg_Dbg (obj, " subdevice name: %s",
                snd_pcm_info_get_subdevice_name (info));
    }
}

static void DumpDeviceStatus (vlc_object_t *obj, snd_pcm_t *pcm)
{
    snd_pcm_status_t *status;

    snd_pcm_status_alloca (&status);
    snd_pcm_status (pcm, status);
    Dump (obj, "current status:\n", snd_pcm_status_dump, status);
}
#define DumpDeviceStatus(o, p) DumpDeviceStatus(VLC_OBJECT(o), p)

/**
 * Initializes list of devices.
 */
static void Probe (vlc_object_t *obj, const char *dev)
{
    vlc_value_t text;

    var_Create (obj, "audio-device", VLC_VAR_STRING | VLC_VAR_HASCHOICE);
    text.psz_string = _("Audio Device");
    var_Change (obj, "audio-device", VLC_VAR_SETTEXT, &text, NULL);

    GetDevices (obj, NULL, dev);

    var_AddCallback (obj, "audio-device", aout_ChannelsRestart, NULL);
    var_TriggerCallback (obj, "intf-change");
}


static void Play  (audio_output_t *, block_t *);
static void Pause (audio_output_t *, bool, mtime_t);
static void PauseDummy (audio_output_t *, bool, mtime_t);
static void Flush (audio_output_t *, bool);

/** Initializes an ALSA playback stream */
static int Open (vlc_object_t *obj)
{
    audio_output_t *aout = (audio_output_t *)obj;

    /* Get device name */
    char *device = var_InheritString (aout, "alsa-audio-device");
    if (unlikely(device == NULL))
        return VLC_ENOMEM;

    snd_pcm_format_t pcm_format; /* ALSA sample format */
    vlc_fourcc_t fourcc = aout->format.i_format;
    bool spdif = false;

    switch (fourcc)
    {
        case VLC_CODEC_F64B:
            pcm_format = SND_PCM_FORMAT_FLOAT64_BE;
            break;
        case VLC_CODEC_F64L:
            pcm_format = SND_PCM_FORMAT_FLOAT64_LE;
            break;
        case VLC_CODEC_F32B:
            pcm_format = SND_PCM_FORMAT_FLOAT_BE;
            break;
        case VLC_CODEC_F32L:
            pcm_format = SND_PCM_FORMAT_FLOAT_LE;
            break;
        case VLC_CODEC_FI32:
            fourcc = VLC_CODEC_FL32;
            pcm_format = SND_PCM_FORMAT_FLOAT;
            break;
        case VLC_CODEC_S32B:
            pcm_format = SND_PCM_FORMAT_S32_BE;
            break;
        case VLC_CODEC_S32L:
            pcm_format = SND_PCM_FORMAT_S32_LE;
            break;
        case VLC_CODEC_S24B:
            pcm_format = SND_PCM_FORMAT_S24_3BE;
            break;
        case VLC_CODEC_S24L:
            pcm_format = SND_PCM_FORMAT_S24_3LE;
            break;
        case VLC_CODEC_U24B:
            pcm_format = SND_PCM_FORMAT_U24_3BE;
            break;
        case VLC_CODEC_U24L:
            pcm_format = SND_PCM_FORMAT_U24_3LE;
            break;
        case VLC_CODEC_S16B:
            pcm_format = SND_PCM_FORMAT_S16_BE;
            break;
        case VLC_CODEC_S16L:
            pcm_format = SND_PCM_FORMAT_S16_LE;
            break;
        case VLC_CODEC_U16B:
            pcm_format = SND_PCM_FORMAT_U16_BE;
            break;
        case VLC_CODEC_U16L:
            pcm_format = SND_PCM_FORMAT_U16_LE;
            break;
        case VLC_CODEC_S8:
            pcm_format = SND_PCM_FORMAT_S8;
            break;
        case VLC_CODEC_U8:
            pcm_format = SND_PCM_FORMAT_U8;
            break;
        default:
            if (AOUT_FMT_SPDIF(&aout->format))
                spdif = var_InheritBool (aout, "spdif");
            if (spdif)
            {
                fourcc = VLC_CODEC_SPDIFL;
                pcm_format = SND_PCM_FORMAT_S16;
            }
            else
            if (HAVE_FPU)
            {
                fourcc = VLC_CODEC_FL32;
                pcm_format = SND_PCM_FORMAT_FLOAT;
            }
            else
            {
                fourcc = VLC_CODEC_S16N;
                pcm_format = SND_PCM_FORMAT_S16;
            }
    }

    /* ALSA channels */
    /* XXX: maybe this should be shared with other dumb outputs */
    uint32_t map = var_InheritInteger (aout, "alsa-audio-channels");
    map &= aout->format.i_physical_channels;
    if (unlikely(map == 0)) /* WTH? */
        map = AOUT_CHANS_STEREO;

    unsigned channels = popcount (map);
    if (channels < aout_FormatNbChannels (&aout->format))
        msg_Dbg (aout, "downmixing from %u to %u channels",
                 aout_FormatNbChannels (&aout->format), channels);
    else
        msg_Dbg (aout, "keeping %u channels", channels);

    /* Choose the IEC device for S/PDIF output:
       if the device is overridden by the user then it will be the one.
       Otherwise we compute the default device based on the output format. */
    if (spdif && !strcmp (device, "default"))
    {
        unsigned aes3;

        switch (aout->format.i_rate)
        {
#define FS(freq) \
            case freq: aes3 = IEC958_AES3_CON_FS_ ## freq; break;
            FS( 44100) /* def. */ FS( 48000) FS( 32000)
            FS( 22050)            FS( 24000)
            FS( 88200) FS(768000) FS( 96000)
            FS(176400)            FS(192000)
#undef FS
            default:
                aes3 = IEC958_AES3_CON_FS_NOTID;
                break;
        }

        free (device);
        if (asprintf (&device,
                      "iec958:AES0=0x%x,AES1=0x%x,AES2=0x%x,AES3=0x%x",
                      IEC958_AES0_CON_EMPHASIS_NONE | IEC958_AES0_NONAUDIO,
                      IEC958_AES1_CON_ORIGINAL | IEC958_AES1_CON_PCM_CODER,
                      0, aes3) == -1)
            return VLC_ENOMEM;
    }

    /* Allocate structures */
    aout_sys_t *sys = malloc (sizeof (*sys));
    if (unlikely(sys == NULL))
    {
        free (device);
        return VLC_ENOMEM;
    }
    aout->sys = sys;

    /* Open the device */
    snd_pcm_t *pcm;
    /* VLC always has a resampler. No need for ALSA's. */
    const int mode = SND_PCM_NO_AUTO_RESAMPLE;

    int val = snd_pcm_open (&pcm, device, SND_PCM_STREAM_PLAYBACK, mode);
#if (SND_LIB_VERSION <= 0x010015)
# warning Please update alsa-lib to version > 1.0.21a.
    var_Create (aout->p_libvlc, "alsa-working", VLC_VAR_BOOL);
    if (val != 0 && var_GetBool (aout->p_libvlc, "alsa-working"))
        dialog_Fatal (aout, "ALSA version problem",
            "VLC failed to re-initialize your audio output device.\n"
            "Please update alsa-lib to version 1.0.22 or higher "
            "to fix this issue.");
    var_SetBool (aout->p_libvlc, "alsa-working", !val);
#endif
    if (val != 0)
    {
#if (SND_LIB_VERSION <= 0x010017)
# warning Please update alsa-lib to version > 1.0.23.
        var_Create (aout->p_libvlc, "alsa-broken", VLC_VAR_BOOL);
        if (!var_GetBool (aout->p_libvlc, "alsa-broken"))
        {
            var_SetBool (aout->p_libvlc, "alsa-broken", true);
            dialog_Fatal (aout, "Potential ALSA version problem",
                "VLC failed to initialize your audio output device (if any).\n"
                "Please update alsa-lib to version 1.0.24 or higher "
                "to try to fix this issue.");
        }
#endif
        msg_Err (aout, "cannot open ALSA device \"%s\": %s", device,
                 snd_strerror (val));
        dialog_Fatal (aout, _("Audio output failed"),
                      _("The audio device \"%s\" could not be used:\n%s."),
                      device, snd_strerror (val));
        free (device);
        free (sys);
        return VLC_EGENERIC;
    }
    sys->pcm = pcm;

    /* Print some potentially useful debug */
    msg_Dbg (aout, "using ALSA device: %s", device);
    DumpDevice (VLC_OBJECT(aout), pcm);

    /* Get Initial hardware parameters */
    snd_pcm_hw_params_t *hw;
    unsigned param;

    snd_pcm_hw_params_alloca (&hw);
    snd_pcm_hw_params_any (pcm, hw);
    Dump (aout, "initial hardware setup:\n", snd_pcm_hw_params_dump, hw);

    val = snd_pcm_hw_params_set_rate_resample(pcm, hw, 0);
    if (val)
    {
        msg_Err (aout, "cannot disable resampling: %s", snd_strerror (val));
        goto error;
    }

    val = snd_pcm_hw_params_set_access (pcm, hw,
                                        SND_PCM_ACCESS_RW_INTERLEAVED);
    if (val)
    {
        msg_Err (aout, "cannot set access mode: %s", snd_strerror (val));
        goto error;
    }

    /* Set sample format */
    if (snd_pcm_hw_params_test_format (pcm, hw, pcm_format) == 0)
        ;
    else
    if (snd_pcm_hw_params_test_format (pcm, hw, SND_PCM_FORMAT_FLOAT) == 0)
    {
        fourcc = VLC_CODEC_FL32;
        pcm_format = SND_PCM_FORMAT_FLOAT;
    }
    else
    if (snd_pcm_hw_params_test_format (pcm, hw, SND_PCM_FORMAT_S32) == 0)
    {
        fourcc = VLC_CODEC_S32N;
        pcm_format = SND_PCM_FORMAT_S32;
    }
    else
    if (snd_pcm_hw_params_test_format (pcm, hw, SND_PCM_FORMAT_S16) == 0)
    {
        fourcc = VLC_CODEC_S16N;
        pcm_format = SND_PCM_FORMAT_S16;
    }
    else
    {
        msg_Err (aout, "no supported sample format");
        goto error;
    }

    val = snd_pcm_hw_params_set_format (pcm, hw, pcm_format);
    if (val)
    {
        msg_Err (aout, "cannot set sample format: %s", snd_strerror (val));
        goto error;
    }

    /* Set channels count */
    /* By default, ALSA plug will pad missing channels with zeroes, which is
     * usually fine. However, it will also discard extraneous channels, which
     * is not acceptable. Thus the user must configure the physically
     * available channels, and VLC will downmix if needed. */
    val = snd_pcm_hw_params_set_channels (pcm, hw, channels);
    if (val)
    {
        msg_Err (aout, "cannot set %u channels: %s", channels,
                 snd_strerror (val));
        goto error;
    }

    /* Set sample rate */
    unsigned rate = aout->format.i_rate;
    val = snd_pcm_hw_params_set_rate_near (pcm, hw, &rate, NULL);
    if (val)
    {
        msg_Err (aout, "cannot set sample rate: %s", snd_strerror (val));
        goto error;
    }
    if (aout->format.i_rate != rate)
        msg_Dbg (aout, "resampling from %d Hz to %d Hz",
                 aout->format.i_rate, rate);

    /* Set buffer size */
    param = AOUT_MAX_ADVANCE_TIME;
    val = snd_pcm_hw_params_set_buffer_time_near (pcm, hw, &param, NULL);
    if (val)
    {
        msg_Err (aout, "cannot set buffer duration: %s", snd_strerror (val));
        goto error;
    }
#if 0
    val = snd_pcm_hw_params_get_buffer_time (hw, &param, NULL);
    if (val)
    {
        msg_Warn (aout, "cannot get buffer time: %s", snd_strerror(val));
        param = AOUT_MIN_PREPARE_TIME;
    }
    else
        param /= 2;
#else /* work-around for period-long latency outputs (e.g. PulseAudio): */
    param = AOUT_MIN_PREPARE_TIME;
#endif
    val = snd_pcm_hw_params_set_period_time_near (pcm, hw, &param, NULL);
    if (val)
    {
        msg_Err (aout, "cannot set period: %s", snd_strerror (val));
        goto error;
    }

    /* Commit hardware parameters */
    val = snd_pcm_hw_params (pcm, hw);
    if (val < 0)
    {
        msg_Err (aout, "cannot commit hardware parameters: %s",
                 snd_strerror (val));
        goto error;
    }
    Dump (aout, "final HW setup:\n", snd_pcm_hw_params_dump, hw);

    /* Get Initial software parameters */
    snd_pcm_sw_params_t *sw;

    snd_pcm_sw_params_alloca (&sw);
    snd_pcm_sw_params_current (pcm, sw);
    Dump (aout, "initial software parameters:\n", snd_pcm_sw_params_dump, sw);

    /* START REVISIT */
    //snd_pcm_sw_params_set_avail_min( pcm, sw, i_period_size );
    // FIXME: useful?
    val = snd_pcm_sw_params_set_start_threshold (pcm, sw, 1);
    if( val < 0 )
    {
        msg_Err( aout, "unable to set start threshold (%s)",
                 snd_strerror( val ) );
        goto error;
    }
    /* END REVISIT */

    /* Commit software parameters. */
    val = snd_pcm_sw_params (pcm, sw);
    if (val)
    {
        msg_Err (aout, "cannot commit software parameters: %s",
                 snd_strerror (val));
        goto error;
    }
    Dump (aout, "final software parameters:\n", snd_pcm_sw_params_dump, sw);

    val = snd_pcm_prepare (pcm);
    if (val)
    {
        msg_Err (aout, "cannot prepare device: %s", snd_strerror (val));
        goto error;
    }

    /* Setup audio_output_t */
    aout->format.i_format = fourcc;
    aout->format.i_rate = rate;
    if (spdif)
    {
        aout->format.i_bytes_per_frame = AOUT_SPDIF_SIZE;
        aout->format.i_frame_length = A52_FRAME_NB;
        aout_VolumeNoneInit (aout);
    }
    else
    {
        aout->format.i_original_channels =
        aout->format.i_physical_channels = map;
        aout_VolumeSoftInit (aout);
    }

    aout->pf_play = Play;
    if (snd_pcm_hw_params_can_pause (hw))
        aout->pf_pause = Pause;
    else
    {
        aout->pf_pause = PauseDummy;
        msg_Warn (aout, "device cannot be paused");
    }
    aout->pf_flush = Flush;

    Probe (obj, device);
    free (device);
    return 0;

error:
    snd_pcm_close (pcm);
    free (device);
    free (sys);
    return VLC_EGENERIC;
}

/**
 * Queues one audio buffer to the hardware.
 */
static void Play (audio_output_t *aout, block_t *block)
{
    aout_sys_t *sys = aout->sys;
    snd_pcm_t *pcm = sys->pcm;
    snd_pcm_sframes_t frames;
    snd_pcm_state_t state = snd_pcm_state (pcm);

    if (snd_pcm_delay (pcm, &frames) == 0)
    {
        mtime_t delay = frames * CLOCK_FREQ / aout->format.i_rate;

        if (state != SND_PCM_STATE_RUNNING)
        {
            delay = block->i_pts - (mdate () + delay);
            if (delay > 0)
            {
                if (aout->format.i_format != VLC_CODEC_SPDIFL)
                {
                    frames = (delay * aout->format.i_rate) / CLOCK_FREQ;
                    msg_Dbg (aout, "prepending %ld zeroes", frames);

                    void *z = calloc (frames, aout->format.i_bytes_per_frame);
                    if (likely(z != NULL))
                    {
                        snd_pcm_writei (pcm, z, frames);
                        free (z);
                        delay = 0;
                    }
                }
                /* Lame fallback if zero padding does not work */
                if (delay > 0)
                {
                    msg_Dbg (aout, "deferring start (%"PRId64" us)", delay);
                    msleep (delay);
                }
            }
            else
                msg_Dbg (aout, "starting late (%"PRId64" us)", delay);
        }
        else
            aout_TimeReport (aout, block->i_pts - delay);
    }

    /* TODO: better overflow handling */
    /* TODO: no period wake ups */

    while (block->i_nb_samples > 0)
    {
        frames = snd_pcm_writei (pcm, block->p_buffer, block->i_nb_samples);
        if (frames >= 0)
        {
            size_t bytes = snd_pcm_frames_to_bytes (pcm, frames);
            block->i_nb_samples -= frames;
            block->p_buffer += bytes;
            block->i_buffer -= bytes;
            // pts, length
        }
        else  
        {
            int val = snd_pcm_recover (pcm, frames, 1);
            if (val)
            {
                msg_Err (aout, "cannot recover playback stream: %s",
                         snd_strerror (val));
                DumpDeviceStatus (aout, pcm);
                break;
            }
            msg_Warn (aout, "cannot write samples: %s", snd_strerror (frames));
        }
    }
    block_Release (block);
}

/**
 * Pauses/resumes the audio playback.
 */
static void Pause (audio_output_t *aout, bool pause, mtime_t date)
{
    snd_pcm_t *pcm = aout->sys->pcm;

    int val = snd_pcm_pause (pcm, pause);
    if (unlikely(val))
        PauseDummy (aout, pause, date);
}

static void PauseDummy (audio_output_t *aout, bool pause, mtime_t date)
{
    snd_pcm_t *pcm = aout->sys->pcm;

    /* Stupid device cannot pause. Discard samples. */
    if (pause)
        snd_pcm_drop (pcm);
    else
        snd_pcm_prepare (pcm);
    (void) date;
}

/**
 * Flushes/drains the audio playback buffer.
 */
static void Flush (audio_output_t *aout, bool wait)
{
    snd_pcm_t *pcm = aout->sys->pcm;

    if (wait)
        snd_pcm_drain (pcm);
    else
        snd_pcm_drop (pcm);
    snd_pcm_prepare (pcm);
}


/**
 * Releases the audio output.
 */
static void Close (vlc_object_t *obj)
{
    audio_output_t *aout = (audio_output_t *)obj;
    aout_sys_t *sys = aout->sys;
    snd_pcm_t *pcm = aout->sys->pcm;

    /* FIXME: ugly hack so selected ALSA device survives restart */
    char *device = var_InheritString (obj, "audio-device");
    if (device != NULL)
    {
        if (!var_Type (obj, "alsa-audio-device"))
            var_Create (obj, "alsa-audio-device", VLC_VAR_STRING);
        var_SetString (obj, "alsa-audio-device", device);
        free (device);
    }

    var_DelCallback (obj, "audio-device", aout_ChannelsRestart, NULL);
    var_Destroy (obj, "audio-device");
    snd_pcm_drop (pcm);
    snd_pcm_close (pcm);
    free (sys);
}

/*****************************************************************************
 * config variable callback
 *****************************************************************************/
static int FindDevicesCallback( vlc_object_t *p_this, char const *psz_name,
                               vlc_value_t newval, vlc_value_t oldval, void *p_unused )
{
    module_config_t *p_item;
    (void)newval;
    (void)oldval;
    (void)p_unused;

    p_item = config_FindConfig( p_this, psz_name );
    if( !p_item ) return VLC_SUCCESS;

    /* Clear-up the current list */
    if( p_item->i_list )
    {
        int i;

        /* Keep the first entrie */
        for( i = 1; i < p_item->i_list; i++ )
        {
            free( (char *)p_item->ppsz_list[i] );
            free( (char *)p_item->ppsz_list_text[i] );
        }
        /* TODO: Remove when no more needed */
        p_item->ppsz_list[i] = NULL;
        p_item->ppsz_list_text[i] = NULL;
    }
    p_item->i_list = 1;

    GetDevices (p_this, p_item, "default");

    /* Signal change to the interface */
    p_item->b_dirty = true;

    return VLC_SUCCESS;
}


static void GetDevices (vlc_object_t *obj, module_config_t *item,
                        const char *prefs_dev)
{
    void **hints;
    bool hinted_default = false;
    bool hinted_prefs = !strcmp (prefs_dev, "default");

    msg_Dbg(obj, "Available ALSA PCM devices:");

    if (snd_device_name_hint(-1, "pcm", &hints) < 0)
        return;

    for (size_t i = 0; hints[i] != NULL; i++)
    {
        void *hint = hints[i];

        char *name = snd_device_name_get_hint(hint, "NAME");
        if (unlikely(name == NULL))
            continue;

        char *desc = snd_device_name_get_hint(hint, "DESC");
        if (desc != NULL)
            for (char *lf = strchr(desc, '\n'); lf; lf = strchr(lf, '\n'))
                 *lf = ' ';
        msg_Dbg(obj, "%s (%s)", (desc != NULL) ? desc : name, name);

        if (!strcmp (name, "default"))
            hinted_default = true;
        if (!strcmp (name, prefs_dev))
            hinted_prefs = true;

        if (item != NULL)
        {
            item->ppsz_list = xrealloc(item->ppsz_list,
                                       (item->i_list + 2) * sizeof(char *));
            item->ppsz_list_text = xrealloc(item->ppsz_list_text,
                                          (item->i_list + 2) * sizeof(char *));
            item->ppsz_list[item->i_list] = name;
            if (desc == NULL)
                desc = strdup(name);
            item->ppsz_list_text[item->i_list] = desc;
            item->i_list++;
        }
        else
        {
            vlc_value_t val, text;

            val.psz_string = name;
            text.psz_string = desc;
            var_Change(obj, "audio-device", VLC_VAR_ADDCHOICE, &val, &text);
            free(desc);
            free(name);
        }
    }

    snd_device_name_free_hint(hints);

    if (item != NULL)
    {
        item->ppsz_list[item->i_list] = NULL;
        item->ppsz_list_text[item->i_list] = NULL;
    }
    else
    {
        vlc_value_t val, text;

        if (!hinted_default)
        {
            val.psz_string = (char *)"default";
            text.psz_string = (char *)N_("Default");
            var_Change(obj, "audio-device", VLC_VAR_ADDCHOICE, &val, &text);
        }

        val.psz_string = (char *)prefs_dev;
        if (!hinted_prefs)
        {
            text.psz_string = (char *)N_("VLC preferences");
            var_Change(obj, "audio-device", VLC_VAR_ADDCHOICE, &val, &text);
        }
        var_Change(obj, "audio-device", VLC_VAR_SETVALUE, &val, NULL);
    }
}
