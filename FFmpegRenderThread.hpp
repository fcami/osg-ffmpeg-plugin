#ifndef HEADER_GUARD_FFMPEG_RENDERTHREAD_H
#define HEADER_GUARD_FFMPEG_RENDERTHREAD_H

#include <osg/ImageStream>
#include <OpenThreads/Thread>
#include "FFmpegFileHolder.hpp"

namespace osgFFmpeg {

class FFmpegILibAvStreamImpl;

class FFmpegRenderThread : public OpenThreads::Thread
{
    osg::ImageStream            * m_pImgStream;
    FFmpegILibAvStreamImpl      * m_pLibAvStream;
    Size                        m_frameSize;
    volatile bool               m_renderingThreadStop;

    virtual void                run();
    virtual void                quit(bool waitForThreadToExit = true);
public:

    virtual                     ~FFmpegRenderThread();

    const int                   Initialize(FFmpegILibAvStreamImpl *, osg::ImageStream *, const Size &);

    void                        Stop();
};

} // namespace osgFFmpeg

#endif // HEADER_GUARD_FFMPEG_RENDERTHREAD_H