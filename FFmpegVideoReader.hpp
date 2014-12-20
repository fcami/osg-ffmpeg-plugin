#ifndef HEADER_GUARD_FFMPEG_VIDEOREADER_H
#define HEADER_GUARD_FFMPEG_VIDEOREADER_H

#include "FFmpegHeaders.hpp"

namespace osgFFmpeg {

class FFmpegParameters;

class FFmpegVideoReader
{
private:
    bool                m_FirstFrame;
    int                 m_bytesRemaining;
#ifdef USE_SWSCALE
    struct SwsContext * img_convert_ctx;
#endif
    AVPacket            m_packet;
    AVFrame *           m_pSeekFrame;
    AVFrame *           m_pSrcFrame;
    PixelFormat         m_pixelFormat;
    mutable bool        m_is_video_duration_determined;
    mutable int64_t     m_video_duration;
    double              m_lastFoundInSeekTimeStamp_sec;
    bool                m_seekFoundLastTimeStamp;

    unsigned int        m_new_width;
    unsigned int        m_new_height;

    bool                GetNextFrame(AVCodecContext *pCodecCtx, AVFrame *pFrame, unsigned long & currPacketPos, double & currTime);
public:
    AVFormatContext *   m_fmt_ctx_ptr;
    short               m_videoStreamIndex;
    //
    // Search index of video-stream. If no one video-stream had not been found, return error.
    // If [scaledWidth] == 0, actual video size will produced. In another case, video-size may be scaled to
    // width == [scaledWidth] with actual Video Aspect Ratio when new width is less then actual width.
    const int           openFile(const char *filename, FFmpegParameters * parameters, const bool useRGB_notBGR, const long scaledWidth, float & aspectRatio, float & frame_rate, bool & alphaChannel);
    void                close(void);
    int                 seek(int64_t timestamp, unsigned char * ptrRGBmap);
    // buffer-size should be width*height*3 bytes;
    int                 grabNextFrame(uint8_t * buffer, double & timeStampInSec);
    //
    //
    //
    // type(int) derived from libavcodec.
    const int           get_width(void) const;
    const int           get_height(void) const;
    const float         get_fps(void) const;
    // max value for using in seek
    const int64_t       get_duration(void) const;
    float               findAspectRatio() const;
    const bool          alphaChannel() const;

};

} // namespace osgFFmpeg

#endif // HEADER_GUARD_FFMPEG_VIDEOREADER_H