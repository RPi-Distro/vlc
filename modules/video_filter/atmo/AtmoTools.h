/*
 * AtmoTools.h: Collection of tool and helperfunction
 *
 * See the README.txt file for copyright information and how to reach the author(s).
 *
 * $Id: d80ea0760bdf70ac2dfbf41aa72803e411b7e842 $
 */
#ifndef _AtmoTools_h_
#define _AtmoTools_h_

#include "AtmoDefs.h"

#include "AtmoConfig.h"
#include "AtmoConnection.h"
#include "AtmoDynData.h"

/*
  implements some tool functions - for use in different classes - and cases!

  to avoid copy and paste code ...
*/
class CAtmoTools
{
private:
    CAtmoTools(void);
    ~CAtmoTools(void);
public:
    static EffectMode SwitchEffect(CAtmoDynData *pDynData, EffectMode newEffectMode);
    static void ShowShutdownColor(CAtmoDynData *pDynData);
    static ATMO_BOOL RecreateConnection(CAtmoDynData *pDynData);

    static tColorPacket WhiteCalibration(CAtmoConfig *pAtmoConfig, tColorPacket ColorPacket);
    static tColorPacket ApplyGamma(CAtmoConfig *pAtmoConfig, tColorPacket ColorPacket);

    static int SetChannelAssignment(CAtmoDynData *pDynData, int index);

#if !defined(_ATMO_VLC_PLUGIN_)
    static void SaveBitmap(HDC hdc,HBITMAP hBmp,char *fileName);
#endif
};

#endif
