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


#include "VideoVectorBuffer.hpp"
#include "FFmpegWrapper.hpp"
#include <limits>

size_t getMemorySize();

namespace osgFFmpeg
{

VideoVectorBuffer::VideoVectorBuffer()
:m_fileIndex(-1)
{
}

VideoVectorBuffer::~VideoVectorBuffer()
{
    release();
}


void
VideoVectorBuffer::flush()
{
    if (m_fileIndex < 0)
        return;

    setStreamFinished (false); // this func locked by \m_mutex

    {
        ScopedLock              lock (m_mutex);

        const unsigned int      bufferFrameCount = m_pool.FrameCount();

        m_bufferGrabPtrStart        = 0;
        m_bufferGrabPtrEnd          = bufferFrameCount;
        m_bufferGrabPtrEnd_found    = bufferFrameCount;
        //
        // Prepare necessary time/frame map
        //
        m_timeMappingList.resize(bufferFrameCount);

        for (unsigned int i = 0; i < bufferFrameCount; ++i)
        {
            m_timeMappingList[i].Set (i, 0.0);
        }
    }
}

const int
VideoVectorBuffer::alloc(const FFmpegFileHolder * pHolder)
{
    if (pHolder == NULL)
        return -1;
    if (pHolder->videoIndex() < 0)
        return -1;

    m_forcedFrameTimeMS              = -1.0;
    //
    // If buffer has been allocated before,
    // * Exit with success, if necessary buffer has been opened for this video before
    // * Release buffer, if buffer will be allocated for new video
    //
    if (m_fileIndex >= 0)
    {
        if (m_fileIndex == pHolder->videoIndex())
        {
            flush();
            return 0;
        }
        else
        {
            release();
        }
    }


    int         err = -1;
    //
    try
    {
        {
            ScopedLock  lock (m_mutex);

            m_frameSize = avpicture_get_size(pHolder->getPixFormat(), pHolder->width(), pHolder->height());

            const size_t    partOfPhysicMemorySizeInBytes = floor((double)getMemorySize() / 4.0); // could return 0
            const size_t    available_frame_nb = partOfPhysicMemorySizeInBytes > 0 ? (std::min<size_t>(20, floor((double)partOfPhysicMemorySizeInBytes / (double)m_frameSize))) : 20;

            m_pool.alloc(m_frameSize, available_frame_nb);

            av_log(NULL, AV_LOG_INFO, "Video allocs pool for %d frames\n", m_pool.FrameCount());
            //
            // Determine time-stamp between frames
            //
            m_videoLength = pHolder->duration_ms();
            m_fileIndex = pHolder->videoIndex();
            m_fps = pHolder->frameRate();
        }

        flush();
        err = 0;
    }
    catch (...)
    {
    }
    return err;
}

void
VideoVectorBuffer::release()
{
    ScopedLock  lock (m_mutex);

    m_pool.release();
    m_fileIndex = -1;
}

/*
* If Return value is 0(No error, and ptr are active), user SHOULD call ReleaseFoundFrame()
* to release memory. Before calling ReleaseFoundFrame(), user may use returned ptr with guaranty
* to frame do not changed by other/shadow threads.
*
* DO NOT FORGET CALL ReleaseFoundFrame() WHEN GetFramePtr() CALLED.
* DO NOT CALL GetFramePtr() TWICE. ALWAYS CALL ReleaseFoundFrame() AFTER EACH CALLING GetFramePtr().
*
*/
const int
VideoVectorBuffer::GetFramePtr(const unsigned long & msTime,
                                unsigned char *& pArray,
                                const bool useRibbonTimeStrategy)
{
    if (m_fileIndex < 0)
        return -1;

    if (msTime >= m_videoLength)
        return -1;

    ScopedLock  lock (m_mutex);
/*
    //
    // Display video buffer state
    //
    {
        size_t              i;
        const size_t        le              = m_bufferGrabPtrEnd % m_pool.FrameCount();
        const size_t        ls              = m_bufferGrabPtrStart % m_pool.FrameCount();
        const unsigned int  bufferedPart    = (ls > le) ? (ls - le) :
                                                        m_pool.FrameCount() - (le - ls);
        char                bufferFillState[100];
        const double        ratio           = (double)bufferedPart / (double)m_pool.FrameCount();
        const double        ratio_10        = ratio * 10;
        if (ratio < 0.8)
        {
            int debug_breakPoint = 0;
        }
        for (i = 0; i < ratio_10; ++i)
            bufferFillState[i] = '|';
        bufferFillState[i] = 0;
        av_log(NULL, AV_LOG_DEBUG, "VideoBuf: %s", bufferFillState);
    }
*/
    pArray = NULL;
    m_bufferGrabPtrEnd_found = m_bufferGrabPtrEnd;

    //
    //
    //
    const double        timeInSec = (double)msTime / 1000.0; // convert to seconds
    const unsigned int  fillFrameCount = (unsigned int)((int)m_bufferGrabPtrEnd - (int)m_bufferGrabPtrStart);
    if (fillFrameCount == m_pool.FrameCount())
    {
        pArray = m_pool.m_ptr[m_bufferGrabPtrStart];
        return 1;
    }
    bool                hasBufferedData = false;
    bool                singlePass = true;
    //
    const unsigned int  searchRezult_2 = m_pool.FrameCount();
    const unsigned int  searchStart_1 = 0;
    const unsigned int  searchEnd_1 = m_bufferGrabPtrStart;
    unsigned int        searchStart_2 = searchRezult_2;
    unsigned int        searchEnd_2 = searchRezult_2;
    unsigned int        searchRezult;
    // For two passes case
    if (m_bufferGrabPtrEnd != m_pool.FrameCount())
    {
        searchStart_2 = m_bufferGrabPtrEnd;
        searchEnd_2 = m_pool.FrameCount();
        singlePass = false;
    }
    hasBufferedData = false;
    if (singlePass == true)
    {
        for (searchRezult = searchStart_1; searchRezult < searchEnd_1; ++searchRezult)
        {
            if (m_timeMappingList[searchRezult].Time >= timeInSec)
            {
                break;
            }
        }
        if (searchRezult != searchEnd_1)
        {
            hasBufferedData = true;
        }
    }
    else
    {
        for (searchRezult = searchStart_2; searchRezult < searchEnd_2; ++searchRezult)
        {
            if (m_timeMappingList[searchRezult].Time >= timeInSec)
            {
                break;
            }
        }
        if (searchRezult != searchEnd_2)
        {
            hasBufferedData = true;
        }
        else
        {
            for (searchRezult = searchStart_1; searchRezult < searchEnd_1; searchRezult++)
            {
                if (m_timeMappingList[searchRezult].Time >= timeInSec)
                {
                    break;
                }
            }
            if (searchRezult != searchEnd_1)
            {
                hasBufferedData = true;
            }
        }
    }
    if (hasBufferedData == false)
    {
        double      maxT            = -std::numeric_limits<double>::max();
        size_t      ui_maxT         = 0;
        //
        for (size_t ui = 0; ui < m_timeMappingList.size(); ++ui)
        {
            const double frameTimeMS = m_timeMappingList[ui].Time * 1000.0;
            if (frameTimeMS > maxT)
            {
                maxT = frameTimeMS;
                ui_maxT = ui;
            }
        }
        if (useRibbonTimeStrategy)
        {
            //
            // Do not drop frames, and use latest available frame
            // So if even some place of file have long decoding time-period, render playback will slow-down
            // but later, when decoding speed-up, returns to necessary timing.
            //
            searchRezult = ui_maxT;
        }
        else
        {
            //
            // If streaming finished but someone wants frame, and buffer has not it, this is no error
            //
            if (isStreamFinished() == true)
            {
                searchRezult = ui_maxT;
            }
            else
            {
                float   frameDurationMS     = 1000.0f / m_fps;
                m_forcedFrameTimeMS         = msTime + frameDurationMS * 1;
                if (m_forcedFrameTimeMS + frameDurationMS > m_videoLength)
                    m_forcedFrameTimeMS = m_videoLength - frameDurationMS;

                // Use nearest (in time domain) frame
                pArray = m_pool.m_ptr [ m_timeMappingList[ui_maxT].Ptr ];
                return 1;
            }
        }

    }
    pArray = m_pool.m_ptr [ m_timeMappingList[searchRezult].Ptr ];

    //
    // Store pointer. It will be in use by ReleaseFoundFrame()
    //
    m_bufferGrabPtrEnd_found = searchRezult;
    m_forcedFrameTimeMS = -1.0;

    return 0;
}


void
VideoVectorBuffer::ReleaseFoundFrame()
{
    ScopedLock  lock (m_mutex);

    m_bufferGrabPtrEnd = m_bufferGrabPtrEnd_found;
}

const unsigned int
VideoVectorBuffer::freeSpaceSize() const
{
    ScopedLock  lock (m_mutex);

    const unsigned int bufferFreeSize = (m_bufferGrabPtrEnd >= m_bufferGrabPtrStart) ?
                        (m_bufferGrabPtrEnd - m_bufferGrabPtrStart) :
                        m_pool.FrameCount() - (m_bufferGrabPtrStart - m_bufferGrabPtrEnd);

    return bufferFreeSize;
}

const unsigned int
VideoVectorBuffer::size() const
{
    return m_pool.FrameCount();
}

const bool
VideoVectorBuffer::isBufferFull()
{
    //
    // Important:
    //
    // condition \(bufferFreeSize <= 2) should be "<= 2"
    // because it guaranty that view-frame(pointed by \m_bufferGrabPtrEnd) will not be rewrited by grabber
    //
    return (freeSpaceSize() <= 2);
}

void
VideoVectorBuffer::setStreamFinished(const bool value)
{
    ScopedLock  lock (m_mutex);

    m_video_buffering_finished = value;
}

const bool
VideoVectorBuffer::isStreamFinished()
{
    // Should not be locked, because it used in this class where wrapped by m_mutex
    // ScopedLock  lock (m_mutex);

    return m_video_buffering_finished;
}

void
VideoVectorBuffer::writeFrame(const unsigned int & flag, const size_t & drop_frame_nb)
{
    // Fix local value of cross-thread params
    unsigned int    loc_bufferGrabPtrStart = m_bufferGrabPtrStart;

    if (loc_bufferGrabPtrStart == m_pool.FrameCount())
        loc_bufferGrabPtrStart = 0;


    double timeStampSec;
    const short result = FFmpegWrapper::getNextImage (m_fileIndex,
                                                    m_pool.m_ptr[loc_bufferGrabPtrStart],
                                                    timeStampSec,
                                                    drop_frame_nb,
                                                    (flag & 1) ? false : true,
                                                    m_forcedFrameTimeMS);


    if (result == 0)
    {
        ScopedLock  lock (m_mutex);

        m_timeMappingList[loc_bufferGrabPtrStart].Time = timeStampSec;

        m_bufferGrabPtrStart = loc_bufferGrabPtrStart + 1;
    }
    else
    {
        setStreamFinished (true);
    }
}

} // namespace osgFFmpeg

