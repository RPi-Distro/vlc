/*****************************************************************************
 * file.c : audio output which writes the samples to a file
 *****************************************************************************
 * Copyright (C) 2002 the VideoLAN team
 * $Id: 3cbfe2ef1d7a83f8915e8691a8709a6d1d963e00 $
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
 *          Gildas Bazin <gbazin@netcourrier.com>
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
#include <vlc_aout.h>
#include <vlc_codecs.h> /* WAVEHEADER */
#include <vlc_fs.h>

#define A52_FRAME_NB 1536

/*****************************************************************************
 * aout_sys_t: audio output method descriptor
 *****************************************************************************
 * This structure is part of the audio output thread descriptor.
 * It describes the direct sound specific properties of an audio device.
 *****************************************************************************/
struct aout_sys_t
{
    FILE     * p_file;
    bool b_add_wav_header;

    WAVEHEADER waveh;                      /* Wave header of the output file */
};

#define CHANNELS_MAX 6
static const int pi_channels_maps[CHANNELS_MAX+1] =
{
    0,
    AOUT_CHAN_CENTER,
    AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT,
    AOUT_CHAN_CENTER | AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT,
    AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_REARLEFT
     | AOUT_CHAN_REARRIGHT,
    AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER
     | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT,
    AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER
     | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT | AOUT_CHAN_LFE
};

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
static int     Open        ( vlc_object_t * );
static void    Close       ( vlc_object_t * );
static void    Play        ( audio_output_t *, block_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define FORMAT_TEXT N_("Output format")

#define CHANNELS_TEXT N_("Number of output channels")
#define CHANNELS_LONGTEXT N_("By default (0), all the channels of the incoming " \
    "will be saved but you can restrict the number of channels here.")

#define WAV_TEXT N_("Add WAVE header")
#define WAV_LONGTEXT N_("Instead of writing a raw file, you can add a WAV " \
                        "header to the file.")

static const char *const format_list[] = { "u8", "s8", "u16", "s16", "u16_le",
                                     "s16_le", "u16_be", "s16_be", "fixed32",
                                     "float32", "spdif" };
static const int format_int[] = { VLC_CODEC_U8,
                                  VLC_CODEC_S8,
                                  VLC_CODEC_U16N, VLC_CODEC_S16N,
                                  VLC_CODEC_U16L,
                                  VLC_CODEC_S16L,
                                  VLC_CODEC_U16B,
                                  VLC_CODEC_S16B,
                                  VLC_CODEC_FI32,
                                  VLC_CODEC_F32L,
                                  VLC_CODEC_SPDIFL };

#define FILE_TEXT N_("Output file")
#define FILE_LONGTEXT N_("File to which the audio samples will be written to. (\"-\" for stdout")

vlc_module_begin ()
    set_description( N_("File audio output") )
    set_shortname( N_("File") )
    set_category( CAT_AUDIO )
    set_subcategory( SUBCAT_AUDIO_AOUT )

    add_savefile( "audiofile-file", "audiofile.wav", FILE_TEXT,
                  FILE_LONGTEXT, false )
    add_string( "audiofile-format", "s16",
                FORMAT_TEXT, FORMAT_TEXT, true )
        change_string_list( format_list, 0, 0 )
    add_integer( "audiofile-channels", 0,
                 CHANNELS_TEXT, CHANNELS_LONGTEXT, true )
        change_integer_range( 0, 6 )
    add_bool( "audiofile-wav", true, WAV_TEXT, WAV_LONGTEXT, true )

    set_capability( "audio output", 0 )
    add_shortcut( "file", "audiofile" )
    set_callbacks( Open, Close )
vlc_module_end ()

/*****************************************************************************
 * Open: open a dummy audio device
 *****************************************************************************/
static int Open( vlc_object_t * p_this )
{
    audio_output_t * p_aout = (audio_output_t *)p_this;
    char * psz_name, * psz_format;
    const char * const * ppsz_compare = format_list;
    int i_channels, i = 0;

    psz_name = var_CreateGetString( p_this, "audiofile-file" );
    if( !psz_name || !*psz_name )
    {
        msg_Err( p_aout, "you need to specify an output file name" );
        free( psz_name );
        return VLC_EGENERIC;
    }

    /* Allocate structure */
    p_aout->sys = malloc( sizeof( aout_sys_t ) );
    if( p_aout->sys == NULL )
        return VLC_ENOMEM;

    if( !strcmp( psz_name, "-" ) )
        p_aout->sys->p_file = stdout;
    else
        p_aout->sys->p_file = vlc_fopen( psz_name, "wb" );

    free( psz_name );
    if ( p_aout->sys->p_file == NULL )
    {
        free( p_aout->sys );
        return VLC_EGENERIC;
    }

    p_aout->pf_play = Play;
    p_aout->pf_pause = NULL;
    p_aout->pf_flush = NULL;

    /* Audio format */
    psz_format = var_CreateGetString( p_this, "audiofile-format" );

    while ( *ppsz_compare != NULL )
    {
        if ( !strncmp( *ppsz_compare, psz_format, strlen(*ppsz_compare) ) )
        {
            break;
        }
        ppsz_compare++; i++;
    }

    if ( *ppsz_compare == NULL )
    {
        msg_Err( p_aout, "cannot understand the format string (%s)",
                 psz_format );
        if( p_aout->sys->p_file != stdout )
            fclose( p_aout->sys->p_file );
        free( p_aout->sys );
        free( psz_format );
        return VLC_EGENERIC;
    }
    free( psz_format );

    p_aout->format.i_format = format_int[i];
    if ( AOUT_FMT_SPDIF( &p_aout->format ) )
    {
        p_aout->format.i_bytes_per_frame = AOUT_SPDIF_SIZE;
        p_aout->format.i_frame_length = A52_FRAME_NB;
    }
    aout_VolumeNoneInit( p_aout );

    /* Channels number */
    i_channels = var_CreateGetInteger( p_this, "audiofile-channels" );

    if( i_channels > 0 && i_channels <= CHANNELS_MAX )
    {
        p_aout->format.i_physical_channels =
            pi_channels_maps[i_channels];
    }

    /* WAV header */
    p_aout->sys->b_add_wav_header = var_CreateGetBool( p_this,
                                                        "audiofile-wav" );

    if( p_aout->sys->b_add_wav_header )
    {
        /* Write wave header */
        WAVEHEADER *wh = &p_aout->sys->waveh;

        memset( wh, 0, sizeof(*wh) );

        switch( p_aout->format.i_format )
        {
        case VLC_CODEC_F32L:
            wh->Format     = WAVE_FORMAT_IEEE_FLOAT;
            wh->BitsPerSample = sizeof(float) * 8;
            break;
        case VLC_CODEC_U8:
            wh->Format     = WAVE_FORMAT_PCM;
            wh->BitsPerSample = 8;
            break;
        case VLC_CODEC_S16L:
        default:
            wh->Format     = WAVE_FORMAT_PCM;
            wh->BitsPerSample = 16;
            break;
        }

        wh->MainChunkID = VLC_FOURCC('R', 'I', 'F', 'F');
        wh->Length = 0;                    /* temp, to be filled in as we go */
        wh->ChunkTypeID = VLC_FOURCC('W', 'A', 'V', 'E');
        wh->SubChunkID = VLC_FOURCC('f', 'm', 't', ' ');
        wh->SubChunkLength = 16;

        wh->Modus = aout_FormatNbChannels( &p_aout->format );
        wh->SampleFreq = p_aout->format.i_rate;
        wh->BytesPerSample = wh->Modus * ( wh->BitsPerSample / 8 );
        wh->BytesPerSec = wh->BytesPerSample * wh->SampleFreq;

        wh->DataChunkID = VLC_FOURCC('d', 'a', 't', 'a');
        wh->DataLength = 0;                /* temp, to be filled in as we go */

        /* Header -> little endian format */
        SetWLE( &wh->Format, wh->Format );
        SetWLE( &wh->BitsPerSample, wh->BitsPerSample );
        SetDWLE( &wh->SubChunkLength, wh->SubChunkLength );
        SetWLE( &wh->Modus, wh->Modus );
        SetDWLE( &wh->SampleFreq, wh->SampleFreq );
        SetWLE( &wh->BytesPerSample, wh->BytesPerSample );
        SetDWLE( &wh->BytesPerSec, wh->BytesPerSec );

        if( fwrite( wh, sizeof(WAVEHEADER), 1,
                    p_aout->sys->p_file ) != 1 )
        {
            msg_Err( p_aout, "write error (%m)" );
        }
    }

    return 0;
}

/*****************************************************************************
 * Close: close our file
 *****************************************************************************/
static void Close( vlc_object_t * p_this )
{
    audio_output_t * p_aout = (audio_output_t *)p_this;

    msg_Dbg( p_aout, "closing audio file" );

    if( p_aout->sys->b_add_wav_header )
    {
        /* Update Wave Header */
        p_aout->sys->waveh.Length =
            p_aout->sys->waveh.DataLength + sizeof(WAVEHEADER) - 4;

        /* Write Wave Header */
        if( fseek( p_aout->sys->p_file, 0, SEEK_SET ) )
        {
            msg_Err( p_aout, "seek error (%m)" );
        }

        /* Header -> little endian format */
        SetDWLE( &p_aout->sys->waveh.Length,
                 p_aout->sys->waveh.Length );
        SetDWLE( &p_aout->sys->waveh.DataLength,
                 p_aout->sys->waveh.DataLength );

        if( fwrite( &p_aout->sys->waveh, sizeof(WAVEHEADER), 1,
                    p_aout->sys->p_file ) != 1 )
        {
            msg_Err( p_aout, "write error (%m)" );
        }
    }

    if( p_aout->sys->p_file != stdout )
        fclose( p_aout->sys->p_file );
    free( p_aout->sys );
}

/*****************************************************************************
 * Play: pretend to play a sound
 *****************************************************************************/
static void Play( audio_output_t * p_aout, block_t *p_buffer )
{
    if( fwrite( p_buffer->p_buffer, p_buffer->i_buffer, 1,
                p_aout->sys->p_file ) != 1 )
    {
        msg_Err( p_aout, "write error (%m)" );
    }

    if( p_aout->sys->b_add_wav_header )
    {
        /* Update Wave Header */
        p_aout->sys->waveh.DataLength += p_buffer->i_buffer;
    }

    aout_BufferFree( p_buffer );
}
