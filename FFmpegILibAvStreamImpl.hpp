#ifndef HEADER_GUARD_FFMPEG_ILIBAVSTREAMIMPL_H
#define HEADER_GUARD_FFMPEG_ILIBAVSTREAMIMPL_H

namespace osg {

    class AudioSink;
}; // namespace osg

namespace osgFFmpeg {

class FFmpegPlayer;
class FFmpegFileHolder;

class FFmpegILibAvStreamImpl
{
public:
    virtual                         ~FFmpegILibAvStreamImpl() {};

    virtual void                    setAudioSink(osg::AudioSink * audio_sink) = 0;
    virtual void                    setAudioDelay (const double & audioDelay) = 0;
    virtual const int               initialize(const FFmpegFileHolder * pHolder, FFmpegPlayer * pPlayer) = 0;
    virtual void                    loop(const bool loop) = 0;
    virtual const bool              loop() const = 0;


    virtual void                    Start() = 0;
    virtual void                    Pause() = 0;
    virtual void                    Stop() = 0;
    virtual void                    Seek(const unsigned long & newTimeMS) = 0;
    virtual const unsigned long     ElapsedMilliseconds() const = 0;

    virtual const bool              isHasAudio() const = 0;
    virtual void                    setAudioVolume(const float &) = 0;
    virtual const float             getAudioVolume() const = 0;
    // Return audio playback time in ms
    virtual const unsigned long     GetAudio(void * buffer, int bytesLength) = 0;
    virtual const unsigned long     GetAudioPlaybackTime() const = 0;
    //
    /*
     * DO NOT FORGET CALL ReleaseFoundFrame() AFTER GetFramePtr() CALLED AND PTR HAS BEEN USED.
     * DO NOT CALL GetFramePtr() TWICE. ALWAYS CALL ReleaseFoundFrame() AFTER EACH CALLING GetFramePtr().
     */
    virtual int                     GetFramePtr(const unsigned long & timePosMS, unsigned char *& pArray) = 0;
    virtual void                    ReleaseFoundFrame() = 0;
    virtual const bool              isHasVideo() const = 0;
    virtual float                   fps() const = 0;
};

} // osgFFmpeg

#endif // HEADER_GUARD_FFMPEG_ILIBAVSTREAMIMPL_H