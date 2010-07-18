/*****************************************************************************
 * misc.h: code not specific to vlc
 *****************************************************************************
 * Copyright (C) 2003 the VideoLAN team
 * $Id: 38fb496b4d7daf183e1d2caf06eecb9c0703d309 $
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
}

- (void)setCanBecomeKeyWindow: (BOOL)canBecomeKey;
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
 * ITSliderCell
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

@end
