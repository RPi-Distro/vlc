/*****************************************************************************
 * messages.c: messages interface
 * This library provides an interface to the message queue to be used by other
 * modules, especially intf modules. See config.h for output configuration.
 *****************************************************************************
 * Copyright (C) 1998-2005 the VideoLAN team
 * $Id$
 *
 * Authors: Vincent Seguin <seguin@via.ecp.fr>
 *          Samuel Hocevar <sam@zoy.org>
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
#include <stdio.h>                                               /* required */
#include <stdarg.h>                                       /* va_list for BSD */
#include <stdlib.h>                                              /* malloc() */
#include <string.h>                                            /* strerror() */

#include <vlc/vlc.h>

#ifdef HAVE_FCNTL_H
#   include <fcntl.h>                  /* O_CREAT, O_TRUNC, O_WRONLY, O_SYNC */
#endif

#include <errno.h>                                                  /* errno */

#ifdef HAVE_UNISTD_H
#   include <unistd.h>                                   /* close(), write() */
#endif

#include "vlc_interface.h"
#include "charset.h"

/*****************************************************************************
 * Local macros
 *****************************************************************************/
#if defined(HAVE_VA_COPY)
#   define vlc_va_copy(dest,src) va_copy(dest,src)
#elif defined(HAVE___VA_COPY)
#   define vlc_va_copy(dest,src) __va_copy(dest,src)
#else
#   define vlc_va_copy(dest,src) (dest)=(src)
#endif

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static void QueueMsg ( vlc_object_t *, int, int , const char *,
                       const char *, va_list );
static void FlushMsg ( msg_queue_t * );
static void PrintMsg ( vlc_object_t *, msg_item_t * );
static void CreateMsgQueue( vlc_object_t *p_this, int i_queue );

/**
 * Initialize messages queues
 * This function initializes all message queues
 */
void __msg_Create( vlc_object_t *p_this )
{
    vlc_mutex_init( p_this, &(p_this->p_libvlc->msg_bank.lock) );

    CreateMsgQueue( p_this, MSG_QUEUE_NORMAL );
    CreateMsgQueue( p_this, MSG_QUEUE_HTTPD_ACCESS );

#ifdef UNDER_CE
    p_this->p_libvlc->msg_bank.pp_queues[MSG_QUEUE_NORMAL]->logfile =
        CreateFile( L"vlc-log.txt", GENERIC_WRITE,
                    FILE_SHARE_READ|FILE_SHARE_WRITE, NULL,
                    CREATE_ALWAYS, 0, NULL );
    SetFilePointer( p_this->p_libvlc->msg_bank.pp_queues[MSG_QUEUE_NORMAL]->
                                     logfile, 0, NULL, FILE_END );
#endif

}

static void CreateMsgQueue( vlc_object_t *p_this, int i_queue )
{
    msg_queue_t *p_queue = (msg_queue_t *)malloc( sizeof( msg_queue_t ) );

    vlc_mutex_init( p_this, &p_queue->lock );

    p_queue->b_overflow = VLC_FALSE;
    p_queue->i_id = i_queue;
    p_queue->i_start = 0;
    p_queue->i_stop = 0;

    p_queue->i_sub = 0;
    p_queue->pp_sub = NULL;

    INSERT_ELEM( p_this->p_libvlc->msg_bank.pp_queues,
                 p_this->p_libvlc->msg_bank.i_queues,
                 i_queue,
                 p_queue );
}

/**
 * Flush all message queues
 */
void __msg_Flush( vlc_object_t *p_this )
{
    int i;

    for( i = 0 ; i < p_this->p_libvlc->msg_bank.i_queues; i++ )
    {
        vlc_mutex_lock( &p_this->p_libvlc->msg_bank.pp_queues[i]->lock );
        FlushMsg( p_this->p_libvlc->msg_bank.pp_queues[i] );
        vlc_mutex_unlock( &p_this->p_libvlc->msg_bank.pp_queues[i]->lock );
    }
}

/**
 * Free resources allocated by msg_Create
 *
 * This functions prints all messages remaining in the normal queue,
 * then frees all the allocated ressources
 * No other messages interface functions should be called after this one.
 */
void __msg_Destroy( vlc_object_t *p_this )
{
    int i;
    for( i = p_this->p_libvlc->msg_bank.i_queues -1 ; i >= 0;  i-- )
    {
        msg_queue_t *p_queue = p_this->p_libvlc->msg_bank.pp_queues[i];
        if( p_queue->i_sub )
        {
            msg_Err( p_this, "stale interface subscribers" );
        }
        FlushMsg( p_queue );

#ifdef UNDER_CE
        if( i == MSG_QUEUE_NORMAL )
            CloseHandle( p_this->p_libvlc->msg_bank.pp_queues[MSG_QUEUE_NORMAL]->logfile );
#endif
        /* Destroy lock */
        vlc_mutex_destroy( &p_queue->lock );
        REMOVE_ELEM( p_this->p_libvlc->msg_bank.pp_queues,
                     p_this->p_libvlc->msg_bank.i_queues, i );
        free( p_queue );
    }
    vlc_mutex_destroy( &(p_this->p_libvlc->msg_bank.lock) );
}

/**
 * Subscribe to the message queue.
 */
msg_subscription_t *__msg_Subscribe( vlc_object_t *p_this, int i_queue )
{
    msg_bank_t *p_bank = &p_this->p_libvlc->msg_bank;
    msg_subscription_t *p_sub = malloc( sizeof( msg_subscription_t ) );
    msg_queue_t *p_queue = NULL;
    int i;

    vlc_mutex_lock( &p_bank->lock );

    for( i = 0 ; i <p_bank->i_queues ;i++ )
    {
        if( p_bank->pp_queues[i]->i_id == i_queue )
        {
            p_queue = p_bank->pp_queues[i];
        }
    }

    if( p_queue == NULL )
    {
        vlc_mutex_unlock( &p_bank->lock );
        return NULL;
    }

    vlc_mutex_lock( &p_queue->lock );

    /* Add subscription to the list */
    INSERT_ELEM( p_bank->pp_queues[i_queue]->pp_sub,
                 p_bank->pp_queues[i_queue]->i_sub,
                 p_bank->pp_queues[i_queue]->i_sub,
                 p_sub );

    p_sub->i_start = p_queue->i_start;
    p_sub->pi_stop = &p_queue->i_stop;

    p_sub->p_msg   = p_queue->msg;
    p_sub->p_lock  = &p_queue->lock;

    vlc_mutex_unlock( &p_queue->lock );
    vlc_mutex_unlock( &p_bank->lock );

    return p_sub;
}

/**
 * Unsubscribe from the message queue.
 */
void __msg_Unsubscribe( vlc_object_t *p_this, msg_subscription_t *p_sub )
{
    msg_bank_t *p_bank = &p_this->p_libvlc->msg_bank;
    int i,j;

    free( p_sub ); /* we won't dereference the pointer */

    vlc_mutex_lock( &p_bank->lock );

    for( i = 0 ; i< p_bank->i_queues ; i++ )
    {
        vlc_mutex_lock( & p_bank->pp_queues[i]->lock );
        for( j = 0 ; j< p_bank->pp_queues[i]->i_sub ; j++ )
        {
            if( p_bank->pp_queues[i]->pp_sub[j] == p_sub )
            {
                REMOVE_ELEM( p_bank->pp_queues[i]->pp_sub,
                             p_bank->pp_queues[i]->i_sub,
                             j );
            }
        }
        vlc_mutex_unlock( & p_bank->pp_queues[i]->lock );
    }

    vlc_mutex_unlock( &p_bank->lock );
}

const char *msg_GetObjectTypeName(int i_object_type )
{
    switch( i_object_type )
    {
        case VLC_OBJECT_ROOT: return "root";
        case VLC_OBJECT_MODULE: return "module";
        case VLC_OBJECT_INTF: return "interface";
        case VLC_OBJECT_PLAYLIST: return "playlist";
        case VLC_OBJECT_ITEM: return "item";
        case VLC_OBJECT_INPUT: return "input";
        case VLC_OBJECT_DECODER: return "decoder";
        case VLC_OBJECT_PACKETIZER: return "packetizer";
        case VLC_OBJECT_ENCODER: return "encoder";
        case VLC_OBJECT_VOUT: return "video output";
        case VLC_OBJECT_AOUT: return "audio output";
        case VLC_OBJECT_SOUT: return "stream output";
        case VLC_OBJECT_HTTPD: return "http server";
        case VLC_OBJECT_HTTPD_HOST: return "http server";
        case VLC_OBJECT_DIALOGS: return "dialogs provider";
        case VLC_OBJECT_VLM: return "vlm";
        case VLC_OBJECT_ANNOUNCE: return "announce handler";
        case VLC_OBJECT_DEMUX: return "demuxer";
        case VLC_OBJECT_ACCESS: return "access";
        default: return "private";
    }
}

/*****************************************************************************
 * __msg_*: print a message
 *****************************************************************************
 * These functions queue a message for later printing.
 *****************************************************************************/
void __msg_Generic( vlc_object_t *p_this, int i_queue_id, int i_type,
                    const char *psz_module,
                    const char *psz_format, ... )
{
    va_list args;

    va_start( args, psz_format );
    QueueMsg( p_this, i_queue_id, i_type, psz_module, psz_format, args );
    va_end( args );
}

void __msg_GenericVa( vlc_object_t *p_this, int i_queue_id,
                      int i_type, const char *psz_module,
                      const char *psz_format, va_list args )
{
    QueueMsg( p_this, i_queue_id, i_type, psz_module, psz_format, args );
}

/* Generic functions used when variadic macros are not available. */
#define DECLARE_MSG_FN( FN_NAME, FN_TYPE ) \
    void FN_NAME( vlc_object_t *p_this, const char *psz_format, ... ) \
    { \
        va_list args; \
        va_start( args, psz_format ); \
        QueueMsg( (vlc_object_t *)p_this,MSG_QUEUE_NORMAL, FN_TYPE, "unknown", \
                  psz_format, args ); \
        va_end( args ); \
    } \
    struct _
/**
 * Output an informational message.
 * \note Do not use this for debug messages
 * \see input_AddInfo
 */
DECLARE_MSG_FN( __msg_Info, VLC_MSG_INFO );
/**
 * Output an error message.
 */
DECLARE_MSG_FN( __msg_Err,  VLC_MSG_ERR );
/**
 * Output a waring message
 */
DECLARE_MSG_FN( __msg_Warn, VLC_MSG_WARN );
/**
 * Output a debug message
 */
DECLARE_MSG_FN( __msg_Dbg,  VLC_MSG_DBG );

/**
 * Add a message to a queue
 *
 * This function provides basic functionnalities to other msg_* functions.
 * It adds a message to a queue (after having printed all stored messages if it
 * is full). If the message can't be converted to string in memory, it issues
 * a warning.
 */
static void QueueMsg( vlc_object_t *p_this, int i_queue_id, int i_type,
                      const char *psz_module,
                      const char *psz_format, va_list _args )
{
    int         i_header_size;             /* Size of the additionnal header */
    vlc_object_t *p_obj;
    msg_bank_t  *p_bank;                                     /* message bank */
    msg_queue_t *p_queue = NULL;
    char *       psz_str = NULL;                 /* formatted message string */
    char *       psz_header = NULL;
    va_list      args;
    msg_item_t * p_item = NULL;                        /* pointer to message */
    msg_item_t   item;                    /* message in case of a full queue */

#if !defined(HAVE_VASPRINTF) || defined(__APPLE__) || defined(SYS_BEOS)
    int          i_size = strlen(psz_format) + INTF_MAX_MSG_SIZE;
#endif
    int i;

    if( p_this == NULL || p_this->i_flags & OBJECT_FLAGS_QUIET ||
        (p_this->i_flags & OBJECT_FLAGS_NODBG && i_type == VLC_MSG_DBG) )
    {
        return;
    }

    p_bank = &p_this->p_libvlc->msg_bank;

    /*
     * Convert message to string
     */
#if defined(HAVE_VASPRINTF) && !defined(__APPLE__) && !defined( SYS_BEOS )
    vlc_va_copy( args, _args );
    vasprintf( &psz_str, psz_format, args );
    va_end( args );
#else
    psz_str = (char*) malloc( i_size * sizeof(char) );
#endif

    if( psz_str == NULL )
    {
        fprintf( stderr, "main warning: can't store message (%s): ",
                 strerror(errno) );
        vlc_va_copy( args, _args );
        /* We should use utf8_vfprintf - but it calls malloc()... */
        vfprintf( stderr, psz_format, args );
        va_end( args );
        fputs( "\n", stderr );
        return;
    }

    i_header_size = 0;
    p_obj = p_this;
    while( p_obj != NULL )
    {
        char *psz_old = NULL;
        if( p_obj == NULL ) break;
        if( p_obj->psz_header )
        {
            i_header_size += strlen( p_obj->psz_header ) + 4;
            if( psz_header )
            {
                psz_old = strdup( psz_header );
                psz_header = (char*)realloc( psz_header, i_header_size );
                snprintf( psz_header, i_header_size , "[%s] %s",
                          p_obj->psz_header, psz_old );
            }
            else
            {
                psz_header = (char *)malloc( i_header_size );
                snprintf( psz_header, i_header_size, "[%s]",
                          p_obj->psz_header );
            }
        }
        if( psz_old ) free( psz_old );
        p_obj = p_obj->p_parent;
    }

#if !defined(HAVE_VASPRINTF) || defined(__APPLE__) || defined(SYS_BEOS)
    vlc_va_copy( args, _args );
    vsnprintf( psz_str, i_size, psz_format, args );
    va_end( args );
    psz_str[ i_size - 1 ] = 0; /* Just in case */
#endif

    /* Put message in queue */
    vlc_mutex_lock( &p_bank->lock );
    for( i = 0 ; i <p_bank->i_queues ;i++ )
    {
        if( p_bank->pp_queues[i]->i_id == i_queue_id )
        {
            p_queue = p_bank->pp_queues[i];
        }
    }

    if( p_queue == NULL )
    {
        vlc_mutex_unlock( &p_bank->lock );
        if( psz_str ) free( psz_str );
        if( psz_header ) free( psz_header );
        return;
    }

    vlc_mutex_lock( &p_queue->lock );

    /* Check there is room in the queue for our message */
    if( p_queue->b_overflow )
    {
        FlushMsg( p_queue );

        if( ((p_queue->i_stop - p_queue->i_start + 1) % VLC_MSG_QSIZE) == 0 )
        {
            /* Still in overflow mode, print from a dummy item */
            p_item = &item;
        }
        else
        {
            /* Pheeew, at last, there is room in the queue! */
            p_queue->b_overflow = VLC_FALSE;
        }
    }
    else if( ((p_queue->i_stop - p_queue->i_start + 2) % VLC_MSG_QSIZE) == 0 )
    {
        FlushMsg( p_queue );

        if( ((p_queue->i_stop - p_queue->i_start + 2) % VLC_MSG_QSIZE) == 0 )
        {
            p_queue->b_overflow = VLC_TRUE;

            if( p_queue->i_id == MSG_QUEUE_NORMAL )
            {
               /* Put the overflow message in the queue */
                p_item = p_queue->msg + p_queue->i_stop;
                p_queue->i_stop = (p_queue->i_stop + 1) % VLC_MSG_QSIZE;

                p_item->i_type =        VLC_MSG_WARN;
                p_item->i_object_id =   p_this->i_object_id;
                p_item->i_object_type = p_this->i_object_type;
                p_item->psz_module =    strdup( "message" );
                p_item->psz_msg =       strdup( "message queue overflowed" );
                p_item->psz_header =    NULL;

               PrintMsg( p_this, p_item );
               /* We print from a dummy item */
               p_item = &item;
            }
        }
    }

    if( !p_queue->b_overflow )
    {
        /* Put the message in the queue */
        p_item = p_queue->msg + p_queue->i_stop;
        p_queue->i_stop = (p_queue->i_stop + 1) % VLC_MSG_QSIZE;
    }

    /* Fill message information fields */
    p_item->i_type =        i_type;
    p_item->i_object_id =   p_this->i_object_id;
    p_item->i_object_type = p_this->i_object_type;
    p_item->psz_module =    strdup( psz_module );
    p_item->psz_msg =       psz_str;
    p_item->psz_header =    psz_header;

    if( p_queue->i_id == MSG_QUEUE_NORMAL )
        PrintMsg( p_this, p_item );

    if( p_queue->b_overflow )
    {
        if( p_item->psz_module )
            free( p_item->psz_module );
        if( p_item->psz_msg )
            free( p_item->psz_msg );
        if( p_item->psz_header )
            free( p_item->psz_header );
    }

    vlc_mutex_unlock ( &p_queue->lock );
    vlc_mutex_unlock( &p_bank->lock );
}

/* following functions are local */

/*****************************************************************************
 * FlushMsg
 *****************************************************************************
 * Print all messages remaining in queue. MESSAGE QUEUE MUST BE LOCKED, since
 * this function does not check the lock.
 *****************************************************************************/
static void FlushMsg ( msg_queue_t *p_queue )
{
    int i_index, i_start, i_stop;

    /* Get the maximum message index that can be freed */
    i_stop = p_queue->i_stop;

    /* Check until which value we can free messages */
    for( i_index = 0; i_index < p_queue->i_sub; i_index++ )
    {
        i_start = p_queue->pp_sub[ i_index ]->i_start;

        /* If this subscriber is late, we don't free messages before
         * his i_start value, otherwise he'll miss messages */
        if(   ( i_start < i_stop
               && (p_queue->i_stop <= i_start || i_stop <= p_queue->i_stop) )
           || ( i_stop < i_start
               && (i_stop <= p_queue->i_stop && p_queue->i_stop <= i_start) ) )
        {
            i_stop = i_start;
        }
    }

    /* Free message data */
    for( i_index = p_queue->i_start;
         i_index != i_stop;
         i_index = (i_index+1) % VLC_MSG_QSIZE )
    {
        if( p_queue->msg[i_index].psz_msg )
            free( p_queue->msg[i_index].psz_msg );
        if( p_queue->msg[i_index].psz_module )
            free( p_queue->msg[i_index].psz_module );
        if( p_queue->msg[i_index].psz_header )
            free( p_queue->msg[i_index].psz_header );
    }

    /* Update the new start value */
    p_queue->i_start = i_index;
}

/*****************************************************************************
 * PrintMsg: output a standard message item to stderr
 *****************************************************************************
 * Print a message to stderr, with colour formatting if needed.
 *****************************************************************************/
static void PrintMsg ( vlc_object_t * p_this, msg_item_t * p_item )
{
#   define COL(x)  "\033[" #x ";1m"
#   define RED     COL(31)
#   define GREEN   COL(32)
#   define YELLOW  COL(33)
#   define WHITE   COL(37)
#   define GRAY    "\033[0m"

#ifdef UNDER_CE
    int i_dummy;
#endif
    static const char * ppsz_type[4] = { "", " error", " warning", " debug" };
    static const char *ppsz_color[4] = { WHITE, RED, YELLOW, GRAY };
    char *psz_object = "private";
    int i_type = p_item->i_type;

    switch( i_type )
    {
        case VLC_MSG_ERR:
            if( p_this->p_libvlc->i_verbose < 0 ) return;
            break;
        case VLC_MSG_INFO:
            if( p_this->p_libvlc->i_verbose < 0 ) return;
            break;
        case VLC_MSG_WARN:
            if( p_this->p_libvlc->i_verbose < 1 ) return;
            break;
        case VLC_MSG_DBG:
            if( p_this->p_libvlc->i_verbose < 2 ) return;
            break;
    }

    psz_object = msg_GetObjectTypeName(p_item->i_object_type);

#ifdef UNDER_CE
#   define CE_WRITE(str) WriteFile( p_this->p_libvlc->msg_bank.pp_queues[MSG_QUEUE_NORMAL]->logfile, \
                                    str, strlen(str), &i_dummy, NULL );
    CE_WRITE( p_item->psz_module );
    CE_WRITE( " " );
    CE_WRITE( psz_object );
    CE_WRITE( ppsz_type[i_type] );
    CE_WRITE( ": " );
    CE_WRITE( p_item->psz_msg );
    CE_WRITE( "\r\n" );
    FlushFileBuffers( p_this->p_libvlc->msg_bank.pp_queues[MSG_QUEUE_NORMAL]->logfile );

#else
    /* Send the message to stderr */
    if( p_this->p_libvlc->b_color )
    {
        if( p_item->psz_header )
        {
            utf8_fprintf( stderr, "[" GREEN "%.8i" GRAY "] %s %s %s%s: %s%s" GRAY
                              "\n",
                         p_item->i_object_id, p_item->psz_header,
                         p_item->psz_module, psz_object,
                         ppsz_type[i_type], ppsz_color[i_type],
                         p_item->psz_msg );
        }
        else
        {
             utf8_fprintf( stderr, "[" GREEN "%.8i" GRAY "] %s %s%s: %s%s" GRAY "\n",
                         p_item->i_object_id, p_item->psz_module, psz_object,
                         ppsz_type[i_type], ppsz_color[i_type],
                         p_item->psz_msg );
        }
    }
    else
    {
        if( p_item->psz_header )
        {
            utf8_fprintf( stderr, "[%.8i] %s %s %s%s: %s\n", p_item->i_object_id,
                         p_item->psz_header, p_item->psz_module,
                         psz_object, ppsz_type[i_type], p_item->psz_msg );
        }
        else
        {
            utf8_fprintf( stderr, "[%.8i] %s %s%s: %s\n", p_item->i_object_id,
                         p_item->psz_module, psz_object, ppsz_type[i_type],
                         p_item->psz_msg );
        }
    }

#   if defined(WIN32)
    fflush( stderr );
#   endif
#endif
}
