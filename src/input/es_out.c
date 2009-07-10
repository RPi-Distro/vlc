/*****************************************************************************
 * es_out.c: Es Out handler for input.
 *****************************************************************************
 * Copyright (C) 2003-2004 the VideoLAN team
 * $Id: 040365d21d835555e67d5513d2119fca2bd00920 $
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Jean-Paul Saman <jpsaman #_at_# m2x dot nl>
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

#include <stdio.h>
#include <assert.h>
#include <vlc_common.h>

#include <vlc_input.h>
#include <vlc_es_out.h>
#include <vlc_block.h>
#include <vlc_aout.h>

#include "input_internal.h"
#include "clock.h"
#include "decoder.h"
#include "es_out.h"
#include "event.h"

#include "../stream_output/stream_output.h"

#include <vlc_iso_lang.h>
/* FIXME we should find a better way than including that */
#include "../text/iso-639_def.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
typedef struct
{
    /* Program ID */
    int i_id;

    /* Number of es for this pgrm */
    int i_es;

    bool b_selected;
    bool b_scrambled;

    /* Clock for this program */
    input_clock_t *p_clock;

    char    *psz_name;
    char    *psz_now_playing;
    char    *psz_publisher;

    vlc_epg_t *p_epg;
} es_out_pgrm_t;

struct es_out_id_t
{
    /* ES ID */
    int       i_id;
    es_out_pgrm_t *p_pgrm;

    /* */
    bool b_scrambled;

    /* Channel in the track type */
    int         i_channel;
    es_format_t fmt;
    char        *psz_language;
    char        *psz_language_code;

    decoder_t   *p_dec;
    decoder_t   *p_dec_record;

    /* Fields for Video with CC */
    bool  pb_cc_present[4];
    es_out_id_t  *pp_cc_es[4];

    /* Field for CC track from a master video */
    es_out_id_t *p_master;

    /* ID for the meta data */
    int         i_meta_id;
};

struct es_out_sys_t
{
    input_thread_t *p_input;

    /* */
    vlc_mutex_t   lock;

    /* all programs */
    int           i_pgrm;
    es_out_pgrm_t **pgrm;
    es_out_pgrm_t **pp_selected_pgrm; /* --programs */
    es_out_pgrm_t *p_pgrm;  /* Master program */

    /* all es */
    int         i_id;
    int         i_es;
    es_out_id_t **es;

    /* mode gestion */
    bool  b_active;
    int         i_mode;

    /* es count */
    int         i_audio;
    int         i_video;
    int         i_sub;

    /* es to select */
    int         i_audio_last, i_audio_id;
    int         i_sub_last, i_sub_id;
    int         i_default_sub_id;   /* As specified in container; if applicable */
    char        **ppsz_audio_language;
    char        **ppsz_sub_language;

    /* current main es */
    es_out_id_t *p_es_audio;
    es_out_id_t *p_es_video;
    es_out_id_t *p_es_sub;

    /* delay */
    int64_t i_audio_delay;
    int64_t i_spu_delay;

    /* Clock configuration */
    mtime_t     i_pts_delay;
    int         i_cr_average;
    int         i_rate;

    /* */
    bool        b_paused;
    mtime_t     i_pause_date;

    /* Current preroll */
    mtime_t     i_preroll_end;

    /* Used for buffering */
    bool        b_buffering;
    mtime_t     i_buffering_extra_initial;
    mtime_t     i_buffering_extra_stream;
    mtime_t     i_buffering_extra_system;

    /* Record */
    sout_instance_t *p_sout_record;
};

static es_out_id_t *EsOutAdd    ( es_out_t *, const es_format_t * );
static int          EsOutSend   ( es_out_t *, es_out_id_t *, block_t * );
static void         EsOutDel    ( es_out_t *, es_out_id_t * );
static int          EsOutControl( es_out_t *, int i_query, va_list );
static void         EsOutDelete ( es_out_t * );

static void         EsOutSelect( es_out_t *, es_out_id_t *es, bool b_force );
static void         EsOutUpdateInfo( es_out_t *, es_out_id_t *es, const es_format_t *, const vlc_meta_t * );
static int          EsOutSetRecord(  es_out_t *, bool b_record );

static bool EsIsSelected( es_out_id_t *es );
static void EsSelect( es_out_t *out, es_out_id_t *es );
static void EsUnselect( es_out_t *out, es_out_id_t *es, bool b_update );
static void EsOutDecoderChangeDelay( es_out_t *out, es_out_id_t *p_es );
static void EsOutDecodersChangePause( es_out_t *out, bool b_paused, mtime_t i_date );
static void EsOutProgramChangePause( es_out_t *out, bool b_paused, mtime_t i_date );
static void EsOutProgramsChangeRate( es_out_t *out );
static void EsOutDecodersStopBuffering( es_out_t *out, bool b_forced );
static char *LanguageGetName( const char *psz_code );
static char *LanguageGetCode( const char *psz_lang );
static char **LanguageSplit( const char *psz_langs );
static int LanguageArrayIndex( char **ppsz_langs, char *psz_lang );

static char *EsOutProgramGetMetaName( es_out_pgrm_t *p_pgrm );

static const vlc_fourcc_t EsOutFourccClosedCaptions[4] = {
    VLC_FOURCC('c', 'c', '1', ' '),
    VLC_FOURCC('c', 'c', '2', ' '),
    VLC_FOURCC('c', 'c', '3', ' '),
    VLC_FOURCC('c', 'c', '4', ' '),
};
static inline int EsOutGetClosedCaptionsChannel( vlc_fourcc_t fcc )
{
    int i;
    for( i = 0; i < 4; i++ )
    {
        if( fcc == EsOutFourccClosedCaptions[i] )
            return i;
    }
    return -1;
}
static inline bool EsFmtIsTeletext( const es_format_t *p_fmt )
{
    return p_fmt->i_cat == SPU_ES && p_fmt->i_codec == VLC_FOURCC( 't', 'e', 'l', 'x' );
}

/*****************************************************************************
 * input_EsOutNew:
 *****************************************************************************/
es_out_t *input_EsOutNew( input_thread_t *p_input, int i_rate )
{
    es_out_t     *out = malloc( sizeof( *out ) );
    if( !out )
        return NULL;

    es_out_sys_t *p_sys = malloc( sizeof( *p_sys ) );
    if( !p_sys )
    {
        free( out );
        return NULL;
    }

    out->pf_add     = EsOutAdd;
    out->pf_send    = EsOutSend;
    out->pf_del     = EsOutDel;
    out->pf_control = EsOutControl;
    out->pf_destroy = EsOutDelete;
    out->p_sys      = p_sys;
    out->b_sout     = p_input->p->p_sout != NULL;


    vlc_mutex_init_recursive( &p_sys->lock );
    p_sys->p_input = p_input;

    p_sys->b_active = false;
    p_sys->i_mode   = ES_OUT_MODE_AUTO;


    TAB_INIT( p_sys->i_pgrm, p_sys->pgrm );
    p_sys->p_pgrm   = NULL;

    p_sys->i_id    = 0;

    TAB_INIT( p_sys->i_es, p_sys->es );

    p_sys->i_audio = 0;
    p_sys->i_video = 0;
    p_sys->i_sub   = 0;

    /* */
    p_sys->i_audio_last = var_GetInteger( p_input, "audio-track" );

    p_sys->i_sub_last = var_GetInteger( p_input, "sub-track" );

    p_sys->i_default_sub_id   = -1;

    if( !p_input->b_preparsing )
    {
        char *psz_string;

        psz_string = var_GetString( p_input, "audio-language" );
        p_sys->ppsz_audio_language = LanguageSplit( psz_string );
        if( p_sys->ppsz_audio_language )
        {
            for( int i = 0; p_sys->ppsz_audio_language[i]; i++ )
                msg_Dbg( p_input, "selected audio language[%d] %s",
                         i, p_sys->ppsz_audio_language[i] );
        }
        free( psz_string );

        psz_string = var_GetString( p_input, "sub-language" );
        p_sys->ppsz_sub_language = LanguageSplit( psz_string );
        if( p_sys->ppsz_sub_language )
        {
            for( int i = 0; p_sys->ppsz_sub_language[i]; i++ )
                msg_Dbg( p_input, "selected subtitle language[%d] %s",
                         i, p_sys->ppsz_sub_language[i] );
        }
        free( psz_string );
    }
    else
    {
        p_sys->ppsz_sub_language = NULL;
        p_sys->ppsz_audio_language = NULL;
    }

    p_sys->i_audio_id = var_GetInteger( p_input, "audio-track-id" );

    p_sys->i_sub_id = var_GetInteger( p_input, "sub-track-id" );

    p_sys->p_es_audio = NULL;
    p_sys->p_es_video = NULL;
    p_sys->p_es_sub   = NULL;

    p_sys->i_audio_delay= 0;
    p_sys->i_spu_delay  = 0;

    p_sys->b_paused = false;
    p_sys->i_pause_date = -1;

    p_sys->i_rate = i_rate;
    p_sys->i_pts_delay = 0;
    p_sys->i_cr_average = 0;

    p_sys->b_buffering = true;
    p_sys->i_buffering_extra_initial = 0;
    p_sys->i_buffering_extra_stream = 0;
    p_sys->i_buffering_extra_system = 0;
    p_sys->i_preroll_end = -1;

    p_sys->p_sout_record = NULL;

    return out;
}

/*****************************************************************************
 *
 *****************************************************************************/
static void EsOutDelete( es_out_t *out )
{
    es_out_sys_t *p_sys = out->p_sys;
    int i;

    if( p_sys->p_sout_record )
        EsOutSetRecord( out, false );

    for( i = 0; i < p_sys->i_es; i++ )
    {
        if( p_sys->es[i]->p_dec )
            input_DecoderDelete( p_sys->es[i]->p_dec );

        free( p_sys->es[i]->psz_language );
        free( p_sys->es[i]->psz_language_code );
        es_format_Clean( &p_sys->es[i]->fmt );

        free( p_sys->es[i] );
    }
    if( p_sys->ppsz_audio_language )
    {
        for( i = 0; p_sys->ppsz_audio_language[i]; i++ )
            free( p_sys->ppsz_audio_language[i] );
        free( p_sys->ppsz_audio_language );
    }
    if( p_sys->ppsz_sub_language )
    {
        for( i = 0; p_sys->ppsz_sub_language[i]; i++ )
            free( p_sys->ppsz_sub_language[i] );
        free( p_sys->ppsz_sub_language );
    }
    free( p_sys->es );

    /* FIXME duplicate work EsOutProgramDel (but we cannot use it) add a EsOutProgramClean ? */
    for( i = 0; i < p_sys->i_pgrm; i++ )
    {
        es_out_pgrm_t *p_pgrm = p_sys->pgrm[i];
        input_clock_Delete( p_pgrm->p_clock );
        free( p_pgrm->psz_now_playing );
        free( p_pgrm->psz_publisher );
        free( p_pgrm->psz_name );
        if( p_pgrm->p_epg )
            vlc_epg_Delete( p_pgrm->p_epg );

        free( p_pgrm );
    }
    TAB_CLEAN( p_sys->i_pgrm, p_sys->pgrm );
    vlc_mutex_destroy( &p_sys->lock );

    free( p_sys );
    free( out );
}

static mtime_t EsOutGetWakeup( es_out_t *out )
{
    es_out_sys_t   *p_sys = out->p_sys;
    input_thread_t *p_input = p_sys->p_input;

    if( !p_sys->p_pgrm )
        return 0;

    /* We do not have a wake up date if the input cannot have its speed
     * controlled or sout is imposing its own or while buffering
     *
     * FIXME for !p_input->p->b_can_pace_control a wkeup time is still needed to avoid too strong buffering */
    if( !p_input->p->b_can_pace_control ||
        p_input->p->b_out_pace_control ||
        p_sys->b_buffering )
        return 0;

    return input_clock_GetWakeup( p_sys->p_pgrm->p_clock );
}

static es_out_id_t *EsOutGetFromID( es_out_t *out, int i_id )
{
    int i;
    if( i_id < 0 )
    {
        /* Special HACK, -i_id is the cat of the stream */
        return (es_out_id_t*)((uint8_t*)NULL-i_id);
    }

    for( i = 0; i < out->p_sys->i_es; i++ )
    {
        if( out->p_sys->es[i]->i_id == i_id )
            return out->p_sys->es[i];
    }
    return NULL;
}

static bool EsOutDecodersIsEmpty( es_out_t *out )
{
    es_out_sys_t      *p_sys = out->p_sys;
    int i;

    if( p_sys->b_buffering && p_sys->p_pgrm )
    {
        EsOutDecodersStopBuffering( out, true );
        if( p_sys->b_buffering )
            return true;
    }

    for( i = 0; i < p_sys->i_es; i++ )
    {
        es_out_id_t *es = p_sys->es[i];

        if( es->p_dec && !input_DecoderIsEmpty( es->p_dec ) )
            return false;
        if( es->p_dec_record && !input_DecoderIsEmpty( es->p_dec_record ) )
            return false;
    }
    return true;
}

static void EsOutSetDelay( es_out_t *out, int i_cat, int64_t i_delay )
{
    es_out_sys_t *p_sys = out->p_sys;

    if( i_cat == AUDIO_ES )
        p_sys->i_audio_delay = i_delay;
    else if( i_cat == SPU_ES )
        p_sys->i_spu_delay = i_delay;

    for( int i = 0; i < p_sys->i_es; i++ )
        EsOutDecoderChangeDelay( out, p_sys->es[i] );
}

static int EsOutSetRecord(  es_out_t *out, bool b_record )
{
    es_out_sys_t   *p_sys = out->p_sys;
    input_thread_t *p_input = p_sys->p_input;

    assert( ( b_record && !p_sys->p_sout_record ) || ( !b_record && p_sys->p_sout_record ) );

    if( b_record )
    {
        char *psz_path = var_CreateGetString( p_input, "input-record-path" );
        if( !psz_path || *psz_path == '\0' )
        {
            free( psz_path );
            psz_path = strdup( config_GetHomeDir() );
        }

        char *psz_sout = NULL;  // TODO conf

        if( !psz_sout && psz_path )
        {
            char *psz_file = input_CreateFilename( VLC_OBJECT(p_input), psz_path, INPUT_RECORD_PREFIX, NULL );
            if( psz_file )
            {
                if( asprintf( &psz_sout, "#record{dst-prefix='%s'}", psz_file ) < 0 )
                    psz_sout = NULL;
                free( psz_file );
            }
        }
        free( psz_path );

        if( !psz_sout )
            return VLC_EGENERIC;

#ifdef ENABLE_SOUT
        p_sys->p_sout_record = sout_NewInstance( p_input, psz_sout );
#endif
        free( psz_sout );

        if( !p_sys->p_sout_record )
            return VLC_EGENERIC;

        for( int i = 0; i < p_sys->i_es; i++ )
        {
            es_out_id_t *p_es = p_sys->es[i];

            if( !p_es->p_dec || p_es->p_master )
                continue;

            p_es->p_dec_record = input_DecoderNew( p_input, &p_es->fmt, p_es->p_pgrm->p_clock, p_sys->p_sout_record );
            if( p_es->p_dec_record && p_sys->b_buffering )
                input_DecoderStartBuffering( p_es->p_dec_record );
        }
    }
    else
    {
        for( int i = 0; i < p_sys->i_es; i++ )
        {
            es_out_id_t *p_es = p_sys->es[i];

            if( !p_es->p_dec_record )
                continue;

            input_DecoderDelete( p_es->p_dec_record );
            p_es->p_dec_record = NULL;
        }
#ifdef ENABLE_SOUT
        sout_DeleteInstance( p_sys->p_sout_record );
#endif
        p_sys->p_sout_record = NULL;
    }

    return VLC_SUCCESS;
}
static void EsOutChangePause( es_out_t *out, bool b_paused, mtime_t i_date )
{
    es_out_sys_t *p_sys = out->p_sys;

    /* XXX the order is important */
    if( b_paused )
    {
        EsOutDecodersChangePause( out, true, i_date );
        EsOutProgramChangePause( out, true, i_date );
    }
    else
    {
        if( p_sys->i_buffering_extra_initial > 0 )
        {
            mtime_t i_stream_start;
            mtime_t i_system_start;
            mtime_t i_stream_duration;
            mtime_t i_system_duration;
            int i_ret;
            i_ret = input_clock_GetState( p_sys->p_pgrm->p_clock,
                                          &i_stream_start, &i_system_start,
                                          &i_stream_duration, &i_system_duration );
            if( !i_ret )
            {
                /* FIXME pcr != exactly what wanted */
                const mtime_t i_used = /*(i_stream_duration - p_sys->p_input->p->i_pts_delay)*/ p_sys->i_buffering_extra_system - p_sys->i_buffering_extra_initial;
                i_date -= i_used;
            }
            p_sys->i_buffering_extra_initial = 0;
            p_sys->i_buffering_extra_stream = 0;
            p_sys->i_buffering_extra_system = 0;
        }
        EsOutProgramChangePause( out, false, i_date );
        EsOutDecodersChangePause( out, false, i_date );

        EsOutProgramsChangeRate( out );
    }
    p_sys->b_paused = b_paused;
    p_sys->i_pause_date = i_date;
}

static void EsOutChangeRate( es_out_t *out, int i_rate )
{
    es_out_sys_t      *p_sys = out->p_sys;

    p_sys->i_rate = i_rate;

    if( !p_sys->b_paused )
        EsOutProgramsChangeRate( out );
}

static void EsOutChangePosition( es_out_t *out )
{
    es_out_sys_t      *p_sys = out->p_sys;

    input_SendEventCache( p_sys->p_input, 0.0 );

    for( int i = 0; i < p_sys->i_es; i++ )
    {
        es_out_id_t *p_es = p_sys->es[i];

        if( !p_es->p_dec )
            continue;

        input_DecoderStartBuffering( p_es->p_dec );

        if( p_es->p_dec_record )
            input_DecoderStartBuffering( p_es->p_dec_record );
    }

    for( int i = 0; i < p_sys->i_pgrm; i++ )
        input_clock_Reset( p_sys->pgrm[i]->p_clock );

    p_sys->b_buffering = true;
    p_sys->i_buffering_extra_initial = 0;
    p_sys->i_buffering_extra_stream = 0;
    p_sys->i_buffering_extra_system = 0;
    p_sys->i_preroll_end = -1;
}



static void EsOutDecodersStopBuffering( es_out_t *out, bool b_forced )
{
    es_out_sys_t *p_sys = out->p_sys;
    int i_ret;

    mtime_t i_stream_start;
    mtime_t i_system_start;
    mtime_t i_stream_duration;
    mtime_t i_system_duration;
    i_ret = input_clock_GetState( p_sys->p_pgrm->p_clock,
                                  &i_stream_start, &i_system_start,
                                  &i_stream_duration, &i_system_duration );
    assert( !i_ret || b_forced );
    if( i_ret )
        return;

    mtime_t i_preroll_duration = 0;
    if( p_sys->i_preroll_end >= 0 )
        i_preroll_duration = __MAX( p_sys->i_preroll_end - i_stream_start, 0 );

    const mtime_t i_buffering_duration = p_sys->i_pts_delay +
                                         i_preroll_duration +
                                         p_sys->i_buffering_extra_stream - p_sys->i_buffering_extra_initial;

    if( i_stream_duration <= i_buffering_duration && !b_forced )
    {
        const double f_level = (double)i_stream_duration / i_buffering_duration;
        input_SendEventCache( p_sys->p_input, f_level );

        msg_Dbg( p_sys->p_input, "Buffering %d%%", (int)(100 * f_level) );
        return;
    }
    input_SendEventCache( p_sys->p_input, 1.0 );

    msg_Dbg( p_sys->p_input, "Stream buffering done (%d ms in %d ms)",
              (int)(i_stream_duration/1000), (int)(i_system_duration/1000) );
    p_sys->b_buffering = false;
    p_sys->i_preroll_end = -1;

    if( p_sys->i_buffering_extra_initial > 0 )
    {
        /* FIXME wrong ? */
        return;
    }

    const mtime_t i_decoder_buffering_start = mdate();
    for( int i = 0; i < p_sys->i_es; i++ )
    {
        es_out_id_t *p_es = p_sys->es[i];

        if( !p_es->p_dec || p_es->fmt.i_cat == SPU_ES )
            continue;
        input_DecoderWaitBuffering( p_es->p_dec );
        if( p_es->p_dec_record )
            input_DecoderWaitBuffering( p_es->p_dec_record );
    }

    msg_Dbg( p_sys->p_input, "Decoder buffering done in %d ms",
              (int)(mdate() - i_decoder_buffering_start)/1000 );

    /* Here is a good place to destroy unused vout with every demuxer */
    input_resource_TerminateVout( p_sys->p_input->p->p_resource );

    /* */
    const mtime_t i_wakeup_delay = 10*1000; /* FIXME CLEANUP thread wake up time*/
    const mtime_t i_current_date = p_sys->b_paused ? p_sys->i_pause_date : mdate();

    input_clock_ChangeSystemOrigin( p_sys->p_pgrm->p_clock, i_current_date + i_wakeup_delay - i_buffering_duration );

    for( int i = 0; i < p_sys->i_es; i++ )
    {
        es_out_id_t *p_es = p_sys->es[i];

        if( !p_es->p_dec )
            continue;

        input_DecoderStopBuffering( p_es->p_dec );
        if( p_es->p_dec_record )
            input_DecoderStopBuffering( p_es->p_dec_record );
    }
}
static void EsOutDecodersChangePause( es_out_t *out, bool b_paused, mtime_t i_date )
{
    es_out_sys_t *p_sys = out->p_sys;

    /* Pause decoders first */
    for( int i = 0; i < p_sys->i_es; i++ )
    {
        es_out_id_t *es = p_sys->es[i];

        if( es->p_dec )
        {
            input_DecoderChangePause( es->p_dec, b_paused, i_date );
            if( es->p_dec_record )
                input_DecoderChangePause( es->p_dec_record, b_paused, i_date );
        }
    }
}
static void EsOutProgramChangePause( es_out_t *out, bool b_paused, mtime_t i_date )
{
    es_out_sys_t *p_sys = out->p_sys;

    for( int i = 0; i < p_sys->i_pgrm; i++ )
        input_clock_ChangePause( p_sys->pgrm[i]->p_clock, b_paused, i_date );
}

static void EsOutDecoderChangeDelay( es_out_t *out, es_out_id_t *p_es )
{
    es_out_sys_t *p_sys = out->p_sys;

    mtime_t i_delay = 0;
    if( p_es->fmt.i_cat == AUDIO_ES )
        i_delay = p_sys->i_audio_delay;
    else if( p_es->fmt.i_cat == SPU_ES )
        i_delay = p_sys->i_spu_delay;

    if( i_delay != 0 )
    {
        if( p_es->p_dec )
            input_DecoderChangeDelay( p_es->p_dec, i_delay );
        if( p_es->p_dec_record )
            input_DecoderChangeDelay( p_es->p_dec_record, i_delay );
    }
}
static void EsOutProgramsChangeRate( es_out_t *out )
{
    es_out_sys_t      *p_sys = out->p_sys;

    for( int i = 0; i < p_sys->i_pgrm; i++ )
        input_clock_ChangeRate( p_sys->pgrm[i]->p_clock, p_sys->i_rate );
}

static void EsOutFrameNext( es_out_t *out )
{
    es_out_sys_t *p_sys = out->p_sys;
    es_out_id_t *p_es_video = NULL;

    if( p_sys->b_buffering )
    {
        msg_Warn( p_sys->p_input, "buffering, ignoring 'frame next'" );
        return;
    }

    assert( p_sys->b_paused );

    for( int i = 0; i < p_sys->i_es; i++ )
    {
        es_out_id_t *p_es = p_sys->es[i];

        if( p_es->fmt.i_cat == VIDEO_ES && p_es->p_dec )
        {
            p_es_video = p_es;
            break;
        }
    }

    if( !p_es_video )
    {
        msg_Warn( p_sys->p_input, "No video track selected, ignoring 'frame next'" );
        return;
    }

    mtime_t i_duration;
    input_DecoderFrameNext( p_es_video->p_dec, &i_duration );

    msg_Dbg( out->p_sys->p_input, "EsOutFrameNext consummed %d ms", (int)(i_duration/1000) );

    if( i_duration <= 0 )
        i_duration = 40*1000;

    /* FIXME it is not a clean way ? */
    if( p_sys->i_buffering_extra_initial <= 0 )
    {
        mtime_t i_stream_start;
        mtime_t i_system_start;
        mtime_t i_stream_duration;
        mtime_t i_system_duration;
        int i_ret;

        i_ret = input_clock_GetState( p_sys->p_pgrm->p_clock,
                                      &i_stream_start, &i_system_start,
                                      &i_stream_duration, &i_system_duration );
        if( i_ret )
            return;

        p_sys->i_buffering_extra_initial = 1 + i_stream_duration - p_sys->i_pts_delay; /* FIXME < 0 ? */
        p_sys->i_buffering_extra_system =
        p_sys->i_buffering_extra_stream = p_sys->i_buffering_extra_initial;
    }

    const int i_rate = input_clock_GetRate( p_sys->p_pgrm->p_clock );

    p_sys->b_buffering = true;
    p_sys->i_buffering_extra_system += i_duration;
    p_sys->i_buffering_extra_stream = p_sys->i_buffering_extra_initial +
                                      ( p_sys->i_buffering_extra_system - p_sys->i_buffering_extra_initial ) *
                                                INPUT_RATE_DEFAULT / i_rate;

    p_sys->i_preroll_end = -1;
}
static mtime_t EsOutGetBuffering( es_out_t *out )
{
    es_out_sys_t *p_sys = out->p_sys;

    if( !p_sys->p_pgrm )
        return 0;

    int i_ret;
    mtime_t i_stream_start;
    mtime_t i_system_start;
    mtime_t i_stream_duration;
    mtime_t i_system_duration;
    i_ret = input_clock_GetState( p_sys->p_pgrm->p_clock,
                                  &i_stream_start, &i_system_start,
                                  &i_stream_duration, &i_system_duration );

    if( i_ret )
        return 0;

    mtime_t i_delay;

    if( p_sys->b_buffering && p_sys->i_buffering_extra_initial <= 0 )
    {
        i_delay = i_stream_duration;
    }
    else
    {
        mtime_t i_system_duration;
        if( p_sys->b_paused )
        {
            i_system_duration = p_sys->i_pause_date  - i_system_start;
            if( p_sys->i_buffering_extra_initial > 0 )
                i_system_duration += p_sys->i_buffering_extra_system - p_sys->i_buffering_extra_initial;
        }
        else
        {
            i_system_duration = mdate() - i_system_start;
        }

        const mtime_t i_consumed = i_system_duration * INPUT_RATE_DEFAULT / p_sys->i_rate - i_stream_duration;
        i_delay = p_sys->i_pts_delay - i_consumed;
    }
    if( i_delay < 0 )
        return 0;
    return i_delay;
}

static void EsOutESVarUpdateGeneric( es_out_t *out, int i_id,
                                     const es_format_t *fmt, const char *psz_language,
                                     bool b_delete )
{
    es_out_sys_t      *p_sys = out->p_sys;
    input_thread_t    *p_input = p_sys->p_input;
    vlc_value_t       val, text;

    if( b_delete )
    {
        if( EsFmtIsTeletext( fmt ) )
            input_SendEventTeletextDel( p_sys->p_input, i_id );

        input_SendEventEsDel( p_input, fmt->i_cat, i_id );
        return;
    }

    /* Get the number of ES already added */
    const char *psz_var;
    if( fmt->i_cat == AUDIO_ES )
        psz_var = "audio-es";
    else if( fmt->i_cat == VIDEO_ES )
        psz_var = "video-es";
    else
        psz_var = "spu-es";

    var_Change( p_input, psz_var, VLC_VAR_CHOICESCOUNT, &val, NULL );
    if( val.i_int == 0 )
    {
        vlc_value_t val2;

        /* First one, we need to add the "Disable" choice */
        val2.i_int = -1; text.psz_string = _("Disable");
        var_Change( p_input, psz_var, VLC_VAR_ADDCHOICE, &val2, &text );
        val.i_int++;
    }

    /* Take care of the ES description */
    if( fmt->psz_description && *fmt->psz_description )
    {
        if( psz_language && *psz_language )
        {
            text.psz_string = malloc( strlen( fmt->psz_description) +
                                      strlen( psz_language ) + 10 );
            sprintf( text.psz_string, "%s - [%s]", fmt->psz_description,
                                                   psz_language );
        }
        else text.psz_string = strdup( fmt->psz_description );
    }
    else
    {
        if( psz_language && *psz_language )
        {
            if( asprintf( &text.psz_string, "%s %i - [%s]", _( "Track" ), val.i_int, psz_language ) == -1 )
                text.psz_string = NULL;
        }
        else
        {
            if( asprintf( &text.psz_string, "%s %i", _( "Track" ), val.i_int ) == -1 )
                text.psz_string = NULL;
        }
    }

    input_SendEventEsAdd( p_input, fmt->i_cat, i_id, text.psz_string );
    if( EsFmtIsTeletext( fmt ) )
    {
        char psz_page[3+1];
        snprintf( psz_page, sizeof(psz_page), "%d%2.2x",
                  fmt->subs.teletext.i_magazine,
                  fmt->subs.teletext.i_page );
        input_SendEventTeletextAdd( p_sys->p_input,
                                    i_id, fmt->subs.teletext.i_magazine >= 0 ? psz_page : NULL );
    }

    free( text.psz_string );
}

static void EsOutESVarUpdate( es_out_t *out, es_out_id_t *es,
                              bool b_delete )
{
    EsOutESVarUpdateGeneric( out, es->i_id, &es->fmt, es->psz_language, b_delete );
}

/* EsOutProgramSelect:
 *  Select a program and update the object variable
 */
static void EsOutProgramSelect( es_out_t *out, es_out_pgrm_t *p_pgrm )
{
    es_out_sys_t      *p_sys = out->p_sys;
    input_thread_t    *p_input = p_sys->p_input;
    int               i;

    if( p_sys->p_pgrm == p_pgrm )
        return; /* Nothing to do */

    if( p_sys->p_pgrm )
    {
        es_out_pgrm_t *old = p_sys->p_pgrm;
        msg_Dbg( p_input, "unselecting program id=%d", old->i_id );

        for( i = 0; i < p_sys->i_es; i++ )
        {
            if( p_sys->es[i]->p_pgrm == old && EsIsSelected( p_sys->es[i] ) &&
                p_sys->i_mode != ES_OUT_MODE_ALL )
                EsUnselect( out, p_sys->es[i], true );
        }

        p_sys->p_es_audio = NULL;
        p_sys->p_es_sub = NULL;
        p_sys->p_es_video = NULL;
    }

    msg_Dbg( p_input, "selecting program id=%d", p_pgrm->i_id );

    /* Mark it selected */
    p_pgrm->b_selected = true;

    /* Switch master stream */
    p_sys->p_pgrm = p_pgrm;

    /* Update "program" */
    input_SendEventProgramSelect( p_input, p_pgrm->i_id );

    /* Update "es-*" */
    input_SendEventEsDel( p_input, AUDIO_ES, -1 );
    input_SendEventEsDel( p_input, VIDEO_ES, -1 );
    input_SendEventEsDel( p_input, SPU_ES, -1 );
    input_SendEventTeletextDel( p_input, -1 );
    input_SendEventProgramScrambled( p_input, p_pgrm->i_id, p_pgrm->b_scrambled );

    /* TODO event */
    var_SetInteger( p_input, "teletext-es", -1 );

    for( i = 0; i < p_sys->i_es; i++ )
    {
        if( p_sys->es[i]->p_pgrm == p_sys->p_pgrm )
            EsOutESVarUpdate( out, p_sys->es[i], false );
        EsOutSelect( out, p_sys->es[i], false );
    }

    /* Update now playing */
    input_item_SetNowPlaying( p_input->p->p_item, p_pgrm->psz_now_playing );
    input_item_SetPublisher( p_input->p->p_item, p_pgrm->psz_publisher );

    input_SendEventMeta( p_input );
}

/* EsOutAddProgram:
 *  Add a program
 */
static es_out_pgrm_t *EsOutProgramAdd( es_out_t *out, int i_group )
{
    es_out_sys_t      *p_sys = out->p_sys;
    input_thread_t    *p_input = p_sys->p_input;

    es_out_pgrm_t *p_pgrm = malloc( sizeof( es_out_pgrm_t ) );
    if( !p_pgrm )
        return NULL;

    /* Init */
    p_pgrm->i_id = i_group;
    p_pgrm->i_es = 0;
    p_pgrm->b_selected = false;
    p_pgrm->b_scrambled = false;
    p_pgrm->psz_name = NULL;
    p_pgrm->psz_now_playing = NULL;
    p_pgrm->psz_publisher = NULL;
    p_pgrm->p_epg = NULL;
    p_pgrm->p_clock = input_clock_New( p_sys->i_rate );
    if( !p_pgrm->p_clock )
    {
        free( p_pgrm );
        return NULL;
    }
    if( p_sys->b_paused )
        input_clock_ChangePause( p_pgrm->p_clock, p_sys->b_paused, p_sys->i_pause_date );
    input_clock_SetJitter( p_pgrm->p_clock, p_sys->i_pts_delay, p_sys->i_cr_average );

    /* Append it */
    TAB_APPEND( p_sys->i_pgrm, p_sys->pgrm, p_pgrm );

    /* Update "program" variable */
    input_SendEventProgramAdd( p_input, i_group, NULL );

    if( i_group == var_GetInteger( p_input, "program" ) )
        EsOutProgramSelect( out, p_pgrm );

    return p_pgrm;
}

/* EsOutDelProgram:
 *  Delete a program
 */
static int EsOutProgramDel( es_out_t *out, int i_group )
{
    es_out_sys_t      *p_sys = out->p_sys;
    input_thread_t    *p_input = p_sys->p_input;
    es_out_pgrm_t     *p_pgrm = NULL;
    int               i;

    for( i = 0; i < p_sys->i_pgrm; i++ )
    {
        if( p_sys->pgrm[i]->i_id == i_group )
        {
            p_pgrm = p_sys->pgrm[i];
            break;
        }
    }

    if( p_pgrm == NULL )
        return VLC_EGENERIC;

    if( p_pgrm->i_es )
    {
        msg_Dbg( p_input, "can't delete program %d which still has %i ES",
                 i_group, p_pgrm->i_es );
        return VLC_EGENERIC;
    }

    TAB_REMOVE( p_sys->i_pgrm, p_sys->pgrm, p_pgrm );

    /* If program is selected we need to unselect it */
    if( p_sys->p_pgrm == p_pgrm )
        p_sys->p_pgrm = NULL;

    input_clock_Delete( p_pgrm->p_clock );

    free( p_pgrm->psz_name );
    free( p_pgrm->psz_now_playing );
    free( p_pgrm->psz_publisher );
    if( p_pgrm->p_epg )
        vlc_epg_Delete( p_pgrm->p_epg );
    free( p_pgrm );

    /* Update "program" variable */
    input_SendEventProgramDel( p_input, i_group );

    return VLC_SUCCESS;
}

/* EsOutProgramFind
 */
static es_out_pgrm_t *EsOutProgramFind( es_out_t *p_out, int i_group )
{
    es_out_sys_t *p_sys = p_out->p_sys;

    for( int i = 0; i < p_sys->i_pgrm; i++ )
    {
        if( p_sys->pgrm[i]->i_id == i_group )
            return p_sys->pgrm[i];
    }
    return EsOutProgramAdd( p_out, i_group );
}

/* EsOutProgramMeta:
 */
static char *EsOutProgramGetMetaName( es_out_pgrm_t *p_pgrm )
{
    char *psz = NULL;
    if( p_pgrm->psz_name )
    {
        if( asprintf( &psz, _("%s [%s %d]"), p_pgrm->psz_name, _("Program"), p_pgrm->i_id ) == -1 )
            return NULL;
    }
    else
    {
        if( asprintf( &psz, "%s %d", _("Program"), p_pgrm->i_id ) == -1 )
            return NULL;
    }
    return psz;
}

static void EsOutProgramMeta( es_out_t *out, int i_group, vlc_meta_t *p_meta )
{
    es_out_sys_t      *p_sys = out->p_sys;
    es_out_pgrm_t     *p_pgrm;
    input_thread_t    *p_input = p_sys->p_input;
    char              *psz_cat;
    const char        *psz_title = NULL;
    const char        *psz_provider = NULL;
    int i;

    msg_Dbg( p_input, "EsOutProgramMeta: number=%d", i_group );

    /* Check against empty meta data (empty for what we handle) */
    if( !vlc_meta_Get( p_meta, vlc_meta_Title) &&
        !vlc_meta_Get( p_meta, vlc_meta_NowPlaying) &&
        !vlc_meta_Get( p_meta, vlc_meta_Publisher) &&
        vlc_dictionary_keys_count( &p_meta->extra_tags ) <= 0 )
    {
        return;
    }
    /* Find program */
    p_pgrm = EsOutProgramFind( out, i_group );
    if( !p_pgrm )
        return;

    /* */
    psz_title = vlc_meta_Get( p_meta, vlc_meta_Title);
    psz_provider = vlc_meta_Get( p_meta, vlc_meta_Publisher);

    /* Update the description text of the program */
    if( psz_title && *psz_title )
    {
        if( !p_pgrm->psz_name || strcmp( p_pgrm->psz_name, psz_title ) )
        {
            char *psz_cat = EsOutProgramGetMetaName( p_pgrm );

            /* Remove old entries */
            input_Control( p_input, INPUT_DEL_INFO, psz_cat, NULL );
            /* TODO update epg name ?
             * TODO update scrambled info name ? */
            free( psz_cat );
        }
        free( p_pgrm->psz_name );
        p_pgrm->psz_name = strdup( psz_title );

        char *psz_text;
        if( psz_provider && *psz_provider )
        {
            if( asprintf( &psz_text, "%s [%s]", psz_title, psz_provider ) < 0 )
                psz_text = NULL;
        }
        else
        {
            psz_text = strdup( psz_title );
        }

        /* ugly but it works */
        if( psz_text )
        {
            input_SendEventProgramDel( p_input, i_group );
            input_SendEventProgramAdd( p_input, i_group, psz_text );

            free( psz_text );
        }
    }

    psz_cat = EsOutProgramGetMetaName( p_pgrm );
    if( psz_provider )
    {
        if( p_sys->p_pgrm == p_pgrm )
        {
            input_item_SetPublisher( p_input->p->p_item, psz_provider );
            input_SendEventMeta( p_input );
        }
        input_Control( p_input, INPUT_ADD_INFO, psz_cat, input_MetaTypeToLocalizedString(vlc_meta_Publisher), psz_provider );
    }
    char ** ppsz_all_keys = vlc_dictionary_all_keys( &p_meta->extra_tags );
    for( i = 0; ppsz_all_keys[i]; i++ )
    {
        input_Control( p_input, INPUT_ADD_INFO, psz_cat,
                       vlc_gettext(ppsz_all_keys[i]),
                       vlc_dictionary_value_for_key( &p_meta->extra_tags,
                       ppsz_all_keys[i] ) );
        free( ppsz_all_keys[i] );
    }
    free( ppsz_all_keys );

    free( psz_cat );
}

static void vlc_epg_Merge( vlc_epg_t *p_dst, const vlc_epg_t *p_src )
{
    int i;

    /* Add new event */
    for( i = 0; i < p_src->i_event; i++ )
    {
        vlc_epg_event_t *p_evt = p_src->pp_event[i];
        bool b_add = true;
        int j;

        for( j = 0; j < p_dst->i_event; j++ )
        {
            if( p_dst->pp_event[j]->i_start == p_evt->i_start && p_dst->pp_event[j]->i_duration == p_evt->i_duration )
            {
                b_add = false;
                break;
            }
            if( p_dst->pp_event[j]->i_start > p_evt->i_start )
                break;
        }
        if( b_add )
        {
            vlc_epg_event_t *p_copy = calloc( 1, sizeof(vlc_epg_event_t) );
            if( !p_copy )
                break;
            p_copy->i_start = p_evt->i_start;
            p_copy->i_duration = p_evt->i_duration;
            p_copy->psz_name = p_evt->psz_name ? strdup( p_evt->psz_name ) : NULL;
            p_copy->psz_short_description = p_evt->psz_short_description ? strdup( p_evt->psz_short_description ) : NULL;
            p_copy->psz_description = p_evt->psz_description ? strdup( p_evt->psz_description ) : NULL;
            TAB_INSERT( p_dst->i_event, p_dst->pp_event, p_copy, j );
        }
    }
    /* Update current */
    if( p_src->p_current )
        vlc_epg_SetCurrent( p_dst, p_src->p_current->i_start );

    /* Keep only 1 old event  */
    if( p_dst->p_current )
    {
        while( p_dst->i_event > 1 && p_dst->pp_event[0] != p_dst->p_current && p_dst->pp_event[1] != p_dst->p_current )
            TAB_REMOVE( p_dst->i_event, p_dst->pp_event, p_dst->pp_event[0] );
    }
}

static void EsOutProgramEpg( es_out_t *out, int i_group, vlc_epg_t *p_epg )
{
    es_out_sys_t      *p_sys = out->p_sys;
    input_thread_t    *p_input = p_sys->p_input;
    es_out_pgrm_t     *p_pgrm;
    char *psz_cat;

    /* Find program */
    p_pgrm = EsOutProgramFind( out, i_group );
    if( !p_pgrm )
        return;

    /* Merge EPG */
    if( !p_pgrm->p_epg )
        p_pgrm->p_epg = vlc_epg_New( p_pgrm->psz_name );
    vlc_epg_Merge( p_pgrm->p_epg, p_epg );

    /* Update info */
    msg_Dbg( p_input, "EsOutProgramEpg: number=%d name=%s", i_group, p_pgrm->p_epg->psz_name );

    psz_cat = EsOutProgramGetMetaName( p_pgrm );

    char *psz_epg;
    if( asprintf( &psz_epg, "EPG %s", psz_cat ) >= 0 )
    {
        input_item_SetEpg( p_input->p->p_item, psz_epg, p_pgrm->p_epg );
        free( psz_epg );
    }

    /* Update now playing */
    free( p_pgrm->psz_now_playing );
    p_pgrm->psz_now_playing = NULL;
    if( p_pgrm->p_epg->p_current && p_pgrm->p_epg->p_current->psz_name && *p_pgrm->p_epg->p_current->psz_name )
        p_pgrm->psz_now_playing = strdup( p_pgrm->p_epg->p_current->psz_name );

    if( p_pgrm == p_sys->p_pgrm )
    {
        input_item_SetNowPlaying( p_input->p->p_item, p_pgrm->psz_now_playing );
        input_SendEventMeta( p_input );
    }

    if( p_pgrm->psz_now_playing )
    {
        input_Control( p_input, INPUT_ADD_INFO, psz_cat,
            input_MetaTypeToLocalizedString(vlc_meta_NowPlaying),
            p_pgrm->psz_now_playing );
    }
    else
    {
        input_Control( p_input, INPUT_DEL_INFO, psz_cat,
            input_MetaTypeToLocalizedString(vlc_meta_NowPlaying) );
    }

    free( psz_cat );
}

static void EsOutProgramUpdateScrambled( es_out_t *p_out, es_out_pgrm_t *p_pgrm )
{
    es_out_sys_t    *p_sys = p_out->p_sys;
    input_thread_t  *p_input = p_sys->p_input;
    bool b_scrambled = false;

    for( int i = 0; i < p_sys->i_es; i++ )
    {
        if( p_sys->es[i]->p_pgrm == p_pgrm && p_sys->es[i]->b_scrambled )
        {
            b_scrambled = true;
            break;
        }
    }
    if( !p_pgrm->b_scrambled == !b_scrambled )
        return;

    p_pgrm->b_scrambled = b_scrambled;
    char *psz_cat = EsOutProgramGetMetaName( p_pgrm );

    if( b_scrambled )
        input_Control( p_input, INPUT_ADD_INFO, psz_cat, _("Scrambled"), _("Yes") );
    else
        input_Control( p_input, INPUT_DEL_INFO, psz_cat, _("Scrambled") );

    input_SendEventProgramScrambled( p_input, p_pgrm->i_id, b_scrambled );
}

/* EsOutAdd:
 *  Add an es_out
 */
static es_out_id_t *EsOutAdd( es_out_t *out, const es_format_t *fmt )
{
    es_out_sys_t      *p_sys = out->p_sys;
    input_thread_t    *p_input = p_sys->p_input;

    if( fmt->i_group < 0 )
    {
        msg_Err( p_input, "invalid group number" );
        return NULL;
    }

    es_out_id_t   *es = malloc( sizeof( *es ) );
    es_out_pgrm_t *p_pgrm;
    int i;

    if( !es )
        return NULL;

    vlc_mutex_lock( &p_sys->lock );

    /* Search the program */
    p_pgrm = EsOutProgramFind( out, fmt->i_group );
    if( !p_pgrm )
    {
        vlc_mutex_unlock( &p_sys->lock );
        free( es );
        return NULL;
    }

    /* Increase ref count for program */
    p_pgrm->i_es++;

    /* Set up ES */
    es->p_pgrm = p_pgrm;
    es_format_Copy( &es->fmt, fmt );
    if( es->fmt.i_id < 0 )
        es->fmt.i_id = out->p_sys->i_id;
    es->i_id = es->fmt.i_id;
    es->i_meta_id = out->p_sys->i_id;
    es->b_scrambled = false;

    switch( es->fmt.i_cat )
    {
    case AUDIO_ES:
    {
        audio_replay_gain_t rg;

        es->i_channel = p_sys->i_audio;

        memset( &rg, 0, sizeof(rg) );
        vlc_mutex_lock( &p_input->p->p_item->lock );
        vlc_audio_replay_gain_MergeFromMeta( &rg, p_input->p->p_item->p_meta );
        vlc_mutex_unlock( &p_input->p->p_item->lock );

        for( i = 0; i < AUDIO_REPLAY_GAIN_MAX; i++ )
        {
            if( !es->fmt.audio_replay_gain.pb_peak[i] )
            {
                es->fmt.audio_replay_gain.pb_peak[i] = rg.pb_peak[i];
                es->fmt.audio_replay_gain.pf_peak[i] = rg.pf_peak[i];
            }
            if( !es->fmt.audio_replay_gain.pb_gain[i] )
            {
                es->fmt.audio_replay_gain.pb_gain[i] = rg.pb_gain[i];
                es->fmt.audio_replay_gain.pf_gain[i] = rg.pf_gain[i];
            }
        }
        break;
    }

    case VIDEO_ES:
        es->i_channel = p_sys->i_video;
        if( es->fmt.video.i_frame_rate && es->fmt.video.i_frame_rate_base )
            vlc_ureduce( &es->fmt.video.i_frame_rate,
                         &es->fmt.video.i_frame_rate_base,
                         es->fmt.video.i_frame_rate,
                         es->fmt.video.i_frame_rate_base, 0 );
        break;

    case SPU_ES:
        es->i_channel = p_sys->i_sub;
        break;

    default:
        es->i_channel = 0;
        break;
    }
    es->psz_language = LanguageGetName( es->fmt.psz_language ); /* remember so we only need to do it once */
    es->psz_language_code = LanguageGetCode( es->fmt.psz_language );
    es->p_dec = NULL;
    es->p_dec_record = NULL;
    for( i = 0; i < 4; i++ )
        es->pb_cc_present[i] = false;
    es->p_master = NULL;

    if( es->p_pgrm == p_sys->p_pgrm )
        EsOutESVarUpdate( out, es, false );

    /* Select it if needed */
    EsOutSelect( out, es, false );


    TAB_APPEND( out->p_sys->i_es, out->p_sys->es, es );
    p_sys->i_id++;  /* always incremented */
    switch( es->fmt.i_cat )
    {
        case AUDIO_ES:
            p_sys->i_audio++;
            break;
        case SPU_ES:
            p_sys->i_sub++;
            break;
        case VIDEO_ES:
            p_sys->i_video++;
            break;
    }

    EsOutUpdateInfo( out, es, &es->fmt, NULL );

    if( es->b_scrambled )
        EsOutProgramUpdateScrambled( out, es->p_pgrm );

    vlc_mutex_unlock( &p_sys->lock );

    return es;
}

static bool EsIsSelected( es_out_id_t *es )
{
    if( es->p_master )
    {
        bool b_decode = false;
        if( es->p_master->p_dec )
        {
            int i_channel = EsOutGetClosedCaptionsChannel( es->fmt.i_codec );
            if( i_channel != -1 )
                input_DecoderGetCcState( es->p_master->p_dec, &b_decode, i_channel );
        }
        return b_decode;
    }
    else
    {
        return es->p_dec != NULL;
    }
}
static void EsCreateDecoder( es_out_t *out, es_out_id_t *p_es )
{
    es_out_sys_t   *p_sys = out->p_sys;
    input_thread_t *p_input = p_sys->p_input;

    p_es->p_dec = input_DecoderNew( p_input, &p_es->fmt, p_es->p_pgrm->p_clock, p_input->p->p_sout );
    if( p_es->p_dec )
    {
        if( p_sys->b_buffering )
            input_DecoderStartBuffering( p_es->p_dec );

        if( !p_es->p_master && p_sys->p_sout_record )
        {
            p_es->p_dec_record = input_DecoderNew( p_input, &p_es->fmt, p_es->p_pgrm->p_clock, p_sys->p_sout_record );
            if( p_es->p_dec_record && p_sys->b_buffering )
                input_DecoderStartBuffering( p_es->p_dec_record );
        }
    }

    EsOutDecoderChangeDelay( out, p_es );
}
static void EsDestroyDecoder( es_out_t *out, es_out_id_t *p_es )
{
    VLC_UNUSED(out);

    if( !p_es->p_dec )
        return;

    input_DecoderDelete( p_es->p_dec );
    p_es->p_dec = NULL;

    if( p_es->p_dec_record )
    {
        input_DecoderDelete( p_es->p_dec_record );
        p_es->p_dec_record = NULL;
    }
}

static void EsSelect( es_out_t *out, es_out_id_t *es )
{
    es_out_sys_t   *p_sys = out->p_sys;
    input_thread_t *p_input = p_sys->p_input;

    if( EsIsSelected( es ) )
    {
        msg_Warn( p_input, "ES 0x%x is already selected", es->i_id );
        return;
    }

    if( es->p_master )
    {
        int i_channel;
        if( !es->p_master->p_dec )
            return;

        i_channel = EsOutGetClosedCaptionsChannel( es->fmt.i_codec );
        if( i_channel == -1 || input_DecoderSetCcState( es->p_master->p_dec, true, i_channel ) )
            return;
    }
    else
    {
        if( es->fmt.i_cat == VIDEO_ES || es->fmt.i_cat == SPU_ES )
        {
            if( !var_GetBool( p_input, out->b_sout ? "sout-video" : "video" ) )
            {
                msg_Dbg( p_input, "video is disabled, not selecting ES 0x%x",
                         es->i_id );
                return;
            }
        }
        else if( es->fmt.i_cat == AUDIO_ES )
        {
            if( !var_GetBool( p_input, out->b_sout ? "sout-audio" : "audio" ) )
            {
                msg_Dbg( p_input, "audio is disabled, not selecting ES 0x%x",
                         es->i_id );
                return;
            }
        }
        if( es->fmt.i_cat == SPU_ES )
        {
            if( !var_GetBool( p_input, out->b_sout ? "sout-spu" : "spu" ) )
            {
                msg_Dbg( p_input, "spu is disabled, not selecting ES 0x%x",
                         es->i_id );
                return;
            }
        }

        EsCreateDecoder( out, es );

        if( es->p_dec == NULL || es->p_pgrm != p_sys->p_pgrm )
            return;
    }

    /* Mark it as selected */
    input_SendEventEsSelect( p_input, es->fmt.i_cat, es->i_id );
    input_SendEventTeletextSelect( p_input, EsFmtIsTeletext( &es->fmt ) ? es->i_id : -1 );
}

static void EsUnselect( es_out_t *out, es_out_id_t *es, bool b_update )
{
    es_out_sys_t   *p_sys = out->p_sys;
    input_thread_t *p_input = p_sys->p_input;

    if( !EsIsSelected( es ) )
    {
        msg_Warn( p_input, "ES 0x%x is already unselected", es->i_id );
        return;
    }

    if( es->p_master )
    {
        if( es->p_master->p_dec )
        {
            int i_channel = EsOutGetClosedCaptionsChannel( es->fmt.i_codec );
            if( i_channel != -1 )
                input_DecoderSetCcState( es->p_master->p_dec, false, i_channel );
        }
    }
    else
    {
        const int i_spu_id = var_GetInteger( p_input, "spu-es");
        int i;
        for( i = 0; i < 4; i++ )
        {
            if( !es->pb_cc_present[i] || !es->pp_cc_es[i] )
                continue;

            if( i_spu_id == es->pp_cc_es[i]->i_id )
            {
                /* Force unselection of the CC */
                input_SendEventEsSelect( p_input, SPU_ES, -1 );
            }
            EsOutDel( out, es->pp_cc_es[i] );

            es->pb_cc_present[i] = false;
        }
        EsDestroyDecoder( out, es );
    }

    if( !b_update )
        return;

    /* Mark it as unselected */
    input_SendEventEsSelect( p_input, es->fmt.i_cat, -1 );
    if( EsFmtIsTeletext( &es->fmt ) )
        input_SendEventTeletextSelect( p_input, -1 );
}

/**
 * Select an ES given the current mode
 * XXX: you need to take a the lock before (stream.stream_lock)
 *
 * \param out The es_out structure
 * \param es es_out_id structure
 * \param b_force ...
 * \return nothing
 */
static void EsOutSelect( es_out_t *out, es_out_id_t *es, bool b_force )
{
    es_out_sys_t      *p_sys = out->p_sys;

    int i_cat = es->fmt.i_cat;

    if( !p_sys->b_active ||
        ( !b_force && es->fmt.i_priority < 0 ) )
    {
        return;
    }

    if( p_sys->i_mode == ES_OUT_MODE_ALL || b_force )
    {
        if( !EsIsSelected( es ) )
            EsSelect( out, es );
    }
    else if( p_sys->i_mode == ES_OUT_MODE_PARTIAL )
    {
        vlc_value_t val;
        int i;
        var_Get( p_sys->p_input, "programs", &val );
        for ( i = 0; i < val.p_list->i_count; i++ )
        {
            if ( val.p_list->p_values[i].i_int == es->p_pgrm->i_id || b_force )
            {
                if( !EsIsSelected( es ) )
                    EsSelect( out, es );
                break;
            }
        }
        var_Change( p_sys->p_input, "programs", VLC_VAR_FREELIST, &val, NULL );
    }
    else if( p_sys->i_mode == ES_OUT_MODE_AUTO )
    {
        int i_wanted  = -1;

        if( es->p_pgrm != p_sys->p_pgrm )
            return;

        if( i_cat == AUDIO_ES )
        {
            int idx1 = LanguageArrayIndex( p_sys->ppsz_audio_language,
                                     es->psz_language_code );

            if( p_sys->p_es_audio &&
                p_sys->p_es_audio->fmt.i_priority >= es->fmt.i_priority )
            {
                int idx2 = LanguageArrayIndex( p_sys->ppsz_audio_language,
                                         p_sys->p_es_audio->psz_language_code );

                if( idx1 < 0 || ( idx2 >= 0 && idx2 <= idx1 ) )
                    return;
                i_wanted = es->i_channel;
            }
            else
            {
                /* Select audio if (no audio selected yet)
                 * - no audio-language
                 * - no audio code for the ES
                 * - audio code in the requested list */
                if( idx1 >= 0 ||
                    !strcmp( es->psz_language_code, "??" ) ||
                    !p_sys->ppsz_audio_language )
                    i_wanted = es->i_channel;
            }

            if( p_sys->i_audio_last >= 0 )
                i_wanted = p_sys->i_audio_last;

            if( p_sys->i_audio_id >= 0 )
            {
                if( es->i_id == p_sys->i_audio_id )
                    i_wanted = es->i_channel;
                else
                    return;
            }
        }
        else if( i_cat == SPU_ES )
        {
            int idx1 = LanguageArrayIndex( p_sys->ppsz_sub_language,
                                     es->psz_language_code );

            if( p_sys->p_es_sub &&
                p_sys->p_es_sub->fmt.i_priority >= es->fmt.i_priority )
            {
                int idx2 = LanguageArrayIndex( p_sys->ppsz_sub_language,
                                         p_sys->p_es_sub->psz_language_code );

                msg_Dbg( p_sys->p_input, "idx1=%d(%s) idx2=%d(%s)",
                        idx1, es->psz_language_code, idx2,
                        p_sys->p_es_sub->psz_language_code );

                if( idx1 < 0 || ( idx2 >= 0 && idx2 <= idx1 ) )
                    return;
                /* We found a SPU that matches our language request */
                i_wanted  = es->i_channel;
            }
            else if( idx1 >= 0 )
            {
                msg_Dbg( p_sys->p_input, "idx1=%d(%s)",
                        idx1, es->psz_language_code );

                i_wanted  = es->i_channel;
            }
            else if( p_sys->i_default_sub_id >= 0 )
            {
                if( es->i_id == p_sys->i_default_sub_id )
                    i_wanted = es->i_channel;
            }

            if( p_sys->i_sub_last >= 0 )
                i_wanted  = p_sys->i_sub_last;

            if( p_sys->i_sub_id >= 0 )
            {
                if( es->i_id == p_sys->i_sub_id )
                    i_wanted = es->i_channel;
                else
                    return;
            }
        }
        else if( i_cat == VIDEO_ES )
        {
            i_wanted  = es->i_channel;
        }

        if( i_wanted == es->i_channel && !EsIsSelected( es ) )
            EsSelect( out, es );
    }

    /* FIXME TODO handle priority here */
    if( EsIsSelected( es ) )
    {
        if( i_cat == AUDIO_ES )
        {
            if( p_sys->i_mode == ES_OUT_MODE_AUTO &&
                p_sys->p_es_audio &&
                p_sys->p_es_audio != es &&
                EsIsSelected( p_sys->p_es_audio ) )
            {
                EsUnselect( out, p_sys->p_es_audio, false );
            }
            p_sys->p_es_audio = es;
        }
        else if( i_cat == SPU_ES )
        {
            if( p_sys->i_mode == ES_OUT_MODE_AUTO &&
                p_sys->p_es_sub &&
                p_sys->p_es_sub != es &&
                EsIsSelected( p_sys->p_es_sub ) )
            {
                EsUnselect( out, p_sys->p_es_sub, false );
            }
            p_sys->p_es_sub = es;
        }
        else if( i_cat == VIDEO_ES )
        {
            p_sys->p_es_video = es;
        }
    }
}

/**
 * Send a block for the given es_out
 *
 * \param out the es_out to send from
 * \param es the es_out_id
 * \param p_block the data block to send
 */
static int EsOutSend( es_out_t *out, es_out_id_t *es, block_t *p_block )
{
    es_out_sys_t   *p_sys = out->p_sys;
    input_thread_t *p_input = p_sys->p_input;
    int i_total = 0;

    if( libvlc_stats( p_input ) )
    {
        vlc_mutex_lock( &p_input->p->counters.counters_lock );
        stats_UpdateInteger( p_input, p_input->p->counters.p_demux_read,
                             p_block->i_buffer, &i_total );
        stats_UpdateFloat( p_input , p_input->p->counters.p_demux_bitrate,
                           (float)i_total, NULL );

        /* Update number of corrupted data packats */
        if( p_block->i_flags & BLOCK_FLAG_CORRUPTED )
        {
            stats_UpdateInteger( p_input, p_input->p->counters.p_demux_corrupted,
                                 1, NULL );
        }
        /* Update number of discontinuities */
        if( p_block->i_flags & BLOCK_FLAG_DISCONTINUITY )
        {
            stats_UpdateInteger( p_input, p_input->p->counters.p_demux_discontinuity,
                                 1, NULL );
        }
        vlc_mutex_unlock( &p_input->p->counters.counters_lock );
    }

    vlc_mutex_lock( &p_sys->lock );

    /* Mark preroll blocks */
    if( p_sys->i_preroll_end >= 0 )
    {
        int64_t i_date = p_block->i_pts;
        if( i_date <= 0 )
            i_date = p_block->i_dts;

        if( i_date < p_sys->i_preroll_end )
            p_block->i_flags |= BLOCK_FLAG_PREROLL;
    }

    p_block->i_rate = 0;

    if( !es->p_dec )
    {
        block_Release( p_block );
        vlc_mutex_unlock( &p_sys->lock );
        return VLC_SUCCESS;
    }

    /* Check for sout mode */
    if( out->b_sout )
    {
        /* FIXME review this, proper lock may be missing */
        if( p_input->p->p_sout->i_out_pace_nocontrol > 0 &&
            p_input->p->b_out_pace_control )
        {
            msg_Dbg( p_input, "switching to sync mode" );
            p_input->p->b_out_pace_control = false;
        }
        else if( p_input->p->p_sout->i_out_pace_nocontrol <= 0 &&
                 !p_input->p->b_out_pace_control )
        {
            msg_Dbg( p_input, "switching to async mode" );
            p_input->p->b_out_pace_control = true;
        }
    }

    /* Decode */
    if( es->p_dec_record )
    {
        block_t *p_dup = block_Duplicate( p_block );
        if( p_dup )
            input_DecoderDecode( es->p_dec_record, p_dup,
                                 p_input->p->b_out_pace_control );
    }
    input_DecoderDecode( es->p_dec, p_block,
                         p_input->p->b_out_pace_control );

    es_format_t fmt_dsc;
    vlc_meta_t  *p_meta_dsc;
    if( input_DecoderHasFormatChanged( es->p_dec, &fmt_dsc, &p_meta_dsc ) )
    {
        EsOutUpdateInfo( out, es, &fmt_dsc, p_meta_dsc );

        es_format_Clean( &fmt_dsc );
        if( p_meta_dsc )
            vlc_meta_Delete( p_meta_dsc );
    }

    /* Check CC status */
    bool pb_cc[4];

    input_DecoderIsCcPresent( es->p_dec, pb_cc );
    for( int i = 0; i < 4; i++ )
    {
        static const vlc_fourcc_t fcc[4] = {
            VLC_FOURCC('c', 'c', '1', ' '),
            VLC_FOURCC('c', 'c', '2', ' '),
            VLC_FOURCC('c', 'c', '3', ' '),
            VLC_FOURCC('c', 'c', '4', ' '),
        };
        es_format_t fmt;

        if(  es->pb_cc_present[i] || !pb_cc[i] )
            continue;
        msg_Dbg( p_input, "Adding CC track %d for es[%d]", 1+i, es->i_id );

        es_format_Init( &fmt, SPU_ES, fcc[i] );
        fmt.i_group = es->fmt.i_group;
        if( asprintf( &fmt.psz_description,
                      _("Closed captions %u"), 1 + i ) == -1 )
            fmt.psz_description = NULL;
        es->pp_cc_es[i] = EsOutAdd( out, &fmt );
        es->pp_cc_es[i]->p_master = es;
        es_format_Clean( &fmt );

        /* */
        es->pb_cc_present[i] = true;
    }

    vlc_mutex_unlock( &p_sys->lock );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * EsOutDel:
 *****************************************************************************/
static void EsOutDel( es_out_t *out, es_out_id_t *es )
{
    es_out_sys_t *p_sys = out->p_sys;
    bool b_reselect = false;
    int i;

    vlc_mutex_lock( &p_sys->lock );

    /* We don't try to reselect */
    if( es->p_dec )
    {
        while( !p_sys->p_input->b_die && !p_sys->b_buffering && es->p_dec )
        {
            if( input_DecoderIsEmpty( es->p_dec ) &&
                ( !es->p_dec_record || input_DecoderIsEmpty( es->p_dec_record ) ))
                break;
            /* FIXME there should be a way to have auto deleted es, but there will be
             * a problem when another codec of the same type is created (mainly video) */
            msleep( 20*1000 );
        }
        EsUnselect( out, es, es->p_pgrm == p_sys->p_pgrm );
    }

    if( es->p_pgrm == p_sys->p_pgrm )
        EsOutESVarUpdate( out, es, true );

    TAB_REMOVE( p_sys->i_es, p_sys->es, es );

    /* Update program */
    es->p_pgrm->i_es--;
    if( es->p_pgrm->i_es == 0 )
        msg_Dbg( p_sys->p_input, "Program doesn't contain anymore ES" );

    if( es->b_scrambled )
        EsOutProgramUpdateScrambled( out, es->p_pgrm );

    /* */
    if( p_sys->p_es_audio == es || p_sys->p_es_video == es ||
        p_sys->p_es_sub == es ) b_reselect = true;

    if( p_sys->p_es_audio == es ) p_sys->p_es_audio = NULL;
    if( p_sys->p_es_video == es ) p_sys->p_es_video = NULL;
    if( p_sys->p_es_sub   == es ) p_sys->p_es_sub   = NULL;

    switch( es->fmt.i_cat )
    {
        case AUDIO_ES:
            p_sys->i_audio--;
            break;
        case SPU_ES:
            p_sys->i_sub--;
            break;
        case VIDEO_ES:
            p_sys->i_video--;
            break;
    }

    /* Re-select another track when needed */
    if( b_reselect )
    {
        for( i = 0; i < p_sys->i_es; i++ )
        {
            if( es->fmt.i_cat == p_sys->es[i]->fmt.i_cat )
                EsOutSelect( out, p_sys->es[i], false );
        }
    }

    free( es->psz_language );
    free( es->psz_language_code );

    es_format_Clean( &es->fmt );

    vlc_mutex_unlock( &p_sys->lock );

    free( es );
}

/**
 * Control query handler
 *
 * \param out the es_out to control
 * \param i_query A es_out query as defined in include/ninput.h
 * \param args a variable list of arguments for the query
 * \return VLC_SUCCESS or an error code
 */
static int EsOutControlLocked( es_out_t *out, int i_query, va_list args )
{
    es_out_sys_t *p_sys = out->p_sys;
    bool  b, *pb;
    int         i;

    es_out_id_t *es;

    switch( i_query )
    {
        case ES_OUT_SET_ES_STATE:
            es = (es_out_id_t*) va_arg( args, es_out_id_t * );
            b = (bool) va_arg( args, int );
            if( b && !EsIsSelected( es ) )
            {
                EsSelect( out, es );
                return EsIsSelected( es ) ? VLC_SUCCESS : VLC_EGENERIC;
            }
            else if( !b && EsIsSelected( es ) )
            {
                EsUnselect( out, es, es->p_pgrm == p_sys->p_pgrm );
                return VLC_SUCCESS;
            }
            return VLC_SUCCESS;

        case ES_OUT_GET_ES_STATE:
            es = (es_out_id_t*) va_arg( args, es_out_id_t * );
            pb = (bool*) va_arg( args, bool * );

            *pb = EsIsSelected( es );
            return VLC_SUCCESS;

        case ES_OUT_SET_ACTIVE:
        {
            b = (bool) va_arg( args, int );
            if( b && !p_sys->b_active && p_sys->i_es > 0 )
            {
                /* XXX Terminate vout if there are tracks but no video one.
                 * This one is not mandatory but is he earliest place where it
                 * can be done */
                for( i = 0; i < p_sys->i_es; i++ )
                {
                    es_out_id_t *p_es = p_sys->es[i];
                    if( p_es->fmt.i_cat == VIDEO_ES )
                        break;
                }
                if( i >= p_sys->i_es )
                    input_resource_TerminateVout( p_sys->p_input->p->p_resource );
            }
            p_sys->b_active = b;
            return VLC_SUCCESS;
        }

        case ES_OUT_SET_MODE:
            i = (int) va_arg( args, int );
            if( i == ES_OUT_MODE_NONE || i == ES_OUT_MODE_ALL ||
                i == ES_OUT_MODE_AUTO || i == ES_OUT_MODE_PARTIAL )
            {
                p_sys->i_mode = i;

                /* Reapply policy mode */
                for( i = 0; i < p_sys->i_es; i++ )
                {
                    if( EsIsSelected( p_sys->es[i] ) )
                    {
                        EsUnselect( out, p_sys->es[i],
                                    p_sys->es[i]->p_pgrm == p_sys->p_pgrm );
                    }
                }
                for( i = 0; i < p_sys->i_es; i++ )
                {
                    EsOutSelect( out, p_sys->es[i], false );
                }
                return VLC_SUCCESS;
            }
            return VLC_EGENERIC;

        case ES_OUT_SET_ES:
        case ES_OUT_RESTART_ES:
        {
            int i_cat;

            es = (es_out_id_t*) va_arg( args, es_out_id_t * );

            if( es == NULL )
                i_cat = UNKNOWN_ES;
            else if( es == (es_out_id_t*)((uint8_t*)NULL+AUDIO_ES) )
                i_cat = AUDIO_ES;
            else if( es == (es_out_id_t*)((uint8_t*)NULL+VIDEO_ES) )
                i_cat = VIDEO_ES;
            else if( es == (es_out_id_t*)((uint8_t*)NULL+SPU_ES) )
                i_cat = SPU_ES;
            else
                i_cat = -1;

            for( i = 0; i < p_sys->i_es; i++ )
            {
                if( i_cat == -1 )
                {
                    if( es == p_sys->es[i] )
                    {
                        EsOutSelect( out, es, true );
                        break;
                    }
                }
                else
                {
                    if( i_cat == UNKNOWN_ES || p_sys->es[i]->fmt.i_cat == i_cat )
                    {
                        if( EsIsSelected( p_sys->es[i] ) )
                        {
                            if( i_query == ES_OUT_RESTART_ES )
                            {
                                if( p_sys->es[i]->p_dec )
                                {
                                    EsDestroyDecoder( out, p_sys->es[i] );
                                    EsCreateDecoder( out, p_sys->es[i] );
                                }
                            }
                            else
                            {
                                EsUnselect( out, p_sys->es[i],
                                            p_sys->es[i]->p_pgrm == p_sys->p_pgrm );
                            }
                        }
                    }
                }
            }
            return VLC_SUCCESS;
        }
 
        case ES_OUT_SET_ES_DEFAULT:
        {
            es = (es_out_id_t*) va_arg( args, es_out_id_t * );

            if( es == NULL )
            {
                /*p_sys->i_default_video_id = -1;*/
                /*p_sys->i_default_audio_id = -1;*/
                p_sys->i_default_sub_id = -1;
            }
            else if( es == (es_out_id_t*)((uint8_t*)NULL+AUDIO_ES) )
            {
                /*p_sys->i_default_video_id = -1;*/
            }
            else if( es == (es_out_id_t*)((uint8_t*)NULL+VIDEO_ES) )
            {
                /*p_sys->i_default_audio_id = -1;*/
            }
            else if( es == (es_out_id_t*)((uint8_t*)NULL+SPU_ES) )
            {
                p_sys->i_default_sub_id = -1;
            }
            else
            {
                /*if( es->fmt.i_cat == VIDEO_ES )
                    p_sys->i_default_video_id = es->i_id;
                else
                if( es->fmt.i_cat == AUDIO_ES )
                    p_sys->i_default_audio_id = es->i_id;
                else*/
                if( es->fmt.i_cat == SPU_ES )
                    p_sys->i_default_sub_id = es->i_id;
            }
            return VLC_SUCCESS;
        }

        case ES_OUT_SET_PCR:
        case ES_OUT_SET_GROUP_PCR:
        {
            es_out_pgrm_t *p_pgrm = NULL;
            int            i_group = 0;
            int64_t        i_pcr;

            if( i_query == ES_OUT_SET_PCR )
            {
                p_pgrm = p_sys->p_pgrm;
                if( !p_pgrm )
                    p_pgrm = EsOutProgramAdd( out, i_group );   /* Create it */
            }
            else
            {
                i_group = (int)va_arg( args, int );
                p_pgrm = EsOutProgramFind( out, i_group );
            }
            if( !p_pgrm )
                return VLC_EGENERIC;

            i_pcr = (int64_t)va_arg( args, int64_t );
            if( i_pcr <= VLC_TS_INVALID )
            {
                msg_Err( p_sys->p_input, "Invalid PCR value in ES_OUT_SET_(GROUP_)PCR !" );
                return VLC_EGENERIC;
            }

            /* search program
             * TODO do not use mdate() but proper stream acquisition date */
            input_clock_Update( p_pgrm->p_clock, VLC_OBJECT(p_sys->p_input),
                                p_sys->p_input->p->b_can_pace_control || p_sys->b_buffering, i_pcr, mdate() );
            /* Check buffering state on master clock update */
            if( p_sys->b_buffering && p_pgrm == p_sys->p_pgrm )
                EsOutDecodersStopBuffering( out, false );

            return VLC_SUCCESS;
        }

        case ES_OUT_RESET_PCR:
            msg_Err( p_sys->p_input, "ES_OUT_RESET_PCR called" );
            EsOutChangePosition( out );
            return VLC_SUCCESS;

        case ES_OUT_SET_GROUP:
        {
            int j;
            i = (int) va_arg( args, int );
            for( j = 0; j < p_sys->i_pgrm; j++ )
            {
                es_out_pgrm_t *p_pgrm = p_sys->pgrm[j];
                if( p_pgrm->i_id == i )
                {
                    EsOutProgramSelect( out, p_pgrm );
                    return VLC_SUCCESS;
                }
            }
            return VLC_EGENERIC;
        }

        case ES_OUT_SET_ES_FMT:
        {
            /* This ain't pretty but is need by some demuxers (eg. Ogg )
             * to update the p_extra data */
            es_format_t *p_fmt;
            es = (es_out_id_t*) va_arg( args, es_out_id_t * );
            p_fmt = (es_format_t*) va_arg( args, es_format_t * );
            if( es == NULL )
                return VLC_EGENERIC;

            if( p_fmt->i_extra )
            {
                es->fmt.i_extra = p_fmt->i_extra;
                es->fmt.p_extra = realloc( es->fmt.p_extra, p_fmt->i_extra );
                memcpy( es->fmt.p_extra, p_fmt->p_extra, p_fmt->i_extra );

                if( !es->p_dec )
                    return VLC_SUCCESS;
#if 1
                EsDestroyDecoder( out, es );

                EsCreateDecoder( out, es );
#else
                es->p_dec->fmt_in.i_extra = p_fmt->i_extra;
                es->p_dec->fmt_in.p_extra =
                    realloc( es->p_dec->fmt_in.p_extra, p_fmt->i_extra );
                memcpy( es->p_dec->fmt_in.p_extra,
                        p_fmt->p_extra, p_fmt->i_extra );
#endif
            }

            return VLC_SUCCESS;
        }

        case ES_OUT_SET_ES_SCRAMBLED_STATE:
        {
            es = (es_out_id_t*) va_arg( args, es_out_id_t * );
            bool b_scrambled = (bool)va_arg( args, int );

            if( !es->b_scrambled != !b_scrambled )
            {
                es->b_scrambled = b_scrambled;
                EsOutProgramUpdateScrambled( out, es->p_pgrm );
            }
            return VLC_SUCCESS;
        }

        case ES_OUT_SET_NEXT_DISPLAY_TIME:
        {
            const int64_t i_date = (int64_t)va_arg( args, int64_t );

            if( i_date < 0 )
                return VLC_EGENERIC;

            p_sys->i_preroll_end = i_date;

            return VLC_SUCCESS;
        }
        case ES_OUT_SET_GROUP_META:
        {
            int i_group = (int)va_arg( args, int );
            vlc_meta_t *p_meta = (vlc_meta_t*)va_arg( args, vlc_meta_t * );

            EsOutProgramMeta( out, i_group, p_meta );
            return VLC_SUCCESS;
        }
        case ES_OUT_SET_GROUP_EPG:
        {
            int i_group = (int)va_arg( args, int );
            vlc_epg_t *p_epg = (vlc_epg_t*)va_arg( args, vlc_epg_t * );

            EsOutProgramEpg( out, i_group, p_epg );
            return VLC_SUCCESS;
        }

        case ES_OUT_DEL_GROUP:
        {
            int i_group = (int)va_arg( args, int );

            return EsOutProgramDel( out, i_group );
        }

        case ES_OUT_GET_WAKE_UP:
        {
            mtime_t *pi_wakeup = (mtime_t*)va_arg( args, mtime_t* );
            *pi_wakeup = EsOutGetWakeup( out );
            return VLC_SUCCESS;
        }

        case ES_OUT_SET_ES_BY_ID:
        case ES_OUT_RESTART_ES_BY_ID:
        case ES_OUT_SET_ES_DEFAULT_BY_ID:
        {
            const int i_id = (int)va_arg( args, int );
            es_out_id_t *p_es = EsOutGetFromID( out, i_id );
            int i_new_query;

            switch( i_query )
            {
            case ES_OUT_SET_ES_BY_ID:         i_new_query = ES_OUT_SET_ES; break;
            case ES_OUT_RESTART_ES_BY_ID:     i_new_query = ES_OUT_RESTART_ES; break;
            case ES_OUT_SET_ES_DEFAULT_BY_ID: i_new_query = ES_OUT_SET_ES_DEFAULT; break;
            default:
              assert(0);
            }
            /* TODO if the lock is made non recursive it should be changed */
            int i_ret = es_out_Control( out, i_new_query, p_es );

            /* Clean up vout after user action (in active mode only).
             * FIXME it does not work well with multiple video windows */
            if( p_sys->b_active )
                input_resource_TerminateVout( p_sys->p_input->p->p_resource );
            return i_ret;
        }

        case ES_OUT_GET_BUFFERING:
            pb = (bool *)va_arg( args, bool* );
            *pb = p_sys->b_buffering;
            return VLC_SUCCESS;

        case ES_OUT_GET_EMPTY:
            pb = (bool *)va_arg( args, bool* );
            *pb = EsOutDecodersIsEmpty( out );
            return VLC_SUCCESS;

        case ES_OUT_SET_DELAY:
        {
            const int i_cat = (int)va_arg( args, int );
            const mtime_t i_delay = (mtime_t)va_arg( args, mtime_t );
            EsOutSetDelay( out, i_cat, i_delay );
            return VLC_SUCCESS;
        }

        case ES_OUT_SET_RECORD_STATE:
            b = (bool) va_arg( args, int );
            return EsOutSetRecord( out, b );

        case ES_OUT_SET_PAUSE_STATE:
        {
            const bool b_source_paused = (bool)va_arg( args, int );
            const bool b_paused = (bool)va_arg( args, int );
            const mtime_t i_date = (mtime_t) va_arg( args, mtime_t );

            assert( !b_source_paused == !b_paused );
            EsOutChangePause( out, b_paused, i_date );

            return VLC_SUCCESS;
        }

        case ES_OUT_SET_RATE:
        {
            const int i_src_rate = (int)va_arg( args, int );
            const int i_rate = (int)va_arg( args, int );

            assert( i_src_rate == i_rate );
            EsOutChangeRate( out, i_rate );

            return VLC_SUCCESS;
        }

        case ES_OUT_SET_TIME:
        {
            const mtime_t i_date = (mtime_t)va_arg( args, mtime_t );

            assert( i_date == -1 );
            EsOutChangePosition( out );

            return VLC_SUCCESS;
        }

        case ES_OUT_SET_FRAME_NEXT:
            EsOutFrameNext( out );
            return VLC_SUCCESS;

        case ES_OUT_SET_TIMES:
        {
            double f_position = (double)va_arg( args, double );
            mtime_t i_time = (mtime_t)va_arg( args, mtime_t );
            mtime_t i_length = (mtime_t)va_arg( args, mtime_t );

            /* Fix for buffering delay */
            const mtime_t i_delay = EsOutGetBuffering( out );

            i_time -= i_delay;
            if( i_time < 0 )
                i_time = 0;

            if( i_length > 0 )
                f_position -= (double)i_delay / i_length;
            if( f_position < 0 )
                f_position = 0;

            if( !p_sys->b_buffering )
                input_SendEventTimes( p_sys->p_input, f_position, i_time, i_length );
            return VLC_SUCCESS;
        }
        case ES_OUT_SET_JITTER:
        {
            mtime_t i_pts_delay = (mtime_t)va_arg( args, mtime_t );
            int     i_cr_average = (int)va_arg( args, int );

            if( i_pts_delay == p_sys->i_pts_delay &&
                i_cr_average == p_sys->i_cr_average )
                return VLC_SUCCESS;

            p_sys->i_pts_delay = i_pts_delay;
            p_sys->i_cr_average = i_cr_average;

            for( int i = 0; i < p_sys->i_pgrm; i++ )
                input_clock_SetJitter( p_sys->pgrm[i]->p_clock,
                                       i_pts_delay, i_cr_average );
            return VLC_SUCCESS;
        }

        default:
            msg_Err( p_sys->p_input, "unknown query in es_out_Control" );
            return VLC_EGENERIC;
    }
}
static int EsOutControl( es_out_t *out, int i_query, va_list args )
{
    es_out_sys_t *p_sys = out->p_sys;
    int i_ret;

    vlc_mutex_lock( &p_sys->lock );
    i_ret = EsOutControlLocked( out, i_query, args );
    vlc_mutex_unlock( &p_sys->lock );

    return i_ret;
}

/****************************************************************************
 * LanguageGetName: try to expend iso639 into plain name
 ****************************************************************************/
static char *LanguageGetName( const char *psz_code )
{
    const iso639_lang_t *pl;

    if( psz_code == NULL )
    {
        return strdup( "" );
    }

    if( strlen( psz_code ) == 2 )
    {
        pl = GetLang_1( psz_code );
    }
    else if( strlen( psz_code ) == 3 )
    {
        pl = GetLang_2B( psz_code );
        if( !strcmp( pl->psz_iso639_1, "??" ) )
        {
            pl = GetLang_2T( psz_code );
        }
    }
    else
    {
        return strdup( psz_code );
    }

    if( !strcmp( pl->psz_iso639_1, "??" ) )
    {
       return strdup( psz_code );
    }
    else
    {
        if( *pl->psz_native_name )
        {
            return strdup( pl->psz_native_name );
        }
        return strdup( pl->psz_eng_name );
    }
}

/* Get a 2 char code */
static char *LanguageGetCode( const char *psz_lang )
{
    const iso639_lang_t *pl;

    if( psz_lang == NULL || *psz_lang == '\0' )
        return strdup("??");

    for( pl = p_languages; pl->psz_iso639_1 != NULL; pl++ )
    {
        if( !strcasecmp( pl->psz_eng_name, psz_lang ) ||
            !strcasecmp( pl->psz_native_name, psz_lang ) ||
            !strcasecmp( pl->psz_iso639_1, psz_lang ) ||
            !strcasecmp( pl->psz_iso639_2T, psz_lang ) ||
            !strcasecmp( pl->psz_iso639_2B, psz_lang ) )
            break;
    }

    if( pl->psz_iso639_1 != NULL )
        return strdup( pl->psz_iso639_1 );

    return strdup("??");
}

static char **LanguageSplit( const char *psz_langs )
{
    char *psz_dup;
    char *psz_parser;
    char **ppsz = NULL;
    int i_psz = 0;

    if( psz_langs == NULL ) return NULL;

    psz_parser = psz_dup = strdup(psz_langs);

    while( psz_parser && *psz_parser )
    {
        char *psz;
        char *psz_code;

        psz = strchr(psz_parser, ',' );
        if( psz ) *psz++ = '\0';

        if( !strcmp( psz_parser, "any" ) )
        {
            TAB_APPEND( i_psz, ppsz, strdup("any") );
        }
        else
        {
            psz_code = LanguageGetCode( psz_parser );
            if( strcmp( psz_code, "??" ) )
            {
                TAB_APPEND( i_psz, ppsz, psz_code );
            }
            else
            {
                free( psz_code );
            }
        }

        psz_parser = psz;
    }

    if( i_psz )
    {
        TAB_APPEND( i_psz, ppsz, NULL );
    }

    free( psz_dup );
    return ppsz;
}

static int LanguageArrayIndex( char **ppsz_langs, char *psz_lang )
{
    int i;

    if( !ppsz_langs || !psz_lang ) return -1;

    for( i = 0; ppsz_langs[i]; i++ )
    {
        if( !strcasecmp( ppsz_langs[i], psz_lang ) ||
            !strcasecmp( ppsz_langs[i], "any" ) )
        {
            return i;
        }
    }

    return -1;
}

/****************************************************************************
 * EsOutUpdateInfo:
 * - add meta info to the playlist item
 ****************************************************************************/
static void EsOutUpdateInfo( es_out_t *out, es_out_id_t *es, const es_format_t *fmt, const vlc_meta_t *p_meta )
{
    es_out_sys_t   *p_sys = out->p_sys;
    input_thread_t *p_input = p_sys->p_input;
    const es_format_t *p_fmt_es = &es->fmt;
    char           *psz_cat;
    lldiv_t         div;

    /* Create category name */
    if( asprintf( &psz_cat, _("Stream %d"), es->i_meta_id ) == -1 )
        return;

    /* Remove previous information */
    input_Control( p_input, INPUT_DEL_INFO, psz_cat, NULL );

    /* Add informations */
    const char *psz_type;
    switch( fmt->i_cat )
    {
    case AUDIO_ES:
        psz_type = _("Audio");
        break;
    case VIDEO_ES:
        psz_type = _("Video");
        break;
    case SPU_ES:
        psz_type = _("Subtitle");
        break;
    default:
        psz_type = NULL;
        break;
    }

    if( psz_type )
        input_Control( p_input, INPUT_ADD_INFO, psz_cat, _("Type"), psz_type );

    if( es->i_meta_id != es->i_id )
        input_Control( p_input, INPUT_ADD_INFO, psz_cat, _("Original ID"),
                       "%d", es->i_id );

    input_Control( p_input, INPUT_ADD_INFO, psz_cat, _("Codec"),
                   "%.4s", (char*)&p_fmt_es->i_codec );

    if( es->psz_language && *es->psz_language )
        input_Control( p_input, INPUT_ADD_INFO, psz_cat, _("Language"),
                       "%s", es->psz_language );
    if( fmt->psz_description && *fmt->psz_description )
        input_Control( p_input, INPUT_ADD_INFO, psz_cat, _("Description"),
                       "%s", fmt->psz_description );

    switch( fmt->i_cat )
    {
    case AUDIO_ES:
        input_Control( p_input, INPUT_ADD_INFO, psz_cat,
                       _("Type"), _("Audio") );

        if( fmt->audio.i_physical_channels & AOUT_CHAN_PHYSMASK )
            input_Control( p_input, INPUT_ADD_INFO, psz_cat, _("Channels"),
                           "%s", _( aout_FormatPrintChannels( &fmt->audio ) ) );
        else if( fmt->audio.i_channels > 0 )
            input_Control( p_input, INPUT_ADD_INFO, psz_cat, _("Channels"),
                           "%u", fmt->audio.i_channels );

        if( fmt->audio.i_rate > 0 )
        {
            input_Control( p_input, INPUT_ADD_INFO, psz_cat, _("Sample rate"),
                           _("%u Hz"), fmt->audio.i_rate );
            /* FIXME that should be removed or improved ! (used by text/strings.c) */
            var_SetInteger( p_input, "sample-rate", fmt->audio.i_rate );
        }

        unsigned int i_bitspersample = fmt->audio.i_bitspersample;
        if( i_bitspersample <= 0 )
            i_bitspersample = aout_BitsPerSample( p_fmt_es->i_codec );
        if( i_bitspersample > 0 )
            input_Control( p_input, INPUT_ADD_INFO, psz_cat,
                           _("Bits per sample"), "%u",
                           i_bitspersample );

        if( fmt->i_bitrate > 0 )
        {
            input_Control( p_input, INPUT_ADD_INFO, psz_cat, _("Bitrate"),
                           _("%u kb/s"), fmt->i_bitrate / 1000 );
            /* FIXME that should be removed or improved ! (used by text/strings.c) */
            var_SetInteger( p_input, "bit-rate", fmt->i_bitrate );
        }
        for( int i = 0; i < AUDIO_REPLAY_GAIN_MAX; i++ )
        {
            const audio_replay_gain_t *p_rg = &fmt->audio_replay_gain;
            if( !p_rg->pb_gain[i] )
                continue;
            const char *psz_name;
            if( i == AUDIO_REPLAY_GAIN_TRACK )
                psz_name = _("Track replay gain");
            else
                psz_name = _("Album replay gain");
            input_Control( p_input, INPUT_ADD_INFO, psz_cat,
                           psz_name, _("%.2f dB"), p_rg->pf_gain[i] );
        }
        break;

    case VIDEO_ES:
        input_Control( p_input, INPUT_ADD_INFO, psz_cat,
                       _("Type"), _("Video") );

        if( fmt->video.i_width > 0 && fmt->video.i_height > 0 )
            input_Control( p_input, INPUT_ADD_INFO, psz_cat,
                           _("Resolution"), "%ux%u",
                           fmt->video.i_width, fmt->video.i_height );

        if( fmt->video.i_visible_width > 0 &&
            fmt->video.i_visible_height > 0 )
            input_Control( p_input, INPUT_ADD_INFO, psz_cat,
                           _("Display resolution"), "%ux%u",
                           fmt->video.i_visible_width,
                           fmt->video.i_visible_height);
       if( fmt->video.i_frame_rate > 0 &&
           fmt->video.i_frame_rate_base > 0 )
       {
           div = lldiv( (float)fmt->video.i_frame_rate /
                               fmt->video.i_frame_rate_base * 1000000,
                               1000000 );
           if( div.rem > 0 )
               input_Control( p_input, INPUT_ADD_INFO, psz_cat,
                              _("Frame rate"), "%"PRId64".%06u",
                              div.quot, (unsigned int )div.rem );
           else
               input_Control( p_input, INPUT_ADD_INFO, psz_cat,
                              _("Frame rate"), "%"PRId64, div.quot );
       }
       break;

    case SPU_ES:
        input_Control( p_input, INPUT_ADD_INFO, psz_cat,
                       _("Type"), _("Subtitle") );
        break;

    default:
        break;
    }

    /* Append generic meta */
    if( p_meta )
    {
        char **ppsz_all_keys = vlc_dictionary_all_keys( &p_meta->extra_tags );
        for( int i = 0; ppsz_all_keys && ppsz_all_keys[i]; i++ )
        {
            char *psz_key = ppsz_all_keys[i];
            char *psz_value = vlc_dictionary_value_for_key( &p_meta->extra_tags, psz_key );

            if( psz_value )
                input_Control( p_input, INPUT_ADD_INFO, psz_cat,
                               vlc_gettext(psz_key), vlc_gettext(psz_value) );
            free( psz_key );
        }
        free( ppsz_all_keys );
    }

    free( psz_cat );
}
