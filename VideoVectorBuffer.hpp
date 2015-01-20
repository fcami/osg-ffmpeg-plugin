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
    struct MemoryPool
    {
        std::vector<unsigned char *>    m_ptr;
        //
        //
        //
        ~MemoryPool()
        {
            release();
        }
        const size_t    FrameCount() const
        {
            return m_ptr.size();
        }
        void release()
        {
            size_t  i;
            for (i=0; i < m_ptr.size(); ++i)
            {
                unsigned char * ptr = m_ptr[i];
                av_free(ptr);
            }
            m_ptr.clear();
        }
        void alloc(const size_t & frameSize, const size_t & max_frame_nb)
        {
            try
            {
                size_t  i;
                int     err = 0;
                for (i=0; i < max_frame_nb && err == 0; ++i)
                {
                    unsigned char * ptr = (unsigned char *)av_malloc(frameSize);
                    if (ptr != NULL)
                    {
                        m_ptr.push_back(ptr);
                    }
                    else
                    {
                        // interrupt allocation
                        err = -1;
                    }
                }
            }
            catch (...)
            {
                // catch interrupted allocation
            }
        }
    };
    typedef OpenThreads::Mutex              Mutex;
    typedef OpenThreads::ScopedLock<Mutex>  ScopedLock;

private:
    mutable Mutex                   m_mutex;
    long                            m_fileIndex;
    unsigned long                   m_videoLength;
    unsigned int                    m_frameSize;
    unsigned int                    m_bufferGrabPtrStart;   // Pointer where frames will be grabbed. Modified in \writeFrame() or \flush()
    unsigned int                    m_bufferGrabPtrEnd;     // Pointer where last frame has been copied for output. Modified in \ReleaseFoundFrame() or \flush()
    unsigned int                    m_bufferGrabPtrEnd_found;
    bool                            m_video_buffering_finished;
    MemoryPool                      m_pool;
    float                           m_fps;
    volatile double                 m_forcedFrameTimeMS;
    std::vector<TimedFramePointer>  m_timeMappingList;

                            VideoVectorBuffer(const VideoVectorBuffer & other){}; // hide copy constructor
public:
                            VideoVectorBuffer();
                            ~VideoVectorBuffer();

    const int               alloc(const FFmpegFileHolder * pHolder);
    void                    flush();
    void                    release();
    void                    writeFrame(const unsigned int & flag, const size_t & drop_frame_nb);


    const unsigned int      freeSpaceSize() const;
    const unsigned int      size() const;

    const bool              isBufferFull();
    const bool              isStreamFinished();

    void                    setStreamFinished(const bool value);

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