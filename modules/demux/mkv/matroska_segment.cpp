/*****************************************************************************
 * mkv.cpp : matroska demuxer
 *****************************************************************************
 * Copyright (C) 2003-2004 the VideoLAN team
 * $Id: 117dc0390139eab7fed2119590fd924e5407257e $
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

#include "matroska_segment.hpp"

#include "chapters.hpp"

#include "demux.hpp"

extern "C" {
#include "../vobsub.h"
}

/* GetFourCC helper */
#define GetFOURCC( p )  __GetFOURCC( (uint8_t*)p )
static vlc_fourcc_t __GetFOURCC( uint8_t *p )
{
    return VLC_FOURCC( p[0], p[1], p[2], p[3] );
}

/* Destructor */
matroska_segment_c::~matroska_segment_c()
{
    for( size_t i_track = 0; i_track < tracks.size(); i_track++ )
    {
        delete tracks[i_track]->p_compression_data;
        es_format_Clean( &tracks[i_track]->fmt );
        free( tracks[i_track]->p_extra_data );
        free( tracks[i_track]->psz_codec );
        delete tracks[i_track];
    }

    free( psz_writing_application );
    free( psz_muxing_application );
    free( psz_segment_filename );
    free( psz_title );
    free( psz_date_utc );
    free( p_indexes );

    delete ep;
    delete segment;
    delete p_segment_uid;
    delete p_prev_segment_uid;
    delete p_next_segment_uid;

    std::vector<chapter_edition_c*>::iterator index = stored_editions.begin();
    while ( index != stored_editions.end() )
    {
        delete (*index);
        index++;
    }
    std::vector<chapter_translation_c*>::iterator indext = translations.begin();
    while ( indext != translations.end() )
    {
        delete (*indext);
        indext++;
    }
    std::vector<KaxSegmentFamily*>::iterator indexf = families.begin();
    while ( indexf != families.end() )
    {
        delete (*indexf);
        indexf++;
   }
}


/*****************************************************************************
 * Tools
 *  * LoadCues : load the cues element and update index
 *
 *  * LoadTags : load ... the tags element
 *
 *  * InformationCreate : create all information, load tags if present
 *
 *****************************************************************************/
void matroska_segment_c::LoadCues( KaxCues *cues )
{
    EbmlParser  *ep;
    EbmlElement *el;

    if( b_cues )
    {
        msg_Err( &sys.demuxer, "There can be only 1 Cues per section." );
        return;
    }

    ep = new EbmlParser( &es, cues, &sys.demuxer );
    while( ( el = ep->Get() ) != NULL )
    {
        if( MKV_IS_ID( el, KaxCuePoint ) )
        {
#define idx p_indexes[i_index]

            idx.i_track       = -1;
            idx.i_block_number= -1;
            idx.i_position    = -1;
            idx.i_time        = 0;
            idx.b_key         = true;

            ep->Down();
            while( ( el = ep->Get() ) != NULL )
            {
                if( MKV_IS_ID( el, KaxCueTime ) )
                {
                    KaxCueTime &ctime = *(KaxCueTime*)el;

                    ctime.ReadData( es.I_O() );

                    idx.i_time = uint64( ctime ) * i_timescale / (mtime_t)1000;
                }
                else if( MKV_IS_ID( el, KaxCueTrackPositions ) )
                {
                    ep->Down();
                    while( ( el = ep->Get() ) != NULL )
                    {
                        if( MKV_IS_ID( el, KaxCueTrack ) )
                        {
                            KaxCueTrack &ctrack = *(KaxCueTrack*)el;

                            ctrack.ReadData( es.I_O() );
                            idx.i_track = uint16( ctrack );
                        }
                        else if( MKV_IS_ID( el, KaxCueClusterPosition ) )
                        {
                            KaxCueClusterPosition &ccpos = *(KaxCueClusterPosition*)el;

                            ccpos.ReadData( es.I_O() );
                            idx.i_position = segment->GetGlobalPosition( uint64( ccpos ) );
                        }
                        else if( MKV_IS_ID( el, KaxCueBlockNumber ) )
                        {
                            KaxCueBlockNumber &cbnum = *(KaxCueBlockNumber*)el;

                            cbnum.ReadData( es.I_O() );
                            idx.i_block_number = uint32( cbnum );
                        }
                        else
                        {
                            msg_Dbg( &sys.demuxer, "         * Unknown (%s)", typeid(*el).name() );
                        }
                    }
                    ep->Up();
                }
                else
                {
                    msg_Dbg( &sys.demuxer, "     * Unknown (%s)", typeid(*el).name() );
                }
            }
            ep->Up();

#if 0
            msg_Dbg( &sys.demuxer, " * added time=%"PRId64" pos=%"PRId64
                     " track=%d bnum=%d", idx.i_time, idx.i_position,
                     idx.i_track, idx.i_block_number );
#endif

            i_index++;
            if( i_index >= i_index_max )
            {
                i_index_max += 1024;
                p_indexes = (mkv_index_t*)xrealloc( p_indexes,
                                        sizeof( mkv_index_t ) * i_index_max );
            }
#undef idx
        }
        else
        {
            msg_Dbg( &sys.demuxer, " * Unknown (%s)", typeid(*el).name() );
        }
    }
    delete ep;
    b_cues = true;
    msg_Dbg( &sys.demuxer, "|   - loading cues done." );
}

void matroska_segment_c::LoadTags( KaxTags *tags )
{
    EbmlParser  *ep;
    EbmlElement *el;

    /* Master elements */
    ep = new EbmlParser( &es, tags, &sys.demuxer );

    while( ( el = ep->Get() ) != NULL )
    {
        if( MKV_IS_ID( el, KaxTag ) )
        {
            msg_Dbg( &sys.demuxer, "+ Tag" );
            ep->Down();
            while( ( el = ep->Get() ) != NULL )
            {
                if( MKV_IS_ID( el, KaxTagTargets ) )
                {
                    msg_Dbg( &sys.demuxer, "|   + Targets" );
                    ep->Down();
                    while( ( el = ep->Get() ) != NULL )
                    {
                        msg_Dbg( &sys.demuxer, "|   |   + Unknown (%s)", typeid( *el ).name() );
                    }
                    ep->Up();
                }
                else if( MKV_IS_ID( el, KaxTagGeneral ) )
                {
                    msg_Dbg( &sys.demuxer, "|   + General" );
                    ep->Down();
                    while( ( el = ep->Get() ) != NULL )
                    {
                        msg_Dbg( &sys.demuxer, "|   |   + Unknown (%s)", typeid( *el ).name() );
                    }
                    ep->Up();
                }
                else if( MKV_IS_ID( el, KaxTagGenres ) )
                {
                    msg_Dbg( &sys.demuxer, "|   + Genres" );
                    ep->Down();
                    while( ( el = ep->Get() ) != NULL )
                    {
                        msg_Dbg( &sys.demuxer, "|   |   + Unknown (%s)", typeid( *el ).name() );
                    }
                    ep->Up();
                }
                else if( MKV_IS_ID( el, KaxTagAudioSpecific ) )
                {
                    msg_Dbg( &sys.demuxer, "|   + Audio Specific" );
                    ep->Down();
                    while( ( el = ep->Get() ) != NULL )
                    {
                        msg_Dbg( &sys.demuxer, "|   |   + Unknown (%s)", typeid( *el ).name() );
                    }
                    ep->Up();
                }
                else if( MKV_IS_ID( el, KaxTagImageSpecific ) )
                {
                    msg_Dbg( &sys.demuxer, "|   + Images Specific" );
                    ep->Down();
                    while( ( el = ep->Get() ) != NULL )
                    {
                        msg_Dbg( &sys.demuxer, "|   |   + Unknown (%s)", typeid( *el ).name() );
                    }
                    ep->Up();
                }
                else if( MKV_IS_ID( el, KaxTagMultiComment ) )
                {
                    msg_Dbg( &sys.demuxer, "|   + Multi Comment" );
                }
                else if( MKV_IS_ID( el, KaxTagMultiCommercial ) )
                {
                    msg_Dbg( &sys.demuxer, "|   + Multi Commercial" );
                }
                else if( MKV_IS_ID( el, KaxTagMultiDate ) )
                {
                    msg_Dbg( &sys.demuxer, "|   + Multi Date" );
                }
                else if( MKV_IS_ID( el, KaxTagMultiEntity ) )
                {
                    msg_Dbg( &sys.demuxer, "|   + Multi Entity" );
                }
                else if( MKV_IS_ID( el, KaxTagMultiIdentifier ) )
                {
                    msg_Dbg( &sys.demuxer, "|   + Multi Identifier" );
                }
                else if( MKV_IS_ID( el, KaxTagMultiLegal ) )
                {
                    msg_Dbg( &sys.demuxer, "|   + Multi Legal" );
                }
                else if( MKV_IS_ID( el, KaxTagMultiTitle ) )
                {
                    msg_Dbg( &sys.demuxer, "|   + Multi Title" );
                }
                else
                {
                    msg_Dbg( &sys.demuxer, "|   + LoadTag Unknown (%s)", typeid( *el ).name() );
                }
            }
            ep->Up();
        }
        else
        {
            msg_Dbg( &sys.demuxer, "+ Unknown (%s)", typeid( *el ).name() );
        }
    }
    delete ep;

    msg_Dbg( &sys.demuxer, "loading tags done." );
}

/*****************************************************************************
 * InformationCreate:
 *****************************************************************************/
void matroska_segment_c::InformationCreate( )
{
    sys.meta = vlc_meta_New();

    if( psz_title )
    {
        vlc_meta_SetTitle( sys.meta, psz_title );
    }
    if( psz_date_utc )
    {
        vlc_meta_SetDate( sys.meta, psz_date_utc );
    }
#if 0
    if( psz_segment_filename )
    {
        fprintf( stderr, "***** WARNING: Unhandled meta - Use custom\n" );
    }
    if( psz_muxing_application )
    {
        fprintf( stderr, "***** WARNING: Unhandled meta - Use custom\n" );
    }
    if( psz_writing_application )
    {
        fprintf( stderr, "***** WARNING: Unhandled meta - Use custom\n" );
    }

    for( size_t i_track = 0; i_track < tracks.size(); i_track++ )
    {
//        mkv_track_t *tk = tracks[i_track];
//        vlc_meta_t *mtk = vlc_meta_New();
        fprintf( stderr, "***** WARNING: Unhandled child meta\n");
    }
#endif
#if 0
    if( i_tags_position >= 0 )
    {
        bool b_seekable;

        stream_Control( sys.demuxer.s, STREAM_CAN_FASTSEEK, &b_seekable );
        if( b_seekable )
        {
            LoadTags( );
        }
    }
#endif
}


/*****************************************************************************
 * Misc
 *****************************************************************************/

void matroska_segment_c::IndexAppendCluster( KaxCluster *cluster )
{
#define idx p_indexes[i_index]
    idx.i_track       = -1;
    idx.i_block_number= -1;
    idx.i_position    = cluster->GetElementPosition();
    idx.i_time        = -1;
    idx.b_key         = true;

    i_index++;
    if( i_index >= i_index_max )
    {
        i_index_max += 1024;
        p_indexes = (mkv_index_t*)xrealloc( p_indexes,
                                        sizeof( mkv_index_t ) * i_index_max );
    }
#undef idx
}


bool matroska_segment_c::PreloadFamily( const matroska_segment_c & of_segment )
{
    if ( b_preloaded )
        return false;

    for (size_t i=0; i<families.size(); i++)
    {
        for (size_t j=0; j<of_segment.families.size(); j++)
        {
            if ( *(families[i]) == *(of_segment.families[j]) )
                return Preload( );
        }
    }

    return false;
}

bool matroska_segment_c::CompareSegmentUIDs( const matroska_segment_c * p_item_a, const matroska_segment_c * p_item_b )
{
    EbmlBinary *p_tmp;

    if ( p_item_a == NULL || p_item_b == NULL )
        return false;

    p_tmp = (EbmlBinary *)p_item_a->p_segment_uid;
    if ( p_item_b->p_prev_segment_uid != NULL
          && *p_tmp == *p_item_b->p_prev_segment_uid )
        return true;

    p_tmp = (EbmlBinary *)p_item_a->p_next_segment_uid;
    if ( !p_tmp )
        return false;
 
    if ( p_item_b->p_segment_uid != NULL
          && *p_tmp == *p_item_b->p_segment_uid )
        return true;

    if ( p_item_b->p_prev_segment_uid != NULL
          && *p_tmp == *p_item_b->p_prev_segment_uid )
        return true;

    return false;
}

bool matroska_segment_c::Preload( )
{
    if ( b_preloaded )
        return false;

    EbmlElement *el = NULL;

    ep->Reset( &sys.demuxer );

    while( ( el = ep->Get() ) != NULL )
    {
        if( MKV_IS_ID( el, KaxSeekHead ) )
        {
            /* Multiple allowed */
            /* We bail at 10, to prevent possible recursion */
            msg_Dbg(  &sys.demuxer, "|   + Seek head" );
            if( i_seekhead_count < 10 )
            {
                i_seekhead_position = (int64_t) es.I_O().getFilePointer();
                ParseSeekHead( static_cast<KaxSeekHead*>( el ) );
            }
        }
        else if( MKV_IS_ID( el, KaxInfo ) )
        {
            /* Multiple allowed, mandatory */
            msg_Dbg(  &sys.demuxer, "|   + Information" );
            if( i_info_position < 0 ) // FIXME
                ParseInfo( static_cast<KaxInfo*>( el ) );
            i_info_position = (int64_t) es.I_O().getFilePointer();
        }
        else if( MKV_IS_ID( el, KaxTracks ) )
        {
            /* Multiple allowed */
            msg_Dbg(  &sys.demuxer, "|   + Tracks" );
            if( i_tracks_position < 0 ) // FIXME
                ParseTracks( static_cast<KaxTracks*>( el ) );
            if ( tracks.size() == 0 )
            {
                msg_Err( &sys.demuxer, "No tracks supported" );
                return false;
            }
            i_tracks_position = (int64_t) es.I_O().getFilePointer();
        }
        else if( MKV_IS_ID( el, KaxCues ) )
        {
            msg_Dbg(  &sys.demuxer, "|   + Cues" );
            if( i_cues_position < 0 )
                LoadCues( static_cast<KaxCues*>( el ) );
            i_cues_position = (int64_t) es.I_O().getFilePointer();
        }
        else if( MKV_IS_ID( el, KaxCluster ) )
        {
            msg_Dbg( &sys.demuxer, "|   + Cluster" );

            cluster = (KaxCluster*)el;

            i_cluster_pos = i_start_pos = cluster->GetElementPosition();
            ParseCluster( );

            ep->Down();
            /* stop pre-parsing the stream */
            break;
        }
        else if( MKV_IS_ID( el, KaxAttachments ) )
        {
            msg_Dbg( &sys.demuxer, "|   + Attachments" );
            if( i_attachments_position < 0 )
                ParseAttachments( static_cast<KaxAttachments*>( el ) );
            i_attachments_position = (int64_t) es.I_O().getFilePointer();
        }
        else if( MKV_IS_ID( el, KaxChapters ) )
        {
            msg_Dbg( &sys.demuxer, "|   + Chapters" );
            if( i_chapters_position < 0 )
                ParseChapters( static_cast<KaxChapters*>( el ) );
            i_chapters_position = (int64_t) es.I_O().getFilePointer();
        }
        else if( MKV_IS_ID( el, KaxTag ) )
        {
            msg_Dbg( &sys.demuxer, "|   + Tags" );
            if( i_tags_position < 0) // FIXME
                ;//LoadTags( static_cast<KaxTags*>( el ) );
            i_tags_position = (int64_t) es.I_O().getFilePointer();
        }
        else
            msg_Dbg( &sys.demuxer, "|   + Preload Unknown (%s)", typeid(*el).name() );
    }

    b_preloaded = true;

    return true;
}

/* Here we try to load elements that were found in Seek Heads, but not yet parsed */
bool matroska_segment_c::LoadSeekHeadItem( const EbmlCallbacks & ClassInfos, int64_t i_element_position )
{
    int64_t     i_sav_position = (int64_t)es.I_O().getFilePointer();
    EbmlElement *el;

    es.I_O().setFilePointer( i_element_position, seek_beginning );
    el = es.FindNextID( ClassInfos, 0xFFFFFFFFL);

    if( el == NULL )
    {
        msg_Err( &sys.demuxer, "cannot load some cues/chapters/tags etc. (broken seekhead or file)" );
        es.I_O().setFilePointer( i_sav_position, seek_beginning );
        return false;
    }

    if( MKV_IS_ID( el, KaxSeekHead ) )
    {
        /* Multiple allowed */
        msg_Dbg( &sys.demuxer, "|   + Seek head" );
        if( i_seekhead_count < 10 )
        {
            i_seekhead_position = i_element_position;
            ParseSeekHead( static_cast<KaxSeekHead*>( el ) );
        }
    }
    else if( MKV_IS_ID( el, KaxInfo ) ) // FIXME
    {
        /* Multiple allowed, mandatory */
        msg_Dbg( &sys.demuxer, "|   + Information" );
        if( i_info_position < 0 )
            ParseInfo( static_cast<KaxInfo*>( el ) );
        i_info_position = i_element_position;
    }
    else if( MKV_IS_ID( el, KaxTracks ) ) // FIXME
    {
        /* Multiple allowed */
        msg_Dbg( &sys.demuxer, "|   + Tracks" );
        if( i_tracks_position < 0 )
            ParseTracks( static_cast<KaxTracks*>( el ) );
        if ( tracks.size() == 0 )
        {
            msg_Err( &sys.demuxer, "No tracks supported" );
            delete el;
            es.I_O().setFilePointer( i_sav_position, seek_beginning );
            return false;
        }
        i_tracks_position = i_element_position;
    }
    else if( MKV_IS_ID( el, KaxCues ) )
    {
        msg_Dbg( &sys.demuxer, "|   + Cues" );
        if( i_cues_position < 0 )
            LoadCues( static_cast<KaxCues*>( el ) );
        i_cues_position = i_element_position;
    }
    else if( MKV_IS_ID( el, KaxAttachments ) )
    {
        msg_Dbg( &sys.demuxer, "|   + Attachments" );
        if( i_attachments_position < 0 )
            ParseAttachments( static_cast<KaxAttachments*>( el ) );
        i_attachments_position = i_element_position;
    }
    else if( MKV_IS_ID( el, KaxChapters ) )
    {
        msg_Dbg( &sys.demuxer, "|   + Chapters" );
        if( i_chapters_position < 0 )
            ParseChapters( static_cast<KaxChapters*>( el ) );
        i_chapters_position = i_element_position;
    }
    else if( MKV_IS_ID( el, KaxTag ) ) // FIXME
    {
        msg_Dbg( &sys.demuxer, "|   + Tags" );
        if( i_tags_position < 0 )
            ;//LoadTags( static_cast<KaxTags*>( el ) );
        i_tags_position = i_element_position;
    }
    else
    {
        msg_Dbg( &sys.demuxer, "|   + LoadSeekHeadItem Unknown (%s)", typeid(*el).name() );
    }
    delete el;

    es.I_O().setFilePointer( i_sav_position, seek_beginning );
    return true;
}

void matroska_segment_c::Seek( mtime_t i_date, mtime_t i_time_offset, int64_t i_global_position )
{
    KaxBlock    *block;
    KaxSimpleBlock *simpleblock;
    int         i_track_skipping;
    int64_t     i_block_duration;
    size_t      i_track;
    int64_t     i_seek_position = i_start_pos;
    int64_t     i_seek_time = i_start_time;

    if( i_global_position >= 0 )
    {
        /* Special case for seeking in files with no cues */
        EbmlElement *el = NULL;
        es.I_O().setFilePointer( i_start_pos, seek_beginning );
        delete ep;
        ep = new EbmlParser( &es, segment, &sys.demuxer );
        cluster = NULL;

        while( ( el = ep->Get() ) != NULL )
        {
            if( MKV_IS_ID( el, KaxCluster ) )
            {
                cluster = (KaxCluster *)el;
                i_cluster_pos = cluster->GetElementPosition();
                if( i_index == 0 ||
                        ( i_index > 0 && p_indexes[i_index - 1].i_position < (int64_t)cluster->GetElementPosition() ) )
                {
                    IndexAppendCluster( cluster );
                }
                if( es.I_O().getFilePointer() >= i_global_position )
                {
                    ParseCluster();
                    msg_Dbg( &sys.demuxer, "we found a cluster that is in the neighbourhood" );
                    return;
                }
            }
        }
        msg_Err( &sys.demuxer, "This file has no cues, and we were unable to seek to the requested position by parsing." );
        return;
    }

    if ( i_index > 0 )
    {
        int i_idx = 0;

        for( ; i_idx < i_index; i_idx++ )
        {
            if( p_indexes[i_idx].i_time + i_time_offset > i_date )
            {
                break;
            }
        }

        if( i_idx > 0 )
        {
            i_idx--;
        }

        i_seek_position = p_indexes[i_idx].i_position;
        i_seek_time = p_indexes[i_idx].i_time;
    }

    msg_Dbg( &sys.demuxer, "seek got %"PRId64" (%d%%)",
                i_seek_time, (int)( 100 * i_seek_position / stream_Size( sys.demuxer.s ) ) );

    es.I_O().setFilePointer( i_seek_position, seek_beginning );

    delete ep;
    ep = new EbmlParser( &es, segment, &sys.demuxer );
    cluster = NULL;

    sys.i_start_pts = i_date;

    /* now parse until key frame */
    i_track_skipping = 0;
    for( i_track = 0; i_track < tracks.size(); i_track++ )
    {
        if( tracks[i_track]->fmt.i_cat == VIDEO_ES )
        {
            tracks[i_track]->b_search_keyframe = true;
            i_track_skipping++;
        }
    }
    es_out_Control( sys.demuxer.out, ES_OUT_SET_NEXT_DISPLAY_TIME, i_date );

    while( i_track_skipping > 0 )
    {
        bool b_key_picture;
        bool b_discardable_picture;
        if( BlockGet( block, simpleblock, &b_key_picture, &b_discardable_picture, &i_block_duration ) )
        {
            msg_Warn( &sys.demuxer, "cannot get block EOF?" );

            return;
        }

        for( i_track = 0; i_track < tracks.size(); i_track++ )
        {
            if( (simpleblock && tracks[i_track]->i_number == simpleblock->TrackNum()) ||
                (block && tracks[i_track]->i_number == block->TrackNum()) )
            {
                break;
            }
        }

        if( simpleblock )
            sys.i_pts = (sys.i_chapter_time + simpleblock->GlobalTimecode()) / (mtime_t) 1000;
        else
            sys.i_pts = (sys.i_chapter_time + block->GlobalTimecode()) / (mtime_t) 1000;

        if( i_track < tracks.size() )
        {
            if( sys.i_pts > sys.i_start_pts )
            {
                cluster = static_cast<KaxCluster*>(ep->UnGet( i_block_pos, i_cluster_pos ));
                i_track_skipping = 0;
            }
            else if( tracks[i_track]->fmt.i_cat == VIDEO_ES )
            {
                if( b_key_picture && tracks[i_track]->b_search_keyframe )
                {
                    tracks[i_track]->b_search_keyframe = false;
                    i_track_skipping--;
                }
                if( !tracks[i_track]->b_search_keyframe )
                {
                    BlockDecode( &sys.demuxer, block, simpleblock, sys.i_pts, 0, b_key_picture || b_discardable_picture );
                }
            }
        }

        delete block;
    }

    /* FIXME current ES_OUT_SET_NEXT_DISPLAY_TIME does not work that well if
     * the delay is too high. */
    if( sys.i_pts + 500*1000 < sys.i_start_pts )
    {
        sys.i_start_pts = sys.i_pts;

        es_out_Control( sys.demuxer.out, ES_OUT_SET_NEXT_DISPLAY_TIME, sys.i_start_pts );
    }
}


int matroska_segment_c::BlockFindTrackIndex( size_t *pi_track,
                                             const KaxBlock *p_block, const KaxSimpleBlock *p_simpleblock )
{
    size_t          i_track;

    for( i_track = 0; i_track < tracks.size(); i_track++ )
    {
        const mkv_track_t *tk = tracks[i_track];

        if( ( p_block != NULL && tk->i_number == p_block->TrackNum() ) ||
            ( p_simpleblock != NULL && tk->i_number == p_simpleblock->TrackNum() ) )
        {
            break;
        }
    }

    if( i_track >= tracks.size() )
        return VLC_EGENERIC;

    if( pi_track )
        *pi_track = i_track;
    return VLC_SUCCESS;
}

bool matroska_segment_c::Select( mtime_t i_start_time )
{
    size_t i_track;

    /* add all es */
    msg_Dbg( &sys.demuxer, "found %d es", (int)tracks.size() );
    sys.b_pci_packet_set = false;

    for( i_track = 0; i_track < tracks.size(); i_track++ )
    {
        mkv_track_t *p_tk = tracks[i_track];
        es_format_t *p_fmt = &p_tk->fmt;

        if( p_fmt->i_cat == UNKNOWN_ES || !p_tk->psz_codec )
        {
            msg_Warn( &sys.demuxer, "invalid track[%d, n=%d]", (int)i_track, tracks[i_track]->i_number );
            tracks[i_track]->p_es = NULL;
            continue;
        }

        if( !strcmp( tracks[i_track]->psz_codec, "V_MS/VFW/FOURCC" ) )
        {
            if( tracks[i_track]->i_extra_data < (int)sizeof( BITMAPINFOHEADER ) )
            {
                msg_Err( &sys.demuxer, "missing/invalid BITMAPINFOHEADER" );
                tracks[i_track]->fmt.i_codec = VLC_FOURCC( 'u', 'n', 'd', 'f' );
            }
            else
            {
                BITMAPINFOHEADER *p_bih = (BITMAPINFOHEADER*)tracks[i_track]->p_extra_data;

                tracks[i_track]->fmt.video.i_width = GetDWLE( &p_bih->biWidth );
                tracks[i_track]->fmt.video.i_height= GetDWLE( &p_bih->biHeight );
                tracks[i_track]->fmt.i_codec       = GetFOURCC( &p_bih->biCompression );

                tracks[i_track]->fmt.i_extra       = GetDWLE( &p_bih->biSize ) - sizeof( BITMAPINFOHEADER );
                if( tracks[i_track]->fmt.i_extra > 0 )
                {
                    tracks[i_track]->fmt.p_extra = xmalloc( tracks[i_track]->fmt.i_extra );
                    memcpy( tracks[i_track]->fmt.p_extra, &p_bih[1], tracks[i_track]->fmt.i_extra );
                }
            }
            p_tk->b_dts_only = true;
        }
        else if( !strcmp( tracks[i_track]->psz_codec, "V_MPEG1" ) ||
                 !strcmp( tracks[i_track]->psz_codec, "V_MPEG2" ) )
        {
            tracks[i_track]->fmt.i_codec = VLC_CODEC_MPGV;
            if( tracks[i_track]->fmt.i_extra > 0 )
            {
                tracks[i_track]->fmt.p_extra = xmalloc( tracks[i_track]->i_extra_data );
                memcpy( tracks[i_track]->fmt.p_extra,tracks[i_track]->p_extra_data, tracks[i_track]->i_extra_data );
            }
        }
        else if( !strncmp( tracks[i_track]->psz_codec, "V_THEORA", 8 ) )
        {
            tracks[i_track]->fmt.i_codec = VLC_CODEC_THEORA;
            tracks[i_track]->fmt.i_extra = tracks[i_track]->i_extra_data;
            tracks[i_track]->fmt.p_extra = xmalloc( tracks[i_track]->i_extra_data );
            memcpy( tracks[i_track]->fmt.p_extra,tracks[i_track]->p_extra_data, tracks[i_track]->i_extra_data );
            tracks[i_track]->b_pts_only = true;
        }
        else if( !strncmp( tracks[i_track]->psz_codec, "V_REAL/RV", 9 ) )
        {
            if( !strcmp( p_tk->psz_codec, "V_REAL/RV10" ) )
                p_fmt->i_codec = VLC_CODEC_RV10;
            else if( !strcmp( p_tk->psz_codec, "V_REAL/RV20" ) )
                p_fmt->i_codec = VLC_CODEC_RV20;
            else if( !strcmp( p_tk->psz_codec, "V_REAL/RV30" ) )
                p_fmt->i_codec = VLC_CODEC_RV30;
            else if( !strcmp( p_tk->psz_codec, "V_REAL/RV40" ) )
                p_fmt->i_codec = VLC_CODEC_RV40;

            if( p_tk->i_extra_data > 26 )
            {
                p_fmt->p_extra = malloc( p_tk->i_extra_data - 26 );
                if( p_fmt->p_extra )
                {
                    p_fmt->i_extra = p_tk->i_extra_data - 26;
                    memcpy( p_fmt->p_extra, &p_tk->p_extra_data[26], p_fmt->i_extra );
                }
            }
            p_tk->b_dts_only = true;
        }
        else if( !strncmp( tracks[i_track]->psz_codec, "V_DIRAC", 7 ) )
        {
            tracks[i_track]->fmt.i_codec = VLC_CODEC_DIRAC;
        }
        else if( !strncmp( tracks[i_track]->psz_codec, "V_VP8", 5 ) )
        {
            tracks[i_track]->fmt.i_codec = VLC_CODEC_VP8;
            tracks[i_track]->b_pts_only = true;
        }
        else if( !strncmp( tracks[i_track]->psz_codec, "V_MPEG4", 7 ) )
        {
            if( !strcmp( tracks[i_track]->psz_codec, "V_MPEG4/MS/V3" ) )
            {
                tracks[i_track]->fmt.i_codec = VLC_CODEC_DIV3;
            }
            else if( !strncmp( tracks[i_track]->psz_codec, "V_MPEG4/ISO", 11 ) )
            {
                /* A MPEG 4 codec, SP, ASP, AP or AVC */
                if( !strcmp( tracks[i_track]->psz_codec, "V_MPEG4/ISO/AVC" ) )
                    tracks[i_track]->fmt.i_codec = VLC_FOURCC( 'a', 'v', 'c', '1' );
                else
                    tracks[i_track]->fmt.i_codec = VLC_CODEC_MP4V;
                tracks[i_track]->fmt.i_extra = tracks[i_track]->i_extra_data;
                tracks[i_track]->fmt.p_extra = xmalloc( tracks[i_track]->i_extra_data );
                memcpy( tracks[i_track]->fmt.p_extra,tracks[i_track]->p_extra_data, tracks[i_track]->i_extra_data );
            }
        }
        else if( !strcmp( tracks[i_track]->psz_codec, "V_QUICKTIME" ) )
        {
            MP4_Box_t *p_box = (MP4_Box_t*)xmalloc( sizeof( MP4_Box_t ) );
            stream_t *p_mp4_stream = stream_MemoryNew( VLC_OBJECT(&sys.demuxer),
                                                       tracks[i_track]->p_extra_data,
                                                       tracks[i_track]->i_extra_data,
                                                       true );
            if( MP4_ReadBoxCommon( p_mp4_stream, p_box ) &&
                MP4_ReadBox_sample_vide( p_mp4_stream, p_box ) )
            {
                tracks[i_track]->fmt.i_codec = p_box->i_type;
                tracks[i_track]->fmt.video.i_width = p_box->data.p_sample_vide->i_width;
                tracks[i_track]->fmt.video.i_height = p_box->data.p_sample_vide->i_height;
                tracks[i_track]->fmt.i_extra = p_box->data.p_sample_vide->i_qt_image_description;
                tracks[i_track]->fmt.p_extra = xmalloc( tracks[i_track]->fmt.i_extra );
                memcpy( tracks[i_track]->fmt.p_extra, p_box->data.p_sample_vide->p_qt_image_description, tracks[i_track]->fmt.i_extra );
                MP4_FreeBox_sample_vide( p_box );
            }
            else
            {
                free( p_box );
            }
            stream_Delete( p_mp4_stream );
        }
        else if( !strcmp( tracks[i_track]->psz_codec, "V_MJPEG" ) )
        {
            p_tk->fmt.i_codec = VLC_CODEC_MJPG;
        }
        else if( !strcmp( tracks[i_track]->psz_codec, "A_MS/ACM" ) )
        {
            if( tracks[i_track]->i_extra_data < (int)sizeof( WAVEFORMATEX ) )
            {
                msg_Err( &sys.demuxer, "missing/invalid WAVEFORMATEX" );
                tracks[i_track]->fmt.i_codec = VLC_FOURCC( 'u', 'n', 'd', 'f' );
            }
            else
            {
                WAVEFORMATEX *p_wf = (WAVEFORMATEX*)tracks[i_track]->p_extra_data;

                wf_tag_to_fourcc( GetWLE( &p_wf->wFormatTag ), &tracks[i_track]->fmt.i_codec, NULL );

                tracks[i_track]->fmt.audio.i_channels   = GetWLE( &p_wf->nChannels );
                tracks[i_track]->fmt.audio.i_rate = GetDWLE( &p_wf->nSamplesPerSec );
                tracks[i_track]->fmt.i_bitrate    = GetDWLE( &p_wf->nAvgBytesPerSec ) * 8;
                tracks[i_track]->fmt.audio.i_blockalign = GetWLE( &p_wf->nBlockAlign );;
                tracks[i_track]->fmt.audio.i_bitspersample = GetWLE( &p_wf->wBitsPerSample );

                tracks[i_track]->fmt.i_extra            = GetWLE( &p_wf->cbSize );
                if( tracks[i_track]->fmt.i_extra > 0 )
                {
                    tracks[i_track]->fmt.p_extra = xmalloc( tracks[i_track]->fmt.i_extra );
                    memcpy( tracks[i_track]->fmt.p_extra, &p_wf[1], tracks[i_track]->fmt.i_extra );
                }
            }
        }
        else if( !strcmp( tracks[i_track]->psz_codec, "A_MPEG/L3" ) ||
                 !strcmp( tracks[i_track]->psz_codec, "A_MPEG/L2" ) ||
                 !strcmp( tracks[i_track]->psz_codec, "A_MPEG/L1" ) )
        {
            tracks[i_track]->fmt.i_codec = VLC_CODEC_MPGA;
        }
        else if( !strcmp( tracks[i_track]->psz_codec, "A_AC3" ) )
        {
            tracks[i_track]->fmt.i_codec = VLC_CODEC_A52;
        }
        else if( !strcmp( tracks[i_track]->psz_codec, "A_EAC3" ) )
        {
            tracks[i_track]->fmt.i_codec = VLC_CODEC_EAC3;
        }
        else if( !strcmp( tracks[i_track]->psz_codec, "A_DTS" ) )
        {
            tracks[i_track]->fmt.i_codec = VLC_CODEC_DTS;
        }
        else if( !strcmp( tracks[i_track]->psz_codec, "A_MLP" ) )
        {
            tracks[i_track]->fmt.i_codec = VLC_CODEC_MLP;
        }
        else if( !strcmp( tracks[i_track]->psz_codec, "A_TRUEHD" ) )
        {
            /* FIXME when more samples arrive */
            tracks[i_track]->fmt.i_codec = VLC_CODEC_TRUEHD;
            p_fmt->b_packetized = false;
        }
        else if( !strcmp( tracks[i_track]->psz_codec, "A_FLAC" ) )
        {
            tracks[i_track]->fmt.i_codec = VLC_CODEC_FLAC;
            tracks[i_track]->fmt.i_extra = tracks[i_track]->i_extra_data;
            tracks[i_track]->fmt.p_extra = xmalloc( tracks[i_track]->i_extra_data );
            memcpy( tracks[i_track]->fmt.p_extra,tracks[i_track]->p_extra_data, tracks[i_track]->i_extra_data );
        }
        else if( !strcmp( tracks[i_track]->psz_codec, "A_VORBIS" ) )
        {
            tracks[i_track]->fmt.i_codec = VLC_CODEC_VORBIS;
            tracks[i_track]->fmt.i_extra = tracks[i_track]->i_extra_data;
            tracks[i_track]->fmt.p_extra = xmalloc( tracks[i_track]->i_extra_data );
            memcpy( tracks[i_track]->fmt.p_extra,tracks[i_track]->p_extra_data, tracks[i_track]->i_extra_data );
        }
        else if( !strncmp( tracks[i_track]->psz_codec, "A_AAC/MPEG2/", strlen( "A_AAC/MPEG2/" ) ) ||
                 !strncmp( tracks[i_track]->psz_codec, "A_AAC/MPEG4/", strlen( "A_AAC/MPEG4/" ) ) )
        {
            int i_profile, i_srate, sbr = 0;
            static const unsigned int i_sample_rates[] =
            {
                    96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050,
                        16000, 12000, 11025, 8000,  7350,  0,     0,     0
            };

            tracks[i_track]->fmt.i_codec = VLC_CODEC_MP4A;
            /* create data for faad (MP4DecSpecificDescrTag)*/

            if( !strcmp( &tracks[i_track]->psz_codec[12], "MAIN" ) )
            {
                i_profile = 0;
            }
            else if( !strcmp( &tracks[i_track]->psz_codec[12], "LC" ) )
            {
                i_profile = 1;
            }
            else if( !strcmp( &tracks[i_track]->psz_codec[12], "SSR" ) )
            {
                i_profile = 2;
            }
            else if( !strcmp( &tracks[i_track]->psz_codec[12], "LC/SBR" ) )
            {
                i_profile = 1;
                sbr = 1;
            }
            else
            {
                i_profile = 3;
            }

            for( i_srate = 0; i_srate < 13; i_srate++ )
            {
                if( i_sample_rates[i_srate] == tracks[i_track]->i_original_rate )
                {
                    break;
                }
            }
            msg_Dbg( &sys.demuxer, "profile=%d srate=%d", i_profile, i_srate );

            tracks[i_track]->fmt.i_extra = sbr ? 5 : 2;
            tracks[i_track]->fmt.p_extra = xmalloc( tracks[i_track]->fmt.i_extra );
            ((uint8_t*)tracks[i_track]->fmt.p_extra)[0] = ((i_profile + 1) << 3) | ((i_srate&0xe) >> 1);
            ((uint8_t*)tracks[i_track]->fmt.p_extra)[1] = ((i_srate & 0x1) << 7) | (tracks[i_track]->fmt.audio.i_channels << 3);
            if (sbr != 0)
            {
                int syncExtensionType = 0x2B7;
                int iDSRI;
                for (iDSRI=0; iDSRI<13; iDSRI++)
                    if( i_sample_rates[iDSRI] == tracks[i_track]->fmt.audio.i_rate )
                        break;
                ((uint8_t*)tracks[i_track]->fmt.p_extra)[2] = (syncExtensionType >> 3) & 0xFF;
                ((uint8_t*)tracks[i_track]->fmt.p_extra)[3] = ((syncExtensionType & 0x7) << 5) | 5;
                ((uint8_t*)tracks[i_track]->fmt.p_extra)[4] = ((1 & 0x1) << 7) | (iDSRI << 3);
            }
        }
        else if( !strcmp( tracks[i_track]->psz_codec, "A_AAC" ) )
        {
            tracks[i_track]->fmt.i_codec = VLC_CODEC_MP4A;
            tracks[i_track]->fmt.i_extra = tracks[i_track]->i_extra_data;
            tracks[i_track]->fmt.p_extra = xmalloc( tracks[i_track]->i_extra_data );
            memcpy( tracks[i_track]->fmt.p_extra, tracks[i_track]->p_extra_data, tracks[i_track]->i_extra_data );
        }
        else if( !strcmp( tracks[i_track]->psz_codec, "A_WAVPACK4" ) )
        {
            tracks[i_track]->fmt.i_codec = VLC_CODEC_WAVPACK;
            tracks[i_track]->fmt.i_extra = tracks[i_track]->i_extra_data;
            tracks[i_track]->fmt.p_extra = xmalloc( tracks[i_track]->i_extra_data );
            memcpy( tracks[i_track]->fmt.p_extra, tracks[i_track]->p_extra_data, tracks[i_track]->i_extra_data );
        }
        else if( !strcmp( tracks[i_track]->psz_codec, "A_TTA1" ) )
        {
            p_fmt->i_codec = VLC_CODEC_TTA;
            p_fmt->i_extra = p_tk->i_extra_data;
            if( p_fmt->i_extra > 0 )
            {
                p_fmt->p_extra = xmalloc( p_tk->i_extra_data );
                memcpy( p_fmt->p_extra, p_tk->p_extra_data, p_tk->i_extra_data );
            }
            else
            {
                p_fmt->i_extra = 30;
                p_fmt->p_extra = xmalloc( p_fmt->i_extra );
                uint8_t *p_extra = (uint8_t*)p_fmt->p_extra;
                memcpy( &p_extra[ 0], "TTA1", 4 );
                SetWLE( &p_extra[ 4], 1 );
                SetWLE( &p_extra[ 6], p_fmt->audio.i_channels );
                SetWLE( &p_extra[ 8], p_fmt->audio.i_bitspersample );
                SetDWLE( &p_extra[10], p_fmt->audio.i_rate );
                SetDWLE( &p_extra[14], 0xffffffff );
                memset( &p_extra[18], 0, 30  - 18 );
            }
        }
        else if( !strcmp( tracks[i_track]->psz_codec, "A_PCM/INT/BIG" ) ||
                 !strcmp( tracks[i_track]->psz_codec, "A_PCM/INT/LIT" ) ||
                 !strcmp( tracks[i_track]->psz_codec, "A_PCM/FLOAT/IEEE" ) )
        {
            if( !strcmp( tracks[i_track]->psz_codec, "A_PCM/INT/BIG" ) )
            {
                tracks[i_track]->fmt.i_codec = VLC_FOURCC( 't', 'w', 'o', 's' );
            }
            else
            {
                tracks[i_track]->fmt.i_codec = VLC_FOURCC( 'a', 'r', 'a', 'w' );
            }
            tracks[i_track]->fmt.audio.i_blockalign = ( tracks[i_track]->fmt.audio.i_bitspersample + 7 ) / 8 * tracks[i_track]->fmt.audio.i_channels;
        }
        else if( !strncmp( tracks[i_track]->psz_codec, "A_REAL/", 7 ) )
        {
            if( !strcmp( tracks[i_track]->psz_codec, "A_REAL/COOK" ) )
                tracks[i_track]->fmt.i_codec = VLC_CODEC_COOK;
            else if( !strcmp( tracks[i_track]->psz_codec, "A_REAL/ATRC" ) )
                tracks[i_track]->fmt.i_codec = VLC_CODEC_ATRAC3;
            else if( !strcmp( tracks[i_track]->psz_codec, "A_REAL/28_8" ) )
                tracks[i_track]->fmt.i_codec = VLC_CODEC_RA_288;
            /* FIXME 14_4, RALF and SIPR */
            tracks[i_track]->fmt.i_extra = tracks[i_track]->i_extra_data;
            tracks[i_track]->fmt.p_extra = xmalloc( tracks[i_track]->i_extra_data );
            memcpy( tracks[i_track]->fmt.p_extra,tracks[i_track]->p_extra_data, tracks[i_track]->i_extra_data );
        }
        /* disabled due to the potential "S_KATE" namespace issue */
        else if( !strcmp( tracks[i_track]->psz_codec, "S_KATE" ) )
        {
            int i, i_offset = 1, i_extra, num_headers, size_so_far;
            uint8_t *p_extra;

            tracks[i_track]->fmt.i_codec = VLC_CODEC_KATE;
            tracks[i_track]->fmt.subs.psz_encoding = strdup( "UTF-8" );

            tracks[i_track]->fmt.i_extra = tracks[i_track]->i_extra_data;
            tracks[i_track]->fmt.p_extra = xmalloc( tracks[i_track]->i_extra_data );
            memcpy( tracks[i_track]->fmt.p_extra,tracks[i_track]->p_extra_data, tracks[i_track]->i_extra_data );
        }
        else if( !strcmp( tracks[i_track]->psz_codec, "S_TEXT/ASCII" ) )
        {
            p_fmt->i_codec = VLC_CODEC_SUBT;
            p_fmt->subs.psz_encoding = NULL; /* Is there a place where it is stored ? */
        }
        else if( !strcmp( tracks[i_track]->psz_codec, "S_TEXT/UTF8" ) )
        {
            tracks[i_track]->fmt.i_codec = VLC_CODEC_SUBT;
            tracks[i_track]->fmt.subs.psz_encoding = strdup( "UTF-8" );
        }
        else if( !strcmp( tracks[i_track]->psz_codec, "S_TEXT/USF" ) )
        {
            tracks[i_track]->fmt.i_codec = VLC_FOURCC( 'u', 's', 'f', ' ' );
            tracks[i_track]->fmt.subs.psz_encoding = strdup( "UTF-8" );
            if( tracks[i_track]->i_extra_data )
            {
                tracks[i_track]->fmt.i_extra = tracks[i_track]->i_extra_data;
                tracks[i_track]->fmt.p_extra = xmalloc( tracks[i_track]->i_extra_data );
                memcpy( tracks[i_track]->fmt.p_extra, tracks[i_track]->p_extra_data, tracks[i_track]->i_extra_data );
            }
        }
        else if( !strcmp( tracks[i_track]->psz_codec, "S_TEXT/SSA" ) ||
                 !strcmp( tracks[i_track]->psz_codec, "S_TEXT/ASS" ) ||
                 !strcmp( tracks[i_track]->psz_codec, "S_SSA" ) ||
                 !strcmp( tracks[i_track]->psz_codec, "S_ASS" ))
        {
            tracks[i_track]->fmt.i_codec = VLC_CODEC_SSA;
            tracks[i_track]->fmt.subs.psz_encoding = strdup( "UTF-8" );
            if( tracks[i_track]->i_extra_data )
            {
                tracks[i_track]->fmt.i_extra = tracks[i_track]->i_extra_data;
                tracks[i_track]->fmt.p_extra = xmalloc( tracks[i_track]->i_extra_data );
                memcpy( tracks[i_track]->fmt.p_extra, tracks[i_track]->p_extra_data, tracks[i_track]->i_extra_data );
            }
        }
        else if( !strcmp( tracks[i_track]->psz_codec, "S_VOBSUB" ) )
        {
            tracks[i_track]->fmt.i_codec = VLC_CODEC_SPU;
            if( tracks[i_track]->i_extra_data )
            {
                char *psz_start;
                char *psz_buf = (char *)malloc( tracks[i_track]->i_extra_data + 1);
                if( psz_buf != NULL )
                {
                    memcpy( psz_buf, tracks[i_track]->p_extra_data , tracks[i_track]->i_extra_data );
                    psz_buf[tracks[i_track]->i_extra_data] = '\0';

                    psz_start = strstr( psz_buf, "size:" );
                    if( psz_start &&
                        vobsub_size_parse( psz_start,
                                           &tracks[i_track]->fmt.subs.spu.i_original_frame_width,
                                           &tracks[i_track]->fmt.subs.spu.i_original_frame_height ) == VLC_SUCCESS )
                    {
                        msg_Dbg( &sys.demuxer, "original frame size vobsubs: %dx%d",
                                 tracks[i_track]->fmt.subs.spu.i_original_frame_width,
                                 tracks[i_track]->fmt.subs.spu.i_original_frame_height );
                    }
                    else
                    {
                        msg_Warn( &sys.demuxer, "reading original frame size for vobsub failed" );
                    }

                    psz_start = strstr( psz_buf, "palette:" );
                    if( psz_start &&
                        vobsub_palette_parse( psz_start, &tracks[i_track]->fmt.subs.spu.palette[1] ) == VLC_SUCCESS )
                    {
                        tracks[i_track]->fmt.subs.spu.palette[0] =  0xBeef;
                        msg_Dbg( &sys.demuxer, "vobsub palette read" );
                    }
                    else
                    {
                        msg_Warn( &sys.demuxer, "reading original palette failed" );
                    }
                    free( psz_buf );
                }
            }
        }
        else if( !strcmp( tracks[i_track]->psz_codec, "S_HDMV/PGS" ) )
        {
            tracks[i_track]->fmt.i_codec = VLC_CODEC_BD_PG;
        }
        else if( !strcmp( tracks[i_track]->psz_codec, "B_VOBBTN" ) )
        {
            tracks[i_track]->fmt.i_cat = NAV_ES;
            continue;
        }
        else if( !strcmp( p_tk->psz_codec, "A_REAL/14_4" ) )
        {
            p_fmt->i_codec = VLC_CODEC_RA_144;
            p_fmt->audio.i_channels = 1;
            p_fmt->audio.i_rate = 8000;
            p_fmt->audio.i_blockalign = 0x14;
        }
        else
        {
            msg_Err( &sys.demuxer, "unknown codec id=`%s'", tracks[i_track]->psz_codec );
            tracks[i_track]->fmt.i_codec = VLC_FOURCC( 'u', 'n', 'd', 'f' );
        }
        if( tracks[i_track]->b_default )
        {
            tracks[i_track]->fmt.i_priority = 1000;
        }

        tracks[i_track]->p_es = es_out_Add( sys.demuxer.out, &tracks[i_track]->fmt );

        /* Turn on a subtitles track if it has been flagged as default -
         * but only do this if no subtitles track has already been engaged,
         * either by an earlier 'default track' (??) or by default
         * language choice behaviour.
         */
        if( tracks[i_track]->b_default )
        {
            es_out_Control( sys.demuxer.out,
                            ES_OUT_SET_ES_DEFAULT,
                            tracks[i_track]->p_es );
        }
    }
    es_out_Control( sys.demuxer.out, ES_OUT_SET_NEXT_DISPLAY_TIME, i_start_time );
 
    sys.i_start_pts = i_start_time;
    // reset the stream reading to the first cluster of the segment used
    es.I_O().setFilePointer( i_start_pos );

    delete ep;
    ep = new EbmlParser( &es, segment, &sys.demuxer );

    return true;
}

void matroska_segment_c::UnSelect( )
{
    size_t i_track;

    for( i_track = 0; i_track < tracks.size(); i_track++ )
    {
        if ( tracks[i_track]->p_es != NULL )
        {
//            es_format_Clean( &tracks[i_track]->fmt );
            es_out_Del( sys.demuxer.out, tracks[i_track]->p_es );
            tracks[i_track]->p_es = NULL;
        }
    }
    delete ep;
    ep = NULL;
}

int matroska_segment_c::BlockGet( KaxBlock * & pp_block, KaxSimpleBlock * & pp_simpleblock, bool *pb_key_picture, bool *pb_discardable_picture, int64_t *pi_duration )
{
    pp_simpleblock = NULL;
    pp_block = NULL;

    *pb_key_picture         = true;
    *pb_discardable_picture = false;

    for( ;; )
    {
        EbmlElement *el = NULL;
        int         i_level;

        if ( ep == NULL )
            return VLC_EGENERIC;

        if( pp_simpleblock != NULL || ((el = ep->Get()) == NULL && pp_block != NULL) )
        {
            /* Check blocks validity to protect againts broken files */
            if( BlockFindTrackIndex( NULL, pp_block , pp_simpleblock ) )
            {
                delete pp_block;
                pp_simpleblock = NULL;
                pp_block = NULL;
                continue;
            }
            if( pp_simpleblock != NULL )
            {
                *pb_key_picture         = pp_simpleblock->IsKeyframe();
                *pb_discardable_picture = pp_simpleblock->IsDiscardable();
            }

            /* update the index */
#define idx p_indexes[i_index - 1]
            if( i_index > 0 && idx.i_time == -1 )
            {
                if ( pp_simpleblock != NULL )
                    idx.i_time        = pp_simpleblock->GlobalTimecode() / (mtime_t)1000;
                else
                    idx.i_time        = (*pp_block).GlobalTimecode() / (mtime_t)1000;
                idx.b_key         = *pb_key_picture;
            }
#undef idx
            return VLC_SUCCESS;
        }

        i_level = ep->GetLevel();

        if( el == NULL )
        {
            if( i_level > 1 )
            {
                ep->Up();
                continue;
            }
            msg_Warn( &sys.demuxer, "EOF" );
            return VLC_EGENERIC;
        }

        /* Verify that we are still inside our cluster
         * It can happens whith broken files and when seeking
         * without index */
        if( i_level > 1 )
        {
            if( cluster && !ep->IsTopPresent( cluster ) )
            {
                msg_Warn( &sys.demuxer, "Unexpected escape from current cluster" );
                cluster = NULL;
            }
            if( !cluster )
                continue;
        }

        /* do parsing */
        switch ( i_level )
        {
        case 1:
            if( MKV_IS_ID( el, KaxCluster ) )
            {
                cluster = (KaxCluster*)el;
                i_cluster_pos = cluster->GetElementPosition();

                /* add it to the index */
                if( i_index == 0 ||
                    ( i_index > 0 && p_indexes[i_index - 1].i_position < (int64_t)cluster->GetElementPosition() ) )
                {
                    IndexAppendCluster( cluster );
                }

                // reset silent tracks
                for (size_t i=0; i<tracks.size(); i++)
                {
                    tracks[i]->b_silent = false;
                }

                ep->Down();
            }
            else if( MKV_IS_ID( el, KaxCues ) )
            {
                msg_Warn( &sys.demuxer, "find KaxCues FIXME" );
                return VLC_EGENERIC;
            }
            else
            {
                msg_Dbg( &sys.demuxer, "unknown (%s)", typeid( el ).name() );
            }
            break;
        case 2:
            if( MKV_IS_ID( el, KaxClusterTimecode ) )
            {
                KaxClusterTimecode &ctc = *(KaxClusterTimecode*)el;

                ctc.ReadData( es.I_O(), SCOPE_ALL_DATA );
                cluster->InitTimecode( uint64( ctc ), i_timescale );
            }
            else if( MKV_IS_ID( el, KaxClusterSilentTracks ) )
            {
                ep->Down();
            }
            else if( MKV_IS_ID( el, KaxBlockGroup ) )
            {
                i_block_pos = el->GetElementPosition();
                ep->Down();
            }
            else if( MKV_IS_ID( el, KaxSimpleBlock ) )
            {
                pp_simpleblock = (KaxSimpleBlock*)el;

                pp_simpleblock->ReadData( es.I_O() );
                pp_simpleblock->SetParent( *cluster );
            }
            break;
        case 3:
            if( MKV_IS_ID( el, KaxBlock ) )
            {
                pp_block = (KaxBlock*)el;

                pp_block->ReadData( es.I_O() );
                pp_block->SetParent( *cluster );

                ep->Keep();
            }
            else if( MKV_IS_ID( el, KaxBlockDuration ) )
            {
                KaxBlockDuration &dur = *(KaxBlockDuration*)el;

                dur.ReadData( es.I_O() );
                *pi_duration = uint64( dur );
            }
            else if( MKV_IS_ID( el, KaxReferenceBlock ) )
            {
                KaxReferenceBlock &ref = *(KaxReferenceBlock*)el;

                ref.ReadData( es.I_O() );

                if( *pb_key_picture )
                    *pb_key_picture = false;
                else if( int64( ref ) > 0 )
                    *pb_discardable_picture = true;
            }
            else if( MKV_IS_ID( el, KaxClusterSilentTrackNumber ) )
            {
                KaxClusterSilentTrackNumber &track_num = *(KaxClusterSilentTrackNumber*)el;
                track_num.ReadData( es.I_O() );
                // find the track
                for (size_t i=0; i<tracks.size(); i++)
                {
                    if ( tracks[i]->i_number == uint32(track_num))
                    {
                        tracks[i]->b_silent = true;
                        break;
                    }
                }
            }
            break;
        default:
            msg_Err( &sys.demuxer, "invalid level = %d", i_level );
            return VLC_EGENERIC;
        }
    }
}

