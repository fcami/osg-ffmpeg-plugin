#include "FFmpegFileHolder.hpp"
#include "FFmpegWrapper.hpp"

namespace osgFFmpeg {

FFmpegFileHolder::FFmpegFileHolder()
:m_audioIndex(-1),m_videoIndex(-1),
m_duration(0),
m_alpha_channel(false),
m_pixAspectRatio(1.0f)
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

const short
FFmpegFileHolder::open (const std::string & filename, FFmpegParameters* parameters)
{
    //todo: here we need copy implementationf of \parameters from the analogue
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
        }
        //
        // Open For Video
        //
        m_frameSize.Width = 640;    // default
        m_frameSize.Height = 480;   // values
        m_alpha_channel = false;
        //
        const bool useRGB_notBGR = true;    // all Video-data will be represented as RGB24
        const long scaledWidth = 0;         // actual video size will produced

        m_videoIndex = FFmpegWrapper::openVideo(filename.c_str(),
                                                parameters,
                                                useRGB_notBGR,
                                                scaledWidth,
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