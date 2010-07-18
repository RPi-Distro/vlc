/*****************************************************************************
 * textdomain.c : Modules text domain management
 *****************************************************************************
 * Copyright (C) 2010 Rémi Denis-Courmont
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
#include "modules/modules.h"

#ifdef ENABLE_NLS
# include <libintl.h>
# if defined (__APPLE__) || defined (WIN32)
#  include "config/configuration.h"
# endif
#endif

int vlc_bindtextdomain (const char *domain)
{
    int ret = 0;

#if defined (ENABLE_NLS)
    /* Specify where to find the locales for current domain */
# if !defined (__APPLE__) && !defined (WIN32)
    static const char path[] = LOCALEDIR;
# else
    char *datadir = config_GetDataDirDefault();
    char *path;

    if (unlikely(datadir == NULL))
        return -1;
    ret = asprintf (&path, "%s" DIR_SEP "locale", datadir);
    free (datadir);
# endif

    if (bindtextdomain (domain, path) == NULL)
    {
        fprintf (stderr, "%s: text domain not found in %s\n", domain, path);
        ret = -1;
        goto out;
    }

    /* LibVLC wants all messages in UTF-8.
     * Unfortunately, we cannot ask UTF-8 for strerror_r(), strsignal_r()
     * and other functions that are not part of our text domain.
     */
    if (bind_textdomain_codeset (PACKAGE_NAME, "UTF-8") == NULL)
    {
        fprintf (stderr, "%s: UTF-8 encoding bot available\n", domain);
        // Unbinds the text domain to avoid broken encoding
        bindtextdomain (PACKAGE_NAME, "/DOES_NOT_EXIST");
        ret = -1;
        goto out;
    }

    /* LibVLC does NOT set the default textdomain, since it is a library.
     * This could otherwise break programs using LibVLC (other than VLC).
     * textdomain (PACKAGE_NAME);
     */
out:
# if defined (__APPLE__) || defined (WIN32)
    free (path);
# endif

#else /* !ENABLE_NLS */
    (void)domain;
#endif

    return ret;
}


