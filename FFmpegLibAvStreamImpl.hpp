#ifndef HEADER_GUARD_FFMPEG_LIBAVSTREAMIMPL_H
#define HEADER_GUARD_FFMPEG_LIBAVSTREAMIMPL_H

#include <OpenThreads/Thread>
#include <OpenThreads/Condition>

#include "FFmpegILibAvStreamImpl.hpp"
#include "AudioBuffer.hpp"
#include "VideoVectorBuffer.hpp"
#include "FFmpegTimer.hpp"
#include "FFmpegRenderThread.hpp"


namespace osgFFmpeg {

class FFmpegLibAvStreamImpl : public FFmpegILibAvStreamImpl, public OpenThreads::Thread
{
private:
    typedef OpenThreads::Mutex              Mutex;
    typedef OpenThreads::ScopedLock<Mutex>  ScopedLock;
    typedef OpenThreads::Condition          Condition;
    Condition                               m_threadLocker;

    bool                            m_loop;

    osg::ref_ptr<osg::AudioSink>    m_audio_sink; // owned by this object
    long                            m_audioIndex;
    AudioFormat                     m_audioFormat;
    const unsigned char             m_AudioBufferTimeSec;
    AudioBuffer                     m_audio_buffer;
    unsigned long                   m_ellapsedAudioMicroSec;
    unsigned long                   m_audioDelay_ms;
    volatile bool                   m_audio_buffering_finished;
    //
    long                            m_videoIndex;
    VideoVectorBuffer               m_video_buffer;
    FFmpegRenderThread              m_renderer;
    float                           m_frame_rate;
    //
    FFmpegTimer                     m_playerTimer;
    bool                            m_isNeedFlushBuffers;
    FFmpegPlayer *                  m_pPlayer;
    volatile bool                   m_shadowThreadStop;
    void                            preRun();
    void                            startPlayback();
    virtual void                    run ();
    void                            postRun();
    void                            stopShadowThread();

public:
                                    FFmpegLibAvStreamImpl();
    virtual                         ~FFmpegLibAvStreamImpl();

    virtual void                    setAudioSink(osg::AudioSink * audio_sink);
    virtual void                    setAudioDelay (const double & audioDelay);
    virtual const int               initialize(const FFmpegFileHolder * pHolder, FFmpegPlayer * pPlayer);
    virtual void                    loop(const bool loop);
    virtual const bool              loop() const;

    virtual void                    Start();
    virtual void                    Pause();
    virtual void                    Stop();
    virtual void                    Seek(const unsigned long & newTimeMS);
    virtual const unsigned long     ElapsedMilliseconds() const;

    virtual const bool              isHasAudio() const;
    virtual void                    setAudioVolume(const float &);
    virtual const float             getAudioVolume() const;
    virtual void                    GetAudio(void * buffer, int bytesLength);
    virtual const unsigned long     GetAudioPlaybackTime() const;
    //
    /*
     * DO NOT FORGET CALL ReleaseFoundFrame() AFTER GetFramePtr() CALLED AND PTR HAS BEEN USED.
     * DO NOT CALL GetFramePtr() TWICE. ALWAYS CALL ReleaseFoundFrame() AFTER EACH CALLING GetFramePtr().
     */
    virtual int                     GetFramePtr(const unsigned long & timePosMS, unsigned char *& pArray);
    virtual void                    ReleaseFoundFrame();
    virtual const bool              isHasVideo() const;
    virtual float                   fps() const;
};

} // namespace osgFFmpeg

#endif // HEADER_GUARD_FFMPEG_LIBAVSTREAMIMPL_H
