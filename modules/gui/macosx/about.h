/*****************************************************************************
 * about.h: MacOS X About Panel
 *****************************************************************************
 * Copyright (C) 2001-2012 VLC authors and VideoLAN
 * $Id: d96efb07e7f875b5c4b43b3666f53b4598850c62 $
 *
 * Authors: Derk-Jan Hartman <thedj@users.sourceforge.net>
 *          Felix Paul Kühne <fkuehne -at- videolan.org>
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

#import <WebKit/WebKit.h> //we need to be here, because we're using a WebView object below

/*****************************************************************************
 * VLAboutBox interface
 *****************************************************************************/
@interface VLAboutBox : NSObject
{
    /* main about panel and stuff related to its views */
    IBOutlet id o_about_window;
    IBOutlet id o_name_version_field;
    IBOutlet id o_revision_field;
    IBOutlet id o_copyright_field;
    IBOutlet id o_credits_textview;
    IBOutlet id o_credits_scrollview;
    IBOutlet id o_gpl_btn;
    IBOutlet id o_name_field;
    id o_color_backdrop;

    NSTimer *o_scroll_timer;
    float f_current;
    float f_end;
    NSTimeInterval i_start;
    BOOL b_restart;
    BOOL b_isSetUp;

    /* generic help window */
    IBOutlet id o_help_window;
    IBOutlet WebView *o_help_web_view; //we may _not_ use id here because of method name collisions
    IBOutlet id o_help_bwd_btn;
    IBOutlet id o_help_fwd_btn;
    IBOutlet id o_help_home_btn;

    /* licence window */
    IBOutlet id o_gpl_window;
    IBOutlet id o_gpl_field;
}

+ (VLAboutBox *)sharedInstance;
- (void)showAbout;
- (void)showHelp;
- (IBAction)showGPL:(id)sender;
- (IBAction)helpGoHome:(id)sender;

@end

@interface VLAboutColoredBackdrop : NSView

@end
