/*****************************************************************************
 * CompatibilityFixes.h: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2011-2017 VLC authors and VideoLAN
 * $Id: 069aefd8d702a0a2df74852f54ee0c03dfd6a2cd $
 *
 * Authors: Felix Paul Kühne <fkuehne -at- videolan -dot- org>
 *          Marvin Scholz <epirat07 -at- gmail -dot- com>
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

#pragma mark -
#pragma OS detection code
#define OSX_LION_AND_HIGHER (NSAppKitVersionNumber >= 1115.2)
#define OSX_MOUNTAIN_LION_AND_HIGHER (NSAppKitVersionNumber >= 1162)
#define OSX_MAVERICKS_AND_HIGHER (NSAppKitVersionNumber >= 1244)
#define OSX_YOSEMITE_AND_HIGHER (NSAppKitVersionNumber >= 1334)
#define OSX_EL_CAPITAN_AND_HIGHER (NSAppKitVersionNumber >= 1404)
#define OSX_SIERRA_AND_HIGHER (NSAppKitVersionNumber >= 1485)
#define OSX_HIGH_SIERRA_AND_HIGHER (NSAppKitVersionNumber >= 1560)


// Sierra only APIs
#ifndef MAC_OS_X_VERSION_10_12

typedef NS_OPTIONS(NSUInteger, NSStatusItemBehavior) {

    NSStatusItemBehaviorRemovalAllowed = (1 << 1),
    NSStatusItemBehaviorTerminationOnRemoval = (1 << 2),
};

@interface NSStatusItem(IntroducedInSierra)

@property (assign) NSStatusItemBehavior behavior;
@property (assign, getter=isVisible) BOOL visible;
@property (null_resettable, copy) NSString *autosaveName;

@end

typedef NSUInteger NSWindowStyleMask;

#endif

void swapoutOverride(Class _Nonnull cls, SEL _Nonnull selector);

