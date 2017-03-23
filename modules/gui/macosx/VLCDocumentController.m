/*****************************************************************************
 * VLCDocumentController.m
 ****************************************************************************
 * Copyright (C) 2016 VLC authors and VideoLAN
 * Authors: David Fuhrmann <david dot fuhrmann at googlemail dot com>
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

#import "VLCDocumentController.h"

#import "intf.h"

@implementation VLCDocumentController

- (IBAction)clearRecentDocuments:(id)sender
{
    msg_Dbg(VLCIntf, "Clear recent items list and resume points");
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    [defaults setObject:[NSDictionary dictionary] forKey:@"recentlyPlayedMedia"];
    [defaults setObject:[NSArray array] forKey:@"recentlyPlayedMediaList"];
    [defaults synchronize];

    [super clearRecentDocuments:sender];
}

@end
