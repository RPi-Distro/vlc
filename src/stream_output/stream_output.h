/*****************************************************************************
 * stream_output.h : internal stream output
 *****************************************************************************
 * Copyright (C) 2002-2005 the VideoLAN team
 * $Id: 40e5a05bc1df1e85b636d8d252b9a025157ba201 $
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
 *          Laurent Aimar <fenrir@via.ecp.fr>
 *          Eric Petit <titer@videolan.org>
 *          Jean-Paul Saman <jpsaman #_at_# m2x.nl>
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
 ***************************************************************************/

#if defined(__PLUGIN__) || defined(__BUILTIN__) || !defined(__LIBVLC__)
# error This header file can only be included from LibVLC.
#endif

#ifndef VLC_SRC_STREAMOUT_H
# define VLC_SRC_STREAMOUT_H 1

# include <vlc_sout.h>
# include <vlc_network.h>

/****************************************************************************
 * sout_packetizer_input_t: p_sout <-> p_packetizer
 ****************************************************************************/
struct sout_packetizer_input_t
{
    sout_instance_t     *p_sout;

    es_format_t         *p_fmt;

    sout_stream_id_t    *id;
};

#define sout_NewInstance(a,b) __sout_NewInstance(VLC_OBJECT(a),b)
sout_instance_t *  __sout_NewInstance( vlc_object_t *, const char * );
void sout_DeleteInstance( sout_instance_t * );

sout_packetizer_input_t *sout_InputNew( sout_instance_t *, es_format_t * );
int sout_InputDelete( sout_packetizer_input_t * );
int sout_InputSendBuffer( sout_packetizer_input_t *, block_t* );

/* Announce system */

struct session_descriptor_t
{
    struct sockaddr_storage orig;
    socklen_t origlen;
    struct sockaddr_storage addr;
    socklen_t addrlen;

    char *psz_sdp;
    bool b_ssm;
};

struct sap_handler_t *SAP_Create (vlc_object_t *);
void SAP_Destroy (struct sap_handler_t *);
int SAP_Add (struct sap_handler_t *, session_descriptor_t *);
void SAP_Del (struct sap_handler_t *, const session_descriptor_t *);

#endif
