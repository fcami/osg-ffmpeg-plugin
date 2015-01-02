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
    setAudioSink (NULL);
}

void FFmpegAudioStream::setAudioSink(osg::AudioSink* audio_sink)
{
    OSG_NOTICE<<"FFmpegAudioStream::setAudioSink( "<<audio_sink<<")"<<std::endl;
    m_streamer->setAudioSink(audio_sink);
}


void FFmpegAudioStream::consumeAudioBuffer(void * const buffer, const size_t size)
{
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
