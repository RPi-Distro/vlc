/*****************************************************************************
 * ipv6.c: IPv6 network abstraction layer
 *****************************************************************************
 * Copyright (C) 2002-2006 the VideoLAN team
 * $Id$
 *
 * Authors: Alexis Guillard <alexis.guillard@bt.com>
 *          Christophe Massiot <massiot@via.ecp.fr>
 *          Remco Poortinga <poortinga@telin.nl>
 *          Rémi Denis-Courmont <rem # videolan.org>
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
#include <stdlib.h>
#include <string.h>

#include <vlc/vlc.h>

#include <errno.h>

#ifdef WIN32
#   include <winsock2.h>
#   include <ws2tcpip.h>
#   define if_nametoindex( str ) atoi( str )
#else
#   include <sys/types.h>
#   include <unistd.h>
#   include <netdb.h>                                         /* hostent ... */
#   include <sys/socket.h>
#   include <netinet/in.h>
#   include <net/if.h>
#endif

#include "network.h"

#if defined(WIN32)
static const struct in6_addr in6addr_any = {{IN6ADDR_ANY_INIT}};
# define close closesocket
#endif

#if defined (WIN32) && !defined (MCAST_JOIN_SOURCE_GROUP)
/* Interim Vista definitions */
#  define MCAST_JOIN_SOURCE_GROUP 45 /* from <ws2ipdef.h> */
struct group_source_req
{
       uint32_t           gsr_interface;  /* interface index */
       struct sockaddr_storage gsr_group;      /* group address */
       struct sockaddr_storage gsr_source;     /* source address */
};
#endif

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int OpenUDP( vlc_object_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_description( _("UDP/IPv6 network abstraction layer") );
    set_capability( "network", 40 );
    set_callbacks( OpenUDP, NULL );
vlc_module_end();

/*****************************************************************************
 * BuildAddr: utility function to build a struct sockaddr_in6
 *****************************************************************************/
static int BuildAddr( vlc_object_t *p_this, struct sockaddr_in6 *p_socket,
                      const char *psz_address, int i_port )
{
    struct addrinfo hints, *res;
    int i;

    memset( &hints, 0, sizeof( hints ) );
    hints.ai_family = AF_INET6;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE;

    i = vlc_getaddrinfo( p_this, psz_address, 0, &hints, &res );
    if( i )
    {
        msg_Dbg( p_this, "%s: %s", psz_address, vlc_gai_strerror( i ) );
        return -1;
    }
    if ( res->ai_addrlen > sizeof (struct sockaddr_in6) )
    {
        vlc_freeaddrinfo( res );
        return -1;
    }

    memcpy( p_socket, res->ai_addr, res->ai_addrlen );
    vlc_freeaddrinfo( res );
    p_socket->sin6_port = htons( i_port );

    return 0;
}

#if defined(WIN32) || defined(UNDER_CE)
# define WINSOCK_STRERROR_SIZE 20
static const char *winsock_strerror( char *buf )
{
    snprintf( buf, WINSOCK_STRERROR_SIZE, "Winsock error %d",
              WSAGetLastError( ) );
    buf[WINSOCK_STRERROR_SIZE - 1] = '\0';
    return buf;
}
#endif


/*****************************************************************************
 * OpenUDP: open a UDP socket
 *****************************************************************************
 * psz_bind_addr, i_bind_port : address and port used for the bind()
 *   system call. If psz_bind_addr == NULL, the socket is bound to
 *   in6addr_any and broadcast reception is enabled. If psz_bind_addr is a
 *   multicast (FF00::/8) address, join the multicast group.
 * psz_server_addr, i_server_port : address and port used for the connect()
 *   system call. It can avoid receiving packets from unauthorized IPs.
 *   Its use leads to great confusion and is currently discouraged.
 * This function returns -1 in case of error.
 *****************************************************************************/
static int OpenUDP( vlc_object_t * p_this )
{
    network_socket_t *p_socket = p_this->p_private;
    const char *psz_bind_addr = p_socket->psz_bind_addr;
    int i_bind_port = p_socket->i_bind_port;
    const char *psz_server_addr = p_socket->psz_server_addr;
    int i_server_port = p_socket->i_server_port;
    int i_handle, i_opt;
    struct sockaddr_in6 loc, rem;
    vlc_value_t val;
    vlc_bool_t do_connect = VLC_TRUE;
#if defined(WIN32) || defined(UNDER_CE)
    char strerror_buf[WINSOCK_STRERROR_SIZE];
# define strerror( x ) winsock_strerror( strerror_buf )
#endif

    p_socket->i_handle = -1;

    /* Build the local socket */
    if ( BuildAddr( p_this, &loc, psz_bind_addr, i_bind_port )
    /* Build socket for remote connection */
      || BuildAddr( p_this, &rem, psz_server_addr, i_server_port ) )
        return 0;

    /* Open a SOCK_DGRAM (UDP) socket, in the AF_INET6 domain, automatic (0)
     * protocol */
    if( (i_handle = socket( AF_INET6, SOCK_DGRAM, 0 )) == -1 )
    {
        msg_Warn( p_this, "cannot create socket (%s)", strerror(errno) );
        return 0;
    }

#ifdef IPV6_V6ONLY
    val.i_int = p_socket->v6only;

    if( setsockopt( i_handle, IPPROTO_IPV6, IPV6_V6ONLY, (void *)&val.i_int,
                    sizeof( val.i_int ) ) )
    {
        msg_Warn( p_this, "IPV6_V6ONLY: %s", strerror( errno ) );
        p_socket->v6only = 1;
    }
#else
    p_socket->v6only = 1;
#endif

#ifdef WIN32
# ifndef IPV6_PROTECTION_LEVEL
#   define IPV6_PROTECTION_LEVEL 23
#  endif
    {
        int i_val = 10 /*PROTECTION_LEVEL_UNRESTRICTED*/;
        setsockopt( i_handle, IPPROTO_IPV6, IPV6_PROTECTION_LEVEL, &i_val,
                    sizeof( i_val ) );
    }
#endif

    /* We may want to reuse an already used socket */
    i_opt = 1;
    if( setsockopt( i_handle, SOL_SOCKET, SO_REUSEADDR,
                    (void *) &i_opt, sizeof( i_opt ) ) == -1 )
    {
        msg_Warn( p_this, "cannot configure socket (SO_REUSEADDR: %s)",
                         strerror(errno) );
        close( i_handle );
        return 0;
    }

    /* Increase the receive buffer size to 1/2MB (8Mb/s during 1/2s) to avoid
     * packet loss caused by scheduling problems */
    i_opt = 0x80000;
    if( setsockopt( i_handle, SOL_SOCKET, SO_RCVBUF,
                    (void *) &i_opt, sizeof( i_opt ) ) == -1 )
    {
        msg_Warn( p_this, "cannot configure socket (SO_RCVBUF: %s)",
                          strerror(errno) );
    }

#if defined(WIN32)
    /* Under Win32 and for multicasting, we bind to IN6ADDR_ANY */
    if( IN6_IS_ADDR_MULTICAST(&loc.sin6_addr) )
    {
        struct sockaddr_in6 sockany = loc;
        sockany.sin6_addr = in6addr_any;
        sockany.sin6_scope_id = 0;

        /* Bind it */
        if( bind( i_handle, (struct sockaddr *)&sockany, sizeof( sockany ) ) < 0 )
        {
            msg_Warn( p_this, "cannot bind socket (%s)", strerror(errno) );
            close( i_handle );
            return 0;
        }
    }
    else
#endif
    /* Bind it */
    if( bind( i_handle, (struct sockaddr *)&loc, sizeof( loc ) ) < 0 )
    {
        msg_Warn( p_this, "cannot bind socket (%s)", strerror(errno) );
        close( i_handle );
        return 0;
    }

    /* Join the multicast group if the socket is a multicast address */
    if( IN6_IS_ADDR_MULTICAST(&loc.sin6_addr) )
    {
        if (memcmp (&rem.sin6_addr, &in6addr_any, 16)
         /*&& ((U32_AT (&sock.sin6_addr) & 0xff30ffff) == 0xff300000)*/)
        {
#ifndef MCAST_JOIN_SOURCE_GROUP
            errno = ENOSYS;
#else
            struct group_source_req imr;
            struct sockaddr_in6 *p_sin6;

            memset (&imr, 0, sizeof (imr));
            imr.gsr_group.ss_family = AF_INET6;
            imr.gsr_source.ss_family = AF_INET6;
            p_sin6 = (struct sockaddr_in6 *)&imr.gsr_group;
            p_sin6->sin6_addr = loc.sin6_addr;
            p_sin6 = (struct sockaddr_in6 *)&imr.gsr_source;
            p_sin6->sin6_addr = rem.sin6_addr;

            msg_Dbg( p_this, "MCAST_JOIN_SOURCE_GROUP multicast request" );
            if( setsockopt( i_handle, IPPROTO_IPV6, MCAST_JOIN_SOURCE_GROUP,
                           (void *)&imr, sizeof(struct group_source_req) ) == 0 )
                do_connect = VLC_FALSE;
            else
#endif
            {

                msg_Err( p_this, "Source specific multicast failed (%s) -"
                          " check if your OS really supports MLDv2",
                          strerror(errno) );
                goto mldv1;
            }
        }
        else
mldv1:
        {
            struct ipv6_mreq     imr;

            memset (&imr, 0, sizeof (imr));
            imr.ipv6mr_interface = loc.sin6_scope_id;
            imr.ipv6mr_multiaddr = loc.sin6_addr;
            msg_Dbg( p_this, "IPV6_JOIN_GROUP multicast request" );
            if (setsockopt(i_handle, IPPROTO_IPV6, IPV6_JOIN_GROUP, (void*) &imr,
#if defined(WIN32)
                         sizeof(imr) + 4
            /* Doesn't work without this
             * - Really? because it's really a buffer overflow... */
#else
                         sizeof(imr)
#endif
                          ))
            {
                msg_Err( p_this, "cannot join multicast group" );
                close( i_handle );
                return 0;
            }
        }
    }

#if !defined (__linux__) && !defined (WIN32)
    else
#endif

    if( memcmp (&rem.sin6_addr, &in6addr_any, 16) )
    {
        int ttl;

        /* Connect the socket */
        if( do_connect
         && connect( i_handle, (struct sockaddr *) &rem, sizeof( rem ) ) )
        {
            msg_Warn( p_this, "cannot connect socket (%s)", strerror(errno) );
            close( i_handle );
            return 0;
        }

        /* Set the time-to-live */
        ttl = p_socket->i_ttl;
        if( ttl <= 0 )
            ttl = config_GetInt( p_this, "ttl" );

        if( ttl > 0 )
        {
            if( IN6_IS_ADDR_MULTICAST(&rem.sin6_addr) )
            {
                if( setsockopt( i_handle, IPPROTO_IPV6, IPV6_MULTICAST_HOPS,
                                (void *)&ttl, sizeof( ttl ) ) < 0 )
                {
                    msg_Err( p_this, "failed to set multicast ttl (%s)",
                             strerror(errno) );
                }
            }
            else
            {
                if( setsockopt( i_handle, IPPROTO_IPV6, IPV6_UNICAST_HOPS,
                                (void *)&ttl, sizeof( ttl ) ) < 0 )
                {
                    msg_Err( p_this, "failed to set unicast ttl (%s)",
                              strerror(errno) );
                }
            }
        }

        /* Set multicast output interface */
        if( IN6_IS_ADDR_MULTICAST(&rem.sin6_addr) )
        {
            char *psz_mif = config_GetPsz( p_this, "miface" );
            if( psz_mif != NULL )
            {
                int intf = if_nametoindex( psz_mif );

                if( intf != 0 )
                {
                    if( setsockopt( i_handle, IPPROTO_IPV6, IPV6_MULTICAST_IF,
                        &intf, sizeof( intf ) ) < 0 )
                    {
                        msg_Err( p_this, "%s as multicast interface: %s",
                                 psz_mif, strerror(errno) );
                        free( psz_mif  );
                        close( i_handle );
                        i_handle = -1;
                    }
                }
                else
                {
                    msg_Err( p_this, "%s: bad IPv6 interface specification",
                             psz_mif );
                    close( i_handle );
                    i_handle = -1;
                }
                free( psz_mif );
                if( i_handle == -1 )
                    return 0;
            }
        }
    }

    p_socket->i_handle = i_handle;

    var_Create( p_this, "mtu", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
    var_Get( p_this, "mtu", &val );
    p_socket->i_mtu = val.i_int;

    return 0;
}
