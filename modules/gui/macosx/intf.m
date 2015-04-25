/*****************************************************************************
 * intf.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2002-2013 VLC authors and VideoLAN
 * $Id: 05779af18d607819fc66312f7221f512c8b340d1 $
 *
 * Authors: Jon Lech Johansen <jon-vl@nanocrew.net>
 *          Derk-Jan Hartman <hartman at videolan.org>
 *          Felix Paul Kühne <fkuehne at videolan dot org>
 *          Pierre d'Herbemont <pdherbemont # videolan org>
 *          David Fuhrmann <david dot fuhrmann at googlemail dot com>
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
#include <string.h>
#include <vlc_common.h>
#include <vlc_keys.h>
#include <vlc_dialog.h>
#include <vlc_url.h>
#include <vlc_modules.h>
#include <vlc_plugin.h>
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
#import "ExtensionsManager.h"
#import "BWQuincyManager.h"
#import "ControlsBar.h"

#import "VideoEffects.h"
#import "AudioEffects.h"

#import <Sparkle/Sparkle.h>                 /* we're the update delegate */

#import "iTunes.h"
#import "Spotify.h"

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
static void Run (intf_thread_t *p_intf);

static void updateProgressPanel (void *, const char *, float);
static bool checkProgressPanel (void *);
static void destroyProgressPanel (void *);

static int InputEvent(vlc_object_t *, const char *,
                      vlc_value_t, vlc_value_t, void *);
static int PLItemChanged(vlc_object_t *, const char *,
                         vlc_value_t, vlc_value_t, void *);
static int PLItemUpdated(vlc_object_t *, const char *,
                         vlc_value_t, vlc_value_t, void *);
static int PlaylistUpdated(vlc_object_t *, const char *,
                           vlc_value_t, vlc_value_t, void *);
static int PlaybackModeUpdated(vlc_object_t *, const char *,
                               vlc_value_t, vlc_value_t, void *);
static int VolumeUpdated(vlc_object_t *, const char *,
                         vlc_value_t, vlc_value_t, void *);
static int BossCallback(vlc_object_t *, const char *,
                         vlc_value_t, vlc_value_t, void *);

#pragma mark -
#pragma mark VLC Interface Object Callbacks

static bool b_intf_starting = false;
static vlc_mutex_t start_mutex = VLC_STATIC_MUTEX;
static vlc_cond_t  start_cond = VLC_STATIC_COND;

/*****************************************************************************
 * OpenIntf: initialize interface
 *****************************************************************************/
int OpenIntf (vlc_object_t *p_this)
{
    NSAutoreleasePool *o_pool = [[NSAutoreleasePool alloc] init];
    [VLCApplication sharedApplication];

    intf_thread_t *p_intf = (intf_thread_t*) p_this;
    msg_Dbg(p_intf, "Starting macosx interface");
    Run(p_intf);

    [o_pool release];
    return VLC_SUCCESS;
}

static NSLock * o_vout_provider_lock = nil;

static int WindowControl(vout_window_t *, int i_query, va_list);

int WindowOpen(vout_window_t *p_wnd, const vout_window_cfg_t *cfg)
{
    msg_Dbg(p_wnd, "Opening video window");

    /*
     * HACK: Wait 200ms for the interface to come up.
     * WindowOpen might be called before the mac intf is started. Lets wait until OpenIntf gets called
     * and does basic initialization. Enqueuing the vout controller request into the main loop later on
     * ensures that the actual window is created after the interface is fully initialized
     * (applicationDidFinishLaunching).
     *
     * Timeout is needed as the mac intf is not always started at all.
     */
    mtime_t deadline = mdate() + 200000;
    vlc_mutex_lock(&start_mutex);
    while (!b_intf_starting) {
        if (vlc_cond_timedwait(&start_cond, &start_mutex, deadline)) {
            break; // timeout
        }
    }

    if (!b_intf_starting) {
        msg_Err(p_wnd, "Cannot create vout as Mac OS X interface was not found");
        vlc_mutex_unlock(&start_mutex);
        return VLC_EGENERIC;
    }
    vlc_mutex_unlock(&start_mutex);

    NSAutoreleasePool *o_pool = [[NSAutoreleasePool alloc] init];

    NSRect proposedVideoViewPosition = NSMakeRect(cfg->x, cfg->y, cfg->width, cfg->height);

    [o_vout_provider_lock lock];
    VLCVoutWindowController *o_vout_controller = [[VLCMain sharedInstance] voutController];
    if (!o_vout_controller) {
        [o_vout_provider_lock unlock];
        [o_pool release];
        return VLC_EGENERIC;
    }

    SEL sel = @selector(setupVoutForWindow:withProposedVideoViewPosition:);
    NSInvocation *inv = [NSInvocation invocationWithMethodSignature:[o_vout_controller methodSignatureForSelector:sel]];
    [inv setTarget:o_vout_controller];
    [inv setSelector:sel];
    [inv setArgument:&p_wnd atIndex:2]; // starting at 2!
    [inv setArgument:&proposedVideoViewPosition atIndex:3];

    [inv performSelectorOnMainThread:@selector(invoke) withObject:nil
                       waitUntilDone:YES];

    VLCVoutView *videoView = nil;
    [inv getReturnValue:&videoView];

    // this method is not supposed to fail
    assert(videoView != nil);

    msg_Dbg(VLCIntf, "returning videoview with proposed position x=%i, y=%i, width=%i, height=%i", cfg->x, cfg->y, cfg->width, cfg->height);
    p_wnd->handle.nsobject = videoView;

    [o_vout_provider_lock unlock];

    p_wnd->control = WindowControl;

    [o_pool release];
    return VLC_SUCCESS;
}

static int WindowControl(vout_window_t *p_wnd, int i_query, va_list args)
{
    NSAutoreleasePool *o_pool = [[NSAutoreleasePool alloc] init];

    [o_vout_provider_lock lock];
    VLCVoutWindowController *o_vout_controller = [[VLCMain sharedInstance] voutController];
    if (!o_vout_controller) {
        [o_vout_provider_lock unlock];
        [o_pool release];
        return VLC_EGENERIC;
    }

    switch(i_query) {
        case VOUT_WINDOW_SET_STATE:
        {
            unsigned i_state = va_arg(args, unsigned);

            if (i_state & VOUT_WINDOW_STATE_BELOW)
            {
                msg_Dbg(p_wnd, "Ignore change to VOUT_WINDOW_STATE_BELOW");
                goto out;
            }

            NSInteger i_cooca_level = NSNormalWindowLevel;
            if (i_state & VOUT_WINDOW_STATE_ABOVE)
                i_cooca_level = NSStatusWindowLevel;

            SEL sel = @selector(setWindowLevel:forWindow:);
            NSInvocation *inv = [NSInvocation invocationWithMethodSignature:[o_vout_controller methodSignatureForSelector:sel]];
            [inv setTarget:o_vout_controller];
            [inv setSelector:sel];
            [inv setArgument:&i_cooca_level atIndex:2]; // starting at 2!
            [inv setArgument:&p_wnd atIndex:3];
            [inv performSelectorOnMainThread:@selector(invoke) withObject:nil
                               waitUntilDone:NO];

            break;
        }
        case VOUT_WINDOW_SET_SIZE:
        {
            unsigned int i_width  = va_arg(args, unsigned int);
            unsigned int i_height = va_arg(args, unsigned int);

            NSSize newSize = NSMakeSize(i_width, i_height);
            SEL sel = @selector(setNativeVideoSize:forWindow:);
            NSInvocation *inv = [NSInvocation invocationWithMethodSignature:[o_vout_controller methodSignatureForSelector:sel]];
            [inv setTarget:o_vout_controller];
            [inv setSelector:sel];
            [inv setArgument:&newSize atIndex:2]; // starting at 2!
            [inv setArgument:&p_wnd atIndex:3];
            [inv performSelectorOnMainThread:@selector(invoke) withObject:nil
                               waitUntilDone:NO];

            break;
        }
        case VOUT_WINDOW_SET_FULLSCREEN:
        {
            if (var_InheritBool(VLCIntf, "video-wallpaper")) {
                msg_Dbg(p_wnd, "Ignore fullscreen event as video-wallpaper is on");
                goto out;
            }

            int i_full = va_arg(args, int);
            BOOL b_animation = YES;

            SEL sel = @selector(setFullscreen:forWindow:withAnimation:);
            NSInvocation *inv = [NSInvocation invocationWithMethodSignature:[o_vout_controller methodSignatureForSelector:sel]];
            [inv setTarget:o_vout_controller];
            [inv setSelector:sel];
            [inv setArgument:&i_full atIndex:2]; // starting at 2!
            [inv setArgument:&p_wnd atIndex:3];
            [inv setArgument:&b_animation atIndex:4];
            [inv performSelectorOnMainThread:@selector(invoke) withObject:nil
                               waitUntilDone:NO];

            break;
        }
        default:
        {
            msg_Warn(p_wnd, "unsupported control query");
            [o_vout_provider_lock unlock];
            [o_pool release];
            return VLC_EGENERIC;
        }
    }

out:
    [o_vout_provider_lock unlock];
    [o_pool release];
    return VLC_SUCCESS;
}

void WindowClose(vout_window_t *p_wnd)
{
    NSAutoreleasePool *o_pool = [[NSAutoreleasePool alloc] init];

    [o_vout_provider_lock lock];
    VLCVoutWindowController *o_vout_controller = [[VLCMain sharedInstance] voutController];
    if (!o_vout_controller) {
        [o_vout_provider_lock unlock];
        [o_pool release];
        return;
    }

    [o_vout_controller performSelectorOnMainThread:@selector(removeVoutforDisplay:) withObject:[NSValue valueWithPointer:p_wnd] waitUntilDone:NO];
    [o_vout_provider_lock unlock];

    [o_pool release];
}

/* Used to abort the app.exec() on OSX after libvlc_Quit is called */
#include "../../../lib/libvlc_internal.h" /* libvlc_SetExitHandler */

static void QuitVLC( void *obj )
{
    [[VLCApplication sharedApplication] performSelectorOnMainThread:@selector(terminate:) withObject:nil waitUntilDone:NO];
}

/*****************************************************************************
 * Run: main loop
 *****************************************************************************/
static NSLock * o_appLock = nil;    // controls access to f_appExit

static void Run(intf_thread_t *p_intf)
{
    NSAutoreleasePool * o_pool = [[NSAutoreleasePool alloc] init];
    [VLCApplication sharedApplication];

    o_appLock = [[NSLock alloc] init];
    o_vout_provider_lock = [[NSLock alloc] init];

    libvlc_SetExitHandler(p_intf->p_libvlc, QuitVLC, p_intf);
    [[VLCMain sharedInstance] setIntf: p_intf];

    vlc_mutex_lock(&start_mutex);
    b_intf_starting = true;
    vlc_cond_signal(&start_cond);
    vlc_mutex_unlock(&start_mutex);

    [NSBundle loadNibNamed: @"MainMenu" owner: NSApp];

    [NSApp run];
    msg_Dbg(p_intf, "Run loop has been stopped");
    [[VLCMain sharedInstance] applicationWillTerminate:nil];
    [o_appLock release];
    [o_vout_provider_lock release];
    o_vout_provider_lock = nil;
    [o_pool release];

    raise(SIGTERM);
}

#pragma mark -
#pragma mark Variables Callback

static int InputEvent(vlc_object_t *p_this, const char *psz_var,
                       vlc_value_t oldval, vlc_value_t new_val, void *param)
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
            dispatch_async(dispatch_get_main_queue(), ^{
                [[[VLCMain sharedInstance] info] updateStatistics];
            });
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
            [[VLCMain sharedInstance] updateRecordState: var_GetBool(p_this, "record")];
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
            break;
    }

    [o_pool release];
    return VLC_SUCCESS;
}

static int PLItemChanged(vlc_object_t *p_this, const char *psz_var,
                         vlc_value_t oldval, vlc_value_t new_val, void *param)
{
    NSAutoreleasePool * o_pool = [[NSAutoreleasePool alloc] init];

    [[VLCMain sharedInstance] performSelectorOnMainThread:@selector(PlaylistItemChanged) withObject:nil waitUntilDone:NO];

    [o_pool release];
    return VLC_SUCCESS;
}

/**
 * Callback for item-change variable. Is triggered after update of duration or metadata.
 */
static int PLItemUpdated(vlc_object_t *p_this, const char *psz_var,
                         vlc_value_t oldval, vlc_value_t new_val, void *param)
{
    NSAutoreleasePool * o_pool = [[NSAutoreleasePool alloc] init];

    [[VLCMain sharedInstance] performSelectorOnMainThread:@selector(plItemUpdated) withObject:nil waitUntilDone:NO];

    [o_pool release];
    return VLC_SUCCESS;
}

static int PlaylistUpdated(vlc_object_t *p_this, const char *psz_var,
                         vlc_value_t oldval, vlc_value_t new_val, void *param)
{
    NSAutoreleasePool * o_pool = [[NSAutoreleasePool alloc] init];

    /* Avoid event queue flooding with playlistUpdated selectors, leading to UI freezes.
     * Therefore, only enqueue if no selector already enqueued.
     */
    VLCMain *o_main = [VLCMain sharedInstance];
    @synchronized(o_main) {
        if(![o_main playlistUpdatedSelectorInQueue]) {
            [o_main setPlaylistUpdatedSelectorInQueue:YES];
            [o_main performSelectorOnMainThread:@selector(playlistUpdated) withObject:nil waitUntilDone:NO];
        }
    }

    [o_pool release];
    return VLC_SUCCESS;
}

static int PlaybackModeUpdated(vlc_object_t *p_this, const char *psz_var,
                         vlc_value_t oldval, vlc_value_t new_val, void *param)
{
    NSAutoreleasePool * o_pool = [[NSAutoreleasePool alloc] init];
    [[VLCMain sharedInstance] performSelectorOnMainThread:@selector(playbackModeUpdated) withObject:nil waitUntilDone:NO];

    [o_pool release];
    return VLC_SUCCESS;
}

static int VolumeUpdated(vlc_object_t *p_this, const char *psz_var,
                         vlc_value_t oldval, vlc_value_t new_val, void *param)
{
    NSAutoreleasePool * o_pool = [[NSAutoreleasePool alloc] init];
    [[VLCMain sharedInstance] performSelectorOnMainThread:@selector(updateVolume) withObject:nil waitUntilDone:NO];

    [o_pool release];
    return VLC_SUCCESS;
}

static int BossCallback(vlc_object_t *p_this, const char *psz_var,
                        vlc_value_t oldval, vlc_value_t new_val, void *param)
{
    NSAutoreleasePool * o_pool = [[NSAutoreleasePool alloc] init];

    [[VLCCoreInteraction sharedInstance] performSelectorOnMainThread:@selector(pause) withObject:nil waitUntilDone:NO];
    [[VLCApplication sharedApplication] hide:nil];

    [o_pool release];
    return VLC_SUCCESS;
}

/*****************************************************************************
 * ShowController: Callback triggered by the show-intf playlist variable
 * through the ShowIntf-control-intf, to let us show the controller-win;
 * usually when in fullscreen-mode
 *****************************************************************************/
static int ShowController(vlc_object_t *p_this, const char *psz_variable,
                     vlc_value_t old_val, vlc_value_t new_val, void *param)
{
    intf_thread_t * p_intf = VLCIntf;
    if (p_intf) {
        playlist_t * p_playlist = pl_Get(p_intf);
        BOOL b_fullscreen = var_GetBool(p_playlist, "fullscreen");
        if (b_fullscreen)
            [[VLCMain sharedInstance] performSelectorOnMainThread:@selector(showFullscreenController) withObject:nil waitUntilDone:NO];
        else if (!strcmp(psz_variable, "intf-show"))
            [[VLCMain sharedInstance] performSelectorOnMainThread:@selector(showMainWindow) withObject:nil waitUntilDone:NO];
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * DialogCallback: Callback triggered by the "dialog-*" variables
 * to let the intf display error and interaction dialogs
 *****************************************************************************/
static int DialogCallback(vlc_object_t *p_this, const char *type, vlc_value_t previous, vlc_value_t value, void *data)
{
    NSAutoreleasePool * o_pool = [[NSAutoreleasePool alloc] init];

    if ([[NSString stringWithUTF8String:type] isEqualToString: @"dialog-progress-bar"]) {
        /* the progress panel needs to update itself and therefore wants special treatment within this context */
        dialog_progress_bar_t *p_dialog = (dialog_progress_bar_t *)value.p_address;

        p_dialog->pf_update = updateProgressPanel;
        p_dialog->pf_check = checkProgressPanel;
        p_dialog->pf_destroy = destroyProgressPanel;
        p_dialog->p_sys = VLCIntf->p_libvlc;
    }

    NSValue *o_value = [NSValue valueWithPointer:value.p_address];
    [[[VLCMain sharedInstance] coreDialogProvider] performEventWithObject: o_value ofType: type];

    [o_pool release];
    return VLC_SUCCESS;
}

void updateProgressPanel (void *priv, const char *text, float value)
{
    NSAutoreleasePool *o_pool = [[NSAutoreleasePool alloc] init];

    NSString *o_txt = toNSStr(text);
    dispatch_async(dispatch_get_main_queue(), ^{
        [[[VLCMain sharedInstance] coreDialogProvider] updateProgressPanelWithText: o_txt andNumber: (double)(value * 1000.)];
    });

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
    return [[[VLCMain sharedInstance] coreDialogProvider] progressCancelled];
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

vout_thread_t *getVoutForActiveWindow(void)
{
    vout_thread_t *p_vout = nil;

    id currentWindow = [NSApp keyWindow];
    if ([currentWindow respondsToSelector:@selector(videoView)]) {
        VLCVoutView *videoView = [currentWindow videoView];
        if (videoView) {
            p_vout = [videoView voutThread];
        }
    }

    if (!p_vout)
        p_vout = getVout();

    return p_vout;
}

audio_output_t *getAout(void)
{
    intf_thread_t *p_intf = VLCIntf;
    if (!p_intf)
        return NULL;
    return playlist_GetAout(pl_Get(p_intf));
}

#pragma mark -
#pragma mark Private

@interface VLCMain () <BWQuincyManagerDelegate>
- (void)removeOldPreferences;
@end

@interface VLCMain (Internal)
- (void)resetMediaKeyJump;
- (void)coreChangedMediaKeySupportSetting: (NSNotification *)o_notification;
@end

/*****************************************************************************
 * VLCMain implementation
 *****************************************************************************/
@implementation VLCMain

@synthesize voutController=o_vout_controller;
@synthesize nativeFullscreenMode=b_nativeFullscreenMode;
@synthesize playlistUpdatedSelectorInQueue=b_playlist_updated_selector_in_queue;

#pragma mark -
#pragma mark Initialization

static VLCMain *_o_sharedMainInstance = nil;

+ (VLCMain *)sharedInstance
{
    return _o_sharedMainInstance ? _o_sharedMainInstance : [[self alloc] init];
}

- (id)init
{
    if (_o_sharedMainInstance) {
        [self dealloc];
        return _o_sharedMainInstance;
    } else
        _o_sharedMainInstance = [super init];

    p_intf = NULL;
    p_current_input = NULL;

    o_open = [[VLCOpen alloc] init];
    o_coredialogs = [[VLCCoreDialogProvider alloc] init];
    o_mainmenu = [[VLCMainMenu alloc] init];
    o_coreinteraction = [[VLCCoreInteraction alloc] init];
    o_eyetv = [[VLCEyeTVController alloc] init];

    /* announce our launch to a potential eyetv plugin */
    [[NSDistributedNotificationCenter defaultCenter] postNotificationName: @"VLCOSXGUIInit"
                                                                   object: @"VLCEyeTVSupport"
                                                                 userInfo: NULL
                                                       deliverImmediately: YES];

    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    NSDictionary *appDefaults = [NSDictionary dictionaryWithObject:@"NO" forKey:@"LiveUpdateTheMessagesPanel"];
    [defaults registerDefaults:appDefaults];

    o_vout_controller = [[VLCVoutWindowController alloc] init];

    informInputChangedQueue = dispatch_queue_create("org.videolan.vlc.inputChangedQueue", DISPATCH_QUEUE_SERIAL);

    return _o_sharedMainInstance;
}

- (void)setIntf: (intf_thread_t *)p_mainintf
{
    p_intf = p_mainintf;
}

- (intf_thread_t *)intf
{
    return p_intf;
}

- (void)awakeFromNib
{
    playlist_t *p_playlist;
    if (!p_intf) return;
    var_Create(p_intf, "intf-change", VLC_VAR_BOOL);

    /* Check if we already did this once. Opening the other nibs calls it too,
     because VLCMain is the owner */
    if (nib_main_loaded)
        return;

    p_playlist = pl_Get(p_intf);

    var_AddCallback(p_intf->p_libvlc, "intf-toggle-fscontrol", ShowController, self);
    var_AddCallback(p_intf->p_libvlc, "intf-show", ShowController, self);
    var_AddCallback(p_intf->p_libvlc, "intf-boss", BossCallback, self);
    var_AddCallback(p_playlist, "item-change", PLItemUpdated, self);
    var_AddCallback(p_playlist, "activity", PLItemChanged, self);
    var_AddCallback(p_playlist, "leaf-to-parent", PlaylistUpdated, self);
    var_AddCallback(p_playlist, "playlist-item-append", PlaylistUpdated, self);
    var_AddCallback(p_playlist, "playlist-item-deleted", PlaylistUpdated, self);
    var_AddCallback(p_playlist, "random", PlaybackModeUpdated, self);
    var_AddCallback(p_playlist, "repeat", PlaybackModeUpdated, self);
    var_AddCallback(p_playlist, "loop", PlaybackModeUpdated, self);
    var_AddCallback(p_playlist, "volume", VolumeUpdated, self);
    var_AddCallback(p_playlist, "mute", VolumeUpdated, self);

    if (!OSX_SNOW_LEOPARD) {
        if ([NSApp currentSystemPresentationOptions] & NSApplicationPresentationFullScreen)
            var_SetBool(p_playlist, "fullscreen", YES);
    }

    /* load our Shared Dialogs nib */
    [NSBundle loadNibNamed:@"SharedDialogs" owner: NSApp];

    /* subscribe to various interactive dialogues */
    var_Create(p_intf, "dialog-error", VLC_VAR_ADDRESS);
    var_AddCallback(p_intf, "dialog-error", DialogCallback, self);
    var_Create(p_intf, "dialog-critical", VLC_VAR_ADDRESS);
    var_AddCallback(p_intf, "dialog-critical", DialogCallback, self);
    var_Create(p_intf, "dialog-login", VLC_VAR_ADDRESS);
    var_AddCallback(p_intf, "dialog-login", DialogCallback, self);
    var_Create(p_intf, "dialog-question", VLC_VAR_ADDRESS);
    var_AddCallback(p_intf, "dialog-question", DialogCallback, self);
    var_Create(p_intf, "dialog-progress-bar", VLC_VAR_ADDRESS);
    var_AddCallback(p_intf, "dialog-progress-bar", DialogCallback, self);
    dialog_Register(p_intf);

    /* init Apple Remote support */
    o_remote = [[AppleRemote alloc] init];
    [o_remote setClickCountEnabledButtons: kRemoteButtonPlay];
    [o_remote setDelegate: _o_sharedMainInstance];

    /* yeah, we are done */
    b_nativeFullscreenMode = NO;
#ifdef MAC_OS_X_VERSION_10_7
    if (!OSX_SNOW_LEOPARD)
        b_nativeFullscreenMode = var_InheritBool(p_intf, "macosx-nativefullscreenmode");
#endif

    if (config_GetInt(VLCIntf, "macosx-icon-change")) {
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

    nib_main_loaded = TRUE;
}

- (void)applicationWillFinishLaunching:(NSNotification *)aNotification
{
    playlist_t * p_playlist = pl_Get(VLCIntf);
    PL_LOCK;
    items_at_launch = p_playlist->p_local_category->i_children;
    PL_UNLOCK;

    [NSBundle loadNibNamed:@"MainWindow" owner: self];

    // This cannot be called directly here, as the main loop is not running yet so it would have no effect.
    // So lets enqueue it into the loop for later execution.
    [o_mainwindow performSelector:@selector(makeKeyAndOrderFront:) withObject:nil afterDelay:0];

    [[SUUpdater sharedUpdater] setDelegate:self];
}

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification
{
    launched = YES;

    if (!p_intf)
        return;

    NSString *appVersion = [[[NSBundle mainBundle] infoDictionary] valueForKey: @"CFBundleVersion"];
    NSRange endRande = [appVersion rangeOfString:@"-"];
    if (endRande.location != NSNotFound)
        appVersion = [appVersion substringToIndex:endRande.location];

    BWQuincyManager *quincyManager = [BWQuincyManager sharedQuincyManager];
    [quincyManager setApplicationVersion:appVersion];
    [quincyManager setSubmissionURL:@"http://crash.videolan.org/crash_v200.php"];
    [quincyManager setDelegate:self];
    [quincyManager setCompanyName:@"VideoLAN"];

    [self updateCurrentlyUsedHotkeys];

    /* init media key support */
    b_mediaKeySupport = var_InheritBool(VLCIntf, "macosx-mediakeys");
    if (b_mediaKeySupport) {
        o_mediaKeyController = [[SPMediaKeyTap alloc] initWithDelegate:self];
        [[NSUserDefaults standardUserDefaults] registerDefaults:[NSDictionary dictionaryWithObjectsAndKeys:
                                                                 [SPMediaKeyTap defaultMediaKeyUserBundleIdentifiers], kMediaKeyUsingBundleIdentifiersDefaultsKey,
                                                                 nil]];
    }
    [[NSNotificationCenter defaultCenter] addObserver: self selector: @selector(coreChangedMediaKeySupportSetting:) name: @"VLCMediaKeySupportSettingChanged" object: nil];

    [self removeOldPreferences];

    /* Handle sleep notification */
    [[[NSWorkspace sharedWorkspace] notificationCenter] addObserver:self selector:@selector(computerWillSleep:)
           name:NSWorkspaceWillSleepNotification object:nil];

    /* update the main window */
    [o_mainwindow updateWindow];
    [o_mainwindow updateTimeSlider];
    [o_mainwindow updateVolumeSlider];

    /* Hack: Playlist is started before the interface.
     * Thus, call additional updaters as we might miss these events if posted before
     * the callbacks are registered.
     */
    [self PlaylistItemChanged];
    [self playbackModeUpdated];

    // respect playlist-autostart
    // note that PLAYLIST_PLAY will not stop any playback if already started
    playlist_t * p_playlist = pl_Get(VLCIntf);
    PL_LOCK;
    BOOL kidsAround = p_playlist->p_local_category->i_children != 0;
    if (kidsAround && var_GetBool(p_playlist, "playlist-autostart"))
        playlist_Control(p_playlist, PLAYLIST_PLAY, true);
    PL_UNLOCK;
}

/* don't allow a double termination call. If the user has
 * already invoked the quit then simply return this time. */
static bool f_appExit = false;

#pragma mark -
#pragma mark Termination

- (BOOL)isTerminating
{
    return f_appExit;
}

- (void)applicationWillTerminate:(NSNotification *)notification
{
    bool isTerminating;

    [o_appLock lock];
    isTerminating = f_appExit;
    f_appExit = true;
    [o_appLock unlock];

    if (isTerminating)
        return;

    [self resumeItunesPlayback:nil];

    if (notification == nil)
        [[NSNotificationCenter defaultCenter] postNotificationName: NSApplicationWillTerminateNotification object: nil];

    playlist_t * p_playlist = pl_Get(p_intf);

    /* save current video and audio profiles */
    [[VLCVideoEffects sharedInstance] saveCurrentProfile];
    [[VLCAudioEffects sharedInstance] saveCurrentProfile];

    /* Save some interface state in configuration, at module quit */
    config_PutInt(p_intf, "random", var_GetBool(p_playlist, "random"));
    config_PutInt(p_intf, "loop", var_GetBool(p_playlist, "loop"));
    config_PutInt(p_intf, "repeat", var_GetBool(p_playlist, "repeat"));

    msg_Dbg(p_intf, "Terminating");

    /* unsubscribe from the interactive dialogues */
    dialog_Unregister(p_intf);
    var_DelCallback(p_intf, "dialog-error", DialogCallback, self);
    var_DelCallback(p_intf, "dialog-critical", DialogCallback, self);
    var_DelCallback(p_intf, "dialog-login", DialogCallback, self);
    var_DelCallback(p_intf, "dialog-question", DialogCallback, self);
    var_DelCallback(p_intf, "dialog-progress-bar", DialogCallback, self);
    var_DelCallback(p_playlist, "item-change", PLItemUpdated, self);
    var_DelCallback(p_playlist, "activity", PLItemChanged, self);
    var_DelCallback(p_playlist, "leaf-to-parent", PlaylistUpdated, self);
    var_DelCallback(p_playlist, "playlist-item-append", PlaylistUpdated, self);
    var_DelCallback(p_playlist, "playlist-item-deleted", PlaylistUpdated, self);
    var_DelCallback(p_playlist, "random", PlaybackModeUpdated, self);
    var_DelCallback(p_playlist, "repeat", PlaybackModeUpdated, self);
    var_DelCallback(p_playlist, "loop", PlaybackModeUpdated, self);
    var_DelCallback(p_playlist, "volume", VolumeUpdated, self);
    var_DelCallback(p_playlist, "mute", VolumeUpdated, self);
    var_DelCallback(p_intf->p_libvlc, "intf-toggle-fscontrol", ShowController, self);
    var_DelCallback(p_intf->p_libvlc, "intf-show", ShowController, self);
    var_DelCallback(p_intf->p_libvlc, "intf-boss", BossCallback, self);

    if (p_current_input) {
        /* continue playback where you left off */
        [[self playlist] storePlaybackPositionForItem:p_current_input];

        var_DelCallback(p_current_input, "intf-event", InputEvent, [VLCMain sharedInstance]);
        vlc_object_release(p_current_input);
        p_current_input = NULL;
    }

    /* remove global observer watching for vout device changes correctly */
    [[NSNotificationCenter defaultCenter] removeObserver: self];

    [o_vout_provider_lock lock];
    // release before o_info!
    // closes all open vouts
    [o_vout_controller release];
    o_vout_controller = nil;
    [o_vout_provider_lock unlock];

    /* release some other objects here, because it isn't sure whether dealloc
     * will be called later on */
    if (o_sprefs)
        [o_sprefs release];

    if (o_prefs)
        [o_prefs release];

    [o_open release];

    if (o_info)
        [o_info release];

    if (o_wizard)
        [o_wizard release];

    if (!o_bookmarks)
        [o_bookmarks release];

    [o_coredialogs release];
    [o_eyetv release];
    [o_remote release];

    /* unsubscribe from libvlc's debug messages */
    vlc_LogSet(p_intf->p_libvlc, NULL, NULL);

    [o_usedHotkeys release];
    o_usedHotkeys = NULL;

    [o_mediaKeyController release];

    /* write cached user defaults to disk */
    [[NSUserDefaults standardUserDefaults] synchronize];

    [o_mainmenu release];
    [o_coreinteraction release];

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

/* don't be enthusiastic about an update if we currently play a video */
- (BOOL)updaterMayCheckForUpdates:(SUUpdater *)bundle
{
    if ([self activeVideoPlayback])
        return NO;

    return YES;
}

#pragma mark -
#pragma mark Media Key support

-(void)mediaKeyTap:(SPMediaKeyTap*)keyTap receivedMediaKeyEvent:(NSEvent*)event
{
    if (b_mediaKeySupport) {
        assert([event type] == NSSystemDefined && [event subtype] == SPSystemDefinedEventMediaKeys);

        int keyCode = (([event data1] & 0xFFFF0000) >> 16);
        int keyFlags = ([event data1] & 0x0000FFFF);
        int keyState = (((keyFlags & 0xFF00) >> 8)) == 0xA;
        int keyRepeat = (keyFlags & 0x1);

        if (keyCode == NX_KEYTYPE_PLAY && keyState == 0)
            [[VLCCoreInteraction sharedInstance] playOrPause];

        if ((keyCode == NX_KEYTYPE_FAST || keyCode == NX_KEYTYPE_NEXT) && !b_mediakeyJustJumped) {
            if (keyState == 0 && keyRepeat == 0)
                [[VLCCoreInteraction sharedInstance] next];
            else if (keyRepeat == 1) {
                [[VLCCoreInteraction sharedInstance] forwardShort];
                b_mediakeyJustJumped = YES;
                [self performSelector:@selector(resetMediaKeyJump)
                           withObject: NULL
                           afterDelay:0.25];
            }
        }

        if ((keyCode == NX_KEYTYPE_REWIND || keyCode == NX_KEYTYPE_PREVIOUS) && !b_mediakeyJustJumped) {
            if (keyState == 0 && keyRepeat == 0)
                [[VLCCoreInteraction sharedInstance] previous];
            else if (keyRepeat == 1) {
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
    if (!p_intf)
        return;
    if (var_InheritBool(p_intf, "macosx-appleremote") == YES)
        [o_remote startListening: self];
}
- (void)applicationDidResignActive:(NSNotification *)aNotification
{
    if (!p_intf)
        return;
    [o_remote stopListening: self];
}

/* Triggered when the computer goes to sleep */
- (void)computerWillSleep: (NSNotification *)notification
{
    [[VLCCoreInteraction sharedInstance] pause];
}

#pragma mark -
#pragma mark File opening over dock icon

- (void)application:(NSApplication *)o_app openFiles:(NSArray *)o_names
{
    if (launched == NO) {
        if (items_at_launch) {
            int items = [o_names count];
            if (items > items_at_launch)
                items_at_launch = 0;
            else
                items_at_launch -= items;
            return;
        }
    }

    char *psz_uri = vlc_path2uri([[o_names objectAtIndex:0] UTF8String], NULL);

    // try to add file as subtitle
    if ([o_names count] == 1 && psz_uri) {
        input_thread_t * p_input = pl_CurrentInput(VLCIntf);
        if (p_input) {
            int i_result = input_AddSubtitleOSD(p_input, [[o_names objectAtIndex:0] UTF8String], true, true);
            vlc_object_release(p_input);
            if (i_result == VLC_SUCCESS) {
                free(psz_uri);
                return;
            }
        }
    }
    free(psz_uri);

    NSArray *o_sorted_names = [o_names sortedArrayUsingSelector: @selector(caseInsensitiveCompare:)];
    NSMutableArray *o_result = [NSMutableArray arrayWithCapacity: [o_sorted_names count]];
    for (NSUInteger i = 0; i < [o_sorted_names count]; i++) {
        psz_uri = vlc_path2uri([[o_sorted_names objectAtIndex:i] UTF8String], "file");
        if (!psz_uri)
            continue;

        NSDictionary *o_dic = [NSDictionary dictionaryWithObject:[NSString stringWithCString:psz_uri encoding:NSUTF8StringEncoding] forKey:@"ITEM_URL"];
        free(psz_uri);
        [o_result addObject: o_dic];
    }

    [o_playlist appendArray: o_result atPos: -1 enqueue: !config_GetInt(VLCIntf, "macosx-autoplay")];
}

/* When user click in the Dock icon our double click in the finder */
- (BOOL)applicationShouldHandleReopen:(NSApplication *)theApplication hasVisibleWindows:(BOOL)hasVisibleWindows
{
    if (!hasVisibleWindows)
        [o_mainwindow makeKeyAndOrderFront:self];

    return YES;
}

#pragma mark -
#pragma mark Apple Remote Control

/* Helper method for the remote control interface in order to trigger forward/backward and volume
   increase/decrease as long as the user holds the left/right, plus/minus button */
- (void) executeHoldActionForRemoteButton: (NSNumber*) buttonIdentifierNumber
{
    if (b_remote_button_hold) {
        switch([buttonIdentifierNumber intValue]) {
            case kRemoteButtonRight_Hold:
                [[VLCCoreInteraction sharedInstance] forward];
                break;
            case kRemoteButtonLeft_Hold:
                [[VLCCoreInteraction sharedInstance] backward];
                break;
            case kRemoteButtonVolume_Plus_Hold:
                if (p_intf)
                    var_SetInteger(p_intf->p_libvlc, "key-action", ACTIONID_VOL_UP);
                break;
            case kRemoteButtonVolume_Minus_Hold:
                if (p_intf)
                    var_SetInteger(p_intf->p_libvlc, "key-action", ACTIONID_VOL_DOWN);
                break;
        }
        if (b_remote_button_hold) {
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
    switch(buttonIdentifier) {
        case k2009RemoteButtonFullscreen:
            [[VLCCoreInteraction sharedInstance] toggleFullscreen];
            break;
        case k2009RemoteButtonPlay:
            [[VLCCoreInteraction sharedInstance] playOrPause];
            break;
        case kRemoteButtonPlay:
            if (count >= 2)
                [[VLCCoreInteraction sharedInstance] toggleFullscreen];
            else
                [[VLCCoreInteraction sharedInstance] playOrPause];
            break;
        case kRemoteButtonVolume_Plus:
            if (config_GetInt(VLCIntf, "macosx-appleremote-sysvol"))
                [NSSound increaseSystemVolume];
            else
                if (p_intf)
                    var_SetInteger(p_intf->p_libvlc, "key-action", ACTIONID_VOL_UP);
            break;
        case kRemoteButtonVolume_Minus:
            if (config_GetInt(VLCIntf, "macosx-appleremote-sysvol"))
                [NSSound decreaseSystemVolume];
            else
                if (p_intf)
                    var_SetInteger(p_intf->p_libvlc, "key-action", ACTIONID_VOL_DOWN);
            break;
        case kRemoteButtonRight:
            if (config_GetInt(VLCIntf, "macosx-appleremote-prevnext"))
                [[VLCCoreInteraction sharedInstance] forward];
            else
                [[VLCCoreInteraction sharedInstance] next];
            break;
        case kRemoteButtonLeft:
            if (config_GetInt(VLCIntf, "macosx-appleremote-prevnext"))
                [[VLCCoreInteraction sharedInstance] backward];
            else
                [[VLCCoreInteraction sharedInstance] previous];
            break;
        case kRemoteButtonRight_Hold:
        case kRemoteButtonLeft_Hold:
        case kRemoteButtonVolume_Plus_Hold:
        case kRemoteButtonVolume_Minus_Hold:
            /* simulate an event as long as the user holds the button */
            b_remote_button_hold = pressedDown;
            if (pressedDown) {
                NSNumber* buttonIdentifierNumber = [NSNumber numberWithInt:buttonIdentifier];
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
#pragma mark Key Shortcuts

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

    if (i_pressed_modifiers & NSControlKeyMask)
        val.i_int |= KEY_MODIFIER_CTRL;

    if (i_pressed_modifiers & NSAlternateKeyMask)
        val.i_int |= KEY_MODIFIER_ALT;

    if (i_pressed_modifiers & NSShiftKeyMask)
        val.i_int |= KEY_MODIFIER_SHIFT;

    if (i_pressed_modifiers & NSCommandKeyMask)
        val.i_int |= KEY_MODIFIER_COMMAND;

    NSString * characters = [o_event charactersIgnoringModifiers];
    if ([characters length] > 0) {
        key = [[characters lowercaseString] characterAtIndex: 0];

        /* handle Lion's default key combo for fullscreen-toggle in addition to our own hotkeys */
        if (key == 'f' && i_pressed_modifiers & NSControlKeyMask && i_pressed_modifiers & NSCommandKeyMask) {
            [[VLCCoreInteraction sharedInstance] toggleFullscreen];
            return YES;
        }

        if (!b_force) {
            switch(key) {
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

        val.i_int |= CocoaKeyToVLC(key);

        BOOL b_found_key = NO;
        for (NSUInteger i = 0; i < [o_usedHotkeys count]; i++) {
            NSString *str = [o_usedHotkeys objectAtIndex:i];
            unsigned int i_keyModifiers = [[VLCStringUtility sharedInstance] VLCModifiersToCocoa: str];

            if ([[characters lowercaseString] isEqualToString: [[VLCStringUtility sharedInstance] VLCKeyToString: str]] &&
               (i_keyModifiers & NSShiftKeyMask)     == (i_pressed_modifiers & NSShiftKeyMask) &&
               (i_keyModifiers & NSControlKeyMask)   == (i_pressed_modifiers & NSControlKeyMask) &&
               (i_keyModifiers & NSAlternateKeyMask) == (i_pressed_modifiers & NSAlternateKeyMask) &&
               (i_keyModifiers & NSCommandKeyMask)   == (i_pressed_modifiers & NSCommandKeyMask)) {
                b_found_key = YES;
                break;
            }
        }

        if (b_found_key) {
            var_SetInteger(p_intf->p_libvlc, "key-pressed", val.i_int);
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
    assert(p_main);
    unsigned confsize;
    module_config_t *p_config;

    p_config = module_config_get (p_main, &confsize);

    for (size_t i = 0; i < confsize; i++) {
        module_config_t *p_item = p_config + i;

        if (CONFIG_ITEM(p_item->i_type) && p_item->psz_name != NULL
           && !strncmp(p_item->psz_name , "key-", 4)
           && !EMPTY_STR(p_item->psz_text)) {
            if (p_item->value.psz)
                [o_tempArray addObject: [NSString stringWithUTF8String:p_item->value.psz]];
        }
    }
    module_config_free (p_config);

    if (o_usedHotkeys)
        [o_usedHotkeys release];
    o_usedHotkeys = [[NSArray alloc] initWithArray: o_tempArray copyItems: YES];
    [o_tempArray release];
}

#pragma mark -
#pragma mark Interface updaters
// This must be called on main thread
- (void)PlaylistItemChanged
{
    input_thread_t *p_input_changed = NULL;

    if (p_current_input && (p_current_input->b_dead || !vlc_object_alive(p_current_input))) {
        var_DelCallback(p_current_input, "intf-event", InputEvent, [VLCMain sharedInstance]);
        vlc_object_release(p_current_input);
        p_current_input = NULL;

        [o_mainmenu setRateControlsEnabled: NO];

        [[NSNotificationCenter defaultCenter] postNotificationName:VLCInputChangedNotification
                                                            object:nil];
    }
    else if (!p_current_input) {
        // object is hold here and released then it is dead
        p_current_input = playlist_CurrentInput(pl_Get(VLCIntf));
        if (p_current_input) {
            var_AddCallback(p_current_input, "intf-event", InputEvent, [VLCMain sharedInstance]);
            [self playbackStatusUpdated];
            [o_mainmenu setRateControlsEnabled: YES];

            if ([self activeVideoPlayback] && [[o_mainwindow videoView] isHidden]) {
                [o_mainwindow changePlaylistState: psPlaylistItemChangedEvent];
            }

            p_input_changed = vlc_object_hold(p_current_input);

            [[self playlist] continuePlaybackWhereYouLeftOff:p_current_input];

            [[NSNotificationCenter defaultCenter] postNotificationName:VLCInputChangedNotification
                                                                object:nil];
        }
    }

    [o_playlist updateRowSelection];
    [o_mainwindow updateWindow];
    [self updateDelays];
    [self updateMainMenu];

    /*
     * Due to constraints within NSAttributedString's main loop runtime handling
     * and other issues, we need to inform the extension manager on a separate thread.
     * The serial queue ensures that changed inputs are propagated in the same order as they arrive.
     */
    dispatch_async(informInputChangedQueue, ^{
        [[ExtensionsManager getInstance:p_intf] inputChanged:p_input_changed];
        if (p_input_changed)
            vlc_object_release(p_input_changed);
    });
}

- (void)plItemUpdated
{
    [o_mainwindow updateName];

    if (o_info != NULL)
        [o_info updateMetadata];
}

- (void)updateMainMenu
{
    [o_mainmenu setupMenus];
    [o_mainmenu updatePlaybackRate];
    [[VLCCoreInteraction sharedInstance] resetAtoB];
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
    // defer selector here (possibly another time) to ensure that keyWindow is set properly
    // (needed for NSApplicationDidBecomeActiveNotification)
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
    [o_mainwindow updateTimeSlider];
    [[VLCCoreInteraction sharedInstance] updateAtoB];
}

- (void)updateVolume
{
    [o_mainwindow updateVolumeSlider];
}

- (void)playlistUpdated
{
    @synchronized(self) {
        b_playlist_updated_selector_in_queue = NO;
    }

    [self playbackStatusUpdated];
    [o_playlist playlistUpdated];
    [o_mainwindow updateWindow];
    [o_mainwindow updateName];

    [[NSNotificationCenter defaultCenter] postNotificationName: @"VLCMediaKeySupportSettingChanged"
                                                        object: nil
                                                      userInfo: nil];
}

- (void)updateRecordState: (BOOL)b_value
{
    [o_mainmenu updateRecordState:b_value];
}

- (void)updateInfoandMetaPanel
{
    [o_playlist outlineViewSelectionDidChange:nil];
}

- (void)resumeItunesPlayback:(id)sender
{
    if (var_InheritInteger(p_intf, "macosx-control-itunes") > 1) {
        if (b_has_itunes_paused) {
            iTunesApplication *iTunesApp = (iTunesApplication *) [SBApplication applicationWithBundleIdentifier:@"com.apple.iTunes"];
            if (iTunesApp && [iTunesApp isRunning]) {
                if ([iTunesApp playerState] == iTunesEPlSPaused) {
                    msg_Dbg(p_intf, "unpausing iTunes");
                    [iTunesApp playpause];
                }
            }
        }

        if (b_has_spotify_paused) {
            SpotifyApplication *spotifyApp = (SpotifyApplication *) [SBApplication applicationWithBundleIdentifier:@"com.spotify.client"];
            if (spotifyApp) {
                if ([spotifyApp respondsToSelector:@selector(isRunning)] && [spotifyApp respondsToSelector:@selector(playerState)]) {
                    if ([spotifyApp isRunning] && [spotifyApp playerState] == kSpotifyPlayerStatePaused) {
                        msg_Dbg(p_intf, "unpausing Spotify");
                        [spotifyApp play];
                    }
                }
            }
        }
    }

    b_has_itunes_paused = NO;
    b_has_spotify_paused = NO;
    o_itunes_play_timer = nil;
}

- (void)playbackStatusUpdated
{
    int state = -1;
    if (p_current_input) {
        state = var_GetInteger(p_current_input, "state");
    }

    int i_control_itunes = var_InheritInteger(p_intf, "macosx-control-itunes");
    // cancel itunes timer if next item starts playing
    if (state > -1 && state != END_S && i_control_itunes > 0) {
        if (o_itunes_play_timer) {
            [o_itunes_play_timer invalidate];
            o_itunes_play_timer = nil;
        }
    }

    if (state == PLAYING_S) {
        if (i_control_itunes > 0) {
            // pause iTunes
            if (!b_has_itunes_paused) {
                iTunesApplication *iTunesApp = (iTunesApplication *) [SBApplication applicationWithBundleIdentifier:@"com.apple.iTunes"];
                if (iTunesApp && [iTunesApp isRunning]) {
                    if ([iTunesApp playerState] == iTunesEPlSPlaying) {
                        msg_Dbg(p_intf, "pausing iTunes");
                        [iTunesApp pause];
                        b_has_itunes_paused = YES;
                    }
                }
            }

            // pause Spotify
            if (!b_has_spotify_paused) {
                SpotifyApplication *spotifyApp = (SpotifyApplication *) [SBApplication applicationWithBundleIdentifier:@"com.spotify.client"];

                if (spotifyApp) {
                    if ([spotifyApp respondsToSelector:@selector(isRunning)] && [spotifyApp respondsToSelector:@selector(playerState)]) {
                        if ([spotifyApp isRunning] && [spotifyApp playerState] == kSpotifyPlayerStatePlaying) {
                            msg_Dbg(p_intf, "pausing Spotify");
                            [spotifyApp pause];
                            b_has_spotify_paused = YES;
                        }
                    }
                }
            }
        }

        /* Declare user activity.
         This wakes the display if it is off, and postpones display sleep according to the users system preferences
         Available from 10.7.3 */
#ifdef MAC_OS_X_VERSION_10_7
        if ([self activeVideoPlayback] && IOPMAssertionDeclareUserActivity)
        {
            CFStringRef reasonForActivity = CFStringCreateWithCString(kCFAllocatorDefault, _("VLC media playback"), kCFStringEncodingUTF8);
            IOPMAssertionDeclareUserActivity(reasonForActivity,
                                             kIOPMUserActiveLocal,
                                             &userActivityAssertionID);
            CFRelease(reasonForActivity);
        }
#endif

        /* prevent the system from sleeping */
        if (systemSleepAssertionID > 0) {
            msg_Dbg(VLCIntf, "releasing old sleep blocker (%i)" , systemSleepAssertionID);
            IOPMAssertionRelease(systemSleepAssertionID);
        }

        IOReturn success;
        /* work-around a bug in 10.7.4 and 10.7.5, so check for 10.7.x < 10.7.4, 10.8 and 10.6 */
        if ((NSAppKitVersionNumber >= 1115.2 && NSAppKitVersionNumber < 1138.45) || OSX_MOUNTAIN_LION || OSX_MAVERICKS || OSX_YOSEMITE || OSX_SNOW_LEOPARD) {
            CFStringRef reasonForActivity = CFStringCreateWithCString(kCFAllocatorDefault, _("VLC media playback"), kCFStringEncodingUTF8);
            if ([self activeVideoPlayback])
                success = IOPMAssertionCreateWithName(kIOPMAssertionTypeNoDisplaySleep, kIOPMAssertionLevelOn, reasonForActivity, &systemSleepAssertionID);
            else
                success = IOPMAssertionCreateWithName(kIOPMAssertionTypeNoIdleSleep, kIOPMAssertionLevelOn, reasonForActivity, &systemSleepAssertionID);
            CFRelease(reasonForActivity);
        } else {
            /* fall-back on the 10.5 mode, which also works on 10.7.4 and 10.7.5 */
            if ([self activeVideoPlayback])
                success = IOPMAssertionCreate(kIOPMAssertionTypeNoDisplaySleep, kIOPMAssertionLevelOn, &systemSleepAssertionID);
            else
                success = IOPMAssertionCreate(kIOPMAssertionTypeNoIdleSleep, kIOPMAssertionLevelOn, &systemSleepAssertionID);
        }

        if (success == kIOReturnSuccess)
            msg_Dbg(VLCIntf, "prevented sleep through IOKit (%i)", systemSleepAssertionID);
        else
            msg_Warn(VLCIntf, "failed to prevent system sleep through IOKit");

        [[self mainMenu] setPause];
        [o_mainwindow setPause];
    } else {
        [o_mainmenu setSubmenusEnabled: FALSE];
        [[self mainMenu] setPlay];
        [o_mainwindow setPlay];

        /* allow the system to sleep again */
        if (systemSleepAssertionID > 0) {
            msg_Dbg(VLCIntf, "releasing sleep blocker (%i)" , systemSleepAssertionID);
            IOPMAssertionRelease(systemSleepAssertionID);
        }

        if (state == END_S || state == -1) {
            /* continue playback where you left off */
            if (p_current_input)
                [[self playlist] storePlaybackPositionForItem:p_current_input];

            if (i_control_itunes > 0) {
                if (o_itunes_play_timer) {
                    [o_itunes_play_timer invalidate];
                }
                o_itunes_play_timer = [NSTimer scheduledTimerWithTimeInterval: 0.5
                                                                       target: self
                                                                     selector: @selector(resumeItunesPlayback:)
                                                                     userInfo: nil
                                                                      repeats: NO];
            }
        }
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
    playlist_t * p_playlist = pl_Get(VLCIntf);

    bool loop = var_GetBool(p_playlist, "loop");
    bool repeat = var_GetBool(p_playlist, "repeat");
    if (repeat) {
        [[o_mainwindow controlsBar] setRepeatOne];
        [o_mainmenu setRepeatOne];
    } else if (loop) {
        [[o_mainwindow controlsBar] setRepeatAll];
        [o_mainmenu setRepeatAll];
    } else {
        [[o_mainwindow controlsBar] setRepeatOff];
        [o_mainmenu setRepeatOff];
    }

    [[o_mainwindow controlsBar] setShuffle];
    [o_mainmenu setShuffle];
}

#pragma mark -
#pragma mark Window updater

- (void)setActiveVideoPlayback:(BOOL)b_value
{
    assert([NSThread isMainThread]);

    b_active_videoplayback = b_value;
    if (o_mainwindow) {
        [o_mainwindow setVideoplayEnabled];
    }

    // update sleep blockers
    [self playbackStatusUpdated];
}

#pragma mark -
#pragma mark Other objects getters

- (id)mainMenu
{
    return o_mainmenu;
}

- (VLCMainWindow *)mainWindow
{
    return o_mainwindow;
}

- (id)controls
{
    return o_controls;
}

- (id)bookmarks
{
    if (!o_bookmarks)
        o_bookmarks = [[VLCBookmarks alloc] init];

    if (!nib_bookmarks_loaded)
        nib_bookmarks_loaded = [NSBundle loadNibNamed:@"Bookmarks" owner: NSApp];

    return o_bookmarks;
}

- (id)open
{
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
    if (!o_prefs)
        o_prefs = [[VLCPrefs alloc] init];

    if (!nib_prefs_loaded)
        nib_prefs_loaded = [NSBundle loadNibNamed:@"Preferences" owner: NSApp];

    return o_prefs;
}

- (id)playlist
{
    return o_playlist;
}

- (id)info
{
    if (!o_info)
        o_info = [[VLCInfo alloc] init];

    if (! nib_info_loaded)
        nib_info_loaded = [NSBundle loadNibNamed:@"MediaInfo" owner: NSApp];

    return o_info;
}

- (id)wizard
{
    if (!o_wizard)
        o_wizard = [[VLCWizard alloc] init];

    if (!nib_wizard_loaded) {
        nib_wizard_loaded = [NSBundle loadNibNamed:@"Wizard" owner: NSApp];
        [o_wizard initStrings];
    }

    return o_wizard;
}

- (id)coreDialogProvider
{
    if (!nib_coredialogs_loaded) {
        nib_coredialogs_loaded = [NSBundle loadNibNamed:@"CoreDialogs" owner: NSApp];
    }

    return o_coredialogs;
}

- (id)eyeTVController
{
    return o_eyetv;
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
#pragma mark Remove old prefs


static NSString * kVLCPreferencesVersion = @"VLCPreferencesVersion";
static const int kCurrentPreferencesVersion = 3;

+ (void)initialize
{
    NSDictionary *appDefaults = [NSDictionary dictionaryWithObject:[NSNumber numberWithInt:kCurrentPreferencesVersion]
                                                            forKey:kVLCPreferencesVersion];

    [[NSUserDefaults standardUserDefaults] registerDefaults:appDefaults];
}

- (void)resetAndReinitializeUserDefaults
{
    // note that [NSUserDefaults resetStandardUserDefaults] will NOT correctly reset to the defaults

    NSString *appDomain = [[NSBundle mainBundle] bundleIdentifier];
    [[NSUserDefaults standardUserDefaults] removePersistentDomainForName:appDomain];

    // set correct version to avoid question about outdated config
    [[NSUserDefaults standardUserDefaults] setInteger:kCurrentPreferencesVersion forKey:kVLCPreferencesVersion];
    [[NSUserDefaults standardUserDefaults] synchronize];
}

- (void)removeOldPreferences
{
    NSUserDefaults * defaults = [NSUserDefaults standardUserDefaults];
    int version = [defaults integerForKey:kVLCPreferencesVersion];

    /*
     * Store version explicitely in file, for ease of debugging.
     * Otherwise, the value will be just defined at app startup,
     * as initialized above.
     */
    [defaults setInteger:version forKey:kVLCPreferencesVersion];
    if (version >= kCurrentPreferencesVersion)
        return;

    if (version == 1) {
        [defaults setInteger:kCurrentPreferencesVersion forKey:kVLCPreferencesVersion];
        [defaults synchronize];

        if (![[VLCCoreInteraction sharedInstance] fixPreferences])
            return;
        else
            config_SaveConfigFile(VLCIntf); // we need to do manually, since we won't quit libvlc cleanly
    } else if (version == 2) {
        /* version 2 (used by VLC 2.0.x and early versions of 2.1) can lead to exceptions within 2.1 or later
         * so we reset the OS X specific prefs here - in practice, no user will notice */
        [self resetAndReinitializeUserDefaults];

    } else {
        NSArray *libraries = NSSearchPathForDirectoriesInDomains(NSLibraryDirectory,
            NSUserDomainMask, YES);
        if (!libraries || [libraries count] == 0) return;
        NSString * preferences = [[libraries objectAtIndex:0] stringByAppendingPathComponent:@"Preferences"];

        int res = NSRunInformationalAlertPanel(_NS("Remove old preferences?"),
                    _NS("We just found an older version of VLC's preferences files."),
                    _NS("Move To Trash and Relaunch VLC"), _NS("Ignore"), nil, nil);
        if (res != NSOKButton) {
            [defaults setInteger:kCurrentPreferencesVersion forKey:kVLCPreferencesVersion];
            return;
        }

        // Do NOT add the current plist file here as this would conflict with caching.
        // Instead, just reset below.
        NSArray * ourPreferences = [NSArray arrayWithObjects:@"org.videolan.vlc", @"VLC", nil];

        /* Move the file to trash one by one. Using above array the method would stop after first file
           not found. */
        for (NSString *file in ourPreferences) {
            [[NSWorkspace sharedWorkspace] performFileOperation:NSWorkspaceRecycleOperation source:preferences destination:@"" files:[NSArray arrayWithObject:file] tag:nil];
        }

        [self resetAndReinitializeUserDefaults];
    }

    /* Relaunch now */
    const char * path = [[[NSBundle mainBundle] executablePath] UTF8String];

    /* For some reason we need to fork(), not just execl(), which reports a ENOTSUP then. */
    if (fork() != 0) {
        exit(0);
    }
    execl(path, path, NULL);
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

- (void)resetMediaKeyJump
{
    b_mediakeyJustJumped = NO;
}

- (void)coreChangedMediaKeySupportSetting: (NSNotification *)o_notification
{
    b_mediaKeySupport = var_InheritBool(VLCIntf, "macosx-mediakeys");
    if (b_mediaKeySupport && !o_mediaKeyController)
        o_mediaKeyController = [[SPMediaKeyTap alloc] initWithDelegate:self];

    if (b_mediaKeySupport && ([[[VLCMain sharedInstance] playlist] currentPlaylistRoot]->i_children > 0 ||
                              p_current_input)) {
        if (!b_mediaKeyTrapEnabled) {
            b_mediaKeyTrapEnabled = YES;
            msg_Dbg(p_intf, "Enable media key support");
            [o_mediaKeyController startWatchingMediaKeys];
        }
    } else {
        if (b_mediaKeyTrapEnabled) {
            b_mediaKeyTrapEnabled = NO;
            msg_Dbg(p_intf, "Disable media key support");
            [o_mediaKeyController stopWatchingMediaKeys];
        }
    }
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
