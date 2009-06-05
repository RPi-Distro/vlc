/*****************************************************************************
 * cache.c: Plugins cache
 *****************************************************************************
 * Copyright (C) 2001-2007 the VideoLAN team
 * $Id: 89d1a99a6d51f0a798c25fc33b2133097c058f5e $
 *
 * Authors: Sam Hocevar <sam@zoy.org>
 *          Ethan C. Baldridge <BaldridgeE@cadmus.com>
 *          Hans-Peter Jansen <hpj@urpla.net>
 *          Gildas Bazin <gbazin@videolan.org>
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

#include <vlc_common.h>
#include "libvlc.h"

#include <stdlib.h>                                      /* free(), strtol() */
#include <stdio.h>                                              /* sprintf() */
#include <string.h>                                              /* strdup() */
#include <vlc_plugin.h>

#ifdef HAVE_SYS_TYPES_H
#   include <sys/types.h>
#endif
#ifdef HAVE_UNISTD_H
#   include <unistd.h>
#endif

#if !defined(HAVE_DYNAMIC_PLUGINS)
    /* no support for plugins */
#elif defined(HAVE_DL_DYLD)
#   if defined(HAVE_MACH_O_DYLD_H)
#       include <mach-o/dyld.h>
#   endif
#elif defined(HAVE_DL_BEOS)
#   if defined(HAVE_IMAGE_H)
#       include <image.h>
#   endif
#elif defined(HAVE_DL_WINDOWS)
#   include <windows.h>
#elif defined(HAVE_DL_DLOPEN)
#   if defined(HAVE_DLFCN_H) /* Linux, BSD, Hurd */
#       include <dlfcn.h>
#   endif
#   if defined(HAVE_SYS_DL_H)
#       include <sys/dl.h>
#   endif
#elif defined(HAVE_DL_SHL_LOAD)
#   if defined(HAVE_DL_H)
#       include <dl.h>
#   endif
#endif

#include "config/configuration.h"

#include "vlc_charset.h"

#include "modules/modules.h"


/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
#ifdef HAVE_DYNAMIC_PLUGINS
static int    CacheLoadConfig  ( module_t *, FILE * );
static int    CacheSaveConfig  ( module_t *, FILE * );

/* Sub-version number
 * (only used to avoid breakage in dev version when cache structure changes) */
#define CACHE_SUBVERSION_NUM 4

/* Format string for the cache filename */
#define CACHENAME_FORMAT \
    "plugins-%.2zx%.2zx%.2"PRIx8".dat"
/* Magic for the cache filename */
#define CACHENAME_VALUES \
    sizeof(int), sizeof(void *), *(uint8_t *)&(uint16_t){ 0xbe1e }


/*****************************************************************************
 * LoadPluginsCache: loads the plugins cache file
 *****************************************************************************
 * This function will load the plugin cache if present and valid. This cache
 * will in turn be queried by AllocateAllPlugins() to see if it needs to
 * actually load the dynamically loadable module.
 * This allows us to only fully load plugins when they are actually used.
 *****************************************************************************/
void CacheLoad( vlc_object_t *p_this, module_bank_t *p_bank, bool b_delete )
{
    char *psz_filename, *psz_cachedir = config_GetCacheDir();
    FILE *file;
    int i, j, i_size, i_read;
    char p_cachestring[sizeof("cache " COPYRIGHT_MESSAGE)];
    char p_cachelang[6], p_lang[6];
    int i_cache;
    module_cache_t **pp_cache = 0;
    int32_t i_file_size, i_marker;

    if( !psz_cachedir ) /* XXX: this should never happen */
    {
        msg_Err( p_this, "Unable to get cache directory" );
        return;
    }

    if( asprintf( &psz_filename, "%s"DIR_SEP CACHENAME_FORMAT,
                  psz_cachedir, CACHENAME_VALUES ) == -1 )
    {
        free( psz_cachedir );
        return;
    }
    free( psz_cachedir );

    if( b_delete )
    {
#if !defined( UNDER_CE )
        unlink( psz_filename );
#else
        wchar_t psz_wf[MAX_PATH];
        MultiByteToWideChar( CP_ACP, 0, psz_filename, -1, psz_wf, MAX_PATH );
        DeleteFile( psz_wf );
#endif
        msg_Dbg( p_this, "removing plugins cache file %s", psz_filename );
        free( psz_filename );
        return;
    }

    msg_Dbg( p_this, "loading plugins cache file %s", psz_filename );

    file = utf8_fopen( psz_filename, "rb" );
    if( !file )
    {
        msg_Warn( p_this, "could not open plugins cache file %s for reading",
                  psz_filename );
        free( psz_filename );
        return;
    }
    free( psz_filename );

    /* Check the file size */
    i_read = fread( &i_file_size, 1, sizeof(i_file_size), file );
    if( i_read != sizeof(i_file_size) )
    {
        msg_Warn( p_this, "This doesn't look like a valid plugins cache "
                  "(too short)" );
        fclose( file );
        return;
    }

    fseek( file, 0, SEEK_END );
    if( ftell( file ) != i_file_size )
    {
        msg_Warn( p_this, "This doesn't look like a valid plugins cache "
                  "(corrupted size)" );
        fclose( file );
        return;
    }
    fseek( file, sizeof(i_file_size), SEEK_SET );

    /* Check the file is a plugins cache */
    i_size = sizeof("cache " COPYRIGHT_MESSAGE) - 1;
    i_read = fread( p_cachestring, 1, i_size, file );
    if( i_read != i_size ||
        memcmp( p_cachestring, "cache " COPYRIGHT_MESSAGE, i_size ) )
    {
        msg_Warn( p_this, "This doesn't look like a valid plugins cache" );
        fclose( file );
        return;
    }

#ifdef DISTRO_VERSION
    /* Check for distribution specific version */
    char p_distrostring[sizeof( DISTRO_VERSION )];
    i_size = sizeof( DISTRO_VERSION ) - 1;
    i_read = fread( p_distrostring, 1, i_size, file );
    if( i_read != i_size ||
        memcmp( p_distrostring, DISTRO_VERSION, i_size ) )
    {
        msg_Warn( p_this, "This doesn't look like a valid plugins cache" );
        fclose( file );
        return;
    }
#endif

    /* Check Sub-version number */
    i_read = fread( &i_marker, 1, sizeof(i_marker), file );
    if( i_read != sizeof(i_marker) || i_marker != CACHE_SUBVERSION_NUM )
    {
        msg_Warn( p_this, "This doesn't look like a valid plugins cache "
                  "(corrupted header)" );
        fclose( file );
        return;
    }

    /* Check the language hasn't changed */
    sprintf( p_lang, "%5.5s", _("C") ); i_size = 5;
    i_read = fread( p_cachelang, 1, i_size, file );
    if( i_read != i_size || memcmp( p_cachelang, p_lang, i_size ) )
    {
        msg_Warn( p_this, "This doesn't look like a valid plugins cache "
                  "(language changed)" );
        fclose( file );
        return;
    }

    /* Check header marker */
    i_read = fread( &i_marker, 1, sizeof(i_marker), file );
    if( i_read != sizeof(i_marker) ||
        i_marker != ftell( file ) - (int)sizeof(i_marker) )
    {
        msg_Warn( p_this, "This doesn't look like a valid plugins cache "
                  "(corrupted header)" );
        fclose( file );
        return;
    }

    p_bank->i_loaded_cache = 0;
    if (fread( &i_cache, 1, sizeof(i_cache), file ) != sizeof(i_cache) )
    {
        msg_Warn( p_this, "This doesn't look like a valid plugins cache "
                  "(file too short)" );
        fclose( file );
        return;
    }

    if( i_cache )
        pp_cache = p_bank->pp_loaded_cache =
                   malloc( i_cache * sizeof(void *) );

#define LOAD_IMMEDIATE(a) \
    if( fread( (void *)&a, sizeof(char), sizeof(a), file ) != sizeof(a) ) goto error
#define LOAD_STRING(a) \
{ \
    a = NULL; \
    if( ( fread( &i_size, sizeof(i_size), 1, file ) != 1 ) \
     || ( i_size > 16384 ) ) \
        goto error; \
    if( i_size ) { \
        char *psz = malloc( i_size ); \
        if( fread( psz, i_size, 1, file ) != 1 ) { \
            free( psz ); \
            goto error; \
        } \
        if( psz[i_size-1] ) { \
            free( psz ); \
            goto error; \
        } \
        a = psz; \
    } \
}

    for( i = 0; i < i_cache; i++ )
    {
        uint16_t i_size;
        int i_submodules;

        pp_cache[i] = malloc( sizeof(module_cache_t) );
        p_bank->i_loaded_cache++;

        /* Load common info */
        LOAD_STRING( pp_cache[i]->psz_file );
        LOAD_IMMEDIATE( pp_cache[i]->i_time );
        LOAD_IMMEDIATE( pp_cache[i]->i_size );
        LOAD_IMMEDIATE( pp_cache[i]->b_junk );
        pp_cache[i]->b_used = false;

        if( pp_cache[i]->b_junk ) continue;

        pp_cache[i]->p_module = vlc_module_create( p_this );

        /* Load additional infos */
        free( pp_cache[i]->p_module->psz_object_name );
        LOAD_STRING( pp_cache[i]->p_module->psz_object_name );
        LOAD_STRING( pp_cache[i]->p_module->psz_shortname );
        LOAD_STRING( pp_cache[i]->p_module->psz_longname );
        LOAD_STRING( pp_cache[i]->p_module->psz_help );
        for( j = 0; j < MODULE_SHORTCUT_MAX; j++ )
        {
            LOAD_STRING( pp_cache[i]->p_module->pp_shortcuts[j] ); // FIX
        }
        LOAD_STRING( pp_cache[i]->p_module->psz_capability );
        LOAD_IMMEDIATE( pp_cache[i]->p_module->i_score );
        LOAD_IMMEDIATE( pp_cache[i]->p_module->i_cpu );
        LOAD_IMMEDIATE( pp_cache[i]->p_module->b_unloadable );
        LOAD_IMMEDIATE( pp_cache[i]->p_module->b_reentrant );
        LOAD_IMMEDIATE( pp_cache[i]->p_module->b_submodule );

        /* Config stuff */
        if( CacheLoadConfig( pp_cache[i]->p_module, file ) != VLC_SUCCESS )
            goto error;

        LOAD_STRING( pp_cache[i]->p_module->psz_filename );

        LOAD_IMMEDIATE( i_submodules );

        while( i_submodules-- )
        {
            module_t *p_module = vlc_submodule_create( pp_cache[i]->p_module );
            free( p_module->psz_object_name );
            LOAD_STRING( p_module->psz_object_name );
            LOAD_STRING( p_module->psz_shortname );
            LOAD_STRING( p_module->psz_longname );
            LOAD_STRING( p_module->psz_help );
            for( j = 0; j < MODULE_SHORTCUT_MAX; j++ )
            {
                LOAD_STRING( p_module->pp_shortcuts[j] ); // FIX
            }
            LOAD_STRING( p_module->psz_capability );
            LOAD_IMMEDIATE( p_module->i_score );
            LOAD_IMMEDIATE( p_module->i_cpu );
            LOAD_IMMEDIATE( p_module->b_unloadable );
            LOAD_IMMEDIATE( p_module->b_reentrant );
        }
    }

    fclose( file );
    return;

 error:

    msg_Warn( p_this, "plugins cache not loaded (corrupted)" );

    /* TODO: cleanup */
    p_bank->i_loaded_cache = 0;

    fclose( file );
    return;
}


static int CacheLoadConfig( module_t *p_module, FILE *file )
{
    uint32_t i_lines;
    uint16_t i_size;

    /* Calculate the structure length */
    LOAD_IMMEDIATE( p_module->i_config_items );
    LOAD_IMMEDIATE( p_module->i_bool_items );

    LOAD_IMMEDIATE( i_lines );

    /* Allocate memory */
    if (i_lines)
    {
        p_module->p_config =
            (module_config_t *)calloc( i_lines, sizeof(module_config_t) );
        if( p_module->p_config == NULL )
        {
            p_module->confsize = 0;
            return VLC_ENOMEM;
        }
    }
    p_module->confsize = i_lines;

    /* Do the duplication job */
    for (size_t i = 0; i < i_lines; i++ )
    {
        LOAD_IMMEDIATE( p_module->p_config[i] );

        LOAD_STRING( p_module->p_config[i].psz_type );
        LOAD_STRING( p_module->p_config[i].psz_name );
        LOAD_STRING( p_module->p_config[i].psz_text );
        LOAD_STRING( p_module->p_config[i].psz_longtext );
        LOAD_STRING( p_module->p_config[i].psz_oldname );
        LOAD_IMMEDIATE( p_module->p_config[i].b_removed );

        if (IsConfigStringType (p_module->p_config[i].i_type))
        {
            LOAD_STRING (p_module->p_config[i].orig.psz);
            p_module->p_config[i].value.psz =
                    (p_module->p_config[i].orig.psz != NULL)
                        ? strdup (p_module->p_config[i].orig.psz) : NULL;
            p_module->p_config[i].saved.psz = NULL;
        }
        else
        {
            memcpy (&p_module->p_config[i].value, &p_module->p_config[i].orig,
                    sizeof (p_module->p_config[i].value));
            memcpy (&p_module->p_config[i].saved, &p_module->p_config[i].orig,
                    sizeof (p_module->p_config[i].saved));
        }

        p_module->p_config[i].b_dirty = false;

        p_module->p_config[i].p_lock = &p_module->lock;

        if( p_module->p_config[i].i_list )
        {
            if( p_module->p_config[i].ppsz_list )
            {
                int j;
                p_module->p_config[i].ppsz_list =
                    malloc( (p_module->p_config[i].i_list+1) * sizeof(char *));
                if( p_module->p_config[i].ppsz_list )
                {
                    for( j = 0; j < p_module->p_config[i].i_list; j++ )
                        LOAD_STRING( p_module->p_config[i].ppsz_list[j] );
                    p_module->p_config[i].ppsz_list[j] = NULL;
                }
            }
            if( p_module->p_config[i].ppsz_list_text )
            {
                int j;
                p_module->p_config[i].ppsz_list_text =
                    malloc( (p_module->p_config[i].i_list+1) * sizeof(char *));
                if( p_module->p_config[i].ppsz_list_text )
                {
                  for( j = 0; j < p_module->p_config[i].i_list; j++ )
                      LOAD_STRING( p_module->p_config[i].ppsz_list_text[j] );
                  p_module->p_config[i].ppsz_list_text[j] = NULL;
                }
            }
            if( p_module->p_config[i].pi_list )
            {
                p_module->p_config[i].pi_list =
                    malloc( (p_module->p_config[i].i_list + 1) * sizeof(int) );
                if( p_module->p_config[i].pi_list )
                {
                    for (int j = 0; j < p_module->p_config[i].i_list; j++)
                        LOAD_IMMEDIATE( p_module->p_config[i].pi_list[j] );
                }
            }
        }

        if( p_module->p_config[i].i_action )
        {
            p_module->p_config[i].ppf_action =
                malloc( p_module->p_config[i].i_action * sizeof(void *) );
            p_module->p_config[i].ppsz_action_text =
                malloc( p_module->p_config[i].i_action * sizeof(char *) );

            for (int j = 0; j < p_module->p_config[i].i_action; j++)
            {
                p_module->p_config[i].ppf_action[j] = 0;
                LOAD_STRING( p_module->p_config[i].ppsz_action_text[j] );
            }
        }

        LOAD_IMMEDIATE( p_module->p_config[i].pf_callback );
    }

    return VLC_SUCCESS;

 error:

    return VLC_EGENERIC;
}

static int CacheSaveSubmodule( FILE *file, module_t *p_module );

/*****************************************************************************
 * SavePluginsCache: saves the plugins cache to a file
 *****************************************************************************/
void CacheSave( vlc_object_t *p_this, module_bank_t *p_bank )
{
    static char const psz_tag[] =
        "Signature: 8a477f597d28d172789f06886806bc55\r\n"
        "# This file is a cache directory tag created by VLC.\r\n"
        "# For information about cache directory tags, see:\r\n"
        "#   http://www.brynosaurus.com/cachedir/\r\n";

    char *psz_cachedir = config_GetCacheDir();
    FILE *file;
    int i, j, i_cache;
    module_cache_t **pp_cache;
    uint32_t i_file_size = 0;

    if( !psz_cachedir ) /* XXX: this should never happen */
    {
        msg_Err( p_this, "unable to get cache directory" );
        return;
    }

    char psz_filename[sizeof(DIR_SEP) + 32 + strlen(psz_cachedir)];
    config_CreateDir( p_this, psz_cachedir );

    snprintf( psz_filename, sizeof( psz_filename ),
              "%s"DIR_SEP"CACHEDIR.TAG", psz_cachedir );
    file = utf8_fopen( psz_filename, "wb" );
    if (file != NULL)
    {
        if (fwrite (psz_tag, 1, sizeof (psz_tag) - 1, file) != 1)
            clearerr (file); /* what else can we do? */
        fclose( file );
    }

    snprintf( psz_filename, sizeof( psz_filename ),
              "%s"DIR_SEP CACHENAME_FORMAT, psz_cachedir,
              CACHENAME_VALUES );
    free( psz_cachedir );
    msg_Dbg( p_this, "writing plugins cache %s", psz_filename );

    char psz_tmpname[sizeof (psz_filename) + 12];
    snprintf (psz_tmpname, sizeof (psz_tmpname), "%s.%"PRIu32, psz_filename,
#ifdef UNDER_CE
              (uint32_t)GetCurrentProcessId ()
#else
              (uint32_t)getpid ()
#endif
             );
    file = utf8_fopen( psz_tmpname, "wb" );
    if (file == NULL)
        goto error;

    /* Empty space for file size */
    if (fwrite (&i_file_size, sizeof (i_file_size), 1, file) != 1)
        goto error;

    /* Contains version number */
    if (fputs ("cache "COPYRIGHT_MESSAGE, file) == EOF)
        goto error;
#ifdef DISTRO_VERSION
    /* Allow binary maintaner to pass a string to detect new binary version*/
    if (fputs( DISTRO_VERSION, file ) == EOF)
        goto error;
#endif
    /* Sub-version number (to avoid breakage in the dev version when cache
     * structure changes) */
    i_file_size = CACHE_SUBVERSION_NUM;
    if (fwrite (&i_file_size, sizeof (i_file_size), 1, file) != 1 )
        goto error;

    /* Language */
    if (fprintf (file, "%5.5s", _("C")) == EOF)
        goto error;

    /* Header marker */
    i_file_size = ftell( file );
    if (fwrite (&i_file_size, sizeof (i_file_size), 1, file) != 1)
        goto error;

    i_cache = p_bank->i_cache;
    pp_cache = p_bank->pp_cache;

    if (fwrite( &i_cache, sizeof (i_cache), 1, file) != 1)
        goto error;

#define SAVE_IMMEDIATE( a ) \
    if (fwrite (&a, sizeof(a), 1, file) != 1) \
        goto error
#define SAVE_STRING( a ) \
    { \
        uint16_t i_size = (a != NULL) ? (strlen (a) + 1) : 0; \
        if ((fwrite (&i_size, sizeof (i_size), 1, file) != 1) \
         || (a && (fwrite (a, 1, i_size, file) != i_size))) \
            goto error; \
    } while(0)

    for( i = 0; i < i_cache; i++ )
    {
        uint32_t i_submodule;

        /* Save common info */
        SAVE_STRING( pp_cache[i]->psz_file );
        SAVE_IMMEDIATE( pp_cache[i]->i_time );
        SAVE_IMMEDIATE( pp_cache[i]->i_size );
        SAVE_IMMEDIATE( pp_cache[i]->b_junk );

        if( pp_cache[i]->b_junk ) continue;

        /* Save additional infos */
        SAVE_STRING( pp_cache[i]->p_module->psz_object_name );
        SAVE_STRING( pp_cache[i]->p_module->psz_shortname );
        SAVE_STRING( pp_cache[i]->p_module->psz_longname );
        SAVE_STRING( pp_cache[i]->p_module->psz_help );
        for( j = 0; j < MODULE_SHORTCUT_MAX; j++ )
        {
            SAVE_STRING( pp_cache[i]->p_module->pp_shortcuts[j] ); // FIX
        }
        SAVE_STRING( pp_cache[i]->p_module->psz_capability );
        SAVE_IMMEDIATE( pp_cache[i]->p_module->i_score );
        SAVE_IMMEDIATE( pp_cache[i]->p_module->i_cpu );
        SAVE_IMMEDIATE( pp_cache[i]->p_module->b_unloadable );
        SAVE_IMMEDIATE( pp_cache[i]->p_module->b_reentrant );
        SAVE_IMMEDIATE( pp_cache[i]->p_module->b_submodule );

        /* Config stuff */
        if (CacheSaveConfig (pp_cache[i]->p_module, file))
            goto error;

        SAVE_STRING( pp_cache[i]->p_module->psz_filename );

        i_submodule = pp_cache[i]->p_module->submodule_count;
        SAVE_IMMEDIATE( i_submodule );
        if( CacheSaveSubmodule( file, pp_cache[i]->p_module->submodule ) )
            goto error;
    }

    /* Fill-up file size */
    i_file_size = ftell( file );
    fseek( file, 0, SEEK_SET );
    if (fwrite (&i_file_size, sizeof (i_file_size), 1, file) != 1
     || fflush (file)) /* flush libc buffers */
        goto error;

#ifndef WIN32
    utf8_rename (psz_tmpname, psz_filename); /* atomically replace old cache */
    fclose (file);
#else
    utf8_unlink (psz_filename);
    fclose (file);
    utf8_rename (psz_tmpname, psz_filename);
#endif
    return; /* success! */

error:
    msg_Warn (p_this, "could not write plugins cache %s (%m)",
              psz_filename);
    if (file != NULL)
    {
        clearerr (file);
        fclose (file);
    }
}

static int CacheSaveSubmodule( FILE *file, module_t *p_module )
{
    if( !p_module )
        return 0;
    if( CacheSaveSubmodule( file, p_module->next ) )
        goto error;

    SAVE_STRING( p_module->psz_object_name );
    SAVE_STRING( p_module->psz_shortname );
    SAVE_STRING( p_module->psz_longname );
    SAVE_STRING( p_module->psz_help );
    for( unsigned j = 0; j < MODULE_SHORTCUT_MAX; j++ )
         SAVE_STRING( p_module->pp_shortcuts[j] ); // FIXME

    SAVE_STRING( p_module->psz_capability );
    SAVE_IMMEDIATE( p_module->i_score );
    SAVE_IMMEDIATE( p_module->i_cpu );
    SAVE_IMMEDIATE( p_module->b_unloadable );
    SAVE_IMMEDIATE( p_module->b_reentrant );
    return 0;

error:
    return -1;
}


static int CacheSaveConfig( module_t *p_module, FILE *file )
{
    uint32_t i_lines = p_module->confsize;

    SAVE_IMMEDIATE( p_module->i_config_items );
    SAVE_IMMEDIATE( p_module->i_bool_items );
    SAVE_IMMEDIATE( i_lines );

    for (size_t i = 0; i < i_lines ; i++)
    {
        SAVE_IMMEDIATE( p_module->p_config[i] );

        SAVE_STRING( p_module->p_config[i].psz_type );
        SAVE_STRING( p_module->p_config[i].psz_name );
        SAVE_STRING( p_module->p_config[i].psz_text );
        SAVE_STRING( p_module->p_config[i].psz_longtext );
        SAVE_STRING( p_module->p_config[i].psz_oldname );
        SAVE_IMMEDIATE( p_module->p_config[i].b_removed );

        if (IsConfigStringType (p_module->p_config[i].i_type))
            SAVE_STRING( p_module->p_config[i].orig.psz );

        if( p_module->p_config[i].i_list )
        {
            if( p_module->p_config[i].ppsz_list )
            {
                for (int j = 0; j < p_module->p_config[i].i_list; j++)
                    SAVE_STRING( p_module->p_config[i].ppsz_list[j] );
            }

            if( p_module->p_config[i].ppsz_list_text )
            {
                for (int j = 0; j < p_module->p_config[i].i_list; j++)
                    SAVE_STRING( p_module->p_config[i].ppsz_list_text[j] );
            }
            if( p_module->p_config[i].pi_list )
            {
                for (int j = 0; j < p_module->p_config[i].i_list; j++)
                    SAVE_IMMEDIATE( p_module->p_config[i].pi_list[j] );
            }
        }

        for (int j = 0; j < p_module->p_config[i].i_action; j++)
            SAVE_STRING( p_module->p_config[i].ppsz_action_text[j] );

        SAVE_IMMEDIATE( p_module->p_config[i].pf_callback );
    }
    return 0;

error:
    return -1;
}

/*****************************************************************************
 * CacheMerge: Merge a cache module descriptor with a full module descriptor.
 *****************************************************************************/
void CacheMerge( vlc_object_t *p_this, module_t *p_cache, module_t *p_module )
{
    (void)p_this;

    p_cache->pf_activate = p_module->pf_activate;
    p_cache->pf_deactivate = p_module->pf_deactivate;
    p_cache->handle = p_module->handle;

    /* FIXME: This looks too simplistic an algorithm to me. What if the module
     * file was altered such that the number of order of submodules was
     * altered... after VLC started -- Courmisch, 09/2008 */
    module_t *p_child = p_module->submodule,
             *p_cchild = p_cache->submodule;
    while( p_child && p_cchild )
    {
        p_cchild->pf_activate = p_child->pf_activate;
        p_cchild->pf_deactivate = p_child->pf_deactivate;
        p_child = p_child->next;
        p_cchild = p_cchild->next;
    }

    p_cache->b_loaded = true;
    p_module->b_loaded = false;
}

/*****************************************************************************
 * CacheFind: finds the cache entry corresponding to a file
 *****************************************************************************/
module_cache_t *CacheFind( module_bank_t *p_bank, const char *psz_file,
                           int64_t i_time, int64_t i_size )
{
    module_cache_t **pp_cache;
    int i_cache, i;

    pp_cache = p_bank->pp_loaded_cache;
    i_cache = p_bank->i_loaded_cache;

    for( i = 0; i < i_cache; i++ )
    {
        if( !strcmp( pp_cache[i]->psz_file, psz_file ) &&
            pp_cache[i]->i_time == i_time &&
            pp_cache[i]->i_size == i_size ) return pp_cache[i];
    }

    return NULL;
}

#endif /* HAVE_DYNAMIC_PLUGINS */
