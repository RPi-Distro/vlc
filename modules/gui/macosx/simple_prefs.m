/*****************************************************************************
* simple_prefs.m: Simple Preferences for Mac OS X
*****************************************************************************
* Copyright (C) 2008-2009 the VideoLAN team
* $Id: a38642901785e97a4d83587ea0f4630ec8d19215 $
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

#import "simple_prefs.h"
#import "prefs.h"
#import <vlc_keys.h>
#import <vlc_interface.h>
#import <vlc_dialog.h>
#import "misc.h"
#import "intf.h"
#import "AppleRemote.h"
#import <Sparkle/Sparkle.h>                        //for o_intf_last_update_lbl

static NSString* VLCSPrefsToolbarIdentifier = @"Our Simple Preferences Toolbar Identifier";
static NSString* VLCIntfSettingToolbarIdentifier = @"Intf Settings Item Identifier";
static NSString* VLCAudioSettingToolbarIdentifier = @"Audio Settings Item Identifier";
static NSString* VLCVideoSettingToolbarIdentifier = @"Video Settings Item Identifier";
static NSString* VLCOSDSettingToolbarIdentifier = @"Subtitles Settings Item Identifier";
static NSString* VLCInputSettingToolbarIdentifier = @"Input Settings Item Identifier";
static NSString* VLCHotkeysSettingToolbarIdentifier = @"Hotkeys Settings Item Identifier";

@implementation VLCSimplePrefs

static VLCSimplePrefs *_o_sharedInstance = nil;

+ (VLCSimplePrefs *)sharedInstance
{
    return _o_sharedInstance ? _o_sharedInstance : [[self alloc] init];
}

- (id)init
{
    if (_o_sharedInstance) {
        [self dealloc];
    } else {
        _o_sharedInstance = [super init];
        p_intf = VLCIntf;
    }

    return _o_sharedInstance;
}

- (void)dealloc
{
    [o_currentlyShownCategoryView release];

    [o_hotkeySettings release];
    [o_hotkeyDescriptions release];
    [o_hotkeysNonUseableKeys release];

    [o_keyInTransition release];

    [super dealloc];
}


- (NSString *)OSXKeyToString:(int)val
{
    NSMutableString *o_temp_str = [[[NSMutableString alloc] init] autorelease];
    if( val & KEY_MODIFIER_CTRL )
        [o_temp_str appendString: [NSString stringWithUTF8String: "\xE2\x8C\x83"]];
    if( val & KEY_MODIFIER_ALT )
        [o_temp_str appendString: [NSString stringWithUTF8String: "\xE2\x8C\xA5"]];
    if( val & KEY_MODIFIER_SHIFT )
        [o_temp_str appendString: [NSString stringWithUTF8String: "\xE2\x87\xA7"]];
    if( val & KEY_MODIFIER_COMMAND )
        [o_temp_str appendString: [NSString stringWithUTF8String: "\xE2\x8C\x98"]];

    char *base = KeyToString( val & ~KEY_MODIFIER );
    if( base )
    {
        [o_temp_str appendString: [NSString stringWithUTF8String: base]];
        free( base );
    }
    else
        o_temp_str = [NSMutableString stringWithString:_NS("Not Set")];
    return o_temp_str;
}

- (void)awakeFromNib
{
    [self initStrings];

    /* setup the toolbar */
    NSToolbar * o_sprefs_toolbar = [[[NSToolbar alloc] initWithIdentifier: VLCSPrefsToolbarIdentifier] autorelease];
    [o_sprefs_toolbar setAllowsUserCustomization: NO];
    [o_sprefs_toolbar setAutosavesConfiguration: NO];
    [o_sprefs_toolbar setDisplayMode: NSToolbarDisplayModeIconAndLabel];
    [o_sprefs_toolbar setSizeMode: NSToolbarSizeModeRegular];
    [o_sprefs_toolbar setDelegate: self];
    [o_sprefs_win setToolbar: o_sprefs_toolbar];

    /* setup useful stuff */
    o_hotkeysNonUseableKeys = [[NSArray arrayWithObjects:
                                [NSNumber numberWithInt: KEY_MODIFIER_COMMAND|'c'],
                                [NSNumber numberWithInt: KEY_MODIFIER_COMMAND|'x'],
                                [NSNumber numberWithInt: KEY_MODIFIER_COMMAND|'v'],
                                [NSNumber numberWithInt: KEY_MODIFIER_COMMAND|'a'],
                                [NSNumber numberWithInt: KEY_MODIFIER_COMMAND|','],
                                [NSNumber numberWithInt: KEY_MODIFIER_COMMAND|'h'],
                                [NSNumber numberWithInt: KEY_MODIFIER_COMMAND|KEY_MODIFIER_ALT|'h'],
                                [NSNumber numberWithInt: KEY_MODIFIER_COMMAND|KEY_MODIFIER_SHIFT|'o'],
                                [NSNumber numberWithInt: KEY_MODIFIER_COMMAND|'o'],
                                [NSNumber numberWithInt: KEY_MODIFIER_COMMAND|'d'],
                                [NSNumber numberWithInt: KEY_MODIFIER_COMMAND|'n'],
                                [NSNumber numberWithInt: KEY_MODIFIER_COMMAND|'s'],
                                [NSNumber numberWithInt: KEY_MODIFIER_COMMAND|'z'],
                                [NSNumber numberWithInt: KEY_MODIFIER_COMMAND|'l'],
                                [NSNumber numberWithInt: KEY_MODIFIER_COMMAND|'r'],
                                [NSNumber numberWithInt: KEY_MODIFIER_COMMAND|'0'],
                                [NSNumber numberWithInt: KEY_MODIFIER_COMMAND|'1'],
                                [NSNumber numberWithInt: KEY_MODIFIER_COMMAND|'2'],
                                [NSNumber numberWithInt: KEY_MODIFIER_COMMAND|'3'],
                                [NSNumber numberWithInt: KEY_MODIFIER_COMMAND|'m'],
                                [NSNumber numberWithInt: KEY_MODIFIER_COMMAND|'w'],
                                [NSNumber numberWithInt: KEY_MODIFIER_COMMAND|KEY_MODIFIER_SHIFT|'w'],
                                [NSNumber numberWithInt: KEY_MODIFIER_COMMAND|KEY_MODIFIER_SHIFT|'c'],
                                [NSNumber numberWithInt: KEY_MODIFIER_COMMAND|KEY_MODIFIER_SHIFT|'p'],
                                [NSNumber numberWithInt: KEY_MODIFIER_COMMAND|'i'],
                                [NSNumber numberWithInt: KEY_MODIFIER_COMMAND|'e'],
                                [NSNumber numberWithInt: KEY_MODIFIER_COMMAND|KEY_MODIFIER_SHIFT|'e'],
                                [NSNumber numberWithInt: KEY_MODIFIER_COMMAND|'b'],
                                [NSNumber numberWithInt: KEY_MODIFIER_COMMAND|KEY_MODIFIER_SHIFT|'m'],
                                [NSNumber numberWithInt: KEY_MODIFIER_COMMAND|KEY_MODIFIER_CTRL|'m'],
                                [NSNumber numberWithInt: KEY_MODIFIER_COMMAND|'?'],
                                [NSNumber numberWithInt: KEY_MODIFIER_COMMAND|KEY_MODIFIER_ALT|'?'],
                                nil] retain];
}

#define CreateToolbarItem( o_name, o_desc, o_img, sel ) \
    o_toolbarItem = create_toolbar_item(o_itemIdent, o_name, o_desc, o_img, self, @selector(sel));
static inline NSToolbarItem *
create_toolbar_item( NSString * o_itemIdent, NSString * o_name, NSString * o_desc, NSString * o_img, id target, SEL selector )
{
    NSToolbarItem *o_toolbarItem = [[[NSToolbarItem alloc] initWithItemIdentifier: o_itemIdent] autorelease]; \

    [o_toolbarItem setLabel: o_name];
    [o_toolbarItem setPaletteLabel: o_desc];

    [o_toolbarItem setToolTip: o_desc];
    [o_toolbarItem setImage: [NSImage imageNamed: o_img]];

    [o_toolbarItem setTarget: target];
    [o_toolbarItem setAction: selector];

    [o_toolbarItem setEnabled: YES];
    [o_toolbarItem setAutovalidates: YES];

    return o_toolbarItem;
}

- (NSToolbarItem *) toolbar: (NSToolbar *)o_sprefs_toolbar
      itemForItemIdentifier: (NSString *)o_itemIdent
  willBeInsertedIntoToolbar: (BOOL)b_willBeInserted
{
    NSToolbarItem *o_toolbarItem = nil;

    if( [o_itemIdent isEqual: VLCIntfSettingToolbarIdentifier] )
    {
        CreateToolbarItem( _NS("Interface"), _NS("Interface Settings"), @"spref_cone_Interface_64", showInterfaceSettings );
    }
    else if( [o_itemIdent isEqual: VLCAudioSettingToolbarIdentifier] )
    {
        CreateToolbarItem( _NS("Audio"), _NS("General Audio Settings"), @"spref_cone_Audio_64", showAudioSettings );
    }
    else if( [o_itemIdent isEqual: VLCVideoSettingToolbarIdentifier] )
    {
        CreateToolbarItem( _NS("Video"), _NS("General Video Settings"), @"spref_cone_Video_64", showVideoSettings );
    }
    else if( [o_itemIdent isEqual: VLCOSDSettingToolbarIdentifier] )
    {
        CreateToolbarItem( _NS("Subtitles & OSD"), _NS("Subtitles & On Screen Display Settings"), @"spref_cone_Subtitles_64", showOSDSettings );
    }
    else if( [o_itemIdent isEqual: VLCInputSettingToolbarIdentifier] )
    {
        CreateToolbarItem( _NS("Input & Codecs"), _NS("Input & Codec settings"), @"spref_cone_Input_64", showInputSettings );
    }
    else if( [o_itemIdent isEqual: VLCHotkeysSettingToolbarIdentifier] )
    {
        CreateToolbarItem( _NS("Hotkeys"), _NS("Hotkeys settings"), @"spref_cone_Hotkeys_64", showHotkeySettings );
    }

    return o_toolbarItem;
}

- (NSArray *)toolbarDefaultItemIdentifiers: (NSToolbar *)toolbar
{
    return [NSArray arrayWithObjects: VLCIntfSettingToolbarIdentifier, VLCAudioSettingToolbarIdentifier, VLCVideoSettingToolbarIdentifier,
        VLCOSDSettingToolbarIdentifier, VLCInputSettingToolbarIdentifier, VLCHotkeysSettingToolbarIdentifier, NSToolbarFlexibleSpaceItemIdentifier, nil];
}

- (NSArray *)toolbarAllowedItemIdentifiers: (NSToolbar *)toolbar
{
    return [NSArray arrayWithObjects: VLCIntfSettingToolbarIdentifier, VLCAudioSettingToolbarIdentifier, VLCVideoSettingToolbarIdentifier,
        VLCOSDSettingToolbarIdentifier, VLCInputSettingToolbarIdentifier, VLCHotkeysSettingToolbarIdentifier, NSToolbarFlexibleSpaceItemIdentifier, nil];
}

- (NSArray *)toolbarSelectableItemIdentifiers:(NSToolbar *)toolbar
{
    return [NSArray arrayWithObjects: VLCIntfSettingToolbarIdentifier, VLCAudioSettingToolbarIdentifier, VLCVideoSettingToolbarIdentifier,
        VLCOSDSettingToolbarIdentifier, VLCInputSettingToolbarIdentifier, VLCHotkeysSettingToolbarIdentifier, nil];
}

- (void)initStrings
{
    /* audio */
    [o_audio_dolby_txt setStringValue: _NS("Force detection of Dolby Surround")];
    [o_audio_effects_box setTitle: _NS("Effects")];
    [o_audio_enable_ckb setTitle: _NS("Enable Audio")];
    [o_audio_general_box setTitle: _NS("General Audio")];
    [o_audio_headphone_ckb setTitle: _NS("Headphone surround effect")];
    [o_audio_lang_txt setStringValue: _NS("Preferred Audio language")];
    [o_audio_last_ckb setTitle: _NS("Enable Last.fm submissions")];
    [o_audio_lastpwd_txt setStringValue: _NS("Password")];
    [o_audio_lastuser_txt setStringValue: _NS("User name")];
    [o_audio_norm_ckb setTitle: _NS("Volume normalizer")];
    [o_audio_spdif_ckb setTitle: _NS("Use S/PDIF when available")];
    [o_audio_visual_txt setStringValue: _NS("Visualization")];
    [o_audio_vol_txt setStringValue: _NS("Default Volume")];

    /* hotkeys */
    [o_hotkeys_change_btn setTitle: _NS("Change")];
    [o_hotkeys_change_win setTitle: _NS("Change Hotkey")];
    [o_hotkeys_change_cancel_btn setTitle: _NS("Cancel")];
    [o_hotkeys_change_ok_btn setTitle: _NS("OK")];
    [o_hotkeys_clear_btn setTitle: _NS("Clear")];
    [o_hotkeys_lbl setStringValue: _NS("Select an action to change the associated hotkey:")];
    [[[o_hotkeys_listbox tableColumnWithIdentifier: @"action"] headerCell] setStringValue: _NS("Action")];
    [[[o_hotkeys_listbox tableColumnWithIdentifier: @"shortcut"] headerCell] setStringValue: _NS("Shortcut")];

    /* input */
    [o_input_avi_txt setStringValue: _NS("Repair AVI Files")];
    [o_input_cachelevel_txt setStringValue: _NS("Default Caching Level")];
    [o_input_caching_box setTitle: _NS("Caching")];
    [o_input_cachelevel_custom_txt setStringValue: _NS("Use the complete preferences to configure custom caching values for each access module.")];
    [o_input_httpproxy_txt setStringValue: _NS("HTTP Proxy")];
    [o_input_httpproxypwd_txt setStringValue: _NS("Password for HTTP Proxy")];
    [o_input_mux_box setTitle: _NS("Codecs / Muxers")];
    [o_input_net_box setTitle: _NS("Network")];
    [o_input_postproc_txt setStringValue: _NS("Post-Processing Quality")];
    [o_input_rtsp_ckb setTitle: _NS("Use RTP over RTSP (TCP)")];
    [o_input_skipLoop_txt setStringValue: _NS("Skip the loop filter for H.264 decoding")];
    [o_input_serverport_txt setStringValue: _NS("Default Server Port")];

    /* interface */
    [o_intf_art_txt setStringValue: _NS("Album art download policy")];
    [o_intf_embedded_ckb setTitle: _NS("Add controls to the video window")];
    [o_intf_fspanel_ckb setTitle: _NS("Show Fullscreen Controller")];
    [o_intf_lang_txt setStringValue: _NS("Language")];
    [o_intf_network_box setTitle: _NS("Privacy / Network Interaction")];
	[o_intf_appleremote_ckb setTitle: _NS("Control playback with the Apple Remote")];
	[o_intf_mediakeys_ckb setTitle: _NS("Control playback with media keys")];
    [o_intf_mediakeys_bg_ckb setTitle: _NS("...when VLC is in background")];
    [o_intf_update_ckb setTitle: _NS("Automatically check for updates")];
    [o_intf_last_update_lbl setStringValue: @""];

    /* Subtitles and OSD */
    [o_osd_encoding_txt setStringValue: _NS("Default Encoding")];
    [o_osd_font_box setTitle: _NS("Display Settings")];
    [o_osd_font_btn setTitle: _NS("Choose...")];
    [o_osd_font_color_txt setStringValue: _NS("Font Color")];
    [o_osd_font_size_txt setStringValue: _NS("Font Size")];
    [o_osd_font_txt setStringValue: _NS("Font")];
    [o_osd_lang_box setTitle: _NS("Subtitle Languages")];
    [o_osd_lang_txt setStringValue: _NS("Preferred Subtitle Language")];
    [o_osd_osd_box setTitle: _NS("On Screen Display")];
    [o_osd_osd_ckb setTitle: _NS("Enable OSD")];

    /* video */
    [o_video_black_ckb setTitle: _NS("Black screens in Fullscreen mode")];
    [o_video_device_txt setStringValue: _NS("Fullscreen Video Device")];
    [o_video_display_box setTitle: _NS("Display")];
    [o_video_enable_ckb setTitle: _NS("Enable Video")];
    [o_video_fullscreen_ckb setTitle: _NS("Fullscreen")];
    [o_video_onTop_ckb setTitle: _NS("Always on top")];
    [o_video_output_txt setStringValue: _NS("Output module")];
    [o_video_skipFrames_ckb setTitle: _NS("Skip frames")];
    [o_video_snap_box setTitle: _NS("Video snapshots")];
    [o_video_snap_folder_btn setTitle: _NS("Browse...")];
    [o_video_snap_folder_txt setStringValue: _NS("Folder")];
    [o_video_snap_format_txt setStringValue: _NS("Format")];
    [o_video_snap_prefix_txt setStringValue: _NS("Prefix")];
    [o_video_snap_seqnum_ckb setTitle: _NS("Sequential numbering")];

    /* generic stuff */
    [[o_sprefs_basicFull_matrix cellAtRow: 0 column: 0] setStringValue: _NS("Basic")];
    [[o_sprefs_basicFull_matrix cellAtRow: 0 column: 1] setStringValue: _NS("All")];
    [o_sprefs_cancel_btn setTitle: _NS("Cancel")];
    [o_sprefs_reset_btn setTitle: _NS("Reset All")];
    [o_sprefs_save_btn setTitle: _NS("Save")];
    [o_sprefs_win setTitle: _NS("Preferences")];
}

/* TODO: move this part to core */
#define config_GetLabel(a,b) __config_GetLabel(VLC_OBJECT(a),b)
static inline char * __config_GetLabel( vlc_object_t *p_this, const char *psz_name )
{
    module_config_t *p_config;

    p_config = config_FindConfig( p_this, psz_name );

    /* sanity checks */
    if( !p_config )
    {
        msg_Err( p_this, "option %s does not exist", psz_name );
        return NULL;
    }

    if ( p_config->psz_longtext )
        return p_config->psz_longtext;
    else if( p_config->psz_text )
        return p_config->psz_text;
    else
        msg_Warn( p_this, "option %s does not include any help", psz_name );

    return NULL;
}

- (void)setupButton: (NSPopUpButton *)object forStringList: (const char *)name
{
    module_config_t *p_item;

    [object removeAllItems];
    p_item = config_FindConfig( VLC_OBJECT(p_intf), name );

    /* serious problem, if no item found */
    assert( p_item );

    for( int i = 0; i < p_item->i_list; i++ )
    {
        NSMenuItem *mi;
        if( p_item->ppsz_list_text != NULL )
            mi = [[NSMenuItem alloc] initWithTitle: _NS( p_item->ppsz_list_text[i] ) action:NULL keyEquivalent: @""];
        else if( p_item->ppsz_list[i] && strcmp(p_item->ppsz_list[i],"") == 0 )
        {
            [[object menu] addItem: [NSMenuItem separatorItem]];
            continue;
        }
        else if( p_item->ppsz_list[i] )
            mi = [[NSMenuItem alloc] initWithTitle: [NSString stringWithUTF8String: p_item->ppsz_list[i]] action:NULL keyEquivalent: @""];
        else
            msg_Err( p_intf, "item %d of pref %s failed to be created", i, name );
        [mi setRepresentedObject:[NSString stringWithUTF8String: p_item->ppsz_list[i]]];
        [[object menu] addItem: [mi autorelease]];
        if( p_item->value.psz && !strcmp( p_item->value.psz, p_item->ppsz_list[i] ) )
            [object selectItem:[object lastItem]];
    }
    [object setToolTip: _NS( p_item->psz_longtext )];
}

- (void)setupButton: (NSPopUpButton *)object forIntList: (const char *)name
{
    module_config_t *p_item;

    [object removeAllItems];
    p_item = config_FindConfig( VLC_OBJECT(p_intf), name );

    /* serious problem, if no item found */
    assert( p_item );

    for( int i = 0; i < p_item->i_list; i++ )
    {
        NSMenuItem *mi;
        if( p_item->ppsz_list_text != NULL)
            mi = [[NSMenuItem alloc] initWithTitle: _NS( p_item->ppsz_list_text[i] ) action:NULL keyEquivalent: @""];
        else if( p_item->pi_list[i] )
            mi = [[NSMenuItem alloc] initWithTitle: [NSString stringWithFormat: @"%d", p_item->pi_list[i]] action:NULL keyEquivalent: @""];
        else
            msg_Err( p_intf, "item %d of pref %s failed to be created", i, name);
        [mi setRepresentedObject:[NSNumber numberWithInt: p_item->pi_list[i]]];
        [[object menu] addItem: [mi autorelease]];
        if( p_item->value.i == p_item->pi_list[i] )
            [object selectItem:[object lastItem]];
    }
    [object setToolTip: _NS( p_item->psz_longtext )];
}

- (void)setupButton: (NSPopUpButton *)object forModuleList: (const char *)name
{
    module_config_t *p_item;
    module_t *p_parser, **p_list;
    int y = 0;

    [object removeAllItems];

    p_item = config_FindConfig( VLC_OBJECT(p_intf), name );
    p_list = module_list_get( NULL );
    if( !p_item ||!p_list )
    {
        if( p_list ) module_list_free(p_list);
        msg_Err( p_intf, "serious problem, item or list not found" );
        return;
    }

    [object addItemWithTitle: _NS("Default")];
    for( size_t i_index = 0; p_list[i_index]; i_index++ )
    {
        p_parser = p_list[i_index];
        if( module_provides( p_parser, p_item->psz_type ) )
        {
            [object addItemWithTitle: [NSString stringWithUTF8String: module_GetLongName( p_parser ) ?: ""]];
            if( p_item->value.psz && !strcmp( p_item->value.psz, module_get_object( p_parser ) ) )
                [object selectItem: [object lastItem]];
        }
    }
    module_list_free( p_list );
    [object setToolTip: _NS(p_item->psz_longtext)];
}

- (void)setupButton: (NSButton *)object forBoolValue: (const char *)name
{
    [object setState: config_GetInt( p_intf, name )];
    [object setToolTip: [NSString stringWithUTF8String: config_GetLabel( p_intf, name )]];
}

- (void)setupField:(NSTextField *)o_object forOption:(const char *)psz_option
{
    char *psz_tmp = config_GetPsz( p_intf, psz_option );
    [o_object setStringValue: [NSString stringWithUTF8String: psz_tmp ?: ""]];
    [o_object setToolTip: [NSString stringWithUTF8String: config_GetLabel( p_intf, psz_option )]];
    free( psz_tmp );
}

- (void)resetControls
{
    module_config_t *p_item;
    int i, y = 0;
    char *psz_tmp;

    [[o_sprefs_basicFull_matrix cellAtRow:0 column:0] setState: NSOnState];
    [[o_sprefs_basicFull_matrix cellAtRow:0 column:1] setState: NSOffState];

    /**********************
     * interface settings *
     **********************/
    [self setupButton: o_intf_lang_pop forStringList: "language"];
    [self setupButton: o_intf_art_pop forIntList: "album-art"];

    [self setupButton: o_intf_fspanel_ckb forBoolValue: "macosx-fspanel"];
    [self setupButton: o_intf_embedded_ckb forBoolValue: "embedded-video"];
	[self setupButton: o_intf_appleremote_ckb forBoolValue: "macosx-appleremote"];
	[self setupButton: o_intf_mediakeys_ckb forBoolValue: "macosx-mediakeys"];
    [self setupButton: o_intf_mediakeys_bg_ckb forBoolValue: "macosx-mediakeys-background"];
    [o_intf_mediakeys_bg_ckb setEnabled: [o_intf_mediakeys_ckb state]];
    if( [[SUUpdater sharedUpdater] lastUpdateCheckDate] != NULL )
        [o_intf_last_update_lbl setStringValue: [NSString stringWithFormat: _NS("Last check on: %@"), [[[SUUpdater sharedUpdater] lastUpdateCheckDate] descriptionWithLocale: [[NSUserDefaults standardUserDefaults] dictionaryRepresentation]]]];
    else
        [o_intf_last_update_lbl setStringValue: _NS("No check was performed yet.")];

    /******************
     * audio settings *
     ******************/
    [self setupButton: o_audio_enable_ckb forBoolValue: "audio"];
    i = (config_GetInt( p_intf, "volume" ) * 0.390625);
    [o_audio_vol_fld setToolTip: [NSString stringWithUTF8String: config_GetLabel( p_intf, "volume")]];
    [o_audio_vol_fld setIntValue: i];
    [o_audio_vol_sld setToolTip: [o_audio_vol_fld toolTip]];
    [o_audio_vol_sld setIntValue: i];

    [self setupButton: o_audio_spdif_ckb forBoolValue: "spdif"];

    [self setupButton: o_audio_dolby_pop forIntList: "force-dolby-surround"];
    [self setupField: o_audio_lang_fld forOption: "audio-language"];

    [self setupButton: o_audio_headphone_ckb forBoolValue: "headphone-dolby"];

    psz_tmp = config_GetPsz( p_intf, "audio-filter" );
    if( psz_tmp )
    {
        [o_audio_norm_ckb setState: (NSInteger)strstr( psz_tmp, "volnorm" )];
        [o_audio_norm_fld setEnabled: [o_audio_norm_ckb state]];
        [o_audio_norm_stepper setEnabled: [o_audio_norm_ckb state]];
        free( psz_tmp );
    }
    [o_audio_norm_fld setFloatValue: config_GetFloat( p_intf, "norm-max-level" )];
    [o_audio_norm_fld setToolTip: [NSString stringWithUTF8String: config_GetLabel( p_intf, "norm-max-level")]];

    [self setupButton: o_audio_visual_pop forModuleList: "audio-visual"];

    /* Last.FM is optional */
    if( module_exists( "audioscrobbler" ) )
    {
        [self setupField: o_audio_lastuser_fld forOption:"lastfm-username"];
        [self setupField: o_audio_lastpwd_sfld forOption:"lastfm-password"];

        if( config_ExistIntf( VLC_OBJECT( p_intf ), "audioscrobbler" ) )
        {
            [o_audio_last_ckb setState: NSOnState];
            [o_audio_lastuser_fld setEnabled: YES];
            [o_audio_lastpwd_sfld setEnabled: YES];
        }
        else
        {
            [o_audio_last_ckb setState: NSOffState];
            [o_audio_lastuser_fld setEnabled: NO];
            [o_audio_lastpwd_sfld setEnabled: NO];
        }
    }
    else
        [o_audio_last_ckb setEnabled: NO];

    /******************
     * video settings *
     ******************/
    [self setupButton: o_video_enable_ckb forBoolValue: "video"];
    [self setupButton: o_video_fullscreen_ckb forBoolValue: "fullscreen"];
    [self setupButton: o_video_onTop_ckb forBoolValue: "video-on-top"];
    [self setupButton: o_video_skipFrames_ckb forBoolValue: "skip-frames"];
    [self setupButton: o_video_black_ckb forBoolValue: "macosx-black"];

    [self setupButton: o_video_output_pop forModuleList: "vout"];

    [o_video_device_pop removeAllItems];
    i = 0;
    y = [[NSScreen screens] count];
    [o_video_device_pop addItemWithTitle: _NS("Default")];
    [[o_video_device_pop lastItem] setTag: 0];
    while( i < y )
    {
        NSRect s_rect = [[[NSScreen screens] objectAtIndex: i] frame];
        [o_video_device_pop addItemWithTitle:
         [NSString stringWithFormat: @"%@ %i (%ix%i)", _NS("Screen"), i+1,
                   (int)s_rect.size.width, (int)s_rect.size.height]];
        [[o_video_device_pop lastItem] setTag: (int)[[[NSScreen screens] objectAtIndex: i] displayID]];
        i++;
    }
    [o_video_device_pop selectItemAtIndex: 0];
    [o_video_device_pop selectItemWithTag: config_GetInt( p_intf, "macosx-vdev" )];

    [self setupField: o_video_snap_folder_fld forOption:"snapshot-path"];
    [self setupField: o_video_snap_prefix_fld forOption:"snapshot-prefix"];
    [self setupButton: o_video_snap_seqnum_ckb forBoolValue: "snapshot-sequential"];
    [self setupButton: o_video_snap_format_pop forStringList: "snapshot-format"];

    /***************************
     * input & codecs settings *
     ***************************/
    [o_input_serverport_fld setIntValue: config_GetInt( p_intf, "server-port")];
    [o_input_serverport_fld setToolTip: [NSString stringWithUTF8String: config_GetLabel( p_intf, "server-port")]];
    [self setupField: o_input_httpproxy_fld forOption:"http-proxy"];
    [self setupField: o_input_httpproxypwd_sfld forOption:"http-proxy-pwd"];
    [o_input_postproc_fld setIntValue: config_GetInt( p_intf, "postproc-q")];
    [o_input_postproc_fld setToolTip: [NSString stringWithUTF8String: config_GetLabel( p_intf, "postproc-q")]];

    [self setupButton: o_input_avi_pop forIntList: "avi-index"];

    [self setupButton: o_input_rtsp_ckb forBoolValue: "rtsp-tcp"];
    [self setupButton: o_input_skipLoop_pop forIntList: "ffmpeg-skiploopfilter"];

    [o_input_cachelevel_pop removeAllItems];
    [o_input_cachelevel_pop addItemsWithTitles:
        [NSArray arrayWithObjects: _NS("Custom"), _NS("Lowest latency"), _NS("Low latency"), _NS("Normal"),
            _NS("High latency"), _NS("Higher latency"), nil]];
    [[o_input_cachelevel_pop itemAtIndex: 0] setTag: 0];
    [[o_input_cachelevel_pop itemAtIndex: 1] setTag: 100];
    [[o_input_cachelevel_pop itemAtIndex: 2] setTag: 200];
    [[o_input_cachelevel_pop itemAtIndex: 3] setTag: 300];
    [[o_input_cachelevel_pop itemAtIndex: 4] setTag: 400];
    [[o_input_cachelevel_pop itemAtIndex: 5] setTag: 500];

#define TestCaC( name ) \
    b_cache_equal =  b_cache_equal && \
        ( i_cache == config_GetInt( p_intf, name ) )

#define TestCaCi( name, int ) \
        b_cache_equal = b_cache_equal &&  \
        ( ( i_cache * int ) == config_GetInt( p_intf, name ) )

    /* Select the accurate value of the PopupButton */
    bool b_cache_equal = true;
    int i_cache = config_GetInt( p_intf, "file-caching");

    TestCaC( "udp-caching" );
    if( module_exists ("dvdread") )
        TestCaC( "dvdread-caching" );
    if( module_exists ("dvdnav") )
        TestCaC( "dvdnav-caching" );
    TestCaC( "tcp-caching" );
    TestCaC( "fake-caching" );
    TestCaC( "cdda-caching" );
    TestCaC( "screen-caching" );
    TestCaC( "vcd-caching" );
    TestCaCi( "rtsp-caching", 4 );
    TestCaCi( "ftp-caching", 2 );
    TestCaCi( "http-caching", 4 );
    if(module_exists ("access_realrtsp"))
        TestCaCi( "realrtsp-caching", 10 );
    TestCaCi( "mms-caching", 19 );
    if( b_cache_equal )
    {
        [o_input_cachelevel_pop selectItemWithTag: i_cache];
        [o_input_cachelevel_custom_txt setHidden: YES];
    }
    else
    {
        [o_input_cachelevel_pop selectItemWithTitle: _NS("Custom")];
        [o_input_cachelevel_custom_txt setHidden: NO];
    }

    /*********************
     * subtitle settings *
     *********************/
    [self setupButton: o_osd_osd_ckb forBoolValue: "osd"];

    [self setupButton: o_osd_encoding_pop forStringList: "subsdec-encoding"];
    [self setupField: o_osd_lang_fld forOption: "sub-language" ];

	if( module_exists( "quartztext" ) )
	{
		[self setupField: o_osd_font_fld forOption: "quartztext-font"];
		[self setupButton: o_osd_font_color_pop forIntList: "quartztext-color"];
		[self setupButton: o_osd_font_size_pop forIntList: "quartztext-rel-fontsize"];
	}
	else
	{
        /* fallback on freetype */
		[self setupField: o_osd_font_fld forOption: "freetype-font"];
		[self setupButton: o_osd_font_color_pop forIntList: "freetype-color"];
		[self setupButton: o_osd_font_size_pop forIntList: "freetype-rel-fontsize"];
		/* selector button is useless in this case */
		[o_osd_font_btn setEnabled: NO];
	}


    /********************
     * hotkeys settings *
     ********************/
    const struct hotkey *p_hotkeys = p_intf->p_libvlc->p_hotkeys;
    [o_hotkeySettings release];
    o_hotkeySettings = [[NSMutableArray alloc] init];
    NSMutableArray *o_tempArray_desc = [[NSMutableArray alloc] init];
    i = 1;

    while( i < 100 )
    {
        p_item = config_FindConfig( VLC_OBJECT(p_intf), p_hotkeys[i].psz_action );
        if( !p_item )
            break;

        [o_tempArray_desc addObject: _NS( p_item->psz_text )];
        [o_hotkeySettings addObject: [NSNumber numberWithInt: p_item->value.i]];

        i++;
    }
    [o_hotkeyDescriptions release];
    o_hotkeyDescriptions = [[NSArray alloc] initWithArray: o_tempArray_desc copyItems: YES];
    [o_tempArray_desc release];
    [o_hotkeys_listbox reloadData];
}

- (void)showSimplePrefs
{
    /* we want to show the interface settings, if no category was chosen */
    if( [[o_sprefs_win toolbar] selectedItemIdentifier] == nil )
    {
        [[o_sprefs_win toolbar] setSelectedItemIdentifier: VLCIntfSettingToolbarIdentifier];
        [self showInterfaceSettings];
    }

    [self resetControls];

    [o_sprefs_win center];
    [o_sprefs_win makeKeyAndOrderFront: self];
}

- (IBAction)buttonAction:(id)sender
{
    if( sender == o_sprefs_cancel_btn )
        [o_sprefs_win orderOut: sender];
    else if( sender == o_sprefs_save_btn )
    {
        [self saveChangedSettings];
        [o_sprefs_win orderOut: sender];
    }
    else if( sender == o_sprefs_reset_btn )
        NSBeginInformationalAlertSheet( _NS("Reset Preferences"), _NS("Cancel"),
                                        _NS("Continue"), nil, o_sprefs_win, self,
                                        @selector(sheetDidEnd: returnCode: contextInfo:), NULL, nil,
                                        _NS("Beware this will reset the VLC media player preferences.\n"
                                            "Are you sure you want to continue?") );
    else if( sender == o_sprefs_basicFull_matrix )
    {
        [o_sprefs_win orderOut: self];
        [[o_sprefs_basicFull_matrix cellAtRow:0 column:0] setState: NSOffState];
        [[o_sprefs_basicFull_matrix cellAtRow:0 column:1] setState: NSOnState];
        [[[VLCMain sharedInstance] preferences] showPrefs];
    }
    else
        msg_Warn( p_intf, "unknown buttonAction sender" );
}

- (void)sheetDidEnd:(NSWindow *)o_sheet
         returnCode:(int)i_return
        contextInfo:(void *)o_context
{
    if( i_return == NSAlertAlternateReturn )
    {
        config_ResetAll( p_intf );
        [self resetControls];
        config_SaveConfigFile( p_intf, NULL );
    }
}

static inline void save_int_list( intf_thread_t * p_intf, id object, const char * name )
{
    NSNumber *p_valueobject;
    module_config_t *p_item;
    p_item = config_FindConfig( VLC_OBJECT(p_intf), name );
    p_valueobject = (NSNumber *)[[object selectedItem] representedObject];
    assert([p_valueobject isKindOfClass:[NSNumber class]]);
    if( p_valueobject) config_PutInt( p_intf, name, [p_valueobject intValue] );
}

static inline void save_string_list( intf_thread_t * p_intf, id object, const char * name )
{
    NSString *p_stringobject;
    module_config_t *p_item;
    p_item = config_FindConfig( VLC_OBJECT(p_intf), name );
    p_stringobject = (NSString *)[[object selectedItem] representedObject];
    assert([p_stringobject isKindOfClass:[NSString class]]);
    if( p_stringobject )
    {
        config_PutPsz( p_intf, name, [p_stringobject UTF8String] );
    }
}

static inline void save_module_list( intf_thread_t * p_intf, id object, const char * name )
{
    module_config_t *p_item;
    module_t *p_parser, **p_list;

    p_item = config_FindConfig( VLC_OBJECT(p_intf), name );

    p_list = module_list_get( NULL );
    for( size_t i_module_index = 0; p_list[i_module_index]; i_module_index++ )
    {
        p_parser = p_list[i_module_index];

        if( p_item->i_type == CONFIG_ITEM_MODULE && module_provides( p_parser, p_item->psz_type ) )
        {
            if( [[[object selectedItem] title] isEqualToString: _NS( module_GetLongName( p_parser ) )] )
            {
                config_PutPsz( p_intf, name, strdup( module_get_object( p_parser )));
                break;
            }
        }
    }
    module_list_free( p_list );
    if( [[[object selectedItem] title] isEqualToString: _NS( "Default" )] )
        config_PutPsz( p_intf, name, "" );
}

- (void)saveChangedSettings
{
    char *psz_tmp;
    int i;

#define SaveIntList( object, name ) save_int_list( p_intf, object, name )

#define SaveStringList( object, name ) save_string_list( p_intf, object, name )

#define SaveModuleList( object, name ) save_module_list( p_intf, object, name )

    /**********************
     * interface settings *
     **********************/
    if( b_intfSettingChanged )
    {
        SaveStringList( o_intf_lang_pop, "language" );
        SaveIntList( o_intf_art_pop, "album-art" );

        config_PutInt( p_intf, "macosx-fspanel", [o_intf_fspanel_ckb state] );
        config_PutInt( p_intf, "embedded-video", [o_intf_embedded_ckb state] );
		config_PutInt( p_intf, "macosx-appleremote", [o_intf_appleremote_ckb state] );
		config_PutInt( p_intf, "macosx-mediakeys", [o_intf_mediakeys_ckb state] );
        config_PutInt( p_intf, "macosx-mediakeys-background", [o_intf_mediakeys_bg_ckb state] );

		/* activate stuff without restart */
		if( [o_intf_appleremote_ckb state] == YES )
			[[[VLCMain sharedInstance] appleRemoteController] startListening: [VLCMain sharedInstance]];
		else
			[[[VLCMain sharedInstance] appleRemoteController] stopListening: [VLCMain sharedInstance]];
        [[NSNotificationCenter defaultCenter] postNotificationName: @"VLCMediaKeySupportSettingChanged"
                                                            object: nil
                                                          userInfo: nil];

        /* okay, let's save our changes to vlcrc */
        i = config_SaveConfigFile( p_intf, "main" );
        i = i + config_SaveConfigFile( p_intf, "macosx" );

        if( i != 0 )
        {
            msg_Err( p_intf, "An error occurred while saving the Interface settings using SimplePrefs (%i)", i );
            dialog_Fatal( p_intf, _("Interface Settings not saved"),
                        _("An error occured while saving your settings via SimplePrefs (%i)."), i );
            i = 0;
        }

        b_intfSettingChanged = NO;
    }

    /******************
     * audio settings *
     ******************/
    if( b_audioSettingChanged )
    {
        config_PutInt( p_intf, "audio", [o_audio_enable_ckb state] );
        config_PutInt( p_intf, "volume", ([o_audio_vol_sld intValue] * 2.56));
        config_PutInt( p_intf, "spdif", [o_audio_spdif_ckb state] );

        SaveIntList( o_audio_dolby_pop, "force-dolby-surround" );

        config_PutPsz( p_intf, "audio-language", [[o_audio_lang_fld stringValue] UTF8String] );
        config_PutInt( p_intf, "headphone-dolby", [o_audio_headphone_ckb state] );

        if( [o_audio_norm_ckb state] == NSOnState )
        {
            psz_tmp = config_GetPsz( p_intf, "audio-filter" );
            if(! psz_tmp)
                config_PutPsz( p_intf, "audio-filter", "volnorm" );
            else if( (NSInteger)strstr( psz_tmp, "normvol" ) == NO )
            {
                /* work-around a GCC 4.0.1 bug */
                psz_tmp = (char *)[[NSString stringWithFormat: @"%s:volnorm", psz_tmp] UTF8String];
                config_PutPsz( p_intf, "audio-filter", psz_tmp );
                free( psz_tmp );
            }
        }
        else
        {
            psz_tmp = config_GetPsz( p_intf, "audio-filter" );
            if( psz_tmp )
            {
                char *psz_tmp2 = (char *)[[[NSString stringWithUTF8String: psz_tmp] stringByTrimmingCharactersInSet: [NSCharacterSet characterSetWithCharactersInString:@":volnorm"]] UTF8String];
                psz_tmp2 = (char *)[[[NSString stringWithUTF8String: psz_tmp2] stringByTrimmingCharactersInSet: [NSCharacterSet characterSetWithCharactersInString:@"volnorm:"]] UTF8String];
                psz_tmp2 = (char *)[[[NSString stringWithUTF8String: psz_tmp2] stringByTrimmingCharactersInSet: [NSCharacterSet characterSetWithCharactersInString:@"volnorm"]] UTF8String];
                config_PutPsz( p_intf, "audio-filter", psz_tmp );
                free( psz_tmp );
            }
        }
        config_PutFloat( p_intf, "norm-max-level", [o_audio_norm_fld floatValue] );

        SaveModuleList( o_audio_visual_pop, "audio-visual" );

        /* Last.FM is optional */
        if( module_exists( "audioscrobbler" ) )
        {
            [o_audio_last_ckb setEnabled: YES];
            if( [o_audio_last_ckb state] == NSOnState )
                config_AddIntf( p_intf, "audioscrobbler" );
            else
                config_RemoveIntf( p_intf, "audioscrobbler" );

            config_PutPsz( p_intf, "lastfm-username", [[o_audio_lastuser_fld stringValue] UTF8String] );
            config_PutPsz( p_intf, "lastfm-password", [[o_audio_lastpwd_sfld stringValue] UTF8String] );
        }
        else
            [o_audio_last_ckb setEnabled: NO];

        /* okay, let's save our changes to vlcrc */
        i = config_SaveConfigFile( p_intf, "main" );
        i = i + config_SaveConfigFile( p_intf, "audioscrobbler" );
        i = i + config_SaveConfigFile( p_intf, "volnorm" );

        if( i != 0 )
        {
            msg_Err( p_intf, "An error occurred while saving the Audio settings using SimplePrefs (%i)", i );
            dialog_Fatal( p_intf, _("Audio Settings not saved"),
                        _("An error occured while saving your settings via SimplePrefs (%i)."), i );

            i = 0;
        }
        b_audioSettingChanged = NO;
    }

    /******************
     * video settings *
     ******************/
    if( b_videoSettingChanged )
    {
        config_PutInt( p_intf, "video", [o_video_enable_ckb state] );
        config_PutInt( p_intf, "fullscreen", [o_video_fullscreen_ckb state] );
        config_PutInt( p_intf, "video-on-top", [o_video_onTop_ckb state] );
        config_PutInt( p_intf, "skip-frames", [o_video_skipFrames_ckb state] );
        config_PutInt( p_intf, "macosx-black", [o_video_black_ckb state] );

        SaveModuleList( o_video_output_pop, "vout" );
        config_PutInt( p_intf, "macosx-vdev", [[o_video_device_pop selectedItem] tag] );

        config_PutPsz( p_intf, "snapshot-path", [[o_video_snap_folder_fld stringValue] UTF8String] );
        config_PutPsz( p_intf, "snapshot-prefix", [[o_video_snap_prefix_fld stringValue] UTF8String] );
        config_PutInt( p_intf, "snapshot-sequential", [o_video_snap_seqnum_ckb state] );
        SaveStringList( o_video_snap_format_pop, "snapshot-format" );

        i = config_SaveConfigFile( p_intf, "main" );
        i = i + config_SaveConfigFile( p_intf, "macosx" );

        if( i != 0 )
        {
            msg_Err( p_intf, "An error occurred while saving the Video settings using SimplePrefs (%i)", i );
            dialog_Fatal( p_intf, _("Video Settings not saved"),
                        _("An error occured while saving your settings via SimplePrefs (%i)."), i );
            i = 0;
        }
        b_videoSettingChanged = NO;
    }

    /***************************
     * input & codecs settings *
     ***************************/
    if( b_inputSettingChanged )
    {
        config_PutInt( p_intf, "server-port", [o_input_serverport_fld intValue] );
        config_PutPsz( p_intf, "http-proxy", [[o_input_httpproxy_fld stringValue] UTF8String] );
        config_PutPsz( p_intf, "http-proxy-pwd", [[o_input_httpproxypwd_sfld stringValue] UTF8String] );
        config_PutInt( p_intf, "postproc-q", [o_input_postproc_fld intValue] );

        SaveIntList( o_input_avi_pop, "avi-index" );

        config_PutInt( p_intf, "rtsp-tcp", [o_input_rtsp_ckb state] );
        SaveIntList( o_input_skipLoop_pop, "ffmpeg-skiploopfilter" );

        #define CaCi( name, int ) config_PutInt( p_intf, name, int * [[o_input_cachelevel_pop selectedItem] tag] )
        #define CaC( name ) CaCi( name, 1 )
        msg_Dbg( p_intf, "Adjusting all cache values to: %i", (int)[[o_input_cachelevel_pop selectedItem] tag] );
        CaC( "udp-caching" );
        if( module_exists ( "dvdread" ) )
        {
            CaC( "dvdread-caching" );
            i = i + config_SaveConfigFile( p_intf, "dvdread" );
        }
        if( module_exists ( "dvdnav" ) )
        {
            CaC( "dvdnav-caching" );
            i = i + config_SaveConfigFile( p_intf, "dvdnav" );
        }
        CaC( "tcp-caching" ); CaC( "vcd-caching" );
        CaC( "fake-caching" ); CaC( "cdda-caching" ); CaC( "file-caching" );
        CaC( "screen-caching" );
        CaCi( "rtsp-caching", 4 ); CaCi( "ftp-caching", 2 );
        CaCi( "http-caching", 4 );
        if( module_exists ( "access_realrtsp" ) )
        {
            CaCi( "realrtsp-caching", 10 );
            i = i + config_SaveConfigFile( p_intf, "access_realrtsp" );
        }
        CaCi( "mms-caching", 19 );

        i = config_SaveConfigFile( p_intf, "main" );
        i = i + config_SaveConfigFile( p_intf, "avcodec" );
        i = i + config_SaveConfigFile( p_intf, "postproc" );
        i = i + config_SaveConfigFile( p_intf, "access_http" );
        i = i + config_SaveConfigFile( p_intf, "access_file" );
        i = i + config_SaveConfigFile( p_intf, "access_tcp" );
        i = i + config_SaveConfigFile( p_intf, "access_fake" );
        i = i + config_SaveConfigFile( p_intf, "cdda" );
        i = i + config_SaveConfigFile( p_intf, "screen" );
        i = i + config_SaveConfigFile( p_intf, "vcd" );
        i = i + config_SaveConfigFile( p_intf, "access_ftp" );
        i = i + config_SaveConfigFile( p_intf, "access_mms" );
        i = i + config_SaveConfigFile( p_intf, "live555" );
        i = i + config_SaveConfigFile( p_intf, "avi" );

        if( i != 0 )
        {
            msg_Err( p_intf, "An error occurred while saving the Input settings using SimplePrefs (%i)", i );
            dialog_Fatal( p_intf, _("Input Settings not saved"),
                        _("An error occured while saving your settings via SimplePrefs (%i)."), i );
            i = 0;
        }
        b_inputSettingChanged = NO;
    }

    /**********************
     * subtitles settings *
     **********************/
    if( b_osdSettingChanged )
    {
        config_PutInt( p_intf, "osd", [o_osd_osd_ckb state] );

        if( [o_osd_encoding_pop indexOfSelectedItem] >= 0 )
            SaveStringList( o_osd_encoding_pop, "subsdec-encoding" );
        else
            config_PutPsz( p_intf, "subsdec-encoding", "" );

        config_PutPsz( p_intf, "sub-language", [[o_osd_lang_fld stringValue] UTF8String] );

		if( module_exists( "quartztext" ) )
		{
			config_PutPsz( p_intf, "quartztext-font", [[o_osd_font_fld stringValue] UTF8String] );
			SaveIntList( o_osd_font_color_pop, "quartztext-color" );
			SaveIntList( o_osd_font_size_pop, "quartztext-rel-fontsize" );
		}
		else
		{
            /* fallback on freetype */
			config_PutPsz( p_intf, "freetype-font", [[o_osd_font_fld stringValue] UTF8String] );
			SaveIntList( o_osd_font_color_pop, "freetype-color" );
			SaveIntList( o_osd_font_size_pop, "freetype-rel-fontsize" );
		}

        i = config_SaveConfigFile( p_intf, NULL );

        if( i != 0 )
        {
            msg_Err( p_intf, "An error occurred while saving the OSD/Subtitle settings using SimplePrefs (%i)", i );
            dialog_Fatal( p_intf, _("On Screen Display/Subtitle Settings not saved"),
                        _("An error occured while saving your settings via SimplePrefs (%i)."), i );
            i = 0;
        }
        b_osdSettingChanged = NO;
    }

    /********************
     * hotkeys settings *
     ********************/
    if( b_hotkeyChanged )
    {
        const struct hotkey *p_hotkeys = p_intf->p_libvlc->p_hotkeys;
        i = 1;
        while( i < [o_hotkeySettings count] )
        {
            config_PutInt( p_intf, p_hotkeys[i].psz_action, [[o_hotkeySettings objectAtIndex: i-1] intValue] );
            i++;
        }

        i = config_SaveConfigFile( p_intf, "main" );

        if( i != 0 )
        {
            msg_Err( p_intf, "An error occurred while saving the Hotkey settings using SimplePrefs (%i)", i );
            dialog_Fatal( p_intf, _("Hotkeys not saved"),
                        _("An error occured while saving your settings via SimplePrefs (%i)."), i );
            i = 0;
        }
        b_hotkeyChanged = NO;
    }
}

- (void)showSettingsForCategory: (id)o_new_category_view
{
    NSRect o_win_rect, o_view_rect, o_old_view_rect;
    o_win_rect = [o_sprefs_win frame];
    o_view_rect = [o_new_category_view frame];

    if( o_currentlyShownCategoryView != nil )
    {
        /* restore our window's height, if we've shown another category previously */
        o_old_view_rect = [o_currentlyShownCategoryView frame];
        o_win_rect.size.height = o_win_rect.size.height - o_old_view_rect.size.height;
        o_win_rect.origin.y = ( o_win_rect.origin.y + o_old_view_rect.size.height ) - o_view_rect.size.height;

        /* remove our previous category view */
        [o_currentlyShownCategoryView removeFromSuperviewWithoutNeedingDisplay];
    }

    o_win_rect.size.height = o_win_rect.size.height + o_view_rect.size.height;

    [o_sprefs_win displayIfNeeded];
    [o_sprefs_win setFrame: o_win_rect display:YES animate: YES];

    [o_new_category_view setFrame: NSMakeRect( 0,
                                               [o_sprefs_controls_box frame].size.height,
                                               o_view_rect.size.width,
                                               o_view_rect.size.height )];
    [o_new_category_view setNeedsDisplay: YES];
    [o_new_category_view setAutoresizesSubviews: YES];
    [[o_sprefs_win contentView] addSubview: o_new_category_view];

    /* keep our current category for further reference */
    [o_currentlyShownCategoryView release];
    o_currentlyShownCategoryView = o_new_category_view;
    [o_currentlyShownCategoryView retain];
}

- (IBAction)interfaceSettingChanged:(id)sender
{
    if( sender == o_intf_mediakeys_ckb )
        [o_intf_mediakeys_bg_ckb setEnabled: [o_intf_mediakeys_ckb state]];
    b_intfSettingChanged = YES;
}

- (void)showInterfaceSettings
{
    [self showSettingsForCategory: o_intf_view];
}

- (IBAction)audioSettingChanged:(id)sender
{
    if( sender == o_audio_vol_sld )
        [o_audio_vol_fld setIntValue: [o_audio_vol_sld intValue]];

    if( sender == o_audio_vol_fld )
        [o_audio_vol_sld setIntValue: [o_audio_vol_fld intValue]];

    if( sender == o_audio_norm_ckb )
    {
        [o_audio_norm_stepper setEnabled: [o_audio_norm_ckb state]];
        [o_audio_norm_fld setEnabled: [o_audio_norm_ckb state]];
    }

    if( sender == o_audio_last_ckb )
    {
        if( [o_audio_last_ckb state] == NSOnState )
        {
            [o_audio_lastpwd_sfld setEnabled: YES];
            [o_audio_lastuser_fld setEnabled: YES];
        }
        else
        {
            [o_audio_lastpwd_sfld setEnabled: NO];
            [o_audio_lastuser_fld setEnabled: NO];
        }
    }

    b_audioSettingChanged = YES;
}

- (void)showAudioSettings
{
    [self showSettingsForCategory: o_audio_view];
}

- (IBAction)videoSettingChanged:(id)sender
{
    if( sender == o_video_snap_folder_btn )
    {
        o_selectFolderPanel = [[NSOpenPanel alloc] init];
        [o_selectFolderPanel setCanChooseDirectories: YES];
        [o_selectFolderPanel setCanChooseFiles: NO];
        [o_selectFolderPanel setResolvesAliases: YES];
        [o_selectFolderPanel setAllowsMultipleSelection: NO];
        [o_selectFolderPanel setMessage: _NS("Choose the folder to save your video snapshots to.")];
        [o_selectFolderPanel setCanCreateDirectories: YES];
        [o_selectFolderPanel setPrompt: _NS("Choose")];
        [o_selectFolderPanel beginSheetForDirectory: nil file: nil modalForWindow: o_sprefs_win
                                      modalDelegate: self
                                     didEndSelector: @selector(savePanelDidEnd:returnCode:contextInfo:)
                                        contextInfo: o_video_snap_folder_btn];
    }
    else
        b_videoSettingChanged = YES;
}

- (void)savePanelDidEnd:(NSOpenPanel * )panel returnCode: (int)returnCode contextInfo: (void *)contextInfo
{
    if( returnCode == NSOKButton )
    {
        if( contextInfo == o_video_snap_folder_btn )
        {
            [o_video_snap_folder_fld setStringValue: [o_selectFolderPanel filename]];
            b_videoSettingChanged = YES;
        }
    }

    [o_selectFolderPanel release];
}

- (void)showVideoSettings
{
    [self showSettingsForCategory: o_video_view];
}

- (IBAction)osdSettingChanged:(id)sender
{
    b_osdSettingChanged = YES;
}

- (void)showOSDSettings
{
    [self showSettingsForCategory: o_osd_view];
}

- (IBAction)showFontPicker:(id)sender
{
	if( module_exists( "quartztext" ) )
	{
		char * font = config_GetPsz( p_intf, "quartztext-font" );
		NSString * fontFamilyName = font ? [NSString stringWithUTF8String: font] : nil;
		free(font);
		if( fontFamilyName )
		{
			NSFontDescriptor * fd = [NSFontDescriptor fontDescriptorWithFontAttributes:nil];
			NSFont * font = [NSFont fontWithDescriptor:[fd fontDescriptorWithFamily:fontFamilyName] textTransform:nil];
			[[NSFontManager sharedFontManager] setSelectedFont:font isMultiple:NO];
		}
		[[NSFontManager sharedFontManager] setTarget: self];
		[[NSFontPanel sharedFontPanel] orderFront:self];
	}
}

- (void)changeFont:(id)sender
{
    NSFont * font = [sender convertFont:[[NSFontManager sharedFontManager] selectedFont]];
    [o_osd_font_fld setStringValue:[font familyName]];
    [self osdSettingChanged:self];
}

- (IBAction)inputSettingChanged:(id)sender
{
    if( sender == o_input_cachelevel_pop )
    {
        if( [[[o_input_cachelevel_pop selectedItem] title] isEqualToString: _NS("Custom")] )
            [o_input_cachelevel_custom_txt setHidden: NO];
        else
            [o_input_cachelevel_custom_txt setHidden: YES];
    }

    b_inputSettingChanged = YES;
}

- (void)showInputSettings
{
    [self showSettingsForCategory: o_input_view];
}

- (IBAction)hotkeySettingChanged:(id)sender
{
    if( sender == o_hotkeys_change_btn || sender == o_hotkeys_listbox )
    {
        [o_hotkeys_change_lbl setStringValue: [NSString stringWithFormat: _NS("Press new keys for\n\"%@\""),
                                               [o_hotkeyDescriptions objectAtIndex: [o_hotkeys_listbox selectedRow]]]];
        [o_hotkeys_change_keys_lbl setStringValue: [self OSXKeyToString:[[o_hotkeySettings objectAtIndex: [o_hotkeys_listbox selectedRow]] intValue]]];
        [o_hotkeys_change_taken_lbl setStringValue: @""];
        [o_hotkeys_change_win setInitialFirstResponder: [o_hotkeys_change_win contentView]];
        [o_hotkeys_change_win makeFirstResponder: [o_hotkeys_change_win contentView]];
        [NSApp runModalForWindow: o_hotkeys_change_win];
    }
    else if( sender == o_hotkeys_change_cancel_btn )
    {
        [NSApp stopModal];
        [o_hotkeys_change_win close];
    }
    else if( sender == o_hotkeys_change_ok_btn )
    {
        NSInteger i_returnValue;
        if(! o_keyInTransition )
        {
            [NSApp stopModal];
            [o_hotkeys_change_win close];
            msg_Err( p_intf, "internal error prevented the hotkey switch" );
            return;
        }

        b_hotkeyChanged = YES;

        i_returnValue = [o_hotkeySettings indexOfObject: o_keyInTransition];
        if( i_returnValue != NSNotFound )
            [o_hotkeySettings replaceObjectAtIndex: i_returnValue withObject: [[NSNumber numberWithInt: 0] retain]];

        [o_hotkeySettings replaceObjectAtIndex: [o_hotkeys_listbox selectedRow] withObject: [o_keyInTransition retain]];

        [NSApp stopModal];
        [o_hotkeys_change_win close];

        [o_hotkeys_listbox reloadData];
    }
    else if( sender == o_hotkeys_clear_btn )
    {
        [o_hotkeySettings replaceObjectAtIndex: [o_hotkeys_listbox selectedRow] withObject: [NSNumber numberWithInt: 0]];
        [o_hotkeys_listbox reloadData];
        b_hotkeyChanged = YES;
    }
}

- (void)showHotkeySettings
{
    [self showSettingsForCategory: o_hotkeys_view];
}

- (int)numberOfRowsInTableView:(NSTableView *)aTableView
{
    return [o_hotkeySettings count];
}

- (id)tableView:(NSTableView *)aTableView objectValueForTableColumn:(NSTableColumn *)aTableColumn row:(int)rowIndex
{
    if( [[aTableColumn identifier] isEqualToString: @"action"] )
        return [o_hotkeyDescriptions objectAtIndex: rowIndex];
    else if( [[aTableColumn identifier] isEqualToString: @"shortcut"] )
        return [self OSXKeyToString: [[o_hotkeySettings objectAtIndex: rowIndex] intValue]];
    else
    {
        msg_Err( p_intf, "unknown TableColumn identifier (%s)!", [[aTableColumn identifier] UTF8String] );
        return NULL;
    }
}

- (BOOL)changeHotkeyTo: (int)i_theNewKey
{
    NSInteger i_returnValue;
    i_returnValue = [o_hotkeysNonUseableKeys indexOfObject: [NSNumber numberWithInt: i_theNewKey]];
    if( i_returnValue != NSNotFound || i_theNewKey == 0 )
    {
        [o_hotkeys_change_keys_lbl setStringValue: _NS("Invalid combination")];
        [o_hotkeys_change_taken_lbl setStringValue: _NS("Regrettably, these keys cannot be assigned as hotkey shortcuts.")];
        [o_hotkeys_change_ok_btn setEnabled: NO];
        return NO;
    }
    else
    {
        NSString *o_temp;
        if( o_keyInTransition )
            [o_keyInTransition release];
        o_keyInTransition = [[NSNumber numberWithInt: i_theNewKey] retain];

        o_temp = [self OSXKeyToString: i_theNewKey];

        [o_hotkeys_change_keys_lbl setStringValue: o_temp];

        i_returnValue = [o_hotkeySettings indexOfObject: o_keyInTransition];
        if( i_returnValue != NSNotFound )
            [o_hotkeys_change_taken_lbl setStringValue: [NSString stringWithFormat:
                                                         _NS("This combination is already taken by \"%@\"."),
                                                         [o_hotkeyDescriptions objectAtIndex: i_returnValue]]];
        else
            [o_hotkeys_change_taken_lbl setStringValue: @""];

        [o_hotkeys_change_ok_btn setEnabled: YES];
        return YES;
    }
}

@end

/********************
 * hotkeys settings *
 ********************/

@implementation VLCHotkeyChangeWindow

- (BOOL)acceptsFirstResponder
{
    return YES;
}

- (BOOL)becomeFirstResponder
{
    return YES;
}

- (BOOL)resignFirstResponder
{
    /* We need to stay the first responder or we'll miss the user's input */
    return NO;
}

- (BOOL)performKeyEquivalent:(NSEvent *)o_theEvent
{
    unichar key;
    int i_key = 0;

    if( [o_theEvent modifierFlags] & NSControlKeyMask )
        i_key |= KEY_MODIFIER_CTRL;

    if( [o_theEvent modifierFlags] & NSAlternateKeyMask  )
        i_key |= KEY_MODIFIER_ALT;

    if( [o_theEvent modifierFlags] & NSShiftKeyMask )
        i_key |= KEY_MODIFIER_SHIFT;

    if( [o_theEvent modifierFlags] & NSCommandKeyMask )
        i_key |= KEY_MODIFIER_COMMAND;

    key = [[[o_theEvent charactersIgnoringModifiers] lowercaseString] characterAtIndex: 0];
    if( key )
    {
        i_key |= CocoaKeyToVLC( key );
        return [[[VLCMain sharedInstance] simplePreferences] changeHotkeyTo: i_key];
    }
    return FALSE;
}

@end

@implementation VLCSimplePrefsWindow

- (BOOL)acceptsFirstResponder
{
    return YES;
}

- (void)changeFont:(id)sender
{
    [[[VLCMain sharedInstance] simplePreferences] changeFont: sender];
}
@end
