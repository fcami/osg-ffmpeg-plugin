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


#include "FFmpegFileHolder.hpp"
#include "FFmpegWrapper.hpp"

namespace osgFFmpeg {

FFmpegFileHolder::FFmpegFileHolder()
:m_audioIndex(-1),m_videoIndex(-1),
m_duration(0),
m_pixAspectRatio(1.0f),
m_alpha_channel(false)
{
}

const long
FFmpegFileHolder::videoIndex() const
{
    return m_videoIndex;
}

const long
FFmpegFileHolder::audioIndex() const
{
    return m_audioIndex;
}

const unsigned short
FFmpegFileHolder::width() const
{
    return m_frameSize.Width;
}

const unsigned short
FFmpegFileHolder::height() const
{
    return m_frameSize.Height;
}

const float
FFmpegFileHolder::pixelAspectRatio() const
{
    return m_pixAspectRatio;
}

const float
FFmpegFileHolder::frameRate() const
{
    return m_frame_rate;
}

const bool
FFmpegFileHolder::alphaChannel() const
{
    return m_alpha_channel;
}

const bool
FFmpegFileHolder::isHasAudio() const
{
    return m_audioIndex >= 0;
}

const unsigned long
FFmpegFileHolder::duration_ms() const
{
    return m_duration;
}

const AudioFormat &
FFmpegFileHolder::getAudioFormat() const
{
    return m_audioFormat;
}

const AVPixelFormat
FFmpegFileHolder::getPixFormat() const
{
    return m_pixFmt;
}

void
FFmpegFileHolder::getGLPixFormats (const AVPixelFormat pixFmt, GLint & outInternalTexFmt, GLint & outPixFmt)
{
    outInternalTexFmt   = GL_RGB;
    outPixFmt           = GL_RGB;

    switch (pixFmt)
    {
        case AV_PIX_FMT_RGB24:
        {
            outInternalTexFmt = GL_RGB;
            outPixFmt = GL_RGB;
            break;
        }
        case AV_PIX_FMT_BGR24:
        {
            outInternalTexFmt = GL_RGB;
            outPixFmt = GL_BGR;
            break;
        }
        case AV_PIX_FMT_BGRA:
        {
            outInternalTexFmt = GL_RGBA;
            outPixFmt = GL_BGRA;
            break;
        }
        case AV_PIX_FMT_RGBA:
        {
            outInternalTexFmt = GL_RGBA;
            outPixFmt = GL_RGBA;
            break;
        }
        default:
        {
            av_log(NULL, AV_LOG_WARNING, "Cannot find GL pix format for libav pix format");
        }
    };
}

const short
FFmpegFileHolder::open (const std::string & filename, FFmpegParameters* parameters)
{
    if (m_audioIndex < 0 && m_videoIndex < 0)
    {
        //
        // Open For Audio
        //
        m_audioIndex = FFmpegWrapper::openAudio(filename.c_str(), parameters);
        if (m_audioIndex >= 0)
        {
            unsigned long audioInfo[4];
            FFmpegWrapper::getAudioInfo(m_audioIndex, audioInfo);
            //
            //
            //
            m_audioFormat.m_bytePerSample   = audioInfo[0];
            m_audioFormat.m_channelsNb      = audioInfo[1];
            m_audioFormat.m_sampleRate      = audioInfo[2];
            m_audioFormat.m_avSampleFormat  = (AVSampleFormat)audioInfo[3];
            //
            // Because SDL available for: AUDIO_U16SYS,AUDIO_S16SYS,AUDIO_S32SYS,AUDIO_F32SYS only (see "SDL_audio.h")
            // we can use limited set of output sample format only.
            // Moreover, base test-app (Example osgmovie) use "AUDIO_S16SYS" hardcoded, so
            // let it be follow:
            //
            m_audioFormat.m_bytePerSample   = 2;
            m_audioFormat.m_avSampleFormat  = AV_SAMPLE_FMT_S16;
        }
        //
        // Open For Video
        //
        m_frameSize.Width                   = 640;  // default
        m_frameSize.Height                  = 480;  // values
        m_alpha_channel                     = false;

        m_videoIndex = FFmpegWrapper::openVideo(filename.c_str(),
                                                parameters,
                                                m_pixFmt,
                                                m_pixAspectRatio,
                                                m_frame_rate,
                                                m_alpha_channel);
        //
        // Prepare General parameters
        //
        if (m_audioIndex >= 0 || m_videoIndex >= 0)
        {
            //
            // Determine Duration (ms)
            //
            unsigned long audioDuration = 0;
            unsigned long videoDuration = 0;
            unsigned long timeLimits[2];
            if (m_audioIndex >= 0)
            {
                FFmpegWrapper::getAudioTimeLimits(m_audioIndex, timeLimits);

                audioDuration = timeLimits[1] - timeLimits[0];
            }
            if (m_videoIndex >= 0)
            {
                FFmpegWrapper::getVideoTimeLimits(m_videoIndex, timeLimits);

                videoDuration = timeLimits[1] - timeLimits[0];
            }
            m_duration = std::max<unsigned long>(audioDuration, videoDuration);
            //
            // Determine Frame Range
            //
            if (m_videoIndex >= 0)
            {
                unsigned short  imgSize[2];
                FFmpegWrapper::getImageSize(m_videoIndex, imgSize);

                m_frameSize.Width = imgSize[0];
                m_frameSize.Height = imgSize[1];
            }

            return 0; // NoError
        }
    }
    else
    {
        // Try open opened file
    }

    return -1;
}

void
FFmpegFileHolder::close ()
{
    if (m_audioIndex >= 0)
    {
        FFmpegWrapper::closeAudio(m_audioIndex);
        m_audioIndex = -1;
    }
    if (m_videoIndex >= 0)
    {
        FFmpegWrapper::closeVideo(m_videoIndex);
        m_videoIndex = -1;
    }
}


} // namespace osgFFmpeg
