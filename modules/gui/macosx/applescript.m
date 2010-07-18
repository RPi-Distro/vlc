/*****************************************************************************
 * applescript.m: MacOS X AppleScript support
 *****************************************************************************
 * Copyright (C) 2002-2009 the VideoLAN team
 * $Id: f3e8c4cb140c47c0617123d4c78917f1148d9984 $
 *
 * Authors: Derk-Jan Hartman <thedj@users.sourceforge.net>
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
#include "intf.h"
#include "applescript.h"
#include "controls.h"
#include "open.h"

/*****************************************************************************
 * VLGetURLScriptCommand implementation
 *****************************************************************************/
@implementation VLGetURLScriptCommand

- (id)performDefaultImplementation {
    NSString *o_command = [[self commandDescription] commandName];
    NSString *o_urlString = [self directParameter];

    if ( [o_command isEqualToString:@"GetURL"] || [o_command isEqualToString:@"OpenURL"] )
    {
        intf_thread_t * p_intf = VLCIntf;
        playlist_t * p_playlist = pl_Get( p_intf );
        if( p_playlist == NULL )
        {
            return nil;
        }

        if ( o_urlString )
        {
            NSURL * o_url;
            input_item_t *p_input;

            p_input = input_item_New( p_playlist,
                                    [o_urlString fileSystemRepresentation],
                                    [[[NSFileManager defaultManager]
                                    displayNameAtPath: o_urlString] UTF8String] );
            /* FIXME: playlist_AddInput() can fail */
            playlist_AddInput( p_playlist, p_input, PLAYLIST_INSERT,
                               PLAYLIST_END, true, pl_Unlocked );

            vlc_gc_decref( p_input );

            o_url = [NSURL fileURLWithPath: o_urlString];
            if( o_url != nil )
            {
                [[NSDocumentController sharedDocumentController]
                    noteNewRecentDocumentURL: o_url];
            }
        }
    }
    return nil;
}

@end


/*****************************************************************************
 * VLControlScriptCommand implementation
 *****************************************************************************/
/*
 * This entire control command needs a better design. more object oriented.
 * Applescript developers would be very welcome (hartman)
 */
@implementation VLControlScriptCommand

- (id)performDefaultImplementation {
    NSString *o_command = [[self commandDescription] commandName];

    intf_thread_t * p_intf = VLCIntf;
    playlist_t * p_playlist = pl_Get( p_intf );
    if( p_playlist == NULL )
    {
        return nil;
    }
 
    VLCControls * o_controls = (VLCControls *)[[NSApp delegate] controls];
 
    if ( o_controls )
    {
        if ( [o_command isEqualToString:@"play"] )
        {
            [o_controls play:self];
        }
        else if ( [o_command isEqualToString:@"stop"] )
        {
            [o_controls stop:self];
        }
        else if ( [o_command isEqualToString:@"previous"] )
        {
            [o_controls prev:self];
        }
        else if ( [o_command isEqualToString:@"next"] )
        {
            [o_controls next:self];
        }
        else if ( [o_command isEqualToString:@"fullscreen"] )
        {
            [o_controls toogleFullscreen: self];
        }
        else if ( [o_command isEqualToString:@"mute"] )
        {
            [o_controls mute:self];
        }
        else if ( [o_command isEqualToString:@"volumeUp"] )
        {
            [o_controls volumeUp:self];
        }
        else if ( [o_command isEqualToString:@"volumeDown"] )
        {
            [o_controls volumeDown:self];
        }
    }
    return nil;
}

@end

/*****************************************************************************
 * Category that adds AppleScript support to NSApplication
 *****************************************************************************/
@implementation NSApplication(ScriptSupport)

- (BOOL) scriptFullscreenMode {    
    VLCControls * o_controls = (VLCControls *)[[self delegate] controls];

    return [o_controls isFullscreen];
}
- (void) setScriptFullscreenMode: (BOOL) mode {
    VLCControls * o_controls = (VLCControls *)[[self delegate] controls];
    if (mode == [o_controls isFullscreen]) return;
    [o_controls toogleFullscreen: self];
}

@end
