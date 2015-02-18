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


#include "FFmpegRenderThread.hpp"
#include "FFmpegILibAvStreamImpl.hpp"
#include "FFmpegFileHolder.hpp"
#include <osg/Timer>

namespace osgFFmpeg {


FFmpegRenderThread::~FFmpegRenderThread()
{
    quit(true);
}

const int
FFmpegRenderThread::Initialize(FFmpegILibAvStreamImpl * pSrc, osg::ImageStream * pDst, const FFmpegFileHolder * pFileHolder)
{
    m_pFileHolder   = pFileHolder;
    m_pLibAvStream  = pSrc;
    m_pImgStream    = pDst;

    if (pSrc == NULL || pDst == NULL)
        return -1;

    return 0;
}

void
FFmpegRenderThread::Start()
{
    //
    // Guaranty that thread will starts even if it was run before
    //
    if (isRunning() == true)
        Stop();

    if (isRunning() == false)
    {
        // To avoid thread concurent conflicts, follow param should be
        // defined from parent thread
        m_renderingThreadStop = false;

        // start thread
        start();
    }
}

void
FFmpegRenderThread::run()
{
    try
    {
        unsigned char *         pFramePtr;
        unsigned long           timePosMS;

        osg::Timer              loopTimer;
        loopTimer.setStartTick(0);
        const float             frameTimeMS = 1000.0f / m_pLibAvStream->fps();
        double                  tick_start_ms = -1.0;
        double                  tick_end_ms;
        double                  frame_ms;
        double                  dist_frame_ms;
        int                     iErr;
        //
        GLint                   internalTexFmt;
        GLint                   pixFmt;
        FFmpegFileHolder::getGLPixFormats (m_pFileHolder->getPixFormat(), internalTexFmt, pixFmt);
        //
        while (m_renderingThreadStop == false)
        {
            //
            //
            timePosMS = m_pLibAvStream->GetPlaybackTime();

            iErr = m_pLibAvStream->GetFramePtr (timePosMS, pFramePtr);
            //
            // Frame could be not best time position(iErr > 0),
            // but to avoid stucking, we should draw it
            if (pFramePtr != NULL && iErr >= 0)
            {
                //
                // Calculate extra time to sleep thread before next the frame is appear
                //
                if (tick_start_ms >= 0.0)
                {
                    tick_end_ms = loopTimer.time_m();
                    frame_ms = tick_end_ms - tick_start_ms;
                    dist_frame_ms = frameTimeMS - frame_ms;
                    if (dist_frame_ms > 0.0 && dist_frame_ms < frameTimeMS)
                    {
                        //
                        // Devide by 2 for avoid twitching of the image which smoothly changes the background
                        //
                        OpenThreads::Thread::microSleep(1000 * dist_frame_ms / 2);
                    }
                }
                m_pImgStream->setImage(
                    m_pFileHolder->width(),
                    m_pFileHolder->height(),
                    1, internalTexFmt, pixFmt, GL_UNSIGNED_BYTE,
                    pFramePtr, osg::Image::NO_DELETE
                );
                tick_start_ms = loopTimer.time_m();
            }

            m_pLibAvStream->ReleaseFoundFrame();
        }
    }
    catch (const std::exception & error)
    {
        OSG_WARN << "FFmpegRenderThread::run : " << error.what() << std::endl;
    }

    catch (...)
    {
        OSG_WARN << "FFmpegRenderThread::run : unhandled exception" << std::endl;
    }
}

void
FFmpegRenderThread::quit(bool waitForThreadToExit)
{
    m_renderingThreadStop = true;
    if (isRunning())
    {
        if (waitForThreadToExit)
            join();
    }
}

void
FFmpegRenderThread::Stop()
{
    quit (true);
}

} // namespace osgFFmpeg
