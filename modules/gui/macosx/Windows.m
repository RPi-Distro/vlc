/*****************************************************************************
 * Windows.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2012-2014 VLC authors and VideoLAN
 * $Id: 331da6158e814dd141d4403357b7d5ef340d594b $
 *
 * Authors: Felix Paul Kühne <fkuehne -at- videolan -dot- org>
 *          David Fuhrmann <david dot fuhrmann at googlemail dot com>
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

#import "Windows.h"
#import "intf.h"
#import "CoreInteraction.h"
#import "ControlsBar.h"
#import "VideoView.h"
#import "CompatibilityFixes.h"

/*****************************************************************************
 * VLCWindow
 *
 *  Missing extension to NSWindow
 *****************************************************************************/

@implementation VLCWindow

@synthesize hasActiveVideo=b_has_active_video;
@synthesize fullscreen=b_fullscreen;

- (id)initWithContentRect:(NSRect)contentRect styleMask:(NSUInteger)styleMask
                  backing:(NSBackingStoreType)backingType defer:(BOOL)flag
{
    self = [super initWithContentRect:contentRect styleMask:styleMask backing:backingType defer:flag];
    if (self) {
        /* we don't want this window to be restored on relaunch */
        if (!OSX_SNOW_LEOPARD)
            [self setRestorable:NO];
    }
    return self;
}

- (void)setCanBecomeKeyWindow: (BOOL)canBecomeKey
{
    b_isset_canBecomeKeyWindow = YES;
    b_canBecomeKeyWindow = canBecomeKey;
}

- (BOOL)canBecomeKeyWindow
{
    if (b_isset_canBecomeKeyWindow)
        return b_canBecomeKeyWindow;

    return [super canBecomeKeyWindow];
}

- (void)setCanBecomeMainWindow: (BOOL)canBecomeMain
{
    b_isset_canBecomeMainWindow = YES;
    b_canBecomeMainWindow = canBecomeMain;
}

- (BOOL)canBecomeMainWindow
{
    if (b_isset_canBecomeMainWindow)
        return b_canBecomeMainWindow;

    return [super canBecomeMainWindow];
}

- (void)closeAndAnimate: (BOOL)animate
{
    NSInvocation *invoc;

    if (!animate) {
        [super close];
        return;
    }

    // TODO this callback stuff does not work and is not needed
    invoc = [[[NSInvocation alloc] init] autorelease];
    [invoc setSelector:@selector(close)];
    [invoc setTarget: self];

    if (![self isVisible] || [self alphaValue] == 0.0) {
        [super close];
        return;
    }

    [self orderOut: self animate: YES callback: invoc];
}

- (void)orderOut: (id)sender animate: (BOOL)animate
{
    NSInvocation *invoc = [[[NSInvocation alloc] init] autorelease];
    [invoc setSelector:@selector(orderOut:)];
    [invoc setTarget: self];
    [invoc setArgument: sender atIndex: 2];
    [self orderOut: sender animate: animate callback: invoc];
}

- (void)orderOut: (id)sender animate: (BOOL)animate callback:(NSInvocation *)callback
{
    NSViewAnimation *anim;
    NSViewAnimation *current_anim;
    NSMutableDictionary *dict;

    if (!animate) {
        [self orderOut: sender];
        return;
    }

    dict = [[NSMutableDictionary alloc] initWithCapacity:2];

    [dict setObject:self forKey:NSViewAnimationTargetKey];

    [dict setObject:NSViewAnimationFadeOutEffect forKey:NSViewAnimationEffectKey];
    anim = [[NSViewAnimation alloc] initWithViewAnimations:[NSArray arrayWithObject:dict]];
    [dict release];

    [anim setAnimationBlockingMode:NSAnimationNonblocking];
    [anim setDuration:0.9];
    [anim setFrameRate:30];
    [anim setUserInfo:callback];
    [anim setDelegate:self];

    @synchronized(self) {
        current_anim = self->o_current_animation;

        if ([[[current_anim viewAnimations] objectAtIndex:0] objectForKey: NSViewAnimationEffectKey] == NSViewAnimationFadeOutEffect && [current_anim isAnimating]) {
            [anim release];
        } else {
            if (current_anim) {
                [current_anim stopAnimation];
                [anim setCurrentProgress:1.0 - [current_anim currentProgress]];
                [current_anim release];
            }
            else
                [anim setCurrentProgress:1.0 - [self alphaValue]];
            self->o_current_animation = anim;
            [anim startAnimation];
        }
    }
}

- (void)orderFront: (id)sender animate: (BOOL)animate
{
    NSViewAnimation *anim;
    NSViewAnimation *current_anim;
    NSMutableDictionary *dict;

    if (!animate) {
        [super orderFront: sender];
        [self setAlphaValue: 1.0];
        return;
    }

    if (![self isVisible]) {
        [self setAlphaValue: 0.0];
        [super orderFront: sender];
    }
    else if ([self alphaValue] == 1.0) {
        [super orderFront: self];
        return;
    }

    dict = [[NSMutableDictionary alloc] initWithCapacity:2];

    [dict setObject:self forKey:NSViewAnimationTargetKey];

    [dict setObject:NSViewAnimationFadeInEffect forKey:NSViewAnimationEffectKey];
    anim = [[NSViewAnimation alloc] initWithViewAnimations:[NSArray arrayWithObject:dict]];
    [dict release];

    [anim setAnimationBlockingMode:NSAnimationNonblocking];
    [anim setDuration:0.5];
    [anim setFrameRate:30];
    [anim setDelegate:self];

    @synchronized(self) {
        current_anim = self->o_current_animation;

        if ([[[current_anim viewAnimations] objectAtIndex:0] objectForKey: NSViewAnimationEffectKey] == NSViewAnimationFadeInEffect && [current_anim isAnimating]) {
            [anim release];
        } else {
            if (current_anim) {
                [current_anim stopAnimation];
                [anim setCurrentProgress:1.0 - [current_anim currentProgress]];
                [current_anim release];
            }
            else
                [anim setCurrentProgress:[self alphaValue]];
            self->o_current_animation = anim;
            [self orderFront: sender];
            [anim startAnimation];
        }
    }
}

- (void)animationDidEnd:(NSAnimation*)anim
{
    if ([self alphaValue] <= 0.0) {
        NSInvocation * invoc;
        [super orderOut: nil];
        [self setAlphaValue: 1.0];
        if ((invoc = [anim userInfo])) {
            [invoc invoke];
        }
    }
}

- (VLCVoutView *)videoView
{
    NSArray *o_subViews = [[self contentView] subviews];
    if ([o_subViews count] > 0) {
        id o_vout_view = [o_subViews objectAtIndex:0];

        if ([o_vout_view class] == [VLCVoutView class])
            return (VLCVoutView *)o_vout_view;
    }

    return nil;
}

- (NSRect)constrainFrameRect:(NSRect)frameRect toScreen:(NSScreen *)screen
{
    if (!screen)
        screen = [self screen];
    NSRect screenRect = [screen frame];
    NSRect constrainedRect = [super constrainFrameRect:frameRect toScreen:screen];

    /*
     * Ugly workaround!
     * With Mavericks, there is a nasty bug resulting in grey bars on top in fullscreen mode.
     * It looks like this is enforced by the os because the window is in the way for the menu bar.
     *
     * According to the documentation, this constraining can be changed by overwriting this
     * method. But in this situation, even the received frameRect is already contrained with the
     * menu bars height substracted. This case is detected here, and the full height is
     * enforced again.
     *
     * See #9469 and radar://15583566
     */

    BOOL b_inFullscreen = [self fullscreen] || ([self respondsToSelector:@selector(inFullscreenTransition)] && [(VLCVideoWindowCommon *)self inFullscreenTransition]);

    if((OSX_MAVERICKS || OSX_YOSEMITE) && b_inFullscreen && constrainedRect.size.width == screenRect.size.width
          && constrainedRect.size.height != screenRect.size.height
          && abs(screenRect.size.height - constrainedRect.size.height) <= 25.) {

        msg_Dbg(VLCIntf, "Contrain window height %.1f to screen height %.1f",
                constrainedRect.size.height, screenRect.size.height);
        constrainedRect.size.height = screenRect.size.height;
    }

    return constrainedRect;
}

@end


/*****************************************************************************
 * VLCVideoWindowCommon
 *
 *  Common code for main window, detached window and extra video window
 *****************************************************************************/

@interface VLCVideoWindowCommon (Internal)
- (void)customZoom:(id)sender;
- (void)hasBecomeFullscreen;
- (void)hasEndedFullscreen;
@end

@implementation VLCVideoWindowCommon

@synthesize videoView=o_video_view;
@synthesize controlsBar=o_controls_bar;
@synthesize inFullscreenTransition=b_in_fullscreen_transition;
@synthesize windowShouldExitFullscreenWhenFinished=b_windowShouldExitFullscreenWhenFinished;

#pragma mark -
#pragma mark Init

- (id)initWithContentRect:(NSRect)contentRect styleMask:(NSUInteger)styleMask
                  backing:(NSBackingStoreType)backingType defer:(BOOL)flag
{
    b_dark_interface = config_GetInt(VLCIntf, "macosx-interfacestyle");

    if (b_dark_interface) {
        styleMask = NSBorderlessWindowMask;
#ifdef MAC_OS_X_VERSION_10_7
        if (!OSX_SNOW_LEOPARD)
            styleMask |= NSResizableWindowMask;
#endif
    }

    self = [super initWithContentRect:contentRect styleMask:styleMask
                              backing:backingType defer:flag];

    /* we want to be moveable regardless of our style */
    [self setMovableByWindowBackground: YES];
    [self setCanBecomeKeyWindow:YES];

    o_temp_view = [[NSView alloc] init];
    [o_temp_view setAutoresizingMask:NSViewHeightSizable | NSViewWidthSizable];

    return self;
}

- (void)dealloc
{
    [o_temp_view release];
    [super dealloc];
}

- (void)awakeFromNib
{
    BOOL b_nativeFullscreenMode = NO;
#ifdef MAC_OS_X_VERSION_10_7
    if (!OSX_SNOW_LEOPARD)
        b_nativeFullscreenMode = var_InheritBool(VLCIntf, "macosx-nativefullscreenmode");
#endif

    if (b_nativeFullscreenMode) {
        [self setCollectionBehavior: NSWindowCollectionBehaviorFullScreenPrimary];
    }


    [super awakeFromNib];
}

- (void)setTitle:(NSString *)title
{
    if (!title || [title length] < 1)
        return;

    if (b_dark_interface && o_titlebar_view)
        [o_titlebar_view setWindowTitle: title];

    [super setTitle: title];
}

#pragma mark -
#pragma mark zoom / minimize / close

- (BOOL)validateMenuItem:(NSMenuItem *)menuItem
{
    SEL s_menuAction = [menuItem action];

    if ((s_menuAction == @selector(performClose:)) || (s_menuAction == @selector(performMiniaturize:)) || (s_menuAction == @selector(performZoom:)))
        return YES;

    return [super validateMenuItem:menuItem];
}

- (BOOL)windowShouldClose:(id)sender
{
    return YES;
}

- (void)performClose:(id)sender
{
    if (!([self styleMask] & NSTitledWindowMask)) {
        [[NSNotificationCenter defaultCenter] postNotificationName:NSWindowWillCloseNotification object:self];

        [self close];
    } else
        [super performClose: sender];
}

- (void)performMiniaturize:(id)sender
{
    if (!([self styleMask] & NSTitledWindowMask))
        [self miniaturize: sender];
    else
        [super performMiniaturize: sender];
}

- (void)performZoom:(id)sender
{
    if (!([self styleMask] & NSTitledWindowMask))
        [self customZoom: sender];
    else
        [super performZoom: sender];
}

- (void)zoom:(id)sender
{
    if (!([self styleMask] & NSTitledWindowMask))
        [self customZoom: sender];
    else
        [super zoom: sender];
}

/**
 * Given a proposed frame rectangle, return a modified version
 * which will fit inside the screen.
 *
 * This method is based upon NSWindow.m, part of the GNUstep GUI Library, licensed under LGPLv2+.
 *    Authors:  Scott Christley <scottc@net-community.com>, Venkat Ajjanagadde <venkat@ocbi.com>,
 *              Felipe A. Rodriguez <far@ix.netcom.com>, Richard Frith-Macdonald <richard@brainstorm.co.uk>
 *    Copyright (C) 1996 Free Software Foundation, Inc.
 */
- (NSRect) customConstrainFrameRect: (NSRect)frameRect toScreen: (NSScreen*)screen
{
    NSRect screenRect = [screen visibleFrame];
    CGFloat difference;

    /* Move top edge of the window inside the screen */
    difference = NSMaxY (frameRect) - NSMaxY (screenRect);
    if (difference > 0) {
        frameRect.origin.y -= difference;
    }

    /* If the window is resizable, resize it (if needed) so that the
     bottom edge is on the screen or can be on the screen when the user moves
     the window */
    difference = NSMaxY (screenRect) - NSMaxY (frameRect);
    if (_styleMask & NSResizableWindowMask) {
        CGFloat difference2;

        difference2 = screenRect.origin.y - frameRect.origin.y;
        difference2 -= difference;
        // Take in account the space between the top of window and the top of the
        // screen which can be used to move the bottom of the window on the screen
        if (difference2 > 0) {
            frameRect.size.height -= difference2;
            frameRect.origin.y += difference2;
        }

        /* Ensure that resizing doesn't makewindow smaller than minimum */
        difference2 = [self minSize].height - frameRect.size.height;
        if (difference2 > 0) {
            frameRect.size.height += difference2;
            frameRect.origin.y -= difference2;
        }
    }

    return frameRect;
}

#define DIST 3

/**
 Zooms the receiver.   This method calls the delegate method
 windowShouldZoom:toFrame: to determine if the window should
 be allowed to zoom to full screen.
 *
 * This method is based upon NSWindow.m, part of the GNUstep GUI Library, licensed under LGPLv2+.
 *    Authors:  Scott Christley <scottc@net-community.com>, Venkat Ajjanagadde <venkat@ocbi.com>,
 *              Felipe A. Rodriguez <far@ix.netcom.com>, Richard Frith-Macdonald <richard@brainstorm.co.uk>
 *    Copyright (C) 1996 Free Software Foundation, Inc.
 */
- (void) customZoom: (id)sender
{
    NSRect maxRect = [[self screen] visibleFrame];
    NSRect currentFrame = [self frame];

    if ([[self delegate] respondsToSelector: @selector(windowWillUseStandardFrame:defaultFrame:)]) {
        maxRect = [[self delegate] windowWillUseStandardFrame: self defaultFrame: maxRect];
    }

    maxRect = [self customConstrainFrameRect: maxRect toScreen: [self screen]];

    // Compare the new frame with the current one
    if ((abs(NSMaxX(maxRect) - NSMaxX(currentFrame)) < DIST)
        && (abs(NSMaxY(maxRect) - NSMaxY(currentFrame)) < DIST)
        && (abs(NSMinX(maxRect) - NSMinX(currentFrame)) < DIST)
        && (abs(NSMinY(maxRect) - NSMinY(currentFrame)) < DIST)) {
        // Already in zoomed mode, reset user frame, if stored
        if ([self frameAutosaveName] != nil) {
            [self setFrame: previousSavedFrame display: YES animate: YES];
            [self saveFrameUsingName: [self frameAutosaveName]];
        }
        return;
    }

    if ([self frameAutosaveName] != nil) {
        [self saveFrameUsingName: [self frameAutosaveName]];
        previousSavedFrame = [self frame];
    }

    [self setFrame: maxRect display: YES animate: YES];
}

#pragma mark -
#pragma mark Video window resizing logic

- (void)setWindowLevel:(NSInteger)i_state
{
    if (var_InheritBool(VLCIntf, "video-wallpaper") || [self level] < NSNormalWindowLevel)
        return;

    if (!b_fullscreen && !b_in_fullscreen_transition)
        [self setLevel: i_state];

    // save it for restore if window is currently minimized or in fullscreen
    i_originalLevel = i_state;
}

- (NSRect)getWindowRectForProposedVideoViewSize:(NSSize)size
{
    NSSize windowMinSize = [self minSize];
    NSRect screenFrame = [[self screen] visibleFrame];

    NSPoint topleftbase = NSMakePoint(0, [self frame].size.height);
    NSPoint topleftscreen = [self convertBaseToScreen: topleftbase];

    CGFloat f_width = size.width;
    CGFloat f_height = size.height;
    if (f_width < windowMinSize.width)
        f_width = windowMinSize.width;
    if (f_height < f_min_video_height)
        f_height = f_min_video_height;

    /* Calculate the window's new size */
    NSRect new_frame;
    new_frame.size.width = [self frame].size.width - [o_video_view frame].size.width + f_width;
    new_frame.size.height = [self frame].size.height - [o_video_view frame].size.height + f_height;
    new_frame.origin.x = topleftscreen.x;
    new_frame.origin.y = topleftscreen.y - new_frame.size.height;

    /* make sure the window doesn't exceed the screen size the window is on */
    if (new_frame.size.width > screenFrame.size.width) {
        new_frame.size.width = screenFrame.size.width;
        new_frame.origin.x = screenFrame.origin.x;
    }
    if (new_frame.size.height > screenFrame.size.height) {
        new_frame.size.height = screenFrame.size.height;
        new_frame.origin.y = screenFrame.origin.y;
    }
    if (new_frame.origin.y < screenFrame.origin.y)
        new_frame.origin.y = screenFrame.origin.y;

    CGFloat right_screen_point = screenFrame.origin.x + screenFrame.size.width;
    CGFloat right_window_point = new_frame.origin.x + new_frame.size.width;
    if (right_window_point > right_screen_point)
        new_frame.origin.x -= (right_window_point - right_screen_point);

    return new_frame;
}

- (void)resizeWindow
{
    // VOUT_WINDOW_SET_SIZE is triggered when exiting fullscreen. This event is ignored here
    // to avoid interference with the animation.
    if ([self fullscreen] || b_in_fullscreen_transition)
        return;

    NSRect window_rect = [self getWindowRectForProposedVideoViewSize:nativeVideoSize];
    [[self animator] setFrame:window_rect display:YES];
}

- (void)setNativeVideoSize:(NSSize)size
{
    nativeVideoSize = size;

    if (var_InheritBool(VLCIntf, "macosx-video-autoresize") && !var_InheritBool(VLCIntf, "video-wallpaper"))
        [self resizeWindow];
}

- (NSSize)windowWillResize:(NSWindow *)window toSize:(NSSize)proposedFrameSize
{
    if (![[VLCMain sharedInstance] activeVideoPlayback] || nativeVideoSize.width == 0. || nativeVideoSize.height == 0. || window != self)
        return proposedFrameSize;

    // needed when entering lion fullscreen mode
    if (b_in_fullscreen_transition || [self fullscreen])
        return proposedFrameSize;

    if ([o_video_view isHidden])
        return proposedFrameSize;

    if ([[VLCCoreInteraction sharedInstance] aspectRatioIsLocked]) {
        NSRect videoWindowFrame = [self frame];
        NSRect viewRect = [o_video_view convertRect:[o_video_view bounds] toView: nil];
        NSRect contentRect = [self contentRectForFrameRect:videoWindowFrame];
        CGFloat marginy = viewRect.origin.y + videoWindowFrame.size.height - contentRect.size.height;
        CGFloat marginx = contentRect.size.width - viewRect.size.width;
        if (o_titlebar_view && b_dark_interface)
            marginy += [o_titlebar_view frame].size.height;

        proposedFrameSize.height = (proposedFrameSize.width - marginx) * nativeVideoSize.height / nativeVideoSize.width + marginy;
    }

    return proposedFrameSize;
}

- (void)windowWillMiniaturize:(NSNotification *)notification
{
    // Set level to normal as a workaround for Mavericks bug causing window
    // to vanish from screen, see radar://15473716
    i_originalLevel = [self level];
    [self setLevel: NSNormalWindowLevel];
}

- (void)windowDidDeminiaturize:(NSNotification *)notification
{
    [self setLevel: i_originalLevel];
}

#pragma mark -
#pragma mark Mouse cursor handling

//  NSTimer selectors require this function signature as per Apple's docs
- (void)hideMouseCursor:(NSTimer *)timer
{
    [NSCursor setHiddenUntilMouseMoves: YES];
}

- (void)recreateHideMouseTimer
{
    if (t_hide_mouse_timer != nil) {
        [t_hide_mouse_timer invalidate];
        [t_hide_mouse_timer release];
    }

    t_hide_mouse_timer = [NSTimer scheduledTimerWithTimeInterval:2
                                                          target:self
                                                        selector:@selector(hideMouseCursor:)
                                                        userInfo:nil
                                                         repeats:NO];
    [t_hide_mouse_timer retain];
}

//  Called automatically if window's acceptsMouseMovedEvents property is true
- (void)mouseMoved:(NSEvent *)theEvent
{
    if (b_fullscreen)
        [self recreateHideMouseTimer];

    [super mouseMoved: theEvent];
}

#pragma mark -
#pragma mark Key events

- (void)flagsChanged:(NSEvent *)theEvent
{
    BOOL b_alt_pressed = ([theEvent modifierFlags] & NSAlternateKeyMask) != 0;
    [o_titlebar_view informModifierPressed: b_alt_pressed];

    [super flagsChanged:theEvent];
}

#pragma mark -
#pragma mark Lion native fullscreen handling

- (void)becomeKeyWindow
{
    [super becomeKeyWindow];

    // change fspanel state for the case when multiple windows are in fullscreen
    if ([self hasActiveVideo] && [self fullscreen])
        [[[VLCMainWindow sharedInstance] fsPanel] setActive:nil];
    else
        [[[VLCMainWindow sharedInstance] fsPanel] setNonActive:nil];
}

- (void)resignKeyWindow
{
    [super resignKeyWindow];

    [[[VLCMainWindow sharedInstance] fsPanel] setNonActive:nil];
}

-(NSArray*)customWindowsToEnterFullScreenForWindow:(NSWindow *)window
{
    if (window == self) {
        return [NSArray arrayWithObject:window];
    }

    return nil;
}

- (NSArray*)customWindowsToExitFullScreenForWindow:(NSWindow*)window
{
    if (window == self) {
        return [NSArray arrayWithObject:window];
    }

    return nil;
}

- (void)window:window startCustomAnimationToEnterFullScreenWithDuration:(NSTimeInterval)duration
{
    [window setStyleMask:([window styleMask] | NSFullScreenWindowMask)];

    NSScreen *screen = [window screen];
    NSRect screenFrame = [screen frame];

    [NSAnimationContext runAnimationGroup:^(NSAnimationContext *context) {
        [context setDuration:0.5 * duration];
        [[window animator] setFrame:screenFrame display:YES];
    } completionHandler:nil];
}

- (void)window:window startCustomAnimationToExitFullScreenWithDuration:(NSTimeInterval)duration
{
    [window setStyleMask:([window styleMask] & ~NSFullScreenWindowMask)];
    [[window animator] setFrame:frameBeforeLionFullscreen display:YES animate:YES];

    [NSAnimationContext runAnimationGroup:^(NSAnimationContext *context) {
        [context setDuration:0.5 * duration];
        [[window animator] setFrame:frameBeforeLionFullscreen display:YES animate:YES];
    } completionHandler:nil];
}

- (void)windowWillEnterFullScreen:(NSNotification *)notification
{
    b_windowShouldExitFullscreenWhenFinished = [[VLCMain sharedInstance] activeVideoPlayback];

    NSInteger i_currLevel = [self level];
    // b_fullscreen and b_in_fullscreen_transition must not be true yet
    [[[VLCMain sharedInstance] voutController] updateWindowLevelForHelperWindows: NSNormalWindowLevel];
    [self setLevel:NSNormalWindowLevel];
    i_originalLevel = i_currLevel;

    b_in_fullscreen_transition = YES;

    var_SetBool(pl_Get(VLCIntf), "fullscreen", true);

    frameBeforeLionFullscreen = [self frame];

    if ([self hasActiveVideo]) {
        vout_thread_t *p_vout = getVoutForActiveWindow();
        if (p_vout) {
            var_SetBool(p_vout, "fullscreen", true);
            vlc_object_release(p_vout);
        }
    }

    if ([self hasActiveVideo])
        [[VLCMainWindow sharedInstance] recreateHideMouseTimer];

    if (b_dark_interface) {
        [o_titlebar_view removeFromSuperviewWithoutNeedingDisplay];

        NSRect winrect;
        CGFloat f_titleBarHeight = [o_titlebar_view frame].size.height;
        winrect = [self frame];

        winrect.size.height = winrect.size.height - f_titleBarHeight;
        [self setFrame: winrect display:NO animate:NO];
    }

    [o_video_view setFrame: [[self contentView] frame]];
    if (![o_video_view isHidden]) {
        [[o_controls_bar bottomBarView] setHidden: YES];
    }

    [self setMovableByWindowBackground: NO];
}

- (void)windowDidEnterFullScreen:(NSNotification *)notification
{
    // Indeed, we somehow can have an "inactive" fullscreen (but a visible window!).
    // But this creates some problems when leaving fs over remote intfs, so activate app here.
    [NSApp activateIgnoringOtherApps:YES];

    [self setFullscreen: YES];
    b_in_fullscreen_transition = NO;

    if ([self hasActiveVideo]) {
        [[[VLCMainWindow sharedInstance] fsPanel] setVoutWasUpdated: self];
        if (![o_video_view isHidden])
            [[[VLCMainWindow sharedInstance] fsPanel] setActive: nil];
    }

    NSArray *subviews = [[self videoView] subviews];
    NSUInteger count = [subviews count];

    for (NSUInteger x = 0; x < count; x++) {
        if ([[subviews objectAtIndex:x] respondsToSelector:@selector(reshape)])
            [[subviews objectAtIndex:x] reshape];
    }
}

- (void)windowWillExitFullScreen:(NSNotification *)notification
{
    b_in_fullscreen_transition = YES;
    [self setFullscreen: NO];

    if ([self hasActiveVideo]) {
        var_SetBool(pl_Get(VLCIntf), "fullscreen", false);

        vout_thread_t *p_vout = getVoutForActiveWindow();
        if (p_vout) {
            var_SetBool(p_vout, "fullscreen", false);
            vlc_object_release(p_vout);
        }
    }

    [NSCursor setHiddenUntilMouseMoves: NO];
    [[[VLCMainWindow sharedInstance] fsPanel] setNonActive: nil];


    if (b_dark_interface) {
        NSRect winrect;
        CGFloat f_titleBarHeight = [o_titlebar_view frame].size.height;

        winrect = [o_video_view frame];
        winrect.size.height -= f_titleBarHeight;
        [o_video_view setFrame: winrect];

        winrect = [self frame];
        [o_titlebar_view setFrame: NSMakeRect(0, winrect.size.height - f_titleBarHeight,
                                              winrect.size.width, f_titleBarHeight)];
        [[self contentView] addSubview: o_titlebar_view];

        winrect.size.height = winrect.size.height + f_titleBarHeight;
        [self setFrame: winrect display:NO animate:NO];
    }

    NSRect videoViewFrame = [o_video_view frame];
    videoViewFrame.origin.y += [o_controls_bar height];
    videoViewFrame.size.height -= [o_controls_bar height];
    [o_video_view setFrame: videoViewFrame];

    if (![o_video_view isHidden]) {
        [[o_controls_bar bottomBarView] setHidden: NO];
    }

    [self setMovableByWindowBackground: YES];
}

- (void)windowDidExitFullScreen:(NSNotification *)notification
{
    b_in_fullscreen_transition = NO;

    [[[VLCMain sharedInstance] voutController] updateWindowLevelForHelperWindows: i_originalLevel];
    [self setLevel:i_originalLevel];
}

#pragma mark -
#pragma mark Fullscreen Logic

- (void)enterFullscreenWithAnimation:(BOOL)b_animation
{
    NSMutableDictionary *dict1, *dict2;
    NSScreen *screen;
    NSRect screen_rect;
    NSRect rect;
    BOOL blackout_other_displays = var_InheritBool(VLCIntf, "macosx-black");

    screen = [NSScreen screenWithDisplayID:(CGDirectDisplayID)var_InheritInteger(VLCIntf, "macosx-vdev")];

    if (!screen) {
        msg_Dbg(VLCIntf, "chosen screen isn't present, using current screen for fullscreen mode");
        screen = [self screen];
    }
    if (!screen) {
        msg_Dbg(VLCIntf, "Using deepest screen");
        screen = [NSScreen deepestScreen];
    }

    screen_rect = [screen frame];

    if (o_controls_bar)
        [o_controls_bar setFullscreenState:YES];
    [[[VLCMainWindow sharedInstance] controlsBar] setFullscreenState:YES];

    [[VLCMainWindow sharedInstance] recreateHideMouseTimer];

    if (blackout_other_displays)
        [screen blackoutOtherScreens];

    /* Make sure we don't see the window flashes in float-on-top mode */
    NSInteger i_currLevel = [self level];
    // b_fullscreen must not be true yet
    [[[VLCMain sharedInstance] voutController] updateWindowLevelForHelperWindows: NSNormalWindowLevel];
    [self setLevel:NSNormalWindowLevel];
    i_originalLevel = i_currLevel; // would be overwritten by previous call

    /* Only create the o_fullscreen_window if we are not in the middle of the zooming animation */
    if (!o_fullscreen_window) {
        /* We can't change the styleMask of an already created NSWindow, so we create another window, and do eye catching stuff */

        rect = [[o_video_view superview] convertRect: [o_video_view frame] toView: nil]; /* Convert to Window base coord */
        rect.origin.x += [self frame].origin.x;
        rect.origin.y += [self frame].origin.y;
        o_fullscreen_window = [[VLCWindow alloc] initWithContentRect:rect styleMask: NSBorderlessWindowMask backing:NSBackingStoreBuffered defer:YES];
        [o_fullscreen_window setBackgroundColor: [NSColor blackColor]];
        [o_fullscreen_window setCanBecomeKeyWindow: YES];
        [o_fullscreen_window setCanBecomeMainWindow: YES];
        [o_fullscreen_window setHasActiveVideo: YES];
        [o_fullscreen_window setFullscreen: YES];

        /* Make sure video view gets visible in case the playlist was visible before */
        b_video_view_was_hidden = [o_video_view isHidden];
        [o_video_view setHidden: NO];

        if (!b_animation) {
            /* We don't animate if we are not visible, instead we
             * simply fade the display */
            CGDisplayFadeReservationToken token;

            if (blackout_other_displays) {
                CGAcquireDisplayFadeReservation(kCGMaxDisplayReservationInterval, &token);
                CGDisplayFade(token, 0.5, kCGDisplayBlendNormal, kCGDisplayBlendSolidColor, 0, 0, 0, YES);
            }

            NSDisableScreenUpdates();
            [o_video_view retain];
            [[o_video_view superview] replaceSubview:o_video_view with:o_temp_view];
            [o_temp_view setFrame:[o_video_view frame]];
            [[o_fullscreen_window contentView] addSubview:o_video_view];
            [o_video_view setFrame: [[o_fullscreen_window contentView] frame]];
            [o_video_view release];
            NSEnableScreenUpdates();

            [screen setFullscreenPresentationOptions];

            [o_fullscreen_window setFrame:screen_rect display:YES animate:NO];

            [o_fullscreen_window orderFront:self animate:YES];

            [o_fullscreen_window setLevel:NSNormalWindowLevel];

            if (blackout_other_displays) {
                CGDisplayFade(token, 0.3, kCGDisplayBlendSolidColor, kCGDisplayBlendNormal, 0, 0, 0, NO);
                CGReleaseDisplayFadeReservation(token);
            }

            /* Will release the lock */
            [self hasBecomeFullscreen];

            return;
        }

        /* Make sure we don't see the o_video_view disappearing of the screen during this operation */
        NSDisableScreenUpdates();
        [o_video_view retain];
        [[o_video_view superview] replaceSubview:o_video_view with:o_temp_view];
        [o_temp_view setFrame:[o_video_view frame]];
        [[o_fullscreen_window contentView] addSubview:o_video_view];
        [o_video_view setFrame: [[o_fullscreen_window contentView] frame]];
        [o_video_view release];
        [o_fullscreen_window makeKeyAndOrderFront:self];
        NSEnableScreenUpdates();
    }

    /* We are in fullscreen (and no animation is running) */
    if ([self fullscreen]) {
        /* Make sure we are hidden */
        [self orderOut: self];

        return;
    }

    if (o_fullscreen_anim1) {
        [o_fullscreen_anim1 stopAnimation];
        [o_fullscreen_anim1 release];
    }
    if (o_fullscreen_anim2) {
        [o_fullscreen_anim2 stopAnimation];
        [o_fullscreen_anim2 release];
    }

    [screen setFullscreenPresentationOptions];

    dict1 = [[NSMutableDictionary alloc] initWithCapacity:2];
    dict2 = [[NSMutableDictionary alloc] initWithCapacity:3];

    [dict1 setObject:self forKey:NSViewAnimationTargetKey];
    [dict1 setObject:NSViewAnimationFadeOutEffect forKey:NSViewAnimationEffectKey];

    [dict2 setObject:o_fullscreen_window forKey:NSViewAnimationTargetKey];
    [dict2 setObject:[NSValue valueWithRect:[o_fullscreen_window frame]] forKey:NSViewAnimationStartFrameKey];
    [dict2 setObject:[NSValue valueWithRect:screen_rect] forKey:NSViewAnimationEndFrameKey];

    /* Strategy with NSAnimation allocation:
     - Keep at most 2 animation at a time
     - leaveFullscreen/enterFullscreen are the only responsible for releasing and alloc-ing
     */
    o_fullscreen_anim1 = [[NSViewAnimation alloc] initWithViewAnimations:[NSArray arrayWithObject:dict1]];
    o_fullscreen_anim2 = [[NSViewAnimation alloc] initWithViewAnimations:[NSArray arrayWithObject:dict2]];

    [dict1 release];
    [dict2 release];

    [o_fullscreen_anim1 setAnimationBlockingMode: NSAnimationNonblocking];
    [o_fullscreen_anim1 setDuration: 0.3];
    [o_fullscreen_anim1 setFrameRate: 30];
    [o_fullscreen_anim2 setAnimationBlockingMode: NSAnimationNonblocking];
    [o_fullscreen_anim2 setDuration: 0.2];
    [o_fullscreen_anim2 setFrameRate: 30];

    [o_fullscreen_anim2 setDelegate: self];
    [o_fullscreen_anim2 startWhenAnimation: o_fullscreen_anim1 reachesProgress: 1.0];

    [o_fullscreen_anim1 startAnimation];
    /* fullscreenAnimation will be unlocked when animation ends */

    b_in_fullscreen_transition = YES;
}

- (void)hasBecomeFullscreen
{
    if ([[o_video_view subviews] count] > 0)
        [o_fullscreen_window makeFirstResponder: [[o_video_view subviews] objectAtIndex:0]];

    [o_fullscreen_window makeKeyWindow];
    [o_fullscreen_window setAcceptsMouseMovedEvents: YES];

    /* tell the fspanel to move itself to front next time it's triggered */
    [[[VLCMainWindow sharedInstance] fsPanel] setVoutWasUpdated: o_fullscreen_window];
    [[[VLCMainWindow sharedInstance] fsPanel] setActive: nil];

    if ([self isVisible])
        [self orderOut: self];

    b_in_fullscreen_transition = NO;
    [self setFullscreen:YES];
}

- (void)leaveFullscreenWithAnimation:(BOOL)b_animation
{
    NSMutableDictionary *dict1, *dict2;
    NSRect frame;
    BOOL blackout_other_displays = var_InheritBool(VLCIntf, "macosx-black");

    if (o_controls_bar)
        [o_controls_bar setFullscreenState:NO];
    [[[VLCMainWindow sharedInstance] controlsBar] setFullscreenState:NO];

    /* We always try to do so */
    [NSScreen unblackoutScreens];

    [[o_video_view window] makeKeyAndOrderFront: nil];

    /* Don't do anything if o_fullscreen_window is already closed */
    if (!o_fullscreen_window) {
        return;
    }

    [[[VLCMainWindow sharedInstance] fsPanel] setNonActive: nil];
    [[o_fullscreen_window screen] setNonFullscreenPresentationOptions];

    if (o_fullscreen_anim1) {
        [o_fullscreen_anim1 stopAnimation];
        [o_fullscreen_anim1 release];
        o_fullscreen_anim1 = nil;
    }
    if (o_fullscreen_anim2) {
        [o_fullscreen_anim2 stopAnimation];
        [o_fullscreen_anim2 release];
        o_fullscreen_anim2 = nil;
    }

    b_in_fullscreen_transition = YES;
    [self setFullscreen:NO];

    if (!b_animation) {
        /* We don't animate if we are not visible, instead we
         * simply fade the display */
        CGDisplayFadeReservationToken token;

        if (blackout_other_displays) {
            CGAcquireDisplayFadeReservation(kCGMaxDisplayReservationInterval, &token);
            CGDisplayFade(token, 0.3, kCGDisplayBlendNormal, kCGDisplayBlendSolidColor, 0, 0, 0, YES);
        }

        [self setAlphaValue:1.0];
        [self orderFront: self];

        /* Will release the lock */
        [self hasEndedFullscreen];

        if (blackout_other_displays) {
            CGDisplayFade(token, 0.5, kCGDisplayBlendSolidColor, kCGDisplayBlendNormal, 0, 0, 0, NO);
            CGReleaseDisplayFadeReservation(token);
        }

        return;
    }

    [self setAlphaValue: 0.0];
    [self orderFront: self];
    [[o_video_view window] orderFront: self];

    frame = [[o_temp_view superview] convertRect: [o_temp_view frame] toView: nil]; /* Convert to Window base coord */
    frame.origin.x += [self frame].origin.x;
    frame.origin.y += [self frame].origin.y;

    dict2 = [[NSMutableDictionary alloc] initWithCapacity:2];
    [dict2 setObject:self forKey:NSViewAnimationTargetKey];
    [dict2 setObject:NSViewAnimationFadeInEffect forKey:NSViewAnimationEffectKey];

    o_fullscreen_anim2 = [[NSViewAnimation alloc] initWithViewAnimations:[NSArray arrayWithObject:dict2]];
    [dict2 release];

    [o_fullscreen_anim2 setAnimationBlockingMode: NSAnimationNonblocking];
    [o_fullscreen_anim2 setDuration: 0.3];
    [o_fullscreen_anim2 setFrameRate: 30];

    [o_fullscreen_anim2 setDelegate: self];

    dict1 = [[NSMutableDictionary alloc] initWithCapacity:3];

    [dict1 setObject:o_fullscreen_window forKey:NSViewAnimationTargetKey];
    [dict1 setObject:[NSValue valueWithRect:[o_fullscreen_window frame]] forKey:NSViewAnimationStartFrameKey];
    [dict1 setObject:[NSValue valueWithRect:frame] forKey:NSViewAnimationEndFrameKey];

    o_fullscreen_anim1 = [[NSViewAnimation alloc] initWithViewAnimations:[NSArray arrayWithObject:dict1]];
    [dict1 release];

    [o_fullscreen_anim1 setAnimationBlockingMode: NSAnimationNonblocking];
    [o_fullscreen_anim1 setDuration: 0.2];
    [o_fullscreen_anim1 setFrameRate: 30];
    [o_fullscreen_anim2 startWhenAnimation: o_fullscreen_anim1 reachesProgress: 1.0];

    /* Make sure o_fullscreen_window is the frontmost window */
    [o_fullscreen_window orderFront: self];

    [o_fullscreen_anim1 startAnimation];
    /* fullscreenAnimation will be unlocked when animation ends */
}

- (void)hasEndedFullscreen
{
    b_in_fullscreen_transition = NO;

    /* This function is private and should be only triggered at the end of the fullscreen change animation */
    /* Make sure we don't see the o_video_view disappearing of the screen during this operation */
    NSDisableScreenUpdates();
    [o_video_view retain];
    [o_video_view removeFromSuperviewWithoutNeedingDisplay];
    [[o_temp_view superview] replaceSubview:o_temp_view with:o_video_view];
    [o_video_view release];
    [o_video_view setFrame:[o_temp_view frame]];
    if ([[o_video_view subviews] count] > 0)
        [self makeFirstResponder: [[o_video_view subviews] objectAtIndex:0]];

    [o_video_view setHidden: b_video_view_was_hidden];

    [self makeKeyAndOrderFront:self];

    [o_fullscreen_window orderOut: self];
    NSEnableScreenUpdates();

    [o_fullscreen_window release];
    o_fullscreen_window = nil;

    [[[VLCMain sharedInstance] voutController] updateWindowLevelForHelperWindows: i_originalLevel];
    [self setLevel:i_originalLevel];

    [self setAlphaValue: config_GetFloat(VLCIntf, "macosx-opaqueness")];
}

- (void)animationDidEnd:(NSAnimation*)animation
{
    NSArray *viewAnimations;
    if ([animation currentValue] < 1.0)
        return;

    /* Fullscreen ended or started (we are a delegate only for leaveFullscreen's/enterFullscren's anim2) */
    viewAnimations = [o_fullscreen_anim2 viewAnimations];
    if ([viewAnimations count] >=1 &&
        [[[viewAnimations objectAtIndex:0] objectForKey: NSViewAnimationEffectKey] isEqualToString:NSViewAnimationFadeInEffect]) {
        /* Fullscreen ended */
        [self hasEndedFullscreen];
    } else
    /* Fullscreen started */
        [self hasBecomeFullscreen];
}

#pragma mark -
#pragma mark Accessibility stuff

- (NSArray *)accessibilityAttributeNames
{
    if (!b_dark_interface || !o_titlebar_view)
        return [super accessibilityAttributeNames];

    static NSMutableArray *attributes = nil;
    if (attributes == nil) {
        attributes = [[super accessibilityAttributeNames] mutableCopy];
        NSArray *appendAttributes = [NSArray arrayWithObjects:NSAccessibilitySubroleAttribute,
                                     NSAccessibilityCloseButtonAttribute,
                                     NSAccessibilityMinimizeButtonAttribute,
                                     NSAccessibilityZoomButtonAttribute, nil];

        for(NSString *attribute in appendAttributes) {
            if (![attributes containsObject:attribute])
                [attributes addObject:attribute];
        }
    }
    return attributes;
}

- (id)accessibilityAttributeValue: (NSString*)o_attribute_name
{
    if (b_dark_interface && o_titlebar_view) {
        VLCMainWindowTitleView *o_tbv = o_titlebar_view;

        if ([o_attribute_name isEqualTo: NSAccessibilitySubroleAttribute])
            return NSAccessibilityStandardWindowSubrole;

        if ([o_attribute_name isEqualTo: NSAccessibilityCloseButtonAttribute])
            return [[o_tbv closeButton] cell];

        if ([o_attribute_name isEqualTo: NSAccessibilityMinimizeButtonAttribute])
            return [[o_tbv minimizeButton] cell];

        if ([o_attribute_name isEqualTo: NSAccessibilityZoomButtonAttribute])
            return [[o_tbv zoomButton] cell];
    }

    return [super accessibilityAttributeValue: o_attribute_name];
}

@end
