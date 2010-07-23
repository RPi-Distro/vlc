/*****************************************************************************
 * imem.c : Memory input for VLC
 *****************************************************************************
 * Copyright (C) 2009-2010 Laurent Aimar
 * $Id: 25cd4fbbb271c86f1e10aa280208fb260f7938e6 $
 *
 * Author: Laurent Aimar <fenrir _AT_ videolan _DOT org>
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
#include <limits.h>
#include <math.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_access.h>
#include <vlc_demux.h>
#include <vlc_charset.h>

/*****************************************************************************
 * Module descriptior
 *****************************************************************************/
static int  OpenAccess (vlc_object_t *);
static void CloseAccess(vlc_object_t *);

static int  OpenDemux (vlc_object_t *);
static void CloseDemux(vlc_object_t *);

#define CACHING_TEXT N_("Caching value in ms")
#define CACHING_LONGTEXT N_(\
    "Caching value for imem streams. This " \
    "value should be set in milliseconds.")

#define ID_TEXT N_("ID")
#define ID_LONGTEXT N_(\
    "Set the ID of the elementary stream")

#define GROUP_TEXT N_("Group")
#define GROUP_LONGTEXT N_(\
    "Set the group of the elementary stream")

#define CAT_TEXT N_("Category")
#define CAT_LONGTEXT N_(\
    "Set the category of the elementary stream")
static const int cat_values[] = {
    0, 1, 2, 3, 4,
};
static const char *cat_texts[] = {
    N_("Unknown"), N_("Audio"), N_("Video"), N_("Subtitle"), N_("Data")
};

#define CODEC_TEXT N_("Codec")
#define CODEC_LONGTEXT N_(\
    "Set the codec of the elementary stream")

#define LANGUAGE_TEXT N_("Language")
#define LANGUAGE_LONGTEXT N_(\
    "Language of the elementary stream as described by ISO639")

#define SAMPLERATE_TEXT N_("Sample rate")
#define SAMPLERATE_LONGTEXT N_(\
    "Sample rate of an audio elementary stream")

#define CHANNELS_TEXT N_("Channels count")
#define CHANNELS_LONGTEXT N_(\
    "Channels count of an audio elementary stream")

#define WIDTH_TEXT N_("Width")
#define WIDTH_LONGTEXT N_("Width of video or subtitle elementary streams")

#define HEIGHT_TEXT N_("Height")
#define HEIGHT_LONGTEXT N_("Height of video or subtitle elementary streams")

#define DAR_TEXT N_("Display aspect ratio")
#define DAR_LONGTEXT N_(\
    "Display aspect ratio of a video elementary stream")

#define FPS_TEXT N_("Frame rate")
#define FPS_LONGTEXT N_(\
    "Frame rate of a video elementary stream")

#define COOKIE_TEXT N_("Callback cookie string")
#define COOKIE_LONGTEXT N_(\
    "Text identifier for the callback functions")

#define DATA_TEXT N_("Callback data")
#define DATA_LONGTEXT N_(\
    "Data for the get and release functions")

#define GET_TEXT N_("Get function")
#define GET_LONGTEXT N_(\
    "Address of the get callback function")

#define RELEASE_TEXT N_("Release function")
#define RELEASE_LONGTEXT N_(\
    "Address of the release callback function")

#define SIZE_TEXT N_("Size")
#define SIZE_LONGTEXT N_(\
    "Size of stream in bytes")

vlc_module_begin()
    set_shortname(N_("Memory input"))
    set_description(N_("Memory input"))
    set_category(CAT_INPUT)
    set_subcategory(SUBCAT_INPUT_ACCESS)

    add_integer("imem-caching", DEFAULT_PTS_DELAY / 1000, NULL, CACHING_TEXT, CACHING_LONGTEXT, true)
        change_private()
    add_string ("imem-get", "0", NULL, GET_TEXT, GET_LONGTEXT, true)
        change_volatile()
    add_string ("imem-release", "0", NULL, RELEASE_TEXT, RELEASE_LONGTEXT, true)
        change_volatile()
    add_string ("imem-cookie", NULL, NULL, COOKIE_TEXT, COOKIE_LONGTEXT, true)
        change_volatile()
        change_safe()
    add_string ("imem-data", "0", NULL, DATA_TEXT, DATA_LONGTEXT, true)
        change_volatile()

    add_integer("imem-id", -1, NULL, ID_TEXT, ID_LONGTEXT, true)
        change_private()
        change_safe()
    add_integer("imem-group", 0, NULL, GROUP_TEXT, GROUP_LONGTEXT, true)
        change_private()
        change_safe()
    add_integer("imem-cat", 0, NULL, CAT_TEXT, CAT_LONGTEXT, true)
        change_integer_list(cat_values, cat_texts, NULL)
        change_private()
        change_safe()
    add_string ("imem-codec", NULL, NULL, CODEC_TEXT, CODEC_LONGTEXT, true)
        change_private()
        change_safe()
    add_string( "imem-language", NULL, NULL, LANGUAGE_TEXT, LANGUAGE_LONGTEXT, false)
        change_private()
        change_safe()

    add_integer("imem-samplerate", 0, NULL, SAMPLERATE_TEXT, SAMPLERATE_LONGTEXT, true)
        change_private()
        change_safe()
    add_integer("imem-channels", 0, NULL, CHANNELS_TEXT, CHANNELS_LONGTEXT, true)
        change_private()
        change_safe()

    add_integer("imem-width", 0, NULL, WIDTH_TEXT, WIDTH_LONGTEXT, true)
        change_private()
        change_safe()
    add_integer("imem-height", 0, NULL, HEIGHT_TEXT, HEIGHT_LONGTEXT, true)
        change_private()
        change_safe()
    add_string ("imem-dar", NULL, NULL, DAR_TEXT, DAR_LONGTEXT, true)
        change_private()
        change_safe()
    add_string ("imem-fps", NULL, NULL, FPS_TEXT, FPS_LONGTEXT, true)
        change_private()
        change_safe()

    add_integer ("imem-size", 0, NULL, SIZE_TEXT, SIZE_LONGTEXT, true)
        change_private()
        change_safe()

    add_shortcut("imem")
    set_capability("access_demux", 0)
    set_callbacks(OpenDemux, CloseDemux)

    add_submodule()
        add_shortcut("imem")
        set_capability("access", 0)
        set_callbacks(OpenAccess, CloseAccess)
vlc_module_end()

/*****************************************************************************
 * Exported API
 *****************************************************************************/

/* The clock origin for the DTS and PTS is assumed to be 0.
 * A negative value means unknown.
 *
 * TODO define flags
 */
typedef int  (*imem_get_t)(void *data, const char *cookie,
                           int64_t *dts, int64_t *pts, unsigned *flags,
                           size_t *, void **);
typedef void (*imem_release_t)(void *data, const char *cookie, size_t, void *);

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/

/* */
static block_t *Block(access_t *);
static int ControlAccess(access_t *, int, va_list);

static int Demux(demux_t *);
static int ControlDemux(demux_t *, int, va_list);

/* */
typedef struct {
    struct {
        imem_get_t      get;
        imem_release_t  release;
        void           *data;
        char           *cookie;
    } source;

    es_out_id_t  *es;

    mtime_t      pts_delay;

    mtime_t      dts;

    mtime_t      deadline;
} imem_sys_t;

static void ParseMRL(vlc_object_t *, const char *);
#define var_InheritRational(a,b,c,d) var_InheritRational(VLC_OBJECT(a),b,c,d)
static int (var_InheritRational)(vlc_object_t *,
                                 unsigned *num, unsigned *den,
                                 const char *var);

/**
 * It closes the common part of the access and access_demux
 */
static void CloseCommon(imem_sys_t *sys)
{
    free(sys->source.cookie);
    free(sys);
}

/**
 * It initializes the common part for imem access/access_demux.
 */
static int OpenCommon(vlc_object_t *object, imem_sys_t **sys_ptr, const char *psz_path)
{
    char *tmp;

	/* */
    imem_sys_t *sys = calloc(1, sizeof(*sys));
	if (!sys)
		return VLC_ENOMEM;

    /* Read the user functions */
    tmp = var_InheritString(object, "imem-get");
    if (tmp)
        sys->source.get = (imem_get_t)(intptr_t)strtoll(tmp, NULL, 0);
    free(tmp);

    tmp = var_InheritString(object, "imem-release");
    if (tmp)
        sys->source.release = (imem_release_t)(intptr_t)strtoll(tmp, NULL, 0);
    free(tmp);

    if (!sys->source.get || !sys->source.release) {
        msg_Err(object, "Invalid get/release function pointers");
        free(sys);
        return VLC_EGENERIC;
    }

    tmp = var_InheritString(object, "imem-data");
    if (tmp)
        sys->source.data = (void *)(uintptr_t)strtoull(tmp, NULL, 0);
    free(tmp);

    /* Now we can parse the MRL (get/release must not be parsed to avoid
     * security risks) */
    if (*psz_path)
        ParseMRL(object, psz_path);

    sys->source.cookie = var_InheritString(object, "imem-cookie");

    msg_Dbg(object, "Using get(%p), release(%p), data(%p), cookie(%s)",
            sys->source.get, sys->source.release, sys->source.data,
            sys->source.cookie ? sys->source.cookie : "(null)");

    /* */
    sys->pts_delay = var_InheritInteger(object, "imem-caching") * INT64_C(1000);
    sys->dts       = 0;
    sys->deadline  = VLC_TS_INVALID;

    *sys_ptr = sys;
    return VLC_SUCCESS;
}

/**
 * It opens an imem access.
 */
static int OpenAccess(vlc_object_t *object)
{
    access_t   *access = (access_t *)object;
    imem_sys_t *sys;

    if (OpenCommon(object, &sys, access->psz_path))
        return VLC_EGENERIC;

    if (var_InheritInteger(object, "imem-cat") != 4) {
        CloseCommon(sys);
        return VLC_EGENERIC;
    }

    /* */
    access_InitFields(access);
    access->pf_control = ControlAccess;
    access->pf_read    = NULL;
    access->pf_block   = Block;
    access->pf_seek    = NULL;
    access->p_sys      = (access_sys_t*)sys;
    access->info.i_size = var_InheritInteger(object, "imem-size");

    return VLC_SUCCESS;
}

/**
 * It closes an imem access
 */
static void CloseAccess(vlc_object_t *object)
{
    access_t *access = (access_t *)object;

    CloseCommon((imem_sys_t*)access->p_sys);
}

/**
 * It controls an imem access
 */
static int ControlAccess(access_t *access, int i_query, va_list args)
{
    imem_sys_t *sys = (imem_sys_t*)access->p_sys;

    switch (i_query)
    {
    case ACCESS_CAN_SEEK:
    case ACCESS_CAN_FASTSEEK: {
        bool *b = va_arg( args, bool* );
        *b = false;
        return VLC_SUCCESS;
    }
    case ACCESS_CAN_PAUSE:
    case ACCESS_CAN_CONTROL_PACE: {
        bool *b = va_arg( args, bool* );
        *b = true;
        return VLC_SUCCESS;
    }
    case ACCESS_GET_PTS_DELAY: {
        int64_t *delay = va_arg(args, int64_t *);
        *delay = sys->pts_delay;
        return VLC_SUCCESS;
    }
    case ACCESS_SET_PAUSE_STATE:
        return VLC_SUCCESS;

    case ACCESS_GET_TITLE_INFO:
    case ACCESS_SET_TITLE:
    case ACCESS_SET_SEEKPOINT:
    case ACCESS_SET_PRIVATE_ID_STATE:
    case ACCESS_GET_META:
    case ACCESS_GET_PRIVATE_ID_STATE:
    case ACCESS_GET_CONTENT_TYPE:
    default:
        return VLC_EGENERIC;
    }
}

/**
 * It retreives data using the get() callback, copies them,
 * and then release them using the release() callback.
 */
static block_t *Block(access_t *access)
{
    imem_sys_t *sys = (imem_sys_t*)access->p_sys;

    unsigned flags;
    size_t buffer_size;
    void   *buffer;

    if (sys->source.get(sys->source.data, sys->source.cookie,
                        NULL, NULL, &flags, &buffer_size, &buffer)) {
        access->info.b_eof = true;
        return NULL;
    }

    block_t *block = NULL;
    if (buffer_size > 0) {
        block = block_New(access, buffer_size);
        if (block)
            memcpy(block->p_buffer, buffer, buffer_size);
    }

    sys->source.release(sys->source.data, sys->source.cookie,
                        buffer_size, buffer);
    return block;
}

/**
 * It opens an imem access_demux.
 */
static int OpenDemux(vlc_object_t *object)
{
    demux_t    *demux = (demux_t *)object;
    imem_sys_t *sys;

    if (OpenCommon(object, &sys, demux->psz_path))
        return VLC_EGENERIC;

	/* ES format */
    es_format_t fmt;
	es_format_Init(&fmt, UNKNOWN_ES, 0);

    fmt.i_id = var_InheritInteger(object, "imem-id");
    fmt.i_group = var_InheritInteger(object, "imem-group");

    char *tmp = var_InheritString(object, "imem-codec");
    if (tmp)
        fmt.i_codec = vlc_fourcc_GetCodecFromString(UNKNOWN_ES, tmp);
    free(tmp);

    const int cat = var_InheritInteger(object, "imem-cat");
    switch (cat) {
    case 1: {
        fmt.i_cat = AUDIO_ES;
        fmt.audio.i_channels = var_InheritInteger(object, "imem-channels");
        fmt.audio.i_rate = var_InheritInteger(object, "imem-samplerate");

        msg_Dbg(object, "Audio %4.4s %d channels %d Hz",
                (const char *)&fmt.i_codec,
                fmt.audio.i_channels, fmt.audio.i_rate);
        break;
    }
    case 2: {
        fmt.i_cat = VIDEO_ES;
        fmt.video.i_width  = var_InheritInteger(object, "imem-width");
        fmt.video.i_height = var_InheritInteger(object, "imem-height");
        unsigned num, den;
        if (!var_InheritRational(object, &num, &den, "imem-dar") && num > 0 && den > 0) {
            if (fmt.video.i_width > 0 && fmt.video.i_height > 0) {
                fmt.video.i_sar_num = num * fmt.video.i_height;
                fmt.video.i_sar_den = den * fmt.video.i_width;
            }
        }
        if (!var_InheritRational(object, &num, &den, "imem-fps") && num > 0 && den > 0) {
            fmt.video.i_frame_rate      = num;
            fmt.video.i_frame_rate_base = den;
        }

        msg_Dbg(object, "Video %4.4s %dx%d  SAR %d:%d frame rate %u/%u",
                (const char *)&fmt.i_codec,
                fmt.video.i_width, fmt.video.i_height,
                fmt.video.i_sar_num, fmt.video.i_sar_den,
                fmt.video.i_frame_rate, fmt.video.i_frame_rate_base);
        break;
    }
    case 3: {
        fmt.i_cat = SPU_ES;
        fmt.subs.spu.i_original_frame_width =
            var_InheritInteger(object, "imem-width");
        fmt.subs.spu.i_original_frame_height =
            var_InheritInteger(object, "imem-height");

        msg_Dbg(object, "Subtitle %4.4s",
                (const char *)&fmt.i_codec);
        break;
    }
    default:
        if (cat != 4)
            msg_Err(object, "Invalid ES category");
        es_format_Clean(&fmt);
        CloseCommon(sys);
        return VLC_EGENERIC;
    }

    fmt.psz_language = var_InheritString(object, "imem-language");

	sys->es = es_out_Add(demux->out, &fmt);
    es_format_Clean(&fmt);

    if (!sys->es) {
        CloseCommon(sys);
        return VLC_EGENERIC;
    }

    /* */
    demux->pf_control = ControlDemux;
    demux->pf_demux   = Demux;
    demux->p_sys      = (demux_sys_t*)sys;

    demux->info.i_update = 0;
    demux->info.i_title = 0;
    demux->info.i_seekpoint = 0;
    return VLC_SUCCESS;
}

/**
 * It closes an imem access_demux
 */
static void CloseDemux(vlc_object_t *object)
{
    demux_t *demux = (demux_t *)object;

    CloseCommon((imem_sys_t*)demux->p_sys);
}

/**
 * It controls an imem access_demux
 */
static int ControlDemux(demux_t *demux, int i_query, va_list args)
{
    imem_sys_t *sys = (imem_sys_t*)demux->p_sys;

    switch (i_query)
    {
    case DEMUX_CAN_PAUSE:
    case DEMUX_CAN_CONTROL_PACE: {
        bool *b = va_arg(args, bool *);
        *b = true;
        return VLC_SUCCESS;
    }
    case DEMUX_SET_PAUSE_STATE:
        return VLC_SUCCESS;

    case DEMUX_GET_PTS_DELAY: {
        int64_t *delay = va_arg(args, int64_t *);
        *delay = sys->pts_delay;
        return VLC_SUCCESS;
    }
    case DEMUX_GET_POSITION: {
        double *position = va_arg(args, double *);
        *position = 0.0;
        return VLC_SUCCESS;
    }
    case DEMUX_GET_TIME: {
        int64_t *t = va_arg(args, int64_t *);
        *t = sys->dts;
        return VLC_SUCCESS;
    }
    case DEMUX_GET_LENGTH: {
        int64_t *l = va_arg(args, int64_t *);
        *l = 0;
        return VLC_SUCCESS;
    }
    case DEMUX_SET_NEXT_DEMUX_TIME:
        sys->deadline = va_arg(args, int64_t);
        return VLC_SUCCESS;

    /* */
    case DEMUX_CAN_SEEK:
    case DEMUX_SET_POSITION:
    case DEMUX_SET_TIME:
    default:
        return VLC_EGENERIC;
    }

    return VLC_EGENERIC;
}

/**
 * It retreives data using the get() callback, sends them to es_out
 * and the release it using the release() callback.
 */
static int Demux(demux_t *demux)
{
    imem_sys_t *sys = (imem_sys_t*)demux->p_sys;

    if (sys->deadline == VLC_TS_INVALID)
        sys->deadline = sys->dts + 1;

    for (;;) {
        if (sys->deadline <= sys->dts)
            break;

        /* */
        int64_t dts, pts;
        unsigned flags;
        size_t buffer_size;
        void   *buffer;

        if (sys->source.get(sys->source.data, sys->source.cookie,
                            &dts, &pts, &flags, &buffer_size, &buffer))
            return 0;

        if (dts < 0)
            dts = pts;

        if (buffer_size > 0) {
            block_t *block = block_New(demux, buffer_size);
            if (block) {
                block->i_dts = dts >= 0 ? (1 + dts) : VLC_TS_INVALID;
                block->i_pts = pts >= 0 ? (1 + pts) : VLC_TS_INVALID;
                memcpy(block->p_buffer, buffer, buffer_size);

                es_out_Control(demux->out, ES_OUT_SET_PCR, block->i_dts);
                es_out_Send(demux->out, sys->es, block);
            }
        }

        sys->dts = dts;

        sys->source.release(sys->source.data, sys->source.cookie,
                            buffer_size, buffer);
    }
    sys->deadline = VLC_TS_INVALID;
    return 1;
}

/**
 * It parses a rational number (it also accepts basic float number).
 *
 * It returns an error if the rational number cannot be parsed (0/0 is valid).
 */
static int (var_InheritRational)(vlc_object_t *object,
                                 unsigned *num, unsigned *den,
                                 const char *var)
{
    /* */
    *num = 0;
    *den = 0;

    /* */
    char *tmp = var_InheritString(object, var);
    if (!tmp)
        goto error;

    char *next;
    unsigned n = strtol(tmp,  &next, 0);
    unsigned d = strtol(*next ? &next[1] : "0", NULL, 0);

    if (*next == '.') {
        /* Interpret as a float number */
        double r = us_atof(tmp);
        double c = ceil(r);
        if (c >= UINT_MAX)
            goto error;
        unsigned m = c;
        if (m > 0) {
            d = UINT_MAX / m;
            n = r * d;
        } else {
            n = 0;
            d = 0;
        }
    }

    if (n > 0 && d > 0)
        vlc_ureduce(num, den, n, d, 0);

    free(tmp);
    return VLC_SUCCESS;

error:
    free(tmp);
    return VLC_EGENERIC;
}

/**
 * Parse the MRL and extract configuration from it.
 *
 * Syntax: option1=value1[:option2=value2[...]]
 *
 * XXX get and release are not supported on purpose.
 */
static void ParseMRL(vlc_object_t *object, const char *psz_path)
{
    static const struct {
        const char *name;
        int        type;
    } options[] = {
        { "caching",    VLC_VAR_INTEGER },
        { "id",         VLC_VAR_INTEGER },
        { "group",      VLC_VAR_INTEGER },
        { "cat",        VLC_VAR_INTEGER },
        { "samplerate", VLC_VAR_INTEGER },
        { "channels",   VLC_VAR_INTEGER },
        { "width",      VLC_VAR_INTEGER },
        { "height",     VLC_VAR_INTEGER },
        { "cookie",     VLC_VAR_STRING },
        { "codec",      VLC_VAR_STRING },
        { "language",   VLC_VAR_STRING },
        { "dar",        VLC_VAR_STRING },
        { "fps",        VLC_VAR_STRING },
        { NULL, -1 }
    };

    char *dup = strdup(psz_path);
    if (!dup)
        return;
    char *current = dup;

    while (current) {
        char *next = strchr(current, ':');
        if (next)
            *next++ = '\0';

        char *option = current;
        char *value = strchr(current, '=');
        if (value) {
            *value++ = '\0';
            msg_Dbg(object, "option '%s' value '%s'", option, value);
        } else {
            msg_Dbg(object, "option '%s' without value (unsupported)", option);
        }

        char *name;
        if (asprintf(&name, "imem-%s", option) < 0)
            name = NULL;
        for (unsigned i = 0; name && options[i].name; i++) {
            if (strcmp(options[i].name, option))
                continue;
            /* */
            var_Create(object, name, options[i].type | VLC_VAR_DOINHERIT);
            if (options[i].type == VLC_VAR_INTEGER && value) {
                var_SetInteger(object, name, strtol(value, NULL, 0));
            } else if (options[i].type == VLC_VAR_STRING && value) {
                var_SetString(object, name, value);
            }
            break;
        }
        free(name);

        /* */
        current = next;
    }
    free(dup);
}
