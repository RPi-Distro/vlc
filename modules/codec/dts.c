/*****************************************************************************
 * dts.c: parse DTS audio sync info and packetize the stream
 *****************************************************************************
 * Copyright (C) 2003-2009 the VideoLAN team
 * $Id: a2325546e615528892799e5501ac789f0af8688d $
 *
 * Authors: Jon Lech Johansen <jon-vl@nanocrew.net>
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
#include <assert.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_codec.h>
#include <vlc_aout.h>
#include <vlc_block_helper.h>
#include <vlc_bits.h>

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  OpenDecoder   ( vlc_object_t * );
static int  OpenPacketizer( vlc_object_t * );
static void CloseCommon   ( vlc_object_t * );

vlc_module_begin ()
    set_description( N_("DTS parser") )
    set_capability( "decoder", 100 )
    set_callbacks( OpenDecoder, CloseCommon )

    add_submodule ()
    set_description( N_("DTS audio packetizer") )
    set_capability( "packetizer", 10 )
    set_callbacks( OpenPacketizer, CloseCommon )
vlc_module_end ()

/*****************************************************************************
 * decoder_sys_t : decoder descriptor
 *****************************************************************************/
struct decoder_sys_t
{
    /* Module mode */
    bool b_packetizer;

    /*
     * Input properties
     */
    int i_state;

    block_bytestream_t bytestream;

    /*
     * Common properties
     */
    date_t  end_date;

    mtime_t i_pts;

    bool         b_dts_hd;  /* Is the current frame a DTS HD one */
    unsigned int i_bit_rate;
    unsigned int i_frame_size;
    unsigned int i_frame_length;
    unsigned int i_rate;
    unsigned int i_channels;
    unsigned int i_channels_conf;
};

enum {

    STATE_NOSYNC,
    STATE_SYNC,
    STATE_HEADER,
    STATE_NEXT_SYNC,
    STATE_GET_DATA,
    STATE_SEND_DATA
};

#define DTS_HEADER_SIZE 14

/****************************************************************************
 * Local prototypes
 ****************************************************************************/
static int OpenCommon( vlc_object_t *, bool b_packetizer );
static void *DecodeBlock( decoder_t *, block_t ** );

static inline int SyncCode( const uint8_t * );
static int  SyncInfo( const uint8_t *, bool *, unsigned int *, unsigned int *,
                      unsigned int *, unsigned int *, unsigned int * );

static uint8_t       *GetOutBuffer ( decoder_t *, block_t ** );
static aout_buffer_t *GetAoutBuffer( decoder_t * );
static block_t       *GetSoutBuffer( decoder_t * );

/*****************************************************************************
 * OpenDecoder: probe the decoder
 *****************************************************************************/
static int OpenDecoder( vlc_object_t *p_this )
{
    /* HACK: Don't use this codec if we don't have an dts audio filter */
    if( !module_exists( "dtstofloat32" ) )
        return VLC_EGENERIC;

    return OpenCommon( p_this, false );
}

/*****************************************************************************
 * OpenPacketizer: probe the packetizer
 *****************************************************************************/
static int OpenPacketizer( vlc_object_t *p_this )
{
    return OpenCommon( p_this, true );
}

/*****************************************************************************
 * OpenCommon:
 *****************************************************************************/
static int OpenCommon( vlc_object_t *p_this, bool b_packetizer )
{
    decoder_t *p_dec = (decoder_t*)p_this;
    decoder_sys_t *p_sys;

    if( p_dec->fmt_in.i_codec != VLC_CODEC_DTS )
        return VLC_EGENERIC;

    /* Allocate the memory needed to store the decoder's structure */
    if( ( p_dec->p_sys = p_sys = malloc(sizeof(*p_sys)) ) == NULL )
        return VLC_ENOMEM;

    /* Misc init */
    p_sys->b_packetizer = b_packetizer;
    p_sys->i_state = STATE_NOSYNC;
    date_Set( &p_sys->end_date, 0 );
    p_sys->b_dts_hd = false;
    p_sys->i_pts = VLC_TS_INVALID;

    p_sys->bytestream = block_BytestreamInit();

    /* Set output properties */
    p_dec->fmt_out.i_cat = AUDIO_ES;
    p_dec->fmt_out.i_codec = VLC_CODEC_DTS;
    p_dec->fmt_out.audio.i_rate = 0; /* So end_date gets initialized */

    /* Set callback */
    p_dec->pf_decode_audio = (aout_buffer_t *(*)(decoder_t *, block_t **))
        DecodeBlock;
    p_dec->pf_packetize    = (block_t *(*)(decoder_t *, block_t **))
        DecodeBlock;

    return VLC_SUCCESS;
}

/****************************************************************************
 * DecodeBlock: the whole thing
 ****************************************************************************/
static void *DecodeBlock( decoder_t *p_dec, block_t **pp_block )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    uint8_t p_header[DTS_HEADER_SIZE];
    uint8_t *p_buf;
    block_t *p_out_buffer;

    if( !pp_block || !*pp_block )
        return NULL;

    if( (*pp_block)->i_flags&(BLOCK_FLAG_DISCONTINUITY|BLOCK_FLAG_CORRUPTED) )
    {
        if( (*pp_block)->i_flags&BLOCK_FLAG_CORRUPTED )
        {
            p_sys->i_state = STATE_NOSYNC;
            block_BytestreamEmpty( &p_sys->bytestream );
        }
        date_Set( &p_sys->end_date, 0 );
        block_Release( *pp_block );
        return NULL;
    }

    if( !date_Get( &p_sys->end_date ) && (*pp_block)->i_pts <= VLC_TS_INVALID )
    {
        /* We've just started the stream, wait for the first PTS. */
        block_Release( *pp_block );
        return NULL;
    }

    block_BytestreamPush( &p_sys->bytestream, *pp_block );

    while( 1 )
    {
        switch( p_sys->i_state )
        {
        case STATE_NOSYNC:
            /* Look for sync code - should be 0x7ffe8001 */
            while( block_PeekBytes( &p_sys->bytestream, p_header, 6 )
                   == VLC_SUCCESS )
            {
                if( SyncCode( p_header ) == VLC_SUCCESS )
                {
                    p_sys->i_state = STATE_SYNC;
                    break;
                }
                block_SkipByte( &p_sys->bytestream );
            }
            if( p_sys->i_state != STATE_SYNC )
            {
                block_BytestreamFlush( &p_sys->bytestream );

                /* Need more data */
                return NULL;
            }

        case STATE_SYNC:
            /* New frame, set the Presentation Time Stamp */
            p_sys->i_pts = p_sys->bytestream.p_block->i_pts;
            if( p_sys->i_pts > VLC_TS_INVALID &&
                p_sys->i_pts != date_Get( &p_sys->end_date ) )
            {
                date_Set( &p_sys->end_date, p_sys->i_pts );
            }
            p_sys->i_state = STATE_HEADER;

        case STATE_HEADER:
            /* Get DTS frame header (DTS_HEADER_SIZE bytes) */
            if( block_PeekBytes( &p_sys->bytestream, p_header,
                                 DTS_HEADER_SIZE ) != VLC_SUCCESS )
            {
                /* Need more data */
                return NULL;
            }

            /* Check if frame is valid and get frame info */
            p_sys->i_frame_size = SyncInfo( p_header,
                                            &p_sys->b_dts_hd,
                                            &p_sys->i_channels,
                                            &p_sys->i_channels_conf,
                                            &p_sys->i_rate,
                                            &p_sys->i_bit_rate,
                                            &p_sys->i_frame_length );
            if( !p_sys->i_frame_size )
            {
                msg_Dbg( p_dec, "emulated sync word" );
                block_SkipByte( &p_sys->bytestream );
                p_sys->i_state = STATE_NOSYNC;
                break;
            }
            p_sys->i_state = STATE_NEXT_SYNC;

        case STATE_NEXT_SYNC:
            /* TODO: If pp_block == NULL, flush the buffer without checking the
             * next sync word */

            /* Check if next expected frame contains the sync word */
            if( block_PeekOffsetBytes( &p_sys->bytestream,
                                       p_sys->i_frame_size, p_header, 6 )
                != VLC_SUCCESS )
            {
                /* Need more data */
                return NULL;
            }

            if( p_sys->b_packetizer &&
                p_header[0] == 0 && p_header[1] == 0 )
            {
                /* DTS wav files and audio CD's use stuffing */
                p_sys->i_state = STATE_SEND_DATA;
                break;
            }

            if( SyncCode( p_header ) != VLC_SUCCESS )
            {
                msg_Dbg( p_dec, "emulated sync word "
                         "(no sync on following frame): %2.2x%2.2x%2.2x%2.2x",
                         (int)p_header[0], (int)p_header[1],
                         (int)p_header[2], (int)p_header[3] );
                p_sys->i_state = STATE_NOSYNC;
                block_SkipByte( &p_sys->bytestream );
                break;
            }
            p_sys->i_state = STATE_SEND_DATA;
            break;

        case STATE_GET_DATA:
            /* Make sure we have enough data.
             * (Not useful if we went through NEXT_SYNC) */
            if( block_WaitBytes( &p_sys->bytestream,
                                 p_sys->i_frame_size ) != VLC_SUCCESS )
            {
                /* Need more data */
                return NULL;
            }
            p_sys->i_state = STATE_SEND_DATA;

        case STATE_SEND_DATA:
            if( p_sys->b_dts_hd  )
            {
                /* Ignore DTS-HD */
                block_SkipBytes( &p_sys->bytestream, p_sys->i_frame_size );
                p_sys->i_state = STATE_NOSYNC;
                break;
            }

            if( !(p_buf = GetOutBuffer( p_dec, &p_out_buffer )) )
            {
                //p_dec->b_error = true;
                return NULL;
            }

            /* Copy the whole frame into the buffer. When we reach this point
             * we already know we have enough data available. */
            block_GetBytes( &p_sys->bytestream,
                            p_buf, __MIN( p_sys->i_frame_size, p_out_buffer->i_buffer ) );

            /* Make sure we don't reuse the same pts twice */
            if( p_sys->i_pts == p_sys->bytestream.p_block->i_pts )
                p_sys->i_pts = p_sys->bytestream.p_block->i_pts = VLC_TS_INVALID;

            p_sys->i_state = STATE_NOSYNC;

            /* So p_block doesn't get re-added several times */
            *pp_block = block_BytestreamPop( &p_sys->bytestream );

            return p_out_buffer;
        }
    }

    return NULL;
}

/*****************************************************************************
 * CloseCommon: clean up the decoder
 *****************************************************************************/
static void CloseCommon( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t*)p_this;
    decoder_sys_t *p_sys = p_dec->p_sys;

    block_BytestreamRelease( &p_sys->bytestream );

    free( p_sys );
}

/*****************************************************************************
 * GetOutBuffer:
 *****************************************************************************/
static uint8_t *GetOutBuffer( decoder_t *p_dec, block_t **pp_out_buffer )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    uint8_t *p_buf;

    if( p_dec->fmt_out.audio.i_rate != p_sys->i_rate )
    {
        msg_Info( p_dec, "DTS channels:%d samplerate:%d bitrate:%d",
                  p_sys->i_channels, p_sys->i_rate, p_sys->i_bit_rate );

        date_Init( &p_sys->end_date, p_sys->i_rate, 1 );
        date_Set( &p_sys->end_date, p_sys->i_pts );
    }

    p_dec->fmt_out.audio.i_rate     = p_sys->i_rate;
    p_dec->fmt_out.audio.i_channels = p_sys->i_channels;
    /* Hack for DTS S/PDIF filter which needs to pad the DTS frames */
    p_dec->fmt_out.audio.i_bytes_per_frame =
        __MAX( p_sys->i_frame_size, p_sys->i_frame_length * 4 );
    p_dec->fmt_out.audio.i_frame_length = p_sys->i_frame_length;

    p_dec->fmt_out.audio.i_original_channels = p_sys->i_channels_conf;
    p_dec->fmt_out.audio.i_physical_channels =
        p_sys->i_channels_conf & AOUT_CHAN_PHYSMASK;

    p_dec->fmt_out.i_bitrate = p_sys->i_bit_rate;

    if( p_sys->b_packetizer )
    {
        block_t *p_sout_buffer = GetSoutBuffer( p_dec );
        p_buf = p_sout_buffer ? p_sout_buffer->p_buffer : NULL;
        *pp_out_buffer = p_sout_buffer;
    }
    else
    {
        aout_buffer_t *p_aout_buffer = GetAoutBuffer( p_dec );
        p_buf = p_aout_buffer ? p_aout_buffer->p_buffer : NULL;
        *pp_out_buffer = p_aout_buffer;
    }

    return p_buf;
}

/*****************************************************************************
 * GetAoutBuffer:
 *****************************************************************************/
static aout_buffer_t *GetAoutBuffer( decoder_t *p_dec )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    aout_buffer_t *p_buf;

    /* Hack for DTS S/PDIF filter which needs to send 3 frames at a time
     * (plus a few header bytes) */
    p_buf = decoder_NewAudioBuffer( p_dec, p_sys->i_frame_length * 4 );
    if( p_buf == NULL ) return NULL;
    p_buf->i_nb_samples = p_sys->i_frame_length;
    p_buf->i_buffer = p_sys->i_frame_size;

    p_buf->i_pts = date_Get( &p_sys->end_date );
    p_buf->i_length = date_Increment( &p_sys->end_date, p_sys->i_frame_length )
                      - p_buf->i_pts;

    return p_buf;
}

/*****************************************************************************
 * GetSoutBuffer:
 *****************************************************************************/
static block_t *GetSoutBuffer( decoder_t *p_dec )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    block_t *p_block;

    p_block = block_New( p_dec, p_sys->i_frame_size );
    if( p_block == NULL ) return NULL;

    p_block->i_pts = p_block->i_dts = date_Get( &p_sys->end_date );

    p_block->i_length = date_Increment( &p_sys->end_date,
        p_sys->i_frame_length ) - p_block->i_pts;

    return p_block;
}

/*****************************************************************************
 * SyncInfo: parse DTS sync info
 *****************************************************************************/
static const unsigned int ppi_dts_samplerate[] =
{
    0, 8000, 16000, 32000, 0, 0, 11025, 22050, 44100, 0, 0,
    12000, 24000, 48000, 96000, 192000
};

static const unsigned int ppi_dts_bitrate[] =
{
    32000, 56000, 64000, 96000, 112000, 128000,
    192000, 224000, 256000, 320000, 384000,
    448000, 512000, 576000, 640000, 768000,
    896000, 1024000, 1152000, 1280000, 1344000,
    1408000, 1411200, 1472000, 1536000, 1920000,
    2048000, 3072000, 3840000, 1/*open*/, 2/*variable*/, 3/*lossless*/
};

static int SyncInfo16be( const uint8_t *p_buf,
                         unsigned int *pi_audio_mode,
                         unsigned int *pi_sample_rate,
                         unsigned int *pi_bit_rate,
                         unsigned int *pi_frame_length )
{
    unsigned int i_frame_size;
    unsigned int i_lfe;

    *pi_frame_length = (p_buf[4] & 0x01) << 6 | (p_buf[5] >> 2);
    i_frame_size = (p_buf[5] & 0x03) << 12 | (p_buf[6] << 4) |
                   (p_buf[7] >> 4);

    *pi_audio_mode = (p_buf[7] & 0x0f) << 2 | (p_buf[8] >> 6);
    *pi_sample_rate = (p_buf[8] >> 2) & 0x0f;
    *pi_bit_rate = (p_buf[8] & 0x03) << 3 | ((p_buf[9] >> 5) & 0x07);

    i_lfe = (p_buf[10] >> 1) & 0x03;
    if( i_lfe ) *pi_audio_mode |= 0x10000;

    return i_frame_size + 1;
}

static void BufLeToBe( uint8_t *p_out, const uint8_t *p_in, int i_in )
{
    int i;

    for( i = 0; i < i_in/2; i++  )
    {
        p_out[i*2] = p_in[i*2+1];
        p_out[i*2+1] = p_in[i*2];
    }
}

static int Buf14To16( uint8_t *p_out, const uint8_t *p_in, int i_in, int i_le )
{
    unsigned char tmp, cur = 0;
    int bits_in, bits_out = 0;
    int i, i_out = 0;

    for( i = 0; i < i_in; i++  )
    {
        if( i%2 )
        {
            tmp = p_in[i-i_le];
            bits_in = 8;
        }
        else
        {
            tmp = p_in[i+i_le] & 0x3F;
            bits_in = 8 - 2;
        }

        if( bits_out < 8 )
        {
            int need = __MIN( 8 - bits_out, bits_in );
            cur <<= need;
            cur |= ( tmp >> (bits_in - need) );
            tmp <<= (8 - bits_in + need);
            tmp >>= (8 - bits_in + need);
            bits_in -= need;
            bits_out += need;
        }

        if( bits_out == 8 )
        {
            p_out[i_out] = cur;
            cur = 0;
            bits_out = 0;
            i_out++;
        }

        bits_out += bits_in;
        cur <<= bits_in;
        cur |= tmp;
    }

    return i_out;
}

static inline int SyncCode( const uint8_t *p_buf )
{
    /* 14 bits, little endian version of the bitstream */
    if( p_buf[0] == 0xff && p_buf[1] == 0x1f &&
        p_buf[2] == 0x00 && p_buf[3] == 0xe8 &&
        (p_buf[4] & 0xf0) == 0xf0 && p_buf[5] == 0x07 )
    {
        return VLC_SUCCESS;
    }
    /* 14 bits, big endian version of the bitstream */
    else if( p_buf[0] == 0x1f && p_buf[1] == 0xff &&
             p_buf[2] == 0xe8 && p_buf[3] == 0x00 &&
             p_buf[4] == 0x07 && (p_buf[5] & 0xf0) == 0xf0 )
    {
        return VLC_SUCCESS;
    }
    /* 16 bits, big endian version of the bitstream */
    else if( p_buf[0] == 0x7f && p_buf[1] == 0xfe &&
             p_buf[2] == 0x80 && p_buf[3] == 0x01 )
    {
        return VLC_SUCCESS;
    }
    /* 16 bits, little endian version of the bitstream */
    else if( p_buf[0] == 0xfe && p_buf[1] == 0x7f &&
             p_buf[2] == 0x01 && p_buf[3] == 0x80 )
    {
        return VLC_SUCCESS;
    }
    /* DTS-HD */
    else if( p_buf[0] == 0x64 && p_buf[1] ==  0x58 &&
             p_buf[2] == 0x20 && p_buf[3] ==  0x25 )
    {
        return VLC_SUCCESS;
    }

    return VLC_EGENERIC;
}

static int SyncInfo( const uint8_t *p_buf,
                     bool *pb_dts_hd,
                     unsigned int *pi_channels,
                     unsigned int *pi_channels_conf,
                     unsigned int *pi_sample_rate,
                     unsigned int *pi_bit_rate,
                     unsigned int *pi_frame_length )
{
    unsigned int i_audio_mode;
    unsigned int i_frame_size;

    /* 14 bits, little endian version of the bitstream */
    if( p_buf[0] == 0xff && p_buf[1] == 0x1f &&
        p_buf[2] == 0x00 && p_buf[3] == 0xe8 &&
        (p_buf[4] & 0xf0) == 0xf0 && p_buf[5] == 0x07 )
    {
        uint8_t conv_buf[DTS_HEADER_SIZE];
        Buf14To16( conv_buf, p_buf, DTS_HEADER_SIZE, 1 );
        i_frame_size = SyncInfo16be( conv_buf, &i_audio_mode, pi_sample_rate,
                                     pi_bit_rate, pi_frame_length );
        i_frame_size = i_frame_size * 8 / 14 * 2;
    }
    /* 14 bits, big endian version of the bitstream */
    else if( p_buf[0] == 0x1f && p_buf[1] == 0xff &&
             p_buf[2] == 0xe8 && p_buf[3] == 0x00 &&
             p_buf[4] == 0x07 && (p_buf[5] & 0xf0) == 0xf0 )
    {
        uint8_t conv_buf[DTS_HEADER_SIZE];
        Buf14To16( conv_buf, p_buf, DTS_HEADER_SIZE, 0 );
        i_frame_size = SyncInfo16be( conv_buf, &i_audio_mode, pi_sample_rate,
                                     pi_bit_rate, pi_frame_length );
        i_frame_size = i_frame_size * 8 / 14 * 2;
    }
    /* 16 bits, big endian version of the bitstream */
    else if( p_buf[0] == 0x7f && p_buf[1] == 0xfe &&
             p_buf[2] == 0x80 && p_buf[3] == 0x01 )
    {
        i_frame_size = SyncInfo16be( p_buf, &i_audio_mode, pi_sample_rate,
                                     pi_bit_rate, pi_frame_length );
    }
    /* 16 bits, little endian version of the bitstream */
    else if( p_buf[0] == 0xfe && p_buf[1] == 0x7f &&
             p_buf[2] == 0x01 && p_buf[3] == 0x80 )
    {
        uint8_t conv_buf[DTS_HEADER_SIZE];
        BufLeToBe( conv_buf, p_buf, DTS_HEADER_SIZE );
        i_frame_size = SyncInfo16be( p_buf, &i_audio_mode, pi_sample_rate,
                                     pi_bit_rate, pi_frame_length );
    }
    /* DTS-HD */
    else
    {
        assert( p_buf[0] == 0x64 && p_buf[1] ==  0x58 &&
                p_buf[2] == 0x20 && p_buf[3] ==  0x25 );

        int i_dts_hd_size;
        bs_t s;
        bs_init( &s, &p_buf[4], DTS_HEADER_SIZE - 4 );

        bs_skip( &s, 8 + 2 );

        if( bs_read1( &s ) )
        {
            bs_skip( &s, 12 );
            i_dts_hd_size = bs_read( &s, 20 ) + 1;
        }
        else
        {
            bs_skip( &s, 8 );
            i_dts_hd_size = bs_read( &s, 16 ) + 1;
        }
        //uint16_t s0 = bs_read( &s, 16 );
        //uint16_t s1 = bs_read( &s, 16 );
        //fprintf( stderr, "DTS HD=%d : %x %x\n", i_dts_hd_size, s0, s1 );

        *pb_dts_hd = true;
        /* As we ignore the stream, do not modify those variables:
        *pi_channels = ;
        *pi_channels_conf = ;
        *pi_sample_rate = ;
        *pi_bit_rate = ;
        *pi_frame_length = ;
        */
        return i_dts_hd_size;
    }

    *pb_dts_hd = false;

    switch( i_audio_mode & 0xFFFF )
    {
        case 0x0:
            /* Mono */
            *pi_channels = 1;
            *pi_channels_conf = AOUT_CHAN_CENTER;
            break;
        case 0x1:
            /* Dual-mono = stereo + dual-mono */
            *pi_channels = 2;
            *pi_channels_conf = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT |
                           AOUT_CHAN_DUALMONO;
            break;
        case 0x2:
        case 0x3:
        case 0x4:
            /* Stereo */
            *pi_channels = 2;
            *pi_channels_conf = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT;
            break;
        case 0x5:
            /* 3F */
            *pi_channels = 3;
            *pi_channels_conf = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT |
                                AOUT_CHAN_CENTER;
            break;
        case 0x6:
            /* 2F/1R */
            *pi_channels = 3;
            *pi_channels_conf = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT |
                                AOUT_CHAN_REARCENTER;
            break;
        case 0x7:
            /* 3F/1R */
            *pi_channels = 4;
            *pi_channels_conf = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT |
                                AOUT_CHAN_CENTER | AOUT_CHAN_REARCENTER;
            break;
        case 0x8:
            /* 2F2R */
            *pi_channels = 4;
            *pi_channels_conf = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT |
                                AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT;
            break;
        case 0x9:
            /* 3F2R */
            *pi_channels = 5;
            *pi_channels_conf = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT |
                                AOUT_CHAN_CENTER | AOUT_CHAN_REARLEFT |
                                AOUT_CHAN_REARRIGHT;
            break;
        case 0xA:
        case 0xB:
            /* 2F2M2R */
            *pi_channels = 6;
            *pi_channels_conf = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT |
                                AOUT_CHAN_MIDDLELEFT | AOUT_CHAN_MIDDLERIGHT |
                                AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT;
            break;
        case 0xC:
            /* 3F2M2R */
            *pi_channels = 7;
            *pi_channels_conf = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT |
                                AOUT_CHAN_CENTER | AOUT_CHAN_MIDDLELEFT |
                                AOUT_CHAN_MIDDLERIGHT | AOUT_CHAN_REARLEFT |
                                AOUT_CHAN_REARRIGHT;
            break;
        case 0xD:
        case 0xE:
            /* 3F2M2R/LFE */
            *pi_channels = 8;
            *pi_channels_conf = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT |
                                AOUT_CHAN_CENTER | AOUT_CHAN_MIDDLELEFT |
                                AOUT_CHAN_MIDDLERIGHT | AOUT_CHAN_REARLEFT |
                                AOUT_CHAN_REARRIGHT | AOUT_CHAN_LFE;
            break;

        default:
            if( i_audio_mode <= 63 )
            {
                /* User defined */
                *pi_channels = 0;
                *pi_channels_conf = 0;
            }
            else return 0;
            break;
    }

    if( i_audio_mode & 0x10000 )
    {
        (*pi_channels)++;
        *pi_channels_conf |= AOUT_CHAN_LFE;
    }

    if( *pi_sample_rate >= sizeof( ppi_dts_samplerate ) /
                           sizeof( ppi_dts_samplerate[0] ) )
    {
        return 0;
    }
    *pi_sample_rate = ppi_dts_samplerate[ *pi_sample_rate ];
    if( !*pi_sample_rate ) return 0;

    if( *pi_bit_rate >= sizeof( ppi_dts_bitrate ) /
                        sizeof( ppi_dts_bitrate[0] ) )
    {
        return 0;
    }
    *pi_bit_rate = ppi_dts_bitrate[ *pi_bit_rate ];
    if( !*pi_bit_rate ) return 0;

    *pi_frame_length = (*pi_frame_length + 1) * 32;

    return i_frame_size;
}
