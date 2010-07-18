/*****************************************************************************
 * prefs.h: MacOS X module for vlc
 *****************************************************************************
 * Copyright (C) 2002-2005 the VideoLAN team
 * $Id: 3927d9c508ce1d184bdec7b442516a055c25818f $
 *
 * Authors: Jon Lech Johansen <jon-vl@nanocrew.net> 
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

@interface VLCTreeItem : NSObject
{
    NSString *o_name;
    NSString *o_title;
    NSString *o_help;
    int i_object_id;
    VLCTreeItem *o_parent;
    NSMutableArray *o_children;
    int i_object_category;
    NSMutableArray *o_subviews;
}

+ (VLCTreeItem *)rootItem;
- (int)numberOfChildren;
- (VLCTreeItem *)childAtIndex:(int)i_index;
- (int)getObjectID;
- (NSString *)getName;
- (NSString *)getTitle;
- (NSString *)getHelp;
- (BOOL)hasPrefs:(NSString *)o_module_name;
- (NSView *)showView:(NSScrollView *)o_prefs_view advancedView:(vlc_bool_t) b_advanced;
- (void)applyChanges;
- (void)resetView;

@end

/*****************************************************************************
 * VLCPrefs interface
 *****************************************************************************/
@interface VLCPrefs : NSObject
{
    intf_thread_t *p_intf;
    vlc_bool_t b_advanced;
    VLCTreeItem *o_config_tree;
    NSView *o_empty_view;
    NSMutableDictionary *o_save_prefs;
    
    IBOutlet id o_prefs_window;
    IBOutlet id o_title;
    IBOutlet id o_tree;
    IBOutlet id o_prefs_view;
    IBOutlet id o_save_btn;
    IBOutlet id o_cancel_btn;
    IBOutlet id o_reset_btn;
    IBOutlet id o_advanced_ckb;
}

+ (VLCPrefs *)sharedInstance;

- (void)initStrings;
- (void)setTitle: (NSString *) o_title_name;
- (void)showPrefs;
- (IBAction)savePrefs: (id)sender;
- (IBAction)closePrefs: (id)sender;
- (IBAction)resetAll: (id)sender;
- (IBAction)advancedToggle: (id)sender;

@end

@interface VLCFlippedView : NSView
{

}

@end
