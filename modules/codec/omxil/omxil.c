/*****************************************************************************
 * omxil.c: Video decoder module making use of OpenMAX IL components.
 *****************************************************************************
 * Copyright (C) 2010 the VideoLAN team
 * $Id: 0653c5c4ef6553e266eb8cc859148c91e7a5e34d $
 *
 * Authors: Gildas Bazin <gbazin@videolan.org>
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

#if defined(HAVE_DL_DLOPEN)
# include <dlfcn.h>
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_codec.h>
#include <vlc_aout.h>
#include <vlc_block_helper.h>
#include <vlc_cpu.h>

#include "omxil.h"

//#define OMXIL_EXTRA_DEBUG

/*****************************************************************************
 * List of OpenMAX IL core we will try in order
 *****************************************************************************/
static const char *ppsz_dll_list[] =
{
    "libOMX_Core.so", /* TI OMAP IL core */
    "libomxil-bellagio.so",  /* Bellagio IL core */
    0
};

/*****************************************************************************
 * defines
 *****************************************************************************/
#if defined(HAVE_DL_DLOPEN)
# define dll_open(name) dlopen( name, RTLD_NOW )
# define dll_close(handle) dlclose(handle)
#else
# define dll_open(name) (NULL)
# define dll_close(handle) (void)0
#endif

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  OpenDecoder( vlc_object_t * );
static int  OpenEncoder( vlc_object_t * );
static int  OpenGeneric( vlc_object_t *, bool b_encode );
static void CloseGeneric( vlc_object_t * );

static picture_t *DecodeVideo( decoder_t *, block_t ** );
static aout_buffer_t *DecodeAudio ( decoder_t *, block_t ** );
static block_t *EncodeVideo( encoder_t *, picture_t * );

static OMX_ERRORTYPE OmxEventHandler( OMX_HANDLETYPE, OMX_PTR, OMX_EVENTTYPE,
                                      OMX_U32, OMX_U32, OMX_PTR );
static OMX_ERRORTYPE OmxEmptyBufferDone( OMX_HANDLETYPE, OMX_PTR,
                                         OMX_BUFFERHEADERTYPE * );
static OMX_ERRORTYPE OmxFillBufferDone( OMX_HANDLETYPE, OMX_PTR,
                                        OMX_BUFFERHEADERTYPE * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin ()
    set_description( N_("Audio/Video decoder (using OpenMAX IL)") )
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_SCODEC )
    set_section( N_("Decoding") , NULL )
#ifdef HAVE_MAEMO
    set_capability( "decoder", 80 )
#else
    set_capability( "decoder", 0 )
#endif
    set_callbacks( OpenDecoder, CloseGeneric )

    add_submodule ()
    set_section( N_("Encoding") , NULL )
    set_description( N_("Video encoder (using OpenMAX IL)") )
    set_capability( "encoder", 0 )
    set_callbacks( OpenEncoder, CloseGeneric )
vlc_module_end ()

/*****************************************************************************
 * CreateComponentsList: creates a list of components matching the given role
 *****************************************************************************/
static int CreateComponentsList(decoder_t *p_dec, const char *psz_role)
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    char psz_name[OMX_MAX_STRINGNAME_SIZE];
    OMX_ERRORTYPE omx_error;
    OMX_U32 roles = 0;
    OMX_U8 **ppsz_roles = 0;
    unsigned int i, j, len;

    if(!psz_role) goto end;
    len = strlen(psz_role);

    for( i = 0; ; i++ )
    {
        bool b_found = false;

        omx_error = p_sys->pf_component_enum(psz_name, OMX_MAX_STRINGNAME_SIZE, i);
        if(omx_error != OMX_ErrorNone) break;

        msg_Dbg(p_dec, "component %s", psz_name);

        omx_error = p_sys->pf_get_roles_of_component(psz_name, &roles, 0);
        if(omx_error != OMX_ErrorNone || !roles) continue;

        ppsz_roles = malloc(roles * (sizeof(OMX_U8*) + OMX_MAX_STRINGNAME_SIZE));
        if(!ppsz_roles) continue;

        for( j = 0; j < roles; j++ )
            ppsz_roles[j] = ((OMX_U8 *)(&ppsz_roles[roles])) +
                j * OMX_MAX_STRINGNAME_SIZE;

        omx_error = p_sys->pf_get_roles_of_component(psz_name, &roles, ppsz_roles);
        if(omx_error != OMX_ErrorNone) roles = 0;

        for(j = 0; j < roles; j++)
        {
            msg_Dbg(p_dec, "  - role: %s", ppsz_roles[j]);
            if(!strncmp((char *)ppsz_roles[j], psz_role, len)) b_found = true;
        }

        free(ppsz_roles);

        if(!b_found) continue;

        if(p_sys->components >= MAX_COMPONENTS_LIST_SIZE)
        {
            msg_Dbg(p_dec, "too many matching components");
            continue;
        }

        strncpy(p_sys->ppsz_components[p_sys->components], psz_name,
                OMX_MAX_STRINGNAME_SIZE-1);
        p_sys->components++;
    }

 end:
    msg_Dbg(p_dec, "found %i matching components for role %s",
            p_sys->components, psz_role);
    for( i = 0; i < p_sys->components; i++ )
        msg_Dbg(p_dec, "- %s", p_sys->ppsz_components[i]);

    return p_sys->components;
}

/*****************************************************************************
 * ImplementationSpecificWorkarounds: place-holder for implementation
 * specific workarounds
 *****************************************************************************/
static OMX_ERRORTYPE ImplementationSpecificWorkarounds(decoder_t *p_dec,
    OmxPort *p_port, es_format_t *p_fmt)
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    OMX_PARAM_PORTDEFINITIONTYPE *def = &p_port->definition;
    int i_profile = 0xFFFF, i_level = 0xFFFF;

    /* Try to find out the profile of the video */
    while(p_fmt->i_cat == VIDEO_ES && def->eDir == OMX_DirInput &&
          p_fmt->i_codec == VLC_CODEC_H264)
    {
        uint8_t *p = (uint8_t*)p_dec->fmt_in.p_extra;
        if(!p || !p_dec->fmt_in.p_extra) break;

        /* Check the profile / level */
        if(p_dec->fmt_in.i_original_fourcc == VLC_FOURCC('a','v','c','1') &&
           p[0] == 1)
        {
            if(p_dec->fmt_in.i_extra < 12) break;
            p_sys->i_nal_size_length = 1 + (p[4]&0x03);
            if( !(p[5]&0x1f) ) break;
            p += 8;
        }
        else
        {
            if(p_dec->fmt_in.i_extra < 8) break;
            if(!p[0] && !p[1] && !p[2] && p[3] == 1) p += 4;
            else if(!p[0] && !p[1] && p[2] == 1) p += 3;
            else break;
        }

        if( ((*p++)&0x1f) != 7) break;

        /* Get profile/level out of first SPS */
        i_profile = p[0];
        i_level = p[2];
        break;
    }

    if(!strcmp(p_sys->psz_component, "OMX.TI.Video.Decoder"))
    {
        if(p_fmt->i_cat == VIDEO_ES && def->eDir == OMX_DirInput &&
           p_fmt->i_codec == VLC_CODEC_H264 &&
           (i_profile != 66 || i_level > 30))
        {
            msg_Dbg(p_dec, "h264 profile/level not supported (0x%x, 0x%x)",
                    i_profile, i_level);
            return OMX_ErrorNotImplemented;
        }

        if(p_fmt->i_cat == VIDEO_ES && def->eDir == OMX_DirOutput &&
           p_fmt->i_codec == VLC_CODEC_I420)
        {
            /* I420 xvideo is slow on OMAP */
            def->format.video.eColorFormat = OMX_COLOR_FormatCbYCrY;
            GetVlcChromaFormat( def->format.video.eColorFormat,
                                &p_fmt->i_codec, 0 );
            GetVlcChromaSizes( p_fmt->i_codec,
                               def->format.video.nFrameWidth,
                               def->format.video.nFrameHeight,
                               &p_port->i_frame_size, &p_port->i_frame_stride,
                               &p_port->i_frame_stride_chroma_div );
            def->format.video.nStride = p_port->i_frame_stride;
            def->nBufferSize = p_port->i_frame_size;
        }
    }
    else if(!strcmp(p_sys->psz_component, "OMX.st.video_encoder"))
    {
        if(p_fmt->i_cat == VIDEO_ES)
        {
            /* Bellagio's encoder doesn't encode the framerate in Q16 */
            def->format.video.xFramerate >>= 16;
        }
    }

    return OMX_ErrorNone;
}

/*****************************************************************************
 * SetPortDefinition: set definition of the omx port based on the vlc format
 *****************************************************************************/
static OMX_ERRORTYPE SetPortDefinition(decoder_t *p_dec, OmxPort *p_port,
                                       es_format_t *p_fmt)
{
    OMX_PARAM_PORTDEFINITIONTYPE *def = &p_port->definition;
    OMX_ERRORTYPE omx_error;

    omx_error = OMX_GetParameter(p_port->omx_handle,
                                 OMX_IndexParamPortDefinition, def);
    CHECK_ERROR(omx_error, "OMX_GetParameter failed (%x : %s)",
                omx_error, ErrorToString(omx_error));

    switch(p_fmt->i_cat)
    {
    case VIDEO_ES:
        def->format.video.nFrameWidth = p_fmt->video.i_width;
        def->format.video.nFrameHeight = p_fmt->video.i_height;
        if(def->format.video.eCompressionFormat == OMX_VIDEO_CodingUnused)
            def->format.video.nStride = def->format.video.nFrameWidth;
        if( p_fmt->video.i_frame_rate > 0 &&
            p_fmt->video.i_frame_rate_base > 0 )
            def->format.video.xFramerate = (p_fmt->video.i_frame_rate << 16) /
                p_fmt->video.i_frame_rate_base;

        if(def->eDir == OMX_DirInput || p_dec->p_sys->b_enc)
        {
            def->nBufferSize = def->format.video.nFrameWidth *
              def->format.video.nFrameHeight * 2;
            p_port->i_frame_size = def->nBufferSize;

            if(!GetOmxVideoFormat(p_fmt->i_codec,
                                  &def->format.video.eCompressionFormat, 0) )
            {
                if(!GetOmxChromaFormat(p_fmt->i_codec,
                                       &def->format.video.eColorFormat, 0) )
                {
                    omx_error = OMX_ErrorNotImplemented;
                    CHECK_ERROR(omx_error, "codec %4.4s doesn't match any OMX format",
                                (char *)&p_fmt->i_codec );
                }
                GetVlcChromaSizes( p_fmt->i_codec,
                                   def->format.video.nFrameWidth,
                                   def->format.video.nFrameHeight,
                                   &p_port->i_frame_size, &p_port->i_frame_stride,
                                   &p_port->i_frame_stride_chroma_div );
                def->format.video.nStride = p_port->i_frame_stride;
                def->nBufferSize = p_port->i_frame_size;
            }
        }
        else
        {
            if( !GetVlcChromaFormat( def->format.video.eColorFormat,
                                     &p_fmt->i_codec, 0 ) )
            {
                omx_error = OMX_ErrorNotImplemented;
                CHECK_ERROR(omx_error, "OMX color format %i not supported",
                            (int)def->format.video.eColorFormat );
            }
            GetVlcChromaSizes( p_fmt->i_codec,
                               def->format.video.nFrameWidth,
                               def->format.video.nFrameHeight,
                               &p_port->i_frame_size, &p_port->i_frame_stride,
                               &p_port->i_frame_stride_chroma_div );
            def->format.video.nStride = p_port->i_frame_stride;
            def->nBufferSize = p_port->i_frame_size;
        }
        break;

    case AUDIO_ES:
        p_port->i_frame_size = def->nBufferSize;
        if(def->eDir == OMX_DirInput)
        {
            if(!GetOmxAudioFormat(p_fmt->i_codec,
                                  &def->format.audio.eEncoding, 0) )
            {
                omx_error = OMX_ErrorNotImplemented;
                CHECK_ERROR(omx_error, "codec %4.4s doesn't match any OMX format",
                            (char *)&p_fmt->i_codec );
            }
        }
        else
        {
            if( !GetVlcAudioFormat(def->format.audio.eEncoding,
                                   &p_fmt->i_codec, 0 ) )
            {
                omx_error = OMX_ErrorNotImplemented;
                CHECK_ERROR(omx_error, "OMX audio encoding %i not supported",
                            (int)def->format.audio.eEncoding );
            }
        }
        break;

    default: return OMX_ErrorNotImplemented;
    }

    omx_error = ImplementationSpecificWorkarounds(p_dec, p_port, p_fmt);
    CHECK_ERROR(omx_error, "ImplementationSpecificWorkarounds failed (%x : %s)",
                omx_error, ErrorToString(omx_error));

    omx_error = OMX_SetParameter(p_port->omx_handle,
                                 OMX_IndexParamPortDefinition, def);
    CHECK_ERROR(omx_error, "OMX_SetParameter failed (%x : %s)",
                omx_error, ErrorToString(omx_error));

    omx_error = OMX_GetParameter(p_port->omx_handle,
                                 OMX_IndexParamPortDefinition, def);
    CHECK_ERROR(omx_error, "OMX_GetParameter failed (%x : %s)",
                omx_error, ErrorToString(omx_error));

    if(p_port->i_frame_size > def->nBufferSize)
        def->nBufferSize = p_port->i_frame_size;
    p_port->i_frame_size = def->nBufferSize;

    /* Deal with audio params */
    if(p_fmt->i_cat == AUDIO_ES)
    {
        omx_error = SetAudioParameters(p_port->omx_handle,
                                       &p_port->format_param, def->nPortIndex,
                                       def->format.audio.eEncoding,
                                       p_fmt->audio.i_channels,
                                       p_fmt->audio.i_rate,
                                       p_fmt->i_bitrate,
                                       p_fmt->audio.i_bitspersample,
                                       p_fmt->audio.i_blockalign);
        CHECK_ERROR(omx_error, "SetAudioParameters failed (%x : %s)",
                    omx_error, ErrorToString(omx_error));
    }

 error:
    return omx_error;
}

/*****************************************************************************
 * GetPortDefinition: set vlc format based on the definition of the omx port
 *****************************************************************************/
static OMX_ERRORTYPE GetPortDefinition(decoder_t *p_dec, OmxPort *p_port,
                                       es_format_t *p_fmt)
{
    OMX_PARAM_PORTDEFINITIONTYPE *def = &p_port->definition;
    OMX_ERRORTYPE omx_error;

    omx_error = OMX_GetParameter(p_port->omx_handle,
                                 OMX_IndexParamPortDefinition, def);
    CHECK_ERROR(omx_error, "OMX_GetParameter failed (%x : %s)",
                omx_error, ErrorToString(omx_error));

    switch(p_fmt->i_cat)
    {
    case VIDEO_ES:
        p_fmt->video.i_width = def->format.video.nFrameWidth;
        p_fmt->video.i_visible_width = def->format.video.nFrameWidth;
        p_fmt->video.i_height = def->format.video.nFrameHeight;
        p_fmt->video.i_visible_height = def->format.video.nFrameHeight;
        p_fmt->video.i_frame_rate = p_dec->fmt_in.video.i_frame_rate;
        p_fmt->video.i_frame_rate_base = p_dec->fmt_in.video.i_frame_rate_base;

        if(!GetVlcVideoFormat( def->format.video.eCompressionFormat,
                               &p_fmt->i_codec, 0 ) )
        {
            if( !GetVlcChromaFormat( def->format.video.eColorFormat,
                                     &p_fmt->i_codec, 0 ) )
            {
                omx_error = OMX_ErrorNotImplemented;
                CHECK_ERROR(omx_error, "OMX color format %i not supported",
                            (int)def->format.video.eColorFormat );
            }
            GetVlcChromaSizes( p_fmt->i_codec,
                               def->format.video.nFrameWidth,
                               def->format.video.nFrameHeight,
                               &p_port->i_frame_size, &p_port->i_frame_stride,
                               &p_port->i_frame_stride_chroma_div );
        }
        if(p_port->i_frame_size > def->nBufferSize)
            def->nBufferSize = p_port->i_frame_size;
        p_port->i_frame_size = def->nBufferSize;
#if 0
        if((int)p_port->i_frame_stride > def->format.video.nStride)
            def->format.video.nStride = p_port->i_frame_stride;
#endif
        p_port->i_frame_stride = def->format.video.nStride;
        break;

    case AUDIO_ES:
        if( !GetVlcAudioFormat( def->format.audio.eEncoding,
                                &p_fmt->i_codec, 0 ) )
        {
            omx_error = OMX_ErrorNotImplemented;
            CHECK_ERROR(omx_error, "OMX audio format %i not supported",
                        (int)def->format.audio.eEncoding );
        }

        omx_error = GetAudioParameters(p_port->omx_handle,
                                       &p_port->format_param, def->nPortIndex,
                                       def->format.audio.eEncoding,
                                       &p_fmt->audio.i_channels,
                                       &p_fmt->audio.i_rate,
                                       &p_fmt->i_bitrate,
                                       &p_fmt->audio.i_bitspersample,
                                       &p_fmt->audio.i_blockalign);
        CHECK_ERROR(omx_error, "GetAudioParameters failed (%x : %s)",
                    omx_error, ErrorToString(omx_error));

        if(p_fmt->audio.i_channels < 9)
        {
            static const int pi_channels_maps[9] =
            {
                0, AOUT_CHAN_CENTER, AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT,
                AOUT_CHAN_CENTER | AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT,
                AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_REARLEFT
                | AOUT_CHAN_REARRIGHT,
                AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER
                | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT,
                AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER
                | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT | AOUT_CHAN_LFE,
                AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER
                | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT | AOUT_CHAN_MIDDLELEFT
                | AOUT_CHAN_MIDDLERIGHT,
                AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER | AOUT_CHAN_REARLEFT
                | AOUT_CHAN_REARRIGHT | AOUT_CHAN_MIDDLELEFT | AOUT_CHAN_MIDDLERIGHT
                | AOUT_CHAN_LFE
            };
            p_fmt->audio.i_physical_channels =
                p_fmt->audio.i_original_channels =
                    pi_channels_maps[p_fmt->audio.i_channels];
        }

        date_Init( &p_dec->p_sys->end_date, p_fmt->audio.i_rate, 1 );

        break;

    default: return OMX_ErrorNotImplemented;
    }

 error:
    return omx_error;
}

/*****************************************************************************
 * DeinitialiseComponent: Deinitialise and unload an OMX component
 *****************************************************************************/
static OMX_ERRORTYPE DeinitialiseComponent(decoder_t *p_dec,
                                           OMX_HANDLETYPE omx_handle)
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    OMX_ERRORTYPE omx_error;
    OMX_STATETYPE state;
    unsigned int i, j;

    if(!omx_handle) return OMX_ErrorNone;

    omx_error = OMX_GetState(omx_handle, &state);
    CHECK_ERROR(omx_error, "ONX_GetState failed (%x)", omx_error );

    if(state == OMX_StateExecuting)
    {
        omx_error = OMX_SendCommand( omx_handle, OMX_CommandStateSet,
                                     OMX_StateIdle, 0 );
        CHECK_ERROR(omx_error, "ONX_CommandStateSet Idle failed (%x)", omx_error );
        omx_error = WaitForSpecificOmxEvent(p_dec, OMX_EventCmdComplete, 0, 0, 0);
        CHECK_ERROR(omx_error, "Wait for Idle failed (%x)", omx_error );
    }

    omx_error = OMX_GetState(omx_handle, &state);
    CHECK_ERROR(omx_error, "ONX_GetState failed (%x)", omx_error );

    if(state == OMX_StateIdle)
    {
        omx_error = OMX_SendCommand( omx_handle, OMX_CommandStateSet,
                                     OMX_StateLoaded, 0 );
        CHECK_ERROR(omx_error, "ONX_CommandStateSet Loaded failed (%x)", omx_error );

        for(i = 0; i < p_sys->ports; i++)
        {
            OmxPort *p_port = &p_sys->p_ports[i];
            OMX_BUFFERHEADERTYPE *p_buffer;

            for(j = 0; j < p_port->i_buffers; j++)
            {
                OMX_FIFO_GET(&p_port->fifo, p_buffer);
                omx_error = OMX_FreeBuffer( omx_handle,
                                            p_port->i_port_index, p_buffer );

                if(omx_error != OMX_ErrorNone) break;
            }
            CHECK_ERROR(omx_error, "ONX_FreeBuffer failed (%x, %i, %i)",
                        omx_error, (int)p_port->i_port_index, j );
        }

        omx_error = WaitForSpecificOmxEvent(p_dec, OMX_EventCmdComplete, 0, 0, 0);
        CHECK_ERROR(omx_error, "Wait for Loaded failed (%x)", omx_error );
    }

 error:
    for(i = 0; i < p_sys->ports; i++)
    {
        OmxPort *p_port = &p_sys->p_ports[i];
        free(p_port->pp_buffers);
        p_port->pp_buffers = 0;
    }
    omx_error = p_sys->pf_free_handle( omx_handle );
    return omx_error;
}

/*****************************************************************************
 * InitialiseComponent: Load and initialise an OMX component
 *****************************************************************************/
static OMX_ERRORTYPE InitialiseComponent(decoder_t *p_dec,
    OMX_STRING psz_component, OMX_HANDLETYPE *p_handle)
{
    static OMX_CALLBACKTYPE callbacks =
        { OmxEventHandler, OmxEmptyBufferDone, OmxFillBufferDone };
    decoder_sys_t *p_sys = p_dec->p_sys;
    OMX_HANDLETYPE omx_handle;
    OMX_ERRORTYPE omx_error;
    unsigned int i;
    OMX_U8 psz_role[OMX_MAX_STRINGNAME_SIZE];
    OMX_PARAM_COMPONENTROLETYPE role;
    OMX_PARAM_PORTDEFINITIONTYPE definition;
    OMX_PORT_PARAM_TYPE param;

    /* Load component */
    omx_error = p_sys->pf_get_handle( &omx_handle, psz_component,
                                      p_dec, &callbacks );
    if(omx_error != OMX_ErrorNone)
    {
        msg_Warn( p_dec, "OMX_GetHandle(%s) failed (%x: %s)", psz_component,
                  omx_error, ErrorToString(omx_error) );
        return omx_error;
    }
    strncpy(p_sys->psz_component, psz_component, OMX_MAX_STRINGNAME_SIZE-1);

    OMX_ComponentRoleEnum(omx_handle, psz_role, 0);
    msg_Dbg(p_dec, "loaded component %s of role %s", psz_component, psz_role);
    PrintOmx(p_dec, omx_handle, OMX_ALL);

    /* Set component role */
    OMX_INIT_STRUCTURE(role);
    strcpy((char*)role.cRole,
           GetOmxRole(p_sys->b_enc ? p_dec->fmt_out.i_codec : p_dec->fmt_in.i_codec,
                      p_dec->fmt_in.i_cat, p_sys->b_enc));

    omx_error = OMX_SetParameter(omx_handle, OMX_IndexParamStandardComponentRole,
                                 &role);
    omx_error = OMX_GetParameter(omx_handle, OMX_IndexParamStandardComponentRole,
                                 &role);
    if(omx_error == OMX_ErrorNone)
        msg_Dbg(p_dec, "component standard role set to %s", role.cRole);

    /* Find the input / output ports */
    OMX_INIT_STRUCTURE(param);
    OMX_INIT_STRUCTURE(definition);
    omx_error = OMX_GetParameter(omx_handle, p_dec->fmt_in.i_cat == VIDEO_ES ?
                                 OMX_IndexParamVideoInit : OMX_IndexParamAudioInit, &param);
    if(omx_error != OMX_ErrorNone) param.nPorts = 0;

    for(i = 0; i < param.nPorts; i++)
    {
        OmxPort *p_port;

        /* Get port definition */
        definition.nPortIndex = param.nStartPortNumber + i;
        omx_error = OMX_GetParameter(omx_handle, OMX_IndexParamPortDefinition,
                                     &definition);
        if(omx_error != OMX_ErrorNone) continue;

        if(definition.eDir == OMX_DirInput) p_port = &p_sys->in;
        else  p_port = &p_sys->out;

        p_port->b_valid = true;
        p_port->i_port_index = definition.nPortIndex;
        p_port->definition = definition;
        p_port->omx_handle = omx_handle;
    }

    if(!p_sys->in.b_valid || !p_sys->out.b_valid)
    {
        omx_error = OMX_ErrorInvalidComponent;
        CHECK_ERROR(omx_error, "couldn't find an input and output port");
    }

    /* Set port definitions */
    for(i = 0; i < p_sys->ports; i++)
    {
        omx_error = SetPortDefinition(p_dec, &p_sys->p_ports[i],
                                      p_sys->p_ports[i].p_fmt);
        if(omx_error != OMX_ErrorNone) goto error;
    }

    /* Allocate our array for the omx buffers and enable ports */
    for(i = 0; i < p_sys->ports; i++)
    {
        OmxPort *p_port = &p_sys->p_ports[i];

        p_port->pp_buffers =
            malloc(p_port->definition.nBufferCountActual *
                   sizeof(OMX_BUFFERHEADERTYPE*));
        if(!p_port->pp_buffers)
        {
          omx_error = OMX_ErrorInsufficientResources;
          CHECK_ERROR(omx_error, "memory allocation failed");
        }
        p_port->i_buffers = p_port->definition.nBufferCountActual;

        /* Enable port */
        if(!p_port->definition.bEnabled)
        {
            omx_error = OMX_SendCommand( omx_handle, OMX_CommandPortEnable,
                                         p_port->i_port_index, NULL);
            CHECK_ERROR(omx_error, "ONX_CommandPortEnable on %i failed (%x)",
                        (int)p_port->i_port_index, omx_error );
            omx_error = WaitForSpecificOmxEvent(p_dec, OMX_EventCmdComplete, 0, 0, 0);
            CHECK_ERROR(omx_error, "Wait for PortEnable on %i failed (%x)",
                        (int)p_port->i_port_index, omx_error );
        }
    }

    *p_handle = omx_handle;
    return OMX_ErrorNone;

 error:
    DeinitialiseComponent(p_dec, omx_handle);
    *p_handle = 0;
    return omx_error;
}

/*****************************************************************************
 * OpenDecoder: Create the decoder instance
 *****************************************************************************/
static int OpenDecoder( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t*)p_this;
    int status;

    if( 0 || !GetOmxRole(p_dec->fmt_in.i_codec, p_dec->fmt_in.i_cat, false) )
        return VLC_EGENERIC;

#ifdef HAVE_MAEMO
    if( p_dec->fmt_in.i_cat != VIDEO_ES && !p_dec->b_force)
        return VLC_EGENERIC;
#endif

    status = OpenGeneric( p_this, false );
    if(status != VLC_SUCCESS) return status;

    p_dec->pf_decode_video = DecodeVideo;
    p_dec->pf_decode_audio = DecodeAudio;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * OpenEncoder: Create the encoder instance
 *****************************************************************************/
static int OpenEncoder( vlc_object_t *p_this )
{
    encoder_t *p_enc = (encoder_t*)p_this;
    int status;

    if( !GetOmxRole(p_enc->fmt_out.i_codec, p_enc->fmt_in.i_cat, true) )
        return VLC_EGENERIC;

    status = OpenGeneric( p_this, true );
    if(status != VLC_SUCCESS) return status;

    p_enc->pf_encode_video = EncodeVideo;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * OpenGeneric: Create the generic decoder/encoder instance
 *****************************************************************************/
static int OpenGeneric( vlc_object_t *p_this, bool b_encode )
{
    void *dll_handle = 0, *pf_init = 0, *pf_deinit = 0;
    void *pf_get_handle = 0, *pf_free_handle = 0, *pf_component_enum = 0;
    void *pf_get_roles_of_component = 0;
    decoder_t *p_dec = (decoder_t*)p_this;
    decoder_sys_t *p_sys;
    OMX_ERRORTYPE omx_error;
    OMX_BUFFERHEADERTYPE *p_header;
    unsigned int i, j;

    /* Load the OMX core */
    for( i = 0; ppsz_dll_list[i]; i++ )
    {
        dll_handle = dll_open( ppsz_dll_list[i] );
        if( dll_handle ) break;
    }
    if( !dll_handle ) return VLC_EGENERIC;

    pf_init = dlsym( dll_handle, "OMX_Init" );
    pf_deinit = dlsym( dll_handle, "OMX_Deinit" );
    pf_get_handle = dlsym( dll_handle, "OMX_GetHandle" );
    pf_free_handle = dlsym( dll_handle, "OMX_FreeHandle" );
    pf_component_enum = dlsym( dll_handle, "OMX_ComponentNameEnum" );
    pf_get_roles_of_component = dlsym( dll_handle, "OMX_GetRolesOfComponent" );
    if( !pf_init || !pf_deinit || !pf_get_handle || !pf_free_handle ||
        !pf_component_enum || !pf_get_roles_of_component )
    {
        msg_Warn( p_this, "cannot find OMX_* symbols in `%s' (%s)",
                  ppsz_dll_list[i], dlerror() );
        dll_close(dll_handle);
    }

    if( !pf_init || !pf_deinit || !pf_get_handle || !pf_free_handle || !pf_component_enum )
    {
        dll_close(dll_handle);
        return VLC_EGENERIC;
    }

    /* Allocate the memory needed to store the decoder's structure */
    if( ( p_dec->p_sys = p_sys = calloc( 1, sizeof(*p_sys)) ) == NULL )
    {
        dll_close(dll_handle);
        return VLC_ENOMEM;
    }

    /* Initialise the thread properties */
    if(!b_encode)
    {
        p_dec->fmt_out.i_cat = p_dec->fmt_in.i_cat;
        p_dec->fmt_out.video = p_dec->fmt_in.video;
        p_dec->fmt_out.audio = p_dec->fmt_in.audio;
        p_dec->fmt_out.i_codec = 0;
    }
    p_sys->b_enc = b_encode;
    p_sys->dll_handle = dll_handle;
    p_sys->pf_init = pf_init;
    p_sys->pf_deinit = pf_deinit;
    p_sys->pf_get_handle = pf_get_handle;
    p_sys->pf_free_handle = pf_free_handle;
    p_sys->pf_component_enum = pf_component_enum;
    p_sys->pf_get_roles_of_component = pf_get_roles_of_component;
    p_sys->pp_last_event = &p_sys->p_events;
    vlc_mutex_init (&p_sys->mutex);
    vlc_cond_init (&p_sys->cond);
    vlc_mutex_init (&p_sys->lock);
    vlc_mutex_init (&p_sys->in.fifo.lock);
    vlc_cond_init (&p_sys->in.fifo.wait);
    p_sys->in.fifo.offset = offsetof(OMX_BUFFERHEADERTYPE, pOutputPortPrivate) / sizeof(void *);
    p_sys->in.fifo.pp_last = &p_sys->in.fifo.p_first;
    p_sys->in.b_direct = false;
    p_sys->in.b_flushed = true;
    p_sys->in.p_fmt = &p_dec->fmt_in;
    vlc_mutex_init (&p_sys->out.fifo.lock);
    vlc_cond_init (&p_sys->out.fifo.wait);
    p_sys->out.fifo.offset = offsetof(OMX_BUFFERHEADERTYPE, pInputPortPrivate) / sizeof(void *);
    p_sys->out.fifo.pp_last = &p_sys->out.fifo.p_first;
    p_sys->out.b_direct = true;
    p_sys->out.b_flushed = true;
    p_sys->out.p_fmt = &p_dec->fmt_out;
    p_sys->ports = 2;
    p_sys->p_ports = &p_sys->in;

    msg_Dbg(p_dec, "fmt in:%4.4s, out: %4.4s", (char *)&p_dec->fmt_in.i_codec,
            (char *)&p_dec->fmt_out.i_codec);

    /* Initialise the OMX core */
    omx_error = p_sys->pf_init();
    if(omx_error != OMX_ErrorNone)
    {
        msg_Warn( p_this, "OMX_Init failed (%x: %s)", omx_error,
                  ErrorToString(omx_error) );
        CloseGeneric(p_this);
        return VLC_EGENERIC;
    }
    p_sys->b_init = true;

    /* Enumerate components and build a list of the one we want to try */
    if( !CreateComponentsList(p_dec,
             GetOmxRole(p_sys->b_enc ? p_dec->fmt_out.i_codec :
                        p_dec->fmt_in.i_codec, p_dec->fmt_in.i_cat,
                        p_sys->b_enc)) )
    {
        msg_Warn( p_this, "couldn't find an omx component for codec %4.4s",
                  (char *)&p_dec->fmt_in.i_codec );
        CloseGeneric(p_this);
        return VLC_EGENERIC;
    }

    /* Try to load and initialise a component */
    for(i = 0; i < p_sys->components; i++)
    {
        omx_error = InitialiseComponent(p_dec, p_sys->ppsz_components[i],
                                        &p_sys->omx_handle);
        if(omx_error == OMX_ErrorNone) break;
    }
    CHECK_ERROR(omx_error, "no component could be initialised" );

    /* Move component to Idle then Executing state */
    OMX_SendCommand( p_sys->omx_handle, OMX_CommandStateSet, OMX_StateIdle, 0 );
    CHECK_ERROR(omx_error, "ONX_CommandStateSet Idle failed (%x)", omx_error );

    /* Allocate omx buffers */
    for(i = 0; i < p_sys->ports; i++)
    {
        OmxPort *p_port = &p_sys->p_ports[i];

        for(j = 0; j < p_port->i_buffers; j++)
        {
#if 0
#define ALIGN(x,BLOCKLIGN) (((x) + BLOCKLIGN - 1) & ~(BLOCKLIGN - 1))
            char *p_buf = malloc(p_port->definition.nBufferSize +
                                 p_port->definition.nBufferAlignment);
            p_port->pp_buffers[i] = (void *)ALIGN((uintptr_t)p_buf, p_port->definition.nBufferAlignment);
#endif

            if(0 && p_port->b_direct)
                omx_error =
                    OMX_UseBuffer( p_sys->omx_handle, &p_port->pp_buffers[j],
                                   p_port->i_port_index, 0,
                                   p_port->definition.nBufferSize, (void*)1);
            else
                omx_error =
                    OMX_AllocateBuffer( p_sys->omx_handle, &p_port->pp_buffers[j],
                                        p_port->i_port_index, 0,
                                        p_port->definition.nBufferSize);

            if(omx_error != OMX_ErrorNone) break;
            OMX_FIFO_PUT(&p_port->fifo, p_port->pp_buffers[j]);
        }
        p_port->i_buffers = j;
        CHECK_ERROR(omx_error, "ONX_UseBuffer failed (%x, %i, %i)",
                    omx_error, (int)p_port->i_port_index, j );
    }

    omx_error = WaitForSpecificOmxEvent(p_dec, OMX_EventCmdComplete, 0, 0, 0);
    CHECK_ERROR(omx_error, "Wait for Idle failed (%x)", omx_error );

    omx_error = OMX_SendCommand( p_sys->omx_handle, OMX_CommandStateSet,
                                 OMX_StateExecuting, 0);
    CHECK_ERROR(omx_error, "ONX_CommandStateSet Executing failed (%x)", omx_error );
    omx_error = WaitForSpecificOmxEvent(p_dec, OMX_EventCmdComplete, 0, 0, 0);
    CHECK_ERROR(omx_error, "Wait for Executing failed (%x)", omx_error );

    /* Send codec configuration data */
    if( p_dec->fmt_in.i_extra )
    {
        OMX_FIFO_GET(&p_sys->in.fifo, p_header);
        p_header->nFilledLen = p_dec->fmt_in.i_extra;

        if(p_sys->in.b_direct)
        {
            p_header->pOutputPortPrivate = p_header->pBuffer;
            p_header->pBuffer = p_dec->fmt_in.p_extra;
        }
        else
        {
            if(p_header->nFilledLen > p_header->nAllocLen)
            {
                msg_Dbg(p_dec, "buffer too small (%i,%i)", (int)p_header->nFilledLen,
                        (int)p_header->nAllocLen);
                p_header->nFilledLen = p_header->nAllocLen;
            }
            memcpy(p_header->pBuffer, p_dec->fmt_in.p_extra, p_header->nFilledLen);
        }

        p_header->nOffset = 0;
        p_header->nFlags = OMX_BUFFERFLAG_CODECCONFIG;
        msg_Dbg(p_dec, "sending codec config data %p, %p, %i", p_header,
                p_header->pBuffer, (int)p_header->nFilledLen);
        OMX_EmptyThisBuffer(p_sys->omx_handle, p_header);
    }

    /* Get back output port definition */
    omx_error = GetPortDefinition(p_dec, &p_sys->out, p_sys->out.p_fmt);
    if(omx_error != OMX_ErrorNone) goto error;

    PrintOmx(p_dec, p_sys->omx_handle, p_dec->p_sys->in.i_port_index);
    PrintOmx(p_dec, p_sys->omx_handle, p_dec->p_sys->out.i_port_index);

    if(p_sys->b_error) goto error;

    p_dec->b_need_packetized = true;
    return VLC_SUCCESS;

 error:
    CloseGeneric(p_this);
    return VLC_EGENERIC;
}

/*****************************************************************************
 * PortReconfigure
 *****************************************************************************/
static OMX_ERRORTYPE PortReconfigure(decoder_t *p_dec, OmxPort *p_port)
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    OMX_PARAM_PORTDEFINITIONTYPE definition;
    OMX_BUFFERHEADERTYPE *p_buffer;
    OMX_ERRORTYPE omx_error;
    unsigned int i;

    /* Sanity checking */
    OMX_INIT_STRUCTURE(definition);
    definition.nPortIndex = p_port->i_port_index;
    omx_error = OMX_GetParameter(p_dec->p_sys->omx_handle, OMX_IndexParamPortDefinition,
                                 &definition);
    if(omx_error != OMX_ErrorNone ||
       !definition.format.video.nFrameWidth ||
       !definition.format.video.nFrameHeight )
        return OMX_ErrorUndefined;

    omx_error = OMX_SendCommand( p_sys->omx_handle, OMX_CommandPortDisable,
                                 p_port->i_port_index, NULL);
    CHECK_ERROR(omx_error, "ONX_CommandPortDisable on %i failed (%x)",
                (int)p_port->i_port_index, omx_error );

    for(i = 0; i < p_port->i_buffers; i++)
    {
        OMX_FIFO_GET(&p_port->fifo, p_buffer);
        omx_error = OMX_FreeBuffer( p_sys->omx_handle,
                                    p_port->i_port_index, p_buffer );

        if(omx_error != OMX_ErrorNone) break;
    }
    CHECK_ERROR(omx_error, "ONX_FreeBuffer failed (%x, %i, %i)",
                omx_error, (int)p_port->i_port_index, i );

    omx_error = WaitForSpecificOmxEvent(p_dec, OMX_EventCmdComplete, 0, 0, 0);
    CHECK_ERROR(omx_error, "Wait for PortDisable failed (%x)", omx_error );

    /* Get the new port definition */
    omx_error = GetPortDefinition(p_dec, &p_sys->out, p_sys->out.p_fmt);
    if(omx_error != OMX_ErrorNone) goto error;
    omx_error = OMX_SetParameter(p_dec->p_sys->omx_handle, OMX_IndexParamPortDefinition,
                                 &definition);
    CHECK_ERROR(omx_error, "OMX_SetParameter failed (%x : %s)",
                omx_error, ErrorToString(omx_error));

    omx_error = OMX_SendCommand( p_sys->omx_handle, OMX_CommandPortEnable,
                                 p_port->i_port_index, NULL);
    CHECK_ERROR(omx_error, "ONX_CommandPortEnable on %i failed (%x)",
                (int)p_port->i_port_index, omx_error );

    for(i = 0; i < p_port->i_buffers; i++)
    {
        if(0 && p_port->b_direct)
            omx_error =
                OMX_UseBuffer( p_sys->omx_handle, &p_port->pp_buffers[i],
                               p_port->i_port_index, 0,
                               p_port->definition.nBufferSize, (void*)1);
        else
            omx_error =
                OMX_AllocateBuffer( p_sys->omx_handle, &p_port->pp_buffers[i],
                                    p_port->i_port_index, 0,
                                    p_port->definition.nBufferSize);

        if(omx_error != OMX_ErrorNone) break;
        OMX_FIFO_PUT(&p_port->fifo, p_port->pp_buffers[i]);
    }
    p_port->i_buffers = i;
    CHECK_ERROR(omx_error, "ONX_UseBuffer failed (%x, %i, %i)",
                omx_error, (int)p_port->i_port_index, i );

    omx_error = WaitForSpecificOmxEvent(p_dec, OMX_EventCmdComplete, 0, 0, 0);
    CHECK_ERROR(omx_error, "Wait for PortEnable failed (%x)", omx_error );

    PrintOmx(p_dec, p_sys->omx_handle, p_dec->p_sys->in.i_port_index);
    PrintOmx(p_dec, p_sys->omx_handle, p_dec->p_sys->out.i_port_index);

 error:
    return omx_error;
}

/*****************************************************************************
 * DecodeVideo: Called to decode one frame
 *****************************************************************************/
static picture_t *DecodeVideo( decoder_t *p_dec, block_t **pp_block )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    picture_t *p_pic = NULL, *p_next_pic;
    OMX_ERRORTYPE omx_error;
    unsigned int i;

    OMX_BUFFERHEADERTYPE *p_header;
    block_t *p_block;

    if( !pp_block || !*pp_block )
        return NULL;

    p_block = *pp_block;

    /* Check for errors from codec */
    if(p_sys->b_error)
    {
        msg_Dbg(p_dec, "error during decoding");
        block_Release( p_block );
        return 0;
    }

    if( p_block->i_flags & (BLOCK_FLAG_DISCONTINUITY|BLOCK_FLAG_CORRUPTED) )
    {
        block_Release( p_block );
        if(!p_sys->in.b_flushed)
        {
            msg_Dbg(p_dec, "flushing");
            OMX_SendCommand( p_sys->omx_handle, OMX_CommandFlush,
                             p_sys->in.definition.nPortIndex, 0 );
        }
        p_sys->in.b_flushed = true;
        return NULL;
    }

    /* Take care of decoded frames first */
    while(!p_pic)
    {
        OMX_FIFO_PEEK(&p_sys->out.fifo, p_header);
        if(!p_header) break; /* No frame available */

        if(p_header->nFilledLen)
        {
            p_pic = p_header->pAppPrivate;
            if(!p_pic)
            {
                /* We're not in direct rendering mode.
                 * Get a new picture and copy the content */
                p_pic = decoder_NewPicture( p_dec );
                if( !p_pic ) break; /* No picture available */

                CopyOmxPicture(p_dec, p_pic, p_header);
            }

            p_pic->date = p_header->nTimeStamp;
            p_header->nFilledLen = 0;
            p_header->pAppPrivate = 0;
        }

        /* Get a new picture */
        if(p_sys->in.b_direct && !p_header->pAppPrivate)
        {
            p_next_pic = decoder_NewPicture( p_dec );
            if(!p_next_pic) break;

            OMX_FIFO_GET(&p_sys->out.fifo, p_header);
            p_header->pAppPrivate = p_next_pic;
            p_header->pInputPortPrivate = p_header->pBuffer;
            p_header->pBuffer = p_next_pic->p[0].p_pixels;
        }
        else
        {
            OMX_FIFO_GET(&p_sys->out.fifo, p_header);
        }

#ifdef OMXIL_EXTRA_DEBUG
        msg_Dbg( p_dec, "FillThisBuffer %p, %p", p_header, p_header->pBuffer );
#endif
        OMX_FillThisBuffer(p_sys->omx_handle, p_header);
    }

    /* Send the input buffer to the component */
    OMX_FIFO_GET(&p_sys->in.fifo, p_header);
    if(p_header)
    {
        p_header->nFilledLen = p_block->i_buffer;
        p_header->nOffset = 0;
        p_header->nFlags = OMX_BUFFERFLAG_ENDOFFRAME;
        p_header->nTimeStamp = p_block->i_dts;

        /* In direct mode we pass the input pointer as is.
         * Otherwise we memcopy the data */
        if(p_sys->in.b_direct)
        {
            p_header->pOutputPortPrivate = p_header->pBuffer;
            p_header->pBuffer = p_block->p_buffer;
            p_header->pAppPrivate = p_block;
        }
        else
        {
            if(p_header->nFilledLen > p_header->nAllocLen)
            {
                msg_Dbg(p_dec, "buffer too small (%i,%i)",
                        (int)p_header->nFilledLen, (int)p_header->nAllocLen);
                p_header->nFilledLen = p_header->nAllocLen;
            }
            memcpy(p_header->pBuffer, p_block->p_buffer, p_header->nFilledLen );
            block_Release(p_block);
        }

#ifdef OMXIL_EXTRA_DEBUG
        msg_Dbg( p_dec, "EmptyThisBuffer %p, %p, %i", p_header, p_header->pBuffer,
                 (int)p_header->nFilledLen );
#endif
        OMX_EmptyThisBuffer(p_sys->omx_handle, p_header);
        p_sys->in.b_flushed = false;
        *pp_block = NULL; /* Avoid being fed the same packet again */
    }

    /* Handle the PortSettingsChanged events */
    for(i = 0; i < p_sys->ports; i++)
    {
        OmxPort *p_port = &p_sys->p_ports[i];
        if(!p_port->b_reconfigure) continue;
        p_port->b_reconfigure = 0;
        omx_error = PortReconfigure(p_dec, p_port);
    }

    return p_pic;
}

/*****************************************************************************
 * DecodeAudio: Called to decode one frame
 *****************************************************************************/
aout_buffer_t *DecodeAudio ( decoder_t *p_dec, block_t **pp_block )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    aout_buffer_t *p_buffer = 0;
    OMX_BUFFERHEADERTYPE *p_header;
    OMX_ERRORTYPE omx_error;
    block_t *p_block;
    unsigned int i;

    if( !pp_block || !*pp_block ) return NULL;

    p_block = *pp_block;

    /* Check for errors from codec */
    if(p_sys->b_error)
    {
        msg_Dbg(p_dec, "error during decoding");
        block_Release( p_block );
        return 0;
    }

    if( p_block->i_flags & (BLOCK_FLAG_DISCONTINUITY|BLOCK_FLAG_CORRUPTED) )
    {
        block_Release( p_block );
        date_Set( &p_sys->end_date, 0 );
        if(!p_sys->in.b_flushed)
        {
            msg_Dbg(p_dec, "flushing");
            OMX_SendCommand( p_sys->omx_handle, OMX_CommandFlush,
                             p_sys->in.definition.nPortIndex, 0 );
        }
        p_sys->in.b_flushed = true;
        return NULL;
    }

    if( !date_Get( &p_sys->end_date ) )
    {
        if( !p_block->i_pts )
        {
            /* We've just started the stream, wait for the first PTS. */
            block_Release( p_block );
            return NULL;
        }
        date_Set( &p_sys->end_date, p_block->i_pts );
    }

    /* Take care of decoded frames first */
    while(!p_buffer)
    {
        unsigned int i_samples;

        OMX_FIFO_PEEK(&p_sys->out.fifo, p_header);
        if(!p_header) break; /* No frame available */

        i_samples = p_header->nFilledLen / p_sys->out.p_fmt->audio.i_channels / 2;
        if(i_samples)
        {
            p_buffer = decoder_NewAudioBuffer( p_dec, i_samples );
            if( !p_buffer ) break; /* No audio buffer available */

            memcpy( p_buffer->p_buffer, p_header->pBuffer, p_buffer->i_buffer );
            p_header->nFilledLen = 0;

            if( p_header->nTimeStamp != 0 &&
                p_header->nTimeStamp != date_Get( &p_sys->end_date ) )
                date_Set( &p_sys->end_date, p_header->nTimeStamp );

            p_buffer->i_pts = date_Get( &p_sys->end_date );
            p_buffer->i_length = date_Increment( &p_sys->end_date, i_samples ) -
                p_buffer->i_pts;
        }

#ifdef OMXIL_EXTRA_DEBUG
        msg_Dbg( p_dec, "FillThisBuffer %p, %p", p_header, p_header->pBuffer );
#endif
        OMX_FIFO_GET(&p_sys->out.fifo, p_header);
        OMX_FillThisBuffer(p_sys->omx_handle, p_header);
    }


    /* Send the input buffer to the component */
    OMX_FIFO_GET(&p_sys->in.fifo, p_header);
    if(p_header)
    {
        p_header->nFilledLen = p_block->i_buffer;
        p_header->nOffset = 0;
        p_header->nFlags = OMX_BUFFERFLAG_ENDOFFRAME;
        p_header->nTimeStamp = p_block->i_dts;

        /* In direct mode we pass the input pointer as is.
         * Otherwise we memcopy the data */
        if(p_sys->in.b_direct)
        {
            p_header->pOutputPortPrivate = p_header->pBuffer;
            p_header->pBuffer = p_block->p_buffer;
            p_header->pAppPrivate = p_block;
        }
        else
        {
            if(p_header->nFilledLen > p_header->nAllocLen)
            {
                msg_Dbg(p_dec, "buffer too small (%i,%i)",
                        (int)p_header->nFilledLen, (int)p_header->nAllocLen);
                p_header->nFilledLen = p_header->nAllocLen;
            }
            memcpy(p_header->pBuffer, p_block->p_buffer, p_header->nFilledLen );
            block_Release(p_block);
        }

#ifdef OMXIL_EXTRA_DEBUG
        msg_Dbg( p_dec, "EmptyThisBuffer %p, %p, %i", p_header, p_header->pBuffer,
                 (int)p_header->nFilledLen );
#endif
        OMX_EmptyThisBuffer(p_sys->omx_handle, p_header);
        p_sys->in.b_flushed = false;
        *pp_block = NULL; /* Avoid being fed the same packet again */
    }

    /* Handle the PortSettingsChanged events */
    for(i = 0; i < p_sys->ports; i++)
    {
        OmxPort *p_port = &p_sys->p_ports[i];
        if(!p_port->b_reconfigure) continue;
        p_port->b_reconfigure = 0;
        omx_error = PortReconfigure(p_dec, p_port);
    }

    return p_buffer;
}

/*****************************************************************************
 * EncodeVideo: Called to encode one frame
 *****************************************************************************/
static block_t *EncodeVideo( encoder_t *p_enc, picture_t *p_pic )
{
    decoder_t *p_dec = ( decoder_t *)p_enc;
    decoder_sys_t *p_sys = p_dec->p_sys;
    OMX_ERRORTYPE omx_error;
    unsigned int i;

    OMX_BUFFERHEADERTYPE *p_header;
    block_t *p_block = 0;

    /* Check for errors from codec */
    if(p_sys->b_error)
    {
        msg_Dbg(p_dec, "error during encoding");
        return 0;
    }

    /* Send the input buffer to the component */
    OMX_FIFO_GET(&p_sys->in.fifo, p_header);
    if(p_header)
    {
        /* In direct mode we pass the input pointer as is.
         * Otherwise we memcopy the data */
        if(p_sys->in.b_direct)
        {
            p_header->pOutputPortPrivate = p_header->pBuffer;
            p_header->pBuffer = p_pic->p[0].p_pixels;
        }
        else
        {
            CopyVlcPicture(p_dec, p_header, p_pic);
        }

        p_header->nFilledLen = p_sys->in.i_frame_size;
        p_header->nOffset = 0;
        p_header->nFlags = OMX_BUFFERFLAG_ENDOFFRAME;
        p_header->nTimeStamp = p_pic->date;
#ifdef OMXIL_EXTRA_DEBUG
        msg_Dbg( p_dec, "EmptyThisBuffer %p, %p, %i", p_header, p_header->pBuffer,
                 (int)p_header->nFilledLen );
#endif
        OMX_EmptyThisBuffer(p_sys->omx_handle, p_header);
        p_sys->in.b_flushed = false;
    }

    /* Handle the PortSettingsChanged events */
    for(i = 0; i < p_sys->ports; i++)
    {
        OmxPort *p_port = &p_sys->p_ports[i];
        if(!p_port->b_reconfigure) continue;
        p_port->b_reconfigure = 0;
        omx_error = PortReconfigure(p_dec, p_port);
    }

    /* Wait for the decoded frame */
    while(!p_block)
    {
        OMX_FIFO_GET(&p_sys->out.fifo, p_header);

        if(p_header->nFilledLen)
        {
            if(p_header->nFlags & OMX_BUFFERFLAG_CODECCONFIG)
            {
                /* TODO: need to store codec config */
                msg_Dbg(p_dec, "received codec config %i", (int)p_header->nFilledLen);
            }

            p_block = p_header->pAppPrivate;
            if(!p_block)
            {
                /* We're not in direct rendering mode.
                 * Get a new block and copy the content */
                p_block = block_New( p_dec, p_header->nFilledLen );
                memcpy(p_block->p_buffer, p_header->pBuffer, p_header->nFilledLen );
            }

            p_block->i_buffer = p_header->nFilledLen;
            p_block->i_pts = p_block->i_dts = p_header->nTimeStamp;
            p_header->nFilledLen = 0;
            p_header->pAppPrivate = 0;
        }

#ifdef OMXIL_EXTRA_DEBUG
        msg_Dbg( p_dec, "FillThisBuffer %p, %p", p_header, p_header->pBuffer );
#endif
        OMX_FillThisBuffer(p_sys->omx_handle, p_header);
    }

    msg_Dbg(p_dec, "done");
    return p_block;
}

/*****************************************************************************
 * CloseGeneric: omxil decoder destruction
 *****************************************************************************/
static void CloseGeneric( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t *)p_this;
    decoder_sys_t *p_sys = p_dec->p_sys;

    if(p_sys->omx_handle) DeinitialiseComponent(p_dec, p_sys->omx_handle);
    if(p_sys->b_init) p_sys->pf_deinit();
    dll_close( p_sys->dll_handle );

    vlc_mutex_destroy (&p_sys->mutex);
    vlc_cond_destroy (&p_sys->cond);
    vlc_mutex_destroy (&p_sys->in.fifo.lock);
    vlc_cond_destroy (&p_sys->in.fifo.wait);
    vlc_mutex_destroy (&p_sys->out.fifo.lock);
    vlc_cond_destroy (&p_sys->out.fifo.wait);

    free( p_sys );
}

/*****************************************************************************
 * OmxEventHandler: 
 *****************************************************************************/
static OMX_ERRORTYPE OmxEventHandler( OMX_HANDLETYPE omx_handle,
    OMX_PTR app_data, OMX_EVENTTYPE event, OMX_U32 data_1,
    OMX_U32 data_2, OMX_PTR event_data )
{
    decoder_t *p_dec = (decoder_t *)app_data;
    decoder_sys_t *p_sys = p_dec->p_sys;
    unsigned int i;
    (void)omx_handle;

    switch (event)
    {
    case OMX_EventCmdComplete:
        switch ((OMX_STATETYPE)data_1)
        {
        case OMX_CommandStateSet:
            msg_Dbg( p_dec, "OmxEventHandler (%s, %s, %s)", EventToString(event),
                     CommandToString(data_1), StateToString(data_2) );
            break;

        default:
            msg_Dbg( p_dec, "OmxEventHandler (%s, %s, %u)", EventToString(event),
                     CommandToString(data_1), (unsigned int)data_2 );
            break;
        }
        break;

    case OMX_EventError:
        msg_Dbg( p_dec, "OmxEventHandler (%s, %s, %u, %s)", EventToString(event),
                 ErrorToString((OMX_ERRORTYPE)data_1), (unsigned int)data_2,
                 (const char *)event_data);
        //p_sys->b_error = true;
        break;

    case OMX_EventPortSettingsChanged:
        msg_Dbg( p_dec, "OmxEventHandler (%s, %u, %u)", EventToString(event),
                 (unsigned int)data_1, (unsigned int)data_2 );
        for(i = 0; i < p_sys->ports; i++)
            if(p_sys->p_ports[i].definition.eDir == OMX_DirOutput)
                p_sys->p_ports[i].b_reconfigure = true;
        break;

    default:
        msg_Dbg( p_dec, "OmxEventHandler (%s, %u, %u)", EventToString(event),
                 (unsigned int)data_1, (unsigned int)data_2 );
        break;
    }

    PostOmxEvent(p_dec, event, data_1, data_2, event_data);
    return OMX_ErrorNone;
}

static OMX_ERRORTYPE OmxEmptyBufferDone( OMX_HANDLETYPE omx_handle,
    OMX_PTR app_data, OMX_BUFFERHEADERTYPE *omx_header )
{
    decoder_t *p_dec = (decoder_t *)app_data;
    decoder_sys_t *p_sys = p_dec->p_sys;
    (void)omx_handle;

#ifdef OMXIL_EXTRA_DEBUG
    msg_Dbg( p_dec, "OmxEmptyBufferDone %p, %p", omx_header, omx_header->pBuffer );
#endif

    if(omx_header->pAppPrivate || omx_header->pOutputPortPrivate)
    {
        block_t *p_block = (block_t *)omx_header->pAppPrivate;
        omx_header->pBuffer = omx_header->pOutputPortPrivate;
        if(p_block) block_Release(p_block);
        omx_header->pAppPrivate = 0;
    }
    OMX_FIFO_PUT(&p_sys->in.fifo, omx_header);

    return OMX_ErrorNone;
}

static OMX_ERRORTYPE OmxFillBufferDone( OMX_HANDLETYPE omx_handle,
    OMX_PTR app_data, OMX_BUFFERHEADERTYPE *omx_header )
{
    decoder_t *p_dec = (decoder_t *)app_data;
    decoder_sys_t *p_sys = p_dec->p_sys;
    (void)omx_handle;

#ifdef OMXIL_EXTRA_DEBUG
    msg_Dbg( p_dec, "OmxFillBufferDone %p, %p, %i", omx_header, omx_header->pBuffer,
             (int)omx_header->nFilledLen );
#endif

    if(omx_header->pInputPortPrivate)
    {
        omx_header->pBuffer = omx_header->pInputPortPrivate;
    }
    OMX_FIFO_PUT(&p_sys->out.fifo, omx_header);

    return OMX_ErrorNone;
}
