/*****************************************************************************
 * mkv.cpp : matroska demuxer
 *****************************************************************************
 * Copyright (C) 2003-2004 the VideoLAN team
 * $Id: 95b60fbf4c77d2622b9465a42c49dbd0b4c4e637 $
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/

/* config.h may include inttypes.h, so make sure we define that option
 * early enough. */
#define __STDC_FORMAT_MACROS 1
#define __STDC_CONSTANT_MACROS 1

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <inttypes.h>

#include <vlc_common.h>
#include <vlc_plugin.h>

#ifdef HAVE_TIME_H
#   include <time.h>                                               /* time() */
#endif


#include <vlc_codecs.h>               /* BITMAPINFOHEADER, WAVEFORMATEX */
#include <vlc_iso_lang.h>
#include "vlc_meta.h"
#include <vlc_charset.h>
#include <vlc_input.h>
#include <vlc_demux.h>

#include <iostream>
#include <cassert>
#include <typeinfo>
#include <string>
#include <vector>
#include <algorithm>

#ifdef HAVE_DIRENT_H
#   include <dirent.h>
#endif

/* libebml and matroska */
#include "ebml/EbmlHead.h"
#include "ebml/EbmlSubHead.h"
#include "ebml/EbmlStream.h"
#include "ebml/EbmlContexts.h"
#include "ebml/EbmlVoid.h"
#include "ebml/EbmlVersion.h"
#include "ebml/StdIOCallback.h"

#include "matroska/KaxAttachments.h"
#include "matroska/KaxAttached.h"
#include "matroska/KaxBlock.h"
#include "matroska/KaxBlockData.h"
#include "matroska/KaxChapters.h"
#include "matroska/KaxCluster.h"
#include "matroska/KaxClusterData.h"
#include "matroska/KaxContexts.h"
#include "matroska/KaxCues.h"
#include "matroska/KaxCuesData.h"
#include "matroska/KaxInfo.h"
#include "matroska/KaxInfoData.h"
#include "matroska/KaxSeekHead.h"
#include "matroska/KaxSegment.h"
#include "matroska/KaxTag.h"
#include "matroska/KaxTags.h"
#include "matroska/KaxTagMulti.h"
#include "matroska/KaxTracks.h"
#include "matroska/KaxTrackAudio.h"
#include "matroska/KaxTrackVideo.h"
#include "matroska/KaxTrackEntryData.h"
#include "matroska/KaxContentEncoding.h"
#include "matroska/KaxVersion.h"

#include "ebml/StdIOCallback.h"

#include "vlc_keys.h"

extern "C" {
   #include "mp4/libmp4.h"
}
#ifdef HAVE_ZLIB_H
#   include <zlib.h>
#endif

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

vlc_module_begin();
    set_shortname( "Matroska" );
    set_description( N_("Matroska stream demuxer" ) );
    set_capability( "demux", 50 );
    set_callbacks( Open, Close );
    set_category( CAT_INPUT );
    set_subcategory( SUBCAT_INPUT_DEMUX );

    add_bool( "mkv-use-ordered-chapters", 1, NULL,
            N_("Ordered chapters"),
            N_("Play ordered chapters as specified in the segment."), true );

    add_bool( "mkv-use-chapter-codec", 1, NULL,
            N_("Chapter codecs"),
            N_("Use chapter codecs found in the segment."), true );

    add_bool( "mkv-preload-local-dir", 1, NULL,
            N_("Preload Directory"),
            N_("Preload matroska files from the same family in the same directory (not good for broken files)."), true );

    add_bool( "mkv-seek-percent", 0, NULL,
            N_("Seek based on percent not time"),
            N_("Seek based on percent not time."), true );

    add_bool( "mkv-use-dummy", 0, NULL,
            N_("Dummy Elements"),
            N_("Read and discard unknown EBML elements (not good for broken files)."), true );

    add_shortcut( "mka" );
    add_shortcut( "mkv" );
vlc_module_end();



#define MATROSKA_COMPRESSION_NONE  -1
#define MATROSKA_COMPRESSION_ZLIB   0
#define MATROSKA_COMPRESSION_BLIB   1
#define MATROSKA_COMPRESSION_LZOX   2
#define MATROSKA_COMPRESSION_HEADER 3

#define MKVD_TIMECODESCALE 1000000

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#undef ATTRIBUTE_PACKED
#undef PRAGMA_PACK_BEGIN
#undef PRAGMA_PACK_END

#if defined(__GNUC__)
#if __GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ >= 95)
#define ATTRIBUTE_PACKED __attribute__ ((packed))
#define PRAGMA_PACK 0
#endif
#endif

#if !defined(ATTRIBUTE_PACKED)
#define ATTRIBUTE_PACKED
#define PRAGMA_PACK 1
#endif

#if PRAGMA_PACK
#pragma pack(1)
#endif

/*************************************
*  taken from libdvdnav / libdvdread
**************************************/

/**
 * DVD Time Information.
 */
typedef struct {
  uint8_t hour;
  uint8_t minute;
  uint8_t second;
  uint8_t frame_u; /* The two high bits are the frame rate. */
} ATTRIBUTE_PACKED dvd_time_t;

/**
 * User Operations.
 */
typedef struct {
#ifdef WORDS_BIGENDIAN
  unsigned char zero                           : 7; /* 25-31 */
  unsigned char video_pres_mode_change         : 1; /* 24 */
 
  unsigned char karaoke_audio_pres_mode_change : 1; /* 23 */
  unsigned char angle_change                   : 1;
  unsigned char subpic_stream_change           : 1;
  unsigned char audio_stream_change            : 1;
  unsigned char pause_on                       : 1;
  unsigned char still_off                      : 1;
  unsigned char button_select_or_activate      : 1;
  unsigned char resume                         : 1; /* 16 */
 
  unsigned char chapter_menu_call              : 1; /* 15 */
  unsigned char angle_menu_call                : 1;
  unsigned char audio_menu_call                : 1;
  unsigned char subpic_menu_call               : 1;
  unsigned char root_menu_call                 : 1;
  unsigned char title_menu_call                : 1;
  unsigned char backward_scan                  : 1;
  unsigned char forward_scan                   : 1; /* 8 */
 
  unsigned char next_pg_search                 : 1; /* 7 */
  unsigned char prev_or_top_pg_search          : 1;
  unsigned char time_or_chapter_search         : 1;
  unsigned char go_up                          : 1;
  unsigned char stop                           : 1;
  unsigned char title_play                     : 1;
  unsigned char chapter_search_or_play         : 1;
  unsigned char title_or_time_play             : 1; /* 0 */
#else
  unsigned char video_pres_mode_change         : 1; /* 24 */
  unsigned char zero                           : 7; /* 25-31 */
 
  unsigned char resume                         : 1; /* 16 */
  unsigned char button_select_or_activate      : 1;
  unsigned char still_off                      : 1;
  unsigned char pause_on                       : 1;
  unsigned char audio_stream_change            : 1;
  unsigned char subpic_stream_change           : 1;
  unsigned char angle_change                   : 1;
  unsigned char karaoke_audio_pres_mode_change : 1; /* 23 */
 
  unsigned char forward_scan                   : 1; /* 8 */
  unsigned char backward_scan                  : 1;
  unsigned char title_menu_call                : 1;
  unsigned char root_menu_call                 : 1;
  unsigned char subpic_menu_call               : 1;
  unsigned char audio_menu_call                : 1;
  unsigned char angle_menu_call                : 1;
  unsigned char chapter_menu_call              : 1; /* 15 */
 
  unsigned char title_or_time_play             : 1; /* 0 */
  unsigned char chapter_search_or_play         : 1;
  unsigned char title_play                     : 1;
  unsigned char stop                           : 1;
  unsigned char go_up                          : 1;
  unsigned char time_or_chapter_search         : 1;
  unsigned char prev_or_top_pg_search          : 1;
  unsigned char next_pg_search                 : 1; /* 7 */
#endif
} ATTRIBUTE_PACKED user_ops_t;

/**
 * Type to store per-command data.
 */
typedef struct {
  uint8_t bytes[8];
} ATTRIBUTE_PACKED vm_cmd_t;
#define COMMAND_DATA_SIZE 8

/**
 * PCI General Information
 */
typedef struct {
  uint32_t nv_pck_lbn;      /**< sector address of this nav pack */
  uint16_t vobu_cat;        /**< 'category' of vobu */
  uint16_t zero1;           /**< reserved */
  user_ops_t vobu_uop_ctl;  /**< UOP of vobu */
  uint32_t vobu_s_ptm;      /**< start presentation time of vobu */
  uint32_t vobu_e_ptm;      /**< end presentation time of vobu */
  uint32_t vobu_se_e_ptm;   /**< end ptm of sequence end in vobu */
  dvd_time_t e_eltm;        /**< Cell elapsed time */
  char vobu_isrc[32];
} ATTRIBUTE_PACKED pci_gi_t;

/**
 * Non Seamless Angle Information
 */
typedef struct {
  uint32_t nsml_agl_dsta[9];  /**< address of destination vobu in AGL_C#n */
} ATTRIBUTE_PACKED nsml_agli_t;

/**
 * Highlight General Information
 *
 * For btngrX_dsp_ty the bits have the following meaning:
 * 000b: normal 4/3 only buttons
 * XX1b: wide (16/9) buttons
 * X1Xb: letterbox buttons
 * 1XXb: pan&scan buttons
 */
typedef struct {
  uint16_t hli_ss; /**< status, only low 2 bits 0: no buttons, 1: different 2: equal 3: eual except for button cmds */
  uint32_t hli_s_ptm;              /**< start ptm of hli */
  uint32_t hli_e_ptm;              /**< end ptm of hli */
  uint32_t btn_se_e_ptm;           /**< end ptm of button select */
#ifdef WORDS_BIGENDIAN
  unsigned char zero1 : 2;          /**< reserved */
  unsigned char btngr_ns : 2;       /**< number of button groups 1, 2 or 3 with 36/18/12 buttons */
  unsigned char zero2 : 1;          /**< reserved */
  unsigned char btngr1_dsp_ty : 3;  /**< display type of subpic stream for button group 1 */
  unsigned char zero3 : 1;          /**< reserved */
  unsigned char btngr2_dsp_ty : 3;  /**< display type of subpic stream for button group 2 */
  unsigned char zero4 : 1;          /**< reserved */
  unsigned char btngr3_dsp_ty : 3;  /**< display type of subpic stream for button group 3 */
#else
  unsigned char btngr1_dsp_ty : 3;
  unsigned char zero2 : 1;
  unsigned char btngr_ns : 2;
  unsigned char zero1 : 2;
  unsigned char btngr3_dsp_ty : 3;
  unsigned char zero4 : 1;
  unsigned char btngr2_dsp_ty : 3;
  unsigned char zero3 : 1;
#endif
  uint8_t btn_ofn;     /**< button offset number range 0-255 */
  uint8_t btn_ns;      /**< number of valid buttons  <= 36/18/12 (low 6 bits) */
  uint8_t nsl_btn_ns;  /**< number of buttons selectable by U_BTNNi (low 6 bits)   nsl_btn_ns <= btn_ns */
  uint8_t zero5;       /**< reserved */
  uint8_t fosl_btnn;   /**< forcedly selected button  (low 6 bits) */
  uint8_t foac_btnn;   /**< forcedly activated button (low 6 bits) */
} ATTRIBUTE_PACKED hl_gi_t;


/**
 * Button Color Information Table
 * Each entry beeing a 32bit word that contains the color indexs and alpha
 * values to use.  They are all represented by 4 bit number and stored
 * like this [Ci3, Ci2, Ci1, Ci0, A3, A2, A1, A0].   The actual palette
 * that the indexes reference is in the PGC.
 * \todo split the uint32_t into a struct
 */
typedef struct {
  uint32_t btn_coli[3][2];  /**< [button color number-1][select:0/action:1] */
} ATTRIBUTE_PACKED btn_colit_t;

/**
 * Button Information
 *
 * NOTE: I've had to change the structure from the disk layout to get
 * the packing to work with Sun's Forte C compiler.
 * The 4 and 7 bytes are 'rotated' was: ABC DEF GHIJ  is: ABCG DEFH IJ
 */
typedef struct {
#ifdef WORDS_BIGENDIAN
  uint32        btn_coln         : 2;  /**< button color number */
  uint32        x_start          : 10; /**< x start offset within the overlay */
  uint32        zero1            : 2;  /**< reserved */
  uint32        x_end            : 10; /**< x end offset within the overlay */

  uint32        zero3            : 2;  /**< reserved */
  uint32        up               : 6;  /**< button index when pressing up */

  uint32        auto_action_mode : 2;  /**< 0: no, 1: activated if selected */
  uint32        y_start          : 10; /**< y start offset within the overlay */
  uint32        zero2            : 2;  /**< reserved */
  uint32        y_end            : 10; /**< y end offset within the overlay */

  uint32        zero4            : 2;  /**< reserved */
  uint32        down             : 6;  /**< button index when pressing down */
  unsigned char zero5            : 2;  /**< reserved */
  unsigned char left             : 6;  /**< button index when pressing left */
  unsigned char zero6            : 2;  /**< reserved */
  unsigned char right            : 6;  /**< button index when pressing right */
#else
  uint32        x_end            : 10;
  uint32        zero1            : 2;
  uint32        x_start          : 10;
  uint32        btn_coln         : 2;

  uint32        up               : 6;
  uint32        zero3            : 2;

  uint32        y_end            : 10;
  uint32        zero2            : 2;
  uint32        y_start          : 10;
  uint32        auto_action_mode : 2;

  uint32        down             : 6;
  uint32        zero4            : 2;
  unsigned char left             : 6;
  unsigned char zero5            : 2;
  unsigned char right            : 6;
  unsigned char zero6            : 2;
#endif
  vm_cmd_t cmd;
} ATTRIBUTE_PACKED btni_t;

/**
 * Highlight Information
 */
typedef struct {
  hl_gi_t     hl_gi;
  btn_colit_t btn_colit;
  btni_t      btnit[36];
} ATTRIBUTE_PACKED hli_t;

/**
 * PCI packet
 */
typedef struct {
  pci_gi_t    pci_gi;
  nsml_agli_t nsml_agli;
  hli_t       hli;
  uint8_t     zero1[189];
} ATTRIBUTE_PACKED pci_t;


#if PRAGMA_PACK
#pragma pack()
#endif
////////////////////////////////////////////////////////////////////////////////////////////////////////////////


/**
 * What's between a directory and a filename?
 */
#if defined( WIN32 )
    #define DIRECTORY_SEPARATOR '\\'
#else
    #define DIRECTORY_SEPARATOR '/'
#endif

using namespace LIBMATROSKA_NAMESPACE;
using namespace std;

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
#ifdef HAVE_ZLIB_H
block_t *block_zlib_decompress( vlc_object_t *p_this, block_t *p_in_block ) {
    int result, dstsize, n;
    unsigned char *dst;
    block_t *p_block;
    z_stream d_stream;

    d_stream.zalloc = (alloc_func)0;
    d_stream.zfree = (free_func)0;
    d_stream.opaque = (voidpf)0;
    result = inflateInit(&d_stream);
    if( result != Z_OK )
    {
        msg_Dbg( p_this, "inflateInit() failed. Result: %d", result );
        return NULL;
    }

    d_stream.next_in = (Bytef *)p_in_block->p_buffer;
    d_stream.avail_in = p_in_block->i_buffer;
    n = 0;
    p_block = block_New( p_this, 0 );
    dst = NULL;
    do
    {
        n++;
        p_block = block_Realloc( p_block, 0, n * 1000 );
        dst = (unsigned char *)p_block->p_buffer;
        d_stream.next_out = (Bytef *)&dst[(n - 1) * 1000];
        d_stream.avail_out = 1000;
        result = inflate(&d_stream, Z_NO_FLUSH);
        if( ( result != Z_OK ) && ( result != Z_STREAM_END ) )
        {
            msg_Dbg( p_this, "Zlib decompression failed. Result: %d", result );
            return NULL;
        }
    }
    while( ( d_stream.avail_out == 0 ) && ( d_stream.avail_in != 0 ) &&
           ( result != Z_STREAM_END ) );

    dstsize = d_stream.total_out;
    inflateEnd( &d_stream );

    p_block = block_Realloc( p_block, 0, dstsize );
    p_block->i_buffer = dstsize;
    block_Release( p_in_block );

    return p_block;
}
#endif

/**
 * Helper function to print the mkv parse tree
 */
static void MkvTree( demux_t & demuxer, int i_level, const char *psz_format, ... )
{
    va_list args;
    if( i_level > 9 )
    {
        msg_Err( &demuxer, "too deep tree" );
        return;
    }
    va_start( args, psz_format );
    static const char psz_foo[] = "|   |   |   |   |   |   |   |   |   |";
    char *psz_foo2 = (char*)malloc( ( i_level * 4 + 3 + strlen( psz_format ) ) * sizeof(char) );
    strncpy( psz_foo2, psz_foo, 4 * i_level );
    psz_foo2[ 4 * i_level ] = '+';
    psz_foo2[ 4 * i_level + 1 ] = ' ';
    strcpy( &psz_foo2[ 4 * i_level + 2 ], psz_format );
    __msg_GenericVa( VLC_OBJECT(&demuxer),VLC_MSG_DBG, "mkv", psz_foo2, args );
    free( psz_foo2 );
    va_end( args );
}

/*****************************************************************************
 * Stream managment
 *****************************************************************************/
class vlc_stream_io_callback: public IOCallback
{
  private:
    stream_t       *s;
    bool           mb_eof;
    bool           b_owner;

  public:
    vlc_stream_io_callback( stream_t *, bool );

    virtual ~vlc_stream_io_callback()
    {
        if( b_owner )
            stream_Delete( s );
    }

    virtual uint32   read            ( void *p_buffer, size_t i_size);
    virtual void     setFilePointer  ( int64_t i_offset, seek_mode mode = seek_beginning );
    virtual size_t   write           ( const void *p_buffer, size_t i_size);
    virtual uint64   getFilePointer  ( void );
    virtual void     close           ( void );
};

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


/*****************************************************************************
 * Some functions to manipulate memory
 *****************************************************************************/
#define GetFOURCC( p )  __GetFOURCC( (uint8_t*)p )
static vlc_fourcc_t __GetFOURCC( uint8_t *p )
{
    return VLC_FOURCC( p[0], p[1], p[2], p[3] );
}

/*****************************************************************************
 * definitions of structures and functions used by this plugins
 *****************************************************************************/
typedef struct
{
//    ~mkv_track_t();

    bool         b_default;
    bool         b_enabled;
    unsigned int i_number;

    int          i_extra_data;
    uint8_t      *p_extra_data;

    char         *psz_codec;

    uint64_t     i_default_duration;
    float        f_timecodescale;
    mtime_t      i_last_dts;

    /* video */
    es_format_t fmt;
    float       f_fps;
    es_out_id_t *p_es;

    /* audio */
    unsigned int i_original_rate;

    bool            b_inited;
    /* data to be send first */
    int             i_data_init;
    uint8_t         *p_data_init;

    /* hack : it's for seek */
    bool            b_search_keyframe;
    bool            b_silent;

    /* informative */
    const char   *psz_codec_name;
    const char   *psz_codec_settings;
    const char   *psz_codec_info_url;
    const char   *psz_codec_download_url;

    /* encryption/compression */
    int                    i_compression_type;
    KaxContentCompSettings *p_compression_data;

} mkv_track_t;

typedef struct
{
    int     i_track;
    int     i_block_number;

    int64_t i_position;
    int64_t i_time;

    bool       b_key;
} mkv_index_t;

class demux_sys_t;

const binary MATROSKA_DVD_LEVEL_SS   = 0x30;
const binary MATROSKA_DVD_LEVEL_LU   = 0x2A;
const binary MATROSKA_DVD_LEVEL_TT   = 0x28;
const binary MATROSKA_DVD_LEVEL_PGC  = 0x20;
const binary MATROSKA_DVD_LEVEL_PG   = 0x18;
const binary MATROSKA_DVD_LEVEL_PTT  = 0x10;
const binary MATROSKA_DVD_LEVEL_CN   = 0x08;

class chapter_codec_cmds_c
{
public:
    chapter_codec_cmds_c( demux_sys_t & demuxer, int codec_id = -1)
    :p_private_data(NULL)
    ,i_codec_id( codec_id )
    ,sys( demuxer )
    {}
 
    virtual ~chapter_codec_cmds_c()
    {
        delete p_private_data;
        std::vector<KaxChapterProcessData*>::iterator indexe = enter_cmds.begin();
        while ( indexe != enter_cmds.end() )
        {
            delete (*indexe);
            indexe++;
        }
        std::vector<KaxChapterProcessData*>::iterator indexl = leave_cmds.begin();
        while ( indexl != leave_cmds.end() )
        {
            delete (*indexl);
            indexl++;
        }
        std::vector<KaxChapterProcessData*>::iterator indexd = during_cmds.begin();
        while ( indexd != during_cmds.end() )
        {
            delete (*indexd);
            indexd++;
        }
    }

    void SetPrivate( const KaxChapterProcessPrivate & private_data )
    {
        p_private_data = new KaxChapterProcessPrivate( private_data );
    }

    void AddCommand( const KaxChapterProcessCommand & command );
 
    /// \return wether the codec has seeked in the files or not
    virtual bool Enter() { return false; }
    virtual bool Leave() { return false; }
    virtual std::string GetCodecName( bool f_for_title = false ) const { return ""; }
    virtual int16 GetTitleNumber() { return -1; }

    KaxChapterProcessPrivate *p_private_data;

protected:
    std::vector<KaxChapterProcessData*> enter_cmds;
    std::vector<KaxChapterProcessData*> during_cmds;
    std::vector<KaxChapterProcessData*> leave_cmds;

    int i_codec_id;
    demux_sys_t & sys;
};

class dvd_command_interpretor_c
{
public:
    dvd_command_interpretor_c( demux_sys_t & demuxer )
    :sys( demuxer )
    {
        memset( p_PRMs, 0, sizeof(p_PRMs) );
        p_PRMs[ 0x80 + 1 ] = 15;
        p_PRMs[ 0x80 + 2 ] = 62;
        p_PRMs[ 0x80 + 3 ] = 1;
        p_PRMs[ 0x80 + 4 ] = 1;
        p_PRMs[ 0x80 + 7 ] = 1;
        p_PRMs[ 0x80 + 8 ] = 1;
        p_PRMs[ 0x80 + 16 ] = 0xFFFFu;
        p_PRMs[ 0x80 + 18 ] = 0xFFFFu;
    }
 
    bool Interpret( const binary * p_command, size_t i_size = 8 );
 
    uint16 GetPRM( size_t index ) const
    {
        if ( index < 256 )
            return p_PRMs[ index ];
        else return 0;
    }

    uint16 GetGPRM( size_t index ) const
    {
        if ( index >= 0 && index < 16 )
            return p_PRMs[ index ];
        else return 0;
    }

    uint16 GetSPRM( size_t index ) const
    {
        // 21,22,23 reserved for future use
        if ( index >= 0x80 && index < 0x95 )
            return p_PRMs[ index ];
        else return 0;
    }

    bool SetPRM( size_t index, uint16 value )
    {
        if ( index >= 0 && index < 16 )
        {
            p_PRMs[ index ] = value;
            return true;
        }
        return false;
    }
 
    bool SetGPRM( size_t index, uint16 value )
    {
        if ( index >= 0 && index < 16 )
        {
            p_PRMs[ index ] = value;
            return true;
        }
        return false;
    }

    bool SetSPRM( size_t index, uint16 value )
    {
        if ( index > 0x80 && index <= 0x8D && index != 0x8C )
        {
            p_PRMs[ index ] = value;
            return true;
        }
        return false;
    }

protected:
    std::string GetRegTypeName( bool b_value, uint16 value ) const
    {
        std::string result;
        char s_value[6], s_reg_value[6];
        sprintf( s_value, "%.5d", value );

        if ( b_value )
        {
            result = "value (";
            result += s_value;
            result += ")";
        }
        else if ( value < 0x80 )
        {
            sprintf( s_reg_value, "%.5d", GetPRM( value ) );
            result = "GPreg[";
            result += s_value;
            result += "] (";
            result += s_reg_value;
            result += ")";
        }
        else
        {
            sprintf( s_reg_value, "%.5d", GetPRM( value ) );
            result = "SPreg[";
            result += s_value;
            result += "] (";
            result += s_reg_value;
            result += ")";
        }
        return result;
    }

    uint16       p_PRMs[256];
    demux_sys_t  & sys;
 
    // DVD command IDs

    // Tests
    // wether it's a comparison on the value or register
    static const uint16 CMD_DVD_TEST_VALUE          = 0x80;
    static const uint16 CMD_DVD_IF_GPREG_AND        = (1 << 4);
    static const uint16 CMD_DVD_IF_GPREG_EQUAL      = (2 << 4);
    static const uint16 CMD_DVD_IF_GPREG_NOT_EQUAL  = (3 << 4);
    static const uint16 CMD_DVD_IF_GPREG_SUP_EQUAL  = (4 << 4);
    static const uint16 CMD_DVD_IF_GPREG_SUP        = (5 << 4);
    static const uint16 CMD_DVD_IF_GPREG_INF_EQUAL  = (6 << 4);
    static const uint16 CMD_DVD_IF_GPREG_INF        = (7 << 4);
 
    static const uint16 CMD_DVD_NOP                    = 0x0000;
    static const uint16 CMD_DVD_GOTO_LINE              = 0x0001;
    static const uint16 CMD_DVD_BREAK                  = 0x0002;
    // Links
    static const uint16 CMD_DVD_NOP2                   = 0x2001;
    static const uint16 CMD_DVD_LINKPGCN               = 0x2004;
    static const uint16 CMD_DVD_LINKPGN                = 0x2006;
    static const uint16 CMD_DVD_LINKCN                 = 0x2007;
    static const uint16 CMD_DVD_JUMP_TT                = 0x3002;
    static const uint16 CMD_DVD_JUMPVTS_TT             = 0x3003;
    static const uint16 CMD_DVD_JUMPVTS_PTT            = 0x3005;
    static const uint16 CMD_DVD_JUMP_SS                = 0x3006;
    static const uint16 CMD_DVD_CALLSS_VTSM1           = 0x3008;
    //
    static const uint16 CMD_DVD_SET_HL_BTNN2           = 0x4600;
    static const uint16 CMD_DVD_SET_HL_BTNN_LINKPGCN1  = 0x4604;
    static const uint16 CMD_DVD_SET_STREAM             = 0x5100;
    static const uint16 CMD_DVD_SET_GPRMMD             = 0x5300;
    static const uint16 CMD_DVD_SET_HL_BTNN1           = 0x5600;
    static const uint16 CMD_DVD_SET_HL_BTNN_LINKPGCN2  = 0x5604;
    static const uint16 CMD_DVD_SET_HL_BTNN_LINKCN     = 0x5607;
    // Operations
    static const uint16 CMD_DVD_MOV_SPREG_PREG         = 0x6100;
    static const uint16 CMD_DVD_GPREG_MOV_VALUE        = 0x7100;
    static const uint16 CMD_DVD_SUB_GPREG              = 0x7400;
    static const uint16 CMD_DVD_MULT_GPREG             = 0x7500;
    static const uint16 CMD_DVD_GPREG_DIV_VALUE        = 0x7600;
    static const uint16 CMD_DVD_GPREG_AND_VALUE        = 0x7900;
 
    // callbacks when browsing inside CodecPrivate
    static bool MatchIsDomain     ( const chapter_codec_cmds_c &data, const void *p_cookie, size_t i_cookie_size );
    static bool MatchIsVMG        ( const chapter_codec_cmds_c &data, const void *p_cookie, size_t i_cookie_size );
    static bool MatchVTSNumber    ( const chapter_codec_cmds_c &data, const void *p_cookie, size_t i_cookie_size );
    static bool MatchVTSMNumber   ( const chapter_codec_cmds_c &data, const void *p_cookie, size_t i_cookie_size );
    static bool MatchTitleNumber  ( const chapter_codec_cmds_c &data, const void *p_cookie, size_t i_cookie_size );
    static bool MatchPgcType      ( const chapter_codec_cmds_c &data, const void *p_cookie, size_t i_cookie_size );
    static bool MatchPgcNumber    ( const chapter_codec_cmds_c &data, const void *p_cookie, size_t i_cookie_size );
    static bool MatchChapterNumber( const chapter_codec_cmds_c &data, const void *p_cookie, size_t i_cookie_size );
    static bool MatchCellNumber   ( const chapter_codec_cmds_c &data, const void *p_cookie, size_t i_cookie_size );
};

class dvd_chapter_codec_c : public chapter_codec_cmds_c
{
public:
    dvd_chapter_codec_c( demux_sys_t & sys )
    :chapter_codec_cmds_c( sys, 1 )
    {}

    bool Enter();
    bool Leave();
    std::string GetCodecName( bool f_for_title = false ) const;
    int16 GetTitleNumber();
};

class matroska_script_interpretor_c
{
public:
    matroska_script_interpretor_c( demux_sys_t & demuxer )
    :sys( demuxer )
    {}

    bool Interpret( const binary * p_command, size_t i_size );
 
    // DVD command IDs
    static const std::string CMD_MS_GOTO_AND_PLAY;
 
protected:
    demux_sys_t  & sys;
};

const std::string matroska_script_interpretor_c::CMD_MS_GOTO_AND_PLAY = "GotoAndPlay";


class matroska_script_codec_c : public chapter_codec_cmds_c
{
public:
    matroska_script_codec_c( demux_sys_t & sys )
    :chapter_codec_cmds_c( sys, 0 )
    ,interpretor( sys )
    {}

    bool Enter();
    bool Leave();

protected:
    matroska_script_interpretor_c interpretor;
};

class chapter_translation_c
{
public:
    chapter_translation_c()
        :p_translated(NULL)
    {}

    ~chapter_translation_c()
    {
        delete p_translated;
    }

    KaxChapterTranslateID  *p_translated;
    unsigned int           codec_id;
    std::vector<uint64_t>  editions;
};

class chapter_item_c
{
public:
    chapter_item_c()
    :i_start_time(0)
    ,i_end_time(-1)
    ,i_user_start_time(-1)
    ,i_user_end_time(-1)
    ,i_seekpoint_num(-1)
    ,b_display_seekpoint(true)
    ,b_user_display(false)
    ,psz_parent(NULL)
    ,b_is_leaving(false)
    {}

    virtual ~chapter_item_c()
    {
        std::vector<chapter_codec_cmds_c*>::iterator index = codecs.begin();
        while ( index != codecs.end() )
        {
            delete (*index);
            index++;
        }
        std::vector<chapter_item_c*>::iterator index_ = sub_chapters.begin();
        while ( index_ != sub_chapters.end() )
        {
            delete (*index_);
            index_++;
        }
    }

    int64_t RefreshChapters( bool b_ordered, int64_t i_prev_user_time );
    int PublishChapters( input_title_t & title, int & i_user_chapters, int i_level = 0 );
    virtual chapter_item_c * FindTimecode( mtime_t i_timecode, const chapter_item_c * p_current, bool & b_found );
    void Append( const chapter_item_c & edition );
    chapter_item_c * FindChapter( int64_t i_find_uid );
    virtual chapter_item_c *BrowseCodecPrivate( unsigned int codec_id,
                                    bool (*match)(const chapter_codec_cmds_c &data, const void *p_cookie, size_t i_cookie_size ),
                                    const void *p_cookie,
                                    size_t i_cookie_size );
    std::string                 GetCodecName( bool f_for_title = false ) const;
    bool                        ParentOf( const chapter_item_c & item ) const;
    int16                       GetTitleNumber( ) const;
 
    int64_t                     i_start_time, i_end_time;
    int64_t                     i_user_start_time, i_user_end_time; /* the time in the stream when an edition is ordered */
    std::vector<chapter_item_c*> sub_chapters;
    int                         i_seekpoint_num;
    int64_t                     i_uid;
    bool                        b_display_seekpoint;
    bool                        b_user_display;
    std::string                 psz_name;
    chapter_item_c              *psz_parent;
    bool                        b_is_leaving;
 
    std::vector<chapter_codec_cmds_c*> codecs;

    static bool CompareTimecode( const chapter_item_c * itemA, const chapter_item_c * itemB )
    {
        return ( itemA->i_user_start_time < itemB->i_user_start_time || (itemA->i_user_start_time == itemB->i_user_start_time && itemA->i_user_end_time < itemB->i_user_end_time) );
    }

    bool Enter( bool b_do_subchapters );
    bool Leave( bool b_do_subchapters );
    bool EnterAndLeave( chapter_item_c *p_item, bool b_enter = true );
};

class chapter_edition_c : public chapter_item_c
{
public:
    chapter_edition_c()
    :b_ordered(false)
    {}
 
    void RefreshChapters( );
    mtime_t Duration() const;
    std::string GetMainName() const;
    chapter_item_c * FindTimecode( mtime_t i_timecode, const chapter_item_c * p_current );
 
    bool                        b_ordered;
};

class matroska_segment_c
{
public:
    matroska_segment_c( demux_sys_t & demuxer, EbmlStream & estream )
        :segment(NULL)
        ,es(estream)
        ,i_timescale(MKVD_TIMECODESCALE)
        ,i_duration(-1)
        ,i_start_time(0)
        ,i_cues_position(-1)
        ,i_info_position(-1)
        ,i_chapters_position(-1)
        ,i_tags_position(-1)
        ,i_tracks_position(-1)
        ,i_attachments_position(-1)
        ,i_seekhead_position(-1)
        ,i_seekhead_count(0)
        ,cluster(NULL)
        ,i_block_pos(0)
        ,i_cluster_pos(0)
        ,i_start_pos(0)
        ,p_segment_uid(NULL)
        ,p_prev_segment_uid(NULL)
        ,p_next_segment_uid(NULL)
        ,b_cues(false)
        ,i_index(0)
        ,i_index_max(1024)
        ,psz_muxing_application(NULL)
        ,psz_writing_application(NULL)
        ,psz_segment_filename(NULL)
        ,psz_title(NULL)
        ,psz_date_utc(NULL)
        ,i_default_edition(0)
        ,sys(demuxer)
        ,ep(NULL)
        ,b_preloaded(false)
    {
        p_indexes = (mkv_index_t*)malloc( sizeof( mkv_index_t ) * i_index_max );
    }

    virtual ~matroska_segment_c()
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

    KaxSegment              *segment;
    EbmlStream              & es;

    /* time scale */
    uint64_t                i_timescale;

    /* duration of the segment */
    mtime_t                 i_duration;
    mtime_t                 i_start_time;

    /* all tracks */
    std::vector<mkv_track_t*> tracks;

    /* from seekhead */
    int                     i_seekhead_count;
    int64_t                 i_seekhead_position;
    int64_t                 i_cues_position;
    int64_t                 i_tracks_position;
    int64_t                 i_info_position;
    int64_t                 i_chapters_position;
    int64_t                 i_tags_position;
    int64_t                 i_attachments_position;

    KaxCluster              *cluster;
    uint64                  i_block_pos;
    uint64                  i_cluster_pos;
    int64_t                 i_start_pos;
    KaxSegmentUID           *p_segment_uid;
    KaxPrevUID              *p_prev_segment_uid;
    KaxNextUID              *p_next_segment_uid;

    bool                    b_cues;
    int                     i_index;
    int                     i_index_max;
    mkv_index_t             *p_indexes;

    /* info */
    char                    *psz_muxing_application;
    char                    *psz_writing_application;
    char                    *psz_segment_filename;
    char                    *psz_title;
    char                    *psz_date_utc;

    /* !!!!! GCC 3.3 bug on Darwin !!!!! */
    /* when you remove this variable the compiler issues an atomicity error */
    /* this variable only works when using std::vector<chapter_edition_c> */
    std::vector<chapter_edition_c*> stored_editions;
    int                             i_default_edition;

    std::vector<chapter_translation_c*> translations;
    std::vector<KaxSegmentFamily*>  families;
 
    demux_sys_t                    & sys;
    EbmlParser                     *ep;
    bool                           b_preloaded;

    bool Preload( );
    bool LoadSeekHeadItem( const EbmlCallbacks & ClassInfos, int64_t i_element_position );
    bool PreloadFamily( const matroska_segment_c & segment );
    void ParseInfo( KaxInfo *info );
    void ParseAttachments( KaxAttachments *attachments );
    void ParseChapters( KaxChapters *chapters );
    void ParseSeekHead( KaxSeekHead *seekhead );
    void ParseTracks( KaxTracks *tracks );
    void ParseChapterAtom( int i_level, KaxChapterAtom *ca, chapter_item_c & chapters );
    void ParseTrackEntry( KaxTrackEntry *m );
    void ParseCluster( );
    void IndexAppendCluster( KaxCluster *cluster );
    void LoadCues( KaxCues *cues );
    void LoadTags( KaxTags *tags );
    void InformationCreate( );
    void Seek( mtime_t i_date, mtime_t i_time_offset, int64_t i_global_position );
    int BlockGet( KaxBlock * &, KaxSimpleBlock * &, int64_t *, int64_t *, int64_t *);

    int BlockFindTrackIndex( size_t *pi_track,
                             const KaxBlock *, const KaxSimpleBlock * );


    bool Select( mtime_t i_start_time );
    void UnSelect( );

    static bool CompareSegmentUIDs( const matroska_segment_c * item_a, const matroska_segment_c * item_b );
};

// class holding hard-linked segment together in the playback order
class virtual_segment_c
{
public:
    virtual_segment_c( matroska_segment_c *p_segment )
        :p_editions(NULL)
        ,i_sys_title(0)
        ,i_current_segment(0)
        ,i_current_edition(-1)
        ,psz_current_chapter(NULL)
    {
        linked_segments.push_back( p_segment );

        AppendUID( p_segment->p_segment_uid );
        AppendUID( p_segment->p_prev_segment_uid );
        AppendUID( p_segment->p_next_segment_uid );
    }

    void Sort();
    size_t AddSegment( matroska_segment_c *p_segment );
    void PreloadLinked( );
    mtime_t Duration( ) const;
    void Seek( demux_t & demuxer, mtime_t i_date, mtime_t i_time_offset, chapter_item_c *psz_chapter, int64_t i_global_position );

    inline chapter_edition_c *Edition()
    {
        if ( i_current_edition >= 0 && size_t(i_current_edition) < p_editions->size() )
            return (*p_editions)[i_current_edition];
        return NULL;
    }

    matroska_segment_c * Segment() const
    {
        if ( linked_segments.size() == 0 || i_current_segment >= linked_segments.size() )
            return NULL;
        return linked_segments[i_current_segment];
    }

    inline chapter_item_c *CurrentChapter() {
        return psz_current_chapter;
    }

    bool SelectNext()
    {
        if ( i_current_segment < linked_segments.size()-1 )
        {
            i_current_segment++;
            return true;
        }
        return false;
    }

    bool FindUID( KaxSegmentUID & uid ) const
    {
        for ( size_t i=0; i<linked_uids.size(); i++ )
        {
            if ( linked_uids[i] == uid )
                return true;
        }
        return false;
    }

    bool UpdateCurrentToChapter( demux_t & demux );
    void PrepareChapters( );

    chapter_item_c *BrowseCodecPrivate( unsigned int codec_id,
                                        bool (*match)(const chapter_codec_cmds_c &data, const void *p_cookie, size_t i_cookie_size ),
                                        const void *p_cookie,
                                        size_t i_cookie_size );
    chapter_item_c *FindChapter( int64_t i_find_uid );

    std::vector<chapter_edition_c*>  *p_editions;
    int                              i_sys_title;

protected:
    std::vector<matroska_segment_c*> linked_segments;
    std::vector<KaxSegmentUID>       linked_uids;
    size_t                           i_current_segment;

    int                              i_current_edition;
    chapter_item_c                   *psz_current_chapter;

    void                             AppendUID( const EbmlBinary * UID );
};

class matroska_stream_c
{
public:
    matroska_stream_c( demux_sys_t & demuxer )
        :p_in(NULL)
        ,p_es(NULL)
        ,sys(demuxer)
    {}

    virtual ~matroska_stream_c()
    {
        delete p_in;
        delete p_es;
    }

    IOCallback         *p_in;
    EbmlStream         *p_es;

    std::vector<matroska_segment_c*> segments;

    demux_sys_t                      & sys;
};

typedef struct
{
    VLC_COMMON_MEMBERS

    demux_t        *p_demux;
    vlc_mutex_t     lock;

    bool            b_moved;
    bool            b_clicked;
    int             i_key_action;

} event_thread_t;


class attachment_c
{
public:
    attachment_c()
        :p_data(NULL)
        ,i_size(0)
    {}
    virtual ~attachment_c()
    {
        free( p_data );
    }

    std::string    psz_file_name;
    std::string    psz_mime_type;
    void          *p_data;
    int            i_size;
};

class demux_sys_t
{
public:
    demux_sys_t( demux_t & demux )
        :demuxer(demux)
        ,i_pts(0)
        ,i_start_pts(0)
        ,i_chapter_time(0)
        ,meta(NULL)
        ,i_current_title(0)
        ,p_current_segment(NULL)
        ,dvd_interpretor( *this )
        ,f_duration(-1.0)
        ,b_ui_hooked(false)
        ,p_input(NULL)
        ,b_pci_packet_set(false)
        ,p_ev(NULL)
    {
        vlc_mutex_init( &lock_demuxer );
    }

    virtual ~demux_sys_t()
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

    /* current data */
    demux_t                 & demuxer;

    mtime_t                 i_pts;
    mtime_t                 i_start_pts;
    mtime_t                 i_chapter_time;

    vlc_meta_t              *meta;

    std::vector<input_title_t*>      titles; // matroska editions
    size_t                           i_current_title;

    std::vector<matroska_stream_c*>  streams;
    std::vector<attachment_c*>       stored_attachments;
    std::vector<matroska_segment_c*> opened_segments;
    std::vector<virtual_segment_c*>  used_segments;
    virtual_segment_c                *p_current_segment;

    dvd_command_interpretor_c        dvd_interpretor;

    /* duration of the stream */
    float                   f_duration;

    matroska_segment_c *FindSegment( const EbmlBinary & uid ) const;
    chapter_item_c *BrowseCodecPrivate( unsigned int codec_id,
                                        bool (*match)(const chapter_codec_cmds_c &data, const void *p_cookie, size_t i_cookie_size ),
                                        const void *p_cookie,
                                        size_t i_cookie_size,
                                        virtual_segment_c * & p_segment_found );
    chapter_item_c *FindChapter( int64_t i_find_uid, virtual_segment_c * & p_segment_found );

    void PreloadFamily( const matroska_segment_c & of_segment );
    void PreloadLinked( matroska_segment_c *p_segment );
    bool PreparePlayback( virtual_segment_c *p_new_segment );
    matroska_stream_c *AnalyseAllSegmentsFound( demux_t *p_demux, EbmlStream *p_estream, bool b_initial = false );
    void JumpTo( virtual_segment_c & p_segment, chapter_item_c * p_chapter );

    void StartUiThread();
    void StopUiThread();
    bool b_ui_hooked;
    inline void SwapButtons();

    /* for spu variables */
    input_thread_t *p_input;
    pci_t          pci_packet;
    bool           b_pci_packet_set;
    uint8_t        palette[4][4];
    vlc_mutex_t    lock_demuxer;

    /* event */
    event_thread_t *p_ev;
    static void * EventThread( vlc_object_t *p_this );
    static int EventMouse( vlc_object_t *p_this, char const *psz_var,
                       vlc_value_t oldval, vlc_value_t newval, void *p_data );
    static int EventKey( vlc_object_t *p_this, char const *psz_var,
                     vlc_value_t oldval, vlc_value_t newval, void *p_data );



protected:
    virtual_segment_c *VirtualFromSegments( matroska_segment_c *p_segment ) const;
    bool IsUsedSegment( matroska_segment_c &p_segment ) const;
};

static int  Demux  ( demux_t * );
static int  Control( demux_t *, int, va_list );
static void Seek   ( demux_t *, mtime_t i_date, double f_percent, chapter_item_c *psz_chapter );

#define MKV_IS_ID( el, C ) ( EbmlId( (*el) ) == C::ClassInfos.GlobalId )

static inline char * ToUTF8( const UTFstring &u )
{
    return strdup( u.GetUTF8().c_str() );
}

/*****************************************************************************
 * Open: initializes matroska demux structures
 *****************************************************************************/
static int Open( vlc_object_t * p_this )
{
    demux_t            *p_demux = (demux_t*)p_this;
    demux_sys_t        *p_sys;
    matroska_stream_c  *p_stream;
    matroska_segment_c *p_segment;
    const uint8_t      *p_peek;
    std::string         s_path, s_filename;
    vlc_stream_io_callback *p_io_callback;
    EbmlStream         *p_io_stream;

    /* peek the begining */
    if( stream_Peek( p_demux->s, &p_peek, 4 ) < 4 ) return VLC_EGENERIC;

    /* is a valid file */
    if( p_peek[0] != 0x1a || p_peek[1] != 0x45 ||
        p_peek[2] != 0xdf || p_peek[3] != 0xa3 ) return VLC_EGENERIC;

    /* Set the demux function */
    p_demux->pf_demux   = Demux;
    p_demux->pf_control = Control;
    p_demux->p_sys      = p_sys = new demux_sys_t( *p_demux );

    p_io_callback = new vlc_stream_io_callback( p_demux->s, false );
    p_io_stream = new EbmlStream( *p_io_callback );

    if( p_io_stream == NULL )
    {
        msg_Err( p_demux, "failed to create EbmlStream" );
        delete p_io_callback;
        delete p_sys;
        return VLC_EGENERIC;
    }

    p_stream = p_sys->AnalyseAllSegmentsFound( p_demux, p_io_stream, true );
    if( p_stream == NULL )
    {
        msg_Err( p_demux, "cannot find KaxSegment" );
        goto error;
    }
    p_sys->streams.push_back( p_stream );

    p_stream->p_in = p_io_callback;
    p_stream->p_es = p_io_stream;

    for (size_t i=0; i<p_stream->segments.size(); i++)
    {
        p_stream->segments[i]->Preload();
    }

    p_segment = p_stream->segments[0];
    if( p_segment->cluster == NULL )
    {
        msg_Err( p_demux, "cannot find any cluster, damaged file ?" );
        goto error;
    }

    if (config_GetInt( p_demux, "mkv-preload-local-dir" ))
    {
        /* get the files from the same dir from the same family (based on p_demux->psz_path) */
        if (p_demux->psz_path[0] != '\0' && !strcmp(p_demux->psz_access, ""))
        {
            // assume it's a regular file
            // get the directory path
            s_path = p_demux->psz_path;
            if (s_path.at(s_path.length() - 1) == DIRECTORY_SEPARATOR)
            {
                s_path = s_path.substr(0,s_path.length()-1);
            }
            else
            {
                if (s_path.find_last_of(DIRECTORY_SEPARATOR) > 0)
                {
                    s_path = s_path.substr(0,s_path.find_last_of(DIRECTORY_SEPARATOR));
                }
            }

            DIR *p_src_dir = utf8_opendir(s_path.c_str());

            if (p_src_dir != NULL)
            {
                char *psz_file;
                while ((psz_file = utf8_readdir(p_src_dir)) != NULL)
                {
                    if (strlen(psz_file) > 4)
                    {
                        s_filename = s_path + DIRECTORY_SEPARATOR + psz_file;

#ifdef WIN32
                        if (!strcasecmp(s_filename.c_str(), p_demux->psz_path))
#else
                        if (!s_filename.compare(p_demux->psz_path))
#endif
                        {
                            free (psz_file);
                            continue; // don't reuse the original opened file
                        }

#if defined(__GNUC__) && (__GNUC__ < 3)
                        if (!s_filename.compare("mkv", s_filename.length() - 3, 3) ||
                            !s_filename.compare("mka", s_filename.length() - 3, 3))
#else
                        if (!s_filename.compare(s_filename.length() - 3, 3, "mkv") ||
                            !s_filename.compare(s_filename.length() - 3, 3, "mka"))
#endif
                        {
                            // test wether this file belongs to our family
                            const uint8_t *p_peek;
                            bool          file_ok = false;
                            stream_t      *p_file_stream = stream_UrlNew(
                                                            p_demux,
                                                            s_filename.c_str());
                            /* peek the begining */
                            if( p_file_stream &&
                                stream_Peek( p_file_stream, &p_peek, 4 ) >= 4
                                && p_peek[0] == 0x1a && p_peek[1] == 0x45 &&
                                p_peek[2] == 0xdf && p_peek[3] == 0xa3 ) file_ok = true;

                            if ( file_ok )
                            {
                                vlc_stream_io_callback *p_file_io = new vlc_stream_io_callback( p_file_stream, true );
                                EbmlStream *p_estream = new EbmlStream(*p_file_io);

                                p_stream = p_sys->AnalyseAllSegmentsFound( p_demux, p_estream );

                                if ( p_stream == NULL )
                                {
                                    msg_Dbg( p_demux, "the file '%s' will not be used", s_filename.c_str() );
                                    delete p_estream;
                                    delete p_file_io;
                                }
                                else
                                {
                                    p_stream->p_in = p_file_io;
                                    p_stream->p_es = p_estream;
                                    p_sys->streams.push_back( p_stream );
                                }
                            }
                            else
                            {
                                if( p_file_stream ) {
                                    stream_Delete( p_file_stream );
                                }
                                msg_Dbg( p_demux, "the file '%s' cannot be opened", s_filename.c_str() );
                            }
                        }
                    }
                    free (psz_file);
                }
                closedir( p_src_dir );
            }
        }

        p_sys->PreloadFamily( *p_segment );
    }

    p_sys->PreloadLinked( p_segment );

    if ( !p_sys->PreparePlayback( NULL ) )
    {
        msg_Err( p_demux, "cannot use the segment" );
        goto error;
    }

    p_sys->StartUiThread();
 
    return VLC_SUCCESS;

error:
    delete p_sys;
    return VLC_EGENERIC;
}

/*****************************************************************************
 * Close: frees unused data
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    demux_t     *p_demux = (demux_t*)p_this;
    demux_sys_t *p_sys   = p_demux->p_sys;

    delete p_sys;
}

/*****************************************************************************
 * Control:
 *****************************************************************************/
static int Control( demux_t *p_demux, int i_query, va_list args )
{
    demux_sys_t        *p_sys = p_demux->p_sys;
    int64_t     *pi64;
    double      *pf, f;
    int         i_skp;
    size_t      i_idx;

    vlc_meta_t *p_meta;
    input_attachment_t ***ppp_attach;
    int *pi_int;
    int i;

    switch( i_query )
    {
        case DEMUX_GET_ATTACHMENTS:
            ppp_attach = (input_attachment_t***)va_arg( args, input_attachment_t*** );
            pi_int = (int*)va_arg( args, int * );

            if( p_sys->stored_attachments.size() <= 0 )
                return VLC_EGENERIC;

            *pi_int = p_sys->stored_attachments.size();
            *ppp_attach = (input_attachment_t**)malloc( sizeof(input_attachment_t**) *
                                                        p_sys->stored_attachments.size() );
            if( !(*ppp_attach) )
                return VLC_ENOMEM;
            for( i = 0; i < p_sys->stored_attachments.size(); i++ )
            {
                attachment_c *a = p_sys->stored_attachments[i];
                (*ppp_attach)[i] = vlc_input_attachment_New( a->psz_file_name.c_str(), a->psz_mime_type.c_str(), NULL,
                                                             a->p_data, a->i_size );
            }
            return VLC_SUCCESS;

        case DEMUX_GET_META:
            p_meta = (vlc_meta_t*)va_arg( args, vlc_meta_t* );
            vlc_meta_Merge( p_meta, p_sys->meta );
            return VLC_SUCCESS;

        case DEMUX_GET_LENGTH:
            pi64 = (int64_t*)va_arg( args, int64_t * );
            if( p_sys->f_duration > 0.0 )
            {
                *pi64 = (int64_t)(p_sys->f_duration * 1000);
                return VLC_SUCCESS;
            }
            return VLC_EGENERIC;

        case DEMUX_GET_POSITION:
            pf = (double*)va_arg( args, double * );
            if ( p_sys->f_duration > 0.0 )
                *pf = (double)(p_sys->i_pts >= p_sys->i_start_pts ? p_sys->i_pts : p_sys->i_start_pts ) / (1000.0 * p_sys->f_duration);
            return VLC_SUCCESS;

        case DEMUX_SET_POSITION:
            f = (double)va_arg( args, double );
            Seek( p_demux, -1, f, NULL );
            return VLC_SUCCESS;

        case DEMUX_GET_TIME:
            pi64 = (int64_t*)va_arg( args, int64_t * );
            *pi64 = p_sys->i_pts;
            return VLC_SUCCESS;

        case DEMUX_GET_TITLE_INFO:
            if( p_sys->titles.size() > 1 || ( p_sys->titles.size() == 1 && p_sys->titles[0]->i_seekpoint > 0 ) )
            {
                input_title_t ***ppp_title = (input_title_t***)va_arg( args, input_title_t*** );
                int *pi_int    = (int*)va_arg( args, int* );

                *pi_int = p_sys->titles.size();
                *ppp_title = (input_title_t**)malloc( sizeof( input_title_t**) * p_sys->titles.size() );

                for( size_t i = 0; i < p_sys->titles.size(); i++ )
                {
                    (*ppp_title)[i] = vlc_input_title_Duplicate( p_sys->titles[i] );
                }
                return VLC_SUCCESS;
            }
            return VLC_EGENERIC;

        case DEMUX_SET_TITLE:
            /* TODO handle editions as titles */
            i_idx = (int)va_arg( args, int );
            if( i_idx < p_sys->used_segments.size() )
            {
                p_sys->JumpTo( *p_sys->used_segments[i_idx], NULL );
                return VLC_SUCCESS;
            }
            return VLC_EGENERIC;

        case DEMUX_SET_SEEKPOINT:
            i_skp = (int)va_arg( args, int );

            // TODO change the way it works with the << & >> buttons on the UI (+1/-1 instead of a number)
            if( p_sys->titles.size() && i_skp < p_sys->titles[p_sys->i_current_title]->i_seekpoint)
            {
                Seek( p_demux, (int64_t)p_sys->titles[p_sys->i_current_title]->seekpoint[i_skp]->i_time_offset, -1, NULL);
                p_demux->info.i_seekpoint |= INPUT_UPDATE_SEEKPOINT;
                p_demux->info.i_seekpoint = i_skp;
                return VLC_SUCCESS;
            }
            return VLC_EGENERIC;

        case DEMUX_GET_FPS:
            pf = (double *)va_arg( args, double * );
            *pf = 0.0;
            if( p_sys->p_current_segment && p_sys->p_current_segment->Segment() )
            {
                const matroska_segment_c *p_segment = p_sys->p_current_segment->Segment();
                for( size_t i = 0; i < p_segment->tracks.size(); i++ )
                {
                    mkv_track_t *tk = p_segment->tracks[i];
                    if( tk->fmt.i_cat == VIDEO_ES && tk->fmt.video.i_frame_rate_base > 0 )
                    {
                        *pf = (double)tk->fmt.video.i_frame_rate / tk->fmt.video.i_frame_rate_base;
                        break;
                    }
                }
            }
            return VLC_SUCCESS;

        case DEMUX_SET_TIME:
        default:
            return VLC_EGENERIC;
    }
}

int matroska_segment_c::BlockGet( KaxBlock * & pp_block, KaxSimpleBlock * & pp_simpleblock, int64_t *pi_ref1, int64_t *pi_ref2, int64_t *pi_duration )
{
    pp_simpleblock = NULL;
    pp_block = NULL;
    *pi_ref1  = 0;
    *pi_ref2  = 0;

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

            /* update the index */
#define idx p_indexes[i_index - 1]
            if( i_index > 0 && idx.i_time == -1 )
            {
                if ( pp_simpleblock != NULL )
                    idx.i_time        = pp_simpleblock->GlobalTimecode() / (mtime_t)1000;
                else
                    idx.i_time        = (*pp_block).GlobalTimecode() / (mtime_t)1000;
                idx.b_key         = *pi_ref1 == 0 ? true : false;
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
                if( *pi_ref1 == 0 )
                {
                    *pi_ref1 = int64( ref ) * cluster->GlobalTimecodeScale();
                }
                else if( *pi_ref2 == 0 )
                {
                    *pi_ref2 = int64( ref ) * cluster->GlobalTimecodeScale();
                }
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

static block_t *MemToBlock( demux_t *p_demux, uint8_t *p_mem, int i_mem, size_t offset)
{
    block_t *p_block;
    if( !(p_block = block_New( p_demux, i_mem + offset ) ) ) return NULL;
    memcpy( p_block->p_buffer + offset, p_mem, i_mem );
    //p_block->i_rate = p_input->stream.control.i_rate;
    return p_block;
}

static void BlockDecode( demux_t *p_demux, KaxBlock *block, KaxSimpleBlock *simpleblock,
                         mtime_t i_pts, mtime_t i_duration, bool f_mandatory )
{
    demux_sys_t        *p_sys = p_demux->p_sys;
    matroska_segment_c *p_segment = p_sys->p_current_segment->Segment();

    size_t          i_track;
    unsigned int    i;
    bool            b;

    if( p_segment->BlockFindTrackIndex( &i_track, block, simpleblock ) )
    {
        msg_Err( p_demux, "invalid track number" );
        return;
    }

    mkv_track_t *tk = p_segment->tracks[i_track];

    if( tk->fmt.i_cat != NAV_ES && tk->p_es == NULL )
    {
        msg_Err( p_demux, "unknown track number" );
        return;
    }
    if( i_pts + i_duration < p_sys->i_start_pts && tk->fmt.i_cat == AUDIO_ES )
    {
        return; /* discard audio packets that shouldn't be rendered */
    }

    if ( tk->fmt.i_cat != NAV_ES )
    {
        es_out_Control( p_demux->out, ES_OUT_GET_ES_STATE, tk->p_es, &b );

        if( !b )
        {
            tk->b_inited = false;
            return;
        }
    }


    /* First send init data */
    if( !tk->b_inited && tk->i_data_init > 0 )
    {
        block_t *p_init;

        msg_Dbg( p_demux, "sending header (%d bytes)", tk->i_data_init );
        p_init = MemToBlock( p_demux, tk->p_data_init, tk->i_data_init, 0 );
        if( p_init ) es_out_Send( p_demux->out, tk->p_es, p_init );
    }
    tk->b_inited = true;


    for( i = 0;
         (block != NULL && i < block->NumberFrames()) || (simpleblock != NULL && i < simpleblock->NumberFrames());
         i++ )
    {
        block_t *p_block;
        DataBuffer *data;
        if( simpleblock != NULL )
        {
            data = &simpleblock->GetBuffer(i);
            // condition when the DTS is correct (keyframe or B frame == NOT P frame)
            f_mandatory = simpleblock->IsDiscardable() || simpleblock->IsKeyframe();
        }
        else
        {
            data = &block->GetBuffer(i);
        }

        if( tk->i_compression_type == MATROSKA_COMPRESSION_HEADER && tk->p_compression_data != NULL )
            p_block = MemToBlock( p_demux, data->Buffer(), data->Size(), tk->p_compression_data->GetSize() );
        else
            p_block = MemToBlock( p_demux, data->Buffer(), data->Size(), 0 );

        if( p_block == NULL )
        {
            break;
        }

#if defined(HAVE_ZLIB_H)
        if( tk->i_compression_type == MATROSKA_COMPRESSION_ZLIB )
        {
            p_block = block_zlib_decompress( VLC_OBJECT(p_demux), p_block );
        }
        else
#endif
        if( tk->i_compression_type == MATROSKA_COMPRESSION_HEADER )
        {
            memcpy( p_block->p_buffer, tk->p_compression_data->GetBuffer(), tk->p_compression_data->GetSize() );
        }

        if ( tk->fmt.i_cat == NAV_ES )
        {
            // TODO handle the start/stop times of this packet
            if ( p_sys->b_ui_hooked )
            {
                vlc_mutex_lock( &p_sys->p_ev->lock );
                memcpy( &p_sys->pci_packet, &p_block->p_buffer[1], sizeof(pci_t) );
                p_sys->SwapButtons();
                p_sys->b_pci_packet_set = true;
                vlc_mutex_unlock( &p_sys->p_ev->lock );
                block_Release( p_block );
            }
            return;
        }
        // correct timestamping when B frames are used
        if( tk->fmt.i_cat != VIDEO_ES )
        {
            p_block->i_dts = p_block->i_pts = i_pts;
        }
        else
        {
            if( !strcmp( tk->psz_codec, "V_MS/VFW/FOURCC" ) )
            {
                // in VFW we have no idea about B frames
                p_block->i_pts = 0;
                p_block->i_dts = i_pts;
            }
            else
            {
                p_block->i_pts = i_pts;
                if ( f_mandatory )
                    p_block->i_dts = p_block->i_pts;
                else
                    p_block->i_dts = min( i_pts, tk->i_last_dts + (mtime_t)(tk->i_default_duration >> 10));
                p_sys->i_pts = p_block->i_dts;
            }
        }
        tk->i_last_dts = p_block->i_dts;

#if 0
msg_Dbg( p_demux, "block i_dts: %"PRId64" / i_pts: %"PRId64, p_block->i_dts, p_block->i_pts);
#endif
        if( strcmp( tk->psz_codec, "S_VOBSUB" ) )
        {
            p_block->i_length = i_duration * 1000;
        }

        es_out_Send( p_demux->out, tk->p_es, p_block );

        /* use time stamp only for first block */
        i_pts = 0;
    }
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
    if (std::string(doc_type) != "matroska")
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
        if (EbmlId(*p_l0) == KaxSegment::ClassInfos.GlobalId)
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
            p_l0 = p_l0->SkipData(*p_estream, KaxSegment_Context);
    }

    if ( !b_keep_stream )
    {
        delete p_stream1;
        p_stream1 = NULL;
    }

    return p_stream1;
}

bool matroska_segment_c::Select( mtime_t i_start_time )
{
    size_t i_track;

    /* add all es */
    msg_Dbg( &sys.demuxer, "found %d es", (int)tracks.size() );
    sys.b_pci_packet_set = false;

    for( i_track = 0; i_track < tracks.size(); i_track++ )
    {
        if( tracks[i_track]->fmt.i_cat == UNKNOWN_ES )
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
                    tracks[i_track]->fmt.p_extra = malloc( tracks[i_track]->fmt.i_extra );
                    memcpy( tracks[i_track]->fmt.p_extra, &p_bih[1], tracks[i_track]->fmt.i_extra );
                }
            }
        }
        else if( !strcmp( tracks[i_track]->psz_codec, "V_MPEG1" ) ||
                 !strcmp( tracks[i_track]->psz_codec, "V_MPEG2" ) )
        {
            tracks[i_track]->fmt.i_codec = VLC_FOURCC( 'm', 'p', 'g', 'v' );
        }
        else if( !strncmp( tracks[i_track]->psz_codec, "V_THEORA", 8 ) )
        {
            uint8_t *p_data = tracks[i_track]->p_extra_data;
            tracks[i_track]->fmt.i_codec = VLC_FOURCC( 't', 'h', 'e', 'o' );
            if( tracks[i_track]->i_extra_data >= 4 ) {
                if( p_data[0] == 2 ) {
                    int i = 1;
                    int i_size1 = 0, i_size2 = 0;
                    p_data++;
                    /* read size of first header packet */
                    while( *p_data == 0xFF &&
                           i < tracks[i_track]->i_extra_data )
                    {
                        i_size1 += *p_data;
                        p_data++;
                        i++;
                    }
                    i_size1 += *p_data;
                    p_data++;
                    i++;
                    msg_Dbg( &sys.demuxer, "first theora header size %d", i_size1 );
                    /* read size of second header packet */
                    while( *p_data == 0xFF &&
                           i < tracks[i_track]->i_extra_data )
                    {
                        i_size2 += *p_data;
                        p_data++;
                        i++;
                    }
                    i_size2 += *p_data;
                    p_data++;
                    i++;
                    int i_size3 = tracks[i_track]->i_extra_data - i - i_size1
                        - i_size2;
                    msg_Dbg( &sys.demuxer, "second theora header size %d", i_size2 );
                    msg_Dbg( &sys.demuxer, "third theora header size %d", i_size3 );
                    tracks[i_track]->fmt.i_extra = i_size1 + i_size2 + i_size3
                        + 6;
                    if( i_size1 > 0 && i_size2 > 0 && i_size3 > 0  ) {
                        tracks[i_track]->fmt.p_extra =
                            malloc( tracks[i_track]->fmt.i_extra );
                        uint8_t *p_out = (uint8_t*)tracks[i_track]->fmt.p_extra;
                        *p_out++ = (i_size1>>8) & 0xFF;
                        *p_out++ = i_size1 & 0xFF;
                        memcpy( p_out, p_data, i_size1 );
                        p_data += i_size1;
                        p_out += i_size1;
 
                        *p_out++ = (i_size2>>8) & 0xFF;
                        *p_out++ = i_size2 & 0xFF;
                        memcpy( p_out, p_data, i_size2 );
                        p_data += i_size2;
                        p_out += i_size2;

                        *p_out++ = (i_size3>>8) & 0xFF;
                        *p_out++ = i_size3 & 0xFF;
                        memcpy( p_out, p_data, i_size3 );
                        p_data += i_size3;
                        p_out += i_size3;
                    }
                    else
                    {
                        msg_Err( &sys.demuxer, "inconsistant theora extradata" );
                    }
                }
                else {
                    msg_Err( &sys.demuxer, "Wrong number of ogg packets with theora headers (%d)", p_data[0] + 1 );
                }
            }
        }
        else if( !strncmp( tracks[i_track]->psz_codec, "V_REAL/RV", 9 ) )
        {
            if( !strcmp( tracks[i_track]->psz_codec, "V_REAL/RV10" ) )
                tracks[i_track]->fmt.i_codec = VLC_FOURCC( 'R', 'V', '1', '0' );
            else if( !strcmp( tracks[i_track]->psz_codec, "V_REAL/RV20" ) )
                tracks[i_track]->fmt.i_codec = VLC_FOURCC( 'R', 'V', '2', '0' );
            else if( !strcmp( tracks[i_track]->psz_codec, "V_REAL/RV30" ) )
                tracks[i_track]->fmt.i_codec = VLC_FOURCC( 'R', 'V', '3', '0' );
            else if( !strcmp( tracks[i_track]->psz_codec, "V_REAL/RV40" ) )
                tracks[i_track]->fmt.i_codec = VLC_FOURCC( 'R', 'V', '4', '0' );
        }
        else if( !strncmp( tracks[i_track]->psz_codec, "V_DIRAC", 7 ) )
        {
            tracks[i_track]->fmt.i_codec = VLC_FOURCC('d', 'r', 'a', 'c' );
        }
        else if( !strncmp( tracks[i_track]->psz_codec, "V_MPEG4", 7 ) )
        {
            if( !strcmp( tracks[i_track]->psz_codec, "V_MPEG4/MS/V3" ) )
            {
                tracks[i_track]->fmt.i_codec = VLC_FOURCC( 'D', 'I', 'V', '3' );
            }
            else if( !strncmp( tracks[i_track]->psz_codec, "V_MPEG4/ISO", 11 ) )
            {
                /* A MPEG 4 codec, SP, ASP, AP or AVC */
                if( !strcmp( tracks[i_track]->psz_codec, "V_MPEG4/ISO/AVC" ) )
                    tracks[i_track]->fmt.i_codec = VLC_FOURCC( 'a', 'v', 'c', '1' );
                else
                    tracks[i_track]->fmt.i_codec = VLC_FOURCC( 'm', 'p', '4', 'v' );
                tracks[i_track]->fmt.i_extra = tracks[i_track]->i_extra_data;
                tracks[i_track]->fmt.p_extra = malloc( tracks[i_track]->i_extra_data );
                memcpy( tracks[i_track]->fmt.p_extra,tracks[i_track]->p_extra_data, tracks[i_track]->i_extra_data );
            }
        }
        else if( !strcmp( tracks[i_track]->psz_codec, "V_QUICKTIME" ) )
        {
            MP4_Box_t *p_box = (MP4_Box_t*)malloc( sizeof( MP4_Box_t ) );
            stream_t *p_mp4_stream = stream_MemoryNew( VLC_OBJECT(&sys.demuxer),
                                                       tracks[i_track]->p_extra_data,
                                                       tracks[i_track]->i_extra_data,
                                                       true );
            MP4_ReadBoxCommon( p_mp4_stream, p_box );
            MP4_ReadBox_sample_vide( p_mp4_stream, p_box );
            tracks[i_track]->fmt.i_codec = p_box->i_type;
            tracks[i_track]->fmt.video.i_width = p_box->data.p_sample_vide->i_width;
            tracks[i_track]->fmt.video.i_height = p_box->data.p_sample_vide->i_height;
            tracks[i_track]->fmt.i_extra = p_box->data.p_sample_vide->i_qt_image_description;
            tracks[i_track]->fmt.p_extra = malloc( tracks[i_track]->fmt.i_extra );
            memcpy( tracks[i_track]->fmt.p_extra, p_box->data.p_sample_vide->p_qt_image_description, tracks[i_track]->fmt.i_extra );
            MP4_FreeBox_sample_vide( p_box );
            stream_Delete( p_mp4_stream );
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
                    tracks[i_track]->fmt.p_extra = malloc( tracks[i_track]->fmt.i_extra );
                    memcpy( tracks[i_track]->fmt.p_extra, &p_wf[1], tracks[i_track]->fmt.i_extra );
                }
            }
        }
        else if( !strcmp( tracks[i_track]->psz_codec, "A_MPEG/L3" ) ||
                 !strcmp( tracks[i_track]->psz_codec, "A_MPEG/L2" ) ||
                 !strcmp( tracks[i_track]->psz_codec, "A_MPEG/L1" ) )
        {
            tracks[i_track]->fmt.i_codec = VLC_FOURCC( 'm', 'p', 'g', 'a' );
        }
        else if( !strcmp( tracks[i_track]->psz_codec, "A_AC3" ) )
        {
            tracks[i_track]->fmt.i_codec = VLC_FOURCC( 'a', '5', '2', ' ' );
        }
        else if( !strcmp( tracks[i_track]->psz_codec, "A_DTS" ) )
        {
            tracks[i_track]->fmt.i_codec = VLC_FOURCC( 'd', 't', 's', ' ' );
        }
        else if( !strcmp( tracks[i_track]->psz_codec, "A_FLAC" ) )
        {
            tracks[i_track]->fmt.i_codec = VLC_FOURCC( 'f', 'l', 'a', 'c' );
            tracks[i_track]->fmt.i_extra = tracks[i_track]->i_extra_data;
            tracks[i_track]->fmt.p_extra = malloc( tracks[i_track]->i_extra_data );
            memcpy( tracks[i_track]->fmt.p_extra,tracks[i_track]->p_extra_data, tracks[i_track]->i_extra_data );
        }
        else if( !strcmp( tracks[i_track]->psz_codec, "A_VORBIS" ) )
        {
            int i, i_offset = 1, i_size[3], i_extra;
            uint8_t *p_extra;

            tracks[i_track]->fmt.i_codec = VLC_FOURCC( 'v', 'o', 'r', 'b' );

            /* Split the 3 headers */
            if( tracks[i_track]->p_extra_data[0] != 0x02 )
                msg_Err( &sys.demuxer, "invalid vorbis header" );

            for( i = 0; i < 2; i++ )
            {
                i_size[i] = 0;
                while( i_offset < tracks[i_track]->i_extra_data )
                {
                    i_size[i] += tracks[i_track]->p_extra_data[i_offset];
                    if( tracks[i_track]->p_extra_data[i_offset++] != 0xff ) break;
                }
            }

            i_size[0] = __MIN(i_size[0], tracks[i_track]->i_extra_data - i_offset);
            i_size[1] = __MIN(i_size[1], tracks[i_track]->i_extra_data -i_offset -i_size[0]);
            i_size[2] = tracks[i_track]->i_extra_data - i_offset - i_size[0] - i_size[1];

            tracks[i_track]->fmt.i_extra = 3 * 2 + i_size[0] + i_size[1] + i_size[2];
            tracks[i_track]->fmt.p_extra = malloc( tracks[i_track]->fmt.i_extra );
            p_extra = (uint8_t *)tracks[i_track]->fmt.p_extra; i_extra = 0;
            for( i = 0; i < 3; i++ )
            {
                *(p_extra++) = i_size[i] >> 8;
                *(p_extra++) = i_size[i] & 0xFF;
                memcpy( p_extra, tracks[i_track]->p_extra_data + i_offset + i_extra,
                        i_size[i] );
                p_extra += i_size[i];
                i_extra += i_size[i];
            }
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

            tracks[i_track]->fmt.i_codec = VLC_FOURCC( 'm', 'p', '4', 'a' );
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
            tracks[i_track]->fmt.p_extra = malloc( tracks[i_track]->fmt.i_extra );
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
            tracks[i_track]->fmt.i_codec = VLC_FOURCC( 'm', 'p', '4', 'a' );
            tracks[i_track]->fmt.i_extra = tracks[i_track]->i_extra_data;
            tracks[i_track]->fmt.p_extra = malloc( tracks[i_track]->i_extra_data );
            memcpy( tracks[i_track]->fmt.p_extra, tracks[i_track]->p_extra_data, tracks[i_track]->i_extra_data );
        }
        else if( !strcmp( tracks[i_track]->psz_codec, "A_WAVPACK4" ) )
        {
            tracks[i_track]->fmt.i_codec = VLC_FOURCC( 'W', 'V', 'P', 'K' );
            tracks[i_track]->fmt.i_extra = tracks[i_track]->i_extra_data;
            tracks[i_track]->fmt.p_extra = malloc( tracks[i_track]->i_extra_data );
            memcpy( tracks[i_track]->fmt.p_extra, tracks[i_track]->p_extra_data, tracks[i_track]->i_extra_data );
        }
        else if( !strcmp( tracks[i_track]->psz_codec, "A_TTA1" ) )
        {
            tracks[i_track]->fmt.i_codec = VLC_FOURCC( 'T', 'T', 'A', '1' );
            tracks[i_track]->fmt.i_extra = tracks[i_track]->i_extra_data;
            tracks[i_track]->fmt.p_extra = malloc( tracks[i_track]->i_extra_data );
            memcpy( tracks[i_track]->fmt.p_extra, tracks[i_track]->p_extra_data, tracks[i_track]->i_extra_data );
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
        /* disabled due to the potential "S_KATE" namespace issue */
        else if( !strcmp( tracks[i_track]->psz_codec, "S_KATE" ) )
        {
            int i, i_offset = 1, *i_size, i_extra, num_headers, size_so_far;
            uint8_t *p_extra;

            tracks[i_track]->fmt.i_codec = VLC_FOURCC( 'k', 'a', 't', 'e' );
            tracks[i_track]->fmt.subs.psz_encoding = strdup( "UTF-8" );

            /* Recover the number of headers to expect */
            num_headers = tracks[i_track]->p_extra_data[0]+1;
            msg_Dbg( &sys.demuxer, "kate in mkv detected: %d headers in %u bytes",
                num_headers, tracks[i_track]->i_extra_data);

            /* this won't overflow the stack as is can allocate only 1020 bytes max */
            i_size = (int*)alloca(num_headers*sizeof(int));

            /* Split the headers */
            size_so_far = 0;
            for( i = 0; i < num_headers-1; i++ )
            {
                i_size[i] = 0;
                while( i_offset < tracks[i_track]->i_extra_data )
                {
                    i_size[i] += tracks[i_track]->p_extra_data[i_offset];
                    if( tracks[i_track]->p_extra_data[i_offset++] != 0xff ) break;
                }
                msg_Dbg( &sys.demuxer, "kate header %d is %d bytes", i, i_size[i]);
                size_so_far += i_size[i];
            }
            i_size[num_headers-1] = tracks[i_track]->i_extra_data - (size_so_far+i_offset);
            msg_Dbg( &sys.demuxer, "kate last header (%d) is %d bytes", num_headers-1, i_size[num_headers-1]);

            tracks[i_track]->fmt.i_extra = 1 + num_headers * 2 + size_so_far + i_size[num_headers-1];
            tracks[i_track]->fmt.p_extra = malloc( tracks[i_track]->fmt.i_extra );

            p_extra = (uint8_t *)tracks[i_track]->fmt.p_extra;
            i_extra = 0;
            *(p_extra++) = num_headers;
            ++i_extra;
            for( i = 0; i < num_headers; i++ )
            {
                *(p_extra++) = i_size[i] >> 8;
                *(p_extra++) = i_size[i] & 0xFF;
                memcpy( p_extra, tracks[i_track]->p_extra_data + i_offset + i_extra-1,
                        i_size[i] );
                p_extra += i_size[i];
                i_extra += i_size[i];
            }
        }
        else if( !strcmp( tracks[i_track]->psz_codec, "S_TEXT/UTF8" ) )
        {
            tracks[i_track]->fmt.i_codec = VLC_FOURCC( 's', 'u', 'b', 't' );
            tracks[i_track]->fmt.subs.psz_encoding = strdup( "UTF-8" );
        }
        else if( !strcmp( tracks[i_track]->psz_codec, "S_TEXT/USF" ) )
        {
            tracks[i_track]->fmt.i_codec = VLC_FOURCC( 'u', 's', 'f', ' ' );
            tracks[i_track]->fmt.subs.psz_encoding = strdup( "UTF-8" );
            if( tracks[i_track]->i_extra_data )
            {
                tracks[i_track]->fmt.i_extra = tracks[i_track]->i_extra_data;
                tracks[i_track]->fmt.p_extra = malloc( tracks[i_track]->i_extra_data );
                memcpy( tracks[i_track]->fmt.p_extra, tracks[i_track]->p_extra_data, tracks[i_track]->i_extra_data );
            }
        }
        else if( !strcmp( tracks[i_track]->psz_codec, "S_TEXT/SSA" ) ||
                 !strcmp( tracks[i_track]->psz_codec, "S_TEXT/ASS" ) ||
                 !strcmp( tracks[i_track]->psz_codec, "S_SSA" ) ||
                 !strcmp( tracks[i_track]->psz_codec, "S_ASS" ))
        {
            tracks[i_track]->fmt.i_codec = VLC_FOURCC( 's', 's', 'a', ' ' );
            tracks[i_track]->fmt.subs.psz_encoding = strdup( "UTF-8" );
            if( tracks[i_track]->i_extra_data )
            {
                tracks[i_track]->fmt.i_extra = tracks[i_track]->i_extra_data;
                tracks[i_track]->fmt.p_extra = malloc( tracks[i_track]->i_extra_data );
                memcpy( tracks[i_track]->fmt.p_extra, tracks[i_track]->p_extra_data, tracks[i_track]->i_extra_data );
            }
        }
        else if( !strcmp( tracks[i_track]->psz_codec, "S_VOBSUB" ) )
        {
            tracks[i_track]->fmt.i_codec = VLC_FOURCC( 's','p','u',' ' );
            if( tracks[i_track]->i_extra_data )
            {
                char *p_start;
                char *p_buf = (char *)malloc( tracks[i_track]->i_extra_data + 1);
                memcpy( p_buf, tracks[i_track]->p_extra_data , tracks[i_track]->i_extra_data );
                p_buf[tracks[i_track]->i_extra_data] = '\0';
 
                p_start = strstr( p_buf, "size:" );
                if( sscanf( p_start, "size: %dx%d",
                        &tracks[i_track]->fmt.subs.spu.i_original_frame_width, &tracks[i_track]->fmt.subs.spu.i_original_frame_height ) == 2 )
                {
                    msg_Dbg( &sys.demuxer, "original frame size vobsubs: %dx%d", tracks[i_track]->fmt.subs.spu.i_original_frame_width, tracks[i_track]->fmt.subs.spu.i_original_frame_height );
                }
                else
                {
                    msg_Warn( &sys.demuxer, "reading original frame size for vobsub failed" );
                }
                free( p_buf );
            }
        }
        else if( !strcmp( tracks[i_track]->psz_codec, "B_VOBBTN" ) )
        {
            tracks[i_track]->fmt.i_cat = NAV_ES;
            continue;
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
                            ES_OUT_SET_DEFAULT,
                            tracks[i_track]->p_es );
        }

        es_out_Control( sys.demuxer.out, ES_OUT_SET_NEXT_DISPLAY_TIME, tracks[i_track]->p_es, i_start_time );
    }
 
    sys.i_start_pts = i_start_time;
    // reset the stream reading to the first cluster of the segment used
    es.I_O().setFilePointer( i_start_pos );

    delete ep;
    ep = new EbmlParser( &es, segment, &sys.demuxer );

    return true;
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
                        VLC_THREAD_PRIORITY_LOW, false );
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
            int i;

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
            vlc_value_t valx, valy;

            vlc_mutex_lock( &p_ev->lock );
            pci_t *pci = (pci_t *) &p_sys->pci_packet;
            var_Get( p_vout, "mouse-x", &valx );
            var_Get( p_vout, "mouse-y", &valy );

            if( p_ev->b_clicked )
            {
                int32_t button;
                int32_t best,dist,d;
                int32_t mx,my,dx,dy;

                msg_Dbg( p_ev->p_demux, "Handle Mouse Event: Mouse clicked x(%d)*y(%d)", (unsigned)valx.i_int, (unsigned)valy.i_int);

                b_activated = true;
                // get current button
                best = 0;
                dist = 0x08000000; /* >> than  (720*720)+(567*567); */
                for(button = 1; button <= pci->hli.hl_gi.btn_ns; button++)
                {
                    btni_t *button_ptr = &(pci->hli.btnit[button-1]);

                    if(((unsigned)valx.i_int >= button_ptr->x_start)
                     && ((unsigned)valx.i_int <= button_ptr->x_end)
                     && ((unsigned)valy.i_int >= button_ptr->y_start)
                     && ((unsigned)valy.i_int <= button_ptr->y_end))
                    {
                        mx = (button_ptr->x_start + button_ptr->x_end)/2;
                        my = (button_ptr->y_start + button_ptr->y_end)/2;
                        dx = mx - valx.i_int;
                        dy = my - valy.i_int;
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
//                dvdnav_mouse_select( NULL, pci, valx.i_int, valy.i_int );
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

    return VLC_SUCCESS;
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

void virtual_segment_c::PrepareChapters( )
{
    if ( linked_segments.size() == 0 )
        return;

    // !!! should be called only once !!!
    matroska_segment_c *p_segment;
    size_t i, j;

    // copy editions from the first segment
    p_segment = linked_segments[0];
    p_editions = &p_segment->stored_editions;

    for ( i=1 ; i<linked_segments.size(); i++ )
    {
        p_segment = linked_segments[i];
        // FIXME assume we have the same editions in all segments
        for (j=0; j<p_segment->stored_editions.size(); j++)
        {
            if( j >= p_editions->size() ) /* Protect against broken files (?) */
                break;
            (*p_editions)[j]->Append( *p_segment->stored_editions[j] );
        }
    }
}

std::string chapter_edition_c::GetMainName() const
{
    if ( sub_chapters.size() )
    {
        return sub_chapters[0]->GetCodecName( true );
    }
    return "";
}

int chapter_item_c::PublishChapters( input_title_t & title, int & i_user_chapters, int i_level )
{
    // add support for meta-elements from codec like DVD Titles
    if ( !b_display_seekpoint || psz_name == "" )
    {
        psz_name = GetCodecName();
        if ( psz_name != "" )
            b_display_seekpoint = true;
    }

    if (b_display_seekpoint)
    {
        seekpoint_t *sk = vlc_seekpoint_New();

        sk->i_level = i_level;
        sk->i_time_offset = i_start_time;
        sk->psz_name = strdup( psz_name.c_str() );

        // A start time of '0' is ok. A missing ChapterTime element is ok, too, because '0' is its default value.
        title.i_seekpoint++;
        title.seekpoint = (seekpoint_t**)realloc( title.seekpoint, title.i_seekpoint * sizeof( seekpoint_t* ) );
        title.seekpoint[title.i_seekpoint-1] = sk;

        if ( b_user_display )
            i_user_chapters++;
    }

    for ( size_t i=0; i<sub_chapters.size() ; i++)
    {
        sub_chapters[i]->PublishChapters( title, i_user_chapters, i_level+1 );
    }

    i_seekpoint_num = i_user_chapters;

    return i_user_chapters;
}

bool virtual_segment_c::UpdateCurrentToChapter( demux_t & demux )
{
    demux_sys_t & sys = *demux.p_sys;
    chapter_item_c *psz_curr_chapter;
    bool b_has_seeked = false;

    /* update current chapter/seekpoint */
    if ( p_editions->size() )
    {
        /* 1st, we need to know in which chapter we are */
        psz_curr_chapter = (*p_editions)[i_current_edition]->FindTimecode( sys.i_pts, psz_current_chapter );

        /* we have moved to a new chapter */
        if (psz_curr_chapter != NULL && psz_current_chapter != psz_curr_chapter)
        {
            if ( (*p_editions)[i_current_edition]->b_ordered )
            {
                // Leave/Enter up to the link point
                b_has_seeked = psz_curr_chapter->EnterAndLeave( psz_current_chapter );
                if ( !b_has_seeked )
                {
                    // only physically seek if necessary
                    if ( psz_current_chapter == NULL || (psz_current_chapter->i_end_time != psz_curr_chapter->i_start_time) )
                        Seek( demux, sys.i_pts, 0, psz_curr_chapter, -1 );
                }
            }
 
            if ( !b_has_seeked )
            {
                psz_current_chapter = psz_curr_chapter;
                if ( psz_curr_chapter->i_seekpoint_num > 0 )
                {
                    demux.info.i_update |= INPUT_UPDATE_TITLE | INPUT_UPDATE_SEEKPOINT;
                    demux.info.i_title = sys.i_current_title = i_sys_title;
                    demux.info.i_seekpoint = psz_curr_chapter->i_seekpoint_num - 1;
                }
            }

            return true;
        }
        else if (psz_curr_chapter == NULL)
        {
            // out of the scope of the data described by chapters, leave the edition
            if ( (*p_editions)[i_current_edition]->b_ordered && psz_current_chapter != NULL )
            {
                if ( !(*p_editions)[i_current_edition]->EnterAndLeave( psz_current_chapter, false ) )
                    psz_current_chapter = NULL;
                else
                    return true;
            }
        }
    }
    return false;
}

chapter_item_c *virtual_segment_c::BrowseCodecPrivate( unsigned int codec_id,
                                    bool (*match)(const chapter_codec_cmds_c &data, const void *p_cookie, size_t i_cookie_size ),
                                    const void *p_cookie,
                                    size_t i_cookie_size )
{
    // FIXME don't assume it is the first edition
    std::vector<chapter_edition_c*>::iterator index = p_editions->begin();
    if ( index != p_editions->end() )
    {
        chapter_item_c *p_result = (*index)->BrowseCodecPrivate( codec_id, match, p_cookie, i_cookie_size );
        if ( p_result != NULL )
            return p_result;
    }
    return NULL;
}

chapter_item_c *virtual_segment_c::FindChapter( int64_t i_find_uid )
{
    // FIXME don't assume it is the first edition
    std::vector<chapter_edition_c*>::iterator index = p_editions->begin();
    if ( index != p_editions->end() )
    {
        chapter_item_c *p_result = (*index)->FindChapter( i_find_uid );
        if ( p_result != NULL )
            return p_result;
    }
    return NULL;
}

chapter_item_c *chapter_item_c::BrowseCodecPrivate( unsigned int codec_id,
                                    bool (*match)(const chapter_codec_cmds_c &data, const void *p_cookie, size_t i_cookie_size ),
                                    const void *p_cookie,
                                    size_t i_cookie_size )
{
    // this chapter
    std::vector<chapter_codec_cmds_c*>::const_iterator index = codecs.begin();
    while ( index != codecs.end() )
    {
        if ( match( **index ,p_cookie, i_cookie_size ) )
            return this;
        index++;
    }
 
    // sub-chapters
    chapter_item_c *p_result = NULL;
    std::vector<chapter_item_c*>::const_iterator index2 = sub_chapters.begin();
    while ( index2 != sub_chapters.end() )
    {
        p_result = (*index2)->BrowseCodecPrivate( codec_id, match, p_cookie, i_cookie_size );
        if ( p_result != NULL )
            return p_result;
        index2++;
    }
 
    return p_result;
}

void chapter_item_c::Append( const chapter_item_c & chapter )
{
    // we are appending content for the same chapter UID
    size_t i;
    chapter_item_c *p_chapter;

    for ( i=0; i<chapter.sub_chapters.size(); i++ )
    {
        p_chapter = FindChapter( chapter.sub_chapters[i]->i_uid );
        if ( p_chapter != NULL )
        {
            p_chapter->Append( *chapter.sub_chapters[i] );
        }
        else
        {
            sub_chapters.push_back( chapter.sub_chapters[i] );
        }
    }

    i_user_start_time = min( i_user_start_time, chapter.i_user_start_time );
    i_user_end_time = max( i_user_end_time, chapter.i_user_end_time );
}

chapter_item_c * chapter_item_c::FindChapter( int64_t i_find_uid )
{
    size_t i;
    chapter_item_c *p_result = NULL;

    if ( i_uid == i_find_uid )
        return this;

    for ( i=0; i<sub_chapters.size(); i++)
    {
        p_result = sub_chapters[i]->FindChapter( i_find_uid );
        if ( p_result != NULL )
            break;
    }
    return p_result;
}

std::string chapter_item_c::GetCodecName( bool f_for_title ) const
{
    std::string result;

    std::vector<chapter_codec_cmds_c*>::const_iterator index = codecs.begin();
    while ( index != codecs.end() )
    {
        result = (*index)->GetCodecName( f_for_title );
        if ( result != "" )
            break;
        index++;
    }

    return result;
}

std::string dvd_chapter_codec_c::GetCodecName( bool f_for_title ) const
{
    std::string result;
    if ( p_private_data->GetSize() >= 3)
    {
        const binary* p_data = p_private_data->GetBuffer();
/*        if ( p_data[0] == MATROSKA_DVD_LEVEL_TT )
        {
            uint16_t i_title = (p_data[1] << 8) + p_data[2];
            char psz_str[11];
            sprintf( psz_str, " %d  ---", i_title );
            result = N_("---  DVD Title");
            result += psz_str;
        }
        else */ if ( p_data[0] == MATROSKA_DVD_LEVEL_LU )
        {
            char psz_str[11];
            sprintf( psz_str, " (%c%c)  ---", p_data[1], p_data[2] );
            result = N_("---  DVD Menu");
            result += psz_str;
        }
        else if ( p_data[0] == MATROSKA_DVD_LEVEL_SS && f_for_title )
        {
            if ( p_data[1] == 0x00 )
                result = N_("First Played");
            else if ( p_data[1] == 0xC0 )
                result = N_("Video Manager");
            else if ( p_data[1] == 0x80 )
            {
                uint16_t i_title = (p_data[2] << 8) + p_data[3];
                char psz_str[20];
                sprintf( psz_str, " %d -----", i_title );
                result = N_("----- Title");
                result += psz_str;
            }
        }
    }

    return result;
}

int16 chapter_item_c::GetTitleNumber( ) const
{
    int result = -1;

    std::vector<chapter_codec_cmds_c*>::const_iterator index = codecs.begin();
    while ( index != codecs.end() )
    {
        result = (*index)->GetTitleNumber( );
        if ( result >= 0 )
            break;
        index++;
    }

    return result;
}

int16 dvd_chapter_codec_c::GetTitleNumber()
{
    if ( p_private_data->GetSize() >= 3)
    {
        const binary* p_data = p_private_data->GetBuffer();
        if ( p_data[0] == MATROSKA_DVD_LEVEL_SS )
        {
            return int16( (p_data[2] << 8) + p_data[3] );
        }
    }
    return -1;
}

static void Seek( demux_t *p_demux, mtime_t i_date, double f_percent, chapter_item_c *psz_chapter )
{
    demux_sys_t        *p_sys = p_demux->p_sys;
    virtual_segment_c  *p_vsegment = p_sys->p_current_segment;
    matroska_segment_c *p_segment = p_vsegment->Segment();
    mtime_t            i_time_offset = 0;
    int64_t            i_global_position = -1;

    int         i_index;

    msg_Dbg( p_demux, "seek request to %"PRId64" (%f%%)", i_date, f_percent );
    if( i_date < 0 && f_percent < 0 )
    {
        msg_Warn( p_demux, "cannot seek nowhere !" );
        return;
    }
    if( f_percent > 1.0 )
    {
        msg_Warn( p_demux, "cannot seek so far !" );
        return;
    }

    /* seek without index or without date */
    if( f_percent >= 0 && (config_GetInt( p_demux, "mkv-seek-percent" ) || !p_segment->b_cues || i_date < 0 ))
    {
        if( p_sys->f_duration >= 0 && p_segment->b_cues )
        {
            i_date = int64_t( f_percent * p_sys->f_duration * 1000.0 );
        }
        else
        {
            int64_t i_pos = int64_t( f_percent * stream_Size( p_demux->s ) );

            msg_Dbg( p_demux, "inaccurate way of seeking for pos:%"PRId64, i_pos );
            for( i_index = 0; i_index < p_segment->i_index; i_index++ )
            {
                if( p_segment->b_cues && p_segment->p_indexes[i_index].i_position < i_pos )
                    break;
                if( !p_segment->b_cues && p_segment->p_indexes[i_index].i_position >= i_pos && p_segment->p_indexes[i_index].i_time > 0 )
                    break;
            }
            if( i_index == p_segment->i_index )
            {
                i_index--;
            }

            i_date = p_segment->p_indexes[i_index].i_time;

            if( !p_segment->b_cues && ( p_segment->p_indexes[i_index].i_position < i_pos || p_segment->p_indexes[i_index].i_position - i_pos > 2000000 ))
            {
                msg_Dbg( p_demux, "no cues, seek request to global pos: %"PRId64, i_pos );
                i_global_position = i_pos;
            }
        }
    }

    p_vsegment->Seek( *p_demux, i_date, i_time_offset, psz_chapter, i_global_position );
}

/*****************************************************************************
 * Demux: reads and demuxes data packets
 *****************************************************************************
 * Returns -1 in case of error, 0 in case of EOF, 1 otherwise
 *****************************************************************************/
static int Demux( demux_t *p_demux)
{
    demux_sys_t        *p_sys = p_demux->p_sys;

    vlc_mutex_lock( &p_sys->lock_demuxer );

    virtual_segment_c  *p_vsegment = p_sys->p_current_segment;
    matroska_segment_c *p_segment = p_vsegment->Segment();
    if ( p_segment == NULL ) return 0;
    int                i_block_count = 0;
    int                i_return = 0;

    for( ;; )
    {
        if ( p_sys->demuxer.b_die )
            break;

        if( p_sys->i_pts >= p_sys->i_start_pts  )
            if ( p_vsegment->UpdateCurrentToChapter( *p_demux ) )
            {
                i_return = 1;
                break;
            }
 
        if ( p_vsegment->Edition() && p_vsegment->Edition()->b_ordered && p_vsegment->CurrentChapter() == NULL )
        {
            /* nothing left to read in this ordered edition */
            if ( !p_vsegment->SelectNext() )
                break;
            p_segment->UnSelect( );
 
            es_out_Control( p_demux->out, ES_OUT_RESET_PCR );

            /* switch to the next segment */
            p_segment = p_vsegment->Segment();
            if ( !p_segment->Select( 0 ) )
            {
                msg_Err( p_demux, "Failed to select new segment" );
                break;
            }
            continue;
        }

        KaxBlock *block;
        KaxSimpleBlock *simpleblock;
        int64_t i_block_duration = 0;
        int64_t i_block_ref1;
        int64_t i_block_ref2;

        if( p_segment->BlockGet( block, simpleblock, &i_block_ref1, &i_block_ref2, &i_block_duration ) )
        {
            if ( p_vsegment->Edition() && p_vsegment->Edition()->b_ordered )
            {
                const chapter_item_c *p_chap = p_vsegment->CurrentChapter();
                // check if there are more chapters to read
                if ( p_chap != NULL )
                {
                    /* TODO handle successive chapters with the same user_start_time/user_end_time
                    if ( p_chap->i_user_start_time == p_chap->i_user_start_time )
                        p_vsegment->SelectNext();
                    */
                    p_sys->i_pts = p_chap->i_user_end_time;
                    p_sys->i_pts++; // trick to avoid staying on segments with no duration and no content

                    i_return = 1;
                }

                break;
            }
            else
            {
                msg_Warn( p_demux, "cannot get block EOF?" );
                p_segment->UnSelect( );
 
                es_out_Control( p_demux->out, ES_OUT_RESET_PCR );

                /* switch to the next segment */
                if ( !p_vsegment->SelectNext() )
                    // no more segments in this stream
                    break;
                p_segment = p_vsegment->Segment();
                if ( !p_segment->Select( 0 ) )
                {
                    msg_Err( p_demux, "Failed to select new segment" );
                    break;
                }

                continue;
            }
        }

        if( simpleblock != NULL )
            p_sys->i_pts = (p_sys->i_chapter_time + simpleblock->GlobalTimecode()) / (mtime_t) 1000;
        else
            p_sys->i_pts = (p_sys->i_chapter_time + block->GlobalTimecode()) / (mtime_t) 1000;

        if( p_sys->i_pts >= p_sys->i_start_pts  )
        {
            es_out_Control( p_demux->out, ES_OUT_SET_PCR, p_sys->i_pts );

            if ( p_vsegment->UpdateCurrentToChapter( *p_demux ) )
            {
                i_return = 1;
                delete block;
                break;
            }
        }
 
        if ( p_vsegment->Edition() && p_vsegment->Edition()->b_ordered && p_vsegment->CurrentChapter() == NULL )
        {
            /* nothing left to read in this ordered edition */
            if ( !p_vsegment->SelectNext() )
            {
                delete block;
                break;
            }
            p_segment->UnSelect( );
 
            es_out_Control( p_demux->out, ES_OUT_RESET_PCR );

            /* switch to the next segment */
            p_segment = p_vsegment->Segment();
            if ( !p_segment->Select( 0 ) )
            {
                msg_Err( p_demux, "Failed to select new segment" );
                delete block;
                break;
            }
            delete block;
            continue;
        }

        BlockDecode( p_demux, block, simpleblock, p_sys->i_pts, i_block_duration, i_block_ref1 >= 0 || i_block_ref2 > 0 );

        delete block;
        i_block_count++;

        // TODO optimize when there is need to leave or when seeking has been called
        if( i_block_count > 5 )
        {
            i_return = 1;
            break;
        }
    }

    vlc_mutex_unlock( &p_sys->lock_demuxer );

    return i_return;
}



/*****************************************************************************
 * Stream managment
 *****************************************************************************/
vlc_stream_io_callback::vlc_stream_io_callback( stream_t *s_, bool b_owner_ )
{
    s = s_;
    b_owner = b_owner_;
    mb_eof = false;
}

uint32 vlc_stream_io_callback::read( void *p_buffer, size_t i_size )
{
    if( i_size <= 0 || mb_eof )
    {
        return 0;
    }

    return stream_Read( s, p_buffer, i_size );
}
void vlc_stream_io_callback::setFilePointer(int64_t i_offset, seek_mode mode )
{
    int64_t i_pos;

    switch( mode )
    {
        case seek_beginning:
            i_pos = i_offset;
            break;
        case seek_end:
            i_pos = stream_Size( s ) - i_offset;
            break;
        default:
            i_pos= stream_Tell( s ) + i_offset;
            break;
    }

    if( i_pos < 0 || i_pos >= stream_Size( s ) )
    {
        mb_eof = true;
        return;
    }

    mb_eof = false;
    if( stream_Seek( s, i_pos ) )
    {
        mb_eof = true;
    }
    return;
}
size_t vlc_stream_io_callback::write( const void *p_buffer, size_t i_size )
{
    return 0;
}
uint64 vlc_stream_io_callback::getFilePointer( void )
{
    if ( s == NULL )
        return 0;
    return stream_Tell( s );
}
void vlc_stream_io_callback::close( void )
{
    return;
}


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
    mb_dummy = config_GetInt( p_demux, "mkv-use-dummy" );
}

EbmlParser::~EbmlParser( void )
{
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
#if LIBEBML_VERSION >= 0x000704
    // a little faster and cleaner
    m_es->I_O().setFilePointer( static_cast<KaxSegment*>(m_el[0])->GetGlobalPosition(0) );
#else
    m_es->I_O().setFilePointer( m_el[0]->GetElementPosition() + m_el[0]->ElementSize(true) - m_el[0]->GetSize() );
#endif
    mb_dummy = config_GetInt( p_demux, "mkv-use-dummy" );
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
    size_t i, j;

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
                p_indexes = (mkv_index_t*)realloc( p_indexes, sizeof( mkv_index_t ) * i_index_max );
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
    size_t i, j;

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
 * ParseSeekHead:
 *****************************************************************************/
void matroska_segment_c::ParseSeekHead( KaxSeekHead *seekhead )
{
    EbmlParser  *ep;
    EbmlElement *l;
    size_t i, j;
    int i_upper_level = 0;
    bool b_seekable;

    i_seekhead_count++;

    stream_Control( sys.demuxer.s, STREAM_CAN_SEEK, &b_seekable );
    if( !b_seekable )
        return;

    ep = new EbmlParser( &es, seekhead, &sys.demuxer );

    while( ( l = ep->Get() ) != NULL )
    {
        if( MKV_IS_ID( l, KaxSeek ) )
        {
            EbmlId id = EbmlVoid::ClassInfos.GlobalId;
            int64_t i_pos = -1;

            msg_Dbg( &sys.demuxer, "|   |   + Seek" );
            ep->Down();
            while( ( l = ep->Get() ) != NULL )
            {
                if( MKV_IS_ID( l, KaxSeekID ) )
                {
                    KaxSeekID &sid = *(KaxSeekID*)l;
                    sid.ReadData( es.I_O() );
                    id = EbmlId( sid.GetBuffer(), sid.GetSize() );
                }
                else if( MKV_IS_ID( l, KaxSeekPosition ) )
                {
                    KaxSeekPosition &spos = *(KaxSeekPosition*)l;
                    spos.ReadData( es.I_O() );
                    i_pos = (int64_t)segment->GetGlobalPosition( uint64( spos ) );
                }
                else
                {
                    /* Many mkvmerge files hit this case. It seems to be a broken SeekHead */
                    msg_Dbg( &sys.demuxer, "|   |   + Unknown (%s)", typeid(*l).name()  );
                }
            }
            ep->Up();

            if( i_pos >= 0 )
            {
                if( id == KaxCues::ClassInfos.GlobalId )
                {
                    msg_Dbg( &sys.demuxer, "|   - cues at %"PRId64, i_pos );
                    LoadSeekHeadItem( KaxCues::ClassInfos, i_pos );
                }
                else if( id == KaxInfo::ClassInfos.GlobalId )
                {
                    msg_Dbg( &sys.demuxer, "|   - info at %"PRId64, i_pos );
                    LoadSeekHeadItem( KaxInfo::ClassInfos, i_pos );
                }
                else if( id == KaxChapters::ClassInfos.GlobalId )
                {
                    msg_Dbg( &sys.demuxer, "|   - chapters at %"PRId64, i_pos );
                    LoadSeekHeadItem( KaxChapters::ClassInfos, i_pos );
                }
                else if( id == KaxTags::ClassInfos.GlobalId )
                {
                    msg_Dbg( &sys.demuxer, "|   - tags at %"PRId64, i_pos );
                    LoadSeekHeadItem( KaxTags::ClassInfos, i_pos );
                }
                else if( id == KaxSeekHead::ClassInfos.GlobalId )
                {
                    msg_Dbg( &sys.demuxer, "|   - chained seekhead at %"PRId64, i_pos );
                    LoadSeekHeadItem( KaxSeekHead::ClassInfos, i_pos );
                }
                else if( id == KaxTracks::ClassInfos.GlobalId )
                {
                    msg_Dbg( &sys.demuxer, "|   - tracks at %"PRId64, i_pos );
                    LoadSeekHeadItem( KaxTracks::ClassInfos, i_pos );
                }
                else if( id == KaxAttachments::ClassInfos.GlobalId )
                {
                    msg_Dbg( &sys.demuxer, "|   - attachments at %"PRId64, i_pos );
                    LoadSeekHeadItem( KaxAttachments::ClassInfos, i_pos );
                }
                else
                    msg_Dbg( &sys.demuxer, "|   - unknown seekhead reference at %"PRId64, i_pos );
            }
        }
        else
            msg_Dbg( &sys.demuxer, "|   |   + ParseSeekHead Unknown (%s)", typeid(*l).name() );
    }
    delete ep;
}

/*****************************************************************************
 * ParseTrackEntry:
 *****************************************************************************/
void matroska_segment_c::ParseTrackEntry( KaxTrackEntry *m )
{
    size_t i, j, k, n;
    bool bSupported = true;

    mkv_track_t *tk;

    msg_Dbg( &sys.demuxer, "|   |   + Track Entry" );

    tk = new mkv_track_t();

    /* Init the track */
    memset( tk, 0, sizeof( mkv_track_t ) );

    es_format_Init( &tk->fmt, UNKNOWN_ES, 0 );
    tk->fmt.psz_language = strdup("English");
    tk->fmt.psz_description = NULL;

    tk->b_default = true;
    tk->b_enabled = true;
    tk->b_silent = false;
    tk->i_number = tracks.size() - 1;
    tk->i_extra_data = 0;
    tk->p_extra_data = NULL;
    tk->psz_codec = NULL;
    tk->i_default_duration = 0;
    tk->f_timecodescale = 1.0;

    tk->b_inited = false;
    tk->i_data_init = 0;
    tk->p_data_init = NULL;

    tk->psz_codec_name = NULL;
    tk->psz_codec_settings = NULL;
    tk->psz_codec_info_url = NULL;
    tk->psz_codec_download_url = NULL;
 
    tk->i_compression_type = MATROSKA_COMPRESSION_NONE;
    tk->p_compression_data = NULL;

    for( i = 0; i < m->ListSize(); i++ )
    {
        EbmlElement *l = (*m)[i];

        if( MKV_IS_ID( l, KaxTrackNumber ) )
        {
            KaxTrackNumber &tnum = *(KaxTrackNumber*)l;

            tk->i_number = uint32( tnum );
            msg_Dbg( &sys.demuxer, "|   |   |   + Track Number=%u", uint32( tnum ) );
        }
        else  if( MKV_IS_ID( l, KaxTrackUID ) )
        {
            KaxTrackUID &tuid = *(KaxTrackUID*)l;

            msg_Dbg( &sys.demuxer, "|   |   |   + Track UID=%u",  uint32( tuid ) );
        }
        else  if( MKV_IS_ID( l, KaxTrackType ) )
        {
            const char *psz_type;
            KaxTrackType &ttype = *(KaxTrackType*)l;

            switch( uint8(ttype) )
            {
                case track_audio:
                    psz_type = "audio";
                    tk->fmt.i_cat = AUDIO_ES;
                    break;
                case track_video:
                    psz_type = "video";
                    tk->fmt.i_cat = VIDEO_ES;
                    break;
                case track_subtitle:
                    psz_type = "subtitle";
                    tk->fmt.i_cat = SPU_ES;
                    break;
                case track_buttons:
                    psz_type = "buttons";
                    tk->fmt.i_cat = SPU_ES;
                    break;
                default:
                    psz_type = "unknown";
                    tk->fmt.i_cat = UNKNOWN_ES;
                    break;
            }

            msg_Dbg( &sys.demuxer, "|   |   |   + Track Type=%s", psz_type );
        }
//        else  if( EbmlId( *l ) == KaxTrackFlagEnabled::ClassInfos.GlobalId )
//        {
//            KaxTrackFlagEnabled &fenb = *(KaxTrackFlagEnabled*)l;

//            tk->b_enabled = uint32( fenb );
//            msg_Dbg( &sys.demuxer, "|   |   |   + Track Enabled=%u",
//                     uint32( fenb )  );
//        }
        else  if( MKV_IS_ID( l, KaxTrackFlagDefault ) )
        {
            KaxTrackFlagDefault &fdef = *(KaxTrackFlagDefault*)l;

            tk->b_default = uint32( fdef );
            msg_Dbg( &sys.demuxer, "|   |   |   + Track Default=%u", uint32( fdef )  );
        }
        else  if( MKV_IS_ID( l, KaxTrackFlagLacing ) )
        {
            KaxTrackFlagLacing &lac = *(KaxTrackFlagLacing*)l;

            msg_Dbg( &sys.demuxer, "|   |   |   + Track Lacing=%d", uint32( lac ) );
        }
        else  if( MKV_IS_ID( l, KaxTrackMinCache ) )
        {
            KaxTrackMinCache &cmin = *(KaxTrackMinCache*)l;

            msg_Dbg( &sys.demuxer, "|   |   |   + Track MinCache=%d", uint32( cmin ) );
        }
        else  if( MKV_IS_ID( l, KaxTrackMaxCache ) )
        {
            KaxTrackMaxCache &cmax = *(KaxTrackMaxCache*)l;

            msg_Dbg( &sys.demuxer, "|   |   |   + Track MaxCache=%d", uint32( cmax ) );
        }
        else  if( MKV_IS_ID( l, KaxTrackDefaultDuration ) )
        {
            KaxTrackDefaultDuration &defd = *(KaxTrackDefaultDuration*)l;

            tk->i_default_duration = uint64(defd);
            msg_Dbg( &sys.demuxer, "|   |   |   + Track Default Duration=%"PRId64, uint64(defd) );
        }
        else  if( MKV_IS_ID( l, KaxTrackTimecodeScale ) )
        {
            KaxTrackTimecodeScale &ttcs = *(KaxTrackTimecodeScale*)l;

            tk->f_timecodescale = float( ttcs );
            msg_Dbg( &sys.demuxer, "|   |   |   + Track TimeCodeScale=%f", tk->f_timecodescale );
        }
        else if( MKV_IS_ID( l, KaxTrackName ) )
        {
            KaxTrackName &tname = *(KaxTrackName*)l;

            tk->fmt.psz_description = ToUTF8( UTFstring( tname ) );
            msg_Dbg( &sys.demuxer, "|   |   |   + Track Name=%s", tk->fmt.psz_description );
        }
        else  if( MKV_IS_ID( l, KaxTrackLanguage ) )
        {
            KaxTrackLanguage &lang = *(KaxTrackLanguage*)l;

            if ( tk->fmt.psz_language != NULL )
                free( tk->fmt.psz_language );
            tk->fmt.psz_language = strdup( string( lang ).c_str() );
            msg_Dbg( &sys.demuxer,
                     "|   |   |   + Track Language=`%s'", tk->fmt.psz_language );
        }
        else  if( MKV_IS_ID( l, KaxCodecID ) )
        {
            KaxCodecID &codecid = *(KaxCodecID*)l;

            tk->psz_codec = strdup( string( codecid ).c_str() );
            msg_Dbg( &sys.demuxer, "|   |   |   + Track CodecId=%s", string( codecid ).c_str() );
        }
        else  if( MKV_IS_ID( l, KaxCodecPrivate ) )
        {
            KaxCodecPrivate &cpriv = *(KaxCodecPrivate*)l;

            tk->i_extra_data = cpriv.GetSize();
            if( tk->i_extra_data > 0 )
            {
                tk->p_extra_data = (uint8_t*)malloc( tk->i_extra_data );
                memcpy( tk->p_extra_data, cpriv.GetBuffer(), tk->i_extra_data );
            }
            msg_Dbg( &sys.demuxer, "|   |   |   + Track CodecPrivate size=%"PRId64, cpriv.GetSize() );
        }
        else if( MKV_IS_ID( l, KaxCodecName ) )
        {
            KaxCodecName &cname = *(KaxCodecName*)l;

            tk->psz_codec_name = ToUTF8( UTFstring( cname ) );
            msg_Dbg( &sys.demuxer, "|   |   |   + Track Codec Name=%s", tk->psz_codec_name );
        }
        else if( MKV_IS_ID( l, KaxContentEncodings ) )
        {
            EbmlMaster *cencs = static_cast<EbmlMaster*>(l);
            MkvTree( sys.demuxer, 3, "Content Encodings" );
            if ( cencs->ListSize() > 1 )
            {
                msg_Err( &sys.demuxer, "Multiple Compression method not supported" );
                bSupported = false;
            }
            for( j = 0; j < cencs->ListSize(); j++ )
            {
                EbmlElement *l2 = (*cencs)[j];
                if( MKV_IS_ID( l2, KaxContentEncoding ) )
                {
                    MkvTree( sys.demuxer, 4, "Content Encoding" );
                    EbmlMaster *cenc = static_cast<EbmlMaster*>(l2);
                    for( k = 0; k < cenc->ListSize(); k++ )
                    {
                        EbmlElement *l3 = (*cenc)[k];
                        if( MKV_IS_ID( l3, KaxContentEncodingOrder ) )
                        {
                            KaxContentEncodingOrder &encord = *(KaxContentEncodingOrder*)l3;
                            MkvTree( sys.demuxer, 5, "Order: %i", uint32( encord ) );
                        }
                        else if( MKV_IS_ID( l3, KaxContentEncodingScope ) )
                        {
                            KaxContentEncodingScope &encscope = *(KaxContentEncodingScope*)l3;
                            MkvTree( sys.demuxer, 5, "Scope: %i", uint32( encscope ) );
                        }
                        else if( MKV_IS_ID( l3, KaxContentEncodingType ) )
                        {
                            KaxContentEncodingType &enctype = *(KaxContentEncodingType*)l3;
                            MkvTree( sys.demuxer, 5, "Type: %i", uint32( enctype ) );
                        }
                        else if( MKV_IS_ID( l3, KaxContentCompression ) )
                        {
                            EbmlMaster *compr = static_cast<EbmlMaster*>(l3);
                            MkvTree( sys.demuxer, 5, "Content Compression" );
                            for( n = 0; n < compr->ListSize(); n++ )
                            {
                                EbmlElement *l4 = (*compr)[n];
                                if( MKV_IS_ID( l4, KaxContentCompAlgo ) )
                                {
                                    KaxContentCompAlgo &compalg = *(KaxContentCompAlgo*)l4;
                                    MkvTree( sys.demuxer, 6, "Compression Algorithm: %i", uint32(compalg) );
                                    tk->i_compression_type = uint32( compalg );
                                    if ( ( tk->i_compression_type != MATROSKA_COMPRESSION_ZLIB ) &&
                                         ( tk->i_compression_type != MATROSKA_COMPRESSION_HEADER ) )
                                    {
                                        msg_Err( &sys.demuxer, "Track Compression method %d not supported", tk->i_compression_type );
                                        bSupported = false;
                                    }
                                }
                                else if( MKV_IS_ID( l4, KaxContentCompSettings ) )
                                {
                                    tk->p_compression_data = new KaxContentCompSettings( *(KaxContentCompSettings*)l4 );
                                }
                                else
                                {
                                    MkvTree( sys.demuxer, 6, "Unknown (%s)", typeid(*l4).name() );
                                }
                            }
                        }
                        else
                        {
                            MkvTree( sys.demuxer, 5, "Unknown (%s)", typeid(*l3).name() );
                        }
                    }
                }
                else
                {
                    MkvTree( sys.demuxer, 4, "Unknown (%s)", typeid(*l2).name() );
                }
            }
        }
//        else if( EbmlId( *l ) == KaxCodecSettings::ClassInfos.GlobalId )
//        {
//            KaxCodecSettings &cset = *(KaxCodecSettings*)l;

//            tk->psz_codec_settings = ToUTF8( UTFstring( cset ) );
//            msg_Dbg( &sys.demuxer, "|   |   |   + Track Codec Settings=%s", tk->psz_codec_settings );
//        }
//        else if( EbmlId( *l ) == KaxCodecInfoURL::ClassInfos.GlobalId )
//        {
//            KaxCodecInfoURL &ciurl = *(KaxCodecInfoURL*)l;

//            tk->psz_codec_info_url = strdup( string( ciurl ).c_str() );
//            msg_Dbg( &sys.demuxer, "|   |   |   + Track Codec Info URL=%s", tk->psz_codec_info_url );
//        }
//        else if( EbmlId( *l ) == KaxCodecDownloadURL::ClassInfos.GlobalId )
//        {
//            KaxCodecDownloadURL &cdurl = *(KaxCodecDownloadURL*)l;

//            tk->psz_codec_download_url = strdup( string( cdurl ).c_str() );
//            msg_Dbg( &sys.demuxer, "|   |   |   + Track Codec Info URL=%s", tk->psz_codec_download_url );
//        }
//        else if( EbmlId( *l ) == KaxCodecDecodeAll::ClassInfos.GlobalId )
//        {
//            KaxCodecDecodeAll &cdall = *(KaxCodecDecodeAll*)l;

//            msg_Dbg( &sys.demuxer, "|   |   |   + Track Codec Decode All=%u <== UNUSED", uint8( cdall ) );
//        }
//        else if( EbmlId( *l ) == KaxTrackOverlay::ClassInfos.GlobalId )
//        {
//            KaxTrackOverlay &tovr = *(KaxTrackOverlay*)l;

//            msg_Dbg( &sys.demuxer, "|   |   |   + Track Overlay=%u <== UNUSED", uint32( tovr ) );
//        }
        else  if( MKV_IS_ID( l, KaxTrackVideo ) )
        {
            EbmlMaster *tkv = static_cast<EbmlMaster*>(l);
            unsigned int j;
            unsigned int i_crop_right = 0, i_crop_left = 0, i_crop_top = 0, i_crop_bottom = 0;
            unsigned int i_display_unit = 0, i_display_width = 0, i_display_height = 0;

            msg_Dbg( &sys.demuxer, "|   |   |   + Track Video" );
            tk->f_fps = 0.0;

            tk->fmt.video.i_frame_rate_base = (unsigned int)(tk->i_default_duration / 1000);
            tk->fmt.video.i_frame_rate = 1000000;
 
            for( j = 0; j < tkv->ListSize(); j++ )
            {
                EbmlElement *l = (*tkv)[j];
//                if( EbmlId( *el4 ) == KaxVideoFlagInterlaced::ClassInfos.GlobalId )
//                {
//                    KaxVideoFlagInterlaced &fint = *(KaxVideoFlagInterlaced*)el4;

//                    msg_Dbg( &sys.demuxer, "|   |   |   |   + Track Video Interlaced=%u", uint8( fint ) );
//                }
//                else if( EbmlId( *el4 ) == KaxVideoStereoMode::ClassInfos.GlobalId )
//                {
//                    KaxVideoStereoMode &stereo = *(KaxVideoStereoMode*)el4;

//                    msg_Dbg( &sys.demuxer, "|   |   |   |   + Track Video Stereo Mode=%u", uint8( stereo ) );
//                }
//                else
                if( MKV_IS_ID( l, KaxVideoPixelWidth ) )
                {
                    KaxVideoPixelWidth &vwidth = *(KaxVideoPixelWidth*)l;

                    tk->fmt.video.i_width += uint16( vwidth );
                    msg_Dbg( &sys.demuxer, "|   |   |   |   + width=%d", uint16( vwidth ) );
                }
                else if( MKV_IS_ID( l, KaxVideoPixelHeight ) )
                {
                    KaxVideoPixelWidth &vheight = *(KaxVideoPixelWidth*)l;

                    tk->fmt.video.i_height += uint16( vheight );
                    msg_Dbg( &sys.demuxer, "|   |   |   |   + height=%d", uint16( vheight ) );
                }
                else if( MKV_IS_ID( l, KaxVideoDisplayWidth ) )
                {
                    KaxVideoDisplayWidth &vwidth = *(KaxVideoDisplayWidth*)l;

                    i_display_width = uint16( vwidth );
                    msg_Dbg( &sys.demuxer, "|   |   |   |   + display width=%d", uint16( vwidth ) );
                }
                else if( MKV_IS_ID( l, KaxVideoDisplayHeight ) )
                {
                    KaxVideoDisplayWidth &vheight = *(KaxVideoDisplayWidth*)l;

                    i_display_height = uint16( vheight );
                    msg_Dbg( &sys.demuxer, "|   |   |   |   + display height=%d", uint16( vheight ) );
                }
                else if( MKV_IS_ID( l, KaxVideoPixelCropBottom ) )
                {
                    KaxVideoPixelCropBottom &cropval = *(KaxVideoPixelCropBottom*)l;

                    i_crop_bottom = uint16( cropval );
                    msg_Dbg( &sys.demuxer, "|   |   |   |   + crop pixel bottom=%d", uint16( cropval ) );
                }
                else if( MKV_IS_ID( l, KaxVideoPixelCropTop ) )
                {
                    KaxVideoPixelCropTop &cropval = *(KaxVideoPixelCropTop*)l;

                    i_crop_top = uint16( cropval );
                    msg_Dbg( &sys.demuxer, "|   |   |   |   + crop pixel top=%d", uint16( cropval ) );
                }
                else if( MKV_IS_ID( l, KaxVideoPixelCropRight ) )
                {
                    KaxVideoPixelCropRight &cropval = *(KaxVideoPixelCropRight*)l;

                    i_crop_right = uint16( cropval );
                    msg_Dbg( &sys.demuxer, "|   |   |   |   + crop pixel right=%d", uint16( cropval ) );
                }
                else if( MKV_IS_ID( l, KaxVideoPixelCropLeft ) )
                {
                    KaxVideoPixelCropLeft &cropval = *(KaxVideoPixelCropLeft*)l;

                    i_crop_left = uint16( cropval );
                    msg_Dbg( &sys.demuxer, "|   |   |   |   + crop pixel left=%d", uint16( cropval ) );
                }
                else if( MKV_IS_ID( l, KaxVideoFrameRate ) )
                {
                    KaxVideoFrameRate &vfps = *(KaxVideoFrameRate*)l;

                    tk->f_fps = float( vfps );
                    msg_Dbg( &sys.demuxer, "   |   |   |   + fps=%f", float( vfps ) );
                }
                else if( EbmlId( *l ) == KaxVideoDisplayUnit::ClassInfos.GlobalId )
                {
                    KaxVideoDisplayUnit &vdmode = *(KaxVideoDisplayUnit*)l;

                    i_display_unit = uint8( vdmode );
                    msg_Dbg( &sys.demuxer, "|   |   |   |   + Track Video Display Unit=%s",
                             uint8( vdmode ) == 0 ? "pixels" : ( uint8( vdmode ) == 1 ? "centimeters": "inches" ) );
                }
//                else if( EbmlId( *l ) == KaxVideoAspectRatio::ClassInfos.GlobalId )
//                {
//                    KaxVideoAspectRatio &ratio = *(KaxVideoAspectRatio*)l;

//                    msg_Dbg( &sys.demuxer, "   |   |   |   + Track Video Aspect Ratio Type=%u", uint8( ratio ) );
//                }
//                else if( EbmlId( *l ) == KaxVideoGamma::ClassInfos.GlobalId )
//                {
//                    KaxVideoGamma &gamma = *(KaxVideoGamma*)l;

//                    msg_Dbg( &sys.demuxer, "   |   |   |   + gamma=%f", float( gamma ) );
//                }
                else
                {
                    msg_Dbg( &sys.demuxer, "|   |   |   |   + Unknown (%s)", typeid(*l).name() );
                }
            }
            if( i_display_height && i_display_width )
                tk->fmt.video.i_aspect = VOUT_ASPECT_FACTOR * i_display_width / i_display_height;
            if( i_crop_left || i_crop_right || i_crop_top || i_crop_bottom )
            {
                tk->fmt.video.i_visible_width = tk->fmt.video.i_width;
                tk->fmt.video.i_visible_height = tk->fmt.video.i_height;
                tk->fmt.video.i_x_offset = i_crop_left;
                tk->fmt.video.i_y_offset = i_crop_top;
                tk->fmt.video.i_visible_width -= i_crop_left + i_crop_right;
                tk->fmt.video.i_visible_height -= i_crop_top + i_crop_bottom;
            }
            /* FIXME: i_display_* allows you to not only set DAR, but also a zoom factor.
               we do not support this atm */
        }
        else  if( MKV_IS_ID( l, KaxTrackAudio ) )
        {
            EbmlMaster *tka = static_cast<EbmlMaster*>(l);
            unsigned int j;

            msg_Dbg( &sys.demuxer, "|   |   |   + Track Audio" );

            for( j = 0; j < tka->ListSize(); j++ )
            {
                EbmlElement *l = (*tka)[j];

                if( MKV_IS_ID( l, KaxAudioSamplingFreq ) )
                {
                    KaxAudioSamplingFreq &afreq = *(KaxAudioSamplingFreq*)l;

                    tk->i_original_rate = tk->fmt.audio.i_rate = (int)float( afreq );
                    msg_Dbg( &sys.demuxer, "|   |   |   |   + afreq=%d", tk->fmt.audio.i_rate );
                }
                else if( MKV_IS_ID( l, KaxAudioOutputSamplingFreq ) )
                {
                    KaxAudioOutputSamplingFreq &afreq = *(KaxAudioOutputSamplingFreq*)l;

                    tk->fmt.audio.i_rate = (int)float( afreq );
                    msg_Dbg( &sys.demuxer, "|   |   |   |   + aoutfreq=%d", tk->fmt.audio.i_rate );
                }
                else if( MKV_IS_ID( l, KaxAudioChannels ) )
                {
                    KaxAudioChannels &achan = *(KaxAudioChannels*)l;

                    tk->fmt.audio.i_channels = uint8( achan );
                    msg_Dbg( &sys.demuxer, "|   |   |   |   + achan=%u", uint8( achan ) );
                }
                else if( MKV_IS_ID( l, KaxAudioBitDepth ) )
                {
                    KaxAudioBitDepth &abits = *(KaxAudioBitDepth*)l;

                    tk->fmt.audio.i_bitspersample = uint8( abits );
                    msg_Dbg( &sys.demuxer, "|   |   |   |   + abits=%u", uint8( abits ) );
                }
                else
                {
                    msg_Dbg( &sys.demuxer, "|   |   |   |   + Unknown (%s)", typeid(*l).name() );
                }
            }
        }
        else
        {
            msg_Dbg( &sys.demuxer, "|   |   |   + Unknown (%s)",
                     typeid(*l).name() );
        }
    }

    if ( bSupported )
    {
        tracks.push_back( tk );
    }
    else
    {
        msg_Err( &sys.demuxer, "Track Entry %d not supported", tk->i_number );
        delete tk;
    }
}

/*****************************************************************************
 * ParseTracks:
 *****************************************************************************/
void matroska_segment_c::ParseTracks( KaxTracks *tracks )
{
    EbmlElement *el;
    unsigned int i;
    int i_upper_level = 0;

    /* Master elements */
    tracks->Read( es, tracks->Generic().Context, i_upper_level, el, true );

    for( i = 0; i < tracks->ListSize(); i++ )
    {
        EbmlElement *l = (*tracks)[i];

        if( MKV_IS_ID( l, KaxTrackEntry ) )
        {
            ParseTrackEntry( static_cast<KaxTrackEntry *>(l) );
        }
        else
        {
            msg_Dbg( &sys.demuxer, "|   |   + Unknown (%s)", typeid(*l).name() );
        }
    }
}

/*****************************************************************************
 * ParseInfo:
 *****************************************************************************/
void matroska_segment_c::ParseInfo( KaxInfo *info )
{
    EbmlElement *el;
    EbmlMaster  *m;
    size_t i, j;
    int i_upper_level = 0;

    /* Master elements */
    m = static_cast<EbmlMaster *>(info);
    m->Read( es, info->Generic().Context, i_upper_level, el, true );

    for( i = 0; i < m->ListSize(); i++ )
    {
        EbmlElement *l = (*m)[i];

        if( MKV_IS_ID( l, KaxSegmentUID ) )
        {
            if ( p_segment_uid == NULL )
                p_segment_uid = new KaxSegmentUID(*static_cast<KaxSegmentUID*>(l));

            msg_Dbg( &sys.demuxer, "|   |   + UID=%d", *(uint32*)p_segment_uid->GetBuffer() );
        }
        else if( MKV_IS_ID( l, KaxPrevUID ) )
        {
            if ( p_prev_segment_uid == NULL )
                p_prev_segment_uid = new KaxPrevUID(*static_cast<KaxPrevUID*>(l));

            msg_Dbg( &sys.demuxer, "|   |   + PrevUID=%d", *(uint32*)p_prev_segment_uid->GetBuffer() );
        }
        else if( MKV_IS_ID( l, KaxNextUID ) )
        {
            if ( p_next_segment_uid == NULL )
                p_next_segment_uid = new KaxNextUID(*static_cast<KaxNextUID*>(l));

            msg_Dbg( &sys.demuxer, "|   |   + NextUID=%d", *(uint32*)p_next_segment_uid->GetBuffer() );
        }
        else if( MKV_IS_ID( l, KaxTimecodeScale ) )
        {
            KaxTimecodeScale &tcs = *(KaxTimecodeScale*)l;

            i_timescale = uint64(tcs);

            msg_Dbg( &sys.demuxer, "|   |   + TimecodeScale=%"PRId64,
                     i_timescale );
        }
        else if( MKV_IS_ID( l, KaxDuration ) )
        {
            KaxDuration &dur = *(KaxDuration*)l;

            i_duration = mtime_t( double( dur ) );

            msg_Dbg( &sys.demuxer, "|   |   + Duration=%"PRId64,
                     i_duration );
        }
        else if( MKV_IS_ID( l, KaxMuxingApp ) )
        {
            KaxMuxingApp &mapp = *(KaxMuxingApp*)l;

            psz_muxing_application = ToUTF8( UTFstring( mapp ) );

            msg_Dbg( &sys.demuxer, "|   |   + Muxing Application=%s",
                     psz_muxing_application );
        }
        else if( MKV_IS_ID( l, KaxWritingApp ) )
        {
            KaxWritingApp &wapp = *(KaxWritingApp*)l;

            psz_writing_application = ToUTF8( UTFstring( wapp ) );

            msg_Dbg( &sys.demuxer, "|   |   + Writing Application=%s",
                     psz_writing_application );
        }
        else if( MKV_IS_ID( l, KaxSegmentFilename ) )
        {
            KaxSegmentFilename &sfn = *(KaxSegmentFilename*)l;

            psz_segment_filename = ToUTF8( UTFstring( sfn ) );

            msg_Dbg( &sys.demuxer, "|   |   + Segment Filename=%s",
                     psz_segment_filename );
        }
        else if( MKV_IS_ID( l, KaxTitle ) )
        {
            KaxTitle &title = *(KaxTitle*)l;

            psz_title = ToUTF8( UTFstring( title ) );

            msg_Dbg( &sys.demuxer, "|   |   + Title=%s", psz_title );
        }
        else if( MKV_IS_ID( l, KaxSegmentFamily ) )
        {
            KaxSegmentFamily *uid = static_cast<KaxSegmentFamily*>(l);

            families.push_back( new KaxSegmentFamily(*uid) );

            msg_Dbg( &sys.demuxer, "|   |   + family=%d", *(uint32*)uid->GetBuffer() );
        }
#if defined( HAVE_GMTIME_R )
        else if( MKV_IS_ID( l, KaxDateUTC ) )
        {
            KaxDateUTC &date = *(KaxDateUTC*)l;
            time_t i_date;
            struct tm tmres;
            char   buffer[256];

            i_date = date.GetEpochDate();
            memset( buffer, 0, 256 );
            if( gmtime_r( &i_date, &tmres ) &&
                asctime_r( &tmres, buffer ) )
            {
                buffer[strlen( buffer)-1]= '\0';
                psz_date_utc = strdup( buffer );
                msg_Dbg( &sys.demuxer, "|   |   + Date=%s", psz_date_utc );
            }
        }
#endif
        else if( MKV_IS_ID( l, KaxChapterTranslate ) )
        {
            KaxChapterTranslate *p_trans = static_cast<KaxChapterTranslate*>( l );
            chapter_translation_c *p_translate = new chapter_translation_c();

            p_trans->Read( es, p_trans->Generic().Context, i_upper_level, el, true );
            for( j = 0; j < p_trans->ListSize(); j++ )
            {
                EbmlElement *l = (*p_trans)[j];

                if( MKV_IS_ID( l, KaxChapterTranslateEditionUID ) )
                {
                    p_translate->editions.push_back( uint64( *static_cast<KaxChapterTranslateEditionUID*>( l ) ) );
                }
                else if( MKV_IS_ID( l, KaxChapterTranslateCodec ) )
                {
                    p_translate->codec_id = uint32( *static_cast<KaxChapterTranslateCodec*>( l ) );
                }
                else if( MKV_IS_ID( l, KaxChapterTranslateID ) )
                {
                    p_translate->p_translated = new KaxChapterTranslateID( *static_cast<KaxChapterTranslateID*>( l ) );
                }
            }

            translations.push_back( p_translate );
        }
        else
        {
            msg_Dbg( &sys.demuxer, "|   |   + Unknown (%s)", typeid(*l).name() );
        }
    }

    double f_dur = double(i_duration) * double(i_timescale) / 1000000.0;
    i_duration = mtime_t(f_dur);
}


/*****************************************************************************
 * ParseChapterAtom
 *****************************************************************************/
void matroska_segment_c::ParseChapterAtom( int i_level, KaxChapterAtom *ca, chapter_item_c & chapters )
{
    size_t i, j;

    msg_Dbg( &sys.demuxer, "|   |   |   + ChapterAtom (level=%d)", i_level );
    for( i = 0; i < ca->ListSize(); i++ )
    {
        EbmlElement *l = (*ca)[i];

        if( MKV_IS_ID( l, KaxChapterUID ) )
        {
            chapters.i_uid = uint64_t(*(KaxChapterUID*)l);
            msg_Dbg( &sys.demuxer, "|   |   |   |   + ChapterUID: %lld", chapters.i_uid );
        }
        else if( MKV_IS_ID( l, KaxChapterFlagHidden ) )
        {
            KaxChapterFlagHidden &flag =*(KaxChapterFlagHidden*)l;
            chapters.b_display_seekpoint = uint8( flag ) == 0;

            msg_Dbg( &sys.demuxer, "|   |   |   |   + ChapterFlagHidden: %s", chapters.b_display_seekpoint ? "no":"yes" );
        }
        else if( MKV_IS_ID( l, KaxChapterTimeStart ) )
        {
            KaxChapterTimeStart &start =*(KaxChapterTimeStart*)l;
            chapters.i_start_time = uint64( start ) / INT64_C(1000);

            msg_Dbg( &sys.demuxer, "|   |   |   |   + ChapterTimeStart: %lld", chapters.i_start_time );
        }
        else if( MKV_IS_ID( l, KaxChapterTimeEnd ) )
        {
            KaxChapterTimeEnd &end =*(KaxChapterTimeEnd*)l;
            chapters.i_end_time = uint64( end ) / INT64_C(1000);

            msg_Dbg( &sys.demuxer, "|   |   |   |   + ChapterTimeEnd: %lld", chapters.i_end_time );
        }
        else if( MKV_IS_ID( l, KaxChapterDisplay ) )
        {
            EbmlMaster *cd = static_cast<EbmlMaster *>(l);

            msg_Dbg( &sys.demuxer, "|   |   |   |   + ChapterDisplay" );
            for( j = 0; j < cd->ListSize(); j++ )
            {
                EbmlElement *l= (*cd)[j];

                if( MKV_IS_ID( l, KaxChapterString ) )
                {
                    int k;

                    KaxChapterString &name =*(KaxChapterString*)l;
                    for (k = 0; k < i_level; k++)
                        chapters.psz_name += '+';
                    chapters.psz_name += ' ';
                    char *psz_tmp_utf8 = ToUTF8( UTFstring( name ) );
                    chapters.psz_name += psz_tmp_utf8;
                    chapters.b_user_display = true;

                    msg_Dbg( &sys.demuxer, "|   |   |   |   |    + ChapterString '%s'", psz_tmp_utf8 );
                    free( psz_tmp_utf8 );
                }
                else if( MKV_IS_ID( l, KaxChapterLanguage ) )
                {
                    KaxChapterLanguage &lang =*(KaxChapterLanguage*)l;
                    const char *psz = string( lang ).c_str();

                    msg_Dbg( &sys.demuxer, "|   |   |   |   |    + ChapterLanguage '%s'", psz );
                }
                else if( MKV_IS_ID( l, KaxChapterCountry ) )
                {
                    KaxChapterCountry &ct =*(KaxChapterCountry*)l;
                    const char *psz = string( ct ).c_str();

                    msg_Dbg( &sys.demuxer, "|   |   |   |   |    + ChapterCountry '%s'", psz );
                }
            }
        }
        else if( MKV_IS_ID( l, KaxChapterProcess ) )
        {
            msg_Dbg( &sys.demuxer, "|   |   |   |   + ChapterProcess" );

            KaxChapterProcess *cp = static_cast<KaxChapterProcess *>(l);
            chapter_codec_cmds_c *p_ccodec = NULL;

            for( j = 0; j < cp->ListSize(); j++ )
            {
                EbmlElement *k= (*cp)[j];

                if( MKV_IS_ID( k, KaxChapterProcessCodecID ) )
                {
                    KaxChapterProcessCodecID *p_codec_id = static_cast<KaxChapterProcessCodecID*>( k );
                    if ( uint32(*p_codec_id) == 0 )
                        p_ccodec = new matroska_script_codec_c( sys );
                    else if ( uint32(*p_codec_id) == 1 )
                        p_ccodec = new dvd_chapter_codec_c( sys );
                    break;
                }
            }

            if ( p_ccodec != NULL )
            {
                for( j = 0; j < cp->ListSize(); j++ )
                {
                    EbmlElement *k= (*cp)[j];

                    if( MKV_IS_ID( k, KaxChapterProcessPrivate ) )
                    {
                        KaxChapterProcessPrivate * p_private = static_cast<KaxChapterProcessPrivate*>( k );
                        p_ccodec->SetPrivate( *p_private );
                    }
                    else if( MKV_IS_ID( k, KaxChapterProcessCommand ) )
                    {
                        p_ccodec->AddCommand( *static_cast<KaxChapterProcessCommand*>( k ) );
                    }
                }
                chapters.codecs.push_back( p_ccodec );
            }
        }
        else if( MKV_IS_ID( l, KaxChapterAtom ) )
        {
            chapter_item_c *new_sub_chapter = new chapter_item_c();
            ParseChapterAtom( i_level+1, static_cast<KaxChapterAtom *>(l), *new_sub_chapter );
            new_sub_chapter->psz_parent = &chapters;
            chapters.sub_chapters.push_back( new_sub_chapter );
        }
    }
}

/*****************************************************************************
 * ParseAttachments:
 *****************************************************************************/
void matroska_segment_c::ParseAttachments( KaxAttachments *attachments )
{
    EbmlElement *el;
    int i_upper_level = 0;

    attachments->Read( es, attachments->Generic().Context, i_upper_level, el, true );

    KaxAttached *attachedFile = FindChild<KaxAttached>( *attachments );

    while( attachedFile && ( attachedFile->GetSize() > 0 ) )
    {
        std::string psz_mime_type  = GetChild<KaxMimeType>( *attachedFile );
        KaxFileName  &file_name    = GetChild<KaxFileName>( *attachedFile );
        KaxFileData  &img_data     = GetChild<KaxFileData>( *attachedFile );

        attachment_c *new_attachment = new attachment_c();

        if( new_attachment )
        {
            char* tmp = ToUTF8( UTFstring( file_name ) );
            new_attachment->psz_file_name  = tmp;
            free( tmp );
            new_attachment->psz_mime_type  = psz_mime_type;
            new_attachment->i_size         = img_data.GetSize();
            new_attachment->p_data         = malloc( img_data.GetSize() );

            if( new_attachment->p_data )
            {
                memcpy( new_attachment->p_data, img_data.GetBuffer(), img_data.GetSize() );
                sys.stored_attachments.push_back( new_attachment );
            }
            else
            {
                delete new_attachment;
            }
        }

        attachedFile = &GetNextChild<KaxAttached>( *attachments, *attachedFile );
    }
}

/*****************************************************************************
 * ParseChapters:
 *****************************************************************************/
void matroska_segment_c::ParseChapters( KaxChapters *chapters )
{
    EbmlElement *el;
    size_t i;
    int i_upper_level = 0;
    mtime_t i_dur;

    /* Master elements */
    chapters->Read( es, chapters->Generic().Context, i_upper_level, el, true );

    for( i = 0; i < chapters->ListSize(); i++ )
    {
        EbmlElement *l = (*chapters)[i];

        if( MKV_IS_ID( l, KaxEditionEntry ) )
        {
            chapter_edition_c *p_edition = new chapter_edition_c();
 
            EbmlMaster *E = static_cast<EbmlMaster *>(l );
            size_t j;
            msg_Dbg( &sys.demuxer, "|   |   + EditionEntry" );
            for( j = 0; j < E->ListSize(); j++ )
            {
                EbmlElement *l = (*E)[j];

                if( MKV_IS_ID( l, KaxChapterAtom ) )
                {
                    chapter_item_c *new_sub_chapter = new chapter_item_c();
                    ParseChapterAtom( 0, static_cast<KaxChapterAtom *>(l), *new_sub_chapter );
                    p_edition->sub_chapters.push_back( new_sub_chapter );
                }
                else if( MKV_IS_ID( l, KaxEditionUID ) )
                {
                    p_edition->i_uid = uint64(*static_cast<KaxEditionUID *>( l ));
                }
                else if( MKV_IS_ID( l, KaxEditionFlagOrdered ) )
                {
                    p_edition->b_ordered = config_GetInt( &sys.demuxer, "mkv-use-ordered-chapters" ) ? (uint8(*static_cast<KaxEditionFlagOrdered *>( l )) != 0) : 0;
                }
                else if( MKV_IS_ID( l, KaxEditionFlagDefault ) )
                {
                    if (uint8(*static_cast<KaxEditionFlagDefault *>( l )) != 0)
                        i_default_edition = stored_editions.size();
                }
                else
                {
                    msg_Dbg( &sys.demuxer, "|   |   |   + Unknown (%s)", typeid(*l).name() );
                }
            }
            stored_editions.push_back( p_edition );
        }
        else
        {
            msg_Dbg( &sys.demuxer, "|   |   + Unknown (%s)", typeid(*l).name() );
        }
    }

    for( i = 0; i < stored_editions.size(); i++ )
    {
        stored_editions[i]->RefreshChapters( );
    }
 
    if ( stored_editions.size() != 0 && stored_editions[i_default_edition]->b_ordered )
    {
        /* update the duration of the segment according to the sum of all sub chapters */
        i_dur = stored_editions[i_default_edition]->Duration() / INT64_C(1000);
        if (i_dur > 0)
            i_duration = i_dur;
    }
}

void matroska_segment_c::ParseCluster( )
{
    EbmlElement *el;
    EbmlMaster  *m;
    unsigned int i;
    int i_upper_level = 0;

    /* Master elements */
    m = static_cast<EbmlMaster *>( cluster );
    m->Read( es, cluster->Generic().Context, i_upper_level, el, true );

    for( i = 0; i < m->ListSize(); i++ )
    {
        EbmlElement *l = (*m)[i];

        if( MKV_IS_ID( l, KaxClusterTimecode ) )
        {
            KaxClusterTimecode &ctc = *(KaxClusterTimecode*)l;

            cluster->InitTimecode( uint64( ctc ), i_timescale );
            break;
        }
    }

    i_start_time = cluster->GlobalTimecode() / 1000;
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
        p_indexes = (mkv_index_t*)realloc( p_indexes, sizeof( mkv_index_t ) * i_index_max );
    }
#undef idx
}

void chapter_edition_c::RefreshChapters( )
{
    chapter_item_c::RefreshChapters( b_ordered, -1 );
    b_display_seekpoint = false;
}

int64_t chapter_item_c::RefreshChapters( bool b_ordered, int64_t i_prev_user_time )
{
    int64_t i_user_time = i_prev_user_time;
 
    // first the sub-chapters, and then ourself
    std::vector<chapter_item_c*>::iterator index = sub_chapters.begin();
    while ( index != sub_chapters.end() )
    {
        i_user_time = (*index)->RefreshChapters( b_ordered, i_user_time );
        index++;
    }

    if ( b_ordered )
    {
        // the ordered chapters always start at zero
        if ( i_prev_user_time == -1 )
        {
            if ( i_user_time == -1 )
                i_user_time = 0;
            i_prev_user_time = 0;
        }

        i_user_start_time = i_prev_user_time;
        if ( i_end_time != -1 && i_user_time == i_prev_user_time )
        {
            i_user_end_time = i_user_start_time - i_start_time + i_end_time;
        }
        else
        {
            i_user_end_time = i_user_time;
        }
    }
    else
    {
        if ( sub_chapters.begin() != sub_chapters.end() )
            std::sort( sub_chapters.begin(), sub_chapters.end(), chapter_item_c::CompareTimecode );
        i_user_start_time = i_start_time;
        if ( i_end_time != -1 )
            i_user_end_time = i_end_time;
        else if ( i_user_time != -1 )
            i_user_end_time = i_user_time;
        else
            i_user_end_time = i_user_start_time;
    }

    return i_user_end_time;
}

mtime_t chapter_edition_c::Duration() const
{
    mtime_t i_result = 0;
 
    if ( sub_chapters.size() )
    {
        std::vector<chapter_item_c*>::const_iterator index = sub_chapters.end();
        index--;
        i_result = (*index)->i_user_end_time;
    }
 
    return i_result;
}

chapter_item_c * chapter_edition_c::FindTimecode( mtime_t i_timecode, const chapter_item_c * p_current )
{
    if ( !b_ordered )
        p_current = NULL;
    bool b_found_current = false;
    return chapter_item_c::FindTimecode( i_timecode, p_current, b_found_current );
}

chapter_item_c *chapter_item_c::FindTimecode( mtime_t i_user_timecode, const chapter_item_c * p_current, bool & b_found )
{
    chapter_item_c *psz_result = NULL;

    if ( p_current == this )
        b_found = true;

    if ( i_user_timecode >= i_user_start_time &&
        ( i_user_timecode < i_user_end_time ||
          ( i_user_start_time == i_user_end_time && i_user_timecode == i_user_end_time )))
    {
        std::vector<chapter_item_c*>::iterator index = sub_chapters.begin();
        while ( index != sub_chapters.end() && ((p_current == NULL && psz_result == NULL) || (p_current != NULL && (!b_found || psz_result == NULL))))
        {
            psz_result = (*index)->FindTimecode( i_user_timecode, p_current, b_found );
            index++;
        }
 
        if ( psz_result == NULL )
            psz_result = this;
    }

    return psz_result;
}

bool chapter_item_c::ParentOf( const chapter_item_c & item ) const
{
    if ( &item == this )
        return true;

    std::vector<chapter_item_c*>::const_iterator index = sub_chapters.begin();
    while ( index != sub_chapters.end() )
    {
        if ( (*index)->ParentOf( item ) )
            return true;
        index++;
    }

    return false;
}

void demux_sys_t::PreloadFamily( const matroska_segment_c & of_segment )
{
    for (size_t i=0; i<opened_segments.size(); i++)
    {
        opened_segments[i]->PreloadFamily( of_segment );
    }
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
            std::string sz_name;
            input_title_t *p_title = vlc_input_title_New();
            p_seg->i_sys_title = i;
            int i_chapters;

            // TODO use a name for each edition, let the TITLE deal with a codec name
            for ( j=0; j<p_seg->p_editions->size(); j++ )
            {
                if ( p_title->psz_name == NULL )
                {
                    sz_name = (*p_seg->p_editions)[j]->GetMainName();
                    if ( sz_name != "" )
                        p_title->psz_name = strdup( sz_name.c_str() );
                }

                chapter_edition_c *p_edition = (*p_seg->p_editions)[j];

                i_chapters = 0;
                p_edition->PublishChapters( *p_title, i_chapters, 0 );
            }

            // create a name if there is none
            if ( p_title->psz_name == NULL )
            {
                sz_name = N_("Segment");
                char psz_str[6];
                sprintf( psz_str, " %d", (int)i );
                sz_name += psz_str;
                p_title->psz_name = strdup( sz_name.c_str() );
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

void virtual_segment_c::Sort()
{
    // keep the current segment index
    matroska_segment_c *p_segment = linked_segments[i_current_segment];

    std::sort( linked_segments.begin(), linked_segments.end(), matroska_segment_c::CompareSegmentUIDs );

    for ( i_current_segment=0; i_current_segment<linked_segments.size(); i_current_segment++)
        if ( linked_segments[i_current_segment] == p_segment )
            break;
}

size_t virtual_segment_c::AddSegment( matroska_segment_c *p_segment )
{
    size_t i;
    // check if it's not already in here
    for ( i=0; i<linked_segments.size(); i++ )
    {
        if ( linked_segments[i]->p_segment_uid != NULL
            && p_segment->p_segment_uid != NULL
            && *p_segment->p_segment_uid == *linked_segments[i]->p_segment_uid )
            return 0;
    }

    // find possible mates
    for ( i=0; i<linked_uids.size(); i++ )
    {
        if (   (p_segment->p_segment_uid != NULL && *p_segment->p_segment_uid == linked_uids[i])
            || (p_segment->p_prev_segment_uid != NULL && *p_segment->p_prev_segment_uid == linked_uids[i])
            || (p_segment->p_next_segment_uid !=NULL && *p_segment->p_next_segment_uid == linked_uids[i]) )
        {
            linked_segments.push_back( p_segment );

            AppendUID( p_segment->p_prev_segment_uid );
            AppendUID( p_segment->p_next_segment_uid );

            return 1;
        }
    }
    return 0;
}

void virtual_segment_c::PreloadLinked( )
{
    for ( size_t i=0; i<linked_segments.size(); i++ )
    {
        linked_segments[i]->Preload( );
    }
    i_current_edition = linked_segments[0]->i_default_edition;
}

mtime_t virtual_segment_c::Duration() const
{
    mtime_t i_duration;
    if ( linked_segments.size() == 0 )
        i_duration = 0;
    else {
        matroska_segment_c *p_last_segment = linked_segments[linked_segments.size()-1];
//        p_last_segment->ParseCluster( );

        i_duration = p_last_segment->i_start_time / 1000 + p_last_segment->i_duration;
    }
    return i_duration;
}

void virtual_segment_c::AppendUID( const EbmlBinary * p_UID )
{
    if ( p_UID == NULL )
        return;
    if ( p_UID->GetBuffer() == NULL )
        return;

    for (size_t i=0; i<linked_uids.size(); i++)
    {
        if ( *p_UID == linked_uids[i] )
            return;
    }
    linked_uids.push_back( *(KaxSegmentUID*)(p_UID) );
}

void matroska_segment_c::Seek( mtime_t i_date, mtime_t i_time_offset, int64_t i_global_position )
{
    KaxBlock    *block;
    KaxSimpleBlock *simpleblock;
    int         i_track_skipping;
    int64_t     i_block_duration;
    int64_t     i_block_ref1;
    int64_t     i_block_ref2;
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
                    es_out_Control( sys.demuxer.out, ES_OUT_RESET_PCR );
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

    es_out_Control( sys.demuxer.out, ES_OUT_RESET_PCR );

    /* now parse until key frame */
    i_track_skipping = 0;
    for( i_track = 0; i_track < tracks.size(); i_track++ )
    {
        if( tracks[i_track]->fmt.i_cat == VIDEO_ES )
        {
            tracks[i_track]->b_search_keyframe = true;
            i_track_skipping++;
        }
        es_out_Control( sys.demuxer.out, ES_OUT_SET_NEXT_DISPLAY_TIME, tracks[i_track]->p_es, i_date );
    }

    while( i_track_skipping > 0 )
    {
        if( BlockGet( block, simpleblock, &i_block_ref1, &i_block_ref2, &i_block_duration ) )
        {
            msg_Warn( &sys.demuxer, "cannot get block EOF?" );

            return;
        }
        ep->Down();

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
            if( sys.i_pts >= sys.i_start_pts )
            {
                cluster = static_cast<KaxCluster*>(ep->UnGet( i_block_pos, i_cluster_pos ));
                i_track_skipping = 0;
            }
            else if( tracks[i_track]->fmt.i_cat == VIDEO_ES )
            {
                if( i_block_ref1 == 0 && tracks[i_track]->b_search_keyframe )
                {
                    tracks[i_track]->b_search_keyframe = false;
                    i_track_skipping--;
                }
                if( !tracks[i_track]->b_search_keyframe )
                {
                    
                    //es_out_Control( sys.demuxer.out, ES_OUT_SET_PCR, sys.i_pts );
                    BlockDecode( &sys.demuxer, block, simpleblock, sys.i_pts, 0, i_block_ref1 >= 0 || i_block_ref2 > 0 );
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

        for( i_track = 0; i_track < tracks.size(); i_track++ )
            es_out_Control( sys.demuxer.out, ES_OUT_SET_NEXT_DISPLAY_TIME, tracks[i_track]->p_es, sys.i_start_pts );
    }
}

int matroska_segment_c::BlockFindTrackIndex( size_t *pi_track,
                                             const KaxBlock *p_block, const KaxSimpleBlock *p_simpleblock )
{
    size_t          i_track;
    unsigned int    i;
    bool            b;

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

void virtual_segment_c::Seek( demux_t & demuxer, mtime_t i_date, mtime_t i_time_offset, chapter_item_c *psz_chapter, int64_t i_global_position )
{
    demux_sys_t *p_sys = demuxer.p_sys;
    size_t i;

    // find the actual time for an ordered edition
    if ( psz_chapter == NULL )
    {
        if ( Edition() && Edition()->b_ordered )
        {
            /* 1st, we need to know in which chapter we are */
            psz_chapter = (*p_editions)[i_current_edition]->FindTimecode( i_date, psz_current_chapter );
        }
    }

    if ( psz_chapter != NULL )
    {
        psz_current_chapter = psz_chapter;
        p_sys->i_chapter_time = i_time_offset = psz_chapter->i_user_start_time - psz_chapter->i_start_time;
        if ( psz_chapter->i_seekpoint_num > 0 )
        {
            demuxer.info.i_update |= INPUT_UPDATE_TITLE | INPUT_UPDATE_SEEKPOINT;
            demuxer.info.i_title = p_sys->i_current_title = i_sys_title;
            demuxer.info.i_seekpoint = psz_chapter->i_seekpoint_num - 1;
        }
    }

    // find the best matching segment
    for ( i=0; i<linked_segments.size(); i++ )
    {
        if ( i_date < linked_segments[i]->i_start_time )
            break;
    }

    if ( i > 0 )
        i--;

    if ( i_current_segment != i  )
    {
        linked_segments[i_current_segment]->UnSelect();
        linked_segments[i]->Select( i_date );
        i_current_segment = i;
    }

    linked_segments[i]->Seek( i_date, i_time_offset, i_global_position );
}

void chapter_codec_cmds_c::AddCommand( const KaxChapterProcessCommand & command )
{
    size_t i;

    uint32 codec_time = uint32(-1);
    for( i = 0; i < command.ListSize(); i++ )
    {
        const EbmlElement *k = command[i];

        if( MKV_IS_ID( k, KaxChapterProcessTime ) )
        {
            codec_time = uint32( *static_cast<const KaxChapterProcessTime*>( k ) );
            break;
        }
    }

    for( i = 0; i < command.ListSize(); i++ )
    {
        const EbmlElement *k = command[i];

        if( MKV_IS_ID( k, KaxChapterProcessData ) )
        {
            KaxChapterProcessData *p_data =  new KaxChapterProcessData( *static_cast<const KaxChapterProcessData*>( k ) );
            switch ( codec_time )
            {
            case 0:
                during_cmds.push_back( p_data );
                break;
            case 1:
                enter_cmds.push_back( p_data );
                break;
            case 2:
                leave_cmds.push_back( p_data );
                break;
            default:
                delete p_data;
            }
        }
    }
}

bool chapter_item_c::Enter( bool b_do_subs )
{
    bool f_result = false;
    std::vector<chapter_codec_cmds_c*>::iterator index = codecs.begin();
    while ( index != codecs.end() )
    {
        f_result |= (*index)->Enter();
        index++;
    }

    if ( b_do_subs )
    {
        // sub chapters
        std::vector<chapter_item_c*>::iterator index_ = sub_chapters.begin();
        while ( index_ != sub_chapters.end() )
        {
            f_result |= (*index_)->Enter( true );
            index_++;
        }
    }
    return f_result;
}

bool chapter_item_c::Leave( bool b_do_subs )
{
    bool f_result = false;
    b_is_leaving = true;
    std::vector<chapter_codec_cmds_c*>::iterator index = codecs.begin();
    while ( index != codecs.end() )
    {
        f_result |= (*index)->Leave();
        index++;
    }

    if ( b_do_subs )
    {
        // sub chapters
        std::vector<chapter_item_c*>::iterator index_ = sub_chapters.begin();
        while ( index_ != sub_chapters.end() )
        {
            f_result |= (*index_)->Leave( true );
            index_++;
        }
    }
    b_is_leaving = false;
    return f_result;
}

bool chapter_item_c::EnterAndLeave( chapter_item_c *p_item, bool b_final_enter )
{
    chapter_item_c *p_common_parent = p_item;

    // leave, up to a common parent
    while ( p_common_parent != NULL && !p_common_parent->ParentOf( *this ) )
    {
        if ( !p_common_parent->b_is_leaving && p_common_parent->Leave( false ) )
            return true;
        p_common_parent = p_common_parent->psz_parent;
    }

    // enter from the parent to <this>
    if ( p_common_parent != NULL )
    {
        do
        {
            if ( p_common_parent == this )
                return Enter( true );

            for ( size_t i = 0; i<p_common_parent->sub_chapters.size(); i++ )
            {
                if ( p_common_parent->sub_chapters[i]->ParentOf( *this ) )
                {
                    p_common_parent = p_common_parent->sub_chapters[i];
                    if ( p_common_parent != this )
                        if ( p_common_parent->Enter( false ) )
                            return true;

                    break;
                }
            }
        } while ( 1 );
    }

    if ( b_final_enter )
        return Enter( true );
    else
        return false;
}

bool dvd_chapter_codec_c::Enter()
{
    bool f_result = false;
    std::vector<KaxChapterProcessData*>::iterator index = enter_cmds.begin();
    while ( index != enter_cmds.end() )
    {
        if ( (*index)->GetSize() )
        {
            binary *p_data = (*index)->GetBuffer();
            size_t i_size = *p_data++;
            // avoid reading too much from the buffer
            i_size = __MIN( i_size, ((*index)->GetSize() - 1) >> 3 );
            for ( ; i_size > 0; i_size--, p_data += 8 )
            {
                msg_Dbg( &sys.demuxer, "Matroska DVD enter command" );
                f_result |= sys.dvd_interpretor.Interpret( p_data );
            }
        }
        index++;
    }
    return f_result;
}

bool dvd_chapter_codec_c::Leave()
{
    bool f_result = false;
    std::vector<KaxChapterProcessData*>::iterator index = leave_cmds.begin();
    while ( index != leave_cmds.end() )
    {
        if ( (*index)->GetSize() )
        {
            binary *p_data = (*index)->GetBuffer();
            size_t i_size = *p_data++;
            // avoid reading too much from the buffer
            i_size = __MIN( i_size, ((*index)->GetSize() - 1) >> 3 );
            for ( ; i_size > 0; i_size--, p_data += 8 )
            {
                msg_Dbg( &sys.demuxer, "Matroska DVD leave command" );
                f_result |= sys.dvd_interpretor.Interpret( p_data );
            }
        }
        index++;
    }
    return f_result;
}

// see http://www.dvd-replica.com/DVD/vmcmdset.php for a description of DVD commands
bool dvd_command_interpretor_c::Interpret( const binary * p_command, size_t i_size )
{
    if ( i_size != 8 )
        return false;

    virtual_segment_c *p_segment = NULL;
    chapter_item_c *p_chapter = NULL;
    bool f_result = false;
    uint16 i_command = ( p_command[0] << 8 ) + p_command[1];

    // handle register tests if there are some
    if ( (i_command & 0xF0) != 0 )
    {
        bool b_test_positive = true;//(i_command & CMD_DVD_IF_NOT) == 0;
        bool b_test_value    = (i_command & CMD_DVD_TEST_VALUE) != 0;
        uint8 i_test = i_command & 0x70;
        uint16 i_value;

        // see http://dvd.sourceforge.net/dvdinfo/vmi.html
        uint8  i_cr1;
        uint16 i_cr2;
        switch ( i_command >> 12 )
        {
        default:
            i_cr1 = p_command[3];
            i_cr2 = (p_command[4] << 8) + p_command[5];
            break;
        case 3:
        case 4:
        case 5:
            i_cr1 = p_command[6];
            i_cr2 = p_command[7];
            b_test_value = false;
            break;
        case 6:
        case 7:
            if ( ((p_command[1] >> 4) & 0x7) == 0)
            {
                i_cr1 = p_command[2];
                i_cr2 = (p_command[6] << 8) + p_command[7];
            }
            else
            {
                i_cr1 = p_command[2];
                i_cr2 = (p_command[6] << 8) + p_command[7];
            }
            break;
        }

        if ( b_test_value )
            i_value = i_cr2;
        else
            i_value = GetPRM( i_cr2 );

        switch ( i_test )
        {
        case CMD_DVD_IF_GPREG_EQUAL:
            // if equals
            msg_Dbg( &sys.demuxer, "IF %s EQUALS %s", GetRegTypeName( false, i_cr1 ).c_str(), GetRegTypeName( b_test_value, i_value ).c_str() );
            if (!( GetPRM( i_cr1 ) == i_value ))
            {
                b_test_positive = false;
            }
            break;
        case CMD_DVD_IF_GPREG_NOT_EQUAL:
            // if not equals
            msg_Dbg( &sys.demuxer, "IF %s NOT EQUALS %s", GetRegTypeName( false, i_cr1 ).c_str(), GetRegTypeName( b_test_value, i_value ).c_str() );
            if (!( GetPRM( i_cr1 ) != i_value ))
            {
                b_test_positive = false;
            }
            break;
        case CMD_DVD_IF_GPREG_INF:
            // if inferior
            msg_Dbg( &sys.demuxer, "IF %s < %s", GetRegTypeName( false, p_command[3] ).c_str(), GetRegTypeName( b_test_value, i_value ).c_str() );
            if (!( GetPRM( i_cr1 ) < i_value ))
            {
                b_test_positive = false;
            }
            break;
        case CMD_DVD_IF_GPREG_INF_EQUAL:
            // if inferior or equal
            msg_Dbg( &sys.demuxer, "IF %s < %s", GetRegTypeName( false, p_command[3] ).c_str(), GetRegTypeName( b_test_value, i_value ).c_str() );
            if (!( GetPRM( i_cr1 ) <= i_value ))
            {
                b_test_positive = false;
            }
            break;
        case CMD_DVD_IF_GPREG_AND:
            // if logical and
            msg_Dbg( &sys.demuxer, "IF %s & %s", GetRegTypeName( false, p_command[3] ).c_str(), GetRegTypeName( b_test_value, i_value ).c_str() );
            if (!( GetPRM( i_cr1 ) & i_value ))
            {
                b_test_positive = false;
            }
            break;
        case CMD_DVD_IF_GPREG_SUP:
            // if superior
            msg_Dbg( &sys.demuxer, "IF %s >= %s", GetRegTypeName( false, p_command[3] ).c_str(), GetRegTypeName( b_test_value, i_value ).c_str() );
            if (!( GetPRM( i_cr1 ) > i_value ))
            {
                b_test_positive = false;
            }
            break;
        case CMD_DVD_IF_GPREG_SUP_EQUAL:
            // if superior or equal
            msg_Dbg( &sys.demuxer, "IF %s >= %s", GetRegTypeName( false, p_command[3] ).c_str(), GetRegTypeName( b_test_value, i_value ).c_str() );
            if (!( GetPRM( i_cr1 ) >= i_value ))
            {
                b_test_positive = false;
            }
            break;
        }

        if ( !b_test_positive )
            return false;
    }
 
    // strip the test command
    i_command &= 0xFF0F;
 
    switch ( i_command )
    {
    case CMD_DVD_NOP:
    case CMD_DVD_NOP2:
        {
            msg_Dbg( &sys.demuxer, "NOP" );
            break;
        }
    case CMD_DVD_BREAK:
        {
            msg_Dbg( &sys.demuxer, "Break" );
            // TODO
            break;
        }
    case CMD_DVD_JUMP_TT:
        {
            uint8 i_title = p_command[5];
            msg_Dbg( &sys.demuxer, "JumpTT %d", i_title );

            // find in the ChapProcessPrivate matching this Title level
            p_chapter = sys.BrowseCodecPrivate( 1, MatchTitleNumber, &i_title, sizeof(i_title), p_segment );
            if ( p_segment != NULL )
            {
                sys.JumpTo( *p_segment, p_chapter );
                f_result = true;
            }

            break;
        }
    case CMD_DVD_CALLSS_VTSM1:
        {
            msg_Dbg( &sys.demuxer, "CallSS" );
            binary p_type;
            switch( (p_command[6] & 0xC0) >> 6 ) {
                case 0:
                    p_type = p_command[5] & 0x0F;
                    switch ( p_type )
                    {
                    case 0x00:
                        msg_Dbg( &sys.demuxer, "CallSS PGC (rsm_cell %x)", p_command[4]);
                        break;
                    case 0x02:
                        msg_Dbg( &sys.demuxer, "CallSS Title Entry (rsm_cell %x)", p_command[4]);
                        break;
                    case 0x03:
                        msg_Dbg( &sys.demuxer, "CallSS Root Menu (rsm_cell %x)", p_command[4]);
                        break;
                    case 0x04:
                        msg_Dbg( &sys.demuxer, "CallSS Subpicture Menu (rsm_cell %x)", p_command[4]);
                        break;
                    case 0x05:
                        msg_Dbg( &sys.demuxer, "CallSS Audio Menu (rsm_cell %x)", p_command[4]);
                        break;
                    case 0x06:
                        msg_Dbg( &sys.demuxer, "CallSS Angle Menu (rsm_cell %x)", p_command[4]);
                        break;
                    case 0x07:
                        msg_Dbg( &sys.demuxer, "CallSS Chapter Menu (rsm_cell %x)", p_command[4]);
                        break;
                    default:
                        msg_Dbg( &sys.demuxer, "CallSS <unknown> (rsm_cell %x)", p_command[4]);
                        break;
                    }
                    p_chapter = sys.BrowseCodecPrivate( 1, MatchPgcType, &p_type, 1, p_segment );
                    if ( p_segment != NULL )
                    {
                        sys.JumpTo( *p_segment, p_chapter );
                        f_result = true;
                    }
                break;
                case 1:
                    msg_Dbg( &sys.demuxer, "CallSS VMGM (menu %d, rsm_cell %x)", p_command[5] & 0x0F, p_command[4]);
                break;
                case 2:
                    msg_Dbg( &sys.demuxer, "CallSS VTSM (menu %d, rsm_cell %x)", p_command[5] & 0x0F, p_command[4]);
                break;
                case 3:
                    msg_Dbg( &sys.demuxer, "CallSS VMGM (pgc %d, rsm_cell %x)", (p_command[2] << 8) + p_command[3], p_command[4]);
                break;
            }
            break;
        }
    case CMD_DVD_JUMP_SS:
        {
            msg_Dbg( &sys.demuxer, "JumpSS");
            binary p_type;
            switch( (p_command[5] & 0xC0) >> 6 ) {
                case 0:
                    msg_Dbg( &sys.demuxer, "JumpSS FP");
                break;
                case 1:
                    p_type = p_command[5] & 0x0F;
                    switch ( p_type )
                    {
                    case 0x02:
                        msg_Dbg( &sys.demuxer, "JumpSS VMGM Title Entry");
                        break;
                    case 0x03:
                        msg_Dbg( &sys.demuxer, "JumpSS VMGM Root Menu");
                        break;
                    case 0x04:
                        msg_Dbg( &sys.demuxer, "JumpSS VMGM Subpicture Menu");
                        break;
                    case 0x05:
                        msg_Dbg( &sys.demuxer, "JumpSS VMGM Audio Menu");
                        break;
                    case 0x06:
                        msg_Dbg( &sys.demuxer, "JumpSS VMGM Angle Menu");
                        break;
                    case 0x07:
                        msg_Dbg( &sys.demuxer, "JumpSS VMGM Chapter Menu");
                        break;
                    default:
                        msg_Dbg( &sys.demuxer, "JumpSS <unknown>");
                        break;
                    }
                    // find the VMG
                    p_chapter = sys.BrowseCodecPrivate( 1, MatchIsVMG, NULL, 0, p_segment );
                    if ( p_segment != NULL )
                    {
                        p_chapter = p_segment->BrowseCodecPrivate( 1, MatchPgcType, &p_type, 1 );
                        if ( p_chapter != NULL )
                        {
                            sys.JumpTo( *p_segment, p_chapter );
                            f_result = true;
                        }
                    }
                break;
                case 2:
                    p_type = p_command[5] & 0x0F;
                    switch ( p_type )
                    {
                    case 0x02:
                        msg_Dbg( &sys.demuxer, "JumpSS VTSM (vts %d, ttn %d) Title Entry", p_command[4], p_command[3]);
                        break;
                    case 0x03:
                        msg_Dbg( &sys.demuxer, "JumpSS VTSM (vts %d, ttn %d) Root Menu", p_command[4], p_command[3]);
                        break;
                    case 0x04:
                        msg_Dbg( &sys.demuxer, "JumpSS VTSM (vts %d, ttn %d) Subpicture Menu", p_command[4], p_command[3]);
                        break;
                    case 0x05:
                        msg_Dbg( &sys.demuxer, "JumpSS VTSM (vts %d, ttn %d) Audio Menu", p_command[4], p_command[3]);
                        break;
                    case 0x06:
                        msg_Dbg( &sys.demuxer, "JumpSS VTSM (vts %d, ttn %d) Angle Menu", p_command[4], p_command[3]);
                        break;
                    case 0x07:
                        msg_Dbg( &sys.demuxer, "JumpSS VTSM (vts %d, ttn %d) Chapter Menu", p_command[4], p_command[3]);
                        break;
                    default:
                        msg_Dbg( &sys.demuxer, "JumpSS VTSM (vts %d, ttn %d) <unknown>", p_command[4], p_command[3]);
                        break;
                    }

                    p_chapter = sys.BrowseCodecPrivate( 1, MatchVTSMNumber, &p_command[4], 1, p_segment );

                    if ( p_segment != NULL && p_chapter != NULL )
                    {
                        // find the title in the VTS
                        p_chapter = p_chapter->BrowseCodecPrivate( 1, MatchTitleNumber, &p_command[3], 1 );
                        if ( p_chapter != NULL )
                        {
                            // find the specified menu in the VTSM
                            p_chapter = p_segment->BrowseCodecPrivate( 1, MatchPgcType, &p_type, 1 );
                            if ( p_chapter != NULL )
                            {
                                sys.JumpTo( *p_segment, p_chapter );
                                f_result = true;
                            }
                        }
                        else
                            msg_Dbg( &sys.demuxer, "Title (%d) does not exist in this VTS", p_command[3] );
                    }
                    else
                        msg_Dbg( &sys.demuxer, "DVD Domain VTS (%d) not found", p_command[4] );
                break;
                case 3:
                    msg_Dbg( &sys.demuxer, "JumpSS VMGM (pgc %d)", (p_command[2] << 8) + p_command[3]);
                break;
            }
            break;
        }
    case CMD_DVD_JUMPVTS_PTT:
        {
            uint8 i_title = p_command[5];
            uint8 i_ptt = p_command[3];

            msg_Dbg( &sys.demuxer, "JumpVTS Title (%d) PTT (%d)", i_title, i_ptt);

            // find the current VTS content segment
            p_chapter = sys.p_current_segment->BrowseCodecPrivate( 1, MatchIsDomain, NULL, 0 );
            if ( p_chapter != NULL )
            {
                int16 i_curr_title = p_chapter->GetTitleNumber( );
                if ( i_curr_title > 0 )
                {
                    p_chapter = sys.BrowseCodecPrivate( 1, MatchVTSNumber, &i_curr_title, sizeof(i_curr_title), p_segment );

                    if ( p_segment != NULL && p_chapter != NULL )
                    {
                        // find the title in the VTS
                        p_chapter = p_chapter->BrowseCodecPrivate( 1, MatchTitleNumber, &i_title, sizeof(i_title) );
                        if ( p_chapter != NULL )
                        {
                            // find the chapter in the title
                            p_chapter = p_chapter->BrowseCodecPrivate( 1, MatchChapterNumber, &i_ptt, sizeof(i_ptt) );
                            if ( p_chapter != NULL )
                            {
                                sys.JumpTo( *p_segment, p_chapter );
                                f_result = true;
                            }
                        }
                    else
                        msg_Dbg( &sys.demuxer, "Title (%d) does not exist in this VTS", i_title );
                    }
                    else
                        msg_Dbg( &sys.demuxer, "DVD Domain VTS (%d) not found", i_curr_title );
                }
                else
                    msg_Dbg( &sys.demuxer, "JumpVTS_PTT command found but not in a VTS(M)");
            }
            else
                msg_Dbg( &sys.demuxer, "JumpVTS_PTT command but the DVD domain wasn't found");
            break;
        }
    case CMD_DVD_SET_GPRMMD:
        {
            msg_Dbg( &sys.demuxer, "Set GPRMMD [%d]=%d", (p_command[4] << 8) + p_command[5], (p_command[2] << 8) + p_command[3]);
 
            if ( !SetGPRM( (p_command[4] << 8) + p_command[5], (p_command[2] << 8) + p_command[3] ) )
                msg_Dbg( &sys.demuxer, "Set GPRMMD failed" );
            break;
        }
    case CMD_DVD_LINKPGCN:
        {
            uint16 i_pgcn = (p_command[6] << 8) + p_command[7];
 
            msg_Dbg( &sys.demuxer, "Link PGCN(%d)", i_pgcn );
            p_chapter = sys.p_current_segment->BrowseCodecPrivate( 1, MatchPgcNumber, &i_pgcn, 2 );
            if ( p_chapter != NULL )
            {
                if ( !p_chapter->Enter( true ) )
                    // jump to the location in the found segment
                    sys.p_current_segment->Seek( sys.demuxer, p_chapter->i_user_start_time, -1, p_chapter, -1 );

                f_result = true;
            }
            break;
        }
    case CMD_DVD_LINKCN:
        {
            uint8 i_cn = p_command[7];
 
            p_chapter = sys.p_current_segment->CurrentChapter();

            msg_Dbg( &sys.demuxer, "LinkCN (cell %d)", i_cn );
            p_chapter = p_chapter->BrowseCodecPrivate( 1, MatchCellNumber, &i_cn, 1 );
            if ( p_chapter != NULL )
            {
                if ( !p_chapter->Enter( true ) )
                    // jump to the location in the found segment
                    sys.p_current_segment->Seek( sys.demuxer, p_chapter->i_user_start_time, -1, p_chapter, -1 );

                f_result = true;
            }
            break;
        }
    case CMD_DVD_GOTO_LINE:
        {
            msg_Dbg( &sys.demuxer, "GotoLine (%d)", (p_command[6] << 8) + p_command[7] );
            // TODO
            break;
        }
    case CMD_DVD_SET_HL_BTNN1:
        {
            msg_Dbg( &sys.demuxer, "SetHL_BTN (%d)", p_command[4] );
            SetSPRM( 0x88, p_command[4] );
            break;
        }
    default:
        {
            msg_Dbg( &sys.demuxer, "unsupported command : %02X %02X %02X %02X %02X %02X %02X %02X"
                     ,p_command[0]
                     ,p_command[1]
                     ,p_command[2]
                     ,p_command[3]
                     ,p_command[4]
                     ,p_command[5]
                     ,p_command[6]
                     ,p_command[7]);
            break;
        }
    }

    return f_result;
}

bool dvd_command_interpretor_c::MatchIsDomain( const chapter_codec_cmds_c &data, const void *p_cookie, size_t i_cookie_size )
{
    return ( data.p_private_data != NULL && data.p_private_data->GetBuffer()[0] == MATROSKA_DVD_LEVEL_SS );
}

bool dvd_command_interpretor_c::MatchIsVMG( const chapter_codec_cmds_c &data, const void *p_cookie, size_t i_cookie_size )
{
    if ( data.p_private_data == NULL || data.p_private_data->GetSize() < 2 )
        return false;

    return ( data.p_private_data->GetBuffer()[0] == MATROSKA_DVD_LEVEL_SS && data.p_private_data->GetBuffer()[1] == 0xC0);
}

bool dvd_command_interpretor_c::MatchVTSNumber( const chapter_codec_cmds_c &data, const void *p_cookie, size_t i_cookie_size )
{
    if ( i_cookie_size != 2 || data.p_private_data == NULL || data.p_private_data->GetSize() < 4 )
        return false;
 
    if ( data.p_private_data->GetBuffer()[0] != MATROSKA_DVD_LEVEL_SS || data.p_private_data->GetBuffer()[1] != 0x80 )
        return false;

    uint16 i_gtitle = (data.p_private_data->GetBuffer()[2] << 8 ) + data.p_private_data->GetBuffer()[3];
    uint16 i_title = *(uint16*)p_cookie;

    return (i_gtitle == i_title);
}

bool dvd_command_interpretor_c::MatchVTSMNumber( const chapter_codec_cmds_c &data, const void *p_cookie, size_t i_cookie_size )
{
    if ( i_cookie_size != 1 || data.p_private_data == NULL || data.p_private_data->GetSize() < 4 )
        return false;
 
    if ( data.p_private_data->GetBuffer()[0] != MATROSKA_DVD_LEVEL_SS || data.p_private_data->GetBuffer()[1] != 0x40 )
        return false;

    uint8 i_gtitle = data.p_private_data->GetBuffer()[3];
    uint8 i_title = *(uint8*)p_cookie;

    return (i_gtitle == i_title);
}

bool dvd_command_interpretor_c::MatchTitleNumber( const chapter_codec_cmds_c &data, const void *p_cookie, size_t i_cookie_size )
{
    if ( i_cookie_size != 1 || data.p_private_data == NULL || data.p_private_data->GetSize() < 4 )
        return false;
 
    if ( data.p_private_data->GetBuffer()[0] != MATROSKA_DVD_LEVEL_TT )
        return false;

    uint16 i_gtitle = (data.p_private_data->GetBuffer()[1] << 8 ) + data.p_private_data->GetBuffer()[2];
    uint8 i_title = *(uint8*)p_cookie;

    return (i_gtitle == i_title);
}

bool dvd_command_interpretor_c::MatchPgcType( const chapter_codec_cmds_c &data, const void *p_cookie, size_t i_cookie_size )
{
    if ( i_cookie_size != 1 || data.p_private_data == NULL || data.p_private_data->GetSize() < 8 )
        return false;
 
    if ( data.p_private_data->GetBuffer()[0] != MATROSKA_DVD_LEVEL_PGC )
        return false;

    uint8 i_pgc_type = data.p_private_data->GetBuffer()[3] & 0x0F;
    uint8 i_pgc = *(uint8*)p_cookie;

    return (i_pgc_type == i_pgc);
}

bool dvd_command_interpretor_c::MatchPgcNumber( const chapter_codec_cmds_c &data, const void *p_cookie, size_t i_cookie_size )
{
    if ( i_cookie_size != 2 || data.p_private_data == NULL || data.p_private_data->GetSize() < 8 )
        return false;
 
    if ( data.p_private_data->GetBuffer()[0] != MATROSKA_DVD_LEVEL_PGC )
        return false;

    uint16 *i_pgc_n = (uint16 *)p_cookie;
    uint16 i_pgc_num = (data.p_private_data->GetBuffer()[1] << 8) + data.p_private_data->GetBuffer()[2];

    return (i_pgc_num == *i_pgc_n);
}

bool dvd_command_interpretor_c::MatchChapterNumber( const chapter_codec_cmds_c &data, const void *p_cookie, size_t i_cookie_size )
{
    if ( i_cookie_size != 1 || data.p_private_data == NULL || data.p_private_data->GetSize() < 2 )
        return false;
 
    if ( data.p_private_data->GetBuffer()[0] != MATROSKA_DVD_LEVEL_PTT )
        return false;

    uint8 i_chapter = data.p_private_data->GetBuffer()[1];
    uint8 i_ptt = *(uint8*)p_cookie;

    return (i_chapter == i_ptt);
}

bool dvd_command_interpretor_c::MatchCellNumber( const chapter_codec_cmds_c &data, const void *p_cookie, size_t i_cookie_size )
{
    if ( i_cookie_size != 1 || data.p_private_data == NULL || data.p_private_data->GetSize() < 5 )
        return false;
 
    if ( data.p_private_data->GetBuffer()[0] != MATROSKA_DVD_LEVEL_CN )
        return false;

    uint8 *i_cell_n = (uint8 *)p_cookie;
    uint8 i_cell_num = data.p_private_data->GetBuffer()[3];

    return (i_cell_num == *i_cell_n);
}

bool matroska_script_codec_c::Enter()
{
    bool f_result = false;
    std::vector<KaxChapterProcessData*>::iterator index = enter_cmds.begin();
    while ( index != enter_cmds.end() )
    {
        if ( (*index)->GetSize() )
        {
            msg_Dbg( &sys.demuxer, "Matroska Script enter command" );
            f_result |= interpretor.Interpret( (*index)->GetBuffer(), (*index)->GetSize() );
        }
        index++;
    }
    return f_result;
}

bool matroska_script_codec_c::Leave()
{
    bool f_result = false;
    std::vector<KaxChapterProcessData*>::iterator index = leave_cmds.begin();
    while ( index != leave_cmds.end() )
    {
        if ( (*index)->GetSize() )
        {
            msg_Dbg( &sys.demuxer, "Matroska Script leave command" );
            f_result |= interpretor.Interpret( (*index)->GetBuffer(), (*index)->GetSize() );
        }
        index++;
    }
    return f_result;
}

// see http://www.matroska.org/technical/specs/chapters/index.html#mscript
//  for a description of existing commands
bool matroska_script_interpretor_c::Interpret( const binary * p_command, size_t i_size )
{
    bool b_result = false;

    char *psz_str = (char*) malloc( i_size + 1 );
    memcpy( psz_str, p_command, i_size );
    psz_str[ i_size ] = '\0';

    std::string sz_command = psz_str;
    free( psz_str );

    msg_Dbg( &sys.demuxer, "command : %s", sz_command.c_str() );

#if defined(__GNUC__) && (__GNUC__ < 3)
    if ( sz_command.compare( CMD_MS_GOTO_AND_PLAY, 0, CMD_MS_GOTO_AND_PLAY.size() ) == 0 )
#else
    if ( sz_command.compare( 0, CMD_MS_GOTO_AND_PLAY.size(), CMD_MS_GOTO_AND_PLAY ) == 0 )
#endif
    {
        size_t i,j;

        // find the (
        for ( i=CMD_MS_GOTO_AND_PLAY.size(); i<sz_command.size(); i++)
        {
            if ( sz_command[i] == '(' )
            {
                i++;
                break;
            }
        }
        // find the )
        for ( j=i; j<sz_command.size(); j++)
        {
            if ( sz_command[j] == ')' )
            {
                i--;
                break;
            }
        }

        std::string st = sz_command.substr( i+1, j-i-1 );
        int64_t i_chapter_uid = atoi( st.c_str() );

        virtual_segment_c *p_segment;
        chapter_item_c *p_chapter = sys.FindChapter( i_chapter_uid, p_segment );

        if ( p_chapter == NULL )
            msg_Dbg( &sys.demuxer, "Chapter %"PRId64" not found", i_chapter_uid);
        else
        {
            if ( !p_chapter->EnterAndLeave( sys.p_current_segment->CurrentChapter() ) )
                p_segment->Seek( sys.demuxer, p_chapter->i_user_start_time, -1, p_chapter, -1 );
            b_result = true;
        }
    }

    return b_result;
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
