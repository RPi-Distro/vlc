/*
 * AbstractAbstractPlaylist.cpp
 *****************************************************************************
 * Copyright (C) 2010 - 2011 Klagenfurt University
 * Copyright (C) 2015 VideoLAN and VLC authors
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "AbstractPlaylist.hpp"
#include "../tools/Helper.h"
#include "BasePeriod.h"
#include "SegmentTimeline.h"
#include <vlc_common.h>
#include <vlc_stream.h>

#include <algorithm>

using namespace adaptive::playlist;

AbstractPlaylist::AbstractPlaylist (vlc_object_t *p_object_) :
    ICanonicalUrl(),
    p_object(p_object_)
{
    playbackStart.Set(0);
    availabilityStartTime.Set( 0 );
    availabilityEndTime.Set( 0 );
    duration.Set( 0 );
    minUpdatePeriod.Set( 2 * CLOCK_FREQ );
    maxSegmentDuration.Set( 0 );
    minBufferTime = 0;
    timeShiftBufferDepth.Set( 0 );
    suggestedPresentationDelay.Set( 0 );
}

AbstractPlaylist::~AbstractPlaylist()
{
    for(size_t i = 0; i < this->periods.size(); i++)
        delete(this->periods.at(i));
}

const std::vector<BasePeriod *>& AbstractPlaylist::getPeriods()
{
    return periods;
}

void AbstractPlaylist::addBaseUrl(const std::string &url)
{
    baseUrls.push_back(url);
}

void AbstractPlaylist::setPlaylistUrl(const std::string &url)
{
    playlistUrl = url;
}

void AbstractPlaylist::addPeriod(BasePeriod *period)
{
    periods.push_back(period);
}

void AbstractPlaylist::setType(const std::string &type_)
{
    type = type_;
}

void AbstractPlaylist::setMinBuffering( mtime_t min )
{
    minBufferTime = min;
}

mtime_t AbstractPlaylist::getMinBuffering() const
{
    return std::max(minBufferTime, 6*CLOCK_FREQ);
}

mtime_t AbstractPlaylist::getMaxBuffering() const
{
    const mtime_t minbuf = getMinBuffering();
    return std::max(minbuf, 60 * CLOCK_FREQ);
}

Url AbstractPlaylist::getUrlSegment() const
{
    Url ret;

    if (!baseUrls.empty())
        ret = Url(baseUrls.front());

    if( !ret.hasScheme() && !playlistUrl.empty() )
        ret.prepend( Url(playlistUrl) );

    return ret;
}

vlc_object_t * AbstractPlaylist::getVLCObject() const
{
    return p_object;
}

BasePeriod* AbstractPlaylist::getFirstPeriod()
{
    std::vector<BasePeriod *> periods = getPeriods();

    if( !periods.empty() )
        return periods.front();
    else
        return NULL;
}

BasePeriod* AbstractPlaylist::getNextPeriod(BasePeriod *period)
{
    std::vector<BasePeriod *> periods = getPeriods();

    for(size_t i = 0; i < periods.size(); i++)
    {
        if(periods.at(i) == period && (i + 1) < periods.size())
            return periods.at(i + 1);
    }

    return NULL;
}

void AbstractPlaylist::mergeWith(AbstractPlaylist *updatedAbstractPlaylist, mtime_t prunebarrier)
{
    availabilityEndTime.Set(updatedAbstractPlaylist->availabilityEndTime.Get());

    for(size_t i = 0; i < periods.size() && i < updatedAbstractPlaylist->periods.size(); i++)
        periods.at(i)->mergeWith(updatedAbstractPlaylist->periods.at(i), prunebarrier);
}

void AbstractPlaylist::pruneByPlaybackTime(mtime_t time)
{
    for(size_t i = 0; i < periods.size(); i++)
        periods.at(i)->pruneByPlaybackTime(time);
}

