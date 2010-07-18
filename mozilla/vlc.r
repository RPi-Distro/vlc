/*****************************************************************************
 * VLC Plugin description for OS X
 *****************************************************************************/

/* Definitions of system resource types */

data 'carb' (0)
{
};

/* The first string in the array is a plugin description,
 * the second is the plugin name */
resource 'STR#' (126)
{
    {
        "Version 0.8.6, copyright 1996-2008 The VideoLAN Team"
        "<BR><A HREF='http://www.videolan.org'>http://www.videolan.org</A>",
        "VLC Multimedia Plug-in"
    };
};

/* A description for each MIME type in resource 128 */
resource 'STR#' (127)
{
    {
        /* MPEG-1 and MPEG-2 */
        "MPEG audio",
        "MPEG audio",
        "MPEG video",
        "MPEG video",
        "MPEG video",
        "MPEG video",
        /* MPEG-4 */
        "MPEG-4 video",
        "MPEG-4 audio",
        "MPEG-4 video",
        "MPEG-4 video",
        /* AVI */
        "AVI video",
        /* Quicktime */
/*        "QuickTime video", */
        /* OGG */
        "Ogg stream",
        "Ogg stream",
        /* VLC */
        "VLC plug-in",
        /* Windows Media */
        "Windows Media video",
        "Windows Media video",
        "Windows Media plug-in",
        "Windows Media video",
        /* Google VLC */
        "Google VLC plug-in",
        /* WAV audio */
        "WAV audio",
        "WAV audio",
        /* 3GPP */
        "3GPP audio",
        "3GPP video",
        /* 3GPP2 */
        "3GPP2 audio",
        "3GPP2 video",
        /* DIVX */
        "DivX video",
    };
};

/* A series of pairs of strings... first MIME type, then file extension(s) */
resource 'STR#' (128,"MIME Type")
{
    {
        /* MPEG-1 and MPEG-2 */
        "audio/mpeg", "mp2,mp3,mpga,mpega",
        "audio/x-mpeg", "mp2,mp3,mpga,mpega",
        "video/mpeg", "mpg,mpeg,mpe",
        "video/x-mpeg", "mpg,mpeg,mpe",
        "video/mpeg-system", "mpg,mpeg,vob",
        "video/x-mpeg-system", "mpg,mpeg,vob",
        /* MPEG-4 */
        "video/mpeg4", "mp4,mpg4",
        "audio/mpeg4", "mp4,mpg4",
        "application/mpeg4-iod", "mp4,mpg4",
        "application/mpeg4-muxcodetable", "mp4,mpg4",
        /* AVI */
        "video/x-msvideo", "avi",
        /* Quicktime */
/*        "video/quicktime", "mov,qt", */
        /* OGG */
        "application/ogg", "ogg",
        "application/x-ogg", "ogg",
        /* VLC */
        "application/x-vlc-plugin", "vlc",
        /* Windows Media */
        "video/x-ms-asf-plugin", "asf,asx",
        "video/x-ms-asf", "asf,asx",
        "application/x-mplayer2", "",
        "video/x-ms-wmv", "wmv",
        /* Google VLC */
        "video/x-google-vlc-plugin", "",
        /* WAV audio */
        "audio/wav", "wav",
        "audio/x-wav", "wav",
        /* 3GPP */
        "audio/3gpp", "3gp,3gpp",
        "video/3gpp", "3gp,3gpp",
        /* 3GPP2 */
        "audio/3gpp2", "3g2,3gpp2",
        "video/3gpp2", "3g2,3gpp2",
        /* DIVX */
        "video/divx", "divx",
    };
};

