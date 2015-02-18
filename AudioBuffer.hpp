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

    const unsigned int  freeSpaceSize() const;
    const unsigned int  size() const;
};

}

#endif // HEADER_GUARD_FFMPEG_AUDIOBUFFER_H
