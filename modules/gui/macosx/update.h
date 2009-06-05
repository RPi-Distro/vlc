/*****************************************************************************
 * update.h: MacOS X Check-For-Update window
 *****************************************************************************
 * Copyright © 2005-2008 the VideoLAN team
 * $Id$
 *
 * Authors: Felix Kühne <fkuehne@users.sf.net>
 *          Rafaël Carré <funman@videolanorg>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
#   include "config.h"
#endif

#ifdef UPDATE_CHECK
#import <Cocoa/Cocoa.h>
#import "intf.h"
#import <vlc_update.h>


@interface VLCUpdate : NSObject
{
    IBOutlet id o_btn_DownloadNow;
    IBOutlet id o_btn_okay;
    IBOutlet id o_fld_releaseNote;
    IBOutlet id o_fld_currentVersion;
    IBOutlet id o_fld_status;
    IBOutlet id o_update_window;
    IBOutlet id o_bar_checking;
    IBOutlet id o_chk_updateOnStartup;

    update_t * p_u;
    bool b_checked;
}

- (void)end;

- (IBAction)download:(id)sender;
- (IBAction)okay:(id)sender;
- (IBAction)changeCheckUpdateOnStartup:(id)sender;

- (BOOL)shouldCheckForUpdate;

- (void)showUpdateWindow;
- (void)checkForUpdate;
- (void)performDownload:(NSString *)path;

+ (VLCUpdate *)sharedInstance;

@end
#endif
