/* Improved ffmpeg plugin for OpenSceneGraph -
 * Copyright (C) 1998-2010 Robert Osfield
 * Modifications copyright (C) 2014-2015 Digitalis Education Solutions, Inc. (http://DigitalisEducation.com)
 * Modification author: Oktay Radzhabov (oradzhabov at jazzros dot com)
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


#include "FFmpegAudioStream.hpp"

#include "FFmpegFileHolder.hpp"
#include "FFmpegStreamer.hpp"

#include <stdexcept>

namespace osgFFmpeg {



FFmpegAudioStream::FFmpegAudioStream(FFmpegFileHolder * pFileHolder, FFmpegStreamer * pStreamer)
:m_pFileHolder(pFileHolder),
m_streamer(pStreamer)
{
}


FFmpegAudioStream::FFmpegAudioStream(const FFmpegAudioStream & audio, const osg::CopyOp & copyop) :
    osg::AudioStream(audio, copyop)
{
}

FFmpegAudioStream::~FFmpegAudioStream()
{
    // detact the audio sink first to avoid destrction order issues.
    // trystan: caused #3333, crash on close audio.
 //   setAudioSink (NULL);
}

void FFmpegAudioStream::setAudioSink(osg::AudioSink* audio_sink)
{
    OSG_NOTICE<<"FFmpegAudioStream::setAudioSink( "<<audio_sink<<")"<<std::endl;
    m_streamer->setAudioSink(audio_sink);
}


void FFmpegAudioStream::consumeAudioBuffer(void * const buffer, const size_t size)
{
    //
    // Inform consume audio buffer size
    //
    static double        playbackSec = -1.0;
    if (playbackSec < 0.0 && size > 0 && m_pFileHolder)
    {
        const AudioFormat   audioFormat = m_pFileHolder->getAudioFormat();
        playbackSec = (double)size / (double)(audioFormat.m_sampleRate * audioFormat.m_bytePerSample * audioFormat.m_channelsNb);
        av_log(NULL, AV_LOG_INFO, "Consumed audio chunk: %f sec", playbackSec);
    }

    m_streamer->audio_fillBuffer(buffer, size);
}

double FFmpegAudioStream::duration() const
{
    return m_pFileHolder->duration_ms();
}



int FFmpegAudioStream::audioFrequency() const
{
    return m_pFileHolder->getAudioFormat().m_sampleRate;
}



int FFmpegAudioStream::audioNbChannels() const
{
    return m_pFileHolder->getAudioFormat().m_channelsNb;
}



osg::AudioStream::SampleFormat
FFmpegAudioStream::audioSampleFormat() const
{
    osg::AudioStream::SampleFormat  result;
    //
    // see: https://www.ffmpeg.org/doxygen/2.5/group__lavu__sampfmts.html#details
    //
    // "The data described by the sample format is always in native-endian order.
    //  Sample values can be expressed by native C types, hence the lack of a signed 24-bit sample format
    //  even though it is a common raw audio data format."
    //
    // So, even if some file contains audio with 24-bit samples,
    // ffmpeg converts it to available AV_SAMPLE_FMT_S32 automatically
    //
    switch (m_pFileHolder->getAudioFormat().m_avSampleFormat)
    {
    case AV_SAMPLE_FMT_U8:
        result = osg::AudioStream::SAMPLE_FORMAT_U8;
        break;
    case AV_SAMPLE_FMT_S16:
        result = osg::AudioStream::SAMPLE_FORMAT_S16;
        break;
    case AV_SAMPLE_FMT_S32:
        result = osg::AudioStream::SAMPLE_FORMAT_S32;
        break;
    case AV_SAMPLE_FMT_FLT:
        result = osg::AudioStream::SAMPLE_FORMAT_F32;
        break;
    default:
        throw std::runtime_error("unknown audio format");
    };

    return result;
}


} // namespace osgFFmpeg
