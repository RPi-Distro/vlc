/*****************************************************************************
 * misc.h: code not specific to vlc
 *****************************************************************************
 * Copyright (C) 2003-2007 the VideoLAN team
 * $Id$
 *
 * Authors: Jon Lech Johansen <jon-vl@nanocrew.net>
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

#import <Cocoa/Cocoa.h>
#import <ApplicationServices/ApplicationServices.h>

/*****************************************************************************
 * NSImage (VLCAddition)
 *****************************************************************************/

@interface NSImage (VLCAdditions)
+ (id)imageWithWarningIcon;
+ (id)imageWithErrorIcon;
@end

/*****************************************************************************
 * NSAnimation (VLCAddition)
 *****************************************************************************/

@interface NSAnimation (VLCAdditions)
- (void)setUserInfo: (void *)userInfo;
- (void *)userInfo;
@end

/*****************************************************************************
 * NSScreen (VLCAdditions)
 *
 *  Missing extension to NSScreen
 *****************************************************************************/

@interface NSScreen (VLCAdditions)

+ (NSScreen *)screenWithDisplayID: (CGDirectDisplayID)displayID;
- (BOOL)isMainScreen;
- (BOOL)isScreen: (NSScreen*)screen;
- (CGDirectDisplayID)displayID;
- (void)blackoutOtherScreens;
+ (void)unblackoutScreens;
@end

/*****************************************************************************
 * VLCWindow
 *
 *  Missing extension to NSWindow
 *****************************************************************************/

@interface VLCWindow : NSWindow
{
    BOOL b_canBecomeKeyWindow;
    BOOL b_isset_canBecomeKeyWindow;
    NSViewAnimation *animation;
}

- (void)setCanBecomeKeyWindow: (BOOL)canBecomeKey;

/* animate mode is only supported in >=10.4 */
- (void)orderFront: (id)sender animate: (BOOL)animate;

/* animate mode is only supported in >=10.4 */
- (void)orderOut: (id)sender animate: (BOOL)animate;

/* animate mode is only supported in >=10.4 */
- (void)orderOut: (id)sender animate: (BOOL)animate callback:(NSInvocation *)callback;

/* animate mode is only supported in >=10.4 */
- (void)closeAndAnimate: (BOOL)animate;
@end


/*****************************************************************************
 * VLCControllerWindow
 *****************************************************************************/


@interface VLCControllerWindow : NSWindow
{
}

@end

/*****************************************************************************
 * VLCControllerView
 *****************************************************************************/

@interface VLCControllerView : NSView
{
}

@end

/*****************************************************************************
 * VLBrushedMetalImageView
 *****************************************************************************/

@interface VLBrushedMetalImageView : NSImageView
{

}

@end


/*****************************************************************************
 * MPSlider
 *****************************************************************************/

@interface MPSlider : NSSlider
{
}

@end

/*****************************************************************************
 * ITSlider
 *****************************************************************************/

@interface ITSlider : NSSlider
{
}

@end

/*****************************************************************************
 * ITSliderCell
 *****************************************************************************/

@interface ITSliderCell : NSSliderCell
{
    NSImage *_knobOff;
    NSImage *_knobOn;
    BOOL b_mouse_down;
}
- (void)controlTintChanged;

@end
