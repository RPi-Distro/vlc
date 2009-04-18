/*****************************************************************************
 * vlcproc.hpp
 *****************************************************************************
 * Copyright (C) 2003 the VideoLAN team
 * $Id: 8db9b07b33685ff15a7370293f108be76d3e026e $
 *
 * Authors: Cyril Deguet     <asmax@via.ecp.fr>
 *          Olivier Teulière <ipkiss@via.ecp.fr>
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

#ifndef VLCPROC_HPP
#define VLCPROC_HPP

#include <set>

#include "../vars/equalizer.hpp"
#include "../vars/playtree.hpp"
#include "../vars/time.hpp"
#include "../vars/volume.hpp"
#include "../utils/position.hpp"
#include "../utils/var_text.hpp"
#include "../commands/cmd_generic.hpp"

class OSTimer;
class VarBool;
struct aout_instance_t;
struct vout_window_t;


/// Singleton object handling VLC internal state and playlist
class VlcProc: public SkinObject
{
    public:
        /// Get the instance of VlcProc
        /// Returns NULL if the initialization of the object failed
        static VlcProc *instance( intf_thread_t *pIntf );

        /// Delete the instance of VlcProc
        static void destroy( intf_thread_t *pIntf );

        /// Getter for the playtree variable
        Playtree &getPlaytreeVar() { return *((Playtree*)m_cPlaytree.get()); }

        /// Getter for the time variable
        StreamTime &getTimeVar() { return *((StreamTime*)(m_cVarTime.get())); }

        /// Getter for the volume variable
        Volume &getVolumeVar() { return *((Volume*)(m_cVarVolume.get())); }

        /// Getter for the stream name variable
        VarText &getStreamNameVar()
           { return *((VarText*)(m_cVarStreamName.get())); }

        /// Getter for the stream URI variable
        VarText &getStreamURIVar()
            { return *((VarText*)(m_cVarStreamURI.get())); }

        /// Getter for the stream bitrate variable
        VarText &getStreamBitRateVar()
            { return *((VarText*)(m_cVarStreamBitRate.get())); }

        /// Getter for the stream sample rate variable
        VarText &getStreamSampleRateVar()
            { return *((VarText*)(m_cVarStreamSampleRate.get())); }

        /// Getter for the vout size variable
        VarBox &getVoutSizeVar() { return m_varVoutSize; }

        /// Set the vout window handle
        void registerVoutWindow( void *pVoutWindow );

        /// Unset the vout window handle
        void unregisterVoutWindow( void *pVoutWindow );

        /// Indicate whether the embedded video output is currently used
        bool isVoutUsed() const { return m_pVout != NULL; }

        /// If an embedded video output is used, drop it (i.e. tell it to stop
        /// using our window handle)
        void dropVout();

    protected:
        // Protected because it is a singleton
        VlcProc( intf_thread_t *pIntf );
        virtual ~VlcProc();

    private:
        /// Timer to call manage() regularly (via doManage())
        OSTimer *m_pTimer;
        /// Playtree variable
        VariablePtr m_cPlaytree;
        VariablePtr m_cVarRandom;
        VariablePtr m_cVarLoop;
        VariablePtr m_cVarRepeat;
        /// Variable for current position of the stream
        VariablePtr m_cVarTime;
        /// Variable for audio volume
        VariablePtr m_cVarVolume;
        /// Variable for current stream properties
        VariablePtr m_cVarStreamName;
        VariablePtr m_cVarStreamURI;
        VariablePtr m_cVarStreamBitRate;
        VariablePtr m_cVarStreamSampleRate;
        /// Variable for the "mute" state
        VariablePtr m_cVarMute;
        /// Variables related to the input
        VariablePtr m_cVarPlaying;
        VariablePtr m_cVarStopped;
        VariablePtr m_cVarPaused;
        VariablePtr m_cVarSeekable;
        /// Variables related to the vout
        VariablePtr m_cVarFullscreen;
        VarBox m_varVoutSize;
        VariablePtr m_cVarHasVout;
        /// Variables related to audio
        VariablePtr m_cVarHasAudio;
        /// Equalizer variables
        EqualizerBands m_varEqBands;
        VariablePtr m_cVarEqPreamp;
        VariablePtr m_cVarEqualizer;
        /// Variable for DVD detection
        VariablePtr m_cVarDvdActive;

        /// Set of handles of vout windows
        /**
         * When changing the skin, the handles of the 2 skins coexist in the
         * set (but this is temporary, until the old theme is destroyed).
         */
        set<void *> m_handleSet;
        /// Vout thread
        vout_thread_t *m_pVout;
        /// Audio output
        aout_instance_t *m_pAout;

        /**
         * Poll VLC internals to update the status (volume, current time in
         * the stream, current filename, play/pause/stop status, ...)
         * This function should be called regurlarly, since there is no
         * callback mechanism (yet?) to automatically update a variable when
         * the internal status changes
         */
        void manage();

        /// Define the command that calls manage()
        DEFINE_CALLBACK( VlcProc, Manage );

        /// Refresh audio variables
        void refreshAudio();
        /// Refresh playlist variables
        void refreshPlaylist();
        /// Refresh input variables
        void refreshInput();

        /// Update the stream name variable
        void updateStreamName( playlist_t *p_playlist );

        /// Callback for intf-change variable
        static int onIntfChange( vlc_object_t *pObj, const char *pVariable,
                                 vlc_value_t oldVal, vlc_value_t newVal,
                                 void *pParam );

        /// Callback for intf-show variable
        static int onIntfShow( vlc_object_t *pObj, const char *pVariable,
                               vlc_value_t oldVal, vlc_value_t newVal,
                               void *pParam );

        /// Callback for item-change variable
        static int onItemChange( vlc_object_t *pObj, const char *pVariable,
                                 vlc_value_t oldVal, vlc_value_t newVal,
                                 void *pParam );

        /// Callback for item-change variable
        static int onItemAppend( vlc_object_t *pObj, const char *pVariable,
                                 vlc_value_t oldVal, vlc_value_t newVal,
                                 void *pParam );

        /// Callback for item-change variable
        static int onItemDelete( vlc_object_t *pObj, const char *pVariable,
                                 vlc_value_t oldVal, vlc_value_t newVal,
                                 void *pParam );


        /// Callback for playlist-current variable
        static int onPlaylistChange( vlc_object_t *pObj, const char *pVariable,
                                     vlc_value_t oldVal, vlc_value_t newVal,
                                     void *pParam );

        /// Callback for skins2-to-load variable
        static int onSkinToLoad( vlc_object_t *pObj, const char *pVariable,
                                 vlc_value_t oldVal, vlc_value_t newVal,
                                 void *pParam );

        /// Callback for interaction variable
        static int onInteraction( vlc_object_t *pObj, const char *pVariable,
                                  vlc_value_t oldVal, vlc_value_t newVal,
                                  void *pParam );

    public: /* FIXME: these used to be private for a reason */
        /// Callback to request a vout window
        static void *getWindow( intf_thread_t *pIntf, vout_thread_t *pVout,
                                int *pXHint, int *pYHint,
                                unsigned int *pWidthHint,
                                unsigned int *pHeightHint );

        /// Callback to release a vout window
        static void releaseWindow( intf_thread_t *pIntf, void *pWindow );

        /// Callback to change a vout window
        static int controlWindow( struct vout_window_t *pWnd,
                                  int query, va_list args );
    private: /* end of FIXME */

        /// Callback for equalizer-bands variable
        static int onEqBandsChange( vlc_object_t *pObj, const char *pVariable,
                                    vlc_value_t oldVal, vlc_value_t newVal,
                                    void *pParam );

        /// Callback for equalizer-preamp variable
        static int onEqPreampChange( vlc_object_t *pObj, const char *pVariable,
                                     vlc_value_t oldVal, vlc_value_t newVal,
                                     void *pParam );
};


#endif
