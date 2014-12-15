#ifndef HEADER_GUARD_FFMPEG_VIDEOVECTORBUFFER_H
#define HEADER_GUARD_FFMPEG_VIDEOVECTORBUFFER_H

#include <OpenThreads/Thread>
#include <OpenThreads/ScopedLock>
#include "FFmpegFileHolder.hpp"
#include <vector>

namespace osgFFmpeg {


class VideoVectorBuffer
{
    struct TimedFramePointer
    {
        unsigned int    Ptr;
        double          Time;   // Time in seconds
        //
        void            Set(const unsigned int & lPtr, const double & lTime)
        {
            Ptr = lPtr;
            Time = lTime;
        }
    };

    typedef OpenThreads::Mutex              Mutex;
    typedef OpenThreads::ScopedLock<Mutex>  ScopedLock;

private:
    Mutex                           m_mutex;
    long                            m_fileIndex;
    unsigned long                   m_videoLength;
    const unsigned int              m_bufferFrameCount;
    unsigned int                    m_frameSize;
    unsigned int                    m_bufferSize;
    unsigned int                    m_bufferGrabPtrStart;   // Pointer where frames will be grabbed. Modified in \writeFrame() or \flush()
    unsigned int                    m_bufferGrabPtrEnd;     // Pointer where last frame has been copied for output. Modified in \ReleaseFoundFrame() or \flush()
    unsigned int                    m_bufferGrabPtrEnd_found;
    bool                            m_video_buffering_finished;
    unsigned char *                 m_buffer;
    std::vector<TimedFramePointer>  m_timeMappingList;

                            VideoVectorBuffer(const VideoVectorBuffer & other):m_bufferFrameCount(other.m_bufferFrameCount){}; // hide copy constructor
public:
                            VideoVectorBuffer();
                            ~VideoVectorBuffer();

    const int               alloc(const FFmpegFileHolder * pHolder);
    void                    flush();
    void                    release();
    void                    writeFrame();



    const bool              isBufferFull();
    const bool              isStreamFinished() const;


    /*
    * If Return value is 0(No error, and ptr are active), user SHOULD call ReleaseFoundFrame()
    * to release memory. Before calling ReleaseFoundFrame(), user may use returned ptr with guaranty
    * to frame do not changed by other/shadow threads.
    * 
    * DO NOT FORGET CALL ReleaseFoundFrame() WHEN GetFramePtr() CALLED.
    * DO NOT CALL GetFramePtr() TWICE. ALWAYS CALL ReleaseFoundFrame() AFTER EACH CALLING GetFramePtr().
    * 
    */
    const int               GetFramePtr(const unsigned long & msTime, unsigned char *& pArray);
    void                    ReleaseFoundFrame();
};

} // namespace osgFFmpeg

#endif // HEADER_GUARD_FFMPEG_VIDEOVECTORBUFFER_H