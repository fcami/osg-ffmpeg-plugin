#ifndef HEADER_GUARD_FFMPEG_PLAYER_H
#define HEADER_GUARD_FFMPEG_PLAYER_H

#include <osg/ImageStream>
#include <OpenThreads/Thread>
#include <OpenThreads/Condition>
#include "MessageQueue.hpp"
#include "FFmpegFileHolder.hpp"
#include "FFmpegStreamer.hpp"

namespace osgFFmpeg {

// This parameter should be incremented each time before commit to repository
#define OSG_FFMPEG_PLUGIN_RELEASE_VERSION_INT   6


template <class T>
class MessageQueue;

class FFmpegParameters;

class FFmpegPlayer: public osg::ImageStream, public OpenThreads::Thread
{
public:
                                FFmpegPlayer();
                                FFmpegPlayer(const FFmpegPlayer & player,
                                        const osg::CopyOp & copyop = osg::CopyOp::SHALLOW_COPY);

    META_Object(osgFFmpeg, FFmpegPlayer);

    bool                        open(const std::string & filename,
                                        FFmpegParameters* parameters);

    virtual void                play();
    virtual void                pause();
    virtual void                rewind();
    virtual void                seek(double time);
    virtual void                quit(bool waitForThreadToExit = true);

    virtual void                setVolume(float volume);
    virtual float               getVolume() const;

    // balance of the audio: -1 = left, 0 = center,  1 = right
    virtual float               getAudioBalance();
    virtual void                setAudioBalance(float balance);

    virtual double              getCreationTime() const;
    virtual double              getLength() const;
    virtual double              getReferenceTime () const; 
    virtual double              getCurrentTime() const;
    virtual double              getFrameRate() const;

    virtual bool                isImageTranslucent() const;

private:
    void                        close();

    enum Command
    {
        CMD_PLAY,
        CMD_PAUSE,
        CMD_STOP,
        CMD_REWIND,
        CMD_SEEK
    };

    typedef MessageQueue<Command>   CommandQueue;
    typedef OpenThreads::Mutex      Mutex;
    typedef OpenThreads::ScopedLock<Mutex> ScopedLock;
    typedef OpenThreads::Condition  Condition;

    virtual                     ~FFmpegPlayer();
    virtual void                run();
    virtual void                applyLoopingMode();

    void                        pushCommand(Command cmd);
    bool                        handleCommand(Command cmd);

    void                        cmdPlay();
    void                        cmdPause();
    void                        cmdRewind();
    void                        cmdSeek(double time);

    FFmpegFileHolder            m_fileHolder;
    FFmpegStreamer              m_streamer;

    CommandQueue *              m_commands;
    Condition                   m_commandQueue_cond;
    double                      m_seek_time;
};

} // namespace osgFFmpeg

#endif // HEADER_GUARD_FFMPEG_PLAYER_H