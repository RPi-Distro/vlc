/*****************************************************************************
 * intf.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2002-2012 VLC authors and VideoLAN
 * $Id: 654c554291a2918a9a81517bd26dd98563b046c6 $
 *
 * Authors: Jon Lech Johansen <jon-vl@nanocrew.net>
 *          Christophe Massiot <massiot@via.ecp.fr>
 *          Derk-Jan Hartman <hartman at videolan.org>
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdlib.h>                                      /* malloc(), free() */
#include <sys/param.h>                                    /* for MAXPATHLEN */
#include <string.h>
#include <vlc_common.h>
#include <vlc_keys.h>
#include <vlc_dialog.h>
#include <vlc_url.h>
#include <vlc_modules.h>
#include <vlc_aout_intf.h>
#include <vlc_vout_window.h>
#include <vlc_vout_display.h>
#include <unistd.h> /* execl() */

#import "CompatibilityFixes.h"
#import "intf.h"
#import "MainMenu.h"
#import "VideoView.h"
#import "prefs.h"
#import "playlist.h"
#import "playlistinfo.h"
#import "controls.h"
#import "open.h"
#import "wizard.h"
#import "bookmarks.h"
#import "coredialogs.h"
#import "AppleRemote.h"
#import "eyetv.h"
#import "simple_prefs.h"
#import "CoreInteraction.h"
#import "TrackSynchronization.h"

#import <AddressBook/AddressBook.h>         /* for crashlog send mechanism */
#import <Sparkle/Sparkle.h>                 /* we're the update delegate */

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
static void Run ( intf_thread_t *p_intf );

static void updateProgressPanel (void *, const char *, float);
static bool checkProgressPanel (void *);
static void destroyProgressPanel (void *);

static void MsgCallback( void *data, int type, const msg_item_t *item, const char *format, va_list ap );

static int InputEvent( vlc_object_t *, const char *,
                      vlc_value_t, vlc_value_t, void * );
static int PLItemChanged( vlc_object_t *, const char *,
                         vlc_value_t, vlc_value_t, void * );
static int PlaylistUpdated( vlc_object_t *, const char *,
                           vlc_value_t, vlc_value_t, void * );
static int PlaybackModeUpdated( vlc_object_t *, const char *,
                               vlc_value_t, vlc_value_t, void * );
static int VolumeUpdated( vlc_object_t *, const char *,
                         vlc_value_t, vlc_value_t, void * );

#pragma mark -
#pragma mark VLC Interface Object Callbacks

/*****************************************************************************
 * OpenIntf: initialize interface
 *****************************************************************************/
int OpenIntf ( vlc_object_t *p_this )
{
    NSAutoreleasePool *o_pool = [[NSAutoreleasePool alloc] init];
    [VLCApplication sharedApplication];
    intf_thread_t *p_intf = (intf_thread_t*) p_this;

    p_intf->p_sys = malloc( sizeof( intf_sys_t ) );
    if( p_intf->p_sys == NULL )
        return VLC_ENOMEM;

    memset( p_intf->p_sys, 0, sizeof( *p_intf->p_sys ) );

    /* subscribe to LibVLCCore's messages */
    p_intf->p_sys->p_sub = vlc_Subscribe( MsgCallback, NULL );
    p_intf->pf_run = Run;
    p_intf->b_should_run_on_first_thread = true;

    [o_pool release];
    return VLC_SUCCESS;
}

/*****************************************************************************
 * CloseIntf: destroy interface
 *****************************************************************************/
void CloseIntf ( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t*) p_this;

    free( p_intf->p_sys );
}

static int WindowControl( vout_window_t *, int i_query, va_list );

int WindowOpen( vout_window_t *p_wnd, const vout_window_cfg_t *cfg )
{
    NSAutoreleasePool *o_pool = [[NSAutoreleasePool alloc] init];
    intf_thread_t *p_intf = VLCIntf;
    if (!p_intf) {
        msg_Err( p_wnd, "Mac OS X interface not found" );
        return VLC_EGENERIC;
    }

    int i_x = cfg->x;
    int i_y = cfg->y;
    unsigned i_width = cfg->width;
    unsigned i_height = cfg->height;
    p_wnd->handle.nsobject = [[VLCMain sharedInstance] getVideoViewAtPositionX: &i_x Y: &i_y withWidth: &i_width andHeight: &i_height];

    if ( !p_wnd->handle.nsobject ) {
        msg_Err( p_wnd, "got no video view from the interface" );
        [o_pool release];
        return VLC_EGENERIC;
    }

    [[VLCMain sharedInstance] setNativeVideoSize:NSMakeSize( cfg->width, cfg->height )];
    [[VLCMain sharedInstance] setActiveVideoPlayback: YES];
    p_wnd->control = WindowControl;
    p_wnd->sys = (vout_window_sys_t *)VLCIntf;
    [o_pool release];
    return VLC_SUCCESS;
}

static int WindowControl( vout_window_t *p_wnd, int i_query, va_list args )
{
    switch( i_query )
    {
        case VOUT_WINDOW_SET_STATE:
        {
            unsigned i_state = va_arg( args, unsigned );
            [[VLCMain sharedInstance] performSelectorOnMainThread:@selector(setWindowLevel:) withObject:[NSNumber numberWithUnsignedInt:i_state] waitUntilDone:NO];
            return VLC_SUCCESS;
        }
        case VOUT_WINDOW_SET_SIZE:
        {
            unsigned int i_width  = va_arg( args, unsigned int );
            unsigned int i_height = va_arg( args, unsigned int );
            [[VLCMain sharedInstance] setNativeVideoSize:NSMakeSize( i_width, i_height )];
            return VLC_SUCCESS;
        }
        case VOUT_WINDOW_SET_FULLSCREEN:
        {
            NSAutoreleasePool *o_pool = [[NSAutoreleasePool alloc] init];
            int i_full = va_arg( args, int );
            [[VLCMain sharedInstance] performSelectorOnMainThread:@selector(checkFullscreenChange:) withObject:[NSNumber numberWithInt: i_full] waitUntilDone:NO];
            [o_pool release];
            return VLC_SUCCESS;
        }
        default:
            msg_Warn( p_wnd, "unsupported control query" );
            return VLC_EGENERIC;
    }
}

void WindowClose( vout_window_t *p_wnd )
{
    NSAutoreleasePool *o_pool = [[NSAutoreleasePool alloc] init];
    [[VLCMain sharedInstance] setActiveVideoPlayback:NO];

    [o_pool release];
}

/*****************************************************************************
 * Run: main loop
 *****************************************************************************/
static NSLock * o_appLock = nil;    // controls access to f_appExit

static void Run( intf_thread_t *p_intf )
{
    NSAutoreleasePool * o_pool = [[NSAutoreleasePool alloc] init];
    [VLCApplication sharedApplication];

    o_appLock = [[NSLock alloc] init];

    [[VLCMain sharedInstance] setIntf: p_intf];
    [NSBundle loadNibNamed: @"MainMenu" owner: NSApp];

    [NSApp run];
    [[VLCMain sharedInstance] applicationWillTerminate:nil];
    [o_appLock release];
    [o_pool release];
}

#pragma mark -
#pragma mark Variables Callback

/*****************************************************************************
 * MsgCallback: Callback triggered by the core once a new debug message is
 * ready to be displayed. We store everything in a NSArray in our Cocoa part
 * of this file.
 *****************************************************************************/
static void MsgCallback( void *data, int type, const msg_item_t *item, const char *format, va_list ap )
{
    int canc = vlc_savecancel();
    char *str;

    if (vasprintf( &str, format, ap ) == -1)
    {
        vlc_restorecancel( canc );
        return;
    }

    NSAutoreleasePool *o_pool = [[NSAutoreleasePool alloc] init];
    [[VLCMain sharedInstance] processReceivedlibvlcMessage: item ofType: type withStr: str];
    [o_pool release];

    vlc_restorecancel( canc );
    free( str );
}

static int InputEvent( vlc_object_t *p_this, const char *psz_var,
                       vlc_value_t oldval, vlc_value_t new_val, void *param )
{
    NSAutoreleasePool *o_pool = [[NSAutoreleasePool alloc] init];
    switch (new_val.i_int) {
        case INPUT_EVENT_STATE:
            [[VLCMain sharedInstance] performSelectorOnMainThread:@selector(playbackStatusUpdated) withObject: nil waitUntilDone:NO];
            break;
        case INPUT_EVENT_RATE:
            [[[VLCMain sharedInstance] mainMenu] performSelectorOnMainThread:@selector(updatePlaybackRate) withObject: nil waitUntilDone:NO];
            break;
        case INPUT_EVENT_POSITION:
            [[VLCMain sharedInstance] performSelectorOnMainThread:@selector(updatePlaybackPosition) withObject: nil waitUntilDone:NO];
            break;
        case INPUT_EVENT_TITLE:
        case INPUT_EVENT_CHAPTER:
            [[VLCMain sharedInstance] performSelectorOnMainThread:@selector(updateMainMenu) withObject: nil waitUntilDone:NO];
            break;
        case INPUT_EVENT_CACHE:
            [[VLCMain sharedInstance] performSelectorOnMainThread:@selector(updateMainWindow) withObject: nil waitUntilDone: NO];
            break;
        case INPUT_EVENT_STATISTICS:
            [[[VLCMain sharedInstance] info] performSelectorOnMainThread:@selector(updateStatistics) withObject: nil waitUntilDone: NO];
            break;
        case INPUT_EVENT_ES:
            break;
        case INPUT_EVENT_TELETEXT:
            break;
        case INPUT_EVENT_AOUT:
            break;
        case INPUT_EVENT_VOUT:
            break;
        case INPUT_EVENT_ITEM_META:
        case INPUT_EVENT_ITEM_INFO:
            [[VLCMain sharedInstance] performSelectorOnMainThread:@selector(updateMainMenu) withObject: nil waitUntilDone:NO];
            [[VLCMain sharedInstance] performSelectorOnMainThread:@selector(updateName) withObject: nil waitUntilDone:NO];
            [[VLCMain sharedInstance] performSelectorOnMainThread:@selector(updateInfoandMetaPanel) withObject: nil waitUntilDone:NO];
            break;
        case INPUT_EVENT_BOOKMARK:
            break;
        case INPUT_EVENT_RECORD:
            [[VLCMain sharedInstance] updateRecordState: var_GetBool( p_this, "record" )];
            break;
        case INPUT_EVENT_PROGRAM:
            [[VLCMain sharedInstance] performSelectorOnMainThread:@selector(updateMainMenu) withObject: nil waitUntilDone:NO];
            break;
        case INPUT_EVENT_ITEM_EPG:
            break;
        case INPUT_EVENT_SIGNAL:
            break;

        case INPUT_EVENT_ITEM_NAME:
            [[VLCMain sharedInstance] performSelectorOnMainThread:@selector(updateName) withObject: nil waitUntilDone:NO];
            [[VLCMain sharedInstance] performSelectorOnMainThread:@selector(playlistUpdated) withObject: nil waitUntilDone:NO];
            break;

        case INPUT_EVENT_AUDIO_DELAY:
        case INPUT_EVENT_SUBTITLE_DELAY:
            [[VLCMain sharedInstance] performSelectorOnMainThread:@selector(updateDelays) withObject:nil waitUntilDone:NO];
            break;

        case INPUT_EVENT_DEAD:
            [[VLCMain sharedInstance] performSelectorOnMainThread:@selector(updateName) withObject: nil waitUntilDone:NO];
            [[VLCMain sharedInstance] performSelectorOnMainThread:@selector(updatePlaybackPosition) withObject:nil waitUntilDone:NO];
            break;

        case INPUT_EVENT_ABORT:
            [[VLCMain sharedInstance] performSelectorOnMainThread:@selector(updateName) withObject: nil waitUntilDone:NO];
            [[VLCMain sharedInstance] performSelectorOnMainThread:@selector(updatePlaybackPosition) withObject:nil waitUntilDone:NO];
            break;

        default:
            //msg_Warn( p_this, "unhandled input event (%lld)", new_val.i_int );
            break;
    }

    [o_pool release];
    return VLC_SUCCESS;
}

static int PLItemChanged( vlc_object_t *p_this, const char *psz_var,
                         vlc_value_t oldval, vlc_value_t new_val, void *param )
{
    NSAutoreleasePool * o_pool = [[NSAutoreleasePool alloc] init];
    [[VLCMain sharedInstance] performSelectorOnMainThread:@selector(PlaylistItemChanged) withObject:nil waitUntilDone:NO];

    [o_pool release];
    return VLC_SUCCESS;
}

static int PlaylistUpdated( vlc_object_t *p_this, const char *psz_var,
                         vlc_value_t oldval, vlc_value_t new_val, void *param )
{
    NSAutoreleasePool * o_pool = [[NSAutoreleasePool alloc] init];
    [[VLCMain sharedInstance] performSelectorOnMainThread:@selector(playlistUpdated) withObject:nil waitUntilDone:NO];

    [o_pool release];
    return VLC_SUCCESS;
}

static int PlaybackModeUpdated( vlc_object_t *p_this, const char *psz_var,
                         vlc_value_t oldval, vlc_value_t new_val, void *param )
{
    NSAutoreleasePool * o_pool = [[NSAutoreleasePool alloc] init];
    [[VLCMain sharedInstance] performSelectorOnMainThread:@selector(playbackModeUpdated) withObject:nil waitUntilDone:NO];

    [o_pool release];
    return VLC_SUCCESS;
}

static int VolumeUpdated( vlc_object_t *p_this, const char *psz_var,
                         vlc_value_t oldval, vlc_value_t new_val, void *param )
{
    NSAutoreleasePool * o_pool = [[NSAutoreleasePool alloc] init];
    [[VLCMain sharedInstance] performSelectorOnMainThread:@selector(updateVolume) withObject:nil waitUntilDone:NO];

    [o_pool release];
    return VLC_SUCCESS;
}

/*****************************************************************************
 * ShowController: Callback triggered by the show-intf playlist variable
 * through the ShowIntf-control-intf, to let us show the controller-win;
 * usually when in fullscreen-mode
 *****************************************************************************/
static int ShowController( vlc_object_t *p_this, const char *psz_variable,
                     vlc_value_t old_val, vlc_value_t new_val, void *param )
{
    intf_thread_t * p_intf = VLCIntf;
    if( p_intf && p_intf->p_sys )
    {
        playlist_t * p_playlist = pl_Get( p_intf );
        BOOL b_fullscreen = var_GetBool( p_playlist, "fullscreen" );
        if( strcmp(psz_variable, "intf-toggle-fscontrol") || b_fullscreen )
        {
            [[VLCMain sharedInstance] performSelectorOnMainThread:@selector(showFullscreenController) withObject:nil waitUntilDone:NO];
        }
        else
        {
            [[VLCMain sharedInstance] performSelectorOnMainThread:@selector(showMainWindow) withObject:nil waitUntilDone:NO];
        }
    }
    return VLC_SUCCESS;
}

/*****************************************************************************
 * FullscreenChanged: Callback triggered by the fullscreen-change playlist
 * variable, to let the intf update the controller.
 *****************************************************************************/
static int FullscreenChanged( vlc_object_t *p_this, const char *psz_variable,
                     vlc_value_t old_val, vlc_value_t new_val, void *param )
{
    intf_thread_t * p_intf = VLCIntf;
    if (p_intf)
    {
        NSAutoreleasePool *o_pool = [[NSAutoreleasePool alloc] init];
        [[VLCMain sharedInstance] performSelectorOnMainThread:@selector(fullscreenChanged) withObject:nil waitUntilDone:NO];
        [o_pool release];
    }
    return VLC_SUCCESS;
}

/*****************************************************************************
 * DialogCallback: Callback triggered by the "dialog-*" variables
 * to let the intf display error and interaction dialogs
 *****************************************************************************/
static int DialogCallback( vlc_object_t *p_this, const char *type, vlc_value_t previous, vlc_value_t value, void *data )
{
    NSAutoreleasePool * o_pool = [[NSAutoreleasePool alloc] init];
    VLCMain *interface = (VLCMain *)data;

    if( [[NSString stringWithUTF8String: type] isEqualToString: @"dialog-progress-bar"] )
    {
        /* the progress panel needs to update itself and therefore wants special treatment within this context */
        dialog_progress_bar_t *p_dialog = (dialog_progress_bar_t *)value.p_address;

        p_dialog->pf_update = updateProgressPanel;
        p_dialog->pf_check = checkProgressPanel;
        p_dialog->pf_destroy = destroyProgressPanel;
        p_dialog->p_sys = VLCIntf->p_libvlc;
    }

    NSValue *o_value = [NSValue valueWithPointer:value.p_address];
    [[VLCCoreDialogProvider sharedInstance] performEventWithObject: o_value ofType: type];

    [o_pool release];
    return VLC_SUCCESS;
}

void updateProgressPanel (void *priv, const char *text, float value)
{
    NSAutoreleasePool *o_pool = [[NSAutoreleasePool alloc] init];

    NSString *o_txt;
    if( text != NULL )
        o_txt = [NSString stringWithUTF8String: text];
    else
        o_txt = @"";

    [[[VLCMain sharedInstance] coreDialogProvider] updateProgressPanelWithText: o_txt andNumber: (double)(value * 1000.)];

    [o_pool release];
}

void destroyProgressPanel (void *priv)
{
    NSAutoreleasePool *o_pool = [[NSAutoreleasePool alloc] init];

    if ([[NSApplication sharedApplication] isRunning])
        [[[VLCMain sharedInstance] coreDialogProvider] performSelectorOnMainThread:@selector(destroyProgressPanel) withObject:nil waitUntilDone:YES];

    [o_pool release];
}

bool checkProgressPanel (void *priv)
{
    NSAutoreleasePool *o_pool = [[NSAutoreleasePool alloc] init];
    return [[[VLCMain sharedInstance] coreDialogProvider] progressCancelled];
    [o_pool release];
}

#pragma mark -
#pragma mark Helpers

input_thread_t *getInput(void)
{
    intf_thread_t *p_intf = VLCIntf;
    if (!p_intf)
        return NULL;
    return pl_CurrentInput(p_intf);
}

vout_thread_t *getVout(void)
{
    input_thread_t *p_input = getInput();
    if (!p_input)
        return NULL;
    vout_thread_t *p_vout = input_GetVout(p_input);
    vlc_object_release(p_input);
    return p_vout;
}

audio_output_t *getAout(void)
{
    input_thread_t *p_input = getInput();
    if (!p_input)
        return NULL;
    audio_output_t *p_aout = input_GetAout(p_input);
    vlc_object_release(p_input);
    return p_aout;
}

#pragma mark -
#pragma mark Private

@interface VLCMain ()
- (void)_removeOldPreferences;
@end

/*****************************************************************************
 * VLCMain implementation
 *****************************************************************************/
@implementation VLCMain

#pragma mark -
#pragma mark Initialization

static VLCMain *_o_sharedMainInstance = nil;

+ (VLCMain *)sharedInstance
{
    return _o_sharedMainInstance ? _o_sharedMainInstance : [[self alloc] init];
}

- (id)init
{
    if( _o_sharedMainInstance)
    {
        [self dealloc];
        return _o_sharedMainInstance;
    }
    else
        _o_sharedMainInstance = [super init];

    p_intf = NULL;
    p_current_input = NULL;

    o_msg_lock = [[NSLock alloc] init];
    o_msg_arr = [[NSMutableArray arrayWithCapacity: 600] retain];

    o_open = [[VLCOpen alloc] init];
    //o_embedded_list = [[VLCEmbeddedList alloc] init];
    o_coredialogs = [[VLCCoreDialogProvider alloc] init];
    o_info = [[VLCInfo alloc] init];
    o_mainmenu = [[VLCMainMenu alloc] init];
    o_coreinteraction = [[VLCCoreInteraction alloc] init];
    o_eyetv = [[VLCEyeTVController alloc] init];
    o_mainwindow = [[VLCMainWindow alloc] init];

    /* announce our launch to a potential eyetv plugin */
    [[NSDistributedNotificationCenter defaultCenter] postNotificationName: @"VLCOSXGUIInit"
                                                                   object: @"VLCEyeTVSupport"
                                                                 userInfo: NULL
                                                       deliverImmediately: YES];

    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    NSDictionary *appDefaults = [NSDictionary dictionaryWithObject:@"NO" forKey:@"LiveUpdateTheMessagesPanel"];
    [defaults registerDefaults:appDefaults];

    return _o_sharedMainInstance;
}

- (void)setIntf: (intf_thread_t *)p_mainintf {
    p_intf = p_mainintf;
}

- (intf_thread_t *)intf {
    return p_intf;
}

- (void)awakeFromNib
{
    playlist_t *p_playlist;
    vlc_value_t val;
    if( !p_intf ) return;
    var_Create( p_intf, "intf-change", VLC_VAR_BOOL );

    /* Check if we already did this once. Opening the other nibs calls it too,
     because VLCMain is the owner */
    if( nib_main_loaded ) return;

    [o_msgs_panel setExcludedFromWindowsMenu: YES];
    [o_msgs_panel setDelegate: self];

    p_playlist = pl_Get( p_intf );

    val.b_bool = false;

    var_AddCallback(p_playlist, "fullscreen", FullscreenChanged, self);
    var_AddCallback( p_intf->p_libvlc, "intf-toggle-fscontrol", ShowController, self);
    var_AddCallback( p_intf->p_libvlc, "intf-show", ShowController, self);
    //    var_AddCallback(p_playlist, "item-change", PLItemChanged, self);
    var_AddCallback(p_playlist, "item-current", PLItemChanged, self);
    var_AddCallback(p_playlist, "activity", PLItemChanged, self);
    var_AddCallback(p_playlist, "leaf-to-parent", PlaylistUpdated, self);
    var_AddCallback(p_playlist, "playlist-item-append", PlaylistUpdated, self);
    var_AddCallback(p_playlist, "playlist-item-deleted", PlaylistUpdated, self);
    var_AddCallback(p_playlist, "random", PlaybackModeUpdated, self);
    var_AddCallback(p_playlist, "repeat", PlaybackModeUpdated, self);
    var_AddCallback(p_playlist, "loop", PlaybackModeUpdated, self);
    var_AddCallback(p_playlist, "volume", VolumeUpdated, self);
    var_AddCallback(p_playlist, "mute", VolumeUpdated, self);

    if (OSX_LION)
    {
        if ([NSApp currentSystemPresentationOptions] & NSApplicationPresentationFullScreen)
            var_SetBool( p_playlist, "fullscreen", YES );
    }

    /* load our Core Dialogs nib */
    nib_coredialogs_loaded = [NSBundle loadNibNamed:@"CoreDialogs" owner: NSApp];

    /* subscribe to various interactive dialogues */
    var_Create( p_intf, "dialog-error", VLC_VAR_ADDRESS );
    var_AddCallback( p_intf, "dialog-error", DialogCallback, self );
    var_Create( p_intf, "dialog-critical", VLC_VAR_ADDRESS );
    var_AddCallback( p_intf, "dialog-critical", DialogCallback, self );
    var_Create( p_intf, "dialog-login", VLC_VAR_ADDRESS );
    var_AddCallback( p_intf, "dialog-login", DialogCallback, self );
    var_Create( p_intf, "dialog-question", VLC_VAR_ADDRESS );
    var_AddCallback( p_intf, "dialog-question", DialogCallback, self );
    var_Create( p_intf, "dialog-progress-bar", VLC_VAR_ADDRESS );
    var_AddCallback( p_intf, "dialog-progress-bar", DialogCallback, self );
    dialog_Register( p_intf );

    /* init Apple Remote support */
    o_remote = [[AppleRemote alloc] init];
    [o_remote setClickCountEnabledButtons: kRemoteButtonPlay];
    [o_remote setDelegate: _o_sharedMainInstance];

    [o_msgs_refresh_btn setImage: [NSImage imageNamed: NSImageNameRefreshTemplate]];

     BOOL b_video_deco = var_InheritBool( VLCIntf, "video-deco" );
    /* yeah, we are done */
    b_nativeFullscreenMode = NO;
#ifdef MAC_OS_X_VERSION_10_7
    if( OSX_LION && b_video_deco )
        b_nativeFullscreenMode = var_InheritBool( p_intf, "macosx-nativefullscreenmode" );
#endif

    /* recover stored audio device, if set
     * in case it was unplugged in the meantime, auhal will fall back on the default */
    int i_value = config_GetInt( p_intf, "macosx-audio-device" );
    if (i_value > 0)
        var_SetInteger( pl_Get( VLCIntf ), "audio-device", i_value );

    if (config_GetInt( VLCIntf, "macosx-icon-change"))
    {
        /* After day 354 of the year, the usual VLC cone is replaced by another cone
         * wearing a Father Xmas hat.
         * Note: this icon doesn't represent an endorsement of The Coca-Cola Company.
         */
        NSCalendar *gregorian =
        [[NSCalendar alloc] initWithCalendarIdentifier:NSGregorianCalendar];
        NSUInteger dayOfYear = [gregorian ordinalityOfUnit:NSDayCalendarUnit inUnit:NSYearCalendarUnit forDate:[NSDate date]];
        [gregorian release];

        if (dayOfYear >= 354)
            [[VLCApplication sharedApplication] setApplicationIconImage: [NSImage imageNamed:@"vlc-xmas"]];
    }

    [self initStrings];

    nib_main_loaded = TRUE;
}

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification
{
    if( !p_intf ) return;

    [self updateCurrentlyUsedHotkeys];

    /* init media key support */
    b_mediaKeySupport = var_InheritBool( VLCIntf, "macosx-mediakeys" );
    if( b_mediaKeySupport )
    {
        o_mediaKeyController = [[SPMediaKeyTap alloc] initWithDelegate:self];
        [o_mediaKeyController startWatchingMediaKeys];
        [[NSUserDefaults standardUserDefaults] registerDefaults:[NSDictionary dictionaryWithObjectsAndKeys:
                                                                 [SPMediaKeyTap defaultMediaKeyUserBundleIdentifiers], kMediaKeyUsingBundleIdentifiersDefaultsKey,
                                                                 nil]];
    }
    [[NSNotificationCenter defaultCenter] addObserver: self selector: @selector(coreChangedMediaKeySupportSetting:) name: @"VLCMediaKeySupportSettingChanged" object: nil];

    [self _removeOldPreferences];

    /* Handle sleep notification */
    [[[NSWorkspace sharedWorkspace] notificationCenter] addObserver:self selector:@selector(computerWillSleep:)
           name:NSWorkspaceWillSleepNotification object:nil];

    [[VLCMain sharedInstance] performSelectorOnMainThread:@selector(lookForCrashLog) withObject:nil waitUntilDone:NO];

    /* we will need this, so let's load it here so the interface appears to be more responsive */
    nib_open_loaded = [NSBundle loadNibNamed:@"Open" owner: NSApp];

    /* update the main window */
    [o_mainwindow updateWindow];
    [o_mainwindow updateTimeSlider];
    [o_mainwindow updateVolumeSlider];

    playlist_t * p_playlist = pl_Get(VLCIntf);
    PL_LOCK;
    BOOL kidsAround = p_playlist->p_local_category->i_children;
    PL_UNLOCK;
    if (kidsAround && var_GetBool(p_playlist, "playlist-autostart"))
        [[self playlist] playItem:nil];
}

- (void)initStrings
{
    if( !p_intf ) return;

    /* messages panel */
    [o_msgs_panel setTitle: _NS("Messages")];
    [o_msgs_crashlog_btn setTitle: _NS("Open CrashLog...")];
    [o_msgs_save_btn setTitle: _NS("Save this Log...")];

    /* crash reporter panel */
    [o_crashrep_send_btn setTitle: _NS("Send")];
    [o_crashrep_dontSend_btn setTitle: _NS("Don't Send")];
    [o_crashrep_title_txt setStringValue: _NS("VLC crashed previously")];
    [o_crashrep_win setTitle: _NS("VLC crashed previously")];
    [o_crashrep_desc_txt setStringValue: _NS("Do you want to send details on the crash to VLC's development team?\n\nIf you want, you can enter a few lines on what you did before VLC crashed along with other helpful information: a link to download a sample file, a URL of a network stream, ...")];
    [o_crashrep_includeEmail_ckb setTitle: _NS("I agree to be possibly contacted about this bugreport.")];
    [o_crashrep_includeEmail_txt setStringValue: _NS("Only your default E-Mail address will be submitted, including no further information.")];
}

#pragma mark -
#pragma mark Termination

- (void)applicationWillTerminate:(NSNotification *)notification
{
    /* don't allow a double termination call. If the user has
     * already invoked the quit then simply return this time. */
    static bool f_appExit = false;
    bool isTerminating;

    [o_appLock lock];
    isTerminating = f_appExit;
    f_appExit = true;
    [o_appLock unlock];

    if (isTerminating)
        return;

    if (notification == nil)
        [[NSNotificationCenter defaultCenter] postNotificationName: NSApplicationWillTerminateNotification object: nil];

    playlist_t * p_playlist = pl_Get( p_intf );
    int returnedValue = 0;

    /* always exit fullscreen on quit, otherwise we get ugly artifacts on the next launch */
    if (b_nativeFullscreenMode)
    {
        [o_mainwindow toggleFullScreen: self];
        [NSApp setPresentationOptions:(NSApplicationPresentationDefault)];
    }

    /* Save some interface state in configuration, at module quit */
    config_PutInt( p_intf, "random", var_GetBool( p_playlist, "random" ) );
    config_PutInt( p_intf, "loop", var_GetBool( p_playlist, "loop" ) );
    config_PutInt( p_intf, "repeat", var_GetBool( p_playlist, "repeat" ) );

    msg_Dbg( p_intf, "Terminating" );

    /* HACK: the playlist will re-start on quit because of a core vs. UI module limitation
     * in turn, items created by transcoding can and will be over-written with garbage */
    playlist_Clear(p_playlist, false);

    /* unsubscribe from the interactive dialogues */
    dialog_Unregister( p_intf );
    var_DelCallback( p_intf, "dialog-error", DialogCallback, self );
    var_DelCallback( p_intf, "dialog-critical", DialogCallback, self );
    var_DelCallback( p_intf, "dialog-login", DialogCallback, self );
    var_DelCallback( p_intf, "dialog-question", DialogCallback, self );
    var_DelCallback( p_intf, "dialog-progress-bar", DialogCallback, self );
    //var_DelCallback(p_playlist, "item-change", PLItemChanged, self);
    var_DelCallback(p_playlist, "item-current", PLItemChanged, self);
    var_DelCallback(p_playlist, "activity", PLItemChanged, self);
    var_DelCallback(p_playlist, "leaf-to-parent", PlaylistUpdated, self);
    var_DelCallback(p_playlist, "playlist-item-append", PlaylistUpdated, self);
    var_DelCallback(p_playlist, "playlist-item-deleted", PlaylistUpdated, self);
    var_DelCallback(p_playlist, "random", PlaybackModeUpdated, self);
    var_DelCallback(p_playlist, "repeat", PlaybackModeUpdated, self);
    var_DelCallback(p_playlist, "loop", PlaybackModeUpdated, self);
    var_DelCallback(p_playlist, "volume", VolumeUpdated, self);
    var_DelCallback(p_playlist, "mute", VolumeUpdated, self);
    var_DelCallback(p_playlist, "fullscreen", FullscreenChanged, self);
    var_DelCallback(p_intf->p_libvlc, "intf-toggle-fscontrol", ShowController, self);
    var_DelCallback(p_intf->p_libvlc, "intf-show", ShowController, self);

    if( p_current_input )
    {
        var_DelCallback( p_current_input, "intf-event", InputEvent, [VLCMain sharedInstance] );
        vlc_object_release( p_current_input );
        p_current_input = NULL;
    }

    /* remove global observer watching for vout device changes correctly */
    [[NSNotificationCenter defaultCenter] removeObserver: self];

    /* release some other objects here, because it isn't sure whether dealloc
     * will be called later on */
    if( o_sprefs )
        [o_sprefs release];

    if( o_prefs )
        [o_prefs release];

    [o_open release];

    if( o_info )
        [o_info release];

    if( o_wizard )
        [o_wizard release];

    [crashLogURLConnection cancel];
    [crashLogURLConnection release];

    [o_embedded_list release];
    [o_coredialogs release];
    [o_eyetv release];

    /* unsubscribe from libvlc's debug messages */
    vlc_Unsubscribe( p_intf->p_sys->p_sub );

    [o_msg_arr removeAllObjects];
    [o_msg_arr release];
    o_msg_arr = NULL;
    [o_usedHotkeys release];
    o_usedHotkeys = NULL;

    [o_msg_lock release];

    /* write cached user defaults to disk */
    [[NSUserDefaults standardUserDefaults] synchronize];

    /* Make sure the Menu doesn't have any references to vlc objects anymore */
    //FIXME: this should be moved to VLCMainMenu
    [o_mainmenu releaseRepresentedObjects:[NSApp mainMenu]];
    [o_mainmenu release];

    libvlc_Quit( p_intf->p_libvlc );

    [o_mainwindow release];
    o_mainwindow = NULL;

    [self setIntf:nil];
}

#pragma mark -
#pragma mark Sparkle delegate
/* received directly before the update gets installed, so let's shut down a bit */
- (void)updater:(SUUpdater *)updater willInstallUpdate:(SUAppcastItem *)update
{
    [NSApp activateIgnoringOtherApps:YES];
    [o_remote stopListening: self];
    [[VLCCoreInteraction sharedInstance] stop];
}

#pragma mark -
#pragma mark Media Key support

-(void)mediaKeyTap:(SPMediaKeyTap*)keyTap receivedMediaKeyEvent:(NSEvent*)event
{
    if( b_mediaKeySupport )
       {
        assert([event type] == NSSystemDefined && [event subtype] == SPSystemDefinedEventMediaKeys);

        int keyCode = (([event data1] & 0xFFFF0000) >> 16);
        int keyFlags = ([event data1] & 0x0000FFFF);
        int keyState = (((keyFlags & 0xFF00) >> 8)) == 0xA;
        int keyRepeat = (keyFlags & 0x1);

        if( keyCode == NX_KEYTYPE_PLAY && keyState == 0 )
            [[VLCCoreInteraction sharedInstance] play];

        if( (keyCode == NX_KEYTYPE_FAST || keyCode == NX_KEYTYPE_NEXT) && !b_mediakeyJustJumped )
        {
            if( keyState == 0 && keyRepeat == 0 )
                [[VLCCoreInteraction sharedInstance] next];
            else if( keyRepeat == 1 )
            {
                [[VLCCoreInteraction sharedInstance] forwardShort];
                b_mediakeyJustJumped = YES;
                [self performSelector:@selector(resetMediaKeyJump)
                           withObject: NULL
                           afterDelay:0.25];
            }
        }

        if( (keyCode == NX_KEYTYPE_REWIND || keyCode == NX_KEYTYPE_PREVIOUS) && !b_mediakeyJustJumped )
        {
            if( keyState == 0 && keyRepeat == 0 )
                [[VLCCoreInteraction sharedInstance] previous];
            else if( keyRepeat == 1 )
            {
                [[VLCCoreInteraction sharedInstance] backwardShort];
                b_mediakeyJustJumped = YES;
                [self performSelector:@selector(resetMediaKeyJump)
                           withObject: NULL
                           afterDelay:0.25];
            }
        }
    }
}

#pragma mark -
#pragma mark Other notification

/* Listen to the remote in exclusive mode, only when VLC is the active
   application */
- (void)applicationDidBecomeActive:(NSNotification *)aNotification
{
    if( !p_intf ) return;
    if( var_InheritBool( p_intf, "macosx-appleremote" ) == YES )
        [o_remote startListening: self];
}
- (void)applicationDidResignActive:(NSNotification *)aNotification
{
    if( !p_intf ) return;
    [o_remote stopListening: self];
}

/* Triggered when the computer goes to sleep */
- (void)computerWillSleep: (NSNotification *)notification
{
    [[VLCCoreInteraction sharedInstance] pause];
}

#pragma mark -
#pragma mark File opening over dock icon

- (BOOL)application:(NSApplication *)o_app openFiles:(NSArray *)o_names
{
    BOOL b_autoplay = config_GetInt( VLCIntf, "macosx-autoplay" );
    char *psz_uri = make_URI([[o_names objectAtIndex:0] UTF8String], "file" );

    // try to add file as subtitle
    if( [o_names count] == 1 && psz_uri )
    {
        input_thread_t * p_input = pl_CurrentInput( VLCIntf );
        if( p_input )
        {
            BOOL b_returned = NO;
            b_returned = input_AddSubtitle( p_input, psz_uri, true );
            vlc_object_release( p_input );
            if( !b_returned )
            {
                free( psz_uri );
                return YES;
            }
        }
    }
    free( psz_uri );

    NSArray *o_sorted_names = [o_names sortedArrayUsingSelector: @selector(caseInsensitiveCompare:)];
    NSMutableArray *o_result = [NSMutableArray arrayWithCapacity: [o_sorted_names count]];
    for( int i = 0; i < [o_sorted_names count]; i++ )
    {
        psz_uri = make_URI([[o_sorted_names objectAtIndex: i] UTF8String], "file" );
        if( !psz_uri )
            continue;

        NSDictionary *o_dic = [NSDictionary dictionaryWithObject:[NSString stringWithCString:psz_uri encoding:NSUTF8StringEncoding] forKey:@"ITEM_URL"];
        free( psz_uri );
        [o_result addObject: o_dic];
    }

    if( b_autoplay )
        [o_playlist appendArray: o_result atPos: -1 enqueue: NO];
    else
        [o_playlist appendArray: o_result atPos: -1 enqueue: YES];

    return( TRUE );
}

/* When user click in the Dock icon our double click in the finder */
- (BOOL)applicationShouldHandleReopen:(NSApplication *)theApplication hasVisibleWindows:(BOOL)hasVisibleWindows
{
    if(!hasVisibleWindows)
        [o_mainwindow makeKeyAndOrderFront:self];

    return YES;
}

#pragma mark -
#pragma mark Apple Remote Control

/* Helper method for the remote control interface in order to trigger forward/backward and volume
   increase/decrease as long as the user holds the left/right, plus/minus button */
- (void) executeHoldActionForRemoteButton: (NSNumber*) buttonIdentifierNumber
{
    if(b_remote_button_hold)
    {
        switch([buttonIdentifierNumber intValue])
        {
            case kRemoteButtonRight_Hold:
                [[VLCCoreInteraction sharedInstance] forward];
                break;
            case kRemoteButtonLeft_Hold:
                [[VLCCoreInteraction sharedInstance] backward];
                break;
            case kRemoteButtonVolume_Plus_Hold:
                if( p_intf )
                    var_SetInteger( p_intf->p_libvlc, "key-action", ACTIONID_VOL_UP );
                break;
            case kRemoteButtonVolume_Minus_Hold:
                if( p_intf )
                    var_SetInteger( p_intf->p_libvlc, "key-action", ACTIONID_VOL_DOWN );
                break;
        }
        if(b_remote_button_hold)
        {
            /* trigger event */
            [self performSelector:@selector(executeHoldActionForRemoteButton:)
                         withObject:buttonIdentifierNumber
                         afterDelay:0.25];
        }
    }
}

/* Apple Remote callback */
- (void) appleRemoteButton: (AppleRemoteEventIdentifier)buttonIdentifier
               pressedDown: (BOOL) pressedDown
                clickCount: (unsigned int) count
{
    switch( buttonIdentifier )
    {
        case k2009RemoteButtonFullscreen:
            [[VLCCoreInteraction sharedInstance] toggleFullscreen];
            break;
        case k2009RemoteButtonPlay:
            [[VLCCoreInteraction sharedInstance] play];
            break;
        case kRemoteButtonPlay:
            if(count >= 2) {
                [[VLCCoreInteraction sharedInstance] toggleFullscreen];
            } else {
                [[VLCCoreInteraction sharedInstance] play];
            }
            break;
        case kRemoteButtonVolume_Plus:
            if( p_intf )
                var_SetInteger( p_intf->p_libvlc, "key-action", ACTIONID_VOL_UP );
            break;
        case kRemoteButtonVolume_Minus:
            if( p_intf )
                var_SetInteger( p_intf->p_libvlc, "key-action", ACTIONID_VOL_DOWN );
            break;
        case kRemoteButtonRight:
            [[VLCCoreInteraction sharedInstance] next];
            break;
        case kRemoteButtonLeft:
            [[VLCCoreInteraction sharedInstance] previous];
            break;
        case kRemoteButtonRight_Hold:
        case kRemoteButtonLeft_Hold:
        case kRemoteButtonVolume_Plus_Hold:
        case kRemoteButtonVolume_Minus_Hold:
            /* simulate an event as long as the user holds the button */
            b_remote_button_hold = pressedDown;
            if( pressedDown )
            {
                NSNumber* buttonIdentifierNumber = [NSNumber numberWithInt: buttonIdentifier];
                [self performSelector:@selector(executeHoldActionForRemoteButton:)
                           withObject:buttonIdentifierNumber];
            }
            break;
        case kRemoteButtonMenu:
            [o_controls showPosition: self]; //FIXME
            break;
        case kRemoteButtonPlay_Sleep:
        {
            NSAppleScript * script = [[NSAppleScript alloc] initWithSource:@"tell application \"System Events\" to sleep"];
            [script executeAndReturnError:nil];
            [script release];
            break;
        }
        default:
            /* Add here whatever you want other buttons to do */
            break;
    }
}

#pragma mark -
#pragma mark String utility
// FIXME: this has nothing to do here

- (NSString *)localizedString:(const char *)psz
{
    NSString * o_str = nil;

    if( psz != NULL )
    {
        o_str = [NSString stringWithCString: _(psz) encoding:NSUTF8StringEncoding];

        if( o_str == NULL )
        {
            msg_Err( VLCIntf, "could not translate: %s", psz );
            return( @"" );
        }
    }
    else
    {
        msg_Warn( VLCIntf, "can't translate empty strings" );
        return( @"" );
    }

    return( o_str );
}



- (char *)delocalizeString:(NSString *)id
{
    NSData * o_data = [id dataUsingEncoding: NSUTF8StringEncoding
                          allowLossyConversion: NO];
    char * psz_string;

    if( o_data == nil )
    {
        o_data = [id dataUsingEncoding: NSUTF8StringEncoding
                     allowLossyConversion: YES];
        psz_string = malloc( [o_data length] + 1 );
        [o_data getBytes: psz_string];
        psz_string[ [o_data length] ] = '\0';
        msg_Err( VLCIntf, "cannot convert to the requested encoding: %s",
                 psz_string );
    }
    else
    {
        psz_string = malloc( [o_data length] + 1 );
        [o_data getBytes: psz_string];
        psz_string[ [o_data length] ] = '\0';
    }

    return psz_string;
}

/* i_width is in pixels */
- (NSString *)wrapString: (NSString *)o_in_string toWidth: (int) i_width
{
    NSMutableString *o_wrapped;
    NSString *o_out_string;
    NSRange glyphRange, effectiveRange, charRange;
    NSRect lineFragmentRect;
    unsigned glyphIndex, breaksInserted = 0;

    NSTextStorage *o_storage = [[NSTextStorage alloc] initWithString: o_in_string
        attributes: [NSDictionary dictionaryWithObjectsAndKeys:
        [NSFont labelFontOfSize: 0.0], NSFontAttributeName, nil]];
    NSLayoutManager *o_layout_manager = [[NSLayoutManager alloc] init];
    NSTextContainer *o_container = [[NSTextContainer alloc]
        initWithContainerSize: NSMakeSize(i_width, 2000)];

    [o_layout_manager addTextContainer: o_container];
    [o_container release];
    [o_storage addLayoutManager: o_layout_manager];
    [o_layout_manager release];

    o_wrapped = [o_in_string mutableCopy];
    glyphRange = [o_layout_manager glyphRangeForTextContainer: o_container];

    for( glyphIndex = glyphRange.location ; glyphIndex < NSMaxRange(glyphRange) ;
            glyphIndex += effectiveRange.length) {
        lineFragmentRect = [o_layout_manager lineFragmentRectForGlyphAtIndex: glyphIndex
                                            effectiveRange: &effectiveRange];
        charRange = [o_layout_manager characterRangeForGlyphRange: effectiveRange
                                    actualGlyphRange: &effectiveRange];
        if([o_wrapped lineRangeForRange:
                NSMakeRange(charRange.location + breaksInserted, charRange.length)].length > charRange.length) {
            [o_wrapped insertString: @"\n" atIndex: NSMaxRange(charRange) + breaksInserted];
            breaksInserted++;
        }
    }
    o_out_string = [NSString stringWithString: o_wrapped];
    [o_wrapped release];
    [o_storage release];

    return o_out_string;
}


#pragma mark -
#pragma mark Key Shortcuts

static struct
{
    unichar i_nskey;
    unsigned int i_vlckey;
} nskeys_to_vlckeys[] =
{
    { NSUpArrowFunctionKey, KEY_UP },
    { NSDownArrowFunctionKey, KEY_DOWN },
    { NSLeftArrowFunctionKey, KEY_LEFT },
    { NSRightArrowFunctionKey, KEY_RIGHT },
    { NSF1FunctionKey, KEY_F1 },
    { NSF2FunctionKey, KEY_F2 },
    { NSF3FunctionKey, KEY_F3 },
    { NSF4FunctionKey, KEY_F4 },
    { NSF5FunctionKey, KEY_F5 },
    { NSF6FunctionKey, KEY_F6 },
    { NSF7FunctionKey, KEY_F7 },
    { NSF8FunctionKey, KEY_F8 },
    { NSF9FunctionKey, KEY_F9 },
    { NSF10FunctionKey, KEY_F10 },
    { NSF11FunctionKey, KEY_F11 },
    { NSF12FunctionKey, KEY_F12 },
    { NSInsertFunctionKey, KEY_INSERT },
    { NSHomeFunctionKey, KEY_HOME },
    { NSEndFunctionKey, KEY_END },
    { NSPageUpFunctionKey, KEY_PAGEUP },
    { NSPageDownFunctionKey, KEY_PAGEDOWN },
    { NSMenuFunctionKey, KEY_MENU },
    { NSTabCharacter, KEY_TAB },
    { NSCarriageReturnCharacter, KEY_ENTER },
    { NSEnterCharacter, KEY_ENTER },
    { NSBackspaceCharacter, KEY_BACKSPACE },
    { NSDeleteCharacter, KEY_DELETE },
    {0,0}
};

unsigned int CocoaKeyToVLC( unichar i_key )
{
    unsigned int i;

    for( i = 0; nskeys_to_vlckeys[i].i_nskey != 0; i++ )
    {
        if( nskeys_to_vlckeys[i].i_nskey == i_key )
        {
            return nskeys_to_vlckeys[i].i_vlckey;
        }
    }
    return (unsigned int)i_key;
}

- (unsigned int)VLCModifiersToCocoa:(NSString *)theString
{
    unsigned int new = 0;

    if([theString rangeOfString:@"Command"].location != NSNotFound)
        new |= NSCommandKeyMask;
    if([theString rangeOfString:@"Alt"].location != NSNotFound)
        new |= NSAlternateKeyMask;
    if([theString rangeOfString:@"Shift"].location != NSNotFound)
        new |= NSShiftKeyMask;
    if([theString rangeOfString:@"Ctrl"].location != NSNotFound)
        new |= NSControlKeyMask;
    return new;
}

- (NSString *)VLCKeyToString:(NSString *)theString
{
    if (![theString isEqualToString:@""]) {
        if ([theString characterAtIndex:([theString length] - 1)] != 0x2b)
            theString = [theString stringByReplacingOccurrencesOfString:@"+" withString:@""];
        else
        {
            theString = [theString stringByReplacingOccurrencesOfString:@"+" withString:@""];
            theString = [NSString stringWithFormat:@"%@+", theString];
        }
        if ([theString characterAtIndex:([theString length] - 1)] != 0x2d)
            theString = [theString stringByReplacingOccurrencesOfString:@"-" withString:@""];
        else
        {
            theString = [theString stringByReplacingOccurrencesOfString:@"-" withString:@""];
            theString = [NSString stringWithFormat:@"%@-", theString];
        }
        theString = [theString stringByReplacingOccurrencesOfString:@"Command" withString:@""];
        theString = [theString stringByReplacingOccurrencesOfString:@"Alt" withString:@""];
        theString = [theString stringByReplacingOccurrencesOfString:@"Shift" withString:@""];
        theString = [theString stringByReplacingOccurrencesOfString:@"Ctrl" withString:@""];
    }
    if ([theString length] > 1)
    {
        if([theString rangeOfString:@"Up"].location != NSNotFound)
            return [NSString stringWithFormat:@"%C", NSUpArrowFunctionKey];
        else if([theString rangeOfString:@"Down"].location != NSNotFound)
            return [NSString stringWithFormat:@"%C", NSDownArrowFunctionKey];
        else if([theString rangeOfString:@"Right"].location != NSNotFound)
            return [NSString stringWithFormat:@"%C", NSRightArrowFunctionKey];
        else if([theString rangeOfString:@"Left"].location != NSNotFound)
            return [NSString stringWithFormat:@"%C", NSLeftArrowFunctionKey];
        else if([theString rangeOfString:@"Enter"].location != NSNotFound)
            return [NSString stringWithFormat:@"%C", NSEnterCharacter]; // we treat NSCarriageReturnCharacter as aquivalent
        else if([theString rangeOfString:@"Insert"].location != NSNotFound)
            return [NSString stringWithFormat:@"%C", NSInsertFunctionKey];
        else if([theString rangeOfString:@"Home"].location != NSNotFound)
            return [NSString stringWithFormat:@"%C", NSHomeFunctionKey];
        else if([theString rangeOfString:@"End"].location != NSNotFound)
            return [NSString stringWithFormat:@"%C", NSEndFunctionKey];
        else if([theString rangeOfString:@"Pageup"].location != NSNotFound)
            return [NSString stringWithFormat:@"%C", NSPageUpFunctionKey];
        else if([theString rangeOfString:@"Pagedown"].location != NSNotFound)
            return [NSString stringWithFormat:@"%C", NSPageDownFunctionKey];
        else if([theString rangeOfString:@"Menu"].location != NSNotFound)
            return [NSString stringWithFormat:@"%C", NSMenuFunctionKey];
        else if([theString rangeOfString:@"Tab"].location != NSNotFound)
            return [NSString stringWithFormat:@"%C", NSTabCharacter];
        else if([theString rangeOfString:@"Backspace"].location != NSNotFound)
            return [NSString stringWithFormat:@"%C", NSBackspaceCharacter];
        else if([theString rangeOfString:@"Delete"].location != NSNotFound)
            return [NSString stringWithFormat:@"%C", NSDeleteCharacter];
        else if([theString rangeOfString:@"F12"].location != NSNotFound)
            return [NSString stringWithFormat:@"%C", NSF12FunctionKey];
        else if([theString rangeOfString:@"F11"].location != NSNotFound)
            return [NSString stringWithFormat:@"%C", NSF11FunctionKey];
        else if([theString rangeOfString:@"F10"].location != NSNotFound)
            return [NSString stringWithFormat:@"%C", NSF10FunctionKey];
        else if([theString rangeOfString:@"F9"].location != NSNotFound)
            return [NSString stringWithFormat:@"%C", NSF9FunctionKey];
        else if([theString rangeOfString:@"F8"].location != NSNotFound)
            return [NSString stringWithFormat:@"%C", NSF8FunctionKey];
        else if([theString rangeOfString:@"F7"].location != NSNotFound)
            return [NSString stringWithFormat:@"%C", NSF7FunctionKey];
        else if([theString rangeOfString:@"F6"].location != NSNotFound)
            return [NSString stringWithFormat:@"%C", NSF6FunctionKey];
        else if([theString rangeOfString:@"F5"].location != NSNotFound)
            return [NSString stringWithFormat:@"%C", NSF5FunctionKey];
        else if([theString rangeOfString:@"F4"].location != NSNotFound)
            return [NSString stringWithFormat:@"%C", NSF4FunctionKey];
        else if([theString rangeOfString:@"F3"].location != NSNotFound)
            return [NSString stringWithFormat:@"%C", NSF3FunctionKey];
        else if([theString rangeOfString:@"F2"].location != NSNotFound)
            return [NSString stringWithFormat:@"%C", NSF2FunctionKey];
        else if([theString rangeOfString:@"F1"].location != NSNotFound)
            return [NSString stringWithFormat:@"%C", NSF1FunctionKey];
        /* note that we don't support esc here, since it is reserved for leaving fullscreen */
    }

    return theString;
}


/*****************************************************************************
 * hasDefinedShortcutKey: Check to see if the key press is a defined VLC
 * shortcut key.  If it is, pass it off to VLC for handling and return YES,
 * otherwise ignore it and return NO (where it will get handled by Cocoa).
 *****************************************************************************/
- (BOOL)hasDefinedShortcutKey:(NSEvent *)o_event force:(BOOL)b_force
{
    unichar key = 0;
    vlc_value_t val;
    unsigned int i_pressed_modifiers = 0;

    val.i_int = 0;
    i_pressed_modifiers = [o_event modifierFlags];

    if( i_pressed_modifiers & NSControlKeyMask ) {
        val.i_int |= KEY_MODIFIER_CTRL;
    }
    if( i_pressed_modifiers & NSAlternateKeyMask ) {
        val.i_int |= KEY_MODIFIER_ALT;
    }
    if( i_pressed_modifiers & NSShiftKeyMask ) {
        val.i_int |= KEY_MODIFIER_SHIFT;
    }
    if( i_pressed_modifiers & NSCommandKeyMask ) {
        val.i_int |= KEY_MODIFIER_COMMAND;
    }

    NSString * characters = [o_event charactersIgnoringModifiers];
    if ([characters length] > 0)
    {
        key = [[characters lowercaseString] characterAtIndex: 0];

        /* handle Lion's default key combo for fullscreen-toggle in addition to our own hotkeys */
        if( key == 'f' && i_pressed_modifiers & NSControlKeyMask && i_pressed_modifiers & NSCommandKeyMask )
        {
            [[VLCCoreInteraction sharedInstance] toggleFullscreen];
            return YES;
        }

        if( !b_force )
        {
            switch( key )
            {
                case NSDeleteCharacter:
                case NSDeleteFunctionKey:
                case NSDeleteCharFunctionKey:
                case NSBackspaceCharacter:
                case NSUpArrowFunctionKey:
                case NSDownArrowFunctionKey:
                case NSEnterCharacter:
                case NSCarriageReturnCharacter:
                    return NO;
            }
        }

        if( key == 0x0020 ) // space key
        {
            [[VLCCoreInteraction sharedInstance] play];
            return YES;
        }

        val.i_int |= CocoaKeyToVLC( key );

        BOOL b_found_key = NO;
        for( int i = 0; i < [o_usedHotkeys count]; i++ )
        {
            NSString *str = [o_usedHotkeys objectAtIndex: i];
            unsigned int i_keyModifiers = [self VLCModifiersToCocoa: str];

            if( [[characters lowercaseString] isEqualToString: [self VLCKeyToString: str]] &&
               (i_keyModifiers & NSShiftKeyMask)     == (i_pressed_modifiers & NSShiftKeyMask) &&
               (i_keyModifiers & NSControlKeyMask)   == (i_pressed_modifiers & NSControlKeyMask) &&
               (i_keyModifiers & NSAlternateKeyMask) == (i_pressed_modifiers & NSAlternateKeyMask) &&
               (i_keyModifiers & NSCommandKeyMask)   == (i_pressed_modifiers & NSCommandKeyMask) )
            {
                b_found_key = YES;
                break;
            }
        }

        if( b_found_key )
        {
            var_SetInteger( p_intf->p_libvlc, "key-pressed", val.i_int );
            return YES;
        }
    }

    return NO;
}

- (void)updateCurrentlyUsedHotkeys
{
    NSMutableArray *o_tempArray = [[NSMutableArray alloc] init];
    /* Get the main Module */
    module_t *p_main = module_get_main();
    assert( p_main );
    unsigned confsize;
    module_config_t *p_config;

    p_config = module_config_get (p_main, &confsize);

    for (size_t i = 0; i < confsize; i++)
    {
        module_config_t *p_item = p_config + i;

        if( CONFIG_ITEM(p_item->i_type) && p_item->psz_name != NULL
           && !strncmp( p_item->psz_name , "key-", 4 )
           && !EMPTY_STR( p_item->psz_text ) )
        {
            if (p_item->value.psz)
            {
                [o_tempArray addObject: [NSString stringWithUTF8String:p_item->value.psz]];
            }
        }
    }
    module_config_free (p_config);

    if( o_usedHotkeys )
        [o_usedHotkeys release];
    o_usedHotkeys = [[NSArray alloc] initWithArray: o_tempArray copyItems: YES];
    [o_tempArray release];
}

#pragma mark -
#pragma mark Interface updaters
- (void)fullscreenChanged
{
    playlist_t * p_playlist = pl_Get( VLCIntf );
    BOOL b_fullscreen = var_GetBool( p_playlist, "fullscreen" );

    if (b_nativeFullscreenMode)
    {
        // this is called twice in certain situations, so only toogle if we really need to
        if( (  b_fullscreen && !([NSApp currentSystemPresentationOptions] & NSApplicationPresentationFullScreen) ) ||
            ( !b_fullscreen &&  ([NSApp currentSystemPresentationOptions] & NSApplicationPresentationFullScreen) ) )
            [o_mainwindow toggleFullScreen: self];

        if(b_fullscreen)
            [NSApp setPresentationOptions:(NSApplicationPresentationFullScreen | NSApplicationPresentationAutoHideDock | NSApplicationPresentationAutoHideMenuBar)];
        else
            [NSApp setPresentationOptions:(NSApplicationPresentationDefault)];
    }
    else
    {
        if( b_fullscreen )
        {
            input_thread_t * p_input = pl_CurrentInput( VLCIntf );
            if( p_input != NULL && [self activeVideoPlayback] )
            {
                // activate app, as method can also be triggered from outside the app (prevents nasty window layout)
                [NSApp activateIgnoringOtherApps:YES];
                [o_mainwindow performSelectorOnMainThread:@selector(enterFullscreen) withObject:nil waitUntilDone:NO];
            }
            if (p_input)
                vlc_object_release( p_input );
        }
        else
        {
            // leaving fullscreen is always allowed
            [o_mainwindow performSelectorOnMainThread:@selector(leaveFullscreen) withObject:nil waitUntilDone:NO];
        }
    }
}

- (void)checkFullscreenChange:(NSNumber *)o_full
{
    BOOL b_full = [o_full boolValue];
    if( p_intf && !var_GetBool( pl_Get( p_intf ), "fullscreen" ) != !b_full )
    {
        var_SetBool( pl_Get(p_intf), "fullscreen", b_full );
    }
}

- (void)PlaylistItemChanged
{
    if( p_current_input && ( p_current_input->b_dead || !vlc_object_alive( p_current_input ) ))
    {
        var_DelCallback( p_current_input, "intf-event", InputEvent, [VLCMain sharedInstance] );
        vlc_object_release( p_current_input );
        p_current_input = NULL;

        [o_mainmenu setRateControlsEnabled: NO];
    }
    else if( !p_current_input )
    {
        // object is hold here and released then it is dead
        p_current_input = playlist_CurrentInput( pl_Get( VLCIntf ));
        if( p_current_input )
        {
            var_AddCallback( p_current_input, "intf-event", InputEvent, [VLCMain sharedInstance] );
            [self playbackStatusUpdated];
            [o_mainmenu setRateControlsEnabled: YES];
            if ( [self activeVideoPlayback] && [[o_mainwindow videoView] isHidden] )
                [o_mainwindow performSelectorOnMainThread:@selector(togglePlaylist:) withObject: nil waitUntilDone:NO];
        }
    }

    [o_playlist updateRowSelection];
    [o_mainwindow updateWindow];
    [self updateDelays];
    [self updateMainMenu];
}

- (void)updateMainMenu
{
    [o_mainmenu setupMenus];
    [o_mainmenu updatePlaybackRate];
}

- (void)updateMainWindow
{
    [o_mainwindow updateWindow];
}

- (void)showMainWindow
{
    [o_mainwindow performSelectorOnMainThread:@selector(makeKeyAndOrderFront:) withObject:nil waitUntilDone:NO];
}

- (void)showFullscreenController
{
    [o_mainwindow performSelectorOnMainThread:@selector(showFullscreenController) withObject:nil waitUntilDone:NO];
}

- (void)updateDelays
{
    [[VLCTrackSynchronization sharedInstance] performSelectorOnMainThread: @selector(updateValues) withObject: nil waitUntilDone:NO];
}

- (void)updateName
{
    [o_mainwindow updateName];
}

- (void)updatePlaybackPosition
{
#ifndef __x86_64__
    /* 10.5 compatibility mode, for sane stuff, check playbackStatusUpdated */
    if (NSAppKitVersionNumber < 1038) {
        input_thread_t * p_input;
        p_input = pl_CurrentInput(p_intf);
        if (p_input) {
            if (var_GetInteger(p_input, "state") == PLAYING_S && [self activeVideoPlayback])
                UpdateSystemActivity(UsrActivity);
            vlc_object_release(p_input);
        }
    }
#endif
    [o_mainwindow updateTimeSlider];
}

- (void)updateVolume
{
    [o_mainwindow updateVolumeSlider];
}

- (void)playlistUpdated
{
    [self playbackStatusUpdated];
    [o_playlist playlistUpdated];
    [o_mainwindow updateWindow];
    [o_mainwindow updateName];
}

- (void)updateRecordState: (BOOL)b_value
{
    [o_mainmenu updateRecordState:b_value];
}

- (void)updateInfoandMetaPanel
{
    [o_playlist outlineViewSelectionDidChange:nil];
}

- (void)playbackStatusUpdated
{
    input_thread_t * p_input;

    p_input = pl_CurrentInput( p_intf );
    if( p_input )
    {
        IOReturn success;

        int state = var_GetInteger( p_input, "state" );
        if( state == PLAYING_S )
        {
            /* check for previous blocker and release it if needed */
            if (systemSleepAssertionID > 0) {
                msg_Dbg( VLCIntf, "releasing sleep blocker (%i)" , systemSleepAssertionID );
                success = IOPMAssertionRelease( systemSleepAssertionID );
                if (success == kIOReturnSuccess)
                    systemSleepAssertionID = 0;
            }

#ifdef __x86_64__
            /* prevent the system from sleeping using the 10.5 API to be as compatible as possible */
            /* work-around a bug in 10.7.4 and 10.7.5, so check for 10.7.x < 10.7.4 and 10.8 */
            if ((NSAppKitVersionNumber >= 1115.2 && NSAppKitVersionNumber < 1138.45) || OSX_MOUNTAIN_LION) {
                CFStringRef reasonForActivity= CFStringCreateWithCString(kCFAllocatorDefault, "VLC media playback", kCFStringEncodingUTF8);
                if ([self activeVideoPlayback]) {
                    NSLog( @"kIOPMAssertionTypePreventUserIdleDisplaySleep" );
                    success = IOPMAssertionCreateWithName(kIOPMAssertionTypePreventUserIdleDisplaySleep, kIOPMAssertionLevelOn, reasonForActivity, &systemSleepAssertionID);
                } else {
                    NSLog( @"kIOPMAssertionTypePreventUserIdleSystemSleep" );
                    success = IOPMAssertionCreateWithName(kIOPMAssertionTypePreventUserIdleSystemSleep, kIOPMAssertionLevelOn, reasonForActivity, &systemSleepAssertionID);
                    }
                CFRelease(reasonForActivity);
            } else { // 10.6 and later
                /* use the legacy mode, which works on 10.6, 10.7.4 and 10.7.5 */
                if ([self activeVideoPlayback])
                    success = IOPMAssertionCreate(kIOPMAssertionTypeNoDisplaySleep, kIOPMAssertionLevelOn, &systemSleepAssertionID);
                else
                    success = IOPMAssertionCreate(kIOPMAssertionTypeNoIdleSleep, kIOPMAssertionLevelOn, &systemSleepAssertionID);
            }
#else
            if (NSAppKitVersionNumber >= 1038) { // 10.6 and later
                /* use the legacy mode, which works on 10.6, 10.7.4 and 10.7.5 */
                if ([self activeVideoPlayback])
                    success = IOPMAssertionCreate(kIOPMAssertionTypeNoDisplaySleep, kIOPMAssertionLevelOn, &systemSleepAssertionID);
                else
                    success = IOPMAssertionCreate(kIOPMAssertionTypeNoIdleSleep, kIOPMAssertionLevelOn, &systemSleepAssertionID);
            } // else is handled in updatePlaybackPosition
#endif

            if (success == kIOReturnSuccess)
                msg_Dbg( VLCIntf, "prevented sleep through IOKit (%i)", systemSleepAssertionID);
            else
                msg_Warn( VLCIntf, "failed to prevent system sleep through IOKit");

            [[self mainMenu] setPause];
            [o_mainwindow setPause];
        }
        else
        {
            if (state == END_S)
                [o_mainmenu setSubmenusEnabled: FALSE];
            [[self mainMenu] setPlay];
            [o_mainwindow setPlay];

            /* allow the system to sleep again */
            if (systemSleepAssertionID > 0) {
                msg_Dbg( VLCIntf, "releasing sleep blocker (%i)" , systemSleepAssertionID );
                success = IOPMAssertionRelease( systemSleepAssertionID );
                if (success == kIOReturnSuccess)
                    systemSleepAssertionID = 0;
            }
        }
        vlc_object_release( p_input );
    }

    [[VLCMain sharedInstance] performSelectorOnMainThread:@selector(updateMainWindow) withObject: nil waitUntilDone: NO];
    [self performSelectorOnMainThread:@selector(sendDistributedNotificationWithUpdatedPlaybackStatus) withObject: nil waitUntilDone: NO];
}

- (void)sendDistributedNotificationWithUpdatedPlaybackStatus
{
    [[NSDistributedNotificationCenter defaultCenter] postNotificationName:@"VLCPlayerStateDidChange"
                                                                   object:nil
                                                                 userInfo:nil
                                                       deliverImmediately:YES];
}

- (void)playbackModeUpdated
{
    vlc_value_t looping,repeating;
    playlist_t * p_playlist = pl_Get( VLCIntf );

    bool loop = var_GetBool( p_playlist, "loop" );
    bool repeat = var_GetBool( p_playlist, "repeat" );
    if( repeat ) {
        [o_mainwindow setRepeatOne];
        [o_mainmenu setRepeatOne];
    } else if( loop ) {
        [o_mainwindow setRepeatAll];
        [o_mainmenu setRepeatAll];
    } else {
        [o_mainwindow setRepeatOff];
        [o_mainmenu setRepeatOff];
    }

    [o_mainwindow setShuffle];
    [o_mainmenu setShuffle];
}


#pragma mark -
#pragma mark Window updater

- (void)setWindowLevel:(NSNumber*)state
{
    if ([[VLCMainWindow sharedInstance] isFullscreen])
        return;

    if ([state unsignedIntValue] & VOUT_WINDOW_STATE_ABOVE)
        [[[[VLCMainWindow sharedInstance] videoView] window] setLevel: NSStatusWindowLevel];
    else
        [[[[VLCMainWindow sharedInstance] videoView] window] setLevel: NSNormalWindowLevel];
}

- (void)setActiveVideoPlayback:(BOOL)b_value
{
    b_active_videoplayback = b_value;

    if( o_mainwindow )
    {
        [o_mainwindow performSelectorOnMainThread:@selector(setVideoplayEnabled) withObject:nil waitUntilDone:YES];
        [o_mainwindow performSelectorOnMainThread:@selector(togglePlaylist:) withObject:nil waitUntilDone:NO];
    }

    [self performSelectorOnMainThread:@selector(playbackStatusUpdated) withObject:nil waitUntilDone:NO];
}

- (void)setNativeVideoSize:(NSSize)size
{
    [o_mainwindow setNativeVideoSize:size];
}

#pragma mark -
#pragma mark Other objects getters

- (id)mainMenu
{
    return o_mainmenu;
}

- (id)mainWindow
{
    return o_mainwindow;
}

- (id)controls
{
    if( o_controls )
        return o_controls;

    return nil;
}

- (id)bookmarks
{
    if (!o_bookmarks )
        o_bookmarks = [[VLCBookmarks alloc] init];

    if( !nib_bookmarks_loaded )
        nib_bookmarks_loaded = [NSBundle loadNibNamed:@"Bookmarks" owner: NSApp];

    return o_bookmarks;
}

- (id)open
{
    if (!o_open)
        return nil;

    if (!nib_open_loaded)
        nib_open_loaded = [NSBundle loadNibNamed:@"Open" owner: NSApp];

    return o_open;
}

- (id)simplePreferences
{
    if (!o_sprefs)
        o_sprefs = [[VLCSimplePrefs alloc] init];

    if (!nib_prefs_loaded)
        nib_prefs_loaded = [NSBundle loadNibNamed:@"Preferences" owner: NSApp];

    return o_sprefs;
}

- (id)preferences
{
    if( !o_prefs )
        o_prefs = [[VLCPrefs alloc] init];

    if( !nib_prefs_loaded )
        nib_prefs_loaded = [NSBundle loadNibNamed:@"Preferences" owner: NSApp];

    return o_prefs;
}

- (id)playlist
{
    if( o_playlist )
        return o_playlist;

    return nil;
}

- (id)info
{
    if(! nib_info_loaded )
        nib_info_loaded = [NSBundle loadNibNamed:@"MediaInfo" owner: NSApp];

    if( o_info )
        return o_info;

    return nil;
}

- (id)wizard
{
    if( !o_wizard )
        o_wizard = [[VLCWizard alloc] init];

    if( !nib_wizard_loaded )
    {
        nib_wizard_loaded = [NSBundle loadNibNamed:@"Wizard" owner: NSApp];
        [o_wizard initStrings];
    }
    return o_wizard;
}

- (id)getVideoViewAtPositionX: (int *)pi_x Y: (int *)pi_y withWidth: (unsigned int*)pi_width andHeight: (unsigned int*)pi_height
{
    [o_mainwindow performSelectorOnMainThread:@selector(setupVideoView) withObject:nil waitUntilDone:YES];
    id videoView = [o_mainwindow videoView];
    NSRect videoRect = [videoView frame];
    int i_x = (int)videoRect.origin.x;
    int i_y = (int)videoRect.origin.y;
    unsigned int i_width = (int)videoRect.size.width;
    unsigned int i_height = (int)videoRect.size.height;
    pi_x = &i_x;
    pi_y = &i_y;
    pi_width = &i_width;
    pi_height = &i_height;
    msg_Dbg( VLCIntf, "returning videoview with x=%i, y=%i, width=%i, height=%i", i_x, i_y, i_width, i_height );
    return videoView;
}

- (id)embeddedList
{
    if( o_embedded_list )
        return o_embedded_list;

    return nil;
}

- (id)coreDialogProvider
{
    if( o_coredialogs )
        return o_coredialogs;

    return nil;
}

- (id)eyeTVController
{
    if( o_eyetv )
        return o_eyetv;

    return nil;
}

- (id)appleRemoteController
{
    return o_remote;
}

- (BOOL)activeVideoPlayback
{
    return b_active_videoplayback;
}

#pragma mark -
#pragma mark Crash Log
- (void)sendCrashLog:(NSString *)crashLog withUserComment:(NSString *)userComment
{
    NSString *urlStr = @"http://crash.videolan.org/crashlog/sendcrashreport.php";
    NSURL *url = [NSURL URLWithString:urlStr];

    NSMutableURLRequest *req = [NSMutableURLRequest requestWithURL:url];
    [req setHTTPMethod:@"POST"];

    NSString * email;
    if( [o_crashrep_includeEmail_ckb state] == NSOnState )
    {
        ABPerson * contact = [[ABAddressBook sharedAddressBook] me];
        ABMultiValue *emails = [contact valueForProperty:kABEmailProperty];
        email = [emails valueAtIndex:[emails indexForIdentifier:
                    [emails primaryIdentifier]]];
    }
    else
        email = [NSString string];

    NSString *postBody;
    postBody = [NSString stringWithFormat:@"CrashLog=%@&Comment=%@&Email=%@\r\n",
            [crashLog stringByAddingPercentEscapesUsingEncoding:NSUTF8StringEncoding],
            [userComment stringByAddingPercentEscapesUsingEncoding:NSUTF8StringEncoding],
            [email stringByAddingPercentEscapesUsingEncoding:NSUTF8StringEncoding]];

    [req setHTTPBody:[postBody dataUsingEncoding:NSUTF8StringEncoding]];

    /* Released from delegate */
    crashLogURLConnection = [[NSURLConnection alloc] initWithRequest:req delegate:self];
}

- (void)connectionDidFinishLoading:(NSURLConnection *)connection
{
    msg_Dbg( p_intf, "crash report successfully sent" );
    [crashLogURLConnection release];
    crashLogURLConnection = nil;
}

- (void)connection:(NSURLConnection *)connection didFailWithError:(NSError *)error
{
    msg_Warn (p_intf, "Error when sending the crash report: %s (%li)", [[error localizedDescription] UTF8String], [error code]);
    [crashLogURLConnection release];
    crashLogURLConnection = nil;
}

- (NSString *)latestCrashLogPathPreviouslySeen:(BOOL)previouslySeen
{
    NSString * crashReporter;
    if( OSX_MOUNTAIN_LION )
        crashReporter = [@"~/Library/Logs/DiagnosticReports" stringByExpandingTildeInPath];
    else
        crashReporter = [@"~/Library/Logs/CrashReporter" stringByExpandingTildeInPath];
    NSDirectoryEnumerator *direnum = [[NSFileManager defaultManager] enumeratorAtPath:crashReporter];
    NSString *fname;
    NSString * latestLog = nil;
    int year  = !previouslySeen ? [[NSUserDefaults standardUserDefaults] integerForKey:@"LatestCrashReportYear"] : 0;
    int month = !previouslySeen ? [[NSUserDefaults standardUserDefaults] integerForKey:@"LatestCrashReportMonth"]: 0;
    int day   = !previouslySeen ? [[NSUserDefaults standardUserDefaults] integerForKey:@"LatestCrashReportDay"]  : 0;
    int hours = !previouslySeen ? [[NSUserDefaults standardUserDefaults] integerForKey:@"LatestCrashReportHours"]: 0;

    while (fname = [direnum nextObject])
    {
        [direnum skipDescendents];
        if([fname hasPrefix:@"VLC"] && [fname hasSuffix:@"crash"])
        {
            NSArray * compo = [fname componentsSeparatedByString:@"_"];
            if( [compo count] < 3 ) continue;
            compo = [[compo objectAtIndex:1] componentsSeparatedByString:@"-"];
            if( [compo count] < 4 ) continue;

            // Dooh. ugly.
            if( year < [[compo objectAtIndex:0] intValue] ||
                (year ==[[compo objectAtIndex:0] intValue] &&
                 (month < [[compo objectAtIndex:1] intValue] ||
                  (month ==[[compo objectAtIndex:1] intValue] &&
                   (day   < [[compo objectAtIndex:2] intValue] ||
                    (day   ==[[compo objectAtIndex:2] intValue] &&
                      hours < [[compo objectAtIndex:3] intValue] ))))))
            {
                year  = [[compo objectAtIndex:0] intValue];
                month = [[compo objectAtIndex:1] intValue];
                day   = [[compo objectAtIndex:2] intValue];
                hours = [[compo objectAtIndex:3] intValue];
                latestLog = [crashReporter stringByAppendingPathComponent:fname];
            }
        }
    }

    if(!(latestLog && [[NSFileManager defaultManager] fileExistsAtPath:latestLog]))
        return nil;

    if( !previouslySeen )
    {
        [[NSUserDefaults standardUserDefaults] setInteger:year  forKey:@"LatestCrashReportYear"];
        [[NSUserDefaults standardUserDefaults] setInteger:month forKey:@"LatestCrashReportMonth"];
        [[NSUserDefaults standardUserDefaults] setInteger:day   forKey:@"LatestCrashReportDay"];
        [[NSUserDefaults standardUserDefaults] setInteger:hours forKey:@"LatestCrashReportHours"];
    }
    return latestLog;
}

- (NSString *)latestCrashLogPath
{
    return [self latestCrashLogPathPreviouslySeen:YES];
}

- (void)lookForCrashLog
{
    NSAutoreleasePool *o_pool = [[NSAutoreleasePool alloc] init];
    // This pref key doesn't exists? this VLC is an upgrade, and this crash log come from previous version
    BOOL areCrashLogsTooOld = ![[NSUserDefaults standardUserDefaults] integerForKey:@"LatestCrashReportYear"];
    NSString * latestLog = [self latestCrashLogPathPreviouslySeen:NO];
    if( latestLog && !areCrashLogsTooOld )
        [NSApp runModalForWindow: o_crashrep_win];
    [o_pool release];
}

- (IBAction)crashReporterAction:(id)sender
{
    if( sender == o_crashrep_send_btn )
        [self sendCrashLog:[NSString stringWithContentsOfFile: [self latestCrashLogPath] encoding: NSUTF8StringEncoding error: NULL] withUserComment: [o_crashrep_fld string]];

    [NSApp stopModal];
    [o_crashrep_win orderOut: sender];
}

- (IBAction)openCrashLog:(id)sender
{
    NSString * latestLog = [self latestCrashLogPath];
    if( latestLog )
    {
        [[NSWorkspace sharedWorkspace] openFile: latestLog withApplication: @"Console"];
    }
    else
    {
        NSBeginInformationalAlertSheet(_NS("No CrashLog found"), _NS("Continue"), nil, nil, o_msgs_panel, self, NULL, NULL, nil, _NS("Couldn't find any trace of a previous crash.") );
    }
}

#pragma mark -
#pragma mark Remove old prefs

- (void)_removeOldPreferences
{
    static NSString * kVLCPreferencesVersion = @"VLCPreferencesVersion";
    static const int kCurrentPreferencesVersion = 2;
    int version = [[NSUserDefaults standardUserDefaults] integerForKey:kVLCPreferencesVersion];
    if( version >= kCurrentPreferencesVersion ) return;

    if( version == 1 )
    {
        [[NSUserDefaults standardUserDefaults] setInteger:kCurrentPreferencesVersion forKey:kVLCPreferencesVersion];
        [[NSUserDefaults standardUserDefaults] synchronize];

        if (![[VLCCoreInteraction sharedInstance] fixPreferences])
            return;
        else
            config_SaveConfigFile( VLCIntf ); // we need to do manually, since we won't quit libvlc cleanly
    }
    else
    {
        NSArray *libraries = NSSearchPathForDirectoriesInDomains(NSLibraryDirectory,
            NSUserDomainMask, YES);
        if( !libraries || [libraries count] == 0) return;
        NSString * preferences = [[libraries objectAtIndex:0] stringByAppendingPathComponent:@"Preferences"];

        /* File not found, don't attempt anything */
        if(![[NSFileManager defaultManager] fileExistsAtPath:[preferences stringByAppendingPathComponent:@"org.videolan.vlc"]] &&
           ![[NSFileManager defaultManager] fileExistsAtPath:[preferences stringByAppendingPathComponent:@"org.videolan.vlc.plist"]] )
        {
            [[NSUserDefaults standardUserDefaults] setInteger:kCurrentPreferencesVersion forKey:kVLCPreferencesVersion];
            return;
        }

        int res = NSRunInformationalAlertPanel(_NS("Remove old preferences?"),
                    _NS("We just found an older version of VLC's preferences files."),
                    _NS("Move To Trash and Relaunch VLC"), _NS("Ignore"), nil, nil);
        if( res != NSOKButton )
        {
            [[NSUserDefaults standardUserDefaults] setInteger:kCurrentPreferencesVersion forKey:kVLCPreferencesVersion];
            return;
        }

        NSArray * ourPreferences = [NSArray arrayWithObjects:@"org.videolan.vlc.plist", @"VLC", @"org.videolan.vlc", nil];

        /* Move the file to trash so that user can find them later */
        [[NSWorkspace sharedWorkspace] performFileOperation:NSWorkspaceRecycleOperation source:preferences destination:nil files:ourPreferences tag:0];

        /* really reset the defaults from now on */
        [NSUserDefaults resetStandardUserDefaults];

        [[NSUserDefaults standardUserDefaults] setInteger:kCurrentPreferencesVersion forKey:kVLCPreferencesVersion];
        [[NSUserDefaults standardUserDefaults] synchronize];
    }

    /* Relaunch now */
    const char * path = [[[NSBundle mainBundle] executablePath] UTF8String];

    /* For some reason we need to fork(), not just execl(), which reports a ENOTSUP then. */
    if(fork() != 0)
    {
        exit(0);
        return;
    }
    execl(path, path, NULL);
}

#pragma mark -
#pragma mark Errors, warnings and messages
- (IBAction)updateMessagesPanel:(id)sender
{
    [self windowDidBecomeKey:nil];
}

- (IBAction)showMessagesPanel:(id)sender
{
    [o_msgs_panel makeKeyAndOrderFront: sender];
}

- (void)windowDidBecomeKey:(NSNotification *)o_notification
{
    [o_msgs_table reloadData];
    [o_msgs_table scrollRowToVisible: [o_msg_arr count] - 1];
}

- (NSInteger)numberOfRowsInTableView:(NSTableView *)aTableView
{
    if (aTableView == o_msgs_table)
        return [o_msg_arr count];
    return 0;
}

- (id)tableView:(NSTableView *)aTableView objectValueForTableColumn:(NSTableColumn *)aTableColumn row:(NSInteger)rowIndex
{
    NSMutableAttributedString *result = NULL;

    [o_msg_lock lock];
    if( rowIndex < [o_msg_arr count] )
        result = [o_msg_arr objectAtIndex: rowIndex];
    [o_msg_lock unlock];

    if( result != NULL )
        return result;
    else
        return @"";
}

- (void)processReceivedlibvlcMessage:(const msg_item_t *) item ofType: (int)i_type withStr: (char *)str
{
    if (o_msg_arr)
    {
        NSColor *o_white = [NSColor whiteColor];
        NSColor *o_red = [NSColor redColor];
        NSColor *o_yellow = [NSColor yellowColor];
        NSColor *o_gray = [NSColor grayColor];
        NSString * firstString, * secondString;

        NSColor * pp_color[4] = { o_white, o_red, o_yellow, o_gray };
        static const char * ppsz_type[4] = { ": ", " error: ", " warning: ", " debug: " };

        NSDictionary *o_attr;
        NSMutableAttributedString *o_msg_color;

        [o_msg_lock lock];

        if( [o_msg_arr count] > 600 )
        {
            [o_msg_arr removeObjectAtIndex: 0];
            [o_msg_arr removeObjectAtIndex: 1];
        }
        if (!item->psz_module)
            return;
        if (!str)
            return;

        firstString = [NSString stringWithFormat:@"%s%s", item->psz_module, ppsz_type[i_type]];
        secondString = [NSString stringWithFormat:@"%@%s\n", firstString, str];

        o_attr = [NSDictionary dictionaryWithObject: pp_color[i_type]  forKey: NSForegroundColorAttributeName];
        o_msg_color = [[NSMutableAttributedString alloc] initWithString: secondString attributes: o_attr];
        o_attr = [NSDictionary dictionaryWithObject: pp_color[3] forKey: NSForegroundColorAttributeName];
        [o_msg_color setAttributes: o_attr range: NSMakeRange( 0, [firstString length] )];
        [o_msg_arr addObject: [o_msg_color autorelease]];

        b_msg_arr_changed = YES;
        [o_msg_lock unlock];
    }
}

- (IBAction)saveDebugLog:(id)sender
{
    NSSavePanel * saveFolderPanel = [[NSSavePanel alloc] init];

    [saveFolderPanel setCanSelectHiddenExtension: NO];
    [saveFolderPanel setCanCreateDirectories: YES];
    [saveFolderPanel setAllowedFileTypes: [NSArray arrayWithObject:@"rtfd"]];
    [saveFolderPanel beginSheetForDirectory:nil file: [NSString stringWithFormat: _NS("VLC Debug Log (%s).rtfd"), VERSION_MESSAGE] modalForWindow: o_msgs_panel modalDelegate:self didEndSelector:@selector(saveDebugLogAsRTF:returnCode:contextInfo:) contextInfo:nil];
}

- (void)saveDebugLogAsRTF: (NSSavePanel *)sheet returnCode: (int)returnCode contextInfo: (void *)contextInfo
{
    BOOL b_returned;
    if( returnCode == NSOKButton )
    {
        NSUInteger count = [o_msg_arr count];
        NSMutableAttributedString * string = [[NSMutableAttributedString alloc] init];
        for (NSUInteger i = 0; i < count; i++)
        {
            [string appendAttributedString: [o_msg_arr objectAtIndex: i]];
        }
        b_returned = [[string RTFDFileWrapperFromRange:NSMakeRange( 0, [string length] ) documentAttributes:[NSDictionary dictionaryWithObject: NSRTFDTextDocumentType forKey: NSDocumentTypeDocumentAttribute]] writeToFile:[[sheet URL] path] atomically:YES updateFilenames:NO];
        [string release];

        if(! b_returned )
            msg_Warn( p_intf, "Error while saving the debug log" );
    }
}

#pragma mark -
#pragma mark Playlist toggling

- (void)updateTogglePlaylistState
{
    [[self playlist] outlineViewSelectionDidChange: NULL];
}

#pragma mark -

@end

@implementation VLCMain (Internal)

- (void)handlePortMessage:(NSPortMessage *)o_msg
{
    id ** val;
    NSData * o_data;
    NSValue * o_value;
    NSInvocation * o_inv;
    NSConditionLock * o_lock;

    o_data = [[o_msg components] lastObject];
    o_inv = *((NSInvocation **)[o_data bytes]);
    [o_inv getArgument: &o_value atIndex: 2];
    val = (id **)[o_value pointerValue];
    [o_inv setArgument: val[1] atIndex: 2];
    o_lock = *(val[0]);

    [o_lock lock];
    [o_inv invoke];
    [o_lock unlockWithCondition: 1];
}
- (void)resetMediaKeyJump
{
    b_mediakeyJustJumped = NO;
}
- (void)coreChangedMediaKeySupportSetting: (NSNotification *)o_notification
{
    b_mediaKeySupport = var_InheritBool( VLCIntf, "macosx-mediakeys" );
    if (b_mediaKeySupport)
    {
        if (!o_mediaKeyController)
            o_mediaKeyController = [[SPMediaKeyTap alloc] initWithDelegate:self];
        [o_mediaKeyController startWatchingMediaKeys];
    }
    else if (!b_mediaKeySupport && o_mediaKeyController)
        [o_mediaKeyController stopWatchingMediaKeys];
}

@end

/*****************************************************************************
 * VLCApplication interface
 *****************************************************************************/

@implementation VLCApplication
// when user selects the quit menu from dock it sends a terminate:
// but we need to send a stop: to properly exits libvlc.
// However, we are not able to change the action-method sent by this standard menu item.
// thus we override terminate: to send a stop:
// see [af97f24d528acab89969d6541d83f17ce1ecd580] that introduced the removal of setjmp() and longjmp()
- (void)terminate:(id)sender
{
    [self activateIgnoringOtherApps:YES];
    [self stop:sender];
}

@end
