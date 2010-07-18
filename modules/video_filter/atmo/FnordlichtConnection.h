/*
 * FnordlichtConnection.h: class to access a FnordlichtLight Hardware
 * - the description could be found
 * here: http://github.com/fd0/fnordlicht/raw/master/doc/PROTOCOL
 *
 * (C) Kai Lauterbach (klaute at gmail.com)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * $Id: 08f9cf14313aa5c766d07d860601508b8128eadc $
 */
#ifndef _FnordlichtConnection_h_
#define _FnordlichtConnection_h_

#include "AtmoDefs.h"
#include "AtmoConnection.h"
#include "AtmoConfig.h"

#if defined(WIN32)
#   include <windows.h>
#endif


class CFnordlichtConnection : public CAtmoConnection
{
    private:
        HANDLE m_hComport;

        ATMO_BOOL sync(void);
        ATMO_BOOL stop(unsigned char addr);
        ATMO_BOOL reset(unsigned char addr);
        ATMO_BOOL start_bootloader(unsigned char addr);
        ATMO_BOOL boot_enter_application(unsigned char addr);

#if defined(WIN32)
        DWORD  m_dwLastWin32Error;
    public:
        DWORD getLastError() { return m_dwLastWin32Error; }
#endif

    public:
        CFnordlichtConnection (CAtmoConfig *cfg);
        virtual ~CFnordlichtConnection (void);

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

        virtual int getAmountFnordlichter();

        virtual const char *getDevicePath() { return "fnordlicht"; }

#if !defined(_ATMO_VLC_PLUGIN_)
        virtual char *getChannelName(int ch);
        virtual ATMO_BOOL ShowConfigDialog(HINSTANCE hInst, HWND parent,
                                            CAtmoConfig *cfg);
#endif

        virtual ATMO_BOOL CreateDefaultMapping(CAtmoChannelAssignment *ca);
};

#endif
