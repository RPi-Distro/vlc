/*****************************************************************************
 * intf.h: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2002-2014 VLC authors and VideoLAN
 * $Id: 1b25621f237eedecea521cc3dab2dd06378b0d4b $
 *
 * Authors: Jon Lech Johansen <jon-vl@nanocrew.net>
 *          Derk-Jan Hartman <hartman at videolan dot org>
 *          Felix Paul Kühne <fkuehne at videolan dot org>
 *          David Fuhrmann <david dot fuhrmann at googlemail dot com>
 *          Pierre d'Herbemont <pdherbemont # videolan org>
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

#ifdef HAVE_CONFIG_H
#   include "config.h"
#endif

#import <vlc_common.h>
#import <vlc_interface.h>
#import <vlc_playlist.h>
#import <vlc_vout.h>
#import <vlc_aout.h>
#import <vlc_input.h>
#import <vlc_vout_window.h>

#import <Cocoa/Cocoa.h>
#import "SPMediaKeyTap.h"                   /* for the media key support */
#import "misc.h"
#import "MainWindow.h"
#import "VLCVoutWindowController.h"
#import "StringUtility.h"

#import <IOKit/pwr_mgt/IOPMLib.h>           /* for sleep prevention */

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
#define VLCIntf [[VLCMain sharedInstance] intf]

// You need to release those objects after use
input_thread_t *getInput(void);
vout_thread_t *getVout(void);
vout_thread_t *getVoutForActiveWindow(void);
audio_output_t *getAout(void);

static NSString * VLCInputChangedNotification = @"VLCInputChangedNotification";

/*****************************************************************************
 * VLCMain interface
 *****************************************************************************/
@class AppleRemote;
@class VLCControls;
@class VLCPlaylist;
@class ResumeDialogController;

@interface VLCMain : NSObject <NSWindowDelegate, NSApplicationDelegate>
{
    intf_thread_t *p_intf;      /* The main intf object */
    input_thread_t *p_current_input;
    BOOL launched;              /* finishedLaunching */
    int items_at_launch;        /* items in playlist after launch */
    id o_mainmenu;              /* VLCMainMenu */
    id o_prefs;                 /* VLCPrefs       */
    id o_sprefs;                /* VLCSimplePrefs */
    id o_open;                  /* VLCOpen        */
    id o_wizard;                /* VLCWizard      */
    id o_coredialogs;           /* VLCCoreDialogProvider */
    id o_info;                  /* VLCInformation */
    id o_eyetv;                 /* VLCEyeTVController */
    id o_bookmarks;             /* VLCBookmarks */
    id o_coreinteraction;       /* VLCCoreInteraction */
    ResumeDialogController *o_resume_dialog;

    BOOL nib_main_loaded;       /* main nibfile */
    BOOL nib_open_loaded;       /* open nibfile */
    BOOL nib_about_loaded;      /* about nibfile */
    BOOL nib_wizard_loaded;     /* wizard nibfile */
    BOOL nib_prefs_loaded;      /* preferences nibfile */
    BOOL nib_info_loaded;       /* information panel nibfile */
    BOOL nib_coredialogs_loaded; /* CoreDialogs nibfile */
    BOOL nib_bookmarks_loaded;   /* Bookmarks nibfile */
    BOOL b_active_videoplayback;
    BOOL b_nativeFullscreenMode;

    IBOutlet VLCMainWindow *o_mainwindow;            /* VLCMainWindow */

    IBOutlet VLCControls * o_controls;     /* VLCControls    */
    IBOutlet VLCPlaylist * o_playlist;     /* VLCPlaylist    */

    AppleRemote * o_remote;
    BOOL b_remote_button_hold; /* true as long as the user holds the left,right,plus or minus on the remote control */

    /* media key support */
    BOOL b_mediaKeySupport;
    BOOL b_mediakeyJustJumped;
    SPMediaKeyTap * o_mediaKeyController;
    BOOL b_mediaKeyTrapEnabled;

    NSArray *o_usedHotkeys;

    /* sleep management */
    IOPMAssertionID systemSleepAssertionID;
    IOPMAssertionID userActivityAssertionID;

    VLCVoutWindowController *o_vout_controller;

    /* iTunes/Spotify play/pause support */
    BOOL b_has_itunes_paused;
    BOOL b_has_spotify_paused;
    NSTimer *o_itunes_play_timer;

    BOOL b_playlist_updated_selector_in_queue;

    dispatch_queue_t informInputChangedQueue;
}

@property (readonly) VLCVoutWindowController* voutController;
@property (readonly) BOOL nativeFullscreenMode;
@property (nonatomic, readwrite) BOOL playlistUpdatedSelectorInQueue;
+ (VLCMain *)sharedInstance;

- (intf_thread_t *)intf;
- (void)setIntf:(intf_thread_t *)p_mainintf;

- (id)mainMenu;
- (VLCMainWindow *)mainWindow;
- (id)controls;
- (id)bookmarks;
- (id)open;
- (id)simplePreferences;
- (id)preferences;
- (id)playlist;
- (id)info;
- (id)wizard;
- (id)coreDialogProvider;
- (ResumeDialogController *)resumeDialog;
- (id)eyeTVController;
- (id)appleRemoteController;
- (void)setActiveVideoPlayback:(BOOL)b_value;
- (BOOL)activeVideoPlayback;
- (void)applicationWillTerminate:(NSNotification *)notification;
- (void)updateCurrentlyUsedHotkeys;
- (BOOL)hasDefinedShortcutKey:(NSEvent *)o_event force:(BOOL)b_force;

- (void)PlaylistItemChanged;
- (void)plItemUpdated;
- (void)playbackStatusUpdated;
- (void)sendDistributedNotificationWithUpdatedPlaybackStatus;
- (void)playbackModeUpdated;
- (void)updateVolume;
- (void)updatePlaybackPosition;
- (void)updateName;
- (void)playlistUpdated;
- (void)updateRecordState: (BOOL)b_value;
- (void)updateInfoandMetaPanel;
- (void)updateMainMenu;
- (void)updateMainWindow;
- (void)showMainWindow;
- (void)showFullscreenController;
- (void)updateDelays;

- (void)updateTogglePlaylistState;

- (void)mediaKeyTap:(SPMediaKeyTap*)keyTap receivedMediaKeyEvent:(NSEvent*)event;

- (void)resetAndReinitializeUserDefaults;

- (BOOL)isTerminating;

@end


/*****************************************************************************
 * VLCApplication interface
 *****************************************************************************/

@interface VLCApplication : NSApplication

@end
