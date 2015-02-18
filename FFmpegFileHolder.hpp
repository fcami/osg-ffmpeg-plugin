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


#ifndef HEADER_GUARD_FFMPEG_FILEHOLDER_H
#define HEADER_GUARD_FFMPEG_FILEHOLDER_H

#include "FFmpegHeaders.hpp"
#include <osg/ImageStream>
#include <string>

namespace osgFFmpeg {

class FFmpegParameters;

struct Size
{
    unsigned short Width;
    unsigned short Height;

    Size (const unsigned short & w = 0, const unsigned short & h = 0):Width(w),Height(h){}
};

struct AudioFormat
{
    unsigned char           m_bytePerSample;
    unsigned char           m_channelsNb;
    unsigned int            m_sampleRate;
    AVSampleFormat          m_avSampleFormat;

    void                    clear()
    {
        m_bytePerSample = 0;
        m_channelsNb = 0;
        m_sampleRate = 0;
        m_avSampleFormat = AV_SAMPLE_FMT_NONE;
    }
};

class FFmpegFileHolder
{
    long                    m_audioIndex;
    long                    m_videoIndex;
    unsigned long           m_duration; // ms
    //
    AudioFormat             m_audioFormat;
    //
    Size                    m_frameSize;
    AVPixelFormat           m_pixFmt;
    float                   m_pixAspectRatio;
    float                   m_frame_rate;
    bool                    m_alpha_channel;


                            FFmpegFileHolder(const FFmpegFileHolder &) {} // Avoid copy-constructor

public:
                            FFmpegFileHolder();
    const short             open (const std::string & filename, FFmpegParameters* parameters);
    void                    close ();
    //
    const long              videoIndex() const;
    const unsigned short    width() const;
    const unsigned short    height() const;
    const float             pixelAspectRatio() const;
    const float             frameRate() const;
    const bool              alphaChannel() const;
    const AVPixelFormat     getPixFormat() const;
    static void             getGLPixFormats(const AVPixelFormat pixFmt, GLint & outInternalTexFmt, GLint & outPixFmt);
    //
    const unsigned long     duration_ms() const;
    //
    const long              audioIndex() const;
    const bool              isHasAudio() const;
    const AudioFormat &     getAudioFormat() const;
};

} // namespace osgFFmpeg

#endif // HEADER_GUARD_FFMPEG_FILEHOLDER_H
