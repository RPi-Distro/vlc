/* Increase this value if you add a new profile */
#define NB_PROFILE 12

static const char *const video_profile_name_list[] = {
    "Video - H.264 + AAC (TS)",
    "Video - Dirac + AAC (TS)",
    "Video - Theora + Vorbis (OGG)",
    "Video - Theora + Flac (OGG)",
    "Video - MPEG-4 + AAC (MP4)",
    "Video - MPEG-2 + MPGA (TS)",
    "Video - WMV + WMA (ASF)",
    "Video - DIV3 + MP3 (ASF)",
    "Audio - Vorbis (OGG)",
    "Audio - MP3",
    "Audio - AAC (MP4)",
    "Audio - FLAC",
};

static const char *const video_profile_value_list[] = {
    /* Container(string), transcode video(bool), transcode audio(bool), */
    /* use subtitles(bool), video codec(string), video bitrate(integer), */
    /* scale(float), fps(float), width(integer, height(integer), */
    /* audio codec(string), audio bitrate(integer), channels(integer), */
    /* samplerate(integer), subtitle codec(string), subtitle overlay(bool) */
    "ts;1;1;0;h264;800;1;0;0;0;mp4a;128;2;44100;0;0",
    "ts;1;1;0;drac;800;1;0;0;0;mp4a;128;2;44100;0;0",
    "ogg;1;1;0;theo;800;1;0;0;0;vorb;128;2;44100;0;0",
    "ogg;1;1;0;theo;800;1;0;0;0;flac;128;2;44100;0;0",
    "mp4;1;1;0;mp4v;800;1;0;0;0;mp4a;128;2;44100;0;0",
    "ts;1;1;0;mp2v;800;1;0;0;0;mpga;128;2;44100;0;0",
    "asf;1;1;0;WMV2;800;1;0;0;0;wma2;128;2;44100;0;0",
    "asf;1;1;0;DIV3;800;1;0;0;0;mp3;128;2;44100;0;0",
    "ogg;0;1;0;0;800;1;0;0;0;vorb;128;2;44100;0;0",
    "raw;0;1;0;0;800;1;0;0;0;mp3;128;2;44100;0;0",
    "mp4;0;1;0;0;800;1;0;0;0;mp4a;128;2;44100;0;0",
    "raw;0;1;0;0;800;1;0;0;0;flac;128;2;44100;0;0",
};


