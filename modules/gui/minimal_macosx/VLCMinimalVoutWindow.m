/*****************************************************************************
 * VLCMinimalVoutWindow.m: MacOS X Minimal interface window
 *****************************************************************************
 * Copyright (C) 2007 the VideoLAN team
 * $Id: f3357e0adc43d90c602cd45d74481a054a9a2f8c $
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include "intf.h"
#include "voutgl.h"
#include "VLCOpenGLVoutView.h"
#include "VLCMinimalVoutWindow.h"

#import <Cocoa/Cocoa.h>

/* SetSystemUIMode, ... */
#import <Carbon/Carbon.h>

@implementation VLCMinimalVoutWindow
- (id)initWithContentRect:(NSRect)contentRect
{
    if( self = [super initWithContentRect:contentRect styleMask:NSBorderlessWindowMask backing:NSBackingStoreBuffered defer:NO])
    {
        initialFrame = contentRect;
        fullscreen = NO;
        [self setBackgroundColor:[NSColor blackColor]];
        [self setHasShadow:YES];
        [self setMovableByWindowBackground: YES];
        [self center];
    }
    return self;
}

/* @protocol VLCOpenGLVoutEmbedding */
- (void)addVoutSubview:(NSView *)view
{
    [view setAutoresizingMask:NSViewHeightSizable|NSViewWidthSizable];
    [[self contentView] addSubview:view];
    [view setFrame:[[self contentView] bounds]];
}

- (void)removeVoutSubview:(NSView *)view
{
    [self close];
    [self release];
}

- (void)enterFullscreen
{
    fullscreen = YES;
    initialFrame = [self frame];
    SetSystemUIMode( kUIModeAllHidden, kUIOptionAutoShowMenuBar);
    [self setFrame:[[self screen] frame] display:YES animate:YES];
}

- (void)leaveFullscreen
{
    fullscreen = NO;
    SetSystemUIMode( kUIModeNormal, kUIOptionAutoShowMenuBar);
    [self setFrame:initialFrame display:YES animate:YES];
}

- (BOOL)stretchesVideo
{
    return NO;
}

- (void)setOnTop: (BOOL)ontop
{

}
@end

