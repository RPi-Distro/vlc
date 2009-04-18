/*****************************************************************************
 * theme_repository.cpp
 *****************************************************************************
 * Copyright (C) 2004 the VideoLAN team
 * $Id: 3db8a424bd7e53b418870cca251405e09aba8b10 $
 *
 * Authors: Cyril Deguet     <asmax@via.ecp.fr>
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

#include "theme_repository.hpp"
#include "os_factory.hpp"
#include "../commands/async_queue.hpp"
#include "../commands/cmd_dialogs.hpp"
#ifdef HAVE_UNISTD_H
#   include <unistd.h>
#elif defined( WIN32 ) && !defined( UNDER_CE )
#   include <direct.h>
#endif
#ifdef HAVE_DIRENT_H
#   include <dirent.h>
#endif


const char *ThemeRepository::kOpenDialog = "{openSkin}";


ThemeRepository *ThemeRepository::instance( intf_thread_t *pIntf )
{
    if( pIntf->p_sys->p_repository == NULL )
    {
        pIntf->p_sys->p_repository = new ThemeRepository( pIntf );
    }

    return pIntf->p_sys->p_repository;
}


void ThemeRepository::destroy( intf_thread_t *pIntf )
{
    if( pIntf->p_sys->p_repository )
    {
        delete pIntf->p_sys->p_repository;
        pIntf->p_sys->p_repository = NULL;
    }
}


ThemeRepository::ThemeRepository( intf_thread_t *pIntf ): SkinObject( pIntf )
{
    vlc_value_t val, text;

    // Create a variable to add items in wxwindows popup menu
    var_Create( pIntf, "intf-skins", VLC_VAR_STRING |
                VLC_VAR_HASCHOICE | VLC_VAR_ISCOMMAND );
    text.psz_string = _("Select skin");
    var_Change( pIntf, "intf-skins", VLC_VAR_SETTEXT, &text, NULL );

    // Scan vlt files in the resource path
    OSFactory *pOsFactory = OSFactory::instance( pIntf );
    list<string> resPath = pOsFactory->getResourcePath();
    list<string>::const_iterator it;
    for( it = resPath.begin(); it != resPath.end(); it++ )
    {
        parseDirectory( *it );
    }

    // Add an entry for the "open skin" dialog
    val.psz_string = (char*)kOpenDialog;
    text.psz_string = _("Open skin...");
    var_Change( getIntf(), "intf-skins", VLC_VAR_ADDCHOICE, &val,
                &text );

    // Set the callback
    var_AddCallback( pIntf, "intf-skins", changeSkin, this );
}


ThemeRepository::~ThemeRepository()
{
    var_Destroy( getIntf(), "intf-skins" );
}


void ThemeRepository::parseDirectory( const string &rDir_locale )
{
    DIR *pDir;
    char *pszDirContent;
    vlc_value_t val, text;
    // Path separator
    const string &sep = OSFactory::instance( getIntf() )->getDirSeparator();

    // Open the dir
    // FIXME: parseDirectory should be invoked with UTF-8 input instead!!
    string rDir = sFromLocale( rDir_locale );
    pDir = utf8_opendir( rDir.c_str() );

    if( pDir == NULL )
    {
        // An error occurred
        msg_Dbg( getIntf(), "cannot open directory %s", rDir.c_str() );
        return;
    }

    // While we still have entries in the directory
    while( ( pszDirContent = utf8_readdir( pDir ) ) != NULL )
    {
        string name = pszDirContent;
        string extension;
        if( name.size() > 4 )
        {
            extension = name.substr( name.size() - 4, 4 );
        }
        if( extension == ".vlt" || extension == ".wsz" )
        {
            string path = rDir + sep + name;
            msg_Dbg( getIntf(), "found skin %s", path.c_str() );

            // Add the theme in the popup menu
            string shortname = name.substr( 0, name.size() - 4 );
            val.psz_string = new char[path.size() + 1];
            text.psz_string = new char[shortname.size() + 1];
            strcpy( val.psz_string, path.c_str() );
            strcpy( text.psz_string, shortname.c_str() );
            var_Change( getIntf(), "intf-skins", VLC_VAR_ADDCHOICE, &val,
                        &text );
            delete[] val.psz_string;
            delete[] text.psz_string;
        }

        free( pszDirContent );
    }

    closedir( pDir );
}



int ThemeRepository::changeSkin( vlc_object_t *pIntf, char const *pCmd,
                                 vlc_value_t oldval, vlc_value_t newval,
                                 void *pData )
{
    ThemeRepository *pThis = (ThemeRepository*)(pData);

    // Special menu entry for the open skin dialog
    if( !strcmp( newval.psz_string, kOpenDialog ) )
    {
        CmdDlgChangeSkin cmd( pThis->getIntf() );
        cmd.execute();
    }
    else
    {
        // Try to load the new skin
        CmdChangeSkin *pCmd = new CmdChangeSkin( pThis->getIntf(),
                                                 newval.psz_string );
        AsyncQueue *pQueue = AsyncQueue::instance( pThis->getIntf() );
        pQueue->push( CmdGenericPtr( pCmd ) );
    }

    return VLC_SUCCESS;
}

