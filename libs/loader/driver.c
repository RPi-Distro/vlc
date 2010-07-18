/*
 * $Id: dde6a1d6ee387a4519d18f03c2e6fa1b73a54ad0 $
 *
 * Copyright 1993, 1994 Martin Ayotte
 * Copyright 1998 Marcus Meissner
 * Copyright 1999 Eric Pouech
 *
 * Originally distributed under LPGL 2.1 (or later) by the Wine project.
 *
 * Modified for use with MPlayer, detailed CVS changelog at
 * http://www.mplayerhq.hu/cgi-bin/cvsweb.cgi/main/
 *
 * File now distributed as part of VLC media player with no modifications.
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
 */

#include "config.h"

#include <stdio.h>
#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif
#include <stdlib.h>
#ifdef __FreeBSD__
#include <sys/time.h>
#endif

#include "win32.h"
#include "wine/driver.h"
#include "wine/pe_image.h"
#include "wine/winreg.h"
#include "wine/vfw.h"
#include "registry.h"
#ifdef WIN32_LOADER
#include "ldt_keeper.h"
#endif
#include "driver.h"
#ifndef __MINGW32__
#include "ext.h"
#endif

#ifndef WIN32_LOADER
char* def_path=WIN32_PATH;
#else
extern char* def_path;
#endif

#if 1

/*
 * STORE_ALL/REST_ALL seems like an attempt to workaround problems due to
 * WINAPI/no-WINAPI bustage.
 *
 * There should be no need for the STORE_ALL/REST_ALL hack once all
 * function definitions agree with their prototypes (WINAPI-wise) and
 * we make sure, that we do not call these functions without a proper
 * prototype in scope.
 */

#define STORE_ALL
#define REST_ALL
#else
// this asm code is no longer needed
#define STORE_ALL \
    __asm__ __volatile__ ( \
    "push %%ebx\n\t" \
    "push %%ecx\n\t" \
    "push %%edx\n\t" \
    "push %%esi\n\t" \
    "push %%edi\n\t"::)

#define REST_ALL \
    __asm__ __volatile__ ( \
    "pop %%edi\n\t" \
    "pop %%esi\n\t" \
    "pop %%edx\n\t" \
    "pop %%ecx\n\t" \
    "pop %%ebx\n\t"::)
#endif

static int needs_free=0;
void SetCodecPath(const char* path)
{
    if(needs_free)free(def_path);
    if(path==NULL)
    {
	def_path=WIN32_PATH;
	needs_free=0;
	return;
    }
    def_path = (char*) malloc(strlen(path)+1);
    strcpy(def_path, path);
    needs_free=1;
}

static DWORD dwDrvID = 0;

LRESULT WINAPI SendDriverMessage(HDRVR hDriver, UINT message,
				 LPARAM lParam1, LPARAM lParam2)
{
    DRVR* module=(DRVR*)hDriver;
    int result;
#ifndef __svr4__
    char qw[300];
#endif
#ifdef DETAILED_OUT
    printf("SendDriverMessage: driver %X, message %X, arg1 %X, arg2 %X\n", hDriver, message, lParam1, lParam2);
#endif
    if (!module || !module->hDriverModule || !module->DriverProc) return -1;
#ifndef __svr4__
    __asm__ __volatile__ ("fsave (%0)\n\t": :"r"(&qw));
#endif

#ifdef WIN32_LOADER
    Setup_FS_Segment();
#endif

    STORE_ALL;
    result=module->DriverProc(module->dwDriverID, hDriver, message, lParam1, lParam2);
    REST_ALL;

#ifndef __svr4__
    __asm__ __volatile__ ("frstor (%0)\n\t": :"r"(&qw));
#endif

#ifdef DETAILED_OUT
    printf("\t\tResult: %X\n", result);
#endif
    return result;
}

void DrvClose(HDRVR hDriver)
{
    if (hDriver)
    {
	DRVR* d = (DRVR*)hDriver;
	if (d->hDriverModule)
	{
#ifdef WIN32_LOADER
	    Setup_FS_Segment();
#endif
	    if (d->DriverProc)
	    {
		SendDriverMessage(hDriver, DRV_CLOSE, 0, 0);
		d->dwDriverID = 0;
		SendDriverMessage(hDriver, DRV_FREE, 0, 0);
	    }
	    FreeLibrary(d->hDriverModule);
	}
	free(d);
    }
#ifdef WIN32_LOADER
    CodecRelease();
#endif
}

//DrvOpen(LPCSTR lpszDriverName, LPCSTR lpszSectionName, LPARAM lParam2)
HDRVR DrvOpen(LPARAM lParam2)
{
    NPDRVR hDriver;
    int i;
    char unknown[0x124];
    const char* filename = (const char*) ((ICOPEN*) lParam2)->pV1Reserved;

#ifdef MPLAYER
#ifdef WIN32_LOADER
    Setup_LDT_Keeper();
#endif
    printf("Loading codec DLL: '%s'\n",filename);
#endif

    hDriver = (NPDRVR) malloc(sizeof(DRVR));
    if (!hDriver)
	return ((HDRVR) 0);
    memset((void*)hDriver, 0, sizeof(DRVR));

#ifdef WIN32_LOADER
    CodecAlloc();
    Setup_FS_Segment();
#endif

    hDriver->hDriverModule = LoadLibraryA(filename);
    if (!hDriver->hDriverModule)
    {
	printf("Can't open library %s\n", filename);
	DrvClose((HDRVR)hDriver);
	return ((HDRVR) 0);
    }

    hDriver->DriverProc = (DRIVERPROC) GetProcAddress(hDriver->hDriverModule,
						      "DriverProc");
    if (!hDriver->DriverProc)
    {
	printf("Library %s is not a valid VfW/ACM codec\n", filename);
	DrvClose((HDRVR)hDriver);
	return ((HDRVR) 0);
    }

    TRACE("DriverProc == %X\n", hDriver->DriverProc);
    SendDriverMessage((HDRVR)hDriver, DRV_LOAD, 0, 0);
    TRACE("DRV_LOAD Ok!\n");
    SendDriverMessage((HDRVR)hDriver, DRV_ENABLE, 0, 0);
    TRACE("DRV_ENABLE Ok!\n");
    hDriver->dwDriverID = ++dwDrvID; // generate new id

    // open driver and remmeber proper DriverID
    hDriver->dwDriverID = SendDriverMessage((HDRVR)hDriver, DRV_OPEN, (LPARAM) unknown, lParam2);
    TRACE("DRV_OPEN Ok!(%X)\n", hDriver->dwDriverID);

    printf("Loaded DLL driver %s at %x\n", filename, hDriver->hDriverModule);
    return (HDRVR)hDriver;
}
