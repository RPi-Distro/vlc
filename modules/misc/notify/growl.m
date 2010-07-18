/*****************************************************************************
 * growl.m : growl notification plugin
 *****************************************************************************
 * VLC specific code:
 * 
 * Copyright © 2008 the VideoLAN team
 * $Id: 71ecdd1033e078797f4c26102bf55b7b56210358 $
 *
 * Authors: Rafaël Carré <funman@videolanorg>
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
 *
 * Growl specific code, ripped from growlnotify:
 *
 * Copyright (c) The Growl Project, 2004-2005
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 notice, this list of conditions and the following disclaimer in the
 documentation and/or other materials provided with the distribution.
 * 3. Neither the name of Growl nor the names of its contributors
 may be used to endorse or promote products derived from this software
 without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#import <Foundation/Foundation.h>
#import <Growl/GrowlDefines.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_playlist.h>
#include <vlc_meta.h>
#include <vlc_interface.h>
#include <vlc_url.h>


/*****************************************************************************
 * intf_sys_t
 *****************************************************************************/
struct intf_sys_t
{
    CFDataRef           default_icon;
    NSAutoreleasePool   *p_pool;
    CFStringRef         app_name;
    CFStringRef         notification_type;
};

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Open    ( vlc_object_t * );
static void Close   ( vlc_object_t * );

static int ItemChange( vlc_object_t *, const char *,
                       vlc_value_t, vlc_value_t, void * );

static void RegisterToGrowl( vlc_object_t * );
static void NotifyToGrowl( intf_thread_t *, const char *, CFDataRef );

static CFDataRef readFile(const char *);

/*****************************************************************************
 * Module descriptor
 ****************************************************************************/

vlc_module_begin ()
    set_category( CAT_INTERFACE )
    set_subcategory( SUBCAT_INTERFACE_CONTROL )
    set_shortname( "Growl" )
    set_description( N_("Growl Notification Plugin") )
    set_capability( "interface", 0 )
    set_callbacks( Open, Close )
vlc_module_end ()

/*****************************************************************************
 * Open: initialize and create stuff
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_this;
    intf_sys_t    *p_sys;

    p_sys = p_intf->p_sys = calloc( 1, sizeof(intf_sys_t) );
    if( !p_sys )
        return VLC_ENOMEM;

    p_sys->p_pool = [[NSAutoreleasePool alloc] init];
    p_sys->app_name = CFSTR( "VLC media player" );
    p_sys->notification_type = CFSTR( "New input playing" );

    char *data_path = config_GetDataDir ( p_this );
    char buf[strlen (data_path) + sizeof ("/vlc48x48.png")];
    snprintf (buf, sizeof (buf), "%s/vlc48x48.png", data_path);
    free( data_path );
    p_sys->default_icon = (CFDataRef) readFile( buf );

    var_AddCallback( pl_Get( p_intf ), "item-current", ItemChange, p_intf );

    RegisterToGrowl( p_this );
    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: destroy interface stuff
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    intf_sys_t *p_sys = ((intf_thread_t*)p_this)->p_sys;

    var_DelCallback( pl_Get( p_this ), "item-current", ItemChange, p_this );

    CFRelease( p_sys->default_icon );
    CFRelease( p_sys->app_name );
    CFRelease( p_sys->notification_type );
    [p_sys->p_pool release];
    free( p_sys );
}

/*****************************************************************************
 * ItemChange: Playlist item change callback
 *****************************************************************************/
static int ItemChange( vlc_object_t *p_this, const char *psz_var,
                       vlc_value_t oldval, vlc_value_t newval, void *param )
{
    VLC_UNUSED(psz_var); VLC_UNUSED(oldval); VLC_UNUSED(newval);

    intf_thread_t *p_intf   = (intf_thread_t*)param;
    char *psz_tmp           = NULL;
    char *psz_title         = NULL;
    char *psz_artist        = NULL;
    char *psz_album         = NULL;
    input_thread_t *p_input;
    p_input = playlist_CurrentInput( (playlist_t*)p_this );

    if( !p_input ) return VLC_SUCCESS;

    char *psz_name = input_item_GetName( input_GetItem( p_input ) );
    if( p_input->b_dead || !psz_name )
    {
        /* Not playing anything ... */
        free( psz_name );
        vlc_object_release( p_input );
        return VLC_SUCCESS;
    }
    free( psz_name );

    /* Playing something ... */
    input_item_t *p_item = input_GetItem( p_input );

    psz_title = input_item_GetTitleFbName( p_item );
    if( EMPTY_STR( psz_title ) )
    {
        free( psz_title );
        vlc_object_release( p_input );
        return VLC_SUCCESS;
    }

    psz_artist = input_item_GetArtist( p_item );
    if( EMPTY_STR( psz_artist ) ) FREENULL( psz_artist );
    psz_album = input_item_GetAlbum( p_item ) ;
    if( EMPTY_STR( psz_album ) ) FREENULL( psz_album );

    int i_ret;
    if( psz_artist && psz_album )
        i_ret = asprintf( &psz_tmp, "%s\n%s [%s]",
                psz_title, psz_artist, psz_album );
    else if( psz_artist )
        i_ret = asprintf( &psz_tmp, "%s\n%s", psz_title, psz_artist );
    else
    {
        psz_tmp = strdup( psz_title );
        if( psz_tmp == NULL )
           i_ret = -1;
    }

    if( i_ret == -1 )
    {
        free( psz_title );
        free( psz_artist );
        free( psz_album );
        vlc_object_release( p_input );
        return VLC_ENOMEM;
    }

    char *psz_arturl = input_item_GetArtURL( p_item );
    if( psz_arturl )
    {
        char *psz = make_path( psz_arturl );
        free( psz_arturl );
        psz_arturl = psz;
    }
    CFDataRef art = NULL;
    if( psz_arturl )
        art = (CFDataRef) readFile( psz_arturl );

    free( psz_title );
    free( psz_artist );
    free( psz_album );
    free( psz_arturl );

    NotifyToGrowl( p_intf, psz_tmp, art );

    if( art ) CFRelease( art );
    free( psz_tmp );

    vlc_object_release( p_input );
    return VLC_SUCCESS;
}

/*****************************************************************************
 * RegisterToGrowl
 *****************************************************************************/
static void RegisterToGrowl( vlc_object_t *p_this )
{
    intf_sys_t *p_sys = ((intf_thread_t *)p_this)->p_sys;

    CFArrayRef defaultAndAllNotifications = CFArrayCreate(
        kCFAllocatorDefault, (const void **)&(p_sys->notification_type), 1,
        &kCFTypeArrayCallBacks );
    
    CFTypeRef registerKeys[4] = {
        GROWL_APP_NAME,
        GROWL_NOTIFICATIONS_ALL,
        GROWL_NOTIFICATIONS_DEFAULT,
        GROWL_APP_ICON
    };

    CFTypeRef registerValues[4] = {
        p_sys->app_name,
        defaultAndAllNotifications,
        defaultAndAllNotifications,
        p_sys->default_icon
    };

    CFDictionaryRef registerInfo = CFDictionaryCreate(
        kCFAllocatorDefault, registerKeys, registerValues, 4,
        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );

    CFRelease( defaultAndAllNotifications );

    CFNotificationCenterPostNotificationWithOptions(
        CFNotificationCenterGetDistributedCenter(),
        (CFStringRef)GROWL_APP_REGISTRATION, NULL, registerInfo,
        kCFNotificationPostToAllSessions );
    CFRelease( registerInfo );
}

static void NotifyToGrowl( intf_thread_t *p_intf, const char *psz_desc, CFDataRef art )
{
    intf_sys_t *p_sys = p_intf->p_sys;

    CFStringRef title = CFStringCreateWithCString( kCFAllocatorDefault, _("Now playing"), kCFStringEncodingUTF8 );
    CFStringRef desc = CFStringCreateWithCString( kCFAllocatorDefault, psz_desc, kCFStringEncodingUTF8 );

    CFMutableDictionaryRef notificationInfo = CFDictionaryCreateMutable(
        kCFAllocatorDefault, 5, &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks);

    CFDictionarySetValue( notificationInfo, GROWL_NOTIFICATION_NAME, p_sys->notification_type );
    CFDictionarySetValue( notificationInfo, GROWL_APP_NAME, p_sys->app_name );
    CFDictionarySetValue( notificationInfo, GROWL_NOTIFICATION_TITLE, title );
    CFDictionarySetValue( notificationInfo, GROWL_NOTIFICATION_DESCRIPTION, desc );

    CFDictionarySetValue( notificationInfo, GROWL_NOTIFICATION_ICON, 
        art ? art : p_sys->default_icon );

    CFRelease( title );
    CFRelease( desc );

    CFNotificationCenterPostNotificationWithOptions(
        CFNotificationCenterGetDistributedCenter(),
        (CFStringRef)GROWL_NOTIFICATION, NULL, notificationInfo,
        kCFNotificationPostToAllSessions );

    CFRelease( notificationInfo );
}

/* Ripped from CFGrowlAdditions.c 
 * Strangely, this function does exist in Growl shared library, but is not
 * defined in public header files */
static CFDataRef readFile(const char *filename)
{
    CFDataRef data;
    // read the file into a CFDataRef
    FILE *fp = fopen(filename, "r");
    if( !fp )
    return NULL;

    fseek(fp, 0, SEEK_END);
    long dataLength = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    unsigned char *fileData = malloc(dataLength);
    fread(fileData, 1, dataLength, fp);
    fclose(fp);
    return CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, fileData,
        dataLength, kCFAllocatorMalloc);
}

