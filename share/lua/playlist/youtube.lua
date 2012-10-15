--[[
 $Id$

 Copyright © 2007-2012 the VideoLAN team

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
--]]

-- Helper function to get a parameter's value in a URL
function get_url_param( url, name )
    local _, _, res = string.find( url, "[&?]"..name.."=([^&]*)" )
    return res
end

function get_arturl()
    local iurl = get_url_param( vlc.path, "iurl" )
    if iurl then
        return iurl
    end
    local video_id = get_url_param( vlc.path, "v" )
    if not video_id then
        return nil
    end
    return "http://img.youtube.com/vi/"..video_id.."/default.jpg"
end

function get_prefres()
    local prefres = -1
    if vlc.var and vlc.var.inherit then
        prefres = vlc.var.inherit(nil, "preferred-resolution")
        if prefres == nil then
            prefres = -1
        end
    end
    return prefres
end

-- Pick the most suited format available
function get_fmt( fmt_list )
    local prefres = get_prefres()
    if prefres < 0 then
        return nil
    end

    local fmt = nil
    for itag,height in string.gmatch( fmt_list, "(%d+)/%d+x(%d+)/[^,]+" ) do
        -- Apparently formats are listed in quality
        -- order, so we take the first one that works,
        -- or fallback to the lowest quality
        fmt = itag
        if tonumber(height) <= prefres then
            break
        end
    end
    return fmt
end

-- Parse and pick our video URL
function pick_url( url_map, fmt )
    local path = nil
    for stream in string.gmatch( url_map, "[^,]+" ) do
        -- Apparently formats are listed in quality order,
        -- so we can afford to simply take the first one
        local itag = string.match( stream, "itag=(%d+)" )
        if not fmt or not itag or tonumber( itag ) == tonumber( fmt ) then
            local url = string.match( stream, "url=([^&,]+)" )
            if url then
                url = vlc.strings.decode_uri( url )

                local sig = string.match( stream, "sig=([^&,]+)" )
                local signature = ""
                if sig then
                    signature = "&signature="..sig
                end

                path = url..signature
                break
            end
        end
    end
    return path
end

-- Probe function.
function probe()
    if vlc.access ~= "http" and vlc.access ~= "https" then
        return false
    end
    youtube_site = string.match( string.sub( vlc.path, 1, 8 ), "youtube" )
    if not youtube_site then
        -- FIXME we should be using a builtin list of known youtube websites
        -- like "fr.youtube.com", "uk.youtube.com" etc..
        youtube_site = string.find( vlc.path, ".youtube.com" )
        if youtube_site == nil then
            return false
        end
    end
    return (  string.match( vlc.path, "/watch%?" ) -- the html page
            or string.match( vlc.path, "/get_video_info%?" ) -- info API
            or string.match( vlc.path, "/v/" ) -- video in swf player
            or string.match( vlc.path, "/player2.swf" ) ) -- another player url
end

-- Parse function.
function parse()
    if string.match( vlc.path, "/watch%?" )
    then -- This is the HTML page's URL
        -- fmt is the format of the video
        -- (cf. http://en.wikipedia.org/wiki/YouTube#Quality_and_codecs)
        fmt = get_url_param( vlc.path, "fmt" )
        while true do
            -- Try to find the video's title
            line = vlc.readline()
            if not line then break end
            if string.match( line, "<meta name=\"title\"" ) then
                _,_,name = string.find( line, "content=\"(.-)\"" )
                name = vlc.strings.resolve_xml_special_chars( name )
                name = vlc.strings.resolve_xml_special_chars( name )
            end
            if string.match( line, "<meta name=\"description\"" ) then
               -- Don't ask me why they double encode ...
                _,_,description = string.find( line, "content=\"(.-)\"" )
                description = vlc.strings.resolve_xml_special_chars( description )
                description = vlc.strings.resolve_xml_special_chars( description )
            end
            if string.match( line, "<meta property=\"og:image\"" ) then
                _,_,arturl = string.find( line, "content=\"(.-)\"" )
            end
            if string.match( line, " rel=\"author\"" ) then
                _,_,artist = string.find( line, "href=\"/user/([^\"]*)\"" )
            end
            -- JSON parameters, also formerly known as "swfConfig",
            -- "SWF_ARGS", "swfArgs", "PLAYER_CONFIG" ...
            if string.match( line, "playerConfig" ) then
                if not fmt then
                    fmt_list = string.match( line, "\"fmt_list\": \"(.-)\"" )
                    if fmt_list then
                        fmt_list = string.gsub( fmt_list, "\\/", "/" )
                        fmt = get_fmt( fmt_list )
                    end
                end

                url_map = string.match( line, "\"url_encoded_fmt_stream_map\": \"(.-)\"" )
                if url_map then
                    -- FIXME: do this properly
                    url_map = string.gsub( url_map, "\\u0026", "&" )
                    path = pick_url( url_map, fmt )
                end

                if not path then
                    -- If this is a live stream, the URL map will be empty
                    -- and we get the URL from this field instead 
                    local hlsvp = string.match( line, "\"hlsvp\": \"(.-)\"" )
                    if hlsvp then
                        hlsvp = string.gsub( hlsvp, "\\/", "/" )
                        path = hlsvp
                    end
                end
            -- There is also another version of the parameters, encoded
            -- differently, as an HTML attribute of an <object> or <embed>
            -- tag; but we don't need it now
            end
        end

        if not path then
            local video_id = get_url_param( vlc.path, "v" )
            if video_id then
                if fmt then
                    format = "&fmt=" .. fmt
                else
                    format = ""
                end 
                -- Without "el=detailpage", /get_video_info fails for many
                -- music videos with errors about copyrighted content being
                -- "restricted from playback on certain sites"
                path = "http://www.youtube.com/get_video_info?video_id="..video_id..format.."&el=detailpage"
                vlc.msg.warn( "Couldn't extract video URL, falling back to alternate youtube API" )
            end
        end

        if not path then
            vlc.msg.err( "Couldn't extract youtube video URL, please check for updates to this script" )
            return { }
        end

        if not arturl then
            arturl = get_arturl()
        end

        return { { path = path; name = name; description = description; artist = artist; arturl = arturl } }

    elseif string.match( vlc.path, "/get_video_info%?" ) then -- video info API
        local line = vlc.readline() -- data is on one line only

        local fmt = get_url_param( vlc.path, "fmt" )
        if not fmt then
            local fmt_list = string.match( line, "&fmt_list=([^&]*)" )
            if fmt_list then
                fmt_list = vlc.strings.decode_uri( fmt_list )
                fmt = get_fmt( fmt_list )
            end
        end

        local url_map = string.match( line, "&url_encoded_fmt_stream_map=([^&]*)" )
        if url_map then
            url_map = vlc.strings.decode_uri( url_map )
            path = pick_url( url_map, fmt )
        end

        if not path then
            -- If this is a live stream, the URL map will be empty
            -- and we get the URL from this field instead 
            local hlsvp = string.match( line, "&hlsvp=([^&]*)" )
            if hlsvp then
                hlsvp = vlc.strings.decode_uri( hlsvp )
                path = hlsvp
            end
        end

        if not path then
            vlc.msg.err( "Couldn't extract youtube video URL, please check for updates to this script" )
            return { }
        end

        local title = string.match( line, "&title=([^&]*)" )
        if title then
            title = string.gsub( title, "+", " " )
            title = vlc.strings.decode_uri( title )
        end
        local artist = string.match( line, "&author=([^&]*)" )
        local arturl = string.match( line, "&thumbnail_url=([^&]*)" )
        if arturl then
            arturl = vlc.strings.decode_uri( arturl )
        end

        return { { path = path, title = title, artist = artist, arturl = arturl } }

    else -- This is the flash player's URL
        video_id = get_url_param( vlc.path, "video_id" )
        if not video_id then
            _,_,video_id = string.find( vlc.path, "/v/([^?]*)" )
        end
        if not video_id then
            vlc.msg.err( "Couldn't extract youtube video URL" )
            return { }
        end
        fmt = get_url_param( vlc.path, "fmt" )
        if fmt then
            format = "&fmt=" .. fmt
        else
            format = ""
        end
        return { { path = "http://www.youtube.com/watch?v="..video_id..format } }
    end
end
