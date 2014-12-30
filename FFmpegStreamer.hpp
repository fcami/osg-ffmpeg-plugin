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
