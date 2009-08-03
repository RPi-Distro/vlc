/*****************************************************************************
 * registry.cpp: Windows Registry Manipulation
 ****************************************************************************
 * Copyright (C) 2008 the VideoLAN team
 * $Id: 263a3bf556eac98eb5e9a51c0c1dd556b72224b2 $
 *
 * Authors: Andre Weber <WeberAndre # gmx - de>
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef WIN32

#include "registry.hpp"

QVLCRegistry::QVLCRegistry( HKEY rootKey )
{
    m_RootKey = rootKey;
}

QVLCRegistry::~QVLCRegistry( void )
{
}

bool QVLCRegistry::RegistryKeyExists( const char *path )
{
    HKEY keyHandle;
    if(  RegOpenKeyEx( m_RootKey, path, 0, KEY_READ, &keyHandle ) == ERROR_SUCCESS )
    {
        RegCloseKey( keyHandle );
        return true;
    }
    return false;
}

bool QVLCRegistry::RegistryValueExists( const char *path, const char *valueName )
{
    HKEY keyHandle;
    bool temp = false;
    DWORD size1;
    DWORD valueType;

    if(  RegOpenKeyEx( m_RootKey, path, 0, KEY_READ, &keyHandle ) == ERROR_SUCCESS )
    {
        if( RegQueryValueEx( keyHandle, valueName, NULL,
                             &valueType, NULL, &size1 ) == ERROR_SUCCESS )
        {
           temp = true;
        }
        RegCloseKey( keyHandle );
    }
    return temp;
}

void QVLCRegistry::WriteRegistryInt( const char *path, const char *valueName, int value )
{
    HKEY keyHandle;

    if(  RegCreateKeyEx( m_RootKey, path, 0, NULL, REG_OPTION_NON_VOLATILE,
                         KEY_WRITE, NULL, &keyHandle, NULL )  == ERROR_SUCCESS )
    {
        RegSetValueEx( keyHandle, valueName, 0, REG_DWORD,
                (LPBYTE)&value, sizeof( int ) );
        RegCloseKey( keyHandle );
    }
}

void QVLCRegistry::WriteRegistryString( const char *path, const char *valueName, const char *value )
{
    HKEY keyHandle;

    if(  RegCreateKeyEx( m_RootKey, path, 0, NULL, REG_OPTION_NON_VOLATILE,
                         KEY_WRITE, NULL, &keyHandle, NULL )  == ERROR_SUCCESS )
    {
        RegSetValueEx( keyHandle, valueName, 0, REG_SZ, (LPBYTE)value,
                (DWORD)( strlen( value ) + 1 ) );
        RegCloseKey( keyHandle );
    }
}

void QVLCRegistry::WriteRegistryDouble( const char *path, const char *valueName, double value )
{
    HKEY keyHandle;
    if( RegCreateKeyEx( m_RootKey, path, 0, NULL, REG_OPTION_NON_VOLATILE,
                       KEY_WRITE, NULL, &keyHandle, NULL ) == ERROR_SUCCESS )
    {
        RegSetValueEx( keyHandle, valueName, 0, REG_BINARY, (LPBYTE)&value, sizeof( double ) );
        RegCloseKey( keyHandle );
    }
}

int QVLCRegistry::ReadRegistryInt( const char *path, const char *valueName, int default_value ) {
    HKEY keyHandle;
    int tempValue;
    DWORD size1;
    DWORD valueType;

    if(  RegOpenKeyEx( m_RootKey, path, 0, KEY_READ, &keyHandle ) == ERROR_SUCCESS )
    {
        if( RegQueryValueEx(  keyHandle, valueName, NULL, &valueType, NULL, &size1 ) == ERROR_SUCCESS )
        {
           if( valueType == REG_DWORD )
           {
               if( RegQueryValueEx(  keyHandle, valueName, NULL, &valueType, (LPBYTE)&tempValue, &size1 ) == ERROR_SUCCESS )
               {
                  default_value = tempValue;
               };
           }
        }
        RegCloseKey( keyHandle );
    }
    return default_value;
}

char * QVLCRegistry::ReadRegistryString( const char *path, const char *valueName, char *default_value )
{
    HKEY keyHandle;
    char *tempValue = NULL;
    DWORD size1;
    DWORD valueType;

    if( RegOpenKeyEx( m_RootKey, path, 0, KEY_READ, &keyHandle ) == ERROR_SUCCESS )
    {
        if( RegQueryValueEx(  keyHandle, valueName, NULL, &valueType, NULL, &size1 ) == ERROR_SUCCESS )
        {
           if( valueType == REG_SZ )
           {
               // free
               tempValue = ( char * )malloc( size1+1 ); // +1 für NullByte`?
               if( RegQueryValueEx(  keyHandle, valueName, NULL, &valueType, (LPBYTE)tempValue, &size1 ) == ERROR_SUCCESS )
               {
                  default_value = tempValue;
               };
           }
        }
        RegCloseKey( keyHandle );
    }
    if( tempValue == NULL )
    {
        // wenn tempValue nicht aus registry gelesen wurde dafür sorgen das ein neuer String mit der Kopie von DefaultValue
        // geliefert wird - das macht das Handling des Rückgabewertes der Funktion einfacher - immer schön mit free freigeben!
        default_value = strdup( default_value );
    }

    return default_value;
}

double QVLCRegistry::ReadRegistryDouble( const char *path, const char *valueName, double default_value )
{
    HKEY keyHandle;
    double tempValue;
    DWORD size1;
    DWORD valueType;

    if( RegOpenKeyEx( m_RootKey, path, 0, KEY_READ, &keyHandle ) == ERROR_SUCCESS )
    {
        if( RegQueryValueEx( keyHandle, valueName, NULL, &valueType,
                             NULL, &size1 ) == ERROR_SUCCESS )
        {
           if( ( valueType == REG_BINARY ) && ( size1 == sizeof( double ) ) )
           {
               if( RegQueryValueEx(  keyHandle, valueName, NULL, &valueType,
                           (LPBYTE)&tempValue, &size1 ) == ERROR_SUCCESS )
               {
                  default_value = tempValue;
               };
           }
        }
        RegCloseKey( keyHandle );
    }
    return default_value;
}

int QVLCRegistry::DeleteValue( const char *path, const char *valueName )
{
    HKEY keyHandle;
    long result;
    if( (result = RegOpenKeyEx(m_RootKey, path, 0, KEY_WRITE, &keyHandle)) == ERROR_SUCCESS)
    {
        result = RegDeleteValue(keyHandle, valueName);
        RegCloseKey(keyHandle);
    }
    //ERROR_SUCCESS = ok everything else you have a problem*g*,
    return result;
}

long QVLCRegistry::DeleteKey( const char *path, const char *keyName )
{
    HKEY keyHandle;
    long result;
    if( (result = RegOpenKeyEx(m_RootKey, path, 0, KEY_WRITE, &keyHandle)) == ERROR_SUCCESS)
    {
         // be warned the key "keyName" will not be deleted if there are subkeys below him, values
        // I think are ok and will be recusively deleted, but not keys...
        // for this case we have to do a little bit more work!
        result = RegDeleteKey(keyHandle, keyName);
        RegCloseKey(keyHandle);
    }
    //ERROR_SUCCESS = ok everything else you have a problem*g*,
    return result;
}

#endif /* WIN32 */
