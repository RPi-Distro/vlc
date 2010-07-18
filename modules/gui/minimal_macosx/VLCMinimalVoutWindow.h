/*****************************************************************************
 * VLCMinimalVoutWindow.h: MacOS X Minimal window for a vout
 *****************************************************************************
 * Copyright (C) 2007 the VideoLAN team
 * $Id: 7e2fd2803937e11887830d6116a7e21d1a531f82 $
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

#import <Cocoa/Cocoa.h>

#include <vlc_common.h>

@interface VLCMinimalVoutWindow : NSWindow
{
    NSRect initialFrame;
    NSPoint initialLocation, initialLocationOnScreen;
    BOOL fullscreen;
    BOOL mouseDraggedShouldResize;
}

- (id)initWithContentRect:(NSRect)contentRect;

/* @protocol VLCOpenGLVoutEmbedding */
- (void)addVoutSubview:(NSView *)view;
- (void)removeVoutSubview:(NSView *)view;
- (void)enterFullscreen;
- (void)leaveFullscreen;
- (BOOL)stretchesVideo;
- (void)setOnTop: (BOOL)ontop;
@end
