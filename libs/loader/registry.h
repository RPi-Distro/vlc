#ifndef AVIFILE_REGISTRY_H
#define AVIFILE_REGISTRY_H

/********************************************************
 *
 *       Declaration of registry access functions
 *       Copyright 2000 Eugene Kuznetsov  (divx@euro.ru)
 *
 ********************************************************/

/*
 * Modified for use with MPlayer, detailed CVS changelog at
 * http://www.mplayerhq.hu/cgi-bin/cvsweb.cgi/main/
 * $Id$
 */

#ifdef __cplusplus
extern "C" {
#endif

void free_registry(void);

long __stdcall RegOpenKeyExA(long key, const char* subkey, long reserved,
		   long access, int* newkey);
long __stdcall RegCloseKey(long key);
long __stdcall RegQueryValueExA(long key, const char* value, int* reserved,
		      int* type, int* data, int* count);
long __stdcall RegCreateKeyExA(long key, const char* name, long reserved,
		     void* classs, long options, long security,
		     void* sec_attr, int* newkey, int* status);
long __stdcall RegSetValueExA(long key, const char* name, long v1, long v2,
		    const void* data, long size);

#ifdef __WINE_WINERROR_H

long __stdcall RegEnumKeyExA(HKEY hKey, DWORD dwIndex, LPSTR lpName, LPDWORD lpcbName,
		   LPDWORD lpReserved, LPSTR lpClass, LPDWORD lpcbClass,
		   LPFILETIME lpftLastWriteTime);
long __stdcall RegEnumValueA(HKEY hkey, DWORD index, LPSTR value, LPDWORD val_count,
		   LPDWORD reserved, LPDWORD type, LPBYTE data, LPDWORD count);
#endif
#ifdef __cplusplus
};
#endif

#endif // AVIFILE_REGISTRY_H
