/*****************************************************************************
 * sidestatusview.h: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2005-2008 the VideoLAN team
 * $Id: eb330e25b9332362693967f66fb611b34d1c621b $
 *
 * Authors: Eric Dudiak <dudiak at gmail dot com>
 *          Colloquy <http://colloquy.info/>
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

@interface sidestatusview : NSView {
    IBOutlet NSSplitView *splitView;
	float _clickOffset;
	BOOL _insideResizeArea;
}

@end
