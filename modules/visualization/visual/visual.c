/*****************************************************************************
 * visual.c : Visualisation system
 *****************************************************************************
 * Copyright (C) 2002-2006 the VideoLAN team
 * $Id: 73a71a67217e56fcee4d674c003569e2c05f309e $
 *
 * Authors: Clément Stenac <zorglub@via.ecp.fr>
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
#include <vlc_vout.h>
#include <vlc_aout.h>

#include "visual.h"

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define ELIST_TEXT N_( "Effects list" )
#define ELIST_LONGTEXT N_( \
      "A list of visual effect, separated by commas.\n"  \
      "Current effects include: dummy, scope, spectrum." )

#define WIDTH_TEXT N_( "Video width" )
#define WIDTH_LONGTEXT N_( \
      "The width of the effects video window, in pixels." )

#define HEIGHT_TEXT N_( "Video height" )
#define HEIGHT_LONGTEXT N_( \
      "The height of the effects video window, in pixels." )

#define NBBANDS_TEXT N_( "Number of bands" )
#define NBBANDS_LONGTEXT N_( \
      "Number of bands used by spectrum analyzer, should be 20 or 80." )
#define SPNBBANDS_LONGTEXT N_( \
      "Number of bands used by the spectrometer, from 20 to 80." )

#define SEPAR_TEXT N_( "Band separator" )
#define SEPAR_LONGTEXT N_( \
        "Number of blank pixels between bands.")

#define AMP_TEXT N_( "Amplification" )
#define AMP_LONGTEXT N_( \
        "This is a coefficient that modifies the height of the bands.")

#define PEAKS_TEXT N_( "Enable peaks" )
#define PEAKS_LONGTEXT N_( \
        "Draw \"peaks\" in the spectrum analyzer." )

#define ORIG_TEXT N_( "Enable original graphic spectrum" )
#define ORIG_LONGTEXT N_( \
        "Enable the \"flat\" spectrum analyzer in the spectrometer." )

#define BANDS_TEXT N_( "Enable bands" )
#define BANDS_LONGTEXT N_( \
        "Draw bands in the spectrometer." )

#define BASE_TEXT N_( "Enable base" )
#define BASE_LONGTEXT N_( \
        "Defines whether to draw the base of the bands." )

#define RADIUS_TEXT N_( "Base pixel radius" )
#define RADIUS_LONGTEXT N_( \
        "Defines radius size in pixels, of base of bands(beginning)." )

#define SSECT_TEXT N_( "Spectral sections" )
#define SSECT_LONGTEXT N_( \
        "Determines how many sections of spectrum will exist." )

#define PEAK_HEIGHT_TEXT N_( "Peak height" )
#define PEAK_HEIGHT_LONGTEXT N_( \
        "Total pixel height of the peak items." )

#define PEAK_WIDTH_TEXT N_( "Peak extra width" )
#define PEAK_WIDTH_LONGTEXT N_( \
        "Additions or subtractions of pixels on the peak width." )

#define COLOR1_TEXT N_( "V-plane color" )
#define COLOR1_LONGTEXT N_( \
        "YUV-Color cube shifting across the V-plane ( 0 - 127 )." )

#define STARS_TEXT N_( "Number of stars" )
#define STARS_LONGTEXT N_( \
        "Number of stars to draw with random effect." )

static int  Open         ( vlc_object_t * );
static void Close        ( vlc_object_t * );

vlc_module_begin();
    set_shortname( N_("Visualizer"));
    set_category( CAT_AUDIO );
    set_subcategory( SUBCAT_AUDIO_VISUAL );
    set_description( N_("Visualizer filter") );
    set_section( N_( "General") , NULL );
    add_string("effect-list", "spectrum", NULL,
            ELIST_TEXT, ELIST_LONGTEXT, true );
    add_integer("effect-width",VOUT_WIDTH,NULL,
             WIDTH_TEXT, WIDTH_LONGTEXT, false );
    add_integer("effect-height" , VOUT_HEIGHT , NULL,
             HEIGHT_TEXT, HEIGHT_LONGTEXT, false );
    set_section( N_("Spectrum analyser") , NULL );
    add_integer("visual-nbbands", 80, NULL,
             NBBANDS_TEXT, NBBANDS_LONGTEXT, true );
    add_integer("visual-separ", 1, NULL,
             SEPAR_TEXT, SEPAR_LONGTEXT, true );
    add_integer("visual-amp", 3, NULL,
             AMP_TEXT, AMP_LONGTEXT, true );
    add_bool("visual-peaks", true, NULL,
             PEAKS_TEXT, PEAKS_LONGTEXT, true );
    set_section( N_("Spectrometer") , NULL );
    add_bool("spect-show-original", false, NULL,
             ORIG_TEXT, ORIG_LONGTEXT, true );
    add_bool("spect-show-base", true, NULL,
             BASE_TEXT, BASE_LONGTEXT, true );
    add_integer("spect-radius", 42, NULL,
             RADIUS_TEXT, RADIUS_LONGTEXT, true );
    add_integer("spect-sections", 3, NULL,
             SSECT_TEXT, SSECT_LONGTEXT, true );
    add_integer("spect-color", 80, NULL,
             COLOR1_TEXT, COLOR1_LONGTEXT, true );
    add_bool("spect-show-bands", true, NULL,
             BANDS_TEXT, BANDS_LONGTEXT, true );
    add_integer("spect-nbbands", 32, NULL,
             NBBANDS_TEXT, SPNBBANDS_LONGTEXT, true );
    add_integer("spect-separ", 1, NULL,
             SEPAR_TEXT, SEPAR_LONGTEXT, true );
    add_integer("spect-amp", 8, NULL,
             AMP_TEXT, AMP_LONGTEXT, true );
    add_bool("spect-show-peaks", true, NULL,
             PEAKS_TEXT, PEAKS_LONGTEXT, true );
    add_integer("spect-peak-width", 61, NULL,
             PEAK_WIDTH_TEXT, PEAK_WIDTH_LONGTEXT, true );
    add_integer("spect-peak-height", 1, NULL,
             PEAK_HEIGHT_TEXT, PEAK_HEIGHT_LONGTEXT, true );
    set_capability( "visualization", 0 );
    set_callbacks( Open, Close );
    add_shortcut( "visualizer");
vlc_module_end();


/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static void DoWork( aout_instance_t *, aout_filter_t *,
                    aout_buffer_t *, aout_buffer_t * );
static int FilterCallback( vlc_object_t *, char const *,
                           vlc_value_t, vlc_value_t, void * );
static const struct
{
    const char *psz_name;
    int  (*pf_run)( visual_effect_t *, aout_instance_t *,
                    aout_buffer_t *, picture_t *);
} pf_effect_run[]=
{
    { "scope",      scope_Run },
    { "vuMeter",    vuMeter_Run },
    { "spectrum",   spectrum_Run },
    { "spectrometer",   spectrometer_Run },
    { "dummy",      dummy_Run},
    { NULL,         dummy_Run}
};

/*****************************************************************************
 * Open: open the visualizer
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    aout_filter_t     *p_filter = (aout_filter_t *)p_this;
    aout_filter_sys_t *p_sys;
    vlc_value_t        val;

    char *psz_effects, *psz_parser;
    video_format_t fmt;


    if( ( p_filter->input.i_format != VLC_FOURCC('f','l','3','2') &&
          p_filter->input.i_format != VLC_FOURCC('f','i','3','2') ) )
    {
        return VLC_EGENERIC;
    }

    p_sys = p_filter->p_sys = malloc( sizeof( aout_filter_sys_t ) );
    if( p_sys == NULL )
        return VLC_EGENERIC;

    p_sys->i_height = config_GetInt( p_filter , "effect-height");
    p_sys->i_width  = config_GetInt( p_filter , "effect-width");

    if( p_sys->i_height < 20 ) p_sys->i_height =  20;
    if( p_sys->i_width  < 20 ) p_sys->i_width  =  20;
    if( (p_sys->i_height % 2 ) != 0 ) p_sys->i_height--;
    if( (p_sys->i_width % 2 )  != 0 ) p_sys->i_width--;

    p_sys->i_effect = 0;
    p_sys->effect   = NULL;

    /* Parse the effect list */
    var_Create( p_filter, "effect-list", VLC_VAR_STRING | VLC_VAR_DOINHERIT );
    var_Get( p_filter, "effect-list", &val);
    psz_parser = psz_effects = strdup( val.psz_string );
    free( val.psz_string );

    var_AddCallback( p_filter, "effect-list", FilterCallback, NULL );

    while( psz_parser && *psz_parser != '\0' )
    {
        visual_effect_t *p_effect;
        int  i;

        p_effect = malloc( sizeof( visual_effect_t ) );
        if( !p_effect )
            break;
        p_effect->i_width = p_sys->i_width;
        p_effect->i_height= p_sys->i_height;
        p_effect->i_nb_chans = aout_FormatNbChannels( &p_filter->input);
        p_effect->psz_args = NULL;
        p_effect->p_data = NULL;

        p_effect->pf_run = NULL;
        p_effect->psz_name = NULL;

        for( i = 0; pf_effect_run[i].psz_name != NULL; i++ )
        {
            if( !strncasecmp( psz_parser,
                              pf_effect_run[i].psz_name,
                              strlen( pf_effect_run[i].psz_name ) ) )
            {
                p_effect->pf_run = pf_effect_run[i].pf_run;
                p_effect->psz_name = strdup( pf_effect_run[i].psz_name );
                break;
            }
        }

        if( p_effect->psz_name )
        {
            psz_parser += strlen( p_effect->psz_name );

            if( *psz_parser == '{' )
            {
                char *psz_eoa;

                psz_parser++;

                if( ( psz_eoa = strchr( psz_parser, '}') ) == NULL )
                {
                   msg_Err( p_filter, "unable to parse effect list. Aborting");
                   break;
                }
                p_effect->psz_args =
                    strndup( psz_parser, psz_eoa - psz_parser);
            }
            TAB_APPEND( p_sys->i_effect, p_sys->effect, p_effect );
        }
        else
        {
            msg_Err( p_filter, "unknown visual effect: %s", psz_parser );
            free( p_effect );
        }

        if( strchr( psz_parser, ',' ) )
        {
            psz_parser = strchr( psz_parser, ',' ) + 1;
        }
        else if( strchr( psz_parser, ':' ) )
        {
            psz_parser = strchr( psz_parser, ':' ) + 1;
        }
        else
        {
            break;
        }
    }

    free( psz_effects );

    if( !p_sys->i_effect )
    {
        msg_Err( p_filter, "no effects found" );
        free( p_sys );
        return VLC_EGENERIC;
    }

    /* Open the video output */
    memset( &fmt, 0, sizeof(video_format_t) );

    fmt.i_width = fmt.i_visible_width = p_sys->i_width;
    fmt.i_height = fmt.i_visible_height = p_sys->i_height;
    fmt.i_chroma = VLC_FOURCC('I','4','2','0');
    fmt.i_aspect = VOUT_ASPECT_FACTOR * p_sys->i_width/p_sys->i_height;
    fmt.i_sar_num = fmt.i_sar_den = 1;

    p_sys->p_vout = vout_Request( p_filter, NULL, &fmt );
    if( p_sys->p_vout == NULL )
    {
        msg_Err( p_filter, "no suitable vout module" );
        free( p_sys );
        return VLC_EGENERIC;
    }

    p_filter->pf_do_work = DoWork;
    p_filter->b_in_place= 1;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * DoWork: convert a buffer
 *****************************************************************************
 * Audio part pasted from trivial.c
 ****************************************************************************/
static void DoWork( aout_instance_t *p_aout, aout_filter_t *p_filter,
                    aout_buffer_t *p_in_buf, aout_buffer_t *p_out_buf )
{
    aout_filter_sys_t *p_sys = p_filter->p_sys;
    picture_t *p_outpic;
    int i;

    p_out_buf->i_nb_samples = p_in_buf->i_nb_samples;
    p_out_buf->i_nb_bytes = p_in_buf->i_nb_bytes *
                            aout_FormatNbChannels( &p_filter->output ) /
                            aout_FormatNbChannels( &p_filter->input );

    /* First, get a new picture */
    while( ( p_outpic = vout_CreatePicture( p_sys->p_vout, 0, 0, 3 ) ) == NULL)
    {
        if( !vlc_object_alive (p_aout) )
        {
            return;
        }
        msleep( VOUT_OUTMEM_SLEEP );
    }

    /* Blank the picture */
    for( i = 0 ; i < p_outpic->i_planes ; i++ )
    {
        memset( p_outpic->p[i].p_pixels, i > 0 ? 0x80 : 0x00,
                p_outpic->p[i].i_visible_lines * p_outpic->p[i].i_pitch );
    }

    /* We can now call our visualization effects */
    for( i = 0; i < p_sys->i_effect; i++ )
    {
#define p_effect p_sys->effect[i]
        if( p_effect->pf_run )
        {
            p_effect->pf_run( p_effect, p_aout, p_out_buf, p_outpic );
        }
#undef p_effect
    }

    vout_DatePicture( p_sys->p_vout, p_outpic,
                      ( p_in_buf->start_date + p_in_buf->end_date ) / 2 );

    vout_DisplayPicture( p_sys->p_vout, p_outpic );
}

/*****************************************************************************
 * Close: close the plugin
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    aout_filter_t * p_filter = (aout_filter_t *)p_this;
    aout_filter_sys_t *p_sys = p_filter->p_sys;

    int i;

    if( p_filter->p_sys->p_vout )
    {
        vout_Request( p_filter, p_filter->p_sys->p_vout, 0 );
    }

    /* Free the list */
    for( i = 0; i < p_sys->i_effect; i++ )
    {
#define p_effect p_sys->effect[i]
        free( p_effect->p_data );
        free( p_effect->psz_name );
        free( p_effect->psz_args );
        free( p_effect );
#undef p_effect
    }

    free( p_sys->effect );
    free( p_filter->p_sys );
}

/*****************************************************************************
 * FilterCallback: called when changing the deinterlace method on the fly.
 *****************************************************************************/
static int FilterCallback( vlc_object_t *p_this, char const *psz_cmd,
                           vlc_value_t oldval, vlc_value_t newval,
                           void *p_data )
{
    VLC_UNUSED(psz_cmd); VLC_UNUSED(oldval);
    VLC_UNUSED(p_data); VLC_UNUSED(newval);
    aout_filter_t     *p_filter = (aout_filter_t *)p_this;
    /* restart this baby */
    msg_Dbg( p_filter, "we should restart the visual filter" );
    return VLC_SUCCESS;
}

