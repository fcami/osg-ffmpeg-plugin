#include "FFmpegRenderThread.hpp"
#include "FFmpegILibAvStreamImpl.hpp"
#include <osg/Timer>

namespace osgFFmpeg {


FFmpegRenderThread::~FFmpegRenderThread()
{
    quit(true);
}

const int
FFmpegRenderThread::Initialize(FFmpegILibAvStreamImpl * pSrc, osg::ImageStream * pDst, const Size & frameSize)
{
    m_pLibAvStream = pSrc;
    m_pImgStream = pDst;
    m_frameSize = frameSize;

    if (pSrc == NULL || pDst == NULL)
        return -1;

    return 0;
}

void
FFmpegRenderThread::run()
{
    try
    {
        m_renderingThreadStop = false;
        unsigned char *         pFramePtr;
        unsigned long           timePosMS;

        osg::Timer              loopTimer;
        loopTimer.setStartTick(0);
        const float             frameTimeMS = 1000.0f / m_pLibAvStream->fps();
        double                  tick_start_ms;
        double                  tick_end_ms;
        double                  frame_ms;
        double                  dist_frame_ms;
        int                     iErr;
        //
        while (m_renderingThreadStop == false)
        {
            tick_start_ms = loopTimer.time_m();
            //
            //
            if (m_pLibAvStream->isHasAudio())
                timePosMS = m_pLibAvStream->GetAudioPlaybackTime();
            else
                timePosMS = m_pLibAvStream->ElapsedMilliseconds();

            {
                iErr = m_pLibAvStream->GetFramePtr (timePosMS, pFramePtr);
                // 
                // Frame could be not best time position(iErr > 0),
                // but to avoid stucking, we should draw it
                if (pFramePtr != NULL && iErr >= 0)
                {
                    m_pImgStream->setImage(
                        m_frameSize.Width,
                        m_frameSize.Height,
                        1, GL_RGB, GL_RGB, GL_UNSIGNED_BYTE,
                        pFramePtr, osg::Image::NO_DELETE
                    );
                }
                else
                {
                    // fprintf (stdout, "pFramePtr = NULL in %\n");
                }

                m_pLibAvStream->ReleaseFoundFrame();
            }
            //
            // Calc extra time to sleep thread
            //
            tick_end_ms = loopTimer.time_m();
            frame_ms = tick_end_ms - tick_start_ms;
            dist_frame_ms = frameTimeMS - frame_ms;
            if (dist_frame_ms > 0.0 && dist_frame_ms < frameTimeMS)
            {
                OpenThreads::Thread::microSleep(1000 * dist_frame_ms);
            }
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