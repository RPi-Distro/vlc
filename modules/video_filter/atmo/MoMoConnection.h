/*
 * MoMoConnection.h: class to access a MoMoLight Hardware - the description could be found
 * here: http://lx.divxstation.com/article.asp?aId=151
 *
 * See the README.txt file for copyright information and how to reach the author(s).
 *
 * $Id: 2a0315b4da41e0239e085289a68171b8b26f9084 $
 */
#ifndef _MoMoConnection_h_
#define _MoMoConnection_h_

#include "AtmoDefs.h"
#include "AtmoConnection.h"
#include "AtmoConfig.h"

#if defined(WIN32)
#   include <windows.h>
#endif


class CMoMoConnection : public CAtmoConnection {
    private:
        HANDLE m_hComport;

#if defined(WIN32)
        DWORD  m_dwLastWin32Error;
    public:
        DWORD getLastError() { return m_dwLastWin32Error; }
#endif

    public:
       CMoMoConnection (CAtmoConfig *cfg);
       virtual ~CMoMoConnection (void);

       virtual ATMO_BOOL OpenConnection();

       virtual void CloseConnection();

       virtual ATMO_BOOL isOpen(void);

       virtual ATMO_BOOL SendData(pColorPacket data);

       virtual ATMO_BOOL HardwareWhiteAdjust(int global_gamma,
                                             int global_contrast,
                                             int contrast_red,
                                             int contrast_green,
                                             int contrast_blue,
                                             int gamma_red,
                                             int gamma_green,
                                             int gamma_blue,
                                             ATMO_BOOL storeToEeprom);

       virtual int getNumChannels();


       virtual const char *getDevicePath() { return "momo"; }

#if !defined(_ATMO_VLC_PLUGIN_)
       virtual char *getChannelName(int ch);
       virtual ATMO_BOOL ShowConfigDialog(HINSTANCE hInst, HWND parent, CAtmoConfig *cfg);
#endif

       virtual ATMO_BOOL CreateDefaultMapping(CAtmoChannelAssignment *ca);
};

#endif
