/*****************************************************************************
 * auhal.c: AUHAL and Coreaudio output plugin
 *****************************************************************************
 * Copyright (C) 2005, 2011 the VideoLAN team
 * $Id: 4ef7a94be692785dfc663e20372777cd25469ebc $
 *
 * Authors: Derk-Jan Hartman <hartman at videolan dot org>
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
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_dialog.h>                   // dialog_Fatal
#include <vlc_aout.h>                     // aout_*
#include <vlc_aout_intf.h>

#include <AudioUnit/AudioUnit.h>          // AudioUnit
#include <CoreAudio/CoreAudio.h>      // AudioDeviceID
#include <AudioToolbox/AudioFormat.h>     // AudioFormatGetProperty
#include <CoreServices/CoreServices.h>

#ifndef verify_noerr
# define verify_noerr(a) assert((a) == noErr)
#endif

#define STREAM_FORMAT_MSG( pre, sfm ) \
    pre "[%f][%4.4s][%u][%u][%u][%u][%u][%u]", \
    sfm.mSampleRate, (char *)&sfm.mFormatID, \
    (unsigned int)sfm.mFormatFlags, (unsigned int)sfm.mBytesPerPacket, \
    (unsigned int)sfm.mFramesPerPacket, (unsigned int)sfm.mBytesPerFrame, \
    (unsigned int)sfm.mChannelsPerFrame, (unsigned int)sfm.mBitsPerChannel

#define FRAMESIZE 2048
#define BUFSIZE (FRAMESIZE * 8) * 8
#define AOUT_VAR_SPDIF_FLAG 0xf00000

/*
 * TODO:
 * - clean up the debug info
 * - be better at changing stream setup or devices setup changes while playing.
 * - fix 6.1 and 7.1
 */

/*****************************************************************************
 * aout_sys_t: private audio output method descriptor
 *****************************************************************************
 * This structure is part of the audio output thread descriptor.
 * It describes the CoreAudio specific properties of an output thread.
 *****************************************************************************/
struct aout_sys_t
{
    aout_packet_t               packet;
    AudioDeviceID               i_default_dev;       /* DeviceID of defaultOutputDevice */
    AudioDeviceID               i_selected_dev;      /* DeviceID of the selected device */
    AudioDeviceIOProcID         i_procID;            /* DeviceID of current device */
    UInt32                      i_devices;           /* Number of CoreAudio Devices */
    bool                        b_digital;           /* Are we running in digital mode? */
    mtime_t                     clock_diff;          /* Difference between VLC clock and Device clock */

    /* AUHAL specific */
    Component                   au_component;        /* The Audiocomponent we use */
    AudioUnit                   au_unit;             /* The AudioUnit we use */
    uint8_t                     p_remainder_buffer[BUFSIZE];
    uint32_t                    i_read_bytes;
    uint32_t                    i_total_bytes;

    /* CoreAudio SPDIF mode specific */
    pid_t                       i_hog_pid;           /* The keep the pid of our hog status */
    AudioStreamID               i_stream_id;         /* The StreamID that has a cac3 streamformat */
    int                         i_stream_index;      /* The index of i_stream_id in an AudioBufferList */
    AudioStreamBasicDescription stream_format;       /* The format we changed the stream to */
    AudioStreamBasicDescription sfmt_revert;         /* The original format of the stream */
    bool                        b_revert;            /* Wether we need to revert the stream format */
    bool                        b_changed_mixing;    /* Wether we need to set the mixing mode back */
};

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
static int      Open                    ( vlc_object_t * );
static int      OpenAnalog              ( audio_output_t * );
static int      OpenSPDIF               ( audio_output_t * );
static void     Close                   ( vlc_object_t * );

static void     Probe                   ( audio_output_t * );

static int      AudioDeviceHasOutput    ( AudioDeviceID );
static int      AudioDeviceSupportsDigital( audio_output_t *, AudioDeviceID );
static int      AudioStreamSupportsDigital( audio_output_t *, AudioStreamID );
static int      AudioStreamChangeFormat ( audio_output_t *, AudioStreamID, AudioStreamBasicDescription );

static OSStatus RenderCallbackAnalog    ( vlc_object_t *, AudioUnitRenderActionFlags *, const AudioTimeStamp *,
                                          unsigned int, unsigned int, AudioBufferList *);
static OSStatus RenderCallbackSPDIF     ( AudioDeviceID, const AudioTimeStamp *, const void *, const AudioTimeStamp *,
                                          AudioBufferList *, const AudioTimeStamp *, void * );
static OSStatus HardwareListener        ( AudioObjectID, UInt32, const AudioObjectPropertyAddress *, void * );
static OSStatus StreamListener          ( AudioObjectID, UInt32, const AudioObjectPropertyAddress *, void * );
static int      AudioDeviceCallback     ( vlc_object_t *, const char *,
                                          vlc_value_t, vlc_value_t, void * );

static int      VolumeSet               ( audio_output_t *, float, bool );


/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define ADEV_TEXT N_("Audio Device")
#define ADEV_LONGTEXT N_("Choose a number corresponding to the number of an " \
    "audio device, as listed in your 'Audio Device' menu. This device will " \
    "then be used by default for audio playback.")

vlc_module_begin ()
    set_shortname( "auhal" )
    set_description( N_("HAL AudioUnit output") )
    set_capability( "audio output", 101 )
    set_category( CAT_AUDIO )
    set_subcategory( SUBCAT_AUDIO_AOUT )
    set_callbacks( Open, Close )
    add_integer( "macosx-audio-device", 0, ADEV_TEXT, ADEV_LONGTEXT, false )
vlc_module_end ()

/*****************************************************************************
 * Open: open macosx audio output
 *****************************************************************************/
static int Open( vlc_object_t * p_this )
{
    OSStatus                err = noErr;
    UInt32                  i_param_size = 0;
    struct aout_sys_t       *p_sys = NULL;
    vlc_value_t             val;
    audio_output_t          *p_aout = (audio_output_t *)p_this;

    /* Use int here, to match kAudioDevicePropertyDeviceIsAlive
     * property size */
    int                     b_alive = false;

    /* Allocate structure */
    p_aout->sys = malloc( sizeof( aout_sys_t ) );
    if( p_aout->sys == NULL )
        return VLC_ENOMEM;

    p_sys = p_aout->sys;
    p_sys->i_default_dev = 0;
    p_sys->i_selected_dev = 0;
    p_sys->i_devices = 0;
    p_sys->b_digital = false;
    p_sys->au_component = NULL;
    p_sys->au_unit = NULL;
    p_sys->clock_diff = (mtime_t) 0;
    p_sys->i_read_bytes = 0;
    p_sys->i_total_bytes = 0;
    p_sys->i_hog_pid = -1;
    p_sys->i_stream_id = 0;
    p_sys->i_stream_index = -1;
    p_sys->b_revert = false;
    p_sys->b_changed_mixing = false;
    memset( p_sys->p_remainder_buffer, 0, sizeof(uint8_t) * BUFSIZE );

    p_aout->pf_play = aout_PacketPlay;
    p_aout->pf_pause = aout_PacketPause;
    p_aout->pf_flush = aout_PacketFlush;

    aout_FormatPrint( p_aout, "VLC is looking for:", &p_aout->format );

    /* Persistent device variable */
    if( var_Type( p_aout->p_libvlc, "macosx-audio-device" ) == 0 )
    {
        var_Create( p_aout->p_libvlc, "macosx-audio-device", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
    }

    /* Build a list of devices */
    if( var_Type( p_aout, "audio-device" ) == 0 )
    {
        Probe( p_aout );
    }

    /* What device do we want? */
    if( var_Get( p_aout, "audio-device", &val ) < 0 )
    {
        msg_Err( p_aout, "audio-device var does not exist. device probe failed." );
        goto error;
    }

    p_sys->i_selected_dev = val.i_int & ~AOUT_VAR_SPDIF_FLAG; /* remove SPDIF flag to get the true DeviceID */
    bool b_supports_digital = ( val.i_int & AOUT_VAR_SPDIF_FLAG );
    if( b_supports_digital )
        msg_Dbg( p_aout, "audio device supports digital output" );

    /* Check if the desired device is alive and usable */
    i_param_size = sizeof( b_alive );
    AudioObjectPropertyAddress audioDeviceAliveAddress = { kAudioDevicePropertyDeviceIsAlive,
                                              kAudioObjectPropertyScopeGlobal,
                                              kAudioObjectPropertyElementMaster };
    err = AudioObjectGetPropertyData( p_sys->i_selected_dev, &audioDeviceAliveAddress, 0, NULL, &i_param_size, &b_alive );

    if( err != noErr )
    {
        /* Be tolerant, only give a warning here */
        msg_Warn( p_aout, "could not check whether device [0x%x] is alive: %4.4s",
                           (unsigned int)p_sys->i_selected_dev, (char *)&err );
        b_alive = false;
    }

    if( !b_alive )
    {
        msg_Warn( p_aout, "selected audio device is not alive, switching to default device" );
        p_sys->i_selected_dev = p_sys->i_default_dev;
    }

    /* add a callback to see if the device dies later on */
    err = AudioObjectAddPropertyListener( p_sys->i_selected_dev, &audioDeviceAliveAddress, HardwareListener, (void *)p_aout );
    if( err != noErr )
    {
        /* Be tolerant, only give a warning here */
        msg_Warn( p_aout, "could not set alive check callback on device [0x%x]: %4.4s",
                 (unsigned int)p_sys->i_selected_dev, (char *)&err );
    }

    AudioObjectPropertyAddress audioDeviceHogModeAddress = { kAudioDevicePropertyHogMode,
                                  kAudioDevicePropertyScopeOutput,
                                  kAudioObjectPropertyElementMaster };
    i_param_size = sizeof( p_sys->i_hog_pid );
    err = AudioObjectGetPropertyData( p_sys->i_selected_dev, &audioDeviceHogModeAddress, 0, NULL, &i_param_size, &p_sys->i_hog_pid );
    if( err != noErr )
    {
        /* This is not a fatal error. Some drivers simply don't support this property */
        msg_Warn( p_aout, "could not check whether device is hogged: %4.4s",
                 (char *)&err );
        p_sys->i_hog_pid = -1;
    }

    if( p_sys->i_hog_pid != -1 && p_sys->i_hog_pid != getpid() )
    {
        msg_Err( p_aout, "Selected audio device is exclusively in use by another program." );
        dialog_Fatal( p_aout, _("Audio output failed"), "%s",
                        _("The selected audio output device is exclusively in "
                          "use by another program.") );
        goto error;
    }

    /* If we change the device we want to use, we should renegotiate the audio chain */
    var_AddCallback( p_aout, "audio-device", AudioDeviceCallback, NULL );

    /* Check for Digital mode or Analog output mode */
    if( AOUT_FMT_SPDIF( &p_aout->format ) && b_supports_digital )
    {
        if( OpenSPDIF( p_aout ) )
        {
            msg_Dbg( p_aout, "digital output successfully opened" );
            return VLC_SUCCESS;
        }
    }
    else
    {
        if( OpenAnalog( p_aout ) )
        {
            msg_Dbg( p_aout, "analog output successfully opened" );
            return VLC_SUCCESS;
        }
    }

error:
    /* If we reach this, this aout has failed */
    msg_Err( p_aout, "opening the auhal output failed" );
    var_Destroy( p_aout, "audio-device" );
    free( p_sys );
    return VLC_EGENERIC;
}

/*****************************************************************************
 * Open: open and setup a HAL AudioUnit to do analog (multichannel) audio output
 *****************************************************************************/
static int OpenAnalog( audio_output_t *p_aout )
{
    struct aout_sys_t           *p_sys = p_aout->sys;
    OSStatus                    err = noErr;
    UInt32                      i_param_size = 0;
    int                         i_original;
    ComponentDescription        desc;
    AudioStreamBasicDescription DeviceFormat;
    AudioChannelLayout          *layout;
    AudioChannelLayout          new_layout;
    AURenderCallbackStruct      input;

    /* Lets go find our Component */
    desc.componentType = kAudioUnitType_Output;
    desc.componentSubType = kAudioUnitSubType_HALOutput;
    desc.componentManufacturer = kAudioUnitManufacturer_Apple;
    desc.componentFlags = 0;
    desc.componentFlagsMask = 0;

    p_sys->au_component = FindNextComponent( NULL, &desc );
    if( p_sys->au_component == NULL )
    {
        msg_Warn( p_aout, "we cannot find our HAL component" );
        return false;
    }

    err = OpenAComponent( p_sys->au_component, &p_sys->au_unit );
    if( err != noErr )
    {
        msg_Warn( p_aout, "we cannot open our HAL component" );
        return false;
    }

    /* Set the device we will use for this output unit */
    err = AudioUnitSetProperty( p_sys->au_unit,
                         kAudioOutputUnitProperty_CurrentDevice,
                         kAudioUnitScope_Global,
                         0,
                         &p_sys->i_selected_dev,
                         sizeof( AudioDeviceID ));

    if( err != noErr )
    {
        msg_Warn( p_aout, "we cannot select the audio device" );
        return false;
    }

    /* Get the current format */
    i_param_size = sizeof(AudioStreamBasicDescription);

    err = AudioUnitGetProperty( p_sys->au_unit,
                                   kAudioUnitProperty_StreamFormat,
                                   kAudioUnitScope_Input,
                                   0,
                                   &DeviceFormat,
                                   &i_param_size );

    if( err != noErr ) return false;
    else msg_Dbg( p_aout, STREAM_FORMAT_MSG( "current format is: ", DeviceFormat ) );

    /* Get the channel layout of the device side of the unit (vlc -> unit -> device) */
    err = AudioUnitGetPropertyInfo( p_sys->au_unit,
                                   kAudioDevicePropertyPreferredChannelLayout,
                                   kAudioUnitScope_Output,
                                   0,
                                   &i_param_size,
                                   NULL );

    if( err == noErr )
    {
        layout = (AudioChannelLayout *)malloc( i_param_size);

        verify_noerr( AudioUnitGetProperty( p_sys->au_unit,
                                       kAudioDevicePropertyPreferredChannelLayout,
                                       kAudioUnitScope_Output,
                                       0,
                                       layout,
                                       &i_param_size ));

        /* We need to "fill out" the ChannelLayout, because there are multiple ways that it can be set */
        if( layout->mChannelLayoutTag == kAudioChannelLayoutTag_UseChannelBitmap)
        {
            /* bitmap defined channellayout */
            verify_noerr( AudioFormatGetProperty( kAudioFormatProperty_ChannelLayoutForBitmap,
                                    sizeof( UInt32), &layout->mChannelBitmap,
                                    &i_param_size,
                                    layout ));
        }
        else if( layout->mChannelLayoutTag != kAudioChannelLayoutTag_UseChannelDescriptions )
        {
            /* layouttags defined channellayout */
            verify_noerr( AudioFormatGetProperty( kAudioFormatProperty_ChannelLayoutForTag,
                                    sizeof( AudioChannelLayoutTag ), &layout->mChannelLayoutTag,
                                    &i_param_size,
                                    layout ));
        }

        msg_Dbg( p_aout, "layout of AUHAL has %d channels" , (int)layout->mNumberChannelDescriptions );

        /* Initialize the VLC core channel count */
        p_aout->format.i_physical_channels = 0;
        i_original = p_aout->format.i_original_channels & AOUT_CHAN_PHYSMASK;

        if( i_original == AOUT_CHAN_CENTER || layout->mNumberChannelDescriptions < 2 )
        {
            /* We only need Mono or cannot output more than 1 channel */
            p_aout->format.i_physical_channels = AOUT_CHAN_CENTER;
        }
        else if( i_original == (AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT) || layout->mNumberChannelDescriptions < 3 )
        {
            /* We only need Stereo or cannot output more than 2 channels */
            p_aout->format.i_physical_channels = AOUT_CHAN_RIGHT | AOUT_CHAN_LEFT;
        }
        else
        {
            /* We want more than stereo and we can do that */
            for( unsigned int i = 0; i < layout->mNumberChannelDescriptions; i++ )
            {
                msg_Dbg( p_aout, "this is channel: %d", (int)layout->mChannelDescriptions[i].mChannelLabel );

                switch( layout->mChannelDescriptions[i].mChannelLabel )
                {
                    case kAudioChannelLabel_Left:
                        p_aout->format.i_physical_channels |= AOUT_CHAN_LEFT;
                        continue;
                    case kAudioChannelLabel_Right:
                        p_aout->format.i_physical_channels |= AOUT_CHAN_RIGHT;
                        continue;
                    case kAudioChannelLabel_Center:
                        p_aout->format.i_physical_channels |= AOUT_CHAN_CENTER;
                        continue;
                    case kAudioChannelLabel_LFEScreen:
                        p_aout->format.i_physical_channels |= AOUT_CHAN_LFE;
                        continue;
                    case kAudioChannelLabel_LeftSurround:
                        p_aout->format.i_physical_channels |= AOUT_CHAN_REARLEFT;
                        continue;
                    case kAudioChannelLabel_RightSurround:
                        p_aout->format.i_physical_channels |= AOUT_CHAN_REARRIGHT;
                        continue;
                    case kAudioChannelLabel_RearSurroundLeft:
                        p_aout->format.i_physical_channels |= AOUT_CHAN_MIDDLELEFT;
                        continue;
                    case kAudioChannelLabel_RearSurroundRight:
                        p_aout->format.i_physical_channels |= AOUT_CHAN_MIDDLERIGHT;
                        continue;
                    case kAudioChannelLabel_CenterSurround:
                        p_aout->format.i_physical_channels |= AOUT_CHAN_REARCENTER;
                        continue;
                    default:
                        msg_Warn( p_aout, "unrecognized channel form provided by driver: %d", (int)layout->mChannelDescriptions[i].mChannelLabel );
                }
            }
            if( p_aout->format.i_physical_channels == 0 )
            {
                p_aout->format.i_physical_channels = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT;
                msg_Err( p_aout, "You should configure your speaker layout with Audio Midi Setup Utility in /Applications/Utilities. Now using Stereo mode." );
                dialog_Fatal( p_aout, _("Audio device is not configured"), "%s",
                                _("You should configure your speaker layout with "
                                  "the \"Audio Midi Setup\" utility in /Applications/"
                                  "Utilities. Stereo mode is being used now.") );
            }
        }
        free( layout );
    }
    else
    {
        msg_Warn( p_aout, "this driver does not support kAudioDevicePropertyPreferredChannelLayout. BAD DRIVER AUTHOR !!!" );
        p_aout->format.i_physical_channels = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT;
    }

    msg_Dbg( p_aout, "selected %d physical channels for device output", aout_FormatNbChannels( &p_aout->format ) );
    msg_Dbg( p_aout, "VLC will output: %s", aout_FormatPrintChannels( &p_aout->format ));

    memset (&new_layout, 0, sizeof(new_layout));
    switch( aout_FormatNbChannels( &p_aout->format ) )
    {
        case 1:
            new_layout.mChannelLayoutTag = kAudioChannelLayoutTag_Mono;
            break;
        case 2:
            new_layout.mChannelLayoutTag = kAudioChannelLayoutTag_Stereo;
            break;
        case 3:
            if( p_aout->format.i_physical_channels & AOUT_CHAN_CENTER )
            {
                new_layout.mChannelLayoutTag = kAudioChannelLayoutTag_DVD_7; // L R C
            }
            else if( p_aout->format.i_physical_channels & AOUT_CHAN_LFE )
            {
                new_layout.mChannelLayoutTag = kAudioChannelLayoutTag_DVD_4; // L R LFE
            }
            break;
        case 4:
            if( p_aout->format.i_physical_channels & ( AOUT_CHAN_CENTER | AOUT_CHAN_LFE ) )
            {
                new_layout.mChannelLayoutTag = kAudioChannelLayoutTag_DVD_10; // L R C LFE
            }
            else if( p_aout->format.i_physical_channels & ( AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT ) )
            {
                new_layout.mChannelLayoutTag = kAudioChannelLayoutTag_DVD_3; // L R Ls Rs
            }
            else if( p_aout->format.i_physical_channels & ( AOUT_CHAN_CENTER | AOUT_CHAN_REARCENTER ) )
            {
                new_layout.mChannelLayoutTag = kAudioChannelLayoutTag_DVD_3; // L R C Cs
            }
            break;
        case 5:
            if( p_aout->format.i_physical_channels & ( AOUT_CHAN_CENTER ) )
            {
                new_layout.mChannelLayoutTag = kAudioChannelLayoutTag_DVD_19; // L R Ls Rs C
            }
            else if( p_aout->format.i_physical_channels & ( AOUT_CHAN_LFE ) )
            {
                new_layout.mChannelLayoutTag = kAudioChannelLayoutTag_DVD_18; // L R Ls Rs LFE
            }
            break;
        case 6:
            if( p_aout->format.i_physical_channels & ( AOUT_CHAN_LFE ) )
            {
                new_layout.mChannelLayoutTag = kAudioChannelLayoutTag_DVD_20; // L R Ls Rs C LFE
            }
            else
            {
                new_layout.mChannelLayoutTag = kAudioChannelLayoutTag_AudioUnit_6_0; // L R Ls Rs C Cs
            }
            break;
        case 7:
            /* FIXME: This is incorrect. VLC uses the internal ordering: L R Lm Rm Lr Rr C LFE but this is wrong */
            new_layout.mChannelLayoutTag = kAudioChannelLayoutTag_MPEG_6_1_A; // L R C LFE Ls Rs Cs
            break;
        case 8:
            /* FIXME: This is incorrect. VLC uses the internal ordering: L R Lm Rm Lr Rr C LFE but this is wrong */
            new_layout.mChannelLayoutTag = kAudioChannelLayoutTag_MPEG_7_1_A; // L R C LFE Ls Rs Lc Rc
            break;
    }

    /* Set up the format to be used */
    DeviceFormat.mSampleRate = p_aout->format.i_rate;
    DeviceFormat.mFormatID = kAudioFormatLinearPCM;

    /* We use float 32. It's the best supported format by both VLC and Coreaudio */
    p_aout->format.i_format = VLC_CODEC_FL32;
    DeviceFormat.mFormatFlags = kAudioFormatFlagsNativeFloatPacked;
    DeviceFormat.mBitsPerChannel = 32;
    DeviceFormat.mChannelsPerFrame = aout_FormatNbChannels( &p_aout->format );

    /* Calculate framesizes and stuff */
    DeviceFormat.mFramesPerPacket = 1;
    DeviceFormat.mBytesPerFrame = DeviceFormat.mBitsPerChannel * DeviceFormat.mChannelsPerFrame / 8;
    DeviceFormat.mBytesPerPacket = DeviceFormat.mBytesPerFrame * DeviceFormat.mFramesPerPacket;

    /* Set the desired format */
    i_param_size = sizeof(AudioStreamBasicDescription);
    verify_noerr( AudioUnitSetProperty( p_sys->au_unit,
                                   kAudioUnitProperty_StreamFormat,
                                   kAudioUnitScope_Input,
                                   0,
                                   &DeviceFormat,
                                   i_param_size ));

    msg_Dbg( p_aout, STREAM_FORMAT_MSG( "we set the AU format: " , DeviceFormat ) );

    /* Retrieve actual format */
    verify_noerr( AudioUnitGetProperty( p_sys->au_unit,
                                   kAudioUnitProperty_StreamFormat,
                                   kAudioUnitScope_Input,
                                   0,
                                   &DeviceFormat,
                                   &i_param_size ));

    msg_Dbg( p_aout, STREAM_FORMAT_MSG( "the actual set AU format is " , DeviceFormat ) );

    /* Do the last VLC aout setups */
    aout_FormatPrepare( &p_aout->format );
    aout_PacketInit( p_aout, &p_sys->packet, FRAMESIZE );
    aout_VolumeHardInit( p_aout, VolumeSet );

    /* Initialize starting volume */
    audio_volume_t volume = var_InheritInteger (p_aout, "volume");
    bool mute = var_InheritBool (p_aout, "mute");
    VolumeSet(p_aout, volume / (float)AOUT_VOLUME_DEFAULT, mute);

    /* set the IOproc callback */
    input.inputProc = (AURenderCallback) RenderCallbackAnalog;
    input.inputProcRefCon = p_aout;

    verify_noerr( AudioUnitSetProperty( p_sys->au_unit,
                            kAudioUnitProperty_SetRenderCallback,
                            kAudioUnitScope_Input,
                            0, &input, sizeof( input ) ) );

    input.inputProc = (AURenderCallback) RenderCallbackAnalog;
    input.inputProcRefCon = p_aout;

    /* Set the new_layout as the layout VLC will use to feed the AU unit */
    verify_noerr( AudioUnitSetProperty( p_sys->au_unit,
                            kAudioUnitProperty_AudioChannelLayout,
                            kAudioUnitScope_Input,
                            0, &new_layout, sizeof(new_layout) ) );

    if( new_layout.mNumberChannelDescriptions > 0 )
        free( new_layout.mChannelDescriptions );

    /* AU initiliaze */
    verify_noerr( AudioUnitInitialize(p_sys->au_unit) );

    /* Find the difference between device clock and mdate clock */
    p_sys->clock_diff = - (mtime_t)
        AudioConvertHostTimeToNanos( AudioGetCurrentHostTime() ) / 1000;
    p_sys->clock_diff += mdate();

    /* Start the AU */
    verify_noerr( AudioOutputUnitStart(p_sys->au_unit) );

    return true;
}

/*****************************************************************************
 * Setup a encoded digital stream (SPDIF)
 *****************************************************************************/
static int OpenSPDIF( audio_output_t * p_aout )
{
    struct aout_sys_t       *p_sys = p_aout->sys;
    OSStatus                err = noErr;
    UInt32                  i_param_size = 0, b_mix = 0;
    Boolean                 b_writeable = false;
    AudioStreamID           *p_streams = NULL;
    unsigned                i_streams = 0;

    /* Start doing the SPDIF setup proces */
    p_sys->b_digital = true;

    /* Hog the device */
    AudioObjectPropertyAddress audioDeviceHogModeAddress = { kAudioDevicePropertyHogMode, kAudioDevicePropertyScopeOutput, kAudioObjectPropertyElementMaster };
    i_param_size = sizeof( p_sys->i_hog_pid );
    p_sys->i_hog_pid = getpid() ;

    err = AudioObjectSetPropertyData( p_sys->i_selected_dev, &audioDeviceHogModeAddress, 0, NULL, i_param_size, &p_sys->i_hog_pid );

    if( err != noErr )
    {
        msg_Err( p_aout, "failed to set hogmode: [%4.4s]", (char *)&err );
        return false;
    }

    AudioObjectPropertyAddress audioDeviceSupportsMixingAddress = { kAudioDevicePropertySupportsMixing , kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMaster };

    if (AudioObjectHasProperty(p_sys->i_selected_dev, &audioDeviceSupportsMixingAddress))
    {
        /* Set mixable to false if we are allowed to */
        err = AudioObjectIsPropertySettable( p_sys->i_selected_dev, &audioDeviceSupportsMixingAddress, &b_writeable );
        err = AudioObjectGetPropertyDataSize( p_sys->i_selected_dev, &audioDeviceSupportsMixingAddress, 0, NULL, &i_param_size );
        err = AudioObjectGetPropertyData( p_sys->i_selected_dev, &audioDeviceSupportsMixingAddress, 0, NULL, &i_param_size, &b_mix );

        if( err == noErr && b_writeable )
        {
            b_mix = 0;
            err = AudioObjectSetPropertyData( p_sys->i_selected_dev, &audioDeviceSupportsMixingAddress, 0, NULL, i_param_size, &b_mix );
            p_sys->b_changed_mixing = true;
        }

        if( err != noErr )
        {
            msg_Err( p_aout, "failed to set mixmode: [%4.4s]", (char *)&err );
            return false;
        }
    }

    /* Get a list of all the streams on this device */
    AudioObjectPropertyAddress streamsAddress = { kAudioDevicePropertyStreams, kAudioDevicePropertyScopeOutput, kAudioObjectPropertyElementMaster };
    err = AudioObjectGetPropertyDataSize( p_sys->i_selected_dev, &streamsAddress, 0, NULL, &i_param_size );
    if( err != noErr )
    {
        msg_Err( p_aout, "could not get number of streams: [%4.4s]", (char *)&err );
        return false;
    }

    i_streams = i_param_size / sizeof( AudioStreamID );
    p_streams = (AudioStreamID *)malloc( i_param_size );
    if( p_streams == NULL )
        return false;

    err = AudioObjectGetPropertyData( p_sys->i_selected_dev, &streamsAddress, 0, NULL, &i_param_size, p_streams );

    if( err != noErr )
    {
        msg_Err( p_aout, "could not get number of streams: [%4.4s]", (char *)&err );
        free( p_streams );
        return false;
    }

    AudioObjectPropertyAddress physicalFormatsAddress = { kAudioStreamPropertyAvailablePhysicalFormats, kAudioObjectPropertyScopeGlobal, 0 };
    for( unsigned i = 0; i < i_streams && p_sys->i_stream_index < 0 ; i++ )
    {
        /* Find a stream with a cac3 stream */
        AudioStreamRangedDescription *p_format_list = NULL;
        int                          i_formats = 0;
        bool                         b_digital = false;

        /* Retrieve all the stream formats supported by each output stream */
        err = AudioObjectGetPropertyDataSize( p_streams[i], &physicalFormatsAddress, 0, NULL, &i_param_size );
        if( err != noErr )
        {
            msg_Err( p_aout, "OpenSPDIF: could not get number of streamformats: [%s] (%i)", (char *)&err, (int32_t)err );
            continue;
        }

        i_formats = i_param_size / sizeof( AudioStreamRangedDescription );
        p_format_list = (AudioStreamRangedDescription *)malloc( i_param_size );
        if( p_format_list == NULL )
            continue;

        err = AudioObjectGetPropertyData( p_streams[i], &physicalFormatsAddress, 0, NULL, &i_param_size, p_format_list );
        if( err != noErr )
        {
            msg_Err( p_aout, "could not get the list of streamformats: [%4.4s]", (char *)&err );
            free( p_format_list );
            continue;
        }

        /* Check if one of the supported formats is a digital format */
        for( int j = 0; j < i_formats; j++ )
        {
            if( p_format_list[j].mFormat.mFormatID == 'IAC3' ||
               p_format_list[j].mFormat.mFormatID == 'iac3' ||
               p_format_list[j].mFormat.mFormatID == kAudioFormat60958AC3 ||
               p_format_list[j].mFormat.mFormatID == kAudioFormatAC3 )
            {
                b_digital = true;
                break;
            }
        }

        if( b_digital )
        {
            /* if this stream supports a digital (cac3) format, then go set it. */
            int i_requested_rate_format = -1;
            int i_current_rate_format = -1;
            int i_backup_rate_format = -1;

            p_sys->i_stream_id = p_streams[i];
            p_sys->i_stream_index = i;

            if( !p_sys->b_revert )
            {
                AudioObjectPropertyAddress currentPhysicalFormatAddress = { kAudioStreamPropertyPhysicalFormat, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMaster };
                /* Retrieve the original format of this stream first if not done so already */
                i_param_size = sizeof( p_sys->sfmt_revert );
                err = AudioObjectGetPropertyData( p_sys->i_stream_id, &currentPhysicalFormatAddress, 0, NULL, &i_param_size, &p_sys->sfmt_revert );
                if( err != noErr )
                {
                    msg_Err( p_aout, "could not retrieve the original streamformat: [%4.4s]", (char *)&err );
                    continue;
                }
                p_sys->b_revert = true;
            }

            for( int j = 0; j < i_formats; j++ )
            {
                if( p_format_list[j].mFormat.mFormatID == 'IAC3' ||
                   p_format_list[j].mFormat.mFormatID == 'iac3' ||
                   p_format_list[j].mFormat.mFormatID == kAudioFormat60958AC3 ||
                   p_format_list[j].mFormat.mFormatID == kAudioFormatAC3 )
                {
                    if( p_format_list[j].mFormat.mSampleRate == p_aout->format.i_rate )
                    {
                        i_requested_rate_format = j;
                        break;
                    }
                    else if( p_format_list[j].mFormat.mSampleRate == p_sys->sfmt_revert.mSampleRate )
                    {
                        i_current_rate_format = j;
                    }
                    else
                    {
                        if( i_backup_rate_format < 0 || p_format_list[j].mFormat.mSampleRate > p_format_list[i_backup_rate_format].mFormat.mSampleRate )
                            i_backup_rate_format = j;
                    }
                }

            }

            if( i_requested_rate_format >= 0 ) /* We prefer to output at the samplerate of the original audio */
                p_sys->stream_format = p_format_list[i_requested_rate_format].mFormat;
            else if( i_current_rate_format >= 0 ) /* If not possible, we will try to use the current samplerate of the device */
                p_sys->stream_format = p_format_list[i_current_rate_format].mFormat;
            else p_sys->stream_format = p_format_list[i_backup_rate_format].mFormat; /* And if we have to, any digital format will be just fine (highest rate possible) */
        }
        free( p_format_list );
    }
    free( p_streams );

    /* get notified when we don't have spdif-output anymore */
    err = AudioObjectAddPropertyListener(p_sys->i_stream_id, &physicalFormatsAddress, HardwareListener, (void *)p_aout);
    if (err != noErr)
        msg_Warn(p_aout, "could not set audio device property streams callback on device: %4.4s", (char *)&err);

    msg_Dbg( p_aout, STREAM_FORMAT_MSG( "original stream format: ", p_sys->sfmt_revert ) );

    if( !AudioStreamChangeFormat( p_aout, p_sys->i_stream_id, p_sys->stream_format ) )
        return false;

    /* Set the format flags */
    if( p_sys->stream_format.mFormatFlags & kAudioFormatFlagIsBigEndian )
        p_aout->format.i_format = VLC_CODEC_SPDIFB;
    else
        p_aout->format.i_format = VLC_CODEC_SPDIFL;
    p_aout->format.i_bytes_per_frame = AOUT_SPDIF_SIZE;
    p_aout->format.i_frame_length = A52_FRAME_NB;
    p_aout->format.i_rate = (unsigned int)p_sys->stream_format.mSampleRate;
    aout_FormatPrepare( &p_aout->format );
    aout_PacketInit( p_aout, &p_sys->packet, A52_FRAME_NB );
    aout_VolumeNoneInit( p_aout );

    /* Add IOProc callback */
    err = AudioDeviceCreateIOProcID( p_sys->i_selected_dev,
                                   (AudioDeviceIOProc)RenderCallbackSPDIF,
                                   (void *)p_aout,
                                   &p_sys->i_procID );
    if( err != noErr )
    {
        msg_Err( p_aout, "AudioDeviceCreateIOProcID failed: [%4.4s]", (char *)&err );
        aout_PacketDestroy (p_aout);
        return false;
    }

    /* Check for the difference between the Device clock and mdate */
    p_sys->clock_diff = - (mtime_t)
        AudioConvertHostTimeToNanos( AudioGetCurrentHostTime() ) / 1000;
    p_sys->clock_diff += mdate();

    /* Start device */
    err = AudioDeviceStart( p_sys->i_selected_dev, p_sys->i_procID );
    if( err != noErr )
    {
        msg_Err( p_aout, "AudioDeviceStart failed: [%4.4s]", (char *)&err );

        err = AudioDeviceDestroyIOProcID( p_sys->i_selected_dev,
                                          p_sys->i_procID );
        if( err != noErr )
        {
            msg_Err( p_aout, "AudioDeviceDestroyIOProcID failed: [%4.4s]", (char *)&err );
        }
        aout_PacketDestroy (p_aout);
        return false;
    }

    return true;
}


/*****************************************************************************
 * Close: Close HAL AudioUnit
 *****************************************************************************/
static void Close( vlc_object_t * p_this )
{
    audio_output_t     *p_aout = (audio_output_t *)p_this;
    struct aout_sys_t   *p_sys = p_aout->sys;
    OSStatus            err = noErr;
    UInt32              i_param_size = 0;

    AudioObjectPropertyAddress deviceAliveAddress = { kAudioDevicePropertyDeviceIsAlive, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMaster };
    err = AudioObjectRemovePropertyListener( p_sys->i_selected_dev, &deviceAliveAddress, HardwareListener, NULL );
    if( err != noErr )
    {
        msg_Err( p_aout, "failed to remove audio device life checker: [%4.4s]", (char *)&err );
    }

    if (p_sys->b_digital) {
        AudioObjectPropertyAddress physicalFormatsAddress = { kAudioStreamPropertyAvailablePhysicalFormats, kAudioObjectPropertyScopeGlobal, 0 };
        err = AudioObjectRemovePropertyListener(p_sys->i_stream_id, &physicalFormatsAddress, HardwareListener, NULL);
        if (err != noErr)
            msg_Err(p_aout, "failed to remove audio device property streams callback: [%4.4s]", (char *)&err);
    }

    if( p_sys->au_unit )
    {
        verify_noerr( AudioOutputUnitStop( p_sys->au_unit ) );
        verify_noerr( AudioUnitUninitialize( p_sys->au_unit ) );
        verify_noerr( CloseComponent( p_sys->au_unit ) );
    }

    if( p_sys->b_digital )
    {
        /* Stop device */
        err = AudioDeviceStop( p_sys->i_selected_dev,
                               p_sys->i_procID );
        if( err != noErr )
        {
            msg_Err( p_aout, "AudioDeviceStop failed: [%4.4s]", (char *)&err );
        }

        /* Remove IOProc callback */
        err = AudioDeviceDestroyIOProcID( p_sys->i_selected_dev,
                                          p_sys->i_procID );
        if( err != noErr )
        {
            msg_Err( p_aout, "AudioDeviceDestroyIOProcID failed: [%4.4s]", (char *)&err );
        }

        if( p_sys->b_revert )
        {
            AudioStreamChangeFormat( p_aout, p_sys->i_stream_id, p_sys->sfmt_revert );
        }

        if( p_sys->b_changed_mixing && p_sys->sfmt_revert.mFormatID != kAudioFormat60958AC3 )
        {
            int b_mix;
            Boolean b_writeable = false;
            /* Revert mixable to true if we are allowed to */
            AudioObjectPropertyAddress audioDeviceSupportsMixingAddress = { kAudioDevicePropertySupportsMixing , kAudioDevicePropertyScopeOutput, kAudioObjectPropertyElementMaster };
            err = AudioObjectIsPropertySettable( p_sys->i_selected_dev, &audioDeviceSupportsMixingAddress, &b_writeable );
            err = AudioObjectGetPropertyData( p_sys->i_selected_dev, &audioDeviceSupportsMixingAddress, 0, NULL, &i_param_size, &b_mix );

            if( err == noErr && b_writeable )
            {
                msg_Dbg( p_aout, "mixable is: %d", b_mix );
                b_mix = 1;
                err = AudioObjectSetPropertyData( p_sys->i_selected_dev, &audioDeviceSupportsMixingAddress, 0, NULL, i_param_size, &b_mix );
            }

            if( err != noErr )
            {
                msg_Err( p_aout, "failed to set mixmode: [%4.4s]", (char *)&err );
            }
        }
    }

    AudioObjectPropertyAddress audioDevicesAddress = { kAudioHardwarePropertyDevices, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMaster };
    err = AudioObjectRemovePropertyListener( kAudioObjectSystemObject, &audioDevicesAddress, HardwareListener, NULL );

    if( err != noErr )
    {
        msg_Err( p_aout, "AudioHardwareRemovePropertyListener failed: [%4.4s]", (char *)&err );
    }

    if( p_sys->i_hog_pid == getpid() )
    {
        p_sys->i_hog_pid = -1;
        i_param_size = sizeof( p_sys->i_hog_pid );
        AudioObjectPropertyAddress audioDeviceHogModeAddress = { kAudioDevicePropertyHogMode,
            kAudioDevicePropertyScopeOutput,
            kAudioObjectPropertyElementMaster };
        err = AudioObjectSetPropertyData( p_sys->i_selected_dev, &audioDeviceHogModeAddress, 0, NULL, i_param_size, &p_sys->i_hog_pid );
        if( err != noErr ) msg_Err( p_aout, "Could not release hogmode: [%4.4s]", (char *)&err );
    }

    var_DelCallback( p_aout, "audio-device", AudioDeviceCallback, NULL );

    aout_PacketDestroy( p_aout );
    free( p_sys );
}

/*****************************************************************************
 * Probe: Check which devices the OS has, and add them to our audio-device menu
 *****************************************************************************/
static void Probe( audio_output_t * p_aout )
{
    OSStatus            err = noErr;
    UInt32              i_param_size = 0;
    AudioDeviceID       devid_def = 0;
    AudioDeviceID       *p_devices = NULL;
    vlc_value_t         val, text;

    struct aout_sys_t   *p_sys = p_aout->sys;

    /* Get number of devices */
    AudioObjectPropertyAddress audioDevicesAddress = { kAudioHardwarePropertyDevices, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMaster };
    err = AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &audioDevicesAddress, 0, NULL, &i_param_size);
    if( err != noErr )
    {
        msg_Err( p_aout, "Could not get number of devices: [%s]", (char *)&err );
        goto error;
    }

    p_sys->i_devices = i_param_size / sizeof( AudioDeviceID );

    if( p_sys->i_devices < 1 )
    {
        msg_Err( p_aout, "No audio output devices were found." );
        goto error;
    }
    msg_Dbg( p_aout, "found %u audio device(s)", (unsigned)p_sys->i_devices );

    /* Allocate DeviceID array */
    p_devices = (AudioDeviceID*)malloc( sizeof(AudioDeviceID) * p_sys->i_devices );
    if( p_devices == NULL )
        goto error;

    /* Populate DeviceID array */
    err = AudioObjectGetPropertyData( kAudioObjectSystemObject, &audioDevicesAddress, 0, NULL, &i_param_size, p_devices );
    if( err != noErr )
    {
        msg_Err( p_aout, "could not get the device IDs: [%s]", (char *)&err );
        goto error;
    }

    /* Find the ID of the default Device */
    AudioObjectPropertyAddress defaultDeviceAddress = { kAudioHardwarePropertyDefaultOutputDevice, kAudioDevicePropertyScopeOutput, kAudioObjectPropertyElementMaster };
    i_param_size = sizeof( AudioDeviceID );
    err= AudioObjectGetPropertyData( kAudioObjectSystemObject, &defaultDeviceAddress, 0, NULL, &i_param_size, &devid_def );
    if( err != noErr )
    {
        msg_Err( p_aout, "could not get default audio device: [%s]", (char *)&err );
        goto error;
    }
    p_sys->i_default_dev = devid_def;

    var_Create( p_aout, "audio-device", VLC_VAR_INTEGER|VLC_VAR_HASCHOICE );
    text.psz_string = (char*)_("Audio Device");
    var_Change( p_aout, "audio-device", VLC_VAR_SETTEXT, &text, NULL );

    AudioObjectPropertyAddress deviceNameAddress = { kAudioObjectPropertyName, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMaster };

    for( unsigned int i = 0; i < p_sys->i_devices; i++ )
    {
        CFStringRef device_name_ref;
        char *psz_name;
        CFIndex length;


        /* Retrieve the length of the device name */
        err = AudioObjectGetPropertyDataSize( p_devices[i], &deviceNameAddress, 0, NULL, &i_param_size );
        if (err != noErr) {
            goto error;
        }

        /* Retrieve the name of the device */
        err = AudioObjectGetPropertyData( p_devices[i], &deviceNameAddress, 0, NULL, &i_param_size, &device_name_ref );
        if (err != noErr) {
            goto error;
        }
        length = CFStringGetLength(device_name_ref);
        length++;
        psz_name = (char *)malloc(length);
        CFStringGetCString(device_name_ref, psz_name, length, kCFStringEncodingUTF8);

        msg_Dbg( p_aout, "DevID: %u DevName: %s", (unsigned)p_devices[i], psz_name );

        if( !AudioDeviceHasOutput( p_devices[i]) )
        {
            msg_Dbg( p_aout, "this device is INPUT only. skipping..." );
            free( psz_name );
            continue;
        }

        /* Add the menu entries */
        val.i_int = (int)p_devices[i];
        text.psz_string = psz_name;
        var_Change( p_aout, "audio-device", VLC_VAR_ADDCHOICE, &val, &text );
        text.psz_string = NULL;
        if( p_sys->i_default_dev == p_devices[i] )
        {
            /* The default device is the selected device normally */
            var_Change( p_aout, "audio-device", VLC_VAR_SETDEFAULT, &val, NULL );
            var_Set( p_aout, "audio-device", val );
        }

        if( AudioDeviceSupportsDigital( p_aout, p_devices[i] ) )
        {
            val.i_int = (int)p_devices[i] | AOUT_VAR_SPDIF_FLAG;
            if( asprintf( &text.psz_string, _("%s (Encoded Output)"), psz_name ) != -1 )
            {
                var_Change( p_aout, "audio-device", VLC_VAR_ADDCHOICE, &val, &text );
                free( text.psz_string );
                if( p_sys->i_default_dev == p_devices[i]
                 && var_InheritBool( p_aout, "spdif" ) )
                {
                    /* We selected to prefer SPDIF output if available
                     * then this "dummy" entry should be selected */
                    var_Change( p_aout, "audio-device", VLC_VAR_SETDEFAULT, &val, NULL );
                    var_Set( p_aout, "audio-device", val );
                }
            }
        }

        CFRelease(device_name_ref);
        free(psz_name);
    }

    /* If a device is already "preselected", then use this device */
    var_Get( p_aout->p_libvlc, "macosx-audio-device", &val );
    if( val.i_int > 0 )
    {
        msg_Dbg( p_aout, "using preselected output device %#"PRIx64, val.i_int );
        var_Change( p_aout, "audio-device", VLC_VAR_SETDEFAULT, &val, NULL );
        var_Set( p_aout, "audio-device", val );
    }

    /* Attach a Listener so that we are notified of a change in the Device setup */
    err = AudioObjectAddPropertyListener( kAudioObjectSystemObject, &audioDevicesAddress, HardwareListener, (void *)p_aout );
    if( err != noErr ) {
        msg_Warn( p_aout, "failed to add listener for audio device configuration (%i)", err );
        goto error;
    }

    free( p_devices );
    return;

error:
    msg_Warn( p_aout, "audio device already in use" );
    free( p_devices );
    return;
}

/*****************************************************************************
 * AudioDeviceHasOutput: Checks if the Device actually provides any outputs at all
 *****************************************************************************/
static int AudioDeviceHasOutput( AudioDeviceID i_dev_id )
{
    UInt32            dataSize;

    AudioObjectPropertyAddress streamsAddress = { kAudioDevicePropertyStreams, kAudioDevicePropertyScopeOutput, kAudioObjectPropertyElementMaster };
    verify_noerr( AudioObjectGetPropertyDataSize( i_dev_id, &streamsAddress, 0, NULL, &dataSize ) );
    if (dataSize == 0) return FALSE;

    return TRUE;
}

/*****************************************************************************
 * AudioDeviceSupportsDigital: Check i_dev_id for digital stream support.
 *****************************************************************************/
static int AudioDeviceSupportsDigital( audio_output_t *p_aout, AudioDeviceID i_dev_id )
{
    OSStatus                    err = noErr;
    UInt32                      i_param_size = 0;
    AudioStreamID               *p_streams = NULL;
    int                         i_streams = 0;
    bool                  b_return = false;

    /* Retrieve all the output streams */
    AudioObjectPropertyAddress streamsAddress = { kAudioDevicePropertyStreams, kAudioDevicePropertyScopeOutput, kAudioObjectPropertyElementMaster };
    err = AudioObjectGetPropertyDataSize( i_dev_id, &streamsAddress, 0, NULL, &i_param_size );
    if( err != noErr )
    {
        msg_Err( p_aout, "could not get number of streams: [%s] (%i)", (char *)&err, (int32_t)err );
        return false;
    }

    i_streams = i_param_size / sizeof( AudioStreamID );
    p_streams = (AudioStreamID *)malloc( i_param_size );
    if( p_streams == NULL )
        return VLC_ENOMEM;

    err = AudioObjectGetPropertyData( i_dev_id, &streamsAddress, 0, NULL, &i_param_size, p_streams );
    if( err != noErr )
    {
        msg_Err( p_aout, "could not get list of streams: [%s]", (char *)&err );
        return false;
    }

    for( int i = 0; i < i_streams; i++ )
    {
        if( AudioStreamSupportsDigital( p_aout, p_streams[i] ) )
            b_return = true;
    }

    free( p_streams );
    return b_return;
}

/*****************************************************************************
 * AudioStreamSupportsDigital: Check i_stream_id for digital stream support.
 *****************************************************************************/
static int AudioStreamSupportsDigital( audio_output_t *p_aout, AudioStreamID i_stream_id )
{
    OSStatus                    err = noErr;
    UInt32                      i_param_size = 0;
    AudioStreamRangedDescription *p_format_list = NULL;
    int                         i_formats = 0;
    bool                        b_return = false;

    /* Retrieve all the stream formats supported by each output stream */
    AudioObjectPropertyAddress physicalFormatsAddress = { kAudioStreamPropertyAvailablePhysicalFormats, kAudioObjectPropertyScopeGlobal, 0 };
    err = AudioObjectGetPropertyDataSize( i_stream_id, &physicalFormatsAddress, 0, NULL, &i_param_size );
    if( err != noErr )
    {
        msg_Err( p_aout, "could not get number of streamformats: [%s] (%i)", (char *)&err, (int32_t)err );
        return false;
    }

    i_formats = i_param_size / sizeof( AudioStreamRangedDescription );
    msg_Dbg( p_aout, "found %i stream formats", i_formats );

    p_format_list = (AudioStreamRangedDescription *)malloc( i_param_size );
    if( p_format_list == NULL )
        return false;

    err = AudioObjectGetPropertyData( i_stream_id, &physicalFormatsAddress, 0, NULL, &i_param_size, p_format_list );
    if( err != noErr )
    {
        msg_Err( p_aout, "could not get the list of streamformats: [%4.4s]", (char *)&err );
        free( p_format_list);
        p_format_list = NULL;
        return false;
    }

    for( int i = 0; i < i_formats; i++ )
    {
        msg_Dbg( p_aout, STREAM_FORMAT_MSG( "supported format: ", p_format_list[i].mFormat ) );

        if( p_format_list[i].mFormat.mFormatID == 'IAC3' ||
           p_format_list[i].mFormat.mFormatID == 'iac3' ||
           p_format_list[i].mFormat.mFormatID == kAudioFormat60958AC3 ||
           p_format_list[i].mFormat.mFormatID == kAudioFormatAC3 )
        {
            b_return = true;
        }
    }

    free( p_format_list );
    return b_return;
}

/*****************************************************************************
 * AudioStreamChangeFormat: Change i_stream_id to change_format
 *****************************************************************************/
static int AudioStreamChangeFormat( audio_output_t *p_aout, AudioStreamID i_stream_id, AudioStreamBasicDescription change_format )
{
    OSStatus            err = noErr;
    UInt32              i_param_size = 0;

    AudioObjectPropertyAddress physicalFormatAddress = { kAudioStreamPropertyPhysicalFormat, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMaster };

    struct { vlc_mutex_t lock; vlc_cond_t cond; } w;

    msg_Dbg( p_aout, STREAM_FORMAT_MSG( "setting stream format: ", change_format ) );

    /* Condition because SetProperty is asynchronious */
    vlc_cond_init( &w.cond );
    vlc_mutex_init( &w.lock );
    vlc_mutex_lock( &w.lock );

    /* Install the callback */
    err = AudioObjectAddPropertyListener( i_stream_id, &physicalFormatAddress, StreamListener, (void *)&w );
    if( err != noErr )
    {
        msg_Err( p_aout, "AudioObjectAddPropertyListener for kAudioStreamPropertyPhysicalFormat failed: [%4.4s]", (char *)&err );
        return false;
    }

    /* change the format */
    err = AudioObjectSetPropertyData( i_stream_id, &physicalFormatAddress, 0, NULL, sizeof( AudioStreamBasicDescription ),
                                     &change_format );
    if( err != noErr )
    {
        msg_Err( p_aout, "could not set the stream format: [%4.4s]", (char *)&err );
        return false;
    }

    /* The AudioStreamSetProperty is not only asynchronious (requiring the locks)
     * it is also not atomic in its behaviour.
     * Therefore we check 5 times before we really give up.
     * FIXME: failing isn't actually implemented yet. */
    for( int i = 0; i < 5; i++ )
    {
        AudioStreamBasicDescription actual_format;
        mtime_t timeout = mdate() + 500000;

        if( vlc_cond_timedwait( &w.cond, &w.lock, timeout ) )
        {
            msg_Dbg( p_aout, "reached timeout" );
        }

        i_param_size = sizeof( AudioStreamBasicDescription );
        err = AudioObjectGetPropertyData( i_stream_id, &physicalFormatAddress, 0, NULL, &i_param_size, &actual_format );

        msg_Dbg( p_aout, STREAM_FORMAT_MSG( "actual format in use: ", actual_format ) );
        if( actual_format.mSampleRate == change_format.mSampleRate &&
            actual_format.mFormatID == change_format.mFormatID &&
            actual_format.mFramesPerPacket == change_format.mFramesPerPacket )
        {
            /* The right format is now active */
            break;
        }
        /* We need to check again */
    }

    /* Removing the property listener */
    err = AudioObjectRemovePropertyListener( i_stream_id, &physicalFormatAddress, StreamListener, (void *)&w );
    if( err != noErr )
    {
        msg_Err( p_aout, "AudioStreamRemovePropertyListener failed: [%4.4s]", (char *)&err );
        return false;
    }

    /* Destroy the lock and condition */
    vlc_mutex_unlock( &w.lock );
    vlc_mutex_destroy( &w.lock );
    vlc_cond_destroy( &w.cond );

    return true;
}

/*****************************************************************************
 * RenderCallbackAnalog: This function is called everytime the AudioUnit wants
 * us to provide some more audio data.
 * Don't print anything during normal playback, calling blocking function from
 * this callback is not allowed.
 *****************************************************************************/
static OSStatus RenderCallbackAnalog( vlc_object_t *_p_aout,
                                      AudioUnitRenderActionFlags *ioActionFlags,
                                      const AudioTimeStamp *inTimeStamp,
                                      unsigned int inBusNumber,
                                      unsigned int inNumberFrames,
                                      AudioBufferList *ioData )
{
    AudioTimeStamp  host_time;
    mtime_t         current_date = 0;
    uint32_t        i_mData_bytes = 0;

    audio_output_t * p_aout = (audio_output_t *)_p_aout;
    struct aout_sys_t * p_sys = p_aout->sys;

    VLC_UNUSED(ioActionFlags);
    VLC_UNUSED(inBusNumber);
    VLC_UNUSED(inNumberFrames);

    host_time.mFlags = kAudioTimeStampHostTimeValid;
    AudioDeviceTranslateTime( p_sys->i_selected_dev, inTimeStamp, &host_time );

    /* Check for the difference between the Device clock and mdate */
    p_sys->clock_diff = - (mtime_t)
        AudioConvertHostTimeToNanos( AudioGetCurrentHostTime() ) / 1000;
    p_sys->clock_diff += mdate();

    current_date = p_sys->clock_diff +
                   AudioConvertHostTimeToNanos( host_time.mHostTime ) / 1000;
                   //- ((mtime_t) 1000000 / p_aout->format.i_rate * 31 ); // 31 = Latency in Frames. retrieve somewhere

    if( ioData == NULL && ioData->mNumberBuffers < 1 )
    {
        msg_Err( p_aout, "no iodata or buffers");
        return 0;
    }
    if( ioData->mNumberBuffers > 1 )
        msg_Err( p_aout, "well this is weird. seems like there is more than one buffer..." );


    if( p_sys->i_total_bytes > 0 )
    {
        i_mData_bytes = __MIN( p_sys->i_total_bytes - p_sys->i_read_bytes, ioData->mBuffers[0].mDataByteSize );
        vlc_memcpy( ioData->mBuffers[0].mData,
                    &p_sys->p_remainder_buffer[p_sys->i_read_bytes],
                    i_mData_bytes );
        p_sys->i_read_bytes += i_mData_bytes;
        current_date += (mtime_t) ( (mtime_t) 1000000 / p_aout->format.i_rate ) *
                        ( i_mData_bytes / 4 / aout_FormatNbChannels( &p_aout->format )  ); // 4 is fl32 specific

        if( p_sys->i_read_bytes >= p_sys->i_total_bytes )
            p_sys->i_read_bytes = p_sys->i_total_bytes = 0;
    }

    while( i_mData_bytes < ioData->mBuffers[0].mDataByteSize )
    {
        /* We don't have enough data yet */
        aout_buffer_t * p_buffer;
        p_buffer = aout_PacketNext( p_aout, current_date );

        if( p_buffer != NULL )
        {
            uint32_t i_second_mData_bytes = __MIN( p_buffer->i_buffer, ioData->mBuffers[0].mDataByteSize - i_mData_bytes );

            vlc_memcpy( (uint8_t *)ioData->mBuffers[0].mData + i_mData_bytes,
                        p_buffer->p_buffer, i_second_mData_bytes );
            i_mData_bytes += i_second_mData_bytes;

            if( i_mData_bytes >= ioData->mBuffers[0].mDataByteSize )
            {
                p_sys->i_total_bytes = p_buffer->i_buffer - i_second_mData_bytes;
                vlc_memcpy( p_sys->p_remainder_buffer,
                            &p_buffer->p_buffer[i_second_mData_bytes],
                            p_sys->i_total_bytes );
                aout_BufferFree( p_buffer );
                break;
            }
            else
            {
                /* update current_date */
                current_date += (mtime_t) ( (mtime_t) 1000000 / p_aout->format.i_rate ) *
                                ( i_second_mData_bytes / 4 / aout_FormatNbChannels( &p_aout->format )  ); // 4 is fl32 specific
            }
            aout_BufferFree( p_buffer );
        }
        else
        {
            vlc_memset( (uint8_t *)ioData->mBuffers[0].mData +i_mData_bytes,
                       0,ioData->mBuffers[0].mDataByteSize - i_mData_bytes );
            i_mData_bytes += ioData->mBuffers[0].mDataByteSize - i_mData_bytes;
        }
    }
    return( noErr );
}

/*****************************************************************************
 * RenderCallbackSPDIF: callback for SPDIF audio output
 *****************************************************************************/
static OSStatus RenderCallbackSPDIF( AudioDeviceID inDevice,
                                    const AudioTimeStamp * inNow,
                                    const void * inInputData,
                                    const AudioTimeStamp * inInputTime,
                                    AudioBufferList * outOutputData,
                                    const AudioTimeStamp * inOutputTime,
                                    void * threadGlobals )
{
    aout_buffer_t * p_buffer;
    mtime_t         current_date;

    audio_output_t * p_aout = (audio_output_t *)threadGlobals;
    struct aout_sys_t * p_sys = p_aout->sys;

    VLC_UNUSED(inDevice);
    VLC_UNUSED(inInputData);
    VLC_UNUSED(inInputTime);

    /* Check for the difference between the Device clock and mdate */
    p_sys->clock_diff = - (mtime_t)
        AudioConvertHostTimeToNanos( inNow->mHostTime ) / 1000;
    p_sys->clock_diff += mdate();

    current_date = p_sys->clock_diff +
                   AudioConvertHostTimeToNanos( inOutputTime->mHostTime ) / 1000;
                   //- ((mtime_t) 1000000 / p_aout->format.i_rate * 31 ); // 31 = Latency in Frames. retrieve somewhere

    p_buffer = aout_PacketNext( p_aout, current_date );

#define BUFFER outOutputData->mBuffers[p_sys->i_stream_index]
    if( p_buffer != NULL )
    {
        if( (int)BUFFER.mDataByteSize != (int)p_buffer->i_buffer)
            msg_Warn( p_aout, "bytesize: %d nb_bytes: %d", (int)BUFFER.mDataByteSize, (int)p_buffer->i_buffer );

        /* move data into output data buffer */
        vlc_memcpy( BUFFER.mData, p_buffer->p_buffer, p_buffer->i_buffer );
        aout_BufferFree( p_buffer );
    }
    else
    {
        vlc_memset( BUFFER.mData, 0, BUFFER.mDataByteSize );
    }
#undef BUFFER

    return( noErr );
}

/*****************************************************************************
 * HardwareListener: Warns us of changes in the list of registered devices
 *****************************************************************************/
static OSStatus HardwareListener( AudioObjectID inObjectID,  UInt32 inNumberAddresses, const AudioObjectPropertyAddress inAddresses[], void*inClientData)
{
    OSStatus err = noErr;
    audio_output_t     *p_aout = (audio_output_t *)inClientData;
    VLC_UNUSED(inObjectID);

    for ( unsigned int i = 0; i < inNumberAddresses; i++ )
    {
        if( inAddresses[i].mSelector == kAudioHardwarePropertyDevices )
        {
            /* something changed in the list of devices */
            /* We trigger the audio-device's aout_ChannelsRestart callback */
            msg_Warn( p_aout, "audio device configuration changed, resetting cache" );
            var_TriggerCallback( p_aout, "audio-device" );
            var_Destroy( p_aout, "audio-device" );
        }
        else if( inAddresses[i].mSelector == kAudioDevicePropertyDeviceIsAlive )
        {
            msg_Warn( p_aout, "audio device died, resetting aout" );
            var_TriggerCallback( p_aout, "audio-device" );
            var_Destroy( p_aout, "audio-device" );
        } else if (inAddresses[i].mSelector == kAudioStreamPropertyAvailablePhysicalFormats) {
            msg_Warn(p_aout, "available physical formats for audio device changed, resetting aout");
            var_TriggerCallback(p_aout, "audio-device");
            var_Destroy(p_aout, "audio-device");
        }
    }

    return( err );
}

/*****************************************************************************
 * StreamListener
 *****************************************************************************/
static OSStatus StreamListener( AudioObjectID inObjectID,  UInt32 inNumberAddresses, const AudioObjectPropertyAddress inAddresses[], void*inClientData)
{
    OSStatus err = noErr;
    struct { vlc_mutex_t lock; vlc_cond_t cond; } * w = inClientData;

    VLC_UNUSED(inObjectID);

    for( unsigned int i = 0; i < inNumberAddresses; i++ )
    {
        if( inAddresses[i].mSelector == kAudioStreamPropertyPhysicalFormat )
        {
            vlc_mutex_lock( &w->lock );
            vlc_cond_signal( &w->cond );
            vlc_mutex_unlock( &w->lock );
            break;
        }
    }
    return( err );
}

/*****************************************************************************
 * AudioDeviceCallback: Callback triggered when the audio-device variable is changed
 *****************************************************************************/
static int AudioDeviceCallback( vlc_object_t *p_this, const char *psz_variable,
                     vlc_value_t old_val, vlc_value_t new_val, void *param )
{
    audio_output_t *p_aout = (audio_output_t *)p_this;
    var_Set( p_aout->p_libvlc, "macosx-audio-device", new_val );
    msg_Dbg( p_aout, "Set Device: %#"PRIx64, new_val.i_int );
    return aout_ChannelsRestart( p_this, psz_variable, old_val, new_val, param );
}


/*****************************************************************************
 * VolumeSet: Implements pf_volume_set(). Update the CoreAudio AU volume immediately.
 *****************************************************************************/
static int VolumeSet( audio_output_t * p_aout, float volume, bool mute )
{
    struct   aout_sys_t *p_sys = p_aout->sys;
    OSStatus ostatus;

    if( mute )
        volume = 0.0;
    else
        volume = volume * volume * volume; // cubic mapping from output.c

    /* Set volume for output unit */
    ostatus = AudioUnitSetParameter( p_sys->au_unit,
                                     kHALOutputParam_Volume,
                                     kAudioUnitScope_Global,
                                     0,
                                     volume,
                                     0 );

    return ostatus;
}
