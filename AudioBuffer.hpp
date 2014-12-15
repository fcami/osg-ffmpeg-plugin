#ifndef HEADER_GUARD_FFMPEG_AUDIOBUFFER_H
#define HEADER_GUARD_FFMPEG_AUDIOBUFFER_H

#include <OpenThreads/Thread>
#include <OpenThreads/ScopedLock>


namespace osgFFmpeg {

class AudioBuffer
{
    typedef OpenThreads::Mutex              Mutex;
    typedef OpenThreads::ScopedLock<Mutex>  ScopedLock;
    //
    //
    //
    Mutex                                   m_mutex;
    volatile unsigned char *                m_Buffer;
    int                                     m_bufferSize;
    volatile int                            m_endIndicator;
    volatile int                            m_startIndicator;
    //
                        AudioBuffer(const AudioBuffer & other) {} // Hide copy contructor
    void                setStartIndicator(const int &);
    void                setEndIndicator(const int &);

public:
                        AudioBuffer();
                        ~AudioBuffer();

    const int           alloc(const unsigned int & bytesNb);
    void                flush();
    void                release();
    //
    // The only way to modify \m_startIndicator
    const unsigned long read (void * buffer, const int & bytesNb);
    //
    // The only way to modify \m_endIndicator
    const int           write (const unsigned char * buffer, const int & bytesNb);

    const bool          isEnoughWriteSpace(const int & bytesNb) const;
    const unsigned int  size() const;
};

}

#endif // HEADER_GUARD_FFMPEG_AUDIOBUFFER_H