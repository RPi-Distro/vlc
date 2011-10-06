/*****************************************************************************
 * pulse.c : Pulseaudio output plugin for vlc
 *****************************************************************************
 * Copyright (C) 2008 the VideoLAN team
 * Copyright (C) 2009-2011 Rémi Denis-Courmont
 *
 * Authors: Martin Hamrle <hamrle @ post . cz>
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

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_aout.h>
#include <vlc_cpu.h>

#include <pulse/pulseaudio.h>
#define vlc_pa_rttime_free(e) \
    ((pa_threaded_mainloop_get_api (sys->mainloop))->time_free (e))

#if !defined(PA_CHECK_VERSION) || !PA_CHECK_VERSION(0,9,22)
    #ifdef X_DISPLAY_MISSING
    # error Xlib required due to PulseAudio bug 799!
    #endif
    #include <vlc_xlib.h>
#endif

static int  Open        ( vlc_object_t * );
static void Close       ( vlc_object_t * );

vlc_module_begin ()
    set_shortname( "PulseAudio" )
    set_description( N_("Pulseaudio audio output") )
    set_capability( "audio output", 160 )
    set_category( CAT_AUDIO )
    set_subcategory( SUBCAT_AUDIO_AOUT )
    add_shortcut( "pulseaudio" )
    add_shortcut( "pa" )
    set_callbacks( Open, Close )
vlc_module_end ()

struct aout_sys_t
{
    pa_stream *stream; /**< PulseAudio playback stream object */
    pa_context *context; /**< PulseAudio connection context */
    pa_threaded_mainloop *mainloop; /**< PulseAudio event loop */
    pa_time_event *trigger; /**< Deferred stream trigger */
    mtime_t pts; /**< Play time of buffer write offset */
    mtime_t desync; /**< Measured desynchronization */
    unsigned rate; /**< Current stream sample rate */
};

/* Context helpers */
static void context_state_cb(pa_context *c, void *userdata)
{
    pa_threaded_mainloop *mainloop = userdata;

    switch (pa_context_get_state(c)) {
        case PA_CONTEXT_READY:
        case PA_CONTEXT_FAILED:
        case PA_CONTEXT_TERMINATED:
            pa_threaded_mainloop_signal(mainloop, 0);
        default:
            break;
    }
}

static bool context_wait(pa_threaded_mainloop *mainloop, pa_context *context)
{
    pa_context_state_t state;

    while ((state = pa_context_get_state(context)) != PA_CONTEXT_READY) {
        if (state == PA_CONTEXT_FAILED || state == PA_CONTEXT_TERMINATED)
            return -1;
        pa_threaded_mainloop_wait(mainloop);
    }
    return 0;
}

static void error(aout_instance_t *aout, const char *msg, pa_context *context)
{
    msg_Err(aout, "%s: %s", msg, pa_strerror(pa_context_errno(context)));
}

/* Latency management and lip synchronization */
static mtime_t vlc_pa_get_latency(aout_instance_t *aout,
                                  pa_context *ctx, pa_stream *s)
{
    pa_usec_t latency;
    int negative;

    if (pa_stream_get_latency(s, &latency, &negative)) {
        if (pa_context_errno (ctx) != PA_ERR_NODATA)
            error(aout, "unknown latency", ctx);
        return VLC_TS_INVALID;
    }
    return negative ? -latency : +latency;
}

static void stream_reset_sync(pa_stream *s, aout_instance_t *aout)
{
    aout_sys_t *sys = aout->output.p_sys;
    const unsigned rate = aout->output.output.i_rate;

    sys->pts = VLC_TS_INVALID;
    sys->desync = 0;
    pa_operation *op = pa_stream_update_sample_rate(s, rate, NULL, NULL);
    if (unlikely(op == NULL))
        return;
    pa_operation_unref(op);
    sys->rate = rate;
}

static void stream_start(pa_stream *s, aout_instance_t *aout)
{
    aout_sys_t *sys = aout->output.p_sys;
    pa_operation *op;

    if (sys->trigger != NULL) {
        vlc_pa_rttime_free(sys->trigger);
        sys->trigger = NULL;
    }

    op = pa_stream_cork(s, 0, NULL, NULL);
    if (op != NULL)
        pa_operation_unref(op);
    op = pa_stream_trigger(s, NULL, NULL);
    if (likely(op != NULL))
        pa_operation_unref(op);
}

static void stream_stop(pa_stream *s, aout_instance_t *aout)
{
    aout_sys_t *sys = aout->output.p_sys;
    pa_operation *op;

    if (sys->trigger != NULL) {
        vlc_pa_rttime_free(sys->trigger);
        sys->trigger = NULL;
    }

    op = pa_stream_cork(s, 1, NULL, NULL);
    if (op != NULL)
        pa_operation_unref(op);
}

static void stream_trigger_cb(pa_mainloop_api *api, pa_time_event *e,
                              const struct timeval *tv, void *userdata)
{
    aout_instance_t *aout = userdata;
    aout_sys_t *sys = aout->output.p_sys;

    msg_Dbg(aout, "starting deferred");
    assert (sys->trigger == e);
    stream_start(sys->stream, aout);
    (void) api; (void) e; (void) tv;
}

/**
 * Starts or resumes the playback stream.
 * Tries start playing back audio samples at the most accurate time
 * in order to minimize desync and resampling during early playback.
 * @note PulseAudio lock required.
 */
static void stream_resync(aout_instance_t *aout, pa_stream *s)
{
    aout_sys_t *sys = aout->output.p_sys;
    mtime_t delta;

    assert (pa_stream_is_corked(s) > 0);
    assert (sys->pts != VLC_TS_INVALID);

    delta = vlc_pa_get_latency(aout, sys->context, s);
    if (unlikely(delta == VLC_TS_INVALID))
        delta = 0; /* screwed */

    delta = (sys->pts - mdate()) - delta;
    if (delta > 0) {
        if (sys->trigger == NULL) {
            msg_Dbg(aout, "deferring start (%"PRId64" us)", delta);
            delta += pa_rtclock_now();
            sys->trigger = pa_context_rttime_new(sys->context, delta,
                                                 stream_trigger_cb, aout);
        }
    } else {
        msg_Warn(aout, "starting late (%"PRId64" us)", delta);
        stream_start(s, aout);
    }
}

/* Values from EBU R37 */
#define AOUT_EARLY_TOLERANCE 40000
#define AOUT_LATE_TOLERANCE  60000

static void stream_latency_cb(pa_stream *s, void *userdata)
{
    aout_instance_t *aout = userdata;
    aout_sys_t *sys = aout->output.p_sys;
    mtime_t delta, change;

    if (pa_stream_is_corked(s))
        return;
    if (sys->pts == VLC_TS_INVALID)
    {
        msg_Dbg(aout, "missing latency from input");
        return;
    }

    /* Compute lip desynchronization */
    delta = vlc_pa_get_latency(aout, sys->context, s);
    if (delta == VLC_TS_INVALID)
        return;

    delta = (sys->pts - mdate()) - delta;
    change = delta - sys->desync;
    sys->desync = delta;
    //msg_Dbg(aout, "desync: %+"PRId64" us (variation: %+"PRId64" us)",
    //        delta, change);

    const unsigned inrate = aout->output.output.i_rate;
    unsigned outrate = sys->rate;
    bool sync = false;

    if (delta < -AOUT_LATE_TOLERANCE)
        msg_Warn(aout, "too late by %"PRId64" us", -delta);
    else if (delta > +AOUT_EARLY_TOLERANCE)
        msg_Warn(aout, "too early by %"PRId64" us", delta);
    else if (outrate  == inrate)
        return; /* In sync, do not add unnecessary disturbance! */
    else
        sync = true;

    /* Compute playback sample rate */
    /* This is empirical (especially the shift values).
     * Feel free to define something smarter. */
    int adj = sync ? (outrate - inrate)
                   : outrate * ((delta >> 4) + change) / (CLOCK_FREQ << 2);
    /* This avoids too quick rate variation. It sounds really bad and
     * causes unstability (e.g. oscillation around the correct rate). */
    int limit = inrate >> 10;
    /* However, to improve stability and try to converge, closing to the
     * nominal rate is favored over drifting from it. */
    if ((adj > 0) == (sys->rate > inrate))
        limit *= 2;
    if (adj > +limit)
        adj = +limit;
    if (adj < -limit)
        adj = -limit;
    outrate -= adj;

    /* This keeps the effective rate within specified range
     * (+/-AOUT_MAX_RESAMPLING% - see <vlc_aout.h>) of the nominal rate. */
    limit = inrate * AOUT_MAX_RESAMPLING / 100;
    if (outrate > inrate + limit)
        outrate = inrate + limit;
    if (outrate < inrate - limit)
        outrate = inrate - limit;

    /* Apply adjusted sample rate */
    if (outrate == sys->rate)
        return;
    pa_operation *op = pa_stream_update_sample_rate(s, outrate, NULL, NULL);
    if (unlikely(op == NULL)) {
        error(aout, "cannot change sample rate", sys->context);
        return;
    }
    pa_operation_unref(op);
    msg_Dbg(aout, "changed sample rate to %u Hz",outrate);
    sys->rate = outrate;
}

/* Stream helpers */
static void stream_state_cb(pa_stream *s, void *userdata)
{
    pa_threaded_mainloop *mainloop = userdata;

    switch (pa_stream_get_state(s)) {
        case PA_STREAM_READY:
        case PA_STREAM_FAILED:
        case PA_STREAM_TERMINATED:
            pa_threaded_mainloop_signal(mainloop, 0);
        default:
            break;
    }
    (void) userdata;
}

static void stream_moved_cb(pa_stream *s, void *userdata)
{
    vlc_object_t *obj = userdata;

    msg_Dbg(obj, "connected to device %s (%u)",
            pa_stream_get_device_name(s),
            pa_stream_get_device_index(s));
}

static void stream_overflow_cb(pa_stream *s, void *userdata)
{
    aout_instance_t *aout = userdata;

    msg_Err(aout, "overflow");
    (void) s;
}

static void stream_started_cb(pa_stream *s, void *userdata)
{
    aout_instance_t *aout = userdata;

    msg_Dbg(aout, "started");
    (void) s;
}

static void stream_suspended_cb(pa_stream *s, void *userdata)
{
    aout_instance_t *aout = userdata;

    msg_Dbg(aout, "suspended");
    stream_reset_sync(s, aout);
}

static void stream_underflow_cb(pa_stream *s, void *userdata)
{
    aout_instance_t *aout = userdata;

    msg_Warn(aout, "underflow");
    stream_stop(s, aout);
    stream_reset_sync(s, aout);
}

static int stream_wait(pa_threaded_mainloop *mainloop, pa_stream *stream)
{
    pa_stream_state_t state;

    while ((state = pa_stream_get_state(stream)) != PA_STREAM_READY) {
        if (state == PA_STREAM_FAILED || state == PA_STREAM_TERMINATED)
            return -1;
        pa_threaded_mainloop_wait(mainloop);
    }
    return 0;
}

/* Memory free callback. The block_t address is in front of the data. */
static void data_free(void *data)
{
    block_t **pp = data, *block;

    memcpy(&block, pp - 1, sizeof (block));
    block_Release(block);
}

static void *data_convert(block_t **pp)
{
    block_t *block = *pp;
    /* In most cases, there is enough head room, and this is really cheap: */
    block = block_Realloc(block, sizeof (block), block->i_buffer);
    *pp = block;
    if (unlikely(block == NULL))
        return NULL;

    memcpy(block->p_buffer, &block, sizeof (block));
    block->p_buffer += sizeof (block);
    block->i_buffer -= sizeof (block);
    return block->p_buffer;
}

/*****************************************************************************
 * Play: play a sound samples buffer
 *****************************************************************************/
static void Play(aout_instance_t *aout)
{
    aout_sys_t *sys = aout->output.p_sys;
    pa_stream *s = sys->stream;

    /* This function is called exactly once per block in the output FIFO. */
    block_t *block = aout_FifoPop(aout, &aout->output.fifo);
    assert (block != NULL);

    const void *ptr = data_convert(&block);
    if (unlikely(ptr == NULL))
        return;

    size_t len = block->i_buffer;
    mtime_t pts = block->i_pts + block->i_length;

    /* Note: The core already holds the output FIFO lock at this point.
     * Therefore we must not under any circumstances (try to) acquire the
     * output FIFO lock while the PulseAudio threaded main loop lock is held
     * (including from PulseAudio stream callbacks). Otherwise lock inversion
     * will take place, and sooner or later a deadlock. */
    pa_threaded_mainloop_lock(sys->mainloop);

    sys->pts = pts;
    if (pa_stream_is_corked(s) > 0)
        stream_resync(aout, s);

#if 0 /* Fault injector to test underrun recovery */
    static volatile unsigned u = 0;
    if ((++u % 1000) == 0) {
        msg_Err(aout, "fault injection");
        pa_operation_unref(pa_stream_flush(s, NULL, NULL));
    }
#endif

    if (pa_stream_write(s, ptr, len, data_free, 0, PA_SEEK_RELATIVE) < 0) {
        error(aout, "cannot write", sys->context);
        block_Release(block);
    }

    pa_threaded_mainloop_unlock(sys->mainloop);
}

/*****************************************************************************
 * Open: open the audio device
 *****************************************************************************/
static int Open(vlc_object_t *obj)
{
    aout_instance_t *aout = (aout_instance_t *)obj;

#if !defined(PA_CHECK_VERSION) || !PA_CHECK_VERSION(0,9,22)
    if( !vlc_xlib_init( obj ) )
        return VLC_EGENERIC;
#endif

    /* Sample format specification */
    struct pa_sample_spec ss;
#if PA_CHECK_VERSION(1,0,0)
    pa_encoding_t encoding = PA_ENCODING_INVALID;
#endif

    switch(aout->output.output.i_format)
    {
        case VLC_CODEC_F64B:
            aout->output.output.i_format = VLC_CODEC_F32B;
        case VLC_CODEC_F32B:
            ss.format = PA_SAMPLE_FLOAT32BE;
            break;
        case VLC_CODEC_F64L:
            aout->output.output.i_format = VLC_CODEC_F32L;
        case VLC_CODEC_F32L:
            ss.format = PA_SAMPLE_FLOAT32LE;
            break;
        case VLC_CODEC_FI32:
            aout->output.output.i_format = VLC_CODEC_FL32;
            ss.format = PA_SAMPLE_FLOAT32NE;
            break;
        case VLC_CODEC_S32B:
            ss.format = PA_SAMPLE_S32BE;
            break;
        case VLC_CODEC_S32L:
            ss.format = PA_SAMPLE_S32LE;
            break;
        case VLC_CODEC_S24B:
            ss.format = PA_SAMPLE_S24BE;
            break;
        case VLC_CODEC_S24L:
            ss.format = PA_SAMPLE_S24LE;
            break;
        case VLC_CODEC_S16B:
            ss.format = PA_SAMPLE_S16BE;
            break;
        case VLC_CODEC_S16L:
            ss.format = PA_SAMPLE_S16LE;
            break;
        case VLC_CODEC_S8:
            aout->output.output.i_format = VLC_CODEC_U8;
        case VLC_CODEC_U8:
            ss.format = PA_SAMPLE_U8;
            break;
#if PA_CHECK_VERSION(1,0,0)
        case VLC_CODEC_A52:
            aout->output.output.i_format = VLC_CODEC_SPDIFL;
            encoding = PA_ENCODING_AC3_IEC61937;
            ss.format = HAVE_FPU ? PA_SAMPLE_FLOAT32NE : PA_SAMPLE_S16NE;
            break;
        /*case VLC_CODEC_EAC3:
            aout->output.output.i_format = VLC_CODEC_SPDIFL FIXME;
            encoding = PA_ENCODING_EAC3_IEC61937;
            ss.format = HAVE_FPU ? PA_SAMPLE_FLOAT32NE : PA_SAMPLE_S16NE;
            break;
        case VLC_CODEC_MPGA:
            aout->output.output.i_format = VLC_CODEC_SPDIFL FIXME;
            encoding = PA_ENCODING_MPEG_IEC61937;
            ss.format = HAVE_FPU ? PA_SAMPLE_FLOAT32NE : PA_SAMPLE_S16NE;
            break;*/
        case VLC_CODEC_DTS:
            aout->output.output.i_format = VLC_CODEC_SPDIFL;
            encoding = PA_ENCODING_DTS_IEC61937;
            ss.format = HAVE_FPU ? PA_SAMPLE_FLOAT32NE : PA_SAMPLE_S16NE;
            break;
#endif
        default:
            if (HAVE_FPU)
            {
                aout->output.output.i_format = VLC_CODEC_FL32;
                ss.format = PA_SAMPLE_FLOAT32NE;
            }
            else
            {
                aout->output.output.i_format = VLC_CODEC_S16N;
                ss.format = PA_SAMPLE_S16NE;
            }
            break;
    }

    ss.rate = aout->output.output.i_rate;
    ss.channels = aout_FormatNbChannels(&aout->output.output);
    if (!pa_sample_spec_valid(&ss)) {
        msg_Err(aout, "unsupported sample specification");
        return VLC_EGENERIC;
    }

    /* Channel mapping (order defined in vlc_aout.h) */
    struct pa_channel_map map;
    map.channels = 0;

    if (aout->output.output.i_physical_channels & AOUT_CHAN_LEFT)
        map.map[map.channels++] = PA_CHANNEL_POSITION_FRONT_LEFT;
    if (aout->output.output.i_physical_channels & AOUT_CHAN_RIGHT)
        map.map[map.channels++] = PA_CHANNEL_POSITION_FRONT_RIGHT;
    if (aout->output.output.i_physical_channels & AOUT_CHAN_MIDDLELEFT)
        map.map[map.channels++] = PA_CHANNEL_POSITION_SIDE_LEFT;
    if (aout->output.output.i_physical_channels & AOUT_CHAN_MIDDLERIGHT)
        map.map[map.channels++] = PA_CHANNEL_POSITION_SIDE_RIGHT;
    if (aout->output.output.i_physical_channels & AOUT_CHAN_REARLEFT)
        map.map[map.channels++] = PA_CHANNEL_POSITION_REAR_LEFT;
    if (aout->output.output.i_physical_channels & AOUT_CHAN_REARRIGHT)
        map.map[map.channels++] = PA_CHANNEL_POSITION_REAR_RIGHT;
    if (aout->output.output.i_physical_channels & AOUT_CHAN_REARCENTER)
        map.map[map.channels++] = PA_CHANNEL_POSITION_REAR_CENTER;
    if (aout->output.output.i_physical_channels & AOUT_CHAN_CENTER)
    {
        if (ss.channels == 1)
            map.map[map.channels++] = PA_CHANNEL_POSITION_MONO;
        else
            map.map[map.channels++] = PA_CHANNEL_POSITION_FRONT_CENTER;
    }
    if (aout->output.output.i_physical_channels & AOUT_CHAN_LFE)
        map.map[map.channels++] = PA_CHANNEL_POSITION_LFE;

    for (unsigned i = 0; map.channels < ss.channels; i++) {
        map.map[map.channels++] = PA_CHANNEL_POSITION_AUX0 + i;
        msg_Warn(aout, "mapping channel %"PRIu8" to AUX%u", map.channels, i);
    }

    if (!pa_channel_map_valid(&map)) {
        msg_Err(aout, "unsupported channel map");
        return VLC_EGENERIC;
    } else {
        const char *name = pa_channel_map_to_name(&map);
        msg_Dbg(aout, "using %s channel map", (name != NULL) ? name : "?");
    }

    /* Stream parameters */
    const pa_stream_flags_t flags = PA_STREAM_START_CORKED
                                  //| PA_STREAM_INTERPOLATE_TIMING
                                  | PA_STREAM_AUTO_TIMING_UPDATE
                                  | PA_STREAM_VARIABLE_RATE;

    const uint32_t byterate = pa_bytes_per_second(&ss);
    struct pa_buffer_attr attr;
    attr.maxlength = -1;
    /* PulseAudio assumes that tlength bytes are available in the buffer. Thus
     * we need to be conservative and set the minimum value that the VLC
     * audio decoder thread warrants. Otherwise, PulseAudio buffers will
     * underrun on hardware with large buffers. VLC keeps at least
     * AOUT_MIN_PREPARE and at most AOUT_MAX_PREPARE worth of audio buffers.
     * TODO? tlength could be adaptively increased to reduce wakeups. */
    attr.tlength = byterate * AOUT_MIN_PREPARE_TIME / CLOCK_FREQ;
    attr.prebuf = 0; /* trigger manually */
    attr.minreq = -1;
    attr.fragsize = 0; /* not used for output */

    /* Allocate structures */
    aout_sys_t *sys = malloc(sizeof(*sys));
    if (unlikely(sys == NULL))
        return VLC_ENOMEM;
    aout->output.p_sys = sys;
    sys->context = NULL;
    sys->stream = NULL;
    //sys->byterate = byterate;

    /* Allocate threaded main loop */
    pa_threaded_mainloop *mainloop = pa_threaded_mainloop_new();
    if (unlikely(mainloop == NULL)) {
        free(sys);
        return VLC_ENOMEM;
    }
    sys->mainloop = mainloop;

    if (pa_threaded_mainloop_start(mainloop) < 0) {
        pa_threaded_mainloop_free(mainloop);
        free(sys);
        return VLC_ENOMEM;
    }
    pa_threaded_mainloop_lock(mainloop);

    /* Connect to PulseAudio server */
    char *user_agent = var_InheritString(aout, "user-agent");
    pa_context *ctx = pa_context_new(pa_threaded_mainloop_get_api(mainloop),
                                     user_agent);
    free(user_agent);
    if (unlikely(ctx == NULL))
        goto fail;
    sys->context = ctx;
    sys->trigger = NULL;
    sys->pts = VLC_TS_INVALID;
    sys->desync = 0;
    sys->rate = ss.rate;

    pa_context_set_state_callback(ctx, context_state_cb, mainloop);
    if (pa_context_connect(ctx, NULL, 0, NULL) < 0
     || context_wait(mainloop, ctx)) {
        error(aout, "cannot connect to server", ctx);
        goto fail;
    }

#if PA_CHECK_VERSION(1,0,0)
    pa_format_info *formatv[2];
    unsigned formatc = 0;

    /* Favor digital pass-through if available*/
    if (encoding != PA_ENCODING_INVALID) {
        formatv[formatc] = pa_format_info_new();
        formatv[formatc]->encoding = encoding;
        pa_format_info_set_rate(formatv[formatc], ss.rate);
        pa_format_info_set_channels(formatv[formatc], ss.channels);
        formatc++;
    }

    /* Fallback to PCM */
    formatv[formatc] = pa_format_info_new();
    formatv[formatc]->encoding = PA_ENCODING_PCM;
    pa_format_info_set_sample_format(formatv[formatc], ss.format);
    pa_format_info_set_rate(formatv[formatc], ss.rate);
    pa_format_info_set_channels(formatv[formatc], ss.channels);
    formatc++;

    /* Create a playback stream */
    pa_stream *s;

    s = pa_stream_new_extended(ctx, "audio stream", formatv, formatc, NULL);

    for (unsigned i = 0; i < formatc; i++)
        pa_format_info_free(formatv[i]);
#else
    pa_stream *s = pa_stream_new(ctx, "audio stream", &ss, &map);
#endif
    if (s == NULL) {
        error(aout, "cannot create stream", ctx);
        goto fail;
    }
    sys->stream = s;
    pa_stream_set_state_callback(s, stream_state_cb, mainloop);
    pa_stream_set_latency_update_callback(s, stream_latency_cb, aout);
    pa_stream_set_moved_callback(s, stream_moved_cb, aout);
    pa_stream_set_overflow_callback(s, stream_overflow_cb, aout);
    pa_stream_set_started_callback(s, stream_started_cb, aout);
    pa_stream_set_suspended_callback(s, stream_suspended_cb, aout);
    pa_stream_set_underflow_callback(s, stream_underflow_cb, aout);

    if (pa_stream_connect_playback(s, NULL, &attr, flags, NULL, NULL) < 0
     || stream_wait(mainloop, s)) {
        error(aout, "cannot connect stream", ctx);
        goto fail;
    }
    stream_moved_cb(s, aout);

#if PA_CHECK_VERSION(1,0,0)
    if (encoding != PA_ENCODING_INVALID) {
        const pa_format_info *info = pa_stream_get_format_info(s);

        assert (info != NULL);
        if (pa_format_info_is_pcm (info)) {
            msg_Dbg(aout, "digital pass-through not available");
            aout->output.output.i_format = HAVE_FPU ? VLC_CODEC_FL32 : VLC_CODEC_S16N;
        } else {
            msg_Dbg(aout, "digital pass-through enabled");
            pa_stream_set_latency_update_callback(s, NULL, NULL);
        }
    }
#endif

    const struct pa_buffer_attr *pba = pa_stream_get_buffer_attr(s);
    msg_Dbg(aout, "using buffer metrics: maxlength=%u, tlength=%u, "
            "prebuf=%u, minreq=%u",
            pba->maxlength, pba->tlength, pba->prebuf, pba->minreq);

    aout->output.i_nb_samples = pba->minreq / pa_frame_size(&ss);
    pa_threaded_mainloop_unlock(mainloop);

    aout->output.pf_play = Play;
    aout_VolumeSoftInit(aout);
    return VLC_SUCCESS;

fail:
    pa_threaded_mainloop_unlock(mainloop);
    Close(obj);
    return VLC_EGENERIC;
}

/*****************************************************************************
 * Close: close the audio device
 *****************************************************************************/
static void Close (vlc_object_t *obj)
{
    aout_instance_t *aout = (aout_instance_t *)obj;
    aout_sys_t *sys = aout->output.p_sys;
    pa_threaded_mainloop *mainloop = sys->mainloop;
    pa_context *ctx = sys->context;
    pa_stream *s = sys->stream;

    pa_threaded_mainloop_lock(mainloop);
    if (s != NULL) {
        if (unlikely(sys->trigger != NULL))
            vlc_pa_rttime_free(sys->trigger);
        pa_stream_disconnect(s);

        /* Clear all callbacks */
        pa_stream_set_state_callback(s, NULL, NULL);
        pa_stream_set_latency_update_callback(s, NULL, aout);
        pa_stream_set_moved_callback(s, NULL, aout);
        pa_stream_set_overflow_callback(s, NULL, aout);
        pa_stream_set_started_callback(s, NULL, aout);
        pa_stream_set_suspended_callback(s, NULL, aout);
        pa_stream_set_underflow_callback(s, NULL, aout);

        pa_stream_unref(s);
    }
    if (ctx != NULL) {
        pa_context_disconnect(ctx);
        pa_context_set_state_callback (ctx, NULL, NULL);
        pa_context_unref(ctx);
    }
    pa_threaded_mainloop_unlock(mainloop);
    pa_threaded_mainloop_free(mainloop);
    free(sys);
}
