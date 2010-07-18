/*****************************************************************************
 * es_out_timeshift.h: Es Out timeshift.
 *****************************************************************************
 * Copyright (C) 2008 Laurent Aimar
 * $Id: b5eec9753e130f18c21e1b29ce8b97e3e01f4bd2 $
 *
 * Authors: Laurent Aimar < fenrir _AT_ via _DOT_ ecp _DOT_ fr>
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

#if defined(__PLUGIN__) || defined(__BUILTIN__) || !defined(__LIBVLC__)
# error This header file can only be included from LibVLC.
#endif

#ifndef _INPUT_ES_OUT_TIMESHIFT_H
#define _INPUT_ES_OUT_TIMESHIFT_H 1

#include <vlc_common.h>


es_out_t *input_EsOutTimeshiftNew( input_thread_t *, es_out_t *, int i_rate );

#endif
