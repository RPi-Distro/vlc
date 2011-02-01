
/*****************************************************************************
 * mkv.cpp : matroska demuxer
 *****************************************************************************
 * Copyright (C) 2003-2004 the VideoLAN team
 * $Id: dfa1ca613c4f506ad5bbb81609af8615bfc8eeab $
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

#include "demux.hpp"

#include "Ebml_parser.hpp"

demux_sys_t::~demux_sys_t()
{
    StopUiThread();
    size_t i;
    for ( i=0; i<streams.size(); i++ )
        delete streams[i];
    for ( i=0; i<opened_segments.size(); i++ )
        delete opened_segments[i];
    for ( i=0; i<used_segments.size(); i++ )
        delete used_segments[i];
    for ( i=0; i<stored_attachments.size(); i++ )
        delete stored_attachments[i];
    if( meta ) vlc_meta_Delete( meta );

    while( titles.size() )
    { vlc_input_title_Delete( titles.back() ); titles.pop_back();}

    vlc_mutex_destroy( &lock_demuxer );
}


matroska_stream_c *demux_sys_t::AnalyseAllSegmentsFound( demux_t *p_demux, EbmlStream *p_estream, bool b_initial )
{
    int i_upper_lvl = 0;
    size_t i;
    EbmlElement *p_l0, *p_l1, *p_l2;
    bool b_keep_stream = false, b_keep_segment;

    // verify the EBML Header
    p_l0 = p_estream->FindNextID(EbmlHead::ClassInfos, 0xFFFFFFFFL);
    if (p_l0 == NULL)
    {
        msg_Err( p_demux, "No EBML header found" );
        return NULL;
    }

    // verify we can read this Segment, we only support Matroska version 1 for now
    p_l0->Read(*p_estream, EbmlHead::ClassInfos.Context, i_upper_lvl, p_l0, true);

    EDocType doc_type = GetChild<EDocType>(*static_cast<EbmlHead*>(p_l0));
    if (std::string(doc_type) != "matroska" && std::string(doc_type) != "webm" )
    {
        msg_Err( p_demux, "Not a Matroska file : DocType = %s ", std::string(doc_type).c_str());
        return NULL;
    }

    EDocTypeReadVersion doc_read_version = GetChild<EDocTypeReadVersion>(*static_cast<EbmlHead*>(p_l0));
    if (uint64(doc_read_version) > 2)
    {
        msg_Err( p_demux, "This matroska file is needs version %"PRId64" and this VLC only supports version 1 & 2", uint64(doc_read_version));
        return NULL;
    }

    delete p_l0;


    // find all segments in this file
    p_l0 = p_estream->FindNextID(KaxSegment::ClassInfos, 0xFFFFFFFFFLL);
    if (p_l0 == NULL)
    {
        return NULL;
    }

    matroska_stream_c *p_stream1 = new matroska_stream_c( *this );

    while (p_l0 != 0)
    {
        if ( MKV_IS_ID( p_l0, KaxSegment) )
        {
            EbmlParser  *ep;
            matroska_segment_c *p_segment1 = new matroska_segment_c( *this, *p_estream );
            b_keep_segment = b_initial;

            ep = new EbmlParser(p_estream, p_l0, &demuxer );
            p_segment1->ep = ep;
            p_segment1->segment = (KaxSegment*)p_l0;

            while ((p_l1 = ep->Get()))
            {
                if (MKV_IS_ID(p_l1, KaxInfo))
                {
                    // find the families of this segment
                    KaxInfo *p_info = static_cast<KaxInfo*>(p_l1);

                    p_info->Read(*p_estream, KaxInfo::ClassInfos.Context, i_upper_lvl, p_l2, true);
                    for( i = 0; i < p_info->ListSize(); i++ )
                    {
                        EbmlElement *l = (*p_info)[i];

                        if( MKV_IS_ID( l, KaxSegmentUID ) )
                        {
                            KaxSegmentUID *p_uid = static_cast<KaxSegmentUID*>(l);
                            b_keep_segment = (FindSegment( *p_uid ) == NULL);
                            if ( !b_keep_segment )
                                break; // this segment is already known
                            opened_segments.push_back( p_segment1 );
                            delete p_segment1->p_segment_uid;
                            p_segment1->p_segment_uid = new KaxSegmentUID(*p_uid);
                        }
                        else if( MKV_IS_ID( l, KaxPrevUID ) )
                        {
                            p_segment1->p_prev_segment_uid = new KaxPrevUID( *static_cast<KaxPrevUID*>(l) );
                        }
                        else if( MKV_IS_ID( l, KaxNextUID ) )
                        {
                            p_segment1->p_next_segment_uid = new KaxNextUID( *static_cast<KaxNextUID*>(l) );
                        }
                        else if( MKV_IS_ID( l, KaxSegmentFamily ) )
                        {
                            KaxSegmentFamily *p_fam = new KaxSegmentFamily( *static_cast<KaxSegmentFamily*>(l) );
                            p_segment1->families.push_back( p_fam );
                        }
                    }
                    break;
                }
            }
            if ( b_keep_segment )
            {
                b_keep_stream = true;
                p_stream1->segments.push_back( p_segment1 );
            }
            else
            {
                p_segment1->segment = NULL;
                delete p_segment1;
            }
        }
        if (p_l0->IsFiniteSize() )
        {
            p_l0->SkipData(*p_estream, KaxMatroska_Context);
            p_l0 = p_estream->FindNextID(KaxSegment::ClassInfos, 0xFFFFFFFFL);
        }
        else
        {
            p_l0 = NULL;
        }
    }

    if ( !b_keep_stream )
    {
        delete p_stream1;
        p_stream1 = NULL;
    }

    return p_stream1;
}

void demux_sys_t::StartUiThread()
{
    if ( !b_ui_hooked )
    {
        msg_Dbg( &demuxer, "Starting the UI Hook" );
        b_ui_hooked = true;
        /* FIXME hack hack hack hack FIXME */
        /* Get p_input and create variable */
        p_input = (input_thread_t *) vlc_object_find( &demuxer, VLC_OBJECT_INPUT, FIND_PARENT );
        var_Create( p_input, "x-start", VLC_VAR_INTEGER );
        var_Create( p_input, "y-start", VLC_VAR_INTEGER );
        var_Create( p_input, "x-end", VLC_VAR_INTEGER );
        var_Create( p_input, "y-end", VLC_VAR_INTEGER );
        var_Create( p_input, "color", VLC_VAR_ADDRESS );
        var_Create( p_input, "menu-palette", VLC_VAR_ADDRESS );
        var_Create( p_input, "highlight", VLC_VAR_BOOL );
        var_Create( p_input, "highlight-mutex", VLC_VAR_MUTEX );

        /* Now create our event thread catcher */
        p_ev = (event_thread_t *) vlc_object_create( &demuxer, sizeof( event_thread_t ) );
        p_ev->p_demux = &demuxer;
        p_ev->b_die = false;
        vlc_mutex_init( &p_ev->lock );
        vlc_thread_create( p_ev, "mkv event thread handler", EventThread,
                        VLC_THREAD_PRIORITY_LOW );
    }
}

void demux_sys_t::StopUiThread()
{
    if ( b_ui_hooked )
    {
        vlc_object_kill( p_ev );
        vlc_thread_join( p_ev );
        vlc_object_release( p_ev );

        p_ev = NULL;

        var_Destroy( p_input, "highlight-mutex" );
        var_Destroy( p_input, "highlight" );
        var_Destroy( p_input, "x-start" );
        var_Destroy( p_input, "x-end" );
        var_Destroy( p_input, "y-start" );
        var_Destroy( p_input, "y-end" );
        var_Destroy( p_input, "color" );
        var_Destroy( p_input, "menu-palette" );

        vlc_object_release( p_input );

        msg_Dbg( &demuxer, "Stopping the UI Hook" );
    }
    b_ui_hooked = false;
}

int demux_sys_t::EventMouse( vlc_object_t *p_this, char const *psz_var,
                       vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    event_thread_t *p_ev = (event_thread_t *) p_data;
    vlc_mutex_lock( &p_ev->lock );
    if( psz_var[6] == 'c' )
    {
        p_ev->b_clicked = true;
        msg_Dbg( p_this, "Event Mouse: clicked");
    }
    else if( psz_var[6] == 'm' )
        p_ev->b_moved = true;
    vlc_mutex_unlock( &p_ev->lock );

    return VLC_SUCCESS;
}

int demux_sys_t::EventKey( vlc_object_t *p_this, char const *,
                           vlc_value_t, vlc_value_t newval, void *p_data )
{
    event_thread_t *p_ev = (event_thread_t *) p_data;
    vlc_mutex_lock( &p_ev->lock );
    p_ev->i_key_action = newval.i_int;
    vlc_mutex_unlock( &p_ev->lock );
    msg_Dbg( p_this, "Event Key");

    return VLC_SUCCESS;
}

void * demux_sys_t::EventThread( vlc_object_t *p_this )
{
    event_thread_t *p_ev = (event_thread_t*)p_this;
    demux_sys_t    *p_sys = p_ev->p_demux->p_sys;
    vlc_object_t   *p_vout = NULL;
    int canc = vlc_savecancel ();

    p_ev->b_moved   = false;
    p_ev->b_clicked = false;
    p_ev->i_key_action = 0;

    /* catch all key event */
    var_AddCallback( p_ev->p_libvlc, "key-action", EventKey, p_ev );

    /* main loop */
    while( vlc_object_alive (p_ev) )
    {
        if ( !p_sys->b_pci_packet_set )
        {
            /* Wait 100ms */
            msleep( 100000 );
            continue;
        }

        bool b_activated = false;

        /* KEY part */
        if( p_ev->i_key_action )
        {
            msg_Dbg( p_ev->p_demux, "Handle Key Event");

            vlc_mutex_lock( &p_ev->lock );

            pci_t *pci = (pci_t *) &p_sys->pci_packet;

            uint16 i_curr_button = p_sys->dvd_interpretor.GetSPRM( 0x88 );

            switch( p_ev->i_key_action )
            {
            case ACTIONID_NAV_LEFT:
                if ( i_curr_button > 0 && i_curr_button <= pci->hli.hl_gi.btn_ns )
                {
                    btni_t *p_button_ptr = &(pci->hli.btnit[i_curr_button-1]);
                    if ( p_button_ptr->left > 0 && p_button_ptr->left <= pci->hli.hl_gi.btn_ns )
                    {
                        i_curr_button = p_button_ptr->left;
                        p_sys->dvd_interpretor.SetSPRM( 0x88, i_curr_button );
                        btni_t button_ptr = pci->hli.btnit[i_curr_button-1];
                        if ( button_ptr.auto_action_mode )
                        {
                            vlc_mutex_unlock( &p_ev->lock );
                            vlc_mutex_lock( &p_sys->lock_demuxer );

                            // process the button action
                            p_sys->dvd_interpretor.Interpret( button_ptr.cmd.bytes, 8 );

                            vlc_mutex_unlock( &p_sys->lock_demuxer );
                            vlc_mutex_lock( &p_ev->lock );
                        }
                    }
                }
                break;
            case ACTIONID_NAV_RIGHT:
                if ( i_curr_button > 0 && i_curr_button <= pci->hli.hl_gi.btn_ns )
                {
                    btni_t *p_button_ptr = &(pci->hli.btnit[i_curr_button-1]);
                    if ( p_button_ptr->right > 0 && p_button_ptr->right <= pci->hli.hl_gi.btn_ns )
                    {
                        i_curr_button = p_button_ptr->right;
                        p_sys->dvd_interpretor.SetSPRM( 0x88, i_curr_button );
                        btni_t button_ptr = pci->hli.btnit[i_curr_button-1];
                        if ( button_ptr.auto_action_mode )
                        {
                            vlc_mutex_unlock( &p_ev->lock );
                            vlc_mutex_lock( &p_sys->lock_demuxer );

                            // process the button action
                            p_sys->dvd_interpretor.Interpret( button_ptr.cmd.bytes, 8 );

                            vlc_mutex_unlock( &p_sys->lock_demuxer );
                            vlc_mutex_lock( &p_ev->lock );
                        }
                    }
                }
                break;
            case ACTIONID_NAV_UP:
                if ( i_curr_button > 0 && i_curr_button <= pci->hli.hl_gi.btn_ns )
                {
                    btni_t *p_button_ptr = &(pci->hli.btnit[i_curr_button-1]);
                    if ( p_button_ptr->up > 0 && p_button_ptr->up <= pci->hli.hl_gi.btn_ns )
                    {
                        i_curr_button = p_button_ptr->up;
                        p_sys->dvd_interpretor.SetSPRM( 0x88, i_curr_button );
                        btni_t button_ptr = pci->hli.btnit[i_curr_button-1];
                        if ( button_ptr.auto_action_mode )
                        {
                            vlc_mutex_unlock( &p_ev->lock );
                            vlc_mutex_lock( &p_sys->lock_demuxer );

                            // process the button action
                            p_sys->dvd_interpretor.Interpret( button_ptr.cmd.bytes, 8 );

                            vlc_mutex_unlock( &p_sys->lock_demuxer );
                            vlc_mutex_lock( &p_ev->lock );
                        }
                    }
                }
                break;
            case ACTIONID_NAV_DOWN:
                if ( i_curr_button > 0 && i_curr_button <= pci->hli.hl_gi.btn_ns )
                {
                    btni_t *p_button_ptr = &(pci->hli.btnit[i_curr_button-1]);
                    if ( p_button_ptr->down > 0 && p_button_ptr->down <= pci->hli.hl_gi.btn_ns )
                    {
                        i_curr_button = p_button_ptr->down;
                        p_sys->dvd_interpretor.SetSPRM( 0x88, i_curr_button );
                        btni_t button_ptr = pci->hli.btnit[i_curr_button-1];
                        if ( button_ptr.auto_action_mode )
                        {
                            vlc_mutex_unlock( &p_ev->lock );
                            vlc_mutex_lock( &p_sys->lock_demuxer );

                            // process the button action
                            p_sys->dvd_interpretor.Interpret( button_ptr.cmd.bytes, 8 );

                            vlc_mutex_unlock( &p_sys->lock_demuxer );
                            vlc_mutex_lock( &p_ev->lock );
                        }
                    }
                }
                break;
            case ACTIONID_NAV_ACTIVATE:
                b_activated = true;
 
                if ( i_curr_button > 0 && i_curr_button <= pci->hli.hl_gi.btn_ns )
                {
                    btni_t button_ptr = pci->hli.btnit[i_curr_button-1];

                    vlc_mutex_unlock( &p_ev->lock );
                    vlc_mutex_lock( &p_sys->lock_demuxer );

                    // process the button action
                    p_sys->dvd_interpretor.Interpret( button_ptr.cmd.bytes, 8 );

                    vlc_mutex_unlock( &p_sys->lock_demuxer );
                    vlc_mutex_lock( &p_ev->lock );
                }
                break;
            default:
                break;
            }
            p_ev->i_key_action = 0;
            vlc_mutex_unlock( &p_ev->lock );
        }

        /* MOUSE part */
        if( p_vout && ( p_ev->b_moved || p_ev->b_clicked ) )
        {
            int x, y;

            var_GetCoords( p_vout, "mouse-moved", &x, &y );
            vlc_mutex_lock( &p_ev->lock );
            pci_t *pci = (pci_t *) &p_sys->pci_packet;

            if( p_ev->b_clicked )
            {
                int32_t button;
                int32_t best,dist,d;
                int32_t mx,my,dx,dy;

                msg_Dbg( p_ev->p_demux, "Handle Mouse Event: Mouse clicked x(%d)*y(%d)", x, y);

                b_activated = true;
                // get current button
                best = 0;
                dist = 0x08000000; /* >> than  (720*720)+(567*567); */
                for(button = 1; button <= pci->hli.hl_gi.btn_ns; button++)
                {
                    btni_t *button_ptr = &(pci->hli.btnit[button-1]);

                    if(((unsigned)x >= button_ptr->x_start)
                     && ((unsigned)x <= button_ptr->x_end)
                     && ((unsigned)y >= button_ptr->y_start)
                     && ((unsigned)y <= button_ptr->y_end))
                    {
                        mx = (button_ptr->x_start + button_ptr->x_end)/2;
                        my = (button_ptr->y_start + button_ptr->y_end)/2;
                        dx = mx - x;
                        dy = my - y;
                        d = (dx*dx) + (dy*dy);
                        /* If the mouse is within the button and the mouse is closer
                        * to the center of this button then it is the best choice. */
                        if(d < dist) {
                            dist = d;
                            best = button;
                        }
                    }
                }

                if ( best != 0)
                {
                    btni_t button_ptr = pci->hli.btnit[best-1];
                    uint16 i_curr_button = p_sys->dvd_interpretor.GetSPRM( 0x88 );

                    msg_Dbg( &p_sys->demuxer, "Clicked button %d", best );
                    vlc_mutex_unlock( &p_ev->lock );
                    vlc_mutex_lock( &p_sys->lock_demuxer );

                    // process the button action
                    p_sys->dvd_interpretor.SetSPRM( 0x88, best );
                    p_sys->dvd_interpretor.Interpret( button_ptr.cmd.bytes, 8 );

                    msg_Dbg( &p_sys->demuxer, "Processed button %d", best );

                    // select new button
                    if ( best != i_curr_button )
                    {
                        vlc_value_t val;

                        if( var_Get( p_sys->p_input, "highlight-mutex", &val ) == VLC_SUCCESS )
                        {
                            vlc_mutex_t *p_mutex = (vlc_mutex_t *) val.p_address;
                            uint32_t i_palette;

                            if(button_ptr.btn_coln != 0) {
                                i_palette = pci->hli.btn_colit.btn_coli[button_ptr.btn_coln-1][1];
                            } else {
                                i_palette = 0;
                            }

                            for( int i = 0; i < 4; i++ )
                            {
                                uint32_t i_yuv = 0xFF;//p_sys->clut[(hl.palette>>(16+i*4))&0x0f];
                                uint8_t i_alpha = (i_palette>>(i*4))&0x0f;
                                i_alpha = i_alpha == 0xf ? 0xff : i_alpha << 4;

                                p_sys->palette[i][0] = (i_yuv >> 16) & 0xff;
                                p_sys->palette[i][1] = (i_yuv >> 0) & 0xff;
                                p_sys->palette[i][2] = (i_yuv >> 8) & 0xff;
                                p_sys->palette[i][3] = i_alpha;
                            }

                            vlc_mutex_lock( p_mutex );
                            val.i_int = button_ptr.x_start; var_Set( p_sys->p_input, "x-start", val );
                            val.i_int = button_ptr.x_end;   var_Set( p_sys->p_input, "x-end",   val );
                            val.i_int = button_ptr.y_start; var_Set( p_sys->p_input, "y-start", val );
                            val.i_int = button_ptr.y_end;   var_Set( p_sys->p_input, "y-end",   val );

                            val.p_address = (void *)p_sys->palette;
                            var_Set( p_sys->p_input, "menu-palette", val );

                            val.b_bool = true; var_Set( p_sys->p_input, "highlight", val );
                            vlc_mutex_unlock( p_mutex );
                        }
                    }
                    vlc_mutex_unlock( &p_sys->lock_demuxer );
                    vlc_mutex_lock( &p_ev->lock );
                }
            }
            else if( p_ev->b_moved )
            {
//                dvdnav_mouse_select( NULL, pci, x, y );
            }

            p_ev->b_moved = false;
            p_ev->b_clicked = false;
            vlc_mutex_unlock( &p_ev->lock );
        }

        /* VOUT part */
        if( p_vout && !vlc_object_alive (p_vout) )
        {
            var_DelCallback( p_vout, "mouse-moved", EventMouse, p_ev );
            var_DelCallback( p_vout, "mouse-clicked", EventMouse, p_ev );
            vlc_object_release( p_vout );
            p_vout = NULL;
        }

        else if( p_vout == NULL )
        {
            p_vout = (vlc_object_t*) vlc_object_find( p_sys->p_input, VLC_OBJECT_VOUT,
                                      FIND_CHILD );
            if( p_vout)
            {
                var_AddCallback( p_vout, "mouse-moved", EventMouse, p_ev );
                var_AddCallback( p_vout, "mouse-clicked", EventMouse, p_ev );
            }
        }

        /* Wait a bit, 10ms */
        msleep( 10000 );
    }

    /* Release callback */
    if( p_vout )
    {
        var_DelCallback( p_vout, "mouse-moved", EventMouse, p_ev );
        var_DelCallback( p_vout, "mouse-clicked", EventMouse, p_ev );
        vlc_object_release( p_vout );
    }
    var_DelCallback( p_ev->p_libvlc, "key-action", EventKey, p_ev );

    vlc_mutex_destroy( &p_ev->lock );

    vlc_restorecancel (canc);
    return VLC_SUCCESS;
}

void demux_sys_t::PreloadFamily( const matroska_segment_c & of_segment )
{
    for (size_t i=0; i<opened_segments.size(); i++)
    {
        opened_segments[i]->PreloadFamily( of_segment );
    }
}

// preload all the linked segments for all preloaded segments
void demux_sys_t::PreloadLinked( matroska_segment_c *p_segment )
{
    size_t i_preloaded, i, j;
    virtual_segment_c *p_seg;

    p_current_segment = VirtualFromSegments( p_segment );
 
    used_segments.push_back( p_current_segment );

    // create all the other virtual segments of the family
    do {
        i_preloaded = 0;
        for ( i=0; i< opened_segments.size(); i++ )
        {
            if ( opened_segments[i]->b_preloaded && !IsUsedSegment( *opened_segments[i] ) )
            {
                p_seg = VirtualFromSegments( opened_segments[i] );
                used_segments.push_back( p_seg );
                i_preloaded++;
            }
        }
    } while ( i_preloaded ); // worst case: will stop when all segments are found as family related

    // publish all editions of all usable segment
    for ( i=0; i< used_segments.size(); i++ )
    {
        p_seg = used_segments[i];
        if ( p_seg->p_editions != NULL )
        {
            input_title_t *p_title = vlc_input_title_New();
            p_seg->i_sys_title = i;
            int i_chapters;

            // TODO use a name for each edition, let the TITLE deal with a codec name
            for ( j=0; j<p_seg->p_editions->size(); j++ )
            {
                if ( p_title->psz_name == NULL )
                {
                    const char* psz_tmp = (*p_seg->p_editions)[j]->GetMainName().c_str();
                    if( *psz_tmp != '\0' )
                        p_title->psz_name = strdup( psz_tmp );
                }

                chapter_edition_c *p_edition = (*p_seg->p_editions)[j];

                i_chapters = 0;
                p_edition->PublishChapters( *p_title, i_chapters, 0 );
            }

            // create a name if there is none
            if ( p_title->psz_name == NULL )
            {
                if( asprintf(&(p_title->psz_name), "%s %d", N_("Segment"), (int)i) == -1 )
                    p_title->psz_name = NULL;
            }

            titles.push_back( p_title );
        }
    }

    // TODO decide which segment should be first used (VMG for DVD)
}

bool demux_sys_t::IsUsedSegment( matroska_segment_c &segment ) const
{
    for ( size_t i=0; i< used_segments.size(); i++ )
    {
        if ( used_segments[i]->FindUID( *segment.p_segment_uid ) )
            return true;
    }
    return false;
}

virtual_segment_c *demux_sys_t::VirtualFromSegments( matroska_segment_c *p_segment ) const
{
    size_t i_preloaded, i;

    virtual_segment_c *p_result = new virtual_segment_c( p_segment );

    // fill our current virtual segment with all hard linked segments
    do {
        i_preloaded = 0;
        for ( i=0; i< opened_segments.size(); i++ )
        {
            i_preloaded += p_result->AddSegment( opened_segments[i] );
        }
    } while ( i_preloaded ); // worst case: will stop when all segments are found as linked

    p_result->Sort( );

    p_result->PreloadLinked( );

    p_result->PrepareChapters( );

    return p_result;
}

bool demux_sys_t::PreparePlayback( virtual_segment_c *p_new_segment )
{
    if ( p_new_segment != NULL && p_new_segment != p_current_segment )
    {
        if ( p_current_segment != NULL && p_current_segment->Segment() != NULL )
            p_current_segment->Segment()->UnSelect();

        p_current_segment = p_new_segment;
        i_current_title = p_new_segment->i_sys_title;
    }
    if( !p_current_segment->Segment()->b_cues )
        msg_Warn( &p_current_segment->Segment()->sys.demuxer, "no cues/empty cues found->seek won't be precise" );

    f_duration = p_current_segment->Duration();

    /* add information */
    p_current_segment->Segment()->InformationCreate( );
    p_current_segment->Segment()->Select( 0 );

    return true;
}

void demux_sys_t::JumpTo( virtual_segment_c & vsegment, chapter_item_c * p_chapter )
{
    // if the segment is not part of the current segment, select the new one
    if ( &vsegment != p_current_segment )
    {
        PreparePlayback( &vsegment );
    }

    if ( p_chapter != NULL )
    {
        if ( !p_chapter->Enter( true ) )
        {
            // jump to the location in the found segment
            vsegment.Seek( demuxer, p_chapter->i_user_start_time, -1, p_chapter, -1 );
        }
    }
 
}

matroska_segment_c *demux_sys_t::FindSegment( const EbmlBinary & uid ) const
{
    for (size_t i=0; i<opened_segments.size(); i++)
    {
        if ( *opened_segments[i]->p_segment_uid == uid )
            return opened_segments[i];
    }
    return NULL;
}

chapter_item_c *demux_sys_t::BrowseCodecPrivate( unsigned int codec_id,
                                        bool (*match)(const chapter_codec_cmds_c &data, const void *p_cookie, size_t i_cookie_size ),
                                        const void *p_cookie,
                                        size_t i_cookie_size,
                                        virtual_segment_c * &p_segment_found )
{
    chapter_item_c *p_result = NULL;
    for (size_t i=0; i<used_segments.size(); i++)
    {
        p_result = used_segments[i]->BrowseCodecPrivate( codec_id, match, p_cookie, i_cookie_size );
        if ( p_result != NULL )
        {
            p_segment_found = used_segments[i];
            break;
        }
    }
    return p_result;
}

chapter_item_c *demux_sys_t::FindChapter( int64_t i_find_uid, virtual_segment_c * & p_segment_found )
{
    chapter_item_c *p_result = NULL;
    for (size_t i=0; i<used_segments.size(); i++)
    {
        p_result = used_segments[i]->FindChapter( i_find_uid );
        if ( p_result != NULL )
        {
            p_segment_found = used_segments[i];
            break;
        }
    }
    return p_result;
}

void demux_sys_t::SwapButtons()
{
#ifndef WORDS_BIGENDIAN
    uint8_t button, i, j;

    for( button = 1; button <= pci_packet.hli.hl_gi.btn_ns; button++) {
        btni_t *button_ptr = &(pci_packet.hli.btnit[button-1]);
        binary *p_data = (binary*) button_ptr;

        uint16 i_x_start = ((p_data[0] & 0x3F) << 4 ) + ( p_data[1] >> 4 );
        uint16 i_x_end   = ((p_data[1] & 0x03) << 8 ) + p_data[2];
        uint16 i_y_start = ((p_data[3] & 0x3F) << 4 ) + ( p_data[4] >> 4 );
        uint16 i_y_end   = ((p_data[4] & 0x03) << 8 ) + p_data[5];
        button_ptr->x_start = i_x_start;
        button_ptr->x_end   = i_x_end;
        button_ptr->y_start = i_y_start;
        button_ptr->y_end   = i_y_end;

    }
    for ( i = 0; i<3; i++ )
    {
        for ( j = 0; j<2; j++ )
        {
            pci_packet.hli.btn_colit.btn_coli[i][j] = U32_AT( &pci_packet.hli.btn_colit.btn_coli[i][j] );
        }
    }
#endif
}
