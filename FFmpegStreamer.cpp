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


#include "FFmpegStreamer.hpp"
#include "FFmpegLibAvStreamImpl.hpp"

namespace osgFFmpeg
{

FFmpegStreamer::FFmpegStreamer()
:m_holder(NULL),
m_pLibAvStreamImpl(NULL)
{
    m_pLibAvStreamImpl = new FFmpegLibAvStreamImpl();
}

FFmpegStreamer::~FFmpegStreamer()
{
    delete m_pLibAvStreamImpl;
}

void
FFmpegStreamer::loop(const bool value)
{
    m_pLibAvStreamImpl->loop (value);
}

const bool
FFmpegStreamer::loop() const
{
    return m_pLibAvStreamImpl->loop();
}

void
FFmpegStreamer::setAudioVolume(const float & volume)
{
    m_pLibAvStreamImpl->setAudioVolume(volume);
}

const float
FFmpegStreamer::getAudioVolume() const
{
    return m_pLibAvStreamImpl->getAudioVolume();
}

const float
FFmpegStreamer::getAudioBalance() const
{
    return m_pLibAvStreamImpl->getAudioBalance();
}

void
FFmpegStreamer::setAudioBalance(const float & balance)
{
    m_pLibAvStreamImpl->setAudioBalance(balance);
}

const int
FFmpegStreamer::open(const FFmpegFileHolder * holder, FFmpegPlayer * pRenderDest)
{
    if (holder != NULL && m_holder == NULL)
    {
        m_holder = holder;

        const int err = m_pLibAvStreamImpl->initialize (m_holder, pRenderDest);
        return err;
    }
    return -1;
}

void
FFmpegStreamer::close()
{
    if (m_holder != NULL)
    {
        m_holder = NULL;
    }
}


const unsigned char *
FFmpegStreamer::getFrame() const
{
    unsigned char *         pFrame;
    const unsigned long     timePosMS = 0;

    //
    // Could returns NULL when videoBuffer has not been allocated
    // But it is not critical
    //
    m_pLibAvStreamImpl->GetFramePtr (timePosMS, pFrame);
    m_pLibAvStreamImpl->ReleaseFoundFrame();

    return pFrame;
}


void
FFmpegStreamer::setAudioSink(osg::AudioSink * audio_sink)
{
    m_pLibAvStreamImpl->setAudioSink(audio_sink);
}


void
FFmpegStreamer::audio_fillBuffer(void * buffer, size_t size)
{
    m_pLibAvStreamImpl->GetAudio(buffer, size);
}

void
FFmpegStreamer::play()
{
    if (m_holder != NULL)
    {
        m_pLibAvStreamImpl->Start ();
    }
}

void
FFmpegStreamer::pause()
{
    if (m_holder != NULL)
    {
        m_pLibAvStreamImpl->Pause();
    }
}

void
FFmpegStreamer::seek(const unsigned long & timeMS)
{
    if (m_holder != NULL)
    {
        m_pLibAvStreamImpl->Seek (timeMS);
    }
}

const double
FFmpegStreamer::getCurrentTimeSec() const
{
    unsigned long   ulCurrTimeMS = m_pLibAvStreamImpl->GetPlaybackTime();

    const double    dCurrTimeSec = (double)ulCurrTimeMS/1000.0;

    return dCurrTimeSec;
}

} // osgFFmpeg
