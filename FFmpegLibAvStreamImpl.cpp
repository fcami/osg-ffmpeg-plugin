#include "FFmpegLibAvStreamImpl.hpp"
#include "FFmpegWrapper.hpp"
#include "FFmpegPlayer.hpp"
#include <osg/Notify>
#include <limits>
#include <stdexcept>

namespace osgFFmpeg
{

FFmpegLibAvStreamImpl::FFmpegLibAvStreamImpl()
:m_loop(false),
m_audioVolumeAlternative(1.0f),
m_audioBalance(0.0f),
m_AudioBufferTimeSec(6),
m_ellapsedAudioMicroSec(0),
m_pPlayer(NULL),
m_useRibbonTimeStrategy(true)
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
        //
        m_isAudioSinkImplementsVolumeControl = detectIsItImplementedAudioVolume();
    }
}

void
FFmpegLibAvStreamImpl::setAudioDelayMicroSec (const double & audioDelayMicroSec)
{
fprintf (stdout, "setAudioDelayMicroSec() %f\n", audioDelayMicroSec);
    m_audioDelayMicroSec = audioDelayMicroSec;
}

const int
FFmpegLibAvStreamImpl::initialize(const FFmpegFileHolder * pHolder, FFmpegPlayer * pPlayer)
{
fprintf (stdout, "FFmpegLibAvStreamImpl::initialize()\n");
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
fprintf (stdout, "FFmpegLibAvStreamImpl::initialize() finished\n");

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
FFmpegLibAvStreamImpl::GetPlaybackTime() const
{
    if (isHasAudio())
    {
        ScopedLock  lock (m_mutex);

        const unsigned long actual_offset_micros = (m_ellapsedAudioMicroSecOffsetInitial > 0) ? (m_ellapsedAudioMicroSecOffsetTimer.time_u() - m_ellapsedAudioMicroSecOffsetInitial) : 0;
        const unsigned long actual_value_ms = (m_ellapsedAudioMicroSec + actual_offset_micros - m_audioDelayMicroSec) / 1000;

        return (m_ellapsedAudioMicroSec < m_audioDelayMicroSec) ? 0 : actual_value_ms;
    }

    return m_playerTimer.ElapsedMilliseconds ();
}

void
FFmpegLibAvStreamImpl::Start()
{
fprintf (stdout, "FFmpegLibAvStreamImpl::Start()\n");

    //
    // Guaranty that thread will starts even if it was run before
    //
    if (isRunning() == true)
        Pause();

    if (isRunning() == false)
    {
        // To avoid thread concurent conflicts, follow param should be
        // defined from parent thread
        m_shadowThreadStop = false;
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
    m_ellapsedAudioMicroSecOffsetInitial = 0;
}

void
FFmpegLibAvStreamImpl::Seek(const unsigned long & newTimeMS)
{
    if (isRunning())
        Pause();

    m_isNeedFlushBuffers = true;
    m_playerTimer.Reset();
    m_playerTimer.ElapsedMilliseconds (newTimeMS);
    m_ellapsedAudioMicroSec = newTimeMS * 1000;
    m_ellapsedAudioMicroSecOffsetInitial = 0;
}

const bool
FFmpegLibAvStreamImpl::detectIsItImplementedAudioVolume()
{
fprintf (stdout, "FFmpegLibAvStreamImpl::detectIsItImplementedAudioVolume()\n");
    bool    result = false;
    if (m_audio_sink.valid())
    {
        const float prevAudioVolume = m_audio_sink->getVolume();

        m_audio_sink->setVolume(0.3f);
        if ((m_audio_sink->getVolume() - 0.3f) > 1.e-3)
            result = true;

        m_audio_sink->setVolume(0.9f);
        if ((m_audio_sink->getVolume() - 0.9f) > 1.e-3)
            result = true;

        // Return previous audio volume
        m_audio_sink->setVolume(prevAudioVolume);
    }
    return result;
}


void
FFmpegLibAvStreamImpl::setAudioVolume(const float & volume)
{
    if (m_audio_sink.valid())
    {
        const float trimmed_volume = std::min(1.0f, std::max(0.0f, volume));
        //
        m_audioVolumeAlternative = trimmed_volume;
        m_audio_sink->setVolume(trimmed_volume);
    }
}

const float
FFmpegLibAvStreamImpl::getAudioVolume() const
{
    if (m_audio_sink.valid())
    {
        if (m_isAudioSinkImplementsVolumeControl == true)
            return m_audio_sink->getVolume();
        else
            return m_audioVolumeAlternative;
    }
    return 0.0f;
}

const float
FFmpegLibAvStreamImpl::getAudioBalance() const
{
    if (m_audio_sink.valid())
    {
        return m_audioBalance;
    }
    return 0.0f;
}

void
FFmpegLibAvStreamImpl::setAudioBalance(const float & balance)
{
    if (m_audio_sink.valid())
    {
        m_audioBalance = std::min(1.0f, std::max(-1.0f, balance));
    }
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
    // Multiply samples by master/balanced volume
    //
    if (m_audio_sink.valid())
    {
        unsigned long       sampleInd;
        const unsigned long playBackSamples             = playbackBytes / m_audioFormat.m_bytePerSample;
        // [0..1]
        // Warning: Do not forget implement functions "setVolume(float)" and "float getVolume() const"
        // for subclass of AudioSink.
        const float         audioMaterVolume            = getAudioVolume ();
        // balance of the audio: -1 = left, 0 = center,  1 = right
        const float         audioBallance               = m_audioBalance;
        //
        float               audioVolumeChannel[128];
        audioVolumeChannel[0] = audioMaterVolume - ((audioBallance > 0.0f) ? (audioBallance * audioMaterVolume) : 0.0f);
        audioVolumeChannel[1] = audioMaterVolume + ((audioBallance < 0.0f) ? (audioBallance * audioMaterVolume) : 0.0f);
        audioVolumeChannel[2] = audioMaterVolume;
        audioVolumeChannel[3] = audioMaterVolume;
        audioVolumeChannel[4] = audioVolumeChannel[0];
        audioVolumeChannel[5] = audioVolumeChannel[1];

        switch (m_audioFormat.m_avSampleFormat)
        {
        case AV_SAMPLE_FMT_U8:
            {
                uint8_t *           ptr = (uint8_t *)buffer;
                for (sampleInd = 0; sampleInd < playBackSamples; ++sampleInd, ++ptr)
                    (*ptr) *= audioVolumeChannel[sampleInd % m_audioFormat.m_channelsNb];
                break;
            }
        case AV_SAMPLE_FMT_S16:
            {
                int16_t *           ptr = (int16_t *)buffer;
                for (sampleInd = 0; sampleInd < playBackSamples; ++sampleInd, ++ptr)
                    (*ptr) *= audioVolumeChannel[sampleInd % m_audioFormat.m_channelsNb];
                break;
            }
        case AV_SAMPLE_FMT_S32:
            {
                int32_t *           ptr = (int32_t *)buffer;
                for (sampleInd = 0; sampleInd < playBackSamples; ++sampleInd, ++ptr)
                    (*ptr) *= audioVolumeChannel[sampleInd % m_audioFormat.m_channelsNb];
                break;
            }
        case AV_SAMPLE_FMT_FLT:
            {
                float *             ptr = (float *)buffer;
                for (sampleInd = 0; sampleInd < playBackSamples; ++sampleInd, ++ptr)
                    (*ptr) *= audioVolumeChannel[sampleInd % m_audioFormat.m_channelsNb];
                break;
            }
        };
    }
    //
    // Signal thread that audio needs new samples
    // To avoid overloading the grabbing thread, signals when free space more than half of buffer-size
    //
    if (m_audio_buffer.freeSpaceSize() > m_audio_buffer.size() / 2)
        m_threadLocker.signal();

    {
        ScopedLock  lock (m_mutex);

        const double    curr_micros         = m_ellapsedAudioMicroSecOffsetTimer.time_u();
        double          compensate_micros   = 0.0;

        if (m_ellapsedAudioMicroSecOffsetInitial > 0)
            compensate_micros  = (curr_micros - m_ellapsedAudioMicroSecOffsetInitial) - playbackSec * 1000000.0;

        m_ellapsedAudioMicroSecOffsetInitial = curr_micros;
        m_ellapsedAudioMicroSec += playbackSec * 1000000.0 + compensate_micros;
    }
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
        err = m_video_buffer.GetFramePtr (timePosMS, pArray, m_useRibbonTimeStrategy);
        if (err != 0)
        {
            if (m_useRibbonTimeStrategy == false)
            {
                // To unlock grabbing stream(and avoid deadlock)
                // here we have to flush buffer
                if (m_video_buffer.isBufferFull() &&
                    m_video_buffer.isStreamFinished() == false)
                {
                    //
                    // Clear buffer to load next block
                    //
                    m_video_buffer.flush();
                }
            }
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
    m_ellapsedAudioMicroSecOffsetInitial = 0;
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
            const int biErr = m_video_buffer.GetFramePtr(0, pFrame, true);

            const short sErr = FFmpegWrapper::getImage(m_videoIndex, elapsedTimeMS, pFrame);
            m_video_buffer.ReleaseFoundFrame();

            if (sErr < 0)
            {
                m_video_buffer.setStreamFinished (true);
            }
        }
    }
}

void
FFmpegLibAvStreamImpl::postRun()
{
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
        m_ellapsedAudioMicroSecOffsetInitial = 0;
    }
    //
    // If buffering finished, reset pointer to start position
    //
    if (isPlaybackFinished())
    {
        // V/A readers operate with time-values, based on ZERO time-point.
        // So here we reset timer by 0.
        //
        m_playerTimer.Reset();
        m_playerTimer.ElapsedMilliseconds (0);
        m_ellapsedAudioMicroSec = 0;
        m_ellapsedAudioMicroSecOffsetInitial = 0;

        if (m_pPlayer)
        {
            m_pPlayer->pause();
            m_pPlayer->rewind();
            if (m_loop)
                m_pPlayer->play();
            else
            	m_audio_sink->play(); // Cover edge case of paused audio sink still holding buffered data.
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
    if (isHasVideo() && m_video_buffer.isStreamFinished() == false)
        m_renderer.Start();
}

const bool
FFmpegLibAvStreamImpl::isPlaybackFinished()
{
    // Audio finish playback
    if (isHasAudio())
    {
        if (m_audio_buffering_finished == true &&
            m_audio_buffer.size() == m_audio_buffer.freeSpaceSize())//audio buffer is empty, playback finished
            return true;
    }
    else
    {
        const unsigned long elapsedTimeMS   = m_playerTimer.ElapsedMilliseconds();
        const double        duration_ms     = m_pPlayer->getLength();

        if (elapsedTimeMS >= duration_ms)
            return true;
    }
    return false;
}

void
FFmpegLibAvStreamImpl::run()
{
    Mutex                   lockMutex;
    ScopedLock              lock (lockMutex);

    preRun();

    // Minimal samples for time-period passed by one frame multiplied by two(speed of filling audio buffer should be faster than audio playback),
    // limited by 32767 as restriction of ffmpeg-wrapper
    const unsigned short    samplesPart = std::min((double)32767, (double)(m_audioFormat.m_bytePerSample * m_audioFormat.m_channelsNb * m_audioFormat.m_sampleRate) / m_frame_rate * 2);
    const unsigned int      minBlockSize = samplesPart * m_audioFormat.m_bytePerSample * m_audioFormat.m_channelsNb;
    unsigned char *         pAudioData = minBlockSize > 0 ? new unsigned char[minBlockSize * 2] : NULL; // ... * 2], because it could read more than minBlockSize
    try
    {
        //
        bool                videoGrabbingInProcess;
        bool                audioGrabbingInProcess;
        //
        bool                isPlaybackStarted   = false;
        size_t              drop_frame_nb       = 0;
        //
        while (m_shadowThreadStop == false)
        {
            unsigned int    videoWriteFlag = 0;
            audioGrabbingInProcess = false;
            videoGrabbingInProcess = false;
            //
            // Grab Audio Buffer
            //
            if (isHasAudio() && pAudioData != NULL)
            {
                const unsigned int space_audio_size = m_audio_buffer.freeSpaceSize();

                if (space_audio_size > minBlockSize && m_audio_buffering_finished == false)
                {
                    double  max_avail_time_micros = -1.0;
                    //
                    // Limit audio-grabbing by time to avoid video artifacts
                    //
                    if (isPlaybackStarted)
                    {
                        //
                        // 1. To avoid audio-artifacts, we should guaranty that audio-buffer filled.
                        //    So, as less audio-buffer filled, as more available time for audio grabbing.
                        //    Even if it will become to appear the video artifacts.
                        //
                        // 2. To avoid video-artifacts, we should TRY spend as less as possible time for
                        //    audio grabbing.
                        //
                        const double audio_bufferedTimeSec  = (double)(m_audio_buffer.size() - space_audio_size) / (double)(m_audioFormat.m_bytePerSample * m_audioFormat.m_channelsNb * m_audioFormat.m_sampleRate);
                        //
                        const double audio_bufferedRatio    = (double)(m_audio_buffer.size() - space_audio_size) / (double)m_audio_buffer.size();
                        if (audio_bufferedRatio > 0.25)
                        {
                            const double video_frameTimeSec = 1.0 / m_frame_rate;

                                                                                                    // audio_bufferedRatio:       0.25 ... 0.5       ... 1.0
                            double coeff = 0.25 / (audio_bufferedRatio - 0.25 + std::numeric_limits<double>::min()) - 0.333;   // +INF ... 0.7       ... 0.0
                            max_avail_time_micros = video_frameTimeSec * coeff * 1000000.0;         //                            +INF ... frame*0.7 ... 0.0
                        }
                        else
                        {
                            if (m_useRibbonTimeStrategy == false)
                                videoWriteFlag |= 1; // disable decoding during searching
                        }
                    }
                    const int bytesread = FFmpegWrapper::getAudioSamples(m_audioIndex,
                                                                            123456789,
                                                                            m_audioFormat.m_channelsNb,
                                                                            m_audioFormat.m_avSampleFormat,
                                                                            m_audioFormat.m_sampleRate,
                                                                            samplesPart,
                                                                            pAudioData,
                                                                            max_avail_time_micros) * m_audioFormat.m_bytePerSample * m_audioFormat.m_channelsNb;
                    if (bytesread > 0)
                    {
                        m_audio_buffer.write (pAudioData, bytesread);
                        audioGrabbingInProcess = true;
                    }
                    else
                    {
                        m_audio_buffering_finished = true;
                        if (bytesread < 0)
                            throw std::runtime_error("Audio failed");
                    }
                }
            }
            //
            // Grab Video Buffer
            //
            if (isHasVideo())
            {
                if (m_video_buffer.isStreamFinished() == false)
                {
                    if (m_video_buffer.isBufferFull() == false)
                    {
                        if (m_useRibbonTimeStrategy == true)
                        {
                            drop_frame_nb = 0;
                        }
                        else
                        {
                            //
                            // Provide little acceleration for video grabbing
                            // Test shows, that 20-frame buffer accelerated enough by 1-frame dropping in half of the buffer-size.
                            //
                            if (isPlaybackStarted && m_video_buffer.isStreamFinished() == false)
                            {
                                double aspect = (double)m_video_buffer.freeSpaceSize() / (double)m_video_buffer.size();
                                drop_frame_nb = aspect * 2;
                            }
                        }
                        m_video_buffer.writeFrame (videoWriteFlag, drop_frame_nb);

                        videoGrabbingInProcess = true;
                    }
                }
                else
                {
                    m_renderer.quit(false);
                }
            }
            if (isPlaybackStarted && isPlaybackFinished())
            {
                break;
            }
            //
            // If grabbings do not work(waste a time):
            //
            // 1. If it is first wasting of the time and playback did not started before, we may start playback.
            // 2. It may couse to get too many empty loops, so locks this thread till some buffer opens for writing, or interrupt for exit
            //
            if (audioGrabbingInProcess == false &&
                videoGrabbingInProcess == false)
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
