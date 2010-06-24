/*****************************************************************************
 * misc.m: code not specific to vlc
 *****************************************************************************
 * Copyright (C) 2003-2009 the VideoLAN team
 * $Id: 4d77a3824b94d39f8b7cfdbd67d1507ee8fff918 $
 *
 * Authors: Jon Lech Johansen <jon-vl@nanocrew.net>
 *          Felix Paul Kühne <fkuehne at videolan dot org>
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
#import <Carbon/Carbon.h>

#import "intf.h"                                          /* VLCApplication */
#import "misc.h"
#import "playlist.h"
#import "controls.h"

/*****************************************************************************
 * NSImage (VLCAdditions)
 *
 *  Addition to NSImage
 *****************************************************************************/
@implementation NSImage (VLCAdditions)
+ (id)imageWithSystemName:(int)name
{
    /* ugly Carbon stuff following...
     * regrettably, you can't get the icons through clean Cocoa */

    /* retrieve our error icon */
    NSImage * icon;
    IconRef ourIconRef;
    int returnValue;
    returnValue = GetIconRef(kOnSystemDisk, 'macs', name, &ourIconRef);
    icon = [[[NSImage alloc] initWithSize:NSMakeSize(32,32)] autorelease];
    [icon lockFocus];
    CGRect rect = CGRectMake(0,0,32,32);
    PlotIconRefInContext((CGContextRef)[[NSGraphicsContext currentContext]
        graphicsPort],
        &rect,
        kAlignNone,
        kTransformNone,
        NULL /*inLabelColor*/,
        kPlotIconRefNormalFlags,
        (IconRef)ourIconRef);
    [icon unlockFocus];
    returnValue = ReleaseIconRef(ourIconRef);
    return icon;
}

+ (id)imageWithWarningIcon
{
    static NSImage * imageWithWarningIcon = nil;
    if( !imageWithWarningIcon )
    {
        imageWithWarningIcon = [[[self class] imageWithSystemName:'caut'] retain];
    }
    return imageWithWarningIcon;
}

+ (id)imageWithErrorIcon
{
    static NSImage * imageWithErrorIcon = nil;
    if( !imageWithErrorIcon )
    {
        imageWithErrorIcon = [[[self class] imageWithSystemName:'stop'] retain];
    }
    return imageWithErrorIcon;
}

@end
/*****************************************************************************
 * NSAnimation (VLCAdditions)
 *
 *  Missing extension to NSAnimation
 *****************************************************************************/

@implementation NSAnimation (VLCAdditions)
/* fake class attributes  */
static NSMapTable *VLCAdditions_userInfo = NULL;

+ (void)load
{
    /* init our fake object attribute */
    VLCAdditions_userInfo = NSCreateMapTable(NSNonRetainedObjectMapKeyCallBacks, NSObjectMapValueCallBacks, 16);
}

- (void)dealloc
{
    NSMapRemove(VLCAdditions_userInfo, self);
    [super dealloc];
}

- (void)setUserInfo: (void *)userInfo
{
    NSMapInsert(VLCAdditions_userInfo, self, (void*)userInfo);
}

- (void *)userInfo
{
    return NSMapGet(VLCAdditions_userInfo, self);
}
@end

/*****************************************************************************
 * NSScreen (VLCAdditions)
 *
 *  Missing extension to NSScreen
 *****************************************************************************/

@implementation NSScreen (VLCAdditions)

static NSMutableArray *blackoutWindows = NULL;

+ (void)load
{
    /* init our fake object attribute */
    blackoutWindows = [[NSMutableArray alloc] initWithCapacity:1];
}

+ (NSScreen *)screenWithDisplayID: (CGDirectDisplayID)displayID
{
    int i;
 
    for( i = 0; i < [[NSScreen screens] count]; i++ )
    {
        NSScreen *screen = [[NSScreen screens] objectAtIndex: i];
        if([screen displayID] == displayID)
            return screen;
    }
    return nil;
}

- (BOOL)isMainScreen
{
    return ([self displayID] == [[[NSScreen screens] objectAtIndex:0] displayID]);
}

- (BOOL)isScreen: (NSScreen*)screen
{
    return ([self displayID] == [screen displayID]);
}

- (CGDirectDisplayID)displayID
{
	return (CGDirectDisplayID)[[[self deviceDescription] objectForKey: @"NSScreenNumber"] intValue];
}

- (void)blackoutOtherScreens
{
    unsigned int i;

    /* Free our previous blackout window (follow blackoutWindow alloc strategy) */
    [blackoutWindows makeObjectsPerformSelector:@selector(close)];
    [blackoutWindows removeAllObjects];

    for(i = 0; i < [[NSScreen screens] count]; i++)
    {
        NSScreen *screen = [[NSScreen screens] objectAtIndex: i];
        VLCWindow *blackoutWindow;
        NSRect screen_rect;
 
        if([self isScreen: screen])
            continue;

        screen_rect = [screen frame];
        screen_rect.origin.x = screen_rect.origin.y = 0;

        /* blackoutWindow alloc strategy
            - The NSMutableArray blackoutWindows has the blackoutWindow references
            - blackoutOtherDisplays is responsible for alloc/releasing its Windows
        */
        blackoutWindow = [[VLCWindow alloc] initWithContentRect: screen_rect styleMask: NSBorderlessWindowMask
                backing: NSBackingStoreBuffered defer: NO screen: screen];
        [blackoutWindow setBackgroundColor:[NSColor blackColor]];
        [blackoutWindow setLevel: NSFloatingWindowLevel]; /* Disappear when Expose is triggered */
 
        [blackoutWindow displayIfNeeded];
        [blackoutWindow orderFront: self animate: YES];

        [blackoutWindows addObject: blackoutWindow];
        [blackoutWindow release];
        
        if( [screen isMainScreen ] )
           SetSystemUIMode( kUIModeAllHidden, kUIOptionAutoShowMenuBar);
    }
}

+ (void)unblackoutScreens
{
    unsigned int i;

    for(i = 0; i < [blackoutWindows count]; i++)
    {
        VLCWindow *blackoutWindow = [blackoutWindows objectAtIndex: i];
        [blackoutWindow closeAndAnimate: YES];
    }
    
   SetSystemUIMode( kUIModeNormal, 0);
}

@end

/*****************************************************************************
 * VLCWindow
 *
 *  Missing extension to NSWindow
 *****************************************************************************/

@implementation VLCWindow
- (id)initWithContentRect:(NSRect)contentRect styleMask:(NSUInteger)styleMask
    backing:(NSBackingStoreType)backingType defer:(BOOL)flag
{
    self = [super initWithContentRect:contentRect styleMask:styleMask backing:backingType defer:flag];
    if( self )
        b_isset_canBecomeKeyWindow = NO;
    return self;
}
- (void)setCanBecomeKeyWindow: (BOOL)canBecomeKey
{
    b_isset_canBecomeKeyWindow = YES;
    b_canBecomeKeyWindow = canBecomeKey;
}

- (BOOL)canBecomeKeyWindow
{
    if(b_isset_canBecomeKeyWindow)
        return b_canBecomeKeyWindow;

    return [super canBecomeKeyWindow];
}

- (void)closeAndAnimate: (BOOL)animate
{
    NSInvocation *invoc;
 
    if (!animate)
    {
        [super close];
        return;
    }

    invoc = [NSInvocation invocationWithMethodSignature:[super methodSignatureForSelector:@selector(close)]];
    [invoc setTarget: (id)super];

    if (![self isVisible] || [self alphaValue] == 0.0)
    {
        [super close];
        return;
    }

    [self orderOut: self animate: YES callback: invoc];
}

- (void)orderOut: (id)sender animate: (BOOL)animate
{
    NSInvocation *invoc = [NSInvocation invocationWithMethodSignature:[super methodSignatureForSelector:@selector(orderOut:)]];
    [invoc setTarget: (id)super];
    [invoc setArgument: sender atIndex: 0];
    [self orderOut: sender animate: animate callback: invoc];
}

- (void)orderOut: (id)sender animate: (BOOL)animate callback:(NSInvocation *)callback
{
    NSViewAnimation *anim;
    NSViewAnimation *current_anim;
    NSMutableDictionary *dict;

    if (!animate)
    {
        [self orderOut: sender];
        return;
    }

    dict = [[NSMutableDictionary alloc] initWithCapacity:2];

    [dict setObject:self forKey:NSViewAnimationTargetKey];

    [dict setObject:NSViewAnimationFadeOutEffect forKey:NSViewAnimationEffectKey];
    anim = [[NSViewAnimation alloc] initWithViewAnimations:[NSArray arrayWithObjects:dict, nil]];
    [dict release];

    [anim setAnimationBlockingMode:NSAnimationNonblocking];
    [anim setDuration:0.9];
    [anim setFrameRate:30];
    [anim setUserInfo: callback];

    @synchronized(self) {
        current_anim = self->animation;

        if ([[[current_anim viewAnimations] objectAtIndex:0] objectForKey: NSViewAnimationEffectKey] == NSViewAnimationFadeOutEffect && [current_anim isAnimating])
        {
            [anim release];
        }
        else
        {
            if (current_anim)
            {
                [current_anim stopAnimation];
                [anim setCurrentProgress:1.0-[current_anim currentProgress]];
                [current_anim release];
            }
            else
                [anim setCurrentProgress:1.0 - [self alphaValue]];
            self->animation = anim;
            [self setDelegate: self];
            [anim startAnimation];
        }
    }
}

- (void)orderFront: (id)sender animate: (BOOL)animate
{
    NSViewAnimation *anim;
    NSViewAnimation *current_anim;
    NSMutableDictionary *dict;
 
    if (!animate)
    {
        [super orderFront: sender];
        [self setAlphaValue: 1.0];
        return;
    }

    if (![self isVisible])
    {
        [self setAlphaValue: 0.0];
        [super orderFront: sender];
    }
    else if ([self alphaValue] == 1.0)
    {
        [super orderFront: self];
        return;
    }

    dict = [[NSMutableDictionary alloc] initWithCapacity:2];

    [dict setObject:self forKey:NSViewAnimationTargetKey];
 
    [dict setObject:NSViewAnimationFadeInEffect forKey:NSViewAnimationEffectKey];
    anim = [[NSViewAnimation alloc] initWithViewAnimations:[NSArray arrayWithObjects:dict, nil]];
    [dict release];
 
    [anim setAnimationBlockingMode:NSAnimationNonblocking];
    [anim setDuration:0.5];
    [anim setFrameRate:30];

    @synchronized(self) {
        current_anim = self->animation;

        if ([[[current_anim viewAnimations] objectAtIndex:0] objectForKey: NSViewAnimationEffectKey] == NSViewAnimationFadeInEffect && [current_anim isAnimating])
        {
            [anim release];
        }
        else
        {
            if (current_anim)
            {
                [current_anim stopAnimation];
                [anim setCurrentProgress:1.0 - [current_anim currentProgress]];
                [current_anim release];
            }
            else
                [anim setCurrentProgress:[self alphaValue]];
            self->animation = anim;
            [self setDelegate: self];
            [self orderFront: sender];
            [anim startAnimation];
        }
    }
}

- (void)animationDidEnd:(NSAnimation*)anim
{
    if ([self alphaValue] <= 0.0)
    {
        NSInvocation * invoc;
        [super orderOut: nil];
        [self setAlphaValue: 1.0];
        if ((invoc = [anim userInfo]))
            [invoc invoke];
    }
}
@end

/*****************************************************************************
 * VLCControllerWindow
 *****************************************************************************/

@implementation VLCControllerWindow

- (id)initWithContentRect:(NSRect)contentRect styleMask:(NSUInteger)styleMask
    backing:(NSBackingStoreType)backingType defer:(BOOL)flag
{
    /* FIXME: this should enable the SnowLeopard window style, however, it leads to ugly artifacts
     *        needs some further investigation! -- feepk
     BOOL b_useTextured = YES;

    if( [[NSWindow class] instancesRespondToSelector:@selector(setContentBorderThickness:forEdge:)] )
    {
        b_useTextured = NO;
        styleMask ^= NSTexturedBackgroundWindowMask;
    } */

    self = [super initWithContentRect:contentRect styleMask:styleMask //& ~NSTitledWindowMask
    backing:backingType defer:flag];

    [[VLCMain sharedInstance] updateTogglePlaylistState];

    /* FIXME: see above...
    if(! b_useTextured )
    {
        [self setContentBorderThickness:28.0 forEdge:NSMinYEdge];
    }
    */
    return self;
}

- (BOOL)performKeyEquivalent:(NSEvent *)o_event
{
    /* We indeed want to prioritize Cocoa key equivalent against libvlc,
       so we perform the menu equivalent now. */
    if([[NSApp mainMenu] performKeyEquivalent:o_event])
        return TRUE;

    return [[VLCMain sharedInstance] hasDefinedShortcutKey:o_event] ||
           [(VLCControls *)[[VLCMain sharedInstance] controls] keyEvent:o_event];
}

@end



/*****************************************************************************
 * VLCControllerView
 *****************************************************************************/

@implementation VLCControllerView

- (void)dealloc
{
    [self unregisterDraggedTypes];
    [super dealloc];
}

- (void)awakeFromNib
{
    [self registerForDraggedTypes:[NSArray arrayWithObjects:NSTIFFPboardType,
        NSFilenamesPboardType, nil]];
}

- (NSDragOperation)draggingEntered:(id <NSDraggingInfo>)sender
{
    if ((NSDragOperationGeneric & [sender draggingSourceOperationMask])
                == NSDragOperationGeneric)
    {
        return NSDragOperationGeneric;
    }
    else
    {
        return NSDragOperationNone;
    }
}

- (BOOL)prepareForDragOperation:(id <NSDraggingInfo>)sender
{
    return YES;
}

- (BOOL)performDragOperation:(id <NSDraggingInfo>)sender
{
    NSPasteboard *o_paste = [sender draggingPasteboard];
    NSArray *o_types = [NSArray arrayWithObjects: NSFilenamesPboardType, nil];
    NSString *o_desired_type = [o_paste availableTypeFromArray:o_types];
    NSData *o_carried_data = [o_paste dataForType:o_desired_type];

    if( o_carried_data )
    {
        if ([o_desired_type isEqualToString:NSFilenamesPboardType])
        {
            int i;
            NSArray *o_array = [NSArray array];
            NSArray *o_values = [[o_paste propertyListForType: NSFilenamesPboardType]
                        sortedArrayUsingSelector:@selector(caseInsensitiveCompare:)];

            for( i = 0; i < (int)[o_values count]; i++)
            {
                NSDictionary *o_dic;
                o_dic = [NSDictionary dictionaryWithObject:[o_values objectAtIndex:i] forKey:@"ITEM_URL"];
                o_array = [o_array arrayByAddingObject: o_dic];
            }
            [(VLCPlaylist *)[[VLCMain sharedInstance] playlist] appendArray: o_array atPos: -1 enqueue:NO];
            return YES;
        }
    }
    [self setNeedsDisplay:YES];
    return YES;
}

- (void)concludeDragOperation:(id <NSDraggingInfo>)sender
{
    [self setNeedsDisplay:YES];
}

@end

/*****************************************************************************
 * VLBrushedMetalImageView
 *****************************************************************************/

@implementation VLBrushedMetalImageView

- (BOOL)mouseDownCanMoveWindow
{
    return YES;
}

- (void)dealloc
{
    [self unregisterDraggedTypes];
    [super dealloc];
}

- (void)awakeFromNib
{
    [self registerForDraggedTypes:[NSArray arrayWithObjects:NSTIFFPboardType,
        NSFilenamesPboardType, nil]];
}

- (NSDragOperation)draggingEntered:(id <NSDraggingInfo>)sender
{
    if ((NSDragOperationGeneric & [sender draggingSourceOperationMask])
                == NSDragOperationGeneric)
    {
        return NSDragOperationGeneric;
    }
    else
    {
        return NSDragOperationNone;
    }
}

- (BOOL)prepareForDragOperation:(id <NSDraggingInfo>)sender
{
    return YES;
}

- (BOOL)performDragOperation:(id <NSDraggingInfo>)sender
{
    NSPasteboard *o_paste = [sender draggingPasteboard];
    NSArray *o_types = [NSArray arrayWithObjects: NSFilenamesPboardType, nil];
    NSString *o_desired_type = [o_paste availableTypeFromArray:o_types];
    NSData *o_carried_data = [o_paste dataForType:o_desired_type];
    BOOL b_autoplay = config_GetInt( VLCIntf, "macosx-autoplay" );

    if( o_carried_data )
    {
        if ([o_desired_type isEqualToString:NSFilenamesPboardType])
        {
            int i;
            NSArray *o_array = [NSArray array];
            NSArray *o_values = [[o_paste propertyListForType: NSFilenamesPboardType]
                        sortedArrayUsingSelector:@selector(caseInsensitiveCompare:)];

            for( i = 0; i < (int)[o_values count]; i++)
            {
                NSDictionary *o_dic;
                o_dic = [NSDictionary dictionaryWithObject:[o_values objectAtIndex:i] forKey:@"ITEM_URL"];
                o_array = [o_array arrayByAddingObject: o_dic];
            }
            if( b_autoplay )
                [[[VLCMain sharedInstance] playlist] appendArray: o_array atPos: -1 enqueue:NO];
            else
                [[[VLCMain sharedInstance] playlist] appendArray: o_array atPos: -1 enqueue:YES];
            return YES;
        }
    }
    [self setNeedsDisplay:YES];
    return YES;
}

- (void)concludeDragOperation:(id <NSDraggingInfo>)sender
{
    [self setNeedsDisplay:YES];
}

@end


/*****************************************************************************
 * MPSlider
 *****************************************************************************/
@implementation MPSlider

void _drawKnobInRect(NSRect knobRect)
{
    // Center knob in given rect
    knobRect.origin.x += (int)((float)(knobRect.size.width - 7)/2.0);
    knobRect.origin.y += (int)((float)(knobRect.size.height - 7)/2.0);
 
    // Draw diamond
    NSRectFillUsingOperation(NSMakeRect(knobRect.origin.x + 3, knobRect.origin.y + 6, 1, 1), NSCompositeSourceOver);
    NSRectFillUsingOperation(NSMakeRect(knobRect.origin.x + 2, knobRect.origin.y + 5, 3, 1), NSCompositeSourceOver);
    NSRectFillUsingOperation(NSMakeRect(knobRect.origin.x + 1, knobRect.origin.y + 4, 5, 1), NSCompositeSourceOver);
    NSRectFillUsingOperation(NSMakeRect(knobRect.origin.x + 0, knobRect.origin.y + 3, 7, 1), NSCompositeSourceOver);
    NSRectFillUsingOperation(NSMakeRect(knobRect.origin.x + 1, knobRect.origin.y + 2, 5, 1), NSCompositeSourceOver);
    NSRectFillUsingOperation(NSMakeRect(knobRect.origin.x + 2, knobRect.origin.y + 1, 3, 1), NSCompositeSourceOver);
    NSRectFillUsingOperation(NSMakeRect(knobRect.origin.x + 3, knobRect.origin.y + 0, 1, 1), NSCompositeSourceOver);
}

void _drawFrameInRect(NSRect frameRect)
{
    // Draw frame
    NSRectFillUsingOperation(NSMakeRect(frameRect.origin.x, frameRect.origin.y, frameRect.size.width, 1), NSCompositeSourceOver);
    NSRectFillUsingOperation(NSMakeRect(frameRect.origin.x, frameRect.origin.y + frameRect.size.height-1, frameRect.size.width, 1), NSCompositeSourceOver);
    NSRectFillUsingOperation(NSMakeRect(frameRect.origin.x, frameRect.origin.y, 1, frameRect.size.height), NSCompositeSourceOver);
    NSRectFillUsingOperation(NSMakeRect(frameRect.origin.x+frameRect.size.width-1, frameRect.origin.y, 1, frameRect.size.height), NSCompositeSourceOver);
}

- (void)drawRect:(NSRect)rect
{
    // Draw default to make sure the slider behaves correctly
    [[NSGraphicsContext currentContext] saveGraphicsState];
    NSRectClip(NSZeroRect);
    [super drawRect:rect];
    [[NSGraphicsContext currentContext] restoreGraphicsState];
 
    // Full size
    rect = [self bounds];
    int diff = (int)(([[self cell] knobThickness] - 7.0)/2.0) - 1;
    rect.origin.x += diff-1;
    rect.origin.y += diff;
    rect.size.width -= 2*diff-2;
    rect.size.height -= 2*diff;
 
    // Draw dark
    NSRect knobRect = [[self cell] knobRectFlipped:NO];
    [[[NSColor blackColor] colorWithAlphaComponent:0.6] set];
    _drawFrameInRect(rect);
    _drawKnobInRect(knobRect);
 
    // Draw shadow
    [[[NSColor blackColor] colorWithAlphaComponent:0.1] set];
    rect.origin.x++;
    rect.origin.y++;
    knobRect.origin.x++;
    knobRect.origin.y++;
    _drawFrameInRect(rect);
    _drawKnobInRect(knobRect);
}

@end


/*****************************************************************************
 * ITSlider
 *****************************************************************************/

@implementation ITSlider

- (void)awakeFromNib
{
    if ([[self cell] class] != [ITSliderCell class]) {
        // replace cell
        NSSliderCell *oldCell = [self cell];
        NSSliderCell *newCell = [[[ITSliderCell alloc] init] autorelease];
        [newCell setTag:[oldCell tag]];
        [newCell setTarget:[oldCell target]];
        [newCell setAction:[oldCell action]];
        [newCell setControlSize:[oldCell controlSize]];
        [newCell setType:[oldCell type]];
        [newCell setState:[oldCell state]];
        [newCell setAllowsTickMarkValuesOnly:[oldCell allowsTickMarkValuesOnly]];
        [newCell setAltIncrementValue:[oldCell altIncrementValue]];
        [newCell setControlTint:[oldCell controlTint]];
        [newCell setKnobThickness:[oldCell knobThickness]];
        [newCell setMaxValue:[oldCell maxValue]];
        [newCell setMinValue:[oldCell minValue]];
        [newCell setDoubleValue:[oldCell doubleValue]];
        [newCell setNumberOfTickMarks:[oldCell numberOfTickMarks]];
        [newCell setEditable:[oldCell isEditable]];
        [newCell setEnabled:[oldCell isEnabled]];
        [newCell setFormatter:[oldCell formatter]];
        [newCell setHighlighted:[oldCell isHighlighted]];
        [newCell setTickMarkPosition:[oldCell tickMarkPosition]];
        [self setCell:newCell];
    }
}

@end

/*****************************************************************************
 * ITSliderCell
 *****************************************************************************/
@implementation ITSliderCell

- (id)init
{
    self = [super init];
    _knobOff = [NSImage imageNamed:@"volumeslider_normal"];
    [self controlTintChanged];
    [[NSNotificationCenter defaultCenter] addObserver: self
                                             selector: @selector( controlTintChanged )
                                                 name: NSControlTintDidChangeNotification
                                               object: nil];
    b_mouse_down = FALSE;
    return self;
}

- (void)controlTintChanged
{
    if( [NSColor currentControlTint] == NSGraphiteControlTint )
        _knobOn = [NSImage imageNamed:@"volumeslider_graphite"];
    else
        _knobOn = [NSImage imageNamed:@"volumeslider_blue"];
}

- (void)dealloc
{
    [[NSNotificationCenter defaultCenter] removeObserver: self];
    [_knobOff release];
    [_knobOn release];
    [super dealloc];
}

- (void)drawKnob:(NSRect)knob_rect
{
    NSImage *knob;

    if( b_mouse_down )
        knob = _knobOn;
    else
        knob = _knobOff;

    [[self controlView] lockFocus];
    [knob compositeToPoint:NSMakePoint( knob_rect.origin.x + 1,
        knob_rect.origin.y + knob_rect.size.height -2 )
        operation:NSCompositeSourceOver];
    [[self controlView] unlockFocus];
}

- (void)stopTracking:(NSPoint)lastPoint at:(NSPoint)stopPoint inView:
        (NSView *)controlView mouseIsUp:(BOOL)flag
{
    b_mouse_down = NO;
    [self drawKnob];
    [super stopTracking:lastPoint at:stopPoint inView:controlView mouseIsUp:flag];
}

- (BOOL)startTrackingAt:(NSPoint)startPoint inView:(NSView *)controlView
{
    b_mouse_down = YES;
    [self drawKnob];
    return [super startTrackingAt:startPoint inView:controlView];
}

@end

