#include "VideoVectorBuffer.hpp"
#include "FFmpegWrapper.hpp"


namespace osgFFmpeg
{

VideoVectorBuffer::VideoVectorBuffer()
:m_buffer(NULL),
m_bufferFrameCount(10),
m_bufferSize(0),
m_fileIndex(-1)
{
}

VideoVectorBuffer::~VideoVectorBuffer()
{
    release();
}


void
VideoVectorBuffer::flush()
{
    ScopedLock  lock (m_mutex);

    if (m_fileIndex < 0)
        return;

    m_bufferGrabPtrStart        = 0;
    m_bufferGrabPtrEnd          = m_bufferSize;
    m_bufferGrabPtrEnd_found    = m_bufferSize;
    m_video_buffering_finished  = false;
    //
    // Prepare necessary time/frame map
    //
    unsigned int    locPtr = m_bufferGrabPtrStart;
    m_timeMappingList.resize(m_bufferFrameCount);

    for (unsigned int i = 0; i < m_bufferFrameCount; ++i)
    {
        m_timeMappingList[i].Set (locPtr, 0.0);
        locPtr += m_frameSize;
    }
}

const int
VideoVectorBuffer::alloc(const FFmpegFileHolder * pHolder)
{
    if (pHolder == NULL)
        return -1;
    if (pHolder->videoIndex() < 0)
        return -1;
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

            m_frameSize = 3 * pHolder->width() * pHolder->height();
            m_bufferSize = m_frameSize * m_bufferFrameCount;
            m_buffer = new unsigned char[m_bufferSize];
            //
            // Determine time-stamp between frames
            //
            m_videoLength = pHolder->duration_ms();
            m_fileIndex = pHolder->videoIndex();
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

    if (m_buffer != NULL)
    {
        delete []m_buffer;
        m_buffer = NULL;
    }
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
VideoVectorBuffer::GetFramePtr(const unsigned long & msTime, unsigned char *& pArray)
{
    ScopedLock  lock (m_mutex);

    pArray = NULL;
    m_bufferGrabPtrEnd_found = m_bufferGrabPtrEnd;

    if (m_fileIndex < 0)
        return -1;
    //
    //
    //
    const double        timeInSec = (double)msTime / 1000.0; // convert to seconds
    const unsigned int  fillFrameCount = (unsigned int)(((int)m_bufferGrabPtrEnd - (int)m_bufferGrabPtrStart) / (int)m_frameSize);
    if (fillFrameCount == m_bufferFrameCount)
    {
        pArray = m_buffer + m_bufferGrabPtrStart;
        return 1;
    }
    bool                hasBufferedData = false;
    bool                singlePass = true;
    //
    const unsigned int  searchRezult_2 = m_bufferFrameCount;
    const unsigned int  searchStart_1 = 0;
    const unsigned int  searchEnd_1 = m_bufferGrabPtrStart / m_frameSize;
    unsigned int        searchStart_2 = searchRezult_2;
    unsigned int        searchEnd_2 = searchRezult_2;
    unsigned int        searchRezult;
    // For two passes case
    if (m_bufferGrabPtrEnd != m_bufferSize)
    {
        searchStart_2 = m_bufferGrabPtrEnd / m_frameSize;
        searchEnd_2 = m_bufferFrameCount;
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
        // the buffer began be late is here
        //
        //
#ifdef _DEBUG
        fprintf(stdout, "Buffered time not found for %d ms\n", msTime);
#endif // _DEBUG

        return -1;
    }
    pArray = m_buffer + m_timeMappingList[searchRezult].Ptr;

    //
    // Store pointer. It will be in use by ReleaseFoundFrame()
    //
    m_bufferGrabPtrEnd_found = searchRezult * m_frameSize;

    return 0;
}


void
VideoVectorBuffer::ReleaseFoundFrame()
{
    ScopedLock  lock (m_mutex);

    m_bufferGrabPtrEnd = m_bufferGrabPtrEnd_found;
}


const bool
VideoVectorBuffer::isBufferFull()
{
    ScopedLock  lock (m_mutex);

    const unsigned int bufferFreeSize = (m_bufferGrabPtrEnd >= m_bufferGrabPtrStart) ?
                        (m_bufferGrabPtrEnd - m_bufferGrabPtrStart) :
                        m_bufferSize - (m_bufferGrabPtrStart - m_bufferGrabPtrEnd);
    //
    // Important:
    //
    // condition \(bufferFreeSize <= m_frameSize*2) should be "<= m_frameSize*2" instead of "< m_frameSize"
    // because it guaranty that view-frame(pointed by \m_bufferGrabPtrEnd) will not be rewrited by grabber
    //
    return ((bufferFreeSize <= m_frameSize*2) || (m_video_buffering_finished == true));
}

const bool
VideoVectorBuffer::isStreamFinished() const
{
    return m_video_buffering_finished;
}

void
VideoVectorBuffer::writeFrame()
{
    // Fix local value of cross-thread params
    unsigned int    loc_bufferGrabPtrStart = m_bufferGrabPtrStart;

    if (loc_bufferGrabPtrStart == m_bufferSize)
        loc_bufferGrabPtrStart = 0;
    

    double timeStampSec;
    const short result = FFmpegWrapper::getNextImage (m_fileIndex,
                                                    & m_buffer[loc_bufferGrabPtrStart],
                                                    timeStampSec);

    
    if (result == 0)
    {
        ScopedLock  lock (m_mutex);

        const unsigned int index = loc_bufferGrabPtrStart / m_frameSize;
        m_timeMappingList[index].Time = timeStampSec;

        m_bufferGrabPtrStart = loc_bufferGrabPtrStart + m_frameSize;
    }
    else
    {
        m_video_buffering_finished = true;
    }
}

} // namespace osgFFmpeg

