#include "FFmpegPlayer.hpp"
#include "FFmpegParameters.hpp"
#include "FFmpegAudioStream.hpp"

#include <OpenThreads/ScopedLock>
#include <osg/Notify>

#include <memory>

namespace osgFFmpeg {

FFmpegPlayer::FFmpegPlayer() :
    m_commands(0)
{
    setOrigin(osg::Image::TOP_LEFT);

    std::auto_ptr<CommandQueue> commands(new CommandQueue);

    m_commands = commands.release();
}



FFmpegPlayer::FFmpegPlayer(const FFmpegPlayer & image, const osg::CopyOp & copyop) :
    osg::ImageStream(image, copyop)
{
    // todo: probably incorrect or incomplete
}



FFmpegPlayer::~FFmpegPlayer()
{
    OSG_INFO << "Destructing FFmpegPlayer..." << std::endl;

    quit(true);

    OSG_INFO<<"Have done quit"<<std::endl;

    // release athe audio streams to make sure that the decoder doesn't retain any external
    // refences.
    getAudioStreams().clear();

    delete m_commands;

    OSG_INFO<<"Destructed FFmpegPlayer."<<std::endl;
}

bool FFmpegPlayer::open(const std::string & filename, FFmpegParameters* parameters)
{
    setFileName(filename);

    if (m_fileHolder.open(filename, parameters) < 0)
        return false;

    if (m_streamer.open(& m_fileHolder, this) < 0)
    {
        m_fileHolder.close();
        return false;
    }

    setImage(
        m_fileHolder.width(), m_fileHolder.height(), 1, GL_RGB, GL_RGB, GL_UNSIGNED_BYTE,
        const_cast<unsigned char *>(m_streamer.getFrame()), NO_DELETE
    );


    setPixelAspectRatio(m_fileHolder.pixelAspectRatio());

    OSG_NOTICE<<"ffmpeg::open("<<filename<<") size("<<s()<<", "<<t()<<") aspect ratio "<<m_fileHolder.pixelAspectRatio()<<std::endl;

#if 1
    // swscale is reported errors and then crashing when rescaling video of size less than 10 by 10.
    if (s()<=10 || t()<=10) return false;
#endif

    if (m_fileHolder.isHasAudio())
    {
        OSG_NOTICE<<"Attaching FFmpegAudioStream"<<std::endl;

        getAudioStreams().push_back(new FFmpegAudioStream(& m_fileHolder, & m_streamer));
    }

    _status = PAUSED;
    applyLoopingMode();

    start(); // start thread

    return true;
}

void FFmpegPlayer::close()
{
    m_streamer.close();
    m_fileHolder.close();
}

void FFmpegPlayer::play()
{
    pushCommand(CMD_PLAY);

#if 0
    // Wait for at least one frame to be published before returning the call
    OpenThreads::ScopedLock<Mutex> lock(m_mutex);

    while (duration() > 0 && ! m_frame_published_flag)
        m_frame_published_cond.wait(&m_mutex);

#endif
}



void FFmpegPlayer::pause()
{
    pushCommand(CMD_PAUSE);
}



void FFmpegPlayer::rewind()
{
    pushCommand(CMD_REWIND);
}

void FFmpegPlayer::seek(double time)
{
    m_seek_time = time;
    pushCommand(CMD_SEEK);
}



void FFmpegPlayer::quit(bool waitForThreadToExit)
{
    // Stop control thread
    if (isRunning())
    {
        pushCommand(CMD_STOP);

        if (waitForThreadToExit)
            join();
    }
}

void FFmpegPlayer::setVolume(float volume)
{
    m_streamer.setAudioVolume(volume);
}

float FFmpegPlayer::getVolume() const
{
    return m_streamer.getAudioVolume();
}

float FFmpegPlayer::getAudioBalance()
{
    return m_streamer.getAudioBalance();
}

void FFmpegPlayer::setAudioBalance(float balance)
{
    m_streamer.setAudioBalance(balance);
}

double FFmpegPlayer::getCreationTime() const
{
    // V/A readers operate with time-values, based on ZERO time-point.
    // So here we returns 0.
    return 0;
}

double FFmpegPlayer::getLength() const
{
    return m_fileHolder.duration_ms();
}


double FFmpegPlayer::getReferenceTime () const
{
    return m_streamer.getCurrentTimeSec();
}

double FFmpegPlayer::getCurrentTime() const
{
    return m_streamer.getCurrentTimeSec();
}



double FFmpegPlayer::getFrameRate() const
{
    return m_fileHolder.frameRate();
}



bool FFmpegPlayer::isImageTranslucent() const
{
    return m_fileHolder.alphaChannel();
}



void FFmpegPlayer::run()
{
    try
    {
        Mutex       lockMutex;
        ScopedLock  lock(lockMutex);

        bool done = false;

        while (! done)
        {
            if (_status == PLAYING)
            {
                bool no_cmd;
                const Command cmd = m_commands->timedPop(no_cmd, 1);

                if (no_cmd)
                {
                    m_commandQueue_cond.wait(& lockMutex);
                }
                else
                {
                    done = ! handleCommand(cmd);
                }
            }
            else
            {
                done = ! handleCommand(m_commands->pop());
            }
        }
    }

    catch (const std::exception & error)
    {
        OSG_WARN << "FFmpegPlayer::run : " << error.what() << std::endl;
    }

    catch (...)
    {
        OSG_WARN << "FFmpegPlayer::run : unhandled exception" << std::endl;
    }

    OSG_NOTICE<<"Finished FFmpegPlayer::run()"<<std::endl;
    
    // Ensure that all threads are closed
    cmdPause();
    //
    // Because \open() starts thread, here we should call \close()
    close();
}



void FFmpegPlayer::applyLoopingMode()
{
    m_streamer.loop (getLoopingMode() == LOOPING);
}

void FFmpegPlayer::pushCommand(Command cmd)
{
    m_commands->push(cmd);
    m_commandQueue_cond.signal();
}

bool FFmpegPlayer::handleCommand(const Command cmd)
{
    switch (cmd)
    {
    case CMD_PLAY:
        cmdPlay();
        return true;

    case CMD_PAUSE:
        cmdPause();
        return true;

    case CMD_REWIND:
        cmdRewind();
        return true;

    case CMD_SEEK:
        cmdSeek(m_seek_time);
        return true;

    case CMD_STOP:
        cmdPause();
        return false;

    default:
        OSG_WARN << "FFmpegPlayer::handleCommand() Unsupported command" << std::endl;
        return false;
    }
}



void FFmpegPlayer::cmdPlay()
{
    if (_status == PAUSED)
        m_streamer.play();

    _status = PLAYING;
}



void FFmpegPlayer::cmdPause()
{
    if (_status == PLAYING)
    {
        m_streamer.pause();
    }

    _status = PAUSED;
}



void FFmpegPlayer::cmdRewind()
{
    bool isPlayed = false;

    if (_status == PLAYING)
    {
        isPlayed = true;
        m_streamer.pause();
    }

    // V/A readers operate with time-values, based on ZERO time-point.
    // So here we seeking to ZERO-time point
    m_streamer.seek(0);

    if (isPlayed == true)
        m_streamer.play();
}

void FFmpegPlayer::cmdSeek(double time)
{
    cmdPause();
    //
    const unsigned long ul_time = time;
    m_streamer.seek(ul_time);
}

} // namespace osgFFmpeg
