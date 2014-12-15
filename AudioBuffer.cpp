#include "AudioBuffer.hpp"
#include <memory>

#include <string.h>

namespace osgFFmpeg {

AudioBuffer::AudioBuffer()
:m_Buffer(NULL),
m_endIndicator(0),
m_startIndicator(0)
{
}

AudioBuffer::~AudioBuffer()
{
    release ();
}

const int
AudioBuffer::alloc(const unsigned int & bytesNb)
{
    int err = 0;

    if (m_Buffer != NULL)
        release ();

    {
        ScopedLock  lock (m_mutex);

        try
        {
            m_Buffer = new unsigned char [bytesNb];
            m_endIndicator = 0;
            m_startIndicator = 0;
            m_bufferSize = bytesNb;
        }
        catch (...)
        {
            err = -1;
            m_Buffer = NULL;
        }
    }
    return err;
}

void
AudioBuffer::flush()
{
    ScopedLock  lock (m_mutex);

    m_endIndicator = 0;
    m_startIndicator = 0;
}

void
AudioBuffer::release()
{
    ScopedLock  lock (m_mutex);

    m_bufferSize = 0;
    if (m_Buffer != NULL)
    {
        delete []m_Buffer;
        m_Buffer = NULL;
    }

    m_endIndicator = 0;
    m_startIndicator = 0;
}

void
AudioBuffer::setStartIndicator(const int & value)
{
    ScopedLock  lock (m_mutex);

    m_startIndicator = value;
}

void
AudioBuffer::setEndIndicator(const int & value)
{
    ScopedLock  lock (m_mutex);

    m_endIndicator = value;
}

const unsigned long
AudioBuffer::read (void * buffer, const int & bytesNb)
{
    if (bytesNb == 0)
        return 0;

    // Fix local value of cross-thread params
    const int loc_endIndicator  = m_endIndicator;
    //
    const bool isSwitchEnd      = (m_startIndicator + bytesNb) % m_bufferSize != m_startIndicator + bytesNb;
    const bool signBefore       = m_startIndicator - loc_endIndicator > 0;
    const bool signAfter        = (m_startIndicator + bytesNb) % m_bufferSize - loc_endIndicator > 0;
    const bool isSignConst      = signBefore == signAfter;
    //
    //
    if (isSwitchEnd == isSignConst)
    {
        memset ((unsigned char *)buffer, 0, bytesNb);
        return 0;
    }

    if (isSwitchEnd)
    {
        const size_t rhs        = m_startIndicator + bytesNb - m_bufferSize;
        const size_t lhs        = m_bufferSize - m_startIndicator;

        memcpy (buffer, (const void *)(m_Buffer + m_startIndicator), lhs);
        
        memcpy ( ((unsigned char *)buffer + lhs), (const void *)m_Buffer,  rhs);
    }
    else
    {
        memcpy (buffer, (unsigned char *)(m_Buffer) + m_startIndicator, bytesNb);
    }
    //
    // After all, we can change \m_startIndicator
    //
    setStartIndicator ( (m_startIndicator + bytesNb) % m_bufferSize );

    return bytesNb;
}

const int
AudioBuffer::write (const unsigned char * data, const int & bytesNb)
{
    const int rhs       = m_endIndicator + bytesNb - m_bufferSize;

    if (rhs >= 0)
    {
        const int lhs   = m_bufferSize - m_endIndicator;

        memcpy ((unsigned char*)(m_Buffer + m_endIndicator), data, lhs);

        if (rhs > 0)
        {
            memcpy ((unsigned char*)m_Buffer, data + lhs, rhs);
        }
    }
    else
    {
        memcpy ((unsigned char*)(m_Buffer + m_endIndicator), data, bytesNb);
    }

    //
    // After all, we can change \m_endIndicator
    //
    setEndIndicator ((m_endIndicator + bytesNb) % m_bufferSize);

    return 0;
}

const bool
AudioBuffer::isEnoughWriteSpace(const int & blockSize) const
{
    // Fix local value of cross-thread params
    const int loc_endIndicator      = m_endIndicator;
    const int loc_startIndicator    = m_startIndicator;
    //
    const bool isSwitchEnd          = (loc_endIndicator + blockSize) % m_bufferSize != loc_endIndicator + blockSize;
    const bool signBefore           = loc_endIndicator - loc_startIndicator > 0;
    const bool signAfter            = (loc_endIndicator + blockSize) % m_bufferSize - loc_startIndicator > 0;
    const bool isSignConst          = signBefore == signAfter;
    //
    if (isSwitchEnd != isSignConst || loc_startIndicator == loc_endIndicator)
        return true;

    return false;
}

const unsigned int
AudioBuffer::size() const
{
    return m_bufferSize;
}

} // namespace osgFFmpeg
