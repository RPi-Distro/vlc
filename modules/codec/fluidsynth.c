/*****************************************************************************
 * fluidsynth.c: Software MIDI synthesizer using libfluidsynth
 *****************************************************************************
 * Copyright © 2007 Rémi Denis-Courmont
 * $Id: 363a64fae3a79d667d46facdfa4d090765b74cb5 $
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
#include <vlc_codec.h>
#include <vlc_cpu.h>
#include <vlc_dialog.h>
#include <vlc_charset.h>

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#ifdef _POSIX_VERSION
# include <glob.h>
#endif

/* On Win32, we link statically */
#ifdef WIN32
# define FLUIDSYNTH_NOT_A_DLL
#endif

#include <fluidsynth.h>

#if (FLUIDSYNTH_VERSION_MAJOR < 1) \
 || (FLUIDSYNTH_VERSION_MAJOR == 1 && FLUIDSYNTH_VERSION_MINOR < 1)
# define FLUID_FAILED (-1)
# define fluid_synth_sysex(synth, ptr, len, d, e, f, g) (FLUID_FAILED)
# define fluid_synth_system_reset(synth) (FLUID_FAILED)
# define fluid_synth_channel_pressure(synth, channel, p) (FLUID_FAILED)
#endif

#define SOUNDFONT_TEXT N_("Sound fonts")
#define SOUNDFONT_LONGTEXT N_( \
    "A sound fonts file is required for software synthesis." )

#define GAIN_TEXT N_("Synthesis gain")
#define GAIN_LONGTEXT N_("This gain is applied to synthesis output. " \
    "High values may cause saturation when many notes are played at a time." )


static int  Open  (vlc_object_t *);
static void Close (vlc_object_t *);

vlc_module_begin ()
    set_description (N_("FluidSynth MIDI synthesizer"))
    set_capability ("decoder", 100)
    set_shortname (N_("FluidSynth"))
    set_category (CAT_INPUT)
    set_subcategory (SUBCAT_INPUT_ACODEC)
    set_callbacks (Open, Close)
    add_loadfile ("soundfont", "",
                  SOUNDFONT_TEXT, SOUNDFONT_LONGTEXT, false)
    add_float ("synth-gain", .5, GAIN_TEXT, GAIN_LONGTEXT, false)
        change_float_range (0., 10.)
vlc_module_end ()


struct decoder_sys_t
{
    fluid_settings_t *settings;
    fluid_synth_t    *synth;
    int               soundfont;
    bool              fixed;
    date_t            end_date;
};


static aout_buffer_t *DecodeBlock (decoder_t *p_dec, block_t **pp_block);


static int Open (vlc_object_t *p_this)
{
    decoder_t *p_dec = (decoder_t *)p_this;

    if (p_dec->fmt_in.i_codec != VLC_CODEC_MIDI)
        return VLC_EGENERIC;

    decoder_sys_t *p_sys = malloc (sizeof (*p_sys));
    if (unlikely(p_sys == NULL))
        return VLC_ENOMEM;

    p_sys->settings = new_fluid_settings ();
    p_sys->synth = new_fluid_synth (p_sys->settings);
    p_sys->soundfont = -1;

    char *font_path = var_InheritString (p_this, "soundfont");
    if (font_path != NULL)
    {
        const char *lpath = ToLocale (font_path);

        msg_Dbg (p_this, "loading sound fonts file %s", font_path);
        p_sys->soundfont = fluid_synth_sfload (p_sys->synth, font_path, 1);
        LocaleFree (lpath);
        if (p_sys->soundfont == -1)
            msg_Err (p_this, "cannot load sound fonts file %s", font_path);
        free (font_path);
    }
#ifdef _POSIX_VERSION
    else
    {
        glob_t gl;

        glob ("/usr/share/sounds/sf2/*.sf2", GLOB_NOESCAPE, NULL, &gl);
        for (size_t i = 0; i < gl.gl_pathc; i++)
        {
            const char *path = gl.gl_pathv[i];

            msg_Dbg (p_this, "loading sound fonts file %s", path);
            p_sys->soundfont = fluid_synth_sfload (p_sys->synth, path, 1);
            if (p_sys->soundfont != -1)
                break; /* it worked! */
            msg_Err (p_this, "cannot load sound fonts file %s", path);
        }
        globfree (&gl);
    }
#endif

    if (p_sys->soundfont == -1)
    {
        msg_Err (p_this, "sound font file required for synthesis");
        dialog_Fatal (p_this, _("MIDI synthesis not set up"),
            _("A sound font file (.SF2) is required for MIDI synthesis.\n"
              "Please install a sound font and configure it "
              "from the VLC preferences "
              "(Input / Codecs > Audio codecs > FluidSynth).\n"));
        delete_fluid_synth (p_sys->synth);
        delete_fluid_settings (p_sys->settings);
        free (p_sys);
        return VLC_EGENERIC;
    }

    fluid_synth_set_gain (p_sys->synth,
                          var_InheritFloat (p_this, "synth-gain"));

    p_dec->fmt_out.i_cat = AUDIO_ES;
    p_dec->fmt_out.audio.i_rate = 44100;
    p_dec->fmt_out.audio.i_channels = 2;
    p_dec->fmt_out.audio.i_original_channels =
    p_dec->fmt_out.audio.i_physical_channels =
        AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT;
    if (HAVE_FPU)
    {
        p_dec->fmt_out.i_codec = VLC_CODEC_FL32;
        p_dec->fmt_out.audio.i_bitspersample = 32;
        p_sys->fixed = false;
    }
    else
    {
        p_dec->fmt_out.i_codec = VLC_CODEC_S16N;
        p_dec->fmt_out.audio.i_bitspersample = 16;
        p_sys->fixed = true;
    }
    date_Init (&p_sys->end_date, p_dec->fmt_out.audio.i_rate, 1);
    date_Set (&p_sys->end_date, 0);

    p_dec->p_sys = p_sys;
    p_dec->pf_decode_audio = DecodeBlock;
    return VLC_SUCCESS;
}


static void Close (vlc_object_t *p_this)
{
    decoder_sys_t *p_sys = ((decoder_t *)p_this)->p_sys;

    fluid_synth_sfunload (p_sys->synth, p_sys->soundfont, 1);
    delete_fluid_synth (p_sys->synth);
    delete_fluid_settings (p_sys->settings);
    free (p_sys);
}


static aout_buffer_t *DecodeBlock (decoder_t *p_dec, block_t **pp_block)
{
    block_t *p_block;
    decoder_sys_t *p_sys = p_dec->p_sys;
    aout_buffer_t *p_out = NULL;

    if (pp_block == NULL)
        return NULL;
    p_block = *pp_block;
    if (p_block == NULL)
        return NULL;
    *pp_block = NULL;

    if (p_block->i_pts > VLC_TS_INVALID && !date_Get (&p_sys->end_date))
        date_Set (&p_sys->end_date, p_block->i_pts);
    else
    if (p_block->i_pts < date_Get (&p_sys->end_date))
    {
        msg_Warn (p_dec, "MIDI message in the past?");
        goto drop;
    }

    if (p_block->i_buffer < 1)
        goto drop;

    uint8_t event = p_block->p_buffer[0];
    uint8_t channel = p_block->p_buffer[0] & 0xf;
    event &= 0xF0;

    if (event == 0xF0)
        switch (channel)
        {
            case 0:
                if (p_block->p_buffer[p_block->i_buffer - 1] != 0xF7)
                {
            case 7:
                    msg_Warn (p_dec, "fragmented SysEx not implemented");
                    goto drop;
                }
                fluid_synth_sysex (p_sys->synth, (char *)p_block->p_buffer + 1,
                                   p_block->i_buffer - 2, NULL, NULL, NULL, 0);
                break;
            case 0xF:
                fluid_synth_system_reset (p_sys->synth);
                break;
        }

    uint8_t p1 = (p_block->i_buffer > 1) ? (p_block->p_buffer[1] & 0x7f) : 0;
    uint8_t p2 = (p_block->i_buffer > 2) ? (p_block->p_buffer[2] & 0x7f) : 0;

    switch (event & 0xF0)
    {
        case 0x80:
            fluid_synth_noteoff (p_sys->synth, channel, p1);
            break;
        case 0x90:
            fluid_synth_noteon (p_sys->synth, channel, p1, p2);
            break;
        /*case 0xA0: note aftertouch not implemented */
        case 0xB0:
            fluid_synth_cc (p_sys->synth, channel, p1, p2);
            break;
        case 0xC0:
            fluid_synth_program_change (p_sys->synth, channel, p1);
            break;
        case 0xD0:
            fluid_synth_channel_pressure (p_sys->synth, channel, p1);
            break;
        case 0xE0:
            fluid_synth_pitch_bend (p_sys->synth, channel, (p2 << 7) | p1);
            break;
    }

    unsigned samples =
        (p_block->i_pts - date_Get (&p_sys->end_date)) * 441 / 10000;
    if (samples == 0)
        goto drop;

    p_out = decoder_NewAudioBuffer (p_dec, samples);
    if (p_out == NULL)
        goto drop;

    p_out->i_pts = date_Get (&p_sys->end_date );
    p_out->i_length = date_Increment (&p_sys->end_date, samples)
                      - p_out->i_pts;
    if (!p_sys->fixed)
        fluid_synth_write_float (p_sys->synth, samples,
                                 p_out->p_buffer, 0, 2,
                                 p_out->p_buffer, 1, 2);
    else
        fluid_synth_write_s16 (p_sys->synth, samples,
                               (int16_t *)p_out->p_buffer, 0, 2,
                               (int16_t *)p_out->p_buffer, 1, 2);
drop:
    block_Release (p_block);
    return p_out;
}
