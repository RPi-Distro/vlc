/*****************************************************************************
 * embeddedwindow.h: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2002-2005 the VideoLAN team
 * $Id: f1b5d2d8d6396dcb0d9b93fbb0e90315edb50a3d $
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
 * VLCEmbeddedWindow interface
 *****************************************************************************/

#import "misc.h"

@interface VLCEmbeddedWindow : NSWindow
{
    IBOutlet id o_btn_backward;
    IBOutlet id o_btn_forward;
    IBOutlet id o_btn_fullscreen;
    IBOutlet id o_btn_play;
    IBOutlet id o_slider;
    IBOutlet id o_time;
    IBOutlet id o_view;

    NSImage * o_img_play;
    NSImage * o_img_play_pressed;
    NSImage * o_img_pause;
    NSImage * o_img_pause_pressed;
    
    NSRect o_saved_frame;

    VLCWindow       * o_fullscreen_window;
    NSView          * o_temp_view;
    /* set to yes if we are fullscreen and all animations are over */
    BOOL              b_fullscreen;
    NSRecursiveLock * o_animation_lock;
}

- (void)setTime:(NSString *)o_arg_ime position:(float)f_position;
- (void)playStatusUpdated:(int)i_status;
- (void)setSeekable:(BOOL)b_seekable;
- (void)setFullscreen:(BOOL)b_fullscreen_state;

- (NSView *)mainView;

- (BOOL)isFullscreen;

- (void)lockFullscreenAnimation;
- (void)unlockFullscreenAnimation;

- (void)enterFullscreen;
- (void)leaveFullscreen;
/* Allows to leave fullscreen by simply fading out the display */
- (void)leaveFullscreenAndFadeOut: (BOOL)fadeout;

/* private */
- (void)hasEndedFullscreen;
- (void)hasBecomeFullscreen;

- (void)setFrameOnMainThread:(NSData*)packedargs;
@end

