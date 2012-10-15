/*****************************************************************************
 * CoreInteraction.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2011-2012 Felix Paul Kühne
 * $Id: 7895eda23bd78de8c98d7107aa14aaf5f9ac75a6 $
 *
 * Authors: Felix Paul Kühne <fkuehne -at- videolan -dot- org>
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

#import "CoreInteraction.h"
#import "intf.h"
#import "open.h"
#import "playlist.h"
#import <vlc_playlist.h>
#import <vlc_input.h>
#import <vlc_keys.h>
#import <vlc_osd.h>
#import <vlc_aout_intf.h>
#import <vlc/vlc.h>
#import <vlc_strings.h>
#import <vlc_url.h>

@implementation VLCCoreInteraction
static VLCCoreInteraction *_o_sharedInstance = nil;

+ (VLCCoreInteraction *)sharedInstance
{
    return _o_sharedInstance ? _o_sharedInstance : [[self alloc] init];
}

#pragma mark -
#pragma mark Initialization

- (id)init
{
    if( _o_sharedInstance )
    {
        [self dealloc];
        return _o_sharedInstance;
    }
    else
    {
        _o_sharedInstance = [super init];
    }

    return _o_sharedInstance;
}

- (void)dealloc
{
    [[NSNotificationCenter defaultCenter] removeObserver: self];
    [super dealloc];
}

- (void)awakeFromNib
{
    [[NSNotificationCenter defaultCenter] addObserver: self 
                                             selector: @selector(applicationWillFinishLaunching:)
                                                 name: NSApplicationWillFinishLaunchingNotification
                                               object: nil];
}

#pragma mark -
#pragma mark Playback Controls

- (void)play
{
    input_thread_t * p_input;
    p_input = pl_CurrentInput(VLCIntf);
    if (p_input) {
        var_SetInteger(VLCIntf->p_libvlc, "key-action", ACTIONID_PLAY_PAUSE);
        vlc_object_release(p_input);
    } else {
        playlist_t * p_playlist = pl_Get(VLCIntf);
        bool empty;

        PL_LOCK;
        empty = playlist_IsEmpty(p_playlist);
        PL_UNLOCK;

        if ([[[VLCMain sharedInstance] playlist] isSelectionEmpty] && ([[[VLCMain sharedInstance] playlist] currentPlaylistRoot] == p_playlist->p_local_category || [[[VLCMain sharedInstance] playlist] currentPlaylistRoot] == p_playlist->p_ml_category))
            [[[VLCMain sharedInstance] open] openFileGeneric];
        else
            [[[VLCMain sharedInstance] playlist] playItem:nil];
    }
}

- (void)pause
{
    var_SetInteger( VLCIntf->p_libvlc, "key-action", ACTIONID_PAUSE );
}

- (void)stop
{
    var_SetInteger( VLCIntf->p_libvlc, "key-action", ACTIONID_STOP );
}

- (void)faster
{
    var_SetInteger( VLCIntf->p_libvlc, "key-action", ACTIONID_FASTER );
}

- (void)slower
{
    var_SetInteger( VLCIntf->p_libvlc, "key-action", ACTIONID_SLOWER );
}

- (void)normalSpeed
{
    var_SetInteger( VLCIntf->p_libvlc, "key-action", ACTIONID_RATE_NORMAL );
}

- (void)toggleRecord
{
    intf_thread_t *p_intf = VLCIntf;
    if( !p_intf )
        return;

    input_thread_t * p_input;
    p_input = pl_CurrentInput( p_intf );
    if( p_input )
    {
        var_ToggleBool( p_input, "record" );
        vlc_object_release( p_input );
    }
}

- (void)setPlaybackRate:(int)i_value
{
    playlist_t * p_playlist = pl_Get( VLCIntf );

    double speed = pow( 2, (double)i_value / 17 );
    int rate = INPUT_RATE_DEFAULT / speed;
    if( i_currentPlaybackRate != rate )
        var_SetFloat( p_playlist, "rate", (float)INPUT_RATE_DEFAULT / (float)rate );
    i_currentPlaybackRate = rate;
}

- (int)playbackRate
{
    float f_rate;

    intf_thread_t *p_intf = VLCIntf;
    if( !p_intf )
        return 0;

    input_thread_t * p_input;
    p_input = pl_CurrentInput( p_intf );
    if( p_input )
    {
        f_rate = var_GetFloat( p_input, "rate" );
        vlc_object_release( p_input );
    }
    else
    {
        playlist_t * p_playlist = pl_Get( VLCIntf );
        f_rate = var_GetFloat( p_playlist, "rate" );
    }

    double value = 17 * log( f_rate ) / log( 2. );
    int returnValue = (int) ( ( value > 0 ) ? value + .5 : value - .5 );

    if( returnValue < -34 )
        returnValue = -34;
    else if( returnValue > 34 )
        returnValue = 34;

    i_currentPlaybackRate = returnValue;
    return returnValue;
}

- (void)previous
{
    var_SetInteger( VLCIntf->p_libvlc, "key-action", ACTIONID_PREV );
}

- (void)next
{
    var_SetInteger( VLCIntf->p_libvlc, "key-action", ACTIONID_NEXT );
}

- (BOOL)isPlaying
{
    intf_thread_t *p_intf = VLCIntf;
    if( !p_intf )
        return NO;

    input_thread_t * p_input = pl_CurrentInput( p_intf );
    if( !p_input )
        return NO;

    input_state_e i_state = ERROR_S;
    input_Control( p_input, INPUT_GET_STATE, &i_state );
    vlc_object_release( p_input );

    return ( ( i_state == OPENING_S ) || ( i_state == PLAYING_S ) );
}

- (int)currentTime
{
    input_thread_t * p_input = pl_CurrentInput( VLCIntf );
    int64_t i_currentTime = -1;

    if( !p_input )
        return i_currentTime;

    input_Control( p_input, INPUT_GET_TIME, &i_currentTime );
    vlc_object_release( p_input );

    return (int)( i_currentTime / 1000000 );
}

- (void)setCurrentTime:(int)i_value
{
    int64_t i64_value = (int64_t)i_value;
    input_thread_t * p_input = pl_CurrentInput( VLCIntf );

    if ( !p_input )
        return;

    input_Control( p_input, INPUT_SET_TIME, (int64_t)(i64_value * 1000000) );
    vlc_object_release( p_input );
}

- (int)durationOfCurrentPlaylistItem
{
    intf_thread_t *p_intf = VLCIntf;
    if( !p_intf )
        return 0;

    input_thread_t * p_input = pl_CurrentInput( p_intf );
    int64_t i_duration = -1;
    if( !p_input )
        return i_duration;

    input_Control( p_input, INPUT_GET_LENGTH, &i_duration );
    vlc_object_release( p_input );

    return (int)( i_duration / 1000000 );
}

- (NSURL*)URLOfCurrentPlaylistItem
{
    intf_thread_t *p_intf = VLCIntf;
    if( !p_intf )
        return nil;

    input_thread_t *p_input = pl_CurrentInput( p_intf );
    if( !p_input )
        return nil;

    input_item_t *p_item = input_GetItem( p_input );
    if( !p_item )
    {
        vlc_object_release( p_input );
        return nil;
    }

    char *psz_uri = input_item_GetURI( p_item );
    if( !psz_uri )
    {
        vlc_object_release( p_input );
        return nil;
    }

    NSURL *o_url;
    o_url = [NSURL URLWithString:[NSString stringWithUTF8String:psz_uri]];
    vlc_object_release( p_input );

    return o_url;
}

- (NSString*)nameOfCurrentPlaylistItem
{
    intf_thread_t *p_intf = VLCIntf;
    if( !p_intf )
        return nil;

    input_thread_t *p_input = pl_CurrentInput( p_intf );
    if( !p_input )
        return nil;

    input_item_t *p_item = input_GetItem( p_input );
    if( !p_item )
    {
        vlc_object_release( p_input );
        return nil;
    }

    char *psz_uri = input_item_GetURI( p_item );
    if( !psz_uri )
    {
        vlc_object_release( p_input );
        return nil;
    }

    NSString *o_name;
    char *format = var_InheritString( VLCIntf, "input-title-format" );
    char *formated = str_format_meta( p_input, format );
    free( format );
    o_name = [NSString stringWithUTF8String:formated];
    free( formated );

    NSURL * o_url = [NSURL URLWithString: [NSString stringWithUTF8String: psz_uri]];
    free( psz_uri );

    if( [o_name isEqualToString:@""] )
    {
        if( [o_url isFileURL] ) 
            o_name = [[NSFileManager defaultManager] displayNameAtPath: [o_url path]];
        else
            o_name = [o_url absoluteString];
    }
    vlc_object_release( p_input );
    return o_name;
}

- (void)forward
{
    //LEGACY SUPPORT
    [self forwardShort];
}

- (void)backward
{
    //LEGACY SUPPORT
    [self backwardShort];
}

- (void)forwardExtraShort
{
    var_SetInteger( VLCIntf->p_libvlc, "key-action", ACTIONID_JUMP_FORWARD_EXTRASHORT );
}

- (void)backwardExtraShort
{
    var_SetInteger( VLCIntf->p_libvlc, "key-action", ACTIONID_JUMP_BACKWARD_EXTRASHORT );
}

- (void)forwardShort
{
    var_SetInteger( VLCIntf->p_libvlc, "key-action", ACTIONID_JUMP_FORWARD_SHORT );
}

- (void)backwardShort
{
    var_SetInteger( VLCIntf->p_libvlc, "key-action", ACTIONID_JUMP_BACKWARD_SHORT );
}

- (void)forwardMedium
{
    var_SetInteger( VLCIntf->p_libvlc, "key-action", ACTIONID_JUMP_FORWARD_MEDIUM );
}

- (void)backwardMedium
{
    var_SetInteger( VLCIntf->p_libvlc, "key-action", ACTIONID_JUMP_BACKWARD_MEDIUM );
}

- (void)forwardLong
{
    var_SetInteger( VLCIntf->p_libvlc, "key-action", ACTIONID_JUMP_FORWARD_LONG );
}

- (void)backwardLong
{
    var_SetInteger( VLCIntf->p_libvlc, "key-action", ACTIONID_JUMP_BACKWARD_LONG );
}

- (void)shuffle
{
    intf_thread_t *p_intf = VLCIntf;
    if( !p_intf )
        return;

    vlc_value_t val;
    playlist_t * p_playlist = pl_Get( p_intf );
    vout_thread_t *p_vout = getVout();

    var_Get( p_playlist, "random", &val );
    val.b_bool = !val.b_bool;
    var_Set( p_playlist, "random", val );
    if( val.b_bool )
    {
        if( p_vout )
        {
            vout_OSDMessage( p_vout, SPU_DEFAULT_CHANNEL, "%s", _( "Random On" ) );
            vlc_object_release( p_vout );
        }
        config_PutInt( p_playlist, "random", 1 );
    }
    else
    {
        if( p_vout )
        {
            vout_OSDMessage( p_vout, SPU_DEFAULT_CHANNEL, "%s", _( "Random Off" ) );
            vlc_object_release( p_vout );
        }
        config_PutInt( p_playlist, "random", 0 );
    }
}

- (void)repeatAll
{
    intf_thread_t *p_intf = VLCIntf;
    if( !p_intf )
        return;

    playlist_t * p_playlist = pl_Get( p_intf );

    var_SetBool( p_playlist, "repeat", NO );
    var_SetBool( p_playlist, "loop", YES );
    config_PutInt( p_playlist, "repeat", NO );
    config_PutInt( p_playlist, "loop", YES );

    vout_thread_t *p_vout = getVout();
    if( p_vout )
    {
        vout_OSDMessage( p_vout, SPU_DEFAULT_CHANNEL, "%s", _( "Repeat All" ) );
        vlc_object_release( p_vout );
    }
}

- (void)repeatOne
{
    intf_thread_t *p_intf = VLCIntf;
    if( !p_intf )
        return;

    playlist_t * p_playlist = pl_Get( p_intf );

    var_SetBool( p_playlist, "repeat", YES );
    var_SetBool( p_playlist, "loop", NO );
    config_PutInt( p_playlist, "repeat", YES );
    config_PutInt( p_playlist, "loop", NO );

    vout_thread_t *p_vout = getVout();
    if( p_vout )
    {
        vout_OSDMessage( p_vout, SPU_DEFAULT_CHANNEL, "%s", _( "Repeat One" ) );
        vlc_object_release( p_vout );
    }
}

- (void)repeatOff
{
    intf_thread_t *p_intf = VLCIntf;
    if( !p_intf )
        return;

    playlist_t * p_playlist = pl_Get( p_intf );

    var_SetBool( p_playlist, "repeat", NO );
    var_SetBool( p_playlist, "loop", NO );
    config_PutInt( p_playlist, "repeat", NO );
    config_PutInt( p_playlist, "loop", NO );

    vout_thread_t *p_vout = getVout();
    if( p_vout )
    {
        vout_OSDMessage( p_vout, SPU_DEFAULT_CHANNEL, "%s", _( "Repeat Off" ) );
        vlc_object_release( p_vout );
    }
}

- (void)volumeUp
{
    intf_thread_t *p_intf = VLCIntf;
    if( !p_intf )
        return;

    aout_VolumeUp( pl_Get( p_intf ), 1, NULL );
}

- (void)volumeDown
{
    intf_thread_t *p_intf = VLCIntf;
    if( !p_intf )
        return;

    aout_VolumeDown( pl_Get( p_intf ), 1, NULL );
}

- (void)mute
{
    intf_thread_t *p_intf = VLCIntf;
    if( !p_intf )
        return;

    aout_ToggleMute( pl_Get( p_intf ), NULL );
}

- (BOOL)isMuted
{
    intf_thread_t *p_intf = VLCIntf;
    if( !p_intf )
        return NO;

    BOOL b_is_muted = NO;
    b_is_muted = aout_IsMuted( VLC_OBJECT(pl_Get( p_intf )) );

    return b_is_muted;
}

- (int)volume
{
    intf_thread_t *p_intf = VLCIntf;
    if( !p_intf )
        return 0;

    audio_volume_t i_volume = aout_VolumeGet( pl_Get( p_intf ) );

    return (int)i_volume;
}

- (void)setVolume: (int)i_value
{
    intf_thread_t *p_intf = VLCIntf;
    if( !p_intf )
        return;

    aout_VolumeSet( pl_Get( p_intf ), i_value );
}

#pragma mark -
#pragma mark drag and drop support for VLCVoutView, VLBrushedMetalImageView and VLCThreePartDropView
- (BOOL)performDragOperation:(id <NSDraggingInfo>)sender
{
    NSPasteboard *o_paste = [sender draggingPasteboard];
    NSArray *o_types = [NSArray arrayWithObject: NSFilenamesPboardType];
    NSString *o_desired_type = [o_paste availableTypeFromArray:o_types];
    NSData *o_carried_data = [o_paste dataForType:o_desired_type];
    BOOL b_autoplay = config_GetInt( VLCIntf, "macosx-autoplay" );

    if( o_carried_data )
    {
        if( [o_desired_type isEqualToString:NSFilenamesPboardType] )
        {
            NSArray *o_array = [NSArray array];
            NSArray *o_values = [[o_paste propertyListForType: NSFilenamesPboardType] sortedArrayUsingSelector:@selector(caseInsensitiveCompare:)];
            NSUInteger count = [o_values count];

            input_thread_t * p_input = pl_CurrentInput( VLCIntf );
            BOOL b_returned = NO;

            if( count == 1 && p_input )
            {
                b_returned = input_AddSubtitle( p_input, make_URI([[o_values objectAtIndex:0] UTF8String], NULL), true );
                vlc_object_release( p_input );
                if( !b_returned )
                    return YES;
            }
            else if( p_input )
                vlc_object_release( p_input );

            for( NSUInteger i = 0; i < count; i++)
            {
                NSDictionary *o_dic;
                char *psz_uri = make_URI([[o_values objectAtIndex:i] UTF8String], NULL);
                if( !psz_uri )
                    continue;

                o_dic = [NSDictionary dictionaryWithObject:[NSString stringWithCString:psz_uri encoding:NSUTF8StringEncoding] forKey:@"ITEM_URL"];
                free( psz_uri );

                o_array = [o_array arrayByAddingObject: o_dic];
            }
            if( b_autoplay )
                [[[VLCMain sharedInstance] playlist] appendArray: o_array atPos: -1 enqueue:NO];
            else
                [[[VLCMain sharedInstance] playlist] appendArray: o_array atPos: -1 enqueue:YES];

            return YES;
        }
    }
    return NO;
}

#pragma mark -
#pragma mark video output stuff

- (void)setAspectRatioLocked:(BOOL)b_value
{
    config_PutInt( VLCIntf, "macosx-lock-aspect-ratio", b_value );
}

- (BOOL)aspectRatioIsLocked
{
    return config_GetInt( VLCIntf, "macosx-lock-aspect-ratio" );
}

- (void)toggleFullscreen
{
    intf_thread_t *p_intf = VLCIntf;
    if( !p_intf )
        return;

    BOOL b_fs = var_ToggleBool( pl_Get( p_intf ), "fullscreen" );

    vout_thread_t *p_vout = getVout();
    if( p_vout )
    {
        var_SetBool( p_vout, "fullscreen", b_fs );
        vlc_object_release( p_vout );
    }
}

#pragma mark -
#pragma mark uncommon stuff

- (BOOL)fixPreferences
{
    NSMutableString * o_workString;
    NSRange returnedRange;
    NSRange fullRange;
    BOOL b_needsRestart = NO;

    #define fixpref( pref ) \
    o_workString = [[NSMutableString alloc] initWithFormat:@"%s", config_GetPsz( VLCIntf, pref )]; \
    if ([o_workString length] > 0) \
    { \
        returnedRange = [o_workString rangeOfString:@"macosx" options: NSCaseInsensitiveSearch]; \
        if (returnedRange.location != NSNotFound) \
        { \
            if ([o_workString isEqualToString:@"macosx"]) \
                [o_workString setString:@""]; \
            fullRange = NSMakeRange( 0, [o_workString length] ); \
            [o_workString replaceOccurrencesOfString:@":macosx" withString:@"" options: NSCaseInsensitiveSearch range: fullRange]; \
            fullRange = NSMakeRange( 0, [o_workString length] ); \
            [o_workString replaceOccurrencesOfString:@"macosx:" withString:@"" options: NSCaseInsensitiveSearch range: fullRange]; \
            \
            config_PutPsz( VLCIntf, pref, [o_workString UTF8String] ); \
            b_needsRestart = YES; \
        } \
    } \
    [o_workString release]

    fixpref( "control" );
    fixpref( "extraintf" );
    #undef fixpref

    return b_needsRestart;
}

@end
