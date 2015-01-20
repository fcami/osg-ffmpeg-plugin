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
