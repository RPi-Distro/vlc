/*****************************************************************************
 * log.c: libvlc new API log functions
 *****************************************************************************
 * Copyright (C) 2005 the VideoLAN team
 *
 * $Id: core.c 14187 2006-02-07 16:37:40Z courmisch $
 *
 * Authors: Damien Fouilleul <damienf@videolan.org>
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

#include <libvlc_internal.h>
#include <vlc/libvlc.h>

struct libvlc_log_t
{
    const libvlc_instance_t  *p_instance;
    msg_subscription_t *p_messages;
};

struct libvlc_log_iterator_t
{
    msg_subscription_t *p_messages;
    int i_start;
    int i_pos;
    int i_end;
};

unsigned libvlc_get_log_verbosity( const libvlc_instance_t *p_instance, libvlc_exception_t *p_e )
{
    if( p_instance )
    {
        return p_instance->p_vlc->p_libvlc->i_verbose;
    }
    RAISEZERO("Invalid VLC instance!");
}

void libvlc_set_log_verbosity( libvlc_instance_t *p_instance, unsigned level, libvlc_exception_t *p_e )
{
    if( p_instance )
    {
        p_instance->p_vlc->p_libvlc->i_verbose = level;
    }
    else
        RAISEVOID("Invalid VLC instance!");
}

libvlc_log_t *libvlc_log_open( const libvlc_instance_t *p_instance, libvlc_exception_t *p_e )
{
    
    struct libvlc_log_t *p_log =
        (struct libvlc_log_t *)malloc(sizeof(struct libvlc_log_t));

    if( !p_log ) RAISENULL( "Out of memory" );

    p_log->p_instance = p_instance;
    p_log->p_messages = msg_Subscribe(p_instance->p_vlc, MSG_QUEUE_NORMAL);

    if( !p_log->p_messages ) RAISENULL( "Out of memory" );

    return p_log;
}

void libvlc_log_close( libvlc_log_t *p_log, libvlc_exception_t *p_e )
{
    if( p_log && p_log->p_messages )
    {
        msg_Unsubscribe(p_log->p_instance->p_vlc, p_log->p_messages);
        free(p_log);
    }
    else
        RAISEVOID("Invalid log object!");
}

unsigned libvlc_log_count( const libvlc_log_t *p_log, libvlc_exception_t *p_e )
{
    if( p_log && p_log->p_messages )
    {
        int i_start = p_log->p_messages->i_start;
        int i_stop  = *(p_log->p_messages->pi_stop);

        if( i_stop >= i_start )
            return i_stop-i_start;
        else
            return VLC_MSG_QSIZE-(i_start-i_stop);
    }
    RAISEZERO("Invalid log object!");
}

void libvlc_log_clear( libvlc_log_t *p_log, libvlc_exception_t *p_e )
{
    if( p_log && p_log->p_messages )
    {
        vlc_mutex_lock(p_log->p_messages->p_lock);
        p_log->p_messages->i_start = *(p_log->p_messages->pi_stop);
        vlc_mutex_unlock(p_log->p_messages->p_lock);
    }
    else
        RAISEVOID("Invalid log object!");
}

libvlc_log_iterator_t *libvlc_log_get_iterator( const libvlc_log_t *p_log, libvlc_exception_t *p_e )
{
    if( p_log && p_log->p_messages )
    {
        struct libvlc_log_iterator_t *p_iter =
            (struct libvlc_log_iterator_t *)malloc(sizeof(struct libvlc_log_iterator_t));

        if( !p_iter ) RAISENULL( "Out of memory" );

        vlc_mutex_lock(p_log->p_messages->p_lock);
        p_iter->p_messages = p_log->p_messages;
        p_iter->i_start    = p_log->p_messages->i_start;
        p_iter->i_pos      = p_log->p_messages->i_start;
        p_iter->i_end      = *(p_log->p_messages->pi_stop);
        vlc_mutex_unlock(p_log->p_messages->p_lock);

        return p_iter;
    }
    RAISENULL("Invalid log object!");
}

void libvlc_log_iterator_free( libvlc_log_iterator_t *p_iter, libvlc_exception_t *p_e )
{
    if( p_iter )
    {
        free(p_iter);
    }
    else
        RAISEVOID("Invalid log iterator!");
}

int libvlc_log_iterator_has_next( const libvlc_log_iterator_t *p_iter, libvlc_exception_t *p_e )
{
    if( p_iter )
    {
        return p_iter->i_pos != p_iter->i_end;
    }
    RAISEZERO("Invalid log iterator!");
}

libvlc_log_message_t *libvlc_log_iterator_next( libvlc_log_iterator_t *p_iter,
                                                struct libvlc_log_message_t *buffer,
                                                libvlc_exception_t *p_e )
{
    if( p_iter )
    {
        if( buffer && (sizeof(struct libvlc_log_message_t) == buffer->sizeof_msg) )
        {
            int i_pos = p_iter->i_pos;
            if( i_pos != p_iter->i_end )
            {
                msg_item_t *msg;
                vlc_mutex_lock(p_iter->p_messages->p_lock);
                msg = p_iter->p_messages->p_msg+i_pos;
                buffer->i_severity  = msg->i_type;
                buffer->psz_type    = msg_GetObjectTypeName(msg->i_object_type);
                buffer->psz_name    = msg->psz_module;
                buffer->psz_header  = msg->psz_header;
                buffer->psz_message = msg->psz_msg;
                p_iter->i_pos = ++i_pos % VLC_MSG_QSIZE;
                vlc_mutex_unlock(p_iter->p_messages->p_lock);

                return buffer;
            }
            RAISENULL("No more messages");
        }
        RAISENULL("Invalid message buffer!");
    }
    RAISENULL("Invalid log iterator!");
}

