/*****************************************************************************
 * podcast.c : podcast playlist imports
 *****************************************************************************
 * Copyright (C) 2005-2009 the VideoLAN team
 * $Id: df6736e1ea498417079bb562dded31363611c4ca $
 *
 * Authors: Antoine Cellerier <dionoea -at- videolan -dot- org>
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
#include <vlc_demux.h>

#include "playlist.h"
#include <vlc_xml.h>

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int Demux( demux_t *p_demux);
static int Control( demux_t *p_demux, int i_query, va_list args );
static mtime_t strTimeToMTime( const char *psz );

/*****************************************************************************
 * Import_podcast: main import function
 *****************************************************************************/
int Import_podcast( vlc_object_t *p_this )
{
    demux_t *p_demux = (demux_t *)p_this;

    if( !demux_IsForced( p_demux, "podcast" ) )
        return VLC_EGENERIC;

    p_demux->pf_demux = Demux;
    p_demux->pf_control = Control;
    msg_Dbg( p_demux, "using podcast reader" );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Deactivate: frees unused data
 *****************************************************************************/
void Close_podcast( vlc_object_t *p_this )
{
    (void)p_this;
}

/* "specs" : http://phobos.apple.com/static/iTunesRSS.html */
static int Demux( demux_t *p_demux )
{
    bool b_item = false;
    bool b_image = false;
    int i_ret;

    xml_t *p_xml;
    xml_reader_t *p_xml_reader = NULL;
    char *psz_elname = NULL;
    char *psz_item_mrl = NULL;
    char *psz_item_size = NULL;
    char *psz_item_type = NULL;
    char *psz_item_name = NULL;
    char *psz_item_date = NULL;
    char *psz_item_author = NULL;
    char *psz_item_category = NULL;
    char *psz_item_duration = NULL;
    char *psz_item_keywords = NULL;
    char *psz_item_subtitle = NULL;
    char *psz_item_summary = NULL;
    char *psz_art_url = NULL;
    int i_type;
    input_item_t *p_input;
    input_item_node_t *p_subitems = NULL;

    input_item_t *p_current_input = GetCurrentItem(p_demux);

    p_xml = xml_Create( p_demux );
    if( !p_xml )
        goto error;

    p_xml_reader = xml_ReaderCreate( p_xml, p_demux->s );
    if( !p_xml_reader )
        goto error;

    /* xml */
    /* check root node */
    if( xml_ReaderRead( p_xml_reader ) != 1 )
    {
        msg_Err( p_demux, "invalid file (no root node)" );
        goto error;
    }

    while( xml_ReaderNodeType( p_xml_reader ) == XML_READER_NONE )
    {
        if( xml_ReaderRead( p_xml_reader ) != 1 )
        {
            msg_Err( p_demux, "invalid file (no root node)" );
            goto error;
        }
    }

    if( xml_ReaderNodeType( p_xml_reader ) != XML_READER_STARTELEM ||
        ( psz_elname = xml_ReaderName( p_xml_reader ) ) == NULL ||
        strcmp( psz_elname, "rss" ) )
    {
        msg_Err( p_demux, "invalid root node %i, %s",
                 xml_ReaderNodeType( p_xml_reader ), psz_elname );
        goto error;
    }
    FREENULL( psz_elname );

    p_subitems = input_item_node_Create( p_current_input );

    while( (i_ret = xml_ReaderRead( p_xml_reader )) == 1 )
    {
        // Get the node type
        i_type = xml_ReaderNodeType( p_xml_reader );
        switch( i_type )
        {
            // Error
            case -1:
                goto error;

            case XML_READER_STARTELEM:
            {
                // Read the element name
                free( psz_elname );
                psz_elname = xml_ReaderName( p_xml_reader );
                if( !psz_elname )
                    goto error;

                if( !strcmp( psz_elname, "item" ) )
                {
                    b_item = true;
                }
                else if( !strcmp( psz_elname, "image" ) )
                {
                    b_image = true;
                }

                // Read the attributes
                while( xml_ReaderNextAttr( p_xml_reader ) == VLC_SUCCESS )
                {
                    char *psz_name = xml_ReaderName( p_xml_reader );
                    char *psz_value = xml_ReaderValue( p_xml_reader );
                    if( !psz_name || !psz_value )
                    {
                        free( psz_name );
                        free( psz_value );
                        goto error;
                    }

                    if( !strcmp( psz_elname, "enclosure" ) )
                    {
                        if( !strcmp( psz_name, "url" ) )
                        {
                            free( psz_item_mrl );
                            psz_item_mrl = psz_value;
                        }
                        else if( !strcmp( psz_name, "length" ) )
                        {
                            free( psz_item_size );
                            psz_item_size = psz_value;
                        }
                        else if( !strcmp( psz_name, "type" ) )
                        {
                            free( psz_item_type );
                            psz_item_type = psz_value;
                        }
                        else
                        {
                            msg_Dbg( p_demux,"unhandled attribute %s in element %s",
                                     psz_name, psz_elname );
                            free( psz_value );
                        }
                    }
                    else
                    {
                        msg_Dbg( p_demux,"unhandled attribute %s in element %s",
                                  psz_name, psz_elname );
                        free( psz_value );
                    }
                    free( psz_name );
                }
                break;
            }
            case XML_READER_TEXT:
            {
                if(!psz_elname) break;

                char *psz_text = xml_ReaderValue( p_xml_reader );

#define SET_DATA( field, name )                 \
    else if( !strcmp( psz_elname, name ) )      \
    {                                           \
        field = psz_text;                       \
    }
                /* item specific meta data */
                if( b_item == true )
                {
                    if( !strcmp( psz_elname, "title" ) )
                    {
                        psz_item_name = psz_text;
                    }
                    else if( !strcmp( psz_elname, "itunes:author" ) ||
                             !strcmp( psz_elname, "author" ) )
                    { /* <author> isn't standard iTunes podcast stuff */
                        psz_item_author = psz_text;
                    }
                    else if( !strcmp( psz_elname, "itunes:summary" ) ||
                             !strcmp( psz_elname, "description" ) )
                    { /* <description> isn't standard iTunes podcast stuff */
                        psz_item_summary = psz_text;
                    }
                    SET_DATA( psz_item_date, "pubDate" )
                    SET_DATA( psz_item_category, "itunes:category" )
                    SET_DATA( psz_item_duration, "itunes:duration" )
                    SET_DATA( psz_item_keywords, "itunes:keywords" )
                    SET_DATA( psz_item_subtitle, "itunes:subtitle" )
                    else
                        free( psz_text );
                }
#undef SET_DATA

                /* toplevel meta data */
                else if( b_image == false )
                {
                    if( !strcmp( psz_elname, "title" ) )
                    {
                        input_item_SetName( p_current_input, psz_text );
                    }
#define ADD_GINFO( info, name ) \
    else if( !strcmp( psz_elname, name ) ) \
    { \
        input_item_AddInfo( p_current_input, _("Podcast Info"), \
                                _( info ), "%s", psz_text ); \
    }
                    ADD_GINFO( "Podcast Link", "link" )
                    ADD_GINFO( "Podcast Copyright", "copyright" )
                    ADD_GINFO( "Podcast Category", "itunes:category" )
                    ADD_GINFO( "Podcast Keywords", "itunes:keywords" )
                    ADD_GINFO( "Podcast Subtitle", "itunes:subtitle" )
#undef ADD_GINFO
                    else if( !strcmp( psz_elname, "itunes:summary" ) ||
                             !strcmp( psz_elname, "description" ) )
                    { /* <description> isn't standard iTunes podcast stuff */
                        input_item_AddInfo( p_current_input,
                            _( "Podcast Info" ), _( "Podcast Summary" ),
                            "%s", psz_text );
                    }
                    free( psz_text );
                }
                else
                {
                    if( !strcmp( psz_elname, "url" ) )
                    {
                        free( psz_art_url );
                        psz_art_url = psz_text;
                    }
                    else
                    {
                        msg_Dbg( p_demux, "unhandled text in element '%s'",
                                 psz_elname );
                        free( psz_text );
                    }
                }
                break;
            }
            // End element
            case XML_READER_ENDELEM:
            {
                // Read the element name
                free( psz_elname );
                psz_elname = xml_ReaderName( p_xml_reader );
                if( !psz_elname )
                    goto error;
                if( !strcmp( psz_elname, "item" ) )
                {
                    if( psz_item_mrl == NULL )
                    {
                        msg_Err( p_demux, "invalid XML (no enclosure markup)" );
                        goto error;
                    }
                    p_input = input_item_New( p_demux, psz_item_mrl, psz_item_name );
                    if( p_input == NULL ) break;
#define ADD_INFO( info, field ) \
    if( field ) { input_item_AddInfo( p_input, \
                            _( "Podcast Info" ),  _( info ), "%s", field ); }
                    ADD_INFO( "Podcast Publication Date", psz_item_date  );
                    ADD_INFO( "Podcast Author", psz_item_author );
                    ADD_INFO( "Podcast Subcategory", psz_item_category );
                    ADD_INFO( "Podcast Duration", psz_item_duration );
                    ADD_INFO( "Podcast Keywords", psz_item_keywords );
                    ADD_INFO( "Podcast Subtitle", psz_item_subtitle );
                    ADD_INFO( "Podcast Summary", psz_item_summary );
                    ADD_INFO( "Podcast Type", psz_item_type );
#undef ADD_INFO

                    /* Set the duration if available */
                    if( psz_item_duration )
                        input_item_SetDuration( p_input, strTimeToMTime( psz_item_duration ) );
                    /* Add the global art url to this item, if any */
                    if( psz_art_url )
                        input_item_SetArtURL( p_input, psz_art_url );

                    if( psz_item_size )
                    {
                        input_item_AddInfo( p_input,
                                                _( "Podcast Info" ),
                                                _( "Podcast Size" ),
                                                "%s bytes",
                                                psz_item_size );
                    }
                    input_item_node_AppendItem( p_subitems, p_input );
                    vlc_gc_decref( p_input );
                    FREENULL( psz_item_name );
                    FREENULL( psz_item_mrl );
                    FREENULL( psz_item_size );
                    FREENULL( psz_item_type );
                    FREENULL( psz_item_date );
                    FREENULL( psz_item_author );
                    FREENULL( psz_item_category );
                    FREENULL( psz_item_duration );
                    FREENULL( psz_item_keywords );
                    FREENULL( psz_item_subtitle );
                    FREENULL( psz_item_summary );
                    b_item = false;
                }
                else if( !strcmp( psz_elname, "image" ) )
                {
                    b_image = false;
                }
                free( psz_elname );
                psz_elname = strdup( "" );

                break;
            }
        }
    }

    if( i_ret != 0 )
    {
        msg_Warn( p_demux, "error while parsing data" );
    }

    free( psz_art_url );
    free( psz_elname );
    xml_ReaderDelete( p_xml, p_xml_reader );
    xml_Delete( p_xml );

    input_item_node_PostAndDelete( p_subitems );
    vlc_gc_decref(p_current_input);
    return 0; /* Needed for correct operation of go back */

error:
    free( psz_item_name );
    free( psz_item_mrl );
    free( psz_item_size );
    free( psz_item_type );
    free( psz_item_date );
    free( psz_item_author );
    free( psz_item_category );
    free( psz_item_duration );
    free( psz_item_keywords );
    free( psz_item_subtitle );
    free( psz_item_summary );
    free( psz_art_url );
    free( psz_elname );

    if( p_xml_reader )
        xml_ReaderDelete( p_xml, p_xml_reader );
    if( p_xml )
        xml_Delete( p_xml );
    if( p_subitems )
        input_item_node_Delete( p_subitems );

    vlc_gc_decref(p_current_input);
    return -1;
}

static int Control( demux_t *p_demux, int i_query, va_list args )
{
    VLC_UNUSED(p_demux); VLC_UNUSED(i_query); VLC_UNUSED(args);
    return VLC_EGENERIC;
}

static mtime_t strTimeToMTime( const char *psz )
{
    int h, m, s;
    switch( sscanf( psz, "%u:%u:%u", &h, &m, &s ) )
    {
    case 3:
        return (mtime_t)( ( h*60 + m )*60 + s ) * 1000000;
    case 2:
        return (mtime_t)( h*60 + m ) * 1000000;
        break;
    default:
        return -1;
    }
}
