/*****************************************************************************
 * messages.c: messages interface
 * This library provides an interface to the message queue to be used by other
 * modules, especially intf modules. See vlc_config.h for output configuration.
 *****************************************************************************
 * Copyright (C) 1998-2005 the VideoLAN team
 * $Id: d99c70ac69b9870a992121e9b5607b8536b7c564 $
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>

#include <stdarg.h>                                       /* va_list for BSD */
#ifdef __APPLE__
# include <xlocale.h>
#elif HAVE_LOCALE_H
# include <locale.h>
#endif
#include <errno.h>                                                  /* errno */

#ifdef WIN32
#   include <vlc_network.h>          /* 'net_strerror' and 'WSAGetLastError' */
#endif

#ifdef HAVE_UNISTD_H
#   include <unistd.h>                                   /* close(), write() */
#endif

#include <assert.h>

#include <vlc_charset.h>
#include "../libvlc.h"

typedef struct
{
    int i_code;
    char * psz_message;
} msg_context_t;

static void cleanup_msg_context (void *data)
{
    msg_context_t *ctx = data;
    free (ctx->psz_message);
    free (ctx);
}

static vlc_threadvar_t msg_context;
static uintptr_t banks = 0;

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

static inline msg_bank_t *libvlc_bank (libvlc_int_t *inst)
{
    return (libvlc_priv (inst))->msg_bank;
}

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static void PrintMsg ( vlc_object_t *, msg_item_t * );

static vlc_mutex_t msg_stack_lock = VLC_STATIC_MUTEX;

/**
 * Store all data required by messages interfaces.
 */
struct msg_bank_t
{
    /** Message queue lock */
    vlc_rwlock_t lock;

    /* Subscribers */
    int i_sub;
    msg_subscription_t **pp_sub;

    locale_t locale; /**< C locale for error messages */
    vlc_dictionary_t enabled_objects; ///< Enabled objects
    bool all_objects_enabled; ///< Should we print all objects?
};

/**
 * Initialize messages queues
 * This function initializes all message queues
 */
msg_bank_t *msg_Create (void)
{
    msg_bank_t *bank = malloc (sizeof (*bank));

    vlc_rwlock_init (&bank->lock);
    vlc_dictionary_init (&bank->enabled_objects, 0);
    bank->all_objects_enabled = true;

    bank->i_sub = 0;
    bank->pp_sub = NULL;

    /* C locale to get error messages in English in the logs */
    bank->locale = newlocale (LC_MESSAGES_MASK, "C", (locale_t)0);

    vlc_mutex_lock( &msg_stack_lock );
    if( banks++ == 0 )
        vlc_threadvar_create( &msg_context, cleanup_msg_context );
    vlc_mutex_unlock( &msg_stack_lock );
    return bank;
}

/**
 * Object Printing selection
 */
static void const * kObjectPrintingEnabled = &kObjectPrintingEnabled;
static void const * kObjectPrintingDisabled = &kObjectPrintingDisabled;

#undef msg_EnableObjectPrinting
void msg_EnableObjectPrinting (vlc_object_t *obj, const char * psz_object)
{
    msg_bank_t *bank = libvlc_bank (obj->p_libvlc);

    vlc_rwlock_wrlock (&bank->lock);
    if( !strcmp(psz_object, "all") )
        bank->all_objects_enabled = true;
    else
        vlc_dictionary_insert (&bank->enabled_objects, psz_object,
                               (void *)kObjectPrintingEnabled);
    vlc_rwlock_unlock (&bank->lock);
}

#undef msg_DisableObjectPrinting
void msg_DisableObjectPrinting (vlc_object_t *obj, const char * psz_object)
{
    msg_bank_t *bank = libvlc_bank (obj->p_libvlc);

    vlc_rwlock_wrlock (&bank->lock);
    if( !strcmp(psz_object, "all") )
        bank->all_objects_enabled = false;
    else
        vlc_dictionary_insert (&bank->enabled_objects, psz_object,
                               (void *)kObjectPrintingDisabled);
    vlc_rwlock_unlock (&bank->lock);
}

/**
 * Destroy the message queues
 *
 * This functions prints all messages remaining in the queues,
 * then frees all the allocated resources
 * No other messages interface functions should be called after this one.
 */
void msg_Destroy (msg_bank_t *bank)
{
    if (unlikely(bank->i_sub != 0))
        fputs ("stale interface subscribers (LibVLC might crash)\n", stderr);

    vlc_mutex_lock( &msg_stack_lock );
    assert(banks > 0);
    if( --banks == 0 )
        vlc_threadvar_delete( &msg_context );
    vlc_mutex_unlock( &msg_stack_lock );

    if (bank->locale != (locale_t)0)
       freelocale (bank->locale);

    vlc_dictionary_clear (&bank->enabled_objects, NULL, NULL);

    vlc_rwlock_destroy (&bank->lock);
    free (bank);
}

struct msg_subscription_t
{
    libvlc_int_t   *instance;
    msg_callback_t  func;
    msg_cb_data_t  *opaque;
};

/**
 * Subscribe to the message queue.
 * Whenever a message is emitted, a callback will be called.
 * Callback invocation are serialized within a subscription.
 *
 * @param instance LibVLC instance to get messages from
 * @param cb callback function
 * @param opaque data for the callback function
 * @return a subscription pointer, or NULL in case of failure
 */
msg_subscription_t *msg_Subscribe (libvlc_int_t *instance, msg_callback_t cb,
                                   msg_cb_data_t *opaque)
{
    msg_subscription_t *sub = malloc (sizeof (*sub));
    if (sub == NULL)
        return NULL;

    sub->instance = instance;
    sub->func = cb;
    sub->opaque = opaque;

    msg_bank_t *bank = libvlc_bank (instance);
    vlc_rwlock_wrlock (&bank->lock);
    TAB_APPEND (bank->i_sub, bank->pp_sub, sub);
    vlc_rwlock_unlock (&bank->lock);

    return sub;
}

/**
 * Unsubscribe from the message queue.
 * This function waits for the message callback to return if needed.
 */
void msg_Unsubscribe (msg_subscription_t *sub)
{
    msg_bank_t *bank = libvlc_bank (sub->instance);

    vlc_rwlock_wrlock (&bank->lock);
    TAB_REMOVE (bank->i_sub, bank->pp_sub, sub);
    vlc_rwlock_unlock (&bank->lock);
    free (sub);
}

/*****************************************************************************
 * msg_*: print a message
 *****************************************************************************
 * These functions queue a message for later printing.
 *****************************************************************************/
void msg_Generic( vlc_object_t *p_this, int i_type, const char *psz_module,
                    const char *psz_format, ... )
{
    va_list args;

    va_start( args, psz_format );
    msg_GenericVa (p_this, i_type, psz_module, psz_format, args);
    va_end( args );
}

/**
 * Destroys a message.
 */
static void msg_Free (gc_object_t *gc)
{
    msg_item_t *msg = vlc_priv (gc, msg_item_t);

    free (msg->psz_module);
    free (msg->psz_msg);
    free (msg->psz_header);
    free (msg);
}

#undef msg_GenericVa
/**
 * Add a message to a queue
 *
 * This function provides basic functionnalities to other msg_* functions.
 * It adds a message to a queue (after having printed all stored messages if it
 * is full). If the message can't be converted to string in memory, it issues
 * a warning.
 */
void msg_GenericVa (vlc_object_t *p_this, int i_type,
                           const char *psz_module,
                           const char *psz_format, va_list _args)
{
    size_t      i_header_size;             /* Size of the additionnal header */
    vlc_object_t *p_obj;
    char *       psz_str = NULL;                 /* formatted message string */
    char *       psz_header = NULL;
    va_list      args;

    assert (p_this);

    if( p_this->i_flags & OBJECT_FLAGS_QUIET ||
        (p_this->i_flags & OBJECT_FLAGS_NODBG && i_type == VLC_MSG_DBG) )
        return;

    msg_bank_t *bank = libvlc_bank (p_this->p_libvlc);
    locale_t locale = uselocale (bank->locale);

#ifndef __GLIBC__
    /* Expand %m to strerror(errno) - only once */
    char buf[strlen( psz_format ) + 2001], *ptr;
    strcpy( buf, psz_format );
    ptr = (char*)buf;
    psz_format = (const char*) buf;

    for( ;; )
    {
        ptr = strchr( ptr, '%' );
        if( ptr == NULL )
            break;

        if( ptr[1] == 'm' )
        {
            char errbuf[2001];
            size_t errlen;

#ifndef WIN32
            strerror_r( errno, errbuf, 1001 );
#else
            int sockerr = WSAGetLastError( );
            if( sockerr )
            {
                strncpy( errbuf, net_strerror( sockerr ), 1001 );
                WSASetLastError( sockerr );
            }
            if ((sockerr == 0)
             || (strcmp ("Unknown network stack error", errbuf) == 0))
                strncpy( errbuf, strerror( errno ), 1001 );
#endif
            errbuf[1000] = 0;

            /* Escape '%' from the error string */
            for( char *percent = strchr( errbuf, '%' );
                 percent != NULL;
                 percent = strchr( percent + 2, '%' ) )
            {
                memmove( percent + 1, percent, strlen( percent ) + 1 );
            }

            errlen = strlen( errbuf );
            memmove( ptr + errlen, ptr + 2, strlen( ptr + 2 ) + 1 );
            memcpy( ptr, errbuf, errlen );
            break; /* Only once, so we don't overflow */
        }

        /* Looks for conversion specifier... */
        do
            ptr++;
        while( *ptr && ( strchr( "diouxXeEfFgGaAcspn%", *ptr ) == NULL ) );
        if( *ptr )
            ptr++; /* ...and skip it */
    }
#endif

    /* Convert message to string  */
    vlc_va_copy( args, _args );
    if( vasprintf( &psz_str, psz_format, args ) == -1 )
        psz_str = NULL;
    va_end( args );

    if( psz_str == NULL )
    {
        int canc = vlc_savecancel (); /* Do not print half of a message... */
#ifdef __GLIBC__
        fprintf( stderr, "main warning: can't store message (%m): " );
#else
        char psz_err[1001];
#ifndef WIN32
        /* we're not using GLIBC, so we are sure that the error description
         * will be stored in the buffer we provide to strerror_r() */
        strerror_r( errno, psz_err, 1001 );
#else
        strncpy( psz_err, strerror( errno ), 1001 );
#endif
        psz_err[1000] = '\0';
        fprintf( stderr, "main warning: can't store message (%s): ", psz_err );
#endif
        vlc_va_copy( args, _args );
        /* We should use utf8_vfprintf - but it calls malloc()... */
        vfprintf( stderr, psz_format, args );
        va_end( args );
        fputs( "\n", stderr );
        vlc_restorecancel (canc);
        uselocale (locale);
        return;
    }
    uselocale (locale);

    msg_item_t * p_item = malloc (sizeof (*p_item));
    if (p_item == NULL)
        return; /* Uho! */

    vlc_gc_init (p_item, msg_Free);
    p_item->psz_module = p_item->psz_msg = p_item->psz_header = NULL;



    i_header_size = 0;
    p_obj = p_this;
    while( p_obj != NULL )
    {
        char *psz_old = NULL;
        if( p_obj->psz_header )
        {
            i_header_size += strlen( p_obj->psz_header ) + 4;
            if( psz_header )
            {
                psz_old = strdup( psz_header );
                psz_header = xrealloc( psz_header, i_header_size );
                snprintf( psz_header, i_header_size , "[%s] %s",
                          p_obj->psz_header, psz_old );
            }
            else
            {
                psz_header = xmalloc( i_header_size );
                snprintf( psz_header, i_header_size, "[%s]",
                          p_obj->psz_header );
            }
        }
        free( psz_old );
        p_obj = p_obj->p_parent;
    }

    /* Fill message information fields */
    p_item->i_type =        i_type;
    p_item->i_object_id =   (uintptr_t)p_this;
    p_item->psz_object_type = p_this->psz_object_type;
    p_item->psz_module =    strdup( psz_module );
    p_item->psz_msg =       psz_str;
    p_item->psz_header =    psz_header;

    PrintMsg( p_this, p_item );

    vlc_rwlock_rdlock (&bank->lock);
    for (int i = 0; i < bank->i_sub; i++)
    {
        msg_subscription_t *sub = bank->pp_sub[i];
        libvlc_priv_t *priv = libvlc_priv( sub->instance );
        msg_bank_t *bank = priv->msg_bank;
        void *val = vlc_dictionary_value_for_key( &bank->enabled_objects,
                                                  p_item->psz_module );
        if( val == kObjectPrintingDisabled ) continue;
        if( val != kObjectPrintingEnabled  ) /*if not allowed */
        {
            val = vlc_dictionary_value_for_key( &bank->enabled_objects,
                                                 p_item->psz_object_type );
            if( val == kObjectPrintingDisabled ) continue;
            if( val == kObjectPrintingEnabled  ); /* Allowed */
            else if( !bank->all_objects_enabled ) continue;
        }

        sub->func (sub->opaque, p_item, 0);
    }
    vlc_rwlock_unlock (&bank->lock);
    msg_Release (p_item);
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
#   define WHITE   COL(0)
#   define GRAY    "\033[0m"

    static const char ppsz_type[4][9] = { "", " error", " warning", " debug" };
    static const char ppsz_color[4][8] = { WHITE, RED, YELLOW, GRAY };
    const char *psz_object;
    libvlc_priv_t *priv = libvlc_priv (p_this->p_libvlc);
    msg_bank_t *bank = priv->msg_bank;
    int i_type = p_item->i_type;

    switch( i_type )
    {
        case VLC_MSG_ERR:
            if( priv->i_verbose < 0 ) return;
            break;
        case VLC_MSG_INFO:
            if( priv->i_verbose < 0 ) return;
            break;
        case VLC_MSG_WARN:
            if( priv->i_verbose < 1 ) return;
            break;
        case VLC_MSG_DBG:
            if( priv->i_verbose < 2 ) return;
            break;
    }

    psz_object = p_item->psz_object_type;
    void * val = vlc_dictionary_value_for_key (&bank->enabled_objects,
                                               p_item->psz_module);
    if( val == kObjectPrintingDisabled )
        return;
    if( val == kObjectPrintingEnabled )
        /* Allowed */;
    else
    {
        val = vlc_dictionary_value_for_key (&bank->enabled_objects,
                                            psz_object);
        if( val == kObjectPrintingDisabled )
            return;
        if( val == kObjectPrintingEnabled )
            /* Allowed */;
        else if( !bank->all_objects_enabled )
            return;
    }

    int canc = vlc_savecancel ();
    /* Send the message to stderr */
    utf8_fprintf( stderr, "[%s%p%s] %s%s%s %s%s: %s%s%s\n",
                  priv->b_color ? GREEN : "",
                  (void *)p_item->i_object_id,
                  priv->b_color ? GRAY : "",
                  p_item->psz_header ? p_item->psz_header : "",
                  p_item->psz_header ? " " : "",
                  p_item->psz_module, psz_object,
                  ppsz_type[i_type],
                  priv->b_color ? ppsz_color[i_type] : "",
                  p_item->psz_msg,
                  priv->b_color ? GRAY : "" );

#ifdef WIN32
    fflush( stderr );
#endif
    vlc_restorecancel (canc);
}

static msg_context_t* GetContext(void)
{
    msg_context_t *p_ctx = vlc_threadvar_get( msg_context );
    if( p_ctx == NULL )
    {
        p_ctx = malloc( sizeof( msg_context_t ) );
        if( !p_ctx )
            return NULL;
        p_ctx->psz_message = NULL;
        vlc_threadvar_set( msg_context, p_ctx );
    }
    return p_ctx;
}

void msg_StackSet( int i_code, const char *psz_message, ... )
{
    va_list ap;
    msg_context_t *p_ctx = GetContext();

    if( p_ctx == NULL )
        return;
    free( p_ctx->psz_message );

    va_start( ap, psz_message );
    if( vasprintf( &p_ctx->psz_message, psz_message, ap ) == -1 )
        p_ctx->psz_message = NULL;
    va_end( ap );

    p_ctx->i_code = i_code;
}

void msg_StackAdd( const char *psz_message, ... )
{
    char *psz_tmp;
    va_list ap;
    msg_context_t *p_ctx = GetContext();

    if( p_ctx == NULL )
        return;

    va_start( ap, psz_message );
    if( vasprintf( &psz_tmp, psz_message, ap ) == -1 )
        psz_tmp = NULL;
    va_end( ap );

    if( !p_ctx->psz_message )
        p_ctx->psz_message = psz_tmp;
    else
    {
        char *psz_new;
        if( asprintf( &psz_new, "%s: %s", psz_tmp, p_ctx->psz_message ) == -1 )
            psz_new = NULL;

        free( p_ctx->psz_message );
        p_ctx->psz_message = psz_new;
        free( psz_tmp );
    }
}

const char* msg_StackMsg( void )
{
    msg_context_t *p_ctx = GetContext();
    assert( p_ctx );
    return p_ctx->psz_message;
}
