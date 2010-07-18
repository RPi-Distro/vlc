/*****************************************************************************
 * VLCProgressPanel.h: A Generic Progress Indicator Panel created for VLC
 *****************************************************************************
 * Copyright (C) 2009-2010 the VideoLAN team
 * $Id: 725f2ce5cea15f11dfa4ab00d0b4e155469ac6ee $
 *
 * Authors: Felix Paul Kühne <fkuehne at videolan dot org>
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


@interface VLCProgressPanel : NSPanel {
    BOOL _isCancelled;

    IBOutlet NSProgressIndicator * _progressBar;
    IBOutlet NSTextField *_titleField;
    IBOutlet NSTextField *_messageField;
    IBOutlet NSButton *_cancelButton;
    IBOutlet NSImageView *_iconView;
}
- (void)createContentView;

- setDialogTitle:(NSString *)title;
- setDialogMessage:(NSString *)message;
- setCancelButtonLabel:(NSString *)cancelLabel;
- setProgressAsDouble:(double)value;
- (BOOL)isCancelled;

- (IBAction)cancelDialog:(id)sender;

@end
