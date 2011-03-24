/*****************************************************************************
 * common.h : DirectShow access module for vlc
 *****************************************************************************
 * Copyright (C) 2002 the VideoLAN team
 * $Id: a7cfca057039ad14caf498f42ef3e31b19be4eaf $
 *
 * Author: Gildas Bazin <gbazin@videolan.org>
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
#include <string>
#include <list>
#include <deque>
using namespace std;

#ifndef _MSC_VER
#   include <wtypes.h>
#   include <unknwn.h>
#   include <ole2.h>
#   include <limits.h>
#   ifdef _WINGDI_
#      undef _WINGDI_
#   endif
#   define _WINGDI_ 1
#   define AM_NOVTABLE
#   define _OBJBASE_H_
#   undef _X86_
#   ifndef _I64_MAX
#     define _I64_MAX LONG_LONG_MAX
#   endif
#   define LONGLONG long long
#endif

#include <dshow.h>

typedef struct dshow_stream_t dshow_stream_t;

/****************************************************************************
 * Crossbar stuff
 ****************************************************************************/
#define MAX_CROSSBAR_DEPTH 10

typedef struct CrossbarRouteRec
{
    IAMCrossbar *pXbar;
    LONG        VideoInputIndex;
    LONG        VideoOutputIndex;
    LONG        AudioInputIndex;
    LONG        AudioOutputIndex;

} CrossbarRoute;

void DeleteCrossbarRoutes( access_sys_t * );
HRESULT FindCrossbarRoutes( vlc_object_t *, access_sys_t *,
                            IPin *, LONG, int = 0 );

/****************************************************************************
 * Access descriptor declaration
 ****************************************************************************/
struct access_sys_t
{
    /* These 2 must be left at the beginning */
    vlc_mutex_t lock;
    vlc_cond_t  wait;

    IFilterGraph           *p_graph;
    ICaptureGraphBuilder2  *p_capture_graph_builder2;
    IMediaControl          *p_control;

    int                     i_crossbar_route_depth;
    CrossbarRoute           crossbar_routes[MAX_CROSSBAR_DEPTH];

    /* list of elementary streams */
    dshow_stream_t **pp_streams;
    int            i_streams;
    int            i_current_stream;

    /* misc properties */
    int            i_width;
    int            i_height;
    int            i_chroma;
    bool           b_chroma; /* Force a specific chroma on the dshow input */
};

const GUID IID_IAMBufferNegotiation = {0x56ed71a0, 0xaf5f, 0x11d0, {0xb3, 0xf0, 0x00, 0xaa, 0x00, 0x37, 0x61, 0xc5}};
const GUID IID_IAMTVAudio      = {0x83EC1C30, 0x23D1, 0x11d1, {0x99, 0xE6, 0x00, 0xA0, 0xC9, 0x56, 0x02, 0x66}};
const GUID IID_IAMCrossbar     = {0xC6E13380, 0x30AC, 0x11d0, {0xA1, 0x8C, 0x00, 0xA0, 0xC9, 0x11, 0x89, 0x56}};
const GUID IID_IAMTVTuner      = {0x211A8766, 0x03AC, 0x11d1, {0x8D, 0x13, 0x00, 0xAA, 0x00, 0xBD, 0x83, 0x39}};

/* http://msdn.microsoft.com/en-us/library/dd373441%28v=vs.85%29.aspx */
typedef enum tagAMTunerModeType {
    AMTUNER_MODE_DEFAULT    = 0x0000,
    AMTUNER_MODE_TV         = 0x0001,
    AMTUNER_MODE_FM_RADIO   = 0x0002,
    AMTUNER_MODE_AM_RADIO   = 0x0004,
    AMTUNER_MODE_DSS        = 0x0008
} AMTunerModeType;

typedef enum tagAMTunerSubChannel {
    AMTUNER_SUBCHAN_NO_TUNE = -2,
    AMTUNER_SUBCHAN_DEFAULT = -1
} AMTunerSubChannel;

/* http://msdn.microsoft.com/en-us/library/dd407232%28v=vs.85%29.aspx */
typedef enum tagTunerInputType {
    TunerInputCable = 0,
    TunerInputAntenna = TunerInputCable + 1
} TunerInputType;

#define AMPROPERTY_PIN_CATEGORY 0

/* http://msdn.microsoft.com/en-us/library/dd377421%28v=vs.85%29.aspx */
typedef enum tagPhysicalConnectorType {
    PhysConn_Video_Tuner             = 1,
    PhysConn_Video_Composite,
    PhysConn_Video_SVideo,
    PhysConn_Video_RGB,
    PhysConn_Video_YRYBY,
    PhysConn_Video_SerialDigital,
    PhysConn_Video_ParallelDigital,
    PhysConn_Video_SCSI,
    PhysConn_Video_AUX,
    PhysConn_Video_1394,
    PhysConn_Video_USB,
    PhysConn_Video_VideoDecoder,
    PhysConn_Video_VideoEncoder,
    PhysConn_Video_SCART,
    PhysConn_Video_Black,
    PhysConn_Audio_Tuner             = 4096, /* 0x1000 */
    PhysConn_Audio_Line,
    PhysConn_Audio_Mic,
    PhysConn_Audio_AESDigital,
    PhysConn_Audio_SPDIFDigital,
    PhysConn_Audio_SCSI,
    PhysConn_Audio_AUX,
    PhysConn_Audio_1394,
    PhysConn_Audio_USB,
    PhysConn_Audio_AudioDecoder
} PhysicalConnectorType;

/* http://msdn.microsoft.com/en-us/library/dd389142%28v=vs.85%29.aspx */
#undef INTERFACE
#define INTERFACE IAMBufferNegotiation
DECLARE_INTERFACE_(IAMBufferNegotiation, IUnknown)
{
    STDMETHOD(QueryInterface) (THIS_ REFIID, PVOID*) PURE;
    STDMETHOD_(ULONG,AddRef )(THIS);
    STDMETHOD_(ULONG,Release )(THIS);
    STDMETHOD(SuggestAllocatorProperties )(THIS_ const ALLOCATOR_PROPERTIES *);
    STDMETHOD(GetAllocatorProperties )(THIS_ ALLOCATOR_PROPERTIES *);
};

/* http://msdn.microsoft.com/en-us/library/dd389171%28v=vs.85%29.aspx */
#undef INTERFACE
#define INTERFACE IAMCrossbar
DECLARE_INTERFACE_(IAMCrossbar, IUnknown)
{
    STDMETHOD(QueryInterface) (THIS_ REFIID, PVOID*) PURE;
    STDMETHOD_(ULONG, AddRef) (THIS);
    STDMETHOD_(ULONG, Release) (THIS);
    STDMETHOD(get_PinCounts) (THIS_ long *, long *);
    STDMETHOD(CanRoute) (THIS_ long, long);
    STDMETHOD(Route) (THIS_ long, long);
    STDMETHOD(get_IsRoutedTo) (THIS_ long, long *);
    STDMETHOD(get_CrossbarPinInfo) (THIS_ BOOL, long, long *, long *);
};

/* http://msdn.microsoft.com/en-us/library/dd375971%28v=vs.85%29.aspx */
#undef INTERFACE
#define INTERFACE IAMTVTuner
DECLARE_INTERFACE_(IAMTVTuner, IUnknown)
{
    STDMETHOD(QueryInterface) (THIS_ REFIID, PVOID*) PURE;
    STDMETHOD_(ULONG, AddRef) (THIS);
    STDMETHOD_(ULONG, Release) (THIS);
    STDMETHOD(put_Channel) (THIS_ long, long, long);
    STDMETHOD(get_Channel) (THIS_ long *, long *, long *);
    STDMETHOD(ChannelMinMax) (THIS_ long *, long *);
    STDMETHOD(put_CountryCode) (THIS_ long);
    STDMETHOD(get_CountryCode) (THIS_ long *);
    STDMETHOD(put_TuningSpace) (THIS_ long);
    STDMETHOD(get_TuningSpace) (THIS_ long *);
    STDMETHOD(Logon) (THIS_ HANDLE);
    STDMETHOD(Logout) (IAMTVTuner *);
    STDMETHOD(SignalPresen) (THIS_ long *);
    STDMETHOD(put_Mode) (THIS_ AMTunerModeType);
    STDMETHOD(get_Mode) (THIS_ AMTunerModeType *);
    STDMETHOD(GetAvailableModes) (THIS_ long *);
//    STDMETHOD(RegisterNotificationCallBack) (THIS_ LPAMTUNERNOTIFICATION, long);
//    STDMETHOD(UnRegisterNotificationCallBack) (THIS_ LPAMTUNERNOTIFICATION);
    STDMETHOD(get_AvailableTVFormats) (THIS_ long *);
    STDMETHOD(get_TVFormat) (THIS_ long *);
    STDMETHOD(AutoTune) (THIS_ long, long *);
    STDMETHOD(StoreAutoTune) (IAMTVTuner *);
    STDMETHOD(get_NumInputConnections) (THIS_ long *);
    STDMETHOD(put_InputType) (THIS_ long, TunerInputType);
    STDMETHOD(get_InputType) (THIS_ long, TunerInputType *);
    STDMETHOD(put_ConnectInput) (THIS_ long);
    STDMETHOD(get_ConnectInput) (THIS_ long *);
    STDMETHOD(get_VideoFrequency) (THIS_ long *);
    STDMETHOD(get_AudioFrequency) (THIS_ long *);
};

/* http://msdn.microsoft.com/en-us/library/dd375962%28v=vs.85%29.aspx */
#undef INTERFACE
#define INTERFACE IAMTVAudio
DECLARE_INTERFACE_(IAMTVAudio, IUnknown)
{
    STDMETHOD(QueryInterface) (THIS_ REFIID, PVOID*) PURE;
    STDMETHOD_(ULONG, AddRef) (THIS);
    STDMETHOD_(ULONG, Release) (THIS);
    STDMETHOD(GetHardwareSupportedTVAudioModes) (THIS_ long *);
    STDMETHOD(GetAvailableTVAudioModes) (THIS_ long *);
    STDMETHOD(get_TVAudioMode) (THIS_ long *);
    STDMETHOD(put_TVAudioMode) (THIS_ long);
//    STDMETHOD(RegisterNotificationCallBack) (THIS_ LPAMTUNERNOTIFICATION, long);
//    STDMETHOD(UnRegisterNotificationCallBack) (THIS_ LPAMTUNERNOTIFICATION);
};

/* http://msdn.microsoft.com/en-us/library/dd407352%28v=vs.85%29.aspx */
typedef struct _VIDEO_STREAM_CONFIG_CAPS {
    GUID     guid;
    ULONG    VideoStandard;
    SIZE     InputSize;
    SIZE     MinCroppingSize;
    SIZE     MaxCroppingSize;
    int      CropGranularityX;
    int      CropGranularityY;
    int      CropAlignX;
    int      CropAlignY;
    SIZE     MinOutputSize;
    SIZE     MaxOutputSize;
    int      OutputGranularityX;
    int      OutputGranularityY;
    int      StretchTapsX;
    int      StretchTapsY;
    int      ShrinkTapsX;
    int      ShrinkTapsY;
    LONGLONG MinFrameInterval;
    LONGLONG MaxFrameInterval;
    LONG     MinBitsPerSecond;
    LONG     MaxBitsPerSecond;
} VIDEO_STREAM_CONFIG_CAPS;

/* http://msdn.microsoft.com/en-us/library/dd317597%28v=vs.85%29.aspx */
typedef struct _AUDIO_STREAM_CONFIG_CAPS {
    GUID  guid;
    ULONG MinimumChannels;
    ULONG MaximumChannels;
    ULONG ChannelsGranularity;
    ULONG MinimumBitsPerSample;
    ULONG MaximumBitsPerSample;
    ULONG BitsPerSampleGranularity;
    ULONG MinimumSampleFrequency;
    ULONG MaximumSampleFrequency;
    ULONG SampleFrequencyGranularity;
} AUDIO_STREAM_CONFIG_CAPS;

