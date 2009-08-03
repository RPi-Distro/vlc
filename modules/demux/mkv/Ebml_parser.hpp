
/*****************************************************************************
 * mkv.cpp : matroska demuxer
 *****************************************************************************
 * Copyright (C) 2003-2004 the VideoLAN team
 * $Id: a4d9bc5ad3548da317c81985775853d667182293 $
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Steve Lhomme <steve.lhomme@free.fr>
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
#ifndef _EBML_PARSER_HPP_
#define _EBML_PARSER_HPP_

#include "mkv.hpp"
/*****************************************************************************
 * Ebml Stream parser
 *****************************************************************************/
class EbmlParser
{
  public:
    EbmlParser( EbmlStream *es, EbmlElement *el_start, demux_t *p_demux );
    virtual ~EbmlParser( void );

    void Up( void );
    void Down( void );
    void Reset( demux_t *p_demux );
    EbmlElement *Get( void );
    void        Keep( void );
    EbmlElement *UnGet( uint64 i_block_pos, uint64 i_cluster_pos );

    int  GetLevel( void );

    /* Is the provided element presents in our upper elements */
    bool IsTopPresent( EbmlElement * );

  private:
    EbmlStream  *m_es;
    int         mi_level;
    EbmlElement *m_el[10];
    int64_t      mi_remain_size[10];

    EbmlElement *m_got;

    int         mi_user_level;
    bool        mb_keep;
    bool        mb_dummy;
};

#endif
