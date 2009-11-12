/*****************************************************************************
 * test3.m : Empty Objective C module for vlc
 *****************************************************************************
 * Copyright (C) 2000-2001 the VideoLAN team
 * $Id$
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
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
#include <vlc_plugin.h>

#include <objc/Object.h>

/*****************************************************************************
 * The description class
 *****************************************************************************/
@class Desc;

@interface Desc : Object
+ (const char*) ription;
@end

@implementation Desc
+ (const char*) ription
{
    return "Objective C module that does nothing";
}
@end

/*****************************************************************************
 * Module descriptor.
 *****************************************************************************/
vlc_module_begin ()
    set_description( N_([Desc ription]) )
vlc_module_end ()


