/*****************************************************************************
 * applescript.m: MacOS X AppleScript support
 *****************************************************************************
 * Copyright (C) 2002-2009 VLC authors and VideoLAN
 * $Id: 3ac23b5370b2442d7d8ed360197cd380ad095888 $
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
#include "CoreInteraction.h"
#include "vlc_aout_intf.h"

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

        if ( o_urlString )
        {
            NSURL * o_url;
            input_item_t *p_input;
            int returnValue;

            p_input = input_item_New( [o_urlString fileSystemRepresentation],
                                    [[[NSFileManager defaultManager]
                                    displayNameAtPath: o_urlString] UTF8String] );
            if (!p_input)
                return nil;

            returnValue = playlist_AddInput( p_playlist, p_input, PLAYLIST_INSERT,
                               PLAYLIST_END, true, pl_Unlocked );
            vlc_gc_decref( p_input );

            if (returnValue != VLC_SUCCESS)
                return nil;

            o_url = [NSURL fileURLWithPath: o_urlString];
            if( o_url != nil )
                [[NSDocumentController sharedDocumentController] noteNewRecentDocumentURL: o_url];
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
    NSString *o_parameter = [self directParameter];

    intf_thread_t * p_intf = VLCIntf;
    playlist_t * p_playlist = pl_Get( p_intf );
    if( p_playlist == NULL )
    {
        return nil;
    }
 
    if ( [o_command isEqualToString:@"play"] )
    {
        [[VLCCoreInteraction sharedInstance] play];
    }
    else if ( [o_command isEqualToString:@"stop"] )
    {
        [[VLCCoreInteraction sharedInstance] stop];
    }
    else if ( [o_command isEqualToString:@"previous"] )
    {
        [[VLCCoreInteraction sharedInstance] previous];
    }
    else if ( [o_command isEqualToString:@"next"] )
    {
        [[VLCCoreInteraction sharedInstance] next];
    }
    else if ( [o_command isEqualToString:@"fullscreen"] )
    {
        [[VLCCoreInteraction sharedInstance] toggleFullscreen];
    }
    else if ( [o_command isEqualToString:@"mute"] )
    {
        [[VLCCoreInteraction sharedInstance] mute];
    }
    else if ( [o_command isEqualToString:@"volumeUp"] )
    {
        [[VLCCoreInteraction sharedInstance] volumeUp];
    }
    else if ( [o_command isEqualToString:@"volumeDown"] )
    {
        [[VLCCoreInteraction sharedInstance] volumeDown];
    }
        else if ( [o_command isEqualToString:@"stepForward"] )
    {
        //default: forwardShort
        if (o_parameter)
        {
            int i_parameter = [o_parameter intValue];
            switch (i_parameter)
            {
                case 1:
                    [[VLCCoreInteraction sharedInstance] forwardExtraShort];
                    break;
                case 2:
                    [[VLCCoreInteraction sharedInstance] forwardShort];
                    break;
                case 3:
                    [[VLCCoreInteraction sharedInstance] forwardMedium];
                    break;
                case 4:
                    [[VLCCoreInteraction sharedInstance] forwardLong];
                    break;
                default:
                    [[VLCCoreInteraction sharedInstance] forwardShort];
                    break;
            }
        }
        else
        {
            [[VLCCoreInteraction sharedInstance] forwardShort];
        }
    }
    else if ( [o_command isEqualToString:@"stepBackward"] )
    {
        //default: backwardShort
        if (o_parameter)
        {
            int i_parameter = [o_parameter intValue];
            switch (i_parameter)
            {
                case 1:
                    [[VLCCoreInteraction sharedInstance] backwardExtraShort];
                    break;
                case 2:
                    [[VLCCoreInteraction sharedInstance] backwardShort];
                    break;
                case 3:
                    [[VLCCoreInteraction sharedInstance] backwardMedium];
                    break;
                case 4:
                    [[VLCCoreInteraction sharedInstance] backwardLong];
                    break;
                default:
                    [[VLCCoreInteraction sharedInstance] backwardShort];
                    break;
            }
        }
        else
        {
            [[VLCCoreInteraction sharedInstance] backwardShort];
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
    vout_thread_t * p_vout = getVout();
    if( !p_vout )
        return NO;
    BOOL b_value = var_GetBool( p_vout, "fullscreen");
    vlc_object_release( p_vout );
    return b_value;
}
- (void) setScriptFullscreenMode: (BOOL) mode {
    vout_thread_t * p_vout = getVout();
    if( !p_vout )
        return;
    if (var_GetBool( p_vout, "fullscreen") == mode)
    {
        vlc_object_release( p_vout );
        return;
    }
    vlc_object_release( p_vout );
    [[VLCCoreInteraction sharedInstance] toggleFullscreen];
}

- (BOOL) muted {
    return [[VLCCoreInteraction sharedInstance] isMuted];
}

- (BOOL) playing {
    return [[VLCCoreInteraction sharedInstance] isPlaying];
}

- (int) audioVolume {
    return ( [[VLCCoreInteraction sharedInstance] volume] );
}

- (void) setAudioVolume: (int) i_audioVolume {
    [[VLCCoreInteraction sharedInstance] setVolume:(int)i_audioVolume];
}

- (int) currentTime {
    return [[VLCCoreInteraction sharedInstance] currentTime];
}

- (void) setCurrentTime: (int) i_currentTime {
    if (i_currentTime)
        [[VLCCoreInteraction sharedInstance] setCurrentTime:i_currentTime];
}

#pragma mark -
//TODO:whenever VLC should implement NSDocument, the methods below should move or be additionaly implemented in the NSDocument category
- (int) durationOfCurrentItem {
    return [[VLCCoreInteraction sharedInstance] durationOfCurrentPlaylistItem];
}

- (NSString*) pathOfCurrentItem {
    return [[[VLCCoreInteraction sharedInstance] URLOfCurrentPlaylistItem] path];
}

- (NSString*) nameOfCurrentItem {
    return [[VLCCoreInteraction sharedInstance] nameOfCurrentPlaylistItem];
}

@end
