
/*****************************************************************************
 * mkv.cpp : matroska demuxer
 *****************************************************************************
 * Copyright (C) 2003-2004 the VideoLAN team
 * $Id: aeec50d13dab90597ac31bf254653343a15b46fd $
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
#include "Ebml_parser.hpp"

/*****************************************************************************
 * Ebml Stream parser
 *****************************************************************************/
EbmlParser::EbmlParser( EbmlStream *es, EbmlElement *el_start, demux_t *p_demux )
{
    int i;

    m_es = es;
    m_got = NULL;
    m_el[0] = el_start;
    mi_remain_size[0] = el_start->GetSize();

    for( i = 1; i < 6; i++ )
    {
        m_el[i] = NULL;
    }
    mi_level = 1;
    mi_user_level = 1;
    mb_keep = false;
    mb_dummy = var_InheritBool( p_demux, "mkv-use-dummy" );
}

EbmlParser::~EbmlParser( void )
{
    if( !mi_level )
    {
        assert( !mb_keep );
        delete m_el[1];
        return;
    }

    int i;

    for( i = 1; i < mi_level; i++ )
    {
        if( !mb_keep )
        {
            delete m_el[i];
        }
        mb_keep = false;
    }
}

EbmlElement* EbmlParser::UnGet( uint64 i_block_pos, uint64 i_cluster_pos )
{
    if ( mi_user_level > mi_level )
    {
        while ( mi_user_level != mi_level )
        {
            delete m_el[mi_user_level];
            m_el[mi_user_level] = NULL;
            mi_user_level--;
        }
    }
    m_got = NULL;
    mb_keep = false;
    if ( m_el[1]->GetElementPosition() == i_cluster_pos )
    {
        m_es->I_O().setFilePointer( i_block_pos, seek_beginning );
        return (EbmlMaster*) m_el[1];
    }
    else
    {
        // seek to the previous Cluster
        m_es->I_O().setFilePointer( i_cluster_pos, seek_beginning );
        mi_level--;
        mi_user_level--;
        delete m_el[mi_level];
        m_el[mi_level] = NULL;
        return NULL;
    }
}

void EbmlParser::Up( void )
{
    if( mi_user_level == mi_level )
    {
        fprintf( stderr," arrrrrrrrrrrrrg Up cannot escape itself\n" );
    }

    mi_user_level--;
}

void EbmlParser::Down( void )
{
    mi_user_level++;
    mi_level++;
}

void EbmlParser::Keep( void )
{
    mb_keep = true;
}

int EbmlParser::GetLevel( void )
{
    return mi_user_level;
}

void EbmlParser::Reset( demux_t *p_demux )
{
    while ( mi_level > 0)
    {
        delete m_el[mi_level];
        m_el[mi_level] = NULL;
        mi_level--;
    }
    mi_user_level = mi_level = 1;
    // a little faster and cleaner
    m_es->I_O().setFilePointer( static_cast<KaxSegment*>(m_el[0])->GetGlobalPosition(0) );
    mb_dummy = var_InheritBool( p_demux, "mkv-use-dummy" );
}


/* This function workarounds a bug in KaxBlockVirtual implementation */
class KaxBlockVirtualWorkaround : public KaxBlockVirtual
{
public:
    void Fix()
    {
        if( Data == DataBlock )
            SetBuffer( NULL, 0 );
    }
};

EbmlElement *EbmlParser::Get( void )
{
    int i_ulev = 0;

    if( mi_user_level != mi_level )
    {
        return NULL;
    }
    if( m_got )
    {
        EbmlElement *ret = m_got;
        m_got = NULL;

        return ret;
    }

    if( m_el[mi_level] )
    {
        m_el[mi_level]->SkipData( *m_es, m_el[mi_level]->Generic().Context );
        if( !mb_keep )
        {
            if( MKV_IS_ID( m_el[mi_level], KaxBlockVirtual ) )
                static_cast<KaxBlockVirtualWorkaround*>(m_el[mi_level])->Fix();
            delete m_el[mi_level];
        }
        mb_keep = false;
    }

    m_el[mi_level] = m_es->FindNextElement( m_el[mi_level - 1]->Generic().Context, i_ulev, 0xFFFFFFFFL, mb_dummy != 0, 1 );
//    mi_remain_size[mi_level] = m_el[mi_level]->GetSize();
    if( i_ulev > 0 )
    {
        while( i_ulev > 0 )
        {
            if( mi_level == 1 )
            {
                mi_level = 0;
                return NULL;
            }

            delete m_el[mi_level - 1];
            m_got = m_el[mi_level -1] = m_el[mi_level];
            m_el[mi_level] = NULL;

            mi_level--;
            i_ulev--;
        }
        return NULL;
    }
    else if( m_el[mi_level] == NULL )
    {
        fprintf( stderr," m_el[mi_level] == NULL\n" );
    }

    return m_el[mi_level];
}

bool EbmlParser::IsTopPresent( EbmlElement *el )
{
    for( int i = 0; i < mi_level; i++ )
    {
        if( m_el[i] && m_el[i] == el )
            return true;
    }
    return false;
}


