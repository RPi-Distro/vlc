/*****************************************************************************
 * input.c: input thread
 *****************************************************************************
 * Copyright (C) 1998-2007 the VideoLAN team
 * $Id: 9fc59c741625e66923ad4ed9a2f5f82bc2cd7f6c $
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
 *          Laurent Aimar <fenrir@via.ecp.fr>
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

#include <ctype.h>
#include <limits.h>
#include <assert.h>

#include "input_internal.h"

#include <vlc_sout.h>
#include "../stream_output/stream_output.h"

#include <vlc_interface.h>
#include <vlc_url.h>
#include <vlc_charset.h>

#ifdef HAVE_SYS_STAT_H
#   include <sys/stat.h>
#endif

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static void Destructor( input_thread_t * p_input );

static  void* Run            ( vlc_object_t *p_this );
static  void* RunAndDestroy  ( vlc_object_t *p_this );

static input_thread_t * Create  ( vlc_object_t *, input_item_t *,
                                  const char *, bool, sout_instance_t * );
static  int             Init    ( input_thread_t *p_input );
static void             WaitDie   ( input_thread_t *p_input );
static void             End     ( input_thread_t *p_input );
static void             MainLoop( input_thread_t *p_input );

static inline int ControlPopNoLock( input_thread_t *, int *, vlc_value_t * );
static void       ControlReduce( input_thread_t * );
static bool Control( input_thread_t *, int, vlc_value_t );

static int  UpdateFromAccess( input_thread_t * );
static int  UpdateFromDemux( input_thread_t * );

static void UpdateItemLength( input_thread_t *, int64_t i_length );

static void MRLSections( input_thread_t *, char *, int *, int *, int *, int *);

static input_source_t *InputSourceNew( input_thread_t *);
static int  InputSourceInit( input_thread_t *, input_source_t *,
                             const char *, const char *psz_forced_demux );
static void InputSourceClean( input_source_t * );
/* TODO */
//static void InputGetAttachments( input_thread_t *, input_source_t * );
static void SlaveDemux( input_thread_t *p_input );
static void SlaveSeek( input_thread_t *p_input );

static void InputMetaUser( input_thread_t *p_input, vlc_meta_t *p_meta );
static void InputUpdateMeta( input_thread_t *p_input, vlc_meta_t *p_meta );

static void DemuxMeta( input_thread_t *p_input, vlc_meta_t *p_meta, demux_t *p_demux );
static void AccessMeta( input_thread_t * p_input, vlc_meta_t *p_meta );
static void AppendAttachment( int *pi_attachment, input_attachment_t ***ppp_attachment,
                              int i_new, input_attachment_t **pp_new );

static void SubtitleAdd( input_thread_t *p_input, char *psz_subtitle, bool b_forced );

/*****************************************************************************
 * This function creates a new input, and returns a pointer
 * to its description. On error, it returns NULL.
 *
 * Variables for _public_ use:
 * * Get and Set:
 *  - state
 *  - rate,rate-slower, rate-faster
 *  - position, position-offset
 *  - time, time-offset
 *  - title,title-next,title-prev
 *  - chapter,chapter-next, chapter-prev
 *  - program, audio-es, video-es, spu-es
 *  - audio-delay, spu-delay
 *  - bookmark
 * * Get only:
 *  - length
 *  - bookmarks
 *  - seekable (if you can seek, it doesn't say if 'bar display' has be shown
 *    or not, for that check position != 0.0)
 *  - can-pause
 *  - teletext-es to get the index of spu track that is teletext --1 if no teletext)
 * * For intf callback upon changes:
 *  - intf-change
 *  - intf-change-vout for when a vout is created or destroyed
 *  - rate-change for when playback rate changes
 * TODO explain when Callback is called
 * TODO complete this list (?)
 *****************************************************************************/
static input_thread_t *Create( vlc_object_t *p_parent, input_item_t *p_item,
                               const char *psz_header, bool b_quick,
                               sout_instance_t *p_sout )
{
    static const char input_name[] = "input";
    input_thread_t *p_input = NULL;                 /* thread descriptor */
    vlc_value_t val;
    int i;

    /* Allocate descriptor */
    p_input = vlc_custom_create( p_parent, sizeof( *p_input ),
                                 VLC_OBJECT_INPUT, input_name );
    if( p_input == NULL )
        return NULL;

    /* Construct a nice name for the input timer */
    char psz_timer_name[255];
    char * psz_name = input_item_GetName( p_item );
    snprintf( psz_timer_name, sizeof(psz_timer_name),
              "input launching for '%s'", psz_name );

    msg_Dbg( p_input, "Creating an input for '%s'", psz_name);

    free( psz_name );

    /* Start a timer to mesure how long it takes
     * to launch an input */
    stats_TimerStart( p_input, psz_timer_name,
        STATS_TIMER_INPUT_LAUNCHING );

    MALLOC_NULL( p_input->p, input_thread_private_t );
    memset( p_input->p, 0, sizeof( input_thread_private_t ) );

    /* One "randomly" selected input thread is responsible for computing
     * the global stats. Check if there is already someone doing this */
    if( p_input->p_libvlc->p_stats && !b_quick )
    {
        libvlc_priv_t *priv = libvlc_priv (p_input->p_libvlc);
        vlc_mutex_lock( &p_input->p_libvlc->p_stats->lock );
        if( priv->p_stats_computer == NULL )
            priv->p_stats_computer = p_input;
        vlc_mutex_unlock( &p_input->p_libvlc->p_stats->lock );
    }

    p_input->b_preparsing = b_quick;
    p_input->psz_header = psz_header ? strdup( psz_header ) : NULL;

    /* Init events */
    vlc_event_manager_t * p_em = &p_input->p->event_manager;
    vlc_event_manager_init_with_vlc_object( p_em, p_input );
    vlc_event_manager_register_event_type( p_em, vlc_InputStateChanged );
    vlc_event_manager_register_event_type( p_em, vlc_InputSelectedStreamChanged );

    /* Init Common fields */
    p_input->b_eof = false;
    p_input->b_can_pace_control = true;
    p_input->p->i_start = 0;
    p_input->i_time  = 0;
    p_input->p->i_stop  = 0;
    p_input->p->i_run  = 0;
    p_input->p->i_title = 0;
    p_input->p->title   = NULL;
    p_input->p->i_title_offset = p_input->p->i_seekpoint_offset = 0;
    p_input->i_state = INIT_S;
    p_input->p->i_rate  = INPUT_RATE_DEFAULT;
    TAB_INIT( p_input->p->i_bookmark, p_input->p->bookmark );
    TAB_INIT( p_input->p->i_attachment, p_input->p->attachment );
    p_input->p->p_es_out = NULL;
    p_input->p->p_sout  = NULL;
    p_input->p->b_out_pace_control = false;
    p_input->i_pts_delay = 0;

    /* Init Input fields */
    vlc_gc_incref( p_item ); /* Released in Destructor() */
    p_input->p->input.p_item = p_item;
    p_input->p->input.p_access = NULL;
    p_input->p->input.p_stream = NULL;
    p_input->p->input.p_demux  = NULL;
    p_input->p->input.b_title_demux = false;
    p_input->p->input.i_title  = 0;
    p_input->p->input.title    = NULL;
    p_input->p->input.i_title_offset = p_input->p->input.i_seekpoint_offset = 0;
    p_input->p->input.b_can_pace_control = true;
    p_input->p->input.b_can_rate_control = true;
    p_input->p->input.b_rescale_ts = true;
    p_input->p->input.b_eof = false;
    p_input->p->input.i_cr_average = 0;

    vlc_mutex_lock( &p_item->lock );

    if( !p_item->p_stats )
        p_item->p_stats = stats_NewInputStats( p_input );
    vlc_mutex_unlock( &p_item->lock );

    /* No slave */
    p_input->p->i_slave = 0;
    p_input->p->slave   = NULL;

    /* Init control buffer */
    vlc_mutex_init( &p_input->p->lock_control );
    p_input->p->i_control = 0;

    /* Parse input options */
    vlc_mutex_lock( &p_item->lock );
    assert( (int)p_item->optflagc == p_item->i_options );
    for( i = 0; i < p_item->i_options; i++ )
        var_OptionParse( VLC_OBJECT(p_input), p_item->ppsz_options[i],
                         !!(p_item->optflagv[i] & VLC_INPUT_OPTION_TRUSTED) );
    vlc_mutex_unlock( &p_item->lock );

    /* Create Object Variables for private use only */
    input_ConfigVarInit( p_input );

    /* Create Objects variables for public Get and Set */
    input_ControlVarInit( p_input );

    p_input->p->input.i_cr_average = var_GetInteger( p_input, "cr-average" );

    if( !p_input->b_preparsing )
    {
        var_Get( p_input, "bookmarks", &val );
        if( val.psz_string )
        {
            /* FIXME: have a common cfg parsing routine used by sout and others */
            char *psz_parser, *psz_start, *psz_end;
            psz_parser = val.psz_string;
            while( (psz_start = strchr( psz_parser, '{' ) ) )
            {
                 seekpoint_t *p_seekpoint = vlc_seekpoint_New();
                 char backup;
                 psz_start++;
                 psz_end = strchr( psz_start, '}' );
                 if( !psz_end ) break;
                 psz_parser = psz_end + 1;
                 backup = *psz_parser;
                 *psz_parser = 0;
                 *psz_end = ',';
                 while( (psz_end = strchr( psz_start, ',' ) ) )
                 {
                     *psz_end = 0;
                     if( !strncmp( psz_start, "name=", 5 ) )
                     {
                         p_seekpoint->psz_name = strdup(psz_start + 5);
                     }
                     else if( !strncmp( psz_start, "bytes=", 6 ) )
                     {
                         p_seekpoint->i_byte_offset = atoll(psz_start + 6);
                     }
                     else if( !strncmp( psz_start, "time=", 5 ) )
                     {
                         p_seekpoint->i_time_offset = atoll(psz_start + 5) *
                                                        1000000;
                     }
                     psz_start = psz_end + 1;
                }
                msg_Dbg( p_input, "adding bookmark: %s, bytes=%"PRId64", time=%"PRId64,
                                  p_seekpoint->psz_name, p_seekpoint->i_byte_offset,
                                  p_seekpoint->i_time_offset );
                input_Control( p_input, INPUT_ADD_BOOKMARK, p_seekpoint );
                vlc_seekpoint_Delete( p_seekpoint );
                *psz_parser = backup;
            }
            free( val.psz_string );
        }
    }

    /* Remove 'Now playing' info as it is probably outdated */
    input_item_SetNowPlaying( p_item, NULL );

    /* */
    if( p_input->b_preparsing )
        p_input->i_flags |= OBJECT_FLAGS_QUIET | OBJECT_FLAGS_NOINTERACT;

    /* */
    if( p_sout )
        p_input->p->p_sout = p_sout;

    memset( &p_input->p->counters, 0, sizeof( p_input->p->counters ) );
    vlc_mutex_init( &p_input->p->counters.counters_lock );

    /* Set the destructor when we are sure we are initialized */
    vlc_object_set_destructor( p_input, (vlc_destructor_t)Destructor );

    /* Attach only once we are ready */
    vlc_object_attach( p_input, p_parent );

    return p_input;
}

/**
 * Input destructor (called when the object's refcount reaches 0).
 */
static void Destructor( input_thread_t * p_input )
{
    input_thread_private_t *priv = p_input->p;

#ifndef NDEBUG
    char * psz_name = input_item_GetName( p_input->p->input.p_item );
    msg_Dbg( p_input, "Destroying the input for '%s'", psz_name);
    free( psz_name );
#endif

    vlc_event_manager_fini( &p_input->p->event_manager );

    stats_TimerDump( p_input, STATS_TIMER_INPUT_LAUNCHING );
    stats_TimerClean( p_input, STATS_TIMER_INPUT_LAUNCHING );
#ifdef ENABLE_SOUT
    if( priv->p_sout )
        sout_DeleteInstance( priv->p_sout );
#endif
    vlc_gc_decref( p_input->p->input.p_item );

    vlc_mutex_destroy( &p_input->p->counters.counters_lock );

    vlc_mutex_destroy( &priv->lock_control );
    free( priv );
}

/**
 * Initialize an input thread and run it. You will need to monitor the
 * thread to clean up after it is done
 *
 * \param p_parent a vlc_object
 * \param p_item an input item
 * \return a pointer to the spawned input thread
 */
input_thread_t *__input_CreateThread( vlc_object_t *p_parent,
                                      input_item_t *p_item )
{
    return __input_CreateThreadExtended( p_parent, p_item, NULL, NULL );
}

/* */
input_thread_t *__input_CreateThreadExtended( vlc_object_t *p_parent,
                                              input_item_t *p_item,
                                              const char *psz_log, sout_instance_t *p_sout )
{
    input_thread_t *p_input;

    p_input = Create( p_parent, p_item, psz_log, false, p_sout );
    if( !p_input )
        return NULL;

    /* Create thread and wait for its readiness. */
    if( vlc_thread_create( p_input, "input", Run,
                           VLC_THREAD_PRIORITY_INPUT, true ) )
    {
        input_ChangeState( p_input, ERROR_S );
        msg_Err( p_input, "cannot create input thread" );
        vlc_object_detach( p_input );
        vlc_object_release( p_input );
        return NULL;
    }

    return p_input;
}

/**
 * Initialize an input thread and run it. This thread will clean after itself,
 * you can forget about it. It can work either in blocking or non-blocking mode
 *
 * \param p_parent a vlc_object
 * \param p_item an input item
 * \param b_block should we block until read is finished ?
 * \return the input object id if non blocking, an error code else
 */
int __input_Read( vlc_object_t *p_parent, input_item_t *p_item,
                   bool b_block )
{
    input_thread_t *p_input;

    p_input = Create( p_parent, p_item, NULL, false, NULL );
    if( !p_input )
        return VLC_EGENERIC;

    if( b_block )
    {
        RunAndDestroy( VLC_OBJECT(p_input) );
        return VLC_SUCCESS;
    }
    else
    {
        if( vlc_thread_create( p_input, "input", RunAndDestroy,
                               VLC_THREAD_PRIORITY_INPUT, true ) )
        {
            input_ChangeState( p_input, ERROR_S );
            msg_Err( p_input, "cannot create input thread" );
            vlc_object_release( p_input );
            return VLC_EGENERIC;
        }
    }
    return p_input->i_object_id;
}

/**
 * Initialize an input and initialize it to preparse the item
 * This function is blocking. It will only accept to parse files
 *
 * \param p_parent a vlc_object_t
 * \param p_item an input item
 * \return VLC_SUCCESS or an error
 */
int __input_Preparse( vlc_object_t *p_parent, input_item_t *p_item )
{
    input_thread_t *p_input;

    /* Allocate descriptor */
    p_input = Create( p_parent, p_item, NULL, true, NULL );
    if( !p_input )
        return VLC_EGENERIC;

    if( !Init( p_input ) )
        End( p_input );

    vlc_object_detach( p_input );
    vlc_object_release( p_input );

    return VLC_SUCCESS;
}

/**
 * Request a running input thread to stop and die
 *
 * \param the input thread to stop
 */
static void ObjectKillChildrens( input_thread_t *p_input, vlc_object_t *p_obj )
{
    vlc_list_t *p_list;
    int i;

    if( p_obj->i_object_type == VLC_OBJECT_VOUT ||
        p_obj->i_object_type == VLC_OBJECT_AOUT ||
        p_obj == VLC_OBJECT(p_input->p->p_sout) )
        return;

    vlc_object_kill( p_obj );

    p_list = vlc_list_children( p_obj );
    for( i = 0; i < p_list->i_count; i++ )
        ObjectKillChildrens( p_input, p_list->p_values[i].p_object );
    vlc_list_release( p_list );
}
void input_StopThread( input_thread_t *p_input )
{
    /* Set die for input and ALL of this childrens (even (grand-)grand-childrens)
     * It is needed here even if it is done in INPUT_CONTROL_SET_DIE handler to
     * unlock the control loop */
    ObjectKillChildrens( p_input, VLC_OBJECT(p_input) );

    input_ControlPush( p_input, INPUT_CONTROL_SET_DIE, NULL );
}

sout_instance_t * input_DetachSout( input_thread_t *p_input )
{
    sout_instance_t *p_sout = p_input->p->p_sout;
    vlc_object_detach( p_sout );
    p_input->p->p_sout = NULL;
    return p_sout;
}

/*****************************************************************************
 * Run: main thread loop
 * This is the "normal" thread that spawns the input processing chain,
 * reads the stream, cleans up and waits
 *****************************************************************************/
static void* Run( vlc_object_t *p_this )
{
    input_thread_t *p_input = (input_thread_t *)p_this;
    /* Signal that the thread is launched */
    vlc_thread_ready( p_input );

    if( Init( p_input ) )
    {
        /* If we failed, wait before we are killed, and exit */
        WaitDie( p_input );

        /* Tell we're dead */
        p_input->b_dead = true;

        return NULL;
    }

    MainLoop( p_input );

    if( !p_input->b_eof && !p_input->b_error && p_input->p->input.b_eof )
    {
        /* We have finish to demux data but not to play them */
        while( !p_input->b_die )
        {
            if( input_EsOutDecodersEmpty( p_input->p->p_es_out ) )
                break;

            msg_Dbg( p_input, "waiting decoder fifos to empty" );

            msleep( INPUT_IDLE_SLEEP );
        }

        /* We have finished */
        input_ChangeState( p_input, END_S );
    }

    /* Wait until we are asked to die */
    if( !p_input->b_die )
    {
        WaitDie( p_input );
    }

    /* Clean up */
    End( p_input );

    return NULL;
}

/*****************************************************************************
 * RunAndDestroy: main thread loop
 * This is the "just forget me" thread that spawns the input processing chain,
 * reads the stream, cleans up and releases memory
 *****************************************************************************/
static void* RunAndDestroy( vlc_object_t *p_this )
{
    input_thread_t *p_input = (input_thread_t *)p_this;
    /* Signal that the thread is launched */
    vlc_thread_ready( p_input );

    if( Init( p_input ) )
        goto exit;

    MainLoop( p_input );

    if( !p_input->b_eof && !p_input->b_error && p_input->p->input.b_eof )
    {
        /* We have finished demuxing data but not playing it */
        while( !p_input->b_die )
        {
            if( input_EsOutDecodersEmpty( p_input->p->p_es_out ) )
                break;

            msg_Dbg( p_input, "waiting decoder fifos to empty" );

            msleep( INPUT_IDLE_SLEEP );
        }

        /* We have finished */
        input_ChangeState( p_input, END_S );
    }

    /* Clean up */
    End( p_input );

exit:
    /* Release memory */
    vlc_object_release( p_input );
    return 0;
}

/*****************************************************************************
 * Main loop: Fill buffers from access, and demux
 *****************************************************************************/
static void MainLoop( input_thread_t *p_input )
{
    int64_t i_start_mdate = mdate();
    int64_t i_intf_update = 0;
    int i_updates = 0;

    /* Stop the timer */
    stats_TimerStop( p_input, STATS_TIMER_INPUT_LAUNCHING );

    while( !p_input->b_die && !p_input->b_error && !p_input->p->input.b_eof )
    {
        bool b_force_update = false;
        int i_ret;
        int i_type;
        vlc_value_t val;

        /* Do the read */
        if( p_input->i_state != PAUSE_S )
        {
            if( ( p_input->p->i_stop > 0 && p_input->i_time >= p_input->p->i_stop ) ||
                ( p_input->p->i_run > 0 && i_start_mdate+p_input->p->i_run < mdate() ) )
                i_ret = 0; /* EOF */
            else
                i_ret = p_input->p->input.p_demux->pf_demux(p_input->p->input.p_demux);

            if( i_ret > 0 )
            {
                /* TODO */
                if( p_input->p->input.b_title_demux &&
                    p_input->p->input.p_demux->info.i_update )
                {
                    i_ret = UpdateFromDemux( p_input );
                    b_force_update = true;
                }
                else if( !p_input->p->input.b_title_demux &&
                          p_input->p->input.p_access &&
                          p_input->p->input.p_access->info.i_update )
                {
                    i_ret = UpdateFromAccess( p_input );
                    b_force_update = true;
                }
            }

            if( i_ret == 0 )    /* EOF */
            {
                vlc_value_t repeat;

                var_Get( p_input, "input-repeat", &repeat );
                if( repeat.i_int == 0 )
                {
                    /* End of file - we do not set b_die because only the
                     * playlist is allowed to do so. */
                    msg_Dbg( p_input, "EOF reached" );
                    p_input->p->input.b_eof = true;
                }
                else
                {
                    msg_Dbg( p_input, "repeating the same input (%d)",
                             repeat.i_int );
                    if( repeat.i_int > 0 )
                    {
                        repeat.i_int--;
                        var_Set( p_input, "input-repeat", repeat );
                    }

                    /* Seek to start title/seekpoint */
                    val.i_int = p_input->p->input.i_title_start -
                        p_input->p->input.i_title_offset;
                    if( val.i_int < 0 || val.i_int >= p_input->p->input.i_title )
                        val.i_int = 0;
                    input_ControlPush( p_input,
                                       INPUT_CONTROL_SET_TITLE, &val );

                    val.i_int = p_input->p->input.i_seekpoint_start -
                        p_input->p->input.i_seekpoint_offset;
                    if( val.i_int > 0 /* TODO: check upper boundary */ )
                        input_ControlPush( p_input,
                                           INPUT_CONTROL_SET_SEEKPOINT, &val );

                    /* Seek to start position */
                    if( p_input->p->i_start > 0 )
                    {
                        val.i_time = p_input->p->i_start;
                        input_ControlPush( p_input, INPUT_CONTROL_SET_TIME,
                                           &val );
                    }
                    else
                    {
                        val.f_float = 0.0;
                        input_ControlPush( p_input, INPUT_CONTROL_SET_POSITION,
                                           &val );
                    }

                    /* */
                    i_start_mdate = mdate();
                }
            }
            else if( i_ret < 0 )
            {
                input_ChangeState( p_input, ERROR_S );
            }

            if( i_ret > 0 && p_input->p->i_slave > 0 )
            {
                SlaveDemux( p_input );
            }
        }
        else
        {
            /* Small wait */
            msleep( 10*1000 );
        }

        /* Handle control */
        vlc_mutex_lock( &p_input->p->lock_control );
        ControlReduce( p_input );
        while( !ControlPopNoLock( p_input, &i_type, &val ) )
        {
            msg_Dbg( p_input, "control type=%d", i_type );
            if( Control( p_input, i_type, val ) )
                b_force_update = true;
        }
        vlc_mutex_unlock( &p_input->p->lock_control );

        if( b_force_update || i_intf_update < mdate() )
        {
            vlc_value_t val;
            double f_pos;
            int64_t i_time, i_length;
            /* update input status variables */
            if( !demux_Control( p_input->p->input.p_demux,
                                 DEMUX_GET_POSITION, &f_pos ) )
            {
                val.f_float = (float)f_pos;
                var_Change( p_input, "position", VLC_VAR_SETVALUE, &val, NULL );
            }
            if( !demux_Control( p_input->p->input.p_demux,
                                 DEMUX_GET_TIME, &i_time ) )
            {
                p_input->i_time = i_time;
                val.i_time = i_time;
                var_Change( p_input, "time", VLC_VAR_SETVALUE, &val, NULL );
            }
            if( !demux_Control( p_input->p->input.p_demux,
                                 DEMUX_GET_LENGTH, &i_length ) )
            {
                vlc_value_t old_val;
                var_Get( p_input, "length", &old_val );
                val.i_time = i_length;
                var_Change( p_input, "length", VLC_VAR_SETVALUE, &val, NULL );

                if( old_val.i_time != val.i_time )
                {
                    UpdateItemLength( p_input, i_length );
                }
            }

            var_SetBool( p_input, "intf-change", true );
            i_intf_update = mdate() + INT64_C(150000);
        }
        /* 150ms * 8 = ~ 1 second */
        if( ++i_updates % 8 == 0 )
        {
            stats_ComputeInputStats( p_input, p_input->p->input.p_item->p_stats );
            /* Are we the thread responsible for computing global stats ? */
            if( libvlc_priv (p_input->p_libvlc)->p_stats_computer == p_input )
            {
                stats_ComputeGlobalStats( p_input->p_libvlc,
                                          p_input->p_libvlc->p_stats );
            }
        }
    }
}

static void InitStatistics( input_thread_t * p_input )
{
    if( p_input->b_preparsing ) return;

    /* Prepare statistics */
#define INIT_COUNTER( c, type, compute ) p_input->p->counters.p_##c = \
 stats_CounterCreate( p_input, VLC_VAR_##type, STATS_##compute);
    if( libvlc_stats (p_input) )
    {
        INIT_COUNTER( read_bytes, INTEGER, COUNTER );
        INIT_COUNTER( read_packets, INTEGER, COUNTER );
        INIT_COUNTER( demux_read, INTEGER, COUNTER );
        INIT_COUNTER( input_bitrate, FLOAT, DERIVATIVE );
        INIT_COUNTER( demux_bitrate, FLOAT, DERIVATIVE );
        INIT_COUNTER( played_abuffers, INTEGER, COUNTER );
        INIT_COUNTER( lost_abuffers, INTEGER, COUNTER );
        INIT_COUNTER( displayed_pictures, INTEGER, COUNTER );
        INIT_COUNTER( lost_pictures, INTEGER, COUNTER );
        INIT_COUNTER( decoded_audio, INTEGER, COUNTER );
        INIT_COUNTER( decoded_video, INTEGER, COUNTER );
        INIT_COUNTER( decoded_sub, INTEGER, COUNTER );
        p_input->p->counters.p_sout_send_bitrate = NULL;
        p_input->p->counters.p_sout_sent_packets = NULL;
        p_input->p->counters.p_sout_sent_bytes = NULL;
        if( p_input->p->counters.p_demux_bitrate )
            p_input->p->counters.p_demux_bitrate->update_interval = 1000000;
        if( p_input->p->counters.p_input_bitrate )
            p_input->p->counters.p_input_bitrate->update_interval = 1000000;
    }
}

#ifdef ENABLE_SOUT
static int InitSout( input_thread_t * p_input )
{
    char *psz;

    if( p_input->b_preparsing ) return VLC_SUCCESS;

    /* Find a usable sout and attach it to p_input */
    psz = var_GetNonEmptyString( p_input, "sout" );
    if( psz && strncasecmp( p_input->p->input.p_item->psz_uri, "vlc:", 4 ) )
    {
        /* Check the validity of the provided sout */
        if( p_input->p->p_sout )
        {
            if( strcmp( p_input->p->p_sout->psz_sout, psz ) )
            {
                msg_Dbg( p_input, "destroying unusable sout" );

                sout_DeleteInstance( p_input->p->p_sout );
                p_input->p->p_sout = NULL;
            }
        }

        if( p_input->p->p_sout )
        {
            /* Reuse it */
            msg_Dbg( p_input, "sout keep: reusing sout" );
            msg_Dbg( p_input, "sout keep: you probably want to use "
                              "gather stream_out" );
            vlc_object_attach( p_input->p->p_sout, p_input );
        }
        else
        {
            /* Create a new one */
            p_input->p->p_sout = sout_NewInstance( p_input, psz );
            if( !p_input->p->p_sout )
            {
                input_ChangeState( p_input, ERROR_S );
                msg_Err( p_input, "cannot start stream output instance, " \
                                  "aborting" );
                free( psz );
                return VLC_EGENERIC;
            }
        }
        if( libvlc_stats (p_input) )
        {
            INIT_COUNTER( sout_sent_packets, INTEGER, COUNTER );
            INIT_COUNTER (sout_sent_bytes, INTEGER, COUNTER );
            INIT_COUNTER( sout_send_bitrate, FLOAT, DERIVATIVE );
            if( p_input->p->counters.p_sout_send_bitrate )
                 p_input->p->counters.p_sout_send_bitrate->update_interval =
                         1000000;
        }
    }
    else if( p_input->p->p_sout )
    {
        msg_Dbg( p_input, "destroying useless sout" );

        sout_DeleteInstance( p_input->p->p_sout );
        p_input->p->p_sout = NULL;
    }
    free( psz );

    return VLC_SUCCESS;
}
#endif

static void InitTitle( input_thread_t * p_input )
{
    vlc_value_t val;

    if( p_input->b_preparsing ) return;

    /* Create global title (from master) */
    p_input->p->i_title = p_input->p->input.i_title;
    p_input->p->title   = p_input->p->input.title;
    p_input->p->i_title_offset = p_input->p->input.i_title_offset;
    p_input->p->i_seekpoint_offset = p_input->p->input.i_seekpoint_offset;
    if( p_input->p->i_title > 0 )
    {
        /* Setup variables */
        input_ControlVarNavigation( p_input );
        input_ControlVarTitle( p_input, 0 );
    }

    /* Global flag */
    p_input->b_can_pace_control = p_input->p->input.b_can_pace_control;
    p_input->p->b_can_pause        = p_input->p->input.b_can_pause;
    p_input->p->b_can_rate_control = p_input->p->input.b_can_rate_control;

    /* Fix pts delay */
    if( p_input->i_pts_delay < 0 )
        p_input->i_pts_delay = 0;

    /* If the desynchronisation requested by the user is < 0, we need to
     * cache more data. */
    var_Get( p_input, "audio-desync", &val );
    if( val.i_int < 0 ) p_input->i_pts_delay -= (val.i_int * 1000);

    /* Update cr_average depending on the caching */
    p_input->p->input.i_cr_average *= (10 * p_input->i_pts_delay / 200000);
    p_input->p->input.i_cr_average /= 10;
    if( p_input->p->input.i_cr_average < 10 ) p_input->p->input.i_cr_average = 10;
}

static void StartTitle( input_thread_t * p_input )
{
    double f_fps;
    vlc_value_t val;
    int i_delay;
    char *psz;
    char *psz_subtitle;
    int64_t i_length;

    /* Start title/chapter */

    if( p_input->b_preparsing )
    {
        p_input->p->i_start = 0;
        return;
    }

    val.i_int = p_input->p->input.i_title_start -
                p_input->p->input.i_title_offset;
    if( val.i_int > 0 && val.i_int < p_input->p->input.i_title )
        input_ControlPush( p_input, INPUT_CONTROL_SET_TITLE, &val );
    val.i_int = p_input->p->input.i_seekpoint_start -
                p_input->p->input.i_seekpoint_offset;
    if( val.i_int > 0 /* TODO: check upper boundary */ )
        input_ControlPush( p_input, INPUT_CONTROL_SET_SEEKPOINT, &val );

    /* Start time*/
    /* Set start time */
    p_input->p->i_start = INT64_C(1000000) * var_GetInteger( p_input, "start-time" );
    p_input->p->i_stop  = INT64_C(1000000) * var_GetInteger( p_input, "stop-time" );
    p_input->p->i_run   = INT64_C(1000000) * var_GetInteger( p_input, "run-time" );
    i_length = var_GetTime( p_input, "length" );
    if( p_input->p->i_run < 0 )
    {
        msg_Warn( p_input, "invalid run-time ignored" );
        p_input->p->i_run = 0;
    }

    if( p_input->p->i_start > 0 )
    {
        if( p_input->p->i_start >= i_length )
        {
            msg_Warn( p_input, "invalid start-time ignored" );
        }
        else
        {
            vlc_value_t s;

            msg_Dbg( p_input, "starting at time: %ds",
                              (int)( p_input->p->i_start / INT64_C(1000000) ) );

            s.i_time = p_input->p->i_start;
            input_ControlPush( p_input, INPUT_CONTROL_SET_TIME, &s );
        }
    }
    if( p_input->p->i_stop > 0 && p_input->p->i_stop <= p_input->p->i_start )
    {
        msg_Warn( p_input, "invalid stop-time ignored" );
        p_input->p->i_stop = 0;
    }

    /* Load subtitles */
    /* Get fps and set it if not already set */
    if( !demux_Control( p_input->p->input.p_demux, DEMUX_GET_FPS, &f_fps ) &&
        f_fps > 1.0 )
    {
        float f_requested_fps;

        var_Create( p_input, "sub-original-fps", VLC_VAR_FLOAT );
        var_SetFloat( p_input, "sub-original-fps", f_fps );

        f_requested_fps = var_CreateGetFloat( p_input, "sub-fps" );
        if( f_requested_fps != f_fps )
        {
            var_Create( p_input, "sub-fps", VLC_VAR_FLOAT|
                                            VLC_VAR_DOINHERIT );
            var_SetFloat( p_input, "sub-fps", f_requested_fps );
        }
    }

    i_delay = var_CreateGetInteger( p_input, "sub-delay" );
    if( i_delay != 0 )
    {
        var_SetTime( p_input, "spu-delay", (mtime_t)i_delay * 100000 );
    }

    /* Look for and add subtitle files */
    psz_subtitle = var_GetNonEmptyString( p_input, "sub-file" );
    if( psz_subtitle != NULL )
    {
        msg_Dbg( p_input, "forced subtitle: %s", psz_subtitle );
        SubtitleAdd( p_input, psz_subtitle, true );
    }

    var_Get( p_input, "sub-autodetect-file", &val );
    if( val.b_bool )
    {
        char *psz_autopath = var_GetNonEmptyString( p_input, "sub-autodetect-path" );
        char **ppsz_subs = subtitles_Detect( p_input, psz_autopath,
                                             p_input->p->input.p_item->psz_uri );
        free( psz_autopath );

        for( int i = 0; ppsz_subs && ppsz_subs[i]; i++ )
        {
            /* Try to autoselect the first autodetected subtitles file
             * if no subtitles file was specified */
            bool b_forced = i == 0 && !psz_subtitle;

            if( !psz_subtitle || strcmp( psz_subtitle, ppsz_subs[i] ) )
                SubtitleAdd( p_input, ppsz_subs[i], b_forced );

            free( ppsz_subs[i] );
        }
        free( ppsz_subs );
    }
    free( psz_subtitle );

    /* Look for slave */
    psz = var_GetNonEmptyString( p_input, "input-slave" );
    if( psz != NULL )
    {
        char *psz_delim;
        input_source_t *slave;
        while( psz && *psz )
        {
            while( *psz == ' ' || *psz == '#' )
            {
                psz++;
            }
            if( ( psz_delim = strchr( psz, '#' ) ) )
            {
                *psz_delim++ = '\0';
            }
            if( *psz == 0 )
            {
                break;
            }

            msg_Dbg( p_input, "adding slave input '%s'", psz );
            slave = InputSourceNew( p_input );
            if( !InputSourceInit( p_input, slave, psz, NULL ) )
            {
                TAB_APPEND( p_input->p->i_slave, p_input->p->slave, slave );
            }
            else free( slave );
            psz = psz_delim;
        }
        free( psz );
    }
}

static void InitPrograms( input_thread_t * p_input )
{
    int i_es_out_mode;
    vlc_value_t val;

    if( p_input->b_preparsing ) return;

    /* Set up es_out */
    es_out_Control( p_input->p->p_es_out, ES_OUT_SET_ACTIVE, true );
    i_es_out_mode = ES_OUT_MODE_AUTO;
    val.p_list = NULL;
    if( p_input->p->p_sout )
    {
        var_Get( p_input, "sout-all", &val );
        if ( val.b_bool )
        {
            i_es_out_mode = ES_OUT_MODE_ALL;
            val.p_list = NULL;
        }
        else
        {
            var_Get( p_input, "programs", &val );
            if ( val.p_list && val.p_list->i_count )
            {
                i_es_out_mode = ES_OUT_MODE_PARTIAL;
                /* Note : we should remove the "program" callback. */
            }
            else
                var_Change( p_input, "programs", VLC_VAR_FREELIST, &val,
                            NULL );
        }
    }
    es_out_Control( p_input->p->p_es_out, ES_OUT_SET_MODE, i_es_out_mode );

    /* Inform the demuxer about waited group (needed only for DVB) */
    if( i_es_out_mode == ES_OUT_MODE_ALL )
    {
        demux_Control( p_input->p->input.p_demux, DEMUX_SET_GROUP, -1, NULL );
    }
    else if( i_es_out_mode == ES_OUT_MODE_PARTIAL )
    {
        demux_Control( p_input->p->input.p_demux, DEMUX_SET_GROUP, -1,
                        val.p_list );
    }
    else
    {
        demux_Control( p_input->p->input.p_demux, DEMUX_SET_GROUP,
                       (int) var_GetInteger( p_input, "program" ), NULL );
    }
}

static int Init( input_thread_t * p_input )
{
    vlc_meta_t *p_meta;
    vlc_value_t val;
    int i, ret;

    for( i = 0; i < p_input->p->input.p_item->i_options; i++ )
    {
        if( !strncmp( p_input->p->input.p_item->ppsz_options[i], "meta-file", 9 ) )
        {
            msg_Dbg( p_input, "Input is a meta file: disabling unneeded options" );
            var_SetString( p_input, "sout", "" );
            var_SetBool( p_input, "sout-all", false );
            var_SetString( p_input, "input-slave", "" );
            var_SetInteger( p_input, "input-repeat", 0 );
            var_SetString( p_input, "sub-file", "" );
            var_SetBool( p_input, "sub-autodetect-file", false );
        }
    }

    InitStatistics( p_input );
#ifdef ENABLE_SOUT
    ret = InitSout( p_input );
    if( ret != VLC_SUCCESS )
        return ret; /* FIXME: goto error; should be better here */
#endif

    /* Create es out */
    p_input->p->p_es_out = input_EsOutNew( p_input, p_input->p->i_rate );
    es_out_Control( p_input->p->p_es_out, ES_OUT_SET_ACTIVE, false );
    es_out_Control( p_input->p->p_es_out, ES_OUT_SET_MODE, ES_OUT_MODE_NONE );

    var_Create( p_input, "bit-rate", VLC_VAR_INTEGER );
    var_Create( p_input, "sample-rate", VLC_VAR_INTEGER );

    if( InputSourceInit( p_input, &p_input->p->input,
                         p_input->p->input.p_item->psz_uri, NULL ) )
    {
        goto error;
    }

    InitTitle( p_input );

    /* Load master infos */
    /* Init length */
    if( !demux_Control( p_input->p->input.p_demux, DEMUX_GET_LENGTH,
                         &val.i_time ) && val.i_time > 0 )
    {
        var_Change( p_input, "length", VLC_VAR_SETVALUE, &val, NULL );
        UpdateItemLength( p_input, val.i_time );
    }
    else
    {
        val.i_time = input_item_GetDuration( p_input->p->input.p_item );
        if( val.i_time > 0 )
        { /* fallback: gets length from metadata */
            var_Change( p_input, "length", VLC_VAR_SETVALUE, &val, NULL );
            UpdateItemLength( p_input, val.i_time );
        }
    }

    StartTitle( p_input );

    InitPrograms( p_input );

    if( !p_input->b_preparsing && p_input->p->p_sout )
    {
        p_input->p->b_out_pace_control = (p_input->p->p_sout->i_out_pace_nocontrol > 0);

        if( p_input->b_can_pace_control && p_input->p->b_out_pace_control )
        {
            /* We don't want a high input priority here or we'll
             * end-up sucking up all the CPU time */
            vlc_thread_set_priority( p_input, VLC_THREAD_PRIORITY_LOW );
        }

        msg_Dbg( p_input, "starting in %s mode",
                 p_input->p->b_out_pace_control ? "async" : "sync" );
    }

    p_meta = vlc_meta_New();

    /* Get meta data from users */
    InputMetaUser( p_input, p_meta );

    /* Get meta data from master input */
    DemuxMeta( p_input, p_meta, p_input->p->input.p_demux );

    /* Access_file does not give any meta, and there are no slave */
    AccessMeta( p_input, p_meta );

    InputUpdateMeta( p_input, p_meta );

    if( !p_input->b_preparsing )
    {
        msg_Dbg( p_input, "`%s' successfully opened",
                 p_input->p->input.p_item->psz_uri );

    }

    /* initialization is complete */
    input_ChangeState( p_input, PLAYING_S );

    return VLC_SUCCESS;

error:
    input_ChangeState( p_input, ERROR_S );

    if( p_input->p->p_es_out )
        input_EsOutDelete( p_input->p->p_es_out );
#ifdef ENABLE_SOUT
    if( p_input->p->p_sout )
    {
        vlc_object_detach( p_input->p->p_sout );
        sout_DeleteInstance( p_input->p->p_sout );
    }
#endif

    if( !p_input->b_preparsing && libvlc_stats (p_input) )
    {
#define EXIT_COUNTER( c ) do { if( p_input->p->counters.p_##c ) \
                                   stats_CounterClean( p_input->p->counters.p_##c );\
                               p_input->p->counters.p_##c = NULL; } while(0)
        EXIT_COUNTER( read_bytes );
        EXIT_COUNTER( read_packets );
        EXIT_COUNTER( demux_read );
        EXIT_COUNTER( input_bitrate );
        EXIT_COUNTER( demux_bitrate );
        EXIT_COUNTER( played_abuffers );
        EXIT_COUNTER( lost_abuffers );
        EXIT_COUNTER( displayed_pictures );
        EXIT_COUNTER( lost_pictures );
        EXIT_COUNTER( decoded_audio );
        EXIT_COUNTER( decoded_video );
        EXIT_COUNTER( decoded_sub );

        if( p_input->p->p_sout )
        {
            EXIT_COUNTER( sout_sent_packets );
            EXIT_COUNTER (sout_sent_bytes );
            EXIT_COUNTER( sout_send_bitrate );
        }
#undef EXIT_COUNTER
    }

    /* Mark them deleted */
    p_input->p->input.p_demux = NULL;
    p_input->p->input.p_stream = NULL;
    p_input->p->input.p_access = NULL;
    p_input->p->p_es_out = NULL;
    p_input->p->p_sout = NULL;

    return VLC_EGENERIC;
}

/*****************************************************************************
 * WaitDie: Wait until we are asked to die.
 *****************************************************************************
 * This function is called when an error occurred during thread main's loop.
 *****************************************************************************/
static void WaitDie( input_thread_t *p_input )
{
    input_ChangeState( p_input, p_input->b_error ? ERROR_S : END_S );
    while( !p_input->b_die )
    {
        /* Sleep a while */
        msleep( INPUT_IDLE_SLEEP );
    }
}

/*****************************************************************************
 * End: end the input thread
 *****************************************************************************/
static void End( input_thread_t * p_input )
{
    int i;

    /* We are at the end */
    input_ChangeState( p_input, END_S );

    /* Clean control variables */
    input_ControlVarStop( p_input );

    /* Clean up master */
    InputSourceClean( &p_input->p->input );

    /* Delete slave */
    for( i = 0; i < p_input->p->i_slave; i++ )
    {
        InputSourceClean( p_input->p->slave[i] );
        free( p_input->p->slave[i] );
    }
    free( p_input->p->slave );

    /* Unload all modules */
    if( p_input->p->p_es_out )
        input_EsOutDelete( p_input->p->p_es_out );

    if( !p_input->b_preparsing )
    {
#define CL_CO( c ) stats_CounterClean( p_input->p->counters.p_##c ); p_input->p->counters.p_##c = NULL;
        if( libvlc_stats (p_input) )
        {
            libvlc_priv_t *priv = libvlc_priv (p_input->p_libvlc);

            /* make sure we are up to date */
            stats_ComputeInputStats( p_input, p_input->p->input.p_item->p_stats );
            if( priv->p_stats_computer == p_input )
            {
                stats_ComputeGlobalStats( p_input->p_libvlc,
                                          p_input->p_libvlc->p_stats );
                priv->p_stats_computer = NULL;
            }
            CL_CO( read_bytes );
            CL_CO( read_packets );
            CL_CO( demux_read );
            CL_CO( input_bitrate );
            CL_CO( demux_bitrate );
            CL_CO( played_abuffers );
            CL_CO( lost_abuffers );
            CL_CO( displayed_pictures );
            CL_CO( lost_pictures );
            CL_CO( decoded_audio) ;
            CL_CO( decoded_video );
            CL_CO( decoded_sub) ;
        }

        /* Close optional stream output instance */
        if( p_input->p->p_sout )
        {
            CL_CO( sout_sent_packets );
            CL_CO( sout_sent_bytes );
            CL_CO( sout_send_bitrate );

            vlc_object_detach( p_input->p->p_sout );
        }
#undef CL_CO
    }

    if( p_input->p->i_attachment > 0 )
    {
        for( i = 0; i < p_input->p->i_attachment; i++ )
            vlc_input_attachment_Delete( p_input->p->attachment[i] );
        TAB_CLEAN( p_input->p->i_attachment, p_input->p->attachment );
    }

    /* Tell we're dead */
    p_input->b_dead = true;
}

/*****************************************************************************
 * Control
 *****************************************************************************/
static inline int ControlPopNoLock( input_thread_t *p_input,
                                    int *pi_type, vlc_value_t *p_val )
{
    if( p_input->p->i_control <= 0 )
    {
        return VLC_EGENERIC;
    }

    *pi_type = p_input->p->control[0].i_type;
    *p_val   = p_input->p->control[0].val;

    p_input->p->i_control--;
    if( p_input->p->i_control > 0 )
    {
        int i;

        for( i = 0; i < p_input->p->i_control; i++ )
        {
            p_input->p->control[i].i_type = p_input->p->control[i+1].i_type;
            p_input->p->control[i].val    = p_input->p->control[i+1].val;
        }
    }

    return VLC_SUCCESS;
}

static void ControlReduce( input_thread_t *p_input )
{
    int i;

    if( !p_input )
        return;

    for( i = 1; i < p_input->p->i_control; i++ )
    {
        const int i_lt = p_input->p->control[i-1].i_type;
        const int i_ct = p_input->p->control[i].i_type;

        /* XXX We can't merge INPUT_CONTROL_SET_ES */
/*        msg_Dbg( p_input, "[%d/%d] l=%d c=%d", i, p_input->p->i_control,
                 i_lt, i_ct );
*/
        if( i_lt == i_ct &&
            ( i_ct == INPUT_CONTROL_SET_STATE ||
              i_ct == INPUT_CONTROL_SET_RATE ||
              i_ct == INPUT_CONTROL_SET_POSITION ||
              i_ct == INPUT_CONTROL_SET_TIME ||
              i_ct == INPUT_CONTROL_SET_PROGRAM ||
              i_ct == INPUT_CONTROL_SET_TITLE ||
              i_ct == INPUT_CONTROL_SET_SEEKPOINT ||
              i_ct == INPUT_CONTROL_SET_BOOKMARK ) )
        {
            int j;
//            msg_Dbg( p_input, "merged at %d", i );
            /* Remove the i-1 */
            for( j = i; j <  p_input->p->i_control; j++ )
                p_input->p->control[j-1] = p_input->p->control[j];
            p_input->p->i_control--;
        }
        else
        {
            /* TODO but that's not that important
                - merge SET_X with SET_X_CMD
                - remove SET_SEEKPOINT/SET_POSITION/SET_TIME before a SET_TITLE
                - remove SET_SEEKPOINT/SET_POSITION/SET_TIME before another among them
                - ?
                */
        }
    }
}

static bool Control( input_thread_t *p_input, int i_type,
                           vlc_value_t val )
{
    bool b_force_update = false;

    if( !p_input ) return b_force_update;

    switch( i_type )
    {
        case INPUT_CONTROL_SET_DIE:
            msg_Dbg( p_input, "control: stopping input" );

            /* Mark all submodules to die */
            ObjectKillChildrens( p_input, VLC_OBJECT(p_input) );
            break;

        case INPUT_CONTROL_SET_POSITION:
        case INPUT_CONTROL_SET_POSITION_OFFSET:
        {
            double f_pos;
            if( i_type == INPUT_CONTROL_SET_POSITION )
            {
                f_pos = val.f_float;
            }
            else
            {
                /* Should not fail */
                demux_Control( p_input->p->input.p_demux,
                                DEMUX_GET_POSITION, &f_pos );
                f_pos += val.f_float;
            }
            if( f_pos < 0.0 ) f_pos = 0.0;
            if( f_pos > 1.0 ) f_pos = 1.0;
            /* Reset the decoders states and clock sync (before calling the demuxer */
            input_EsOutChangePosition( p_input->p->p_es_out );
            if( demux_Control( p_input->p->input.p_demux, DEMUX_SET_POSITION,
                                f_pos ) )
            {
                msg_Err( p_input, "INPUT_CONTROL_SET_POSITION(_OFFSET) "
                         "%2.1f%% failed", f_pos * 100 );
            }
            else
            {
                if( p_input->p->i_slave > 0 )
                    SlaveSeek( p_input );

                b_force_update = true;
            }
            break;
        }

        case INPUT_CONTROL_SET_TIME:
        case INPUT_CONTROL_SET_TIME_OFFSET:
        {
            int64_t i_time;
            int i_ret;

            if( i_type == INPUT_CONTROL_SET_TIME )
            {
                i_time = val.i_time;
            }
            else
            {
                /* Should not fail */
                demux_Control( p_input->p->input.p_demux,
                                DEMUX_GET_TIME, &i_time );
                i_time += val.i_time;
            }
            if( i_time < 0 ) i_time = 0;

            /* Reset the decoders states and clock sync (before calling the demuxer */
            input_EsOutChangePosition( p_input->p->p_es_out );

            i_ret = demux_Control( p_input->p->input.p_demux,
                                    DEMUX_SET_TIME, i_time );
            if( i_ret )
            {
                int64_t i_length;

                /* Emulate it with a SET_POS */
                demux_Control( p_input->p->input.p_demux,
                                DEMUX_GET_LENGTH, &i_length );
                if( i_length > 0 )
                {
                    double f_pos = (double)i_time / (double)i_length;
                    i_ret = demux_Control( p_input->p->input.p_demux,
                                            DEMUX_SET_POSITION, f_pos );
                }
            }
            if( i_ret )
            {
                msg_Warn( p_input, "INPUT_CONTROL_SET_TIME(_OFFSET) %"PRId64
                         " failed or not possible", i_time );
            }
            else
            {
                if( p_input->p->i_slave > 0 )
                    SlaveSeek( p_input );

                b_force_update = true;
            }
            break;
        }

        case INPUT_CONTROL_SET_STATE:
            if( ( val.i_int == PLAYING_S && p_input->i_state == PAUSE_S ) ||
                ( val.i_int == PAUSE_S && p_input->i_state == PAUSE_S ) )
            {
                int i_ret;
                if( p_input->p->input.p_access )
                    i_ret = access_Control( p_input->p->input.p_access,
                                             ACCESS_SET_PAUSE_STATE, false );
                else
                    i_ret = demux_Control( p_input->p->input.p_demux,
                                            DEMUX_SET_PAUSE_STATE, false );

                if( i_ret )
                {
                    /* FIXME What to do ? */
                    msg_Warn( p_input, "cannot unset pause -> EOF" );
                    vlc_mutex_unlock( &p_input->p->lock_control );
                    input_ControlPush( p_input, INPUT_CONTROL_SET_DIE, NULL );
                    vlc_mutex_lock( &p_input->p->lock_control );
                }

                b_force_update = true;

                /* Switch to play */
                input_ChangeStateWithVarCallback( p_input, PLAYING_S, false );

                /* */
                if( !i_ret )
                    input_EsOutChangeState( p_input->p->p_es_out );
            }
            else if( val.i_int == PAUSE_S && p_input->i_state == PLAYING_S &&
                     p_input->p->b_can_pause )
            {
                int i_ret, state;
                if( p_input->p->input.p_access )
                    i_ret = access_Control( p_input->p->input.p_access,
                                             ACCESS_SET_PAUSE_STATE, true );
                else
                    i_ret = demux_Control( p_input->p->input.p_demux,
                                            DEMUX_SET_PAUSE_STATE, true );

                b_force_update = true;

                if( i_ret )
                {
                    msg_Warn( p_input, "cannot set pause state" );
                    state = p_input->i_state;
                }
                else
                {
                    state = PAUSE_S;
                }

                /* Switch to new state */
                input_ChangeStateWithVarCallback( p_input, state, false );

                /* */
                if( !i_ret )
                    input_EsOutChangeState( p_input->p->p_es_out );
            }
            else if( val.i_int == PAUSE_S && !p_input->p->b_can_pause )
            {
                b_force_update = true;

                /* Correct "state" value */
                input_ChangeStateWithVarCallback( p_input, p_input->i_state, false );
            }
            else if( val.i_int != PLAYING_S && val.i_int != PAUSE_S )
            {
                msg_Err( p_input, "invalid state in INPUT_CONTROL_SET_STATE" );
            }
            break;

        case INPUT_CONTROL_SET_RATE:
        case INPUT_CONTROL_SET_RATE_SLOWER:
        case INPUT_CONTROL_SET_RATE_FASTER:
        {
            int i_rate;

            if( i_type == INPUT_CONTROL_SET_RATE )
            {
                i_rate = val.i_int;
            }
            else
            {
                static const int ppi_factor[][2] = {
                    {1,64}, {1,32}, {1,16}, {1,8}, {1,4}, {1,3}, {1,2}, {2,3},
                    {1,1},
                    {3,2}, {2,1}, {3,1}, {4,1}, {8,1}, {16,1}, {32,1}, {64,1},
                    {0,0}
                };
                int i_error;
                int i_idx;
                int i;

                i_error = INT_MAX;
                i_idx = -1;
                for( i = 0; ppi_factor[i][0] != 0; i++ )
                {
                    const int i_test_r = INPUT_RATE_DEFAULT * ppi_factor[i][0] / ppi_factor[i][1];
                    const int i_test_e = abs(p_input->p->i_rate - i_test_r);
                    if( i_test_e < i_error )
                    {
                        i_idx = i;
                        i_error = i_test_e;
                    }
                }
                assert( i_idx >= 0 && ppi_factor[i_idx][0] != 0 );

                if( i_type == INPUT_CONTROL_SET_RATE_SLOWER )
                {
                    if( ppi_factor[i_idx+1][0] > 0 )
                        i_rate = INPUT_RATE_DEFAULT * ppi_factor[i_idx+1][0] / ppi_factor[i_idx+1][1];
                    else
                        i_rate = INPUT_RATE_MAX+1;
                }
                else
                {
                    assert( i_type == INPUT_CONTROL_SET_RATE_FASTER );
                    if( i_idx > 0 )
                        i_rate = INPUT_RATE_DEFAULT * ppi_factor[i_idx-1][0] / ppi_factor[i_idx-1][1];
                    else
                        i_rate = INPUT_RATE_MIN-1;
                }
            }

            if( i_rate < INPUT_RATE_MIN )
            {
                msg_Dbg( p_input, "cannot set rate faster" );
                i_rate = INPUT_RATE_MIN;
            }
            else if( i_rate > INPUT_RATE_MAX )
            {
                msg_Dbg( p_input, "cannot set rate slower" );
                i_rate = INPUT_RATE_MAX;
            }
            if( i_rate != INPUT_RATE_DEFAULT &&
                ( ( !p_input->b_can_pace_control && !p_input->p->b_can_rate_control ) ||
                  ( p_input->p->p_sout && !p_input->p->b_out_pace_control ) ) )
            {
                msg_Dbg( p_input, "cannot change rate" );
                i_rate = INPUT_RATE_DEFAULT;
            }
            if( i_rate != p_input->p->i_rate &&
                !p_input->b_can_pace_control && p_input->p->b_can_rate_control )
            {
                int i_ret;
                if( p_input->p->input.p_access )
                    i_ret = VLC_EGENERIC;
                else
                    i_ret = demux_Control( p_input->p->input.p_demux,
                                            DEMUX_SET_RATE, &i_rate );
                if( i_ret )
                {
                    msg_Warn( p_input, "ACCESS/DEMUX_SET_RATE failed" );
                    i_rate = p_input->p->i_rate;
                }
            }

            /* */
            if( i_rate != p_input->p->i_rate )
            {
                val.i_int = i_rate;
                var_Change( p_input, "rate", VLC_VAR_SETVALUE, &val, NULL );
                var_SetBool( p_input, "rate-change", true );

                p_input->p->i_rate  = i_rate;

                /* FIXME do we need a RESET_PCR when !p_input->p->input.b_rescale_ts ? */
                if( p_input->p->input.b_rescale_ts )
                    input_EsOutChangeRate( p_input->p->p_es_out, i_rate );

                b_force_update = true;
            }
            break;
        }

        case INPUT_CONTROL_SET_PROGRAM:
            /* No need to force update, es_out does it if needed */
            es_out_Control( p_input->p->p_es_out,
                            ES_OUT_SET_GROUP, val.i_int );

            demux_Control( p_input->p->input.p_demux, DEMUX_SET_GROUP, val.i_int,
                            NULL );
            break;

        case INPUT_CONTROL_SET_ES:
            /* No need to force update, es_out does it if needed */
            es_out_Control( p_input->p->p_es_out, ES_OUT_SET_ES,
                            input_EsOutGetFromID( p_input->p->p_es_out,
                                                  val.i_int ) );
            break;

        case INPUT_CONTROL_SET_AUDIO_DELAY:
            input_EsOutSetDelay( p_input->p->p_es_out,
                                 AUDIO_ES, val.i_time );
            var_Change( p_input, "audio-delay", VLC_VAR_SETVALUE, &val, NULL );
            break;

        case INPUT_CONTROL_SET_SPU_DELAY:
            input_EsOutSetDelay( p_input->p->p_es_out,
                                 SPU_ES, val.i_time );
            var_Change( p_input, "spu-delay", VLC_VAR_SETVALUE, &val, NULL );
            break;

        case INPUT_CONTROL_SET_TITLE:
        case INPUT_CONTROL_SET_TITLE_NEXT:
        case INPUT_CONTROL_SET_TITLE_PREV:
            if( p_input->p->input.b_title_demux &&
                p_input->p->input.i_title > 0 )
            {
                /* TODO */
                /* FIXME handle demux title */
                demux_t *p_demux = p_input->p->input.p_demux;
                int i_title;

                if( i_type == INPUT_CONTROL_SET_TITLE_PREV )
                    i_title = p_demux->info.i_title - 1;
                else if( i_type == INPUT_CONTROL_SET_TITLE_NEXT )
                    i_title = p_demux->info.i_title + 1;
                else
                    i_title = val.i_int;

                if( i_title >= 0 && i_title < p_input->p->input.i_title )
                {
                    input_EsOutChangePosition( p_input->p->p_es_out );

                    demux_Control( p_demux, DEMUX_SET_TITLE, i_title );
                    input_ControlVarTitle( p_input, i_title );
                }
            }
            else if( p_input->p->input.i_title > 0 )
            {
                access_t *p_access = p_input->p->input.p_access;
                int i_title;

                if( i_type == INPUT_CONTROL_SET_TITLE_PREV )
                    i_title = p_access->info.i_title - 1;
                else if( i_type == INPUT_CONTROL_SET_TITLE_NEXT )
                    i_title = p_access->info.i_title + 1;
                else
                    i_title = val.i_int;

                if( i_title >= 0 && i_title < p_input->p->input.i_title )
                {
                    input_EsOutChangePosition( p_input->p->p_es_out );

                    access_Control( p_access, ACCESS_SET_TITLE, i_title );
                    stream_AccessReset( p_input->p->input.p_stream );
                }
            }
            break;
        case INPUT_CONTROL_SET_SEEKPOINT:
        case INPUT_CONTROL_SET_SEEKPOINT_NEXT:
        case INPUT_CONTROL_SET_SEEKPOINT_PREV:
            if( p_input->p->input.b_title_demux &&
                p_input->p->input.i_title > 0 )
            {
                demux_t *p_demux = p_input->p->input.p_demux;
                int i_seekpoint;
                int64_t i_input_time;
                int64_t i_seekpoint_time;

                if( i_type == INPUT_CONTROL_SET_SEEKPOINT_PREV )
                {
                    i_seekpoint = p_demux->info.i_seekpoint;
                    i_seekpoint_time = p_input->p->input.title[p_demux->info.i_title]->seekpoint[i_seekpoint]->i_time_offset;
                    if( i_seekpoint_time >= 0 &&
                         !demux_Control( p_demux,
                                          DEMUX_GET_TIME, &i_input_time ) )
                    {
                        if ( i_input_time < i_seekpoint_time + 3000000 )
                            i_seekpoint--;
                    }
                    else
                        i_seekpoint--;
                }
                else if( i_type == INPUT_CONTROL_SET_SEEKPOINT_NEXT )
                    i_seekpoint = p_demux->info.i_seekpoint + 1;
                else
                    i_seekpoint = val.i_int;

                if( i_seekpoint >= 0 && i_seekpoint <
                    p_input->p->input.title[p_demux->info.i_title]->i_seekpoint )
                {

                    input_EsOutChangePosition( p_input->p->p_es_out );

                    demux_Control( p_demux, DEMUX_SET_SEEKPOINT, i_seekpoint );
                }
            }
            else if( p_input->p->input.i_title > 0 )
            {
                demux_t *p_demux = p_input->p->input.p_demux;
                access_t *p_access = p_input->p->input.p_access;
                int i_seekpoint;
                int64_t i_input_time;
                int64_t i_seekpoint_time;

                if( i_type == INPUT_CONTROL_SET_SEEKPOINT_PREV )
                {
                    i_seekpoint = p_access->info.i_seekpoint;
                    i_seekpoint_time = p_input->p->input.title[p_access->info.i_title]->seekpoint[i_seekpoint]->i_time_offset;
                    if( i_seekpoint_time >= 0 &&
                        demux_Control( p_demux,
                                        DEMUX_GET_TIME, &i_input_time ) )
                    {
                        if ( i_input_time < i_seekpoint_time + 3000000 )
                            i_seekpoint--;
                    }
                    else
                        i_seekpoint--;
                }
                else if( i_type == INPUT_CONTROL_SET_SEEKPOINT_NEXT )
                    i_seekpoint = p_access->info.i_seekpoint + 1;
                else
                    i_seekpoint = val.i_int;

                if( i_seekpoint >= 0 && i_seekpoint <
                    p_input->p->input.title[p_access->info.i_title]->i_seekpoint )
                {
                    input_EsOutChangePosition( p_input->p->p_es_out );

                    access_Control( p_access, ACCESS_SET_SEEKPOINT,
                                    i_seekpoint );
                    stream_AccessReset( p_input->p->input.p_stream );
                }
            }
            break;

        case INPUT_CONTROL_ADD_SUBTITLE:
            if( val.psz_string )
            {
                SubtitleAdd( p_input, val.psz_string, true );
                free( val.psz_string );
            }
            break;

        case INPUT_CONTROL_ADD_SLAVE:
            if( val.psz_string )
            {
                input_source_t *slave = InputSourceNew( p_input );

                if( !InputSourceInit( p_input, slave, val.psz_string, NULL ) )
                {
                    vlc_meta_t *p_meta;
                    int64_t i_time;

                    /* Add the slave */
                    msg_Dbg( p_input, "adding %s as slave on the fly",
                             val.psz_string );

                    /* Set position */
                    if( demux_Control( p_input->p->input.p_demux,
                                        DEMUX_GET_TIME, &i_time ) )
                    {
                        msg_Err( p_input, "demux doesn't like DEMUX_GET_TIME" );
                        InputSourceClean( slave );
                        free( slave );
                        break;
                    }
                    if( demux_Control( slave->p_demux,
                                        DEMUX_SET_TIME, i_time ) )
                    {
                        msg_Err( p_input, "seek failed for new slave" );
                        InputSourceClean( slave );
                        free( slave );
                        break;
                    }

                    /* Get meta (access and demux) */
                    p_meta = vlc_meta_New();
                    access_Control( slave->p_access, ACCESS_GET_META,
                                     p_meta );
                    demux_Control( slave->p_demux, DEMUX_GET_META, p_meta );
                    InputUpdateMeta( p_input, p_meta );

                    TAB_APPEND( p_input->p->i_slave, p_input->p->slave, slave );
                }
                else
                {
                    free( slave );
                    msg_Warn( p_input, "failed to add %s as slave",
                              val.psz_string );
                }

                free( val.psz_string );
            }
            break;

        case INPUT_CONTROL_SET_BOOKMARK:
        default:
            msg_Err( p_input, "not yet implemented" );
            break;
    }

    return b_force_update;
}

/*****************************************************************************
 * UpdateFromDemux:
 *****************************************************************************/
static int UpdateFromDemux( input_thread_t *p_input )
{
    demux_t *p_demux = p_input->p->input.p_demux;
    vlc_value_t v;

    if( p_demux->info.i_update & INPUT_UPDATE_TITLE )
    {
        v.i_int = p_demux->info.i_title;
        var_Change( p_input, "title", VLC_VAR_SETVALUE, &v, NULL );

        input_ControlVarTitle( p_input, p_demux->info.i_title );

        p_demux->info.i_update &= ~INPUT_UPDATE_TITLE;
    }
    if( p_demux->info.i_update & INPUT_UPDATE_SEEKPOINT )
    {
        v.i_int = p_demux->info.i_seekpoint;
        var_Change( p_input, "chapter", VLC_VAR_SETVALUE, &v, NULL);

        p_demux->info.i_update &= ~INPUT_UPDATE_SEEKPOINT;
    }
    p_demux->info.i_update &= ~INPUT_UPDATE_SIZE;

    /* Hmmm only works with master input */
    if( p_input->p->input.p_demux == p_demux )
    {
        int i_title_end = p_input->p->input.i_title_end -
            p_input->p->input.i_title_offset;
        int i_seekpoint_end = p_input->p->input.i_seekpoint_end -
            p_input->p->input.i_seekpoint_offset;

        if( i_title_end >= 0 && i_seekpoint_end >= 0 )
        {
            if( p_demux->info.i_title > i_title_end ||
                ( p_demux->info.i_title == i_title_end &&
                  p_demux->info.i_seekpoint > i_seekpoint_end ) ) return 0;
        }
        else if( i_seekpoint_end >=0 )
        {
            if( p_demux->info.i_seekpoint > i_seekpoint_end ) return 0;
        }
        else if( i_title_end >= 0 )
        {
            if( p_demux->info.i_title > i_title_end ) return 0;
        }
    }

    return 1;
}

/*****************************************************************************
 * UpdateFromAccess:
 *****************************************************************************/
static int UpdateFromAccess( input_thread_t *p_input )
{
    access_t *p_access = p_input->p->input.p_access;
    vlc_value_t v;

    if( p_access->info.i_update & INPUT_UPDATE_TITLE )
    {
        v.i_int = p_access->info.i_title;
        var_Change( p_input, "title", VLC_VAR_SETVALUE, &v, NULL );

        input_ControlVarTitle( p_input, p_access->info.i_title );

        stream_AccessUpdate( p_input->p->input.p_stream );

        p_access->info.i_update &= ~INPUT_UPDATE_TITLE;
    }
    if( p_access->info.i_update & INPUT_UPDATE_SEEKPOINT )
    {
        v.i_int = p_access->info.i_seekpoint;
        var_Change( p_input, "chapter", VLC_VAR_SETVALUE, &v, NULL);
        p_access->info.i_update &= ~INPUT_UPDATE_SEEKPOINT;
    }
    if( p_access->info.i_update & INPUT_UPDATE_META )
    {
        /* TODO maybe multi - access ? */
        vlc_meta_t *p_meta = vlc_meta_New();
        access_Control( p_input->p->input.p_access,ACCESS_GET_META, p_meta );
        InputUpdateMeta( p_input, p_meta );
        p_access->info.i_update &= ~INPUT_UPDATE_META;
    }

    p_access->info.i_update &= ~INPUT_UPDATE_SIZE;

    /* Hmmm only works with master input */
    if( p_input->p->input.p_access == p_access )
    {
        int i_title_end = p_input->p->input.i_title_end -
            p_input->p->input.i_title_offset;
        int i_seekpoint_end = p_input->p->input.i_seekpoint_end -
            p_input->p->input.i_seekpoint_offset;

        if( i_title_end >= 0 && i_seekpoint_end >=0 )
        {
            if( p_access->info.i_title > i_title_end ||
                ( p_access->info.i_title == i_title_end &&
                  p_access->info.i_seekpoint > i_seekpoint_end ) ) return 0;
        }
        else if( i_seekpoint_end >=0 )
        {
            if( p_access->info.i_seekpoint > i_seekpoint_end ) return 0;
        }
        else if( i_title_end >= 0 )
        {
            if( p_access->info.i_title > i_title_end ) return 0;
        }
    }

    return 1;
}

/*****************************************************************************
 * UpdateItemLength:
 *****************************************************************************/
static void UpdateItemLength( input_thread_t *p_input, int64_t i_length )
{
    input_item_SetDuration( p_input->p->input.p_item, (mtime_t) i_length );
}

/*****************************************************************************
 * InputSourceNew:
 *****************************************************************************/
static input_source_t *InputSourceNew( input_thread_t *p_input )
{
    (void)p_input;
    input_source_t *in = (input_source_t*) malloc( sizeof( input_source_t ) );
    if( in )
        memset( in, 0, sizeof( input_source_t ) );
    return in;
}

/*****************************************************************************
 * InputSourceInit:
 *****************************************************************************/
static int InputSourceInit( input_thread_t *p_input,
                            input_source_t *in, const char *psz_mrl,
                            const char *psz_forced_demux )
{
    const bool b_master = in == &p_input->p->input;
    const char *psz_access;
    const char *psz_demux;
    char *psz_path;
    char *psz_tmp;
    char *psz;
    vlc_value_t val;
    double f_fps;

    if( !in ) return VLC_EGENERIC;
    if( !p_input ) return VLC_EGENERIC;

    char *psz_dup = strdup( psz_mrl );

    if( psz_dup == NULL )
        goto error;

    /* Split uri */
    input_SplitMRL( &psz_access, &psz_demux, &psz_path, psz_dup );

    msg_Dbg( p_input, "`%s' gives access `%s' demux `%s' path `%s'",
             psz_mrl, psz_access, psz_demux, psz_path );
    if( !p_input->b_preparsing )
    {
        /* Hack to allow udp://@:port syntax */
        if( !psz_access ||
            (strncmp( psz_access, "udp", 3 ) &&
             strncmp( psz_access, "rtp", 3 )) )
        {
            /* Find optional titles and seekpoints */
            MRLSections( p_input, psz_path, &in->i_title_start, &in->i_title_end,
                     &in->i_seekpoint_start, &in->i_seekpoint_end );
        }

        if( psz_forced_demux && *psz_forced_demux )
        {
            psz_demux = psz_forced_demux;
        }
        else if( *psz_demux == '\0' )
        {
            /* special hack for forcing a demuxer with --demux=module
             * (and do nothing with a list) */
            char *psz_var_demux = var_GetNonEmptyString( p_input, "demux" );

            if( psz_var_demux != NULL &&
                !strchr(psz_var_demux, ',' ) &&
                !strchr(psz_var_demux, ':' ) )
            {
                psz_demux = psz_var_demux;

                msg_Dbg( p_input, "enforced demux ` %s'", psz_demux );
            }
        }

        /* Try access_demux if no demux given */
        if( *psz_demux == '\0' )
        {
            in->p_demux = demux_New( p_input, psz_access, psz_demux, psz_path,
                                      NULL, p_input->p->p_es_out, false );
        }
    }
    else
    {
        /* Preparsing is only for file:// */
        if( *psz_demux )
            goto error;
        if( !*psz_access ) /* path without scheme:// */
            psz_access = "file";
        if( strcmp( psz_access, "file" ) )
            goto error;
        msg_Dbg( p_input, "trying to pre-parse %s",  psz_path );
    }

    if( in->p_demux )
    {
        int64_t i_pts_delay;

        /* Get infos from access_demux */
        demux_Control( in->p_demux,
                        DEMUX_GET_PTS_DELAY, &i_pts_delay );
        p_input->i_pts_delay = __MAX( p_input->i_pts_delay, i_pts_delay );

        in->b_title_demux = true;
        if( demux_Control( in->p_demux, DEMUX_GET_TITLE_INFO,
                            &in->title, &in->i_title,
                            &in->i_title_offset, &in->i_seekpoint_offset ) )
        {
            in->i_title = 0;
            in->title   = NULL;
        }
        if( demux_Control( in->p_demux, DEMUX_CAN_CONTROL_PACE,
                            &in->b_can_pace_control ) )
            in->b_can_pace_control = false;

        if( !in->b_can_pace_control )
        {
            if( demux_Control( in->p_demux, DEMUX_CAN_CONTROL_RATE,
                                &in->b_can_rate_control, &in->b_rescale_ts ) )
            {
                in->b_can_rate_control = false;
                in->b_rescale_ts = true; /* not used */
            }
        }
        else
        {
            in->b_can_rate_control = true;
            in->b_rescale_ts = true;
        }
        if( demux_Control( in->p_demux, DEMUX_CAN_PAUSE,
                            &in->b_can_pause ) )
            in->b_can_pause = false;
        var_SetBool( p_input, "can-pause", in->b_can_pause );

        int ret = demux_Control( in->p_demux, DEMUX_CAN_SEEK,
                        &val.b_bool );
        if( ret != VLC_SUCCESS )
            val.b_bool = false;
        var_Set( p_input, "seekable", val );
    }
    else
    {
        int64_t i_pts_delay;

        if( b_master )
            input_ChangeState( p_input, OPENING_S );

        /* Now try a real access */
        in->p_access = access_New( p_input, psz_access, psz_demux, psz_path );

        /* Access failed, URL encoded ? */
        if( in->p_access == NULL && strchr( psz_path, '%' ) )
        {
            decode_URI( psz_path );

            msg_Dbg( p_input, "retrying with access `%s' demux `%s' path `%s'",
                     psz_access, psz_demux, psz_path );

            in->p_access = access_New( p_input,
                                        psz_access, psz_demux, psz_path );
        }
        if( in->p_access == NULL )
        {
            msg_Err( p_input, "open of `%s' failed: %s", psz_mrl,
                                                         msg_StackMsg() );
            intf_UserFatal( VLC_OBJECT( p_input), false,
                            _("Your input can't be opened"),
                            _("VLC is unable to open the MRL '%s'."
                            " Check the log for details."), psz_mrl );
            goto error;
        }

        /* */
        psz_tmp = psz = var_GetNonEmptyString( p_input, "access-filter" );
        while( psz && *psz )
        {
            access_t *p_access = in->p_access;
            char *end = strchr( psz, ':' );

            if( end )
                *end++ = '\0';

            in->p_access = access_FilterNew( in->p_access, psz );
            if( in->p_access == NULL )
            {
                in->p_access = p_access;
                msg_Warn( p_input, "failed to insert access filter %s",
                          psz );
            }

            psz = end;
        }
        free( psz_tmp );

        /* Get infos from access */
        if( !p_input->b_preparsing )
        {
            access_Control( in->p_access,
                             ACCESS_GET_PTS_DELAY, &i_pts_delay );
            p_input->i_pts_delay = __MAX( p_input->i_pts_delay, i_pts_delay );

            in->b_title_demux = false;
            if( access_Control( in->p_access, ACCESS_GET_TITLE_INFO,
                                 &in->title, &in->i_title,
                                &in->i_title_offset, &in->i_seekpoint_offset ) )

            {
                in->i_title = 0;
                in->title   = NULL;
            }
            access_Control( in->p_access, ACCESS_CAN_CONTROL_PACE,
                             &in->b_can_pace_control );
            in->b_can_rate_control = in->b_can_pace_control;
            in->b_rescale_ts = true;

            access_Control( in->p_access, ACCESS_CAN_PAUSE,
                             &in->b_can_pause );
            var_SetBool( p_input, "can-pause", in->b_can_pause );
            access_Control( in->p_access, ACCESS_CAN_SEEK,
                             &val.b_bool );
            var_Set( p_input, "seekable", val );
        }

        if( b_master )
            input_ChangeState( p_input, BUFFERING_S );

        /* Create the stream_t */
        in->p_stream = stream_AccessNew( in->p_access, p_input->b_preparsing );
        if( in->p_stream == NULL )
        {
            msg_Warn( p_input, "cannot create a stream_t from access" );
            goto error;
        }

        /* Open a demuxer */
        if( *psz_demux == '\0' && *in->p_access->psz_demux )
        {
            psz_demux = in->p_access->psz_demux;
        }

        {
            /* Take access redirections into account */
            char *psz_real_path;
            char *psz_buf = NULL;
            if( in->p_access->psz_path )
            {
                const char *psz_a, *psz_d;
                psz_buf = strdup( in->p_access->psz_path );
                input_SplitMRL( &psz_a, &psz_d, &psz_real_path, psz_buf );
            }
            else
            {
                psz_real_path = psz_path;
            }
            in->p_demux = demux_New( p_input, psz_access, psz_demux,
                                      psz_real_path,
                                      in->p_stream, p_input->p->p_es_out,
                                      p_input->b_preparsing );
            free( psz_buf );
        }

        if( in->p_demux == NULL )
        {
            msg_Err( p_input, "no suitable demux module for `%s/%s://%s'",
                     psz_access, psz_demux, psz_path );
            intf_UserFatal( VLC_OBJECT( p_input ), false,
                            _("VLC can't recognize the input's format"),
                            _("The format of '%s' cannot be detected. "
                            "Have a look the log for details."), psz_mrl );
            goto error;
        }

        /* Get title from demux */
        if( !p_input->b_preparsing && in->i_title <= 0 )
        {
            if( demux_Control( in->p_demux, DEMUX_GET_TITLE_INFO,
                                &in->title, &in->i_title,
                                &in->i_title_offset, &in->i_seekpoint_offset ))
            {
                TAB_INIT( in->i_title, in->title );
            }
            else
            {
                in->b_title_demux = true;
            }
        }
    }

    free( psz_dup );

    /* get attachment
     * FIXME improve for b_preparsing: move it after GET_META and check psz_arturl */
    if( 1 || !p_input->b_preparsing )
    {
        int i_attachment;
        input_attachment_t **attachment;
        if( !demux_Control( in->p_demux, DEMUX_GET_ATTACHMENTS,
                             &attachment, &i_attachment ) )
        {
            vlc_mutex_lock( &p_input->p->input.p_item->lock );
            AppendAttachment( &p_input->p->i_attachment, &p_input->p->attachment,
                              i_attachment, attachment );
            vlc_mutex_unlock( &p_input->p->input.p_item->lock );
        }
    }
    if( !demux_Control( in->p_demux, DEMUX_GET_FPS, &f_fps ) )
    {
        vlc_mutex_lock( &p_input->p->input.p_item->lock );
        in->f_fps = f_fps;
        vlc_mutex_unlock( &p_input->p->input.p_item->lock );
    }

    if( var_GetInteger( p_input, "clock-synchro" ) != -1 )
        in->b_can_pace_control = !var_GetInteger( p_input, "clock-synchro" );

    return VLC_SUCCESS;

error:
    if( b_master )
        input_ChangeState( p_input, ERROR_S );

    if( in->p_demux )
        demux_Delete( in->p_demux );

    if( in->p_stream )
        stream_Delete( in->p_stream );

    if( in->p_access )
        access_Delete( in->p_access );
    free( psz_dup );

    return VLC_EGENERIC;
}

/*****************************************************************************
 * InputSourceClean:
 *****************************************************************************/
static void InputSourceClean( input_source_t *in )
{
    int i;

    if( in->p_demux )
        demux_Delete( in->p_demux );

    if( in->p_stream )
        stream_Delete( in->p_stream );

    if( in->p_access )
        access_Delete( in->p_access );

    if( in->i_title > 0 )
    {
        for( i = 0; i < in->i_title; i++ )
            vlc_input_title_Delete( in->title[i] );
        TAB_CLEAN( in->i_title, in->title );
    }
}

static void SlaveDemux( input_thread_t *p_input )
{
    int64_t i_time;
    int i;

    if( demux_Control( p_input->p->input.p_demux, DEMUX_GET_TIME, &i_time ) )
    {
        msg_Err( p_input, "demux doesn't like DEMUX_GET_TIME" );
        return;
    }

    for( i = 0; i < p_input->p->i_slave; i++ )
    {
        input_source_t *in = p_input->p->slave[i];
        int i_ret = 1;

        if( in->b_eof )
            continue;

        if( demux_Control( in->p_demux, DEMUX_SET_NEXT_DEMUX_TIME, i_time ) )
        {
            for( ;; )
            {
                int64_t i_stime;
                if( demux_Control( in->p_demux, DEMUX_GET_TIME, &i_stime ) )
                {
                    msg_Err( p_input, "slave[%d] doesn't like "
                             "DEMUX_GET_TIME -> EOF", i );
                    i_ret = 0;
                    break;
                }

                if( i_stime >= i_time )
                    break;

                if( ( i_ret = in->p_demux->pf_demux( in->p_demux ) ) <= 0 )
                    break;
            }
        }
        else
        {
            i_ret = in->p_demux->pf_demux( in->p_demux );
        }

        if( i_ret <= 0 )
        {
            msg_Dbg( p_input, "slave %d EOF", i );
            in->b_eof = true;
        }
    }
}

static void SlaveSeek( input_thread_t *p_input )
{
    int64_t i_time;
    int i;

    if( !p_input ) return;

    if( demux_Control( p_input->p->input.p_demux, DEMUX_GET_TIME, &i_time ) )
    {
        msg_Err( p_input, "demux doesn't like DEMUX_GET_TIME" );
        return;
    }

    for( i = 0; i < p_input->p->i_slave; i++ )
    {
        input_source_t *in = p_input->p->slave[i];

        if( demux_Control( in->p_demux, DEMUX_SET_TIME, i_time ) )
        {
            if( !in->b_eof )
                msg_Err( p_input, "seek failed for slave %d -> EOF", i );
            in->b_eof = true;
        }
        else
        {
            in->b_eof = false;
        }
    }
}

/*****************************************************************************
 * InputMetaUser:
 *****************************************************************************/
static void InputMetaUser( input_thread_t *p_input, vlc_meta_t *p_meta )
{
    vlc_value_t val;

    if( !p_meta ) return;

    /* Get meta information from user */
#define GET_META( field, s ) \
    var_Get( p_input, (s), &val );  \
    if( *val.psz_string ) \
        vlc_meta_Set( p_meta, vlc_meta_ ## field, val.psz_string ); \
    free( val.psz_string )

    GET_META( Title, "meta-title" );
    GET_META( Artist, "meta-artist" );
    GET_META( Genre, "meta-genre" );
    GET_META( Copyright, "meta-copyright" );
    GET_META( Description, "meta-description" );
    GET_META( Date, "meta-date" );
    GET_META( URL, "meta-url" );
#undef GET_META
}

/*****************************************************************************
 * InputUpdateMeta: merge p_item meta data with p_meta taking care of
 * arturl and locking issue.
 *****************************************************************************/
static void InputUpdateMeta( input_thread_t *p_input, vlc_meta_t *p_meta )
{
    input_item_t *p_item = p_input->p->input.p_item;
    char * psz_arturl = NULL;
    char *psz_title = NULL;
    int i_arturl_event = false;

    if( !p_meta )
        return;

    psz_arturl = input_item_GetArtURL( p_item );

    vlc_mutex_lock( &p_item->lock );
    if( vlc_meta_Get( p_meta, vlc_meta_Title ) && !p_item->b_fixed_name )
        psz_title = strdup( vlc_meta_Get( p_meta, vlc_meta_Title ) );

    vlc_meta_Merge( p_item->p_meta, p_meta );

    if( psz_arturl && *psz_arturl )
    {
        vlc_meta_Set( p_item->p_meta, vlc_meta_ArtworkURL, psz_arturl );
        i_arturl_event = true;
    }

    vlc_meta_Delete( p_meta );

    if( psz_arturl && !strncmp( psz_arturl, "attachment://", strlen("attachment") ) )
    {
        /* Don't look for art cover if sout
         * XXX It can change when sout has meta data support */
        if( p_input->p->p_sout && !p_input->b_preparsing )
        {
            vlc_meta_Set( p_item->p_meta, vlc_meta_ArtworkURL, "" );
            i_arturl_event = true;

        }
        else
            input_ExtractAttachmentAndCacheArt( p_input );
    }
    free( psz_arturl );

    /* A bit ugly */
    p_meta = NULL;
    if( vlc_dictionary_keys_count( &p_item->p_meta->extra_tags ) > 0 )
    {
        p_meta = vlc_meta_New();
        vlc_meta_Merge( p_meta, input_item_GetMetaObject( p_item ) );
    }
    vlc_mutex_unlock( &p_item->lock );

    input_item_SetPreparsed( p_item, true );

    if( i_arturl_event == true )
    {
        vlc_event_t event;

        /* Notify interested third parties */
        event.type = vlc_InputItemMetaChanged;
        event.u.input_item_meta_changed.meta_type = vlc_meta_ArtworkURL;
        vlc_event_send( &p_item->event_manager, &event );
    }

    if( psz_title )
    {
        input_Control( p_input, INPUT_SET_NAME, psz_title );
        free( psz_title );
    }

    /** \todo handle sout meta */
}


static void AppendAttachment( int *pi_attachment, input_attachment_t ***ppp_attachment,
                              int i_new, input_attachment_t **pp_new )
{
    int i_attachment = *pi_attachment;
    input_attachment_t **attachment = *ppp_attachment;
    int i;

    attachment = realloc( attachment,
                          sizeof(input_attachment_t**) * ( i_attachment + i_new ) );
    for( i = 0; i < i_new; i++ )
        attachment[i_attachment++] = pp_new[i];
    free( pp_new );

    /* */
    *pi_attachment = i_attachment;
    *ppp_attachment = attachment;
}

static void AccessMeta( input_thread_t * p_input, vlc_meta_t *p_meta )
{
    int i;

    if( p_input->b_preparsing )
        return;

    if( p_input->p->input.p_access )
        access_Control( p_input->p->input.p_access, ACCESS_GET_META,
                         p_meta );

    /* Get meta data from slave input */
    for( i = 0; i < p_input->p->i_slave; i++ )
    {
        DemuxMeta( p_input, p_meta, p_input->p->slave[i]->p_demux );
        if( p_input->p->slave[i]->p_access )
        {
            access_Control( p_input->p->slave[i]->p_access,
                             ACCESS_GET_META, p_meta );
        }
    }
}

static void DemuxMeta( input_thread_t *p_input, vlc_meta_t *p_meta, demux_t *p_demux )
{
    bool b_bool;
    module_t *p_id3;


#if 0
    /* XXX I am not sure it is a great idea, besides, there is more than that
     * if we want to do it right */
    vlc_mutex_lock( &p_item->lock );
    if( p_item->p_meta && (p_item->p_meta->i_status & ITEM_PREPARSED ) )
    {
        vlc_mutex_unlock( &p_item->lock );
        return;
    }
    vlc_mutex_unlock( &p_item->lock );
#endif

    demux_Control( p_demux, DEMUX_GET_META, p_meta );
    if( demux_Control( p_demux, DEMUX_HAS_UNSUPPORTED_META, &b_bool ) )
        return;
    if( !b_bool )
        return;

    p_demux->p_private = malloc( sizeof( demux_meta_t ) );
    if(! p_demux->p_private )
        return;

    p_id3 = module_Need( p_demux, "meta reader", NULL, 0 );
    if( p_id3 )
    {
        demux_meta_t *p_demux_meta = (demux_meta_t *)p_demux->p_private;

        if( p_demux_meta->p_meta )
        {
            vlc_meta_Merge( p_meta, p_demux_meta->p_meta );
            vlc_meta_Delete( p_demux_meta->p_meta );
        }

        if( p_demux_meta->i_attachments > 0 )
        {
            vlc_mutex_lock( &p_input->p->input.p_item->lock );
            AppendAttachment( &p_input->p->i_attachment, &p_input->p->attachment,
                              p_demux_meta->i_attachments, p_demux_meta->attachments );
            vlc_mutex_unlock( &p_input->p->input.p_item->lock );
        }
        module_Unneed( p_demux, p_id3 );
    }
    free( p_demux->p_private );
}


/*****************************************************************************
 * MRLSplit: parse the access, demux and url part of the
 *           Media Resource Locator.
 *****************************************************************************/
void input_SplitMRL( const char **ppsz_access, const char **ppsz_demux, char **ppsz_path,
                     char *psz_dup )
{
    char *psz_access = NULL;
    char *psz_demux  = NULL;
    char *psz_path;

    /* Either there is an access/demux specification before ://
     * or we have a plain local file path. */
    psz_path = strstr( psz_dup, "://" );
    if( psz_path != NULL )
    {
        *psz_path = '\0';
        psz_path += 3; /* skips "://" */

        /* Separate access from demux (<access>/<demux>://<path>) */
        psz_access = psz_dup;
        psz_demux = strchr( psz_access, '/' );
        if( psz_demux )
            *psz_demux++ = '\0';

        /* We really don't want module name substitution here! */
        if( psz_access[0] == '$' )
            psz_access++;
        if( psz_demux && psz_demux[0] == '$' )
            psz_demux++;
    }
    else
    {
        psz_path = psz_dup;
    }
    *ppsz_access = psz_access ? psz_access : (char*)"";
    *ppsz_demux = psz_demux ? psz_demux : (char*)"";
    *ppsz_path = psz_path;
}

static inline bool next(char ** src)
{
    char *end;
    errno = 0;
    long result = strtol( *src, &end, 0 );
    if( errno != 0 || result >= LONG_MAX || result <= LONG_MIN ||
        end == *src )
    {
        return false;
    }
    *src = end;
    return true;
}

/*****************************************************************************
 * MRLSections: parse title and seekpoint info from the Media Resource Locator.
 *
 * Syntax:
 * [url][@[title-start][:chapter-start][-[title-end][:chapter-end]]]
 *****************************************************************************/
static void MRLSections( input_thread_t *p_input, char *psz_source,
                         int *pi_title_start, int *pi_title_end,
                         int *pi_chapter_start, int *pi_chapter_end )
{
    char *psz, *psz_end, *psz_next, *psz_check;

    *pi_title_start = *pi_title_end = -1;
    *pi_chapter_start = *pi_chapter_end = -1;

    /* Start by parsing titles and chapters */
    if( !psz_source || !( psz = strrchr( psz_source, '@' ) ) ) return;


    /* Check we are really dealing with a title/chapter section */
    psz_check = psz + 1;
    if( !*psz_check ) return;
    if( isdigit(*psz_check) )
        if(!next(&psz_check)) return;
    if( *psz_check != ':' && *psz_check != '-' && *psz_check ) return;
    if( *psz_check == ':' && ++psz_check )
    {
        if( isdigit(*psz_check) )
            if(!next(&psz_check)) return;
    }
    if( *psz_check != '-' && *psz_check ) return;
    if( *psz_check == '-' && ++psz_check )
    {
        if( isdigit(*psz_check) )
            if(!next(&psz_check)) return;
    }
    if( *psz_check != ':' && *psz_check ) return;
    if( *psz_check == ':' && ++psz_check )
    {
        if( isdigit(*psz_check) )
            if(!next(&psz_check)) return;
    }
    if( *psz_check ) return;

    /* Separate start and end */
    *psz++ = 0;
    if( ( psz_end = strchr( psz, '-' ) ) ) *psz_end++ = 0;

    /* Look for the start title */
    *pi_title_start = strtol( psz, &psz_next, 0 );
    if( !*pi_title_start && psz == psz_next ) *pi_title_start = -1;
    *pi_title_end = *pi_title_start;
    psz = psz_next;

    /* Look for the start chapter */
    if( *psz ) psz++;
    *pi_chapter_start = strtol( psz, &psz_next, 0 );
    if( !*pi_chapter_start && psz == psz_next ) *pi_chapter_start = -1;
    *pi_chapter_end = *pi_chapter_start;

    if( psz_end )
    {
        /* Look for the end title */
        *pi_title_end = strtol( psz_end, &psz_next, 0 );
        if( !*pi_title_end && psz_end == psz_next ) *pi_title_end = -1;
        psz_end = psz_next;

        /* Look for the end chapter */
        if( *psz_end ) psz_end++;
        *pi_chapter_end = strtol( psz_end, &psz_next, 0 );
        if( !*pi_chapter_end && psz_end == psz_next ) *pi_chapter_end = -1;
    }

    msg_Dbg( p_input, "source=`%s' title=%d/%d seekpoint=%d/%d",
             psz_source, *pi_title_start, *pi_chapter_start,
             *pi_title_end, *pi_chapter_end );
}

/*****************************************************************************
 * input_AddSubtitles: add a subtitles file and enable it
 *****************************************************************************/
static void SubtitleAdd( input_thread_t *p_input, char *psz_subtitle, bool b_forced )
{
    input_source_t *sub;
    vlc_value_t count;
    vlc_value_t list;
    char *psz_path, *psz_extension;

    /* if we are provided a subtitle.sub file,
     * see if we don't have a subtitle.idx and use it instead */
    psz_path = strdup( psz_subtitle );
    if( psz_path )
    {
        psz_extension = strrchr( psz_path, '.');
        if( psz_extension && strcmp( psz_extension, ".sub" ) == 0 )
        {
            struct stat st;

            strcpy( psz_extension, ".idx" );

            if( !utf8_stat( psz_path, &st ) )
            {
                msg_Dbg( p_input, "using %s subtitles file instead of %s",
                         psz_path, psz_subtitle );
                strcpy( psz_subtitle, psz_path );
            }
        }
        free( psz_path );
    }

    var_Change( p_input, "spu-es", VLC_VAR_CHOICESCOUNT, &count, NULL );

    sub = InputSourceNew( p_input );
    if( InputSourceInit( p_input, sub, psz_subtitle, "subtitle" ) )
    {
        free( sub );
        return;
    }
    TAB_APPEND( p_input->p->i_slave, p_input->p->slave, sub );

    /* Select the ES */
    if( b_forced && !var_Change( p_input, "spu-es", VLC_VAR_GETLIST, &list, NULL ) )
    {
        if( count.i_int == 0 )
            count.i_int++;
        /* if it was first one, there is disable too */

        if( count.i_int < list.p_list->i_count )
        {
            int i_id = list.p_list->p_values[count.i_int].i_int;
            es_out_id_t *p_es = input_EsOutGetFromID( p_input->p->p_es_out, i_id );

            es_out_Control( p_input->p->p_es_out, ES_OUT_SET_DEFAULT, p_es );
            es_out_Control( p_input->p->p_es_out, ES_OUT_SET_ES, p_es );
        }
        var_Change( p_input, "spu-es", VLC_VAR_FREELIST, &list, NULL );
    }
}

bool input_AddSubtitles( input_thread_t *p_input, char *psz_subtitle,
                               bool b_check_extension )
{
    vlc_value_t val;

    if( b_check_extension && !subtitles_Filter( psz_subtitle ) )
        return false;

    assert( psz_subtitle != NULL );

    val.psz_string = strdup( psz_subtitle );
    if( val.psz_string )
        input_ControlPush( p_input, INPUT_CONTROL_ADD_SUBTITLE, &val );
    return true;
}

/*****************************************************************************
 * input_get_event_manager
 *****************************************************************************/
vlc_event_manager_t *input_get_event_manager( input_thread_t *p_input )
{
    return &p_input->p->event_manager;
}
