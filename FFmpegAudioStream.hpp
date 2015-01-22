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


#ifndef HEADER_GUARD_OSGFFMPEG_FFMPEG_AUDIO_STREAM_H
#define HEADER_GUARD_OSGFFMPEG_FFMPEG_AUDIO_STREAM_H

#include <osg/AudioStream>

namespace osgFFmpeg
{
    class FFmpegFileHolder;
    class FFmpegStreamer;

    class FFmpegAudioStream : public osg::AudioStream
    {
    public:

                                        FFmpegAudioStream (FFmpegFileHolder * pFileHolder = NULL,
                                                        FFmpegStreamer * pStreamer = NULL);

                                        FFmpegAudioStream(const FFmpegAudioStream & audio,
                                                        const osg::CopyOp & copyop = osg::CopyOp::SHALLOW_COPY);

        META_Object(osgFFmpeg, FFmpegAudioStream);

        virtual void                    setAudioSink(osg::AudioSink* audio_sink);
        
        void                            consumeAudioBuffer(void * const buffer,
                                                        const size_t size);
        
        int                             audioFrequency() const;
        int                             audioNbChannels() const;
        osg::AudioStream::SampleFormat  audioSampleFormat() const;

        double duration() const;

    private:

        virtual                         ~FFmpegAudioStream();

        FFmpegFileHolder *              m_pFileHolder;
        FFmpegStreamer *                m_streamer;

    };

}



#endif // HEADER_GUARD_OSGFFMPEG_FFMPEG_IMAGE_STREAM_H
