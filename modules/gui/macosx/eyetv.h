/*****************************************************************************
* eyetv.h: small class to control the notification parts of the EyeTV plugin
*****************************************************************************
* Copyright (C) 2006-2007 the VideoLAN team
* $Id: c93dcfcc5bb71d5932b54264960ac4fa0e011bef $
*
* Authors: Felix Kühne <fkuehne at videolan dot org>
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


@interface VLCEyeTVController : NSObject
{
    BOOL b_eyeTVactive;
    BOOL b_deviceConnected;
}
- (void)globalNotificationReceived: (NSNotification *)theNotification;
- (BOOL)isEyeTVrunning;
- (BOOL)isDeviceConnected;
- (void)launchEyeTV;
- (int)currentChannel;
- (int)switchChannelUp:(BOOL)b_yesOrNo;
- (void)selectChannel:(int)theChannelNum;
- (NSEnumerator *)allChannels;

@end
