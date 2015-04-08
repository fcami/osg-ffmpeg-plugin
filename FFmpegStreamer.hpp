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


#ifndef HEADER_GUARD_FFMPEG_STREAMER_H
#define HEADER_GUARD_FFMPEG_STREAMER_H

#include <cstdlib>

namespace osg {

    class AudioSink;
} // namespace osg


namespace osgFFmpeg {

class FFmpegPlayer;
class FFmpegFileHolder;
class FFmpegILibAvStreamImpl;
class FFmpegStreamer
{
    const FFmpegFileHolder *                m_holder;
    FFmpegILibAvStreamImpl *                m_pLibAvStreamImpl;

public:
                            FFmpegStreamer();
                            ~FFmpegStreamer();

    const int               open(const FFmpegFileHolder * pHolder, FFmpegPlayer * pRenderDest);
    void                    close();
    
    const unsigned char*    getFrame() const;

    void                    setAudioSink(osg::AudioSink * audio_sink);
    void                    audio_fillBuffer(void * buffer, size_t size);
    //
    void                    loop(const bool loop);
    const bool              loop() const;
    //
    void                    setAudioVolume(const float &);
    const float             getAudioVolume() const;
    // balance of the audio: -1 = left, 0 = center,  1 = right
    const float             getAudioBalance() const;
    void                    setAudioBalance(const float & balance);
    //
    void                    play();
    void                    pause();
    void                    seek(const unsigned long & timeMS);
    const double            getCurrentTimeSec() const;
};

} // namespace osgFFmpeg

#endif // HEADER_GUARD_FFMPEG_STREAMER_H
