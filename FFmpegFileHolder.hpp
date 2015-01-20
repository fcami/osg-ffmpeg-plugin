#ifndef HEADER_GUARD_FFMPEG_FILEHOLDER_H
#define HEADER_GUARD_FFMPEG_FILEHOLDER_H

#include "FFmpegHeaders.hpp"
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
    //
    const unsigned long     duration_ms() const;
    //
    const long              audioIndex() const;
    const bool              isHasAudio() const;
    const AudioFormat &     getAudioFormat() const;
};

} // namespace osgFFmpeg

#endif // HEADER_GUARD_FFMPEG_FILEHOLDER_H