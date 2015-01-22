/* Improved ffmpeg plugin for OpenSceneGraph - 
 * Copyright (C) 2014-2015 Digitalis Education Solutions, Inc. (http://DigitalisEducation.com)
 * File author: Oktay Radzhabov (oradzhabov at jazzros dot com)
 *
 * This library is open source and may be redistributed and/or modified under
 * the terms of the OpenSceneGraph Public License (OSGPL) version 0.0 or
 * (at your option) any later version.  The full license is in LICENSE file
 * included with this distribution, and on the openscenegraph.org website.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * OpenSceneGraph Public License for more details.
*/


#include "FFmpegTimer.hpp"

namespace osgFFmpeg
{

FFmpegTimer::FFmpegTimer()
{
    Reset();
}

void
FFmpegTimer::Start()
{
    m_stop_time_ms = 0;

    // Make stop-time less than start-time

    m_timer.setStartTick(1);
    m_start_time_ms = m_timer.time_m();
}

void
FFmpegTimer::Stop()
{
    m_offset = ElapsedMilliseconds();

    if (m_stop_time_ms < m_start_time_ms)
        m_stop_time_ms = m_start_time_ms;
}

void
FFmpegTimer::Reset()
{
    m_start_time_ms = 0;
    m_stop_time_ms = 0;
    m_offset = 0;
    m_timer.setStartTick(0);
}

const unsigned long
FFmpegTimer::ElapsedMilliseconds() const
{
    unsigned long spent_time_ms = 0;
    if (m_stop_time_ms >= m_start_time_ms)
    {
        spent_time_ms = m_stop_time_ms - m_start_time_ms;
    }
    else
    {
        spent_time_ms = m_timer.time_m() - m_start_time_ms;
    }
    return m_offset + spent_time_ms;
}

void
FFmpegTimer::ElapsedMilliseconds(const unsigned long & ms)
{
    m_offset = ms;
}



} // namespace osgFFmpeg
