/*****************************************************************************
 * vlc_services_discovery.h : Services Discover functions
 *****************************************************************************
 * Copyright (C) 1999-2004 the VideoLAN team
 * $Id: 956b77b65f943f9660858ebc07d51a9e27daea36 $
 *
 * Authors: Pierre d'Herbemont <pdherbemont # videolan.org>
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

#ifndef VLC_SERVICES_DISCOVERY_H_
#define VLC_SERVICES_DISCOVERY_H_

#include <vlc_input.h>
#include <vlc_events.h>
#include <vlc_probe.h>

/**
 * \file
 * This file functions and structures for service discovery in vlc
 */

# ifdef __cplusplus
extern "C" {
# endif

/*
 * @{
 */

struct services_discovery_t
{
    VLC_COMMON_MEMBERS
    module_t *          p_module;

    vlc_event_manager_t event_manager;      /* Accessed through Setters for non class function */

    char *psz_name;
    config_chain_t *p_cfg;

    services_discovery_sys_t *p_sys;
};

enum services_discovery_category_e
{
    SD_CAT_DEVICES = 1,
    SD_CAT_LAN,
    SD_CAT_INTERNET,
    SD_CAT_MYCOMPUTER
};

/***********************************************************************
 * Service Discovery
 ***********************************************************************/

/* Get the services discovery modules names to use in Create(), in a null
 * terminated string array. Array and string must be freed after use. */
VLC_EXPORT( char **, vlc_sd_GetNames, ( vlc_object_t *, char ***, int ** ) );
#define vlc_sd_GetNames(obj, pln, pcat ) \
        vlc_sd_GetNames(VLC_OBJECT(obj), pln, pcat)

/* Creation of a service_discovery object */
VLC_EXPORT( services_discovery_t *, vlc_sd_Create, ( vlc_object_t *, const char * ) );
VLC_EXPORT( bool, vlc_sd_Start, ( services_discovery_t * ) );
VLC_EXPORT( void, vlc_sd_Stop, ( services_discovery_t * ) );
VLC_EXPORT( void, vlc_sd_Destroy, ( services_discovery_t * ) );

static inline void vlc_sd_StopAndDestroy( services_discovery_t * p_this )
{
    vlc_sd_Stop( p_this );
    vlc_sd_Destroy( p_this );
}

/* Read info from discovery object */
VLC_EXPORT( char *,                 services_discovery_GetLocalizedName, ( services_discovery_t * p_this ) );

/* Receive event notification (preferred way to get new items) */
VLC_EXPORT( vlc_event_manager_t *,  services_discovery_EventManager, ( services_discovery_t * p_this ) );

/* Used by services_discovery to post update about their items */
    /* About the psz_category, it is a legacy way to add info to the item,
     * for more options, directly set the (meta) data on the input item */
VLC_EXPORT( void,                   services_discovery_AddItem, ( services_discovery_t * p_this, input_item_t * p_item, const char * psz_category ) );
VLC_EXPORT( void,                   services_discovery_RemoveItem, ( services_discovery_t * p_this, input_item_t * p_item ) );


/* SD probing */

VLC_EXPORT(int, vlc_sd_probe_Add, (vlc_probe_t *, const char *, const char *, int category));

#define VLC_SD_PROBE_SUBMODULE \
    add_submodule() \
        set_capability( "services probe", 100 ) \
        set_callbacks( vlc_sd_probe_Open, NULL )

#define VLC_SD_PROBE_HELPER(name, longname, cat) \
static int vlc_sd_probe_Open (vlc_object_t *obj) \
{ \
    return vlc_sd_probe_Add ((struct vlc_probe_t *)obj, \
                             name "{longname=\"" longname "\"}", \
                             longname, cat); \
}

/** @} */
# ifdef __cplusplus
}
# endif

#endif
