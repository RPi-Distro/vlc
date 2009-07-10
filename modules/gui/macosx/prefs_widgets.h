/*****************************************************************************
 * prefs_widgets.h: Preferences controls
 *****************************************************************************
 * Copyright (C) 2002-2007 the VideoLAN team
 * $Id: 8d44a7733481de855f405c96669bb57c8b1f248d $
 *
 * Authors: Derk-Jan Hartman <hartman at videolan.org>
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

#define CONFIG_ITEM_STRING_LIST (CONFIG_ITEM_STRING + 1)
#define CONFIG_ITEM_RANGED_INTEGER (CONFIG_ITEM_INTEGER + 1)
#define CONFIG_ITEM_KEY_AFTER_10_3 (CONFIG_ITEM_KEY + 2)
#define LEFTMARGIN  18
#define RIGHTMARGIN 18

static NSMenu   *o_keys_menu = nil;

@interface VLCConfigControl : NSView
{
    module_config_t *p_item;
    char            *psz_name;
    NSTextField     *o_label;
    int             i_type;
    int             i_view_type;
    bool      b_advanced;
}

+ (VLCConfigControl *)newControl: (module_config_t *)_p_item
        withView: (NSView *)o_parent_view;
- (id)initWithFrame: (NSRect)frame item: (module_config_t *)p_item;
- (NSString *)name;
- (int)type;
- (int)viewType;
- (BOOL)isAdvanced;
- (void)setYPos:(int)i_yPos;
- (int)intValue;
- (float)floatValue;
- (char *)stringValue;
- (void)applyChanges;
- (void)resetValues;
- (int)labelSize;
- (void) alignWithXPosition:(int)i_xPos;

+ (int)calcVerticalMargin: (int)i_curItem lastItem:(int)i_lastItem;

@end

@interface StringConfigControl : VLCConfigControl
{
    NSTextField     *o_textfield;
}

- (id) initWithItem: (module_config_t *)_p_item
           withView: (NSView *)o_parent_view;

@end

@interface StringListConfigControl : VLCConfigControl
{
    NSComboBox      *o_combo;
}

- (id) initWithItem: (module_config_t *)_p_item
           withView: (NSView *)o_parent_view;

@end

@interface FileConfigControl : VLCConfigControl
{
    NSTextField     *o_textfield;
    NSButton        *o_button;
    BOOL            b_directory;
}

- (id) initWithItem: (module_config_t *)_p_item
           withView: (NSView *)o_parent_view;

- (IBAction)openFileDialog: (id)sender;
- (void)pathChosenInPanel:(NSOpenPanel *)o_sheet withReturn:(int)i_return_code contextInfo:(void  *)o_context_info;

@end

@interface ModuleConfigControl : VLCConfigControl
{
    NSPopUpButton   *o_popup;
}

- (id) initWithItem: (module_config_t *)_p_item
           withView: (NSView *)o_parent_view;

@end

@interface IntegerConfigControl : VLCConfigControl
{
    NSTextField     *o_textfield;
    NSStepper       *o_stepper;
}


- (id) initWithItem: (module_config_t *)_p_item
           withView: (NSView *)o_parent_view;
- (IBAction)stepperChanged:(id)sender;
- (void)textfieldChanged:(NSNotification *)o_notification;

@end

@interface IntegerListConfigControl : VLCConfigControl
{
    NSComboBox      *o_combo;
}

- (id) initWithItem: (module_config_t *)_p_item
           withView: (NSView *)o_parent_view;

@end

@interface RangedIntegerConfigControl : VLCConfigControl
{
    NSSlider        *o_slider;
    NSTextField     *o_textfield;
    NSTextField     *o_textfield_min;
    NSTextField     *o_textfield_max;
}


- (id) initWithItem: (module_config_t *)_p_item
           withView: (NSView *)o_parent_view;
- (IBAction)sliderChanged:(id)sender;
- (void)textfieldChanged:(NSNotification *)o_notification;

@end

@interface BoolConfigControl : VLCConfigControl
{
    NSButton        *o_checkbox;
}

- (id) initWithItem: (module_config_t *)_p_item
           withView: (NSView *)o_parent_view;

@end

@interface FloatConfigControl : VLCConfigControl
{
    NSTextField     *o_textfield;
    NSStepper       *o_stepper;
}


- (id) initWithItem: (module_config_t *)_p_item
           withView: (NSView *)o_parent_view;
- (IBAction)stepperChanged:(id)sender;
- (void)textfieldChanged:(NSNotification *)o_notification;

@end

@interface RangedFloatConfigControl : VLCConfigControl
{
    NSSlider        *o_slider;
    NSTextField     *o_textfield;
    NSTextField     *o_textfield_min;
    NSTextField     *o_textfield_max;
}


- (id) initWithItem: (module_config_t *)_p_item
           withView: (NSView *)o_parent_view;
- (IBAction)sliderChanged:(id)sender;
- (void)textfieldChanged:(NSNotification *)o_notification;

@end

@interface KeyConfigControl : VLCConfigControl
{
    NSPopUpButton   *o_popup;
}

- (id) initWithItem: (module_config_t *)_p_item
           withView: (NSView *)o_parent_view;

@end

@interface ModuleListConfigControl : VLCConfigControl
{
    NSTextField     *o_textfield;
    NSScrollView    *o_scrollview;
    NSMutableArray  *o_modulearray;
}

- (id) initWithItem: (module_config_t *)_p_item
           withView: (NSView *)o_parent_view;

@end

//#undef CONFIG_ITEM_LIST_STRING
//#undef CONFIG_ITEM_RANGED_INTEGER
//#undef CONFIG_ITEM_KEY_AFTER_10_3

