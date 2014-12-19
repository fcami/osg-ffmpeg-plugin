#include "FFmpegLibAvStreamImpl.hpp"
#include "FFmpegWrapper.hpp"
#include "FFmpegPlayer.hpp"
#include <osg/Notify>
#include <limits>

namespace osgFFmpeg
{

// todo: When audio reader grabbs all audio, it seems that timer stops, and video do not change frames.

FFmpegLibAvStreamImpl::FFmpegLibAvStreamImpl()
:m_AudioBufferTimeSec(6),
m_ellapsedAudioMicroSec(0),
m_loop(false),
m_pPlayer(NULL)
{
}
    
FFmpegLibAvStreamImpl::~FFmpegLibAvStreamImpl()
{
    stopShadowThread();

    if (m_audio_sink.valid())
        m_audio_sink->stop();
}

void
FFmpegLibAvStreamImpl::setAudioSink(osg::AudioSink * audio_sink)
{
    // The FFmpegLibAvStreamImpl object takes the responsability of destroying the audio_sink.
    OSG_NOTICE << "Assigning " << audio_sink << std::endl;

    m_audio_sink = audio_sink;
    //
    if (m_audio_sink.valid())
    {
        //
        // Because implementation of AudioSink in osgMovie example some strange,
        // we should initialize \m_audio_sink in follow:
        //
        m_audio_sink->play();
        m_audio_sink->pause();
    }
}

void
FFmpegLibAvStreamImpl::setAudioDelayMicroSec (const double & audioDelayMicroSec)
{
    m_audioDelayMicroSec = audioDelayMicroSec;
}

const int
FFmpegLibAvStreamImpl::initialize(const FFmpegFileHolder * pHolder, FFmpegPlayer * pPlayer)
{
    m_frame_rate = 1;
    m_audioFormat.clear();
    m_audioIndex = pHolder->audioIndex();
    m_videoIndex = pHolder->videoIndex();
    m_pPlayer = pPlayer;
    m_isNeedFlushBuffers = true;

    if (isHasAudio())
    {
        m_audioFormat = pHolder->getAudioFormat();
        
        if (m_audio_buffer.alloc ( m_audioFormat.m_sampleRate *
                                        m_audioFormat.m_channelsNb *
                                        m_audioFormat.m_bytePerSample * 
                                        m_AudioBufferTimeSec) < 0)
        {
            m_audioFormat.clear();
            m_audio_buffer.release();
            m_audioIndex = -1;

            OSG_NOTICE << "Cannot alloc audio buffer" << std::endl;
        }
    }
    //
    if (isHasVideo())
    {
        m_frame_rate = pHolder->frameRate();
        
        if (m_video_buffer.alloc(pHolder) < 0)
        {
            m_video_buffer.release();
            m_videoIndex = -1;
            OSG_NOTICE << "Cannot alloc video buffer" << std::endl;
        }
        else
        {
            if (m_renderer.Initialize (this,
                                    m_pPlayer,
                                    Size(pHolder->width(), pHolder->height())
                                    ) < 0)
            {
                m_video_buffer.release();
                m_videoIndex = -1;
                OSG_NOTICE << "Cannot initialize render thread" << std::endl;
            }
        }
    }

    return (isHasAudio() || isHasVideo()) ? 0 : -1;
}

void
FFmpegLibAvStreamImpl::loop(const bool loop)
{
    m_loop = loop;
}

const bool
FFmpegLibAvStreamImpl::loop() const
{
    return m_loop;
}

const unsigned long
FFmpegLibAvStreamImpl::GetAudioPlaybackTime() const
{
    if (m_audio_buffering_finished == true)
        return std::numeric_limits<unsigned long>::max();

    return (m_ellapsedAudioMicroSec < m_audioDelayMicroSec) ? 0 : (m_ellapsedAudioMicroSec - m_audioDelayMicroSec)/1000;
}

void
FFmpegLibAvStreamImpl::Start()
{
    if (isRunning() == false)
    {
        // start thread
        start();
    }
}

void
FFmpegLibAvStreamImpl::Pause()
{
    stopShadowThread();
}

void
FFmpegLibAvStreamImpl::Stop()
{
    stopShadowThread();
    //
    m_playerTimer.Reset();
    m_ellapsedAudioMicroSec = 0;
}

void
FFmpegLibAvStreamImpl::Seek(const unsigned long & newTimeMS)
{
    m_isNeedFlushBuffers = true;
    m_playerTimer.Reset();
    m_playerTimer.ElapsedMilliseconds (newTimeMS);
    m_ellapsedAudioMicroSec = newTimeMS * 1000;
}

const unsigned long
FFmpegLibAvStreamImpl::ElapsedMilliseconds() const
{
    return m_playerTimer.ElapsedMilliseconds();
}

void
FFmpegLibAvStreamImpl::setAudioVolume(const float & volume)
{
    if (m_audio_sink.valid())
    {
        m_audio_sink->setVolume(volume);
    }
}

const float
FFmpegLibAvStreamImpl::getAudioVolume() const
{
    if (m_audio_sink.valid())
    {
        return m_audio_sink->getVolume();
    }
    return 0.0f;
}

const bool
FFmpegLibAvStreamImpl::isHasAudio() const
{
    return m_audioIndex >= 0;
}

void
FFmpegLibAvStreamImpl::GetAudio(void * buffer, int bytesLength)
{
    const unsigned long     playbackBytes = m_audio_buffer.read (buffer, bytesLength);
    //
    const double            playbackSec = (double)playbackBytes / (double)(m_audioFormat.m_sampleRate * m_audioFormat.m_bytePerSample * m_audioFormat.m_channelsNb);
    //
    // Signal thread that audio needs new samples
    // To avoid overloading the grabbing thread, signal when free space more than half of buffer-size
    //
    if (m_audio_buffer.freeSpaceSize() > m_audio_buffer.size() / 2)
        m_threadLocker.signal();

    m_ellapsedAudioMicroSec += playbackSec * 1000000.0;
    //
    // Important:
    //
    // Even text output will stuck audio playback thread
    //
    // fprintf (stdout, "m_ellapsedAudioMicroSec = %d\n", m_ellapsedAudioMicroSec);
}

int
FFmpegLibAvStreamImpl::GetFramePtr(const unsigned long & timePosMS, unsigned char *& pArray)
{
    pArray = NULL;
    int err = 0;
    try
    {
        err = m_video_buffer.GetFramePtr (timePosMS, pArray);
        if (err != 0)
        {
            //
            // Clear buffer to load next block
            //
            m_video_buffer.flush();
        }
    }
    catch (...)
    {
    }

    return err;
}

void
FFmpegLibAvStreamImpl::ReleaseFoundFrame()
{
    m_video_buffer.ReleaseFoundFrame();
    //
    // Signal thread that video needs new frames
    //
    m_threadLocker.signal();
}

const bool
FFmpegLibAvStreamImpl::isHasVideo() const
{
    return (m_videoIndex >= 0);
}

float
FFmpegLibAvStreamImpl::fps() const
{
    return m_frame_rate;
}


void
FFmpegLibAvStreamImpl::stopShadowThread()
{
    // Stop grabbing thread
    if (isRunning())
    {
        // To exit from thread-loop with Condition,
        // set flag to exit-state and signals condition after that.
        m_shadowThreadStop = true;
        m_threadLocker.signal();

        join();
        m_isNeedFlushBuffers = false;
    }
}

void
FFmpegLibAvStreamImpl::preRun()
{
    const unsigned long elapsedTimeMS = m_playerTimer.ElapsedMilliseconds ();
    m_ellapsedAudioMicroSec = 0;

    //
    // Prepare Audio to streaming
    //
    m_audio_buffering_finished = true;
    m_audioDelayMicroSec = 0;
    if (isHasAudio())
    {
        if (m_audio_sink.valid())
        {
            m_ellapsedAudioMicroSec = elapsedTimeMS * 1000;
        
            if (m_isNeedFlushBuffers == true)
            {
                FFmpegWrapper::seekAudio(m_audioIndex, elapsedTimeMS);
                
                m_audio_buffer.flush();
            }

            m_audio_buffering_finished = false;
        }
        else
        {
            //
            // If by some reason, audioSink has not been attached after initialization,
            // we should remove audio (to avoid sync video by audio) because audio will not playback.
            //
            m_audioFormat.clear();
            m_audio_buffer.release();
            m_audioIndex = -1;
        }
    }
    //
    if (isHasVideo())
    {
        if (m_isNeedFlushBuffers == true)
        {
            m_video_buffer.flush();

            unsigned char * pFrame;
            // todo: should be processed to error
            const int biErr = m_video_buffer.GetFramePtr(0, pFrame);

            // todo: should be processed to error
            const short sErr = FFmpegWrapper::getImage(m_videoIndex, elapsedTimeMS, pFrame);
            m_video_buffer.ReleaseFoundFrame();
        }
    }
}

void
FFmpegLibAvStreamImpl::postRun()
{
    if (isHasVideo())
        m_renderer.Stop();

    if (m_audio_sink.valid())
        m_audio_sink->pause();

    m_playerTimer.Stop();
    //
    // Reset Audio timing, so next Start will starts from the stopped audio moment
    //
    if (isHasAudio() && m_audio_sink.valid())
    {
        m_playerTimer.Reset();
        m_playerTimer.ElapsedMilliseconds (m_ellapsedAudioMicroSec / 1000.0);
        m_ellapsedAudioMicroSec = 0;
    }
    //
    // If streams finished, reset pointer to start position
    // 
    if (m_audio_buffering_finished == true && m_video_buffer.isStreamFinished())
    {
        // V/A readers operate with time-values, based on ZERO time-point.
        // So here we reset timer by 0.
        //
        m_playerTimer.Reset();
        m_playerTimer.ElapsedMilliseconds (0);
        m_ellapsedAudioMicroSec = 0;
        
        if (m_pPlayer)
        {
            m_pPlayer->pause();
            m_pPlayer->rewind();
            if (m_loop)
                m_pPlayer->play();
        }
    }
}

void
FFmpegLibAvStreamImpl::startPlayback()
{
    m_playerTimer.Start();
    //
    if (isHasAudio() &&
        m_audio_sink.valid() &&
        m_audio_sink->playing() == false)
    {
        setAudioDelayMicroSec ( m_audio_sink->getDelay() * 1000.0 );
        //
        m_audio_sink->play();
    }
    if (isHasVideo())
        m_renderer.start();
}

void
FFmpegLibAvStreamImpl::run()
{
    Mutex                   lockMutex;
    ScopedLock              lock (lockMutex);

    preRun();

    const unsigned short    samplesPart = 32767; // do not change. This is restriction of ffmpeg-wrapper
    const unsigned int      minBlockSize = samplesPart * m_audioFormat.m_bytePerSample * m_audioFormat.m_channelsNb;
    unsigned char *         pAudioData = minBlockSize > 0 ? new unsigned char[minBlockSize * 2] : NULL; // ... * 2], because it could read more than minBlockSize
    try
    {
        m_shadowThreadStop = false;
        //
        bool                videoBufferFull;
        bool                audioBufferFull;
        //
        bool                isPlaybackStarted   = false;
        unsigned int        necAudioSpace       = minBlockSize; // may take only two valus: 1) minBlockSize 2) m_audio_buffer.size() / 2
        while (m_shadowThreadStop == false)
        {
            audioBufferFull = true;
            videoBufferFull = true;
            //
            // Grab Audio Buffer
            //
            if (isHasAudio() && pAudioData != NULL)
            {
                const unsigned int space_audio_size = m_audio_buffer.freeSpaceSize();
                // Optimizing for grabbing audio:
                //
                // 1. If audio buffer almost full, thread stops grabbing audio, to put more time for grabbing video
                // 2. If audio buffer free for grabbing more than at half, (re)starts grabbing audio
                if (necAudioSpace == minBlockSize)
                {
                    // Audio buffer is overfull
                    if (space_audio_size < necAudioSpace)
                    {
                        // fprintf (stdout, "Audio buffer is overfull\n");
                        necAudioSpace = m_audio_buffer.size() / 2;
                    }
                }
                else
                {
                    // Big part of audio-buffer is empty. Now it may be forced to overfull
                    if (space_audio_size > necAudioSpace)
                    {
                        // fprintf (stdout, "Audio buffer forced to overfull\n");
                        necAudioSpace = minBlockSize;
                    }
                }
                if (space_audio_size > necAudioSpace && m_audio_buffering_finished == false)
                {
                    const int bytesread = FFmpegWrapper::getAudioSamples(m_audioIndex,
                                                                            123456789,
                                                                            m_audioFormat.m_channelsNb,
                                                                            m_audioFormat.m_avSampleFormat,
                                                                            m_audioFormat.m_sampleRate,
                                                                            samplesPart,
                                                                            pAudioData) * m_audioFormat.m_bytePerSample * m_audioFormat.m_channelsNb;
                    if (bytesread > 0)
                    {
                        m_audio_buffer.write (pAudioData, bytesread);
                        audioBufferFull = false;
                    }
                    else
                    {
                        m_audio_buffering_finished = true;
                    }
                }
            }
            //
            // Grab Video Buffer
            //
            if (isHasVideo())
            {
                if (m_video_buffer.isBufferFull() == false)
                {
                    m_video_buffer.writeFrame();
                    videoBufferFull = false;
                }
            }
            if (m_audio_buffering_finished == true && m_video_buffer.isStreamFinished())
            {
                break;
            }
            //
            // If buffers are full:
            //
            // 1. If it is first buffer's full state, we may start playback.
            // 2. it may couse to get too many empty loops, so locks this thread till some buffer opens for writing
            //
            if (audioBufferFull && videoBufferFull)
            {
                if (isPlaybackStarted == false)
                {
                    isPlaybackStarted = true;
                    startPlayback();
                }
                if (m_shadowThreadStop == false)
                    m_threadLocker.wait (& lockMutex);
            }
        }
    }
    catch (const std::exception & error)
    {
        OSG_WARN << "FFmpegLibAvStreamImpl::run : " << error.what() << std::endl;
    }

    catch (...)
    {
        OSG_WARN << "FFmpegLibAvStreamImpl::run : unhandled exception" << std::endl;
    }

    if (pAudioData)
        delete []pAudioData;

    m_shadowThreadStop = true;

    postRun();
}


} // namespace osgFFmpeg