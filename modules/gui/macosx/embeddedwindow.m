/*****************************************************************************
* embeddedwindow.m: MacOS X interface module
*****************************************************************************
* Copyright (C) 2002-2005 the VideoLAN team
* $Id: 3bb1ea9a38b042486f837029f401ecd63700e444 $
*
* Authors: Benjamin Pracht <bigben at videolan dot org>
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

/* DisableScreenUpdates, SetSystemUIMode, ... */
#import <QuickTime/QuickTime.h>

#include "intf.h"
#include "vout.h"
#include "embeddedwindow.h"
#import "fspanel.h"
#import "controls.h"

/*****************************************************************************
* VLCEmbeddedWindow Implementation
*****************************************************************************/

@implementation VLCEmbeddedWindow

- (void)awakeFromNib
{
    [self setDelegate: self];
    
    [o_btn_backward setToolTip: _NS("Rewind")];
    [o_btn_forward setToolTip: _NS("Fast Forward")];
    [o_btn_fullscreen setToolTip: _NS("Fullscreen")];
    [o_btn_play setToolTip: _NS("Play")];
    [o_slider setToolTip: _NS("Position")];
    
    o_img_play = [NSImage imageNamed: @"play_embedded"];
    o_img_play_pressed = [NSImage imageNamed: @"play_embedded_blue"];
    o_img_pause = [NSImage imageNamed: @"pause_embedded"];
    o_img_pause_pressed = [NSImage imageNamed: @"pause_embedded_blue"];
    
    o_saved_frame = NSMakeRect( 0.0f, 0.0f, 0.0f, 0.0f );
    
    /* Useful to save o_view frame in fullscreen mode */
    o_temp_view = [[NSView alloc] init];
    [o_temp_view setAutoresizingMask:NSViewHeightSizable | NSViewWidthSizable];
    
    o_fullscreen_window = nil;
    
    /* Not fullscreen when we wake up */
    [o_btn_fullscreen setState: NO];
    b_fullscreen = NO;
    /* Use a recursive lock to be able to trigger enter/leavefullscreen
        * in middle of an animation, providing that the enter/leave functions
        * are called from the same thread */
    o_animation_lock = [[NSRecursiveLock alloc] init];
}

- (void)setTime:(NSString *)o_arg_time position:(float)f_position
{
    [o_time setStringValue: o_arg_time];
    [o_slider setFloatValue: f_position];
}

- (void)playStatusUpdated:(int)i_status
{
    if( i_status == PLAYING_S )
    {
        [o_btn_play setImage: o_img_pause];
        [o_btn_play setAlternateImage: o_img_pause_pressed];
        [o_btn_play setToolTip: _NS("Pause")];
    }
    else
    {
        [o_btn_play setImage: o_img_play];
        [o_btn_play setAlternateImage: o_img_play_pressed];
        [o_btn_play setToolTip: _NS("Play")];
    }
}

- (void)setSeekable:(BOOL)b_seekable
{
    [o_btn_forward setEnabled: b_seekable];
    [o_btn_backward setEnabled: b_seekable];
    [o_slider setEnabled: b_seekable];
}

- (void)setFullscreen:(BOOL)b_fullscreen_state
{
    [o_btn_fullscreen setState: b_fullscreen_state];
}

- (void)zoom:(id)sender
{
    if( ![self isZoomed] )
    {
        NSRect zoomRect = [[self screen] frame];
        o_saved_frame = [self frame];
        /* we don't have to take care of the eventual menu bar and dock
            as zoomRect will be cropped automatically by setFrame:display:
            to the right rectangle */
        [self setFrame: zoomRect display: YES animate: YES];
    }
    else
    {
        /* unzoom to the saved_frame if the o_saved_frame coords look sound
        (just in case) */
        if( o_saved_frame.size.width > 0 && o_saved_frame.size.height > 0 )
            [self setFrame: o_saved_frame display: YES animate: YES];
    }
}

- (BOOL)windowShouldClose:(id)sender
{
    playlist_t * p_playlist = vlc_object_find( VLCIntf, VLC_OBJECT_PLAYLIST,
                                               FIND_ANYWHERE );
    if( p_playlist == NULL )
    {
        return NO;
    }
    
    playlist_Stop( p_playlist );
    vlc_object_release( p_playlist );
    return YES;
}

- (NSView *)mainView
{
    if (o_fullscreen_window)
        return o_temp_view;
    else
        return o_view;
}

/*****************************************************************************
* Fullscreen support
*/

- (BOOL)isFullscreen
{
    return b_fullscreen;
}

- (void)lockFullscreenAnimation
{
    [o_animation_lock lock];
}

- (void)unlockFullscreenAnimation
{
    [o_animation_lock unlock];
}

- (void)enterFullscreen
{
    NSScreen *screen;
    NSRect screen_rect;
    NSRect rect;
    vout_thread_t *p_vout = vlc_object_find( VLCIntf, VLC_OBJECT_VOUT, FIND_ANYWHERE );
    BOOL blackout_other_displays = config_GetInt( VLCIntf, "macosx-black" );

    screen = [NSScreen screenWithDisplayID: (CGDirectDisplayID)var_GetInteger( p_vout, "video-device" )]; 
 	if( !screen ) 
    {
        msg_Dbg( p_vout, "chosen screen isn't present, using current screen for fullscreen mode" );
        screen = [self screen];
    }

    [self lockFullscreenAnimation];

    vlc_object_release( p_vout );
    
    screen_rect = [screen frame];
    
    [o_btn_fullscreen setState: YES];
    
    [NSCursor setHiddenUntilMouseMoves: YES];
    
    /* Only create the o_fullscreen_window if we are not in the middle of the zooming animation */
    if (!o_fullscreen_window)
    {
        /* We can't change the styleMask of an already created NSWindow, so we create an other window, and do eye catching stuff */
        
        rect = [[o_view superview] convertRect: [o_view frame] toView: nil]; /* Convert to Window base coord */
        rect.origin.x += [self frame].origin.x;
        rect.origin.y += [self frame].origin.y;
        o_fullscreen_window = [[VLCWindow alloc] initWithContentRect:rect styleMask: NSBorderlessWindowMask backing:NSBackingStoreBuffered defer:YES];
        [o_fullscreen_window setBackgroundColor: [NSColor blackColor]];
        [o_fullscreen_window setCanBecomeKeyWindow: YES];
        
        CGDisplayFadeReservationToken token;
        
        [o_fullscreen_window setFrame:screen_rect display:NO];
        
        CGAcquireDisplayFadeReservation(kCGMaxDisplayReservationInterval, &token);
        CGDisplayFade( token, 0.3, kCGDisplayBlendNormal, kCGDisplayBlendSolidColor, 0, 0, 0, YES );
        
        if ([screen isMainScreen])
            SetSystemUIMode( kUIModeAllHidden, kUIOptionAutoShowMenuBar);
        
        if (blackout_other_displays)
            [screen blackoutOtherScreens];

        [o_view retain];
        [[self contentView] replaceSubview:o_view with:o_temp_view];
        [o_temp_view setFrame:[o_view frame]];
        [o_fullscreen_window setContentView:o_view];
        [o_view release];
        [o_fullscreen_window makeKeyAndOrderFront:self];
        
        CGDisplayFade( token, 0.5, kCGDisplayBlendSolidColor, kCGDisplayBlendNormal, 0, 0, 0, NO );
        CGReleaseDisplayFadeReservation( token);
        
        /* Will release the lock */
        [self hasBecomeFullscreen];
        return;
    }

    [self unlockFullscreenAnimation];
}

- (void)hasBecomeFullscreen
{
    [o_fullscreen_window makeFirstResponder: [[[VLCMain sharedInstance] getControls] getVoutView]];
    
    [o_fullscreen_window makeKeyWindow];
    [o_fullscreen_window setAcceptsMouseMovedEvents: TRUE];
    
    /* tell the fspanel to move itself to front next time it's triggered */
    [[[[VLCMain sharedInstance] getControls] getFSPanel] setVoutWasUpdated: [o_fullscreen_window screen]];
    
    [super orderOut: self];
    
    [[[[VLCMain sharedInstance] getControls] getFSPanel] setActive: nil];
    b_fullscreen = YES;
    [self unlockFullscreenAnimation];
}

- (void)leaveFullscreen
{
    [self leaveFullscreenAndFadeOut: NO];
}

- (void)leaveFullscreenAndFadeOut: (BOOL)fadeout
{
    [self lockFullscreenAnimation];
    
    b_fullscreen = NO;
    [o_btn_fullscreen setState: NO];
    
    /* Don't do anything if o_fullscreen_window is already closed */
    if (!o_fullscreen_window)
    {
        /* We always try to do so */
        [NSScreen unblackoutScreens];

        [self unlockFullscreenAnimation];
        return;
    }
    
    /* simply fade the display */
    CGDisplayFadeReservationToken token;
    
    CGAcquireDisplayFadeReservation(kCGMaxDisplayReservationInterval, &token);
    CGDisplayFade( token, 0.3, kCGDisplayBlendNormal, kCGDisplayBlendSolidColor, 0, 0, 0, YES );
    
    [[[[VLCMain sharedInstance] getControls] getFSPanel] setNonActive: nil];
    SetSystemUIMode( kUIModeNormal, 0);
    
    /* We always try to do so */
    [NSScreen unblackoutScreens];
    
    if (!fadeout)
        [self orderFront: self];
    
    /* Will release the lock */
    [self hasEndedFullscreen];
    
    CGDisplayFade( token, 0.5, kCGDisplayBlendSolidColor, kCGDisplayBlendNormal, 0, 0, 0, NO );
    CGReleaseDisplayFadeReservation( token);
}

- (void)hasEndedFullscreen
{
    /* This function is private and should be only triggered at the end of the fullscreen change animation */
    /* Make sure we don't see the o_view disappearing of the screen during this operation */
    DisableScreenUpdates();
    [o_view retain];
    [o_view removeFromSuperviewWithoutNeedingDisplay];
    [[self contentView] replaceSubview:o_temp_view with:o_view];
    [o_view release];
    [o_view setFrame:[o_temp_view frame]];
    [self makeFirstResponder: o_view];
    if ([self isVisible])
        [self makeKeyAndOrderFront:self];
    [o_fullscreen_window orderOut: self];
    EnableScreenUpdates();
    
    [o_fullscreen_window release];
    o_fullscreen_window = nil;
    [self unlockFullscreenAnimation];
}

- (void)orderOut: (id)sender
{
    [super orderOut: sender];
    /* Make sure we leave fullscreen */
    [self leaveFullscreenAndFadeOut: YES];
}

/* Make sure setFrame gets executed on main thread especially if we are animating.
* (Thus we won't block the video output thread) */
- (void)setFrame:(NSRect)frame display:(BOOL)display animate:(BOOL)animate
{
    struct { NSRect frame; BOOL display; BOOL animate;} args;
    NSData *packedargs;
    
    args.frame = frame;
    args.display = display;
    args.animate = animate;
    
    packedargs = [NSData dataWithBytes:&args length:sizeof(args)];
    
    [self performSelectorOnMainThread:@selector(setFrameOnMainThread:)
                           withObject: packedargs waitUntilDone: YES];
}

- (void)setFrameOnMainThread:(NSData*)packedargs
{
    struct args { NSRect frame; BOOL display; BOOL animate; } * args = (struct args*)[packedargs bytes];
    
    [super setFrame: args->frame display: args->display animate:args->animate];
}

@end
