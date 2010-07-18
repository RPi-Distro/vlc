/*****************************************************************************
 * chain.c : configuration module chain parsing stuff
 *****************************************************************************
 * Copyright (C) 2002-2007 the VideoLAN team
 * $Id: a9a6fce29713eeabfdeb0e8c94f4cda504bb25e4 $
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
 *          Laurent Aimar <fenrir@via.ecp.fr>
 *          Eric Petit <titer@videolan.org>
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

#include <vlc_common.h>
#include "libvlc.h"
#include <vlc_charset.h>

#include "vlc_interface.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static bool IsEscapeNeeded( char c )
{
    return c == '\'' || c == '"' || c == '\\';
}
static bool IsEscape( const char *psz )
{
    if( !psz )
        return false;
    return psz[0] == '\\' && IsEscapeNeeded( psz[1] );
}
static bool IsSpace( char c  )
{
    return c == ' ' || c == '\t';
}

#define SKIPSPACE( p ) p += strspn( p, " \t" )
#define SKIPTRAILINGSPACE( p, e ) \
    do { while( e > p && IsSpace( *(e-1) ) ) e--; } while(0)

/**
 * This function will return a pointer after the end of a string element.
 * It will search the closing element which is
 * } for { (it will handle nested { ... })
 * " for "
 * ' for '
 */
static const char *ChainGetEnd( const char *psz_string )
{
    const char *p = psz_string;
    char c;

    if( !psz_string )
        return NULL;

    /* Look for a opening character */
    SKIPSPACE( p );

    for( ;; p++)
    {
        if( *p == '\0' || *p == ',' || *p == '}' )
            return p;

        if( *p == '{' || *p == '"' || *p == '\'' )
            break;
    }

    /* Set c to the closing character */
    if( *p == '{' )
        c = '}';
    else
        c = *p;
    p++;

    /* Search the closing character, handle nested {..} */
    for( ;; )
    {
        if( *p == '\0')
            return p;

        if( IsEscape( p ) )
            p += 2;
        else if( *p == c )
            return ++p;
        else if( *p == '{' && c == '}' )
            p = ChainGetEnd( p );
        else
            p++;
    }
}

/**
 * It will extract an option value (=... or {...}).
 * It will remove the initial = if present but keep the {}
 */
static char *ChainGetValue( const char **ppsz_string )
{
    const char *p = *ppsz_string;

    char *psz_value = NULL;
    const char *end;
    bool b_keep_brackets = (*p == '{');

    if( *p == '=' )
        p++;

    end = ChainGetEnd( p );
    if( end <= p )
    {
        psz_value = NULL;
    }
    else
    {
        /* Skip heading and trailing spaces.
         * This ain't necessary but will avoid simple
         * user mistakes. */
        SKIPSPACE( p );
    }

    if( end <= p )
    {
        psz_value = NULL;
    }
    else
    {
        if( *p == '\'' || *p == '"' || ( !b_keep_brackets && *p == '{' ) )
        {
            p++;

            if( *(end-1) != '\'' && *(end-1) == '"' )
                SKIPTRAILINGSPACE( p, end );

            if( end - 1 <= p )
                psz_value = NULL;
            else
                psz_value = strndup( p, end -1 - p );
        }
        else
        {
            SKIPTRAILINGSPACE( p, end );
            if( end <= p )
                psz_value = NULL;
            else
                psz_value = strndup( p, end - p );
        }
    }

    /* */
    if( psz_value )
        config_StringUnescape( psz_value );

    /* */
    *ppsz_string = end;
    return psz_value;
}

char *config_ChainCreate( char **ppsz_name, config_chain_t **pp_cfg,
                          const char *psz_chain )
{
    config_chain_t **pp_next = pp_cfg;
    size_t len;

    *ppsz_name = NULL;
    *pp_cfg    = NULL;

    if( !psz_chain )
        return NULL;
    SKIPSPACE( psz_chain );

    /* Look for parameter (a {...} or :...) or end of name (space or nul) */
    len = strcspn( psz_chain, "{: \t" );
    *ppsz_name = strndup( psz_chain, len );
    psz_chain += len;

    /* Parse the parameters */
    SKIPSPACE( psz_chain );
    if( *psz_chain == '{' )
    {
        /* Parse all name=value[,] elements */
        do
        {
            psz_chain++; /* skip previous delimiter */
            SKIPSPACE( psz_chain );

            /* Look for the end of the name (,={}_space_) */
            len = strcspn( psz_chain, "=,{} \t" );
            if( len == 0 )
                continue; /* ignore empty parameter */

            /* Append the new parameter */
            config_chain_t *p_cfg = malloc( sizeof(*p_cfg) );
            if( !p_cfg )
                break;
            p_cfg->psz_name = strndup( psz_chain, len );
            psz_chain += len;
            p_cfg->psz_value = NULL;
            p_cfg->p_next = NULL;

            *pp_next = p_cfg;
            pp_next = &p_cfg->p_next;

            /* Extract the option value */
            SKIPSPACE( psz_chain );
            if( strchr( "={", *psz_chain ) )
            {
                p_cfg->psz_value = ChainGetValue( &psz_chain );
                SKIPSPACE( psz_chain );
            }
        }
        while( !memchr( "}", *psz_chain, 2 ) );

        if( *psz_chain ) psz_chain++; /* skip '}' */;
        SKIPSPACE( psz_chain );
    }

    if( *psz_chain == ':' )
        return strdup( psz_chain + 1 );

    return NULL;
}

void config_ChainDestroy( config_chain_t *p_cfg )
{
    while( p_cfg != NULL )
    {
        config_chain_t *p_next;

        p_next = p_cfg->p_next;

        FREENULL( p_cfg->psz_name );
        FREENULL( p_cfg->psz_value );
        free( p_cfg );

        p_cfg = p_next;
    }
}

#undef config_ChainParse
void config_ChainParse( vlc_object_t *p_this, const char *psz_prefix,
                        const char *const *ppsz_options, config_chain_t *cfg )
{
    if( psz_prefix == NULL ) psz_prefix = "";
    size_t plen = 1 + strlen( psz_prefix );

    /* First, var_Create all variables */
    for( size_t i = 0; ppsz_options[i] != NULL; i++ )
    {
        const char *optname = ppsz_options[i];
        if (optname[0] == '*')
            optname++;

        char name[plen + strlen( optname )];
        snprintf( name, sizeof (name), "%s%s", psz_prefix, optname );
        if( var_Create( p_this, name,
                        config_GetType( p_this, name ) | VLC_VAR_DOINHERIT ) )
            return /* VLC_xxx */;
    }

    /* Now parse options and set value */
    for(; cfg; cfg = cfg->p_next )
    {
        vlc_value_t val;
        bool b_yes = true;
        bool b_once = false;
        module_config_t *p_conf;
        int i_type;
        size_t i;

        if( cfg->psz_name == NULL || *cfg->psz_name == '\0' )
            continue;

        for( i = 0; ppsz_options[i] != NULL; i++ )
        {
            if( !strcmp( ppsz_options[i], cfg->psz_name ) )
            {
                break;
            }
            if( ( !strncmp( cfg->psz_name, "no-", 3 ) &&
                  !strcmp( ppsz_options[i], cfg->psz_name + 3 ) ) ||
                ( !strncmp( cfg->psz_name, "no", 2 ) &&
                  !strcmp( ppsz_options[i], cfg->psz_name + 2 ) ) )
            {
                b_yes = false;
                break;
            }

            if( *ppsz_options[i] == '*' &&
                !strcmp( &ppsz_options[i][1], cfg->psz_name ) )
            {
                b_once = true;
                break;
            }

        }

        if( ppsz_options[i] == NULL )
        {
            msg_Warn( p_this, "option %s is unknown", cfg->psz_name );
            continue;
        }

        /* create name */
        char name[plen + strlen( ppsz_options[i] )];
        const char *psz_name = name;
        snprintf( name, sizeof (name), "%s%s", psz_prefix,
                  b_once ? (ppsz_options[i] + 1) : ppsz_options[i] );

        /* Check if the option is deprecated */
        p_conf = config_FindConfig( p_this, name );

        /* This is basically cut and paste from src/misc/configuration.c
         * with slight changes */
        if( p_conf )
        {
            if( p_conf->b_removed )
            {
                msg_Err( p_this, "Option %s is not supported anymore.",
                         name );
                /* TODO: this should return an error and end option parsing
                 * ... but doing this would change the VLC API and all the
                 * modules so i'll do it later */
                continue;
            }
            if( p_conf->psz_oldname
             && !strcmp( p_conf->psz_oldname, name ) )
            {
                 psz_name = p_conf->psz_name;
                 msg_Warn( p_this, "Option %s is obsolete. Use %s instead.",
                           name, psz_name );
            }
        }
        /* </Check if the option is deprecated> */

        /* get the type of the variable */
        i_type = config_GetType( p_this, psz_name );
        if( !i_type )
        {
            msg_Warn( p_this, "unknown option %s (value=%s)",
                      cfg->psz_name, cfg->psz_value );
            continue;
        }

        i_type &= CONFIG_ITEM;

        if( i_type != VLC_VAR_BOOL && cfg->psz_value == NULL )
        {
            msg_Warn( p_this, "missing value for option %s", cfg->psz_name );
            continue;
        }
        if( i_type != VLC_VAR_STRING && b_once )
        {
            msg_Warn( p_this, "*option_name need to be a string option" );
            continue;
        }

        switch( i_type )
        {
            case VLC_VAR_BOOL:
                val.b_bool = b_yes;
                break;
            case VLC_VAR_INTEGER:
                val.i_int = strtol( cfg->psz_value ? cfg->psz_value : "0",
                                    NULL, 0 );
                break;
            case VLC_VAR_FLOAT:
                val.f_float = us_atof( cfg->psz_value ? cfg->psz_value : "0" );
                break;
            case VLC_VAR_STRING:
            case VLC_VAR_MODULE:
                val.psz_string = cfg->psz_value;
                break;
            default:
                msg_Warn( p_this, "unhandled config var type (%d)", i_type );
                memset( &val, 0, sizeof( vlc_value_t ) );
                break;
        }
        if( b_once )
        {
            vlc_value_t val2;

            var_Get( p_this, psz_name, &val2 );
            if( *val2.psz_string )
            {
                free( val2.psz_string );
                msg_Dbg( p_this, "ignoring option %s (not first occurrence)", psz_name );
                continue;
            }
            free( val2.psz_string );
        }
        var_Set( p_this, psz_name, val );
        msg_Dbg( p_this, "set config option: %s to %s", psz_name,
                 cfg->psz_value ? cfg->psz_value : "(null)" );
    }
}

config_chain_t *config_ChainDuplicate( const config_chain_t *p_src )
{
    config_chain_t *p_dst = NULL;
    config_chain_t **pp_last = &p_dst;

    for( ; p_src != NULL; p_src = p_src->p_next )
    {
        config_chain_t *p = malloc( sizeof(*p) );
        if( !p )
            break;
        p->p_next    = NULL;
        p->psz_name  = p_src->psz_name  ? strdup( p_src->psz_name )  : NULL;
        p->psz_value = p_src->psz_value ? strdup( p_src->psz_value ) : NULL;

        *pp_last = p;
        pp_last = &p->p_next;
    }
    return p_dst;
}

char *config_StringUnescape( char *psz_string )
{
    char *psz_src = psz_string;
    char *psz_dst = psz_string;
    if( !psz_src )
        return NULL;

    while( *psz_src )
    {
        if( IsEscape( psz_src ) )
            psz_src++;
        *psz_dst++ = *psz_src++;
    }
    *psz_dst = '\0';

    return psz_string;
}

char *config_StringEscape( const char *psz_string )
{
    char *psz_return;
    char *psz_dst;
    int i_escape;

    if( !psz_string )
        return NULL;

    i_escape = 0;
    for( const char *p = psz_string; *p; p++ )
    {
        if( IsEscapeNeeded( *p ) )
            i_escape++;
    }

    psz_return = psz_dst = malloc( strlen( psz_string ) + i_escape + 1 );
    for( const char *p = psz_string; *p; p++ )
    {
        if( IsEscapeNeeded( *p ) )
            *psz_dst++ = '\\';
        *psz_dst++ = *p;
    }
    *psz_dst = '\0';

    return psz_return;
}


