/*****************************************************************************
 * core.c management of the modules configuration
 *****************************************************************************
 * Copyright (C) 2001-2007 the VideoLAN team
 * $Id: e54df62248c1ccc614016e51feea4d808ee7be9d $
 *
 * Authors: Gildas Bazin <gbazin@videolan.org>
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
#include "vlc_keys.h"
#include "vlc_configuration.h"

#include <assert.h>

#include "configuration.h"
#include "modules/modules.h"

vlc_rwlock_t config_lock;

static inline char *strdupnull (const char *src)
{
    return src ? strdup (src) : NULL;
}

/* Item types that use a string value (i.e. serialized in the module cache) */
int IsConfigStringType (int type)
{
    static const unsigned char config_types[] =
    {
        CONFIG_ITEM_STRING, CONFIG_ITEM_FILE, CONFIG_ITEM_MODULE,
        CONFIG_ITEM_DIRECTORY, CONFIG_ITEM_MODULE_CAT, CONFIG_ITEM_PASSWORD,
        CONFIG_ITEM_MODULE_LIST, CONFIG_ITEM_MODULE_LIST_CAT, CONFIG_ITEM_FONT
    };

    /* NOTE: this needs to be changed if we ever get more than 255 types */
    return memchr (config_types, type, sizeof (config_types)) != NULL;
}


int IsConfigIntegerType (int type)
{
    static const unsigned char config_types[] =
    {
        CONFIG_ITEM_INTEGER, CONFIG_ITEM_KEY, CONFIG_ITEM_BOOL,
        CONFIG_CATEGORY, CONFIG_SUBCATEGORY
    };

    return memchr (config_types, type, sizeof (config_types)) != NULL;
}

#undef config_GetType
/*****************************************************************************
 * config_GetType: get the type of a variable (bool, int, float, string)
 *****************************************************************************
 * This function is used to get the type of a variable from its name.
 * Beware, this is quite slow.
 *****************************************************************************/
int config_GetType( vlc_object_t *p_this, const char *psz_name )
{
    module_config_t *p_config;
    int i_type;

    p_config = config_FindConfig( p_this, psz_name );

    /* sanity checks */
    if( !p_config )
    {
        return 0;
    }

    switch( p_config->i_type )
    {
    case CONFIG_ITEM_BOOL:
        i_type = VLC_VAR_BOOL;
        break;

    case CONFIG_ITEM_INTEGER:
    case CONFIG_ITEM_KEY:
        i_type = VLC_VAR_INTEGER;
        break;

    case CONFIG_ITEM_FLOAT:
        i_type = VLC_VAR_FLOAT;
        break;

    case CONFIG_ITEM_MODULE:
    case CONFIG_ITEM_MODULE_CAT:
    case CONFIG_ITEM_MODULE_LIST:
    case CONFIG_ITEM_MODULE_LIST_CAT:
        i_type = VLC_VAR_MODULE;
        break;

    case CONFIG_ITEM_STRING:
        i_type = VLC_VAR_STRING;
        break;

    case CONFIG_ITEM_PASSWORD:
        i_type = VLC_VAR_STRING;
        break;

    case CONFIG_ITEM_FILE:
        i_type = VLC_VAR_FILE;
        break;

    case CONFIG_ITEM_DIRECTORY:
        i_type = VLC_VAR_DIRECTORY;
        break;

    default:
        i_type = 0;
        break;
    }

    return i_type;
}

#undef config_GetInt
/*****************************************************************************
 * config_GetInt: get the value of an int variable
 *****************************************************************************
 * This function is used to get the value of variables which are internally
 * represented by an integer (CONFIG_ITEM_INTEGER and
 * CONFIG_ITEM_BOOL).
 *****************************************************************************/
int config_GetInt( vlc_object_t *p_this, const char *psz_name )
{
    module_config_t *p_config;

    p_config = config_FindConfig( p_this, psz_name );

    /* sanity checks */
    if( !p_config )
    {
        msg_Err( p_this, "option %s does not exist", psz_name );
        return -1;
    }

    if (!IsConfigIntegerType (p_config->i_type))
    {
        msg_Err( p_this, "option %s does not refer to an int", psz_name );
        return -1;
    }

    int val;

    vlc_rwlock_rdlock (&config_lock);
    val = p_config->value.i;
    vlc_rwlock_unlock (&config_lock);
    return val;
}

#undef config_GetFloat
/*****************************************************************************
 * config_GetFloat: get the value of a float variable
 *****************************************************************************
 * This function is used to get the value of variables which are internally
 * represented by a float (CONFIG_ITEM_FLOAT).
 *****************************************************************************/
float config_GetFloat( vlc_object_t *p_this, const char *psz_name )
{
    module_config_t *p_config;

    p_config = config_FindConfig( p_this, psz_name );

    /* sanity checks */
    if( !p_config )
    {
        msg_Err( p_this, "option %s does not exist", psz_name );
        return -1;
    }

    if (!IsConfigFloatType (p_config->i_type))
    {
        msg_Err( p_this, "option %s does not refer to a float", psz_name );
        return -1;
    }

    float val;

    vlc_rwlock_rdlock (&config_lock);
    val = p_config->value.f;
    vlc_rwlock_unlock (&config_lock);
    return val;
}

#undef config_GetPsz
/*****************************************************************************
 * config_GetPsz: get the string value of a string variable
 *****************************************************************************
 * This function is used to get the value of variables which are internally
 * represented by a string (CONFIG_ITEM_STRING, CONFIG_ITEM_FILE,
 * CONFIG_ITEM_DIRECTORY, CONFIG_ITEM_PASSWORD, and CONFIG_ITEM_MODULE).
 *
 * Important note: remember to free() the returned char* because it's a
 *   duplicate of the actual value. It isn't safe to return a pointer to the
 *   actual value as it can be modified at any time.
 *****************************************************************************/
char * config_GetPsz( vlc_object_t *p_this, const char *psz_name )
{
    module_config_t *p_config;

    p_config = config_FindConfig( p_this, psz_name );

    /* sanity checks */
    if( !p_config )
    {
        msg_Err( p_this, "option %s does not exist", psz_name );
        return NULL;
    }

    if (!IsConfigStringType (p_config->i_type))
    {
        msg_Err( p_this, "option %s does not refer to a string", psz_name );
        return NULL;
    }

    /* return a copy of the string */
    vlc_rwlock_rdlock (&config_lock);
    char *psz_value = strdupnull (p_config->value.psz);
    vlc_rwlock_unlock (&config_lock);

    return psz_value;
}

#undef config_PutPsz
/*****************************************************************************
 * config_PutPsz: set the string value of a string variable
 *****************************************************************************
 * This function is used to set the value of variables which are internally
 * represented by a string (CONFIG_ITEM_STRING, CONFIG_ITEM_FILE,
 * CONFIG_ITEM_DIRECTORY, CONFIG_ITEM_PASSWORD, and CONFIG_ITEM_MODULE).
 *****************************************************************************/
void config_PutPsz( vlc_object_t *p_this,
                      const char *psz_name, const char *psz_value )
{
    module_config_t *p_config;
    vlc_value_t oldval;

    p_config = config_FindConfig( p_this, psz_name );


    /* sanity checks */
    if( !p_config )
    {
        msg_Warn( p_this, "option %s does not exist", psz_name );
        return;
    }

    if (!IsConfigStringType (p_config->i_type))
    {
        msg_Err( p_this, "option %s does not refer to a string", psz_name );
        return;
    }

    vlc_rwlock_wrlock (&config_lock);

    /* backup old value */
    oldval.psz_string = (char *)p_config->value.psz;

    if ((psz_value != NULL) && *psz_value)
        p_config->value.psz = strdup (psz_value);
    else
        p_config->value.psz = NULL;

    p_config->b_dirty = true;
    vlc_rwlock_unlock (&config_lock);

    if( p_config->pf_callback )
    {
        vlc_value_t val;

        val.psz_string = (char *)psz_value;
        p_config->pf_callback( p_this, psz_name, oldval, val,
                               p_config->p_callback_data );
    }

    /* free old string */
    free( oldval.psz_string );
}

#undef config_PutInt
/*****************************************************************************
 * config_PutInt: set the integer value of an int variable
 *****************************************************************************
 * This function is used to set the value of variables which are internally
 * represented by an integer (CONFIG_ITEM_INTEGER and
 * CONFIG_ITEM_BOOL).
 *****************************************************************************/
void config_PutInt( vlc_object_t *p_this, const char *psz_name, int i_value )
{
    module_config_t *p_config;
    vlc_value_t oldval;

    p_config = config_FindConfig( p_this, psz_name );

    /* sanity checks */
    if( !p_config )
    {
        msg_Warn( p_this, "option %s does not exist", psz_name );
        return;
    }

    if (!IsConfigIntegerType (p_config->i_type))
    {
        msg_Err( p_this, "option %s does not refer to an int", psz_name );
        return;
    }

    /* if i_min == i_max == 0, then do not use them */
    if ((p_config->min.i == 0) && (p_config->max.i == 0))
        ;
    else if (i_value < p_config->min.i)
        i_value = p_config->min.i;
    else if (i_value > p_config->max.i)
        i_value = p_config->max.i;

    vlc_rwlock_wrlock (&config_lock);
    /* backup old value */
    oldval.i_int = p_config->value.i;

    p_config->value.i = i_value;
    p_config->b_dirty = true;
    vlc_rwlock_unlock (&config_lock);

    if( p_config->pf_callback )
    {
        vlc_value_t val;

        val.i_int = i_value;
        p_config->pf_callback( p_this, psz_name, oldval, val,
                               p_config->p_callback_data );
    }
}

#undef config_PutFloat
/*****************************************************************************
 * config_PutFloat: set the value of a float variable
 *****************************************************************************
 * This function is used to set the value of variables which are internally
 * represented by a float (CONFIG_ITEM_FLOAT).
 *****************************************************************************/
void config_PutFloat( vlc_object_t *p_this,
                      const char *psz_name, float f_value )
{
    module_config_t *p_config;
    vlc_value_t oldval;

    p_config = config_FindConfig( p_this, psz_name );

    /* sanity checks */
    if( !p_config )
    {
        msg_Warn( p_this, "option %s does not exist", psz_name );
        return;
    }

    if (!IsConfigFloatType (p_config->i_type))
    {
        msg_Err( p_this, "option %s does not refer to a float", psz_name );
        return;
    }

    /* if f_min == f_max == 0, then do not use them */
    if ((p_config->min.f == 0) && (p_config->max.f == 0))
        ;
    else if (f_value < p_config->min.f)
        f_value = p_config->min.f;
    else if (f_value > p_config->max.f)
        f_value = p_config->max.f;

    vlc_rwlock_wrlock (&config_lock);
    /* backup old value */
    oldval.f_float = p_config->value.f;

    p_config->value.f = f_value;
    p_config->b_dirty = true;
    vlc_rwlock_unlock (&config_lock);

    if( p_config->pf_callback )
    {
        vlc_value_t val;

        val.f_float = f_value;
        p_config->pf_callback( p_this, psz_name, oldval, val,
                               p_config->p_callback_data );
    }
}

static int confcmp (const void *a, const void *b)
{
    const module_config_t *const *ca = a, *const *cb = b;

    return strcmp ((*ca)->psz_name, (*cb)->psz_name);
}

static int confnamecmp (const void *key, const void *elem)
{
    const module_config_t *const *conf = elem;

    return strcmp (key, (*conf)->psz_name);
}

static struct
{
    module_config_t **list;
    size_t count;
} config = { NULL, 0 };

/**
 * Index the configuration items by name for faster lookups.
 */
int config_SortConfig (void)
{
    size_t nmod;
    module_t **mlist = module_list_get (&nmod);
    if (unlikely(mlist == NULL))
        return VLC_ENOMEM;

    size_t nconf = 0;
    for (size_t i = 0; i < nmod; i++)
         nconf  += mlist[i]->confsize;

    module_config_t **clist = malloc (sizeof (*clist) * nconf);
    if (unlikely(clist == NULL))
    {
        module_list_free (mlist);
        return VLC_ENOMEM;
    }

    nconf = 0;
    for (size_t i = 0; i < nmod; i++)
    {
        module_t *parser = mlist[i];
        module_config_t *item, *end;

        for (item = parser->p_config, end = item + parser->confsize;
             item < end;
             item++)
        {
            if (item->i_type & CONFIG_HINT)
                continue; /* ignore hints */
            clist[nconf++] = item;
        }
    }
    module_list_free (mlist);

    qsort (clist, nconf, sizeof (*clist), confcmp);

    config.list = clist;
    config.count = nconf;
    return VLC_SUCCESS;
}

void config_UnsortConfig (void)
{
    module_config_t **clist;

    clist = config.list;
    config.list = NULL;
    config.count = 0;

    free (clist);
}

/*****************************************************************************
 * config_FindConfig: find the config structure associated with an option.
 *****************************************************************************
 * FIXME: remove p_this pointer parameter (or use it)
 *****************************************************************************/
module_config_t *config_FindConfig (vlc_object_t *p_this, const char *name)
{
    VLC_UNUSED(p_this);

    if (unlikely(name == NULL))
        return NULL;

    module_config_t *const *p;
    p = bsearch (name, config.list, config.count, sizeof (*p), confnamecmp);
    return p ? *p : NULL;
}

/*****************************************************************************
 * config_Free: frees a duplicated module's configuration data.
 *****************************************************************************
 * This function frees all the data duplicated by config_Duplicate.
 *****************************************************************************/
void config_Free( module_t *p_module )
{
    int i;

    for (size_t j = 0; j < p_module->confsize; j++)
    {
        module_config_t *p_item = p_module->p_config + j;

        free( p_item->psz_type );
        free( p_item->psz_name );
        free( p_item->psz_text );
        free( p_item->psz_longtext );
        free( p_item->psz_oldname );

        if (IsConfigStringType (p_item->i_type))
        {
            free (p_item->value.psz);
            free (p_item->orig.psz);
            free (p_item->saved.psz);
        }

        if( p_item->ppsz_list )
            for( i = 0; i < p_item->i_list; i++ )
                free( p_item->ppsz_list[i] );
        if( p_item->ppsz_list_text )
            for( i = 0; i < p_item->i_list; i++ )
                free( p_item->ppsz_list_text[i] );
        free( p_item->ppsz_list );
        free( p_item->ppsz_list_text );
        free( p_item->pi_list );

        if( p_item->i_action )
        {
            for( i = 0; i < p_item->i_action; i++ )
            {
                free( p_item->ppsz_action_text[i] );
            }
            free( p_item->ppf_action );
            free( p_item->ppsz_action_text );
        }
    }

    free (p_module->p_config);
    p_module->p_config = NULL;
}

#undef config_ResetAll
/*****************************************************************************
 * config_ResetAll: reset the configuration data for all the modules.
 *****************************************************************************/
void config_ResetAll( vlc_object_t *p_this )
{
    VLC_UNUSED(p_this);
    module_t *p_module;
    module_t **list = module_list_get (NULL);

    vlc_rwlock_wrlock (&config_lock);
    for (size_t j = 0; (p_module = list[j]) != NULL; j++)
    {
        if( p_module->b_submodule ) continue;

        for (size_t i = 0; i < p_module->confsize; i++ )
        {
            module_config_t *p_config = p_module->p_config + i;

            if (IsConfigIntegerType (p_config->i_type))
                p_config->value.i = p_config->orig.i;
            else
            if (IsConfigFloatType (p_config->i_type))
                p_config->value.f = p_config->orig.f;
            else
            if (IsConfigStringType (p_config->i_type))
            {
                free ((char *)p_config->value.psz);
                p_config->value.psz =
                        strdupnull (p_config->orig.psz);
            }
        }
    }
    vlc_rwlock_unlock (&config_lock);

    module_list_free (list);
}
