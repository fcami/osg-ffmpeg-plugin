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

class FFmpegLibAvStreamImpl : public FFmpegILibAvStreamImpl, protected OpenThreads::Thread
{
private:
    typedef OpenThreads::Mutex              Mutex;
    typedef OpenThreads::ScopedLock<Mutex>  ScopedLock;
    typedef OpenThreads::Condition          Condition;
    Condition                               m_threadLocker;
    mutable Mutex                           m_mutex;

    bool                            m_loop;

    osg::ref_ptr<osg::AudioSink>    m_audio_sink; // owned by this object
    long                            m_audioIndex;
    AudioFormat                     m_audioFormat;
    bool                            m_isAudioSinkImplementsVolumeControl;
    float                           m_audioVolumeAlternative; // if audioSink does not implements setVolume() & getVolume() (see \m_isAudioSinkImplementsVolumeControl) use this variable
    float                           m_audioBalance; // balance of the audio: -1 = left, 0 = center,  1 = right
    const unsigned char             m_AudioBufferTimeSec;
    AudioBuffer                     m_audio_buffer;
    unsigned long                   m_ellapsedAudioMicroSec;
    osg::Timer                      m_ellapsedAudioMicroSecOffsetTimer;
    unsigned long                   m_ellapsedAudioMicroSecOffsetInitial;
    unsigned long                   m_audioDelayMicroSec;
    volatile bool                   m_audio_buffering_finished;
    //
    long                            m_videoIndex;
    VideoVectorBuffer               m_video_buffer;
    FFmpegRenderThread              m_renderer;
    float                           m_frame_rate;
    bool                            m_useRibbonTimeStrategy;
    //
    FFmpegTimer                     m_playerTimer;
    bool                            m_isNeedFlushBuffers;
    FFmpegPlayer *                  m_pPlayer;
    volatile bool                   m_shadowThreadStop;
    const bool                      isPlaybackFinished();
    const bool                      detectIsItImplementedAudioVolume();
    void                            preRun();
    void                            startPlayback();
    virtual void                    run ();
    void                            postRun();
    void                            stopShadowThread();

public:
                                    FFmpegLibAvStreamImpl();
    virtual                         ~FFmpegLibAvStreamImpl();

    virtual void                    setAudioSink(osg::AudioSink * audio_sink);
    virtual void                    setAudioDelayMicroSec (const double & audioDelayMicroSec);
    virtual const int               initialize(const FFmpegFileHolder * pHolder, FFmpegPlayer * pPlayer);
    virtual void                    loop(const bool loop);
    virtual const bool              loop() const;

    virtual void                    Start();
    virtual void                    Pause();
    virtual void                    Stop();
    virtual void                    Seek(const unsigned long & newTimeMS);

    virtual const bool              isHasAudio() const;
    virtual void                    setAudioVolume(const float &);
    virtual const float             getAudioVolume() const;
    // balance of the audio: -1 = left, 0 = center,  1 = right
    virtual const float             getAudioBalance() const;
    virtual void                    setAudioBalance(const float & balance);
    virtual void                    GetAudio(void * buffer, int bytesLength);
    virtual const unsigned long     GetPlaybackTime() const;
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
